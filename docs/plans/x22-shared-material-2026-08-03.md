# X22 — the shared `char_shared.milo` material is real, is fixed, and is NOT why the band is pink

**Date:** 2026-08-03
**Predecessor:** [X21](x21-compose-path-2026-08-03.md) "the compose pass never dispatches"
**Branch:** `x22-shared-material`, from `main` @ `a46979fb`, rebased onto `495364cd`
**Engine:** `milo-native-engine` pinned at **`138e1606`**, **zero engine edits**
**Change surface:** `native/src/main_render.cpp` (native-only) + **three shared `src/` TUs**, every edit inside `#ifdef HX_NATIVE`: `src/system/bandobj/BandCharacter.{cpp,h}`, `src/system/bandobj/OutfitConfig.cpp`.

---

## Verdict

⛔ **MY CHARTER'S CENTRAL PREMISE IS REFUTED BY MEASUREMENT.** It stated the
shared material "is the reason the band is pink independent of everything else"
and "will leave the visible meshes pink even after [the compose CRs] land". The
opposite is true: the shared-material defect is real and is now closed, and it
moves the band's pinkness from **2670 px to 2615 px** (8.0% → 7.9%). **It was
never why the band is pink.** §3

★★★ **THE BAND IS PINK BECAUSE OF THE *CLOTHING* MATERIALS, WHICH NO CENSUS ON
THIS LADDER HAS EVER LOOKED AT.** Five lanes have censused the five *skin*
materials and called the result "why the band is pink". A clothed member is
mostly cloth: `torso_militaryjacket.mat`, `trackjacket_resource.mat`,
`torso_shred.mat`, `torso_plaidshirt.mat`, `legs_grungepants.mat`,
`tiightjeans_resource.mat` — 1150–1778 verts each, all on `dummy_*.tex`, and all
invisible to a filter keyed on the five skin names. §4

★★★ **THE PINK IS MEASURED, NOT INFERRED — `dummy_torso/legs/feet/guitar.tex` ARE
8×8 DXT MAGENTA.** Block-0 endpoints `rgb(246,222,238)` / `rgb(189,28,238)`, all
four identical. This was on the handoff list since X19 and never done; without it
the last link of the chain was unmeasured. §5

★★★ **THE MISSING OPERATION IS `MatSwap::SwapResource()`, AND IT IS SHIPPED CODE
WHOSE ONLY CALLER IS THE FUNCTION X21 MEASURED AT 0 CALLS.**
`OutfitConfig::DrawPreClear()` calls `SwapResource()` at `OutfitConfig.cpp:1088`
and `Compose()` at `:1097` — **eleven lines apart in the same function**. §6

★★★ **DISPATCHING THE SWAP ALONE REMOVES THE PINK: 2670 px → 80 px (a 97%
reduction).** And it leaves the clothing **dark** (dark pixels 370 → 1035),
because the swap materials point at `*_output.tex` render targets that
`Compose()` never paints. **So the answer to the charter's question is YES — the
compose pass IS required — and the two halves are separable.** §6.2

✅ **Milestone 1's defect closed and MEASURED.** 20/20 per-member replacement
materials exist and are owned by the member; 18 showing meshes repointed, every
one `reach==1` by pointer; frame delta **1048 px, entirely inside the band's
bbox, 0 px outside it**. §3

★★ **THE FIRST TWO TESTS I WROTE FOR "IS THIS MESH THE MEMBER'S OWN" WERE BOTH
WRONG AND BOTH WOULD HAVE PRODUCED A CONFIDENT WRONG ANSWER.** `mesh->Dir() == bc`
said 0 of 172 were repointable; the dir's *name* is the sixth-time-rule trap.
Only reach-count by pointer is the question actually being asked. §2.2

✅ **Gate PASS 18/18 fresh, rc=0, 0 SKIPs**, before and after. `main` was **not**
broken by a decomp lane. §8

