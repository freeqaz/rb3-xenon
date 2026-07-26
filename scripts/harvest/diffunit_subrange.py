#!/usr/bin/env python3
"""diffunit_subrange.py -- sub-range margin selection for different-unit gaps.

WHY SUB-RANGES
--------------
`diffunit_margin.py` decides a WHOLE gap for one neighbour.  That is strictly
worse than necessary, for two reasons:

  1.  Funding a gap drags in every function it contains -- including the
      `both`-class ones (matched under EITHER owner), which carry no ownership
      information.  At whole-gap granularity the best achievable evidence share
      was 79.8%, i.e. one in five funded matches was a coin flip.
  2.  A splits pin is a RANGE, but the two neighbours extend from OPPOSITE
      ends.  Left can claim a PREFIX (`left.end` moves right) and right a
      SUFFIX (`right.start` moves left) of the SAME gap, leaving the middle
      unowned.  Whole-gap argmax cannot express that; it forces a false choice.

So the decision variable is not a direction but a pair of cuts
`va_lo <= p <= q <= va_hi`: left claims `[va_lo, p)`, right claims `[q, va_hi)`,
`[p, q)` stays unowned.  Both edits are overlap-free by construction.

THE RULE
--------
Per side, choose the cut that maximises EVIDENCED functions subject to a margin
against IDENTITY-UNRESOLVED riders:

    include prefix of length k  iff  ev_L(k) >= 1  and  ev_L(k) - un(k) >= T

where `ev_L(k)` = functions in the first k that matched under LEFT-assignment
only, and `un(k)` = functions in the first k that matched under BOTH.  Ties go
to the smaller k (claim as little as the evidence justifies).  Symmetric for the
suffix with `ev_R`.  If the two cuts would cross, the side with more evidence
keeps its cut and the other is shrunk to meet it.

T=0 is argmax-equivalent (any evidence funds).  T>=1 requires the evidence to
strictly outweigh the coin flips it drags in.  Calibrate T on a held-out set.

INPUT
-----
Two whole-binary builds: every gap assigned LEFT, and every gap assigned RIGHT.
Their `report.json` per-function tables give, unit-agnostically by NAME, which
functions reach 100.0 under each assignment.

USAGE
  diffunit_subrange.py --gaps gaps.json --left L.json --right R.json
                       --base B.json [--curve] [-T N] [--out sel.json]
"""
import argparse
import json


def matched_names(p):
    return {n for _u, n, pct in json.load(open(p)) if pct == 100.0}


def classify(gaps, mL, mR, mB):
    for g in gaps:
        g['fns'].sort(key=lambda f: f['va'])
        for f in g['fns']:
            n = f['name']
            if n in mB:
                f['cls'] = 'pre'
                continue
            l, r = n in mL, n in mR
            f['cls'] = 'both' if (l and r) else 'L' if l else 'R' if r else 'none'
    return gaps


def best_cut(fns, side, T):
    """-> (k, evidenced, unresolved) best prefix length for `side` in {'L','R'}.

    fns is in claim order (address order for a left prefix, reversed for a
    right suffix)."""
    ev = un = 0
    best = (0, 0, 0)
    for i, f in enumerate(fns, 1):
        c = f.get('cls')
        if c == side:
            ev += 1
        elif c == 'both':
            un += 1
        if ev >= 1 and ev - un >= T and ev > best[1]:
            best = (i, ev, un)
    return best


def select(gaps, T):
    out = []
    for g in gaps:
        fns = g['fns']
        kL, evL, unL = best_cut(fns, 'L', T)
        kR, evR, unR = best_cut(list(reversed(fns)), 'R', T)
        # resolve a crossing: the weaker side yields
        while kL + kR > len(fns):
            if evL >= evR:
                kR, evR, unR = best_cut(list(reversed(fns))[:len(fns) - kL], 'R', T)
                if kL + kR <= len(fns):
                    break
                kR = evR = unR = 0
            else:
                kL, evL, unL = best_cut(fns[:len(fns) - kR], 'L', T)
                if kL + kR <= len(fns):
                    break
                kL = evL = unL = 0
        rec = {'va_lo': g['va_lo'], 'va_hi': g['va_hi'],
               'left_unit': g['left_unit'], 'right_unit': g['right_unit'],
               'left_block': g['left_block'], 'right_block': g['right_block']}
        if kL:
            f = fns[kL - 1]
            rec['p'] = f['va'] + f['size']
            rec['evL'], rec['unL'] = evL, unL
        if kR:
            rec['q'] = fns[len(fns) - kR]['va']
            rec['evR'], rec['unR'] = evR, unR
        if kL or kR:
            out.append(rec)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gaps', required=True)
    ap.add_argument('--left', required=True)
    ap.add_argument('--right', required=True)
    ap.add_argument('--base', required=True)
    ap.add_argument('--curve', action='store_true')
    ap.add_argument('-T', type=int, default=1)
    ap.add_argument('--out')
    a = ap.parse_args()

    gaps = [g for g in json.load(open(a.gaps)) if g.get('n_fns')]
    classify(gaps, matched_names(a.left), matched_names(a.right),
             matched_names(a.base))

    if a.curve:
        print(f"{'T':>4} {'gaps':>6} {'cuts':>6} {'evidenced':>10} "
              f"{'unresolved':>11} {'total':>7} {'evid share':>11}")
        for T in (0, 1, 2, 3, 4, 5, 8, 12):
            sel = select(gaps, T)
            ev = sum(r.get('evL', 0) + r.get('evR', 0) for r in sel)
            un = sum(r.get('unL', 0) + r.get('unR', 0) for r in sel)
            cuts = sum(('p' in r) + ('q' in r) for r in sel)
            print(f"{T:4d} {len(sel):6d} {cuts:6d} {ev:10d} {un:11d} "
                  f"{ev+un:7d} {ev/max(1,ev+un):10.1%}")

    sel = select(gaps, a.T)
    ev = sum(r.get('evL', 0) + r.get('evR', 0) for r in sel)
    un = sum(r.get('unL', 0) + r.get('unR', 0) for r in sel)
    print(f"\nT={a.T}: {len(sel)} gaps touched, {ev} evidenced + {un} unresolved "
          f"= {ev+un} fns ({ev/max(1,ev+un):.1%} evidenced)")
    if a.out:
        json.dump(sel, open(a.out, 'w'))
        print('wrote', a.out)


if __name__ == '__main__':
    main()
