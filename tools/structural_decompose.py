#!/usr/bin/env python3
"""
structural_decompose.py -- decompose crossing_worklist.py's STRUCTURAL class
into what its rows ACTUALLY are, and test the claim attached to that class.

WHY THIS EXISTS (lane DR-2, 2026-08-03)
=======================================
Lane DQ-3 shipped `tools/crossing_worklist.py`, whose top recommendation was:

    "STRUCTURAL at mm<=3 -- 132 rows / 17,212 B.  insert/delete/replace means a
     genuinely missing or extra instruction, i.e. real source divergence with a
     real fix.  Highest-count live class.  Fund."

Lane DR-2 funded it, and the class DOES pay -- but the stated reason is wrong
for the largest half of it, in a way that matters for how you work the rows.
`STRUCTURAL` is one label over THREE objdiff `match_type`s that mean different
things, and the tool never separated them:

  * `insert` / `delete` really are a missing or extra instruction.  Their rows
    are the ones where the handover's advice holds, and they are the ones that
    paid: GamePanel::Exit (3 `delete` = a missing ThePresenceMgr.SetNotInGame()
    call, +208 B) and CrowdAudio::SetTypeDef (3 `insert` = an `if (TypeDef() !=
    arr)` guard retail does not have, +748 B).
  * `replace` does NOT mean a missing or extra instruction.  objdiff emits it
    when target[i] and base[i] are paired at the SAME index with a different
    opcode -- nothing is missing, the lengths are equal.  It is the single
    biggest sub-class (67 of 132 rows at the measurement below), and it covers
    at least two unrelated causes: adjacent instruction TRANSPOSITIONS (pure
    scheduling; the same instructions in a different order) and genuine
    one-instruction divergences.

MEASURED at 2a48b057 (settled worktree build, report.json regenerated)
----------------------------------------------------------------------
STRUCTURAL-dominant rows with mm<=3: 132 rows / 17,212 B  (reproduces DQ-3)

  by match_type composition:
    replace only                67 rows  6,832 B   <- NOT "missing/extra"
    insert only                 22 rows  2,704 B
    delete only                 10 rows  1,404 B
    diff_arg+replace             9 rows  2,384 B
    delete+replace               6 rows  1,168 B
    diff_arg+insert              6 rows  1,756 B
    (7 further mixed shapes)    12 rows  1,000 B

  adjacent-transposition test (same instructions, different ORDER):
    ALL mismatches transposed   11 rows  2,552 B   <- scheduling, nothing missing
    SOME transposed              1 row     244 B
    NONE transposed            120 rows 14,416 B

Re-running this on the post-DR-2 tree reports 127 rows / 15,908 B, which is the
baseline above minus exactly the 5 rows / 1,304 B this lane crossed
(3 x 116 ObjRefConcrete + 208 GamePanel::Exit + 748 CrowdAudio::SetTypeDef).
That arithmetic closing to the byte is a free consistency check on both the
census and the claimed fixes -- if you change this tool and it stops closing,
suspect the tool.

So the honest restatement is: ~32 rows / ~4,108 B are literally insert/delete
("code is missing or extra", the productive shape), 11 rows are provably pure
reordering, and the `replace` bulk is a mixed bag that must be read
instruction-by-instruction rather than trusted as a class.

⚠ AND THE CLASS LABEL IS NOT WHERE THE VALUE WAS.  DR-2's biggest single win in
this class came from a `replace`-only row family (ObjRefConcrete<T>::~ObjRefConcrete
passing mOwner vs `this`, 3 rows), which the "missing or extra instruction"
framing would have deprioritised.  The lever that actually generalised was
"identical single-instruction mismatch repeated across template instantiations",
which cuts ACROSS match_type entirely.  Rank by repeated instruction SHAPE, not
by match_type.

INSTRUMENT DISCIPLINE (docs/decomp/INSTRUMENT_DESIGN.md)
--------------------------------------------------------
 * Reuses crossing_worklist's loader/classifier/diff-cache verbatim rather than
   reimplementing them, so this cannot drift from the census it is auditing --
   and so its `--selftest` inherits that tool's `$`-symbol control (shape 2).
 * shape 3 (one-label classifier): asserts >= 3 distinct match_type shapes and
   that BOTH the transposed and non-transposed populations are non-empty.  A
   degenerate classifier that called everything one thing fails loudly.
 * The transposition test compares opcode + non-symbol args only; symbol args
   are relocations, and including them would make two instructions that differ
   only by which instantiation they call look different.
   ⚠ NOT because they are unscored -- "score-invisible under
   functionRelocDiffs=none" was true when this was written and died on
   2026-08-12 (`d04c83df`, the name_check flip).  See `key()` below, which
   carries this and a second, larger defect inherited from this tool's sibling.

USAGE
-----
    python3 tools/structural_decompose.py [--project-dir DIR] [--max-mismatch 3]
"""
import argparse, collections, os, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import crossing_worklist as C  # noqa: E402


