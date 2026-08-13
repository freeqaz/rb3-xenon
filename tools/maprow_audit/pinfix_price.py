#!/usr/bin/env python3
"""Lane PINFIX-1: price the MISPIN_SINGLE candidates against report.json.

Reads the CURRENT score of each candidate row in the unit it is WRONGLY pinned
to, so the upside is measured rather than assumed.

⚠ report.json is protobuf-JSON: DEFAULTS ARE OMITTED (absent fuzzy = 0, absent
matched_code = 0) and several numerics are JSON STRINGS.  Every read here is
`int(x.get(k, 0))` / `float(x.get(k, 0))`.

⚠ The two rulers disagree by construction: units are counted on `mpn`, bytes
follow `fuzzy`.  Both are reported per row.
"""
import argparse
import collections
import json
import os
import sys

REPORT = 'build/45410914/report.json'


def load_report(path=REPORT):
    d = json.load(open(path))
    prov = d.get('provenance', {})
    rows = {}          # (unit, name) -> row
    byname = collections.defaultdict(list)
    units = {}
    for u in d.get('units', []):
        un = u.get('name', '')
        m = u.get('measures', {})
        units[un] = dict(
            matched_functions=int(m.get('matched_functions', 0)),
            total_functions=int(m.get('total_functions', 0)),
            matched_code=int(m.get('matched_code', 0)),
            total_code=int(m.get('total_code', 0)),
        )
        for f in u.get('functions', []):
            nm = f.get('name', '')
            r = dict(unit=un, name=nm,
                     size=int(f.get('size', 0)),
                     fuzzy=float(f.get('fuzzy_match_percent', 0) or 0),
                     mpn=float(f.get('match_percent_normalized', 0) or 0))
            rows[(un, nm)] = r
            byname[nm].append(r)
    tm = d.get('measures', {})
    top = dict(total_code=int(tm.get('total_code', 0)),
               total_functions=int(tm.get('total_functions', 0)),
               matched_functions=int(tm.get('matched_functions', 0)),
               matched_code=int(tm.get('matched_code', 0)),
               masked_equal=int(tm.get('masked_equal_functions', 0)))
    return rows, byname, units, top, prov


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cands', default=os.path.expanduser('~/tmp/pinfix_cands.json'))
    ap.add_argument('--json', help='write the priced rows here')
    a = ap.parse_args()

    cands = json.load(open(a.cands))
    rows, byname, units, top, prov = load_report()
    print('[provenance] %s' % json.dumps(prov)[:300])
    print('[whole-binary] total_code=%d total_functions=%d matched_functions=%d '
          'matched_code=%d masked_equal=%d'
          % (top['total_code'], top['total_functions'], top['matched_functions'],
             top['matched_code'], top['masked_equal']))

    def unitkey(u):
        base = u[:-4] if u.endswith('.cpp') else u
        return 'default/' + os.path.basename(base)

    out = []
    tally = collections.Counter()
    for c in cands:
        nm = c['name']
        pk = unitkey(c['pinned_unit'])
        r = rows.get((pk, nm))
        hits = byname.get(nm, [])
        rec = dict(c)
        rec['report_unit'] = pk
        if r is None:
            rec['status'] = 'NO_REPORT_ROW'
            rec['size'] = 0
            rec['fuzzy'] = None
            rec['mpn'] = None
            tally['NO_REPORT_ROW'] += 1
        else:
            rec['status'] = 'PRICED'
            rec['size'] = r['size']
            rec['fuzzy'] = r['fuzzy']
            rec['mpn'] = r['mpn']
            tally['PRICED'] += 1
        rec['name_rows_elsewhere'] = [(h['unit'], h['size'], h['fuzzy'], h['mpn'])
                                      for h in hits if h['unit'] != pk]
        out.append(rec)

    priced = [r for r in out if r['status'] == 'PRICED']
    tot = sum(r['size'] for r in priced)
    print('\n[priced] %d of %d candidates have a report row in their pinned unit'
          % (len(priced), len(out)))
    print('[bound]  sum of sizes = %d B = %.4f pp of total_code'
          % (tot, 100.0 * tot / max(top['total_code'], 1)))
    zf = sum(1 for r in priced if r['fuzzy'] == 0.0)
    zm = sum(1 for r in priced if r['mpn'] == 0.0)
    print('[scores] %d/%d at fuzzy 0.000, %d/%d at mpn 0.0'
          % (zf, len(priced), zm, len(priced)))
    nz = [r for r in priced if r['fuzzy'] != 0.0]
    if nz:
        print('  NOT at fuzzy 0: %s' % [(r['addr'], r['fuzzy']) for r in nz][:10])
    print('[sizes]  max %d B, median %d B'
          % (max((r['size'] for r in priced), default=0),
             sorted(r['size'] for r in priced)[len(priced) // 2] if priced else 0))

    if a.json:
        json.dump(out, open(a.json, 'w'), indent=1)
        print('\nwrote %s' % a.json)


if __name__ == '__main__':
    main()
