#!/usr/bin/env python3
"""
shape_families.py -- rank sub-100 rows by REPEATED INSTRUCTION SHAPE across rows,
which is the ranking lane DR-2 concluded actually predicts value, instead of by
objdiff's `match_type` (which DR-2 proved describes the PAIRING, not the defect).

WHY THIS EXISTS (lane DS-3, 2026-08-03)
=======================================
Lane DQ-3 ranked the sub-100 population by `match_type`, producing the class
`STRUCTURAL` (= insert|delete|replace) and the rationale "a genuinely missing or
extra instruction".  Lane DR-2 funded that class, crossed 5 rows out of it, and
REFUTED the rationale: `replace` does not mean anything is missing -- objdiff
emits it whenever target[i] and base[i] pair at the same index with a different
opcode.  DR-2's own biggest win came from a `replace`-only family that the
"missing instruction" framing had deprioritised, and it closed with:

    "The lever that actually generalised was `identical single-instruction
     mismatch repeated across template instantiations`, which cuts ACROSS
     match_type entirely.  Rank by repeated instruction SHAPE, not by match_type."

This tool is that ranking.  The unit of analysis is not the row and not the
match_type -- it is the SHAPE:

    shape = (match_type, target opcode + non-symbol args,
                         base   opcode + non-symbol args)

and a row's SIGNATURE is the sorted multiset of its shapes.  A FAMILY is a set
of >= 2 rows with an IDENTICAL signature.  That is the fingerprint of one source
defect replicated across instantiations: every member diverges from retail in
exactly the same way, at every mismatched instruction, so one source fix should
carry all of them.

WHY SIGNATURE-IDENTITY AND NOT SHAPE-FANOUT
-------------------------------------------
`matched_code` is ALL-OR-NOTHING PER ROW, so the yield of a fix is the size of
the rows that FULLY cross, never the number of sites it touches (fan-out is
blast radius, not yield -- feedback_site_count_is_not_defect_count).  A shape
appearing in 40 rows pays nothing if each of those rows has three OTHER
mismatches as well.  Signature-identity is the strict version: every member of a
family has NO other mismatch, so fixing the shared cause crosses the whole
family.  The tool reports shape-fanout too, but explicitly as blast radius, in a
column that is NOT the ranking key.

SYMBOL ARGS ARE EXCLUDED FROM THE SHAPE, DELIBERATELY
------------------------------------------------------
Under `functionRelocDiffs=none` a relocation argument is masked: a wrong callee
scores 100 and is invisible to the ruler.  Including symbol args in the shape
would split one real family into N singletons (each instantiation calls a
different per-T callee) and would manufacture differences the metric cannot see.
This is the same reason structural_decompose.py's `key()` drops them.

TWO GRANULARITIES, BOTH REPORTED
---------------------------------
  exact  -- registers kept.  Two rows group only if the same registers are
            involved.  Precise, but a regalloc difference splits a real family.
  regnorm-- registers renumbered in first-appearance order per instruction pair.
            Groups instantiations that differ only by register assignment.
The gap between the two counts is itself a measurement: if regnorm finds many
more families than exact, register assignment is fragmenting real families.

INSTRUMENT DISCIPLINE (docs/decomp/INSTRUMENT_DESIGN.md)
--------------------------------------------------------
 * Reuses crossing_worklist's loader + argv-only diff driver verbatim, so it
   cannot drift from the census it extends, and inherits its `$`-symbol
   protection (a shell here-string ate 204/461 template symbols with no error).
 * shape 3 (one-label classifier): --selftest asserts the signature function is
   not degenerate -- it must produce MANY distinct signatures, AND both
   multi-row families and singleton rows must exist.  A constant signature
   function (everything one family) and an injective one (no families at all)
   both fail loudly.
 * NULL CONTROL (--null): the finding "N families of size >= 2 exist" is
   meaningless without knowing how many a random grouping yields.  --null
   reshuffles which row carries which signature, keeping both marginals fixed,
   and reports the family count under permutation.  If the observed count is not
   well above the null, signature-identity is not detecting anything.
 * The ranking key is CROSSING BYTES (sum of member sizes), never site count.

USAGE
-----
    python3 tools/shape_families.py --selftest  [--project-dir DIR]
    python3 tools/shape_families.py --families  [--min-rows 2] [--max-mismatch 8]
    python3 tools/shape_families.py --null
    python3 tools/shape_families.py --vs-matchtype   # how this ranking differs
    python3 tools/shape_families.py --show SIGHASH   # dump one family's rows
"""
import argparse, collections, hashlib, json, os, random, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import crossing_worklist as C  # noqa: E402

