#!/usr/bin/env python3
"""Partition the "anonymous rows at 0% inside pairable units" vein by ORACLE
AVAILABILITY, because porting a body is per-function grind whose cost is
dominated by whether a faithful source oracle exists.

WHY THIS TOOL (lane P1-BODYTRIAGE, 2026-09-10)
----------------------------------------------
`docs/plans/ROADMAP_GAP_TO_TARGET_2026-09-01.md` item 4 is the last large
un-triaged vein: ~1.32 MB of anonymous zero-scoring rows in units that already
pin and already compile.  Its charter says "triage FIRST: how much is Quazal
(low value per directive) vs HMX game code with an rb3-Wii oracle; fund only
the oracle-backed slice."

The existing `nobody_*` family already censuses this class three ways (shape by
unit, UNWRITTEN-vs-DIVERGENT by function-count deficit, mis-pin contamination,
asm-extent repricing).  THIS tool adds the one axis they lack -- ORACLE
AVAILABILITY AND ORACLE VALUE -- and joins all of them into one table so a
funding decision reads off a single artifact.

WHAT IT REFUSES TO CONFLATE
---------------------------
* **Oracle PRESENCE is not oracle VALUE.**  Our `src/system/` engine is a
  VERBATIM DC3 COPY and DC3 IS NEWER, so a DC3 file existing at the same path
  proves nothing -- we already hold that source.  The tool therefore reports
  `our_loc` beside `wii_loc`/`dc3_loc` and marks a unit ORACLE_SURPLUS only when
  an oracle is materially LARGER than what we already hold.
* **"We do not hold the body" is not "we never wrote the code."**  The
  function-count deficit (retail code symbols in the pin MINUS code symbols our
  base obj defines) is the direct discriminator, and it is reported per unit.
  A unit with a NEGATIVE deficit is not short of code; its rows are divergence.
* **A row's prize is its ASM EXTENT, not its `report.json` size.**  dtk bills an
  unbounded symbol to the next boundary; one row was billed 8,852 B for a 12-byte
  `return true`.  Every ranked byte figure here is the dtk asm extent where dtk
  offers one, and the report size only where it does not.  Both are printed.

CLASSES (row-level, precedence-ordered, so the partition is exact)
------------------------------------------------------------------
  E_FOLD_AMBIG   the row's relocation-normalized body is duplicated inside its
                 OWN target obj -> which spelling a name/body belongs to is
                 destroyed by construction; cannot pay at any effort.
  D_XDK          src/xdk vendor -- out of scope for porting by directive.
  A_QUAZAL       src/network (Quazal/Rendez-vous middleware) -- LOW VALUE per
                 the standing directive (network analogue of XDK).
  B_BAND3        src/band3 HMX game code -- rb3-Wii is the oracle.
  C_SYSTEM       src/system Milo engine -- DC3 is the oracle (but see above).
  OTHER          root-level / unclassified.

SELF-VALIDATION
---------------
Rows and bytes reconstruct the class total exactly (the tool asserts), and the
class total is recomputed here from COFF + report.json independently of
`nobody_unit_census.py` -- pass `--expect-rows/--expect-bytes` to require
agreement with that tool and REFUSE on drift.

USAGE
    python3 tools/p1_body_oracle_triage.py --worktree . \
        [--tsv out.tsv] [--md out.md] [--top N] \
        [--expect-rows 6695 --expect-bytes 1176404]
"""
import argparse
import collections
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ident_body_channel import (  # noqa: E402
    build_supply, build_demand, is_placeholder, function_slices,
)

WII = Path('/home/free/code/milohax/rb3')
DC3 = Path('/home/free/code/milohax/dc3-decomp')

SIZE_RE = re.compile(
    r'# \.text:0x[0-9A-Fa-f]+ \| 0x([0-9A-F]+) \| size: (0x[0-9A-Fa-f]+)')

# Our source is a map SCAFFOLD, not code, below this many lines.  AUTOID-1
# measured 103 of 117 `src/network/` sources at <20 lines, median 7 -- they are
# literally `namespace Quazal {}`.
SCAFFOLD_LOC = 20


