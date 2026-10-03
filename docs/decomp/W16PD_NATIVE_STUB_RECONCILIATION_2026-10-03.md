# W16-PD — native link-stub reconciliation against `src/` (2026-10-03)

Branch `w16-pd`, worktree `~/tmp/wt-w16-pd`, based on main `bca4bbcaf`.

**Brief.** Reconcile every stub the native build links in place of real code
against `src/`: delete each stub whose real body now compiles from `src/`, write
the in-scope ones a native target actually reaches, record the rest as
platform-only or unreached. Out of scope: `src/network`, `src/xdk`, the
milo-native-engine pin.

**Full per-symbol inventory (1,221 rows, one verdict each):**
[`W16PD_stub_inventory_2026-10-03.tsv`](W16PD_stub_inventory_2026-10-03.tsv).
This doc is the summary and the evidence behind it.

## 0. Headline

| | before | after |
|---|---:|---:|
| stub/shim TUs with authored definitions | 24 (+2 empty `.s`) | **21** |
| distinct (file, symbol) stub definitions | 1,481 | **1,221** (−260) |
| stubs *executed* on a native run (C++ probe + gdb) | — | 64 (file, symbol) rows / 44 distinct symbols, every one adjudicated (§3) |
| CPU targets that run to completion | **14 / 16** | **16 / 16** |
| X360 A/B of the one `src/` change | — | **Δ0 on every measure, 0 of 68,909 rows moved** |

★ **Biggest find was not a stub: `rb3-vocal2` and `rb3-harmony` have segfaulted
on their first frame since `f3ec9592d` (2026-08-03).** Nothing caught it: the
native gate only *links*, and `native_health.sh` only *runs* milo / ark / render
/ score2. See §2.1.

## 1. Instruments, with the controls that make them trustworthy

1. **Census — `tools/native_stub_census.py` (new).** `nm` over the objects on
   each of the 18 targets' link edges, parsed from `native/build/build.ninja`
   (not from CMake text). For every symbol a stub TU *authors* (all `.s` weak
   stubs; strong T/D/B/R from `.cpp` stubs, which drops the header-inline
   COMDATs — the unfiltered count is 49,790 rows and is meaningless) it records:
   overridden in that target by a real object, referenced (UNDEF) by another
   object, and which targets compile a `src/` object defining it.
   ⚠ **Measured blind spot:** a reference from *inside the stub's own object*
   never appears as UNDEF. `TheNetMessageFactory` read "unreferenced in all
   targets" and is used by an inline `StaticByteCode()` emitted in the same
   TU — the re-link failed and it was restored. So "unreferenced" is a
   candidate list; **every deletion here was re-linked across all 18 targets**.
2. **C++ reachability — the CC-5 stub probe (`-DRB3_STUB_PROBE=ON`).** Two
   defects fixed first, both of which made it blind:
   - `RB3_STUB_TUS` listed 7 TUs; every stub TU added after CC-5 (m12,
     milo_link_stubs, x7, x20, native_link_glue, xdk_shims, m8/m10 support)
     was **uninstrumented**. Widened to all of them.
   - `rb3-render` exits through `_exit()`, so the destructor dump never ran:
     **rb3-render was invisible to the probe**. Added a weak
     `rb3_stub_probe_dump()` hook called before `_exit` (no-op symbol in a
     normal build). Render now reports 74 distinct entries.
   Resolver committed as `tools/stub_probe_resolve.py` (CC-5 left it unnamed).
   Final-state run: all 17 runnable targets (rb3-frame links 1 object + the
   engine library, no stubs) rc=0.
3. **`.s` reachability — one-shot gdb breakpoints.** `-finstrument-functions`
   cannot see assembly stubs, so each target ran under gdb with a `tbreak` on
   every surviving `.s` stub function (34–69 per target).
   ⚠ **The first run was VACUOUS — 0 hits in 16 targets — because the command
   file path was relative to the wrong cwd and gdb never armed anything.**
   Rerun with a **positive control, `tbreak main`, asserted hit in every
   target**; all 17 programs `exited normally` under gdb. Result: exactly one
   `.s` stub ever executes — `OutputDebugStringA` (§3).
4. **Behaviour diff.** Every change to a target's link set was followed by
   running all 16 CPU targets and byte-diffing stdout/stderr against the
   previous state. Across the whole lane the only differences were ASLR pointer
   prints (`tempoMap=0x…`) in gem/hit/score, which link none of the changed
   code. rb3-render: `ALL GATES PASSED`.

## 2. What changed

