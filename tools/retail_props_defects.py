#!/usr/bin/env python3
"""Size AND widen the retail property-name DEFECT class.  Companion to
tools/retail_props.py.

WHY THE CLASS IS INVISIBLE TO THE DEFAULT RULER: property names are relocation
ARGS and objdiff's `report.rs:394` hard-sets reloc args to None.  A function that
enumerates the RIGHT number of properties in the RIGHT order with the WRONG NAMES
scores exactly 100.0.  This script is the only thing that sees them.

=============================================================================
LANE CV-3 (2026-08-02) -- WHAT CHANGED AND WHY
=============================================================================
CU-1 proved the CS-4/CT-4 finder was STRUCTURALLY BLIND to a third of the
corpus: stratum (a) required a row to be NAMED **and** AT-100 **and** PINNED,
which corpus-wide was 2,170/3,194 = 67.9%.  This rewrite widens all three and
MEASURES each one, because a filter you have not measured is a filter you are
quietly trusting.

  filter    | before                   | after
  ----------+--------------------------+-------------------------------------
  pinned    | assumed to be a blind    | MEASURED and it is NOT one: 98.78%
            | spot                     | of the 11,990 Symbol-ctor sites are
            |                          | already inside a pinned .text range
            |                          | (30 of 3,243 prop-bearing retail fns
            |                          | are unpinned).  See ImageScanner.
  at-100    | hard `pct < 100 -> skip` | dropped; every named row is compared
            |                          | and the percent is REPORTED, on BOTH
            |                          | rulers, not used as a gate
  named     | `fn_XXXXXXXX -> stratum  | still cannot self-compare (no name to
            | (c)`, backlog only       | pair), but the SOURCE-BLOCK INDEX it
            |                          | is matched against is ~4x wider

TWO ROOT-CAUSE TOOL BUGS FIXED IN retail_props.py, NOT PAPERED OVER HERE:
  * `Image.cstr()` returned None for a ZERO-LENGTH string, so retail's
    `Symbol("")` vanished from the list while our side (via demangle_literal)
    correctly produced "".  That single line WAS the entire "5 EMPTY rows"
    artifact class.  CS-4 counted it and mis-attributed it to "global char*";
    CT-4 proved that mechanism false but left the bug.  Now fixed at the root
    -- the class is 0 by construction, and the detector below is kept only so a
    regression shows up as a nonzero count.
  * `defines()` treated floating-point opcodes 59/63 as UNMODELLED, hence as
    BARRIERS.  An `fmr f30,f1` scheduled between `lis r11,hi` and
    `addi r4,r11,lo` truncated the backward scan and DROPPED the property.
    Also fixed: load/store-with-update forms write RA (were reported as writing
    nothing), and `lhz` was in neither list.  Coverage 10,927 -> 11,005 names
    (91.13% -> 91.78%); barriers 125 -> 44.

THE SOURCE-BLOCK INDEX IS THE REAL WIDENING (stratum b).  CT-4 observed that
stratum (b) "STRUCTURALLY CANNOT FIRE" on HANDLE/CUSTOM_PROPSYNC bodies because
it only scanned `BEGIN_PROPSYNCS`.  CU-1 then had to identify Watcher::Handle ->
BandConfiguration::Handle BY HAND, out of an oracle the tool never read.  Both
gaps are closed here: the index scans BEGIN_PROPSYNCS **+ BEGIN_HANDLERS +
BEGIN_CUSTOM_PROPSYNC**, across **our tree + rb3-Wii + DC3**.

⚠ ARTIFACT TAXONOMY IS PART OF THE OUTPUT, NOT PROSE.  CS-4's raw count was 62
where the real number was 11 -- a 15:1 false-positive ratio -- and the
subtraction lived in a docstring.  `classify()` below returns a label for every
mismatching row and the summary prints the histogram.  Quote the REAL bucket,
never the raw count.
"""
import collections
import json
import os
import re
import sys

WT = os.environ.get("WT", os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(WT, "tools"))
import retail_props as RP  # noqa: E402

ORACLES = [("ours", os.path.join(WT, "src")),
           ("rb3wii", "/home/free/code/milohax/rb3/src"),
           ("dc3", "/home/free/code/milohax/dc3-decomp/src")]

