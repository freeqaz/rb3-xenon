# X17 — the pose residual and the rebind skip share a SET, not a CAUSE; X16's one-defect hypothesis is refuted causally and confirmed set-wise

**Date:** 2026-08-03
**Predecessor:** [X16](x16-ownerptr-class-2026-08-03.md) "the `ObjOwnerPtr` null is a class of 14, repaired upstream; X15's pose cause refuted"
**Branch:** `x17-pose-residual`, from `main` @ `fa0d0914`
**Engine:** `milo-native-engine` pinned at **`138e1606`**, **zero engine edits**
**Change surface:** **one native-only file** — `native/src/main_render.cpp`. **No shared `src/` edit at all**, so the X360 blast radius is **zero by construction**, not by measurement.

---

## Verdict

★★★ **THE TWO DEFECTS ARE NOT ONE — CAUSALLY.** X16's hypothesis was that the
polled-pose residual and X15's rebind-skip are the same defect. With the rebind
**completely disabled**, every per-figure recompose line is **byte-identical** to
the full-rebind arm. The same A/B moves all four members' hands from four
distinct authored slots onto a **single collapsed point** — the largest
geometric change available in this scene. The rebind is maximally potent and
the residual does not move. §2

★★★ **BUT X16's SET-LEVEL CROSS-REFERENCE IS EXACTLY RIGHT.** Dumped the *whole*
deviating population instead of the argmax: **123 of 7380 admissible bones
(1.7%)**, and of the **60 distinct names, 60 are hair or trouser bones** — in
**both** naming conventions — plus exactly **three** others (`bone_pelvis`,
`bone_mic_stand_bottom`, `bone_mic`). That is X15's rebind-skip family, named
independently. §1

★★★ **SO THE RIGHT ANSWER IS "ONE CONDITION, SIBLING SYMPTOMS", NOT YES OR NO.**
These hair/trouser/prop bones are not resolvable under the member — which makes
the rebind skip them (`Find` fails) **and** leaves them out of what `Poll()`
recomposes. Neither symptom causes the other; both are downstream of the same
structural fact. §3

★★ **THE RESIDUAL IS CHAIN-STRUCTURED, AND ONLY ~57 BONES ARE CANDIDATE SITES.**
Every deviating bone is labelled ROOT (parent clean ⇒ real site) or inherited
(parent already deviating ⇒ propagation). ROOTs hang off exactly three kinds of
attachment point: `bone_hair.mesh`, `exo_pelvis.mesh`, and the **Character root
itself** (`player1`, `player2`). No lane has had this decomposition. §4

⛔ **NOT A CONVERGENCE ARTIFACT AND NOT X13's CONTAMINATION.** The residual is
**invariant** at `6.172e+01` across poll iterations **1, 2, 8 and 32**, and with
`Enter()` on. §5

✅ **X15's `Enter()` RESULT REPRODUCES EXACTLY** — `bone_R-hand.mesh` at
`1.097e+01` / `1.102e+01` on two figures (X15: "1.09e+01"). Its *measurement* was
right; §6 questions its *interpretation*.

⛔ **A REPRODUCTION HAZARD THAT SILENTLY PASSES THE GATE.** Without
`RB3_BAND_PLACE=1` the oracle measures a **bandless** scene and prints
`[PASS] handpose-recompose` over 2636 bones. I hit this on my first run. §7

⚠ **THE POSE STILL DOES NOT VALIDATE, AND X14's DRIVER-SIDE CALL IS NOT
RETIRED.** Third lane to hold this line. §9

⛔ **RETRACTED, MINE, MID-LANE: one mover count taken over a contaminated
field.** §8

⚠ **Textures: UNREACHED.** Milestone 1 consumed the lane. §10

✅ **Gate PASS 18/18 fresh, rc=0, 0 SKIPs.** `main` was **not** broken by a
decomp lane. Default frame **byte-identical to X16's artifact**. §11

---

## 1. ★★★ The full deviating set — an argmax is not a set

Every lane from X13 to X16 reasoned about this residual from **six per-figure
`worst:` names**. X16's hypothesis is a claim about a *set*, and an argmax is
precisely the sample most likely to agree with whatever story you already hold.

