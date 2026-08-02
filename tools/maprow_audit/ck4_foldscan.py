#!/usr/bin/env python3
"""RELOCATION-NORMALIZED BODY HASHING over .pdata-authoritative extents (lane CK-4).

THE QUESTION
------------
Lane CJ-2 left 217 forwarder-thunk map rows unrepaired and RECOMMENDED AGAINST
name synthesis "until the ICF-fold question is settled", because the systematic
`Replace@RndDir -> Copy` pattern in its class (b) looks like the same artifact as
0x82402f68's fan-in 21.

The confound, stated precisely.  For a thunk row A the branch channel reads

    h(A) = method claimed by A's own map name
    w(A) = method of the map name on the address A BRANCHES TO

and calls A defective when h != w.  w(A) is only as good as the TARGET row's
name.  If the target is an ICF fold representative carrying several true
identities and the map names only one of them, then w(A) is wrong, A is
CORRECTLY named, and "repairing" A would BREAK it.

WHY THE OBVIOUS INSTRUMENTS CANNOT ANSWER THIS (both measured elsewhere)
-----------------------------------------------------------------------
* match% / objdiff -- report.rs:394 hard-sets reloc args to None, so a folded
  callee and a WRONG callee score identically.  Structurally blind.
* raw memcmp for duplicate bodies -- SILENTLY VACUOUS.  PC-relative `b`/`bl`
  displacements differ at different addresses, so two IDENTICAL functions are
  NOT identical bytes.  This instrument would "prove" ICF by finding nothing.

WHAT THIS TOOL DOES
-------------------
Hash every .pdata-delimited body twice:

  RELOC-IDENTICAL  the 24-bit LI displacement of every b/bl is replaced by the
                   ABSOLUTE target VA.  Two copies hash equal iff they are the
                   same code CALLING THE SAME CALLEES.  This is the population
                   MSVC's /OPT:ICF is allowed to fold.
  SHAPE-IDENTICAL  the LI displacement is ZEROED.  Two copies hash equal iff
                   they are the same code IGNORING call targets -- the
                   per-instantiation template family that ICF may NOT fold.

Conditional branches (opcode 16) are left ALONE in both: their BD displacement
is intra-function and therefore already identical between two copies of the same
function.  Absolutising them would make identical functions hash DIFFERENTLY --
the exact inversion of the memcmp defect above.

THE NULL (without it this tool confirms whatever you point it at)
----------------------------------------------------------------
`--null` re-hashes each function's body read from a RANDOM OFFSET inside .text,
preserving the length distribution and the instruction mix but destroying
function-boundary alignment.  Duplicate surplus found there is what the
instruction mix alone produces.  A treatment surplus at or below the null means
NO detectable duplicate structure -- which for the reloc-identical population is
the SIGNATURE OF FOLDING HAVING ALREADY HAPPENED.
"""
import argparse
import collections
import hashlib
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from thunk_identity import Image                                    # noqa: E402

BAND = os.path.join(ROOT, 'orig/45410914/band.exe')


