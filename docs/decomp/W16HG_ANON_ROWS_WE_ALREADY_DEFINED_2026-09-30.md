# W16-HG — anonymous game-layer rows that our source already defined (2026-09-30)

Branch `w16-hg` off main `57013cc80`. Scope: game layer (`src/band3`,
`src/network` minus `quazal`), skipping Game.cpp, StoreOfferProvider.cpp,
BandStorePanel.cpp, MemMgr.cpp and `src/system/`.

## 1. The brief's premise, tested first

The brief said the gap is anonymous `fn_` rows at fuzzy 0 "whose functions our
source never defines", to be ported from the rb3-Wii oracle. Three measurements
before any edit, all on the settled tree (44,527 fns / 4,311,868 B):

| check | result |
|---|---|
| anon fuzzy-0 bytes per unit (160 units, 329,488 B) | top 4: DuplicatedObject 24,100 · RockCentral 20,224 · CustomizePanel 8,400 · Scheduler 7,408 |
| DuplicatedObject / Scheduler oracle | 74 / 30 lines, inside the Quazal `/Od` band 0x82A6D168–0x82B54190 ⇒ **no oracle**; skipped |
| RockCentral | pin carries foreign blocks (e.g. `fn_826D9938` refs `ui/startup/eng/startup_autosave_esrb_keep.milo`) ⇒ a pin problem, not porting; skipped |
| oracle `Class::Method` definitions absent from our base objs, top 70 units | **almost none** — CustomizePanel 1 (a stripped cheat), MetaPerformer 1, the rest 0; the only real lists are Wii-only (`SongStatusCacheMgr`, which retail replaced with a `hash_map`; `OvershellProfileProvider`/`WiiFriends*`) |

⚠ The first run of that oracle-gap check reported **65–125 "missing" functions
per unit**. It was vacuous: `coff_function_sizes` wants a `Path`, the bare string
raised, and a blanket `except` turned it into an empty symbol set. Re-run with
the fix, the result flipped to "almost nothing missing".

⇒ **The rows are not unwritten functions. They are functions we define whose
bodies diverged enough to fall below W16-HD's T≥75 identity gate**, so the map
never named them and they never paired. W16-HD's "most of the anon gap is
porting" is right, but the porting is body repair of existing functions.

## 2. The dominant cause: Wii-era message globals vs retail function-local statics

Retail builds `static Message x(Symbol("x"))` inside the function (guard word,
`??0Symbol`, `??0Message@@QAA@VSymbol@@@Z`, `atexit`). The dev-build port
references `extern Message x_msg` from `utl/Messages*.h`, and **those globals are
defined nowhere in the tree**. A scan (`~/tmp/w16hg/msgscan.py`) paired
every unclaimed function of ours referencing `?X_msg@@3VMessage@@A` with an anon
row in the same unit whose retail strings contain `X`. It found 23 pairs; 19 were
converted here. CampaignGoalsLeaderboardPanel's 4 were held back, because each
method has **two** retail rows (a second class in the TU) and the pairing is
ambiguous.

Other retail-vs-port differences fixed on the way:

- **GemPlayer::CheckSolo** has a TU5 block absent from the oracle. On solo entry
  it looks up `ClosestMarkerIdxAtOrAfter(mSyncOffset + ms)` and drops the solo if
  that gem's tick ≥ `endTick`.
- **GemTrainerPanel::Enter** does the gem-list, gem-manager and `mTab->Init`
  setup up front, instead of lazily in `Poll`. Its metronome `Find`s are inline
  arguments (right-to-left evaluation), and it uses a local `static Symbol
  song_name`.
- **CharCache::OnGetPatchTex**: retail has no prefab-customizable path. The dev
  block is gated with the house pattern `#if defined(MILO_DEBUG) &&
  defined(HX_NATIVE)`.
- **GemPlayer Hopo/SwingAtHopo** have a dead `&mStats` materialization. The
  existing house idiom, `Stats *stats = &mStats`, reproduces it.
- **SetlistMergePanel**: `static Symbol` feeding `static Message`, two guards.

## 3. Rows, before and after

