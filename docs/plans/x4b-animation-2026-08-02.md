# X4b — animation: a posed character, and the two silent defects that stood in the way

**Date:** 2026-08-02
**Predecessor:** [X4a](x4a-venue-render-2026-08-02.md) "PARTIAL"
**Branch:** `x4b-animation`, rebased onto `main`
**Engine:** `milo-native-engine` — pin moved `2ea8e343…` → **`138e1606…`**, **zero engine edits**

---

## Verdict: **POSED — and the bone-length oracle proves it, to 7.47e-05**

`rb3-render` plays a real shipped RB3 `CharClip` through the engine's own
`Character::Poll()` path and produces a posed, textured crowd character.
All 39 bones satisfy the rigid-skeleton invariant exactly.

```
=== char/crowd/gen/crowd_female01.milo_xbox ===
  clips: loaded 'char/crowd/anim/gen/female_base.milo_xbox'
  clips: 44 CharClip(s) available; playing 'crowd_reaching_01' (beats 0.00..30.00, 61 frames)
  clips: bones stuffed into CharServoBone 'bone.servo'
  clips: polled 41 time(s) to beat 8.145 (bpm 120.0, 2.000 s)
  [PASS] clip-driven — a real shipped CharClip was played through Character::Poll()
  [PASS] bone-length-invariant — max ratio 0.9999 over 39 bone(s)
                                 (deviation 7.47e-05; tolerance 1e-3)
  [PASS] draws-issued — 6 of 6 · [PASS] png
  [PASS] image-not-empty — coverage 23.12%, 21601 distinct colours
RESULT: ALL GATES PASSED (0 gate failure(s))
```

★ **Two defects were found, and both were silent.** Neither produced an assert,
a log line, or a crash. One made **every `Sine()` and `Cosine()` in the entire
native tree return `0.0`** (§3). The other made `Multiply(Transform, Transform,
Transform)` wrong whenever its destination aliased an argument — the exact
hazard class the charter named, present and provable (§2).

⚠ **The headline deliverable is qualified, and the qualification is the honest
part.** The *bones* are posed and provably correct. The *skinned mesh* smears.
Bone evaluation and world compose are VERIFIED; the skinning palette is **not**.
A picture of a posed character is not the same claim as a picture of a *correct*
character, and this document does not conflate them.

★ **A third silent defect was root-caused but not fixed** (§5.1): every skinned
mesh is **truncated to 4 bones at load**, because `MaxBones()` reads a gfx-mode
global that only `PreInitSystem` ever sets. That is the same root-cause shape as
§3 — a bring-up sub-init the hand-rolled native drivers skipped — and it is the
explanation for the `exceeds bone limit (20 of 4)` warning X3 recorded and left
open. The one-line fix is measured and **deliberately not landed** (§5.2).

★ **X4a's central structural finding is RETRACTED on measurement** (§4). The 14
"band3" classes blocking every venue root contain **zero** `src/band3/` classes,
and two of them were already compiled and linked into every binary.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native target gate, **fresh**, on the rebased tree | ✅ **PASS — 18/18** | `tools/native_build_gate.sh` → `PASS (rc=0, 0 errors, 0 warnings, 18/18)`. Run in a worktree with **no `build/` directory at all**, so no binary could be stale by construction — every target reported `relinked this run`. |
| b | Zero `milo-native-engine` source edits | ✅ **PASS** | Engine `HEAD` = `138e1606…` = the new pin. Two engine requests are filed as text in §7, not as edits. |
| c | Shared-`src/` edits `HX_NATIVE`-gated, X360 arm token-for-token | ✅ **PASS — 2 files** | `math/mtx.cpp` and `math/Trig.cpp`. Both changes are entirely inside `#ifdef HX_NATIVE`; the match build passes no `/D` at all, so the X360 token stream is unchanged. Neither is a decomp-bug promotion, so no objdiff A/B debt is incurred. |
| d | PNG determinism ×2 on every cited image | ✅ **PASS** | Posed cell `sha256 7c7dd7ee1c11f1f0…` identical ×2 at `--frames 1`. X3 cells identical ×2. |
| e | X3 + X4a no-regression | ✅ **PASS — byte-identical, 4/4** | `cbdb29fa95a5b574…`, `30692a8d02c1ada0…`, `d9624b900a1b0699…`, `d30f600d8ea3bcfe…` — the exact SHAs X3 and X4a recorded. Re-verified **after each** of the three landed source changes, not once at the end. |

