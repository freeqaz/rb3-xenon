#!/usr/bin/env python3
"""Lane MISPIN-1: apply the evidenced `.text` span moves (+ their map rows).

WHY BOTH FILES MOVE TOGETHER, AND WHY THAT IS STILL "ONE CHANGE"
----------------------------------------------------------------
STEPS-1 handed these rows over as MIS-PINS: the proposed name is defined in a
different unit than the splits pin, so a rename ALONE strands it (-1 function,
+0 bytes) and a splits move ALONE leaves the moved address wearing the WRONG
name in the destination unit (which does not define the incumbent) -- also
unpaired.  The two edits are not independent levers; they are one repair with
two halves, and either half applied by itself measures worse than doing
nothing.  So the patch carries both, and it is one change.

THE FOUR CHANNELS EVERY LANDED ROW CLEARS
  1. DEFINEDNESS, whole-tree.  The proposed name's defining set is computed over
     all 1,204 objs, not against a suspected pair.  Only SINGLETON sets are
     landed: on a multi-definer COMDAT a two-unit probe confirms whichever unit
     you point it at (measured here at 2 and 3 definers on the deferred rows).
  2. RETAIL IDENTITY.  Decode the thunk at A, follow its tail branch to T, and
     require T to be the class::method the proposed name claims.  Non-circular:
     0 of 8 destinations are themselves suspect rows.
  3. DESTINATION OWNERSHIP.  The unit whose pin CONTAINS T must be the unit we
     are moving A into.  This is independent of (1) and (2) and it is what
     splits the deferred pair -- two thunks of the SAME class whose real methods
     are pinned to two DIFFERENT units.
  4. GEOMETRY.  A must sit in a HOLE or hard against a boundary of the
     destination unit's pinned range.

SIZING.  `.pdata` cannot size these: an adjustor thunk is an 8-byte-class leaf
touching neither stack nor LR, so it carries NO unwind record (0 of 8 are
`.pdata` BeginAddresses).  The ruler is the thunk body -- measured identical on
all 8: lwz r11,-4(r3) / subf r3,r11,r3 / b <method> / padding = 16 B.  The
report sizes the SYMBOL at 12 B; the existing pins for these same thunks are
16 B blocks, and this file follows that.

⚠ `.pdata` lines in splits.txt are DERIVED OUTPUT -- re-derived from `.text` on
every split run.  Only `.text` is written here.
"""
import argparse
import json
import re
import sys

SPLITS = 'config/45410914/splits.txt'
MAP = 'scripts/target_symbol_map.json'

# addr -> (proposed name, source unit, destination unit)
# Only rows clearing ALL FOUR channels.  Deferred rows and their reasons live in
# DEFERRED below so the refusal is recorded next to the acceptance.
LAND = {
    0x82319C30: ('??_EMiniLeaderboardDisplay@@$4PPPPPPPM@A@AAPAXI@Z',
                 'ScoreDisplay.cpp', 'system/hamobj/MiniLeaderboardDisplay.cpp'),
    0x8231E748: ('??_EReviewDisplay@@$4PPPPPPPM@A@AAPAXI@Z',
                 'StarDisplay.cpp', 'system/bandobj/ReviewDisplay.cpp'),
    0x823BA1E0: ('?SyncProperty@CharWeightSetter@@$4PPPPPPPM@A@AA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z',
                 'FlowValueCase.cpp', 'CharWeightSetter.cpp'),
    0x82482340: ('?SyncProperty@RndScreenMask@@$4PPPPPPPM@A@AA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z',
                 'Ribbon.cpp', 'ScreenMask.cpp'),
    0x8273E4D0: ('??_EDxMovie@@$4PPPPPPPM@A@AAPAXI@Z',
                 'system/rnddx9/CubeTex.cpp', 'system/rnddx9/Movie.cpp'),
}
THUNK = 0x10

DEFERRED = {
    0x822ABA00: 'defining set MULTI {OutfitConfig, ExternalMic, Gem}; channel 3 puts the '
                'branch destination 0x822AB8A0 in Gem.cpp while its sibling row 0x822ABA30 '
                'lands in OutfitConfig.cpp -- the region\'s pins are internally inconsistent',
    0x822ABA30: 'defining set MULTI {OutfitConfig, ExternalMic, Gem}; block sits exactly in a '
                'Gem hole while the names say OutfitConfig, and its sibling contradicts it',
    0x82809038: 'defining set MULTI {PanelDir, UISlider}; channels 2+3 say PanelDir but the '
                'GEOMETRY (which is what speaks to the contributing TU) abuts UISlider, and '
                'the block is a mixed multi-class thunk cluster whose pin may be correct',
}


