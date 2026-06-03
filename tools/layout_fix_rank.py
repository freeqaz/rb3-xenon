#!/usr/bin/env python3
"""Rank candidate struct/base-class LAYOUT fixes by empirical fan-out.

The engine matching wall is offset-class: ~97% of near-miss functions differ from
retail only by struct-field-offset immediates (`lwz/stw/addi rX, off(rBase)` with a
wrong `off`). Those offsets cascade from a small set of shared base-class layout
divergences. This tool quantifies the cascade: for every near-miss function it
extracts the OFFSET-class immediate deltas, separates *struct* reads (base register
is an object pointer) from *stack* noise (base = r1/r31 = regalloc/permuter-class),
then aggregates by owning unit and by shared offset delta so we can SEE which struct
fix unblocks the most functions before touching a single header.

Output:
  - per-unit rollup: how many near-miss fns, their dominant struct-offset delta,
    whether the unit is "delta-coherent" (one delta dominates -> a clean layout bug).
  - global delta clusters: units sharing the same dominant struct delta -> the
    keystone candidates, ranked by total functions + bytes they would unblock.

This is analysis only; it never edits anything. Feed --out JSON to the mapping
agents so each works from the same empirical fan-out picture.
"""
import json, subprocess, sys, re, os, argparse
from collections import Counter, defaultdict

ROOT = "/home/free/code/milohax/rb3-xenon"
CLI = os.path.join(ROOT, "bin", "objdiff-cli")
UNNAMED = re.compile(r'\b(fn_[0-9A-Fa-f]+|lbl_[0-9A-Fa-f]+|sub_[0-9A-Fa-f]+|loc_[0-9A-Fa-f]+)\b')
# PPC base registers that mean "stack/frame", not "object pointer".
#  r1  = stack pointer
#  r31 = MSVC X360 frame-pointer alias (subi r31, r1, FRAMESIZE)
#  r12 = funclet/EH entry establisher (mflr r12; subi r31, r12, FRAMESIZE in cleanup
#        funclets). THIS WAS THE SYSTEMATIC FALSE-POSITIVE: a `subi r31, r12, N`
#        frame-reconstruct in an EH-cleanup funclet parses as base=r12 and, when the
#        parent frame size differs between target/base, shows a coherent "struct delta"
#        (BandDirector +32, Part -64, MidiParser -48 ... were all this). r12-based and
#        r31-writing computes are frame setup, never field access.
STACK_REGS = {"r1", "r31", "r12"}
FRAME_DEST_REGS = {"r1", "r31"}  # an addi/subi WRITING these is frame establish, not a field read

def diff_fn(unit, sym, tmp):
    r = subprocess.run(
        [CLI, 'diff', '-p', ROOT, '-u', unit, sym, '-f', 'json', '-o', tmp,
         '--include-instructions'],
        capture_output=True, text=True, timeout=120)
    try:
        return json.load(open(tmp))
    except Exception:
        return None

def parse_mem_operand(args):
    """Return (imm:int, base_reg:str, dest_reg:str|None) for a `... imm(rBase)` or
    `rD, rBase, imm` operand, else (None, None, None). dest_reg is set only for the
    addi/subi form (where writing r1/r31 = frame establish, not a field read)."""
    if not args:
        return None, None, None
    # form: `r3, 0x1c(r30)` or `r3, 0x1c (r30)`  (load/store: dest is not frame-relevant)
    m = re.search(r'(-?0x[0-9A-Fa-f]+|-?\d+)\s*\(\s*(r\d+)\s*\)', args)
    if m:
        return int(m.group(1), 0), m.group(2), None
    # form: `r3, r30, 0x1c`  (addi/subi rD, rBase, imm)
    m = re.search(r'\br(\d+)\s*,\s*(r\d+)\s*,\s*(-?0x[0-9A-Fa-f]+|-?\d+)\b', args)
    if m:
        return int(m.group(3), 0), 'r' + m.group(2), 'r' + m.group(1)
    return None, None, None

