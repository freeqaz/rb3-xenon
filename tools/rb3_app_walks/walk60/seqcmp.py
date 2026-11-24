#!/usr/bin/env python3
# seqcmp.py L.log 6.log: settled-screen name sequences of two runs (frames dropped), first divergence.
import re, sys
def seq(fn):
    s = []; prev = None; last = 0
    for l in open(fn, 'rb'):
        if not l.startswith(b'rb3-app-det: frame '): continue
        m = re.search(rb'frame (\d+) .* screen (\S+) transition (\S+)', l)
        if m:
            last = int(m.group(1))
            if m.group(3) == b'none' and m.group(2) != prev:
                prev = m.group(2); s.append((m.group(2).decode().replace('_screen', ''), last))
    return s, last
(a, fa), (b, fb) = seq(sys.argv[1]), seq(sys.argv[2])
n = 0
while n < min(len(a), len(b)) and a[n][0] == b[n][0]: n += 1
print(f'L {len(a)} screens to frame {fa}, 60 {len(b)} screens to frame {fb}; common prefix {n}')
print('  L  tail:', ' > '.join(f'{x}@{f}' for x, f in a[max(0, n - 2):n + 4]))
print('  60 tail:', ' > '.join(f'{x}@{f}' for x, f in b[max(0, n - 2):n + 4]))
