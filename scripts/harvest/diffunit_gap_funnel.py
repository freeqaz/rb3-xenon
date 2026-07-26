#!/usr/bin/env python3
"""diffunit_gap_funnel.py -- enumerate DIFFERENT-UNIT unclaimed `.text` gaps.

CONTEXT
-------
`config/45410914/splits.txt` pins per-object `.text` ranges.  Every maximal
address interval that no unit claims is a *gap*.  laneAL
(docs/plans/lane-al-autocarve-2026-07-26.md) split the gap set in two:

  * **interior** -- the SAME unit is pinned on both sides.  Retail packs a TU
    contiguously, so an interior hole is overwhelmingly that unit's own
    unpinned code.  laneAL swept all 1,084 of these: +2,287, 53.6% hit rate.
  * **different-unit** -- the gap is fenced by TWO DIFFERENT units.  There is
    NO contiguity argument here; retail genuinely interleaves TUs, and a prior
    lane measured 87.2% of such holes to be *genuine COMDAT scatter*, i.e. not
    a defect at all.  laneAL deliberately declined to sweep these.

This tool enumerates ONLY the second class, which is laneAM's pool.

DERIVE FROM splits.txt, NEVER FROM report.json
----------------------------------------------
dtk COALESCES unowned regions into larger `auto_03_<VA>_text` units whose ends
do not coincide with a pin.  Deriving candidate gaps from those unit boundaries
undercounted the real gap set by 27x (laneAL's first, wrong, 153-function
ceiling).  So: gaps come from splits.txt intervals; report.json is used ONLY to
census which functions live inside a gap.

SCOPE
-----
`0x82800000 .. 0x82D00000` is XDK + Quazal, hard-skipped by the project owner
(58% of the raw pool).  Excluded INSIDE the funnel, never after.

OUTPUT
------
JSON list of gap records:
  {va_lo, va_hi, size, left_unit, right_unit, left_block, right_block,
   n_fns, fns:[{va,name,size,matched}], named_fns:[...]}

USAGE
-----
  diffunit_gap_funnel.py --worktree WT [--out gaps.json] [--interior-out i.json]
"""
import argparse
import json
import os
import re
import sys
import collections

VENDOR_LO = 0x82800000
VENDOR_HI = 0x82D00000


def parse_splits(path):
    """-> (blocks, unit_sections)

    blocks: list of dicts {unit, section, start, end, lineno} for .text only
    """
    blocks = []
    cur = None
    with open(path) as f:
        for i, ln in enumerate(f):
            m = re.match(r'^(\S[^\s:]*(?:\s[^\s:]*)*):\s*$', ln)
            if m and not ln.startswith('\t') and not ln.startswith(' '):
                cur = m.group(1)
                continue
            m = re.match(r'^\s+(\S+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)', ln)
            if m and cur and cur != 'Sections':
                sec, s, e = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
                if sec == '.text':
                    blocks.append({'unit': cur, 'section': sec,
                                   'start': s, 'end': e, 'lineno': i})
    return blocks


def gap_set(blocks):
    """Maximal unclaimed intervals between consecutive pinned .text blocks."""
    b = sorted(blocks, key=lambda x: (x['start'], x['end']))
    gaps = []
    for i in range(len(b) - 1):
        prev, nxt = b[i], b[i + 1]
        # guard: overlaps / inversions should not exist; report if they do
        if prev['end'] > nxt['start']:
            continue
        if prev['end'] == nxt['start']:
            continue
        gaps.append({'va_lo': prev['end'], 'va_hi': nxt['start'],
                     'size': nxt['start'] - prev['end'],
                     'left_unit': prev['unit'], 'right_unit': nxt['unit'],
                     'left_block': (prev['start'], prev['end']),
                     'right_block': (nxt['start'], nxt['end'])})
    return gaps


