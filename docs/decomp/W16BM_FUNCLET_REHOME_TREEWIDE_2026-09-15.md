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

## 5. Measurement vs the pre-registration

Both batches were built with `touch config/45410914/config.yml` then a full `./tools/ninja-locked`
redirected to a log with `$?` tested on the **next** line (never `| tail`, which reads tail's status),
iterated to a `symbols.txt` fixed point per ABSPLIT-1.

⚠ Both batches' first build returned **rc=1** on the split guard's *"THE SPLIT REWROTE ITS OWN INPUT"*.
That is the documented `.pdata` re-derivation — `.pdata` is derived output and every split run clears
and re-derives one range per `.text` block — and W16-BJ hit the identical thing ("recovery is one
build"). Batch 2: BUILD6 rc=1, BUILD7 rc=0, BUILD8 rc=0 with `symbols.txt` byte-identical ⇒ fixed
point reached in one rebuild, **no second split iteration needed on either batch**.

### 5.1 Batch 1 — 423 rows, twin available (commit `4093d7ad`)

| measure | before | after | Δ measured | Δ predicted | verdict |
|---|---:|---:|---:|---:|---|
| `matched_functions` | 43,554 | 43,699 | **+145** | +146 | **−1 miss** |
| `matched_code` | 4,040,412 | 4,046,168 | **+5,756 B** | +5,764 B (band −872 … +7,384) | **−8 B, in band** |
| `matched_code_percent` | 39.42993 | 39.486103 | +0.056173 | — | |
| `total_functions` | 69,240 | 69,240 | 0 | 0 | as predicted |
| `total_code` | 10,247,068 | 10,247,068 | 0 | 0 | as predicted |

Reconciled to the byte: moved rows **+5,676 B**, collateral **+80 B**, total **+5,756 B**. An
independent `mpn == 100` row count over `report.json` gives **43,699**, equal to `matched_functions`
— two derivations sharing no arithmetic.

**The −1 function shortfall, localised rather than smoothed.** It is *not* in the moved set (423/423
at `mpn == 100` after) and *not* in the 4 fuzzy-collateral rows (all 4 still `mpn == 100`). It is
therefore a **non-moved row that left `mpn == 100` while staying below `fuzzy == 100`** — a membership
change `rowset_snapshot.py` is **structurally blind to**, because it keys on `fuzzy == 100` only. The
response was to fix the instrument, not the record: `~/tmp/bm/rowdump.py` dumps
`unit::name → (size, fuzzy, mpn)` so both memberships are visible. Batch 2 was measured with it.

### 5.2 Batch 2 — 277 rows, no twin (commit `3c7bfe38`)

| measure | before | after | Δ measured | Δ predicted | verdict |
|---|---:|---:|---:|---:|---|
| `matched_functions` | 43,699 | 43,519 | **−180** | −202 (band −202 … −178) | **in band** |
| `matched_code` | 4,046,168 | 4,039,316 | **−6,852 B** | −7,204 B (point, no band) | **+352 B miss** |
| `matched_code_percent` | 39.486103 | 39.419235 | −0.066868 | — | |
| `masked_equal_functions` | 23,223 | 23,043 | **−180** | — | = Δ`matched_functions` exactly |
| `total_functions` | 69,240 | 69,240 | **0** | 0 | as predicted |
| `total_code` | 10,247,068 | 10,247,068 | **0** | 0 | as predicted |

★ **The loss side was EXACT on every axis, to the row and to the byte.** Pre-registered: *"rows
`100 → <100` = 161"*, *"Δ`matched_code` = −7,204 B — the 161 rows at `fuzzy == 100`"*, *"all 202 rows
now at `mpn == 100` lose their pairing"*. Measured: **161** rows fell out of `fuzzy == 100` worth
**7,204 B**, and **202** keys fell out of `mpn == 100`.

★ **`masked_equal_functions` fell by exactly 180 — identical to Δ`matched_functions`.** The entire
net function loss is the byte-signature-paired (false-twin) class and nothing else. That is the
mechanism BJ §5 named, now priced on 277 rows.

⚠ **Keying caveat, stated because it changes the reading.** `rowdump.py` keys on `unit::name`, and a
re-homed row *changes unit*. A moved row that keeps its credit therefore appears as one FELL OUT
(donor key) **plus** one CROSSED IN (receiver key) = net 0. So "202 out / 22 in" does **not** mean 202
rows were damaged; it means 202 keys vacated and **22 of those rows re-acquired 100 under their new
unit key**. The headline measures are key-independent and are the authority.

**The one real miss: I pre-registered rows `<100 → 100` = 0 and measured 7 (+352 B).** The function
band already anticipated survivors — *"P(`mpn100` | no twin) = 8.7%, so up to ~24 might survive"* —
and **22 survived**, so the banded measure was well calibrated. The byte measure was stated as a bare
point value with no band, and that is exactly where it missed. **Lesson: the batch whose mechanism I
claimed to understand least got the tightest prediction interval. The band belonged on both measures.**

### 5.3 Two gain mechanisms the pre-registration did not model

Neither is noise; both are consequences of re-homing that the twin probe **cannot** see by
construction. 19 of the 22 surviving rows are moved rows; 3 are not.

**(a) Receiver-side sibling pairing — 19 rows.** Arriving funclets pair with **each other** once
co-resident. The probe reported `twin available in receiver: 0 / 277` because it probes the
receiver's compiled object **as it stands before the arrival**; a twin that the move itself creates is
invisible to it. Concentration is in the multi-arrival receivers: `BaseMaterial.cpp` took 12 funclets
at once (12 crossed in), `MatAnim.cpp` 27 (2), `system/bandobj/DialogDisplay.cpp` 6 (3),
`BandSongMetadata.cpp` 5 (1); `CharIKSliderMidi.cpp` is the single 1-arrival survivor. **36 of 89
receivers took ≥2 funclets**, so the mechanism had ample opportunity. ⇒ **A pre-move twin probe is a
LOWER BOUND on post-move credit, never an estimate.** This is a bound on the instrument, not a defect
in it, and it is the honest reason the batch split read 0/277.

**(b) Donor-side liberation — 3 rows, and the finding I did not expect at all.** Draining an alien
funclet **frees** the byte-signature pairing its presence was absorbing, so the donor unit's
**genuine** funclets re-pair. `CharUpperTwist.cpp` gave up six funclets at
`0x82329FD8`–`0x8232A0C4` — roughly 1 MB from its real body at `0x823C6xxx`, a glaring mis-pin — and
its own rows moved:

| row | `mpn` before | after |
|---|---:|---:|
| `fn_823C6D88` | 0.0 | **100.0** |
| `fn_823C6DDC` | 0.0 | **100.0** |
| `fn_823C6D1C` | 99.94118 | **100.0** |
| `fn_823C6D60` | 93.9 | 99.8 |
| `fn_823C6DB4` | 93.9 | 99.8 |
| `fn_823C6E08` | 0.0 | 93.9 |

⇒ **the false-twin mechanism running in reverse.** A wrong pin does not merely mis-credit the
squatter; it *denies* the rightful occupant its pairing. Re-homing pays on the donor side as well as
costing on the receiver side, and **this direction is pure accuracy — the rows that gained are the
ones that actually belong to the unit.**

### 5.4 Collateral is local and immaterial to both rulers

Of the 19 **non-moved** rows whose score changed anywhere in the tree:

| unit role in batch 2 | rows up | rows down | net Δ`fuzzy==100` bytes |
|---|---:|---:|---:|
| donor | 8 | 1 | **+68 B** |
| donor **and** receiver | 4 | 0 | 0 |
| receiver only | 0 | 3 | 0 |
| **neither** | 0 | **3** | **0** |

The 3 rows in units that neither donated nor received are `BandStorePanel::fn_82607184` and
`fn_8260715C` (99.9 → 99.8) and `system/os/UsbMidiGuitar::fn_82C3FA94` (78.5 → 0.0). **All three are
below 100 on BOTH rulers before and after**, so they contribute exactly 0 to `matched_functions` and
`matched_code`. Recorded as unexplained score churn with a **measured zero effect on both headline
measures** — not waved away, and not inflated into a finding.

### 5.5 Aggregate, both batches, against the lane baseline

| measure | baseline | final | Δ measured | Δ pre-registered | band | verdict |
|---|---:|---:|---:|---:|---|---|
| `matched_functions` | 43,554 | **43,519** | **−35** | −56 | −56 … −32 | **in band** |
| `matched_code` | 4,040,412 | **4,039,316** | **−1,096 B** | −1,440 B | −8,076 … +180 | **in band** |
| `matched_code_percent` | 39.42993 | **39.419235** | **−0.010695** | — | — | |
| `total_functions` | 69,240 | 69,240 | **0** | 0 | — | as predicted |
| `total_code` | 10,247,068 | 10,247,068 | **0** | 0 | — | as predicted |

Δ`total_*` = 0 on both batches confirms the flagged **Class-4 over-carve merge** hazard did not fire —
no row was created or destroyed, and no heading drained in batch 2.

**The net cost of making 700 pins true is 35 functions and 1,096 bytes — 0.0107 pp.** That is the
whole price of the accuracy play, and it is ~13× smaller than batch 2's gross loss because batch 1's
correctly-homed rows paid for most of it.

### 5.6 Closing census — the lane's actual deliverable

| verdict | rows before | rows after | bytes after |
|---|---:|---:|---:|
| HOMED | 24,221 | **24,921** | 965,392 |
| **MIS-PINNED** | **706** | **6** | **208** |
| ORPHAN | 1,352 | 1,352 | 56,168 |
| UNPINNED-FUNCLET | 42 | 42 | 1,676 |

**700 of 706 MIS-PINNED funclets re-homed; the remaining 6 are exactly the barred rows** and nothing
else — no row was skipped for being a loser:

| funclet | size | pinned to | parent's unit | bar |
|---|---:|---|---|---|
| `0x8227A280` | 32 B | BandCamShot.cpp | BandCharacter.cpp | W16-BL |
| `0x8228732C` | 40 B | BandCharacter.cpp | RockCentral.cpp | W16-BL |
| `0x8228C960` | 32 B | BandCharacter.cpp | BandDirector.cpp | W16-BL |
| `0x8228CA90` | 32 B | BandCharacter.cpp | BandDirector.cpp | W16-BL |
| `0x823F4A30` | 40 B | UI.cpp | CharTaskMgr.cpp | W16-BK |
| `0x8247E084` | 32 B | Line.cpp | Gen.cpp | W16-BL |

## 6. Item 5 — the ORPHAN sweep (REPORT ONLY; this lane took no action on it)

**Self-validation first:** the sweep reproduces the briefed ORPHAN figures **exactly — 1,352 rows /
56,168 B** — before any conclusion is drawn from it.

ORPHAN = the funclet has parents, but **every** parent address falls outside all 6,556 pinned `.text`
blocks, so no pin move can help it; it needs its parent **identified**. Parent → `auto_*` cluster was
resolved through `report.json`, keying on the `fn_<VA>` row names that `auto_*` units carry (those
names encode the VA, so this avoids dtk's synthetic `.s` address columns entirely).

| | rows | bytes |
|---|---:|---:|
| ORPHAN total | 1,352 | 56,168 |
| parent resolves to an `auto_*` cluster | **1,344** | **55,856** (99.4%) |
| parent unresolved in `report.json` | 8 | 312 |
| funclet **already co-resident** in that same cluster ⇒ pinning it homes the funclet with **no move** | **1,170** | **48,768** |
| funclet lives elsewhere ⇒ needs the pin **and** a re-home move | 174 | 7,088 |

**68 distinct `auto_*` clusters**; multi-parent-unit funclets: **0**. Top 20 = **66.8%** of the bytes.

| # | `auto_*` cluster | rows | bytes | co-resident | elsewhere | distinct parents |
|---:|---|---:|---:|---:|---:|---:|
| 1 | `auto_03_822FC4F8_text` | 125 | **5,348** | 0 | 125 | **1** |
| 2 | `auto_03_82B497A4_text` | 73 | 3,040 | 73 | 0 | 25 |
| 3 | `auto_03_82AABF60_text` | 68 | 2,776 | 68 | 0 | 23 |
| 4 | `auto_03_82B3A488_text` | 60 | 2,528 | 60 | 0 | 17 |
| 5 | `auto_03_82B058F8_text` | 60 | 2,464 | 60 | 0 | 22 |
| 6 | `auto_03_82560B08_text` | 60 | 2,252 | 60 | 0 | 15 |
| 7 | `auto_03_82B2CF08_text` | 51 | 2,072 | 51 | 0 | 21 |
| 8 | `auto_03_82B17810_text` | 41 | 2,000 | 41 | 0 | 19 |
| 9 | `auto_03_82B3DD60_text` | 46 | 1,872 | 46 | 0 | 9 |
| 10 | `auto_03_82B34FAC_text` | 36 | 1,516 | 36 | 0 | 12 |
| 11 | `auto_03_823ECD58_text` | 37 | 1,464 | 37 | 0 | 17 |
| 12 | `auto_03_82B0A730_text` | 32 | 1,416 | 32 | 0 | 14 |
| 13 | `auto_03_82A6DC40_text` | 32 | 1,348 | 32 | 0 | 12 |
| 14 | `auto_03_82B2FEA4_text` | 30 | 1,272 | 30 | 0 | 10 |
| 15 | `auto_03_82AFE7A4_text` | 25 | 1,068 | 25 | 0 | 8 |
| 16 | `auto_03_82AC0C78_text` | 26 | 1,060 | 26 | 0 | 8 |
| 17 | `auto_03_82B14F90_text` | 25 | 1,056 | 25 | 0 | 4 |
| 18 | `auto_03_82AF926C_text` | 22 | 980 | 22 | 0 | 3 |
| 19 | `auto_03_825632BC_text` | 26 | 888 | 26 | 0 | 2 |
| 20 | `auto_03_82B4547C_text` | 20 | 880 | 20 | 0 | 9 |

⛔ **Do NOT price this as a byte lever, and the reason is in the address band.** Split at
`0x82A00000` (CLAUDE.md's vendor/Quazal boundary — itself known to be ~450 KB low, so this
understates the vendor share):

| band | rows | bytes |
|---|---:|---:|
| below `0x82A00000` (HMX game/engine) | 395 | **15,536** |
| at/above `0x82A00000` (vendor/Quazal) | **949** | **40,320** (72.2%) |

**72% of the ORPHAN bytes sit in the band where source is absent or is a 7-line Quazal map scaffold.**
Pinning those clusters buys a *pairable row at 0% with no content* — the `ForceEmit_*` failure mode the
project explicitly forbids. The reachable part of item 5 is the **395 rows / 15,536 B below
`0x82A00000`**, and even there the payout is attribution truth, not bytes.

### 6.1 The one clean, high-value recommendation — `auto_03_822FC4F8_text`

Cluster #1 is shaped unlike every other row in the table (**125 funclets, exactly ONE parent, and the
only cluster whose funclets all live elsewhere**), and it is the strongest recommendation this lane
can make without acting:

* The parent is a single function at **`0x822FC508`, 3,428 B**, with **125 `unwind` funclets** spanning
  `0x822FD26C`–`0x822FE728`.
* **All 125 funclets are already pinned to the named unit `VocalTrackDir.cpp`.** They read ORPHAN only
  because the *parent* sits in a pin **gap**.
* The gap is `0x822FC4F8`–`0x822FD26C` (**3,444 B**), and it is **flanked by `VocalTrackDir.cpp` blocks
  on both sides** (`…–0x822FC430` before, `0x822FD26C–…` after).
* **The fit is exact: `0x822FC508 + 0xD64 = 0x822FD26C`.** The gap holds precisely 16 B of EH
  prefix/padding plus that one 3,428 B function — one function, nothing else.
* Spatial corroboration is licensed here: there is **no whole-program optimization**, so `.text` TU
  grouping is preserved and a gap flanked by one unit on both sides is very likely that unit's code.

⇒ Pinning that single gap to `VocalTrackDir.cpp` would flip **125 ORPHAN rows (5,348 B) to HOMED with
no funclet move at all**, and it is an `auto_*`→named **reattribution**, which PIN-NEUTRALITY scopes as
**metric-neutral by construction** (unlike the re-homing this lane did). ⚠ It is nonetheless a
**pin over a 3,428 B function whose identity is inferred from spatial flanking**, so it wants the
usual identification evidence before landing — this lane did **not** verify that the function's
content is VocalTrackDir's. Filed as a recommendation, not a conclusion.

## 7. Gates

Run in order, in the worktree, on the final tree (`3c7bfe38` + this doc).
`native_build_gate.sh` **last**, per the comment-only-commit incident.

| # | gate | rc | result |
|---:|---|---:|---|
| 1 | full `./tools/ninja-locked` | **0** | no-op on the settled tree |
| 2 | `scripts/verify_ruler_agreement.py --check` | **0** | `OK: both objdiff-cli entry points resolve the same ruler.` |
| 3 | `scripts/verify_objs_patched.py --verify-manifest` | **0** | `OK: 1215 decomp, 3110 target objects match 2026-09-15T05:14:15Z (tree_sha256=8608ea7b3d18d56f)` |
| 4 | `tools/icf_alias_finder.py --validate` | **0** | `PASS -- 1404 map-consistent, 247 tolerated, 0 contradicted, 1652 total` |
| 5 | `tools/funclet_homing.py --validate` | **0** | `PASS` — see below |
| 6 | `tools/native_build_gate.sh` | **0** | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` |

Gate 6's summary line is pasted **verbatim** and carries `skipped=0`, which is the rule with the track
record — `PASS` alone is not sufficient (an INCOMPLETE run self-labels but is trivially relayed as
"PASS").

Gate 5 re-derives the lane's own census independently of the measurement path, and is the closing
check on the deliverable:

```
FuncInfos=8541 funclet targets=26321 .text pins=6556
fan-in (funclet -> #FuncInfos): {1: 26321}
verdicts: {'HOMED': 24921, 'MIS-PINNED': 6, 'ORPHAN': 1352, 'UNPINNED-FUNCLET': 42}
extra EH-prefix sites: 537 catch-funclet + 4 folded-parent (expect 537 + 4)
ambiguous FuncInfos: 1 (expect 1: the folded no-action record)
VALIDATE: PASS
```

Note `fan-in {1: 26321}` — every funclet still has exactly one FuncInfo after 700 moves, so no move
merged or split an EH record, and BJ's fan-in==1 finding survives the edit.

## 8. What this lane did NOT do, and why

1. **Did not touch the 6 barred rows** (§5.6 table): `UIColor.cpp`/`UI.cpp` are W16-BK's splits
   surface, `Line.cpp`/`BandCharacter.cpp` are W16-BL's. Bars were matched on **full heading AND
   basename** — deliberately over-broad, so a bar can only over-skip, never under-skip.
2. **Did not act on item 5.** The brief says report only, and §6's band analysis says 72% of it is
   unreachable anyway. §6.1 is filed as a recommendation with its evidence and its one gap named.
3. **Did not add an in-tree caller for the mutation gate.** `~/tmp/bm/mutate_assertions.py` proves all
   9 assertion-sabotage cases fail as they must, but it lives in scratch with **no in-tree caller**, so
   nothing re-runs it and it will rot. Adding a file is outside this lane's surface
   (`splits.txt` `.text` lines, `tools/funclet_homing.py`, this write-up). **Recommendation:** register
   it in `scripts/test_tools.py`'s script arm, the way `sabotage_obj_pairing.py` is.
4. **Did not name or edit a single map row, alias group, or `src/` file.** Out of surface by the brief.
5. **Did not use `merge_touching` on real emitted lines.** It exists only to compute the covered
   **set** for assertion A1; applying it to lines collapses 6,683 → 4,361 by merging pre-existing
   adjacent blocks tree-wide, which would silently change dtk's `.pdata` derivation (one range per
   `.text` **block**). A8 now makes an untouched heading byte-identical so this cannot recur.
6. **Did not hand-edit or hand-carry a single `.pdata` line.** `.pdata` is derived output; the
   committed diffs contain `.pdata` changes **only** because the split re-derived them (batch 2's
   commit: 211 `.text` removed / 187 added by me, 211 `.pdata` removed / 176 re-derived by dtk, and
   **zero heading lines**).
7. **Did not use `run_objdiff` or `ninja <one>.obj` for any measurement.** Both skip the six obj
   patchers, which are part of the ruler. Every number here is from `report.json` after a full build.
8. **Did not re-run the batch-1 measurement with the fixed instrument.** The −1 function shortfall was
   localised by elimination (§5.1) rather than by a re-measure, because re-measuring batch 1 in
   isolation would have required reverting batch 2. The aggregate in §5.5 is measured directly and is
   unaffected by that choice.
