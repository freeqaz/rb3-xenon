#!/usr/bin/env python3
"""TU-locality identification: home a byte-identical twin by its retail NEIGHBOURS.

Why this is not the dead "neighbourhood fingerprint" idea
--------------------------------------------------------
Lane K measured and killed every probe computed from the function's *own* bytes
(`.pdata` prolog shape, call-graph shape, frame size): a hit set is built out of
functions whose masked machine code is equal, so any such probe is constant
within the set by construction.  It also looked at raw neighbourhood *byte*
fingerprints (`prev4`, alignment, `.pdata` gap) -- those do vary, but they
describe the RETAIL neighbourhood and our COMDAT-per-function objs have no
matching key, so they are not evidence.

This tool uses a neighbourhood key that **does** exist on both sides: **TU
membership**.  The retail build is `/O1` with no LTCG and no cross-TU
reordering, so TU spatial grouping in `.text` is preserved.  Therefore:

    if our function F is compiled by TU T, and candidate VA v is surrounded (in
    .pdata address order) by retail functions the map already homes to names
    that TU T also compiles, then v is F's home -- and a competitor v'
    surrounded by names from other TUs is not.

Honesty model
-------------
A mispaired ICF twin still reads 100%, so a wrong accept is invisible and
permanently corrupts the map.  Consequently:

* the accept rule is a **decisive margin**, never an argmax: the winner needs
  >= `--min-same` same-TU neighbours AND every competitor must have <=
  `--max-rival` (default 0).  Ties are refused and counted.
* COMDAT **scatter** is real in this tree, so TU-locality is a *statistical*
  signal.  The operating point is chosen for precision, not reach.
* precision is measured **held-out** with the same protocol as
  `caller_side_invert.py --validate`, and -- because this tool sweeps a large
  grid of operating points -- the grid is searched on a **DEV split** and the
  reported number comes from a **disjoint TEST split** (`--split`).  Selecting
  an operating point on the same data you report is overfitting; don't.

Modes
-----
    --sweep              grid search on the DEV split, full precision/reach curve
    --validate           held-out precision at one operating point (TEST split)
    --joint              additionally measure caller-side inversion alone and
                         the TU-locality AND caller-side intersection, on the
                         same records, bucketed by sibling-family size
    (default)            production resolve -> proposals.json in homing_scan
                         result format (records re-classified UNIQUE)

Usage
-----
    tu_locality_invert.py --results merged.json --worktree WT --sweep
    tu_locality_invert.py --results merged.json --worktree WT --validate \\
        --window 3 --min-same 4 --pure --adjacent --max-family 99
    tu_locality_invert.py --results merged.json --worktree WT \\
        --window 3 --min-same 4 --pure --adjacent --max-family 99 \\
        --out prop.json
"""
import argparse
import json
import os
import pickle
import re
import sys
import zlib
from collections import defaultdict, Counter
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from multi_content_disambiguate import Band, func_table, load_tmap  # noqa: E402
from funclet_cascade_rank import PE, parse_pdata                    # noqa: E402

# Funclets are not real TU-locality evidence: MSVC emits them next to (or far
# from) their parent by its own rules and they carry the parent's name mangled,
# so counting them would double-count the parent.  Dropped from the spine.
FUNCLET_RX = re.compile(r'__unwind|__ehhandler|__catch|__tls|__GSHandler')
# auto_03_* spans are XDK vendor + Quazal, hard-skipped by the project owner.
VENDOR_RX = re.compile(r'^(?:default/)?auto_\d+_')

FAM_BUCKETS = ('1', '2-4', '5-16', '17-99', '100+')


def fam_bucket(fs):
    if fs == 1:
        return '1'
    if fs <= 4:
        return '2-4'
    if fs <= 16:
        return '5-16'
    if fs < 100:
        return '17-99'
    return '100+'


