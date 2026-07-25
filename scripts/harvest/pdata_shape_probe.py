#!/usr/bin/env python3
"""Measure whether `.pdata` / surrounding-context shape discriminates ICF twins.

Hypothesis under test (candidate discriminator #1 for the homing-scan residue):
the reloc-masked byte compare throws away the `.pdata` unwind record, so the
prolog length / exception flag might tell two byte-identical retail twins apart.

Xbox 360 RUNTIME_FUNCTION (8 bytes, big-endian):
    word0  BeginAddress
    word1  bits  0.. 7  PrologLen   (instructions)
           bits  8..29  FuncLen     (instructions)
           bit  30      ThirtyTwoBit
           bit  31      ExceptionFlag

Probes, per MULTI hit set:
  pdata     -- does (PrologLen, ThirtyTwoBit, ExceptionFlag) vary across hits?
  prev4     -- does the word immediately BEFORE the function vary? (an
               exception-flagged function stores its handler pointer there)
  align     -- does the VA's 8/16-byte alignment vary?
  gap       -- does the distance to the previous .pdata entry's end vary?

A probe only *discriminates* if it varies within the hit set; a probe that is
constant across every hit set is worthless and should not be built on.

Usage:  pdata_shape_probe.py --results merged.json --worktree WT
"""
import argparse
import json
import os
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from multi_content_disambiguate import Band, load_tmap  # noqa: E402


def pdata_table(band):
    for va, vs, praw, rs, nm in band.secs:
        if nm == '.pdata':
            d = band.d[praw:praw + rs]
            out = {}
            ents = []
            for i in range(len(d) // 8):
                b, w1 = struct.unpack_from('>II', d, i * 8)
                if not b:
                    continue
                out[b] = dict(prolog=w1 & 0xFF,
                              flen=((w1 >> 8) & 0x3FFFFF) * 4,
                              bit30=(w1 >> 30) & 1,
                              eh=(w1 >> 31) & 1)
                ents.append(b)
            ents.sort()
            return out, ents
    raise SystemExit('no .pdata')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--results', required=True)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--band')
    a = ap.parse_args()
    wt = a.worktree
    band = Band(a.band or os.path.join(wt, 'orig/45410914/band.exe'))
    pd, ents = pdata_table(band)
    tmap, _ = load_tmap(os.path.join(wt, 'scripts/target_symbol_map.json'))
    res = json.load(open(a.results))

    stats = defaultdict(int)
    # how far a probe could go: sets it splits into >1 group, and sets it
    # reduces to a single survivor
    for tu, recs in res.items():
        if not isinstance(recs, list):
            continue
        for r in recs:
            hits = [int(h, 16) for h in (r.get('hits') or [])]
            if len(hits) < 2:
                continue
            stats['sets'] += 1
            for probe, fn in (
                ('pdata', lambda v: (pd.get(v, {}).get('prolog'),
                                     pd.get(v, {}).get('bit30'),
                                     pd.get(v, {}).get('eh'))),
                ('prev4', lambda v: band.read(v - 4, 4)),
                ('align', lambda v: v & 0xF),
                ('gap',   lambda v: (v - (ents[max(0, __import__('bisect')
                                                   .bisect_left(ents, v) - 1)]
                                          + pd.get(ents[max(0, __import__('bisect')
                                                            .bisect_left(ents, v) - 1)],
                                                   {}).get('flen', 0)))),
            ):
                vals = [fn(v) for v in hits]
                nd = len({repr(x) for x in vals})
                if nd > 1:
                    stats[probe + '/varies'] += 1
                    # would it isolate the truth?  only measurable when known
                    own = [v for v in hits if tmap.get(v) == r['name']]
                    if len(own) == 1:
                        mine = repr(fn(own[0]))
                        if sum(1 for x in vals if repr(x) == mine) == 1:
                            stats[probe + '/isolates-truth'] += 1
                        stats[probe + '/measurable'] += 1
                else:
                    stats[probe + '/constant'] += 1
    print(json.dumps(dict(sorted(stats.items())), indent=1))


if __name__ == '__main__':
    main()
