#!/usr/bin/env python3
"""
w42_verify.py -- adjudicate one candidate name-permutation family on RETAIL BYTES.

Every check the standing protocol requires before a rename may be fired, in one
place, because each of them has independently changed a lane's patch:

  * BYTE GEOMETRY -- does the address have its own retail `.pdata` entry?  A
    PHANTOM row (a dtk mis-carve) is INDISTINGUISHABLE from an unidentified one
    by every name-keyed instrument.  (Tiny leaf stubs legitimately have no
    `.pdata`: an 8-byte leaf touches neither stack nor LR, so it gets no unwind
    record.  Absence is only suspicious for a large body.)
  * DOES THE THING EXIST -- `TrackerManager::HandleGameOver` has no retail
    address at all; `/O1 /Ob2` inlined it into `Poll`.
  * WHAT THE OBJ DEFINES -- proving a name wrong does NOT make renaming it
    safe.  If the base obj cannot DEFINE the new name the row reads a permanent
    0%.  Check DEFINES, not merely references.
  * CALL-SITE FAN-IN + CALLER SPELLING -- the strongest independent instrument,
    and the only thing that prices the cascade.  Arity and type contradictions
    are evidence, not noise.

Usage:
    python3 tools/w42_verify.py 0x826cda68 0x826cdad8 ...
"""
import bisect
import collections
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from w42_family_sweep import (EXE, REPO, TEXT_VA, TEXT_OFF, TEXT_SZ, exe,
                              pdata, pd_extent, symmap, mapstarts, retail_calls,
                              retail_extent, parse_obj)
import glob


def disasm(va, n):
    """Raw-word branch-aware listing.  Only branches are decoded by name; the
    rest is shown as hex, deliberately -- capstone silently drops words it
    cannot decode, and a dropped row is a false negative."""
    sm = symmap()
    out = []
    for off in range(0, n, 4):
        w = struct.unpack_from('>I', exe(), va - TEXT_VA + TEXT_OFF + off)[0]
        s = '  %08x  %08x' % (va + off, w)
        op = w >> 26
        if op == 18:
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            t = li if (w & 2) else (va + off + li)
            s += '  %-4s 0x%08x  ; %s' % ('bl' if w & 1 else 'b', t,
                                          sm.get(t) or 'UNNAMED')
        elif op == 32:
            s += '  lwz   r%d,%d(r%d)' % ((w >> 21) & 31, _s16(w & 0xFFFF), (w >> 16) & 31)
        elif op == 14:
            s += '  addi  r%d,r%d,%d' % ((w >> 21) & 31, (w >> 16) & 31, _s16(w & 0xFFFF))
        elif op == 19 and ((w >> 1) & 0x3FF) == 16:
            s += '  blr'
        out.append(s)
    return out


def _s16(v):
    return v - 0x10000 if v & 0x8000 else v


_encl = None


def enclosing(va):
    ent = pdata()
    starts = [e[0] for e in ent]
    i = bisect.bisect_right(starts, va) - 1
    if i >= 0:
        s, l = ent[i]
        if s <= va < s + l:
            return s
    return None


def callers(targets):
    hits = {t: [] for t in targets}
    d = exe()
    for off in range(0, TEXT_SZ - 4, 4):
        w = struct.unpack_from('>I', d, TEXT_OFF + off)[0]
        if (w >> 26) != 18 or (w & 2):
            continue
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        t = TEXT_VA + off + li
        if t in hits:
            hits[t].append(TEXT_VA + off)
    return hits


def main():
    addrs = [int(a, 16) for a in sys.argv[1:]]
    sm = symmap()
    assert len(sm) > 20000
    objs = sorted(glob.glob(os.path.join(REPO, 'build/45410914/src/**/*.obj'),
                            recursive=True))
    defines = {}
    for p in objs:
        try:
            defined, _ = parse_obj(p)
        except Exception:
            continue
        for nm in defined:
            defines.setdefault(nm, p)
    print('symbols DEFINED by our objs: %d  (assert built!)' % len(defines))

    hits = callers(set(addrs))
    for va in addrs:
        nm = sm.get(va)
        e = pd_extent(va)
        size, auth = retail_extent(va)
        print('\n' + '=' * 78)
        print('ADDR 0x%08x  map=%s' % (va, nm))
        print('  .pdata      : %s' % (
            'OWN ENTRY %d B' % e[1] if e and e[0] == va else
            ('INSIDE 0x%08x+%d  <== NOT a function start!' % e if e else 'NONE (leaf stub?)')))
        print('  extent used : %d B (%s)' % (size, 'authoritative' if auth else 'derived'))
        if nm:
            print('  our obj DEFINES this name: %s' % (
                os.path.relpath(defines[nm], REPO) if nm in defines else '*** NO ***'))
        for L in disasm(va, min(size, 96)):
            print(L)
        cs = hits.get(va, [])
        print('  CALL SITES: %d' % len(cs))
        agg = collections.Counter()
        for pc in cs:
            en = enclosing(pc)
            agg[(en, sm.get(en))] += 1
        for (en, cn), c in agg.most_common(14):
            print('    %3d x from 0x%08x  %s' % (c, en or 0, cn or '(unnamed/unattributed)'))


if __name__ == '__main__':
    main()