def insn_offset_delta(ins):
    """If this instruction is an OFFSET-class struct/stack diff, return
    (delta, base_reg, is_stack). Else None."""
    if ins.get('match_type') == 'equal':
        return None
    tg = ins.get('target'); bs = ins.get('base')
    if not tg or not bs:
        return None
    if tg.get('opcode') != bs.get('opcode'):
        return None
    # symbol tokens must match (else it's NAME_RELOC / WRONG_PAIR, not offset)
    def syms(a):
        out = set()
        for t in re.split(r'[,\s()]+', a or ''):
            t = t.strip()
            if not t or re.fullmatch(r'r\d+|f\d+|cr\d+|-?0x[0-9A-Fa-f]+|-?\d+', t):
                continue
            out.add(t)
        return out
    if syms(tg.get('args')) != syms(bs.get('args')):
        return None
    ti, tb, td = parse_mem_operand(tg.get('args'))
    bi, bb, bd = parse_mem_operand(bs.get('args'))
    if ti is None or bi is None or ti == bi:
        return None
    if tb != bb:
        return None  # base register itself differs -> not a clean offset delta
    # Frame establish (subi/addi writing r1/r31) is NOT a struct field read -> drop.
    if td in FRAME_DEST_REGS or bd in FRAME_DEST_REGS:
        return None
    is_stack = (tb in STACK_REGS)
    return (ti - bi, tb, is_stack)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lo", type=float, default=80.0)
    ap.add_argument("--hi", type=float, default=100.0)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--units", default="")
    ap.add_argument("--out", default="/home/free/tmp/layout_fix_rank.json")
    ap.add_argument("--tmp", default="/home/free/tmp/_lfr.json")
    a = ap.parse_args()

    rep = json.load(open(os.path.join(ROOT, 'build/45410914/report.json')))
    unit_filter = set(u.strip() for u in a.units.split(',') if u.strip())
    targets = []
    for unit in rep['units']:
        un = unit['name']
        if unit_filter and un not in unit_filter:
            continue
        for f in unit.get('functions', []):
            mp = f.get('match_percent_normalized', 0.0)
            if a.lo <= mp < a.hi:
                targets.append((un, f['name'], mp, int(f.get('size', 0))))
    targets.sort(key=lambda t: -t[3])
    if a.limit:
        targets = targets[:a.limit]
    print(f"ranking {len(targets)} fns in [{a.lo},{a.hi})", file=sys.stderr)

    per_fn = []
    for i, (un, sym, mp, sz) in enumerate(targets):
        d = diff_fn(un, sym, a.tmp)
        if not d:
            continue
        struct_deltas = Counter()
        stack_deltas = Counter()
        for ins in d.get('instructions', []):
            r = insn_offset_delta(ins)
            if not r:
                continue
            delta, base, is_stack = r
            (stack_deltas if is_stack else struct_deltas)[delta] += 1
        dom_struct = struct_deltas.most_common(1)[0][0] if struct_deltas else None
        per_fn.append({
            'unit': un, 'sym': sym, 'mp': round(mp, 3), 'size': sz,
            'struct_deltas': dict(struct_deltas),
            'stack_deltas': dict(stack_deltas),
            'dom_struct_delta': dom_struct,
            'n_struct': sum(struct_deltas.values()),
            'n_stack': sum(stack_deltas.values()),
        })
        if (i + 1) % 50 == 0:
            print(f"  {i+1}/{len(targets)}", file=sys.stderr)

    # --- per-unit rollup ---
    units = defaultdict(lambda: {'fns': [], 'delta_votes': Counter(), 'bytes': 0,
                                 'n_struct_fns': 0, 'n_stack_only': 0})
    for r in per_fn:
        U = units[r['unit']]
        U['fns'].append(r['sym'])
        U['bytes'] += r['size']
        if r['dom_struct_delta'] is not None:
            U['delta_votes'][r['dom_struct_delta']] += 1
            U['n_struct_fns'] += 1
        elif r['n_stack'] and not r['n_struct']:
            U['n_stack_only'] += 1
    unit_rows = []
    for un, U in units.items():
        dv = U['delta_votes']
        dom = dv.most_common(1)[0] if dv else (None, 0)
        coherent = bool(dv) and dom[1] >= max(2, 0.6 * U['n_struct_fns'])
        unit_rows.append({
            'unit': un, 'n_fns': len(U['fns']), 'n_struct_fns': U['n_struct_fns'],
            'n_stack_only': U['n_stack_only'], 'bytes': U['bytes'],
            'dom_delta': dom[0], 'dom_delta_votes': dom[1],
            'delta_votes': dict(dv), 'coherent': coherent,
        })
    unit_rows.sort(key=lambda r: (-r['n_struct_fns'], -r['n_fns']))

    # --- global delta clusters (keystone candidates) ---
    # group COHERENT units by their dominant struct delta
    clusters = defaultdict(lambda: {'units': [], 'n_fns': 0, 'bytes': 0})
    for r in unit_rows:
        if r['coherent'] and r['dom_delta'] is not None:
            C = clusters[r['dom_delta']]
            C['units'].append((r['unit'], r['dom_delta_votes']))
            C['n_fns'] += r['dom_delta_votes']
            C['bytes'] += r['bytes']
    cluster_rows = []
    for delta, C in clusters.items():
        cluster_rows.append({
            'delta': delta, 'n_units': len(C['units']), 'n_fns': C['n_fns'],
            'bytes': C['bytes'],
            'units': sorted(C['units'], key=lambda x: -x[1]),
        })
    cluster_rows.sort(key=lambda r: (-r['n_fns'], -r['n_units']))

    out = {'per_fn': per_fn, 'units': unit_rows, 'delta_clusters': cluster_rows}
    json.dump(out, open(a.out, 'w'), indent=1)

    print("\n=== TOP UNITS BY STRUCT-OFFSET NEAR-MISS COUNT ===", file=sys.stderr)
    print(f"{'unit':52s} {'fns':>4s} {'strct':>5s} {'stko':>4s} {'domΔ':>6s} {'vote':>4s} coh", file=sys.stderr)
    for r in unit_rows[:45]:
        dd = '' if r['dom_delta'] is None else f"{r['dom_delta']:+d}"
        print(f"{r['unit'][:52]:52s} {r['n_fns']:>4d} {r['n_struct_fns']:>5d} "
              f"{r['n_stack_only']:>4d} {dd:>6s} {r['dom_delta_votes']:>4d} "
              f"{'Y' if r['coherent'] else ''}", file=sys.stderr)

    print("\n=== GLOBAL DELTA CLUSTERS (keystone candidates) ===", file=sys.stderr)
    print(f"{'delta':>6s} {'units':>5s} {'fns':>4s} {'bytes':>7s}  top units", file=sys.stderr)
    for r in cluster_rows[:25]:
        tops = ", ".join(f"{u}({n})" for u, n in r['units'][:6])
        print(f"{r['delta']:+6d} {r['n_units']:>5d} {r['n_fns']:>4d} {r['bytes']:>7d}  {tops}",
              file=sys.stderr)
    print(f"\nwrote {a.out}", file=sys.stderr)

if __name__ == '__main__':
    main()
