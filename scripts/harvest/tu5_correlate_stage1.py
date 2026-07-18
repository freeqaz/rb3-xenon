#!/usr/bin/env python3
"""Stage-1 CLEAN-sweep driver for the TU5 reloc-masked correlator stack.

Sweeps ALL paired units (pairs.json from tu5_gen_pairs.py) and pairs each
unmapped target fn_<addr> to a base compiled symbol by reloc-masked byte
identity (tu5_reloc_masked_correlate.func_bodies). A CLEAN 1<->1 byte-identical
pairing is a guaranteed strict-100 flip once a {addr: mangled_name} map entry is
added.

Gates are IDENTICAL to the landed +1,493 sweep (commit 366709b9): a proposal is
CLEAN only when
  * the base content matches exactly one base symbol (not 0 = nomatch, not >1 =
    MULTI ICF-ambiguous),
  * the target content is unique among unmapped fn_ (not amb_tgt), and
  * the base name is not already present (named) in the target obj (base_taken).
On top of those, this driver adds the collision guards the original one-off
lacked (needed for repeatable re-runs against an already-partially-mapped map):
  * addr_already_mapped : the fn_ addr is already a key in target_symbol_map.json,
  * denylist            : the addr is in the map's _denylist,
  * name_value_taken    : the base name is already a mapped value, and
  * a final global dedup dropping duplicate pick names (dup_pick_name).
The decision logic is otherwise byte-for-byte equivalent to the landed run.

Outputs (in --out-dir):
  proposals.json   : all CLEAN proposals (with cur_pct).
  per_unit.json    : per-unit classification counts.
  errors.json      : (unit, error) for objs that failed to parse.
  map_fragment.json: {addr: mangled_name} for YIELD proposals only
                     (cur_pct < 100) — ready for tu5_map_apply_fragment.py.

Usage:
  tu5_correlate_stage1.py [--project-dir DIR] [--pairs pairs.json] [--out-dir DIR]
"""
import argparse
import json
import os
import sys
from collections import defaultdict

HARVEST_DIR = os.path.dirname(os.path.abspath(__file__))
if HARVEST_DIR not in sys.path:
    sys.path.insert(0, HARVEST_DIR)
import tu5_reloc_masked_correlate as C  # noqa: E402

DEFAULT_PROJECT_DIR = os.path.dirname(os.path.dirname(HARVEST_DIR))


