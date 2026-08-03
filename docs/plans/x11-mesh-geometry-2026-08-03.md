# X11 — the empty meshes were never missing; they were LOADED, then RELEASED

**Date:** 2026-08-03
**Predecessor:** [X10](x10-band-geometry-2026-08-03.md) "the band's geometry was mostly already there; the probe was reading the wrong array"
**Branch:** `x11-mesh-geometry`, rebased onto `main` @ `de2f463f`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** 3 shared `src/` files (all hunks inside `#ifdef`/`#ifndef HX_NATIVE`) + 2 native driver files. **Zero X360 cost, measured twice.**

---

## Verdict

★★★ **ALL THREE NAMED MESHES NOW CARRY GEOMETRY, AND SO DO TWO X10 DID NOT
NAME.** `head.mesh`, `hands_naked.mesh` and `eyebrows*_resource.mesh` — plus
`malewrist_barbedwire_right.mesh` and `malewrist_hercules_right.mesh` — are
populated and drawing. Per member **SHOWN-BUT-EMPTY 3–4 → 0**, and **DRAWABLE
now equals `showing` exactly** (19/19, 29/29, 22/22, 19/19). §1, §4

★★★ **THEY WERE NEVER EMPTY AS SHIPPED. X10's central conclusion is REFUTED.**
`head.mesh` carries **2592 verts / 4726 faces** in `char/main/head/male/head.milo`,
`hands_naked.mesh` 1876/3092, `eyebrows1_resource.mesh` 302/254 — read straight
out of the assets. They load every run. Something **throws them away**. §2

★★ **THE MECHANISM IS `SetKeepMeshData(false)`, AT TWO SITES, AND IT IS CORRECT
ON CONSOLE.** It clears `mVerts` and frees `mFaces` while leaving
`mCompressedVerts` untouched — exactly the observed signature. The console
releases the CPU copy because its platform vertex buffer already exists by
then; the dc3 WebGPU backend builds its buffer **lazily at first draw**, so the
release destroys the geometry before it is ever uploaded. **A lifetime
mismatch, not a decomp defect.** §3

★★ **MILESTONE 2 CLOSED — the D3D9 row is out of the WebGPU target — but NOT
for the reason X10's handoff gave, and it was NOT free.** Guarding the edge
broke the link with **exactly one** undefined symbol: `Hmx::Matrix4::Col3`,
a math accessor **defined inside `rnddx9/Cam.cpp`**. 105 `Dx*::` symbols → 0. §5

⚠ **WHAT DID NOT LAND: I did not verify that the restored hands are in the
RIGHT PLACE.** They render; their skinning is well-formed (`nullbones=0`); at
this camera they read as detached from the sleeves. **Flagged, not asserted.** §6

★ **X6's SHA table is vindicated a FIFTH time** — `5282bd275159f10b`,
byte-identical to X10's artifact, after a fresh gate rebuild *and* after the
rebase. §7

---

## 1. The result, measured

`small_club_01`, `RB3_BAND_PLACE=1` (`x11-run-rebased.log`):

| metric (per member) | X10 | **X11** |
|---|---|---|
| SHOWN-BUT-EMPTY | 3–4 | **0, 0, 0, 0** |
| DRAWABLE | 16 / 26 / 18 / 15 | **19 / 29 / 22 / 19** |
| `showing` | 19 / 29 / 22 / 19 | unchanged |
| **DRAWABLE == showing?** | no | **yes, all four** |

Second venue, `arena_01` (`x11-run-arena01.log`): SHOWN-BUT-EMPTY **0** on all
four, DRAWABLE == showing (19/30/22/19), and the drum riser still measures
**320.90 − 255.79 = +65.1** — X7's asset-derived prediction and X9's placement
are non-regressed.

★ The restored meshes report **`nullbones=0`** — every bone slot is bound, so
this is real skinned geometry, not a mesh collapsed onto the identity.

---

## 2. ★★★ The comparative read — three hypotheses refuted on one line

X10 handed over "they ship with zero geometry — verified against the corrected
predicate, so these are real, not artifacts." The predicate was right. The
**conclusion drawn from it** was wrong.

**Step 1 — where do they live?** `--mesh-detail` (new, native driver) prints
every empty mesh *beside the loading meshes in the same `Dir`*:

```
DIR outfit (char/main/outfit.milo)
  fingernails_resource.mesh   LOAD  v=0    cv=350  f=300  bones=10  owner=self
  tongue.mesh                 LOAD  v=0    cv=58   f=94   bones=4   owner=self
  head.mesh                   EMPTY v=0    cv=0    f=0    bones=33  owner=self
  upperteeth.mesh             LOAD  v=0    cv=276  f=502  bones=1   owner=self
  eyebrows1_resource.mesh     EMPTY v=0    cv=0    f=0    bones=6   owner=self
  youngozzie_resource.mesh    LOAD  v=0    cv=2348 f=3012 bones=17  owner=self
  hands_naked.mesh            EMPTY v=0    cv=0    f=0    bones=38  owner=self
```

⛔ Every mesh, empty **and** loading, is in **ONE `Dir`** with **`owner=self`**.
That single block refutes three separate hypotheses at once — "they come from a
different milo", "their milo failed to load", and "their `mGeomOwner` dangles"
(the last was my own leading suspect, killed before it cost anything).

**Step 2 — is the geometry in the asset?** Load the wardrobe milos standalone
(`x11-src-milos.log`):

| mesh, loaded standalone | verts | faces | bones |
|---|---|---|---|
| `head.mesh` (`char/main/head/male/head.milo`) | **2592** | **4726** | 33 |
| `hands_naked.mesh` (`male_hands_naked.milo`) | **1876** | **3092** | 0 |
| `eyebrows1_resource.mesh` | **302** | **254** | 0 |

⛔ **The geometry is there. X10's "genuinely empty as shipped" is retracted.**

**Step 3 — what distinguishes the two populations?** The loading meshes are
**compressed** (`cv>0, v=0`); the empty ones are **uncompressed** (`v>0, cv=0`)
in the source milo. `RndMesh::SaveVertices` (`Mesh.cpp:1836-1843`) explains
why: a mesh with `mMutable & 0x1F` or `mKeepMeshData` is written **uncompressed**
— i.e. exactly the deform targets. So the empty set is the *deformable* set,
and the search narrowed to code that touches uncompressed CPU geometry.

---

## 3. ★★ The mechanism: `SetKeepMeshData(false)`, twice

`rndobj/Mesh.cpp:954-965`:

```cpp
void RndMesh::SetKeepMeshData(bool keep) {
    if (keep != mKeepMeshData) {
        mKeepMeshData = keep;
        if (!mKeepMeshData) {
            mVerts.clear();
            std::vector<Face>().swap(mFaces);
            std::vector<unsigned char>().swap(mPatches);
```

Clears verts **and** faces, leaves `mCompressedVerts` alone — the observed
signature exactly (`v=0 f=0 cv=0`, bones and material intact).

### 3.1 The positive/negative control that proves it

`RB3_TRACE_KEEPMESH=1` names **every** mesh that reaches a release, with its
live vertex count. The result is not a count — it is a **set identity**:

```
[KEEPMESH] release 'hands_naked.mesh'              verts=1876 faces=3092 bones=38
[KEEPMESH] release 'eyebrows1_resource.mesh'       verts=302  faces=254  bones=6
[KEEPMESH] release 'malewrist_barbedwire_right.mesh' verts=405 faces=479 bones=3
[KEEPMESH-ANY] 'head.mesh' loses verts=2592 faces=4726
```

⛔ **The released set IS the shown-but-empty set, and nothing else appears in
it.** `eyes`, `tongue`, `upperteeth`, `lowerteeth`, the hair, `fingernails`,
`maleearrings_plug`, `male_neck_ao`, `male_tattoo_torso` — all absent from the
trace, all rendering fine. That is the comparative discriminator X10 asked the
next lane to find.

### 3.2 ⚠ TWO sites, not one — and the trace is what caught it

| site | file:line | meshes |
|---|---|---|
| 1. `BandCharacter::SyncObjects`, the `unk610` `RndMeshDeform` drain | `bandobj/BandCharacter.cpp:1118` | `hands_naked`, `eyebrows*_resource`, `malewrist_*_right` |
| 2. `MeshCacher::~MeshCacher` | `char/CharMeshCacheMgr.h:9-16` | **`head.mesh`** |

