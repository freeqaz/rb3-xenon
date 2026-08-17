# Lane W25-UI — the UILabel / UIListWidget cluster, scoped and priced

**Date:** 2026-08-17 · **Base:** `cc38cc43` (worktree `~/tmp/wt-w25ui`, branch `w25-ui`)
**Ruler:** `functionRelocDiffs=name_check`, resolved at runtime from
`report.json`'s own `provenance.diff_config` — never hardcoded.

Baseline **re-measured, not inherited** (and it reproduced the brief exactly):
44,503 fns · 3,756,568 B · 36.398514% · honest 21,593 · `total_code` 10,320,664 ·
`total_functions` 69,226.

---

## 1. The cluster is 35 units, and half its gap has no residual to work

`tools/w25_scope.py` (self-validating: it refuses unless the row sums reproduce
`total_functions`, `total_code`, `matched_functions` and `matched_code` exactly —
they do, 4/4).

| | rows | bytes |
|---|---:|---:|
| all `UI*` units (35) | 2,452 | 265,212 |
| matched (`fuzzy == 100`) | — | 177,480 |
| **gap** | 637 sub-100 | **87,732** |

Partition of the gap:

| class | rows | bytes | share |
|---|---:|---:|---:|
| `fuzzy == 0` — **unpaired, nothing to work** | 309 | 43,808 | **49.9%** |
| `fuzzy > 0` — a real residual exists | 328 | 43,924 | 50.1% |
| └ named **and** `fuzzy > 0` (the workable stratum) | 127 | 35,516 | |
| └ anonymous `fn_`/`lbl_` with a residual | 201 | 8,408 | |

⚠ **Instrument correction, made before use.** My first partition used
`fuzzy == mpn` as a "clean instruction-level residual" certificate and it
labelled **319 rows PURE — of which 309 were `fuzzy == 0`**, where `0 == 0`
makes the certificate trivially true. That is the *unpaired* class wearing a
clean-residual label. The certificate is only meaningful on `fuzzy > 0`, where
it holds for just **13 rows / 596 B** in the entire 35-unit cluster.

## 2. Collectability: 91% of the workable stratum is name-charged

`tools/w25_charge_census.py`, W19's discriminator (only a **bare** `arg:{Symbol}`
is a real relocation-name charge; a `diff_arg` that also moves a Register is
charged *by the register*). Coverage is reported and was **127/127 rows, 0
dropped** — a silently-dropped row would have understated the wall.

| verdict | rows | bytes |
|---|---:|---:|
| **COLLECTABLE by source work alone** | 17 | **3,208** |
| needs a name/map adjudication too | 110 | **32,308 (91.0%)** |

Every one of the top 10 rows by size has **`hard = 0`** — a byte-perfect body
whose entire residual is 1–3 relocation-name charges. `tools/w25_pair_dump.py`
sizes this precisely: **88 rows / 23,748 B are gated ONLY by name pairs**, and
the pairs are textbook ICF folds — `ObjPtrList<CharInterest>` vs
`ObjPtrList<Object>`, `RndTransformable::ctor` vs `UIComponent::ctor`, and wholly
unrelated bodies like `StlNodeAlloc<_List_node<int>>::ctor` vs
`Poll@WaitingUserGate`.

This independently reproduces MPNGAP-1's ~91% figure on a cluster it never
examined. **The UI cluster's dominant wall is relocation-name/fold noise, not
unwritten code.**

## 3. The one big row that WAS collectable — and the trap on the way

`?Handle@BandUI@@` (3,564 B, the largest row in the cluster): 891 instructions,
**zero** instruction mismatches, 3 bare name charges. Fixed, **+3,564 B measured,
predicted exactly** (`e54116d2`).

**The route that looked right and was wrong.** `tools/dispatch_fold_enum.py` shows
retail's `Handle@BandUI` @`0x82539210` calling ONE address, `0x825390E0`, from
FIVE arms whose message classes are read from retail bytes via the COL at
`vtable[-1]`: `UIComponentFocusChangeMsg`, `ButtonDownMsg`, `ButtonUpMsg`,
`UIComponentSelectMsg` (the map-resident survivor), `UIComponentSelectDoneMsg`.
One address cannot be five distinct handlers — so I began adding the three
missing spellings to the alias group, which would have paid the same +3,564 B.

