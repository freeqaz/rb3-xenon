# W9 L5 — reconcile-handle-prereq-FINAL (ADVERSARIAL DISCOVER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the lever is real and dominant, but the frontier's
restore list is **INCOMPLETE** and its EV is slightly under. Corrected below with
COFF/asm ground truth.
**Mode:** read-only in main; ground truth from dtk-split target asm
(`build/45410914/asm/*.s`) + report.json + git history of the two unmerged branches.

## TL;DR (what the frontier got right / wrong)

- **RIGHT:** The two Handle branches diverge on the MessageTimer gate, and the
  spike silently regresses timer-on TUs. Restoring the timer per-TU recovers them.
  The global END_HANDLERS PathName tail (spike) is the higher-leverage form vs
  tiny's per-TU push_macro.
- **WRONG #1 (restore list incomplete):** The frontier names 5 TUs to restore
  (BandDirector + CharBoneDir/Dir/Group/CharLipSync). Ground truth: the spike's
  `7043e56` losses span **6** TUs (BandDirector -24, Draw -5, StorePanel -3,
  UI -3, Anim -2, FlowSound -1), and asm proves **Draw, StorePanel, UI, Anim ALSO
  compiled their retail Handle WITH the timer** (`bl fn_82725EE8` timer-ctor
  present). They need `/DMILO_MESSAGE_TIMERS` too. Only **FlowSound -1** is NOT
  timer-recoverable (no timer in its Handle — pure END-tail near-miss cost).
  Plus e57d204's -4 (CharBoneDir/CharLipSync/Group/Dir) which tiny already restores.
- **WRONG #2 (EV slightly low):** Recovering all timer-coupled losses = ~+37 over
  the spike's net, not just +24. Spike measured ~8502 (+188); reconciliation
  ceiling ≈ 8502 + ~37 = **~8539 (+225)**, frontier said +215. Both > tiny (8448).
- **Mechanism CONFIRMED:** retail BandDirector::Handle (fn_822804F8) frame = **0x240**
  with the MessageTimer block (`subi r31,r1,0x240; stwu`; static-Symbol-guard +
  `bl ??0Symbol` (fn_8279B788) + timer-ctor `bl fn_82725EE8` at frame+0x154). The
  46 contiguous 0x28 unwind funclets after it (all 100% on main) encode that 0x240
  frame. Spike's `HX_NATIVE` gate drops the timer → frame shrinks → funclets slip
  → -24. The `MILO_MESSAGE_TIMERS` gate restores the timer per-TU → frame 0x240 →
  funclets re-align.

## The two unmerged branches (root of the reconciliation)

Both branch directly off main@812e1df (8314):

- **SPIKE** `w9-spike-strategyB-global-end-handlers-pathname-tail` (tip **51fc194**):
  - `e57d204` — BEGIN_HANDLERS drop MessageTimer, gated `#ifdef HX_NATIVE`
    (timer ALWAYS off in match build, NO per-TU escape) + UIComponent resource
    handler port (UIComponent.cpp/.h). +139/+143, -4 slips
    (CharBoneDir/CharLipSync/Group/Dir).
  - `7043e56` — global END_HANDLERS / END_CUSTOM_HANDLERS PathName tail
    (`if(_warn) (void)(PathName(this),sym);`, gated `#ifndef HX_NATIVE`). +49 over
    e57d204, **-38 funclet slips** (BandDirector -24/Draw -5/StorePanel -3/UI -3/
    Anim -2/FlowSound -1).
  - `51fc194` — 4 sliver target_symbol_map pairings (UIListCustom/UIListSlot/
    WorldInstance/CharIKFoot). Measured **~8502**.
- **TINY** `w9-handle-wave-tiny-engine-ui-rnd` (tip **f9588ba**):
  - `b259212` — BEGIN_HANDLERS gate the timer behind **`#ifdef MILO_MESSAGE_TIMERS`**
    (undefined default = retail shape, **per-TU restorable** via objects.json),
    and restores 4 TUs (CharBoneDir, Dir, Group, CharLipSync) with `/DMILO_MESSAGE_TIMERS`.
    +130, **zero regressions** (proves the per-TU restore mechanism).
  - `f9588ba` — per-TU push_macro PathName tail for 4 specific TUs (UIListCustom/
    UIListSlot/WorldInstance/ModalKeyListener). +4. Measured **~8448**.

**The reconciliation = spike's GLOBAL END tail + tiny's `MILO_MESSAGE_TIMERS`
gate (NOT `HX_NATIVE`) + restore the timer in EVERY timer-on TU.**

## Ground truth established (this investigation)

Per-Handle timer/pathname/frame from dtk-split target asm:

| unit | Handle fn | timer_ctor | pathname_tail | frame | spike loss |
|---|---|---|---|---|---|
| BandDirector | fn_822804F8 | **1** | 0 | **0x240** | -24 |
| Draw | fn_823F47C0 | **1** | 1 | 0xd0 | -5 |
| StorePanel | fn_827923A0 | **1** | 1 | 0xa0 | -3 |
| Anim | fn_823EF728 | **1** | 1 | 0xb0 | -2 |
| UI | fn_827DF8B8 | **1** | — | — | -3 |
| FlowSound | (none timer-bearing) | **0** | — | — | -1 (NOT recoverable) |
| CharBoneDir/CharLipSync/Group/Dir | — | (timer-on; tiny restores) | — | — | -4 (e57d204) |

- `fn_82725EE8` = MessageTimer ctor / `Timer::Restart` block (constructed at
  frame+0x154 in BandDirector); `fn_8279B788` = `??0Symbol@@QAA@PBD@Z`
  (`Symbol::Symbol(const char*)`) for the timer's message Symbol, behind the
  function-static init guard (`lwz/clrlwi. 31/bne/ori/stw`).
