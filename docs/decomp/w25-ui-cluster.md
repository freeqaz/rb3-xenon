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

---

# Lane W28-UISRC — working the §4 remainder

**Date:** 2026-08-17 · **Base:** `0ba670bc` (worktree `~/tmp/wt-w28uisrc`, branch
`w28-uisrc`) · **Ruler:** `name_check`, resolved from `report.json` provenance.

Baseline **re-measured, not inherited**: 44,505 fns · 3,760,224 B · 36.433937% ·
honest 21,595 · `total_code` 10,320,664 · `total_functions` 69,226. (+2 fns /
+3,656 B ahead of W25's baseline — W25's own landing plus later work.)

★ **§4's ranking REPRODUCED EXACTLY** by re-running `w25_charge_census.py`:
17 rows / 3,208 B COLLECTABLE, same rows, same sizes, 126/126 profiled, 0
dropped. The ranking is sound. Two rows W25's top-10 table cut off are charged
by **immediates only**, not registers — so they are workable, unlike the 828 B
register-only class.

## Collected: +128 B, predicted exactly

`?CurrentTransitionEvent@UIEventMgr@@` 84.16 → **100.0%** (32/32 equal).
A/B: Δmatched **+1**, Δhonest **+1**, Δcode_bytes **+128**, unit
`default/UIEventMgr` 35→36. Native gate `verdict=PASS … skipped=0 rc=0`.

Retail constructs the returned `Symbol` into a **stack temporary per branch**
and copies one word into the return slot; our `if/else` returned each branch
expression directly, so MSVC applied RVO. `return event ? event->Type() :
Symbol(gNullStr);` reproduces retail exactly.
⛔ **Both oracles are WRONG here** — rb3-Wii and our tree carry the identical
`if/else`; the ternary is in neither. Retail bytes were the only guide.
⚠ The natural "named local" reading (`Symbol s; … return s;`) scored **84.2 →
65.4**: it produced the right copy-to-return-slot *and* frame, but
`Symbol s;` default-constructs from `gNullStr` and MSVC **hoists that to the top**.
The shape must come from temporaries, not a default-constructed local.

## ⛔ The headline correction: "COLLECTABLE" ≠ "workable"

`COLLECTABLE` means only *"no relocation-name charge blocks this row."* It does
**not** mean the blocker is a source body. Adjudicated on retail bytes, at least
three of the remaining rows are **layout or map-identification** problems that no
body edit can reach:

| row | evidence | real class |
|---|---|---|
| `?FocusComponent@UIPanel@@` (40 B) | source is IDENTICAL to rb3-Wii; retail loads field `0x2c` + vtable slot `0x6c`, we load `0x8` (`mDir`, compiler-VERIFIED) + slot `0x34`. **Two independent** differences on a maximally generic body | wrong map name / layout |
| `?NewObject@UIPanel@@` (76 B) | retail `li r3, 0x108` (264 B alloc) vs our `li r3, 0x68` (= our `sizeof(UIPanel)`); retail calls unnamed `fn_8268B4E8` and **omits the vbase `this` adjustment** we emit | layout / identification |
| `??0?$reverse_iterator@PAH@…` (8 B) | whole body is `stw r4, 0x18(r3); blr` vs our `stw r4, 0x0(r3)` | ICF fold artifact |

⇒ **Price the §4 remainder well below 2,380 B.** Budget against rows whose
charges name a *source construct*, not merely rows with no name charge.

## ⛔ `?Scroll@UIListState@@` (652 B) — 84.19 → 86.0, NOT crossed, REVERTED

Worth reading before anyone reopens it; four defects were positively identified.

Our body is a near-verbatim **DC3** copy; **rb3-Wii's is structurally different**
(goto-into-loop + a `State2Data` helper). DC3 is *newer* than RB3, so for
pure-logic engine code rb3-Wii is the truer era oracle — and it fixed real things:

- retail's two unconditional forward `b`s (`b 0x6c`, `b 0x284`) = goto-into-loop;
- `int hitBoundary` with explicit `0/1` inside `if`s kills the bool
  materialization (our `hitBoundary = (x == y)` emitted `subf`/`cntlzw`/`extrwi`
  where retail just does `cmpw`) — **this is the BOOL_MASK the detector flags, and
  its cause was a source construct, not the permuter**;
- but `changed` must stay **`bool`** (DC3's spelling): rb3-Wii's `int changed`
  forces a `clrlwi` zero-extend where retail does `mr` ⇒ **the answer was a
  HYBRID of the two oracles, neither one verbatim**;
- comparison order `mFirstShowing != mTargetShowing` (DC3's) matches retail's
  `cmpw` operand order.

Verbatim rb3-Wii alone scored **84.0** (worse than baseline) — oracle mode 3.
Remaining wall at 86.0%: a **16-byte frame deficit** + a 12-instruction register
cascade + 1 bool mask. Retail caches `state.mSelectedDisplay` in a callee-saved
register (`mr r11,r29`) where we reload it (`lwz r11,0x54(r1)`) — an extra local.
Adding `curFirst`/`curSel` **does close the frame gap** (the `stwu` mismatch
disappears), but hand-inlining `State2Data` alongside it regressed to **80.8**
(retail tests `mCircular` where we then loaded `mProvider`), and confining the
locals to the boundary block leaves them **dead-store-eliminated and inert**.

**Reverted**: 86.0% pays **0 bytes** (`matched_code` is all-or-nothing) and the
row's residual is permuter-class, which is OFF.
⚠ Correction to a fear I raised and then disproved: rb3-Wii's `State2Data` using
`SelectedDisplay()` is **exactly equivalent** to DC3's `sel = mMinDisplay`,
because our `SelectedDisplay()` *is* `if (mCircular) return mMinDisplay;`. There
is **no** semantic divergence between the two spellings.

## ⛔ `?SetTypeDef@UIComponent@@` (500 B) — not attempted past diagnosis

rb3-Wii guards the tail with `if (TypeDef() != da)`. **Retail does NOT** —
instructions 127–136 show `bl SetTypeDef@Object@Hmx` then `bl UpdateResource`
with no preceding compare, on both sides. Our unguarded version is already right;
adding the oracle's guard would have been a regression. **Checking retail bytes
first is what prevented it.**
Real residual is region 97–126 (30 instructions, 0%): the same `ClassName()` /
`Name()` / `PathName(Dir())` sequences inside the `MILO_WARN`/`MILO_FAIL` branches
in a **different evaluation order**, with retail sharing a tail via `b` where we
duplicate `mtctr`/`bctrl`.

## ⛔ Wide-ripple, deliberately untouched

`_M_erase@vector<UILabel::LabelStyle>` (112 B, 88.57): retail zero-initializes a
`random_access_iterator_tag` stack temp and passes **one fewer argument**; we
pass an extra `Distance*` (`PAH`). That is an **STLport `__copy` signature**
difference in shared headers used by every `vector` in the binary — a
force-multiplier or a disaster, and not A/B-able within one lane's budget.

## Not attempted

`PostLoad@PanelDir` (428 B), `??0UIListArrow@@` (144 B), `Copy@UISlider` (112 B),
`_M_allocate_and_copy<FlowMathOp>` (96 B), `Terminate@UIEventMgr` (80 B — all 7
mismatches attribute to REGISTER_SWAP + address-relocation noise), and
`ReadMetaEvent@MidiReader` (972 B, the queued extra). The 828 B register-only
class and the 88 name-pair rows remain out of scope per W25.
