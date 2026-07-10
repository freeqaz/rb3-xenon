# "Sentinel-init ctor family" diagnosis — 2026-07-10

Scanner v1/v2 flagged ~8 ctors/dtors as "ours-missing self-pointing intrusive-list
sentinel init". **That reading is wrong.** The r30-based stores are member inits, and
the "self-pointing" `addi r11,r30,0x20; stw r11,...` rows are actually
`addi r11,r30,0x20` (= &member) followed by `stw r11, 0x50(r31)` (EH homing of the
inlinee's `this` into the FRAME, not into the object) plus a *vtable* store
`stw r11, 0x20(r30)` where r11 = `??_7ObjPtr@...` — i.e. **retail inline-expands the
`ObjPtr<T>::ObjPtr(Hmx::Object*, T* = 0)` constructor** (3 field stores + vtable
install + EH-home of &member), while **our build emits an out-of-line
`bl ??0?$ObjPtr@...`** at the same site.

## Evidence

- `RndMorph::RndMorph()` (default/Morph, 90.5%): our base side idx 45-50 =
  `li r5,0; addi r3,r30,0x20; addi r4,...; bl ??0?$ObjPtr@VRndMesh@...`; retail
  (fn_82471CC8 = 0x82338424 in Morph.s) has the ctor body EXPANDED:
  `stw r11,0x24(r30)` (mOwner = vbase+4), `stw r29,0x28(r30)` (mObject=0),
  `lis/addi r11, lbl_82010C4C` (= ??_7ObjPtr<RndMesh>), `stw r11,0x20(r30)` (vptr),
  plus `addi r11,r30,0x20; stw r11,0x50(r31)` (EH home). No AddRef — folded (ptr=0).
- Same shape confirmed: RndMeshAnim ctor + fn_82456EB8 (MeshAnim.s), TexBlendController,
  BandCharacter, RockCentral, Crowd all reference lbl_82010C4C inline.
- `FileStream::~FileStream` (90.5%) is a SEPARATE sub-mechanism: retail inlines
  `DeleteChecksum()` as `lwz r3,0x20(r30); bl OggFree; li 0; stw 0x20; stw 0x24`
  (direct free call, NO null check, trivial dtor), ours emits `bl DeleteChecksum`
  because our body is `delete mChecksumValidator` (nontrivial-dtor delete path is
  too fat to inline). Not ObjPtr; TU-local fix.

## The member type

All ctor-family members are `ObjPtr<T>` (retail layout {vptr@0, mOwner@4, mObject@8},
base ObjRefConcrete<T>): RndMorph::mTarget 0x20 ObjPtr<RndMesh>,
SharedGroup::mPollMaster 0xc ObjPtr<WorldInstance>, RndAnimFilter::mAnim 0x10
ObjPtr<RndAnimatable>, RndMultiMeshProxy::mMultiMesh, NgSpotlightDrawer::mSavedCam,
CharDriver::mBones/mClips/mTestClip/mDefaultClip, CharIKFoot::mFootBone/mData/mMe.

## Why retail inlines and we do not

Our header (src/system/obj/ObjPtr_p.h, retail path) has ONE ctor:
`ObjPtr(Hmx::Object* owner, T* ptr = nullptr) : ObjRefConcrete<T>(owner, ptr)
{ if (mObject) mObject->AddRef(this); }`. The body contains a branch + call
(+ a virtual-base adjust `T*`->`Hmx::Object*` for vbase-derived T — see our emitted
COMDAT: `lwz r11,4(r5); lwz r11,4(r11); add r11,r11,r5; addi r3,r11,4; bl AddRef`),
and MSVC /O1 /Ob2 NEVER inlines it — every site is a `bl` in our build.

Retail per-site behavior is inconsistent with any single-ctor phrasing:
- SampleZone ctor (fn_8270CD10, our SampleZone.cpp = 100% matched) does
  `li r5,0; bl fn_8270B9A8` — out-of-line 2-arg ctor WITH the `if (ptr) AddRef`
  branch, even though ptr is a constant 0 at the site.
- RndMorph/MeshAnim/etc. sites inline a body with NO branch/AddRef at all.

MSVC cannot both fold-inline and not-fold-call the same tiny function at two
identical (ptr=0-const) sites under one set of flags. The consistent explanation:
**retail's 360 header had TWO constructors** —
`ObjPtr(Hmx::Object* owner)` (no ptr, no AddRef — trivially small, always inlined
by /Ob2: exactly the 3-store+vtable expansion observed) and
`ObjPtr(Hmx::Object* owner, T* ptr)` (branch + AddRef — never inlined at /O1,
always a bl). Which one a site uses is a source-level per-site fact (arg count).
Oracle corroboration: rb3-Wii SampleZone.cpp writes `mSample(obj, 0)` (2-arg,
= retail bl site) and rb3-Wii StandardStream ChannelParams writes `mFxSend(0, 0)`
(2-arg; our ChannelParams is 100% matched with a bl); the family ctors are written
1-arg in DC3 (`mTarget(this)` etc., = retail inline sites). rb3-Wii Morph/SharedGroup
are 2-arg in the Wii DEV branch but retail-360 inlines there — branch drift; the 360
retail phrasing is recovered from the 360 binary itself (inline = 1-arg).

## Population (scan of all our .obj relocs to 2-arg ObjPtr ctors)

876 refs / 307 distinct referencing fns / 118 instantiations; only ~27 referencing
fns are in mapped units. Currently matched-100 referencing fns (must KEEP the bl —
their sites must stay/become explicitly 2-arg): SampleZone ctor,
BandWardrobe ctor (already `(this,0)`), CharInterest ctor (`mDartRulesetOverride(this)`
— must flip to `(this,0)`), StandardStream::ChannelParams ctor (`mFxSend(nullptr)` —
must flip to `(nullptr, nullptr)`; Wii oracle literally has `mFxSend(0,0)`).
StreakMeter::SyncObjects (99.6) already writes `(this, 0)` locals.
Family gains expected in: RndMorph 90.5, SharedGroup 90.4, RndAnimFilter 87.8,
CharDriver 85.8, CharIKFoot 85.2, RndMultiMeshProxy 92.7, NgSpotlightDrawer 92.4,
plus candidates CharFaceServo 93.3, RndParticleSysAnim 91.7, RndGenerator 81.3,
CharBoneOffset 81.3, RndParticleSys 78.3, CharEyes 75.4, RndPartLauncher 74.9,
CharLookAt 73.3, CharIKRod 71.3, UIListArrow 61.2, CamShotCrowd 56.9,
CharNeckTwist 43.9 (all reference the ctor and are sub-100).

## Fix being trialed (worktree wt-sentinel-family)

1. Header split (src/system/obj/Object.h + ObjPtr_p.h, retail #else path only):
   add `ObjPtr(Hmx::Object* owner);` = `: ObjRefConcrete<T>(owner, nullptr) {}`
   (no AddRef — behaviorally identical, ptr is null); remove the `= nullptr`
   default from the 2-arg ctor.
2. Site protection: CharInterest `(this, 0)`; ChannelParams `(nullptr, nullptr)`;
   SampleZone `(owner, 0)` (Wii-oracle-faithful).
3. Fleet-wide A/B gate: fresh_report before/after, measure_delta strict gate +
   manual fuzzy comparison; iterate per-site arg-count flips for any regressors.

## Trial state at quota-pause (2026-07-10, from transcript review)

The trial agent applied the two-ctor header split + 3 site protections
(CharInterest/ChannelParams/SampleZone kept on the 2-arg form) — 5 files,
**uncommitted**, preserved in worktree `~/tmp/wt-sentinel-family` (branch
`sentinel-family`, 0 commits ahead of main). Killed at the first smoke test:

- **Mechanism CONFIRMED**: with the 1-arg ctor present, the inline fires at the
  RndMorph site (no more `bl`).
- **But the expansion is scrambled**: RndMorph regressed 90.5% → 66.9% — wrong
  regalloc (r10↔r11) and vtable/store ordering vs retail's expansion.
- Fleet A/B never ran. **Do NOT land the worktree as-is.**

Next step for a resume: iterate the 1-arg ctor body/init-list phrasing until the
RndMorph site's store/vtable ORDER byte-matches retail (member-init order and
the vbase adjust placement are the knobs), THEN run the fleet-wide
zero-strict-regression A/B per the campaign doc §T3 gate. Population if it
works: ~20 family fns at 43–93% (refs list: `~/tmp/sentinel/objptr_ctor_refs.txt`,
876 `??0?$ObjPtr` sites tree-wide). Full review of both killed agents:
`~/tmp/spill_incomplete_review.md`.

Separate TU-local spin-off (NOT ObjPtr): `FileStream::~FileStream` (90.5%) —
retail inlines a trivial `DeleteChecksum()` (`OggFree` + zero two fields, no
null check); our `delete mChecksumValidator` emits the fat nontrivial-dtor
delete path out-of-line. Fix in FileStream.cpp phrasing. **Still live** — this
one is independent of the two-ctor question below and was never attempted.

## RESOLUTION (2026-07-10, Opus resume): CLOSED — at-limit, NOT source-recoverable

The resume agent ran the full fleet A/B and the lead is **dead as a source
change**. Two independent root causes, either one fatal:

1. **Over-application (the killer).** Retail emits the out-of-line `bl` (2-arg
   AddRef ctor) at the *majority* of `mX(this)` member-init sites — e.g.
   `CharBone::CharBone() : … mTarget(this)`, one of 98 functions that were at
   **100%** via main's single 2-arg-default (bl) ctor — and inline-expands only
   at a *minority* (the RndMorph family). **Both are written identically as
   1-arg `mX(this)` in source.** So the inline-vs-`bl` choice is MSVC's
   per-caller `/Ob2` inlining heuristic, *not* an arg-count fact the source can
   encode. Adding the 1-arg ctor forces inline at *every* 1-arg site, so the
   `bl`-majority that was matching regresses. This is inherent to adding the
   ctor at all — independent of any RndMorph phrasing.
2. **Schedule mismatch even where retail inlines.** Of 15 predicted family
   ctors, **0 reach 100%** with the inline. The expansion is the right *kind* of
   code but never the right *schedule*: our inline materializes the `??_7ObjPtr`
   vtable ptr and float constants early (holding regs live across the sibling
   `ObjVector`/scalar inits), homes `&member` early, and orders
   mOwner/mObject/vtable differently from retail's `mOwner→mObject→vtable-last`,
   plus a downstream r10↔r11 volatile swap. Whole-tail scheduling divergence,
   not a local store-order lever. RndMorph: 90.54% → **66.86%**.

**Fleet A/B:** strict 15428 → **15340 (−88; 98 regressed, 10 gained)**; fuzzy
code% 17.3702 → 17.3144 (−0.056). Net-negative on both. **HARD GATE FAILED.**

**Per-TU `/D` gate (RB3_MAP_0x1C-style) also NO-GO:** scoping the 1-arg ctor to
just the family TUs fixes the blast radius (root cause 1) but **still produces
zero strict gains** (root cause 2 — RndMorph 66.86 < its 90.54 bl baseline, a
net loss *within* the gate). Only 3 fuzzy-only, report-fragile movers. Not worth
shared-header complexity.

**Disposition:** keep main's single 2-arg-default `ObjPtr` ctor (bl everywhere —
current state; unchanged). The two-ctor observation is a genuine fact about the
retail binary but the per-site inline/bl split is a compiler heuristic and the
inline schedule is permuter/at-limit-class per-ctor. **Do not re-hunt.** The 3
site protections (CharInterest/SampleZone/StandardStream) are no-ops without the
split. Patch preserved for the record only at
`~/tmp/spill_patches/objptr-two-ctor.patch` (do NOT apply); worktree removed.
RndMorph reported at_limit@90.54 in decomp.db.
