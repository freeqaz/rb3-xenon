#!/usr/bin/env python3
"""Honest "true progress" classifier for the near-miss pool.

Motivation (LTO/ICF investigation, 2026-06-06): the project owner suspected the
retail XEX was an LTCG/LTO build whose link-time optimization was hiding matches.
Forensics refuted that (no /GL, no /LTCG -- TU grouping preserved, cross-TU bl in
byte-exact matches, per-float lis). The ONLY link-time optimization present is
ICF (/OPT:ICF, on by default), which folds identical bodies to one address.

A prior analysis claimed ~625 functions were "byte-correct, blocked only by an
ICF/merged call target" and would flip to 100% under link-aware comparison. That
was WRONG: it conflated two things that objdiff's coarse classes lump together as
"a differing immediate":

  * NAME_RELOC  - a bl/lis/addi whose SYMBOL token differs. Subcategorised below.
  * STRUCT_OFF  - a raw load/store DISPLACEMENT differs (e.g. lwz r3,0x40 vs 0xf0)
                  with NO relocation. A real struct-layout bug. NOT recoverable by
                  linking -- this is the coupled-base-class layout grind.
  * FRAME_RECON - subi/addi against the frame regs (r31<-r12/r1) differ, plus the
                  stack-local addi shifts that cascade from a different frame size.
                  Often funclet-pairing noise (see engine_baseclass_layout_wall
                  memory); ambiguous, reported separately, NOT counted as progress.
  * REG / OPCODE / WRONG_PAIR - real codegen differences.

NAME_RELOC sub-classes (formerly all lumped as one bucket -- that conflation seeded
a false hypothesis that ~500 fns were "blocked only by data labels", probe of 771
fns in [99.9,100) showed 0% were data-label-only):

  BL_LBL_FUNCLET  : bl lbl_<addr> (branch to frameless EH funclet). Always
                    co-occurs with FRAME_RECON (subi r31,r12,N). Parent-gated;
                    resolves automatically as the enclosing function matches.
                    Probe: ~36.6% of [99.9,100) pool.
  ANON_FN_CALLEE  : bl fn_<addr> (call to still-anonymous function). Unblocks
                    once the callee TU is pinned/compiled.
                    Probe: ~13.9% of [99.9,100) pool.
  NAMED_MISMATCH  : bl to differently-named real symbols (both sides named).
                    Usually a wrong-pair / source-divergence artefact.
                    Probe: ~5.1% of [99.9,100) pool.
  DATA_LBL        : lis/addi/lwz with lbl_<addr> or __real@... (unnamed data
                    symbol or float literal pool). NEVER the sole blocker per
                    the probe (0% data-label-only fns); always co-occurs with
                    a real STRUCT_OFF or FRAME_RECON.

This tool re-runs objdiff per near-miss function, classifies every differing
instruction at this finer grain, and buckets each FUNCTION:

  HAS_REAL        - has STRUCT_OFF / REG / OPCODE residuals. These are the
                    genuine remaining work (struct-layout + codegen bugs).
                    Probe: ~42.8% of [99.9,100) -- ~330 fns.
  BL_LBL_FUNCLET  - only bl lbl_ + FRAME_RECON => EH-funclet wall; resolves
                    as parents match. NOT independently fixable.
  ANON_FN_CALLEE  - only bl fn_ (+ FRAME_RECON) => blocked on callee naming.
  NAMED_MISMATCH  - only named-vs-named symbol divergence.
  DATA_LBL        - only unnamed data symbol (rare; 0 pure instances in probe).
  FRAME_ONLY      - only FRAME_RECON with no branch label noise.
  RECOVERABLE     - legacy: only old NAME_RELOC (should now be empty / tiny).
  CLEAN           - no mismatches found (rounding artifact; effectively matched).

Output: per-bucket counts and an honest "true matched %" = official matched +
RECOVERABLE. Use --worklist to emit the HAS_REAL pool as a ranked JSON worklist.

Usage:
  tools/true_progress.py                          # [99,100) band
  tools/true_progress.py --lo 90 --hi 100         # wider band
  tools/true_progress.py --out /tmp/tp.json
  tools/true_progress.py --lo 99.9 --worklist ~/tmp/hasreal_worklist.json
"""
import sys, os, json, re, subprocess, argparse
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build/45410914/report.json')
CLI = os.path.join(ROOT, 'bin', 'objdiff-cli')

UNNAMED_RE = re.compile(r'^(fn_|lbl_|sub_|loc_)[0-9A-Fa-f]+$')
HEX_RE = re.compile(r'^-?0x[0-9A-Fa-f]+$|^-?\d+$')
REG_RE = re.compile(r'^[rf]\d+$|^cr\d+$|^r1$')
MEM_OPS = {'lwz', 'lbz', 'lhz', 'lha', 'lwzu', 'lbzu', 'lfs', 'lfd', 'lwa',
           'stw', 'stb', 'sth', 'stwu', 'stfs', 'stfd', 'lmw', 'stmw'}


