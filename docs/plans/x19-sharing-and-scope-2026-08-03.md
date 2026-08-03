# X19 — the sharing is BOTH real and a lookup artifact; the retirement blocker was a coupling defect in the driver, not a shared-`src/` default

**Date:** 2026-08-03
**Predecessor:** [X18](x18-gate-and-roots-2026-08-03.md) "the gate was over-reporting; the retirement blocker is the rebind scope"
**Branch:** `x19-sharing-and-scope`, from `main` @ `c1cb4ba3`
**Engine:** `milo-native-engine` pinned at **`138e1606`**, **zero engine edits**
**Change surface:** **one native-only file** — `native/src/main_render.cpp`. **No shared `src/` edit at all**, which is itself a finding (§3), not a scoping accident.

---

## Verdict

★★★ **THE SHARING QUESTION IS BOTH, AND THE TWO CLAIMS RESOLVE DIFFERENTLY.**
X18 asked whether the band's shared `bone_L-hand` pointer is genuine object
sharing or a `FindBoneNamed` artifact. It is **genuine sharing** *and* **a
lookup artifact**, which is why counting could not settle it and enumerating
could. Every player figure carries **two** bones of that name: the shared
unplaced one (chain root unnamed, identical across all eight band entries) and
its **own placed one** (chain root `player0`…`player3`, four distinct pointers).
`FindBoneNamed` returns the first, which is the shared one. §1

★★★ **THE CONSEQUENCE X18 COULD NOT SEE: THE ONLY GEOMETRIC ORACLE ON THIS
LADDER WAS MEASURING THE WRONG BONE.** The hand-mesh gap gate — the seam X18
handed forward for validating the published pose — keys off `lh`/`rh`. Five
lanes of band hand gaps (32–128u) were read off a bone belonging to no member.
Against each member's **own** bone the gap is **0.000 (INSIDE)** on all four
members' `hands_naked.mesh`. §2

★★★ **THE PRECISE BLAST RADIUS: 120 OF 123 DEVIATING BONES ARE FOREIGN-ROOTED,
AND THE MEMBERS' OWN SKELETONS ARE CLEAN.** 7380 admissible slots split
2607 own-rooted / 4773 foreign-rooted; the 123 deviating split **3 own / 120
foreign**. The residual four lanes have chased lives almost entirely on the one
shared unplaced skeleton. §4

★★★ **THE RETIREMENT BLOCKER WAS A COUPLING DEFECT IN THE DRIVER, NOT A
SHARED-`src/` DEFAULT.** The `quiescent → FULL` `setenv` sat **inside** the
`!pollOnly` block alongside X14's direct call, so the `only` arm skipped the
**scope decision** as a side effect of skipping the call block. Three lanes read
the resulting collapse as evidence about `BandCharacter.cpp`'s default. §3

★★★ **X14's DRIVER-SIDE CALL IS RETIRED.** After hoisting the scope decision,
the poll arm with the call removed is **byte-identical** to the arm that keeps
it, and to X18's own poll artifact. §3.2

⛔ **THE SHARED-`src/` DEFAULT WAS DELIBERATELY *NOT* FLIPPED, AND THAT IS THE
EVIDENCE-BASED ANSWER, NOT A FIFTH DECLINE.** Flipping it is unnecessary (§3)
and unjustified (§3.3): X14 §5 refuted the shard mechanism explicitly *"in the
un-animated case"* only. §3.3

★★★ **TEXTURES REACHED — FIRST PRECISE BLOCKER IN SEVEN LANES.** The band's
drawn skin materials still carry the **authored `dummy_torso/legs/feet.tex`
placeholders** (plus NULL on `head_naked.mat`) because
`OutfitConfig::SetSkinTextures` never runs. A fourth state nobody enumerated:
not null, not unuploaded, not shader tint. Three inherited explanations —
missing assets, an upload gap, and X14's "419 texture-resolution failures" — are
**refuted by measurement**. §5

⚠ **THE POSE IS NOW PARTLY AFFIRMED, NOT MERELY UN-INVALIDATED** — but by a
geometric oracle with named limits. §2.2

⛔ **MINE, MID-LANE: retiring the call silently deleted the scope diagnostic.**
§7

✅ **Gate PASS 18/18 fresh, rc=0, 0 SKIPs.** `main` was **not** broken by a
decomp lane. §8

---

## 1. ★★★ Milestone 1 — the sharing, settled by enumerating rather than counting

