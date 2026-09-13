# Pricing the −74, and finishing the alias hygiene — lane W9-D, 2026-09-13

Branch `w9-alias-hygiene`, worktree `~/tmp/wt-w9-d`, based on main `1d560c2b`
(`git merge-base --is-ancestor 1d560c2b HEAD` asserted before the first edit).

This lane executes lane W8-A's handoffs **H2** (the unpriced −74 event), **H4**
and **H5**. H1 — the star→clique union that manufactures
`FABRICATED_CLOSURE_NOT_PARTITION` — is explicitly **not** this lane's and was
not touched.

Baseline, after this lane's **first full build** (mandatory: a reflinked
worktree's target objs are pre-renamer, so every mangled-name lookup reads
"absent" until the renamer's pre-compile step has run, and any negative taken
before that is vacuous):

```
./tools/ninja-locked                                       EXIT=0
python3 scripts/verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3085 target objects match
  tree_sha256=66321266892c04e8                             rc=0
report.json  matched_functions 42,739 / matched_code 3,865,716 / 37.729187%
```

★ That baseline **reconciles with W8-A's** (42,738 / 3,865,216 / 37.724308% at
`2fc2552a`) exactly through the wave-8 merge that landed between us, whose own
ledger line records **+1 fn / +500 B**: 42,738 + 1 = 42,739 and
3,865,216 + 500 = 3,865,716. Two independently measured absolutes agreeing
through a third recorded delta is worth more than either alone.

## 0. Headline

**The −74 is −46,520 B if taken blind, and −808 B once adjudicated.** 69 of the
74 withdrawals were **wrong**, and the regeneration that would have dropped them
would have destroyed 45,712 B of *proven* forgiveness with no evidence behind the
loss.

| | memberships | Δmatched_code | Δmatched_functions | rows |
|---|---:|---:|---:|---:|
| the whole −74 event, priced | 74 | **−46,520 B** | −160 | 164 |
| …of it, withdrawal **STANDS** ⇒ enacted here | 9 | **−808 B** | −6 | 6 |
| …of it, withdrawal **REFUTED** ⇒ reversed here | 65 | **+0 B** (45,712 B protected) | — | — |

Net effect of the lane on the metric: **−808 B / −6 matched_functions**, taken
deliberately. Final state **42,733 / 3,864,908 / 37.721302%**.

Ledger self-consistency — the actual point of H2 — is closed:

```
withdrawn AND still live at the same group:          74 -> 0
...whose spelling is also another group's SURVIVOR:  10 -> 0
```

## 1. Testing the briefed figures first

Every figure W8-A handed over was re-derived before anything was built on it.
Three reproduced exactly and one did not:

| briefed | measured here | verdict |
|---|---|---|
| 74 live-and-withdrawn memberships | **74** | ✅ exact |
| class split 70 / 3 / 1 (`UNDER_PARTITIONED_ICF_CLOSURE` / `SURVIVOR_SIZE_MISMATCH` / `<none>`) | **70 / 3 / 1** | ✅ exact |
| 10 of them also a SURVIVOR elsewhere | **10** | ✅ exact |
| `0x8233c668` carries **8** `FIXPOINT_ROOT_DIFFERS` withdrawals (H5) | **11** | ⛔ wrong |

The 11 is not my count against W8-A's tool — it **is** W8-A's tool:
`alias_withdrawal_audit.py --map-coupled` prints eleven records at that address.

## 2. How the −74 was priced

`scripts/symbol_aliases.json` feeds `tools/gen_symbol_alias_map.py` →
`build/45410914/icf_aliases.map`, which objdiff reads at **report** time via
`objdiff.json`'s `map_file`. So an alias ablation is a map + report regeneration
with **zero compiles**, and the settling hazard that makes source A/Bs expensive
does not arise.

The prober is `tools/alias_forgiveness_audit.py`'s `leg()` construction reused
verbatim — restore-on-every-exit-path, and **refuses if ninja compiled anything**
— changed only to ablate a *named set* of `(address, spelling)` memberships
instead of all groups. That construction is licensed: it reproduced
ALIASAUDIT-1's full `ab_measure` figure (720,992 B / 6.985907 pp) to the byte.

