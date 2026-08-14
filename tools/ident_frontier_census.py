#!/usr/bin/env python3
"""Census the IDENTIFICATION frontier: which retail rows lack a name, and for
how many of those do we actually hold a compiled body?

WHY THIS EXISTS
---------------
Five independent lanes bottomed out on the same wall from different directions:

  * SPLITBLOCK-1  244 of 246 epilogue-overcarve blocks are gated behind the map,
                  not the splitter; 159 anon-head runs sit at fuzzy 0.000.
  * PINSRC-1      3,218 of 3,310 owned functions have no retail row bearing
                  their name.  "A pin does not create a name."
  * INCOMPLETE-1  87.5% of charged relocation-name pairs have placeholder
                  targets.

All three are the same quantity seen from three sides: IDENTIFICATION COVERAGE.
Nobody had censused it directly, and -- crucially -- nobody had separated
"unnamed" from "unnamed AND we hold the body", which is the only slice any
channel can reach.

THE THREE QUESTIONS, KEPT SEPARATE
----------------------------------
  Q1  How many retail rows carry a placeholder name?          (coverage)
  Q2  Of those, how many are STRUCTURALLY UNREACHABLE?        (subtract first)
  Q3  Of the remainder, for how many do we hold a body?       (the frontier)

Q2 is where every optimistic estimate dies, so it is subtracted BEFORE any
headline number is printed.  A row is structurally unreachable when it lives in
a unit that can never draw credit -- no compiled base obj (auto_* carves and the
230 declared-but-sourceless xdk units) -- or when it is a funclet.

USAGE
    python3 tools/ident_frontier_census.py --worktree <wt> [--json out.json]
"""

import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

# objdiff-core diff/code.rs is_placeholder_symbol_name
PLACEHOLDER_RE = re.compile(r'^(fn|lbl|jumptable|data|bss|rdata|sdata|sbss)_[0-9A-Fa-f]+$')


def is_placeholder(name: str) -> bool:
    return bool(PLACEHOLDER_RE.match(name))


def is_funclet(name: str) -> bool:
    return '__unwind$' in name or '__catch$' in name or '__ehhandler$' in name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--json')
    args = ap.parse_args()
    wt = Path(args.worktree)

    report = json.loads((wt / 'build/45410914/report.json').read_text())
    objdiff = json.loads((wt / 'objdiff.json').read_text())

    # unit name -> config (base_path presence is what decides pairability)
    cfg = {u['name']: u for u in objdiff['units']}

    m = report['measures']
    total_functions = int(m['total_functions'])
    total_code = int(m['total_code'])

    rows = []
    for unit in report['units']:
        uname = unit['name']
        uc = cfg.get(uname, {})
        base_path = uc.get('base_path')
        meta = unit.get('metadata') or {}
        auto = bool(meta.get('auto_generated'))
        cats = meta.get('progress_categories') or []
        src = meta.get('source_path')
        # protobuf-JSON omits defaults: a zero-function unit has NO 'functions' key
        for f in unit.get('functions', []):
            rows.append({
                'unit': uname,
                'name': f['name'],
                'size': int(f.get('size', 0) or 0),
                'mpn': float(f.get('match_percent_normalized', 0.0) or 0.0),
                'has_base': bool(base_path),
                'auto': auto,
                'cats': cats,
                'src': src,
            })

    # --- self-validation: rows must sum to the report's own totals ---
    assert len(rows) == total_functions, f'row count {len(rows)} != {total_functions}'
    byte_sum = sum(r['size'] for r in rows)
    assert byte_sum == total_code, f'byte sum {byte_sum} != {total_code}'
    print(f'SELF-VALIDATION OK: {len(rows)} rows == total_functions, '
          f'{byte_sum} B == total_code')
    print()

    def tally(pred):
        n = sum(1 for r in rows if pred(r))
        b = sum(r['size'] for r in rows if pred(r))
        return n, b

    def pct(b):
        return 100.0 * b / total_code

    print('=== Q1. IDENTIFICATION COVERAGE (whole binary) ===')
    named_n, named_b = tally(lambda r: not is_placeholder(r['name']))
    ph_n, ph_b = tally(lambda r: is_placeholder(r['name']))
    print(f'  NAMED        {named_n:7d} rows  {named_b:10,d} B  {pct(named_b):6.2f}%')
    print(f'  PLACEHOLDER  {ph_n:7d} rows  {ph_b:10,d} B  {pct(ph_b):6.2f}%')
    print(f'  coverage by rows = {100.0*named_n/len(rows):.2f}%   '
          f'by bytes = {pct(named_b):.2f}%')
    print()

    print('=== Q2. STRUCTURAL SUBTRACTIONS from the placeholder population ===')
    ph = [r for r in rows if is_placeholder(r['name'])]

    def ptally(pred):
        n = sum(1 for r in ph if pred(r))
        b = sum(r['size'] for r in ph if pred(r))
        return n, b

    strata = [
        ('funclet (unwind/catch)', lambda r: is_funclet(r['name'])),
        ('no base obj: auto_* carve', lambda r: not is_funclet(r['name']) and not r['has_base'] and r['auto']),
        ('no base obj: pinned, no src', lambda r: not is_funclet(r['name']) and not r['has_base'] and not r['auto']),
        ('HAS base obj (reachable)', lambda r: not is_funclet(r['name']) and r['has_base']),
    ]
    for label, pred in strata:
        n, b = ptally(pred)
        print(f'  {label:32s} {n:7d} rows  {b:10,d} B  {pct(b):6.2f}%')
    print()

    print('=== Q2b. placeholder rows WITH a base obj, by progress category ===')
    reach = [r for r in ph if r['has_base'] and not is_funclet(r['name'])]
    catc = Counter()
    catb = Counter()
    for r in reach:
        key = ','.join(r['cats']) or '(none)'
        catc[key] += 1
        catb[key] += r['size']
    for k, c in catc.most_common():
        print(f'  {k:24s} {c:7d} rows  {catb[k]:10,d} B  {pct(catb[k]):6.2f}%')
    print()

    print('=== Q2c. top units holding reachable placeholder rows ===')
    uc_ = Counter()
    ub_ = Counter()
    for r in reach:
        uc_[r['unit']] += 1
        ub_[r['unit']] += r['size']
    for u, b in ub_.most_common(25):
        print(f'  {u:52s} {uc_[u]:5d} rows  {b:9,d} B')
    print()

    if args.json:
        out = {
            'total_functions': total_functions,
            'total_code': total_code,
            'named': {'rows': named_n, 'bytes': named_b},
            'placeholder': {'rows': ph_n, 'bytes': ph_b},
            'strata': {label: dict(zip(('rows', 'bytes'), ptally(pred)))
                       for label, pred in strata},
            'reachable_rows': [
                {'unit': r['unit'], 'name': r['name'], 'size': r['size'],
                 'cats': r['cats'], 'src': r['src']}
                for r in reach
            ],
        }
        Path(args.json).write_text(json.dumps(out, indent=1))
        print(f'wrote {args.json}')


if __name__ == '__main__':
    main()
