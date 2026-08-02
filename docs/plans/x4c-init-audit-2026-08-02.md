# X4c — the init audit, and why `kNewGfx` was never the thing that emptied the frame

**Date:** 2026-08-02
**Predecessor:** [X4b](x4b-animation-2026-08-02.md) "POSED, qualified"
**Branch:** `x4c-init-audit`, rebased onto `main`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**

---

## Verdict

**Milestone 1 — LANDED.** All ~23 `PreInitSystem`/`SystemInit` sub-inits audited
against the linked binary (not just grep), boot-time invariant checks added and
**validated on a known-bad control**, and the systemic question answered: the
reason 18 drivers have skipped `PreInitSystem` for four milestones **is false**.
Two new Tier-1 omissions found (`ThreadCallInit`, `ObjectDir::Init`); the two the
charter named as most-suspect (`GeoInit`, `TrigInit`) are both **innocent**.

**Milestone 2 — LANDED, and the headline is a retraction.** `SetGfxMode(kNewGfx)`
is now the default and the posed character is **visibly correct**: coherent
limbs, raised reaching arms, and the arm spikes of X4b's posed PNG are **gone**.

★ **But `kNewGfx` was never what emptied the frame.** The empty posed frame was a
**separate, pre-existing, gfx-mode-independent defect**: the clip's big-endian
byte-swap used a 4-byte float width on a section that is **three shorts**, so the
root bone's Z was overwritten with an unrelated quaternion component. Restoring
the bones did not break the pose — it *exposed* an already-broken pose, by
letting every vertex follow the garbage root instead of only some.

★ **The honest consequence for X4b: its posed character was mostly BIND-POSE
geometry**, and its pose was wrong the whole time. With `MaxBones()==4`, vertices
weighted to bones 4..19 index palette slots the engine fills with identity, so
the majority of that figure was not posed at all. The "smear" was the minority
that *did* follow the bad pose.

**Milestone 3 — the coordinator's experiment landed; the venue did not.**
Per-path split is **675 top-level / 0 persistent** — no persistent-object wall.
The venue root still SIGSEGVs, so the misses were never the only defect.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native gate, **fresh** (`rm -rf native/build` first), rc=0 | ✅ **PASS — 18/18** | `tools/native_build_gate.sh` → `PASS (rc=0, 0 errors, 0 warnings, 18/18)`, every target `relinked this run`. ⚠ The gate's own `cmake` line does **not** pass `MILO_ENGINE_PATH`/`Dawn_DIR`; it relies on the CMake cache. In a worktree with no `build/` the cache is empty and **3 targets silently SKIP while the gate still reports `PASS rc=0`** (measured: 15/18). Seed the cache with an explicit `cmake -S . -B build …` first. |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606…`, untouched. Engine requests filed as text in §6. |
| c | Shared-`src/` edits `HX_NATIVE`-gated, X360 arm token-for-token | ✅ **PASS — 4 files, VERIFIED BY REBUILD** | `obj/DirLoader.cpp`, `world/Instance.cpp`, `char/CharBonesSamples.{cpp,h}`. Not asserted from inspection — the three X360 `.obj`s were rebuilt and byte-compared. They differ in **3, 9 and 5 bytes**; a no-change rebuild control differs in **4** (offsets 5-8, an embedded timestamp). Every remaining differing byte is inside the **absolute build path string** baked into the object (`laneX4A`/`laneDD3` → `laneX4C`, all 7 chars, hence identical file sizes). **Zero codegen difference.** The `CharBonesSamples` fix is a native-port LE defect (rb3-Wii is big-endian and has no swap code at all), not a decomp divergence — **no objdiff A/B debt**. |
| d | PNG determinism ×2 | ✅ **PASS** | posed cell `sha256 e6c880fbe17185bf…` identical ×2; X3 cells identical ×2 |
| e | X3/X4a evidence PNGs | ✅ **PASS — 3 of 4 byte-identical, 1 root-caused** | `tracksystem_meshes cbdb29fa95a5b574…`, X4a `d9624b900a1b0699…` / `d30f600d8ea3bcfe…` — **byte-identical**. X3's crowd **bind** cell `30692a8d02c1ada0…` → `a2a69cee7094f152…`: **changed, intended, root-caused** — it is the bone restoration (coverage 11.07% → 15.78%, the extra coverage being exactly the vertices the truncation had pinned at bind). |

