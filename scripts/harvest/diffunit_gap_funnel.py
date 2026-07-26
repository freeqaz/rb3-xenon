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

SCOPE -- source path, NOT an address window  [CORRECTED 2026-07-26, laneGAPFILL]
-------------------------------------------------------------------------------
This funnel used to hard-skip the VA window `0x82800000 .. 0x82D00000` as
"XDK + Quazal".  **That guard was wrong and it was expensive.**  It is a
proxy: it assumes vendor code occupies a contiguous address range.  It does
not.  That window also contains a large amount of plainly matchable, already-
pinned, already-compiled Milo/game code -- measured occupants include `UI`,
`UIListWidget`, `UIPicture`, `UIListDir`, `UILabel`, `GemTrack`, `Mesh`,
`CameraManager`, `TourDescPanel`, `Tail`, `VorbisReader`, `Track`, `Lyric`.

Concretely: laneGAPFILL's interior-gap sweep found **77% of all content-bearing
interior-gap bytes inside this window**, and **150 of its 181 landed matches
came from inside it**.  laneAL's "interior holes: exhausted" verdict was
therefore only ever true OUTSIDE the window.

The guard is now the honest test: **a unit is out of scope iff its own
`source_path` classifies as a no-oracle tier** (`xdk` / `vendor`) per
`tools/scope_map.bucket_for_source`.  Address is not evidence of provenance.
Note this correctly keeps `src/network/` (Quazal) IN scope as `game` -- it is
low *priority* per the owner, but it is not vendor, and conflating the two is
what produced the window in the first place.

`--legacy-window` restores the old VA guard for A/B comparison only.

★ TRAP -- STALE `auto_03_*_text.s` ASM
--------------------------------------
Anything that reads dtk's per-unit asm for *content* evidence (strings, callees)
must filter those files by **mtime against `build/45410914/config.json`**.  A
warm worktree carries thousands of stale blocks from earlier split states beside
the live ones -- measured **4,426 stale vs 2,504 live**, one of them dated 13
days earlier and spanning 55 KB straight across current pins.  Reading them
unfiltered produces *false content evidence*: it attributed XDK
`xgraphics\\ucode\\compiler\\ir\\block.cpp` strings to a `DuplicatedObject` gap
that the live asm proves is DuplicatedObject's own code (it carries the literal
`.\\DuplicatedObject.cpp` MILO_FAIL path string).  That nearly caused a correct
span to be rejected.

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
    """LEGACY VA-window guard.  Kept only for --legacy-window A/B.  See the
    SCOPE note in the module docstring: address is not evidence of provenance."""
    return not (hi <= VENDOR_LO or lo >= VENDOR_HI)


# --- source-path scope test (replaces the VA window) ----------------------
NO_ORACLE_BUCKETS = {"xdk", "vendor"}


def unit_source_paths(report_path):
    """-> {objdiff unit name: source_path} from report.json metadata."""
    out = {}
    try:
        rep = json.load(open(report_path))
    except OSError:
        return out
    for u in rep.get("units", []):
        sp = (u.get("metadata") or {}).get("source_path")
        if sp:
            out[u["name"]] = sp
            out[u["name"].split("/")[-1]] = sp
    return out


def _bucket_for_source():
    """Import tools/scope_map.bucket_for_source lazily (it is the single
    source-path classifier the project already trusts)."""
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    root = os.path.dirname(here)
    if root not in sys.path:
        sys.path.insert(0, root)
    try:
        from tools.scope_map import bucket_for_source
        return bucket_for_source
    except Exception:
        try:
            sys.path.insert(0, os.path.join(root, "tools"))
            from scope_map import bucket_for_source
            return bucket_for_source
        except Exception:
            return lambda sp: None


def make_scope_test(report_path, legacy=False):
    """-> fn(gap) -> True if the gap is OUT of scope."""
    if legacy:
        return lambda g: in_vendor(g['va_lo'], g['va_hi'])
    srcs = unit_source_paths(report_path)
    bucket = _bucket_for_source()

    def _unit_out(unit):
        # splits keys are like 'UI.cpp' or 'band3/bandtrack/Track.cpp';
        # objdiff keys are 'default/<stem>'.  Try the splits path first --
        # it IS a source path once prefixed with src/.
        for cand in (unit, 'src/' + unit, 'src/system/' + unit):
            b = bucket(cand)
            if b:
                return b in NO_ORACLE_BUCKETS
        stem = os.path.basename(unit).rsplit('.', 1)[0]
        sp = srcs.get('default/' + stem) or srcs.get(stem)
        b = bucket(sp) if sp else None
        return bool(b) and b in NO_ORACLE_BUCKETS

    def _test(g):
        # a gap is out of scope only when BOTH fences are no-oracle units:
        # either fence being real code makes the span attributable.
        return _unit_out(g['left_unit']) and _unit_out(g['right_unit'])

    return _test


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--out')
    ap.add_argument('--interior-out')
    ap.add_argument('--quiet', action='store_true')
    ap.add_argument('--legacy-window', action='store_true',
                    help='use the old 0x82800000-0x82D00000 VA guard (A/B only)')
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

    out_of_scope = make_scope_test(report, legacy=a.legacy_window)
    interior, diffunit, vendor = [], [], 0
    for g in gaps:
        if out_of_scope(g):
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
        print(f"  out-of-scope excluded  : {vendor}"
              + ("  [LEGACY VA WINDOW]" if a.legacy_window else "  [source-path test]"))
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
