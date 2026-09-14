# W16-AF — the Accomplishment comparator rotation: `0x825f32a8` identified, five families repaired

Lane W16-AF, 2026-09-14. Branch `w16-af`, based on main `0abb5e91`.
Commits: `81c0fdf9` (evidence + partition artifacts), `af053fe9` (the map + splits edit).

**Result: `0x825f32a8` = `??RGoalCmp@@QBA_NVSymbol@@0@Z`.** The rotation the brief handed me as
three rows is a permutation over **five comparator families x 15 STL algorithm instantiations**
spanning two physical bands. Repaired as one coherent edit: **+32 matched functions / +6,016 B**
measured against a pre-registered +5,948 B.

---

## 1. What the brief asked, and how the scope changed

W16-AD (`W16AD_UNDECIDED_MASKED_RETAIL_FOLD_WITNESS_643_2026-09-14.md` §5/§5.5) left three rows in
the `0x825f3xxx` band map-named `<AccomplishmentCmp>` while they call an **unnamed** comparator at
`0x825f32a8`. Its hypothesis was that these are `CampaignGoalsLeaderboardChoicePanel`'s `<GoalCmp>`
instantiations.

**The hypothesis is correct, and the blast radius is 18x larger than the brief assumed.** Once
`0x825f32a8` is identified, the same instrument that identifies it partitions the *whole* comparator
population — and the partition disagrees with the map on **56 rows**, not 3. Repairing only the three
briefed rows would have left 53 rows carrying names that the same evidence proves wrong, orphaned the
names freed by the three (a `<GoalCmp>` name vacated in `AccomplishmentPanel` has nowhere to go), and
regressed rows that are currently at 100 only because two errors cancel. So the edit is the whole
permutation. The brief explicitly directs this ("apply as ONE coherent map + splits edit").

## 2. Identifying `0x825f32a8` — six independent lines, all on retail bytes

The body is 176 B. I adjudicated on the retail image (`orig/45410914/band.exe`, read through
`scripts/wrong_callee_triage.Image`), not on the oracle.

1. **Body semantics.** It materialises a **function-local static** `Symbol("campaign_metascore")`
   behind an init guard at `0x82E00398`, then calls `AccomplishmentManager::GetAccomplishment` on
   both arguments and compares `[+0x80]` with `<`. The string `campaign_metascore` occurs in exactly
   one source file in either oracle: `CampaignGoalsLeaderboardChoicePanel.cpp`. No Accomplishment
   comparator references it at all.
2. **Fan-in shape.** 7 callers, every one an STL algorithm instantiation, all inside the CampaignGoals
   band. A comparator with a non-algorithm caller would not be a comparator.
3. **An independently-named caller.** One caller, `0x825f45f0`, was *already* map-named
   `__merge_without_buffer<GoalCmp>` before this lane touched anything. A `<GoalCmp>` algorithm
   calling `0x825f32a8` directly is the strongest single row of evidence, and it was pre-existing.
4. **Connected-component closure.** Taking retail's own `bl` edges and closing transitively from
   `0x825f32a8`: the component reaches **no other comparator**, and it contains
   `CampaignGoalsLeaderboardChoiceProvider::ctor`, whose source is
   `stable_sort(..., GoalCmp(TheAccomplishmentMgr))`. The component is a clean family.
5. **Size agreement across all 15 algorithms.** Every algorithm `.pdata` extent in that component
   equals the corresponding `<GoalCmp>` COMDAT size in our compiled object. 15/15.
6. **Charge pattern.** Before the edit, rows calling `0x825f32a8` read 100 *only* via objdiff's
   placeholder forgiveness (`is_placeholder_symbol_name` forgives an `fn_`-named destination). They
   were not matched; they were unbilled.

