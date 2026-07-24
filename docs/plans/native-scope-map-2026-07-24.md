# RB3-Xenon — NATIVE-SCOPE decomp map

**Date:** 2026-07-24 · report.json @ main 4df07844 (24,781 strict fns) ·
`report.cache` cleared before read. Re-run: `python3 scripts/native_scope_map.py`
(committed on branch `nscope`, worktree `~/tmp/wt-nscope`).

**Scope directive.** Decomp scope = code on the **NATIVE PORT** path
(`native/` x86_64 engine; goal `docs/plans/engine-reuse-and-asset-rendering.md`,
memory `project_native_port`). Vendor XDK = hard skip; Quazal = low value. The
port's plan borrows the Milo **engine** from DC3 (rndobj/render/materials/mesh)
and decompiles only the **RB3 game layer** — so the engine-render TUs are
*redundant* to match here.

---

## What the native build actually links today

`native/CMakeLists.txt` → target `rb3-dta` compiles **only** these engine dirs:
`src/system/{obj,utl,os,math}/*.cpp` + the single TU `src/system/rndobj/Anim.cpp`
(needed for `Song : RndAnimatable` vtable thunks) + the flex lexer `DataFlex.c`.
Platform TUs are dropped (`_Xbox/_Win/_Wii/_X360`, `Dx9/Gpu/D3D`) and replaced by
**native shims** in `native/src/platform/` — `*_Native.cpp` = real host impls
(File, AsyncFile, Memory, PlatformMgr, System, ThreadCall); `*_Stub.cpp` = holes
(Achievements, Cache, Joypad, Keyboard, MapFile, Memcard, Net, NetworkSocket,
NetXbox, VirtualKeyboard).

`native/src/dta_link_stubs.s` = **77 weak stubs** (61 functions + 16 data
symbols) — every stub is a symbol referenced by a compiled-but-unused engine
vtable that isn't on the DTA path. **The stub list IS the boundary evidence:**
almost all are render/anim/audio/platform, i.e. DC3- or shim-territory. Only a
handful are game-logic holes (see backlog §Stub holes).

---

## Class table (per native-relevance bucket)