def _tokens(s):
    return [t.strip() for t in (s or '').split(',') if t.strip()]


def _classify_insn_sub(ins):
    """Return a set of fine-grained sub-class strings for one differing instruction.

    Returns a set rather than a single string because one instruction can
    contribute to multiple sub-classes (e.g. a lwz with a data-symbol
    displacement also counts as DATA_LBL).
    """
    t = ins.get('target') or {}
    b = ins.get('base') or {}
    if not t or not b:
        return set()
    top = t.get('opcode', '')
    bop = b.get('opcode', '')
    ta = (t.get('args') or '').strip()
    ba_s = (b.get('args') or '').strip()
    tt = _tokens(ta)
    bt = _tokens(ba_s)

    if top != bop:
        return {'OPCODE'}
    if ta == ba_s:
        return set()

    # Branch instructions -- classify by target symbol type
    if top in ('bl', 'b', 'ba', 'bla'):
        if ta.startswith('lbl_'):
            return {'BL_LBL_FUNCLET'}
        elif UNNAMED_RE.match(ta):
            return {'ANON_FN'}
        else:
            # Both are named but differ, or base is anonymous
            b_unnamed = UNNAMED_RE.match(ba_s) if ba_s else False
            if b_unnamed:
                # base is anonymous, target is named -- old NAME_RELOC
                return {'ANON_FN'}
            return {'NAMED_MISMATCH'}

    # addi/subi immediate -- either FRAME_RECON or STRUCT_OFF
    if top in ('subi', 'addi', 'addic', 'addic.'):
        dest = tt[0] if tt else ''
        srcs = tt[1:]
        if dest in ('r31', 'r1') and any(s in ('r12', 'r1') for s in srcs):
            return {'FRAME_RECON'}
        else:
            return {'STRUCT_OFF'}

    # Memory ops -- differentiate offset delta vs data-symbol reference
    if top in MEM_OPS and ta != ba_s:
        result = set()
        tl = [x.strip() for x in ta.split(',')]
        bl2 = [x.strip() for x in ba_s.split(',')]
        for x, y in zip(tl, bl2):
            if x == y:
                continue
            if HEX_RE.match(x) and HEX_RE.match(y):
                result.add('STRUCT_OFF')
            elif 'lbl_' in x or '__real@' in x or 'lbl_' in y or '__real@' in y:
                result.add('DATA_LBL')
            elif REG_RE.match(x) and REG_RE.match(y):
                result.add('REG')
            else:
                result.add('REG')   # conservative
        return result or {'REG'}

    # Everything else: check for data symbols vs register diffs vs generic
    result = set()
    for x, y in zip(tt, bt):
        if x == y:
            continue
        if 'lbl_' in x or '__real@' in x or 'lbl_' in y or '__real@' in y:
            result.add('DATA_LBL')
        elif REG_RE.match(x) and REG_RE.match(y):
            result.add('REG')
        elif HEX_RE.match(x) and HEX_RE.match(y):
            result.add('STRUCT_OFF')
        else:
            result.add('REG')
    if not result and ta != ba_s:
        # length mismatch or unrecognised
        result.add('REG')
    return result


# Legacy classify_insn (single class) -- kept for backward compatibility with
# any callers that import this module directly.
def classify_insn(ins):
    """Return a single legacy class for one differing instruction (backward compat)."""
    sub = _classify_insn_sub(ins)
    # Map new sub-classes back to old classes
    if 'OPCODE' in sub:
        return 'OPCODE'
    if 'BL_LBL_FUNCLET' in sub:
        return 'NAME_RELOC'
    if 'ANON_FN' in sub:
        return 'NAME_RELOC'
    if 'NAMED_MISMATCH' in sub:
        return 'NAME_RELOC'
    if 'DATA_LBL' in sub:
        return 'NAME_RELOC'
    if 'FRAME_RECON' in sub:
        return 'FRAME_RECON'
    if 'STRUCT_OFF' in sub:
        return 'STRUCT_OFF'
    if 'REG' in sub:
        return 'REG'
    return None


def classify_fn_subs(instructions):
    """Classify all differing instructions in a function, returning aggregated sub-class set + counts."""
    sub_union = set()
    counts = Counter()
    for ins in instructions:
        if ins.get('match_type') not in ('diff_arg', 'replace', 'mismatch'):
            continue
        sub = _classify_insn_sub(ins)
        sub_union |= sub
        for s in sub:
            counts[s] += 1
    return sub_union, counts


