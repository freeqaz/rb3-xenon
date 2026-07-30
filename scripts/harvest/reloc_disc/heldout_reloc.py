#!/usr/bin/env python3
"""laneAS-B — leave-one-out precision of the RELOC-CONTENT discriminator on the
EXACT_AMBIG tier, measured against the alphabetical-tie-break baseline on the
SAME population.

Protocol (identical population to heldout_exact.py):
  for each target VA in a pinned unit that has a truth name in
  target_symbol_map.json and whose name exists as a code symbol in that unit's
  base obj:
      supply := unpaired base code symbols U {held-out symbol}
      class  := supply members with masked bytes == target masked bytes
      if |distinct names| >= 2  -> EXACT_AMBIG; run both deciders.
  The held-out VA is removed from the resolver so no self-reference leaks.
"""
import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402
import pickle                 # noqa: E402


def _scope(mangled):
    """{scope/name tokens} of an MSVC mangled symbol, up to the first '@@'."""
    if not mangled.startswith("?"):
        return set()
    body = mangled.lstrip("?")
    i = body.find("@@")
    if i < 0:
        return set()
    return {t for t in body[:i].split("@") if t}


def _ev(vs, pick, S):
    """evidence summary for the picked candidate + strongest competitor"""
    if pick is None:
        return None
    w = None
    others = []
    for c, s in vs:
        d = dict(agree=s["agree"], contra=s["contra"], unk=s["unk"],
                 chans=s["chans"])
        if S.anon_ns_strip(c["name"]) == pick:
            w = d
        else:
            others.append(d)
    sc = _scope(pick)
    scope_ok = False
    for c, s in vs:
        if S.anon_ns_strip(c["name"]) != pick:
            continue
        for o, bn, tt, v, ch in s["det"]:
            if v == "AGREE" and (sc & _scope(S.anon_ns_strip(bn))):
                scope_ok = True
    return dict(win=w, nother=len(others), scope_ok=scope_ok,
                other_max_agree=max([o["agree"] for o in others], default=0),
                other_min_contra=min([o["contra"] for o in others], default=0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--out", default=None)
    ap.add_argument("--bodyidx", default=None,
                    help="optional; defaults to <lblidx dir>/bodyidx.pkl")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    lbl = json.loads(Path(args.lblidx).read_text())
    lbl = {int(k): v for k, v in lbl.items()}

    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    truth = {}
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            try:
                truth[int(k, 16)] = v
            except ValueError:
                pass

    res = R.Resolver(wt)
    disc = D.Disc(wt, lbl, res, S)
    # bodyidx is OPTIONAL: only the R1/R2 variant (pick2/tier2) consults it. This
    # load used to be unconditional, so a missing bodyidx.pkl hard-crashed the
    # whole harness -- and no script on laneAS-B ever produced that file, which is
    # a large part of why this tool set died unrun. Build one with bodyidx.py.
    bip = Path(args.bodyidx) if args.bodyidx else Path(args.lblidx).parent / "bodyidx.pkl"
    if bip.exists():
        disc.bodyidx = pickle.load(open(bip, "rb"))
    else:
        disc.bodyidx = None
        print(f"[warn] no bodyidx at {bip}: R2 disabled (pick2 == pick)",
              file=sys.stderr)

    rows = []
    units = list(D.unit_iter(wt))
    for k, (name, tobj, tasm, cobj) in enumerate(units):
        if k % 100 == 0:
            print(f"[{k}/{len(units)}] {name}", file=sys.stderr)
        if not (tobj.exists() and tasm.exists() and cobj.exists()):
            continue
        try:
            tf = R.target_funcs(tasm)
            _, _, tsyms = S._parse_coff(tobj)
            tnames = {S.anon_ns_strip(s["name"]) for s in tsyms}
            bf, _defined = R.base_funcs(cobj)
        except Exception:
            continue
        by_name = defaultdict(list)
        for f in bf:
            by_name[S.anon_ns_strip(f["name"])].append(f)
        supply = []
        for f in bf:
            nm = S.anon_ns_strip(f["name"])
            if nm in tnames or S.is_internal(f["name"]):
                continue
            if D.FUNCLET_LIKE.match(f["name"]):
                continue
            supply.append(f)
        by_bytes = defaultdict(list)
        for f in supply:
            by_bytes[f["masked"]].append(f)

        for va, ti in tf.items():
            tn = truth.get(va)
            if tn is None or S.is_internal(tn) or D.FUNCLET_LIKE.match(tn):
                continue
            hs = by_name.get(S.anon_ns_strip(tn))
            if not hs:
                continue
            held = hs[0]
            grp = list(by_bytes.get(ti["masked"], []))
            if held["masked"] == ti["masked"] and \
               all(S.anon_ns_strip(g["name"]) != S.anon_ns_strip(tn) for g in grp):
                grp = grp + [held]
            if len(grp) < 2:
                continue
            # dedup by stripped name
            seen, cands = set(), []
            for g in grp:
                n2 = S.anon_ns_strip(g["name"])
                if n2 in seen:
                    continue
                seen.add(n2)
                cands.append(g)
            if len(cands) < 2:
                continue
            truthn = S.anon_ns_strip(tn)
            # baseline: alphabetical first
            base_pick = sorted(seen)[0]
            # hide this VA from the resolver (no self-reference leak)
            hidden = res.va2names.pop(va, None)
            popped = []
            if hidden:
                for nm2 in hidden:
                    if va in res.name2vas.get(nm2, ()):
                        res.name2vas[nm2].discard(va)
                        popped.append(nm2)
            try:
                pick, tier, vs = D.decide(disc, ti, cands)
                pick2, tier2, vs2 = D.decide(disc, ti, cands,
                                             use_r1=True, use_r2=True)
            finally:
                if hidden:
                    res.va2names[va] = hidden
                    for nm2 in popped:
                        res.name2vas[nm2].add(va)
            rows.append(dict(unit=name, va=va, size=ti["size"], n=len(cands),
                             truth=truthn, base_pick=base_pick,
                             base_ok=(base_pick == truthn),
                             pick=pick, tier=tier,
                             ok=(None if pick is None else pick == truthn),
                             nrel=len(ti["relocs"]),
                             pick2=pick2, tier2=tier2,
                             ok2=(None if pick2 is None else pick2 == truthn),
                             ev=_ev(vs, pick, disc.S),
                             ev2=_ev(vs2, pick2, disc.S)))
    if args.out:
        json.dump(rows, open(args.out, "w"))

    def band(sz):
        return "<=32" if sz <= 32 else ("33-68" if sz <= 68 else ">68")

    print(f"\n=== EXACT_AMBIG population n={len(rows)} ===")
    bo = sum(1 for r in rows if r["base_ok"])
    print(f"BASELINE (alphabetical tie-break): {100.0*bo/max(1,len(rows)):.2f}%"
          f"  ({bo}/{len(rows)})")
    print("\ntier breakdown:")
    for t, c in Counter(r["tier"] for r in rows).most_common():
        sub = [r for r in rows if r["tier"] == t]
        dec = [r for r in sub if r["pick"] is not None]
        ok = sum(1 for r in dec if r["ok"])
        b = sum(1 for r in sub if r["base_ok"])
        print(f"  {t:18s} n={len(sub):6d}  decided={len(dec):6d}  "
              f"precision={'%.2f%%'%(100.0*ok/len(dec)) if dec else '   --  '}"
              f"   (baseline on same rows {100.0*b/len(sub):.2f}%)")

    dec = [r for r in rows if r["pick"] is not None]
    ok = sum(1 for r in dec if r["ok"])
    bsame = sum(1 for r in dec if r["base_ok"])
    print(f"\nALL DECIDED  n={len(dec)}  precision={100.0*ok/max(1,len(dec)):.2f}%"
          f"   recall={100.0*len(dec)/max(1,len(rows)):.1f}%"
          f"   (baseline on those same rows {100.0*bsame/max(1,len(dec)):.2f}%)")

    strict = [r for r in rows if r["tier"] == "DECISIVE"]
    oks = sum(1 for r in strict if r["ok"])
    print(f"DECISIVE-only n={len(strict)}  precision="
          f"{100.0*oks/max(1,len(strict)):.2f}%")

    print("\nby size band (DECISIVE tier):")
    for bnd in ("<=32", "33-68", ">68"):
        sub = [r for r in strict if band(r["size"]) == bnd]
        allb = [r for r in rows if band(r["size"]) == bnd]
        o = sum(1 for r in sub if r["ok"])
        bb = sum(1 for r in allb if r["base_ok"])
        print(f"  {bnd:6s} decided={len(sub):5d} precision="
              f"{'%.2f%%'%(100.0*o/len(sub)) if sub else '  --  '}"
              f"   | pop={len(allb):5d} baseline="
              f"{'%.2f%%'%(100.0*bb/len(allb)) if allb else '  --  '}")

    print("\nby class size n (DECISIVE tier):")
    for nb in ((2, 2), (3, 4), (5, 8), (9, 10 ** 9)):
        sub = [r for r in strict if nb[0] <= r["n"] <= nb[1]]
        allb = [r for r in rows if nb[0] <= r["n"] <= nb[1]]
        o = sum(1 for r in sub if r["ok"])
        bb = sum(1 for r in allb if r["base_ok"])
        lab = f"n={nb[0]}" if nb[0] == nb[1] else f"n={nb[0]}-{nb[1] if nb[1]<10**9 else '+'}"
        print(f"  {lab:8s} decided={len(sub):5d} precision="
              f"{'%.2f%%'%(100.0*o/len(sub)) if sub else '  --  '}"
              f"   | pop={len(allb):5d} baseline="
              f"{'%.2f%%'%(100.0*bb/len(allb)) if allb else '  --  '}")

    bad = [r for r in strict if r["ok"] is False]
    print(f"\nDECISIVE errors: {len(bad)}")
    for r in bad[:20]:
        print(f"  {r['unit']:34s} 0x{r['va']:08x} {r['size']:4d}B n={r['n']}")
        print(f"     truth {r['truth'][:95]}")
        print(f"     pick  {r['pick'][:95]}")


if __name__ == "__main__":
    main()
