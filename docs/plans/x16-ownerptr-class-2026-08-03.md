# X16 — the null is a class of 14, repaired upstream; and X15's cause for the bad pose is refuted

**Date:** 2026-08-03
**Predecessor:** [X15](x15-poll-unblock-2026-08-03.md) "`Poll()` runs, and the cause X14 gave for why it could not is refuted"
**Branch:** `x16-ownerptr-class`, from `main` @ `ad4d61d2`
**Engine:** `milo-native-engine` pinned at **`138e1606`**, **zero engine edits**
**Change surface:** three shared `src/` files (`obj/Object.h`, `obj/Object.cpp`, `obj/ObjPtr_p.h`) — every edited region inside `#ifdef HX_NATIVE` — plus one new tool (`tools/x16_ownerptr_census.py`). No native-driver edits.

---

## Verdict

★★★ **THE CLASS IS 14 SITES, NOT 2.** X15 found two by walking into them and
said "two is not a proof that there are only two". Enumerated: **14**
`ObjOwnerPtr` members seeded `(this, this)` whose invariant is restored *only* in
`Replace()`. X15 fixed 2; **12 were unrepaired**. §1

★★★ **THE UPSTREAM REPAIR LANDED, AND IT IS PROVABLY SCOPED.** One change in
`ObjOwnerPtr` fixes all 14 without firing a single consumer callback, so it has
**zero** exposure to the `delete this` hazard the `HX_NATIVE` shortcut exists to
dodge. X15's two site-guards are now redundant (kept — they are retail-equivalent
and harmless). §3

★★★ **PROVED BY A SET IDENTITY, NOT A SPOT CHECK.** In one binary via a
kill-switch: NULL owners → self **one-for-one** (7→0/+7, 17→0/+17, 7→0/+7) while
`other` is **invariant** — so nothing legitimately-foreign was stolen. The
vocalist (0 nulls) and the crowd are **bit-identical across arms**: the
in-measurement negative control. §4

⛔ **X15's NAMED CAUSE FOR THE BAD POSE IS REFUTED — CHEAPLY.** X15: "the
destroyed `CharWeightSetter`s are the blend-weight sources for the IK/MIDI
drivers, so IK is applied at the wrong strength." With every weight owner
restored (0 NULL, censused), the polled pose is **bit-identical** to the
unrepaired arm — same worst deviation to four figures, same worst bone. §5

⛔ **SO THE POSE IS STILL NOT VALIDATED, AND X14's DRIVER-SIDE CALL IS NOT
RETIRED.** Holding X15's line. §5.3

★★ **BUT THERE IS A MUCH BETTER LEAD, AND IT CROSS-REFERENCES X15's OWN
DATA.** The polled recompose residual is concentrated on **prop, trouser and
hair** bones — `bone_mic_stand_bottom`, `bone_legs-ring2`, `bone_legs_L03`,
`bone_hair_back01` — which is **the same set** as X15 §7.2's seven skipped meshes
with unresolved bones. The pose defect and the rebind-skip defect are plausibly
**one defect**. §6

⛔ **RETRACTED, MINE, MID-LANE: two measurement runs that were vacuous.** §7

✅ **X360 blast radius ZERO** — `.text` byte-identical across 6 TUs. **Gate PASS
18/18 fresh, 0 SKIPs.** `main` was **not** broken by a decomp lane. §9

---

## 1. ★★★ The enumeration — 14 sites, by an instrument with five controls

`tools/x16_ownerptr_census.py` (committed) classifies every `ObjOwnerPtr` member
in `src/`:

| verdict | n | meaning |
|---|---|---|
| **EXPOSED** | **14** | seeded `(this, this)` **and** restored only in `Replace()` — the defect class |
| SEEDED_NO_REPL | 2 | seeded `(this, this)`, no `Replace` restore — invariant is not restored on *either* arm, so not native-specific |
| NOT_SELF_SEEDED | 23 | an `ObjOwnerPtr` without the "null means me" invariant |

**The 14:**

