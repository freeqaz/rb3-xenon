"""Enumerate MSVC RTTI in the RB3-360 retail PE: .?AV<Class>@@ TypeDescriptor
-> ??_R4 Complete Object Locator -> vtable (COL sits at vtable-4).
Dumps every resolved vtable's slots to vtables.json.
"""
import struct, re, json, sys, collections
from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
from vt_pe import data, secs, SEC, BASE, u32, read, is_text, sec_of, TEXT_LO, TEXT_HI

# ---- 1. type descriptor names ----
# TypeDescriptor { void* pVFTable; void* spare; char name[]; }  -> name at TD+8
tds = {}  # td_va -> mangled name
for name, sva, vs, pr, rs in secs:
    if name not in ('.data', '.rdata'):
        continue
    blob = data[pr:pr + min(vs, rs)]
    for m in re.finditer(rb'\.\?A[VU][^\x00]{1,250}\x00', blob):
        nm_off = m.start()
        td_va = sva + nm_off - 8
        s = m.group()[:-1].decode('ascii', 'replace')
        tds[td_va] = s

print(f'type descriptors: {len(tds)}', file=sys.stderr)

# ---- 2. index every aligned BE dword in data sections ----
idx = collections.defaultdict(list)
for name, sva, vs, pr, rs in secs:
    if name not in ('.data', '.rdata'):
        continue
    n = min(vs, rs) & ~3
    vals = struct.unpack_from('>%dI' % (n // 4), data, pr)
    for i, v in enumerate(vals):
        if v:
            idx[v].append(sva + 4 * i)
print(f'indexed dwords: {len(idx)}', file=sys.stderr)

# ---- 3. COL: sig(0) off(4) cdoff(8) pTD(12) pCHD(16) ----
cols = {}  # col_va -> (td_va, offset, cdoff, chd)
for td_va, nm in tds.items():
    for p in idx.get(td_va, ()):
        col = p - 12
        sig = u32(col)
        if sig != 0:
            continue
        chd = u32(col + 16)
        if chd is None or sec_of(chd) not in ('.data', '.rdata'):
            continue
        cols[col] = (td_va, u32(col + 4), u32(col + 8), chd)
print(f'COLs: {len(cols)}', file=sys.stderr)

# ---- 4. vtables: pointer to COL, vtable starts at ptr+4 ----
out = {}
for col, (td_va, offs, cdoff, chd) in cols.items():
    nm = tds[td_va]
    cls = nm[4:-2]  # strip .?AV / @@
    for p in idx.get(col, ()):
        vt = p + 4
        slots = []
        v = vt
        while True:
            x = u32(v)
            if not is_text(x):
                break
            slots.append(x)
            v += 4
            if len(slots) > 400:
                break
        if not slots:
            continue
        out.setdefault(cls, []).append({
            'td': td_va, 'col': col, 'vt': vt, 'offset': offs,
            'cdoff': cdoff, 'chd': chd, 'slots': slots,
        })

print(f'classes with >=1 vtable: {len(out)}', file=sys.stderr)
json.dump(out, open(SCRATCH+'/vtables.json', 'w'))
json.dump({hex(k): v for k, v in tds.items()},
          open(SCRATCH+'/typedescs.json', 'w'))
