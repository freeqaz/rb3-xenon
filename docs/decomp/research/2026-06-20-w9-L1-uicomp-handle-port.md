# W9 L1 — UIComponent::Handle port (frontier "uicomp-handle-port"): adversarial drill

**Date:** 2026-06-20. **Baseline:** main @812e1df, report.json fresh, **8314 matched**.
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus L1), read-only in main.
**Frontier item:** `uicomp-handle-port` (body-port, est +12) — "Port UIComponent::Handle
fn_827D9928 (956B, 0%, BEGIN_HANDLERS) from rb3-Wii; unblocks the funclet family
fn_827D9D44..E24."
**Lead doc:** `research/2026-06-19-w8-uicomp-reconstruction.md` (§Group-2 FUNCLET_WALL),
`research/2026-06-11-bp4-uicomp.md` (§B/§D).

---

## HEADLINE VERDICT: REAL_ACTIONABLE — but the root cause is NOT "port a missing body",
it is **3 missing handler entries in our already-present `BEGIN_HANDLERS` block**, and the
est +12 is optimistic. Firm honest delta is **+9** (Handle +1, OnGetResourcesPath reveal +1,
7 Handle-EH funclets +7). The 3 funclets the lead doc lumped in (fn_827DA854/8A8/AEA8) are
**NOT Handle funclets** (different parent frames) — adversarial correction below.

---

## Ground truth (asm + COFF + oracle)

### 1. Our Handle compiles a real body but it is WRONG (3 entries short), and unpaired.
- `src/system/ui/UIComponent.cpp:350-358` already has `BEGIN_HANDLERS(UIComponent)` with
  **3 handlers** (get_state / set_state / can_have_focus) + 4 supers, **no HANDLE_CHECK**.
- `run_objdiff fn_827D9928` → "239 insert / Stub (High)". That is the **no-map-entry**
  signal (the anonymous target has no paired base symbol), NOT a literal stub. There is no
  `?Handle@UIComponent@@...` entry in `scripts/target_symbol_map.json` (grep: none).

### 2. The retail Handle (fn_827D9928, build/45410914/asm/UIComponent.s:1433-1741) decodes to:
`Symbol sym = _msg->Sym(1)` (bl fn_82725EE8 on `_msg+8`), then **5 static-symbol handlers**
(each a `_NEW_STATIC_SYMBOL` local static with its own guard bit in the multi-static guard
word `lbl_82DA0017+0x35FF1`, bits 0x1/0x2/0x4/0x8/0x10), then **4 HANDLE_SUPERCLASS**
forwards, then **HANDLE_CHECK** + `return DataNode(kDataUnhandled,0)`:

| guard bit | handler | retail codegen |
|---|---|---|
| 0x1  | `get_state`        | returns `GetState()` (mState read) |
| 0x2  | `set_state`        | vcall slot 0x34 = `SetState(_msg->Int(2))` |
| 0x4  | `can_have_focus`   | vcall slot 0x44 = `CanHaveFocus()` |
| 0x8  | `get_resource_dir` | **ResourceDir() INLINED**: `mResourceDir.mObject`@this+0x128, else `mResource`@this+0x108 `->Dir()`, else 0 |
| 0x10 | `get_resources_path`| `bl fn_827D98A0` = **OnGetResourcesPath(_msg)**, out-of-line |
| —    | super RndTransformable | `bl fn_823E7E40`, sret slot 0x60 |
| —    | super RndDrawable  | `bl fn_823F47C0`, sret slot 0x68 |
| —    | super RndPollable  | `bl fn_8240E828`, sret slot 0x70 |
| —    | super Hmx::Object  | `bl fn_827371D8`, sret slot 0x78 |
| —    | HANDLE_CHECK tail (0x827D9CA0) | `if(_warn){ PathName(this); } return DataNode(6,0)` — `bl fn_82732F68`=`?PathName@@YAPBD...` (confirmed in map), then `li r11,6; stw r11,0x4(sret)` |

Frame: `subi r31, r1, 0xd0` / `stwu r1, -0xd0`. The 0xd0 frame holds the 5 sret DataNode
temps (slots 0x50/0x58/0x60/0x68/0x70/0x78). **Our 3-handler body compiles a 0x90 frame.**

