#!/usr/bin/env python3
"""carve_extent_scan.py [repo] : find symbols.txt functions whose retail control flow
runs past the end dtk carved for them (lane W16-KE, 2026-10-01).

Reads only retail bytes (orig/45410914/band.exe), config/45410914/symbols.txt,
scripts/target_symbol_map.json and build/45410914/report.json, and prints one
line per function whose reached extent differs from its carved size, with the
anonymous fragments the reached extent would absorb. --base adds our compiled
size for the head's map name (from build/45410914/src/**/*.obj), which is the
corroboration W16-KE used to choose what to fix: a head is only taken when our
independently compiled function is exactly the reached extent.

For every .text function symbol S, walk retail code from S.start following
fall-through and non-call branches (b/bc, and byte/half jump tables behind a
bctr). A branch target is INTERNAL when it lies in [S.start, limit), where
limit = next .pdata BeginAddress after S.start, and the target is not a pdata
start, has no reference sourced outside the walked span, and is not the start
of a function the map names that is reached from elsewhere. b to anything else
is a tail call. Reached extent = max reached instruction + 4, plus one trailing
unreferenced `blr` directly after a terminator (MSVC's dead epilogue after a
tail call). Report every S whose reached extent is larger than its carved extent.
"""
import sys, os, re, json, struct, bisect, glob
WT = next((a for a in sys.argv[1:] if not a.startswith('--')), '.')
BASE = '--base' in sys.argv
data = open(os.path.join(WT, 'orig/45410914/band.exe'), 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe+6)[0]
opt = struct.unpack_from('<H', data, pe+20)[0]
base = struct.unpack_from('<I', data, pe+24+28)[0]
SEC = []
for i in range(nsec):
    o = pe+24+opt+i*40
    nm = data[o:o+8].rstrip(b'\0').decode()
    vs, va, rs, rp = struct.unpack_from('<IIII', data, o+8)
    SEC.append((nm, base+va, vs, rp, rs))
TEXT = [s for s in SEC if s[0] == '.text'][0]
def off(v):
    for nm, sva, vs, rp, rs in SEC:
        if sva <= v < sva+min(vs, rs): return rp+(v-sva)
def w32(v):
    o = off(v); return struct.unpack_from('>I', data, o)[0] if o is not None else None
def u8(v): return data[off(v)]
def u16(v): return struct.unpack_from('>H', data, off(v))[0]
pd = [s for s in SEC if s[0] == '.pdata'][0]
PDS = set(); PDL = []
for k in range(pd[2]//8):
    b, f = struct.unpack_from('>II', data, pd[3]+k*8)
    if b: PDS.add(b); PDL.append(b)
PDL.sort()
# Every code branch and every data word that targets an address in the image.
X = {}; D = {}
for nm, sva, vs, rp, rs in SEC:
    n = min(vs, rs)
    if nm == '.text':
        for k in range(0, n, 4):
            w = struct.unpack_from('>I', data, rp+k)[0]
            a = sva+k; op = w >> 26; t = None
            if op == 18:
                li = w & 0x03FFFFFC
                if li & 0x02000000: li -= 0x04000000
                t = (0 if w & 2 else a)+li; kind = 'bl' if w & 1 else 'b'
            elif op == 16:
                bd = w & 0xFFFC
                if bd & 0x8000: bd -= 0x10000
                t = a+bd; kind = 'bc'
            if t is not None: X.setdefault(t & 0xffffffff, []).append((a, kind))
    else:
        for k in range(0, n-3, 4):
            w = struct.unpack_from('>I', data, rp+k)[0]
            if 0x82000000 <= w < 0x83000000:
                D.setdefault(w, []).append((nm, sva+k))
SYM = []
for line in open(os.path.join(WT, 'config/45410914/symbols.txt')):
    m = re.match(r'(\S+) = \.text:0x([0-9A-Fa-f]+); // type:(\w+)(?: size:0x([0-9A-Fa-f]+))?', line)
    if m and m[3] in ('function', 'object'):
        SYM.append((int(m[2], 16), int(m[4], 16) if m[4] else 0, m[1], m[3]))
SYM.sort()
SB = [s[0] for s in SYM]
FUNCSTART = {s[0] for s in SYM if s[3] == 'function'}
OBJSTART = {s[0]: s for s in SYM if s[3] == 'object'}
TM = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))
A2N = {int(k, 16): v for k, v in TM.items() if k.lower().startswith('0x')}

def btarget(a, w):
    op = w >> 26
    if op == 18:
        li = w & 0x03FFFFFC
        if li & 0x02000000: li -= 0x04000000
        return ((0 if w & 2 else a)+li) & 0xffffffff, ('bl' if w & 1 else 'b')
    if op == 16:
        bd = w & 0xFFFC
        if bd & 0x8000: bd -= 0x10000
        return (a+bd) & 0xffffffff, ('bcl' if w & 1 else 'bc')
    return None, None

