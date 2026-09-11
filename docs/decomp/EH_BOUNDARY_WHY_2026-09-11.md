# Why `obj_eh_boundary_patcher` went inert — it was SUPERSEDED, not broken

**Lane W4-E, 2026-09-11.** Base `3ab3f494`, branch `w4-eh-boundary-why`,
worktree `~/tmp/wt-w4-e`.

Lane PATCH-LIVE (`c0dd8a84`) established **that** the pass is inert — 6,395
boundaries written across 577 files, 0 of 69,219 report rows moved — and
explicitly refused to retire it on that aggregate zero, because *"huge activity,
zero value" is the shape of a measurement error*. This lane establishes **why**.

## Verdict

**The pass is redundant, not broken. It still works perfectly; objdiff simply
started doing the same job itself.**

> `objdiff` commit **`b76f376`, 2026-09-01 09:27**, *"Stop charging the MSVC EH
> funclet prefix to the preceding function"* — merged as `0321226`, and the
> **deployed fleet binary at `/home/free/code/milohax/objdiff/target/release/objdiff-cli`
> was rebuilt from it at 09:40 the same day.**

`report.json`'s own `provenance.tool_commit` on this tree reads `032122696555`,
so the grading run that measured the zero was produced by a binary carrying the
fix. The commit message even names our patcher in its blast-radius section and
predicts the zero:

> *rb3-xenon (MSVC PPC COFF, same flags): 0 change across 69,219 functions, no
> measure moved. It is unaffected because its own post-compile pass
> (`scripts/obj_eh_boundary_patcher.py`, 2026-08-02) already plants a `$EH00000`
> boundary symbol at all 6,138 of its funclet prefixes — an independent
> derivation of the same mechanism.*

⛔ **Both suspects named in the brief are refuted, by date and by mechanism.**
The `name_check` ruler flip (`d04c83df`, 2026-08-12) and objdiff 4.2.8
(`87cc042`, 2026-08-22) both **predate** the cause by 10 and 20 days. Neither
touches symbol-extent inference. The window between the pass's `+193` and
PATCH-LIVE's `0` contained exactly one relevant change, and it was not either of
them.

## The mechanism, on both sides

MSVC X360 lays an EH-enabled function out inside its `.text` COMDAT as:

```
+0x000  .long __CxxFrameHandler          <- 8-byte EH prefix
+0x004  .long __ehfuncinfo$?Func@@...
+0x008  ?Func@@...                       <- the function symbol
  ...   body ...
+0x1a4  .long __CxxFrameHandler          <- EH prefix for the FUNCLET
+0x1a8  .long __ehfuncinfo$?Func@@...
+0x1ac  __catch$92771                    <- catch funclet
```

The function's true end is `0x1a4`, but MSVC marks it only with a **class-6
`IMAGE_SYM_CLASS_LABEL` `$M#####`**, which `infer_symbol_sizes` correctly
declines to treat as a boundary. The extent therefore runs on to `__catch$` at
`0x1ac` and swallows the funclet's 8-byte prefix, which is disassembled as two
trailing `<illegal>` instructions. dtk gives the **target** side an
`except_data_<addr>` symbol there, so only our side was ever wrong — an
*attribution* artifact, not a codegen difference.

Two independent implementations now fix it:

| | `scripts/obj_eh_boundary_patcher.py` (2026-08-02) | objdiff `b76f376` (2026-09-01) |
|---|---|---|
| where | post-compile pass over our `.obj` | `infer_symbol_sizes` in `objdiff-core/src/obj/read.rs` |
| how | appends a class-3 `$EH#####` symbol at the prefix | backs `next_address` off by 8 |
| handler reloc | `== "__CxxFrameHandler"` | `starts_with("__CxxFrameHandler")` |
| funcinfo reloc | `starts_with("__ehfuncinfo$")` | same |
| 8 bytes must be zero | **not checked** | **required** |
| position | any site without a class-2/3 symbol | only at `next_address - 8`, **once**, not looped |

The two firing conditions are **not identical**, so residual coverage was
conceivable. It is not merely conceivable-and-absent; it is measured absent.

## Residual census — `tools/eh_boundary_probe.py census`

Population = exactly the sites the pass acts on
(`find_boundaries(stripped object)`), classified against the symbol table
objdiff would see **in the pass's absence**:

```
objects scanned: 1205 (577 carry >=1 prefix site)
sites in the pass population: 6395
  BOTH_COVER      6395  100.00%
  NONZERO            0    0.00%   <- objdiff declines (8 bytes not zero)
  ADJACENT           0    0.00%   <- two prefixes in a row; single back-off
  ORPHAN             0    0.00%   <- no symbol at X+8; objdiff never looks
RESIDUAL (patcher fires, objdiff's native back-off does not): 0
```

577 files / 6,395 sites **reproduces PATCH-LIVE's activity figures exactly**
from an independent instrument. The pass's population is not a subset of
objdiff's — it is **equal** to it.

