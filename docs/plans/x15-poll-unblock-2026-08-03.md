# X15 — `Poll()` runs, and the cause X14 gave for why it could not is refuted

**Date:** 2026-08-03
**Predecessor:** [X14](x14-band-placement-2026-08-03.md) "the band lands on its marks; the repair was already in the tree and was never called"
**Branch:** `x15-poll-unblock`, from `main` @ `57093d0a`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**
**Change surface:** three shared `src/` files (`char/CharWeightable.h`, `char/Character.cpp`, `bandobj/BandCharacter.cpp`) — every edited region inside `#ifdef HX_NATIVE` with the X360 token stream preserved verbatim in the `#else` — plus the native-only driver `native/src/main_render.cpp`.

---

## Verdict

★★★ **`BandCharacter::Poll()` RUNS.** 4/4 members, `rc=0`, in two venues. It is
the shipped per-frame entry point and it has never executed in this ladder. §1

⛔ **X14's ROOT CAUSE IS REFUTED.** X14: "`Load` overwrites it with
`d >> mWeightOwner` (:74), which resolves NULL when the named owner is absent."
The weight-owner slot is **never** an empty-name load (0 of 11,494), and it
resolves to a real named object — `left_hand.weight` / `right_hand.weight` —
**every time**. §2

★★★ **THE REAL CAUSE IS A NATIVE-ONLY RING-TEARDOWN SHORTCUT, AND IT IS A CLASS
OF DEFECT, NOT A SITE.** `~ObjectDir` Phase 0 calls `NullifyAllRefs()`, which by
its own documented design does **not** fire `Replace()` — and `Replace()` is the
only place these classes restore their "null means me" invariant. Fixing
`CharWeightable::Weight()` moved the fault straight to `Character::mSphereBase`,
which has the identical pattern. §3

★★ **THE ANSWER TO THE ACCEPTANCE CRITERION IS "PARTLY, AND THE REMAINDER IS A
FINDING".** `Poll()` **does** perform the rebind — the band stays on its marks
with X14's driver-side call removed entirely. But the frame is **not** unchanged,
because `Poll()` also *poses* the skeleton. So the call is redundant *for the
rebind* and is **not** retired, because retiring it would silently adopt an
unvalidated pose as the default. §4

★★★ **PLACEMENT SURVIVES ANIMATION — PROVED BY X14's OWN INVARIANT.** The polled
per-member `drawn z − slot z` is identical across `small_club_01` and `arena_01`
to **0.000 / 0.001 / 0.007 / 0.000** — the same precision as the bind-pose arm. §4.2

⛔ **BUT THE POSE ITSELF IS UNDECIDED, AND I CAN NAME WHY.** The destroyed
`CharWeightSetter`s are the blend-weight sources for the IK and MIDI drivers, so
under `Poll()` those weights fall back to a default instead of the setter's
computed value. `Enter()` — X13's control — makes it *worse*, not better. §5

⛔ **RETRACTED, MINE, MID-LANE: three "empty result" measurements that were the
binary missing.** §6.2

⚠ **WHAT DID NOT LAND: textures and hair.** The `OutfitConfig` bill was
re-derived and is **accurate**; it is not payable here. Hair is unchanged — and
X14's characterisation of it is **corrected**: the residual is not hair-only. §7

---

## 1. ★★★ `Poll()` runs

`RB3_BAND_POLL=1` (driver, `main_render.cpp`) calls the shipped
`BandCharacter::Poll()` on each wardrobe member.

```
band: BandCharacter::Poll() x8 on 4 member(s) — the SHIPPED per-frame entry
point, reached for the first time in this ladder.
```

`rc=0`, `small_club_01` and `arena_01`. Before this lane it was `rc=139`.

The instrument refuses to report a pass over an empty set: with no
`TheBandWardrobe`, or with 0 members reached, it prints `⛔ POLL SKIPPED` /
`⛔ POLL REACHED 0 MEMBERS` and says in the message why an empty poll would read
identically to a successful one.

---

## 2. ⛔ Why X14's cause is wrong — three independent measurements

X14 §3.1 states the fault as `Load` resolving NULL "when the named owner is
absent". Each step below was measured, not argued.

