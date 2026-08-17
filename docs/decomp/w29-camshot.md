# W29-CAMSHOT — the size-blind `PAIRS` verdict, fixed; and W26's refused lead, shipped at +2,364 B

**2026-08-17, branch `w29-camshot`, worktree `~/tmp/wt-w29camshot`, from `43ac6c43`
on `grounded2-restoration`.** (`grounded2-restoration` was one docs-only commit
ahead at the time; base pinned as briefed.)

Baseline measured in-worktree, never inherited, ruler `name_check` read from
`report.json`'s own `provenance.diff_config`:
**44,505 fns / 3,760,224 B / 36.433937%**, `masked_equal` 22,910, honest 21,595,
`total_code` 10,320,664, `total_functions` 69,226.

**Shipped: +2,364 B / +0.022903pp, `matched_functions` +0.** Commits `ce2df740`
(instrument) and `fe089f83` (fix). Native gate:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

---

## JOB 1 — `cascade_price`'s `PAIRS` verdict checked DEFINITION, never IDENTITY

W26 found this, scoped it out, and landed it with a warning. Reproduced first:
on the 15-row rename the old tool printed
`PAIRS (obj defines the new name)` **15 times out of 15**, including on
`0x822b4298`, where the body it would have selected is **1604 B against retail's
1692 B**.

The verdict was never *wrong about definition*. It was **silent about identity**.
That distinction only bites in the **scatter-include** case — `BandCamShot.cpp`
`#include`s `hamobj/HamCamShot.cpp`, so one obj defines **both** spellings (268
Ham + 281 Band symbols). There a rename does not create a pairing; it **swaps
which body objdiff compares**, and the name resolves either way, so the tool was
green either way.

### The tolerance is CALIBRATED, not chosen — and the first calibration was VACUOUS

A row at `fuzzy == 100` is instruction-for-instruction equal, so its base COMDAT
size **must** equal retail's extent. Every such row is a free null. Measured:

| delta = retail_extent − our_COMDAT_size, at `fuzzy == 100` | rows |
|---|---:|
| −4 | 8 |
| **0** | **18,844** |
| +4 | 222 (mostly `$4PPPPPPPM@A@` vbase adjustor thunks) |
| **≥ 8** | **0** |

Not one provably-matching row disagrees by 8 or more; the ±4 band is 8-byte carve
granularity. So firing at **|delta| > 4** has a measured FP of **0 / 19,074**
while still firing on **557** sub-100 rows. It discriminates.

⛔ **The first calibration run reported "0 disagreements" over ZERO rows.** It
joined report rows (mangled map names) against `symbols.txt` (anonymous
`fn_<addr>`), so nothing matched. It was caught **only** because the script
counted "not reached" alongside the headline — the headline alone read like a
clean pass. Same family as `all([])`, and the same shape as this project's
vacuous `/GS` cookie detector.

⚠ Sizes come from **`coff_bodies_ext` and nothing else**. Lane STLPORT-1 measured
a whole fabricated "+8 B STLport source bug" that was its predecessor billing the
**successor symbol's 8-byte EH prefix** into the function above it. A hand-rolled
COMDAT span would resurrect that bug at exactly the tolerance that matters.

### The negative control was built FIRST, and it goes red

`python3 tools/cascade_price.py selftest --project-dir <wt>`

* **GREEN** — 0 fires over 19,074 rows at `fuzzy == 100`.
* **RED** — fires on **557/557** rows whose |delta| > 4.
* **VACUITY GUARD** — refuses (exit 2) below 1,000 green / 1 red rather than
  passing on an empty population.
* **`--self-break`** disables the check and asserts the selftest **FAILS**:
  measured **0/557**. A repair whose test cannot go red is worth nothing.

★ The self-break selects its population with a **frozen** constant, never the
mutable one under test — otherwise it would empty the red set and trip the
*vacuity refusal* instead of producing the red it exists to demonstrate. A
self-break that cannot break is the same disease one level up.

Regression: the frozen **W17 known-answer fixture still passes 4/4 exactly**
(−580 total).

### ⚠ A SECOND instance of the same defect, found but NOT fixed

`price_row` computes a LOCAL row's `CLEARED`/`NEW_CHARGE`/`PERSISTS` verdicts
against the **pre-swap** base body. In the scatter-include case objdiff will pair
the **post-swap** body, whose callees are already Band-spelled, so those verdicts
are computed against the wrong body.

⚠ **And on the one cell where I disputed it, the tool was RIGHT and I was WRONG**
(`0x822b5ef0`, `FALLS −396` — it fell). So this is a real modelling gap but its
sign is not predictable by hand; do not "correct" it without a fixture. The new
`SIZE_MISMATCH` verdict is the *detector* for the condition under which the local
charge model is unreliable, which is the cheap half of the fix.

