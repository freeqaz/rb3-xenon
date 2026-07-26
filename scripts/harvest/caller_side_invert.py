#!/usr/bin/env python3
"""Caller-side identification: invert the call graph to home a *contentless* function.

Why
---
`homing_scan.py` finds a retail home by reloc-masked byte identity; when several
retail VAs are byte-identical it reports `MULTI` / `UNIQUE-ICF` and refuses.
`multi_content_disambiguate.py` cracks part of that residue by reading what the
*callee itself* references (strings, `__real@` constants, trusted callees)
through the masked slots.

That leaves the functions with nothing to say about themselves: ~18k records
with **no** string / constant / trusted callee at any masked slot, and ~1.3k
where every candidate conflicts.  Those are structurally untouchable from the
callee side.

This tool attacks them from the other end.  Masking destroys the *callee's*
identity, but our object files still record, for every call site, **which symbol
the call points at**.  So:

    if a caller G is confidently homed at retail VA(G),
    and our G has a relocation at offset `o` naming callee F,
    then the retail instruction at VA(G)+o decodes to F's retail address.

That is a *derivation*, not a filter -- one homed caller determines the callee's
VA outright, and it needs zero content in F.  That is exactly why it reaches the
NO-EVIDENCE pool.

Honesty model
-------------
A mispair is invisible in objdiff's normalized score, so a wrong accept costs
more than a hundred refusals.  The tool is built around one global table rather
than per-function heuristics:

    claims[v] = { every callee symbol name that some anchor's relocation names
                  at a slot where retail resolves to VA v }

and six mandatory guards:

1. **Anchor verification** -- a caller G is an anchor only if its name maps to
   exactly one VA *and* our compiled G is reloc-masked byte-identical to retail
   at that VA.
2. **Unanimity** -- all anchors naming F must resolve to the same VA
   (`len(vs) == 1`), otherwise DISAGREE.
3. **Exclusivity** -- `claims[v] == {F}`.  If any *other* callee name is also
   resolved to v by some anchor, v is an ICF fold (or one of the two anchors is
   mispaired) and both names are refused.  This is the guard the naive
   formulation lacks, and it is what the template-instantiation families trip.
4. **Hit-set containment** -- v must be one of F's own reloc-masked
   byte-identical candidate VAs.  A vote from a mispaired anchor lands somewhere
   that is not byte-identical to our F and dies here.
5. **Unclaimed** -- v must not already be mapped to another name, and no two
   functions may derive the same v in one wave.
6. **Content guard** (`--content-guard`) -- the derived VA must additionally
   survive MAP-FREE content evidence (strings / float constants F itself
   references).  Vacuous for the NO-EVIDENCE pool by construction, decisive for
   everything else.

`--min-anchors N` demands N independent anchoring callers.
`--strict-anchors` further demands that the anchor is itself *unambiguously*
homed (its own reloc-masked byte-identity hit set has exactly one element), i.e.
it cannot be an ICF twin.

Held-out precision (`--validate`)
---------------------------------
Restrict to functions whose home is already known -- name maps to exactly one
VA, and that VA is in the hit set.  The candidate set is rebuilt to mirror
production exactly (all *unmapped* hits, plus the truth, since in production
both F and its home would be unmapped); the resolver never learns which
candidate is the truth.  Contested drops are applied before scoring, as in
production.

Usage
-----
    caller_side_invert.py --results merged.json --worktree WT \\
        [--validate] [--content-guard] [--strict-anchors] [--min-anchors 1] \\
        [--out proposals.json] [--report report.txt]

Emits homing_scan result format (records re-classified `UNIQUE`) so
`homing_gen4.py` / `homing_apply4.py` consume it unchanged.
"""
import argparse
import json
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from multi_content_disambiguate import (  # noqa: E402
    Band, func_table, load_tmap, decode_slots, evaluate)


def masked_eq(our_body, band_body, offs):
    if band_body is None or len(band_body) != len(our_body):
        return False
    bb = bytearray(band_body)
    for off in offs:
        for b in range(4):
            if off + b < len(bb):
                bb[off + b] = 0
    return bytes(bb) == our_body


