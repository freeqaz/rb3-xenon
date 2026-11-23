# Lane ubsantri — every rb3-app UBSan site, judged against retail (2026-11-23)

Goal: take each UBSan site the rb3-app ASan+UBSan walks report (scorewalk S2, gemwalk S,
keyswalk KS/GS, menuwalk S_*), decide on `orig/45410914/band.exe` (`tools/va_disasm.py`)
whether it is a port bug or retail-faithful, fix port bugs, and give a retail-faithful unset
read an `#ifdef HX_NATIVE` init only where the native read can change behaviour.

Prediction before starting: no port bug left (every site already had a verdict from the
sanitize / asanwalk / asanwalk2 / menuwalk lanes), and at most a couple of retail-faithful
unset reads whose "harmless" verdict holds only for ASan's 0xBE fill, not for an arbitrary
heap value. That is what was found (two, §2).

## Method

- Site list: the union of `~/tmp/scorewalk/S2.sites` (36 file:line:col sites), menuwalk's
  `~/tmp/menuwalk/S_all.sites` (24 file:line) and the keyswalk record (KS/GS: 25 sites, all in
  S2). One stack per site from the run logs (`~/tmp/ubsantri/stacks.py`).
- Retail: `python3 tools/va_disasm.py <va> <n>` on clean TU5 band.exe. For "the ctor never
  stores offset X" I count `st[wbhf]* rN, X(rM)` over the ctor's disassembly with
  `~/tmp/ubsantri/cnt.sh`, and next to each zero a positive control on a neighbouring offset the
  ctor does store (a first version of the regex required a space after the mnemonic, the
  disassembler prints a tab, and the controls read 0 too: that screen could not fail).
- Fresh walks (this lane, §4) on main `c67614e09` and on the fix, plain and ASan+UBSan.

## 1. Verdict for every site