# First macro argument is the wire Symbol name (Milo convention `Symbol foo("foo")`;
# the local-static variants literally stringize it as `#symbol`).
HANDLE_NAMED = ("HANDLE", "HANDLE_ACTION", "HANDLE_EXPR", "HANDLE_ACTION_IF",
                "HANDLE_ACTION_STATIC", "HANDLE_EXPR_STATIC",
                "HANDLE_ACTION_IF_ELSE", "HANDLE_STATIC")
# Deliberately EXCLUDED, each for a different reason -- including any of these
# would inject a phantom name into every list it appears in:
#   HANDLE_SUPERCLASS/HANDLE_FORWARD  arg is a CLASS or a function, not a symbol
#   HANDLE_MESSAGE                    arg is a msg TYPE; the Symbol is built in
#                                     the message class, not in this function
#   HANDLE_MEMBER_PTR                 forwards to member->Handle, emits no name
#   HANDLE_CHECK                      arg is a __LINE__ number
#   HANDLE_CONDITION                  FIRST ARG IS A CONDITION, not a symbol
_HANDLE_RE = re.compile(r"^[ \t]*(" + "|".join(HANDLE_NAMED) + r")\s*\(\s*([A-Za-z_]\w*)",
                        re.M)
_BLOCK_RE = re.compile(r"BEGIN_(PROPSYNCS|CUSTOM_PROPSYNC|HANDLERS)\(\s*([A-Za-z_][\w:]*)")


def scan_blocks(tree):
    """-> [(kind, cls, [names], path)] for every macro block under `tree`."""
    out = []
    for dp, _, fs in os.walk(tree):
        for f in fs:
            if not f.endswith((".cpp", ".c", ".h", ".inl")):
                continue
            p = os.path.join(dp, f)
            try:
                txt = open(p, errors="replace").read()
            except OSError:
                continue
            if "BEGIN_" not in txt:
                continue
            for m in _BLOCK_RE.finditer(txt):
                kind = m.group(1)
                end = txt.find("END_HANDLERS" if kind == "HANDLERS"
                               else ("END_CUSTOM_PROPSYNC" if kind == "CUSTOM_PROPSYNC"
                                     else "END_PROPSYNCS"), m.end())
                if end < 0:
                    continue
                body = txt[m.end():end]
                names = ([x[1] for x in _HANDLE_RE.findall(body)] if kind == "HANDLERS"
                         else RP.SRC_RE.findall(body))
                if names:
                    out.append((kind, m.group(2), names, p))
    return out


# --------------------------------------------------------------- taxonomy ---

def _t32(a, b):
    """MSVC truncates `??_C@` literal SYMBOL NAMES at 32 chars, so OUR side can
    legitimately read a 32-char prefix of retail's string.  46 rows on this
    corpus.  Symmetric on purpose -- the truncated side is whichever is 32."""
    if a == b:
        return True
    if len(b) == 32 and a.startswith(b):
        return True
    if len(a) == 32 and b.startswith(a):
        return True
    return False


def classify(t, b):
    """Label one (retail, ours) pair.  None means the lists agree.

    Returns one of:
      'T32'        pure MSVC 32-char symbol-name truncation -- ARTIFACT
      'EMPTY'      zero-length literal handled asymmetrically -- ARTIFACT,
                   fixed at the root in CV-3 so this MUST print 0; a nonzero
                   count is a REGRESSION of Image.cstr(), not a new finding
      'OURS_NONE'  ★ THE DOMINANT NEW ARTIFACT CLASS THE WIDENING EXPOSED, and
                   the one to subtract before quoting any number.  OUR side
                   emits ZERO literal Symbol ctors while retail emits some, so
                   there is nothing to compare and the row is UNDECIDABLE --
                   not wrong.  Cause is not naming at all: our headers
                   reference CENTRALIZED GLOBAL Symbols (Symbols2/3/4.h) where
                   retail builds FUNCTION-LOCAL statics, the exact divergence
                   `RB3_HANDLE_LOCAL_STATIC` in obj/ObjMacros.h describes.
                   64 of the 68 raw DISJOINT rows are this.  Left in the output
                   because the retail list is still usable for identification:
                   if it equals the source block of the symbol's OWN class the
                   PAIRING IS CONFIRMED (a local-static lever candidate), and
                   if it equals a DIFFERENT class's block it is a mispair lead.
      'ORDER'      same names, different order -- REAL (CT-4's TourPerformerLocal)
      'RENAME'     same length, differing names -- REAL, and the purest member
                   of the invisible class: a wrong name is free under the mask
      'SUBSET'     ours is missing properties retail has
      'SUPERSET'   ours has properties retail does not
      'DISJOINT'   no overlap at all -- the MAP MISPAIR signature
      'LENGTH'     lengths differ some other way
    ⚠ Only ORDER and RENAME are guaranteed metric-invisible.  A length change
    alters the instruction count, so SUBSET/SUPERSET/DISJOINT rows CAN move the
    ruler -- which is exactly why they are labelled separately rather than
    lumped into one "defects" number.
    """
    if t == b:
        return None
    if b == [] and t:
        return "OURS_NONE"
    if t == [] and b:
        return "RETAIL_NONE"
    if len(t) == len(b):
        if all(_t32(x, y) for x, y in zip(t, b)):
            return "T32"
        if any((x == "") != (y == "") for x, y in zip(t, b)):
            return "EMPTY"
        if sorted(t) == sorted(b):
            return "ORDER"
        return "RENAME"
    st, sb = set(t), set(b)
    if not (st & sb):
        return "DISJOINT"
    if st > sb:
        return "SUBSET"
    if sb > st:
        return "SUPERSET"
    return "LENGTH"