def symbols_map(symbols_path):
    """-> {name: (va, size)} from dtk's config/45410914/symbols.txt.

    Lines look like `Name = .text:0x8226ABCD; // type:function size:0x40 ...`.
    """
    out = {}
    pat = re.compile(r'^(\S+)\s*=\s*\.text:(0x[0-9A-Fa-f]+);.*?size:(0x[0-9A-Fa-f]+)')
    try:
        fh = open(symbols_path)
    except OSError:
        return out
    with fh:
        for ln in fh:
            m = pat.match(ln)
            if m:
                out[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return out


def map_name_to_va(map_path):
    """-> {mangled_name: va} inverted from scripts/target_symbol_map.json.

    dtk's `symbols.txt` only ever names functions `fn_<VA>`; the mangled names
    that appear in `report.json` are painted on by the target-symbol renamer, so
    the only place to recover their address is the map itself.  Meta keys
    (`_comment`, `_icf_arbitrary`, `_bijection_arbitrary`, ...) are not VAs.
    """
    out = {}
    try:
        m = json.load(open(map_path))
    except OSError:
        return out
    for va, name in m.items():
        if not va.startswith('0x') or not isinstance(name, str):
            continue
        out.setdefault(name, int(va, 16))
    return out


def report_fns(report_path, symbols_path=None, map_path=None):
    """-> sorted list of (va, name, size, matched, unit) for auto_03 units.

    ! Do NOT derive the VA as `auto-unit base + report 'address'`.  That is wrong:
    measured against the authoritative addresses, only 1,917 of 17,801 anonymous
    functions agree; the rest drift low by +4..+40 and more, because inter-symbol
    alignment padding is not represented in the report's address field.  Cuts
    computed from those VAs land INSIDE real symbols and dtk hard-fails the build
    with "Split <unit> .text (...) ends within symbol 'fn_XXXXXXXX'".

    The authoritative VA is in the NAME for anonymous functions (`fn_<8hex>` is
    the retail address dtk carved it at); for the handful of named ones we look
    the symbol up in `config/45410914/symbols.txt`, and skip it if absent.
    """
    r = json.load(open(report_path))
    syms = symbols_map(symbols_path) if symbols_path else {}
    mapva = map_name_to_va(map_path) if map_path else {}
    out, unresolved = [], 0
    for u in r['units']:
        if not re.match(r'^default/auto_03_[0-9A-Fa-f]{8}_text$', u['name']):
            continue
        for f in u.get('functions', []):
            name, size = f['name'], int(f['size'])
            m = re.match(r'^fn_([0-9A-Fa-f]{8})$', name)
            if m:
                # symbols.txt is authoritative for BOTH address and size; the
                # report's size can disagree where dtk padded the symbol.
                va, size = syms.get(name, (int(m.group(1), 16), size))
            elif name in syms:
                va, size = syms[name]
            elif name in mapva:
                va = mapva[name]
            else:
                unresolved += 1
                continue
            out.append((va, name, size,
                        f.get('match_percent_normalized') == 100.0, u['name']))
    out.sort()
    if unresolved:
        print(f"  (skipped {unresolved} functions with no resolvable address)",
              file=sys.stderr)
    return out


def in_vendor(lo, hi):
    return not (hi <= VENDOR_LO or lo >= VENDOR_HI)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--out')
    ap.add_argument('--interior-out')
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args()

    wt = a.worktree
    splits = os.path.join(wt, 'config/45410914/splits.txt')
    report = os.path.join(wt, 'build/45410914/report.json')

    blocks = parse_splits(splits)
    gaps = gap_set(blocks)
    fns = report_fns(report, os.path.join(wt, 'config/45410914/symbols.txt'),
                     os.path.join(wt, 'scripts/target_symbol_map.json'))
    fva = [x[0] for x in fns]
    import bisect

    interior, diffunit, vendor = [], [], 0
    for g in gaps:
        if in_vendor(g['va_lo'], g['va_hi']):
            vendor += 1
            continue
        lo = bisect.bisect_left(fva, g['va_lo'])
        hi = bisect.bisect_left(fva, g['va_hi'])
        g['fns'] = [{'va': v, 'name': n, 'size': s, 'matched': mm}
                    for v, n, s, mm, _ in fns[lo:hi]]
        g['n_fns'] = len(g['fns'])
        g['n_named'] = sum(1 for f in g['fns'] if not f['name'].startswith('fn_'))
        (interior if g['left_unit'] == g['right_unit'] else diffunit).append(g)

    if not a.quiet:
        print(f"pinned .text blocks      : {len(blocks)}")
        print(f"raw gaps                 : {len(gaps)}")
        print(f"  vendor-window excluded : {vendor}")
        print(f"  interior (same unit)   : {len(interior):5d}  "
              f"fns {sum(g['n_fns'] for g in interior)}")
        print(f"  DIFFERENT-UNIT         : {len(diffunit):5d}  "
              f"fns {sum(g['n_fns'] for g in diffunit)}")
        nz = [g for g in diffunit if g['n_fns']]
        print(f"  ...with >=1 function   : {len(nz):5d}")
        sz = collections.Counter()
        for g in nz:
            sz[min(g['n_fns'], 20)] += 1
        print("  fn-count histogram (20=20+):",
              ', '.join(f"{k}:{v}" for k, v in sorted(sz.items())))
        print(f"  named fns in pool      : {sum(g['n_named'] for g in diffunit)}")

    if a.out:
        json.dump(diffunit, open(a.out, 'w'))
    if a.interior_out:
        json.dump(interior, open(a.interior_out, 'w'))


if __name__ == '__main__':
    main()
