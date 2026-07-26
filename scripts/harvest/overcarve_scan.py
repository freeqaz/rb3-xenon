#!/usr/bin/env python3
"""Over-carve scanner (laneAT, 2026-07-26).

★ The rule this exists to apply, learned by measurement: **a large single-parent
funclet cluster is as likely to be a WRONG-UNIT carve as a layout defect.** The
biggest "class-member offset" cluster in this lane (Waypoint 0x822d90c8, 20
funclets) turned out to be StreakMeter::StreakMeter() over-carved into
Waypoint.cpp's span -- our obj already emitted the byte-identical cleanup
census, and the class's destructor was already correctly pinned to
StreakMeter.cpp. A splits move fixed all 20; no header touched.

For every candidate target symbol, take its relocation-masked signature and
search EVERY compiled base obj for an exact (zero-word-diff) twin. A twin in a
DIFFERENT unit than the symbol is currently pinned to means the code probably
belongs to that unit.

Measured over the 390 true class-member rows on main at 38,259:
    306  exact twin in a different unit  (over-carve candidate)
     84  no exact twin anywhere          (genuine layout/body defect)
     97  of the 306 have a UNIQUE other-unit owner
    209  ambiguous (twin in >1 other unit) -- NOT a work queue
Dominated by one pair: Waypoint.obj -> system/bandobj/VocalTrackDir.obj, 68.

⚠ Per-row byte evidence is near-worthless for 40-44 B EH funclets -- masking
zeroes almost every instruction, and a prior lane's move experiment on that
population converted 10 of 99 and was reverted. What makes a call safe is
CONCENTRATION (many rows, one unique owner, one unit) plus the corroborating
test: are that class's OTHER members already pinned to the proposed owner?

⚠ After any pin move, re-run unit_scoped_twin_map.py --apply and
map_misassign_repair.py --apply to fixpoint before measuring -- a move strands
every map entry whose VA crossed the boundary.

Requires a FULL build in the worktree first.

Usage: python3 scripts/harvest/overcarve_scan.py <worktree> <rows.json> [--json out]
       rows.json = list of {"sym","unit"} (e.g. neartwin_cause_census.py output)
"""
import argparse, collections, glob, json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'analysis'))
from coffx import read_coff, infer_sizes, funclet_signature, K_SEC


def canon_sigs(path):
    try:
        data = open(path, 'rb').read()
    except OSError:
        return []
    secs, syms = read_coff(data)
    if secs is None:
        return []
    infer_sizes(secs, syms)
    out = []
    for s in syms:
        if s.sec <= 0 or s.size == 0 or s.kind == K_SEC or s.cls not in (2, 3):
            continue
        sec = secs[s.sec - 1]
        if not sec.is_code:
            continue
        g = funclet_signature(sec, s)
        if g is None:
            continue
        lo, hi = s.value, s.value + s.size
        rend = 0
        for (va, si, typ) in sec.relocs:
            if lo <= va < hi:
                rend = max(rend, va - lo + 4)
        end = len(g)
        while end >= 4 and g[end - 4:end] == b'\0\0\0\0' and rend <= end - 4:
            end -= 4
        if end:
            out.append((g[:end], s.name, s.size))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('rows')
    ap.add_argument('--json')
    a = ap.parse_args()
    wt = a.worktree
    rows = json.load(open(a.rows))
    idx = collections.defaultdict(set)
    for p in glob.glob(os.path.join(wt, 'build/45410914/src/**/*.obj'), recursive=True):
        u = os.path.relpath(p, os.path.join(wt, 'build/45410914/src'))
        for sig, nm, sz in canon_sigs(p):
            idx[sig].add((u, nm))
    by_unit = collections.defaultdict(list)
    for r in rows:
        by_unit[r['unit']].append(r)
    stats, hits = collections.Counter(), []
    for unit, rs in by_unit.items():
        tp = os.path.join(wt, 'build/45410914/obj', unit)
        if not os.path.exists(tp):
            stats['no_target_obj'] += len(rs)
            continue
        sigs = {nm: sig for sig, nm, sz in canon_sigs(tp)}
        for r in rs:
            sig = sigs.get(r['sym'])
            if sig is None:
                stats['symbol_missing'] += 1
                continue
            owners = idx.get(sig)
            if not owners:
                stats['no_exact_twin_anywhere'] += 1
                continue
            other = {o for o in owners if o[0] != unit}
            if not other:
                stats['exact_twin_same_unit_only'] += 1
                continue
            stats['OVER_CARVE_CANDIDATE'] += 1
            stats['  unique_owner' if len(other) == 1 else '  ambiguous'] += 1
            hits.append({'sym': r['sym'], 'cur_unit': unit,
                         'owners': sorted(other)[:4], 'n_owners': len(other)})
    print(json.dumps(stats, indent=1))
    if a.json:
        json.dump(hits, open(a.json, 'w'), indent=1)
    pairs = collections.Counter((h['cur_unit'], h['owners'][0][0])
                                for h in hits if h['n_owners'] == 1)
    print('\nunique-owner pairs (current unit -> proposed owner):')
    for k, v in pairs.most_common(20):
        print('  %4d  %s  ->  %s' % (v, k[0], k[1]))


if __name__ == '__main__':
    main()