⛔ **That inference does not follow.** "Five roles resolve to one address" is
equally consistent with retail having ONE handler and *our* source wrongly
carrying five. `tools/w25_fold_proof.py` settled it against the alias: our three
bodies were **64 B / 4 relocations** (calling `?Print@HAQManager@@` plus
`__savegprlr_29`/`__restgprlr_29` for the frame that call forces) against the
survivor's **52 B / 1 relocation**. **Different-size COMDATs cannot fold.**

The real defect: **retail strips `HAQManager::Print`**, and this file already
recorded that strip for two sibling handlers — a prior pass found the pattern and
missed three sites. Removing them makes all five COMDATs 52 B / 1 relocation,
byte- and relocation-identical, which *is* `/OPT:ICF`'s own condition. Only then
is the fold real, and the alias forgives a naming artifact instead of hiding a
behavioural divergence.

★ **The `none` control could not have caught the bad version.** `none` ignores
relocation names and reads +0 whether the alias is real or fabricated — its
flatness is the hazard's signature, not a clearance. What caught it was a
**relocation-set** comparison on our own COMDATs. Note it had to be the
relocation set and not the size: STLPORT-1 showed a pure size test can be a
one-sided reader artifact that cancels on both legs of a two-sided control.

**Force-multiplier check: drained.** After this fix there are **zero**
`HAQManager::` call sites anywhere in `src/` outside `HAQManager.cpp` itself.
No further yield in this pattern.

## 4. What is left, ranked — for the next lane

The remaining COLLECTABLE stratum is **17 rows / 3,208 B**, of which **828 B is
register-only** (`hard = 0`, no name charge: `SyncDir@UIProxy` 324,
`SetSelectedSimulateScroll@UIListState` 292, `ScrollToTarget@UIListState` 128,
`SubList@UIListSubList` 84) — **permuter-class, and permuter is OFF by standing
directive.** So the genuinely source-workable remainder is ≈ **2,380 B**:

| size | fuzzy | charges | symbol |
|---:|---:|---|---|
| 652 | 84.19 | hard 27, reg 9, br 7 | `?Scroll@UIListState@@` |
| 500 | 82.30 | hard 25, reg 6, br 1 | `?SetTypeDef@UIComponent@@` |
| 428 | 51.06 | hard 63, reg 21, br 6 | `?PostLoad@PanelDir@@` |
| 144 | 61.22 | hard 15, reg 5 | `??0UIListArrow@@` |
| 128 | 84.16 | hard 5, imm 2, br 1 | `?CurrentTransitionEvent@UIEventMgr@@` |
| 112 | 88.57 | hard 4 | `?_M_erase@vector<UILabel::LabelStyle>` |
| 112 | 37.46 | hard 19, reg 4 | `?Copy@UISlider@@` |
| 96 | 50.58 | hard 12, reg 5 | `_M_allocate_and_copy<FlowMathOp>` |
| 80 | 83.45 | hard 3, reg 4 | `?Terminate@UIEventMgr@@` |
| 76 | 68.37 | hard 6, imm 1 | `?NewObject@UIPanel@@` |

⚠ **Price these from the ASM EXTENT, not from `report.json`'s size** — one lane
was billed 8,852 B for a 12-byte `return true`.

## 5. What this lane did NOT do, and why

- **Did not touch the 88 name-charged rows / 23,748 B.** Each needs the same
  per-pair adjudication `Handle@BandUI` got, and the one I *did* adjudicate came
  back **refuting** the alias. Bulk-installing them on name similarity is exactly
  the integrity hazard: `TEMPLATE_ARGS_DIFFER` is what a *proven* fold looks
  like, so name similarity is the least informative feature available.
- **Did not work the 309 `fuzzy == 0` rows (43,808 B, half the gap).** They are
  unpaired — an identification problem, not a source problem — and 277 of them
  are anonymous, where naming has zero byte upside because placeholder targets
  are already forgiven.
- **Did not open the register-only rows** (828 B): permuter is OFF.
- **Did not re-verify the 5 pre-existing CONTRADICTED `_Rb_tree` alias groups.**
  They fail `icf_alias_finder.py --validate` at baseline, before and after this
  lane, and are out of scope — but they mean that gate reads FAIL rather than
  PASS, so measure its baseline before attributing a failure to your own edit.
- **Did not trust any `UILabel.h` header claim.** W16-HEADERTRUTH found its cited
  vtable `0x8211AEB4` is +0xc inside a 2-entry EH IP-to-state map and its cited
  "function `0x827CCDF0`" is not a function. That correction lives on the
  unlanded `w16-headertruth` branch, so **this worktree still carries the false
  citation**; nothing here rests on it.