`RB3_RECOMPOSE_DUMP=1` (new, native-only) prints the whole admissible deviating
population per figure. `small_club_01`, `RB3_BAND_PLACE=1 RB3_BAND_POLL=1`:

| figure | deviating / admissible | ROOT | inherited |
|---|---|---|---|
| player0 ×2 | 18 / 1151, 18 / 608 | 8, 8 | 10, 10 |
| player1 ×2 | 29 / 1167, 28 / 619 | 23, 22 | 6, 6 |
| player2 ×2 | 6 / 1097, 4 / 592 | 6, 4 | 0, 0 |
| player3 ×2 | 10 / 1145, 10 / 593 | 8, 8 | 2, 2 |
| 8 crowd figures | **0 / 51 each** | — | — |

**123 deviating of 7380 admissible — 1.7%.** The skeleton composes exactly
almost everywhere; this is not a broken pose, it is a small, structured set.

### 1.1 The 60 distinct names

**All 60** are `bone_hair_*` / `bone_hair-*` / `bone_legs_*` / `bone_legs-*`,
spanning **both** naming conventions X15 warned about (`bone_hair_l-01` *and*
`bone_hair-L-01`), plus exactly **three** non-family bones:

- `bone_pelvis.mesh` — `3.565e+00` (X15's and X16's bone, reproduced)
- `bone_mic_stand_bottom.mesh` — `6.172e+01` (the global worst)
- `bone_mic.mesh` — `9.746e+00`

★★★ X15 §7.2's unresolved-bone list was `bone_hair_*`, `bone_hair-*`,
`bone_legs_a01..g01`, `bone_legs_{L,M,R}0{1,2,3}`, `bone_legs-ring1`. The
deviating set contains **every one of those**, plus `bone_legs-ring2`. **X16's
cross-reference is confirmed, and now from the population rather than from six
samples.**

---

## 2. ★★★ …and yet the rebind is NOT the cause — three arms, one binary

If the rebind-skip *caused* the residual, changing the rebind must change the
residual. All three arms are the same binary, `RB3_BAND_POLL=1`:

| arm | rebind | per-figure recompose lines | global worst |
|---|---|---|---|
| A | FULL figure (default) | — | `6.172e+01` `bone_mic_stand_bottom` |
| B | `RB3_NO_BAND_REBIND_FULL=1` (shipped torso scope) | **identical to A** | `6.172e+01`, same bone |
| C | `RB3_NO_BAND_REBIND=1` (**never runs at all**) | **identical to A** | `6.172e+01`, same bone |

`diff` over the per-figure `recompose` lines: **zero differences**, A vs B and
A vs C.

### 2.1 ★★★ The positive control — this A/B is violently non-vacuous

The obvious objection is that the env vars did nothing. They did not:

| arm | `hands_naked` centroids |
|---|---|
| A (FULL) | **four distinct** slots: x ≈ −65.8, +13.8, −10.1, +74.2 |
| B (torso) | **all four collapse** to x ≈ −0.0, z ≈ 41 |
| C (none) | **all four collapse** to x ≈ −0.0, z ≈ 41 |

Arm A reproduces X15 §4.1 (band on its marks); arms B and C reproduce X14's
pre-repair "all four at one point". Turning the rebind off is the **largest
geometric change this scene can express** — and the recompose residual does not
move by one ULP.

⇒ **X16's one-defect hypothesis is REFUTED as a causal claim.**

### 2.2 The mechanism agrees, and was checked second

`RebindOutfitBonesToOwnSkeleton` (`src/system/bandobj/BandCharacter.cpp:769-1051`)
only ever calls `mesh->SetBone(b, own, calc)` (`:940`), which writes
`mBones[idx].mBone` and optionally `mBones[idx].mOffset`
(`src/system/rndobj/Mesh.cpp:1083-1090`). It touches **no** `RndTransformable`'s
`LocalXfm`, `WorldXfm` or `TransParent`. The oracle
(`native/src/main_render.cpp:2316-2337`) compares
`Multiply(t->LocalXfm(), p->WorldXfm())` against `t->WorldXfm()` — a **bone
hierarchy** identity that never reads a mesh bone palette.

★ Recorded in this order deliberately. The measurement decided it; the source
reading only explains it. Three lanes in a row were wrong doing the reverse.

