#!/usr/bin/env python3
"""One fixpoint round of byte-identity map repair for target_symbol_map.json.

WHY THIS EXISTS
---------------
`map_rotation_repair.py` repairs mispairs using *content* evidence: the string
and float literals a function references.  That is decisive but narrow -- it
only fires for functions that HAVE a literal of their own, which excludes most
of the boilerplate (`??_G` thunks, `?Type@`, `?SyncProperty@`, STL members)
where mispairs concentrate.  On the 2026-07-26 lane it resolved 19 of 304
proven mispairs.

This tool uses a strictly stronger and much wider signal: **whole-function
reloc-masked byte identity**, the same comparison objdiff's normalized diff
performs.  `homing_scan_all.sh` already computes, for every function in our
objs, the set of retail VAs whose bytes are identical once both sides'
relocation slots are masked.  Then:

    the map says NAME lives at VA_m
    the bytes say NAME is identical to retail at {h1, h2, ...}
    VA_m not in {h1, ...}   ==>  the map is WRONG (or our source has drifted)

TWO DISCRIMINATORS, APPLIED IN ORDER
------------------------------------
1. **Forced repoint.**  Exactly one byte-identical hit in the whole 11.8 MB
   binary and the name mapped at exactly one VA.  There is nowhere else the
   function can be.

2. **Spatial (multi-hit).**  ICF twins and boilerplate give many hits, and
   byte identity cannot rank them.  But retail is NOT LTCG-built, so `.text`
   preserves per-TU grouping.  Feed every hit through `span_predictor.py` and
   keep the name only if EXACTLY ONE hit lands in the pinned span of a unit
   whose obj defines that symbol.  Measured 21 gains / 0 losses on its first
   wave.

WHAT IT REFUSES, ON PURPOSE
---------------------------
* **ICF ambiguity.**  If the destination's current holder is ALSO byte-identical
  there, two distinct source functions compiled to the same machine code and the
  linker merged them.  A VA->name map cannot express both and byte identity
  cannot rank them, so neither is asserted.  This is the honest floor of the
  method (~44 entries), not a backlog item.
* **Contested destinations** -- two claimants, same VA.
* The **0x828-0x82C vendor band** (XDK + Quazal), out of project scope.

KNOWN FALSE-POSITIVE CLASS IN THE *CONTENT* AUDIT
-------------------------------------------------
`multi_content_disambiguate.py --trust-audit` will flag some correct repoints
made by this tool as CONTRADICTED.  Verified cause: functions using the
local-static `Symbol` pattern (or passing RTTI descriptors to `__RTDynamicCast`)
reference the static's `.data` storage + guard, not the literal, so the token
comparison misses.  Example: `SongSortByArtist::Init` genuinely references
"song_select" and the repointed VA genuinely contains it, yet the audit
disagrees.  Unique whole-binary byte identity outranks the content audit here.

USAGE
-----
Iterate to a fixpoint -- each wave vacates VAs, freeing destinations for the
next.  The 2026-07-26 lane converged in 4 rounds (+109 gross / -7):

    python3 scripts/harvest/map_repoint_round.py --worktree $PWD \
        --results ~/tmp/homing/merged.json --out ~/tmp/plan_w4.json
    python3 scripts/harvest/map_rotation_repair.py apply \
        --plan ~/tmp/plan_w4.json --map scripts/target_symbol_map.json
    touch config/45410914/config.yml && rm -f build/45410914/report.cache
    ./tools/ninja-locked            # then re-run; stop when it plans 0 moves

VA comparison is always case-insensitive: 264 legacy keys are uppercase "0X...".
"""
import argparse
import json
import os
import subprocess
import sys
from collections import defaultdict

VENDOR_LO, VENDOR_HI = 0x82800000, 0x82C00000


def load_map(path):
    raw = {k.lower(): v for k, v in json.load(open(path)).items()
           if isinstance(v, str)}
    name2va = defaultdict(set)
    for k, v in raw.items():
        name2va[v].add(k)
    return raw, name2va


