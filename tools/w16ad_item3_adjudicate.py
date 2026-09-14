#!/usr/bin/env python3
"""W16-AD Item 3: adjudicate the Accomplishment/Goal algorithm-instantiation
map rows on RETAIL BYTES.

THE INVARIANT AND WHY IT IS ADJUDICABLE
=======================================
An STL algorithm instantiated on comparator `XCmp` calls `??RXCmp@@` -- our
build satisfies this 24/24 with no exceptions, so it is a property of the
source, not of a name.  The comparator map rows are independently sound: their
retail `.pdata` extents discriminate all three candidates by SIZE
(`??RAccomplishmentCmp` 104 B @0x825f7000, `??RAccomplishmentCategoryCmp`
112 B @0x825f6f20, `??RGoalCmp` 144 B -- absent).  So decoding retail's call at
the offset where OUR body calls the comparator names the comparator retail's
function is instantiated on, and therefore names the function.

⛔ THE MASKED-BODY TEST CANNOT DO THIS.  `w16aa_adjudicate_dest.py` reports BOTH
candidates MASKED-EQ at BOTH addresses (84 B / 3 relocs each) -- necessarily,
because the comparator `bl` is precisely the relocated word masking removes.
The discriminator has to be the decoded DESTINATION, not the masked body.

⛔ AND A PROVEN-WRONG NAME IS NOT A LICENCE TO RENAME.  Un-pairing is 80.5% of
a map edit's delta, so a row is only repaired when the obj that would pair
DEFINES the new name, and when both sides of the swap are charged (a side that
already reads 100 is being forgiven a placeholder destination -- renaming it
converts a forgiven site into a checked one and can only lose).
"""
import collections
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                     # noqa: E402
from wrong_callee_triage import Image                # noqa: E402

BUILD = "45410914"
CMPS = ("AccomplishmentCmp", "AccomplishmentCategoryCmp",
        "AccomplishmentGroupCmp", "GoalCmp", "GoalAlpaCmp")


def cmp_of(name):
    """Which comparator does this symbol name involve?  Longest match wins.

    Two spellings matter and BOTH are load-bearing: a template ARGUMENT is
    `V<C>@@` / `U<C>@@`, while the comparator's own `operator()` is
    `??R<C>@@...`.  Missing the second spelling silently files every decoded
    destination as DEST_NOT_A_COMPARATOR -- i.e. it reads as "no evidence"
    rather than as a bug, which is why it has to be tested, not assumed.
    """
    if name is None:
        return None
    best = None
    for c in CMPS:
        if ("V" + c + "@@" in name or "U" + c + "@@" in name
                or name.startswith("??R" + c + "@@")):
            if best is None or len(c) > len(best):
                best = c
    return best


