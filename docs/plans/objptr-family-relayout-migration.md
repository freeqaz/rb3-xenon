# ObjRef / ObjPtr family re-layout — migration design + plan (bug #1)

**Status:** DESIGN DOC for a dedicated, user-owned session. Do NOT attempt this
as a per-cluster autonomous fix — it is a global refactor with ~233-file blast
radius. This doc captures everything three independent agents discovered hitting
this wall (2026-05-29), so the migration session starts from evidence, not zero.

**Why now:** with the easy one-cause clusters landed (LightPreset +28,
CharFaceServo +7, RndBitmap +5, BinStream +11, NgStats +3, Cache_Xbox +1 → main
@1970), the remaining engine near-miss queue is dominated by this single
foundational bug. The Instance/SharedGroup, MemcardMgr, MidiInstrument, and
TexRenderer clusters all traced their residual diffs to it.

---

## 1. Executive summary

Our smart-pointer model (`src/system/obj/Object.h`, `src/system/obj/ObjPtr_p.h`,
inherited from dc3-decomp) collapses **three structurally distinct retail
layouts into one** `ObjRefConcrete : ObjRef`. They happen to be the same *size*
(0xc) for the two scalar cases, so the `f09aab3` keystone (which made ObjPtr
non-poly 0xc and landed **+85**) fixed the *size* cascade — but the **field
order and construction code are still wrong**, so every ObjPtr-heavy ctor/dtor
emits the wrong instruction shape and can't reach 100%.

The three retail layouts that must be separated:

| # | role | retail layout | size | polymorphic? |
|---|---|---|---|---|
| **(a)** | **ring node** — `Hmx::Object::mRefs` head | `{next@0, prev@4, mObject@8}` | 0xc | no |
| **(b)** | **standalone smart pointer** — `ObjPtr<T>`/`ObjOwnerPtr<T>` as a class member | `{vtable@0, mOwner@4, mObject@8}` | **0xc** | **yes (vtable-first)** |
| **(c)** | **list/vec node** — `ObjPtrList`/`ObjPtrVec` element | `{mObject@0, next@4, prev@8, mOwner@c}` | 0x10 | no, mObject-**first** |

Our current code makes **(b) look like (a)** (`{next,prev,mObject}`, no vtable,
no mOwner) and makes **(c)** a different wrong shape (mObject not first, carries
an extra ring). The migration is to give each role its own correct type.

---

## 2. Current model (exact, as of main @1970)

From `src/system/obj/Object.h`:

```
class ObjRef {                       // line 62
    ObjRef *next;   // 0x0 (retail) / 0x4 (native, after vptr)
    ObjRef *prev;   // 0x4 (retail) / 0x8 (native)
    // methods gated by OBJREF_VIRTUAL: `virtual` under HX_NATIVE, EMPTY for X360
};
template<class T1,class T2=ObjectDir>
class ObjRefConcrete : public ObjRef {   // line 234
    T1 *mObject;    // 0x8 (retail X360) / 0xc (HX_NATIVE)
};
template<class T> class ObjPtr      : public ObjRefConcrete<T> {  // line 273
    Hmx::Object *mOwner;  // 0x10  — HX_NATIVE ONLY (absent in X360 build)
};
template<class T> class ObjOwnerPtr : public ObjRefConcrete<T> {  // line 309
    ObjRefOwner *mOwner;  // 0x10  — HX_NATIVE ONLY
};
```

So in the **X360 match build**: `ObjPtr<T>` == `ObjOwnerPtr<T>` ==
`{next@0, prev@4, mObject@8}` = **0xc, non-polymorphic, ring-fields-inline,
no mOwner**. That is shape **(a)**, not the retail standalone shape **(b)**.

`ObjPtrList` / `ObjPtrList::Node` (line 543-596) and `ObjPtrVec` (line 349-529)
carry their own `Node` with `mOwner@0x10, next@0x14, prev@0x18` on top of the
ObjRefConcrete base — i.e. mObject is NOT first and there's a redundant ring.
That is the wrong shape for **(c)**.

