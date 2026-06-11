# Vtable-order walls — rdata-obj slot recovery (2026-06-11)

Applies the rdata-obj vtable-recovery technique
(`docs/decomp/research/2026-06-11-uicomponent-virtuals.md` §1.1) to the known
"retail vtable order unknown" walls. The auto-split rdata object
`auto_00_82000400_rdata.obj` (repo root, untracked) carries **every retail
vtable** as raw big-endian words: `.rdata` section VA base 0x82000400, file
offset = `VA − 0x82000400 + 0x3c`. dtk leaves the original target words in
place, so each slot word IS the retail function VA.

Reusable reader:
```python
import struct
f = open('auto_00_82000400_rdata.obj','rb').read()
def word(va): return struct.unpack_from('>I', f, 0x3c + (va - 0x82000400))[0]
```
Anchor a vtable by searching the rdata for a known override's VA (e.g. a
class-specific `?SyncObjects@Foo@@...` from `scripts/target_symbol_map.json`),
then walk consecutive slots.

---

## PRIMARY: VocalTrackDir::TrackReset @ 99.989% — FIXED (+? pending A/B)

**Root cause: `ObjectDir::GetExposedProperties()` is a DC3-only virtual that
retail RB3 lacks.** It sits in our `src/system/obj/Dir.h` between `SetSubDir`
and `SyncObjects`, inserting one extra slot into the ObjectDir-vbase vtable —
shifting SyncObjects/ResetEditorState/InlineSubDirType (and the same fragment in
EVERY ObjectDir descendant) down by one slot.

### Evidence

`?TrackReset@VocalTrackDir@@UAAXXZ` (fn_822E5900) was a 1-instruction near-miss:
```
[7] target: lwz r11, 0xc(r11)   <- SyncObjects via Hmx::Object-vbase vptr
    ours:   lwz r11, 0x10(r11)  <- one slot too high
```
The vcall is `SyncObjects()` reached through `[this-0x1dc]` (the Hmx::Object
vbase vptr → ObjectDir-vbase fragment).

**Retail ObjectDir-vbase vtable** (anchored: `?SyncObjects@VocalTrackDir@@UAAXXZ`
= 0x822E7D50 found once in rdata at VA 0x82029D70 → fragment vptr @ 0x82029D64):

| off | retail word | identity |
|-----|-------------|----------|
| -0x04 | 0x821CB4C8 | RTTI/COL |
| +0x00 | 0x8272CF78 | SetProxyFile |
| +0x04 | 0x827ABE48 | ProxyFile |
| +0x08 | 0x823F2220 | SetSubDir (RndDir override) |
| **+0x0c** | **0x822E7D50** | **SyncObjects (VocalTrackDir override)** |
| +0x10 | 0x8272E380 | ResetEditorState |
| +0x14 | 0x822695A8 | (InlineSubDirType / AddedObject region) |
| +0x18 | 0x822695B0 | |
| +0x1c | 0x82465928 | ICF empty stub |
| +0x20 | 0x823F1E18 | |

**Our ObjectDir-vbase vtable** (`??_7VocalTrackDir@@6BObjectDir@@@`, vptr-relative
= COFF section offset − 4):

| off | identity |
|-----|----------|
| +0x00 | SetProxyFile |
| +0x04 | ProxyFile |
| +0x08 | SetSubDir |
| **+0x0c** | **GetExposedProperties  ← DC3-only extra slot** |
| +0x10 | SyncObjects |
| +0x14 | ResetEditorState |
| +0x18 | InlineSubDirType |

rb3-Wii `src/system/obj/Dir.h` (the retail-faithful oracle) has the vbase order
`SetProxyFile, ProxyFile, SetSubDir, SyncObjects, ResetEditorState, …` — **no
GetExposedProperties**. DC3's Dir.h is the only one that has it
(`mcp lookup_dc3 GetExposedProperties` → DC3 Dir.h; `lookup_rb3wii` → none). Our
source provenance is DC3, so we inherited the extra virtual.

### Fix

