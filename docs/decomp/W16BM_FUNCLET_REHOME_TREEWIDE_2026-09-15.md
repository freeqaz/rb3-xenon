# W16-BM — tree-wide re-home of the MIS-PINNED EH funclets (W16-BJ §5.9 item 1)

Lane **W16-BM**, 2026-09-15. Worktree `~/tmp/wt-w16-bm`, branch `w16-bm`, based on main
`6b176997ccd3`. Scope: `config/45410914/splits.txt` **`.text` lines only**, `tools/funclet_homing.py`,
and this record. **No map row, no alias group, no `src/` file, no `.pdata` line touched.**

W16-BJ moved 26 funclets as a bounded pilot and sized the remainder. This lane moves the
remainder: **700 of the 706 MIS-PINNED rows** (6 skipped under live concurrency bars).

---

## 1. Baseline and census reproduction

Built in-worktree (`~/tmp/rb3_build_w16bm_1.log`, `BUILD rc=0`), settled to **zero compile edges**
on build 2 (`_2.log`). Every briefed figure confirmed literally:

| key | briefed (ledger `39872374106e`) | measured |
|---|---:|---:|
| `matched_functions` | 43,554 | **43,554** |
| `matched_code` | 4,040,412 | **4,040,412** |
| `matched_code_percent` | 39.4299 | **39.42993** |
| `total_functions` | 69,240 | **69,240** |
| `total_code` | 10,247,068 | **10,247,068** |
| `masked_equal_functions` | 23,078 | **23,078** |
| objdiff | 4.2.9 `5a51cd51fe0a353f` | **4.2.9 `5a51cd51fe0a353f`** |

Ruler read out of `report.json`'s `provenance.diff_config`, not assumed: `functionRelocDiffs=name_check`,
`ppc.calculatePoolRelocations=false`, both `combine*` true — the graded ruler with all four pins.
Renamer liveness (the reflinked-worktree trap): **38,843 mangled `?…` names in a 400-obj sample
alone**, far above the brief's 27,000 floor, so every "absent" below is a real absence.
Row-set baseline proven set-identical to `~/tmp/rows_w16bm_base.json` (**CROSSED IN 0 / FELL OUT 0**),
so this tree is a true main baseline.

### 1.1 The census does NOT reproduce the briefed 708 — and the 2-row gap is landed progress

| verdict | briefed (BJ post-pilot) | **measured here** |
|---|---:|---:|
| HOMED | 24,219 / 937,352 B | **24,221 / 937,424 B** |
| **MIS-PINNED** | **708 / 28,248 B** | **706 / 28,176 B** |
| ORPHAN | 1,352 / 56,168 B | **1,352 / 56,168 B** ✅ |
| UNPINNED-FUNCLET | 42 / 1,676 B | **42 / 1,676 B** ✅ |
| total | 26,321 / 1,023,444 B | **26,321 / 1,023,444 B** ✅ |

⚠ First tell: `.text pins` read **6,683**, where BJ's post-edit validate printed **6,685**.
Rather than assume, I ran the *same classifier* against BJ's own pre-rebase `splits.txt`
(`git show 645e765a:config/45410914/splits.txt`) and reproduced BJ's four verdicts **exactly**
— HOMED 24,219 / MIS-PINNED **708** / ORPHAN 1,352 / UNPINNED 42. So the instrument is
byte-faithful to BJ's and the difference is a **real, landed change**, not a measurement drift.

Set-diffing the two MIS-PINNED sets names the 2 rows, and **both are rows BJ FILED under a
concurrency bar** (its §3.2 list):

| row | size | BJ state | now | by |
|---|---:|---|---|---|
| `0x8227A800` | 32 B | MIS-PINNED, pinned `Ham.cpp` | **HOMED** under `BandCharacter.cpp` | W16-BH `2361a5c1` deleted the `Ham.cpp:` heading (a DC3 scaffold pin) |
| `0x8268B9EC` | 40 B | MIS-PINNED under `BandUser.cpp` | **HOMED** (same heading) | W16-BI `b9b3d8de` merged BandUser's flanking blocks |

32 + 40 = **72 B**, and 28,248 − 28,176 = **72 B** exactly. ⇒ the briefed figure was correct when
measured; the delta is two other lanes landing. **The remaining class is 706 / 28,176 B.**

### 1.2 The exposure, re-derived on this tree (BJ's 419 was pre-pilot)

| | rows | bytes |
|---|---:|---:|
| MIS-PINNED at `fuzzy == 100` — **carried by a false twin** | **399** | **15,832** |
| MIS-PINNED below `fuzzy == 100` | 307 | 12,344 |
| MIS-PINNED at `mpn == 100` (function credit at stake) | 485 | — |
| MIS-PINNED with no report row | 0 | — |

Of the **700 movable** rows, **395 / 15,704 B** read `fuzzy == 100` today on a unit whose object
never emitted them. That is what this lane is paying with.

---

## 2. The instrument, and the two bugs the assertions caught

`tools/funclet_homing.py` gains `--emit-splits` / `--apply` (commit `f3a9188a`). Design choices
that are *not* obvious, each measured rather than assumed:

