#!/usr/bin/env python3
"""xref.py <string>...: retail band.exe code sites that form the address of each NUL-terminated string
(lis rX,hi ; addi rY,rX,lo within 12 insns), with the containing function from .pdata and map name."""
import sys, struct, json, bisect
sys.path.insert(0, '/home/free/code/milohax/rb3-xenon/tools')
import va_disasm as V
data, base, secs = V.load_sections('/home/free/code/milohax/rb3-xenon/orig/45410914/band.exe')
def secva(n): return [s for s in secs if s[0] == n][0]
# pdata: BE pairs (begin, packed)
pd = secva('.pdata'); fns = []
for i in range(0, pd[2] // 8 * 8, 8):
    b, p = struct.unpack_from('>II', data, pd[3] + i)
    if b: fns.append((b, ((p >> 8) & 0x3FFFFF) * 4))
fns.sort(); starts = [f[0] for f in fns]
m = json.load(open('/home/free/code/milohax/rb3-xenon/scripts/target_symbol_map.json'))
names = {}
for k, v in (m.items() if isinstance(m, dict) else []):
    try: names[int(k.replace('fn_', ''), 16)] = v if isinstance(v, str) else v.get('name', str(v))
    except Exception: pass
def fn_of(va):
    i = bisect.bisect_right(starts, va) - 1
    if i >= 0 and va < fns[i][0] + fns[i][1]: return fns[i][0]
    return None
txt = secva('.text')
code = data[txt[3]:txt[3] + txt[4]]
for s in sys.argv[1:]:
    pat = s.encode() + b'\0'; hits = []
    for name, va, vs, rp, rs in secs:
        if name in ('.text', '.pdata'): continue
        blob = data[rp:rp + rs]; o = blob.find(pat)
        while o >= 0:
            hits.append(va + o)
            o = blob.find(pat, o + 1)
    sites = []
    for a in hits:
        hi = ((a + 0x8000) >> 16) & 0xFFFF; lo = a & 0xFFFF
        for i in range(0, len(code) - 4, 4):
            w = struct.unpack_from('>I', code, i)[0]
            if (w >> 26) == 15 and ((w >> 16) & 31) == 0 and (w & 0xFFFF) == hi:
                rx = (w >> 21) & 31
                for j in range(1, 13):
                    if i + 4 * j >= len(code): break
                    w2 = struct.unpack_from('>I', code, i + 4 * j)[0]
                    if (w2 >> 26) == 14 and ((w2 >> 16) & 31) == rx and (w2 & 0xFFFF) == lo:
                        sites.append(txt[1] + i + 4 * j); break
    print('%s: str %s' % (s, ' '.join('%08X' % h for h in hits)))
    for x in sites:
        f = fn_of(x); print('   site %08X in fn %s %s' % (x, ('%08X' % f) if f else '?', names.get(f, '')))
