#!/usr/bin/env python3
"""Fail the build when the dtk-split TARGET objs have not been renamed.

WHY THIS EXISTS
---------------
`obj_target_symbol_renamer` rewrites the split target objs' anonymous
`fn_<addr>` symbols to MSVC mangled names, and **objdiff pairs target rows
against our compiled rows BY NAME**.  If a split re-emits virgin objs and the
renamer does not run over them, essentially every named row loses its partner
and the whole-binary metric collapses -- with a **settled, zero-error build**
and nothing on stderr.

⛔ MEASURED ON `main`, 2026-08-21: a clean settled build reported

      matched_functions 22962   matched_code_percent 8.633728

when the true value was

      matched_functions 42198   matched_code_percent 36.730980

a **28-point** misreport.  The target objs carried **1,415** mangled symbols
instead of **80,036**.  `rm build/<title>/target_symbol_renames.stamp && touch
config/<title>/config.yml && ninja` restored it exactly (`1826 files patched,
85343 total symbol renames`).

The failure is silent in BOTH directions that matter: the renamer is a **no-op
on already-renamed objs**, so re-running it reports `0 files patched` and exits
0 whether or not anything needed doing (see
`project_split_did_not_depend_on_map_2026-08-21`), and the *stamp* can be newer
than the objs it attests to.  ⇒ **Attest to the OBJS, not to the stamp.**

THE INVARIANT, AND WHY IT IS SELF-CALIBRATING
---------------------------------------------
Read `scripts/target_symbol_map.json` -- the renamer's own input -- and ask what
fraction of the names it wants to install are actually PRESENT as symbols in the
target objs.  There is no hardcoded expected count, so the check does not rot as
the map grows or as pins move (`total_code` and the reachable ceiling both move
in this tree; a memorised constant would be wrong within days).

  healthy : 25,482 of 29,008 map names present = 87.8%
  virgin  : far below the floor -- the names simply are not there yet

⚠ Coverage is NOT expected to reach 100%: a map name whose address lies outside
every pinned `.text` range has no target obj to live in.  The floor is therefore
deliberately loose -- it is a **catastrophe detector**, not a quality metric.
Do not tighten it into one; a gate that fires on ordinary pin churn gets
disabled, and then it protects nothing.

⛔ AND THE DENOMINATOR ITSELF NEEDS A FLOOR, because the ratio above is taken
OVER THE MAP.  An empty map gives 0/0, and this gate used to answer that with
`return 0` and the words "skipped (map is empty)" -- passing over a population
of nothing, in exactly the state it exists to catch.  Measured 2026-09-11 (lane
GATE-DISC): with `{}` as the map, this gate exited 0 AND
`tools/map_name_injectivity.py` reported "0 applied rows ... injective" and
exited 0, so an emptied or truncated `scripts/target_symbol_map.json` passed
EVERY in-graph gate that reads it -- while the renamer installed no names and
the metric would read ~28 points low off a clean, settled build.  Hence
`DEFAULT_MIN_MAP_NAMES`: a gate whose input can be empty is a gate that cannot
fail.
"""
import argparse
import glob
import json
import os
import re
import sys

MANGLED = re.compile(rb'\?[A-Za-z_?][\x21-\x7e]{3,120}')

#: Absolute floor on the MAP'S OWN SIZE.  Live is 28,960 distinct names over
#: 29,061 address rows; this floor sits far below that so ordinary pin churn
#: and map curation can never trip it, and far enough above zero that a
#: truncated or emptied map does.  Same idiom, and the same reasoning, as
#: DEFAULT_MIN_DECLARED in scripts/verify_objs_patched.py.
#:
#: ⛔ WITHOUT IT THIS GATE IS VACUOUS IN PRECISELY ITS OWN CATASTROPHE.  The
#: check is a FRACTION OF THE MAP -- "how many of the names it wants to install
#: are present in the target objs" -- so an empty map makes that 0/0, and the
#: code used to `return 0` with the words "skipped (map is empty)".  But an
#: empty map is not a benign absence: the renamer would install NO names, every
#: named row would lose its partner, and the whole-binary metric would read ~28
#: points low off a clean, settled, zero-error build -- the exact misreport
#: measured on main 2026-08-21 and recorded in this file's header.
#:
#: Measured 2026-09-11 (lane GATE-DISC): with `{}` as the map this gate exited
#: 0, and `tools/map_name_injectivity.py` independently reported "0 applied
#: rows, 0 distinct names, injective" and also exited 0 -- so an emptied map
#: passed EVERY in-graph gate that reads it.
DEFAULT_MIN_MAP_NAMES = 1000


