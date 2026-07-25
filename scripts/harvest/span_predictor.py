#!/usr/bin/env python3
"""Will pointing NAME at retail VA actually pay?  splits.txt x report.json check.

A repair lane learned this the hard way: renaming a target symbol onto its true
retail home only converts into a strict match when that home is

  (a) inside a pinned `.text` range in `config/45410914/splits.txt`, and
  (b) inside the range of the unit that *compiles* the symbol

-- otherwise the previous pairing was a false match and the "repair" costs it.

This tool classifies every proposed resolution before a wave is applied:

  PAYS        VA sits in a pinned .text range belonging to the same unit that
              compiles the symbol -> map entry alone converts it.
  WRONG-UNIT  VA is pinned, but to a DIFFERENT unit.  objdiff pairs per unit, so
              our obj is never compared against that target obj.  Free only if
              the owning unit also compiles the symbol (scatter-include); the
              tool reports which unit owns it so that can be checked.
  UNPINNED    VA is in no .text range at all -> needs a splits pin first
              (homing_gen4.py / homing_apply4.py path).

Usage:
    span_predictor.py --proposals prop.json --worktree WT [--out cls.json]
                      [--only PAYS]
"""
import argparse
import bisect
import json
import os
import re
from collections import Counter, defaultdict


def parse_splits(path):
    units = {}
    cur = None
    rng = re.compile(r'\.(\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)')
    for line in open(path):
        st = line.strip()
        if st.endswith(':') and not line.startswith((' ', '\t')):
            cur = st[:-1]
            if cur == 'Sections':
                cur = None
                continue
            units.setdefault(cur, [])
            continue
        m = rng.search(line)
        if m and cur and m.group(1) == 'text':
            units[cur].append((int(m.group(2), 16), int(m.group(3), 16)))
    return units


class Coverage:
    def __init__(self, units):
        self.iv = []
        for u, rs in units.items():
            for s, e in rs:
                self.iv.append((s, e, u))
        self.iv.sort()
        self.starts = [x[0] for x in self.iv]

    def owner(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i < 0:
            return None
        s, e, u = self.iv[i]
        return u if s <= va < e else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--proposals', required=True,
                    help='homing_scan-format result dict tu -> [records with va]')
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--out')
    ap.add_argument('--only', help='write only records with this class')
    a = ap.parse_args()

    units = parse_splits(os.path.join(a.worktree, 'config/45410914/splits.txt'))
    cov = Coverage(units)

    # splits headers are inconsistent: some are bare basenames ("Object.cpp"),
    # some carry a partial path ("band3/meta_band/BandProfile.cpp").  A unit key
    # ("band3/meta_band/BandProfile") matches a header when the header is a
    # path-suffix of "<key>.cpp".
    prop = json.load(open(a.proposals))

    def matches(tu, header):
        want = tu + '.cpp'
        return want == header or want.endswith('/' + header)


    stats = Counter()
    out = defaultdict(list)
    detail = []
    for tu, recs in sorted(prop.items()):
        for r in recs:
            va = int(r['va'], 16)
            own = cov.owner(va)
            if own is None:
                cls = 'UNPINNED'
            elif matches(tu, own):
                cls = 'PAYS'
            else:
                cls = 'WRONG-UNIT'
            stats[cls] += 1
            detail.append(dict(tu=tu, name=r['name'], va=r['va'], cls=cls,
                               owner=own))
            if a.only is None or cls == a.only:
                out[tu].append(r)

    print('span prediction:', dict(stats))
    misown = Counter(d['owner'] for d in detail if d['cls'] == 'WRONG-UNIT')
    if misown:
        print('  WRONG-UNIT owners (top):', misown.most_common(8))
    if a.out:
        json.dump(dict(out), open(a.out, 'w'), indent=1)
        json.dump(detail, open(a.out.replace('.json', '_detail.json'), 'w'),
                  indent=1)
        print('->', a.out)


if __name__ == '__main__':
    main()
