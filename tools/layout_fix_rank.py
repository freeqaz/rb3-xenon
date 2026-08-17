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

def dform_written_reg(side, base_reg):
    """The register a d-form load/store WRITES, when it is a frame register.

    objdiff-cli only started printing the parenthesised d-form in 4.2 (the
    `args` field was a flat comma-join from 2026-01-31 to 2026-08-16), so the
    paren branch of parse_mem_operand below was dead for six months and the
    `dest_reg=None` it returns was never exercised. Live, it lets
    `stwu r1, -0xa0(r1)` -- a frame ESTABLISH, the systematic false positive
    this module's header calls out by name -- past the FRAME_DEST_REGS guard
    and into the is_stack bucket.

    The naive repair ("dest_reg = typed_args[0]") is wrong: on a plain store
    the first operand is the value being READ, so `stw r31, 0x1c(r30)` -- an
    ordinary struct field write -- would be dropped as a frame establish. So
    resolve what the opcode actually writes:

      l..    (load)         -> operand 0 (the destination register)
      l..u   (load update)  -> operand 0 AND the base (write-back)
      st..   (store)        -> nothing
      st..u  (store update) -> the base only (write-back)

    Returns the written register that is in FRAME_DEST_REGS, else None.
    """
    if not side:
        return None
    op = (side.get("opcode") or "").strip().lower()
    if not op:
        return None
    written = set()
    is_store = op.startswith("st")
    if not is_store:
        ta = side.get("typed_args") or []
        if ta and ta[0].get("type") == "Register":
            written.add(str(ta[0].get("value")))
    if op.endswith("u"):          # stwu/stdu/stbu/lwzu/ldu/lfsu/... write back
        written.add(base_reg)
    hit = written & FRAME_DEST_REGS
    return sorted(hit)[0] if hit else None


def parse_mem_operand(args, side=None):
    """Return (imm:int, base_reg:str, dest_reg:str|None) for a `... imm(rBase)` or
    `rD, rBase, imm` operand, else (None, None, None). dest_reg is the register
    the instruction WRITES when that register is a frame register (r1/r31), i.e.
    the frame-establish tell; None otherwise. For the addi/subi form it is the
    literal destination operand; for the d-form it needs the opcode, so pass
    `side` (the objdiff instruction side dict) -- without it the d-form branch
    degrades to the old, guard-bypassing dest_reg=None."""
    if not args:
        return None, None, None
    # form: `r3, 0x1c(r30)` or `r3, 0x1c (r30)`  (load/store d-form)
    m = re.search(r'(-?0x[0-9A-Fa-f]+|-?\d+)\s*\(\s*(r\d+)\s*\)', args)
    if m:
        base = m.group(2)
        return int(m.group(1), 0), base, dform_written_reg(side, base)
    # form: `r3, r30, 0x1c`  (addi/subi rD, rBase, imm)
    # group(1) is BARE digits (`\br(\d+)`), group(2) already carries its `r`
    # (`(r\d+)`) -- the shipped code prefixed BOTH, so every base register out
    # of this branch was spelled `rr1`/`rr12`/`rr30`. Nothing in either repo
    # printed a base register, so it went unseen for the six months this was
    # the tool's ONLY live branch (the paren branch below was dead until
    # objdiff-cli 4.2). It silently disabled the STACK_REGS split: `rr1` and
    # `rr12` are not in STACK_REGS, so every addi/subi off the stack pointer
    # or off the r12 funclet establisher -- the case this module's header
    # names as THE SYSTEMATIC FALSE-POSITIVE -- was bucketed as a struct
    # field access.
    m = re.search(r'\br(\d+)\s*,\s*(r\d+)\s*,\s*(-?0x[0-9A-Fa-f]+|-?\d+)\b', args)
    if m:
        return int(m.group(3), 0), m.group(2), 'r' + m.group(1)
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
    # symbol tokens must match (else it's NAME_RELOC / WRONG_PAIR, not offset).
    # Read them from typed_args, NOT from the display string: objdiff-cli 4.2
    # drops a relocation that is not part of a printed operand, so a row that
    # differs ONLY in its relocation name now looks symbol-free on both sides
    # and would sail through this gate. (Its displayed form also carries an
    # `@h`/`@l` suffix that is presentation, not identity.)
    def syms(side):
        out = set()
        for a in (side or {}).get('typed_args') or []:
            if a.get('type') == 'Symbol':
                out.add(str(a.get('value')))
        return out
    if syms(tg) != syms(bs):
        return None
    ti, tb, td = parse_mem_operand(tg.get('args'), tg)
    bi, bb, bd = parse_mem_operand(bs.get('args'), bs)
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

