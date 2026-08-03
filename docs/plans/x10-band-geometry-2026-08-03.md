# X10 — the band's geometry was mostly already there; the probe that said otherwise was reading the wrong array

**Date:** 2026-08-03
**Predecessor:** [X9](x9-band-marks-2026-08-03.md) "the band renders on its shipped marks; the wall was a guard copied from a container it doesn't describe"
**Branch:** `x10-band-geometry`, rebased onto `main` @ `fac3e802`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** ONE native driver file (`native/src/main_render.cpp`) + two new tools. **Zero shared `src/` files. Zero X360 units.**

---

## Verdict

★★★ **X9'S HEADLINE GAP WAS AN INSTRUMENT ARTIFACT FOR SIX OF ITS NINE MESHES.**
The probe asked `NumVerts() > 0`, which reads a mesh's *own* `mVerts` — but the
native loader deliberately empties `mVerts` and parks the shipped blob in
`mCompressedVerts`. The hair X9 called empty carries **2348 vertices and 3012
faces**. Corrected, a member reports **SHOWN-BUT-EMPTY 3, not 9**, and
**DRAWABLE 16–26, not 5–9**. §1

★★ **THE `OutfitConfig` WALL IS NOT WHAT X9 SAID IT WAS, IN EITHER HALF.** Its
stated cause — "`BandPatchMesh.cpp` is scatter-included into `LightPreset.cpp`,
which is not in `rb3-render`'s source list" — is refuted twice: `LightPreset.cpp`
**is** in the source list (via `file(GLOB world/*.cpp)`), and `BandPatchMesh.cpp`
is listed **directly** at `native/CMakeLists.txt:1227`. The real cause is that
`BandPatchMesh.cpp` is a **191-line partial port** whose six methods are declared
in the header and defined nowhere. §3

★★ **THE SCATTER-INCLUDE AUDIT IS DONE, AND DIRECTION B IS REAL.** `rb3-milo`
and `rb3-render` each compile **zero** TUs from **8 modules** yet link **20
files** from them — including five **Direct3D9** renderer TUs into a target that
renders through **WebGPU**. Nobody chose that; an X360 packing decision did.
Instrument validated by a positive control that reproduces **109/109** of the
build's own pruning decisions with zero disagreements. §2

★ **X9's "34 FULL-BUT-HIDDEN are correct" HOLDS — but the number was 140, and
the reason is better than X9's.** The hidden set is the *entire both-gender
wardrobe option catalogue*, and the gender flip X9 asked for is directly
observed. §4

⚠ **WHAT DID NOT LAND: `OutfitConfig` IS STILL NOT REGISTERED, AND THE THREE
GENUINELY-EMPTY MESHES ARE STILL EMPTY.** I did not fix either. I did establish,
with a controlled comparison, that **they are not the same problem** — which is
why the next lane should not spend itself on the link wall expecting geometry. §5

★ **X6's SHA table is vindicated a FOURTH time**, independently, both
instruments agreeing. §6

---

## 1. ★★★ The probe was wrong, and it set two lanes' charters

### 1.1 What X9 handed over

> Nine **SHOWN-BUT-EMPTY** meshes per member — `head`, `eyes`, `tongue`,
> upper/lower teeth, `hands_naked`, `fingernails`, `eyebrows`, hair — selected
> correctly by the recompose but carrying **zero vertices**.

Reproduced first, before touching anything
(`x10-run-club-verbose.log`): `rc=0`, `Can't make OutfitConfig` **×40**, 411
meshes / 203 draws / 38.92 % coverage — X9's numbers to the digit.

### 1.2 The disproof was already in X9's own log