def bucket_from_subs(sub_classes):
    """Assign a top-level bucket from the aggregated sub-class set for a function."""
    cs = set(sub_classes)
    cs.discard(None)
    if not cs:
        return 'CLEAN'

    # HAS_REAL = any genuine struct/codegen diff regardless of label noise
    if cs & {'STRUCT_OFF', 'OPCODE', 'REG'}:
        return 'HAS_REAL'

    # EH-funclet wall: bl lbl_ (+ frame recon). Parent-gated.
    if 'BL_LBL_FUNCLET' in cs:
        return 'BL_LBL_FUNCLET'

    # Anonymous callee -- unblocks when callee is named/compiled
    if 'ANON_FN' in cs:
        return 'ANON_FN_CALLEE'

    # Named-vs-named divergence
    if 'NAMED_MISMATCH' in cs:
        return 'NAMED_MISMATCH'

    # Data label only (rare; 0 pure instances in probe)
    if 'DATA_LBL' in cs:
        return 'DATA_LBL'

    # Frame reconstruction only
    if 'FRAME_RECON' in cs:
        return 'FRAME_ONLY'

    return 'OTHER'


# Legacy bucket() using old class names -- preserved for external callers
def bucket(classes):
    """Legacy bucket function (old class names). Use bucket_from_subs for new code."""
    cs = set(classes)
    cs.discard(None)
    if not cs:
        return 'CLEAN'
    if cs <= {'NAME_RELOC'}:
        return 'RECOVERABLE'
    if cs <= {'NAME_RELOC', 'FRAME_RECON'}:
        return 'FRAME_ONLY'
    if 'STRUCT_OFF' in cs:
        return 'STRUCT_WORK'
    if cs & {'OPCODE', 'REG', 'WRONG_PAIR', 'IMM_OTHER'}:
        return 'CODEGEN_WORK'
    return 'OTHER'


def diff_fn(unit, sym, tmp):
    r = subprocess.run(
        [CLI, 'diff', '-p', ROOT, '-u', unit, sym, '-f', 'json', '-o', tmp,
         '--include-instructions'],
        capture_output=True, text=True, timeout=120)
    try:
        return json.load(open(tmp))
    except Exception:
        return None


def collect_diff_samples(instructions, n=3):
    """Return up to n sample differing instruction pairs as compact strings."""
    samples = []
    for ins in instructions:
        if ins.get('match_type') not in ('diff_arg', 'replace', 'mismatch'):
            continue
        t = ins.get('target') or {}
        b = ins.get('base') or {}
        tgt_s = f"{t.get('opcode', '')} {t.get('args', '')}".strip()
        base_s = f"{b.get('opcode', '')} {b.get('args', '')}".strip()
        samples.append({'tgt': tgt_s, 'base': base_s})
        if len(samples) >= n:
            break
    return samples


