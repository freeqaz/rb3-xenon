# DROPWALK: mid-song pause menu, drop-out, drop-in and sit-out walks (2026-11-23)

Lane dropwalk, branch `dropwalk` (from main `4123d5744`), worktree `~/tmp/wt-dropwalk`.
Targets the mid-song paths that `project_rb3app_walk_coverage_2026-11-23` listed as never walked:
the pause menu's options (resume, restart, change difficulty, quit, game options / no-fail),
a player dropping out from the pause menu, a pad dropping in mid-song, and a band member sitting
out a section. Scratch, logs and binaries: `~/tmp/dropwalk/` (`r_<tag>/run.log`,
`r_<tag>/screens.txt`). Scripts: `tools/rb3_app_walks/dropwalk/`. modwalk's `disc.sh`/`runb.sh`/
`build.sh` with `A=~/tmp/dropwalk`; `disc.sh` also takes `P1`/`P2`/`P3` (per-pad scripts), `EP`
(the whole `RB3_APP_EXTRA_PADS` value) and `SKIP` (`RB3_APP_AUTOPLAY_SKIP`). `runs.sh` launches the
set. Every run went through `coord/no610m.sh` and the shared `gpu1.lock`, with
`RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_FOCUS_LOG=1 RB3_APP_MOD_LOG=1`, on a fresh
reflink copy of `memcard/content_a`. The full native selftest was not run (left to the
coordinator, as briefed).

## Result

**One native bug, fixed.** When a player confirmed "leave" from the in-song pause menu, rb3-app
aborted on the failing stub `Synth360::ActivateLocalChat(int, bool)` [ring online-netsession].
The stack ran `OvershellSlot::RemoveUser → SessionMgr::RemoveLocalUser(Impl) →
NetSession::RemoveLocalUser → XboxSession::RemoveLocalFromSession → Synth360::ActivateLocalChat`.
Every function on that path is fuzzy 100 in main's `report.json`. The stub was the only native
difference. `RemoveLocalFromSession` (0x823EEEF8) calls `ActivateLocalChat` unconditionally, so
this was **every** local-user removal from the session, on a menu as well as in a song. No earlier
lane had walked a drop-out.

Every other path behaves as retail's code says (negative results, table below).

## Commits

| commit | what |
|---|---|
| `a867c3ab1` | `native/src/rb3_app_native_glue.cpp`: `Synth360::ActivateLocalChat` gets retail's empty-mics arm (`// ring: native-real`). `native/src/rb3_app_link_stubs.s` regenerated with `tools/rb3_app_link_stubs_gen.py`: the file was emptied, rb3-app linked with `--error-limit=0` (142 undefined), and the generator was rerun. Functions went 139 → 138; the only stub removed is that one. The rest of the diff is `.Lname_N` renumbering |
| `f34cfa0f3` | `native/src/rb3_app_chartplay.cpp`: `RB3_APP_CHARTPLAY_MISS_TO=n` (opt-in harness knob: with `_MISS=100 _MISS_FROM=a _MISS_TO=b`, pad 0 sits out gems [a, b) and plays the rest). Walk scripts |
| (this) | this record |

No `src/` change, so there is nothing to price with `ab_measure`. No native file was added, so
`native_stub_census.py` needs no new classification. The new body sits in
`rb3_app_native_glue.cpp`, which the census already reads as `native-real`.

### The fix, against retail

Retail `Synth360::ActivateLocalChat` 0x82B5BBA8 (`va_disasm.py 82b5bba8 64`):
`lwz r11,0xa8(r3); lwz r10,0xac(r3); cmplw cr6,r11,r10; beq cr6,<ret>; bl MicManagerXbox::GetInstance;
bl MicManagerXbox::ActivateLocalChat`. It does nothing when `mMics` is empty. Natively,
`TheXboxSynth` is a never-constructed, zeroed `Synth360` with an empty `mMics` (A23's reasoning,
already used for the sibling native-real `Synth360::GetChatData`), so the native body is that
empty-mics arm with `MILO_ASSERT(mMics.empty())`.

## Binaries

- `bin0` `e97b18c66991`: plain rb3-app at `4123d5744` plus the MISS_TO knob (behaviour-neutral
  unless set).
- `bin1` `191c635b5545`: `bin0` plus the ActivateLocalChat fix.
- `bin1a`: `bin1`'s tree with `RB3X_SANITIZE=address,undefined` (default build type;
  `RelWithDebInfo` fails to link here with 14 undefined inline accessors, so do not pass it).

