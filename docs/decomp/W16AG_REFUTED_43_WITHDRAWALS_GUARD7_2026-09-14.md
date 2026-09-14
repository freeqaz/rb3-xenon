# W16-AG — landing W16-AD's 43 refuted alias withdrawals, and adjudicating its guard

**Lane:** W16-AG (opus) · **Branch:** `w16-ag` · **Date:** 2026-09-14
**Base:** main `0abb5e91` · **Predecessor:** `docs/decomp/W16AD_UNDECIDED_MASKED_RETAIL_FOLD_WITNESS_643_2026-09-14.md`
**Artifacts:** `W16AG_fold_witness_2026-09-14.json` (my own 643-row sweep),
`W16AG_guard_adjudication_2026-09-14.json`, `W16AG_withdrawal_prediction_2026-09-14.json`,
`W16AG_selection_51_2026-09-14.json`, `W16AG_batch{A,B,C,D}_applied.json`

---

## Headline

**51 alias memberships withdrawn — all 43 W16-AD refuted but did not land, plus
all 8 its own twin guard blocked.** Δ`matched_functions` **0**, Δ`matched_code`
**0**, pre-registered as 0 on every batch. Groups 1,634 before and after;
`folded` memberships 5,294 → 5,243; **nothing pruned** — every removal carries a
`withdrawn` record naming the two retail addresses that refuted the fold.

Three corrections to the predecessor, each measured rather than argued:

| | W16-AD | W16-AG measured |
|---|---|---|
| guard-blocked population | 7 rows / 492 B | **8 memberships / 572 B** |
| strict ceiling | 70 / 4,320 B | **71 / 4,400 B** |
| genuine CD-7 twins among them | 0 (raw test) | **0** (correct test) |
| its withdrawal **prediction** | "0 bytes" | **vacuous by construction** |

The bytes are zero and that is the honest result. What the lane buys is that the
alias file no longer asserts 51 folds retail bytes say did not happen.

---

## 1. Baseline — reproduced before anything was touched

`tools/rowset_snapshot.py save` in the worktree, against main's
`~/tmp/rows_w16ac_main.json`:

```
base : {"matched_functions": 43319, "matched_code": 3990660, "matched_code_percent": 38.948635, "fuzzy_match_percent": 49.524593}
main : {"matched_functions": 43319, "matched_code": 3990660, "matched_code_percent": 38.948635, "fuzzy_match_percent": 49.524593}
MEASURES EQUAL: True
rowset EQUAL: True  |only-base 0 only-main 0
```

Not just the four headline measures — the **entire 40,562-row `fuzzy==100` set**
is identical, so nothing downstream rests on a baseline that merely *totals* the
same.

## 2. The verdicts are mine, not inherited

The brief required re-running the witness because W16-AD's selftest
hand-reproduced only 2/6. The **whole 643-row sweep** was re-run on this tree
(`tools/w16ad_fold_witness.py --sweep`), not just the 43:

| verdict | rows | bytes |
|---|---:|---:|
| `NO_WITNESS_FOLDED_SIDE` | 496 | 30,864 |
| **`WITNESS_REFUTED`** | **63** | **3,828** |
| `WITNESS_INCONCLUSIVE` | 52 | 3,512 |
| `WITNESS_CONFIRMED` | 23 | 1,944 |
| `CHANNEL_B` | 4 | 696 |
| `WITNESS_PARTIAL` | 4 | 380 |
| `ALREADY_NAMED` | 1 | 136 |
| total | 643 | 41,360 |

**0 verdict disagreements over all 643 rows**, and the 43 not-landed rows are
**43/43 `WITNESS_REFUTED`** (2,580 B). The selftest reproduces identically —
`hand_reproduced=2/6 positive_control=PASS negative_control=FIRED`, so the
refutation arm is live and this is not a CONFIRM-only instrument.

That agreement is worth more than a re-read because the tree **moved**: W16-AC
wired `rnddx9/Utl.cpp` and W16-AD's own map swaps landed in between. 20 of 643
rows do differ, all inside `pairs` and **none among the 43** — extra strong
witnesses created by W16-AD's Item 3 swaps naming more callers.