**Alternatives rejected.** `AccomplishmentCmp` / `AccomplishmentCategoryCmp` / `AccomplishmentGroupCmp`
are each refuted by (1) alone — all three dispatch through `GetAccomplishmentGroup` /
`GetAccomplishmentCategory` / `GetAccomplishment` with **no** `campaign_metascore` special case, and
none takes a function-local static. `GoalAlpaCmp` is refuted by size and by body (it is an alphabetical
`Symbol` comparison, 40 B, no manager dereference).

### 2.1 A correction to W16-AD

W16-AD §5.5 states that comparator `.pdata` extents "discriminate all three candidates by SIZE".
**That is false and I relied on it before checking.** `AccomplishmentCategoryCmp` and
`AccomplishmentGroupCmp` are **both 112 B** — the size test cannot separate them. Adjudicating them on
their lookup callee instead shows **the map had them swapped**:

| address | retail lookup callee | correct name | map said |
|---|---|---|---|
| `0x825f6f20` | `GetAccomplishmentGroup` | `AccomplishmentGroupCmp` | `AccomplishmentCategoryCmp` |
| `0x825f6f90` | `GetAccomplishmentCategory` | `AccomplishmentCategoryCmp` | `AccomplishmentGroupCmp` |
| `0x825f7000` | `GetAccomplishment` | `AccomplishmentCmp` | correct already |

That swap is the root of the 53 rows beyond the briefed three: every algorithm keyed on those two
comparators inherited the swap.

## 3. The instrument, and why it is self-validating

Family assignment is **not** a judgement call per row. Each row's family is fixed by topological
closure over retail's own `bl` edges, seeded only by the comparators whose identity is proven by
their lookup callee. The algorithm half of each name comes from the map (already correct everywhere)
or, where the map was silent, by elimination against the retail `.pdata` extent.

It self-validates four ways, and each of these could have failed:

- the closure yields **exactly five families of exactly 15 algorithms** — no leftovers, no row in two
  families (`problems: []`);
- every retail `.pdata` extent equals our compiled COMDAT size for the assigned `<Cmp>`;
- each family's one remaining non-algorithm member is the semantically right caller
  (`CampaignGoalsLeaderboardChoiceProvider::ctor` -> `GoalCmp`, `AccomplishmentGroupProvider::Update`
  -> `GroupCmp`, and so on);
- the 56 changes are a **true permutation** — 0 names freed without being reassigned.

Artifacts: `docs/decomp/W16AF_family_partition_2026-09-14.json`,
`docs/decomp/W16AF_leaf_census_2026-09-14.json` (committed in `81c0fdf9`).

**An instrument defect caught mid-lane.** My first algorithm assignment came out 74/75, with
`AccomplishmentCmp` missing its `__stable_sort_aux`. The cause was taking the size table from **our**
COMDATs, where `tools/coff_bodies_ext.py` bills the successor's EH funclet prefix into the span —
our `__stable_sort_aux` reads 156 B against retail's 116 B. This is the STLPORT-1 hazard
(`project_one_sided_instrument_error_invisible_to_two_sided_control_2026-08-16.md`) recurring
verbatim. Re-deriving sizes from **retail `.pdata` extents** gives 75/75. I did not "fix" the
mismatch by relaxing the match; the one-off was the instrument, exactly as that memory predicts.

## 4. The edit

`scripts/target_symbol_map.json`: **56 names changed, 13 of them previously unnamed.** Every
algorithm name is unchanged — only the comparator half rotated.

`config/45410914/splits.txt`: 6 `.text` blocks carved out, 8 added to
`band3/meta_band/CampaignGoalsLeaderboardChoicePanel.cpp`.