def run(project_dir, pairs_path, out_dir):
    """Run the stage-1 sweep. CWD must be project_dir."""
    os.makedirs(out_dir, exist_ok=True)
    pairs = json.load(open(pairs_path))

    # report: unit_name -> {fn_name: match%}
    r = json.load(open('build/45410914/report.json'))
    rep = {}
    for u in r['units']:
        rep[u['name']] = {f['name']: f.get('match_percent_normalized', 0)
                          for f in u.get('functions', [])}

    # existing map keys/denylist guards (parity with stage-2/3 scripts).
    _mp = json.load(open('scripts/target_symbol_map.json'))
    existing_keys = set(k.lower() for k in _mp.keys())
    denylist = set(a.lower() for a in _mp.get('_denylist', []))
    used_names = set(v for v in _mp.values() if isinstance(v, str))

    proposals = []
    per_unit = []
    errors = []
    for p in pairs:
        unit = p['name']
        try:
            tgt = C.func_bodies(p['tgt'])
            base = C.func_bodies(p['baseobj'])
        except Exception as e:
            errors.append((unit, str(e)))
            continue
        base_by_content = defaultdict(list)
        for n, b in base.items():
            base_by_content[b].append(n)
        unmapped = {n: b for n, b in tgt.items() if n.startswith('fn_')}
        tgt_content = defaultdict(list)
        for n, b in unmapped.items():
            tgt_content[b].append(n)
        tgt_named = set(n for n in tgt if not n.startswith('fn_'))
        cnt = defaultdict(int)
        match_pct = rep.get(unit, {})
        for n in sorted(unmapped):
            b = unmapped[n]
            cands = base_by_content.get(b, [])
            if len(cands) == 0:
                cnt['nomatch'] += 1
                continue
            if len(cands) > 1:
                cnt['multi'] += 1
                continue
            bn = cands[0]
            if len(tgt_content[b]) > 1:
                cnt['amb_tgt'] += 1
                continue
            if bn in tgt_named:
                cnt['base_taken'] += 1
                continue
            addr = '0x' + n[3:].lower()
            if addr in existing_keys:
                cnt['addr_already_mapped'] += 1
                continue
            if addr in denylist:
                cnt['denylist'] += 1
                continue
            if bn in used_names:
                cnt['name_value_taken'] += 1
                continue
            cur = match_pct.get(n, 0)
            proposals.append(dict(unit=unit, fn_addr=addr, fn=n,
                                  mangled_name=bn, size=len(b), cur_pct=cur))
            cnt['clean'] += 1
        per_unit.append(dict(unit=unit, **cnt, n_unmapped=len(unmapped)))

    # global dedup on pick name
    seen = {}
    deduped = []
    for x in sorted(proposals, key=lambda z: z['fn_addr']):
        if x['mangled_name'] in seen:
            x['dropped'] = 'dup_pick_name'
            continue
        seen[x['mangled_name']] = x['fn_addr']
        deduped.append(x)
    dropped_dup = len(proposals) - len(deduped)
    proposals = deduped

    yield_props = [x for x in proposals if x['cur_pct'] < 100]
    json.dump(proposals, open(os.path.join(out_dir, 'proposals.json'), 'w'), indent=1)
    json.dump(per_unit, open(os.path.join(out_dir, 'per_unit.json'), 'w'), indent=1)
    json.dump(errors, open(os.path.join(out_dir, 'errors.json'), 'w'), indent=1)
    # fragment = cur_pct<100 yield only (cur_pct==100 already matched, no yield)
    frag = {x['fn_addr']: x['mangled_name'] for x in yield_props}
    json.dump(frag, open(os.path.join(out_dir, 'map_fragment.json'), 'w'), indent=1)

    print('pairs run:', len(pairs), 'errors:', len(errors))
    print('CLEAN proposals total:', len(proposals), 'dup_pick_dropped:', dropped_dup)
    print('  yield (cur_pct<100):', len(yield_props))
    print('  cur_pct==100 (no yield):',
          sum(1 for x in proposals if x['cur_pct'] >= 100))
    agg = defaultdict(int)
    for cu in per_unit:
        for k, v in cu.items():
            if k not in ('unit',):
                agg[k] += v
    print('agg:', dict(agg))
    per_unit.sort(key=lambda x: -x.get('clean', 0))
    for x in per_unit[:15]:
        if x.get('clean'):
            print(f"  {x['unit']:45s} clean={x.get('clean',0):3d} "
                  f"multi={x.get('multi',0):3d} amb_tgt={x.get('amb_tgt',0):3d} "
                  f"nomatch={x.get('nomatch',0):4d}")
    return proposals, per_unit, errors, frag


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=DEFAULT_PROJECT_DIR,
                    help='repo/worktree root to sweep (default: repo root)')
    ap.add_argument('--pairs', default=None,
                    help='pairs.json path (default: <project-dir>/pairs.json)')
    ap.add_argument('--out-dir', default=None,
                    help='output dir (default: <project-dir>/tu5_stage1)')
    args = ap.parse_args()

    project_dir = os.path.abspath(args.project_dir)
    pairs_path = os.path.abspath(args.pairs) if args.pairs \
        else os.path.join(project_dir, 'pairs.json')
    out_dir = os.path.abspath(args.out_dir) if args.out_dir \
        else os.path.join(project_dir, 'tu5_stage1')
    os.chdir(project_dir)
    run(project_dir, pairs_path, out_dir)


if __name__ == '__main__':
    main()
