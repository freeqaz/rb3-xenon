# UIComponent missing-virtuals reconstruction — research dossier (2026-06-11)

**Goal:** identify which own-virtuals retail-360 `UIComponent` has that our header
lacks, so the banked RndDrawable Draw-devirt patch
(`docs/decomp/handoff/rnddrawable-devirt-banked.patch`) lands as a true +4 +
cascade instead of net-0.

**VERDICT (high confidence — every claim below is anchored in retail machine
code, not oracle inference):**

Retail UIComponent's own-virtual set is **exactly rb3-Wii's 8 own-virtuals**
(`../rb3/src/system/ui/UIComponent.h`), in Wii declaration order, and does
**NOT** contain DC3's `OldResourcePreload`. Versus our current header
(`src/system/ui/UIComponent.h`, 6 own slots) the delta is:

- **ADD `ResourceCopy(const UIComponent *)`** — new slot BEFORE `SetState`.
- **ADD `CopyMembers(const UIComponent *, Hmx::Object::CopyType)`** — new slot
  AFTER `CanHaveFocus`.
- **ADD `Update()`** — new slot AFTER `CopyMembers` (last own slot).
- **REMOVE the `virtual` from `OldResourcePreload(BinStream &)`** (DC3-only
  virtual; retail has no such slot) — gate `virtual` behind `HX_NATIVE` exactly
  like `DRAW_DC3_VIRTUAL` in `src/system/rndobj/Draw.h:71-75`.
- (slot-neutral, optional phase B) retail UIComponent **overrides
  `SetTypeDef(DataArray *)`** (Object-vbase vtable slot 15 → thunk
  `fn_827DAB68`); rb3-Wii `UIComponent.h:55` has it, DC3/ours dropped it.

Net: +3 own slots − 1 = **+2 slots**, exactly canceling the banked patch's −2
RndDrawable slots (Draw@5, DrawShadow@7). This corrects the roadmap framing
"keeps exactly 2 of rb3-Wii's 4": slot-wise retail keeps **3 of 4**
(ResourceCopy, CopyMembers, Update; the 4th, SetTypeDef, is a slot-neutral
override) and additionally LACKS our DC3-only OldResourcePreload slot — the
observed uniform −8 was the **net** ±2-slot arithmetic.