### Reproduce

```bash
cd rb3-xenon/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine \
      -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build --target rb3-render

ASSETS=/home/free/code/milohax/rb3/orig-assets/xbox-zip
./build/rb3-render $ASSETS build/x4b/posed --frames 1 \
    --clips char/crowd/anim/gen/female_base.milo_xbox \
    --clip crowd_reaching_01 --beat 4.0 --bone-audit \
    char/crowd/gen/crowd_female01.milo_xbox

./build/rb3-render $ASSETS build/x4b/bind --frames 1 --bone-audit \
    char/crowd/gen/crowd_female01.milo_xbox      # the control: bind pose
```

⚠ In a worktree, `MILO_ENGINE_PATH` and `Dawn_DIR` **must** be passed explicitly
(X4a §1 documents why: both default relative to the source tree, and the engine
silently auto-disables at `rc=0` when they miss).

---

## 2. ⛔ Defect 1 — `Multiply(Transform, Transform, Transform)` was alias-unsafe **both** ways

The charter named this hazard class in advance. It was present.

`math/mtx.cpp:77`'s `HX_NATIVE` arm wrote its result before it finished reading
its operands:

```c
Multiply(a.m, b.m, out.m);                       // <-- STORE
out.v.x = a.v.x * b.m.x.x + … + b.v.x;           // <-- then READ a.v and b
```

**The X360 arm directly below it does not have this bug.** It branches on
`if (&b != &out)` and hoists every b-side load into locals first — i.e. retail
*knew* the destination could alias. The native transcription dropped the guard.

Broken in **both** directions, for two different reasons:

| aliasing | mechanism | live call sites |
|---|---|---|
| `&out == &b` | `Multiply(a.m, b.m, out.m)` overwrites `b.m`; the translation lines then read the **product** matrix instead of `b` | `rndobj/Trans.cpp:651` `Multiply(tf48, tf78, tf78)` in `RndTransformable::SetTransParent` |
| `&out == &a` | writing `out.v.x` clobbers `a.v.x`, which the **very next line** reads for `out.v.y` | `rndobj/Trans.cpp:697/709/710` `Multiply(mWorldXfm, tf, mWorldXfm)` in `ApplyDynamicConstraint` |

★ The `&out == &a` case is the one worth remembering: it is invisible to a
"reorder the stores so the matrix goes last" fix, which is the obvious repair
and an incomplete one.

**Proven, not argued.** A standalone harness linked against the *real* compiled
`mtx.cpp` object, `a = rotZ(30°) t(10,0,0)`, `b = rotZ(60°) t(0,5,2)`:

| leg | before fix | after fix |
|---|---|---|
| reference `(a,b,out)` | `v = [5.000 13.660 2.000]` ✅ | unchanged |
| `dest == b` `(a,b,b)` | `v = [-0.000 15.000 2.000]` ❌ | `[5.000 13.660 2.000]` ✅ |
| `dest == a` `(a,b,a)` | `v = [5.000 9.330 2.000]` ❌ | `[5.000 13.660 2.000]` ✅ |

Fix pattern: **snapshot every operand read before the first store**. Identical
defect and identical fix to rb3-Wii's `Rot.cpp` `Multiply`, which cost that lane
~15 waves because it presents as skewed bone composition far from the arithmetic
that caused it.

**Scope, measured:** 11 textual `Multiply(x, y, y)` `Transform` call sites exist
in `src/`. The `Hmx::Matrix3` and `Hmx::Quat` overloads are **alias-safe by
construction** — they evaluate all nine/four products as *call arguments*, which
are sequenced before `Set()` stores — so only the `Transform` overload needed
fixing. Fixed in the callee, as retail does, not at the 11 call sites.

⚠ **This fix alone changes none of the X3/X4a PNGs**, and that is the point: the
aliasing sites are on reparent/constraint paths that a static T-posed scene never
enters. It was latent until a character was driven, which is exactly why it
survived four milestones.

---

## 3. ⛔ Defect 2 — `TrigTableInit()` never ran, so `sin == cos == 0` **everywhere**

This is the more serious of the two, and the more embarrassing to have shipped.