* **Minimal-span moves, not BJ's "delete the donor block, extend the receiver".** BJ's §4 argued
  the narrow edit is not well-formed, because moving 1 of 3 funclets would leave two siblings of the
  same parent in a foreign unit. **That objection dissolves at tree scale**: this lane moves *every*
  MIS-PINNED row, so the siblings travel anyway. Minimal spans also avoid dragging non-funclet
  occupants into a new unit, which would re-home functions outside this lane's mandate.
* **A catch funclet's 8-byte EH prefix travels with it** — but only when it lies in the *same* donor
  block. Measured: all **59** catch rows in the class have `{__CxxFrameHandler,&FuncInfo}` at
  `addr-8` and **none** of those 8 bytes lies inside another report row (0/59), so the extension is
  exact. The block guard exists because the straddle assertion *fired on the first real run*
  (`0x822A2F80` sits exactly at its donor block's start, so its prefix belongs to the preceding
  heading). Left-behind prefixes cost nothing: objdiff marks `except_data_*` **Hidden**, excluding it
  from numerator and denominator.
* Every funclet has exactly **1 parent** (asserted, not assumed: 0 of 706 rows have ≠1), and every
  row has a nonzero extent taken from `report.json` with `int()` coercion.

### 2.1 Eight assertions, each PROVEN able to fail

`~/tmp/bm/mutate_assertions.py` mutates one input / sabotages one step per case and **requires** a
`Refuse`; the unmutated plan is the control. **9/9 behaved as required:**

| assertion | sabotage | tripped with |
|---|---|---|
| A1 covered-address set identical | `merge_dirty` silently drops a range | `A1: covered-address set CHANGED (1421 intervals/8714596 B -> 1724/6863376 B)` |
| A2 no overlap | overlapping input ranges | `A2: overlapping ranges in T` |
| A2 no empty/inverted | inverted range | `A2: empty/inverted range` |
| A3 only `.text` differs | emit a `.pdata` line instead | `A3: 6678 non-.text line(s) changed` |
| A4 no zero-`.text` heading | `_drop_drained` returns `[]` | `A4: a heading survived with zero .text ranges` |
| A5 boundary on a symbol edge | shift one span by +4 | `A5: 2 new boundary/ies not on a known symbol edge` |
| A8 untouched heading byte-identical | merge every adjacent block | `A8: 425 untouched heading(s) were REFORMATTED` |
| straddle guard | grow one span past its block | `0x82272B98-0x82272C00 straddles donor MessageTimer.cpp block` |
| **CONTROL** unmutated plan | — | **passes, no Refuse** |

⛔ **A8 is not theoretical — it is the bug I shipped first and had to catch.** The initial rewriter
merged *every* pre-existing adjacent block tree-wide: **`.text` lines 6,683 → 4,361**. A1 could
**not** see it, because collapsing touching blocks is byte-neutral on the covered set — a whole-file
reformat that passes the partition test. It also matters mechanically: jeff derives **one `.pdata`
range per `.text` block** (`split.rs` `split_pdata`), so collapsing blocks silently changes the
target `.pdata` layout. ⇒ **a partition assertion is not a formatting assertion**; both are needed.

### 2.2 The twin probe, and why it is not vacuous

⛔ **`coff_bodies_ext.function_bodies_ext` cannot be used for funclet twins.** It calls
`is_aux_code_symbol()`, which drops every `__unwind$` / `__ehhandler$` symbol — exactly the bodies
wanted. A probe built on it returns "no twin" for **every** row: a decisive-looking negative that is
pure instrument blindness. Our objs really do carry them (`system/rndobj/Utl.obj`: 1,612 `__unwind$`
+ 124 `__catch$` type-0x20 defs), so the probe slices the COFF itself.

**Anti-vacuity control, drawn from HOMED rows — a *different* population from the MIS-PINNED rows
the probe is applied to.** This is precisely where BJ's control was confounded (§5.5: its 8/8
biconditional was measured on the donor, where a `masked_equal` false pairing has a byte twin *by
construction*):

| conditional | measured |
|---|---:|
| P(`mpn == 100` \| twin) | **100.0%** (22,263 / 22,263) |
| P(`mpn == 100` \| no twin) | **8.7%** (170 / 1,958) |
| P(`fuzzy == 100` \| twin ∧ `mpn == 100`) | **89.8%** (19,987 / 22,263) |
| P(`fuzzy == 100` \| no twin ∧ `mpn < 100`) | **0.0%** (0 / 1,788) |

★ **The 89.8% SIZES BJ's §5.5 miss class for the first time: 10.2% of twin-bearing rows sit at
`mpn == 100` with `fuzzy < 100`**, on a relocation-name residual the masked comparator masks by
construction. So the probe is an **`mpn`-grade instrument** — sharp on `matched_functions`,
structurally banded on `matched_code`. Predictions below say which measure they are for.

