# X14 — the band lands on its marks; the repair was already in the tree and was never called

**Date:** 2026-08-03
**Predecessor:** [X13](x13-animated-pose-2026-08-03.md) "the hands survive animation; the band does not survive placement"
**Branch:** `x14-band-placement`, rebased onto `main` @ `eec0cb39`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** ONE native driver file (`native/src/main_render.cpp`) + **one shared `src/` file** (`src/system/bandobj/BandCharacter.cpp`, entirely inside an existing `#ifdef HX_NATIVE` region; X360 A/B clean).

---

## Verdict

★★★ **THE BAND DRAWS AT ITS MARKS, IN TWO VENUES, PROVED BY SET IDENTITY.** Four
distinct drawn `hands_naked` centroids tracking the four shipped slot
transforms, in `small_club_01` and `arena_01`; the `arena_01` drummer's riser is
in the **rendered** result. §2

★★★ **THE REPAIR WAS ALREADY IN THIS TREE, ALREADY DEFAULT-ON, AND NEVER RAN.**
`BandCharacter::RebindOutfitBonesToOwnSkeleton()` (`bandobj/BandCharacter.cpp:769`)
is called from `BandCharacter::Poll()` (`:533`). This harness polls a character
only from `DriveSceneCharacters`, which — correctly — polls only characters it
drove with a clip. The band has no reachable clip, so it is never polled, so the
repair never fired. **The two milestones on the charter are one defect.** §3

★★★ **THE SKELETON IS NOT MERELY UNPLACED — THERE IS ONLY ONE OF IT.** All four
members' `bone_L-hand.mesh` is the **same object, bit-identical pointer**. §1

⛔ **RETRACTED, X13's: "`Rnd_Wgpu_RB3.cpp:4077` … `Rnd_Wgpu_RB3.cpp` wins the
link at `:6133`."** That TU is **not compiled into this binary at all**. The
conclusion it supported survives; the citation does not. §4.1

⛔ **RETRACTED, MINE, MID-LANE: "the full-scope rebind does not shard."** It
shards the hair by 7–14×. I had checked only the hands. §4.3

⛔ **AND THE MECHANISM I THEN FOUND IS NOT THE ONE ON FILE.** The shard is a
**partial rebind**, not a rotation-basis mismatch. §5

⚠ **WHAT DID NOT LAND: the hair still draws at the venue origin, and the band is
untextured.** Both named, measured, and the second is shown not to be mine. §6

---

## 1. What the band's skeleton actually is

### 1.1 ★★★ One skeleton, four members — pointer identity

New instrument `--char-topo` (`main_render.cpp`, `ReportCharacterTopology` +
`ReportChainRoots` + `ReportRebindFeasibility`). Walking `bone_L-hand.mesh`'s
`TransParent` chain to its root, for each member (`x14-E-chain-paths.log`):

| member | `bone_L-hand.mesh` object | chain root [8] | chain root [9] |
|---|---|---|---|
| player0 | `0x5625e4497ac0` | `0x5625e43f8730` | `0x5625e43fb7b0` |
| player1 | `0x5625e4497ac0` | `0x5625e43f8730` | `0x5625e43fb7b0` |
| player2 | `0x5625e4497ac0` | `0x5625e43f8730` | `0x5625e43fb7b0` |
| player3 | `0x5625e4497ac0` | `0x5625e43f8730` | `0x5625e43fb7b0` |

★★★ **Bit-identical, all four.** X13 read these two terminal links as "an
unnamed `Character`" and could not say which object it was. They are the root
dirs of `char/main/skeleton.milo` → `char/main/skeleton_unshared.milo`, both
with `Dir() == NULL` — root-loaded, not objects in any dir. This is a stronger
statement than "the placement is not on the bone chain": there is only **one
figure's worth of bones in existence** for the whole band, which is the direct
mechanical reason "only one member reads as a complete figure."

★ **The comparative control, in the same block.** `crowd_male01`'s chain root is
`crowd_male01` **itself** (`selfPath='char/crowd/crowd_male01.milo'`,
`Dir()='world/shared/chars.milo'`) — an object inside the venue tree. The crowd
is built the way the band is not.

### 1.2 ★★ Both skeletons exist — which is what makes a repair possible

