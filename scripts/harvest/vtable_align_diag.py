#!/usr/bin/env python3
"""Diagnose WHY vtable_global.py's alignment fails for most base vtables.

vtable_global.py places a base ??_7Class@@6B@ COMDAT onto a retail .rdata
pointer run only when >=2 already-mapped anchor names align at a constant slot
offset with 0 disagreements. At HEAD that succeeds for ~40 of ~1,969 base
vtables. This script classifies every base vtable into exactly one failure (or
success) bucket so the bottleneck is measured, not guessed.

Buckets (evaluated in order):
  ANCHOR_0            no slot name is in the current map's values at all
  ANCHOR_1            exactly one mapped anchor  (-> vtable_1anchor.py's pass)
  NO_RUN_FOR_ANCHOR   >=2 anchors but the first anchor name appears in NO run
  NO_CONSISTENT_ALIGN >=2 anchors, candidate runs exist, but none survives the
                      >=2-ok/0-bad + full-run-consistency test
  AMBIG               >=2 surviving alignments tied at max ok count
  MATCHED_NOTHING_NEW placed, but every anon slot is already mapped / name
                      already located / not defined by us => 0 candidates
  MATCHED_YIELD       placed and produced >=1 candidate

Also reports, for the ANCHOR_0/ANCHOR_1 buckets, how many *would* be placeable
by length-and-shape alone (how many runs have exactly that slot count), which
bounds what a length-based anchor could ever buy.

Usage: vtable_align_diag.py [PROJ]
"""
import json, os, re, sys, glob, importlib.util
from collections import Counter, defaultdict

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('vtable_global', os.path.join(_here, 'vtable_global.py'))
vg = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(vg)

PROJ = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
RDATA_OBJ = os.path.join(PROJ, 'build/45410914/obj/auto_00_82000400_rdata.obj')
FN = re.compile(r'^fn_([0-9a-fA-F]{8})')


def load_base_vtables(proj, objdiff_units):
    seen = set(); out = []
    for u in objdiff_units:
        bp = u.get('base_path')
        if not bp or not os.path.exists(bp): continue
        try: d, syms, secs, sbi = vg.read_coff(bp)
        except Exception: continue
        for vtn, slots in vg.vtable_slots(d, syms, secs, sbi).items():
            bnames = [nm for off, nm in sorted(slots)
                      if nm.startswith('?') and not nm.startswith('??_7')]
            if len(bnames) < 3: continue
            key = (vtn, tuple(bnames))
            if key in seen: continue
            seen.add(key); out.append((vtn, bnames, u['name']))
    return out


