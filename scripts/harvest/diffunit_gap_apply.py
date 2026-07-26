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

USAGE
  diffunit_gap_apply.py --worktree WT --gaps gaps.json --dir left|right
                        [--select sel.json] [--dry]
  diffunit_gap_apply.py --worktree WT --audit
"""
import argparse
import collections
import json
import os
import re
import sys


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


def apply_subranges(path, lines, sel, dry):
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
    a = ap.parse_args()

    path = os.path.join(a.worktree, 'config/45410914/splits.txt')
    lines = read_splits(path)

    if a.audit:
        f = audit(lines)
        print('\n'.join(f) if f else 'AUDIT CLEAN: 0 overlaps, 0 inversions, '
                                     '0 duplicate blocks, 0 sectionless blocks')
        return 1 if f else 0

    if a.subranges:
        return apply_subranges(path, lines, json.load(open(a.subranges)), a.dry)

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
        open(path, 'w').write('\n'.join(lines))
        print('wrote', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