Every reading below **RECONCILES**: the fallen rows' sizes sum exactly to the
measured `matched_code` delta, which is the prober's own refusal condition. The
whole-74 run was executed **twice** and returned identical numbers.

★ **All 74 are in addressed groups.** 51 of the 1,595 groups carry
`address: null` and are skipped outright by the map renderer (no address, no
bucket, no equivalence), so a membership in one of them would forgive nothing by
construction. None of the 74 is in that class — the exposure is entirely real.

## 3. The adjudication — 69 real, 3 fabricated, 2 unknowable

`tools/alias_membership_adjudicate.py --wt ~/tmp/wt-w9-d`, the layered L1..L5
retail-byte adjudicator, run on the fully built tree. Its anti-vacuity gate
passed — **27,193 mangled names among 69,414 target bodies** — so the renamer had
run and no verdict is the vacuous `absent` that a pre-renamer tree returns for
everything.

| retail-byte verdict | count | of which |
|---|---:|---|
| **PROVEN** | 69 | 65 flat `L1_T1`, **4 `L2_RECURSIVE` only** |
| **CONTRADICTED** | 3 | |
| **NEEDS_SOURCE** | 2 | body absent on our side |

The lane splits on the **evidence layer**, not on the `PROVEN` label, and that
split is the whole difference between an adjudication and a bulk edit.

### 3.1 The 3 CONTRADICTED — fabricated, and W7-D's rule says so before any byte

`0x8280af88`, `0x8280aed8`, `0x8280c480`: the `UILabel`/`LabelSort` sort helpers
`__unguarded_linear_insert`, `__median`, `__unguarded_partition`. Each body calls
its **comparator functor**, and the two sides name different ones:

```
retail names  ??RWidgetDrawSort@?A0x530db9db@@QBA_NPBVUIListWidget@@0@Z
we     name   ??RLabelSort@?A0x15e3583a@@QAA_NPAVUILabel@@0@Z
```

A **type-dependent relocation target** means retail kept the instantiations at
distinct addresses. That is W7-D's rule — *a family folds iff its relocation
targets are type-independent* — and it forecloses the fold before any byte is
read. **The withdrawal was right; these were fabricated.**

### 3.2 The 4 PROVEN-by-fixpoint — withdrawal stands

All four are in one group, `?_M_erase@?$vector@V?$ObjOwnerPtr@VWaypoint@@@@…` @
`0x822cb828`, folding `vector<ObjPtr<BandTrack>>`, `<ObjPtr<RndGroup>>`,
`<ObjPtr<RndPartLauncher>>`, `<ObjPtr<RndPropAnim>>`. Every one carries
`why: "ICF fixpoint via chase"` — name equality closed under the very fold class
under test. **That is exactly the circularity `FIXPOINT_ROOT_DIFFERS` is named
for**, and a fixpoint cannot outrank the recorded
`UNDER_PARTITIONED_ICF_CLOSURE` refutation. Flat `L1_T1` does **not** prove them.
Withdrawal stands; cost **−512 B / −4 fns**, all four rows being the
corresponding `?resize@?$vector@V?$ObjPtr@…` instantiations (128 B each).

### 3.3 The 2 NEEDS_SOURCE — free to enact, explicitly reversible

`??0StoreArtLoaderPanel@@QAA@XZ` @ `0x827b7800` and
`?insert@?$list@VTargetCache@BandCamShot@@…` @ `0x822b1f90`. Our build has no
body, so no retail-byte evidence can license them, and they are **measured to
forgive 0 B**.

⚠ This is the one place the lane runs *near* a standing prohibition, so it is
flagged rather than buried. CLAUDE.md warns: *do not prune classes that
currently forgive 0, because they become live as porting advances, and a prior
prune cost +94,616 B to reverse.* What that forbids is **deleting records**;
nothing is deleted here. The withdrawal records are kept in full and annotated,
so if source lands and the fold is then proven on retail bytes, W8-A's override
mechanism re-admits them by naming the record. The alternative — leaving them
live — asserts a fold that cannot be proven, which is the integrity hazard this
whole campaign exists to suppress.

