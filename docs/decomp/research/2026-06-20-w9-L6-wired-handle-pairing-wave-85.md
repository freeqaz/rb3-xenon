# W9 L6 — wired-handle-pairing-wave-85 (ADVERSARIAL DISCOVER/PLANNER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the lever is real and the prereq is now a MEASURED
worktree (+196), but the frontier's "+40 / ~85 self-contained flips" framing has
**three load-bearing errors**: (1) it is NOT self-contained vs main@8314 — every
item depends on the un-landed `reconcile-handle-prereq-FINAL` (9fb9016); (2) the
clean trivial-flip tier is **~10–13, not 85**; (3) ~30 of the 107 census entries
are **attribution-trap mis-pins** (owning-TU source has NO Handle for that class).
**Mode:** read-only in main; ground truth from the reconcile worktree
(`wt-w9-reconcile-handle-prereq-FINAL`, objdiff at 100% on paired ones), per-unit
target asm, COFF symbol parse of `auto_03_82260000_text.obj`, and source blocks.

## TL;DR — what's RIGHT / WRONG

- **RIGHT:** after the macro prereq, a wired END_HANDLERS-shaped Handle that is a
  *pure superclass-forward* (no own HANDLE_* actions) becomes byte-exact and
  flips from a bare `target_symbol_map.json` entry. **Proven** in the reconcile
  worktree: `?Handle@UIListCustom@@…` = 100.0% (41 instr, all equal),
  `?Handle@WorldInstance@@…` = 100.0% (48 instr). No source edit.
- **WRONG #1 — DEPENDENCY (the killer):** the +196 prereq is **NOT on main**.
  Main@812e1df still emits the head `DataNode::Sym` + sizeof-stripped END tail in
  its current macro form; only the reconcile worktree (`9fb9016`, measured **8510
  = +196**) has the head-gate + global PathName tail + the 6 `/DMILO_MESSAGE_TIMERS`
  restores. **No pairing item is independently landable vs main@8314.** Each must
  either branch off the prereq tip OR carry the whole prereq diff in-worktree.
  The two pre-created batch worktrees (`…small-tier1-char-clean`,
  `…mid-tier-per-fn-verify`) are sitting at bare main@812e1df — UNUSABLE until
  rebased onto the prereq.
- **WRONG #2 — COUNT:** not "~85 near-certain". Of 107 census entries: **8 already
  paired** in the reconcile map; **~10–13 clean pure-superclass-forwards** (the real
  near-certain tier); **~62 per-fn handler-list-verify** (flip iff our HANDLE_*
  list == retail's, per-fn); **~30 attribution-trap mis-pins** (source has no
  Handle for the owning unit's class — these are foreign-merged or different-class
  bodies; NOT bare-map flips). Honest EV for the clean tier ≈ **+10..+13**; the
  per-fn tier adds another **+15..+30** spread over many small body verifications.
- **WRONG #3 — fn_82725EE8 is NOT the MessageTimer.** Both L5 and the frontier
  reasoning leaned on "`bl fn_82725EE8` = timer". **Ground truth (objdiff index 13
  on UIListCustom):** `fn_82725EE8` pairs to `?Sym@DataNode@@QBA?AVSymbol@@PBVDataArray@@@Z`
  = **`DataNode::Sym(const DataArray*)`** (the `msg->Sym(1)` message-name extraction
  present at the head of EVERY BEGIN_HANDLERS body, timer-on or not). Its presence
  in PropKeys/StorePanel/UIGuide/NetCacheMgr does **NOT** mean timer-on. The real
  timer tell is `bl fn_8279B788` (`Symbol::Symbol(const char*)`) behind a
  *function-static init guard* AND a >0xb0 frame — and `fn_8279B788` ALSO appears
  as a plain `HANDLE("msg",…)` static-Symbol, so it is NOT a reliable timer tell
  either. Use the reconcile worktree's existing `/DMILO_MESSAGE_TIMERS` set
  (BandDirector, CharBoneDir, UI, Dir, Group, CharLipSync) as the authoritative
  timer-on list; do not re-derive it from a single bl.

## Ground truth established (this investigation)

- **Prereq is a real measured worktree:** `wt-w9-reconcile-handle-prereq-FINAL`
  @9fb9016 = **8510 matched (+196 vs main@8314)**. Its `target_symbol_map.json`
  already pairs 8 of the census Handles (UIComponent, UIManager×2, UIListSlot,
  UIListCustom, WorldInstance, CharIKFoot, ModalKeyListener) plus DancerSequence,
  GuitarController, QuestFilterPanel.
