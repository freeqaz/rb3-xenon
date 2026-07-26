#!/usr/bin/env python3
"""Global family closure: bipartite elimination over reloc-masked sibling families.

Background
----------
`homing_scan.py` homes one of our compiled functions on a retail VA by
reloc-masked byte identity.  When several retail VAs are byte-identical it emits
`MULTI` and refuses.  `multi_content_disambiguate.py` (callee content) and
`caller_side_invert.py` (caller-side call-graph inversion) crack part of that
residue -- but both decide **one function at a time**, and both collapse on the
big template/STL instantiation swarms: caller-side inversion measured 99.09 %
held-out at sibling-family <= 16 and only 96.65 % at family 17+, so it refused
3,843 records as BIG-FAMILY.

This tool looks at the *family* rather than the function.

    A sibling family is the set of OUR names F1..Fn whose reloc-masked bodies are
    byte-identical, together with the set of retail VAs V1..Vm that are
    byte-identical to them (the shared hit set).

Two closure rules are implemented over that bipartite graph.

RULE 1 -- naked single (counting / all-different)
    A name whose domain has collapsed to one VA is forced.  This is the classic
    elimination argument and it needs the assignment to be a *matching*.

    ** It is unsound on this binary and is OFF by default. **  Retail ICF-folds
    family members onto shared addresses, so the truth is not a matching: the
    census below shows every large family is `m < n` (e.g. 982 names / 577 hits,
    130 names / 4 hits).  Where retail folded k of our names onto one VA there is
    no correct 1:1 answer to find, only a choice of which name to record.  Enable
    with --rule1 only for measurement.

RULE 2 -- span-local uniqueness (the rule this tool exists for)
    Everything inside unit U's pinned `.text` range in `splits.txt` comes from
    U's linked object contribution.  So if

      * retail VA `v` lies in U's pinned span, and
      * `v` is byte-identical to the family body, and
      * exactly ONE of our family members is emitted by U (counting every COMDAT
        in U's obj, funclets included), and that member `F` is unassigned,

    then the function at `v` is a family-bodied COMDAT contributed by U, and `F`
    is the only family member U can contribute -- so `v` is `F`.

    This is a *derivation from the linker's own partitioning*, not a count, and
    it is **robust to ICF folding**: if retail folded U's copy of `F` away into
    some other object's copy, U's span simply contains no family-bodied function
    and `F` is refused (empty domain) rather than mis-assigned.  Conversely if
    U's copy is the survivor that others folded onto, `v` really is `F` and the
    other names lose their own spans and are refused.  Folding costs reach, never
    correctness -- which is exactly why this rule survives the `m < n` regime
    that kills Rule 1.

Both rules iterate to a fixed point: an assignment consumes its VA for every
other family member and removes its name from every VA's claimant set, which can
force further singles.  **Only forced assignments are emitted.**  No argmax, no
guessing to complete a matching; every family that does not close is refused and
counted.

Soundness caveats, all handled explicitly and reported
------------------------------------------------------
1. *Folding breaks the bijection.*  Regime census (`m == n` / `m > n` / `m < n`)
   is reported per family.  Rule 1 is gated on `m >= n` even when enabled.
2. *Closure needs the family to be COMPLETE.*  A family member with no homing
   record (below the scan's 32-byte floor, or funclet-filtered) means our view of
   the family is partial; families with such members are marked INCOMPLETE and
   Rule 1 is refused for them.  Rule 2's unit-local count deliberately counts
   *all* COMDATs in the obj, records or not, so an unrecorded sibling in the same
   unit blocks the rule rather than corrupting it.
3. *Names in no wired TU / VAs in no pinned span.*  A VA in no pinned span has no
   owner and is feasible for everybody (it cannot exclude anyone); Rule 2 never
   fires on it.  `--pinned-only` additionally removes unpinned VAs from domains
   (an assumption, measured separately -- it is NOT sound and is off by default).

Held-out measurement
--------------------
`--validate` scores the resolver against the incumbent map, with a
**whole-family hold-out** (`--holdout family`, default): every map entry naming a
member of the family, and every map entry sitting on a VA of the family's hit
set, is hidden before the family is resolved.  The family is presented to the
resolver exactly as an entirely unsolved family would be, so no ground-truth
assignment can leak into the elimination -- which is the circularity that a
leave-one-out protocol suffers from (in a fully-mapped family, hiding one name
leaves exactly one free VA and elimination "recovers" it for free, measuring
nothing).  `--holdout name` reproduces that optimistic protocol for comparison;
it is reported separately and labelled.

Contested drops (a VA picked by more than one distinct name) are applied before
HIT/MISS scoring, as in production.  Precision is bucketed by famsize.

Usage
-----
    family_closure.py --results merged.json --worktree WT --validate
    family_closure.py --results merged.json --worktree WT --out prop.json

Emits homing_scan result format (records re-classified `UNIQUE`) so
`span_predictor.py` / `homing_gen4.py` / `homing_apply4.py` consume it unchanged.
"""
import argparse
import bisect
import hashlib
import json
import os
import re
import sys
from collections import defaultdict, Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from multi_content_disambiguate import (  # noqa: E402
    Band, func_table, load_tmap, evaluate)
