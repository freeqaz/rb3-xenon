#!/usr/bin/env python3
"""Sandwich + definer over-carve candidate generator (laneAT, 2026-07-26).

Two over-carves found in this lane (StreakMeter +13, VocalTrackDir +67) shared
one shape: a `.text` block pinned to unit A whose BOTH immediate gap-0
neighbours belong to the same other unit B, where B already defines the code.
A sandwich alone is NOT proof -- both wins needed a second, decisive test:

  ★ DEFINER TEST: do the functions in the block relocation-masked byte-match
    symbols defined in unit B's COMPILED obj? If a large fraction do, B is the
    real owner and the block is an over-carve.

This tool computes both and ranks by the definer-test hit rate, so a candidate
arrives already corroborated rather than merely suspicious.

Deliberately does NOT rank by "matched functions in the donor" -- report.json
unit names are BASENAMES, so `system/rnddx9/Rnd.cpp` and `system/rndobj/Rnd.cpp`
collide and that column is unreliable for path-qualified units.

⚠ After applying any move: the .pdata must be moved too (splits_move.py only
moves .text, and it silently appends a DUPLICATE unit block if given a
path-qualified key instead of the bare basename), then re-run
unit_scoped_twin_map.py --apply and map_misassign_repair.py --apply to fixpoint
-- a move strands every map entry whose VA crossed the boundary.

Requires a FULL build in the worktree first.

Usage: python3 scripts/harvest/sandwich_overcarve.py <worktree> [--json out] [--min-frac 0.5]
"""
import argparse, collections, glob, json, os, re, sys
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


def read_pins(path):
    pins, cur = [], None
    for line in open(path):
        m = re.match(r'^(\S+):', line)
        if m:
            cur = m.group(1)
            continue
        m = re.search(r'\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
        if m and cur:
            pins.append((int(m.group(1), 16), int(m.group(2), 16), cur))
    pins.sort()
    return pins


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('worktree')
    ap.add_argument('--json')
    ap.add_argument('--min-frac', type=float, default=0.5)
    a = ap.parse_args()
    wt = a.worktree
    pins = read_pins(os.path.join(wt, 'config/45410914/splits.txt'))

    sandwiches = []
    for i in range(1, len(pins) - 1):
        s, e, u = pins[i]
        ps, pe, pu = pins[i - 1]
        ns, ne, nu = pins[i + 1]
        if pu == nu and pu != u and pe == s and ne >= e and ns == e:
            sandwiches.append((s, e, u, pu))
    print('gap-0 sandwiched .text blocks: %d' % len(sandwiches))

    base_by_unit = {}
    for p in glob.glob(os.path.join(wt, 'build/45410914/src/**/*.obj'), recursive=True):
        base_by_unit[os.path.relpath(p, os.path.join(wt, 'build/45410914/src'))] = p

    # signature -> count, for the destination's compiled obj
    def base_sig_counts(unit_cpp):
        obj = unit_cpp[:-4] + '.obj'
        p = base_by_unit.get(obj)
        if p is None:
            cands = [v for k, v in base_by_unit.items()
                     if os.path.basename(k) == os.path.basename(obj)]
            if len(cands) != 1:
                return None
            p = cands[0]
        c = collections.Counter()
        for sig, nm, sz in canon_sigs(p):
            c[sig] += 1
        return c

    # signature -> count of symbols ALREADY pinned to the destination and
    # already matching at 100% (they are consuming a base symbol today)
    rep = json.load(open(os.path.join(wt, 'build/45410914/report.json')))
    pct = {}
    for un in rep['units']:
        for f in (un.get('functions') or []):
            pct[(un['name'].split('/', 1)[-1], f['name'])] = f['match_percent_normalized']

    def claimed_counts(unit_cpp):
        obj = unit_cpp[:-4] + '.obj'
        tp = os.path.join(wt, 'build/45410914/obj', obj)
        if not os.path.exists(tp):
            cands = glob.glob(os.path.join(wt, 'build/45410914/obj', '**',
                                           os.path.basename(obj)), recursive=True)
            if len(cands) != 1:
                return collections.Counter()
            tp = cands[0]
        key = unit_cpp[:-4]
        c = collections.Counter()
        for sig, nm, sz in canon_sigs(tp):
            if pct.get((key, nm)) == 100.0:
                c[sig] += 1
        return c

    sig_cache, claim_cache, rows = {}, {}, []
    for s, e, u, owner in sandwiches:
        tobj = os.path.join(wt, 'build/45410914/obj', u[:-4] + '.obj')
        if not os.path.exists(tobj):
            cands = glob.glob(os.path.join(wt, 'build/45410914/obj', '**',
                                           os.path.basename(u[:-4] + '.obj')),
                              recursive=True)
            if len(cands) != 1:
                continue
            tobj = cands[0]
        if owner not in sig_cache:
            sig_cache[owner] = base_sig_counts(owner)
        osigs = sig_cache[owner]
        if not osigs:
            continue
        # ★ PREDICATE 3 -- CAPACITY. Supply = base symbols in the destination
        # carrying a signature. Demand already present = target symbols ALREADY
        # pinned to the destination that carry it and are already at 100%.
        # A move is only free when supply - claimed >= incoming, per signature;
        # otherwise the newcomers displace incumbents under greedy pairing and
        # the losses land in the RECEIVING unit. Measured: a 19-block batch at
        # 100% definer corroboration but WITHOUT this check ran -23
        # (49 gained / 72 lost).
        if owner not in claim_cache:
            claim_cache[owner] = claimed_counts(owner)
        claimed = claim_cache[owner]
        incoming = collections.Counter()
        tot = hit = 0
        for sig, nm, sz in canon_sigs(tobj):
            m = re.match(r'^fn_([0-9A-Fa-f]{8})$', nm)
            va = int(m.group(1), 16) if m else None
            if va is None or not (s <= va < e):
                continue
            tot += 1
            if sig in osigs:
                hit += 1
                incoming[sig] += 1
        if not tot:
            continue
        frac = hit / tot
        squeeze = 0
        for sig, want in incoming.items():
            spare = osigs.get(sig, 0) - claimed.get(sig, 0)
            if want > spare:
                squeeze += want - spare
        rows.append({'start': hex(s), 'end': hex(e), 'bytes': e - s,
                     'pinned_to': u, 'proposed_owner': owner,
                     'fns': tot, 'definer_hits': hit, 'frac': round(frac, 3),
                     'squeeze': squeeze, 'capacity_ok': squeeze == 0})
    rows.sort(key=lambda r: (-r['frac'], -r['fns']))
    strong = [r for r in rows if r['frac'] >= a.min_frac]
    safe = [r for r in strong if r['capacity_ok']]
    print('scored blocks: %d   definer-corroborated (frac >= %.2f): %d  (%d fns)'
          % (len(rows), a.min_frac, len(strong), sum(r['fns'] for r in strong)))
    print('★ ALSO capacity-safe (predicate 3): %d blocks (%d fns);'
          ' rejected %d blocks (%d fns) for squeeze'
          % (len(safe), sum(r['fns'] for r in safe),
             len(strong) - len(safe),
             sum(r['fns'] for r in strong if not r['capacity_ok'])))
    for r in safe[:25]:
        print('  SAFE %5d fns  %5.1f%%  %s -> %s   %s-%s'
              % (r['fns'], 100 * r['frac'], r['pinned_to'], r['proposed_owner'],
                 r['start'], r['end']))
    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