# Which typed_arg kinds participate in an instruction's identity.
#
# ★ BOTH DEFECTS THIS TUPLE CARRIED ARE FIXED (lane W4-F, 2026-09-11).  Until
# then it read ('Register','Signed','Unsigned','Opaque'), which its own sibling
# `tools/shape_families.py` had already refuted and fixed WITHOUT this module
# getting the change.  Measured ground truth: over 4,000 cached objdiff diffs the
# ONLY arg types objdiff emits are
#     Register  Symbol  Signed  BranchDest  Other  Unsigned
# and `Opaque` occurs EXACTLY ZERO TIMES.  So the old tuple silently discarded
# THREE live kinds, not one: `Other` (where shift and mask amounts live, e.g.
# {'type':'Other','value':'24'} on `clrlwi.`), `BranchDest` (a real scored
# intra-function branch target), and `Symbol` (relocations -- charged since
# `d04c83df` shipped name_check on 2026-08-12, and actually PRESENT in these
# diffs since `d1b4f708` put crossing_worklist on the graded ruler).
#
# WHY THAT MATTERED HERE, which is NOT how it mattered in shape_families.  This
# tuple feeds exactly one consumer: `transposed_pairs`, which asks "are these two
# adjacent mismatches literally the same two instructions, reordered?" and whose
# ALL-transposed bucket is labelled `scheduling, nothing missing` -- a DISMISSAL.
# Every discarded arg kind makes two DIFFERENT instructions compare EQUAL, so the
# blindness runs one way: it manufactures false transpositions and dismisses real
# divergences as scheduling noise.  A reordering that ALSO changes the callee --
# the `??__FsFrames` shape, retail destroying `ObjDirPtr<ObjectDir>` where we
# destroy `vector<RecordedFrame>` -- was read as pure scheduling.
ARGT = ('Register', 'Signed', 'Unsigned', 'Other', 'BranchDest', 'Symbol')


def key(side):
    """opcode + all scored args (see ARGT), symbol names LITERAL.

    Literal, not normalised, and for a different reason than in shape_families.
    That tool groups ACROSS rows, so it had to weigh fragmenting template
    instantiations.  This one compares two instructions WITHIN one row and asks
    whether they are the same instruction moved.  If retail's call goes to `Foo`
    and ours goes to `Bar`, they are not the same instruction and the pair is not
    a transposition -- so the literal name is the whole point.

    ⚠ DELIBERATE ASYMMETRY: a folded-alias callee (our spelling differs from
    retail's but both resolve to one function) will now read as a genuine
    divergence rather than as scheduling.  That is the safe direction -- it
    routes the row to a human instead of dismissing it -- and it is the direction
    CLAUDE.md asks for, since objdiff cannot separate `folded` from `wrong`."""
    if not side:
        return None
    args = tuple(str(a.get('value')) for a in (side.get('typed_args') or [])
                 if a.get('type') in ARGT)
    return (side.get('opcode'), args)


def transposed_pairs(diff):
    """Count adjacent i,i+1 mismatch pairs where target[i]==base[i+1] and
    target[i+1]==base[i] -- i.e. the same two instructions, reordered."""
    ins = diff.get('instructions') or []
    bad = [i for i, x in enumerate(ins) if x.get('match_type') not in (None, 'equal')]
    badset, used, n = set(bad), set(), 0
    for i in bad:
        j = i + 1
        if i in used or j in used or j not in badset:
            continue
        a, b = ins[i], ins[j]
        if key(a.get('target')) == key(b.get('base')) and \
           key(b.get('target')) == key(a.get('base')):
            n += 1
            used.update((i, j))
    return n, len(bad)


