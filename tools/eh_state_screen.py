#!/usr/bin/env python3
"""Compare MSVC C++ EH state counts (FuncInfo.maxState) between RETAIL and OUR build.

Mechanism-level screen for the "surplus EH state" defect class (lane W12-A):
when our compiler cannot prove a callee nothrow but retail's could, we emit an
extra EH state.  Symptoms vary (extra frame slots, displaced temps, a virtual
base flag store), so screening on any single symptom under-counts.  maxState is
the mechanism itself.

RETAIL side: every EH-bearing function carries an 8-byte prefix
    DCD __CxxFrameHandler ; DCD __ehfuncinfo$<fn>
which dtk renders in the split .s as an `.obj except_data_*` immediately before
the `.fn`.  FuncInfo lives in .rdata of orig/45410914/band.exe (big-endian):
    +00 magic 0x19930522, +04 maxState, +08 pUnwindMap, ...

OUR side: each compiled .obj carries a COFF symbol `__ehfuncinfo$<mangled>`;
its section data at +4 is maxState (section data is big-endian PPC).

Join is by MSVC mangled name via scripts/target_symbol_map.json.
"""
import json, os, re, struct, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------- retail image ----------
class Image:
    def __init__(self, path):
        d = open(path, 'rb').read(); self.d = d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        self.secs = []
        off = pe + 24 + optsz
        for _ in range(nsec):
            vsz, va, rsz, ra = struct.unpack_from('<IIII', d, off + 8)
            self.secs.append((va, vsz, ra, rsz)); off += 40
    def v2f(self, v):
        r = v - self.base
        for va, vsz, ra, rsz in self.secs:
            if va <= r < va + max(vsz, rsz):
                return ra + (r - va)
        return None
    def be32(self, v):
        f = self.v2f(v)
        if f is None or f + 4 > len(self.d): return None
        return struct.unpack_from('>I', self.d, f)[0]

def retail_maxstates(img, asmdir):
    """fn_<ADDR> -> maxState, from the split .s EH prefixes."""
    out = {}
    pend = None
    rx_obj = re.compile(r'^\.obj\s+(except_data_[0-9A-Fa-f]+)')
    rx_4b  = re.compile(r'^\s*\.4byte\s+(0x[0-9A-Fa-f]+)')
    rx_fn  = re.compile(r'^\.fn\s+(fn_[0-9A-Fa-f]+)')
    for p in sorted(glob.glob(os.path.join(asmdir, '*.s'))):
        words, inobj = [], False
        for line in open(p, errors='replace'):
            if rx_obj.match(line):
                inobj, words = True, []; continue
            if inobj:
                m = rx_4b.match(line)
                if m: words.append(int(m.group(1), 16)); continue
                if line.startswith('.endobj'):
                    inobj = False
                    pend = words[1] if len(words) == 2 else None
                    continue
            m = rx_fn.match(line)
            if m:
                if pend is not None and img.be32(pend) == 0x19930522:
                    out[m.group(1)] = img.be32(pend + 4)
                pend = None
            elif line.startswith('.fn ') or line.startswith('.endfn'):
                pend = None
    return out

# ---------- our objects ----------
def coff_ehfuncinfo(path):
    """mangled -> maxState for one compiled .obj."""
    d = open(path, 'rb').read()
    if len(d) < 20: return {}
    nsec = struct.unpack_from('<H', d, 2)[0]
    symptr, nsym = struct.unpack_from('<II', d, 8)
    secs = []
    off = 20
    for _ in range(nsec):
        raw = struct.unpack_from('<I', d, off + 20)[0]
        secs.append(raw); off += 40
    strtab = symptr + nsym * 18
    res = {}
    for i in range(nsym):
        o = symptr + i * 18
        if o + 18 > len(d): break
        nm = d[o:o + 8]
        if nm[:4] == b'\0\0\0\0':
            so = struct.unpack_from('<I', d, o + 4)[0]
            e = d.find(b'\0', strtab + so)
            name = d[strtab + so:e].decode('latin1')
        else:
            name = nm.rstrip(b'\0').decode('latin1')
        val, sec = struct.unpack_from('<Ih', d, o + 8)
        naux = d[o + 17]
        if name.startswith('__ehfuncinfo$') and 1 <= sec <= len(secs):
            fo = secs[sec - 1] + val
            if fo + 8 <= len(d):
                magic, ms = struct.unpack_from('>II', d, fo)
                if magic == 0x19930522:
                    res[name[len('__ehfuncinfo$'):]] = ms
        i_skip = naux
    return res

def main():
    img = Image(os.path.join(ROOT, 'orig/45410914/band.exe'))
    retail = retail_maxstates(img, os.path.join(ROOT, 'build/45410914/asm'))
    smap = json.load(open(os.path.join(ROOT, 'scripts/target_symbol_map.json')))
    addr2name = {k.lower(): v for k, v in smap.items() if isinstance(v, str)}
    ours = {}
    for p in glob.glob(os.path.join(ROOT, 'build/45410914/src/**/*.obj'), recursive=True):
        for k, v in coff_ehfuncinfo(p).items():
            ours.setdefault(k, set()).add(v)
    print('retail EH functions (split .s): %d' % len(retail))
    print('our   EH symbols (compiled obj): %d' % len(ours))

    rows = []
    joined = unmapped = 0
    for fn, rms in retail.items():
        addr = '0x' + fn[3:].lower()
        name = addr2name.get(addr)
        if not name:
            unmapped += 1; continue
        o = ours.get(name)
        if not o: continue
        joined += 1
        for oms in sorted(o):
            if oms != rms:
                rows.append((oms - rms, rms, oms, name, fn))
    print('joined (name known on both sides): %d   retail-EH-but-unmapped: %d' % (joined, unmapped))
    surplus = [r for r in rows if r[0] > 0]
    deficit = [r for r in rows if r[0] < 0]
    print('DISAGREE: %d   (ours>retail: %d, ours<retail: %d)' % (len(rows), len(surplus), len(deficit)))
    print()
    print('--- ours > retail (surplus EH state: the W12-A class) ---')
    for d_, rms, oms, name, fn in sorted(surplus, key=lambda r: -r[0])[:60]:
        print('  +%d  retail=%d ours=%d  %s  %s' % (d_, rms, oms, fn, name[:88]))
    print()
    print('--- ours < retail (deficit) ---')
    for d_, rms, oms, name, fn in sorted(deficit)[:25]:
        print('  %d  retail=%d ours=%d  %s  %s' % (d_, rms, oms, fn, name[:88]))
    return retail, ours, addr2name

if __name__ == '__main__':
    main()
