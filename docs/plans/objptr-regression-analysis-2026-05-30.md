# ObjPtr migration — post-landing regression analysis + next-work plan

**Context:** the ObjRef/ObjPtr family re-layout landed on main @ `9e25ac2` (P2+P3+P5
+ landing fixes), net **2006 → 2060 (+54)** on the full unit set. Four units showed a
*net* matched-count drop (−11 total); per-unit subagent diagnosis (2026-05-30) classified
every one. **None is real degradation** — they are MSVC frame-pointer-anchor codegen
artifacts of the now-polymorphic ObjPtr, semantically-equal code one instruction below the
exact-100% line. This doc records the findings + the concrete reclaim plan.

## The regressed units (all diagnosed)

| unit | net | functions | cause |
|---|---|---|---|
| **Gen** | −7 | 7 fns 100→99.8/99.9 (+8 more went 0→99.8) | FP-anchor slip (Pattern A: coupled `subi`/`lwz` offset; Pattern B: + ICF dtor-label mismatch) |
| **Group** | −2 | 7 fns 100→99.83 (−7), but +5 gains incl. `RndGroup::Merge` 100% | FP-anchor slip; the 7 regressions + 4 gains are two sides of ONE K-constant |
| **TexRenderer** | −1 | `fn_82431210` (ctor EH thunk) 100→99.82 | FP-anchor slip **+ one real diff**: polymorphic ObjPtr virtual dtor changes the EH cleanup path (`bl fn_82263570` vtable-thunk → `bl ~RndAnimatable`) |
| **UITransitionHandler** | −1 | `fn_827DC860` (SetOutAnim) 100→99.82 | FP-anchor slip (sibling SetInAnim@0x4 stays 100%; SetOutAnim@0x10 slips) |

**Repo-wide:** a function-level scan found **19** functions total at this FP-anchor limit
(Gen 10, Group 7, CharBoneOffset 1, LightPreset 1, TexRenderer 1, UITransitionHandler 1 — the
last two units' NET went negative; the others absorbed it). All are ≥99.82%, semantically equal.

## The FP-anchor slip (root cause, confirmed)

The polymorphic `ObjPtr`/`ObjOwnerPtr` (vtable + EH-unwind reservation) shifts MSVC's
frame-pointer-establisher constant. Signature (uniform):
```
[0] subi r31, r12, K        target K vs base K differ by exactly ±0x10
[4] lwz/stw  off(r31)        off shifts by the SAME ±0x10  →  K+off identical both sides
    stwu r1, -0x60, r1       frame size IDENTICAL; saved-reg count IDENTICAL
    addi r3, r11, OFF        member offsets IDENTICAL
```
The effective address (`r31 ± K ± off`) is bit-identical → the code is **semantically equal**.
It is **source-immune** (no `.cpp` edit changes K without re-breaking a paired function) and
**permuter-immune** (no regalloc/extract knob touches the FP-establisher constant). This is the
documented cost of P2's polymorphism that bought the +54.

## Next-work plan (priority order)

### 1. objdiff-fork FP-anchor normalization — THE lever (high value, repo-wide)
Two normalization rules in the freeqaz `../objdiff` fork (see `[[project-objdiff-fork]]`):
- **(a) FP-anchor pair rule:** when `(subi r31,r12,K_tgt)` + the paired `(lwz/stw off_tgt(r31))`
  differ from base but `K_tgt + off_tgt == K_base + off_base` AND frame size + saved-reg counts
  match → score both instructions equal.
- **(b) ICF label resolution:** when target `bl lbl_XXXXXXXX` and base `bl ??mangled@@` resolve
  to the same function body (via the dtk symbol table), score equal. (`lbl_822605C0` = ICF-merged
  `ObjPtr<T>::~ObjPtr`; `fn_82260570` = the ref-decrement/release body.)

Payoff: reclaims the **19 regressed** functions AND the **~8–10 newly-near-100% gains**
(Gen's 8 fns that went 0→99.8) — a net swing well beyond the −11, repo-wide (not just ObjPtr
units; any polymorphic-base dtor thunk benefits). This is the single highest-leverage follow-up.

### 2. Group "base class +0x10" lead — possible real source fix (investigate)
Group's diagnosis: the 7 regressions (12-insn vbase dtor thunks, want K=0x70) and 4 gains
(11-insn direct thunks, want K=0x80) are mathematically constrained by ONE layout constant.
Hypothesis: one base in `{RndAnimatable, RndDrawable, RndTransformable}` is **+0x10 over retail**
in our post-migration source. If true, correcting it converges all 12 Group functions at once
(and likely helps Gen/TexRenderer/UITransitionHandler which share these bases). Action: Ghidra
struct-layout cross-check of the three bases vs the RB3 binary. NOTE: this is a *base-class layout*
investigation, adjacent to but distinct from the ObjPtr migration.

### 3. TexRenderer EH-path diff — patcher territory (low priority)
`fn_82431210`'s real (non-FP-anchor) diff is the polymorphic-dtor EH cleanup path. Not
source-fixable without reverting P2 polymorphism; recoverable only via a post-compile
transplant/bl-patcher (the dormant `scripts/` patchers). Low priority, 44 bytes.

### 4. Cosmetic: stale header comment
`src/system/ui/UITransitionHandler.h:44` `ObjPtr<RndAnimatable> mOutAnim; // 0x18` → `// 0x10`
(ObjPtr is 0xc now; compiled offset already 0x10 — comment only).

## References
- Raw per-function diffs: `/tmp/regress-{gen,group,texrenderer,uitransition}/`, `/tmp/objptr-slip-diag/SUMMARY.txt`.
- Migration design + confirmed architecture: `docs/plans/objptr-family-relayout-migration.md` §12.
- Memories: `[[project-objptr-relayout-migration]]`, `[[project-objdiff-fork]]`,
  `[[feedback-fuzzy-gap-needs-permuter]]` (note: these are NOT permuter-class — they're
  objdiff-normalization-class), `[[project-engine-baseclass-layout-wall]]`.