X18 committed an instrument that prints the **count** of bones carrying the
name per figure, and did not run it to conclusion. A count cannot settle this:
"2 bones of this name" is consistent with both hypotheses. What decides it is
**which object the lookup skipped**, so the probe enumerates every candidate
with its chain root and world.

★ **The prediction was named before the run, from X14 §1.2** (each member owns
a placed skeleton of 492 same-named bones), so the second hypothesis was
testable rather than fitted afterwards:

| hypothesis | prediction for the extra candidates |
|---|---|
| pure object sharing | **same pointer** — one object reached by two paths |
| lookup artifact | **different pointer**, chain root = the member itself |

### 1.1 The result (`x19-m1-candidates.log`)

```
--- player0 ---
  cand[0] 0x…f3b0  world (-21.749, 0.197, 44.274)  chain root '(unnamed)'  <== RETURNED
  cand[1] 0x…a330  world (-54.463, 72.879, 57.667) chain root 'player0'        (passed over)
--- player1 ---
  cand[0] 0x…f3b0  world (-21.749, 0.197, 44.274)  chain root '(unnamed)'  <== RETURNED
  cand[1] 0x…4d50  world ( 30.213, 133.490, 64.770) chain root 'player1'       (passed over)
```

| figure class | bones of that name | `cand[0]` | `cand[1]` |
|---|---|---|---|
| `player0`…`player3` | **2** | **one shared pointer**, unnamed root, `(-21.749, 0.197, 44.274)` | **four distinct** pointers, own root, four placed worlds |
| four `outfit` sub-Characters | **1** | the same shared pointer | — (own no skeleton) |
| eight crowd figures | **1** | own pointer, chain root == itself | — |

⇒ **Both claims are true.** The shared skeleton is one real object reached by
eight collections — corroborating X14 §1.1 by an independent route — **and** the
lookup passes over a correctly-placed own bone that exists.

---

## 2. ★★★ What that broke: the geometric oracle

`ReportHandPose` measures the decisive geometric quantity — how far each hand
bone is from the geometry hanging off it — as
`DistToExtentBox(e, cand[k]->WorldXfm().v)` with `cand[2] = { lh, rh }`
(`native/src/main_render.cpp`). Both are `FindBoneNamed` results, i.e. the
shared unplaced bone for all eight band entries.

★ **This is X18's proposed seam.** Its handoff named "the hand-mesh gap gates"
as the existing seam for a geometric oracle over the published pose. That seam
was measuring a bone that belongs to no member.

`FindBoneNamedOwn` applies a **structural** predicate — walk to the chain root,
require it to be the Character being measured — and the corrected gap is
printed **alongside** the legacy number rather than replacing it, so five lanes
of prior logs stay comparable.

### 2.1 The corrected numbers (`x19-m1-ownbone2.log`)

| member | mesh | legacy (shared bone) L / R | **own bone L / R** |
|---|---|---|---|
| player0 | `hands_naked.mesh` | 73.362 / 99.706 | **0.000 / 0.000 (INSIDE)** |
| player1 | `hands_naked.mesh` | 127.834 / 127.041 | **0.000 / 0.000 (INSIDE)** |
| player2 | `hands_naked.mesh` | 32.521 / 37.134 | **0.000 / 0.000 (INSIDE)** |
| player3 | `hands_naked.mesh` | 90.015 / 53.295 | **0.000 / 0.000 (INSIDE)** |
| player2 | `malewrist_barbedwire_right.mesh` | 42.662 / 59.776 | 15.608 / 10.631 |
| player3 | `malewrist_hercules_right.mesh` | 92.208 / 56.089 | 20.274 / 0.503 |

**The four residuals are not defects.** They are *wrist accessory* meshes, and
`isHand` matches them via `strstr(nm, "wrist")` while probing **both** hand
bones — so a **right** wristband is expected to sit ~15–20u from the **left**
hand bone. The two meshes are bound to `bone_R-foreTwist1.mesh`, not to a hand.

### 2.2 ⚠ What this does and does NOT establish

- ✅ It **does** say each member's own hand bone lies **inside** that member's
  own drawn hand geometry, in the poll arm, through the shipped
  `RndMesh::SkinVertex`.
- ⛔ It says **nothing** about finger curl, wrist angle, or agreement with any
  authored reference pose. A hand closed when it should be open passes it.
- ⛔ It is a **band** statement. Crowd figures carry no hand mesh at all.

