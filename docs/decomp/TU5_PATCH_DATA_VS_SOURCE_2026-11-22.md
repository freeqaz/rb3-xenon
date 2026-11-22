# TU5 patch data vs our source — what TU5's data asks of the executable (2026-11-22)

Lane `tu5audit`, read-only. Goal: find code bugs the official TU5 content
(`orig/45410914/tu5-package/gen/patch_xbox.hdr` + `patch_xbox_0.ark`, 52 entries)
would expose. The executable we decompile is TU5, but every native run so far read
the launch-disc versions of these files. No source was changed.

## Method and instruments

- Extracted all 52 patch entries, plus the disc counterpart of each from
  `~/code/milohax/rb3/orig-assets/xbox-zip/gen/main_xbox.hdr`, with the
  `native/tools/ark_extract.py` reader. Output and scripts are in `~/tmp/tu5audit/` and
  `~/tmp/tu5audit_tools/` (not committed).
  - 42 entries have a disc copy; 10 do not (`ui/audition/gen/{audition.dtb,main.milo_xbox,session.milo_xbox}`,
    `ui/startup/gen/startup_madcatz_keep.milo_xbox`, five `ui/locale/*/gen/locale_updates_keep.dtb`,
    `songs/gen/missing_song_data_updates.dtb`). The `checksum_xref.txt` count of 7 new
    files covers `.dtb` only; the three new `.milo_xbox` files are the rest.
- Wrote a `.dtb` decoder (`dtb2dta.py`): u32 plaintext seed, then a Rand2-XORed stream
  (the same cipher as the `.hdr`), then `u8 present` and a DataArray
  `{i16 size, i16 line, i16 deprecated, nodes}`. Node payloads follow `DataNode::Load`.
  `#else`/`#endif`/`kDataUnhandled` carry a 4-byte int. Check: all 76 files (34 pairs plus
  8 new) decode to the end of the stream with zero trailing bytes. `band_version` reads
  `"Build: 100807"` (disc) and `"Build: 110902"` (TU5).
  Note: `.dtb` keeps `#ifdef` directives, so platform-gated blocks are visible.
- Diffed the decoded text of every replaced file (`~/tmp/tu5audit/all.diff`, 1,658 lines).
  I read every hunk except the body of `locale_credits_keep` (a credits text reshuffle plus
  a new Analog Devices/Xiph licence block, with no code-facing keys).
- Per item: a retail-string census over `orig/45410914/band.exe`, NUL-delimited
  (`symcheck.py`: file offset == VA − 0x82000000, verified on `AuditionMgr::Init`'s
  `"audition_mgr"` literal at 0x82098094), and `rg -w` over `src/band3 src/system src/network`.
  Then I read the consuming function for anything that was not a plain handler name. The
  rule I applied: a name that is absent from retail and present only in data is
  data-defined (script `{func}`, screen handler, type prop), so it needs no executable
  support. I checked this for the absent names by scanning every disc `.dtb`
  (`scan_disc.py`). For example, `is_allowed_to_preview_offer`, `play_instr_sfx`,
  `get_track_panel` and `quit_early` are all defined in disc data.
- `.milo_xbox`: wrote a block decompressor (`milo_census.py`, magic `0xCDBEDEAF`, 0x01000000
  flag = stored block) and compared the class tables, disc against TU5.

## Headline findings

1. **MISSING: `AuditionMgr::Handle` (retail 0x825638B0), and with it every `audition_mgr`
   script call.** The header declares it but no body exists. Ghidra decompile of
   0x825638B0 lists 18 handler symbols: `apply_settings are_drum_fills_enabled
   can_enter_audition enter_audition exit_audition get_download_progress
   get_loaded_song_status get_network_status get_venue_sym has_autoplayer
   has_validation_failed is_ready is_validating remove_autoplay_users set_venue_sym
   start_net_poll start_validation toggle_drum_fills`. It also handles three messages:
   `SigninChangedMsg` → 0x82561FD0, `ConnectionStatusChangedMsg` → 0x82562228 and
   `ServerStatusChangedMsg` → 0x82562418. All 18 names are in retail and appear 0 times in
   our source. TU5 data calls them from `modes.dta` (the `audition` mode's
   `enter`/`exit`/`allow_drum_fills`/`win_screen`/`endgame_screen`), `audition.dta`
   (transfer and session panels), `overshell.dta` and `slot_states.dta`
   (`can_enter_audition`). EnterStart and EnterEnd can only be reached through
   `enter_audition`/`exit_audition`, so without Handle the audition state machine never
   starts.
