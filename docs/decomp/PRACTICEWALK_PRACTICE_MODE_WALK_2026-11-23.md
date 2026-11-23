# PRACTICEWALK: rb3-app driven through practice mode (2026-11-23)

Lane practicewalk, branch `practicewalk` (from main `cb24bc209`), worktree `~/tmp/wt-practicewalk`.
Scratch, logs and binaries: `~/tmp/practicewalk/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`).
Walk scripts are committed in `tools/rb3_app_walks/practicewalk/` (copy the `.txt`/`.py` files into
`~/tmp/practicewalk/walks/`; `disc.sh <tag> <bin> <walk> [frames] [env…]` is trainwalk's runner and
always sets `RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1`). Every run went through
`coord/no610m.sh` under the shared `gpu1.lock`, on a fresh reflink copy of `memcard/content_a`.
Movies were off (harness default). The full native selftest was not run (coordinator's job).

## Commits

| commit | what |
|---|---|
| `61fc67908` | **fix**: `StandardStream::UpdateTime`'s headless timer fallback scaled its wall-clock catch-up by nothing, so a practice/trainer start below 100% leapt ahead (finding 1) |
| `cc1423ca7` | **fix (harness)**: chartplay's `Summary("ended")` read the gem list the quit had already freed (finding 2) |
| (this commit) | walk scripts, `gdb_pct.py`, this record |

`61fc67908` priced with `tools/ab_measure.py --revert 61fc67908`: **Δmatched +0, Δhonest +0,
Δcode +0 B** (`name_check`; leg A = leg B = 55,268 matched, 59.665310%). Predicted Δ0: the change
is inside `#ifdef HX_NATIVE`. Run dir `.ab_measure_runs/20261123-172704-practicewalk-stream-3142147/`.

## How practice was observed

- Screens, list rows and pause-menu rows: the focus log (`sel=N:<token>`).
- Song clock: the det line's `seconds` field (song time). Its slope per 150 frames (5 s) reads the
  playback speed; a drop reads a loop.
- Results: `gdb_pct.py` (batch gdb, no debug info needed: SysV registers) logs every
  `GemPlayer::OnGetPercentHitGemsPractice(slot, startMs, endMs)` with its return value (the
  `percentage_callback` behind `hit_percent.lbl` and, at each loop, `best_percent.lbl`), every
  `Game::Jump`, `Game::SetMusicSpeed` and `PracticePanel::MarkGemsAsProcessed`.
- Real input: `RB3_APP_CHARTPLAY=1` with `RB3_APP_JOYPAD_KIND=guitar` and `=drums`.
- The flow was checked against the 360 dtb (`ui/practice/gen/practice.dtb`,
  `config/gen/modes.dtb`, `ui/overshell/gen/*.dtb`, decoded with trainwalk's `dtb.py`). Every C++
  function on the path (PracticePanel, PracticeSectionProvider, GameConfig section bounds,
  `Game::SetMusicSpeed/Jump/AdjustForVocalPhrases`, `GemPlayer::OnGetPercentHitGemsPractice/
  SetPitchShiftRatio/SetMusicSpeed`, `VocalPlayer::GetPracticeHitPercentage`) is fuzzy-100 in
  main's `report.json`, so the retail-code question reduces to the native-only arms under them.

## Screens reached and verdicts

Song: the deterministic seed's song (guitar part 217 gems on Easy, 329 on Medium; drums 563/770).

| area | walk (tags) | what happened | verdict |
|---|---|---|---|
| Section select | `w_disc2` (disc2_g/d), all | `practice_sel_section_screen` opens on row 2 `full_song`; rows 3.. are `prc_*` sections. Start = row 4 (`prc_main_riff_1` guitar / `prc_verse_1` drums), end = row 5. | works as practice.dta |
| Speed select | all | `practice_sel_speed_screen` `speeds.lst` rows `practice_speed_0..3`; row 3 picked | works |
| Speed 70% | `w_full` fG/fD, disc2 | song clock +3.4998 s per 5 s = **0.700** | works (after finding 1, start offset fixed) |
| Speed changes in game | `w_full` | d-pad right ×3 at 70%: slopes 0.8, 0.9, 1.0; left: 0.9. gdb: `SetMusicSpeed` 0.8/0.9/1.0/0.9 | works (right = faster, as `inc_speed`) |
| Looping | all | guitar: `Game::Jump 28306.7` + `MarkGemsAsProcessed` every loop, the clock drops from ~58-60 s to ~28-31 s; drums 86→32 s. Loop period 1350 frames for main_riff_1..verse_1b at 100% (45 s) | works (jump = section start − 4 beats ×2 when <2 s, as `jump_to_start`) |
| Pause menu | all | `options_in_game`: 0 return, 1 restart, 2 practice_new_section, 3 change_diff, 4 game_options, 5 pause_quit (no leave, no metronome: modes.dta `show_metronome_options FALSE`) | as overshell_dir.dta |
| Restart | `w_full` | `restart_sync_audio_net_screen → sync_audio_net_screen → practice_game_screen`; speed kept (gdb `SPEED 1.000` then `0.900`, the chosen speed), same jump target | works |
| New section | `w_nsec` nG/nD | pause row 2 → `practice_sel_section_screen` reopens on the previous **end** section (`initial_section` = end+1); new range main_riff_1..verse_1b (drums verse_1..chorus_1) at 100%; loops at the new end | works |
| Change difficulty | `w_diff` dGg/dDg, `w_full` | row 3 → `choose_diff` (easy, medium…) → medium: new gem list (329), song resumes, loops reach 100% again | works |
| Quit | `w_full` fD | row 5 → `meta_loading_quit_screen → song_select_screen` | works |
| Results | gdb legs | at each loop end the percent is **100.00** with chartplay (24 events on Easy, 33-37 on Medium); control `RB3_APP_CHARTPLAY_MISS=50` (mGg): **40.00** at the loop end (17 acted, 19 skipped) | works; the instrument discriminates |

## Findings

### 1. Headless song clock leapt ahead at every sub-100% practice start (fixed, `61fc67908`)

`StandardStream::UpdateTime` has an `HX_NATIVE` fallback for no audio device (all harness runs:
`AudioDevice: skipped`): once 500 ms of wall time pass with the raw stream time under 10% of it, it
resets `mTimer` to `mStartMs + wallElapsed` and reads `mTimer` from then on. `mTimer` is a
`VarTimer` running at `mSpeed` (`VarTimer::Ms` = `mSpeed * raw + mAccumMs`), so the catch-up was
the unscaled wall time. Measured on det run disc2_g (70%): the clock sits at 28.2670 s for 16
frames, then steps **+0.5120 s** in one frame (frame 1902), where 0.7 × that wall time was due.
After the fix (bin-p1, run nG, same frames): **+0.3520 s**, and the clock from then on is 0.16 s
behind the old binary (frame 2000: 30.9377 vs 31.0977). Retail has no such path (real audio
clocks the stream). At 100% the two are identical. It affects det baselines only for practice and
trainer starts below 100%.

### 2. ASan+UBSan: one harness use-after-free on quit (fixed); the game adds nothing

`bin-asan-p0` (`-DRB3X_SANITIZE=address,undefined`, `ASAN_OPTIONS` as `tools/native_asan.sh`),
`w_full` with chartplay ran to the 590 s timeout (rc 124): guitar aG to frame 15,669, drums aD to
18,310 (past the quit). UBSan: 26 report lines per leg, every site already listed by
ASANWALK/ASANWALK2/UBSANTRI (`StandardStream.cpp:685`, `SongParser.cpp:234`, `GameGem.h:84/85`,
`BeatMap.cpp:90`, `ChunkStream.cpp:501`, `BinStream.cpp:33`, the bandobj `Delta` alignment group,
engine `mikktspace.c`/`Keygen_Stub.cpp`). No new site.

ASan: guitar 0; drums **2 heap-use-after-free**, both at the pause-menu quit (frame 18,219):
`GameGemList::NumGems` ← `Summary` (`rb3_app_chartplay.cpp:217`) ← `RB3AppChartPlayButtons`, freed
by `GameGemDB::~GameGemDB` ← `SongDB::~SongDB` ← `Game::~Game` ← `GamePanel::Unload`. On the poll
after the Game goes, chartplay prints its "ended" summary through the stale `gSong.list`; the plain
run fD printed `song ended: gems -792862159`. Harness code, not the game: every chartplay walk that
leaves a song this way has it. The fix prints the count cached at bind (`gSong.numGems`). Check
(`w_quit`, drums, 4,200 frames, same walk): bin-asan-p0 **2** reports, bin-asan-p1 (`7eb52cc49cd6`)
**0**, and both print `song ended: gems 563 acted 60 …` and reach `song_select_screen`.

### 3. Harness limits found (not game bugs; listed, not changed)

- **Chartplay holds guitar frets through a pause.** `RB3AppChartPlayButtons` keeps `gSong.frets`
  down while the song clock stands still, so a scripted `a`/`b` of a held colour gives no edge. Seen:
  `b` did not close the guitar pause menu (disc2_g, 7745), `a` on `pause_quit` did nothing (fG,
  18206); the same presses work on drums (one-poll presses). Walks navigate the guitar pause with
  strum up/down and an `a` early in the pause. A fix would release frets while `TheGame` is paused.
- **Chartplay does not re-seek on a forward jump.** Its cursor resets only when the clock goes back
  >1 s. Practice starts at 0 and jumps forward to the section, so the first poll presses gems 0..11
  at once (`late 12 (max 16646.8 ms)`). Those gems lie before the scored range, so the percent is
  unaffected.
- **Leftover scripted presses land in gameplay when a `wait_screen` times out.** In fG a
  `restart_sync_audio_net_screen` wait missed a 3-frame screen; later `a` presses (green) landed in
  play after the Medium change and the loop ended at 0.00%. The clean rerun dGg (no stray presses)
  reads 100% on every Medium loop. `w_full`'s later blocks are therefore misaligned; the per-area
  walks `w_nsec` and `w_diff` are the reliable ones.
- The headless stream also stalls for ~0.5 s (16 frames) at every start before the fallback
  engages (all songs, all speeds). Pre-existing, left alone.

## Not done

- Vocals practice (`practice_vocal_end_screen`, guide/score part toggles, harmony) beyond what
  menuwalk covered; `confirm_practice_screen` from the lose screen; `game_options` from the practice
  pause; the qp_practice entry. Pitch-shift audio (`set_pitch_shift_ratio`) was not checked by ear:
  no audio device in harness runs.
- The pause-list row memory (reopening on the last row) was not judged against retail; it is
  UIList/dta behaviour.
- No harness change beyond finding 2; the full native selftest (coordinator).
- Binaries: bin-p0 `026212c2eb4c` (main `cb24bc209`), bin-p1 `b8e0a7e7add7` (`61fc67908`),
  bin-asan-p0 `09105bbc49c7` (main), bin-asan-p1 `7eb52cc49cd6` (both fixes). Nothing pushed.
