# X2 — a real `.milo_xbox` loads as a live object graph from the mounted ark

**Date:** 2026-08-01
**Predecessors:** [SPIKE-X0](spike-x0-engine-dc3-flavor-2026-08-01.md) "COMPOSES" → [X1](x1-engine-link-2026-08-01.md) "LINKS, RUNS, DRAWS"
**Ladder:** `rb3/docs/native/xenon-bridge-2026-08-01/ASSESSMENT.md` (X0–X4)
**Commits:** `bf03982c`, `6c2187fe`, `0373184a`
**Engine:** `milo-native-engine` @ `2ea8e343…` — **zero engine edits**, and X2 does not even link it

---

## Verdict: **LOADS**

`rb3-milo` mounts RB3's shipped ten-part `main_xbox` archive, runs a named
`.milo_xbox` through the real `DirLoader`, and censuses the object graph it
built. Both requested files pass every gate, `rc=0`, deterministic.

```
=== char/crowd/gen/crowd_female01.milo_xbox ===
  [PASS] header-table — rev 28, dir 'crowd_female01' [Character], 65 entries
  [PASS] no-unmakeable-Dir — every class in the file is constructible
root: 'crowd_female01'  [Character]
--- object census (68 objects, 12 distinct classes) ---
      1  AmbientOcclusion        1  FileMerger          1  MotionBlur
      2  CharClipSet             5  Group               4  Tex
     11  CharCollide             2  Mat                33  Trans
      1  CharDriver              6  Mesh
      1  CharServoBone
  [PASS] header-reconciled — 65 own + 0 skipped = 65, header says 65
                             (+3 from subdirs/merges, counted in the census)

=== ui/track/gen/tracksystem_meshes.milo_xbox ===
  [PASS] header-table — rev 28, dir 'tracksystem_meshes' [RndDir], 131 entries
  [PASS] no-unmakeable-Dir — every class in the file is constructible
--- object census (131 objects, 2 distinct classes) ---
      1  Group                 130  Mesh
  [PASS] header-reconciled — 131 own + 0 skipped = 131, header says 131

=== combined: 199 objects across 2/2 milo(s), 12 distinct classes ===
RESULT: ALL GATES PASSED (0 gate failure(s))
```

★ **An independent oracle agrees.** DC3's engine loaded these same two RB3-360
files in a separate experiment (`docs/plans/engine-reuse-and-asset-rendering.md`)
and reported *"130 meshes"* for `tracksystem_meshes` and a `Character` with
*"6 meshes / 2 materials / 4 textures"* for `crowd_female01`. X2's numbers are
130 Mesh + 1 Group, and 6 Mesh / 2 Mat / 4 Tex. Two unrelated engines, one asset
set, the same answer — which is worth more than any self-consistency check this
harness could run on itself.

**LOAD ONLY, as chartered.** `rb3-milo` does not link `libmilo-engine.a` and
instantiates no `Rnd` / `NgRnd` / `WgpuRnd`, so the member-layout caveat at
`src/system/rndobj/Rnd.h:354-360` is exactly as unmeasured as X1 left it. §7 is
where it comes due.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | rb3-frame + the X1-restored targets still build; `tools/native_build_gate.sh` passes with zero regressions | ✅ **PASS — and better than the charter asked** | `17/17 targets verified, 0 errors, 0 warnings`. The charter said "rb3-frame + the 7 X1-restored targets"; the tree now links **all 17**, because a concurrent agent closed the `Symbols*.h` × POSIX blocker in `7ca1ef8a` while X2 was in flight. `rb3-frame` reproduces X1's exact PNG, `sha256 3371f9e0…`; `rb3-ark` still passes all its gates on the real archive. |
| b | rb3-milo census deterministic across 2 runs, both milos, exit 0 | ✅ **PASS** | Two consecutive runs byte-identical, `sha256 42594008…`; with `--verbose` (every object name, not just counts) also byte-identical, `sha256 943004fd…`. `rc=0` both times. |
| c | Zero `milo-native-engine` source edits | ✅ **PASS** | The engine tree was never written to. X2 goes further than X1 and does not even *link* it — the one thing taken from it is the header-only `platform/NativeSettings.h` (`NativeSettings::Get()` is inline; no `.cpp` exists) that `rndobj/Cam.cpp` includes under `#ifdef HX_NATIVE`. The `FxSendNative.cpp` dirt from another agent is untouched. |
| d | X360 match build unaffected | ✅ **PASS by construction** — see §5 for the per-file argument | Every shared-`src/` hunk is `#ifdef HX_NATIVE`, provably codegen-identical, or already excluded when `HX_NATIVE` is undefined. The match build passes **no `/D` at all** (`CLAUDE.md`), so `HX_NATIVE` is never defined there and the preprocessed token stream is unchanged. |