## Walk set

The band is modwalk's: pad 0 vocals runs the walk; pads 1–3 are guitar (Easy), drums (Easy) and
bass (Medium, `RB3_APP_AUTOPLAY_SKIP=3`, so it misses everything, fails, and the band loses). The
pause is pad 0's Start at `game_screen` +1500 (frame 2689); pads 1 and 3 press at their own +1500
(frame 2617). The in-song options list is built by `overshell_dir.dta`
`update_in_game_options`. With quickplay's `modes.dta` flags (`show_pause_restart`,
`show_change_difficulty`, `pause_menu_quit_token overshell_quit`), the predicted list was
return, restart, change_diff, game_options, quit, leave. The focus log showed exactly that, sel 0–5.

**Control.** `base` and `base2` (bin0) are IDENTICAL over 20,000 frames (`rb3_app_det_cmp.py`).
Every variant first diverges at its own first scripted press: frame 2689 (pad 0 pause), 2617
(pads 1/3), or 536 (drop-in, where pad 3 skips the hub join). So each difference below is caused
by the walked path.

Prediction before running: some mid-song path would hit a stub or a native-only arm, because
none had been walked. The drop-out walk confirmed it. The rest came out clean.

| tag | path (focus log) | outcome | as retail? |
|---|---|---|---|
| base / base2 | no pause | bass fails (en 1, crowd −2.2), **lose_screen** at frame 3383 | control |
| pause | Start → options_in_game sel 0 `overshell_return` → A (2749 hidden) | game frozen 2689–2749, then the same lose (3567 = 3383 + the 60-frame pause + menu transitions) | yes |
| restart | sel 1 `overshell_restart` → `confirm_restart` sel 0 yes | `restart_sync_audio_net_screen` → `game_screen` 2813: beat −8.08, every score 0, every player crowd 0.8666, bass re-enabled (en 0), later loses again (4951) | yes (`Game::Restart`, `Band::Restart`, `Player::Restart` all 100%). `Band::mCrowdRatings` keeps the pre-restart −0.7727 through the count-in until the first rating change (frame ~3300). `Band::Restart` (100%) never touches it, so that is retail |
| diff | pad 1 (guitar): sel 2 `overshell_change_diff` → `choose_diff` Easy→Medium → `overshell_difficulty_confirm` sel 0 `overshell_continue` | `u1 diff` 0→1 at 2858. The player's gem status is cleared and resized (hit counter restarts at 0), and autoplay keeps hitting Medium gems with 0 misses (16 hits by 3300) | yes (`GemPlayer::ChangeDifficulty`, `GemTrack::ChangeDifficulty`, `SongDB/SongData::ChangeTrackDiff`, `ConfirmChooseDiff`, `BandUser::SetDifficulty` all 100%) |
| quit | sel 4 `overshell_quit` → `confirm_quit` yes | `meta_loading_quit_screen` → `song_select_screen` 2917 → pad 0 picks a song → `part_difficulty_screen` with every slot back at choose_part | yes. The walk parks there because pads 1–3 had already used up their 1,800-poll `wait_screen` (a harness limit, not the game) |
| nofail | sel 3 `overshell_game_options` → sel 1 `mod_no_fail_band` → B, B | `nofail=1` from frame 2899 (mods log). The bass keeps failing but the band no longer loses; it reaches coop_endgame | yes (`SetBandNoFail`, `IsNoFailActive`, `CheckCrowdFailure` 100%; modwalk covered the same flags set on the hub) |
| leave (bin0) | pad 3 (failed bass): sel 5 `overshell_leave` → `overshell_confirm_remove_user_in_song` (default no, sel 1) → up → yes | **abort at frame 2886**: `UNREACHED stub [ring online-netsession] Synth360::ActivateLocalChat` | **native bug → fixed** |
| leave1 (bin1) | same | the failed bass (en 1, crowd −0.54) leaves at 2887. `Band` erases its `mCrowdRatings` entry, the band crowd goes 0.61 → 1.00 on the three left, and the song ends at **coop_endgame** (frame 10181) where base lost. UNREACHED 0 over 20,000 frames | yes (`RemoveUser`, `AttemptRemoveUser`, `NetSession::RemoveLocalUser`, `Game::RemovePlayer`, `TrackPanel::HandleRemoveUser` 100%) |
| dropin | pad 3 never joins on the hub; at +1500 it presses Start (`kState_Join` `on_start` → `attempt_to_add_user`), then A, A | `choose_part` with bass preselected (2618) → `choose_diff` (2678) → Easy → slot hidden (2738). A fourth player appears at 2740 in enabled-state 3 (dropping in), is enabled (0) by 3500, then misses everything (not autoplayed) and floors at crowd 0.2000 (Easy). The song completes | yes (`AttemptToAddUser`, `Band::AddPlayerDynamically`, `TrackPanel::DoHandleAddPlayer`/`PostHandleAddPlayer`, `BandCrowdMeter::DropInPlayer` 100%; quickplay `prevent_in_game_drop_in FALSE`) |
| sitout (Easy) | pad 0 chartplay guitar sits out gems 40–199 (`_MISS=100 _MISS_FROM=40 _MISS_TO=200`); pads 1–2 autoplay drums/bass with `RB3_APP_AUTODEPLOY=1` | `song done: gems 313 acted 153 skipped 160`. Pad 0's crowd falls to 0.2000 and **holds** with en 0, then climbs back to 1.0 when it plays again; coop_endgame, 5 stars | yes: `Player::CheckCrowdFailure` never disables an Easy player (`GetDifficulty() == kDifficultyEasy` → `SetDisplayValue(0)`) |
| sitoutm (Medium) | same, pad 0 on Medium (`+300 down` on choose_diff) | fails at 2607 (en 1), **saved** by a bandmate's deploy at 2912 (en 2 → 0, crowd 0.8666). Fails 3400, saved 3681. Fails a third time at 4163 and is **not** saved although both bandmates deploy (4400, 4800). The band crowd falls to 0.1995 and the band loses at 5173 | yes: retail `Player::Saveable` 0x826A2540 is `mEnabledState(+0x280) == 1 && mTimesFailed(+0x284) < 3 && +0x2CC == 0`, so the third failure is final |