★ So X18's "un-invalidated, not validated" is upgraded only this far: an
absolute geometric oracle now **affirms bone-inside-geometry** for the band.
That is strictly more than the algebraic gate could say, and strictly less than
"the pose is correct."

### 2.3 ⚠ A control I claimed and then could not exercise

I intended the crowd as the built-in negative control: their chain root **is**
themselves, so `FindBoneNamedOwn` must be a provable no-op there. **It is not
exercised by the gap gate** — crowd figures carry no hand mesh, so no crowd gap
line exists. Rather than fold that into a single `n/a`, the output distinguishes
`SAME (own==legacy; the control)` from `NO-OWN (figure owns no skeleton)`, and
this doc states the control as **unavailable** rather than claiming it.

---

## 3. ★★★ Milestone 2 — the retirement blocker was never a shared-`src/` default

### 3.1 The coupling defect

`native/src/main_render.cpp` already forced FULL scope for a quiescent scene:

```c
bool pollOnly = pollMode && strcmp(pollMode, "only") == 0;
if (TheBandWardrobe && !getenv("RB3_NO_BAND_REBIND") && !pollOnly) {   // <-- the call block
    ...
    bool quiescent = (gSceneClip == nullptr && gClipsFile == nullptr);
    if (quiescent && !getenv("RB3_SKEL_REBIND_FULL") && !getenv("RB3_NO_BAND_REBIND_FULL"))
        setenv("RB3_SKEL_REBIND_FULL", "1", 0);                        // <-- INSIDE it
    ...
    bc->RebindOutfitBonesToOwnSkeleton();
}
```

⛔ **The `only` arm never skipped "the rebind". It skipped the SCOPE DECISION**,
as a side effect of skipping the call block, and `Poll()`'s own rebind then ran
at the torso whitelist. X18's collapse — and the conclusion that
`BandCharacter.cpp:849`'s default had to be flipped — follows entirely from
this coupling.

**Fix:** hoist the decision out of the call block, so the scope is a property of
the **scene** (is anything being animated?) rather than of which code path
happens to invoke the rebind — which is what it was always documented to mean.

### 3.2 ★★★ Retiring the call

The guard on the direct call became `!pollMode` (was `!pollOnly`).

⚠ **Under `!pollOnly`, the `RB3_BAND_POLL=1` arm ran BOTH** the direct call and
`Poll()`'s rebind. The arm meant to demonstrate `Poll()` doing the work was
being carried by the direct call, and **could not have detected `Poll()` failing
to rebind at all.** The no-poll arm keeps the direct call deliberately: `Poll()`
never runs there and nothing else performs the rebind.

| comparison | result |
|---|---|
| poll (call **retired**) vs `only` | **BYTE-IDENTICAL** |
| default arm vs X18's `x18-A-base-club.png` | **BYTE-IDENTICAL** |
| poll arm vs X18's `x18-B-poll-club.png` | **BYTE-IDENTICAL** |
| determinism ×2, all three arms | **IDENTICAL** |

Head / eyebrows / fingernails in the `only` arm: `(-70.08, 77.50, 80.49)` /
`(-70.13, 75.97, 81.59)` / `(-65.41, 68.67, 57.23)` — **on player0**, not the
origin. **PNG opened** (§8.1).

⇒ **X14's driver-side call is RETIRED** in every arm where `Poll()` runs.

### 3.3 ⛔ Why the shared-`src/` default was NOT flipped

The charter's milestone 2 was to flip `BandCharacter.cpp`'s torso default with
an X360 A/B. **The correct answer is that it should not be flipped**, on
evidence rather than caution:

1. **It is unnecessary.** The driver already selects FULL for a quiescent
   scene; §3.1 was the only thing preventing the `only` arm from seeing it.
2. **It is unjustified.** X14 §5 refuted the rotation-basis shard mechanism
   explicitly **"in the un-animated case"**. No lane has measured FULL scope
   under a driven clip. Flipping the default unconditionally would assert
   something nobody has tested.
3. **There is no "retail behaviour" to justify it against.** Three lanes called
   the torso scope "the shipped default" and "a deliberate departure from
   shipped behaviour". **`RebindOutfitBonesToOwnSkeleton()` is entirely inside
   `#ifdef HX_NATIVE`** (`BandCharacter.cpp:719-1052`). It does not exist in the
   X360 build. There is no retail default here to depart from — the torso scope
   is a wave-08 native workaround, whose stated justification (the rotation
   basis) X14 later refuted.