⚠ **Gate (d) was argued, not executed.** The full X360 A/B was not run; the
claim rests on the preprocessor argument in §5, which is checkable line by line
but is not a measurement. The two hunks that would repay an actual A/B are named
explicitly in §5 (`Character.h`'s new enumerator and `BandWardrobe`'s `NodeCmp`),
and both are argued to be no-ops on that side.

---

## 2. TU-surface delta

| dir | in the target's source list | of which compile standalone | rest emitted via scatter-includes | excluded |
|---|---|---|---|---|
| `rndobj/` | 84 / 86 | 68 | 16 | MeshAnim, MeshDeform |
| `char/` | 60 / 60 | 55 | 5 | — |
| `world/` | 23 / 24 | 17 | 6 | BeatClock |
| `ui/` | 38 / 38 | 31 | 7 | — |
| **fork total** | **203** (was **1**) | 171 | 32 | **3** |
| `midi/` | 6 (reused `M2_SOURCES`) | 5 | 1 | — |
| `synth/` | 2 leaves only | 2 | — | — |

The two columns matter separately. "In the source list" is the honest measure of
*what X2 made compile*; "compiles standalone" is lower because
`cmake/ScatterIncludes.cmake` correctly drops any TU that another TU in the same
target `#include`s wholesale.

### The exclusion list went from 8 to 3, and the reason is the real finding

The first measurement said rndobj was 13/86 broken. It was not 13 problems.

| root cause | TUs it took down |
|---|---|
| `BandCharacter::TextureCompressed(int)` cannot override an `intptr_t` pure virtual under LP64 → `BandCharacter` stays abstract → `NEW_OBJ` ill-formed | `Console`, `Font`, `Crowd` (three different scatter chains) |
| missing `-I src/band3` include root | `CubeTex`, and 4 "file not found" reports elsewhere |
| `char/CharDriver.cpp`'s `stlpmtx_std` block had **inverted `#ifdef` polarity** (dc3 scaffold `c5c1650f`) | `AmbientOcclusion`, `PropKeys`, `TexBlender`, `Utl` |
| `bandtrack/GemManager.cpp`'s unguarded sw3 force-emit block | `CharBonesMeshes` |

**Counting failing TUs before finding root causes overstates the work by ~3×.**
13 rndobj failures were 6 defects; 5 of the original 8 exclusions dissolved.

The three that remain:

| TU | why, and whose it is |
|---|---|
| `rndobj/MeshAnim.cpp` | scatter-includes `synth_xbox/Voice.cpp` (XAUDIO2 + `CreateThread`) **and** `band3/game/NetGameMsgs.cpp`, which sits on the `Symbols*.h` × POSIX path — the one thing X2 was told not to touch. Now that `7ca1ef8a` has closed that collision, this is probably cheap; not re-measured here. |
| `rndobj/MeshDeform.cpp` | same collision via `band3/meta_band/BandUI.cpp`, plus an `InterstitialMgr::mRandomOverride` member gap. |
| `world/BeatClock.cpp` | xenon's `SongPos` has no `Phrase` field; `BeatClock.cpp:31,:147,:153` read `AccessPhrase()`/`GetPhrase()` unconditionally. A genuine header-shape divergence against the beatmatch `SongPos`, **not** a native-only break — it wants a `SongPos` audit, not a guard. |

---

