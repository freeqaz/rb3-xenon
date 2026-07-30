#!/usr/bin/env python3
"""laneBU4 -- NEGATIVE CONTROL for the LIVE EXACT_AMBIG emission channel.

Why the calibrated 99.41% does NOT transfer to the live pool
------------------------------------------------------------
heldout_reloc.py only admits a VA whose truth name EXISTS as a code symbol in
that unit's base obj:

    hs = by_name.get(S.anon_ns_strip(tn))
    if not hs: continue

so its population guarantees, by construction, that the correct answer is among
the candidates.  Its precision is therefore conditional on TRUTH-PRESENT.

The live pool offers no such guarantee.  Those VAs are unmapped precisely
because no earlier pass could name them; the true owner may not be compiled, may
live in another unit's supply, or may not exist in our tree at all.  When truth
is absent, EVERY candidate is wrong and the only correct behaviour is to REFUSE.
An emitter that happily picks the least-contradicted wrong name would still
score 99.41% on the calibration population and still poison the map here.

Control construction (truth ablation)
-------------------------------------
Take the calibration population -- mapped VAs, EXACT_AMBIG, truth known and
present -- and DELETE the true candidate from the candidate set.  Truth is now
absent by construction, exactly mirroring the live failure mode.  Require >=2
candidates to remain so the row is still a genuine ambiguity.  Hide the VA from
the resolver so no self-reference leaks (same as heldout).

    any row that passes the SHIP GATE is a FALSE PLANT.

The number that matters is the false-plant rate under the ship gate, in the
33-68 B band that is actually shipped.  A near-zero rate means the gate is safe
on the live pool *regardless* of how often truth is present there, because it
refuses when truth is absent -- which is what makes the control decisive rather
than merely suggestive.

Usage:
  livecontrol.py --worktree WT --lblidx LBL [--bodyidx B] [--out rows.json]
"""
import argparse
import json
import pickle
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reloclib as R          # noqa: E402
import relocdisc as D         # noqa: E402


def _scope(mangled):
    if not mangled.startswith("?"):
        return set()
    body = mangled.lstrip("?")
    i = body.find("@@")
    if i < 0:
        return set()
    return {t for t in body[:i].split("@") if t}


def scope_vacuous(pick, cands, S):
    """True when some RIVAL candidate carries the identical scope-token set as
    the pick.

    laneBU4: this is the mechanism behind the truth-absent false plants. shipB
    accepts on `scope_ok`, which fires when an AGREEing callee shares a scope
    token with the pick. If a rival shares the pick's ENTIRE scope, that test is
    satisfied identically by the rival and therefore separates nothing -- it is
    evidence-shaped non-evidence. Empirically the plants concentrate exactly
    there: `?ByteCode@C@@UBAEXZ` vs `?StaticByteCode@C@@SAEXZ` (same class,
    different method) plant at 76-91%, whereas `?ClassName@A@@` vs
    `?ClassName@B@@` (same method, different class, so scopes DIFFER) plant at
    1.96%.
    """
    if pick is None:
        return True
    ps = _scope(pick)
    for c in cands:
        n = S.anon_ns_strip(c["name"])
        if n == pick:
            continue
        if _scope(n) == ps:
            return True
    return False


def scope_unique(pick, cands, vs, S):
    """Does the scope overlap that fired `scope_ok` use a token the RIVALS do
    not also carry?

    laneBU4, second formulation. The first (scope_vacuous, exact set equality)
    removed 0 of 48 plants and was simply wrong: _scope() includes the METHOD
    token, so {ByteCode,C} != {StaticByteCode,C} and same-class siblings never
    compare equal. But scope_ok fires on ANY shared token, and for those pairs
    the shared token is the CLASS name -- which the rival carries too. So the
    overlap is real yet powerless. Require instead that some AGREE-producing
    overlap token is absent from every rival's scope.
    """
    if pick is None:
        return False
    ps = _scope(pick)
    rival = set()
    for c in cands:
        n = S.anon_ns_strip(c["name"])
        if n != pick:
            rival |= _scope(n)
    for c, s in vs:
        if S.anon_ns_strip(c["name"]) != pick:
            continue
        for o, bn, tt, v, ch in s["det"]:
            if v != "AGREE":
                continue
            if (ps & _scope(S.anon_ns_strip(bn))) - rival:
                return True
    return False