def main():
    # ---- our side: algorithm COMDAT -> (comparator offset, comparator name)
    ours = {}
    for p in glob.glob(str(ROOT / "build" / BUILD / "src/**/*.obj"), recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code") or "stlpmtx_std" not in n:
                continue
            if cmp_of(n) is None:
                continue
            sites = [(o, s) for o, s, _t in (v["fn_relocs"] or [])
                     if s.startswith("??R")]
            if len(sites) == 1:
                ours[n] = (sites[0][0], sites[0][1], len(v["fn_raw"]),
                           Path(p).name)

    # ---- map: name -> va, and va -> name
    smap = json.loads((ROOT / "scripts/target_symbol_map.json").read_text())
    byname, byva = {}, {}
    for k, v in smap.items():
        try:
            va = int(k, 16)
        except ValueError:
            continue
        nm = v if isinstance(v, str) else None
        if nm:
            byname[nm] = va
            byva[va] = nm

    # ---- report rows
    rep = json.load(open(ROOT / "build" / BUILD / "report.json"))
    fuzzy = {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            fuzzy[f.get("name", "")] = (float(f.get("fuzzy_match_percent", 0)),
                                        int(f.get("size", 0)), u["name"])

    img = Image(ROOT / "orig" / BUILD / "band.exe")

    rows = []
    for n, (off, our_cmp, sz, obj) in sorted(ours.items()):
        va = byname.get(n)
        if va is None:
            rows.append((n, None, off, our_cmp, None, None, sz, obj, "NOT_IN_MAP"))
            continue
        o = img.off(va + off)
        dest = None
        if o is not None:
            w = struct.unpack_from(">I", img.data, o)[0]
            if (w >> 26) == 18 and not (w & 2):
                d = w & 0x03FFFFFC
                if d & 0x02000000:
                    d -= 0x04000000
                dest = (va + off + d) & 0xFFFFFFFF
        dname = byva.get(dest) if dest else None
        retail_cmp = cmp_of(dname) if dname else None
        if dest is None:
            verdict = "NO_BRANCH_AT_OFFSET"
        elif dname is None:
            verdict = "DEST_UNNAMED"
        elif retail_cmp is None:
            verdict = "DEST_NOT_A_COMPARATOR"
        elif retail_cmp == cmp_of(n):
            verdict = "CONSISTENT"
        else:
            verdict = "INCONSISTENT"
        rows.append((n, va, off, our_cmp, dest, dname, sz, obj, verdict))

    # ---- report
    tally = collections.Counter(r[8] for r in rows)
    print(f"{len(rows)} algorithm instantiations with exactly one comparator reloc")
    for k, v in sorted(tally.items()):
        print(f"   {v:3d}  {k}")
    print()

    # group by (algorithm, obj) to expose swap pairs
    def algo(n):
        return n.split("@PAVSymbol")[0]

    groups = collections.defaultdict(list)
    for r in rows:
        groups[algo(r[0])].append(r)

    print("=" * 100)
    print("PER-ALGORITHM TABLE  (fz = our row's current fuzzy_match_percent)")
    print("=" * 100)
    swaps = []
    for a in sorted(groups):
        print(f"\n{a}")
        for n, va, off, oc, dest, dn, sz, obj, verdict in sorted(groups[a],
                                                                key=lambda r: r[0]):
            fz, rsz, unit = fuzzy.get(n, (float("nan"), 0, "-"))
            vas = f"0x{va:08x}" if va else "  (unmapped)"
            ds = f"0x{dest:08x}" if dest else "    -     "
            print(f"   {vas} +0x{off:02x} -> {ds} {str(cmp_of(dn) if dn else dn):26s}"
                  f" ours={oc.split('@@')[0][3:]:26s} fz={fz:9.5f} {verdict}")
        # a clean swap pair: two INCONSISTENT rows pointing at each other,
        # both charged, both defined in the same obj
        inc = [r for r in groups[a] if r[8] == "INCONSISTENT"]
        for i, ri in enumerate(inc):
            for rj in inc[i + 1:]:
                ci, cj = cmp_of(ri[0]), cmp_of(rj[0])
                di, dj = cmp_of(ri[5]), cmp_of(rj[5])
                if di == cj and dj == ci and ri[7] == rj[7]:
                    fi = fuzzy.get(ri[0], (100.0, 0, ""))
                    fj = fuzzy.get(rj[0], (100.0, 0, ""))
                    if fi[0] < 100.0 and fj[0] < 100.0:
                        swaps.append((a, ri, rj, fi, fj))

    print("\n" + "=" * 100)
    print("CLEAN SWAP PAIRS  (mutual, both charged, same defining obj)")
    print("=" * 100)
    tot = 0
    for a, ri, rj, fi, fj in swaps:
        print(f"\n{a}   obj={ri[7]}")
        print(f"   0x{ri[1]:08x} named <{cmp_of(ri[0])}> but calls <{cmp_of(ri[5])}>"
              f"  (our row fz={fi[0]:.5f}, {fi[1]} B)")
        print(f"   0x{rj[1]:08x} named <{cmp_of(rj[0])}> but calls <{cmp_of(rj[5])}>"
              f"  (our row fz={fj[0]:.5f}, {fj[1]} B)")
        tot += fi[1] + fj[1]
    print(f"\n{len(swaps)} clean swap pairs = {2*len(swaps)} row edits, "
          f"{tot} B currently withheld")
    return swaps


if __name__ == "__main__":
    main()
