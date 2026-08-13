# `BaseMaterial` does not exist in RB3 retail — the two-class split is a DC3 refactor

Lane BASEMAT-1, 2026-08-13. Settles the question lane MAT-1 (`32f4fdb1`) flagged
as out of scope: *"retail also lacks `.?AVBaseMaterial@@` RTTI."*

**VERDICT: the `BaseMaterial` / `RndMat` split is a DC3-era refactor that RB3
retail does not have. Retail has ONE material class, `RndMat`, deriving directly
from `Hmx::Object`.** MAT-1's flag is CONFIRMED — but *not* for the reason it
gave, and the reason matters (see "why RTTI absence alone was insufficient").

Nothing was merged. This lane was scoped to settle + census + hand off.

---

## Why RTTI absence alone was NOT sufficient — and what actually discriminated

`CLAUDE.md` records that RTTI **type-name strings are not a `/GR` test** (`/GR-`
drops `??_R4` 44→0 but `??_R0` only 73→40, because EH also emits them), and a
**pure-abstract or never-instantiated base can legitimately lack a `??_R4` COL**
while still existing. Retail has 2,220 COLs; a class never constructed as a
complete object need not appear among them. So `BaseMaterial: no COL found` is
**consistent with both hypotheses** and settles nothing on its own.

What settles it is a *different* RTTI artifact with different emission rules:
the **`??_R2` base-class array** reached from a **derived** class's `??_R3`
ClassHierarchyDescriptor. That array is emitted **completely and
unconditionally** — it is what `dynamic_cast` walks — so an abstract,
never-instantiated, or private base still gets a `??_R1` + `??_R0` entry there.
A base cannot be in the hierarchy and absent from the derived class's array.

```
$ python3 tools/retail_rtti.py class DxMat
COL @ 0x821ea730  CHD@0x821ea744  numBaseClasses=5
   [0] .?AVDxMat@@       ncb=4 PMD(m=0,p=-1,v=0)
   [1] .?AVNgMat@@       ncb=3 PMD(m=0,p=-1,v=0)
   [2] .?AVRndMat@@      ncb=2 PMD(m=0,p=-1,v=0)
   [3] .?AVObject@Hmx@@  ncb=1 PMD(m=0,p=-1,v=0)
   [4] .?AVObjRef@@      ncb=0 PMD(m=0,p=-1,v=0)
```

★ **The `ncb` (numContainedBases) chain is 4,3,2,1,0 — gapless and
arithmetically complete.** `RndMat` contains exactly **2** bases: `Hmx::Object`
and `ObjRef`. An intermediate class between `RndMat` and `Hmx::Object` would
make `RndMat`'s `ncb` 3 and the array 6 entries long. **There is no room for an
intermediate under ANY name** — which also forecloses the obvious escape hatch
"maybe retail called it something else", without depending on a string at all.

`NgMat` (4 bases) and `DxMat` (5) prove intermediates *are* listed when they
exist. The test could have failed and did not.

### Corroborating strands (each weaker alone; all agree)

| # | evidence | value |
|---|---|---|
| 1 | `??_R2` base-class array, gapless `ncb` | **DECISIVE** (above) |
| 2 | One implementation of each material virtual | **DECISIVE** (below) |
| 3 | `ObjPtr<RndMat>` + `ObjPtrList<RndMat>` type descriptors exist; **no** `ObjPtr<BaseMaterial>` | positive ID of `mNextPass`'s type |
| 4 | literal `BaseMaterial` occurs **0×** in the 14 MB image | corroboration only |
| 5 | `REGISTER_OBJ_FACTORY(BaseMaterial)` needs the string `"BaseMaterial"` (0 hits) ⇒ retail cannot register such a factory | corroboration |
| 6 | rb3-Wii (RB3-**era** oracle) has `class RndMat : public Hmx::Object`, no `BaseMaterial.h`/`MetaMaterial.h`; DC3 (newer) has all three | corroboration, labelled |

Strand 4 is the one MAT-1 leaned on. It is real (positive controls fire:
`RndMat` 7, `NgMat` 1, `DxMat` 1, `RndTex`, `RndDrawable`, `Object@Hmx`; and
`RndFontBase` 0 reproduces the known `FontBase` precedent) — but it is
corroboration, not proof, and this lane does not rest on it.

### Evidence class 2: one virtual implementation, not two

Retail vtables `RndMat` @`0x8206571c`, `NgMat` @`0x82075294`, `DxMat`
@`0x8210315c` are each **21 slots**, and slots **6–10 are byte-identical across
all three**:

| slot | retail address | what our map calls it |
|---|---|---|
| 6 `Handle`       | `0x82438138` | `?Handle@RndMat@@` (pinned in `Mat.cpp`) |
| 7 `SyncProperty` | `0x82436488` | *(pinned inside `MetaMaterial.cpp`'s range)* |
| 8 `Save`         | `0x82435dc0` | `?Save@BaseMaterial@@` (pinned in `BaseMaterial.cpp`) |
| 9 `Copy`         | `0x82438c28` | unnamed |
| 10 `Load`        | `0x82438f40` | unnamed |

Retail has **one** `Handle`, **one** `SyncProperty`, **one** `Save`, **one**
`Copy`, **one** `Load` for the entire material hierarchy. Our tree emits
**fifteen** — `BEGIN_SAVES(BaseMaterial)`, `BEGIN_SAVES(RndMat)`,
`BEGIN_SAVES(MetaMaterial)` and likewise for the other four. Ten of those
fifteen have no retail counterpart.

⇒ **Our `BaseMaterial` *is* retail's `RndMat`.** `?Save@BaseMaterial@@` matches
100% at 988 B *because* it is retail's one material `Save`. Our `RndMat::Save`
(3 lines, `SAVE_SUPERCLASS(BaseMaterial)`) is surplus DC3 scaffolding.

⚠ The slot **count** (21) is *consistent* with the merge but does **not**
discriminate — a split whose derived class adds no new virtuals also yields 21.
Only the one-vs-two *implementation* count discriminates. Slots 0/3/5/18
resolving to `ModalKeyListener` / `TrackPanelDirBase` / `DanceRemixer` /
`XShaderPDBBuilder_AddRef` are ICF fold-aliases of trivial bodies — expected per
this repo's ICF finding, and evidence of nothing.

### What was ruled OUT, explicitly

- **`sizeof` did not discriminate.** After MAT-1, `sizeof(BaseMaterial) ==
  sizeof(RndMat) == 396` **by construction** (single inheritance, derived adds no
  members), so retail's `li r3, 396` at the material factory (`0x8240f5d0`,
  extent 84, then `bl 0x82438398`) is consistent with *both* hypotheses. It
  settled MAT-1's question, not this one. Its only contribution here is that
  retail has **one** factory where we emit two.
- **Absence of a `??_R4` COL for `BaseMaterial`** — insufficient, per above.
- **Oracle agreement** — DC3 is newer and rb3-Wii has holes; strand 6 is stated
  as corroboration. It does agree on a *specific structural prediction*
  (`RndMat : Hmx::Object`, ncb=2) rather than vaguely, which is worth something,
  but the verdict rests on retail bytes.

### Instrument provenance

`tools/retail_rtti.py`, reused not rebuilt (its header exists precisely because
three lanes re-derived this and two baked in the wrong `.data` VA skew, producing
**false absences shaped like decisive negatives** — the exact failure mode this
lane was most exposed to). `--selftest` **8/8**; `--selftest --sabotage naive-va`
drops to **2/8**, so the screen is demonstrably falsifiable. All binary scanning
done in Python — never `grep`, which is binary-blind here.

---

## Census — for the lane that executes the merge

**Map rows carrying any `BaseMaterial` mangled form: 6** (full-form matched, not
substring). Of those, **5 are at `mpn` 100**, totalling **1,532 B**:

| addr | symbol | unit | mpn / fuzzy | size |
|---|---|---|---|---|
| `0x824a8db0` | `?Queue@RndSoftParticleBuffer@@…W4Blend@BaseMaterial@@@Z` | `SoftParticleBuffer` | 100 / 100 | 188 |
| `0x8240f5d0` | `?NewObject@BaseMaterial@@SAPAVObject@Hmx@@XZ` | `rndobj/Rnd` | 100 / 99.76 | 84 |
| `0x82435858` | `??1?$ObjRefConcrete@VBaseMaterial@@VObjectDir@@@@UAA@XZ` | `BaseMaterial` | 100 / 100 | 100 |
| `0x82435dc0` | `?Save@BaseMaterial@@UAAXAAVBinStream@@@Z` | `BaseMaterial` | 100 / 100 | 988 |
| `0x824e2c20` | `?SetMatColorFlags@@…W4ColorModFlags@BaseMaterial@@…` | `Crowd` | 100 / 100 | 172 |
| `0x82435608` | `?SetObjConcrete@?$ObjRefConcrete@VBaseMaterial@@…@Z` | `BaseMaterial` | 56.69 / 55.73 | 104 |

Compare the two precedents: a lane that found **224 rows / 208 at 100% /
19,464 B** correctly **REFUSED**; a lane that found **1 row per symbol, none at
100%** correctly **PROCEEDED**. This sits far closer to the second — and unlike a
one-sided rename, all 6 can be updated **in lockstep** with our class rename, so
pairing is preserved and Δmatched should be ≈0.

**Source / symbol blast radius:**

