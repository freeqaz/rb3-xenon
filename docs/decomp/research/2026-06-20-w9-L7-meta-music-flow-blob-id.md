# W9 L7 — meta-music-flow-blob-id (owner-TU identification)

**Date:** 2026-06-20 · **Mode:** adversarial discover/planner (Opus L7), READ-ONLY in main
**Frontier item:** `meta-music-flow-blob-id` (kind=scout, est +15) — seeded by L6
(`2026-06-20-w9-L6-songselect-panel-family-port-scout.md` lines 113-116).
**Baseline:** main @ 8314 matched.
**Verdict:** **REAL_ACTIONABLE** — both unpinned blobs are real game-TU bodies with
rb3-Wii oracles. NOT static-init mirages.

## What the frontier got right vs wrong

- RIGHT: `meta_music` @0x8209B6C4 + jumptable @0x8209B6E0 → 0x8255B9xx is real,
  jump-table-driven dispatch code at fn_8255B638 (sz 0x2a8). Confirmed by disasm.
- WRONG (imprecise): "0x826135CC / 0x82615D48 (refs mini_leaderboard_rotation_off)".
  Those two addrs are just unpinned fns at the END of the Cam.cpp pin — they do NOT
  reference the mini_leaderboard strings. The ACTUAL consumer of
  `mini_leaderboard_rotation_off/on` (0x820C91D0/0x820C91B0) is **fn_8261B208**
  (a different cluster). Owner = SongSelectPanel.cpp, NOT a "meta_music flow blob".

## Owner TU #1 — MetaPanel.cpp (the meta_music blob)

`meta_music` @0x8209B6C4 and the adjacent handler strings
(`send_back_sound_msg_to_all` 0x8209B6A8, `sync_game_timer` 0x8209B698) are ALL
referenced by **fn_8255B638** = `MetaPanel::Handle` (the `BEGIN_HANDLERS(MetaPanel)`
expansion at MetaPanel.cpp:556). Oracle: `~/code/milohax/rb3/src/band3/meta_band/
MetaPanel.cpp` (564 lines).

- `MetaPanel` classname string @0x82099618 is referenced by fn_82556B48 — which is
  INSIDE the ContextChecker.cpp pin (0x82555398-0x82558EAC). That is the
  COMDAT-scattered `ClassName()`/static-init sliver, NOT the TU body. (Classic
  Waypoint-style COMDAT template-scatter; the L6 doc's "exactly-once classname ref"
  observation generalizes here.)
- The MetaPanel **body methods** cluster at ~**0x8255AF04 → 0x8255BA08** (~0xB00 bytes,
  ~16 named fns). String anchors in-cluster: `BandEventPreviewMsg` 0x8255AF04,
  `TriggerBackSoundMsg` 0x8255AFBC (MetaPanel message types), `meta_music`,
  `send_back_sound_msg_to_all`, `sync_game_timer`, `sfx/shell_fx.milo` 0x8255BAF0,
  `background_music_level.fade` 0x8255BBCC.
- **MetaPanel::Init()** (the heavy registration fn with ~90 #includes worth of panel
  classname refs) is NOT in this cluster — zero in-cluster fn has the high rdata-ref
  count Init would. It is COMDAT-scattered into the panel-registration block near
  0x82559E64 (where all panel classnames cluster). So the pinnable body cluster is
  the LIGHT methods: Handle, ToggleUnlockAll/IsPlaytest/LaunchedGoalMsgsOnly,
  SyncGameTimer, UpdateMusicMuteState, UpdateMetaMusic, OnSendBackSoundMsgToAll,
  the two OnMsg(...) handlers, dtor/Poll/Enter/Exit. **Init can be a stub** — it
  isn't in the pin span.
- BOUNDARY CAUTION: above the cluster is a `MoviePanel` static-init sliver
  (classname 0x8255A724); below at 0x8255BA10 is **MetaMusic.cpp** body
  (`SystemConfig("synth","metamusic")` — strings `music`/`metamusic`/`synth`/
  `sfx/streams/%s` at 0x8255BA34-BAB0). MetaMusic.cpp is NOT wired (its only pin is a
  0x60 sliver @0x82749660). The MetaPanel/MetaMusic split at ~0x8255BA08
  (except_data_8255BA10 marker) and the MoviePanel-sliver/MetaPanel start
  (~0x8255AECC-0x8255AF04) both need a per-fn boundary-derive at execution time.
- MetaPanel.h ALREADY exists in xenon (clean, ported — mMusic@0x5c, mTour@0x38,
  full member map). Deps for the light methods: MetaMusicManager.h, MetaMusic.h,
  SessionMgr.h, NetSession.h, rndobj/PostProc.h, Faders.h, Synth.h — all present.
  The 90-include footprint is only needed by Init(), which is out of span.