# --- selftest ---------------------------------------------------------------
# EVERY fixture is a verbatim instruction side from live `objdiff-cli diff
# -f json --include-instructions` (4.2.3, 0fd82159607c) over rb3-xenon
# build/45410914 objects. The paren branch below was dead for six months while
# nothing noticed, so fixtures here must come from the CLI, not from memory.
_ST_STWU_FRAME = {"opcode": "stwu", "args": "r1, -0xb0(r1)", "typed_args": [
    {"type": "Register", "value": "r1"}, {"type": "Signed", "value": -176},
    {"type": "Register", "value": "r1"}]}
_ST_STW_R31_FIELD = {"opcode": "stw", "args": "r31, 0x8(r4)", "typed_args": [
    {"type": "Register", "value": "r31"}, {"type": "Signed", "value": 8},
    {"type": "Register", "value": "r4"}]}
_ST_LWZ_FIELD = {"opcode": "lwz", "args": "r10, 0x0(r11)", "typed_args": [
    {"type": "Register", "value": "r10"}, {"type": "Signed", "value": 0},
    {"type": "Register", "value": "r11"}]}
_ST_ADDI_FRAME = {"opcode": "addi", "args": "r3, r1, 0x50", "typed_args": [
    {"type": "Register", "value": "r3"}, {"type": "Register", "value": "r1"},
    {"type": "Signed", "value": 80}]}


def _selftest():
    checks = [
        # THE SYSTEMATIC FALSE POSITIVE: a frame establish must surface a
        # frame dest_reg so the FRAME_DEST_REGS guard can drop it.
        ("stwu frame establish -> dest r1",
         parse_mem_operand(_ST_STWU_FRAME["args"], _ST_STWU_FRAME),
         (-176, "r1", "r1")),
        # ...and the naive "dest_reg = typed_args[0]" repair must NOT drop
        # this: r31 here is the value being STORED, not a destination, so an
        # ordinary struct field write would vanish as a "frame establish".
        ("plain store of r31 to a field -> no frame dest",
         parse_mem_operand(_ST_STW_R31_FIELD["args"], _ST_STW_R31_FIELD),
         (8, "r4", None)),
        ("ordinary field load -> no frame dest",
         parse_mem_operand(_ST_LWZ_FIELD["args"], _ST_LWZ_FIELD),
         (0, "r11", None)),
        ("addi form still reports its literal dest",
         parse_mem_operand(_ST_ADDI_FRAME["args"], _ST_ADDI_FRAME),
         (80, "r1", "r3")),
    ]
    bad = 0
    for name, got, want in checks:
        if got != want:
            bad += 1
            print(f"FAIL {name}: want {want!r}, got {got!r}")

    # A row differing ONLY in its relocation name is NAME_RELOC, not a struct
    # offset delta. Under 4.2 neither relocation is printed, so this can only
    # be seen through typed_args.
    reloc_row = {
        "match_type": "diff_arg",
        "target": {"opcode": "lwz", "args": "r10, 0x4(r22)", "typed_args": [
            {"type": "Register", "value": "r10"},
            {"type": "Signed", "value": 4},
            {"type": "Register", "value": "r22"},
            {"type": "Symbol", "value": "lbl_82E05B3C"}]},
        "base": {"opcode": "lwz", "args": "r11, 0x0(r22)", "typed_args": [
            {"type": "Register", "value": "r11"},
            {"type": "Signed", "value": 0},
            {"type": "Register", "value": "r22"},
            {"type": "Symbol", "value": "?notInlinedSubDirs@@4V?$vector@@A"}]},
    }
    if insn_offset_delta(reloc_row) is not None:
        bad += 1
        print("FAIL relocation-name-only row scored as a struct offset delta: "
              f"{insn_offset_delta(reloc_row)!r}")

    # And the frame-establish row must contribute no delta at all.
    frame_row = {
        "match_type": "diff_arg",
        "target": _ST_STWU_FRAME,
        "base": {"opcode": "stwu", "args": "r1, -0xa0(r1)", "typed_args": [
            {"type": "Register", "value": "r1"},
            {"type": "Signed", "value": -160},
            {"type": "Register", "value": "r1"}]},
    }
    if insn_offset_delta(frame_row) is not None:
        bad += 1
        print("FAIL frame-size delta entered the offset buckets: "
              f"{insn_offset_delta(frame_row)!r}")

    total = len(checks) + 2
    print(f"selftest: {total - bad}/{total} passed")
    return 1 if bad else 0


if __name__ == '__main__':
    if '--selftest' in sys.argv:
        sys.exit(_selftest())
    main()
