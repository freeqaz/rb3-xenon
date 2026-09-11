#!/usr/bin/env python3
"""Assert that the objdiff-cli this repo CONSUMES still bounds a function at the
MSVC EH-funclet prefix by itself.

WHY (lane W4-E, 2026-09-11)
---------------------------
`scripts/obj_eh_boundary_patcher.py` plants a `$EH#####` boundary symbol at every
funclet EH prefix so objdiff stops our function where retail stops it.  objdiff
gained the identical mechanism natively in `b76f376` (2026-09-01, "Stop charging
the MSVC EH funclet prefix to the preceding function"), which backs
`next_address` off by 8 in `infer_symbol_sizes`.  Measured in this repo, whole
binary, same tree, one variable:

    objdiff b76f376  (live)   pass ON 42505 fns / 3834712 B ... pass OFF: IDENTICAL
    objdiff b76f376^ (pre)    pass ON 42505 fns / 3834712 B ... pass OFF: 42358 / 3811868
                                                               (-147 fns, -22844 B)

So the pass is redundant *only while the consumed binary carries that commit*.
objdiff-cli here is a PREBUILT binary shared with ../rb3 and ../dc3-decomp and
swapped by hand; CLAUDE.md documents fleet swaps and rollbacks as routine.  If
this pass is retired and the binary is ever rolled back past b76f376, 147
functions and 22,844 bytes disappear with NOTHING failing -- the score simply
reads lower, which is the failure class this repo has been bitten by repeatedly.

This guard closes that: it asserts the REDUNDANCY PROPERTY directly (stripping
the pass from an object changes nothing objdiff reports), not some proxy for the
implementation.  If it goes red, do not retire the pass -- or restore it.

CONTROL -- verified, and it is the whole point of the tool
----------------------------------------------------------
Run against an objdiff-cli built from `b76f376^` and this guard MUST fail.
Measured 2026-09-11 on the witness it selects by itself
(default/ClipDistMap `?_M_insert_overflow_aux@?$vector@VVector3@@...`):

    --objdiff <live, 0321226>   103 instrs both sides            -> PASS (rc=0)
    --objdiff <pre-fix probe>   103 patched vs 105 stripped      -> FAIL (rc=2)

A guard that cannot go red is not a guard; reproduce the red leg with

    git clone <objdiff> /tmp/x && git -C /tmp/x checkout b76f376^ && \
      CARGO_TARGET_DIR=/tmp/xt cargo build --release -p objdiff-cli
    python3 tools/check_objdiff_eh_prefix.py --objdiff /tmp/xt/release/objdiff-cli

(NEVER `cargo build --release` inside ../objdiff itself: its target/release path
IS the deployed fleet binary.)
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
sys.path.insert(0, str(_HERE.parent / "scripts"))
import obj_eh_boundary_patcher as P  # noqa: E402
import eh_boundary_probe as Q  # noqa: E402


def consumed_objdiff(root: Path) -> str:
    """The binary the report edge actually runs -- read from build.ninja, so a
    swap of the symlink or the rule is reflected instead of assumed."""
    ninja = root / "build.ninja"
    if ninja.exists():
        txt = ninja.read_text(errors="replace")
        m = re.search(r"^rule report\n\s+command = (\S+)", txt, re.M)
        if m:
            return m.group(1)
    return str(root / "bin" / "objdiff-cli")


def find_witness(root: Path):
    """(unit, target_obj, base_obj, symbol) for a function bounded by a planted
    `$EH` whose next real symbol is 8 bytes further on -- i.e. a function that
    WOULD over-run into a funclet prefix if nobody bounded it."""
    rep = json.loads((root / "build/45410914/report.json").read_text())
    od = json.loads((root / "objdiff.json").read_text())
    tgt = {u["name"]: u.get("target_path") for u in od["units"]}
    base = {u["name"]: u.get("base_path") for u in od["units"]}
    for unit in rep["units"]:
        name = unit["name"]
        bp, tp = base.get(name), tgt.get(name)
        if not bp or not tp:
            continue
        bpath, tpath = root / bp, root / tp
        if not bpath.exists() or not tpath.exists():
            continue
        data = bpath.read_bytes()
        if Q.strip_eh(data)[1] == 0:
            continue
        parsed = P._parse(data)
        if not parsed:
            continue
        syms = parsed[5]
        bysec = {}
        for nm, val, sec, cls, _r in syms:
            if sec > 0 and cls in P.BOUNDARY_CLASSES:
                bysec.setdefault(sec, []).append((val, nm))
        known = {f.get("name", "") for f in unit.get("functions", [])}
        for nm, val, sec, cls, _r in syms:
            if sec <= 0 or cls != 2 or nm not in known:
                continue
            after = sorted(x for x in bysec.get(sec, []) if x[0] > val)
            if not after or not after[1 - 1][1].startswith("$EH"):
                continue
            real = [x for x in after if not x[1].startswith("$EH")]
            if real and real[0][0] - after[0][0] == 8:
                return name, tpath, bpath, nm
    return None


def read_diff(objdiff: str, target: Path, base: Path, symbol: str):
    out = subprocess.run(
        [objdiff, "diff", "-1", str(target), "-2", str(base), symbol,
         "-f", "json", "-o", "-", "--include-instructions",
         "-c", "functionRelocDiffs=name_check",
         "-c", "ppc.calculatePoolRelocations=false",
         "-c", "combineDataSections=true",
         "-c", "combineTextSections=true"],
        capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit("objdiff-cli failed (rc=%d): %s"
                         % (out.returncode, out.stderr[-400:]))

    def dig(o, k):
        if isinstance(o, dict):
            if k in o:
                return o[k]
            for v in o.values():
                r = dig(v, k)
                if r is not None:
                    return r
        elif isinstance(o, list):
            for v in o:
                r = dig(v, k)
                if r is not None:
                    return r
        return None
    d = json.loads(out.stdout)
    ins = dig(d, "instructions") or []
    return len(ins), float(dig(d, "normalized_match_percent") or 0.0)


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=str(_HERE.parent))
    ap.add_argument("--objdiff", help="binary to test (default: the one build.ninja runs)")
    args = ap.parse_args()
    root = Path(args.root)
    objdiff = args.objdiff or consumed_objdiff(root)

    w = find_witness(root)
    if w is None:
        # Vacuity is an OUTCOME, not a pass.  With no witness this tool proves
        # nothing, and saying so is the only honest exit.
        print("VACUOUS: no witness function found (is the tree built, and has "
              "the EH boundary pass run?).  This guard asserts nothing.",
              file=sys.stderr)
        return 5
    unit, target, base, symbol = w
    with tempfile.TemporaryDirectory() as td:
        stripped = Path(td) / "stripped.obj"
        data, removed = Q.strip_eh(base.read_bytes())
        stripped.write_bytes(data)
        if removed == 0:
            print("VACUOUS: witness object carries no $EH symbol to remove.",
                  file=sys.stderr)
            return 5
        n_on, p_on = read_diff(objdiff, target, base, symbol)
        n_off, p_off = read_diff(objdiff, target, stripped, symbol)

    print("objdiff : %s" % objdiff)
    print("witness : %s  %s" % (unit, symbol[:64]))
    print("  pass ON  : %d instructions, %.4f%%" % (n_on, p_on))
    print("  pass OFF : %d instructions, %.4f%%" % (n_off, p_off))
    if (n_on, round(p_on, 4)) == (n_off, round(p_off, 4)):
        print("PASS: this objdiff bounds the EH funclet prefix ITSELF; "
              "scripts/obj_eh_boundary_patcher.py is redundant here.")
        return 0
    print("FAIL: removing the boundary pass CHANGES what this objdiff reports "
          "(%d -> %d instructions). This binary does NOT carry objdiff b76f376. "
          "Keep scripts/obj_eh_boundary_patcher.py wired -- retiring it costs "
          "~147 functions / ~22,844 bytes." % (n_on, n_off), file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
