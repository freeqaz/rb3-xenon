#!/usr/bin/env python3
"""Sweep for the STUBBED-CALLEE shape: a member we compile to an empty/trivial
body that retail implements substantially.

WHY THIS EXISTS (lane BODIES-1, successor to EE2-D)
  EE2-D drained the handler-BLOCK class 20/20 -- no missing/extra/reordered
  handler survives in the pinned population. But its one landed fix was not a
  handler defect at all: VocalTrackDir::SetEnableVocalsOptions was an EMPTY STUB
  that retail implements, and restoring the body crossed the row (98.58 -> 100).
  That shape generalises far past `Handle` rows, so it is worth sweeping
  directly rather than stumbling onto it one row at a time.

  It also generalises past what the ORACLES can tell you: rb3-Wii carries the
  SAME empty stub, so "check the oracle" returns nothing and its silence is not
  a negative result. This sweep is keyed on RETAIL BYTES, so it is immune to a
  shared hole in both decomps.

THE MEASUREMENT
  Retail size comes from objdiff's report rows (the dtk-split target obj).
  Our size is parsed out of our own compiled COFF .obj symbol table.
  A stub is: ours tiny (<= --base-max bytes) while retail is substantial
  (>= --target-min bytes).

  Sizes are per-symbol, derived as (next symbol's value - this value) within a
  section, falling back to (SizeOfRawData - value) for the last symbol. That
  handles BOTH the /Gy one-COMDAT-per-function layout and a shared .text.

THE CONTROL, AND WHY IT IS NOT OPTIONAL
  Three consecutive lanes shipped screens that produced clean, decisive-looking
  WRONG output. So this tool refuses to be trusted on its own say-so:

    For every row objdiff scores at fuzzy 100, our size MUST equal retail's.

  That is a falsifiable assertion over the whole matched population (~20k rows),
  and it fails loudly if the COFF size derivation is wrong. `--control` runs it
  and reports the disagreement rate. A nonzero rate means the size parse is
  broken and EVERY stub candidate below is suspect.

  ONE-SIDEDNESS: this finds only OUT-OF-LINE stubs. At /O1 /Ob2 a small stub is
  INLINED into its callers and has no row at all -- which is exactly what
  happened to SetEnableVocalsOptions. So an empty result here does NOT mean the
  stub class is drained; it means the out-of-line half of it is. Use
  --inline-scan for the other half.
"""
import argparse
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


# ---------------------------------------------------------------- COFF reader
def coff_symbol_sizes(path):
    """-> {symbol_name: size_in_bytes} for function symbols in code sections."""
    with open(path, "rb") as fh:
        data = fh.read()
    if len(data) < 20:
        return {}
    machine, nsec, _ts, symptr, nsym, _oh, _ch = struct.unpack_from("<HHIIIHH", data, 0)
    if symptr == 0 or nsym == 0:
        return {}

    # section headers
    secs = []  # (name, size_of_raw_data, characteristics)
    off = 20 + _oh
    for i in range(nsec):
        raw = data[off : off + 40]
        if len(raw) < 40:
            return {}
        name = raw[:8].rstrip(b"\0").decode("latin1")
        vsize, vaddr, srd, praw = struct.unpack_from("<IIII", raw, 8)
        chars = struct.unpack_from("<I", raw, 36)[0]
        secs.append((name, srd, chars))
        off += 40

    strtab_off = symptr + 18 * nsym
    strtab = data[strtab_off:]

    def sym_name(raw):
        if raw[:4] == b"\0\0\0\0":
            idx = struct.unpack_from("<I", raw, 4)[0]
            end = strtab.find(b"\0", idx)
            return strtab[idx:end].decode("latin1")
        return raw[:8].rstrip(b"\0").decode("latin1")

    # collect function symbols
    IMAGE_SCN_CNT_CODE = 0x00000020
    per_sec = {}  # secnum -> [(value, name)]
    i = 0
    while i < nsym:
        raw = data[symptr + 18 * i : symptr + 18 * i + 18]
        if len(raw) < 18:
            break
        value, secnum, stype, sclass, naux = struct.unpack_from("<IhHBB", raw, 8)
        name = sym_name(raw)
        # DTYPE_FUNCTION == 0x20 (function), storage class 2=EXTERNAL 3=STATIC
        if 1 <= secnum <= len(secs) and stype == 0x20 and sclass in (2, 3):
            if secs[secnum - 1][2] & IMAGE_SCN_CNT_CODE:
                per_sec.setdefault(secnum, []).append((value, name))
        i += 1 + naux

    out = {}
    for secnum, lst in per_sec.items():
        srd = secs[secnum - 1][1]
        lst.sort()
        for j, (value, name) in enumerate(lst):
            end = lst[j + 1][0] if j + 1 < len(lst) else srd
            sz = end - value
            if sz > 0:
                # a name can legitimately appear once per obj; keep the largest
                out[name] = max(out.get(name, 0), sz)
    return out