⚠ **Note for the shipped default:** arm B is the *shipped* torso scope, and it
collapses the hands. Only the FULL scope places them. X14 flagged FULL as "the
one place this lane departs from the shipped default"; that departure is
load-bearing, not cosmetic.

---

## 3. ★★★ The precise answer: one condition, sibling symptoms

Both symptoms are downstream of a single structural fact — **these hair, trouser
and prop bones are not resolvable under the member's own skeleton**:

| symptom | mechanism | lane |
|---|---|---|
| rebind **skips** 7 meshes | `Find<RndTransformable>(name, false)` fails under this member (`BandCharacter.cpp:902`) ⇒ all-or-nothing skip (`:913`) | X15 |
| recompose **deviates** on the same bones | those bones are not in what `Poll()` recomposes; parents move, they do not | X17 |
| hairpieces **float** at the venue origin | same bones, never composed into the member's placed world | X15 |

Neither symptom is upstream of the other — arm C proves it. They are **siblings**.
"Are these one defect?" has no yes/no answer: **one condition, three symptoms**,
and repairing the rebind would fix the first and third while leaving the second
exactly where it is.

---

## 4. ★★ Where the residual actually lives — ROOT vs inherited

`W == L*parentW` is a **chain** identity: a bone whose parent's world is already
wrong inherits a deviation it did not cause. The dump labels each bone by whether
its parent is clean.

**~57 ROOT bones of 123.** Every ROOT hangs off one of exactly three attachment
points:

```
ROOT   dev 6.172e+01  bone 'bone_mic_stand_bottom.mesh'  parent 'player2'          (parent dev 0.000e+00)
ROOT   dev 3.565e+00  bone 'bone_pelvis.mesh'            parent 'player1'          (parent dev 0.000e+00)
ROOT   dev 9.746e+00  bone 'bone_mic.mesh'               parent 'bone_mic_stand_top.mesh'
ROOT   dev 5.966e-01  bone 'bone_legs-ring1.mesh'        parent 'exo_pelvis.mesh'  (parent dev 0.000e+00)
ROOT   dev 2.725e-01  bone 'bone_hair-back-01.mesh'      parent 'bone_hair.mesh'   (parent dev 0.000e+00)
inherit dev 1.049e+00 bone 'bone_hair-back-02.mesh'      parent 'bone_hair-back-01.mesh' (parent dev 2.725e-01)
```

★★ `bone_pelvis` and `bone_mic_stand_bottom` — the two bones every previous lane
quoted — are **parented directly to the `Character` object itself**, not into the
skeleton. They are attachment roots, not body bones. `bone_pelvis` carrying
`3.565e+00` is therefore **not** "the pelvis is mispose d"; it is an attachment
root whose parent moved.

### 4.1 ⚠ Sampled, not settled: are the ROOTs stale?

For six sampled ROOT bones (`bone_mic_stand_bottom`, `bone_mic`, `bone_pelvis`,
`bone_hair-back-01`, `bone_legs-ring1`, `bone_legs_R01`) the published world is
**bit-identical between the no-Poll and Poll arms** — they did not move at all —
and sits at unplaced, bind-space coordinates (e.g. `bone_mic_stand_bottom` at
`(0.000, 10.000, 0.000)` while player2 stands at `(-10.026, 31.389, 13.218)`).
That is consistent with "never composed into the member's world", and with X15's
floating hairpieces.

⚠ **Stated as a sample, not a set result.** `RB3_RECOMPOSE_DUMP=all` emits all
7380 rows in both arms and 1309 bones genuinely move under `Poll()`, but 59
hair/leg bones are **among** the movers — so the population splits, and a
name-keyed diff is confounded by the same bone name recurring across figures.
**Whether every ROOT is frozen is UNRESOLVED.** It is the cheapest next probe and
the instrument to answer it is already committed.

---

## 5. ⛔ It is neither a convergence artifact nor X13's contamination

| arm | global worst |
|---|---|
| `RB3_BAND_POLL_ITERS=1` | `6.172e+01` `bone_mic_stand_bottom` |
| `=2` | `6.172e+01`, same bone |
| `=8` (default) | `6.172e+01`, same bone |
| `=32` | `6.172e+01`, same bone |
| `RB3_BAND_ENTER=1` | `6.172e+01`, same bone |

