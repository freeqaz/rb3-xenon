---
name: stack-layout
description: Compare stack frame layouts between target and current compilation. Shows which source variables occupy which stack slots, identifies swaps, and diagnoses frame size mismatches. Use when offset mismatches dominate a function's diff.
argument-hint: "<symbol_name>"
allowed-tools: Bash(python3 scripts/analysis/diff_inspect.py *), Read, Grep, Glob
---

# Stack Layout Diff Skill

Reconstruct and compare stack frame layouts between target and base compilations.

## Arguments

`$ARGUMENTS` - The function symbol name (mangled or demangled).

## Steps

1. **Run the stack layout analysis**:
   ```bash
   python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --stack-layout --project-dir .
   ```

2. **Present the results**:
   - **Frame sizes**: Target vs source frame sizes and delta
   - **Slot table**: Side-by-side stack slot comparison with access patterns
   - **Diagnosis**: Whether mismatches are from variable swaps (fixable) or frame shift (AT_LIMIT)

3. **Interpret for decomp**:
   - **MATCH**: Slot at same offset, same access pattern
   - **SWAPPED**: Two slots exchanged positions — try reordering declarations
   - **TGT_ONLY/SRC_ONLY**: Slot exists on one side only — may be compiler temp or different optimization
   - Frame delta from callee-saved register count is AT_LIMIT (unfixable)

## When to Use

- Function has dominant offset mismatches (e.g., `off:+16` across many instructions)
- `--offsets` mode shows a histogram with outlier deltas (beyond the dominant shift)
- Variable declaration reorder is suspected as a fix
- Confirming whether a frame size mismatch is fixable or AT_LIMIT

## Tips

- Run `--offsets` first to see the delta histogram, then `--stack-layout` for full diagnosis
- Pure frame shift (all same delta) = usually callee-saved register count difference
- Mixed deltas = likely variable swap + frame shift combined
