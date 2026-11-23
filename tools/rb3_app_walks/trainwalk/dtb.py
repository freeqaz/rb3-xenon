#!/usr/bin/env python3
"""dtb.py <file.dtb>: decrypt an RB3 (new-style crypt) dtb and print it as dta text."""
import struct, sys
def crand(k):
    r = (k - (k // 0x1F31D) * 0x1F31D) * 0x41A7 - (k // 0x1F31D) * 0xB14
    return r + 0x7FFFFFFF if r <= 0 else r
def decrypt(d):
    k = struct.unpack('<i', d[:4])[0]; out = bytearray()
    for b in d[4:]:
        k = crand(k); out.append(b ^ (k & 0xFF))
    return bytes(out)
class R:
    def __init__(s, b): s.b, s.p = b, 0
    def u(s, f):
        v = struct.unpack_from('<' + f, s.b, s.p); s.p += struct.calcsize(f); return v[0]
    def st(s):
        n = s.u('I'); v = s.b[s.p:s.p+n].decode('latin1'); s.p += n; return v
def arr(r):
    n = r.u('H'); r.u('i'); return [node(r) for _ in range(n)]
def node(r):
    t = r.u('I')
    if t == 0: return r.u('i')
    if t == 1: return r.u('f')
    if t in (0x10, 0x11, 0x13): return (t, arr(r))
    if t == 0x12: return '"%s"' % r.st()
    if t == 2: return '$' + r.st()
    if t == 6: r.u('i'); return 'kDataUnhandled'
    if t in (8, 9): r.u('i'); return {8: '#else', 9: '#endif'}[t]
    if t == 0x24: return ('#autorun', arr(r))
    if t == 0x14: n = r.u('I'); r.p += n; return '<glob %d>' % n
    s = r.st()
    return {5: s, 7: '#ifdef ' + s, 0x20: '#define ' + s, 0x21: '#include ' + s, 0x22: '#merge ' + s, 0x23: '#ifndef ' + s, 0x24: '#else', 0x25: '#undef ' + s}.get(t, '?%x:%s' % (t, s))
def fmt(n, ind=0):
    if isinstance(n, tuple) and n[0] == '#autorun': return '#autorun ' + fmt((0x11, n[1]), ind)
    if isinstance(n, tuple):
        o, c = {0x10: '()', 0x11: '{}', 0x13: '[]'}[n[0]]
        parts = [fmt(x, ind + 1) for x in n[1]]
        one = o + ' '.join(parts) + c
        if len(one) < 100 and '\n' not in one: return one
        return o + ('\n' + '  ' * (ind + 1)).join(parts) + c
    if isinstance(n, float): return '%g' % n
    return str(n)
d = open(sys.argv[1], 'rb').read()
for raw in (decrypt(d), d):
    try:
        r = R(raw); assert r.u('B') == 1
        for x in arr(r): print(fmt(x))
        break
    except Exception as e:
        err = e
else:
    sys.exit('decode failed: %r' % err)
