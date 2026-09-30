# W16-HI — anonymous fn_ naming in src/system (engine) units: controls and verdicts (2026-09-30)

This lane applies the W16-HD scorer and the W16-HF adjudicator to a new population: compiled
`src/system/` units minus `bandobj/`, `rndobj/`, `char/` and `rnddx9/`, which another session
owns (W17-ANON). Branch `w16-hi` off main `c02c86962`, built before measuring. Ruler:
report.json's `name_check` plus the three other pinned keys, through `icf_aliases.map`.

**Result: 29 names / 6,020 B added to `scripts/target_symbol_map.json`, 1 refused.
A/B: +7 matched / +7 honest / +788 B / +0.007694 pp code% / +0.056390 pp fuzzy.**
`icf_alias_finder.py --validate` passes, with the same counts as before the edit.

## 1. Population

This lane uses main's `--scope` (from W17-ANON). The lane's first version added its own flag,
which was dropped at the coordinator's request. The scope is 21 prefixes: every `src/system/<dir>/`
of a compiled unit except the four excluded ones.

```
beatmatch dsp flow gesture hamobj math meta midi movie moviebink net obj oggvorbis os synth
synth_xbox track ui utl world zlib
```

| quantity | value |
|---|---|
| units | **468** |
| targets (`fn_`/`lbl_`, fuzzy 0) | **2,097 rows / 321,384 B** |
| candidate pool | 130,342 EXTERNAL + 738 STATIC = 39,383 distinct names |
| in-band candidates per target | median 41, max 2,044, 49 with none |
| targets scored | 1,993 (49 no candidate, 52 `null` in map, 3 already mapped) |

The dropped own-flag version produced the same census exactly (468 units / 2,097 rows /
321,384 B). That agreement is the check that the two scope definitions select one population.

## 2. Scorer negative control (`control2`, rerun on this population)

The setup is the same as W16-HD: 2,000 rows stratified to the target size distribution, seed
20260930, k=6, two legs × two brackets. It was run twice, once on the pre-rebase tree and once
after rebasing onto `c02c86962`, which changed `obj/Object.h` and so recompiled every engine TU.
58 of the 24,348 scores differ between the runs (max |Δ| 75.8), but **no rule outcome differs in
any cell**.

**Pre-registered predictions and outcomes:**

| # | prediction | outcome |
|---|---|---|
| 1 | spatial FP OUT at T=75 is 0.5–1%, above the game layer's 0.25% | **true: 0.60%** |
| 2 | W16-HD's T=75 knee may not carry over | **true**: FP rises steadily below T=90 |
| 3 | in-pool precision ≥ 99% | **true: 100%** at every T with the spatial gate |

**Score alone does not work here** (M=1, named bracket). True-removed FP is 2.55% at T=100,
9.75% at T=90 and **13.90% at T=75**, against W16-HD's 6.05% at T=90 in the game layer.

**Score + spatial gate, M=1.** Named and anon agree within ±1 fire.

| T | fires IN | prec IN | recall IN | fires OUT | FP OUT |
|---|---|---|---|---|---|
| 100 | 367 | 100.00 | 18.35 | 3 | 0.15 % |
| 95 | 392 | 100.00 | 19.60 | 9 | 0.45 % |
| **90** | **395** | **100.00** | 19.75 | **9** | **0.45 %** |
| 85 | 396 | 100.00 | 19.80 | 11 | 0.55 % |
| 80 | 400 | 100.00 | 20.00 | 11 | 0.55 % |
| 75 | 404 | 100.00 | 20.20 | 12 | 0.60 % |
| 70 | 405 | 100.00 | 20.25 | 13 | 0.65 % |

**Rule: T=90, M=1, spatial gate on.** It was chosen from the first control run before the rerun
or any real proposal was read. The pre-stated condition was "T=90 if its FP ≤ 0.5%, else T=95",
and the rerun satisfied it. T=90 is the bottom of the only plateau (95→90). Going from 90 to 75
adds 9 recall rows for 3 more FP.

All 12 FP events at T=75 are twins or siblings:

- template instances: `__uninitialized_copy` PB/PA, `__upper_bound<RangedData<…>>`, `_M_insert_overflow_aux`
- `StandardEffect<X>::OnSetParameters`→`Reset`, 4 events, all in `synth_xbox`/`synth`
- a `$4PPPPPPPM` thunk: `PostSave@ObjectDir`→`Export@RndDir`
- source siblings: `IsRunning` Parallel/Serial, `Save` GroupSeq/ParallelGroupSeq,
  `UpdateExtendedMesh`→`Text`, `String::operator+` overloads

Per-dir, the FP concentrates in `synth_xbox` (4/179), `ui` (3/275), `synth` and `beatmatch`
(2 each).