class Index:
    """Every compiled function in the worktree, with its outgoing relocations."""

    def __init__(self, wt, results):
        self.fn = {}                        # (tu, name) -> func_table entry
        self.tus = defaultdict(list)        # name -> [tu]
        n_obj = 0
        for tu in sorted(results):
            if not isinstance(results[tu], list):
                continue
            obj = os.path.join(wt, 'build/45410914/src', tu + '.obj')
            if not os.path.exists(obj):
                continue
            n_obj += 1
            for cname, f in func_table(obj).items():
                self.fn[(tu, cname)] = f
                self.tus[cname].append(tu)
        self.n_obj = n_obj


def build_claims(band, idx, anchors):
    """claims[v] -> {callee name}; sites[(name, v)] -> [(caller, off, caller_va)]"""
    claims = defaultdict(set)
    sites = defaultdict(list)
    done = set()
    for (tu, cname), f in idx.fn.items():
        va_c = anchors.get(cname)
        if va_c is None or cname in done:
            continue
        done.add(cname)
        slots = decode_slots(band, va_c, f['size'], f['offs'], f['words'])
        if not slots:
            continue
        for off, (tname, ty, tok) in f['refs'].items():
            if tname == cname:
                continue                      # self / recursion
            rec = slots.get(off)
            if rec is None or rec['kind'] not in ('call', 'addr'):
                continue
            v = rec['value']
            claims[v].add(tname)
            sites[(tname, v)].append((cname, off, va_c))
    return claims, sites


def resolve(name, cands, byname, claims, sites, famsize=None, maxfam=0,
            containment=True):
    """-> (verdict, va, evidence)

    `containment=False` drops guard #4 (the derived VA need not be one of the
    callee's own reloc-masked byte-identical candidates).  That is what a lane
    which only needs the *name* -- e.g. EH-funclet parentage cascade -- would
    want, since such a parent's body does not match retail at all and therefore
    has no hit set to be contained in.  It is also the guard that makes the
    derivation safe; see --validate-nocontain for the measured cost.
    """
    if maxfam and famsize is not None and famsize.get(name, 1) > maxfam:
        return 'BIG-FAMILY', None, []
    vs = byname.get(name)
    if not vs:
        return 'NO-ANCHOR', None, []
    if len(vs) > 1:
        return 'DISAGREE', None, sorted('0x%08x' % v for v in vs)
    v = next(iter(vs))
    ev = sites[(name, v)]
    if claims[v] != {name}:
        return 'SHARED-VA', v, sorted(claims[v] - {name})[:3]
    if containment and v not in cands:
        return 'NOT-IN-HITS', v, ev
    return 'RESOLVED', v, ev


