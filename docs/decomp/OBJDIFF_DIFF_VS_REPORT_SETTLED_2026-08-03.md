# `objdiff-cli diff` vs `report generate` — SETTLED (lane EB-4, 2026-08-03)

**Verdict: no defect in either path. The disagreement is TWO things, both named,
both counted, both mechanically eliminable.**

- Lane DR-1's **"64% of rows still disagree at identical config — NOT settled"**
  is a **field-pairing error**, not a path defect. It compared two fields that
  share the word *"normalized"* and measure orthogonal axes. On the correct
  pairing the agreement is **1639/1639 and 1030/1030 — exact to float
  precision, across every stratum.**
- Lane DQ-3's **"up to 14.75 pp"** is real, and is **one config flag**:
  `ppc.calculatePoolRelocations`. The graded ruler sets it `false`; the
  per-function tool leaves the schema default `true`.

Baseline: worktree `laneEB4` off main `2e589b9b`; report regenerated from the
objs on disk with a cold cache (`0 hits, 1998 misses`), reproducing the briefed
baseline **exactly**: `matched_functions 43,848 / matched_code 4,233,200 /
code% 39.603523 / total_functions 69,304 / total_code 10,688,948`.

⚠ **Neither tool was rebuilt and no binary was swapped.** Both legs are the
same live binary (`../objdiff/target/release/objdiff-cli`, fork HEAD `4e932e6`),
reading the same `.obj` files. Any difference is therefore purely code-path or
config — never input skew.

---

## 1. The mechanism

### 1a. "normalized" means two different things (this is the 64%)

| | `objdiff-cli diff` JSON | `report.json` |
|---|---|---|
| **`normalized_match_percent`** | `SymbolDiff::match_percent` computed **with `function_reloc_diffs` applied** | — (no such field) |
| **`raw_match_percent`** | `SymbolDiff::match_percent` recomputed at `NameAddress` | — |
| **`fuzzy_match_percent`** | **an ALIAS of `normalized_match_percent`** | `SymbolDiff::match_percent` |
| **`match_percent_normalized`** | — (**never emitted**) | `SymbolDiff::match_percent_normalized` |

`objdiff-cli/src/cmd/diff.rs:1119-1182` computes the pair by running
`diff_objs` **twice** at two `FunctionRelocDiffs` settings, then assigns
`fuzzy_match_percent: normalized_match_percent` (line 1180) — so in `diff`
output, *"normalized"* means **relocation-normalized** and `fuzzy` is a
duplicate of it.

`report.rs:829-848` reads **two genuinely different fields** off one diff run:
`match_percent` → `fuzzy_match_percent`, and `match_percent_normalized` → the
`mpn` key. Per `objdiff-core/src/diff/code.rs:283-291`, *"normalized"* there
means **arg-penalty-excluded**:

```rust
let normalized_diff_score = diff_score.saturating_sub(diff_state.arg_diff_score).min(max_score);
```

⇒ **`mpn ≥ fuzzy` ALWAYS**, strictly greater whenever any arg-only penalty
(register swap, offset swap) exists. That is why DR-1 saw **"report > diff in
221/221, 0 the other way"**: the sign was not evidence of a report-side bug,
**it was arithmetically forced by the field they picked.**