`main_render.cpp` (X9's diagnostic) asked:

```cpp
bool nv = cm[j]->NumVerts() > 0;
```

`Mesh.h:203` is `int NumVerts() const { return mVerts.size(); }` — **this**
mesh's own `mVerts`, with **no `mGeomOwner` indirection**, unlike `Verts()`
(`:240`) and `NumCompressedVerts()` (`:262`), which both do indirect. And
`RndMesh::LoadVertices`'s `HX_NATIVE` arm (`Mesh.cpp:1705-1739`) *deliberately*
does `mVerts.resize(0)` and parks the shipped blob in `mCompressedVerts`
whenever the per-mesh compression flag is set — which is exactly the population
skinned character meshes belong to.

⛔ **The positive control was sitting in the same log the whole time.** The
venue's own `stage.mesh` reports `verts=0 cverts=140`, and the stage is visibly
rendered in **every frame X6–X9 shipped**. `verts == 0` plainly does not mean
"not drawn".

### 1.3 The quantitative test — an aggregate the two worlds could NOT both produce

Parsed from the per-mesh census, against the renderer's own draw counter:

| predicate | count |
|---|---|
| `showing && NumVerts()>0` | **30** ← X9's `DRAWABLE` |
| `showing && (verts>0 \|\| cverts>0)` | **219** |
| `showing && faces>0` | 237 |
| **renderer actually issued** | **203 draws** |

X9's predicate undercounts real draws by **~7×**. Nothing else in the
measurement chain is off by 7×.

### 1.4 The nine, individually

| mesh | `cverts` | `faces` | verdict |
|---|---|---|---|
| `youngozzie_resource` (hair) | **2348** | **3012** | ⛔ artifact — full geometry |
| `lowerteeth` | 290 | 510 | ⛔ artifact |
| `upperteeth` | 276 | 502 | ⛔ artifact |
| `fingernails_resource` | 286–350 | 200–300 | ⛔ artifact |
| `eyes` | 194 | 288 | ⛔ artifact |
| `tongue` | 58 | 94 | ⛔ artifact |
| **`head`** | **0** | **0** | ✅ genuinely empty (33–39 bones) |
| **`hands_naked`** | **0** | **0** | ✅ genuinely empty (38–40 bones) |
| **`eyebrows*_resource`** | **0** | **0** | ✅ genuinely empty |

### 1.5 Result, measured after the fix

| metric (per member) | X9 | **X10 corrected** |
|---|---|---|
| SHOWN-BUT-EMPTY | 9 | **3** |
| `verts>0` | 39–44 | **157–166** |
| DRAWABLE | 5–9 | **16–26** |

⛔ **NOT A RENDER CHANGE — A MEASUREMENT CHANGE.** The band frame is
`cmp`-identical before and after (§6). Nothing was made to draw that was not
already drawing; the count was wrong, not the picture.

---

## 2. ★★ The scatter-include audit (the charter's priority item)

`~250` `#include "*.cpp"` edges exist to reproduce retail COMDAT placement for
objdiff **scoring**. The X360 build never links, so a scatter edge is
structurally invisible there. The native build is the only place one surfaces.

### 2.1 The instrument, and why its results are usable

`tools/scatter_audit.py` mirrors `native/cmake/ScatterIncludes.cmake`'s exact
state machine (unconditional edges only, includer-dir-first resolution) and
takes each target's compiled set **from the ninja graph** — post-prune ground
truth, not the pre-prune `CMakeLists` list.

★ **Positive control.** The configure log records every file the build itself
declined to compile standalone. The instrument reproduces **all 109 of those
decisions across 17 targets with ZERO disagreements**. A negative from it is
evidence, not silence.

### 2.2 ⛔ DIRECTION B — a target linking code it never asked for. REAL.

Decidable from the graph: a target that compiles **zero** TUs of module *M*
standalone, yet links files from *M* dragged in by a scatter host.

| target | module | compiles | **links** | via |
|---|---|---|---|---|
| `rb3-milo` / `rb3-render` | **`src/system/rnddx9`** | 0 | **5** — `Cam`, `CubeTex`, `Lit`, `MultiMesh`, `Part` | `rndobj/CubeTex.cpp:284` |
| `rb3-milo` / `rb3-render` | `src/system/hamobj` | 0 | 5 — `FilterQueue`, `HamMove`, `HamSupereasyData`, `Pose`, `StarsDisplay` | `rndobj/{Morph,PartAnim,PropKeys}.cpp`, `ui/UIListSlot.cpp` |
| `rb3-milo` / `rb3-render` | `src/system/flow` | 0 | 4 | `rndobj/EventTrigger.cpp`, `ui/UI.cpp` |
| `rb3-milo` / `rb3-render` | `src/band3/bandtrack` | 0 | 2 — `GemManager`, `Tail` | `char/CharBonesMeshes.cpp`, `rndobj/Font.cpp` |
| `rb3-milo` / `rb3-render` | `src/band3/game` | 0 | 1 — `Stats` | `rndobj/EventTrigger.cpp` |
| `rb3-milo` / `rb3-render` | `src/band3/meta_band` | 0 | 1 — `AppLabel` | `rnddx9/CubeTex.cpp` |
| `rb3-milo` / `rb3-render` | `src/system/gesture` | 0 | 1 — `SkeletonClip` | `rndobj/EventTrigger.cpp` |
| `rb3-milo` / `rb3-render` | `src/system/midi` | 0 | 1 | `char/CharCollide.cpp` |

**8 modules, 20 files, both milo targets.** The `rnddx9` row is the sharpest:
`rb3-render` renders through the **dc3 WebGPU** backend and compiles the
**Direct3D9** renderer anyway. `rndobj/CubeTex.cpp.o` contains **105 `Dx*::`
symbols**, including `DxCubeTex::Select` calling `D3DDevice_SetTexture`, from
a file that `#include`s `xdk/D3D9.h`.

⚠ **Stated precisely, because it changes the severity:** those 105 symbols are
**compiled but not in the final binary** — `nm -C build/rb3-render | grep 'Dx.*::'`
returns **0**, because the targets build with `-ffunction-sections -fdata-sections`
and link with `--gc-sections`. So this is a **latent** defect and a build-time
cost, **not** an active runtime one. `--gc-sections` is therefore load-bearing
in a way nothing documents; the object even carries `U DxMultiMesh::DrawShowing()`
and `U DxMultiMesh::sVertexDecl`, which would be hard link errors if anything
ever retained that code.

⛔ **I initially wrote "89 D3D symbols are in the binary." That was wrong and I
retract it** — sampling showed the matches were hex *addresses* containing
`d3d` (`000000000078d3d0`). The charter's "sample the matches before trusting a
count" caught my own instrument, one step after it caught X9's.

### 2.3 ⚠ DIRECTION A — NOT decidable from the graph, and I did not pretend it was

"Missing" is a **demand** property. Only the link can answer it, and every
target links today. My first cut emitted **2110 rows** by flagging every guest
not reachable in every target; that number is meaningless and is **discarded,
not reported**.

What *is* decidable is the strictly weaker predictor, and it is labelled as one:

> **42 scatter-guest files reach NO native target at all** — not compiled
> standalone anywhere, and every unconditional host of theirs is absent from
> every target. Each will produce "undefined reference" the moment anything
> references it.

That is the shape of wall X9 hit, generalized. Full list in
`x10-scatter-report.txt`; it is dominated by `src/band3/meta_band` (11),
`src/band3/game` (4), `src/system/hamobj` (5).

⛔ **`BandPatchMesh.cpp` appears ZERO times in the entire audit report** — not
an orphan, not a Direction-B item. It reaches the link. §3.

### 2.4 Multi-host guests — 22 duplicate-definition landmines, none currently live

22 files have **two or more** unconditional hosts (`obj/PropSync.cpp` has three;
`bandobj/BandWardrobe.cpp` three). In **no** current target are two hosts of the
same guest simultaneously live — which is why everything links, and is X4b's
807-duplicate incident (`EventTrigger` 313 / `Font` 244 / `CharIKScale` 162)
seen from the other side. Adding any listed non-source host to a target arms it.

### 2.5 ★ The general lesson, corrected

X9 proposed: *"an X360 match-build packing choice silently decides what the
native link contains."* The audit says this is **half right, and the wrong
half is the one that was actioned.**

- Packing choices **do** silently decide what a native target links — **but in
  Direction B** (20 files of unwanted code), which nobody had noticed.
- In **Direction A** the machinery is *working*: `ScatterIncludes.cmake` derives
  the prune per target and got all 109 decisions right. The `OutfitConfig` wall
  was never a packing problem.

⚠ **`ScatterIncludes.cmake` is more capable than the comments around it claim.**
`native/CMakeLists.txt:1160-1165` says the module "does NOT follow edges out of
a file that is not itself a target source," and hand-lists
`_MILO_SCATTER_TRANSITIVE_PRUNE CharClip Character` to compensate. The module's
`rb3_scatter_prune` **is** transitive — it appends every resolved includee to
`_queue` unconditionally. The comment is **stale**; the hand-list is probably
now redundant. **Not touched here** — removing it is a source-list change to a
module all 18 targets share, and it needs its own A/B.

---

## 3. ★★ What `OutfitConfig` actually took — and why X9's cause was wrong twice

### 3.1 The stated cause, refuted on two independent grounds

> `BandPatchMesh.cpp` is not compiled standalone. It is scatter-included into
> `LightPreset.cpp:1503` … and `LightPreset.cpp` is not in `rb3-render`'s
> source list (only `LightPresetManager.cpp` is).

1. ⛔ **`LightPreset.cpp` IS in the source list.** `ENGINE_WORLD` is
   `file(GLOB ${REPO_ROOT}/src/system/world/*.cpp)` (`native/CMakeLists.txt:1039`),
   and `LightPreset.cpp` is in `src/system/world/`.
2. ⛔ **`BandPatchMesh.cpp` is listed DIRECTLY** at
   `native/CMakeLists.txt:1227`, added by X7 — i.e. *before* the X9 doc.

And the build says so in its own voice (`x10-configure.log`):

```
[scatter] rb3-render: not compiling src/system/bandobj/BandPatchMesh.cpp standalone (emitted by a scatter-include)
[scatter] rb3-render: not compiling src/system/world/LightPreset.cpp       standalone (emitted by a scatter-include)
```

Both are pruned because both are *emitted*, through
`ui/UIListDir.cpp → world/LightPreset.cpp → bandobj/BandPatchMesh.cpp`. The
code is in the link. Adding `LightPreset.cpp` to the target would have supplied
nothing.

### 3.2 The real cause

`src/system/bandobj/BandPatchMesh.cpp` is **191 lines** and defines four
symbols. Its own header comment says so: *"Only the worklist target functions
and the helpers required to compile + emit them are ported here."* The six
methods are declared at `BandPatchMesh.h:99-105` and **defined nowhere in
rb3-xenon**. rb3-Wii's equivalent is **1511 lines** and defines all six.

### 3.3 The exact bill, measured

Adding `OutfitConfig::Init()` and building: `rc=1`, **48 distinct undefined
symbols** (`x10-undef.txt`), in two clean classes:

| class | count | what |
|---|---|---|
| `BandPatchMesh::*` | **11** | the 6 methods + ctor, copy-ctor, `operator=`, `operator>>`, `PropSync` |
| **bare globals** | **37** | 36 `Symbol`s (`recompose`, `two_color_mask`, `color1_palette`, `meshao`, `piercing`, …) + `bool gRB3OutfitComposeActive` |

★ **X9 named only ONE of the 37** ("the `Symbol recompose`"). The dominant class
by count is **X8's `Symbol`-globals class**, and it already has a home in the
tree — `native/src/m*_symbols.cpp` exists precisely because xenon ships
`Symbols*.h` headers and no `.cpp` (X9 §6 established this). Those 37 are
mechanical.

### 3.4 ⛔ Why I did NOT then go and fix it

The charter's ★ rule: *ask what the missing code would DO before planning to
write it.* Two independent investigations — my own runtime evidence and a
separate source trace across all three trees — agree:

**`OutfitConfig` supplies NO geometry.** It is `RndDrawable` (not an
`ObjectDir`) owning material swaps, two-colour palettes, texture blenders,
baked-AO **vertex-colour** coefficients, piercings and patch records. Every
write it makes to a mesh is a material binding, a vertex *colour*, or a
*displacement of already-existing verts*. `MeshAO::Apply` bails if the vert
count does not already match; `Piercing::Deform` returns immediately if the
head mesh has zero verts. It never allocates or copies vertices.

`BandPatchMesh` is a **tattoo / decal / face-paint projector** — `PreRender`
ray-casts a projector against the body mesh and builds *new transient*
`*_patch.mesh` objects; `Render` draws them into the outfit's render target.
Its geometry never lands in `head.mesh` or `hands_naked.mesh`.

Registering `OutfitConfig` would restore **textures, tattoos, skin composites,
the band logo** — all real and worth having. It would **not** put a vertex in
any of the three empty meshes.

---

## 4. ★ Does X9's "34 FULL-BUT-HIDDEN are correct" hold? YES — better than stated

**The verdict holds. The number does not.** X9's 34 came from the same broken
predicate; corrected it is **140–141 per member**.

And the composition is more reassuring than "tattoo/AO/placement decals": it is
the **entire both-gender wardrobe option catalogue** — `female_makeup_eyes_1..29`,
`female_makeup_lips_*`, `male_tattoo_*`, `female_tattoo_*`, `*_placement_*`,
`*_ao_seams`, `*_wrinkle_*`. A character milo ships every option and the
recompose shows one. **Hidden = unselected = correct by design.**

★ **The cheap gender check X9 asked for was run, and it passes.** `player0`
(male) selects `eyebrows1_resource`; `player1` (female) selects
`female_eyebrows_09_resource`; each member hides the other gender's decals.

⚠ **One caveat that weakens the mechanism, not the observation.**
`CharMeshHide::HideAll` — the function that actually sets `Showing()` on body
meshes — is an **inert stub** at `native/src/x7_band_stubs.cpp:95`, with no body
anywhere in `src/`. So the shown/hidden state is *what the milo authored plus
what the merge left*, not the outfit's live selection. The gender flip is real
as observed; X9's phrase "selected correctly **by the recompose**" is not
supported by the code in this tree.

---

## 5. ⚠ What did NOT land — stated plainly

1. ⛔ **`OutfitConfig` is still not registered.** The 48-symbol bill is measured
   and partitioned (§3.3) but not paid. The band still has no tattoos, no
   two-colour skin composite, no band logo.
2. ⛔ **`head.mesh`, `hands_naked.mesh`, `eyebrows*_resource.mesh` are still
   empty**, and **I did not find out why.** What I established is a *controlled
   comparison* that removes the leading suspect — see §5.1.
3. ⚠ **The Direction-B finding is reported, not fixed.** Nothing was changed in
   any source list; every such change is match-relevant or 18-target-wide.
4. ⚠ **`_MILO_SCATTER_TRANSITIVE_PRUNE` left in place** though probably
   redundant (§2.5).

### 5.1 ★ The controlled comparison that rules `OutfitConfig` out

The 40 `Can't make OutfitConfig` failures are **one per wardrobe-piece milo** —
torso, legs, head, feet, hair, facehair, hands, eyebrows, instruments. They fire
**uniformly**, on milos whose geometry loads *and* on milos whose geometry does
not:

| milo | `OutfitConfig` | mesh geometry |
|---|---|---|
| `char/main/hair/male/male_hair_youngozzy_resource.milo` | **fails** | `cverts=2348 faces=3012` ✅ |
| `char/main/torso/male/vestdenim_canvas.milo` | **fails** | loads ✅ |
| `char/main/head/male/head.milo` | **fails** | `cverts=0 faces=0` ❌ |
| `char/main/hands/male/male_hands_naked.milo` | **fails** | `cverts=0 faces=0` ❌ |

The hair milo is the **positive control**: identical failure, geometry loads
fine. A cause constant across both arms cannot explain the difference.

Two further disproofs, both of my *own* hypotheses:

- ⛔ **"`RndMeshDeform::Reskin` is stubbed, so the verts are never built."**
  Retracted by reading the oracle. `rb3/src/system/rndobj/MeshDeform.cpp:298-327`
  writes into an **already-sized** `mMesh->Verts()` and *aborts* if the counts
  differ. A stubbed `Reskin` leaves geometry alone; it cannot zero it.
- ⛔ **"the resource milo fails to load and takes the meshes with it."**
  Retracted: within the **same** `head.milo`, `eyes`/`tongue`/`upperteeth`/
  `lowerteeth` all load full geometry while `head.mesh` does not. Not a load
  failure.

**So `head.mesh` and `hands_naked.mesh` appear to be genuinely empty *as
shipped*** — proxy meshes carrying 33–40 bones, a material and no triangles,
whose geometry must be supplied at runtime by something not yet identified.
`BandHeadShaper.cpp` (402 lines vs rb3-Wii's 403) and `BandFaceDeform.cpp` (328
vs 323) are **fully ported and compiled**, so they are not the gap either.
**UNREACHED — named honestly rather than guessed at.**

---

## 6. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native gate, **fresh** (`rm -rf native/build`), rc=0, **0 SKIPs** | ✅ **PASS — 18/18** | `NATIVE GATE: PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`; all 18 *relinked this run*; `grep -c SKIP` → **0**. Cache seeded with **all four** flags per X9's §5.1 warning. `x10-native-gate.log` |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606a202f2b3226e38a8f28010b096f3d441`. ⚠ The foreign uncommitted edit to `src/platform/FxSendNative.cpp` disclosed by X4d/X5/X6/X7/X8/X9 is **still there, still not mine**, left untouched — **seventh lane running.** |
| c | Shared-`src/` edits X360-faithful | ✅ **VACUOUS — none made** | `git diff --name-only main..HEAD \| grep -c '^src/'` → **0**. My only source change is `native/src/main_render.cpp`, a native driver TU. **There is no objdiff position to report because no scoreable unit was touched** — that is a stronger statement than "unchanged", and it is why no X360 A/B was required. |
| d | PNG determinism ×2 on every cited image | ✅ **PASS** | both cited frames rendered ×2 → `cmp` identical; E1 re-rendered a third time after the fresh gate rebuild → still identical |
| e | Prior lanes' evidence non-regressed, vs **artifacts** | ✅ **PASS — byte-identical** | §6.1 |
| f | Was `main` broken by a decomp lane? | ✅ **NO** | baseline `rb3-render` at `9501caed` built `rc=0` before any edit; full 18/18 gate `rc=0` after rebasing onto `fac3e802` (which added lanes DM-2 and DM-3). |

### 6.1 Non-regression and the SHA table

★ The default frame is **byte-identical to X9's shipped artifact**, verified by
`cmp` against the PNG on disk — not against a transcribed hash — and **both my
instruments agree with each other and with the document**:

| frame | vs X9 artifact | sha256 (prefix) | X6's recorded value |
|---|---|---|---|
| default E0 (`small_club_01`, no `RB3_BAND_PLACE`) | **IDENTICAL** | `5282bd275159f10b` | `5282bd275159f10b` ✅ |

**X6's SHA table is now vindicated a FOURTH time.**

⚠ A trap worth recording: X9's "E0 default" is `small_club_01` rendered
*without* `RB3_BAND_PLACE`, **not** the two-cell no-arkPath run. Comparing
against the two-cell PNGs shows a mismatch that looks exactly like a
regression. I nearly reported one. **Re-measure the artifact before retracting
another lane's number** — the charter's rule, and it earned its place again.

★ The **band** frame is also `cmp`-identical before and after my change, which
is the point: the probe fix is diagnostic-only and moved **zero** pixels.

---

## 7. Per-subsystem verdict table

| subsystem | verdict | evidence |
|---|---|---|
| **`NumVerts()` probe correctness** | ★★★ **DEFECT FOUND + FIXED** | 30 vs 203 actual draws; `stage.mesh` positive control. §1 |
| **"9 SHOWN-BUT-EMPTY head/hands meshes" (X9)** | ⛔ **REFUTED for 6 of 9** | hair `cverts=2348 faces=3012`; eyes/tongue/teeth/nails all populated. §1.4 |
| **Band head/hands/hair geometry RENDERS** | ✅ **YES, LARGELY** | corrected DRAWABLE 16–26/member; hair, eyes, teeth, tongue, nails all drawing; head+hands+eyebrows do not. §1.5 |
| **`head` / `hands_naked` / `eyebrows` empty** | ⚠ **REAL, CAUSE UNREACHED** | `cverts=0 faces=0`; 3 meshes/member, not 9. §5.1 |
| **Scatter-include audit, Direction B** | ★★ **VERIFIED — 8 modules, 20 files, 2 targets** | incl. 5 D3D9 TUs into a WebGPU target. §2.2 |
| **Scatter-include audit, Direction A** | ⚠ **NOT DECIDABLE — predictor given instead** | 42 files reach no target; 2110-row first cut discarded as meaningless. §2.3 |
| **Audit instrument trustworthiness** | ★★ **POSITIVE CONTROL 109/109, 0 disagreements** | reproduces the build's own prune decisions. §2.1 |
| **X9's `OutfitConfig` root cause** | ⛔ **REFUTED on two independent grounds** | `LightPreset.cpp` IS globbed in; `BandPatchMesh.cpp` listed at CMakeLists:1227. §3.1 |
| **Real `OutfitConfig` cause** | ★★ **NAMED — 191-line partial port + 37 undefined globals** | 48 symbols, measured. §3.2–3.3 |
| **`OutfitConfig` supplies geometry?** | ⛔ **NO — materials/textures/AO-colours only** | two independent traces agree. §3.4 |
| **`OutfitConfig` REGISTERED** | ⛔ **STILL BLOCKED — did not land** | §5 |
| **"34 FULL-BUT-HIDDEN correct" (X9)** | ✅ **VERDICT HOLDS, count was 140 not 34** | both-gender option catalogue; gender flip observed. §4 |
| **`CharMeshHide::HideAll`** | ⚠ **INERT STUB — mechanism unsupported** | `x7_band_stubs.cpp:95`. §4 |
| **22 multi-host duplicate landmines** | ✅ **NONE CURRENTLY LIVE** | §2.4 |
| **`ScatterIncludes.cmake` transitivity comment** | ⚠ **STALE — module IS transitive** | `_MILO_SCATTER_TRANSITIVE_PRUNE` likely redundant; not touched. §2.5 |
| **X360 blast radius** | ✅ **ZERO — no shared `src/` file touched** | not scoreable because not touched. Gate c |
| **Venue / crowd / lighting / band placement** | ✅ **ALIVE, non-regressed** | byte-identical to X9's artifact. §6.1 |
| **`BandCamShot` (611); 18 stubbed bodies; `band.play_mode`; `RB3_BAND_PLACE` opt-in; foreign `FxSendNative.cpp` edit** | ⚠ **CARRIED, untouched** | as before |

---

## 8. Retracted hypotheses, with evidence

1. ⛔ **X9's "nine SHOWN-BUT-EMPTY meshes carry zero vertices."** Retracted for
   **six of nine** by the `cverts` column and by a 30-vs-203 draw-count
   comparison. §1
2. ⛔ **X9's "`BandPatchMesh.cpp` is not compiled standalone … `LightPreset.cpp`
   is not in `rb3-render`'s source list."** Retracted on two independent
   grounds; the build prints the disproof at configure time. §3.1
3. ⛔ **X9's implied framing that the wall is a build-system problem.** Retracted:
   the build system got all 109 of its decisions right. The wall is 11 unported
   bodies and 37 undefined globals. §3.2
4. ⛔ **X9's "34 FULL-BUT-HIDDEN" count** (same broken predicate) — superseded by
   140–141. The *verdict* that they are correct **stands**. §4
5. ⛔ **My own: "the D3D9 code is in the binary — 89 symbols."** Retracted after
   sampling: hex addresses containing `d3d`. Corrected to 105 `Dx*::` symbols in
   the **object**, **zero** in the binary (`--gc-sections`). §2.2
6. ⛔ **My own: "`RndMeshDeform::Reskin` being stubbed empties the meshes."**
   Retracted before any code, by reading the oracle: `Reskin` writes into an
   already-sized array and aborts on a count mismatch. §5.1
7. ⛔ **My own: "the head resource milo fails to load."** Retracted: `eyes`,
   `tongue` and both teeth load full geometry from **the same milo**. §5.1
8. ⛔ **My own Direction-A criterion (2110 rows).** Discarded, not reported —
   "missing" is a demand property the graph cannot decide. §2.3
9. ⚠ **Explicitly NOT claimed:** that registering `OutfitConfig` fixes anything
   geometric. It fixes textures, tattoos, skin composites and the band logo. §3.4
10. ⚠ **Explicitly NOT resolved:** why `head.mesh` / `hands_naked.mesh` /
    `eyebrows*` ship with zero triangles, and what fills them. §5.1
11. ⚠ **Explicitly NOT resolved (carried):** the band-vs-crowd z gap; the 109
    dead keys in 7 other targets; `band.play_mode`.

---

## 9. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★ **What fills `head.mesh` / `hands_naked.mesh`?** | THE remaining band-geometry question, now correctly scoped to **3 meshes, not 9**. They ship with 33–40 bones and zero triangles. `OutfitConfig` (§3.4), `Reskin` (§5.1) and milo-load failure (§5.1) are all **ruled out with evidence**. Start from `RndMesh::LoadVertices` + `CopyGeometry` — the only two places geometry is ever populated — and ask which one *should* have fired. | X11 |
| ⚠ **`OutfitConfig` registration: 48 symbols, partitioned** | 37 globals are mechanical (`m*_symbols.cpp` is exactly this file's job); 11 `BandPatchMesh` bodies are either a port from rb3-Wii's 1511-line TU or labelled stubs beside the existing `ConstructQuad` one. ⚠ rb3-Wii **un-excluded** `BandPatchMesh` from its native fork because `ObjVector<BandPatchMesh>::resize` calls its dtor from `OutfitConfig::Load` and a weak stub **SEGVs** — so stub the dtor for real, not weakly. Pays for **textures/tattoos**, not geometry. | X11 |
| ⛔ **Direction B: 20 files of unasked-for code in 2 targets** | Incl. 5 D3D9 TUs in a WebGPU target. Currently masked by `--gc-sections`, so **latent**. The fix is native-side (`#if !HX_NATIVE` on the offending edges, the mechanism already used at `LightPreset.cpp:1488`) and costs **zero** X360 score, since the guard is inert on X360. **Do not** solve it by changing packing. | build-system |
| ⚠ **42 files reach no native target** | The predictor for future `OutfitConfig`-shaped walls. `x10-scatter-report.txt`. | build-system |
| ⚠ **`ScatterIncludes.cmake` comment is stale; hand-list likely redundant** | `native/CMakeLists.txt:1160-1165` claims non-transitivity; the code is transitive. Removing `_MILO_SCATTER_TRANSITIVE_PRUNE` needs an 18-target A/B. | build-system |
| ⚠ **`CharMeshHide::HideAll` is an inert stub** | The shown/hidden selection currently works by accident of authoring + merge, not by the shipped mechanism. | X11 |
| ⚠ **`RB3_BAND_PLACE` still opt-in; `band.play_mode`; 109 dead keys; `BandCamShot` (611); impostor RTT; `video_05`; foreign `FxSendNative.cpp` edit** | All carried, untouched. | as before |

---

## 10. Recommended X11 shape

1. ★★ **Check the instrument before you believe its most interesting output.**
   Two lanes' charters — X9's and mine — were set by one call to the wrong
   accessor. The tell was available for free: the *same log* showed the venue's
   visibly-rendered `stage.mesh` reporting `verts=0`. When a probe says
   something surprising about a subsystem that visibly works, suspect the probe.
2. ★★ **A cause that is constant across the working and broken arms is not the
   cause.** The 40 `Can't make OutfitConfig` fired identically on the hair milo
   (geometry loads) and the head milo (geometry does not). One line of the log,
   read comparatively, retired the entire briefed milestone.
3. ★ **"Ask what the missing code would DO" is worth more the more expensive the
   fix looks.** The `OutfitConfig` bill is 48 symbols. Reading what
   `OutfitConfig` *is* — a material/texture compositor — cost an hour and showed
   that paying it would not have moved a single vertex.
4. ★ **Audit both directions, and expect the silent one to be the populated
   one.** Direction A (loud, one instance, already handled correctly by the
   build) had a lane's attention. Direction B (silent, 20 files, 8 modules,
   including a D3D9 renderer in a WebGPU target) had nobody's.
5. **The band is placed AND largely drawn.** For the first time the open
   question is neither "where do they go" nor "why is nothing drawn" — it is
   three named meshes that ship with no triangles.