⚠ The first version of this census was wrong and its control was vacuous: it
keyed on every prefix site per file, which counts each function's *own* leading
prefix at COMDAT offset 0 where the pass deliberately does not fire; and the
round-trip control then picked `Main.obj`, which has **no** planted `$EH`, so
strip removed 0 symbols and "byte-identical to the original" was true no matter
what strip did. It printed `SELFTEST PASS`. Fixed, and the control now requires
`removed > 0`.

## The 166 rows, then and now

★ **The "+166" in the docstring was never a measurement.** It was CM-3's
predicted *ceiling*; the measured value in `5f05def4` was **+193**, and the
prediction was a strict lower bound with 100% precision (all 166 gained, plus 27
multi-funclet functions the ±8 filter had excluded by construction).

Today that population still exists and is still defended — **by objdiff instead
of by the pass**:

| | rows leaving `fuzzy == 100` when the pass is ablated | bytes |
|---|---|---|
| 2026-08-02, objdiff pre-fix (CM-3, as `+193`) | 193 | 33,624 |
| 2026-09-11, objdiff **pre-fix** (this lane) | **146** | **22,844** |
| 2026-09-11, objdiff **live** (this lane) | **0** | **0** |

The rows are the same shape: **145 of 146 are STLport template instantiations**
(`?_M_insert_overflow_aux@?$vector@...`, `_Rb_tree::_M_copy`), spread over 82
units, headed by `MetaPerformer` 436 B, `ClipDistMap` 412 B, `CharClip` 408 B.
`ClipDistMap`'s `_M_insert_overflow_aux<vector<Vector3>>` at boundary `0x1a4` is
**CM-3's own positive control, still present at the same offset**. The drift
193 → 146 is ordinary population drift across 40 days of pins, ports and a ruler
flip, not decay of the mechanism.

★ The aggregate `−147 functions / −22,844 B` and the row count `146` reconcile
exactly, and the gap is the documented ruler split: **147 rows leave
`mpn == 100`** (what `matched_functions` counts) while **146 leave
`fuzzy == 100`** (what `matched_code` counts). The extra row —
`default/MeshAnim ??$_M_allocate_and_copy@...`, 100 B — was already at
`fuzzy 99.8`, so it moves a function and no bytes.

## The discriminating swap

Four cells, one variable each, `--no-cache` on every leg, same tree.
"Stripped" = the pass's `$EH` symbols removed from the built objects with
mtimes preserved — an ablation with **no recompile and no stamp touched**,
licensed by control 1 (strip is the *exact byte inverse* of the pass).

**Whole binary:**

| objdiff | pass ON | pass ABLATED | Δ |
|---|---|---|---|
| `b76f376` (**live**, `0321226`) | 42,505 / 3,834,712 B / 37.426590% | 42,505 / 3,834,712 B / 37.426590% | **0 / 0 / 0.000000** |
| `b76f376^` (pre-fix probe) | 42,505 / 3,834,712 B / 37.426590% | 42,358 / 3,811,868 B / 37.203632% | **−147 / −22,844 B / −0.222958 pp** |

Per row: **0 of 69,219 differ** on live (reproducing PATCH-LIVE's per-row
finding independently); **270 differ** on pre-fix.

**Single row** (`default/ClipDistMap`, CM-3's control, `-1`/`-2` against
untouchable copies outside the build tree, `$EH` count printed for the exact
files diffed — 13 vs 0):

| objdiff | pass ON | pass ABLATED |
|---|---|---|
| `b76f376^` | 103 instrs / 99.8544% | **105 instrs / 97.9126%** ← the two `<illegal>` words return |
| `b76f376` | 103 instrs / 99.8544% | 103 instrs / 99.8544% |
| **live fleet** | 103 instrs / 99.8544% | 103 instrs / 99.8544% |

⇒ **`live ≡ POSTFIX` confirms the probe reproduces the fleet binary**, so the
`PREFIX` cell's difference is attributable to that single commit and nothing
else. **The swap that makes the pass live again is reverting objdiff to
`b76f376^`.** This is a discriminating swap, not a hypothesis: it could have
failed (pre-fix + ablated reading 0 would have refuted the whole account) and it
did not.

★ Leg 1 (`POSTFIX × patched`, `--no-cache`) reproduces the ninja-generated
`report.json` **to the last digit**, which is the control proving the rig
measures what the grader measures.

⚠ Note the row reads `99.8544%` with **"103 instructions, all equal"** — the
documented `diff_arg` trap: relocation-name charges are argument-level and
coexist with every instruction equal. `report.json` scores it 100 via alias
forgiveness. Irrelevant here, because the measurement is the *difference between
cells*, but do not read `99.8544` as a defect.

## Cost, measured on this tree (not inherited)

PATCH-LIVE quoted **23 s**, a single sample at loadavg 90. Re-measured here:

| | elapsed |
|---|---|
| `--batch --apply` on a settled tree (0 files patched) | **8 s** |
| `--batch --apply` with real work (577 files, 6,395 boundaries) | **15 s** |

