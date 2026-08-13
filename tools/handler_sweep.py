#!/usr/bin/env python3
"""Sweep EVERY sub-100 pinned `*::Handle` row: retail's handler ORDER (read from
band.exe .text bytes) vs our BEGIN_HANDLERS block.

WHY THIS EXISTS, AND WHY IT IS NOT tools/handler_order.py
  handler_order.py reads the ADDRESS COLUMN of a dtk `.s` file. For multi-block
  units those columns are SYNTHETIC (dtk computes them as first_block_start +
  cumulative section offset), so they name addresses the function does not live
  at -- and PropAnim.s, one of the rows this sweep had to cover, is multi-block.
  This tool reads the retail PE directly and is keyed on nothing but a VA+size
  from scripts/target_symbol_map.json, so the synthetic-column hazard cannot
  reach it.

THE READER'S ONE LOAD-BEARING CONSTRAINT
  A HANDLE(name, ...) macro emits a lazily-initialised static Symbol whose init
  is always:   lis rX, HI(lit) ; addi r4, rX, LO(lit) ; bl Symbol::Symbol(const char*)
  We accept a literal ONLY when it is passed in r4 to the Symbol ctor.

  The obvious looser reading -- "any lis/addi pair pointing into .rdata" -- is
  MEASURABLY WRONG and fails in the decisive-looking direction. It invented
  'keyboard_wide_frets' for CreditsPanel (whose real set is pause_panel /
  is_cheat_on), reported it 5x for StorePreviewMgr, and produced tail-merged
  suffixes as if they were names ('ong' from 'song', 'ets' from 'frets',
  'brary_unfiltered_fmt' from 'music_library_unfiltered_fmt'). It flagged 6 rows;
  4 of those 6 were pure instrument error. Do not loosen it back.
  (Window sensitivity was checked, not assumed: arm counts are stable for
  window=5..20, so a missed `bl` is not hiding arms.)

THE OUR-SIDE PARSER'S TWO ARTIFACTS -- both cost a lane real time once
  1. `#if` guards. MILO_DEBUG is force-defined tree-wide but HX_NATIVE never is,
     so the house pattern `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` is
     never compiled in the match build. Not stripping it reads those arms as
     EXTRA (this invented CreditsPanel::debug_toggle_autoscroll).
  2. The macro list. HANDLE_ACTION_IF / HANDLE_MESSAGE / HANDLE_SUPERCLASS are
     real handlers; omitting HANDLE_ACTION_IF read VocalTrackDir::rebuild_hud as
     MISSING when we have it in the right place.
  Fixing both retired 4 of the 6 flagged rows. An unflagged row is only as
  trustworthy as this parser -- widen it, don't work around it.

DIRECTIONALITY (inherited from EE2-C, still true)
  Absence of `name`+NUL from the image is ONE-SIDED proof retail cannot have
  that handler. Presence proves nothing, because the linker TAIL-MERGES string
  literals: a name that is a suffix of another is structurally invisible.

RESULT AT THE TIME OF WRITING (2026-08-13, lane EE2-D)
  On the GRADING ruler (functionRelocDiffs=none): 20 of 20 sub-100 pinned Handle
  rows have EXACTLY retail's handler set AND order. ⇒ The handler-block
  divergence class is DRAINED on the pinned population; EE2-C harvested it. The
  productive direction is no longer "which handler is missing" but the BODIES
  the arms call.

  ⚠ WHICH RULER YOU PASS CHANGES THE POPULATION. build/45410914/report.json is
  whichever ruler last wrote it -- an ab_measure run leaves the name_check one
  there. name_check yields 57 rows instead of 20; the extra 37 are already 100%
  on the grading ruler. Nine of them still flag 1-3 handler deltas, and since a
  handler arm is ~28 instructions it CANNOT fit inside their 0.01% residual, so
  those are parser gaps or inert duplicates, NOT missing handlers. Do not work
  them without first proving the delta can fit in the residual.

  ⚠ Our side really does carry duplicated handler lines in places (Game
  num_active_players x2, VocalTrackDir num_vocal_parts x2). They are source
  defects but appear metric-inert -- VocalTrackDir's row reached 100% with the
  duplicate still present.

Usage:  tools/handler_sweep.py [--report build/45410914/report.json]
        # pass the GRADING ruler explicitly if unsure:
        #   ./bin/objdiff-cli report generate -p . -c functionRelocDiffs=none -o /tmp/r.json
        #   tools/handler_sweep.py --report /tmp/r.json
"""
import argparse
import json
import os
import re
import struct
import sys

