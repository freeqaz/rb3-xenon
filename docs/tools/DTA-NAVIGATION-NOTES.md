# DTA navigation notes — driving RB3 headlessly via RB3E `/execute`

Hardware-validated 2026-07-15 on the devkit (RB3DX TU5 + RB3Enhanced,
`feature/same-instrument` build). Companion script:
`tools/oss-xbox-build/si_2p_setup.py` (reaches 2-player same-instrument
gameplay, P1 guitar expert / P2 guitar easy, no controllers connected).

All expressions below were sent as
`GET http://192.168.8.180:21070/execute?script=<urlencoded DTA>`.
**Always brace-balance-check client-side first** — an unbalanced script wedges
all RB3E networking (recovery: magicboot the title; an *interrupted* magicboot
falls back to Aurora — just re-issue it from there, where it returns `200- OK`).

## Verified WORKING vocabulary

### State reads (safe any time)
| Expression | Result |
|---|---|
| `{{ui current_screen} name}` | current screen name, e.g. `main_hub_screen` |
| `{ui in_transition}` / `{{ui transition_screen} name}` | transition state + target |
| `{{user_mgr get_user_from_pad_num N}}` | LocalBandUser for pad N (pre-allocated for pads 0-3 even with **no controller connected**) |
| `{<user> is_participating} / get_track_sym / get_difficulty / get_controller_type / connected_controller_type / get autoplay` | per-user readbacks |
| `{game is_playing} / {game get_paused} / {game get_song_ms}` | in-song state (song_ms advancing = definitely playing) |
| `{beatmatch num_active_players}` | active player count in-song |
| `{preload_panel is_loaded}` | song preload finished |
| `{game is_loaded} {world_panel is_loaded} {coop_track_panel is_loaded} {sync_audio_net_panel is_loaded}` | per-panel game_screen load gates |
| `{overshell should_pause} / {overshell all_slots_ready_to_play}` | overshell pause gates |
| `{{overshell get_panel_from_slot_num N} get_current_view}` | slot view (`hidden` = good; `reconnect_controller` = pausing the game) |
| `{meta_performer get_venue} / {meta_performer song}` | selected venue/song |
| `{session_mgr is_leader_local} / {session_mgr is_local}` | session state |
| `{gamemode get game_screen}` | mode's gameplay screen symbol |

### Actions (validated end-to-end)
- `{ui goto_screen main_hub_screen}` — works from `splash_screen` (retry until
  the screen flips; it's ignored while the splash is still settling).
- `{gamemode set_mode qp_coop}`
- `{setup_game <song> '' ((guitar expert 1) (guitar easy 1))}` — **the** key
  entry point (HMX debug func in `ui/global.dta`). Per player-list entry
  `(track diff [autoplay])`, pad = list index: joins the user into the session,
  sets track/controller/difficulty/autoplay **and a prefab character**
  (character required — see FAILED section). Use difficulty *symbols*
  (`easy medium hard expert`) and track symbols (`guitar` etc.); the
  `kDifficulty*`/`kTrack*` names are ARK-build preprocessor defines that do
  **not** resolve at `/execute` time.
- `{ui goto_screen preloading_screen}` then poll `{preload_panel is_loaded}`.
  During the load burst `/execute` can take >20 s to answer — treat probe
  timeouts as "not yet", don't panic (and don't hammer it; ~5 s interval).
- `{meta_performer select_random_venue}` — **mandatory, and only AFTER the
  preload completes**: without a venue, BandDirector never becomes
  ReadyForMidiParsers and the game_screen transition hangs on a black screen
  forever; selecting the venue *before* entering `preloading_screen`
  crash-loops the main thread instead (0xC0000005 null read @0x82577f00).
  Readback: `{meta_performer get_venue}`.
- `{net_sync disable}` + `{ui goto_screen {gamemode get game_screen}}` — the
  body of the preload panel's `on_preload_ok` handler, which does NOT
  auto-fire when preloading_screen was entered out-of-flow.
- Overshell "reconnect controller" pause fix (no physical pads):
  - `{set {var fake_controllers} 1}` — HMX test hook; disables
    `OvershellPanel::CheckForControllerDisconnects` entirely and makes
    `LocalUser::IsJoypadConnected()` return true. Note the `{var ...}` form:
    a bare `{set $fake_controllers 1}` sets a *local* eval var and does nothing.
  - per user: `{<user> set_controller_type <its connected_controller_type>}` —
    the slot auto-reverts out of `kState_ReconnectController` only when
    connected type == set type (compared per pad; on this console pad 0
    reported 0 and pad 1 reported 1).
  - `{overshell update_all}` — runs `ResolveSlotStates()`, which performs the
    revert; the game then unpauses by itself.
