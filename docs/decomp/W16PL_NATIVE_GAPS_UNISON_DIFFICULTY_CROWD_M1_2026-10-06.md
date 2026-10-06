# W16-PL: closing W16-PJ's native gaps — unison path, difficulty stub, score3 crowd config, m1 stubs (2026-10-06)

Branch `w16-pl`, worktree `~/tmp/wt-w16-pl`, based on main `5bb85d98f`.

**Brief.** W16-PJ (`W16PJ_NATIVE_REAL_SONGDB_CAPTURER_UTL_2026-10-06.md` §4)
left four gaps. This lane takes each one and either closes it with real
`src/` code and shipped data, or records why it stops:

1. rb3-score3's hand-written `(crowd …)` block;
2. the null track panel that a unison-phrase chart would hit;
3. `BandUser::GetDifficulty`, a stub that returned 0;
4. the `m1_link_stubs.s` stubs that might no longer be needed.

The milo-native-engine pin is unchanged.

## 0. Headline

| gap | result |
|---|---|
| score3 crowd config | **closed.** It splices the shipped block; PlayerParams now reads 0.8333 / 3.5 / 0.04 instead of 0.3 / 2.0 / 0.1. |
| unison / null track panel | **closed for scoring, recorded for presentation.** A shipped chart (centerfold) runs a four-player band through the real capturer's unison path. Every player and every phrase agrees with the driver's own expectation. |
| difficulty stub | **closed.** The stub is now the real body. That exposed a **real native defect**: rb3-vocal2 and rb3-harmony scored every VocalPart at **Easy**. It is fixed, and harmony's stars go from 2 to 5. The analyzer substitution stays, for a measured reason. |
| m1_link_stubs.s | **24 → 14.** 7 are replaced by the real `LicenseMgr.cpp`; 3 were no longer referenced. The other 14 are measured as never entered. |

Two defects found on the way, both fixed:

- `SongDB::SetupCommonPhrasesForTrack` walks off the end of the gem vector on a
  shipped chart.
- `CommonPhraseCapturer.cpp`'s header claimed the file is X360-inert. It is in
  the match build.

## 1. Instruments

- **Before/after outputs.** `~/tmp/w16pl/runall.sh <buildDir> <outDir>` is
  W16-PJ's script unchanged. It runs every CPU target with
  `tools/native_health.sh`'s inputs and captures stdout, stderr and rc.
  `~/tmp/w16pl/cmp.sh` diffs two such directories, pointer-normalised.
  - Baseline: the binaries built from this worktree **before any edit**
    (`~/tmp/w16pl/base-bin`, run into `out-base`).
  - After: the full rebuild at the final commit (`out-new`).
- **Link probes.** A probe is a throwaway source added to `rb3-score3` and kept
  alive with `-Wl,--undefined=<sym>`. The probe reports the undefined set from
  a `--gc-sections` link, and `CMakeLists.txt` is restored after every probe.
- **Reachability.** Measured with gdb breakpoints, not by reading the source.
- **Controls.** Each new check was also run against a broken leg, to show it
  can fail (§3.4, §4.2).
- **X360.** `tools/ab_measure.py --patch` over the branch's whole `src/` diff,
  in a fresh worktree off main (§7).

## 2. m1_link_stubs.s: 24 → 14

**Method.** Delete the file from rb3-song's link, then read the
`--gc-sections` linker's undefined list. That gives exactly the stubs still
referenced. **21 of 24 were.**

The three that were not are now gone:

- `BandUserMgr::GetParticipatingBandUsers`;
- `BandMachineMgr::IsSongShared`;
- `BandUser::GetControllerSym`.

**LicenseMgr: 7 stubs replaced by the real TU.** rb3-song now links
`src/band3/meta_band/LicenseMgr.cpp`.

- **This was a real defect, not just tidying.** `BandSongMgr::Init` does
  `mLicenseMgr = new LicenseMgr()`. The stub constructor was
  `xor rax,rax; ret`, so the object's `std::set` and `hash_map` were never
  constructed.
- **One header edit.** `LicenseMgr.h` used the STLport-only
  `stlpmtx_std` / `_STLP_TEMPLATE_NULL` spelling of `hash<Symbol>`. It gains
  the house `#if HX_NATIVE` branch, copied from `meta/SongMgr.h` and
  `NextSongPanel.h`. The `#else` branch is the old text verbatim.