```
CARVED OUT
  TexLoadPanel.cpp         0x825F2210-0x825F32A8
  TourDescPanel.cpp        0x825F3378-0x825F33D0
  AccomplishmentPanel.cpp  0x825F3428-0x825F3480
  AccomplishmentPanel.cpp  0x825F3560-0x825F3640
  AccomplishmentPanel.cpp  0x825F3C0C-0x825F3C58 + 0x825F3D08-0x825F3DB8
  AccomplishmentPanel.cpp  0x825F4948-0x825F4CA8 + 0x825F4DF8-0x825F4F50 + 0x825F4FF8-0x825F50A0
ADDED to CampaignGoalsLeaderboardChoicePanel.cpp
  0x825F32A8-0x825F3378  0x825F33D0-0x825F3428  0x825F3480-0x825F3560  0x825F3640-0x825F3708
  0x825F3C58-0x825F3D08  0x825F3DB8-0x825F3F70  0x825F4CA8-0x825F4DF8  0x825F4F50-0x825F4FF8
```

**Why re-homing was mandatory, not cosmetic.** objdiff pairs target<->base **by name within a unit**.
`<GoalCmp>` is defined only by the `CampaignGoalsLeaderboardChoicePanel` object. Renaming those
addresses to `<GoalCmp>` while leaving them homed in `AccomplishmentPanel` would have made them
**permanently unpairable** — a correct name scoring 0% forever. Re-homing is the half that converts
the rename from a regression into a gain, and it is exactly the non-neutral case CLAUDE.md warns
about ("RE-HOMING an already-pinned address is NOT metric-neutral").

**Boundaries were chosen to keep each EH funclet with its body.** `0x825F3358` is the guard-rollback
handler for `??RGoalCmp`'s local static; `0x825F4FC4` and `0x825F506C` are `_Temporary_buffer`
cleanup funclets. Splitting a funclet from its body is how a carve silently costs bytes.

**`.pdata` was not hand-edited.** dtk re-derives it from the new `.text` blocks. The first build after
rewriting splits.txt failed the split-guard ("THE SPLIT REWROTE ITS OWN INPUT") — the documented
behaviour; I verified by `git diff` that every `.text` change was exactly mine, then retried to rc=0.

## 5. Predicted vs measured

Pre-registered **before** the edit: **+5,948 B, 33 rows gaining, 0 losing.**
Measured on a full build (rc=0), by `fuzzy==100` set-diff against `~/tmp/rows_w16af_base.json`:

```
CROSSED IN : 35 rows, 6196 B
FELL OUT   :  3 rows,  180 B
NET bytes  : +6016
  matched_functions      43319 -> 43351     delta +32
  matched_code           3990660 -> 3996676 delta +6016
  matched_code_percent   38.948635 -> 39.00735
  fuzzy_match_percent    49.524593 -> 49.54273
```

Prediction error **+68 B (1.1%)**.

### 5.1 The three "fall-outs" are not losses — they are the set-diff seeing a re-home

`rowset_snapshot.py` keys rows by `unit/name`, so a re-homed row **must** appear once on each side.
All three are at `fuzzy==100` right now, under their new unit:

| row | left | entered | fuzzy now |
|---|---|---|---|
| `??$__chunk_insertion_sort@...UGoalAlpaCmp@@...` (108 B) | `AccomplishmentPanel` | `CampaignGoals...Panel` | 100 |
| `fn_825F4FC4` (40 B) | `AccomplishmentPanel` | `CampaignGoals...Panel` | 100 |
| `fn_825F3358` (32 B) | `TexLoadPanel` | `CampaignGoals...Panel` | 100 |

So `6196 - 180 = 6016` is not a netting of a gain against a loss: **180 B is counted on both sides and
the real content is 32 rows / 6,016 B.** There are **zero** genuine regressions.

Two `<GoalAlpaCmp>` instantiations legitimately land in CampaignGoals rather than AccomplishmentPanel,
and this is correct rather than accidental: `CampaignGoalsLeaderboardChoicePanel.cpp:133` is
`std::stable_sort(syms.begin(), syms.end(), GoalAlpaCmp())`, so **both** TUs instantiate the family,
retail folded each COMDAT to one address, and those addresses fall in different carves. Both our
objects define the name, so pairability holds either way.

### 5.2 Where the bytes came from