### Reproduce

```bash
cd rb3-xenon/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine \
      -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build --target rb3-render

ASSETS=/home/free/code/milohax/rb3/orig-assets/xbox-zip

# default is now kNewGfx. RB3_GFX_MODE=old restores the truncated control.
# NOTE: write the flags out in full. Do NOT stash them in a shell variable and
# expand it unquoted -- zsh does not word-split unquoted expansions, so the whole
# string arrives as ONE argv entry and the run silently renders BIND POSE. That
# mistake produced two wrong measurements in this milestone (§7.5).
./build/rb3-render $ASSETS out --frames 1 \
    --clips char/crowd/anim/gen/female_base.milo_xbox \
    --clip crowd_reaching_01 --beat 4.0 --bone-audit \
    char/crowd/gen/crowd_female01.milo_xbox

RB3_GFX_MODE=old ./build/rb3-render $ASSETS out_old --frames 1 \
    --clips char/crowd/anim/gen/female_base.milo_xbox \
    --clip crowd_reaching_01 --beat 4.0 char/crowd/gen/crowd_female01.milo_xbox
```

⚠ In a worktree `MILO_ENGINE_PATH` and `Dawn_DIR` **must** be passed explicitly
(X4a §1: both default relative to the source tree and the engine silently
auto-disables at `rc=0` when they miss).

⚠ **The venue root path in X4a's prose is wrong.** `world/venue/small_club/small_club_01`
is *not in the archive index*. The real path is
`world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox`.

---

## 2. Milestone 1 — the sub-init audit

Method: source grep over all of `native/`, **plus linker ground truth**. The
native targets build with `-ffunction-sections` + `--gc-sections`, so a function
that is defined in a linked TU but **absent from `nm` on the binary** is
proof-by-construction that nothing references it. Controls confirm absence means
"never called" and not "TU not linked": `CheckBSPTree` (same TU as `GeoInit`)
survives, `ObjectDir::PreInit` (same TU as `ObjectDir::Init`) survives.

**Headline: `PreInitSystem` and `SystemInit` are both absent from the binary
entirely.** Not one of the 18 drivers calls either.

### 2.1 Verdict table

`main_render`'s prologue is `main_render.cpp:1481-1503` / `:1513-1526`.

