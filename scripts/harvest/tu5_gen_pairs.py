#!/usr/bin/env python3
"""Regenerate pairs.json for the TU5 reloc-masked correlator stack at the
current build state.

A "pair" = a report unit that has ALL of:
  * a dtk target obj  (build/45410914/obj/<rel>.obj),
  * a compiled base obj (build/45410914/src/**/<name>.obj), and
  * >=1 anonymous fn_<addr> whose match_percent_normalized < 100.

This is the first stage of the re-runnable TU5 identification pipeline. It
replaces the ad-hoc one-off enumeration used by the original +1,493 landing
(the hardcoded ~/tmp/correlator_sizing/pairs.json), making the stack
re-runnable against any worktree/build state.

Resolution rules (learned during the 2026-07-18 lane-C re-run):
  * report unit names are 'default/<rel>' where <rel> is either a bare basename
    ('MasterAudio') or a source-relative path ('system/rndobj/Rnd'; 105 such).
    The dtk target obj tree mirrors <rel> (nested dirs for pathed names).
  * pathed units resolve their base obj directly: build/45410914/src/<rel>.obj.
  * bare-name units whose basename is ambiguous (13 duplicate basenames this
    run: Dir, Utl, Rnd, Movie, CubeTex, FxSend*, ...) are disambiguated by
    masked-content overlap (tu5_reloc_masked_correlate.func_bodies) between the
    target obj and each candidate base obj; the highest-overlap candidate wins.
    Zero-overlap winners are harmless: no shared content => no proposals downstream.
  * report contains duplicate unit names (auto_* split stubs, ~266) — first wins.

Usage:
  tu5_gen_pairs.py [--project-dir DIR] [--out pairs.json]
"""
import argparse
import json
import os
import sys
from collections import defaultdict

# Robust import of the correlator regardless of CWD: the harvest dir is always
# the directory this script lives in.
HARVEST_DIR = os.path.dirname(os.path.abspath(__file__))
if HARVEST_DIR not in sys.path:
    sys.path.insert(0, HARVEST_DIR)
import tu5_reloc_masked_correlate as C  # noqa: E402

# Repo root defaults to two levels up from scripts/harvest/.
DEFAULT_PROJECT_DIR = os.path.dirname(os.path.dirname(HARVEST_DIR))


def gen_pairs(project_dir):
    """Return (pairs, skipped_nt, skipped_nb, amb) for the given build state.

    Must be called with CWD == project_dir (paths in the result are
    project-relative, matching what the downstream stage drivers expect).
    """
    r = json.load(open('build/45410914/report.json'))

    # Index every compiled base obj by basename (handles nested src/ layout).
    base_idx = defaultdict(list)
    for dp, _, fns in os.walk('build/45410914/src'):
        for fn in fns:
            if fn.endswith('.obj'):
                base_idx[fn[:-4]].append(
                    os.path.relpath(os.path.join(dp, fn), project_dir))

    pairs, skipped_nt, skipped_nb, amb = [], [], [], []
    seen = set()
    for u in r['units']:
        nm = u['name']
        if nm in seen:  # duplicate report unit names (auto_* split stubs): first wins
            continue
        seen.add(nm)
        fns = u.get('functions', [])
        n_anon = sum(1 for f in fns if f['name'].startswith('fn_'))
        n_lt = sum(1 for f in fns if f['name'].startswith('fn_')
                   and f.get('match_percent_normalized', 0) < 100)
        if n_lt == 0:
            continue
        assert nm.startswith('default/'), nm
        rel = nm[len('default/'):]
        tgt = f'build/45410914/obj/{rel}.obj'
        if not os.path.exists(tgt):
            skipped_nt.append(nm)
            continue
        bn = rel.split('/')[-1]
        if '/' in rel:
            # pathed unit: base obj resolves directly by the same rel path.
            bo = f'build/45410914/src/{rel}.obj'
            if not os.path.exists(bo):
                cands = base_idx.get(bn, [])
                if len(cands) == 1:
                    bo = cands[0]
                else:
                    skipped_nb.append(nm)
                    continue
        else:
            # bare-name unit: resolve by basename, disambiguate dups by overlap.
            cands = base_idx.get(bn, [])
            if not cands:
                skipped_nb.append(nm)
                continue
            if len(cands) == 1:
                bo = cands[0]
            else:
                tb = set(C.func_bodies(tgt).values())
                scored = sorted(((len(tb & set(C.func_bodies(c).values())), c)
                                 for c in cands), reverse=True)
                amb.append((nm, scored))
                bo = scored[0][1]
        pairs.append(dict(name=nm, base_name=bn, tgt=tgt, baseobj=bo,
                          n_anon=n_anon, n_anon_lt100=n_lt))
    return pairs, skipped_nt, skipped_nb, amb


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default=DEFAULT_PROJECT_DIR,
                    help='repo/worktree root to enumerate (default: repo root)')
    ap.add_argument('--out', default=None,
                    help='output pairs.json path (default: <project-dir>/pairs.json)')
    args = ap.parse_args()

    project_dir = os.path.abspath(args.project_dir)
    out_path = args.out or os.path.join(project_dir, 'pairs.json')
    os.chdir(project_dir)

    pairs, skipped_nt, skipped_nb, amb = gen_pairs(project_dir)

    print('pairs:', len(pairs), 'no-tgt:', len(skipped_nt),
          'no-base:', len(skipped_nb))
    for a in amb:
        print('  amb:', a[0], '->', a[1])
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    json.dump(pairs, open(out_path, 'w'), indent=1)
    print('wrote', out_path)


if __name__ == '__main__':
    main()
