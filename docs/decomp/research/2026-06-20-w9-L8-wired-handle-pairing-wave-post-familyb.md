# W9 L8 — wired-handle-pairing-wave-post-familyb (ADVERSARIAL DISCOVER/PLANNER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched)
**Verdict: REAL_ACTIONABLE** — the lever is real and EMPIRICALLY RE-PROVEN this
pass, but it is **NOT independently landable vs main@8314** (it is the THIRD step
in a dependency chain), and my own cross-check of the L4/L6 "clean tier" found it
is **smaller and noisier than claimed** (one confirmed attribution trap inside the
"clean" list, three more that need per-fn verify). Cold-executable items below.
**Mode:** read-only in main; ground truth from the prereq worktree
(`wt-w9-reconcile-handle-prereq-FINAL` @9fb9016, **measured 8510 = +196**,
`run_objdiff` 100% on paired Handles), per-unit dtk-split target asm bl-chains,
source `BEGIN_HANDLERS` blocks (rb3-xenon + rb3-Wii), and `splits.txt`/`objects.json`.

## TL;DR — what's RIGHT / WRONG

- **RIGHT (mechanism, re-proven):** after the Family-B macro prereq, a wired+pinned
  Handle body that is a pure superclass-forward becomes byte-exact; a bare
  `target_symbol_map.json` `0xVA -> ?Handle@Class@@UAA?AVDataNode@@PAVDataArray@@_N@Z`
  entry flips it to 100% with NO source edit. **Re-confirmed this pass via
  `run_objdiff` (project_dir = prereq worktree):** `?Handle@CharIKFoot@@…` = **100.0%
  normalized** (48 instr all equal); `?Handle@WorldInstance@@…` = **100.0%** (48 instr).
  A wrong VA->name pair reads 0% (self-validating, never a false +).
- **WRONG #1 — DEPENDENCY (the killer, unchanged from L5/L6/L7):** the +196 prereq
  is **NOT on main**. main@812e1df still emits the timer head + sizeof-stripped END
  tail. The prereq exists ONLY as branch `w9-reconcile-handle-prereq-FINAL` @9fb9016
  and its single-commit land candidate **`w9-land-reconcile-handle-prereq-9fb9016`
  @a7175af** (a7175af = ONE commit on top of main@812e1df, "+196 @100%"). **No
  pairing item in this wave is independently landable vs main@8314** — each must
  branch off the prereq tip OR carry the whole a7175af diff in-worktree (the
  SELF-CONTAINED RULE). The two pre-created batch worktrees
  (`wt-w9-handle-pair-batch-small-tier1-char-clean`,
  `…mid-tier-per-fn-verify`) are sitting at bare main@812e1df — UNUSABLE until
  rebased onto a7175af.
- **WRONG #2 — the "clean tier" is smaller AND has a trap inside it.** I
  cross-checked every L6 TIER-1 candidate by diffing its SOURCE `HANDLE_SUPERCLASS`
  list against its TARGET superclass-forward bl-chain (resolved super VAs below).
  Result: only **7 are truly clean** (src-super == tgt-fwd exactly); **1 is a
  CONFIRMED attribution trap that L6 mislabeled clean** (CharEyeDartRuleset); **3
  need per-fn body verify** (CharBonesBlender ICF-fold, CharCollide own-handlers,
  DepthBuffer3D). Honest clean-tier EV ≈ **+7..+8**, not +13.
- **WRONG #3 — the census super-forward field is unreliable for the "small Flow /
  small UIGuide" rows.** The census's `UIGuide fn_828020D0` (48 instr) forwards to
  UIComponent::Handle but UIGuide.cpp has only ONE `BEGIN_HANDLERS(UIGuide)` whose
  real body is the **93-instr fn_82803500** (own actions + UIComponent forward).
  fn_828020D0 is a DIFFERENT UI class merged into UIGuide.s. Likewise the census
  `Flow fn_8229D0E0` (48) forwards via fn_82431B18 (a TexRenderer-range VA) — not
  the real Flow::Handle (the 80-instr fn_8229BA18, in auto_03_8227EF2C_text.s).
  These census rows are mis-attributed; do NOT pair them blind by owning-.s name.

## Ground truth established this pass

