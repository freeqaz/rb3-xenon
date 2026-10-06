# W16-PJ: native drivers link the real SongDB, CommonPhraseCapturer and Utl.cpp (2026-10-06)

Branch `w16-pj`, worktree `~/tmp/wt-w16-pj`, based on main `e095c7ab7`.

**Brief.** W16-PD (`W16PD_NATIVE_STUB_RECONCILIATION_2026-10-03.md` §5) tried
three replacements and backed them out. This lane takes them over and links
them, resolving the link conflicts instead of stubbing around them:

1. the real `src/band3/game/SongDB.cpp` in the scoring drivers;
2. the real `CommonPhraseCapturer` in rb3-score2/3;
3. `src/band3/meta_band/Utl.cpp` in rb3-song.

The milo-native-engine pin is unchanged, and no engine change request was
needed.

## 0. Headline

| | before (main `e095c7ab7`) | after |
|---|---:|---:|
| stub/shim TUs with authored definitions (census) | 20 | **18** (`m6_link_stubs.cpp` and `m10_link_stubs.cpp` deleted) |
| distinct (file, symbol) authored stub definitions | 1,220 | **981** (−239) |
| distinct stub symbols | 1,004 | **969** (−41 retired, +6 helpers / real-body hooks) |
| CPU targets running rc=0 with native_health inputs | 16 / 16 | **16 / 16** |
| `src/` files changed | — | **0** (the X360 match build is untouched, so no `ab_measure` run was owed) |

All three items are **linked**:

| item | targets | status |
|---|---|---|
| real `SongDB.cpp` + `MultiplayerAnalyzer.cpp` | score2, score3, score4, vocal, vocal2, harmony, crowd | **linked and driven.** The synthetic SongDB is deleted. |
| real `CommonPhraseCapturer.cpp` | score2, score3 | **linked.** score3 now drives it per gem and checks its credit against its own verdict. |
| `meta_band/Utl.cpp` (+ `game/Defines.cpp`) | rb3-song | **linked** under `--gc-sections`. A new gate runs the real `IsRestricted` over all 138 songs. |

## 1. Instruments

- **Before/after outputs.** `~/tmp/w16pj/runall.sh <buildDir> <outDir>` runs
  every CPU target with `tools/native_health.sh`'s own inputs and captures
  stdout, stderr and rc:
  - assets: the ark;
  - charts: vicarious, pills and centerfold;
  - `songs.dta`;
  - the `songs.dtb` reference.

  The baseline is the binaries built from the worktree before any edit
  (`~/tmp/w16pj/base-bin`, run into `out-base`). After is the full rebuild at
  `1e6bf8281` (`out-new`). Diffs normalise pointer values (`0x` plus 8 or more
  hex digits). Without that, gem/hit/score "differ" only in an ASLR'd
  `tempoMap=0x…` print.
- **Stub census.** `tools/native_stub_census.py <build> <json> --authored`:
  - main's copy of the tool run over main's native build (the before column);
  - this branch's copy run over this build (the after column).

  The only change to the tool is dropping the two deleted files from its file
  regex.
- **Controls.** Two of the new behaviours were run against a deliberately
  broken leg to show they can fail (§2.2, §2.3).

## 2. What changed

### 2.1 Real SongDB in the scoring drivers

**The W16-PD blocker** was 26 duplicate definitions against `m8_support.cpp`'s
synthetic SongDB, plus 2 undefined symbols. The fix was to delete the
synthetic layer, not to work around it. These are gone:

- `SongDB` ctor/dtor;
- `GetSongDurationMs`;
- `GetPhraseID` (via `gM8GemPhrase`);
- `GetGems`;
- `GetCommonPhraseTracks` (it returned `1 << track`);
- `IsUnisonPhrase` (it returned `false`);
- `GetCommonPhraseID` (it returned `-1`);
- `GetBaseScores`;
- the seven `gM8*` globals;
- ten one-line `SongDB` stubs in `m8_link_stubs.cpp`;
- `SongDB::ChangeDifficulty` in `m10_leaf_stubs.cpp`;
- the `SongDB` vocal overrides in `m10_support.cpp`.

`native/CMakeLists.txt` gains `SONGDB_SOURCES` (`SongDB.cpp`,
`MultiplayerAnalyzer.cpp`). It is defined before the M6 block, because score2
consumes it.

