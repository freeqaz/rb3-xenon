# W16-UJ — every shipped venue milo loads natively; the native clamp's NaN settled (2026-10-07)

Branch `w16-uj`, worktree `~/tmp/wt-w16uj`, based on main `09d26b810`.
Takes up two failures earlier lanes recorded but did not investigate:

- W16-UF §5.3: a standalone load of `big_club_07.milo` spins after `Stream error: Can't read`
  (`docs/decomp/W16UF_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md`).
- W16-TY §3: `small_club_15`'s load fails partway, so `LightPreset::Load` never ran on a
  completed shipped venue (`docs/decomp/W16TY_VIA_DC3_UNENTERED_ROWS_2026-10-07.md`).

It also settles UF's open note that native float `Clamp` maps NaN to the lower bound.

## 0. Headline

- **All 52 shipped venue milos now load and unload natively.** No load reads a failed stream,
  and no created object leaves the stream off its `0xADDEADDE` marker. That is 44 venue roots
  plus 8 props, banners and `test`.
- **Two native-only defects stopped them, and both are fixed (`HX_NATIVE` arms only).**
  1. `ChunkStream::ReadImpl` failed the stream on `TempEof`. This stopped **all 44 venue
     roots**, not just the two that had been reported. Main's own binary hangs on
     `small_club_01`, `big_club_01` and `arena_01` too (§1.1).
  2. Native `ObjOwnerPtr` had no user-declared copy-assignment. This crashed every arena and
     festival (12 milos) with SIGSEGV in its own **unload** (§1.2).
- **One hardening change.** Native `ReadDead` now stops on a failed stream instead of spinning
  forever. The spin UF saw was this loop, not the defect itself.
- **Clamp verdict: native differed from retail, and is fixed for the native build (§3).**
  - Retail computes float `Min`/`Max` with `fsel`, which passes a NaN difference to `fB`, so
    retail's `Clamp(-2, 2, NaN)` is NaN.
  - Native returned the other operand, so it gave −2.
  - Native `Min`/`Max` are now written the way `fsel` selects. No non-NaN result changed: 796
    cases are identical.
- **56 new gates in rb3-render's default mode (`native/src/w16uj_phase.cpp`), all pass:**
  - 52 venue gates and 1 summary gate;
  - `uj-sc15-lightpresets`: 29 of 29 presets, all with keyframes, checked against
    small_club_15's directory table read from the shipped bytes;
  - 2 clamp gates.
- **Each fix, when reverted, fails its gates as predicted, apart from one wrong prediction
  (§5).** The predictions were written to a file before each build.
  - Round A: 40 FAIL / 9 PASS. Every gate that reported matched its prediction. I predicted no
    crash, and the run crashed in video_01's load.
  - Round B: the one predicted FAIL, and nothing else.
- **X360 A/B: Δ0 on every measure, as predicted (§6).** Leg B recompiled 1,078 TUs.

## 1. Causes

### 1.1 `ChunkStream::ReadImpl`: `TempEof` at a chunk boundary failed the stream

**How it was found.**

- The X4d stream audit (`RB3_STREAM_AUDIT=1`) reported **0 anomalies** on both venues. No
  created object over- or under-read. So this was not a desync.
- Both failures sat exactly at the start of an **uncompressed** chunk (flag `0x01000000`) of
  `0x2004E` = 131,150 B. That is incompressible audio:
  - small_club_15 chunk 58 at stream offset 7,701,339;
  - big_club_07 chunk 106 at 16,718,704.
- The offsets were mapped by decompressing the shipped milos
  (`~/tmp/w16uj_raw/unmilo.py`, `0xCDBEDEAF` blocks).
- A temporary diagnostic at the failure printed `eof=2` (`TempEof`) at tell 7,832,489, the
  **end** of chunk 58. The next chunk's buffer was in state 1 (`kReading`).

**Mechanism.**

- The native read-ahead keeps 2 buffers. `Eof()` issues the read for chunk k+1 only once
  chunk k−1's buffer has been released. It marks chunk k+1 ready only on a **later** call:
  `ReadDone` first, then the decompress or the uncompressed-ready step.
- Native has no factories for `Sfx`, `SynthSample`, `MoggClip` and `BandCamShot`, so it skips
  those objects with `ReadDead`. `ReadDead` reads byte by byte and never polls `Eof()`.
- A skip that spans a whole chunk therefore reaches that chunk's end with the next buffer
  still `kReading`.
- The native cross-chunk branch of `ReadImpl` assumed "on native all I/O is synchronous,
  TempEof should not happen" and failed the stream. It came in with the 2026-05-26 dc3
  scaffold (`c5c1650fb`).
- `ReadDead` on a failed stream then reads zeros forever. That is UF's spin.

**Fix (`src/system/utl/ChunkStream.cpp`, `HX_NATIVE` arm).**