### Mechanism re-proof (prereq worktree, run_objdiff)
- `?Handle@CharIKFoot@@UAA?AVDataNode@@PAVDataArray@@_N@Z` = **100.0% normalized**.
- `?Handle@WorldInstance@@UAA?AVDataNode@@PAVDataArray@@_N@Z` = **100.0% normalized**.
- Prereq worktree report.json `measures.matched_functions` = **8510** (+196).
- 12 `?Handle@…` entries already in the prereq map (UIComponent, UIManager×2,
  UIListSlot, UIListCustom, WorldInstance, CharIKFoot, ModalKeyListener,
  QuestFilterPanel, DancerSequence×2, GuitarController). None of the L8 batch VAs.

### Superclass-forward VA resolution (used to classify clean vs trap)
| VA | super Handle |
|---|---|
| fn_827371D8 | Hmx::Object::Handle |
| fn_827D9928 | UIComponent::Handle |
| fn_823E7E40 | RndTransformable::Handle |
| fn_823F47C0 | RndDrawable::Handle |
| fn_8240E828 | RndPollable::Handle |
| fn_825BE6A8 | CharWeightable::Handle |
| fn_827EFE58 | UIListSlot::Handle |
| fn_82725EE8 | DataNode::Sym (Sym(1) head — in EVERY body, NOT a super) |
| fn_82732F68 | PathName (END tail side effect — in EVERY tail) |
| fn_82725930 / fn_82260570 | DataNode helpers (NOT supers) |
| fn_8279B788 | Symbol::Symbol(const char*) (HANDLE-action / HANDLE_CHECK string — NOT a super) |

### Source-super (rb3-xenon) vs target-forward cross-check (the decisive table)
| unit | fn | tgt-fwd chain | src HANDLE_SUPERCLASS | verdict |
|---|---|---|---|---|
| CharFaceServo | fn_823909C0 | Hmx::Object | Hmx::Object | **CLEAN** |
| CharSleeve | fn_823BD8A8 | Hmx::Object | Hmx::Object | **CLEAN** |
| CharPosConstraint | fn_823B04E8 | Hmx::Object | Hmx::Object | **CLEAN** |
| CharIKHead | fn_823ADF30 | CharWeightable,Hmx::Object | CharWeightable,Hmx::Object | **CLEAN** |
| CharIKFingers | fn_8239FC68 | CharWeightable,Hmx::Object | CharWeightable,Hmx::Object | **CLEAN** |
| BandIKEffector | fn_822B1880 | CharWeightable,Hmx::Object | CharWeightable,Hmx::Object(+HANDLE_CHECK tail) | **CLEAN** |
| Waypoint | fn_823C8630 | RndTransformable,Hmx::Object | RndTransformable,Hmx::Object (src/system/char/Waypoint.cpp) | **CLEAN** |
| CharBonesBlender | fn_823C6458 | RndPollable,Hmx::Object | CharPollable,CharBonesAlloc | ICF-fold? (CharPollable::Handle→RndPollable, CharBonesAlloc::Handle→Hmx::Object are pure-forwards that ICF-fold; LIKELY flips — objdiff-verify) |
| CharCollide | fn_822B7860 | (via fn_823F2538, own handlers) | RndTransformable,Hmx::Object | PER-FN (own HANDLE action + non-super forward; not pure-forward) |
| DepthBuffer3D | fn_826DC060 | (no super-bl detected) | (none found this pass) | PER-FN (verify body shape) |
| CharEyeDartRuleset | fn_823ABFD8 | RndTransformable,Hmx::Object | Hmx::Object ONLY | **ATTRIBUTION TRAP — DROP** (class extends Hmx::Object only; a body forwarding via RndTransformable is a DIFFERENT class merged into CharEyeDartRuleset.s) |

All 7 CLEAN owner TUs are wired (`objects.json` NonMatching) and pinned in
`splits.txt` with a `.text` span covering the candidate VA:
CharFaceServo [0x8238FCE0,0x82391298), CharSleeve [0x823BD6D8,0x823BE0E8),
CharPosConstraint [0x823B0318,0x823B0D38), CharIKHead [0x823ADCF0,0x823AF410),
CharIKFingers [0x8239E5A0,0x823A1230), BandIKEffector [0x822B0C60,0x822B3D28),
Waypoint [0x823C7CC8,0x823CA668). **No splits.txt edits needed** — map entries only.

