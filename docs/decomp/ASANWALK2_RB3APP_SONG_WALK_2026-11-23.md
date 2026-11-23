# Lane asanwalk2 — rb3-app's song walk under ASan + UBSan (2026-11-23)

Branch `asanwalk2`, worktree `~/tmp/wt-asanwalk2`, base `detwalk` tip `da6d1904c` (main `ab3ae40eb` +
the unlanded deterministic-walk lane, so `RB3_APP_DETERMINISTIC` exists). Scratch:
`/home/free/tmp/claude-1000/rb3-scout/asanwalk2/`. Follows lane asanwalk
(`ASANWALK_RB3APP_BOOT_WALK_2026-11-23.md`), which stopped at game_screen.

## 0. Headline

- Six runs of the a25 walk under ASan + UBSan, 540 s each: s0-s2 (seeds 0, 1, 2) on the base binary,
  f0, f3, f4 (seeds 0, 3, 4) with the fix. Every run played a whole song (thecon, combatbaby,
  everybodywantstorule, smokeonthewater), went through the endgame screens and song select, and
  started a second song. f0 and f3 also finished the second song and reached its endgame. All six
  ended at the wall-clock cap (rc 4).
- **ASan: zero reports** in all six runs: no heap, stack, use-after-free or SEGV. The endgame-exit
  SIGSEGV (perturb §3.4, `CharClipGroup::Replace` under `~CharClip`) did not happen in any run,
  across the six endgame exits (one per run).
- **UBSan: 37 distinct sites** (line:column) over s0-s2; f0, f3 and f4 add none. 27 are already in the asanwalk record.
  10 are new; by cause they are 6 findings (§1.2).
  - **1 fixed** (3 sites): CrowdAudio's `-1` excitement sentinel outside the `ExcitementLevel` enum.
  - 1 char site recorded, not fixed (the sanitize row-13 `Transitions::Resize` class, now reached by
    rb3-app).
  - The rest are retail-faithful reads of members retail never initialises either, or a NaN-to-int
    that gives retail's result. Retail evidence below.
- Side result: the ASan seed-0 run is **frame-identical** to detwalk's plain-build runs A, B and R
  over all 14,969 common frames (`tools/rb3_app_det_cmp.py`). ASan's 0xBE malloc fill does not move
  the det state hash (RNG bytes, task clocks, screen names) anywhere on this walk. The hash does not
  cover everything, so this does not prove the uninitialised reads below are inert, but none of them
  moves the walk.

## Method

- Build: `native/build-asan` in the worktree, `-DRB3X_SANITIZE=address,undefined` (ASan + default
  UBSan group, recover mode, `-g`, no `-O`), engine snapshot `rb3-scout/eng-6b34583`
  (= `MILO_ENGINE_PIN` = live engine HEAD). Script `asanwalk2/build_asan.sh`. Frozen binaries
  `asanwalk2/bin1` (base, sha256 `5e7ea98ca4fd…`) and `asanwalk2/bin2` (+ the fix, `386e1f6dd8f0…`).
- Run: `asanwalk2/runb.sh` = a copy of `a25/runb.sh` with `A=` this dir, `X=~/tmp/wt-asanwalk2`, the
  asanwalk `ASAN_OPTIONS`/`UBSAN_OPTIONS` (recover, `detect_leaks=0`, stack traces) and
  `RB3_APP_DETERMINISTIC=1`; `ulimit -d unlimited`, `coord/no610m.sh`, gpu1 lock.
  `SECS=540 TMO=580`, 90,000 frames, script `a25/walk.txt`, `RB3_APP_SEED=<n>`.
- Sites: `asanwalk2/sites.sh <run.log>` lists distinct `file:line:col: runtime error` sites minus the
  union of asanwalk's `w1.sites`/`w2.sites`, plus any `AddressSanitizer` line.
- Retail bytes: `tools/va_disasm.py` on `orig/45410914/band.exe` (clean TU5).

| run | seed | song | game_screen | coop_endgame_screen | 2nd game_screen | frames in 540 s |
|---|---|---|---|---|---|---|
| s0 | 0 | thecon | 1119 | 8009 | 8843 | 14,969 |
| s1 | 1 | combatbaby | 1242 | 8082 | 8786 | 15,202 |
| s2 | 2 | everybodywantstorule | 1242 | 9340 | 10056 | 14,262 |

## 1. Every report and its verdict

### 1.1 Already in the asanwalk record (27 sites, not re-litigated)

BandFaceDeform.cpp:185-189 and .h:15,16, BinStream.cpp:33 (x2), ChunkStream.cpp:501, BandHeadShaper.cpp:317,
318, 320, BeatMap.cpp:90, GameGem.h:84,85, Keygen_Stub.cpp:63 (x2), MatchmakingSettings.cpp:45,98,
mikktspace.c:1460,1667, SongParser.cpp:234 (x2), SongRecord.cpp:154, StandardStream.cpp:685.