ROOT = os.environ.get('DECOMP_ROOT') or os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'orig', '45410914', 'band.exe')
TMAP = os.path.join(ROOT, 'scripts', 'target_symbol_map.json')

_data = open(EXE, 'rb').read()


def _sections():
    pe = struct.unpack_from('<I', _data, 0x3C)[0]
    nsec = struct.unpack_from('<H', _data, pe + 6)[0]
    opt = struct.unpack_from('<H', _data, pe + 20)[0]
    base = struct.unpack_from('<I', _data, pe + 24 + 28)[0]
    off = pe + 24 + opt
    secs = []
    for i in range(nsec):
        e = _data[off + 40 * i: off + 40 * (i + 1)]
        vsz, va, rsz, ra = struct.unpack_from('<IIII', e, 8)
        secs.append((va, vsz, ra, rsz))
    return base, secs


BASE, SECS = _sections()


def va2off(va):
    rva = va - BASE
    for sva, vsz, ra, rsz in SECS:
        if sva <= rva < sva + max(vsz, rsz):
            d = rva - sva
            if d < rsz:
                return ra + d
    return None


def cstr(va, maxlen=96):
    o = va2off(va)
    if o is None:
        return None
    s = _data[o:o + maxlen].split(b'\x00')[0]
    try:
        t = s.decode('ascii')
    except UnicodeDecodeError:
        return None
    return t if t and all(32 <= ord(c) < 127 for c in t) else None


_tm = json.load(open(TMAP))
SYM2VA = {}
for _v, _s in _tm.items():
    if isinstance(_s, str) and isinstance(_v, str) and _v.startswith('0x'):
        SYM2VA.setdefault(_s, int(_v, 16))
SYMCTOR = {va for s, va in SYM2VA.items() if s == '??0Symbol@@QAA@PBD@Z'}
assert SYMCTOR, 'no retail VA for Symbol::Symbol(const char*) -- refusing'


def retail_arms(va0, size, window=5):
    """Ordered handler names: literals passed in r4 to the Symbol ctor."""
    o = va2off(va0)
    if o is None:
        return []
    n = size // 4
    w = [struct.unpack_from('>I', _data, o + 4 * i)[0] for i in range(n)]

    def bl_target(i):
        x = w[i]
        if (x >> 26) != 18 or not (x & 1):
            return None
        d = x & 0x03FFFFFC
        if d & 0x02000000:
            d -= 0x04000000
        return (va0 + 4 * i) + (0 if (x & 2) else d)

    out, hi = [], {}
    for i in range(n):
        x = w[i]
        op = x >> 26
        if op == 15:                                   # lis / addis
            rt, ra, imm = (x >> 21) & 31, (x >> 16) & 31, x & 0xFFFF
            if ra == 0:
                hi[rt] = imm
            else:
                hi.pop(rt, None)
        elif op == 14:                                 # addi
            rt, ra, imm = (x >> 21) & 31, (x >> 16) & 31, x & 0xFFFF
            if rt == 4 and ra in hi:
                lo = imm - 0x10000 if imm & 0x8000 else imm
                if any(bl_target(j) in SYMCTOR
                       for j in range(i + 1, min(i + 1 + window, n))):
                    s = cstr((hi[ra] << 16) + lo)
                    if s:
                        out.append(s)
            if rt != ra:
                hi.pop(rt, None)
    return out


# --- CONTROLS: two rows whose true handler list is known from the objdiff
# listing. A reader that cannot reproduce these must not be believed.
_C = [
    (0x827AF620, 496, ['pause_panel', 'is_cheat_on']),                 # CreditsPanel
    (0x8242D0E8, 3136, ['remove_keys', 'has_keys', 'add_keys']),       # RndPropAnim (prefix)
]
for _va, _sz, _exp in _C:
    _got = retail_arms(_va, _sz)
    assert _got[:len(_exp)] == _exp, f'CONTROL FAILED at {_va:#x}: {_got[:5]}'
