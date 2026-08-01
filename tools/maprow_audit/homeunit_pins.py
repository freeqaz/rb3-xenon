#!/usr/bin/env python3
"""HOME-UNIT / PIN GEOGRAPHY for map rows (lane CI-4).

WHY THIS EXISTS
---------------
Lane CH-3 discovered, by measuring a -4, that RETAIL-BYTE CORRECTNESS AND THE
METRIC CAN DISAGREE.  objdiff pairs target<->base symbols BY NAME WITHIN A
UNIT.  So a map row that is *correct by retail bytes* still COSTS a match when
its address lies outside the pinned .text range of the unit whose object file
actually compiles that symbol.

The exact metric condition for a map row (address A, symbol S) to be able to
pair is therefore a CONJUNCTION of two independent facts:

    (1) some unit U's compiled base .obj DEFINES symbol S, and
    (2) A lies inside one of U's pinned .text blocks in splits.txt.

This tool measures (1) from the COFF symbol tables of OUR OWN COMPILED OBJECTS
and (2) from splits.txt.  Neither reads the map to decide the other, so the
two legs are independent.

WHAT IT DELIBERATELY DOES NOT DO
--------------------------------
It does NOT adjudicate whether a row is correct by retail bytes.  That is the
RTTI/vtable oracle's job (tools/maprow_audit/rtti_vtable_index.py).  This tool
answers only "could the metric ever pay for this row where it now sits, and if
not, is there a pin that would let it".

★★★ SPLITS RANGES ARE HALF-OPEN: [start, end).  READ THEM THAT WAY.
--------------------------------------------------------------------
An inclusive-end read (`start <= va <= end`) manufactures a phantom defect
class: it reports the function that BEGINS at a block's `end:` as living
INSIDE that block, which then looks like "unit U's pin swallowed a body of
class C".  Lane CI-2 flagged CharCollide.cpp / MoveMgr.cpp /
MoveAsyncDetector.cpp on exactly this basis; all three REFUTE:

  - CharCollide pins 0x822c8f50-0x822c9300, and `?Copy@BandLeadMeter@@` begins
    at 0x822c9300 -- i.e. AT the exclusive end, not inside.  The CharCollide
    split asm contains ZERO occurrences of the string 'BandLeadMeter' across
    67,876 bytes (denominator printed on purpose).
  - MoveMgr has 2 such boundary rows, MoveAsyncDetector 9 (MicInputArrow x7,
    InlineHelp, GemPlayer).  No unit among the three has ANY map row genuinely
    inside its pins whose symbol it does not define.

SIZE OF THE PHANTOM CLASS, tree-wide: 2,748 pin-end addresses hold a map row,
988 of which name a different class than the pinning unit.  An inclusive-end
reader would report all 988 as unit/class mismatches.  They are the OPPOSITE
of a defect -- they are the pin correctly stopping where a foreign TU's code
begins.

POSITIVE CONTROL for the exclusivity (measured, not asserted): lane CI-4's
wave 1 extended pins to cover rows at gap EXACTLY +0 from a block's end -- e.g.
`?Copy@CharHair@@` at 0x823ab978, the precise `end:` of CharHair's block -- and
those extensions GAINED matches (+19 whole-binary).  Had `end` been inclusive,
those functions were already in the unit and no gain was possible.
"""
import os
import sys
import json
import re
import struct
import random
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
BUILD = os.path.join(ROOT, 'build', '45410914')
SPLITS = os.path.join(ROOT, 'config', '45410914', 'splits.txt')
MAP = os.path.join(ROOT, 'scripts', 'target_symbol_map.json')

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