- **fn identity (COFF + objdiff resolved):**
  - `fn_82725EE8` = `DataNode::Sym(const DataArray*)` (Sym(1) extraction; in DataNode.s).
  - `fn_82732F68` = `PathName(const Hmx::Object*)` (END tail side effect).
  - `fn_827371D8` = `Hmx::Object::Handle` (base superclass forward).
  - `fn_827D9928` = `UIComponent::Handle`; `fn_823E7E40` = `RndTransformable::Handle`;
    `fn_823F47C0` = `RndDrawable::Handle`; `fn_8240E828` = `RndPollable::Handle`;
    `fn_825BE6A8` = `CharWeightable::Handle` (resolved from CharIK* forwards);
    `fn_827EFE58` = `UIListSlot::Handle`.
  - `fn_8279B788` = `Symbol::Symbol(const char*)` — a static-message-Symbol ctor
    used by both real `HANDLE(...)` actions AND (behind a static guard, big frame)
    the timer. NOT a superclass Handle; do not treat as one.
- **The `auto_03_82260000_text.obj` COFF dump is ANONYMOUS** — its 107917 symbols
  are `fn_<VA>`, NOT mangled retail names (it is dtk-split output, not a leaked
  map). So the COFF cannot name the real class for a mis-pinned body; the real
  identity must come from the rb3-Wii/DC3 oracle + the superclass-forward chain.
  (Parser: little-endian COFF header; symoff=0xda06e6, 18-byte records, strtab at
  symoff+n*18; `value` = section offset, VA = 0x82260000 + value.)
- **Mnemonic-stream proof of the clean tier:** CharFaceServo (fn_823909C0),
  CharSleeve (fn_823BD8A8), CharPosConstraint (fn_823B04E8) have **byte-identical
  mnemonic streams to each other** and match the proven CharIKFoot shape
  (HANDLE_SUPERCLASS + END_HANDLERS); their forward-bl is `fn_827371D8`
  (Hmx::Object::Handle), and all three sources are exactly
  `BEGIN_HANDLERS(X) / HANDLE_SUPERCLASS(Hmx::Object) / END_HANDLERS`. These are
  the canonical clean flips.

## Confidence tiers (107 census, 8 already paired → 99 in play)

### TIER 1 — clean pure-superclass-forward (near-certain flips, ~10–13)
Source = `BEGIN_HANDLERS(X) / HANDLE_SUPERCLASS(Super) / END_HANDLERS` with NO own
HANDLE_* action; target body = Sym + super-forward + (custom?dynamic_cast:)PathName tail.
| instr | unit | fn | super | sc_bl | note |
|---|---|---|---|---|---|
| 51 | CharFaceServo | fn_823909C0 | Hmx::Object | fn_827371D8 | mnemonic == proven |
| 51 | CharSleeve | fn_823BD8A8 | Hmx::Object | fn_827371D8 | mnemonic == proven |
| 51 | CharPosConstraint | fn_823B04E8 | Hmx::Object | fn_827371D8 | mnemonic == proven |
| 51 | FlowIf | fn_823B4E30 | FlowNode | fn_827371D8 | ⚠ L4: source `HANDLE_SUPERCLASS(FlowNode)` but target forwards to Hmx::Object::Handle — VERIFY (1-line source fix may be needed) |
| 67 | CharIKHead | fn_823ADF30 | CharWeightable | fn_825BE6A8 | clean |
| 67 | CharIKFingers | fn_8239FC68 | CharWeightable | fn_825BE6A8 | clean |
| 67 | CharBonesBlender | fn_823C6458 | CharPollable | fn_8240E828 | clean (forward via RndPollable) |
| 67 | CharEyeDartRuleset | fn_823ABFD8 | (verify) | fn_823E7E40 | RndTransformable forward |
| 67 | Waypoint | fn_823C8630 | RndTransformable | fn_823E7E40 | clean |
| 68 | CharCollide | fn_822B7860 | RndTransformable | (verify) | clean |
| 105 | DepthBuffer3D | fn_826DC060 | RndDrawable | (verify) | clean |
| 54 | UIGuide | fn_828020D0 | — | fn_827D9928 | ⚠ L4: source `HANDLE_SUPERCLASS(Hmx::Object)` but target forwards to UIComponent::Handle — 1-line source fix to `HANDLE_SUPERCLASS(UIComponent)` then flip |

