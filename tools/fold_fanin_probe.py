#!/usr/bin/env python3
"""fold_fanin_probe.py — prove a fold by HETEROGENEOUS RETAIL FAN-IN, not by body shape.

Lane FOLDPROVE-1. The vacuity floor in icf_alias_build rejects a candidate whose BODY
carries too little unmasked information. That is a property of the body comparator, so
no amount of re-reading the body can answer it -- the honest way past it is an
INDEPENDENT channel that never looks at the body at all.

This is that channel (the one that survived ALIASAUDIT-2's re-audit): enumerate every
retail CALL SITE that reaches the candidate address, and identify the CALLER via
scripts/target_symbol_map.json. If one retail address is reached from call sites whose
contexts demand DIFFERENT types/classes, it cannot be a single source-level function,
so the linker must have folded several COMDATs onto it. Internal inconsistency needs no
fold model to be decisive.

Also scans .data/.rdata for the address appearing as a POINTER WORD (a vtable slot or
function table), which for a VIRTUAL candidate is the strongest form: present in class
X's vtable AND directly called from a non-X context.

Usage:  python3 tools/fold_fanin_probe.py 0x82545c88 [0x823ea598 ...]
"""
import argparse
import bisect
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import xex_string_at  # noqa: E402

EXE = os.path.join(ROOT, "orig", "45410914", "band.exe")
MAP = os.path.join(ROOT, "scripts", "target_symbol_map.json")


def load_map():
    raw = json.load(open(MAP))
    a2n = {}
    for k, v in raw.items():
        try:
            a = int(k, 16) if isinstance(k, str) else int(k)
        except ValueError:
            continue
        name = v.get("name") if isinstance(v, dict) else v
        if name:
            a2n[a] = name
    return a2n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addrs", nargs="+")
    args = ap.parse_args()
    targets = [int(a, 16) for a in args.addrs]

    data, base, secs = xex_string_at.load_sections(EXE)
    a2n = load_map()
    starts = sorted(a2n)

    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        if i < 0:
            return None, None
        return a2n[starts[i]], starts[i]

    text = next(s for s in secs if s[0] == ".text")
    _n, tv, tvs, trp, trs = text
    tstart, tsize = base + tv, max(tvs, trs)
    tdata = data[trp:trp + tsize]

    print("scanning .text 0x%08x..0x%08x (%d B) for bl/b to %d target(s)"
          % (tstart, tstart + tsize, tsize, len(targets)))
    tset = set(targets)
    hits = {t: [] for t in targets}

    for off in range(0, len(tdata) - 3, 4):
        w = struct.unpack_from(">I", tdata, off)[0]
        op = w >> 26
        if op != 18:                       # I-form: b / bl / ba / bla
            continue
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        aa, lk = (w >> 1) & 1, w & 1
        pc = tstart + off
        tgt = li if aa else pc + li
        if tgt in tset:
            hits[tgt].append((pc, "bl" if lk else "b"))

    # pointer-word occurrences (vtable slots / function tables) in data sections
    ptr = {t: [] for t in targets}
    for nm, v, vs, rp, rs in secs:
        if nm == ".text":
            continue
        blob = data[rp:rp + max(vs, rs)]
        vabase = base + v
        for off in range(0, len(blob) - 3, 4):
            w = struct.unpack_from(">I", blob, off)[0]
            if w in tset:
                ptr[w].append((nm, vabase + off))

    for t in targets:
        nm, st = owner(t)
        print()
        print("=" * 92)
        print("TARGET 0x%08x   map name: %s" % (t, nm if st == t else "(not a map start; inside %s)" % nm))
        print("=" * 92)
        cs = hits[t]
        print("retail call sites: %d" % len(cs))
        callers = {}
        for pc, kind in cs:
            cn, cst = owner(pc)
            callers.setdefault(cn, []).append((pc, kind))
        for cn, lst in sorted(callers.items(), key=lambda kv: -len(kv[1])):
            print("   %-4s x%-3d  from %s" % (lst[0][1], len(lst), cn))
        pw = ptr[t]
        print("pointer-word occurrences (vtable/function-table slots): %d" % len(pw))
        for nm2, va in pw[:24]:
            on, ost = owner(va)
            print("   %-8s @0x%08x" % (nm2, va))
    return 0


if __name__ == "__main__":
    sys.exit(main())
