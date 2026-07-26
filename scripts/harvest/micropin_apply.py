#!/usr/bin/env python3
"""micropin_apply.py -- insert parentage-evidenced `.text` micro-pins into splits.txt.

Input: {"<unit header>": [["0xSTART","0xEND"], ...]} produced by
pdata_parent_owner.py -> the EH funclets whose PARENT function is already pinned
in that unit.  A funclet is emitted while compiling its parent's TU, so the
parent's unit is a hard owner (not a similarity score).

Insertion policy per unit: union the unit's existing `.text` intervals with the
new ones, coalesce only EXACTLY-ADJACENT/overlapping intervals *within the same
unit* (never claims a byte the union did not already claim), and rewrite that
unit's `.text` lines in place, preserving the file's column formatting.

Audit (reused verbatim from diffunit_gap_apply.audit): refuses to write on any
cross-unit overlap, inversion, duplicate block or sectionless (.text-less) unit
block.  Additionally refuses if any *new* interval overlaps an existing pinned
`.text` range of a DIFFERENT unit.

USAGE
  micropin_apply.py --worktree WT --pins micropins.json [--dry]
"""
import argparse
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from diffunit_gap_apply import parse_blocks, audit, read_splits  # noqa: E402

LINE_RE = re.compile(
    r'^(\s+)(\S+)(\s+)start:(0x[0-9A-Fa-f]+)(\s+)end:(0x[0-9A-Fa-f]+)\s*$')


def coalesce(ivs):
    ivs = sorted(ivs)
    out = []
    for s, e in ivs:
        if out and s <= out[-1][1]:
            out[-1][1] = max(out[-1][1], e)
        else:
            out.append([s, e])
    return [(s, e) for s, e in out]


def merge_minimal(old_iv, new_iv):
    """Union old+new, but coalesce ONLY runs that contain a new interval.

    Pre-existing adjacent blocks are left exactly as they were (minimum churn:
    we do not want to conflate this wave with an unrelated block-merge edit).
    """
    tagged = sorted([(s, e, 0) for s, e in old_iv] + [(s, e, 1) for s, e in new_iv])
    out = []
    run = []
    for iv in tagged:
        if run and iv[0] <= max(x[1] for x in run):
            run.append(iv)
        else:
            out.extend(_flush(run))
            run = [iv]
    out.extend(_flush(run))
    return sorted(out)


def _flush(run):
    if not run:
        return []
    if any(t == 1 for _, _, t in run):
        return [(min(s for s, _, _ in run), max(e for _, e, _ in run))]
    return [(s, e) for s, e, _ in run]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', default='.')
    ap.add_argument('--pins', required=True)
    ap.add_argument('--only-units', help='JSON list of unit headers to restrict to')
    ap.add_argument('--dry', action='store_true')
    a = ap.parse_args()

    path = os.path.join(a.worktree, 'config/45410914/splits.txt')
    lines = read_splits(path)
    pins = json.load(open(a.pins))
    if a.only_units:
        keep = set(json.load(open(a.only_units)))
        pins = {k: v for k, v in pins.items() if k in keep}

    pre = audit(lines)
    if pre:
        print(f'PRE-AUDIT FAILED ({len(pre)}) -- file already dirty, refusing:')
        for f in pre[:20]:
            print('  ', f)
        return 3

    blocks = parse_blocks(lines)
    declared = {u for _, u, _, _, _ in blocks}
    text_by_unit = collections.defaultdict(list)   # unit -> [(lineno, s, e)]
    for i, unit, sec, s, e in blocks:
        if sec == '.text':
            text_by_unit[unit].append((i, s, e))

    # global existing .text map for cross-unit overlap pre-check
    existing = sorted((s, e, u) for i, u, sec, s, e in blocks if sec == '.text')

    problems = []
    for unit, ivs in pins.items():
        if unit not in declared:
            problems.append(f'UNKNOWN UNIT HEADER: {unit}')
            continue
        if not text_by_unit.get(unit):
            problems.append(f'UNIT HAS NO .text BLOCK: {unit}')
    for unit, ivs in pins.items():
        for lo, hi in ivs:
            s, e = int(lo, 16), int(hi, 16)
            if s >= e:
                problems.append(f'INVERTED NEW BLOCK {unit} {s:#x}-{e:#x}')
            for xs, xe, xu in existing:
                if xs < e and s < xe and xu != unit:
                    problems.append(
                        f'NEW BLOCK {unit} {s:#x}-{e:#x} OVERLAPS PINNED {xu} {xs:#x}-{xe:#x}')
    if problems:
        print(f'PRECHECK FAILED ({len(problems)}), refusing to write:')
        for p in problems[:20]:
            print('  ', p)
        return 2

    # build new per-unit .text sets
    n_new_bytes = 0
    n_lines_before = 0
    n_lines_after = 0
    out_lines = list(lines)
    del_lines = set()
    insert_at = {}   # lineno -> [rendered lines]

    for unit, ivs in pins.items():
        old = text_by_unit[unit]
        old_iv = [(s, e) for _, s, e in old]
        new_iv = [(int(lo, 16), int(hi, 16)) for lo, hi in ivs]
        n_new_bytes += sum(e - s for s, e in new_iv)
        merged = merge_minimal(old_iv, new_iv)
        # sanity: merged must cover exactly union
        cov_old = sum(e - s for s, e in coalesce(old_iv))
        cov_new = sum(e - s for s, e in merged)
        if cov_new < cov_old:
            print(f'BUG: coverage shrank for {unit}')
            return 4
        # take the formatting from the unit's first .text line
        tmpl = LINE_RE.match(lines[old[0][0]])
        pre_ws, sec, mid, _, mid2, _ = tmpl.groups()
        rendered = [f'{pre_ws}{sec}{mid}start:0x{s:08X}{mid2}end:0x{e:08X}'
                    for s, e in merged]
        n_lines_before += len(old)
        n_lines_after += len(rendered)
        for ln, _, _ in old:
            del_lines.add(ln)
        insert_at[old[0][0]] = rendered

    final = []
    for i, ln in enumerate(out_lines):
        if i in insert_at:
            final.extend(insert_at[i])
        if i in del_lines:
            continue
        final.append(ln)

    findings = audit(final)
    print(f'units {len(pins)}  new blocks {sum(len(v) for v in pins.values())}  '
          f'new bytes {n_new_bytes}  .text lines {n_lines_before} -> {n_lines_after}')
    if findings:
        print(f'AUDIT FAILED ({len(findings)}), refusing to write:')
        for f in findings[:20]:
            print('  ', f)
        return 2
    print('AUDIT CLEAN')
    if not a.dry:
        open(path, 'w').write('\n'.join(final))
        print('wrote', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
