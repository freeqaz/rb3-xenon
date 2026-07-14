# WAVE 5 — Song-start risk verdict: why RB3DX blocks same-part, and what our DTA-only fix ships

**Date:** 2026-07-09 · **Lead:** Fable (INVESTIGATE+DECIDE) · **Inputs:** wave5 finders
(intent/audio/gems/config, all checkpointed in `/tmp/si-hw-fix/wave5/`), all load-bearing
claims re-verified first-hand against `/home/free/code/milohax/rb3/src/`.

---

## 1. Why did RB3DX disable same-part selection?

**PROVEN (from RB3DX source + engine ground truth):**

1. **It is a deliberate design boundary, not a workaround for a crash they hit.**
   `dx_check_for_dupe`'s own header comment states its entire purpose:
   *"function to allow 5L/Pro instruments together but still block same parts from being
   selected"* (`_ark/dx/overshell/dx_overshell_funcs.dta:38`). RB3DX **added** cross-family
   duping (5-lane Guitar + Pro Guitar — which vanilla blocked) and **kept** vanilla's
   same-part block, replacing the vanilla `{show_choose_part_wait}` routing
   (commented out at `slot_states.dta:3253-3255` — "; dx - deprecated vanilla behavior").
2. **Cross-family duping needed ZERO engine machinery — same-part cannot get away with that.**
   kTrackGuitar and kTrackRealGuitar are *different track numbers*: separate audio stem
   (`TrackData`, one owner each) and separate `GameGemList`. That's exactly why RB3DX's
   cross-family feature could be pure DTA. Same-PART means the *same* track number → shared
   gem list, shared stem, shared occupancy slot — and the engine's one-player-per-track
   assumption (`PlayerTrackConfigList::mTrackOccupied`) hard-fails on the 2nd claimant.
3. **The `#ifdef DISABLED` sibling ("i tried refactoring and it broke vocal duping",
   `dx_overshell_funcs.dta:117-119`) is a red herring for the song-start question.** It's an
   abandoned cleanup of the *selection gate's* DTA control flow whose vocals special-case
   misfired for harmony vocalists (a script bug, not an engine crash). It does show the
   author actively iterated on this gate and treated vocals as the only safe dupe — vocals
   are safe because vocal stems are `SetNonmutable` and harmonies use separate HARM tracks.

