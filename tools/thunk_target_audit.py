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
Measured at `56b82629` over all 1,560 adjustor thunks in the map, and
independently REPRODUCED there by a second lane before it acted on the output:

    CONSISTENT  (the control) 1,293 rows -- 1,287 at fuzzy==100  (99.5%)
    INCONSISTENT                132 rows --     4 at fuzzy==100  ( 3.0%)

A 33x separation on a column this audit never reads, and the control could have
failed: if flagging were noise the two rates would agree.  128 of the 132 are
below 100 and every one of them is exactly 12 B => the whole vein is 1,536 B.

**After this lane's own repairs the same run reads 1,552 thunks / CONSISTENT
1,305 (99.5%) / INCONSISTENT 116 (3.4%) / vein 1,344 B** -- i.e. 12 rows moved
INCONSISTENT -> CONSISTENT and 8 misnamed holders were nulled.

⛔ **EVERY FIGURE ABOVE WAS COMPUTED OVER 72% OF THE POPULATION** -- `decode_thunk`
handled only the 3-instruction adjustor form and missed 611 thunks (28.2%); see
its docstring.  Re-measured at `05ff76aa` with the generalized decoder
(lane SLOTMAP, 2026-08-31), whole binary, freshly built tree:

    adjustor thunks in map                             2,164   (was 1,553)
    CONSISTENT  (the control)  1,835 rows -- 1,827 at fuzzy==100  (99.6%)
    INCONSISTENT                 113 rows --     4 at fuzzy==100  ( 3.1%)
    TARGET_UNNAMED                199   IRREDUCIBLE (fold hub)  17
    vein size                  1,564 B across 126 sub-100 rows

The separation is **32x** and the control could still have failed, so the
instrument survives the population correction intact -- what changed is its
REACH, never its validity.  That is what you should see today.