`Sine()` is **not** `std::sin`. It is a lookup into `gBigSinTable`
(`math/Trig.cpp:33-45`), a table filled by `TrigTableInit()`. `gBigSinTable` has
static storage, so before that init it is legitimately **all zeroes** and
`Sine()` answers `0.0` for every input. `Cosine()` is defined as
`Sine(f + π/2)` (`Trig.h:10`), so it returned `0.0` too.

On X360 this cannot happen: `TrigTableInit()` is called from `SystemInit()`
(`os/System.cpp:522/536`) and the game always boots through it. But **all 17 of
this repo's native drivers deliberately hand-roll a reduced bring-up instead** —
`main_render.cpp:207-243` explains why (`SystemInit` → `PreInitSystem` stands the
*renderer* up, which a headless tool must sequence itself) — and **not one of
them called `TrigTableInit`**. The entire native tree ran with `sin == cos == 0`.

### How it surfaced, and why it hid for four milestones

`CharBonesMeshes::PoseMeshes` calls `MakeRotMatrixZ` for the six `.rotz` bones
(L/R knee, forearm, toe). With `c == s == 0` that builds

```
[ 0 0 0 ]
[ 0 0 0 ]      det = 0     — singular
[ 0 0 1 ]
```

so every child collapsed **exactly** onto its parent, and the compose below it
blew up. The oracle's output before the fix:

| bone | parent | authored | live | ratio | detLocal | detWorld |
|---|---|---|---|---|---|---|
| `bone_R-knee.mesh` | `bone_R-thigh.mesh` | 20.2728 | 20.2728 | 1.0000 | **0.0000** | 0 |
| `bone_R-ankle.mesh` | `bone_R-knee.mesh` | 18.0339 | **0.0000** | **0.0000** | 1.0000 | −1.9e13 |
| `r_foot.coll` | `bone_R-ankle.mesh` | 4.6231 | 0.0744 | 0.0161 | 0.9999 | **−3.9e14** |

★ Note the knee's own **ratio is 1.0000** while its `detLocal` is already 0. The
bone that is broken looks fine on the length test; only its *children* fail.
That is precisely why the determinant columns and the depth ordering earn their
place in the instrument (§6).

**Quaternion bones were unaffected** — `MakeRotMatrix(Quat)` is pure multiplies,
no trig — which is why this was invisible to X1–X4a. Nothing before this
milestone ever built a rotation matrix *from an angle*.

**Fixed at the class, not the instance.** A file-scope initializer in `Trig.cpp`
covers all 17 targets where a per-driver call would have fixed one. Safe by
construction: `gBigSinTable` is a POD array, zero-initialised at load *before*
any dynamic initializer runs, and any static ctor that read it would have got
`0.0` before this existed — so every ordering is strictly improved. `SystemInit`'s
explicit call stays and is idempotent.

After the fix: all 39 bones read ratio 1.0000/0.9999, every determinant 1.000.

---

## 4. ★ STEP-0 — X4a's structural blocker, re-measured and **retracted**

X4a measured 684 factory misses over 14 classes on a venue root, attributed them
to `band3`, and concluded a venue root is blocked until `src/band3/` compiles.
That attribution is wrong, and correcting it is most of the cost.

**None of the 14 is in `src/band3/`:**

| directory | n | classes |
|---|---|---|
| `src/system/bandobj/` | 5 | `BandCamShot`, `BandCharacter`, `BandLabel`, `BandWardrobe`, `BandConfiguration` (inline in `Band.cpp`) |
| `src/system/synth/` | 7 | `Sfx`, `SynthSample`, `MoggClip`, `SynthFader` (**class `Fader`** in `Faders.cpp`), `ParallelGroupSeq` + `RandomGroupSeq` (`Sequence.cpp`), `FxSendEQ` |
| `src/system/world/` | 1 | `WorldCrowd` (`world/Crowd.cpp`) |
| `src/system/ui/` | 1 | `UIColor` (`ui/UIColor.cpp`) |

`src/band3/` is 260 TUs. **The venue needs none of them.**

### 4.1 Compile measurement — 12 of 13 TUs are already clean

`-fsyntax-only` under `rb3-render`'s exact flags:

| result | count |
|---|---|
| **0 errors** | **12 of 13** defining TUs |
| fails | 1 — `bandobj/BandCharacter.cpp`, 18 errors |

