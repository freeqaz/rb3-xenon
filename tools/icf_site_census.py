#!/usr/bin/env python3
"""Self-contained NAME-MISMATCH call-site census, computed straight from the COFF objs.

WHY THIS EXISTS
---------------
``tools/icf_alias_build.py`` (lane CB-11/A) consumed ``~/tmp/cb9_allsites.pkl`` -- an
ad-hoc lane CB-9 artifact whose generator was NEVER COMMITTED. That made the whole
ICF-alias pipeline unreproducible: the pickle silently ages against every map fix, and
its top pair (``?EasePolyIn@@YAMMMM@Z``) was a map defect ALREADY REPAIRED in 683ee54d.
A generator you cannot re-run is a number you cannot re-derive.

WHAT IT COMPUTES
----------------
Exactly the population objdiff's ``-c functionRelocDiffs=name_check`` charges, but
without objdiff: for every unit in report.json, pair functions BY NAME between the
dtk-split target obj (``build/45410914/obj/<Unit>.obj``, already renamed to mangled
names by obj_target_symbol_renamer) and our compiled obj
(``build/45410914/src/**/<Unit>.obj``), then walk the two relocation tables together
and record every slot where the TARGET-side symbol name differs from the BASE-side one.

ALIGNMENT GATE (this is the load-bearing part)
----------------------------------------------
Two relocation tables are only comparable slot-for-slot when they describe the same
code. We therefore require the two functions to agree on
    (a) body size, and
    (b) the full ``(offset, reloc_type)`` sequence.
Anything else is skipped as UNALIGNED. This is deliberately conservative: it keeps the
census to the population where a name difference is a pure NAMING question rather than
a codegen difference, which is precisely the population an ICF alias may speak to.
Aligning by index without gate (b) silently pairs unrelated slots once one side has an
extra relocation, and manufactures pairs that were never charged.

Output JSON (durable, unlike a pickle):
    {"records": [[unit, fn, [[kind, target_name, base_name], ...]], ...], "stats": {...}}

Read-only. Mutates no build input.

★ CY-1 (2026-08-02) -- TWO PAIRING/READER DEFECTS FIXED. ⚠ DEPENDENT CENSUSES MOVE.
------------------------------------------------------------------------------
Measured on one build state, all three legs against the same root:

    metric            OLD pair+legacy   NEW pair+legacy   NEW pair+EXT (default)
    units                        784               948               948
    fn_paired                 13,025            16,363            22,466
    fn_aligned                12,103            15,297            20,474
    fn_not_in_base            29,886            37,157            31,517
    fn_charged                 7,103             8,918            14,181
    sites                     25,310            31,446           103,337
    distinct pairs             6,873             8,268            27,685

Any number derived from this tool BEFORE this change was computed in the first
column. Re-derive with ICF_CENSUS_LEGACY_READER=1 (recovers column 2 only --
the pairing fix is unconditional, because keying on a bare stem could pair a
unit against ANOTHER UNIT'S obj and there is no reason to preserve that).

WHY THE EXT READER IS THE DEFAULT, and the check that nearly changed it: the
3.3x jump in `sites` looks alarming, and the obvious worry is that slicing by
consecutive symbol values shreds dtk's monolithic target `.text` into spurious
"functions". MEASURED OVER 60 RANDOM UNITS: ext-minus-legacy is +55 on TARGET
objs and +2,822 on OUR objs -- a 51x asymmetry. The recovery is happening in
our MSVC /Gy COMDATs, which is exactly the population coff_bodies_ext was
written and validated for; the target side barely moves. The extra sites are
real relocation slots in big EH-bearing functions that previously could not
pair at all (fn_not_in_base falls by 5,640).

⚠ HONEST SCOPE -- do not oversell this. Over CHARGED NAMED rows the coverage
only moves 8.5% -> 9.6%. The dominant miss is units with NO COMPILED OBJ AT ALL
(unported TUs): 5,323 rows / 70.7% / 2,345,255 B, which no instrument fixes.
Better alignment tiers can buy AT MOST 5.7% / 60,892 B.
"""

import argparse
import collections
import glob
import json
import os
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
from icf_fold_evidence import function_bodies  # noqa: E402
from coff_bodies_ext import function_bodies_ext  # noqa: E402

# ★ CY-1: the EXTENDED body reader is the DEFAULT, following the precedent
# tools/xbin_adjudicate.py already set with CW2_LEGACY_READER. The legacy
# `function_bodies` accepts a code section only when it holds EXACTLY ONE
# defining symbol AT OFFSET 0, and both halves of that gate misfire on every
# EH-bearing function (the 8-byte EH prefix pushes the entry off 0, and
# __unwind$NNN is a second type-0x20 def). Measured on this tree: 31,075
# two-def and 8,849 three-def sections were dropped ENTIRELY.
# ⚠ ICF_CENSUS_LEGACY_READER=1 re-derives any census computed before this
# change -- dependent numbers MOVE, see the header note on load_unit_objs.
USE_LEGACY_READER = os.environ.get("ICF_CENSUS_LEGACY_READER") == "1"

# COFF PowerPC relocation types we care to label. The reloc TYPE is a more robust
# "kind" than decoding the instruction: type 6 is the call/branch relocation.
KIND = {0x06: "bl", 0x10: "refhi", 0x11: "reflo", 0x12: "pair", 0x02: "addr32",
        0x03: "addr24", 0x0A: "toc", 0x07: "rel14"}


