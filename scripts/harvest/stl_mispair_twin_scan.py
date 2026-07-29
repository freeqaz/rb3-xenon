#!/usr/bin/env python3
"""mispair_verdict.py -- settle the STL-instantiation near-miss class.

For every sub-100 named STL-template function in report.json:
  1. take the TARGET body (dtk obj) with relocation words masked
  2. look it up in a tree-wide index of OUR compiled bodies (same masking)
  3. if the target body is byte-identical to our instantiation F<U> for some
     U != T, the target COMDAT is *U's* instantiation -> objdiff is comparing
     apples to oranges -> MAP MISPAIR, unfixable in source.
     If the target body has no twin, the divergence is genuine (or the twin
     just isn't compiled anywhere) -> candidate for a real layout fix.

Usage: mispair_verdict.py <project_dir>
"""
import json, os, re, struct, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coff_func_bodies import parse

SHAPES = ('_M_insert_overflow_aux', '_M_fill_insert', '__uninitialized_fill_n',
          '__uninitialized_copy', '_M_allocate_and_copy', '_Destroy_Range',
          '_M_create_node', '__destroy_range_aux', 'push_back', '?resize@',
          '_M_erase', '_M_fill_insert_aux', '__destroy_mv_srcs', '_Copy_Construct')


def bodies(path, maxsz=1200):
    """-> {name: (masked_bytes, [reloc names])}"""
    d, secs, syms, idx = parse(path)
    bysec = {}
    for (name, val, secnum, typ, sc, si) in syms:
        if secnum <= 0 or secnum > len(secs) or sc not in (2, 3):
            continue
        sec = secs[secnum - 1]
        if not sec['name'].startswith('.text'):
            continue
        bysec.setdefault(secnum, []).append((val, name))
    out = {}
    for secnum, ents in bysec.items():
        sec = secs[secnum - 1]
        ents.sort()
        rels = []
        for r in range(sec['nrel']):
            o = sec['relptr'] + r * 10
            va, symidx, typ = struct.unpack_from('<IIH', d, o)
            s = idx.get(symidx)
            rels.append((va, s[0] if s else '?', typ))
        rels.sort()
        for k, (off, name) in enumerate(ents):
            end = ents[k + 1][0] if k + 1 < len(ents) else sec['rawsz']
            if end - off <= 0 or end - off > maxsz:
                continue
            body = bytearray(d[sec['rawptr'] + off: sec['rawptr'] + end])
            rn = []
            for (va, r, t) in rels:
                if off <= va < end:
                    rn.append(r)
                    p = (va - off) & ~3
                    if p + 4 <= len(body):
                        body[p:p + 4] = b'\0\0\0\0'
            prev = out.get(name)
            if prev is None or len(body) > len(prev[0]):
                out[name] = (bytes(body), rn)
    return out


def main():
    pd = sys.argv[1]
    cfg = json.load(open(os.path.join(pd, 'objdiff.json')))
    rep = json.load(open(os.path.join(pd, 'build/45410914/report.json')))

    unit_paths = {u['name']: (u.get('target_path'), u.get('base_path'))
                  for u in cfg['units']
                  if u.get('target_path') and u.get('base_path')}

    # tree-wide index of OUR bodies
    print('indexing base objs...', file=sys.stderr)
    index = collections.defaultdict(set)
    base_cache = {}
    for name, (tp, bp) in unit_paths.items():
        bp_full = os.path.join(pd, bp)
        if not os.path.exists(bp_full):
            continue
        try:
            b = bodies(bp_full)
        except Exception as e:
            continue
        base_cache[name] = b
        for fn, (body, rn) in b.items():
            if any(s in fn for s in SHAPES):
                index[body].add(fn)
    print(f'  {len(index)} distinct masked bodies', file=sys.stderr)

    rows = []
    for u in rep['units']:
        un = u['name']
        if un not in unit_paths:
            continue
        tp = os.path.join(pd, unit_paths[un][0])
        if not os.path.exists(tp):
            continue
        cand = [f for f in u.get('functions', [])
                if 96.0 <= f.get('fuzzy_match_percent', 0) < 100.0
                and any(s in f['name'] for s in SHAPES)]
        if not cand:
            continue
        try:
            T = bodies(tp)
        except Exception:
            continue
        B = base_cache.get(un, {})
        for f in cand:
            fn = f['name']
            if fn not in T:
                rows.append((un, fn, f['fuzzy_match_percent'], 'NO_TARGET_BODY', ''))
                continue
            tb, trn = T[fn]
            twins = index.get(tb, set())
            if not twins:
                verdict = 'NO_TWIN'
                alt = ''
            elif fn in twins:
                verdict = 'SELF_TWIN'
                alt = ''
            else:
                verdict = 'MISPAIR'
                alt = sorted(twins)[0]
            rows.append((un, fn, f['fuzzy_match_percent'], verdict, alt))

    counts = collections.Counter(r[3] for r in rows)
    print('\n===== VERDICT COUNTS =====')
    for k, v in counts.most_common():
        print(f'  {k:16s} {v}')
    print(f'  TOTAL            {len(rows)}')
    out = os.environ.get('LANE_OUT', '/tmp/stl_mispair_twin.tsv')
    with open(out, 'w') as fh:
        for r in rows:
            fh.write('\t'.join(str(x) for x in r) + '\n')
    print('\nwrote', out)


if __name__ == '__main__':
    main()
