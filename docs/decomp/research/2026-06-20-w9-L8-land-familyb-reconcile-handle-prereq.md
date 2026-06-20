# W9 L8 — land-familyb-reconcile-handle-prereq (ADVERSARIAL DISCOVER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched — CURRENT HEAD, moved past 8314-claim era; 8314 is also the literal number now)
**Verdict: REAL_ACTIONABLE** — the lever is REAL, the implementation is ALREADY DONE
on a measured branch, and it is independently landable. The frontier's est +225 is
NOT achieved; ground-truth is **+196** (zero-regression, reproduced in TWO worktrees).
The remaining ~+3–10 is genuine ADDITIVE adjacent work (NOT recovered losses — the
disputed TUs are 0% on BOTH sides).
**Mode:** read-only in main; ground truth = the two already-built landing worktree
report.json's + dtk-split target asm + objdiff + git history.

## TL;DR — what the frontier / L5 got right and wrong

- **RIGHT (mechanism):** Two-part Object.h Family-B Handle reconcile = (a) gate the
  per-handler `MessageTimer` behind `#ifdef MILO_MESSAGE_TIMERS` (NOT `HX_NATIVE`),
  undefined-default = retail timer-off, per-TU restorable; (b) global `END_HANDLERS`
  / `END_CUSTOM_HANDLERS` PathName(this) comma tail `if(_warn)(void)(PathName(this),sym);`
  gated `#ifndef HX_NATIVE`; (c) UIComponent resource-handler body-port; (d) restore
  the timer in the timer-on TUs via objects.json `/DMILO_MESSAGE_TIMERS`; (e) sliver
  Handle pairings in target_symbol_map. **All verified correct.**
- **ALREADY IMPLEMENTED & MEASURED:** branch `w9-reconcile-handle-prereq-FINAL`
  @9fb9016 (== a7175af). Built in BOTH `wt-w9-land-reconcile-handle-prereq-9fb9016`
  (a7175af) and `wt-w9-reconcile-handle-prereq-FINAL` (9fb9016): **report.json
  measures.matched_functions = 8510** in both = **+196 vs main 8314**. The "+196"
  commit-message claim is accurate and reproducible.
- **WRONG #1 (EV):** frontier est +225, L5 projected ~+225 ceiling. Actual **+196**.
  The ~29-fn gap is the un-restored Draw/StorePanel/Anim Handle bodies (+ partial UI).
- **WRONG #2 (L5's loss framing):** L5 claimed restoring Draw(-5)/StorePanel(-3)/
  Anim(-2) *recovers losses*. GROUND TRUTH (objdiff + per-fn report): Draw::Handle
  (fn_823F47C0), StorePanel::Handle (fn_827923A0), Anim::Handle (fn_823EF728) are
  **norm=0.0% on BOTH main AND the FINAL build** — they were never 100%, so there is
  NO loss to recover. The FINAL build is genuinely **zero-regression** (per-unit AND
  per-function delta scans confirm: 0 fns went 100→0 anywhere). Restoring those TUs
  is **ADDITIVE** (flip new fns), not defensive.
- **WRONG #3 (restore list):** FINAL restored 6 TUs (BandDirector, CharBoneDir, UI,
  Dir, Group, CharLipSync) — all show **+0 delta** (timer restore exactly offsets the
  global drop, net-neutral, defensive, correct). Draw/StorePanel/Anim were left OUT;
  asm proves they ARE timer-on (`bl fn_82725EE8` INSIDE the Handle fn range) and in
  PINNED units — so they're additive targets, see adjacent work.

## Ground truth established (this investigation)

### Measurement (the load-bearing claim)
| worktree | HEAD | matched | vs main |
|---|---|---|---|
| main | 812e1df | 8314 | — |
| wt-w9-land-reconcile-handle-prereq-9fb9016 | a7175af | **8510** | **+196** |
| wt-w9-reconcile-handle-prereq-FINAL | 9fb9016 | **8510** | **+196** |

Per-unit delta (main→FINAL), top gains, **ZERO regressions**: Part +26, UIList +18,
MidiParser +14, CameraShot +10, UIComponent +9, Gen +9, JoypadClient +5,
ContentMgr_Xbox +5, UISlider/Spotlight/Line/LightHue/HeldButtonPanel/Crowd +4, …
net = +196. Per-fn scan of Part/UIList/MidiParser/UIComponent: every unit is pure
gains, 0 functions regressed 100→0.

