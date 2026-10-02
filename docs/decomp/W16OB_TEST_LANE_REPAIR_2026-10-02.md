# W16-OB — `scripts/test_tools.py` back to green: five red arms, two stale entries (2026-10-02)

**Brief.** `python3 scripts/test_tools.py` reported FAIL on main (`871b44290`),
and the same failures already existed at `05e7507a3` (start of 2026-10-02). For
each failure, decide whether the test or the code it guards is wrong, fix that
side without weakening the check, and prove the repaired test can still fail.

**Result.** All five arms were **test-side** defects: fixtures and premises that
correct code changes had overtaken. **No guarded code was wrong.** The one
non-test source edit is a docstring in `tools/comdat_retail_verify.py` that
called a now-live branch "dead". The known-bad manifest is now **empty**.

| | measured on | verdict |
|---|---|---|
| lane, `--strict-manifest`, built tree, pre-rebase (`03172d701` on `871b44290`) | worktree, full `./tools/ninja-locked` rc=0 | `RESULT: PASS (new=0 timeout=0 broken=0 script-fail=0 uncovered=0 hollow=0 stale=0 known-bad-hit=0)`, rc=0 |
| same, **after rebase onto `dc741ff08`** (W16-NY: objects/splits/symbols/map) | `touch config.yml` + full build rc=0, settled (next build runs only the always-run check edges), `verify_objs_patched.py --verify-manifest` rc=0 | `RESULT: PASS`, 20/20 arms ok, rc=0 |
| mutation proofs (`docs/decomp/w16ob_mutation_proofs.py`) | both trees | 9/9 defects caught by the named test, 9/9 restores green, tree clean afterwards |

## 1. `scripts/test_patch_state.py` — 26 tests red (pytest showed "27 failed"; it counts subtests)

**Prediction:** one shared cause, because every failure was at the same control
line. **Measured:** every one returned **rc=7** at
`self.fx.run("--check", "--emit").returncode == 0`, with
`REFUSE(denylist): cannot read <tmp>/scripts/target_symbol_map.json`.