★ **The lane-relevant consequence:** `matched_functions` — the headline count —
is scored on **`mpn`**, and **`objdiff-cli diff` has no field that reports
`mpn` at all.** The per-function tool is structurally blind to the ruler that
decides whether a row counts as matched. At this commit **221 rows / 102,900 B
have `mpn == 100` while `fuzzy < 100`** (and **0** rows the other way,
confirming the inequality's direction) — these are counted as matched
*functions* while their bytes are withheld from matched *code*. That is the
DB-4 class, re-measured here as a by-product.

### 1b. The residual config delta (this is the 14.75 pp)

`report.rs:406-412` vs `diff.rs:872-875`:

| field | `report generate` | `objdiff-cli diff` |
|---|---|---|
| `function_reloc_diffs` | `None` | `DataValue` |
| `combine_data_sections` | `true` | `false` (schema default) |
| `combine_text_sections` | `true` | `false` (schema default) |
| `ppc_calculate_pool_relocations` | **`false`** | **`true`** (schema default) |

`objdiff.json` sets **no** project- or unit-level `options`, so nothing
reconciles them. Both paths *do* load the same `map_file`
(`build/45410914/icf_aliases.map`, 784 ICF equivalences) into `MappingConfig`
— so the "suspected `symbol_equivalences` wired differently" hypothesis DR-1
left open is **refuted**: `report.rs:450-460` and `diff.rs:884-908` load it
identically, and `diff` gets it whenever `-p <project>` is passed (which every
lane-facing caller does).

`ppc_calculate_pool_relocations` is not cosmetic: `obj/read.rs:697` →
`arch/ppc/mod.rs:191-205` → `generate_fake_pool_relocations_for_function`
**synthesizes fake relocations** by simulating GPR state to resolve pooled
`lis`/`addi` loads into symbol references. Those synthetic relocs become
instruction args, so they change scoring. The graded ruler deliberately turns
this heuristic **off**; the per-function tool leaves it **on**.

---

## 2. The classification — counts, not adjectives

Population: **every** named row in the report, not a sample.
Comparison is always `diff.normalized_match_percent` vs a report field.

### At replicated (`full`) config — all four fields matched to `report generate`

| population | n | disagree vs `report.fuzzy` | disagree vs `report.mpn` |
|---|---|---|---|
| named, `0 < fuzzy < 100` | 1,639 | **0 (0.00%)** | 1,264 (77.12%), **report>diff 1264/1264**, max 7.30 pp |
| wide (400 @100 + 400 @0 + 400 anon + all 221 boundary rows) | 1,030 | **0 (0.00%)** | 246 (23.88%), **report>diff 246/246** |

⇒ **(c) genuine defect in either path: ZERO.** Agreement is exact on 2,669 rows
spanning every stratum — sub-100, at-100, at-0, anonymous `fn_*`, and the
`mpn==100 & fuzzy<100` boundary class.

⇒ **(a) definitional/expected: 1,264 + 246.** This is DR-1's 64% (I measure
**77.12%** of named sub-100 rows have `mpn ≠ fuzzy`; DR-1's 64% came from a
250-row sample with a different band mix). It is not a disagreement between
*paths* — it is one path reporting a field the other never computes.

### At the config lanes actually run

| leg | flags passed to `diff` | disagree vs `report.fuzzy` (of 1,639) | max gap |
|---|---|---|---|
| `full` | all four | **0 (0.00%)** | — |
| `mcp_pool` | `functionRelocDiffs=none` + `ppc.calculatePoolRelocations=false` | **0 (0.00%)** | — |
| `mcp` *(what `run_objdiff` passes today)* | `functionRelocDiffs=none` | **118 (7.20%)** | **14.75 pp** |
| `mcp_ctext` | `…=none` + `combineTextSections=true` | 118 (7.20%) | 14.75 pp |
| `mcp_cdata` | `…=none` + `combineDataSections=true` | 117 (7.14%) | 14.75 pp |
| `default` *(what `diff_inspect.py` / `stack_layout.py` pass — i.e. nothing)* | — | **1,460 (89.08%)** | **16.00 pp** |

⇒ **(b) config-driven and eliminable: 118 of 1,639** on the MCP path.
**`ppc.calculatePoolRelocations=false` alone is necessary AND sufficient** —
it takes 118 → 0 by itself, while `combineTextSections` is **inert** (118 → 118)
and `combineDataSections` moves exactly **one** row (118 → 117).
DQ-3's 14.75 pp is reproduced to the digit on `?MoveBeat@MoveDir@@QBAHXZ`
(report 84.75 / diff 70.00).

### The class that actually costs lanes time

Over **all 20,667 named rows the grader scores at `fuzzy == 100`**, run at the
MCP config: **11 rows (0.053%)** read below 100 in `run_objdiff` —
`?DataVarName@@YAPBDPBVDataNode@@@Z` 97.69, `??0Synchronizable@@QAA@PBD@Z` 98.50,
four `Campaign::Update*MajorLevelIcon` at 98.67–98.70,
`?SetBandName@AppLabel@@QAAXPBVBandProfile@@@Z` 98.75,
`?Terminate@BandHeadShaper@@SAXXZ` 98.82,
`?InqGoalsAcquiredForSong@AccomplishmentManager@@…` 99.05,
`?AllowOverride@BandCharacter@@QAA_NPBD@Z` 99.19,
`?JoypadPollForButton@@YAIH@Z` 99.26.
**A lane grinding any of these is polishing a row that is already graded
complete — and any "fix" that moves it can only regress it.** All 11 vanish
under `ppc.calculatePoolRelocations=false`.

★ **The error is one-directional in every stratum measured: `report ≥ diff`,
0 exceptions in 1,264 + 246 + 11 + 118 rows.** `run_objdiff` never *over*-reports
against the grader — so it cannot make an unfinished row look done. It can only
make a finished row look unfinished.

### Not a disagreement: the null-percent stratum

For target symbols with **`base_size == 0`** (no compiled counterpart — XDK
`D3DXShader`, `xWMA`, etc.) `report` assigns `0.0` while `diff` returns
`normalized_match_percent: null`. 391 of the 1,421 wide-sample rows. This is an
absence, not a conflict, and is excluded from every count above.