This **exactly** matches the rb3-Wii oracle `../rb3/src/system/ui/UIComponent.cpp:442-453`:
```
BEGIN_HANDLERS(UIComponent)
    HANDLE_EXPR(get_state, GetState())
    HANDLE_ACTION(set_state, SetState((UIComponent::State)_msg->Int(2)))
    HANDLE_EXPR(can_have_focus, CanHaveFocus())
    HANDLE_EXPR(get_resource_dir, ResourceDir())     <-- MISSING from ours
    HANDLE(get_resources_path, OnGetResourcesPath)   <-- MISSING from ours
    HANDLE_SUPERCLASS(RndTransformable/RndDrawable/RndPollable/Hmx::Object)
    HANDLE_CHECK(579)                                <-- MISSING from ours
END_HANDLERS
```
**DC3 is a FALSE FRIEND** here: `../dc3-decomp/src/system/ui/UIComponent.cpp:154-162`
dropped get_resource_dir / get_resources_path / HANDLE_CHECK (DC3 is newer/refactored).
Our cpp currently mirrors DC3. Use **rb3-Wii**.

### 3. The funclet wall — what ACTUALLY flips with Handle (objdiff full_listing, ground truth):

- **fn_827D9D84 / DAC / DD4 / DFC / E24** (DataNode-temp EH cleanups, 99.8-99.9):
  the *only* scoring diff is idx-0 `subi r31, r12, 0xd0` (target) vs `0x90` (ours). The
  `bl lbl_822605C0` vs `??1DataNode@@QAA@XZ` is reloc-normalized (ignored). **They flip the
  instant Handle's frame is 0xd0.** → **+5**.
- **fn_827D9D44 / D64** (static-init-guard release funclets, 92.5): the diff is the
  guard-bit *mask width*. Target `rlwinm r11,r11,0,29,27` (clears 3 bits) / `0,28,26`;
  ours `clrrwi r11,r11,1` (clears 1 bit). The base symbol is literally
  `?$S7@?2??Handle@UIComponent@@UAA?AVDataNode@@PAVDataArray@@_N@Z@4IA` — the Handle
  static-init guard word. Our Handle has **3** statics → narrow mask; retail has **5** →
  wider mask. **Adding the 2 missing handler statics widens the mask to match.** → **+2**.
- **ADVERSARIAL CORRECTION** to the lead doc / bp4-uicomp §D: fn_827DA854 / fn_827DA8A8
  reconstruct `subi r31, r12, 0x80` (parent frame 0x80) and fn_827DAEA8 `subi r31, r12, 0x70`
  (parent frame 0x70). **They are NOT Handle funclets** (Handle is 0xd0) — they belong to
  OTHER functions in the TU. They will NOT flip from the Handle fix. The bp4-uicomp §D
  "same family, unverified" guess is WRONG; do not count them here.

### 4. THE CRITICAL CATCH — HANDLE_CHECK + PathName eval vs the project MILO_WARN no-op.
Retail's HANDLE_CHECK tail **calls `PathName(this)`** (bl fn_82732F68). But our project
no-ops `MILO_WARN`/`MILO_NOTIFY` to `((void)sizeof(MakeString(...)))` (`src/system/os/Debug.h:149,161`)
— `sizeof` is **unevaluated**, so PathName would be DROPPED → mismatch.
rb3-Wii's RELEASE `MILO_WARN(...)` is `(void)(__VA_ARGS__)` (`../rb3/src/system/os/Debug.h:151`)
which DOES evaluate the comma-expression (PathName(this) side-effect survives, format/line/sym
fold away). **This is why PathName is kept in 505 unit .s Handle tails across the binary.**
Object.h's `END_HANDLERS` (`src/system/obj/Object.h:1030`) already writes
`if(_warn) MILO_NOTIFY("...", PathName(this), sym);` — under our sizeof no-op it won't emit
PathName. **The fix MUST make this one TU evaluate PathName.** Self-contained options:
- **(A) preferred:** wrap the handler block with a TU-local `#define MILO_NOTIFY(...)
  (void)(__VA_ARGS__)` (then `#undef`/restore) so Object.h's END_HANDLERS comma-evaluates
  `PathName(this)`. Zero blast radius (one TU, restored after the block).
