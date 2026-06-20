# W9 L6 — family-a-handle-check-comma-form (ADVERSARIAL DISCOVER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the lever is real, but the frontier's framing is
**necessary-not-sufficient** (the exact same trap L3 caught for Family-B's
tail-only edit). A tail-only `HANDLE_CHECK` comma-form flips **ZERO** Family-A
Handle bodies to 100%. The correct actionable is a **Family-A reconcile**
(head-timer drop + tail comma-form + per-body pairing), mirroring L5's
reconcile-handle but for `ObjMacros.h`.
**Mode:** read-only in main; ground truth from `run_objdiff` (GuitarController,
DancerSequence), the dtk-split target `.s`, `report.json`, and the COFF objs.

## TL;DR (what the frontier got right / wrong)

- **RIGHT:** Family-A `HANDLE_CHECK` *does* drop the retail `PathName(this)`
  side effect, because `HANDLE_CHECK` -> `MILO_WARN(...)` and our global
  `MILO_WARN(...) = ((void)sizeof(MakeString(...)))` is **unevaluated**
  (`Debug.h:149`). Retail (and rb3-Wii release `MILO_WARN = (void)(__VA_ARGS__)`,
  `../rb3/.../Debug.h:151`) keeps the eval. GuitarController already proves the
  comma-form fix works (its tail is byte-identical, idx 83-87 below).
- **WRONG #1 (mechanism — the dominant blocker is the HEAD, not the tail):**
  Family-A `BEGIN_HANDLERS` (`ObjMacros.h:73-79`) is `#ifdef MILO_DEBUG`-gated,
  and `macros.h:3` force-defines `MILO_DEBUG` tree-wide → every Family-A Handle
  emits a spurious `MessageTimer timer(...)` that **retail stripped**.
  GuitarController's own in-source comment is the ground truth:
  *"the retail target's GuitarController::Handle has zero Timer references"* —
  so it `#undef BEGIN_HANDLERS` to the timer-OFF form **in addition to** the
  MILO_WARN comma-form. **The tail fix alone flips nothing while the head timer
  is still emitted.** This is identical to the L3 finding for Family-B.
- **WRONG #2 (EV — "+15" is the gross flip ceiling, not the net):** there is
  **no standing pool of already-paired near-100 Family-A Handles gated solely on
  the tail.** Only **3 Handle bodies are paired binary-wide** (GuitarController,
  DancerSequence[Family-B], UIManager[Family-B]); the only paired Family-A one
  (GuitarController) is already solved-but-stuck at 97.6% on an UNRELATED
  bool_mask+offset_swap residual. Every Family-A flip therefore needs head-drop
  **+** tail-comma **+** a `target_symbol_map` pairing **+** a clean residual, all
  together. Realistic net is the subset of the **17 pinned** Family-A Handle
  bodies whose body is otherwise byte-exact after head+tail+pair — single digits,
  not +15, and only via the full reconcile.
- **SEQUENCING NOTE:** the frontier says "de-risked by Family-B success", but on
  **main@8314 the Family-B reconcile is NOT landed** (`Object.h:928-930` still
  emits the timer unconditionally; `Object.h:1031-1032` END_HANDLERS still uses
  the sizeof-stripped `MILO_NOTIFY`). Family-A is INDEPENDENT of Family-B
  (different macros in a different header), so it does not need Family-B first —
  but it cannot inherit a "proven on main" template either. It must stand alone.

## Ground truth established (this investigation)

### The two macros that block Family-A (root of everything)
- `src/macros.h:3` `#define MILO_DEBUG` (tree-wide; load-bearing elsewhere — do
  NOT undefine globally).
- `ObjMacros.h:73-79` (head): `#ifdef MILO_DEBUG` arm of `BEGIN_HANDLERS` emits
  `MessageTimer timer((MessageTimer::Active()) ? static_cast<Hmx::Object*>(this) : 0, sym);`.
  Retail Family-A Handle has **no** timer (GuitarController in-source proof).
- `ObjMacros.h:210-214` (tail): `HANDLE_CHECK(line_num)` ->
  `if (_warn) MILO_WARN("%s(%d): %s unhandled msg: %s", __FILE__, line_num, PathName(this), sym);`.
  `MILO_WARN` sizeof-form drops the `PathName(this)` vcall side effect retail keeps.
  (Note: L3 cited `ObjMacros.h:191`/`:210-218`; the **actual** HANDLE_CHECK is at
  `ObjMacros.h:210-214` in our tree — line refs corrected here.)

