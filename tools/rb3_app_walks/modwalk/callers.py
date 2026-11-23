#!/usr/bin/env python3
"""callers.py <hexva>...: retail bl/b sites targeting each VA, with containing function + map name."""
import sys, struct
sys.argv, args = sys.argv[:1], sys.argv[1:]
import os; exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'xref.py')).read().split('txt = secva')[0])
txt = secva('.text'); code = data[txt[3]:txt[3] + txt[4]]
for a in args:
    t = int(a, 16); print('%08X %s' % (t, names.get(t, '')))
    for i in range(0, len(code), 4):
        w = struct.unpack_from('>I', code, i)[0]
        if (w >> 26) == 18:
            off = w & 0x03FFFFFC
            if off & 0x02000000: off -= 0x04000000
            src = txt[1] + i
            if (src + off if not (w & 2) else off) == t:
                f = fn_of(src); print('   %s %08X in %s %s' % ('bl' if w & 1 else 'b ', src, ('%08X' % f) if f else '?', names.get(f, '')))