# ---------------------------------------------------------------- COFF
def obj_defined_symbols(path):
    """EXTERNAL symbols DEFINED (SectionNumber>0) in a COFF .obj.

    ⚠ Undefined externals (SectionNumber==0) are REFERENCES, not definitions.
    Counting them would attribute a symbol to every unit that merely calls it
    -- which would make the home-unit test vacuously true almost everywhere.
    That failure mode is exactly what `--selftest` control C3 checks.
    """
    with open(path, 'rb') as f:
        d = f.read()
    if len(d) < 20:
        return set()
    _mach, _ns, _ts, symoff, nsym, _oh, _ch = struct.unpack_from('<HHIIIHH', d, 0)
    if symoff == 0 or nsym == 0 or symoff + 18 * nsym > len(d):
        return set()
    strtab = symoff + 18 * nsym
    out = set()
    i = 0
    while i < nsym:
        off = symoff + 18 * i
        raw = d[off:off + 8]
        value, secnum, _typ, sclass, naux = struct.unpack_from('<IhHBB', d, off + 8)
        if raw[:4] == b'\x00\x00\x00\x00':
            stroff = struct.unpack_from('<I', raw, 4)[0]
            e = d.find(b'\x00', strtab + stroff)
            name = d[strtab + stroff:e].decode('ascii', 'replace')
        else:
            name = raw.rstrip(b'\x00').decode('ascii', 'replace')
        if sclass == IMAGE_SYM_CLASS_EXTERNAL and secnum > 0:
            out.add(name)
        i += 1 + naux
    return out


def build_symbol_index(verbose=False):
    """basename(unit) -> set(symbol), and symbol -> set(basename)."""
    unit_syms = {}
    sym_units = collections.defaultdict(set)
    srcdir = os.path.join(BUILD, 'src')
    n = 0
    dupbase = collections.defaultdict(list)
    for dirpath, _dirs, files in os.walk(srcdir):
        for fn in files:
            if not fn.endswith('.obj'):
                continue
            p = os.path.join(dirpath, fn)
            base = fn[:-4]
            dupbase[base].append(p)
            syms = obj_defined_symbols(p)
            unit_syms.setdefault(base, set()).update(syms)
            for s in syms:
                sym_units[s].add(base)
            n += 1
    collisions = {k: v for k, v in dupbase.items() if len(v) > 1}
    if verbose:
        print(f'[obj-index] {n} objs, {len(unit_syms)} distinct basenames, '
              f'{len(sym_units):,} distinct defined external symbols')
        print(f'[obj-index] basename COLLISIONS: {len(collisions)}'
              + (f'  {list(collisions)[:5]}' if collisions else ''))
    return unit_syms, sym_units, collisions


# ---------------------------------------------------------------- splits
def parse_splits(path=SPLITS):
    """unit(with .cpp) -> [(start,end)] for .text only."""
    units = collections.OrderedDict()
    unit = None
    for ln in open(path):
        s = ln.rstrip('\n')
        if not s.strip() or s.lstrip().startswith('#'):
            continue
        if not s.startswith((' ', '\t')) and s.rstrip().endswith(':'):
            unit = s.strip()[:-1]
            units.setdefault(unit, [])
        elif unit is not None and s.strip().startswith('.text'):
            m = re.search(r'start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)', s)
            if m:
                units[unit].append((int(m.group(1), 16), int(m.group(2), 16)))
    return units


def splits_base(units):
    """basename (no dir, no .cpp/.c/.s) -> merged list of ranges.

    ⚠ splits.txt MIXES TWO NAMING FORMS: bare unit names ('MasterAudio.cpp')
    and full repo-relative paths ('system/rndobj/SoftParticles.cpp').  An
    earlier version of this function stripped only the extension, so the key
    'system/rndobj/SoftParticles' could never equal the obj basename
    'SoftParticles'.  That join failure manufactured a fake population of
    4,722 "homeless" map rows (17.15% of the map) -- 4,371 of which objdiff
    scores at 100%.  The report.json cross-check is what caught it.  Take the
    BASENAME.
    """
    out = collections.defaultdict(list)
    for u, rs in units.items():
        b = re.sub(r'\.(cpp|c|s|cc)$', '', os.path.basename(u))
        out[b].extend(rs)
    return out