**The other 14.** Their only referrers are five functions:

- `BandSongMgr::ContentDone`
- `BandSongMgr::AllowContentToBeAdded`
- `BandSongMgr::GetRankedSongs`
- `BandSongMgr::SyncSharedSongs`
- `BandSongMetadata::HasPart`

These are virtual slots or unexercised accessors. I set gdb breakpoints on all
five over the native_health ark run, and **none was ever entered.** Their real
TUs are SessionMgr, ProfileMgr, RockCentral, SaveLoadManager, UIEventMgr and
GameMode, which pull in the session, profile, net and UI graph.

⚠ **The old header said every `TheXxx` global is a null pointer. Two are not.**
`TheProfileMgr` and `TheRockCentral` are **objects** in real code, so the
8 zero bytes in the stub file are not a valid object. `ContentDone` is the only
reader (`TheRockCentral.IsOnline()`), and it is never entered. The header now
says this.

**rb3-song output: byte-identical, all 8 gates pass.**

## 3. The unison path and the null track panel

### 3.1 What actually crashes, and when

**Prediction (W16-PJ):** only a multi-player unison would reach the null
`GetTrackPanelDir()`. **Measured: wrong.** A single player crashes too.

The mechanism:

1. `SongDB::GetCommonPhraseTracks` masks the analyzer's tracks with
   `Game::GetScoringTracks()`.
2. With one player, a unison phrase is therefore one track wide.
3. Completing it goes straight to `AllTracksCompletedPhrase`, which calls
   `GetTrackPanelDir()->UnisonSucceed()`.

So rb3-score3 or rb3-score4 on any chart with a unison would fault. Two things
stood in front of that crash on centerfold:

- **The baseline died earlier**, on a libstdc++ bounds assertion (rc=134) in
  `SongDB::SetupCommonPhrasesForTrack` (§3.2).
- **The control.** I bounded only that loop and reverted the capturer to
  main's text. The result was **SIGSEGV in
  `CommonPhraseCapturer::AllTracksCompletedPhrase`**, called from
  `LocalHitLastGem` ← `HitLastGem` ← `HandlePhraseNote`.

### 3.2 `SetupCommonPhrasesForTrack` walks off the gem vector

The function is a **100% match** (584 B). It walks `gems[i11]` with no bound,
and so does retail.

centerfold's PART VOCALS shows the problem:

- Its `GameGemList` holds the **14** note-96 percussion hits (ticks 4320–17760).
  I counted these straight from the .mid, so the list is genuine, not a driver
  parse defect.
- Its 10 note-116 OD phrases all come later.
- Retail therefore reads past the end of the vector, silently.
- libstdc++ 16 defines `_GLIBCXX_ASSERTIONS` at -O0, so natively this is an
  abort.

**Fix.** Under `#ifdef HX_NATIVE`, both `while` conditions gain
`i11 < (int)gems.size() &&`. `GetPhraseID` only ever serves indices below
`gems.size()`, and no entry below that index changes.

### 3.3 The panel calls: guarded, not faked

`CommonPhraseCapturer` makes five panel calls:

- `UnisonStart` / `UnisonEnd`
- `UnisonSucceed`
- `UnisonPlayerSuccess` / `UnisonPlayerFailure`

All five are presentation: EndingBonus and gem-track animation triggers.
Natively each is now skipped when `GetTrackPanel()` is null, the same guard
`Player::UnisonHit` already uses (`Player.cpp`, `#ifdef HX_NATIVE if
(!GetTrackPanel()) return;`). The scoring state still runs for real:

- `mFinishedTracks`
- `mInUnisonPhrase`
- `mPhraseStates`
- `CompleteCommonPhrase(true, multi)`
- `UnisonMiss`

**Why not a real TrackPanelDir.** I measured it with a probe: a headless
`TrackPanelDirBase` subclass linked into rb3-score3.

- **144 distinct undefined symbols**, mostly from these classes:

  | class | undefined symbols |
  |---|---:|
  | `RndDir` | 29 |
  | `RndDir` non-virtual thunks | 22 |
  | `RndTransformable` | 18 |
  | `RndDrawable` | 14 |
  | `RndPollable` | 5 |

  The UIManager/UIPanel/Flow typeinfo makes up much of the rest.
