from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
import struct, re, os, json

BAND = BANDEXE
data = open(BAND, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt = struct.unpack_from('<H', data, pe + 20)[0]
BASE = 0x82000000
secs = []
off = pe + 24 + opt
for i in range(nsec):
    e = data[off + 40 * i: off + 40 * i + 40]
    name = e[:8].rstrip(b'\0').decode()
    vs, va, rs, pr = struct.unpack_from('<IIII', e, 8)
    secs.append((name, BASE + va, vs, pr, rs))

SEC = {s[0]: s for s in secs}
TEXT_LO = SEC['.text'][1]
TEXT_HI = SEC['.text'][1] + SEC['.text'][2]


def sec_of(va):
    for name, sva, vs, pr, rs in secs:
        if sva <= va < sva + vs:
            return name
    return None


def off_of(va):
    for name, sva, vs, pr, rs in secs:
        if sva <= va < sva + vs:
            return pr + (va - sva)
    return None


def read(va, n):
    o = off_of(va)
    if o is None:
        return None
    return data[o:o + n]


def u32(va):
    b = read(va, 4)
    if b is None or len(b) < 4:
        return None
    return struct.unpack('>I', b)[0]


def is_text(v):
    return v is not None and TEXT_LO <= v < TEXT_HI


# ---------- .pdata function boundaries ----------
_pd = SEC['.pdata']
_pdoff, _pdsize = _pd[3], _pd[2]
FUNCS = []  # (start_va, length)
for i in range(_pdsize // 8):
    a, w = struct.unpack_from('>II', data, _pdoff + 8 * i)
    if a == 0:
        continue
    ln = ((w >> 8) & 0x3FFFFF) * 4
    FUNCS.append((a, ln))
FUNCS.sort()
FSTARTS = [f[0] for f in FUNCS]


def func_of(va):
    """Return (start,len) of the pdata RUNTIME_FUNCTION containing va, else None."""
    import bisect
    i = bisect.bisect_right(FSTARTS, va) - 1
    if i < 0:
        return None
    s, l = FUNCS[i]
    if s <= va < s + l:
        return (s, l)
    return None
