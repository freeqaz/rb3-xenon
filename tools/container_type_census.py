#!/usr/bin/env python3
"""Census the container/element-TYPE divergence class, WITH THE CALLER INCLUDED.

WHAT THIS MEASURES
------------------
Every charged relocation-name site at the SHIPPED ruler (read from report.json's
provenance, never hardcoded) whose two spellings are sibling instantiations of
the same class template differing only in template arguments -- e.g.

    retail  ?insert@?$list@PAVCharClip@@...@@   (list<CharClip*>::insert)
    ours    ?insert@?$list@PAVObject@@...@@     (list<Object*>::insert)

WHY THIS TOOL EXISTS WHEN tools/at100_sibling_split.py ALREADY CLASSIFIES THESE
------------------------------------------------------------------------------
at100_sibling_split.py flags this exact shape SIBLING_SAME_CLASS and prints
"MAP DEFECT (source structurally cannot do this)". Its structural argument is:

    sort<T,Cmp> calls __introsort_loop<T,...,Cmp> because the template says so;
    the compiler cannot emit a call from one instantiation into a sibling's body.

*** THAT ARGUMENT IS SOUND ONLY WHEN THE CALLER IS ITSELF AN INSTANTIATION OF
THE SAME TEMPLATE FAMILY, and classify(t, b) never looks at the caller. ***

When the caller is an ordinary function, the element type is chosen by a
DECLARATION, not by the caller's own template arguments:

    struct CharBonesObject { ObjPtrList<Object> mThings; };   // ours
    struct CharBonesObject { ObjPtrList<CharInterest> mThings; };  // retail
    void CharBonesObject::Poll() { mThings.push_back(x); }    // ordinary caller

Here our source emits a call to the <Object> instantiation and retail's to the
<CharInterest> one, and the cause is a WRONG HEADER DECLARATION -- a source
defect, fully representable, and fixable by one header edit that pays every call
site in the TU. So SIBLING_SAME_CLASS is OVER-BROAD: it is a proof of map-defect
only on the sub-population where the caller is a sibling too.

This tool therefore splits the class on the caller:

    CALLER_SIBLING     caller is an instantiation of the same template family
                       as T and B  =>  at100_sibling_split's argument APPLIES,
                       source structurally cannot do it, MAP defect.
    CALLER_ORDINARY    caller is not  =>  BOTH readings stay open; a wrong
                       member/local declaration is representable. CANDIDATE.

FOLD ADJUDICATION (one-directional, per lane GAP-C -- stated as a bound)
-----------------------------------------------------------------------
scripts/target_symbol_map.json is ADDRESS-KEYED: no address carries two names.
So "T and B are both in the map at DIFFERENT addresses" proves they did NOT
fold. The converse is UNREPRESENTABLE: a name absent from the map, or the two
resolving to one address, is NOT evidence of a fold -- absence of a fold verdict
is vacuous. Every verdict here is therefore reported as one of

    NOT_REFUTED_BY_MAP   both names in map, different addresses
    SAME_ADDR            the two names resolve to ONE address
    UNKNOWN              one or both absent from the map -> NO VERDICT

and only NOT_REFUTED_BY_MAP rows are counted as class members.

⛔ THE LABEL USED TO BE CALLED `NOFOLD`, AND THAT NAME ASSERTED MORE THAN THE
EVIDENCE SUPPORTS. Lane L7-CONTAINER2 (docs/decomp/CONTAINER2_2026-09-10.md §6)
MEASURED the premise "the map's name at each address is right" and it fails
often:

    ~ObjRefConcrete<T> (vtable/RTTI)   70 names checked, 30 provably wrong  43%
    list<T*>::insert   (node size)      8 names checked,  6 provably wrong  75%

So the label inherits a 43-75% error rate on exactly the addresses it consults,
and L7's headline pair is the demonstration: `list<CharClip*>::insert` vs
`list<Object*>::insert` was labelled NOFOLD because the names sit at different
addresses -- but `0x823c3ac8` is not `list<Object*>::insert` at all (24-byte
node, copy-constructs a `CharIKHand::IKTarget`), so the premise rested on a
wrong name and THE TRUTH WAS THE OPPOSITE OF THE LABEL: the pointer-element
family DID fold. A reader who saw "NOFOLD" and stopped there would have
concluded exactly backwards, which is why the name changed rather than a caveat
being added to a doc nobody re-reads.

WHAT THIS TOOL DOES NOT DO
--------------------------
It does not decide WHICH side is wrong. A NOFOLD + CALLER_ORDINARY row is a
locus worth reading retail bytes at; it is not a confirmed source bug. The
5,612 B Handle@GemPlayer archetype (lane MPNGAP-1) sat in exactly this class and
adjudicated as THE MAP BEING WRONG AND OUR SOURCE RIGHT.
"""
import argparse, collections, json, os, re, subprocess, sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