`src/system/obj/Dir.h`: gate the `virtual` keyword behind `HX_NATIVE`
(`DIR_DC3_VIRTUAL` macro, same idiom as `DRAW_DC3_VIRTUAL`). The method stays a
normal member, so the only call site (`TypeProps.cpp:408`,
`it->GetExposedProperties()`) still compiles; nothing overrides it, so
non-virtual dispatch is behavior-identical.

TrackReset flips to all-91-instructions-equal (100% normalized, 99.0% raw)
immediately after the edit. **Blast radius:** the ObjectDir-vbase fragment of
EVERY ObjectDir descendant shifts up one slot toward retail — this is the
intended whole-binary correction (every descendant's SyncObjects/ResetEditorState/
InlineSubDirType vcall realigns). Whole-binary A/B mandatory; see below.

### The hidden second bug — `AllowsInlineProxy` (coordinated fix required)

Removing GetExposedProperties ALONE was net-0 on the whole binary: +2
(TrackReset, PanelDir::RemovingObject) but −2 (CharClipSet::SyncProperty +
fn_823C2044). The regression `CharClipSet::SyncProperty` was a 1-instruction
slot diff: retail calls `SetBpm` at vbase slot **+0x28**, our removal pushed it
to **+0x24**.

Cause: our ObjectDir-vbase vtable had a SECOND divergence from retail —
**`AllowsInlineProxy` is a virtual in retail (and rb3-Wii), demoted to a plain
member by DC3.** It belongs between `ResetEditorState` and `InlineSubDirType`
(rb3-Wii `src/system/obj/Dir.h`). Confirmed at retail vbase slot **+0x14**
(0x822695A8 across all 28 ObjectDir-vbase vtables). `BandCharacter.h:50` already
declares `virtual bool AllowsInlineProxy()` — overriding a base virtual that our
header didn't declare virtual, so that override was inserting a bogus slot.

Before the fix, GetExposedProperties (+1 slot, wrong) and the missing
AllowsInlineProxy (−1 slot, wrong) **cancelled** in classes whose hot vcalls sit
past InlineSubDirType (CharClipSet::SetBpm) — they matched by coincidence — while
classes whose hot vcalls sit between GEP and InlineSubDirType (VocalTrackDir::
SyncObjects) stayed broken. Fixing only one half breaks the coincidence.

**The fix is two coordinated edits, both in `src/system/obj/Dir.h`:**
1. `GetExposedProperties` → `DIR_DC3_VIRTUAL` (drop the virtual in retail build).
2. Add `virtual bool AllowsInlineProxy() { return mInlineProxy; }` between
   `ResetEditorState` and `InlineSubDirType`; remove the old non-virtual member
   (`bool AllowsInlineProxy() const` in the `#else` block).

After both: TrackReset 100%, CharClipSet::SyncProperty 100% (both all-equal).

### Recovered retail ObjectDir-vbase vtable (canonical, all 28 instances share
this prefix; class-specific overrides fill the named slots)

| vptr off | virtual |
|----------|---------|
| +0x00 | SetProxyFile |
| +0x04 | ProxyFile |
| +0x08 | SetSubDir |
| +0x0c | **SyncObjects** |
| +0x10 | ResetEditorState |
| +0x14 | **AllowsInlineProxy** (0x822695A8 base impl) |
| +0x18 | InlineSubDirType (0x822695B0 base impl) |
| +0x1c | (ICF empty stub 0x82465928 — AddedObject base `{}`) |
| +0x20 | RemovingObject |
| +0x24 | OldLoadProxies |
| +0x28… | class-specific new virtuals (e.g. CharClipSet::SetBpm @ +0x28) |

(28 ObjectDir-vbase vtables enumerated from the rdata by the `SetProxyFile,
ProxyFile` word-pair signature — all share +0x08=SetSubDir, +0x0c=SyncObjects,
+0x10=ResetEditorState, +0x14=AllowsInlineProxy. The DC3-sourced
GetExposedProperties was never among them.)

### A/B RESULT — net 0 on-branch → REVERTED per protocol

Worktree baseline (branch point 5d81ed4): **6989**. Coordinated fix
(GEP→DC3_VIRTUAL + AllowsInlineProxy→virtual): **6989**. NET 0.