**The two levers MUST land in the same commit.** Today both sides total 20
primary slots, so all derived-class own-slot vcalls (≥0x50) coincidentally
align (that's where the current 100% matches live). The devirt patch alone
shifts them −8 (the banked regression); the header fix alone shifts them +8.
Together: every slot aligns. Roadmap already says DO NOT split — this extends
that to "do not land the header fix alone either."

---

## 1. The retail vtable group (primary evidence)

### 1.1 Where it came from — NO Ghidra needed (service was down)

The Ghidra MCP (port 8002) was down/restarting throughout; everything below
was recovered from **split-target artifacts already in the repo**:

1. **Retail UIComponent dtor `fn_827DABC0`** asm is in
   `build/45410914/asm/auto_03_827CBF5C_text.s` (extract with
   `awk '/^\.fn fn_827DABC0/,/^\.endfn fn_827DABC0/'`). Its vptr stores give
   the vtable group addresses:
   - primary (RndDrawable-led) vptr @this+0x0 ← `lbl_8211D490+0x14` = **0x8211D4A4**
   - RndTransformable vptr @this+0x24 ← `lbl_8211D490+0x8` = 0x8211D498
   - RndPollable vptr @this+0xd8 ← `lbl_8211D484` = 0x8211D484
   - Hmx::Object vbase vptr ← `lbl_8211D408+0x24` = 0x8211D42C (via vbtable@this+4, entry +4)
   - RndHighlightable vbase vptr ← `lbl_8211D408+0x1C` = 0x8211D424 (vbtable entry +8)
2. **The vtable contents** were read as raw big-endian words from the dtk
   split rdata object **`auto_00_82000400_rdata.obj`** (repo root; section VA
   base 0x82000400, file offset = VA − 0x82000400 + section data offset).
   dtk leaves the original target words in place, so each slot word IS the
   retail function VA. ⚠️ Reusable technique: this obj carries **every retail
   vtable** — it also unblocks the VocalTrackDir::TrackReset
   "ObjectDir-vbase vtable single-slot wall" (roadmap batch-2) that was parked
   as "COFF split objs carry no ??_7". They do — in the auto rdata obj.
3. **Slot identities** by extracting each slot fn's asm from
   `auto_03_827CBF5C_text.s` and matching against rb3-Wii bodies
   (`../rb3/src/system/ui/UIComponent.cpp`), plus
   `scripts/target_symbol_map.json` names for callees.

### 1.2 Retail primary vtable @0x8211D4A4 — 20 slots (next RTTI/COL at 0x8211D4F4)

| slot | +off | retail word (fn VA) | identity | evidence |
|---|---|---|---|---|
| 0 | 0x00 | 0x82285FD8 | RndDrawable::UpdateSphere | position |
| 1 | 0x04 | 0x8270B818 | GetDistanceToPlane (ret 0.0f) | position |
| 2 | 0x08 | 0x82B59210 | MakeWorldSphere (ICF `li r3,0; blr`) | shared-0 stub |
| 3 | 0x0c | 0x82B59210 | CamOverride (ret 0) | shared-0 stub |
| 4 | 0x10 | 0x82465928 | Mats {} (ICF empty `blr`) | shared-empty stub |
| 5 | 0x14 | 0x82465928 | **DrawShowing** {} | matches banked-patch anchor (RndLine tgt 0x14) |
| 6 | 0x18 | 0x82465928 | ListDrawChildren {} | |
| 7 | 0x1c | 0x82B59210 | CollideShowing (ret 0) | |
| 8 | 0x20 | 0x823F3D60 | CollidePlane (real body) | same TU as Draw body fn_823F3A80 |
| 9 | 0x24 | 0x823F4C98 | **CollideList** | matches banked-patch anchor (RndGroup tgt 0x24) |
| 10 | 0x28 | 0x82465928 | DrawPreClear {} | rb3-Wii `rndobj/Draw.h:59` |
| 11 | 0x2c | 0x82465928 | UpdatePreClearState {} | rb3-Wii `Draw.h:60` |
| 12 | 0x30 | **0x827D9FF8** | **UIComponent::ResourceCopy** | §1.3a |
| 13 | 0x34 | **0x827D9178** | **UIComponent::SetState** | §1.3b + Enter vcalls 0x34 |
| 14 | 0x38 | **0x827D8EF0** | **UIComponent::StateSym** | calls map-named `?UIComponentStateToSym@@YA…` fn_827D8DC8 |
| 15 | 0x3c | 0x82B59210 | **Entering** {return false} | ICF ret-0 stub; = PanelDir::Entering tgt-slot 0x3c anchor |
| 16 | 0x40 | **0x827D8FA0** | **Exiting** {return mState==kSelecting} | `lwz r11,0xe0(r3); subi 3; cntlzw` |
| 17 | 0x44 | 0x827AD1F0 | **CanHaveFocus** {return true} | ICF ret-1 stub (map: `?IsLoadConst@IRLoadConst@XGRAPHICS@@` — another ret-true); SetState vcalls 0x44 |
| 18 | 0x48 | **0x827DA0C8** | **UIComponent::CopyMembers** | §1.3c |
| 19 | 0x4c | **0x827DB8C8** | **UIComponent::Update** | §1.3d |

Retail RndDrawable slice = 12 slots (no Draw, no DrawShadow, no Wii
DrawShowingBudget; HAS DrawPreClear/UpdatePreClearState — both oracles have
them). Own region = slots 12–19 = 8 own virtuals. Total 20 = our current 20.

Non-primary tables all already match ours (so the header fix touches ONLY the
primary count): Object-vbase @0x8211D42C = 21 slots (incl. UIComponent
overrides ??_E/SetType/Handle/SyncProperty/Save/Copy/Load/PreLoad/PostLoad as
thunks fn_827DA9xx/fn_827DABxx; **slot 15 = fn_827DAB68 = retail
UIComponent::SetTypeDef override thunk** — ours points to
`?SetTypeDef@Object@Hmx@@` instead); RndPollable @0x8211D484 = 4 slots
{Poll=fn_827DA580, Enter=fn_827D8FB8, Exit=0x82364888 (ICF), ListPollChildren=
0x82465928}; RndTransformable @0x8211D498 = 1 slot (UpdatedWorldXfm {});
RndHighlightable-vbase @0x8211D424 = 1 slot (Highlight thunk fn_827DA9F0).

Our compiled layout for comparison (dump:
`python3 scripts/dump_vtable.py` mis-hits `UIComponentSelectMsg`; use the COFF
sections directly from `build/45410914/src/system/ui/UIComponent.obj` — vtable
sections: `??_7UIComponent@@6BRndDrawable@@@` sec 467 = primary):
slots 0–13 = RndDrawable incl. **Draw@0x14 + DrawShadow@0x1c** (the 2 the
banked patch removes), 14–19 own = SetState@0x38, StateSym@0x3c, Entering@0x40,
Exiting@0x44, CanHaveFocus@0x48, OldResourcePreload@0x4c.

### 1.3 Body identifications (asm ↔ rb3-Wii source)

All extracted from `build/45410914/asm/auto_03_827CBF5C_text.s`; Wii bodies in
`/home/free/code/milohax/rb3/src/system/ui/UIComponent.cpp`.

a) **fn_827D9FF8 = ResourceCopy(const UIComponent *c)** (Wii cpp:103-115):
   direct `bl fn_82735F20` = Hmx::Object::SetTypeDef(c->TypeDef()) (TypeDef
   read at c's Object-vbase+0x14); then **vcall slot 0x48 with (this, c,
   r5=1)** = `CopyMembers(c, kCopyShallow)`; inline strlen of
   mResourcePath.c_str (this+0x138); if len: `fn_822606F0(this+0x124,
   c->[0x128])` = ObjDirPtr assign (mResourceDir); else copies `c->[0x108]` →
   `this->[0x108]` + `bl fn_827FDDA8` on it = `mResource = c->mResource;
   mResource->PostLoad();` (MILO_ASSERTs stripped); ends with **vcall slot
   0x4c (this)** = `Update()`. This single function pins THREE slots
   (0x48=CopyMembers, 0x4c=Update, itself@0x30=ResourceCopy).