MISMATCH_ROW = re.compile(r"^\|\s*(\d+)\s*\|\s*`([^`]*)`\s*\|\s*`([^`]*)`\s*\|\s*(\w+)\s*\|")


def ruler_from_report(rep):
    """Read the SHIPPED ruler out of report.json rather than hardcoding it."""
    for entry in rep.get("provenance", {}).get("diff_config", []):
        if entry.startswith("functionRelocDiffs="):
            return entry.split("=", 1)[1]
    raise SystemExit("report.json carries no functionRelocDiffs -- refusing to guess")


# ---------------------------------------------------------------- name parsing
def tmpl_class(name):
    """Mangled name -> (member, class-template-base, full-class) or None.

    ?insert@?$list@PAVCharClip@@V?$allocator@...@@@@QAA...  ->
        ('insert', 'list', '?$list@PAVCharClip@@V?$allocator@...')
    ??1?$ObjRefConcrete@VCharLookAt@@@@UAA@XZ ->
        ('??1', 'ObjRefConcrete', '?$ObjRefConcrete@VCharLookAt@@')
    """
    if not name.startswith("?"):
        return None
    # special (ctor/dtor/operator): ??<code> then class
    m = re.match(r"^(\?\?[0-9A-Za-z_]|\?\?_[0-9A-Za-z])(.*)$", name)
    if m:
        member, rest = m.group(1), m.group(2)
    else:
        m = re.match(r"^\?([A-Za-z0-9_]+)@(.*)$", name)
        if not m:
            return None
        member, rest = m.group(1), m.group(2)
    if not rest.startswith("?$"):
        return None                      # class is not a template
    base = rest[2:].split("@", 1)[0]
    # full class token = up to the "@@" that closes the class, best-effort
    return (member, base, rest)


def template_family(name):
    """Template family of ANY symbol, for the caller test: the class-template
    base if the enclosing class is a template, else the free-template-function
    identifier, else None (ordinary)."""
    if name.startswith("??$"):
        i = name.find("@", 3)
        return name[3:i] if i > 0 else name[3:]
    t = tmpl_class(name)
    return t[1] if t else None


SYMTOK = re.compile(r"(\?[\?$]?[A-Za-z0-9_@$?<>\-.]+)")


def symbol_in(text):
    """Pull the mangled symbol out of an objdiff Target/Base cell.

    *** THE CELL IS DISASSEMBLED INSTRUCTION TEXT, NOT A BARE SYMBOL. *** It
    looks like `bl ??1?$ObjRefConcrete@VCharLookAt@@VObjectDir@@@@UAA@XZ`, or
    for a hi/lo pair `lis r11, <name>@h` / `addi r6, r11, <name>@l`. Feeding the
    whole cell to a mangled-name parser makes every row fail the leading-`?`
    test and the census reports a clean, decisive ZERO -- which is exactly what
    this tool did on its first full run over all 8,191 rows.
    """
    m = SYMTOK.search(text)
    if not m:
        return None
    s = m.group(1)
    for suf in ("@h", "@l", "@ha"):
        if s.endswith(suf):
            s = s[: -len(suf)]
    return s


