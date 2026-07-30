#!/usr/bin/env python3
"""laneAS-B — emit the reloc-content-discriminated fragment for the LIVE
EXACT_AMBIG pool, plus a census of decisive / tie / no-evidence refusals.

Ship gate (calibrated leave-one-out on real map entries, ARBITRARY-truth rows
excluded because the alphabetical baseline reproduces them 99% of the time --
they are circular):

  DECISIVE and (content-agreement OR (scope-consistent AND unk==0))
  and 33 <= size <= 68                      -> 95.62% (131/137 held-out rows)

  same gate at size > 68 -> 85.37%, at size <= 32 -> 83.93%: BOTH REFUSED.

  ("137/137" as originally written contradicts 95.62%; 131/137 = 95.620%.
   laneBT5 corrected the arithmetic and re-measured the gate on the CURRENT,
   much larger map: 99.41% (168/169), with the out-of-band refusals reproduced
   at 84.52% (<=32 B) and 84.91% (>68 B). The band gate is sound.)

★ MISSING INPUT: --funnels expects JSON rows tagged cls=="EXACT_AMBIG" for the
  LIVE (unmapped) pool. No producer for those funnel files exists on main or on
  laneAS-B, so this emitter cannot be run as shipped. heldout_reloc.py computes
  the equivalent population for ALREADY-MAPPED VAs (where truth is known) and is
  therefore the runnable calibration path. Writing the live-pool funnel producer
  is the remaining work to make this emitter usable.
"""
import argparse
import json
import pickle
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402
import livecontrol as LC      # noqa: E402


def _scope(mangled):
    if not mangled.startswith("?"):
        return set()
    body = mangled.lstrip("?")
    i = body.find("@@")
    if i < 0:
        return set()
    return {t for t in body[:i].split("@") if t}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--bodyidx", required=True)
    ap.add_argument("--funnels", nargs="+", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--census", required=True)
    ap.add_argument("--scope-unique", action="store_true",
                    help="laneBU4 live-pool cut: additionally require that the "
                         "scope overlap firing scope_ok uses a token NO rival "
                         "carries. Calibrated on the two-arm control in "
                         "livecontrol.py: truth-absent false plants 48->9 of "
                         "338 (14.20%%->2.66%%) while truth-present precision "
                         "rises 99.41%%->100.00%% (133/133). Recommended for "
                         "the LIVE pool, where truth-presence is NOT "
                         "guaranteed the way it is in the calibration set.")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    lbl = {int(k): v for k, v in json.loads(Path(args.lblidx).read_text()).items()}
    res = R.Resolver(wt)
    disc = D.Disc(wt, lbl, res, S)
    disc.bodyidx = pickle.load(open(args.bodyidx, "rb"))

    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    taken = {v for k, v in cur.items() if isinstance(v, str) and k.startswith("0x")}
    mapped_vas = {int(k, 16) for k in cur if k.startswith("0x")}

    amb = []
    for f in args.funnels:
        for r in json.loads(Path(f).read_text()):
            if r.get("cls") == "EXACT_AMBIG":
                amb.append(r)
    print(f"live EXACT_AMBIG rows: {len(amb)}", file=sys.stderr)

    od = json.loads((wt / "objdiff.json").read_text())
    P = {}
    for u in od["units"]:
        tp, bp = u.get("target_path"), u.get("base_path")
        if tp and bp:
            P[u["name"]] = (wt / tp,
                            wt / (tp.replace("/obj/", "/asm/")[:-4] + ".s"),
                            wt / bp)

    by_unit = defaultdict(list)
    for r in amb:
        by_unit[r["unit"]].append(r)

    census = []
    for unit, members in sorted(by_unit.items()):
        ent = P.get(unit)
        if not ent:
            continue
        tobj, tasm, cobj = ent
        if not (tobj.exists() and tasm.exists() and cobj.exists()):
            continue
        try:
            tf = R.target_funcs(tasm)
            _, _, tsyms = S._parse_coff(tobj)
            tnames = {S.anon_ns_strip(s["name"]) for s in tsyms}
            bf, _ = R.base_funcs(cobj)
        except Exception as e:
            print(f"  parse fail {unit}: {e}", file=sys.stderr)
            continue
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
        for r in members:
            ti = tf.get(r["va"])
            if ti is None:
                continue
            grp = by_bytes.get(ti["masked"], [])
            seen, cands = set(), []
            for g in grp:
                n2 = S.anon_ns_strip(g["name"])
                if n2 in seen:
                    continue
                seen.add(n2)
                cands.append(g)
            if len(cands) < 2:
                continue
            pick, tier, vs = D.decide(disc, ti, cands)
            win = None
            for c, s in vs:
                if pick is not None and S.anon_ns_strip(c["name"]) == pick:
                    win = s
            chans = win["chans"] if win else {}
            content = sum(v for k, v in chans.items()
                          if k in ("string", "rtti", "const"))
            scope_ok = False
            if pick is not None:
                sc = _scope(pick)
                for c, s in vs:
                    if S.anon_ns_strip(c["name"]) != pick:
                        continue
                    for o, bn, tt, v, ch in s["det"]:
                        if v == "AGREE" and (sc & _scope(S.anon_ns_strip(bn))):
                            scope_ok = True
            inband = 33 <= r["size"] <= 68
            shipA = tier == "DECISIVE" and content >= 1 and inband
            shipB = (tier == "DECISIVE" and scope_ok
                     and win["unk"] == 0 and inband and content < 1)
            uniq = LC.scope_unique(pick, cands, vs, S)
            if args.scope_unique and not uniq:
                shipA = shipB = False
            census.append(dict(unit=unit, va=r["va"], size=r["size"],
                               n=len(cands), tier=tier, pick=pick,
                               chans=chans, scope_ok=scope_ok, uniq=uniq,
                               agree=win["agree"] if win else 0,
                               unk=win["unk"] if win else 0,
                               ship=bool(shipA or shipB),
                               tierlab=("A" if shipA else ("B" if shipB else "")),
                               alpha=sorted(seen)[0]))

    Path(args.census).write_text(json.dumps(census, indent=1))
    ship = [c for c in census if c["ship"]]
    # collision guard: never plant a name already used at another VA
    frag, dropped = {}, []
    used = set()
    for c in sorted(ship, key=lambda x: x["va"]):
        if c["pick"] in taken or c["pick"] in used:
            dropped.append(c)
            continue
        if c["va"] in mapped_vas:
            dropped.append(c)
            continue
        used.add(c["pick"])
        frag[f"0x{c['va']:08x}"] = c["pick"]
    Path(args.out).write_text(json.dumps(frag, indent=1, sort_keys=True))

    print(f"\n=== LIVE EXACT_AMBIG census (n={len(census)}) ===")
    for t, n in Counter(c["tier"] for c in census).most_common():
        print(f"  {t:18s} {n:5d}")
    print(f"\n  ship tier A (content)      "
          f"{sum(1 for c in census if c['tierlab']=='A'):5d}")
    print(f"  ship tier B (agree>=2,<=68) "
          f"{sum(1 for c in census if c['tierlab']=='B'):5d}")
    print(f"  fragment entries            {len(frag):5d}"
          f"   (dropped for name/VA collision: {len(dropped)})")
    print("\n  size bands of fragment:")
    bb = Counter("<=32" if c["size"] <= 32 else (">68" if c["size"] > 68 else "33-68")
                 for c in ship)
    for k, v in bb.items():
        print(f"    {k:6s} {v}")


if __name__ == "__main__":
    main()