---

## JOB 2 — BandCamShot / HamCamShot: W26's refusal, re-adjudicated

### Re-verified, not inherited — every W26 figure reproduces

| probe | W26 | W29 |
|---|---:|---:|
| `.?AVBandCamShot@@` / `.?AVHamCamShot@@` in retail | 1 / 0 | **1 / 0** |
| ASCII `HamCamShot` in the 14.36 MB binary | 0 | **0** |
| controls `.?AVBandDirector@@` / `.?AVFaderGroup@@` | 1 / 0 | **1 / 0** |
| Ham-spelled map rows / bytes | 15 / 3,600 | **15 / 3,600** |
| of those at `fuzzy == 100` | 9 / 1,308 B | **9 / 1,308 B** |
| duplicate-name collisions | 4 | **4** (same addresses) |
| cascade GAIN, genuine vs alias-capture | 1,196 / 576 | **1,196 / 576** |

⚠ All binary probes run in **Python**, never `grep` — the shell's `grep` is a
function routing through `ugrep -I` and yields false negatives shaped like
decisive ones.

### Correction 1 — the divergence is ONE FUNCTION, not the family

W26 read `0x822b4298`'s 1604-vs-1692 as *"our two class definitions genuinely
diverge (~88 B)"* and concluded the family could not be renamed coherently.
Re-measured on the EH-fixed reader, **exactly 1 of 15 rows diverges**; the other
14 are byte-size-identical under both spellings.

The cause is one missing property — `BEGIN_CUSTOM_PROPSYNC(BandCamShot::Target)`
lacked **`SYNC_PROP(to, o.mXfm)`**, which the Ham spelling of the same block has
as its entry #2. Adding it moved our Band COMDAT **1604 B/180 relocs → 1692
B/194** — *predicted exactly*, and identical to both the Ham spelling and retail.

⚠ Adjudicated on **retail bytes**, not on a preference between oracles: our
Ham-spelled COMDAT is 1692 B/194 relocs and scores `fuzzy` 99.9882 against the
target with exactly **one** charged site (a callee NAME). Retail's
`BandCamShot::Target` PropSync *does* sync `to`; ours did not. That is a real
behavioural bug regardless of the metric.

### Correction 2 — the rename is a BODY SWAP, not a re-pairing

The charged sites are **not on the renamed addresses**. They are on **callee
spellings inside the bodies**, where the map already says `BandCamShot` and our
body — compiled from `HamCamShot.cpp` through the scatter-include — says
`HamCamShot`:

| row | charge |
|---|---|
| `PropSync` 1692 B | T `UpdateTarget@Target@BandCamShot` vs B `…HamCamShot` |
| `GetNumShots` 140 B | T `ListNextShots@BandCamShot` vs B `…HamCamShot` |
| `GetTotalDurationSeconds` 172 B | same callee |
| `SyncProperty` thunk 12 B | T `SyncProperty@BandCamShot` vs B `…HamCamShot` |

⇒ **the map was internally inconsistent with itself**, naming 15 addresses
`HamCamShot` while naming their own callees `BandCamShot`. Renaming makes objdiff
pair the Band-spelled COMDAT, which calls Band-spelled callees. This is why Job
1's size check is load-bearing here rather than a nicety.

### Measured in three legs so the channels are ATTRIBUTED, not asserted

| run | patch | graded Δ | `none` Δ |
|---|---|---:|---:|
| 1 | source only | **+0 B** | +0 |
| 2 | map only, 11 rows | **+672 B** | **−1,692** |
| 3 | source + map | **+2,364 B** | **+0** |

★ Run 2's `none = −1,692` was **pre-registered to the byte**: `none` ignores the
name charge, so `PropSync` earns its 1,692 B there *today*, and the rename swapped
it onto the un-fixed 1604 B body. The **+1,692 swing from run 2 to run 3 is the
source fix alone.** That is the body swap measured on the ruler that ignores names.

★ `ALIAS_SUSPECT` **could** fire on run 2 (map-only) and did not — `none` moved,
so the guard classified it `REAL_PAIRING`. The map-only leg was run *specifically*
so the guard would be live rather than masked behind a source change.

⚠ `matched_functions` is **+0** across +2,364 B: every charge is an arg-only
relocation-name diff, which `mpn` excludes. Textbook "bytes move with Δfunctions
= 0".

### The prediction that MISSED, which is the useful part

Run 2 was pre-registered at **+1,244** and measured **+672**. The row-level diff
closes exactly (gains 1,992 / losses 1,320):

