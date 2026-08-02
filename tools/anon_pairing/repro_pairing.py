"""Reimplement objdiff's pair_funclets_by_bytes (mod.rs:1410) passes 1/2/2b/3
and predict, per unit, how many anonymous fn_ target rows can pair.

Prediction under H_byte (byte-signature pairing):
  anon rows reaching an EXACT signature partner  ->  should score 100%
  anon rows reaching only pass-3 fuzzy           ->  0 < score < 100
  anon rows with no same-size candidate at all   ->  exactly 0.0
Under H_pos (positional pairing) these three buckets have no reason to line up
with the report's 100 / mid / 0 buckets at all.
"""
import sys, json, collections
sys.path.insert(0, '/home/free/tmp/laneCX4')
import coffsig as C

WT = '/home/free/tmp/laneCX4/wt'


def predict(target_path, base_path):
    tgt = C.parse(target_path)
    bas = C.parse(base_path)
    L = [d for d in C.code_defs(tgt) if C.is_funclet_like(d['name']) and d['size'] > 0]
    R = [d for d in C.code_defs(bas) if C.is_funclet_like(d['name']) and d['size'] > 0]
    # objdiff sorts both candidate lists by symbol name (mod.rs:1467-1468)
    L.sort(key=lambda d: d['name'])
    R.sort(key=lambda d: d['name'])
    lsig = {d['name']: C.signature(tgt, d) for d in L}
    rsig = {d['name']: C.signature(bas, d) for d in R}
    lby = collections.defaultdict(list)
    for d in L:
        lby[lsig[d['name']]].append(d['name'])
    rby = collections.defaultdict(list)
    for d in R:
        rby[rsig[d['name']]].append(d['name'])

    lused, rused = set(), set()
    verdict = {}
    # pass 1: unique-on-both-sides exact
    for sig, ls in lby.items():
        if len(ls) != 1:
            continue
        rs = rby.get(sig)
        if not rs or len(rs) != 1:
            continue
        if ls[0] in lused or rs[0] in rused:
            continue
        verdict[ls[0]] = 'p1_exact'
        lused.add(ls[0]); rused.add(rs[0])
    # pass 2: ambiguous exact groups, greedy zip
    p2 = []
    for sig, ls in lby.items():
        rs = rby.get(sig)
        if not rs:
            continue
        lr = [x for x in ls if x not in lused]
        rr = [x for x in rs if x not in rused]
        p2 += list(zip(lr, rr))
    for l, r in p2:
        if l in lused or r in rused:
            continue
        verdict[l] = 'p2_exact'
        lused.add(l); rused.add(r)
    # pass 2b: over-subscribed overflow, many-to-one (does NOT consume base)
    for sig, ls in lby.items():
        rs = rby.get(sig)
        if not rs:
            continue
        for l in ls:
            if l in lused:
                continue
            verdict[l] = 'p2b_oversub'
            lused.add(l)
    # pass 3: same-size, >=50% byte equality, greedy by descending similarity
    rl = [x for x in L if x['name'] not in lused]
    rr = [x for x in R if x['name'] not in rused]
    scored = []
    for a in rl:
        sa = lsig[a['name']]
        for b in rr:
            sb = rsig[b['name']]
            if len(sa) != len(sb):
                continue
            m = sum(1 for x, y in zip(sa, sb) if x == y)
            if m * 2 >= len(sa):
                scored.append((m, a['name'], b['name']))
    scored.sort(key=lambda t: (-t[0], t[1], t[2]))
    for m, l, r in scored:
        if l in lused or r in rused:
            continue
        verdict[l] = 'p3_fuzzy'
        lused.add(l); rused.add(r)
    for d in L:
        verdict.setdefault(d['name'], 'UNPAIRED')
    return verdict, {d['name']: d['size'] for d in L}


if __name__ == '__main__':
    cfg = json.load(open(f'{WT}/objdiff.json'))
    rep = json.load(open(f'{WT}/build/45410914/report.json'))
    repu = {u['name']: u for u in rep['units']}
    units = sys.argv[1:]
    grand = collections.Counter()
    for un in units:
        uc = next((u for u in cfg['units'] if u['name'] == un), None)
        if not uc or not uc.get('base_path'):
            print(f'{un}: no base'); continue
        v, sizes = predict(f"{WT}/{uc['target_path']}", f"{WT}/{uc['base_path']}")
        rows = {f['name']: f for f in repu[un].get('functions', [])}
        tbl = collections.Counter()
        mismatch = []
        for name, kind in v.items():
            f = rows.get(name)
            if f is None:
                tbl[(kind, 'NOT-IN-REPORT')] += 1
                continue
            mpn = f.get('match_percent_normalized')
            mpn = mpn if mpn is not None else f.get('fuzzy_match_percent', 0.0)
            bucket = '100' if mpn == 100.0 else ('0' if mpn == 0.0 else 'mid')
            tbl[(kind, bucket)] += 1
            grand[(kind, bucket)] += 1
            exact = kind in ('p1_exact', 'p2_exact', 'p2b_oversub')
            if exact != (bucket == '100'):
                mismatch.append((name, kind, mpn, sizes[name]))
        print(f'== {un}')
        for k in sorted(tbl):
            print(f'   {k[0]:12s} -> report {k[1]:>4s} : {tbl[k]}')
        if mismatch:
            print(f'   PREDICTION MISSES ({len(mismatch)}):', mismatch[:8])
    if len(units) > 1:
        print('\n== GRAND'); [print(f'   {k[0]:12s} -> {k[1]:>4s} : {grand[k]}') for k in sorted(grand)]