`OBJREF_VIRTUAL` (line 57-59): `virtual` under `HX_NATIVE`, **empty** for X360.
This macro is the existing lever that makes plain `ObjRef` non-poly (0x8) for the
match build. The migration must keep ObjRef (ring node) non-poly while making the
**standalone smart pointer** polymorphic — these are now the same class, which is
the root of the problem.

`Hmx::Object` (line ~1240-1265, X360 layout): `TypeProps mTypeProps@0x4` inline
(vtable@4, mMap@8, mOwner@c), `mTypeDef@0x10`, `mRefs` ring head, `mName`,
`mNote`, etc. — total **0x28**. `mRefs` is shape (a). Changing ObjPtr must NOT
disturb the tuned 0x28 Object or the 0x8 plain ObjRef.

---

## 3. The three retail layouts — evidence

### (a) Ring node `{next@0, prev@4, mObject@8}` = 0xc
- The `Hmx::Object::mRefs` intrusive ring head. Confirmed by the ring-free
  routine `fn_82451A48` (reads `next@0`, frees 0xc-byte nodes).
- Our current `ObjRefConcrete` already matches this. **No change needed for (a).**

### (b) Standalone smart pointer `{vtable@0, mOwner@4, mObject@8}` = 0xc, polymorphic
- **Evidence — `ObjOwnerPtr` copy ctor `fn_82489268`** (Ghidra): stores vtable@0,
  copies field@4 (mOwner), copies mObject@8, then ring-inserts via **virtual
  dispatch** (not via inline next/prev writes).
- **Evidence — SharedGroup ctor `??0SharedGroup@@QAA@PAVRndGroup@@@Z`**
  (`build/45410914/asm/Instance.s:1845`; cross-checked vs DC3's matching ctor
  `dc3-decomp/build/373307D9/asm/system/world/Instance.s:3324`): the **32 bytes
  our build omits** (212 base vs 244 target, 86.9%) are exactly the `mPollMaster`
  ObjPtr construction — retail stores `vtable = 8207DCF8+0x4`, `mOwner = this+vbase+4`,
  `mObject = 0`, then the `mPolls` list starts at **0x18** (not 0x20). Our non-poly
  ObjPtr emits neither the vtable store nor the mOwner store, so the ctor is short.
- **Evidence — Instance ctor `fn_824D8698`** (the stopped agent was examining this;
  see §9).
- **Corroboration — MidiInstrument SampleZone** (independent agent): the standalone
  `SampleZone.cpp` unit's real ctor `fn_8270CD10` + Load `fn_8270CDC0` show
  `mSample` (ObjPtr) is **0xc** with the rb3-Wii field offsets — confirming retail
  ObjPtr is 0xc, and that our `SampleZone.h` currently uses the HX_NATIVE 0x14 size
  (a separate but related bug; see §10).