**Over-subscription** (BJ's named downside mechanism) is checked and is immaterial: of 184
receivers, exactly **1** (`MetaMaterial.cpp`, 0 code slices in our obj) would have more funclet
targets than our object supplies — and the twin test already classifies that row as no-twin.

---

## 3. Batching and the concurrency bars

Twin availability splits the 700 movable rows:

| batch | rows | bytes | at `fuzzy==100` now | at `mpn==100` now |
|---|---:|---:|---:|---:|
| **1 — twin available in receiver** | **423** | 15,884 | 234 (8,500 B) | 277 |
| **2 — no twin** | **277** | 12,084 | 161 (7,204 B) | 202 |

**6 rows SKIPPED under live bars** (donor *or* receiver is barred; matched on the full heading and
on its basename — deliberately over-broad, and the four barred names each exist exactly once
unqualified, so nothing is lost to over-barring):

| row | donor → receiver | bar | owner |
|---|---|---|---|
| `0x8227A280` | BandCamShot.cpp → **BandCharacter.cpp** | receiver | W16-BL |
| `0x8228732C` | **BandCharacter.cpp** → RockCentral.cpp | donor | W16-BL |
| `0x8228C960` | **BandCharacter.cpp** → BandDirector.cpp | donor | W16-BL |
| `0x8228CA90` | **BandCharacter.cpp** → BandDirector.cpp | donor | W16-BL |
| `0x823F4A30` | **UI.cpp** → CharTaskMgr.cpp | donor | W16-BK |
| `0x8247E084` | **Line.cpp** → Gen.cpp | donor | W16-BL |

Five are BJ §3.2's filed rows; `0x8247E084` is new to this lane's bar set. BJ's `0x824EA034` /
`0x824EA05C` (Instance → BandUser) **are moved here**, because BH/BI's `BandUser.cpp:` bar is lifted.

**3 headings drain completely and lose their ENTIRE entry** (requirement (c)):
`FlowDistance.cpp`, `FlowSound.cpp`, `HamCharacter.cpp`. Each has exactly one `.text` block, and the
block is **tiled exactly** by mis-pinned funclets of a parent in another unit (68 B = 68, 108 = 108,
432 = 432) — i.e. the whole pin is foreign. All three are declared in `objects.json` with real
compiled base objects, so this was checked rather than waved through: `objdiff.json` units come from
dtk's unit list, which is derived from `splits.txt`, so a heading with no `.text` simply stops being
declared — the same status as any un-pinned `objects.json` entry — and no gate invariant references
it. `HamCharacter.cpp`'s 9 rows are all funclets of `0x82812080` in `system/ui/UILabelDir.cpp`.

---

## 4. Pre-registered prediction (recorded and committed BEFORE the build)

Method: BJ §4's receiving-unit twin test, read from our COFF **after** the patchers, calibrated by
§2.2's control. **Stated per measure, because the instrument only supports one of them sharply.**

### Batch 1 — 423 rows, twin available

* **Δ`matched_functions` = +146** — all 423 reach `mpn == 100` (control: P = 100.0%); 277 already
  are, so 146 cross. Sharp: this is what the instrument measures.
* **Δ`matched_code` ≈ +5,764 B, band [−872, +7,384]** — central = 89.8% × 15,884 B at `fuzzy == 100`
  after (14,264 B) minus the 8,500 B held now. The band's lower end is "none of the 189 sub-100 rows
  crosses **and** 10.2% of the 234 already-100 rows fall on a relocation-name residual"; the upper is
  "all 189 cross, none falls". **The band is wide because the comparator is `mpn`-grade (BJ §5.5),
  not because the rows are unclear.**
* Expected rows `<100 → 100` ≈ **170**; rows `100 → <100` ≈ **24**.

### Batch 2 — 277 rows, no twin

* **Δ`matched_functions` = −202**, band [−202, −178] — all 202 rows now at `mpn == 100` lose their
  pairing (control: P(`mpn100` | no twin) = 8.7%, so up to ~24 might survive).
* **Δ`matched_code` = −7,204 B** — the 161 rows at `fuzzy == 100`. The 116 already below 100
  contribute 0 on both measures whatever they do.
* Expected rows `<100 → 100` = **0**; rows `100 → <100` = **161**.

### Aggregate

| measure | prediction | band |
|---|---:|---|
| Δ`matched_functions` | **−56** | −56 … −32 |
| Δ`matched_code` | **−1,440 B** | −8,076 … +180 |
| Δ`total_functions` | **0** | a re-home moves rows, it does not create them |
| Δ`total_code` | **0** | same |

⚠ The one mechanism that could break Δtotal = 0 is dtk's **Class-4 over-carve merge**
(`merge_branch_reached_overcarve_tails`), which converges *across* re-splits — hence ABSPLIT-1's
rule that each leg must re-split to a `symbols.txt` fixed point. Flagged, and checked below.

⇒ **This is an ACCURACY play and it is pre-registered as net-NEGATIVE on both headline measures.**
Standing directive: a code% drop from a truer attribution is a win, and a metric that hides real
bugs is worse than a lower metric. The 15,704 B being surrendered was never earned — it was paid to
units whose objects never emitted those funclets. Nothing below is padded, and no row is skipped for
being a loser.