### 3.4 The 65 reversed — why the original withdrawal was wrong

All 65 are `L1_T1`: flat retail-byte identity **with relocation target names
compared**, the strongest non-circular layer available.

They came from the **ALIAS-REPAIR 2026-08-19** partition sweep, whose predicate —
quoted from the groups' own `repair.why` — resolves operands *"through the ICF
congruence over our own build"*. But **ICF happened in retail's link**, so that
predicate is unsound in precisely this direction. It is the same objection
already on record against the star→clique union (revert `760cb450`, H1). Retail
bytes outrank an our-build predicate.

★ Corroboration for the 3 `SURVIVOR_SIZE_MISMATCH` members among them — the
`?OnMsg@BandUI@@` `ButtonDownMsg` / `ButtonUpMsg` / `UIComponentFocusChangeMsg`
overloads, all `L1_T1`. That class is **already on record as a reader artifact**:
STLPORT-1 showed `tools/coff_bodies_ext.py` was billing the successor symbol's EH
funclet prefix into the COMDAT span, and GROUNDED-2 restored 6 of 8 size-based
withdrawals for that reason. This is the same artifact caught the same way — an
independent instrument arriving at a conclusion this project already reached by
another route.

★ All **10** invariant-breakers (spelling is also another group's survivor) are
`PROVEN`. So the survivor collision is a **partitioning** artifact — the same
real fold class split across two groups — not a fold defect. Merging those groups
is H1's job, and this lane deliberately did not attempt it.

## 4. The row list

### 4.1 The 6 rows the lane actually gives up (the enacted 9)

| bytes | unit | function |
|---:|---|---|
| 188 | `default/UIListDir` | `??$__introsort_loop@PAPAVUILabel@@PAV1@HULabelSort@?A0x15e3583a@@@stlpmtx_std@@…` |
| 128 | `default/TrackPanelDir` | `?resize@?$vector@V?$ObjPtr@VBandTrack@@@@…` |
| 128 | `default/BandCrowdMeter` | `?resize@?$vector@V?$ObjPtr@VRndGroup@@@@…` |
| 128 | `default/StreakMeter` | `?resize@?$vector@V?$ObjPtr@VRndPartLauncher@@@@…` |
| 128 | `default/StreakMeter` | `?resize@?$vector@V?$ObjPtr@VRndPropAnim@@@@…` |
| 108 | `default/UIListDir` | `??$__final_insertion_sort@PAPAVUILabel@@ULabelSort@?A0x15e3583a@@@stlpmtx_std@@…` |
| **808** | 4 units | **6 rows** |

Unit attribution measured independently by `ab_measure`: `StreakMeter` −2,
`UIListDir` −2, `BandCrowdMeter` −1, `TrackPanelDir` −1 — sum −6, matching the
whole-binary Δ.

### 4.2 The 158 rows the reversal PROTECTS (45,712 B)

Measured directly against the post-lane tree, not by subtraction:
**−45,712 B / −154 matched_functions / 158 rows, RECONCILES.** Top 20 of the
164-row full −74 set (the 6 above are the remainder):

| bytes | unit | function |
|---:|---|---|
| 3,564 | `default/BandUI` | `?Handle@BandUI@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 2,020 | `default/GemTrackDir` | `?SyncObjects@GemTrackDir@@UAAXXZ` |
| 1,900 | `default/StoreOffer` | `?Handle@StoreOffer@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 1,444 | `default/Task` | `?Handle@TaskMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 1,432 | `default/band3/game/Player` | `?LocalSetEnabledState@Player@@UAAX…@Z` |
| 1,080 | `default/GuitarFx` | `?Poll@GuitarFx@@QAAXH_N0MMM00@Z` |
| 984 | `default/band3/game/RGTrainerPanel` | `?UpdateStepText@RGTrainerPanel@@QAAX…@Z` |
| 840 | `default/SongSectionController` | `?UpdateOverlay@SongSectionController@@QAAXXZ` |
| 820 | `default/MainHubMessageProvider` | `?SetMessageLabel@MainHubMessageProvider@@QBAX…@Z` |
| 796 | `default/AppLabel` | `?SetBattleTimeLeft@AppLabel@@QAAXH@Z` |
| 708 | `default/system/rndobj/Utl` | `?OnTestDrawGroups@@YA?AVDataNode@@PAVDataArray@@@Z` |
| 696 | `default/band3/meta_band/AccomplishmentManager` | `?Cleanup@AccomplishmentManager@@QAAXXZ` |
| 684 | `default/MeterDisplay` | `?UpdateDisplay@MeterDisplay@@IAAXXZ` |
| 652 | `default/BandDirector` | `?OnMidiShotCategory@BandDirector@@QAA…@Z` |
| 648 | `default/CharBoneDir` | `?Init@CharBoneDir@@SAXXZ` |
| 632 | `default/system/bandobj/BandTrack` | `?SetInstrument@BandTrack@@UAAXW4TrackInstrument@@@Z` |
| 628 | `default/MemTracker` | `?DiffTblReport@@YAXPBDAAVBlockStatTable@@1AAVTextStream@@@Z` |
| 592 | `default/MetaPerformer` | `??1MetaPerformer@@UAA@XZ` |
| 560 | `default/system/synth/Synth` | `?SetFX@Synth@@QAAXPBVDataArray@@@Z` |
| 544 | `default/AppLabel` | `?SetSongAndArtistNameFromSymbol@AppLabel@@QAAXVSymbol@@H@Z` |

Full sets: `~/tmp/w9d/fell74.json` (164 rows) and `~/tmp/w9d/fell65.json` (158).

## 5. Predicted vs measured

| change | commit | predicted | measured | agree |
|---|---|---|---|---|
| **A** — enact the 9 standing withdrawals | `c491da7c` | −808 B / −6 fns / 6 rows | **−808 B / −6 fns**, units StreakMeter −2, UIListDir −2, BandCrowdMeter −1, TrackPanelDir −1 | ✅ exact, both keys and attribution |
| **B** — reverse the 65 refuted withdrawals | `7984276c` | **Δ0 exactly** | Δ0; rendered map **byte-identical** (`8c0e67a0a4a6d7a0…444c`, `cmp` clean); report 42,733 / 3,864,908 / 37.721302% unmoved | ✅ |
| **C** — H4 annotation | `ab040d34` | **Δ0 exactly** | Δ0; rendered map still byte-identical; report unmoved | ✅ |

Change A was measured with `python3 tools/ab_measure.py --worktree ~/tmp/wt-w9-d
--from-dirty`, one change in the run, both legs settled with 0 recompiles.

★ **B and C were deliberately NOT put through `ab_measure`,** and that is the
stronger statement, not a weaker one. `gen_symbol_alias_map.py` reads only
`survivor` + `folded`; both are untouched by B and C, so the artifact the build
consumes **cannot** move, and this is verifiable by rendering the map from each
ledger and running `cmp`. An `ab_measure` run over an input that did not change
is the absent-vs-absent reading the tool exists to refuse. Same argument W8-A
made with `tree_sha256`.

⚠ **On the `none`-ruler control for change A.** `ab_measure` printed
`[control none] FLAT: none UNMOVED`. That is expected and proves nothing — the
`none` ruler ignores relocation names, so it reads flat over any alias change by
construction, and that flatness is the fabrication hazard's *signature*, not a
clearance. It is recorded here only so nobody later reads its absence as a gap.
The licence for change A is retail bytes: two different comparator functors.

## 6. H4 — `0x824afa78` folds a spelling into ITSELF

**Verdict: a SURVIVOR-RENAME SHADOW, not a degenerate generator input** (W8-A's
guess). Fixed by annotation; commit `ab040d34`.

The group was emitted with survivor `ObjDirItr<Object>` and
`ObjDirItr<SpotlightDrawer>` folded into it. The **S2-CONTAINER map repair** then
split the two instantiations across distinct addresses and the group's `survivor`
field was rewritten to the map's name *at this address* — which is the folded
spelling. The withdrawal records name the folded side and were never rewritten,
so survivor and spelling collapsed onto one symbol. Verified against the map
rather than inferred:

```
0x824afa78 -> ??E?$ObjDirItr@VSpotlightDrawer@@@@QAAAAV0@XZ    (== the survivor)
0x823587b0 -> ??E?$ObjDirItr@VObject@Hmx@@@@QAAAAV0@XZ
```

Two distinct retail addresses — which is what `DISTINCT_ADDRESSES_CANNOT_FOLD`
records — and S2-CONTAINER's retail-byte argument stands independently:
`ObjDirItr<T>::operator++` must call `Advance<T>`, and `Advance<T>` loads a
per-`T` `??_R0` RTTI descriptor naming the filter type in plain text. Another
type-dependent relocation target, another W7-D foreclosure.

**Impact: inert, measured.** `folded` is empty, so the renderer emits a single
symbol at that address, and a one-symbol bucket is not an equivalence — the group
forgives **0 B**.

**Why annotation and not deletion.** There is nowhere to re-home the record to:
**there is no group at `0x823587b0` at all** (checked). And the residual denial
is *protective*, not merely harmless — keyed `(address, spelling)` it stops a
future regeneration proposing `ObjDirItr<SpotlightDrawer>` as a **folded**
spelling at `0x824afa78`, where S2-CONTAINER proved it is the **survivor**.
Deleting the records to tidy a cosmetic symptom would discard the only surviving
trace of that adjudication.

## 7. H5 — `0x8233c668` is NOT a map lane

**Verdict: nothing is owed, to anyone. The address is CLOSED.** W8-A called it
"a map lane, not an alias one"; it is neither.

* **The count is 11, not 8** — `alias_withdrawal_audit.py --map-coupled` itself
  prints eleven `FIXPOINT_ROOT_DIFFERS` records at that address.
* **It is "map-coupled" by LABEL, not by evidence.** `MAP_COUPLED` in
  `tools/alias_withdrawal_audit.py` is a **hardcoded set of class names** and
  `FIXPOINT_ROOT_DIFFERS` is simply a member of it. Nothing about this address
  was measured into that bucket.
* **The recorded cause is retail bytes, not a map identification.** Lane
  W41-CONTRA's `withdrawn_reason` says the `_Rb_tree` parent bodies are
  byte-identical modulo relocations, but *every differing relocation names the
  member's own instantiation sibling* (`_M_create_node` / `_M_insert` /
  `insert_unique`) — a fixpoint that holds only if you assume the folds you are
  trying to prove. That is the identical objection this lane applied to the 4
  `L2_RECURSIVE` memberships in §3.2, and it is a per-`T` callee, so W7-D's rule
  forecloses it.
* **No map work is owed.** The map names
  `insert_unique<_Rb_tree<int, pair<const int,bool>>>` at `0x8233c668`, that name
  is **unique to that address**, and it is one of the withdrawn spellings — i.e.
  the repair already picked the right instantiation out of the set the group had
  conflated. The group's `survivor` still spells the `pair<const int,float>`
  instantiation, so `map != survivor`, which is the state **W8-A's own §6 calls
  healthy**: the repair landed and the survivor field is the stale, refuted name.
* **No alias work is owed.** `folded` is empty: the group forgives **0 B**.

Nothing was changed at this address.

## 8. Gates

Run after every alias edit — `touch config/45410914/config.yml`, full build,
then all three:

```
./tools/ninja-locked                                            EXIT=0
python3 tools/icf_alias_finder.py --validate
  VALIDATE: PASS -- 1357 map-consistent, 236 tolerated, 0 contradicted, 1595 total   rc=0
python3 scripts/verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3085 target objects match
  tree_sha256=66321266892c04e8                                  rc=0
```

`tree_sha256` is **unchanged across the whole lane** — expected, and worth
stating: the alias ledger is a *report-time* input, so it moves `report.json`
without moving a single object byte.

Guard discrimination for change B, with a control that can fail:

```
of the 65 reversed: denied by the guard AFTER        = 0    (want 0)
of the  9 enacted : denied by the guard AFTER        = 9    (want 9)
of the 65 reversed: denied by the PRE-change ledger  = 65   (the test CAN fail, and does)
ledger records 9,993, above alias_withdrawals.MIN_RECORDS = 5,000 (anti-vacuity floor intact)
```

Ledger accounting: groups 1,595 (unchanged) · live memberships 5,351 → **5,342**
(−9) · withdrawal records 10,058 → **9,993** (−65, all *moved*, none deleted) ·
`restored` records 6 → **71**.

★ The `restored[]` convention was **verified against the file, not assumed**: all
6 pre-existing `restored` records (GROUNDED-2) are live and carry **no** surviving
`withdrawn` record. So restoring means *moving* the record. Annotating around it
would not have worked — the guard reads `withdrawn`, and `0x824afa78` is the
standing proof: it carries a RELOC-RECONCILE `restore` **and** a later
withdrawal, and the withdrawal wins.

## 9. Corrections to the record

1. **W8-A H5's "8 `FIXPOINT_ROOT_DIFFERS` withdrawals" at `0x8233c668` is wrong;
   it is 11** — per W8-A's own tool.
2. **W8-A H4's "whatever produced that record had a degenerate input" is wrong.**
   The input was fine; a later map repair rewrote the record's survivor context
   (§6).
3. ⚠ **This lane's own commit `7984276c` mis-states two figures in its message.**
   It says the reversal protects "−152 matched_functions across 156 rows". Those
   were obtained by *subtracting* the enacted 9 from the whole-74 measurement.
   Measured directly afterwards against the post-lane tree, the true figures are
   **−154 matched_functions across 158 rows**; the byte figure (45,712 B) was
   right. The commit is left standing and corrected here rather than amended —
   and it is a clean small instance of the rule the lane is built on: *a figure
   obtained by arithmetic on two other figures is not a measurement.*

## 10. What this lane did NOT do

* **Did not touch H1.** The star→clique union at the survivor key, which
  manufactures `FABRICATED_CLOSURE_NOT_PARTITION`, is deliberately unfixed
  (revert `760cb450`). Out of scope by instruction. Note §3.4's finding is
  *evidence for* H1 being the real cause: all 10 invariant-breakers are PROVEN
  folds, i.e. one real class split across two groups.
* **Did not merge the 10 collided groups.** That is the partition repair, which
  is H1's, and doing it here would be the unproven bulk edit this campaign keeps
  refusing.
* **Did not touch any map row**, including `0x8233c668` and `0x824afa78`.
* **Did not regenerate `scripts/symbol_aliases.json`.** The file is a fixed
  point, not a derived artifact; its own `_comment` says a regeneration is a
  regression. Everything here is a targeted edit to the shipped file.
* **Did not adjudicate the 104 live CONTRADICTED memberships** outside H2's scope
  (§11) — sized and handed off, not acted on.
* **Did not re-run W8-A's §3 exposure audit** (the "110 re-emitted"). It needs a
  fresh census + evidence regeneration. What *can* be said without it, because it
  is true by construction: the **carry-forward** half of that exposure — the 72
  memberships `--merge` would launder out of the shipped file's own live set — is
  defined as the intersection of `live` and `withdrawn`, and that intersection is
  now **empty**. The generation half is untouched and still needs H1.

## 11. Handoffs

1. **⛔ 104 CONTRADICTED memberships are still LIVE, ledger-wide.** The
   retail-byte adjudicator returns `CONTRADICTED` for **107** memberships; this
   lane removed 3 of them (the only 3 that were also withdrawn, i.e. inside H2's
   scope). The other **104 are live and carry no withdrawal at all**, so nothing
   currently opposes them. ★ Note the instrument gap this exposes:
   `icf_alias_finder --validate` reports **0 CONTRADICTED** on the same tree,
   because it measures **map-consistency**, not folding — the exact
   `OK (grounded)` → `OK (MAP-CONSISTENT)` lesson, restated with a current
   number. Do **not** bulk-withdraw them: this lane's own 3 needed individual
   inspection to confirm, and a tool's confident "contradicted" is the claim most
   worth auditing. List: `~/tmp/w9d/live_contradicted.json`.
2. **535 live `NEEDS_SOURCE` and 107 live `NEEDS_MAP_ID` memberships.** Not
   defects — they are the *unadjudicable* remainder, and they mark where source
   or an identification would convert a guess into a verdict.
3. **H1 remains the root cause** and is now better evidenced (§10).
