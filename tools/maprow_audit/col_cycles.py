#!/usr/bin/env python3
"""Lane SOLVER-1: the multiset solver over lane ORACLE-1's RTTI-COL anchor.

WHY A STEP IS UNSAFE AND A CYCLE IS NOT
---------------------------------------
The $4PPPPPPPM@ adjustor thunks are byte-twins, so a WRONG name on one still
scores a clean 100 on the `match_percent_normalized` ruler.  The incumbents
therefore draw FAKE CREDIT, and correcting one row removes an incumbent while
the symbol it displaced is left with no address at all.  ORACLE-1 measured the
consequence exactly: landing all 40 of its verified repairs read -15 matched,
with 36 rows lost -- every one carrying a removed incumbent -- vs 21 gained.

So a name may only be corrected as part of a block that CONSERVES THE MULTISET
of names.  Let cur[A] be the incumbent at address A and new[A] the anchor's
proposal, and define

    sigma(A) = B   such that   cur[B] == new[A]

("the name A wants is currently parked at B").  Repairing A strands B unless B
is repaired too, so sigma IS the dependency edge.  Two shapes are closable:

  CYCLE  A1 -> A2 -> ... -> An -> A1.  Then new[Ai] == cur[A(i+1)] for all i, so
         the multiset after equals the multiset before.  Nothing stranded,
         nothing invented.  This is the only PURE shape.

  CHAIN  A1 -> ... -> An with sigma(An) undefined (its proposal is held by
         nobody).  Every incumbent except the HEAD's is re-taken by its
         predecessor; cur[A1] would be stranded, so the chain closes only if
         the anchor proposes cur[A1] at some address that is currently
         UNMAPPED.  Then nothing is stranded and exactly one name is added.
         (ORACLE-1 landed the n==1 case and called it a "completed move"; it
         never followed sigma forward, so every n>=2 chain was invisible to it.)

TWO PROPERTIES WORTH STATING, BOTH FREE
  - sigma is INJECTIVE on the candidate set (measured in-degree histogram
    {1: 47}), so the graph is a disjoint union of simple paths and cycles --
    there is no branching to resolve.
  - A cycle is closed over CANDIDATES only, so it can never steal a name from a
    branch-certified AGREE row.  The "do not touch a row whose far end is an
    AGREE row" hazard is enforced BY CONSTRUCTION rather than by a filter that
    could be forgotten.

WHAT THE RULERS DO HERE (measured, and it is NOT what the author predicted)
  An in-place rewrite inside a cycle/chain is worth ZERO functions -- the row
  read mpn 100 with the wrong name and reads mpn 100 with the right one -- but
  it IS worth bytes, because a wrong branch-target name is an ARG-ONLY penalty:
  `match_percent_normalized` excludes those and `fuzzy_match_percent` includes
  them.  Measured over this block: mpn ruler 9 gained / 3 lost = +6, while the
  fuzzy ruler (which `matched_code` follows) gained 22 rows and lost NONE.
  => Do not price this vein on Delta-matched alone.

CONTROL.  The verification must be able to reject a permutation, not merely a
multiset.  --selftest rotates each cycle's assignment by one extra step: that is
a DIFFERENT permutation of the SAME multiset, so it passes conservation and
injectivity identically and only the independent branch channel can tell it
apart.  Measured: the real block verifies AGREE/AGREE_METHOD, the rotated sham
stays METHOD_DIFFERS.
"""
import sys, os, json, glob, pickle, collections, argparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
import col_anchor as CA                                        # noqa: E402
from rtti_vtable_index import Rtti, demangle_class             # noqa: E402
from retail_reader import Image as RImage                      # noqa: E402
from thunk_identity import (Image as TImage, code_population,   # noqa: E402
                            adjudicate_strict)


# ------------------------------------------------------- secondary pairing
# An adjustor thunk exists BECAUSE of multiple inheritance, so it lives only in
# a SECONDARY sub-object vtable -- 173/174 of the suspect rows sit in some
# vtable and NONE at COL offset 0.  Primary vtables therefore adjudicate ZERO of
# this vein, and secondaries carry no offset we can join on directly.  They are
# paired instead by a map-independent signal: both sides agreeing on WHICH SLOTS
# ARE INHERITED UNCHANGED from the base's primary vtable.
def _basecls(b):
    return b[:-2] if b.endswith('@@') else b


