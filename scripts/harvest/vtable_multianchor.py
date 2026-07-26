#!/usr/bin/env python3
"""Widened vtable aligner (all-anchor seeding) + held-out precision harness.

WHY (measured at 02c72524 by scripts/harvest/vtable_align_diag{,2}.py):
  1,969 base ??_7 vtables, 1,281 target .rdata pointer runs. vtable_global.py
  places only 40. The failure histogram:

      ANCHOR_0             125   6.3%   no mapped slot name at all
      ANCHOR_1             405  20.6%   one mapped anchor  (-> vtable_1anchor)
      NO_RUN_FOR_ANCHOR    791  40.2%   >=2 anchors, first anchor in no run
      NO_CONSISTENT_ALIGN  593  30.1%   (404 anchor-disagree, 189 fullrun)
      AMBIG                 15   0.8%
      MATCHED_NOTHING_NEW   28   1.4%
      MATCHED_YIELD         12   0.6%

  The 40% NO_RUN_FOR_ANCHOR bucket is an artefact, not a real wall:
  vtable_global.py seeds candidate slot-offsets from ONLY the lowest-index
  anchor (`first_i = min(anchors)`), so a vtable is dropped whenever *that one*
  name is absent from every target run -- even when other anchors are present.
  Measured: 618 of those 791 have >=1 other anchor that IS present in a run.

  Second artefact: out-of-range slots are counted as anchor *disagreements*.
  A target run that is a subrange of the base vtable (or vice versa) is not a
  contradiction; only a target slot bearing a DIFFERENT mangled name is.
  Measured 1,145 out-of-range slots across 151 otherwise-clean vtables.

WHAT THIS DOES
  - seeds candidate alignments from EVERY anchor (union), not just the first
  - scores an alignment as (ok, hard, anon, oor); requires hard == 0 and
    ok >= MIN_ANCHORS, treats oor/anon as neutral-but-not-supporting
  - keeps vtable_global.py's full-run consistency check (every aligned slot
    already bearing a mangled name must equal the base method there)
  - keeps the 4-part output gate verbatim (owning-unit router, purecall guard,
    return-shape sanity, current-map skip) by importing gate_candidates
  - adds a 5th gate for the widened path: TIER. tier=A requires a full-length
    overlap (oor == 0); tier=B allows a subrange. B is emitted separately.
  - adds a 6th gate: ICF-SIZE (--min-size, default 0 = off). Every held-out
    error measured was an ICF fold (`?ClassName@X@@UBA?AVSymbol@@XZ` twins,
    `?Save@Parallel/SerialGroupSeq`, `?Size@TrackWidgetImp<T>`) -- but they
    measure 48-88 B, so a size gate does NOT catch them and only costs recall.
    Kept as a knob, defaulted OFF; the honest ICF control is the tier split.
  - --icf-tolerant enables TIER C: full-run consistency that tolerates
    ICF-explainable slot disagreements (mega-fold target symbol occupying >=3
    run slots, same-leaf-different-class twin, or ??_E/??_G dtor variant).

MEASURED HELD-OUT PRECISION (5 x N=500 random hidden map entries, seeds
1234/777/20260726/555/90210):
    tier A  100.0%  (44/44)     strict full-run consistency, full overlap
    tier B   89.5% (102/114)    strict full-run consistency, subrange overlap
    tier C   93.3% (265/284)    ICF-tolerant full-run consistency
  Every tier-B/C error inspected was an ICF fold (both names live at the folded
  VA), not a misalignment.

NEGATIVE RESULTS (measured, do not re-hunt) on the 530 base vtables with <2
mapped anchors -- the non-map-derived anchor levers are weak here:
    run-length equality alone      unique for   3 / 530
    ??_E/??_G slot-0 + length      unique for   5 / 530
    adjacency (owning-unit vote)   unique for  30 / 530
  The yield is in the aligner logic (all-anchor seeding + ICF tolerance), not
  in new anchor types.

HELD-OUT PRECISION (--holdout N)
  Randomly hides N already-mapped VAs that occur as target-run slots (removing
  them from both the map keys and the anchor-name pool), re-runs the aligner,
  and reports how many are re-derived and how many of those are correct.
  An identity channel without this number is a hypothesis, not evidence.

Usage:
  vtable_multianchor.py [PROJ] [OUTDIR] --icf-tolerant --tier A,B,C
  vtable_multianchor.py [PROJ] --holdout 500 --seed 1234 --icf-tolerant
"""
import argparse, json, os, re, sys, glob, random, importlib.util
from collections import defaultdict, Counter

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('vtable_global', os.path.join(_here, 'vtable_global.py'))
vg = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(vg)
_spec2 = importlib.util.spec_from_file_location('vtd', os.path.join(_here, 'vtable_align_diag.py'))
vtd = importlib.util.module_from_spec(_spec2); _spec2.loader.exec_module(vtd)