2. **MISSING: the `kRBNMemberSignedOut` (5) sign-out path.** TU5 `macros.dta` adds
   `kRBNMemberSignedOut = 5`, and `band_ui.dta`'s `sign_out` `on_dismiss` maps it to
   `error_club_member_signed_out`. In retail, AuditionMgr's `SigninChangedMsg` handler
   (0x82561FD0) raises it. When state ≠ 0 and the RBN member user (+0x38) signs out, it
   builds `init` with `li r11,5` and calls `UIEventMgr::TriggerEvent(sign_out, …)`.
   Nothing in our tree sends reason 5. The other sign-out raisers (`Campaign.cpp:721`,
   `CriticalUserListener.cpp:59`, `UGCPurchasePanel.cpp:169`, `Tour.cpp:502`) are
   pre-TU5 reasons.
3. **MISSING: the rest of the audition back end.** `AuditionSessionBuilder` is declared
   only; none of its methods has a body. `AuditionMgr.h:18` says so. Its methods are
   GetSlotState 0x825EADC8, GetSlotTrack 0x825EADF8, GetSlotDifficulty 0x825EAE10,
   SetSlotAutoplay 0x825EBE10, NextSlotDifficulty 0x825EB1A0, NextSlotTrack 0x825EB0D0,
   Reset 0x825EBB08, Finish 0x825EBF28 and the ctor 0x825EC118. AuditionSessionPanel's
   handlers exist but call these. Also unwritten in AuditionMgr:
   `EnterState4`/`EnterLoad`/`EnterState7`/`PollLoad`/`SendToScreen`.
   `AuditionDownloader` and `AuditionNetClient` are declared only. Native links none of
   this: `native/src/rb3_app_link_stubs.cpp:63` makes `TheAuditionMgr` an 8-byte blob,
   and `AuditionMgr::Init` is gated in `rb3_app_gates.txt`. Native therefore has no object
   named `audition_mgr` either.
4. **MISSING: UGCNet config (`ugcnet` / `packets_per_chunk`).** TU5 `band_keep.dta` adds
   `(ugcnet (packets_per_chunk 20))`. Retail reads it at 0x82B82478, the only `lis/addi`
   reference to the literal at 0x8219CB70. That address falls in the `splits.txt` span
   headed `JsonUtils.cpp`, next to `"UGCNet::SetState: Unhandled state %d"`. Our source
   has no `UGCNet`, `ugcnet` or `packets_per_chunk`. This is the PC→360 audition transfer
   client. It is 360-only network, so it is out of scope for funding per CLAUDE.md, and
   only the config key is recorded here.
5. **The native port never mounts the TU5 patch.** `ArchiveInit` (`os/Archive.cpp:389`)
   merges `%s/gen/patch_%s` only when `TheContentMgr.TitleContentPath()` is non-empty.
   The base `ContentMgr::TitleContentPath()` returns `nullptr`
   (`os/ContentMgr.h:141`), and only `ContentMgr_Xbox` overrides it. The match build is
   correct (HANDLED), but a native host will read disc data until it supplies a title
   content path or mounts `patch_xbox` itself. This is the root cause of "every native
   run read the disc versions".
6. **Our TU5-faithful code needs TU5 data (disc data trips it).** In these places the
   source is retail-TU5-correct, and the disc data is what is out of date. They explain
   failures native would hit on disc data. None is a code bug, but all of them argue for
   mounting the patch (item 5):
   - `Game::Properties` (`band3/game/Game.cpp:1894`) reads
     `TheGameMode->Property("allow_drum_fills", true)`. Only TU5 `modes.dta` defines it
     (disc scan: 0 hits), so every Game construction fails the property lookup on disc data.
   - `BandDirector` `midi_cleanup_presets` is `OnMidiPresetCleanup(_msg->Int(2))`
     (`BandDirector.cpp:949`). Disc `midi_parsers.dta:1237` calls it with no argument and
     TU5 passes `TRUE`. `world_lighting_presets` sets `legacy_presets` for any
     `[lighting (...)]` or `verse`/`chorus` text event, so any such disc song's `term` reads
     node 2 of a 2-node array. I did not count how many disc songs carry those events.
   - `AccomplishmentPanel` `is_user_on_correct_instrument` takes `_msg->Obj<LocalBandUser>(2)`
     (`AccomplishmentPanel.cpp:1419`). Disc data calls it without `$user`.
   - `CustomizePanel` `save_prefab` takes `_msg->Str(2)` (`CustomizePanel.cpp:1741`).
     Disc `cheats_funcs` calls it bare. It is a cheat only.
   - The `PickShot`/`OnMidiShot5Cleanup` reads of `objects BandDirector generic_cam_cats`
     and `cam_cat_priorities` exist only in TU5 `band_objects.dta`. They are reached only
     from the TU5 RBN parsers, so they are safe on disc data.