b) **fn_827D9178 = SetState(State)** (Wii cpp:79-ish): vcall **slot 0x44** on
   this = `CanHaveFocus()`; if false, `s==kFocused(1) → kNormal(0)` branchless
   (`subi/subfic/subfe/and`); `stw r31, 0xe0(r30)` = mState@0xe0 store.

c) **fn_827DA0C8 = CopyMembers(const UIComponent *, CopyType)** (Wii
   cpp:118-127): null-preserving vbase-adjust of `c` to `const Hmx::Object*`,
   then `bl fn_823E6918` = **`?Copy@RndTransformable@@UAAX…`** (map-named) with
   r3 = this+0xdc, and `bl fn_823F35F8` = **`?Copy@RndDrawable@@UAAX…`** with
   r3 = this+0x28 — i.e. Wii's `RndTransformable::Copy(c,ty);
   RndDrawable::Copy(c,ty);` (callee-this convention = subobject + nonvirtual
   extent + 4: Trans 0x24+0xb4+4=0xdc, Draw 0x0+0x24+4=0x28; same convention
   as the dtor entering with r3=this+0x144). Then ObjPtr assigns
   `fn_827D90F0(this+0xe4, c->[0xec])` mNavRight, `(this+0xf0, c->[0xf8])`
   mNavDown; String assigns `fn_82799140` this+0x118 ← c+0x118 mResourceName,
   this+0x130 ← c+0x130 mResourcePath; ObjDirPtr `fn_822606F0` this+0x124 ←
   c->[0x128] mResourceDir. Member-for-member identical to Wii CopyMembers.

d) **fn_827DB8C8 = Update()** (Wii cpp:233+; the Wii file even carries a
   comment "matches on retail: https://decomp.me/scratch/3ya1L"): strlen
   mResourcePath; if non-empty and mResourceDir.mObject (this+0x128) null →
   `MakeString("%s/%s.milo", …, "default")`-style path build (string-pool lbls
   lbl_82085E08/lbl_8204C8A4) + `fn_8250E2C8` (FileStat/stat) + mResourceName
   assign (`fn_82799518` on this+0x118) + `bl fn_827DB290(this, 0)` =
   `ResourceFileUpdated(false)`; else-branch walks mResource(this+0x108)->…
   and the mMeshes vector (this+0x10c/0x110), with a function-local
   static-guard block (guard word in .data lbl_82DA0017+0x36035 — the Gem-+8
   static-Symbol idiom, relevant when the body is eventually ported).

e) **fn_827D8FB8 = Enter()** (RndPollable-table slot 1; body `this` =
   RndPollable subobject this+0xd8): `bl fn_8240EA08` (RndPollable::Enter),
   `li r11,0; stw r11, 0x2c(r31)` = **mSelected = 0 @ absolute 0x104**,
   `lwz r11,0x8(r31)` = mState@0xe0, `cmpwi 3` (kSelecting), then vcall
   **primary slot 0x34 with r4=1** = `SetState(kFocused)` — independent
   confirmation of SetState@0x34.

### 1.4 ⚠️ Member-layout corrections discovered en route (do NOT skip)

- **mSelected is at 0x104 and mResource at 0x108** — our header
  (`src/system/ui/UIComponent.h:91-92`) and the plan-doc table
  (`docs/plans/ui-base-layout-reconstruction.md` §"verified retail layout")
  have them **swapped** (mResource 0x104 / mSelected 0x108). Proof:
  ResourceCopy copies c->[0x108] and calls PostLoad on it (= mResource);
  the dtor null-checks `[this+0x108]` and calls `fn_827FDEE8` on it; Enter
  stores 0 to 0x104 (= mSelected, and it IS an int — `stw`). Fix the two
  header comments/decl order in the same edit (swap lines so
  `int mSelected; // 0x104` precedes `UIResource *mResource; // 0x108`).
  Blast radius ~0 today (UIComponent unit is pinned only to StaticClassName,
  see §4; no matched body reads either field), but every future
  UIComponent-body port depends on it.