`ReportChainRoots`, per member (`x14-F-chainroots.log`):

| member | shared root (`char/main/skeleton.milo`) | own placed root (`char/main/main.milo`) |
|---|---|---|
| player0 | `0x558646600eb0` world (0,0,0) — **n=531** | `player0` world (−70.00, 80.66, 13.50) — n=559 |
| player1 | `0x558646600eb0` — **n=544** | `player1` world (14.43, 146.13, 13.18) — n=569 |
| player2 | `0x558646600eb0` — **n=514** | `player2` world (−10.03, 31.39, 13.22) — n=521 |
| player3 | `0x558646600eb0` — **n=516** | `player3` world (68.77, 51.44, 13.25) — n=568 |

★ **Set identity again, in both directions**: the shared root is one pointer
across four members; the own roots are four distinct pointers at four distinct
slots. Each member already owns a full placed skeleton — **492 same-named bones,
the same number for every member.** The drawn meshes simply bind to the wrong
one.

### 1.3 ⚠ This confirms a cause already on file in another repo

`src/system/bandobj/BandCharacter.cpp:2477` (a comment landed by an rb3-Wii lane,
2026-06-06) states this mechanism, and
`rb3/docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` pins the share to
`share=true` non-inlined subdir references resolved by `DirLoader::Find` on the
FilePath string. **Reached here independently by pointer identity before that
document was read**, which is why it is reported as a confirmation rather than a
citation. That document also records the un-share attempts as **proven dead
ends** (skip-the-preload: inert; un-share at `LoadSubDir`: inert; prune the
subdir: strips all outfit bones; per-character deep copy: crash). ⛔ **This lane
did not retry any of them**, and a further reason not to is below: un-sharing
alone would produce four *unplaced* skeletons, because a fresh instance still
has `TransParent()==NULL`. Un-sharing is neither necessary nor sufficient.

---

## 2. ★★★ The result — set identity, two venues

Drawn geometry, skinned through the **shipped** `RndMesh::SkinVertex`
(`rndobj/Mesh.cpp:762`), not a re-derived convention.

**`small_club_01`** (`x14-P-club.log`):

| member | authored slot | drawn `hands_naked` centroid |
|---|---|---|
| player0 | (−70.003, 80.657, 13.495) | **(−70.02, 79.97, 53.54)** |
| player1 | (14.429, 146.133, 13.182) | **(14.55, 145.01, 45.35)** |
| player2 | (−10.026, 31.389, 13.218) | **(−10.03, 30.70, 53.26)** |
| player3 | (68.770, 51.436, 13.248) | **(68.84, 50.75, 53.29)** |

**`arena_01`** (`x14-P-arena.log`):

| member | authored slot | drawn `hands_naked` centroid |
|---|---|---|
| player0 | (−103.728, −522.716, 255.825) | **(−104.00, −523.35, 295.87)** |
| player1 | (−5.024, −46.208, **320.901**) | **(−5.02, −47.34, 353.07)** |
| player2 | (3.359, −531.800, 255.795) | **(3.56, −532.46, 295.84)** |
| player3 | (98.797, −533.737, 256.118) | **(98.93, −534.41, 296.16)** |

Baseline for comparison — the same measurement with `RB3_NO_BAND_REBIND=1`
(`x14-N-OFF.log`): (−0.00, 0.66, 40.08) / (−0.00, 0.51, 40.48) / (−0.00, 0.66,
40.08) / (−0.00, 0.66, 40.08). One point, four members.

### 2.1 ★★ Why `z` is the axis the claim is made on

The slot transforms carry **yaw**, so `drawn = slot + local` is only valid on the
rotation-invariant axis. That axis is `z`. Taking `drawn z − slot z`:

| member | small_club_01 | arena_01 |
|---|---|---|
| player0 | **40.045** | **40.045** |
| player1 | **32.168** | **32.169** |
| player2 | **40.042** | **40.045** |
| player3 | **40.042** | **40.042** |

★★★ **A per-member constant, identical to three decimals across two unrelated
venues.** The drawn geometry is a pure rigid transform of the authored slot. This
is the load-bearing evidence — not the four numbers looking approximately right.

### 2.2 ★★ The `arena_01` riser, in the rendered result, with its residual accounted for