# Which typed_arg kinds participate in a shape.
#
# ⚠ THIS LIST WAS WRONG IN THIS TOOL'S FIRST DRAFT AND THE SELFTEST PASSED ANYWAY.
# The first draft used ('Register','Signed','Unsigned','Opaque').  `Opaque` is NOT
# a type objdiff emits -- the real name is `Other`, and `Other` is where SHIFT AND
# MASK AMOUNTS live ({'type':'Other','value':'24'}).  `BranchDest` was excluded on
# the theory that it is masked, but functionRelocDiffs=none masks `Symbol` only; an
# intra-function branch destination is a REAL, SCORED difference.
#
# The effect was not a lost row here or there -- it INVENTED FAMILIES AT THE TOP OF
# THE RANKING.  A mismatch whose only difference was a shift amount or a branch
# target rendered IDENTICALLY on both sides, so N unrelated rows that merely each
# had one such mismatch collapsed into a single N-row "family": 15 rows of `beq cr6`
# ranked #2 by fan-out, plus `srawi`/`slwi` families, 20 rows / 2,016 B of pure
# artifact.  Nothing errored.  See CONTROL 5 below, which now makes this fail loudly.
ARGT = ('Register', 'Signed', 'Unsigned', 'Other', 'BranchDest')


def side_sig(side, regmap=None):
    """opcode + non-symbol args.  Symbol args are relocations => masked by
    functionRelocDiffs=none, so they must NOT participate (see header)."""
    if not side:
        return None
    out = []
    for a in (side.get('typed_args') or []):
        t = a.get('type')
        if t not in ARGT:
            continue
        v = str(a.get('value'))
        if t == 'Register' and regmap is not None:
            v = regmap.setdefault(v, f'%{len(regmap)}')
        out.append(v)
    return (side.get('opcode'), tuple(out))


def instr_shape(ins, regnorm):
    regmap = {} if regnorm else None
    return (ins.get('match_type'),
            side_sig(ins.get('target'), regmap),
            side_sig(ins.get('base'), regmap))


def row_signature(diff, regnorm):
    shapes = [instr_shape(i, regnorm) for i in (diff.get('instructions') or [])
              if i.get('match_type') not in (None, 'equal')]
    return tuple(sorted(map(repr, shapes))), shapes


def sighash(sig):
    return hashlib.md5(repr(sig).encode()).hexdigest()[:10]


def build(project_dir, cache_dir, regnorm, max_mm):
    M, rows = C.load_rows(project_dir)
    diffs = C.diff_many(project_dir, rows, os.path.join(cache_dir, 'diffs'), workers=8)
    C.ruler_gate(rows, diffs)
    keep = []
    for r, d in zip(rows, diffs):
        sig, shapes = row_signature(d, regnorm)
        if not sig or len(sig) > max_mm:
            continue
        r['sig'], r['shapes'], r['diff'] = sig, shapes, d
        r['arms'] = C.classify_arms(d)
        r['cls'] = collections.Counter(r['arms']).most_common(1)[0][0]
        keep.append(r)
    return M, keep


def group(rows):
    g = collections.defaultdict(list)
    for r in rows:
        g[r['sig']].append(r)
    return g


