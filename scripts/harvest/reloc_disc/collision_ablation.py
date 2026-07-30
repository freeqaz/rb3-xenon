#!/usr/bin/env python3
"""laneBV3 -- TRUTH-ABLATION control for the collision-adjudication channel.

BT-5's collision_control.py is a TRUTH-PRESENT control: it pairs the correct
address A against one decoy and counts false flips (99.63% at agree>=3). Lane
BU-4 showed on the *live* channel that a truth-present precision does NOT
transfer to a population where truth may be absent -- the inherited gate passed
14.20% of truth-ablated plants. The collision channel has never been given the
equivalent arm, and it needs one: an asserted "main-map defect" is exactly the
case where main's address might be wrong AND the branch's might be wrong too.

Construction
------------
Anchor: an uncontested main map entry N@A whose target function at A is
relocation-CONSISTENT with N's compiled COMDAT (verdict ok, agree>0). That
consistency is the evidence that A is truth.

  POS arm : adjudicate over {A, decoy}          -> truth PRESENT  (reproduces BT-5)
  NEG arm : adjudicate over {decoy1, decoy2}    -> truth ABSENT by construction

Decoys are drawn from A's reloc-masked byte-TWIN pool, which is the correct
distractor distribution: 31 of the 32 live asserted defects have both rivals
masked-byte-identical to the same COMDAT.

Any DECISIVE verdict in the NEG arm is a FALSE PLANT -- the only correct
behaviour when truth is absent is to refuse. Both arms run through the identical
tier logic (collision_adjudicate.verdict + the same DECISIVE rule) so they are
strictly comparable.

Read-only.
"""
import argparse
import json
import random
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R                 # noqa: E402
import relocdisc as D                # noqa: E402
import collision_adjudicate as CA    # noqa: E402


def decisive(cands):
    """cands: {key: verdict}. Same rule as collision_adjudicate."""
    oks = [k for k, v in cands.items() if v["ok"] and v["agree"] > 0]
    allok = [k for k, v in cands.items() if v["ok"]]
    if len(allok) == 1 and len(oks) == 1:
        return oks[0]
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--decisive", required=True,
                    help="collision_verdicts_decisive.json (names to exclude)")
    ap.add_argument("--seed", type=int, default=20260730)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    lbl = {int(k): v for k, v in json.loads(Path(args.lblidx).read_text()).items()}
    res = R.Resolver(wt)
    disc = D.Disc(wt, lbl, res, S)
    rnd = random.Random(args.seed)

    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    arb = set()
    for key in ("_bijection_arbitrary", "_icf_arbitrary", "_denylist"):
        for a in (cur.get(key) or []):
            try:
                arb.add(int(a, 16))
            except (ValueError, TypeError):
                pass
    va2name = {}
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            try:
                va2name[int(k, 16)] = v
            except ValueError:
                pass
    contested = {r["name"] for r in json.loads(Path(args.decisive).read_text())}

    va2ti, name2fn = CA.build_index(wt, S)
    twins = defaultdict(list)
    for va, (u, ti) in va2ti.items():
        twins[ti["masked"]].append(va)

    pos, neg = [], []
    stats = defaultdict(int)
    for va, nm in sorted(va2name.items()):
        if va in arb or nm in contested:
            stats["skip_arb_or_contested"] += 1
            continue
        ent = va2ti.get(va)
        if ent is None:
            stats["skip_no_target"] += 1
            continue
        bf = name2fn.get(S.anon_ns_strip(nm))
        if bf is None:
            stats["skip_no_comdat"] += 1
            continue
        v_true = CA.verdict(disc, ent[1], bf)
        if not (v_true["ok"] and v_true["agree"] > 0):
            stats["skip_anchor_inconsistent"] += 1
            continue
        pool = [b for b in twins.get(ent[1]["masked"], [])
                if b != va and b in va2name and va2name[b] != nm]
        if len(pool) < 2:
            stats["skip_pool_lt2"] += 1
            continue
        d1, d2 = rnd.sample(pool, 2)
        stats["eligible"] += 1

        # POS arm: truth present {A, d1}
        c = {"true": v_true, "decoy": CA.verdict(disc, va2ti[d1][1], bf)}
        w = decisive(c)
        if w:
            pos.append(dict(name=nm, win=w, agree=c[w]["agree"]))

        # NEG arm: truth ABSENT {d1, d2}
        c2 = {f"0x{d1:08x}": CA.verdict(disc, va2ti[d1][1], bf),
              f"0x{d2:08x}": CA.verdict(disc, va2ti[d2][1], bf)}
        w2 = decisive(c2)
        neg.append(dict(name=nm, anchor=f"0x{va:08x}",
                        plant=w2, agree=(c2[w2]["agree"] if w2 else 0)))

    Path(args.out).write_text(json.dumps(dict(pos=pos, neg=neg), indent=1))

    print("\n=== funnel ===")
    for k in sorted(stats):
        print(f"  {k:28s} {stats[k]:6d}")

    print("\n=== COLLISION CHANNEL: truth-present (POS) vs truth-ablated (NEG) ===")
    print(f"{'cut':>12} | {'POS n':>6} {'correct':>7} {'prec':>8} | "
          f"{'NEG trials':>10} {'PLANTS':>7} {'plant-rate':>10}")
    negn = len(neg)
    for cut in (1, 2, 3, 4, 5):
        p = [r for r in pos if r["agree"] >= cut]
        pc = sum(1 for r in p if r["win"] == "true")
        pl = [r for r in neg if r["plant"] and r["agree"] >= cut]
        print(f"  agree>={cut:<5}| {len(p):6d} {pc:7d} "
              f"{100.0*pc/max(1,len(p)):7.2f}% | {negn:10d} {len(pl):7d} "
              f"{100.0*len(pl)/max(1,negn):9.2f}%")


if __name__ == "__main__":
    main()
