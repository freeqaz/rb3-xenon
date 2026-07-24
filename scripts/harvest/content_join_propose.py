#!/usr/bin/env python3
"""Propose target_symbol_map entries by joining a dtk TARGET obj to our compiled
BASE obj on (reloc-masked byte identity) AND (referenced-string identity).

Why the string join matters: retail is full of ICF-identical boilerplate
families (DECLARE_MESSAGE's Name/StaticByteCode/NewNetMessage, enum-to-Symbol
converters, ...).  Reloc-masked byte comparison alone returns MULTI for all of
them, because after masking the relocation sites the bodies really are
identical.  What still distinguishes them is the *content* they reference: the
class-name string literal, the DataGetMacro name, and so on.  Joining on that
collapses most MULTI groups to UNIQUE.

Two content sources are read for each target-side relocation:
  * the C string at the referenced VA, and
  * the C string at VA+0x20 -- in this binary's .rdata pool a vftable is
    consistently followed by its class-name string at that delta.
Both rely on band.exe's .rdata mapping VA == 0x82000000 + file offset.

Landed +30 of laneA's +49 mechanically (GameMode / Game / Defines / GamePanel,
commit f7d609a2).  Complementary to scripts/harvest/homing_scan.py: that one
searches the WHOLE binary's .pdata inventory for a home; this one disambiguates
*within* an already-pinned unit.

Usage:
    content_join_propose.py <target.obj> <base.obj> [--root <repo>]

  target.obj  build/45410914/obj/<Unit>.obj        (dtk split of retail)
  base.obj    build/45410914/src/<path>/<Unit>.obj (ours)

Output lines are UNIQUE (safe to map) or MULTI(n) (do NOT map -- guessing an
ICF twin creates a mispair, which is worse than leaving it unmapped).
"""
import struct, sys, re, json
from pathlib import Path

STR_MIN, STR_MAX = 2, 60
VFTABLE_NAME_DELTA = 0x20   # .rdata: vftable at V -> class-name string at V+0x20
RDATA_VA_BASE = 0x82000000  # band.exe .rdata: VA == base + file offset