- Keep polling `Eof()` while it returns `TempEof`, bounded at 100,000 polls with
  `Timer::Sleep(0)`, as `BinStream::WaitUntilReady` already does.
- Fail only on a real end of stream or a timeout.
- Retail polls `while (Eof() == TempEof)` around the same pipeline (`ReadChunks`).

**Reach.**

- The rule "a chunk lies wholly inside one factory-miss span" was computed over every
  venue's own stream (`~/tmp/w16uj_sweep/predict_s1.py`). It predicts **all 44 venue roots**
  fail. The 8 props, banners and `test` have no misses.
- It names the observed failing chunk exactly for both reported venues: 106 and 58.
- Round A (§5) confirmed all 37 venue roots it reached.
- Main's own pre-fix binary (`native/build/rb3-render`, built 17:18) **hangs** on
  `small_club_01`, `big_club_01` and `arena_01` standalone: rc 124 after 90 s, one
  `Can't read` each.
- Earlier X-series lanes did render `small_club_01`. This lane did not establish what
  differed then.

**Hardening (`src/system/obj/DirLoader.cpp`, `HX_NATIVE`).**

- `ReadDead` returns when `bs.Fail()`.
- `BinStream::Read` counts reads on a failed stream in `gNativeFailedStreamReads`, because the
  existing notice fires only once per process.
- The X4d stream audit gained a quiet mode and three counters for the gate.

### 1.2 `ObjOwnerPtr` copy-assign: every arena and festival crashed in its unload

**Symptom.**

- With 1.1 fixed, all 10 arena roots and both festivals loaded and rendered.
- Each then died with SIGSEGV in `dirPtr = nullptr`:
  `ObjOwnerPtr<RndTransformable>::NullifyObj` ← `Hmx::Object::NullifyAllRefs` ←
  `ObjectDir::~ObjectDir` ← `WorldDir::~WorldDir` (gdb, ELF symbols).
- The fault is in the X16 seed restore, `seed->AddRef(this)`, on a seed whose vtable pointer
  is 0.

**How it was found.** Temporary diagnostics logged each Character dtor, each seed restore and
each seeded `ObjOwnerPtr` construction (all removed again).

- The crashing seed was `female_extras03`'s `RndTransformable`. That extras template had been
  destroyed at depth 0, before the venue teardown started.
- The crashing ref was **constructed** as a different Character's own `mSphereBase`: owner
  `0x…e3a0`, seed equal to that Character.
- At the crash the same ref held the dead template's owner and seed. Owner and seed had been
  overwritten wholesale.

**Cause.**

- The native `ObjOwnerPtr` declares a copy constructor, and it deliberately drops the seed.
  It declares no copy-**assignment**.
- So `mSphereBase = c->mSphereBase` used the implicit operator. It memberwise-copies `mOwner`
  and `mSelfSeed` from the source.
- `Character::Copy`'s `COPY_MEMBER(mSphereBase)` is the likely site. The diagnostics proved
  the overwrite but not which call made it.
- The X360 arm already declares `void operator=(const ObjOwnerPtr &o) { SetOwnerObj(o.mObject); }`.
  Its comment says retail calls `SetOwnerObj` at every such site.

**Fix (`src/system/obj/Object.h`, native class).** The same user-declared copy-assignment
through `SetOwnerObj`. The destination keeps its own owner and seed.

## 2. The 52 venue milos

The sweep ran each milo standalone in rb3-render's cell mode (`--frames 1`,
`RB3_STREAM_AUDIT=1`, 300 s timeout), first with 1.1 fixed and then with both fixes
(`~/tmp/w16uj_sweep/sweep.sh`, `fix/` and `fix2/`). "Before" is the 1.1-only sweep.

- **Load.** All 52 loaded on both sweeps with 0 `Can't read` and 0 audit anomalies.
  Factory-miss skips: 1,492–1,570 per arena, 1,018–1,040 per festival, 1,074–1,124 per big
  club, 709–738 per small club, 622–624 per video venue, 0 for the 8 props.
- **Unload.** In the "before" sweep the 10 arena roots and both festivals crashed (rc 139).
  With both fixes all 52 unload.
- **Render.** 44 render and pass every cell gate. The other 8 are listed below.

| milo | load | unload | render (one frame, cell harness) | outcome |
|---|---|---|---|---|
| 10 arena roots, 2 festivals | clean | **SIGSEGV before 1.2**, clean after | RENDER | fixed (1.1, 1.2) |
| 22 big clubs, 11 small clubs, video_01 | clean after 1.1 (hung before) | clean | RENDER | fixed (1.1) |
| 4 arena_02 props, stone_block, big_club_12 `test` | clean | clean | RENDER | loaded cleanly before this lane too |
| `arena_11/banner` | clean | clean | **EMPTY**: 2 of 2 meshes draw, coverage 0.00% | **recorded** |
| `big_club_14/banner_mim` | clean | clean | **EMPTY**: 1 of 1 draws, 0.00% | **recorded** |
| `video_02`–`video_07` | clean after 1.1 | clean | **EMPTY**: 60 of 203 draw, 0.00% (video_03 0.01%) | **recorded** |