def map_names(map_path):
    with open(map_path) as fh:
        raw = json.load(fh)
    out = set()
    for _k, v in raw.items():
        if isinstance(v, str):
            out.add(v)
        elif isinstance(v, list) and v and isinstance(v[0], str):
            out.add(v[0])
    return out


def obj_symbols(obj_dir):
    found = set()
    n = 0
    for p in glob.glob(os.path.join(obj_dir, '**', '*.obj'), recursive=True):
        n += 1
        with open(p, 'rb') as fh:
            found |= {m.decode('ascii', 'replace') for m in MANGLED.findall(fh.read())}
    return found, n


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--title', default='45410914')
    ap.add_argument('--project-dir', default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument('--floor', type=float, default=40.0,
                    help='minimum %% of map names present in target objs '
                         '(healthy measures ~88%%; virgin is near zero)')
    ap.add_argument('--min-map-names', type=int, default=DEFAULT_MIN_MAP_NAMES,
                    help='refuse if the map declares fewer than this many '
                         'mangled names (default: %(default)s; live is ~28,960).'
                         ' Lower it only for a synthetic fixture -- lowering it '
                         'on the real tree restores the vacuity this gate was '
                         'measured to have.')
    ap.add_argument('--stamp', help='touch this path on success')
    args = ap.parse_args()

    root = args.project_dir
    mp = os.path.join(root, 'scripts', 'target_symbol_map.json')
    od = os.path.join(root, 'build', args.title, 'obj')
    if not os.path.isdir(od) or not os.path.exists(mp):
        # Nothing split yet -- not this gate's business to invent a failure.
        print('[renamed-check] skipped (no target objs or no map yet)')
        return 0

    # The map's own size is asserted BEFORE the objs are scanned: it is the
    # denominator of every number below, and scanning 3,000+ objects to divide
    # by zero would be slower and no more informative.
    names = map_names(mp)
    if len(names) < args.min_map_names:
        print(f'[renamed-check] {mp} declares {len(names)} mangled name(s), '
              f'below the floor of {args.min_map_names}')
        print('THE MAP IS EMPTY OR TRUNCATED -- so this check has no '
              'denominator.  Coverage here is a FRACTION OF THE MAP, and an '
              'empty map makes it 0/0: the gate would pass over a population '
              'of nothing, which is the one state it must never call healthy. '
              'With no map the renamer installs NO names, every named row '
              'loses its partner, and the whole-binary metric reads ~28 points '
              'low off a clean, settled, zero-error build.')
        print('  fix: git checkout -- scripts/target_symbol_map.json, then '
              're-run the build.')
        return 1
    found, nobjs = obj_symbols(od)
    hit = names & found
    pct = 100.0 * len(hit) / len(names)
    line = (f'[renamed-check] {len(hit)}/{len(names)} map names present in '
            f'{nobjs} target objs = {pct:.1f}% (floor {args.floor:.0f}%)')
    if pct < args.floor:
        print(line)
        print('TARGET OBJS ARE NOT RENAMED -- every named row will fail to pair '
              'and the whole-binary metric is MEANINGLESS until this is fixed.')
        print(f'  fix: rm build/{args.title}/target_symbol_renames.stamp && '
              f'touch config/{args.title}/config.yml && ninja')
        print('  (the renamer is a no-op on already-renamed objs, so only a '
              'fresh SPLIT followed by the renamer repairs this)')
        return 1
    print(line)
    if args.stamp:
        # Content-addressed like the other always-dirty gates: rewrite the stamp
        # ONLY when what it attests to actually moved, so `restat=True` lets
        # ninja clean the (expensive) REPORT edge on a steady-state build.  A
        # stamp that is rewritten every run would re-report every build.
        attest = f'{len(hit)}/{len(names)}\n'
        try:
            with open(args.stamp) as fh:
                same = fh.read() == attest
        except OSError:
            same = False
        if not same:
            with open(args.stamp, 'w') as fh:
                fh.write(attest)
    return 0


if __name__ == '__main__':
    sys.exit(main())
