#!/usr/bin/env python3
# uniq.py a.log b.log: message kinds (digits/hex/addresses stripped) present in only one log, with counts;
# det-frame lines and scripted-input lines skipped.
import re, sys, collections
def kinds(fn):
    c = collections.Counter()
    for l in open(fn, 'rb'):
        l = l.decode('latin-1').rstrip()
        if l.startswith(('rb3-app-det: frame', 'DC3 Input', 'rb3-app joypad: pad', 'rb3-app-chartplay: gem', 'rb3-app-mic')): continue
        k = re.sub(r'0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?', '#', l)[:160]
        c[k] += 1
    return c
a, b = kinds(sys.argv[1]), kinds(sys.argv[2])
for k in sorted(set(a) - set(b)): print(f'  only-L x{a[k]}: {k}')
for k in sorted(set(b) - set(a)): print(f'  only-6 x{b[k]}: {k}')