def ship_gate(pick, tier, vs, size, S):
    """The EXACT ship gate of emit_reloc_frag.py, as a reusable predicate.
    -> (ship: bool, label: 'A'|'B'|'', detail dict)"""
    if pick is None:
        return False, "", {}
    win = None
    for c, s in vs:
        if S.anon_ns_strip(c["name"]) == pick:
            win = s
    if win is None:
        return False, "", {}
    chans = win["chans"]
    content = sum(v for k, v in chans.items() if k in ("string", "rtti", "const"))
    sc = _scope(pick)
    scope_ok = False
    for c, s in vs:
        if S.anon_ns_strip(c["name"]) != pick:
            continue
        for o, bn, tt, v, ch in s["det"]:
            if v == "AGREE" and (sc & _scope(S.anon_ns_strip(bn))):
                scope_ok = True
    inband = 33 <= size <= 68
    shipA = tier == "DECISIVE" and content >= 1 and inband
    shipB = (tier == "DECISIVE" and scope_ok and win["unk"] == 0
             and inband and content < 1)
    return (bool(shipA or shipB), ("A" if shipA else ("B" if shipB else "")),
            dict(content=content, scope_ok=scope_ok, agree=win["agree"],
                 unk=win["unk"], chans=chans))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--lblidx", required=True)
    ap.add_argument("--bodyidx", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--no-ablate", action="store_true",
                    help="POSITIVE arm: keep truth in the candidate set. Same "
                         "gate code, so the two arms are comparable.")
    args = ap.parse_args()

    wt = Path(args.worktree).resolve()
    S = R.load_S(wt)
    lbl = {int(k): v for k, v in json.loads(Path(args.lblidx).read_text()).items()}
    res = R.Resolver(wt)
    disc = D.Disc(wt, lbl, res, S)
    bip = Path(args.bodyidx) if args.bodyidx else Path(args.lblidx).parent / "bodyidx.pkl"
    disc.bodyidx = pickle.load(open(bip, "rb")) if bip.exists() else None

    cur = json.loads((wt / "scripts/target_symbol_map.json").read_text())
    truth = {}
    for k, v in cur.items():
        if isinstance(v, str) and k.startswith("0x"):
            try:
                truth[int(k, 16)] = v
            except ValueError:
                pass

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
            bf, _ = R.base_funcs(cobj)
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
            truthn = S.anon_ns_strip(tn)
            hs = by_name.get(truthn)
            if not hs:
                continue
            held = hs[0]
            grp = list(by_bytes.get(ti["masked"], []))
            if held["masked"] == ti["masked"] and \
               all(S.anon_ns_strip(g["name"]) != truthn for g in grp):
                grp = grp + [held]
            seen, cands = set(), []
            for g in grp:
                n2 = S.anon_ns_strip(g["name"])
                if n2 in seen:
                    continue
                seen.add(n2)
                cands.append(g)
            if len(cands) < 2:
                continue          # not an EXACT_AMBIG row at all

            if args.no_ablate:
                # POSITIVE arm: truth present, identical gate code. This is the
                # calibration population, re-measured through the same predicate
                # so the two arms are strictly comparable.
                abl = cands
            else:
                # ---- ABLATION: delete the true candidate. Truth now ABSENT.
                abl = [c for c in cands if S.anon_ns_strip(c["name"]) != truthn]
                if len(abl) == len(cands):
                    continue      # truth was not in the class; nothing ablated
                if len(abl) < 2:
                    continue      # ambiguity collapses; not comparable to live

            hidden = res.va2names.pop(va, None)
            popped = []
            if hidden:
                for nm2 in hidden:
                    if va in res.name2vas.get(nm2, ()):
                        res.name2vas[nm2].discard(va)
                        popped.append(nm2)
            try:
                pick, tier, vs = D.decide(disc, ti, abl)
            finally:
                if hidden:
                    res.va2names[va] = hidden
                    for nm2 in popped:
                        res.name2vas[nm2].add(va)

            ship, lab, det = ship_gate(pick, tier, vs, ti["size"], S)
            rows.append(dict(unit=name, va=va, size=ti["size"],
                             n_orig=len(cands), n_abl=len(abl),
                             truth=truthn, pick=pick, tier=tier,
                             ship=ship, tierlab=lab, det=det,
                             vacuous=scope_vacuous(pick, abl, S),
                             uniq=scope_unique(pick, abl, vs, S),
                             ok=(None if pick is None else pick == truthn)))

    if args.out:
        json.dump(rows, open(args.out, "w"))

    def band(s):
        return "<=32" if s <= 32 else ("33-68" if s <= 68 else ">68")

    print(f"\n=== TRUTH-ABLATION NEGATIVE CONTROL  (truth absent by "
          f"construction; correct behaviour = REFUSE) ===")
    print(f"  ablated rows tested        : {len(rows)}")
    print(f"  still DECISIVE (a pick)    : {sum(1 for r in rows if r['tier']=='DECISIVE')}")
    fp = [r for r in rows if r["ship"]]
    print(f"  PASSED SHIP GATE = FALSE PLANTS : {len(fp)}")
    print(f"  refusal rate (all bands)   : "
          f"{100.0*(len(rows)-len(fp))/max(1,len(rows)):.2f}%")

    print("\n  by size band:")
    for b in ("<=32", "33-68", ">68"):
        sub = [r for r in rows if band(r["size"]) == b]
        f2 = [r for r in sub if r["ship"]]
        print(f"    {b:6s} n={len(sub):5d}  false plants={len(f2):4d}  "
              f"refusal={100.0*(len(sub)-len(f2))/max(1,len(sub)):6.2f}%")

    print("\n  tier distribution under ablation:")
    for t, c in Counter(r["tier"] for r in rows).most_common():
        print(f"    {t:18s} {c:6d}")

    inb = [r for r in rows if 33 <= r["size"] <= 68]
    inbf = [r for r in inb if r["ship"]]
    print(f"\n  ** SHIP BAND (33-68 B): n={len(inb)}  false plants={len(inbf)}"
          f"  false-plant rate={100.0*len(inbf)/max(1,len(inb)):.2f}% **")
    for r in inbf[:20]:
        print(f"    PLANT {r['unit']:34s} 0x{r['va']:08x} {r['size']:3d}B "
              f"n{r['n_orig']}->{r['n_abl']} tier{r['tierlab']}")
        print(f"       truth(absent) {r['truth'][:88]}")
        print(f"       planted       {str(r['pick'])[:88]}")


if __name__ == "__main__":
    main()
