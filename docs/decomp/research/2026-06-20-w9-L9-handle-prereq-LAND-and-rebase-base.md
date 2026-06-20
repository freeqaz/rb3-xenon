# W9 L9 — handle-prereq LAND-and-rebase-base (ADVERSARIAL DISCOVER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the prereq is real, fully verified, reproduced in
TWO independent build dirs, and is a **PURE FAST-FORWARD** (not a rebase). Land it.
**Mode:** read-only in main; ground truth = two already-built worktree report.json's
+ git tree/ancestry + per-unit/per-headline delta scan.

## TL;DR — what the frontier got right / wrong

- **RIGHT (the lever, the EV, the gate decision):** a7175af is the dominant Family-B
  Handle reconcile (MessageTimer `#ifdef MILO_MESSAGE_TIMERS` gate + global
  END_HANDLERS PathName comma tail + UIComponent resource-handler port + per-TU
  `/DMILO_MESSAGE_TIMERS` restores + 6 sliver Handle pairings). Measured **+196**,
  zero-regression. It IS the gate that unblocks the whole handle wave.
- **WRONG (terminology — "rebase a7175af onto current main"):** there is NOTHING to
  rebase. `git rev-parse a7175af^` == `812e1df` == **current main HEAD**. main has
  NOT moved past 812e1df. So a7175af already sits directly on the current tip →
  the land is a **trivial `git merge --ff-only a7175af`** (or push the ref), not a
  rebase + replay. The frontier's "rebase-base" framing is right in spirit (this
  commit becomes the base the whole wave rebases onto) but the land itself is a FF.
- **EV is EXACT, not an estimate:** est +196 == measured +196. Confirmed on disk in
  two worktrees AND by a fresh per-unit delta scan of the FINAL report vs main.

## Ground truth established (this investigation)

### Ancestry / FF-ability (the load-bearing structural fact)
| fact | value |
|---|---|
| `a7175af^` (parent) | `812e1df59ae2…` |
| current main HEAD | `812e1df59ae2…` |
| **=> relationship** | **a7175af is a direct child of main HEAD = pure fast-forward** |
| `a7175af` tree | `fe74a0f4a414…` |
| `9fb9016` (FINAL branch tip) tree | `fe74a0f4a414…` (**identical**) |
| `git diff a7175af 9fb9016` | empty (tree-identical, dual-named same commit content) |

### Measurement (reproduced in TWO build dirs — the load-bearing number)
| worktree | HEAD | report matched | vs main 8314 |
|---|---|---|---|
| main | 812e1df | 8314 | — |
| wt-w9-land-reconcile-handle-prereq-9fb9016 | a7175af | **8510** | **+196** |
| wt-w9-reconcile-handle-prereq-FINAL | 9fb9016 | **8510** | **+196** |

### Honesty gate (fresh per-unit + headline delta scan, main report vs FINAL report)
- **headline +196; sum of per-unit gains == 196** (59 units gained). Match.
- **ZERO regressions** — full scan over all 65544 fns: no unit lost a function, no
  fn went 100→0 anywhere.
- Top gains: Part +26, UIList +18, MidiParser +14, CameraShot +10, UIComponent +9,
  Gen +9, JoypadClient +5, ContentMgr_Xbox +5, UISlider/Spotlight/Line/LightHue/
  HeldButtonPanel/Crowd +4 each, …
- **BandDirector 172 → 172** (the L5/L8 funclet concern): net-NEUTRAL. The per-TU
  `/DMILO_MESSAGE_TIMERS` restore exactly offsets the global timer drop → frame 0x240
  preserved → its 46 0x28-funclets stay 100%. Confirmed by direct report read.
- Restore TUs all net-neutral/positive: Draw 20→20, StorePanel 26→26, Anim 47→47,
  Dir 50→50, Group 33→33, CharBoneDir 60→60, CharLipSync 45→45, **UI 26→29 (+3)**.
  (Draw/StorePanel/Anim are 0%-Handle on BOTH sides — additive A2 targets, NOT losses,
  exactly as L8 corrected L5.)
- No `>=8-contiguous foreign fn_@0%` concern: this is a header-macro + body-port +
  pin change in owned TUs; the 59 gaining units are all wired-owned engine/UI TUs.

### Files in the commit (5, self-contained, branches off main)
- `src/system/obj/Object.h`: BEGIN_HANDLERS/BEGIN_CUSTOM_HANDLERS `#ifdef MILO_MESSAGE_TIMERS`
  (timer off by default); END_HANDLERS/END_CUSTOM_HANDLERS `#ifndef HX_NATIVE` →
  `(void)(PathName(this),sym)` / `(void)(PathName(dynamic_cast<Hmx::Object*>(this)),sym)`.
- `src/system/ui/UIComponent.cpp/.h`: ResourceDir() + OnGetResourcesPath() body-port
  (rb3-Wii shape) + 2 new HANDLE entries. (Carries a now-dead push_macro MILO_NOTIFY
  block — harmless, optional cleanup.)
- `config/45410914/objects.json`: `/DMILO_MESSAGE_TIMERS` on BandDirector, CharBoneDir,
  UI, Dir, Group, CharLipSync.
- `scripts/target_symbol_map.json`: 6 new Handle pairings (CharIKFoot, ModalKeyListener,
  WorldInstance, UIComponent+OnGetResourcesPath, UIListSlot, UIListCustom).

## Downstream coordination (why this is THE gate)
Handle worktrees are currently split across THREE bases — landing the prereq lets
them all rebase onto ONE consistent reconciled Object.h:
- already on prereq: `w9-handle-pair-clean-char-tier-7-post-prereq` (on a7175af),
  `w9-handle-pair-tier1-char-clean` (on 9fb9016).
- still on main (need prereq): `w9-handle-pair-batch-mid-tier-per-fn-verify`,
  `w9-handle-pair-batch-small-tier1-char-clean`.
- on OTHER/stale bases: `w9-handle-pair-tier2-single-handler`,
  `w9-handle-timer-restore-draw-storepanel-anim` (this last is the OLD +130
  intermediate @6c8094f — main + BEGIN gate only, NO end tail / NO UIComponent port;
  it is NOT the A2 follow-up and must be re-cut on the landed prereq).

## Verdict
**REAL_ACTIONABLE.** Land a7175af via fast-forward (no rebase, no replay). +196,
zero-regression, exact, twice-reproduced. It is the rebase-base for the entire
Family-B handle wave.

## Actionable items
See structured output. One coordinator FF-land item (A1) + one additive follow-up
(A2, timer-restore Draw/StorePanel/Anim, must be re-cut on the LANDED prereq base).