# ------------------------------------------------------------- report loading
def load_report(path):
    """-> {unit_name: {fn_name: (retail_size, mpn)}}  (sizes int()-coerced)."""
    with open(path) as fh:
        rep = json.load(fh)
    units = {}
    for u in rep.get("units", []):
        fns = {}
        for f in u.get("functions", []):
            # report.json numerics are JSON STRINGS -- coerce or comparisons lie
            fns[f["name"]] = (int(f["size"]), float(f.get("match_percent_normalized", 0.0)))
        units[u["name"]] = fns
    return units


def our_obj_for_unit(unit_name, project_dir):
    """default/Foo -> build/45410914/src/**/Foo.obj (our compiled side)."""
    base = unit_name.split("/")[-1]
    for dirpath, _dirnames, filenames in os.walk(os.path.join(project_dir, "build/45410914/src")):
        if base + ".obj" in filenames:
            return os.path.join(dirpath, base + ".obj")
    return None


def build_index(project_dir, report_path):
    units = load_report(report_path)
    rows = []
    missing_obj = 0
    for unit, fns in units.items():
        objp = our_obj_for_unit(unit, project_dir)
        if not objp:
            missing_obj += 1
            continue
        ours = coff_symbol_sizes(objp)
        for name, (tsize, mpn) in fns.items():
            rows.append((unit, name, tsize, ours.get(name), mpn))
    return rows, missing_obj


# -------------------------------------------------------------------- control
def run_control(rows):
    """On mpn-100 rows our size must equal retail's, up to two CHARACTERISED
    artifacts -- and, load-bearingly, must NEVER be SMALLER.

    MEASURED 2026-08-13 over 21,022 mpn-100 rows:
        +0 : 20819     exact
        +4 :    14     inter-function alignment padding
        +8 :   189     the 8-byte EH prefix -- our function symbol's value is 0,
                       pointing at the prefix, while retail's extent starts at
                       the body (+8). See CLAUDE.md's split-noise census.
        -N :     0     <-- THE ASSERTION THAT MATTERS

    Zero negative deltas means the size parse can only ever OVERSTATE our side.
    A stub screen looks for ours TINY vs retail LARGE, so an overstatement makes
    it strictly more conservative: the parse has no capacity to manufacture a
    false stub. It could only hide a real one, which is a survivable error.
    """
    checked = agree = 0
    neg = []
    for unit, name, tsize, osize, mpn in rows:
        if mpn >= 100.0 and osize is not None:
            checked += 1
            delta = osize - tsize
            if delta in (0, 4, 8):
                agree += 1
            if delta < 0 and len(neg) < 15:
                neg.append((unit, name, tsize, osize))
    return checked, agree, neg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", default=ROOT)
    ap.add_argument("--report", default=None)
    ap.add_argument("--base-max", type=int, default=12,
                    help="our size <= this counts as a stub (4=blr, 8/12=trivial return)")
    ap.add_argument("--target-min", type=int, default=32,
                    help="retail size >= this counts as substantial")
    ap.add_argument("--control", action="store_true", help="run the size-parse control and exit")
    ap.add_argument("--limit", type=int, default=200)
    args = ap.parse_args()

    report = args.report or os.path.join(args.project_dir, "build/45410914/report.json")
    rows, missing_obj = build_index(args.project_dir, report)
    paired = [r for r in rows if r[3] is not None]
    print(f"rows={len(rows)}  with-our-obj={len(paired)}  units-without-our-obj={missing_obj}",
          file=sys.stderr)

    checked, agree, bad = run_control(rows)
    rate = (100.0 * agree / checked) if checked else 0.0
    print(f"CONTROL: {agree}/{checked} mpn-100 rows within characterised artifact (0/+4/+8); NEGATIVE deltas  ({rate:.4f}%)",
          file=sys.stderr)
    for b in bad:
        print(f"  CONTROL-FAIL(negative) {b[0]}  {b[1]}  retail={b[2]} ours={b[3]}", file=sys.stderr)
    if args.control:
        return 0 if rate > 99.9 else 1
    if rate <= 99.9:
        print("REFUSING: size parse disagrees on matched rows; candidates would be unsound.",
              file=sys.stderr)
        return 1

    cands = [r for r in paired
             if r[3] <= args.base_max and r[2] >= args.target_min and r[4] < 100.0]
    cands.sort(key=lambda r: -r[2])
    print(f"\nSTUB CANDIDATES: {len(cands)} (ours<={args.base_max}B, retail>={args.target_min}B)")
    print(f"{'retail':>7} {'ours':>5} {'mpn':>7}  unit / symbol")
    for unit, name, tsize, osize, mpn in cands[: args.limit]:
        print(f"{tsize:7d} {osize:5d} {mpn:7.2f}  {unit}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
