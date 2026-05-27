# BSF Engine — Compiler Register Allocation Tracing

The BSF engine traces the MSVC PPC cross-compiler's internal register allocator to understand how variables map to physical registers. This enables targeted declaration reordering instead of blind permutation.

## Background

MSVC's `c2.dll` code generator uses graph coloring for register allocation. The `BSF` (Bit Scan Forward) x86 instruction is used to pick the lowest available color from an availability bitmask. By tracing every BSF call during compilation, we can observe:

- Which colors are assigned to which variables
- The order of color assignment (which corresponds to declaration order)
- How the availability mask changes as colors are consumed

## Architecture

```
tools/compiler_trace/
├── bsf_trace.py        # Core: GDB-based BSF call tracing + per-function partitioning
├── bsf_diff.py         # Diff two BSF traces to find divergences
├── asm_diff.py         # Extract functions from ASM listings, detect register swaps (strict/relaxed)
├── regmap_solver.py    # Color→GPR mapping, guided swap candidate generation (isolation-aware)
├── invoker.py          # CompilerInvoker: wibo + cl.exe with project flags
├── gdb_script.py       # GDB script generation for BSF breakpoints
└── tests/
    ├── test_bsf_engine.py      # 30 integration tests (incl. 3 partition tests)
    └── test_bsf_pipeline.py    # 3 end-to-end pipeline tests
```

## How BSF Tracing Works

1. **GDB script**: Sets a breakpoint on the BSF instruction inside `c2.dll` (RVA `0x026780`)
2. **wibo**: Runs the MSVC PPC cross-compiler under Linux via wibo (a Windows PE loader)
3. **Capture**: At each BSF hit, records: caller RVA, availability mask (lo/hi), base register class, and result bit (color assigned)
4. **BSFTrace**: Collects all calls into a `BSFTrace` object with phase-filtering capabilities

### Caller RVAs (Allocation Phases)

| RVA | Phase | Description |
|-----|-------|-------------|
| `0x027242` | Initial coloring | First pass — assigns colors to variables in symbol ID order |
| `0x026B5E` | Coalescing | Merges move-related variables to reduce copies |
| `0x0272E8` | Recoloring | Second pass — reassigns after coalescing changes |

The initial coloring phase is most relevant for declaration reordering since it processes variables in declaration order.

## Color-to-GPR Mapping

Empirically determined by compiling synthetic functions and correlating BSF traces with assembly register usage (`test_bsf_engine.py::TestColorToRegisterMapping`):

### Callee-Saved GPRs (most relevant for decomp)

| Color | Register | Variable (in 5-var test) |
|-------|----------|-------------------------|
| 7 | r31 | 1st declared |
| 8 | r30 | 2nd declared |
| 9 | r29 | 3rd declared |
| 10 | r28 | 4th declared |
| 11 | r27 | 5th declared |
| ... | ... | ... |
| 25 | r13 | 19th declared |

**Formula**: `register_number = 38 - color`

### Volatile GPRs

| Color | Register |
|-------|----------|
| 0 | r11 |
| 1 | r10 |
| 2 | r9 |
| 3 | r8 |
| 4 | r7 |
| 5 | r6 |
| 6 | r5 |

**Formula**: `register_number = 11 - color`

### Important Notes

- **Color 7 is callee-saved (r31)**, not volatile (r4). This was the key bug in the original implementation.
- Colors ≥20 may appear for FPR or other register classes (distinguished by the `base` field, not color alone)
- Colors 32+ in BSF traces with base=0 may be FP register colors in the same allocation pass

## Guided Swap Generation

Given a diagnosed register swap pair (e.g., `r30 <-> r31`):

1. `gpr_to_color("r30")` → color 8, `gpr_to_color("r31")` → color 7
2. Look up which declaration indices were assigned those colors in the BSF trace
3. Generate a variant that swaps those two declarations
4. Also try +-1 neighbor indices for near-miss coverage
5. If multiple swap pairs, try applying all simultaneously

This is implemented in `regmap_solver.guided_pairwise_search()`.

## Per-Function Isolation