⛔ **MILESTONE 2 (the 120 shared-skeleton publications) NOT ADVANCED — fourth
consecutive lane.** Dead-ends doc read first, as instructed; none of the four
retried, no fifth proposed. §7

---

## 1. What the shared material turned out to be

| | |
|---|---|
| **objects** | `torso_naked.mat`, `legs_skin.mat`, `feet_socks_skin.mat`, `head_naked.mat` — 4 distinct `RndMat`s owned by `ObjectDir (char/main/shared/char_shared.milo)` |
| **their diffuse** | `dummy_torso.tex` / `dummy_legs.tex` / `dummy_feet.tex`, and **NULL** for `head_naked.mat` |
| **is the sharing authored?** | **The sharing is authored. The PERSISTENCE of it is the defect.** |

`char_shared.milo` genuinely ships one material per skin slot — that is what the
file is for. Retail's design is **author once, un-share per member at merge**,
and the un-sharing is shipped code in `BandCharacter::Filter`
(`src/system/bandobj/BandCharacter.cpp:2519-2523`):

```cpp
if (o1->Dir() == sCharSharedDir) {
    Hmx::Object *mine = Find<Hmx::Object>(o1->Name(), true);
    MILO_ASSERT(mine->Dir() == this, 0xAB8);
    ::ReplaceRefs(o1, mine);
    return kIgnore;
}
```

⇒ **a resolution defect, not authored sharing.** That arm only runs if
char_shared's objects are **walked** by the merge, and the native `FilterSubdir`
shim (`:2586-2635`, added to fix "char textures rendering white") converts
`kMergeMerge → kMergeReplace` for every subdir that is its own on-disk milo,
deliberately keeping char_shared as a shared **reference**. So `Filter` never
sees those objects and `ReplaceRefs` never runs. **The port kept the authoring
and lost the un-sharing.**

★ **This is X19's shared-object shape confirmed, not merely asserted.** X21
called it "the same shape as `FindBoneNamed`"; the mechanism is now located:
a shipped un-sharing step that a native shim prevents from being reached.

### 1.1 ⚠ A supporting fact worth its own line

