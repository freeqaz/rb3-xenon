# ⚠ RULER CHANGE — 2026-08-12: `functionRelocDiffs=name_check` is the SHIPPED grading default

**If you are reading a `matched_code` byte absolute from before 2026-08-12 00:47, it
is on a different ruler and is ~817 kB / 7.9 pp higher than the same tree scores
today. Nothing regressed.**

This is the sibling of [`RULER_CHANGE_2026-08-02.md`](RULER_CHANGE_2026-08-02.md).
That flip was a **disclosure** change (`masked_equal_functions`, score keys
untouched). This one is a **scoring** change: it moves bytes.

## What changed

Commit **`d04c83df`** (2026-08-12 00:47), *"Ship functionRelocDiffs=name_check"*.
`objdiff.json` now carries `options = {"functionRelocDiffs": "name_check"}` and
`objdiff_report_args` is **empty** ⇒ **`name_check` is the default and `none` is
the opt-in.** Recorded by lane **FORK-DIAG** (2026-08-13), which corrected a
CLAUDE.md bullet that had claimed for months that the ninja report edge
hard-codes `functionRelocDiffs=none`.

Under `none`, `reloc_eq` returns true **regardless of the relocation target's
name** — a call to the right address with the wrong name, and a call to the wrong
callee entirely, both scored clean. Under `name_check` the target **name** is
compared, with one carve-out: **placeholder names are forgiven** —
`fn_` / `lbl_` / `jumptable_` / `data_` / `bss_` / `rdata_`, see
`is_placeholder_symbol_name` in objdiff-core `diff/code.rs`. An *unnamed* callee
is therefore already uncharged.

## Measured cost of the ruler alone — zero source change

Lane **FORK-DIAG** / **RECOVER-95K**, 2026-08-13. One binary (`9f6c6c32ae11`),
one tree, `report.cache` wiped between legs — the only variable is the ruler:

| key | `@none` | `@name_check` | Δ |
|---|---:|---:|---:|
| `matched_code` | 4,366,752 B | **3,549,568 B** | **−817,184 B** |
| `matched_code_percent` | 42.31% | **34.39%** | **−7.9 pp** |
| `matched_functions` | 44,252 | 44,252 | **0** |
| `masked_equal` | 22,886 | 22,886 | **0** |

⇒ **ANY byte absolute recorded before 2026-08-12 00:47 is incomparable to one
recorded after it unless the ruler is stated.**

**Why the function count does not move:** `matched_functions` counts rows at
`match_percent_normalized == 100`, and `mpn` **excludes arg-only penalties**;
`none`→`name_check` changes *only* relocation-name **argument** comparison. So
`matched_functions` and `masked_equal` are **ruler-invariant**, while
`matched_code` (which keys on `fuzzy_match_percent == 100`) is not.

⚠ **Mis-attribution hazard, already realised once.** A swing of exactly this
shape was blamed on an objdiff **rebuild** on 2026-08-13. It was the **ruler**,
not the binary. `report.json` self-declares `diff_config`, `tool_commit` and
`tool_binary_hash` in a `provenance` block — **read it, do not infer.**

## Naming economics under `name_check` — asymmetric, three cases

The old rule *"naming costs zero on both rulers"* was a property of `none` and is
**now false** (lane RULER-SWEEP, 2026-08-13).

- **(a) Repairing a WRONG existing map name PAYS.** Lane MAPDEF-3 (`db9eb318`)
  measured **+108 B** from 9 such rows, with the `none` control **unmoved at +0**.
- **(b) NAMING a previously-anonymous address has ZERO call-site upside and REAL
  downside.** The placeholder was already forgiven; naming converts a **forgiven**
  site into a **checked** one — right name = still 0, wrong name = a new charge.
  It still pays via the separate *pairing* channel (+1 honest), which is
  unchanged. **Precedent:** lane MAPID-1 (2026-08-16, `436bfb22`) named
  `0x827bcd38` = `?MemAlloc@@YAPAXHH@Z` for **−1,656 B** — and **6 of 7
  disagreements were REAL wrong-callee bugs the placeholder had been hiding**
  (retail calls one function where our tree spells three: `MemAlloc`,
  `_MemAlloc`, and the TEMP allocator `_MemAllocTemp`). ⇒ **naming is a bet whose
  payout is BUG EXPOSURE, not bytes.**
- **(c) An ALIAS is pure forgiveness and therefore always "pays".** objdiff
  consults `SymbolEquivalences` and drops the charge, so an *unproven* alias lifts
  the score **by construction** — an integrity hazard, not a win.
  ⚠ **The `none` control CANNOT catch a fabricated alias**: `none` ignores
  relocation names, so it reads +0 there by construction. **That flatness is the
  SIGNATURE of the hazard, not a clearance.**
  ★ But **name_check-UP / `none`-FLAT is ALSO the wrong-callee-FIX signature**.
  The two shapes separate only by **patch kind**: map-only ⇒ alias-suspect,
  `source` in the patch ⇒ the most valuable class of real fix we have.
  `ab_measure`'s `control_none_shape()` (`5247b811`) encodes exactly this — do not
  re-derive the guard by hand.