def classify_pair(t, b):
    """SIBLING_SAME_CLASS iff same member, same class-template base, different
    template arguments."""
    pt, pb = tmpl_class(t), tmpl_class(b)
    if not pt or not pb:
        return None
    if pt[0] != pb[0]:
        return None                      # different member
    if pt[1] != pb[1]:
        return None                      # different class template
    if pt[2] == pb[2]:
        return None                      # identical -> not a divergence
    return "SIBLING_SAME_CLASS"


# ---------------------------------------------------------------------- diff
def diff_one(args):
    project, unit, symbol, ruler = args
    cmd = [str(Path(project) / "bin/objdiff-cli"), "diff", "-p", str(project),
           "-u", unit, "-c", f"functionRelocDiffs={ruler}",
           "--include-instructions", "--analyze", symbol]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    except subprocess.TimeoutExpired:
        return unit, symbol, None
    if r.returncode != 0:
        return unit, symbol, None
    sites = []
    for line in r.stdout.splitlines():
        m = MISMATCH_ROW.match(line)
        if m:
            sites.append((m.group(2), m.group(3), m.group(4)))
    return unit, symbol, sites




def is_container_site(tgt_txt, base_txt, kind):
    """True iff this charged site is one the container class can close."""
    if kind != "diff_arg":
        return False
    t, b = symbol_in(tgt_txt), symbol_in(base_txt)
    return bool(t and b and t != b and classify_pair(t, b))


def row_coverage(sites):
    """(charged sites, container sites, fully_covered) for ONE row.

    THE CROSSING CRITERION, per CONTAINER2 §1. `matched_code` is all-or-nothing
    per row, so a row crosses only when EVERY charged site on it is one this
    class can close. Extracted from the loop so the criterion is testable
    without a tree: the band criterion it replaces was never tested at all,
    which is part of why it survived being 3.1x wrong.

    ⚠ `fully_covered` is False for a row with zero charged sites, not True. An
    `all()` over an empty list is vacuously True and would silently promote
    every row objdiff returned nothing for into the realisable prize -- the
    `all([])` trap CLAUDE.md names elsewhere.
    """
    total = len(sites)
    container = sum(1 for t, b, k in sites if is_container_site(t, b, k))
    return total, container, (total > 0 and container == total)


# ---------------------------------------------------------------- aggregate
#: The band the briefs used before CONTAINER2 §1 refuted it. Kept ONLY so the
#: summary can print both and show the gap; never used to select work.
LEGACY_MM_BAND = 3


