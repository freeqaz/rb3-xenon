#!/usr/bin/env python3
"""Lane PINFIX-1: carve each mis-pinned row out of its source unit and re-home it.

WHAT IS EDITED, AND WHAT IS DELIBERATELY NOT
--------------------------------------------
ONLY `.text`.  `.pdata` in splits.txt is DERIVED OUTPUT -- every split run clears
the whole `.pdata` set and re-derives one range per `.text` block, then rewrites
the file.  Hand-editing or hand-carrying `.pdata` is therefore both useless and a
way to manufacture a conflict; those lines are passed through verbatim.

SIZING: report.json's row size IS THE WRONG RULER FOR A SPLIT BOUNDARY, and the
split refuses rather than silently mis-carving.  Measured here:

    Failed: Split system/bandobj/ReviewDisplay.cpp .text (0x8231E778..0x8231E784)
            ends within symbol 'fn_8231E778' (0x8231E778..0x8231E788)

report.json scores that adjustor thunk at 12 B; dtk's own symbol extent is 16 B
(the thunk body plus its 4 bytes of trailing padding, which carry no function
row and so are not in the scored size).  Only 3 of 76 rows disagree -- but a
single one aborts the whole split, so the carve is sized from
`config/45410914/symbols.txt`, which is the ruler the split validates against.
Note the two rulers move in BOTH directions (0x827D0A10 is 96 B by report and
84 B by symbols), so this is not a uniform "+4 for padding" fudge.
symbols.txt is READ ONLY -- ab_measure refuses any patch that touches it.

Never "up to the next named symbol", which measured 304 B against a true 88 B on
one MISPIN-1 row.  Rows with no report row are NOT moved: their size would be a
guess, and a pin one row off can EVICT a verified neighbour.

Existing blocks in this file are per-function and NOT merged even when
contiguous (PatchDir has 39 adjacent .text blocks), so inserted blocks follow
that style rather than merging into neighbours.

CHECKS THAT MUST PASS, all executed rather than asserted in prose:
  * byte conservation -- total covered .text bytes identical before and after
    (a move relocates coverage, it never creates or destroys it)
  * zero overlaps globally, after the edit
  * every unit's blocks strictly ascending
  * NO unit drained of its LAST .text block -- an empty unit still emits a
    42-byte obj and report.json then hard-fails with `Invalid COFF/PE section
    headers`; such a unit's whole entry would have to be deleted instead
"""
import argparse
import collections
import json
import os
import re
import sys

SPLITS = 'config/45410914/splits.txt'
SYMBOLS = 'config/45410914/symbols.txt'
FMT = '\t.text       start:0x%08X end:0x%08X'


def symbol_sizes(path=SYMBOLS):
    """-> {va: size} for .text symbols, from dtk's OWN extents.

    This is the ruler the split enforces: a block whose end falls inside a
    symbol aborts the split outright.
    """
    out = {}
    pat = re.compile(r'=\s*\.text:0x([0-9A-Fa-f]+);.*?size:0x([0-9A-Fa-f]+)')
    for ln in open(path):
        m = pat.search(ln)
        if m:
            out[int(m.group(1), 16)] = int(m.group(2), 16)
    return out


def parse(path=SPLITS):
    """-> (lines, {unit: {'hdr': idx, 'other': [str], 'text': [(s,e)]}}, order)"""
    lines = open(path).read().split('\n')
    units = collections.OrderedDict()
    cur = None
    for ln in lines:
        m = re.match(r'^(\S+):\s*$', ln)
        if m:
            cur = m.group(1)
            if cur == 'Sections':
                cur = None
                continue
            units[cur] = dict(other=[], text=[])
            continue
        if cur is None:
            continue
        if not ln.strip():
            cur = None
            continue
        m = re.match(r'^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', ln)
        if m:
            units[cur]['text'].append((int(m.group(1), 16), int(m.group(2), 16)))
        else:
            units[cur]['other'].append(ln)
    return lines, units


def subtract(blocks, lo, hi):
    out = []
    hit = False
    for s, e in blocks:
        if e <= lo or s >= hi:
            out.append((s, e))
            continue
        hit = True
        if s < lo:
            out.append((s, lo))
        if hi < e:
            out.append((hi, e))
    return sorted(out), hit


