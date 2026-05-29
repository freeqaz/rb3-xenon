# Engine matching wall = a few foundational base-class layout bugs (2026-05-29)

**Status:** verified finding from the struct-offset sweep (5 worktree agents +
orchestrator Ghidra verification). **This redirects engine matching strategy.**

## The headline

The ~200+ engine "near-miss" functions (80–99.99% normalized, the
`scripts/permuter_targets.py` queue) are **NOT independent leaf-struct bugs and
NOT permuter-class.** A parallel sweep of 5 unit-clusters (CharClip, EventTrigger,
Geo, Instance/CharBoneDir, LightHue/LightPreset/Env_NG) found that **essentially
every near-miss cascades from a small set of SHARED base-class layout
divergences** vs retail. No safe leaf fix exists for these — the leaf agents
correctly landed **+0** (a shared-base change regresses the whole 1595 baseline,
so they flagged rather than landed). Fixing the base classes is the real lever;
each cascades to many near-misses but is high-blast-radius and must be done
serially with full no-regression + native-build validation.

## The foundational bugs (with evidence)

### 1. ObjRef / ObjPtr / ObjOwnerPtr family — smart pointers are 8 bytes too big
**Biggest cascade (every class with `ObjPtr`/`ObjOwnerPtr` members).** Our code
(`src/system/obj/Object.h`) conflates two layouts retail keeps separate:
- **Ring node** (inside `Hmx::Object::mRefs`): retail = non-polymorphic
  `{next@0, prev@4, mObject@8}` = **0xc**. (Confirmed: ring-free `fn_82451A48`
  reads `next@0` and frees 0xc-byte nodes; matches the existing `OBJREF_VIRTUAL`
  model that already makes plain `ObjRef` 0x8.)
- **Smart pointer** (`ObjOwnerPtr`/`ObjPtr` as a class member): retail =
  **polymorphic, vtable-first** `{vtable@0, mOwner@4, mObject@8}` = **0xc** with
  **no next/prev fields**. (Confirmed by Ghidra decompile of the `ObjOwnerPtr`
  copy ctor `fn_82489268`: stores vtable@0, copies field@4, copies mObject@8,
  then ring-inserts via virtual dispatch.)

Our `ObjRefConcrete : ObjRef` carries `{next@0, prev@4, vptr@8, mObject@0xc}` =
0x10, and `ObjPtr`/`ObjOwnerPtr` add `mOwner@0x10` → **0x14**. That is +8 vs
retail's 0xc smart pointer (it has next/prev it shouldn't) AND wrong field order
(retail is vtable-first).
- **Evidence across units:** EventTrigger `Anim`/`ProxyCall`/`HideDelay` all +8;
  Instance `PropSync<WorldInstance>` reads `mObject` at ObjDirPtr+0x4 (target) vs
  +0xc (ours); SharedGroup `mPollMaster.mObject` at +0x14 vs +0x18.
- **Fix (NOT a one-liner):** structurally separate the polymorphic smart-pointer
  (`{vtable, mOwner, mObject}`=0xc) from the non-poly ring node
  (`{next, prev, mObject}`=0xc). Touches ring insert/remove, the `Object` dtor,
  ReplaceRefs, and the native build. **Dedicated careful session; validate hard.**

### 2. ObjectDir is 4 bytes too small
`src/system/obj/Dir.h`. `CharBoneDir::mRecenter` lands at **0xa0 (target) vs 0x9c
(ours)** — ObjectDir's non-virtual portion is one 4-byte field short. DC3's layout
is also 0x9c → a genuine **RB3-vs-DC3 version difference** (RB3 has an extra field
DC3 lacks). Cascades to all ObjectDir subclasses. **Fix:** identify the missing
4-byte field (needs Ghidra RE of ObjectDir; RB3-Wii's `mInlineProxy`/proxy region
is a candidate but Wii layout differs).
- Evidence: `CharBoneDir::Copy` `lwz 0xcc` (target) vs `0xc8` (ours) for
  `mMoveContext`; dtor thunks `subi 0x100` (target) vs `0xfc` (ours).

