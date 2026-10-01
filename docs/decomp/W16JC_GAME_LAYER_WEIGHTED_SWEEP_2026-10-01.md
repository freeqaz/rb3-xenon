# W16-JC — game-layer sweep ranked by size × (100 − fuzzy) (2026-10-01)

**Branch** `w16-jc`, rebased onto main `8091a837c`. **Ruler** `name_check` (graded; read from
`report.json` `provenance.diff_config`). Not merged to main.

**Population.** Every row with `fuzzy < 100` in a unit whose `source_path` is under
`src/band3/meta_band/`, `game/`, `bandtrack/` or `net_band/`, ranked by `size × (100 − fuzzy)`.
That covers anonymous `fn_` rows at 0, named rows at 0–90 and named rows at 90–99.99.

Rows whose only charges are relocation-name arguments belong to lane W16-JE (coordinator scope note)
and were left alone. Pure register-allocation residue was skipped after about three spellings.

Classified on main `169512b3e` (`~/tmp/w16jc/pop.py`, `classify.py`). The named rows were classified
by their charged instructions in `objdiff-cli diff`:

| class | rows | bytes | weight |
|---|---:|---:|---:|
| anonymous, fuzzy 0 | 404 | 50,100 | **50,100** |
| anonymous, 0 < fuzzy < 100 (byte-paired EH funclets) | 321 | 13,304 | 340 |
| named, fuzzy 0 | 53 | 5,268 | 5,268 |
| named, 0 < fuzzy < 100, instruction-level charges | 198 | — | **13,878** |
| named, relocation-name-only charges (W16-JE's) | 265 | — | 71 |
| named, register-only charges | 58 | — | 106 |
| named, other argument-only charges (register + reloc, immediates, branch targets) | 26 | — | 45 |
| **total** | | | **69,809** |

Most of the anonymous-row weight sat in five TU5-era classes that have no source anywhere:
- the RockCentral pin (49 rows / 11,316 B, mostly `XboxEntityUploader`);
- `MusicLibraryStore` (17 / 5,224);
- `AppInlineHelp` (17 / 3,760);
- `RetryAudioPanel` (12 / 1,976);
- `ContextChecker` (19 / 1,864).

W16-IB and W16-IC had identified most of them and recorded them as "no body in our tree".

## 1. Result

**Whole-branch A/B, one run:** `python3 tools/ab_measure.py --worktree ~/tmp/w16jc-ab/wt --patch <8091a837c..w16-jc, minus docs and symbols.txt>`.
The patch kinds were configgen + map + source + splits. §7 explains the rig.
- **Leg A** is main `8091a837c` plus `83e8d5b21`, which carries only the split's own over-carve merges.
- Both legs split at a `symbols.txt` fixed point (0 extra re-splits each).
- Leg B recompiled 388 TUs, and the renamer patched 1,861 objects.
- objdiff-cli sha `c1b7d952` was stable across the legs.
- Run dir: `~/tmp/w16jc-ab/wt/.ab_measure_runs/20261001-112615-w16-jc-final-864129/`.

```
leg A: matched=48315 masked=24035 honest=24280 code%=48.756096  (recompiles: 0, settled)
leg B: matched=48716 masked=24175 honest=24541 code%=49.338818  (recompiles: 388, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+401  Δmasked_equal=+140  Δhonest=+261  Δcode%=+0.582722pp  Δcode_bytes=+59712
Δfuzzy=+0.507400pp   (legA 57.369328 -> legB 57.876728)
units at 100% [mpn ruler]: legA 318 -> legB 335  (Δ+17; 17 reached 100, 0 fell off; pairable units 1761->1763)
units at 100% [all-rows-fuzzy ruler]: legA 271 -> legB 284  (Δ+13; 13 reached 100, 0 fell off)
[control none] Δmatched_code=+61856 B (default ruler +59712 B) -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run.** Leg A ≈ main's 48,315 and our tip reads 48,716, so Δmatched
≈ +401 (range +395 to +410). Bytes ≈ wave 1's measured +54,024, plus fork 7's +5,708, plus §3.7's +212
≈ +59.9 KB (range 58 to 61 KB). Measured: **+401 exactly and +59,712 B**.

**Row-level diff of the archived leg reports**, keyed by retail address and run on both rulers
(`~/tmp/w16jc/rowdiff.py`, `rowdiff_mpn.py`):

| ruler | rows up | of which now 100 | rows down | rows off 100 |
|---|---:|---:|---:|---:|
| fuzzy (`matched_code`) | 474 | 425 | 2 | **0** |
| `mpn` (`matched_functions`) | 442 | 401 | 3 | **0** |

The rows that went down are explained:
- `ProfileMgr::GetSongToTaskMgrMs` 13.33 → 0 (48 B). Our body is now retail's full 108 B, and dtk carves its
  switch tail into three `fn_` rows pinned to CharServoBone (§3.4).
- `TrainerGemTab::Render`'s old key 0x826EFC98 (mpn 4.09 → 0). That address is the ctor's mis-carved tail,
  and the name moved to Render's real start, 0x826F0170, where it reads 99.92.
- TransAnim `_M_allocate_and_copy<Key<Vector3>>` 99.96 → 99.76. Its callee is now named as the TrackData
  copy, which scores 100 against that body (3.7 for the `Key<Vector3>` one).

Five units read lower in matched count: AppInlineHelp −16, RetryAudioPanel −4, and FreestyleMoveRecorder,
Mesh and SelectDifficultyPanel −1 each. Every one of them is a re-home that moved rows into the unit that
defines them (AuditionSessionPanel, JoinInvitePanel, AssetStore, MultiplayerAnalyzer, SaveLoadStatusPanel).
The address-keyed diff shows no row losing score.

**The population.** Weight fell from **69,809 to 23,755 (−66%)** (§5).
- The relocation-name class (weight 71) also shrank because W16-JE landed on main, so that part of the drop
  is not this lane's.
- **Wave 1 alone** (forks 1–6) measured **+358 fns / +237 honest / +54,024 B** in its own A/B, on main
  `bd4ae22ec`, with 0 rows off 100 on fuzzy. That run is in
  `~/tmp/w16jc-ab/wt/.ab_measure_runs/20261001-101638-w16-jc-wave1-323517/`.

## 2. Method

1. **Seven forks**, each with its own worktree and branch. Wave 1 (forks 1–6) started from main
   `169512b3e` with disjoint scopes:

   | fork | scope |
   |---|---|
   | 1 | RockCentral / EntityUploader anonymous rows and `net_band` instruction rows |
   | 2 | MusicLibraryStore, AppInlineHelp, RetryAudioPanel, ContextChecker anonymous rows |
   | 3 | every other anonymous fuzzy-0 row, plus the 53 named fuzzy-0 rows |
   | 4 | `meta_band` instruction rows (outside fork 2's files) |
   | 5 | `game` instruction rows outside the vocal cluster |
   | 6 | `bandtrack` and the vocal cluster (Singer, VocalPlayer, VocalPart, Tambourine*, GameMic*) instruction rows |

   Fork 7 (wave 2) started from the rebased wave-1 tip and took the anonymous rows of 100 B or more that
   were still left.
2. **Per-change check.** Each fork confirmed every change with a full `./tools/ninja-locked` build and a
   whole-report row diff keyed by retail address (`~/tmp/w16jc/rowdiff.py`, from W16-IC), never with
   `run_objdiff` alone.
3. **Merge.** Each fork landed on `w16-jc` with `git merge --no-ff`.
   - `target_symbol_map.json` conflicts were resolved key by key (`~/tmp/w16jc/resolve_map.py`, W16-ID's
     resolver). It refuses any key that both sides changed differently, and it refused none.
   - **Interaction check.** The six wave-1 forks merged to **+358 fns / +53,176 B** against `169512b3e`.
     The sum of the fork measurements is +359 / +52,836. Three rows account for the difference
     (`~/tmp/w16jc/interact.py`), all of them positive or double counts:
     - `GetSloppyPitch` reaches 100 only with fork 3's naming and fork 6's body fix together: +436 B,
       +1 fn.
     - Both forks 3 and 6 named and counted `GetVolumeParam` (0x826E3C88, 96 B).
     - `_M_copy_from<hash_map<Symbol,String>>` took fork 4's value.
4. **Rebase** with `--rebase-merges`: onto `bd4ae22ec` after wave 1, then onto `8091a837c` after fork 7 (§6).

## 3. What was fixed, by fork

Fork figures are each fork's own full build against `169512b3e`, with an address-keyed row diff. §1's
A/B is the authoritative aggregate. Function names on the TU5 classes are ours, because retail keeps
no symbols. Each comment says so and cites retail addresses only.

### 3.1 Fork 1: RockCentral's TU5 code (+98 fns / +12,748 B; 125 up, 0 off 100)

- **`XboxEntityUploader` is a new TU written from retail bytes**, block 0x82508708–0x8250A510 (RTTI
  `.?AVXboxEntityUploader@@`, vtable 0x82085054, 29 slots).
  - Retail sends names through XStringVerify in batches of 10 before the RockCentral ops.
  - `EntityUploader` loses its user-declared dtor.
  - +51 fns / +7,220 B. 12 of the 13 named rows read 100, including `UpdateFromProfile` (1,736 B).
- **`RockCentral::Init`** is retail's Xbox body at 0x824F9CE0 (640 B → 100).
- **`RockCentral::Terminate`** is cut to retail's three RemoveSinks (208 B → 100).
- **`GetIsDiskSong`** uses two signed compares (85.3 → 100).
- **TU5 art-file transfer code**, which no surviving source has: `SaveBinaryData`, `GetArtFile`,
  `SaveArtUpdater`, `ArtFileConverter` and `Quazal::RBBinaryBuffer` (+16 fns / +2,424 B).
- **XboxServer.** Cutting `Terminate` to retail's body stopped the RockCentral object from emitting
  `SigninChangedMsg::Type`, whose 100 had been a false pairing. Retail's only caller of `Type` is
  `XboxServer::Handle` (0x823ED2A8), which lives in the Server TU.
  - Fork 1 wrote the minimal XboxServer code retail keeps there, in `Server.cpp`: the ctor, `GetPlayerID`,
    `OnMsg(SigninChangedMsg)`, `Handle`, and `SetLSPLoginFilter` (descriptive name).
  - It re-homed 0x823EC538–0x823EC788 to Server.cpp and pinned 0x823ED1E8–0x823ED458 there.
  - All 10 rows in those blocks now read 100 (+10 fns / +1,116 B), and `Type` pairs again on its real
    caller.
- **Map corrections:**
  - 0x823F1028 was `??0RBDataClient`; it is `ByteCode@JoinResponseMsg` (retail vtable slot 6).
  - 0x824F9388 was `GamePanel::OnStartLoadSong`; it is ArtFileConverter's texture loader.
  - The name at 0x824F6878 (`??1DeleteUserCompleteMsg`, a Quazal buffer dtor) was cleared.
- **Re-homes:**
  - 0x822CEEE8 → BandScoreboard;
  - 0x823F0FC0 and 0x823F1028 → SessionMessages;
  - 0x824F9380 → RockCentral;
  - 0x823EC538 → Server.cpp.

### 3.2 Fork 2: MusicLibraryStore, AppInlineHelp, RetryAudioPanel (+92 fns / +12,636 B; 0 down)

- **`MusicLibraryStore` is written from retail**, and the undefined `MusicLibraryUnkOp` stub is gone
  (`MusicLibrary::unk19c` is now a `MusicLibraryStore*`).
  - Bodies: LoadOffers, IsDownloading, LoadStoreArt, SetStorePreview, ShowPurchaseError,
    OnMsg(ContentInstalledMsg), Handle, ParseOffers, PurchaseSongs, Poll.
  - Also `MusicLibrary::RefreshStoreDisplay` (0x8253F050) and `StorePreviewMgr::DownloadPreviewFile`
    (0x827B2350).
  - **Behaviour:** the ctor and ClearPreview register with `ThePlatformMgr`, not `TheContentMgr`
    (94.6 / 92.8 → 100).
- **`AuditionSessionPanel`** (TU5 Rock Band Network audition screen) is a new TU, pinned
  0x826033F0–0x82603A78 and 0x82603A84–0x826047A0. `AuditionMgr.h` declares only the surface the panel
  uses.
- **`JoinInvitePanel`** moves out of MetaPanel.cpp into its own TU, pinned 0x826308B8–0x82631A20. Every
  row reads 100.
- **Utl.**
  - `MaxAllowedHmxMaturityLevel` (0x825BE310) is written from retail.
  - The font-char helpers at 0x825BE650, 0x825BEE58 and 0x825BE760 are named.
  - Naming them exposed a **rotation** of the three `On*` handler names at 0x825BEDE8, 0x825BEEF0 and
    0x825BF050. Our `UtlInit` also registered them in the wrong order. Both are fixed from retail's
    registered strings.
  - `AllowedToAccessContent` goes 16.1 → 100.
- **Deliberately unnamed:**
  - The deleting dtor at 0x82603C10. Naming it would drop CampaignSongInfoPanel's shared `??_E` thunk, and
    that fold cannot be proven.
  - 0x825BD390, a vector-erase survivor with 9 callers.

### 3.3 Fork 3: anonymous and named-0 rows (+72 fns / +7,536 B; 94 up, 0 off 100)

- **Identification.** W16-IB's caller-alignment and RTTI vtable sweeps were re-run on today's tree.
- **About 90 map keys.**
  - 79 names added.
  - 16 stale template / `??_E` spellings that no object defined were renamed.
  - `GetLane` moves from 0x826EECE8 to 0x826EECE0. `TrainerGemTab::Render` moves from 0x826EFC98, the
    mis-carved ctor tail, to 0x826F0170.
  - Three keys on carved-off return stubs were nulled.
- **Five `.text` re-homes:**
  - two Mesh blocks → MultiplayerAnalyzer;
  - the Prune tail and `~vector<SingerStats>` → PerformanceData;
  - 0x826F6840 → Singer;
  - 0x82631EA8 → SaveLoadStatusPanel.
- **About 35 bodies repaired or written from retail.**
  - `SaveLoadManager::HandleEventResponse` (720 B, 80.3 → 100) had wrong case values, and
    `SaveLoadErrorSetState` loses its ManualLoad arm.
  - `ParseConfigData` parses the config `DataArray` as retail does.
  - `SetProGuitarOrBassSongs` was an empty body (now 99.7).
- **`symbols.txt`.** The naming let jeff merge carved-off tails, and the split's own rewrite is committed
  (`f996d88ca`). There are no hand edits (§7).

### 3.4 Fork 4: `meta_band` instruction rows (+34 fns / +7,308 B; 40 up, 1 down, 0 off 100)

- **Behaviour fixes to retail** (each now at 100 unless noted):
  - `BandSongMgr::ContentMounted` (1.7 →) and `LicenseMgr::ContentMounted` (1.3 →).
  - `MusicLibrary::GetStoreOffers`: it was empty; retail returns every store offer whose song is not
    installed.
  - `MusicLibrary::RebuildProfileData`: rebuilds the filtered list when bad reviews are hidden.
  - `OvershellSlot::SwapUserProfile` and `ConfirmSwapUserProfile` return `void`.
  - `ConfigureAccomplishmentData`.
  - `CriticalUserListener::OnMsg(LocalUserLeftMsg)` does not clear `mCanSaveData`.
  - `SessionMgr::OnMsg(SigninChangedMsg)`.
  - `ModifierMgr::ToggleModifierEnabled`.
  - `SongStatusMgr::ClearLeastImportantSongStatusEntry`, with the new `RemoveSongStatus` (0x825D30D8).
  - `BandSongMgr::AddSongData(DataLoader)`.
  - `CustomizePanel::ContentDone`.
  - `LayerProvider::GetMatForData` rotates about Z and scales by the layer aspect (37.4 → 86.7).
  - `CalibrationPanel::UpdateProgress` has no `(n+2)/n` factor and no clamp (46.3 → 82.6).
- **Function-local `static` Symbols.** Thirteen rows went to 100 this way (`SetHair` / `SetGlasses` to 98.82), including
  `InitializeTourSafeDiscSongs` (1,076 B). W16-IB had recorded that row as "a different pattern from the
  statics fix", but it is the statics fix.
- **Map corrections.**
  - 0x825A07E0 / 0x825A05D8 are the `hashtable<Symbol,String>` copy ctor and `_M_copy_from`, not
    `<Symbol,float>`. All three retail callers copy-construct `vector<hash_map<Symbol,String>>`.
  - `CustomizePanel::RefreshPremiumAssetsList` (0x82617148) is written and named.
- **The one row down.** `ProfileMgr::GetSongToTaskMgrMs` goes 13.33 → 0 (48 B). Our body is now retail's
  full 108 B (70/55/35/15 ms for the four practice speeds). dtk splits its switch tail into three `fn_`
  rows pinned to CharServoBone, so it needs a `symbols.txt` merge.

### 3.5 Fork 5: `game` instruction rows outside the vocal cluster (+30 fns / +5,620 B; 32 up, 0 down)

- **`PresenceMgr::UpdatePresence`** is written from retail 0x82680A20 (41.7 → 100).
  - It tests `LocalUser::IsSignedIn`, remaps context 0 to 0xe, and truncates the song title at 22
    characters with `"..."`.
  - Three `PlatformMgr` LocalUser wrappers are named at 0x82514BD0, 0x82514C18 and 0x82514C60.
- **Behaviour fixes:**
  - `GemPlayer::SetPaused` now calls `Player::CountPause` (7.1 → 100).
  - `Player::GetNumStars`, `GetNumStarsFloat` and `GemPlayer::GetNumStars` read the individual score.
  - `Player::Restart` resets the pause counter, so pauses no longer accumulate toward the quarantine at 10.
  - `PracticePanel::Poll` compares music speed with 1.0f, not −1.0f.
  - `UpstrumPercentStatMemberTracker::GetStatValue` tests `> 0`.
- **Shape fixes.**
  - `GemTrainerPanel::Poll` drops the lazy init that retail does in Enter (48.6 → 100).
  - The PerfectSectionTracker / Focus / Overdrive trackers use function-local statics (10 rows).
  - `SymToControllerType`, `FindHeldNoteFromSlot` and `GetLastGameGemInSection` are fixed.

### 3.6 Fork 6: `bandtrack` and the vocal cluster (+33 fns / +6,988 B; 42 up, 0 down)

- **`VocalPlayer::OnMsg(ButtonDownMsg)`** follows retail's TU5 vocal-volume dispatch (30.7 → 100). Its
  callees are written from retail bytes: `HandleActivateVolume`, `HasSingerOnMic`,
  `VocalTrackDir::ActivateVolume` / `ChangeVolume` / `SetVolumeSlider`, and `Singer::GetMicID`.
  - 0x826F6930 was mapped as `SyncLocalMachineMsg::Dispatch`. It is `GetMicID`, so the key is corrected
    and the block re-homed to Singer.
- **Behaviour fixes:**
  - `GetSloppyPitch` returns `outPitch = ms`.
  - `SuddenOctaveShift` returns 0 for an in-range pitch; ours always returned at least ±1.
  - `DrawBeatLine` had its key-shift arrow widgets swapped.
  - **Native:** `m10_support.cpp` defined the `kInvalidPitch` shim as **+1000.0f**, with the wrong sign. It
    is replaced by the real `static const float VocalPlayer::kInvalidPitch = -1000.0f` (retail `.rdata`
    0x820F14B4).
- **Rows to 100** include the Singer ctor, ComputeTambourinePoints, UpdateFills, TambourineManager::Handle,
  UpdatePitchHistory, AllScoresAreIn, AddHopoTails, CrowdRatingDefaultVal and ClearTrackMasks.
  `TambourineManager.cpp` gets `/DRB3_HANDLE_LOCAL_STATIC`.

### 3.7 Lane edit: SessionMessages' funclets out of RockCentral's pin (+3 fns / +212 B)

0x823F27B8–0x823F2898 lies between SessionMessages blocks. It holds five unwind funclets of SessionMessages
functions; the first is `??0JoinRequestMsg`'s.
- Filed under RockCentral, they paired by byte signature with unrelated RockCentral funclets.
- Fork 1's rewrite removed the funclet `fn_823F27B8` had been paired with, so its `mpn` fell 100 → 99.8 in
  the wave-1 A/B. That was the only row off 100 on either ruler.
- Re-homed to SessionMessages (`9d9fabe67`), all five read 100 on both rulers.

### 3.8 Fork 7 (wave 2): remaining anonymous rows of 100 B or more (+43 fns / +5,708 B; 48 up, 0 down)

Fork 7 started from the first-rebase wave-1 tip (`425c73794`, now `d235d39aa`). Every body below is written from retail asm.

- **`PresenceMgr::Init`** (0x82680DC8, 756 B → 100). It reads the `presence_mgr` config and registers five
  sinks. Our source had no Init at all.
- **`PremiumAssetProvider`** (TU5).
  - Text, UpdateExtendedText, the dtor, `??_G`, the ctor (0x82670A88) and its funclets → 100.
  - It moves out of CustomizePanel.h into AssetProvider.h.
  - Map: 0x82670B90 was named `??_EAssetProvider@@W3`, but it is Premium's adjustor thunk. AssetProvider's
    real thunk is 0x826714D0. Both keys were corrected.
  - **`AssetMgr::GetPremiumAssets`** (0x8256B570) picks Premium-boutique assets of one gender, then sorts.
  - The ctor and its funclets were the only blocks of `HamSupereasyData`, a DC3 unit name. They move to
    AssetProvider and that split entry is removed.
- **`BandSongMetadata::IsDLCOrUGC`** (0x8259E5B0, 332 B → 100). `AddSongData` calls it on `Data(songID)`,
  as retail does. This replaces a zero-argument `RB3AddSongDataUpgradeGate` extern and its weak native stub.
- **New `AssetStore` TU** (0x825EC840–0x825ED2A0, RTTI `.?AVAssetStore@@`). 12 rows named, 10 at 100. The
  DC3-named `FreestyleMoveRecorder` entry that held its blocks is removed.
- **New `BandMemcardAction` TU** (0x825D77D8–0x825D7AD8) for `Save/LoadMemcardAction`.
  - They were declared bodiless in SaveLoadManager.cpp, with an `Action()` slot that retail does not have.
  - Both ctors, Save `PreAction`, `??_G` and two funclets → 100.
  - Adds `FixedSizeSaveableStream::GetSymbolCount()`.
- **MetaPanel rows.**
  - The AuditionSessionPanel / CalibrationWelcomePanel `NewObject` and `??0CalibrationWelcomePanel` are
    named, and `~NextSongPanel` → 100.
  - The ctor and dtor are implicit, because retail emits them inline.
  - An attempt to move the ctor out of line took 11 rows off 100 and was reverted before commit.

### 3.9 Lane edit: the PlatformMgr members W16-JD also added (`78a2abc42`, de-duplication)

Forks 4 and 5 added `PlatformMgr::SwapUserPads` (0x8251D6C8) and the `SetUserContext` / `SetUserProperty`
/ `SetUserPresence` forwarders (0x82514BD0 / 0x82514C18 / 0x82514C60). Main's W16-JD added the same four
in parallel, with the same addresses and identical forwarder bodies, and wrote `SwapUserPads` in
`PlatformMgr_Xbox.cpp`.

The second rebase auto-merged both copies, and MSVC rejected the redeclarations (C2535). Main's copies are
kept and ours are removed.

### 3.10 `VocalNote::PitchAt`: main and fork 6 wrote the same function

Main's W16-JD and fork 6 independently made 0x826F16E0 an inline member, with different operand orders.

Retail computes `(1 − t) * begin + end * t`: a `fmuls f0, f8(end), f0(t)`, then `fmadds f1, f13(1−t),
f9(begin), f0`. That is fork 6's spelling, so fork 6's body is kept with a comment that combines both
lanes' facts.

On main the row read 0. Main's VocalPart.cpp never called the helper, so the COMDAT was not emitted, and
main's body was never scored.

## 4. Alias folds

Two new groups. Both re-chase **CHASED T1 PROVEN with no CYCLE-ASSUMED leaf** on main's post-W16-JG
adjudicator (`tools/icf_pair_adjudicate.py --chase`, after the rebase):

| survivor | folded | address | lane |
|---|---|---|---|
| `?OnMsg@AuditionSessionPanel@@…SigninChangedMsg` | `…ProfileSwappedMsg` overload | 0x82603D90 | fork 2 |
| `??1_DDL_RBBinaryBuffer@Quazal@@UAA@XZ` | `??1RBBinaryBuffer@Quazal@@UAA@XZ` | 0x824F6878 | fork 1 |

- **AuditionSessionPanel.** Both arms of retail `AuditionSessionPanel::Handle` (0x82603DE0) call 0x82603D90.
- **RBBinaryBuffer.** The retail call site is `SaveBinaryData`'s EH funclet `fn_824F8CC0`. The flat T1 check
  is VACUOUS for this 24-byte destructor. The chase decides it: VACUOUS-BUT-IDENTICAL, once the two
  vtables were named from their retail RTTI.

No other alias membership was added.

## 5. Rows left, by blocker

Remaining population on the final tip, the same instrument as the opening table:

| class | rows | bytes | weight |
|---|---:|---:|---:|
| anonymous, fuzzy 0 | 205 | 16,728 | 16,728 |
| named, fuzzy 0 | 30 | 3,164 | 3,164 |
| named, instruction-level charges | 131 | — | 3,477 |
| everything else | | | 386 |
| **total** | | | **23,755** (was 69,809) |

- **TU5 code with no source, identified but not written.**
  - `AutoplayAuditionUser : NullLocalBandUser` (RTTI; about 1.2 KB at 0x8268EB10–0x8268F0D0, including about
    a dozen vtordisp thunks). Fork 7's turn budget ran out.
  - The rest of XboxServer: about 9.8 KB of Quazal login code in the Server TU.
  - RockCentral's Quazal RBBinaryData DDL client (0x8250A510–0x8250AF88) and its type-check helpers. This is
    Quazal middleware, so it is out of scope.
  - ContextChecker's 19 rows. They are XboxSession/XSessionData (0x823EF498), StreamReader/XMAReader
    (0x82B6A384) and Quazal MessageBroker (0x823F4268), and our tree has no source for any of them.
  - BandSongMgr's erase-by-constant-key helper (`fn_82577480` + `fn_82576B70`), which has no name.
- **dtk mis-carves that need a `symbols.txt` merge.** Our source is right in each.
  - `ProfileMgr::GetSongToTaskMgrMs` (§3.4).
  - `PerformanceData::Prune`'s tail `fn_82657570` (82.2).
  - `CrowdRating::GetThreshold`'s two return stubs.
  - The jump-table bodies of `GetConfigNameFromAssetType` / `GetAssetTypeFromCurrentState`.
  - The `TrainerGemTab` ctor and `BandUser::DeletePlayer`.
- **Wrong map names that were found but not settled.**
  - 0x8268EF78 is named `RemoteBandUser::GetLocalBandUser`, but its body has a deleting-dtor shape.
    Retail's RemoteBandUser vtable slots 6 and 7 both point to 0x8268B8A8.
  - The `BandUser::SyncProperty` `$4` thunk has a different adjustor and target in retail.
  - `ObjPtrList<RndGroup>::push_back` at 0x822750D0 has no clear true identity.
- **Fold survivors deliberately left unnamed**, because naming them would charge call sites that are
  forgiven today:
  - 0x826C32A8 (HeldNote / PlayerTrackConfig);
  - 0x8256A220 (`GetAsset`), which was tried, took five rows off 100, and was reverted;
  - the `_M_find` at 0x826D98D8 (26 callers);
  - 0x825BD390 (9 callers);
  - the 12-byte `Accomplishment::GetAward` / `GetPassiveMsgChannel` survivors;
  - AuditionSessionPanel's `??_E` at 0x82603C10.
- **Relocation-name-only residue (W16-JE's class).**
  - Fold names in AssetStore `ParseOffers` / dtor, `LoadMemcardAction::PostAction`, MusicLibraryStore
    Poll / PurchaseSongs / ParseOffers, and `vector<FlowMathOp>::resize` behind `ObjVector::resize`.
  - `--chase` REFUTED the MusicLibraryStore `push_back` fold, so no alias was added there.
- **Register or scheduling residue, after about three spellings each:** GetSloppyPitch's siblings, `SetAssignedPart`,
  `CalcPhraseScoreMax`, `HandlePhraseEnd`, `DrawFill`, `MultiplayerAnalyzer::AddGems`, `Tracker::Restart`,
  `DeployCountTracker::Poll_`, `HandleExitExtent`, `UpdateProgress` (fmadd/fnmsub), `GetMatForData`,
  `SaveLoadManager::OnMsg(MCResultMsg)` (switch lowering), `SetHair` / `SetGlasses`,
  `ApplyStringVerifyResults`, `FailAllContexts`, `DataPointToQString`, `UpdateSetlist`.
- **Unknown source construct:**
  - `SongDB::PostLoad`: an EH frame for an unidentified object.
  - `TrackerSectionManager::GatherSections`: an 8-byte struct copy.
  - `GemTrainerPanel::ShouldMissCauseFail`: a non-const `GetSection`, which would change the mangled name.
  - `ModifierMgr::IsHidden` / `IsActive` / `IsModifierActive`: an empty r31 frame prologue.
  - `NewObject@AppScoreDisplay` (86.9) needs an engine `operator new` change in ScoreDisplay.
- **Large rows not opened:** `VocalTrack::UpdateScrolling` (8,948 B at 95.27; W16-ID §4 explains why),
  TrackPanel::Poll, Gem::Poll, Singer::Poll_, GameMic::ThreadProcessOneFrame.

## 6. Rebases onto main and how each conflict was resolved

**First rebase (after wave 1):** 84 commits were replayed over 41 new main commits: W16-JE, the
comment-only cleanup `a9864a8fe`, and W16-JG. `target_symbol_map.json` stops were resolved key by key (`~/tmp/w16jc/step.sh` automates them).
The other conflicts:

| file | resolution |
|---|---|
| `MusicLibrary.{h,cpp}`, `MusicLibraryStore.h` (fork 2's commit) | Our code was kept. Main's new comments described the `MusicLibraryUnkOp` stub that fork 2 retires ("Still NOT repaired: unk19c is declared MusicLibraryUnkOp*"), so taking them would have left false comments. The comments now follow main's style, with no rb3-Wii mention, and state the new facts (`LoadOffers`, `ClearPreview`, `unk19c` is a `MusicLibraryStore*`). |
| `MetaPanel.cpp` (JoinInvitePanel moved to its own TU) | took the deletion; main's comment edit applied to the moved class |
| `RockCentral.cpp` (Terminate reduced to retail's three RemoveSinks) | took the deletion of the non-retail lines |
| `VocalPart.cpp` (the `extern "C"` kInvalidPitch shim replaced by a real `static const`) | took the deletion |
| `MusicLibrary.h` when fork 4's merge was recreated | kept the side without the stub, as in the original merge |
| `splits.txt` when fork 3's merge was recreated | kept both forks' adjacent Singer.cpp blocks in address order (0x826F6840–0x826F6930 from fork 3, 0x826F6930–0x826F6944 from fork 6) |

**Second rebase (after fork 7)**, onto `8091a837c` (W16-JD's engine sweep). The conflicts:

| file | resolution |
|---|---|
| `splits.txt`, JoypadMsgs | W16-JD cut 0x82532DC8–0x82533120 out to VirtualKeyboard_Xbox, and fork 2 re-homed 0x825BD1F8–0x825BD254 out of JoypadMsgs. Both were kept; the split re-derived `.pdata`. |
| `VocalNote.h` | W16-JD and fork 6 wrote the same `PitchAt`. Fork 6's operand order is retail's (§3.10). |
| `MusicLibrary.h`, `splits.txt` (Singer), when forks 4 and 3's merges were recreated | same resolutions as the first rebase |
| `PlatformMgr.{h,cpp}`, no textual conflict | duplicate members after the auto-merge; main's copies kept (§3.9) |

After the rebase, 17 added comments still said "no oracle has this", "absent from both oracles", or
"the rb3-Wii dev branch only carries an empty stub". All were reworded to "no surviving source"
(`d235d39aa`, comment-only). The one remaining hit names the real classes `WiiProfileMgr`/`WiiFriendMgr`
to say retail's Xbox `Init` sets up none of them. It does not cite rb3-Wii.

## 7. Measurement notes

- **The first wave-1 A/B was refused, and it was right to refuse.** Fork 3's names let jeff merge
  carved-off tails, which rewrote `symbols.txt` (`f996d88ca`). `ab_measure` refuses a patch that touches
  `symbols.txt`, so leg B split from main's `symbols.txt`. The merge then rewrote it, and the split guard
  (`scripts/verify_split_current.py`) failed the build.
  - The fix follows W16-IB/IC §5. Leg A is main plus one commit carrying only the split's own over-carve
    merges (`418d99db2` on branch `w16-jc-ab-base` for the wave-1 run, `83e8d5b21` on `w16-jc-ab-base2` for the final one). That commit has no map names, so both legs split
    from the same `symbols.txt`.
  - Leg A therefore already contains whatever the merges do on their own, and the Δ excludes it.
- **Leg B re-runs a bare `configure.py`**, because the patch adds TUs. In a `~/tmp` worktree that would
  resolve dtk and objdiff to stale copies under `~/tmp/jeff` (W16-IB §7). The A/B worktree therefore lives
  at `~/tmp/w16jc-ab/wt`, next to `jeff` and `objdiff` symlinks to the live forks, as in W16-IC.
- **`ab_measure` did not restore its worktree after two MEASURED runs.** It printed
  `COULD NOT RESTORE THE WORKTREE ... action: restored`, but 89 and then 101 tracked files were still modified. The
  banner names only `splits.txt` as expected. The numbers are unaffected, because each run reads its own
  legs, but the next run in that worktree needed a manual `checkout -- .`. This is a tool defect and is
  filed here, not fixed.
- **The row check runs on both rulers.** The wave-1 A/B read 0 rows off 100 on fuzzy. On `mpn`, one
  funclet fell from 100 to 99.8 (`fn_823F27B8`, 40 B), and `9d9fabe67` fixes it (§3).

## 8. Gates (final tip)

All four were run on `78a2abc42`, the last code commit. The first three ran after its full build, and the
native gate ran last, after the final A/B:

```
[map-injectivity] OK: 32644 applied rows, 32643 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1610 map-consistent, 267 tolerated (enumerated above), 0 contradicted, 1878 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The only commit after them is this docs-only one, which touches no build input. The native gate also
passed on the first-rebase tip (`425c73794`, before the second rebase) and on the tips of forks 2, 4, 6
and 7.

## 9. Not done

- Not merged to main, per the brief.
- No hand edits to `symbols.txt`. The only `symbols.txt` change is the split's own rewrite after naming
  (`f996d88ca`). The mis-carves in §5 are recorded, not fixed.
- Relocation-name-only rows were left to W16-JE. Of the 265 such rows at `169512b3e`, 118 already read 100
  on main by the time of the final A/B. Two more reach 100 only on this branch, as a side effect of naming
  and source work, and are kept:
  - `vector<PlayerScoreInfo>::_M_insert_overflow_aux` (324 B);
  - the `??_EUIEventMgr` `$4` thunk (12 B).
- The permuter was not run.
- `VocalTrack::UpdateScrolling` (8,948 B at 95.27) was not attempted, as in W16-ID.