★ The scope control matrix, three distinct outcomes from three inputs, so the
diagnostic is not a constant:

| arm | reported scope |
|---|---|
| quiescent poll arm | **FULL figure** |
| `--clips` given (`quiescent` false) | **torso only** — animated path preserved |
| `RB3_NO_BAND_REBIND_FULL=1` | **torso only** — opt-out works |

---

## 4. ★★★ The blast radius, per figure

X18 gave one global ratio and a general warning. That is too coarse to act on:
it does not say **which** numbers survive. Splitting each figure's admissible
population by chain-root ownership (`x19-rootedness.log`):

| figure | own-rooted (deviating) | foreign-rooted (deviating) | total |
|---|---|---|---|
| crowd ×8 | 50 (0) | 1 (0) | 51 |
| player0 | 558 (0) | 593 (**18**) | 1151 |
| outfit0 | **0** (0) | 608 (**18**) | 608 |
| player1 | 561 (**1**) | 606 (**28**) | 1167 |
| outfit1 | **0** (0) | 619 (**28**) | 619 |
| player2 | 521 (**2**) | 576 (**4**) | 1097 |
| outfit2 | **0** (0) | 592 (**4**) | 592 |
| player3 | 567 (0) | 578 (**10**) | 1145 |
| outfit3 | **0** (0) | 593 (**10**) | 593 |
| **total** | **2607 (3)** | **4773 (120)** | **7380 (123)** |

★★★ **The members' own placed skeletons are essentially clean — 3 deviating
bones out of 2207 own-rooted band bones.** The entire residual (120 of 123)
lives on the **one shared, unplaced** `char/main/skeleton.milo` instance,
re-counted once per band collection. Read with X18 §2, this says
`CharHair::SimulateZeroTime()` and the two IK solvers are publishing onto the
**shared** skeleton, not onto the members'.

★ **The outfit rows are exact duplicates.** Every outfit has **zero** own-rooted
bones and its deviating count equals its paired player's foreign count exactly
(18/28/4/10). X17's eight band rows are four own populations plus the same
shared skeleton counted eight times.

### 4.1 Which prior conclusions stand, and which do not

| conclusion | verdict |
|---|---|
| X14 §2 — band drawn geometry on four distinct marks | ✅ **STANDS.** Measured over drawn vertices via the shipped `SkinVertex`; it never calls `FindBoneNamed`. Independently reproduced here in the poll arm: `hands_naked` centroids `(-65.56, 68.61, 56.89)`, `(13.08, 129.86, 66.38)`, `(-10.06, 32.38, 60.95)`, `(74.89, 39.63, 56.77)` — four distinct placed positions |
| X14 §1.1/§1.2 — one shared skeleton + four own placed roots | ✅ **STANDS, and is now the explanation** for the lookup artifact |
| X18 §1 — writer classification (4351 COMPOSED/0, 236 PUBLISHED/123, 2793 LOADED/0) | ✅ **STANDS.** Iterates all of `bones[]`; independent of `FindBoneNamed`. Reproduced exactly |
| X18 §2 — publishers named by return address | ✅ **STANDS**, and §4 sharpens it: they publish onto the shared skeleton |
| X17 §1 — per-figure deviating counts 18/29/6/10 over 1151/1167/1097/1145 | ✅ **REPRODUCE EXACTLY**, and are now **explained**: slot counts whose foreign half is one shared object. The counts are correct; the *reading* "four independent figures" was not |
| X17 §4 — ROOT vs inherited decomposition | ✅ **STANDS** structurally; the ROOTs sit on the shared skeleton |
| ⛔ **Per-figure HAND numbers** (`bone_L-hand`/`bone_R-hand` worlds, the bone-to-geometry gaps) — X12, X13, X15 §5.2, X17 §6 | ⛔ **DO NOT STAND AS PER-FIGURE.** All eight band entries read **one** shared unplaced object. X15's `1.09e+01` and X17's `1.097e+01`/`1.102e+01` are measurements of the same bone, not of two figures |
| X18 §5 C2's 8× compounding | ✅ **FULLY EXPLAINED** by §1 |

---

## 5. ★★★ Milestone 3 — textures: REACHED, and the blocker is precise

