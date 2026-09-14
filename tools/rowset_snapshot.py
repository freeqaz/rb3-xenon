#!/usr/bin/env python3
"""Snapshot / set-diff the `fuzzy == 100` row set of build/45410914/report.json.

A whole-binary delta reported as a rounded gap hides offsetting moves: a change
can add N bytes in one row and silently lose M in another.  CLAUDE.md's rule is
to reconcile crossed-vs-net by SET-DIFF of the fuzzy==100 membership, and that
is what this does.  `matched_code` keys on fuzzy==100 and is all-or-nothing per
row, so the crossed set IS the byte ledger.
"""
import json, sys
from pathlib import Path

def rowset(repo='.'):
    rep = json.loads(Path(repo, 'build/45410914/report.json').read_text())
    out = {}
    for u in rep.get('units', []):
        for f in u.get('functions', []):
            n = f.get('name') or ''
            if not n:
                continue
            key = f"{u['name']}::{n}"
            if float(f.get('fuzzy_match_percent', 0) or 0) == 100.0:
                out[key] = int(f.get('size', 0) or 0)
    m = rep['measures']
    return out, dict(matched_functions=int(m.get('matched_functions', 0)),
                     matched_code=int(m.get('matched_code', 0) or 0),
                     matched_code_percent=float(m.get('matched_code_percent', 0)),
                     fuzzy_match_percent=float(m.get('fuzzy_match_percent', 0)))

if __name__ == '__main__':
    if sys.argv[1] == 'save':
        rs, me = rowset()
        Path(sys.argv[2]).write_text(json.dumps({'rows': rs, 'measures': me}))
        print(f'saved {len(rs)} rows at fuzzy==100, matched_code={me["matched_code"]}')
    else:  # diff <file>
        prev = json.loads(Path(sys.argv[2]).read_text())
        rs, me = rowset()
        old, new = prev['rows'], rs
        gained = {k: v for k, v in new.items() if k not in old}
        lost = {k: v for k, v in old.items() if k not in new}
        print(f'CROSSED IN : {len(gained)} rows, {sum(gained.values())} B')
        for k, v in sorted(gained.items(), key=lambda x: -x[1])[:25]:
            print(f'   +{v:>7} B  {k}')
        print(f'FELL OUT   : {len(lost)} rows, {sum(lost.values())} B')
        for k, v in sorted(lost.items(), key=lambda x: -x[1])[:25]:
            print(f'   -{v:>7} B  {k}')
        print(f'NET bytes  : {sum(gained.values()) - sum(lost.values()):+}')
        for k in prev['measures']:
            print(f'  {k:22s} {prev["measures"][k]} -> {me[k]}   delta {me[k]-prev["measures"][k]:+}')
