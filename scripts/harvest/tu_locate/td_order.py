#!/usr/bin/env python3
"""td_order.py -- the RTTI TYPE-DESCRIPTOR ORDER channel (laneBO2, 2026-07-29).

A THIRD TU-location instrument, independent of laneBD's two (RTTI
class-owned-vtable-slot spans, and string-literal cross-reference).

The idea
--------
RB3-360 is built `/O1 /Oi /GR /EHsc` with **no LTCG**, so TU spatial grouping is
preserved by the linker -- and not only in `.text`.  Every `.?AV<Class>@@` RTTI
type descriptor is emitted by the TU that *defines* the class, into `.rdata`.
So the `.rdata` typedesc sequence is a proxy for object-file order, and object
-file order is what determines `.text` order.

Therefore: given two classes whose TUs are already **pinned** in `splits.txt`,
any class whose typedesc sorts between theirs must have its `.text` between
theirs too.  That is a two-sided BRACKET on an unlocated TU, derived from the
retail binary alone.

Calibration -- USE THE GROUND-TRUTH NUMBER, NOT THE PAIRWISE ONE
-----------------------------------------------------------------
`--score` is the number that matters.  It asks the question the tool is
actually used for: for a class already pinned in `splits.txt`, does the bracket
built from its *neighbours* contain that class's real `.text`?  (A TU's own
blocks are never used to build its own bracket, so there is no leakage.)

    n = 206 spatially-coherent pinned classes with a two-sided bracket
      bracket contains >= 99 % of the unit          161  = 78.2 %
      bracket contains some of the unit               9  =  4.4 %
      bracket contains none of the unit              36  = 17.5 %
    restricted to ZERO skipped anchors      n = 88   73  = 83.0 %
    ... and bracket <= 8 KB wide            n = 26   18  = 69.2 %

*** NARROW BRACKETS ARE LESS RELIABLE, NOT MORE. ***  This is the opposite of
the intuition and it is the single most important thing to know about this
channel.  It is a selection effect: a narrow bracket means the two flanking
neighbours sit close together, which is exactly what happens when the target TU
is not between them at all.  So a tight bracket is NOT a confident bracket.

Treat a bracket as roughly 4:1 evidence -- a prior that tells you where to look
and what to test.  It cannot settle a pin on its own.

`--calibrate` reports the weaker pairwise proxy (514 joins: global Spearman rho
0.6564; adjacent-pair order preserved 64.1 % unwindowed, 89.5 % when typedescs
are <= 0x40 apart and .text <= 32 KB apart, against a 50 % chance baseline).
That 89.5 % is what the channel looked like before it was scored properly; it
FLATTERS the instrument, because preserving the order of two neighbours is a
much easier test than containing a third TU between them.  Quote 78 %, not 90.

**So: use it LOCALLY or not at all.**  The global correlation is weak; the
signal lives in tight neighbourhoods.  It is a CORROBORATING channel -- decisive
only in combination with the string / RTTI-span instruments and with
`orig/45410914/band.exe` itself, which remains the only decider.

Known failure modes (report as DECLINES, never as counter-evidence)
-------------------------------------------------------------------
* **Name-vs-stem join.**  `Rnd*` classes live in files that drop the prefix
  (class `RndMeshAnim` <-> unit `MeshAnim.cpp`).  `--alias` handles the `Rnd`
  case; other renames are silent misses.  A neighbour reported UNPINNED may
  simply be un-joined.
* **Scattered units cannot anchor.**  A unit whose COMDATs spread over
  megabytes has no single `.text` position.  Such neighbours are SKIPPED
  (`--max-spread`, default 64 KB) and the bracket reaches further out; the
  skips are printed, because a bracket resting on distant anchors is weaker.
* **Inverted brackets** mean the local order broke down.  That is the tool
  DECLINING to place the TU.  It is not evidence against any other channel.
* ★★ **A CLASS FAMILY SHARING BASE HEADERS POOLS ITS TYPEDESCS.**  This is the
  failure that refuted the channel in the field, and it is the one to know.
  laneBO2 sub-lane B placed `SetlistSortByLocation` at `0x825C4B10` and closed
  the unit at **24/24 strict-100, 100.00 % fuzzy**; this channel had said it was
  swallowed between `SongSortByDiff` and `SongSortByRank` at `0x8265D…`, on a
  clean 8-byte degenerate bracket with two coherent anchors.  The channel was
  simply WRONG: the whole `SongSort*` family's typedescs pool together in
  `.rdata` regardless of where their `.text` went, so typedesc adjacency inside
  a family carries no positional information at all.
  ⇒ **GATE, from sub-lane B: before this channel may CONTRADICT a placement,
  require that the class have its own vtable slots inside the bracket.**
  `SetlistSortByLocation`'s and `LocationCmp`'s own slots are all at
  `0x825C4…`; ZERO fall near `0x8265D…`.  That test would have made the tool
  decline instead of mislead.
* ★ **A typedesc between two pinned anchors does NOT imply a separate TU.**  One
  `.cpp` may define several classes.  laneBO2 sub-lane A refuted the claim that
  `LeaderboardShortcutProvider` was a TU swallowed by `Leaderboard.cpp`: its own
  `??_G` is at `0x8266D460` with fold count 6, *inside* `Leaderboard.cpp`'s
  genuine block, and three of its methods already matched at 100 % in
  `Leaderboard.obj`.  It is a class *defined in* `Leaderboard.cpp`.
* ★ Also refuted in the field: the order of `PlayerCampaignCareerLeaderboard`
  and `PlayerCampaignGoalLeaderboard` is INVERTED in `.text` relative to their
  typedescs -- a counter-example inside the tool's own calibration cluster.

★ But TWO KINDS OF DECLINE ARE THEMSELVES FINDINGS, and the tool now says so:

* **Zero-width bracket** -- the flanking pins ABUT, with your class ordered
  strictly between them.  A TU cannot occupy zero bytes, so it has been
  SWALLOWED by one of the two, and those two are named donor candidates.
  Observed on `SetlistSortByLocation` (an 8-byte gap between `SongSortByDiff`
  and `SongSortByRank`), on the four controller TUs (a 4-byte gap between
  `JoypadController` and `GuitarController`), and on `OutfitProvider` /
  `InstrumentFinishProvider` (`FaceHairProvider` abutting `MakeupProvider`).
* **Inverted BY CONTAINMENT** -- the lower anchor's pinned footprint strictly
  contains the upper anchor's.  Two TUs' `.text` do not interleave under /O1
  with no LTCG, so that is impossible and one of the two pins is
  MIS-ATTRIBUTED.  Observed on `PremiumAssetProvider`, where `MakeupProvider`'s
  footprint swallows `AssetProvider`'s -- which is independent structural
  confirmation of laneBL's §6.0 `MakeupProvider` mis-pin.

Worked results from the wave that built it (all confirmed against band.exe by
the sub-lanes, see docs/plans/lane-bo2-collapse-rows-2026-07-29.md):
  * the `*Provider` cluster's 7 pinned anchors are 7/7 monotone;
  * the `SongSort*` family lays out in exactly typedesc order, 7 members;
  * `MultiSelectListPanel` brackets to 0x82626A3C..0x82627118, abutting
    `ManageBandPanel`'s end exactly.

Usage
-----
    venv/bin/python scripts/harvest/tu_locate/td_order.py --class SetlistSortByLocation
    venv/bin/python scripts/harvest/tu_locate/td_order.py --neighbourhood SetlistSortByLocation --radius 12
    venv/bin/python scripts/harvest/tu_locate/td_order.py --score

Read-only.  Reads `orig/45410914/band.exe` and `config/45410914/splits.txt`.
"""
import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
BANDEXE = os.path.join(REPO, 'orig', '45410914', 'band.exe')
SPLITS = os.path.join(REPO, 'config', '45410914', 'splits.txt')


