# Lane leaks — rb3-app heap growth per song and leak triage (2026-11-23)

Branch `leaks`, worktree `~/tmp/wt-leaks`, base main `6db9389e7`. Scratch, logs and binaries:
`/home/free/tmp/claude-1000/rb3-scout/leaks/`. Follows asanwalk2
(`ASANWALK2_RB3APP_SONG_WALK_2026-11-23.md`) and scorewalk
(`SCOREWALK_DETERMINISTIC_SCORING_WALK_2026-11-23.md`), whose walks all ran with `detect_leaks=0`.

## Predictions (written before the first run)

- rb3-app ends with `_exit` (`main_app.cpp` `Finish`), so LeakSanitizer's at-exit check has never
  run on it; an LSan check must be called from inside the walk.
- Expect LSan to report mostly one-shot boot allocations whose last pointer was dropped
  (driver/Vulkan, statics) and a small per-song set; expect per-song live-heap growth of a few MB
  at most, dominated by retained per-song state (Symbol table, song metadata caches) rather than
  unreachable leaks.

## Headline

- **No per-song growth trend.** Plain build, nine-song walk (run P2, 7 endgames reached in 540 s):
  glibc in-use heap at each endgame reads 265.7, 272.6, 276.4, 270.9, 273.4, 274.0, 266.8 MB
  (songs thecon, iwannabesedated, smokeonthewater, falsealarm, stopmeifyouthink, foolin,
  crosstowntraffic). It moves with the song's content, ±5 MB, and song 7 sits 1.2 MB above song 1;
  mmapped blocks stay at 22.82 MB throughout. Prediction (a few MB, retained state) held for the
  size; the "grows" half was wrong: it does not accumulate.
