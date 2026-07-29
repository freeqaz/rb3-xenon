#!/usr/bin/env python3
"""handler_drift_scan.py -- find functions whose COMPILED stack frame differs
from the RETAIL target frame, and rank them by the EH-funclet cascade that a
frame fix would flip.

Mechanism (proven by laneBF commit 05ef434a, +44 whole-binary):

  DC3 (our engine oracle) is NEWER than RB3 retail; its BEGIN_HANDLERS chains
  carry HANDLE() entries retail does not have.  Each surplus HANDLE costs one
  8-byte DataNode temp slot in the parent's frame.  Every EH cleanup funclet of
  that parent encodes the parent frame size in its FIRST instruction
  (`subi r31, r12, <FRAME>`), so one surplus handler mis-sizes every funclet of
  the function.  Deleting the surplus HANDLE lines flips the whole funclet
  population at once.

What this measures (all from machine code, no heuristics about source):
  * target frame  -- `stwu r1, -N(r1)` in the dtk-split TARGET obj (retail bytes)
  * base frame    -- same, in OUR compiled obj
  * funclet demand-- first insn `subi r31, r12, F` of a funclet COMDAT
  * cascade       -- # of sub-100 TARGET funclets in the unit demanding exactly
                     the candidate's target frame (those are the funclets our
                     wrong frame is currently breaking)

CONTROL (--control): re-run the same detector restricted to functions that
already read 100%.  A correct detector must report ~zero frame drift there.
Two detectors already lied in this lane, both in the confirming direction --
always read the control number before acting on the ranking.

Usage:
  python3 scripts/harvest/handler_drift_scan.py            # ranked candidates
  python3 scripts/harvest/handler_drift_scan.py --control  # control only
  python3 scripts/harvest/handler_drift_scan.py --json out.json
"""
import argparse
import json
import os
import re
import struct
import sys

PROJ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# --------------------------------------------------------------------------
# COFF reader
# --------------------------------------------------------------------------


