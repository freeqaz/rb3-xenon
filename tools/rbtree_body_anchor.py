#!/usr/bin/env python3
"""W21-CARVE: an anchor for the `_Rb_tree` components the node-size sweep CANNOT test.

WHY THIS EXISTS
---------------
`tools/rbtree_family_sweep.py` (W17) adjudicates a component by comparing RETAIL's
node function -- `_M_create_node` (builder) or `_M_erase` (eraser) -- against OUR
compiled one for the tree the map declares. That test needs an ALLOCATION SITE: a
`li r3,N`. W17 recorded, in its own doc rather than letting a clean run read as a
clearance, that **35 of its 86 components reach no builder and no eraser** and are
therefore UNTESTED BY CONSTRUCTION: `_M_find`, `swap`, `begin`, `_M_lower_bound`,
bare `_M_erase`. A clean sweep clears the 51, never the 86.

THE ANCHOR
----------
Compare the RETAIL BODY against OUR OWN COMPILED COMDAT OF THE VERY SAME MANGLED
NAME, word by word, with branch displacements masked:

    retail bytes at the mapped VA   (extent from symbols.txt, never a fixed window)
    vs
    our COMDAT with byte-identical mangled name, from build/45410914/src/**/*.obj

No `li r3,N` is required, so this reaches every member kind. It is the same
instrument that settled the `map<int,bool>` chain in this lane -- five members,
zero non-branch word differences -- and the `MemOrPoolAllocSTL` allocator.

WHAT A VERDICT MEANS, AND WHAT IT DOES NOT
------------------------------------------
* `SIZE_MISMATCH` / `SHAPE_MISMATCH` -- the declared name is REFUTED *or* our
  source diverges from retail. For STL template COMDATs our source is STLport and
  is normally identical, which is what makes a disagreement informative; it is
  still an adjudication prompt, never an automatic map defect.
* `BODY_IDENTICAL` -- consistent. NOT proof of identity: ICF twins (an enum key vs
  `int`, two pointer keys) compile to identical bodies, so several spellings pass
  the same test. This lane proved exactly that for `map<int,bool>` vs
  `map<TrackType,bool>`. Treat a pass as SHAPE CONSISTENCY.
* `NO_OUR_COMDAT` -- our build does not instantiate that name, so NOTHING WAS
  TESTED. Reported as its own class and never folded into the clean count; a tool
  that silently drops these confirms whatever it is pointed at (the `all([])` trap).

⛔ Branch displacements are masked, so this test says nothing about whether a
`bl` resolves to the RIGHT callee. That is a relocation-name question and belongs
to `name_check` / retail-byte callee adjudication, not here.

MEASURED ON FIRST RUN (2026-08-17, lane W21-CARVE) -- read these before believing
any verdict this tool prints:

* Selftest: 156 of 202 sabotages flagged (77.2%), so the instrument DISCRIMINATES.
  The 22.8% that survive are ICF twins and are the honest blind spot.
* 251 mapped tree rows: 215 BODY_IDENTICAL, 16 SHAPE_MISMATCH, 9 SIZE_MISMATCH,
  9 NO_OUR_COMDAT, 2 NO_EXTENT.
* On W17's NO_NODE_FN class (34 members): **30 newly REACHED**, 4 still
  NO_OUR_COMDAT. So the class the node-size sweep cannot test shrinks 34 -> 4.

⚠ A SIZE_MISMATCH OF EXACTLY 8 IS *NOT* AUTOMATICALLY THE STLPORT-1 FUNCLET
ARTIFACT -- I assumed it would be and MEASURED OTHERWISE. `0x8243c3c0`
(`_M_find<Edge@RndAmbientOcclusion>`) is +8 with NO `except_data` at
`addr + extent`; retail calls the `__savegprlr` helper where our build INLINES the
save/restore, and our extra 8 bytes are an inlined `mtlr r12; blr` epilogue.
Registers differ throughout. That is the "our source diverges" arm of the verdict,
not a wrong name. **Test the artifact hypothesis per row (is there an 8-byte
`except_data` at `addr+extent`?); never infer it from the number 8.**

Usage:
    python3 rbtree_body_anchor.py [--sweep w17.json] [--json out.json]
    python3 rbtree_body_anchor.py --selftest    # prove it can FAIL
"""
import argparse
import bisect
import collections
import glob
import importlib.util
import json
import os
import re
import struct
import sys

ROOT = os.environ.get("RB3_ROOT", ".")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from retail_body import Img                       # noqa: E402
from coff_bodies_ext import function_bodies_ext   # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "fs", os.path.join(ROOT, "tools/rbtree_family_sweep.py"))
fs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fs)

SYM_RX = re.compile(
    r"^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:function size:0x([0-9A-Fa-f]+)")
TREE_TOKENS = ("_Rb_tree", "?$map@", "?$set@", "?$multimap@", "?$multiset@")


def load_extents():
    ext = {}
    with open(os.path.join(ROOT, "config/45410914/symbols.txt")) as fh:
        for line in fh:
            m = SYM_RX.match(line)
            if m:
                ext[int(m.group(2), 16)] = int(m.group(3), 16)
    return ext


def our_comdats():
    """exact mangled name -> (size, body). First definition wins; COMDATs are
    identical across objs by construction, and we assert that rather than assume."""
    out, conflicts = {}, 0
    for path in glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"),
                          recursive=True):
        try:
            bodies = list(function_bodies_ext(path))
        except Exception:
            continue
        for name, body, _r, _e in bodies:
            if not any(t in name for t in TREE_TOKENS):
                continue
            prev = out.get(name)
            if prev is None:
                out[name] = (len(body), body)
            elif prev[0] != len(body):
                conflicts += 1
    return out, conflicts


