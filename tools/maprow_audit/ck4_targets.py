#!/usr/bin/env python3
"""IS THE FOLD CONFOUND REAL?  Adjudicate the TARGET rows, not the thunk rows.

THE ASYMMETRY LANE CJ-2 COULD NOT SEE
-------------------------------------
The branch channel calls a thunk row A defective when

    h(A) = method claimed by A's own map name
    w(A) = method claimed by the map name on the address A BRANCHES TO

disagree.  Every one of the 225 METHOD_DIFFERS rows is therefore charged AGAINST
A -- but the disagreement is symmetric evidence.  It is equally consistent with

    W1  the TARGET row's name is wrong (a map defect at T), A being fine;
    W2  T is an ICF fold representative with several true identities, the map
        names one, A claims another, and BOTH ARE CORRECT.

CJ-2 assumed neither and priced every row as a defect at A.  If W1 or W2
dominates, the whole 217-row classification inverts, which is why CJ-2 correctly
refused to synthesise names until this was settled.

W2 IS DECIDABLE FROM RETAIL BYTES ALONE
---------------------------------------
/OPT:ICF folds only COMDATs identical INCLUDING RELOCATIONS (measured by
ck4_foldscan: 51 reloc-identical surplus over 40,628 bodies, BELOW a
random-offset null of 168-187, against 3,967 shape-identical survivors -- a 78x
gap).  A fold therefore merges bodies that are byte-for-byte the same code.

So a fold can NEVER unify a 12/16-byte forwarder thunk with a framed function:
they differ in length and in shape.  If the map name on T is a THUNK MANGLING
(`$4`/`W`/`G`...) while T's body is NOT a forwarder, then:

    * the thunk name cannot be a true identity of T under ANY folding, and
    * the name on T is simply WRONG.

That is W1, proven, with no appeal to the map's correctness anywhere else.  The
verdict is map-INDEPENDENT: it compares a NAME's required shape against RETAIL
BYTES.

THE NULL
--------
The same shape test is applied to the targets of the AGREE (known-good) rows.
Those rows are correct by the branch channel, so their targets should be
shape-consistent.  If AGREE targets are contradicted at the same rate, the test
is measuring noise and must be discarded.
"""
import argparse
import collections
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from thunk_identity import (Image, thunk_kind, is_forwarder, sig_strict)   # noqa: E402
import cj2_cycles                                                          # noqa: E402
from ck4_foldscan import pdata, is_funclet                                 # noqa: E402

BAND = os.path.join(ROOT, 'orig/45410914/band.exe')
MAP = os.path.join(ROOT, 'scripts/target_symbol_map.json')


def extents(img):
    return {va: ln for va, ln, _p, _e in pdata(img)}


def shape_verdict(img, ext, va, name):
    """Is `name` shape-compatible with the body at `va`?

    -> 'THUNK_NAME_ON_REAL_BODY'  (map name is a thunk mangling, body is framed
                                   code)  == W1 PROVEN, fold impossible
       'PLAIN_NAME_ON_FORWARDER'  (map name is an ordinary method, body is a
                                   bare forwarder) -- weak, a tail call looks
                                   the same, so NOT charged
       'CONSISTENT'
    """
    fwd = is_forwarder(img, va) is not None
    tk = thunk_kind(name) is not None
    if tk and not fwd:
        return 'THUNK_NAME_ON_REAL_BODY'
    if not tk and fwd:
        return 'PLAIN_NAME_ON_FORWARDER'
    return 'CONSISTENT'


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--map', default=MAP)
    ap.add_argument('--exe', default=BAND)
    ap.add_argument('--json')
    args = ap.parse_args()

    tmap, img, pins, rows = cj2_cycles.build(args.map, args.exe)
    ext = extents(img)

    v = collections.Counter(r['verdict'] for r in rows.values())
    print('thunk code population %d   %s' % (len(rows), dict(v)))

    # ---- fan-in over targets, per verdict -------------------------------
    for verdict in ('METHOD_DIFFERS', 'AGREE'):
        sel = {a: r for a, r in rows.items() if r['verdict'] == verdict}
        fan = collections.Counter(r['target'] for r in sel.values() if r.get('target'))
        shapes = collections.Counter()
        tgt_rows = {}
        for a, r in sel.items():
            t = r.get('target')
            if not t or t not in tgt_rows:
                if t:
                    tgt_rows[t] = shape_verdict(img, ext, int(t, 16), tmap[t])
        # per-SITE and per-DISTINCT-TARGET tallies
        site = collections.Counter(tgt_rows[r['target']]
                                   for r in sel.values() if r.get('target'))
        dist = collections.Counter(tgt_rows.values())
        print('\n=== %s : %d rows over %d DISTINCT targets ==='
              % (verdict, len(sel), len(fan)))
        print('  target shape verdict, per SITE          : %s' % dict(site))
        print('  target shape verdict, per DISTINCT target: %s' % dict(dist))
        if verdict == 'METHOD_DIFFERS':
            print('  top fan-in targets:')
            for t, c in fan.most_common(12):
                ln = ext.get(int(t, 16))
                print('    %s fan-in %-3d len=%-5s %-28s %s'
                      % (t, c, ln, tgt_rows[t], (tmap[t] or '')[:64]))
            md_tgt_rows = tgt_rows
            md_fan = fan
        shapes = shapes  # noqa

    # ---- the headline: how many SITES are explained by a bad TARGET row --
    bad = {t for t, s in md_tgt_rows.items() if s == 'THUNK_NAME_ON_REAL_BODY'}
    sites = sum(md_fan[t] for t in bad)
    print('\n=== W1 (defect is at the TARGET, not the thunk) ===')
    print('  distinct targets shape-CONTRADICTED : %d of %d'
          % (len(bad), len(md_tgt_rows)))
    print('  METHOD_DIFFERS SITES they explain   : %d of %d = %.1f%%'
          % (sites, sum(md_fan.values()), 100.0 * sites / max(sum(md_fan.values()), 1)))

    if args.json:
        out = {'bad_targets': {t: {'fanin': md_fan[t], 'name': tmap[t],
                                   'len': ext.get(int(t, 16))} for t in sorted(bad)},
               'target_shape': md_tgt_rows}
        json.dump(out, open(args.json, 'w'), indent=1)
        print('wrote %s' % args.json)


if __name__ == '__main__':
    main()
