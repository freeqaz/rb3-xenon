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
   are relocations and are score-invisible under functionRelocDiffs=none, so
   including them would make two identical instructions look different.

USAGE
-----
    python3 tools/structural_decompose.py [--project-dir DIR] [--max-mismatch 3]
"""
import argparse, collections, os, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import crossing_worklist as C  # noqa: E402


def key(side):
    """opcode + register/immediate args.  Symbol args are relocations and are
    masked by functionRelocDiffs=none, so they must NOT participate."""
    if not side:
        return None
    args = tuple(str(a.get('value')) for a in (side.get('typed_args') or [])
                 if a.get('type') in ('Register', 'Signed', 'Unsigned', 'Opaque'))
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
        # shape 3: a degenerate one-label classifier must fail here.
        assert len(shapes_r) >= 3, f'only {len(shapes_r)} match_type shape(s) -- one-label classifier?'
        assert buckets['ALL'] > 0, 'no transposed rows -- transposition test is vacuous'
        assert buckets['NONE'] > 0, 'every row transposed -- transposition test is vacuous'
        assert 0 < pure < len(struct), 'insert/delete is all-or-nothing -- decomposition is vacuous'
        print("\nSELFTEST PASSED (>=3 shapes; both transposed and non-transposed "
              "populations non-empty; insert/delete is a proper subset)")


if __name__ == '__main__':
    main()