def fmt_shape(sh):
    mt, t, b = sh
    def s(x):
        return '-' if x is None else (x[0] + (' ' + ','.join(x[1]) if x[1] else ''))
    return f'{mt:<8} target[{s(t)}]  base[{s(b)}]'


def cmd_families(a):
    M, rows = build(a.project_dir, a.cache_dir, a.regnorm, a.max_mismatch)
    g = group(rows)
    fams = {k: v for k, v in g.items() if len(v) >= a.min_rows}
    fb = sum(sum(r['size'] for r in v) for v in fams.values())
    tot = sum(r['size'] for r in rows)
    print(f"named sub-100 rows with 1..{a.max_mismatch} mismatches: {len(rows)} rows / {tot:,} B")
    print(f"distinct signatures: {len(g)}   families (>= {a.min_rows} rows): {len(fams)}"
          f"   covering {sum(len(v) for v in fams.values())} rows / {fb:,} B "
          f"({100*fb/int(M['total_code']):.4f} pp of total_code)\n")

    # shape fan-out, reported as BLAST RADIUS -- deliberately not the rank key.
    fan = collections.Counter()
    for r in rows:
        for sh in set(map(repr, r['shapes'])):
            fan[sh] += 1

    print(f"=== families ranked by CROSSING BYTES (sum of member sizes) ===")
    print(f"{'sig':<11} {'rows':>4} {'bytes':>8} {'mm':>3}  {'match_types':<22} shapes / members")
    order = sorted(fams.items(), key=lambda kv: -sum(r['size'] for r in kv[1]))
    for sig, v in order[:a.top]:
        b = sum(r['size'] for r in v)
        mts = '+'.join(sorted({sh[0] for sh in v[0]['shapes']}))
        print(f"\n{sighash(sig):<11} {len(v):>4} {b:>8,} {len(sig):>3}  {mts:<22}")
        for sh in v[0]['shapes']:
            print(f"    shape  {fmt_shape(sh)}   [in {fan[repr(sh)]} rows total]")
        for r in sorted(v, key=lambda r: -r['size']):
            print(f"      {r['size']:>6} B  fz={r['fz']:>7.3f}  {r['unit'][:28]:<28} {r['sym'][:70]}")


def cmd_vs_matchtype(a):
    """How does ranking by shape-family differ from ranking by match_type?"""
    M, rows = build(a.project_dir, a.cache_dir, a.regnorm, a.max_mismatch)
    g = group(rows)
    fams = {k: v for k, v in g.items() if len(v) >= a.min_rows}
    print("=== does the shape ranking cut ACROSS match_type? ===")
    comp = collections.Counter()
    for sig, v in fams.items():
        mts = tuple(sorted({sh[0] for sh in v[0]['shapes']}))
        comp[mts] += 1
    for k, n in comp.most_common():
        print(f"  {'+'.join(k):<28} {n:>4} families")
    print("\n=== the top-10 shape families, placed in DQ-3's match_type ranking ===")
    order = sorted(fams.items(), key=lambda kv: -sum(r['size'] for r in kv[1]))[:10]
    for sig, v in order:
        b = sum(r['size'] for r in v)
        clss = collections.Counter(r['cls'] for r in v)
        mts = '+'.join(sorted({sh[0] for sh in v[0]['shapes']}))
        print(f"  {sighash(sig)}  {len(v):>3} rows {b:>7,} B  match_type={mts:<20} "
              f"DQ-3 class={dict(clss)}")
    print("\nA family whose match_type is `replace` or `diff_arg` would be ranked LOW or")
    print("EXCLUDED by the STRUCTURAL framing, yet is ranked by crossing bytes here.")


