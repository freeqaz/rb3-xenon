#!/usr/bin/env python3
"""unnamed_thunk_census.py -- the adjustor thunks that have NO map name at all.

WHY THIS POPULATION IS STRUCTURALLY INVISIBLE
=============================================
`tools/thunk_target_audit.py` and `tools/vtable_order_sweep.py` both iterate over
**map-NAMED** addresses.  An adjustor thunk with no map entry is therefore
invisible to both -- it cannot be "named wrong", so no naming instrument reaches
it -- yet it is exactly as misattributed as the named ones, and it is the *other
half* of every wrong name: when the fingerprint matcher put `?Handle@AppLabel@@$4`
on the wrong one of two sibling thunks, the RIGHT one was left anonymous.

⇒ A `SET_DIFFER` row whose fix requires moving a name onto an unnamed sibling is
NOT repairable by any "rename X to its target's name" operation, which is why
lane SLOTMAP measured **0 free renames** over 269 candidates.  The free repairs
live in this population, not in that one.

THE INSTRUMENT (name-free; needs no oracle and no ruler)
--------------------------------------------------------
**A thunk IS its branch target.**  For an unnamed adjustor thunk T:

  1. T decodes as `lwz r11,-4(rN); subf rN,r11,rN; [addi rN,rN,-M;] b BODY`
  2. T has image-wide fan-in 1, and its single referrer is a vtable slot whose
     `??_R4` Complete Object Locator names owner class C (read from RTTI)
  3. BODY **is** map-named, as a virtual method of C

Then T's correct mangled name is DERIVED, not guessed: take BODY's name, replace
the virtual-access char (`E`/`M`/`U` = private/protected/public virtual) with the
adjustor token `$<0|2|4>PPPPPPPM@<M>@`, and map `??_G` -> `??_E` (a vector
deleting dtor thunk forwards to the scalar one).  `M` is the *measured* `addi`
displacement, encoded in MSVC's number grammar.

★ THE ENCODER IS VALIDATED, NOT ASSERTED.  `--validate` re-derives the name of
every ALREADY-CORRECTLY-NAMED thunk in the map from its body and compares to the
map's spelling.  If the derivation were wrong the reproduction rate would fall,
so this control CAN fail.  Do not trust a run whose control has not been read.

⛔ NAMING IS GATED, AND MOST CANDIDATES FAIL THE GATE
=====================================================
objdiff pairs target<->base **per unit**.  A derived name is only *usable* if the
base obj of the unit the address is PINNED into actually defines that spelling.
Naming a pin-gated address drives its row to a permanent 0%.  So each candidate
is classified:

    FREE_AND_DEFINED   derived spelling unused in the map AND defined in the
                       pinned unit's base obj            <- the only actionable class
    PIN_GATED          spelling not defined in that unit's base obj
    NAME_TAKEN         another address already holds the spelling (needs a swap,
                       so it is a rotation question, not a naming one)
    UNPINNED           address is in no pinned .text block

⚠ Per CLAUDE.md, naming a previously-anonymous address has ZERO call-site upside
(objdiff already FORGIVES a placeholder target name) and real downside if wrong.
The upside here is the separate PAIRING channel (+1 honest row) and, above all,
ACCURACY.  Do not fund this as a byte lever -- lane PINHOME2 measured the whole
adjacent vein at under 200 bytes.

⚠ `map_lint.parse_splits` is BROKEN for this purpose -- it assigns rather than
accumulates, so a multi-block unit is described by its LAST block alone (60.8% of
pinned .text invisible; lane PINHOME2 §2.1).  This tool parses splits.txt at
BLOCK level and keys on FULL PATH, never basename, and self-validates the parse.
"""
import argparse
import bisect
import collections
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

from thunk_target_audit import decode_thunk, vtable_index  # noqa: E402

# access char in the BODY's name -> adjustor-thunk token introducer
ACCESS_TO_THUNK = {'E': '$0', 'M': '$2', 'U': '$4'}
VTORDISP = 'PPPPPPPM@'          # the encoding of -4; decode_thunk only accepts -4


def enc_number(n):
    """MSVC's mangled non-negative number grammar.

    ⛔ MUST REJECT NEGATIVES.  `while n: n >>= 4` NEVER TERMINATES for n < 0 in
    Python (-1 >> 4 == -1), so a negative displacement span-locked the census in
    an infinite string-concatenation loop at 98% CPU with no output -- it looked
    like an O(n^2) scan, not a hang.  The --validate control could NOT catch it:
    no already-NAMED thunk has a positive `addi`, so the pathology exists only
    in the unnamed population the control does not reach.
    """
    if n is None or n < 0:
        return None
    if n == 0:
        return 'A@'
    if 1 <= n <= 10:
        return chr(ord('0') + n - 1)
    s = ''
    while n:
        s = chr(ord('A') + (n & 0xF)) + s
        n >>= 4
    return s + '@'


