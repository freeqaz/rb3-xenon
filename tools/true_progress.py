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

  * NAME_RELOC  - a bl/lis/addi whose SYMBOL token differs (callee unnamed/ICF-
                  folded, or a function-local static named differently). Naming /
                  link artifact -- genuinely recoverable once the callee is named.
  * STRUCT_OFF  - a raw load/store DISPLACEMENT differs (e.g. lwz r3,0x40 vs 0xf0)
                  with NO relocation. A real struct-layout bug. NOT recoverable by
                  linking -- this is the coupled-base-class layout grind.
  * FRAME_RECON - subi/addi against the frame regs (r31<-r12/r1) differ, plus the
                  stack-local addi shifts that cascade from a different frame size.
                  Often funclet-pairing noise (see engine_baseclass_layout_wall
                  memory); ambiguous, reported separately, NOT counted as progress.
  * REG / OPCODE / WRONG_PAIR - real codegen differences.

This tool re-runs objdiff per near-miss function, classifies every differing
instruction at this finer grain, and buckets each FUNCTION by its WORST residual:

  RECOVERABLE  - only NAME_RELOC residuals  => would reach 100% once callees named
                 (the honest, verified ICF/naming lever -- expect TENS, not 625)
  FRAME_ONLY   - only NAME_RELOC + FRAME_RECON => likely at_limit/funclet noise
  STRUCT_WORK  - has STRUCT_OFF (the real layout wall)
  CODEGEN_WORK - has REG/OPCODE/WRONG_PAIR (permuter / wrong-source)

Output: the per-bucket counts and an honest "true matched %" = official matched +
RECOVERABLE. Usage:
  tools/true_progress.py                  # near band [99,100)
  tools/true_progress.py --lo 90 --hi 100 # wider
  tools/true_progress.py --out /tmp/tp.json