# ---------------------------------------------------------------------------
# FIXTURE CONTROL for the transposition detector (lane W4-F, 2026-09-11).
#
# WHY A FIXTURE AND NOT THE LIVE POPULATION.  `--selftest` used to assert
# `buckets['ALL'] > 0` -- "no transposed rows -- transposition test is vacuous".
# That asserts a property of the POPULATION, not of the tool, so it cannot tell
# a BROKEN detector from a population that simply has no transpositions left.
# It was already RED on arrival at `3ab3f494` for the second reason (0 ALL /
# 0 SOME / 79 NONE), i.e. the campaign draining the class made the tool's own
# control unsatisfiable.  A control that goes red when the work SUCCEEDS teaches
# a lane to ignore it.  These fixtures pin the detector itself, so the live
# counts can be reported as information instead of as a pass/fail.
def _side(op, args):
    return {'opcode': op, 'typed_args': [{'type': t, 'value': str(v)} for t, v in args]}


def _reordered(a, b):
    """A 2-instruction diff holding `a` then `b` on the target side and the same
    two, swapped, on the base side -- the exact shape `transposed_pairs` hunts."""
    return {'instructions': [
        {'match_type': 'replace', 'target': a[0], 'base': b[1]},
        {'match_type': 'replace', 'target': b[0], 'base': a[1]},
    ]}


def _fixtures():
    """(name, diff, expect_new, expect_old, why).

    `expect_old` is what the pre-W4-F key -- ('Register','Signed','Unsigned',
    'Opaque') -- scored, and it is the half that makes this a CONTROL rather than
    a restatement: every refusal fixture was a FALSE TRANSPOSITION before, so the
    fixtures demonstrate the behaviour CHANGED, in the intended direction."""
    lwz = (_side('lwz', [('Register', 'r4')]),) * 2
    f = []
    # ANTI-VACUITY ARM: a genuine register-only transposition must STILL be
    # detected.  Without this, a key that refused everything would "pass" each
    # refusal fixture below and look like a perfect fix.
    addi = (_side('addi', [('Register', 'r3')]),) * 2
    f.append(('genuine transposition (register-only)', _reordered(addi, lwz), 1, 1,
              'same two instructions, reordered -- really is scheduling'))
    # Symbol: a reorder that ALSO changes the callee is not scheduling.
    bl_t = _side('bl', [('Symbol', '??1?$ObjDirPtr@VObjectDir@@@@UAA@XZ')])
    bl_b = _side('bl', [('Symbol', '??1?$vector@URecordedFrame@@@@UAA@XZ')])
    f.append(('reorder + DIFFERENT callee (Symbol)', _reordered((bl_t, bl_b), lwz), 0, 1,
              'the ??__FsFrames shape: a wrong callee dismissed as scheduling'))
    # Other: shift/mask amounts live here.
    sh_t = _side('clrlwi.', [('Register', 'r3'), ('Other', '24')])
    sh_b = _side('clrlwi.', [('Register', 'r3'), ('Other', '16')])
    f.append(('reorder + DIFFERENT shift amount (Other)', _reordered((sh_t, sh_b), lwz), 0, 1,
              'a mask width is a real divergence, not a reordering'))
    # BranchDest: a real scored intra-function target.
    br_t = _side('beq', [('BranchDest', 500)])
    br_b = _side('beq', [('BranchDest', 132)])
    f.append(('reorder + DIFFERENT branch target (BranchDest)', _reordered((br_t, br_b), lwz), 0, 1,
              'a different branch destination is not a reordering'))
    return f


def _old_key(side):
    """The pre-W4-F key, kept ONLY so the fixtures can show what changed."""
    if not side:
        return None
    args = tuple(str(a.get('value')) for a in (side.get('typed_args') or [])
                 if a.get('type') in ('Register', 'Signed', 'Unsigned', 'Opaque'))
    return (side.get('opcode'), args)


def run_fixture_control(verbose=True):
    """Return (ok, lines).  Pins the detector against synthetic diffs."""
    global key
    lines, ok = [], True
    for name, diff, exp_new, exp_old, why in _fixtures():
        got_new = transposed_pairs(diff)[0]
        real, key = key, _old_key
        try:
            got_old = transposed_pairs(diff)[0]
        finally:
            key = real
        good = (got_new == exp_new and got_old == exp_old)
        ok &= good
        lines.append(f"  {'PASS' if good else 'FAIL'}  fixture: {name}\n"
                     f"           new key -> {got_new} transposition(s) (expect {exp_new}); "
                     f"old key -> {got_old} (expect {exp_old})   [{why}]")
    if verbose:
        print('\n'.join(lines))
    return ok, lines