### TIER 2 — per-fn handler-list verify (flip iff our HANDLE_* list == retail's, ~62)
Single-handler ones are the most tractable: ConnectionStatusPanel
(fn_82795BA0, 1×HANDLE_MESSAGE, super UIPanel), FxSend (fn_826F8968, 1×HANDLE_ACTION
test_with_mic), UIListDir (fn_827E6A10, 1×HANDLE_ACTION), UserMgr (fn_8250FE28,
1×HANDLE_EXPR), CharBoneDir×2 (timer-on; reconcile already has the flag), LightHue×2,
CharLipSync (timer-on), FxSendMeterEffect, ButtonHolder, etc. Each needs the source
handler list compared against the target's `HANDLE`-dispatch chain (count of
static-Symbol compares + bl targets) BEFORE the map entry — bare-map only flips if
identical. Many will flip; some have RB3-vs-DC3 handler-list divergence and need a
1-line source fix. Realistic yield +15..+30 spread across many small verifications.

### ATTRIBUTION TRAP — owning-TU source has NO Handle for that class (~30, DEFER)
PropKeys×2, Console, MatAnim, StreamNull×2, DataFunc, Cheats, PollAnim, TexBlender,
PartAnim, TexBlendController, TexRenderer, ContentMgr_Xbox, CharBonesMeshes,
ByteGrinder, Anim×2, Crowd, Draw, Tex, Gen, MoveVariant, Line, CameraShot, Dir,
MeshAnim, DirLoader, AsyncFileHolmes, Part. These are foreign bodies the linker
placed inside the unit's pinned range (e.g. PropKeys' 2nd .text range
[0x82649C38,0x8264B5F8) holds 0x82649F78/0x8264B118 but PropKeys has NO Handle in
source OR in rb3-Wii). `fn_827371D8` = `Hmx::Object::Handle` ITSELF is in this list
(DirLoader.s, 665 instr) — that is the base, already a full-body match candidate.
Pairing any of these requires identifying the REAL class via the oracle +
superclass chain — NOT a bare-map flip. DO NOT batch these blind.

## Verdict & sequencing (HARD)

REAL_ACTIONABLE but **dependent**. The coordinator MUST land the reconcile prereq
(9fb9016, +196) on main FIRST. Then each pairing sub-batch is a self-contained
worktree *branched off the new main* (= prereq landed): map entries + occasional
1-line source fix + objdiff-verify-each + whole-binary A/B. Until the prereq lands,
every item below is gated and must carry the prereq diff to be independently
landable vs main@8314 (the SELF-CONTAINED RULE).

## Discovered frontier (adjacent leads, seed later layers)

- **prereq-land item (BLOCKING, highest EV):** land reconcile-handle-prereq-FINAL
  (9fb9016, measured +196) onto main. This is the gate for the ENTIRE handle wave
  and is itself the single biggest banked delta. kind=header-macro. (This is really
  L5's actionable; re-surfaced here because it blocks everything.)
- **attribution-trap real-class ID:** systematically resolve the ~30 mis-pinned
  Handle bodies to their REAL class via DC3/rb3-Wii oracle + superclass-forward VA
  chain; several (DirLoader=Hmx::Object::Handle 665i, Part 893i, AsyncFileHolmes
  891i) are large *real* Handles whose owning class IS pinned elsewhere — pair +
  possibly relocate the body's pin. kind=attribution/pin. EV: unknown until IDed.
- **TIER-2 single-handler micro-batch:** ConnectionStatusPanel / FxSend / UIListDir
  / UserMgr (1 handler each) — verify handler-bl chain == source, then pair. Likely
  +4..+8 clean. kind=pin-pair.
- **FlowIf / UIGuide source-super fixes:** target forwards to a DIFFERENT super
  than source declares (FlowIf→Object not FlowNode; UIGuide→UIComponent not
  Object). 1-line `HANDLE_SUPERCLASS` correction then flip. Verify against rb3-Wii
  (DC3 may be a false friend on these). kind=source-fix+pair.
- **timer-on completeness:** the reconcile set restores 6 TUs; a `bl`-into-static-
  guard + frame>0xb0 scan of all pinned Handle bodies would confirm no other
  timer-on TU silently loses funclets under the global head-gate. kind=tooling/census.