### 2.1 A flagged W16-AD item closed in passing

The one guard-blocked row among those 20 is gi=563, whose `S_map_named` went
`['0x82295fb8'] → []`. W16-AD §7 flagged exactly this as unresolved: *"the map
places `c_S` at `0x82295fb8` (96 B) while an EQ caller's decoded branch puts it
at `0x8246ebe0` (100 B)"*. It is now settled — **its own Item 2 commit
`8fa71844` repaired `0x82295fb8`** (misnamed as the `<Key<vector<Vector2>>>`
instantiation, corrected to the `PropertyFilter` one on the size verdict). The
map was wrong, the byte witness right, and the map no longer contradicts it.

## 3. The guard — 8 memberships, not 7, and 0 true refusals

### 3.1 The population was miscounted by its own summary

§6 lists "7 / 492 B — gi=338, 563, 564, 583, 589, 625, 942". Keyed on
`(survivor, address, folded)` the real population is **8 memberships / 572 B**:
gi=942 carries **two different folded spellings** at one (survivor, address).
492 B is the sum over distinct `gi`; 572 B is the sum over memberships. This is
the same *"`gi` is not a key"* defect W16-AD documented in §4.2, resurfacing
inside its own §6 summary. **Strict ceiling is 71 rows / 4,400 B.**

### 3.2 The criterion that would make the refusal TRUE — stated so it can fail

The guard exists for **CD-7's 51-surplus class**: two retail bodies identical
**including call targets**, which `/OPT:ICF` could have folded and did not. For
such a pair, living at two addresses says nothing about whether retail kept the
parent callees apart, so INCONCLUSIVE is right.

> **TRUE refusal ⇔ same length, and every word equal once branch displacements
> are resolved.** Measured over the 8: **0 meet it, 8 do not.**

Seven differ in an `addis`/`addi` pair materialising a **different absolute
address** (vtable / type pointer) — e.g. gi=564 `0x820178f0` vs `0x82020c54`.
gi=563 differs in a real `bl` callee: `0x82774148` (`~vector<Color>`) vs
`0x827740E0` (`~vector<Vector2>`) — **independently reproducing §6's hand
check**. Different COMDAT bytes ⇒ ICF cannot fold ⇒ the pair discriminates.

All 8 additionally carry the survivor signature — retail's own body at X calls
`c_S`, so X **is** the survivor — which holds **51/51** across this lane's whole
selection.

### 3.3 ⛔ Both obvious strict tests are wrong, and one bit me

`tools/w16ad_strict_twin_audit.py` compares **raw words**. CLAUDE.md records raw
duplicate-body comparison as **silently vacuous**: PC-relative displacements
differ at different addresses, so a genuine twin reads "different" and *every*
row flips to SEPARATE — the test would manufacture the withdrawals it gates.

Resolving branches to **absolute** targets is the mirror-image error, and it bit
this lane's first run: gi=942's flagged *"+0x40 call target"* divergence was
**pc+12 on both sides** — one internal branch reported as a changed callee. Had
a row's only difference been such a branch, that would have been a **false
refutation**, the costly error the guard exists to prevent. Caught by reading
the tool's own evidence, not by a control.

⇒ the token is chosen by **where the branch lands**: inside the extent, compare
offset-from-base; outside, compare absolute destination. Neither raw words nor
absolute targets alone are a correct twin test. The verdicts survived the fix;
the *evidence* did not, and it is the evidence that gets audited.

### 3.4 Anti-vacuity control — the test can return TWIN

A test that can never return the blocking verdict restates its own shape.
`tools/w16ag_twin_control.py` over all **57,733** retail `.pdata` functions
finds **1,529** distinct-address groups identical including call targets and
calls the largest pair **TWIN** — `CONTROL PASS`. *(That scan excludes no
funclets and is **not** a re-measurement of CD-7's 51; it exists only to show
this test can fail.)*

## 4. Prediction — and a vacuity inherited from the predictor