### 2.1 `src/` — the only shared-source change (measured)

`src/band3/game/VocalPlayer.cpp`, `VocalPlayer::PressingToTalk`: an
`#ifdef HX_NATIVE` null-`BandUser` early-out. `f3ec9592d` made
`VocalPlayer::Poll` call `PressingToTalk()` unconditionally (retail shape); the
headless vocal drivers build the player with **no** `BandUser`, and used to be
shielded by a `!TheNetSession->IsLocal()` gate that the m10
`NetSession::IsLocal` stub existed to satisfy. That stub was therefore dead (the
census found it unreferenced) and the comment describing it was false.

`tools/ab_measure.py --patch` (leg A = main `b2d214cb5`, leg B = + this diff),
ruler `name_check`, 1 TU recompiled in leg B:

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   units at 100%: 533 -> 533 (mpn), 470 -> 470 (fuzzy)
```

Row-level over both archived reports: **68,909 rows, 0 down, 0 up**. Predicted
Δ0 before running (X360 never defines `HX_NATIVE`); the prediction held.

### 2.2 Stubs replaced by the real `src/` TU (native CMake only)

| stubs removed | real TU now linked | targets | evidence |
|---|---|---|---|
| `PlayerBehavior` ctor + 2 setters ×4 files | `band3/game/PlayerBehavior.cpp` | score2/3/4, vocal, vocal2, harmony, crowd | ⚠ the copies were **not faithful**: ctor defaulted `mCanDeployOverdrive=0, mStreakType=(), mMaxMultiplier=0`; the real one sets `true, "default", 2`. Outputs unchanged (drivers set all three explicitly). |
| `CrowdRating` ×10 (m6, m8, m10) + 3 (m10_leaf) + ctor shim (m8_support) | `band3/game/CrowdRating.cpp` | score2/3/4, vocal, vocal2, harmony (crowd already) | Hot path measured by the probe while still stubbed: `Poll` 25,905× score4, 25,203× vocal2, 12,401× harmony — so the stubs, not the real class, were driving crowd state. score4/vocal2/harmony construct a `CrowdRating`, so they now splice the **shipped** `(crowd …)` block from `crowd_config_dta.h` (as main_crowd does). Outputs byte-identical. |
| `m12_link_stubs.cpp` (whole file) | — | crowd → uses m8 | With CrowdRating gone, m12 ("m8 minus CrowdRating") was code-identical to m8. `RB3_REAL_CROWD` define retired. |
| `TrackTypeToSym` (m1 `.s`) | `system/beatmatch/TrackType.cpp` | song | links clean; output identical, 7/7 gates |
| `SongUpgradeMgr` ×9 + `SongUpgradeData` ×5 (m1 `.s`) | `band3/meta_band/SongUpgradeMgr.cpp` | song | links with **no** new undefined symbols; output identical |

### 2.3 Reached stub written with its real body

`VocalPlayer::InTambourinePhrase` (m10_leaf) — **the hottest reached stub,
37,203 calls in one harmony run**, returned constant `false` on the unmeasured
claim "the synthetic run has no tambourine phrases". Its real body is
`return mTambourineManager.unk60 > 0;` at `band3/bandtrack/VocalTrack.cpp:2827`
(retail places it in VocalTrack's TU). VocalTrack.cpp drags the bandtrack/UI
render closure and cannot link here, but the body only reads
`TambourineManager`, which vocal2/harmony do link — so the real expression now
lives in the stub file, verbatim, with a keep-in-step note. vocal2/harmony
output byte-identical: the old assumption happened to hold for these charts;
it is now code rather than an assumption.

### 2.4 Deleted as dead in every target (link-verified)

- `dta_link_stubs.s`: the entire `RndAnimatable` stub set (ctor, Copy / Load /
  Save / Handle / SyncProperty / SetFrame, 5 virtual thunks, VTT, typeinfo) —
  the real `rndobj/Anim.cpp` is in `RNDOBJ_SOURCES` for every target, so these
  were overridden everywhere; `NativeArkRead` (overridden); `DmGetSystemInfo`,
  `Mod(float,float)` (unreferenced).
- `m1_link_stubs.s`: `SongMetadata::NumVocalParts` (overridden by SongMetadata.cpp).
- `m3_link_stubs.s`, `m3b_link_stubs.s`: empty since creation; files deleted.
- m6/m8/m10: `BandDirector::SetCharacterHideHackEnabled`; m10:
  `NetSession::IsLocal` (§2.1).
- m10_leaf: `NetSession::HasUser`, `ProfileMgr::UpdateAllMicLevels`,
  `VocalOverlay` ctor/dtor, four `Message` globals, `Symbol tambourine`.
- m10_support: the MWCC-mangled `NoteAt__13VocalNoteListCFf` /
  `PitchAt__13VocalNoteListCFf` forwarders (no caller left).
- milo_link_stubs: the `Hmx::Matrix4::Col3` copy (no native TU calls it any
  more — Lit_NG no longer does), `TheHamWardrobe` + two `HamWardrobe` methods
  (Crowd.cpp no longer references them).
- `native_job_stubs.cpp` (all 45 symbols: the four marketplace enum-job
  classes + `_XMMATRIX` ctor) and `thunk_stubs.cpp` (`lbl_82066608`):
  unreferenced in all 18 targets; files deleted.

## 3. Every stub that executes, and its verdict

Probe counts are calls in one default run (final state); "gdb" = `.s` stub hit
at least once. File attribution: m6 → score2/3, m8 → score4/vocal/crowd,
m10 → vocal2/harmony.

| symbol | where hit | verdict |
|---|---|---|
| `VocalPlayer::InTambourinePhrase` | harmony 37,203, vocal2 25,203 | **WRITTEN** — real body (§2.3) |
| `MetaPerformer::Current` | crowd 3,971, score4 2,039, vocal2 63, harmony 56, score3 19 | **KEPT, behaviour-identical**: real (`MetaPerformer.cpp:199`) returns `sMetaPerformer`, null headless. The TU is the meta/UI layer. |
| `GetTrackPanel` | score4 16, crowd 15 | **KEPT, behaviour-identical**: real (`TrackPanel.cpp:78`) returns `TheTrackPanel`, null headless. |
| `Game::OnPlayerAddEnergy` | crowd 15, score4 8, score3 2 | **KEPT**: real (`Game.cpp:1348`) forwards to `mTrackerManager`, which is null in the drivers' calloc'd `Game` — the real body would crash. Game.cpp links only in milo/render. |
| `BandUser::GetDifficulty` | vocal2 25, harmony 8 | **KEPT**: real (`BandUser.cpp:78`) reads `mDifficulty`; the vocal drivers run with **no** BandUser. BandUser.cpp needs net/session. |
| `Net::Net` | 1× in score2/3/4, vocal, vocal2, harmony, crowd | **OUT OF SCOPE** (`src/network`) |
| `OutputDebugStringA` (`.s`) | gdb: 14 targets | **PLATFORM** — Win32 debug-string sink; the no-op swallows engine debug strings. |
| `RndMesh::OnSync` (milo_link_stubs) | milo 130 | **PLATFORM/GPU** — the X360 body is `#ifndef HX_NATIVE` (`rndobj/Mesh.cpp:1267`); render gets the engine's `MeshGpuCache`; milo does not draw. |
| `HDCache::Flush` (native_link_glue) | 1× ark, midi, milo, render, song | **FAITHFUL** — retail's body is an empty `blr` (0x826C3888, ICF fold); its home TU cannot be read off the image. |
| `ObjRefConcrete<ObjectDir,ObjectDir>::CopyRef` | render 2 | **GLUE** — explicit instantiation of a real header template. |
| `InternSymbolGlobals_MiloLinkStubs` | render 1 | **INIT** — interns the file's Symbol globals. |
| 7 `xdk_shims.cpp` functions (critical sections, `CreateEventA`, `CloseHandle`, `GetCurrentThreadId`) | all targets | **PLATFORM** — POSIX-backed implementations of XDK/Win32 API. |
| m8_support: synthetic `SongDB` (ctor, `GetSongDurationMs` 103,620× crowd, `GetPhraseID`, `GetGems`, `GetCommonPhraseTracks`, `IsUnisonPhrase`, `GetBaseScores`), `Game::GetActivePlayers` / `GetPlayerFromTrack`, `GemPlayer::HasDealtWithGem`; m10_support: `GameMic*` / `GameMicManager*`, `SongDB::GetPitchOffsetForTick` / `GetVocalNoteList*` | score4, crowd, vocal2, harmony | **DRIVER SUPPORT** — synthetic headless substitutes by design (a SongDB backed by the driver's SongData; a synthetic microphone). Measured blocker for the real `SongDB.cpp` in rb3-score4: **26 duplicate definitions** against this layer and **2 undefined** (`Game::GetScoringTracks`, `MultiplayerAnalyzer::MultiplayerAnalyzer(SongData*)`). Close, but swapping it means rewriting how four drivers populate SongDB — not done (§5). |

Everything else in the inventory is **unreached**, measured, not assumed.

## 4. Unreached remainder, by class (from the TSV)

| class | rows | meaning |
|---|---:|---|
| Symbol / Message global link satisfier | 417 | `extern Symbol X;` definitions (retail uses function-local statics, the `RB3_HANDLE_LOCAL_STATIC` lever); interned by the files' own init functions, read by nothing on any run |
| unreached, src body exists but its TU is not native-compiled | 204 | e.g. `BandTrack::*`, `SongDB::*` leaves, `Game::*` remote-tracker hooks, `LicenseMgr` / `ProfileMgr` / `BandMachineMgr` in m1 |
| unreferenced by other objects | 168 | 116 are redundant explicit template instantiations in native_link_glue (consumers instantiate implicitly); the rest are virtuals kept alive by a vtable in the same TU |
| platform shim (`xdk_shims.cpp`, unreached) | 113 | XDK/Win32 API |
| unreached, real TU compiles natively elsewhere | 94 | closure not linkable in that target (e.g. CommonPhraseCapturer in score2/3, below) |
| unreached, no src body anywhere | 62 | Win32/XNet/XInput `.s` stubs (15), `VocalOverlay` debug overlay (12), D3D/GPU leaves in milo_link_stubs (9), 24 `ObjOwnerPtr` save-operator instantiations (the shared undecompiled `operator<<` — milo_link_stubs §1 records the trap), `Character::RemoveFromPoll` |
| RTTI/vtable emitted for a stubbed class | 61 | follows the class's stubbed key function |
| driver support (unreached) | 38 | |

## 5. Negative results and what was deliberately NOT done

- **CommonPhraseCapturer into score2/score3** (to drop its 3 m6 stubs): fails
  to link — the real TU needs the m8 SongDB/Game support layer
  (`SongDB::IsUnisonPhrase`, `GetCommonPhraseTracks`,
  `TrackPanel::UnisonPlayerSuccess`, `Game::GetPlayerFromTrack`). Reverted; the
  stubs are unreached there (probe 0).
- **`AllowedToAccessContent` via `band3/meta_band/Utl.cpp` in rb3-song**: 11
  undefined symbols (BandUser track/controller accessors, MetaPerformer setlist
  queries, `SessionMgr::IsLeaderLocal`, `typeinfo for UIPanel`, …). Reverted;
  unreached.
- **Real `SongDB.cpp` in rb3-score4**: trial measured above; not pursued.
- **Not deleted, on purpose:** the 417 Symbol globals (each also needs its
  init line removed; zero behavioural value), the 116 redundant explicit
  instantiations in native_link_glue (instantiations of *real* templates, not
  stubs), the 89 dead XDK shims (platform API surface).
- **`Character::RemoveFromPoll`** (x7): no body in this tree; rb3-Wii has the
  one-liner `VectorRemove(mPolls, poll)`. Unreached on every run (rb3-render
  included), so not written; the retail address was not identified.
- **`ObjOwnerPtr` `operator<<`**: the permanent fix (definition in
  `ObjPtr_p.h` under `HX_NATIVE`) would retire ~29 hand instantiations; not
  done — unreached, and it is a shared-header change owed elsewhere
  (`docs/plans/x4a-venue-render-2026-08-02.md`).
- **Engine change requests:** none needed by this lane.

## 6. Owed / follow-ups

1. **Run the vocal targets somewhere automatic.** Two targets were dead for two
   months because no instrument ran them. `native_health.sh`'s runtime list is
   milo / ark / render / score2; adding vocal2 + harmony (and crowd) would have
   caught `f3ec9592d` the day it landed.
2. `SongDB.cpp` swap for the M8+ drivers (26 dup / 2 undef measured).
3. Re-probe after any lane that changes a driver's link set:
   `cmake -S native -B native/build-probe -DRB3_STUB_PROBE=ON …`, run, then
   `tools/stub_probe_resolve.py <exe> <dump>`; `.s` stubs need the gdb recipe
   in §1.3 **with its `main` control**.

## Reproduce

```bash
python3 tools/native_stub_census.py native/build /tmp/c.json --authored
cmake -S native -B native/build-probe -G Ninja -DRB3_STUB_PROBE=ON \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build native/build-probe
RB3_STUB_PROBE_OUT=~/tmp/p.txt native/build-probe/rb3-harmony
python3 tools/stub_probe_resolve.py native/build-probe/rb3-harmony ~/tmp/p.txt
```
