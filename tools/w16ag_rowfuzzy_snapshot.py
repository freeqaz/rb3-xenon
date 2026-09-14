#!/usr/bin/env python3
"""Snapshot / diff EVERY report row's fuzzy + mpn, not only the fuzzy==100 set.

`tools/rowset_snapshot.py` is the byte ledger and is the right instrument for
pricing: `matched_code` keys on fuzzy==100 and is all-or-nothing per row.  But
it is BLIND to a change that moves a row already below 100, which is exactly
what withdrawing an alias forgiveness does when the forgiven site sits in a row
that was never scored -- the withdrawal then reads as a flat Δ0 and cannot be
distinguished from having done nothing at all.

This records (size, fuzzy, mpn) for every row so a Δ0 batch can still be shown
to have LANDED, and so any row that moves is named rather than inferred from an
aggregate.  protobuf-JSON: defaults omitted, numerics are STRINGS.
"""
import json, sys
from pathlib import Path

def rows(repo='.'):
    rep = json.loads(Path(repo, 'build/45410914/report.json').read_text())
    out = {}
    for u in rep.get('units', []):
        for f in u.get('functions', []):
            n = f.get('name') or ''
            if not n:
                continue
            out[f"{u['name']}::{n}"] = [int(f.get('size', 0) or 0),
                                        float(f.get('fuzzy_match_percent', 0) or 0),
                                        float(f.get('match_percent_normalized', 0) or 0)]
    m = rep['measures']
    return out, dict(matched_functions=int(m.get('matched_functions', 0)),
                     matched_code=int(m.get('matched_code', 0) or 0),
                     fuzzy_match_percent=float(m.get('fuzzy_match_percent', 0)))

if __name__ == '__main__':
    if sys.argv[1] == 'save':
        r, me = rows()
        Path(sys.argv[2]).write_text(json.dumps({'rows': r, 'measures': me}))
        print(f'saved {len(r)} rows, matched_code={me["matched_code"]}')
    else:
        prev = json.loads(Path(sys.argv[2]).read_text())
        r, me = rows()
        old, new = prev['rows'], r
        moved = [(k, old[k], new[k]) for k in new
                 if k in old and (old[k][1] != new[k][1] or old[k][2] != new[k][2])]
        print(f'rows whose fuzzy or mpn MOVED: {len(moved)}')
        for k, o, n in sorted(moved, key=lambda x: -abs(x[1][1] - x[2][1]))[:40]:
            print(f'   {o[1]:>10.5f} -> {n[1]:<10.5f} fuzzy | mpn {o[2]:>9.5f} -> {n[2]:<9.5f} | {o[0]:>6} B  {k}')
        print(f'rows added: {len(set(new)-set(old))}  removed: {len(set(old)-set(new))}')
        for kk in prev['measures']:
            print(f'  {kk:22s} {prev["measures"][kk]} -> {me[kk]}   delta {me[kk]-prev["measures"][kk]:+}')