| sub-init | impl | in `main_render`? | in ANY driver? | what it silently corrupts if skipped |
|---|---|---|---|---|
| `DataSetMacro HX_XBOX/HX_WIN/HX_NG` | `System.cpp:480` | ❌ | ❌ | DTA `#ifdef` branches take the wrong arm |
| `OptionStr("define")` loop | `System.cpp:483` | ❌ | ❌ | inert with no command line |
| `BeginDataRead`/`FinishDataRead` | `DataFile.cpp:487/492` | ~ implicit | ❌ | **inert** — `DataReadFile` self-brackets; costs re-parsing, not correctness |
| `ReadSystemConfig` | `System.cpp:223` | ✅ hand-rolled | milo+render | — (`main_render.cpp:233`) |
| `DataVariable("syscfg")` | `System.cpp:495` | ✅ | milo+render | — |
| `DataRegisterFunc` ×7 | `System.cpp:498-504` | ❌ | ❌ | **loud** — `MILO_FAIL("Couldn't bind %s")` at `DataNode.cpp:731` |
| **`SetGfxMode(kNewGfx)`** | `System.cpp:505` | ❌ | ❌ | ⛔ **`MaxBones()` 40→4; every skinned mesh truncated at load.** §3 |
| `gSystemTimer.Start()` | `System.cpp:516` | ❌ | ❌ | one garbage delta then self-corrects (`Timer.cpp:76` guards on `mRunning>0`) |
| `Symbol::Init()` | `Symbol.cpp:67` | ✅ | **all 17** | — |
| `InitSystem(config)` | `System.cpp:450` | ❌ | ❌ | hand-reconstructed at `main_render.cpp:242-289` |
| `StripEditorData` | `System.cpp:232` | ❌ | ❌ | **inert** — leaves *more* data, never less |
| `gSystemTitles` | `System.cpp:520` | ❌ | ❌ | only reader is `StoreOffer.cpp:205`, out of scope |
| **`ObjectDir::Init()`** | `Dir.cpp:917` | ❌ | ❌ | ⛔ **`"milo"`/`"milo_xbox"` loader factories never registered.** `LoadMgr::AddLoader` (`Loader.cpp:412-424`) falls through to `return new FileLoader(...)` on no match — **no warning, no null** — so any `.milo*` routed through it silently becomes an opaque byte reader. Top-level loads escape (they construct `DirLoader` directly), but `DirLoader::LoadResources`, `ContentMgr.cpp:200`, `Movie.cpp:79`, `CubeTex.cpp:155`, `UIPicture.cpp:181`, `PatchDir.cpp:45` all go through `AddLoader`. |
| **`ThreadCallInit()`** | `ThreadCall_Native.cpp:65` | ❌ | ❌ | ⛔ **silent infinite hang.** `ThreadCall()` IS linked and reachable; `ThreadCallInit` and `ThreadCallPoll` are both GC'd. `DataLoader::LoadFile` (`DataFile.cpp:717-736`) enqueues then spins `while (mThreadObj) Timer::Sleep(0)`, and `mThreadObj` is only cleared from `ThreadCallPoll`. No worker, no poll → spin forever, no diagnostic. Also `ThreadCallPreInit` never runs, so `gMainThreadID` stays `-1`, which `OSFuncs.h:9` treats as "thread checks disabled" — **every `MainThread()` assertion in the engine is silently a no-op.** |
| `GeoInit()` | `Geo.cpp:187` | ❌ | ❌ | **cleared — not a defect.** §2.2 |
| `TrigInit()` | `Trig.cpp:129` | ❌ | ❌ | **cleared — not a defect.** §2.2 |
| `TrigTableInit()` | `Trig.cpp:13` | ✅ (auto-ctor) | — | fixed in X4b; `nm` confirms present |
| `SpewInit()` | `Spew.cpp:3` | ❌ | ❌ | **body is `{}`** — provably inert |
| `FileCache::Init()` | `FileCache.cpp:180` | ❌ | ❌ | **body is `{}`** — provably inert |
| `TheLocale.Init()` | `Locale.cpp:150` | ❌ | ❌ | `mSymTable` null; `Locale::Localize` is **reachable in the binary** while `Locale::Init` is GC'd — a live path to an object that can never be valid. Loud (`MILO_ASSERT` `Locale.cpp:74`). |
| `CheatsInit()` | `Cheats.cpp:404` | ❌ | ❌ | headless-irrelevant; skipping is arguably *correct* (Joypad/Keyboard are stubs) |
| `TheContentMgr.Init()` | `ContentMgr.cpp:29` | ❌ | ❌ | **must NOT be added** — `TheContentMgr` is a weak zeroed 128-byte blob (`dta_link_stubs.s:380`); calling `Init()` on it is a null-vtable call |
| `TheDebug.AddExitCallback` | `System.cpp:532` | ❌ | ❌ | deliberate — drivers `_exit()` (`main_render.cpp:1561`) |
| *(off-list, same class)* `LoadMgr::Init()` | `Loader.cpp:493` | ❌ | ❌ | `DataVariable("sysplatform")` and `"edit_mode"` never set. `mPlatform` itself is safe — the ctor seeds `kPlatformXBox` (`Loader.cpp:246`). |

**Score: ~23 audited, 19 skipped, 2 had already bitten, 2 are new Tier-1 finds
(`ThreadCallInit`, `ObjectDir::Init`), 5 are provably inert.**

### 2.2 ⛔ RETRACTED before it cost anything: `GeoInit`/`TrigInit` are NOT a third instance

These were the charter's named highest-suspicion pair. Both are cleared, and the
reasons are worth recording because they are the *opposite* of the defect shape:

- **`GeoInit`** fills five file-static BSP tuning globals — but they are **not
  zero-initialised**. `Geo.cpp:23-27` gives them sane in-source defaults
  (`gBSPPosTol = 0.01f`, `gBSPCheckScale = 1.1f`, …). Skipping it yields
  *plausible* behaviour, not degenerate behaviour, and all five are
  collision/raycast tuning — **none is on the draw path**. Note `CheckBSPTree`
  does `if (!gBSPCheckScale) return true;`, i.e. a zero would have *disabled*
  the check — and it isn't zero. Fidelity gap, not a defect.
- **`TrigInit`** fills **nothing**. It is six `DataRegisterFunc` calls (`sin cos
  tan asin acos atan`) and nothing else. It shares only a *filename* with
  `TrigTableInit`, the sole owner of `gBigSinTable`. **The name collision is the
  entire reason it looked dangerous.**

The genuine third instance is **`ThreadCallInit`** — same silence, worse failure
mode (a hang rather than a wrong number).

### 2.3 ★ The systemic fix: the stated reason for skipping `PreInitSystem` is FALSE

`main_render.cpp:207` says *"PreInitSystem starts standing the RENDERER up"*, and
`main_milo.cpp:370-373` repeats it. **`PreInitSystem` is `os/System.cpp:472-510`
— 39 lines — and it contains no renderer call of any kind.** Its complete effect
list is the `gHostConfig` archive shim, 3 `DataSetMacro`s, the `-define` loop,
the `-config` override, `BeginDataRead`, `ReadSystemConfig`, `DataVariable("syscfg")`,
7 `DataRegisterFunc`s, `SetGfxMode(kNewGfx)`, and a conditional `InitSystem`
that cannot be reached with no command line. Corroborating: `grep -rn
"TheRnd.Init()\|TheRnd\.PreInit" src/` returns **zero hits tree-wide** — the
renderer is stood up by app-layer code, not the system layer.

> **`PreInitSystem()` is safe to call from a headless driver today**, and doing
> so would close 8 of the 23 items in one line, including `SetGfxMode`. The
> rationale that kept 18 drivers hand-rolling since X2 does not survive reading
> the function, and it has propagated verbatim through four milestones.

**`SystemInit()` is NOT reachable**, and the charter asked for precision on what
blocks it. Four named blockers, in order of severity:

1. `InitSystem(config)` → `MILO_ASSERT(systemConfig, 0x267)` at **`System.cpp:459`**.
   `ReadSystemConfig` is literally `DataReadFile(config, true)`, which returns
   null on a failed open. `config/band_keep.dta` pulls `ui/dev_only/selvenue.dta`,
   absent from the retail archive. **X4b's claim confirmed structurally.**
2. `TheContentMgr.Init()` — member call on a weak zeroed blob. Immediate fault.
3. `TheLocale.Init()` — needs `SystemConfig("locale")`, calls `DmMapDevkitDrive()`
   (a stub here), `MILO_FAIL`s on any missing language file.
4. `CheatsInit()` — `JoypadSubscribe`/`KeyboardSubscribe` with no `JoypadInit()`.

**Recommended shape (not landed this milestone — see §5):** call the real
`PreInitSystem()`, then `SystemInit`'s *safe tail* explicitly —
`gSystemTimer.Start(); ObjectDir::Init(); ThreadCallInit(); GeoInit(); TrigInit();`
— and skip `TheLocale`/`Cheats`/`ContentMgr` deliberately, recording *that* as
the documented split instead of the current all-or-nothing one.

---

## 3. Milestone 2 — `kNewGfx` is correct, and it was never the defect

### 3.1 The bisect that redirected the whole milestone

| leg | posed coverage | colours |
|---|---|---|
| `kOldGfx` (X4b baseline) | 22.80% | 20185 |
| `kNewGfx` | **0.00%** | 1 |
| **`loadonly`** (kNewGfx at `Load`, kOldGfx at draw) | **0.00%** | 1 |

Two measurements decide it:

1. **`loadonly` is still 0.00%.** Restoring the bones at `Load` and reverting to
   kOldGfx before the first draw still empties the frame, so **all 21 draw-time
   `GetGfxMode()` consumers are innocent.**
2. **Posed bone world positions are byte-identical between kOldGfx and kNewGfx.**
   The pose does not depend on gfx mode at all.