def cmd_null(a):
    """Is 'N families of size >= 2' more than random grouping would give?"""
    M, rows = build(a.project_dir, a.cache_dir, a.regnorm, a.max_mismatch)
    g = group(rows)
    obs_f = sum(1 for v in g.values() if len(v) >= a.min_rows)
    obs_r = sum(len(v) for v in g.values() if len(v) >= a.min_rows)
    obs_b = sum(sum(r['size'] for r in v) for v in g.values() if len(v) >= a.min_rows)

    # NULL: keep the multiset of signatures and the multiset of row sizes, but
    # break their association -- i.e. shuffle which row carries which size.
    # Family COUNT is invariant under that, so the informative null must instead
    # ask: how much of the family structure survives if signatures are rebuilt
    # from independently-sampled shapes (marginal preserved, joint destroyed)?
    pool = [repr(sh) for r in rows for sh in r['shapes']]
    rnd = random.Random(20260803)
    fc, rc, bc = [], [], []
    for _ in range(a.trials):
        gg = collections.defaultdict(list)
        for r in rows:
            fake = tuple(sorted(rnd.choice(pool) for _ in r['shapes']))
            gg[fake].append(r)
        fc.append(sum(1 for v in gg.values() if len(v) >= a.min_rows))
        rc.append(sum(len(v) for v in gg.values() if len(v) >= a.min_rows))
        bc.append(sum(sum(r['size'] for r in v) for v in gg.values() if len(v) >= a.min_rows))
    print(f"OBSERVED  families {obs_f:>5}   rows-in-families {obs_r:>5}   bytes {obs_b:>9,}")
    print(f"NULL(x{a.trials}) families {sum(fc)/len(fc):>7.1f} rows-in-families "
          f"{sum(rc)/len(rc):>7.1f}  bytes {sum(bc)/len(bc):>11,.0f}")
    e = obs_b / max(sum(bc) / len(bc), 1e-9)
    print(f"\nenrichment (bytes): {e:.2f}x   -- signature identity is detecting real")
    print("shared structure only if this is well above 1.")
    return e


def cmd_show(a):
    M, rows = build(a.project_dir, a.cache_dir, a.regnorm, a.max_mismatch)
    for sig, v in group(rows).items():
        if sighash(sig) != a.show:
            continue
        print(f"family {a.show}: {len(v)} rows / {sum(r['size'] for r in v):,} B")
        for sh in v[0]['shapes']:
            print(f"  shape  {fmt_shape(sh)}")
        for r in sorted(v, key=lambda r: -r['size']):
            print(f"\n  {r['size']:>6} B  fz={r['fz']:>7.3f}  {r['unit']}  {r['sym']}")
            for i in (r['diff'].get('instructions') or []):
                if i.get('match_type') in (None, 'equal'):
                    continue
                t, b = i.get('target') or {}, i.get('base') or {}
                print(f"      {i['match_type']:<8} T:{t.get('formatted','-'):<44} "
                      f"B:{b.get('formatted','-')}")
        return
    sys.exit(f'no family with hash {a.show}')


