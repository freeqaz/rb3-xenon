#!/usr/bin/env python3
"""thunk_target_audit.py -- find map rows where an MSVC adjustor thunk and the
function it branches to CANNOT both be named correctly.

THE INSTRUMENT
==============
A virtual-base adjustor thunk is three instructions:

    lwz  r11, -4(rN)        # load the vtordisp
    subf rN,  r11, rN       # adjust `this`
    b    <the real body>    # tail-call

**A thunk IS its branch target.**  So if `target_symbol_map.json` names the
thunk `?Foo@C@@$4...` and names the branch target `?Bar@D@@U...`, one of those
two rows is wrong.  That conclusion needs no oracle, no declaration order, no
name-multiset alignment, and no confidence margin -- which is what makes it
worth having next to the weaker instruments listed in vtable_order_sweep.py.

WHY IT PAYS
-----------
objdiff charges the tail-call relocation BY NAME under `name_check`, so a
misnamed thunk scores 98.333336 -- one charged element in a 12-byte body -- and
because `matched_code` is all-or-nothing per row it contributes ZERO bytes.
Every such row is 12 B that a single correct name collects.

SELF-VALIDATION (`--validate`) -- reproduce this before trusting a run
----------------------------------------------------------------------
Measured 2026-08-22 at 914d1822 over all 1,561 adjustor thunks in the map:

    CONSISTENT  (the control) 1,292 rows -- 1,286 at fuzzy==100  (99.5%)
    INCONSISTENT                134 rows --     4 at fuzzy==100  ( 3.0%)

A 33x separation on a column this audit never reads, and the control could have
failed: if flagging were noise the two rates would agree.  130 of the 134 are
below 100 and every one of them is exactly 12 B => the whole vein is 1,560 B.

⛔⛔ THE FLAG IS A DETECTOR, NOT A REPAIR RECIPE
================================================
The audit proves ONE OF THE TWO NAMES IS WRONG.  It does NOT say which.

Renaming the thunk after its target makes the relocation agree and therefore
lifts `name_check` **by construction, whether or not the new name is right** --
which is bit-for-bit the ALIAS_SUSPECT metric-fitting shape CLAUDE.md warns
about ("an unproven alias lifts the score by construction and is an integrity
hazard").  A bulk pass over these 134 rows would buy ~1.5 kB and teach us
nothing about which spelling retail meant.

⇒ Each row needs INDEPENDENT evidence.  The two that worked (lane vtable-w10,
   0x8268e3a8 and 0x8268e398) were settled by:
     * the target's BODY SHAPE against each candidate's SIGNATURE -- Cymbal's
       target returns an int field of a global (`lwz r3,0x9c(r3)`), which a
       `const vector<u64>&` getter cannot be; and
     * a sibling class whose thunk name AND target agree, pinning the slot.
   So this tool reports, for every flagged thunk, WHICH VTABLE SLOT REFERENCES
   IT -- because the owning class + slot index is the evidence you need, and
   looking it up by hand is what makes people skip the step.

TWO BLIND SPOTS, BOTH MEASURED
------------------------------
1. **A thunk and its target misnamed TOGETHER read CONSISTENT.**  Real example
   from the same cluster: `0x8268e468` is named
   `?GetLocalBandUser@LocalBandUser@@$4...` and branches to `0x8268dd50`, named
   `?GetLocalBandUser@LocalBandUser@@UAAPAV1@XZ` -- perfectly consistent, and
   both wrong: the thunk is referenced from slot 21 of the 30-slot **User**
   subobject table, so it is not a BandUser virtual at all (and that 104-byte
   "GetLocalBandUser" scores 5.4%).  A CONSISTENT verdict is NOT a clean bill.
2. **A target that is an ICF fold hub has an arbitrary survivor name**, so the
   disagreement is expected rather than a defect.  Those are reported in their
   own bucket and are NOT a worklist -- which name the slot meant was destroyed
   by the fold.

Usage:
  python3 tools/thunk_target_audit.py --validate
  python3 tools/thunk_target_audit.py --class BandUser
  python3 tools/thunk_target_audit.py --json out.json
"""
import argparse
import bisect
import collections
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

# lwz r11,-4(r3) / lwz r11,-4(r4) -- the second form appears on functions whose
# `this` is argument 2 because the return is an sret struct (e.g. DataNode Handle).
ADJ_LOAD = {0x8163FFFC: 3, 0x8164FFFC: 4}
ADJ_SUBF = {3: 0x7C6B1850, 4: 0x7C8B2050}
BRANCH_OP = 18


def prefix(sym):
    """`?Name@Class` -- everything before the signature token."""
    if not sym:
        return None
    i = sym.find('@@')
    p = sym[:i] if i > 0 else sym
    # a vector-deleting-dtor thunk legitimately forwards to the scalar one
    if p.startswith('??_E'):
        p = '??_G' + p[4:]
    return p


def decode_thunk(u32, va):
    """Branch target of the adjustor thunk at `va`, or None if not one."""
    reg = ADJ_LOAD.get(u32(va))
    if reg is None or u32(va + 4) != ADJ_SUBF[reg]:
        return None
    i2 = u32(va + 8)
    if (i2 >> 26) != BRANCH_OP:
        return None
    off = i2 & 0x03FFFFFC
    if off & 0x02000000:
        off -= 0x04000000
    return (va + 8 + off) & 0xFFFFFFFF


