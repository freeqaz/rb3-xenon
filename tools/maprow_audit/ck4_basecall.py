#!/usr/bin/env python3
"""THE BASE-CALL CHANNEL -- a third, independent read of a body's true method.

WHY A THIRD CHANNEL IS REQUIRED
-------------------------------
The fan-in-21 target 0x82402f68 is charged by the BRANCH channel (16 thunks
named ?Replace@RndDir@@$4... forward to it) and its incumbent name is refuted by
the SHAPE channel (a $4 thunk mangling on a 56-byte framed body; a fold cannot
unify a 12/16-byte forwarder with a framed function).  Both of those, however,
read the MAP.  A candidate from one oracle is not a repair -- CI-2's branch
verifier rejected 2 of its 4 RTTI candidates -- so the confirming channel must
not be the generating one.

THE MECHANISM
-------------
A virtual method on a multiply-inherited class that must run for every base
compiles to a BASE FORWARDER: adjust `this` to each base subobject and call THAT
BASE'S SAME METHOD.  0x82402f68 is exactly this shape:

    mflr r12 / bl __savegprlr / stwu / mr r31,r3
    addi r3,r3,-0x34 / mr r30,r4 / mr r29,r5 / bl  X
    mr r5,r29 / mr r4,r30 / addi r3,r31,-0x54 / bl  Y
    addi r1,r1,0x70 / b __restgprlr

So the METHOD NAMES of X and Y name the body's own method.  This reads the
CALLEES' rows, never the body's own row and never the thunks' rows.

★ THIS CHANNEL IS EXACTLY WHAT report.rs:394 THROWS AWAY.  objdiff hard-sets
relocation args to None, so a body calling ObjectDir::Replace and one calling
ObjectDir::Export are INDISTINGUISHABLE to match%.  That is precisely why match%
cannot adjudicate a fold, and why this channel is independent evidence rather
than a restatement of the metric.

CONTROLS
--------
N1 KNOWN-GOOD FLOOR.  Run on the AGREE population's targets: the consensus
   callee-method must equal the (already known-correct) thunk method.  A channel
   that disagrees with known-good bodies is noise.
N2 SHUFFLE.  Re-run with the callee->name association shuffled across targets.
   The agreement rate must collapse.
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

from thunk_identity import Image, sig_strict                                # noqa: E402
import cj2_cycles                                                          # noqa: E402
from ck4_foldscan import pdata                                             # noqa: E402

BAND = os.path.join(ROOT, 'orig/45410914/band.exe')
MAP = os.path.join(ROOT, 'scripts/target_symbol_map.json')

# helpers the compiler emits around every framed body -- never a base call
HELPER = ('savegpr', 'restgpr', 'savefpr', 'restfpr', 'savevmx', 'restvmx',
          '__security', 'memcpy', 'memset')


def callees(img, va, length):
    """Absolute targets of every `bl` in [va, va+length)."""
    out = []
    off = img.offset(va)
    if off is None:
        return out
    for i in range(length // 4):
        w = struct.unpack_from('>I', img.data, off + i * 4)[0]
        if (w >> 26) == 18 and (w & 1):                 # bl (LK set)
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            t = li if ((w >> 1) & 1) else (va + i * 4 + li)
            out.append(t & 0xFFFFFFFF)
    return out


def body_method(img, tmap, ext, va, namemap=None):
    """Consensus METHOD of a body, read from its named `bl` callees.

    -> (method, n_supporting, n_named_callees) or (None, 0, n).
    """
    ln = ext.get(va)
    if ln is None or ln > 0x400:
        return None, 0, 0
    ms = []
    nm = namemap if namemap is not None else tmap
    for t in callees(img, va, ln):
        n = nm.get('0x%08x' % t)
        if not isinstance(n, str) or any(h in n for h in HELPER):
            continue
        s = sig_strict(n)
        if s:
            ms.append(s[0])
    if not ms:
        return None, 0, 0
    c = collections.Counter(ms)
    top, k = c.most_common(1)[0]
    return top, k, len(ms)


def run(img, tmap, ext, rows, verdict, namemap=None):
    """-> (agree, disagree, abstain) of consensus-callee-method vs thunk method."""
    ag = dis = ab = 0
    detail = []
    for a, r in rows.items():
        if r['verdict'] != verdict or not r.get('target'):
            continue
        t = int(r['target'], 16)
        m, k, n = body_method(img, tmap, ext, t, namemap)
        if m is None:
            ab += 1
            continue
        if m == r['h']:
            ag += 1
        else:
            dis += 1
        detail.append((a, r['target'], r['h'], r['w'], m, k, n))
    return ag, dis, ab, detail


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--map', default=MAP)
    ap.add_argument('--exe', default=BAND)
    ap.add_argument('--json')
    args = ap.parse_args()

    tmap, img, pins, rows = cj2_cycles.build(args.map, args.exe)
    ext = {va: ln for va, ln, _p, _e in pdata(img)}

    print('=== [N1] KNOWN-GOOD FLOOR: AGREE rows ===')
    ag, dis, ab, _ = run(img, tmap, ext, rows, 'AGREE')
    tot = ag + dis
    print('  callee-consensus method == thunk method: %d/%d = %.1f%%  (abstain %d)'
          % (ag, tot, 100.0 * ag / max(tot, 1), ab))

    print('\n=== [N2] SHUFFLE NULL ===')
    import random
    keys = [k for k in tmap if isinstance(k, str) and k.startswith('0x')]
    for seed in (5, 17):
        rnd = random.Random(seed)
        vals = [tmap[k] for k in keys]
        rnd.shuffle(vals)
        sh = dict(zip(keys, vals))
        a2, d2, _b2, _ = run(img, tmap, ext, rows, 'AGREE', namemap=sh)
        print('  seed %-3d agreement: %d/%d = %.1f%%'
              % (seed, a2, a2 + d2, 100.0 * a2 / max(a2 + d2, 1)))

    print('\n=== TREATMENT: METHOD_DIFFERS rows ===')
    ag, dis, ab, detail = run(img, tmap, ext, rows, 'METHOD_DIFFERS')
    tot = ag + dis
    print('  callee-consensus method == THUNK-CLAIMED method (h): %d/%d = %.1f%%'
          '  (abstain %d)' % (ag, tot, 100.0 * ag / max(tot, 1), ab))
    print('  => rows where the THUNK is right and the TARGET ROW is wrong')
    byt = collections.defaultdict(list)
    for d in detail:
        byt[d[1]].append(d)
    print('\n  per distinct target (consensus vs incumbent-derived w):')
    for t, ds in sorted(byt.items(), key=lambda kv: -len(kv[1]))[:14]:
        hs = collections.Counter(d[2] for d in ds)
        print('    %s fan=%-3d consensus=%-12s w=%-12s claimed=%s'
              % (t, len(ds), ds[0][4], ds[0][3], dict(hs)))

    if args.json:
        json.dump([{'addr': d[0], 'target': d[1], 'h': d[2], 'w': d[3],
                    'callee_consensus': d[4], 'support': d[5], 'named_callees': d[6]}
                   for d in detail], open(args.json, 'w'), indent=1)
        print('\nwrote %s' % args.json)


if __name__ == '__main__':
    main()