- **121** distinct emitted symbols carry `BaseMaterial`, across 15+ objects
  (`BaseMaterial.obj` 80, `Mat.obj` 48, `MetaMaterial.obj` 43, `Shader.obj` 30…).
- **53 objects** carry a *nested-enum* mangled name — `Blend` and `ColorModFlags`
  are nested in `BaseMaterial`, so the owning class leaks into every signature
  taking one. `?SetBlend@RndMat@@QAAXW4Blend@BaseMaterial@@@Z` alone appears in
  **50 objects**. This is the widest ripple and the main recompile cost.
- Subclasses affected: `RndMat` (→ merges away), `MetaMaterial` (`: BaseMaterial`).
- We emit `??_R0/R1/R2/R3/R4 BaseMaterial` and the string literal
  `??_C@_0N@PIEAOGAC@BaseMaterial?$AA@`; retail has **none** of them, and our
  `??_R3RndMat@@8` therefore claims `numBaseClasses=4` where retail says **3**.

**Worth / risk.** The merge is **layout-neutral** — single inheritance with no
added members already yields byte-identical objects — so it buys no bytes
directly and is unlikely to move `matched_functions` if the 6 rows move in
lockstep. What it buys is **accuracy**: a correct hierarchy, correct emitted
RTTI, deletion of 10 surplus virtuals with no retail counterpart, and removal of
a string retail does not contain. Under the standing directive that *accuracy
beats headline %* and *a metric that hides real bugs is worse than a lower
metric*, that is worth doing — but it is a correctness play, not a points play,
and should be briefed as one.

## ⚠ Flagged, NOT verified by this lane

`MetaMaterial.cpp` is pinned to `.text` `0x82436488–0x82438138` and
`0x824382D4–0x8243833C`, and `0x82436488` is **retail `RndMat` vtable slot 7**.
Since `MetaMaterial` has zero string and zero RTTI presence in retail, that pin
cannot be covering `MetaMaterial` code — yet `default/MetaMaterial` reports
**53 of 67 functions matched**. The bodies genuinely match retail bytes (same
engine lineage); the *class attribution* is ours. Combined with the pins for
`BaseMaterial.cpp` (`0x82435528–0x8243619C`) and `Mat.cpp`
(`0x824361E0–0x82436278`, `0x82438138–0x824382D4`) interleaving into one
contiguous region ~`0x82435520–0x82439000`, this reads as **one retail material
TU carved into three of our units** — consistent with `/O1` preserving TU
spatial grouping. That is a hypothesis with good support, not a settled finding;
it needs its own lane and its own controls before anyone acts on it.

Unit figures above are read from a `report.json` dated **Aug 13 06:46**, i.e.
*before* MAT-1 landed at 18:50 (it still shows `BaseMaterial` 23, MAT-1's A/B
ended at 22). Ruler `name_check`, read from `provenance`. Treat them as
indicative magnitudes, not current absolutes.

## ⛔⛔ The rename must NOT be propagated into `milo-native-engine` (lane ENGINE-1, 2026-08-13)

ENGINE-1 was commissioned to finish this rename in the shared engine
(`../milo-native-engine`), bump `MILO_ENGINE_PIN`, and delete the
`add_compile_definitions(BaseMaterial=RndMat)` bridge in `native/CMakeLists.txt`.
**It measured the rename and it BREAKS `dc3-decomp`. Do not do it.** The bridge
stays; the lane's own footprint was the corrected comment on that define.

**The premise was false.** `BaseMaterial` in the engine is not a leftover spelling
of `RndMat` — it is DC3's *real base class*, and DC3 has **two** subclasses of it,
so `RndMat` and `MetaMaterial` are **siblings** there and parent/child here:

| | dc3-decomp | rb3-xenon (post-BASEMAT-2) |
|---|---|---|
| base | `BaseMaterial : Hmx::Object` | — (merged away) |
| | `RndMat : BaseMaterial` | `RndMat : Hmx::Object` |
| | `MetaMaterial : BaseMaterial` | `MetaMaterial : RndMat` |
| `NextPass()` | returns `BaseMaterial *` | returns `RndMat *` |
| `mNextPass` | `ObjPtr<BaseMaterial>` | `ObjPtr<RndMat>` |

Because DC3's `NextPass()` returns the *base*, renaming the receiver to `RndMat*`
is a base→derived narrowing with no implicit conversion.