Those 18 errors are **~4 root defects**, all stale rb3-Wii-lineage code inside
`BandCharacter`'s own `HX_NATIVE` arms:

1. protected-member access on `RndDir::mDraws`, `Character::mLods`, `ObjectDir::mStoredFile`
2. `RndMesh::mNativeBonesRebound` — a member xenon's `RndMesh` does not have
3. `Refs()` used as a `std::vector<ObjRef*>` (`.size()`, `operator[]`) at `:2255-2261`, against xenon's intrusive `ObjRef` ring
4. `MergeFilter::Action` vs `MergeFilter::SubdirAction` mixed up at `:2437/:2442`

Counting TUs says "1 of 14 blocked". Counting **defects** says four, in one file
— the same lesson X2 recorded when 13 `rndobj` failures turned out to be 6 root
causes.

### 4.2 ⛔ The real blocker is the **build system**, and it was measured

Wiring the 10 clean TUs into `MILO_FORK_SOURCES` was **attempted and backed
out**: **807 duplicate-definition link errors**, from exactly three emitters —

| emitter | errors |
|---|---|
| `rndobj/EventTrigger.cpp` | 313 |
| `rndobj/Font.cpp` | 244 |
| `char/CharIKScale.cpp` | 162 |

None is a code defect. `cmake/ScatterIncludes.cmake` drops an `#include "*.cpp"`
includee **only when that includee is itself a target source**. Here two
*different* target sources scatter-include the same *non-source* file:

```
bandobj/BandCamShot.cpp -> math/Geo.cpp                        <- char/CharIKScale.cpp
bandobj/BandCamShot.cpp -> flow/*.cpp                          <- rndobj/EventTrigger.cpp
bandobj/BandLabel.cpp   -> char/CharClip.cpp, bandobj/BandDirector.cpp  <- rndobj/Font.cpp
```

so the module has no reason to drop either copy and both are emitted. The fix is
a dedupe pass assigning each transitive includee to exactly one emitter — a
change to a module all 18 targets share, which needs its own lane and a
per-target A/B. That is the same reason X2 declined to make the module
transitive and hand-listed `_MILO_SCATTER_TRANSITIVE_PRUNE` instead. Rationale
is recorded in `native/CMakeLists.txt` rather than half-applied.

### 4.3 Landed, because it was free: 2 classes were **already in the binary**

`WorldCrowd` and `UIColor` were **already compiled and linked** into every
`rb3-milo`/`rb3-render` binary — `world/` and `ui/` are globbed in full — and
were missing nothing but a line in the hand-rolled factory list. `DirLoader`
printed `Can't make WorldCrowd` the entire time for a class sitting in the
binary.

★ `WorldCrowd3DCharHandle`, from a sibling header, **was** registered while
`WorldCrowd` was not. That is the tell: a hand-rolled registration list drifts
silently from the module `Init()` it replaces (`world/World.cpp:24` registers
`WorldCrowd` on X360). Retires **8 of the 684** misses.

### 4.4 Corrected cost

> **"A venue root loads" = one build-system change (scatter dedupe) + four
> source defects in one file.** Not "compile `src/band3/`".

The venue root was **not** retried, because `BandCamShot` — **611 of the 684
misses on its own** — is behind §4.2's blocker. Retrying without it would fail
for the same reason X4a failed and would prove nothing new.

---

## 5. Per-subsystem honesty table

Verdicts are bounded by what was actually observed.