**Proposals:** 30 / 6,116 B, with 2 dropped as non-bijective. The prior-free bound is weak:
wrong ≤ 0.45% × 1,993 ≈ 9 (about 17 at the Poisson upper bound). So the scorer alone guarantees
only about 70% precision on the 30, and the adjudicator carries the rest.

## 3. Adjudicator control, and the engine accept rule

`--control 200` found only **88** eligible rows in these units. As shipped, the tool counts
`NAME_MAPPED_ELSEWHERE` as a negative on the WRONG leg, where it fires by construction. It can
never fire on a real proposal, whose pool excludes mapped names. The coordinator flagged this as
vacuous. **Recounted without it:**

| rule | TRUE accepted | WRONG accepted |
|---|---|---|
| 0 negatives, ≥ 1 positive (W16-HF's rule) | 59 / 88 (67.0%) | **4 / 88 (4.5%)** |
| 0 negatives, **≥ 2 positives** | 47 / 88 (53.4%) | **1 / 88 (1.1%)** |
| 0 negatives, ≥ 3 positives | 29 / 88 | 0 / 88 |

The 4.5% at ≥ 1 is close to W17-ANON's engine figure (5.0%) and far above the game layer's 0.6%.
All four wrong accepts rest on one weak positive, e.g. `op1`/`op7@ByteGrinder` sharing
`DataNode::Int`.

**Engine accept rule, fixed from this table before any real row was read:**

- **Mechanical accept:** 0 content negatives, ≥ 2 positives, and a runner-up carrying ≥ 1 negative.
- **Template instances** (`??$…`, or methods of a `?$` class): FOLD does not count as a positive,
  because a fold is what a twin looks like. They need a twin-separating anchor: a type-specific
  callee or datum, a caller edge, or a vtable slot.
- **Everything else** needs a named independent anchor, or it is refused.

Both `--independent` and the normal run gave the same pre-verdicts: 23 SUPPORTED /
4 CONTRADICTED / 3 UNANCHORED.

## 4. Verdicts (30 proposals: 29 accept, 1 refuse)

`pos`/`neg` come from the `--independent` run. RU = runner-up.

| # | addr | B | name | fz | pos/neg | verdict / anchor |
|---|---|---|---|---|---|---|
| 0 | `0x8231a980` | 16 | `Replace@RndTransformable` `$4` thunk | 100 | 1/0 | ACCEPT (manual): the destination `Replace@RndTransformable` is bound; the RU `Print` thunk is contradicted by it. A thunk is its destination plus adjustor immediates, and the adjustor is covered by the fz-100 score |
| 1 | `0x824c7c48` | 92 | `operator>><CamShotCrowd>` | 97.0 | 3/0 | ACCEPT (template): type-specific callees `Load@CamShotCrowd`, `resize<ObjVector<CamShotCrowd>>`; RU `<pair<int,int>>` contradicted |
| 2 | `0x824d5028` | 444 | `SpotlightDrawer::ApplyLightingApprox` | 93.0 | 5/0 | ACCEPT (mechanical) |
| 3 | `0x824ecd20` | 676 | `WorldInstance::SavePersistentObjects` | 91.8 | 13/0 | ACCEPT (mechanical) |
| 4 | `0x82527f08` | 384 | `VirtualKeyboard::OnShowKeyboardUI` | 92.7 | 7/3 | ACCEPT (manual): matched-caller witness plus 6 agree. All 3 negatives are one retail `dynamic_cast` (`__RTDynamicCast` + 2 TypeDescriptors) that our body lacks, a source divergence. Lone candidate |
| 5 | `0x82725b18` | 96 | `op9@ByteGrinder` | 92.2 | 1/0 | **REFUSE**: a single shared-callee agree (`DataNode::Int`), which the RU `op53` has too. This is exactly the control's WRONG-accept family |
| 6 | `0x82741b18` | 268 | `Splash::Resume` | 98.5 | 4/1 | ACCEPT (manual): see §6. Retail `SetRndSplasherCallback`'s `r5` (the resume callback) → thunk `0x82741c38` → `b 0x82741b18`, while `r4` (suspend) → `fn_82741C28` → the mapped `Suspend@0x82741a00`. The one `CALLER_CONTRA` comes from a map defect |
| 7 | `0x82741ce0` | 332 | `Splash::PrepareNext` | 93.0 | 14/1 | ACCEPT (manual): 11 agree incl. `kSplashMovie`, `Find<TexMovie>`, plus a caller. The negative is one retail-only `bl 0x82743a38` our body lacks |
| 8 | `0x82771500` | 136 | `SongData::GetVocalNoteList` | 95.9 | 2/0 | ACCEPT (mechanical) |
| 9 | `0x82779a10` | 488 | `AddChordLevel` | 93.2 | 32/0 | ACCEPT (manual; lone candidate): `"<gtr>"`, `gSuperscriptStarted`, 29 caller witnesses |
| 10 | `0x8277d4f0` | 532 | `MasterAudio::SetupChannels` | 96.8 | 6/0 | ACCEPT (mechanical) |
| 11 | `0x8277f338` | 264 | `TrackData::SetMapping` | 90.7 | 6/0 | ACCEPT (mechanical; not a template instance, only its parameter is) |
| 12 | `0x82781b00` | 116 | `VocalNoteList::AddNote` | 100 | 6/0 | ACCEPT (mechanical) |
| 13 | `0x82781d58` | 236 | `VocalNoteList::StartPlayerPhrase` | 99.9 | 6/1 | ACCEPT (manual): 5 agree plus a caller; RU `EndPlayerPhrase` contradicted by 3 float literals. The negative is fold-shaped: retail `push_back<BeatCollisionData>` vs our `push_back<VocalPhrase>`, which has no retail address |
| 14 | `0x82789808` | 96 | `__uninitialized_copy<DifficultyInfo>` | 100 | FOLD only | ACCEPT (template): call edge. In #16 (fz 100, 1:1 aligned) retail calls this address at both `copy` sites |
| 15 | `0x82789fd0` | 96 | `__uninitialized_fill_n<DifficultyInfo>` | 100 | FOLD only | ACCEPT (template): call edge. #16 calls this address at its single `fill_n` site |
| 16 | `0x8278a2e0` | 328 | `_M_insert_overflow_aux<DifficultyInfo>` | 100 | 4/0 | ACCEPT (template): type-specific `_Copy_Construct<DifficultyInfo>`, `_M_clear_after_move<DifficultyInfo>`, plus a caller |
| 17 | `0x8278eac8` | 12 | `GameGem::GetFret(uint) const` | 99.7 | 33/0 | ACCEPT (mechanical): 33 caller witnesses |
| 18 | `0x8278ec80` | 52 | `GameGem::GetFret() const` | 99.9 | 0/0 | ACCEPT (manual): immediates. Retail has `li r3,-1` … `li r3,3`, the same as source `ret=-1` … `ret=3` |
| 19 | `0x8278ecb8` | 48 | `GameGem::GetNumStrings` | 91.6 | 0/0 | ACCEPT (manual): immediates. Retail has `li r3,0` … `addi r3,r3,1`, a count, the same as source |
| 20 | `0x8278ed28` | 12 | `GameGem::SetFret` | 99.7 | 1/0 | ACCEPT (manual): caller witness; RU `GetFret(uint)` is bound at `0x8278eac8` by 33 witnesses |
| 21 | `0x827a3948` | 96 | `__uninitialized_fill_n<TrackChannels>` | 100 | 1/0 | ACCEPT (template): caller edge, plus #22 calls it at its `fill_n` site; RU `copy<TrackChannels>` is bound at `0x827d14d0` |
| 22 | `0x827a3cf8` | 324 | `_M_insert_overflow_aux<TrackChannels>` | 99.9 | 5/0 | ACCEPT (template): type-specific `fill_n`/`copy<TrackChannels>`, plus a caller |
| 23 | `0x827c91a0` | 36 | `SecondsToTick` | 100 | 3/0 | ACCEPT (mechanical) |
| 24 | `0x827cc8f0` | 128 | `UTF8toASCIIs` | 96.2 | 2/0 | ACCEPT (mechanical) |
| 25–27 | `0x827d7b38` / `c68` / `d50` | 304 / 228 / 220 | `CacheMgrXbox::{Mount,Unmount,Delete}Async` | 97+ | 7/0 each | ACCEPT (mechanical); also a retail vtable run in consecutive words (file offsets `0x11b7f4/f8/fc`) in the same order as `CacheMgr.h`'s declarations |
| 28 | `0x82811e30` | 16 | `Export@RndDir` `$4` thunk | 100 | 1/0 | ACCEPT (manual): destination `Export@RndDir` bound, RU contradicted; UILabelDir vtable slot 14. The control's T=75 FP was this name one slot away at `0x82811e40`, where the destination differs |
| 29 | `0x828175c0` | 40 | `UIGridSubProvider::UpdateExtendedText` | 90.0 | 0/0 | ACCEPT (manual): retail RTTI vtable slot 5 = our `UIListProvider` slot 5 (dtor, Text, Mat, Provider, Custom, UpdateExtendedText); it sits between the mapped 100% `Custom` and `UpdateExtendedMesh` |

## 5. Map write, validation and A/B

- **Map write:** `gated_map_write.py` selftest 15/15 and dry run passed, then the write:
  "+29 row(s); Q1-Q7 pass; bytes outside the inserted line(s) are IDENTICAL".
- **Build:** a re-split build (`touch config.yml`) ran with rc=0. **`symbols.txt` did not
  change**, so W16-HF's Class-4 over-carve conflict did not recur.
- **`icf_alias_finder.py --validate`:** before, PASS 1,479 / 250 / 0 contradicted of 1,730;
  after, **PASS 1,479 / 250 / 0 of 1,730**. The first after-edit run correctly REFUSED
  (`STALE_TREE`, build owed) until the build ran.

The A/B was `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hi --from-dirty` over the branch
diff (map only). Both legs sat at a split fixed point (0 extra re-splits), the leg-B renamer
patched 1,834 files, and objdiff-cli sha256 `c1b7d952…` was stable across legs.

| | leg A | leg B | Δ |
|---|---|---|---|
| matched_functions | 44,621 | 44,628 | **+7** |
| honest | 21,272 | 21,279 | **+7** |
| matched_code | | | **+788 B** |
| code% | 42.350220 | 42.357914 | +0.007694 pp |
| fuzzy | 51.136610 | 51.193000 | +0.056390 pp |
| `none` control | | | +1,360 B (REAL_PAIRING) |

Units at 100% stayed at 212 → 212. All 29 named rows now pair. 8 of them read fuzzy 100: #0, 12,
14, 15, 16, 21, 23, 28.

**Two existing rows dipped:**

- **`?SuspendFunc@@YAXXZ` (`0x82741c38`, 12 B) fell from 100.0 to 98.33 and loses 12 B.** This is a
  **pre-existing map defect exposed by `Resume@Splash`**, not a wrong new name (see §6).
- **`push_back<RecurseInfo>` (HolmesClient, 116 B) went from 99.97 to 99.79; no bytes were lost**,
  since it was already below 100. Retail calls `0x827a3cf8`, i.e. `_M_insert_overflow_aux` is ICF
  folded across `TrackChannels` and `RecurseInfo`. The `TrackChannels` spelling was chosen because
  its copy is the one in DataArraySongInfo's region, with a DataArraySongInfo caller witness. Its
  other charge is also a pre-existing **source defect**: the element stride is `0x10` in retail and
  `0x18` in ours, so our `RecurseInfo` is 24 B where retail's is 16 B.

## 6. Map defects found, not fixed here

Fixing these means renaming existing rows, which is outside this lane's add-only scope. They are
left for a map lane.

- **`0x82741c38` is `ResumeFunc`, not `SuspendFunc`.** In `BeginSplasher`, retail loads it into
  `r5` of `SetRndSplasherCallback(Poll, Suspend, Resume)`, and it branches to `Resume@0x82741b18`.
- **`fn_82741C28` (anon, 12 B) is `SuspendFunc`.** It is passed in `r4`, and it branches to the
  mapped `Suspend@0x82741a00`.
- **`0x82742c98` is mapped `?ResumeFunc@@YAXXZ` (76.7%)** but is a `TaskMgr::DeltaSeconds` thunk
  (`addi r3,TheTaskMgr; b DeltaSeconds`).
- **`RecurseInfo` size (source, HolmesClient):** 24 B in ours vs 16 B in retail (see §5).

## 7. Not done

- #5 `op9` is refused, and the 2 non-bijective drops were not revisited.
- No threshold below T=90 was applied.
- No alias groups were added for the two dips.
- The adjudicator tool was not edited; the coordinator is fixing its control on main. Its control
  was recounted here from the detail rows instead.

## 8. Reproduce

```
S=src/system/beatmatch/,src/system/dsp/,...,src/system/zlib/     # the 21 prefixes in §1
python3 tools/anon_candidate_scorer.py --scope $S census
python3 tools/anon_candidate_scorer.py --scope $S control2 --rows 2000 --k 6 --workers 14 --json-out ~/tmp/w16hi/ctrl_2000.json
python3 tools/anon_candidate_scorer.py --scope $S evaluate ~/tmp/w16hi/ctrl_2000.json --spatial --by-dir --thresholds 100,95,90,85,80,75,70 --margins 1
python3 tools/anon_candidate_scorer.py --scope $S propose --threshold 100 --margin 100 --k 6 --workers 12 --json-out ~/tmp/w16hi/propose_raw.json
python3 tools/anon_candidate_scorer.py --scope $S apply ~/tmp/w16hi/propose_raw.json --threshold 90 --margin 1 --spatial --json-out ~/tmp/w16hi/props_T90.json
python3 tools/anon_proposal_adjudicate.py ~/tmp/w16hi/props_T90.json [--independent] --json-out ...
python3 tools/anon_proposal_adjudicate.py ~/tmp/w16hi/props_T90.json --control 200 --json-out ~/tmp/w16hi/adjctrl.json
```

The proposals are committed as `docs/decomp/W16HI_SYSTEM_ANON_PROPOSALS_T90_2026-09-30.json`.
The raw score files stay in `~/tmp/w16hi/` and are not committed.
