#!/usr/bin/env python3
"""Second-level diagnosis of vtable_global.py alignment failures.

Level-1 (vtable_align_diag.py) showed the two dominant buckets are
NO_RUN_FOR_ANCHOR (40%) and NO_CONSISTENT_ALIGN/anchor_disagree (20%).
This script tests the two mechanical hypotheses behind them:

H1  NO_RUN_FOR_ANCHOR is an artefact of seeding candidate alignments from
    ONLY the first (lowest-index) anchor. If the first anchor's name does not
    appear in any target run, the vtable is dropped even when other anchors
    do appear. -> measure: how many NO_RUN_FOR_ANCHOR vtables have >=1 OTHER
    anchor that IS present in some run.

H2  anchor_disagree over-counts. `bad` is incremented for a slot that is
    (a) out of the run's range, or (b) anonymous fn_ in the target. Neither is
    a *disagreement* -- (a) is "run is a subrange / base vtable is longer",
    (b) is "slot not yet identified". Only (c) "target slot carries a
    DIFFERENT mangled name" is real contradiction (and even that can be an ICF
    fold). -> measure: for the best alignment of each such vtable, split the
    bad slots into out_of_range / anon / named_differently.

Usage: vtable_align_diag2.py [PROJ]
"""
import json, os, re, sys, glob, importlib.util
from collections import Counter, defaultdict

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('vtable_global', os.path.join(_here, 'vtable_global.py'))
vg = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(vg)
_spec2 = importlib.util.spec_from_file_location('vtd', os.path.join(_here, 'vtable_align_diag.py'))
vtd = importlib.util.module_from_spec(_spec2); _spec2.loader.exec_module(vtd)

PROJ = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
RDATA_OBJ = os.path.join(PROJ, 'build/45410914/obj/auto_00_82000400_rdata.obj')
FN = re.compile(r'^fn_([0-9a-fA-F]{8})')


def main():
    runs = vg.extract_runs(RDATA_OBJ)
    sym_pos = defaultdict(list)
    for ri, run in enumerate(runs):
        for p, s in enumerate(run): sym_pos[s].append((ri, p))

    o = json.load(open(os.path.join(PROJ, 'objdiff.json')))
    mp = json.load(open(os.path.join(PROJ, 'scripts/target_symbol_map.json')))
    mapvals = set(v for v in mp.values() if isinstance(v, str))
    base_vts = vtd.load_base_vtables(PROJ, o['units'])

    h1 = Counter(); h2 = Counter()
    h1_extra_anchor_hist = Counter()
    for vtn, bnames, un in base_vts:
        anchors = {i: nm for i, nm in enumerate(bnames) if nm in mapvals}
        if len(anchors) < 2: continue
        first_i = min(anchors)
        seeded = set()
        for ri, p in sym_pos.get(anchors[first_i], []): seeded.add((ri, p - first_i))

        # ---- H1 : does ANY anchor land in a run?
        allseed = set()
        present = 0
        for i, nm in anchors.items():
            locs = sym_pos.get(nm, [])
            if locs: present += 1
            for ri, p in locs: allseed.add((ri, p - i))
        if not seeded:
            h1['no_run_for_first_anchor'] += 1
            if present:
                h1['  BUT other anchors present'] += 1
                h1_extra_anchor_hist[min(present, 6)] += 1
                h1['  extra_seed_alignments'] += len(allseed)
            else:
                h1['  no anchor present at all'] += 1

        # ---- H2 : classify disagreements at the *best* alignment
        if not seeded and not allseed: continue
        best = None
        for ri, ao in (allseed or seeded):
            run = runs[ri]
            ok = oor = anon = named = 0
            for i, nm in anchors.items():
                p = i + ao
                if not (0 <= p < len(run)): oor += 1; continue
                rs = run[p]
                if rs == nm: ok += 1
                elif rs.startswith('fn_'): anon += 1
                else: named += 1
            score = (ok, -named, -anon, -oor)
            if best is None or score > best[0]: best = (score, ok, oor, anon, named, ri, ao)
        if best is None: continue
        _, ok, oor, anon, named, ri, ao = best
        if ok >= 2 and named == 0 and anon == 0 and oor == 0:
            h2['clean_already'] += 1
        elif ok >= 2 and named == 0:
            h2['soft_only (oor/anon, no contradiction)'] += 1
            h2['  slots_oor'] += oor; h2['  slots_anon'] += anon
        elif ok >= 2 and named > 0:
            h2['hard_contradiction (named differs)'] += 1
            h2['  slots_named_diff'] += named
        else:
            h2['ok_lt2'] += 1

    print('=== H1: first-anchor-only seeding ===')
    for k, v in h1.items(): print(f'{k:36s} {v:6d}')
    print('  #other-anchors-present histogram:', dict(sorted(h1_extra_anchor_hist.items())))
    print()
    print('=== H2: disagreement composition at best alignment (>=2-anchor vtables) ===')
    for k, v in h2.items(): print(f'{k:40s} {v:6d}')


if __name__ == '__main__':
    main()