★ **`head.mesh` arrives at site 1 ALREADY at `verts=0`, on all four members,
including the first.** Had I stopped at site 1 and declared victory, head would
still be empty and the report would have been wrong. Site 2 is reached because
`BandCharacter::SetDeformation` (`:1599`) does **`mgr->Disable(!mInCloset)`** —
so outside the closet *every* `MeshCacher` is disabled, and the disabled arm
calls `SetKeepMeshData(false)` and skips `PopulateMesh`.

### 3.3 Why the console is right and the port is not

Both releases are **correct on X360**: the platform vertex buffer already
exists, so the CPU copy is dead weight (and outside the interactive closet it
is never needed again). The dc3 WebGPU backend builds its vertex buffer
**lazily at first draw**. So the shipped release destroys the geometry before
it is ever uploaded.

Both fixes are `HX_NATIVE`-only; `RB3_RELEASE_MESHDATA=1` opts back in to
console behaviour. At site 2 **only the `mDisabled` arm** is changed — the
closet arm keeps the shipped restore-from-cache verbatim — and it **returns**
rather than falling through to `PopulateMesh`, which would write back the
pre-deform snapshot and quietly **undo the head shaping**.

⛔ **NOT INVENTED.** No geometry, transform or visibility was substituted. The
verts drawn are the ones the asset shipped; the change is that they are no
longer thrown away.

---

## 4. Where the frame actually changed

Band frame vs X10's: bbox **(761,444)–(823,489)**, **101 pixels**. Small,
because at 1280×720 only one member faces the camera and the head is ~45 px
tall. In the crop the face is now shaded where it was blank, and two skin-toned
hand shapes appear that were absent. Evidence: `x11-crop-baseline.png` vs
`x11-crop-x11.png`, `x11-zoom-*.png`.

⚠ **Reported plainly: this is a small visible change.** The measured change is
large (5 meshes/member restored, DRAWABLE == showing); the *visible* change at
this camera is not. Both are stated rather than the flattering one alone.

---

## 5. ★★ Milestone 2 — closed, and X10's cost estimate was wrong

X10's handoff: *"The fix is native-side (`#if !HX_NATIVE` on the offending
edges, the mechanism already used at `LightPreset.cpp:1488`) and costs **zero**
X360 score."* The mechanism and the X360 cost are right. **"Free" is not.**

Guarding `rndobj/CubeTex.cpp:284` with `#ifndef HX_NATIVE` broke the link:

```
undefined reference to `Hmx::Matrix4::Col3(int) const'
```

— and the distinct-undefined-symbol set has size **exactly 1** (measured, not
predicted: apply, link, enumerate).

⛔ **`Hmx::Matrix4::Col3` is declared in `math/Mtx.h:128` — a MATH header — and
DEFINED at `src/system/rnddx9/Cam.cpp:13`**, parked above `DxCam::DxCam()` in
the Direct3D9 camera TU, and called by `rndobj/Lit_NG.cpp`, a real native
target source. So the D3D9 block was **load-bearing after all**. X10 measured
that `--gc-sections` strips every `Dx*` symbol from the binary and concluded
the code was inert; that is true of the `Dx*` symbols and **false of the TU as
a whole**. This is precisely the charter's *"it is latent, so do not break a
working build to tidy it"* hazard, met head-on.

Provided in `native/src/milo_link_stubs.cpp` section (1) *REAL
IMPLEMENTATIONS*, **copied verbatim** from `rnddx9/Cam.cpp:13-15`. No ODR
conflict (that TU is now excluded natively; this file never compiles for X360).

**Result:** `Dx*::` symbols in the compiled `rndobj/CubeTex.cpp.o` **105 → 0**;
`AppLabel` → 0; `rb3-render` + `rb3-milo` link rc=0; **band frame and E0 frame
both byte-identical** to the milestone-1 artifacts — the change moved **zero
pixels**.

⚠ The other 7 Direction-B rows are **NOT** closed. Given that the sharpest and
best-evidenced row turned out to have a hidden dependency, the honest prior is
that the others do too; each needs the same apply-link-enumerate treatment.

---

## 6. ⚠ What did NOT land — stated plainly

1. ⚠ **I did not verify the restored hands are in the correct POSE/PLACE.**
   They draw, `nullbones=0`, but at this camera they read as detached from the
   sleeves. That is an eyeball observation at ~40 px, not a measurement, and it
   is **flagged, not asserted, in either direction**. A close-up camera cell is
   the cheap next check.
2. ⛔ **`OutfitConfig` is still not registered** (48 symbols, X10 §3.3). Still
   no tattoos, skin composite or band logo. Untouched — and X10's finding that
   it supplies no geometry is *confirmed* by this lane from the other side: the
   geometry was there all along.
3. ⛔ **7 of 8 Direction-B rows remain open**; 42 orphan files and 22 multi-host
   landmines untouched.
4. ⚠ **`CharMeshHide::HideAll` is still an inert stub** (X10 §4) — so the
   shown/hidden selection still works by authoring + merge, not by the shipped
   mechanism. Carried.
5. ⚠ **`_MILO_SCATTER_TRANSITIVE_PRUNE` / stale comment** untouched.

---

## 7. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh** (`rm -rf native/build`), rc=0, **0 SKIPs** | ✅ **PASS — 18/18** | run **twice**: once pre-rebase, once fresh after rebasing onto `de2f463f`. All 18 *relinked this run*, 0 errors, 0 warnings, `grep -c SKIP` → **0**. Cache seeded with **all four** flags. `x11-native-gate.log`, `x11-native-gate-rebased.log` |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606a202f2b3226e38a8f28010b096f3d441`. ⚠ The foreign uncommitted `src/platform/FxSendNative.cpp` edit disclosed by X4d–X10 is **still there, still not mine**, left untouched — **eighth lane running.** |
| c | Shared-`src/` X360-faithful, verified at symbol granularity | ✅ **PASS — Δ0.000000pp, twice** | §7.1 |
| d | PNG determinism ×2 on every cited image | ✅ **PASS** | band + E0 rendered ×2 → `cmp` identical; re-rendered again after the fresh gate rebuild **and** after the rebase → still identical |
| e | Prior lanes' evidence non-regressed, vs **artifacts** | ✅ **PASS — byte-identical** | E0 `cmp`-identical to `x10-E0-default.png` on disk, sha `5282bd275159f10b` == X6's recorded value. **Fifth vindication.** `arena_01` riser +65.1 preserved. |
| f | Was `main` broken by a decomp lane? | ✅ **NO** | baseline `rb3-render` built rc=0 at `c2344a87` before any edit; full 18/18 gate rc=0 after rebasing onto `de2f463f` (lane DN-2). |

### 7.1 X360 A/B — two settled runs, `tools/ab_measure.py`

| commit | Δmatched | Δcode% | Δfuzzy | leg-B recompiles |
|---|---|---|---|---|
| mesh-geometry (`BandCharacter.cpp`, `Mesh.cpp`, `CharMeshCacheMgr.h`) | **+0** | **+0.000000pp** | **+0.000000pp** | 6 MSVC |
| Direction B (`rndobj/CubeTex.cpp`) | **+0** | **+0.000000pp** | **+0.000000pp** | 2 MSVC |

Both legs settled to zero-work before measurement, and leg B genuinely
recompiled — so these are **measured zeros, not inert no-ops**.

**Per-TU objdiff position** (unchanged by this lane):

| touched TU | unit | position |
|---|---|---|
| `src/system/bandobj/BandCharacter.cpp` | `default/BandCharacter` | 67.42% code / 85.99% fuzzy, 508/603 |
| `src/system/rndobj/Mesh.cpp` | `default/Mesh` | 61.78% / 82.29%, 260/335 |
| `src/system/rndobj/CubeTex.cpp` | `default/system/rndobj/CubeTex` | 88.20% / 95.34%, 44/47 |
| `src/system/char/CharMeshCacheMgr.h` | — | ⛔ **NOT SCOREABLE — it is a HEADER.** Its inlined consumers are `default/system/char/CharMeshCacheMgr` (**100.0%**, 17/17) and `default/OutfitConfig` (55.72% / 71.07%); both measured unchanged. |
| `native/src/main_render.cpp`, `native/src/milo_link_stubs.cpp` | — | ⛔ **NOT SCOREABLE — native driver TUs, no `splits.txt` entry.** Stronger than "unchanged": there is no X360 object at all. |

### 7.2 ⚠ A trap this lane hit, and nearly reported as a Direction-B regression

