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

    NOFOLD    both names in map, different addresses  -> provably did not fold
    UNKNOWN   one or both names absent from the map   -> NO VERDICT (not "fold")

and only NOFOLD rows are counted as class members.

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=16)
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()

    proj = Path(a.project)
    rep = json.load(open(proj / "build/45410914/report.json"))
    ruler = ruler_from_report(rep)
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
                    fold = "NOFOLD"
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
                })
    json.dump(out, open(a.out, "w"), indent=1)
    print(f"[census] saw diff_arg={seen['diff_arg']} sym_pairs={seen['sym_pair']} "
          f"sibling={seen['sibling']}")
    if seen["diff_arg"] and not seen["sym_pair"]:
        sys.exit("REFUSING: every diff_arg cell failed symbol extraction -- the "
                 "Target/Base columns are instruction TEXT and the parser is "
                 "reading them as bare symbols. A zero here is VACUOUS.")
    print(f"[census] wrote {len(out)} charged sibling-type sites -> {a.out}")


if __name__ == "__main__":
    main()
