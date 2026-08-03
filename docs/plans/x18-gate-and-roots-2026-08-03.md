# X18 — the gate was OVER-REPORTING: 123/123 residual bones are engine publications, and the retirement blocker was never the pose

**Date:** 2026-08-03
**Predecessor:** [X17](x17-pose-residual-2026-08-03.md) "the pose residual and the rebind skip share a SET, not a CAUSE"
**Branch:** `x18-gate-and-roots`, from `main` @ `82db5d93`
**Engine:** `milo-native-engine` pinned at **`138e1606`**, **zero engine edits**
**Change surface:** two shared `src/` files (`rndobj/Trans.h`, `rndobj/Trans.cpp`), every edited region inside `#ifdef HX_NATIVE`, plus one native-only driver (`native/src/main_render.cpp`).

---

## Verdict

★★★ **THE GATE WAS OVER-REPORTING. SETTLED, NOT SUSPECTED.** X17 doubted that
`RecomposeAdmissible` screens constraints but not `SetWorldXfm` publication, and
refused to act on the doubt. It is correct. In the poll arm **all 123 deviating
bones are `PUBLISHED`**, while **4351 `COMPOSED` and 2793 `LOADED` bones deviate
zero**. §1

★★★ **AND THE PUBLISHERS HAVE NAMES, NOT INFERENCES.** The captured return
address of the last `SetWorldXfm` caller resolves to three functions: **120 bones
from `CharHair::SimulateZeroTime()`**, **2 from `CharIKHand::Poll()`**, **1 from
`CharIKScale::Poll()`**. Every one of the 123 is an engine subsystem publishing a
solved world — precisely the case the oracle's own comment names as "the
ENGINE'S DESIGN, not a defect". §2

★★★ **SO X17's THREE "ATTACHMENT ROOTS" ARE THE ROOTS OF THE THREE SIMULATED
CHAINS.** `bone_hair.mesh` is where the hair sim attaches, `exo_pelvis.mesh` /
the thighs are where the trouser-cloth sim attaches, and the `Character` root is
where the two IK solvers attach. They are not misposed and they are not
attachment defects. Milestone 2 dissolves into milestone 1's answer. §2

★★★ **THE RETIREMENT BLOCKER WAS NEVER THE POSE — IT IS THE REBIND SCOPE.**
Removing X14's driver-side call collapses the band's head/eyebrows/fingernails to
the origin. With `RB3_SKEL_REBIND_FULL=1` the same removal is **byte-identical to
the arm that keeps the call**. Three lanes held retirement for a reason that was
an instrument artifact, while the real blocker sat unnamed. §4

★★★ **THE BAND SHARES ONE SKELETON, AND FIVE LANES OF PER-FIGURE NUMBERS ARE NOT
INDEPENDENT.** Caught by a control, not by looking: all four members **and** their
`outfit` sub-Characters resolve `bone_L-hand.mesh` to **one object**. 7380
admissible slots are **3306 distinct objects**; the 123 deviating are **63**. §3

⛔ **X15's TEXTURE CLAIM IS WRONG AS STATED, AND TWO LANES INHERITED IT.**
`OutfitConfig::Init()` is **not** "compiled but called by nothing" — it is called
from `src/system/bandobj/Band.cpp:114`. §6

⚠ **THE CORRECTED GATE IS MUCH WEAKER THAN IT LOOKS**, and I say so where it
cannot be missed: it asserts only that composed bones compose. §1.3

✅ **Gate PASS 18/18 fresh, rc=0, 0 SKIPs.** `main` was **not** broken by a decomp
lane. **X360 blast radius zero in emitted code**, verified by rebuild — and a
naive `cmp` says otherwise. §7

---

## 1. ★★★ The measurement: who last wrote `mWorldXfm`

### 1.1 Why the deviation number could never answer this

`WorldXfm()` (`rndobj/Trans.h:118`) returns the **cached** `mWorldXfm` whenever
`!mDirty`. `RecomposeDev` reads it. So a nonzero deviation on an admissible bone
*implies* the world was not composed on demand — the bone was clean and its cache
disagreed with `L·parentW`. There are only two ways to be in that state:
published directly (`SetWorldXfm`, which sets `mDirty=false`), or stale (a
dirty-propagation failure). **The deviation cannot distinguish them, and every
lane from X13 on has been reading it as though it could.**

