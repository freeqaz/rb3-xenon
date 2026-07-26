#!/usr/bin/env python3
"""diffunit_margin.py -- the MARGIN RULE for different-unit gap attribution.

THE DECISION
------------
A different-unit gap (see diffunit_gap_funnel.py) has exactly two geometrically
available owners: the LEFT neighbour unit or the RIGHT one.  `argmax` -- fund
whichever direction gains more -- is NOT safe here, because objdiff's
`pair_funclets_by_bytes` scores an anonymous funclet purely on masked BYTE
equality against ANY funclet-shaped Code symbol in the assigned unit's obj.

  ! The pairing is NOT uniqueness-gated, contrary to
    docs/plans/lane-al-autocarve-2026-07-26.md and splits_move.py's docstring.
    Uniqueness is required only in pass 1 (mod.rs ~1471).  Pass 2 pairs
    ambiguous exact-signature groups greedily; pass 2b pairs over-subscribed
    groups MANY-TO-ONE onto an already-consumed base funclet on purpose; pass 3
    does same-size fuzzy pairing at >=50% masked byte equality.

Since 72% of this pool is <= 68 bytes -- the modal MSVC PPC EH-cleanup (40 B)
and static-init-guard (32 B) shapes, which occur in hundreds of our objs -- a
large fraction of it scores 100% under EITHER owner.  Such a match is
byte-true but attributed on a coin flip: an IDENTITY-UNRESOLVED fill.

THE SIGNAL
----------
Two whole-binary builds: one assigning every gap LEFT, one assigning every gap
RIGHT.  Per function f:

    exclusive-left   matched under L only   -> evidence f belongs to the left unit
    exclusive-right  matched under R only   -> evidence f belongs to the right unit
    both             matched under both     -> NO ownership information at all
    neither          matched under neither  -> dead, funding it gains nothing

Only the EXCLUSIVE classes carry ownership evidence.  `both` is the fake-match
exposure, and counting it as yield is exactly the dishonesty this lane exists to
avoid.

THE RULE
--------
Fund gap g in direction d iff

    ex_d - ex_other >= T        and        ex_d >= T

with T calibrated on a held-out set (see the lane doc for the measured
precision/yield curve).  T=0 is argmax.  Variants swept here:
`purity` (ex_other == 0), `relative` (share of exclusive evidence), and
`size-gated` (a higher T for bodies <= 44 B, where the fake-match hazard
concentrates).

USAGE
-----
  diffunit_margin.py --gaps gaps.json --left L_table.json --right R_table.json
                     [--base base_table.json] [--curve] [--out sel.json] [-T N]
"""
import argparse
import collections
import json


def matched_names(table_path):
    """report.json dump [[unit, name, pct], ...] -> set of names at 100.0"""
    t = json.load(open(table_path))
    return {n for _u, n, p in t if p == 100.0}


def classify(gaps, mL, mR, mBase):
    """Attach per-function and per-gap L/R evidence classes."""
    for g in gaps:
        exL = exR = both = none = 0
        for f in g['fns']:
            n = f['name']
            if n in mBase:          # already matched before the fill: no signal
                f['cls'] = 'pre'
                continue
            l, r = n in mL, n in mR
            if l and r:
                f['cls'] = 'both'; both += 1
            elif l:
                f['cls'] = 'L'; exL += 1
            elif r:
                f['cls'] = 'R'; exR += 1
            else:
                f['cls'] = 'none'; none += 1
        g['exL'], g['exR'], g['both'], g['none'] = exL, exR, both, none
    return gaps


def decide(g, T, mode='abs', small=44, T_small=None):
    """-> ('left'|'right'|None, evidence, unresolved_riders)"""
    exL, exR = g['exL'], g['exR']
    if mode == 'size':
        # a stricter threshold when the exclusive evidence is all short bodies
        big = lambda side: sum(1 for f in g['fns']
                               if f.get('cls') == side and f['size'] > small)
        t = T if (big('L') or big('R')) else (T_small if T_small is not None else T + 1)
    else:
        t = T
    if mode == 'purity':
        if exL >= t and exR == 0:
            return 'left'
        if exR >= t and exL == 0:
            return 'right'
        return None
    if exL - exR >= t and exL >= max(t, 1):
        return 'left'
    if exR - exL >= t and exR >= max(t, 1):
        return 'right'
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gaps', required=True)
    ap.add_argument('--left', required=True)
    ap.add_argument('--right', required=True)
    ap.add_argument('--base', required=True)
    ap.add_argument('--curve', action='store_true')
    ap.add_argument('--out')
    ap.add_argument('-T', type=int, default=1)
    ap.add_argument('--mode', default='abs',
                    choices=['abs', 'purity', 'size'])
    a = ap.parse_args()

    gaps = [g for g in json.load(open(a.gaps)) if g.get('n_fns')]
    mL, mR, mB = (matched_names(a.left), matched_names(a.right),
                  matched_names(a.base))
    classify(gaps, mL, mR, mB)

    tot = collections.Counter()
    for g in gaps:
        for f in g['fns']:
            tot[f.get('cls')] += 1
    print(f"gap functions: {sum(tot.values())}  " +
          '  '.join(f"{k}={v}" for k, v in sorted(tot.items())))
    print(f"identity-unresolved share of all gains: "
          f"{tot['both'] / max(1, tot['both'] + tot['L'] + tot['R']):.1%}")

    if a.curve:
        print(f"\n{'rule':>14} {'gaps':>6} {'fns(evid)':>10} {'fns(unres)':>11} "
              f"{'total':>7} {'evid share':>11}")
        rules = ([('argmax', 'abs', 0)] +
                 [(f'T>={t}', 'abs', t) for t in (1, 2, 3, 4, 5, 8)] +
                 [(f'pure>={k}', 'purity', k) for k in (1, 2, 3)] +
                 [(f'size-gate T{t}', 'size', t) for t in (1, 2)])
        for label, mode, t in rules:
            ng = ev = un = 0
            for g in gaps:
                d = decide(g, t, mode)
                if not d:
                    continue
                ng += 1
                ev += g['exL'] if d == 'left' else g['exR']
                un += g['both']
            print(f"{label:>14} {ng:6d} {ev:10d} {un:11d} {ev+un:7d} "
                  f"{ev/max(1,ev+un):10.1%}")

    sel = []
    for g in gaps:
        d = decide(g, a.T, a.mode)
        if d:
            sel.append({'va_lo': g['va_lo'], 'dir': d,
                        'evidenced': g['exL'] if d == 'left' else g['exR'],
                        'unresolved': g['both']})
    print(f"\nselected at {a.mode} T={a.T}: {len(sel)} gaps, "
          f"{sum(s['evidenced'] for s in sel)} evidenced + "
          f"{sum(s['unresolved'] for s in sel)} unresolved fns")
    if a.out:
        json.dump(sel, open(a.out, 'w'))
        print('wrote', a.out)


if __name__ == '__main__':
    main()