def pair_secondaries(objroot=CA.OBJROOT, exe=CA.BAND, min_score=0.95):
    rt = Rtti(RImage(exe))
    rt.build_attribution()

    ours = collections.defaultdict(dict)
    for p in glob.glob(os.path.join(objroot, '**', '*.obj'), recursive=True):
        try:
            vts = CA.coff_vtables(p)
        except Exception:
            continue
        for sym, slots in vts.items():
            m = CA._VT_SYM.match(sym)
            if m:
                ours[m.group(1)].setdefault(m.group(2), slots)

    ret = collections.defaultdict(list)
    for vt, (nm, off, sl) in rt.vt_slots.items():
        c = demangle_class(nm or '')
        if c:
            ret[c].append((off, vt, sl))

    def inh_ours(cls, base, slots):
        bp = ours.get(_basecls(base), {}).get('')
        return None if not bp else tuple(i < len(bp) and slots[i] == bp[i]
                                         for i in range(len(slots)))

    def inh_ret(bcls, slots):
        cand = [s for (o, v, s) in ret.get(bcls, []) if o == 0]
        if not cand:
            return None
        bp = cand[0]
        return tuple(i < len(bp) and slots[i] == bp[i] for i in range(len(slots)))

    pairs, stats = [], collections.Counter()
    for cls, od in ours.items():
        rl = ret.get(cls)
        if not rl:
            continue
        osec = {b: s for b, s in od.items() if b}
        rsec = [(o, v, s) for (o, v, s) in rl if o]
        if not osec or not rsec:
            continue
        for b, oslots in osec.items():
            ov = inh_ours(cls, b, oslots)
            cands = [(o, v, s) for (o, v, s) in rsec if len(s) == len(oslots)]
            if not cands:
                stats['no_len_match'] += 1
                continue
            scored = []
            for (o, v, s) in cands:
                rv = inh_ret(_basecls(b), s)
                scored.append(((sum(1 for x, y in zip(ov, rv) if x == y) / max(len(ov), 1))
                               if (ov and rv) else -1.0, o, v, s))
            scored.sort(key=lambda t: -t[0])
            best = scored[0]
            if best[0] < 0:
                stats['no_base_vtable'] += 1
                continue
            if len(scored) > 1 and scored[1][0] == best[0]:
                stats['tie'] += 1
                continue
            stats['paired'] += 1
            pairs.append(dict(cls=cls, base=_basecls(b), score=best[0],
                              off=best[1], vt=best[2], ret=best[3], ours=oslots))
    return rt, pairs, stats


def build_anchor(tmap, objroot=CA.OBJROOT, exe=CA.BAND, min_score=0.95):
    """-> {va: {cls, slot, ours(proposed name), ...}} over UNIQUELY-proposed VAs.

    The unique-VA filter is the ICF containment: 200 of 4,957 VAs carry >1
    proposed name because identical COMDATs fold, and they account for the bulk
    of DISAGREE while yielding little AGREE.  Leaving them in would contaminate
    every downstream verdict.
    """
    rt, pairs, stats = pair_secondaries(objroot, exe, min_score)
    prim, _conf, _srcs = CA.our_vtables(objroot)
    res = CA.compare(CA.join(prim, CA.retail_vtables(rt)), tmap)
    for p in pairs:
        if p['score'] < min_score:
            continue
        for i, (va, nm) in enumerate(zip(p['ret'], p['ours'])):
            k = '0x%08x' % va
            cur = tmap.get(k)
            res.append(dict(cls=p['cls'], base=p['base'], slot=i, va=va, key=k,
                            ours=nm, cur=cur,
                            verdict='UNMAPPED' if cur is None
                            else ('AGREE' if cur == nm else 'DISAGREE')))
    byva = collections.defaultdict(set)
    for x in res:
        byva[x['va']].add(x['ours'])
    mult = {v for v, s in byva.items() if len(s) > 1}
    anchor = {x['va']: x for x in res if x['va'] not in mult}
    return anchor, res, mult, stats