- The plan doc's "0x13c-0x13f mSelected/mState/…" row was already corrected
  once (mState@0xe0); this dossier re-confirms mState@0xe0 (Exiting, SetState,
  Enter all read/write 0xe0).

---

## 2. Exact header edit (`src/system/ui/UIComponent.h`)

Declaration ORDER is load-bearing (MSVC appends new virtuals in declaration
order). Target own-virtual order must be: ResourceCopy, SetState, StateSym,
Entering, Exiting, CanHaveFocus, CopyMembers, Update. Concretely, edit the
block at lines 58-67:

```cpp
    // RndDrawable
    virtual void Highlight() { RndDrawable::Highlight(); }
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    // UIComponent own-virtuals — retail-360 order/set verified from the retail
    // vtable @0x8211D4A4 (20 slots; own region slots 12-19 = 0x30..0x4c), see
    // docs/decomp/research/2026-06-11-uicomponent-virtuals.md
    virtual void ResourceCopy(const UIComponent *);              // slot 12, 0x30
    virtual void SetState(UIComponent::State);                   // slot 13, 0x34
    virtual Symbol StateSym() const;                             // slot 14, 0x38
    virtual bool Entering() const { return false; }              // slot 15, 0x3c
    virtual bool Exiting() const { return mState == kSelecting; }// slot 16, 0x40
    virtual bool CanHaveFocus() { return true; }                 // slot 17, 0x44
    virtual void CopyMembers(const UIComponent *, Hmx::Object::CopyType); // slot 18, 0x48
    virtual void Update();                                       // slot 19, 0x4c
```

and in the protected section (line 80):