**PLAUSIBLE (can't be proven from the shallow repo — 1-commit checkout, no history):**
the RB3DX authors knew or suspected same-part breaks at song start (it does — §2) and never
attempted the engine-side work. Nothing in the repo says they ever tested it and hit the crash.

## 2. Does the DTA-only fix ship a bug? — **YES: probable HARD CRASH at song load**

Classification: **hard-crash** (probable; UB, heap-layout dependent — worst realistic outcome).

Verified chain, all in RB3 Wii decomp (behavioral ground truth for TU5):

| Step | File:line | Fact |
|---|---|---|
| 1 | `PlayerTrackConfigList.cpp:194-243` | 2nd Guitar: `TrackNumOfExactType(kTrackGuitar)` finds only an occupied slot → -1. `TrackNumOfType` has fallbacks ONLY for 22-fret→Real Guitar/Bass; kTrackGuitar → -1 → `MILO_FAIL("Couldn't create track of type %s… head-to-head… obsolete")`, `cfg.mTrackNum` **never assigned** |
| 2 | `PlayerTrackConfig.h:11` | `mTrackNum` ctor default is **-1** |
| 3 | `Debug.h:147-150` | Retail (non-MILO_DEBUG): `MILO_FAIL` = `(void)(...)` **no-op** → no abort, -1 rides downstream silently |
| 4 | `SongData.cpp:1185-1200` | `GetGemList(-1)` → `mTrackDifficulties[-1]` read + `mGemDBs[-1]->GetDiffGemList(...)` garbage-pointer deref. The `#ifdef HX_NATIVE` guards here were added FOR THE PORT — the console path is **unguarded** (direct proof this OOB class is real) |
| 5 | `TrackWatcherImpl.cpp:66-67` | `RecalcGemList(): mGemList = mSongData->GetGemList(mTrack=-1)` — reached at song load via `BeatMatcher::SetTrack` → new watcher |
| 6 | `SongData.cpp:1296` + `MasterAudio.cpp:643-645` | Audio lane: `BeatMatcher::SetTrack` → `mAudio->SetTrack(guid,-1)` → `GetAudioTrackNum(-1)` = `mTrackInfos[-1]->mAudioTrackNum` → garbage `AudioTrackNum` indexes `mTrackData[...]` |
| 7 | `SongDB.cpp:149`, `GemPlayer::PostLoad` | More `vector[-1]` sites (`GetGems(-1)`, `GetTotalGems(-1)`) |

On PPC/360, `std::vector::operator[](-1)` reads just before the buffer (allocator header) →
low/garbage pointer → `->` deref → DSI. **Probable crash during song load** (before or at the
loading screen's end), best-case silent state corruption with mis-mapped stems + garbage gems.

**Finder conflict resolved:** intent's "no crash / soft glitch" and gems' "LOW crash risk"
analyses both implicitly assumed P2 *shares P1's valid track* — i.e. the post-ProcessConfig-assist
world. config+audio traced the actual no-assist world (mTrackNum = -1). All four agree once staged:

- **No assist (our current DTA-only ARK):** probable hard crash at song load.
- **ProcessConfig assist only:** song plays; note-stealing + single-owner stem quirks.
- **Both assists:** playable; residual soft issues only (§3).

The user's "maybe the audio system fails" intuition is half-right: the audio subsystem IS a
failing site (`GetAudioTrackNum(-1)` deref), but it fails as part of the same -1 track-config
crash, not as a graceful audio error.

## 3. Assist decision: **YES — both RB3E-DLL hooks are required** (DTA-only is NOT shippable for same-part)

Risk → mitigation map:

| # | Real risk | Mitigation | Covered by old design's hooks? |
|---|---|---|---|
| R1 | **Crash**: 2nd same-type player gets mTrackNum=-1 → vector[-1] DSI at load | **ProcessConfig hook (TU5 `0x8276FA08`)**: when `TrackNumOfType` returns -1 AND another config of the same type is already assigned, reuse that config's `mTrackNum` instead of leaving -1 | YES — this is exactly what it was designed for |
| R2 | **Note-stealing**: shared `GameGem.mPlayed` (+ `unk10b1` cymbal marker, + whole-list `Reset()`) on the one per-track `GameGemList` — P2 can never hit a gem P1 already hit (`BaseGuitarTrackWatcherImpl.cpp:63` skips played gems) | **RecalcGemList hook (TU5 `0x82794740`)**: give each watcher a PRIVATE `GameGemList` clone (`GameGemList::CopyFrom` copies both bits). MUST install at RecalcGemList (re-fires on restart/section-jump/diff-change), not once at construction | YES — with the choke-point caveat already in the design doc §6.4 |
| R3 | Single-owner audio stem: `MasterAudio::SetTrack` overwrites the one owner guid; mute fader + `Ignore()` filtering follow the LAST-assigned player | NOT covered. Soft: stem mute/unmute tracks one player's hits/misses only. Acceptable v1; a per-player OR-mute model = future work | NO (accepted residual) |
| R4 | `GemPlayer` FxSend last-writer-wins; first same-part leaver nulls the other's FX/pitch-shift (`~GemPlayer` → `SetFXSend(chan,nullptr)`) | NOT covered. Soft, only on mid-song leave. Acceptable v1 | NO (accepted residual) |
| R5 | Forced shared difficulty: `mTrackDifficulties[track]` is per-track; ProcessConfig stamps `mTrackDiffs[num]=diff` so the LAST processed player's difficulty wins for both | NOT covered. UX caveat: both same-part players play one difficulty. Hook should avoid re-stamping P1's diff (or document "same diff required") | NO (document as limitation) |
| R6 | Shared-list readers outside the watcher (BeatMatcher direct `GetGemList` reads, SongDB/visual highway) bypass the clone | Low risk (phrase/fill + visuals, message-driven); watch in test | PARTIAL — monitor |

**"mPlayed is the only shared state" claim: PARTIAL.** The clone incidentally also privatizes
`unk10b1` and the `Reset()` target (both live on the gems), so the clone is sufficient for the
gameplay-correctness core — but R3/R4/R5 are real, un-cloned, and soft.

**Delivery:** RB3Enhanced DLL only (all TU5 pins confirmed in `wave2/pins.json`; the static
.data XEX cave is PROVEN DEAD — Xenia won't JIT-dispatch into .data, console PE .data lacks
EXECUTE). The DTA edit stays as the selection-unblock layer; the DLL adds R1+R2.

**Ship guidance:** do NOT ship the DTA-only ARK as a same-part feature. It is fine to ship as
the cross-family unblock (that's all RB3DX intended), but two same-part Guitars will probably
crash at song load. Gate the DTA TRUE behind the DLL's presence if possible, or ship them together.

## 4. Console test plan (diagnostic, staged — each symptom implicates one lane)

**Stage 0 — DTA-only ARK (current build), P1+P2 both 5-lane Guitar, quickplay any song:**
1. Selection: both players reach ChooseDiff → confirms wave-4 gate removal works (script lane).
2. Start song. **Expected: hard freeze/crash during the loading screen or first frame** (R1,
   config lane). If instead it loads: watch for garbage/absent gems on one highway and wrong
   stem behavior — that's the same -1 lane in its non-crash UB guise. Either outcome = R1 confirmed.
3. Control: same build, P1 Guitar + P2 Bass must load clean (proves crash is dupe-specific).

**Stage 1 — DLL with ProcessConfig hook ONLY (R1 fix), same pairing:**
4. Song loads and plays = R1 fix verified.
5. Both hit the SAME note: if only the first registrant scores and the other's identical hit
   whiffs/streak-breaks → R2 (shared mPlayed, gems lane). HOPO/sustain chains show it loudest.
6. Guitar stem audio on misses: if the stem only ever mutes/unmutes on ONE player's play → R3
   (single-owner stem, audio lane). Not a bug to fix now — just attribute it.
7. One player pauses out/leaves mid-song: if the survivor's FX/whammy dies → R4.

**Stage 2 — DLL with ProcessConfig + RecalcGemList clone (full assist):**
8. Repeat 5: both players now score the same notes independently; streaks independent = R2 fixed.
9. Practice-mode section jump or song restart, then re-check 5 — verifies the clone reinstalls
   at the RecalcGemList choke-point (the §6.4 make-or-break).
10. Play a full song to the end: score screen shows two sane, non-identical scores; back out and
    start a second song (leak/reuse check).
11. Note difficulty: confirm both players got the SAME difficulty regardless of picks (R5, expected).

**Symptom → lane key:** crash at load = R1/config · "my notes are already gone / didn't count" =
R2/gems · stem mute follows only one player = R3/audio-owner · FX dies when other leaves = R4 ·
"it changed my difficulty" = R5.

## 5. Open unknowns (only hardware can settle)

- Whether the retail -1 UB is a *deterministic* crash or intermittent corruption (heap-layout
  dependent). The test plan treats either as R1-confirmed.
- Whether TU5's compiled `ProcessConfig` matches Wii source exactly at the MILO_FAIL site
  (recommend a 5-minute Ghidra-8002 disasm check at 0x8276FA08 before DLL work — expected: the
  format-string load remains but no abort call).
- R6 visual/BeatMatcher shared-read desyncs under the clone — theory says benign; only in-song
  play with the clone active proves it.
- Nothing in this wave touches online/leaderboards; local play only, per the design doc.

**Files of record:** `rb3/src/system/beatmatch/{PlayerTrackConfigList.cpp, PlayerTrackConfig.h,
TrackWatcherImpl.cpp, SongData.cpp, MasterAudio.cpp, BeatMatcher.cpp, GameGemList.cpp,
BaseGuitarTrackWatcherImpl.cpp}`, `rb3/src/system/os/Debug.h`, `rb3/src/band3/game/{Game.cpp,
SongDB.cpp, GemPlayer.cpp}`, `rock-band-3-deluxe/_ark/dx/overshell/dx_overshell_funcs.dta`,
`rock-band-3-deluxe/_ark/ui/overshell/slot_states.dta`,
`rb3-xenon/docs/plans/rb3enhanced-same-instrument-patch.md`, `/tmp/si-hw-fix/wave2/pins.json`.
