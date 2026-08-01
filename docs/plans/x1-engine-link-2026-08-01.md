# X1 — link milo-native-engine into rb3-xenon, and prove it with a frame

**Date:** 2026-08-01
**Engine:** `milo-native-engine` @ `2ea8e343cdc7c8ce680b093433f8b3d038e38b99` (pinned; **zero** engine edits)
**Predecessor:** [SPIKE-X0](spike-x0-engine-dc3-flavor-2026-08-01.md) — "COMPOSES"
**Ladder:** `rb3/docs/native/xenon-bridge-2026-08-01/ASSESSMENT.md` (X0–X4)
**Commits:** `37b97b6e`, `61162969`, `50c5a80f`

---

## Verdict: **LINKS, RUNS, AND DRAWS**

X0's stated limitation was *"nothing was linked into an executable and nothing was
run. The deliverable is `libmilo-engine.a`, not a frame."* X1 closes exactly that
gap. `rb3-frame` stands up the engine's WebGPU device headless on a real adapter,
clears an offscreen target, reads it back, and writes a PNG — deterministically.

The engine compiled **clean on the first try, zero errors, zero modifications to
`milo-native-engine`**, with the 5-entry exclusion list X0 predicted. There were
no surprises on the engine axis at all.

**The surprises were all on the xenon side, and none of them were caused by X1.**
The native build was already dead on `main` when this milestone started, in four
independent ways, and repairing it consumed more of X1 than the engine wiring did.
That is the headline finding, and §3 is the part a reader should not skip.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | All 15 pre-existing native targets configure + build; `tools/native_build_gate.sh` passes | ⚠️ **PARTIAL — and the baseline was 0/15, not 15/15** | 7 of 15 restored from dead to linking (`dta, midi, gem, hit, score, save, ark`); 8 still blocked by a pre-existing defect X1 did not introduce and deliberately did not fix (§3.3). **Zero regressions attributable to X1.** |
| b | `rb3-frame` produces a PNG of the expected solid colour, deterministically | ✅ **PASS** | 320×180 → all 57,600 px exactly `(48,96,160,255)`; two runs byte-identical, `sha256 3371f9e0…`. Repeated at 317×181 (row pitch 1268 B, *not* 256-aligned → exercises the readback de-padding path): all 57,377 px exact, byte-identical, `sha256 7875470e…`. Real adapter: **NVIDIA RTX 3090** via Dawn/Vulkan; the target *refuses to certify* a frame from the null backend. |
| c | Engine builds with zero modifications to `milo-native-engine` | ✅ **PASS** | Engine `HEAD` still `2ea8e343…` = the pin. Only dirty file in that tree is `src/platform/FxSendNative.cpp`, which was already modified by another agent **before X1 began** — and it is on our exclude list, so it cannot even enter this build. |

**Gate (a) needs its caveat stated plainly, because "partial" is doing a lot of
work.** The gate script reported `FAIL` before X1 touched anything. Its measured
progression:

| state | failing TUs visible | targets linking |
|---|---|---|
| `main` @ `443070fe` (X1 start) | 1 (masking 5 more) | **0 / 15** — the 8 binaries on disk were stale, dated Jul 31 |
| after `37b97b6e` (compile fixes) | 6 | 0 / 15 (all 15 now dead at *link*) |
| after `61162969` (link fixes) | 6 | 7 / 15 |
| after `50c5a80f` (X1 wiring) | 6 | 7 / 15 **+ `rb3-frame`** |

⚠ **Why the starting point read as "8 targets linked" and was not.** The gate
script counts *binaries that exist* rather than "Linking" lines — a deliberate
fix for incremental builds (its own comment explains it). But that makes a
**stale** binary indistinguishable from a fresh one. All 8 were dated `Jul 31
19:09`, i.e. before the breakage landed. Suggested hardening: compare each
binary's mtime against its newest input, or have the gate `ninja -t targets`
verify up-to-dateness. As written, the gate can report a healthy count for a
tree in which nothing has linked for days.

⚠ **Second gate-script hazard, and the reason four separate defects hid behind
one error line:** the gate runs plain `cmake --build`, i.e. ninja's default
`-k1`, so it **stops at the first failing TU**. On a tree with independent
breakages it reports the first and conceals the rest — it took four
fix-and-rerun cycles to discover there were four. Suggested hardening: run
`cmake --build build -- -k 0` and report the full distinct-error set. This is
cheap and would have made the whole of §3 a single observation.

---

## 2. What landed

### `native/CMakeLists.txt` — new `RB3X_BUILD_ENGINE` block (default ON)