| subsystem | verdict | evidence |
|---|---|---|
| **Clip decode** | ✅ **VERIFIED** | 44 real `CharClip` + 12 `CharClipGroup` load from shipped `char/crowd/anim/gen/female_base.milo_xbox`; `crowd_reaching_01` reports sane `StartBeat/EndBeat/NumFrames` (0.00..30.00, 61 frames) and its channels reach the servo (`StuffBones`), which the non-zero `.rotz` values at poll time confirm. |
| **Bone evaluation** | ✅ **VERIFIED** | All 39 bones satisfy the rigid-skeleton invariant, max ratio 0.9999, **max deviation 7.47e-05**. Every `det(local)` and `det(world)` = 1.000. Instrument self-validated on the bind-pose control (deviation 4.79e-05). |
| **World compose** | ✅ **VERIFIED** | Same measurement — the invariant *is* a world-compose test (`liveDist` is a world-space distance). It was the instrument that caught both §2 and §3, so it is demonstrably discriminating and not merely passing. |
| **Skinning palette** | ❌ **BROKEN — but ROOT-CAUSED, with the mechanism proven** | The posed mesh smears. Cause found and confirmed by a controlled A/B: every skinned mesh is **truncated to 4 bones at load** (§5.1). Not landed — the one-line fix has a 22-consumer blast radius and empties the posed frame (§5.2). |
| **Shadows (`ShadowPass`)** | ❌ **STILL UNREACHED — reachability now assessed** | X4a left this "pending a driven Character". A driven `Character` now exists, and it is **still not enough**: entering the pass needs `Character::DrawLodOrShadow`, and `rb3-render` draws by iterating `RndMesh` and calling `DrawShowing()` directly. So this is now a *consumer* gap with a named fix, not an unknown. |
| **Transparency ordering** | ❌ **UNREACHED — engine cannot reach it** | Unchanged from X4a §5: `QueueTransparentDraw` still has zero callers in the engine while `FlushTransparentDraws()` runs twice a frame. Re-checked at `138e160`; the two commits in the bump touch only the mesh name-filter seam. |
| **`Rnd`/`NgRnd` semantics** | ✅ **no new mismatches** | X3's eight-member audit still clean; the posed path introduced no new divergence. |

### 5.1 ★ The skinning smear — ROOT-CAUSED: bones are **deleted at load**

Symptom first: at `--beat 0.0` the character is **visually identical to bind
pose** and clean; as the pose deviates the mesh streaks, vertices trailing
behind. Correct-at-bind, degrading-with-deviation is the signature of *a subset
of bones never reaching the palette* — at bind the missing matrix and the true
matrix coincide, so the error is zero, and it grows exactly as the bone rotates
away.

**The cause is X3's unexplained warning, and it is not advisory — it is
destructive.**

```
female_crowd_body01_lod02.mesh: exceeds bone limit (20 of 4)
clap / fist / horns / lighter.mesh: exceeds bone limit (12 of 4)
```

`rndobj/Mesh.h:227`
```cpp
int MaxBones() const { return GetGfxMode() != kOldGfx ? 40 : 4; }
```
`rndobj/Mesh.cpp:567-578`, inside `RndMesh::Load`
```cpp
if (mBones.size() > max) {
    MILO_NOTIFY("%s: exceeds bone limit (%d of %d)", …);
    mBones.resize(MaxBones());     // <<< the bones are DESTROYED
}
```

`gGfxMode` is a **zero-initialised global** (`os/System.cpp:53`) — i.e. `kOldGfx`
— and the only thing that ever sets `kNewGfx` is `PreInitSystem`
(`os/System.cpp:505`), which every native driver deliberately skips. So
`MaxBones()` is **4**, and a 20-bone character is cut to 4 *at load*. The engine
then fills palette slots 4..39 with identity (`BoneSetup.cpp:256-261`) while
`object.world` is forced to identity for skinned meshes (`Mesh_Wgpu.cpp:245-252`)
— so vertices weighted to bones 4..19 sit at raw bind coordinates while bones
0..3 animate. That is the picture, exactly.

★ **The composition itself was audited and is correct**, so this is a deletion
bug and not a math bug: both palette builders compute
`skin = BoneOffsetAt(b) * bone->WorldXfm()` (`BoneSetup.cpp:218`,
`Rnd_Wgpu_RB3.cpp:3865`), and `RndBone::mOffset` genuinely is the inverse bind —
`Mesh.cpp:1076-1083` builds it as `meshWorld * Invert(boneWorld)`. xenon's and
DC3's `RndBone` are also byte-identical in shape, so the DC3-lineage engine
reading xenon's headers is safe here.

★ **This is the SECOND instance in this milestone of the same root-cause shape**
as §3: a `PreInitSystem`/`SystemInit` sub-init that the hand-rolled native
bring-up skipped, silent, latent since X2.

### 5.2 ⛔ Why the one-line fix is NOT landed — it was built and measured

`SetGfxMode(kNewGfx)` before the first load is the obvious repair. It was
implemented and A/B'd, and it is **not** a one-liner: `gGfxMode` has **22
consumers** across `Character`, `ShaderMgr`, `ShaderProgram`, `ShadowMap`,
`rndobj/Utl`, `world/Crowd` and others — a broad behavioural switch, not a bone
cap.