⛔ **`tools/w16ad_predict_withdrawal.py` resolves the group as
`aliases["groups"][gi]`** — the very defect §4.2 fixed in the apply tool and in
`w16s_ablate.py`, left in place in the **predictor**. Measured over its own
input: of the 63 refuted rows, **0 have `gi` equal to the true group index and 0
have their folded spelling present in `groups[gi]`**. Its `members` set is
always an unrelated group's, `nm in members` can never fire, and its *"predicted
loss: 0 bytes"* was produced **by construction**.

This does **not** overturn W16-AD's Δ0 *measurement*, nor its independent
call-site argument. It means the prediction carried no information — and an
all-zero prediction is precisely the shape CLAUDE.md says to distrust.

`tools/w16ag_predict_withdrawal.py` keys on `(survivor, address)` and prices
**both directions** of forgiveness (the old tool walked only call sites of N,
missing sites where we emit another member and retail names N):

```
PREDICTION over 51 memberships
  memberships with >=1 dependent fuzzy==100 row : 0
  distinct dependent rows (predicted to FALL)   : 0
  predicted matched_code loss (bytes)           : 0
  newly-charged sites, direction (a)            : 1
  newly-charged sites, direction (b)            : 0
  caller report-row status                      : {'not_a_report_row': 61, 'scored_below_100': 1}
VACUITY CHECK
  total call sites of N examined                : 62
  of those, in functions that are report rows   : 1
```

**Pre-registered for every batch: Δ`matched_functions` 0, Δ`matched_code` 0, 0
rows crossing either way.** Functions additionally cannot move *in principle*:
relocation-name charges are argument-level (`diff_arg`), which `mpn` excludes by
construction.

## 5. Batches — predicted vs measured

Each batch: apply → `touch config/45410914/config.yml` → full `./tools/ninja-locked`
(rc=0) → set-diff of the `fuzzy==100` row set.

| batch | memberships | census B | commit | predicted | **measured** | rows in / out |
|---|---:|---:|---|---|---|---|
| A — guard-cleared | 8 | 572 | `8b5e5f9a` | Δ0 / Δ0 | **Δ0 / Δ0** | 0 / 0 |
| B — refuted | 20 | 1,200 | `817b9fb0` | Δ0 / Δ0 | **Δ0 / Δ0** | 0 / 0 |
| C — refuted | 20 | 1,200 | `451b6e34` | Δ0 / Δ0 | **Δ0 / Δ0** | 0 / 0 |
| D — refuted | 3 | 180 | `9e304cc4` | Δ0 / Δ0 | **Δ0 / Δ0** | 0 / 0 |
| **lane** | **51** | **3,152** | | **Δ0 / Δ0** | **Δ0 / Δ0** | **0 / 0** |

`matched_functions` 43,319 and `matched_code` 3,990,660 at every single
measurement point.

### 5.1 The Δ0 is distinguishable from inertness, because one row moved

`tools/rowset_snapshot.py` is the byte ledger but is **blind to a row already
below 100**, so a Δ0 batch is indistinguishable from one that did nothing.
`tools/w16ag_rowfuzzy_snapshot.py` records `(size, fuzzy, mpn)` for **all 69,216
rows**:

* **Batch A moved exactly one row.** `fuzzy_match_percent` −3e-06 pp:
  `?_M_insert_overflow_aux@?$vector@V?$Key@V?$vector@VColor@Hmx@@…`
  (`default/MeshAnim`, 324 B) picked up the single direction-(a) charge the
  predictor named. Its fuzzy is **99.75309 = 100 − 0.2/81 instructions** — a
  *fractional* `diff_arg` penalty, the right shape for a relocation-name charge
  rather than a broken instruction, and worth **0 bytes** because the row was
  already below 100.
* **Batches B, C, D moved 0 of 69,216 rows.** Measured, not inferred: that
  forgiveness was live at no scored row at all.

⇒ 50 of the 51 withdrawals are provably inert on the metric; 1 is provably live
and costs nothing. **No withdrawal dropped a row from 100**, so none of the 51
was hiding a wrong callee *that the grader was paying for* — the wrongness is in
the alias, not in our source, exactly as W16-AD found for its own 20.

