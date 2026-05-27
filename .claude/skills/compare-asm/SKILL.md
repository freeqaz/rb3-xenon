---
name: compare-asm
description: Side-by-side target vs base assembly comparison with cluster boundaries and mismatch annotations. Shows aligned instructions with match markers, register swap details, and offset deltas. Use when diagnosing the last few percent of a function.
argument-hint: "<symbol_name>"
allowed-tools: Bash(python3 scripts/analysis/diff_inspect.py *), Read, Grep, Glob
---

# Compare ASM Skill

Side-by-side target vs base assembly comparison with annotations.

## Arguments

`$ARGUMENTS` - The function symbol name (mangled or demangled).

## Steps

1. **Run the comparison**:
   ```bash
   python3 scripts/analysis/diff_inspect.py --symbol "$ARGUMENTS" --compare-asm --project-dir .
   ```

2. **Read the output**:
   - Match markers: `=` equal, `~` diff_arg (same opcode, different args), `!` diff_op, `+` insert (target only), `-` delete (base only), `X` replace
   - Cluster boundaries mark contiguous insert/delete regions
   - `[reg:rN->rM]` annotations show register swap details
   - `[off:+N]` annotations show offset/immediate deltas
   - Consecutive equal instructions are collapsed (first 2 + last 2 shown)

3. **Diagnosis summary** at the end shows:
   - Mismatch type counts
   - Register swap pairs with frequency
   - Dominant offset delta

## When to Use

- Working on a function's "last 10%" and need to see exactly where mismatches are
- Want to correlate register swaps with specific instruction sequences
- Need to understand the structure of insert/delete clusters
- Comparing the effect of a source change on assembly output

## Output Size

- Equal instruction runs of 5+ are collapsed to save space
- Output is capped at 150 instruction rows for large functions
- Focus is on mismatch regions with context

## Tips

- Use `--offsets` mode separately for detailed offset histogram
- Use `--regswaps` mode for comprehensive register pair analysis