**(a) The slot is never an empty-name load.** Instrumenting
`ObjRefConcrete<T1,T2>::Load` (`obj/ObjPtr_p.h`) to print every reference that
comes back NULL, with `typeid(T1)`: a club run produces **11,494** such loads,
and **not one** has `T1 = CharWeightable`. The classes that do appear for these
objects are `CharCollide`, `CharBonesObject`, `CharClip`, `Hmx::Object`
(`x15-objref2.log`).

**(b) The slot resolves to a real, named object.** Instrumenting
`CharWeightable::Load` itself to print the owner after `d >> mWeightOwner`
(`x15-wo2.log`):

| weightable | resolved owner at Load |
|---|---|
| `fret.dmidi` (`CharDriverMidi`) | `left_hand.weight` (`char/main/rigging/guitar_rh.milo`) |
| `strum.dmidi` (`CharDriverMidi`) | `right_hand.weight` (same dir) |
| `fret.ikhand` (`CharIKHand`) | `left_hand.weight` (same dir) |
| `snare.dmidi` (`CharDriverMidi`) | `right_hand.weight` (`char/main/rigging/drum.milo`) |

All 177 `CharWeightable::Load` calls took the `d.rev > 1` read arm. **Zero**
`Replace` calls landed on any object in the null set (13 `Replace`s in the whole
run, all on other objects, all taking the restore arm).

**(c) Pointer identity closes it.** The censused null-owner
`CharDriverMidi:strum.dmidi @0x558339c1edf8` is **bit-identically** the object
that logged `owner=right_hand.weight` at Load. Same object, not a copy. The owner
was resolved and then **taken away**.

⇒ Load is not the mechanism. Something destroys the owner afterwards.

### 2.1 ★★ The census that made this legible was DIFFERENTIAL by construction

A band-only tally of null weight owners is uninterpretable — N could be every
character's normal state. X13 had already driven **eight crowd characters**
through the identical `Character::Poll` → `CharDriver::Poll` path without
crashing, so the crowd is a working control on the same code. `ReportWeightOwners`
reports both, and refuses a population with zero weightables
(`x15-census3.log`):

| scope | weightables | NULL owner | self | other | drivers (null / with queued clip / **would fault**) |
|---|---|---|---|---|---|
| band player0 | 36 | **7** | 23 | 6 | 8 (5 / 2 / **2**) |
| band player1 | 44 | **17** | 23 | 4 | 16 (13 / 0 / 0) |
| band player2 | 30 | **0** | 22 | 8 | 2 (0 / 0 / 0) |
| band player3 | 36 | **7** | 23 | 6 | 8 (5 / 2 / **2**) |
| nonband `lighttarget` | 7 | **0** | 6 | 1 | 6 (0 / 0 / 0) |
| nonband `crowd_male01..03` | 1 each | **0** | 1 | 0 | 1 (0 / 0 / 0) |

★ The control is clean and the band is not, so the defect is in what
distinguishes them. The nulls are **exclusively** instrument rigging objects
(`*.dmidi`, `*.ikhand`, `*_add.drv`) — and **player2, the vocalist, has none**,
because a vocalist has no instrument rigging.

★ The `would fault` column separates a null that *can* fault from one that
cannot: `Weight()` is only reached under `if (mFirst)` (`CharDriver.cpp:674`).
Conflating those two is how "N nulls" becomes a cause it has not earned.

---

## 3. ★★★ The real cause, and why it is a class of defect

### 3.1 The mechanism, named and traced

Those `CharWeightSetter`s are destroyed, and the destruction is instrumented
(`x15-nullify.log`):

```
[nullify] right_hand.weight (guitar_righthanded (char/main/rigging/guitar_rh.milo)) class=CharWeightSetter
[nullify] left_hand.weight  (guitar_righthanded (char/main/rigging/guitar_rh.milo)) class=CharWeightSetter
[nullify] right_hand_ik.weight (drum_rigging (char/main/rigging/drum.milo)) class=CharWeightSetter
…
```

