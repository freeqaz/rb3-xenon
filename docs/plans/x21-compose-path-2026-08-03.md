# X21 — `SyncOutfitConfig` IS reached; the compose pass is never dispatched; and the dc3 backend cannot host it

**Date:** 2026-08-03
**Predecessor:** [X20](x20-textures-2026-08-03.md) "registration was necessary and NOT sufficient"
**Branch:** `x21-compose-path`, from `main` @ `18d4adfb`, rebased onto `869a445c`
**Engine:** `milo-native-engine` pinned at **`138e1606`**, **zero engine edits**
**Change surface:** `native/src/main_render.cpp` (native-only) + **three shared `src/` TUs**, every edit inside `#ifdef HX_NATIVE`: `src/system/bandobj/BandCharacter.cpp`, `src/system/bandobj/OutfitConfig.cpp`, `src/system/rndobj/Rnd.cpp`.

---

## Verdict

⛔ **X20's CENTRAL HANDOFF IS REFUTED BY MEASUREMENT. `SyncOutfitConfig` IS
REACHED — 336 calls, 42 of them `sym='skin'` — AND `SetSkinTextures` RUNS**, on
all four members, with `skin.cfg=FOUND`, correct genders, all five materials
found and all three render targets found. The call path was **already
complete** when X20 handed it forward as the whole remaining distance. §1

★★★ **X20 ASKED FOR A POSITIVE INDICATOR AND THE POSITIVE INDICATOR IS WHAT
OVERTURNED ITS CONCLUSION.** X20 correctly warned that a failure-only predicate
cannot separate "never ran" from "ran and succeeded"; it then reasoned about
the call path without building one. Building one cost 40 lines. §1

★★★ **THE INSTRUMENT WAS MEASURING NAMES, NOT IDENTITIES — SIXTH TIME ON THIS
LADDER.** The census's "58 skin material instances" are **(mesh,mat) PAIRS over
only 11 DISTINCT `RndMat` OBJECTS**. `SetSkinTextures` rebinds all four
per-member materials correctly; every **visible** body mesh references **one
shared material in `char_shared.milo`** that no member ever touches. §2

★★★ **THE OUTFIT COMPOSE PASS NEVER RUNS AT ALL, AND THE CAUSE IS A POLARITY
INVERSION IN `Rnd::DrawPreClear` vs THE rb3-Wii ORACLE.** `mPreClearDraws=0
mDraws=40 listUsed=0`; `OutfitConfig::DrawPreClear` **0 calls**;
`MatSwap::Compose` **0 calls** — against 734 trace lines from the same predicate
in the same run. §3

⛔ **AND CORRECTING IT IS NECESSARY, NOT SUFFICIENT — THE SAME SHAPE X20 HIT ONE
LINK EARLIER, WHICH IS WHY THE FIX SHIPS OPT-IN.** With the polarity corrected
the dispatch is repaired (`listUsed` 0→40, `DrawPreClear` 0→40, `Compose` 0→44)
and the **frame dies**: WebGPU pass-nesting violation, coverage **0.00%**, 1
distinct colour. §4

★★★ **MILESTONE 2 ANSWERED, WITH A COST AND A CROSS-CONSUMER RISK. The repair is
ENGINE-SIDE and is filed as a change request.** The dc3 backend has a full RTT
path and already calls `DrawPreClear()` — but its `DrawRect` **drops
`mat->GetColor()` entirely**, has no `colorMod` awareness, no 2-texture interp
pass and no pipeline cache. It cannot perform the composite. §5, §9

⛔ **THE CHARTER'S OWN VARIABLE NAME WAS WRONG, AND I CHECKED RATHER THAN QUOTED
IT.** `MILO_ENGINE_GPU_PLATFORM_SOURCES_DC3` **does not exist** — zero
occurrences engine-wide. The dc3 list is the **unsuffixed**
`MILO_ENGINE_GPU_PLATFORM_SOURCES`. §5.1

⛔ **A NEW GATE DEFECT: THE INHERITED WORKTREE RECIPE IS INSUFFICIENT AND FAILS
SILENTLY-ISH.** Seeding the four absolute cache flags is **not enough**; the two
compiler variables must be pinned in the gate's own spelling or the gate's
reconfigure **deletes the cache** and SKIPs the three engine targets. Caught by
the charter's 0-SKIP rule. §7

✅ **Default frame BYTE-IDENTICAL to X20's artifact** (md5 `d5d79558…`), twice.
Stronger than X20's quantified 455-pixel non-identity. §8.1

✅ **Gate PASS 18/18 fresh, rc=0, 0 SKIPs.** `main` was **not** broken by a
decomp lane. §8

⛔ **MILESTONE 3 NOT REACHED.** §6

---

## 1. ⛔ Milestone 1 — the premise was false; the chain was already complete

X20 handed forward: *"`SyncOutfitConfig` has exactly two callers, and neither
runs"*, and named the remaining search as *"does `SyncObjects()` run on the band
members, and is `unk620` populated by `AddObject` by then?"*

