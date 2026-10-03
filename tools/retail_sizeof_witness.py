#!/usr/bin/env python3
"""retail_sizeof_witness -- sizeof(T) from RETAIL BYTES, with T named by RTTI.

Lane W16-OP (2026-10-03).  Generalises tools/newobj_size_screen.py, whose
population was only the ~209 `T::NewObject` rows because there "the NAME
determines the allocated type".  Here the allocated type is determined by the
BYTES instead, so no map name is consulted anywhere:

    li    r3, N           <- the allocation size: a plain immediate
    bl    X               <- any allocator (operator new, MemAlloc, PoolAlloc,
                             a class-level operator new ...)
    ...                   <- r3 (the result) flows through mr / cmpwi / beq
    bl    Y               <- Y is called with the allocation as `this`, and Y's
                             body stores a vtable at 0(this): a constructor.
                             Its OWN class is its LAST offset-0 vtable store
                             (inlined base ctors store base vtables first);
                             the vtable's Complete Object Locator names it.
    stw   vt, 0(p)        <- or the caller itself installs the vtable (an
                             inlined ctor); last offset-0 store wins.

=> retail allocated N bytes for an object whose most-derived vtable is T.
   With no placement tricks that is sizeof(T).

The allocator X is NOT assumed: `--allocators` prints which callees produced
witnesses, so a non-allocator that happened to fit the shape (a getter taking
an int index and returning a T*) can be seen and excluded.  A class that gets
several different N values is reported as CONFLICT, never resolved silently.

Our side: `--ours CLASS...` asks the compiler (class_layout_report --json) for
sizeof, so a disagreement is retail bytes vs the compiler, never vs a comment.

Read-only.  Usage:
    python3 tools/retail_sizeof_witness.py --json OUT          # census
    python3 tools/retail_sizeof_witness.py --allocators         # X census
    python3 tools/retail_sizeof_witness.py --class BandCharacter
    python3 tools/retail_sizeof_witness.py --selftest
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from tools.retail_rtti import RetailRtti  # noqa: E402
import tools.vtable_class_name_audit as vca  # noqa: E402

NONVOL = set(range(14, 32))
WINDOW = 48          # instructions followed after the allocator call


def _bl_target(pc, w):
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (pc + li) & 0xFFFFFFFF


class Witness:
    def __init__(self):
        self.R = RetailRtti()
        self.ext = self.R.extents
        self._own = {}

    def extent(self, a):
        e = self.ext.get(a)
        return e if e is not None else vca.leaf_extent(self.R, a)

    def own_class(self, y):
        """Class whose vtable y stores LAST at 0(this) -- None if y is not ctor-shaped."""
        if y in self._own:
            return self._own[y]
        size = self.extent(y)
        cls = None
        if size:
            st = [(i, vt) for i, d, vt in vca.this_stores(self.R, y, size) if d == 0]
            for _i, vt in reversed(st):
                c = self.R.class_of_vtable(vt)
                if c:
                    cls = c
                    break
        self._own[y] = cls
        return cls

    def follow(self, fn, size, start):
        """From the word after the allocator `bl` at index `start`, follow the
        returned pointer.  -> (class, how, ctor_va|None) or None."""
        R = self.R
        ptr = {3}
        regs = {}
        cls = how = ctor = None
        for i in range(start + 1, min(size // 4, start + 1 + WINDOW)):
            pc = fn + 4 * i
            w = R.u32(pc)
            if w is None:
                break
            op = w >> 26
            d, a, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
            if w in (vca.BLR, vca.BCTR) or (op == 18 and not (w & 1)):
                break
            if op == 15:
                base = 0 if a == 0 else regs.get(a)
                ptr.discard(d)
                if base is None:
                    regs.pop(d, None)
                else:
                    regs[d] = (base + (imm << 16)) & 0xFFFFFFFF
            elif op == 14:
                simm = imm - 0x10000 if imm & 0x8000 else imm
                base = 0 if a == 0 else regs.get(a)
                ptr.discard(d)
                if base is None:
                    regs.pop(d, None)
                else:
                    regs[d] = (base + simm) & 0xFFFFFFFF
            elif op == 24 and w != 0x60000000:
                base = regs.get(d)
                ptr.discard(a)
                if base is None:
                    regs.pop(a, None)
                else:
                    regs[a] = base | imm
            elif op == 31:
                xo = (w >> 1) & 0x3FF
                rb = (w >> 11) & 31
                if xo == 444 and d == rb:                       # mr a, d
                    if d in ptr:
                        ptr.add(a)
                    else:
                        ptr.discard(a)
                    if d in regs:
                        regs[a] = regs[d]
                    else:
                        regs.pop(a, None)
                elif xo in vca.X31_STORES:
                    if xo in vca.X31_STORE_UPDATE:
                        ptr.discard(a)
                elif xo in vca.X31_WRITES_RA:
                    ptr.discard(a)
                    regs.pop(a, None)
                elif xo == 0 or xo == 32:                       # cmpw / cmplw
                    pass
                else:
                    ptr.discard(d)
                    regs.pop(d, None)
            elif op in (36, 37):                                # stw
                disp = imm - 0x10000 if imm & 0x8000 else imm
                if disp == 0 and a in ptr and d in regs:
                    c = R.class_of_vtable(regs[d])
                    if c:
                        cls, how, ctor = c, "INLINE", None
                if op == 37:
                    ptr.discard(a)
            elif op == 18 and (w & 1):                          # bl
                if vca._is_gpr_helper(R, pc, w):
                    continue
                y = _bl_target(pc, w)
                arg_is_ptr = 3 in ptr
                if arg_is_ptr:
                    c = self.own_class(y)
                    if c:
                        cls, how, ctor = c, "CTOR", y
                    elif cls is not None:
                        break        # a non-ctor call on the finished object
                elif cls is not None:
                    break
                keep = ptr & NONVOL
                ptr = keep | ({3} if (arg_is_ptr and cls is not None) else set())
                for r in (0,) + tuple(range(3, 13)):
                    regs.pop(r, None)
            elif op in (10, 11):                                # cmpli / cmpi
                pass
            elif op == 16:                                      # bc
                pass
            else:
                if op in vca.WRITES_RD:
                    ptr.discard(d)
                    regs.pop(d, None)
                if op in vca.WRITES_RA or op in vca.UPDATE_FORMS:
                    ptr.discard(a)
                    regs.pop(a, None)
        if cls is None:
            return None
        return cls, how, ctor

    def scan(self):
        """[(fn, site, allocator, N, class, how, ctor)]"""
        R = self.R
        out = []
        for fn, size in sorted(self.ext.items()):
            li3 = None                 # (value, index) of the last `li r3,N`
            for i in range(size // 4):
                w = R.u32(fn + 4 * i)
                if w is None:
                    break
                op = w >> 26
                d, a, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
                if op == 14 and a == 0 and d == 3:            # li r3, imm
                    li3 = (imm - 0x10000 if imm & 0x8000 else imm, i)
                    continue
                if op == 18 and (w & 1):
                    pc = fn + 4 * i
                    if vca._is_gpr_helper(R, pc, w):
                        continue
                    if li3 is not None and li3[0] >= 4 and i - li3[1] <= 8:
                        f = self.follow(fn, size, i)
                        if f:
                            out.append((fn, pc, _bl_target(pc, w), li3[0]) + f)
                    li3 = None
                    continue
                # anything else that writes r3 kills the constant
                if op == 31:
                    xo = (w >> 1) & 0x3FF
                    if xo in vca.X31_WRITES_RA or (xo == 444):
                        if a == 3:
                            li3 = None
                    elif xo not in vca.X31_STORES and d == 3:
                        li3 = None
                elif (op in vca.WRITES_RD or op in (14, 15)) and d == 3:
                    li3 = None
                elif op == 24 and a == 3 and w != 0x60000000:
                    li3 = None
                elif w in (vca.BLR, vca.BCTR) or (op == 18 and not (w & 1)) or op == 16:
                    li3 = None
        return out


def by_class(rows, allowed=None):
    agg = collections.defaultdict(lambda: collections.Counter())
    sites = collections.defaultdict(list)
    for fn, pc, x, n, cls, how, ctor in rows:
        if allowed is not None and x not in allowed:
            continue
        agg[cls][n] += 1
        sites[cls].append((pc, x, n, how, ctor))
    return agg, sites


def demangle_td(td):
    # '.?AVFoo@Bar@@' -> 'Bar::Foo'
    s = td[4:] if td.startswith(".?A") else td
    s = s.rstrip("@")
    parts = [p for p in s.split("@") if p]
    return "::".join(reversed(parts))


def allocator_census(rows, R=None):
    c = collections.Counter(r[2] for r in rows)
    return c


def selftest():
    """Controls drawn from facts established by other instruments:
       PreloadPanel = 164 (NEWOBJ-1, fuzzy 100 row) must be witnessed;
       a sabotage (forget `mr` propagation) must lose witnesses."""
    W = Witness()
    rows = W.scan()
    agg, _ = by_class(rows)
    ok = True
    pp = agg.get(".?AVPreloadPanel@@")
    print("PreloadPanel:", dict(pp) if pp else None)
    if not pp or pp.most_common(1)[0][0] != 164:
        print("FAIL: PreloadPanel control not witnessed at 164")
        ok = False
    n = len(rows)
    print("witnesses:", n, "classes:", len(agg))
    if n < 500:
        print("FAIL: implausibly few witnesses (vacuity guard)")
        ok = False
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json")
    ap.add_argument("--allocators", action="store_true")
    ap.add_argument("--class", dest="cls")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    W = Witness()
    rows = W.scan()
    if a.allocators:
        for x, k in allocator_census(rows).most_common(40):
            print(f"0x{x:08x} {k}")
        return 0
    agg, sites = by_class(rows)
    if a.cls:
        for td in agg:
            if demangle_td(td).split("::")[-1] == a.cls or demangle_td(td) == a.cls:
                print(td, dict(agg[td]))
                for s in sites[td]:
                    print("   site 0x%08x alloc 0x%08x N=%d %s ctor=%s" % (
                        s[0], s[1], s[2], s[3], hex(s[4]) if s[4] else "-"))
        return 0
    out = {}
    for td, cnt in sorted(agg.items()):
        out[td] = {"name": demangle_td(td), "sizes": {str(k): v for k, v in cnt.items()},
                   "sites": [[hex(s[0]), hex(s[1]), s[2], s[3],
                              hex(s[4]) if s[4] else None] for s in sites[td]]}
    if a.json:
        json.dump(out, open(a.json, "w"), indent=1)
    conf = sum(1 for v in agg.values() if len(v) > 1)
    print(f"witness rows {len(rows)}  classes {len(agg)}  CONFLICT {conf}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