Of the 6,016 B, the `<GoalCmp>` family newly pairing in CampaignGoals is the largest block
(`__merge_backward` 312 B, the provider ctor 292 B, `merge` 196 B, `__stable_sort_adaptive` 188 B,
`__merge_sort_loop` 172 B, `__inplace_stable_sort` 144 B, `__linear_insert` 128 B,
`__stable_sort_aux` 116 B, `__upper_bound`/`__lower_bound` 112 B each, `__unguarded_linear_insert`
84 B, `__insertion_sort` 80 B, `__chunk_insertion_sort` 108 B). The Category/Group un-swap supplies
the rest in `AccomplishmentPanel` (`__merge_adaptive` 3x408 B, `__merge_backward` 312 B,
`__merge_without_buffer` 304 B, four `__stable_sort_adaptive` at 188 B, and so on).

## 6. A source divergence found and deliberately NOT fixed

Our `GoalCmp::operator()` uses the **extern global** `campaign_metascore`; retail uses a
**function-local static** with its init guard at `0x82E00398`. That is a real 32 B divergence — ours
compiles to 144 B against retail's 176 B — and it is why `??RGoalCmp@@` itself does not reach 100 even
though it is now correctly named and homed.

I did not fix it. It is a `src/` edit, and the brief scopes this lane to map + splits; a source change
would also oblige a native gate run this lane otherwise does not need. **It is a clean, sized,
ready-to-take follow-up: change the global to a function-local static in
`src/band3/meta_band/CampaignGoalsLeaderboardChoicePanel.cpp` and expect ~+176 B.**

## 7. NOT done, with reasons

- **`??RGoalCmp@@` itself left at <100** — the local-static divergence above. Source work, out of lane
  scope. Sized and handed on.
- **`symbol_aliases.json` untouched.** No rename in this edit forced a membership change, so there is
  nothing to record and no `withdrawn`. Touching it beyond that is a brief-level Do-NOT.
- **The 43 remaining refuted memberships** — W16-AG owns them.
- **No other Accomplishment row opened** — brief-level Do-NOT.
- **jeff/objdiff not rebuilt.**
- **`build_edit.py` / `apply_edit.py` not committed** — they live in `~/tmp/w16af/`. They are
  single-use generators for this permutation, superseded by the two committed JSON artifacts which
  carry the reproducible *result*. Committing a one-shot script whose inputs no longer exist would be
  a maintenance liability, not tooling.
- **Native gate was NOT required** — this lane touched **no `src/` file and no shared header**: the entire diff
  is `scripts/target_symbol_map.json`, `config/45410914/splits.txt` and two new `docs/` JSON
  artifacts. The brief permits a map/splits-only lane to skip it provided that is stated; this is that
  statement. I ran it regardless; the verbatim PASS line is in §8.

## 8. Gates

Run in the brief's order, in the worktree, after the last edit:

```
./tools/ninja-locked                                    BUILD rc=0
python3 scripts/verify_ruler_agreement.py --check       rc=0
  OK  functionRelocDiffs  = name_check
  OK  combineDataSections = true
  OK  combineTextSections = true
  OK  ppc.calculatePoolRelocations = false
  OK: both objdiff-cli entry points resolve the same ruler.
python3 scripts/verify_objs_patched.py --verify-manifest  rc=0
  [patch-state] OK: 1213 decomp, 3115 target objects match
                2026-09-14T18:17:10Z (tree_sha256=0726cc01b3446889)
```

`tools/native_build_gate.sh` — the brief permits a map/splits-only lane to skip this, and this lane
changed no `src/` file and no shared header (§7). I ran it anyway as my last action, because a
measured line is worth more than an argument that one was unnecessary:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Lane-internal before/after, whole-binary, ruler `name_check`:

| | matched_functions | matched_code |
|---|---|---|
| before (`0abb5e91`) | 43,319 | 3,990,660 B |
| after (`af053fe9`) | **43,351** | **3,996,676 B** |
| delta | **+32** | **+6,016 B** |

`total_functions` 69,216 / `total_code` 10,245,956 B at measurement time.