"before" is main `57013cc80`, where every row below is an anonymous `fn_` at
fuzzy 0, unpaired. "pre-port" is our unchanged body scored under that name by
scratch-rename (`anon_candidate_scorer.score_candidate`, report.json's ruler),
where it was measured. "after" is `report.json` `fuzzy_match_percent` after a
full build with the map entries.

| retail row | size | name (unit) | before (main) | pre-port body | after | change |
|---|---:|---|---|---:|---:|---|
| `fn_826C17D8` | 852 | `?FilteredWhammyBar@GemPlayer@@UAAXM@Z` (GemPlayer) | anon, fuzzy 0 | 65.58 | 100.00 | 3 local static Messages + numerator-before-PollMs order |
| `fn_826BFD30` | 168 | `?SwingAtHopo@GemPlayer@@UAAXHMH@Z` (GemPlayer) | anon, fuzzy 0 | 38.93 | 100.00 | local static Message; `Stats *stats = &mStats` |
| `fn_826BFE00` | 272 | `?Hopo@GemPlayer@@UAAXHMH@Z` (GemPlayer) | anon, fuzzy 0 | 53.75 | 100.00 | local static Message; `&mStats` after first bump |
| `fn_826C1E28` | 544 | `?CheckSolo@GemPlayer@@QAAXM@Z` (GemPlayer) | anon, fuzzy 0 | 60.49 | 98.44 | local static; TU5 next-gem-before-solo-end gate |
| `fn_82616ED8` | 168 | `?RefreshNewAssetsList@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82616FA8` | 168 | `?RefreshAssetsList@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82617078` | 168 | `?RefreshCurrentOutfitList@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82617218` | 168 | `?ShowLockedDialog@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_826172E8` | 168 | `?ChooseFinish@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_826173B8` | 168 | `?ChooseColors@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82617488` | 168 | `?GotoCustomizeClothingScreen@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82617558` | 168 | `?EnableFaceHair@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82617628` | 168 | `?DisableFaceHair@CustomizePanel@@QAAXXZ` (CustomizePanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82B7C180` | 300 | `?GetSelectedTourDesc@TourDescPanel@@QAA?AVSymbol@@PAVUIComponent@@@Z` (TourDescPanel) | anon, fuzzy 0 | 72.76 | 100.00 | local static Message |
| `fn_82B7C348` | 192 | `?GetInitiallySelectedTour@TourDescPanel@@QAA?AVSymbol@@XZ` (TourDescPanel) | anon, fuzzy 0 | 39.67 | 100.00 | local static; named DataNode temp |
| `fn_82B7C458` | 168 | `?ClearInitiallySelectedTour@TourDescPanel@@QAAXXZ` (TourDescPanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_8269AFD8` | 340 | `?LocalBlowCoda@Band@@QAAXPAVPlayer@@@Z` (Band) | anon, fuzzy 0 | 75.72 | 100.00 | local static Message |
| `fn_8269B158` | 296 | `?WinCoda@Band@@QAAXXZ` (Band) | anon, fuzzy 0 | 72.50 | 100.00 | local static Message |
| `fn_8269B710` | 212 | `?BlowCoda@Band@@QAAXPAVPlayer@@@Z` (Band) | anon, fuzzy 0 | 22.55 | 100.00 | local static Message |
| `fn_826AEDF8` | 452 | `?Swing@RGTrainerPanel@@QAAXH@Z` (RGTrainerPanel) | anon, fuzzy 0 | 73.06 | 100.00 | local static Message |
| `fn_826AF5E0` | 560 | `?PickFretboardView@RGTrainerPanel@@QAAXABVGameGem@@@Z` (RGTrainerPanel) | anon, fuzzy 0 | 44.44 | 100.00 | local static Message |
| `fn_8256BEC8` | 1096 | `?OnGetPatchTex@CharCache@@QAA?AVDataNode@@PAVDataArray@@@Z` (CharCache) | anon, fuzzy 0 | 76.76 | 100.00 | local static; dev prefab path gated out; compare order |
| `fn_826ABEE0` | 484 | `?HandleLooping@GemTrainerPanel@@QAAXXZ` (GemTrainerPanel) | anon, fuzzy 0 | 76.59 | 100.00 | local static Message |
| `fn_826AC9F0` | 1472 | `?Enter@GemTrainerPanel@@UAAXXZ` (GemTrainerPanel) | anon, fuzzy 0 | n/a¹ | 97.02 | TU5 up-front setup moved from Poll; inline Find args; static song_name |
| `fn_8269D3C8` | 196 | `?SendStreak@Performer@@QAAXXZ` (Performer) | anon, fuzzy 0 | 47.84 | 100.00 | local static Message |
| `fn_8269D4B8` | 228 | `?WinGame@Performer@@QAAXH@Z` (Performer) | anon, fuzzy 0 | 46.65 | 100.00 | local static Message |
| `fn_826340C0` | 228 | `?OnMsg@SetlistMergePanel@@QAA?AVDataNode@@ABVLockStepCompleteMsg@@@Z` (SetlistMergePanel) | anon, fuzzy 0 | 47.51 | 100.00 | static Symbol → static Message (2 guards) |
| `fn_8260F2A0` | 168 | `?SetProviders@CharacterCreatorPanel@@QAAXXZ` (CharacterCreatorPanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_8260F4F0` | 168 | `?UpdateOutfitList@CharacterCreatorPanel@@QAAXXZ` (CharacterCreatorPanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_8260F5C0` | 168 | `?RefreshFaceOptionsList@CharacterCreatorPanel@@QAAXXZ` (CharacterCreatorPanel) | anon, fuzzy 0 | 38.95 | 100.00 | local static Message |
| `fn_82B96BE0` | 552 | `?PlayKeyIntros@GemTrack@@UAAXXZ` (GemTrack) | anon, fuzzy 0 | 84.90 | 99.96 | local static Message (in loop) |

¹ `fn_826AC9F0` has no pre-port score: W16-HF (merged in `1c9ccf517` while this
lane ran) independently named it `?Enter@GemTrainerPanel@@UAAXXZ`, the same
identity, so main's target obj no longer carries the `fn_` name. After the
Enter-only edits it scored 81.10, then 96.40 and 97.02 as the TU5 setup was
ported.

28 of 31 rows reach fuzzy 100 = **8,060 B**. All 31 rows total 10,628 B.


Rows deliberately left below 100:

- `CheckSolo` (98.44): retail if-converts the tick test into a 0/−1 mask
  (`xoris/subf/addc/subfe`). Tried `&=`, `&&`, `if (...) inSolo = false`, and
  both operand orders; none reproduces it.
- `GemTrainerPanel::Enter` (97.02): only the guard-address register residue is
  left (retail holds `&guard` in r29 per branch; we hoist `lis`).
- `PlayKeyIntros` (99.96): the only charge is `??2Task` vs retail's ICF
  survivor `??2CriticalSection`. That would need an alias, and none was added
  without retail-byte proof.

## 4. Measurement

`tools/ab_measure.py --patch <main..w16-hg>`, run twice, and both runs agree:

| run | base | Δmatched | Δhonest | Δmasked_equal | Δcode_bytes | Δcode% | Δfuzzy |
|---|---|---:|---:|---:|---:|---:|---:|
| 1 | `57013cc80` (lane base) | **+31** | +28 | +3 | **+8,132** | +0.079360 pp | +0.103535 pp |
| 2 | `1c9ccf517` (rebased, what lands) | **+31** | +28 | +3 | **+8,132** | +0.079361 pp | +0.092548 pp |

- **Predicted vs measured:** predicted +28 fns / +8,060 B (the 28 rows at fuzzy
  100). The extra 3 functions and 72 B are 3 EH funclets that newly pair by bytes
  (Δmasked_equal +3); Δhonest is exactly +28.
- **Why run 2's Δfuzzy is lower:** W16-HF's line already names `Enter`, so the
  rebased patch no longer gets that row's naming credit.
- **Units:** 11 improved, 0 regressed. Units at 100% unchanged (212 mpn / 187
  all-rows-fuzzy).
- **Rebase hazard, recorded:** the rebase applied both lanes' identical
  `0x826ac9f0` insert at different lines and produced a duplicate JSON key.
  `map_name_injectivity` still said OK, because `json.load` collapses duplicate
  keys first. It was caught by an `object_pairs_hook` count and fixed in
  `df85812e4`.

`tools/icf_alias_finder.py --validate`: PASS — 1,479 map-consistent, 250
tolerated, 0 contradicted. `tools/map_name_injectivity.py`: OK (29,514 applied
rows, injective).

## 5. What was not done, and where the rest is

- **Not done:** DuplicatedObject/Scheduler (Quazal, no oracle), RockCentral
  (foreign blocks in the pin), MusicLibraryStore (Xbox-only, no oracle),
  CustomizePanel's `refresh_premium_assets_list` row (TU5-only, and any name
  would be a guess), and the CampaignGoalsLeaderboardPanel pairs (ambiguous).
- **The next vein is measured:** `~/tmp/w16hg/matrix.py` scored every anon
  fuzzy-0 row in the top 40 units against every unclaimed non-template function
  in the same base obj (no shape prefilter). Of 859 targets, the best candidate
  reaches ≥90 on 20,888 B and 70–90 on 38,568 B, many with large margins
  (e.g. `??0BandProfile@@QAA@H@Z` 99.9, `NetSession::Disconnect` 98.6,
  `GemTrainerPanel::CopyGems(int)` 95.0, `GemPlayer::PostLoad` 95.0). These are
  the same class: an identity we hold, with a body that needs repair. Triage of
  the top four found no quick ones. BandProfile's ctor is held back by ICF-fold
  callee names; MetaPanel's ctor needs a layout change (dev-only `HAQManager`,
  `new(0xac)` vs `0xa8`); the other two are register allocation.
- The raw score file is `~/tmp/w16hg/matrix40.json` (not committed).

## 6. Reproduce

```
python3 ~/tmp/w16hg/msgscan.py            # message-global ↔ anon-row pairs
python3 ~/tmp/w16hg/score.py <unit> 'fn_X=?Name@@...'
python3 tools/ab_measure.py --worktree <wt> --patch <main..w16-hg diff>
```