### GuitarController::Handle (the SOLVED reference) — run_objdiff, full_listing
- 97.6% normalized (`fn_82778070`, in its own pin `0x82777E90-0x8277D790`).
- Tail (idx 83-87) is **byte-identical target==base**:
  `clrlwi. r11,r25,24 / beq / mr r3,r29 / bl ?PathName@@YAPBDPBVObject@Hmx@@@Z / li r11,0x6`.
  This is exactly what a global `HANDLE_CHECK` comma-form would produce for the
  other 36 TUs. **Proves the comma-form is correct.**
- Frame = 0xc0 with **no** MessageTimer ctor/Timer::Restart — proves Family-A
  retail Handle is timer-OFF (the head drop is the right direction).
- Residual 2.4% = idx 32-34 + 66-68: an extra `clrlwi r11,r3,24` (BOOL_MASK,
  bit 24) + a stack-slot OFFSET_SWAP (0x58/0x5c, 0x60/0x64) — **permuter-class,
  unrelated to the tail or head**. So even a perfectly-formed Family-A reconcile
  leaves Guitaranalogues at <100 if they carry the same bool/offset residual.

### Pairing & measurability reality
- `report.json` lists **only 3** paired `?Handle@…@@UAA/MAA` bodies
  (GuitarController/DancerSequence/UIManager); all read `0.00` in the *report*
  (normalized, needs 100). GuitarController's live 97.6% is sub-100 → doesn't count.
- The other 16 pinned Family-A TUs' Handle bodies are **anonymous `fn_<VA>`**, not
  paired in `target_symbol_map.json`, so they read "Stub/all-insert" until a map
  entry is added (the pairing step = attribution claim).
- Family-A TUs split **17 pinned / 17 unpinned**:
  - **PINNED (Handle body measurable post head+tail+pair):** BandCamShot,
    BandCharacter, BandCharDesc, BandIKEffector, BandWardrobe, CalibrationPanel
    (3 Handle bodies: CalibrationPanel/CalibrationModesProvider/CalibrationWelcomePanel),
    GameMode, **GuitarController (already solved)**, MasterAudio, MusicLibrary,
    OvershellSlot, QuestFilterPanel, RockCentral, TourDescPanel, TrackPanelDir,
    UI, VocalTrack, VocalTrackDir.
  - **UNPINNED (pin-gated — macro can't help until the TU's Handle .text is
    pinned):** AccomplishmentManager, AccomplishmentPanel, AccomplishmentProgress,
    BandStoreUIPanel, EventAnim, GamePanel, GemTrack, HamProviderPrinter,
    NameGenerator, NetSync, Player, SongSortByRank, StoreSongSortNode,
    TrainerPanel, UIEventMgr, UploadErrorMgr, VocalPlayer.

### DancerSequence::Handle (paired, Family-B, sanity) = 20%, 199 deletes
- Fully-unported body (target has 199 instructions base lacks) — a reminder that
  many Handle bodies are **body-divergent**, not merely macro-blocked. The macro
  fix is necessary infrastructure but each body still needs to be otherwise exact.

## Verdict & the correct actionable

The frontier thesis is REAL but the standalone "global `HANDLE_CHECK` comma-form,
+15" framing is wrong (flips zero alone). The de-risked, correctly-scoped item is
a **Family-A reconcile** that does BOTH macro halves globally (single direction —
Family-A is uniformly timer-OFF in retail, simpler than Family-B) PLUS the
per-body pairing, in ONE worktree, whole-binary A/B. It is **independent** of the
Family-B reconcile (different header), so it lands standalone vs main@8314.

## Actionable (self-contained, ONE worktree, vs main@8314)

**family-a-reconcile-handle** (kind=header-macro). Branch from main@8314 via
`scripts/setup_worktree.sh`:

1. **`ObjMacros.h:73-79` HEAD (timer drop):** gate the `#ifdef MILO_DEBUG`
   MessageTimer arm of `BEGIN_HANDLERS` so the timer is dropped for the *match*
   build while kept for native. Mirror GuitarController's in-source override but
   globally: keep the timer only under `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`
   (or a dedicated `#ifndef HX_NATIVE` drop). Retail Family-A is uniformly
   timer-OFF (GuitarController proof), so — unlike Family-B — NO per-TU
   `/DMILO_MESSAGE_TIMERS` restore is needed. **Verify** this assumption on 2-3
   pinned bodies (BandCharacter/CalibrationPanel) before trusting it globally.