`tools/ab_measure.py --revert REF` leaves the **reverted** patch applied in the
worktree when it finishes. Everything I built after the first A/B silently
lacked the milestone-1 fix. The band frame then came back **byte-identical to
X10's baseline** with the same 101-pixel bbox — which reads exactly like "the
Direction-B change undid the mesh fix". It did not; the fix was not in the
tree. Caught by `cmp`-ing against *three* candidate artifacts instead of one,
and by `grep`-ing the source for the fix's own comment.

★ This is X10's rule earning its keep a second time: **re-measure the artifact
before retracting anything** — including your own work.

---

## 8. Per-subsystem verdict table

| subsystem | verdict | evidence |
|---|---|---|
| **`head.mesh` carries geometry** | ★★★ **YES — 2592 v / 4726 f, drawing** | §1, §2 |
| **`hands_naked.mesh` carries geometry** | ★★★ **YES — 1876 v / 3092 f** | §1 |
| **`eyebrows*_resource.mesh` carries geometry** | ★★★ **YES — 302/308/328/116 v** | §1 |
| **`malewrist_*_right.mesh`** | ★ **RESTORED — a 4th/5th mesh X10 never listed** | §3.1 |
| **"the 3 meshes ship with zero geometry" (X10)** | ⛔ **REFUTED — they ship FULL** | assets read standalone. §2 |
| **What differs between a loading and non-loading mesh** | ★★★ **NOTHING INTRINSIC — one is released, the other is not** | set-identity trace. §3.1 |
| **Release site 1 — `BandCharacter::SyncObjects`** | ★★ **DEFECT FOUND + FIXED** | `BandCharacter.cpp:1118`. §3.2 |
| **Release site 2 — `MeshCacher::~MeshCacher`** | ★★ **DEFECT FOUND + FIXED** | `CharMeshCacheMgr.h:9-16` via `Disable(!mInCloset)`. §3.2 |
| **Is it a decomp defect?** | ⛔ **NO — a GPU-upload lifetime mismatch** | console release is correct; WebGPU uploads lazily. §3.3 |
| **Skinning of restored meshes** | ✅ **WELL-FORMED — `nullbones=0`** | §1 |
| **Hand POSE / placement correctness** | ⚠ **UNREACHED — flagged, not asserted** | reads detached at ~40 px. §6.1 |
| **Direction B, `rnddx9` row** | ★★ **CLOSED — 105 `Dx*` → 0, zero pixels moved** | §5 |
| **X10's "the Direction-B guard costs nothing"** | ⛔ **REFUTED — it costs exactly 1 symbol** | `Hmx::Matrix4::Col3`. §5 |
| **Direction B, other 7 rows** | ⛔ **NOT CLOSED — deferred with reason** | §5, §6.3 |
| **`OutfitConfig` registration** | ⛔ **STILL BLOCKED — untouched** | §6.2 |
| **X10's "`OutfitConfig` supplies no geometry"** | ✅ **CONFIRMED from the other side** | the geometry was never missing. §6.2 |
| **X9/X7 band placement, `arena_01` riser** | ✅ **NON-REGRESSED — +65.1** | §1 |
| **Venue / crowd / lighting / E0 default frame** | ✅ **BYTE-IDENTICAL to X10's artifact** | §7 gate e |
| **X360 blast radius** | ✅ **ZERO — Δ0.000000pp × 2 settled A/Bs** | §7.1 |
| **`CharMeshHide::HideAll`; `BandCamShot`; `band.play_mode`; `RB3_BAND_PLACE` opt-in; foreign `FxSendNative.cpp` edit** | ⚠ **CARRIED, untouched** | as before |

---

## 9. Retracted hypotheses, with evidence

1. ⛔ **X10's "`head.mesh`/`hands_naked.mesh`/`eyebrows*` ship with zero
   geometry — verified, so these are real, not artifacts."** The *predicate*
   was correct; the *conclusion* is retracted. Loaded standalone they carry
   2592/1876/302 verts. §2
2. ⛔ **X10's "the fix is native-side and costs zero"** (Direction B). Retracted
   as to cost-in-effort: the guard broke the link on `Hmx::Matrix4::Col3`.
   Zero **X360 score** cost is confirmed. §5
3. ⛔ **X10's implied "start from `LoadVertices` / `CopyGeometry`, the only two
   places geometry is ever populated."** Sound advice, wrong subsystem — the
   defect is not in *population* at all but in *release*. Loading always worked.