I did not answer that question. I **instrumented every link so that it fires on
SUCCESS**, which is what X20's own §3 said was missing, and read the result.

`RB3_X21_TRACE=1`, one run, `RB3_BAND_PLACE=1 RB3_BAND_POLL=1 --frames 1`
(`x21-trace-r1.log`):

| link | calls | detail |
|---|---|---|
| `BandCharacter::SyncObjects` | **50** | on `player0`…`player3` |
| `BandCharacter::SetDeformation` | **50** | `clip=male` / `clip=female`, non-null every time |
| `BandCharacter::OnPostMerge` | **254** | 32 of them with `unk630=1` |
| `BandCharacter::SyncOutfitConfig` | **336** | `torso/legs/skin/eyes/feet/hair/hands/wrist/eyebrows/guitar` |
| … of those, `sym='skin'` | **42** | `isSkin=1`, `mOutfitDir=yes` |
| `OutfitConfig::SetSkinTextures` | **42** | **all** with `skin.cfg=FOUND` |

```
[X21] OutfitConfig::SetSkinTextures ENTER dir1='player0 (char/main/main.milo)'
      dir2='outfit (char/main/outfit.milo)' skin.cfg=FOUND gender='male'
[X21]   part='torso' mat='torso_naked.mat' … authored_diff=male_torso_diff.tex
        rt=torso_skin_diffuse_output.tex
```

All four members, genders `male/female/male/male` — matching the asset. Every
material found, every authored diffuse found, every render target found.

★ **So X20's §3.2 search was aimed at a question whose answer was already
"yes".** Its two named sub-questions are both answered here in passing:
`SyncObjects()` **does** run, and `unk620` **is** populated — though not at the
first `SetDeformation` (see below), which is the detail that makes the
"never reached" reading superficially plausible.

⚠ **What made the wrong conclusion reachable.** The *first* `SetDeformation` on
each member does print `unk620=0`, because `unk620` is filled by `AddObject`
from the **merge** filter (`BandCharacter.cpp:2537`), which happens later. A
probe that sampled only the first call would have "confirmed" X20. The trace
prints its denominator on **every** call, which is what exposed the later
non-empty ones.

---

## 2. ★★★ The census was reading names, not identities

X20 read the skin census, saw `dummy_torso/legs/feet.tex` + NULL, and concluded
"unchanged". **RT-bound rows were present in the same census, further down the
same list.** Both readings are true of different objects with the same name.

The census now prints the material **pointer** and its **owning dir**, and
counts distinct objects. `x21-trace-r2.log`:

```
=== X19 SKIN-MATERIAL DIFFUSE CENSUS: 58 skin material instance(s), 54 with a diffuse, 4 NULL ===
=== X21 CORRECTION: those 58 rows are (mesh,mat) PAIRS over 11 DISTINCT RndMat object(s) ===
```

★★ **58 was never a count of materials.** It has been read as one for two lanes.

### 2.1 What `SetSkinTextures` binds vs what draws

| | mat pointer | owning dir | diffuse |
|---|---|---|---|
| bound by `SetSkinTextures` | `0x…3716140` | `player2 (char/main/main.milo)` | `torso_skin_diffuse_output.tex` |
| | `0x…373fcb0` | `player0` | `torso_skin_diffuse_output.tex` |
| | `0x…37a2b10` | `player1` | `torso_skin_diffuse_output.tex` |
| | `0x…3936890` | `player3` | `torso_skin_diffuse_output.tex` |
| **used by the visible meshes** | **`0x…2ed1830`** | **`ObjectDir (char/main/shared/char_shared.milo)`** | **`dummy_torso.tex`** |

`hands_naked.mesh`, `vestdenim_skin.1.mesh`, `plaidshirt_skin.1.mesh`,
`shred_skin.1.mesh`, `trackjacket_skin.2.mesh` and
`tightdistressedpants_skin.3.mesh` **all six** point at the single shared
`0x…2ed1830`. The only meshes using the correctly-rebound per-member materials
are `male_tattoo_torso.mesh` and `female_tattoo_torso.mesh` — because
`SetSkinTextures`' own tail explicitly repoints the tattoo meshes
(`OutfitConfig.cpp:566-574`).

★★★ **This is X19's shared-skeleton defect one subsystem over.** X19 found the
band sharing one unplaced *skeleton* from a shared dir in preference to their own
placed bones; here they share one *material* from `char_shared.milo` in
preference to their own rebound ones. Same root shape, different object type.

### 2.2 The shipped mechanism that should prevent it, and why it does not

`BandCharacter::MergeObject` (`BandCharacter.cpp:2519-2523`) carries the retail
answer:

```cpp
if (o1->Dir() == sCharSharedDir) {
    Hmx::Object *mine = Find<Hmx::Object>(o1->Name(), true);
    MILO_ASSERT(mine->Dir() == this, 0xAB8);
    ::ReplaceRefs(o1, mine);
    return kIgnore;
}
```