## 3. Why the pass criterion is not "it didn't crash"

A `.milo` load fails in ways that look like success, so `exit 0` and even "it
printed a lot of objects" are both worthless as criteria. `main_milo.cpp` reads
the file's object table **twice, by independent routes**, and reconciles them:

- **(A)** the raw `ObjectDir` header straight off the `ChunkStream` — rev, dir
  class, dir name, then N × {className, objName}. Needs **zero** factories, so
  it is ground truth regardless of how much of the fork compiles.
- **(B)** the live graph `DirLoader` actually built, walked with `ObjDirItr`.

Gate: `live-own + factory-skipped == header count`. A stream that desynced
halfway is short and fails the arithmetic even though nothing segfaulted.

Two counts are kept because the header table lists only the **root dir's own**
entries. `crowd_female01` legitimately ends up with 68 objects against a 65-entry
header — the extra 3 arrive from a subdir / `FileMerger` merge. Reconciling
against the recursive total would have failed, and that failure would have been a
bug in the check rather than in the load.

**Unmakeable classes are not all equal, and the driver distinguishes them.** An
unregistered *leaf* is bounded: `DirLoader` logs `Can't make <Class>` and
`ReadDead`-skips its bytes to the next `0xADDEADDE` marker. An unregistered
`*Dir` **subclass** is not: it serializes a nested directory whose inner objects
carry their own dead markers, so `ReadDead` stops at the first inner one and the
parent desyncs. So a `*Dir` gap is a hard FAIL that refuses to run the load at
all, while a leaf gap is counted and reported. (Both files came out clean: every
class in both headers was constructible.)

---

## 4. Bring-up order — four things that are not optional

Each was found by a crash, and each is a prerequisite the next milestone
inherits.

1. **`FileInit()`** — sets `gRoot` and `FilePath::Root()` to `"."`
   (`os/File.cpp:390-398`). Without it both are empty, so
   `FilePath("char/…/x.milo_xbox")` becomes `FileMakePath("", …)` = `/char/…`,
   an **absolute** path, which `FileIsLocal()` routes to the host filesystem
   instead of the ark. `DirLoader` then reports "Could not load" for a file
   `rb3-ark` reads byte-exactly. `main_ark.cpp` escapes this only because it
   calls `NewFile()` with a raw string and never builds a `FilePath`.
2. **`DataInit()`** — registers the data DataFuncs and ends by calling
   `ObjectDir::PreInit(19997, 150000)`, which creates `ObjectDir::Main()`. Not
   decoration: the shipped config DTBs execute script that resolves objects
   through it (`DataArray::Execute → ObjectDir::FindObject`), and with a null
   main dir, *reading `config/objects.dta`* segfaults inside the DTB's own
   script. It also flips `DirLoader::SetCacheMode(true)` under `UsingCD()`.
3. **A system config.** `DirLoader`'s constructor dereferences `SystemConfig()`
   unconditionally (`obj/DirLoader.cpp:68`), so a null `gSystemConfig` crashes
   before the first file is opened. X2 assembles `(objects …)` from
   `config/objects.dta` + `config/rnd_objects.dta` (76 class blocks) rather than
   calling `PreInitSystem`/`SystemInit` — those also do `DataSetMacro`,
   `OptionStr` parsing and `SetGfxMode(kNewGfx)`, i.e. they start standing the
   **renderer** up, which is the one thing X2 must not do. `RB3_SYSCFG` (a
   comma-separated list) overrides it.
4. **`CharBoneDir::Init()`** — sets `sCharClipTypes` and creates the
   `char_resources` dir. Without it `CharServoBone::Load → SetClipType →
   CharBoneDir::StuffBones` dereferences null. It also loads the shipped
   bone-resource milos, which is real content the rig needs.

---

## 5. Shared-`src/` changes and their X360-neutrality

24 files. Every hunk is one of: inside `#ifdef HX_NATIVE`; already excluded when
`HX_NATIVE` is undefined (so the preprocessed token stream is byte-identical);
or a name-binding change with identical codegen. The match build passes **no
`/D`**, so `HX_NATIVE` is never defined there.