def parse_args(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    root = Path(__file__).resolve().parents[2]
    args = list(argv[1:])
    if '--root' in args:
        i = args.index('--root')
        root = Path(args[i + 1])
        del args[i:i + 2]
    return args[0], args[1], root


def cstr(band, va, n=96):
    """C string at a band.exe VA, or None if it isn't printable ASCII."""
    off = va - RDATA_VA_BASE
    if off < 0 or off + n > len(band):
        return None
    raw = band[off:off + n].split(b'\0')[0]
    try:
        s = raw.decode('ascii')
    except UnicodeDecodeError:
        return None
    if STR_MIN <= len(s) <= STR_MAX and all(32 <= ord(c) < 127 for c in s):
        return s
    return None


def parse_obj(path):
    d = Path(path).read_bytes()
    nsec = struct.unpack_from('<H', d, 2)[0]
    symoff = struct.unpack_from('<I', d, 8)[0]
    nsym = struct.unpack_from('<I', d, 12)[0]
    optsz = struct.unpack_from('<H', d, 16)[0]
    strs = symoff + nsym * 18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i * 40
        rawsz, praw, prel = struct.unpack_from('<III', d, o + 16)
        nrel = struct.unpack_from('<H', d, o + 32)[0]
        chars = struct.unpack_from('<I', d, o + 36)[0]
        rel = [struct.unpack_from('<IIH', d, prel + j * 10) for j in range(nrel)]
        secs[i + 1] = dict(sz=rawsz, praw=praw, rel=rel, chars=chars)
    syms = []
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18: symoff + i * 18 + 18]
        nm = rec[:8]
        if nm[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', nm, 4)[0]
            e = d.index(b'\0', strs + off)
            n = d[strs + off:e].decode('latin1')
        else:
            n = nm.rstrip(b'\0').decode('latin1')
        val, sec, typ, cls, naux = struct.unpack_from('<IhHBB', rec, 8)
        syms.append((i, n, sec, val, cls))
        i += 1 + naux
    return d, secs, syms


def fns(secs, syms, drop_special):
    """Symbol -> (section, offset, size). Size runs to the next symbol in the
    same section, which is how COMDAT-per-function objs lay out."""
    bysec = {}
    for i, n, sec, val, cls in syms:
        if cls in (2, 3) and sec > 0 and sec in secs and (secs[sec]['chars'] & 0x20):
            bysec.setdefault(sec, []).append((val, n))
    out = {}
    for sec, lst in bysec.items():
        lst.sort()
        vals = sorted({v for v, _ in lst})
        for val, n in lst:
            if drop_special and (n.startswith('__') or n.startswith('$') or n.startswith('.')):
                continue
            nv = [v for v in vals if v > val]
            end = nv[0] if nv else secs[sec]['sz']
            out.setdefault(n, (sec, val, end - val))
    return out


def body(d, secs, sec, val, sz):
    """Function bytes with every relocation site zeroed on both sides."""
    s = secs[sec]
    b = bytearray(d[s['praw'] + val: s['praw'] + val + sz])
    for rva, _, _ in s['rel']:
        o = rva - val
        if 0 <= o < sz:
            for k in range(4):
                if o + k < sz:
                    b[o + k] = 0
    return bytes(b)


def main():
    target_obj, base_obj, root = parse_args(sys.argv)
    band = Path(root, 'orig/45410914/band.exe').read_bytes()
    tmap = json.load(open(Path(root, 'scripts/target_symbol_map.json')))
    # 264 legacy keys are uppercase "0X..." -- always compare case-insensitively.
    mapped = {k.lower() for k in tmap}

    bd, bsecs, bsyms = parse_obj(base_obj)
    bidx = {i: n for i, n, sec, val, cls in bsyms}
    strdata = {}
    for i, n, sec, val, cls in bsyms:
        if n.startswith('??_C@') and sec > 0 and sec in bsecs:
            s = bsecs[sec]
            strdata[n] = bd[s['praw'] + val: s['praw'] + s['sz']].split(b'\0')[0].decode('latin1')
    bf = fns(bsecs, bsyms, True)
    bstr = {}
    for n, (sec, val, sz) in bf.items():
        refs = set()
        for rva, symi, ty in bsecs[sec]['rel']:
            if val <= rva < val + sz:
                tn = bidx.get(symi, '')
                if tn in strdata:
                    refs.add(strdata[tn])
        bstr[n] = refs

    td, tsecs, tsyms = parse_obj(target_obj)
    tidx = {i: n for i, n, sec, val, cls in tsyms}
    tf = fns(tsecs, tsyms, False)

    props = []
    for tn, (sec, val, sz) in sorted(tf.items()):
        m = re.match(r'fn_([0-9A-Fa-f]{8})$', tn)
        if not m or sz < 8:
            continue
        va = int(m.group(1), 16)
        if ('0x%08x' % va) in mapped:
            continue
        tstr = set()
        for rva, symi, ty in tsecs[sec]['rel']:
            if val <= rva < val + sz:
                rn = tidx.get(symi, '')
                mm = re.match(r'(?:lbl|fn)_([0-9A-Fa-f]{8})$', rn)
                if mm:
                    v = int(mm.group(1), 16)
                    for cand in (cstr(band, v), cstr(band, v + VFTABLE_NAME_DELTA)):
                        if cand:
                            tstr.add(cand)
        tb = body(td, tsecs, sec, val, sz)
        hits = [bn for bn, (bsec, bval, bsz) in bf.items()
                if bsz == sz and body(bd, bsecs, bsec, bval, bsz) == tb]
        if not hits:
            continue
        if tstr:
            filtered = [h for h in hits if bstr.get(h) and bstr[h] & tstr]
            if filtered:
                hits = filtered
        props.append((va, sz, sorted(tstr), hits))

    for va, sz, ts, hits in props:
        tag = 'UNIQUE' if len(hits) == 1 else f'MULTI({len(hits)})'
        print(f"{tag:9s} 0x{va:08x} sz={sz:4d} strs={ts} -> {hits[:4]}")


if __name__ == '__main__':
    main()