`FAIL` lines: 3 per 4-pad walk (2 in the sit-out walks). All are `NetSession.cpp:763
IsUserSignedIn(newUser)` when an extra pad joins (network, out of scope, also in modwalk).

## ASan + UBSan leg

`bin1a` (ASan+UBSan, the fix included) ran `leave` (tag `leaveA`) and `dropin` (`dropinA`) for
6,000 frames each, with deploywalk's `ASAN_OPTIONS=halt_on_error=0:…:alloc_dealloc_mismatch=1:
new_delete_type_mismatch=1` and `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0`. Both legs cover
their mid-song event (drop-out at 2887, drop-in at 2616–2740) and about 3,000 frames after it.

- **ASan: 0 reports** in either leg. UNREACHED 0, rc 0.
- **UBSan: 26 sites per leg, the same 26 in both** (UBSan reports each site once). Every one
  fires before det frame 1113, which is boot and song load, ahead of `game_screen`. None is on a
  drop-out or drop-in path. They are BinStream/ChunkStream/BandFaceDeform/BandHeadShaper misaligned
  loads, SongParser `TrackType` and GameGem `RGNoteType` enum loads, StandardStream bool 190 (the
  perturb fill), BeatMap float→int, and the engine's Keygen_Stub/mikktspace. All are load-time and
  shared with every rb3-app song walk; this lane did not triage them.
- `leaveA` is **IDENTICAL** to the plain `leave1` (bin1) over all 6,000 common frames
  (`rb3_app_det_cmp.py`), so the sanitizer build takes the same path, and the band crowd after the
  drop-out is the same (`crowds 1.0000/1.0000/1.0000` at frame 2890).

## Not done

- Drop-out from a menu (hub) was not walked separately. It goes through the same
  `RemoveLocalFromSession` call, so the fix covers it, but no walk shows it.
- Practice-mode pause options (`overshell_practice_new_section`), the drum metronome entries, and
  skip-song in a multi-song setlist (`can_skip_songs`): none of these is in quickplay's list.
- A drop-in on a pad that then plays real input (it was unplayed, on Easy), and a drop-in into an
  already-full band.
- Change difficulty with `overshell_restart` (the variant that confirms and restarts), and
  a vocals difficulty change.
- Changing a modifier other than no-fail mid-song (the delayed-effect modifiers say "next song" in
  `kState_ModifierDelayedEffect`).