### The FINAL branch diff (5 files, self-contained, branches off main)
- `src/system/obj/Object.h`: BEGIN_HANDLERS/BEGIN_CUSTOM_HANDLERS `#ifdef MILO_MESSAGE_TIMERS`
  gate (timer off default); END_HANDLERS/END_CUSTOM_HANDLERS `#ifndef HX_NATIVE` →
  `(void)(PathName(this),sym)` / `(void)(PathName(dynamic_cast<Hmx::Object*>(this)),sym)`.
- `src/system/ui/UIComponent.cpp/.h`: ResourceDir() + OnGetResourcesPath() body-port
  (matches **rb3-Wii** UIComponent.cpp:365 `if(mResourceDir)return mResourceDir;…`,
  NOT DC3) + the 2 new HANDLE_EXPR/HANDLE entries (get_resource_dir, get_resources_path).
- `config/45410914/objects.json`: `/DMILO_MESSAGE_TIMERS` on BandDirector, CharBoneDir,
  UI, Dir, Group, CharLipSync.
- `scripts/target_symbol_map.json`: 6 new Handle pairings (CharIKFoot, ModalKeyListener,
  WorldInstance, UIComponent, UIListSlot, UIListCustom).

### Timer-on confirmation (dtk-split target asm; fn_82725EE8 = MessageTimer ctor)
| Handle fn | unit | timer ctor INSIDE fn range | main% | FINAL% |
|---|---|---|---|---|
| fn_822804F8 | BandDirector | yes (frame 0x240, 46 0x28-funclets@100) | — | restored, +0 |
| fn_823F47C0 | Draw [1280-1549], timer@1294 | **yes** | **0.0** | **0.0** (NOT restored) |
| fn_827923A0 | StorePanel [3390-3444], timer@3404 | **yes** | **0.0** | **0.0** (NOT restored) |
| fn_823EF728 | Anim [3320-3623], timer@3334 | **yes** | **0.0** | **0.0** (NOT restored) |
| fn_827DF8B8 | UI, timer@827DF8EC | yes | — | restored, +0 |
| FlowSound::Handle | FlowSound | **no** (0 timer ctors in TU) | — | -1 unrecoverable |

All three additive-target Handle fns are in **PINNED** units:
Draw.cpp .text [0x823F3B10,0x823F4D20), Anim.cpp [0x823EDA90,0x823F2D58),
StorePanel.cpp [0x8278FE70,0x827928C8) — each covers its Handle fn. They're at 0%
because the global timer drop changed their expected codegen but their TU lacks the
`/DMILO_MESSAGE_TIMERS` restore. NOT paired in target_symbol_map (still `fn_<VA>`).

### Vestigial-but-harmless note
FINAL's UIComponent.cpp still carries `#pragma push_macro("MILO_NOTIFY") / #undef /
#define MILO_NOTIFY(...) (void)(__VA_ARGS__)` around its BEGIN/END_HANDLERS. Since
the global END_HANDLERS no longer references MILO_NOTIFY (it emits the comma form
directly), this local redefine is now **dead/no-op**. Harmless; can be dropped in a
cleanup but is NOT a correctness or measurement issue.

## Verdict

**REAL_ACTIONABLE.** The dominant Family-B Handle prereq is real, correctly
implemented, and **independently landable at +196, zero regression**, on branch
@9fb9016 (5-file diff off main). Land it AS-IS first (it unblocks the binary-wide
Handle reveal cascade). Then the additive timer-restore follow-up below claims the
remaining Draw/StorePanel/Anim flips that L5 mis-framed as "losses."

## Actionable items (each self-contained, ONE worktree, vs main@8314)