The pass rescans all 1,205 objects even with nothing to do, and
`scripts/verify_objs_patched.py --check` **dry-runs it a second time**
(`verify_objs_patched.py:213`), so a full build pays the scan twice. Retirement
saves roughly **15 s + 8 s ≈ 23 s per full build** — PATCH-LIVE's figure, but
reached as a composite rather than a single pass's cost.

## Recommendation: RETIRE — **conditional on landing the guard first**

Retire `obj_eh_boundary_patcher` from `config.custom_build_steps`, because:

1. objdiff implements the identical mechanism natively and has since 2026-09-01;
2. the residual census is **0 of 6,395** — no class of site is covered by the
   pass and not by objdiff;
3. the pass costs ~23 s of every full build for measured zero value;
4. its own upstream author derived the mechanism independently and verified the
   two compose without over-trimming.

⛔ **But the redundancy is CONDITIONAL, and the condition is a hand-swapped
prebuilt binary.** `objdiff-cli` here is shared with `../rb3` and
`../dc3-decomp`; CLAUDE.md documents fleet swaps and rollbacks as routine, and
records a swap being mis-attributed once already. Retire the pass without a
guard and a rollback past `b76f376` silently costs **147 functions and
22,844 bytes with nothing failing** — the score just reads lower.

`tools/check_objdiff_eh_prefix.py` closes that. It asserts the **redundancy
property itself** (strip the pass from a witness object; require objdiff to
report the same thing) rather than a proxy like a version string, resolves the
binary from `build.ninja`'s `rule report`, and treats vacuity as an outcome
(`rc=5`), not a pass. **Its red leg was run:**

```
--objdiff <live 0321226>   50 instrs ON, 50 OFF, 100.0000% both  -> rc=0 PASS
--objdiff <pre-fix probe>  50 instrs ON, 52 OFF,  96.0000% OFF   -> rc=2 FAIL
```

It selects its own witness (`default/RockCentral` `_Rb_tree<String,…>::_M_copy`)
— a *different* function from the one this lane diffed by hand, so the mechanism
is replicated rather than restated.

⚠ **The CLAUDE.md precedent against pruning currently-zero classes does not
apply here, and the distinction matters.** That precedent (`a745039e`, +94,616 B
to reverse) concerns classes that are zero *today* and **go live as porting
advances**. This zero has a different cause: a second implementation now covers
100% of the population. The risk is not growth, it is **regression of the other
implementation** — which is exactly what the guard detects and a blanket "never
prune" would not.

## What I did NOT do

- **Nothing was deleted or unwired.** `configure.py` is untouched, per the
  brief: the build-graph change belongs to the peer PATCH-LIVE lane that owns
  it. This lane supplies the reason and the guard.
- **The guard is not wired into `build.ninja`** — same reason; it should land
  with the retirement, most naturally as an implicit of the report edge or a
  check inside `verify_objs_patched.py`.
- **I did not re-derive CM-3's original 166-row list.** It was a prediction, not
  an artifact, and no list was committed; I reconstructed the *population* by
  ablation instead, which is stronger (it is the rows that actually move).
- **I did not touch `bool_mangle` / `atexit_scope`**, the other retirement
  candidates PATCH-LIVE flagged. Their zero has neither of this one's two
  explanations and should not inherit this verdict.
- **I did not rebuild objdiff in `../objdiff`.** Both probe binaries were built
  from a clone at `~/tmp/objdiff-probe-src` with
  `CARGO_TARGET_DIR=~/tmp/objdiff-probe`; the fleet binary and
  `../objdiff/target/release/` were never written.
- **I did not run the native gate** — this lane adds two `tools/` scripts and a
  doc, and touches no `src/` file, so it cannot affect the native link.
- **No A/B via `ab_measure`.** Its legs recompile; this ablation deliberately
  does not, because the treatment lives in already-compiled objects and a
  recompile would reapply the pass. The rig's control is leg 1 reproducing
  `report.json` exactly.

## Reproduction

```bash
scripts/setup_worktree.sh ~/tmp/wt-w4-e w4-eh-boundary-why && cd ~/tmp/wt-w4-e
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_w4e.log

python3 tools/eh_boundary_probe.py selftest      # 3 controls, all can fail
python3 tools/eh_boundary_probe.py census        # residual: 0 of 6,395
python3 tools/check_objdiff_eh_prefix.py         # rc=0 on the live binary

# whole-binary ablation (no recompile); restore is verified, not assumed
python3 tools/eh_boundary_probe.py strip-tree
<objdiff-cli> report generate --no-cache -o /tmp/ablated.json
python3 scripts/obj_eh_boundary_patcher.py --batch --apply
python3 tools/eh_boundary_probe.py verify-tree   # 0 of 1205 objects differ
```

Provenance: tree `3ab3f494` + this branch, full `./tools/ninja-locked` (rc=0),
ruler `name_check` (`report.json` `provenance.tool_commit 032122696555`,
`tool_binary_hash 14ac591a0814e6c9`), baseline
**42,505 / 3,834,712 B / 37.426590% / fuzzy 48.995964 / 69,219 functions**.