# --------------------------------------------------------------------- spine
class Spine:
    """Retail .pdata entries in address order = the neighbourhood spine."""

    def __init__(self, pe, tmap, name2tus, mapped_only=False):
        pd = parse_pdata(pe)
        vas = []
        for v in sorted(pd):
            nm = tmap.get(v)
            if nm and FUNCLET_RX.search(nm):
                continue                       # explicit funclet drop
            if mapped_only and nm is None:
                continue
            vas.append(v)
        self.va = vas
        self.idx = {v: i for i, v in enumerate(vas)}
        self.tus = [name2tus.get(tmap.get(v)) if tmap.get(v) else None for v in vas]
        self.n = len(vas)

    def profile(self, v, tus, K, radius=0):
        """-> (same, diff, unk, adj) for candidate VA `v` against TU set `tus`.

        `tus` is a SET because a name may be compiled by several TUs (templates
        / inline functions land in every obj that instantiates them).  The
        linker picks one obj's COMDAT, so a neighbour counts as same-TU if it
        shares ANY of the name's TUs.  adj = 1 iff an immediately-adjacent
        spine entry is same-TU.
        """
        i = self.idx.get(v)
        if i is None:
            return None
        same = diff = unk = adj = 0
        lo, hi = max(0, i - K), min(self.n, i + K + 1)
        for j in range(lo, hi):
            if j == i:
                continue
            if radius and abs(self.va[j] - v) > radius:
                continue
            t = self.tus[j]
            if not t:
                unk += 1
                continue
            if t & tus:
                same += 1
                if abs(j - i) == 1:
                    adj = 1
            else:
                diff += 1
        return same, diff, unk, adj


def pick(spine, tus, cands, P):
    """Decisive-margin resolve.  -> (verdict, va, profiles)"""
    pr = {}
    for c in cands:
        pr[c] = spine.profile(c, tus, P['K'], P['radius']) or (0, 0, 0, 0)
    win = [c for c in cands
           if pr[c][0] >= P['min_same']
           and (not P['pure'] or pr[c][1] == 0)
           and (not P['adjacent'] or pr[c][3])]
    if not win:
        return 'NO-SIGNAL', None, pr
    if len(win) > 1:
        return 'TIE', None, pr
    w = win[0]
    if any(pr[c][0] > P['max_rival'] for c in cands if c != w):
        return 'RIVAL', None, pr
    return 'RESOLVED', w, pr


# ------------------------------------------------------------------- indexes
def build_index_cache(wt, res, band, tmap, name2va, path):
    """famsize (real, from obj bodies) + caller-side claims/sites.  Cached."""
    if path and os.path.exists(path):
        try:
            with open(path, 'rb') as fh:
                return pickle.load(fh)
        except Exception:
            pass
    from caller_side_invert import Index, build_claims, masked_eq
    idx = Index(wt, res)
    print('index: %d objs, %d compiled functions' % (idx.n_obj, len(idx.fn)),
          file=sys.stderr)
    fam = defaultdict(set)
    for (tu, cname), f in idx.fn.items():
        fam[f['body']].add(cname)
    famsize = {}
    for (tu, cname), f in idx.fn.items():
        famsize[cname] = len(fam[f['body']])
    del fam
    anchors = {}
    for (tu, cname), f in idx.fn.items():
        if cname in anchors:
            continue
        vas = name2va.get(cname)
        if not vas or len(vas) != 1:
            continue
        va = next(iter(vas))
        if not masked_eq(f['body'], band.text_bytes(va, f['size']), f['offs']):
            continue
        anchors[cname] = va
    print('anchors: %d' % len(anchors), file=sys.stderr)
    claims, sites = build_claims(band, idx, anchors)
    byname = defaultdict(set)
    for v, names in claims.items():
        for n in names:
            byname[n].add(v)
    blob = dict(famsize=famsize,
                claims={v: sorted(s) for v, s in claims.items()},
                byname={n: sorted(s) for n, s in byname.items()},
                nanchor=len(anchors))
    if path:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as fh:
            pickle.dump(blob, fh, -1)
    return blob


def caller_pick(name, cands, byname, claims):
    """caller_side_invert.resolve, family cap LIFTED.  -> (verdict, va)"""
    vs = byname.get(name)
    if not vs:
        return 'NO-ANCHOR', None
    if len(vs) > 1:
        return 'DISAGREE', None
    v = vs[0]
    if set(claims.get(v, ())) != {name}:
        return 'SHARED-VA', None
    if v not in cands:
        return 'NOT-IN-HITS', None
    return 'RESOLVED', v