def origin(src):
    if not src:
        return 'OTHER'
    if src.startswith('src/network'):
        return 'A_QUAZAL'
    if src.startswith('src/band3'):
        return 'B_BAND3'
    if src.startswith('src/xdk'):
        return 'D_XDK'
    if src.startswith('src/system'):
        return 'C_SYSTEM'
    return 'OTHER'


def loc(p):
    """Line count, or -1 when the file does not exist."""
    try:
        with open(p, errors='ignore') as fh:
            return sum(1 for _ in fh)
    except OSError:
        return -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--tsv')
    ap.add_argument('--md')
    ap.add_argument('--top', type=int, default=40)
    ap.add_argument('--expect-rows', type=int)
    ap.add_argument('--expect-bytes', type=int)
    args = ap.parse_args()
    wt = Path(args.worktree)

    objdiff = json.loads((wt / 'objdiff.json').read_text())
    cfg = {u['name']: u for u in objdiff['units']}

    print('indexing COFF supply/demand...', file=sys.stderr)
    supply, _name_units, _name_size = build_supply(wt, cfg)
    demand = build_demand(wt, cfg)

    named_target_rows = sum(1 for rr in demand.values()
                            for (n, h, s) in rr if not is_placeholder(n))
    if named_target_rows < 1000:
        sys.exit('REFUSING: %d named target rows -- PRE-RENAMER worktree. Build '
                 'it first.' % named_target_rows)

    report = json.loads((wt / 'build/45410914/report.json').read_text())
    total_code = int(report['measures']['total_code'])
    rep = {}
    umeta = {}
    for u in report['units']:
        md = u.get('metadata') or {}
        umeta[u['name']] = {
            'src': md.get('source_path'),
            'ubytes': sum(int(f.get('size', 0) or 0) for f in u.get('functions', [])),
            'umatched': sum(int(f.get('size', 0) or 0) for f in u.get('functions', [])
                            if float(f.get('fuzzy_match_percent', 0) or 0) >= 100.0),
        }
        for f in u.get('functions', []):
            rep[(u['name'], f['name'])] = (int(f.get('size', 0) or 0),
                                           float(f.get('match_percent_normalized', 0) or 0))

    # ---- asm extents, per unit (the honest price)
    asm_cache = {}

    def asm_size(uname, symname):
        if uname not in asm_cache:
            tp = cfg.get(uname, {}).get('target_path')
            out = {}
            if tp:
                p = wt / tp.replace('/obj/', '/asm/').replace('.obj', '.s')
                if p.exists():
                    for line in p.read_text(errors='ignore').splitlines():
                        m = SIZE_RE.match(line)
                        if m:
                            out[m.group(1).lower()] = int(m.group(2), 16)
            asm_cache[uname] = out
        parts = symname.split('_')
        if len(parts) < 2:
            return None
        return asm_cache[uname].get(parts[1].lower())

    # ---- class 3 + fold ambiguity, recomputed independently
    per_unit = collections.defaultdict(
        lambda: {'rows': 0, 'rep': 0, 'asm': 0, 'ambig_rows': 0, 'ambig_asm': 0})
    cls_rows = collections.Counter()
    cls_rep = collections.Counter()
    cls_asm = collections.Counter()
    unit_class = {}
    tot_rows = tot_rep = tot_asm = 0

    for uname, rr in demand.items():
        has_base = bool(cfg.get(uname, {}).get('base_path'))
        hcount = collections.Counter(h for (n, h, s) in rr)
        src = umeta.get(uname, {}).get('src')
        o = origin(src)
        for name, h, _slice in rr:
            if not is_placeholder(name):
                continue
            rsize, rmpn = rep.get((uname, name), (0, 0.0))
            if supply.get(h):          # we hold this body somewhere
                continue
            if not has_base:           # IDENT-1 class 2, not this vein
                continue
            if rmpn >= 100.0:          # already funclet-paired, no name needed
                continue
            a = asm_size(uname, name)
            a = rsize if a is None else a
            k = 'E_FOLD_AMBIG' if hcount[h] >= 2 else o
            cls_rows[k] += 1
            cls_rep[k] += rsize
            cls_asm[k] += a
            tot_rows += 1
            tot_rep += rsize
            tot_asm += a
            pu = per_unit[uname]
            pu['rows'] += 1
            pu['rep'] += rsize
            pu['asm'] += a
            if k == 'E_FOLD_AMBIG':
                pu['ambig_rows'] += 1
                pu['ambig_asm'] += a
            unit_class[uname] = o

    # ---- SELF-VALIDATION
    su_rows = sum(v['rows'] for v in per_unit.values())
    su_rep = sum(v['rep'] for v in per_unit.values())
    su_asm = sum(v['asm'] for v in per_unit.values())
    assert (su_rows, su_rep, su_asm) == (tot_rows, tot_rep, tot_asm), 'per-unit table drift'
    assert sum(cls_rows.values()) == tot_rows and sum(cls_rep.values()) == tot_rep
    print(f'SELF-VALIDATION OK: per-unit and per-class tables both reconstruct '
          f'{tot_rows} rows / {tot_rep:,} B (report) / {tot_asm:,} B (asm)')
    if args.expect_rows is not None and args.expect_rows != tot_rows:
        sys.exit(f'REFUSING: rows {tot_rows} != expected {args.expect_rows} '
                 f'(nobody_unit_census disagreement -- definitions drifted)')
    if args.expect_bytes is not None and args.expect_bytes != tot_rep:
        sys.exit(f'REFUSING: bytes {tot_rep} != expected {args.expect_bytes}')

    # ---- per-unit oracle + deficit
    slices_cache = {}

    def nslices(p):
        if p not in slices_cache:
            slices_cache[p] = sum(1 for _ in function_slices(p)) if p and p.exists() else 0
        return slices_cache[p]

    rowsout = []
    for uname, pu in per_unit.items():
        src = umeta.get(uname, {}).get('src') or ''
        u = cfg.get(uname, {})
        bp = wt / u['base_path'] if u.get('base_path') else None
        tp = wt / u['target_path'] if u.get('target_path') else None
        nb, nt = nslices(bp), nslices(tp)
        ourl = loc(wt / src) if src else -1
        wiil = loc(WII / src) if src else -1
        dc3l = loc(DC3 / src) if src else -1
        best = max(wiil, dc3l)
        if best < 0:
            ostate = 'NONE'
        elif ourl < SCAFFOLD_LOC:
            ostate = 'ORACLE_ONLY(our src is a scaffold)'
        elif best > ourl * 1.15:
            ostate = 'ORACLE_SURPLUS'
        else:
            ostate = 'WE_ALREADY_HOLD'
        if ourl >= 0 and ourl < SCAFFOLD_LOC and best < 0:
            ostate = 'SCAFFOLD_NO_ORACLE'
        rowsout.append(dict(
            unit=uname, cls=unit_class.get(uname, 'OTHER'), rows=pu['rows'],
            rep=pu['rep'], asm=pu['asm'], ambig_asm=pu['ambig_asm'],
            deficit=nt - nb, tgtfn=nt, ourfn=nb, ostate=ostate,
            our_loc=ourl, wii_loc=wiil, dc3_loc=dc3l,
            unit_matched=(100.0 * umeta.get(uname, {}).get('umatched', 0)
                          / max(1, umeta.get(uname, {}).get('ubytes', 1))),
            src=src))
    rowsout.sort(key=lambda r: -r['asm'])

    # ---- report
    print()
    print('=== VEIN BY CLASS (row-level, precedence: fold-ambig > origin) ===')
    print(f'{"class":<16}{"units":>6}{"rows":>7}{"report B":>12}{"asm B":>12}'
          f'{"% asm":>8}{"% total_code":>14}')
    ucount = collections.Counter(r['cls'] if r['ambig_asm'] < r['asm'] else r['cls']
                                 for r in rowsout)
    for k in sorted(cls_asm, key=lambda k: -cls_asm[k]):
        print(f'{k:<16}{ucount.get(k, 0):>6}{cls_rows[k]:>7}{cls_rep[k]:>12,}'
              f'{cls_asm[k]:>12,}{100.0*cls_asm[k]/tot_asm:>7.1f}%'
              f'{100.0*cls_asm[k]/total_code:>13.2f}%')
    print(f'{"TOTAL":<16}{len(rowsout):>6}{tot_rows:>7}{tot_rep:>12,}{tot_asm:>12,}'
          f'{100.0:>7.1f}%{100.0*tot_asm/total_code:>13.2f}%')

    print()
    print('=== ORACLE STATE, within each fundable class (asm bytes) ===')
    grid = collections.Counter()
    gridu = collections.Counter()
    for r in rowsout:
        grid[(r['cls'], r['ostate'])] += r['asm'] - r['ambig_asm']
        gridu[(r['cls'], r['ostate'])] += 1
    for cls in ('B_BAND3', 'C_SYSTEM', 'A_QUAZAL', 'D_XDK', 'OTHER'):
        sub = {k: v for k, v in grid.items() if k[0] == cls}
        if not sub:
            continue
        print(f'  {cls}')
        for (c, st), v in sorted(sub.items(), key=lambda kv: -kv[1]):
            print(f'    {st:<34}{gridu[(c, st)]:>5} units {v:>10,} B')

    print()
    print('=== DEFICIT (retail code syms in pin - our base obj syms) ===')
    dv = collections.Counter()
    dvu = collections.Counter()
    for r in rowsout:
        k = ('UNWRITTEN (short of code)' if r['deficit'] > 0 else 'divergent/other')
        dv[k] += r['asm']
        dvu[k] += 1
    for k, v in sorted(dv.items(), key=lambda kv: -kv[1]):
        print(f'  {k:<30}{dvu[k]:>5} units {v:>10,} B  {100.0*v/tot_asm:5.1f}%')

    print()
    print(f'=== TOP {args.top} UNITS BY SIZE-IF-IT-CROSSES (asm extents) ===')
    print(f'{"unit":<46}{"asmB":>8}{"repB":>8}{"def":>6}{"m%":>6}  '
          f'{"oracle":<32}{"our/wii/dc3"}')
    for r in rowsout[:args.top]:
        print(f'{r["unit"][:46]:<46}{r["asm"]:>8,}{r["rep"]:>8,}{r["deficit"]:>+6d}'
              f'{r["unit_matched"]:>5.0f}%  {r["ostate"][:32]:<32}'
              f'{r["our_loc"]}/{r["wii_loc"]}/{r["dc3_loc"]}')

    if args.tsv:
        with open(args.tsv, 'w') as fh:
            cols = ('unit cls rows rep asm ambig_asm deficit tgtfn ourfn ostate '
                    'our_loc wii_loc dc3_loc unit_matched src').split()
            fh.write('\t'.join(cols) + '\n')
            for r in rowsout:
                fh.write('\t'.join(str(r[c]) for c in cols) + '\n')
        print(f'\nwrote {args.tsv}')

    if args.md:
        with open(args.md, 'w') as fh:
            fh.write('| unit | asm B | report B | deficit | unit matched | '
                     'oracle state | our/wii/dc3 LOC |\n')
            fh.write('|---|---:|---:|---:|---:|---|---|\n')
            for r in rowsout[:args.top]:
                fh.write(f'| `{r["unit"]}` | {r["asm"]:,} | {r["rep"]:,} | '
                         f'{r["deficit"]:+d} | {r["unit_matched"]:.0f}% | '
                         f'{r["ostate"]} | {r["our_loc"]}/{r["wii_loc"]}/'
                         f'{r["dc3_loc"]} |\n')
        print(f'wrote {args.md}')


if __name__ == '__main__':
    main()
