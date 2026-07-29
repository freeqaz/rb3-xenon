#!/usr/bin/env python3
"""diffunit_gap_apply.py -- assign DIFFERENT-UNIT gaps to a neighbour, safely.

A gap is a maximal unclaimed `.text` interval fenced by two DIFFERENT pinned
units (see diffunit_gap_funnel.py).  Exactly two attributions are geometrically
available: give it to the LEFT neighbour (extend that block's `end`) or to the
RIGHT neighbour (pull that block's `start` back).  Because the gap abuts both
blocks, either edit is overlap-free by construction.

This tool applies one direction for a chosen subset of gaps, in one atomic
rewrite of splits.txt, and then AUDITS the whole file:
  * 0 cross-unit overlaps
  * 0 inversions (start >= end)
  * 0 duplicate unit blocks
  * 0 sectionless unit blocks  <-- dtk emits a stub obj and
    `objdiff-cli report generate` HARD-FAILS ("Invalid COFF/PE section
    headers"), producing no report.json at all.  `.pdata` does NOT count: it is
    dtk back-fill derived from our own `.text`.
It refuses to write on any finding.

REFUSAL CRITERION 1 IS SCOPED TO THE SYMBOL, NOT THE GAP  [laneGAPFILL 2026-07-26]
---------------------------------------------------------------------------------
Criterion 1 -- "a mapped, name-paired symbol inside the span that the claimant's
obj does not define" -- was being applied by discarding the WHOLE gap.  That is
over-broad.  The correct scope is **the refused symbol's own byte interval**:

  absorb = gap  minus  union([va, va+size) for each refused symbol)

then snap each surviving sub-span to whole carved-function boundaries (start on
a function start, end on a function end) and drop any sub-span left holding no
complete function.  New `.text` lines are emitted for the survivors.

Measured: 11 sub-spans / 5,076 B recovered this way from 15 criterion-1 gaps,
**+10 strict with 0 losses**, on top of the 170-span main wave.  The refused
symbols really are foreign (`CBaseSkin@LEAPCORE`, `CCfgEngineBase@NUISPEECH`,
`Pipeline@ST`, `??_EStreamRecorder`) -- but the code *around* them in the same
gap is the claimant's, and three of those gaps carry the claimant's own
unit-specific Symbol strings to prove it.

Criterion 2 (claimant already owns a byte-identical 100% symbol of the same
mangled name) fired **0** times across 1,025 gaps.  Criterion 3
(`n_carved_in_span == 0`) selects the alignment-padding gaps -- 851 of the 1,025,
~7 KB, holding no code at all and worth exactly 0.

★★ MANDATORY: THE OVER-SUBSCRIPTION GATE  [laneOVERSUB 2026-07-29]
------------------------------------------------------------------
Absorbing a span is the classic way to manufacture FAKE matches.  objdiff's
funclet pass 2b (`pair_funclets_by_bytes`, objdiff commit 48a5255) credits an
anonymous target funclet 100% by re-using an **already-consumed** byte-identical
base funclet.  So a span that swallows N byte-identical `__unwind$` / `??__F`
thunks while the claimant's own obj emits only M < N of them scores N -- N-M of
which are machine code we never generated.

Tree-wide audit on `559645e9`: **1,565 of 39,520 matched functions (3.96%) are
pass-2b surplus**, over 196 units and 138 splits commits.  See
`scripts/harvest/oversub_guard.py` for the mechanism, the supply rule, and the
instrumented-objdiff validation.

This tool now REFUSES TO WRITE if it cannot see the guard, and on every write it
snapshots the pre-landing census and prints the verify command.  The gate is:

    # (this tool does this for you on --gaps/--subranges writes)
    scripts/harvest/oversub_guard.py --worktree WT \
        --baseline build/45410914/oversub_baseline.json
    ... edit splits.txt, touch config.yml, rm target_symbol_renames.stamp, ninja ...
    scripts/harvest/oversub_guard.py --worktree WT \
        --verify build/45410914/oversub_baseline.json     # exit 3 = fake matches

A landing that fails the gate must drop the offending spans, or be re-priced by
its honest delta (raw credited MINUS the over-subscription growth) before it is
reported.  `--no-oversub-gate` skips the snapshot and prints a loud warning; use
it only when you are deliberately measuring the inflation.

USAGE
  diffunit_gap_apply.py --worktree WT --gaps gaps.json --dir left|right
                        [--select sel.json] [--dry]
  diffunit_gap_apply.py --worktree WT --audit
  diffunit_gap_apply.py --worktree WT --oversub-verify [--allow N]
"""
import argparse
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import oversub_guard
except ImportError:                                     # pragma: no cover
    oversub_guard = None

