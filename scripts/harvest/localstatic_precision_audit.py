#!/usr/bin/env python3
"""Precision audit: what the guard filter does to the existing 102-row census.

Joins the rows of scripts/harvest/localstatic_population_scan.py (which counts
RAW Symbol/Message/atexit relocations) against the guard-verified,
string-resolved sites of scripts/harvest/localstatic_patch_gen.py, and
classifies each old row:

  CONFIRMED      guard-verified local statics, strings resolve, target-side
                 excess survives -> a real conversion target.
  MISPAIR        the resolved strings are impossible for the symbol's name
                 (STL container internals carrying UI property names), or the
                 same mangled name resolves to disjoint strings in two target
                 objs, or the target symbol is >3x/<1/3 the size of the
                 function we compiled under that name.
  NO_GUARD       the extra ctor calls are NOT wrapped in an MSVC guard-bit
                 test/set, so they are temporaries (`Symbol s(str)`,
                 MakeString-built Symbols, DataNode conversions), not statics.
  ATEXIT_ONLY    the whole excess was `atexit`, which the old tool counts as a
                 tell in its own right.

Usage:
  python3 scripts/harvest/localstatic_population_scan.py <wt> --json old.json
  python3 scripts/harvest/localstatic_precision_audit.py <wt> old.json
"""
import collections, json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import localstatic_patch_gen as G
from localstatic_census_wide import STL_RE


def main():
    wt, oldp = sys.argv[1], sys.argv[2]
    old = json.load(open(oldp))
    img = G.Image(os.path.join(wt, 'orig/45410914/band.exe'))
    _t, rev = G.load_target_map(wt)
    tell = G.build_tellname(rev)

    strings_of = collections.defaultdict(set)
    troot = os.path.join(wt, 'build/45410914/obj')
    cache = {}
    for r in old['rows']:
        tp = os.path.join(troot, r['unit'])
        if tp not in cache:
            cache[tp] = G.scan_obj(tp, img, tell)
        got = cache[tp].get(r["sym"])
        if got:
            for s in got[1]:
                if s['form'] == 'LOCAL_STATIC' and s['string']:
                    strings_of[r['sym']].add(s['string'])

    verdicts = collections.Counter()
    detail = []
    for r in old['rows']:
        tp = os.path.join(troot, r['unit'])
        got = cache[tp].get(r["sym"])
        ls = [s for s in (got[1] if got else []) if s['form'] == 'LOCAL_STATIC']
        tmp = [s for s in (got[1] if got else []) if s['form'] == 'TEMPORARY']
        v = 'CONFIRMED'
        why = ''
        if not ls:
            v = 'ATEXIT_ONLY' if set(r['excess']) == {'atexit'} else 'NO_GUARD'
            why = '%d temporaries' % len(tmp)
        elif STL_RE.search(r['sym']):
            v = 'MISPAIR'
            why = 'STL symbol carrying %s' % ', '.join(
                sorted({s['string'] for s in ls if s['string']})[:3])
        elif any(s['string'] is None for s in ls):
            v = 'NO_STRING'
            why = '%d/%d unresolved' % (sum(1 for s in ls if not s['string']),
                                        len(ls))
        verdicts[v] += 1
        detail.append((v, r['unit'], r['sym'], r['pct'], r['n'], len(ls), why))

    print('old rows: %d' % len(old['rows']))
    for k, v in verdicts.most_common():
        print('  %-12s %d' % (k, v))
    print()
    for v, u, s, p, n, ng, why in sorted(detail):
        if v == 'CONFIRMED':
            continue
        print('  %-12s %-28s %-58s %5.1f%%  old_excess=%d guarded=%d  %s'
              % (v, u[:28], s[:58], p, n, ng, why))


if __name__ == '__main__':
    main()