4. ⛔ **My own: "`mGeomOwner` dangles, so the proxy falls back to `this`."**
   Retracted before any code by the `--mesh-detail` block: every mesh reports
   `owner=self`. §2
5. ⛔ **My own: "they come from a different milo / their milo failed to load."**
   Retracted by the same block: one `Dir`, `char/main/outfit.milo`. §2
6. ⛔ **My own: "one release site explains all of them."** Retracted by the raw
   trace ordering: `head.mesh` reaches site 1 already at `verts=0` on all four
   members. §3.2
7. ⛔ **My own, nearly reported: "the Direction-B change regressed the mesh
   fix."** Retracted — `ab_measure --revert` had left the fix reverted in the
   worktree. §7.2
8. ⛔ **Object-hash A/B as an instrument.** Discarded, not reported: MWCC
   objects are not byte-reproducible (rebuilding `main` in the *same* worktree
   produced a third set of hashes). Replaced by `tools/ab_measure.py`.
9. ⚠ **Explicitly NOT claimed:** that the hands are in the right place. §6.1
10. ⚠ **Explicitly NOT claimed:** that the other 7 Direction-B rows are safe to
    guard. The one row that was investigated was not. §5

---

## 10. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★ **Is the restored hand/head geometry correctly POSED?** | The one thing this lane restored but did not verify. `nullbones=0` says the skinning is well-formed; it does not say the bind pose is right. Cheapest check: a close-up camera cell on one member (`--cam-manual`) and a compare against `images/retail-screenshots/`. | X12 |
| ⚠ **`Hmx::Matrix4::Col3` belongs in `math/Mtx.cpp`** | It is declared in `math/Mtx.h:128` and defined in `rnddx9/Cam.cpp:13`. Natively it is now duplicated in `milo_link_stubs.cpp`. Moving it is match-relevant (it leaves `rnddx9/Cam.cpp`'s COMDAT) and needs its own A/B — do not move it casually. | build-system |
| ⛔ **The other 7 Direction-B rows** | `hamobj` (5), `flow` (4), `bandtrack` (2), `game`, `meta_band`, `gesture`, `midi`. Method now proven: guard the edge, link, enumerate the undefined set, provide it natively, A/B. Expect each to have a hidden dependency — the sharpest row did. | build-system |
| ⚠ **`OutfitConfig` registration: 48 symbols** | Unchanged from X10. Pays for textures/tattoos/skin composite/band logo — **not** geometry, now confirmed from both sides. | X12 |
| ⚠ **`ab_measure --revert` leaves the reverted patch applied** | Cost this lane a false regression (§7.2). Either pass `--restore`, or `git checkout HEAD -- .` immediately after every run, or teach the tool to restore by default. | tooling |
| ⚠ **`CharMeshHide::HideAll` inert stub; 42 orphan files; 22 multi-host landmines; `_MILO_SCATTER_TRANSITIVE_PRUNE`; `BandCamShot`; `band.play_mode`; `RB3_BAND_PLACE` opt-in; foreign `FxSendNative.cpp` edit** | All carried, untouched. | as before |

---

## 11. Recommended X12 shape

1. ★★★ **"It is missing" and "it was taken away" look identical at the point of
   observation.** Three lanes measured `verts=0` on `head.mesh` and reasoned
   forward about why it never loaded. It always loaded. The question that broke
   it open was not *"why is this empty?"* but *"was it ever full?"* — answered
   in one command by loading the source asset standalone.
2. ★★ **Make the probe print the working arm next to the broken one, in the same
   block.** `--mesh-detail` killed three hypotheses (different milo / load
   failure / dangling geom owner) before a line of fix was written, because the
   controls were *on screen* rather than in another log.
3. ★★ **A set identity beats a count.** The `KEEPMESH` trace's value was not
   "14 releases" — it was that the released set **equals** the broken set and
   contains nothing else. X10's headline was a count that was wrong; this
   lane's headline is a set that could not be coincidence.
4. ★ **When one site explains most of the symptom, check the residue before
   claiming the cause.** `head.mesh` was already empty at site 1. One `sort |
   uniq -c` hid that; the raw ordering showed it.
5. ★ **A latent defect can still be load-bearing.** `--gc-sections` stripping
   every `Dx*` symbol proved the *symbols* were dead. It did not prove the *file*
   was — and one one-line math accessor in it was holding up the link.