**Measured, not inferred.** DC3's own compile command for each engine TU was
extracted from its `build.ninja` and re-run `-fsyntax-only` (non-mutating — never
writes into dc3-decomp's build dir). Baseline **5/5 OK**; with a naive rename
applied to a scratch copy of one TU:

```
Mesh_Wgpu.cpp:297:13: error: cannot initialize a variable of type 'RndMat *'
                             with an rvalue of type 'BaseMaterial *'
Mesh_Wgpu.cpp:329:30: error: incompatible pointer types assigning to 'RndMat *'
                             from 'BaseMaterial *'
```

⚠ The instrument was validated in **both** directions before being trusted: its
first two revisions returned 0/5 for reasons that had nothing to do with the
change (a stale `cmake_pch.hxx.pch`, then a 4-token `-Xclang -include-pch -Xclang
<path>` group that a 2-token strip left dangling as a *source input*). A gate that
fails on everything blocks exactly as convincingly as one that passes on
everything.

⚠ **`site 329` was invisible to text search** — the variable is bare `nextPass`
there. Any future census of this class must be compiler-driven, not `grep`-driven.

### Why the other two consumers land differently

- **rb3 (Wii): unaffected.** It builds `MILO_ENGINE_GPU_BACKEND=rb3`, which drops
  all 8 affected sources (they are in `MILO_ENGINE_GPU_PLATFORM_SOURCES` /
  `MILO_ENGINE_GFX_RNDOBJ_SOURCES`, both `dc3`-only) and substitutes
  `Rnd_Wgpu_RB3.cpp` + `RB3MaterialBinder.cpp`. Its `src/` contains **zero**
  occurrences of `BaseMaterial`.
- **dc3-decomp: immediately exposed.** It does
  `add_subdirectory(${MILO_ENGINE_PATH})` against the engine **working tree**, and
  its `MILO_ENGINE_PIN` is only a `message(WARNING)` — never a fetch. So an engine
  edit reaches DC3 on its next configure regardless of its (stale, `77eb428b`) pin.
  The soft pin is a notification, **not** an isolation mechanism.

### Scope correction

The bridge comment claimed 4 engine files. The real scope is **8 sources + 2 test
files**: `platform/{MaterialSetup.h,MaterialSetup.cpp,UiRenderHeuristics.h,`
`Rnd_Wgpu.cpp,TransparentQueue.cpp,Mesh_Wgpu.cpp}`, `gfx/{ShadowPass.cpp,`
`PipelineManager.h}`, plus `tests/test_rndcam_projection.cpp` and
`tests/dc3_runtime_sources.cmake`. Of these:

- `gfx/PipelineManager.h` is a **comment** only.
- `tests/dc3_runtime_sources.cmake` is a **file path** (`BaseMaterial.cpp`), which
  exists under that name in *both* trees — nothing to change.
- the `#include "rndobj/BaseMaterial.h"` lines need **no** change either: BASEMAT-2
  deliberately kept the filenames (they are decomp unit-boundary artifacts, and
  renaming them would churn `config/45410914/splits.txt`).
- the engine already spells concrete materials `RndMat*` (`mesh->Mat()` —
  `Mesh_Wgpu.cpp:31/154/173`) and reserves `BaseMaterial*` for genuine base-typed
  values. **The distinction is load-bearing, not sloppy.**

### CHANGE REQUEST (for the coordinator — not executed by this lane)

Replace the global token rewrite with a scoped per-consumer typedef. New
`milo-native-engine/src/platform/MaterialBase.h`:

```cpp
#pragma once
#include "rndobj/BaseMaterial.h"   // this FILENAME exists in both consumers
#ifdef MILO_ENGINE_MATERIAL_BASE_IS_RNDMAT
using MiloMatBase = RndMat;        // RB3 retail: one material class
#else
using MiloMatBase = BaseMaterial;  // DC3 (default): BaseMaterial is the shared base
#endif
```

Then spell the engine's material-base sites `MiloMatBase` — the two
`BuildPassMaterialParams` declarations and `Mesh_Wgpu.cpp:297`/`:329` — and the
`Blend` enum qualifiers `MiloMatBase::kBlend*`. Defaulting to DC3's spelling means
**dc3-decomp and rb3 (Wii) need no change at all**; rb3-xenon swaps
`BaseMaterial=RndMat` for `MILO_ENGINE_MATERIAL_BASE_IS_RNDMAT`.

The win is real but narrow: it trades a **global identifier rewrite applied to
every TU in the native build** for a scoped, type-safe alias. It does *not* make
the define disappear.

⚠ Two verification requirements, and they are why ENGINE-1 did not execute it:

1. `RndMat::kBlend*` is **not** a drop-in for the enum sites. In DC3
   `rndobj/BaseMaterial.h` declares only `BaseMaterial`; `RndMat` lives in
   `rndobj/Mat.h`. `MiloMatBase::kBlend*` avoids this; a bare `RndMat::` does not.
2. `tests/` only builds when `MILO_ENGINE_BUILD_TESTS=ON` (standalone default;
   rb3-xenon forces it OFF, DC3 and rb3-Wii never set it). Touching
   `test_rndcam_projection.cpp` therefore needs the engine's **own standalone test
   build** verified — a third build beyond DC3's and rb3-xenon's.
