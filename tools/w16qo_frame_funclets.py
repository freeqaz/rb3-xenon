#!/usr/bin/env python3
"""Attach funclets to census parents by ADDRESS ADJACENCY (W16-QO).
retail emits a parent's EH cleanup funclets contiguously after its body; each
funclet's first word is `addi rD,r12,-<parent frame>`. Walk fn_ symbols after the
parent's extent while they are funclets at tgt_frame.

Usage: w16qo_frame_funclets.py <w23_collectable --json-out file> <out.json>
Run from the repo root. Misses funclet runs that start >12 B past the parent
(e.g. StorePreviewMgr::Handle, +0x40); the W23 multiset count catches those, so
price a row with the max of the two (docs/decomp/W16QO_FRAME_SIZE_CENSUS_2026-10-06.md)."""
import json, struct, sys, collections
sys.path.insert(0, 'tools')
from w23_frame_scan import scan_unit, _s16
rows = json.load(open(sys.argv[1]))
m = json.load(open('scripts/target_symbol_map.json'))
addr_of = {v: int(k, 16) for k, v in m.items() if k.startswith("0x") and isinstance(v, str)}
rep = json.load(open('build/45410914/report.json'))
fz = {}
for u in rep['units']:
    for f in u.get('functions', []):
        fz[(u['name'], f['name'])] = float(f.get('fuzzy_match_percent', 0) or 0)
cfg = {u['name']: u for u in json.load(open('objdiff.json'))['units']}
cache = {}
for r in rows:
    u = cfg[r['unit']]
    if r['unit'] not in cache:
        cache[r['unit']] = scan_unit(u['target_path'], u['base_path'])[0]
    tgt = cache[r['unit']]
    fns = {}
    for n, (body, _r) in tgt.items():
        if n.startswith('fn_'):
            fns[int(n[3:], 16)] = (n, body)
    a = addr_of.get(r['symbol'])
    got = []
    if a is not None and isinstance(r['tgt_frame'], int):
        p = a + r['tgt_size']
        # skip padding up to 8 bytes / EH prefix
        for _ in range(64):
            cand = [x for x in fns if p <= x <= p + 12]
            if not cand:
                break
            x = min(cand); n, body = fns[x]
            w = struct.unpack_from('>I', body, 0)[0]
            if not (w >> 26 == 14 and (w >> 16) & 31 == 12 and -_s16(w & 0xFFFF) == r['tgt_frame']):
                break
            got.append((n, len(body), fz.get((r['unit'], n), 0.0)))
            p = x + len(body)
    r['adj_funclets'] = got
    r['adj_open'] = [g for g in got if g[2] < 100]
    r['adj_open_bytes'] = sum(g[1] for g in r['adj_open'])
    r['prize2'] = r['tgt_size'] + r['adj_open_bytes']
    r['addr'] = hex(a) if a else None
json.dump(rows, open(sys.argv[2], 'w'), indent=1)
for r in sorted(rows, key=lambda r: -r['prize2']):
    print("%6d %5d %3d/%-3d %5s %6.2f %-14s %s" % (r['prize2'], r['tgt_size'], len(r['adj_open']), len(r['adj_funclets']),
          r.get('delta'), r['fuzzy'] or 0, r['verdict'], r['symbol'][:60]))