So the charter's Milestone 2 — "bisect the 22 consumers" — was the wrong search
space, and the `loadonly` leg cost one build to prove it.

### 3.2 ⛔ The real defect: a float-width byte-swap on a short section

`CharBonesSamples::LoadData`'s `HX_NATIVE` big-endian swap hardcoded a **4-byte**
element width for the POS/SCALE section. At `kCompressVects` and above,
`CharBones::TypeSize` (`CharBones.cpp:69-91`) says that section is **6 bytes —
three SHORTS** — not a multiple of 4. The loop ran twice (`p=0`, `p=4`) and the
second `bswap32` **overran two bytes into the QUAT section**.

Measured on the shipped `char/crowd/anim/gen/female_base.milo_xbox`
(compression 2, counts `[0,1,1,15,15,15,21]`, POS section exactly 6 bytes):

```
pos.x     <- the original Y channel
pos.y     <- the original X channel
pos.z     <- the FIRST QUAT channel's raw short, rescaled by 1300/32767
quat[0].x <- byte-reversed original Z            (also corrupted)
```

Root `bone_pelvis` local Z across beats:

| beat | 0.0 | 0.5 | 1.0 | 2.0 | 3.0 | 4.0 | 6.0 |
|---|---|---|---|---|---|---|---|
| **before** | +42.5 | +43.1 | +10.6 | **−52.3** | +24.4 | **−34.3** | **−29.0** |
| **after** | +42.5 | +39.2 | +44.7 | +44.6 | +46.6 | +45.5 | +41.3 |

Before is noise of plausible magnitude; after is a standing bob.

Fixed by deriving all three section widths from the same conditions as
`TypeSize`, at both swap sites. This also corrected a **second latent width
error**: the quat section used a 2-byte swap whenever `mCompression >=
kCompressRots`, but at `kCompressQuats` the field is 4 **bytes** and must not be
swapped at all.

★ **RB3-specific, and that is why it survived the port.** DC3 fixed the same
defect (`178b3ce4`) but it was a **no-op for DC3's own data** — all DC3 clips are
`kCompressRots`, where POS is uncompressed float and the old width happened to be
right. RB3's crowd clips are `kCompressVects`. rb3-Wii cannot have it at all
(native big-endian, no swap code). **So this is a native-port LE defect, not a
decomp divergence — no objdiff A/B debt.**

### 3.3 ★ Why every X4b oracle was blind to it

The bone-length invariant measures **pairwise distances between bones**. Those
are preserved exactly by **any rigid motion of the whole skeleton**, including an
arbitrary translation of the root. Only the root's position was garbage, and
every child inherited it rigidly — so the skeleton stayed perfectly rigid
(ratio 0.9999, deviation 7.47e-05, all determinants 1.000) while the entire
character teleported. A wrong-but-unit quaternion likewise preserves every length
and determinant.

> **An invariant is blind to the transformation group it is invariant under.**
> Catching this needs one *absolute* measurement. Landmark bone world positions
> found it in a single run.

### 3.4 What was ruled OUT, with evidence