- `/jump?shortname=<x>` (RB3E endpoint) — safe anywhere (internally guarded on
  `song_select_panel->is_up`), but it only *highlights* the song in the music
  library; confirm with `{music_library select_highlighted_node <user>}`
  (that path leads into `part_difficulty_screen`, i.e. the interactive flow —
  the setup_game path above skips it entirely and is preferred).
- `{ui key <code> <shift> <ctrl> <alt>}` — RB3E keyboard injection path
  (Enter=10, Esc=0x12e, arrows 0x140-0x143). Dispatches fine but did NOT
  advance the splash screen; direct `goto_screen` is more reliable.

## FAILED / dangerous — do not retry
- **`{setup_game ...}` after a completed song** (from `coop_endgame_screen` →
  `main_hub_screen`) — crash-loops the main thread (PC lands in a non-code
  region, 0xC0000005 exec at 0x7004ebe0) and wedges RB3E HTTP. The re-join
  path (`remove_local_user`/`add_local_user` on post-game users) is not safe.
  Run the recipe from a FRESH title boot only; to play another song, relaunch
  the title first (or investigate `{meta_performer restart}` /
  `host_restart_last_song`, untested).
- **`{ui goto_screen <anywhere>}` while the preload panel is active or a
  screen transition is loading** — crashed the game main thread twice
  (0xC0000005 first-chance loops at 0x82814270 read+0x64 and 0x825bf710
  read+0x70c). Symptom: HTTP connects but never responds (main thread no
  longer services the RB3E script queue), title keeps rendering. Recovery:
  magicboot relaunch only.
- **Joining users with bare `{session_mgr add_local_user ...}` + manual
  set_track/difficulty (without characters)** — reaches seldiff/gameplay load
  and then crash-loops: users have NULL char, gameplay derefs char data.
  Always go through `setup_game` (it assigns prefab chars via
  `set_prefab_char '' <slot>` → `SetLoadedPrefabChar`).
- `{ui goto_screen preloading_screen}` **without a venue set** — preload
  completes but the game_screen transition black-screens forever (see above).
- `{preload_panel on_preload_ok}` — property *read* (returns the handler
  array object); does not execute the handler. Run its body manually instead.
- `{set $fake_controllers 1}` — silently sets a local; use `{set {var ...} 1}`.
- `{user_mgr debug_set_controller_type_override <pad> <ct>}` — not present in
  this build (empty response, no effect).
- `{{overshell get_panel_from_slot_num N} show_state 5}` / `leave_options` —
  cannot force a slot out of `kState_ReconnectController` (state 50,
  `prevents_override TRUE`, re-asserted every poll); only the
  connected-type match + `update_all` revert works.
- `{meta_performer venue}` — wrong handler name (empty); use `get_venue`.
- `{name <obj>}`, `{get_screen}`, `{the_band_user_mgr}`, `{overshell}` as a
  bare value, `{taskmgr beat}` for song position (stays 0 in-song; use
  `{game get_song_ms}`) — all useless/no-ops.

## Console-side facts learned
- RB3E `/jump` = `MusicLibrarySelectSong()` → guarded `CheckForPanelAndJump`
  (`RB3Enhanced/source/MusicLibrary.c`) — no crash risk outside the library.
- UDP event broadcasts (`rb3e_alive_listen.py`) do **not** reach this dev
  box from the console's subnet; use the XBDM notify stream
  (`xbdm_notify.py`) + `xbox.sh screen` + DTA reads for observability.
- SI-feature load evidence on the notify stream (2 players, both guitar):
  `same-instrument: watcher constructed for track 2` (twice) +
  `same-instrument: cloned gem DB for track 2 (claim 2)`.
- OPEN BUG (2026-07-15, hardware-confirmed): with P1 expert / P2 easy reading
  back correctly at every level (`{<user> get_difficulty}` 3/0 AND
  `{{beatmatch active_player N} difficulty}` 3/0), the TV shows **the same
  chart for both players**, and `{{beatmatch active_player N} get_gem_count}`
  returns **571 for both** (an easy chart would be far smaller). The
  same-instrument gem-DB clone (`cloned gem DB for track 2 (claim 2)`) copies
  the first claimer's gem list without re-filtering at the second claimer's
  difficulty. Fix belongs in the RB3E same-instrument track-clone code.