**Six lanes reported "the band renders untextured pink" as an aggregate.** The
existing census prints `341 of 411 textured`, which **cannot** distinguish a
fully-textured band from a fully-untextured one, because the venue dominates the
count. So this lane censused exactly the five material names
`OutfitConfig::SetSkinTextures` binds (its own table,
`bandobj/OutfitConfig.cpp:475-479`) — the mesh selection is not a guess.

### 5.1 The result (`x19-texture-census.log`)

```
=== X19 SKIN-MATERIAL DIFFUSE CENSUS: 52 skin material instance(s),
    48 with a diffuse, 4 NULL ===
```

| mesh | material | diffuse |
|---|---|---|
| `head.mesh` ×4 | `head_naked.mat` | **NULL** |
| `hands_naked.mesh` | `torso_naked.mat` | **`dummy_torso.tex`** |
| `*pants_skin.2.mesh` | `legs_skin.mat` | **`dummy_legs.tex`** |
| `saddleshoe_skin.2.mesh` | `feet_socks_skin.mat` | **`dummy_feet.tex`** |
| 34 face-morph wrinkle/norm meshes | `head_naked.mat` | `male_head_diff.tex` (deform targets, not drawn skin) |

★★★ **A FOURTH STATE NOBODY ENUMERATED.** The three hypotheses on file were
null-diffuse, not-uploaded, and shader-tint. The band's drawn skin is **none of
them**: it is bound to the **shipped `dummy_*` placeholder textures** that
`SetSkinTextures` (`OutfitConfig.cpp:473-575`) exists to **replace** with the
real per-character skin textures.

### 5.2 The causal chain — every link measured in this lane's own logs

```
OutfitConfig not registered natively (milo_object_factories.cpp omits it)
  -> "Can't make OutfitConfig"                                  [MEASURED: 40]
  -> no OutfitConfig instance exists
  -> BandCharacter::SyncOutfitConfig(OutfitConfig*) never runs
     (BandCharacter.cpp:1630; its SetSkinTextures call is at :1663)
  -> OutfitConfig::SetSkinTextures never runs
  -> skin materials keep their authored dummy_* placeholders
```

★ **Negative proof that it never ran:** `SetSkinTextures` emits
`MILO_WARN("… could not find …")` on six paths. `grep -c "could not find"` in
this lane's poll log = **0**.

⚠ **A correction to X18's framing.** X18 said the blocker is `Init()`'s three
static `New<>` calls. Those three types (`RndMat`, `RndCam`, `BandCharDesc`) are
all already constructible in the native link; referencing `Init()` fails because
it makes GC'd sections live and exposes the unresolved `BandIKEffector` symbols.
Also, `SetSkinTextures` is **not** reachable only from inside `OutfitConfig` —
`BandCharacter.cpp:1663` calls it — but that call sits inside a method taking an
`OutfitConfig *`, so the registration blocker holds.

### 5.3 ⛔ Three inherited explanations refuted by measurement

| inherited claim | measured |
|---|---|
| "missing assets" | ⛔ **REFUTED** — real skin/outfit textures load from the ark |
| "upload / format gap" | ⛔ **REFUTED** — `failed to create GPU texture` count = **0** |
| X14's "419 texture-resolution failures" | ⛔ **REFUTED** — the count is **417**, and **zero** are texture failures: **367** are `does not have name set` **cache-key notices emitted after a bitmap deserialized successfully**, and **50** are `bone_*.ikf` lookups downstream of the 48 `Can't make BandIKEffector` |

⚠ **The 191/1511/48 bill: I did not re-derive it and I do not repeat it.** The
`48` matches the measured `Can't make BandIKEffector` count in my own log; the
`191` and `1511` I cannot vouch for. **Nobody should quote it as measured.**

### 5.4 ⚠ What is NOT claimed

I did **not** sample `dummy_torso.tex`'s texels. Whether the specific pink RGB
comes from the placeholder's own content or from the skin shader's tint is one
further step. **The blocker is located; the last mile of the colour mechanism is
not.**

---