def jumptable(a):
    """bctr at a: decode MSVC `lis/addi; lbzx|lhzx; lis/addi; add; mtctr; bctr`."""
    ins = [(a-4*k, w32(a-4*k)) for k in range(1, 16)]
    regs = {}
    tbl = basev = None; kind = None; count = None
    seq = list(reversed(ins))
    # find table load
    for (x, w) in seq:
        op = w >> 26
        if op == 31 and ((w >> 1) & 0x3ff) in (87, 279):  # lbzx / lhzx
            kind = 1 if ((w >> 1) & 0x3ff) == 87 else 2
    vals = {}
    tl = None
    scale = 1; ldreg = None
    for (x, w) in seq:
        op = w >> 26; rd = (w >> 21) & 31; ra = (w >> 16) & 31; imm = w & 0xffff
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 15 and ra == 0: vals[rd] = (simm << 16) & 0xffffffff
        elif op == 14 and ra in vals: vals[rd] = (vals[ra]+simm) & 0xffffffff
        elif op == 31 and ((w >> 1) & 0x3ff) in (87, 279):
            tl = vals.get(ra); tbl = tl
            vals.pop(rd, None); ldreg = rd
        elif op == 21 and tbl is not None and ((w >> 21) & 31) == ldreg:
            # rlwinm rA,rS,sh,... on the loaded offset: scale
            sh = (w >> 11) & 31; scale = 1 << sh; ldreg = ra
        elif op == 31 and ((w >> 1) & 0x3ff) == 266:  # add
            if tbl is not None:
                basev = vals.get(ra) if ra in vals else vals.get((w >> 11) & 31)
    if tbl is None or basev is None: return None
    # count from a preceding cmplwi crX,rY,N
    for k in range(1, 40):
        w = w32(a-4*k)
        if w is None: break
        if (w >> 26) == 10:  # cmpli
            count = (w & 0xffff)+1; break
    if count is None or count > 512: return None
    out = []
    for i in range(count):
        o = u8(tbl+i) if kind == 1 else u16(tbl+2*i)
        out.append((basev+o*scale) & 0xffffffff)
    return out

def walk(s, limit):
    seen = set(); todo = [s]; jt = False
    while todo:
        a = todo.pop()
        while s <= a < limit and a not in seen:
            w = w32(a)
            if w is None or w == 0: break
            seen.add(a)
            t, k = btarget(a, w)
            op = w >> 26
            if k in ('b',):
                if s <= t < limit and t not in PDS: todo.append(t)
                break
            if k in ('bc',):
                bo = (w >> 21) & 31
                if s <= t < limit and t not in PDS: todo.append(t)
                if bo & 0x14 == 0x14: break  # branch always
                a += 4; continue
            if op == 19:
                xo = (w >> 1) & 0x3ff; bo = (w >> 21) & 31
                if xo in (16, 528) and not (w & 1):  # bclr / bcctr
                    if xo == 528 and bo & 0x14 == 0x14:
                        tg = jumptable(a)
                        if tg:
                            jt = True
                            for t in tg:
                                if s <= t < limit: todo.append(t)
                    if bo & 0x14 == 0x14: break
            a += 4
    return seen, jt

R = json.load(open(os.path.join(WT, 'build/45410914/report.json')))
N2A = {v: k for k, v in A2N.items() if isinstance(v, str)}
ROW = {}
for u in R['units']:
    for f in u.get('functions', []):
        n = f['name']; m = re.match(r'fn_([0-9A-F]{8})$', n)
        a = int(m[1], 16) if m else N2A.get(n)
        if a is not None: ROW[a] = (u['name'][8:], n, float(f.get('fuzzy_match_percent', 0)), int(f['size']))

out = []
for i, (s, size, name, typ) in enumerate(SYM):
    if typ != 'function' or not (TEXT[1] <= s < TEXT[1]+TEXT[2]): continue
    j = bisect.bisect_right(PDL, s)
    limit = PDL[j] if j < len(PDL) else s+0x10000
    # never walk into the next except_data prefix
    for (oa, osz, on, ot) in SYM[i+1:i+40]:
        if ot == 'object' and on.startswith('except_data') and oa < limit:
            limit = oa; break
    seen, jt = walk(s, limit)
    if not seen: continue
    # the walk must not swallow another function that has external refs
    ext = max(seen)+4
    nxt = [x for x in SB[i+1:i+60] if s < x < ext and x in FUNCSTART]
    bad = []
    for x in nxt:
        refs = [src for src, k in X.get(x, []) if not (s <= src < ext)]
        if refs or D.get(x) or x in PDS: bad.append(x)
    if bad:
        ext = min(bad)
        seen = {a for a in seen if a < ext}
        if not seen: continue
        ext = max(seen)+4
    # trailing dead blr after a terminator
    if w32(ext) == 0x4E800020 and not X.get(ext) and not D.get(ext) and ext not in PDS and ext < limit:
        ext += 4
    # Only growth is reported. A reached extent SHORTER than the carve is, in
    # every case inspected, the 4-byte zero alignment word that dtk folds into
    # the preceding symbol (306 such rows at W16-KE's tip), not a defect.
    if ext > s+size:
        absorbed = [x for x in SB[i+1:i+60] if s < x < ext]
        out.append((s, size, ext-s, name, A2N.get(s), jt, absorbed))

BS = {}
if BASE:
    sys.path.insert(0, os.path.join(WT, 'tools', 'extent_census'))
    import coffx
    for p in glob.glob(os.path.join(WT, 'build/45410914/src/**/*.obj'), recursive=True):
        try:
            secs, syms = coffx.parse(open(p, 'rb').read()); coffx.infer_sizes(secs, syms)
        except Exception:
            continue
        for sy in syms:
            if sy.kind == 'function' and sy.size: BS.setdefault(sy.name, set()).add(sy.size)
for s, size, new, name, mp, jt, ab in out:
    r = ROW.get(s)
    b = ''
    if BASE and isinstance(mp, str):
        sizes = BS.get(mp, set())
        b = ' base=MATCH' if new in sizes else (' base=' + ','.join(hex(x) for x in sorted(sizes)) if sizes else ' base=none')
    print(f'{s:#010x} carved={size:#x} cfg={new:#x} jt={int(jt)}{b} map={str(mp)[:60]} row={r} absorbed={[hex(x) for x in ab]}')
print('TOTAL', len(out), file=sys.stderr)