Scale of (c), for context: emptying all 1,493 alias groups moves `matched_code`
by **−720,992 B / 6.985907 pp** with `matched_functions` **exactly +0** (lane
ALIASAUDIT-1, `df90b49f`); 82.51% of those bytes were later **proven on retail
bytes** (lane GROUNDED-1, `f4e26fcc`).

## Two tools were found on the WRONG ruler — in OPPOSITE directions

Lane **MCPRULER-1** (2026-08-14, `7286bfd1`). Both facts follow from one line:
`objdiff-cli diff` applies `objdiff.json`'s `options` block over its own base
config (`diff.rs:953`), and **`-c` args are applied LAST** (`diff.rs:959`).

| tool | what it did | consequence |
|---|---|---|
| `mcp_server.py` | hard-coded `-c functionRelocDiffs=none` | `-c` wins ⇒ **OVERRODE the shipped ruler**. 7,157 rows disagreed; **5,555 rows / 674,936 B** read `fuzzy == 100` under `none` but below 100 graded — rows the orchestrator reported *"Complete — no action needed"* while the grader withheld every byte. |
| `diff_inspect.py`, `stack_layout.py` | passed **no** `-c` | silently **stopped meaning `DataValue`** and started meaning `name_check` on 08-12. They were *already* on the graded ruler; the "deliberately left at `DataValue` so a wrong `bl` shows" property had quietly evaporated (no loss — `name_check` charges a wrong callee by name). |

Both now **resolve the ruler at runtime from `report.json`'s
`provenance.diff_config`** via **`scripts/analysis/ruler.py`**, never hardcoded —
a second hardcoded constant would rot on the same silent schedule. `ruler=graded`
(default) / `none` / `data_value` are explicit opt-ins that change **only**
`functionRelocDiffs`, and **every percentage is labelled with its ruler**.

Verified: `objdiff-cli diff` at graded == `report.json`'s `fuzzy_match_percent`
on **2,617 / 2,617 rows, 0 disagreements**; the same comparison on the OLD
hardcoded ruler **disagrees on 1,332 rows** — i.e. the check *can* fail.

⚠ **The mismatch COUNT is ruler-dependent too, not just the percent**: one row
(`?Handle@OvershellSlot@@`) shows **0 / 2 / 641** charged sites at
`none` / `name_check` / `data_value`, and scores 99.995690 / 99.995690 /
98.044420.

⚠ **Related reading trap that survives the fix:** *"N/N instructions equal"* is
**instruction**-level and does not include relocation-name charges, which are
**argument**-level (`diff_arg`). They coexist with *all instructions equal* — one
row reads "205 instructions | all equal" while scoring **98.4% graded**. **Price
a candidate from `report.json`'s charged-site list, never from a mismatch or
equality count** (lane RESIDUAL-1, `348e3c7b`).

## Conversion rule

**There is none.** Deltas priced on one ruler do not transfer to the other, and
no scalar converts a `@none` byte absolute into a `@name_check` one — the gap is
a property of *which rows* carry relocation-name charges in that particular tree,
not a constant. **Re-measure.** `tools/ab_measure.py`'s same-ruler guard
(`373d17c6`) pins objdiff-cli across both legs and REFUSES on a mid-run swap.

## Provenance

| item | value |
|---|---|
| flip commit | `d04c83df`, 2026-08-12 00:47, *"Ship functionRelocDiffs=name_check"* |
| recorded by | lane FORK-DIAG (2026-08-13), correcting a stale CLAUDE.md bullet |
| ruler cost measured | lanes FORK-DIAG / RECOVER-95K, binary `9f6c6c32ae11`, cache wiped between legs |
| naming economics | lanes RULER-SWEEP (08-13), MAPDEF-3 `db9eb318`, MAPID-1 `436bfb22` (08-16) |
| alias sizing | lane ALIASAUDIT-1 `df90b49f`; provability split lane GROUNDED-1 `f4e26fcc` |
| tool ruler repair | lane MCPRULER-1 `7286bfd1` (08-14); `scripts/analysis/ruler.py` |
| current state | [`CAMPAIGN_STATE_2026-08-17.md`](CAMPAIGN_STATE_2026-08-17.md) — all figures there are `@name_check` |
| predecessor flip | [`RULER_CHANGE_2026-08-02.md`](RULER_CHANGE_2026-08-02.md) — disclosure, not scoring |

**Believe `report.json` for the score, and state the ruler next to every
absolute.**