- `TrackPanelDirBase.cpp` **does not compile natively** (line 65: an `ObjPtr`
  member-name mismatch).
- A real TrackPanelDir also needs a loaded EndingBonus:
  `TrackPanelDir::UnisonEnd` dereferences `mEndingBonus` unchecked.

That is a render-stack target's job, not a scoring driver's.
`TrackPanel::UnisonStart` / `UnisonPlayerSuccess` / `UnisonPlayerFailure`
stay in `m8_link_stubs.cpp` as link-only definitions. They are now never
entered with a null `this`.

The capturer header said "X360-INERT: this file is NOT in objects.json". **It
is in the match build** (`objects.json`, pinned in `splits.txt`, unit
`default/band3/game/CommonPhraseCapturer` at 96.73% / 29 functions). The
header is corrected.

### 3.4 A shipped chart that exercises it: rb3-score3's band stage

rb3-score3 takes an optional third argument, a band chart. After its
single-track stages, `RunBandUnisonStage` does the following:

1. It parses that chart into a **fresh real SongDB**.
2. It seats one Expert `PlayerTrackConfig` and one real `Player` per instrument
   track: PART DRUMS / GUITAR / BASS / KEYS.
3. It runs the real `SongData::PostLoad`, which runs `PhraseAnalyzer::Analyze`,
   then the real `SetupPhrases`.
4. It builds the real capturer after the players exist, because its `Reset`
   reads them.
5. It hands **every phrase gem of every track, in tick order**, to
   `HandlePhraseNote`. One gem is dropped in the second multi-track unison.

**The expectation is computed from the chart alone**, from which tracks have
gems in which phrase and which gem was dropped. It covers:

- each player's own OD credit;
- each player's unison credit;
- the capturer's all-tracks-completed and failed phrase counts;
- `DidTrackFail`;
- balanced unison windows.

It is checked against the real `Stats` counters and `mPhraseStates`. Any
disagreement prints `MISMATCH` and returns rc=1.

The gem-dealt hook `GemPlayer::HasDealtWithGem` gains per-player state
(`gM8DealtByPlayer`), because gem indices are per track.

**centerfold** (shipped RB3 song, already a native_health input):

```
  4 multi-track unison phrase(s); dropping gem 285 of track 0 in unison 15
  player       odPhrases    unisons   energy
  PART DRUMS     8 [  8]    3 [  3]    1.000
  PART GUITAR    9 [  9]    3 [  3]    1.000
  PART BASS      9 [  9]    0 [  0]    1.000
  PART KEYS      9 [  9]    3 [  3]    1.000
  capturer: 4 phrase(s) all-tracks-completed [4], 1 failed [1]; unison 15 DidTrackFail(track 0)=true
  unison windows opened/closed: 5/5; inUnison at end=false finishedTracks=0x0
  REAL CommonPhraseCapturer agrees with the driver on every player and phrase.
```

centerfold's four unisons are all drums+guitar+keys (mask `0xd`). Bass never
joins one, and its 0 unisons is the real answer.

**My first expectation was wrong, and the real arbiter was right.** Phrase 20
is a **keys+vocals** unison (analyzer mask `0x18`). With no vocalist seated,
its common mask is keys only. The real capturer still runs
`AllTracksCompletedPhrase` for it (state 1), with `b4 = false`, so no unison
stat is counted. The first run therefore printed `4 [3]` and MISMATCH. The
expectation now counts state 1 for every unison whose common tracks all
complete, and unison *credit* only when two or more tracks take part.

**Control:** feed the capturer the dropped gem as a hit while the driver still
expects the drop.

```
  PART DRUMS     9 [  8]    4 [  3]
  capturer: 5 phrase(s) all-tracks-completed [4], 0 failed [1]; ... DidTrackFail(track 0)=false
  REAL CommonPhraseCapturer disagrees with the driver -- MISMATCH; aborting.   (rc=1)
```

Restored, the output is byte-identical to the agreeing run.

**native_health** now passes the band chart: rb3-score3's row is
`-- "$MID_VICARIOUS" "PART DRUMS" "$MID_CENTERFOLD"`, with `--needs
"$MID_CENTERFOLD"`. The unison path therefore runs on every health check.
I deliberately added no second row: the row list is a count sentinel against
the link gate's 18 targets.

Single-player runs on centerfold, which crashed on the baseline (rc=134 for
both), now finish:

| target | before | after |
|---|---|---|
| rb3-score3 `centerfold "PART DRUMS"` | rc=134 | rc=0 |
| rb3-score4 `centerfold "PART DRUMS"` | rc=134 | rc=0 |

## 4. The difficulty stub

### 4.1 The stub was load-bearing, and it was wrong

`BandUser::GetDifficulty` returned 0 for any `this`, including null. I made it
the real body (`return mDifficulty;`, `BandUser.cpp:88`). Before that, I
predicted that any caller running on a null user would now fault. **Two
did**, and both had been silently receiving **Easy**:

1. **`VocalPart::VocalPart`**, via `mPlayer->GetUser()->GetDifficulty()`, from
   `VocalPlayer::PostLoad`. rb3-vocal2 and rb3-harmony build their VocalPlayer,
   Singers and TambourineManager at Expert (`gNativeVocalDifficulty = 3`). Every
   VocalPart, though, got the stub's 0. Its siblings `Singer.cpp` and
   `TambourineManager.cpp` already carry an `#ifdef HX_NATIVE` that reads
   `gNativeVocalDifficulty` when there is no user. VocalPart was the one
   missing it, and it now has the same guard. The result is the shipped Expert
   values (`scoring.dta`: `(slop 180 140 120 120)`,
   `(pitch_margin 3.8 2.6 1.9 1.2)`, `(phrase_value 200 400 800 1000)`):

   | | before (Easy) | after (Expert) |
   |---|---|---|
   | slop | 180 ms | **120 ms** |
   | pitchMaxDist | 3.80 | **1.20** |
   | phraseValue | 200 | **1000** |
   | rb3-vocal2 player score | 29,259 | 145,862 |
   | rb3-harmony combined score | 38,437 | 184,534 |
   | rb3-harmony stars | **2 (2.83)** | **5 (5.26)** |

   The harmony stars were deflated because W16-PJ's base came from the real
   VocalPlayer, which was already Expert, while the VocalParts scored at Easy's
   phrase value: one-fifth of Expert's. The harmony part-assignment and
   ambiguity tallies also shift, because the pitch window narrows from 3.8 to
   1.2.
2. **`Player::CheckCrowdFailure`**: `CantFailYet() || mUser->GetDifficulty() ==
   kDifficultyEasy || mUser->IsNullUser() || …`. The stub's Easy was a
   **hidden no-fail gate**. In retail, "no user" is the `NullLocalBandUser`, and
   its `IsNullUser()` takes the same no-fail branch. A null `mUser` now takes
   that branch explicitly, under `#ifdef HX_NATIVE`. The behaviour is
   unchanged, and the reason is now visible instead of an accident.

The last caller, `Player::FinalizeStats` (`isExpert = mUser->GetDifficulty() ==
Expert`), is called by no driver. If one ever does, it now faults instead of
silently reporting "not Expert".

### 4.2 Why the analyzer substitution stays

`NativeSongDBPostLoad` still restores each base score's difficulty from its
`PlayerTrackConfig`. `MultiplayerAnalyzer::AddUser` asks
`TheBandUserMgr->GetBandUser(guid)`, and headless there is no BandUser.

A calloc'd stand-in cannot work: `BandUser : public virtual User`, so reading
`mUserGuid` needs the vbptr. A really-constructed `LocalBandUser` is runtime-safe
(the ctor only calls `DefaultDifficulty()` and makes a Symbol). Its **link**
closure, though, measured by probe even after the cheap real TUs are linked
(`Defines.cpp`, `GameplayOptions.cpp`, `FixedSizeSaveable.cpp`), is:

- **31 symbols:**
  - BandProfile ×4
  - ProfileMgr ×4
  - PrefabMgr ×3 and PrefabChar
  - NetSession ×2
  - BandUserMgr ×2
  - CharSync and SongStatusMgr
  - TourChar, plus the typeinfo for CharData / PrefabChar / TourChar /
    TourCharLocal
  - `GetFontChar*` ×4
  - `TheProfileMgr` / `TheSessionMgr` / `TheCharSync`
  - `SendJunkPatchesToAll`
- **40 handler `Symbol` globals** (`get_difficulty`, `set_track_type`, …)
  that no native TU defines.

That is ~70 new stubs to retire a 10-line substitution, which is the wrong
trade.