def validate_nocontain(a, band, tmap, name2va, idx, hitsets, anchors,
                       claims, sites, byname, famsize, notbyte):
    """Held-out precision of the derivation WITHOUT guard #4 (containment).

    Ground truth population: every name that the map homes at exactly one VA
    *and* whose compiled body is NOT reloc-masked byte-identical there.  That is
    precisely the population guard #4 can never admit -- and precisely the
    population a funclet-cascade lane wants to name, since a funclet flips on
    its parent's frame + savegprlr alone, not on the parent's body.

    No leakage: the map entry for F is never used to derive F's VA (claims[v]
    is built only from F's *callers*, and F itself is not an anchor because it
    is not byte-identical).  Guard #5 ("v unclaimed") cannot be applied here --
    F's true VA is by construction in the map -- so it is emulated: a pick is
    'production-legal' iff tmap.get(v) is None or == F.
    """
    val = defaultdict(int)
    lines = []
    for name, (tu, v0) in sorted(notbyte.items()):
        val['POP'] += 1
        cands = set()               # guard #4 disabled -> unused
        verdict, v, ev = resolve(name, cands, byname, claims, sites,
                                 famsize, a.max_family, containment=False)
        if verdict == 'RESOLVED' and len({e[0] for e in ev}) < a.min_anchors:
            verdict = 'FEW-ANCHORS'
        if verdict != 'RESOLVED':
            val['refuse/' + verdict] += 1
            continue
        ok = (v == v0)
        tag = 'HIT' if ok else 'MISS'
        val[tag] += 1
        # GOLD subpopulation: the incumbent map entry is itself corroborated by
        # map-free content (a string / float constant F references really is at
        # v0).  Scoring against a contaminated map is a lower bound; scoring
        # against the corroborated slice removes that contamination.
        gold = content_check(band, tmap, name2va, idx, tu, name, v0) == 'AGREE'
        if gold:
            val['GOLD/%s' % tag] += 1
            val['GOLDNH%s/%s' % ('0' if nh == 0 else 'N', tag)] += 1
            if fs == 1:
                val['GOLDFAM1/%s' % tag] += 1
        na = len({e[0] for e in ev})
        fs = famsize.get(name, 1)
        fb = ('1' if fs == 1 else '2-4' if fs <= 4 else
              '5-16' if fs <= 16 else '17+')
        ab = '1' if na == 1 else '2' if na == 2 else '3+'
        nh = len(hitsets.get((tu, name), ()))
        val['FAM%s/%s' % (fb, tag)] += 1
        val['ANC%s/%s' % (ab, tag)] += 1
        val['HITS%s/%s' % ('0' if nh == 0 else 'N', tag)] += 1
        legal = tmap.get(v) in (None, name)
        val['LEGAL/%s' % tag] += 1 if legal else 0
        val['ILLEGAL/%s' % tag] += 0 if legal else 1
        if legal:
            val['LEGALFAM%s/%s' % (fb, tag)] += 1
            val['LEGALANC%s/%s' % (ab, tag)] += 1
            if fs == 1:
                val['L1ANC%s/%s' % (ab, tag)] += 1
        if not ok:
            # is the DERIVATION right and the incumbent map entry wrong?  Our
            # body is by construction NOT byte-identical at the map's VA; if it
            # IS byte-identical at the derived VA, the map is the wrong one.
            f = idx.fn.get((tu, name))
            bi = bool(f) and masked_eq(f['body'],
                                       band.text_bytes(v, f['size']), f['offs'])
            val['MISS-derived-byte-identical' if bi
                else 'MISS-derived-not-byte-identical'] += 1
            # independent arbiter: map-free content evidence (strings / float
            # constants F itself references) at each of the two candidate homes
            c_t = content_check(band, tmap, name2va, idx, tu, name, v0)
            c_g = content_check(band, tmap, name2va, idx, tu, name, v)
            val['ARB-%s' % ('map' if (c_t == 'AGREE' and c_g != 'AGREE')
                            else 'derivation' if (c_g == 'AGREE' and c_t != 'AGREE')
                            else 'neither/both')] += 1
            lines.append('MISS %-58s %-34s truth=0x%08x got=0x%08x '
                         'fam=%d anc=%d nhits=%d legal=%d bi@got=%d '
                         'ct=%s cg=%s occupied=%s'
                         % (name[:58], tu[:34], v0, v, fs, na, nh, legal, bi,
                            c_t, c_g, tmap.get(v, '-')))
        else:
            lines.append('HIT  %-58s 0x%08x fam=%d anc=%d' % (name[:58], v, fs, na))
    open(a.report, 'w').write('\n'.join(lines) + '\n')

    def pct(h, m, label):
        if h + m:
            print('   %-26s %5d/%-5d = %6.2f%%' % (label, h, h + m,
                                                   100.0 * h / (h + m)))
    print('NO-CONTAINMENT held-out validation (population = %d singly-mapped '
          'names whose body is NOT byte-identical at their mapped VA)'
          % val['POP'])
    print('  refusals:', {k[7:]: v for k, v in sorted(val.items())
                          if k.startswith('refuse/')})
    pct(val['HIT'], val['MISS'], 'OVERALL')
    for c in ('1', '2-4', '5-16', '17+'):
        pct(val['FAM%s/HIT' % c], val['FAM%s/MISS' % c], 'sibling-family %s' % c)
    for c in ('1', '2', '3+'):
        pct(val['ANC%s/HIT' % c], val['ANC%s/MISS' % c], 'anchors %s' % c)
    for c in ('0', 'N'):
        pct(val['HITS%s/HIT' % c], val['HITS%s/MISS' % c],
            'own hit set %s' % ('empty (NOMATCH)' if c == '0' else 'non-empty'))
    pct(val['LEGAL/HIT'], val['LEGAL/MISS'], 'guard#5-legal only')
    for c in ('1', '2-4', '5-16', '17+'):
        pct(val['LEGALFAM%s/HIT' % c], val['LEGALFAM%s/MISS' % c],
            'legal & family %s' % c)
    for c in ('1', '2', '3+'):
        pct(val['LEGALANC%s/HIT' % c], val['LEGALANC%s/MISS' % c],
            'legal & anchors %s' % c)
    for c in ('1', '2', '3+'):
        pct(val['L1ANC%s/HIT' % c], val['L1ANC%s/MISS' % c],
            'legal & fam1 & anchors %s' % c)
    pct(val['GOLD/HIT'], val['GOLD/MISS'], 'content-corroborated map')
    pct(val['GOLDNH0/HIT'], val['GOLDNH0/MISS'], '  ..& own hit set empty')
    pct(val['GOLDNHN/HIT'], val['GOLDNHN/MISS'], '  ..& own hit set non-empty')
    pct(val['GOLDFAM1/HIT'], val['GOLDFAM1/MISS'], '  ..& family 1')
    print('  of the %d misses, %d are byte-identical at the DERIVED VA '
          '(i.e. the incumbent map entry is the wrong one), %d are not'
          % (val['MISS'], val['MISS-derived-byte-identical'],
             val['MISS-derived-not-byte-identical']))
    if a.stats_out:
        json.dump(dict(val), open(a.stats_out, 'w'), indent=1)