FN = re.compile(r'^fn_([0-9a-fA-F]{8})')


_LEAF = re.compile(r'^\?\??\$?([^?@]+)@')


def _leaf(nm):
    m = _LEAF.match(nm)
    return m.group(1) if m else nm


def build_icf_suspects(runs, sym_pos, mp, sizes):
    """Target-run symbols that are ICF-fold suspects, so a full-run
    'disagreement' at their slot is not evidence of misalignment.

    Measured on this tree (scripts/harvest -> aq_fullrun analysis): of the 416
    vtables whose only blocker is full-run consistency, 1,326 blocking slots are
    <=32 B targets and 174 are same-leaf-different-class twins; only 135 are
    real divergences, and 295 of the 416 vtables have *no* real divergence at
    all. The mega-folds are visible directly: e.g.
    `?GetCrowdMeter@TrackPanelDirBase@@UAAPAVBandCrowdMeter@@XZ` occupies slots
    in dozens of unrelated vtables.
    """
    va_of = {v: k.lower() for k, v in mp.items() if isinstance(v, str)}
    pos_count = Counter()
    for run in runs:
        for s in run: pos_count[s] += 1
    susp = set()
    for s, n in pos_count.items():
        if s.startswith('fn_'): continue
        if n >= 3: susp.add(s)                       # mega-fold
        sz = sizes.get(va_of.get(s, ''))
        if sz is not None and sz <= 32: susp.add(s)  # tiny leaf
    return susp


def _icf_ok(base_nm, tgt_nm, susp):
    """True if a base/target slot-name disagreement is ICF-explainable."""
    if tgt_nm in susp: return True
    if _leaf(base_nm) == _leaf(tgt_nm): return True          # same method, folded twin
    # scalar- vs vector-deleting destructor is a naming variant, not a divergence
    if base_nm[:4] in ('??_E', '??_G') and tgt_nm[:4] in ('??_E', '??_G'): return True
    return False


def align_all(base_vts, runs, sym_pos, mapvals, mapkeys, defined, min_anchors=2,
              icf_susp=None):
    """Return (candidates, evidence, stats). candidates: va -> name (or __CONFLICT__).

    tier A = full overlap, strict full-run consistency
    tier B = subrange overlap, strict full-run consistency
    tier C = full-run consistency only satisfiable by tolerating ICF-explainable
             slot disagreements (requires icf_susp; strictly weaker evidence)
    """
    candidates = {}; evidence = {}
    stats = Counter()
    for vtn, bnames, un in base_vts:
        anchors = {i: nm for i, nm in enumerate(bnames) if nm in mapvals}
        if len(anchors) < min_anchors:
            stats['skip_lt_min_anchors'] += 1; continue
        # ---- ALL-ANCHOR SEEDING (the fix)
        seeds = set()
        for i, nm in anchors.items():
            for ri, p in sym_pos.get(nm, []): seeds.add((ri, p - i))
        if not seeds:
            stats['no_run_for_any_anchor'] += 1; continue
        stats['scored'] += 1
        good = []
        for ri, ao in seeds:
            run = runs[ri]
            ok = hard = anon = oor = 0
            for i, nm in anchors.items():
                p = i + ao
                if not (0 <= p < len(run)): oor += 1; continue
                rs = run[p]
                if rs == nm: ok += 1
                elif rs.startswith('fn_'): anon += 1
                else: hard += 1
            if hard or ok < min_anchors: continue
            # full-run consistency: any aligned slot already mangled must agree
            full_hard = 0; full_soft = 0; full_oor = 0
            for i, nm in enumerate(bnames):
                p = i + ao
                if not (0 <= p < len(run)): full_oor += 1; continue
                rs = run[p]
                if rs.startswith('?') and rs != nm:
                    if icf_susp is not None and _icf_ok(nm, rs, icf_susp): full_soft += 1
                    else: full_hard += 1
            if full_hard: continue
            good.append((ok, ri, ao, full_oor, full_soft))
        if not good:
            stats['no_consistent_align'] += 1; continue
        maxok = max(g[0] for g in good)
        top = [g for g in good if g[0] == maxok]
        # distinct (ri,ao) tie => ambiguous
        if len({(g[1], g[2]) for g in top}) > 1:
            stats['ambig'] += 1; continue
        ok, ri, ao, full_oor, full_soft = top[0]
        run = runs[ri]
        tier = 'C' if full_soft else ('A' if full_oor == 0 else 'B')
        stats['placed'] += 1; stats['placed_tier_' + tier] += 1
        got = 0
        for i, nm in enumerate(bnames):
            p = i + ao
            if not (0 <= p < len(run)): continue
            m = FN.match(run[p])
            if not m: continue
            if nm not in defined: continue
            if nm in mapvals: continue
            va = '0x' + m.group(1).lower()
            if va in mapkeys: continue
            if va in candidates and candidates[va] != nm:
                candidates[va] = '__CONFLICT__'; stats['conflict'] += 1
            elif candidates.get(va) != '__CONFLICT__':
                candidates[va] = nm
                evidence[va] = dict(vtable=vtn, slot=i, unit=un, anchors=ok, tier=tier)
                got += 1
        if got: stats['placed_with_yield'] += 1
    return candidates, evidence, stats