Promotes the X0 harness verbatim where it was measured, and says why at each
point where a choice was made:

- **Soft SHA pin** `MILO_ENGINE_PIN = 2ea8e343…`, warn-never-fail, mirroring
  `rb3/native/CMakeLists.txt:88-102`. Verified against engine `HEAD`.
- **Include order is load-bearing**: `native/src`, `src`, `src/system` — `src`
  **before** `src/system`, or a stub at `src/<dir>/Foo.h` shadows the real
  `src/system/<dir>/Foo.h` (the hazard `CLAUDE.md` "Build wiring" documents; a
  stub `src/os/Debug.h` once broke every macro-using header this way).
  `band3/`, `network/`, `oggvorbis/` are **deliberately absent** — X0 *measured*
  that adding them changes nothing.
- **No consumer STL shim injection**: the engine adds its own `include/` shim
  `BEFORE PUBLIC`, and it is byte-identical to
  `native/include/bits/stl_iterator.h`.
- **`MILO_ENGINE_DECOMP_PCH` deliberately unset.** xenon's PCH (`system.pch`
  from `src/system/decomp_pch.h`) is a **match-build codegen device** — `CLAUDE.md`
  calls it sacred and load-bearing. It has no business in the native build.
- **`MILO_ENGINE_GPU_BACKEND=dc3`**, `MILO_ENGINE_BUILD_GFX=ON`, sticky
  `CACHE … FORCE` so a stale value cannot linger.
- **The `-Werror=` opt-in list is NOT forwarded to the engine**, and the block
  records the measurement rather than the preference: X0 showed that forwarding
  it fails exactly one engine TU — `Mesh_Wgpu.cpp:206,:299`, `GetDrawMode() == 8`
  against a `Rnd::DrawMode` enum topping out at `kDrawVelocity = 6`. That is a
  **real latent engine defect** (two two-sided-cull overrides dead on *every*
  consumer, DC3 included, hidden by its blanket `-w`) and belongs on the engine
  backlog, not in xenon's build gate. Re-enable once the engine is clean; xenon's
  warning policy is a net asset to the shared engine.
- **Dawn absence disables the engine with a warning instead of hard-failing.**
  `find_package(Dawn REQUIRED)` *inside* the engine would abort configure and take
  all 15 pre-existing targets down with it on any machine without
  `dc3-decomp-deps`. A bridge milestone must not be able to brick unrelated work.

### `native/src/main_frame.cpp` — `rb3-frame` (~140 lines)

Links `libmilo-engine.a` and **nothing else from either tree** — no
`ENGINE_SOURCES`, no `NATIVE_SHIMS`, no `dta_link_stubs.s`. That is a *result*,
not a shortcut: `gfx/GpuDevice.cpp` and `gfx/Screenshot.cpp` are decomp-agnostic
and reference no Milo symbols, so the archive satisfies itself.

Four deliberate design choices, each defending against a specific way this
milestone could have produced a *fictional* pass:

1. **Clear-only — no pipeline, no vertex buffer, no bind groups, no depth
   attachment.** Anything more drags in a decomp-shaped contract
   (`VertexFormats`/`UniformStructs`) this target has no business asserting.
2. **Refuses the null backend.** A CPU/null adapter would report success while
   producing nothing real.
3. **Verifies every pixel, not a sample.** A partial clear from a wrong viewport
   or a wrong readback row stride is exactly what a centre-pixel spot check waves
   through — which is why the 317×181 (non-256-aligned pitch) run is reported
   above alongside the aligned one.
4. **Clear colour exact in unorm8** — `(48,96,160,255)`, each channel `k/255` —
   so the `f32 → unorm8` round trip cannot land between two representable values
   and cannot tie-break differently between runs or drivers.

---

## 3. The pre-existing breakage — the part that matters most

**Every defect below was on `main` before X1 started.** Three landed in
`cb926469` (*"land NCCC-0731-5f08 — wave 3 … +366 net"*, 199 files) without the
native gate being run, which is precisely the rule
`tools/native_build_gate.sh`'s own header states: *"run this before landing ANY
change that touches shared `src/`."*

This is the structural hazard the gate script exists to cover, restated with a
fresh instance: **the X360 match build never links.** It compiles TUs and
byte-compares objects, so undefined symbols, renamed members and ODR faults are
*invisible* to it. A wave can be +366 matched functions and simultaneously leave
the only runnable build in the project completely dead — and nothing in the
matching pipeline can notice.

### 3.1 Fixed — compile breaks (`37b97b6e`)

