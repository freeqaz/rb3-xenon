# rb3-app: scripted input through calibration to main_hub_screen (lane postcal, 2026-11-22)

Branch `postcal` (worktree `~/tmp/wt-postcal`), from main `df0f6f45a`. Inputs:
`docs/decomp/PROFPIC_RB3APP_GAMER_PICTURE_2026-11-22.md` and
`MEMCARD_RB3APP_MEMCARD_XBOX_2026-11-22.md`. Before this lane a returning scripted Start stopped on
`first_time_calibration`, waiting for input.

## The scripted input (private, not committed)

profpic's hack (`Joypad_Stub.cpp`, Start at poll 40, SaveLoadManager trace) plus a press schedule:
`RB3_SCRIPT="<when>:<button>,..."`, where `<when>` is a poll number or `<screen>+N` (N polls after
that screen became current with no transition), and presses fire in list order, each held 4 polls.
Saved as `~/tmp/postcal/scripted_input_postcal.diff`; driver `~/tmp/postcal/run.sh` (fresh reflink
copy of memcard's first-boot store `~/tmp/memcard/content_a` per run, `no610m.sh`, data dir
`rb3/orig-assets/xbox-zip`, `RB3_APP_RUN=1`). Logs are `~/tmp/postcal/run_*.log`.

The player's inputs, read off the screens' DTA (`ui/splash/splash.dta`, `ui/calibration/calibration.dta`,
`ui/hints/hints.dta`, `ui/main/main_hub.dta`):