- `fn_82732F68` = `PathName(const Hmx::Object*)` (the END_HANDLERS tail side effect).
- BandDirector::Handle has **NO** pathname_tail (ends via HANDLE_SUPERCLASS forward,
  not the unhandled tail) → its -24 is **purely timer-frame-coupled**, recoverable by
  timer restore ALONE (the END tail change doesn't even touch it).
- At-risk 0x28-funclet@100 pools on main (the loss reservoir): BandDirector 46,
  Dir 23, CharBoneDir 20, CharLipSync 15, Anim 13, StorePanel 7, Draw 6, UI 6,
  Group 0, FlowSound 0.

## Verdict & the gate decision

The frontier thesis is REAL. The **gate name is the crux**: spike's `HX_NATIVE`
gate is wrong because it cannot restore per-TU; tiny's `MILO_MESSAGE_TIMERS` gate
is correct (mirrors `RB3_RBTREE_0x1C` per-TU pattern). The reconciliation must
adopt the `MILO_MESSAGE_TIMERS` gate, layer the spike's global END tail on top,
and restore the timer in ALL timer-on TUs — a SUPERSET of the frontier's list.

## Actionable (self-contained, ONE worktree, vs main@8314)

**reconcile-handle-prereq-FINAL** (kind=header-macro). In ONE worktree via
`scripts/setup_worktree.sh`, branched from main@8314:

1. **Object.h BEGIN_HANDLERS / BEGIN_CUSTOM_HANDLERS**: gate the MessageTimer on
   `#ifdef MILO_MESSAGE_TIMERS` (tiny's `b259212` form), NOT `#ifdef HX_NATIVE`.
   Undefined default = retail (timer off).
2. **Object.h END_HANDLERS / END_CUSTOM_HANDLERS**: spike's `7043e56` global tail
   — `if(_warn) (void)(PathName(this), sym);` and CUSTOM
   `(void)(PathName(dynamic_cast<Hmx::Object*>(this)), sym);`, gated `#ifndef HX_NATIVE`.
   Do NOT touch the global MILO_NOTIFY macro.
3. **UIComponent.cpp/.h**: carry over e57d204's resource-handler port
   (ResourceDir() + OnGetResourcesPath, handler block to retail 5+4 form).
4. **objects.json**: add `extra_cflags ["/DMILO_MESSAGE_TIMERS"]` to **every
   timer-on TU**: `bandobj/BandDirector.cpp` (NEW; the dominant +24),
   `char/CharBoneDir.cpp`, `world/Dir.cpp`, `rndobj/Group.cpp`,
   `char/CharLipSync.cpp` (the 4 tiny restores), PLUS try `rndobj/Draw.cpp`,
   `meta/StorePanel.cpp`, `ui/UI.cpp`, `rndobj/Anim.cpp` (verify each: A/B their
   own funclet pool recovers; keep only the net-positive ones). NOT FlowSound
   (no timer; its -1 is unrecoverable END-tail near-miss — accept).
5. **target_symbol_map.json**: spike's 4 sliver pairings (51fc194) +
   e57d204's 2 + tiny's overlapping entries (dedupe). All attribution claims —
   verify byte-exact post-pair via objdiff.
6. **Whole-binary A/B vs main@8314**: `rm -f build/45410914/target_symbol_renames.stamp
   && touch config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`
   (re-run once for splits FP). Honesty gate: net >= +1, no >=8-contiguous foreign
   fn_@0% run in any changed range, headline == sum of intended unit gains, and it
   does NOT regress the +23 MILO_WARN/NOTIFY no-op (only END_HANDLERS' use changes).
   Confirm BandDirector::Handle's 46 funclets STAY 100% (verify the timer restore
   pins frame 0x240). Projected ~**+225** (8539).

**Attribution risk:** TRUE (adds sliver pairings + per-TU pins; verify each
byte-exact). This is the DOMINANT Handle-wave lever — blocks the whole wave.

## Discovered frontier (adjacent leads, not fully planned)

- **timer-on TU census**: a systematic scan of `build/45410914/asm/*.s` for
  Handle fns containing `bl fn_82725EE8` would produce the COMPLETE timer-on TU
  list (so the `/DMILO_MESSAGE_TIMERS` restore set is exhaustive, not just the
  spike-observed 9). Each timer-on wired TU that ISN'T restored will silently lose
  its Handle funclets under the global drop. EV: completes the reconciliation
  ceiling (could be > the 9 observed). kind=tooling/census.
- **END_CUSTOM_HANDLERS dynamic_cast tail** (4 users: UIListProvider, ScrollSelect,
  LightPresetManager, CharacterTest): the spike's CUSTOM tail emits
  `PathName(dynamic_cast<Hmx::Object*>(this))` — verify the `__RTDynamicCast`
  emission matches retail on these 4 before trusting the global CUSTOM form (none
  were in the spike loss list, so likely fine, but unverified). kind=header-macro.
- **Family A HANDLE_CHECK comma-form** (37 ObjMacros.h TUs, only GuitarController
  patched): a global `HANDLE_CHECK` comma-form (HX_NATIVE-gated) would do for
  Family A what this does for Family B. Verify no +23 WARN no-op regression.
  kind=header-macro. (carried from L3.)
- **The 85 wired Handle pairing wave**: once the reconciled head+tail+gate land,
  each small wired Handle (PropKeys/Flow/UIGuide/NetCacheMgr/CharFaceServo/...) is
  a self-contained pin+pair item — near-certain flips. kind=pin/pair. (carried from L2/L3.)
