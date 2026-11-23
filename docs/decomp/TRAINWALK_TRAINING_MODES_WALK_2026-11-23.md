# TRAINWALK: rb3-app driven into the training modes (2026-11-23)

Lane trainwalk, branch `trainwalk` (from main `7a05edbf4`), worktree `~/tmp/wt-trainwalk`.
Scratch, logs and binaries: `~/tmp/trainwalk/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`).
Walk scripts and helpers are committed in `tools/rb3_app_walks/trainwalk/` (copy them into
`~/tmp/trainwalk/walks/`; `disc.sh <tag> <bin> <walk> [frames] [env…]` is menuwalk's runner
with `A=~/tmp/trainwalk`, and always sets `RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1`).
Every run went through `coord/no610m.sh` under the shared `gpu1.lock`, on a fresh reflink copy
of `memcard/content_a`. Pads: `RB3_APP_JOYPAD_KIND=drums|keys|proguitar` (gemwalk, keyswalk).
The full native selftest was not run (coordinator's job, as briefed).

## Commits

| commit | what |
|---|---|
| `d13cde51e` | **fix**: `GemTrainerPanel::CopyGems` read every live Gem's time out of freed memory (heap-use-after-free, all three gem trainers); **fix**: `Metronome::OverrideEnabled` had no enumerators, so the value 2 every trainer passes was out of range natively |
| (this commit) | walk scripts, `dtb.py`, this record |

## Tools added

- `tools/rb3_app_walks/trainwalk/dtb.py <file.dtb>` decrypts an RB3 360 dtb (new-style crypt:
  first 4 bytes are the seed, the stream is XORed with the low byte of the 0x41A7/0x1F31D
  Park-Miller step) and prints it as dta text. Binary layout as read: a `01` byte, then arrays
  as `u16 count, i32 line` (no id field) and nodes as `u32 type` + payload; `kDataUnhandled`
  (6) and `#else`/`#endif` (8/9) carry a 4-byte payload. It decoded all of `ui/` and `config/`
  in the extracted ark, which is where every screen name and list order below was checked.
  The earlier lanes said the dtbs "do not grep as text"; this is why.
- `gdb_frs.cmd`: batch-gdb breakpoints on `FreestylePanel::Swing/SetBpm/CreateController`,
  `Metronome::PlayBeat` and `Sequence::Play` (with a 3-frame backtrace), run as
  `WRAP="gdb -batch -x gdb_frs.cmd --args"`. The det line hashes neither sound nor input, so
  this was the only way to see that freestyle does anything.

## Screens reached and verdicts

The training menu is main_hub `mb_trainers` → `training_band_practice` (menuwalk) /
`training_trainers` / `training_pro_song_lessons` / `training_drum_freestyle`.
`trainers.lst` holds `trainer_drums, trainer_pro_drums, trainer_real_guitar,
trainer_pro_keyboard` (no 5-lane guitar, bass or vocals trainer in that list); each opens
`lessons.lst` (category rows, lesson rows; `config/trainer_config` lists them).

| area | pad | walk | screens reached (all new to the walks) | verdict |
|---|---|---|---|---|
| Training menu | drums | `t_disc`, `t_trl` | main_hub → **tv6_a_screen → training_screen** (`trainers.lst` sel 0-3 named as above) | works |
| Drum trainer | drums | `w_tdrums`, `w_tdrums_long` (+AUTOPLAY) | trainer_drums → `lessons.lst sel=1:drum_kit_intro` → preloading → **drum_trainer_screen → gem_preview_screen → drum_trainer_screen → lesson_finished_screen** (×4) | **bug, fixed** (finding 1). Sections 1-4 pass under autoplay; section 5 never does without a deploy (finding 3); with `RB3_APP_AUTODEPLOY=1` it passes and **trainer_finished_screen** follows (LD2, frame 6004) |
| Pro drum trainer | drums | `w_tprodrums` | trainers.lst sel 1 → **hint_prodrums_screen** → training_screen `lessons.lst sel=1:pro_drum_kit_intro` → drum_trainer_screen → lesson_finished_screen | works (same panel as drums, same fix) |
| Pro guitar trainer | proguitar (22-fret) | `w_trg`, `w_trg_long` | keyswalk's campaign prefix → trainers.lst sel 2 → `rg_note_basics` → **rg_trainer_screen** → lesson_finished (×7) → **trainer_finished_screen** → (yes) **meta_loading_quit_screen → training_screen**; in-trainer overshell `options_in_game`: return, game_options, quit_trainer, leave | works. 0 ASan reports even before the fix (presumably no section there needed more gems than the list held; not checked) |
| Pro keys trainer | keys (keytar) | `w_trk`, `w_trk_long` | trainers.lst sel 3 → `rk_identifying_gems` → **rk_trainer_screen** → lesson_finished (×4) → trainer_finished_screen (frame 5641) → quit → training_screen → next lesson | **bug, fixed** (finding 1; 24 reports before) |
| Pro song lessons | proguitar | `w_pslg` | mb_trainers → training_pro_song_lessons → song_select (setlist rows unnamed, as menuwalk) → part_difficulty (`choose_part_real_guitar`, `choose_diff`) → preloading → **gem_preview → rg_trainer_screen → lesson_finished_screen** | works |
| Pro song lessons | keys | `w_pslk` | as above, straight to `choose_diff` → **rk_trainer_screen → lesson_finished_screen** | works |
| Drum freestyle | drums | `t_frs`, `w_freestyle` (+gdb) | mb_trainers → training_drum_freestyle → **drum_freestyle_screen**; Start opens overshell `options_in_game` (return, metronome, …) | **works** (finding 2) |
| Trainers on the wrong pad | — | — | not walked (the `trainer_wrong_instrument` dialog) | — |

## Findings

Prediction at the start: these screens would show link-stub aborts or `FAIL`s, as the
previously unwalked pads did. **Wrong.** Over 27 walks: `^FAIL` 0, `UNREACHED` 0. The bugs
showed up only under ASan+UBSan.

### 1. The gem trainers read freed memory at every section start (fixed, `d13cde51e`)

ASan (bin-asan-t0, the long walks, 30000-frame cap hit the 540 s wall cap at ~17,400 frames):
**64** heap-use-after-free reports in the drum trainer walk, **24** in pro keys, 0 in pro
guitar, all in the first 7,000 frames. Stack: `Gem::RemoveAllInstances` (Gem.cpp:424, reading
`mGameGem.mMs`) ← `GemManager::ClearAllGems` ← `GemTrainerPanel::CopyGems` ←
`StartSectionImpl`; freed by `std::vector<GameGem>::reserve` in `GameGemList::SetGems` called
from the same `CopyGems` one line earlier. Every `Gem` holds `const GameGem &` into that
vector, and `SetGems` reallocates it whenever the new section needs more gems.
**Retail has the same order** (`tools/va_disasm.py 826abbb8 300`: `bl 0x8278E398` SetGems,
then `bl 0x82B99E68` ClearAllGems, then `bl 0x82B9B838` ClearMissedPhrases), so the match
build keeps it; the fix is an `HX_NATIVE` arm that calls `ClearAllGems` before `SetGems`. The
read only feeds the times passed to `RemoveAt`, and `ClearAllGemWidgets` runs right after, so
the visible effect natively was nil or small; the fix removes undefined behaviour from every
trainer section start. After (bin-asan-t1, same walks, 7,000 frames): **0** reports in all
three. The det records of the fixed plain binary are **IDENTICAL** to the pre-fix binary over
12,000 frames (drums `T1D` vs `LD`; keys `T1K` vs `LRK`) and under
`glibc.malloc.perturb=190` (`T1P` vs `T1D`). That was predicted, since the det hash does not
cover widgets.

### 2. Drum freestyle works: pads, BPM, kit, solo applause and metronome

The det line hashes neither input nor sound, so freestyle was checked under batch gdb
(`gdb_frs.cmd`, run `gF2`, walk `w_freestyle`). On entry, `SetBpm 100` and
`CreateController`. Then 16 pad presses a/b/x/y → `Swing` slots 4/1/3/2 (green, red, blue,
yellow, as the drum gem map says), each followed by one `Sequence::OnPlay` (the
`pad_hit` handler's cue). The solo was 16 hits 15 frames apart (7.5 s, gaps < 0.7 s), and
`HandleSolo` then played applause (the > 5 s arm). D-pad up → `SetBpm 120`, and Select changed
the kit. Start opens the overshell (`options_in_game`, sel 1 `overshell_metronome`). `a`
toggles `metronome.chk` and calls the panel's `enable_metronome`. After the overshell closed,
`Metronome::PlayBeat` ran **71** times, cycling 0-3, from frame ~1918 to 3200. Predicted from
the BPM changes (120 until two d-pad downs at ~2511 and ~2551, then 100 and 80): ~70.5. No beats
play while the overshell has focus (`Poll` returns early when the panel is not focused), and
that is what the code does.

### 3. Drum lesson section 5 needs an overdrive deploy (walk limit, not a bug)

Without `RB3_APP_AUTODEPLOY`, the 5th section of `drum_kit_intro` (from frame 5414) never
reaches `lesson_finished`. It scores 279 hits and 0 misses through frame 12,000, and energy
fills to 1.0. `gem_trainer.dta`'s `loop` handler only finishes a section after 3 successes
at speed 0. With `RB3_APP_AUTODEPLOY=1` (deploywalk) the same walk logs `autodeploy frame
5726 … deploying 1` and reaches `trainer_finished_screen` at frame 6004. So this section is
the overdrive lesson. The keys and pro guitar lessons walked reached trainer_finished without a deploy.

### 4. New UBSan site in the trainers: `Metronome.cpp:31` (fixed, `d13cde51e`)

Against scorewalk S2 and keyswalk KS/GS, the trainer walks added exactly one UBSan site:
`load of value 2, which is not a valid value for type 'Metronome::OverrideEnabled'`, from
`GemTrainerPanel::Poll`. That function passes 2 while another panel has focus, and
`PracticePanel::Poll` does the same. The enum had no enumerators, so 2 was outside its range.
Retail `Metronome::Poll` (0x826F0930) compares the argument with 1 (`cmpwi r30,1`) and 2
(`cmpwi r30,2`). The enum now names 0/1/2, which changes no codegen or mangling. After the
fix the site is gone, and the fixed ASan walks add **0** sites beyond S2 ∪ KS ∪ GS.

### 5. Keyswalk's open `trainers_guitar_tutorial_01_screen` bounce is the missing movie player

The panel is a `MoviePanel` (`ui/trainers/training.dta`). Its `movie_done` handler earns
`acc_guitartutorial01` and does `ui goto_screen accomplishment_screen`. Natively the
MovieImpl never opens, so `MoviePanel::Poll` (`src/system/meta/MoviePanel.cpp`) sends
`movie_done` on the first poll, as its own comment says. The bounce is the native port having
no Bink playback, so it is not a trainer bug. One side effect: the accomplishment is earned
without the video. Listed, not edited (a movie player is out of this lane's scope).

### 6. Listed, not changed

- `src/band3/meta_band/LessonMgr.cpp` `~LessonMgr`: the first `HX_NATIVE` loop bound is
  `mCategoryToLessonsMap.end()` while iterating `mTrainerToCategoriesMap`, so the "keep the
  correct bound natively" fix was applied to the wrong loop. It is harmless: a hash_map's
  `end()` is the null-node iterator, so either map's `end()` compares equal (retail
  0x825695D8 walks each bucket list until the node is null). The destructor also never runs
  in rb3-app. Not edited.
- No vocal trainer is reachable from `trainers.lst` (four entries). `VocalTrainerPanel` was
  not walked.
- Nothing pointed at `BandCharacter.cpp`, `src/system/ui`, `rndobj` or `char`. The
  pre-existing UBSan sites in `bandobj/BandFaceDeform`/`BandHeadShaper` (misaligned `Delta`)
  and `beatmatch` are all in S2.

## Not done

- The full native selftest (`tools/native_health.sh --selftest`): left to the coordinator.
  Only rb3-app (plain and ASan+UBSan) was built and run here.
- No walk reached past the first lesson of each category, the `opt2` (no) arm of
  lesson_finished, the trainer speed controls, the chord book (`overshell_relearn_chords`), or
  the `trainer_wrong_instrument` / `trainer_must_signin` dialogs. Pro drums used the drums pad,
  which has no cymbal flags.
- The pro guitar and pro keys trainers ran on autoplay only. Real pro input (the raw report) is
  zero natively (keyswalk).
- Binaries: bin-t0 `203dfddbda87` (main `7a05edbf4`), bin-t1 `66e79ba3ada1` and bin-asan-t1
  `bae769598d7b` (`d13cde51e` minus a later comment-only wording change). Not landed, nothing
  pushed.