i.e. anything belonging to `char_shared` is *replaced by the member's own
same-named object*. That only runs if the subdir is **merged** — and the
existing native shim in `BandCharacter::FilterSubdir` (`:2586-2635`, added to fix
"char textures rendering white") converts `kMergeMerge → kMergeReplace` for any
subdir that is its own on-disk milo, deliberately **keeping** `char_shared.milo`
as a shared reference instead of draining it.

⚠ **I did NOT test removing that shim**, and it should not be tried blind: the
in-tree note and `CHAR_SKINNING_DEFORM_INVESTIGATION.md` record full shim-OFF as
a **proven dead end** (retail `kMerge` → same shared root **and** white
textures). Stated as a located tension, not a proposed fix. Owner: §10.

---

## 3. ★★★ The compose pass never runs — polarity inversion in `Rnd::DrawPreClear`

`SetSkinTextures` binds `*_skin_diffuse_output.tex`, a **render target**. X20
flagged, correctly, that binding it may still yield blank skin. It is worse and
cleaner than that: **nothing ever paints those RTs, because the only dispatcher
that can reach the compose never selects the list the composers are in.**

### 3.1 The measurement

```
[X21] Rnd::DrawPreClear ENTER releaseImmediate=0 mPreClearDraws=0 mDraws=40 listUsed=0
OutfitConfig::DrawPreClear     -> 0 calls
OutfitConfig::MatSwap::Compose -> 0 calls
```

★ **The zeros are absences, not a dead instrument.** The same
`RB3_X21_TRACE` predicate, in the same binary, in the same run, emitted **734**
other lines. A positive control was required precisely because X20 showed a
bare zero here is worthless.

★ **The dispatcher is NOT the problem, and separating that mattered.** The probe
prints both list sizes, so "never called" and "called over an empty list" are
distinguishable — they have different owners and different fixes.
`Rnd::DrawPreClear` **is** called; `mPreClearDraws` is **empty**; `mDraws` holds
**40** — exactly the 40 registered `OutfitConfig` instances X20 measured.

### 3.2 The defect, against the oracle

| | expression |
|---|---|
| rb3-Wii (MWCC oracle), `src/system/rndobj/Rnd.cpp:742` | `drawList = unk130 ? &unk110 : &mDraws;` |
| rb3-xenon, `src/system/rndobj/Rnd.cpp:1274` | `drawList = mReleaseImmediate ? &mDraws : &mPreClearDraws;` |

The member correspondence is exact **by offset and by order** — first list,
second list, and the bool immediately after the second list:

| rb3-Wii | offset | rb3-xenon | offset |
|---|---|---|---|
| `unk110` | 0x110 | `mPreClearDraws` | 0x128 |
| `mDraws` | 0x120 | `mDraws` | 0x13c |
| `unk130` | 0x130 | `mReleaseImmediate` | 0x150 |

⇒ **the two arms are swapped**, not the members renamed.

