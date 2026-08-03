#!/usr/bin/env python3
"""lane EB-1: whole-binary per-row snapshot.

The aggregate has CONCEALED a 968-byte deletion of byte-exact retail code before
(lane's rev-dialect finding: the total still read +1,476 while real matched code
was being destroyed).  Only a per-row set-diff caught it.  So every measurement
here snapshots EVERY row in the binary, not the edited units.

Both rulers are kept per row, because they are computed differently and disagree
on ~219 rows: units/functions follow match_percent_normalized, bytes follow
fuzzy_match_percent.

⚠ matched_code is a JSON STRING in this report -- int()-coerce or the arithmetic
silently concatenates.  ⚠ fuzzy/mpn None means 0.0, NOT 'absent'.
"""
import json, sys

REP = '/home/free/tmp/laneEB1/wt/build/45410914/report.json'


def snap(path=REP):
    r = json.load(open(path))
    m = r['measures']
    rows = {}
    units = {}
    for u in r['units']:
        um = u.get('measures') or {}
        units[u['name']] = (um.get('matched_functions'), um.get('total_functions'))
        for f in (u.get('functions') or []):
            mpn = f.get('match_percent_normalized')
            fz = f.get('fuzzy_match_percent')
            rows[u['name'] + '\x00' + f['name']] = [
                (mpn if mpn is not None else 0.0),
                (fz if fz is not None else 0.0),
                int(f.get('size') or 0),
            ]
    return {
        'measures': {k: (int(m[k]) if isinstance(m.get(k), str) and m[k].isdigit() else m.get(k))
                     for k in ('matched_functions', 'matched_code', 'matched_code_percent',
                               'total_code', 'total_functions', 'fuzzy_match_percent',
                               'masked_equal_functions')},
        'rows': rows,
        'units': units,
    }


if __name__ == '__main__':
    s = snap()
    json.dump(s, open(sys.argv[1], 'w'))
    mm = s['measures']
    at100 = sum(1 for u, (mf, tf) in s['units'].items() if tf and mf == tf)
    print(f"  snapshot -> {sys.argv[1]}")
    print(f"    matched_functions {mm['matched_functions']}  matched_code {mm['matched_code']}"
          f"  code% {mm['matched_code_percent']}  fuzzy% {mm['fuzzy_match_percent']}")
    print(f"    total_code {mm['total_code']}  total_functions {mm['total_functions']}"
          f"  masked_equal {mm['masked_equal_functions']}")
    print(f"    honest {mm['matched_functions'] - mm['masked_equal_functions']}"
          f"   units at 100%: {at100} of {sum(1 for _, (mf, tf) in s['units'].items() if tf)}")
