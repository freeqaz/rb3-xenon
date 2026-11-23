# TRAINWALK2: later trainer lessons, lesson completion, save and reload (2026-11-23)

Lane trainwalk2, branch `trainwalk2` (from main `8001d870d`), worktree `~/tmp/wt-trainwalk2`.
Follows `TRAINWALK_TRAINING_MODES_WALK_2026-11-23.md`, which walked each trainer's first lesson.
Scratch, logs and binaries: `~/tmp/trainwalk2/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`).
Scripts are committed in `tools/rb3_app_walks/trainwalk2/`: trainwalk's `disc.sh`/`runb.sh`/`build.sh`
with `A=~/tmp/trainwalk2` (and `runb.sh` takes `CONTENT=<store dir>` in place of
`memcard/content_a`, as tourwalk's does), `gen.py` (writes a walk for trainer index *t*, *n*
lesson-list downs and *k* sections), and two batch-gdb files. Every run used
`RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1`, went through `coord/no610m.sh` and the shared
`gpu1.lock`, and started on a fresh reflink copy of a store. The full native selftest was not run
(left to the coordinator, as briefed).

## Commits

| commit | what |
|---|---|
| (this branch) | `native/src/rb3_app_chartplay.cpp`: the chart player re-seeks when a trainer reloads its gem list in place, so `RB3_APP_CHARTPLAY` can play a trainer lesson (harness fix, not a game fix); walk scripts; this record |

No `src/` change, so nothing to price with `ab_measure`. No native file was added.

## Binaries

- `bin-p0` `634aaaec7e95`: plain rb3-app at `8001d870d`. `bin-a0`: the same tree with
  `RB3X_SANITIZE=address,undefined`.
- `bin-p1`: `bin-p0` plus the chartplay change.

## Prediction

Some bug would show up in the parts trainwalk left out: completion bookkeeping, the
save/load of `mLessonCompletions`, or a later lesson's challenge. **That was wrong.** No game
bug was found on any path below. The only defect was in the native input harness.

## What was walked and what it showed

| run | pad, mode | walk | reached | verdict |
|---|---|---|---|---|
| DC (gdb) | drums, autoplay + autodeploy | `w_dc` | `drum_kit_intro` all 5 sections → **trainer_finished_screen** (frame 6004) → yes → **meta_loading_quit_screen → training_screen** → `drum_basic_beats` all 5 sections → trainer_finished (10418) → quit → training_screen | works (findings 1, 2) |
| RL2 (gdb) | drums | `w_reload`, on DC's saved store | splash → main_hub (no calibration) → training_screen, drum lesson list | works (finding 3) |
| SP (gdb) | drums, autoplay | `w_speed` | drum_kit_intro section 1 at 95% then 90% (two d-pad downs), back to 100% (two ups), then lesson_finished | works (finding 4) |
| BB / ABB | drums, autoplay | `w_d_bb` | `drum_basic_beats`, 3 sections, lesson_finished ×3 | works |
| HF / AHF | drums, autoplay | `w_d_hf` | `drum_hard_fills` (`lessons.lst` focus `sel=10:drum_hard_fills`), 3 sections | works |
| PD / APD | drums, autoplay | `w_pd_af` | pro drums (after hint_prodrums_screen) `pro_drum_advanced_fills`, 3 sections | works |
| RGPC / ARGPC | proguitar (22-fret), autoplay | `w_rg_pc` | `rg_power_chords`, 3 sections on rg_trainer_screen | works |
| RKMB / ARKMB | keys, autoplay | `w_rk_mbc` | `rk_moving_between_chords`, 3 sections on rk_trainer_screen | works |
| CP, CPL | drums, **chartplay** | `w_dc` | first section only: 30 hits, then every later gem missed | harness bug (finding 5) |
| CP1 | drums, chartplay, `bin-p1` | `w_dc` | sections 1-4 pass on real pad input (lesson_finished at 2572, 3561, 4550, 5539; 57 hits, 0 misses); section 5 waits on a deploy | works after the fix |
| WR | gamepad | `w_wrong` | trainers.lst `trainer_drums` → **basic_confirm_dialog_small_screen** (`trainer_wrong_instrument`) → ok → training_screen | works |