It bites because **both trees register identically**:
`Rnd::PreClearDrawAddOrRemove` is `b3 ? [first list] : mDraws` in both, and
`OutfitConfig::UpdatePreClearState` (xenon `OutfitConfig.cpp:865`,
token-identical to rb3-Wii's `:1026`) calls it with **`b3 = false`** — so every
`OutfitConfig` lands in `mDraws`. `mReleaseImmediate` is false all frame (only
`movie/Splash.cpp:190,252` ever writes it, for the boot splash), so the inverted
expression reads the **empty** list.

### 3.3 ⚠ This function CANNOT BE SCORED — stated, not implied

`?DrawPreClear@Rnd@@MAAXXZ` reports **`target_size: 0`** in objdiff, has **no
ICF alias** in `build/45410914/icf_aliases.map`, and the string occurs **nowhere
under `build/45410914/asm`**. There is no X360 target body to diff against.

So the correction is asserted from **the rb3-Wii oracle plus the offset
correspondence**, *not* from a match measurement — and it is confined to
`HX_NATIVE`, leaving the X360 arm byte-identical. A scored lane should revisit
the retail arm if a target body ever appears.

---

## 4. ⛔ The correction is OPT-IN, and that is a finding

The house rule makes native render fixes default-ON once ON-vs-OFF evidence
exists. **The evidence exists and it says do not ship this alone.**

`RB3_X21_PRECLEAR_POLARITY_FIX=1` (`x21-fix-r1.log`):

| quantity | default | fix ON |
|---|---|---|
| `listUsed` | 0 | **40** |
| `OutfitConfig::DrawPreClear` calls | 0 | **40** |
| `MatSwap::Compose` calls | 0 | **44** |
| run rc | 0 | **1** |
| frame coverage | (venue renders) | **0.00%, 1 distinct colour** |

```
GpuDevice: WebGPU error (type 2): Recording in [CommandEncoder "FrameEncoder"]
which is locked while [RenderPassEncoder "MainPassResume"] is open.
  [FAIL] image-not-empty — coverage 0.00% (>= 1%), 1 distinct colours
```

The composites *run* — `MatSwap::Compose mat='guitar_51squier_base_swap.mat'
diffTex='51squier_paint_diff_output.tex' rttDiff=1` — and then the frame's own
`BeginRenderPass` is refused because a pass was left open. **A black frame is a
regression against a venue that currently renders, and a crash is not a frame**
(X7). Kept reachable for the lane that fixes the backend; OFF until then.

★★ **This is the same shape X20 hit one link earlier**: remove the broken first
link of a chain and the last link does not move. X20's own recommendation #1.
I inherited the lesson and still needed the measurement to know *which* link.

---

## 5. ★★★ Milestone 2 — the compose-pass flavor gap, decided

### 5.1 ⛔ First, the name in the charter is wrong

The charter (and X20 §3.4) cite `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` vs
`MILO_ENGINE_GPU_PLATFORM_SOURCES_DC3`. **Verified engine-wide: the `_DC3`
variable does not exist — zero occurrences.** The dc3 list is the *unsuffixed*
`MILO_ENGINE_GPU_PLATFORM_SOURCES` (`CMakeLists.txt:304`), and a **second**
dc3-only list `MILO_ENGINE_GFX_RNDOBJ_SOURCES` (`:277`) holds
`src/gfx/DrawRect2D.cpp`, which is where the dc3 `DrawRect` actually lives.
Anyone writing the CR from the inherited name would patch a variable that isn't
there.

### 5.2 Does the dc3 backend have an equivalent compose path?

**Split answer, and the split is the whole decision.**

| concern | rb3 flavor | dc3 flavor | gap? |
|---|---|---|---|
| pre-clear dispatch (`DrawPreClear()`) | `Rnd_Wgpu_RB3.cpp:2055` | **`Rnd_Wgpu.cpp:985`** | ✅ **none** |
| RT alloc / begin / end | `BandRnd::Begin/EndDrawTarget` | `Tex_Wgpu.cpp:54`, `Rnd_Wgpu.cpp:683/764/772` | ✅ **none** |
| the compose WGSL | `src/gfx/Shaders/rb3_compose.wgsl.inc` | **same file, physically shared** | ✅ **none** |
| `MatSwap::Compose` (the authoring sequence) | consumer game code | **same consumer code** | ✅ **none** |
| **the compose state machine in `DrawRect`** | `RB3Quad.cpp:366-459` | **ABSENT** | ⛔ |
| `mat->GetColor()` folded into modulation | `RB3Quad.cpp:278-293` | **ABSENT — `DrawRect2D.cpp` uses only the param colour (`:116`)** | ⛔ |
| `colorMod` awareness | `RB3Quad.cpp:285` | **ABSENT** | ⛔ |
| 2-texture interp pass (4-entry BGL) | `RB3Quad.cpp:112-155` | **ABSENT (2-entry BGL, `DrawRect2D.cpp:69-81`)** | ⛔ |
| pipeline cache | `RB3QuadPipeKey`, `:209-214` | **ABSENT — creates a pipeline per draw (`:202`)** | ⛔ |

**Verified personally, not inherited:** `DrawRect2D.cpp` contains no
`GetColor`/`ColorMod` reference at all; `Rnd_Wgpu.cpp:985` does call
`DrawPreClear()`; the `_DC3` variable does not exist.

⇒ **Outfit compose is genuinely absent from the dc3 backend.** `Compose` passes
white as the param colour and sets the real tint via `mMat->SetColor(...)`, so on
dc3 **every layer would multiply by white** — no tint, last-layer-wins. And as
§4 measured, in practice it does not even get that far: the pass nesting is
refused outright.

### 5.3 Is the right repair dc3-side, a promotion, or xenon-side?

**All three, in a specific order — and the first step is xenon-side and is
already done here.**

1. **Xenon-side (done, §3):** nothing dispatches. Must be fixed first or the
   engine work cannot be observed at all. **No engine change needed.**
2. **Engine-side (CR, §9):** move the flag out of the rb3 flavor — ~3 lines,
   near-zero risk — then implement the compose in the dc3 path.
3. **Promotion of `RB3Quad.cpp` wholesale: NOT VIABLE.** Not for platform
   reasons — its includes are pure WebGPU + `rndobj` + libc, **no GX, no Wii
   headers** — but because every function is a `BandRnd` member touching ~15
   private members, and it carries RB3-only riders (`RB3_SCREENMASK_FIX`,
   `RB3_DRAWRECT_DBG`, hardcoded `multisample.count = 1`) that must not ride
   along. The **compose sub-path specifically** is cleanly extractable into a
   flavor-neutral helper shaped like `DrawRect2D::Draw`.

---

## 6. ⛔ Milestone 3 — the 120 shared-skeleton publications: NOT REACHED

Read `rb3/docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` and its four proven
dead ends first, as instructed. **I attempted none of them and propose no
fifth.** Nothing was measured; X19 §4's structural result stands exactly as it
was. Milestones 1 and 2 consumed the lane — the third consecutive lane to spend
itself on the texture chain, which is the honest accounting.

★ One free observation, offered as a hypothesis and explicitly **not** as
evidence: §2's finding is *the same shape* as the skeleton one — a shared
`char_shared`/`skeleton.milo` object winning over a correct per-member object —
and both are downstream of the same `FilterSubdir` shim. Whether one fix
addresses both is **unmeasured**.

---

## 7. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Is `SyncOutfitConfig` reached?** | ⛔ **YES — X20's "neither caller runs" is REFUTED. 336 calls, 42 with `sym='skin'`** | §1 |
| **Does `SetSkinTextures` run?** | ★★★ **YES — 42 calls, all 4 members, `skin.cfg=FOUND`, correct genders, all mats + all 3 RTs found** | §1 |
| **X20's two named sub-questions** | ✅ **Both answered in passing: `SyncObjects()` runs; `unk620` IS populated (later than the first `SetDeformation`)** | §1 |
| **Is the band textured?** | ⛔ **NO. ACCEPTANCE NOT MET** | §8.1 |
| **Why not (proximate)** | ★★★ **The visible meshes use a SHARED `char_shared.milo` material that no member rebinds; only the tattoo meshes use the rebound ones** | §2.1 |
| **The "58 skin material instances" figure** | ⛔ **RETIRED — 58 (mesh,mat) PAIRS over 11 DISTINCT RndMat objects. Never a material count** | §2 |
| **Does the compose pass run?** | ★★★ **NO — `DrawPreClear` 0 calls, `Compose` 0 calls, against 734 lines from the same predicate. Positive control held** | §3.1 |
| **Why not** | ★★★ **`Rnd::DrawPreClear`'s list-selection arms are SWAPPED vs the rb3-Wii oracle; offsets map 1:1** | §3.2 |
| **Can that be scored?** | ⚠ **NO — `target_size=0`, no ICF alias, absent from `build/45410914/asm`. CANNOT BE SCORED AT ALL** | §3.3 |
| **Does correcting it fix the band?** | ⛔ **NO — dispatch repaired (0→40/0→44) and the FRAME DIES: WebGPU pass-nesting, coverage 0.00%. Necessary, not sufficient** | §4 |
| **Does dc3 have a compose path?** | ⛔ **NO — state machine, `colorMod`, `mat->GetColor()` fold, 2-texture pass and pipeline cache are ALL absent** | §5.2 |
| **Does dc3 have RTT / pre-clear dispatch?** | ✅ **YES, both — the scaffolding is complete; only the composite itself is missing** | §5.2 |
| **`MILO_ENGINE_GPU_PLATFORM_SOURCES_DC3`** | ⛔ **DOES NOT EXIST — 0 occurrences engine-wide. The charter's own name was wrong** | §5.1 |
| **Promote `RB3Quad.cpp` to shared?** | ⛔ **NOT VIABLE as a file (BandRnd members + RB3-only riders) — but NOT for platform reasons: no GX, no Wii headers. The compose SUB-PATH is extractable** | §5.3 |
| **The 120 shared-skeleton publications** | ⛔ **NOT ADVANCED — untouched, no fifth attempt proposed** | §6 |
| **FULL rebind scope under a driven clip** | ⛔ **NOT REACHED — eighth lane** | §10 |
| **Worktree gate recipe** | ⛔ **INHERITED RECIPE INSUFFICIENT — compilers must be pinned too, or the gate wipes the cache and SKIPs 3 targets** | §7.1 |
| **X360 blast radius** | ✅ **ZERO, MEASURED not assumed — all 3 touched TUs UNCHANGED at unit granularity, both objects built in this worktree** | §8 |
| **Frames** | ✅ **BYTE-IDENTICAL to X20's artifact, twice** | §8.1 |

### 7.1 ⛔ The gate defect (new, and it invalidates X18/X19/X20's recipe)

X18 discovered that a worktree needs four absolute cache flags
(`MILO_ENGINE_PATH`, `Dawn_DIR`, `glfw3_DIR`, `RB3X_BUILD_ENGINE`), and X19/X20
carried that forward. **It is not sufficient.**

`tools/native_build_gate.sh:227` reconfigures with
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`. A plain `cmake -B build`
caches the *resolved* compiler paths, so those literals differ, and CMake
responds with **"You have changed variables that require your cache to be
deleted"** — wiping the four flags and re-deriving `MILO_ENGINE_PATH`/`Dawn_DIR`
from the **relative** defaults, which resolve under `/home/free/tmp/`.

Measured on my first baseline (`x21-gate-baseline.log`, first run):
`NATIVE GATE: PASS (rc=0, 0 errors, 15/18 verified, 3 skipped)` — `rb3-frame`,
`rb3-milo`, `rb3-render` all SKIPPED, i.e. **the gate passed without ever
building the target this lane is about.**

★ **The gate reported SKIPPED loudly and the charter's "0 SKIPs" rule is what
converted that into a stop.** A PASS line alone would have been read as green.

**Corrected recipe — seed SIX flags, in the gate's own spelling:**

```
cmake -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine \
  -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn \
  -Dglfw3_DIR=/usr/lib/cmake/glfw3 -DRB3X_BUILD_ENGINE=ON
```

Verified by simulating the gate's exact reconfigure afterwards: `0` occurrences
of "cache to be deleted", absolute paths intact.

---

## 8. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, 0 warnings, **0 SKIP lines**, all 18 relinked, binaries deleted first, rebuild awaited before probing | `x21-gate-final.log` |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — branch-point gate PASS 18/18 fresh, 0 SKIPs, before any edit. `main` @ `18d4adfb` healthy; rebased onto `869a445c` and re-gated | `x21-gate-baseline.log` |
| b2 | Cache flags seeded | ⛔ **FOUR IS NOT ENOUGH — SIX required.** See §7.1. Caught prospectively by the 0-SKIP rule | §7.1, `x21-configure.log` |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606` unmoved; engine HEAD *is* `138e1606`. The foreign uncommitted `src/platform/FxSendNative.cpp` edit is disclosed and untouched — **eighteenth lane** | verified |
| d | Shared-`src/` X360 faithfulness | ✅ **PASS, MEASURED.** 3 shared TUs touched, every edit inside `#ifdef HX_NATIVE`. Rebuilt in this worktree with **both objects built here**: `default/BandCharacter` 85.9681 (508/603) **UNCHANGED**, `default/OutfitConfig` 70.9891 (182/232) **UNCHANGED**, `default/system/rndobj/Rnd` 87.8689 (296/329) **UNCHANGED** | §8 |
| d2 | objdiff position of touched TUs | ✅ **stated above.** `native/src/main_render.cpp` **cannot be scored at all** — not in `objdiff.json`, native-only by design. Within `rndobj/Rnd`, the specific function edited (`Rnd::DrawPreClear`) **also cannot be scored** (§3.3) | §3.3 |
| e | PNG determinism ×2 | ✅ **PASS** — md5 `d5d79558…` on two full runs | §8.1 |
| f | Prior evidence non-regressed vs **artifacts** | ✅ **PASS — BYTE-IDENTICAL** to X20's `x20-A-outfitcfg-poll-club.png`, `cmp`-clean. Not a quantified near-miss: identical | §8.1 |
| g | `RB3_BAND_PLACE=1` present | ✅ **PASS** — every cited run carries it; the 4-member / 42-`SetSkinTextures` denominators confirm the band was placed | §1 |

### 8.1 Frames — opened

`x21-A-default-club.png` (md5 `d5d79558…`). **Opened it.**

The small-club interior in cutaway, fully lit and textured — plank floors, brick
and plaster walls with a green-panelled section, the bar, a stack of wooden
chairs, the staircase and its metal rails, a doorway and window. A **textured
crowd** of ~20 figures lines the upper balcony in distinct clothing (jeans,
pale shirts, dark shoes). On the stage floor below, **four untextured pink
figures** stand upright on four distinct marks, heads present, no shards, no
missing geometry — holding instruments. **The band is still pink.**

`x21-B-polarityfix-BLACK.png` — the opt-in arm, kept as the ON-vs-OFF evidence:
**an entirely black 1280×720 frame**, one distinct colour, 0.00% coverage. 36 KB
against the default's 984 KB. This is why the fix is opt-in.

---

## 9. ★★★ ENGINE CHANGE REQUEST — for the coordinator

**I made no engine edit.** Repo: `milo-native-engine`, HEAD = pin `138e1606`.
Consumers: `rb3` (backend `rb3`), `rb3-xenon` (backend `dc3`), `dc3-decomp`.

### CR-1 — move `gRB3OutfitComposeActive` to a flavor-independent TU

| | |
|---|---|
| **change** | Move `bool gRB3OutfitComposeActive` from `src/platform/RB3Quad.cpp:225` into a TU in the **unconditional** `MILO_ENGINE_PLATFORM_SOURCES` (e.g. `GameRenderHook.cpp`, `CMakeLists.txt:345`), and declare it in a shared header (it currently has **no** header declaration anywhere). |
| **cost** | ~3 lines. |
| **why** | Both consumers' game code already does `extern bool gRB3OutfitComposeActive;` inside `MatSwap::Compose` (rb3 `OutfitConfig.cpp:108-126`, rb3-xenon `:131-134` — byte-identical). On dc3 the symbol is satisfied only by a **local stub** at `rb3-xenon/native/src/x20_bandpatchmesh_link.cpp:107`. |
| **risk — rb3-Wii** | **None** behaviourally; it keeps reading/writing the same flag. |
| **risk — dc3-decomp** | **None** — nothing in dc3 reads it. |
| **risk — rb3-xenon** | ⚠ **Coordinated deletion required.** X20 deliberately made its stub a *strong* definition so switching backends fails loudly with a duplicate symbol. Landing CR-1 **breaks rb3-xenon's link** until that stub is deleted in the same change. Deliberate, documented, and must be sequenced. |

### CR-2 — implement the compose in the dc3 `DrawRect` path

| | |
|---|---|
| **change** | Port `RB3Quad.cpp`'s compose sub-path into `src/gfx/DrawRect2D.cpp` (or a new shared `OutfitCompose.{h,cpp}` in the unconditional list): the pipeline setup (`RB3Quad.cpp:112-155`, ~44 lines), the 4-arm state machine (`:366-459`, ~93 lines), the 9 `mCompose*` members (`Rnd_Wgpu_RB3.h:371-380`), and ~30 lines plumbing a `RB3RectUB`-style uniform + `colorMod` + a `mat->GetColor()` fold into `DrawRect2D`. **Reuses `src/gfx/Shaders/rb3_compose.wgsl.inc` unchanged — no new shader authoring.** |
| **cost** | **~150-190 lines**, plus a pipeline cache (`DrawRect2D.cpp:202` currently creates a pipeline **per draw**; compose adds 3-4 draws per material). |
| **portability deltas (mechanical)** | `mat->mColorModFlags` → **`GetColorModFlags()`** (dc3's member is not a public bitfield, so direct access will not compile); `GetRB3TexView`/`UploadRndTexIfNeeded` → dc3's `GetGpuTexView`; `RB3RttDisabled()` + the lazy `BeginDrawTarget` hook are **not needed** on dc3 (`sCam->Select()` opens the pass eagerly). |
| **risk — rb3-Wii** | **Low if the rb3 flavor is left alone**; **HIGH if `RB3Quad.cpp` is refactored in place** to share the code, since that file is the live, tuned path for a shipping renderer. Recommend **adding** a dc3 implementation, not unifying, in this CR. |
| **risk — dc3-decomp** | ⚠ **REAL, and it is the folding of `mat->GetColor()`, not the compose.** That changes modulation for **every** dc3 `DrawRect` caller (UI, postproc, vignette), not just compose. **Gate it independently**, mirroring `RB3_COMPOSE_MULT_OFF` (`RB3Quad.cpp:366`). |
| **risk — rb3-xenon** | ⚠ **Pass nesting is a prerequisite, not a detail.** §4 measured that dispatching compose today yields *"Recording in CommandEncoder locked while RenderPassEncoder MainPassResume is open"* and a black frame. CR-2 must resolve that, or CR-2 lands and the frame is still dead. |

### Sequencing

**§3's xenon-side fix must be ON for CR-2 to be observable at all** — on dc3 the
composite is never dispatched, so an engine implementation would sit dead and
untested. Order: land CR-1 + delete the xenon stub → land CR-2 → flip
`RB3_X21_PRECLEAR_POLARITY_FIX` to default-ON in rb3-xenon and re-measure §2's
census. **§2's shared-material defect is independent of all of this** and will
still leave the visible meshes on `char_shared`'s material even after CR-2.

---

## 10. Retracted / corrected

⛔ **X20's "`SyncOutfitConfig` has exactly two callers, neither reached"** —
refuted, §1. Both are reached; the whole path already worked. X20's *static*
reading of the call graph was accurate; its inference that the path was
therefore dead was not tested.

⛔ **X20's "the remaining gap is the call path"** (§3.3) — false. The asset side
*and* the call path were both already complete.

⛔ **"58 skin material instances"** (X19, carried by X20) — retired. 58
(mesh,mat) PAIRS over 11 distinct objects, §2.

⛔ **`MILO_ENGINE_GPU_PLATFORM_SOURCES_DC3`** (charter + X20 §3.4) — does not
exist, §5.1.

⛔ **X18/X19/X20's "four cache flags" worktree recipe** — insufficient; six are
required, §7.1.

⚠ **X20 §3.4's prediction that binding the RT "may still give blank skin"** —
**correct in direction, understated in kind.** It is not that the RTs are blank;
it is that the compose that fills them is never dispatched, and cannot be hosted
by this backend when it is.

⚠ **Mine, named before the run and NOT confirmed:** I expected the blocker to be
`SyncObjects()` not running on the band members, per X20's handoff. It runs 50
times. Kept because it is why I built the positive indicator instead of a
targeted probe — a targeted probe would have found the first
`SetDeformation`'s `unk620=0` and "confirmed" X20.

---

## 11. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Engine CR-1 + CR-2** | §9. Written for a coordinator to act on. **CR-1 breaks rb3-xenon's link unless `x20_bandpatchmesh_link.cpp:107`'s stub is deleted in the same change** | coordinator |
| ★★★ **The shared `char_shared.milo` material** | §2. **Independent of the compose work and will survive it.** Visible body meshes use one shared material no member rebinds. The shipped mechanism (`MergeObject`'s `sCharSharedDir` → `ReplaceRefs`, `:2519`) is bypassed by the `FilterSubdir` shim (`:2586`). ⚠ **Full shim-OFF is a PROVEN DEAD END** — do not retry it blind | X22 |
| ★★ **Flip `RB3_X21_PRECLEAR_POLARITY_FIX` to default-ON** | §4. Only after CR-2 lands and the frame survives. The A/B is already wired | X22 / post-CR |
| ★★ **`Rnd::DrawPreClear`'s retail arm** | §3.3. Left byte-identical because the function **cannot be scored**. If a target body ever appears, the X360 arm is likely wrong the same way | a scored lane |
| ★★ **Six-flag worktree recipe** | §7.1. Correct the instruction inherited from X18 | next lane's setup |
| ★★★ **The 120 shared-skeleton publications** | §6: untouched for a third lane. Read `CHAR_SKINNING_DEFORM_INVESTIGATION.md` and its four dead ends first. ★ May share a root with §2 — **unmeasured** | X22 |
| ★★ **FULL rebind scope under a driven clip** | Unmeasured for an eighth lane. `--clips` takes an **ark-relative** path | X22 |
| ★★★ **Fix `FindBoneNamed` at its other call sites** | Carried from X19/X20 **untouched** | X22 |
| ⚠ **Geometric oracle with a reference pose**; **sample `dummy_torso.tex`'s texels** | Carried, untouched. ⚠ §2 changes the second one's value again: the visible meshes are on the shared material, so `dummy_torso.tex` **is** what they legitimately draw today | X22 |
| ⚠ **`ReProject`/`PreRender` counted stubs** | X20's counters still read 0 | its own lane |
| ⚠ **Widen `RB3_SYNCPROP_LOCAL_STATIC`** | X20 §1.2, untouched | its own lane |

---

## 12. Recommended X22 shape

1. ★★★ **A handoff's *static* reading can be right and its *inference* wrong.**
   X20 read the call graph correctly and inferred the path was dead. It was
   live. **Instrument the link you are about to reason about.** §1
2. ★★★ **X20 named the rule and did not apply it.** It warned that a
   failure-only predicate proves nothing, then reasoned from a call-graph
   argument with no runtime evidence. Writing the rule down is not applying it.
   §1
3. ★★★ **Names are not identities — sixth time.** "58 skin materials" was 11
   objects; `torso_naked.mat` is six different objects. Print the pointer. §2
4. ★★★ **Separate "never called" from "called over an empty set" in the same
   probe.** They have different owners. One extra `printf` turned a dead end
   into a located defect. §3.1
5. ★★ **A fix that works and regresses the frame is still a finding — ship it
   opt-in, not not-at-all.** The ON arm is this lane's strongest evidence about
   the backend. §4
6. ★★ **Check the variable name in the charter.** `_DC3` did not exist. Names
   inherit as silently as numbers. §5.1
7. ★★ **A gate that PASSes is not a gate that ran.** 15/18 with 3 SKIPs, rc=0,
   on the three targets that mattered. The 0-SKIP rule caught it. §7.1

---

## 13. Evidence

All under `/home/free/tmp/laneX21/evidence/`.

| file | what it shows |
|---|---|
| `x21-trace-r1.log` | ★★★ **the refutation** — `SyncOutfitConfig` 336 calls, 42 `sym='skin'`, `SetSkinTextures` 42 calls on 4 members — §1 |
| `x21-trace-r2.log` | ★★★ **the identity census** — 58 pairs / **11 distinct RndMat**, and the shared `char_shared.milo` material — §2 |
| `x21-trace-r3.log` | `DrawPreClear` 0 calls, `Compose` 0 calls, with 734 positive-control lines — §3.1 |
| `x21-trace-r4.log` | ★★★ `Rnd::DrawPreClear ENTER … mPreClearDraws=0 mDraws=40 listUsed=0` — §3.1 |
| `x21-fix-r1.log` | ⛔ **the opt-in arm** — dispatch repaired, frame dead, WebGPU pass-nesting error — §4 |
| `x21-base-r1.log` | baseline reproduction of X20's artifact before any edit |
| `x21-final-r{1,2}.log` | determinism ×2 |
| `x21-A-default-club.png` | **opened** — venue lit, crowd textured, four pink members. Byte-identical to X20's — §8.1 |
| `x21-B-polarityfix-BLACK.png` | **opened** — the entirely black opt-in frame — §8.1 |
| `x21-C-trace-club.png` | the identity-census run's frame |
| `x21-gate-baseline.log` | branch-point health **and** the 3-SKIP gate defect — §7.1 |
| `x21-gate-final.log` | native gate PASS 18/18 fresh, 0 SKIPs |
| `x21-configure.log` | the six-flag configure, no auto-disable — §7.1 |