`ObjectDir::~ObjectDir` Phase 0 (`obj/Dir.cpp:119-135`, **entirely inside
`#ifdef HX_NATIVE`**) calls `Hmx::Object::NullifyAllRefs()` on every object in
the dying dirs. That walks the ring calling `ObjRef::NullifyObj()`, which stores
`mObject = nullptr` directly and — by its own documented design
(`obj/Object.h:2023-2025`, *"No Replace callbacks fire — avoids delete-this in
MessageTask/ScriptTask/PropertyTask/DirLoader"*) — **deliberately does not invoke
the consumer's `Replace()`.**

`CharWeightable::Replace` (`char/CharWeightable.cpp:9-18`) is the **only** place
the class restores `mWeightOwner = this`. It never runs.

Retail X360 has no such shortcut: the referent's death runs `Replace(ref,
nullptr)`, `SetObj` returns null, and `mWeightOwner = this`.

### 3.2 ★★ Who destroys the rigging dir — symbolized backtrace

`x15-dirdeath.log`, `addr2line`-resolved:

```
~ObjectDir(drum_rigging)
  <- RndDir::~RndDir  <- Character::~Character
  <- FileMerger::PostMerge      <- FileMerger::FinishLoading
  <- DirLoader::LoadObjs        <- DirLoader::PollLoading
  <- LoadMgr::PollFrontLoader   <- FileMerger::StartLoadInternal
  <- FileMerger::StartLoad      <- BandCharacter::StartLoad
  <- BandWardrobe::StartClipLoads
