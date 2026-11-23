#!/usr/bin/env python3
"""tcmp.py A.hits B.hits ...: per phrase-hit/miss event frame, each run's score increment since its
previous event (phrase increments; a score change with no hit/miss change is printed as 'bonus')."""
import sys
runs = []
for p in sys.argv[1:]:
    ev = {}; prev = None
    for ln in open(p):
        fr, sc, h, m = map(int, ln.split())
        if prev:
            d = sc - prev[0]
            kind = 'hit' if h > prev[1] else 'miss' if m > prev[2] else 'bonus'
            ev[fr] = (kind, d)
        prev = (sc, h, m)
    runs.append(ev)
last = min(max(e) for e in runs)
frames = sorted(set().union(*[set(e) for e in runs]))
for f in frames:
    if f > last: break
    print(f, *[('%s %+d' % r[f]) if f in r else '-' for r in runs], sep='\t')