```cpp
#ifdef HX_NATIVE
#define UICOMP_DC3_VIRTUAL virtual
#else
#define UICOMP_DC3_VIRTUAL
#endif
    UICOMP_DC3_VIRTUAL void OldResourcePreload(BinStream &);  // DC3-only virtual; retail has NO such slot
```

(Use the same macro placement idiom as `DRAW_DC3_VIRTUAL`, `Draw.h:71-75`.
Note `CopyMembers` was protected on Wii? No — public there; keep public.)

### 2.1 MANDATORY companion edits — derived `OldResourcePreload` overrides

If UIComponent::OldResourcePreload stops being virtual while derived classes
still declare `virtual void OldResourcePreload(BinStream &)`, each such decl
becomes a NEW first-class virtual and inserts a bogus slot into that derived
class (shifting its descendants). Gate ALL of them with the same macro:

- `src/system/ui/UIList.h:106`
- `src/system/ui/UISlider.h:57`
- `src/system/ui/UILabel.h:110`
- `src/system/ui/LabelShrinkWrapper.h:39`
- `src/system/ui/InlineHelp.h:75`  ← InlineHelp unit is 29/38 matched — highest regression risk if missed
- `src/system/hamobj/SongDifficultyDisplay.h:36`, `src/system/hamobj/MeterDisplay.h:28`,
  `src/system/hamobj/MiniLeaderboardDisplay.h:25`, `src/system/hamobj/StarsDisplay.h:42`,
  `src/system/hamobj/HamNavList.h:155` (DC3/hamobj — gate for native-build health)

(Re-grep at implementation time: `grep -rn "OldResourcePreload" src/ --include="*.h"`.)

### 2.2 Audit: derived `Update()` collisions

Adding `virtual void Update()` to the base turns same-signature derived decls
into overrides. Current hits: `src/system/bandobj/MeterDisplay.h:21`,
`src/system/bandobj/CheckboxDisplay.h:20`,
`src/band3/meta_band/AppMiniLeaderboardDisplay.h:44` (all already `virtual`,
all currently-unwired units — MeterDisplay reads src=None 0/1) — becoming
overrides is exactly what retail does (rb3-Wii's whole bandobj display family
overrides UIComponent::Update: ScoreDisplay/MeterDisplay/StarDisplay/
BandButton/InlineHelp/BandHighlight/ReviewDisplay/MicInputArrow/
CheckboxDisplay — see `../rb3/src/system/bandobj/*.h`). Audit non-virtual
`void Update()` decls in UIComponent descendants too
(`grep -rn "void Update()" src/system/ui src/system/bandobj src/band3`) —
hiding stays hiding, but list them in the A/B notes.

## 3. `src/system/ui/UIComponent.cpp` additions (phase A = minimum to compile)

Our cpp is a 164-line skeleton; it lacks all three bodies. The vtable needs
real definitions:

- **CopyMembers** — full port, Wii cpp:118-127 verbatim (members exist; calls
  `RndTransformable::Copy`/`RndDrawable::Copy` which exist):
  ```cpp
  void UIComponent::CopyMembers(const UIComponent *c, Hmx::Object::CopyType ty) {
      RndTransformable::Copy(c, ty);
      RndDrawable::Copy(c, ty);
      mNavRight = c->mNavRight;
      mNavDown = c->mNavDown;
      mResourceName = c->mResourceName;
      mResourceDir = c->mResourceDir;
      mResourcePath = c->mResourcePath;
  }
  ```
- **ResourceCopy** — full port, Wii cpp:103-115 (MILO_ASSERT lines 0x94/0x9B/0xA1
  kept — retail strips them via the established MILO_DEBUG-off behavior; check
  per-TU idiom used elsewhere in this unit):
  ```cpp
  void UIComponent::ResourceCopy(const UIComponent *c) {
      MILO_ASSERT(c, 0x94);
      Hmx::Object::SetTypeDef((DataArray *)c->TypeDef());
      CopyMembers(c, kCopyShallow);
      if (mResourcePath.length() != 0) {
          mResourceDir = c->mResourceDir;
          MILO_ASSERT(mResourceDir.Ptr(), 0x9B);
      } else {
          mResource = c->mResource;
          mResource->PostLoad();
          MILO_ASSERT(mResource->Dir(), 0xA1);
      }
      Update();
  }
  ```
  (If `UIResource`'s `PostLoad/Dir` aren't declared in our `ui/UIResource.h`,
  add minimal decls — check first.)