def load_unit_objs(root: Path):
    """unit name -> (target_obj_path, our_obj_path). Only units having BOTH.

    ★ CY-1: pair from objdiff.json, which states each unit's target_path and
    base_path AUTHORITATIVELY. The previous implementation globbed
    ``obj/*.obj`` NON-RECURSIVELY and keyed on the bare ``Path(p).stem``, which
    was wrong three separate ways:
      * target objs also live in SUBDIRECTORIES -- 12,874 found vs 13,159
        recursive, so 285 target objs were invisible;
      * ``setdefault`` on a bare stem silently kept whichever of 19 COLLIDING
        stems the glob happened to yield first (e.g. rndobj/Utl vs another
        Utl), so some units were compared against ANOTHER UNIT'S obj;
      * net effect: it walked 784 units when 948 are pairable.
    Fixing it moves units 784 -> 948, fn_paired 13,028 -> 16,367 (+25.6%) and
    fn_aligned 12,103 -> 15,297 (+26.4%).
    ⚠ SCOPE, so nobody oversells it: over CHARGED NAMED rows the coverage only
    moves 8.5% -> 9.6%, because 70.7% of the miss is units with NO COMPILED OBJ
    AT ALL (unported TUs, 2,345,255 B), which no instrument fixes. Better
    alignment can buy AT MOST 5.7% / 60,892 B.
    """
    cfg = json.loads((root / "objdiff.json").read_text())
    out = {}
    for u in cfg.get("units", []):
        t, b = u.get("target_path"), u.get("base_path")
        if not t or not b:
            continue
        tp, bp = root / t, root / b
        if tp.exists() and bp.exists():
            out[u["name"]] = (str(tp), str(bp))
    return out


def index_fns(path):
    out = {}
    if USE_LEGACY_READER:
        # DC-4: legacy_ok=True -- this branch IS the deliberate legacy hatch
        # (ICF_CENSUS_LEGACY_READER=1), so it must not trip the frozen-reader
        # warning. A warning that fires on its own sanctioned use is noise.
        for name, raw, relocs in function_bodies(Path(path), legacy_ok=True):
            out.setdefault(name, (len(raw), relocs))
    else:
        for name, raw, relocs, _entry in function_bodies_ext(Path(path)):
            out.setdefault(name, (len(raw), relocs))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=str(PROJECT_ROOT))
    ap.add_argument("--out", default=str(Path.home() / "tmp" / "cd9_allsites.json"))
    args = ap.parse_args()
    root = Path(args.root)

    pairs = load_unit_objs(root)
    print(f"[1/2] {len(pairs)} units have BOTH a target obj and a compiled obj",
          file=sys.stderr)

    records = []
    st = collections.Counter()
    for unit, (tp, op) in sorted(pairs.items()):
        try:
            tf, of = index_fns(tp), index_fns(op)
        except Exception as e:                                    # pragma: no cover
            st["unit_parse_error"] += 1
            print(f"      !! {unit}: {e}", file=sys.stderr)
            continue
        st["units"] += 1
        for fn, (tsize, trl) in tf.items():
            ob = of.get(fn)
            if ob is None:
                st["fn_not_in_base"] += 1
                continue
            osize, orl = ob
            st["fn_paired"] += 1
            strictly_aligned = (
                tsize == osize and len(trl) == len(orl) and
                [(o, t) for o, _n, t in trl] == [(o, t) for o, _n, t in orl])
            if strictly_aligned:
                st["fn_aligned"] += 1
                slots = list(zip(trl, orl))
            else:
                # ★ CD-9: the strict gate above is the RIGHT gate for asking "is this
                # slot charged", but the WRONG gate for ENUMERATING candidate fold
                # pairs, and that distinction cost 199 groups when this tool was first
                # written. A fold is a property of the CALLEE pair (S,F) -- retail's
                # body for S versus our body for F -- and the adjudicator re-derives it
                # from the objs directly. How well the CALLER matched has no bearing on
                # it. Discarding a caller because its data relocations drifted throws
                # away perfectly good callee candidates that are then adjudicated
                # against retail bytes anyway.
                #
                # So fall back to aligning the CALL relocations only (type 0x06) by
                # index, which is sound whenever both sides make the same number of
                # calls: relocation order is code order, so the i-th call on each side
                # is the i-th call site. Data relocations are left out of this tier
                # precisely because they are what tends to drift.
                tb = [(o, n, t) for o, n, t in trl if t == 0x06]
                ob_ = [(o, n, t) for o, n, t in orl if t == 0x06]
                if not tb or len(tb) != len(ob_):
                    st["fn_unaligned"] += 1
                    continue
                st["fn_bl_aligned"] += 1
                slots = list(zip(tb, ob_))
            rows = []
            for (o, tn, ty), (_o, bn, _ty) in slots:
                if tn != bn:
                    rows.append([KIND.get(ty, "t%d" % ty), tn, bn])
            if rows:
                st["fn_charged"] += 1
                st["sites"] += len(rows)
                records.append([unit, fn, rows])

    Path(args.out).write_text(json.dumps({"records": records, "stats": dict(st)}))
    print(f"[2/2] wrote {args.out}", file=sys.stderr)
    print("\n=== census ===")
    for k in ("units", "fn_paired", "fn_aligned", "fn_bl_aligned", "fn_unaligned",
              "fn_not_in_base", "fn_charged", "sites"):
        print("  %-18s %7d" % (k, st[k]))
    dp = collections.Counter()
    for _u, _f, rows in records:
        for _k, t, b in rows:
            dp[(t, b)] += 1
    print("  %-18s %7d" % ("distinct pairs", len(dp)))
    print("\n  top 10 pairs:")
    for (t, b), n in dp.most_common(10):
        print("   %5d  %s\n          <- %s" % (n, t, b))
    return 0


if __name__ == "__main__":
    sys.exit(main())