def total(units):
    return sum(e - s for u in units.values() for s, e in u['text'])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pred', default=os.path.expanduser('~/tmp/pinfix_pred.json'))
    ap.add_argument('--write', action='store_true')
    a = ap.parse_args()

    rows = json.load(open(a.pred))
    moves = [r for r in rows
             if r['defining_units']
             and r['defining_units'][0] != r['pinned_unit']
             and int(r.get('size') or 0) > 0]
    print('[moves] %d rows (movable, priced, distinct destination)' % len(moves))

    lines, units = parse()
    before = total(units)
    print('[before] %d units, %d .text blocks, %d covered bytes'
          % (len(units), sum(len(u['text']) for u in units.values()), before))

    sizes = symbol_sizes()
    nfix = sum(1 for r in moves
               if sizes.get(int(r['addr'], 16), int(r['size'])) != int(r['size']))
    print('[sizing] %d of %d rows take a size from symbols.txt that differs from '
          'report.json' % (nfix, len(moves)))

    applied, skipped = [], []
    for r in moves:
        A = int(r['addr'], 16)
        sz = sizes.get(A)
        if sz is None:
            skipped.append((r['addr'], 'no .text symbol extent in symbols.txt'))
            continue
        su, du = r['pinned_unit'], r['defining_units'][0]
        if su not in units or du not in units:
            skipped.append((r['addr'], 'unit missing from splits'))
            continue
        nb, hit = subtract(units[su]['text'], A, A + sz)
        if not hit:
            skipped.append((r['addr'], 'source block does not cover the span'))
            continue
        units[su]['text'] = nb
        units[du]['text'] = sorted(units[du]['text'] + [(A, A + sz)])
        applied.append(r)

    print('[applied] %d, [skipped] %d' % (len(applied), len(skipped)))
    for s in skipped[:10]:
        print('    skip %s: %s' % s)

    # ------------------------------------------------------------- checks
    after = total(units)
    print('\n[CHECK byte conservation] %d -> %d  %s'
          % (before, after, 'OK' if before == after else '*** FAILED ***'))

    drained = [u for u, d in units.items() if not d['text']]
    print('[CHECK no unit drained of its last .text block] %s'
          % ('OK' if not drained else '*** FAILED: %s ***' % drained))

    flat = []
    for u, d in units.items():
        prev = None
        for s, e in d['text']:
            if prev is not None and s < prev:
                print('*** unit %s not ascending at 0x%08X' % (u, s))
            prev = e
            flat.append((s, e, u))
    flat.sort()
    ov = 0
    for i in range(1, len(flat)):
        if flat[i][0] < flat[i - 1][1]:
            ov += 1
            if ov <= 5:
                print('*** overlap: %s 0x%08X-0x%08X vs %s 0x%08X-0x%08X'
                      % (flat[i - 1][2], flat[i - 1][0], flat[i - 1][1],
                         flat[i][2], flat[i][0], flat[i][1]))
    print('[CHECK zero overlaps] %d overlaps  %s' % (ov, 'OK' if ov == 0 else '*** FAILED ***'))

    if not a.write:
        print('\n(dry run -- pass --write to rewrite splits.txt)')
        return
    if before != after or drained or ov:
        print('\nREFUSING to write: a check failed')
        sys.exit(2)

    # ------------------------------------------------------------- rewrite
    out, cur, buf = [], None, None
    for ln in lines:
        m = re.match(r'^(\S+):\s*$', ln)
        if m and m.group(1) != 'Sections' and m.group(1) in units:
            cur = m.group(1)
            out.append(ln)
            d = units[cur]
            out.extend(d['other'])
            for s, e in d['text']:
                out.append(FMT % (s, e))
            buf = True
            continue
        if buf:
            if not ln.strip():
                out.append(ln)
                buf, cur = None, None
            continue
        out.append(ln)
    open(SPLITS, 'w').write('\n'.join(out))
    print('\nwrote %s' % SPLITS)
    json.dump([r['addr'] for r in applied],
              open(os.path.expanduser('~/tmp/pinfix_applied.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
