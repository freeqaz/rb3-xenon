#!/usr/bin/env python3
"""Adjudicate destructor map rows by RETAIL CALLEE IDENTITY.

Why this instrument: a ??_G (scalar deleting dtor) body is ~5 instructions whose
ONLY discriminating content is the `bl` to the real destructor -- and objdiff
hard-sets relocation args to None (report.rs:394), so that is precisely the bit
the metric cannot see. Reading it from retail bytes is therefore a non-metric
discriminator.

Verdicts per row (class C at retail address A):
  CORROBORATED  - some dethunked callee of A is a destructor of class C
  CONTRADICTED  - some dethunked callee is a destructor of a DIFFERENT class D
  NO_NAMED_CALLEE - callees exist but none are named in the map
  NO_CALLEE     - body has no bl at all (pure thunk / leaf)
"""
import sys, json, re, subprocess, collections
sys.path.insert(0, '/home/free/tmp/laneCG1')
from retail import Image, insns, branch_target
from rtti_cohort import undname, cls_from_dtor


def load():
    img = Image()
    sizes = {int(k, 16): v for k, v in
             json.load(open('/home/free/tmp/laneCG1/sizes.json')).items()}
    am = json.load(open('/home/free/tmp/laneCG1/addrmap.json'))
    addr2sym = {int(k, 16): v for k, v in am.items()}
    return img, sizes, addr2sym


def dethunk(img, va, sizes, depth=0, seen=None):
    """RECURSIVE. Retail has two-level chains (vtordisp -> adjustor -> base);
    a single-level read treats the intermediate thunk as the body."""
    if seen is None:
        seen = set()
    if va in seen or depth > 8:
        return va, depth
    seen.add(va)
    sz = sizes.get(va)
    if sz is None or sz > 40:
        return va, depth
    tb = None
    nlink = 0
    for a, i in insns(img, va, sz):
        t = branch_target(a, i)
        if t:
            if t[1]:
                nlink += 1
            else:
                tb = t[0]
    if tb is None or nlink:
        return va, depth
    return dethunk(img, tb, sizes, depth + 1, seen)


def callees(img, va, sizes, addr2sym):
    sz = sizes.get(va)
    if sz is None:
        return None
    out = []
    for a, i in insns(img, va, sz):
        t = branch_target(a, i)
        if t and t[1]:
            tgt, d = dethunk(img, t[0], sizes)
            out.append((t[0], tgt, d, addr2sym.get(tgt)))
    return out


def run(rows, img, sizes, addr2sym, demcache, label):
    # pre-demangle every callee symbol we will meet
    need = set()
    per = {}
    for r in rows:
        va = int(r['addrs'][0], 16)
        cs = callees(img, va, sizes, addr2sym)
        per[r['sym']] = cs
        for c in (cs or []):
            if c[3]:
                need.add(c[3])
    need -= set(demcache)
    if need:
        demcache.update(undname(sorted(need)))

    verdicts = collections.Counter()
    detail = []
    for r in rows:
        cs = per[r['sym']]
        C = r['cls']
        v = 'NO_CALLEE'
        hits, others, named = [], [], 0
        if cs is None:
            v = 'NO_PDATA'
        elif not cs:
            v = 'NO_CALLEE'
        else:
            for orig, tgt, d, sym in cs:
                if not sym:
                    continue
                named += 1
                dc = cls_from_dtor(demcache.get(sym))
                if dc is None:
                    continue
                if dc == C:
                    hits.append((hex(tgt), sym))
                else:
                    others.append((hex(tgt), sym, dc))
            if hits:
                v = 'CORROBORATED'
            elif others:
                v = 'CONTRADICTED'
            elif named:
                v = 'NAMED_BUT_NO_DTOR'
            else:
                v = 'NO_NAMED_CALLEE'
        verdicts[v] += 1
        detail.append(dict(sym=r['sym'], cls=C, kind=r['kind'], addr=r['addrs'][0],
                           unit=r['unit'], tgt_size=r['tgt_size'], verdict=v,
                           hits=hits, others=others,
                           callees=[(hex(o), hex(t), d, s) for o, t, d, s in (cs or [])]))
    tot = sum(verdicts.values())
    print(f'\n=== {label}  (n={tot}) ===')
    for k, n in verdicts.most_common():
        print(f'   {k:20s} {n:5d}  {100*n/tot:5.1f}%')
    return detail, verdicts


def main():
    img, sizes, addr2sym = load()
    rows = json.load(open('/home/free/tmp/laneCG1/dtor_cohort.json'))
    am = json.load(open('/home/free/tmp/laneCG1/addrmap.json'))
    inv = {}
    for a, s in am.items():
        inv.setdefault(s, []).append(a)
    for r in rows:
        r['addrs'] = sorted(inv.get(r['sym'], []))
        r['present'] = bool(r.get('rtti')) or bool(r.get('mangled_rtti'))

    comparable = [r for r in rows if r['fuzzy'] == 100.0 and r['kind'] in ('_G', '_E')
                  and '<' not in r['cls'] and len(r['addrs']) == 1]
    cohort = [r for r in comparable if not r['present']]
    control = [r for r in comparable if r['present']]
    print(f'comparable rows {len(comparable)} = cohort {len(cohort)} + control {len(control)}')

    dem = {}
    cd, cv = run(cohort, img, sizes, addr2sym, dem, 'COHORT (RTTI-ABSENT) -- the 68')
    td, tv = run(control, img, sizes, addr2sym, dem, 'CONTROL (RTTI-PRESENT) -- untreated population')

    def rate(v, key):
        t = sum(v.values())
        return 100 * v.get(key, 0) / t if t else 0
    print('\n=== ENRICHMENT ===')
    for k in ('CORROBORATED', 'CONTRADICTED', 'NO_NAMED_CALLEE', 'NAMED_BUT_NO_DTOR', 'NO_CALLEE'):
        c, t = rate(cv, k), rate(tv, k)
        print(f'  {k:20s} cohort {c:5.1f}%   control {t:5.1f}%   '
              f'ratio {c/t if t else float("inf"):.2f}x' if t else
              f'  {k:20s} cohort {c:5.1f}%   control {t:5.1f}%')
    json.dump(cd, open('/home/free/tmp/laneCG1/cohort_adjudicated.json', 'w'), indent=1)
    json.dump(td, open('/home/free/tmp/laneCG1/control_adjudicated.json', 'w'), indent=1)
    print('\nwrote cohort_adjudicated.json / control_adjudicated.json')


if __name__ == '__main__':
    main()