2. **`ObjMacros.h:210-214` TAIL (comma-form):** change `HANDLE_CHECK` so the
   `if (_warn)` arm comma-evaluates the args (keeping `PathName(this)`) instead of
   routing through the sizeof-stripped `MILO_WARN`. Either
   `if (_warn) (void)(__FILE__, line_num, PathName(this), sym);` directly, or a
   local `MILO_WARN(...) = ((void)(__VA_ARGS__))` form — match GuitarController's
   emission exactly (idx 83-87). Gate `#ifndef HX_NATIVE` so native keeps the real
   warner. Do NOT touch the global `MILO_WARN`/`MILO_NOTIFY` definitions — only
   `HANDLE_CHECK`'s use — so the **+23 WARN/NOTIFY no-op stays intact**.
3. **Remove GuitarController's now-redundant per-TU overrides** (`#undef MILO_WARN`
   + `#undef BEGIN_HANDLERS`, GuitarController.cpp:17-20 + 300-305) so the global
   form is the single source — and confirm GuitarController::Handle stays 97.6%
   (its residual is bool/offset, untouched).
4. **`target_symbol_map.json` pairings:** add `fn_<VA> -> ?Handle@Class@@UAA…`
   for each pinned Family-A Handle that becomes byte-exact after 1+2. Candidates
   (pinned, body must be verified byte-exact post-pair — attribution claim each):
   QuestFilterPanel, CalibrationPanel (×3), MusicLibrary, MasterAudio, OvershellSlot,
   TourDescPanel, RockCentral, TrackPanelDir, VocalTrack, VocalTrackDir, UI,
   BandCamShot, BandCharacter, BandCharDesc, BandIKEffector, BandWardrobe.
   VAs come from each TU's pin span + the rdata vtable Handle slot (use the
   auto_00 rdata vtable dump + `??_7Class@@6B@` anchor). Add an entry ONLY if
   run_objdiff shows the body 100% post-pair; drop any with a bool/offset/body
   residual (defer to permuter/bodyport).
5. **Whole-binary A/B vs main@8314:**
   `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml
   && NINJA_JOBS=8 tools/fresh_report.sh` (re-run once for the splits FP). Honesty
   gate: net>=+1; headline == sum of intended per-TU Handle flips; **no regression
   to the +23 MILO_WARN/NOTIFY no-op** anywhere (only HANDLE_CHECK's use changed);
   no >=8-contiguous foreign fn_@0% run in any changed range; GuitarController stays
   ≥97.6%. REVERT pairings that read <100.

**Attribution risk:** TRUE (adds per-body `target_symbol_map` pairings; verify each
byte-exact post-pair via objdiff). Header edit = full A/B mandatory.

## Discovered frontier (adjacent leads, not fully planned)

- **family-a-pin-then-pair wave (17 unpinned TUs):** AccomplishmentManager/Panel/
  Progress, Player, GemTrack, VocalPlayer, UIEventMgr, TrainerPanel, GamePanel,
  EventAnim, NameGenerator, NetSync, UploadErrorMgr, SongSortByRank,
  StoreSongSortNode, BandStoreUIPanel, HamProviderPrinter — their Handle bodies
  live in unpinned `auto_03_*` blobs. Each is a self-contained pin (derive the
  Handle .text span from the vtable VA + neighbour splits) + the family-a-reconcile
  macro (prereq) + pair. EV: extends the Family-A yield beyond the 17 pinned.
  kind=pin/pair. attribution_risk=true.
- **GuitarController/CalibrationPanel bool_mask+offset_swap residual:** the 2.4%
  that survives the macro fix (extra `clrlwi r11,r3,24` + 0x58/0x5c stack-slot
  swap) is a recurring `_HANDLE_CHECKED` result-store shape (`DataNode result =
  expr; if (result.Type()!=kDataUnhandled)`). A permuter sweep or a source
  restructure of `_HANDLE_CHECKED` (ObjMacros.h:142) that matches retail's
  store-order might lift the whole Family-A wave the last 2-3%. kind=permuter/macro.
- **`_HANDLE_CHECKED` / `HANDLE_MESSAGE` store-order (force-mult):** the offset_swap
  (target stores `result` at +0x58 BEFORE the type-byte at +0x5c; base reverses)
  recurs across every HANDLE_MESSAGE user. If `_HANDLE_CHECKED`'s temp-DataNode
  declaration order is flipped to match retail, it could fix the residual for ALL
  Family-A AND Family-B Handles at once — a header-level force-multiplier worth a
  spike. kind=header-macro. (verify direction on GuitarController idx 32-34 first.)