`mWorldXfm` is private to `RndTransformable` and written in exactly **five**
places, all in `Trans.cpp` (verified by grep over `src/` + `native/`: no other TU
touches it). All five are tagged, so no writer can be silently misclassified:
ctor, `Load` (straight off the `.milo` stream), `Copy`, `SetWorldXfm`/`SetWorldPos`,
`WorldXfm_Force`.

⚠ **The ordering hazard that would have made the instrument lie.** `WorldXfm()`
is not an observation — when dirty it runs `WorldXfm_Force`, which rewrites the
world and stamps the bone `COMPOSED`. Measuring one bone therefore re-tags its
whole ancestor chain, and any ancestor later in `bones[]` would be read as
`COMPOSED` regardless of how its world was really produced. The instrument would
**systematically under-report `PUBLISHED`** — i.e. it would have manufactured the
answer "the gate is fine". Fixed with a whole-figure pre-pass of plain tag reads
before any measurement (`main_render.cpp`).

### 1.2 The result

`small_club_01`, `RB3_BAND_PLACE=1`, `--frames 1`:

| writer | default arm | | poll arm | |
|---|---|---|---|---|
| | admissible | deviating | admissible | deviating |
| `NEVER` | 0 | 0 | 0 | 0 |
| `COMPOSED` | 4152 | **0** | 4351 | **0** |
| `PUBLISHED` | 0 | 0 | 236 | **123** |
| `LOADED` | 3228 | 0 | 2793 | **0** |
| `COPIED` | 0 | 0 | 0 | 0 |
| **TOTAL** | 7380 | 0 | 7380 | **123** |

**123 of 123 deviating bones are `PUBLISHED`.** Both numerator and denominator
are printed per class, because "0 deviating PUBLISHED" and "there were no
PUBLISHED bones" are different findings and four vacuous passes on this ladder
came from conflating them. Note 236 published bones exist and **113 do not
deviate** — so the finding is not the tautology "everything published deviates".

### 1.3 ⚠ What the corrected gate does and does NOT cover

`handpose-recompose-composed[X18]` restricts the same identity, at the same
threshold, to the population the identity models. It **PASSES** over 4351
composed bones.

- ✅ It **does** assert that dirty-propagation is intact: a bone that composed
  earlier and whose parent then moved without dirtying it would land in the
  `COMPOSED`-deviating column. That column is 0, and §5's C3 control proves the
  column is reachable.
- ⛔ It says **nothing** about whether a published world is the **right** world.
  An IK solver publishing a hand into the floor passes this gate.
- ⛔ It says nothing about `LOADED`, `COPIED` or `NEVER` bones.

★ So the honest statement is that the pose is **un-invalidated**, not
**validated**. The charter's conditional ("if milestone 1 shows the gate was
over-reporting, 'validates' means against the corrected gate") is met — but the
corrected gate is an *algebraic* oracle, and the question "is this pose correct"
is a *geometric* one that only the hand-mesh gap gates and the opened frame can
speak to.

---

## 2. ★★★ The publishers, by name

`SetWorldXfm` records `__builtin_return_address(0)`; the audit prints it with the
PIE load base so it resolves offline.

| publisher | bones | which |
|---|---|---|
| `CharHair::SimulateZeroTime()` | **120** | 92 `bone_hair_*` + 28 `bone_legs_*` (trousers) |
| `CharIKHand::Poll()` | **2** | `bone_mic_stand_bottom.mesh`, `bone_mic.mesh` |
| `CharIKScale::Poll()` | **1** | `bone_pelvis.mesh` |

⚠ **Read the base from the right maps line.** My first resolution used the first
`/proc/self/maps` entry — that is whatever the allocator mapped first, and every
publisher resolved to `??`. Matching the executable's own path fixes it. A stale
base does not fail loudly; it resolves to a confidently wrong function.

### 2.1 X17's three attachment roots, dissolved

X17 found ~57 ROOT sites hanging off exactly three attachment points and
recharacterised `bone_pelvis` / `bone_mic_stand_bottom` as attachment roots rather
than misposed body bones. That was right, and the writer tag says what they are
roots **of**:

