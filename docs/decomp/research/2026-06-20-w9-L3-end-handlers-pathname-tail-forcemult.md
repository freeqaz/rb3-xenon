# W9 L3 — end-handlers-pathname-tail-forcemult (ADVERSARIAL DISCOVER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the lever is real, but the frontier's framing is
wrong in two load-bearing ways (mechanism + EV). Corrected below.
**Mode:** read-only in main; ground truth from objdiff (run_objdiff) + COFF/asm.

## TL;DR (what the frontier got right / wrong)

- **RIGHT:** RB3 retail *does* emit a discarded `bl PathName(this)` at the
  `END_HANDLERS` unhandled tail, and our build *omits* it because
  `MILO_NOTIFY(...) = ((void)sizeof(MakeString(...)))` and `sizeof` is
  **unevaluated** (the same shape MILO_FAIL's `(void)(__VA_ARGS__)` comma form
  fixed for Find<T>). Verified retail-vs-base ground truth below.
- **WRONG #1 (mechanism):** the PathName tail is only **HALF** the Handle
  divergence, and the *smaller* half. The DOMINANT blocker is the **head**:
  `Object.h` `BEGIN_HANDLERS`/`BEGIN_CUSTOM_HANDLERS` emit a debug-only
  `MessageTimer timer(...)` **unconditionally** (no `MILO_DEBUG`/`HX_NATIVE`
  gate), which retail-release stripped. Every Family-B Handle is double-blocked
  (extra MessageTimer at head + missing PathName at tail). **The tail edit alone
  flips ZERO Handle bodies** while the head timer is still emitted.
- **WRONG #2 (EV):** "+45 auto-flips EVERY already-paired wired Handle" is not
  real. Only **6 Handle symbols are mapped** in `target_symbol_map.json`
  (4 classes: UIManager, DancerSequence, GuitarController, QuestFilterPanel).
  There is no standing pool of ~45 already-paired near-100 Handle fns gated
  solely on the tail. The realistic yield is the **tail-marginal** flips on the
  85 wired Handle bodies AFTER the head fix lands, **plus** subsuming per-TU
  wrappers — not an independent +45.
- **SEQUENCING:** this is the *second* change. The prerequisite (global
  MessageTimer drop, commit **e57d204**, +139/+143) is **NOT on main** — it is
  being landed via worktree `wt-w9-land-global-begin-handlers-timer-drop`
  (66be8b2). This was already correctly identified by the **L2 dossier**
  (`2026-06-20-w9-L2-handle-check-pathname-systemic.md`, "Strategy B"). This L3
  is a corroborating deep-drill, not a new lever.

## Ground truth established (this investigation)

### The two macro families (root of everything)
- `src/macros.h:3` force-defines `MILO_DEBUG` tree-wide (load-bearing: it strips
  members elsewhere, e.g. `TrackDir.h:132` drops `TrackTest *mTest`). So
  `MILO_DEBUG` **cannot be globally undefined.**
- **Family A** (28 Handle TUs): `#include "obj/ObjMacros.h"`. Bare `END_HANDLERS`
  (`ObjMacros.h:197` = `return DataNode(kDataUnhandled,0)`), explicit
  `HANDLE_CHECK(line)` (`ObjMacros.h:191`) carries the PathName warn, and
  `BEGIN_HANDLERS` is `#ifdef MILO_DEBUG`-gated → picks the MessageTimer branch.
- **Family B** (334 Handle TUs, mostly engine): use `Object.h`'s macros.
  `Object.h:925` BEGIN_HANDLERS emits MessageTimer **unconditionally**;
  `Object.h:1030` END_HANDLERS bakes the warn into MILO_NOTIFY.
- This mirrors rb3-Wii's `ObjMacros.h:210-218` (HANDLE_CHECK split) — so baking
  the eval into `END_HANDLERS` is self-consistent; rb3-Wii RELEASE `MILO_WARN` is
  `(void)(__VA_ARGS__)` (`../rb3/src/system/os/Debug.h:151`), which keeps the
  PathName side effect (this is what keeps PathName in ~505 retail Handle tails).
- **DC3** (engine twin) keeps `MILO_NOTIFY = TheDebugNotifier << MakeString(...)`
  (real emission) and matches — but RB3 retail stripped that output (the +23
  WARN/NOTIFY no-op lever), retaining only the arg side effects.

### Retail-vs-our-build divergence (objdiff, full_listing)
- **GuitarController::Handle (Family A, ALREADY-SOLVED reference)** @97.6%: the
  PathName tail matches on BOTH sides (idx 83-86: `clrlwi. r11,r25,24; beq <end>;
  mr r3,r29; bl ?PathName@@YAPBDPBVObject@Hmx@@@Z`). It does this because the TU
  *locally* applies BOTH per-TU fixes: `#undef MILO_WARN -> (void)(__VA_ARGS__)`
  (tail) AND `#undef BEGIN_HANDLERS` to the MILO_DEBUG-off form (drops timer).
  Its 2.4% residual is bool_mask + offset_swap — NOT the tail. **Proves the tail
  fix is necessary-not-sufficient.**