assert len(retail_arms(0x8242D0E8, 3136)) == 22, 'RndPropAnim arm-count control FAILED'

_GUARDS = [
    re.compile(r'#if\s+defined\(MILO_DEBUG\)\s*&&\s*defined\(HX_NATIVE\)'
               r'(.*?)(?:#else(.*?))?#endif', re.S),
    re.compile(r'#ifdef\s+HX_NATIVE(.*?)(?:#else(.*?))?#endif', re.S),
]
# Match ANY HANDLE* variant whose first argument is a lowercase snake_case
# symbol followed by a comma. Enumerating variants by hand does not work -- the
# tree has 22 of them, and missing just HANDLE_EXPR_STATIC made OvershellSlot
# and ManageBandPanel parse as ZERO handlers, which then reported all 102 / 18 of
# retail's as MISSING. That is the mechanism behind the "large missing lists"
# EE2-C correctly called false positives. The discriminator is the argument
# shape, not the macro name: handler tokens are lowercase and comma-followed,
# while HANDLE_SUPERCLASS/HANDLE_MESSAGE take CamelCase types and HANDLE_CHECK a
# bare number.
_HANDLE = re.compile(r'\bHANDLE[A-Z_]*\s*\(\s*([a-z][A-Za-z0-9_]*)\s*,')


def our_blocks(src_root):
    blocks = {}
    for root, _, files in os.walk(src_root):
        for fn in files:
            if not fn.endswith('.cpp'):
                continue
            p = os.path.join(root, fn)
            try:
                txt = open(p, errors='replace').read()
            except OSError:
                continue
            for m in re.finditer(r'BEGIN_HANDLERS\((.*?)\)(.*?)END_HANDLERS',
                                 txt, re.S):
                body = m.group(2)
                for g in _GUARDS:
                    body = g.sub(lambda mm: mm.group(2) or '', body)
                cls = m.group(1).strip().split('::')[-1]
                blocks.setdefault(cls, (p, _HANDLE.findall(body)))
    return blocks


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--report',
                    default=os.path.join(ROOT, 'build', '45410914', 'report.json'))
    a = ap.parse_args()
    rep = json.load(open(a.report))
    blocks = our_blocks(os.path.join(ROOT, 'src'))

    rows = []
    pat = re.compile(r'\?Handle@([A-Za-z0-9_]+)@@UAA\?AVDataNode@@PAVDataArray@@_N@Z$')
    for u in rep['units']:
        for f in u.get('functions', []):
            m = pat.match(f.get('name', '') or '')
            if not m or float(f.get('fuzzy_match_percent', 0)) >= 100.0:
                continue
            va = SYM2VA.get(f['name'])
            if va is not None:
                rows.append((m.group(1), va, int(f['size']),
                             float(f['fuzzy_match_percent'])))
    rows.sort(key=lambda r: -r[2])

    print(f'{len(rows)} sub-100 pinned Handle rows; {len(blocks)} blocks parsed\n')
    flagged = 0
    for cls, va, size, fz in rows:
        retail = retail_arms(va, size)
        ent = blocks.get(cls)
        if ent is None:
            print(f'??? {cls:26s} {size:6d}B {fz:9.4f}%  NO BLOCK PARSED')
            continue
        ours = ent[1]
        miss = [s for s in retail if s not in set(ours)]
        extra = [s for s in ours if s not in set(retail)]
        order = ('SAME' if [s for s in retail if s in set(ours)]
                 == [s for s in ours if s in set(retail)] else 'REORDERED')
        ok = not miss and not extra and order == 'SAME'
        flagged += 0 if ok else 1
        print(f'{"OK " if ok else ">>>"}{cls:26s} {size:6d}B {fz:9.4f}%  '
              f'retail={len(retail):2d} ours={len(ours):2d} order={order}')
        if miss:
            print(f'      MISSING (retail has, we lack): {miss}')
        if extra:
            print(f'      EXTRA   (we have, retail lacks): {extra}')
    print(f'\n{len(rows) - flagged}/{len(rows)} rows match retail exactly.')


if __name__ == '__main__':
    main()