| # | site (file:line) | UBSan | verdict | retail evidence / reason |
|---|---|---|---|---|
| 1 | `bandobj/BandTrack.cpp:535` SetupPlayerIntro, `BandTrack.h:132` GetInstrument, `bandobj/GemTrackDir.cpp:487` TrackReset | `TrackInstrument` 0xBEBEBEBE | **retail-faithful; native init added** (§2.1) | ctor `0x8234E308` stores 0x8, 0x9, 0xC, 0x14, 0x18-0x1F, never 0x10 (count 0; control 0x14: 1). SetupPlayerIntro `0x8234FFBC` `lwz r11,0x10(r28)`, `cmpwi 0` / `cmpwi 8`. asanwalk2 M2 called it harmless because 0xBEBEBEBE is negative; a plain-heap value 0..7 takes the icon branch on an unassigned track |
| 2 | `bandtrack/VocalTrack.h:97` IsCurrentVocalParam | `VocalParam` 0xBEBEBEBE | **retail-faithful; native init added** (§2.2) | ctor `0x82BA9A28` stores 0x224 = -1 (`li r11,-1` / `stw r11,0x224(r30)` at `0x82BA9BE4`) and 0x228 = 60, never 0x220 (count 0; control 0x224: 1). Only PushGameplayOptions writes it. A value 1..5 hides the vocalist-volume widget on a button release |
| 3 | `bandobj/StreakMeter.cpp:208` SetIsolatedPart, `:222` NumActiveParts | bool 190 | **already fixed on main** (menuwalk merge `8fbded104`, HX_NATIVE clear of `unk270`/`unk2cc`) | ctor `0x822D90C8` never stores 0x2C0-0x2C2 (asanwalk2 §3.2); absent from this lane's base ASan walks (both :208 and :222), including the practice walk where menuwalk saw :222 |
| 4 | `net/MatchmakingSettings.cpp:98` SetRanked (now :113) | bool 190 | **already fixed on main** (A36, SessionSettings ctor `mRanked = false` under HX_NATIVE) | retail `fn_823F3C78` writes no 0xC (comment in the ctor); absent from this lane's base ASan walks |
| 5 | `net/MatchmakingSettings.cpp:45` Save (NetSearchResult ctor) | bool 190, `mRanked` | **retail-faithful, harmless; not fixed** | retail's inlined ctor in NetSearchResult (`0x823F5AD8..0x823F5B2C`) stores neither 0x8 nor 0xC (sanitize §3.2). Offline the bytes only round-trip through a MemStream into the search result; src/network, 360-ONLY tier |
| 6 | `beatmatch/SongParser.cpp:234` OnEndOfTrack (x2 cols) | `TrackType` 0xBEBEBEBE | **retail-faithful, harmless for any value; not fixed** | ctor `0x8278A658` never stores 0xF8 (count 0; control 0xF4/0xFC/0x100: 1); retail OnEndOfTrack `0x827849E0` reads it (`lwz r11,0xf8`, `cmpwi 4/5`). The bool goes with `mTrack = -1` (ctor) to SongData::OnEndOfTrack, whose `b && i < mGemDBs.size()` is false for -1 (unsigned compare), and to the native gem sink, which tests `trk >= 0`; so the value is never used |
| 7 | `synth/StandardStream.cpp:685` InitInfo | bool 190, `mJumpSamplesInvalid` | **retail-faithful, harmless for any value; not fixed** | ctor `0x827052F0` and Init `0x82704C38` never store 0x94 (counts 0; Init control 0x8C/0x90: 2). A garbage-true flag runs `setJumpSamplesFromMs(0, 0)` with the ms Init zeroed: from = MsToSamp(0) = 0, to = 0, i.e. no jump, the cleared state |
| 8 | `meta_band/SongRecord.cpp:154` UpdateSharedStatus | bool 128 / 64, `mIsShared` | **retail-faithful, harmless for any value; not fixed** | ctor `0x825BBCE0` stores 0x2D and 0x2E (control: 2) and not 0x2C (count 0), then calls UpdateSharedStatus, which always leaves 0x2C equal to the computed value; the ctor drops the return (asanwalk §3.5) |
| 9 | `beatmatch/GameGem.h:84,85` via `GameGem::operator=` | `RGNoteType` garbage words | **retail-faithful, harmless; not fixed** | `GameGem(const MultiGemInfo&)` `0x8278EDA8` stores 0x0-0x12, 0x3C, 0x40 (control: 8), nothing in 0x14-0x2B (count 0). The words are only copied; GetRGNoteType is real-guitar only, and clang without `-fstrict-enums` does not assume the enum range |
| 10 | `game/Stats.h:330` GetHopoPercent, `meta_band/AccomplishmentProgress.cpp:494` | NaN to int (0/0 hopo percent) | **same result as retail; not fixed** | retail `0x8258F1E0` `fctiwz` gives 0x80000000 for NaN, x86 `cvttss2si` the same "integer indefinite" (asanwalk2 §3.3) |
| 11 | `utl/BeatMap.cpp:90` Beat(float) | -4.8e32 to int | **same result as retail; not fixed** | retail `0x827D2B80` `fctiwz` saturates to 0x80000000 = x86's result; then clamped to 0 on both (asanwalk §3.6). The value is MidiParser's huge-negative sentinel, not garbage |
| 12 | `utl/BinStream.cpp:33` SwapData, `utl/ChunkStream.cpp:501` DecompressMemHelper | misaligned `short` / `unsigned int` | **intended, benign on x86; not fixed** | unaligned scalar loads are legal on x86-64 and on the 360's `lhz`/`lwz` to cached memory; the data are packed file bytes (sanitize §3.4) |
| 13 | `bandobj/BandFaceDeform.cpp:185-189`, `BandFaceDeform.h:15,16`, `bandobj/BandHeadShaper.cpp:317,318,320` | misaligned packed `Delta` | **intended, benign on x86; not fixed** | `Delta` is a variable-length packed record walked in place (`thisoffset()`/`next()`); same class as row 12. BandHeadShaper is reached from BandCharacter::DeformHead (not edited) |
| 14 | `char/CharClip.cpp:187` Transitions::Resize | non-zero offset on null | **char: listed, not edited** (§3) | sanitize row 13 / asanwalk2 M6 |
| 15 | engine `platform/Keygen_Stub.cpp:63` random | signed overflow (LCG) | **engine: listed, not edited** (§3) | |
| 16 | engine `gfx/mikktspace.c:1460,1667` | shift by 32 | **engine: listed, not edited** (§3) | |

## 2. Fixes (commit `3f92abcdb`, both `#ifdef HX_NATIVE`)

### 2.1 `BandTrack::mTrackInstrument` starts `kInstNone`

