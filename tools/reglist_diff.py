#!/usr/bin/env python3
"""Diff RB3 retail's factory-registration lists against ours, IN STRING SPACE.

Lane REGORDER-1 (2026-08-14).  The definitive comparator for the class lane
WRONGCALL-2 discovered (``Synth360::Init`` / ``Rnd::PreInit`` order defects).

WHY STRING SPACE.  Retail's side is a list of ``.rdata`` literals -- the argument
to ``OBJ_CLASSNAME`` -- not class names: ``RndFur`` registers as "Fur",
``RndTransformable`` as "Trans".  Comparing retail literals against our CLASS
names therefore mis-reports every ``Rnd*``/``Dx*``/``Ng*`` row.  It also makes
whole-binary membership queries silently vacuous: asking "is RndRibbon
registered anywhere" returns NO for a class registered as "Ribbon".  Both sides
are mapped into literal space here.

TWO EARLIER VERSIONS OF THIS COMPARISON WERE WRONG, BOTH IN THE SAME DIRECTION
(they manufactured work), and both are guarded against here:

  1. Keying retail's slots on ``?StaticClassName@X@@`` NAMES dropped every slot
     the symbol map has not named, reporting surplus registrations on our side
     that do not exist.  Slots are anchored on ``RegisterFactory`` CALL SITES.
  2. Treating an unresolved retail slot as a WILDCARD hid a real defect --
     ``Rnd::PreInit`` slot 0 is ``DOFProc`` in retail and ``RndDrawable`` in
     ours, and a wildcard scored that pair EQUAL.  Unresolved slots are reported
     as UNRESOLVED and never matched.
"""
from __future__ import annotations

import argparse
import collections
import difflib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.reglist_rdata_adjudicate import RegListReader  # noqa: E402
from tools.reglist_whole_binary import find_registrations  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAC = re.compile(r"\bOBJ_CLASSNAME\s*\(\s*([^)\s]+)\s*\)")
CLS = re.compile(r"\b(?:class|struct)\s+([A-Za-z_]\w*)\b")


def class_strings(src_root):
    """class name -> OBJ_CLASSNAME literal, scanned from OUR source only.

    ⚠ Uses ``classname_forwarder_audit._scan_file``, which tracks class/namespace
    scope with a real brace matcher.  A "nearest preceding ``class X``" regex was
    tried first and is NOT adequate: it mapped ``RndAmbientOcclusion`` to "Trans"
    and left most ``Rnd*`` classes unmapped, which then rendered as ~20 phantom
    membership defects in ``Rnd::PreInit`` alone.
    """
    from tools.classname_forwarder_audit import _scan_file
    tab = {}
    for dp, _d, fns in os.walk(src_root):
        for fn in fns:
            if fn.endswith((".h", ".cpp", ".inl")):
                for q, s, _p in _scan_file(os.path.join(dp, fn)):
                    tab.setdefault(q.split("::")[-1], s)
    return tab


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", default=ROOT)
    ap.add_argument("--seed", default="0x8236CE30")
    a = ap.parse_args()
    sys.path.insert(0, os.path.join(a.worktree, "tools"))
    from coff_bodies_ext import function_bodies_ext            # noqa
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from rg_anchor_lib import our_reglists                     # noqa

    rd = RegListReader()
    _r, d = rd.read(int(a.seed, 16))
    callers, _u = find_registrations(rd, d["register_factory"])
    retail = {fn: v for fn, v in callers.items() if len(v) >= 2}

    strtab = class_strings(os.path.join(a.worktree, "src"))
    ours = our_reglists(a.worktree)          # sym -> [class names]
    ours_str = {s: [strtab.get(c, c) for c in v] for s, v in ours.items()}

    print("retail multi-registration lists: %d" % len(retail))
    print("our   multi-registration lists : %d" % len(ours_str))
    # BIJECTION, not greedy-per-row.  Matching each retail list to its best
    # overlap independently paired retail's Synth360::Init (11 FxSend* slots)
    # against our Synth::Init, because Synth::Init CONTAINS those same classes --
    # so the true counterpart was never even considered and the row rendered as a
    # 5-registration membership defect that does not exist.
    pairs = sorted(((len(set(r) & set(o)), fn, s)
                    for fn, r in retail.items() for s, o in ours_str.items()),
                   key=lambda t: -t[0])
    assign, usedf, useds = {}, set(), set()
    for ov, fn, s in pairs:
        if ov >= 2 and fn not in usedf and s not in useds:
            assign[fn] = (s, ov)
            usedf.add(fn)
            useds.add(s)

    summary = collections.Counter()
    for fn, rseq in sorted(retail.items(), key=lambda kv: -len(kv[1])):
        got = assign.get(fn)
        best, bs = ((got[0], ours_str[got[0]]), got[1]) if got else (None, -1)
        if not best or bs < 2:
            summary["NO_COUNTERPART"] += 1
            print("\n%#010x  %2d slots  -> NO COUNTERPART IN OUR SOURCE" % (fn, len(rseq)))
            print("     retail:", ", ".join(rseq))
            continue
        s, oseq = best
        v = "IDENTICAL" if rseq == oseq else ("ORDER_ONLY" if sorted(rseq) == sorted(oseq)
                                              else "MEMBERSHIP")
        summary[v] += 1
        print("\n%#010x  retail=%d  ours=%d (%s)  %s  [overlap %d]"
              % (fn, len(rseq), len(oseq), s, v, bs))
        if v != "IDENTICAL":
            for line in difflib.unified_diff(rseq, oseq, "RETAIL", "OURS", lineterm="", n=1):
                print("      " + line)
    print("\nSUMMARY:", dict(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