def load_hits(results):
    """name -> set(lowercased retail VA whose masked bytes equal ours)."""
    hits = defaultdict(set)
    for _tu, recs in json.load(open(results)).items():
        if not isinstance(recs, list):
            continue
        for r in recs:
            if r.get('hits'):
                hits[r['name']] |= {h.lower() for h in r['hits']}
    return hits


def in_vendor(va):
    return VENDOR_LO <= int(va, 16) < VENDOR_HI


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--results', required=True,
                    help='homing_scan_all.sh merged.json')
    ap.add_argument('--map')
    ap.add_argument('--out', required=True, help='plan.json for '
                    'map_rotation_repair.py apply')
    ap.add_argument('--no-spatial', action='store_true',
                    help='forced single-hit repoints only (skip discriminator 2)')
    a = ap.parse_args()

    wt = a.worktree
    mp = a.map or os.path.join(wt, 'scripts/target_symbol_map.json')
    raw, name2va = load_map(mp)
    hits_of = load_hits(a.results)

    stats = defaultdict(int)
    rows = []

    def holder_also_claims(va):
        h = raw.get(va)
        return h is not None and va in hits_of.get(h, set())

    # ---- discriminator 1: forced (unique whole-binary byte identity)
    mispaired = []
    for name, hits in hits_of.items():
        cur = name2va.get(name)
        if not cur or (cur & hits):
            continue
        stats['mispaired'] += 1
        if len(cur) != 1:
            stats['refuse-name-at-many-VAs'] += 1
            continue
        mispaired.append((name, next(iter(cur)), hits))

    for name, cur, hits in mispaired:
        if len(hits) != 1:
            continue
        va = next(iter(hits))
        if holder_also_claims(va):
            stats['refuse-ICF-fold'] += 1
            continue
        if in_vendor(va):
            stats['refuse-vendor-band'] += 1
            continue
        rows.append((name, cur, va))
    stats['forced'] = len(rows)

    # ---- discriminator 2: spatial (exactly one hit in a paying span)
    if not a.no_spatial:
        multi = [dict(name=n, va=h)
                 for n, _cur, hits in mispaired if len(hits) > 1
                 for h in hits if not in_vendor(h)]
        if multi:
            pin = a.out + '.prop'
            json.dump({'m': multi}, open(pin, 'w'))
            span = a.out + '.span'
            subprocess.run(
                [sys.executable, os.path.join(wt, 'scripts/harvest/span_predictor.py'),
                 '--proposals', pin, '--worktree', wt, '--out', span,
                 '--only', 'PAYS'], check=True, capture_output=True)
            pays = defaultdict(set)
            for r in json.load(open(span))['m']:
                pays[r['name']].add(r['va'].lower())
            for name, vas in pays.items():
                if len(vas) != 1:
                    stats['refuse-several-paying-hits'] += 1
                    continue
                va = next(iter(vas))
                if holder_also_claims(va):
                    stats['refuse-ICF-fold'] += 1
                    continue
                rows.append((name, next(iter(name2va[name])), va))
            stats['spatial'] = len(rows) - stats['forced']

    # ---- drop contested destinations, emit the plan
    byd = defaultdict(list)
    for n, _c, va in rows:
        byd[va].append(n)
    contested = {va for va, ns in byd.items() if len(ns) > 1}
    stats['refuse-contested-dst'] = sum(len(byd[v]) for v in contested)

    setmap, remove = {}, set()
    for name, cur, va in rows:
        if va in contested or va in setmap:
            continue
        setmap[va] = name
        remove.add(cur)
    plan = dict(set=setmap, remove=sorted(remove - set(setmap)),
                evicted=[], blocked={}, contested={}, cycles=[], chains=[])
    json.dump(plan, open(a.out, 'w'), indent=1)

    print('repoint round:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('plan: %d set, %d remove -> %s'
          % (len(setmap), len(plan['remove']), a.out))
    if not setmap:
        print('FIXPOINT REACHED — nothing further this method can prove.')


if __name__ == '__main__':
    main()