from span_predictor import parse_splits, Coverage  # noqa: E402

FUNCLET_TOKENS = ('__unwind', '__ehhandler', '__catch', '__tls', '__GSHandler')


def is_funclet(name):
    return any(t in name for t in FUNCLET_TOKENS)


class ContentGuard:
    """MAP-FREE veto: does the callee itself reference something that is
    demonstrably NOT at the derived VA?

    The elimination argument is purely positional (spans, unit membership); it
    never looks at what the function says about itself.  Strings and `__real@`
    float constants our relocations point at are independent evidence and need
    no symbol map, so they can only ever *refuse* a pick -- never create one.
    A wave-1 measurement showed this is not academic: one forced pick
    (`?StaticClassName@StarDisplay@@`) landed on a VA whose referenced string
    contradicts it, and the trust audit flagged it as a new mispair.
    """

    def __init__(self, wt, tmap, name2va):
        self.wt = wt
        self.band = Band(os.path.join(wt, 'orig/45410914/band.exe'))
        self.tmap = tmap
        self.name2va = name2va
        self._ft = {}

    def ft(self, tu):
        if tu not in self._ft:
            p = os.path.join(self.wt, 'build/45410914/src', tu + '.obj')
            self._ft[tu] = func_table(p) if os.path.exists(p) else {}
            if len(self._ft) > 64:                     # bounded cache
                for k in list(self._ft)[:32]:
                    if k != tu:
                        self._ft.pop(k)
        return self._ft[tu]

    def conflicts(self, tu, name, va):
        f = self.ft(tu).get(name)
        if f is None:
            return False
        r = evaluate(self.band, self.tmap, self.name2va, f, [va],
                     use_sym=False, truth=va)
        if not isinstance(r, str):
            r = r[0]
        return r == 'TRUTH-CONFLICT'


# --------------------------------------------------------------------- index
class Index:
    """Every compiled COMDAT in the worktree, grouped into reloc-masked families."""

    def __init__(self, wt, tus, verbose=True):
        self.emitters = defaultdict(set)     # name -> {tu}
        self.namekey = {}                    # name -> (size, digest) or 'AMBIG'
        self.unit_names = defaultdict(set)   # (tu, key) -> {name}
        self.fam = defaultdict(set)          # key -> {name}
        self.n_obj = 0
        self.n_fn = 0
        for i, tu in enumerate(sorted(tus)):
            obj = os.path.join(wt, 'build/45410914/src', tu + '.obj')
            if not os.path.exists(obj):
                continue
            self.n_obj += 1
            for name, f in func_table(obj).items():
                key = (f['size'], hashlib.blake2b(f['body'], digest_size=16).digest())
                self.n_fn += 1
                self.emitters[name].add(tu)
                prev = self.namekey.get(name)
                if prev is None:
                    self.namekey[name] = key
                elif prev != key and prev != 'AMBIG':
                    # same symbol compiled to different bytes in two TUs: our own
                    # objs disagree, so family membership is undefined for it.
                    self.namekey[name] = 'AMBIG'
                self.fam[key].add(name)
                self.unit_names[(tu, key)].add(name)
            if verbose and (i % 200) == 0:
                print('  ... %d/%d objs' % (i, len(tus)), file=sys.stderr)
        # drop AMBIG names from families entirely
        self.ambig = {n for n, k in self.namekey.items() if k == 'AMBIG'}
        for n in self.ambig:
            for key, names in list(self.fam.items()):
                names.discard(n)