# ---------------------------------------------------------------- held-out set
def held_out(res, tmap, famsize, name2tus, skip_vendor=True):
    """Mirror production exactly; the resolver is never told which is truth.

    ONE RECORD PER NAME -- the map is name -> VA, so the decision unit is the
    name, not the (TU, name) pair.  `caller_side_invert.py` dedups the same way
    (`targets.setdefault(r['name'], ...)`); keeping per-TU duplicates would
    weight precision by template instantiation count.
    """
    out = {}
    for tu, recs in sorted(res.items()):
        if not isinstance(recs, list):
            continue
        if skip_vendor and VENDOR_RX.match(tu):
            continue
        for r in recs:
            if r['name'] in out or len(r.get('hits') or ()) < 2:
                continue
            hits = [int(h, 16) for h in r['hits']]
            truth = [v for v in hits if tmap.get(v) == r['name']]
            if len(truth) != 1:
                continue
            truth = truth[0]
            cands = {v for v in hits if v not in tmap} | {truth}
            if len(cands) < 2:
                continue
            out[r['name']] = dict(tus=frozenset(name2tus[r['name']]),
                                  name=r['name'], truth=truth,
                                  cands=sorted(cands),
                                  fs=famsize.get(r['name'], 1))
    return list(out.values())


def split_of(rec, nsplit=2):
    """Deterministic DEV/TEST split, by NAME so a name never straddles."""
    return zlib.crc32(rec['name'].encode()) % nsplit


def measure(records, spine, P, maxfam=0, contest=True):
    """Held-out precision with the production contested-drop pass."""
    picks = defaultdict(list)
    st = Counter()
    for rec in records:
        if maxfam and rec['fs'] > maxfam:
            st['BIG-FAMILY'] += 1
            continue
        verdict, v, pr = pick(spine, rec['tus'], rec['cands'], P)
        st[verdict] += 1
        if verdict == 'RESOLVED':
            picks[v].append(rec)
    for v, cl in sorted(picks.items()):
        if contest and len({r['name'] for r in cl}) > 1:
            st['DROP-CONTESTED'] += len(cl)
            st['RESOLVED'] -= len(cl)
            continue
        for rec in cl:
            ok = (v == rec['truth'])
            tag = 'HIT' if ok else 'MISS'
            st[tag] += 1
            st['%s/%s' % (fam_bucket(rec['fs']), tag)] += 1
    return st


def prec(st, pfx=''):
    h, m = st.get(pfx + 'HIT', 0), st.get(pfx + 'MISS', 0)
    return (h, m, 100.0 * h / (h + m) if h + m else float('nan'))


def fam_table(st, label):
    h, m, p = prec(st)
    print('  %-28s %5d/%-5d = %6.2f%%' % (label, h, h + m, p))
    for b in FAM_BUCKETS:
        hh, mm = st.get('%s/HIT' % b, 0), st.get('%s/MISS' % b, 0)
        if hh + mm:
            print('      famsize %-6s %5d/%-5d = %6.2f%%'
                  % (b, hh, hh + mm, 100.0 * hh / (hh + mm)))