- Retail never initialises +0x10 in the ctor (table row 1); the only store is
  `BandTrack::SetInstrument`. A track that is reset before it is assigned
  (`TrackPanel::Reset` → `TrackPanelDir::ResetAll` → `GemTrackDir::Retract` → `BandTrack::Reset`)
  reads it three times: SetupPlayerIntro (`0 <= inst < 8` sets the intro's icon from
  `mParent->GetTrackIcon()` and its name label), TrackPanelDirBase::UpdateTrackSpeed
  (`InUse() && inst >= kInstGuitar`) and GemTrackDir::TrackReset
  (`instrument_setup.anim` SetFrame(inst)).
- asanwalk2 M2 judged it harmless because ASan's 0xBEBEBEBE is negative. That reasoning holds
  for that fill only: on glibc's heap the bytes are whatever the last owner left, and a
  0..7 value takes the icon / name-label branch on an unassigned track. So the native read can
  change behaviour, and the heap decides which way.
- Fix: `mTrackInstrument = kInstNone` in the ctor body under `#ifdef HX_NATIVE`. kInstNone (-1)
  is the "no instrument" value the source's own `>= 0` tests expect, and it is the branch every
  ASan walk took (0xBEBEBEBE is negative too), so the walks' behaviour is the one kept.
  The match build compiles none of it.

### 2.2 `VocalTrack::mCharOptParam` starts `kVocalParamAny`

- Retail's ctor stores the neighbouring mCharOptMicID = -1 and nothing to +0x220 (row 2); only
  PushGameplayOptions writes the param. A volume-button release with no push before it
  (VocalPlayer::OnMsg(ButtonUpMsg) → HandleDeactivateVolume → VocalTrackDir::DeactivateVolume)
  compares it in IsCurrentVocalParam and hides the vocalist-volume widget on a match.
  Menuwalk saw it in practice mode with a real pad press.
- The buttons map to MicVolume, Mic1-3Gain or CueVolume (1..5), never kVocalParamAny (0), and
  TrackInterface's default IsCurrentVocalParam returns false. So 0 means "no param is current
  until a push", the base-class answer; a heap value 1..5 would hide the widget on the wrong
  button. Fix: `mCharOptParam = kVocalParamAny` at the end of the ctor under
  `#ifdef HX_NATIVE`.
- I did not establish whether the widget can be showing at that point, so the visible
  effect may be nil; what the init removes is the heap's say in the outcome.

### 2.3 Port bugs found

None. Every site's read is also in retail's code, with the same unset or out-of-range
source.

## 3. Listed, not edited (out of this lane's edit scope)

- `src/system/char/CharClip.cpp:187` `Transitions::Resize` via `RemoveNodes` ← `Transitions::Replace` ←
  `CharClip::Replace` ← `~ObjectDir`: `n = old - mNodeStart` computed against a freed and nulled
  buffer (sanitize row 13, asanwalk2 M6; asanwalk2 §6 records that the `old ? … : nullptr` fix
  proposed there was wrong). char: not edited.
- `src/system/bandobj/BandHeadShaper.cpp:317-320` is reached only from
  `BandCharacter::DeformHead` (BandCharacter.cpp, not edited); its verdict (packed Delta,
  benign on x86) needs no BandCharacter change.
- Engine (`../milo-native-engine`, not this repo): `src/platform/Keygen_Stub.cpp:63` `random()`
  does `s_seed * 0x19660E + 0x3C6EF35F` in `int`. The comment says the wrap must match 32-bit
  arithmetic, and clang emits a wrapping `imul`/`add` today, but the overflow is UB; the fix is
  `unsigned int` arithmetic (same bits). `src/gfx/mikktspace.c:1460,1667`: `uSeed >> (32 - t)`
  with t = 0. x86 masks the count, so the result is uSeed, which a rotate by 0 should give;
  the fix is `t ? rot : uSeed`. Both are benign today and engine-side; listed for the engine owner.
- No src/system/ui, rndobj or joypad site appears in any of the walks.

## 4. Walks: identity and the site list after the fix