# ------------------------------------------------------------ splits mapping
class Spans:
    """splits.txt header <-> unit key resolution, plus VA -> owning unit set."""

    def __init__(self, wt, tus):
        self.units = parse_splits(os.path.join(wt, 'config/45410914/splits.txt'))
        self.cov = Coverage(self.units)
        # a splits header may be a bare basename or a partial path; a unit key
        # `tu` matches header h when `tu + '.cpp'` == h or endswith('/' + h)
        self.header2tu = defaultdict(set)
        headers = list(self.units)
        for tu in tus:
            want = tu + '.cpp'
            for h in headers:
                if want == h or want.endswith('/' + h):
                    self.header2tu[h].add(tu)

    def owner_tus(self, va):
        h = self.cov.owner(va)
        if h is None:
            return None
        return self.header2tu.get(h, frozenset())


# ------------------------------------------------------------- the closure
def close_family(names, target_names, hits, idx, spans, va2name, mapped_names,
                 opts, regime):
    """Resolve one family.

    names        : every one of our COMDAT names with this body (funclets incl.)
    target_names : the subset that carries a homing record (resolution targets)
    hits         : sorted list of retail VAs byte-identical to the body
    va2name      : effective map VA -> name (already held out, if validating)
    mapped_names : effective set of names that already have a home in the map

    Returns (assign {name: va}, refusal Counter).
    """
    ref = Counter()
    assign = {}
    consumed = {v for v in hits if v in va2name}

    todo = [n for n in target_names if n not in mapped_names]
    if not todo:
        ref['NOTHING-TO-DO'] += 1
        return assign, ref

    # ---- domains -------------------------------------------------------
    # feasible(name) = byte-identical, unconsumed VAs whose owning unit emits
    # the name (or that sit in no pinned span at all, unless --pinned-only).
    dom = {}
    for n in todo:
        emit = idx.emitters.get(n, ())
        d = set()
        for v in hits:
            if v in consumed:
                continue
            ots = spans.owner_tus(v)
            if ots is None:
                if not opts.pinned_only:
                    d.add(v)
                continue
            if ots & emit:
                d.add(v)
        dom[n] = d

    # ---- unit-local family membership ---------------------------------
    # how many of OUR family names each unit emits (all COMDATs, funclets
    # included -- an unrecorded sibling in the same unit must block Rule 2).
    def local_members(tu):
        return idx.unit_names.get((tu, opts._key), set())

    changed = True
    while changed:
        changed = False

        # ---------------- RULE 2: span-local uniqueness -----------------
        # Grouped by the *owning splits header*, because the derivation is a
        # statement about one linked object contribution, not about one address:
        #
        #   hdr's span contributes exactly ONE still-free family-bodied retail
        #   function, and our objs for hdr emit exactly ONE still-free family
        #   member  =>  they are the same function.
        #
        # Requiring BOTH sides to be 1 is the operational completeness test.
        # If the span holds two family-bodied functions but we only know one
        # member for that unit, our view of the unit is demonstrably partial and
        # picking either address would be a coin flip -- refuse instead.
        if not opts.no_rule2:
            byhdr = defaultdict(list)
            for v in hits:
                if v in consumed:
                    continue
                h = spans.cov.owner(v)
                if h is not None:
                    byhdr[h].append(v)
            for h, vs in byhdr.items():
                ots = spans.header2tu.get(h) or set()
                local = set()
                for tu in ots:
                    local |= local_members(tu)
                if not local:
                    continue
                free = {n for n in local
                        if n not in mapped_names and n not in assign}
                if len(free) != 1:
                    ref['R2-UNIT-AMBIGUOUS' if len(free) > 1 else 'R2-UNIT-EMPTY'] += 1
                    continue
                if len(vs) != 1 and not opts.no_local_bijection:
                    # retail's contribution holds more family-bodied functions
                    # than we can name for this unit -> incomplete, refuse
                    ref['R2-SPAN-INCOMPLETE'] += 1
                    continue
                v = vs[0]
                n = next(iter(free))
                if n not in dom:
                    ref['R2-NOT-A-TARGET'] += 1     # e.g. funclet / sub-32B
                    continue
                if v not in dom[n]:
                    ref['R2-DOMAIN-CONFLICT'] += 1
                    continue
                assign[n] = v
                consumed.add(v)
                dom.pop(n)
                for d in dom.values():
                    d.discard(v)
                ref['R2-FORCED'] += 1
                changed = True

        # ---------------- RULE 1: naked single --------------------------
        if opts.rule1:
            if opts.rule1_force or (regime in ('m==n', 'm>n')
                                    and not opts._incomplete):
                for n, d in list(dom.items()):
                    if len(d) == 1:
                        v = next(iter(d))
                        assign[n] = v
                        consumed.add(v)
                        dom.pop(n)
                        for dd in dom.values():
                            dd.discard(v)
                        ref['R1-FORCED'] += 1
                        changed = True
            else:
                ref['R1-BLOCKED-REGIME'] += 1

    for n, d in dom.items():
        ref['OPEN-%d' % (0 if not d else (1 if len(d) == 1 else 2))] += 1
    return assign, ref