A lag/settling artifact would decay over 32 iterations of a static pose. It does
not move at all. X13's un-`Enter()`ed-driver contamination would be removed by
`Enter()`. It is not.

---

## 6. ✅ X15's `Enter()` measurement reproduces — and ⚠ its reading is questionable

`Enter()` changes exactly **two** of twelve per-figure lines, both to the same
bone:

| figure | poll | poll + `Enter()` |
|---|---|---|
| player0 | `1.154e+00` `bone_legs-ring2` | **`1.097e+01` `bone_R-hand.mesh`** |
| player3 | `1.413e+00` `bone_hair_l-02` | **`1.102e+01` `bone_R-hand.mesh`** |

X15 reported "1.09e+01 at `bone_R-hand.mesh`". **Reproduced to three figures.**
The global worst is unchanged, so X15's number was per-figure — worth stating,
because it reads as a global in X15 §5.2.

⚠ **But "worse" may be the wrong word, and this is a live question for X18.**
`bone_R-hand` is an **IK-solved** bone, and IK publishes a solved bone through
`SetWorldXfm` (`rndobj/Trans.cpp:419`), which writes `mWorldXfm` directly and
clears `mDirty`. The oracle's own comment
(`native/src/main_render.cpp:2296-2310`) names that as a case where a flat
`L*parentW` check "reports a deviation that is the ENGINE'S DESIGN, not a
defect" — but `RecomposeAdmissible` (`:2311-2314`) screens only
`TransConstraint()` and `HasDynamicConstraint()`. **It does not screen
`SetWorldXfm`-published bones.** So `Enter()` turning IK on and the oracle
immediately reporting the IK bone is the *expected* reading too, and the two are
not distinguished by any evidence I have.

⚠ **I did not settle this.** It matters because it is the difference between
"the pose is wrong" and "the gate cannot speak for an animated IK bone".

---

## 7. ⛔ A reproduction hazard that PASSES

My first two runs used the invocation as documented and produced:

```
=== hand-pose summary: 2636 ADMISSIBLE bone(s) recomposed (worst dev 0.000e+00, -),
    20 excluded; 0 hand mesh(es) measured ===
[FAIL] handpose-measured-hand-geometry — a skeleton was reached but NO hand mesh
       was measured — every geometry verdict above is vacuous
[PASS] handpose-recompose
```

**`RB3_BAND_PLACE=1` is opt-in** (`main_render.cpp:3168`). Without it the band is
never placed, the oracle measures a bandless venue (2636 bones, 320 meshes), and
`handpose-recompose` **PASSES**. The correct denominator is **7380 bones / 12
hand meshes / 411 meshes**.

★ X13's `handpose-measured-hand-geometry` gate is what caught it — the charter's
"make the denominator part of the verdict" rule working exactly as designed, on
me, in my first ten minutes. Recorded because **no predecessor doc states the env
recipe**, and the failure mode is a green recompose line.

**The reproduction recipe, in full:**

```zsh
cd native
RB3_BAND_PLACE=1 RB3_BAND_POLL=1 ./build/rb3-render \
    /home/free/code/milohax/rb3/orig-assets/xbox-zip build/x17/hpP \
    world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox --hand-audit
```

---

## 8. ⛔ Retracted / corrected, mine, mid-lane

⛔ **A mover count taken over a contaminated field.** I diffed the two arms'
full dumps to count "bones whose world moved under `Poll()`" — but the dumped
line also carries the **parent dev** field, which differs between arms *by
construction*. Every row could differ for a reason unrelated to the world. Caught
by reading my own sample output (`parent dev 7.743e-02` on a line I was calling a
"mover"), re-measured with the dev field stripped. **The count held at 1309**, so
the conclusion survived — but it was luck, not method, and the first number was
not evidence.

⚠ **`--frames` is a silent confound across lanes.** My poll PNG is not
byte-identical to X16's `x16-B-poll-club.png`. Cause: X16 rendered `--frames 1`,
the driver default is `--frames 4`. Census is identical (411 meshes, 203 draws,
38.92% coverage); only the animated arm's colour count moves (122552 vs 122563).
The **default** arm is byte-identical to X16 because bind pose is frame-invariant.
**Not a regression** — but it would read as one, and cross-lane PNG comparisons
need the frame count pinned.

