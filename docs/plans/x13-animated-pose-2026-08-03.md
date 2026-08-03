# X13 — the hands survive animation; the band does not survive *placement*

**Date:** 2026-08-03
**Predecessor:** [X12](x12-hand-pose-2026-08-03.md) "the hands are correctly posed; the instrument that said otherwise was measuring light targets"
**Branch:** `x13-animated-pose`, rebased onto `main` @ `512ee560`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** ONE native driver file (`native/src/main_render.cpp`) + **one shared `src/` file** (`src/system/math/Rot.cpp`, `#ifdef HX_NATIVE` arm only, X360 A/B clean).

---

## Verdict

★★★ **UNDER A REAL SHIPPED `CharClip`, THE HAND POSE IS CORRECT.** Eight crowd
members driven by `crowd_realtime_idle_10` at beat 4.0 move substantially — the
left hand travels 15.4 units from bind — and every arm segment length is
preserved *exactly* (6.211 / 9.584 / 10.105, identical bind→animated), with the
recompose identity exact at **0.000e+00 over 7380 admissible bones**. §1

⛔ **THE BAND CANNOT BE ANIMATED AT ALL, AND THAT IS A NAMED DEFECT.** All four
members' `CharDriver` is bound to `body_clips`, which contains **zero
`CharClip`s**. The crowd's bound sets hold 40–44. So for `player0..3` the
animated verdict is **UNDECIDABLE**, and it is undecidable for a structural
reason, not a measurement one. §1.3

★★★ **MILESTONE 2 SOLVED — AND IT IS NOT BENIGN.** The band's skeleton is
parented to an **unnamed `Character` at the origin**; the placed transform is
not on the bone chain. The palette is therefore character-LOCAL, the renderer's
contract draws skinned geometry at `skin·v` (meshWorld is deliberately
cancelled), and **all four band members render stacked at the venue origin**.
Four members at four slots spanning ~140 units all report the same drawn
centroid. §2

★★ **`Rot.cpp:299` FIXED, DEMONSTRATED BOTH WAYS, X360 UNMOVED.** The unfixed
arm returns `y = -2.000` — the already-written `x` — under aliasing; the fixed
arm returns the reference exactly. All **9378 `.text` sections byte-identical**
across the A/B. §3

⛔ **RETRACTED, MINE, MID-LANE: "the recompose identity fails under animation
(3.473e+00 at `bone_pelvis.mesh`)."** That deviation was my own instrument
polling characters it had never started. Same clip, same beat, fixed loop:
**0.000e+00**. §4

⚠ **WHAT DID NOT LAND: Direction-B rows 4 and 3b were not guarded.** §6

---

## 1. Milestone 1 — hand pose under animation

### 1.1 The instrument, and why it is not X12's

X12's oracle is reused for the deep walk (`CollectDeep`), the absolute arm-chain
positions, and the shipped `RndMesh::SkinVertex`. Three things were changed
because the bind-pose version cannot be trusted under a clip:

⛔ **The recompose identity `W == L·parentW` is NOT the engine's world rule.**
`RndTransformable::WorldXfm_Force` (`rndobj/Trans.cpp:666-687`) selects among
**four** compositions on `mConstraint`, and may then overwrite `mWorldXfm`
entirely via `ApplyDynamicConstraint()`; `SetWorldXfm` (`Trans.cpp:419`) writes
it directly and clears `mDirty`, which is how IK publishes a solved bone. At
bind pose none of those arms fire — which is *why* X12 measured an exact zero
over 7434 bones and could not have seen this. The check is now split into
**admissible** (`kConstraintNone`, no dynamic constraint) and **excluded**
populations, both printed. A deviation on an excluded bone proves nothing and is
no longer allowed to look like a failure.

⛔ **A new gate, `handpose-measured-hand-geometry`.** X12's oracle passes
`handpose-reached-a-skeleton` on the crowd while measuring **zero hand meshes** —
a partial vacuity of exactly the shape X12 documented in the old `--bone-audit`.
The mesh denominator is now gated too, and it duly FAILs the band-absent
configuration.