## 6. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Is the band sharing genuine or a lookup artifact?** | ★★★ **BOTH — and they resolve differently. Players carry TWO same-named bones; the lookup returns the shared unplaced one and passes over the member's own placed one** | §1 |
| **Blast radius on prior per-figure numbers** | ★★★ **PRECISE — 123 deviating = 3 own-rooted + 120 foreign-rooted; own skeletons essentially clean** | §4 |
| **X14's four-distinct-centroid placement** | ✅ **STANDS — never used `FindBoneNamed`; independently reproduced in the poll arm** | §4.1 |
| **X17's per-figure recompose table** | ✅ **COUNTS REPRODUCE EXACTLY; the "independent figures" reading does not** | §4.1 |
| **Per-figure HAND / gap numbers (X12–X17)** | ⛔ **NOT INDEPENDENT — one object read eight times** | §4.1 |
| **The geometric oracle** | ★★★ **WAS MEASURING THE WRONG BONE; corrected — 0.000 (INSIDE) on all four members' `hands_naked`** | §2 |
| **Is the polled POSE correct?** | ⚠ **PARTLY AFFIRMED — bone-inside-geometry now holds absolutely for the band; says nothing about finger curl or an authored reference** | §2.2 |
| **The real retirement blocker** | ★★★ **A COUPLING DEFECT IN THE DRIVER — the scope `setenv` sat inside the call block** | §3.1 |
| **X14's driver-side call retired?** | ★★★ **YES — poll-with-call-removed is byte-identical to keeping it, and to X18's poll artifact** | §3.2 |
| **Shared-`src/` scope default flipped?** | ⛔ **NO, AND DELIBERATELY SO — unnecessary (§3.1) and unjustified (X14 §5 refuted the shard mechanism only in the un-animated case)** | §3.3 |
| **"The shipped torso default"** | ⛔ **THERE IS NO SHIPPED DEFAULT — the whole function is inside `#ifdef HX_NATIVE` and does not exist in the X360 build** | §3.3 |
| **Animated path** | ✅ **UNCHANGED — `--clips` still selects torso scope; opt-out `RB3_NO_BAND_REBIND_FULL=1` works** | §3.3 |
| **Crowd negative control for the gap gate** | ⚠ **UNAVAILABLE — crowd figures carry no hand mesh. Stated, not claimed** | §2.3 |
| **Textures — why the band is pink** | ★★★ **REACHED AND PRECISE — the drawn skin materials still carry the authored `dummy_torso/legs/feet.tex` placeholders, plus NULL on `head_naked.mat`, because `OutfitConfig::SetSkinTextures` never runs** | §5 |
| **"Missing assets" / "upload gap"** | ⛔ **BOTH REFUTED BY MEASUREMENT** | §5.3 |
| **X14's "419 texture-resolution failures"** | ⛔ **REFUTED — 417, and zero of them are texture failures** | §5.3 |
| **X15's 191/1511/48 bill** | ⚠ **STILL NOT RE-DERIVED — only the `48` is corroborated. Do not quote it** | §5.3 |
| **The exact pink colour mechanism** | ⚠ **UNDETERMINED — placeholder texel vs shader tint not separated** | §5.4 |
| **X360 blast radius** | ✅ **ZERO BY CONSTRUCTION — no shared `src/` file touched** | §8 |
| **Frames** | ✅ **NO REGRESSION — default and poll arms byte-identical to X18's artifacts; opened** | §8.1 |

---

## 7. ⛔ Retracted / corrected

⛔ **X18's "the retirement blocker is the rebind scope [and `Poll()` must be made
to rebind at FULL scope]"** — the *symptom* was right, the *locus* was wrong.
The blocker was a coupling defect in `main_render.cpp`, not a shared-`src/`
default. No shared-`src/` change was needed. §3.1

⛔ **X14 / X17 / X18's "the shipped torso scope" and "a deliberate departure from
shipped behaviour"** — there is no shipped behaviour to depart from.
`RebindOutfitBonesToOwnSkeleton()` is entirely inside `#ifdef HX_NATIVE`. §3.3

⛔ **X18 §3's framing that the sharing is *either* genuine *or* a lookup
artifact** — it is both, and the disjunction is what made it look undecidable.
§1

⛔ **Mine, mid-lane: retiring the direct call silently deleted the scope
diagnostic.** The `band: rebind scope = …` printf lived *inside* the call block,
so retiring the call for the poll arms removed the readout of **the one variable
this milestone turns on**, in exactly the arms that matter. Caught by running
the animated control and finding nothing to read. Moved to the hoisted decision
site and printed in every arm. ★ The lesson is X18's, re-earned: a diagnostic
that lives inside the thing it diagnoses disappears exactly when you change that
thing.

⚠ **Mine, mid-lane: a mislabelled evidence PNG.** I copied
`build/x19/only/…` as the "collapsed" artifact after that path had been
overwritten by a post-fix run; the file was byte-identical to the poll arm.
Caught by `md5sum`-ing the pair before citing them. Re-copied from the genuine
pre-fix arm and verified distinct.