7. **DIFFERS (known, pre-documented):** `BandSongMgr::AddSongData`
   (`BandSongMgr.cpp:~690`). Retail TU5 replaced the `IsInExclusionList` call with
   `li r3,0`, so TU5 never skips an excluded song. Our source still calls it, so the
   exclusion list is still honoured. This is recorded at the site (lane CR-3) as a
   deliberate one-instruction mismatch. Behaviourally it differs from TU5 whenever an
   exclusion list is populated.
8. **Naming notes (not bugs):** `Game::Properties::mUnkTU5_movieSync` is
   `TheGameMode->InMode("audition")`, so it means "in audition mode", not movie sync.
   `mUnkTU5_prop19` is the mode's `allow_drum_fills`, and `GemPlayer.cpp:1494` gates drum
   fills on it.

## Per-item verdicts

R = retail literal count (NUL-delimited), S = files in our src naming it. "data-only" = the
name is defined in data, and the absent-from-retail result was confirmed by the disc scan.

### config/

| File / item | What TU5 asks | R / S | Verdict |
|---|---|---|---|
| band_keep `(locale eng/locale_keep.dta eng/locale_updates_keep.dta)` | Locale reads N files | — | HANDLED: `Locale::Init` loops `cfg->Size()` files (`utl/Locale.cpp:212`). Native-only note: under `HX_NATIVE` the reads sit inside `if (mInitialized)`, which the code's own comment calls uninitialised (UB). Not TU5-specific. |
| band_keep `missing_song_data` = `#include missing_song_data_updates.dta` + `#merge missing_song_data.dta` | new keys `song_id`, `version`, `year_released`, `year_recorded`, `album_name`, `album_art`, `album_track_number`, `song (vocal_parts)` | — | HANDLED: `AddSongData` → `GetSongID(curArray, cfgArr)` (songs.dta first, missing data as backup), and `BandSongMetadata(cfgArr, curArray)` with `FIND_WITH_BACKUP` for every key. `DataArraySongInfo(main, backup)` handles the `song` sub-array. The ID order is asymmetric (map key prefers songs.dta, `mID` prefers missing data), but it is retail-faithful: AddSongData is 99.6% with its one mismatch elsewhere. |
| band_keep `(ugcnet (packets_per_chunk 20))` | config read | 1/1 / 0 | **MISSING** (item 4; out of scope) |
| band_keep `song_mgr max_song_count` under `#ifdef HX_WII` | Wii gate only | — | HANDLED (360 value unchanged at 3000) |
| band_version | string | — | HANDLED |
| macros `kState_Audition*` 81–85 | overshell slot states | — | HANDLED: `OvershellSlotState.h:89-93` |
| macros `kRBNMemberSignedOut` 5 | sign-out reason | — | **MISSING** sender (item 2) |
| macros `CAM_CAT_PRIORITIES`, `GENERIC_CAM_CATS` | used by band_objects | — | HANDLED (consumed by BandDirector, below) |
| metamusic_scenes: 3 audition screens | data lists | — | HANDLED (data-only) |
| midi_parsers `is_ugc_plus` → `{song_mgr get_meta_data …} is_ugc_plus` | BandSongMetadata handler | 1 / 2 | HANDLED: `BandSongMetadata.cpp:742` |
| midi_parsers `$banddirector rbn2_add_postproc sym sec` | BandDirector | 1 / 2 | HANDLED: `BandDirector.cpp:913`, body at 1864 |
| midi_parsers `midi_cleanup_presets TRUE` | arg added | 1 / 3 | HANDLED (needs TU5 data, item 6) |
| midi_parsers `midi_shot5_cleanup`, key `shot_5` | BandDirector | 1 / 2–3 | HANDLED: `BandDirector.cpp:950`, 1902, 2108 |
| midi_parsers `spot_keyboard` (note 41) | BandDirector prop | 1 / 2 | HANDLED: `BandDirector.cpp:1259`, 2181 |
| midi_parsers `lightpreset_keyframe` from text (`next/prev/first`) | same key, new source | 1 / 2 | HANDLED |
| midi_parsers MidiParser type props `enabled`, `post_procs`, `allow_keys` | typedef props read via `[ ]` | `post_procs`/`allow_keys` 0 / 0 | HANDLED (data-only typedef props; not traced through `MidiParser`, see "not done") |
| modes `allow_drum_fills` (default TRUE; audition: `{audition_mgr are_drum_fills_enabled}`) | Game reads it | 1 / 1 | HANDLED in Game; the audition value is **MISSING** via item 1 |
| modes `audition` mode (`enter_audition`, `exit_audition`, `has_autoplayer` …) | AuditionMgr handlers | 1 each / 0 | **MISSING** (item 1) |
| modes `nofail_allowed insta_rank update_leaderboards can_resume_songs demos_allowed joining_allowed*` | MetaPerformer etc. | present | HANDLED (pre-existing keys) |
| modes `win_screen endgame_screen quit_screen skip_quit_prompt pause_menu_quit_token` | read by data | 0 / 0 (`pause_menu_quit_token` 1/1) | HANDLED (data-only; disc scan finds them in `modes`/`endgame`/`lose`/`slot_states`) |
| preload_subdirs: 3 new `list_overshell_*_ingame.milo` | file paths | — | HANDLED (generic preload; all three are present in the disc ark as `ui/resource/list/gen/*.milo_xbox`) |
| presence_mgr `in_audition (gamemode audition)` | PresenceMgr | — | HANDLED: generic `gamemode` arm, `band3/game/PresenceMgr.cpp:189` |
| acc_grp_band `trophy_id` | PS3 | — | n/a |

