# memcheck uninitialised-value origins on the headless native targets (2026-11-23)

Lane `stackuninit`: worktree `~/tmp/wt-stackuninit`, branch `stackuninit`, base main `6c9c5077d`.

This lane follows lane perturb (`PERTURB_UNINIT_HEAP_READS_2026-11-23.md`, on branch `perturb`, unlanded).
That lane fixed the heap origins it found but left two things undone:
- The stack-origin errors in rb3-harmony (about 5,265) and rb3-vocal2 (22) were never triaged. Its one open
  lead was a `VocalPlayer::FindBestPart` local that reached printed output.
- memcheck was never run on rb3-ark, rb3-dta, rb3-frame or rb3-render.

**Result.** Apart from perturb's heap origins, every uninitialised-value error across the 18
`native_health` runtime targets comes from **three origins**:
- **Stack, `VocalPlayer::FindBestPart`'s `fTargetPitch`.** This is rb3-harmony's 70-context, ~5,227-error
  block. Retail has the same unset read, but natively it reached output, so it is **fixed natively**
  (`HX_NATIVE`, X360 Δ0, commit `4cbc6045c`).
- **Stack, `SongParser::mTrackType`** (the parser lives on `main`'s stack). Seen in 10 targets.
  **Retail-faithful and harmless**, not changed.
- **Heap, `CharPollableSorter::Dep::searchID`** in rb3-render. This is **char**, so it is listed and not fixed.
  It is retail-faithful by the existing header note and harmless.

rb3-ark, rb3-dta and rb3-frame have **0** uninitialised-value errors. Scratch, scripts and logs are in
`~/tmp/stackuninit/`.

## 1. Method

- **Build:** the worktree's native build, configured with `-DCMAKE_C_FLAGS=-g -DCMAKE_CXX_FLAGS=-g` so memcheck
  can print file:line. The shipping native build sets no build type, so both are `-O0`; `-g` adds debug info
  only. Script: `~/tmp/stackuninit/build.sh`.
- **Runner:** `~/tmp/stackuninit/runv.sh <tag> [targets]` runs
  `valgrind --tool=memcheck --undef-value-errors=yes --track-origins=yes --num-callers=24 --error-limit=no`.
  - Arguments: each target gets `native_health.sh`'s argv (lines 373-394), with the NVIDIA 615 ICD.
  - Every run goes through `coord/no610m.sh`, with `DEBUGINFOD_URLS=https://debuginfod.archlinux.org`.
  - Time caps: 540 s per target, 1,500 s for rb3-render.
  - Logs: `v_<tag>/<target>.vg`.
- **Grouping:** `~/tmp/stackuninit/vgsum.py <dir>` keys each error on its origin: the allocation kind plus the
  first two non-allocator frames of "Uninitialised value was created". For each origin it lists the first
  non-libc error site.
- **Retail:** every origin was checked against `band.exe` with `tools/va_disasm.py`.
- **Error counts:** they are per process (`ERROR SUMMARY`). memcheck does not split them by context without
  `-v`, so the per-origin error counts below are differences between runs.

## 2. Error summaries

| target | base (main `6c9c5077d`) | after `4cbc6045c` | log |
|---|---|---|---|
| rb3-harmony | 20,427 errors / 81 contexts | **15,200 / 11** | `v_base/`, `v_fix/` |
| rb3-vocal2 | 4,260 / 7 | 4,260 / 7 | `v_base/`, `v_fix/` |
| rb3-score3 | — | 41 / 7 | `v_fix/` |
| rb3-gem, rb3-hit, rb3-score | — | 34 / 6 each | `v_fix/` |
| rb3-crowd, rb3-score4, rb3-vocal | — | 22 / 6 each | `v_fix/` |
| rb3-midi, rb3-milo, rb3-save, rb3-score2, rb3-song | — | 0 | `v_fix/` |
| **rb3-ark, rb3-dta** (first memcheck) | — | **0** | `v_fix/`, and `v_pt/` on perturb's binaries |
| **rb3-frame** (first memcheck) | — | **0** uninit; 4 NVIDIA-driver API notes | `v_pt/`, `v_gpu/` |
| **rb3-render** (first memcheck) | — | 982 errors / 8 contexts at the cap (4 H3 + 4 driver notes); **partial** (§2 note) | `v_pt/`, `v_gpu/` |

**rb3-render is partial.** Under memcheck it does not finish inside the 1,500 s cap. Two runs exist, both rc
124 by the cap:
- On perturb's binaries (`v_pt/`) it reached 51 of 414 `[PASS]` gates and ended at 982 errors / 8 contexts.
- On this lane's build (`v_gpu/`) it was still running when this record was written; see
  `~/tmp/stackuninit/HANDOFF.md`.

In both runs, all uninitialised-value contexts come from H3. The other 4 contexts are driver notes. So
rb3-render's later cells (venues, characters) have not been covered by memcheck.

All 16 non-GPU targets exited rc 0 under memcheck, each printing native_health's success line (`v_fix/*.out`).

**What the fix removed.** In rb3-harmony it removed exactly the 70 `FindBestPart` contexts and 5,227 errors.
Nothing else moved: the remaining 11 contexts are unchanged.

**How main's counts relate to perturb's.** Main still carries perturb's heap origins, because `perturb` is
unlanded: rb3-harmony has 5 contexts and about 15,162 errors from them, and rb3-vocal2 has 1 context and 4,238
errors. With both branches applied, the expected remainder is:
- rb3-harmony: 5,265 − 5,227 = 38 errors, all of them SongParser's.
- rb3-vocal2: 22 errors, as perturb measured.

## 3. Origins and verdicts

| # | origin (memcheck "created by") | read at | targets | verdict |
|---|---|---|---|---|
| S1 | stack, `VocalPlayer::FindBestPart` (`fTargetPitch`) | `VocalPlayer::Poll` :786, `SendVocalState` → `PackFloats` :1154-1162 and `Clamp`, and the harness `snprintf` (main_harmony.cpp:451) | rb3-harmony (70 contexts, 5,227 errors) | **retail-faithful, but reached native output; fixed natively** (§3.1) |
| S2 | stack, `main` (and `RunBandUnisonStage` in rb3-score3): a `SongParser` local, member `mTrackType` +0xF8 | `SongParser::OnEndOfTrack` SongParser.cpp:234, then `SongData::OnEndOfTrack` :846 or `NativeGemSink::OnEndOfTrack` main_gem.cpp:130 | crowd, gem, harmony, hit, score, score3, score4, vocal, vocal2 (6 contexts each, plus 1 in score3) | **retail-faithful, harmless** (§3.2) |
| H1 | heap, `Singer::Singer` → `new TalkyMatcher` (`VoiceBeat::mVoiceEnergy`/`mFullBandEnergy`) | `CouldScoreAgainstPart` :218, `GetBestHit` :836, main_harmony.cpp:426-427 | rb3-harmony | retail-faithful; **already fixed on branch `perturb`** (`caf143186`), not duplicated here |
| H2 | heap, `VocalPlayer::PostLoad` → `new VocalPart` (`unk38` +0x3C) | `Min` in `ScoreSinger` | rb3-harmony, rb3-vocal2 | retail-faithful; **already fixed on branch `perturb`**, not duplicated here |
| H3 | heap, `map<Object*, CharPollableSorter::Dep>::operator[]` node (`Dep::searchID` +0x10) | `CharPollableSorter::ChangedByRecurse` (CharPollGroup.cpp:189 in the native build) under `Character::SyncObjects` | rb3-render (4 contexts) | **char: listed, not fixed** (§3.3) |
| — | NVIDIA userspace 615.78 (2 `realloc` with size 0, 2 `posix_memalign` with size 0, during ICD/EGL init) | driver | rb3-frame, rb3-render | not engine code; out of scope |

### 3.1 S1: `FindBestPart`'s `fTargetPitch`

- **The unset store, in retail.** `CouldScoreAgainstPart` `0x826F2B90` handles an unpitched (talky) note at
  `0x826F2C1C..0x826F2C6C`. When autoplay is on, or when `unk1 && !unk0 && overEnergy`, it branches to
  `0x826F2C70` (`li r3,1`) and returns with no store through r23, the `outPitch` reference.
  - The pitched hit stores first, at `0x826F2CEC` (`stfs f30,0(r23)`).
  - The miss path stores 0, at `0x826F2CD4` (`stfs f25,0(r23)`).
  - The source has the same shape (VocalPart.cpp:213-220).
- **The read, in retail.** `FindBestPart` `0x826E4BF0` passes `0x50(r1)` as `outPitch` on every pass of the
  part loop (`addi r8,r1,0x50` at `0x826E4C6C`). It reads the slot only after a true return with a score
  above the best, at `0x826E4CBC` (`lfs f0,0x50(r1)` / `stfs f0,0(r24)`). Nothing initialises the slot.
- **What retail does with it.** The slot carries the previous part's value from pass to pass. So only a talky
  hit on the first part, or one after nothing but talky hits, copies out stack garbage. Retail does that too.
- **Natively.** The garbage flows into several places:
  - `Singer::mBestTargetPitch` (VocalPlayer.cpp:708), and from there `mFrameTargetPitch` (:724).
  - Poll's `bHasBestTarget` (:786), which decides `ClearFreestyleDeployment`.
  - The octave-offset branch (:810).
  - `SendVocalState`'s detune `Clamp` and `PackFloats`.
  - rb3-harmony's printed `tgt` column.
- **Fix (`4cbc6045c`, `HX_NATIVE` only).** One `fTargetPitch` is hoisted above the loop and cleared to 0 once.
  This keeps retail's carry from part to part and replaces only the garbage, using the value the miss path
  stores. The X360 declaration stays inside the loop, so the X360 build is unchanged.
- **X360 price.** `tools/ab_measure.py --worktree ~/tmp/wt-stackuninit --from-dirty`, ruler
  `functionRelocDiffs=name_check`, objdiff `sha256:ec3badc0f33323e9`. Leg B recompiled 1 MSVC obj.
  - Deltas: Δmatched +0, Δmasked_equal +0, Δhonest +0, Δcode% +0.000000 pp, Δcode_bytes +0, Δfuzzy
    +0.000000 pp. The `none` control was +0.
  - Leg A: matched 55,268, honest 29,908, code 59.665310%.
  - Units at 100%: 635 (mpn) and 548 (fuzzy) in both legs.
  - Run dir: `.ab_measure_runs/20261123-035248-stackuninit-findbestpart-651589/`.
- **Output change: none on today's input.** Plain rb3-harmony output is byte-identical before and after the fix
  (`~/tmp/stackuninit/plain/harmony_base1.out` vs `harmony_fix.out`), and so is rb3-vocal2's.
  - `perturb=190` does not change it either, before or after.
  - So at `-O0` the slot's garbage happened to be 0 on this input. memcheck's taint is what showed the
    dependence. The fix removes the dependence on stack history; it does not change behaviour that was
    already visible.

### 3.2 S2: `SongParser::mTrackType` (unchanged verdict of asanwalk N6, re-checked here)

- **The read.** Retail `SongParser::OnEndOfTrack` `0x827849E0` reads +0xF8 at `0x82784A6C` (`lwz r11,0xf8(r28)`,
  then `cmpwi 4` and `cmpwi 5`) and passes the bool to the sink's slot 3. This was re-read for this lane.
- **The only store.** In the source it is `Reset` (SongParser.cpp:201). The asanwalk record finds the same
  in retail (`Reset` at `0x82788F48`; the ctor and `ReadMidiFile` do not store it). So the first track's end
  reads garbage on the 360 too.
- **Harmless.** The track index passed alongside it is still `mTrack = -1`.
  - `SongData::OnEndOfTrack` tests `b && i < mGemDBs.size()`; -1 converts to unsigned and fails the size test.
  - The harness `NativeGemSink::OnEndOfTrack` guards on `trk >= 0`.
  - So no merge happens, whatever the bool is.
- **Not changed.** An init would only quiet memcheck.

### 3.3 H3: `CharPollableSorter::Dep::searchID` (char, listed)

- **The read.** `Dep()` initialises `obj` and `poll` and leaves `searchID` alone. The header
  (char/CharPollable.h:16) says retail's `map::operator[]` does the same. `ChangedByRecurse` compares the
  fresh node's `searchID` with `sSearchID` before storing it (CharPollGroup.cpp:189-191 is the copy native
  compiles; Character.cpp:1136 holds a second copy).