| leg | coverage | colours | verdict |
|---|---|---|---|
| bind pose, truncated (today) | 11.07% | 17960 | baseline |
| bind pose, `kNewGfx` | **15.78%** | 18882 | ✅ **better — and this CONFIRMS the diagnosis** |
| posed, truncated (today) | 23.12% | 21601 | smeared |
| posed, `kNewGfx` | **0.00%** | 1 | ❌ frame empty — geometry leaves the camera |

★ The bind-pose row is the confirming evidence: the extra 4.7 points of coverage
are precisely the vertices the truncation had pinned at bind coordinates, now
skinned by their real bones. The bones being restored is *demonstrably* the
right thing.

But the posed frame goes **empty**, and it is **not** the engine's
`numBones >= 8` "skin fling clamp" — re-measured with `RB3_NO_SKIN_CLAMP=1`,
still 0.00%. Something else gated on `kNewGfx` breaks the posed draw.

Landing it would trade a smeared character for **no** character and regress X3's
and X4a's evidence PNGs. So the finding is recorded in
`native/src/main_render.cpp` at the exact place the call belongs, with the
measurements, and handed to X4c. **The truncation is the cause of the smear;
`kNewGfx`'s blast radius is why the cure needs its own lane.**

---

## 6. The oracle, and two design choices that earned themselves

The acceptance instrument is the bone-length invariant. For a rigid skeleton

```
WorldXfm(child) = LocalXfm(child) * WorldXfm(parent)
  ⇒  |world(child) - world(parent)|  ==  |LocalXfm(child).v|     exactly
```

whenever the parent carries a pure rotation. **It needs no ground truth** — no
reference PNG, no retail capture, no judgement call. Any ratio ≠ 1.000 is a
*proof* that a transform on the compose path is wrong.

Both quantities are sampled at the **same instant, after the pose**. Sampling
`|LocalXfm().v|` at bind instead would fold in clip-driven translation channels
and turn signal into noise.

Two choices that were not obvious up front and both paid for themselves inside
this milestone:

1. **`det(local)` and `det(world)` columns.** They separate *a bad local pose*
   from *a bad world compose*. §3 was diagnosed almost entirely from the single
   observation `detLocal == 0.0000 on exactly the six .rotz bones` — which
   pointed straight at `MakeRotMatrixZ`, and from there at its trig.
2. **Rows sorted by DEPTH, not by badness.** A collapse propagates downward, so
   every descendant of a broken bone also fails, usually *worse*. Sorting
   worst-first put the consequences on top and hid the cause. Sorted by depth,
   the first row to leave 1.000 **is** the defect. The initial worst-first
   ordering actively delayed the diagnosis; this is recorded so nobody rebuilds
   it that way.

⚠ **The oracle is self-validating and was validated before being trusted**: on
the bind-pose control it reports max ratio 1.0000 / deviation 4.79e-05 over the
same 39 bones. A failing run is therefore signal, not an artefact of the
instrument. Running that control *first* is the step that made every later
number believable.

---

## 7. Engine-change requests — none blocking, zero edits made

1. **The transparent queue still has no producer.** Re-verified at `138e160`:
   `QueueTransparentDraw` (`TransparentQueue.cpp:111`) is called from nowhere in
   the engine, while `FlushTransparentDraws()` runs twice per frame. Carried
   verbatim from X4a §8.1 because it is unchanged, and it keeps that row of every
   subsystem table unfalsifiable.
2. **A skinning-palette bone-count contract.** The engine should either state its
   maximum bones-per-mesh and *report* when a mesh exceeds it, or size the palette
   from the mesh. Today a 20-bone mesh renders with no engine-side complaint at
   all — the only warning comes from xenon's `RndMesh`, and it is advisory (§5.1).
   Whatever the resolution, silent partial skinning is the wrong failure mode.
3. **Carried unfixed from X4a/X3, all re-observed:** the postproc grain seed is
   not headless-reproducible at `frames ≥ 2` (`RB3PostProc.cpp:429`);
   static-lifetime GPU caches segfault after a clean `rc=0` (both consumers
   independently `_exit()`); `GpuDevice` prints `device lost (reason 2)` before
   reporting successful init.