### 5.1 Two defects that were silently fatal on the native side

**⛔ `obj/DirLoader.cpp` — `LoadHeader` spun forever on a missing or truncated
milo.** The loop's exit test is `Eof() == NotEof`, and the only thing between it
and an infinite spin is a `MILO_ASSERT` — a real abort on X360, deliberately
**non-fatal** natively (`os/Debug.cpp:183`, matching the Xbox "Continue"
dialog). On a stream already at `RealEof` the assert fires, does not abort,
`CheckSplit` stays false, and the loop re-reads `RealEof` forever. Measured, not
theorised: one of `CharBoneDir::Init`'s resource milos did not resolve and this
produced **~3×10⁸ stderr lines in about ten minutes** before it was killed. It
presents as a hang, which is the worst possible way to report a missing file.
Now `RealEof` takes the same `Cleanup()` path `OpenFile`'s `Fail()` branch does.

**`char/Character.h` — character shadows were dead code.** `Character::Draw`
builds `(DrawMode)4` explicitly and `DrawLodOrShadow:928` tests `drawMode == 4`
to dispatch `mShadow->DrawShowing()` — but `4` was not an enumerator
(`kCharDrawNone..kCharDrawAll` stop at 3), so clang's range analysis folds the
test to a constant **false**. Added `kCharDrawShadow = 4`. An added enumerator
emits no code and the underlying type is `int` at 0..3 and 0..4 alike, so X360
is unaffected — MSVC does not narrow this way, which is why retail is correct
and only the port was broken. **This one lands directly on X3's render path.**

### 5.2 Load-path bodies that the dc3 scaffold had guarded off

`rndobj/Tex.cpp` (`RndTex::Load/PreLoad/PostLoad`) and `rndobj/Bitmap.cpp`
(`RndBitmap::Load`) were `#ifndef HX_NATIVE` because DC3's consumer gets them
from the engine (`RndTex_Native.cpp`). Without the engine, that is not "a graph
with no textures" — it is a **desynced stream**, because those functions are what
consume the texture bytes. The guards are dropped and xenon's own *matched
retail* bodies run, which is the more faithful of the two answers anyway. Only
the GPU-upload leaves (`SyncBitmap`/`PresyncBitmap`/`MakeDrawTarget`/
`FinishDrawTarget`, all in `rnddx9/`) remain stubbed.

### 5.3 Two ODR violations the X360 pipeline cannot see

The match build compiles TUs and byte-compares objects; **it never links**. Two
external-linkage functions are defined twice in this tree, and only the native
build can notice:

| symbol | sites | resolution |
|---|---|---|
| `NodeCmp(const void*, const void*)` | `obj/DataArray.cpp:431` and `bandobj/BandWardrobe.cpp:1329`, with **different bodies** (generic vs ".tp"-first) | `static` natively — **not deleted**, because `BandWardrobe::OnSortTargets` genuinely wants its own, and deleting it would silently hand that qsort DataArray's comparator. ⚠ **On X360 both are still external, so one of the two qsorts is calling the wrong comparator.** Match A/B owed. |
| `LimitAng(float)` | `char/CharClip.cpp:19` and `char/CharForeTwist.cpp:8`, identical bodies | CharClip's copy goes inert natively (CharForeTwist is always compiled standalone). |

### 5.4 `os/System.cpp` — `SystemConfig` degrades instead of dereferencing null

A missing config section is fatal-by-design on X360: `FindArray`'s `fail=true`
path `MILO_FAIL`s and returns null and the caller derefs it — fine there, because
the shipped merged config always has what shipped code asks for. A headless
harness has no such guarantee: it assembles a config from the subset of DTBs it
can read without standing up App/UI/renderer, and `config/objects.dta` ships 74
class blocks with **none for `CharClipGroup`**, which `obj/Utl.cpp:82 ClassExt()`
asks for during a `CharClipSet` load. Natively the lookup now returns a shared
empty section; **the `MILO_NOTIFY` still names every miss**, so nothing is
silently invented — only the crash is removed. Entirely inside `#ifdef HX_NATIVE`.