### ui/

| File / item | R / S | Verdict |
|---|---|---|
| init.dta `#include audition/audition.dta` (inside `#ifdef HX_WIN`) | — | HANDLED: `HX_WIN` is set on 360 (`os/System.cpp:591`) |
| audition.dta `AuditionSessionPanel` handlers (`is_slot_empty get_slot_has_part get_user get_inst_icon get_slot_diff_sym get_slot_track_sym is_slot_autoplay add_autoplayer remove_autoplayer next_slot_track next_slot_diff`) | 1 / 1 each | HANDLED at the panel (`AuditionSessionPanel.cpp:117-144`), but the builder bodies are **MISSING** (item 3) |
| audition.dta `audition_mgr …` (16 calls) | 1 / 0 | **MISSING** (item 1) |
| audition.dta `kANS_Transferring` | not defined anywhere | retail DATA bug (`kNS_Transferring` intended). The progress bar never enables, on retail too. |
| accomplishments_panel `is_user_on_correct_instrument $user` | 1 / 2 | HANDLED (item 6) |
| patchcreator `{$this edit_layer} get sticker_category`, `category.lst selected_sym`, `last_selected_category` | 1/2, 1/2, data | HANDLED |
| character_creator `set_eyebrows_selected`, `get_eyebrows` | 1 / 2 | HANDLED: `CharacterCreatorPanel.cpp:874`, 921 |
| endgame/lose `gamemode in_mode audition`, `quit_early`, `audition_quit` | data | HANDLED (data-only) |
| band_ui `go_to_audition`, `error_club_member_signed_out` | data | HANDLED (the event is data; its reason-5 sender is item 2) |
| cheats_funcs `audition_cycle_free_camera` → `has_free_cam get_free_cam set_frozen enable_depth_of_field`, `$bandwardrobe get_character chars_dir`, `get instrument_type` | 1 / 2–4 | HANDLED (`world/CameraManager.cpp`, `world/FreeCamera.cpp`, `bandobj/BandWardrobe.cpp`, `bandobj/BandCharacter.cpp`) |
| cheats_funcs `customize_panel save_prefab $name` (returns a string) | 1 / 3 | HANDLED: `DataNode SavePrefab(const char*)`, `CustomizePanel.cpp:1676` |
| cheats_funcs `cheat_skip_membership_check`, `cheat_skip_audition_transfer` | data | HANDLED (data-only; `audition_transfer_panel` is data) |
| game.dta `audition_jump_back_ms/forward_ms/end_buffer_ms`, `audition_keyboard_synth_volumes`, `audition_cam_toggle` | 1 / 1 | HANDLED: `Game.cpp:996-1074` (read only in audition mode) |
| game.dta mydta panel `saveload_mgr activate`, `is_initial_load_done`, `ready_for_game` | 1/2, data | HANDLED (`SaveLoadManager.cpp:2372-2374`) |
| meta_loading `meta_loading_audition_screen` | data | HANDLED (data-only) |
| vignettes `set_loader_period`, `exit_complete` | 1 / 1 | HANDLED (`utl/Loader.cpp:523`, `ui/UIPanel.cpp:188`) |
| main_hub `get_dlcmotd` | 1 / 2 | HANDLED (`MainHubPanel.cpp:676`) |
| overshell_dir `overshell_audition` option (HX_XBOX) | data | HANDLED |
| overshell `audition_mgr has_autoplayer` | 1 / 0 | **MISSING** (item 1). Short-circuited outside audition mode, so it is harmless there. |
| slot_states `platform_mgr has_hard_drive`, `get_rbn_member_pad_num` | 1 / 2 | HANDLED (`os/PlatformMgr.cpp:236-237`). Native: whatever its PlatformMgr leaves in `mHasHardDrive`/`mRBNMemberPadNum` decides which audition error shows. |
| slot_states `server is_connected`, `session_mgr is_local` | 1 / 3–6 | HANDLED |
| slot_states `audition_mgr can_enter_audition` | 1 / 0 | **MISSING** (item 1) |
| song_select `$item is_ugc` | 1 / 2 | HANDLED: `OwnedSongSortNode` → `HANDLE_MEMBER_PTR(mSongRecord)` → metadata `is_ugc` (`BandSongMetadata.cpp:741`) |
| song_select_extras `ugc_not_allowed_screen`, `setlist_desc_vk_desc` | 1 / 1 | HANDLED (`MusicLibrary.cpp`) |
| store `store_preview_mgr clear_current_preview / set_current_preview_file` | 1 / 2 | HANDLED (`meta/StorePreviewMgr.cpp:116-117`) |
| store_info `$offer has_data / get_data preview_audio`, `store_panel get_request_prefix` | 1 / 2 | HANDLED (`meta/StoreOffer.cpp`, `BandStorePanel.cpp`) |
| track_paneldir `notify` removed | data | n/a |
| rg_trainer `unpause_track_no_rollback` | 0 / 0 | HANDLED (data-only; defined in disc `gem_trainer`/`gem_preview`) |
| locale_updates_keep (5 langs, about 95 tokens: `audition_*`, `prc_ugc_section_*`, `the1920s`…`the1950s`, `token_redemption_error`, `bad_review_showing`, …) | — | HANDLED as data. `audition_time_display_string_format` is consumed by `bandobj/TrackPanelDir.cpp` and `token_redemption_error` by `TokenRedemptionPanel.cpp`. `prc_ugc_section_*` and `the19x0s` have no whole literal in retail (built by format). Not traced. |