- **Update** — `void UIComponent::Update() {}` STUB for phase A (slot
  correctness only; the real body needs GetResourcesPath/ResourceFileUpdated/
  UpdateMeshes helpers we don't have). TODO-comment pointing at Wii cpp:233+
  and retail fn_827DB8C8 (this body is a known-matchable: decomp.me/3ya1L).
  A stub compiles to `blr` and cannot regress anything (no callers in matched
  code today).
- Do NOT port SetTypeDef in phase A (its body calls UpdateResource → Update
  chain; it's slot-neutral). Phase B: SetTypeDef override + real Update +
  helpers + Copy-body rewrite to the Wii `CopyMembers(c, ty)` form + re-pin of
  the UIComponent TU (§6).

Member swap from §1.4 (mSelected 0x104 / mResource 0x108) goes in the same
header edit.

## 4. Apply order + A/B plan

1. Worktree: `scripts/setup_worktree.sh` (auto-copies analysis inputs).
2. Header + cpp edits (§2, §2.1, §3) — build alone first if you want a
   bisectable midpoint, but DO NOT measure/land alone (expected +8-shift
   regressions on derived-own vcalls if measured standalone).
3. `git apply docs/decomp/handoff/rnddrawable-devirt-banked.patch` (applies
   cleanly on 154a11a; it gates Draw/DrawShadow + the 4 subclass Draw
   overrides via DRAW_DC3_VIRTUAL).
4. Verify + measure:
   ```
   rm -f build/45410914/target_symbol_renames.stamp
   touch config/45410914/config.yml
   NINJA_JOBS=12 tools/fresh_report.sh
   ```
   Judge ONLY by `report.json` `measures.matched_functions` (current main
   baseline: **6932**, raw 6897 + 35 FP-anchor gap). diff_inspect --diagnose
   headline % is positional garbage; bare objdiff-cli strict [sym] mismatches
   don't count.
5. Per-unit delta check (don't let a net-positive hide a unit regression):
   diff the per-unit `matched_functions` against main's report for the §7
   watch list.
6. Land both levers + doc corrections (§1.4 plan-doc fix) as one commit.

## 5. Predicted improvers

**Banked +4 (returns immediately, was proven in the batch-2 A/B):**
`?DrawShowing@RndLine@@UAAXXZ` (default/Line, 99.98 → 100, tgt slot 0x14),
`?DrawMeshVec@SpotlightDrawer@@MAAX…` (99.95), `?RenderSheet@NgSpotlightDrawer@@IAAX…`
(99.96), `?CollideList@RndGroup@@UAAX…` (92.46 → has the 0x24 slot fixed; may
need the rest of its diff — it was one of the 4 named winners in the
force-multipliers dossier table, `docs/decomp/research/2026-06-10-force-multipliers.md:99-104`).

**Regression-set restore (the −8 four):** the batch-2 worktree A/B records
weren't persisted (checked `decomp.db` attempts + branches), so the exact 4
names must be confirmed in this A/B. Verified candidate (asm-confirmed −8
class member): **`?OnMsg@UISlider@@IAA?AVDataNode@@ABVButtonDownMsg@@@Z` =
fn_827E4DB8 (default/UISlider, 100%)** — contains `lwz r11,0x0(r30); mr r3,r30;
lwz r11,0x50(r11); mtctr; bctrl` = a this-receiver vcall at slot 0x50, the
first post-UIComponent slot: aligned today (20==20), −8 under patch-alone,
aligned again after both levers. Other ≥0x50 matched-fn vcall sites found by
scan (verify receiver class before worrying): `?Exit@UIScreen@@UAAXPAV1@@Z`
fn_827CBDD0 (0x54), `?SubList@UIListDir@@…` fn_827E5BD0 (0x54),
`?PollWidgets@UIListDir@@…` fn_827E5D68 (0x70), `?StartScroll@UIListDir@@…`
fn_827E5DC8 (0x68), `?CompleteScroll@UIListDir@@…` fn_827E5E58 (0x6c),
`?Draw@UIListArrow@@…` fn_827F8820 (0x54), `?Draw@UIListHighlight@@…`
fn_827F9C98 (0x54), `?SnappedDataForDisplay@UIListState@@…` fn_827E90E0 (0x50),
`?RemoveTrack@TrackPanelDir@@UAAXH@Z` fn_822F3230 (0x74/0x84),
`?Exit@CalibrationPanel@@UAAXXZ` fn_825EEE50 (0xa4), `?Entering@UIScreen@@UBA_NXZ`
fn_827CAA78 (0x84), InlineHelp-unit fns. NOTE: the UIListDir/UIListArrow/
UIListHighlight receivers are `UIListWidget*`, and **UIListWidget : public
Hmx::Object only** (`src/system/ui/UIListWidget.h:62`, same on Wii) — those
slots (Object 21 slots → own start 0x54) do NOT move with either lever; they
are expected to stay 100% throughout. Same for UIScreen/PanelDir-receiver
slots (UIScreen/PanelDir are ObjectDir-primary, not UIComponent-MI).

**Wall-flips (the "+4 UIComponent vtable wall, 5 confirmed fns" from roadmap
line 175 — currently near-miss, flip when both levers land):**
- `?Entering@PanelDir@@UBA_NXZ` fn_827E1458 (default/PanelDir, **99.97**) —
  vcall tgt 0x3c (ours emits 0x40 today). Near-certain +1.
- `?Exiting@PanelDir@@UBA_NXZ` fn_827E14D8 (**99.97**) — tgt 0x40 (ours 0x44).
  Near-certain +1.
- `?Draw@UIListSubList@@UAAX…` fn_827FB5F0 (**99.93**) — has a 0x34 vcall
  (SetState region) among aligned ≥0x50 ones; likely +1.
- `?PanelNav@PanelDir@@AAA_N…` fn_827E1D20 (96.81) and `?Init@UIManager@@UAAXXZ`
  fn_827E0690 (87.59, vcalls 0x3c+0x40) — slot diff removed; may need more.
- `?Draw@Splash@@MAAXXZ` fn_8271C6E8 (97.93) — its vcalls are 0x80/0x84
  (panel-receiver, probably unaffected); do not promise.
- `?DrawStartFinish@TrainerGemTab@@` (band3/TrainerGemTab, 99.92, parked as
  "15-slot UILabel/UIComponent-MI delta") — re-recon after landing; the
  UIComponent piece of its delta is gone.

**Newly-portable veins (the real cascade):**
- 0%-fns that vcall the previously-nonexistent virtuals — now portable:
  GuitarController fn_8277BA90 + fn_8277C960 (**vcall slot 0x30 =
  ResourceCopy**; unit 15/166 w/ rich map names), UIGuide fns fn_828015D0/
  fn_82802778/fn_828027F0 (0x4c=Update, 0x48=CopyMembers; UIGuide : Hmx::Object
  manages UIComponent* — unit 19/166), UIEvent fn_8263C670/fn_8263C860
  (0x38=StateSym), StorePanel 0x40/0x44-caller cluster, UIListLabel fn_827FA400
  (0x30), PanelDir `?EnableComponent…` fn_827E0ED0 (0x34+0x44).
- The bandobj display family (BandLabel/ScoreDisplay/MeterDisplay/StarDisplay/
  BandButton/InlineHelp-style classes) overrides Update/CopyMembers on Wii —
  wiring/porting those TUs was structurally impossible before this fix.
- UIComponent's own TU: see §6.

Realistic immediate net: **+4 (banked) +2-4 (wall-flips) ≈ +6-8**, HIGH
confidence on the +4 floor, plus the porting veins.

## 6. Follow-up (separate session): re-pin the real UIComponent TU

`config/45410914/splits.txt:1106` pins UIComponent.cpp to a single fn
(0x823D9AE0..0x823D9B38 = `?StaticClassName@UIComponent@@SA?AVSymbol@@XZ`, an
ICF-displaced one-off). The REAL retail UIComponent cluster sits unsplit
inside `auto_03_827CBF5C` around **0x827D8EF0..0x827DBDB0** (StateSym
0x827D8EF0, Enter 0x827D8FB8, Exiting 0x827D8FA0, SetState 0x827D9178,
ObjPtr dtor 0x827D94D8, Poll 0x827DA580, ResourceCopy 0x827D9FF8, CopyMembers
0x827DA0C8, dtor 0x827DABC0 + thunks 0x827DA9B0-0x827DABA8, SyncProperty
0x827DB5D0, PreLoad 0x827DB5E0 (+ vbase thunk 0x827DBD90, PostLoad thunk
0x827DBDA0), Update 0x827DB8C8). After phase-A lands, re-pinning + porting the
remaining bodies (Update is decomp.me-proven matchable) is an est. **+5-9**
vein — but it needs boundary recon against neighboring TUs (UIScreen cluster
~0x827CA000-0x827CBE00 below, UI/PanelDir ~0x827E0000+ above) and
target_symbol_map entries for the named bodies (gen via map conventions, lint
with `tools/map_lint.py --check obj_orphan`).

## 7. Regression watch list (per-unit A/B against main report)

UIComponent-MI pinned units (any net change here must be explained):
UISlider 19/50, ScrollSelect 18/24, InlineHelp 29/38, UIPicture 6/15,
UIListCustom 12/24, UIListLabel 7/22, UIListSubList 7/23, UIListArrow 3/6,
UIListMesh 2/5, UIListHighlight 1/1, UIButton 3/9, UILabel 0/1, MoviePanel
31/75, Splash 22/37, StorePanel 26/79, CreditsPanel 10/34, TourDescPanel
33/46, QuestFilterPanel 30/51, CalibrationPanel 17/45, TrainerPanel 33/59,
GamePanel 30/59, TrackPanelDir 29/65, PanelDir 22/42, UI 31/47, UIScreen
38/51, UIEvent 18/31, UIEventMgr 9/26, OvershellSlot 13/19, GuitarController
15/166, DeJitterPanel 8/15, ConnectionStatusPanel 4/23, UIGuide 19/166,
UIListDir 41/74, UIListState 16/21, UIListProvider 9/10, UITransitionHandler
6/17, LabelShrinkWrapper 1/1, LabelNumberTicker 1/1, DataEventList 13/18,
HamLabel 1/3 — plus the banked patch's own engine units (Line, SpotlightDrawer,
SpotlightDrawer_NG, Group, Anim/RndDir, CharClipSet, Character, HamCharacter,
Env). UITrigger (: EventTrigger + RndPollable) and UIListWidget-family slots
are NOT UIComponent-MI — expected no-ops.

Useful one-liner to re-find vcall sites during A/B triage (the scanner used
for §5): grep a unit's `.s` for `lwz rN, 0xNN(rM)` immediately preceding
`mtctr/bctrl` with 0x30 ≤ NN ≤ 0x4c (UIComponent own region) or ≥ 0x50
(derived-own region).

## 8. Confidence + what would falsify

- Slot table: **near-certain** (raw retail .rdata words + 3 independent
  internal cross-checks: SetState→0x44, Enter→0x34, ResourceCopy→0x48+0x4c;
  plus PanelDir::Entering tgt 0x3c and the banked-patch DrawShowing-0x14/
  CollideList-0x24 anchors all landing on the same 20-slot layout).
- The only soft identification is Entering(0x3c)/CanHaveFocus(0x44) being the
  ICF stubs — but both are order-forced by Wii decl order between the
  hard-identified neighbors (StateSym@0x38, Exiting@0x40, CopyMembers@0x48).
- If the A/B nets ≠ expected: first suspect the §2.1 OldResourcePreload
  derived-decl gating (a missed one inserts a slot in that subtree), then
  derived `Update()` collisions (§2.2), then re-read this doc's §5 receiver
  caveats before touching the slot table itself.