- **(B):** locally `#define HANDLE_CHECK(n) if(_warn) (void)(PathName(this), sym);` and emit
  it before END_HANDLERS (matching the oracle's explicit HANDLE_CHECK(579)). Note the oracle
  uses ObjMacros.h's HANDLE_CHECK (line 191) which our cpp does NOT have (our Object.h lacks
  HANDLE_CHECK and is NOT including ObjMacros.h). Either way, do NOT touch global Debug.h.

Verify at A/B which of (A)/(B) reproduces the exact tail (both should; (A) is lower-risk).

---

## What the worktree must do (ONE worktree, lands independently vs main@8314)

1. `src/system/ui/UIComponent.h`: add public decls
   `class ObjectDir *ResourceDir();` and `DataNode OnGetResourcesPath(DataArray *);`
   (rb3-Wii header:74-75). Non-virtual → vtable-neutral. ObjectDir is fwd-declared (Dir.h).
2. `src/system/ui/UIComponent.cpp`: port the two bodies from rb3-Wii cpp:365-371 (ResourceDir)
   and cpp:420-425 (OnGetResourcesPath). Helpers all present: `UIResource::Dir()`
   (UIResource.h:35), `FileRoot`/`FileRelativePath` (File.h), `DataNode(const char*)` ctor
   (fn_82725988 = `??0DataNode@@QAA@PBD@Z`). mResource@0x108 / mResourceDir@0x124 confirmed.
3. Extend the handler block (cpp:350-358) to the oracle's 5+4+CHECK form: add
   `HANDLE_EXPR(get_resource_dir, ResourceDir())`, `HANDLE(get_resources_path, OnGetResourcesPath)`,
   and the HANDLE_CHECK/PathName tail per §4-(A). Symbols `get_resource_dir`/`get_resources_path`
   already declared `extern Symbol` (Symbols3.h:488-489). The `_NEW_STATIC_SYMBOL` static
   ORDER must be get_state, set_state, can_have_focus, get_resource_dir, get_resources_path
   (the guard-bit order 0x1..0x10) — keep oracle order.
4. Compile the TU. A/B each piece with `run_objdiff` (project_dir=worktree):
   - fn_827D9928 (Handle) must hit report-normalized 100 (add its map entry only after byte-exact).
   - fn_827D98A0 (OnGetResourcesPath) reveal: add map entry `"0x827D98A0":
     "?OnGetResourcesPath@UIComponent@@QAA?AVDataNode@@PAVDataArray@@@Z"` (pull exact mangling
     from the compiled obj), A/B → +1.
   - fn_827D9D84/DAC/DD4/DFC/E24 → must auto-flip to 100 (no map entry needed; funclets pair
     positionally). fn_827D9D44/D64 → auto-flip to 100 once 5 statics.
   - Add Handle's own map entry `"0x827D9928": "?Handle@UIComponent@@UAA?AVDataNode@@PAVDataArray@@_N@Z"`.
5. Whole-binary A/B vs main@8314 (HEADER EDIT → mandatory): expect **+9** (Handle 1 +
   OnGetResourcesPath 1 + 7 funclets). Zero regressions outside the unit. Watch ScrollSelect/
   UIScreen/PanelDir/UISlider/InlineHelp (UIComponent subtree) — must not drop.

**Attribution_risk = false** (no splits/pin change; all addrs in the existing pin
0x827D8DC8-0x827DBDB0; reveals are within-unit, funclets pair positionally).

## Discovered adjacent leads (seed later layers)
- **0x80-frame funclet owner** (fn_827DA854/8A8): identify the parent function (frame 0x80,
  in the SendSelect / 0x827DA-area). Same auto-flip mechanic once its parent matches. ~+2.
- **0x70-frame funclet owner** (fn_827DAEA8): parent frame 0x70. ~+1.
- **OnGetResourcesPath cousins**: ResourceFileUpdated (88.84%) + GetResourcesPath (73.55%) +
  Update (71.4%) are the remaining named near-misses in this unit (bp4-uicomp §B/W8 §Group-1),
  now possibly easier with the macro/PathName tail understood. Separate bodyport lane.
- **HANDLE_CHECK PathName systemic lever**: 505 unit .s files carry the PathName Handle tail.
  If the macro-eval fix (§4) generalizes, a binary-wide HANDLE_CHECK-aware handler pass could
  unblock many other 0%/near-miss Handle bodies. High-EV, needs its own discover pass.