### (c) List/vec node `{mObject@0, next@4, prev@8, mOwner@c}` = 0x10, mObject-first
- **Evidence — `AddPolls@SharedGroup`** (Instance unit): target reads
  `mObject@0x0, next@0x4`; ours reads `0xc / 0x14` (our Node has a vtable + ring it
  shouldn't, and mObject is not first).
- **Corroboration — CharBonesMeshes (LANDED, `8ad34bd`)**: `mMeshes` was
  `ObjPtrVec<RndTransformable>` (0x1c, leading vtable + std::vector + mOwner +
  EraseMode + ObjListMode) but retail is `ObjVector<ObjOwnerPtr<RndTransformable>>`
  (a plain `std::vector` subclass, **0x10**, no vtable) at 0x54. This is the (c)
  family — a hint that retail's container-of-ObjPtr uses a thinner node than our
  `ObjPtrVec`.

---

## 4. Why this is bug #1 and not a leaf fix

`ObjPtr<>` / `ObjOwnerPtr<>` are referenced by **233 source files**. Any change to
their layout/construction ripples through: ring insert/remove (`AddRef`/`Release`),
the `Hmx::Object` dtor (walks `mRefs`), `ReplaceRefs`, `ObjPtrList`/`ObjPtrVec`
node access, every `SYNC_PROP`/`PropSync` that touches an ObjPtr member, and the
**native build** (`HX_NATIVE` keeps a polymorphic 0x14 form with a real `mOwner`).
There is no Instance-scoped (or any-cluster-scoped) subset — the baseclass doc
flags it: *"NOT a one-liner… dedicated careful session; validate hard."*

A correct migration should produce a **large positive jump** (many ObjPtr-heavy
ctors/dtors across the 233 consumers flip at once); a wrong one regresses below
the current 0xc-non-poly baseline. The no-regression gate makes the attempt safe
but the iteration cost (full-tree rebuilds, compile-error whack-a-mole across 233
files) is why it needs a dedicated session.

---

## 5. Migration plan (phased)

Each phase in its own `scripts/setup_worktree.sh` worktree, full
`./tools/ninja-locked` rebuild, measure `measures.matched_functions`, keep only if
net-positive. **Do the phases in this order — earlier phases de-risk later ones.**

**Phase 0 — instrument & baseline.** In a fresh worktree, record baseline matched
count. Dump the current `sizeof`/offsets of `ObjPtr<X>`, `ObjOwnerPtr<X>`,
`ObjPtrList<X>::Node`, `ObjPtrVec<X>::Node` for a few representative X via
`tools/struct_db.py` or a static_assert probe, so you can prove each phase changed
exactly what you intended.

**Phase 1 — split the *type identity* without changing X360 layout.** Introduce
three distinct templates/classes (e.g. `ObjRefRing` (a), `ObjPtrBase`/`ObjPtr`
(b), `ObjPtrListNode`/`ObjPtrVecNode` (c)) so each role has its own type, but keep
the X360 byte layout identical to today. Rebuild → must be **+0** (pure
refactor, no codegen change). This isolates the risky part (Phase 2) from the
mechanical part. If it's not +0, you changed a layout unintentionally — fix before
proceeding.

**Phase 2 — make the standalone smart pointer (b) polymorphic, vtable-first.**
Change `ObjPtr`/`ObjOwnerPtr` (the standalone member form) to
`{vtable@0, mOwner@4, mObject@8}` with a real vtable and the ring membership done
via virtual dispatch (matching `fn_82489268`). Keep it **0xc** (mOwner replaces the
inline next/prev — same size). Update `AddRef`/`Release`/ring insert to the virtual
form. Rebuild → expect the **big jump** (SharedGroup ctor, every ObjPtr-member
ctor/dtor). Validate hard; check a handful of previously-100% ObjPtr ctors didn't
regress.

**Phase 3 — fix the list/vec node (c) to mObject-first `{mObject@0,next@4,prev@8,mOwner@c}`.**
Rework `ObjPtrList::Node` / `ObjPtrVec::Node` accessors. Re-measure (AddPolls and
container-heavy units). This is where `ObjVector<ObjOwnerPtr<T>>` (the
CharBonesMeshes shape) should be reconciled with the canonical node.

**Phase 4 — companion unblocks (measure each separately):**
- **RndPollable vtable slot order** — `TryPoll@SharedGroup` reads vtable slot 0
  (`lwz r11,0x0(r11)`) vs our slot 1 (`0x4`). A vtable-order fix in the RndPollable
  base.
- **String / ObjectDir +4** (bugs #2/#5) — `Poll`/`GetDistanceToPlane` read a field
  +4 (`0x60` vs `0x5c`, `0x148` vs `0x144`). May already be partly fixed
  post-String-keystone; re-measure.

**Phase 5 — native build reconciliation.** The `HX_NATIVE` build uses a polymorphic
0x14 ObjPtr with a real `mOwner` and ref-counting Nodes. Ensure the new type split
keeps the native build compiling/linking (it has its own `native/` CMake target —
see `project_native_port` memory). Guard native-only members under `#ifdef HX_NATIVE`.

---

## 6. Validation protocol (mandatory)

- One phase per worktree; **full** `./tools/ninja-locked 2>&1 | tee /tmp/objptr_phaseN.log`.
- After each phase: `python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"` must be **≥** the phase's start (Phase 1 must be exactly equal). Else `git checkout -- .` and rethink.
- Because reach is large, keep a per-unit before/after (the `report.json` `units[]`
  array) so a net-positive total that hides a regressed unit is caught.
- Land to main only after the whole sequence is net-positive and the native build
  is sanity-checked. Land via path-limited commits (shared-index race — see
  `project_shared_index_commit_race` memory).

---

## 7. Risks

- **Same-size trap:** (a) and (b) are both 0xc, so a layout mistake won't change
  `sizeof` — it'll silently emit wrong code and regress the diff without a compile
  error. Phase 1's "+0 refactor" checkpoint guards against this.
- **Native build divergence:** the native runtime relies on the polymorphic 0x14
  form; don't break it.
- **vtable identity:** retail's standalone ObjPtr vtable is a specific label
  (`8207DCF8+0x4` region for ObjOwnerPtr; `lbl_82017140`/`lbl_82000980` seen for
  some MidiInstrument ObjPtr elements). MSVC must emit the vtable in the right
  COMDAT — watch for ICF folding surprises.
- **233 consumers:** expect a long compile-error tail in Phase 2; budget for it.

---

## 8. Clusters this unblocks (the payoff)

- **Instance** unit: SharedGroup ctor (86.9%→), AddPolls, TryPoll, Poll,
  GetDistanceToPlane.
- Every **ObjPtr-member ctor/dtor** across the engine (the bulk of the 99.x% queue
  that survived the String + Rnd-MI keystones).
- **MemcardMgr** (after its separate MsgSource-base fix — see §10) and other
  MsgSource/ObjPtr-heavy classes.
- Removes the confounder under many remaining near-misses so the *true* residual
  (regswap/scheduling, permuter-class) becomes visible.

---

## 9. Prior autonomous attempt (stopped) — starting point

A dedicated Opus agent (`objptr-relayout`, task #18) was launched at this and
**stopped early at the user's request** (the user is owning this migration
deliberately). Its worktree is **deliberately preserved** at
`.claude/worktrees/objptr` (branch `objptr-relayout`) as a starting point.
The following is a Haiku review of its transcript + worktree:

**How far it got:** Created the worktree, ran a full baseline build, and completed
the *analysis* phase (read `Object.h`, `ObjPtr_p.h`, the baseclass doc; pulled the
key ctor/dtor asm). It **stopped before making any source edits**, right as it was
about to start implementing the three-way split.

**Baseline it measured:** `matched_functions = **1954**` (Milo engine 19.77%
matched / 31.85% fuzzy; game code 0.68% matched). This is the number to beat.

**Worktree state (preserved, clean):** branch `objptr-relayout` off main `722602a`;
**no uncommitted changes, no commits added** — it's a pristine analysis-only
starting point. Resume by editing `Object.h`/`ObjPtr_p.h` directly there (then
`scripts/setup_worktree.sh` is unnecessary — the worktree already exists and is
buildable).

**Asm/Ghidra evidence it confirmed** (adds one new artifact to §3):
- `fn_824D8698` — Instance/SharedGroup ctor (the keystone spec evidence).
- `fn_82489268` — ObjOwnerPtr copy ctor (vtable@0, mOwner@4, mObject@8 + virtual ring-insert).
- `fn_82451A48` — ring-free walk (reads `next@0`, frees 0xc nodes).
- **`fn_82738050` — `Hmx::Object` dtor ring-walk** *(new — the dtor that iterates
  `mRefs`; Phase 2 must keep this matching when ring-insert becomes virtual).*

**Its stated resume plan** (matches §5 here): (1) make standalone `ObjPtr`/
`ObjOwnerPtr` polymorphic vtable-first `{vtable,mOwner,mObject}`=0xc, non-ring-
inheriting; (2) keep ObjRef/ObjRefConcrete ring node non-poly (already correct);
(3) redefine list/vec Node mObject-first `{mObject@0,next@4,prev@8,mOwner@c}`;
(4) route ring-insert through virtual dispatch on the referenced object; (5)
rebuild incrementally, measuring which of (a)/(b)/(c) drives the gain. It projected
SharedGroup ctor 86.9% → ~100% once the 32-byte vtable+mOwner+init sequence is
restored.

---

## 10. Adjacent base-class walls found the same day (separate levers, not this doc)

These are NOT part of the ObjPtr migration but were surfaced by the same sweep and
share the "dc3-newer-than-RB3 base class" character. Track separately:

- **UIComponent base** (`docs/plans/next-wave-onediff-clusters.md`, task #17):
  retail UIComponent is ~252 bytes + ~9 vtable slots LARGER than DC3's. 15 UI
  units / 79 fns. Ghidra-reconstruct from `fn_827E47F0` + `??_7UIComponent`.
- **MemcardMgr MsgSource hierarchy**: retail `MemcardMgr : MsgSource, ThreadCallback`
  with the `Hmx::Object` subobject TRAILING at 0x8c (ctor `fn_82787030`), not
  Object-primary. Uniform -0x10 skew. Mirrors the concurrent `a847f4f`
  (`MidiParser : MsgSource`). A "route classes onto MsgSource base" lever.
- **SampleZone double-definition**: two structurally different `SampleZone` types
  both mangle to `SampleZone` — MidiInstrument's `vector<SampleZone>` element is
  0x1c (2 poly ObjPtrs + float), the standalone class is 0x50. Plus `SampleZone.h`
  uses the HX_NATIVE 0x14 ObjPtr where retail is 0xc (rb3-Wii offsets). Fixing the
  standalone-unit ObjPtr size is a small win independent of the vector divergence.

---

## 11. References

- `docs/plans/engine-baseclass-layout-bugs.md` — bug #1 canonical writeup (+ #2/#5
  String/ObjectDir, #4 Object vtable).
- `docs/plans/next-wave-onediff-clusters.md` — the one-cause cluster survey + the
  UIComponent lever.
- Source: `src/system/obj/Object.h` (ObjRef@62, ObjRefConcrete@234, ObjPtr@273,
  ObjOwnerPtr@309, ObjPtrVec@349, ObjPtrList@543, Hmx::Object@~1240),
  `src/system/obj/ObjPtr_p.h` (template method bodies).
- Target asm / Ghidra: `fn_82489268` (ObjOwnerPtr copy ctor), `fn_82451A48`
  (ring-free), `fn_824D8698` (Instance ctor), `??0SharedGroup@@QAA@PAVRndGroup@@@Z`
  (`build/45410914/asm/Instance.s:1845`), DC3 cross-ref
  `dc3-decomp/build/373307D9/asm/system/world/Instance.s:3324`,
  `fn_8270CD10`/`fn_8270CDC0` (standalone SampleZone ctor/Load).
- Oracles: rb3-Wii `~/code/milohax/rb3/src/system/obj/` (game-code, named, ObjPtr
  0xc offsets), DC3 `~/code/milohax/dc3-decomp/src/system/obj/` (engine twin — but
  NEWER, this is the divergence source).
- Memories: `[[project-engine-baseclass-layout-wall]]`,
  `[[project-shared-index-commit-race]]`, `[[project-native-port]]`,
  `[[feedback-verify-assumptions]]`.
- Preserved worktree: `.claude/worktrees/objptr` (branch `objptr-relayout`).