| hypothesis | verdict | evidence |
|---|---|---|
| geometry flies / palette math wrong (X4b's stated explanation) | ⛔ **RETRACTED** | palette oracle: 72/72 bones resolved, `det(skin)==1.0000` (worst \|det−1\| 2.9e−04), `\|skin.v\|` 49–79 — sane for a 72-unit character |
| engine skin-fling clamp | ⛔ ruled out | `SKIN_CLAMP_PROBE=1` prints **nothing** (never fires); `RB3_NO_SKIN_CLAMP=1` still 0.00% |
| non-array-order bone index convention (e.g. D3D9 `idx*3`) | ⛔ ruled out | BLENDINDICES histogram is **dense `0..nBones−1`**, gcd 1, `maxIdx == nBones−1`, all 6 meshes |
| unnormalised bone weights | ⛔ ruled out | per-vert weight sums 0.668–1.000 |
| one of the 21 draw-time gfx consumers | ⛔ ruled out | `loadonly` leg, §3.1 |
| `Character.cpp:347` `MILO_ASSERT(GetGfxMode()==kOldGfx)` fires | ⛔ not reached | `rb3-render` iterates `RndMesh::DrawShowing()` and never enters `Character::DrawShadow`. **Still a live hazard for any future driver that does** — it is an assert that passes today only because `gGfxMode` was 0. |

### 3.5 The visual result

`RB3_GFX_MODE=old` vs default (kNewGfx), same clip, same beat, both with the
pose fix in:

| leg | coverage | what it looks like |
|---|---|---|
| kOldGfx (truncated) | 16.69% | torso and legs coherent, **arms shredded into long flat ribbons streaming off both sides** — X4b's "spikes" |
| **kNewGfx (default)** | 11.87% | **coherent limbs, raised reaching arms, no spikes** |

★ The A/B is a **direct attribution**: the spikes are the arm bones (indices
4..19) that the 20→4 truncation deleted, and they disappear exactly when the
truncation does. Higher coverage on the kOldGfx leg is the *artefact*, not a
win — it is shredded geometry spraying across the frame.

⚠ **Also fixed, and it mattered:** `SceneBounds` framed the camera on
`meshWorld * bindVert`, which is **not where a skinned mesh is** — the palette
places it, and the engine forces `object.world` to identity for skinned meshes.
The two agree only at bind pose (which *is* the palette invariant), so the
mis-framing was invisible until a pose was applied. Now framed on the skinned
positions, scoped to posed renders so the bind cell stays byte-exact.

## 4. What was added

| item | file | why |
|---|---|---|
| `BootInvariants::CheckAll()` | `native/src/boot_invariants.h` | 4 checks; **validated on a known-bad control** (emptying `TrigTableAutoInit` makes `trig-table` report `Sine(pi/2)=0.0000` and FAIL). Advisory; `RB3_STRICT_BOOT=1` makes it fatal. |
| palette invariant | `main_render.cpp` `AuditPalette` | `skin = mOffset*boneWorld` must have `det==1` always and equal `meshWorld` at bind. Exact, no ground truth. |
| landmark absolute positions | `main_render.cpp` `ReportBoneWorldPositions` | the check the bone-length invariant structurally cannot be (§3.2) |
| BLENDINDICES histogram | `main_render.cpp` `ReportVertexBoneIndices` | tests the vert→palette index convention against the shipped stream |
| CPU LBS bounds | `main_render.cpp` `ReportSkinnedBounds` | where the posed geometry actually lands, + weight sums |
| `RB3_GFX_MODE={old,new,loadonly}` | `main_render.cpp` | runtime A/B, and the load-vs-draw bisect |
| `[toplevel]`/`[persistent]` miss tags | `DirLoader.cpp`, `Instance.cpp` | §5 |

---

## 5. Milestone 3 — the coordinator's per-path experiment

`MILO_NOTIFY("%s: Can't make %s", …)` is emitted from **two** places with a
**byte-identical** format string — `obj/DirLoader.cpp:929` and
`world/Instance.cpp:232` — so X4a's log could not be attributed to a path after
the fact. The two have opposite recovery semantics: `DirLoader::LoadObjs` falls
through to `ReadDead(*mStream)` and re-syncs; `LoadPersistentObjects` replays
`PreLoad`/`PostLoad` inline with no marker and desyncs — and it
`DeleteObjects(); return;`s on the **first** miss, so it emits at most one
message per load.

Tagged apart (`HX_NATIVE`-gated). **Measured on the venue root:**

```
toplevel   : 675
persistent : 0
total      : 675

611 BandCamShot   23 Sfx       18 SynthSample  6 MoggClip   5 SynthFader
  4 BandCharacter  2 ParallelGroupSeq  2 BandLabel  1 RandomGroupSeq
  1 FxSendEQ  1 BandWardrobe  1 BandConfiguration
```

675 reconciles exactly with X4a's table minus the 8 (`WorldCrowd` 6 + `UIColor` 2)
X4b retired.

> **Every outstanding factory miss is on the RECOVERABLE path.** There is no
> persistent-object wall.

⚠ **But that does not make the venue load.** It still ends in **SIGSEGV
(rc=139)**, so the misses are not the only defect, and the segfault is
downstream of a stream that 675 `ReadDead` re-syncs have walked.

⚠ **The `BandCamShot` → `CamShot::NewObject` base-class bind (the 611) is
UNVERIFIED and was deliberately not landed.** The bind is mechanically possible
(`class BandCamShot : public CamShot`), and the instrumentation now shows those
misses *are* top-level, which was the coordinator's precondition. But a
base-class substitute runs the **base** `Load()`, which reads fewer bytes than
the derived object wrote. Whether `ReadDead` absorbs that short read is a
question to *measure*, not assume — and with a live SIGSEGV already in the trace
there is no clean signal to measure it against yet.

---

## 6. Engine-change requests — none blocking, zero edits

1. **A skinning-palette bone-count contract** (carried from X4b §7.2, unchanged
   and now better evidenced): the engine silently renders a mesh whose palette
   is partly identity. The only warning comes from xenon's `RndMesh` and it is
   advisory. Silent partial skinning is the wrong failure mode — this milestone
   is the second in a row where it hid a defect.
2. **The transparent queue still has no producer** (X4a §8.1, X4b §7.1,
   re-verified at `138e160`).
3. Carried unfixed: postproc grain seed not headless-reproducible at `frames ≥ 2`;
   static-lifetime GPU caches segfault after a clean `rc=0`; `GpuDevice` prints
   `device lost (reason 2)` before reporting successful init.

---

## 7. Retracted hypotheses, with evidence

1. ⛔ **"`SetGfxMode(kNewGfx)` empties the posed frame / the geometry leaves the
   camera entirely"** (X4b §5.2, and the 50-line comment block at
   `main_render.cpp:1418-1467`). **Retracted.** kNewGfx changes nothing about the
   pose — posed bone world positions are byte-identical between kOldGfx and
   kNewGfx. The frame emptied because restoring the bones let *all* vertices
   follow an **already-garbage root translation**, where the truncation had left
   most of them pinned at bind. The geometry never "left the camera" because of
   kNewGfx; it was in the wrong place all along and only some of it showed.
   Evidence: §3.1, §3.2.