def main():
    ap = argparse.ArgumentParser(
        description='Classify near-miss functions by diff cause and emit a HAS_REAL worklist.')
    ap.add_argument('--lo', type=float, default=99.0,
                    help='Lower bound (inclusive) of match-percent band (default 99.0)')
    ap.add_argument('--hi', type=float, default=100.0,
                    help='Upper bound (exclusive) of match-percent band (default 100.0)')
    ap.add_argument('--limit', type=int, default=0,
                    help='Cap number of functions processed (0 = no limit)')
    ap.add_argument('--report', default=REPORT,
                    help='Path to report.json')
    ap.add_argument('--out', default='/tmp/true_progress.json',
                    help='Output JSON with per-function rows and bucket counts')
    ap.add_argument('--worklist', default='',
                    help='If set, emit the HAS_REAL pool as a ranked worklist JSON')
    a = ap.parse_args()

    rep = json.load(open(a.report))
    official = rep['measures']['matched_functions']
    total = rep['measures']['total_functions']

    targets = []
    for unit in rep['units']:
        un = unit['name']
        for f in unit.get('functions', []):
            mp = f.get('match_percent_normalized', 0.0)
            if a.lo <= mp < a.hi:
                addr = f.get('address', '0')
                targets.append((un, f['name'], mp, int(f.get('size', 0)), str(addr)))
    targets.sort(key=lambda t: -t[3])
    if a.limit:
        targets = targets[:a.limit]
    print(f"classifying {len(targets)} fns in [{a.lo},{a.hi}) ...", file=sys.stderr)

    tmp = '/tmp/_tp_diff.json'
    rows = []
    buckets = Counter()
    # Sub-class counts across all functions for the summary
    sub_totals = Counter()

    for i, (un, sym, mp, sz, addr) in enumerate(targets):
        if i and i % 100 == 0:
            print(f"  {i}/{len(targets)}", file=sys.stderr)
        d = diff_fn(un, sym, tmp)
        if not d:
            buckets['DIFF_FAIL'] += 1
            continue
        instructions = d.get('instructions', [])
        sub_classes, sub_counts = classify_fn_subs(instructions)
        bk = bucket_from_subs(sub_classes)
        buckets[bk] += 1
        for sc in sub_classes:
            sub_totals[sc] += 1

        # Compute diff_count: number of non-equal instructions
        diff_count = sum(1 for ins in instructions
                         if ins.get('match_type') not in ('equal', None))

        row = {
            'unit': un,
            'sym': sym,
            'address': addr,
            'mp': mp,
            'size': sz,
            'bucket': bk,
            'sub_classes': sorted(sub_classes),
            'sub_counts': dict(sub_counts),
            'diff_count': diff_count,
        }
        # Collect sample diff pairs for HAS_REAL entries (useful for worklist)
        if bk == 'HAS_REAL':
            row['diff_samples'] = collect_diff_samples(instructions, n=3)
        rows.append(row)

    json.dump({'band': [a.lo, a.hi], 'buckets': dict(buckets),
               'sub_totals': dict(sub_totals), 'rows': rows},
              open(a.out, 'w'), indent=1)

    # Summary output
    recoverable = buckets.get('RECOVERABLE', 0) + buckets.get('CLEAN', 0)
    has_real = buckets.get('HAS_REAL', 0)
    print(f"\n=== TRUE-PROGRESS near-miss taxonomy  band=[{a.lo},{a.hi}) n={len(targets)} ===")
    print()
    print("  Bucket breakdown:")
    order = ['CLEAN', 'RECOVERABLE', 'HAS_REAL',
             'BL_LBL_FUNCLET', 'ANON_FN_CALLEE', 'NAMED_MISMATCH',
             'DATA_LBL', 'FRAME_ONLY', 'OTHER', 'DIFF_FAIL']
    for k in order:
        if buckets.get(k):
            pct = buckets[k] / len(targets) * 100 if targets else 0
            print(f"    {k:18s} {buckets[k]:5d}  ({pct:.1f}%)")
    print()
    print("  NAME_RELOC sub-class totals (instruction counts, not function counts):")
    for sc in ['BL_LBL_FUNCLET', 'ANON_FN', 'NAMED_MISMATCH', 'DATA_LBL']:
        if sub_totals.get(sc):
            print(f"    {sc:18s} {sub_totals[sc]:5d} insns")
    print()
    print(f"  official matched         {official:6d}  ({official/total*100:.3f}%)")
    print(f"  + naming/ICF-recoverable {recoverable:6d}  (legacy RECOVERABLE+CLEAN in this band)")
    print(f"  = honest ceiling-if-named{official+recoverable:6d}  ({(official+recoverable)/total*100:.3f}%)")
    print()
    print(f"  HAS_REAL = {has_real} fns -- genuine struct/reg/opcode work (not link artefacts)")
    print(f"  BL_LBL_FUNCLET = {buckets.get('BL_LBL_FUNCLET',0)} fns -- EH-funclet wall (parent-gated, NOT independently fixable)")
    print(f"  ANON_FN_CALLEE = {buckets.get('ANON_FN_CALLEE',0)} fns -- blocked on callee TU being named/compiled")
    print(f"  NAMED_MISMATCH = {buckets.get('NAMED_MISMATCH',0)} fns -- named-vs-named symbol divergence")
    print(f"\n  wrote {a.out}")

    # Optional worklist output
    if a.worklist:
        _emit_worklist(rows, a.worklist)


def _emit_worklist(rows, path):
    """Emit the HAS_REAL pool sorted by tractability (fewest diff instructions first,
    then smaller function size first).  Each entry includes unit, symbol, address,
    match%, size, sub-class counts, and sample diff pairs."""
    has_real = [r for r in rows if r['bucket'] == 'HAS_REAL']
    # Tractability sort: fewest diffs, then smallest size
    has_real.sort(key=lambda r: (r['diff_count'], r['size']))

    out = []
    for r in has_real:
        out.append({
            'unit': r['unit'],
            'sym': r['sym'],
            'address': r['address'],
            'mp': r['mp'],
            'size': r['size'],
            'diff_count': r['diff_count'],
            'sub_classes': r['sub_classes'],
            'sub_counts': r['sub_counts'],
            'diff_samples': r.get('diff_samples', []),
        })

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    json.dump({'count': len(out), 'entries': out}, open(path, 'w'), indent=1)

    print(f"\n=== HAS_REAL worklist: {len(out)} functions -> {path} ===")
    print(f"  (sorted by diff_count asc, then size asc)")
    top = out[:10]
    for e in top:
        sc = ','.join(e['sub_classes'])
        print(f"  {e['sym'][:55]:<55s}  mp={e['mp']:.2f}  sz={e['size']:5d}  diffs={e['diff_count']}  [{sc}]")


if __name__ == '__main__':
    main()