def thunk_displacement(u32, va):
    """The `addi rN,rN,-M` displacement of the thunk at `va` (0 if absent).

    Returns None for an `addi` that ADDS -- MSVC's adjustor thunk always
    subtracts, so a positive immediate means this is not the modelled form and
    the row must be excluded rather than given a fabricated spelling.
    """
    reg = 3 if u32(va) == 0x8163FFFC else 4
    w = u32(va + 8)
    if (w >> 26) == 14 and ((w >> 21) & 31) == reg and ((w >> 16) & 31) == reg:
        imm = w & 0xFFFF
        if imm & 0x8000:
            imm -= 0x10000
        return -imm if imm <= 0 else None
    return 0


def derive_thunk_name(body_name, disp):
    """Mangled adjustor-thunk spelling for the thunk that tail-calls `body_name`."""
    if not body_name or not body_name.startswith('?'):
        return None
    i = body_name.find('@@')
    if i < 0:
        return None
    head, rest = body_name[:i + 2], body_name[i + 2:]
    if not rest:
        return None
    tok = ACCESS_TO_THUNK.get(rest[0])
    if tok is None:                      # not a virtual member function
        return None
    if head.startswith('??_G'):          # vector deleting dtor thunk -> ??_E
        head = '??_E' + head[4:]
    num = enc_number(disp)
    if num is None:                      # non-modelled (adding) displacement
        return None
    # NB: enc_number already emits its own '@' terminator for the 0 and >10
    # forms.  Appending another one produced a malformed spelling that matched
    # NOTHING -- and therefore classified every candidate PIN_GATED, i.e. it
    # manufactured a clean "this population is unadjudicable" verdict.  The
    # --validate control caught it at 0/1960; do not remove that control.
    return head + tok + VTORDISP + num + rest[1:]


# ---------------------------------------------------------------- splits (BLOCK level)
def parse_splits(path):
    """[(lo, hi, unit_full_path)] -- every .text BLOCK, keyed on FULL PATH."""
    blocks, unit = [], None
    for line in open(path):
        s = line.rstrip('\n')
        if not s.strip() or s.lstrip().startswith('#'):
            continue
        if not s[0].isspace():
            m = re.match(r'^(\S+):\s*$', s.strip())
            if m:
                unit = m.group(1)
            continue
        m = re.match(r'\s*\.text\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)', s)
        if m and unit:
            blocks.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    blocks.sort()
    return blocks