---

## 9. ⚠ The pose does not validate; X14's driver-side call is NOT retired

The acceptance is three-part: `Poll()` performs the rebind (X15, holds), **the
pose validates against the absolute oracle**, and removing the call leaves the
frame demonstrably correct.

**Part two still fails.** `handpose-recompose` FAILs in the poll arm. This lane
narrowed the failure from "the pose is untrustworthy" to "123 of 7380 bones, all
hair/trouser/prop attachment chains, ~57 real sites" — and refuted the leading
explanation for it — but did not make it pass. §6 raises a real possibility that
part of the residual is the gate's own blind spot rather than a defect; **that is
not established**, and adopting an unvalidated pose as the default on the
strength of an unproven doubt would be exactly the wrong trade.

⇒ **Held. Three lanes, three refusals, same reason.**

---

## 10. ⚠ Milestone 2 — textures: UNREACHED

Not attempted. Milestone 1's A/B matrix and the set dump consumed the lane. X15's
re-derivation stands untouched and unre-verified by me: `OutfitConfig::Init()` is
compiled but called by nothing (`native/src/milo_object_factories.cpp:467-495` is
a block comment), `"Can't make OutfitConfig"` (`obj/DirLoader.cpp:1049`) is a pure
registration test, and the bill is 191/1511/48 symbols — 37 mechanical, 11 a real
port. The band renders untextured pink in this lane's frames too, unchanged and
still **pre-existing**.

---

## 11. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Are the pose and rebind-skip defects ONE?** | ⛔ **NO, CAUSALLY — refuted by a three-arm A/B with a violent positive control** | §2 |
| **Is X16's SET cross-reference right?** | ★★★ **YES — confirmed from the population, not the argmax** | §1 |
| **The right framing** | ★★★ **ONE CONDITION, SIBLING SYMPTOMS — neither is upstream of the other** | §3 |
| **Size of the residual** | ★★★ **VERIFIED — 123 / 7380 admissible bones (1.7%)** | §1 |
| **Structure of the residual** | ★★ **VERIFIED — chain-structured; ~57 ROOT sites off 3 attachment points** | §4 |
| **`bone_pelvis` / `bone_mic_stand_bottom`** | ★★ **RECHARACTERISED — attachment roots parented to the `Character`, not body bones** | §4 |
| **Are the ROOTs stale?** | ⚠ **UNRESOLVED — 6 sampled bones frozen; the set result is confounded** | §4.1 |
| **Convergence / lag artifact?** | ⛔ **NO — invariant over 1, 2, 8, 32 iterations** | §5 |
| **X13's un-`Enter()`ed contamination?** | ⛔ **NO — `Enter()` does not move the global worst** | §5 |
| **X15's `Enter()` measurement** | ✅ **REPRODUCED to three figures (1.097e+01 / 1.102e+01)** | §6 |
| **X15's reading of `Enter()` as "worse"** | ⚠ **QUESTIONED, NOT REFUTED — `bone_R-hand` is IK-published; the oracle does not screen that** | §6 |
| **Oracle admissibility under animation** | ⚠ **KNOWN GAP — screens constraints, not `SetWorldXfm` publication** | §6 |
| **X16's `6.172e+01` / `3.565e+00`** | ✅ **REPRODUCED EXACTLY** | §1, §5 |
| **Is the polled POSE correct?** | ⛔ **STILL UNDECIDED — narrowed, explanation refuted, not validated** | §9 |
| **X14's driver-side call retired?** | ⛔ **NO — part two of the acceptance still fails; deliberately held** | §9 |
| **Shipped torso rebind scope** | ⚠ **FINDING — the shipped scope COLLAPSES all four hands; only FULL places them** | §2.1 |
| **Reproduction recipe** | ⛔ **HAZARD — without `RB3_BAND_PLACE=1` the oracle passes over a bandless scene** | §7 |
| **Frames** | ✅ **NO REGRESSION — opened; venue, crowd, band intact; no shards, no hair explosion** | §12 |
| **X360 blast radius** | ✅ **ZERO BY CONSTRUCTION — no shared `src/` file touched** | §12 |
| **Textures / `OutfitConfig`** | ⚠ **UNREACHED** | §10 |
| **One of my own measurements** | ⛔ **RETRACTED — contaminated diff field; re-measured, count held** | §8 |