def collect(project_dir, max_mm, cache_dir):
    measures, rows = C.load_rows(project_dir)
    diffs = C.diff_many(project_dir, rows, cache_dir, workers=8)
    for r, d in zip(rows, diffs):
        r['arms'] = C.classify_arms(d)
        r['mm'] = len(r['arms'])
        r['diff'] = d
    sel = [r for r in rows if 0 < r['mm'] <= max_mm]
    return [r for r in sel
            if collections.Counter(r['arms']).most_common(1)[0][0] == 'STRUCTURAL']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--project-dir', default=REPO)
    ap.add_argument('--max-mismatch', type=int, default=3)
    ap.add_argument('--cache-dir', default=os.path.expanduser('~/tmp/crossing_worklist/diffs'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    struct = collect(a.project_dir, a.max_mismatch, a.cache_dir)
    print(f"STRUCTURAL-dominant rows with mm<={a.max_mismatch}: "
          f"{len(struct)} rows / {sum(r['size'] for r in struct)} B\n")

    shapes_r, shapes_b = collections.Counter(), collections.Counter()
    for r in struct:
        s = tuple(sorted({x['match_type'] for x in (r['diff'].get('instructions') or [])
                          if x.get('match_type') not in (None, 'equal')}))
        shapes_r[s] += 1
        shapes_b[s] += r['size']
    print("=== by objdiff match_type composition ===")
    for k, v in shapes_r.most_common():
        print(f"  {'+'.join(k):<28} {v:4d} rows {shapes_b[k]:7d} B")

    buckets = collections.Counter()
    bbytes = collections.Counter()
    for r in struct:
        n, tot = transposed_pairs(r['diff'])
        cls = 'ALL' if (n and 2 * n == tot) else ('SOME' if n else 'NONE')
        buckets[cls] += 1
        bbytes[cls] += r['size']
    print("\n=== adjacent-transposition test (same instructions, different ORDER) ===")
    for cls, note in (('ALL', 'scheduling, nothing missing'),
                      ('SOME', 'mixed'),
                      ('NONE', 'genuine insert/delete/opcode')):
        print(f"  {cls:<5} transposed  {buckets[cls]:4d} rows {bbytes[cls]:7d} B   {note}")

    pure = sum(v for k, v in shapes_r.items() if set(k) <= {'insert', 'delete'})
    pureb = sum(v for k, v in shapes_b.items() if set(k) <= {'insert', 'delete'})
    print(f"\n=== literally 'a missing or extra instruction' (insert/delete only) ===")
    print(f"  {pure} rows / {pureb} B  of {len(struct)} rows / "
          f"{sum(r['size'] for r in struct)} B")

    if a.selftest:
        fails = []
        # shape 3: a degenerate one-label classifier must fail here.
        ok = len(shapes_r) >= 3
        print(f"\n  {'PASS' if ok else 'FAIL'}  control 1 (not a one-label classifier): "
              f"{len(shapes_r)} distinct match_type shapes")
        if not ok:
            fails.append(f'only {len(shapes_r)} match_type shape(s) -- one-label classifier?')

        ok = 0 < pure < len(struct)
        print(f"  {'PASS' if ok else 'FAIL'}  control 2 (insert/delete is a proper subset): "
              f"{pure}/{len(struct)} rows")
        if not ok:
            fails.append('insert/delete is all-or-nothing -- decomposition is vacuous')

        # control 3: the DETECTOR, pinned against synthetic diffs.
        #
        # ⚠ This replaces `assert buckets['ALL'] > 0`, which asserted a property
        # of the live POPULATION and so could not distinguish a broken detector
        # from a population with no transpositions left.  It was already failing
        # for the second reason.  The live buckets are still PRINTED above --
        # read them as information, not as a gate.
        print(f"  -- transposition detector, fixture-pinned "
              f"(live population: ALL={buckets['ALL']} SOME={buckets['SOME']} "
              f"NONE={buckets['NONE']}, not a gate) --")
        ok, _ = run_fixture_control()
        if not ok:
            fails.append('transposition detector fixtures failed -- key() or '
                         'transposed_pairs regressed')

        if fails:
            print('\nSELFTEST FAILED:')
            for f in fails:
                print(f'  - {f}')
            sys.exit(2)
        print("\nSELFTEST PASSED (>=3 match_type shapes; insert/delete a proper "
              "subset; transposition detector verified against fixtures)")


if __name__ == '__main__':
    main()