The 22 big clubs, 11 small clubs and video_01 add up to 34 roots; with the 10 arenas they make
the 44 venue roots.

**Reason recorded for the 8 empty renders.**

- They load and unload cleanly by every stream measure. Draws are issued, but the frame comes
  out at the background colour.
- That is rb3-render's single-frame cell framing or material path, not a load defect.
- It was not investigated. These milos' `uj-venue` gates pass, and the venue gate does not
  render.

**Factory misses.**

- The misses (`BandCamShot`, `Sfx`, `SynthSample`, `MoggClip`, `ParallelGroupSeq`,
  `RandomGroupSeq`, `SynthFader`, `FxSendEQ`, `BandIKEffector`) are classes rb3-render does
  not register.
- Their skip is exact: every skip in the audited streams ends on a real marker.
- Registering them is out of scope here.

## 3. Clamp verdict

**Retail.** Read off `fn_822C7040`, `CompressDelta`'s `Clamp(-2, 2, d)`. f13 = −2, f12 = 2,
f0 = the value.

```
fsubs f9,f13,f0 ; fsel f0,f9,f13,f0     Max(min, v) = fsel(min - v, min, v)
fsubs f9,f0,f12 ; fsel f0,f9,f12,f0     Min(x, max) = fsel(x - max, max, x)
```

`fsel fD,fA,fC,fB` computes `fA >= 0 ? fC : fB`, and a NaN `fA` selects `fB`. So:

- retail `Max(x, y)` with a NaN difference returns **y**;
- retail `Min(x, y)` returns **x**;
- retail `Clamp(-2, 2, NaN)` returns **NaN**.

All 508 `fsel` in the retail asm share this NaN rule; it is an instruction property. Whether
every inlined float `Min`/`Max` site compiles to `fsel` rather than a branch was not audited.
The specialisations exist "for the use of fsel instructions", and `fn_822C7040` is the one
site read.

**Native before.**

- Native spelled the select as `(x - y < 0) ? …`, which is false for NaN.
- So it returned the other operand: `Max(-2, NaN)` = −2 and `Clamp(-2, 2, NaN)` = −2.
- Round A (§5) measured this on the reverted code: **164 of 176** NaN-operand cases differed
  from the retail model.

**Fix (`src/system/math/Utl.h`, `HX_NATIVE` arms of the float `Min`/`Max` specialisations).**

- `Min` is `(x - y >= 0) ? y : x` and `Max` is `(x - y >= 0) ? x : y`, i.e. fsel's own form.
  `Clamp` is built from them.
- For non-NaN input `x - y >= 0` is exactly `!(x - y < 0)`. `uj-clamp-finite` checks 796
  non-NaN cases against both the retail model and the old spelling: 0 differ.
- The native build has no `-ffast-math`, and its build type is empty (so `-O0`), so the
  comparison is IEEE.

**Other clamp shapes.**

- The generic `Clamp<T>`, `ClampEq`, `MinEq` and `MaxEq` are branchy on both sides. Utl.h
  records retail's branchy generic `MinEq`. They were not changed.
- UF's `CompressDelta` arm still tests `d[i] != d[i]` first. It is kept, because
  `(long long)NaN` is undefined in C++.

## 4. Gates (`native/src/w16uj_phase.cpp`, rb3-render default mode, `--no-w16uj` skips)

| gate | checks | reference |
|---|---|---|
| `uj-clamp-nan` | float `Max`, `Min` and `Clamp(-2,2,x)` over an 18-value grid, with NaN, ±inf, ±0 and denormals: 176 cases with a NaN operand difference | the retail fsel model above, written in the phase, not a call into Utl.h |
| `uj-clamp-finite` | the 796 non-NaN cases | the same model, and the pre-W16-UJ native spelling |
| `uj-venue <milo>` ×52 | it is in the archive; the root returns; the venue's loads had 0 reads on a failed stream, 0 audited objects off their marker and >0 objects audited | the load's own stream and markers |
| `uj-sc15-lightpresets` | small_club_15 has exactly 29 `LightPreset`s with the 29 shipped names, each with ≥1 keyframe | the 29 `LightPreset` entries in its directory table, read from the decompressed shipped bytes |
| `uj-venues` | 52 of 52 clean and 52 unloaded | — |
| `uj-venue-load` / `uj-venue-unload <milo>` | a SIGSEGV or SIGBUS handler armed only around each load and each unload names the venue in a FAIL line, then re-raises | — |