## 5. rb3-score3's crowd config

The hand-written block was
`(crowd (save_level 0.3)(time_to_return_from_brink 2.0)(crowd_loss_per_sec 0.1))`.
It is replaced by `@CROWD@`, which splices `crowd_config_dta.h`'s verbatim
shipped block exactly as rb3-score4 does. `PlayerParams::PlayerParams` reads
the three keys. A new stdout line prints them, and that line is score3's only
stdout change on vicarious:

```
  PlayerParams (crowd): saveLevel=0.8333 msToReturnFromBrink=3500 crowdLossPerMs=0.000040
```

⚠ **Recorded, not changed.** score3's and score4's `(streaks …)` blocks are
also hand-written, and they differ from the shipped ones:

- `vocals` is `(0 1)(10 2)…` in the drivers but `(0 1)(1 2)(2 3)(3 4)` shipped;
- the shipped `singleplayer` / `multi` / `real_bass` entries are absent;
- `(energy (default (0 1)))` stands in for the shipped per-instrument tables.

`streaks/energy` is loaded by `Scoring::Scoring` but **read by no game code**
(no `GetStreakEnergy` caller), so that part is inert. The multiplier rows used
here (`drum` / `guitar` / `bass` / `keys` → `default`-shaped tables) give the
same values as shipped `default`. It is a follow-up, not a live defect.

## 6. Before/after, every target

**Inputs.** The same inputs as `runall.sh` / native_health; rb3-score3 also
ran with the new band argument. stdout and stderr are pointer-normalised.

| target | rc | stdout | stderr | notes |
|---|---|---|---|---|
| rb3-dta, midi, gem, hit, score, score2, save, ark, milo, vocal, crowd | 0 → 0 | identical | identical | — |
| rb3-song | 0 → 0 | identical | identical | links the real LicenseMgr; 8/8 gates |
| rb3-score4 | 0 → 0 | identical | identical | — |
| rb3-score3 (vicarious) | 0 → 0 | +1 line | identical | the crowd values (§5) |
| rb3-score3 (vicarious + centerfold band) | — → 0 | band stage appended | — | the native_health argv; agreement on all players (§3.4) |
| rb3-vocal2 | 0 → 0 | **8 lines** | identical | VocalPart at Expert (§4.1) |
| rb3-harmony | 0 → 0 | **32 lines** | identical | VocalPart at Expert; stars 2 → 5 (§4.1) |
| rb3-score3 / score4 on centerfold, single player | **134** → 0 | — | — | §3.1–3.2 |

## 7. X360 A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-pl-ab --patch <branch src/ diff>`
(run dir `~/tmp/wt-w16-pl-ab/.ab_measure_runs/20261006-084437-w16pl-src-3804397`).
The patch was the branch's whole `src/` diff: five files
(`CommonPhraseCapturer.cpp`, `Player.cpp`, `SongDB.cpp`, `VocalPart.cpp`,
`LicenseMgr.h`). Every change is either inside `HX_NATIVE` or a comment.
Prediction: Δ0 on every key, with at least one leg-B recompile.

```
leg A: matched=53469 masked=25191 honest=28278 code%=57.799340  (recompiles: 0, settled)
leg B: matched=53469 masked=25191 honest=28278 code%=57.799340  (recompiles: 164, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 63.684402 -> legB 63.684402)
units at 100% [mpn ruler]:             547 -> 547 (0 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]:  488 -> 488 (0 reached 100, 0 fell off)
none ruler: matched 53620 -> 53620, Δmatched_code +0 B
```

The prediction held. Leg B recompiled 164 TUs, so the patch really was in the
build and this is not an absent-vs-absent reading.

"No row going down" was also checked **row by row**, not inferred from the
net zeros. Both archived reports have 68,909 rows with the same key set.
Comparing `fuzzy_match_percent` and `match_percent_normalized` per row
(`legA_report.json.gz` vs `legB_report.json.gz`) found **0 rows down and
0 rows up**. The tree was restored by the tool and verified (rc=0).

## 8. Deliberately not done

- No milo-native-engine pin bump, and no engine change request.
- No real TrackPanelDir and no real BandUser in the score drivers. Both are
  measured above as render/session closures, not stub-sized gaps.
- No change to score3's or score4's `(streaks …)` blocks (§5).
- No second native_health row (§3.4).