| screen | press | why |
|---|---|---|
| first_time_calibration | A | `opt1.btn` (calibrate) pops to cal_welcome_screen |
| cal_welcome_screen | right, A | `cal_mode.lst` is horizontal (down does nothing, run r1); third mode `cal_numbers` -> cal_manual_audio_screen |
| cal_manual_audio_screen | right, A | audio lag +5 ms, confirm -> cal_manual_video_screen |
| cal_manual_video_screen | A | confirm -> cal_manual_complete_screen |
| cal_manual_complete_screen | A | `saveload_mgr autosave`, goto main_hub_screen |
| hint_goalcomplete_screen | A | continue (pushed by meta_loading's hint step) |
| hint_rb3_welcome_screen | down, A | `continue.btn` |
| main_hub_screen | A | Play Now |

(Skipping calibration, down+A on first_time_calibration, also reaches main_hub_screen: run r2.)

## Blockers met, in order, and the fixes

1. **meta_loading_main_screen never advanced** (runs r3-r5). Calibration's complete panel earns
   `acc_calibrate`, so main_hub's TRANSITION_COMPLETE goes to meta_loading_main_screen
   (`main_hub.dta` check_rewards_and_hints). A DTA trace showed its `state` stuck at 1
   (kMetaLoading_Entered) with `content_mgr refresh_done` 0: the state waits for the content refresh.
   Retail RefreshDone (0x8251E510) is `mState == 1`; natively TheContentMgr was a base ContentMgr,
   whose StartRefresh is empty, so mState stayed 0. **Fix:** rb3-app compiles retail
   `os/ContentMgr_Xbox.cpp`; TheContentMgr is a subclass of XboxContentMgr that overrides only
   TitleContentPath (retail "UPDATE:", natively the TU host dir). New shims
   `XContentCreateCrossTitleEnumerator` / `XEnumerateCrossTitle` enumerate the content store (args
   read off StartRefresh 0x82520FD0: bl at 0x825212F0 with r4=0, r5=type, r7=1, r8=NULL; bl at
   0x82521340 with r5=0x138, r6=NULL). Two native-width defects in that TU were fixed on the way:
   `XboxContent::mLicenseBits` is now DWORD (XContentCrossTitleCreate's out type; did not compile
   natively), and PollRefresh's `filename - 0x108` is `offsetof(XCONTENT_CROSS_TITLE_DATA,
   szFileName)` (0x108 on X360, wider natively). Result (r6): meta_loading advances to the hint step.
2. **`SyncStore::Poll` stub** (r7, ring online-net): meta_loading's check_advance runs
   `ui sync_screen`, and NetSync::SyncScreen (0x8259CB48) polls TheSyncStore. Retail Net::Init news
   it (operator new(0xC) at 0x823E06B0, ctor 0x823EA300 at 0x823E06C0) before NetSession::New.
   **Fix:** `net/SyncStore.cpp` compiled; the offline Net::Init gate builds `mSyncStore` in that
   order and the offline Net::Poll polls it (retail 0x823E03CC). It stays empty natively because
   Synchronizable::Publish's HX_NATIVE arm does not insert (pre-existing, not changed).
3. **Crash in the memcard save thread's MakeString** (r9 `double free`, r10 gdb: unbounded
   NextBuf -> MakeString recursion from `MCContainerXbox::Mount` <- `MemcardMgr::ThreadCall_SaveGame`,
   r11 SIGSEGV). NextBuf walked `gThreadIds` (int[6]) through an `unsigned long *`: 8 bytes natively,
   so each step read two ids and skipped a slot, and live threads' slots looked free or vice versa.
   Retail loads 4 bytes with stride 4 (lwz 0x827C3E08, addi r29,r29,4 at 0x827C3E1C; lwz r3 at
   0x827C3E40 before the ValidateThreadId bl at 0x827C3E44, stride at 0x827C3E58). **Fix:** `DWORD *`.
   Result (r12): 1500 frames, rc 0, through to main_hub_screen.

`rb3_app_link_stubs.s` was regenerated with `tools/rb3_app_link_stubs_gen.py` (empty, link,
generate): 168 -> 165 functions, 5 -> 4 vtables, 1 typeinfo. The name diff is exactly
`SyncStore::Poll`, `SyncStore::SyncUser`, `SyncUserMsg::SyncUserMsg` and the SyncAllMsg vtable;
ContentMgr_Xbox.cpp added no stub. Ring online-net is now empty. The `TheSyncStore` hand data stub in
`rb3_app_link_stubs.cpp` is gone. No new file under `native/src`, so `native_stub_census.py` needed
no new classification.

## Gates and sabotages

Construct-only runs (`RB3_APP_CONSTRUCT_ONLY=1`); logs `~/tmp/postcal/run_construct.log`,
`run_sab{A,B,C,D}.log`. Both new gates run right after the Net::Init gate.

| build | gate line | rc |
|---|---|---|
| branch | `[PASS] … SyncStore: TheNet.mSyncStore is TheSyncStore and polls`; `[PASS] … content refresh: XboxContentMgr refresh_done after 5 PollRefresh (state 1, 1 root, 0 packages, enumerators released)` | 0 |
| A: glue subclass back on base ContentMgr | `[FAIL] … TheContentMgr is NOT an XboxContentMgr …` | 7 |
| B: XEnumerateCrossTitle leaves the XOVERLAPPED pending | `[FAIL] … after 16 PollRefresh: state 2, refresh_done 0, …, live enumerators 0 -> 1` | 7 |
| C: no `new SyncStore()` in the offline Net::Init | `[FAIL] … TheNet.mSyncStore=(nil) TheSyncStore=(nil)` | 7 |
| D: NextBuf's old `unsigned long *` | a MakeString thread gate **still passed** (8 sequential threads; then 24 interleaved with the main thread) | 0 |

The MakeString gate could not fail, so it was dropped (commit says so). The NextBuf fix rests on
the end-to-end runs: old stride r9/r10/r11 crashed in the save thread's MakeString, fixed r12 and
`run_final.log` ran to the frame cap with rc 0. The sabotages were reverted with
`git checkout HEAD -- <file>`.

## How far scripted input gets now

`run_final.log` (1800 frames, rc 0, 53 PASS lines): Start -> first_time_calibration (frame 59) ->
cal_welcome -> cal_manual_audio -> cal_manual_video -> cal_manual_complete -> autosave (SLM
1, 0x3D, 0x3E, 0x3F, 0x46, 0x6B, 0) -> main_hub_screen -> meta_loading_main_screen ->
hint_goalcomplete_screen -> meta_loading_main_screen -> main_hub_screen -> hint_rb3_welcome_screen ->
**main_hub_screen** (frame 468), stable to frame 1800.

**What stops it:** nothing aborts. Four A presses on main_hub_screen (polls 528..618) change no
screen; the log shows two `No screen to pop`. Play Now is a MainHubPanel sub-state
(`set_state kMainHubState_PlayNow`, focus `pn_quickplay.btn`, `main_hub.dta`), so a press would not
change the screen anyway; whether the panel state moved was not traced. That is the next thing to
look at (trace `{main_hub_panel get state}` / the focused component per press).

## X360 measurement

Changed X360 sources: `os/ContentMgr_Xbox.h` (mLicenseBits DWORD), `os/ContentMgr_Xbox.cpp`
(offsetof), `utl/MakeString.cpp` (DWORD *): all the same types or constants on X360.
`tools/ab_measure.py --revert 79633851d` (run `20261122-211004-postcal-x360-width-revert`), ruler
`functionRelocDiffs=name_check`, objdiff `ec3badc0f33323e9`, leg B recompiled 3 objects:
matched 55,268 -> 55,268, masked_equal 25,360 -> 25,360, honest 29,908 -> 29,908, code% 59.665310 ->
59.665310, `none` matched 55,322 -> 55,322; every Δ 0, units at 100% unchanged (635 / 548).

## Not done

- `tools/native_health.sh --selftest`: not run (not landing). `xdk_shims.cpp` (two new extern "C"
  functions and a gate counter) and `MakeString.cpp` reach every native target; only rb3-app was
  built and run.
- Main hub navigation beyond the first A press, and the other calibration paths (cal_manual tap
  calibration, cal_auto needs a calbert guitar).
- The pre-existing `FAIL: Couldn't find key (ui/ui.dta, line 150)` and `preview_db` lines (also in
  profpic's runs).