"""
import sys, os, json, re, subprocess, argparse
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPORT = os.path.join(ROOT, 'build/45410914/report.json')
CLI = os.path.join(ROOT, 'bin', 'objdiff-cli')

SYM_RE = re.compile(r'(\?|lbl_|fn_|sub_|loc_|_|[A-Za-z@])')           # a symbol token
UNNAMED_RE = re.compile(r'^(fn_|lbl_|sub_|loc_)[0-9A-Fa-f]+$')
HEX_RE = re.compile(r'^-?0x[0-9A-Fa-f]+$|^-?\d+$')
REG_RE = re.compile(r'^[rf]\d+$|^cr\d+$|^r1$')
MEM_OPS = {'lwz','lbz','lhz','lha','lwzu','lbzu','lfs','lfd','lwa',
           'stw','stb','sth','stwu','stfs','stfd','lmw','stmw'}
FRAME_REGS = {'r31', 'r1', 'r12'}


def tokens(s):
    return [t.strip() for t in (s or '').split(',') if t.strip()]


def is_sym(t):
    return bool(t) and not HEX_RE.match(t) and not REG_RE.match(t)


def classify_insn(ins):
    """Return a finer-grained class for one differing instruction, or None."""
    t = ins.get('target') or {}
    b = ins.get('base') or {}
    if not t or not b:
        return None
    to, bo = t.get('opcode'), b.get('opcode')
    ta = (t.get('args') or '').strip()
    ba = (b.get('args') or '').strip()
    if to != bo:
        return 'OPCODE'
    if ta == ba:
        return None
    tt, bt = tokens(ta), tokens(ba)
    # branch to a different target symbol -> callee naming / ICF fold
    if to in ('bl', 'b', 'ba', 'bla'):
        return 'NAME_RELOC'
    if len(tt) != len(bt):
        return 'WRONG_PAIR'
    # walk tokens; find the differing one and what kind it is
    cls = None
    for x, y in zip(tt, bt):
        if x == y:
            continue
        if is_sym(x) or is_sym(y):
            # a differing symbol token (function-local static, data label, etc.)
            cls = 'NAME_RELOC'
            break
        if REG_RE.match(x) and REG_RE.match(y):
            cls = 'REG'
            break
        if HEX_RE.match(x) and HEX_RE.match(y):
            # a differing immediate. frame vs struct vs other.
            if to in ('subi', 'addi', 'addic', 'addic.') and any(r in tt for r in ('r31', 'r12', 'r1')):
                cls = 'FRAME_RECON'
            elif to in MEM_OPS:
                cls = 'STRUCT_OFF'
            else:
                cls = 'IMM_OTHER'
            break
        cls = 'WRONG_PAIR'
        break
    return cls or 'WRONG_PAIR'


def diff_fn(unit, sym, tmp):
    r = subprocess.run(
        [CLI, 'diff', '-p', ROOT, '-u', unit, sym, '-f', 'json', '-o', tmp,
         '--include-instructions'],
        capture_output=True, text=True, timeout=120)
    try:
        return json.load(open(tmp))
    except Exception:
        return None


# how to bucket a function from its set of residual instruction-classes
def bucket(classes):
    cs = set(classes)
    cs.discard(None)
    if not cs:
        return 'CLEAN'  # rounding; effectively matched
    if cs <= {'NAME_RELOC'}:
        return 'RECOVERABLE'
    if cs <= {'NAME_RELOC', 'FRAME_RECON'}:
        return 'FRAME_ONLY'
    if 'STRUCT_OFF' in cs:
        return 'STRUCT_WORK'
    if cs & {'OPCODE', 'REG', 'WRONG_PAIR', 'IMM_OTHER'}:
        return 'CODEGEN_WORK'
    return 'OTHER'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lo', type=float, default=99.0)
    ap.add_argument('--hi', type=float, default=100.0)
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--report', default=REPORT)
    ap.add_argument('--out', default='/tmp/true_progress.json')
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
                targets.append((un, f['name'], mp, int(f.get('size', 0))))
    targets.sort(key=lambda t: -t[3])
    if a.limit:
        targets = targets[:a.limit]
    print(f"classifying {len(targets)} fns in [{a.lo},{a.hi}) ...", file=sys.stderr)

    tmp = '/tmp/_tp_diff.json'
    rows = []
    buckets = Counter()
    for i, (un, sym, mp, sz) in enumerate(targets):
        if i and i % 100 == 0:
            print(f"  {i}/{len(targets)}", file=sys.stderr)
        d = diff_fn(un, sym, tmp)
        if not d:
            buckets['DIFF_FAIL'] += 1
            continue
        cc = Counter()
        for ins in d.get('instructions', []):
            if ins.get('match_type') in ('diff_arg', 'replace', 'mismatch'):
                c = classify_insn(ins)
                if c:
                    cc[c] += 1
        bk = bucket(cc.keys())
        buckets[bk] += 1
        rows.append({'unit': un, 'sym': sym, 'mp': mp, 'size': sz,
                     'bucket': bk, 'counts': dict(cc)})

    json.dump({'band': [a.lo, a.hi], 'buckets': dict(buckets), 'rows': rows},
              open(a.out, 'w'), indent=1)

    recoverable = buckets.get('RECOVERABLE', 0) + buckets.get('CLEAN', 0)
    print(f"\n=== TRUE-PROGRESS near-miss taxonomy  band=[{a.lo},{a.hi}) n={len(targets)} ===")
    for k in ['CLEAN', 'RECOVERABLE', 'FRAME_ONLY', 'STRUCT_WORK', 'CODEGEN_WORK', 'OTHER', 'DIFF_FAIL']:
        if buckets.get(k):
            print(f"  {k:13s} {buckets[k]:5d}")
    print(f"\n  official matched         {official:6d}  ({official/total*100:.3f}%)")
    print(f"  + naming/ICF-recoverable {recoverable:6d}  (verified link/ICF lever in this band)")
    print(f"  = honest ceiling-if-named{official+recoverable:6d}  ({(official+recoverable)/total*100:.3f}%)")
    print(f"\n  STRUCT_WORK + CODEGEN_WORK = {buckets.get('STRUCT_WORK',0)+buckets.get('CODEGEN_WORK',0)} fns are REAL remaining work (not link artifacts)")
    print(f"  wrote {a.out}")


if __name__ == '__main__':
    main()