# ------------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--results', required=True)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--band')
    ap.add_argument('--tmap')
    ap.add_argument('--cache', default='/home/free/tmp/laneM-tuloc/index_cache.pkl')
    ap.add_argument('--out', default='/home/free/tmp/laneM-tuloc/tuloc_prop.json')
    ap.add_argument('--report', default='/home/free/tmp/laneM-tuloc/tuloc_report.txt')
    ap.add_argument('--classes', default='MULTI,UNIQUE-ICF')
    # operating point
    ap.add_argument('--window', type=int, default=3, dest='K',
                    help='k nearest .pdata entries on EACH side')
    ap.add_argument('--radius', type=int, default=0,
                    help='additionally require |neighbour - candidate| <= R bytes')
    ap.add_argument('--min-same', type=int, default=4)
    ap.add_argument('--max-rival', type=int, default=0)
    ap.add_argument('--pure', action='store_true',
                    help='winner window must contain NO other-TU homed neighbour')
    ap.add_argument('--adjacent', action='store_true',
                    help='an immediately-adjacent .pdata entry must be same-TU')
    ap.add_argument('--max-family', type=int, default=99,
                    help='refuse when >N of OUR names share the masked bytes')
    ap.add_argument('--channel', default='tuloc', choices=['tuloc', 'confirm'],
                    help="tuloc = TU-locality resolves on its own (decisive "
                         "margin).  confirm = caller-side inversion derives the "
                         "home with its family cap LIFTED and TU-locality only "
                         "has to agree -- the fundable big-family channel.")
    ap.add_argument('--confirm-min', type=int, default=3,
                    help='--channel confirm: minimum same-TU neighbours at the '
                         'caller-derived VA (and strictly more than any rival)')
    ap.add_argument('--min-family', type=int, default=1,
                    help='refuse when FEWER than N of our names share the masked '
                         'bytes.  17 = exactly the BIG-FAMILY pool caller-side '
                         'inversion refuses.')
    ap.add_argument('--mapped-spine', action='store_true',
                    help='spine = mapped entries only (window reaches further)')
    # modes
    ap.add_argument('--sweep', action='store_true')
    ap.add_argument('--validate', action='store_true')
    ap.add_argument('--joint', action='store_true')
    ap.add_argument('--split', default='test', choices=['dev', 'test', 'all'])
    a = ap.parse_args()

    wt = a.worktree
    band = Band(a.band or os.path.join(wt, 'orig/45410914/band.exe'))
    pe = PE(Path(a.band or os.path.join(wt, 'orig/45410914/band.exe')))
    tmap, name2va = load_tmap(a.tmap or os.path.join(wt, 'scripts/target_symbol_map.json'))
    res = json.load(open(a.results))

    name2tus = defaultdict(set)
    for tu, recs in res.items():
        if not isinstance(recs, list):
            continue
        for r in recs:
            name2tus[r['name']].add(tu)

    cache = build_index_cache(wt, res, band, tmap, name2va, a.cache)
    famsize = cache['famsize']
    claims, byname = cache['claims'], cache['byname']

    spine = Spine(pe, tmap, name2tus, a.mapped_spine)
    print('spine: %d entries (%s)' % (spine.n, 'mapped-only' if a.mapped_spine
                                      else 'all .pdata, funclets dropped'),
          file=sys.stderr)

    P = dict(K=a.K, radius=a.radius, min_same=a.min_same,
             max_rival=a.max_rival, pure=a.pure, adjacent=a.adjacent)

    # ------------------------------------------------------------------ sweep
    if a.sweep:
        recs = [r for r in held_out(res, tmap, famsize, name2tus)
                if split_of(r) == 0]
        print('DEV split: %d held-out records' % len(recs))
        rows = []
        for K in (2, 3, 4, 6, 8):
            for minS in (2, 3, 4, 5, 6):
                if minS > 2 * K:
                    continue
                for pure in (0, 1):
                    for adjv in (0, 1):
                        for mf in (16, 99, 0):
                            Q = dict(K=K, radius=0, min_same=minS, max_rival=0,
                                     pure=pure, adjacent=adjv)
                            st = measure(recs, spine, Q, maxfam=mf)
                            h, m, p = prec(st)
                            if h + m < 25:
                                continue
                            rows.append((p, h + m, K, minS, pure, adjv, mf, h, m,
                                         st.get('TIE', 0), st.get('DROP-CONTESTED', 0)))
        rows.sort(reverse=True)
        print('\n%-7s %-6s | %-2s %-4s %-4s %-4s %-5s | %-5s %-5s %-4s %-5s'
              % ('prec', 'reach', 'K', 'minS', 'pure', 'adj', 'maxfam',
                 'HIT', 'MISS', 'tie', 'contd'))
        for p, n, K, minS, pure, adjv, mf, h, m, tie, cd in rows:
            print('%-6.2f%% %-6d | %-2d %-4d %-4d %-4d %-5s | %-5d %-5d %-4d %-5d'
                  % (p, n, K, minS, pure, adjv, mf or 'off', h, m, tie, cd))
        return

    # --------------------------------------------------------------- validate
    if a.validate or a.joint:
        allrec = held_out(res, tmap, famsize, name2tus)
        if a.split == 'all':
            recs = allrec
        else:
            want = 0 if a.split == 'dev' else 1
            recs = [r for r in allrec if split_of(r) == want]
        print('%s split: %d held-out records (of %d)'
              % (a.split.upper(), len(recs), len(allrec)))
        print('operating point: %s max_family=%s spine=%s'
              % (P, a.max_family or 'off',
                 'mapped-only' if a.mapped_spine else 'all'))

        st = measure(recs, spine, P, maxfam=a.max_family)
        fam_table(st, 'TU-LOCALITY alone')
        print('    refusals: %s' % dict(
            (k, v) for k, v in sorted(st.items())
            if k in ('NO-SIGNAL', 'TIE', 'RIVAL', 'BIG-FAMILY', 'DROP-CONTESTED')))

        if not a.joint:
            return

        # ---- caller-side alone (family cap LIFTED) and the intersection
        cs = Counter()
        both = Counter()
        tul = {}
        for rec in recs:
            v_t = None
            if not (a.max_family and rec['fs'] > a.max_family):
                verdict, v_t, _ = pick(spine, rec['tus'], rec['cands'], P)
                if verdict != 'RESOLVED':
                    v_t = None
            tul[rec['name']] = v_t
        cpicks = defaultdict(list)
        for rec in recs:
            verdict, v = caller_pick(rec['name'], set(rec['cands']), byname, claims)
            cs[verdict] += 1
            if verdict == 'RESOLVED':
                cpicks[v].append(rec)
        for v, cl in sorted(cpicks.items()):
            if len({r['name'] for r in cl}) > 1:
                cs['DROP-CONTESTED'] += len(cl)
                cs['RESOLVED'] -= len(cl)
                continue
            for rec in cl:
                ok = (v == rec['truth'])
                tag = 'HIT' if ok else 'MISS'
                cs[tag] += 1
                cs['%s/%s' % (fam_bucket(rec['fs']), tag)] += 1
                vt = tul.get(rec['name'])
                if vt is None:
                    both['NO-TULOC'] += 1
                elif vt != v:
                    both['DISAGREE'] += 1
                else:
                    both[tag] += 1
                    both['%s/%s' % (fam_bucket(rec['fs']), tag)] += 1
        fam_table(cs, 'CALLER-SIDE alone (cap off)')
        fam_table(both, 'INTERSECTION (strict: TU-locality resolves too)')
        print('    intersection drops: no-tuloc %d, disagree %d'
              % (both.get('NO-TULOC', 0), both.get('DISAGREE', 0)))

        # ---- TU-locality as a *confirmation filter* on caller-side picks.
        # Much weaker than making TU-locality resolve on its own: we only ask
        # that the TU evidence points the same way, so it keeps caller-side's
        # reach while (hopefully) shedding its big-family errors.
        for label, need_same, strict_gt in (
                ('CONFIRM no-contradiction (same >= every rival)', 0, False),
                ('CONFIRM argmax    (same >= 1, > every rival)', 1, True),
                ('CONFIRM argmax-2  (same >= 2, > every rival)', 2, True),
                ('CONFIRM argmax-3  (same >= 3, > every rival)', 3, True)):
            cf = Counter()
            for v, cl in sorted(cpicks.items()):
                if len({r['name'] for r in cl}) > 1:
                    continue
                for rec in cl:
                    pr = {c: spine.profile(c, rec['tus'], P['K'], P['radius'])
                          or (0, 0, 0, 0) for c in rec['cands']}
                    sv = pr[v][0]
                    rivals = [pr[c][0] for c in rec['cands'] if c != v]
                    mx = max(rivals) if rivals else 0
                    if sv < need_same or (sv > mx if strict_gt else sv >= mx) is False:
                        cf['REFUSED'] += 1
                        continue
                    tag = 'HIT' if v == rec['truth'] else 'MISS'
                    cf[tag] += 1
                    cf['%s/%s' % (fam_bucket(rec['fs']), tag)] += 1
            fam_table(cf, label)
            print('      refused %d' % cf.get('REFUSED', 0))
        return

    # ------------------------------------------------------------- production
    want_cls = set(a.classes.split(','))
    targets = {}
    for tu, recs in sorted(res.items()):
        if not isinstance(recs, list) or VENDOR_RX.match(tu):
            continue
        for r in recs:
            if not r.get('hits') or r.get('cls') not in want_cls:
                continue
            targets.setdefault(r['name'], (tu, r))   # one record per NAME
    print('targets: %d' % len(targets), file=sys.stderr)

    taken = set(tmap)
    stats = Counter()
    picks = defaultdict(list)
    lines = []
    for name, (tu, r) in sorted(targets.items()):
        hits = [int(h, 16) for h in r['hits']]
        if any(tmap.get(v) == name for v in hits):
            stats['ALREADY-HOMED'] += 1
            continue
        if a.max_family and famsize.get(name, 1) > a.max_family:
            stats['BIG-FAMILY'] += 1
            continue
        if famsize.get(name, 1) < a.min_family:
            stats['SMALL-FAMILY'] += 1
            continue
        cands = {v for v in hits if v not in taken}
        if len(cands) < 1:
            stats['NO-FREE-ADDR'] += 1
            continue
        tus = frozenset(name2tus[name])
        if a.channel == 'tuloc':
            verdict, v, pr = pick(spine, tus, cands, P)
            stats[verdict] += 1
            if verdict == 'RESOLVED':
                picks[v].append((tu, name, r, pr[v]))
            continue

        # ---- confirm channel: caller-side inversion DERIVES the home (family
        # cap lifted), TU-locality only has to point the same way.  Measured
        # held-out on famsize >= 17: 99.15 % (TEST) / 99.17 % (DEV) at
        # --confirm-min 3.  caller-side alone on that pool is 96-97 %.
        verdict, v = caller_pick(name, cands, byname, claims)
        if verdict != 'RESOLVED':
            stats['CS-' + verdict] += 1
            continue
        pr = {c: spine.profile(c, tus, P['K'], P['radius']) or (0, 0, 0, 0)
              for c in cands}
        sv = pr[v][0]
        rivals = [pr[c][0] for c in cands if c != v]
        if sv < a.confirm_min or (rivals and sv <= max(rivals)):
            stats['TULOC-UNCONFIRMED'] += 1
            continue
        stats['RESOLVED'] += 1
        picks[v].append((tu, name, r, pr[v]))

    out = defaultdict(list)
    for v, cl in sorted(picks.items()):
        if len({n for _, n, _, _ in cl}) > 1:
            stats['DROP-CONTESTED'] += len(cl)
            stats['RESOLVED'] -= len(cl)
            continue
        for tu, name, r, p in cl:
            nr = dict(r)
            nr['cls'] = 'UNIQUE'
            nr['va'] = '0x%08x' % v
            nr['n_unmapped'] = 1
            nr['disambig'] = ('TU-LOCALITY' if a.channel == 'tuloc'
                              else 'CALLER-SIDE+TU-LOCALITY')
            nr['evidence'] = dict(same=p[0], diff=p[1], unk=p[2], adj=p[3])
            out[tu].append(nr)
            lines.append('RESOLVED %-56s %s -> 0x%08x  same=%d diff=%d adj=%d'
                         % (name[:56], tu, v, p[0], p[1], p[3]))
    out = {k: v for k, v in out.items() if v}
    json.dump(out, open(a.out, 'w'), indent=1)
    open(a.report, 'w').write('\n'.join(lines) + '\n')
    print('verdicts:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('proposed %d resolutions across %d units'
          % (sum(len(v) for v in out.values()), len(out)))
    print('->', a.out)


if __name__ == '__main__':
    main()