OVERSUB_BASELINE = 'build/45410914/oversub_baseline.json'


def snapshot_oversub(worktree, enabled):
    """Snapshot the PRE-landing over-subscription census and print the gate.

    Returns False if the guard is unavailable and the caller must refuse to write.
    """
    if not enabled:
        print('\n!! OVER-SUBSCRIPTION GATE DISABLED (--no-oversub-gate).'
              '  This landing may manufacture fake matches; you MUST report its'
              ' honest delta separately.\n')
        return True
    if oversub_guard is None:
        print('\nREFUSING TO WRITE: scripts/harvest/oversub_guard.py not importable.'
              '\nSpan absorption without the over-subscription gate manufactures'
              ' fake matches (measured 1,565 tree-wide on 559645e9).'
              '\nRe-run with --no-oversub-gate only if you know what you are doing.\n')
        return False
    out = os.path.join(worktree, OVERSUB_BASELINE)
    try:
        c = oversub_guard.census(worktree, False)
    except Exception as e:                              # pragma: no cover
        print('\nREFUSING TO WRITE: over-subscription census failed (%s).'
              '  Build the objs first (./tools/ninja-locked).\n' % e)
        return False
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump(c, open(out, 'w'), indent=0, sort_keys=True)
    tot = sum(v['excess'] for v in c.values())
    print(oversub_guard.BANNER)
    print('pre-landing baseline: %d units / %d fake matches -> %s' % (len(c), tot, out))
    print('AFTER you rebuild (touch config/45410914/config.yml; '
          'rm -f build/45410914/target_symbol_renames.stamp; ./tools/ninja-locked) run:')
    print('    scripts/harvest/oversub_guard.py --worktree %s --verify %s'
          % (worktree, OVERSUB_BASELINE))
    print('exit 3 means the landing bought fake matches -- drop those spans or '
          're-price the landing.\n')
    return True


def read_splits(path):
    return open(path).read().split('\n')


def parse_blocks(lines):
    """-> list of (lineno, unit, section, start, end)"""
    out = []
    cur = None
    for i, ln in enumerate(lines):
        if ln and not ln[0].isspace() and ln.rstrip().endswith(':'):
            cur = ln.rstrip()[:-1]
            continue
        m = re.match(r'^(\s+)(\S+)(\s+)start:(0x[0-9A-Fa-f]+)(\s+)end:(0x[0-9A-Fa-f]+)\s*$', ln)
        if m and cur and cur != 'Sections':
            out.append((i, cur, m.group(2), int(m.group(4), 16), int(m.group(6), 16)))
    return out


def audit(lines):
    blocks = parse_blocks(lines)
    findings = []
    # inversions + duplicates
    seen = collections.Counter()
    for i, unit, sec, s, e in blocks:
        if s >= e:
            findings.append(f"INVERSION {unit} {sec} {s:#x}-{e:#x} (line {i+1})")
        seen[(unit, sec, s, e)] += 1
    for k, v in seen.items():
        if v > 1:
            findings.append(f"DUPLICATE x{v}: {k[0]} {k[1]} {k[2]:#x}-{k[3]:#x}")
    # cross-unit overlaps per section
    bysec = collections.defaultdict(list)
    for i, unit, sec, s, e in blocks:
        bysec[sec].append((s, e, unit))
    for sec, lst in bysec.items():
        lst.sort()
        for a, b in zip(lst, lst[1:]):
            if a[1] > b[0]:
                findings.append(
                    f"OVERLAP {sec}: {a[2]} {a[0]:#x}-{a[1]:#x} vs {b[2]} {b[0]:#x}-{b[1]:#x}")
    # sectionless unit blocks
    units_with = collections.defaultdict(set)
    for i, unit, sec, s, e in blocks:
        units_with[unit].add(sec)
    cur = None
    declared = set()
    for ln in lines:
        if ln and not ln[0].isspace() and ln.rstrip().endswith(':'):
            u = ln.rstrip()[:-1]
            if u != 'Sections':
                declared.add(u)
    for u in declared:
        if '.text' not in units_with.get(u, ()):
            findings.append(f"SECTIONLESS(.text) unit block: {u}")
    return findings