# ---------------------------------------------------------------- PE mapping
def load_pe(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    optsz = struct.unpack_from('<H', d, pe + 20)[0]
    imgbase = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    off = pe + 24 + optsz
    for _ in range(nsec):
        name = d[off:off + 8].rstrip(b'\0').decode('latin1')
        vsz, va, rsz, rp = struct.unpack_from('<IIII', d, off + 8)
        secs.append((name, imgbase + va, vsz, rp, rsz))
        off += 40
    return d, secs


def make_f2v(secs):
    def f2v(fo):
        for _n, va, _vsz, rp, rsz in secs:
            if rp <= fo < rp + rsz:
                return va + (fo - rp)
        return None
    return f2v


# ------------------------------------------------------------ the two inputs
def typedescs(d, f2v, keep_decorated=False):
    """VA of the `.?AV<Class>@@` string -> class name, in .rdata address order.

    Nested / anonymous-namespace forms (`.?AVFoo@?A0x5b3730ba@@`) carry extra
    `@`/`?` and are dropped by default: their emitting TU is much harder to
    name, and they add noise to the order.
    """
    out = {}
    for m in re.finditer(rb'\.\?AV([A-Za-z_][\w@?$]{0,120})@@\x00', d):
        cls = m.group(1).decode('latin1')
        if not keep_decorated and ('@' in cls or '?' in cls):
            continue
        va = f2v(m.start())
        if va is not None:
            out[va] = cls
    return out


def splits_blocks(path):
    """file stem -> sorted list of (lo, hi) `.text` blocks."""
    by_stem = {}
    unit = None
    for line in open(path):
        s = line.rstrip('\n')
        if not s.strip() or s.lstrip().startswith('#'):
            continue
        if not s[0].isspace() and s.rstrip().endswith(':'):
            unit = s.strip()[:-1]
            continue
        t = s.strip()
        if t.startswith('.text') and unit:
            m = re.search(r'start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)', t)
            if m:
                stem = os.path.basename(unit)
                stem = stem[:-4] if stem.endswith('.cpp') else stem
                by_stem.setdefault(stem, []).append(
                    (int(m.group(1), 16), int(m.group(2), 16)))
    for v in by_stem.values():
        v.sort()
    return by_stem


def resolve(cls, by_stem, alias=True):
    """Class name -> its unit's blocks, tolerating the Rnd-prefix rename."""
    if cls in by_stem:
        return by_stem[cls]
    if alias and cls.startswith('Rnd') and cls[3:] in by_stem:
        return by_stem[cls[3:]]
    return None


# -------------------------------------------------------------- the bracket
def spread(blocks):
    return max(b for _a, b in blocks) - min(a for a, _b in blocks)


def bracket(cls, seq, by_stem, alias=True, max_spread=0x10000):
    """Return (td_va, lo, lo_from, hi, hi_from, skipped) or None.

    ★ An anchor must be SPATIALLY COHERENT.  A unit whose `.text` COMDATs are
    scattered across megabytes has no single position, so it cannot bound
    anything: using it produces either a spuriously inverted bracket (if you
    take global max/min) or a tight-but-wrong one (if you take the closest
    pair).  Both were observed while building this tool -- `RndMeshAnim`
    (blocks from 0x822C78A0 to 0x827EBB34, a 5 MB spread) put `BandFaceDeform`
    in a `Font`/`LitAnim` neighbourhood 2 MB from its real one.

    So a neighbour qualifies as an anchor only when the span of ALL its blocks
    is <= `max_spread` (default 64 KB); otherwise we skip it and walk further
    out along the typedesc sequence.  Skipped neighbours are returned so the
    caller can report how far the bracket had to reach -- a bracket resting on
    distant anchors is weaker and should be labelled as such.
    """
    idx = [i for i, (_va, c) in enumerate(seq) if c == cls]
    if not idx:
        return None
    i = idx[0]
    lo = lo_from = hi = hi_from = None
    skipped = []
    for j in range(i - 1, -1, -1):
        bs = resolve(seq[j][1], by_stem, alias)
        if not bs:
            continue
        if spread(bs) > max_spread:
            skipped.append(('lo', seq[j][1], spread(bs)))
            continue
        lo, lo_from = max(b for _a, b in bs), seq[j][1]
        break
    for j in range(i + 1, len(seq)):
        bs = resolve(seq[j][1], by_stem, alias)
        if not bs:
            continue
        if spread(bs) > max_spread:
            skipped.append(('hi', seq[j][1], spread(bs)))
            continue
        hi, hi_from = min(a for a, _b in bs), seq[j][1]
        break
    return seq[i][0], lo, lo_from, hi, hi_from, skipped


def claims_over(by_stem, lo, hi):
    out = []
    for stem, bs in by_stem.items():
        for a, b in bs:
            if b > lo and a < hi:
                out.append((a, b, stem))
    return sorted(out)


# -------------------------------------------------------------- calibration
def calibrate(seq, by_stem, alias=True):
    pairs = []
    for va, cls in seq:
        bs = resolve(cls, by_stem, alias)
        if bs:
            pairs.append((va, min(a for a, _b in bs), cls))
    pairs.sort()
    n = len(pairs)
    if n < 3:
        print('not enough joinable classes')
        return
    rank = lambda xs: {v: i for i, v in enumerate(sorted(xs))}
    r1 = rank([p[0] for p in pairs])
    r2 = rank([p[1] for p in pairs])
    dsum = sum((r1[a] - r2[b]) ** 2 for a, b, _c in pairs)
    rho = 1 - 6 * dsum / (n * (n * n - 1))
    print('classes joinable to a pinned unit : %d' % n)
    print('global Spearman rho               : %.4f   (WEAK -- do not use globally)' % rho)
    for rw, tw in ((1 << 40, 1 << 40), (0x400, 0x20000), (0x100, 0x20000), (0x40, 0x8000)):
        ok = bad = 0
        for i in range(n - 1):
            if pairs[i + 1][0] - pairs[i][0] <= rw and abs(pairs[i + 1][1] - pairs[i][1]) <= tw:
                if pairs[i][1] < pairs[i + 1][1]:
                    ok += 1
                else:
                    bad += 1
        tot = ok + bad
        lbl = 'no window' if rw == 1 << 40 else 'rdata<=0x%-5x text<=0x%-6x' % (rw, tw)
        print('adjacent-pair order preserved, %-30s : %4d/%-4d = %5.1f %%'
              % (lbl, ok, tot, 100.0 * ok / max(1, tot)))
    print('\nchance baseline for a binary order test: 50.0 %')


def score(seq, by_stem, alias=True, max_spread=0x10000):
    """Ground-truth scoring: does a pinned class's bracket contain it?

    Only spatially-coherent pinned classes are scored -- a scattered unit has no
    single position, so "is it inside the bracket" is not a well-posed question
    for it.  A class's own blocks never contribute to its own bracket.
    """
    hits, partial, misses = [], [], []
    for cls in sorted({c for _v, c in seq}):
        bs = resolve(cls, by_stem, alias)
        if not bs or spread(bs) > max_spread:
            continue
        r = bracket(cls, seq, by_stem, alias, max_spread)
        if not r:
            continue
        _td, lo, _lf, hi, _hf, sk = r
        if lo is None or hi is None or lo >= hi:
            continue
        tot = sum(b - a for a, b in bs)
        ins = sum(max(0, min(b, hi) - max(a, lo)) for a, b in bs)
        rec = (cls, lo, hi, hi - lo, ins / tot, len(sk))
        (hits if ins / tot >= 0.99 else (partial if ins else misses)).append(rec)
    allr = hits + partial + misses
    n = len(allr)
    if not n:
        print('nothing scoreable')
        return
    print('GROUND-TRUTH SCORING -- does a pinned class\'s bracket contain it?')
    print('  n                                  = %d' % n)
    print('  contains >= 99 %% of the unit       = %3d  (%.1f %%)' % (len(hits), 100.0 * len(hits) / n))
    print('  contains some of the unit          = %3d  (%.1f %%)' % (len(partial), 100.0 * len(partial) / n))
    print('  contains none of the unit          = %3d  (%.1f %%)' % (len(misses), 100.0 * len(misses) / n))
    for label, sub in (('ZERO skipped anchors', [r for r in allr if r[5] == 0]),
                       ('ZERO skips AND <= 8 KB wide',
                        [r for r in allr if r[5] == 0 and r[3] <= 0x2000])):
        good = [r for r in sub if r[4] >= 0.99]
        if sub:
            print('  %-34s n = %3d ; >= 99 %%: %3d  (%.1f %%)'
                  % (label, len(sub), len(good), 100.0 * len(good) / len(sub)))
    print('\n  *** narrow brackets are LESS reliable, not more -- see the module docstring ***')
    print('\n  widest misses (bracket contains none of the unit):')
    for c, lo, hi, w, _f, sk in sorted(misses, key=lambda r: -r[3])[:10]:
        print('    %-28s %08X..%08X  %8d B  %d skips' % (c, lo, hi, w, sk))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--class', dest='cls', help='bracket this class')
    ap.add_argument('--neighbourhood', help='print the typedesc neighbourhood of this class')
    ap.add_argument('--radius', type=int, default=8, help='neighbours each side (default 8)')
    ap.add_argument('--calibrate', action='store_true',
                    help='pairwise proxy accuracy (FLATTERS the channel -- prefer --score)')
    ap.add_argument('--score', action='store_true',
                    help='ground-truth scoring against pinned units (the number to quote)')
    ap.add_argument('--min-tu', type=lambda x: int(x, 0), default=0x80,
                    help='a bracket narrower than this cannot hold a real TU; report it '
                         'as SWALLOWED rather than as a clean ADD (default 0x80)')
    ap.add_argument('--max-spread', type=lambda x: int(x, 0), default=0x10000,
                    help='an anchor unit whose .text blocks span more than this is '
                         'skipped as spatially incoherent (default 0x10000)')
    ap.add_argument('--no-alias', action='store_true', help='disable the Rnd-prefix alias')
    ap.add_argument('--splits', default=SPLITS)
    ap.add_argument('--exe', default=BANDEXE)
    a = ap.parse_args()

    if not os.path.exists(a.exe):
        sys.exit('missing %s (the decompressed retail PE)' % a.exe)
    d, secs = load_pe(a.exe)
    td = typedescs(d, make_f2v(secs))
    seq = sorted(td.items())
    by_stem = splits_blocks(a.splits)
    alias = not a.no_alias

    if a.score:
        score(seq, by_stem, alias, a.max_spread)
        return

    if a.calibrate:
        calibrate(seq, by_stem, alias)
        return

    if a.neighbourhood:
        idx = [i for i, (_v, c) in enumerate(seq) if c == a.neighbourhood]
        if not idx:
            sys.exit('no plain .?AV typedesc for %r' % a.neighbourhood)
        i = idx[0]
        for j in range(max(0, i - a.radius), min(len(seq), i + a.radius + 1)):
            va, c = seq[j]
            bs = resolve(c, by_stem, alias)
            shown = ' '.join('%08X..%08X' % x for x in bs[:5]) if bs else '-- UNPINNED --'
            more = '  (+%d more)' % (len(bs) - 5) if bs and len(bs) > 5 else ''
            print('%s %08X %-32s %s%s'
                  % ('>>' if j == i else '  ', va, c, shown, more))
        return

    if not a.cls:
        ap.error('one of --class / --neighbourhood / --score / --calibrate is required')

    r = bracket(a.cls, seq, by_stem, alias, a.max_spread)
    if r is None:
        sys.exit('no plain .?AV typedesc for %r -- the class may not exist in retail, '
                 'or may be in an anonymous namespace' % a.cls)
    tdva, lo, lo_from, hi, hi_from, skipped = r
    min_tu = a.min_tu
    print('class            : %s' % a.cls)
    print('typedesc .rdata  : 0x%08X' % tdva)
    print('lower anchor     : %s  (%s)' % (('0x%08X' % lo) if lo else '-- none --', lo_from))
    print('upper anchor     : %s  (%s)' % (('0x%08X' % hi) if hi else '-- none --', hi_from))
    for side, name, sp in skipped:
        print('  skipped %s anchor %-28s (blocks span %d B -- spatially incoherent)'
              % (side, name, sp))
    if lo is None or hi is None:
        print('\nVERDICT: DECLINES -- no two-sided bracket. No evidence either way.')
        return
    if lo >= hi or hi - lo < min_tu:
        lob = resolve(lo_from, by_stem, alias) or []
        hib = resolve(hi_from, by_stem, alias) or []
        if hi - lo < min_tu:
            print('\nVERDICT: DECLINES as a location -- but this DEGENERATE bracket is ITSELF '
                  'A FINDING.\n'
                  'The flanking pins effectively ABUT (%d bytes between them, at 0x%08X): %s ends\n'
                  'where %s begins, with this class ordered strictly between them.  No real TU\n'
                  'fits in %d bytes, so it has been SWALLOWED by one of the two.\n'
                  'THOSE TWO ARE YOUR DONOR CANDIDATES.  Do NOT pin the gap itself.'
                  % (hi - lo, lo, lo_from, hi_from, hi - lo))
        else:
            lo_min = min(a for a, _b in lob) if lob else None
            contained = (lob and hib
                         and lo_min is not None
                         and lo_min <= min(a for a, _b in hib)
                         and max(b for _a, b in lob) >= max(b for _a, b in hib))
            print('\nVERDICT: DECLINES -- bracket INVERTED (0x%08X >= 0x%08X).' % (lo, hi))
            if contained:
                print('CAUSE: CONTAINMENT -- %s\'s pinned footprint STRICTLY CONTAINS %s\'s.\n'
                      'Under /O1 with no LTCG two TUs\' .text do not interleave, so that is\n'
                      'impossible: ONE OF THOSE TWO PINS IS MIS-ATTRIBUTED.  This inversion is a\n'
                      'mis-attribution detector, not a failure -- fix the pin and re-run.'
                      % (lo_from, hi_from))
            else:
                print('CAUSE: the local order broke down (multi-block neighbour, or an un-joined\n'
                      'rename).  No evidence either way.')
        print('\nEither way this is NOT counter-evidence against another channel.')
        return
    print('\nVERDICT: bracket [0x%08X, 0x%08X)  = %d bytes' % (lo, hi, hi - lo))
    print('\ncurrent claims inside the bracket:')
    inside = claims_over(by_stem, lo, hi)
    if not inside:
        print('   (none -- fully unclaimed; a clean ADD)')
    for x, y, stem in inside:
        bs = by_stem[stem]
        others = [b for b in bs if not (b[0] == x and b[1] == y)]
        if others:
            dist = min(min(abs(b[0] - y), abs(x - b[1])) for b in others)
            note = 'nearest other %s block: %d B away' % (stem, dist)
        else:
            note = 'ONLY block of %s -- carving it triggers the EMPTY-UNIT TRAP' % stem
        print('   %08X..%08X %6d  %-28s %s' % (x, y, y - x, stem, note))
    print('\nReminder: corroboration only. band.exe is the decider. A claim inside your\n'
          'bracket whose unit lives far away is an ISLAND (mis-attribution candidate).')


if __name__ == '__main__':
    main()