| class::member | decl | ctor seed | restore in `Replace` |
|---|---|---|---|
| `CharWeightable::mWeightOwner` | `char/CharWeightable.h:80` | `CharWeightable.cpp:7` | `:12` ← X15 |
| `Character::mSphereBase` | `char/Character.h:226` | `Character.cpp:62` | `:76` ← X15 |
| `RndCamAnim::mKeysOwner` | `rndobj/CamAnim.h:46` | `CamAnim.cpp:9` | `:21` |
| `RndEnvAnim::mKeysOwner` | `rndobj/EnvAnim.h:55` | `EnvAnim.cpp:9` | `:29` |
| `RndEnviron::mAmbientFogOwner` | `rndobj/Env.h:133` | `Env.cpp:376` | `:104` |
| `RndFont::mTextureOwner` | `rndobj/Font.h:153` | `Font.cpp:159` | `:169` |
| `RndLight::mColorOwner` | `rndobj/Lit.h:73` | `Lit.cpp:142` | `:78` |
| `RndLightAnim::mKeysOwner` | `rndobj/LitAnim.h:44` | `LitAnim.cpp:17` | `:28` |
| `RndMatAnim::mKeysOwner` | `rndobj/MatAnim.h:68` | `MatAnim.cpp:20` | `:26` |
| `RndMesh::mGeomOwner` | `rndobj/Mesh.h:323` | `Mesh.cpp:125` | `:938` |
| `RndMeshAnim::mKeysOwner` | `rndobj/MeshAnim.h:55` | `MeshAnim.cpp:11` | `:20` |
| `RndParticleSysAnim::mKeysOwner` | `rndobj/PartAnim.h:49` | `PartAnim.cpp:44` | `:51` |
| `RndTransAnim::mKeysOwner` | `rndobj/TransAnim.h:165` | `TransAnim.cpp:11` | `:18` |
| `RndWind::mWindOwner` | `rndobj/Wind.h:54` | `Wind.cpp:57` | `:71` |
| `Spotlight::mColorOwner` | `world/Spotlight.h:184` | `Spotlight.cpp:144` | `:169` |

★ **`RndMesh::mGeomOwner` is the one to notice.** Geometry sharing runs through
it, and it is in the same teardown path as the rigging dirs.

### 1.1 ★★★ The controls, and the two bugs they caught in the instrument itself

The script **exits non-zero and refuses to print a trustworthy verdict** unless
4 positive + 1 negative control pass. It caught two real self-bugs:

1. ⛔ **Comment pollution.** v1 attributed `CharWeightable::mWeightOwner` to a
   class called `already`, because a **doc comment that quotes**
   `` `mWeightOwner(this, this)` `` matched the ctor-seed regex, and `class`
   inside prose was read as a class name. Both positive controls FAILED and the
   run aborted. Fixed by blanking comments/strings (offset-preserving) and doing
   real brace-depth class scoping.
2. ⛔ **A restore regex that was too narrow — 7 false negatives the original
   controls sailed past.** v2 looked only for `mX = this;`. The tree spells the
   restore at least four ways; `mX.SetObjConcrete(this)` and a local
   indirection (`replace = this; … mX = replace;`) were both missed, and both
   *original* controls happen to use `= this`, so they passed while the census
   under-reported by seven. Fixed, **and two new controls were added for the two
   missed spellings** (`RndEnvAnim::mKeysOwner`, `RndFont::mTextureOwner`) so the
   boundary cannot silently regress again.

★ This is the charter's point about instruments that cannot fail. The first
version would have reported "9 EXPOSED" with a green control block.

⚠ **Honest limit:** the screen is regex + brace matching, not a compiler
front-end. Every one of the 14 carries its evidence line and was eyeballed; a
site using a fifth restore spelling *inside a helper function* would still be
missed. The `SEEDED_NO_REPL` bucket (2) is the residual risk surface and is
small enough to read by hand.

---

## 2. ★★★ Why the `HX_NATIVE` shortcut exists — documented, and one framing correction

The charter asked to find out *why* before removing it. It is **documented, not
folklore**. The whole lineage is upstream in `dc3-decomp` (it reached rb3-xenon
in one squashed scaffold commit, `c5c1650f`, which is why local `git log -S` finds
nothing):