- Bounding pins: ContextChecker.cpp ends 0x82558EAC (below); Meta.cpp sliver
  0x825595A0-0x825595F8; next real pin StreamRecorder.cpp @0x825732D0. The whole
  0x825595F8→0x825732D0 gap is unpinned and holds a sequence of panel/meta TUs
  (TourChallengeResults/JoinInvite/SongSelect static-init slivers → MetaPanel →
  MetaMusic → SongMgr → MetaPerformer → SessionMgr, per the string survey).

## Owner TU #2 — SongSelectPanel.cpp (the mini_leaderboard cluster)

`mini_leaderboard_rotation_off/on` (0x820C91D0/0x820C91B0) → consumed by
**fn_8261B208**. The cluster 0x8261B208 → ~0x8261C700 holds the full SongSelectPanel
leaderboard handler set: `leaderboard.mld` 0x8261B230, `lb_success`/`lb_failure`,
`set_mini_leaderboard_showing`, `get_leaderboard`, `set_to_starting_lb_ix`,
`set_leaderboard_mode`, `select_lb_row`, `restart_leaderboard_timer`,
`cancel_leaderboard_timer`, `scroll_lb_up`/`scroll_lb_down`. These are exactly the
`BEGIN_HANDLERS(SongSelectPanel)` strings in
`~/code/milohax/rb3/src/band3/meta_band/SongSelectPanel.cpp:181-201` (207 lines).

- This CORRECTS the L4 doc (`2026-06-20-w9-L4-voiceoverpanel-...`) TU map, which
  put SongSelectPanel at 0x8261C1F0-0x8261C918 (only a tail fragment) and lumped
  0x82616938-0x8261C1F0 as "SelectDifficultyPanel". The strings just below my
  cluster (0x82619324-0x8261A5E4) are actually SetlistMergePanel
  (`setlist_merge_screen`) + SigninScreen (`signing_in_user`, `on_signed_in/out`).
- Cluster END: `no_recommendations`/`recommendations_ready`/`cur_offer` at
  0x8261C918+ = **StoreInfoPanel.cpp** (next TU). SongSelectPanel ends ~0x8261C700.
- SongSelectPanel.cpp is CLEAN to port: all 22 dep headers exist in xenon
  (AppMiniLeaderboardDisplay.h, meta_band/Leaderboard.h, MusicLibrary.h,
  PlayerLeaderboards.h, ProfileMgr.h, SongSortNode.h, ui/UIList.h, ui/UIPanel.h,
  utl/Messages2.h, utl/Symbols3.h). The rb3-Wii .cpp already has HX_NATIVE blocks
  (someone touched it for the native port) — well-understood. SongSelectPanel.h
  exists in xenon. Wii-dep gotcha (per L4): includes `os/ContentMgr_Wii.h` →
  swap to 360 `os/ContentMgr.h` during port (the .cpp already uses `os/ContentMgr.h`).
- Bounding pins: Cam.cpp ends 0x826135CC (far below); NameGenerator.cpp
  @0x82626F80 (far above). The cluster sits mid-gap; tail anchored by StoreInfoPanel
  string transition at 0x8261C918.

## Both pins carry attribution_risk=true

Brand-new wirings of unpinned game TUs: a pin reads false-0 without
target_symbol_map entries; the port must register matches. Boundaries are
string-anchor-derived (COMDAT-interleaved), so each item must run a per-fn
boundary-derive (zip auto_03 `fn_` starts with the report) to confirm
[min_fn, max_fn+size) BEFORE pinning, and the honesty gate (matched>0, no
≥8-contiguous foreign fn_@0% run) must hold per-TU after the port.

## Ground-truth method (reusable)

- `/tmp/coffparse.py` — LE PPC64 COFF parser for auto_*.obj (base from filename;
  symbol value = section-relative, VA = base+value). `/tmp/coffcluster.py`,
  `/tmp/coffdis.py` (capstone PPC BE), `/tmp/coffva.py`, `/tmp/xref.py` (lis/addi
  hi/lo pair → target address attribution to owning fn).
- jumptable @0x8209B6E0 decoded from auto_00 rdata (BE dwords): entries point to
  0x8255B920/B940/B968/B990/B9B8/B9E0 (the small Handle case bodies in-cluster).
  Disasm of fn_8255B638 shows the Symbol-keyed dispatch with lazy `lis -0x7d23 /
  lwz -0x2ecc / clrlwi. / ori 1 / bl 0x8279b788(Symbol::Symbol)` static-Symbol
  guards = canonical HANDLE_EXPR/HANDLE_ACTION expansion.

## Verdict

REAL_ACTIONABLE. Two independent, self-contained gameport items (MetaPanel.cpp,
SongSelectPanel.cpp), each landable vs main@8314 via scaffold+wire+pin+map+port+
objdiff in one worktree. SongSelectPanel is the cleaner/lighter first target
(22 deps all present, light TU). MetaPanel is heavier only because of Init()
(out of span → stub it). Both are genuine RB3 game code (band3/meta_band), the
correct decomp-priority layer.