⚠ **Mine, mid-lane: an over-claimed control.** I described the crowd as the gap
gate's negative control before checking that crowd figures carry a hand mesh.
They do not. §2.3

---

## 8. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, 0 warnings, **0 SKIP lines**. All 18 binaries deleted first and **relinked this run** | `x19-gate.log` |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — branch-point gate passed 18/18 fresh before any edit | `x19-gate-baseline.log` |
| b2 | Four cache flags seeded | ✅ **PASS** — configured with absolute `MILO_ENGINE_PATH`, `Dawn_DIR`, `glfw3_DIR`, `RB3X_BUILD_ENGINE=ON`; cache re-read to confirm no auto-disable. X18's warning held: the defaults do not work in a worktree | `x19-configure.log` |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606` unmoved. The foreign uncommitted `src/platform/FxSendNative.cpp` edit is disclosed and untouched — **sixteenth lane** | verified |
| d | Shared-`src/` X360 faithfulness | ✅ **N/A — ZERO SHARED `src/` EDITS.** `git status` shows one modified file of mine, `native/src/main_render.cpp`. ⚠ This is the milestone's *result*, not a scoping dodge: §3 shows the shared-`src/` change the charter anticipated is unnecessary and unjustified | §3.3 |
| d2 | objdiff position of touched TUs | ✅ **N/A — no TU in the X360 build was touched.** `native/src/main_render.cpp` **cannot be scored at all**: it is not in `objdiff.json` and is native-only by design | stated, not implied |
| e | PNG determinism ×2 | ✅ **PASS** — `default`, `poll`, `only` each `cmp`-identical across two full runs, re-verified with the final binary. The `default` arm differs from the other two, so the set is not vacuous | §3.2 |
| f | Prior evidence non-regressed vs **artifacts** | ✅ **PASS** — `default` **BYTE-IDENTICAL** to X18's `x18-A-base-club.png`; `poll` **BYTE-IDENTICAL** to `x18-B-poll-club.png`. X18's 7380/54/12, 4351/236/2793, 123, 3306/63 and `6.172e+01` `bone_mic_stand_bottom` all reproduce exactly; X17's 1151/1167/1097/1145 and 18/29/6/10 likewise | §4 |
| g | `RB3_BAND_PLACE=1` present | ✅ **PASS** — every cited run carries it; the 7380-bone / 12-hand-mesh denominators confirm the band was placed | §1 |

### 8.1 Frames — opened

`x19-A-default-club.png`, `x19-B-poll-club.png`, `x19-C-only-retired-club.png`,
`x19-D-precoupfix-collapsed.png`. **Opened C and D.**

- **C (`only`, call retired):** venue lit and intact, textured crowd on the
  balcony, **four upright band members at four distinct stage positions** in the
  known untextured pink, **heads present**. No shards, no missing geometry.
- **D (`only` before the coupling fix):** the pink figures are visibly sparse
  and headless — the collapse X18 photographed, reproduced and then removed.

⚠ Carried forward: read the gate's own verdict line, not the pipeline exit code.
`--frames` pinned at 1 for every cross-arm comparison.

---