def apply_subranges(path, lines, sel, dry, worktree='.', gate=True):
    """Apply per-gap PREFIX/SUFFIX claims from diffunit_subrange.py.

    Each record may carry `p` (left claims [va_lo, p)) and/or `q` (right claims
    [q, va_hi)).  Both edits are overlap-free because the gap abuts both blocks
    and `p <= q` is guaranteed by the selector.  A gap may be claimed from BOTH
    ends at once -- the middle simply stays unowned, which is the point: we
    claim only what the evidence covers.
    """
    blocks = parse_blocks(lines)
    idx = {}
    for i, unit, sec, s, e in blocks:
        if sec == '.text':
            idx.setdefault((unit, s, e), []).append(i)

    # A block can legitimately be edited from BOTH sides: it may be the LEFT
    # neighbour of one gap (its `end` grows) and the RIGHT neighbour of another
    # (its `start` shrinks).  Accumulate per block instead of first-wins.
    edits, skipped = {}, []
    for r in sel:
        if r.get('p') is not None:
            key = (r['left_unit'], r['left_block'][0], r['left_block'][1])
            ls = idx.get(key)
            if not ls or len(ls) != 1:
                skipped.append((r['va_lo'], 'left', 'block missing/ambiguous'))
            elif not key[1] < r['p'] <= r['va_hi']:
                skipped.append((r['va_lo'], 'left', 'cut out of range'))
            else:
                cur = edits.setdefault(ls[0], [key[1], key[2]])
                cur[1] = max(cur[1], r['p'])
        if r.get('q') is not None:
            key = (r['right_unit'], r['right_block'][0], r['right_block'][1])
            ls = idx.get(key)
            if not ls or len(ls) != 1:
                skipped.append((r['va_lo'], 'right', 'block missing/ambiguous'))
            elif not r['va_lo'] <= r['q'] < key[2]:
                skipped.append((r['va_lo'], 'right', 'cut out of range'))
            else:
                cur = edits.setdefault(ls[0], [key[1], key[2]])
                cur[0] = min(cur[0], r['q'])

    for ln, (s, e) in edits.items():
        m = re.match(r'^(\s+)(\S+)(\s+)start:(0x[0-9A-Fa-f]+)(\s+)end:(0x[0-9A-Fa-f]+)\s*$',
                     lines[ln])
        lines[ln] = (f"{m.group(1)}{m.group(2)}{m.group(3)}start:0x{s:08X}"
                     f"{m.group(5)}end:0x{e:08X}")

    findings = audit(lines)
    print(f"subrange records {len(sel)}  block edits {len(edits)}  skipped {len(skipped)}")
    for s in skipped[:10]:
        print('  SKIP', s)
    if findings:
        print(f"AUDIT FAILED ({len(findings)}), refusing to write:")
        for f in findings[:20]:
            print('  ', f)
        return 2
    print('AUDIT CLEAN')
    if not dry:
        if not snapshot_oversub(worktree, gate):
            return 3
        open(path, 'w').write('\n'.join(lines))
        print('wrote', path)
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--gaps')
    ap.add_argument('--subranges', help='diffunit_subrange.py selection JSON')
    ap.add_argument('--dir', choices=['left', 'right'])
    ap.add_argument('--select', help='JSON list of gap indices, or of {va_lo,dir}')
    ap.add_argument('--dry', action='store_true')
    ap.add_argument('--audit', action='store_true')
    ap.add_argument('--oversub-verify', action='store_true',
                    help='run the over-subscription gate against %s' % OVERSUB_BASELINE)
    ap.add_argument('--allow', type=int, default=0,
                    help='tolerated growth in fake matches for --oversub-verify')
    ap.add_argument('--no-oversub-gate', action='store_true',
                    help='skip the over-subscription snapshot (LOUD warning)')
    a = ap.parse_args()

    path = os.path.join(a.worktree, 'config/45410914/splits.txt')

    if a.oversub_verify:
        if oversub_guard is None:
            print('oversub_guard.py not importable')
            return 2
        base = json.load(open(os.path.join(a.worktree, OVERSUB_BASELINE)))
        cur = oversub_guard.census(a.worktree, False)
        b_tot = sum(v['excess'] for v in base.values())
        c_tot = sum(v['excess'] for v in cur.values())
        print(oversub_guard.BANNER)
        print('baseline %d fake -> current %d fake (delta %+d, allow %d)'
              % (b_tot, c_tot, c_tot - b_tot, a.allow))
        for name, v in sorted(cur.items(), key=lambda kv: -kv[1]['excess']):
            d = v['excess'] - base.get(name, {}).get('excess', 0)
            if d > 0:
                print('  GREW %-50s +%d fake' % (name, d))
        if c_tot - b_tot > a.allow:
            print('\nREFUSED: landing manufactures %d fake matches.' % (c_tot - b_tot))
            return 3
        print('\nOK: no over-subscription growth.')
        return 0

    lines = read_splits(path)

    if a.audit:
        f = audit(lines)
        print('\n'.join(f) if f else 'AUDIT CLEAN: 0 overlaps, 0 inversions, '
                                     '0 duplicate blocks, 0 sectionless blocks')
        return 1 if f else 0

    if a.subranges:
        return apply_subranges(path, lines, json.load(open(a.subranges)), a.dry,
                               a.worktree, not a.no_oversub_gate)

    gaps = json.load(open(a.gaps))
    if a.select:
        sel = json.load(open(a.select))
        if sel and isinstance(sel[0], dict):
            want = {(int(x['va_lo']), x['dir']) for x in sel}
            plan = [(g, d) for g in gaps for d in ('left', 'right')
                    if (g['va_lo'], d) in want]
        else:
            plan = [(gaps[i], a.dir) for i in sel]
    else:
        plan = [(g, a.dir) for g in gaps if g.get('n_fns', 0) > 0]

    # index blocks by (unit, start, end)
    blocks = parse_blocks(lines)
    idx = {}
    for i, unit, sec, s, e in blocks:
        if sec == '.text':
            idx.setdefault((unit, s, e), []).append(i)

    edits = {}   # lineno -> (new_start, new_end)
    skipped = []
    for g, d in plan:
        if d == 'left':
            key = (g['left_unit'], g['left_block'][0], g['left_block'][1])
        else:
            key = (g['right_unit'], g['right_block'][0], g['right_block'][1])
        ls = idx.get(key)
        if not ls or len(ls) != 1:
            skipped.append((g['va_lo'], d, 'block not found/ambiguous'))
            continue
        ln = ls[0]
        if ln in edits:
            skipped.append((g['va_lo'], d, 'block already edited'))
            continue
        if d == 'left':
            edits[ln] = (key[1], g['va_hi'])
        else:
            edits[ln] = (g['va_lo'], key[2])

    for ln, (s, e) in edits.items():
        m = re.match(r'^(\s+)(\S+)(\s+)start:(0x[0-9A-Fa-f]+)(\s+)end:(0x[0-9A-Fa-f]+)\s*$',
                     lines[ln])
        lines[ln] = (f"{m.group(1)}{m.group(2)}{m.group(3)}start:0x{s:08X}"
                     f"{m.group(5)}end:0x{e:08X}")

    findings = audit(lines)
    print(f"planned {len(plan)}  applied {len(edits)}  skipped {len(skipped)}")
    for s in skipped[:10]:
        print('  SKIP', s)
    if findings:
        print(f"AUDIT FAILED ({len(findings)}), refusing to write:")
        for f in findings[:20]:
            print('  ', f)
        return 2
    print('AUDIT CLEAN')
    if not a.dry:
        if not snapshot_oversub(a.worktree, not a.no_oversub_gate):
            return 3
        open(path, 'w').write('\n'.join(lines))
        print('wrote', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