### 5.5 The other hunks

`bf03982c` (8 latent LP64/HX_NATIVE breaks: `obj/Object.h`'s `this->`
qualification against the `extern Symbol size/back/front/begin` globals;
`world/CameraShot.cpp`'s `mCamShot->WorldXfm()` on a class that has none;
`char/CharDriver.cpp`'s inverted `stlpmtx_std` polarity; `char/CharBoneDir.cpp`'s
`MiloStripEval`; `rndobj/Text.cpp`'s ILP32 `sizeof` gates; the `EnvAnim`/
`PartAnim` `ObjRefConcrete` specializations; `world/Instance.cpp`'s 2-arg
`Replace`) is documented in full in its own commit message.

---

## 6. Collision & duplicate-definition resolutions

### 6.1 The 13 `native/src/platform/` ↔ engine `src/platform/` basename collisions

**Still not due, and deliberately so.** They come due the moment one target links
both the engine and the fork shims. X2 links the engine into **no** target other
than `rb3-frame` (which links `libmilo-engine.a` and nothing else), so nothing
collides. Paying that debt inside a load-only milestone would have meant putting
`WgpuRnd`/`NgRnd` member offsets on the critical path of a result whose whole
value is being interpretable without them. **It is X3's first task**, and §7 says
what X3 gets in exchange.

### 6.2 The duplicate-definition collisions X2 *did* pay

A different set, and one X1 did not predict: widening the glob turned the
decomp's ~250 scatter-includes into duplicate definitions.

| includee | includers | resolution |
|---|---|---|
| `obj/PropSync.cpp` | `Group`, `CharBlendBone`, `CharIKRod`, `Character`, `CharIKFingers` (**5**) | guard all five → compiles standalone |
| `rndobj/Anim.cpp` | `ShaderOptions`, `MatAnim` | guard both → compiles standalone |
| `bandobj/BandDirector.cpp` | `Font`, `UIList` | guard `UIList`'s (609 duplicate symbols, one root cause) |
| `band3/game/Stats.cpp` | `EventTrigger`, `LightPreset` | guard `LightPreset`'s |
| `gesture/SkeletonClip.cpp` | `EventTrigger`, `CharLipSync` | guard `CharLipSync`'s |
| `flow/FlowNode.cpp` | `Font`→`BandDirector`, `FileMerger`→`Morph` | guard `Morph`'s |
| `synth/Synth.cpp` | `CharMeshHide`→`Sfx`, `CheatProvider` | guard `Sfx`'s |
| `world/CameraShot.cpp` | `Sfx`, `LightPreset` chain | guard `Sfx`'s |
| `char/CharClipGroup.cpp`, `char/CharBoneTwist.cpp` | one includer each, but `ScatterIncludes.cmake` classified the edge as *conditional* (it warns at configure time) and declined to prune | explicit `#ifndef HX_NATIVE`, the shape that module documents as always correct |

**A gap in `ScatterIncludes.cmake`, recorded not fixed.** The module scans a
target's *own* sources for `#include "*.cpp"` edges and drops any includee that
is also a source. It does **not** follow edges out of a file that is not itself a
target source. `char/CharClip.cpp` and `char/Character.cpp` are reached via
`rndobj/Font.cpp → bandobj/BandDirector.cpp → …`, and `BandDirector.cpp` is not
compiled here, so the module never sees the second hop — ~30 duplicates. They are
listed in `_MILO_SCATTER_TRANSITIVE_PRUNE` rather than fixed in the module,
because making it transitive changes pruning for all 16 pre-existing targets and
X2 has no way to validate that. **X3 item.**

### 6.3 X1's two delete-me stubs came due exactly as predicted

`native_undecomp_stubs.cpp`'s `CacheResource` and `CacheWav` (and
`SongInfoCopy::GetTracks`) collided with the now-compiled real bodies. Made
`__attribute__((weak))` rather than `#ifdef`'d on a per-target macro: the strong
definition wins wherever the real TU is compiled, every other target keeps the
stub, and the next target to widen its glob needs no coordination.

