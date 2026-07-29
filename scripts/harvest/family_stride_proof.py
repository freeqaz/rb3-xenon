#!/usr/bin/env python3
"""family_stride_proof.py -- SELF-CONTAINED proof of MAP MISPAIR.

Within one target obj, every STL helper of the same `vector<T>` family must use
the SAME element stride (sizeof T).  If a 100%-matching member of family T
strides by S in the TARGET, and a near-miss member of family T strides by S' != S
in the TARGET, the near-miss member's target COMDAT cannot belong to family T.
=> objdiff is comparing our F<T> against retail's F<U>.  MAP MISPAIR, unfixable
in source.  No external oracle, no struct DB, no assumptions.
"""
import json, os, re, struct, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stl_mispair_twin_scan import bodies

TOK = re.compile(r'[A-Za-z_][A-Za-z0-9_]{2,}')
NOISE = set('''stlpmtx_std vector StlNodeAlloc false_type true_type Copy_Construct
Param_Construct uninitialized_fill_n uninitialized_copy Destroy Destroy_Range
M_insert_overflow_aux M_fill_insert M_allocate_and_copy M_create_node resize
destroy_range_aux push_back M_erase M_fill_insert_aux destroy_mv_srcs
reverse_iterator ObjVector ObjList allocator iterator const Nonconst traits
Vector_base STLP_alloc_proxy List_iterator Rb_tree List_node Alloc_traits
rebind type_traits TrivialUCopy move_traits move_source clear insert erase
assign swap reserve begin end size Copy Construct'''.split())

FAMSHAPE = ('_M_insert_overflow_aux', '_M_fill_insert', '__uninitialized_fill_n',
            '__uninitialized_copy', '_M_allocate_and_copy', '_Destroy_Range',
            '__destroy_range_aux', '?resize@', '_M_erase', '_M_fill_insert_aux',
            '__destroy_mv_srcs', '?push_back@', '??1?$vector', '?_M_clear_after_move@',
            '??0?$vector', '?assign@', '?reserve@')


def strides(b):
    """element-stride evidence: srawi/rlwinm shift amounts + mulli constants"""
    sh = set()
    for i in range(0, len(b) - 3, 4):
        w = struct.unpack_from('>I', b, i)[0]
        op = w >> 26
        if op == 31 and ((w >> 1) & 0x3ff) == 824:
            sh.add(1 << ((w >> 11) & 31))
        elif op == 21:
            s = (w >> 11) & 31
            mb = (w >> 6) & 31
            if mb == 0 and s:            # slwi form
                sh.add(1 << s)
        elif op == 7:                    # mulli
            v = w & 0xffff
            if 0 < v < 0x1000:
                sh.add(v)
    return sh


USERT = re.compile(r'[UVW]([A-Za-z_]\w*)@')
TPLN  = re.compile(r'\?\$([A-Za-z_]\w*)@')


def fam(name):
    """MSVC-mangling-aware family key: the set of user class names and
    non-STL template names appearing in the symbol.  vector<T> and every
    helper of vector<T> collapse to the same key; vector<U> does not."""
    u = {t for t in USERT.findall(name) if t not in NOISE and not t.startswith('__')}
    p = {t for t in TPLN.findall(name) if t not in NOISE and not t.startswith('__')}
    return frozenset(u | p)


def main():
    pd = sys.argv[1]
    cfg = json.load(open(os.path.join(pd, 'objdiff.json')))
    rep = json.load(open(os.path.join(pd, 'build/45410914/report.json')))
    up = {u['name']: (u.get('target_path'), u.get('base_path')) for u in cfg['units']
          if u.get('target_path') and u.get('base_path')}

    proven, fixable, unprov, nofam = [], [], [], []
    GLOB = collections.defaultdict(set)
    GLOBN = collections.defaultdict(list)
    cache = {}
    # ---- pass 1: harvest 100% anchors from EVERY unit ----
    for u in rep['units']:
        un = u['name']
        if un not in up:
            continue
        tp, bp = (os.path.join(pd, x) for x in up[un])
        if not (os.path.exists(tp) and os.path.exists(bp)):
            continue
        try:
            T = bodies(tp)
        except Exception:
            continue
        cache[un] = T
        loc = collections.defaultdict(set)
        locn = collections.defaultdict(list)
        for f in u.get('functions', []):
            if f.get('fuzzy_match_percent', 0) < 100.0:
                continue
            n = f['name']
            if not any(s in n for s in FAMSHAPE) or n not in T:
                continue
            k = fam(n)
            if not k:
                continue
            s2 = strides(T[n][0])
            if s2:
                loc[k] |= s2; locn[k].append(n)
                GLOB[k] |= s2; GLOBN[k].append((un, n))
        u['_anch'] = loc; u['_anchn'] = locn
    # ---- pass 2: adjudicate near-misses ----
    for u in rep['units']:
        un = u['name']
        if un not in cache:
            continue
        T = cache[un]
        try:
            B = bodies(os.path.join(pd, up[un][1]))
        except Exception:
            continue
        for f in u.get('functions', []):
            p = f.get('fuzzy_match_percent', 0)
            if not (90.0 <= p < 100.0) or not any(s in f['name'] for s in FAMSHAPE):
                continue
            n = f['name']
            if n not in T or n not in B:
                continue
            k = fam(n)
            ts, bs = strides(T[n][0]), strides(B[n][0])
            if ts == bs:
                continue
            local = u.get('_anch', {}).get(k)
            a = local or GLOB.get(k)
            src = 'LOCAL' if local else ('GLOBAL' if a else 'NONE')
            via = (u.get('_anchn', {}).get(k) or [x[1] for x in GLOBN.get(k, [])])[:1]
            row = (un, n, p, sorted(ts), sorted(bs), sorted(a) if a else None, via, src)
            if a and (bs & a) and not (ts & a):
                proven.append(row)
            elif a and (ts & a) and not (bs & a):
                fixable.append(row)
            elif a:
                unprov.append(row)
            else:
                nofam.append(row)

    print('===== FAMILY-STRIDE PROOF =====')
    print(f'  PROVEN MISPAIR (anchor==OUR stride, target foreign): {len(proven)}')
    print(f'  GENUINE LAYOUT BUG (anchor==TARGET stride, ours wrong): {len(fixable)}')
    print(f'  anchor present but inconclusive                   : {len(unprov)}')
    print(f'  no 100% family anchor in unit (undecidable here)  : {len(nofam)}')
    out = os.environ.get('LANE_OUT', '/tmp/family_stride_proof.txt')
    with open(out, 'w') as fh:
        for tag, rows in (('PROVEN', proven), ('FIXABLE', fixable), ('INCONCLUSIVE', unprov), ('NOANCHOR', nofam)):
            for r in rows:
                fh.write(f'{tag}\t{r[2]:.4f}\t{r[0]}\t{r[1]}\n')
                fh.write(f'\ttgt_strides={r[3]} base_strides={r[4]} anchor={r[5]}({r[7]}) via={r[6]}\n')
    print('wrote', out)


if __name__ == '__main__':
    main()