Gone since asanwalk: Text.cpp:1852 and CharBonesSamples.cpp (fixed at its landing), FileMerger.cpp:488,
the Joypad/Crc/PassiveMessenger sites (fixed), and Data.h:515 with the motion_blur SEGV (A25 removed the
config read).

### 1.2 New

| # | site | kind | verdict |
|---|---|---|---|
| M1 | `bandobj/CrowdAudio.cpp:140,193,210` Poll, SetExcitement | load of -1 as `ExcitementLevel` | **fixed** (§2) |
| M2 | `bandobj/BandTrack.cpp:535` SetupPlayerIntro, `BandTrack.h:132` GetInstrument (via TrackPanelDirBase::UpdateTrackSpeed), `bandobj/GemTrackDir.cpp:487` TrackReset | load of 0xBEBEBEBE as `TrackInstrument`, `mTrackInstrument` | **retail-faithful, harmless** (§3.1) |
| M3 | `bandobj/StreakMeter.cpp:208` SetIsolatedPart | invalid bool 0xBE, `unk270[n]` | **retail-faithful, harmless** (§3.2) |
| M4 | `meta_band/AccomplishmentProgress.cpp:494` UpdateScoreTypeSpecificStats | NaN to int (`0/0` hopo percent) | **benign, same result as retail** (§3.3) |
| M5 | `game/Stats.h:330` Stats::GetHopoPercent | NaN to int, the same 0/0 | **benign, same result as retail** (§3.3) |
| M6 | `char/CharClip.cpp:187` Transitions::Resize via RemoveNodes <- Transitions::Replace <- CharClip::Replace | non-zero offset applied to null | **char, record only**: sanitize row 13's site (then l.173) (§3.4) |

## 2. Fix: ExcitementLevel's -1 sentinel (M1)

Commit `353692336`, `src/system/bandobj/CrowdAudio.h`: `kExcitementNone = -1` added before
`kExcitementBoot` (which stays 0).

- The sentinel: the ctor sets `mOverrideExcitementLevel`/`Prev` to -1, Poll resets the override to -1
  when `override_crowd_audio_level` is 0, and OnMusicStart parks `mLevel` at -1 before
  `SetExcitement(old)`, so that the `level > mLevel` compare picks the "up to" sound.
- Retail SetExcitement (`0x823124C0`): `lwz r11,0x50(r3); cmpwi cr6,r11,-1` (the override test) and
  `cmpw cr6,r30,r11` on `mLevel` (`0x82312504`), both signed. The enum held only 0..5, so natively
  every -1 load was outside its value range.
- Prediction before looking: the native unsigned enum would make `level > mLevel` false against -1
  and play the "down to" sound. **Refuted**: the ASan binary compares `level > mLevel` with
  `cmp %ecx,%eax; jle` (signed). The enum's values 0..5 fit in int, so it promotes to int. So native
  behaviour was already retail's, and the fix only puts the value inside the enum's range. Clang does
  not use enum ranges for optimisation without `-fstrict-enums`, so this removes UB on paper, not a
  visible bug.
- X360: MSVC enums are int either way and `W4ExcitementLevel` mangles the same. A/B in §4.

## 3. Not fixed, and why

### 3.1 BandTrack::mTrackInstrument read before SetInstrument (M2)
- Retail ctor `??0BandTrack@@QAA@PAVObject@Hmx@@@Z` (`0x8234E308`) stores 0x8, 0x9, 0xC (the Symbol),
  0x14 and 0x18-0x1F, never 0x10. The only store is SetInstrument (`mTrackInstrument = inst`).
- Retail SetupPlayerIntro (`0x8234FF18`) reads it as the source does: `lwz r11,0x10(r28)`, `cmpwi 0`,
  `cmpwi 8` at `0x8234FFBC`. So a BandTrack reset before its instrument is assigned (Retract -> Reset)
  reads garbage on the 360 as well.
- Harmless: the 0xBEBEBEBE garbage is negative, so the intro's icon branch is skipped;
  UpdateTrackSpeed counts a track only if `InUse() && inst >= kInstGuitar`, and an unassigned track is
  not in use; TrackReset's `instrument_setup.anim` SetFrame(garbage) is redone when the track is
  assigned (GemTrackDir::SetInstrument calls BandTrack::SetInstrument, then TrackReset). Not fixed.