def vtable_index(R):
    """{thunk_va: [(class, slot), ...]} -- who references each address.

    The owning class comes from the `??_R4` Complete Object Locator that MSVC
    places at vtable[-1], so it is read from RTTI and not guessed.
    """
    starts, owner = [], {}
    for sec in R.sections:
        if sec.name != '.rdata':
            continue
        raw = R.data[sec.rawptr:sec.rawptr + sec.rawsize]
        for off in range(0, len(raw) - 3, 4):
            w = struct.unpack_from('>I', raw, off)[0]
            if not R.is_image_va(w):
                continue
            c = R.decode_col(w)
            if not R._col_is_plausible(c):
                continue
            n = R.td_name(c.ptd)
            if n and n.startswith('.?A'):
                va = sec.va + off + 4
                starts.append(va)
                owner[va] = n
    starts.sort()
    refs = collections.defaultdict(list)
    for sec in R.sections:
        if sec.name != '.rdata':
            continue
        raw = R.data[sec.rawptr:sec.rawptr + sec.rawsize]
        for off in range(0, len(raw) - 3, 4):
            w = struct.unpack_from('>I', raw, off)[0]
            if not R.is_image_va(w):
                continue
            va = sec.va + off
            i = bisect.bisect_right(starts, va) - 1
            if i >= 0:
                b = starts[i]
                refs[w].append((owner[b], (va - b) // 4, b))
    return refs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--validate', action='store_true',
                    help='cross-check flags against report.json (the control)')
    ap.add_argument('--class', dest='cls', help='restrict to one class name')
    ap.add_argument('--json')
    ap.add_argument('--limit', type=int, default=30)
    args = ap.parse_args()

    os.chdir(args.project_dir)
    from retail_rtti import RetailRtti
    R = RetailRtti()
    m = json.load(open('scripts/target_symbol_map.json'))
    names = {int(k, 16): v for k, v in m.items()
             if k.startswith('0x') and isinstance(v, str)}

    thunks = {}
    for va in names:
        t = decode_thunk(R.u32, va)
        if t is not None:
            thunks[va] = t

    # hub proxy: a target reached by more than one DISTINCT thunk identity is an
    # ICF survivor, so its map name is arbitrary and disagreement proves nothing
    by_target = collections.defaultdict(set)
    for va, t in thunks.items():
        by_target[t].add(prefix(names[va]))

    refs = vtable_index(R)
    buckets = collections.Counter()
    flagged, hubbed = [], []
    for va, t in sorted(thunks.items()):
        tn = names.get(t)
        if tn is None:
            buckets['TARGET_UNNAMED (cannot judge)'] += 1
            continue
        if prefix(names[va]) == prefix(tn):
            buckets['CONSISTENT (control; NOT a clean bill)'] += 1
            continue
        row = {'thunk': '0x%08x' % va, 'thunk_name': names[va],
               'target': '0x%08x' % t, 'target_name': tn,
               'referenced_by': ['%s[%d]' % (c, s) for c, s, _ in refs.get(va, [])]}
        if len(by_target[t]) > 1:
            buckets['IRREDUCIBLE (target is a fold hub)'] += 1
            hubbed.append(row)
        else:
            buckets['INCONSISTENT (adjudicate; do NOT bulk-rename)'] += 1
            flagged.append(row)

    if args.cls:
        keep = lambda r: args.cls in r['thunk_name'] or args.cls in r['target_name']
        flagged = [r for r in flagged if keep(r)]
        hubbed = [r for r in hubbed if keep(r)]

    print('adjustor thunks found in map : %d' % len(thunks))
    for k, v in buckets.most_common():
        print('  %-46s %6d' % (k, v))

    if args.validate:
        r = json.load(open('build/45410914/report.json'))
        sc, sz = {}, {}
        for u in r['units']:
            for f in u.get('functions', []):
                if f.get('name'):
                    sc[f['name']] = float(f.get('fuzzy_match_percent', 0) or 0)
                    sz[f['name']] = int(f.get('size', 0))
        fl = {r_['thunk_name'] for r_ in flagged} | {r_['thunk_name'] for r_ in hubbed}
        co = {names[va] for va, t in thunks.items()
              if names.get(t) and prefix(names[va]) == prefix(names[t])}

        def rate(group):
            have = [sc[s] for s in group if s in sc]
            return len(have), sum(1 for x in have if x >= 100.0)

        fn, fh = rate(fl)
        cn, ch = rate(co)
        print('\n[validate] FLAGGED    : %4d rows in report, %4d at fuzzy==100 (%.1f%%)'
              % (fn, fh, 100.0 * fh / max(fn, 1)))
        print('[validate] CONSISTENT : %4d rows in report, %4d at fuzzy==100 (%.1f%%)'
              % (cn, ch, 100.0 * ch / max(cn, 1)))
        print('           ^ CONSISTENT is the CONTROL.  If flagging were noise these '
              'two rates would agree; a run where they DO agree means this\n'
              '             instrument has stopped discriminating -- do not use its '
              'output until you find out why.')
        vein = sum(sz.get(s, 0) for s in fl if sc.get(s, 0) < 100.0)
        print('[validate] vein size  : %d B across %d sub-100 rows'
              % (vein, sum(1 for s in fl if s in sc and sc[s] < 100.0)))

    print('\n=== INCONSISTENT -- one of the two names is wrong; ADJUDICATE each ===')
    print('    (`referenced_by` is the evidence: owning class + slot, read from RTTI)')
    for row in flagged[:args.limit]:
        print('%s %s' % (row['thunk'], row['thunk_name'][:86]))
        print('        -> %s %s' % (row['target'], row['target_name'][:86]))
        if row['referenced_by']:
            print('           slot: %s' % ', '.join(row['referenced_by'][:6]))
    if len(flagged) > args.limit:
        print('... and %d more' % (len(flagged) - args.limit))

    if args.json:
        json.dump({'inconsistent': flagged, 'irreducible_fold_hub': hubbed,
                   'buckets': dict(buckets)}, open(args.json, 'w'), indent=1)
        print('\nwrote %s' % args.json)


if __name__ == '__main__':
    main()
