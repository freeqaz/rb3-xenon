# RB3Enhanced remote-debug capabilities — what we have, what a patch adds

**Date:** 2026-07-14. Source: `/home/free/code/milohax/RB3Enhanced` (the fork we
build). Context: we now control a loadable XDK-free from-source DLL (wave8), so
the HTTP/DTA server is fully ours to extend.

## TL;DR

**DTA-eval-with-return already exists by composition — no patch strictly
required.** `GET /execute?script=…` runs arbitrary DTA on the game thread, and
the built-in DTA function `rb3e_send_event_string` broadcasts a string back over
the UDP event channel (which `rb3e_alive_listen.py` already decodes). A DLL
patch makes this cleaner (synchronous `/eval`) and, more valuably, lets us add
**purpose-built debug getters** (band/player/track-slot dumps) that directly
answer the SI test questions.

## What we have TODAY (already in the DLL)

### 1. HTTP server — TCP port `0x524E` = **21070** (`net_http_server.c`)
Gated by `[HTTP] EnableHTTPServer=true`. Endpoints:
| Endpoint | Does | Gate |
|---|---|---|
| `GET /execute?script=<DTA>` | runs **arbitrary DTA** on the game thread (fire-and-forget, returns "OK") | `AllowScripts=true` |
| `GET /jump?shortname=X` | jump to song X in Music Library | — |
| `GET /song_<id>` | song metadata (shortname/title/artist/album/origin) | — |
| `GET /list_songs` | all songs, streamed | — |
| `GET /` , `/jsonrpc` | serve rb3e_index.html / discordrp.json | — |
Marshalling: the accept-thread stores the script in `PendingScript`; the game's
`RB3E_RunLoop` (main thread) calls `ExecuteDTA(PORT_ROCKCENTRALGATEWAY, script)`
and clears it. **DTA runs on the game thread — safe for engine access.**

### 2. Custom DTA function framework (`DTAFunctions.c`)
`DataRegisterFunc` IS wired on xbox360 (`rb3enhanced.c:407`,
`PORT_DATAREGISTERFUNC`). Already-registered functions callable from any script:
`print_debug`, `rb3e_api_version`, `rb3e_build_tag`, `rb3e_commit`,
`rb3e_change_music_speed`/`rb3e_get_music_speed`, `…track_speed`,
`rb3e_set_venue`, `rb3e_is_emulator`, `rb3e_relaunch_game`,
`rb3e_get_song_count`, `rb3e_get_song_name`/`artist`/`album`/`origin`/`genre`,
`rb3e_delete_songcache`, `rb3e_local_ip`, and **`rb3e_send_event_string`**.

Pattern for a new func — evaluate args, touch engine, return a typed node:
```c
DataNode *DTAGetFoo(DataNode *node, DataArray *args) {
    DataNode *a = DataNodeEvaluate(&args->mNodes->n[1]); // n[0]=fn symbol, n[1..]=args
    node->type = INT_VALUE; node->value.intVal = /* … */;
    return node;
}
```

### 3. The return channel — `rb3e_send_event_string` (DTASendModData)
`{rb3e_send_event_string "tag" "<string>"}` → broadcasts `RB3E_EVENT_DX_DATA`
(type 11) UDP to 255.255.255.255:21070 → decoded by our
`rb3e_alive_listen.py`. **This is the eval-return primitive.** Caveat: both args
must be **STRING** (int/float/object is a source TODO) — so numbers must be
stringified in DTA first.

### 4. Live telemetry PUSH — UDP 21070 (`net_events.c`)
Gated by `[Events] EnableEvents=true` (**ON on the drive today** — that's the
STAGEKIT stream we saw). Events: ALIVE, STATE, SONG_NAME/ARTIST/SHORTNAME,
SCORE, STAGEKIT_FOG/RUMBLE, BAND_INFO, VENUE_NAME, SCREEN_NAME, DX_DATA.

### 5. Raw memory / threads — XBDM TCP 730 (separate, via xbdm.xex plugin2)
`getmem`/`setmem`/`getcontext`/`modules`/`threads`/`stop`/`go`. Always available
independent of RB3E. Covers arbitrary peek/poke; RB3E adds *semantic* access.

## Zero-rebuild path for the SI test (fastest)
1. Set on-drive `rb3.ini`: `[HTTP] EnableHTTPServer=true`, `AllowScripts=true`
   (keep `AllowCORS` as desired). `EnableEvents` already true.
2. While the user plays 2 same-part guitars, query live state:
   `GET http://<xbox>:21070/execute?script={rb3e_send_event_string "dbg" <expr>}`
   and read the DX_DATA reply on the UDP listener. Stringify numbers in DTA.
3. Watch the XBDM notify stream for the H1 `0xC0000005` DSI at song load.

## Patch opportunities (from-source DLL, rides on the wave8 container fix)
Ranked by value for SI debugging + general remote debug:

1. **Debug getters for the band/player/track state (highest value).** New
   `DTAFunctions.c` entries, e.g. `rb3e_dump_band`, `rb3e_get_track_num <slot>`,
   `rb3e_get_player_count`. These read the exact state behind the SI H1 bug (the
   2nd same-part player's `mTrackNum` == -1 → `vector[-1]`). Trivial C given the
   existing ports (BandSongMgr, player list). Callable via `/execute`, returned
   via the DX_DATA channel.
2. **Synchronous `/eval` endpoint.** Register an internal `rb3e_http_return` DTA
   func that writes the evaluated node (typed → text) into a static buffer the
   HTTP accept-thread reads and returns in the HTTP body — removes the UDP
   round-trip and gives a true REPL. Moderate effort (extends the PendingScript
   marshalling with a result buffer + generation counter).
3. **`rb3e_send_event_string` typed args.** Resolve the source TODO so
   int/float/object return without DTA stringification. Small.
4. **`/peek` / `/poke` semantic endpoints.** Lower priority — XBDM already does
   raw memory; only worth it if we want it in the same HTTP surface.

## Sequencing
The from-source container fix (agent in flight) must land first. Any source
patch (getters/`/eval`) then needs: edit `DTAFunctions.c`/`net_http_server.c` →
rebuild the XDK-free DLL → `xextool -c c` → redeploy. So batch all desired hooks
into one patch pass. For the immediate SI test, use the **zero-rebuild ini path**
above.
