#!/usr/bin/env python3
"""Whole-binary census of RB3 retail's factory registrations.

Lane REGORDER-1 (2026-08-14).

``tools/reglist_rdata_adjudicate`` answers "what does retail register in THIS
function".  This answers the two questions that one cannot:

  1. WHERE are all the registration lists?  The obj-side census can only see
     functions inside a PINNED unit, so its population is bounded by our splits
     coverage, not by the binary.  Scanning every ``bl`` to ``RegisterFactory``
     in ``.text`` is bounded by the binary itself.
  2. Does retail register class X ANYWHERE?  Needed before deleting one of our
     surplus registrations: "absent from this list" and "absent from the game"
     are very different, and only the second licenses a plain deletion.

RegisterFactory is located by REPETITION from a seed list (no symbol name, no
map).  Every class is reported as the ``.rdata`` literal its StaticClassName
builds -- retail bytes throughout.
"""
from __future__ import annotations

import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.reglist_rdata_adjudicate import RegListReader, bl_target  # noqa: E402


def find_registrations(rd, rf_va: int):
    """-> {caller_fn_va: [class strings]} over every .text call to `rf_va`."""
    R = rd.R
    ext = R.extents                       # fn_va -> size, from .pdata
    starts = sorted(ext)
    callers = collections.defaultdict(list)
    unresolved = collections.Counter()
    for fn in starts:
        n = ext[fn]
        if n is None or n <= 0 or n > 0x8000:
            continue
        words = []
        for i in range(0, n, 4):
            w = R.u32(fn + i)
            if w is None:
                break
            words.append((fn + i, w))
        bls = [(a, t) for a, w in words if (t := bl_target(a, w)) is not None]
        if not any(t == rf_va for _a, t in bls):
            continue
        for i, (a, t) in enumerate(bls):
            if t != rf_va:
                continue
            prev = next((bls[j][1] for j in range(i - 1, -1, -1) if bls[j][1] != rf_va), None)
            s = rd.static_classname_string(prev) if prev else None
            if s is None:
                unresolved[fn] += 1
            callers[fn].append(s)
    return callers, unresolved


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", default="0x8236CE30",
                    help="a known registration list; RegisterFactory is DERIVED from it")
    ap.add_argument("--rf", help="override the derived RegisterFactory VA (discouraged)")
    ap.add_argument("--find", nargs="*", default=[], help="report where these classes register")
    a = ap.parse_args()
    rd = RegListReader()

    # ⛔ DERIVE, NEVER TRANSCRIBE.  The first version of this tool took the VA as a
    # hand-typed hex default -- and the decimal 2188758400 was transcribed as
    # 0x82756B40 instead of 0x8275CD80.  The scan then found ZERO call sites and
    # reported a confident "NOT REGISTERED ANYWHERE IN RETAIL" for every class
    # queried, INCLUDING four proven-registered positive controls.  A wrong
    # address makes this tool answer "absent" to everything, which is exactly the
    # answer a lane deleting surplus registrations wants to hear.
    _regs, d = rd.read(int(a.seed, 16))
    rf = d.get("register_factory")
    if rf is None:
        raise SystemExit("seed %s is not a registration list: %s" % (a.seed, d))
    if a.rf:
        rf = int(a.rf, 16)
    print("RegisterFactory derived from seed %s: %#x (%d calls there)"
          % (a.seed, rf, d["register_factory_calls"]))
    callers, unres = find_registrations(rd, rf)
    # POSITIVE CONTROL: the seed's own slots must come back, or the scan is vacuous.
    if len(callers.get(int(a.seed, 16), [])) < 2:
        raise SystemExit("VACUOUS: the seed's own registrations were not recovered")

    tot = sum(len(v) for v in callers.values())
    res = sum(1 for v in callers.values() for s in v if s)
    print("RegisterFactory VA           : %#x" % rf)
    print("call sites (registrations)   : %d" % tot)
    print("distinct calling functions   : %d" % len(callers))
    print("slots resolved to a class    : %d (%.1f%%)" % (res, 100.0 * res / max(tot, 1)))
    print("functions with >=2 slots     : %d" % sum(1 for v in callers.values() if len(v) >= 2))
    print("functions with  1 slot       : %d" % sum(1 for v in callers.values() if len(v) == 1))

    classes = collections.Counter(s for v in callers.values() for s in v if s)
    print("distinct classes registered  : %d" % len(classes))
    dup = {c: n for c, n in classes.items() if n > 1}
    print("classes registered >once     : %d %s" % (len(dup), sorted(dup)[:12]))

    print("\n%-12s %-6s %s" % ("caller", "slots", "classes"))
    for fn, v in sorted(callers.items(), key=lambda kv: -len(kv[1])):
        if len(v) >= 2:
            print("%#010x %-6d %s" % (fn, len(v), ", ".join(s or "<?>" for s in v)))

    if a.find:
        print("\n--- membership queries (whole binary) ---")
        for c in a.find:
            hits = [fn for fn, v in callers.items() if c in v]
            print("%-32s %s" % (c, ("REGISTERED at " + ", ".join("%#x" % h for h in hits))
                                if hits else "NOT REGISTERED ANYWHERE IN RETAIL"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