| X17's "attachment point" | what it actually is |
|---|---|
| `bone_hair.mesh` | where `CharHair` attaches the hair sim chain |
| `exo_pelvis.mesh` / `exo_{L,R}-thigh.mesh` | where `CharHair` attaches the trouser-cloth chains |
| the `Character` root | where `CharIKScale` / `CharIKHand` attach |

Every ROOT is a simulated-chain root; every `inherit` is that chain's descendants.
**There is no attachment defect to attack.** The 57 sites X17 handed to this lane
as "the thing to fix" are the correct output of three engine subsystems.

Corroborating: the published worlds are figure-local and plausible, not
bind-space. `bone_pelvis` publishes at `(14.506, 145.424, 48.180)` against
player1 at `(14.429, 146.133, 13.182)`; `bone_mic` at `(-10.022, 29.967, 65.169)`
against player2 at `(-10.026, 31.389, 13.218)`. ★ X17 §4.1 sampled these in the
**no-Poll** arm, where the sim has not run, and saw bind-space coordinates — which
is why "frozen/stale" looked plausible. Under Poll they are placed.

---

## 3. ★★★ The band shares one skeleton — found by a control, not by looking

The `SetWorldXfm` control (§5, C2) displaces `bone_L-hand` by a known `+P`. Each
of the 8 **crowd** figures reported exactly `P`. The 8 **band** entries reported
`P, 2P, 3P … 8P` — a displacement that compounds across figures can only mean
successive figures are publishing onto the **same object**.

Confirmed by printing the pointer:

| figure | `bone_L-hand.mesh` | world |
|---|---|---|
| crowd_male01…04 | 4 **distinct** pointers | own |
| player0 / outfit / player1 / outfit / player2 / outfit / player3 / outfit | **all `0x…46e2770`** | `(-21.749, 0.197, 44.274)` |
| crowd_female01…04 | 4 **distinct** pointers | own |

⚠ Consequences, stated plainly:

- The 7380-bone denominator is **3306 distinct objects**; the 123 deviating are
  **63**. Every per-figure band count on this ladder — including X17's
  `18/1151`, `29/1167`, `6/1097`, `10/1145` table — counts `(figure, bone)`
  **slots**, not bones.
- The shared bone's world `(-21.7, 0.2, 44.3)` is near **none** of the four
  players. The band's *geometry* is placed by the mesh bone palettes (the rebind),
  not by four independent skeletons — which is exactly why the rebind is
  load-bearing and why removing it collapses everything (§4).
- ⚠ **I did not determine** whether this is one skeleton genuinely shared by four
  members, or `FindBoneNamed` returning the first of several matches from
  overlapping `CollectDeep` collections. The audit now prints the per-figure count
  of bones carrying that name; settling it is the cheapest next probe.

---

## 4. ★★★ X14's driver-side call: the blocker was the rebind SCOPE

Three arms, one binary, `--frames 1`, all deterministic ×2:

| arm | fingernails centroid | head centroid | eyebrows centroid |
|---|---|---|---|
| `base` (X14's direct call) | `(-70.03, 79.74, 52.03)` | `(-70.09, 77.42, 80.69)` | `(-70.13, 75.94, 82.57)` |
| `RB3_BAND_POLL=only` (call removed) | `(-0.00, 0.88, 38.57)` | `(0.00, 3.23, 67.21)` | `(0.00, 4.71, 69.09)` |
| `only` + `RB3_SKEL_REBIND_FULL=1` | `(-65.41, 68.67, 57.23)` | `(-70.08, 77.50, 80.49)` | `(-70.13, 75.97, 81.59)` |

**Removing the call collapses the band to the origin** — visible in the frame,
not merely in numbers (§8). **Restoring FULL rebind scope fixes it**, and
`only + FULL` is **byte-identical** to the `poll` arm that keeps the call.

⇒ The direct call is **redundant at FULL scope** and **load-bearing at the shipped
torso scope**. This is X17 §2.1's finding ("the shipped torso scope collapses the
hands; only FULL places them") shown to be *the* retirement blocker.

### 4.1 The three-part acceptance, scored

| part | verdict |
|---|---|
| `Poll()` performs the rebind | ✅ X15, and re-confirmed: `only + FULL` places all four members |
| the pose validates against an oracle you trust | ✅ **against the corrected gate**, PASS over 4351 composed bones, with C3 proving it can fail — **but see §1.3 for what that does not cover** |
| removing the call leaves the frame **demonstrably correct** | ✅ **only in the `only + RB3_SKEL_REBIND_FULL=1` configuration**; ⛔ **FAILS** at the shipped torso scope |

⇒ **Retirement is unblocked, conditional on one named change: `Poll()`'s rebind
must run at FULL scope.** I did **not** make that change. Flipping the shipped
default is a consequential behavioural edit to shared `src/`
(`BandCharacter.cpp:849`) that X14 and X17 both flagged as a deliberate departure
from shipped behaviour; it deserves its own lane, its own X360 A/B and its own
evidence, not a drive-by at the end of an instrumentation lane. **The reason to
hold is now completely different from the previous three lanes' reason, and it is
a much smaller one.**

---

## 5. ★★★ The controls — three, because a gate never seen to fail proves nothing

| control | mechanism | writer tag | old gate | corrected gate |
|---|---|---|---|---|
| **C1** `RB3_HANDPOSE_PERTURB=5` (`SetLocalXfm`) | legitimate motion; dirties and recomposes | stays `COMPOSED` | PASS | PASS |
| **C2** `RB3_HANDPOSE_PUBLISH=5` (`SetWorldXfm`) | publication | flips to `PUBLISHED` | **FAIL** `4.000e+01` | PASS |
| **C3** `RB3_HANDPOSE_STALE=7` (forged) | dirty-propagation failure | forced `COMPOSED` | FAIL | **FAIL** `5.600e+01` |

★ **C1 and C2 are the same bone displaced by the same amount** with opposite
predictions on both the tag and the deviation. If the instrument were blind, or
merely echoed "this bone moved", the two arms would be indistinguishable.

★★★ **C3 is the one that makes the PASS mean anything.** The corrected gate had
passed in every real arm; a gate only ever observed passing is not evidence. C3
forges the exact defect class it exists to catch — a bone marked clean and tagged
`COMPOSED` whose cached world is stale by a known amount — and the gate fails at
that magnitude, over a 4136-bone denominator.

⚠ **A control whose prediction missed, and what it bought.** C2 predicted a
deviation of exactly `P` and delivered `8P`, perfectly linear over
`P ∈ {1,2,5,10}`. I did not wave it through: chasing the factor is what uncovered
the shared band skeleton (§3), which is a larger finding than the control was
built to produce.

---

## 6. ⛔ Textures — NOT reached, and the inherited bill is WRONG

**Sixth consecutive lane.** Not attempted; milestone 1 and its controls consumed
the lane. But the one cheap thing X17 flagged as un-re-verified, I re-verified,
and it does not hold:

⛔ **X15: "`OutfitConfig::Init()` is compiled but called by nothing."** It **is**
called — `src/system/bandobj/Band.cpp:114`, inside `BandInit()`'s registration
list. X17 inherited this claim and said so; it is wrong as stated.

The accurate statement:

- `OutfitConfig::Init()` (`bandobj/OutfitConfig.cpp:404-409`) is called from
  `BandInit()` (`Band.cpp:114`) in shipped code.
- The **native driver never calls `BandInit()`** — it registers factories itself
  in `native/src/milo_object_factories.cpp`, whose block at `:467` (X9's comment)
  **deliberately omits** `OutfitConfig`.
- `Init()` is **not** a pure registration call: it is `Register()` **plus**
  `Hmx::Object::New<RndMat>()`, `New<RndCam>()` and `New<BandCharDesc>()` into
  three statics. That dependency — not a missing call — is why it cannot simply be
  pasted into the native block.

⚠ **The 191/1511/48-symbol bill is still NOT re-verified** — by X17 or by me.
Nobody should quote it as measured.

The band renders untextured pink in this lane's frames, unchanged and still
**pre-existing**.

---

## 7. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Is the recompose gate over-reporting?** | ★★★ **YES — 123/123 deviating bones are `PUBLISHED`; 4351 `COMPOSED` + 2793 `LOADED` deviate zero** | §1 |
| **Who publishes them?** | ★★★ **NAMED — `CharHair::SimulateZeroTime()` ×120, `CharIKHand::Poll()` ×2, `CharIKScale::Poll()` ×1** | §2 |
| **X17's three attachment roots** | ★★★ **DISSOLVED — they are the roots of the three simulated chains, not defects** | §2.1 |
| **X17's "~57 ROOT sites to attack"** | ⛔ **THERE IS NOTHING TO ATTACK — correct engine output** | §2.1 |
| **Instrument non-vacuity** | ★★★ **THREE controls; C3 makes the corrected gate FAIL on demand** | §5 |
| **Corrected gate scope** | ⚠ **NARROW — asserts only that composed bones compose; blind to whether a published world is right** | §1.3 |
| **Is the polled POSE correct?** | ⚠ **UN-INVALIDATED, NOT VALIDATED — the algebraic oracle can no longer speak against it; no geometric oracle affirms it** | §1.3 |
| **X14's driver-side call retired?** | ⚠ **UNBLOCKED BUT NOT RETIRED — conditional on `Poll()` rebinding at FULL scope; I did not make that change** | §4.1 |
| **The real retirement blocker** | ★★★ **THE REBIND SCOPE, not the pose — three lanes held for an artifact** | §4 |
| **Band skeleton sharing** | ★★★ **FOUND — 4 members + 4 outfits share one `bone_L-hand`; 7380 slots = 3306 objects, 123 = 63** | §3 |
| **Is the sharing real or a lookup artifact?** | ⚠ **UNRESOLVED — instrument committed, not run to conclusion** | §3 |
| **X17's `6.172e+01` / `3.565e+00` / 7380 / 54 / 12** | ✅ **ALL REPRODUCED EXACTLY** | §1 |
| **X15's "`OutfitConfig::Init()` called by nothing"** | ⛔ **REFUTED — called from `Band.cpp:114`** | §6 |
| **X15's 191/1511/48 bill** | ⚠ **STILL UNVERIFIED by anyone** | §6 |
| **Textures** | ⛔ **UNREACHED — sixth lane** | §6 |
| **X360 blast radius** | ✅ **ZERO IN EMITTED CODE — verified by rebuild, not by construction** | §7.1 |
| **Frames** | ✅ **NO REGRESSION — opened; venue, crowd, band intact; no shards, no hair explosion** | §8 |

### 7.1 X360 faithfulness — measured, and `cmp` lies here

I edited shared `src/` (`Trans.h`, `Trans.cpp`), so blast radius is a
**measurement**, not a construction argument. Both objects built in **this**
worktree:

| TU | `.text` | `.data` / `.rdata` / `.bss` | differing |
|---|---|---|---|
| `Trans.obj` | **389 COMDATs, byte-identical** | byte-identical | `.debug` only |
| `TransAnim.obj` | byte-identical | byte-identical | `.debug` only |
| `TransProxy.obj` | byte-identical | byte-identical | `.debug` only |

⚠ **A naive whole-file `cmp` says `DIFFERS`** — 143,672 differing bytes, 8-byte
size delta — and would read as a regression. The difference is entirely the
`.debug` section's file/line records, shifted because I added lines to
`Trans.cpp`. ★ **Controlled before being believed**: the unmodified source was
rebuilt twice and produced a **byte-identical** object, proving the `.obj` *is*
byte-reproducible and that the section-level result is real rather than build
noise.

⚠ `native/src/main_render.cpp` **cannot be scored at all** — it is native-only,
not in `objdiff.json`, and never has been.

---

## 8. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, 0 warnings, **0 SKIP lines**. All 18 binaries deleted first and **relinked this run** | `x18-gate.log` |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — branch-point gate passed before any edit | `x18-gate-baseline.log` |
| b2 | Four cache flags seeded | ⚠ **REQUIRED, AND THE DEFAULTS DO NOT WORK IN A WORKTREE.** The baseline gate **SKIPPED 3 targets** (incl. `rb3-render`) because `native/../../milo-native-engine` resolves under `/home/free/tmp/`. Configure with absolute `MILO_ENGINE_PATH`, `Dawn_DIR`, `glfw3_DIR`, `RB3X_BUILD_ENGINE=ON`. **No predecessor doc states this** | §8.1 |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606` unmoved. The foreign uncommitted `src/platform/FxSendNative.cpp` edit is disclosed and untouched — **fifteenth lane** | verified |
| d | Shared-`src/` X360 faithfulness | ✅ **MEASURED, not constructed** — 3 TUs, all code sections byte-identical, `.debug` only | §7.1 |
| e | PNG determinism ×2 | ✅ **PASS** — `base`, `poll`, `only` each `cmp`-identical across two full runs; the three differ from each other, so the set is not vacuous | §4 |
| f | Prior evidence non-regressed vs **artifacts** | ✅ **PASS** — my `base` frame is **BYTE-IDENTICAL** to X17's `x17-A-default-club.png`. X17's `6.172e+01`/`bone_mic_stand_bottom`, `3.565e+00`/`bone_pelvis` and the 7380/54/12 denominators all reproduce exactly | §1 |
| g | `RB3_BAND_PLACE=1` present | ✅ **PASS** — every cited run carries it; denominators 7380 bones / 12 hand meshes confirm the band was placed | §1 |

### 8.1 Frames — opened

`x18-A-base-club.png`, `x18-B-poll-club.png`, `x18-C-only-collapsed.png`,
`x18-D-onlyfull-club.png`. **Opened D and C.**

- **D (`only` + FULL):** venue lit and intact, 180-draw crowd on the balcony,
  **four band members at four distinct stage positions** in the known untextured
  pink. No shards, no missing geometry; X14's 7–14× hair explosion does not recur.
- **C (`only`, torso scope):** the four figures are **visibly shrunken and
  clustered** toward the origin. ★ The collapse is a *picture*, not a number —
  and it is the finding that reframes the whole retirement question.

⚠ Carried forward: read the gate's own verdict line, not the pipeline exit code
(`grep -c SKIP` exits 1 on zero matches, so the failure code *is* the 0-SKIPs
result). `--frames` is pinned at 1 for every cross-arm comparison here.

---

## 9. Retracted / corrected

⛔ **X15's `OutfitConfig::Init()` claim** — refuted, §6. Inherited by X17.

⛔ **X17's "~57 ROOT attachment sites are the thing to attack"** — refuted, §2.1.
They are correct engine output. X17's *decomposition* was right and useful; its
*handoff instruction* was wrong.

⚠ **X17 §4.1's "the ROOTs are frozen at bind-space coordinates"** — explained
rather than refuted. The sample was taken in the **no-Poll** arm, where the sim
has not run. Under Poll the published worlds are figure-local (§2.1).

⛔ **Mine, mid-lane: `cmp` on the X360 object.** I read "DIFFERS" as a possible
blast-radius regression before controlling for reproducibility. Section-level
comparison shows every code section byte-identical. §7.1

⛔ **Mine, mid-lane: the PIE base.** Resolving publishers against the first
`/proc/self/maps` line produced `??` for all three. §2

⚠ **Mine, mid-lane: the C2 magnitude.** Predicted `P`, measured `8P`. Not waved
through — chasing it produced §3.

---

## 10. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Make `Poll()`'s rebind run at FULL scope, then retire X14's call** | §4: the only remaining precondition. `only + FULL` is byte-identical to the arm that keeps the call; the shipped torso scope collapses the band to the origin. Needs its own X360 A/B on `BandCharacter.cpp:849` | X19 |
| ★★★ **Settle whether the band genuinely shares one skeleton** | §3: 4 members + 4 outfits resolve `bone_L-hand` to one object. Either four members share a skeleton (and per-figure numbers must be re-read as slots) or `FindBoneNamed` picks the first of several. The per-name count is already printed; it needs one run and a read | X19 |
| ★★★ **A GEOMETRIC oracle for the published pose** | §1.3: the corrected gate is algebraic and cannot say whether an IK/hair-published world is *right*. The hand-mesh gap gates are the existing seam | X19 |
| ★★ **Do not re-attack the 123 / the 57 ROOTs** | §2.1: correct output of `CharHair` and the two IK solvers | — |
| ★★ **`OutfitConfig` registration** | §6: the blocker is `Init()`'s three static `New<>` calls, not a missing call. The 191/1511/48 bill remains unverified by **anyone** | its own lane |
| ⚠ **Configure worktree native builds with four ABSOLUTE cache flags** | §8 gate b2: defaults silently SKIP `rb3-render` — the binary every pose lane needs | as before |
| ⚠ **`RB3_BAND_PLACE=1` required; pin `--frames`** | carried from X17, both held this lane | as before |
| ⚠ **The 12 `ObjOwnerPtr` sites, `SEEDED_NO_REPL`, `ObjPtrList` NULL-entry, `CharMeshHide::HideAll`, orphans, `BandCamShot`** | carried from X16/X17, untouched | as before |
| ⚠ **Engine CR: none filed** | this lane needed no engine change | — |

---

## 11. Recommended X19 shape

1. ★★★ **An unproven doubt is worth one lane of instrumentation.** X17 named this
   exact doubt and refused to act on it — correctly, because acting on it would
   have meant *adopting* an unvalidated pose. But the doubt was cheap to
   **settle**, and settling it invalidated three lanes of blocking. Refusing to
   act is right; refusing to measure is not.
2. ★★★ **When a gate reports a residual, ask what the gate is structurally
   capable of reporting.** `W == L·parentW` over a *cached* world can only ever
   fire on publication or on dirty-propagation failure. That was deducible from
   `Trans.h:118` before any run, and it bounds the answer before you measure.
3. ★★★ **Chase the control's missed prediction.** C2 was off by 8×. The factor was
   the biggest structural finding of the lane (§3). A control that misses is
   data, not an inconvenience.
4. ★★ **Name the mechanism, don't infer it from names.** "Hair and trouser bones"
   supported four lanes of narrative. One captured return address turned it into
   `CharHair::SimulateZeroTime()`.
5. ★★ **`cmp` is not a blast-radius instrument** for objects carrying debug info.
   Control for reproducibility, then compare sections. §7.1
6. ★ **Two blockers can wear the same clothes.** Retirement was blocked by the
   pose gate *and* by the rebind scope; only the second was real, and it was
   visible in a picture the whole time.

---

## 12. Evidence

All under `/home/free/tmp/laneX18/evidence/`.

| file | what it shows |
|---|---|
| `x18-A-default.log` / `x18-B-poll.log` | the writer table: 123/123 deviating are `PUBLISHED`; 4351 `COMPOSED` deviate 0 — §1 |
| `x18-dump-publisher.log` | full deviating set with writer tags, parents and `pub=` return addresses — §1, §2 |
| `x18-c1-perturb-local.log` | C1 — `SetLocalXfm`, stays `COMPOSED`, both gates PASS — §5 |
| `x18-c2-publish-world.log`, `x18-c2-lin{1,2,5,10}.log` | C2 — `SetWorldXfm`, old gate FAILS; perfect 8× linearity — §5, §3 |
| `x18-c3-stale.log` | **C3 — the corrected gate FAILS at `5.600e+01` on a forged stale bone** — §5 |
| `x18-identity.log` | the 8 band entries share one `bone_L-hand` pointer; crowd figures do not — §3 |
| `x18-arm-{base,poll,only}-r{1,2}.log` | the three retirement arms, determinism ×2 — §4 |
| `x18-arm-onlyfull.log` | `only` + FULL scope — band back on four marks — §4 |
| `x18-A-base-club.png` | **byte-identical to X17's `x17-A-default-club.png`** |
| `x18-C-only-collapsed.png` | **opened** — the band visibly collapsed to the origin — §8.1 |
| `x18-D-onlyfull-club.png` | **opened** — four members on four marks; `cmp`-identical to the `poll` arm |
| `Trans.{mod,orig,orig2}.obj`, `TransAnim.*`, `TransProxy.*` | X360 section comparison + the reproducibility control — §7.1 |
| `x18-gate.log` / `x18-gate-baseline.log` | native gate PASS 18/18 fresh, 0 SKIPs; branch-point health |
