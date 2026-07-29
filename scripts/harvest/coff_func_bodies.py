#!/usr/bin/env python3
"""coff_func_bodies.py -- per-function COFF body + relocation-name extraction.

For a unit, for every function symbol present in BOTH the dtk target obj and our
compiled base obj, extract:
  * the ordered list of relocation TARGET SYMBOL NAMES inside the function body
  * the raw code bytes with reloc fields masked

A "template instantiation" function (e.g. __uninitialized_fill_n<T*>) is
MISPAIRED when the target's callee relocations name a DIFFERENT instantiation
type than ours.  That is unfixable in source: objdiff is comparing our
vector<A> helper against retail's vector<B> helper.

Usage:  relosig.py <unit-obj> <base-obj> [name-substring]
"""
import struct, sys, re

def parse(path):
    d = open(path, 'rb').read()
    machine, nsec, tds, ptr_sym, nsym, opt, chars = struct.unpack_from('<HHIIIHH', d, 0)
    fmt, ent, sh_off = 'coff', 18, 20 + opt
    if machine == 0 and nsec == 0xFFFF:
        machine = struct.unpack_from('<H', d, 6)[0]
        nsec, ptr_sym, nsym = struct.unpack_from('<III', d, 44)
        fmt, ent, sh_off = 'bigobj', 20, 56
    strtab = d[ptr_sym + nsym * ent:]

    def nm(raw):
        if raw[0:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', raw, 4)[0]
            e = strtab.find(b'\0', off)
            return strtab[off:e].decode('latin-1')
        return raw.rstrip(b'\0').decode('latin-1')

    secs = []
    for i in range(nsec):
        o = sh_off + i * 40
        name = d[o:o + 8].rstrip(b'\0').decode('latin-1')
        if name.startswith('/'):
            try:
                so = int(name[1:])
                e = strtab.find(b'\0', so)
                name = strtab[so:e].decode('latin-1')
            except ValueError:
                pass
        vsz, va, rawsz, rawptr, relptr, lnptr, nrel, nln, fl = struct.unpack_from('<IIIIIIHHI', d, o + 8)
        if nrel == 0xFFFF and (fl & 0x01000000):
            # extended relocations: count in first reloc record
            nrel = struct.unpack_from('<I', d, relptr)[0] - 1
            relptr += 10
        secs.append(dict(name=name, rawsz=rawsz, rawptr=rawptr, relptr=relptr, nrel=nrel))

    syms = []
    i = 0
    while i < nsym:
        o = ptr_sym + i * ent
        raw = d[o:o + 8]
        if fmt == 'coff':
            val, secnum, typ, sc, naux = struct.unpack_from('<IhHBB', d, o + 8)
        else:
            val, secnum, typ, sc, naux = struct.unpack_from('<IiHBB', d, o + 8)
        syms.append((nm(raw), val, secnum, typ, sc, i))
        i += 1 + naux
    idx_to_sym = {}
    for s in syms:
        idx_to_sym[s[5]] = s
    return d, secs, syms, idx_to_sym


def func_reloc_names(path):
    """-> {func_name: (size, [reloc target names in address order], code_bytes)}"""
    d, secs, syms, idx = parse(path)
    # map section number -> list of (offset, name) function symbols
    out = {}
    bysec = {}
    for (name, val, secnum, typ, sc, si) in syms:
        if secnum <= 0 or secnum > len(secs):
            continue
        if sc not in (2, 3):  # EXTERNAL / STATIC
            continue
        sec = secs[secnum - 1]
        if not sec['name'].startswith('.text'):
            continue
        bysec.setdefault(secnum, []).append((val, name))
    for secnum, ents in bysec.items():
        sec = secs[secnum - 1]
        ents.sort()
        # collect relocs for this section
        rels = []
        for r in range(sec['nrel']):
            o = sec['relptr'] + r * 10
            va, symidx, typ = struct.unpack_from('<IIH', d, o)
            s = idx.get(symidx)
            rels.append((va, s[0] if s else '?%d' % symidx, typ))
        rels.sort()
        for k, (off, name) in enumerate(ents):
            end = ents[k + 1][0] if k + 1 < len(ents) else sec['rawsz']
            body = d[sec['rawptr'] + off: sec['rawptr'] + end]
            rn = [rn for (va, rn, t) in rels if off <= va < end]
            prev = out.get(name)
            if prev is None or len(body) > len(prev[0]):
                out[name] = (body, rn)
    return out


TPL = re.compile(r'@(?:\?\$)?')

def targ(sym):
    """crude template-argument signature: everything after the first '@?$' group"""
    return sym


def main():
    tobj, bobj = sys.argv[1], sys.argv[2]
    filt = sys.argv[3] if len(sys.argv) > 3 else ''
    T = func_reloc_names(tobj)
    B = func_reloc_names(bobj)
    for name in sorted(set(T) & set(B)):
        if filt and filt not in name:
            continue
        tb, tr = T[name]
        bb, br = B[name]
        if tr == br:
            continue
        pairs = [(a, b) for a, b in zip(tr, br) if a != b]
        if not pairs:
            continue
        print('=' * 100)
        print(name)
        print(f'  tgt {len(tb)}B {len(tr)} relocs | base {len(bb)}B {len(br)} relocs')
        for a, b in pairs:
            print(f'  TGT  {a}')
            print(f'  BASE {b}')


if __name__ == '__main__':
    main()