## 9. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Give the band's simulated chains a per-member home** | §4: 120 of 123 deviating bones are `CharHair`/IK publications onto the **one shared unplaced skeleton**. That is now a single, precisely-located structural fact rather than a 123-bone residual. Note `rb3/docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` records four un-share attempts as **proven dead ends** — read it before retrying any of them | X20 |
| ★★★ **Fix `FindBoneNamed` at its other call sites** | §2: `FindBoneNamedOwn` is used only by the gap gate. The arm-chain walk and the `--char-topo` probes still take the first match, so any future per-figure band number is exposed to the same defect | X20 |
| ★★ **A geometric oracle with a reference pose** | §2.2: bone-inside-geometry now holds, but nothing checks finger curl or agreement with authored data. This is the remaining gap between "un-invalidated" and "correct" | X20 |
| ★★ **FULL rebind scope under a driven clip is UNMEASURED** | §3.3: the in-tree shard warning for the animated case stands un-refuted, and is the only reason the shared-`src/` default is still torso. A clip-driven extent measurement would settle it. ⚠ `--clips` takes an **ark-relative** path; my `char/crowd/anim/gen/female_base.milo_xbox` did **not** load (`gClipsFile` is still set, so the torso-scope control was valid regardless) | X20 |
| ★★★ **Register `OutfitConfig` natively, then re-run the skin census** | §5: the census is now the acceptance test — `dummy_torso/legs/feet.tex` must become the real per-character skin textures and `head_naked.mat`'s NULL must fill. The blocker is the unresolved `BandIKEffector` symbols that referencing `Init()` makes live, **not** its three `New<>` calls | X20 |
| ★★ **Sample `dummy_torso.tex`'s texels** | §5.4: one step from "the blocker is located" to "the colour mechanism is decided" | X20 |
| ★★ **`male_tattoo_head.mesh` / `female_tattoo_head.mesh` remain at the origin** | Present in **both** arms, unchanged by this lane — pre-existing, not introduced. Centroids `(-0.00, 3.07, 67.15)` / `(0.00, 2.41, 65.37)` | X20 |
| ⚠ **The crowd cannot control the gap gate** | §2.3: crowd figures carry no hand mesh. A different negative control is needed | as before |
| ⚠ **Configure worktree native builds with four ABSOLUTE cache flags** | X18 gate b2, confirmed again | as before |
| ⚠ **`RB3_BAND_PLACE=1` required; pin `--frames`** | carried from X17/X18, held | as before |
| ⚠ **The 12 `ObjOwnerPtr` sites, `SEEDED_NO_REPL`, `ObjPtrList` NULL-entry, `CharMeshHide::HideAll`, orphans, `BandCamShot`** | carried from X16/X17/X18, untouched | as before |
| ⚠ **Engine CR: none filed** | this lane needed no engine change | — |

---

## 10. Recommended X20 shape

1. ★★★ **"A or B?" can be answered "both", and the disjunction is what makes it
   look undecidable.** X18 framed the sharing as genuine-or-artifact and could
   not settle it. Enumerating instead of counting showed both hold at once, in
   different places. When a question resists, check whether it is really two
   questions.
2. ★★★ **Ask what the instrument POINTS AT, not just what it computes.** Five
   lanes trusted the gap gate's arithmetic. The arithmetic was always right; the
   *operand* belonged to no member. §2
3. ★★★ **A predecessor can be right about the symptom and wrong about the
   locus.** X18 correctly identified the rebind scope as the blocker and then
   located it in shared `src/`. It was twenty lines away, in the driver. §3.1
4. ★★ **A diagnostic that lives inside the thing it diagnoses disappears exactly
   when you change that thing.** §7
5. ★★ **"The shipped default" deserves one `grep`.** Three lanes reasoned about
   departing from retail behaviour for a function that is entirely inside
   `#ifdef HX_NATIVE`. §3.3
6. ★ **Re-`md5sum` an artifact you re-copied.** §7

---

## 11. Evidence

All under `/home/free/tmp/laneX19/evidence/`.

| file | what it shows |
|---|---|
| `x19-m1-candidates.log` | **the sharing settled** — two candidates per player, shared vs own, with chain roots — §1 |
| `x19-m1-ownbone2.log` | the corrected gap: `0.000 (INSIDE)` on all four `hands_naked` vs 32–128u legacy — §2 |
| `x19-rootedness.log` | **the blast radius** — 2607 own / 4773 foreign, 3 vs 120 deviating — §4 |
| `x19-arm-only.log` / `x19-arm-onlyfull.log` | X18's collapse and `only+FULL` arms reproduced exactly — §3 |
| `x19-final-{default,poll,only}-r{1,2}.log` | the three arms after retirement, determinism ×2 — §3.2 |
| `x19-animated-control.log` | the animated path: `--clips` ⇒ torso scope preserved — §3.3 |
| `x19-A-default-club.png` | **byte-identical to X18's `x18-A-base-club.png`** |
| `x19-B-poll-club.png` | **byte-identical to X18's `x18-B-poll-club.png`** — the call is retired with no observable change |
| `x19-C-only-retired-club.png` | **opened** — four members on four marks, heads present, call retired — §8.1 |
| `x19-D-precoupfix-collapsed.png` | **opened** — the pre-fix collapse, verified distinct by `md5sum` — §7 |
| `x19-texture-census.log` | **the texture blocker** — 52 skin material instances, `dummy_*` placeholders + 4 NULL — §5 |
| `x19-gate.log` / `x19-gate-baseline.log` | native gate PASS 18/18 fresh, 0 SKIPs; branch-point health |
| `x19-configure.log` | the four absolute cache flags, no auto-disable — gate b2 |
</content>