- **LeakSanitizer, first ever run on rb3-app:** 4.26 MB unreachable at song 1's endgame, 4.28 MB at
  song 2's (+25 KB, +46 allocations). The only per-song leak was **ours and is fixed**: the HX_NATIVE
  arm of `~WorldDir` never deleted the ctor-created glow material that retail deletes (one `RndMat`
  plus its color vector per WorldDir; 51 by song 1's endgame, +23 per song). Commit `2ab24492a`.
- Everything else LSan reports is one-shot boot state and does not grow (table in §2): 4.02 MB is the
  NVIDIA EGL core driver (`libnvidia-eglcore.so.615.78.08`, no caller frames), ~1 KB dbus, 131 KB
  is retail's own leaked `NewFile("charnames.zbm")`, and ~50 KB is small DataArray / Sequence-inst
  / TaskMgr debris, recorded and not fixed.
- The ASan live-heap profile diff song 1 -> song 2 (+6.46 MB, -6,068 allocations) is texture
  bitmaps (`RndTex::PostLoad` -> `RndBitmap::AllocateBuffer`, +7.29 MB), meshes (+1.02 MB) and
  samples (+0.43 MB) loaded by DirLoader for song 2's content, offset by frees elsewhere; with the
  seven-song plain trend, that is song 2's content being larger than song 1's, not song 1's
  content retained.

## Method

- Instrument (commit `747ecba98`, `native/src/rb3_app_determinism.cpp`, native-only):
  `RB3_APP_HEAPSNAP=1` takes a snapshot 100 frames (`RB3_APP_HEAPSNAP_DELAY`) after each entry into
  `coop_endgame_screen`, i.e. at the same point of every endgame and before the walk's first press
  there (+120). It prints glibc `mallinfo2` (`inuse` = `uordblks`, `mmap` = `hblkhd`) and, in an
  ASan build, `__sanitizer_get_current_allocated_bytes`. `RB3_APP_HEAPSNAP_PROFILE=1` adds ASan's
  `__sanitizer_print_memory_profile(100, 1000000)` (every live allocation stack) and
  `RB3_APP_HEAPSNAP_LSAN=1` a `__lsan_do_recoverable_leak_check`. This is needed because rb3-app
  ends with `_exit` (`main_app.cpp` `Finish`), so LSan's at-exit check never runs; an in-walk
  check at the endgame is the "LSan at exit" of a process that never tears down (neither does
  retail). All sanitizer entry points are weak, so the plain build links them as null.
- Builds (worktree `~/tmp/wt-leaks`, live `../milo-native-engine`, `leaks/build.sh`):
  `native/build` (plain, the shipping `-O0`) and `native/build-asan` (`-DRB3X_SANITIZE=address`,
  no UBSan, `-g`). Frozen: `leaks/bin` `1c167dd1da21`, `leaks/bin-asan` `fa107dd99f23` (base +
  instrument), `leaks/bin2` `89ddf20b0b36`, `leaks/bin-asan2` `e927bbed51b1` (+ the fix).
- Runs: `leaks/runb.sh` = a25/runb.sh with `A=` the leaks dir, `X=~/tmp/wt-leaks`,
  `RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1`, through `coord/no610m.sh` under the shared gpu1
  lock, `SECS=540 TMO=580`, 90,000 frames, seed 0. Scripts: `a25/walk.txt` (two songs),
  `leaks/walk3.txt` / `leaks/walk9.txt` (the a25 endgame block repeated, three and nine songs).
  ASan runs: `ASAN_OPTIONS=halt_on_error=0:detect_leaks=1:malloc_context_size=20:symbolize=1`.
- Analysis: `leaks/tools/heapdiff.py <run.log> A B [--lsan]` groups profile or LSan entries by
  stack (allocator / STL frames dropped, first N frames kept) and prints per-context growth B-A.
  Song 1 -> 2 profile diff: `leaks/profdiff12.txt`; LSan report at song 1: `leaks/lsan1.txt`.

| run | binary | script | ASan options / snap extras | endgames snapped |
|---|---|---|---|---|
| P1 | bin | walk3 | - | 3 |
| P2 | bin | walk9 | - | 7 |
| S1 | bin-asan | walk | profile + LSan | 2 |
| F1 | bin-asan2 (fix) | walk | LSan | 2 |
| P3 | bin2 (fix) | walk9 | - | 5 (slower: shared CPU) |

S2 (bin-asan, walk3, unsymbolized profiles) was stopped by hand at song 1: at ~32 frames/s it could
not reach a third endgame (frame 26,180) inside 540 s, so it would only have repeated S1.

## 1. Per-song heap (plain build)

| snap | frame | song | P1 inuse MB | P2 inuse MB | P2 arena MB |
|---|---|---|---|---|---|
| 1 | 8422 | thecon | 265.61 | 265.68 | 266.74 |
| 2 | 14329 | iwannabesedated | 272.64 | 272.60 | 277.14 |
| 3 | 26180 | smokeonthewater | 276.37 | 276.40 | 278.19 |
| 4 | 31462 | falsealarm | - | 270.88 | 281.67 |
| 5 | 39258 | stopmeifyouthink | - | 273.39 | 284.29 |
| 6 | 48820 | foolin | - | 273.97 | 280.54 |
| 7 | 54448 | crosstowntraffic | - | 266.85 | 279.89 |

P1 and P2 agree to 0.08 MB on the common snaps (same frames; the walk is deterministic, the heap
nearly so). ASan S1 agrees on the first step: live heap 283.20 -> 289.66 MB (+6.46 MB) against
plain +7.0 MB. The arena (heap high-water held by glibc) creeps 266.7 -> 284.3 -> 279.9 MB, which
is fragmentation and `free` space (`fordblks` 1.0 -> 13.0 MB), not live data.

What this instrument cannot see: a reachable accumulation below ~1 MB per song hides inside the
±5 MB content swing. LSan bounds the unreachable part exactly; the reachable part is bounded only
by the trend above (song 7 vs song 1: +1.2 MB over six song changes).

## 2. LeakSanitizer triage (S1, song 1 and song 2 endgames)

Δ = song 2 minus song 1. D/I = LSan's direct / indirect class. "Retail" is the decision against
`band.exe` (clean TU5) or the source's own retail evidence.

| site (allocation stack, top own frame) | song 1 | Δ | verdict |
|---|---|---|---|
| `WorldDir::WorldDir` -> `Hmx::Object::New<RndMat>` (world/Dir.cpp:50), D, + `ResetColors` vector, I | 51 mats, 56 KB | +23, +25 KB | **native bug, fixed** (§3) |
| `libnvidia-eglcore.so.615.78.08` malloc/calloc, no further frames, D 3.67 MB + I 0.35 MB | 4.02 MB | 0 | driver, not ours |
| dbus (`dbus_bus_register`), D+I | 0.8 KB | 0 | system library, not ours |
| `AsyncFile::Init` buffer + `AsyncFileNative` from `NewFile("charnames.zbm")`, `App::App` (App.cpp:277), I 128 KB + D 88 B | 131 KB | 0 | **retail-faithful**: retail `App::App` 0x82271528 `bl NewFile` (0x825173E0), `cmplwi r3,0` / `bne`, and r3 is overwritten at 0x82271540 without a store or delete: retail leaks this File too |
| `DataArray` headers from `DataNode::Load` (DataNode.cpp:926/920) under `DataReadFile`: `Locale::Init` (309), `ReadSystemConfig`, `DataLoaderThreadObj::ThreadStart`, ~1,400 D in total | ~44 KB | 0 | one-shot boot debris, **open** (below) |
| `Sequence::Play` <- `EventTrigger::TriggerSelf`: `ParallelGroupSeqInst` / `RandomIntervalGroupSeqInst` / `SfxInst` + `SynthSample::NewInst`, all I (a reference cycle) | 2.5 KB | 0 | **open**, small, does not grow |
| `TaskMgr::TaskMgr` (Task.cpp:445) `new TaskTimeline[4]`, D | 264 B | 0 | native-only ctor allocation that `TaskMgr::Init` (Task.cpp:662) overwrites; retail's ctor allocates nothing (0x82747998). Not fixed: something native may start a task before Init, and freeing at Init would drop it; one-shot |

Open: the DataArray debris. The leaked headers carry no leaked node storage (only 34 `NodesAlloc`
blocks, 0.7 KB, are indirect), so these are arrays whose last reference was dropped while their
refcount stayed above zero, or empty arrays. One candidate, not tested: `DataArray::Load` reads a
node that a false `#ifdef` then skips into the same `mNodes[i]` slot, and `DataNode::Load`
overwrites the slot without releasing an array it held. Retail's `DataArray::Load` (0x8274D460)
has the same read-into-the-slot shape (the HX_NATIVE comment at DataNode.cpp:882 cites it), so
this would be retail-faithful if it is the cause. It is boot-only and does not grow.

## 3. Fix: `~WorldDir` deletes the glow material on native (`2ab24492a`)

Retail `~WorldDir` (0x824CE910) deletes `mHUDDir` and then the ctor-created `mGlowMat` (+0x2C0)
through its deleting dtor; `6c2e12b68` gave the X360 arm exactly that and left the HX_NATIVE arm
as `RELEASE(mHUDDir)` alone, so native leaked one `RndMat` per WorldDir (each venue / world load
builds several). The native arm now adds `RELEASE(mGlowMat)`; the X360 arm is unchanged.
`tools/ab_measure.py --from-dirty`: Δmatched +0, Δhonest +0, Δcode +0 B, units at 100% 635 -> 635
(`leaks/ab-glowmat/20261123-070350-glowmat-3721435/result.json`), as expected for a native-only
arm. Nothing else references `mGlowMat` (grep over src, native/src and the engine).

### 3.1 Verification (runs F1, P3)

Predictions: LSan Δ between the two endgames goes to 0 and song 1's report drops by the 51 mats
and their color vectors; the det walk does not move (freeing a material touches no RNG, clock or
screen state).

- **F1 (ASan + LSan, fix):** 4,199,890 B in 2,666 allocations at song 1's endgame **and the same
  4,199,890 B in 2,666 at song 2's: Δ 0 B, 0 allocations** (S1, base: Δ +25,208 B, +46). Song 1's
  report is 55,896 B / 102 allocations smaller than S1's, i.e. exactly the 51 `RndMat` + 51
  `ResetColors` vectors. No `WorldDir::WorldDir` frame left in the log; zero `ERROR:
  AddressSanitizer` lines in F1 (and S1), so the new deletes are not followed by a use.
- **P3 (plain, fix) vs P2 (plain, base):** `tools/rb3_app_det_cmp.py` reads **IDENTICAL over all
  42,534 common frames** (P3 ran fewer frames in its 540 s only because it shared the CPU).
  In-use heap at the same endgames, P3 minus P2: -0.11, -0.30, -0.47, -0.60, -0.77 MB for snaps
  1-5. The fix removes about 0.13-0.17 MB per song in the plain build, more than LSan's 25 KB
  per song of `RndMat` storage. Not chased: the likeliest extra is per-material native-renderer
  state the deleted material releases, which LSan sees as reachable.

## 4. Not done

- No fix in Rnd/Draw/Text/Spotlight, ui, char or the net sync / game-start code was needed: none
  of the per-song leaks or growth sites is there. (The song 1 -> 2 profile's largest growth sites
  are `RndTex::PostLoad` / `RndMesh::Load` / `SynthSample` loads, and the seven-song trend shows
  them bounded, so there is nothing to propose for the excluded rings.)
- The open one-shot items (§2: the DataArray debris, the Sequence-inst cycle, TaskMgr's ctor
  timelines) are boot-only and do not grow; recorded, not fixed.
- The `libnvidia-eglcore` leak (4.02 MB, flat) is the driver's.
- `tools/native_health.sh --selftest` not run (brief). The fix is inside `#ifdef HX_NATIVE`, so the
  X360 match build is unchanged (A/B Δ0 above); the native rb3-app and its ASan build were rebuilt
  and walked (F1, P3).
