# W16-IC — W16-IB's foreign-pinned game rows re-homed; 15 missing retail TUs written (2026-10-01)

Branch `w16-ic`, rebased onto main `a75d0c5b0` (lane start: `358101f81`; §4 has the two rebases). Scope: the re-home candidates W16-IB left in
`docs/decomp/W16IB_ANON_GAME_ROWS_IDENTIFIED_2026-09-30.md` §6 ("50 rows / 4,104 B from the vtable sweep,
76 / 15,268 B from caller alignment"). Four forks did part of the work, each on its own branch and merged
here `--no-ff` (§4).

**Result.** Whole-branch A/B on main `a75d0c5b0`: **+250 fns / +193 honest / +28,976 B / fuzzy +0.294372 pp**.
Units at 100 (`mpn`) went 252 → 267: 12 new units at 100 and 5 DENOMINATOR_SHRANK. Two units fell off because
re-homed rows joined their denominators (§5). The branch adds or changes 195 map keys. All 195 pair, and 179
of them (24,392 B) read fuzzy 100. Fifteen retail TUs our tree never compiled are now written and pinned (§3);
pairable units went 1,745 → 1,760. Eleven existing map keys were corrected or filled on retail evidence. Gates: injectivity OK,
validator PASS, native 18/18.

## 1. The population did not reproduce, and most of it was not a pin problem

W16-IB's two lists were recomputed on the lane-start tree (main `358101f81`, the pre-rewrite W16-HZ merge)
with a **multi-entry** definition index: every base obj that defines a name, not one entry per name.
W16-HZ §2 B showed that a single-entry index files own-unit rows as foreign. Classes, at lane start:

| class | VT rows / B | CA rows / B | what it is |
|---|---:|---:|---|
| foreign: the name is defined only in another unit's base obj | 42 / 3,436 | 27 / 6,088 | a pin problem; re-home |
| **no base def**: our source declares the name, no obj defines it | 8 / 668 | 51 / 11,528 | **a missing TU**, mostly (§3) |
| own unit defines the name | — | 23 / 4,068 | not a re-home; W16-IB's refusals live here |
| the name is mapped at another address | — | 4 / 652 | identity conflict (§6) |
| already named (W16-IB, W16-HZ) | — | 77 | done |

The "76 / 15,268 B" does not reproduce as one class. Foreign plus no-base-def CA rows give 78 rows / 17,616 B,
the nearest match. The instrument below is what was used; W16-IB's figure is not.

**The no-base-def class was the larger lever.** Most of its rows sit inside retail TUs our tree never
compiled, whose retail bodies are pinned under neighbouring units. That is W16-IB's Quest/TourReward
situation, repeated fifteen times (§3). Measured on the final tree:
- 33 of the class's 59 rows (5,492 B) are now named, and 29 of those read 100.
- Writing the TUs named many more rows that no census row pointed at (§3).
- The 26 still open are listed in §6. They are MusicLibraryUnkOp (a deliberate stub), Quazal, XMAReader,
  AssetStore, the `??_E` group, and TU5-era functions with no source anywhere.

## 2. Re-homes (foreign class)

Method: W16-HV/HZ's, unchanged. The row is within 0x400 of the destination's pin, and no named row at
fuzzy ≥ 50 sits between them. The retail row was scored on the graded ruler against every unclaimed
same-size (0.5–2×) function of the destination obj (9,690 diffs). Names were adjudicated with
`tools/anon_proposal_adjudicate.py --independent` through HV's `adj_rehome.py`. `.text` was moved with HV's
`apply_moves.py`, and names were written only through `tools/gated_map_write.py`. The whole `report.json` was
diffed by retail address after every build.

**Exclusion first.** Any move whose range intersected a missing-TU region (§3) was held back. The
TransConstraint move (`0x82628AD0–0x82628C38`) would have carried PassiveMessagesPanel's own functions into
TransConstraint. Fork A then showed that the row itself, `0x82628AD8`, is `??1PassiveMessagesPanel`.

| step | rows named | `.text` moves | in-tree Δ (fns / B), same-tree build before/after |
|---|---:|---:|---:|
| pass 1: adjacency | 50 | 38 | +11 / +868 |
| StatMemberTracker contribution symbols as function-local statics | — | — | +18 / +1,080 |
| pass 2: PerfectSectionTracker (adjacent once pass 1 moved CheckForCompletedSections) + 3 `??_G` by RTTI | 6 | 1 | +2 / +208 |
| `??_GLeaderboardShortcutProvider` (slot 0 of its primary vtable; caller alignment had said `??_E`) | 1 | — | +1 / +76 |

The in-tree Δ is net of collateral, mostly masked funclets losing their old pairing (below). Fork D's
repairs to these rows are in §4.

- **Scoring and adjudication.**
  - 38 of the 50 pass-1 names were top in the destination obj.
  - The other 12 are retail-vtable identities with drifted bodies: ContentLoadingPanel
    `ContentMounted`/`ContentFailed` (32), SaveLoadStatusPanel `FinishLoad`/`Draw` (6.5/14), the eight
    StatMemberTracker `GetContributionSymbol` overrides (15), and `UIListProvider::UpdateExtendedText` (68).
  - Adjudication: 37 SUPPORTED, 2 UNANCHORED (both at 100), and 11 CONTRADICTED. Every CONTRADICTED is on
    body only (RETAIL_ONLY strings or callees), with AGREE identity evidence. None carries a caller or
    binding contradiction.
- **Refused:**
  - `0x822CEEE8` `ObjVector<ObjPtr<RndMesh>>::resize`: 33.7, against `MakeString` at 69.9.
  - `0x822E8CF8` `__uninitialized_copy<ObjPtr<RndPropAnim>>`: ties at 99.8–100 with the EventTrigger
    copies, so it is a fold.
  - `0x823EA910` `FindRangeAtTick` and `0x826414C0` `??1FlowSlider`: body 0.
- **`0x823C4A90`** was a deliberate `null` (W17-HCT: "T not provable, anonymous callees"). Caller alignment
  at `CharBlendBone::Copy`'s `bl` binds `ObjList<CharBlendBone::ConstraintSystem>::operator=`. The body reads
  100 against CharBlendBone.obj, with the runner-up at 70.
- **`0x82692978`**, slots 23/24 of every StatMemberTracker subclass vtable, is the folded `return ""` getter.
  It stays anonymous. W16-HU held a `ClassName@BandCharDescTest` proposal for this address without applying
  it; the vtables rule that out.
- **One over-carve merge.** Naming `0x823E1110` (`__find<User**, LocalUser*>`) let jeff fold its two tails
  (0x110 + 0x20 + 0xC = 0x13C). That `symbols.txt` hunk is its own commit, and it went into leg A (§5).
- **One alias membership withdrawn.** `vector<JumpInstance>::push_back` sat in the fold group at
  `0x8257FCF8` (`push_back<PlayerScore>`).
  - The two retail bodies are word-identical except their two `bl` targets.
  - `0x8257FCF8` calls `_M_insert_overflow_aux<PlayerScore>`, and its callers are the three PlayerScore
    users.
  - `0x82704BB8` calls its own neighbour `0x82704058`, and its only caller is `StandardStream::DoJump`.
  - Naming `0x82704BB8` made `icf_alias_finder --validate` FAIL with 1 contradicted. After the withdrawal it
    PASSes, and no row moved: Δ0 on every key.

Collateral of the re-homes: only the masked funclet class went down. 32-B EH funclets that had paired by
byte signature in their old unit have no counterpart in the new one until the parent body matches. The
nine TrackerManager funclets came back once the contribution symbols were fixed. Seven
PerfectSectionTracker funclets wait on that unit's bodies (fork D, §4).

## 3. Missing retail TUs (no-base-def class)

Located by retail RTTI: every vtable whose COL names the class, its slots, and the census's caller-bound
rows. Extents were then refined from body contents. Each TU was written from retail asm, with the oracle
as a starting point where one exists. Comments state behaviour, never provenance.

| TU | retail extent | was pinned under | rows | by |
|---|---|---|---:|---|
| AppScoreDisplay.cpp | 0x82642450–0x826424B0 | UploadErrorMgr | 1 | lane |
| BandScreen.cpp | 0x826424B0–0x826428E0 | UploadErrorMgr, a UIScreen island | 10 | lane |
| MatchmakingSettings.cpp | 0x823F33E0–0x823F3E24 | unpinned head, PrefabMgr, Matchmaker | 25 | C |
| ShellInputInterceptor.cpp | 0x825ACA64–0x825AD700 | LockStepMgr, RockCentral, CameraManager | 6 (+2 Joypad) | C |
| CampaignCareerLeaderboardPanel.cpp | 0x825F10E0–0x825F2210 | CampaignGoalsLeaderboardPanel | 19 | A |
| PassiveMessagesPanel.cpp | 0x82628AD0–0x82629860 | NewAwardPanel, two TransConstraint islands | 16 | A |
| StandInProvider.cpp | 0x82672F14–0x826731D8 | MainHubMessageProvider | 5 | A |
| HeaderPerformanceProvider.cpp | 0x825CA4E0–0x825CAC64 | AppLabel, RockCentral | 9 | A |
| GameMic.cpp | 0x826CCE28–0x826CD438, 0x826CD4E0–0x826CD8DC | TrainerPanel, MultiplayerAnalyzer | 15 | B |
| FretHand.cpp / ChordPreview.cpp | 0x826F1080–0x826F1430 / –0x826F15FC | RGTutor, VocalPart | 7 | B |
| RKTrainerPanel.cpp | 0x826B1760–0x826B1C88 | RGTrainerPanel, CharFeedback | 11 | B |
| TourCharLocal.cpp | 0x82B799B8–0x82B7A06C | GemManager | 12 | B |
| ProfilePicture.cpp / ProfilePicture_Xbox.cpp | 0x82B8FF80–0x82B90370 / –0x82B90644 | head of TrackPanel | 8 | B |

**BandScreen.** Retail differs from the oracle in four places. All 11 rows read 100 once they follow retail:
- `LoadPanels` and `LoadInterstitials` have no vignette gate; retail has no `mShowVignettes`.
- `TheUI` is read through the pointer.
- `Enter` builds `block_wipe_in` as a function-local `static Message`.
- `CheckIsLoaded`/`IsLoaded` re-normalise the base result (`!= 0`). Without that, MSVC emits a 4-byte tail
  branch, which scores 8.6.

`BandScreen::Exit` (`0x826428A0`) had been mapped for some time but could not pair, because nothing compiled it.

**Map values corrected on retail evidence (forks).**
- `0x82628F40` `??_GTransConstraint` → `??_GPassiveMessagesPanel`.
- `0x825CAC18` `??_GStorePurchaseable` → `??_GSetlistScoresProvider`.
- The `ClassName` thunk at `0x825F1360` now carries CampaignCareerLeaderboardPanel's name.
- `0x826CCE60` → `GameMic::GetMyMic`.
- `0x826B17E8` `XAuthCreateLocalSocket` → RK `IsSongSectionComplete`.
- Four `null` keys were named.

The two wrong `??_G` names had read 100 only because their destructor callee was unnamed, which `name_check`
forgives. ⇒ A `??_G` at 100 is not evidence of its name (W16-HZ §3 found the same).

**Header/flag fix (fork B).** `ProfilePicture.h` lacked two Xbox members (+0x18, +0x1c). That gap, not
"per-TU ODR skew" (`f4972de45`), is why BandProfile.cpp needed `/DRB3_ONLINEID_PLAYERNAME`. The flag is
removed, and no BandProfile row moved. **CLAUDE.md's census of eleven `/D` flags is now ten.**

**Not written: AssetStore.** Retail ~0x825EC840–0x825ED2A0 is identified (vtable 0x820B9874, ctor
0x825ECD58, `Poll` 0x825ED020, …). There is no oracle body, and its offer objects belong to a second,
unidentified class (unpinned 0x8266B4C0–0x8266B650).

## 4. Forks

| fork | work | in-tree Δ on its own base | merged Δ here |
|---|---|---:|---:|
| A | 4 TU ports (meta_band panels and providers) | +61 / +8,668 B | +61 / +8,668 B |
| B | 6 TU ports (game, tour, ProfilePicture) | +63 / +7,428 B | +63 / +7,428 B |
| C | MatchmakingSettings, ShellInputInterceptor | +47 / +5,552 B | +47 / +5,552 B |
| D | body repair of the pass 1/2 rows: 17 of 20 to 100 | +35 / +4,048 B | +35 / +4,048 B |

Every merged Δ equals the fork's own, so the forks compose additively. Merge conflicts, A and B: three
pins that both sides carved. HEAD's `splits.txt` was kept, and the fork's new `.text` blocks were replayed
through `apply_moves.py`, which re-runs the coverage, bisect and overlap checks. The layout in every
affected region was then compared with the fork's and is identical. `target_symbol_map.json` conflicts are
always the gate's top-of-file insertions, resolved as the union and re-checked for duplicate keys and
injectivity.

**Base change mid-lane.** Main re-landed W16-HZ as `a95d5c525` (rewritten commits, 18 source files of
comment-scale difference). Forks A and B branched from it, C and D from the old `358101f81`. `w16-ic` was
rebased onto main `18f408adc` with `--rebase-merges` before A and B were merged. The rebase had one
conflict, resolved by union in the map; `Joypad.cpp` auto-merged. Main then moved again, to `a75d0c5b0` (W17-ANON
and W16-IE). The branch was rebased once more, and the A/B is on that base. That second rebase was checked
for substance before it was trusted. Main's changed map keys and changed `.text` blocks were intersected with
the branch's: **0 shared keys, 0 name collisions, 0 overlapping blocks**. After the rebase, all 38 files carry
the same non-`.pdata` diff as before it. Fork D's twelve commits were made on the pre-rebase `c254e3b0f`. They
were rebased onto its equivalent `4d52898d2` without conflicts and then merged.
⚠ Main has since moved to `78cb928ef` (W17-BPM3). Against the branch: 0 shared map keys, 0 overlapping
blocks, so the landing rebase should be textual only.

**Fork D's repairs, and the two that change behaviour.** The dominant pattern is again TU5 function-local
statics: PerfectSectionTracker ×3 to 100 with 12 funclets re-paired, SaveLoadStatusPanel ×3,
Tracker::StartIntro, StreakFocusTracker, ContentLoadingPanel ×2 and SelectedGoal.
- `SaveLoadStatusPanel::FinishLoad` (6.5 → 100) finds `saveload_icons` and sets the anim rate of its
  `start_saving.trig`/`finish_saving.trig` to `k30_fps_ui`. It does this through an out-of-line
  `EventTrigger::SetAnimRate` (retail `0x8249CE10`, deliberately left unnamed).
- **Behaviour:** `StoreMenuPanel::Enter` tests `mCurrentMenuIx == 0`. Our `== -1` could never fire, because
  the ctor sets 0, so the store index was never requested on first entry. `PlayerLeaderboard`'s ctor has no
  guest-owner profile fallback.

## 5. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/w16ic-ab/wt --patch <07c6b1f38..w16-ic, docs excluded>`,
**one run** (kinds configgen + map + source + splits). Both legs were re-split to a `symbols.txt` fixed point
(0 extra re-splits each). Leg B recompiled 1,009 TUs. Ruler `name_check`, objdiff sha `c1b7d952` stable
across legs. Result: `~/tmp/w16ic-ab/wt/.ab_measure_runs/20261001-025342-w16-ic-1005755/result.json`.

**Leg A** is main `a75d0c5b0` plus one commit (`07c6b1f38`): the split's own over-carve merge for
`0x823E1110` in `symbols.txt`, which `ab_measure` refuses as a patch. It is a no-name merge, so it is inert
on its own. Unlike W16-IB, leg A carries **no** `objects.json` or new `.cpp` scaffolding. Leg B's bare
`configure.py` was made safe instead: the worktree lives at `~/tmp/w16ic-ab/wt` next to `jeff` and `objdiff`
symlinks to the live forks. `configure.py`'s upward walk finds those before the stale `~/tmp/jeff`
(W16-IB §7), and both legs resolve the same binaries (dtk sha `f0937c88…` on either path). So headers,
`objects.json` and the new TUs are all measured.

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 46,875 | 47,125 | **+250** |
| masked_equal | 23,726 | 23,783 | +57 |
| honest (matched − masked_equal) | 23,149 | 23,342 | **+193** |
| matched_code_percent | 46.239200 | 46.521973 | +0.282773 pp (**+28,976 B**) |
| fuzzy_match_percent | 56.024338 | 56.318710 | **+0.294372 pp** |
| units at 100 (`mpn`) | 252 | 267 | +15 |

- **Prediction, written before the run:** the sum of the in-tree step deltas is phase 1 through pass 2
  (+31 / +2,156) + BandScreen/AppScoreDisplay (+12 / +1,048) + `??_GLeaderboardShortcutProvider`
  (+1 / +76) + C (+47 / +5,552) + A (+61 / +8,668) + B (+63 / +7,428) + D (+35 / +4,048). That predicts
  **+250 / +28,976 B**, and the run measured exactly that. Leg B equals the branch's in-tree build on every key.
- **Rows, leg A → leg B, by retail address:** 301 up (266 now at 100), **3 down, none from 100**. Two
  40-B anonymous funclets re-paired by byte signature (RockCentral `fn_824F9FA8`, CameraManager
  `fn_824BBB20`, 99.3/99.4 → 93.5), and the 12-B anonymous `0x82801370` (11.7 → 0) moved into
  UIListProvider's denominator.
- **Units:**
  - 12 new units at 100: BandScreen, AppScoreDisplay, CampaignCareerLeaderboardPanel,
    PassiveMessagesPanel, StandInProvider, HeaderPerformanceProvider, MatchmakingSettings,
    ShellInputInterceptor, RKTrainerPanel, FretHand, ChordPreview and ProfilePicture_Xbox.
  - 5 finished by DENOMINATOR_SHRANK: NewAwardPanel, MainHubMessageProvider, TransConstraint,
    TrackerDisplay and RGTutor.
  - **Fell off:** UIListProvider (rows 11 → 15) and NameGenerator (8 → 10). Re-homed rows below 100
    joined them; no matched row was lost.
  - The per-unit losses (RockCentral −19, NewAwardPanel −11, NetGameMsgs −9, …) are rows leaving for
    their real units; the all-unit net is +250.
- `none`-ruler control: +28,456 B, NOT_APPLICABLE (the patch has source).

Gates on the final tree (`855b9ac11`, the last source commit):
- `tools/map_name_injectivity.py`: **OK** (32,021 applied rows, injective).
- `tools/icf_alias_finder.py --validate`: **PASS** (1,488 map-consistent, 258 tolerated, 0 contradicted,
  1,747 total).
- `scripts/verify_objs_patched.py --verify-manifest`: OK.
- `tools/native_build_gate.sh`, run after the last source commit:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

## 6. Left, with identities where known

- **`??_E` caller names that are not slot 0** (7 rows / 556 B): `0x82603C10` CampaignSongInfoPanel,
  `0x826A8E60` FadePanel, `0x82670C68` AssetProvider, `0x82523DD0` RemoteUser, `0x825748A0`
  CalibrationWelcomePanel, `0x82574D40` NextSongPanel and `0x827B79C8` ProfileMgr. Our callers call `??_E`
  at these sites. None of these addresses is its class's vtable slot 0.
- **No base def, and no oracle body anywhere** (TU5-era): `RockCentral::SaveBinaryData` 0x824F8A98
  (472 B), `RockCentral::GetArtFile` 0x824F83F8 (288 B), `RB3AddSongDataUpgradeGate` 0x8259E5B0 (332 B).
  Also small leftovers: the `JoinInvitePanel` ctor, the `SaveMemcardAction` ctor, `GetStarsForScore`,
  `SessionData::New`, `SessionUsersProvider::ShowGamercard`.
- **Mapped elsewhere, with a real conflict:** retail `BandUI::Terminate` calls `0x82592C30` (92 B, in
  AccomplishmentProgress's pin) where our source calls `UIEventMgr::Terminate`. The map binds that name at
  `0x827CEDD0` (83%). One of the two is wrong; this is engine-side and was not adjudicated here. The other
  three in this class are COMDAT or anonymous-namespace duplicates of rows already at ~100.
- **Far foreign rows:** `SongInfoCopy::GetName` (in UIEventMgr's pin), the InlineHelp
  `__uninitialized_copy`, `StatMemberTracker::GetSingularContributionSymbol` (0x826927D8, in NetGameMsgs'
  pin; the base `StatMemberTracker.cpp` is far away), `Stats::SetNumSections` (in PerformanceData's pin,
  next to `PropKeys`), and `_M_fill_insert_aux<DrivenPropertyEntry>` (a named TrackPanelDir row in the gap).
- **Named rows still below 100** (16 of the 195). Largest gaps:
  - `DataProvider::Text` 304 B @44.6 and `UIListProvider::UpdateExtendedText` 72 B @68.3 (engine `ui/`,
    not repaired here).
  - `PanelDir` ctor 568 B @82.1 (engine).
  - `__find<User**,LocalUser*>` 328 B @79.3.
  - `MultiplayerAnalyzer::AddGems` 400 B @84.97: the inner loop reloads the element after the `mMaxPts`
    store.
  - `GameMic::ThreadProcessOneFrame` 756 B @97.88: load order of `mDetector` vs `&mSamplesRecent`.
  - `FretHand::SetFingers` 480 B @98.21: register assignment only.
  - `VocalTrack::InTambourinePhrase` 64 B @87.2.
  - Seven more at 99.5–99.96, each one register order, one `fadds` operand order, or one ICF callee-name
    charge (`JoinRequestMsg` → `Net::GetGameData` vs `FileLoader::GetSize`; `NameGenerator` →
    `hash_map<Symbol,DataArray*>` vs `<Symbol,int>`).
- **`own unit defines` (23 rows)**: W16-IB's caller-alignment refusals. They are not re-homes, and they were
  not reopened.

## 7. Tooling notes

- **`apply_moves.py`'s summary line prints the wrong source headings** when a move fully consumes a block.
  It calls `heading_of(i)` with indices made stale by the deletion. The edit itself uses fresh indices and
  is correct. Verify a move by re-parsing `splits.txt`, not by its print.
- **The split guard fires on `.pdata` too.** It fires after any move or new heading, because dtk re-derives
  `.pdata` into `splits.txt`. One retry is the fixed point. Commit the rewritten file with the change.
- **A row diff keyed by name cannot see a rename.** A corrected map value reads as one row DOWN and one row
  UP. Key such diffs by retail address, resolving named rows through the matching side's map
  (`~/tmp/w16ic/rowdiff.py`).

## 8. Reproduce (`~/tmp/w16ic/`)

```
python3 census.py && python3 gap.py && python3 plan.py <refused addrs>   # population, adjacency, moves
python3 runnerup.py && python3 mkprops.py && python3 adj_rehome.py props.json adj.json
python3 mkmoves.py && python3 apply_moves.py config/45410914/splits.txt moves.json config/45410914/symbols.txt
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json map_rows.json
python3 tuloc.py                                                          # missing-TU locator (retail RTTI)
python3 rowdiff.py <repA> <mapA> <repB> <mapB> v                          # address-keyed collateral check
```
