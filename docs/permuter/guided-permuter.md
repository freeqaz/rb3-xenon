# Diagnosis-Guided Permuter

The permuter uses objdiff mismatch data to target specific areas of a function instead of blindly generating every possible variant.

**Status**: Implemented (2026-03-03). See [validation session](../sessions/2026-03-03-permuter-bsf-guided-validation.md) for audit details and BSF findings.

## How It Works

### Step 1: Diagnose Baseline

After the scorer builds the baseline and gets the objdiff JSON, it parses the instruction diff through `diff_inspect.parse_breakdowns()` into a `Diagnosis`:

```python
@dataclass
class Diagnosis:
    total_instructions: int
    match_counts: dict[str, int]       # match_type -> count
    reg_swap_pairs: dict[tuple, ...]   # {("r20", "r21"): SwapInfo}
    offset_deltas: dict[int, int]      # {delta: count}
    diff_ops: list[DiffOp]             # real opcode mismatches
    clusters: list[Cluster]            # insert/delete clusters
    noise_explained: int               # diff_arg explained by swaps/offsets/relocs
    noise_total: int
```

This attaches to `FunctionContext.diagnosis` for all patterns to use.

### Step 2: Pattern Filtering

Each pattern has a `relevant(diagnosis)` method that checks whether the diagnosis contains signals it can fix:

| Diagnosis signal | Patterns activated |
|---|---|
| `diff_op` with `cmp*` opcodes | `signed_unsigned`, `comparison_equivalence` |
| `diff_op` with branch opcodes | `signed_unsigned` (zero swap), `branch_polarity` |
| `diff_op` with `fnmsubs`/`fmsubs` | `fma_reorder` |
| Insert/delete clusters around loads | `argument_swap`, `variable_extraction`, `inline_assignment` |
| GPR register swap pairs | `declaration_reorder` |
| All `diff_arg` explained by noise | **skip all patterns** (unfixable) |
| Only register swaps, no other mismatches | only `declaration_reorder` |

This reduces ~60-100 blind variants down to ~5-20 targeted ones.

### Step 3: BSF-Guided Declaration Reorder

When `declaration_reorder` is activated and BSF tracing is available (GDB + wibo + ptrace), it uses the compiler's internal register allocation state to generate targeted swaps instead of blind C(n,2) enumeration.

See [BSF Engine](bsf-engine.md) for the full BSF tracing and color mapping details.

#### Pipeline

1. Trace BSF calls during compilation via GDB breakpoint on `c2.dll!bsf`
2. Extract initial color assignments from the BSF trace
3. Map diagnosed register swap pairs (e.g., `r30 <-> r31`) back to BSF colors via `gpr_to_color()`
4. Map colors to declaration indices via `color_to_decl_idx`
5. Generate only the targeted declaration swaps, plus +-1 neighbor offsets

#### Color-to-GPR Mapping (empirically validated)

Determined via `test_bsf_engine.py` by compiling synthetic functions and correlating BSF traces with assembly register usage:

| Color range | Register range | Formula | Description |
|-------------|---------------|---------|-------------|
| 0-6 | r11-r5 | `reg = 11 - color` | Volatile GPRs |
| 7-25 | r31-r13 | `reg = 38 - color` | Callee-saved GPRs |

Key finding: **color 7 = r31** (first callee-saved), not r4 (volatile). The first declared variable gets color 7 → r31, second gets color 8 → r30, etc.

#### Candidate Generation Strategy

- **Targeted swaps**: For each swap pair `(rA, rB)`, maps to declaration indices, swaps only those
- **Neighbor search**: For each targeted pair `(i, j)`, also tries +-1 index offsets
- **Multi-swap**: When multiple swap pairs exist, applies all simultaneously as an additional candidate
- **Bounded fallback**: For unmapped register pairs, tries only nearby declarations (capped at `2 * len(swap_pairs)`)
- **Unguided fallback**: When BSF tracing fails or `--no-bsf-guided` is set, generates all C(n,2)

### Step 4: Early Skip

Before generating variants, checks if the function is unfixable at source level:
- All mismatches explained by noise (register swaps + offset shifts + symbol relocs)
- No `diff_op`, no clusters → nothing a source pattern can fix
- Exception: if register swap pairs exist, still tries `declaration_reorder`

## Impact

