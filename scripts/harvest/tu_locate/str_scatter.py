#!/usr/bin/env python3
"""str_scatter.py -- separate GENUINE class-name anchors from SCATTER-BLOCK noise.

The problem this solves
-----------------------
A "class-name anchor" is a `.text` site that references a class's own name as a
string literal.  It looks like an ideal TU locator: the literal is emitted by
`OBJ_CLASSNAME` / `?StaticClassName@<Class>@@` in the class's own header, so the
referencing code should be in the class's own TU.

It often is not.  laneBD (§6.3) and laneBL (§5.3) both observed class-name
anchors landing far from the class and concluded that such an anchor is *"an
existence proof only"* -- i.e. that class-name anchors should never be used to
locate.  laneBL's §6.1 shipped `BandFaceDeform` and `ReviewDisplay` with the note
"their string anchor lands OUTSIDE the RTTI-derived span ... reconcile before
pinning either."

**That verdict over-corrects, and this tool measures by how much.**

The mechanism, quantified
-------------------------
The linker groups the small `?StaticClassName@<Class>@@` COMDATs of many
unrelated classes into shared **scatter blocks**.  A site inside such a block
tells you nothing about where its class lives.  A site *outside* one is a real
locator.  Measured over the retail image:

    classes with a .?AV descriptor                     1,127
    class-name literal CODE references                   513   (297 classes)
    scatter blocks (>= 3 distinct classes per 4 KB)        37
    ... references sitting in one                        348   (68 %)
    ... references ISOLATED, i.e. usable as locators      165   (32 %)

So the correct rule is not "class-name anchors are worthless" but:

    ***  A class-name anchor locates a TU only if it is ISOLATED.  If >= 3
         distinct classes reference their own names from the same 4 KB window,
         that window is a scatter block and none of those sites locate
         anything.  ***

The test is self-contained -- it needs only the class-name reference table, no
RTTI, no pins, no oracle -- and it is a FILTER, not a veto: it discards 68 % and
CERTIFIES the other 32 % (165 anchors) as genuine locators.

Worked cases (the pair that motivated it)
-----------------------------------------
* `BandFaceDeform` has two sites.  `0x8227A564` is inside the `0x8227A000`
  scatter block (20 `Band*` classes) -> discard.  `0x822C72D8` is isolated ->
  genuine locator, and it is inside the TU's real span.  So laneBL's
  `0x822C7298` anchor was RIGHT and the `0x8227A528` one was the wanderer.
* `ReviewDisplay` has two sites and NEITHER is in a scatter block, so this filter
  is silent on it and a different discriminator is needed (the order bracket of
  `td_order.py` separates them).

That contrast is the real lesson: neither instrument is the tie-breaker in
general -- ask first which one is even applicable.

Usage
-----
    venv/bin/python scripts/harvest/tu_locate/str_scatter.py --summary
    venv/bin/python scripts/harvest/tu_locate/str_scatter.py --class BandFaceDeform
    venv/bin/python scripts/harvest/tu_locate/str_scatter.py --blocks --min-classes 3

Requires `xref.json` from `str_xref.py` (set `TU_LOCATE_SCRATCH`).  Read-only.
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
from _paths import SCRATCH, BANDEXE  # noqa: E402

WINDOW = 0x1000
MIN_CLASSES = 3


def load():
    xr = os.path.join(SCRATCH, 'xref.json')
    if not os.path.exists(xr):
        sys.exit('missing %s -- run str_xref.py first (TU_LOCATE_SCRATCH=%s)' % (xr, SCRATCH))
    rev = json.load(open(xr))['rev']
    d = open(BANDEXE, 'rb').read()
    classes = {m.group(1).decode('latin1')
               for m in re.finditer(rb'\.\?AV([A-Za-z_][\w]{2,120})@@\x00', d)}
    sites = []
    for c in classes:
        for va in rev.get(c, []) or []:
            sites.append((va, c))
    sites.sort()
    return classes, sites


def blocks_of(sites, window=WINDOW, min_classes=MIN_CLASSES):
    buckets = defaultdict(set)
    for va, c in sites:
        buckets[va // window].add(c)
    return buckets, {k for k, v in buckets.items() if len(v) >= min_classes}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--class', dest='cls', help='verdict for one class')
    ap.add_argument('--blocks', action='store_true', help='list the scatter blocks')
    ap.add_argument('--summary', action='store_true', help='the headline counts')
    ap.add_argument('--isolated', action='store_true',
                    help='list every ISOLATED (usable) class-name anchor')
    ap.add_argument('--window', type=lambda x: int(x, 0), default=WINDOW)
    ap.add_argument('--min-classes', type=int, default=MIN_CLASSES,
                    help='distinct classes in a window that make it a scatter block')
    a = ap.parse_args()

    classes, sites = load()
    buckets, scat = blocks_of(sites, a.window, a.min_classes)
    in_scatter = [(va, c) for va, c in sites if va // a.window in scat]
    isolated = [(va, c) for va, c in sites if va // a.window not in scat]

    if a.cls:
        mine = [(va, c) for va, c in sites if c == a.cls]
        if not mine:
            print('%s: NO class-name code reference at all.' % a.cls)
            print('  The string channel is not weak here -- it is ABSENT by construction.')
            print('  (Either the class has no name literal in the image, or nothing '
                  'references it.)')
            return
        print('%s: %d class-name code site(s)\n' % (a.cls, len(mine)))
        good = []
        for va, _c in mine:
            k = va // a.window
            if k in scat:
                others = sorted(buckets[k] - {a.cls})
                print('  %08X  SCATTER BLOCK %08X..%08X (%d distinct classes) -> DISCARD'
                      % (va, k * a.window, (k + 1) * a.window, len(buckets[k])))
                print('             sharing it: %s%s'
                      % (', '.join(others[:8]), ' ...' if len(others) > 8 else ''))
            else:
                print('  %08X  ISOLATED -> GENUINE LOCATOR' % va)
                good.append(va)
        print()
        if len(good) == 1:
            print('VERDICT: exactly one site survives -- %08X locates this TU.' % good[0])
        elif not good:
            print('VERDICT: every site is a scatter-block artifact. This channel CANNOT\n'
                  'locate this TU. Use td_order.py or the RTTI span instead.')
        else:
            print('VERDICT: %d sites survive the filter, so this channel does not SEPARATE\n'
                  'them. Need a second discriminator (td_order.py bracket, or the RTTI\n'
                  'span with laneBL\'s fold + island tests).' % len(good))
        return

    if a.blocks:
        rows = sorted(((len(buckets[k]), k) for k in scat), reverse=True)
        print('%-21s %7s  %s' % ('window', 'classes', 'examples'))
        for n, k in rows:
            ex = sorted(buckets[k])
            print('%08X..%08X %7d  %s%s'
                  % (k * a.window, (k + 1) * a.window, n,
                     ', '.join(ex[:6]), ' ...' if n > 6 else ''))
        return

    if a.isolated:
        for va, c in isolated:
            print('%08X  %s' % (va, c))
        return

    n = len(sites)
    print('classes with a .?AV descriptor      : %d' % len(classes))
    print('class-name literal CODE references  : %d   (over %d distinct classes)'
          % (n, len({c for _v, c in sites})))
    print('scatter blocks (>= %d classes / %#x) : %d' % (a.min_classes, a.window, len(scat)))
    print('  references INSIDE a scatter block : %d  (%.0f %%)  -> discard, never locate'
          % (len(in_scatter), 100.0 * len(in_scatter) / n))
    print('  references ISOLATED               : %d  (%.0f %%)  -> GENUINE locators'
          % (len(isolated), 100.0 * len(isolated) / n))
    print('\nRule: a class-name anchor locates a TU only if it is ISOLATED.')
    print('This is a FILTER, not a veto -- it certifies the %d, it does not discard them all.'
          % len(isolated))


if __name__ == '__main__':
    main()
