# W9 L4 — wired-handle-pairing-wave-85 (ADVERSARIAL DISCOVER/PLANNER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the lever is real and EMPIRICALLY PROVEN, but the
frontier's framing needs three load-bearing corrections (count, prereq-state,
per-fn confidence is NOT uniform). Cold-executable work-items below.
**Mode:** read-only in main; ground truth from the two existing prereq worktrees
(objdiff at 100%) + per-unit target asm + source handler blocks.

## TL;DR — what's RIGHT / WRONG in the frontier

- **RIGHT (proven):** after the macro prereqs land, wired END_HANDLERS-shaped
  Handle bodies become byte-exact and only need a `target_symbol_map.json` entry
  to flip. **Demonstrated**: worktree
  `wt-w9-spike-strategyB-global-end-handlers-pathname-tail` (@51fc194) is at
  **8502 matched (+188 vs main)** with the two macro prereqs + 6 paired Handles;
  `run_objdiff` confirms `UIListCustom::Handle`=100.0% and `CharIKFoot::Handle`
  =100.0% — both flipped from a bare map entry, NO source edit.
- **WRONG #1 (the prereqs are NOT yet on main, AND there are TWO competing
  implementations).** main @812e1df still emits the head MessageTimer
  (`Object.h:927`) and the sizeof-stripped END_HANDLERS tail (`Object.h:1032`).
  Two unmerged worktrees implement the prereq DIFFERENTLY — they must be
  reconciled by the coordinator BEFORE this wave:
  - **Approach B (spike, 51fc194):** head timer gated `#ifdef HX_NATIVE`; END_HANDLERS
    PathName tail rewritten GLOBALLY in `Object.h` (+139 head).
  - **Approach A (handle-wave-tiny, f9588ba):** head timer gated
    `#ifdef MILO_MESSAGE_TIMERS` (+130 head); PathName tail done PER-TU
    (push_macro/undef/pop_macro). **A explicitly restores 4 TUs that retail
    compiled WITH the timer** — CharBoneDir, CharLipSync, Dir, Group — via
    `objects.json /DMILO_MESSAGE_TIMERS` (the RB3_RBTREE_0x1C per-TU pattern). B
    does not mention these → B may silently regress those 4 (audit before
    landing B's higher +139).
- **WRONG #2 (count): not 85 "near-certain", and ~30 small ones are NOT all
  trivial.** This investigation found **107 END_HANDLERS-shaped functions in
  wired TUs**, of which **6+1 are already paired** (UIComponent, UIListCustom,
  UIListSlot, WorldInstance, CharIKFoot, GuitarController, UIManager) leaving
  **100 unpaired**. The "85" was the L2 count; the true wired set is larger but
  the realistic FLIP yield is bounded by per-fn handler-list match, not the
  count. Honest EV for the small/mid tier is **+18..+30** depending on how many
  have source-vs-retail divergences.
- **WRONG #3 (confidence is per-fn, even in the small tier).** Several SMALL
  candidates have a source/retail SUPERCLASS divergence and will NOT flip from a
  bare map entry — they need a 1-line source fix first:
  - **UIGuide::Handle (48 instr)**: source `HANDLE_SUPERCLASS(Hmx::Object)` but
    retail forwards to `fn_827D9928` = **UIComponent::Handle**. Source bug.
  - **FlowIf::Handle (51 instr)**: source `HANDLE_SUPERCLASS(FlowNode)` but
    retail forwards to `fn_827371D8` = **Hmx::Object::Handle**. Source bug.
  - **Flow::Handle (48 instr)**: source has 5 handlers + 2 supers; the target's
    super-bl resolves to a TexRenderer-range VA — verify the actual handler list.

## Ground truth established

### The prereq macro change (both are needed; B is the cleaner global form)
The spike's `Object.h` diff (the canonical 2-part prereq) is fully captured in
`git diff main..51fc194 -- src/system/obj/Object.h` (worktree
`wt-w9-spike-strategyB-global-end-handlers-pathname-tail`):
1. BEGIN_HANDLERS / BEGIN_CUSTOM_HANDLERS: drop the `MessageTimer timer(...)`
   under `#ifndef HX_NATIVE` (retail has NO profiling timer in Handle heads).
2. END_HANDLERS / END_CUSTOM_HANDLERS: emit `if(_warn) (void)(PathName(this),sym);`
   (CUSTOM: `PathName(dynamic_cast<Hmx::Object*>(this))`) under `#ifndef HX_NATIVE`,
   instead of the sizeof-stripping `MILO_NOTIFY(...)`. Keeps the PathName side
   effect retail emits in ~505 Handle tails.

### Empirical flip proof (run_objdiff, project_dir = spike worktree)
- `?Handle@UIListCustom@@UAA?AVDataNode@@PAVDataArray@@_N@Z` → **100.0% normalized**
  (41 instrs, all equal). Source = bare `HANDLE_SUPERCLASS(UIListSlot)` matches
  target (super-bl fn_827EFE58 = UIListSlot::Handle, then PathName tail).
- `?Handle@CharIKFoot@@UAA?AVDataNode@@PAVDataArray@@_N@Z` → **100.0% normalized**
  (48 instrs). Source = `HANDLE_SUPERCLASS(CharIKHand)`.

### The 100 unpaired wired END_HANDLERS-shaped functions
Full census in `/tmp/wired_handles2.json` (regenerate: grep wired-unit `.s` for
`bl fn_82732F68` immediately followed by `li r11,0x6`; only per-unit `.s` exist
for pinned ranges, so the set is wired-by-construction). Size tiers (target
instr count):

| tier | instr | count (unpaired) | nature |
|---|---|---|---|
| small | 44–67 | ~24 | superclass-forward + PathName-tail; mostly bare HANDLE_SUPERCLASS |
| mid | 68–130 | ~30 | a few handlers + super + tail |
| large | 131–1604 | ~46 | full handler lists (UIList 783, AsyncFileHolmes 891, Part 893, Rnd 1604…) — flip only if our handler list == retail's |

### ATTRIBUTION TRAP confirmed (set attribution_risk=true)
The owning `.s` file name is NOT the Handle's class. Verified: `Rnd.cpp`
fn_823FEDE0 is **`?Handle@ModalKeyListener@@`** (not Rnd::Handle — handle-wave-tiny
paired it correctly). `Instance.cpp` fn_824D7C28 is **`?Handle@WorldInstance@@`**.
Every map entry is an attribution claim: pull the exact mangled `?Handle@Class@@UAA...`
from the rb3-Wii/DC3 oracle for the REAL class, add the entry, objdiff to confirm
100% byte-exact BEFORE trusting it.

### Common superclass-forward VAs (resolved)
- fn_827371D8 = `Hmx::Object::Handle` (the base, per L1 uicomp dossier)
- fn_827D9928 = `UIComponent::Handle`
- fn_823E7E40 = RndTransformable::Handle, fn_823F47C0 = RndDrawable::Handle,
  fn_8240E828 = RndPollable::Handle (per L1 uicomp dossier)
- fn_8279B788 = a HANDLE-dispatch helper that appears as the first `bl` after
  `Sym(1)` in ~20 bodies (NOT a superclass Handle — do not treat as one).

## Pin/wired status — ALL small-tier owner TUs are pinned
Spot-checked `config/45410914/splits.txt`: PropKeys (DUAL range, the 0x82649xxx
candidates are in the SECOND range [0x82649C38,0x8264B5F8)), UIGuide, Flow,
StorePanel, NetCacheMgr, CharFaceServo, CharSleeve, FlowIf, CharPosConstraint,
Console, FxSend, StreamNull, ConnectionStatusPanel — all have a `.text` pin
covering their candidate VA. No new pins are needed for the small/mid tier; the
work is map entries (+ occasional 1-line source fix), NOT splits.txt edits.

## Verdict & sequencing

REAL_ACTIONABLE. SEQUENCING (hard): this wave is the THIRD step and is dependent.
The coordinator must FIRST land ONE reconciled prereq on main (recommend
Approach A's reconciliation — keep the 4 timer-restore TUs — OR Approach B's
global END_HANDLERS form PLUS A's 4 restores; whichever A/Bs higher with zero
regressions). Only then do the pairing sub-batches below land independently vs
the new main. Each item below is self-contained (1 worktree branched from the
prereq tip; map entries + any source fixes + objdiff + whole-binary A/B).

## Actionable (self-contained; each = ONE worktree off the prereq tip)

See the structured items. Two sub-batches by confidence tier, plus a recon
item for the source-divergent smalls.