- **The header's retail citation was not re-verified.** It cites `0x82376B18`, which on clean TU5 is
  mid-function. Retail's `ChangedByRecurse` is `0x8236E980`.
- **Harmless in practice.** A false early-out needs the garbage to equal the current search ID. Only then is
  that dep's changed-by chain skipped, giving a wrong poll order.
- **Not fixed:** char is outside this lane's fix scope.

## 4. Areas outside the fix scope

The brief excluded Rnd/Draw/Text/Spotlight, ui, char and the net sync/game-start code. The only origin that
fell in them is **H3** (char, `CharPollableSorter`, rb3-render). None came from Rnd/Draw/Text/Spotlight, ui or
net sync/game-start.

## 5. Not done

- `native_health.sh --selftest` was not run, per the brief. These runs are memcheck runs with native_health's
  argv, not the health gate.
- perturb's heap fixes (H1, H2) live on unlanded branch `perturb` and were not re-applied here. The two
  branches touch disjoint files (`VoiceBeat.cpp`, `VocalPart.cpp` vs `VocalPlayer.cpp`).
- Error counts per origin are differences between runs (§2), not memcheck's per-context counts.
- The worktree's native build dir is configured with `-g` (`~/tmp/stackuninit/build.sh`). It is a scratch
  build dir, not shipped config.
- rb3-render under memcheck covers only its first ~51 gates (§2). At 51 gates in 25 min, a full pass needs a cap of about 3.5 h,
  or a run per cell.