---

## 3. Conversion rule (use this; keep using the fast tool)

**To predict the graded `report.json` value from a `run_objdiff` reading:**

1. `diff.normalized_match_percent` **==** `report.fuzzy_match_percent`, **exactly**,
   *provided* `diff` was run with `-c functionRelocDiffs=none -c
   ppc.calculatePoolRelocations=false`. Without the pool flag the reading is a
   **lower bound**, low by up to 14.75 pp on 7.2% of sub-100 rows.
2. `report.match_percent_normalized` (**`mpn` — the ruler `matched_functions`
   counts on**) is **NOT DERIVABLE** from any `diff` output field. `mpn ≥ fuzzy`
   always, so a `run_objdiff` reading is a **lower bound on `mpn` too**, and the
   gap is exactly the arg-only penalty share (up to 7.30 pp measured).
3. ⇒ **A sub-100 `run_objdiff` reading NEVER proves a row is unmatched.**
   A row at `diff.normalized` 97.7–99.3 may already be `fuzzy == 100`
   (11 such rows) or `mpn == 100` (221 rows / 102,900 B). **Confirm against
   `report.json` before spending a lane on "the last 2%".**
4. The safe direction still holds: `diff.normalized == 100` ⇒ `report.fuzzy == 100`
   (0 counter-examples in 22,306 named rows).

---

## 4. Fix landed here (repo-side only — NO binary swap)

`scripts/orchestrator/mcp_server.py`: added
`-c ppc.calculatePoolRelocations=false -c combineDataSections=true -c
combineTextSections=true` alongside the existing `-c functionRelocDiffs=none`
at every lane-facing `objdiff-cli diff` call site, so `run_objdiff` /
`run_diff_inspect` report the **grader's** number. All three are added (not just
the one that fixed the sample) on the principle of *replicating the grader's
config*, and all three together measure **0 disagreements on 2,669 rows**.

⛔ **Deliberately NOT changed — a decision for the coordinator, not this lane:**
`scripts/analysis/diff_inspect.py:1763` and `scripts/analysis/stack_layout.py:1039`
pass **no `-c` at all** and so run at `DataValue` (**89.08% disagreement, max
16.00 pp**). Aligning them is *not* obviously correct: at `DataValue` a wrong
`bl` callee shows as an arg mismatch, whereas `functionRelocDiffs=none` **masks
it** (CLAUDE.md: reloc args are score-invisible; that masking is exactly how
wrong callees hide at 100%). Their percent should be treated as a
**defect-hunting** number, never as the graded score. Changing their ruler would
trade wrong-callee visibility for grader alignment — measure before deciding.

The cleaner long-term fix is in the fork: make `diff.rs:872` default to the same
config as `report.rs:406`. **That requires a rebuild + binary swap and is
therefore pending coordinator approval — this lane swapped nothing.**

---

## 5. Anti-vacuity: the comparator was shown capable of failing

Harness: `/home/free/tmp/laneEB4/eb4_compare.py`.

| control | result |
|---|---|
| **NULL — permuted pairing** (row *i*'s report values vs row *i+1*'s diff values) | **1,626/1,639 disagree (99.21%)**, max gap 98.84 pp — **FIRES** |
| true pairing, same code path | 0/1,639 disagree (0.00%) |
| **sabotage config** (`--leg default`, drop all `-c`) | **1,460/1,639 disagree (89.08%)** — **FIRES** |
| **sabotage config** (`--leg mcp`, drop pool flag only) | **118/1,639 disagree (7.20%)** — **FIRES** |

A comparator that reported "no disagreements" because it was hardcoded, mis-keyed,
or silently dropping rows would fail all four rows of that table; this one passes
the null and the two sabotages while returning exact agreement on the treatment.
Row counts are asserted (`compared=1639`, `errors=0`) so a collapsed join
cannot masquerade as agreement.

## 6. What this lane did NOT do

- Did **not** rebuild or swap any objdiff binary; both legs are the live fork
  binary at HEAD `4e932e6`.
- Did **not** run a ninja build. The report was generated directly from the objs
  on disk, so both instruments read byte-identical inputs by construction. Zero
  contribution to the fleet I/O cap.
- Did **not** measure anonymous `fn_*` rows exhaustively — only the 400 in the
  wide sample (0 disagreements at full config, 1 at MCP config).
- Did **not** re-examine whether the equal-length fix (`4e932e6`) contributes:
  DR-1 already proved it symmetric (`diff_code()` is shared verbatim by both
  subcommands, so it **cannot** produce an asymmetry). Independently corroborated
  here — the residue is fully explained without it.
- Did **not** change `diff_inspect.py` / `stack_layout.py` (see §4).