# ----------------------------------------------------------------- report ----

def main():
    ex = RP.Extractor(WT)
    od = json.load(open(os.path.join(WT, "objdiff.json")))
    rep = json.load(open(os.path.join(WT, "build/45410914/report.json")))

    # ---- percents, STRICTLY by key. -------------------------------------
    # `match_percent_normalized` is `optional float` in report.proto and
    # report.rs always sets Some(..) -> present on EVERY record; absence is an
    # anomaly and we REFUSE.
    # `fuzzy_match_percent` is a PLAIN proto3 float, so JSON omits it exactly
    # when it is 0.0.  ⚠ Absence therefore MEANS 0.0 -- verified: 22,610 of the
    # 22,695 absent records also carry mpn == 0.0.  A previous brief called
    # `.get(...,0.0)` an opportunity-manufacturing bug; it is not.  The genuine
    # trap is the NAMING: `Measures.fuzzy_match_percent` is accumulated from
    # `match_percent_normalized` (report.rs:828) while `ReportItem
    # .fuzzy_match_percent` is the RAW per-function value.  Same name, two
    # different rulers.
    pct = {}
    n_no_mpn = 0
    for u in rep["units"]:
        for f in u.get("functions", []):
            if "match_percent_normalized" not in f:
                n_no_mpn += 1
                continue
            pct[(u["name"], f["name"])] = (
                float(f["match_percent_normalized"]),
                float(f.get("fuzzy_match_percent", 0.0)),
                bool(f.get("masked_equal", False)),
                int(f["size"]))
    if n_no_mpn:
        raise SystemExit(f"REFUSE: {n_no_mpn} records lack match_percent_normalized")
    print(f"report.json: {len(pct)} function records, all with mpn (strict-key read OK)")

    # ---- widened source-block index -------------------------------------
    blocks, per_tree = [], {}
    for tag, tree in ORACLES:
        if not os.path.isdir(tree):
            print(f"  WARN oracle missing: {tree}")
            continue
        bl = scan_blocks(tree)
        per_tree[tag] = len(bl)
        for kind, cls, names, p in bl:
            blocks.append((tag, kind, cls, names, p))
    src_index = collections.defaultdict(list)
    for tag, kind, cls, names, p in blocks:
        src_index["|".join(names)].append((tag, kind, cls, p))
    kinds = collections.Counter(b[1] for b in blocks)
    print(f"source-block index: {len(blocks)} blocks from {per_tree} "
          f"kinds={dict(kinds)} distinct-lists={len(src_index)}")

    # ---- walk every pinned target obj ------------------------------------
    rows, unnamed = [], []
    n_named = n_named_100 = n_named_sub100 = 0
    for u in od["units"]:
        tp, bp = u.get("target_path"), u.get("base_path")
        if not tp:
            continue
        p = os.path.join(WT, tp)
        if not os.path.exists(p):
            continue
        c = ex.obj(p)
        if c is None:
            continue
        for name, s in list(c.symbol_map.items()):
            if s.get("section", 0) <= 0 or name.startswith((".", "except_data", "__", "$")):
                continue
            try:
                t = ex.props(p, name)
            except Exception:
                continue
            if not t:
                continue
            if re.fullmatch(r"fn_[0-9A-Fa-f]{8}", name):
                unnamed.append((u["name"], name, t))
                continue
            n_named += 1
            pr = pct.get((u["name"], name))
            if pr is None:
                continue
            mpn, fz, masked, size = pr
            if mpn >= 100.0:
                n_named_100 += 1
            else:
                n_named_sub100 += 1
            b = ex.props(os.path.join(WT, bp), name) if bp else None
            if b is None:
                continue
            lab = classify(t, b)
            if lab:
                rows.append(dict(unit=u["name"], sym=name, retail=t, ours=b,
                                 label=lab, mpn=mpn, fuzzy=fz, masked=masked,
                                 size=size))
        ex._objs.pop(p, None)
        ex._boundcache.pop(p, None)
        for k in [k for k in ex._seccache if k[0] == p]:
            ex._seccache.pop(k, None)

    tot = n_named + len(unnamed)
    print("\n=== COVERAGE (pinned prop-bearing retail symbols) ===")
    print(f"  total                     {tot}")
    print(f"  named                     {n_named}  ({100.0*n_named/tot:.1f}%)")
    print(f"    at 100.0 (mpn)          {n_named_100}")
    print(f"    sub-100  (mpn)          {n_named_sub100}   <-- NEW, was invisible")
    print(f"  unnamed (anon gate)       {len(unnamed)}")
    print(f"  OLD finder visibility     {n_named_100}/{tot} = "
          f"{100.0*n_named_100/tot:.1f}%")
    print(f"  NEW finder visibility     {n_named}/{tot} = {100.0*n_named/tot:.1f}%"
          f"   (self-comparable rows)")

    hist = collections.Counter(r["label"] for r in rows)
    h100 = collections.Counter(r["label"] for r in rows if r["mpn"] >= 100.0)
    print(f"\n=== TAXONOMY of {len(rows)} mismatching named rows ===")
    print(f"  {'label':10s} {'total':>6s} {'at-100':>7s} {'sub-100':>8s}")
    for k in ("T32", "EMPTY", "OURS_NONE", "RETAIL_NONE", "ORDER", "RENAME",
              "DISJOINT", "SUBSET", "SUPERSET", "LENGTH"):
        if hist[k]:
            print(f"  {k:11s} {hist[k]:6d} {h100[k]:7d} {hist[k]-h100[k]:8d}")
    SUBTRACT = ("T32", "EMPTY", "OURS_NONE", "RETAIL_NONE")
    art = sum(hist[k] for k in SUBTRACT)
    invis = hist["ORDER"] + hist["RENAME"]
    print(f"  ARTIFACT/UNDECIDABLE {art}   REAL {len(rows)-art}")
    print(f"  ...of the REAL rows, METRIC-INVISIBLE (ORDER+RENAME, same length"
          f" so no instruction-count change): {invis}")
    print("  ⚠ Quote the REAL bucket, never the raw count.  CS-4 printed 62"
          " where the truth was 11.")

    print("\n=== REAL rows (artifacts + undecidable subtracted) ===")
    real = [r for r in rows if r["label"] not in SUBTRACT]
    real.sort(key=lambda r: (-r["mpn"], r["unit"]))
    for r in real:
        flag = "" if r["mpn"] < 100.0 else ("  [at-100 INVISIBLE]"
                                            if r["label"] in ("ORDER", "RENAME") else "")
        print(f"  [{r['label']}] {r['unit']}  {r['sym'][:64]}")
        print(f"       mpn={r['mpn']:.4f} fuzzy={r['fuzzy']:.4f} "
              f"masked={r['masked']} size={r['size']}{flag}")
        # Show only the DIFFERING positions.  Printing the head of a 100-entry
        # list hid the actual defect in BandCharacter::Handle (both heads were
        # identical for 14 entries) -- a display bug that reads as a tool bug.
        d = [(i, x, y) for i, (x, y) in enumerate(zip(r["retail"], r["ours"]))
             if x != y]
        for i, x, y in d[:6]:
            print(f"       [{i}] retail={x!r}  ours={y!r}")
        if len(r["retail"]) != len(r["ours"]):
            print(f"       len retail={len(r['retail'])} ours={len(r['ours'])}"
                  f"  retail-only={sorted(set(r['retail'])-set(r['ours']))[:6]}"
                  f"  ours-only={sorted(set(r['ours'])-set(r['retail']))[:6]}")
        elif not d:
            print(f"       (equal elementwise?! label={r['label']})")
        hit = src_index.get("|".join(r["retail"]))
        if hit:
            print(f"       *** retail list IS a known source block: {hit[:4]}")

    # ---- stratum (b): repoint candidates over the WIDENED index ----------
    print("\n=== (b) retail list matches EXACTLY ONE source class (repoint candidates) ===")
    nb = 0
    for r in real:
        hit = src_index.get("|".join(r["retail"]))
        if hit and len({h[2] for h in hit}) == 1 and len(r["retail"]) >= 3:
            nb += 1
            print(f"  {r['unit']}  {r['sym'][:56]}  ->  {hit[0][2]} "
                  f"({hit[0][1]}, {hit[0][0]})")
    print(f"  candidates: {nb}    (CS-4/CT-4 reported 0 -- their index could not"
          f" see HANDLERS/CUSTOM_PROPSYNC or the oracles)")

    # ---- OURS_NONE rows are UNDECIDABLE for naming but usable for ID -----
    # `_symclass` must handle FREE functions.  A naive `^\?\w+@(\w+)@@` returns
    # None for `?PropSync@@YA_NAAVOverlay@OutfitConfig@@...`, which made an
    # ad-hoc version of this check flag CU-1's CORRECTLY-repointed
    # OutfitConfig::Overlay row as "different class" -- a false positive in the
    # discriminator itself.
    def _symclass(s):
        m = re.match(r"\?\w+@(\w+)@@", s)
        if m:
            return m.group(1)
        m = re.search(r"@[UV](\w+)@(\w+)@@AAVDataNode", s)   # free PropSync(Outer::Inner&)
        if m:
            return m.group(2)
        return None

    conf, lead = [], []
    for r in rows:
        if r["label"] != "OURS_NONE":
            continue
        hit = src_index.get("|".join(r["retail"]))
        if not hit:
            continue
        c = _symclass(r["sym"])
        (conf if c and any(h[2].split("::")[0] == c for h in hit) else lead).append((r, hit))
    print(f"\n=== OURS_NONE rows that a source block can still adjudicate ===")
    print(f"  pairing CONFIRMED (retail list == own class's block) : {len(conf)}"
          f"   -> local-static (RB3_HANDLE_LOCAL_STATIC) lever candidates")
    print(f"  pairing SUSPECT   (retail list == ANOTHER class)     : {len(lead)}"
          f"   -> mispair leads")
    for r, hit in lead:
        if len(r["retail"]) < 3:
            continue          # CS-4: <3 properties is not trustworthy alone
        print(f"    {r['unit']}  {r['sym'][:56]}  mpn={r['mpn']:.2f}")
        print(f"        retail={r['retail'][:6]}  ->  {sorted({(h[1], h[2]) for h in hit})}")

    # ---- stratum (c): unnamed, against the widened index -----------------
    solved = []
    for unit, name, t in unnamed:
        hit = src_index.get("|".join(t))
        if hit and len({h[2] for h in hit}) == 1:
            solved.append((unit, name, t, hit[0]))
    strong = [r for r in solved if len(r[2]) >= 3]
    print(f"\n=== (c) UNNAMED prop-bearing retail fns: {len(unnamed)} ===")
    print(f"  matched to EXACTLY ONE source class: {len(solved)}  "
          f"(STRONG, >=3 props: {len(strong)})")
    print("  ⚠ <3 properties is NOT trustworthy on its own (CS-4): ['label'] "
          "matches FlowLabel\n     but the fn is provably UIListLabel::SyncProperty.")
    byk = collections.Counter(r[3][1] for r in strong)
    print(f"  STRONG by block kind: {dict(byk)}")
    for r in sorted(strong, key=lambda x: -len(x[2])):
        print(f"    {r[0]:38s} {r[1]}  n={len(r[2]):2d}  ->  {r[3][2]:28s} "
              f"[{r[3][1]}, {r[3][0]}]  {r[3][3].replace(WT + '/', '')}")

    json.dump({"rows": rows, "unnamed": unnamed,
               "solved": [[a, b, c, list(d)] for a, b, c, d in solved]},
              open(os.environ.get("CV3_OUT", "propdefects.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