# --------------------------------------------------------------- the solver
def solve(map_path=CA.MAP, objroot=CA.OBJROOT, exe=CA.BAND):
    raw = json.load(open(map_path))
    deny = {int(x.lower().removeprefix('0x'), 16) for x in raw.get('_denylist', [])}
    tmap = {k: v for k, v in raw.items() if k.lower().startswith('0x')}
    anchor, _res, mult, stats = build_anchor(tmap, objroot, exe)

    timg = TImage(exe)
    recs = [adjudicate_strict(timg, tmap, a, n)
            for a, n in sorted(code_population(timg, tmap).items())]
    by = collections.defaultdict(list)
    for r in recs:
        by[r['verdict']].append(r)

    name2addr = collections.defaultdict(set)
    for k, v in tmap.items():
        if isinstance(v, str):
            name2addr[v].add(int(k, 16))

    # candidates: the SUSPECT stratum only (anchor reproduces the incumbent on
    # 99.1% of the untreated AGREE control but only 1.4% here)
    cand = {}
    for r in by['METHOD_DIFFERS']:
        a = int(r['addr'], 16)
        x = anchor.get(a)
        if x and x['ours'] != r['proposed'] and a not in deny:
            cand[a] = dict(addr=a, cur=r['proposed'], new=x['ours'],
                           cls=x['cls'], slot=x['slot'])

    sigma = {}
    for a, cd in cand.items():
        holders = name2addr.get(cd['new'], set()) - {a}
        if len(holders) == 1:
            sigma[a] = next(iter(holders))
    sig = {a: b for a, b in sigma.items() if b in cand}

    # cycles
    cycles, seen = [], set()
    for start in sig:
        if start in seen:
            continue
        path, cur_, onp = [], start, {}
        while cur_ in sig and cur_ not in onp:
            onp[cur_] = len(path)
            path.append(cur_)
            cur_ = sig[cur_]
        if cur_ in onp:
            cyc = path[onp[cur_]:]
            if all(p not in seen for p in cyc):
                seen.update(cyc)
                cycles.append(cyc)

    # chains: walk forward from every node with no sigma-predecessor
    prop = collections.defaultdict(set)
    for va, x in anchor.items():
        prop[x['ours']].add(va)
    heads = [a for a in cand if a not in set(sig.values())]
    chains = []
    for h in heads:
        path, cur_, loc = [], h, set()
        while cur_ in sig and cur_ not in loc:
            loc.add(cur_)
            path.append(cur_)
            cur_ = sig[cur_]
        if cur_ in cand and cur_ not in path:
            path.append(cur_)
        tail = path[-1]
        if tail in sigma:                       # leaves the candidate set
            continue
        orph = cand[path[0]]['cur']
        free = sorted(v for v in (prop.get(orph, set()) - {path[0]})
                      if ('0x%08x' % v) not in tmap and v not in deny)
        if len(free) == 1 and free[0] not in cand:
            chains.append((path, orph, free[0]))
    return dict(raw=raw, tmap=tmap, deny=deny, anchor=anchor, cand=cand,
                sigma=sigma, sig=sig, cycles=cycles, chains=chains, by=by,
                timg=timg, stats=stats, mult=mult)