### Source-divergent smalls (the L4 "1-line super fix FIRST" set) — re-examined
- **FlowIf fn_823B4E30** (in FlowIf.s, pinned): tgt forwards to **fn_827371D8 =
  Hmx::Object::Handle**, but src = `HANDLE_SUPERCLASS(FlowNode)`. FlowNode has its
  OWN `BEGIN_HANDLERS(FlowNode)` + `virtual DataNode Handle` (FlowNode.h:55), so
  `HANDLE_SUPERCLASS(FlowNode)` emits a `bl FlowNode::Handle`, NOT `bl
  Hmx::Object::Handle`. Either retail folded FlowNode::Handle onto Hmx::Object::Handle
  via ICF (if FlowNode::Handle is itself a pure Hmx::Object forward) OR retail's
  FlowIf forwards straight to Hmx::Object. 1-line fix `HANDLE_SUPERCLASS(Hmx::Object)`
  then objdiff. **rb3-Wii has no FlowIf.cpp grep hit** → DC3 may be the only oracle;
  verify the fix doesn't break FlowNode::Handle's own pairing.
- **UIGuide**: the REAL UIGuide::Handle is **fn_82803500 (93 instr, own actions +
  UIComponent forward)**, NOT the census's fn_828020D0 (48). Src
  `HANDLE_SUPERCLASS(Hmx::Object)` (rb3-Wii agrees) but tgt forwards to
  UIComponent::Handle → genuine src-vs-retail divergence; fix to
  `HANDLE_SUPERCLASS(UIComponent)`, but it is NOT a pure-forward (has own handlers)
  so it is a PER-FN verify, not a clean flip.
- **Flow fn_8229D0E0** (census 48): forwards via fn_82431B18 (TexRenderer range) →
  mis-attributed; real Flow::Handle is the 80-instr fn_8229BA18 with own actions.

## Verdict & sequencing (HARD)

REAL_ACTIONABLE but **dependent**. Coordinator MUST land
`w9-land-reconcile-handle-prereq-9fb9016` @a7175af (+196) on main FIRST. Then the
clean-tier batch below is a self-contained worktree *branched off the new main*:
7 map entries + objdiff-verify-each + whole-binary A/B. The ICF/per-fn items follow
once their bodies are confirmed byte-exact. Until the prereq lands, every item is
gated; to be independently landable vs main@8314 an item must carry the a7175af diff.

## Discovered frontier (adjacent leads, seed later layers)

- **TIER-2 single-handler micro-batch** (carried, refined): ConnectionStatusPanel
  fn_82795BA0, FxSend fn_826F8968, UIListDir fn_827E6A10, UserMgr fn_8250FE28 — one
  HANDLE each; verify the handler-bl chain (count of static-Symbol compares + bl
  targets) == source BEFORE the map entry. +4..+8. kind=pin-pair, attribution_risk.
- **attribution-trap real-class ID sweep** (~30 census mis-pins): resolve each
  mis-pinned Handle body (CharEyeDartRuleset's fn_823ABFD8, UIGuide's fn_828020D0,
  Flow's fn_8229D0E0, the PropKeys 2nd-range ones, DirLoader=Hmx::Object::Handle
  665i, Part 893i, AsyncFileHolmes 891i) to its REAL class via the
  super-forward-VA chain + DC3/rb3-Wii oracle; several are LARGE real Handles whose
  owning class is pinned elsewhere (pair + possibly relocate the body's pin). EV
  unknown until IDed. kind=attribution/pin, attribution_risk.
- **FlowIf / UIGuide-real source-super fixes** (per-fn, post-prereq): FlowIf→Object
  (ICF question) and UIGuide-real(fn_82803500)→UIComponent. Each is a 1-line
  HANDLE_SUPERCLASS fix + verify it doesn't regress the super's own Handle. Verify
  against the oracle (FlowIf may be DC3-only). kind=source-fix+pair.
- **map-coverage reveal-audit TOOL** (carried from L6/L7): for any wired+pinned
  unit, byte-diff every unmapped target `fn_` against same-size own methods on the
  LANDED prereq base, emit byte-exact ones as a reveal worklist. This is the
  legitimate auto-harvest of the whole census (and the residual reveals across the
  W9 port-then-pin lands) — but only finds reveals on the base where the prereq is
  present, never on main. kind=tooling.
- **map re-serialization conflict risk** (carried): the SongStatusMgr branch
  rewrites ~5700 lines of `target_symbol_map.json`; concurrent map-touching lands
  (any reveal batch) conflict on the serialized form. Land map-touching units
  serially or adopt a stable-key-order serializer. kind=tooling.