def main():
    runs = vg.extract_runs(RDATA_OBJ)
    sym_pos = defaultdict(list)
    for ri, run in enumerate(runs):
        for p, s in enumerate(run):
            sym_pos[s].append((ri, p))
    runs_by_len = Counter(len(r) for r in runs)

    o = json.load(open(os.path.join(PROJ, 'objdiff.json')))
    mp = json.load(open(os.path.join(PROJ, 'scripts/target_symbol_map.json')))
    mapvals = set(v for v in mp.values() if isinstance(v, str))
    mapkeys = set(k.lower() for k in mp)

    defined = set()
    for bp in glob.glob(os.path.join(PROJ, 'build/45410914/src/**/*.obj'), recursive=True):
        try: d, syms, secs, sbi = vg.read_coff(bp)
        except Exception: continue
        for s in syms:
            if s['section'] > 0 and s['name'].startswith('?'): defined.add(s['name'])

    base_vts = load_base_vtables(PROJ, o['units'])

    bucket = Counter()
    detail = defaultdict(list)
    # for ANCHOR_0/1: how constraining is length alone?
    len_uniq = Counter()
    anon_reach = Counter()   # bucket -> anon slots that a placement could name

    for vtn, bnames, un in base_vts:
        anchors = {i: nm for i, nm in enumerate(bnames) if nm in mapvals}
        n = len(bnames)
        if len(anchors) == 0:
            bucket['ANCHOR_0'] += 1; detail['ANCHOR_0'].append((vtn, un, n, 0))
            len_uniq['ANCHOR_0_len_unique' if runs_by_len.get(n, 0) == 1 else
                     ('ANCHOR_0_len_none' if runs_by_len.get(n, 0) == 0 else 'ANCHOR_0_len_multi')] += 1
            continue
        if len(anchors) == 1:
            bucket['ANCHOR_1'] += 1; detail['ANCHOR_1'].append((vtn, un, n, 1))
            len_uniq['ANCHOR_1_len_unique' if runs_by_len.get(n, 0) == 1 else
                     ('ANCHOR_1_len_none' if runs_by_len.get(n, 0) == 0 else 'ANCHOR_1_len_multi')] += 1
            continue

        first_i = min(anchors)
        cand = set()
        for ri, p in sym_pos.get(anchors[first_i], []):
            cand.add((ri, p - first_i))
        if not cand:
            bucket['NO_RUN_FOR_ANCHOR'] += 1
            detail['NO_RUN_FOR_ANCHOR'].append((vtn, un, n, len(anchors)))
            continue

        good = []; reasons = Counter()
        for ri, ao in cand:
            run = runs[ri]; ok = 0; bad = 0
            for i, nm in anchors.items():
                p = i + ao
                if 0 <= p < len(run) and run[p] == nm: ok += 1
                else: bad += 1
            if bad != 0: reasons['anchor_disagree'] += 1; continue
            if ok < 2: reasons['anchor_lt2'] += 1; continue
            full_bad = 0
            for i, nm in enumerate(bnames):
                p = i + ao
                if not (0 <= p < len(run)): continue
                rs = run[p]
                if rs.startswith('?') and rs != nm: full_bad += 1
            if full_bad: reasons['fullrun_disagree'] += 1; continue
            good.append((ri, ao, ok))
        if not good:
            top = reasons.most_common(1)[0][0] if reasons else 'none'
            bucket['NO_CONSISTENT_ALIGN'] += 1
            bucket['  reason:' + top] += 1
            detail['NO_CONSISTENT_ALIGN'].append((vtn, un, n, len(anchors)))
            continue
        maxok = max(g[2] for g in good)
        top = [g for g in good if g[2] == maxok]
        if len(top) > 1:
            bucket['AMBIG'] += 1; detail['AMBIG'].append((vtn, un, n, len(anchors)))
            continue
        ri, ao, ok = top[0]; run = runs[ri]
        newc = 0; anon = 0
        for i, nm in enumerate(bnames):
            p = i + ao
            if not (0 <= p < len(run)): continue
            m = FN.match(run[p])
            if not m: continue
            anon += 1
            if nm not in defined: continue
            if nm in mapvals: continue
            if '0x' + m.group(1).lower() in mapkeys: continue
            newc += 1
        if newc:
            bucket['MATCHED_YIELD'] += 1; anon_reach['yield_candidates'] += newc
        else:
            bucket['MATCHED_NOTHING_NEW'] += 1
        anon_reach['matched_anon_slots'] += anon

    total = len(base_vts)
    print(f'=== base vtables: {total}   target runs: {len(runs)} ===')
    order = ['ANCHOR_0', 'ANCHOR_1', 'NO_RUN_FOR_ANCHOR', 'NO_CONSISTENT_ALIGN',
             '  reason:anchor_disagree', '  reason:fullrun_disagree', '  reason:anchor_lt2',
             'AMBIG', 'MATCHED_NOTHING_NEW', 'MATCHED_YIELD']
    for k in order:
        if k in bucket:
            pct = 100.0 * bucket[k] / total
            print(f'{k:30s} {bucket[k]:6d}  {pct:5.1f}%')
    print()
    for k, v in sorted(len_uniq.items()): print(f'{k:30s} {v:6d}')
    print()
    for k, v in sorted(anon_reach.items()): print(f'{k:30s} {v:6d}')

    # anon-slot reach of the unplaced buckets: what is on the table
    print()
    for b in ('ANCHOR_0', 'ANCHOR_1', 'NO_RUN_FOR_ANCHOR', 'NO_CONSISTENT_ALIGN', 'AMBIG'):
        tot_slots = sum(d[2] for d in detail[b])
        print(f'{b:22s} vtables={len(detail[b]):5d}  base slots={tot_slots:7d}')

    if os.environ.get('VTDIAG_DUMP'):
        json.dump({k: v for k, v in detail.items()},
                  open(os.environ['VTDIAG_DUMP'], 'w'), indent=1)
        print('dumped', os.environ['VTDIAG_DUMP'])


if __name__ == '__main__':
    main()