def parse(path=SPLITS):
    """-> [(kind, payload)] preserving every byte we do not deliberately change."""
    out, unit = [], None
    for ln in open(path):
        m = re.match(r'^(\S+):\s*$', ln)
        if m:
            unit = m.group(1)
            out.append(('unit', unit, ln))
            continue
        m = re.match(r'^(\s+)\.text(\s+)start:0x([0-9A-Fa-f]+)(\s+)end:0x([0-9A-Fa-f]+)(.*)$', ln)
        if m and unit:
            out.append(('text', unit, ln, int(m.group(3), 16), int(m.group(5), 16), m))
            continue
        out.append(('raw', unit, ln))
    return out


def fmt(indent, sep1, s, sep2, e, tail):
    return '%s.text%sstart:0x%08X%send:0x%08X%s\n' % (indent, sep1, s, sep2, e, tail.rstrip('\n'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()

    rows = parse()
    # locate the block containing each address, assert it is owned by the stated source
    plan = {}
    for A, (name, src, dst) in LAND.items():
        hit = [r for r in rows if r[0] == 'text' and r[3] <= A < r[4]]
        assert len(hit) == 1, 'address 0x%08X is in %d blocks' % (A, len(hit))
        r = hit[0]
        assert r[1] == src, '0x%08X is pinned to %s, expected %s' % (A, r[1], src)
        assert r[4] >= A + THUNK, '0x%08X: block ends before the thunk does' % A
        plan[A] = r
        print('0x%08X  %-30s block 0x%08X-0x%08X -> carve [0x%08X,0x%08X) to %s'
              % (A, src, r[3], r[4], A, A + THUNK, dst))

    # rewrite .text lines
    add = {}
    for A, (name, src, dst) in LAND.items():
        add.setdefault(dst, []).append((A, A + THUNK))
    out = []
    for r in rows:
        if r[0] != 'text':
            out.append(r[2])
            continue
        _, unit, ln, s, e, m = r
        ind, s1, s2, tail = m.group(1), m.group(2), m.group(4), m.group(6)
        cut = [A for A in plan if plan[A] is r]
        if not cut:
            out.append(ln)
            continue
        A = cut[0]
        # emit the surviving head/tail of the source block; the middle goes away
        if s < A:
            out.append(fmt(ind, s1, s, s2, A, tail))
        if A + THUNK < e:
            out.append(fmt(ind, s1, A + THUNK, s2, e, tail))
        if s == A and A + THUNK == e:
            print('  (block was exactly the thunk -- source line removed entirely)')
    # insert the new blocks into their destination units
    final, i = [], 0
    while i < len(out):
        final.append(out[i])
        i += 1
    text = ''.join(final)
    for dst, blocks in add.items():
        mu = re.search(r'^%s:\s*$' % re.escape(dst), text, re.M)
        assert mu, 'destination unit %s not found in splits.txt' % dst
        # append after the unit's LAST .text line so ordering stays grouped
        start = mu.end()
        nxt = re.search(r'^\S+:\s*$', text[start:], re.M)
        end = start + (nxt.start() if nxt else len(text) - start)
        seg = text[start:end]
        # `.text` lines are ADDRESS-SORTED within a unit throughout this file;
        # appending after the last one would break that ordering (and make the
        # diff unreadable), so insert at the sorted position.
        pos, last = None, None
        for m in re.finditer(r'^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x[0-9A-Fa-f]+.*$\n?',
                             seg, re.M):
            last = m
            if pos is None and int(m.group(1), 16) > min(b[0] for b in blocks):
                pos = m.start()
        assert last, 'no .text line in %s' % dst
        at = start + (last.end() if pos is None else pos)
        ins = ''.join('\t.text       start:0x%08X end:0x%08X\n' % b for b in sorted(blocks))
        text = text[:at] + ins + text[at:]
        print('  + %s  %s' % (dst, ' '.join('0x%08X-0x%08X' % b for b in sorted(blocks))))

    mp = json.load(open(MAP))
    for A, (name, src, dst) in LAND.items():
        k = '0x%08x' % A
        assert k in mp, '%s missing from the map' % k
        print('  map %s  %s -> %s' % (k, mp[k][:44], name[:44]))
        mp[k] = name

    print('\nDEFERRED (ownership not established -- left exactly as found):')
    for A, why in sorted(DEFERRED.items()):
        print('  0x%08X  %s' % (A, why))

    if a.dry_run:
        print('\n--dry-run: nothing written')
        return
    open(SPLITS, 'w').write(text)
    with open(MAP, 'w') as fh:                 # json.dump omits the trailing
        json.dump(mp, fh, indent=1)            # newline the file ships with
        fh.write('\n')
    print('\nwrote %s and %s' % (SPLITS, MAP))


if __name__ == '__main__':
    main()
