#!/usr/bin/env python3
"""Compare our compiled COMDAT .text sections against retail bytes, modulo relocations.

WHY THIS EXISTS (lane STL-104, 2026-08-13)
------------------------------------------
Lane NC-REC reported that our STLport ``_Copy_Construct``/``_Param_Construct``
helpers were **104 bytes where retail's are 60** -- a 44-byte excess repeated
across ~299 helpers, briefed as a systematic source divergence.

It is NOT a divergence. It is a units error, and this tool is the instrument
that settles it. A COMDAT ``.text`` section for an EH-bearing function holds
THREE things, only one of which report.json's ``size`` counts:

    +0x00   8 B   EH prefix    -- ptr to __CxxFrameHandler (.text)
                                  + ptr to __ehfuncinfo$<fn> (.rdata)
    +0x08  60 B   the function -- this is report.json's `size`, from .pdata
    +0x44  44 B   __unwind$<n> -- EH cleanup funclet; calls the placement
                                  operator delete(void*,void*) when T's copy
                                  ctor throws. SEPARATE .pdata entry, so it is
                                  a SEPARATE report row and is never in the 60.
    ------ 112 B total section size

So 104 == 112 - 8 == body + funclet, compared against retail's function-only
extent of 60. Retail has all 112 bytes, at the same addresses, byte-identical.

Measured on the class (see the lane's merge message for the full record):
  * 109 of 113 helpers with a known retail address are byte-identical to retail
    across the FULL section -- prefix, body AND funclet -- modulo relocated
    words. The 4 exceptions are ordinary per-T defects, not a 44-byte class.
  * 89 of 90 named retail 60-byte helpers carry the 8-byte prefix at addr-8
    and a 44-byte .pdata entry at addr+60 with the identical 11-word cleanup
    shape. Whole-binary null: only 23.0% of 60-byte functions are followed by
    a 44-byte one, so the stratum is ~4.3x enriched -- structural, not chance.

THE GENERAL RULE: never compare a COMDAT section size against a report.json
``size``. They measure different spans. Use this tool, which compares BYTES and
skips relocated words (whose displacements differ by construction -- see
CLAUDE.md on why raw memcmp of function bodies is silently vacuous).

Usage:
    python3 tools/comdat_retail_verify.py --pattern _Copy_Construct@
    python3 tools/comdat_retail_verify.py --pattern '??0Foo@@' --show-diffs 20
"""
import argparse
import glob
import json
import os
import struct
import sys


def read_coff(path):
    """Return (data, sections, symbols) for a COFF .obj, or None."""
    d = open(path, 'rb').read()
    if len(d) < 20:
        return None
    _mach, nsec, _ts, symptr, nsym, optsz, _ch = struct.unpack_from('<HHIIIHH', d, 0)
    if not symptr or not nsym:
        return None
    secs = []
    off = 20 + optsz
    for _ in range(nsec):
        raw = d[off:off + 40]
        vsz, va, szraw, ptrraw, ptrrel, _ptrln, nrel, _nln, ch = struct.unpack_from('<IIIIIIHHI', raw, 8)
        secs.append(dict(name=raw[:8], size=szraw, ptr=ptrraw, chars=ch,
                         nrel=nrel, ptrrel=ptrrel))
        off += 40
    strtab = d[symptr + 18 * nsym:]
    syms = []
    i = 0
    while i < nsym:
        raw = d[symptr + 18 * i:symptr + 18 * i + 18]
        if raw[:4] == b'\x00\x00\x00\x00':
            stroff = struct.unpack_from('<I', raw, 4)[0]
            name = strtab[stroff:strtab.index(b'\x00', stroff)].decode('latin1')
        else:
            name = raw[:8].rstrip(b'\x00').decode('latin1')
        val, sec, _typ, sclass, naux = struct.unpack_from('<IhHBB', raw, 8)
        syms.append(dict(name=name, val=val, sec=sec, sclass=sclass, idx=i))
        i += 1 + naux
    return d, secs, syms