- **UIManager::Handle (Family B)**: retail target emits `bl ?PathName@...` at
  the unhandled tail (idx 1290); our base does NOT (calls superclass Handle).
  Confirms the divergence DIRECTION for Family B.
- **DancerSequence::Handle (Family B, UNPORTED)** @20%: base side shows the
  spurious `MessageTimer`/`Timer::Restart`/`SplitMs`/`AddTime` calls our
  unconditional BEGIN_HANDLERS emits — direct evidence of the head divergence.

### Pairing reality
- Only 6 Handle symbols are in `target_symbol_map.json`. The binary has **959
  END_HANDLERS-shaped tails** (per L2: `bl fn_82732F68=PathName` + `li r11,0x6`
  DataNode set), **85 in wired TUs**, the rest pin-gated in `auto_03_*` blobs.
  Unpaired Handle fns read "Stub/all-insert" in objdiff (e.g. UIListCustom
  fn_827F99D0 in the head-fix worktree) until a map entry pairs them — so the
  tail's effect is unmeasurable without ALSO adding the map entry in the same WT.

## Verdict & why it's not a standalone L3 actionable

The frontier item ("global END_HANDLERS PathName tail edit") **==** L2's
**Strategy B**, already specced and risk-flagged by the parent layer. It is REAL
and worth doing, but:
1. It is **dependent**, not self-contained: it only yields anything once
   e57d204's head fix is on main (or rebased in).
2. Its independent value is the **tail-marginal** flips + subsuming the per-TU
   `MILO_NOTIFY` wrappers — an EV of low-single-digits-to-~10 on already-paired
   Handles, NOT +45.
3. The cold-executable item below is the de-risked, correctly-scoped version:
   spike Strategy B in ONE worktree rebased on the head fix, whole-binary A/B,
   keep only if net>=+1 and it subsumes (does not regress) the Strategy-A
   per-TU tail wrappers and the +23 WARN/NOTIFY no-op.

## Actionable (self-contained, ONE worktree, vs main@8314 + head-fix prereq)

**spike-strategyB-global-end-handlers-pathname-tail** (kind=header-macro):
In ONE worktree branched from the head-fix tip (rebase on/cherry-pick e57d204
`BEGIN_HANDLERS` MessageTimer drop FIRST — it is the prereq):
- Edit `Object.h:1030-1043` END_HANDLERS/END_CUSTOM_HANDLERS so the unhandled
  tail comma-evaluates PathName under `#ifndef HX_NATIVE`:
  `if (_warn) (void)(PathName(this), sym);` (CUSTOM:
  `(void)(PathName(dynamic_cast<Hmx::Object*>(this)), sym);`). Do NOT touch the
  global `MILO_NOTIFY` macro — only END_HANDLERS' use. HX_NATIVE keeps the real
  notifier.
- Add `target_symbol_map.json` entries (`fn_<VA> -> ?Handle@Class@@UAA...`) for
  the now-byte-exact wired Handles so they pair (attribution claim — verify each
  byte-exact post-pair).
- Whole-binary A/B vs main@8314: `rm -f target_symbol_renames.stamp && touch
  config.yml && tools/fresh_report.sh` (re-run once for splits FP). Honesty gate:
  net>=+1, no >=8-contiguous foreign fn_@0% run in any changed range, headline ==
  intended, and it does NOT regress the Strategy-A first wave nor the +23
  WARN/NOTIFY no-op anywhere except END_HANDLERS tail. REVERT if net<=0 or noisy.

## Discovered frontier (adjacent, not fully planned here)

- **END_CUSTOM_HANDLERS dynamic_cast tail** (`Object.h:1040`): retail's CUSTOM
  tail calls `PathName(dynamic_cast<Hmx::Object*>(this))` — the dynamic_cast is a
  REAL `__RTDynamicCast`-ish emission, distinct from the plain `this`. Verify on
  UIListProvider/ScrollSelect/LightPresetManager (the 4 END_CUSTOM users) whether
  the cast is elided or emitted in retail before generalizing the CUSTOM tail.
- **HANDLE_CHECK comma-form gap (Family A)**: 37 TUs use `HANDLE_CHECK`, but only
  GuitarController has the `#undef MILO_WARN -> (void)(__VA_ARGS__)`. The other 36
  (incl. QuestFilterPanel) emit no PathName tail. A global ObjMacros.h
  `HANDLE_CHECK` comma-form (gated HX_NATIVE) would do for Family A what Strategy B
  does for Family B — verify it doesn't regress the +23 WARN no-op.
- **The 85 wired Handle pairing wave** (L2's first-wave table): UIListCustom/
  UIListSlot/Flow/StorePanel/UIGuide/Instance/NetCacheMgr/CharIKFoot (~45-54
  instrs each) are PathName-tail-or-superclass-forward ONLY — near-certain flips
  once head+tail+map land together. Each is its own self-contained pin+pair item.