### 3. RndGroup multiple-inheritance sub-object offset
`src/system/rndobj/Group.h`. `RndGroup : RndAnimatable, RndDrawable,
RndTransformable`. The `RndTransformable` sub-object is at **0x34 (target) vs 0x50
(ours)** — a **−0x1c (−28)** delta → `RndAnimatable`/`RndDrawable` sizes (or the
MI vbase layout) are wrong. Cascades to all RndGroup users.
- Evidence: `SharedGroup::MakeWorldSphere` & `GetDistanceToPlane` compute
  `mGroup+0x34` (target) vs `+0x50` (ours).

### 4. Hmx::Object vtable has one extra slot
`src/system/obj/Object.h`. `SetName` dispatches at **vtable+0x40 (target) vs +0x44
(ours)** → our `Hmx::Object` has **one extra virtual** before `SetName` (vtable
index 16 vs 17). Cascades to all virtual dispatch through Object. **Fix:** identify
& remove/reorder the extra virtual (needs the retail vtable order via Ghidra).
- Evidence: `ObjectDir::New<ObjectDir>` `lwz r11,0x40,r11` (target) vs `0x44`.

### 4b. Hmx::Object vtable — PRECISE fix (ContentMgr agent, corroborated)
Two off-by-one slot errors that net to the observed Handle@slot6 / SetName@slot16:
- **We're MISSING `IsDirPtr()` as a virtual** in Object's vtable (retail has it ~slot
  3). In our X360 build `IsDirPtr` is `OBJREF_VIRTUAL` (= non-virtual) on `ObjRef`.
- **We have an EXTRA `InitObject()`** virtual on `Hmx::Object` (a DC3 addition retail
  lacks) before SetName.