---

## 12. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, 0 warnings, **0 SKIP lines**. All 18 binaries **deleted first** and relinked this run; binary presence re-confirmed (`ls -la`) before every probe | `x17-gate.log` |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — baseline gate on the branch point passed 18/18 fresh before any edit. `main` advanced only by `408a58f7`, a docs-only DS-2 commit | `x17-gate-baseline.log` |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606` unmoved. The foreign uncommitted `src/platform/FxSendNative.cpp` edit is disclosed and untouched — **fourteenth lane** | §Verdict |
| d | Shared-`src/` X360 faithfulness | ✅ **N/A — ZERO SHARED `src/` EDITS.** `git status` shows exactly one modified file, `native/src/main_render.cpp`. A change confined to `native/` cannot reach the X360 build, so there is no A/B to run and none is implied | §11 |
| d2 | objdiff position of touched TUs | ✅ **N/A — no TU in the X360 build was touched.** Stated rather than left implied: `native/src/main_render.cpp` is **not scoreable at all** and never has been; it is not in `objdiff.json` and is native-only by design | stated, not implied |
| e | PNG determinism ×2 on every cited image | ✅ **PASS** — both cited PNGs `cmp`-identical across two full runs; the two arms differ from each other, so the pair is not vacuous | §12.1 |
| f | Prior evidence non-regressed vs **artifacts** | ✅ **PASS** — default frame **BYTE-IDENTICAL** to X16's `x16-A-default-club.png`. X16's `6.172e+01`/`bone_mic_stand_bottom`, `3.565e+00`/`bone_pelvis`, the 7380/54/12 denominators and X15's `Enter()` `1.09e+01` all reproduce exactly. Poll-frame delta explained as `--frames` (§8) | §8, §1 |

### 12.1 Frames

`x17-A-default-club.png` and `x17-B-poll-club.png` (determinism ×2 each).
**Opened both.** Venue lit and intact, the 180-draw crowd placed on the balcony,
four band members on the stage in the known untextured pink. **No shards, no
explosion, no missing geometry**; X14's 7–14× hair blow-up does not recur.

⚠ **Read the gate's own verdict line, not the pipeline exit code** — carried from
X12–X16: `grep -c SKIP` exits 1 on zero matches, so the failure code *is* the
0-SKIPs result.

⚠ **No `--revert` trap was live this lane** — nothing in `src/` was reverted,
because nothing in `src/` was edited.

---

## 13. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Settle whether every ROOT bone is frozen** | §4.1: 6 sampled ROOTs have worlds bit-identical across arms, but 59 hair/leg bones are among the 1309 movers, so the population splits. Needs a **figure-keyed** comparison (bone names recur across figures, which is what confounds a name-keyed diff). `RB3_RECOMPOSE_DUMP=all` already emits both arms; only the comparison needs writing | X18 |
| ★★★ **Decide whether the oracle can speak for an animated IK bone** | §6: `RecomposeAdmissible` screens `TransConstraint()`/`HasDynamicConstraint()` but **not** `SetWorldXfm` publication, which the oracle's own comment names as the design case. Until this is settled, `handpose-recompose` FAILing under `Enter()` is **ambiguous**, and it is the gate blocking X14's call. Cheapest instrument: a `SetWorldXfm` last-writer tag (⚠ shared `src/`, needs `HX_NATIVE` gating + the full X360 A/B) | X18 |
| ★★★ **Attack the ~57 ROOT sites, not the 123 bones** | §4: the inherited 66 are propagation and will clear themselves. The three attachment points are `bone_hair.mesh`, `exo_pelvis.mesh`, and the `Character` root | X18 |
| ★★ **The shipped torso rebind scope collapses the hands** | §2.1: arm B is the *shipped* default and puts all four members' hands at one point. Only `RB3_SKEL_REBIND_FULL` places them. That is a real gap between shipped behaviour and correct behaviour, and nobody has framed it that way | X18 |
| ★★ **Fixing the rebind will NOT fix the pose** | §2/§3: worth stating loudly, because X16's handoff proposed `tightdistressedpants_resource.1.mesh` (one bone) as the cheapest probe *of the pose*. It is still the cheapest **rebind** repair; it will move the skip set and the floating hair and will **not** move the recompose residual by one ULP. Arm C already proves this | X18 |
| ★★ **`OutfitConfig` registration** | §10: untouched. X15's price (37 mechanical + 11 a real port) is inherited, **not** re-verified by me | its own lane |
| ⚠ **Pin `--frames` in cross-lane PNG comparisons** | §8: X16 used `--frames 1`, the default is 4; the animated arm's PNG differs for that reason alone | as before |
| ⚠ **Document `RB3_BAND_PLACE=1` in the repro line** | §7: its absence produces a green `handpose-recompose` over a bandless scene | as before |
| ⚠ **The 12 newly-repaired `ObjOwnerPtr` sites, `SEEDED_NO_REPL` (2), `ObjPtrList` NULL-entry, Direction-B rows, `CharMeshHide::HideAll`, orphans, `BandCamShot`** | carried from X16, untouched | as before |
| ⚠ **Engine CR: none filed** | this lane needed no engine change | — |