| site | defect |
|---|---|
| `utl/ChunkStream.cpp:169`, `os/Archive.cpp:266` | Both TU-locally redefine a `MILO_*` macro to `MiloStripEval`, which is declared **only** `#ifndef HX_NATIVE` (`os/Debug.h:89`) because it is a retail-codegen device, not a logger. Guarded with the same `#ifndef HX_NATIVE` `MidiParser.cpp:65` already uses. |
| `native/src/platform/PlatformMgr_Native.cpp:20` | `memset(&mOverlapped, …)` against a member deleted from `os/PlatformMgr.h` on 2026-07-31 (lane NCCC f59 — the member-block **size** is load-bearing for the retail vbase offset, so the DC3-only `XOVERLAPPED` block was removed). |
| `native/src/native_undecomp_stubs.cpp:29` | `User::UnkTU5Virtual_beforeUserName` renamed to `User::IsNullUser` in `cb926469` (`os/User.h:51`). |

X360-neutrality of the two shared-`src/` hunks: both are `#ifndef HX_NATIVE`
guards, and the match build's cflags carry **no `/D` at all** (`CLAUDE.md`), so
`HX_NATIVE` is never defined there and the preprocessed token stream is
unchanged. The other two are `native/`-only and cannot reach the X360 build.

### 3.2 Fixed — link breaks (`61162969`)