- Authored riser: `320.901 − mean(255.825, 255.795, 256.118) = ` **+64.99** ✅ (X7 predicted +65 from asset bytes; X9 measured +65.07 on the objects)
- **Drawn** riser: `353.07 − mean(295.87, 295.84, 296.16) = ` **+57.11**
- Shortfall: **7.88**

⛔ **The shortfall is not slop, and it is not placement error.** player1's own
`drawn z − slot z` is 32.168 against the males' 40.045 — a difference of
**7.877**, measured independently *in the other venue*. The drummer is the
female member and her own gender-posed skeleton puts her hands 7.88 lower. The
riser is fully present in the drawn result; the residual is the drummer's own
figure. Two independent measurements reconciling to 0.003 is the check.

### 2.3 The frames

`x14-V-band-club-focus.png` (1600×900, `--focus hands_naked`) — four separate,
intact figures: vocalist at the mic, drummer behind the kit, guitarist by the
guitar, bassist stage-left. Against `x14-T-focus-OFF.png`, the identical camera
(target (−0.62, 88.39, 53.07), dist 202.18 in both) with the rebind off: **one**
figure standing amid all four members' props. Determinism ×2, `cmp`-identical.

---

## 3. ★★★ Why it was broken — and why that is also the `body_clips` answer

`RebindOutfitBonesToOwnSkeleton()` repoints each outfit skin mesh's bone slots
onto the member's own placed skeleton by **exact name equality** — no geometry,
transform or pose is computed. It is called from `BandCharacter::Poll()`.