Across all of these, `^FAIL` was 0 and `UNREACHED` was 0, and every run ended rc 0. The A-prefixed
runs are `bin-a0` (ASan+UBSan, `halt_on_error=0`, 7,000 frames each). They logged **0 AddressSanitizer
reports**. Their UBSan sites, reduced to file:line, are a subset of trainwalk's
`~/tmp/trainwalk/all.sites` (`comm -23` printed nothing): mikktspace, BandFaceDeform/HeadShaper
misalignment, GameGem/SongParser/StandardStream enum loads, BinStream/ChunkStream misalignment,
Keygen_Stub. trainwalk's `Metronome.cpp:31` site stays gone.

## Findings

### 1. Lesson completion works and follows retail (DC, `gdb_lesson.cmd`)

Each passing section at 100% calls `TrainerPanel::SetLessonComplete(0)` (once per success, three
per section) → `BandProfile::SetLessonComplete(<section name>, 1.0)`. Examples:
`trainer_drm_snareonly_1`, `hatsonly_2`, `tomsonly_5`, `kickonly_6`, `odfill_7`. Only the first
call, which raises the stored speed, runs `CheckForFinishedTrainerAccomplishments`. The header
symbol is written once, when the minimum over every section's stored speed turns nonzero.
That happens on the last section: `BP::SetLessonComplete drum_kit_intro 1.000000`, and the same
for `drum_basic_beats`. The last section ends on its first success: the dta `loop` handler tests
`all_sections_finished` before the three-success rule, and trainer_finished_screen follows.

Retail check with `tools/va_disasm.py`:
- `BandProfile::SetLessonComplete` (0x8258D360): find, compare (`bge` skips when stored ≥ new),
  `operator[]`, `CheckForFinishedTrainerAccomplishments` (0x8258AF20), `stb 1 → mDirty`. This is
  our body.
- `ProTrainerPanel::SetLessonComplete` (0x826AE710): tests `song_lessons` and calls
  `TrainerPanel::SetLessonComplete` (0x826CAAC8) when it is 0. Otherwise it takes
  `get_speed_modifier` and `max` into `mSpeedCompleted[GetCurrSection()]`.
- `RGTrainerPanel::SetSongSectionComplete` (0x826ADEF0): loops difficulty 0..diff and branches on
  `mIsBass` (0x274) for the section call and again for the AllSectionsFinished → lesson call.

All three are the same call order as ours.

### 2. Completion is saved on the way out of the trainer

Choosing yes on trainer_finished_screen (`quit_early`) goes through `meta_loading_quit_screen`,
whose `kMetaLoading_AutoSave` state calls `saveload_mgr autosave`. Order observed:
`SLM::AutoSave` → `BP::SaveFixed`, then training_screen (frames 6080-6097, and again 11962-11979).
Back on the list, `training_mgr get_current_lesson` restores the selection (`sel=1:drum_kit_intro`),
and `LessonProvider::Custom` checks the box: `IsLessonComplete drum_kit_intro 1.0` →
`SetChecked 1`, with the other rows `SetChecked 0`. The stored `save.dat` differs from
`content_a`'s in 1,416,995 bytes, same size (4,434,217 B).

### 3. Reload restores it (RL2, on a reflink snapshot of DC's store, `snap_dc`)

Boot goes splash → `BP::LoadFixed rev=152` → main_hub, with no calibration. The lesson list
shows `drum_kit_intro` **and** `drum_basic_beats` checked (`SetChecked 1, 1, 0`, …). The three `IsLessonComplete` calls before the checkbox ones are the
category progress count, whose return value gdb did not print. So `mLessonCompletions` makes
the round trip through `SaveStd/LoadStd(hash_map<Symbol,float>, 1000, 8)`.