| channel | bytes |
|---|---:|
| `GetNumShots` + `GetTotalDurationSeconds` + `SyncProperty` thunk crossed | +324 |
| cascade: `SyncProperty@BandCamShot` +848, `push_back` +108, `operator>>` +84 | +1,040 |
| `_Destroy` rename → 3 × `fn_` rows fall at −40 | −120 |
| ⛔ `??$PropSync@UTarget@…` `0x822b5ef0` **fell** | −396 |
| ⛔ `resize@ObjList<Target>` `0x822b6720` | −68 |
| ⛔ `??4?$ObjList@…Ham` — an **excluded** collision row | −108 |

Three lessons, all bought with a wrong number:

1. **I disputed `cascade_price`'s `FALLS −396` and it was right.** My reasoning
   ("the swapped body calls Band callees, so no new charge") was sound for the
   rows I checked and wrong for this one.
2. **A partial rename charges you for the rows you excluded.** `0x822b7ac8` kept
   its Ham name (collision) and fell −108 because I renamed its callee out from
   under it. W26 predicted exactly this; it is now measured.
3. **The 7 rows already at 100 wash** — the Ham row vanishes, a Band row appears
   at 100. That channel is invisible to a "which rows cross" model and has to be
   counted as vanish + appear.

### REFUSED — the 4 duplicate-name collisions (492 B), upgraded to a retail-byte fact

W26 refused these on a duplicate-key rule. Adjudicated on bytes instead: all four
pairs are **shape-identical but call DIFFERENT absolute targets** — each half
calls its own sibling family (`0x822b*` BandCamShot cluster vs
`0x824c*`/`0x824d*` PanelDir cluster).

⚠ A masked `bl` comparison says "identical" for all four and is the WRONG
instrument — masking the displacement destroys exactly the information that
decides it. Under ICF, two addresses surviving *means* the bodies are not
reloc-identical, and here the absolute targets show why. They are genuinely two
instantiations, so **one symbol cannot serve both**.

★ Excluding them also removes W26's **+576 B alias-group-85 capture
automatically**, because both capture rows depended on renaming `0x822b7d30`.
The integrity hazard did not have to be argued away — the collision rule drops it.

### REFUSED — the residual 464 B is an ALIAS-COVERAGE GAP, not a source defect

`0x822b5ef0` (−396) and `0x822b6720` (−68) both call an **ICF survivor whose
alias group folds the Ham spelling but not the Band one**
(`insert@list<BandCamShot*>` at `0x822b55e0` = group 85;
`resize@list<BitmapOverride@WorldDir>`).

★ **Note the inversion, which is the durable finding:** retail has **no**
`HamCamShot`, so those groups encode the **wrong spelling**, and the *Ham*
membership is the questionable one — not the Band one that is missing. Once the
map says Band, 9 alias groups whose **survivor is a Ham spelling** are stale.

Closing the gap is an **alias edit**, and adding forgiveness lifts the score by
construction, so it is refused here. ⛔ The `none` control cannot clear it: `none`
ignores relocation names and reads +0 by definition. What would settle it is
relocation-normalized body identity **with target names compared** between our
two COMDATs — cheap, and a well-specified next lane.

★ And this is a live demonstration of the caveat written into Job 1's fix: **a
size test is NECESSARY, NOT SUFFICIENT.** Both rows passed the size check and
still failed to reach 100.

---

## What I did NOT do, and why

* **No alias added or withdrawn.** The 464 B above and W26's +576 B both sit
  behind forgiveness edits.
* **The 4 collision rows are not renamed** and cannot be without splitting the
  symbol — the −108 they cost is the accepted price of excluding them.
* **`cascade_price`'s local-row charge model was NOT rewritten** (see Job 1's
  second defect). It needs a fixture first, and the one cell I hand-adjudicated I
  got wrong.
* **`HamCamShot.cpp` was not deleted and the scatter-include was not dropped.**
  W26 proposed that as the endpoint. It is now *possible* — the two `PropSync`
  bodies are byte-identical in size — but it would restructure a 33 KB TU for
  bytes already collected, and the 4 collisions still need Ham-spelled
  instantiations to exist somewhere.

## Reproducing

```bash
python3 tools/cascade_price.py selftest --project-dir <wt>               # 0 FP / 557 red
python3 tools/cascade_price.py selftest --project-dir <wt> --self-break  # proves it goes RED
python3 tools/cascade_price.py validate --project-dir <wt>               # W17 fixture, 4/4
python3 tools/cascade_price.py price --project-dir <wt> --edit-file <edits.json>
python3 tools/ab_measure.py --worktree <wt> --from-dirty
```