---

## 14. Recommended X18 shape

1. ★★★ **A predecessor's hypothesis can be right about the SET and wrong about
   the CAUSE.** X16 saw hair/trouser bones in both lists and inferred one defect.
   The set claim was *exactly* right — better than it knew, 60 of 60 names. The
   causal claim was wrong. Those are different claims and they needed different
   experiments; the set needed a dump, the cause needed an A/B.
2. ★★★ **Test a cause by making the suspect maximally potent, then showing the
   symptom does not care.** Arm C did not merely disable the rebind — it
   collapsed all four members' hands onto one point. A suspect that powerful,
   with a residual that does not move by one ULP, closes the question in one run.
3. ★★ **Dump the population before you theorise about the argmax.** Six `worst:`
   names supported four lanes of narrative. The set was 123 bones and told a
   different story in one command.
4. ★★ **Label propagation as propagation.** 66 of 123 deviating bones are
   inherited from a deviating parent. Attacking them individually would have been
   57 sites of real work and 66 of noise.
5. ★ **Your own diff field can be the confound.** §8: I compared lines carrying a
   value that differs between arms by construction. The count survived; the method
   did not.

---

## 15. Evidence

All under `/home/free/tmp/laneX17/evidence/`.

| file | what it shows |
|---|---|
| `x17-hp-default.log` / `x17-hp-poll.log` | X16 reproduced exactly — 7380 / 54 / 12; `0.000e+00` PASS vs `6.172e+01` `bone_mic_stand_bottom` FAIL — §1 |
| `x17-hp-poll-torso.log` | rebind at **shipped torso scope** — recompose identical, hands collapsed — §2 |
| `x17-hp-poll-norebind.log` | **rebind never runs** — recompose still identical, hands collapsed — §2, the refutation |
| `x17-hp-poll-iters{1,2,32}.log` | residual invariant over poll iterations — §5 |
| `x17-hp-poll-enter.log` | `Enter()` arm; X15's `1.09e+01` `bone_R-hand` reproduced — §6 |
| `x17-dump-default.log` | **negative control** — 0 deviating bones over real denominators (1097/1145/1151/1167/592/593/608/619) — §1 |
| `x17-dump-poll.log` | the **full deviating set**: 123 bones, ROOT/inherited labels, parents — §1, §4 |
| `x17-dumpall-{default,poll}.log` | all 7380 admissible bones + worlds in both arms, for the staleness question — §4.1 |
| `x17-A-default-club.png` | default frame — **byte-identical to X16's `x16-A-default-club.png`** (determinism ×2) |
| `x17-B-poll-club.png` | poll frame (determinism ×2); `--frames` delta vs X16 explained — §8, §12.1 |
| `x17-gate.log` | native gate PASS 18/18 fresh, rc=0, 0 SKIPs — gate (a) |
| `x17-gate-baseline.log` | branch-point gate, proving `main` was healthy — gate (b) |
