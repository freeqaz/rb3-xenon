# W9 L2 — handle-check-pathname-systemic (DISCOVER/adversarial)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** (with corrections to the frontier premise)
**Mode:** read-only in main; analysis from worktree `wt-w9-uicomp-handle-handlers-plus-funclets` (@e57d204, the just-landed uicomp-handle patch — UNMERGED).

## TL;DR

The lever is REAL but the frontier mis-attributes the mechanism. The +139 came
from **TWO orthogonal changes**, and the PathName tail (fn_82732F68) is only the
*second* of them:

1. **GLOBAL** `BEGIN_HANDLERS`/`BEGIN_CUSTOM_HANDLERS` **MessageTimer drop**
   (`Object.h`, gated `#ifdef HX_NATIVE`). Retail (release) `Handle` has NO
   profiling `MessageTimer` in its head; our DC3-derived `Object.h` emits one
   unconditionally. This is the BULK enabler (+143 across 36 units). **This change
   is NOT on main** — `src/system/obj/Object.h:925-938` still emits the timer
   unconditionally. It lives only in unmerged worktree e57d204.
2. **PER-TU** PathName tail: retail `END_HANDLERS` calls `PathName(this)` (the
   `if(_warn)` unhandled-msg side effect). Our global release `MILO_NOTIFY` is
   `((void)sizeof(MakeString(...)))` and `sizeof` does NOT evaluate its operand,
   so `PathName(this)` is dropped. The patch re-adds it TU-locally via
   `#pragma push_macro("MILO_NOTIFY")/#undef/#define MILO_NOTIFY(...) (void)(__VA_ARGS__)/pop_macro`.
3. **PAIRING**: each now-byte-exact `Handle` fn needs a `target_symbol_map.json`
   entry (`fn_<VA>` -> mangled `?Handle@Class@@UAA...`), or objdiff reads it 0%
   ("Stub/all-insert") even when byte-exact.

## Ground truth established

- **fn_82732F68 IS `PathName(const Hmx::Object*)`** — verified from its body
  (`build/45410914/asm/auto_03_82731D08_text.s:1379`): `if (r3==0) return
  lbl_82101620+0x8` (the "<null>" string) else tail-call `vtable[0x50]` =
  `FindPathName` virtual. Exactly `const char *PathName(...)`.
- **959 END_HANDLERS-shaped functions** binary-wide (call `bl fn_82732F68`
  immediately followed by `li r11,0x6; stw` = set DataNode type `kDataUnhandled`).
- **ZERO are currently matched on main.** Corroborates the commit's "ZERO of the
  binary's Handle@@UAA functions matched while the timer was emitted." => the
  global timer drop has **no Handle bodies to regress** (only -4 funclet byte-shift
  slips, layout-coupled, explained).
- **85 of the 959 live in WIRED TUs whose source already has BEGIN_HANDLERS**
  (the rest are in unpinned `auto_03_*` blobs — blocked by PINNING, not the macro).
- **UIComponent::Handle proof:** with all 3 steps applied, objdiff reports
  `?Handle@UIComponent@@UAA?AVDataNode@@PAVDataArray@@_N@Z` = **100.0% normalized**
  (239 instrs, all equal).
- **UISlider::Handle (fn_827E5008)** in the worktree (has global macro fix) still
  reads "Stub/all-insert" — because there is NO map entry pairing it. This is the
  pairing gap, not a body divergence.

## Tractable first wave (smallest wired Handle bodies, src already has BEGIN_HANDLERS)

Size = target instruction count (smaller = fewer handlers = higher flip prob):