BSF traces capture all functions in a TU. `BSFTrace.partition_by_function(asm_lines)` splits the trace:

1. Compiles with `/FAs` to get MSVC assembly listing
2. Parses `PROC NEAR`/`ENDP` markers for function names in source order
3. Counts callee-saved registers per function from `__savegprlr_N`/`stmw` patterns
4. Walks initial coloring BSF calls, consuming N distinct colors per function

The permuter's `_try_bsf_guided()` calls this automatically and passes only the target function's BSF calls to `guided_pairwise_search()`.

## Test Suite

30 integration tests in `tools/compiler_trace/tests/test_bsf_engine.py` + 3 e2e pipeline tests:

| Test Class | Count | What It Validates |
|------------|-------|-------------------|
| `TestBSFTraceDeterminism` | 1 | Same source → identical trace |
| `TestBSFTraceSensitivity` | 6 | Declaration swaps change BSF traces |
| `TestBSFTraceScope` | 2 | Multi-function TU behavior |
| `TestAsmRegisterAssignment` | 5 | Declaration order → register assignment |
| `TestColorToRegisterMapping` | 2 | Empirical color↔register correlation |
| `TestBSFTraceStructure` | 4 | Trace structure analysis (RVAs, masks, patterns) |
| `TestRealProjectSource` | 3 | Real project .cpp files |
| `TestCurrentMappingAccuracy` | 4 | Mapping correctness + round-trip |
| `TestBSFPartitioning` | 3 | Per-function partition correctness |

**End-to-end pipeline tests** (`test_bsf_pipeline.py`):

| Test | What It Validates |
|------|-------------------|
| `test_2var_swap_recovery` | Guided search finds original order from swapped source |
| `test_5var_targeted_narrowing` | Guided candidates fewer than blind C(n,2) |
| `test_multi_function_isolation_e2e` | Partition isolates target function, guided search works on isolated trace |

### Requirements

- **wibo**: 32-bit debug build (for GDB tracing). Path: `build/tools/wibo`
- **MSVC PPC cross-compiler**: `build/compilers/X360/16.00.11886.00/cl.exe`
- **GDB**: With ptrace access (not available inside sandbox)

### Running

```bash
# All BSF engine tests
python -m pytest tools/compiler_trace/tests/test_bsf_engine.py -v

# Just sensitivity tests
python -m pytest tools/compiler_trace/tests/test_bsf_engine.py -v -k "sensitivity"

# Just mapping validation
python -m pytest tools/compiler_trace/tests/test_bsf_engine.py -v -k "mapping"

# With captured output (see register assignments, color correlations)
python -m pytest tools/compiler_trace/tests/test_bsf_engine.py -v -s
```

## Key Findings

### What Works

- BSF traces are **deterministic** (same source → identical trace every time)
- BSF traces are **sensitive** to declaration order (all 6 sensitivity tests pass on synthetic sources)
- Declaration order directly controls register assignment: 1st declared → r31, 2nd → r30, etc.
- BSF traces are **per-TU** — adding functions changes call count and sequence

### Known Limitations

1. ~~**Per-function isolation**~~: **FIXED** — `BSFTrace.partition_by_function()` splits traces using assembly listing analysis. Validated with 3 partition tests + 3 e2e pipeline tests.

2. ~~**`detect_register_swaps()` gaps**~~: **IMPROVED** — Added `strict=False` relaxed mode that extracts register pairs from lines with mixed register+non-register differences, requiring 2+ consistent occurrences.

3. **Only pairwise swaps**: Multi-way reordering (3+ variable permutations) not yet supported, though the BSF data could inform these.

4. **FPR mapping unknown**: Floating-point register color mapping not yet characterized. The `base` field differentiates register classes but isn't used for targeting.

5. **Sandbox restriction**: BSF tracing requires GDB + ptrace, which is blocked by the default sandbox. Must run with `dangerouslyDisableSandbox: true` or outside the sandbox.

## See Also

- [Guided Permuter](guided-permuter.md) — how BSF integrates with the permuter pipeline
- [Validation session](../sessions/2026-03-03-permuter-bsf-guided-validation.md) — audit and empirical results