★ **Retired this milestone:** X3 §8.2 and X4a §8.3's `_lod` name-skip request. The
engine fixed it in `138e160`, and `rb3-render`'s compensating re-issue is gone (§8).

---

## 8. Rider — the pin bump, and the workaround it retired

**Pin `2ea8e343…` → `138e1606…`.** ⚠ The pin is a **soft, warn-only** check, so
the tree was *already building against* `138e160` — the bump records reality
rather than changing what compiles. The real A/B is therefore whether the
evidence PNGs, recorded in the `2ea8e343` era, still reproduce. **All four do,
byte-identical**, on freshly relinked binaries in a from-scratch worktree.

**Retired: the `_lod` DrawMeshImmediate re-issue** (`main_render.cpp` draw loop).
`138e160` moved both hardcoded content name-filters out of `RndMesh::DrawShowing`
and behind the `ShouldSkipMesh` seam, which rb3-xenon answers `false`
unconditionally (`rb3_render_glue.cpp:45`), so `DrawShowing` no longer filters
anything and the bypass is dead weight.

Retired **on a PNG A/B**, per the charter, not on the engine commit message:

| leg | `crowd_female01` | `tracksystem_meshes` |
|---|---|---|
| bypass ON | `30692a8d02c1ada0…` | `cbdb29fa95a5b574…` |
| bypass OFF | `30692a8d02c1ada0…` | `cbdb29fa95a5b574…` |

The A/B is **discriminating rather than vacuous**: `crowd_female01`'s body mesh is
precisely the mesh that took the bypass, so a behaviour change would have shown
there first. Coverage 11.07% / 17960 colours either way.

---

## 9. The `Refs()` sweep the charter asked for

Swept `src/` for the by-value-copy-of-an-intrusive-ring-head shape that cost
X4a a silent 11-minute hang.

| finding | result |
|---|---|
| by-value binds of `Refs()` | **1**, and it is the known one — `world/Instance.cpp:342`, which sits in the **`#else` (X360 match) arm**. The `HX_NATIVE` arm above it already uses X4a's `MoveBefore`/`ReplaceList` fix. **Confirmed fixed; nothing new.** |
| range-`for` over `Refs()` | **0** true C++11 range-fors. All 9 `for (… Refs() …)` sites call `.begin()`/`.end()` directly on the live reference — correct. |
| iterator/sentinel taken by value | 6 sites, all **correct by construction** (`end()` is the address of the live head, so copying the *iterator* is fine). |
| explicit `const ObjRef &` binds | 5, all correct. |

⚠ **One latent instance found, in code not yet compiled here:**
`bandobj/BandCharacter.cpp:2255` (inside its `HX_NATIVE` arm) treats `Refs()` as
a `std::vector<ObjRef*>` — `.size()`, `operator[]`. Against xenon's `ObjRef` it
cannot even compile, which is why it is item 3 of §4.1's defect list rather than
a runtime hang. **It will become a live hang the moment that TU is wired in**,
so it must be fixed as part of §4.2's work, not after it.

Also flagged: `obj/ObjMacros.h:730-736` calls `.rbegin()`/`.rend()` on `Refs()`,
which `ObjRef` does not provide — dead or unexpanded today, same trap tomorrow.

---

## 10. Retracted hypotheses, with evidence

Recorded because a narrated dead end is worth more than a silent one.

1. ⛔ **"The 14 blocking classes are band3, so a venue needs `src/band3/`"** (X4a §3,
   §9, §10.1 — its single most load-bearing structural claim). **Retracted.**
   Zero of the 14 are in `src/band3/`; they are `bandobj/` (5), `synth/` (7),
   `world/` (1), `ui/` (1). Two were already linked. Evidence: §4.
2. ⛔ **"`MakeRotMatrixZ` is wrong."** Retracted mid-investigation. The function
   is a correct pure-Z rotation and matches rb3-Wii's byte for byte. The
   determinant it produced was 0 because **its inputs** were 0 — the defect was
   two levels down, in an uninitialised lookup table (§3). Reading the callee
   before measuring its inputs would have "fixed" a correct function.
3. ⛔ **"Sort the bone table worst-first."** Retracted as an instrument design
   error (§6.2). It ranks consequences above causes and demonstrably delayed the
   §3 diagnosis.