---

## 7. Remaining stub list, and the X3 mapping

`native/src/milo_link_stubs.cpp` has three sections. Only one is owed anything.

**(1) Real implementations — not stubs, nothing owed.**
`WorldInstance::Load` (declared at `world/Instance.h:27`, defined nowhere; the
`BEGIN_LOADS` macro expands to exactly `PreLoad; PostLoad`, so this is the
implementation, not an approximation — and it is on the venue load path);
`gCharHighlightY` (declared `extern` in `char/Char.h:10`, defined nowhere; `-1`
is the correct sentinel, a zero-init would read as "highlight at y=0");
`operator<<(BinStream&, const ObjOwnerPtr<T>&)` (declared in `obj/Object.h:755`,
commented out in `ObjPtr_p.h:394` — **and commented out in DC3's copy of the same
header too**, so it is a shared undecompiled function; the body follows the
convention every sibling container serializer in that header uses).

**(2) GPU / render backend — 26 symbols, all `src/system/rnddx9/`.** X2 loads and
does not draw, so a no-op is the *correct* behaviour here, not a placeholder for
it. ★ **X3 deletes this entire section by linking `libmilo-engine.a`**, which
already supplies every one:

| stub group | engine TU that replaces it |
|---|---|
| `TheRenderState` + 17 `RndRenderState::Set*` | `src/platform/RenderState_Native.cpp` |
| `TheNgRnd`, `TheShaderMgr` | `src/platform/Rnd_Wgpu.cpp` (`gWgpuRndInstance`) |
| `RndTex::{SyncBitmap,PresyncBitmap,MakeDrawTarget,FinishDrawTarget}` | `src/platform/RndTex_Native.cpp` + `Tex_Wgpu.cpp` |
| `RndMesh::{DrawShowing,OnSync}`, `CleanupGpuMesh` | `src/platform/Mesh_Wgpu.cpp`, `MeshGpuCache.cpp` |
| `DrawParticlesBillboard` | `src/platform/Part_Wgpu.cpp` |
| `FlushPostProcessingForOverlay`, `FlushTransparentDraws`, `SpotlightDrawer::DeSelect`, `RndFont::CellDiff` | X3 leaves |

⚠ `TheNgRnd` and `TheShaderMgr` are bound to **null references** on purpose.
Both are declared as references, so the link demands a definition; binding them
to null makes any use fault at address 0 with a stack trace naming the caller,
which is what should happen if the load path ever reaches the renderer. Pointing
them at a zeroed buffer would let the caller wander on through garbage members.

**(3) Off-path singletons** — `TheUI`, `TheHamDirector`, `TheHamWardrobe`,
`HamWardrobe::ForceCrowdAnimation*`, and 7 handler-only `Symbol` globals.

⚠ **The hazard this file creates, stated plainly.** A stub that is actually
*reached* silently replaces real behaviour with nothing — this repo has already
measured that (a no-op `CrowdRating::Poll` running 25,905 times in one
`rb3-score4` run while the run reported its numbers as real, lane CC-5). Section
(2) **is** expected to be reached; sections (1) and (3) are not, and a hit there
is a bug. The instrument exists: `-DRB3_STUB_PROBE=ON`, `native/src/cc5_stub_probe.c`.

### `-Werror` opt-in exemptions — four real findings, none "fixed" in passing

The `DECOMP_WARNINGS` policy was measured zero across the 251 TUs *in the build
at the time*; X2 adds 162 that measurement never covered, and four trip an
opt-in. That is the policy working. Three are inside **matched** bodies, where
"fixing" is a match A/B with its own objdiff evidence, so they are exempted **per
TU** with the verdict recorded and the policy left fully armed on the other 158.