### system / world / char

| File / item | R / S | Verdict |
|---|---|---|
| band_objects BandDirector `cam_cat_priorities`, `generic_cam_cats` | 1 / 1–2 | HANDLED: `BandDirector.cpp:1842`, 1919 |
| band_objects track widget `{exists milo}` | data | n/a |
| world_objects `change_subdir` now gated on `":"` | 1 / 1 | HANDLED (data-only change; handler unchanged) |
| char_objects `outfit_variation` editor block (`resource_file`, `texture_folder_name`, `setup_outfit`, `perforce_save`, `dir_exists`, `milo …`) | — | n/a (Milo editor tooling; `dir_exists` is absent from retail too) |

### .milo_xbox (11 files)

No new classes: every class in the 11 TU5 milos already exists in our tree (union of 44
classes). `time_info` gains a `UIColor`, which is registered at `ui/UI.cpp:980`, and two
`BandLabel`s. `tracksystem` gains 2 `UIGuide`, 1 `Trans`, 1 `Mat` and 1 `Mesh`.
`overshell`, `overshell_player`, `crowd_meter`, `crowd_meter_icon` and `trackpanel` have
identical class counts. Per-object revisions (the first word after each `0xADDEADDE`
separator) are reliable only where segments = entries + 1: both audition milos,
`startup_madcatz_keep`, `time_info` and `tracksystem_meshes`. All of those carry the usual
revisions (Mat 68, Mesh 38, Tex 11, Group 14, BandLabel 17, PanelDir 28), so they are
not newer than the disc's. Dirs with inline subdirs (`trackpanel`, `tracksystem`,
`overshell*`, `crowd_meter`) misalign, and their per-class revision readout is not
evidence either way.

## What I did not do

- Did not load the TU5 milos through the native loader. `native/build/rb3-milo` needs the
  full config tree next to the ark (it failed at `config/objects.dta` against a
  patch-only data dir), and overlaying the patch on the disc data needs item 5 solved
  first.
- Did not trace MidiParser's handling of the new typedef props (`enabled`, `post_procs`,
  `allow_keys`). I assumed generic Hmx::Object type-prop get/set.
- Did not trace how `prc_ugc_section_*` and `the1920s`… are formatted at runtime.
- No source fixes, no build, no measurement.
