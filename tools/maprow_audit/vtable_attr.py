#!/usr/bin/env python3
"""Dethunked class<->function attribution built on rtti_vtable_index.

A vtable slot often holds an ADJUSTOR/VBASE THUNK, not the function. Those
thunks have NO .pdata entry, so any .pdata-driven size lookup returns None and
a naive dethunk silently bails -- the exact vacuity CG-1 documented (its thunk
stratum read "0/17" which looked like "no information" but was "instrument
broken"). So the window here is FIXED, never .pdata-derived.

Attribution is deliberately a SET per address, never a single answer:
inheritance (a derived class repeating a base slot) and ICF (reloc-identical
bodies folded) both legitimately put one VA in several classes' vtables, and
collapsing that to one label would manufacture false contradictions.
"""
import sys, os, json, struct, collections, random, re

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from retail_reader import Image, insns, branch_target        # noqa: E402
from rtti_vtable_index import Rtti, demangle_class, pdata_sizes, pdata_starts  # noqa: E402

WINDOW = 48   # bytes; thunks are 8-20B, this is generous and size-INDEPENDENT


def thunk_target(img, va):
    """If `va` starts a forwarder thunk, return its unconditional branch
    target, else None. FIXED WINDOW -- never consults .pdata."""
    for a, i in insns(img, va, WINDOW):
        op = i >> 26
        t = branch_target(a, i)
        if t is not None:
            return None if t[1] else t[0]     # bl => real body, not a thunk
        if op == 19:                          # blr / bctr etc -> real body
            return None
        # allow the usual adjustor prologue ops: lwz(32) addi(14) add/or(31)
        if op not in (32, 14, 31):
            return None
    return None


def dethunk(img, va, depth=0, seen=None):
    """RECURSIVE (two-level chains exist: vtordisp -> adjustor -> base)."""
    if seen is None:
        seen = set()
    if va in seen or depth > 8:
        return va, depth
    seen.add(va)
    t = thunk_target(img, va)
    if t is None:
        return va, depth
    return dethunk(img, t, depth + 1, seen)


class Attr:
    def __init__(self, img=None):
        self.img = img or Image()
        self.r = Rtti(self.img)
        self.r.build_attribution()
        self.raw = {}      # VA -> {class -> [(vt, slot, coloff)]}
        self.deth = {}     # dethunked VA -> {class -> [(vt, slot, coloff)]}
        self.slot_deth = {}
        for vt, (nm, off, sl) in self.r.vt_slots.items():
            c = demangle_class(nm)
            for i, fn in enumerate(sl):
                self.raw.setdefault(fn, {}).setdefault(c, []).append((vt, i, off))
                d = self.slot_deth.get(fn)
                if d is None:
                    d = dethunk(self.img, fn)[0]
                    self.slot_deth[fn] = d
                self.deth.setdefault(d, {}).setdefault(c, []).append((vt, i, off))

    def classes(self, va, dethunked=True):
        return set((self.deth if dethunked else self.raw).get(va, {}))


def selftest():
    img = Image()
    ok = True
    # CONTROL 1 (positive, INDEPENDENTLY ESTABLISHED BY ANOTHER LANE):
    # CG-1 verified the three ??_EMasterAudio W3/W7/WDA thunks branch to
    # 0x8277fbe0. If the dethunker is right it must reproduce that.
    print('[C1] MasterAudio thunk cross-check (CG-1 established 0x8277fbe0):')
    found = []
    a = Attr(img)
    for va, d in a.slot_deth.items():
        if d == 0x8277fbe0 and va != d:
            found.append(va)
    print(f'     slots dethunking to 0x8277fbe0: {[hex(x) for x in found]}')
    hit = len(found) >= 1
    print(f'     -> {"OK" if hit else "FAIL (no thunk resolves there)"}')
    ok &= hit

    # CONTROL 2 (FAIL ON DEMAND): a real function body must NOT be read as a
    # thunk. 0x8277fbe0 is a real ??_G body; thunk_target must return None.
    t = thunk_target(img, 0x8277fbe0)
    print(f'[C2] fail-on-demand: thunk_target(0x8277fbe0)={t} -> '
          f'{"OK (real body, not a thunk)" if t is None else "BROKEN"}')
    ok &= t is None

    # CONTROL 3 (positive): a KNOWN thunk must resolve.
    t = thunk_target(img, 0x827f76a0)
    print(f'[C3] thunk_target(0x827f76a0)={hex(t) if t else None} -> '
          f'{"OK" if t == 0x827f6458 else "FAIL"}')
    ok &= t == 0x827f6458

    print(f'[i] vtable slots {len(a.slot_deth)}, of which thunks '
          f'{sum(1 for k,v in a.slot_deth.items() if k!=v)}')
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(selftest())