`os/FileCache.cpp:381-384` gained calls to `CacheResource()` and `CacheWav()`,
both defined in directories the native build does not compile
(`rndobj/Utl.cpp` — only `rndobj/Anim.cpp` is in `RNDOBJ_SOURCES`;
`synth/Utl.cpp` — `synth/` pulls tomcrypt's broken `<angled>` sibling includes).
All seven otherwise-healthy targets died at link.

Neither replacement is a no-op stub — each reproduces the branch that is
genuinely live on a native host (`CacheWav` → the `kPlatformPC` early return,
`synth/Utl.cpp:27-29`; `CacheResource` → the `kCacheUnnecessary` path,
`rndobj/Utl.cpp:1204`, plus the real function's own nullptr-on-empty guard at
`:1166`). The branches *not* reproduced rewrite the path to
`<dir>/gen/<base>.<ext>_<platform>` and ask a **Holmes asset server** to cook it
— console-development machinery with no native counterpart. Both carry a
delete-me marker for when `rndobj/` (X2) or `synth/` joins the build.

### 3.3 NOT fixed — `utl/Symbols*.h` × POSIX identifier collision ⛔

**The single remaining blocker; 8 targets** (`song`, `score2/3/4`, `vocal`,
`vocal2`, `harmony`, `crowd`).

```
utl/Symbols2.h:789   extern Symbol close;     vs  <unistd.h>:358      int close(int)
utl/Symbols2.h:1375  extern Symbol environ;   vs  <unistd.h>:566      char **environ
utl/Symbols4.h:649   extern Symbol pause;     vs  <unistd.h>:489      int pause(void)
utl/Symbols4.h:1505  extern Symbol send;      vs  <sys/socket.h>:138  ssize_t send(...)
utl/Symbols.h:629    extern Symbol sleep;     vs  <unistd.h>:464      unsigned sleep(unsigned)
utl/Symbols.h:949    extern Symbol sync;      vs  <unistd.h>:1005     void sync(void)
```

The POSIX side enters via `src/xdk/xnet/winsockx.h`'s `#ifdef HX_NATIVE` branch
(lines 4-12), reached from game TUs through
`meta_band/… → net/NetSession.h → net/SessionMessages.h → game/NetGameMsgs.h → xdk/xnet/winsockx.h`.

⚠ **Include order cannot fix this, and it is worth being explicit about why**,
because "just include one before the other" is the reflex: C++ forbids an
*object* and a *function* sharing a name in one scope **in either order**. This
is not a shadowing problem with a winning side; it is a hard redeclaration
conflict. Any fix must remove one of the two declarations from the TU.

Two candidate fixes, both shared-header architecture changes that want an owner
decision rather than an X1 drive-by:

- **(A) Make `winsockx.h`'s `HX_NATIVE` branch types-only.** It needs
  `struct in_addr`/`sockaddr_in`, `SHUT_WR`/`SHUT_RDWR`, and `errno` for a
  header-level shim; the actual socket *calls* live in `NetworkSocket_*.cpp`,
  which can include the real POSIX headers itself. Gate the heavy includes
  behind a macro that only those TUs set. Narrow, but needs verification against
  every socket TU.
- **(B) Stop declaring ~10k `Symbol` globals at namespace scope.** The correct
  long-term shape, and it fixes this class permanently rather than the six
  instances that happen to collide today — `open`, `read`, `write`, `time`,
  `select`, `index`, `link` and friends are all one `#include` away from joining
  them. Large change.

**(B) is the one that stops this recurring.** (A) is a smaller patch that buys
X2 headroom now. Recommend (A) as an X2 prerequisite and (B) as a tracked
follow-up.

---

## 4. Exclude-list state — unchanged at 5

| TU | xenon-side disagreement |
|---|---|
| `DataParser_Native.cpp` | engine `extern int gDataLine` vs `obj/DataFile_Flex.h:9 extern DataType gDataLine` |
| `FxSendNative.cpp` | `EQEffect::Params` lacks `mBand4Q`/`mBand5Freq` |
| `Joypad_Native.cpp` | `JoypadData` lacks `mNumAnalogSticks` |
| `Synth_Stub.cpp` | `VorbisReader` ctor 5 args (engine passes 4); `StandardStream` 6 (engine passes 7); `NewStreamDecoder` doesn't override |
| `FFmpegMovieImpl.cpp` | engine `void SetPaused(bool)` vs `movie/MovieImpl.h:24 virtual bool SetPaused(bool)` |

**The two "trivially reconcilable" entries were deliberately NOT reconciled**,
and the reasoning is the point:

- **`FFmpegMovieImpl` / `MovieImpl::SetPaused`** — the return type would have to
  change on one side. Changing it **xenon-side is not a native-only edit**:
  `src/system/movie/MovieImpl.h` is a match-relevant header (`movie/` is one of
  the nine PCH-eligible engine dirs in `configure.py`), a virtual's return type
  is part of the vtable contract, and `bool` vs `void` changes the caller's
  codegen. Changing it **engine-side is out of scope** — engine edits are the rb3
  coordinator's to sequence, and DC3 is the engine's reference consumer. The
  step-5 licence required "provably unaffected"; this is provably *affected*, so
  the exclusion stays.
- **`gDataLine`** — a genuine one-word type disagreement, but the honest
  cost/benefit is nil: xenon parses DTA via `DataFlex.c` compiled directly
  (`DTA_LEXER`), so `DataParser_Native.cpp` is **redundant here even if it
  compiled**. Reconciling buys a TU nobody wants. `DataType gDataLine` is also
  the *declared* type in xenon's own header, so the engine is the side that is
  wrong — an engine-backlog item, not a xenon patch.

Net: **5 stays 5.** X0's "effectively 3" was an accurate read of the *diff size*
and an optimistic read of the *blast radius*.

---

## 5. Rnd / NgRnd coupling — the caveat is fully intact

**`rb3-frame` instantiates no `Rnd`, no `NgRnd`, no `WgpuRnd`, and reads not one
xenon rndobj member.** The engine objects it pulls (`GpuDevice.o`,
`Screenshot.o`, and their transitive libc/Dawn deps) reference zero Milo symbols.

This was the explicit mission constraint and it held without compromise — the
"if instantiating `WgpuRnd` is unavoidable, document which members it touches"
fallback never came into play. So the caveat X0 raised —
`src/system/rndobj/Rnd.h:354-360`, the retail X360 `Rnd`/`NgRnd` member-layout
shift versus the DC3 assumption — is **still entirely unmeasured**, exactly as
intended. It is X3's to settle, on top of this baseline.

The value of that discipline is concrete: when the first `WgpuRnd` frame comes
out wrong, the device, the surface format, the headless target, the readback
stride, and the PNG encoder are all already proven, so the search space is the
Rnd coupling alone.

---

## 6. Link-surface census — the top X2 input

`nm` over `libmilo-engine.a`, minus what the archive defines itself, minus
libc/STL/Dawn/glfw/imgui, minus what xenon's current native object graph already
supplies (measured against the linked `rb3-ark`, 11,181 defined symbols):

**84 symbols the dc3 backend wants and xenon's native build does not yet supply.**
They partition cleanly:

| group | ~count | examples |
|---|---|---|
| **`rndobj/` bodies** | ~50 | `NgRnd::{NgRnd,~NgRnd,PreInit,ReInit,DoPostProcess,DrawLargeQuad,SetShadowMap,UpdateOverlay,…}`, `Rnd::{DoWorldBegin,DoWorldEnd,DrawPreClear,DrawString,Handle,ScreenDump,YRatio}`, `RndBitmap::{Create,LoadHeader,PixelColor,AllocateBuffer,…}`, `RndCam::GetViewProjectXfms`, `RndCam::sCurrent`, `RndEnviron::sCurrent`, `RndShaderMgr::*`, `RndTransformable::{SetWorldXfm,WorldXfm_Force,SetDirty_Force}`, `RndPostProc::Current`, `typeinfo for {NgRnd,RndMat,RndMesh,RndTransformable,RndShaderMgr}` |
| **`synth/` bodies** | ~15 | `SampleInst::*` (+ non-virtual thunks), `StreamReceiver::{StreamReceiver,~StreamReceiver,Poll}`, `StandardStream::{InitInfo,ConsumeData}` |
| **`char`/`world`/misc helpers** | ~10 | `LimitAng`, `NormalizeAboutX`, `CreateAndSetMetaMat`, `ShouldSkipMesh`, `HiResScreen::CurrentTileRect`, `TheHiResScreen`, `TheDOFProc`, `DebugPanel::Toggle` |
| **not real gaps** | ~9 | `atan2f`, `dup2` (libc, dynamic), `ImFontAtlas::AddFontDefault`/`ImFontConfig::ImFontConfig` (supplied by the `imgui` target at link), `vtable for __cxxabiv1::*` (libstdc++) |

★ **The finding: every single Milo-shaped gap is a body-not-yet-compiled, not a
missing declaration.** Not one is "xenon has no such concept" — X0 already proved
the *declarations* are all there and DC3-shaped. So **X2 is a
compile-more-TUs problem, not a port-new-code problem**, and its size is bounded
by the `rndobj/` TU count rather than by unknown header work.

Note the second group is self-inflicted and cheaply reversible: the `SampleInst`
/ `StreamReceiver` / `StandardStream` demand exists *because* we did **not**
exclude `SampleInst_Native.cpp` and `StreamReceiver_Native.cpp` (rb3-Wii excludes
both). If audio is not wanted at X2, excluding those two drops ~15 of the 84
immediately.

---

## 7. Recommended X2 shape

1. **Prerequisite — close §3.3 (fix A).** Not optional. X2 compiles *more* game
   TUs, and every one of them is on the `winsockx.h → Symbols*.h` collision path.
   X2 cannot be measured on a tree where 8 targets don't build.
2. **Harden the gate before relying on it** (§1): `-k 0`, and staleness-check the
   binary count. Two-line changes; without them X2's gate reports are as
   trustworthy as X1's starting point was.
3. **Then X2 proper: widen the native fork glob to `src/system/rndobj/`.** The
   census makes this a closed worklist rather than an exploration — burn down the
   ~14 of 86 rndobj TUs that fail `-fsyntax-only` (X0's measurement: 72/86 pass),
   and the ~50 rndobj symbols above resolve as a consequence.
4. **Consider excluding `SampleInst_Native.cpp` + `StreamReceiver_Native.cpp`**
   (exclude list 5 → 7) to drop the synth group, unless audio is wanted early.
   This trades a bigger exclude list for a smaller X2 — worth an explicit call.
5. **Defer the 13 basename collisions until a target links both sides.**
   `native/src/platform/` and the engine's `src/platform/` share 13 basenames:
   `AsyncFile_Native`, `Cache_Stub`, `ChecksumData_Stub`, `File_Native`,
   `Joypad_Stub`, `Keyboard_Stub`, `MapFile_Stub`, `Memory_Native`, `Net_Stub`,
   `NetworkSocket_Stub`, `PlatformMgr_Native`, `ThreadCall_Native`,
   `VirtualKeyboard_Stub`. `rb3-frame` links only the engine, so nothing collides
   *yet* — but the first target that links the engine **and** `NATIVE_SHIMS` needs
   an exclude-or-delete decision on each. This is the "duplicate-definition
   collisions" wall the assessment listed, still unpaid.
6. **X3 keeps the `Rnd`/`NgRnd` layout question**, now debuggable against a
   known-good clear frame.

### Engine-backlog items surfaced (for the rb3 coordinator, not xenon)

- `Mesh_Wgpu.cpp:206,:299` — `GetDrawMode() == 8` vs a 6-max enum; two
  two-sided-cull overrides dead on every consumer (X0's finding, re-confirmed as
  the reason xenon's `-Werror=` set is withheld).
- `DataParser_Native.cpp:72` — `extern int gDataLine` disagrees with the
  declared `DataType` in both consumers' headers; the engine is the wrong side.
- `GpuDevice` prints `device lost (reason 2): Device was destroyed` **before**
  reporting successful init, on every run. Cosmetic, but it reads as a failure
  in logs and will waste someone's afternoon.