⚠ **The bone-length-ratio oracle was not used**, per the charter. Segment
lengths appear below only as a *supplementary* reading, and are labelled as
what they are: invariant under rigid motion, therefore necessary and not
sufficient.

### 1.2 ★★★ The measurement — the crowd, driven

`small_club_01`, `--scene-clip crowd_realtime_idle_10 --beat 4.0`, 8 characters
driven, 41 polls (`x13-B-anim.log`). `crowd_male01`, left arm:

| bone | bind pose | animated (beat 4.0) | segment length bind → animated |
|---|---|---|---|
| `bone_L-upperArm.mesh` | (−7.406, −1.001, 57.537) | (−5.716, **8.576**, **47.528**) | 6.211 → **6.211** |
| `bone_L-forearm.mesh` | (−14.396, −1.666, 51.015) | (−13.808, 4.012, 45.176) | 9.584 → **9.584** |
| `bone_L-hand.mesh` | (−21.746, 0.025, 44.289) | (**−8.703**, 2.549, **36.579**) | 10.105 → **10.105** |
| `bone_R-hand.mesh` | (21.745, 0.055, 44.294) | (11.815, 2.478, 35.399) | 10.105 → 10.104 |

⛔ **The vacuity check first.** A green under animation is worthless if nothing
moved. The hand travelled **15.4 units** and the whole arm re-oriented (z 57.5 →
47.5, y −1.0 → +8.6). The pose genuinely changed; the diff of the two oracle
blocks is 112 lines (`x13-bind-vs-anim.diff`).

At that same instant: **recompose worst dev 0.000e+00 over 7380 admissible
bones**, 54 excluded as constrained/dynamic, 12 hand meshes measured.

### 1.3 ⛔ The band could not be driven — `body_clips` is empty

`DriveSceneCharacters` deliberately searches **only** the driver-bound set
(no cross-set fallback, so a male never gets a female clip). The old skip
message enumerated *every* set on the character, so a reader picking a clip name
off that line was reading names that could never be selected. It now prints the
bound set's own inventory — which is how this was found:

| character | bound set | clips in the bound set |
|---|---|---|
| `crowd_male01..04` | `male_base` | **40** |
| `crowd_female01..04` | `female_base` | **44** |
| `lighttarget` | `lighttarget_clips_base` | 5 |
| **`player0..3`** | **`body_clips`** | ⛔ **0** |

`body_clips` is the dir `BandCharacter::SetContext("venue")` binds
(`bandobj/BandCharacter.cpp:1849`). The clips that belong in it arrive through
the wardrobe's `FileMerger`, i.e. the path X12 documented as gated on the
`ObjPtrList` NULL-entry defect. The wider catalogue resident on `player0` does
contain real hand animation (`bass_pluck_norm_open_01`, `strum_01`,
`big_strum_02`) — it is simply not in the set the driver consults.

⚠ **Not attempted:** an opt-in "search the character's own resident sets"
fallback. It would have produced a band frame under animation, but it bypasses
the driver's own binding, and inventing a clip selection is the class of thing
this house does not do quietly. Named as the cheapest next step instead (§7).

### 1.4 ★★ The positive control, run *under animation*

`RB3_HANDPOSE_PERTURB=17` in the animated configuration
(`x13-C-anim-perturb17.log`):

| quantity | animated, clean | animated, perturbed | expected |
|---|---|---|---|
| `bone_L-hand` parent distance | 10.105 | **27.105** | exactly **+17.000** ✅ |
| `bone_L-hand` world | (−8.703, 2.549, 36.579) | (−0.114, 0.087, 22.117) | moved ✅ |
| `bone_R-hand` | unchanged | unchanged | negative control ✅ |

The instrument still detects the defect class, to the fourth significant figure,
at the animated instant — so the green in §1.2 is a green that *could* have been
red.