**Cause.** W16-AE's `97ef33f5c` (2026-09-14) added `check_denylist_applied()` to
both `run_check` and `verify_manifest`. When the map cannot be read it refuses
with exit 7, which is correct: a check that passes because its input is missing
is a vacuity. `test_patch_state.py`'s synthetic `Fixture` was last touched at
`1c24d134a` (09-11), predates that check, and supplied no map. So every test died
at its own control before reaching the property it was written for. (W16-AM
diagnosed this in `115a97769`'s message on 09-14 but left it unfixed.)

**Side that was wrong:** the fixture. **Fix:** the fixture writes
`scripts/target_symbol_map.json` = `{"_denylist": []}`. An empty denylist is used
because the fixture's target objs are fake bytes, not COFF, so any live
denylisted name would make the check refuse on parse. The denylist check's own
behaviour is already proved against real objs by `scripts/test_denylist_applied.py`.

**Why that alone would have weakened the suite, and the control added for it.**
An empty-denylist map lets the check pass with a NOTE, so deleting the check from
the verifier would leave all 26 tests green. **Measured:** with the check removed
from **both** call sites, the 26 original tests all pass; only the new test fails.
That test is `test_the_denylist_check_is_still_in_the_chain`. It plants a
malformed `_denylist` and requires rc=7 **and** the denylist's own refusal text on
both `--check` and `--verify-manifest`; repairing the map must then restore green.
One trap hit while writing it: the map is itself a recorded **build input**, so
changing it makes `--verify-manifest` answer **rc=6 BUILD OWED** before it ever
reaches the denylist. That is the right verdict, but not the one under test, so
the test re-emits (`--emit` alone does not run `--check`) after planting.

## 2. `tools/test_comdat_retail_verify.py::test_the_real_map_arbitrary_lists_do_not_overlap`

**Measured:** exactly one address is on both `_icf_arbitrary` (39) and
`_bijection_arbitrary` (975): **`0x826101b8`**, the
`hash_map<int,UIComponent*>::~hash_map` thunk.

**History:** laneAK `62098fc55` (07-26) put it on `_bijection_arbitrary` as a
`~map<int,UIComponent*>` pick. W16-AJ `7c80d49d` renamed it to the hash_map thunk.
W16-AE `b34c2fc1c` (09-14) added it to `_icf_arbitrary` as the Group B fold
survivor, because which of the folded spellings sits on the VA is not established.

**Side that was wrong:** the test's premise. The test only asserted that the
intersection is empty, but its own docstring says an overlap "is correct, not a
bug, but it should be noticed". W16-EX
(`docs/decomp/W16EX_TWO_INSTRUMENTS_THAT_LIE_2026-09-16.md`) already adjudicated
this exact double membership: both lists carry the same treat-as-unresolved
doctrine, and removing either entry "would have corrupted a provenance record". I
considered dropping the bijection entry, but rejected it for that reason and
because single-list consumers (`scripts/wrong_callee_triage.py:354`,
`scripts/harvest/dupname_identity_resolver.py`) would then classify it differently.

**Fix (not a weakening).** The test is renamed
`test_the_real_map_arbitrary_lists_overlap_only_where_adjudicated`. It pins
`ADJUDICATED_OVERLAP = {0x826101b8: <reason>}` **exactly**:

- any new, unadjudicated overlap still fails, as before;
- **and** the adjudicated one silently disappearing now fails too, which the old
  test could not express;
- it asserts the live map really produces the combined label
  `bijection_arbitrary+icf_arbitrary`, so `name_grain_index`'s both-lists branch
  is now exercised on real data;
- an in-test grow/shrink control proves the comparison can fail.

The docstrings that called this branch "dead today" (here and in
`tools/comdat_retail_verify.py::name_grain_index`) are corrected. Live grain
census on the current map: `name_pinned 32,865 · bijection_arbitrary 967 ·
icf_arbitrary 38 · bijection_arbitrary+icf_arbitrary 1`.

## 3. `tools/test_comdat_fold_gate_map_silent.py` — script mode rc=1

**Measured:** the two laundering checks failed. The fixture
`??3Loader@@SAXPAX@Z`, resubmitted with its address dropped against survivor
`??3BinStream@@SAXPAX@Z` @ `0x8240ddb0`, came back **ADMIT** (CF5).

**Cause.** The fixture's premise was "the map places `??3Loader` on a different
live body, `0x823f4698`". On **2026-10-01** `b9e50c1a2` and `da675aa5c` refuted
that row on retail bytes: `0x823f4698` is a `b` into the Quazal /Od block, not a
free, as already recorded in `_single_branch_thunk_misnames_comment`. Those
commits nulled the row and aliased `??3Loader` into the `0x8240ddb0` MemFree
survivor. The spelling became **genuinely map-silent and a proven fold**, so CF5
admitting it is the correct verdict.

**Side that was wrong:** the fixture. The gate is right.

**Fix.** A new laundering pair was found mechanically, not by hand. I resubmitted
every pair in the shipped worklist (`docs/plans/wrong-callee-triage-2026-08-12.json`)
with `base_addr: null` and kept those meeting all three conditions:

- refused with "DOES name";
- placed address different from the survivor's;
- the honest submission is refused by the chain as "retail has a DIFFERENT LIVE body".

**Two qualified:**

- `residual`: `resize<Key<vector<Color>>>` @ `0x82470738` vs `resize<Key<vector<Vector2>>>` placed @ `0x826c83e8` (**used**);
- `bijection_class`: `__destroy_range` / `_Destroy_Range<IKTarget>` @ `0x823977b0` / `0x82371148`.

⚠ A first, sloppier scan reported ~35 "DOES name" candidates. Most of them have
the folded spelling **placed at the survivor's own address**, because `byname` is
alias-closed. Those pairs do not reproduce the laundering property, so the
honest-leg filter was required, not optional.

To keep the next rot diagnosable, the test now has two extra checks:

- a **fixture precondition**: the map places F only at `0x826c83e8`, and the
  survivor row is intact. On failure it says "the FIXTURE is stale, not the gate".
- an **honest-submission control**: the same pair with its real address must be
  REFUSED as a different live body. This proves stage 1 passes, so the laundering
  REFUSE is stage 2's and not an unrelated stage-1 failure.

## 4. `tools/test_fold_thunk_gate_mask.py` — HOLLOW

All 11 assertions ran at **module level**, so the `tools` root imported the file
and collected **0 tests**: a passing run asked it nothing. **Fix:** the same 11
assertions, unchanged, as 5 pytest tests (one per property). A `__main__` runs
pytest on the file itself, which is W16-OA's pattern in
`tools/test_alias_group_key.py`. It is now collected by the `tools` root (5
tests), so it is not added to SCRIPT_ARM, which would run it twice.

## 5. The two stale known-bad entries

Both had been passing since **2026-09-14**. `--strict-manifest` makes a stale
entry fatal, which is why they surfaced as a FAIL.

- `test_prober.py::TestFormatProbeResult::test_format_input_sensitive`: fixed by
  `115a97769` (W16-AM), which tracked the formatter's `fills` → `inputs` label
  rename but did not prune this line.
- `test_comdat_retail_verify.py::test_the_real_map_populations_are_bound_as_floors`:
  measured by walking every map revision since 09-10 through `classify_map_rows`.
  The last red was `9314a27e7` (930 < 939) and the first green `3ffdbbf79` (967).
  The floor itself is unchanged.

Both lines are deleted, with dated tombstones; the manifest now has **0 entries**.

## 6. Mutation proofs (`docs/decomp/w16ob_mutation_proofs.py`)

Each defect is written into the real file and the named test is run. The file is
then restored with `git checkout`, the restore is verified byte-for-byte, and the
test is re-run green. The harness **mutates the checkout**, so it is NOT
registered in SCRIPT_ARM, whose entries promise never to write to the checkout.
Run it only in your own worktree.

| id | defect | caught by |
|---|---|---|
| PS-1 | `check_denylist_applied` dropped from `run_check` | `test_the_denylist_check_is_still_in_the_chain` (only that test) |
| PS-2 | … dropped from `verify_manifest` | same |
| PS-3 | verifier runs 5 of 6 patchers | `test_check_runs_every_patcher`, i.e. the 26 original tests now reach their assertions |
| RV-1 | an extra bijection address added to `_icf_arbitrary` in the live map | `…overlap_only_where_adjudicated` |
| RV-2 | `0x826101b8` removed from `_icf_arbitrary` | same |
| RV-3 | `name_grain_index` reverted to first-key-wins | same |
| MS-1 | CF5's map-present (laundering) refusal disabled | `[FAIL] map-RESIDENT spelling … (laundering)` |
| MS-2 | fixture's map row `0x826c83e8` renamed | `[FAIL] fixture precondition` |
| FT-1 | `mask_word` masks unrelocated words again (the pre-W16-EK vacuity) | `test_an_unrelocated_field_is_compared_whole` |

**9/9 RED, 9/9 restored GREEN**, on both the pre-rebase and the rebased tree.

## Deliberately not done

- `scripts/test_patch_guard_split_hook.py` stays EXCLUDED. Its listed fix (point
  it at a fixture) was not part of this brief.
- Nothing in the map, the alias file or any gate was changed. Every fix here is a
  fixture, a premise or a docstring.
- No metric A/B: no compiled source or map row changed, so there is nothing to price.

## Native gate (run last, on `246f6d1be` rebased onto `dc741ff08`)

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