Net retail: +IsDirPtr (Handle 5→6) − InitObject (SetName 17→16). **CAVEAT:** making
`IsDirPtr` virtual naively re-adds a vtable to `ObjRef` (breaks the tuned 0x8 ObjRef
/ 0x28 Object model + interacts with bug #1). The correct fix likely declares
`IsDirPtr` virtual on **Hmx::Object** (and ObjRefOwner) without re-polymorphizing the
plain `ObjRef` ring node — verify the exact class each vtable slot belongs to before
editing. Removing `InitObject` is the clean half.

### 5. String / FilePath is 0xc in retail, ours is 0x8
`src/system/utl/Str.h` (String) — **huge blast radius (every class with a String/
FilePath member).** Retail `String` is 3 words `{vptr@0, capacity@4, mStr@8}` = 0xc;
ours is `{vptr@0, mStr@4}` = 0x8. Verified from the String ctor `fn_82798E18`
(stores vptr@0, 0@4, gNullStr@8) and resize `fn_82798E68` (capacity@4, mStr@8).
Propagates +4 to `mKeys` in every LightHue near-miss and to `FilePath` everywhere.
**ROOT CAUSE (ContentMgr agent):** our DC3-derived `String` uses `FixedString +
TextStream` multiple inheritance → `{FixedString::mStr@0, TextStream vtable@4}` = 8.
Retail/rb3-Wii is `class String : public TextStream { uint mCap; char* mStr; }` →
`{vtable@0, mCap@4, mStr@8}` = 0xc. Fix = adopt the rb3-Wii single-inheritance
layout. (Note: Hmx::Object's `mName` String currently nets to the right 0x28 total
via a compensating field-order shift — so fixing String must re-check Object.)
**Found by LightHue + ContentMgr sweep agents.**

### 6. MILO_ASSERT not no-op'd — VERIFIED +4 (worktree A/B)
`src/system/os/Debug.h`. Retail compiled `MILO_ASSERT` out (CLAUDE.md: only 41
.cpp assert strings, all 3rd-party); ours emits the check + `MakeString(__FILE__,
line,#cond)` failer call. **Tested in a clean worktree: guarding the assert family
under `#ifdef HX_NATIVE` and no-op'ing the match build (`((void)sizeof(!(cond)))`)
gave +4 net (1595→1599), no regression.** Modest because most assert-containing
fns have other diffs too — but it's correct + foundational (unconfounds future
work). NOT landed to main yet (whole-engine change; got clobbered by a concurrent
permuter run in the shared main tree — land in a coordinated/quiet moment). The
exact +4-vs-bigger question may improve with POOL_OVERLOAD (#7) fixed too.

### 7. POOL_OVERLOAD passes debug args; retail passes null/garbage
`src/system/utl/PoolAlloc.h`. Our `POOL_OVERLOAD` passes `__FILE__`, line, and
`#class` to `PoolAlloc`; retail passes null/garbage (no debug strings). Affects
pool-allocating ctors (e.g. `operator>>(BinStream, BSPNode*&)`). A release-mode
macro variant. **Found by the Geo sweep agent.**

## How to attack (priority order, EV)

Do **one at a time**, each in a dedicated `scripts/setup_worktree.sh` worktree:
1. **#4 Hmx::Object vtable** — most surgical (reorder/remove 1 virtual), but
   high reach. Identify the extra virtual via Ghidra vtable dump, fix, full
   rebuild, check `measures.matched_functions` (must rise, not fall), check native
   build. Revert if net-negative.
2. **#2 ObjectDir +4** — add 1 field; medium reach. Same validation.
3. **#3 RndGroup MI** — fix RndAnimatable/RndDrawable sizes; medium reach.
4. **#1 ObjPtr family** — biggest cascade but an architectural refactor; do last,
   most carefully, expect to also fix the native build.

**Validation protocol (mandatory for each):** isolated worktree → full
`./tools/ninja-locked` → `measures.matched_functions` after ≥ before (else
`git checkout` the header) → spot-check the native build doesn't break → only then
land to main. Because reach is large, a correct fix should produce a *big* jump;
a wrong one regresses — the no-regression gate makes attempts safe.

## Post-String landscape (2026-05-29, main @1697) — the queue + next levers

The String +94 landing exposed a large near-miss queue: **979 fns at 80-99.99%
normalized, 721 of them at 99%+** (one/few diffs from matched). They cluster by
the remaining base-class bugs, in EV order for the next waves:

1. **Hmx::Object vtable + inline-TypeProps layout** (in flight, `sweep-obj`) —
   cascades to every Handle/SetName dispatch + every Object-subclass member access.
2. **RndDrawable / RndTransformable / RndPollable MI chain** — the BIG rendering
   cascade. The bulk of the queue derives from it: LightPreset (63), Gen (33),
   Part (33), MeshAnim (29), TexBlender (28), DepthBuffer3D (28), Group (19),
   MatAnim (16), + UIComponent (UIScreen). Same MI-layout class as bug #3
   (RndGroup 0x34-vs-0x50). Do AFTER Object (these layouts sit on Object).
3. **ObjPtr family 0xc** (#1) — every ObjPtr member; leaf-workaround where raw.
4. **MILO_ASSERT** (#6, verified +4) + **POOL_OVERLOAD** (#7) — re-measure
   post-String (may now be larger), land in a coordinated rebuild.
Non-rendering clusters likely on Object-vtable/ObjPtr: MidiParser (42),
BandDirector (37), Rnd (35), DataFile (28), MidiInstrument (23), CrowdAudio (13).

**Proven cadence:** one base-class fix → big cascade (String +94). Each lever is a
focused worktree agent with the net-positive-or-revert gate; orchestrator lands.

## Why this matters
This converts a vague 200-function engine backlog into **4 concrete, evidenced,
high-leverage targets**, and proves the parallel-leaf-fix approach is futile for
the engine (it's all shared-base). It also means engine matching has a real
ceiling tied to getting these 4 base layouts exactly right — which, once correct,
should cascade-flip a large fraction of the queue at once.

## Refs
- `docs/plans/struct-offset-sweep.md` (the sweep method + the permuter-is-wrong
  finding). `scripts/permuter_targets.py` (the queue).
- Target evidence: `fn_82489268` (ObjOwnerPtr copy ctor, Ghidra), `fn_82451A48`
  (ring-free). Object.h `OBJREF_VIRTUAL` macro (the existing non-poly pattern).
- Memories: `[[project-rb3-xenon-roadmap]]`, `[[feedback-verify-assumptions]]`.