---

## 2. ★★★ Milestone 2 — the palette-vs-`meshWorld` discrepancy, decided

X12 measured it and correctly declined to call it a hand defect (it is constant
across the working and broken arms). It is nonetheless real, and it is a
**whole-band rendering defect**.

### 2.1 The palette's own translation, printed

X12 inferred "`skin.v` is ~0" from a max-elementwise deviation that happened to
equal the placement's y. A max over `|a−b|` cannot establish either operand, so
`skin[0].v` is now printed directly. player2, character world
(−10.026, 31.389, 13.218):

| mesh | `palette skin[0].v` | `meshWorld.v` |
|---|---|---|
| `hands_naked` (first bone `bone_R-pinky03.mesh`) | **(−0.000, −0.000, −0.000)** | (−10.026, 31.389, 13.218) |
| head mesh (first bone `bone_liptop_left.mesh`) | **(0.000, 0.000, −0.000)** | (−10.026, 31.389, 13.218) |
| the mic prop (first bone `bone_mic.mesh`) | **(−10.026, 41.389, 13.226)** | (−10.026, 31.389, 13.218) |

★ The third row is the **within-character positive control**: the placement
*does* reach some transforms on this figure. It does not reach the skeleton.

### 2.2 The chain, walked to its root

```
[ 0] bone_L-hand.mesh    world ( -21.749   0.197  44.274)
[ 1] bone_L-foreArm.mesh world ( -14.399  -1.494  51.000)
...
[ 7] bone_pelvis.mesh    world (  -0.001   1.627  37.576)
[ 8] (unnamed)           world (   0.000   0.000   0.000)   class Character
[ 9] (unnamed)           world (   0.000   0.000   0.000)   class Character
^ chain ROOT reached (no TransParent)
```

⇒ The skeleton terminates at an **unnamed `Character` at the origin**.
`player2`'s own placed transform is not on the chain. The mesh's `WorldXfm` does
carry the slot; the bones do not.

### 2.3 What the renderer does with that

