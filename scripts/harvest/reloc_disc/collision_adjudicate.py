#!/usr/bin/env python3
"""laneBT5 -- adjudicate map NAME-COLLISION rows with the reloc-content discriminator.

A collision row is: some abandoned branch places mangled name N at address B,
while main's target_symbol_map.json places N at a different address A. A mangled
name owns exactly one address, so at most one side is right and the other is a
map MISPAIR. Byte comparison cannot separate them -- these are reloc-masked byte
twins by construction (that is why they collided) -- so this asks the
relocation-content channel instead:

  take N's compiled COMDAT body (base side, with its relocation SYMBOL NAMES) and
  score it against the target function at A and at B. The side whose relocations
  are consistent (contra==0, shape==0, agree>0) owns the name.

Funnel (the COMDAT-availability gate): a row is decidable only if N exists as a
code symbol in some compiled base obj AND the rival VAs live in pinned units with
a dtk asm listing. Most rows fail this and are reported as UNAVAILABLE, not guessed.

Usage:
  collision_adjudicate.py --worktree WT --lblidx LBL --collisions C.json --out O.json
"""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402


def build_index(wt, S):
    """-> (va2ti, name2basefn) over every pinned unit."""
    va2ti, name2fn = {}, {}
    units = list(D.unit_iter(wt))
    for k, (name, tobj, tasm, cobj) in enumerate(units):
        if k % 200 == 0:
            print(f"  [{k}/{len(units)}] {name}", file=sys.stderr)
        if not (tasm.exists() and cobj.exists()):
            continue
        try:
            tf = R.target_funcs(tasm)
            bf, _ = R.base_funcs(cobj)
        except Exception:
            continue
        for va, ti in tf.items():
            va2ti.setdefault(va, (name, ti))
        for f in bf:
            name2fn.setdefault(S.anon_ns_strip(f["name"]), f)
    return va2ti, name2fn


def verdict(disc, ti, bf):
    s = disc.score(ti["relocs"], bf["relocs"])
    size_ok = (ti["size"] == bf["size"])
    ok = (s["contra"] == 0 and s["shape"] == 0 and size_ok)
    return dict(agree=s["agree"], contra=s["contra"], shape=s["shape"],
                size_ok=size_ok, tsize=ti["size"], bsize=bf["size"], ok=ok)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--collisions", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    lbl = {int(k): v for k, v in json.loads(Path(args.lblidx).read_text()).items()}
    res = R.Resolver(wt)
    disc = D.Disc(wt, lbl, res, S)

    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    name2main = {}
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            name2main.setdefault(v, k.lower())

    coll = json.loads(Path(args.collisions).read_text())
    print(f"collision names: {len(coll)}", file=sys.stderr)
    va2ti, name2fn = build_index(wt, S)
    print(f"index: {len(va2ti)} target VAs, {len(name2fn)} base names",
          file=sys.stderr)

    rows, stats = [], defaultdict(int)
    for n, addrs in coll.items():
        bf = name2fn.get(S.anon_ns_strip(n))
        if bf is None:
            stats["no_base_comdat"] += 1
            continue
        mainaddr = name2main.get(n)
        cands = {}
        if mainaddr:
            cands[mainaddr] = "main"
        for a in addrs:
            cands.setdefault(a.lower(), "branch")
        seen = {}
        for a, side in cands.items():
            try:
                va = int(a, 16)
            except ValueError:
                continue
            ent = va2ti.get(va)
            if ent is None:
                continue
            seen[a] = (side, ent[0], verdict(disc, ent[1], bf))
        if len(seen) < 2:
            stats["under_2_targets_available"] += 1
            continue
        oks = [a for a, (sd, u, v) in seen.items() if v["ok"] and v["agree"] > 0]
        allok = [a for a, (sd, u, v) in seen.items() if v["ok"]]
        if len(allok) == 1 and len(oks) == 1:
            win = oks[0]
            tier = "DECISIVE"
        elif len(allok) == 1:
            win = allok[0]
            tier = "ELIM_ONLY"
        elif not allok:
            win, tier = None, "ALL_CONTRA"
        else:
            win, tier = None, "TIE"
        stats[tier] += 1
        rows.append(dict(name=n, tier=tier, win=win,
                         win_side=(seen[win][0] if win else None),
                         branches={a: sorted(v) for a, v in addrs.items()},
                         cands={a: dict(side=sd, unit=u, **v)
                                for a, (sd, u, v) in seen.items()}))
    Path(args.out).write_text(json.dumps(rows, indent=1))
    print("\n=== collision adjudication ===")
    for k in sorted(stats):
        print(f"  {k:26s} {stats[k]:6d}")
    dec = [r for r in rows if r["tier"] == "DECISIVE"]
    print(f"\n  DECISIVE rows: {len(dec)}")
    mw = sum(1 for r in dec if r["win_side"] == "main")
    print(f"    main wins   {mw:5d}   (main map CONFIRMED, branch row was a mispair)")
    print(f"    branch wins {len(dec)-mw:5d}   (main map DEFECT -> repoint candidate)")


if __name__ == "__main__":
    main()