def pdata(img):
    """-> sorted [(va, length_bytes, prolog_insns, has_eh)] from .pdata.

    Xbox 360 compact RUNTIME_FUNCTION: BIG-ENDIAN DWORD pair, MSVC LSB-first
    bitfields over the second dword -- PrologLen:8, FuncLen:22 (INSTRUCTIONS,
    so *4), ThirtyTwoBit:1, ExceptionFlag:1.  Endianness was probed, not
    assumed: the little-endian read yields 0/200 plausible lengths, big-endian
    200/200.
    """
    sec = [s for s in img.secs if s[0] == '.pdata'][0]
    _n, _rva, vsz, praw, _rsz = sec
    out = []
    for i in range(vsz // 8):
        beg, dat = struct.unpack_from('>II', img.data, praw + i * 8)
        if beg == 0:
            continue
        out.append((beg, ((dat >> 8) & 0x3FFFFF) * 4, dat & 0xFF, (dat >> 31) & 1))
    out.sort()
    return out


def is_funclet(img, va):
    """MSVC X360 EH funclet signature: the body opens `addi rX, r12, imm`.

    A funclet does not build its own frame -- it recovers the PARENT's from the
    incoming r12, so its first instruction is always an addi off r12, where a
    real function opens with mflr/stwu/stw.  Cross-validated as a population
    definition, NOT assumed: excluding these leaves 40,628 bodies against lane
    CD-7's independently-derived 40,609 non-funclet population -- agreement to
    19 rows (0.05%) by a completely different discriminator.

    ★ Funclets MUST be excluded from any ICF measurement.  They are
    IMAGE_COMDAT_SELECT_ASSOCIATIVE code tied to their parent function's COMDAT,
    so /OPT:ICF -- which folds COMDATs -- can only fold a funclet when its whole
    PARENT folds.  Identical funclets under differing parents therefore SURVIVE,
    and including them inflates the reloc-identical surplus from 51 to 9,078
    (26x its null) and reads as a decisive "ICF IS OFF".
    """
    o = img.offset(va)
    if o is None or o + 4 > len(img.data):
        return False
    w = struct.unpack_from('>I', img.data, o)[0]
    return (w >> 26) == 14 and ((w >> 16) & 0x1F) == 12


def norm(img, va, length, mode):
    """Normalized body bytes, or None if unreadable.

    mode 'reloc' : b/bl LI -> absolute target VA   (identical INCLUDING callees)
    mode 'shape' : b/bl LI -> 0                    (identical IGNORING callees)
    """
    off = img.offset(va)
    if off is None or off + length > len(img.data) or length < 4:
        return None
    out = bytearray()
    for i in range(length // 4):
        w = struct.unpack_from('>I', img.data, off + i * 4)[0]
        if (w >> 26) == 18:                       # b / bl / ba / bla
            head = w & 0xFC000003                 # opcode + AA + LK
            if mode == 'shape':
                out += struct.pack('>II', head, 0)
            else:
                li = w & 0x03FFFFFC
                if li & 0x02000000:
                    li -= 0x04000000
                tgt = li if ((w >> 1) & 1) else (va + i * 4 + li)
                out += struct.pack('>II', head, tgt & 0xFFFFFFFF)
        else:
            out += struct.pack('>I', w)
    return bytes(out)


def scan(img, funcs, mode, shift=None, rnd=None, text=None):
    """-> {digest: [va, ...]}.  `shift` non-None => random-offset NULL."""
    groups = collections.defaultdict(list)
    tlo, thi = text
    for va, ln, _p, _e in funcs:
        a = va
        if shift is not None:
            a = rnd.randrange(tlo, max(thi - ln, tlo + 4)) & ~3
        b = norm(img, a, ln, mode)
        if b is None:
            continue
        groups[hashlib.sha1(b).digest()].append(va)
    return groups


def surplus(groups):
    dup = {k: v for k, v in groups.items() if len(v) > 1}
    return sum(len(v) - 1 for v in dup.values()), len(dup), dup


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', default=BAND)
    ap.add_argument('--max-len', type=int, default=0x20000)
    ap.add_argument('--nulls', type=int, default=3)
    ap.add_argument('--keep-funclets', action='store_true',
                    help='DO NOT USE for an ICF verdict -- see is_funclet()')
    ap.add_argument('--json', help='dump reloc-identical + shape groups here')
    args = ap.parse_args()

    img = Image(args.exe)
    ts = [s for s in img.secs if s[0] == '.text'][0]
    text = (0x82000000 + ts[1], 0x82000000 + ts[1] + ts[2])
    funcs = [f for f in pdata(img) if 4 <= f[1] <= args.max_len
             and text[0] <= f[0] < text[1]]
    nall = len(funcs)
    if not args.keep_funclets:
        funcs = [f for f in funcs if not is_funclet(img, f[0])]
    print('.text  0x%08x-0x%08x' % text)
    print('funclets excluded: %d of %d' % (nall - len(funcs), nall))
    print('.pdata functions in .text: %d   total bytes %d'
          % (len(funcs), sum(f[1] for f in funcs)))
    lens = collections.Counter(f[1] for f in funcs)
    print('  smallest bodies: %s' % sorted(lens.items())[:6])

    res = {}
    for mode in ('reloc', 'shape'):
        g = scan(img, funcs, mode, text=text)
        s, ng, dup = surplus(g)
        res[mode] = (s, ng, dup)
        print('\n[%s-identical] duplicate groups %d   SURPLUS COPIES %d'
              % (mode.upper(), ng, s))
        nulls = []
        for seed in range(args.nulls):
            rnd = random.Random(1000 + seed)
            gn = scan(img, funcs, mode, shift=True, rnd=rnd, text=text)
            sn, _ngn, _ = surplus(gn)
            nulls.append(sn)
        print('  random-offset NULL surplus: %s' % nulls)
        verdict = ('AT/BELOW NULL -> no detectable duplicate structure'
                   if s <= max(nulls) else
                   'ABOVE NULL by %.1fx -> real duplicate structure' % (s / max(max(nulls), 1)))
        print('  => %s' % verdict)

    sr, _, _ = res['reloc'][:3]
    ss, _, _ = res['shape'][:3]
    print('\nRATIO shape-surplus / reloc-surplus = %.1fx' % (ss / max(sr, 1)))

    if args.json:
        import json
        out = {m: {('%s' % k.hex()): ['0x%08x' % a for a in v]
                   for k, v in res[m][2].items()} for m in res}
        json.dump(out, open(args.json, 'w'), indent=1)
        print('wrote %s' % args.json)


if __name__ == '__main__':
    main()
