#!/usr/bin/env python3
"""Lane PINFIX-1: stronger null for the match predictor + the per-unit ledger.

TWO QUESTIONS THIS ANSWERS BEFORE ANY EDIT IS MADE.

1. IS THE MASKED-EQUAL SCREEN REAL?  One null shift is thin.  Here the screen is
   run against MANY wrong addresses (several fixed shifts AND every other
   candidate's address).  A screen that confirms on wrong addresses is
   low-entropy and its positives mean nothing.

2. WHAT DOES THE MOVE DO TO UNIT COMPLETION?  Moving a row that will NOT match
   is not free: the destination unit gains a 0-scoring row, so a destination
   sitting at 100% FALLS OFF it.  Conversely the source unit LOSES a 0-scoring
   row from its denominator, which can COMPLETE the source (the
   DENOMINATOR_SHRANK mechanism ab_measure reports).  Both directions are
   predicted per unit here so the A/B confirms an expectation instead of
   producing one.
"""
import argparse
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from truncated_pins import Obj, Retail, masked, unmasked_count  # noqa: E402
from pinfix_price import load_report                            # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pred', default=os.path.expanduser('~/tmp/pinfix_pred.json'))
    a = ap.parse_args()

    rows = json.load(open(a.pred))
    R = Retail()
    objcache = {}

    def getobj(p):
        if p not in objcache:
            objcache[p] = Obj(p)
        return objcache[p]

    # ---------------------------------------------------------------- null
    shifts = [0x10, 0x20, 0x40, 0x80, 0x100, -0x40, -0x100]
    addrs = [int(r['addr'], 16) for r in rows]
    nt = nh = 0
    xt = xh = 0
    for r in rows:
        if len(r['defining_objs']) != 1:
            continue
        fb = getobj(r['defining_objs'][0]).funcs.get(r['name'])
        if fb is None:
            continue
        body, mask = fb
        want = masked(body, mask)
        A = int(r['addr'], 16)
        for s in shifts:
            g = R.bytes_at(A + s, len(body))
            if g is None:
                continue
            nt += 1
            if masked(g, mask) == want:
                nh += 1
        # cross-null: this body against every OTHER candidate's address
        for B in addrs:
            if B == A:
                continue
            g = R.bytes_at(B, len(body))
            if g is None:
                continue
            xt += 1
            if masked(g, mask) == want:
                xh += 1

    print('[NULL shifts]      %d/%d confirm at wrong offsets %s' % (nh, nt, shifts))
    print('[NULL cross-addrs] %d/%d confirm at other candidates\' addresses' % (xh, xt))
    print('  %s' % ('screen DISCRIMINATES (a wrong pairing is rejected)'
                    if nh == 0 and xh == 0 else
                    'WARNING: screen confirms wrong pairings -- positives are weak'))

    # low-entropy detail
    lows = [r for r in rows if r.get('predict') == 'MATCH_LOW_ENTROPY']
    print('\n[low-entropy positives] %d rows with <3 fully-unmasked words:' % len(lows))
    for r in lows:
        print('  %s size=%-5s our=%-5s unmasked_words=%s  %s'
              % (r['addr'], r.get('size'), r.get('our_size'),
                 r.get('unmasked_words'), r['name'][:58]))

    # ---------------------------------------------------------------- ledger
    _, _, units, top, _ = load_report()

    def ukey(u):
        base = u[:-4] if u.endswith('.cpp') else u
        return 'default/' + os.path.basename(base)

    will_match = {r['addr'] for r in rows
                  if r.get('predict') in ('MATCH', 'MATCH_LOW_ENTROPY')}

    src = collections.defaultdict(lambda: [0, 0])   # unit -> [rows_out, matched_out]
    dst = collections.defaultdict(lambda: [0, 0])   # unit -> [rows_in, matched_in]
    movable = []
    for r in rows:
        if not r['defining_units']:
            continue                                  # no pinned destination
        s = ukey(r['pinned_unit'])
        d = ukey(r['defining_units'][0])
        if s == d:
            continue
        movable.append(r)
        src[s][0] += 1
        dst[d][0] += 1
        if r['addr'] in will_match:
            dst[d][1] += 1

    print('\n[movable] %d rows have a pinned destination unit distinct from the source'
          % len(movable))

    at100 = {u for u, m in units.items()
             if m['total_functions'] and m['matched_functions'] == m['total_functions']}
    print('[units] %d units currently at 100%% (mpn ruler)' % len(at100))

    risk = []
    for d, (nin, nmatch) in sorted(dst.items()):
        if d in at100 and nmatch < nin:
            risk.append((d, nin, nmatch))
    print('\n[RISK: destination units at 100%% that would gain a NON-matching row]')
    if not risk:
        print('  none -- no destination unit is currently at 100%%')
    for d, nin, nmatch in risk:
        print('  %-40s gains %d rows, %d predicted to match' % (d, nin, nmatch))

    comp = []
    for s, (nout, _) in sorted(src.items()):
        m = units.get(s)
        if not m or not m['total_functions']:
            continue
        # every moved row currently scores 0, so it is unmatched in the source
        if m['matched_functions'] == m['total_functions'] - nout:
            comp.append((s, nout, m['matched_functions'], m['total_functions']))
    print('\n[UPSIDE: source units that COMPLETE once their 0-scoring rows leave]')
    if not comp:
        print('  none')
    for s, nout, mf, tf in comp:
        print('  %-40s %d/%d -> %d/%d  (DENOMINATOR_SHRANK)' % (s, mf, tf, mf, tf - nout))

    nm = sum(1 for r in movable if r['addr'] in will_match)
    nb = sum(int(r.get('size') or 0) for r in movable if r['addr'] in will_match)
    print('\n[PREDICTION for the A/B over the movable set]')
    print('  rows moved            : %d' % len(movable))
    print('  predicted to MATCH    : %d  (+%d matched_functions)' % (nm, nm))
    print('  predicted bytes       : +%d B (%.4f pp of total_code=%d)'
          % (nb, 100.0 * nb / max(top['total_code'], 1), top['total_code']))
    print('  predicted masked_equal: +0 (these are real bodies, not funclet twins)')


if __name__ == '__main__':
    main()
