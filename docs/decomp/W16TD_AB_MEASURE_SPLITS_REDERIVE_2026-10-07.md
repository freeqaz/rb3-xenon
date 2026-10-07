# W16-TD: `ab_measure` reports splits.txt re-derivation (2026-10-07)

Lane W16-TD. Worktree `~/tmp/wt-w16td`, branch `w16-td`, based on main
`f83a5d708`.

The brief: W16-SP (`W16SP_AB_MEASURE_CARVE_PATCHES_2026-10-07.md`) made
`tools/ab_measure.py` save `legB_symbols_fixed_point.diff` and print `LANDING`
when leg B's split settles on a different `symbols.txt`. The split also rewrites
`config/45410914/splits.txt`: every `.pdata` range is re-derived from the `.text`
block that owns its function. The tool recorded that sha (`legB_splits_sha`) and
said nothing about it. W16-TB measured a `.text` re-home correctly (+2 fns /
+84 B) and committed it (`10b3515dc`) without the four re-derived `.pdata` lines.
Main's next build stopped at the split-guard, and `f83a5d708` repaired it.

## 1. Change (`89259d2d6`, plus a docstring line in the record commit)

- **`splits_outcome()`** runs after leg B's fixed point, beside
  `symbols_outcome()`, for every split-forcing kind (map, splits, symbols).
  - `force_split()` now also records the splits.txt bytes the split consumes.
  - `converge_split()` records the sha it converged to.
- **`check_splits_fixed_point()`** (pure) returns `legA_start_is_fixed_point`,
  `patch_is_fixed_point` and `legB_fixed_point_equals_legA`. It **refuses** in one
  case: the patch's only applied file is splits.txt, and leg B's split wrote back
  exactly leg A's file. That is a `.pdata`-only edit, which the split re-derives
  away, so the A/B would compare absent with absent. If any other file is in the
  patch (map, `config.yml`, source), it does not refuse.
- When leg B's start is not its fixed point, the run saves two files:
  - `legB_splits_rederived.diff`: from leg B's start to its fixed point. These
    are exactly the lines to commit on top of the patch. They are checked with
    `git apply --check -R` against the file on disk.
  - `legB_splits_fixed_point.diff`: the same file diffed against HEAD.

  The summary then prints `LANDING`.
- **`apply_patch()`** restores the committed splits.txt before `git apply` if
  leg A's split re-derived it, and saves what it discarded. Leg B then starts
  from HEAD + patch, as `symbols.txt` already did.
- **Deliberately unchanged:** splits.txt is still not part of the convergence
  condition. dtk clears and re-derives the whole `.pdata` set from `.text` on
  every run. So the `.pdata` lines it reads never shape the carve, and the objs
  from the first split are already the fixed point's objs.

## 2. Selftest

`--selftest`: ALL PASS, with 8 new `[W16-TD]` checks. They cover:
- `patch_is_fixed_point` for TB as committed (false) and for TB + `f83a5d708`
  (true).
- The `.pdata`-only refusal, plus three shapes that must not refuse: with a map
  file, with `config.yml`, and a map-only patch.
- A refusal when the bookkeeping is missing.
- The landing diff, run through real `git apply` on a fixture shaped like TB. It
  gives the fixed point byte for byte. The reverse check accepts the fixed point
  and rejects the start.
- The wiring: `splits_outcome` is called after `converge_split("B")`,
  `converge_split` records the sha, and `apply_patch` restores before
  `git apply`.

**Each kind of check can fail.** Three sabotaged copies each turned exactly one
check red:
- `patch_is_fixed_point` forced to `True`;
- diff headers written without `a/`/`b/`;
- the `splits_outcome()` call removed.

## 3. Real-build control: reproducing W16-TB

The worktree was detached at `3d07cb6f2` (TB's parent). The committed tool blob
from `89259d2d6` was run from a copy:
`ab_measure.py --worktree ~/tmp/wt-w16td --pick 10b3515dc` (ruler `name_check`).

**Predicted:**
- +2 fns / +84 B (TB's own figure);
- leg A already a fixed point;
- leg B's split re-derives splits.txt by +3/−4 lines, equivalent to `f83a5d708`;
- `LANDING` printed.

**Measured** (run dir `~/tmp/w16td_runs/20261007-075303-c1-tb-pick-3461110`, copy
in `~/tmp/w16td_keep/`, rc=0). Every prediction held:

```
  [apply] patch applied; modified: ['config/45410914/splits.txt', 'scripts/target_symbol_map.json']
  [leg B] symbols.txt at SPLIT FIXED POINT after 0 extra forced re-split(s) (sha chain d60644c4 -> d60644c4); the first split was already its own fixed point
  [symbols] leg B's starting symbols.txt IS its split fixed point: the patch is complete as written.
  [splits] ⚠ PATCH IS NOT A SPLIT FIXED POINT: leg B's split re-derived splits.txt (+3/-4 lines; .pdata follows moved .text). Leg B was measured at the fixed point. To LAND it, commit the re-derived lines with the patch: `git apply .../legB_splits_rederived.diff` on top of it (verified: it reverse-applies to leg B's file); full file vs HEAD: .../legB_splits_fixed_point.diff
  [control none] Δmatched_code=+84 B Δcode%=+0.000817 (default ruler +84 B)
================ A/B RESULT (MEASURED) ================
  leg A: matched=54894 masked=25210 honest=29684 code%=59.239697  (recompiles: 0, settled)
  leg B: matched=54896 masked=25210 honest=29686 code%=59.240520  (recompiles: 0, split=1, patch_steps=7, settle iterations: 2)
  split fixed point: leg A converged after 0 extra re-split(s), leg B after 0 — BOTH read at a fixed point
  ⚠ LANDING: leg B's split RE-DERIVED splits.txt (+3/-4 lines, .pdata following moved .text); commit those lines with the patch (.../legB_splits_rederived.diff applies on top of it), or main's next build fails the split-guard
  Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.000823pp  Δcode_bytes=+84
  [tree] restored to the pre-run state (2 path(s), verified by re-reading the diff AND the untracked set)
```

`evidence.splits_fixed_point`:
- leg A start == fixed == `d22a52cc7c5a`;
- leg B start `37f6f7724871` → fixed `d37ee76473d1`;
- `legB_rederived_diff_reverse_applies: true`.

**Byte check.** Start from `3d07cb6f2`'s splits.txt. Apply `10b3515dc`'s
splits.txt hunks, then the saved `legB_splits_rederived.diff`. The result is
**byte-identical to `f83a5d708:config/45410914/splits.txt`** (sha256 prefix
`d37ee76473d1b887` on both sides). Its +/− body lines are identical to
`f83a5d708`'s diff. So the saved file is exactly the repair main needed, and it
was produced at measurement time.

The worktree was clean afterwards (`git status --porcelain` empty).

**Old tool on the same input: not re-run.** TB's own run already showed the
before state: a correct +2 / +84 B with no mention of splits.txt. The old code
path only wrote `legB_splits_sha` into the evidence.

## 4. Not done

- The `.pdata`-only refusal was exercised by the selftest only, not on a real
  build. The candidate is `--revert f83a5d708` on main, which should refuse at
  stage `splits-fixed-point`. I skipped it to stay within the single-build
  budget.
- No native gate: the lane touches no `src/` file.
- No merge, no push (coordinator).