`ObjectDir::FindObject` (`src/system/obj/Dir.cpp:1011-1017`) **always descends
into `mSubDirs`, regardless of the `parentDirs` argument.** That is how
`sCharSharedDir` gets discovered at all (`OnInstallFilter` resolves
`feet_skin.mat` through the outfit dir's subdir chain, `:2692-2695`) and how the
outfit meshes resolved onto char_shared's materials at load. The member's *own*
entry table is consulted **first**, which is why `SetSkinTextures` binds
correctly and only the draw-time references are stale.

---

## 2. Milestone 1 — the fix, and the two tests that would have sunk it

### 2.1 Is there anywhere to point?

Retail `MILO_ASSERT`s `mine->Dir() == this`, so it **requires** the member to
already own a same-named object. If ours did not, a repoint would have no target
and the only remaining move would be to **clone** a material — i.e. to invent
one, which this ladder does not do. So that was measured first.

| member | replacement materials owned | missing |
|---|---|---|
| player0 / player1 / player2 / player3 | **5 each — all `OWNED-BY-MEMBER`** | **0** |

**20/20 present.** Retail's assert would hold.

### 2.2 ★★ Which meshes may be repointed — two wrong tests first

| test | answer it gives | why it is wrong |
|---|---|---|
| `mesh->Dir() == bc` | **0 of 172** are the member's own | an outfit mesh legitimately lives in the **outfit milo's** dir, not the member's |
| the dir's **name** | — | names are not identities; **sixth time** on this ladder |
| ★ **reach-count by pointer** across all four members | **18 showing meshes, all `reach==1`** | this is the question actually being asked |

`CollectDeep(bc)` descends into `bc`'s subdirs, and the shared
char_shared/colorpalettes dirs **are** subdirs of every member — that is the
whole shim. So that vector contains meshes **the other three members also see**.
Repointing one of those per member would cross-wire the band, last writer
winning: **the same shared-object trap this lane exists to close, re-entered from
the other side.**

**MEASURED:** the 18 showing body meshes are all `reach==1`, each in its own
outfit/resource milo. **0 shared-dir meshes are showing.** The 37–40
foreign-material meshes per member that are *not* repointable are all
non-showing.

### 2.3 The fix

`BandCharacter::RebindSharedSkinMatsToOwn()`, called from `Poll()` beside the
existing `RebindOutfitBonesToOwnSkeleton()`.

- **Not the global `::ReplaceRefs` (`:2408`)** — it walks the **global** ref
  ring, which carries all four members, so it would repoint the whole band onto
  whichever member ran last. Retail survives that only because each member's
  merge is atomic and ordered; the native loader interleaves, which is *why the
  shim exists*. Per-mesh `SetMat` instead: same consequence, per-member scope.
- **Not invented.** `OutfitConfig::SetSkinTextures`' own tail (`:608-621`)
  already does exactly this for three meshes —
  `torsomesh->SetMat(dir1->Find<RndMat>("torso_naked.mat", false))` — which is
  precisely why the tattoo meshes are the only correctly-bound ones today. This
  applies the identical shipped operation to the member's remaining skin meshes.
- **Full shim-OFF NOT retried.** A proven dead end, built and measured twice
  (`docs/CHAR_SKINNING_DEFORM_INVESTIGATION.md`): white-texture drain, *and* it
  does not fix the skeleton share either.

**Result:** 18 meshes repointed; census **11 → 14 distinct `RndMat`**, NULL
diffuses **4 → 0**, textured meshes **347 → 351**.

---

## 3. ⛔ And it does not make the band textured

| quantity (band region, 130×260 px) | baseline | after the repoint |
|---|---|---|
| pink pixels | **2670 (8.0%)** | **2615 (7.9%)** |
| dark pixels | 370 | 438 |
| frame delta vs X21's artifact | — | **1048 px, bbox y438-555 x722-959, 0 px outside** |

★ **A quantified non-identity confined to the band's own bounding box** — venue
and crowd byte-identical. The fix reached the band and only the band. It simply
is not the thing making it pink.

⚠ **My charter said this defect "will leave the visible meshes pink even after
[the CRs] land" and that it "can actually change the frame".** Both halves are
wrong, and in opposite directions: it barely changes the frame, and the compose
work is exactly what the pink is waiting on.

---

## 4. ★★★ The band is pink because of the CLOTHING materials

The X19 skin census filters on
`strstr(mat->Name(), {torso_naked, legs_skin, feet_skin, feet_socks_skin, head_naked})`.
**By construction it can never see a cloth material.** Censusing every *showing*
mesh with geometry, with no name filter:

| member | mesh | verts | material | diffuse |
|---|---|---|---|---|
| 0 | `vestdenim_resource.mesh` | 1150 | `torso_militaryjacket.mat` | **dummy_torso.tex** |
| 0 | `tightdistressedpants_resource.mesh` | 1739 | `tightdistressed_jeans.mat` | **dummy_legs.tex** |
| 1 | `trackjacket_resource.mesh` | 1732 | `trackjacket_resource.mat` | **dummy_torso.tex** |
| 1 | `saddleshoe_resource.mesh` | 2269 | `feet_saddleshoe.mat` | **dummy_feet.tex** |
| 2 | `shred_resource.mesh` | 1778 | `torso_shred.mat` | **dummy_torso.tex** |
| 2 | `grungepants_resource.mesh` | 1288 | `legs_grungepants.mat` | **dummy_legs.tex** |
| 2 | `nailboots_resource.mesh` | 2122 | `feet_nailboots.mat` | **NULL** |
| 3 | `plaidshirt_resource.mesh` | 1353 | `torso_plaidshirt.mat` | **dummy_torso.tex** |
| 3 | `tightjeans_resource.mesh` | 1298 | `tiightjeans_resource.mat` | **dummy_legs.tex** |
| 3 | `51squier_resource.mesh` | 1810 | `guitar_51squier_base.mat` | **dummy_guitar.tex** |

These are the **highest-vertex meshes on the band** — the visible body. The skin
meshes I fixed are the small exposed bits (`head.mesh` 2592, `hands_naked.mesh`
1876, and `*_skin.N` seam meshes of 15–869 verts).

★ **Same failure shape as the retired "58 skin material instances": a number
measured over the wrong set, read as though it covered the frame.** Five lanes
inherited "the skin materials" as a synonym for "why the band is pink". The two
were never the same question and nobody checked.

---

## 5. ★★★ The pink, measured

`dummy_torso.tex`, `dummy_legs.tex`, `dummy_feet.tex`, `dummy_guitar.tex` —
**all four identical**: `8x8`, `bpp=4`, `order=0x8` (DXT), block-0 RGB565
endpoints **`rgb(246,222,238)`** and **`rgb(189,28,238)`**. Magenta.

★ Carried on the handoff list since X19 and never done. Without it, "cloth
material → dummy texture → pink pixels" had an unmeasured last link, and a lane
could identify the binding correctly and still be wrong about the colour.

⚠ **My first pinkness predicate returned 0 on the baseline too** — it tested for
saturated magenta, and after DXT interpolation plus venue lighting the on-screen
result is a *pale* pink. A failure-only predicate that never fires proves
nothing; the corrected predicate (`R−G > 12` and `B−G > 8` over lit pixels) is
what produced §3's and §6.2's numbers.

---

## 6. ★★★ The missing operation is `MatSwap::SwapResource()`

`OutfitConfig::MatSwap` holds **two** material pointers — `mMat` (`:44`) and
`mResourceMat` (`:45`) — and the class is named for swapping between them.

| | material | owner | diffuse |
|---|---|---|---|
| `mMat` (compose target) | `torso_militaryjacket_swap.mat` | `outfit (char/main/outfit.milo)` | `militaryjacket_canvas_diffuse_output.tex` |
| **what the mesh actually draws** | `torso_militaryjacket.mat` | `vestdenim_resource.milo` | **`dummy_torso.tex`** |

Censused: **40 OutfitConfigs, 35 distinct MatSwap target materials**, all
`*_swap.mat` in `outfit.milo`, all already pointing at their `*_output.tex`
render targets. The meshes draw the **non-swap resource** materials.

`OutfitConfig::MatSwap::SwapResource()` (`OutfitConfig.cpp:60-88`) is the shipped
function that repoints every `Mesh`-owned reference from `mResourceMat` to
`mMat`. **Its only caller is `OutfitConfig::DrawPreClear()`, at `:1088`.**

### 6.1 ⇒ the complete, measured causal chain

1. `Rnd::DrawPreClear` selects the wrong list — arms swapped vs the rb3-Wii
   oracle (X21 §3.2, `rndobj/Rnd.cpp:1274`)
2. ⇒ `OutfitConfig::DrawPreClear()` **0 calls** (X21 §3.1, against a 734-line
   positive control)
3. ⇒ `MatSwap::SwapResource()` never runs ⇒ the outfit meshes keep drawing the
   **resource** materials
4. ⇒ whose diffuse is `dummy_torso/legs/feet.tex`
5. ⇒ which are 8×8 DXT **magenta** (§5)
6. ⇒ **the band is pink.**

`MatSwap::Compose()` — which paints the `*_output.tex` RTs the swap materials
point at — is at `:1097`, **eleven lines below `SwapResource()` in the same
function**. One dispatch gates both.

### 6.2 ★★★ Dispatching the swap ALONE — and why that matters

The two halves have different owners: the swap is pure consumer-side object
plumbing; the composite needs a backend that can paint render targets (X21 §5.2:
absent from the dc3 flavor). From the outside they are indistinguishable.
Separating them:

| quantity (band region) | baseline | skin-mat repoint | **+`SwapResource()`** |
|---|---|---|---|
| pink pixels | 2670 (8.0%) | 2615 (7.9%) | **80 (0.2%)** |
| dark pixels | 370 | 438 | **1035** |
| run rc | 0 | 0 | **0** (no crash, no black frame) |

⇒ **The swap alone removes 97% of the pink** — and leaves the clothing **dark**,
because the RTs it now points at are unpainted. **So the compose is required for
the band to be *textured*, but is NOT required to stop it being *pink*.**

★ Unlike X21's polarity arm, this one does **not** kill the frame: coverage
stays 38.92%, rc=0. The pass-nesting violation X21 hit comes from the *compose*
half (which opens render passes), not the swap half.

⚠ **Currently a harness-only diagnostic** (`RB3_X22_SWAP_RESOURCE=1` in
`native/src/main_render.cpp`) — it calls one half of a function whose halves are
ordered and skips the `PreRender`/patch work between them. **It is evidence, not
a shippable fix.** Its faithful home is the repaired `DrawPreClear` dispatch.

---

## 7. ⛔ Milestone 2 — the 120 shared-skeleton publications: NOT ADVANCED

Read `docs/CHAR_SKINNING_DEFORM_INVESTIGATION.md` and its four proven dead ends
first, as instructed. **I attempted none of them and propose no fifth.** Nothing
was measured; X19 §4's structural result stands exactly as it was. **Fourth
consecutive lane** to spend itself on the texture chain — the honest accounting,
though this one at least ends by relocating the defect rather than confirming it.

★ One observation, offered as an **unmeasured** hypothesis and explicitly not as
evidence: X21 suggested the material and skeleton defects share a root
(`FilterSubdir`). For the **material** case that is now confirmed — but the
repair did **not** require touching the shim; a per-member repoint sufficed. The
skeleton's analogue of that repoint is `RebindOutfitBonesToOwnSkeleton()`, which
**already ships**. So if the shapes really are the same, the skeleton case has
already had this class of fix applied, and the 120 residuals — `CharHair::
SimulateZeroTime()` and IK publications onto the shared unplaced skeleton — are
**downstream of a different mechanism**, not of mesh/material binding. Untested.

---

## 8. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **What is the shared material?** | ✅ **4 distinct `RndMat` in `char_shared.milo` on `dummy_*.tex` + one NULL** | §1 |
| **Authored sharing, or resolution defect?** | ★★★ **The SHARING is authored; the PERSISTENCE is a resolution defect.** Retail's `Filter` un-shares per member (`:2519`); the `FilterSubdir` shim (`:2586`) stops that arm being reached | §1 |
| **Is there a repoint target?** | ✅ **20/20 owned by the member, 0 missing** — retail's `MILO_ASSERT` would hold | §2.1 |
| **Which meshes are repointable?** | ★★ **18, all `reach==1` by pointer. Two earlier tests gave 0 and would have sunk the lane** | §2.2 |
| **Is the shared-material defect fixed?** | ✅ **YES — 18 meshes repointed, 11→14 distinct mats, 4→0 NULL diffuses** | §2.3 |
| **Are the band's visible meshes textured?** | ⛔ **NO. ACCEPTANCE NOT MET** | §3, §9.1 |
| **Did fixing it change the pink?** | ⛔ **ESSENTIALLY NO — 2670 px → 2615 px. THE CHARTER'S PREMISE IS REFUTED** | §3 |
| **Then why IS the band pink?** | ★★★ **The CLOTHING materials — a set no census on this ladder has looked at, invisible to the five-skin-name filter by construction** | §4 |
| **Is the pink measured or inferred?** | ★★★ **MEASURED — `dummy_*.tex` are 8×8 DXT, endpoints rgb(246,222,238)/rgb(189,28,238)** | §5 |
| **What is the missing operation?** | ★★★ **`MatSwap::SwapResource()`, shipped, only caller `OutfitConfig::DrawPreClear():1088` — X21's 0-call function** | §6 |
| **Does the fix require the compose pass?** | ★★★ **YES for TEXTURED; NO for NOT-PINK. The swap and the composite are separable and have different owners** | §6.2 |
| **Swap alone** | ★★★ **pink 2670→80 px (97%), clothing goes DARK, rc=0, frame survives** | §6.2 |
| **The 120 shared-skeleton publications** | ⛔ **NOT ADVANCED — no fifth attempt proposed** | §7 |
| **X360 blast radius** | ✅ **ZERO BY CONSTRUCTION and CHECKED** — 0 unguarded added lines outside `#ifdef HX_NATIVE` (mechanical check); `HX_NATIVE` undefined for X360 | §8.1 |
| **objdiff position of touched TUs** | ✅ **`default/BandCharacter` 508/603, `default/OutfitConfig` 182/232 — both IDENTICAL to X21's**, both objects built in this worktree. `native/src/main_render.cpp` **cannot be scored at all** (not in `objdiff.json`) | §8.1 |
| **Frames** | ✅ **determinism ×2 identical; delta vs X21 artifact = 1048 px, 0 px outside the band bbox** | §9 |

### 8.1 X360 faithfulness

Every added line in the three shared TUs is inside `#ifdef HX_NATIVE`, verified
**mechanically** (a preprocessor-stack walk over the diff's added line numbers:
**0 unguarded**; the single flagged line is the closing `#endif` itself), and
`HX_NATIVE` is not defined for the X360 build. Corroborated at unit granularity
with **both objects built in this worktree**.

⚠ **`OutfitConfig::DrawPreClear` / `MatSwap::SwapResource` were NOT edited**, so
nothing in §6 rests on an unscoreable change. The one scoring gap this lane
inherits is X21's: `Rnd::DrawPreClear` has `target_size=0` and **cannot be scored
at all**.

---

## 9. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, 0 warnings, **0 SKIP lines**; binaries deleted first, rebuild awaited before probing | `x22-gate-final.log` |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — branch-point gate PASS 18/18, 0 SKIPs, before any edit. Rebased onto `495364cd` and re-gated | `x22-gate-baseline.log` |
| b2 | Six cache flags seeded | ✅ **PASS** — X21's corrected recipe used verbatim; **0** occurrences of "cache to be deleted" | `x22-configure.log` |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606` unmoved; engine HEAD *is* `138e1606`. The foreign uncommitted `src/platform/FxSendNative.cpp` edit is disclosed and untouched — **nineteenth lane** | verified |
| d | Shared-`src/` X360 faithfulness | ✅ **PASS, MEASURED** — see §8.1 | §8.1 |
| e | PNG determinism ×2 | ✅ **PASS** — md5 `24c701b5…` on two full runs after the rebase | §9.1 |
| f | Prior evidence non-regressed vs **artifacts** | ✅ **PASS, QUANTIFIED** — 1048 px differ vs X21's `x21-A-default-club.png`, bbox y438-555 x722-959; **0 px differ outside it** (venue + crowd byte-identical) | §3 |
| g | `RB3_BAND_PLACE=1` present | ✅ **PASS** — every cited run carries it; the 4-member / 20-target denominators confirm the band was placed | §2.1 |

### 9.1 Frames — opened

**`x22-A-skinmatfix-club.png`** (md5 `24c701b5…`, the default arm). **Opened it.**
The small-club interior in cutaway, fully lit and textured — plank floors, brick
and plaster walls with a green-panelled section, the bar, the stacked wooden
chairs, staircase and metal rails. A **textured crowd** of ~20 figures lines the
upper balcony in jeans and pale shirts. On the stage floor, **four figures still
in flat pale pink**, upright on their marks, heads present, no shards, holding
instruments. **The band is still pink.** The only visible change from X21's
artifact is a small brown patch on the heads where `head_naked.mat`'s diffuse was
NULL and is now bound.

**`x22-C-swapresource-club.png`** — the `RB3_X22_SWAP_RESOURCE=1` diagnostic arm.
**Opened it.** Same venue and crowd. On the stage, **the pink is gone**: the four
members now wear **dark grey and near-black clothing** with some surface
variation, pale heads and hands still visible. It is not textured clothing — it
is clothing drawing an unpainted render target — but it is unmistakably **not
pink**, and the frame is otherwise intact (coverage 38.92%, rc=0).

**`x22-B-authored-diffuse-club.png`** — the `RB3_X22_AUTHORED_DIFFUSE=1`
diagnostic. **Opened it.** Indistinguishable from the default arm; the band is
still pink. This is what **excluded** "the repoint didn't take effect" and
"the skin RTs are the blocker" as explanations, and sent the search to the
clothing.

---

## 10. Retracted / corrected

⛔ **My charter's "this defect is independent of both [CRs] and will leave the
visible meshes pink even after they land"** — **refuted**, §3. The visible meshes
are pink for an unrelated reason and will stop being pink the moment
`OutfitConfig::DrawPreClear` dispatches, whether or not this lane's fix exists.

⛔ **My charter's "this is the milestone that can actually change the frame"** —
refuted, §3. It changes 1048 px; the compose dispatch changes 2590.

⛔ **"The band is pink because of the skin materials"** (implicit in X19→X21) —
retired, §4. The skin materials are the small exposed bits; the pink is cloth.

⛔ **X21 §10's "`dummy_torso.tex` **is** what [the visible meshes] legitimately
draw today"** — half right. They do draw it, but *not* legitimately: they draw it
because `SwapResource()` never ran, §6.

⚠ **My own first two identity tests** (`mesh->Dir() == bc`, and dir names) —
both wrong, both recorded in the probe's comments rather than deleted, §2.2.

⚠ **My own first pinkness predicate** — returned 0 on the baseline, i.e. it
could not have detected the very thing it was written to measure, §5.

⚠ **Named before the run and NOT confirmed:** I expected the shared material to
be the whole remaining distance, per the charter. It was ~2% of it.

---

## 11. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Sequence the compose work — it is now the WHOLE critical path for the band** | §6. X21's CR-1/CR-2 plus the `Rnd::DrawPreClear` polarity fix. Nothing else stands between the band and correct clothing | coordinator |
| ★★★ **Land `SwapResource()` dispatch SEPARATELY and FIRST** | §6.2. It is consumer-side only, needs **no** engine change, does **not** kill the frame, and removes 97% of the pink on its own. The compose can follow | coordinator / next lane |
| ★★ **`MatSwap::Compose` is what makes it TEXTURED rather than dark** | §6.2. Expect dark clothing, not correct clothing, until CR-2 lands | coordinator |
| ★★ **Promote `RB3_X22_SWAP_RESOURCE` out of the harness** | §6.2. Currently calls one half of an ordered function and skips the `PreRender`/patch work between the halves. Its faithful home is the repaired `DrawPreClear` | next lane |
| ★★ **Re-run the X22 censuses after the compose lands** | §4, §6. `x22-showing-r1.log`'s table is the before-picture; every `dummy_*` row should become an `*_output.tex` row | post-CR |
| ★★★ **The 120 shared-skeleton publications** | §7: untouched for a fourth lane. Read the dead-ends doc first. ★ §7's unmeasured note argues they are downstream of a *different* mechanism than the material share | X23 |
| ★★ **`Rnd::DrawPreClear`'s retail arm** | X21 §3.3. **Cannot be scored** (`target_size=0`). Unchanged here | a scored lane |
| ★★ **Fix `FindBoneNamed` at its other call sites** | Carried from X19/X20/X21 **untouched** | X23 |
| ⚠ **`ReProject`/`PreRender` counted stubs** | still 0 in every X22 run | its own lane |
| ⚠ **Widen `RB3_SYNCPROP_LOCAL_STATIC`** | X20 §1.2, untouched | its own lane |
| ✅ **Sample `dummy_torso.tex`'s texels** | **DONE**, §5. Retire from the list | — |
| ⚠ **Geometric oracle with a reference pose** | carried, untouched | X23 |

---

## 12. Recommended X23 shape

1. ★★★ **A census's FILTER is part of its claim.** Five lanes filtered on five
   skin-material names and read the result as "why the band is pink". The filter
   made the answer unreachable by construction. **Before trusting a census, ask
   what it excludes.** §4
2. ★★★ **"Count identities, not names" has a sibling: COUNT THE RIGHT SET.**
   X21's 58-vs-11 was the right set counted wrongly. This was the wrong set
   counted correctly. Both read as facts about the frame. §4
3. ★★★ **When two operations sit in one function, dispatch them separately
   before pricing the repair.** `SwapResource` and `Compose` are eleven lines
   apart; one needs no engine work and removes the pink, the other needs a
   backend. Eleven lines of separation was worth a whole CR of sequencing. §6.2
4. ★★ **Measure the last link even when the chain is obvious.** "Cloth material →
   dummy texture → pink" was clearly right, and `dummy_*.tex` had gone
   three lanes unsampled. It took one probe. §5
5. ★★ **Write down the wrong tests.** Two of my three identity tests were wrong
   and each gave a clean, confident, false answer. §2.2
6. ★★ **A predicate that returns 0 on your control is broken, not informative** —
   my pinkness test fired 0 on the baseline. §5

---

## 13. Evidence

All under `/home/free/tmp/laneX22/evidence/`.

| file | what it shows |
|---|---|
| `x22-probe-r1.log` | ★★★ **20/20 repoint targets OWNED-BY-MEMBER, 0 missing** — §2.1 |
| `x22-probe-r2.log` | ⛔ the WRONG test (`mesh->Dir()==bc`) reporting 0 of 172 repointable — §2.2 |
| `x22-probe-r3.log` | ★★★ **reach-count by pointer: 18 showing meshes, all reach==1; 0 shared meshes showing** — §2.2 |
| `x22-fix-r1.log` | the repoint firing: 18 showing meshes, per-member, with before/after pointers — §2.3 |
| `x22-showing-r1.log` | ★★★ **the showing-mesh census with NO name filter — the clothing materials on `dummy_*.tex`** — §4 |
| `x22-tex-r1.log` | ★★★ **`dummy_*.tex` = 8×8 DXT, endpoints rgb(246,222,238)/rgb(189,28,238)** — §5 |
| `x22-matswap-r1.log` | ★★★ **35 MatSwap targets, all `*_swap.mat` on `*_output.tex`; the meshes draw the non-swap resource mats** — §6 |
| `x22-swap-r1.log` | ★★★ **`SwapResource()` on 44 swaps, rc=0, coverage 38.92%** — §6.2 |
| `x22-authored-r1.log` | the diagnostic that EXCLUDED the skin-RT explanation — §9.1 |
| `x22-base-r1.log` | baseline reproduction; PNG byte-identical to X21's artifact before any edit |
| `x22-final-r{1,2}.log` | determinism ×2 after the rebase |
| `x22-A-skinmatfix-club.png` | **opened** — default arm; band still pink — §9.1 |
| `x22-B-authored-diffuse-club.png` | **opened** — authored diffuse; still pink — §9.1 |
| `x22-C-swapresource-club.png` | ★★★ **opened — the pink is GONE, clothing dark** — §9.1 |
| `x22-gate-baseline.log` | branch-point health, PASS 18/18, 0 SKIPs |
| `x22-gate-final.log` | native gate PASS 18/18 fresh, 0 SKIPs, after the rebase |
| `x22-configure.log` | the six-flag configure, 0 "cache to be deleted" |
