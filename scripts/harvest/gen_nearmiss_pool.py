#!/usr/bin/env python3
"""Regenerate the near-miss harvest candidate pool from a report.json.

Implements §1 of docs/decomp/playbooks/nearmiss-harvest.md: named real-bodied
non-STL functions in [96, 99.999), size > 44, minus everything in
nearmiss_verdicts.json (proven walls / deferred header-needs).

Usage:
    python3 scripts/harvest/gen_nearmiss_pool.py [report.json] [-o out.json]
                                                 [--min 96] [--max 99.999]
Prints the sweet-spot table (96-99.5, 100-2000B) and writes the full pool.
"""
import argparse, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('report', nargs='?', default='build/45410914/report.json')
    ap.add_argument('-o', '--out', default=os.path.expanduser('~/tmp/nearmiss_pool.json'))
    ap.add_argument('--min', type=float, default=96.0)
    ap.add_argument('--max', type=float, default=99.999)
    args = ap.parse_args()

    verdicts = json.load(open(os.path.join(HERE, 'nearmiss_verdicts.json')))
    excludes = [k for k in verdicts if not k.startswith('_')]

    r = json.load(open(args.report))
    cands, walled = [], 0
    for u in r['units']:
        for f in u.get('functions', []):
            m = f.get('metadata', {})
            pct = f.get('match_percent_normalized')
            if pct is None:
                pct = f.get('fuzzy_match_percent', 0)
            pct = float(pct)
            name = m.get('demangled_name') or f.get('name', '')
            sz = int(f.get('size', 0))
            if not (args.min <= pct < args.max):
                continue
            if sz <= 44 or name.startswith('fn_'):
                continue
            if 'stlport' in u['name'].lower() or name.startswith('?_') or 'std::' in name:
                continue
            if any(x in name for x in excludes):
                walled += 1
                continue
            cands.append({'pct': round(pct, 5), 'size': sz, 'unit': u['name'],
                          'sym': f['name'], 'demangled': name})
    cands.sort(key=lambda c: -c['pct'])
    json.dump(cands, open(args.out, 'w'), indent=1)

    sweet = [c for c in cands if c['pct'] < 99.5 and 100 <= c['size'] <= 2000]
    print(f"pool: {len(cands)} (excluded {walled} walled)  ->  {args.out}")
    print(f"99.5+ band: {sum(1 for c in cands if c['pct'] >= 99.5)}   "
          f"sweet spot 96-99.5 / 100-2000B: {len(sweet)}")
    for c in sweet[:50]:
        print(f"{c['pct']:8.3f} {c['size']:5d} {c['unit']:38s} {c['demangled'][:84]}")


if __name__ == '__main__':
    sys.exit(main())
