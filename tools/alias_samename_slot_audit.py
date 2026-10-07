#!/usr/bin/env python3
"""Audit every installed alias membership for proofs that rest on a SAME-NAME slot.

Lane W16-TT (2026-10-07), docs/decomp/W16TT_FOLD_LEADS_ON_RETAIL_2026-10-07.md.

THE GAP.  Both T1 comparators accept a relocation slot whose two target names are
EQUAL without looking at the callee: `icf_alias_build.relocs_agree` does
`if rn == on: continue`, and `icf_pair_adjudicate.chase` returns True for
`survivor == our_name and depth > 0` ("name equality IS the evidence").  The retail
name comes from scripts/target_symbol_map.json, so the slot is only as good as the
map's identification of the callee.  When the map names a callee with OUR spelling
on a body that is not ours, the one slot that discriminates the fold reads as
agreement.  Group 771 was restored this way on 2026-09-15: the map named
0x82773E70 `__destroy_range_aux<reverse_iterator<vector<short>*>>`, but that body
destroys 12-byte elements and ours destroys 2-byte ones (two levels down).

WHAT THIS RUNS, per membership (survivor S, folded F):
  base   -- the shipped chase, unchanged (flat T1 first, as the tool does).  A
            REFUTED here is a STALE proof: the evidence changed under it.
  strict -- the same chase, except a same-name slot at depth > 0 whose callee is
            present and non-vacuous on BOTH sides is compared on bytes (the
            depth-0 comparison, recursing through its own slots).  Slots that
            cannot be checked (callee absent or vacuous on either side) keep the
            shipped acceptance, so strict only adds failures that byte evidence
            supports.

    python3 tools/alias_samename_slot_audit.py --out ~/tmp/samename_audit.json
    python3 tools/alias_samename_slot_audit.py --selftest   # 771 must fail strict

Needs a BUILT tree (renamed target objs).  Read-only.
"""
import argparse, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import icf_pair_adjudicate as m  # noqa: E402

ORIG = m.chase
STATE = {"strict": False, "samename_checked": 0, "samename_fail": []}


def chase_wrapper(tgt, ours, s, o, mapped, depth=0, stack=None, memo=None,
                  out=None, maxdepth=12, ctx=None):
    if STATE["strict"] and s == o and depth > 0:
        rt, ob = tgt.get(s), ours.get(o)
        if rt is None or ob is None or m.vacuous(rt) or m.vacuous(ob):
            return True                      # nothing to compare: shipped behaviour
        stack = stack if stack is not None else []
        memo = memo if memo is not None else {}
        key = ("SAMENAME", s)
        if key in memo:
            return memo[key]
        if key in stack:
            return True                      # coinductive, as the shipped chase
        stack.append(key)
        STATE["samename_checked"] += 1
        sub = []
        ok = ORIG(tgt, ours, s, o, mapped, depth=0, stack=stack, memo=memo,
                  out=sub, maxdepth=maxdepth, ctx=None)
        stack.pop()
        if not ok:
            # A byte difference alone does not say the MAP is wrong: our port of
            # the callee may simply not match yet (measured: ?Release@Object@Hmx
            # fails here for that reason).  Positive evidence is required: our
            # callee must be PROVEN, by the shipped chase, at a DIFFERENT retail
            # body.  Without it the slot keeps the shipped acceptance.
            STATE["strict"] = False
            try:
                elsewhere = m.locate_retail(tgt, ours, o, mapped, exclude=s)
            finally:
                STATE["strict"] = True
            if elsewhere:
                STATE["samename_fail"].append((s, elsewhere[:3], [f for f in sub if f[1] in (
                    "BYTES-DIFFER", "RELOC-COUNT", "RELOC-SHAPE", "MAPPED-VS-PLACEHOLDER")][:3]))
                if out is not None:
                    out.append((depth, "SAMENAME-CONTRADICTED", s, o))
            else:
                STATE["samename_unchecked"] = STATE.get("samename_unchecked", 0) + 1
                ok = True
        memo[key] = ok
        return ok
    return ORIG(tgt, ours, s, o, mapped, depth=depth, stack=stack, memo=memo,
                out=out, maxdepth=maxdepth, ctx=ctx)


ORIG_SLOTS = m._slots_agree


def slots_wrapper(tgt, ours, rt, ob, survivor, our_name, mapped, depth, stack,
                  memo, out, maxdepth, tolerate_placeholders, ctx=None):
    # `_slots_agree` skips a same-name slot itself (`if rn == on: continue`)
    # before it would ever call chase, so the strict check has to sit here.
    if STATE["strict"] and len(rt[1]) == len(ob[1]):
        for (ro, rn, rty), (oo, on, oty) in zip(rt[1], ob[1]):
            if rn == on and ro == oo and rty == oty and not m.placeholder(rn):
                if not chase_wrapper(tgt, ours, rn, on, mapped, depth=depth + 1,
                                     stack=stack, memo=memo, out=out,
                                     maxdepth=maxdepth, ctx=ctx):
                    return False
    return ORIG_SLOTS(tgt, ours, rt, ob, survivor, our_name, mapped, depth, stack,
                      memo, out, maxdepth, tolerate_placeholders, ctx=ctx)