**Bring-up, mirroring the real Game.** Each driver does the following:

1. `NativeMakeGame()` creates a calloc'd `Game` and sets `TheGame`. Its members
   are set as follows:

   | member | value |
   |---|---|
   | `mEnableStreak` | true |
   | `mEnableOverdrive` | true |
   | `mAllowOverdrivePhrases` | true |
   | `mEndWithSong` | false |
   | `unkdc` | −1 |

2. `TheSongDB = new SongDB()`. The real ctor creates the `SongData`, registers
   itself as the parser sink, and builds `MultiplayerAnalyzer(mSongData)`.
3. The driver parses **into** `*TheSongDB->GetData()`.
4. It registers a real `PlayerTrackConfig` with
   `AddConfig(guid, trackType, Expert, 0, false)` and calls
   `NativeMakeGameConfig`. That gives a calloc'd `GameConfig` carrying only
   `mPlayerTrackConfigList` and `mSongLimitMs`.
5. `SongData::PostLoad(list)` runs **for real**. It fires `SetNumTracks`,
   `AddTrack` and `AddPhrase`, then runs `PhraseAnalyzer::Analyze` and
   `PostLoadVocals`.
6. `NativeSongDBPostLoad` runs the parts of `SongDB::PostLoad` that headless can
   run: `SetupPhrases`, `DisableCodaGems` and `RunMultiplayerAnalyzer`.

**What is skipped, and why.** `ParseEvents` and `SetupPracticeSections` read
`TheGame->GetBeatMaster()->GetMidiParserMgr()->GetEventsList()`. The drivers
call `SongParser` directly and have no BeatMaster. So the song duration is
driver-supplied, and `mCodaStartTick` stays −1 (no coda).

**The one substitution.** `MultiplayerAnalyzer::AddUser` takes the difficulty
from `TheBandUserMgr->GetBandUser(guid)`. When there is no BandUser it falls
back to **Easy**, and headless there is never a BandUser. That fallback is not
cosmetic: `Scoring::ComputeStarThresholds` sets the gold threshold to
999999999 unless every base score is Expert. Measured, the crowd and score4
thresholds read `… 234693 999999999` before the fix. So each base score's
`mDifficulty` is restored from its `PlayerTrackConfig`, which is the value the
real `GameConfig` copies from that BandUser. Then the real
`ComputeStarThresholds` is re-run.

**Real-body hooks.** `Game.cpp` and `BandUserMgr.cpp` drag in the
session/UI/net closure and cannot link here. These bodies are copied verbatim
into `m8_support.cpp`, run over the real members of the calloc'd objects, and
marked "keep in step":

- `Game::GetActivePlayers`
- `Game::GetPlayerFromTrack`
- `Game::NumActivePlayers`
- `Game::GetScoringTracks`
- `BandUserMgr::GetBandUser`

`GemPlayer::HasDealtWithGem` remains a driver hook: the driver's player is a
`Player` and has no GemStatus.

**Config.** `SongDB::RunMultiplayerAnalyzer` reads two blocks from the scoring
config:

- `(scoring (coda (point_rate …)))`, unconditionally;
- `(scoring (solo …))`, for every solo section.

The hand-written driver configs carried neither. `scoring_config_dta.h`
supplies the **shipped** blocks verbatim from the retail
`config/scoring.dta`, and they are spliced next to the real `(crowd …)` block.

**Found on the way: `XNetRandom` was a weak no-op that never filled its
buffer.** That made `HxGuid::Generate`, which loops until the GUID is
non-null, spin forever. It is now a real `getrandom()` shim in
`xdk_shims.cpp`, and the `.s` stub is gone.

### 2.2 Real CommonPhraseCapturer in rb3-score2/3

score2/3 link `CommonPhraseCapturer.cpp` and `m8_support.cpp` +
`m8_link_stubs.cpp` in place of `m6_link_stubs.cpp`. The capturer's three m6
stubs are retired: `Enabled`, `LocalFail` and `LocalHitLastGem`.

**score3 now exercises the capturer.** It does the following:

1. `band->mCommonPhraseCapturer = new CommonPhraseCapturer()`.
2. Its player is pushed into the Game's `mAllActivePlayers`.
3. Stage 2 calls the real `HandlePhraseNote` per gem.
4. Credit is read off the `OverdrivePhrasesCompleted()` delta.

Any disagreement with the driver's own whole-phrase verdict prints `MISMATCH`
and returns rc=1. The driver's direct `CreditOverdrivePhrase` call is removed:
credit now comes only through the real capturer.

- **Control:** a sabotaged leg gave **rc=1, MISMATCH**. Restored, stdout is
  byte-identical to before.
- **score3 runs no analyzer.** It reports no star rating, so it carries no
  `(star_ratings …)` config. It uses `NativeSongDBSetupPhrases`, which is the
  phrase half only.

### 2.3 Utl.cpp in rb3-song

The weak `AllowedToAccessContent` (return 0) in `m1_link_stubs.s` is gone.
rb3-song links `meta_band/Utl.cpp`, which supplies the real
`AllowedToAccessContent` and `MaxAllowedHmxMaturityLevel`.

**The W16-PD blocker** was 11 undefined symbols. They resolved as follows:

- **`ControllerTypeToTrackType` and `ScoreTypeToTrackType`:** resolved by
  linking the real `game/Defines.cpp`. That in turn added one more undefined
  symbol, `OvershellPanel::CanGuitarPlayKeys`.
- **The rest:** these come from Utl.cpp's UI-side functions:
  - `UtlInit`'s DataFunc handlers;
  - `GetUserFontChar`;
  - `IsVignette`;
  - `IsLeaderLocal`;
  - the fake-upload cheat.

  They reference `BandUser`, `MetaPerformer`, `SessionMgr`, `typeinfo for
  UIPanel`, `SongStatusMgr::sFakeLeaderboardUploadFailure` and
  `OvershellPanel`. None of those TUs links in this target, and the song
  driver reaches none of those functions. rb3-song now uses the same link model
  as rb3-score2/3/4 (`-ffunction-sections -fdata-sections`,
  `-Wl,--gc-sections`), which drops the unreferenced sections. No stub was
  written for any of them.

**New gate.** The song driver never reached `IsRestricted` (stdout was
byte-identical with only the link change). So `main_song.cpp` gained a gate
over **all 138** loaded songs: `BandSongMgr::IsRestricted`, which calls the real
`AllowedToAccessContent` and `MaxAllowedHmxMaturityLevel`.

| leg | result |
|---|---|
| real Utl.cpp | `[PASS] no parental restriction natively — 0 of 138 song(s) restricted`, 8 of 8 gates |
| **control:** Utl.cpp unlinked, weak stub restored | `[FAIL] … 138 of 138 song(s) restricted`, rc=1 |

⚠ **Scope of that gate.** Retail disc ratings measured `1=66 2=72`, all
`≤ kMinContentLevel` (2). For these songs, `AllowedToAccessContent` returns
true **before** it consults the maturity table. The gate therefore separates
the real chain from the stub, but it cannot exercise
`MaxAllowedHmxMaturityLevel`'s rating-board table. That table is reachable only
with a song rated 3 or 4.

## 3. Before/after, every affected target

stdout is pointer-normalised; rc is 0 → 0 everywhere.

| target | stdout | stderr | notes |
|---|---|---|---|
| rb3-dta, midi, save, ark, milo, vocal | identical | identical | — |
| rb3-gem, hit, score | identical | identical | differs raw only in an ASLR'd `tempoMap=0x…` print |
| rb3-score2 | identical | identical | real capturer linked (replacing its 3 m6 stubs); outputs byte-identical |
| rb3-score3 | identical | +32 lines | the real capturer's credit equals the driver verdict on every phrase; adds 16 `PostLoadVocals` NOTIFY pairs (see §4) |
| rb3-score4 | +3 lines | +39 lines | see below |
| rb3-crowd | identical | +51 lines | thresholds were `6000 … 109000`, from a hard-coded 100000 `PlayerScoreInfo` placeholder; now the real analyzer's `18775 37551 62585 140816 234693 341088`; plus NOTIFYs |
| rb3-vocal2 | identical | +17 lines | analyzer thresholds print 999999999 (vocal base 0: vocal2 does no base override and prints no stars); `SystemConfig: no 'guitar'/'drum'/'bass' section (native partial config)` notices |
| rb3-harmony | **changed** | +32 lines | see below |
| rb3-song | **+2 lines** | identical | the new restriction gate (§2.3); 8 of 8 gates |