Traced through the live RB3 path (`Rnd_Wgpu_RB3.cpp` wins the link at `:6133`,
not DC3's `Mesh_Wgpu.cpp`):

- palette entry = `Multiply(owner->BoneOffsetAt(b), bt->WorldXfm(), skin)` — `Rnd_Wgpu_RB3.cpp:3823`
- when `meshWorld` is non-identity, each entry is post-multiplied by `inverse(meshWorld)` — `:4077`
- `obj.world = meshWorld` — `:3347`
- shader: `worldPos = object.world * (Σ wᵢ · bones[i] * v)` — `standard_wgsl.inc:696-702`

so `worldPos = meshWorld · (skin · meshWorld⁻¹) · v = **skin · v**`. The code
says so itself at `:4074`. The legacy `RB3_PLACEMENT_CONTRACT_OFF` arm forces
`obj.world` to identity and gives the **same** result — so the contract is not
the culprit; the unplaced skeleton is.

### 2.4 ⛔ The prediction, and the set identity that confirms it

If the drawn position is `skin·v` and `skin` is character-local, four members at
four different slots must draw **on top of each other**. Measured
(`x13-E-chain.log`) — the drawn geometry, skinned through the shipped
`RndMesh::SkinVertex`:

| member | authored slot (character world) | drawn `hands_naked` centroid |
|---|---|---|
| player0 | (−70.003, 80.657, 13.495) | (−0.00, 0.66, 40.08) |
| player1 | (14.429, 146.133, 13.182) | (−0.00, 0.51, 40.48) |
| player2 | (−10.026, 31.389, 13.218) | (−0.00, 0.66, 40.08) |
| player3 | (68.770, 51.436, 13.248) | (−0.00, 0.66, 40.08) |

★★★ **A set identity, not a magnitude.** The authored placements span ~140 units
in x and ~115 in y; the drawn centroids are the same point to two decimals.
**The band renders stacked at the venue origin.** This is the mechanism behind
"only one band member is visible in the wide frame", carried unexplained by both
X11 and X12 — and reproduced in this lane's own `x13-E0-band-club.png` and
`x13-E3-below-elevn35.png`, where one figure stands amid all four members'
props (mic stand, bass, pedals, drum kit).

⚠ **Explicitly NOT claimed:** *where* the placement should have entered the bone
chain, or which shipped call was skipped. The measurement localises the defect
to "the skeleton's root Character is not the placed one"; naming the missing
statement is the next lane's job. X7's `SyncPlayMode`/`SetModeSink` note and
X12's `ObjPtrList` defect are the two candidates already on file.

---

## 3. ★★ `Rot.cpp:299` — fixed, with the X360 A/B

`Multiply(const Vector3&, const Hmx::Quat&, Vector3&)`'s `HX_NATIVE` arm stored
`vout.x` at `:306` and then read `vin.x/.y/.z` at `:307-308`. With
`&vout == &vin` — how `hamobj/HamSkeletonConverter.cpp:564` calls it — rows 2
and 3 consumed the already-rotated x. The `#else` X360 arm hoists to locals and
never had the bug; the native arm now does the same.

**Demonstrated both ways** (`AliasProbe` extended — X12 recommended this probe
and did not write it; reference and subject are the same function, so it cannot
pass by construction). Rotating (1,2,3) by 90° about z:

| build | `dest == vin` | dev |
|---|---|---|
| fix **reverted**, rebuilt (`x13-G-aliasprobe-UNFIXED.log`) | `[−2.000, **−2.000**, 3.000]` — y came back as the already-written x | **3.000e+00** ⛔ |
| fix applied, rebuilt (`x13-H-aliasprobe-REFIXED.log`) | `[−2.000, 1.000, 3.000]` | **0.000e+00** ✅ |

**X360 A/B — both objects built IN THIS WORKTREE:**

| section class | result |
|---|---|
| all **9378** `.text` sections | ✅ **BYTE-IDENTICAL** |
| 847 bytes | header / symtab / strtab (rebuild metadata) |
| 3 bytes | `.debug$S` |

⚠ **A first attempt was discarded**: it compared against an object built in the
*main* tree, and the differing absolute path embedded in the debug info produced
1.28M differing bytes and a 2502-byte size change. That comparison was
confounded, not evidence; it was thrown away rather than explained.

⛔ **`src/system/math/Rot.cpp` has NO unit in `objdiff.json` — this TU CANNOT BE
SCORED by objdiff at all.** Stated rather than implying a match percentage, per
the charter.

★ This lifts X12's hard ordering constraint: adding `hamobj` to a native target
is no longer gated on a dormant wrong-result defect in skeleton conversion.

⚠ **Not touched, deliberately:** `Transform::LookAt` (`math/mtx.cpp:183`) is
alias-unsafe with a **live** call site on both platforms
(`world/CameraShot.cpp:306`), but it is identical on X360, so it is a faithful
decomp of shipped behaviour; changing it is a behaviour change needing its own
A/B. `Invert(const Hmx::Matrix4&, Hmx::Matrix4&)` (`mtx.cpp:213`) is unsafe and
has no caller anywhere.

---

## 4. ⛔ Retracted hypotheses, with their evidence

1. ⛔ **Mine, mid-lane, and it was nearly the headline: "the recompose identity
   FAILS under animation — worst dev 3.473e+00 at `bone_pelvis.mesh`."**
   Retracted. `DriveSceneCharacters` stepped the clock and then polled every
   character that merely *has* a `CharDriver`, while its own comment said "every
   DRIVEN character" — different sets. Characters skipped for lack of a clip
   were never `Enter()`ed or `Play()`ed and were polled anyway. Same clip, same
   beat, same scene:

   | binary | result |
   |---|---|
   | old poll loop | worst dev **3.473e+00** over 2656 bones |
   | fixed poll loop | worst dev **0.000e+00** over 2636 admissible **+ 20 excluded = 2656** |

   The counts reconcile exactly, so the split dropped nothing. ★ The reading was
   the instrument, not the engine — caught before publication, by an A/B rather
   than by argument.

2. ⛔ **The same bug produced a crash, and the crash misdirected too.** With
   `RB3_BAND_PLACE=1` the run was `rc=139` (SIGSEGV); without it, `rc=0`. That
   reads as "the band breaks animation". It was the four band members being
   polled through an un-`Enter()`ed driver over an empty clip dir. Fixed loop:
   `rc=0` with the band placed.

3. ⛔ **X12's framing of the palette invariant as `mOffset_b · boneWorld_b ==
   meshWorld`.** Partially retracted: that is the correct *bind-pose algebraic*
   identity, but it is **not** the contract the renderer relies on. The renderer
   cancels `meshWorld` out (`Rnd_Wgpu_RB3.cpp:4077`) and draws at `skin·v`, so
   the palette is required to be world-space *on its own*. Under that framing
   the deviation is not a curiosity — it is the defect. §2.3

4. ⚠ **The charter's benign reading of the ceiling legs — not refuted, but not
   confirmed by the test that was supposed to confirm it.** §5

5. ⚠ **Explicitly NOT claimed:** that the band's hands are correct *under
   animation*. No clip can reach them today. §1.3

---

## 5. ⚠ The ceiling legs — benign, at medium confidence

**The charter's reading** (upper level, camera below, mezzanine floor not drawn
from underneath by back-face culling) is **not the mechanism**, but the
conclusion is probably right.

- Two purpose-rendered frames (`x13-E2-above-elev55.png`, elevation +55;
  `x13-E3-below-elevn35.png`, elevation −35) did **not** cleanly settle it: in
  both, some legs are cut by the frame edge rather than by geometry, which is
  the confound the test was meant to remove.
- ★ The load-bearing evidence is the **wide** frame `x13-E0-band-club.png`,
  where the *same* mezzanine crowd population is drawn with **full bodies** —
  torsos, arms and heads along the railing. So the geometry exists and is drawn;
  the legs-only reading is **view-dependent occlusion**, not missing geometry.
- The occluder visible in the wide frame is the mezzanine's **fascia beam** at
  the balcony edge, not a floor: from stage level it covers the crowd's
  mid-bodies while the open railing gap below it leaves the legs visible.

⇒ **Benign — a defect is NOT named.** ⚠ Confidence stated honestly as medium:
the two dedicated frames were inconclusive and the verdict rests on a
comparative read of an existing artifact. A frame from *outside* the club
looking in, with nothing cut by the frame edge, would settle it outright and was
not rendered.

---

## 6. ⚠ What did NOT land, stated plainly

1. ⛔ **Direction-B rows 4 and 3b were NOT guarded.** X12 identified them as the
   clean ones and this lane agrees with that analysis, but did not spend the
   build cycles to guard, link and enumerate the undefined set. **Rows 2 and 3a
   remain open and untouched**, as instructed — `ui/PanelDir.cpp:439-455` is
   port-added `#ifdef HX_NATIVE` code driving `Flow`, and `GemManager.cpp:1649`
   carries `TheFlowMgr`. Row 7 remains a mis-classification recommended for
   deletion. **No working build was broken to tidy a latent leak.**
2. ⛔ **Band hand pose under animation — UNDECIDABLE**, and why. §1.3
3. ⛔ **The cause of the unplaced skeleton is not named**, only its address. §2.4
4. ⚠ **`Transform::LookAt` untouched**; `Invert(Matrix4)` untouched. §3
5. ⚠ **`RB3_HANDS_MITTEN` is default-ON in the engine** and lerps finger palette
   entries toward the wrist-rigid transform (`Rnd_Wgpu_RB3.cpp:3740+`). ⛔ **This
   means the fingers in X12's cited close-up are partly synthesized, not purely
   the asset's pose.** Disclosed, not investigated — it is engine-repo code and
   this lane made zero engine edits. §7
6. ⚠ **`OutfitConfig` (48 symbols), `CharMeshHide::HideAll`, 42 orphan files,
   `_MILO_SCATTER_TRANSITIVE_PRUNE`, `BandCamShot`, `band.play_mode`,
   `RB3_BAND_PLACE` opt-in** — all carried from X10/X11/X12, untouched.
7. ⚠ **The foreign uncommitted engine edit `src/platform/FxSendNative.cpp` is
   still there, still not mine, left untouched — tenth lane running.**

---

## 7. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Hand pose under animation — CROWD** | ★★★ **VERIFIED CORRECT** | §1.2 |
| **Hand pose under animation — BAND** | ⛔ **UNDECIDABLE — no clip can reach them** | §1.3 |
| **The skeleton actually moved (anti-vacuity)** | ✅ **YES — hand travelled 15.4u, 112-line diff** | §1.2 |
| **Segment lengths under animation** | ✅ **PRESERVED EXACTLY** (supplementary only) | §1.2 |
| **Recompose identity, animated, admissible bones** | ✅ **0.000e+00 over 7380** | §1.2 |
| **Instrument positive control, animated** | ✅ **VALIDATED — +17.000 measured** | §1.4 |
| **X12's recompose oracle as an animated oracle** | ⛔ **INVALID UNTIL SPLIT — four compose rules + SetWorldXfm** | §1.1 |
| **X12's `handpose-reached-a-skeleton` gate** | ⛔ **PARTIALLY VACUOUS — passes over 0 hand meshes; now gated** | §1.1 |
| **Palette vs `meshWorld`** | ★★★ **EXPLAINED — skeleton parented to an UNPLACED Character** | §2.2 |
| **Is it benign?** | ⛔ **NO — the band draws stacked at the venue origin** | §2.4 |
| **`body_clips` population** | ⛔ **DEFECT — 0 clips vs 40–44 for every crowd member** | §1.3 |
| **`DriveSceneCharacters` poll loop** | ⛔ **DEFECT, FIXED — polled 21, drove 8** | §4.1 |
| **`Rot.cpp:299` alias safety** | ✅ **FIXED, demonstrated both ways** | §3 |
| **`Rot.cpp` X360 blast radius** | ✅ **ZERO — 9378/9378 `.text` sections identical** | §3 |
| **`Rot.cpp` objdiff position** | ⛔ **NOT SCOREABLE — no unit in `objdiff.json`** | §3 |
| **`Transform::LookAt`, `Invert(Matrix4)`** | ⚠ **UNSAFE, untouched, reasons given** | §3 |
| **Direction-B rows 4 / 3b** | ⛔ **NOT CLOSED — not attempted** | §6.1 |
| **Direction-B rows 2 / 3a** | ✅ **LEFT OPEN as instructed** | §6.1 |
| **Ceiling legs** | ⚠ **BENIGN at medium confidence — mechanism is the fascia beam, not floor culling** | §5 |
| **`RB3_HANDS_MITTEN` synthesizing finger pose** | ⚠ **DISCLOSED, uninvestigated** | §6.5 |
| **Prior lanes' frames** | ✅ **NON-REGRESSED — byte-identical to X11 & X12 artifacts** | §8 |

---

## 8. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18** both at baseline and pre-land | `x13-native-gate-baseline.log`, `x13-native-gate-land.log`; cache seeded with **all four** flags |
| b | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606…` unmoved | foreign `FxSendNative.cpp` edit disclosed, left alone — **tenth lane** |
| c | Shared-`src/` X360-faithful at symbol granularity | ✅ **PASS** — 9378/9378 `.text` sections byte-identical | §3. ⛔ TU **not scoreable** by objdiff (no unit) — stated, not implied |
| d | PNG determinism ×2 | ✅ **PASS** — `cmp`-identical | `x13-E0-band-club.png` rendered twice |
| e | Prior evidence non-regressed, vs **artifacts** | ✅ **PASS** | byte-identical to `x12-E0-band-club.png` **and** `x11-E1-band-club-final.png`; **differs** from `x11-E0-band-club-baseline.png` — checked against all three, which is what proves X11's fix is present rather than silently reverted |
| f | Was `main` broken by a decomp lane? | ✅ **NO** | baseline gate PASS 18/18 at `785b7ef6` before any edit |

⚠ **The `--revert` trap was hit twice and both times caught by `git status`**:
`Rot.cpp` was reverted and re-applied for the negative-arm probe and again for
the X360 A/B. Each restore was confirmed by `git status --short` *and* by `cmp`
against the saved patched file before any measurement was taken.

⚠ **Read the gate's own verdict line, not the pipeline's exit code** — carried
from X12; a trailing `grep -c SKIP` exits 1 when it finds zero matches, i.e. the
failure code *is* the 0-SKIPs result.

---

## 9. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Get the placement onto the bone chain** | §2: the skeleton's root is an unnamed `Character` at the origin, so all four members draw stacked. This is the single biggest visible defect in the port right now, and the instrument to verify a fix (`--hand-audit`, chain walk, drawn-centroid set identity) exists. | X14 |
| ★★★ **Populate `body_clips`** | §1.3: until the wardrobe `FileMerger` path completes, no band animation is reachable and the band half of Milestone 1 stays undecidable. Tied to X12's `ObjPtrList` NULL-entry defect. | X14 |
| ★★ **`RB3_HANDS_MITTEN` is default-ON and synthesizes finger pose** | §6.5: engine-side, so it needs an engine change request rather than a workaround. Any "the fingers look right" claim — including X12's close-up — is contaminated until this is characterised ON vs OFF. | engine CR |
| ★ **A conclusive ceiling-legs frame** | §5: from outside the club looking in, nothing cut by the frame edge. | X14, cheap |
| ★★ **Direction-B rows 4 and 3b** | §6.1: still the clean ones; still not done. | build-system |
| ⚠ **`Transform::LookAt`** | §3: unsafe, live, faithful to X360 — needs its own A/B and a matching-vs-correctness decision. | build-system |
| ⚠ **Direction-B rows 2/3a stay open; row 7 delete; rows 1+6 are a pair** | Unchanged from X12 — except row 1's blocker is now **cleared** (§3). | build-system |
| ⚠ **`OutfitConfig`, `CharMeshHide::HideAll`, orphans, foreign `FxSendNative.cpp`** | All carried, untouched. | as before |

---

## 10. Recommended X14 shape

1. ★★★ **Check the denominator of every green — including a green you just
   made yourself.** The new `handpose-measured-hand-geometry` gate exists
   because X12's oracle passed on a skeleton while measuring zero hand meshes.
   The same class of bug appeared three times in three lanes.
2. ★★★ **When an instrument reports a failure, A/B the instrument before the
   engine.** The 3.473e+00 "animation breaks the hierarchy" reading survived
   about twenty minutes and would have been a wrong headline. What killed it was
   rebuilding with the poll loop fixed and re-running the *same* command.
3. ★★ **A predecessor's carefully-hedged open finding is often the real
   defect.** X12 measured the palette/`meshWorld` gap precisely, applied X10's
   rule correctly, and handed it on as "not a hand defect" — which was right,
   and it was also the biggest defect on the board.
4. ★★ **Trace the consumer, not just the producer.** The palette numbers were
   already on disk in X12's logs. What decided them was reading what the shader
   does with them — and finding that `meshWorld` is deliberately cancelled, so
   the palette must be world-space on its own.
5. ★ **Prefer a set identity to a magnitude.** "Four members at four slots, one
   drawn centroid" is worth more than any single deviation figure, and it is
   what turned a prediction into a verdict.