2. ⛔ **"Bisect the 22 `gGfxMode` consumers"** (X4b §11 handoff, and this
   charter's Milestone 2 — the primary chartered task). **Retracted as the wrong
   search space** by the `loadonly` leg: kNewGfx at `Load` + kOldGfx at draw is
   still 0.00%, so 21 of the 22 are innocent and the 22nd (`MaxBones`) is
   correct. One build settled it.
3. ⛔ **"`GeoInit`/`TrigInit` are the likely third instance"** (this charter's
   Milestone 1(C), named as highest-suspicion). **Retracted**: `GeoInit`'s five
   globals have **non-zero in-source defaults** (`Geo.cpp:23-27`) and are all BSP
   collision tuning, off the draw path; `TrigInit` **fills nothing** — it is six
   `DataRegisterFunc` calls, and shares only a *filename* with `TrigTableInit`.
   The name collision is the entire reason it looked dangerous. The real third
   instance is `ThreadCallInit`. §2.2.
4. ⛔ **"`PreInitSystem` stands the renderer up, so a headless driver must not
   call it"** (`main_render.cpp:207`, `main_milo.cpp:370`, propagated verbatim
   through four milestones and used to justify the entire hand-rolled bring-up).
   **Retracted**: the function is 39 lines and contains no renderer call;
   `TheRnd.Init`/`TheRnd.PreInit` have **zero call sites tree-wide**. §2.3.
5. ⛔ **Two of my OWN measurements were wrong, both from the same cause, and it
   nearly inverted a conclusion.** I twice built a `rb3-render` command by
   stashing arguments in a shell variable and expanding it unquoted. **zsh does
   not word-split unquoted parameter expansions**, so the whole string arrives as
   a single `argv` entry:
   - `env $2 ./build/rb3-render …` passed one variable literally named
     `RB3_GFX_MODE` with value `"new SKIN_CLAMP_PROBE=1"`, failing the `strcmp`
     and silently running the **kOldGfx** arm. I briefly concluded X4b's 0.00%
     was non-reproducible. It is deterministic 5/5.
   - `… --frames 1 $C char/crowd/…` passed the entire clip flag string as one
     argument, so **no clip was applied** and the "posed" A/B was silently
     comparing two **bind-pose** renders at 15.78%.
   Recorded prominently because the failure is *silent in both directions* and
   the tool has no way to complain. Write the flags out in full.
