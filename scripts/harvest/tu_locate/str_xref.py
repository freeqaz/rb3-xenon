from _paths import SCRATCH, REPO, BANDEXE  # noqa: E402
"""Global string-xref index over band.exe .text.

Emits edges (code_va, string_va) via lis/addi|ori|lwz high/low pair reconstruction.
Pure-python fixed-width PPC decode (no capstone) for speed.
"""
import struct, sys, json, re, os, array

DATA = open(BANDEXE, 'rb').read()
BASE = 0x82000000
pe = struct.unpack_from('<I', DATA, 0x3c)[0]
nsec = struct.unpack_from('<H', DATA, pe + 6)[0]
optsz = struct.unpack_from('<H', DATA, pe + 20)[0]
SECS = []
off = pe + 24 + optsz
for i in range(nsec):
    e = DATA[off + 40 * i: off + 40 * i + 40]
    name = e[:8].rstrip(b'\0').decode()
    vs, va, rs, pr = struct.unpack_from('<IIII', e, 8)
    SECS.append((name, BASE + va, vs, pr, rs))


def sec(name):
    for s in SECS:
        if s[0] == name:
            return s
    return None


def read(va, n):
    for name, sva, vs, pr, rs in SECS:
        if sva <= va < sva + vs:
            o = pr + (va - sva)
            return DATA[o:o + n]
    return None


# ---------------- string pool ----------------
PRINT = re.compile(rb'[\x20-\x7e]{4,}')


def build_strings(minlen=4):
    """VA -> str for every NUL-terminated printable run in data sections.

    Every suffix position of a run is indexed too, because a printable run can
    begin earlier than the real literal (preceding non-string bytes may happen
    to be printable), which would otherwise shift the key.
    """
    tab = {}
    for name, sva, vs, pr, rs in SECS:
        if name not in ('.rdata', '.data'):
            continue
        blob = DATA[pr:pr + min(vs, rs if rs else vs)]
        for m in PRINT.finditer(blob):
            s = m.group(0)
            end = m.end()
            if end < len(blob) and blob[end] != 0:
                continue
            txt = s.decode('ascii')
            st = sva + m.start()
            n = len(txt)
            for k in range(0, n - minlen + 1):
                tab[st + k] = txt[k:]
    return tab


# ---------------- pdata ----------------
def build_pdata():
    p = sec('.pdata')
    _, sva, vs, pr, rs = p
    n = vs // 8
    fns = []
    for i in range(n):
        a, w1 = struct.unpack_from('>II', DATA, pr + 8 * i)
        if a == 0:
            continue
        ln = ((w1 >> 8) & 0x3FFFFF) * 4
        if ln == 0:
            continue
        fns.append((a, a + ln))
    fns.sort()
    return fns


# ---------------- decode ----------------
def sx16(v):
    return v - 0x10000 if v & 0x8000 else v


LOAD_OPS = {32, 33, 34, 35, 40, 41, 42, 43, 48, 49, 50, 51, 46, 47}  # lwz lbz lhz lha lfs lfd lmw
STORE_OPS = {36, 37, 38, 39, 44, 45, 52, 53, 54, 55, 47}
VOLATILE = set(range(0, 13))


def scan_text(lo, hi, strtab, progress=False):
    tsec = sec('.text')
    _, sva, vs, pr, rs = tsec
    edges = []
    o0 = pr + (lo - sva)
    nins = (hi - lo) // 4
    words = array.array('I')
    words.frombytes(DATA[o0:o0 + nins * 4])
    words.byteswap()  # file is BE, host LE
    hireg = {}
    get = strtab.get
    for i in range(nins):
        w = words[i]
        op = w >> 26
        if op == 15:  # addis
            rD = (w >> 21) & 31
            rA = (w >> 16) & 31
            if rA == 0:
                hireg[rD] = (w & 0xFFFF) << 16
            else:
                hireg.pop(rD, None)
            continue
        if op == 14:  # addi
            rD = (w >> 21) & 31
            rA = (w >> 16) & 31
            if rA in hireg and rA != 0:
                va = (hireg[rA] + sx16(w & 0xFFFF)) & 0xFFFFFFFF
                s = get(va)
                if s is not None:
                    edges.append((lo + 4 * i, va))
            hireg.pop(rD, None)
            continue
        if op == 24:  # ori rA, rS, uimm
            rS = (w >> 21) & 31
            rA = (w >> 16) & 31
            if rS in hireg:
                va = (hireg[rS] | (w & 0xFFFF)) & 0xFFFFFFFF
                s = get(va)
                if s is not None:
                    edges.append((lo + 4 * i, va))
                if rA != rS:
                    hireg.pop(rA, None)
            else:
                hireg.pop(rA, None)
            continue
        if op in LOAD_OPS:
            rD = (w >> 21) & 31
            rA = (w >> 16) & 31
            if rA in hireg and rA != 0:
                va = (hireg[rA] + sx16(w & 0xFFFF)) & 0xFFFFFFFF
                s = get(va)
                if s is not None:
                    edges.append((lo + 4 * i, va))
            if op < 48:
                hireg.pop(rD, None)
            continue
        if op in STORE_OPS:
            rA = (w >> 16) & 31
            if rA in hireg and rA != 0:
                va = (hireg[rA] + sx16(w & 0xFFFF)) & 0xFFFFFFFF
                s = get(va)
                if s is not None:
                    edges.append((lo + 4 * i, va))
            continue
        if op == 18:  # b / bl
            if w & 1:  # bl -> volatiles clobbered
                for r in VOLATILE:
                    hireg.pop(r, None)
            else:
                hireg.clear()
            continue
        if op == 16 or op == 19:  # bc / bclr,bcctr
            if op == 19 and (w & 1):
                for r in VOLATILE:
                    hireg.pop(r, None)
            continue
        if op in (7, 8, 12, 13, 28, 29):
            rD = (w >> 21) & 31
            rA = (w >> 16) & 31
            if op in (28, 29):
                hireg.pop(rA, None)
            else:
                hireg.pop(rD, None)
            continue
        if op == 25 or op == 26 or op == 27:  # oris xori xoris
            hireg.pop((w >> 16) & 31, None)
            continue
        if op == 31:
            xo = (w >> 1) & 0x3FF
            if xo in (151, 183, 215, 247, 407, 439, 663, 727, 662, 918, 214):
                continue  # stores: no GPR dest
            hireg.pop((w >> 21) & 31, None)
            continue
        if op == 21 or op == 20 or op == 23 or op == 30:  # rlwinm rlwimi rlwnm rld*
            hireg.pop((w >> 16) & 31, None)
            continue
    return edges


def main():
    strtab = build_strings()
    print(f'strings: {len(strtab)}', file=sys.stderr)
    lo, hi = 0x82270000, 0x82C4CE3C
    edges = scan_text(lo, hi, strtab)
    print(f'edges: {len(edges)}', file=sys.stderr)
    # forward: code_va -> str; reverse: str -> [code_va]
    rev = {}
    for cva, sva in edges:
        rev.setdefault(strtab[sva], []).append(cva)
    out = {
        'nedges': len(edges),
        'rev': {k: sorted(set(v)) for k, v in rev.items()},
    }
    with open(SCRATCH+'/xref.json', 'w') as f:
        json.dump(out, f)
    print(f'distinct strings referenced: {len(rev)}', file=sys.stderr)


if __name__ == '__main__':
    main()