def cmd_selftest(a):
    fails = []
    M, rows = build(a.project_dir, a.cache_dir, a.regnorm, a.max_mismatch)
    g = group(rows)

    # control 1 (shape 2, inherited): a '$'-bearing template symbol must resolve.
    dollar = [r for r in rows if '$' in r['sym']]
    ok = len(dollar) > 0
    print(f"  {'PASS' if ok else 'FAIL'}  control 1 (template '$' symbols survive argv): n={len(dollar)}")
    if not ok:
        fails.append("no '$' symbols in population -- shell-quoting regression (shape 2)")

    # control 2 (shape 3): the signature function must be neither constant nor
    # injective.  A constant one puts everything in one family; an injective one
    # finds no families at all.  Both would be vacuous.
    nsig = len(g)
    fams = [v for v in g.values() if len(v) >= 2]
    singles = [v for v in g.values() if len(v) == 1]
    ok = nsig > 1 and len(fams) > 0 and len(singles) > 0
    print(f"  {'PASS' if ok else 'FAIL'}  control 2 (signature not constant, not injective): "
          f"{nsig} signatures, {len(fams)} families, {len(singles)} singletons")
    if not ok:
        fails.append('signature function is degenerate (constant or injective)')

    # control 3: the largest family must not be an artifact of an EMPTY shape --
    # e.g. a row whose only mismatch has no args on either side would collapse.
    biggest = max(g.values(), key=len)
    ok = len(biggest) < 0.25 * len(rows)
    print(f"  {'PASS' if ok else 'FAIL'}  control 3 (largest family is not the whole population): "
          f"{len(biggest)}/{len(rows)} rows")
    if not ok:
        fails.append('one family swallows the population -- signature is too coarse')

    # CONTROL 5 (the one that would have caught this tool's own first draft):
    # a `replace`/`diff_arg` shape whose target side and base side render
    # IDENTICALLY is proof the shape function is blind to whatever actually
    # differs on that instruction.  Such a shape groups rows that share nothing
    # but "has one mismatch of this opcode", which is not a family at all.
    blind = collections.Counter()
    for r in rows:
        for sh in r['shapes']:
            mt, t, b = sh
            if mt in ('insert', 'delete'):
                continue        # one side is legitimately absent
            if t is not None and t == b:
                blind[repr(sh)] += 1
    # A blind shape in ONE row cannot form a family, so it is harmless noise (it
    # is the residual Symbol-only `replace`, which the ruler masks anyway).  The
    # hazard is a blind shape spanning >= 2 rows: that MANUFACTURES a family out
    # of rows sharing nothing but an opcode.  Scope the failure to that.
    bad = {k: n for k, n in blind.items() if n >= a.min_rows}
    ok = not bad
    print(f"  {'PASS' if ok else 'FAIL'}  control 5 (no BLIND shape spans >= {a.min_rows} rows): "
          f"{len(bad)} family-forming blind shape(s); {len(blind)-len(bad)} harmless singletons")
    for k, n in sorted(bad.items(), key=lambda kv: -kv[1])[:5]:
        print(f"           blind: {k}  in {n} rows")
    if not ok:
        fails.append(f'{len(bad)} blind shapes span multiple rows -- the shape function '
                     f'cannot see what differs there, so those families are artifacts')

    # control 4 (shape 1, must-be-able-to-fail): the null must show enrichment.
    if not a.skip_null:
        e = cmd_null(argparse.Namespace(**{**vars(a), 'trials': 20}))
        ok = e > 1.5
        print(f"  {'PASS' if ok else 'FAIL'}  control 4 (enrichment over permutation null): {e:.2f}x")
        if not ok:
            fails.append(f'enrichment {e:.2f}x -- signature identity is not detecting structure')

    print()
    if fails:
        print('SELFTEST FAILED:')
        for f in fails:
            print('  -', f)
        sys.exit(2)
    print('SELFTEST PASSED')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=REPO)
    ap.add_argument('--cache-dir', default=os.path.expanduser('~/tmp/crossing_worklist'))
    ap.add_argument('--max-mismatch', type=int, default=8)
    ap.add_argument('--min-rows', type=int, default=2)
    ap.add_argument('--top', type=int, default=20)
    ap.add_argument('--trials', type=int, default=50)
    ap.add_argument('--regnorm', action='store_true',
                    help='renumber registers per instruction pair (coarser grouping)')
    ap.add_argument('--families', action='store_true')
    ap.add_argument('--vs-matchtype', action='store_true')
    ap.add_argument('--null', action='store_true')
    ap.add_argument('--show')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--skip-null', action='store_true')
    a = ap.parse_args()
    if not os.path.exists(C.report_path(a.project_dir)):
        sys.exit(f'REFUSE: no report.json under {a.project_dir}. Build first.')
    if a.selftest:
        cmd_selftest(a)
    elif a.null:
        cmd_null(a)
    elif a.vs_matchtype:
        cmd_vs_matchtype(a)
    elif a.show:
        cmd_show(a)
    elif a.families:
        cmd_families(a)
    else:
        ap.print_help()


if __name__ == '__main__':
    main()