4. ✅ **Promoted, not retracted:** the bone-limit warning *does* cause the
   skinning smear. It began this milestone as a suspect with a matching
   signature and ended it **confirmed** — the mechanism is read off the source
   (`Mesh.cpp:567-578` deletes the bones) and the A/B in §5.2 shows restoring
   them recovers exactly the missing geometry at bind. Recorded here because
   the *strength of the claim changed* mid-milestone, and §5 states the final
   strength rather than the initial one.
5. ⚠ **Explicitly NOT claimed:** that `SetGfxMode(kNewGfx)` is the fix. It
   restores the bones and improves bind pose, and it empties the posed frame
   (§5.2). What breaks is unidentified.

---

## 11. Owed work / handoff

| item | why | owner |
|---|---|---|
| **Skinning palette** — the cause is known (§5.1); find what else `kNewGfx` breaks | The one broken subsystem. Root cause is settled, so this is now a bounded question: bisect the 22 `GetGfxMode()` consumers, or restore `MaxBones()` without the global mode flip. Bind pose already proves restoring the bones is right. | **X4c** |
| **`ScatterIncludes.cmake` dedupe pass** | Unblocks all 10 clean `bandobj`/`synth` TUs at once, and with them `BandCamShot` = 611 of 684 venue misses. Needs a per-target A/B across all 18. | **X4c / build lane** |
| **4 root defects in `bandobj/BandCharacter.cpp`** | The other half of the venue unblock (§4.1). Includes the latent `Refs()` hang (§9). | X4c |
| **`Character::DrawLodOrShadow` draw path** | `rb3-render` iterates meshes and calls `DrawShowing()`; shadows and LOD selection both need the `Character` draw entry point. Now the *only* thing between a driven character and `ShadowPass`. | X4c |
| **Audit the other 16 drivers for missed `SystemInit` sub-inits** | `TrigTableInit` was one item in a list of eight that `SystemInit` runs (`ObjectDir::Init`, `ThreadCallInit`, `GeoInit`, `TrigInit`, `SpewInit`, `TheLocale`, `CheatsInit`, `FileCache::Init`). §3 fixed the one that bit. **Nobody has checked the other seven.** | native |
| **`WorldInstance::PreLoad/PostLoad` rev ordering** (76.92% / 59.07%) | Carried from X4a §9, untouched. Runtime-neutral; wants an A/B, not a theory. | match lane |
| Carried from X3/X4a, untouched | `ObjOwnerPtr` `operator<<` recurrence trap; `DirLoader.cpp:1014` `FilePath` into varargs; `world/BeatClock.cpp` `SongPos`/`Phrase`; `_inactive_crash_gem_top.mesh` 0 verts; `NodeCmp` A/B | — |

---

## 12. Recommended X4c shape

1. ★ **Finish the character before widening.** Exactly one subsystem is broken,
   its cause is **already root-caused** (§5.1) and the fix direction is proven
   correct at bind pose (§5.2). What remains is bisecting which of the 22
   `GetGfxMode()` consumers empties the posed frame -- or bypassing the global
   flip entirely and restoring only `MaxBones()`. This is the cheapest
   high-value work available and it completes a claim already most of the way
   made.
2. **The venue unblock is now a costed, two-item worklist** (§4.4), not the open
   structural question X4a left. One build-system change plus four defects in one
   file. It is no longer "compile `src/band3/`" and should not be scoped that way.
3. ★ **Assume more silent init gaps.** §3 was not a subtle bug — it made *all*
   trig return zero across *every* native target — and it survived four
   milestones because nothing asserted. The other seven `SystemInit` sub-inits
   have never been checked. A boot-time invariant check (`assert(Sine(π/2) ≈ 1)`)
   would have caught it in X1.
4. **Prefer oracles over pictures.** Every real finding here came from the
   invariant, not from looking at the render: the PNG at beat 4 looked
   *plausible* while six bones were singular and the world matrices had reached
   `-3.9e14`. Pick the invariant before the screenshot, and validate it on a
   known-good control before trusting a failure.
5. **Keep the oracle in the loop** (standing advice from X3 §9.5, X4a §10.5, and
   it held again): rb3-Wii's `Rot.cpp` was the witness that made §2's fix
   pattern obvious, and rb3-Wii's `Trig.cpp` confirmed §3's `MakeRotMatrixZ` was
   innocent.