class Pins:
    def __init__(self, base_ranges):
        self.by_unit = base_ranges
        self.flat = []
        for u, rs in base_ranges.items():
            for s, e in rs:
                self.flat.append((s, e, u))
        self.flat.sort()

    def owner(self, va):
        """Which unit's pin contains va (None if unpinned)."""
        import bisect
        i = bisect.bisect_right(self.flat, (va, float('inf'), '')) - 1
        if i < 0:
            return None
        s, e, u = self.flat[i]
        return u if s <= va < e else None

    def in_unit(self, va, unit):
        for s, e in self.by_unit.get(unit, ()):
            if s <= va < e:
                return True
        return False


# ---------------------------------------------------------------- selftest
def selftest():
    ok = True
    print('=== homeunit_pins selftest ===')
    units = parse_splits()
    pins = Pins(splits_base(units))
    nblocks = len(pins.flat)
    cov = sum(e - s for s, e, _ in pins.flat)
    print(f'[C0] splits: {len(units)} units, {nblocks} .text blocks, '
          f'{cov:,} bytes covered')
    c0 = nblocks > 4000 and cov > 5_000_000
    print(f'     -> {"OK" if c0 else "SUSPECT"}')
    ok &= c0

    unit_syms, sym_units, coll = build_symbol_index(verbose=True)

    # C1 POSITIVE, TWO INDEPENDENT PROBES.  ⚠ The first version of this control
    # probed '?ClassName@MasterAudio@@' and FAILED -- correctly.  MasterAudio is
    # a BeatMatchSink, not an Hmx::Object, so it has no ClassName at all.  The
    # control was right and the PROBE was wrong; recording that here so nobody
    # "fixes" it by loosening the assertion.  (It also taught us the PPC
    # mangling is 'UBA' -- __cdecl -- not x86's 'UBE' thiscall.)
    c1 = True
    for probe, want in (('?ClassName@Object@Hmx@@UBA?AVSymbol@@XZ', 'Object'),
                        ('?SetPracticeMode@MasterAudio@@QAAX_N@Z', 'MasterAudio')):
        got = sorted(sym_units.get(probe, ()))
        good = want in got
        print(f'[C1] positive: {probe[:46]:46s} -> {got[:3]} '
              f'{"OK" if good else "BROKEN"}')
        c1 &= good
    ok &= c1

    # C2 FAIL-ON-DEMAND: a symbol that cannot exist must be absent.
    bogus = '?ZzNotReal@CI4@@UAEXXZ'
    print(f'[C2] fail-on-demand: bogus symbol present={bogus in sym_units} -> '
          f'{"OK (absent)" if bogus not in sym_units else "BROKEN"}')
    ok &= bogus not in sym_units

    # C3 FAIL-ON-DEMAND on the DEFINITION filter.  If we had counted UNDEFINED
    # externals as definitions, a leaf symbol would be "defined" in dozens of
    # units.  Measure the fan-out: it must be small.
    fan = collections.Counter(len(v) for v in sym_units.values())
    multi = sum(c for k, c in fan.items() if k > 1)
    print(f'[C3] definition fan-out: {multi:,}/{len(sym_units):,} symbols '
          f'defined in >1 unit ({100*multi/max(1,len(sym_units)):.1f}%)')
    c3 = multi < len(sym_units) * 0.5
    print(f'     -> {"OK (definitions, not references)" if c3 else "BROKEN (looks like references)"}')
    ok &= c3

    # C4 UNTREATED-POPULATION NULL: for RANDOM pinned addresses, how often is
    # the owning unit the "home unit" of a randomly chosen symbol?  This is the
    # base rate any home-unit claim must beat.
    random.seed(4242)
    allsyms = [s for s, u in sym_units.items() if len(u) == 1]
    hits = 0
    N = 2000
    for _ in range(N):
        s, e, u = random.choice(pins.flat)
        va = random.randrange(s, max(s + 1, e))
        sym = random.choice(allsyms)
        home = next(iter(sym_units[sym]))
        if pins.in_unit(va, home):
            hits += 1
    print(f'[C4] untreated null: a RANDOM pinned address falls in a RANDOM '
          f'symbol\'s home unit {hits}/{N} = {100*hits/N:.2f}%')
    print('     (denominator printed on purpose -- home-unit agreement in the '
          'charged stratum must be read against this base rate)')

    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(selftest())