⚠ Re-run `--validate` and reproduce the numbers before acting.  They MOVE as the
map is corrected, so treat any figure quoted here or in a brief as a hypothesis
(the original write-up's 1,561/1,292/134/1,560 predated two earlier repairs).
A run where the two rates CONVERGE means this instrument has stopped
discriminating -- that is a reason to stop, not a clean tree.

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

⛔⛔ CORRECTIONS FROM THE FIRST LANE THAT USED THIS (THUNK-105, 2026-08-24)
==========================================================================
Everything above survived; these four things the FIRST WRITE-UP GOT WRONG did
not.  They are here rather than in a doc because the failure mode is acting on
the flag list directly.

1. **"105 rows are RTTI-adjudicable" IS WRONG -- it is 51.  Do not inherit it.**
   Of the 132 INCONSISTENT: **53** are same-class method-only mismatches (RTTI
   gives the CLASS, never which METHOD), **17** have an RTTI owner matching
   NEITHER name, **11** are referenced from >1 vtable (owner ambiguous), and
   **51** are the shape the write-up described.  I generated the 105 with a
   looser predicate than the one I documented.

2. ★★ **objdiff pairs target<->base PER UNIT.**  "Our tree defines this spelling
   somewhere" is NOT the check -- the **base obj of the unit that owns THAT
   ADDRESS** must define it.  Getting this wrong unpaired 9 rows permanently,
   and **the graded ruler could not see it** (they sat at 98.33 contributing 0
   bytes, so unpairing cost 0).  Only the `none` control showed it, at exactly
   -108 B = 9 x 12.  ⇒ **on a map patch, read the `none` control even though it
   is `NOT_APPLICABLE` for alias adjudication.**

3. **The vein is PIN-GATED, not naming-gated.**  28 of the 45 single-candidate
   shape-A rows have the thunk and its own body in **different pinned units** --
   verified against `config/45410914/splits.txt`, which is name-independent:
   `0x82289748` is inside `Line.cpp`'s `.text` range while its body
   `0x822896e0`, 0x68 earlier, is inside `BandCharacter.cpp`'s.  A thunk belongs
   to its body's TU, so **a pin boundary is wrong**, and renaming drives the row
   to a PERMANENT 0%.  Fixing those is a pin re-homing lane (⚠ re-homing is NOT
   metric-neutral -- CLAUDE.md), not a naming lane.

4. ★★★ **A WRONG NAME CAN COLLECT BYTES AT A FALSE 100 WHILE THE RIGHT ADDRESS
   IS CHARGED.**  `0x822d9fd8` carries `?SetType@Waypoint@@$4...` and scores a
   clean **100.0** -- but it is referenced from `StreakMeter[8]` and branches to
   an **UNNAMED** target, and objdiff's `is_placeholder_symbol_name` FORGIVES a
   placeholder callee, so the name is never charged.  The real Waypoint thunk is
   `0x823dc040` (referenced from `Waypoint[5]`, branching to
   `?SetType@Waypoint@@UAAXVSymbol@@@Z`) and it scored 98.33.  ⇒ **a 100.0 row is
   not evidence its name is right**, and correcting these SHOWS AS A REGRESSION.

★ **A STRONGER DETECTOR EXISTS THAN THE ONE THIS FILE IMPLEMENTS.**  For a thunk
referenced by exactly ONE vtable, the thunk's class must simply EQUAL that
vtable's RTTI owner.  That needs **no target name at all**, so it also judges the
**135 TARGET_UNNAMED** rows this audit must skip -- and it proves **125** thunks
misnamed, vs 132 INCONSISTENT here.  It is what unlocked the blocked chains.
⚠ It does not judge a class's SECONDARY subobject vtables, whose COL names the
same class (so thunk-class == owner-class trivially) -- see blind spot 1.

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
    """Branch target of the adjustor thunk at `va`, or None if not one.

    ⛔ THIS USED TO HANDLE ONLY THE 3-INSTRUCTION FORM AND UNDERCOUNTED BY 28.2%
    (lane SLOTMAP, 2026-08-31).  MSVC emits a FOURTH instruction whenever the
    adjustment beyond the vtordisp is nonzero:

        lwz  r11,-4(rN)
        subf rN,r11,rN
        addi rN,rN,-M        <-- present only when M != 0; WAS NOT DECODED
        b    <the real body>

    Measured over `scripts/target_symbol_map.json` on retail `band.exe`:
    3-insn-only decoder finds **1,553** thunks, this one finds **2,164** -- so
    **611 adjustor thunks (28.2%) were invisible**, and every `--validate`
    figure in the docstring above was computed over 72% of the population.
    The missed rows are exactly the ones whose mangled name carries a nonzero
    displacement token (`$4PPPPPPPM@DM@`, `@CCE@`, `@BHI@` ...) rather than
    `$4PPPPPPPM@A@`; reading them as "not a thunk" is silent, and it biased the
    census toward the zero-displacement (primary-base) stratum.

    ⚠ The `addi` must be recognised by SHAPE (opcode 14 with RT==RA==the same
    `this` register), not by a fixed word: M varies per class, so a constant
    would rebuild the same blind spot one displacement narrower.
    """
    reg = ADJ_LOAD.get(u32(va))
    if reg is None or u32(va + 4) != ADJ_SUBF[reg]:
        return None
    at = va + 8
    w = u32(at)
    if w is None:
        return None
    # optional `addi rN,rN,-M` -- opcode 14, RT == RA == reg
    if (w >> 26) == 14 and ((w >> 21) & 31) == reg and ((w >> 16) & 31) == reg:
        at += 4
        w = u32(at)
        if w is None:
            return None
    if (w >> 26) != BRANCH_OP or (w & 1):   # unconditional `b`; LK set is a CALL
        return None
    off = w & 0x03FFFFFC
    if off & 0x02000000:
        off -= 0x04000000
    return (at + off) & 0xFFFFFFFF


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