The three changed outputs:

- **rb3-score4 (vicarious, PART DRUMS).** The new report lines are
  `REAL SongDB: 15 common phrase(s) on this track (0 unison …); analyzer: 31
  phrase id(s) song-wide, 0 unison` and
  `MultiplayerAnalyzer base: … maxStreakPts 312925 … (driver ideal 312925)`.
  **The real analyzer's base equals the driver's hand-computed ideal exactly.**
  The final thresholds equal the baseline.

  On non-default parts the real base is higher, because the real `AddGem`
  counts sustain-tail points:

  | part | real base | driver base | float stars |
  |---|---:|---:|---|
  | PART GUITAR | 211,461 | 206,875 | 4.43 → 4.39 |
  | PART BASS | 277,336 | 269,975 | 3.77 → 3.73 |

  These are hand runs, not part of native_health.

- **rb3-harmony (centerfold).** The star baseline is now
  `SongDB::OverrideBasePoints(…, vp->GetBaseMaxPoints(),
  vp->GetBaseMaxStreakPoints(), vp->GetBaseBonusPoints())`, which is the real
  VocalPlayer base. The old base was the driver's phrase pool.

  | | before | after |
  |---|---|---|
  | base | 154,921 (driver phrase pool) | REAL `maxStreakPts 218000 maxPts 56000` |
  | thresholds | `7746 17041 29434 71263 119289` | `10900 23980 41420 100280 167860` |
  | stars | **3 (3.22)** | **2 (2.83)** |

  The old phrase pool is still printed, labelled as a cross-check. stderr also
  gains the real `PhraseAnalyzer` diagnostics `Phrases don't quite coincide …
  PART VOCALS and PART KEYS / HARMN`.

## 4. Gaps recorded, not closed

- **Duration and coda are driver-supplied.** There is no `ParseEvents`, so the
  driver supplies the duration and `mCodaStartTick` is −1 (§2.1).
- **Difficulty substitution** in `NativeSongDBPostLoad`. It stays until a real
  BandUser exists headless. The `BandUser::GetDifficulty` stub still returns 0.
- **rb3-harmony's base uses the driver-supplied player GUID** through
  `OverrideBasePoints`.
- **rb3-vocal2 does no base override**, because it prints no stars. Its
  analyzer thresholds read 999999999.
- **rb3-score3's hand-written `(crowd …)` block (0.3 / 2.0 / 0.1) is still
  synthetic.** It is the one config block not yet replaced by the shipped one.
  Follow-up.
- **Unison path.** `SongDB::IsUnisonPhrase` is now real. A chart with unison
  phrases would reach `GetTrackPanelDir()->UnisonSucceed/UnisonEnd`, and that
  returns 0 natively, so those virtual calls would crash. Measured, vicarious
  has 0 analyzer unison phrases, so this is unreached. It needs a native
  TrackPanelDir before any unison chart is driven. Follow-up.
- **The score drivers' vocal parse is incomplete.** The 16
  `vocal overdrive phrase at N ms is after all notes` NOTIFYs come from the
  real `PostLoadVocals`. They fire because score drivers pass the tempo and
  measure maps only after the parse. They are diagnostic only; the outputs are
  unchanged.
- **`TuningOffsetList`.** The real `GetPitchOffsetForTick` returns 100, from the
  seeded `TickedInfo(-1, 100.0f)`. That is outside VocalPlayer's ±50 window, so
  the result is the same as the retired constant-0 stub. Recorded because it
  matches by coincidence, not by construction.
- **`m1_link_stubs.s` under `--gc-sections`.** All its stubs share one
  `.text` section, so gc cannot tell which are still needed. Splitting it per
  symbol would let the linker report which of the 24 remain referenced.
  Not done.

## 5. Deliberately not done

- No `src/` edit, so no X360 A/B.
- No milo-native-engine pin bump, and no engine change request (none needed).
- None of the 11 Utl.cpp UI-side callees was stubbed. `--gc-sections` drops
  their callers instead, which is the score targets' existing model.