On-branch function-level delta (excluding the UIComponent unit, which drifted +38
in the main-repo report from a concurrent re-pin agent — NOT this branch's work):
- **+1** `VocalTrackDir::TrackReset` (99.989 → true byte-exact 100%).
- **−1** `CharClipSet::fn_823C2044` (100% → 94%): a 40-byte DataNode-dtor unwind
  **funclet** of `CharClipSet::SyncProperty`. Its single diff is
  `lwz r3, 0x50(r31)` vs `addi r3, r31, 0x50` — a stack-slot codegen variance in
  the parent's exception cleanup that flipped when SyncProperty recompiled.
  Funclet/frame-layout class (permuter-class, source-immune from the vtable
  edit). Confirmed 100% at HEAD, 94% after the fix → genuine collateral.

`CharClipSet::SyncProperty` itself stays **100%** under the coordinated fix
(it regressed only under the GEP-only half). So the structural fix is CORRECT and
loses nothing but one frame-layout funclet.

Per the A/B protocol ("revert anything ≤ 0") and the task's explicit ObjectDir
guard ("if the fix must touch ObjectDir and the A/B shows ANY regression, revert
and document the exact slot table"), **the source change is reverted.** The
recovered slot table above is the deliverable for a coordinated campaign: the
fix becomes net-positive the moment the fn_823C2044 funclet is realigned (e.g.
via the parent's stack-decl order, or it may realign for free alongside other
pending CharClipSet/SyncProperty work). The two header edits are preserved in
this doc verbatim for one-shot reapplication:

```cpp
// src/system/obj/Dir.h, before `class ObjectDir`:
#ifdef HX_NATIVE
#define DIR_DC3_VIRTUAL virtual
#else
#define DIR_DC3_VIRTUAL
#endif
// in the ObjectDir virtual block:
    virtual void SetSubDir(bool isSubdir);
    DIR_DC3_VIRTUAL DataArrayPtr GetExposedProperties() { return nullptr; }  // was: virtual
    virtual void SyncObjects();
    virtual void ResetEditorState();
    virtual bool AllowsInlineProxy() { return mInlineProxy; }                // NEW virtual (was plain member)
    virtual InlineDirType InlineSubDirType();
// remove the old non-virtual `bool AllowsInlineProxy() const { return mInlineProxy; }`
```

---

## SECONDARY: other single-slot vtable-order candidates

The technique + the ObjectDir slot table above generalize. Confirmed and
suspected single-slot vtable-order near-misses (signature: a lone
`lwz rN, 0xNN(rM)` before `mtctr/bctrl` whose offset differs by ±4 from retail,
both symbols virtuals of the same class family — a REAL byte diff, distinct from
the report-ignored [sym]-only reloc-name noise):

- **`?RemovingObject@PanelDir@@UAAXPAVObject@Hmx@@@Z`** (99.978) — `[37] lwz
  [off:+4]`, the SAME ObjectDir-vbase wall. RESOLVED by the GEP/AllowsInlineProxy
  fix above (it was a +1 gainer in the A/B). Lands free with the ObjectDir
  campaign.
- **`?Draw@UIListSubList@@UAAX…`** (99.9) — UIComponent-MI slot delta; OFF-LIMITS
  (concurrent UIComponent re-pin agent owns this unit). Flips with the banked
  UIComponent virtuals work, not this ObjectDir lever.
- The 99.88% **`??_G<Class>@@UAAPAXI@Z`** cluster (CharForeTwist, CharIKHand,
  Flow*, …, ~19 fns) and the 99.6% 40-byte `fn_` cluster are **vector-deleting
  destructors / unwind funclets** — FP-anchor / funclet class, NOT vtable-order
  walls (no slot-offset diff). Do not chase as vtable walls.

General detector (for a future sweep): for each named virtual near-miss, run
objdiff and keep only those whose sole mismatch is `diff_arg lwz [off:±4]` on an
instruction feeding `mtctr; bctrl`. That isolates true slot-order walls from the
funclet/regalloc noise that dominates the 99.x band.