- **Cost of the phase in the full default run.** Wall time 14.0 s → 52.0 s. Peak RSS 349 MB →
  1,028 MB, measured with `--no-w16uj` against without it.
- **RSS growth.** RSS grows about 13 MB per load and unload cycle: 520 MB at arena_06 and
  1,033 MB at video_06, sampled from `/proc`. So an unload does not return everything.
- That retention is recorded, not investigated. `native_health` runs targets one at a time.

## 5. Sabotage

The predictions were written to `~/tmp/w16uj_sab/{A,B}_predictions.txt` before each build.

**Round A: the `ChunkStream` poll and the `Utl.h` arms reverted**, in one build because their
gates are disjoint.

| prediction | result |
|---|---|
| 44 `uj-venue` FAIL (the venue roots), 8 PASS (props, banners, `test`) | 37 FAIL and 8 PASS, all as predicted. The 7 video gates never printed (next rows) |
| `uj-sc15-lightpresets` FAIL: payloads span 6,775,821–9,348,495 and the stream fails at 7,832,489 | FAIL: 29 loaded, **28** with keyframes |
| `uj-clamp-nan` FAIL with `Clamp(-2,2,NaN) = -2` | FAIL: 164 of 176 differ, `Clamp(-2,2,NaN) = -2`, `Max(-2,NaN) = -2`, `Min(NaN,2) = 2` |
| `uj-clamp-finite` PASS | PASS, 0 of 796 differ |
| **no crash** | **wrong.** SIGSEGV in video_01's **load**, after its stream failed: objects read from a failed stream (zeros) fault later in the load. The handler first covered only the unload, so it was widened to the load and the round was rerun: `[FAIL] uj-venue-load video_01.milo_xbox — signal 11 during the load` |
| no non-UJ gate changes | 0 non-UJ FAIL |

Final round A count: 40 FAIL / 9 PASS of the gates that print before video_01's crash.

**Round B: the `ObjOwnerPtr` copy-assign reverted.**

| prediction | result |
|---|---|
| both clamp gates and `uj-venue arena_01` PASS | PASS |
| `[FAIL] uj-venue-unload arena_01.milo_xbox`, signal 11 | exactly that line. rc −11; the only `[FAIL]` in the run |

Both rounds were restored to the committed source (the tree is clean at `a23d08617`). The
restored build is what `native_health` rebuilt and ran (§8).

## 6. X360 A/B

- **Prediction: Δ0.** Every `src/` change sits in an `HX_NATIVE` arm. This was measured
  anyway, because the edits shift line numbers in `Object.h` and `Utl.h`.
- **Method.** As in W16-UF: a temporary commit reverted `src/`, the `src/` diff was measured
  as a patch, then the commit was dropped (`git reset --hard HEAD~1`).
  `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16uj --patch <src diff> --jobs 12 --label w16uj-hxnative-src`

```
leg A: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 0, settled)
leg B: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 1078, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.315315 -> legB 64.315315)
units at 100% [mpn ruler]: legA 607 -> legB 607
```

The `none` control was flat as well (+0 B).

## 7. Not done

- **The 8 empty renders (§2)** are recorded, not investigated.
- **The factory misses (§2)** remain. rb3-render still skips `Sfx`, `SynthSample`, `MoggClip`,
  `BandCamShot` and five other classes; the skips are exact.
- **The ~13 MB per venue RSS retention (§4)** is recorded, not investigated.
- **`../dc3-decomp/src/system/utl/ChunkStream.cpp` carries the same `TempEof`-as-failure
  branch.** Not touched; it is a follow-up for that repo. The `ObjOwnerPtr` self-seed is
  rb3-xenon's own (X16), so 1.2 has no DC3 counterpart.
- **`LightPreset::Load` itself is not checked against retail here.** The gate shows the 29
  shipped presets load on a completed venue, which is what TY lacked. A body check is TY's row.
- **Whether every float `Min`/`Max` inline site in retail is `fsel` (§3)** was not audited.

## 8. Result lines

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=400 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=13 scatter_dirb=0 scatter_multihost=10 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none retail_config=12/12 unhandled_calls=0
```

`tools/native_health.sh ~/tmp/wt-w16uj` at `a23d08617`. Its rb3-render run reported 303 gates passed;
56 of them are `uj-*` gates, and none failed:

```
[PASS] uj-venues — 52 of 52 shipped venue milos loaded cleanly, 52 unloaded
```

The native build gate's line is in the lane's final report. The build gate ran as the last
action, after this doc was committed.

## Reproduce

```
cmake --build native/build --target rb3-render
native/build/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>          # W16-UJ phase runs by default
RB3_STREAM_AUDIT=1 native/build/rb3-render <assets> <outdir> world/venue/small_club/small_club_15/gen/small_club_15.milo_xbox --frames 1
tools/native_health.sh <worktree>
tools/native_build_gate.sh
```
