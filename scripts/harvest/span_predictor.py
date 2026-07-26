#!/usr/bin/env python3
"""Will pointing NAME at retail VA actually pay?  splits.txt x report.json check.

A repair lane learned this the hard way: renaming a target symbol onto its true
retail home only converts into a strict match when that home is

  (a) inside a pinned `.text` range in `config/45410914/splits.txt`, and
  (b) inside the range of a unit whose obj *defines* the symbol

-- otherwise the previous pairing was a false match and the "repair" costs it.

This tool classifies every proposed resolution before a wave is applied:

  PAYS        VA sits in a pinned .text range belonging to a unit whose obj
              defines the symbol -> map entry alone converts it.
  WRONG-UNIT  VA is pinned, but to a unit that does NOT define the symbol.
              objdiff pairs per unit, so our obj is never compared against that
              target obj.  Fixable only by making the owning unit emit the
              symbol (the scatter-include lever) or by correcting the pin; the
              tool reports which unit owns the range so that can be checked.
  UNPINNED    VA is in no .text range at all -> needs a splits pin first
              (homing_gen4.py / homing_apply4.py path).

IMPORTANT (bug fixed 2026-07-26, lane laneO-wrongunit): condition (b) must be
evaluated against **every** unit whose compiled obj defines the symbol, not
against the single TU key the scan record happened to arrive under.  Our objs
are COMDAT-per-function, so a template/inline symbol is defined in *every* obj
that instantiates it; objdiff pairs as soon as the range-owning unit's obj
defines the name.  Keying off the scan's TU alone mislabelled such records
WRONG-UNIT even though a map entry converts them outright -- measured on the
2026-07-26 caller-side wave, 12 of 51 "WRONG-UNIT" records were really PAYS
(10 landable; 2 blocked by a map name collision), all +1 strict / 0 LOST.
The bug costs *recall* only -- it never labelled a non-paying record PAYS --
so the tool's 126/126 lifetime conversion record still stands.

Usage:
    span_predictor.py --proposals prop.json --worktree WT [--out cls.json]
                      [--only PAYS]
"""
import argparse
import bisect
import glob
import json
import os
import re
import struct
from collections import Counter, defaultdict

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


def coff_defined_symbols(data):
    """Names of every symbol this COFF obj DEFINES (SectionNumber > 0)."""
    out = set()
    if len(data) < 20:
        return out
    sym_offset = struct.unpack_from("<I", data, 8)[0]
    num_syms = struct.unpack_from("<I", data, 12)[0]
    if not sym_offset or not num_syms:
        return out
    strtab = sym_offset + num_syms * 18
    i = 0
    while i < num_syms:
        off = sym_offset + i * 18
        if off + 18 > len(data):
            break
        nb = data[off:off + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            a = strtab + struct.unpack_from("<I", nb, 4)[0]
            if 0 <= a < len(data):
                end = data.find(b"\x00", a)
                name = data[a:end if end >= 0 else len(data)].decode(
                    "ascii", errors="replace")
            else:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", errors="replace")
        sec = struct.unpack_from("<h", data, off + 12)[0]
        if name and sec > 0 and data[off + 16] in (IMAGE_SYM_CLASS_EXTERNAL,
                                                   IMAGE_SYM_CLASS_STATIC):
            out.add(name)
        i += 1 + data[off + 17]
    return out


def build_definer_index(worktree):
    """{symbol_name: {unit_key, ...}} over every compiled obj in the worktree.

    unit_key is the obj path relative to build/<title>/src minus '.obj', which
    is the same key space the scan results and splits headers use.
    """
    idx = defaultdict(set)
    roots = glob.glob(os.path.join(worktree, 'build', '*', 'src'))
    for root in roots:
        for p in glob.glob(os.path.join(root, '**', '*.obj'), recursive=True):
            unit = os.path.relpath(p, root)[:-4]
            try:
                data = open(p, 'rb').read()
            except OSError:
                continue
            for n in coff_defined_symbols(data):
                idx[n].add(unit)
    return idx


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

    # A COMDAT is defined in EVERY obj that instantiates it, so the record's
    # scan TU is only one of possibly many valid owners.  objdiff pairs as soon
    # as the range-owning unit's obj defines the name -- test against all of
    # them, not just the scan key.  (See the module docstring.)
    definers = build_definer_index(a.worktree)

    stats = Counter()
    out = defaultdict(list)
    detail = []
    for tu, recs in sorted(prop.items()):
        for r in recs:
            va = int(r['va'], 16)
            own = cov.owner(va)
            owners = definers.get(r['name'], set()) | {tu}
            if own is None:
                cls = 'UNPINNED'
            elif any(matches(t, own) for t in owners):
                cls = 'PAYS'
            else:
                cls = 'WRONG-UNIT'
            stats[cls] += 1
            detail.append(dict(tu=tu, name=r['name'], va=r['va'], cls=cls,
                               owner=own, n_definers=len(owners)))
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