class Retail:
    """Address-addressable view of the retail PE."""

    def __init__(self, path):
        d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        self.d = d
        self.secs = []
        off = pe + 24 + optsz
        for _ in range(nsec):
            raw = d[off:off + 40]
            name = raw[:8].rstrip(b'\x00').decode()
            vsz, va, _szraw, ptr = struct.unpack_from('<IIII', raw, 8)
            self.secs.append((name, va, vsz, ptr))
            off += 40

    def read(self, addr, n):
        for _nm, va, vsz, ptr in self.secs:
            if self.base + va <= addr < self.base + va + vsz:
                fo = ptr + (addr - (self.base + va))
                return self.d[fo:fo + n]
        return None

    def pdata_lengths(self):
        """addr -> function length in bytes. FunctionLen is 18 bits at bit
        offset 8 of the second BIG-ENDIAN word (getting either wrong inverts
        the verdict -- see CLAUDE.md)."""
        pd = [s for s in self.secs if s[0] == '.pdata'][0]
        raw = self.d[pd[3]:pd[3] + pd[2]]
        out = {}
        for i in range(0, len(raw), 8):
            a, f = struct.unpack_from('>II', raw, i)
            if a:
                out[a] = ((f >> 8) & 0x3FFFF) * 4
        return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default='.', help='worktree to read from')
    ap.add_argument('--pattern', required=True,
                    help='substring matched against mangled symbol names')
    ap.add_argument('--show-diffs', type=int, default=10)
    ap.add_argument('--json', help='write per-symbol rows here')
    args = ap.parse_args()

    root = args.project_dir
    retail = Retail(os.path.join(root, 'orig/45410914/band.exe'))
    lens = retail.pdata_lengths()
    tmap = json.load(open(os.path.join(root, 'scripts/target_symbol_map.json')))
    addr_of = {v: int(k, 16) for k, v in tmap.items() if args.pattern in v}

    ours = {}
    pat = os.path.join(root, 'build/45410914/src/**/*.obj')
    for p in glob.glob(pat, recursive=True):
        r = read_coff(p)
        if not r:
            continue
        dd, secs, syms = r
        for s in syms:
            n = s['name']
            if args.pattern not in n or s['sec'] <= 0 or s['sclass'] != 2:
                continue
            sec = secs[s['sec'] - 1]
            if not sec['name'].startswith(b'.text') or n in ours:
                continue
            rel = {}
            for i in range(sec['nrel']):
                va, _si, ty = struct.unpack_from('<IIH', dd, sec['ptrrel'] + 10 * i)
                rel[va] = ty
            ours[n] = dict(unit=os.path.basename(p), size=sec['size'], rel=rel,
                           body=dd[sec['ptr']:sec['ptr'] + sec['size']],
                           off=s['val'])

    if not ours:
        sys.exit('no COMDAT .text symbols matched %r -- build first?' % args.pattern)

    ident, diff, noaddr, rows = 0, 0, 0, []
    for n, o in sorted(ours.items()):
        a = addr_of.get(n)
        if a is None:
            noaddr += 1
            continue
        rb = retail.read(a - o['off'], o['size'])
        if rb is None or len(rb) < o['size']:
            noaddr += 1
            continue
        bad = []
        for i in range(0, o['size'], 4):
            x = struct.unpack_from('>I', o['body'], i)[0]
            y = struct.unpack_from('>I', rb, i)[0]
            if x != y and i not in o['rel']:
                bad.append((i, x, y))
        rows.append(dict(sym=n, unit=o['unit'], addr='0x%08x' % a,
                         section_size=o['size'], pdata_size=lens.get(a),
                         sym_off=o['off'], nbad=len(bad),
                         bad=[['0x%02x' % i, '%08x' % x, '%08x' % y] for i, x, y in bad]))
        if bad:
            diff += 1
        else:
            ident += 1

    print('matched pattern %r: %d distinct instantiations in our build' % (args.pattern, len(ours)))
    print('  with a retail address in target_symbol_map.json : %d  (unidentified: %d)'
          % (ident + diff, noaddr))
    print('  SECTION byte-identical to retail (reloc words skipped): %d' % ident)
    print('  differing                                            : %d' % diff)
    shown = 0
    for r in rows:
        if not r['nbad'] or shown >= args.show_diffs:
            continue
        shown += 1
        print('  %-26s sec=%d pdata=%s nbad=%d %s'
              % (r['unit'], r['section_size'], r['pdata_size'], r['nbad'], r['sym'][:64]))
        for o, x, y in r['bad'][:6]:
            print('       +%s ours=%s retail=%s' % (o, x, y))
    if args.json:
        json.dump(rows, open(args.json, 'w'), indent=1)
        print('wrote %s' % args.json)


if __name__ == '__main__':
    main()