| site | finding | verdict |
|---|---|---|
| `rndobj/MultiMesh.cpp:363` | `proxy = proxy;` is a harmless no-op — **but** the loop above assigns on every iteration and only breaks on a match, so after a full non-matching scan `proxy` holds the *last* pool entry rather than null and the `if (!proxy)` does not fire. Separate, real logic defect; EditMode-only. | exempt + report |
| `rndobj/Mat_NG.cpp:145` | `cur == 6` against `RndRenderState::ClampMode`, which `rnddx9/RenderState.h:16` declares as **`enum ClampMode {}`** — empty. Every comparison and cast in the tree is trivially out of range. **Not** a real dead branch; a stub-enum artifact in a directory the port replaces. | exempt |
| `rndobj/Text.cpp:245` | `int fixedLength;` assigned only when `rev > 0xB`, read unconditionally three lines later. For `rev ∈ (8, 0xB]` `RndText::Load` stores garbage. Plausibly **faithful**. | exempt + oracle triage owed |
| `rndobj/Font.cpp:341-355` | `float w, h;` **shadowed** by `int w, h;` inside a nested `if`; only the inner pair is written, so on the `rev == 2/3` path the outer pair is read uninitialized to divide bitmap dimensions. Reads like a transcription artifact. | exempt + triage owed |
| `char/CharClipDisplay.cpp:201` | `RndTransformable *data` declared **twice**, and `goto drawIKData` jumps into the second scope past its initializer. On the goto path `data` is an uninitialized pointer read. | exempt + triage owed |
| `char/Character.cpp:928` | the shadow-dispatch enum gap | **FIXED** (§5.1) — provably codegen-neutral, so it needed no exemption |

---

## 8. Recommended X3 shape

1. **Link `libmilo-engine.a` into a render target and pay the 13 basename
   collisions.** The engine is the intended supplier of everything in stub
   section (2) — the table in §7 *is* the worklist, and it is closed, not
   exploratory.
2. **The first place the `Rnd`/`NgRnd` layout shift will bite is not where X0
   expected.** ABI is not the risk: the engine is compiled with **xenon's own
   include dirs** (`MILO_ENGINE_DECOMP_INCLUDE_DIRS`), so both sides see the same
   `rndobj/Rnd.h` and member offsets agree by construction. The risk is
   **semantic** — the DC3 backend reading a xenon member that exists but means
   something else. Concretely: linking the engine pulls `Rnd_Wgpu.cpp`, whose
   `TheRnd`/`TheNgRnd` are references to a **statically-constructed**
   `gWgpuRndInstance`. That runs `NgRnd`'s constructor *before `main`*, on a
   xenon-shaped object, with no chance to instrument it. **Stand `WgpuRnd` up
   lazily, or first, and on its own** — do not let it arrive as a side effect of
   resolving `TheNgRnd`.
3. **Take `char/Character.h`'s `kCharDrawShadow` seriously as the template.** It
   is one enumerator, it was silently deleting the character shadow pass, and it
   was found by a `-Werror` opt-in on a directory that had simply never been
   compiled. There are 158 newly-compiled TUs whose *warnings* are clean but
   whose behaviour has never been executed; the render pass is where that starts
   to matter.
4. **Two cheap unblocks now that `7ca1ef8a` has landed:** `rndobj/MeshAnim.cpp`
   and `rndobj/MeshDeform.cpp` were excluded *only* for the `Symbols*.h` × POSIX
   collision, which is now fixed. Re-measure — the exclusion list may go 3 → 1.
5. **Backlog, unchanged and still owed to their owners:** the transitive gap in
   `ScatterIncludes.cmake` (§6.2); the X360 `NodeCmp` comparator A/B (§5.3); the
   three faithful-vs-transcription triages in §7; and X1's engine items
   (`Mesh_Wgpu.cpp`'s `GetDrawMode() == 8`, `gDataLine`'s type, `GpuDevice`'s
   spurious "device lost" line).

---

## 9. Reproduce

```bash
cd rb3-xenon/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target rb3-milo

./build/rb3-milo ../../rb3/orig-assets/xbox-zip \
    char/crowd/gen/crowd_female01.milo_xbox \
    ui/track/gen/tracksystem_meshes.milo_xbox
# add --verbose for every object name; RB3_SYSCFG=<comma-list> to swap configs
```

Full gate: `tools/native_build_gate.sh` (expects `PASS, 17/17`).