def content_check(band, tmap, name2va, idx, tu, name, v):
    """Map-free content verdict for putting `name` at retail VA `v`.

    'AGREE'    - a string / float constant the callee references is really there
    'CONFLICT' - it is demonstrably NOT there -> the derivation is wrong
    'NONE'     - the callee references nothing checkable (the NO-EVIDENCE pool)
    """
    f = idx.fn.get((tu, name))
    if f is None:
        return 'NONE'
    r = evaluate(band, tmap, name2va, f, [v], use_sym=False, truth=v)
    if not isinstance(r, str):
        r = r[0]
    return {'TRUTH-AGREE': 'AGREE', 'TRUTH-CONFLICT': 'CONFLICT'}.get(r, 'NONE')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--results', required=True)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--band')
    ap.add_argument('--tmap')
    ap.add_argument('--out', default='/home/free/tmp/laneK/caller_prop.json')
    ap.add_argument('--report', default='/home/free/tmp/laneK/caller_report.txt')
    ap.add_argument('--classes', default='MULTI,UNIQUE-ICF')
    ap.add_argument('--min-anchors', type=int, default=1)
    ap.add_argument('--strict-anchors', action='store_true',
                    help='anchor must itself be unambiguously homed (its own '
                         'byte-identity hit set has exactly one element)')
    ap.add_argument('--content-guard', action='store_true')
    ap.add_argument('--max-family', type=int, default=16,
                    help='refuse when more than N of OUR OWN compiled functions '
                         'share the callee\'s reloc-masked bytes.  Large sibling '
                         'families are template/STL instantiation swarms that '
                         'retail partly ICF-folds; measured precision drops from '
                         '99.1%% (family <=16) to 96.7%% (family 17+).  0 = off.')
    ap.add_argument('--iterate', type=int, default=1,
                    help='fixed-point rounds: resolutions from round N become '
                         'anchors in round N+1')
    ap.add_argument('--validate', action='store_true')
    ap.add_argument('--validate-nocontain', action='store_true',
                    help='held-out precision of the derivation with guard #4 '
                         '(hit-set containment) DISABLED, measured over the '
                         'singly-mapped names whose body is NOT byte-identical '
                         'at their mapped VA -- the population a funclet-'
                         'cascade lane wants and that guard #4 excludes.')
    ap.add_argument('--no-containment', action='store_true',
                    help='production: drop guard #4 and admit hitless '
                         '(NOMATCH) targets.  Only ever use with a variant '
                         'whose --validate-nocontain precision you measured.')
    ap.add_argument('--stats-out')
    a = ap.parse_args()

    wt = a.worktree
    band = Band(a.band or os.path.join(wt, 'orig/45410914/band.exe'))
    tmap, name2va = load_tmap(a.tmap or os.path.join(wt, 'scripts/target_symbol_map.json'))
    res = json.load(open(a.results))
    want_cls = set(a.classes.split(','))

    # per-name byte-identity hit sets from the homing scan (used for candidate
    # sets and, with --strict-anchors, for anchor quality)
    hitsets = {}
    for tu, recs in res.items():
        if not isinstance(recs, list):
            continue
        for r in recs:
            h = {int(x, 16) for x in (r.get('hits') or [])}
            if h:
                hitsets.setdefault((tu, r['name']), h)

    targets = {}
    for tu, recs in sorted(res.items()):
        if not isinstance(recs, list):
            continue
        for r in recs:
            if not r.get('hits') and not a.no_containment:
                continue
            if a.validate:
                if len(r['hits'] or ()) < 2:
                    continue
            elif a.no_containment:
                # guard #4 is off, so a hitless (NOMATCH) record is admissible:
                # its body simply does not match retail anywhere, which is the
                # normal state of an unnamed parent.
                if r.get('cls') not in want_cls | {'NOMATCH'}:
                    continue
            elif r.get('cls') not in want_cls:
                continue
            targets.setdefault(r['name'], (tu, r))
    print('targets: %d' % len(targets), file=sys.stderr)

    idx = Index(wt, res)
    print('index: %d objs, %d compiled functions' % (idx.n_obj, len(idx.fn)),
          file=sys.stderr)

    # ---------------------------------------------------------------- anchors
    anchors = {}
    notbyte = {}        # name -> (tu, mapped_va): singly-mapped but our body is
                        # NOT byte-identical there.  This is the held-out ground
                        # truth for --validate-nocontain: it mirrors exactly the
                        # population a funclet-cascade lane wants to name.
    astat = defaultdict(int)
    for (tu, cname), f in idx.fn.items():
        if cname in anchors:
            continue
        vas = name2va.get(cname)
        if not vas:
            astat['unmapped'] += 1
            continue
        if len(vas) != 1:
            astat['multi-mapped'] += 1
            continue
        va = next(iter(vas))
        if a.strict_anchors:
            hs = hitsets.get((tu, cname))
            if hs is None or hs != {va}:
                astat['ambiguous-home'] += 1
                continue
        if not masked_eq(f['body'], band.text_bytes(va, f['size']), f['offs']):
            astat['not-byte-identical'] += 1
            notbyte.setdefault(cname, (tu, va))
            continue
        anchors[cname] = va
        notbyte.pop(cname, None)     # byte-identical in some other TU's COMDAT
        astat['ANCHOR'] += 1
    print('anchors:', dict(astat), file=sys.stderr)

    # sibling families: how many DIFFERENT names of ours compile to the same
    # reloc-masked bytes.  A name in a family >1 is, by construction, one our
    # own object files cannot tell apart -- retail may also have ICF-folded it.
    fam = defaultdict(set)
    for (tu, cname), f in idx.fn.items():
        fam[f['body']].add(cname)
    famsize = {}
    for (tu, cname), f in idx.fn.items():
        famsize[cname] = len(fam[f['body']])
    del fam

    claims, sites = build_claims(band, idx, anchors)
    byname = defaultdict(set)
    for v, names in claims.items():
        for n in names:
            byname[n].add(v)
    print('claims: %d distinct retail VAs claimed by %d callee names'
          % (len(claims), len(byname)), file=sys.stderr)

    if a.validate_nocontain:
        validate_nocontain(a, band, tmap, name2va, idx, hitsets, anchors,
                           claims, sites, byname, famsize, notbyte)
        return

    stats = defaultdict(int)
    lines = []
    out = defaultdict(list)
    picked = defaultdict(list)
    val = defaultdict(int)
    valpicks = defaultdict(list)
    identified_nonbyte = []

    done_names = set()
    taken = set(tmap)
    rounds = 1 if a.validate else max(1, a.iterate)
    for rnd in range(rounds):
      if rnd:
        # fixed point: everything resolved so far is now itself an anchor.  Its
        # home was proven by hit-set containment, so it is byte-identical there
        # by construction -- no re-verification needed.
        n0 = len(anchors)
        for v, cl in picked.items():
            for tu_, nm in cl:
                if a.no_containment:
                    # with guard #4 off a pick is NOT byte-identical by
                    # construction, so promoting it to anchor would decode
                    # retail bytes that are not ours and manufacture bogus
                    # claims.  Promote only the ones that verify.
                    f = idx.fn.get((tu_, nm))
                    if f is None or not masked_eq(
                            f['body'], band.text_bytes(v, f['size']), f['offs']):
                        continue
                anchors.setdefault(nm, v)
        if len(anchors) == n0:
            break
        claims, sites = build_claims(band, idx, anchors)
        byname = defaultdict(set)
        for v, names in claims.items():
            for n in names:
                byname[n].add(v)
        print('round %d: %d anchors, %d claimed VAs'
              % (rnd, len(anchors), len(claims)), file=sys.stderr)
      for name, (tu, r) in sorted(targets.items()):
        if name in done_names:
            continue
        hits = [int(h, 16) for h in (r.get('hits') or ())]
        if a.validate:
            truth = [v for v in hits if tmap.get(v) == name]
            if len(truth) != 1:
                continue
            truth = truth[0]
            cands = {v for v in hits if v not in tmap} | {truth}
            verdict, v, ev = resolve(name, cands, byname, claims, sites,
                                     famsize, a.max_family)
            if verdict == 'RESOLVED' and len({e[0] for e in ev}) < a.min_anchors:
                verdict = 'FEW-ANCHORS'
            cc = 'NONE'
            if verdict == 'RESOLVED':
                cc = content_check(band, tmap, name2va, idx, tu, name, v)
                if a.content_guard and cc == 'CONFLICT':
                    verdict = 'CONTENT-CONFLICT'
            val[verdict] += 1
            if verdict == 'RESOLVED':
                valpicks[v].append((name, truth, ev, tu, cc))
            continue

        if any(tmap.get(v) == name for v in hits) or (
                a.no_containment and name in name2va):
            stats['ALREADY-HOMED'] += 1
            done_names.add(name)
            continue
        cands = {v for v in hits if v not in taken}
        verdict, v, ev = resolve(name, cands, byname, claims, sites,
                                     famsize, a.max_family,
                                     containment=not a.no_containment)
        if verdict == 'RESOLVED':
            if len({e[0] for e in ev}) < a.min_anchors:
                verdict = 'FEW-ANCHORS'
            elif v in taken:
                verdict = 'TAKEN'
            elif a.content_guard and content_check(
                    band, tmap, name2va, idx, tu, name, v) == 'CONFLICT':
                verdict = 'CONTENT-CONFLICT'
        stats[verdict] += 1
        if verdict == 'RESOLVED':
            picked[v].append((tu, name))
            done_names.add(name)
            taken.add(v)
            nr = dict(r)
            nr['cls'] = 'UNIQUE'
            nr['va'] = '0x%08x' % v
            nr['n_unmapped'] = 1
            nr['disambig'] = 'CALLER-SIDE'
            nr['evidence'] = [dict(caller=c, off=o, caller_va='0x%08x' % cv)
                              for c, o, cv in ev[:4]]
            out[tu].append(nr)
            lines.append('RESOLVED %-56s sz=%4d %s -> 0x%08x  via %s'
                         % (name[:56], r['size'], tu, v,
                            ', '.join('%s+0x%x' % (c, o) for c, o, _ in ev[:3])))
        elif verdict == 'NOT-IN-HITS' and v is not None:
            identified_nonbyte.append(dict(name=name, tu=tu, va='0x%08x' % v,
                                           size=r['size'], cls=r.get('cls')))

    if a.validate:
        for v, cl in sorted(valpicks.items()):
            if len({c[0] for c in cl}) > 1:
                val['DROP-CONTESTED'] += len(cl)
                val['RESOLVED'] -= len(cl)
                continue
            for name, truth, ev, tu, cc in cl:
                ok = (v == truth)
                val['HIT' if ok else 'MISS'] += 1
                val['A%d/%s' % (min(len({e[0] for e in ev}), 3),
                                'HIT' if ok else 'MISS')] += 1
                val['C-%s/%s' % (cc, 'HIT' if ok else 'MISS')] += 1
                nh = len(hitsets.get((tu, name), ()))
                fs = famsize.get(name, 1)
                afs = max(famsize.get(c, 1) for c, _, _ in ev)
                fb = ('1' if fs == 1 else '2-4' if fs <= 4 else
                      '5-16' if fs <= 16 else '17+')
                val['FAM%s/%s' % (fb, 'HIT' if ok else 'MISS')] += 1
                val['AFAM%s/%s' % ('1' if afs == 1 else 'N',
                                   'HIT' if ok else 'MISS')] += 1
                val['NH%s/%s' % ('2' if nh <= 2 else ('5' if nh <= 5 else 'M'),
                                 'HIT' if ok else 'MISS')] += 1
                val['GOLD/%s' % ('HIT' if ok else 'MISS')] += (
                    1 if (fs == 1 and afs == 1) else 0)
                if not ok:
                    mc = content_check(band, tmap, name2va, idx, tu, name, truth)
                    val['MISS-mapcontent-%s' % mc] += 1
                    lines.append('VALMISS[got=%s map=%s fam=%d afam=%d nh=%d] '
                                 '%-52s %s truth=0x%08x got=0x%08x ev=%s'
                                 % (cc, mc, fs, afs, nh, name[:52], tu, truth, v,
                                    [(c, o) for c, o, _ in ev[:3]]))
        open(a.report, 'w').write('\n'.join(lines) + '\n')
        print('held-out validation:', dict(sorted(val.items(), key=lambda kv: -kv[1])))
        h, m = val.get('HIT', 0), val.get('MISS', 0)
        if h + m:
            print('  CALLER-SIDE precision %d/%d = %.3f%%'
                  % (h, h + m, 100.0 * h / (h + m)))
        for k in (1, 2, 3):
            hh, mm = val.get('A%d/HIT' % k, 0), val.get('A%d/MISS' % k, 0)
            if hh + mm:
                print('   %d%s anchor(s): %d/%d = %.2f%%'
                      % (k, '+' if k == 3 else '', hh, hh + mm,
                         100.0 * hh / (hh + mm)))
        for c in ('1', '2-4', '5-16', '17+'):
            hh, mm = val.get('FAM%s/HIT' % c, 0), val.get('FAM%s/MISS' % c, 0)
            if hh + mm:
                print('   sibling-family %-5s: %d/%d = %.2f%%'
                      % (c, hh, hh + mm, 100.0 * hh / (hh + mm)))
        for c in ('AGREE', 'NONE'):
            hh, mm = val.get('C-%s/HIT' % c, 0), val.get('C-%s/MISS' % c, 0)
            if hh + mm:
                print('   content=%-5s : %d/%d = %.2f%%'
                      % (c, hh, hh + mm, 100.0 * hh / (hh + mm)))
        if a.stats_out:
            json.dump(dict(val), open(a.stats_out, 'w'), indent=1)
        return

    dropped = 0
    for v, cl in picked.items():
        if len({n for _, n in cl}) > 1:
            for tu, n in cl:
                out[tu] = [x for x in out[tu] if x['name'] != n]
                dropped += 1
            stats['DROP-CONTESTED'] += 1
    out = {k: v for k, v in out.items() if v}

    json.dump(out, open(a.out, 'w'), indent=1)
    open(a.report, 'w').write('\n'.join(lines) + '\n')
    if identified_nonbyte:
        p = a.out.replace('.json', '_nonbyte.json')
        json.dump(identified_nonbyte, open(p, 'w'), indent=1)
        print('identified-but-not-byte-identical (near-miss worklist): %d -> %s'
              % (len(identified_nonbyte), p))
    print('verdicts:', dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print('proposed %d resolutions across %d units (contested dropped %d)'
          % (sum(len(v) for v in out.values()), len(out), dropped))
    print('->', a.out)


if __name__ == '__main__':
    main()