| commit (dc3-decomp) | date | what |
|---|---|---|
| `2daf3686` | 2026-03-20 | introduces `NullifyObj` |
| `c353866f` | 2026-03-20 | TaskMgr cascade guard + async Phase 0 |
| `e474919d` | 2026-03-20 | independent review of the approaches |
| `07bad0ab` | 2026-03-20 | **introduces `NullifyAllRefs` + the delete-this rationale** — the source of the comment now in `Object.h:2023-2025` |
| `2da54174` | 2026-03-20 | hardens it (dead-ring-entry stop, dtor guards) |
| `d41f5bf7` | 2026-03-26 | `ShouldSkipCascadeNullify` / `DetachFromDir` — reparented objects |

`07bad0ab` states it directly: *"ReplaceRefs triggers Replace callbacks which
execute delete-this in MessageTask, ScriptTask, PropertyTask, and DirLoader —
causing use-after-free when Phase 0 continues iterating the todo list."* The
adjudicating design doc is `dc3-decomp/docs/sessions/2026-03-20-cascade-meta-analysis.md`.

### 2.1 ✅ The rationale is REAL — verified against rb3-xenon's own source

All four named consumers genuinely `delete this` from `Replace()`, and none is
`HX_NATIVE`-gated:

| class | file:line | deletes when |
|---|---|---|
| `MessageTask::Replace` | `obj/Task.cpp:54` | `to == nullptr` on `mObj` |
| `ScriptTask::Replace` | `obj/Task.cpp:117` | **every path except** `from == mThis && to != nullptr` |
| `PropertyTask::Replace` | `flow/FlowSetProperty.cpp:138` | `mTarget` becomes null |
| `DirLoader::Replace` | `obj/DirLoader.cpp:155` | any match on `mProxyDir`, **null or not** |

⇒ **A blanket "just fire `Replace()` from `NullifyObj`" is not available.** All
four are `ObjOwnerPtr`s, so it would reintroduce exactly the documented hazard.
This is why the repair below does *not* do that.

### 2.2 ⛔ CORRECTION to the comment's framing (not to its conclusion)

`Object.h:2023-2025` reads as though it describes a *diagnosed* crash. It does
not. The crash that actually drove `NullifyObj` into existence was a **stale
`ObjPtr<Task>` in `TaskTimeline::Poll`** (gdb backtrace, `2daf3686`). The
delete-this UAF was **derived by inspection and never observed** — the
meta-analysis says so itself: *"It hasn't crashed yet because the specific
scenario … may not have been exercised."* The reasoning is sound and I did not
disturb it; only the "this fixed a crash" reading is overstated.

---

## 3. ★★★ The repair — retail's *value* without retail's *callback*

`ObjOwnerPtr` now retains its ctor's seed pointer and restores it in
`NullifyObj()`:

```cpp
T *mSelfSeed;                       // = ptr, in the ctor
void NullifyObj() override {
    T *seed = ObjRefSeedRestoreEnabled() ? mSelfSeed : nullptr;
    ObjRefConcrete<T>::NullifyObj();   // mObject = nullptr, ring self-loops
    if (seed) { mObject = seed; seed->AddRef(this); }
}
```

This reproduces **retail's post-`Replace` state** — `mX == this`, re-registered
in its own ring — with **no consumer callback at all**, so the delete-this blast
radius is zero by construction.

### 3.1 ★★★ Why it is provably scoped, not merely "probably safe"

Two mechanical facts, both checked:

1. **The four dangerous consumers are seeded with the default `ptr = nullptr`.**
   Their seed is null ⇒ `NullifyObj` is **bit-identical to before**. Nothing
   fires, nothing is deleted.
2. **No `ObjOwnerPtr` anywhere in `src/` is seeded with a *foreign* object.**
   Every `(this, <non-null>)` hit in the tree is an `ObjPtrList`/`ObjPtrVec`
   `(owner, mode)` constructor, not an `ObjOwnerPtr`. So the seed can only ever
   be **null or self** — there is no third case in which a restore could
   resurrect a stale pointer.

⇒ The repair is a no-op everywhere except the 14 enumerated sites. That is what
makes it an *upstream* fix rather than a blanket one.

### 3.2 The design decision worth flagging

The copy ctor deliberately does **not** inherit `mSelfSeed` (it takes `nullptr`).
A copy belongs to a *different* owner, so inheriting the source's seed would
restore the copy to the **original** object — a wrong value wearing an
invariant's clothes. Null means a copied `ObjOwnerPtr` behaves exactly as it does
today, so this repair can only improve the directly-seeded sites and can never
regress a copy.

