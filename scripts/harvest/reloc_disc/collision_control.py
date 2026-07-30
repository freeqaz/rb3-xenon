#!/usr/bin/env python3
"""laneBT5 -- NEGATIVE CONTROL for the collision-adjudication channel.

collision_adjudicate.py answers "which of two rival addresses owns mangled name
N". Its 'branch wins' verdicts assert main's map is WRONG, so the number that
matters is the FALSE-FLIP RATE: how often does the adjudicator move a name off an
address that was already correct?

Control construction: take a main map entry N@A that is NOT contested by any
branch and NOT flagged arbitrary, and pair it against a DECOY address B chosen
from the reloc-masked byte twins of A (B is mapped to some other name, so it does
not own N). Truth is A by construction. Any DECISIVE verdict for B is a false
flip -- the exact error mode that would corrupt the map.

Usage:
  collision_control.py --worktree WT --lblidx LBL --collisions C.json
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--collisions", required=True)
    ap.add_argument("--seed", type=int, default=1234)
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
    contested = set(json.loads(Path(args.collisions).read_text()))

    va2ti, name2fn = CA.build_index(wt, S)
    twins = defaultdict(list)
    for va, (u, ti) in va2ti.items():
        twins[ti["masked"]].append(va)

    rows = []
    for va, nm in va2name.items():
        if va in arb or nm in contested:
            continue
        ent = va2ti.get(va)
        if ent is None:
            continue
        bf = name2fn.get(S.anon_ns_strip(nm))
        if bf is None:
            continue
        pool = [b for b in twins.get(ent[1]["masked"], [])
                if b != va and b in va2name and va2name[b] != nm]
        if not pool:
            continue
        decoy = rnd.choice(pool)
        v_true = CA.verdict(disc, ent[1], bf)
        v_dec = CA.verdict(disc, va2ti[decoy][1], bf)
        oks = [(k, v) for k, v in (("true", v_true), ("decoy", v_dec))
               if v["ok"] and v["agree"] > 0]
        allok = [k for k, v in (("true", v_true), ("decoy", v_dec)) if v["ok"]]
        if len(allok) == 1 and len(oks) == 1:
            rows.append((oks[0][0], nm, va, decoy))

    n = len(rows)
    good = sum(1 for r in rows if r[0] == "true")
    print(f"\n=== collision-channel NEGATIVE CONTROL ===")
    print(f"  DECISIVE control rows : {n}")
    print(f"  picked TRUE address   : {good}")
    print(f"  FALSE FLIPS (picked decoy): {n - good}")
    print(f"  precision             : {100.0*good/max(1,n):.2f}%")
    for k, nm, va, dc in [r for r in rows if r[0] != "true"][:15]:
        print(f"    flip 0x{va:08x} -> 0x{dc:08x}  {nm[:70]}")


if __name__ == "__main__":
    main()
