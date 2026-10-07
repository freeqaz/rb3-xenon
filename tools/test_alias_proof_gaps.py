#!/usr/bin/env python3
"""Planted bad memberships for the two alias-proof gaps W16-TT found (lane W16-TV).

Each gap gets a membership that was really installed as PROVEN and is really
wrong, replayed in the map state that let it through.  The test requires:

  * with the pre-W16-TV policies, the planted membership reads PROVEN
    (otherwise the plant tests nothing -- a control nobody has seen pass the
    old rule cannot show the new rule closed anything);
  * with the shipped policies it does NOT read PROVEN;
  * a sound membership still reads PROVEN under the shipped policies
    (otherwise the fix is "refuse everything").

GAP 1, SAME-NAME SLOT (group 771, W16-TT §2).  On 2026-09-15 the map named the
  depth-1 callee 0x82773E70 with OUR spelling
  `__destroy_range_aux<reverse_iterator<vector<short>*>>`, but that body
  destroys 12-byte elements and ours 2-byte ones.  Flat T1 and the chase both
  accepted the same-name slot without reading it.  Plant: give the retail body
  at 0x82773E70 our spelling again, in memory.
GAP 2, UNNAMED RETAIL CALLEE (group 1201, W16-TT §3.1).  W16-CU restored
  `?Issue@D3DQuery@@QAAJK@Z` into `?ThreadStart@Splash@@KAKPAX@Z` on 2026-09-15
  while retail's callee 0x827427E8 was unnamed; flat T1 tolerated the fn_ slot
  because our callee was not map-resident.  The map named 0x827427E8
  `?UpdateThread@Splash@@IAAXXZ` on 09-30.  Plant: unname it again, in memory.
GAP 3 (the callee-name drift check) is planted in
  tools/alias_callee_name_drift.py --selftest, which imports the 771 plant here.

    python3 tools/test_alias_proof_gaps.py        # needs a BUILT tree

Read-only.  Policies are module globals, restored after every leg.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

G771_ADDR = "0x82775950"
G771_SURVIVOR = (
    "??1?$vector@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V"
    "?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V?$StlNodeAlloc"
    "@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@"
    "?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ")
G771_FOLDED = (
    "??1?$vector@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@V?$StlNodeAlloc"
    "@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ")
OURS_DRA = (
    "??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@FV?$StlNodeAlloc@F"
    "@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXV?$reverse_"
    "iterator@PAV?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@@0@0"
    "ABU__false_type@0@@Z")
RANGED_DRA_PREFIX = ("??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@V?$RangedData"
                     "@I@")

G1201_SURVIVOR = "?ThreadStart@Splash@@KAKPAX@Z"
G1201_FOLDED = "?Issue@D3DQuery@@QAAJK@Z"
G1201_CALLEE = "?UpdateThread@Splash@@IAAXXZ"
G1201_CALLEE_VA = 0x827427E8

# W16-JE's SetObjConcrete<UILabel>: flat-T1 PROVEN, retail_bodytwins 1, and its
# same-name ?Release@Object@Hmx slot differs in bytes because OUR port of
# Release is imperfect -- the case a same-name read must NOT refute.
SOUND_SURVIVOR_GROUP = 0
SOUND_FOLDED = "?SetObjConcrete@?$ObjPtr@VUILabel@@@@QAAXPAVUILabel@@@Z"


def _rename(tgt, old, new):
    """A target-obj view in which `old` is spelled `new` everywhere."""
    out = dict(tgt)
    if old in out:
        out[new] = out.pop(old)
    for n, (mb, rl, sz) in list(out.items()):
        if any(x[1] == old for x in rl):
            out[n] = (mb, [(o, new if c == old else c, t) for (o, c, t) in rl], sz)
    return out


def plant_771(tgt, mapped):
    """The 2026-09-15 map state: 0x82773E70 carries OUR spelling."""
    cur = [n for n in tgt if n.startswith(RANGED_DRA_PREFIX)]
    assert len(cur) == 1, cur
    t = _rename(tgt, cur[0], OURS_DRA)
    m = set(mapped)
    m.discard(cur[0])
    m.add(OURS_DRA)
    return t, m


def plant_1201(tgt, mapped):
    """The 2026-09-15 map state: 0x827427E8 is unnamed."""
    assert G1201_CALLEE in tgt
    t = _rename(tgt, G1201_CALLEE, "fn_%08X" % G1201_CALLEE_VA)
    m = set(mapped)
    m.discard(G1201_CALLEE)
    return t, m


OLD = {"SAMENAME_POLICY": "trust", "FLAT_CONFIRM": False, "UNNAMED_CALLEE_POLICY": "tolerate"}
NEW = {"SAMENAME_POLICY": "check", "FLAT_CONFIRM": True, "UNNAMED_CALLEE_POLICY": "refuse"}


def under(policy, fn):
    import icf_alias_build as b
    import icf_pair_adjudicate as m
    saved = (m.SAMENAME_POLICY, m.FLAT_CONFIRM, b.UNNAMED_CALLEE_POLICY)
    m.SAMENAME_POLICY = policy["SAMENAME_POLICY"]
    m.FLAT_CONFIRM = policy["FLAT_CONFIRM"]
    b.UNNAMED_CALLEE_POLICY = policy["UNNAMED_CALLEE_POLICY"]
    try:
        return fn()
    finally:
        m.SAMENAME_POLICY, m.FLAT_CONFIRM, b.UNNAMED_CALLEE_POLICY = saved


def main() -> int:
    import json
    import icf_pair_adjudicate as m
    tgt, ours = m.load_sides()
    mapped = m.load_mapped()
    if sum(1 for n in tgt if n.startswith("?")) < 1000:
        print("REFUSING: target objs look PRE-RENAMER -- build the tree first")
        return 2
    groups = json.loads((ROOT / "scripts/symbol_aliases.json").read_text())["groups"]
    ok = True

    def leg(label, T, M, s, f, pol, want_proven):
        nonlocal ok
        v, why, _tr = under(pol, lambda: m.membership_verdict(T, ours, s, f, M, {}))
        flat, d = under(pol, lambda: m.adjudicate(T, ours, s, f, M, verbose=False))
        good = (v == "PROVEN") == want_proven
        ok &= good
        print("  [%s] %-58s flat %-11s verdict %-11s (want %s)"
              % ("PASS" if good else "FAIL", label, flat, v,
                 "PROVEN" if want_proven else "not PROVEN"))
        if not good or (not want_proven and v != "PROVEN"):
            print("         %s" % (d.get("why") if flat != "PROVEN" else why)[:220])
        return v

    for nm, s, f in (("771", G771_SURVIVOR, G771_FOLDED), ("1201", G1201_SURVIVOR, G1201_FOLDED)):
        if s not in tgt or f not in ours:
            print("REFUSING: plant %s is VACUOUS (survivor in retail: %s, ours compiled: %s)"
                  % (nm, s in tgt, f in ours))
            return 2

    print("GAP 1 -- same-name slot (group 771, 2026-09-15 map state)")
    T, M = plant_771(tgt, mapped)
    leg("planted, pre-W16-TV policies", T, M, G771_SURVIVOR, G771_FOLDED, OLD, True)
    leg("planted, same-name trust but flat confirmed",
        T, M, G771_SURVIVOR, G771_FOLDED, dict(NEW, SAMENAME_POLICY="trust"), True)
    leg("planted, shipped policies", T, M, G771_SURVIVOR, G771_FOLDED, NEW, False)
    leg("today's map, shipped policies (W16-TT: refuted)",
        tgt, mapped, G771_SURVIVOR, G771_FOLDED, NEW, False)

    print("GAP 2 -- unnamed retail callee (group 1201, 2026-09-15 map state)")
    T, M = plant_1201(tgt, mapped)
    check_ours_unmapped = all(c not in mapped for (_o, c, _t) in ours[G1201_FOLDED][1]
                              if not m.placeholder(c) and c in ours)
    print("    our callee(s) map-resident: %s (CD-9 alone cannot catch it)"
          % (not check_ours_unmapped))
    leg("planted, pre-W16-TV policies", T, M, G1201_SURVIVOR, G1201_FOLDED, OLD, True)
    leg("planted, only the unnamed-callee refusal",
        T, M, G1201_SURVIVOR, G1201_FOLDED, dict(OLD, UNNAMED_CALLEE_POLICY="refuse"), False)
    leg("planted, shipped policies", T, M, G1201_SURVIVOR, G1201_FOLDED, NEW, False)

    print("SOUND MEMBERSHIP -- must stay PROVEN (W16-JE SetObjConcrete<UILabel>)")
    leg("today's map, shipped policies", tgt, mapped,
        groups[SOUND_SURVIVOR_GROUP]["survivor"], SOUND_FOLDED, NEW, True)
    print("same-name slots read: %s" % dict(m.SAMENAME_TALLY))
    print("TEST", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