| Metric | Blind mode | Guided mode |
|---|---|---|
| Variants per function | ~60-100 | ~5-20 |
| Build time per function | 60-100 builds | 5-20 builds |
| Register swap coverage | 0% (no targeting) | Targeted via BSF |
| Wasted builds on noise-only | 100% | 0% (early skip) |

## CLI Flags

| Flag | Effect |
|------|--------|
| `--no-guided` | Disable diagnosis; all patterns run on all sites (blind mode) |
| `--no-bsf-guided` | Disable BSF tracing; `declaration_reorder` uses blind C(n,2) |
| `--bsf-required` | Fail with error if BSF tracing fails (instead of falling back) |

## Known Limitations and Future Work

### BSF Per-Function Isolation (IMPLEMENTED)

BSF traces capture ALL functions in a translation unit. For small synthetic test files (1-2 functions), this works well. For real project TUs with many functions, the target function's BSF calls are buried in noise from other functions.

**Solution**: `BSFTrace.partition_by_function(asm_lines)` compiles with `/FAs` to get assembly listings, parses `PROC NEAR`/`ENDP` markers for function boundaries, counts callee-saved registers per function from `__savegprlr_N`/`stmw` patterns, then partitions initial coloring BSF calls by consuming the expected color count per function.

The `_try_bsf_guided()` method in `declaration_reorder.py` automatically compiles with `/FAs`, partitions the trace, and matches the target function by name (exact then fuzzy). Falls back gracefully if partitioning fails.

Validated with 3 partition-specific tests in `test_bsf_engine.py::TestBSFPartitioning` + 3 e2e tests in `test_bsf_pipeline.py`.

### Register Swap Detection (IMPROVED)

`detect_register_swaps()` previously required ALL differing lines to differ ONLY in register names. Lines with mixed register+non-register differences (e.g., different offsets AND swapped registers) were missed entirely.

**Solution**: Added `strict=False` parameter for relaxed mode that extracts register pairs even from lines with mixed differences, requiring 2+ consistent occurrences. Default remains strict for backward compatibility.

### Multi-Way Reordering

Currently only pairwise declaration swaps are attempted. MSVC's graph coloring may require:
- 3-way or N-way permutations (moving a variable "out" of a group)
- Pulling a variable earlier/later rather than swapping with a specific other variable
- The `declaration_movement` pattern partially addresses this but isn't integrated with BSF guidance

### Cross-Function Register Pressure

MSVC processes all functions in a TU sequentially. One function's register usage can affect the coloring of subsequent functions (through callee-saved register save/restore patterns). BSF guidance currently treats each function independently — a unit-level approach could capture these interactions.

### BSF Trace Validation Depth

Validated on synthetic 2-8 variable functions with simple cross-call liveness. Not yet validated on:
- Functions with complex control flow (nested loops, switch statements)
- Functions with many short-lived temporaries (volatile register pressure)
- Functions with FPR (floating-point register) allocation
- The `base` field in BSF calls (register class identification) is observed but not used

## Architecture

```
                    ┌─────────────┐
                    │  __main__.py │  CLI entry point
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  scorer.py   │  Build baseline, run diagnosis
                    └──────┬──────┘
                           │
              ┌────────────▼────────────┐
              │      diagnosis.py        │  Parse objdiff → Diagnosis
              └────────────┬────────────┘
                           │
              ┌────────────▼────────────┐
              │      generator.py        │  Filter patterns, generate variants
              └──┬─────────┬─────────┬──┘
                 │         │         │
         ┌───────▼──┐ ┌───▼───┐ ┌───▼───────────┐
         │ pattern 1 │ │ pat 2 │ │ decl_reorder  │
         └───────────┘ └───────┘ └───────┬───────┘
                                         │ (BSF-guided path)
                                ┌────────▼────────┐
                                │ regmap_solver.py │  Color→GPR mapping
                                └────────┬────────┘
                                         │
                                ┌────────▼────────┐
                                │  bsf_trace.py    │  GDB tracing of c2.dll
                                └─────────────────┘
```

## See Also

- [BSF Engine](bsf-engine.md) — detailed BSF tracing, color mapping, and test suite documentation
- [Permuter INDEX](INDEX.md) — usage docs, CLI reference, pattern catalog
- [Validation session](../sessions/2026-03-03-permuter-bsf-guided-validation.md) — audit results