The harness polls a character only inside `DriveSceneCharacters`, and X13
correctly restricted that to characters it actually drove with a clip (polling
un-`Enter()`ed drivers was X13's own SIGSEGV). The band binds to `body_clips`,
which holds **zero** `CharClip`s. So: **no clips → never driven → never polled →
the repair never ran → the band draws at the origin.** The charter's primary and
secondary milestones are the same defect seen from two ends.

### 3.1 ⛔ Why `Poll()` is *not* what the driver calls

Measured, not assumed. `bc->Poll()` SIGSEGVs (`rc=139`, gdb backtrace):

```
CharWeightable::Weight()  <- CharDriver::Poll() <- CharDriverMidi::Poll()
<- RndDir::Poll() <- Character::Poll() <- BandCharacter::Poll()
```

`Weight()` is `return mWeightOwner->mWeight` (`char/CharWeightable.h:18`) with no
null guard. The constructor seeds `mWeightOwner(this, this)`
(`CharWeightable.cpp:7`), but `Load` overwrites it with `d >> mWeightOwner`
(`:74`), which resolves NULL when the named owner is absent. ⛔ **Named as a
defect and left alone** — it is a separate fault and papering over it would hide
it. The driver calls the public, idempotent `RebindOutfitBonesToOwnSkeleton()`
directly instead, which is exactly what Poll does at `:533` minus the driver path
that crashes.

### 3.2 ★★ `body_clips` — it ships empty, by design, and the clips are elsewhere

The charter offered three possibilities; the answer is the third.

- `body_clips` is bound by `BandCharacter::SetContext("venue")`:
  `mDriver->SetClips(Find<ObjectDir>("body_clips", true))` (`BandCharacter.cpp:1874`).
- A binary scan of the whole shipped asset tree finds the string `body_clips` in
  **exactly one file**: `char/main/gen/main.milo_xbox`. It is a **runtime-filled
  container**, not a shipped clip set. Nothing is missing from it because nothing
  was ever in it.
- The clips live in per-instrument/gender/tempo/genre milos. `BandCharacter`
  builds those paths itself (`BandCharacter.cpp:2703-2718`):
  `char/main/anim/<inst>/body/<gender>/realtime_<genre>.milo`,
  `…/<tempo>_<genre>.milo`, plus `body_add` for drums. On disk:
  `char/main/anim/guitar/body/male/gen/` holds **16** files
  (`fast_banger`, `fast_dramatic`, `fast_rocker`, `fast_spazz`, …).
- The merge is gated on `if (!mGenre.Null() && !mTempo.Null())` and is reached
  through `OnSetFileMerger`. This harness plays no song, so no tempo/genre-driven
  body-clip load is ever requested. **Verified negatively too**: no
  `char/main/anim/*/body` path appears anywhere in the run log.

⇒ **Not a load-path gap and not a wrong set name.** ⚠ What is *not* established:
whether the merge would succeed if requested — that runs through the `FileMerger`
path X12/X13 flagged as gated on the `ObjPtrList` NULL-entry defect. Reported as
undecided rather than guessed.

---

## 4. ⛔ Retracted hypotheses, with their evidence

### 4.1 ⛔ X13's renderer citation — the file is not in the binary

X13 attributed the `skin·v` contract to `Rnd_Wgpu_RB3.cpp:4077` and stated
"`Rnd_Wgpu_RB3.cpp` wins the link at `:6133`". The charter flagged this as
suspect; it is wrong, and the evidence is direct:

- `native/CMakeLists.txt:983` forces `MILO_ENGINE_GPU_BACKEND dc3` (CACHE + FORCE).
- The engine adds `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` **only** under the `rb3`
  flavor (`CMakeLists.txt:373-374`).
- The build directory contains `Rnd_Wgpu.cpp.o` and **no** `Rnd_Wgpu_RB3.cpp.o`.
- `strings rb3-render` yields `Mesh_Wgpu.cpp` and `BoneSetup.cpp`; `Rnd_Wgpu_RB3.cpp`
  is absent, as are `RB3MeshCache` / `RB3MaterialBinder`.

★ **The conclusion survives; the mechanism differs.** The path that actually
executes is `Mesh_Wgpu.cpp:246-256`, which for a skinned mesh writes **identity**
into the object uniform outright ("Skinned: bone matrices already produce
world-space positions, so object transform must be identity to avoid
double-transform") rather than post-multiplying the palette by `inverse(meshWorld)`.
The palette is `Multiply(mesh->BoneOffsetAt(i), boneWorld, skin)` at
`BoneSetup.cpp:219`. So `worldPos = skin·v` either way, and X13's rendering
verdict stands on a citation that was not this build's.

★ **Free consequence, worth recording:** `RB3_HANDS_MITTEN` — which X13 flagged as
default-ON and synthesizing finger pose, contaminating X12's close-up — lives in
`Rnd_Wgpu_RB3.cpp` and is therefore **dead in rb3-xenon**. It cannot contaminate
any frame in this repo. Grepped: no occurrence in any compiled TU.

### 4.2 ⛔ Mine: "un-sharing the skeleton is the faithful fix"

Not attempted, and now argued against on this lane's own evidence rather than on
the prior repo's dead-end list. A freshly instanced `skeleton_unshared.milo` root
would still have `TransParent()==NULL` and sit at the origin — exactly as the
shared one does. **Un-sharing alone places nobody.** The placement reaches the
figure through the member's own `main.milo` skeleton, which already exists (§1.2).

### 4.3 ⛔ Mine, published in a commit message and corrected in the next: "no shard"

I enabled the full rebind scope and wrote *"shard extent checked, not assumed"*
on the strength of `hands_naked` bbox extent (50.44 → 50.45 / 50.47 / 50.23).
The hands were fine. **The hair was not**, and it is plainly visible in
`x14-T-focus-ON.png` as grey radiating spikes:

| mesh | extent, rebind OFF | extent, full rebind |
|---|---|---|
| `youngozzie_resource.mesh` (player0 hair) | 10.71 × 11.01 × 29.25 | **78.74 × 83.49 × 42.74** |
| `blownback_resource.mesh` (player1 hair) | 9.45 × 11.05 × 11.78 | **23.20 × 157.15 × 17.64** |
| `head.mesh` | 7.78 × 9.06 × 12.44 | 7.77 × 9.05 × 12.45 ✅ |
| `hands_naked.mesh` | 50.44 × 5.56 × 8.57 | 50.45 × 6.80 × 8.59 ✅ |

⛔ **I checked one mesh family and generalised.** The rule this house already has
— check the denominator of every green — applies to the *population* a check
covers, not just its count. Caught by looking at the rendered frame, which is the
cheapest instrument in the lane and the one I ran last.

### 4.4 ⛔ Mine: my own `UNREPAIRABLE … (inert)` labels were partly wrong

`ReportRebindFeasibility` labels an unrepairable bone slot "inert" when no vertex
weights it, reading weights from `mesh->Verts()`. ⛔ **A mesh whose geometry is in
`mCompressedVerts` has an empty `Verts()`, so every one of its slots reads as
zero-weight and is labelled inert.** The hair bones were labelled inert and then
tore. I identified this hole when I wrote the check and did not close it; it is
recorded here rather than quietly fixed, because the numbers it produced are in
this lane's logs. The `bone_legs_*` "WOULD TEAR" rows for player1 were correct
(those meshes are uncompressed) and did predict a real tear.

---

## 5. ★★ The shard mechanism is a partial rebind, not a rotation basis

The in-tree comment and the rb3 investigation both attribute the full-scope shard
to the **animated** per-member bone's rotation basis differing from the static
magnet the authored offsets were baked against. ⛔ **That mechanism cannot be
operating here: nothing is animated** (the band has no reachable clip at all).
It sharded anyway, so a second mechanism exists, and it is in the loop:

Bone slots are rebound one at a time; a slot whose name does not resolve under
this member (`!own`) is silently `continue`d and left on the shared magnet. A
mesh with a **mix** of resolvable and unresolvable bones therefore ends up half
on the member's placed skeleton and half at the venue origin, and its weighted
vertices are stretched between the two.

Measured: the unresolvable slots are exactly the `bone_hair_*` set — present in
the shared skeleton, absent under the member's own root. Hair meshes mix hair
bones with `bone_head`; hence hair, and only hair, tore.

**Fix: require every slot to resolve before touching any of them.** This removes
the tear by construction rather than by a name whitelist. Scoped to the
full-scope arm; the shipped torso path is untouched. After
(`x14-U-allornothing.log`): both hair meshes are back to their shipped extents
**exactly** (10.71 × 11.01 × 29.25 and 9.45 × 11.05 × 11.78), and heads still
land — head centroids −70.09 / 14.75 / −10.02 / 69.13, four distinct.

⚠ **This does not refute the rotation-basis mechanism for the animated case.**
Two mechanisms can produce one symptom. Nothing here licenses turning the full
scope on when a clip is playing, and the driver explicitly does not: it selects
the full scope **only while no clip is being driven**, and prints which arm it
took and why.

---

## 6. ⛔ What did NOT land

1. ⛔ **The hair still draws at the venue origin, and the band is bald.** A mesh
   that fails the all-or-nothing test is left exactly as shipped — still bound to
   the shared skeleton. The two hair meshes' centroids are (−0.04, 0.76, 66.41)
   and (0.21, −0.76, 67.73), i.e. the origin, and they are visible mid-stage in
   `x14-V-band-club-focus.png`. A smaller and fully characterised defect than an
   80-unit shard, but a defect. The repair is to give each member its own
   `bone_hair_*` set, which belongs to the load path.
2. ⚠ **The band renders untextured (pink). NOT caused by this lane, and shown so
   rather than asserted:** texture-resolution failures are **identical** with the
   rebind on and off — 419 "couldn't find"/unnamed-bitmap lines, 40 "Can't make
   `OutfitConfig`", **203 of 411 meshes issued a draw** in *both* arms. The
   stacking was hiding three of four figures; separating them exposes a
   pre-existing defect. `OutfitConfig` is the 48-symbol gap carried since X10.
3. ⛔ **`CharWeightable::mWeightOwner` resolves NULL after `Load`**, so
   `BandCharacter::Poll()` cannot be called at all. Named, not fixed, not
   worked around. §3.1
4. ⛔ **Whether the tempo/genre body-clip merge would succeed if requested is
   UNDECIDED** — it runs through the `FileMerger` path gated on X12's `ObjPtrList`
   NULL-entry defect. §3.2
5. ⚠ **Band hand pose under animation remains UNDECIDABLE**, for the same reason
   X13 gave. This lane makes it *reachable in principle* (the clips are located,
   §3.2) but plays none.
6. ⚠ **A conclusive ceiling-legs frame was not rendered.** X13's medium-confidence
   benign verdict stands untouched.
7. ⚠ **Direction-B rows, `OutfitConfig`, `CharMeshHide::HideAll`, 42 orphan files,
   `BandCamShot`, `Transform::LookAt`, `Invert(Matrix4)`** — all carried,
   untouched.
8. ⚠ **The foreign uncommitted engine edit `src/platform/FxSendNative.cpp` is
   still there, still not mine, left untouched — eleventh lane running.**

---

## 7. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Band drawn geometry lands on its marks** | ★★★ **VERIFIED — 4 distinct centroids, 2 venues** | §2 |
| **Proof shape** | ★★★ **SET IDENTITY + a per-member constant identical across venues** | §2.1 |
| **`arena_01` riser in the RENDERED result** | ★★★ **YES — +57.11 drawn vs +64.99 authored, residual reconciled to 0.003** | §2.2 |
| **Which renderer path executes** | ★★★ **`Mesh_Wgpu.cpp` + `BoneSetup.cpp` (dc3 flavor)** | §4.1 |
| **X13's `Rnd_Wgpu_RB3.cpp` citation** | ⛔ **REFUTED — TU not compiled; conclusion survives** | §4.1 |
| **`RB3_HANDS_MITTEN` contamination in rb3-xenon** | ✅ **NONE — the flag's TU is not in this binary** | §4.1 |
| **Number of band skeletons in existence** | ⛔ **ONE, shared — bit-identical pointers** | §1.1 |
| **A per-member placed skeleton exists** | ✅ **YES — 4 distinct roots, 492 same-named bones each** | §1.2 |
| **Root cause of the non-placement** | ★★★ **The landed repair is never invoked (band never polled)** | §3 |
| **`body_clips` is empty because…** | ★★★ **it ships empty — a runtime-filled container; clips are in `char/main/anim/<inst>/body/<gender>/`** | §3.2 |
| **Would the body-clip merge succeed?** | ⛔ **UNDECIDED — gated on the `ObjPtrList` defect** | §3.2 |
| **`BandCharacter::Poll()` usable?** | ⛔ **NO — SIGSEGV, null `mWeightOwner`. Named, not fixed** | §3.1 |
| **Full-scope shard mechanism** | ⛔ **PARTIAL REBIND, not rotation basis (in the un-animated case)** | §5 |
| **Shard after the fix** | ✅ **NONE — hair extents exactly the shipped values** | §5 |
| **Hair placement** | ⛔ **STILL AT THE ORIGIN — disclosed defect** | §6.1 |
| **Band texturing** | ⚠ **PRE-EXISTING — 419/40/203 identical in both arms** | §6.2 |
| **Un-share the skeleton at load** | ⛔ **NOT ATTEMPTED — and argued unnecessary AND insufficient** | §4.2 |
| **My "no shard" claim** | ⛔ **RETRACTED — hands-only, hair blew up 7–14×** | §4.3 |
| **My "inert" weight labels** | ⛔ **PARTLY WRONG — compressed-vert denominator hole, disclosed** | §4.4 |
| **`BandCharacter.cpp` X360 blast radius** | ✅ **ZERO — 1796/1796 `.text` sections identical** | §8 |
| **`BandCharacter.cpp` objdiff position** | ✅ **SCOREABLE — unit `default/BandCharacter` exists** | §8 |
| **Prior lanes' frames** | ✅ **NON-REGRESSED — control byte-identical to X13's artifact** | §8 |

---

## 8. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18 at the branch point** (`962a9200`) — ⛔ **17/18 after rebasing onto `eec0cb39`, and the one failure is `main`'s, PROVED** (§8.1) | `x14-gate-land.log`, `x14-gate-postrebase.log`; **0 SKIPs in both**; stale binaries deleted first; cache seeded with **all four** flags |
| b | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606…` unmoved | foreign `FxSendNative.cpp` edit disclosed, left alone — **eleventh lane** |
| c | Shared-`src/` X360-faithful at symbol granularity | ✅ **PASS** — **1796/1796 `.text` sections byte-identical**, 4087/4088 overall; sole delta `.debug$S` (168 vs 160 B) | both objects built **in this worktree**; the whole edited region is inside an existing `#ifdef HX_NATIVE` |
| c2 | objdiff position of the touched TU | ✅ **SCOREABLE** — unit `default/BandCharacter` is in `objdiff.json`, and the `.text` A/B says the X360 side did not move | stated, not implied |
| d | PNG determinism ×2 | ✅ **PASS** — `cmp`-identical, both venues and the focused frame | `x14-R-band-club.png`, `x14-R-band-arena.png`, `x14-V-band-club-focus.png` |
| e | Prior evidence non-regressed, vs **artifacts** | ✅ **PASS** — with `RB3_NO_BAND_REBIND=1` the club frame is **byte-identical** to X13's `x13-E0-band-club.png` | the strongest available control: my change is provably the only difference |
| f | Was `main` broken by a decomp lane? | ⛔ **YES — `rb3-song` does not link on pristine `main`** | §8.1 |

### 8.1 ⛔ `main` @ `eec0cb39` does not build — and it is not this lane

The pre-rebase gate was **PASS 18/18** at the branch point `962a9200`. After
rebasing onto `eec0cb39` it is **17/18**: `rb3-song` fails to link.

```
BandSongMgr.cpp:(.text+0xf4):   undefined reference to `Jukebox::Jukebox(int)'
BandSongMgr.cpp:(.text+0x2ec):  undefined reference to `Jukebox::Jukebox(int)'
BandSongMgr.cpp:(.text+0x63b8): undefined reference to `Jukebox::Play(int)'
```

⛔ **Proved to be `main`'s, not attributed to `main`'s by argument.** A pristine
worktree was created and **detached at `eec0cb39` with none of this lane's
commits**, configured with the same four flags, and `ninja rb3-song` reproduces
the identical three undefined references (`x14-mainprobe-song.log`). This lane
touches two files, `native/src/main_render.cpp` and
`src/system/bandobj/BandCharacter.cpp`; `git diff main -- src/band3/meta_band/BandSongMgr.cpp
native/CMakeLists.txt` is **empty**.

Origin: `c6e8408f` "match(BandSongMgr::Handle): lane DP-1" introduced `Jukebox`
call sites into `BandSongMgr.cpp` without adding the `Jukebox` TU to the
`rb3-song` target's source list. **It blocks the gate for every lane**, so it is
reported here rather than fixed — folding another lane's target wiring into this
commit would misattribute it. The fix is to add `Jukebox.cpp` to `rb3-song` in
`native/CMakeLists.txt`.

All 17 other targets, **including `rb3-render`** — the only one this lane's
evidence depends on — are OK, and there are **0 SKIPs**.

⚠ **The comparator used for gate (c) was VACUOUS on its first version** — it read
`section['data']`, which the repo's `COFFParser` does not populate, and declared
`BandCharacter.obj` and `BandWardrobe.obj` byte-identical. It now reads section
bytes by `raw_offset`/`raw_size` and **runs a positive control first, refusing to
report the A/B unless it detects a difference it is known to have**
(`x14_sec_cmp.py`). Verified the `.text` sections carry real bytes: 1796 sections,
all with `raw_size > 0`, 145,228 bytes total, largest 5,328.

⚠ **The `--revert` trap was hit once and caught.** `BandCharacter.cpp` was
`git checkout`ed for the X360 baseline object and restored; the restore was
confirmed by `cmp` against the saved patch **and** by `git status --short` showing
the file modified again, before any measurement.

⚠ **Read the gate's own verdict line, not the pipeline's exit code** — carried
from X12/X13; `grep -c SKIP` exits 1 on zero matches, i.e. the failure code *is*
the 0-SKIPs result.

---

## 9. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Give each member its own `bone_hair_*` bones** | §6.1: the only remaining meshes drawing at the origin. The all-or-nothing rule now names them precisely (`SKIP (partial)` lines under `SKEL_REBIND_PROBE=1`), so the target list is mechanical. | X15 |
| ★★★ **`CharWeightable::mWeightOwner` resolves NULL after `Load`** | §3.1: it blocks `BandCharacter::Poll()` entirely, and Poll is the shipped home of the rebind, the animation, and the per-frame outfit sync. Fixing it makes the driver's direct call unnecessary. **Highest-leverage single item on this list.** | X15 |
| ★★ **Band texturing (419 failures, 40 `Can't make OutfitConfig`)** | §6.2: pre-existing, now conspicuous because four figures are visible instead of one. `OutfitConfig` is the 48-symbol registration gap carried since X10. | X15 |
| ★★ **Play a real body clip on the band** | §3.2: the clips are now *located* (`char/main/anim/<inst>/body/<gender>/gen/`, 16 per set). Loading one directly — as X4b did with `--clips` — would make band hand pose under animation decidable for the first time. ⚠ And it would re-open §5: the full rebind scope must NOT be assumed safe under animation. | X15 |
| ⚠ **`ObjPtrList` NULL-entry defect** | §3.2: still the gate on the whole wardrobe `FileMerger` path. Unchanged since X12. | its own lane |
| ⚠ **`Rot.cpp`/`mtx.cpp` alias hazards, Direction-B rows, orphans, `BandCamShot`** | carried from X12/X13, untouched. | as before |
| ⚠ **Engine CR: none filed** | This lane needed no engine change. `RB3_HANDS_MITTEN` turned out to be unreachable here (§4.1), which retires X13's engine-CR item **for rb3-xenon only** — it still stands for rb3-Wii. | — |

---

## 10. Recommended X15 shape

1. ★★★ **Before building a repair, grep for one.** The fix for the headline defect
   was already in this tree, default-ON, complete, and correct. It had never
   executed. Three lanes measured around it. The cheapest possible instrument —
   run the existing probe and see whether it prints anything — was not run until
   this lane, and it was the whole answer.
2. ★★★ **A probe that is silent on the failing case is worse than no probe.** The
   rebind's own report was gated on `meshes > 0 || reboundBones > 0`, so "the
   collector reached nothing" printed *nothing at all* and read as "the probe is
   off". Same family as X12's vacuous PASS and X13's zero-mesh gate — **fourth
   consecutive lane**. Report the zero loudly, and separate pre-filter from
   post-filter denominators.
3. ★★★ **Look at the frame.** Two of this lane's numeric greens (§4.3, §4.4)
   survived every measurement I took and died the moment a 1600×900 PNG was
   opened. Render early, not as the last artifact.
4. ★★ **Pointer identity beats naming.** "An unnamed `Character` at the origin"
   was carried for a lane and could not distinguish "unplaced instance" from "one
   instance for all four". One `%p` settled it.
5. ★★ **Two mechanisms can make one symptom.** The shard had a documented cause
   that was real and was *not* the cause operating here. Ruling a mechanism out
   for your case does not refute it for theirs — say which you have shown.

---

## 11. Evidence

All under `/home/free/tmp/laneX14/evidence/`.

| file | what it shows |
|---|---|
| `x14-E-chain-paths.log` | the bone chain walked to its root with **pointer identity** and each link's owning dir + on-disk path — §1.1 |
| `x14-F-chainroots.log` | per-character chain-root census: one shared root, four distinct placed roots — §1.2 |
| `x14-G-rebind-feas.log`, `x14-H-unrepairable.log` | rebind feasibility; 492 same-named bones per member; the unrepairable slot names — §1.2, §4.4 |
| `x14-J-collector.log`, `x14-M-targets.log` | the collector denominator the probe could not previously report — §10.2 |
| `x14-N-OFF.log` / `x14-N-TORSO.log` / `x14-N-FULL.log` | the three-arm A/B on drawn centroids and bbox extents — §2, §4.3 |
| `x14-P-club.log`, `x14-P-arena.log` | the two-venue set identity and the riser — §2, §2.2 |
| `x14-U-allornothing.log` | the tear removed, hair extents back to shipped values — §5 |
| `x14-V-band-club-focus.png` | **the result** — four separate figures on their marks (determinism ×2) |
| `x14-T-focus-OFF.png` | the same camera with the rebind off — one figure amid four members' props |
| `x14-R-band-club.png`, `x14-R-band-arena.png` | both venues, wide (determinism ×2) |
| `x14-S-control-rebindoff.png` | **byte-identical to X13's `x13-E0-band-club.png`** — the non-regression control |
| `BandCharacter.obj.BASE` / `.FIX`, `x14_sec_cmp.py` | the X360 A/B and its positive control — §8 gate (c) |
| `x14-gate-land.log`, `x14-gate-postrebase.log`, `x14-mainprobe-song.log` | gates, and the proof that `rb3-song` is broken on pristine `main` — §8.1 |