## 6. Per-row disposition — all 51

**Every one of the 43 + 8 was withdrawn. Nothing was kept**, because the witness
reproduced `WITNESS_REFUTED` on 43/43 and the guard's true criterion failed 8/8.
`c_N`/`c_S` are the retail addresses where our folded spelling's callee and the
survivor's callee actually live — *different addresses* is the refutation.

| # | batch | census gi | group X | B | class | c_N @retail | c_S @retail | disposition |
|---|---|---|---|---|---|---|---|---|
| 1 | A | 563 | `0x8246efa8` | 112 | guard-cleared | `0x8246eb78` | `0x8246ebe0` | **withdrawn** |
| 2 | A | 942 | `0x8229ee78` | 80 | guard-cleared | `0x8228d530` | `0x8229d898` | **withdrawn** |
| 3 | A | 942 | `0x8229ee78` | 80 | guard-cleared | `0x8228d530` | `0x8229d898` | **withdrawn** |
| 4 | A | 338 | `0x822cdc18` | 60 | guard-cleared | `0x8229dc70` | `0x822cdb70` | **withdrawn** |
| 5 | A | 564 | `0x82706208` | 60 | guard-cleared | `0x8229d898` | `0x82706170` | **withdrawn** |
| 6 | A | 583 | `0x822cb4d8` | 60 | guard-cleared | `0x8229dc70` | `0x822cb430` | **withdrawn** |
| 7 | A | 589 | `0x822bc3d8` | 60 | guard-cleared | `0x8229dc70` | `0x822bc158` | **withdrawn** |
| 8 | A | 625 | `0x822bc368` | 60 | guard-cleared | `0x8229dc70` | `0x822b1728` | **withdrawn** |
| 9 | B | 564 | `0x82706208` | 60 | refuted | `0x82bba4c8` | `0x82706170` | **withdrawn** |
| 10 | B | 564 | `0x82706208` | 60 | refuted | `0x827adff8` | `0x82706170` | **withdrawn** |
| 11 | B | 564 | `0x82706208` | 60 | refuted | `0x823e4630` | `0x82706170` | **withdrawn** |
| 12 | B | 583 | `0x822cb4d8` | 60 | refuted | `0x823e4630` | `0x822cb430` | **withdrawn** |
| 13 | B | 583 | `0x822cb4d8` | 60 | refuted | `0x8229d898` | `0x822cb430` | **withdrawn** |
| 14 | B | 583 | `0x822cb4d8` | 60 | refuted | `0x82bba4c8` | `0x822cb430` | **withdrawn** |
| 15 | B | 583 | `0x822cb4d8` | 60 | refuted | `0x827adff8` | `0x822cb430` | **withdrawn** |
| 16 | B | 583 | `0x822cb4d8` | 60 | refuted | `0x823e4630` | `0x822cb430` | **withdrawn** |
| 17 | B | 589 | `0x822bc3d8` | 60 | refuted | `0x823e4630` | `0x822bc158` | **withdrawn** |
| 18 | B | 589 | `0x822bc3d8` | 60 | refuted | `0x8229d898` | `0x822bc158` | **withdrawn** |
| 19 | B | 589 | `0x822bc3d8` | 60 | refuted | `0x82bba4c8` | `0x822bc158` | **withdrawn** |
| 20 | B | 589 | `0x822bc3d8` | 60 | refuted | `0x827adff8` | `0x822bc158` | **withdrawn** |
| 21 | B | 589 | `0x822bc3d8` | 60 | refuted | `0x823e4630` | `0x822bc158` | **withdrawn** |
| 22 | B | 620 | `0x8237b938` | 60 | refuted | `0x8229dc70` | `0x8237b890` | **withdrawn** |
| 23 | B | 620 | `0x8237b938` | 60 | refuted | `0x823e4630` | `0x8237b890` | **withdrawn** |
| 24 | B | 620 | `0x8237b938` | 60 | refuted | `0x8229d898` | `0x8237b890` | **withdrawn** |
| 25 | B | 620 | `0x8237b938` | 60 | refuted | `0x82bba4c8` | `0x8237b890` | **withdrawn** |
| 26 | B | 620 | `0x8237b938` | 60 | refuted | `0x827adff8` | `0x8237b890` | **withdrawn** |
| 27 | B | 620 | `0x8237b938` | 60 | refuted | `0x823e4630` | `0x8237b890` | **withdrawn** |
| 28 | B | 625 | `0x822bc368` | 60 | refuted | `0x823e4630` | `0x822b1728` | **withdrawn** |
| 29 | C | 625 | `0x822bc368` | 60 | refuted | `0x8229d898` | `0x822b1728` | **withdrawn** |
| 30 | C | 625 | `0x822bc368` | 60 | refuted | `0x82bba4c8` | `0x822b1728` | **withdrawn** |
| 31 | C | 625 | `0x822bc368` | 60 | refuted | `0x827adff8` | `0x822b1728` | **withdrawn** |
| 32 | C | 625 | `0x822bc368` | 60 | refuted | `0x823e4630` | `0x822b1728` | **withdrawn** |
| 33 | C | 658 | `0x8232fb50` | 60 | refuted | `0x8229dc70` | `0x8232e6d0` | **withdrawn** |
| 34 | C | 658 | `0x8232fb50` | 60 | refuted | `0x823e4630` | `0x8232e6d0` | **withdrawn** |
| 35 | C | 658 | `0x8232fb50` | 60 | refuted | `0x8229d898` | `0x8232e6d0` | **withdrawn** |
| 36 | C | 658 | `0x8232fb50` | 60 | refuted | `0x82bba4c8` | `0x8232e6d0` | **withdrawn** |
| 37 | C | 658 | `0x8232fb50` | 60 | refuted | `0x827adff8` | `0x8232e6d0` | **withdrawn** |
| 38 | C | 658 | `0x8232fb50` | 60 | refuted | `0x823e4630` | `0x8232e6d0` | **withdrawn** |
| 39 | C | 668 | `0x827cda78` | 60 | refuted | `0x8229dc70` | `0x827cd8b8` | **withdrawn** |
| 40 | C | 668 | `0x827cda78` | 60 | refuted | `0x823e4630` | `0x827cd8b8` | **withdrawn** |
| 41 | C | 668 | `0x827cda78` | 60 | refuted | `0x8229d898` | `0x827cd8b8` | **withdrawn** |
| 42 | C | 668 | `0x827cda78` | 60 | refuted | `0x82bba4c8` | `0x827cd8b8` | **withdrawn** |
| 43 | C | 668 | `0x827cda78` | 60 | refuted | `0x827adff8` | `0x827cd8b8` | **withdrawn** |
| 44 | C | 668 | `0x827cda78` | 60 | refuted | `0x823e4630` | `0x827cd8b8` | **withdrawn** |
| 45 | C | 680 | `0x823c9178` | 60 | refuted | `0x823e4630` | `0x8229dc70` | **withdrawn** |
| 46 | C | 680 | `0x823c9178` | 60 | refuted | `0x8229d898` | `0x8229dc70` | **withdrawn** |
| 47 | C | 680 | `0x823c9178` | 60 | refuted | `0x82bba4c8` | `0x8229dc70` | **withdrawn** |
| 48 | C | 680 | `0x823c9178` | 60 | refuted | `0x827adff8` | `0x8229dc70` | **withdrawn** |
| 49 | D | 680 | `0x823c9178` | 60 | refuted | `0x823e4630` | `0x8229dc70` | **withdrawn** |
| 50 | D | 725 | `0x822c9048` | 60 | refuted | `0x824be668` | `0x8228d530` | **withdrawn** |
| 51 | D | 725 | `0x822c9048` | 60 | refuted | `0x824be668` | `0x8228d530` | **withdrawn** |