Binaries (rb3-app, worktree `native/build` / `native/build-asan`, frozen in `~/tmp/ubsantri/`):
`bin-base` `a21a1dacdff5` and `bin-asan-base` `8ca458113b16` (main `c67614e09`), `bin-fix`
`c96257436fcb` and `bin-asan-fix` `426554b44bab` (`3f92abcdb`; the incremental builds recompiled
exactly the 2 TUs). I checked that the base ASan objs for both TUs were written before the edit (obj
mtime 20 s older than the source). Every run: `RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1`, through
`coord/no610m.sh` (gemwalk's `runb.sh`, copied to `~/tmp/ubsantri/runs.sh`), `SECS=420 TMO=460`.
V = `rb3-scout/a25/walk.txt` (vocals pad), G = gemwalk's walk with `RB3_APP_JOYPAD_KIND=guitar`,
R = menuwalk's `w_practice.txt` (practice mode, 7,000 frames). ASan runs use gemwalk's
ASAN/UBSAN_OPTIONS. Prediction: every pair identical; the four sites of §2 gone; no new site.

| pair | V (vocals) | G (guitar) | R (practice) |
|---|---|---|---|
| plain base vs plain fix (`rb3_app_det_cmp.py`) | **IDENTICAL over 32,225** (into song 2's endgame) | **IDENTICAL over 33,780** | **IDENTICAL over 7,000** |
| same, whole log (`rb3_app_log_cmp.py`) | 0 diff hunks | 0 | 0 |
| ASan base vs ASan fix | **IDENTICAL over 12,517** | **IDENTICAL over 13,074** | **IDENTICAL over 7,000** |
| plain fix vs ASan fix | IDENTICAL over 16,875 | IDENTICAL over 17,383 | IDENTICAL over 7,000 |
| plain base vs ASan base | IDENTICAL over 12,517 | IDENTICAL over 13,074 | IDENTICAL over 7,000 |

- FAIL 0, UNREACHED 0, AddressSanitizer reports 0 in all twelve runs.
- Base ASan site union (V+G+R): 27 file:line sites. Against scorewalk S2 it adds only
  `VocalTrack.h:97` (3 hits in R, the practice walk, even with autoplay; 0 in V and G). S2 sites it
  does not reach: `StreakMeter.cpp:208` and `MatchmakingSettings.cpp:98` (both fixed on main),
  `MatchmakingSettings.cpp:45` and `CharClip.cpp:187` (not reached in these shorter ASan walks).
- Fix ASan union: **23 sites. Gone: `VocalTrack.h:97`, `BandTrack.cpp:535`, `BandTrack.h:132`,
  `GemTrackDir.cpp:487`. New: none.** The remaining 23 are table rows 6-13 and 15-16.
- What the identity shows: the det state hash covers RNG, task clocks and screen/transition names,
  not widget state (detwalk record), so the two inits change no walk on those fields, under the
  0xBE fill or the plain heap. It does not show the inits are visible; the UBSan sites are the
  evidence that they now cover the reads.

## 5. X360 A/B

- `python3 tools/ab_measure.py --worktree ~/tmp/wt-ubsantri --revert 3f92abcdb` (log
  `~/tmp/ubsantri/ab_measure.log`, run dir
  `.ab_measure_runs/20261123-101629-3f92abcdb-1253991/`). It is a revert, so read each delta with its sign
  flipped. Predicted Δ0: both inits are inside `#ifdef HX_NATIVE`, which the match build never
  defines.
- Measured (`name_check` ruler, objdiff `sha256:ec3badc0f333`): leg A matched 55,268, masked_equal
  25,360, honest 29,908, code 59.665310 %; leg B identical. **Δmatched +0, Δhonest +0, Δcode_bytes
  +0, Δfuzzy +0.000000 pp**; units at 100 % 635 → 635. Leg B recompiled 3 objs (the two TUs and
  whatever includes them), split 0. The `none` leg also +0 B. Tree restored and verified by the
  tool.

## Not done

- `tools/native_health.sh --selftest`: left to the coordinator, as briefed. Only rb3-app (plain
  and ASan+UBSan) was built, through `native/build` and `native/build-asan` of the worktree.
- No keys / pro guitar / drums walk on the fix: keyswalk's KS/GS sites are a subset of S2
  and neither touched member is instrument-specific beyond BandTrack's reset path, which every
  gem walk runs.
- The volume-widget question in §2.2 (can it be showing before any push?) was not read in
  the milo data.
- No engine edit (Keygen_Stub, mikktspace).
- Not landed, nothing pushed.
