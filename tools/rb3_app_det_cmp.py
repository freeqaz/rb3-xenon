#!/usr/bin/env python3
"""rb3_app_det_cmp.py <logA> <logB>: compare the rb3-app-det lines of two rb3-app runs
(RB3_APP_DETERMINISTIC=1, native/src/rb3_app_determinism.cpp) over their common frames.
Exit 0 identical, 1 at the first differing frame (printed with two frames of context).
Record: docs/decomp/DETWALK_DETERMINISTIC_RB3APP_WALK_2026-11-23.md."""
import sys
def load(p):
    seed, frames, result = [], {}, None
    for line in open(p, 'rb'):
        line = line.decode('latin-1').rstrip('\n')
        if line.startswith('rb3-app-det: frame '):
            frames[int(line.split()[2])] = line
        elif line.startswith('rb3-app-det:'):
            seed.append(line)
        elif line.startswith('RESULT:') or line.startswith('rc='):
            result = (result + ' | ' if result else '') + line
    return seed, frames, result
sa, fa, ra = load(sys.argv[1]); sb, fb, rb = load(sys.argv[2])
print('A: %d det frames, end %s' % (len(fa), ra)); print('B: %d det frames, end %s' % (len(fb), rb))
print('header lines identical:', sa == sb)
common = sorted(set(fa) & set(fb)); n = len(common)
first = next((f for f in common if fa[f] != fb[f]), None)
if first is None:
    print('IDENTICAL over %d common frames (1..%d)' % (n, common[-1] if common else 0))
    if common: print('last:', fa[common[-1]])
    sys.exit(0)
print('FIRST DIVERGENCE at frame %d of %d common' % (first, n))
for f in range(max(1, first - 2), first + 3):
    if f in fa: print(' A', fa[f])
    if f in fb: print(' B', fb[f])
print('differing frames: %d of %d' % (sum(fa[f] != fb[f] for f in common), n))
sys.exit(1)