| size | TU | target Handle | layer |
|---|---|---|---|
| 45 | UIListCustom | fn_827F99D0 | engine ui |
| 45 | UIListSlot | fn_827EFE58 | engine ui |
| 48/55 | PropKeys | fn_82649F78 / fn_8264B118 | engine rndobj |
| 54 | Flow | fn_8229D0E0 | engine flow |
| 54 | StorePanel | fn_827923A0 | engine meta |
| 54 | UIGuide | fn_828020D0 | engine ui |
| 54 | Instance | fn_824D7C28 | engine world |
| 54 | NetCacheMgr | fn_827A9588 | engine utl |
| 54 | CharIKFoot | fn_823AD6E8 | engine char |
| 57 | CharFaceServo/CharPosConstraint/CharSleeve/FlowIf | … | engine char/flow |
| 59 | ConnectionStatusPanel / Rnd | … | engine |
| 61-82 | Console, FxSend, StreamNull, CharBoneDir(×2), MatAnim, CharCollide, CharIKHead, GuitarController, Waypoint, ScrollSelect, UserMgr | … | mixed |

UIListCustom::Handle (45 instrs) = `HANDLE_SUPERCLASS(UIListSlot)` + PathName tail
ONLY — our source matches the target exactly (target calls fn_827EFE58 superclass
then fn_82732F68). This is the canonical trivial flip.

## Two generalization strategies for the PathName tail

**Strategy A (per-TU MILO_NOTIFY redefine, PROVEN on UIComponent):**
wrap each TU's `BEGIN_HANDLERS … END_HANDLERS` block in
`#pragma push_macro("MILO_NOTIFY") / #undef MILO_NOTIFY /
#define MILO_NOTIFY(...) (void)(__VA_ARGS__) / … / #pragma pop_macro("MILO_NOTIFY")`.
Low blast radius (TU-local), zero header risk. Cost: a ~6-line wrapper per TU.

**Strategy B (global END_HANDLERS rewrite, HIGHER-LEVERAGE, header-edit RISK):**
change `Object.h` `END_HANDLERS`/`END_CUSTOM_HANDLERS` so the unhandled tail emits
`PathName(this)` directly (e.g. `if (_warn) (void)(PathName(this), sym);`) instead
of via the sizeof-stripping `MILO_NOTIFY`. This would make the tail global — no
per-TU wrapper needed — flipping every wired Handle at once (paired ones to 100%,
unpaired still need map entries). rb3-Wii's oracle puts the PathName eval in a
separate `HANDLE_CHECK(line_num)` macro (ObjMacros.h:210-218) with a BARE
`END_HANDLERS`; our project bakes it into END_HANDLERS, so the global edit is
self-consistent. **RISK: header edit = #1 cross-TU regression source; must
whole-binary A/B; must NOT regress the +23 MILO_WARN/NOTIFY no-op elsewhere (only
END_HANDLERS' specific use is changed, not global MILO_NOTIFY).** Gate `RB3_*` not
needed since it's the same for retail+all TUs, but native (HX_NATIVE) must keep the
real notifier — so wrap the global change `#ifndef HX_NATIVE`.

Recommended: land Strategy-A first-wave to bank de-risked matches; SPIKE Strategy-B
in one worktree as a force-multiplier (if it A/Bs clean it subsumes all per-TU
wrappers + auto-flips any already-paired Handle).

## Hard gotchas / honesty

- **The +139 is NOT on main.** The coordinator must land worktree e57d204 (or
  re-derive it) FIRST; it is the prerequisite for every actionable item below.
  Verify e57d204 fresh A/B vs main@8314 before generalizing.
- **Bulk (auto-blob) Handles are PIN-GATED, not macro-gated.** The 959-85=~874
  unpinned Handle bodies need their owning TU pinned/wired before any macro helps.
  Do NOT estimate +874 — that conflates two veins. Realistic macro-only yield =
  the 85 wired set, of which the ~30 small ones are near-certain flips and the
  large ones (UIList 1005 instrs, UIScreen, Profile, SongMgr) are flip-IF the full
  handler list matches retail (per-TU verification needed).
- **attribution_risk=true on any item that adds a map entry** (pairing a sliver/VA
  to a mangled name is an attribution claim; verify byte-exact post-pair).
- Each item is SELF-CONTAINED only if it includes: (the global macro prereq via
  rebase on e57d204) + TU-local tail wrapper + map entry + objdiff verify, all in
  ONE worktree, A/B whole-binary vs main.
