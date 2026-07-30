#!/usr/bin/env python3
"""laneBV3 -- price a map REPOINT before building it.

WHY THIS EXISTS
---------------
A map repoint moves mangled name N from address A to address B. Whether that
changes `matched_functions` is fully determined by data already on disk, so
there is no reason to spend a build cycle finding out. Two questions decide it:

  (1) SUPPLY  -- does the compiled BASE obj of that VA's unit define a COMDAT
                 named N?  objdiff pairs unit U's target obj against unit U's
                 base obj BY NAME, so a name parked at a VA whose unit has no
                 such COMDAT pairs with nothing: it scores 0 and merely unpairs
                 whatever it left behind.  (The BQ-1 / BS-3 gate.)
  (2) OUTCOME -- are the base COMDAT's masked bytes equal to the target
                 function's masked bytes at that VA?  That is what scoring 100%
                 means once `report generate` masks relocations.

    dmatched(repoint A -> B) = OUTCOME(B) - OUTCOME(A)

THE RESULT THAT MOTIVATED IT
----------------------------
Run against the 32 collision rows that assert a main-map defect
(`collision_verdicts_decisive.json`, win_side == "branch"), this classified
**31 of 32 as NEUTRAL_both_match**: A and B are BOTH masked-byte-identical to
the same COMDAT, so a repoint just moves the name between two functions that
each already score 100%.  dmatched is 0 *by construction*, not by luck.  A
same-split A/B over all 32 then measured exactly that -- 40953/1518/39435/
34.513645 on both legs, zero change on every axis.

That is a structural property of the collision channel, not an accident of
these 32: a name collision only survives to a DECISIVE verdict when both rivals
are reloc-masked byte twins, and reloc-masked twins are precisely the functions
the report cannot tell apart.  `masked_equal_functions` does NOT disclose them
either -- its only producers are the funclet over-subscription / cross-unit
byte-promotion paths (objdiff-core diff/mod.rs:1378,1384; objdiff-cli
cmd/report.rs:846), never reloc masking.  So the at-100% reloc defect class is
invisible to BOTH priced axes, and collision repoints can only ever be
correctness work.

Use this before funding any repoint wave.  A wave that comes back all
NEUTRAL_both_match is metric-inert and should be priced as correctness only.

USAGE
    repoint_supply.py --worktree WT --verdicts collision_verdicts_decisive.json \\
                      [--side branch] --out supply.json
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402


def classify(win_c, main_c):
    if not win_c["supply"]:
        return "DEAD_no_supply_at_win"
    w = bool(win_c["outcome"])
    m = bool(main_c and main_c["outcome"])
    if w and m:
        return "NEUTRAL_both_match"
    if w and not m:
        return "PAY_+1"
    if m and not w:
        return "LOSS_-1"
    return "ZERO_neither_matches"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--verdicts", required=True)
    ap.add_argument("--side", default="branch",
                    help="only rows whose win_side is this (default: branch)")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    rows = json.loads(Path(args.verdicts).read_text())
    rows = [r for r in rows if r.get("win_side") == args.side]
    print(f"rows: {len(rows)}", file=sys.stderr)

    want_units = {c["unit"] for r in rows for c in r["cands"].values()}
    ubase, utgt = {}, {}
    for uname, tobj, tasm, cobj in D.unit_iter(wt):
        if uname not in want_units or not (tasm.exists() and cobj.exists()):
            continue
        try:
            bf, _ = R.base_funcs(cobj)
            utgt[uname] = R.target_funcs(tasm)
            ubase[uname] = {S.anon_ns_strip(f["name"]): f for f in bf}
        except Exception as e:
            print(f"  !! {uname}: {e}", file=sys.stderr)

    rep_path = wt / "build/45410914/report.json"
    cur = {}
    if rep_path.exists():
        for u in json.loads(rep_path.read_text())["units"]:
            for f in u.get("functions", []):
                cur.setdefault(f["name"], []).append(
                    (u["name"], f.get("fuzzy_match_percent", 0.0)))

    out, tally = [], {}
    for r in rows:
        sn = S.anon_ns_strip(r["name"])
        rec = dict(name=r["name"], win=r["win"], win_agree=r.get("win_agree"),
                   report=cur.get(r["name"]), cands={})
        for a, c in r["cands"].items():
            u, va = c["unit"], int(a, 16)
            bfn = ubase.get(u, {}).get(sn)
            tfn = utgt.get(u, {}).get(va)
            rec["cands"][a] = dict(
                side=c["side"], unit=u, agree=c["agree"], contra=c["contra"],
                supply=bfn is not None,
                outcome=(None if (bfn is None or tfn is None)
                         else bytes(bfn["masked"]) == bytes(tfn["masked"])),
                tsize=(tfn["size"] if tfn else None),
                bsize=(bfn["size"] if bfn else None))
        wc = rec["cands"][r["win"]]
        mains = [c for c in rec["cands"].values() if c["side"] == "main"]
        mc = mains[0] if mains else None
        rec["cls"] = classify(wc, mc)
        rec["dmatched_pred"] = ((1 if wc["outcome"] else 0)
                                - (1 if (mc and mc["outcome"]) else 0))
        tally[rec["cls"]] = tally.get(rec["cls"], 0) + 1
        out.append(rec)

    Path(args.out).write_text(json.dumps(out, indent=1))
    print("\n=== REPOINT PRICING ===")
    for k, v in sorted(tally.items(), key=lambda kv: -kv[1]):
        print(f"  {k:26s} {v:4d}")
    print(f"  predicted net dmatched = {sum(r['dmatched_pred'] for r in out)}")


if __name__ == "__main__":
    main()