def mask(w):
    """Blank a branch's displacement; keep opcode + link/absolute bits."""
    if (w >> 26) in (16, 18):
        return (w >> 26) << 26 | (w & 3)
    return w


def compare(retail, ours):
    if len(retail) != len(ours):
        return "SIZE_MISMATCH", abs(len(retail) - len(ours))
    n = 0
    for i in range(0, len(retail) // 4 * 4, 4):
        a = struct.unpack_from(">I", retail, i)[0]
        b = struct.unpack_from(">I", ours, i)[0]
        if mask(a) != mask(b):
            n += 1
    return ("BODY_IDENTICAL", 0) if n == 0 else ("SHAPE_MISMATCH", n)


def adjudicate(smap, ext, ours, img, addrs=None):
    rows = {}
    for a, name in smap.items():
        if addrs is not None and a not in addrs:
            continue
        if not isinstance(name, str):
            continue
        if not any(t in name for t in TREE_TOKENS):
            continue
        if a not in ext:
            rows[a] = (name, "NO_EXTENT", 0)
            continue
        got = ours.get(name)
        if got is None:
            rows[a] = (name, "NO_OUR_COMDAT", 0)
            continue
        try:
            body = img.read(a, ext[a])
        except Exception:
            rows[a] = (name, "NO_EXTENT", 0)
            continue
        v, n = compare(body, got[1])
        rows[a] = (name, v, n)
    return rows


def selftest(smap, ext, ours, img):
    """VACUITY CONTROL. Take rows that read BODY_IDENTICAL and swap the declared
    name for a DIFFERENT tree's name of the same member kind. The verdict must
    move off BODY_IDENTICAL. A check that only ever passes proves nothing."""
    base = adjudicate(smap, ext, ours, img)
    clean = [a for a, (_n, v, _c) in base.items() if v == "BODY_IDENTICAL"]
    if not clean:
        print("SELFTEST INCONCLUSIVE: no BODY_IDENTICAL row to sabotage")
        return 2
    by_kind = collections.defaultdict(list)
    for name in ours:
        k = fs.member_kind(name)
        if k:
            by_kind[k].append(name)
    tried = caught = 0
    for a in clean:
        name = smap[a]
        k = fs.member_kind(name)
        alts = [x for x in by_kind.get(k, [])
                if fs.tree_key(x) != fs.tree_key(name)]
        if not alts:
            continue
        for alt in alts[:4]:
            tried += 1
            v, _n = compare(img.read(a, ext[a]), ours[alt][1])
            if v != "BODY_IDENTICAL":
                caught += 1
        if tried >= 200:
            break
    rate = caught / tried if tried else 0.0
    print(f"SELFTEST: sabotaged {tried} (row, wrong-tree-name) pairs; "
          f"{caught} flagged ({rate:.1%})")
    print(f"SELFTEST: {'PASS' if caught else 'FAIL -- INSTRUMENT IS VACUOUS'}")
    print("  NOTE: a sabotage that survives is an ICF TWIN (identical body under a"
          " different spelling) -- that residue is the instrument's real blind"
          " spot and is reported, not hidden.")
    return 0 if caught else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sweep", help="rbtree_family_sweep --json output, to scope "
                                    "the report to its NO_NODE_FN components")
    ap.add_argument("--json")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    smap = {int(k, 16): v for k, v in
            json.load(open(os.path.join(ROOT, "scripts/target_symbol_map.json"))).items()
            if k.startswith("0x") and isinstance(v, str)}
    ext = load_extents()
    ours, conflicts = our_comdats()
    img = Img()
    print(f"our tree COMDATs indexed: {len(ours)}  (size conflicts: {conflicts})")
    if len(ours) < 100:
        sys.exit("REFUSED: implausibly few COMDATs indexed -- did the tree build? "
                 "(a reflinked worktree is PRE-RENAMER until its first build)")

    if args.selftest:
        sys.exit(selftest(smap, ext, ours, img))

    untested = set()
    if args.sweep:
        for comp in json.load(open(args.sweep)):
            if "NO_NODE_FN" in (comp.get("flags") or []):
                untested.update(int(x, 16) for x in comp["members"])

    rows = adjudicate(smap, ext, ours, img)
    tally = collections.Counter(v for _n, v, _c in rows.values())
    print(f"\nALL mapped tree rows: {len(rows)}")
    for k, v in tally.most_common():
        print(f"   {k:16s} {v}")

    if untested:
        sub = {a: r for a, r in rows.items() if a in untested}
        t2 = collections.Counter(v for _n, v, _c in sub.values())
        print(f"\nW17 NO_NODE_FN rows (untested by the node-size sweep): "
              f"{len(untested)} members, {len(sub)} mapped")
        for k, v in t2.most_common():
            print(f"   {k:16s} {v}")
        print("\n  newly REACHED (a real verdict where the sweep was silent): "
              f"{t2['BODY_IDENTICAL'] + t2['SIZE_MISMATCH'] + t2['SHAPE_MISMATCH']}")
        print(f"  still UNTESTABLE (no such COMDAT in our build): "
              f"{t2['NO_OUR_COMDAT'] + t2['NO_EXTENT']}")
        for a, (n, v, c) in sorted(sub.items()):
            if v in ("SIZE_MISMATCH", "SHAPE_MISMATCH"):
                print(f"   ** 0x{a:08x} {v} ({c}) {n[:100]}")

    if args.json:
        json.dump({hex(a): {"name": n, "verdict": v, "count": c}
                   for a, (n, v, c) in rows.items()},
                  open(args.json, "w"), indent=1)
        print(f"\nwrote {args.json}")


if __name__ == "__main__":
    main()