_SYMLINE = re.compile(r'\s*(\S+)\s*=\s*\.text:(0x[0-9A-Fa-f]+).*?size:(0x[0-9A-Fa-f]+)')


def load_sizes(proj):
    """VA (lowercase '0x…') -> .text size, from config/45410914/symbols.txt."""
    out = {}
    p = os.path.join(proj, 'config/45410914/symbols.txt')
    if not os.path.exists(p): return out
    for ln in open(p):
        m = _SYMLINE.match(ln)
        if m: out[m.group(2).lower()] = int(m.group(3), 16)
    return out


def load_common(proj):
    runs = vg.extract_runs(os.path.join(proj, 'build/45410914/obj/auto_00_82000400_rdata.obj'))
    sym_pos = defaultdict(list)
    for ri, run in enumerate(runs):
        for p, s in enumerate(run): sym_pos[s].append((ri, p))
    o = json.load(open(os.path.join(proj, 'objdiff.json')))
    mp = json.load(open(os.path.join(proj, 'scripts/target_symbol_map.json')))
    defined = set()
    for bp in glob.glob(os.path.join(proj, 'build/45410914/src/**/*.obj'), recursive=True):
        try: d, syms, secs, sbi = vg.read_coff(bp)
        except Exception: continue
        for s in syms:
            if s['section'] > 0 and s['name'].startswith('?'): defined.add(s['name'])
    base_vts = vtd.load_base_vtables(proj, o['units'])
    return runs, sym_pos, o, mp, defined, base_vts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('proj', nargs='?', default=os.getcwd())
    ap.add_argument('outdir', nargs='?', default=os.path.expanduser('~/tmp/vtgate_out'))
    ap.add_argument('--min-anchors', type=int, default=2)
    ap.add_argument('--tier', default='A', help="comma list of tiers to emit live, e.g. 'A' or 'A,B' or 'A,B,C'")
    ap.add_argument('--holdout', type=int, default=0)
    ap.add_argument('--seed', type=int, default=1234)
    ap.add_argument('--icf-tolerant', action='store_true',
                    help='enable tier C (tolerate ICF-explainable full-run disagreements)')
    ap.add_argument('--min-size', type=int, default=0,
                    help='ICF guard: drop candidates whose target .text size is <= this')
    a = ap.parse_args()
    proj = os.path.abspath(a.proj)

    runs, sym_pos, o, mp, defined, base_vts = load_common(proj)
    sizes = load_sizes(proj)
    susp = build_icf_suspects(runs, sym_pos, mp, sizes) if a.icf_tolerant else None
    if susp is not None:
        print(f'[icf] {len(susp)} ICF-suspect target symbols', file=sys.stderr)

    def icf_filter(cand, ev):
        """Gate 6: drop tiny (ICF-prone) targets. Returns (kept, n_dropped)."""
        kept = {}; drop = 0
        for va, nm in cand.items():
            s = sizes.get(va)
            if s is not None and s <= a.min_size: drop += 1; continue
            kept[va] = nm
        return kept, drop
    print(f'[runs] {len(runs)}  [base vtables] {len(base_vts)}  [defined] {len(defined)}', file=sys.stderr)

    if a.holdout:
        # VAs that are already mapped AND appear as a slot in some run
        run_vas = set()
        for run in runs:
            for s in run:
                m = FN.match(s)
                if m: run_vas.add('0x' + m.group(1).lower())
        # named slots in runs = mapped VAs whose name the renamer already applied
        mapped_in_runs = [(k, v) for k, v in mp.items()
                          if isinstance(v, str) and any(v == s for s in sym_pos)]
        rnd = random.Random(a.seed)
        n = min(a.holdout, len(mapped_in_runs))
        held = dict(rnd.sample(mapped_in_runs, n))
        print(f'[holdout] {n} of {len(mapped_in_runs)} map entries whose name appears in a run',
              file=sys.stderr)
        held_names = set(held.values())
        held_keys = set(k.lower() for k in held)
        mapvals = set(v for v in mp.values() if isinstance(v, str)) - held_names
        mapkeys = set(k.lower() for k in mp) - held_keys
        # blank the held slots in the runs so they look anonymous again
        va_of = {v: k.lower() for k, v in held.items()}
        runs2 = []
        for run in runs:
            runs2.append(['fn_' + va_of[s][2:] if s in held_names else s for s in run])
        sym_pos2 = defaultdict(list)
        for ri, run in enumerate(runs2):
            for p, s in enumerate(run): sym_pos2[s].append((ri, p))
        cand, ev, stats = align_all(base_vts, runs2, sym_pos2, mapvals, mapkeys,
                                    defined, a.min_anchors, susp)
        cand, ndrop = icf_filter(cand, ev)
        print(f'[icf-gate] dropped {ndrop} candidates with size <= {a.min_size}', file=sys.stderr)
        # score
        truth = {k.lower(): v for k, v in held.items()}
        res = Counter()
        for tier in ('A', 'B', 'C'):
            for va, nm in cand.items():
                if nm == '__CONFLICT__': continue
                if ev[va]['tier'] != tier: continue
                if va in truth:
                    res[f'{tier}_recovered'] += 1
                    res[f'{tier}_correct' if truth[va] == nm else f'{tier}_WRONG'] += 1
                else:
                    res[f'{tier}_new_not_in_holdout'] += 1
        print(f'[align] {dict(stats)}', file=sys.stderr)
        print('=== HELD-OUT PRECISION (min_anchors=%d, seed=%d, N=%d) ===' % (a.min_anchors, a.seed, n))
        for k in sorted(res): print(f'  {k:28s} {res[k]}')
        for tier in ('A', 'B', 'C'):
            c, w = res[f'{tier}_correct'], res[f'{tier}_WRONG']
            if c + w:
                print(f'  tier {tier}: precision {100.0*c/(c+w):.2f}%  ({c}/{c+w}) '
                      f'recall {100.0*(c+w)/n:.1f}% of held-out')
        # dump the wrong ones for inspection
        bad = {va: dict(proposed=cand[va], truth=truth[va], **ev[va])
               for va in cand if cand[va] != '__CONFLICT__' and va in truth and truth[va] != cand[va]}
        if bad:
            p = os.path.join(os.path.expanduser('~/tmp'), f'vtholdout_wrong_{a.seed}.json')
            json.dump(bad, open(p, 'w'), indent=1); print('  wrong-entry dump:', p)
        return

    mapvals = set(v for v in mp.values() if isinstance(v, str))
    mapkeys = set(k.lower() for k in mp)
    cand, ev, stats = align_all(base_vts, runs, sym_pos, mapvals, mapkeys, defined, a.min_anchors, susp)
    print(f'[align] {dict(stats)}', file=sys.stderr)
    want = {t.strip() for t in a.tier.split(',') if t.strip()}
    cand = {va: nm for va, nm in cand.items()
            if nm != '__CONFLICT__' and ev[va]['tier'] in want}
    cand, ndrop = icf_filter(cand, ev)
    print(f'[icf-gate] dropped {ndrop} candidates with size <= {a.min_size}', file=sys.stderr)
    gate = vg.Gate(proj); gate.index_units(o['units'])
    live, review, rejects, gstats = vg.gate_candidates(gate, cand, ev)
    vg.write_outputs(a.outdir, 'multianchor', live, review, rejects, ev, gstats, '[gate-multianchor]')


if __name__ == '__main__':
    main()