# --------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--results', required=True)
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--band')
    ap.add_argument('--tmap')
    ap.add_argument('--out', default='/home/free/tmp/laneM/family_prop.json')
    ap.add_argument('--report', default='/home/free/tmp/laneM/family_report.txt')
    ap.add_argument('--classes', default='MULTI,UNIQUE-ICF')
    ap.add_argument('--rule1', action='store_true',
                    help='enable counting-based naked-single elimination '
                         '(UNSOUND under ICF folding; measurement only)')
    ap.add_argument('--no-content-guard', action='store_true',
                    help='disable the map-free content veto on forced picks')
    ap.add_argument('--rule1-force', action='store_true',
                    help='run Rule 1 even in the m<n (ICF-folded) regime -- '
                         'measurement only, the all-different premise is false there')
    ap.add_argument('--no-rule2', action='store_true')
    ap.add_argument('--no-local-bijection', action='store_true',
                    help='drop the completeness guard that demands the owning '
                         'span hold exactly ONE still-free family-bodied VA')
    ap.add_argument('--pinned-only', action='store_true',
                    help='assume the home is inside SOME pinned span, i.e. drop '
                         'unpinned VAs from domains (an assumption, not a proof)')
    ap.add_argument('--min-famsize', type=int, default=0)
    ap.add_argument('--max-emitters', type=int, default=0,
                    help='only EMIT a forced assignment when the name is emitted '
                         'by at most N of our objs (0 = off).  A single-emitter '
                         'COMDAT gave the linker no choice of contributing object, '
                         'so the span argument is exact; a COMDAT emitted by many '
                         'TUs could have been linked from any of them, and our '
                         'per-TU instantiation set diverges from retail\'s.  '
                         'Measured held-out at famsize 17+: 99.47%% at N=1 vs '
                         '73.91%% at 21+ emitters.  The elimination itself still '
                         'uses every family member as a competitor.')
    ap.add_argument('--validate', action='store_true')
    ap.add_argument('--holdout', choices=('family', 'name'), default='family')
    ap.add_argument('--census', action='store_true',
                    help='print the family-regime census and exit')
    ap.add_argument('--stats-out')
    ap.add_argument('--cache', help='pickle path for the obj index (speeds re-runs)')
    a = ap.parse_args()

    wt = a.worktree
    tmap, name2va = load_tmap(a.tmap or os.path.join(wt, 'scripts/target_symbol_map.json'))
    res = json.load(open(a.results))
    tus = [t for t in res if isinstance(res[t], list)]
    want_cls = set(a.classes.split(','))

    idx = None
    if a.cache and os.path.exists(a.cache):
        import pickle
        idx = pickle.load(open(a.cache, 'rb'))
        print('index loaded from %s' % a.cache, file=sys.stderr)
    if idx is None:
        print('indexing %d objs ...' % len(tus), file=sys.stderr)
        idx = Index(wt, tus)
        if a.cache:
            import pickle
            pickle.dump(idx, open(a.cache, 'wb'), 4)
    print('index: %d objs, %d COMDATs, %d families, %d ambiguous-body names'
          % (idx.n_obj, idx.n_fn, len(idx.fam), len(idx.ambig)), file=sys.stderr)
    spans = Spans(wt, tus)
    print('splits: %d headers, %d resolvable to a wired unit'
          % (len(spans.units), sum(1 for h in spans.header2tu if spans.header2tu[h])),
          file=sys.stderr)

    # ---- attach homing records to families ------------------------------
    # rec_by_name: name -> (tu, record).  A name may appear in several TUs with
    # the same record content (same body -> same hits); keep the first.
    rec_by_name = {}
    hits_by_key = {}
    key_incons = 0
    for tu, recs in sorted(res.items()):
        if not isinstance(recs, list):
            continue
        for r in recs:
            n = r['name']
            key = idx.namekey.get(n)
            if key is None or key == 'AMBIG':
                continue
            rec_by_name.setdefault(n, (tu, r))
            h = tuple(int(x, 16) for x in (r.get('hits') or []))
            prev = hits_by_key.get(key)
            if prev is None:
                hits_by_key[key] = h
            elif prev != h:
                key_incons += 1
    print('records attached: %d names, %d families with a hit set '
          '(%d hit-set inconsistencies)'
          % (len(rec_by_name), len(hits_by_key), key_incons), file=sys.stderr)

    # ---- regime census ---------------------------------------------------
    census = Counter()
    census_recs = Counter()
    fam_regime = {}
    fam_incomplete = set()
    for key, names in idx.fam.items():
        h = hits_by_key.get(key)
        if h is None:
            continue
        n, m = len(names), len(h)
        reg = 'm==n' if m == n else ('m>n' if m > n else 'm<n')
        fam_regime[key] = reg
        # completeness: every family member must carry a homing record
        if any(nm not in rec_by_name for nm in names):
            fam_incomplete.add(key)
        if m == 0:
            continue
        fsb = ('1' if n == 1 else '2-4' if n <= 4 else '5-16' if n <= 16
               else '17-99' if n < 100 else '100+')
        census[(fsb, reg)] += 1
        nrec = sum(1 for nm in names
                   if nm in rec_by_name and rec_by_name[nm][1].get('cls') in want_cls)
        census_recs[(fsb, reg)] += nrec

    if a.census or True:
        print('\n=== family regime census (families with a non-empty hit set) ===')
        print('%-8s %-6s %8s %10s' % ('famsize', 'regime', 'families', 'MULTI recs'))
        for fsb in ('1', '2-4', '5-16', '17-99', '100+'):
            for reg in ('m==n', 'm>n', 'm<n'):
                if census[(fsb, reg)]:
                    print('%-8s %-6s %8d %10d'
                          % (fsb, reg, census[(fsb, reg)], census_recs[(fsb, reg)]))
        tot = Counter()
        totr = Counter()
        for (fsb, reg), c in census.items():
            tot[reg] += c
            totr[reg] += census_recs[(fsb, reg)]
        print('TOTAL   ', dict(tot), ' records:', dict(totr))
        print('incomplete families (a member has no homing record): %d'
              % len(fam_incomplete))
    if a.census:
        return

    # ---- resolve ---------------------------------------------------------
    class Opts:
        pass
    opts = Opts()
    opts.rule1 = a.rule1
    opts.no_rule2 = a.no_rule2
    opts.pinned_only = a.pinned_only
    opts.no_local_bijection = a.no_local_bijection
    opts.rule1_force = a.rule1_force

    guard = None
    if not a.no_content_guard:
        guard = ContentGuard(wt, tmap, name2va)

    fams = sorted((k for k in idx.fam if k in hits_by_key),
                  key=lambda k: (-len(idx.fam[k]), k))
    refus = Counter()
    lines = []

    if not a.validate:
        # ------------------------------------------------ production wave
        picks = {}      # name -> (va, key)
        for key in fams:
            names = idx.fam[key]
            if len(names) < a.min_famsize:
                continue
            hits = hits_by_key[key]
            targets = [n for n in names
                       if n in rec_by_name and rec_by_name[n][1].get('cls') in want_cls]
            if not targets:
                continue
            mapped_names = {n for n in names if n in name2va}
            opts._key = key
            opts._incomplete = key in fam_incomplete
            asg, ref = close_family(names, targets, hits, idx, spans, tmap,
                                    mapped_names, opts, fam_regime.get(key))
            refus.update(ref)
            for n, v in asg.items():
                if a.max_emitters and len(idx.emitters.get(n, ())) > a.max_emitters:
                    refus['MULTI-EMITTER-FILTERED'] += 1
                    continue
                if guard is not None and guard.conflicts(rec_by_name[n][0], n, v):
                    refus['CONTENT-CONFLICT'] += 1
                    continue
                picks[n] = (v, key)

        # global contested drop: one VA may be picked by one name only
        byva = defaultdict(list)
        for n, (v, k) in picks.items():
            byva[v].append(n)
        dropped = 0
        for v, ns in byva.items():
            if len(ns) > 1:
                for n in ns:
                    picks.pop(n, None)
                    dropped += 1
                refus['DROP-CONTESTED'] += 1
        # never overwrite an existing map entry
        for n in list(picks):
            v = picks[n][0]
            if v in tmap:
                picks.pop(n)
                refus['VA-ALREADY-MAPPED'] += 1

        out = defaultdict(list)
        for n, (v, key) in sorted(picks.items()):
            tu, r = rec_by_name[n]
            # emit under the unit whose pinned span OWNS the VA when that unit
            # also emits the symbol: objdiff pairs per unit, so the record must
            # be filed against the target obj it will actually be compared with.
            # (rec_by_name keeps the first TU that emitted a COMDAT, which for a
            # 2-3 emitter symbol need not be the one the linker contributed.)
            own = spans.header2tu.get(spans.cov.owner(v) or '') or set()
            cand = own & idx.emitters.get(n, set())
            if cand and tu not in cand:
                tu = sorted(cand)[0]
            nr = dict(r)
            nr['cls'] = 'UNIQUE'
            nr['va'] = '0x%08x' % v
            nr['n_unmapped'] = 1
            nr['disambig'] = 'FAMILY-CLOSURE'
            nr['famsize'] = len(idx.fam[key])
            nr['regime'] = fam_regime.get(key)
            out[tu].append(nr)
            lines.append('FORCED %-70s fam=%4d %-5s %s -> 0x%08x'
                         % (n[:70], len(idx.fam[key]), fam_regime.get(key), tu, v))
        json.dump(dict(out), open(a.out, 'w'), indent=1)
        open(a.report, 'w').write('\n'.join(lines) + '\n')
        print('\nrefusals:', dict(sorted(refus.items(), key=lambda kv: -kv[1])))
        print('forced %d assignments across %d units (contested dropped %d)'
              % (sum(len(v) for v in out.values()), len(out), dropped))
        fs = Counter(r['famsize'] for v in out.values() for r in v)
        print('by famsize bucket:', dict(Counter(
            ('1' if n == 1 else '2-4' if n <= 4 else '5-16' if n <= 16
             else '17-99' if n < 100 else '100+')
            for v in out.values() for r in v for n in [r['famsize']])))
        print('->', a.out)
        return

    # ------------------------------------------------------- validation
    val = Counter()
    picks = {}          # name -> (va, truth, famsize)
    for key in fams:
        names = idx.fam[key]
        if len(names) < a.min_famsize:
            continue
        hits = hits_by_key[key]
        if not hits:
            continue
        # ground truth available for this family
        truth = {}
        for v in hits:
            n = tmap.get(v)
            if n in names:
                truth[n] = v
        if not truth:
            val['FAM-NO-TRUTH'] += 1
            continue
        val['FAM-MEASURABLE'] += 1

        if a.holdout == 'family':
            # hide EVERY map fact about this family: no entry naming a member,
            # no entry on any VA of the hit set.
            eff = {}          # every map fact on a family VA is hidden
            mapped_names = set()
            hidden = set(names)
            targets = [n for n in names if n in rec_by_name]
            opts._key = key
            opts._incomplete = key in fam_incomplete
            asg, ref = close_family(names, targets, hits, idx, spans, eff,
                                    mapped_names, opts, fam_regime.get(key))
            refus.update(ref)
            for n, v in asg.items():
                if a.max_emitters and len(idx.emitters.get(n, ())) > a.max_emitters:
                    val['MULTI-EMITTER-FILTERED'] += 1
                    continue
                if guard is not None and n in rec_by_name and \
                        guard.conflicts(rec_by_name[n][0], n, v):
                    val['CONTENT-CONFLICT'] += 1
                    continue
                if n in truth:
                    picks[n] = (v, truth[n], len(names))
                else:
                    val['UNSCORABLE'] += 1
            for n, v in asg.items():
                if n in truth:
                    ok = (v == truth[n])
                    ne = len(idx.emitters.get(n, ()))
                    eb = ('1' if ne == 1 else '2-4' if ne <= 4
                          else '5-20' if ne <= 20 else '21+')
                    val['EM%s/%s' % (eb, 'HIT' if ok else 'MISS')] += 1
                    if len(names) >= 17:
                        val['F17+EM%s/%s' % (eb, 'HIT' if ok else 'MISS')] += 1
        else:
            # optimistic leave-one-out: hide exactly one member at a time
            for hold, tv in truth.items():
                eff = {v: tmap[v] for v in hits if v in tmap and v != tv}
                mapped_names = {n for n in names if n in name2va and n != hold}
                targets = [n for n in names if n in rec_by_name and n == hold]
                opts._key = key
                opts._incomplete = key in fam_incomplete
                asg, ref = close_family(names, targets, hits, idx, spans, eff,
                                        mapped_names, opts, fam_regime.get(key))
                for n, v in asg.items():
                    picks[(n, hold)] = (v, tv, len(names))

    # contested drop before scoring, exactly as in production
    byva = defaultdict(list)
    for k, (v, t, fs) in picks.items():
        byva[v].append(k)
    for v, ks in byva.items():
        names = {k if isinstance(k, str) else k[0] for k in ks}
        if len(names) > 1:
            for k in ks:
                picks.pop(k, None)
                val['DROP-CONTESTED'] += 1

    for k, (v, t, fs) in sorted(picks.items(), key=lambda kv: str(kv[0])):
        ok = (v == t)
        fsb = ('1' if fs == 1 else '2-4' if fs <= 4 else '5-16' if fs <= 16
               else '17-99' if fs < 100 else '100+')
        val['HIT' if ok else 'MISS'] += 1
        val['FAM%s/%s' % (fsb, 'HIT' if ok else 'MISS')] += 1
        if not ok:
            n = k if isinstance(k, str) else k[0]
            lines.append('VALMISS fam=%d %-64s truth=0x%08x got=0x%08x'
                         % (fs, n[:64], t, v))

    open(a.report, 'w').write('\n'.join(lines) + '\n')
    print('\n=== held-out validation (holdout=%s, rule1=%s, rule2=%s, pinned_only=%s) ==='
          % (a.holdout, a.rule1, not a.no_rule2, a.pinned_only))
    print(dict(sorted(val.items(), key=lambda kv: -kv[1])))
    h, m = val['HIT'], val['MISS']
    if h + m:
        print('  OVERALL precision %d/%d = %.3f%%' % (h, h + m, 100.0 * h / (h + m)))
    for fsb in ('1', '2-4', '5-16', '17-99', '100+'):
        hh, mm = val['FAM%s/HIT' % fsb], val['FAM%s/MISS' % fsb]
        if hh + mm:
            print('   famsize %-6s: %d/%d = %.2f%%'
                  % (fsb, hh, hh + mm, 100.0 * hh / (hh + mm)))
    hh = val['FAM17-99/HIT'] + val['FAM100+/HIT']
    mm = val['FAM17-99/MISS'] + val['FAM100+/MISS']
    if hh + mm:
        print('   famsize 17+   : %d/%d = %.2f%%' % (hh, hh + mm, 100.0 * hh / (hh + mm)))
    print('refusals:', dict(sorted(refus.items(), key=lambda kv: -kv[1])[:12]))
    if a.stats_out:
        json.dump({str(k): v for k, v in val.items()}, open(a.stats_out, 'w'), indent=1)


if __name__ == '__main__':
    main()