class PinIndex(object):
    def __init__(self, blocks):
        self.b = blocks
        self.starts = [x[0] for x in blocks]

    def at(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i < 0:
            return None
        lo, hi, u = self.b[i]
        return (lo, hi, u) if lo <= va < hi else None


# ---------------------------------------------------------------- COFF
def coff_defined(path):
    """Set of DEFINED symbol names (secnum > 0) in a COFF object."""
    try:
        d = open(path, 'rb').read()
    except IOError:
        return set()
    if len(d) < 20:
        return set()
    _, _, _, psym, nsym = struct.unpack_from('<HHIII', d, 0)
    strt = psym + nsym * 18
    out, i = set(), 0
    while i < nsym:
        off = psym + i * 18
        if off + 18 > len(d):
            break
        raw = d[off:off + 8]
        if raw[:4] == b'\x00\x00\x00\x00':
            so = struct.unpack_from('<I', raw, 4)[0]
            e = d.find(b'\x00', strt + so)
            name = d[strt + so:e].decode('latin1') if e > 0 else ''
        else:
            name = raw.rstrip(b'\x00').decode('latin1')
        _, sec, _, _, naux = struct.unpack_from('<IhHBB', d, off + 8)
        if sec > 0 and name:
            out.add(name)
        i += 1 + naux
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--validate', action='store_true',
                    help='re-derive already-correct thunk names (the control)')
    ap.add_argument('--json')
    ap.add_argument('--limit', type=int, default=40)
    ap.add_argument('--only', help='restrict listing to this class substring')
    args = ap.parse_args()

    os.chdir(args.project_dir)
    from retail_rtti import RetailRtti
    R = RetailRtti()

    # ---- anti-vacuity guards (both predecessor lanes publish these) ----
    g = len(R.word_refs(0x823591e8))
    if g != 2770:
        sys.exit('VACUOUS: word_refs(0x823591e8) = %d, expected 2770' % g)
    m = json.load(open('scripts/target_symbol_map.json'))
    names = {int(k, 16): v for k, v in m.items()
             if k.startswith('0x') and isinstance(v, str)}
    named_thunks = {va: t for va, t in
                    ((va, decode_thunk(R.u32, va)) for va in names) if t is not None}
    # ⚠ This count is NOT invariant -- it is a property of the MAP, and it moves
    # by +1 every time a thunk address is named.  It was 2164 at `dc605388` and
    # 2165 after this lane named 0x825720c8.  Treated as a sanity band, never an
    # equality: an equality here fired on the lane's own correct edit.
    if not 2000 <= len(named_thunks) <= 2400:
        sys.exit('VACUOUS: decode_thunk over map = %d, outside the sanity band'
                 % len(named_thunks))
    # THE guard that matters for a name-keyed analysis: a reflinked worktree's
    # target objs are PRE-RENAMER, so every retail mangled name reads "absent"
    # until the first build.  Prove the renamer ran by finding a renamed symbol
    # in a target obj (dtk emits only `fn_<ADDR>` before the renamer).
    probe_obj = 'build/45410914/obj/MetaPanel.obj'
    probe_sym = '??_EAppLabel@@$4PPPPPPPM@CFM@AAPAXI@Z'
    if probe_sym not in coff_defined(probe_obj):
        sys.exit('VACUOUS: %s does not define %s -- the target objs are '
                 'PRE-RENAMER (build the worktree first)' % (probe_obj, probe_sym))
    print('guards OK: word_refs(0x823591e8)=%d  in-map adjustor thunks=%d  '
          'renamer HAS run (target obj carries a mangled name)'
          % (g, len(named_thunks)))

    held = {}
    for a, n in names.items():
        held.setdefault(n, []).append(a)

    # ---- the encoder control ----
    if args.validate:
        ok = bad = nojudge = 0
        misses = []
        for va, body in sorted(named_thunks.items()):
            bn = names.get(body)
            if not bn:
                nojudge += 1
                continue
            want = derive_thunk_name(bn, thunk_displacement(R.u32, va))
            if want is None:
                nojudge += 1
            elif want == names[va]:
                ok += 1
            else:
                bad += 1
                if len(misses) < 8:
                    misses.append((va, names[va], want))
        tot = ok + bad
        print('\n[control] derived name == map name on ALREADY-NAMED thunks whose '
              'body is also named:')
        print('[control]   reproduced %d / %d (%.1f%%)   not-judgeable %d'
              % (ok, tot, 100.0 * ok / max(tot, 1), nojudge))
        print('[control]   ^ these are the rows the map and the body AGREE on plus '
              'the ones they do not;\n'
              '[control]     a low rate means the ENCODER is wrong and every '
              'derived name below is worthless.')
        for va, got, want in misses:
            print('   0x%08x map=%s\n              derived=%s' % (va, got[:78], want[:78]))

    # ---- the census ----
    blocks = parse_splits('config/45410914/splits.txt')
    units = {b[2] for b in blocks}
    ovl = sum(1 for i in range(len(blocks) - 1) if blocks[i][1] > blocks[i + 1][0])
    print('\nsplits: %d .text blocks / %d units / %d overlaps' % (len(blocks), len(units), ovl))
    if ovl:
        sys.exit('splits parse produced overlaps -- refusing')
    P = PinIndex(blocks)

    cfg = json.load(open('objdiff.json'))
    base_by_unit = {}
    for u in cfg['units']:
        bp = u.get('base_path')
        if bp:
            base_by_unit[os.path.basename(u['name'])] = bp
    objcache = {}

    def base_defines(unit_path, sym):
        key = os.path.splitext(os.path.basename(unit_path))[0]
        bp = base_by_unit.get(key)
        if not bp:
            return None                      # unit has no base obj at all
        if bp not in objcache:
            objcache[bp] = coff_defined(bp)
        return sym in objcache[bp]

    # Image-wide aligned-word fan-in index, built ONCE (a per-candidate
    # R.word_refs() rescan is O(candidates x image) and takes tens of minutes).
    # ★ Cross-validated against R.word_refs on the guard hub below, so the fast
    #   path cannot silently disagree with the shipped instrument.
    fan = collections.Counter()
    for sec in R.sections:
        blk = R.data[sec.rawptr:sec.rawptr + sec.rawsize]
        n = len(blk) - (len(blk) % 4)
        for w in struct.unpack_from('>%dI' % (n // 4), blk, 0):
            if 0x82000000 <= w < 0x83000000:
                fan[w] += 1
    if fan[0x823591e8] != g:
        sys.exit('VACUOUS: fan-in index disagrees with word_refs on the guard hub '
                 '(%d vs %d)' % (fan[0x823591e8], g))
    print('fan-in index built; agrees with word_refs on the guard hub (%d)' % fan[0x823591e8])

    # O(1) .text word reader.  `decode_thunk` only ever reads .text (a thunk
    # body lives there), and RetailRtti.u32 walks the section list on EVERY
    # call, which dominates a scan over every .rdata word.  Cross-checked
    # against R.u32 on a known thunk before use, so the fast path cannot
    # silently disagree with the shipped reader.
    tsec = [s for s in R.sections if s.name == '.text'][0]
    tlo, traw, trs = tsec.va, tsec.rawptr, tsec.rawsize
    twords = struct.unpack_from('>%dI' % (trs // 4), R.data, traw)

    def fu32(va):
        if tlo <= va < tlo + trs and not (va & 3):
            return twords[(va - tlo) >> 2]
        return None

    probe = 0x825720b8   # a known adjustor thunk
    if fu32(probe) != R.u32(probe) or fu32(probe) != 0x8164FFFC:
        sys.exit('VACUOUS: fast .text reader disagrees with RetailRtti.u32')
    print('fast .text reader agrees with RetailRtti.u32 on the probe thunk')

    refs = vtable_index(R)
    print('vtable_index: %d distinct referenced addresses' % len(refs))
    rows, buckets = [], collections.Counter()
    seen = 0
    for va, sites in sorted(refs.items()):
        seen += 1
        if seen % 20000 == 0:
            print('  ... scanned %d/%d referenced addresses' % (seen, len(refs)))
        if va in names:
            continue                          # named -> the other instruments own it
        body = decode_thunk(fu32, va)
        if body is None:
            continue
        buckets['unnamed adjustor thunks referenced by a vtable'] += 1
        owners = {c for c, _, _ in sites}
        fanin = fan[va]
        bn = names.get(body)
        if fanin != 1 or len(owners) != 1:
            buckets['  excluded: fan-in>1 or multiple vtable owners'] += 1
            continue
        if not bn:
            buckets['  excluded: BODY UNNAMED (identification wall)'] += 1
            continue
        want = derive_thunk_name(bn, thunk_displacement(fu32, va))
        if want is None:
            buckets['  excluded: body name is not a virtual member function'] += 1
            continue
        buckets['ADJUDICABLE (fan-in 1, one vtable owner, named virtual body)'] += 1
        p = P.at(va)
        pb = P.at(body)
        holder = held.get(want, [])
        if holder:
            cls = 'NAME_TAKEN'
        elif p is None:
            cls = 'UNPINNED'
        else:
            d = base_defines(p[2], want)
            cls = 'FREE_AND_DEFINED' if d else ('PIN_GATED' if d is False else 'NO_BASE_OBJ')
        buckets['    -> ' + cls] += 1
        rows.append({
            'thunk': '0x%08x' % va, 'body': '0x%08x' % body, 'body_name': bn,
            'derived': want, 'cls': cls,
            'owner': sorted(owners)[0], 'slot': sites[0][1],
            'thunk_unit': p[2] if p else None,
            'body_unit': pb[2] if pb else None,
            'same_unit': bool(p and pb and p[2] == pb[2]),
            'held_by': ['0x%08x' % h for h in holder],
        })

    print()
    for k, v in buckets.most_common():
        print('  %-62s %5d' % (k, v))

    show = [r for r in rows if r['cls'] == 'FREE_AND_DEFINED']
    if args.only:
        show = [r for r in show if args.only in r['owner'] or args.only in r['derived']]
    print('\n=== FREE_AND_DEFINED -- derived name is unused AND defined in the '
          'pinned unit\'s base obj ===')
    for r in show[:args.limit]:
        print('%s  %s[%d]   pin=%s' % (r['thunk'], r['owner'], r['slot'], r['thunk_unit']))
        print('        body %s %s' % (r['body'], r['body_name'][:76]))
        print('        name %s' % r['derived'][:88])
    if len(show) > args.limit:
        print('... and %d more' % (len(show) - args.limit))

    if args.json:
        json.dump({'rows': rows, 'buckets': dict(buckets)}, open(args.json, 'w'), indent=1)
        print('\nwrote %s' % args.json)


if __name__ == '__main__':
    main()