Walk note: on a returning boot, main_hub drops a d-pad press that comes 60 frames after it
appears. The first reload walk (RL) went into career instead. `w_reload` waits 300 frames.

### 4. Trainer speed control (SP, `gdb_speed.cmd`)

Two d-pad downs → `SetSpeedRatio 0.95`, `0.90`. Successes at 90% record
`TP::SetLessonComplete idx=2` → `BP::SetLessonComplete trainer_drm_snareonly_1 0.900000`, and
no lesson_finished appears, because the `loop` handler needs `speed_index 0`. Two ups → `0.95`,
`1.00`. Three successes at 100% store 1.0 and push lesson_finished.

Measured song clock (det `seconds` over `ui`):
- frames 1600-2000 (mixed 1.0/0.95/0.90): 12.36 s of song in 13.33 s. Predicted from the
  press frames: 12.41 s.
- frames 3400-3800: 13.33 s in 13.33 s.

So `Game::SetMusicSpeed` really slows the native song stream.

### 5. Chartplay could not play a trainer: harness bug, fixed (native only)

With `RB3_APP_CHARTPLAY=1`, the first drum section scored 30 hits, and then every gem was missed
(CP: `hit 30 miss 611` by frame ~11,000; no lesson_finished).

Cause (CPL, `RB3_APP_CHARTPLAY_LOG=1`): the trainer reloads the watcher's **same**
`GameGemList` object for every loop and section (`GemTrainerPanel::CopyGems` → `SetGems`).
After a reload, gem times are later and indices restart at 0. The song clock never rewinds
(det `seconds` 20.9 → 36.9 across the loop). So neither of chartplay's reset tests fired, the
list pointer or a ≥ 1 s clock rewind, and its cursor stayed past the reloaded gems.

Fix: the player also starts over when the list's gem count or first gem time changes, and
seeks to the first gem at or after now. It now logs a new `gem list … starting at gem 16/32` on
every loop (CP1), and passes sections 1-4 on pad input with 0 misses.

Section 5 of `drum_kit_intro` is the overdrive lesson (trainwalk finding 3). Chartplay fills
energy to 1.0 but plays no fill, so it never deploys: a walk limit, as before.

Regression check: a chartplay song walk (`chartplay/walk.txt`, drums, 9,000 frames) on `bin-p0`
vs `bin-p1` reads **IDENTICAL over 9000 common frames** (`tools/rb3_app_det_cmp.py`; 452 hits,
0 misses both). A normal song's list never changes size or first gem.

## Checked and listed, not changed

- **Pro song-lesson section bits for section ≥ 32.** `SongStatus::SetBitmapLessonComplete` and
  `CheckLessonBit` compute `1 << bit`. Retail uses `slw` (0x825D3660, 0x825D2438), which gives 0
  for a shift of 32-63. So retail never sets or reads a section ≥ 32. Natively, x86 masks the
  shift to 5 bits, and section 32+n would alias bit n (it is also C++ UB).
  - Unreachable on disc content: a count of `begin_pg`/`begin_key` tokens over all 86 extracted
    song MIDIs tops out at 20 (freebird, `begin_pg`) and 17 (llama, `begin_key`).
  - So no fix. A DLC chart with more than 32 trainer sections would expose it.
- Pro song lessons (`SetSongSectionComplete` → SongStatus bits → `SongStatusMgr` save) were
  checked against retail by disassembly only (finding 1). No song lesson was played to
  completion and reloaded.
- Pro guitar and pro keys later lessons ran on autoplay only. Chartplay2's raw-report path was
  not tried in a trainer, and the gem-list fix above applies to it too.
- Not walked: the chord book (`overshell_relearn_chords`), `trainer_must_signin`, the `opt2`
  (no) arms of lesson_finished and trainer_finished, the vocal trainer (not in `trainers.lst`),
  and every lesson past the ones named in the table.