def summarise(recs, total_code=None, out=None):
    """Report the candidate class and the CROSSING population side by side.

    Both criteria are printed from ONE run on ONE tree, because the interesting
    quantity is the GAP between them and two separate runs could not establish
    it (the population moves under every map repair -- see the provenance note
    the caller prints).

    Returns the summary dict so a test can assert on it.
    """
    out = sys.stdout if out is None else out
    cand = [r for r in recs
            if r["caller_class"] == "CALLER_ORDINARY"
            and r["fold"] == "NOT_REFUTED_BY_MAP"]

    def rows_bytes(sel):
        by = {}
        for r in sel:
            by[(r["unit"], r["sym"])] = int(r.get("size", 0))
        return len(by), sum(by.values())

    n_rows, n_bytes = rows_bytes(cand)
    cross = [r for r in cand if r.get("fully_covered")]
    c_rows, c_bytes = rows_bytes(cross)
    single = [r for r in cross if int(r.get("row_sites", 0)) == 1]
    s_rows, s_bytes = rows_bytes(single)
    band = [r for r in cand if int(r.get("row_sites", 0)) <= LEGACY_MM_BAND]
    b_rows, b_bytes = rows_bytes(band)
    # the band's own intersection with reality: banded AND fully covered
    both = [r for r in band if r.get("fully_covered")]
    bc_rows, bc_bytes = rows_bytes(both)

    pp = (lambda v: f"  ({100.0 * v / total_code:.4f} pp)") if total_code else (lambda v: "")

    print("\n=== candidate class: CALLER_ORDINARY & NOT_REFUTED_BY_MAP ===", file=out)
    print(f"  {len(cand):>5} sites  {n_rows:>4} rows  {n_bytes:>8,} B{pp(n_bytes)}", file=out)
    print("  ⚠ NOT_REFUTED_BY_MAP carries a MEASURED 43-75% map-name error rate "
          "(CONTAINER2 §6):", file=out)
    print("    it means 'the map does not refute a fold', NEVER 'did not fold'.", file=out)

    print("\n=== CROSSING population (every charged site on the row is a "
          "container site) ===", file=out)
    print(f"  {c_rows:>4} rows  {c_bytes:>8,} B{pp(c_bytes)}   <- the realisable prize",
          file=out)
    print(f"  of which single-charge rows: {s_rows:>4} rows  {s_bytes:>8,} B", file=out)

    print(f"\n=== the REFUTED criterion, for comparison (mismatches <= "
          f"{LEGACY_MM_BAND}) ===", file=out)
    print(f"  {b_rows:>4} rows  {b_bytes:>8,} B{pp(b_bytes)}", file=out)
    if b_bytes:
        print(f"  ratio crossing/band = {c_bytes / b_bytes:.2f}x by bytes, "
              f"{(c_rows / b_rows) if b_rows else float('nan'):.2f}x by rows", file=out)
    print(f"  banded AND fully covered: {bc_rows} rows / {bc_bytes:,} B -- the band "
          f"ADMITS {b_rows - bc_rows} rows that cannot cross", file=out)
    print(f"  and MISSES {c_rows - bc_rows} rows that can "
          f"({c_bytes - bc_bytes:,} B), because their charge count exceeds the band",
          file=out)

    # anti-vacuity: an empty candidate class is a RESULT only if we saw sites
    if recs and not cand:
        print("\n⚠ the candidate class is EMPTY over a non-empty site population -- "
              "check the caller_class/fold labels before reading this as 'drained'",
              file=out)
    return {"cand_sites": len(cand), "cand_rows": n_rows, "cand_bytes": n_bytes,
            "cross_rows": c_rows, "cross_bytes": c_bytes,
            "single_rows": s_rows, "single_bytes": s_bytes,
            "band_rows": b_rows, "band_bytes": b_bytes,
            "band_and_covered_rows": bc_rows, "band_and_covered_bytes": bc_bytes}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--from", dest="from_json", default=None,
                    help="re-aggregate an existing dump instead of re-diffing "
                         "~8,000 rows. The criterion is then auditable without "
                         "a 7-minute pass, which is what makes it reviewable.")
    ap.add_argument("--jobs", type=int, default=16)
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()

    proj = Path(a.project)
    rep = json.load(open(proj / "build/45410914/report.json"))
    ruler = ruler_from_report(rep)
    prov = rep.get("provenance") or {}
    total_code = int(rep.get("measures", {}).get("total_code", 0) or 0)

    if a.from_json:
        recs = json.load(open(a.from_json))
        missing = [k for k in ("fully_covered", "row_sites")
                   if recs and k not in recs[0]]
        if missing:
            sys.exit(f"REFUSING: {a.from_json} predates the crossing criterion "
                     f"(no {missing}). Re-run the census; a band-era dump cannot "
                     f"be re-aggregated into a crossing figure.")
        print(f"[census] re-aggregating {len(recs):,} sites from {a.from_json}")
        summarise(recs, total_code or None)
        return
    amap = json.load(open(proj / "scripts/target_symbol_map.json"))
    name2addr = collections.defaultdict(set)
    for addr, nm in amap.items():
        # a row's value is normally a string; a few carry a list of spellings
        for one in (nm if isinstance(nm, list) else [nm]):
            if isinstance(one, str):
                name2addr[one].add(addr)

    # every row the grader withholds bytes from, that is not wholly unpaired
    rows, sizes = [], {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            mpn = float(f.get("match_percent_normalized", 0) or 0)
            fz = float(f.get("fuzzy_match_percent", 0) or 0)
            if fz < 100.0 and mpn > 0.0:
                rows.append((str(proj), u["name"], f["name"], ruler))
                sizes[(u["name"], f["name"])] = int(f.get("size", 0))
    if a.limit:
        rows = rows[: a.limit]
    print(f"[census] ruler={ruler}  rows={len(rows)}  jobs={a.jobs}", flush=True)

    out, done = [], 0
    seen = collections.Counter()      # anti-vacuity: what did we actually SEE?
    with ProcessPoolExecutor(max_workers=a.jobs) as ex:
        for unit, sym, sites in ex.map(diff_one, rows, chunksize=8):
            done += 1
            if done % 500 == 0:
                print(f"  ...{done}/{len(rows)}", flush=True)
            if not sites:
                continue
            caller_fam = template_family(sym)

            # ── THE CROSSING CRITERION (CONTAINER2 §1) ───────────────────────
            # `matched_code` is ALL-OR-NOTHING per row, so a row crosses only if
            # EVERY charged site on it resolves. A mismatch-count band is not
            # that criterion and is not even correlated with it in the right
            # direction: a 3-mismatch row with ONE container site does not cross
            # when the pair is fixed, while an 8-mismatch row whose every site
            # is a container site does. L7 measured the band criterion 3.1x too
            # small on this class (19,792 B briefed vs 61,952 B realisable).
            #
            # So count, per ROW: how many charged sites there are in total, and
            # how many of them are container sites. `fully_covered` means the
            # two agree -- every charge on the row is one this class can close.
            row_total, row_container, fully_covered = row_coverage(sites)

            for tgt_txt, base_txt, kind in sites:
                if kind != "diff_arg":
                    continue
                seen["diff_arg"] += 1
                tgt, base = symbol_in(tgt_txt), symbol_in(base_txt)
                if not tgt or not base or tgt == base:
                    continue
                seen["sym_pair"] += 1
                cl = classify_pair(tgt, base)
                if cl:
                    seen["sibling"] += 1
                if cl is None:
                    continue
                ta, ba = name2addr.get(tgt, set()), name2addr.get(base, set())
                if ta and ba and not (ta & ba):
                    # NOT "did not fold" -- "the map does not refute a fold".
                    # 43-75% of the names this consults are wrong; see the
                    # docstring and CONTAINER2 §6.
                    fold = "NOT_REFUTED_BY_MAP"
                elif ta and ba:
                    fold = "SAME_ADDR"
                else:
                    fold = "UNKNOWN"
                fam = template_family(tgt)
                out.append({
                    "unit": unit, "sym": sym, "size": sizes.get((unit, sym), 0),
                    "target": tgt, "base": base, "fold": fold,
                    "caller_family": caller_fam, "callee_family": fam,
                    "caller_class": ("CALLER_SIBLING" if caller_fam and caller_fam == fam
                                     else "CALLER_ORDINARY"),
                    # row-level, repeated on every site of the row so a
                    # consumer can aggregate without re-diffing
                    "row_sites": row_total,
                    "row_container_sites": row_container,
                    "fully_covered": fully_covered,
                })
    json.dump(out, open(a.out, "w"), indent=1)
    print(f"[census] saw diff_arg={seen['diff_arg']} sym_pairs={seen['sym_pair']} "
          f"sibling={seen['sibling']}")
    if seen["diff_arg"] and not seen["sym_pair"]:
        sys.exit("REFUSING: every diff_arg cell failed symbol extraction -- the "
                 "Target/Base columns are instruction TEXT and the parser is "
                 "reading them as bare symbols. A zero here is VACUOUS.")
    print(f"[census] wrote {len(out)} charged sibling-type sites -> {a.out}")
    # PROVENANCE, printed with the numbers and not in a doc: this population
    # moves under every map repair, so a figure without its base is not a
    # measurement. L7's 563/361/92,684 was taken at 3f9619c5 and the 36 map rows
    # it then REPAIRED are landed, so that figure is not reproducible here and
    # must not be inherited.
    print(f"[census] provenance: ruler={ruler}  objdiff={prov.get('tool_version')} "
          f"({prov.get('tool_binary_hash')})  map_entries={prov.get('map_file_entries')} "
          f"total_code={total_code:,}")
    summarise(out, total_code or None)


if __name__ == "__main__":
    main()