m.chase = chase_wrapper
m._slots_agree = slots_wrapper


def judge(tgt, ours, mapped, s, f, memo):
    v, _d = m.adjudicate(tgt, ours, s, f, mapped, verbose=False)
    if v == "PROVEN":
        flat = True
    else:
        flat = False
        if v != "REFUTED":
            return v, None
    out = []
    ok = True if flat else m.chase(tgt, ours, s, f, mapped, out=out, memo=memo)
    return ("PROVEN" if ok else "REFUTED"), out


def run(groups, tgt, ours, mapped, only=None):
    res = []
    memo_b, memo_s = {}, {}
    for gi, g in enumerate(groups):
        if only is not None and gi not in only:
            continue
        s = g["survivor"]
        for f in g.get("folded", []):
            STATE["strict"] = False
            b, _ = judge(tgt, ours, mapped, s, f, memo_b)
            # strict: flat T1 is ALSO blind to same-name slots, so strict always
            # chases (the chase is a superset of flat T1 by construction).
            STATE["strict"] = True
            STATE["samename_fail"] = []
            v, _d = m.adjudicate(tgt, ours, s, f, mapped, verbose=False)
            if v in ("PROVEN", "REFUTED"):
                out = []
                st = "PROVEN" if m.chase(tgt, ours, s, f, mapped, out=out,
                                          memo=memo_s) else "REFUTED"
            else:
                st, out = v, []
            STATE["strict"] = False
            res.append({"group": gi, "address": g.get("address"), "survivor": s,
                        "folded": f, "base": b, "strict": st,
                        "samename_contradicted": [x[2] for x in out
                                                  if x[1] == "SAMENAME-CONTRADICTED"],
                        "leaf": [list(map(str, x)) for x in STATE["samename_fail"]][:3]})
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    groups = json.loads((ROOT / "scripts/symbol_aliases.json").read_text())["groups"]
    tgt, ours = m.load_sides()
    mapped = m.load_mapped()
    mangled = sum(1 for n in tgt if n.startswith("?"))
    if mangled < 1000:
        sys.exit("REFUSING: target objs look PRE-RENAMER (%d mangled names)" % mangled)
    if a.selftest:
        # Positive control: the strict comparator must reject group 771's old
        # proof when the depth-1 callee carries the SAME name on both sides,
        # i.e. the 2026-09-15 map state.  Reproduce it by giving the retail
        # 0x82773E70 body our spelling.
        idx = [i for i, g in enumerate(groups) if (g.get("address") or "").lower() == "0x82775950"]
        g = groups[idx[0]]
        ours_dra = ("??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@FV?$StlNodeAlloc@F"
                    "@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXV?$reverse_"
                    "iterator@PAV?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@@0@0"
                    "ABU__false_type@0@@Z")
        surv_dra = [n for n in tgt if n.startswith("??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@V?$RangedData@I@")]
        assert len(surv_dra) == 1, surv_dra
        rt = tgt[g["survivor"]]
        tgt2 = dict(tgt)
        tgt2[ours_dra] = tgt[surv_dra[0]]
        tgt2[g["survivor"]] = (rt[0], [(o, ours_dra if n == surv_dra[0] else n, t)
                                        for (o, n, t) in rt[1]], rt[2])
        f = g["folded"][0] if g["folded"] else ours_dra.replace("??$__destroy_range_aux", "??1")
        f = "??1?$vector@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@V?$StlNodeAlloc@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA@XZ"
        STATE["strict"] = False
        flat, _ = m.adjudicate(tgt2, ours, g["survivor"], f, mapped, verbose=False)
        STATE["strict"] = True
        st = m.chase(tgt2, ours, g["survivor"], f, mapped, out=[], memo={})
        STATE["strict"] = False
        print("selftest (09-15 map state): shipped flat T1 = %s ; strict chase = %s"
              % (flat, "PROVEN" if st else "REFUTED"))
        good = flat == "PROVEN" and not st
        # Negative control: a membership the strict pass must keep (W16-JE's
        # SetObjConcrete<UILabel>, flat T1 PROVEN, retail_bodytwins 1).
        g0 = groups[0]
        uil = "?SetObjConcrete@?$ObjPtr@VUILabel@@@@QAAXPAVUILabel@@@Z"
        STATE["strict"] = True
        keep = m.chase(tgt, ours, g0["survivor"], uil, mapped, out=[], memo={})
        STATE["strict"] = False
        print("negative control (SetObjConcrete<UILabel>): strict chase = %s"
              % ("PROVEN" if keep else "REFUTED"))
        good = good and keep
        print("SELFTEST", "PASS" if good else "FAIL")
        sys.exit(0 if good else 1)
    res = run(groups, tgt, ours, mapped)
    Path(a.out).write_text(json.dumps(res, indent=1))
    import collections
    c = collections.Counter((r["base"], r["strict"]) for r in res)
    print("memberships", len(res), "same-name slots byte-checked", STATE["samename_checked"])
    for k, v in sorted(c.items()):
        print("  base=%-11s strict=%-11s %d" % (k[0], k[1], v))


if __name__ == "__main__":
    main()