SPELLINGS (row # -> folded spelling):
1. `?_M_clear_after_move@?$vector@V?$Key@V?$vector@VColor@Hmx@@V?$StlNodeAlloc@VColor@Hmx@@@stlpmtx_std@@@stlpmtx_std@@@@V?$StlNodeAlloc@V?$Key@V?$vector@VColor@Hmx@@V?$StlNodeAlloc@VColor@Hmx@@@stlpmtx_std@@@stlpmtx_std@@@@@stlpmtx_std@@@stlpmtx_std@@IAAXXZ`
2. `??$_Copy_Construct@UAutoPropEntry@PropertyEventListener@@@stlpmtx_std@@YAXPAUAutoPropEntry@PropertyEventListener@@ABU12@@Z`
3. `??$_Param_Construct@UAutoPropEntry@PropertyEventListener@@U12@@stlpmtx_std@@YAXPAUAutoPropEntry@PropertyEventListener@@ABU12@@Z`
4. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
5. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
6. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
7. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
8. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
9. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
10. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
11. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
12. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
13. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
14. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
15. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
16. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
17. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
18. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
19. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
20. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
21. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
22. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
23. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
24. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
25. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
26. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
27. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
28. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
29. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
30. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
31. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
32. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
33. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
34. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
35. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
36. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
37. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
38. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
39. `??$_Copy_Construct@UNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@@stlpmtx_std@@YAXPAUNode@?$ObjPtrVec@VRndTransformable@@VObjectDir@@@@ABU12@@Z`
40. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
41. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
42. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
43. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
44. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
45. `??$_Copy_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
46. `??$_Param_Construct@UBoneOp@CharSignalApplier@@U12@@stlpmtx_std@@YAXPAUBoneOp@CharSignalApplier@@ABU12@@Z`
47. `??$_Param_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@V?$RangedData@I@?$RangedDataCollection@I@@@stlpmtx_std@@@0@ABV10@@Z`
48. `??$_Param_Construct@V?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@V?$StlNodeAlloc@V?$RangedData@VRGTrill@@@?$RangedDataCollection@VRGTrill@@@@@stlpmtx_std@@@0@ABV10@@Z`
49. `??$_Param_Construct@V?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeSection@@@stlpmtx_std@@@0@ABV10@@Z`
50. `??$_Copy_Construct@VTransformCrowd@@@stlpmtx_std@@YAXPAVTransformCrowd@@ABV1@@Z`
51. `??$_Param_Construct@VTransformCrowd@@V1@@stlpmtx_std@@YAXPAVTransformCrowd@@ABV1@@Z`

## 7. Validation — `python3 tools/icf_alias_finder.py --validate` (built tree)

```
== freshness == manifest 2026-09-14T18:11:39Z / 4,328 objects | patch state verified | split current
COVERAGE: 1634 groups classified (1634/1634 reached, 6877 member spellings looked up)
  target side  : 3115 live target objs, 28173 mangled names indexed
  compiled side: 1213 compiled objs, 840153 symbols indexed
  OK (MAP-CONSISTENT)     1387
  TOLERATED PLACEHOLDER_SURVIVOR    34
  TOLERATED STALE_SPELLING          85
  TOLERATED SURVIVOR_MISLABELED     28
  TOLERATED UNWITNESSED             99
  CONTRADICTION_EXEMPT       1
  CONTRADICTED (FATAL)       0
VALIDATE: PASS -- 1387 map-consistent, 246 tolerated (enumerated above), 0 contradicted, 1634 total
```

**PASS, 0 CONTRADICTED, 1,634 groups** — the same group count W16-AD reported,
which is the check that nothing was pruned.

## 8. Gates

```
GATE1 BUILD rc=0                                              (full ./tools/ninja-locked)
GATE2 python3 scripts/verify_ruler_agreement.py --check       rc=0
      grader config source: build/45410914/report.json provenance.diff_config
        OK  functionRelocDiffs = name_check
        OK  combineDataSections = true
        OK  combineTextSections = true
        OK  ppc.calculatePoolRelocations = false
      OK: both objdiff-cli entry points resolve the same ruler.
GATE3 python3 scripts/verify_objs_patched.py --verify-manifest rc=0
      [patch-state] OK: 1213 decomp, 3115 target objects match (tree_sha256=9bc7c08ad8a6ba4c)
GATE4 tools/native_build_gate.sh
```

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The lane touches **0 files under `src/`** (`git diff --name-only main..HEAD`),
so the native gate is structurally inapplicable — it was run anyway, last, since
it can only add information.

## 9. What I did NOT do, and why

* **Did not touch any `NO_WITNESS_FOLDED_SIDE` row** (496 rows / 30,864 B — 75%
  of the class). Absence of a witness is absence of evidence, not refutation.
  Brief-excluded and correctly so.
* **Did not touch the 52 `WITNESS_INCONCLUSIVE` rows beyond the 8 the
  twin-guard blocked.** The other 44 are inconclusive for reasons the guard
  audit does not reach (`INCONSISTENT`, non-`.pdata` destinations), and no
  instrument here adjudicates them.
* **Did not touch the Accomplishment rows** — W16-AF owns them (brief).
* **Did not edit `scripts/target_symbol_map.json`** at all.
* **Did not prune a single group.** 1,634 before and after; `folded` 5,294 →
  5,243; 51 `withdrawn` records added.
* **Did not fix any source line** — same finding as W16-AD: in all 51, our
  source spells a callee that genuinely exists at a distinct retail address.
  The defect is the alias, not the source.
* **Did not repair `w16ad_predict_withdrawal.py` in place.** Its defect is
  documented and superseded by `w16ag_predict_withdrawal.py`; editing a
  predecessor's artifact would make its published figures irreproducible.
  ⇒ **follow-on:** any inherited prediction from that tool is uninformative and
  should be re-derived, exactly as §4.2 flagged for `gi`-keyed ablations.
* **Did not re-measure the inherited ablation figures** invalidated by the
  `gi`-keying defect. Still open, still flagged.
* **Did not attempt to raise the ceiling past 71 / 4,400 B.** The remaining
  refuted population is exhausted: 63 refuted = 20 (W16-AD) + 43 (here), plus
  the 8 guard-cleared.

### 9.1 Open, with the evidence that would settle it

The one thing this lane could not settle is whether the **496
`NO_WITNESS_FOLDED_SIDE`** rows are sound folds or unrefuted ones. The witness
declines them for a stated structural reason — the folded spelling's callee has
no map-named EQ caller anywhere, so no branch can be decoded for it.

**What would settle it:** a map identification for any one caller of `c_N` that
compares EQ to retail at its mapped address. That converts the row from
"no witness" to a decidable one by the existing instrument, unchanged. Until
then these rows are neither confirmed nor refuted, and withdrawing them would be
a clobber.

## 10. Lane-internal before / after

| | `matched_functions` | `matched_code` |
|---|---:|---:|
| lane baseline (main `0abb5e91`) | 43,319 | 3,990,660 |
| batch A — 8 guard-cleared | 43,319 | 3,990,660 (Δ0 / Δ0, pre-registered) |
| batch B — 20 refuted | 43,319 | 3,990,660 (Δ0 / Δ0, pre-registered) |
| batch C — 20 refuted | 43,319 | 3,990,660 (Δ0 / Δ0, pre-registered) |
| batch D — 3 refuted | 43,319 | 3,990,660 (Δ0 / Δ0, pre-registered) |
| **lane final** | **43,319** | **3,990,660** |

Net lane delta **0 / 0**, as pre-registered at every step. The product is the 51
withdrawn memberships, the correction of the guard population to 8 / 572 B, the
corrected strict twin test, and the finding that the predecessor's withdrawal
predictor was structurally incapable of predicting anything.

## 11. Commits on `w16-ag`

| sha | contents |
|---|---|
| `df52b036` | own 643-row witness re-run (643/643 agree) + guard adjudication tool + twin control |
| `8b5e5f9a` | batch A — 8 guard-blocked withdrawn, guard shown a false refusal 8/8 |
| `817b9fb0` | batch B — 20 refuted |
| `451b6e34` | batch C — 20 refuted |
| `9e304cc4` | batch D — final 3 refuted |

> The gate was run **after** the last alias edit (batch D) and its line above is
> transcribed from the run, not predicted — it was drafted before the run and
> then checked against it. The only edit made after the gate is this document,
> which is not a build input and cannot affect any of the four gates.