### A1 — land-familyb-reconcile-handle-prereq-FINAL (kind=header-macro, +196)
Cherry-pick/rebase branch `w9-reconcile-handle-prereq-FINAL` @9fb9016 onto main (it
already branches off main@812e1df; the 5-file diff applies clean). Whole-binary A/B:
`rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml
&& NINJA_JOBS=8 tools/fresh_report.sh` (re-run once for splits FP). EXPECT 8510 (+196).
Honesty gate already proven: 0 unit regressions, 0 per-fn 100→0, headline == sum of
unit gains, BandDirector 172 (46 funclets stay 100). attribution_risk=TRUE (6 new
target_symbol_map Handle pairings + 6 per-TU pins). Optional cleanup: drop the now-dead
UIComponent.cpp push_macro/undef MILO_NOTIFY block (no measurement effect).

### A2 — handle-timer-restore-draw-storepanel-anim (kind=pin/pair, +3..+10) [DO AFTER A1]
In ONE worktree branched from main-WITH-A1-LANDED (depends on A1's Object.h gate):
add `/DMILO_MESSAGE_TIMERS` extra_cflags to `rndobj/Draw.cpp`, `meta/StorePanel.cpp`,
`rndobj/Anim.cpp` in objects.json (restores the timer so their Handle frame matches
retail), AND add target_symbol_map entries for the now-byte-exact Handle fns:
`0x823F47C0 -> ?Handle@RndDraw@@…` (verify exact mangling via auto_03 text symbol),
`0x827923A0 -> ?Handle@StorePanel@@…`, `0x823EF728 -> ?Handle@RndAnimatable@@…`
(Anim.cpp's class — confirm via auto_03). Whole-binary A/B vs A1 baseline. Keep ONLY
the TUs whose Handle flips to 100% AND whose own funclet pool doesn't regress (A/B
each: Draw 6 funclets, StorePanel 7, Anim 13). attribution_risk=TRUE. EV +3 (the 3
Handle bodies) to +10 (if restoring also re-aligns each TU's 0x28-funclet pool).
NOTE: must build to discover whether the timer restore makes the body byte-exact —
the END tail alone left them at 0%, so the timer is the missing half.

## Discovered frontier (adjacent leads — seed next layers)

- **timer-on wired-TU exhaustive census** (kind=tooling/census, +5..+20): scan
  `build/45410914/asm/*.s` `.fn …Handle… { … bl fn_82725EE8 … }` to enumerate EVERY
  pinned-unit Handle that is timer-on but lacks `/DMILO_MESSAGE_TIMERS`. Each is an
  A2-shaped pin+pair. The naive `grep -l fn_82725EE8` over-counts (timer is called
  from many non-Handle sites) — needs the fn-range-scoped filter. EV: completes the
  restore set beyond the 9 spike-observed.
- **the 85 wired Handle pairing wave** (kind=pin/pair, +30..+60): once A1 lands, the
  END tail makes ~85 wired Handles (PathName-tail-or-superclass-forward) byte-exact;
  each needs only a target_symbol_map entry to flip. L2's first-wave table lists them
  (PropKeys/Flow/UIGuide/NetCacheMgr/CharFaceServo/…). Self-contained pin+pair items;
  several already-spawned w9 worktrees (handle-pair-tier1/tier2/batch) target these —
  cross-check they rebase on A1's reconciled Object.h, not the un-reconciled spike/tiny.
- **Family A HANDLE_CHECK comma-form** (kind=header-macro, +? ): 37 ObjMacros.h TUs;
  only GuitarController has the per-TU `#undef MILO_WARN -> (void)(__VA_ARGS__)`. A
  global ObjMacros.h `HANDLE_CHECK` comma-form (HX_NATIVE-gated) does for Family A
  what A1 does for Family B. Worktrees `w9-family-a-reconcile-handle` (2073e3a) and
  `w9-land-family-a-handle-reconcile` (b3b419e) already exist — verify they don't
  regress the +23 WARN/NOTIFY no-op. (carried from L3/L5.)
- **END_CUSTOM_HANDLERS dynamic_cast tail** (kind=header-macro): A1's CUSTOM form
  emits `PathName(dynamic_cast<Hmx::Object*>(this))`; none of the 4 CUSTOM users
  (UIListProvider/ScrollSelect/LightPresetManager/CharacterTest) were in any loss
  list (FINAL is zero-regression), so the `__RTDynamicCast` emission is APPARENTLY
  fine — but unverified at the instruction level. Low priority; verify only if a
  CUSTOM-user Handle resists pairing post-A1.