### 3.3 The kill-switch

`RB3_NO_OWNERPTR_SEED=1` restores pre-X16 behaviour **in the same binary**
(default ON). One binary means the A/B has no cross-tree confound from embedded
debug paths and no `--revert` trap. Cached `getenv` — `NullifyObj` is a teardown
hot path.

---

## 4. ★★★ The runtime proof — a set identity, with two controls inside the measurement

`RB3_WEIGHT_CENSUS=1`, `small_club_01`, both arms from the same binary
(`x16-censusA-off.log` / `x16-censusB-on.log`, both `rc=0`):

| scope | NULL owner | self | **other** | drivers "WOULD FAULT" |
|---|---|---|---|---|
| band player0 | **7 → 0** | 23 → 30 | 6 → 6 | **2 → 0** |
| band player1 | **17 → 0** | 23 → 40 | 4 → 4 | 0 → 0 |
| band player2 (vocalist) | 0 → 0 | 22 → 22 | 8 → 8 | 0 → 0 |
| band player3 | **7 → 0** | 23 → 30 | 6 → 6 | **2 → 0** |
| nonband `lighttarget` | 0 → 0 | 6 → 6 | 1 → 1 | 0 → 0 |
| nonband `crowd_male01..03` | 0 → 0 | 1 → 1 | 0 → 0 | 0 → 0 |

★★★ **Three things make this a proof rather than an improvement:**

1. **One-for-one.** Every NULL became a `self` — `+7 / +17 / +7` exactly. No
   pointer went anywhere else.
2. **`other` is invariant.** The repair did **not** steal a single
   legitimately-foreign owner. A blanket fix would have.
3. **The zero rows are bit-identical.** player2 (a vocalist has no instrument
   rigging) and the whole crowd are unchanged across arms — a scope with no nulls
   is untouched. That is the negative control *inside* the measurement.

★ The OFF arm reproduces **X15's published table exactly** (7 / 17 / 0 / 7),
which is an independent confirmation of X15's baseline as well as of the repair.

---

## 5. ⛔ The pose — X15's cause refuted, verdict still UNDECIDED

### 5.1 The three-arm test

`--hand-audit`, `small_club_01`, all three arms from one binary, all with **real
denominators** (7380 admissible bones, 12 hand meshes — not vacuous):

| arm | worst recompose dev | worst bone | `handpose-recompose` |
|---|---|---|---|
| default (no `Poll`) | **0.000e+00** | — | ✅ PASS |
| `Poll` + seed repair **ON** | **6.172e+01** | `bone_mic_stand_bottom.mesh` | ⛔ FAIL |
| `Poll` + seed repair **OFF** | **6.172e+01** | `bone_mic_stand_bottom.mesh` | ⛔ FAIL |

### 5.2 ⛔ Therefore X15's cause is refuted

X15 §5.3 named the mechanism: *"the destroyed `CharWeightSetter`s **are** the
blend-weight sources for these IK and MIDI drivers … so IK is applied at the
wrong strength."* If that were the cause, restoring every weight owner would move
the pose. **Every weight owner is restored** (§4: 0 NULL, 0 WOULD FAULT) and the
polled pose is **bit-identical** — `cmp` on the summary lines confirms it.

★ This is the charter's rule working as intended: X15's mechanism was read off
the source, was plausible, and was wrong. It cost one extra binary-internal arm
to find out.

⚠ **What is NOT refuted:** X15's *observation* that the pose is untrustworthy is
exact and reproduced. Only its explanation falls. And X15's `3.565e+00` at
`bone_pelvis.mesh` is reproduced here **to four figures** — that measurement was
right.

### 5.3 ⛔ So X14's driver-side call is NOT retired

The charter's acceptance was three-part: `Poll()` performs the rebind **and** the
pose validates against the oracle **and** removing the call leaves the frame
demonstrably correct. Part one holds (X15 proved it, untouched here). **Part two
fails.** Retiring the call would make an invalid pose the default for every
later lane. Holding X15's line, for X15's reason.

---

## 6. ★★ The better lead — the residual is on props, trousers and hair

Per-figure recompose in the poll arm (`x16-hp-poll.log`):