```

★ **It is the merge.** `FileMerger::PostMerge` destroys the merged-away
`Character`, which is shipped behaviour. Only the *native* consequence differs.
This is the same `FileMerger` path X12/X13 flagged, reached from a third
direction.

### 3.3 ⇒ Why the guards are RETAIL's value, not a fallback

When the owner dies, retail's post-`Replace` state is `owner == this`, and
`Weight()` then returns `this->mWeight`. The guard returns `this->mWeight`. **The
same number.** Nothing is synthesized, chosen or interpolated; the null case is
spelled out instead of being reached through a pointer the native cascade has
already cleared.

### 3.4 ★★★ It is a class of defect — the second site proved it immediately

Guarding `Weight()` did **not** make `Poll()` run. The fault moved to:

```
Character::MakeWorldSphere <- BandCharacter::CalcBoundingSphere <- BandCharacter::Poll
```

`Character::mSphereBase` is the identical pattern: ctor seeds
`mSphereBase(this, this)` (`Character.cpp:62`), `Load` coerces a null loaded
pointer to `this` **twice and explicitly** (`:257-262`), `Replace` restores
`mSphereBase = this` (`:73-77`). gdb confirms the pointer NULL — `%rax = 0` at
the `+0xd0` dirty-flag test inside `WorldXfm()`. Guarded at both deref sites
(`UpdateSphere`, `MakeWorldSphere`).

⚠ **Two sites are not a proof that there are only two.** Every
`ObjOwnerPtr` seeded `(this, this)` whose class restores the invariant *only* in
`Replace` is exposed. This lane fixed the two that block `Poll()`; it did not
enumerate the rest. The single-point repair — making the cascade preserve
consumer invariants — is named as owed work in §8 and deliberately **not**
attempted, because `NullifyObj`'s Replace-skip is load-bearing against
delete-this in four other subsystems.

---

## 4. ★★ The acceptance criterion, answered precisely

The charter: *"if `Poll()` runs, X14's workaround should become unnecessary, and
removing it should leave the frame unchanged. If it doesn't, that discrepancy is
itself the finding."*

**It runs. It does the rebind. The frame is not unchanged.**

### 4.1 `Poll()` does perform the rebind

`RB3_BAND_POLL=only` skips X14's direct `RebindOutfitBonesToOwnSkeleton()` call
entirely — the log line is absent, verified by `grep -c` = 0. The band is
nevertheless on its marks: four distinct `hands_naked` centroids at the four
authored slots. With no rebind at all, X14 measured all four at one point,
(−0.00, 0.66, 40.08). So the rebind ran, from inside `Poll()` at
`BandCharacter.cpp:533`, exactly as X14 predicted.

### 4.2 ★★★ And the placement is still exact — X14's invariant, under animation

`drawn z − slot z`, the rotation-invariant axis (the slot transforms carry yaw):

| member | **default arm** club / arena / \|Δ\| | **poll arm** club / arena / \|Δ\| |
|---|---|---|
| player0 | 40.045 / 40.045 / **0.0000** | 44.245 / 44.245 / **0.0000** |
| player1 | 32.168 / 32.169 / **0.0010** | 56.168 / 56.169 / **0.0010** |
| player2 | 40.042 / 40.045 / **0.0030** | 47.562 / 47.555 / **0.0070** |
| player3 | 40.042 / 40.042 / **0.0000** | 44.392 / 44.392 / **0.0000** |

★★★ The polled pose is a **deterministic, venue-independent per-member
articulation rigidly composed onto the authored slot**, at the same precision as
the bind-pose arm. Placement survives animation.

⚠ **This proves placement, NOT pose.** A wrong pose applied identically in two
venues passes this test. Said plainly because the distinction is exactly the one
that produced a wrong close-out at X9.

### 4.3 ⛔ So the driver-side call is NOT retired — deliberately

Retiring it would make the unvalidated pose of §5 the default for every
subsequent lane's frames. The default path is unchanged and **byte-identical to
X14's artifact**; `Poll()` lives behind `RB3_BAND_POLL`. When the pose is
validated, retiring the call is a one-line change with a ready A/B.

⚠ **A confound I introduced and then controlled for.** The first `only` arm
differed partly because the `pollOnly` branch also skipped X14's
`setenv(RB3_SKEL_REBIND_FULL)`. Re-run with the scope matched explicitly; it
still differs, for the reason above. Recorded because the first number was wrong
for a reason that had nothing to do with the question.

---

## 5. ⛔ The pose is UNDECIDED, and the reason has an address

**The frames.** `x15-B-poll-club-focus.png` against `x15-A-default-club-focus.png`,
identical camera: the default is a T-pose — four figures with arms straight out.
The polled arm has arms down at their sides, the bassist's hand up at the neck,
and **the drummer seated at the kit with her legs bent and both sticks raised
over the cymbals.** `x15-D-poll-arena-focus.png` shows the same in `arena_01`.
That is a large, plausible, shipped-data-derived change.

**Plausible is not verified.** Three things say so:

1. ⚠ **A contamination signature matching X13's retraction.** X13 published
   "the recompose identity FAILS under animation — 3.473e+00 at
   `bone_pelvis.mesh`" and retracted it: its own loop was polling `CharDriver`s
   that were never `Enter()`ed. The first X15 poll arm measures **3.565e+00 at
   `bone_pelvis.mesh`** — same bone, same magnitude. The gate
   `handpose-recompose` **FAILs** in the poll arm and passes in the default arm.

2. ⛔ **X13's own control makes it WORSE, which is the opposite of X13's result.**
   `RB3_BAND_ENTER=1` calls the shipped `CharDriver::Enter()` on all 34 drivers.
   The guitarists' hands then land **20–27 units off their slots** (player0
   x −66.33 → −93.42, player3 68.84 → 46.51) and the worst recompose deviation
   moves to `bone_R-hand.mesh` at **1.09e+01**. `Enter()` calls `Clear()`, so the
   census afterwards shows **0 drivers with a queued clip** where there were 4.

3. ★★ **The named reason, and it is the same defect.** The destroyed
   `CharWeightSetter`s **are** the blend-weight sources for these IK and MIDI
   drivers — that is what `left_hand.weight` / `right_hand_ik.weight` are for.
   With the setter gone, `Weight()` returns the driver's own `mWeight` (retail's
   post-`Replace` value, §3.3) rather than the setter's per-frame computed value,
   so IK is applied at the wrong strength. `bone_R-hand.mesh` — an IK-solved bone
   — carrying the worst deviation is consistent with that, and the null set being
   exactly the `*.ikhand` / `*.dmidi` objects is consistent with it too.

⇒ **The crash is fixed and `Poll()` is reachable; the animation is not yet
trustworthy, and the blocker is upstream of `Poll()` — the rigging dir's
destruction (§3.2), not `Poll()` itself.** Reported as undecided rather than
shown as a result.

---

## 6. ⛔ Retracted / corrected, with evidence

### 6.1 ⛔ X14's `mWeightOwner` root cause — refuted (§2)

The **symptom** X14 recorded is exact and reproduced (`Weight()` faults through
`CharDriver::Poll`). The **cause** is not Load. X14 read a plausible mechanism
off the source and did not instrument it; three cheap probes contradict it.

### 6.2 ⛔ MINE, MID-LANE: three measurements that were the binary missing

I deleted `native/build/rb3-*` to make the gate fresh, then ran three probes
before the gate had rebuilt. All three returned **empty** output, and I began
reading "no hair skips" and "no unrepairable slots" as findings. They were
`./build/rb3-render: No such file or directory`, rc=127. Caught by checking a log
that was one line long. ★ **Silence is not a result** — the same lesson this
ladder has now learned in four different costumes (X12's vacuous PASS, X13's
zero-mesh gate, X14's silent probe, and this).

### 6.3 ⛔ X14's "the target list is mechanical" — corrected (§7.2)

### 6.4 ⚠ NOT retracted, and NOT re-tested here

X14's placement result, its `body_clips` finding, its shard mechanism, and its
`Rnd_Wgpu_RB3.cpp` refutation are untouched by this lane. The default-arm frames
are byte-identical to X14's artifact, which is the strongest available check that
nothing in them moved.

---

## 7. ⚠ Milestones 2 and 3 — what did NOT land

### 7.1 Textures: the bill is RE-DERIVED and ACCURATE — and is not payable here

The charter asked for re-derivation rather than inheritance. Independently
reproduced from today's tree and today's build artifacts:

- `src/system/bandobj/BandPatchMesh.cpp` is **191** lines; rb3-Wii's is **1511**. ✅
- Enabling `OutfitConfig::Init()` leaves **exactly 48** unresolved symbols —
  measured by `nm -u` on `OutfitConfig.cpp.o` minus the defined-symbol set of the
  other 350 `rb3-render` objects plus `libmilo-engine.a`/`libimgui.a`, filtering
  libc/libstdc++. Raw undefined 212 → **48**. ✅
- The split is **11** `BandPatchMesh` bodies + **37** bare globals (36 `Symbol`s
  + `gRB3OutfitComposeActive`). ✅

★ **Two refinements to X10's framing:**

1. The *diagnosis* is BandPatchMesh's partial port; the **action** is
   registration. `OutfitConfig.cpp` **is** compiled into `rb3-render`
   (`native/CMakeLists.txt:1230`), but `OutfitConfig::Init()` is **called by
   nothing** — `native/src/milo_object_factories.cpp:467-495` is a block comment
   where the call would go. `"Can't make OutfitConfig"` is emitted at
   `obj/DirLoader.cpp:1049`, guarded solely by
   `if (!Hmx::Object::RegisteredFactory(classSym))` at `:1039` — a *registration*
   test, nothing to do with whether the TU compiled. The link succeeds today only
   because `--gc-sections` discards the unreferenced `Init()`.
2. ⛔ **X10's scatter-path citation is stale.** X10 said the live host is
   `ui/UIListDir.cpp → LightPreset.cpp`. It is **`rndobj/TexBlender.cpp`**
   (verified: that object defines `LightPreset::Animate` *and* the
   `BandPatchMesh::WorkVerts` / `MeshVert::AddUV` bodies). X10's conclusion — the
   191 lines *are* in the link — survives; the citation does not. Same shape as
   X14's correction of X13.

⇒ 37 of 48 are a mechanical `m*_symbols.cpp`-shaped chore; **11 are a real port**.
Not started. `OutfitConfig` remains the texture blocker, now with an action and a
verified price rather than an inherited one.

### 7.2 Hair: UNCHANGED — and the residual is **not hair-only**

The skip set is **identical** with `Poll()` on and off (7 meshes × 8 iterations,
both arms), so `Poll()` changes nothing here. The hairpieces still float at the
venue origin, visible in `x15-A-` and `x15-B-`.

⛔ **X14's handoff says the all-or-nothing rule "now names them precisely
(`SKIP (partial)` lines under `SKEL_REBIND_PROBE=1`), so the target list is
mechanical". It did not** — the line named the *mesh* and never the unresolved
*bone*, which is the only thing a repair can act on. Fixed here; the actual list
(`x15-M_hairnames.log`):

| mesh | unresolved / total | bones |
|---|---|---|
| `bedhead_resource.mesh` | 9/10 | `bone_hair_*` |
| `blownback_resource.mesh` | 16/17 | `bone_hair_*` |
| `mohawk_resource.mesh` | 4/5 | `bone_hair_*` |
| `youngozzie_resource.mesh` | 16/17 | `bone_hair-*` ← **hyphens** |
| `buttflappants_belts.mesh` | 7/11 | `bone_legs_a01..g01` |
| `buttflappants_resource.mesh` | 9/12 | `bone_legs_{L,M,R}0{1,2,3}` |
| `tightdistressedpants_resource.1.mesh` | 1/4 | `bone_legs-ring1.mesh` |

★ **Three of seven are trousers.** X14's owed item "give each member its own
`bone_hair_*` bones" repairs 4 of 7. ★ And the naming is **not one convention** —
`youngozzie` uses `bone_hair-L-01` where `bedhead` uses `bone_hair_l-01`, so a
prefix match written against one will silently miss the other.
★ `tightdistressedpants_resource.1.mesh` needs **exactly one** bone and is the
cheapest single win on the board.

⚠ **No shard.** The skip set is byte-identical across arms, so every failing mesh
is left exactly as shipped. X14's 7–14× hair explosion cannot recur here, and the
frames were opened to confirm it — `x15-B-` and `x15-D-` show intact hair
geometry, not spikes.

### 7.3 Also not attempted

- ⚠ The single-point cascade repair (§3.4, §8) — deliberately, blast radius.
- ⚠ Playing a real body clip (X14's ★★ item). §5 makes it *less* attractive than
  X14 expected: the blend-weight sources are destroyed, so a clip would blend
  through the same wrong weights.
- ⚠ `ObjPtrList` NULL-entry, Direction-B rows, `CharMeshHide::HideAll`, 42 orphan
  files, `BandCamShot`, `Transform::LookAt`, `Invert(Matrix4)` — all carried.
- ⚠ The foreign uncommitted engine edit `src/platform/FxSendNative.cpp` is still
  there, still not mine, left untouched — **twelfth lane running**.

---

## 8. Per-subsystem verdicts

| subsystem | verdict | evidence |
|---|---|---|
| **`BandCharacter::Poll()` reachable** | ★★★ **VERIFIED — 4/4 members, rc=0, two venues** | §1 |
| **X14's `mWeightOwner`-from-`Load` cause** | ⛔ **REFUTED — 3 independent measurements** | §2 |
| **Real cause of the null** | ★★★ **VERIFIED — native `~ObjectDir` cascade `NullifyAllRefs` skips `Replace()`** | §3.1 |
| **Who destroys the rigging dir** | ★★★ **VERIFIED — `FileMerger::PostMerge`, symbolized backtrace** | §3.2 |
| **Guard value == retail value** | ★★ **REASONED FROM THE SHIPPED CODE — retail's `Replace` leaves `owner == this`** | §3.3 |
| **Is it one site?** | ⛔ **NO — a CLASS. `mSphereBase` failed identically and immediately** | §3.4 |
| **Are there only two sites?** | ⚠ **UNKNOWN — not enumerated, stated as unknown** | §3.4 |
| **`Poll()` performs the rebind** | ★★★ **VERIFIED — band on marks with X14's call fully removed** | §4.1 |
| **Placement under animation** | ★★★ **VERIFIED — cross-venue invariant, max \|Δ\| 0.007** | §4.2 |
| **X14's driver-side call retired?** | ⛔ **NO — redundant for the rebind, but retiring it would adopt an unvalidated pose** | §4.3 |
| **Is the polled POSE correct?** | ⛔ **UNDECIDED — contamination signature + named blend-weight cause** | §5 |
| **`Enter()` as X13's control** | ⛔ **MAKES IT WORSE — hands 20–27 units off; opposite of X13's result** | §5.2 |
| **Band texturing** | ⛔ **UNREACHED — bill re-derived and CONFIRMED (191/1511/48)** | §7.1 |
| **X10's OutfitConfig numbers** | ✅ **ACCURATE — reproduced independently today** | §7.1 |
| **X10's scatter-host citation** | ⛔ **STALE — `rndobj/TexBlender.cpp`, not `ui/UIListDir.cpp`; conclusion survives** | §7.1 |
| **Hair** | ⛔ **UNCHANGED — `Poll()` alters nothing; skip set identical in both arms** | §7.2 |
| **X14's "hair-only" residual** | ⛔ **CORRECTED — 3 of 7 skipped meshes are trousers; two bone-naming conventions** | §7.2 |
| **Shard/explosion** | ✅ **NONE — skip set identical, frames opened** | §7.2 |
| **Prior lanes' frames** | ✅ **NON-REGRESSED — default club frame BYTE-IDENTICAL to X14's artifact** | §9 |
| **X360 blast radius** | ✅ **ZERO — `.text` byte-identical across 6 TUs** | §9 |
| **Three of my own measurements** | ⛔ **RETRACTED — the binary was missing** | §6.2 |

---

## 9. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Native gate **fresh**, rc=0, **0 SKIPs** | ✅ **PASS 18/18**, rc=0, 0 errors, **0 warnings**, **0 SKIP lines** | `x15-gate.log`; stale `rb3-*` deleted first; cache seeded with **all four** flags |
| b | Was `main` broken by a decomp lane? | ✅ **NO** — `rb3-song` links; `57093d0a` fixed X14's `Jukebox` breakage before this branch point | all 18 relinked this run |
| c | Zero `milo-native-engine` edits | ✅ **PASS** — pin `138e1606…` unmoved | foreign `FxSendNative.cpp` disclosed, untouched — **twelfth lane** |
| d | Shared-`src/` X360-faithful, symbol granularity, **both objects built in this worktree** | ✅ **PASS — `.text` byte-identical across 6 TUs**: `Character` 2403/2403, `CharDriver` 827/827, `CharWeightSetter` 627/627, `CharIKHand` 373/373, `CharWeightable` 93/93, `BandCharacter` 1796/1796. Sole delta `.debug$S` | `x15-x360-ab.log`; comparator runs a **positive control first and refuses to report** without detecting a known difference |
| d2 | objdiff position of touched TUs | ✅ **ALL SCOREABLE** — `default/Character`, `default/CharDriver`, `default/CharWeightable`, `default/CharWeightSetter`, `default/CharIKHand`, `default/BandCharacter` all present in `objdiff.json`. No TU here is unscoreable | stated, not implied |
| e | PNG determinism ×2 on every cited image | ✅ **PASS** — all four `cmp`-identical | §10 |
| f | Prior evidence non-regressed vs **artifacts** | ✅ **PASS** — default club frame **BYTE-IDENTICAL** to X14's `x14-V-band-club-focus.png`; default arena centroids reproduce X14's four rows exactly | §10 |

⚠ **The `.debug$S`-only delta is the expected residue** (embedded build path /
timestamp), the same signature X14 reported.

⚠ **Read the gate's own verdict line, not the pipeline exit code** — carried from
X12/X13/X14: `grep -c SKIP` exits 1 on zero matches, so the failure code *is* the
0-SKIPs result.

⚠ **The `--revert` trap was live and was handled.** Building the X360 BASE
objects required reverting two files in-tree. After restoring, `git status
--short` was re-read **and** `git diff --quiet HEAD` was run on both paths before
any measurement. Both confirmed restored.

⚠ **zsh does not word-split unquoted expansions.** A `$OBJS` variable holding six
object paths was passed to ninja as **one** argument
(`ninja: error: unknown target '<all six concatenated>'`). Caught by the error;
recorded because the charter names it and it still fired.

---

## 10. Owed work / handoff

| item | why | owner |
|---|---|---|
| ★★★ **Make the native `~ObjectDir` cascade preserve consumer invariants** | §3: the single-point repair for the whole class. Every `ObjOwnerPtr` seeded `(this, this)` whose invariant is restored only in `Replace()` is exposed, and this lane found two by walking into them. ⚠ NOT a naive "fire Replace from `NullifyObj`" — that skip is load-bearing against delete-this in MessageTask/ScriptTask/PropertyTask/DirLoader. A narrower option: have `NullifyObj` restore an ObjOwnerPtr whose owner *is* its referent. | X16 |
| ★★★ **Enumerate the rest of the class** | §3.4: two sites are not a proof there are only two. Mechanical: grep for `ObjOwnerPtr<…> mX(this, this)` and check whether each class's invariant is restored only in `Replace`. | X16 |
| ★★★ **Keep the `CharWeightSetter`s alive across the merge** | §5.3: this is what makes the polled POSE untrustworthy, and it is upstream of `Poll()`. Fixing it is the difference between "animation runs" and "animation is right". | X16 |
| ★★ **Then retire X14's driver-side call** | §4.3: one line, with the A/B already built (`RB3_BAND_POLL=only`). Blocked only on the pose being validated. | X16 |
| ★★ **`OutfitConfig` registration** | §7.1: call `OutfitConfig::Init()` at `milo_object_factories.cpp:467`, then pay 48 symbols — 37 mechanical, 11 a real port. Price verified today. | X16 |
| ★★ **The 7 skipped meshes** | §7.2: exact bone list now in the log. Start with `tightdistressedpants_resource.1.mesh` — one bone. Handle **both** naming conventions. | X16 |
| ⚠ **`ObjPtrList` NULL-entry defect** | unchanged since X12 | its own lane |
| ⚠ **`mtx.cpp:77` alias hazard, Direction-B rows, orphans, `BandCamShot`** | carried, untouched | as before |
| ⚠ **Engine CR: none filed** | this lane needed no engine change | — |

---

## 11. Recommended X16 shape

1. ★★★ **A predecessor's mechanism read off the source is a hypothesis; a
   predecessor's *retraction* is a warning.** X14's cause was plausible and
   wrong. X13's retraction was the single most useful sentence in the ladder for
   this lane — it is why the seated-drummer frame is reported as undecided rather
   than as the headline it looked like.
2. ★★★ **Fix the invariant where it is broken, not where it is read.** Two guards
   unblocked `Poll()`, and the third site is somewhere in the tree waiting. The
   guards are correct and retail-equivalent; they are still symptomatic.
3. ★★ **Make the census differential before you make it detailed.** "The band has
   N nulls" was uninterpretable. The crowd column made it a finding in one run.
4. ★★ **A deleted binary and a passing grep look identical.** Three of my
   measurements were `rc=127`. Check that the instrument ran before reading what
   it said.
5. ★ **`typeid(T1)` in a template probe.** One token converted "11,494 nulls, is
   ours among them?" into "ours is provably not among them."

---

## 12. Evidence

All under `/home/free/tmp/laneX15/evidence/`.

| file | what it shows |
|---|---|
| `x15-objref2.log` | 11,494 empty-name ObjRef nulls with `typeid(T1)` — **no `CharWeightable`** — §2a |
| `x15-wo2.log` | `CharWeightable::Load` resolving `left_hand.weight` / `right_hand.weight` every time — §2b |
| `x15-census3.log` | the **differential** weight-owner census, band vs crowd control — §2.1 |
| `x15-nullify.log` | `NullifyAllRefs` firing on the rigging `CharWeightSetter`s, + pointer identity — §2c, §3.1 |
| `x15-dirdeath.log` | the `~ObjectDir` backtrace: `FileMerger::PostMerge` ← `BandCharacter::StartLoad` — §3.2 |
| `x15-A-default-club-focus.png` | default arm — **byte-identical to X14's `x14-V-band-club-focus.png`** (determinism ×2) |
| `x15-B-poll-club-focus.png` | `Poll()` arm, same camera — the posed band, drummer seated (determinism ×2) |
| `x15-C-default-arena-focus.png` / `x15-D-poll-arena-focus.png` | the same pair in `arena_01` (determinism ×2) |
| `x15-D_base_ha.log` / `x15-E_poll_ha.log` | the centroid + recompose A/B — §4.2, §5.1 |
| `x15-F_poll_enter.log` | `Enter()` control making it worse — §5.2 |
| `x15-M_hairnames.log` | the 7 skipped meshes with **every unresolved bone named** — §7.2 |
| `x15-x360-ab.log`, `x15_sec_cmp.py`, `ab/*.{BASE,FIX}` | X360 `.text` A/B over 6 TUs, positive control first — gate (d) |
| `x15-gate.log` | native gate PASS 18/18, rc=0, 0 SKIPs — gate (a) |