6. ✅ **Promoted, not retracted:** the bone truncation is real and restoring the
   bones is right. Independently confirmed by the palette invariant at 20 bones
   and by the visual A/B in §3.5, where the arm spikes disappear exactly when the
   truncation does.
7. ⚠ **Explicitly NOT claimed:** that the character is now *fully* correct. The
   posed figure is coherent and visibly reaching, and every available invariant
   passes — but there is **no retail ground truth in this loop**. "No detectable
   defect" is the claim, not "matches the shipped game".

## 8. Owed work / handoff

| item | why | owner |
|---|---|---|
| ⛔ **`ThreadCallInit` never runs** | Tier-1, new, and the genuine third instance of the X4b defect shape. `DataLoader::LoadFile` can spin forever with **no diagnostic**; `gMainThreadID == -1` silently disables **every** `MainThread()` assertion in the engine. | **X4d / native** |
| ⛔ **`ObjectDir::Init` never runs** | `"milo"`/`"milo_xbox"` loader factories unregistered; `LoadMgr::AddLoader` silently returns a `FileLoader` instead of a `DirLoader`, with no warning and no null. Top-level loads escape; six named call sites do not. | **X4d / native** |
| **Call the real `PreInitSystem()`** | Closes 8 sub-inits in one line and the stated blocker is false (§2.3). Now safe to sequence, since `SetGfxMode(kNewGfx)` — the one thing it does that changes behaviour — is already landed and verified. | native |
| **Venue SIGSEGV** | All 675 factory misses are on the recoverable path (§5), so the segfault is a **separate** defect and the venue was never blocked on registrations alone. | X4d |
| `BandCamShot`→`CamShot` base bind | Precondition now met (misses **are** top-level). Short-read vs `ReadDead` interaction still unverified — measure it, don't assume it. | X4d |
| **Re-audit `Character.cpp:347`** | `MILO_ASSERT(GetGfxMode() == kOldGfx)` in `Character::DrawShadow` passed only because `gGfxMode` was 0. Now that kNewGfx is the default, **any driver that reaches `DrawShadow` will assert.** `rb3-render` does not (it iterates `DrawShowing` directly), so it is latent — and it is squarely in the way of the `Character::DrawLodOrShadow` item below. | X4d |
| `Character::DrawLodOrShadow` draw path | Still the only thing between a driven character and `ShadowPass`. Now coupled to the row above. | X4d |
| Carried from X4a/X4b, untouched | `ScatterIncludes.cmake` dedupe (807 dup-def link errors); 4 root defects in `BandCharacter.cpp` incl. the latent `Refs()` hang; `WorldInstance::PreLoad/PostLoad` rev ordering | — |

## 9. Recommended X4d shape

1. ★ **Pick oracles that are not invariant under the defect you are hunting.**
   This is the transferable lesson of the milestone. X4b's bone-length invariant
   is a genuinely good instrument — it caught two real defects — and it was
   *structurally blind* here, because pairwise distances are preserved by any
   rigid motion of the root, and unit quaternions preserve them even when wrong.
   Before trusting an invariant, ask **what transformation group it is invariant
   under**, and add one absolute measurement to cover it. A single landmark
   world position found in one run what four milestones of rigidity checks could
   not see.
2. ★ **`nm` + `--gc-sections` is the cheap decisive oracle for "is this ever
   called".** Both suspects the charter named were innocent; the real third
   instance was found by linker ground truth, not intuition. Reach for it first
   in any further init audit.
3. **Bisect along the mechanism, not the symptom list.** Milestone 2 was framed
   as "bisect 22 consumers". Splitting them by *when they are read* (load vs
   draw) collapsed 22 candidates to 1 in a single build. Look for a seam that
   partitions the search space before enumerating it.
4. **Distrust a wrong number before distrusting the predecessor.** Both of my bad
   measurements (§7.5) looked like evidence that X4b was wrong. Reproducing the
   predecessor's exact command verbatim, before varying anything, is what caught
   them.