def assemble(S, require_verified=True):
    """-> edits {addr: name}, with every precondition ASSERTED not assumed."""
    cand, tmap = S['cand'], S['tmap']
    edits, prov = {}, {}
    for cyc in S['cycles']:
        assert (collections.Counter(cand[a]['cur'] for a in cyc)
                == collections.Counter(cand[a]['new'] for a in cyc)), 'cycle multiset broken'
        for a in cyc:
            edits[a], prov[a] = cand[a]['new'], 'CYCLE(len%d)' % len(cyc)
    zs = [z for _p, _o, z in S['chains']]
    assert len(set(zs)) == len(zs), 'two chains re-home to the same address'
    for p, orph, z in S['chains']:
        if any(a in edits for a in p) or z in edits:
            continue
        for a in p:
            edits[a], prov[a] = cand[a]['new'], 'CHAIN(len%d)' % len(p)
        edits[z], prov[z] = orph, 'CHAIN-REHOME'

    if require_verified:
        sim = dict(tmap)
        for a, n in edits.items():
            sim['0x%08x' % a] = n
        bad = {a for a, n in edits.items()
               if adjudicate_strict(S['timg'], sim, '0x%08x' % a, n)['verdict']
               not in ('AGREE', 'AGREE_METHOD')}
        # dropping a row would strand its chain -- drop the WHOLE chain
        for p, orph, z in S['chains']:
            if z in bad or any(a in bad for a in p):
                for a in list(p) + [z]:
                    edits.pop(a, None)
                    prov.pop(a, None)

    # --- hard preconditions ---
    removed = collections.Counter(tmap[k] for k in ('0x%08x' % a for a in edits) if k in tmap)
    added = collections.Counter(edits.values())
    stranded = removed - added
    assert not stranded, 'a symbol would be STRANDED: %s' % list(stranded)[:3]
    assert not (set(edits) & S['deny']), 'edit touches a denylisted address'
    sim = dict(tmap)
    for a, n in edits.items():
        sim['0x%08x' % a] = n
    allow = set(S['raw'].get('_internal_linkage_allow', []))

    def dups(m):
        n2a = collections.defaultdict(list)
        for k, v in m.items():
            if isinstance(v, str):
                n2a[v].append(k)
        return {v: a for v, a in n2a.items() if len(a) > 1 and v not in allow}
    assert len(dups(sim)) <= len(dups(tmap)), 'injectivity would REGRESS'
    return edits, prov, dict(stranded=0, invented=sum((added - removed).values()),
                             inj_before=len(dups(tmap)), inj_after=len(dups(sim)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--map', default=CA.MAP, help='map to solve against (default: the live map)')
    ap.add_argument('--json', help='write the edit set here')
    a = ap.parse_args()

    S = solve(map_path=a.map)
    print('secondary pairing: %s' % dict(S['stats']))
    print('branch channel   : %s' % {k: len(v) for k, v in S['by'].items()})
    print('anchor           : %d uniquely-proposed VAs (%d ICF-suspect VAs excluded)'
          % (len(S['anchor']), len(S['mult'])))
    print('candidates       : %d ; sigma|cand edges %d ; in-degree %s'
          % (len(S['cand']), len(S['sig']),
             dict(sorted(collections.Counter(
                 collections.Counter(S['sig'].values()).values()).items()))))
    print('CLOSED CYCLES    : %d over %d addresses %s'
          % (len(S['cycles']), sum(len(c) for c in S['cycles']),
             dict(collections.Counter(len(c) for c in S['cycles']))))
    print('CLOSABLE CHAINS  : %d over %d addresses (+%d re-home rows)'
          % (len(S['chains']), sum(len(c[0]) for c in S['chains']), len(S['chains'])))

    edits, prov, st = assemble(S)
    print('\nEDIT SET %d addresses %s' % (len(edits), dict(collections.Counter(prov.values()))))
    print('  stranded %d ; invented %d ; injectivity %d -> %d'
          % (st['stranded'], st['invented'], st['inj_before'], st['inj_after']))

    if a.selftest:
        # the block must verify, and a DIFFERENT permutation of the SAME
        # multiset must NOT -- otherwise the verification is vacuous.
        def probe(m, label):
            sim = dict(S['tmap'])
            for x, n in m.items():
                sim['0x%08x' % x] = n
            c = collections.Counter(adjudicate_strict(S['timg'], sim, '0x%08x' % x, n)['verdict']
                                    for x, n in m.items())
            print('  %-34s %s' % (label, dict(c)))
            return c
        print('\n[selftest]')
        real = probe(edits, 'real block (must verify)')
        sham = {}
        for cyc in S['cycles']:
            for i, x in enumerate(cyc):
                sham[x] = S['cand'][cyc[(i + 2) % len(cyc)]]['cur']
        if sham:
            assert (collections.Counter(sham.values())
                    == collections.Counter(edits[x] for x in sham)), 'sham is not the same multiset'
            bad = probe(sham, 'rotated sham (must NOT verify)')
            assert bad.get('AGREE', 0) + bad.get('AGREE_METHOD', 0) == 0, \
                'VACUOUS: the sham verified too'
            print('  => the channel discriminates PERMUTATIONS, not just multisets')
        assert real.get('METHOD_DIFFERS', 0) == 0

    if a.json:
        json.dump({'0x%08x' % k: v for k, v in sorted(edits.items())},
                  open(a.json, 'w'), indent=1)
        print('wrote %s' % a.json)


if __name__ == '__main__':
    main()
