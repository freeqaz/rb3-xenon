#!/usr/bin/env python3
"""Apply homing_gen4 blocks to splits.txt (textually) + emit a wave map fragment.

Existing unit blocks get their new `.text` lines spliced into sorted position
among that unit's existing `.text` lines; brand-new units are appended as a
fresh block.  Every other line of splits.txt stays byte-identical.

Refuses to write if any new range overlaps an existing `.text` range or another
new range.

Usage:
  homing_apply4.py --blocks h4_blocks.json --frag h4_map_fragment.json \
                   --worktree WT --units-file wave_units.txt \
                   --out-frag wave_frag.json
"""
import argparse
import json
import re


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--blocks', required=True)
    ap.add_argument('--frag', required=True)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--units-file', required=True)
    ap.add_argument('--out-frag', required=True)
    ap.add_argument('--dry', action='store_true')
    a = ap.parse_args()

    splits_path = f'{a.worktree}/config/45410914/splits.txt'
    blocks = {b['unit']: b for b in json.load(open(a.blocks))}
    frag_all = json.load(open(a.frag))
    wave = [u.strip() for u in open(a.units_file) if u.strip()]
    missing = [u for u in wave if u not in blocks]
    assert not missing, f'unknown units: {missing}'

    lines = open(splits_path).read().split('\n')
    rng = re.compile(r'^\t\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')

    # index unit blocks: header line idx -> (unit, last line idx of block)
    hdr_idx = {}
    cur = None
    for i, ln in enumerate(lines):
        if ln.endswith(':') and ln and not ln.startswith((' ', '\t')):
            cur = ln[:-1]
            if cur == 'Sections':
                cur = None
                continue
            hdr_idx[cur] = i

    existing = []
    for ln in lines:
        m = rng.match(ln)
        if m and m.group(1) == 'text':
            existing.append((int(m.group(2), 16), int(m.group(3), 16)))
    existing.sort()

    new_ranges = []
    for u in wave:
        for s, e in blocks[u]['ranges']:
            new_ranges.append((s, e, u))
    new_ranges.sort()
    # overlap self-check
    for i in range(1, len(new_ranges)):
        assert new_ranges[i][0] >= new_ranges[i - 1][1], \
            f'new/new overlap {new_ranges[i-1]} {new_ranges[i]}'
    for s, e, u in new_ranges:
        for xs, xe in existing:
            assert not (s < xe and e > xs), f'new/existing overlap {u} 0x{s:x}-0x{e:x} vs 0x{xs:x}-0x{xe:x}'

    # splice
    inserts = {}          # line index -> list of lines to insert BEFORE it
    appends = []
    for u in wave:
        b = blocks[u]
        newlines = b['body'].split('\n')
        if u not in hdr_idx:
            appends.append(f'{u}:\n' + b['body'] + '\n')
            continue
        h = hdr_idx[u]
        j = h + 1
        text_lines = []
        while j < len(lines) and (lines[j].startswith('\t') or lines[j].startswith(' ')):
            m = rng.match(lines[j])
            if m and m.group(1) == 'text':
                text_lines.append((int(m.group(2), 16), j))
            j += 1
        end_of_block = j
        for nl in newlines:
            m = rng.match(nl)
            va = int(m.group(2), 16)
            pos = end_of_block
            for xva, xj in text_lines:
                if va < xva:
                    pos = xj
                    break
            inserts.setdefault(pos, []).append((va, nl))

    out = []
    for i, ln in enumerate(lines):
        if i in inserts:
            for _, nl in sorted(inserts[i]):
                out.append(nl)
        out.append(ln)
    txt = '\n'.join(out)
    if appends:
        if not txt.endswith('\n'):
            txt += '\n'
        txt += '\n' + '\n'.join(appends)

    frag = {}
    for u in wave:
        for va, size, in []:
            pass
    wave_vas = set()
    for u in wave:
        for s, e in blocks[u]['ranges']:
            wave_vas.add((s, e))
    for k, v in frag_all.items():
        va = int(k, 16)
        if any(s <= va < e for s, e, u in new_ranges):
            frag[k] = v
    json.dump(frag, open(a.out_frag, 'w'), indent=1)

    if a.dry:
        print(f'[dry] would insert {len(new_ranges)} ranges for {len(wave)} units, '
              f'{len(frag)} map entries')
        return
    open(splits_path, 'w').write(txt)
    print(f'inserted {len(new_ranges)} ranges for {len(wave)} units '
          f'({len(appends)} new blocks); frag={len(frag)} -> {a.out_frag}')


if __name__ == '__main__':
    main()