| figure | worst dev | worst bone | kind |
|---|---|---|---|
| 4 crowd figures | 0.000e+00 | — | clean |
| — | 1.154e+00 | `bone_legs-ring2.mesh` | **trousers** |
| — | 4.125e-01 | `bone_legs_L03.mesh` | **trousers** |
| — | 6.172e+01 | `bone_mic_stand_bottom.mesh` | **prop** |
| — | 5.566e-02 | `bone_hair_back01.mesh` | **hair** |
| — | 3.565e+00 | `bone_pelvis.mesh` | body (X15's bone) |

★★ **Cross-reference:** X15 §7.2 listed seven meshes the rebind skips with
unresolved bones — `bone_hair_*`, `bone_legs_a01..g01`,
`bone_legs_{L,M,R}0{1,2,3}`, `bone_legs-ring1`. **The bones carrying the pose
residual are the same family.** The natural hypothesis for X17: the unresolved
secondary bones (props/trousers/hair) are never rebound, so under `Poll()` they
keep stale world transforms while their parents move — i.e. **the pose defect and
the rebind-skip defect are one defect**, and the mic stand (a prop parented to
the vocalist) is the loudest instance at 61.7 units.

⚠ **Stated as a hypothesis, not a finding.** I did not test it. It is cheap to
test: repair `tightdistressedpants_resource.1.mesh` (X15: needs exactly **one**
bone) and see whether its figure's recompose deviation drops.

---

## 7. ⛔ Retracted, mine, mid-lane

1. ⛔ **Two `--focus hands_naked` audit runs.** Both reported
   `0 skinned mesh(es), 0 bone(s)` and still printed `[PASS] palette-invariant`.
   `--focus` restricts the mesh walk, so the oracle's denominator was zero — a
   **vacuous pass**, the exact failure X12/X13 documented. Discarded, re-run
   without `--focus`.
2. ⛔ **Two `--bone-audit` runs read as if they were the pose oracle.** They are
   a different gate (`bone-length-invariant`). The handpose oracle is
   `--hand-audit` (`main_render.cpp:4004`). Both were caught by the charter's
   rule — I checked the denominator instead of the verdict.

⚠ Also hit and recorded: **`--focus band` matched no mesh name**, producing
`[FAIL] bbox — no mesh contributed a vertex`. That was a bad argument, **not** a
regression, and it would have read as one.

⚠ **zsh's no-word-split rule fired again.** A `$OBJS` string of six object paths
in a `for` loop iterated **once**, with `basename` returning the concatenation —
printing a single `MISSING Task.obj` that looked like a build failure. Fixed with
a real array. The charter names this hazard; it still cost a cycle.

---

## 8. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **Size of the `ObjOwnerPtr` class** | ★★★ **VERIFIED — 14 sites** (X15 fixed 2; 12 were open) | §1 |
| **Enumeration instrument** | ★★★ **VERIFIED — 4 positive + 1 negative control; refuses to report on failure** | §1.1 |
| **Instrument self-bugs** | ⛔ **TWO CAUGHT BY ITS OWN CONTROLS** — comment pollution; a restore regex with 7 false negatives | §1.1 |
| **Why the `HX_NATIVE` shortcut exists** | ★★★ **DOCUMENTED, NOT FOLKLORE — `07bad0ab` + meta-analysis doc** | §2 |
| **Is the delete-this rationale real?** | ✅ **YES — all four consumers verified `delete this` in rb3-xenon source** | §2.1 |
| **Comment's "fixed a crash" framing** | ⛔ **CORRECTED — hazard was predicted, never observed; the real crash was `TaskTimeline::Poll`** | §2.2 |
| **Can the cascade fire `Replace()`?** | ⛔ **NO — and now for a measured reason, not a guess** | §2.1 |
| **Upstream repair** | ★★★ **LANDED — one change, all 14, zero callbacks** | §3 |
| **Repair scoping** | ★★★ **PROVED — dangerous consumers seed null; no foreign seed exists in `src/`** | §3.1 |
| **Repair works at runtime** | ★★★ **VERIFIED — one-for-one set identity, `other` invariant, zero-rows bit-identical** | §4 |
| **X15's baseline census** | ✅ **REPRODUCED EXACTLY (7/17/0/7)** | §4 |
| **X15's blend-weight cause for the pose** | ⛔ **REFUTED — pose bit-identical with all weight owners restored** | §5.2 |
| **X15's `3.565e+00` at `bone_pelvis`** | ✅ **REPRODUCED to four figures** | §5.1 |
| **Is the polled POSE correct?** | ⛔ **STILL UNDECIDED — `handpose-recompose` FAILs, cause now unknown** | §5 |
| **X14's driver-side call retired?** | ⛔ **NO — part two of the acceptance fails; deliberately held** | §5.3 |
| **Where the pose residual lives** | ★★ **NEW LEAD — prop/trouser/hair bones, same family as X15's unresolved-bone list** | §6 |
| **Frames** | ✅ **NO REGRESSION — opened both; no shards, no explosion, venue+crowd unchanged** | §9 |
| **X360 blast radius** | ✅ **ZERO — `.text` byte-identical across 6 TUs** | §9 |
| **Textures / `OutfitConfig`** | ⚠ **UNREACHED — not attempted this lane** | §10 |
| **Hair / the 7 skipped meshes** | ⚠ **UNREACHED — but now implicated in the pose residual** | §6 |
| **Two of my own measurement runs** | ⛔ **RETRACTED — vacuous denominators** | §7 |

---

## 9. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, **0 warnings**, **0 SKIP lines**. All 18 binaries **deleted first** and **relinked this run**; binary presence re-confirmed before any probe | `x16-gate.log` |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — baseline gate on the branch point passed 18/18 before any edit | `x16-gate-baseline.log` |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606` unmoved | foreign `src/platform/FxSendNative.cpp` edit disclosed, untouched — **thirteenth lane** |
| d | Shared-`src/` X360-faithful, symbol granularity, **both objects built in this worktree** | ✅ **PASS — `.text` byte-identical across 6 TUs**: `Object` 274/274, `CharWeightable` 93/93, `Character` 2403/2403, `Dir` 1546/1546, `DirLoader` 470/470, `Task` 333/333. Sole delta `.debug$S` | `x16-x360-ab.log`; comparator runs a **positive control first** and refuses to report without detecting a known difference |
| d2 | objdiff position of touched TUs | ✅ **ALL SCOREABLE** — `default/Object`, `default/CharWeightable`, `default/Character`, `default/system/obj/Dir`, `default/DirLoader`, `default/Task`. **No TU here is unscoreable.** ⚠ `Dir`'s unit is `default/system/obj/Dir`, **not** `default/Dir` — a bare-name lookup wrongly reports it missing | stated, not implied |
| d3 | Does the change alter destruction **order**? | ✅ **NO** — `NullifyObj` writes a pointer and adds a ring entry; it destroys nothing and calls no callback. Destruction order is set by Phase 0's `todo` walk, untouched | §3 |
| e | PNG determinism ×2 on every cited image | ✅ **PASS** — both cited PNGs `cmp`-identical across two full runs; the two arms differ from each other (so the A/B is not vacuous) | §9.1 |
| f | Prior evidence non-regressed | ✅ **PASS** — X15's census table (7/17/0/7), X15's `3.565e+00` at `bone_pelvis`, and X15's `Character` 2403/2403 X360 count all reproduced **exactly** | §4, §5.1, gate d |

### 9.1 Frames

`x16-A-default-club.png` and `x16-B-poll-club.png` (determinism ×2 each). Opened
both. Venue, lighting and the 180-draw crowd are unchanged between arms; the four
band figures on the stage change pose. **No shards, no explosion, no missing
geometry** — X14's 7–14× hair blow-up does not recur.

⚠ **Read the gate's own verdict line, not the pipeline exit code** — carried
from X12–X15: `grep -c SKIP` exits 1 on zero matches, so the failure code *is*
the 0-SKIPs result.

⚠ **The `--revert` trap was live and was handled.** Building the X360 BASE
objects required reverting three files in-tree. After restoring, `git status
--short` was re-read **and** `git diff --quiet HEAD` was run on all three, **and**
the FIX content was re-grepped, before any comparison.

---

## 10. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Test the one-defect hypothesis for the pose** | §6: the recompose residual sits on prop/trouser/hair bones — the same family as X15's unresolved-bone skip list. Cheapest probe on the board: fix `tightdistressedpants_resource.1.mesh` (**one** bone) and watch that figure's deviation. If it drops, the pose defect *is* the rebind-skip defect | X17 |
| ★★★ **`bone_mic_stand_bottom` at 61.7 units** | §6: by far the largest single deviation, on a **prop** parented to the vocalist. Props may not be rebound at all — a whole category, not a bone | X17 |
| ★★ **Then retire X14's driver-side call** | §5.3: blocked *only* on the pose validating. The A/B is built (`RB3_BAND_POLL=only`) and the change is one line | X17 |
| ★★ **The 12 newly-repaired sites are untested individually** | §1: the census proves the *weightable* sites restore. `RndMesh::mGeomOwner`, `RndEnviron::mAmbientFogOwner`, `Spotlight::mColorOwner` etc. are repaired by the same mechanism but no lane has exercised them. Cheap: census them the way §4 censuses weight owners | X17 |
| ★★ **Consider retiring X15's two site-guards** | §3: now redundant. Kept because they are retail-equivalent and harmless, but they are dead weight and hide the class | X17 |
| ⚠ **`SEEDED_NO_REPL` (2 sites)** | §1: `RndFont3d::mTextureOwner`, `RndLightAnim::mKeysOwner` — seeded `(this,this)` with no `Replace` restore found. Either a fifth restore spelling the screen missed, or a genuine shared (non-native) gap | X17 |
| ★★ **`OutfitConfig` registration** | X15 §7.1, untouched here. Price verified by X15: 37 mechanical + 11 a real port | its own lane |
| ⚠ **`ObjPtrList` NULL-entry, Direction-B rows, `CharMeshHide::HideAll`, orphans, `BandCamShot`** | carried, untouched | as before |
| ⚠ **Engine CR: none filed** | this lane needed no engine change | — |

---

## 11. Recommended X17 shape

1. ★★★ **A predecessor's *cause* is a hypothesis even when its *measurement* is
   perfect.** X15's numbers all reproduced exactly; its explanation for them did
   not survive one extra arm. Reproduce the measurement, then test the cause
   separately — they are different claims.
2. ★★★ **A kill-switch in one binary beats two binaries.** Every A/B here was
   `RB3_NO_OWNERPTR_SEED` in the *same* executable: no cross-tree debug-path
   confound, no `--revert` trap, and the refutation in §5.2 became a single
   `cmp`.
3. ★★★ **Build the negative control into the measurement.** The vocalist and the
   crowd rows are what turn §4 from "the numbers improved" into "the change
   touched exactly the intended set and nothing else".
4. ★★ **Controls catch instruments, not just code.** Both self-bugs in §1.1 were
   found by controls, and the second one was invisible to the first two controls —
   *add a control per spelling you learn about.*
5. ★ **Check the denominator before the verdict.** Two of my runs printed PASS
   over zero meshes.

---

## 12. Evidence

All under `/home/free/tmp/laneX16/evidence/`.

| file | what it shows |
|---|---|
| `x16-census.log`, `x16-census.json` | the 14/2/23 enumeration with all five controls passing — §1 |
| `x16-censusA-off.log` | weight-owner census, repair **OFF** — reproduces X15's 7/17/0/7 — §4 |
| `x16-censusB-on.log` | same census, repair **ON** — all NULLs → self, `other` invariant — §4 |
| `x16-hp-default.log` | hand-pose oracle, default arm — `worst dev 0.000e+00`, PASS — §5.1 |
| `x16-hp-poll.log` | poll arm — `6.172e+01` at `bone_mic_stand_bottom`, FAIL; per-figure breakdown — §5.1, §6 |
| `x16-hp-poll-seedoff.log` | poll arm with the repair OFF — **bit-identical**, which refutes X15's cause — §5.2 |
| `x16-A-default-club.png` / `x16-B-poll-club.png` | the two frames (determinism ×2 each) — §9.1 |
| `x16-x360-ab.log`, `../ab/x16_driver.py`, `../ab/*.{BASE,FIX}` | X360 `.text` A/B over 6 TUs, positive control first — gate (d) |
| `x16-gate.log` | native gate PASS 18/18 fresh, rc=0, 0 SKIPs — gate (a) |
| `x16-gate-baseline.log` | the branch-point gate, proving `main` was healthy — gate (b) |
| `x16-ha-default2.log`, `x16-ha-poll2.log` | ⛔ **retracted** — `--bone-audit` is not the pose oracle — §7 |