### 3.2 StreakMeter::unk270 read before SetPartActive (M3)
- Retail ctor `??0StreakMeter@@QAA@XZ` (`0x822D90C8`) stores 0x2B0 (`stb`), 0x2C4.. and the 0x334
  isolated-part int, never 0x2C0-0x2C2. Retail SetIsolatedPart (`0x822D7298`) does `stw r4,0x334(r3)`
  then loops `lbzx r5,r29,r31` over `r29 = this+0x2C0` into SetPartActive (`bl 0x822D71B8`): the same
  unset read.
- SetPartActive returns before using the bool unless the meter has more than one part (`unk2c8 > 1`),
  `n` is below the part count and the part's fade anim exists; only then is the garbage stored and
  used to pick the fade frame. I did not check whether a multi-part meter can reach SetIsolatedPart
  before every part's SetPartActive, so "harmless" rests on retail doing the same read (the
  value is whatever the 360 heap held). Retail-faithful; not fixed.

### 3.3 Hopo percent of a part with no hopo gems (M4, M5)
- `(float)mHopoGemsHopoed / (float)mHopoGemCount` is 0/0 = NaN for a part with no hopos, then
  converted to int.
- Retail UpdateScoreTypeSpecificStats (`0x8258F038`) does `fdivs f13,f12,f13; fmuls; fctiwz` at
  `0x8258F1E0`. `fctiwz` of NaN gives 0x80000000; x86 `cvttss2si` gives the same 0x80000000
  ("integer indefinite"). So `hoposPercent > mBestHoposPercent` is false on both. GetHopoPercent
  (inlined into PerformerStatsInfo::Update, `0x8257B2E8`) is the same conversion. Not fixed.

### 3.4 CharClip::Transitions::Resize(0, nullptr) (M6), src/system/char, record only
- The site is sanitize row 13 (`CharClip.cpp:173` then, now l.187 after the HX_NATIVE
  `intptr_t` arm): with size 0 the block is freed, `mNodeStart` becomes null, and
  `return (NodeVector *)((char *)mNodeStart + n)` adds `n = old - start` (here `0 - start`) to null.
  RemoveNodes (HX_NATIVE) calls `Resize(..., nullptr)` and drops the result.
- New only in the route: rb3-app reaches it from `CharClip::Replace` -> `Transitions::Replace` ->
  `RemoveNodes` when the last node is removed. Benign (the result is unused).
- Proposed fix (char owner): in Resize, `return old ? (NodeVector *)((char *)mNodeStart + n) : nullptr;`
  under HX_NATIVE.

## 4. X360 A/B

`tools/ab_measure.py --worktree ~/tmp/wt-asanwalk2 --revert 353692336` (log
`asanwalk2/ab_measure-fix1.log`, result
`asanwalk2/ab-fix1/20261123-043702-asanwalk2-fix1-revert-1544410/result.json`). It is a revert, so it
measures removing the fix. Prediction: Δ0.

- `ab rc=0`. Ruler `functionRelocDiffs=name_check`, objdiff-cli sha256 `ec3badc0f33323e9` on both legs.
  Leg B recompiled 173 objects and settled in 2 iterations.
- leg A = leg B: matched 55,268, masked_equal 25,360, honest 29,908, code% 59.665310, fuzzy 64.709015;
  units at 100% 635 (mpn) and 548 (all-rows-fuzzy).
- **Δmatched +0, Δhonest +0, Δcode_bytes +0, Δfuzzy +0.000000pp**; the `none` control +0 B.

## 5. Confirmation runs

`bin2` (with the fix), the same recipe, seeds 0, 3, 4 (runs f0, f3, f4; logs `asanwalk2/r_f<n>/run.log`).
Less machine load than for s0-s2, so more frames in 540 s.

| run | seed | song | game_screen | coop_endgame | 2nd game_screen | 2nd coop_endgame | frames |
|---|---|---|---|---|---|---|---|
| f0 | 0 | thecon | 1119 | 8009 | 8843 | 15793 | 17,254 |
| f3 | 3 | combatbaby | 1242 | 8062 | 8790 | 14637 | 17,072 |
| f4 | 4 | smokeonthewater | 1138 | 12205 | 12932 | - | 17,019 |

- The CrowdAudio sites are gone from all three. No site is new against s0-s2, and ASan reports
  nothing. f0 and f3 played two whole songs and reached the second endgame. f4's first song was long,
  so it was still in its second song at the cap.
- f0 against s0 (`tools/rb3_app_det_cmp.py`): **IDENTICAL over 14,969 common frames**, as predicted
  for a fix that changes no behaviour.
- f3 (seed 3) played the same song as s1 (seed 1); its first endgame is at frame 8062 against s1's
  8082. The seeds differ, so the frames differ too.

## 6. Not done

- Not run: native_health --selftest (per brief), native_build_gate, rb3-render.
- No edit to Rnd/Draw/Text/Spotlight, ui, char, the engine, or net sync/game-start.
- Leaks not looked at (`detect_leaks=0`).