def read_coff(path):
    """-> {symbol_name: (frame_or_None, funclet_demand_or_None)}"""
    try:
        d = open(path, "rb").read()
    except OSError:
        return {}
    if len(d) < 20:
        return {}
    mach, nsec, ts, psym, nsym, osz, ch = struct.unpack_from("<HHIIIHH", d, 0)
    if psym == 0 or nsym == 0:
        return {}
    strtab = psym + nsym * 18
    secs = []
    for s in range(nsec):
        off = 20 + osz + s * 40
        if off + 40 > len(d):
            return {}
        flags = struct.unpack_from("<I", d, off + 36)[0]
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", d, off + 8)
        secs.append((rawptr, rawsz, flags))

    out = {}
    i = 0
    while i < nsym:
        off = psym + i * 18
        if off + 18 > len(d):
            break
        raw = d[off : off + 8]
        if raw[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", raw, 4)[0]
            try:
                e = d.index(b"\0", strtab + so)
            except ValueError:
                break
            name = d[strtab + so : e].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        val, secn, typ, sc, naux = struct.unpack_from("<IhHBB", d, off + 8)
        i += 1 + naux
        # only real, defined, code symbols
        if secn <= 0 or secn > len(secs):
            continue
        rawptr, rawsz, flags = secs[secn - 1]
        if not (flags & 0x20):  # IMAGE_SCN_CNT_CODE
            continue
        if rawptr == 0 or rawsz == 0:
            continue
        base = rawptr + val
        end = min(rawptr + rawsz, base + 64)  # first 16 instructions
        if base >= len(d):
            continue
        body = d[base : min(end, len(d))]
        frame = None
        demand = None
        for k in range(0, len(body) - 3, 4):
            w = struct.unpack_from(">I", body, k)[0]
            op = (w >> 16) & 0xFFFF
            imm = w & 0xFFFF
            simm = imm - 0x10000 if imm & 0x8000 else imm
            if op == 0x9421 and frame is None:  # stwu r1, -N(r1)
                frame = -simm
            if k == 0 and op == 0x3BEC:  # subi r31, r12, N  (funclet entry)
                demand = -simm
        if frame is None and demand is None:
            continue
        # a symbol may already be present (aliases) -- keep the first
        out.setdefault(name, (frame, demand))
    return out


# --------------------------------------------------------------------------


def load(project_dir):
    od = json.load(open(os.path.join(project_dir, "objdiff.json")))
    rep = json.load(open(os.path.join(project_dir, "build/45410914/report.json")))
    repunits = {u["name"]: u for u in rep["units"]}
    units = []
    for u in od["units"]:
        md = u.get("metadata") or {}
        if md.get("auto_generated"):
            continue
        tp = u.get("target_path")
        bp = u.get("base_path")
        if not tp or not bp:
            continue
        ru = repunits.get(u["name"])
        if not ru or "functions" not in ru:
            continue
        units.append((u, ru))
    return units


HANDLER_RE = re.compile(r"^\s*HANDLE\w*\s*\(", re.M)


def source_handler_count(project_dir, src):
    if not src:
        return -1
    p = os.path.join(project_dir, src)
    try:
        t = open(p, errors="replace").read()
    except OSError:
        return -1
    return len(HANDLER_RE.findall(t))


def scan(project_dir, control=False):
    units = load(project_dir)
    rows = []
    ctrl_total = 0
    ctrl_drift = 0
    for u, ru in units:
        tgt = read_coff(os.path.join(project_dir, u["target_path"]))
        if not tgt:
            continue
        base = read_coff(os.path.join(project_dir, u["base_path"]))
        if not base:
            continue
        pct = {f["name"]: f.get("match_percent_normalized", 0.0) for f in ru["functions"]}
        size = {f["name"]: int(f.get("size", 0) or 0) for f in ru["functions"]}

        # sub-100 target funclets, bucketed by the parent frame they demand
        demand_sub100 = {}
        demand_all = {}
        for n, (fr, dm) in tgt.items():
            if dm is None:
                continue
            demand_all[dm] = demand_all.get(dm, 0) + 1
            if pct.get(n, 100.0) < 100.0:
                demand_sub100[dm] = demand_sub100.get(dm, 0) + 1

        src = (u.get("metadata") or {}).get("source_path")
        nhandlers = None

        for n, (tf, tdm) in tgt.items():
            if tf is None or tdm is not None:  # parents only
                continue
            bf = base.get(n, (None, None))[0]
            if bf is None:
                continue
            p = pct.get(n)
            if p is None:
                continue
            drift = bf - tf
            if control:
                if p >= 100.0:
                    ctrl_total += 1
                    if drift != 0:
                        ctrl_drift += 1
                        rows.append(
                            dict(
                                unit=u["name"],
                                func=n,
                                pct=round(p, 2),
                                our=hex(bf),
                                retail=hex(tf),
                                delta=drift,
                            )
                        )
                continue
            if p >= 100.0 or drift == 0:
                continue
            if drift % 8 != 0:
                continue
            if nhandlers is None:
                nhandlers = source_handler_count(project_dir, src)
            rows.append(
                dict(
                    unit=u["name"],
                    func=n,
                    src=src,
                    pct=round(p, 2),
                    size=size.get(n, 0),
                    our_frame=hex(bf),
                    retail_frame=hex(tf),
                    delta=drift,
                    slots=drift // 8,
                    cascade=demand_sub100.get(tf, 0),
                    funclets_at_frame=demand_all.get(tf, 0),
                    unit_handlers=nhandlers,
                )
            )
    if control:
        return rows, ctrl_total, ctrl_drift
    rows.sort(key=lambda r: (-r["cascade"], -abs(r["delta"])))
    return rows, 0, 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project-dir", default=PROJ)
    ap.add_argument("--control", action="store_true")
    ap.add_argument("--json")
    ap.add_argument("--top", type=int, default=40)
    a = ap.parse_args()

    if a.control:
        rows, tot, drift = scan(a.project_dir, control=True)
        print(f"CONTROL: already-100% functions scanned: {tot}")
        print(f"CONTROL: of those, frame drift reported: {drift}  "
              f"({(100.0*drift/tot if tot else 0):.4f}%)")
        for r in rows[:20]:
            print(" ", r)
        if tot and drift / tot > 0.005:
            print("!! CONTROL FAILED -- detector reports drift on known-good "
                  "functions; do NOT act on the ranking.")
            return 1
        print("CONTROL PASSED.")
        return 0

    rows, _, _ = scan(a.project_dir)
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)
    print(f"{'casc':>4} {'fnc@f':>5} {'delta':>6} {'slot':>4} "
          f"{'ours':>6} {'retail':>6} {'pct':>6}  unit / function")
    for r in rows[: a.top]:
        print(f"{r['cascade']:>4} {r['funclets_at_frame']:>5} {r['delta']:>6} "
              f"{r['slots']:>4} {r['our_frame']:>6} {r['retail_frame']:>6} "
              f"{r['pct']:>6}  {r['unit']} :: {r['func'][:80]}")
    print(f"\ntotal drift candidates: {len(rows)}   "
          f"total predicted cascade: {sum(r['cascade'] for r in rows)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