| class | units | fns | m_fns | strict fn% | bytes | m_bytes | strict byte% |
|---|--:|--:|--:|--:|--:|--:|--:|
| **NATIVE-CORE** (linked today: obj/utl/os/math + Anim) | 120 | 3,222 | 1,996 | **61.95%** | 406,808 | 173,160 | 42.57% |
| **NATIVE-SOON** (game logic: band3/meta_ham/beatmatch/midi/track/hamobj) | 292 | 16,212 | 10,273 | **63.37%** | 1,892,272 | 883,084 | 46.67% |
| NATIVE-VIA-DC3 (render/asset engine — DC3 supplies, redundant) | 342 | 17,561 | 11,084 | 63.12% | 2,226,760 | 990,276 | 44.47% |
| 360-ONLY (rnddx9/Dx*/xdk/Quazal/synth_xbox/codecs/*_Xbox) | 116 | 2,174 | 1,428 | 65.69% | 358,800 | 168,224 | 46.89% |
| UNKNOWN-anon (dtk auto-split blobs, no source_path) | 3,301 | 30,013 | 0 | 0.00% | 5,803,884 | 0 | — |

Class membership is directory-prefix based (see `scripts/native_scope_map.py`).
`hamobj`, `meta_ham`, `beatmatch`, `midi`, `track` are placed **NATIVE-SOON** —
they are RB3/Harmonix game logic (e.g. `HamGameData`, `MidiParser`,
`GemManager`), corroborated by their presence in the native stub list.

## Headline

- **Native-scope coverage (CORE + SOON, identified TUs): 63.1% fns / 45.9% bytes**
  (12,269 / 19,434 fns).
- **Whole-binary: 35.8% fns / 20.7% bytes.** The whole-binary number is dragged
  down by (a) 30,013 unidentified anon fns and (b) 360-ONLY. **The port is
  ~1.8× further along on its own scope than the headline number suggests.**
- Of the wired remainder, **NATIVE-VIA-DC3 (17.5k fns, ~2.2 MB) is redundant** —
  the port injects DC3's rndobj/render/materials/mesh, so matching those here
  buys the native runtime nothing (it still counts toward whole-binary %, but
  not toward "make native run further").
- **UNKNOWN-anon is the real unknown:** 3,301 blobs / 5.8 MB / 0% matched, no
  `source_path`. Some fraction is un-identified NATIVE-SOON game code; identifying
  it (fingerprint/BinDiff transfer) *is* native-scope work even though it can't be
  bucketed yet.

---

## In-scope backlog — top 30 by remaining bytes (NATIVE-CORE + NATIVE-SOON)

| # | rem_by | rem_fn | fn% | class | milestone | TU |
|--:|--:|--:|--:|---|---|---|
| 1 | 59,436 | 366 | 51.5 | SOON | M4 game UI/flow | band3/net_band/RockCentral.cpp |
| 2 | 34,588 | 375 | 7.4 | SOON | M4 profile/save | band3/meta_band/SaveLoadManager.cpp |
| 3 | 30,688 | 97 | 64.9 | SOON | M4 game UI/flow | band3/meta_band/OvershellSlot.cpp |
| 4 | 30,320 | 295 | 16.4 | SOON | M4 game UI/flow | band3/meta_band/NextSongPanel.cpp |
| 5 | 29,320 | 162 | 48.7 | SOON | M3 gameplay/scoring | band3/game/VocalPlayer.cpp |
| 6 | 28,188 | 61 | 71.5 | SOON | M3 gameplay/scoring | band3/bandtrack/VocalTrack.cpp |
| 7 | 25,612 | 114 | 50.9 | CORE | M0 dta/parse | system/obj/DirLoader.cpp |
| 8 | 23,408 | 66 | 82.1 | SOON | **M1 song load** | band3/meta_band/MusicLibrary.cpp |
| 9 | 22,064 | 64 | 82.4 | SOON | M4 profile/save | band3/meta_band/AccomplishmentPanel.cpp |
| 10 | 21,820 | 110 | 43.6 | CORE | M0 dta/parse | system/obj/DataFunc.cpp |
| 11 | 20,748 | 163 | 26.6 | SOON | M4 profile/save | band3/meta_band/UIStats.cpp |
| 12 | 19,436 | 85 | 58.7 | SOON | M3 gameplay/scoring | band3/game/Game.cpp |
| 13 | 18,880 | 72 | 65.4 | SOON | **M1 song load** | band3/meta_band/MetaPerformer.cpp |
| 14 | 17,424 | 113 | 35.8 | CORE | M0 (anim thunks) | system/rndobj/Anim.cpp |
| 15 | 17,232 | 124 | 40.7 | SOON | M3 gameplay/scoring | system/beatmatch/TrackWatcherImpl.cpp |
| 16 | 16,376 | 78 | 63.9 | SOON | **M1 song load** | band3/meta_band/BandSongMetadata.cpp |
| 17 | 15,812 | 60 | 70.7 | CORE | M0 dta/parse | system/obj/Dir.cpp |
| 18 | 15,812 | 51 | 62.8 | SOON | M3 gameplay/scoring | band3/bandtrack/GemManager.cpp |
| 19 | 15,696 | 42 | 64.7 | SOON | M4 profile/save | band3/meta_band/ProfileMgr.cpp |
| 20 | 15,396 | 113 | 37.6 | SOON | M3 game data (Ham) | system/hamobj/HamCamTransform.cpp |
| 21 | 15,212 | 39 | 85.2 | SOON | M3 gameplay/scoring | band3/game/GemPlayer.cpp |
| 22 | 15,040 | 77 | 65.6 | SOON | M3 gameplay/scoring | band3/game/Player.cpp |
| 23 | 14,360 | 61 | 65.5 | SOON | **M1 song load** | band3/meta_band/BandSongMgr.cpp |
| 24 | 14,196 | 116 | 19.4 | SOON | M3 game data (Ham) | system/hamobj/MoveMgr.cpp |
| 25 | 13,640 | 88 | 61.7 | SOON | **M2 chart parse** | system/midi/MidiParser.cpp |
| 26 | 12,980 | 46 | 66.2 | SOON | M4 game UI/flow | band3/meta_band/CharacterCreatorPanel.cpp |
| 27 | 11,788 | 49 | 84.5 | SOON | M4 profile/save | band3/meta_band/AccomplishmentManager.cpp |
| 28 | 11,624 | 27 | 43.8 | CORE | M0 dta/parse | system/math/Geo.cpp |
| 29 | 10,912 | 56 | 56.9 | SOON | M4 game UI/flow | band3/game/RGTrainerPanel.cpp |
| 30 | 10,848 | 33 | 79.5 | SOON | M4 profile/save | band3/meta_band/Campaign.cpp |

### Critical-path priority (what makes native run *further*, not just bigger)

The pure-size ranking above buries the runtime-order signal. Re-prioritized by the
native milestone chain (**M0 booted → M1 → M2 → M3 → M4**):

1. **M0 finish (NATIVE-CORE holes)** — `obj/DirLoader.cpp` (50.9%),
   `obj/DataFunc.cpp` (43.6%), `obj/Dir.cpp` (70.7%), `math/Geo.cpp` (43.8%),
   `rndobj/Anim.cpp` (35.8%). These are already-linked and below-par; closing
   them hardens the runtime the port stands on. DirLoader/Dir/DataFunc are on the
   object-load path directly.
2. **M1 song load / metadata** — `meta_band/BandSongMgr.cpp` (65.5%),
   `BandSongMetadata.cpp` (63.9%), `MetaPerformer.cpp` (65.4%),
   `MusicLibrary.cpp` (82.1%). Plus the stub hole `SongInfoCopy::GetTracks`.
   This is the *next* runtime milestone after DTA: turn parsed song rows into
   loadable song objects.
3. **M2 chart parse** — `midi/MidiParser.cpp` (61.7%) + stub holes
   `MidiParser::Poll`, `MidiReceiver::ctor`, `TheMidiParserMgr`. Loads a song's
   note chart — the gate to any gameplay logic.
4. **M3 gameplay / scoring** — `bandtrack/GemManager.cpp` (62.8%),
   `game/GemPlayer.cpp` (85.2%), `game/Player.cpp` (65.6%),
   `beatmatch/TrackWatcherImpl.cpp` (40.7%), `game/Game.cpp` (58.7%),
   `game/VocalPlayer.cpp` (48.7%) + Ham stubs (`HamGameData::Player/Venue`,
   `TheGameData`).
5. **M4 profile/save/practice + game UI** — the meta_band bulk
   (`SaveLoadManager` 7.4%, `UIStats` 26.6%, `ProfileMgr` 64.7%, trainers).
   Big byte count but latest in the runtime order.

### Stub holes → class (the native frontier)

`dta_link_stubs.s` 61 function stubs classify as:

- **NATIVE-SOON holes (real game-logic gaps):** `MidiParser::Poll`,
  `MidiReceiver::ctor`, `HamGameData::Venue`/`Player`, `SongInfoCopy::GetTracks`,
  data `TheGameData`/`TheMidiParserMgr`. **These are the highest-value stubs** —
  filling them lets native advance past DTA into song/chart/game-data.
- **NATIVE-VIA-DC3 (satisfied when DC3 rndobj is injected, per the plan):** the
  `RndAnimatable::*` cluster (Load/Save/Copy/Handle/SyncProperty/SetFrame/ctor +
  vtable thunks — the largest group), `Rnd::DrawRectScreen/DrawStringScreen`,
  `RndGraph::*`, `RndOverlay::Find`, `RndGroup::AddObject`, `CameraManager::Poll`,
  `LightPresetManager::*`, `Synth::*`, data `TheRnd/TheSynth`. Do **not** decomp
  these for the port.
- **360-ONLY / shim (native platform layer already covers):** Win32/XInput/XNet/
  WSA (`CloseHandle`, `WaitForSingleObject`, `GetXinputSinceLastFrame`,
  `SetupHX{Guitar,Drums,Keytar,RealGuitar}`, `JoypadSetActuatorsImp`),
  `CacheMgrXbox`, `NetLoaderXbox`, `NetCacheMgrXbox`, `XboxMapFile`,
  data `TheContentMgr/TheWebSvcMgr`.

### Layout-truth double-value seeds (from `docs/plans/repin-batch9.md`)

Class-RE items that fix decomp match **and** are load-bearing for native
struct correctness (native reads the same fields): **BandCharacter `unk6d8`
relayout** (landed +6), **Character vtable slot +4 / vbase −4**, **ProfileMgr +4**,
**Profile virtual-base** (StorePanel vbptr slot 0x64), **Stats layout**
(`mVocalPartPercentages`), **DancerSequence base RndAnimatable→UIPanel**. Prioritize
these where they overlap M1/M4 TUs (ProfileMgr, UIStats, Character*) — a layout fix
there is worth double.

---

## Method notes / caveats

- Only ~870 units carry a `source_path` (the wired TUs); 3,301 are anon dtk
  splits. Coverage % are honest **over identified TUs**; the anon pool is an
  explicit unknown, not folded into the headline.
- `matched_functions` across all classified path-bearing units == whole-binary
  matched (24,781); anon units contribute 0. So every strict match today lives
  in a wired TU.
- Judgment calls: `synth`/`dsp` placed VIA-DC3 (DC3 shares the Milo audio
  engine; audio isn't a listed milestone). `flow`/`meta`/`gesture` placed
  VIA-DC3 as engine. Vendor codecs (jpeg/ogg/speex/zlib) placed 360-ONLY
  (native uses host zlib). Revisit if a milestone pulls one in.
- **Re-runnable:** `python3 scripts/native_scope_map.py [report.json]` after
  each wave. Tool committed at `scripts/native_scope_map.py` (branch `nscope`).
