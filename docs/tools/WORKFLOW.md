# Decomp Tool Workflow

Workflow narratives and patterns for rb3-xenon decompilation. For tool selection and quick command reference, see [INDEX.md](INDEX.md).

## Workflows

### New Function (any match %)

```
1. ./bin/analyze-function "Foo::Bar" --m2c
   - Understand what the function does (Ghidra decompile)
   - Check current match % and verdict
   - Get m2c decompilation for starting point
   - Note callers/callees for context

2. If 0% match and complex:
   - Use the m2c output from step 1 as starting point
   - Or use: tools/decompile.sh "Foo::Bar" --context
   - Or reference RB3 decomp for shared engine code

3. Write/edit C++ code

4. Iterate:
   ./bin/objdiff-cli diff -p . "Foo::Bar" --build --verdict

5. When verdict shows AT_LIMIT or 100%: done
```

### Near-Match (90%+) Tweaking

```
1. ./bin/objdiff-cli diff -p . "Foo::Bar" --verdict

2. Check verdict:
   - LIKELY_FIXABLE: Apply suggested patterns
   - MAYBE_FIXABLE: Try variable reordering, comparison tweaks
   - AT_LIMIT: Accept current match, move on

3. If fixable, iterate:
   - Edit code
   - ./bin/objdiff-cli diff -p . "Foo::Bar" --build --verdict
   - Repeat until 100% or AT_LIMIT
```

### Finding Work Targets

```
# Option A: Find by match percentage
./bin/objdiff-cli report query build/45410914/report.json --functions \
  --min-percent 90 --max-percent 99 --limit 20

# Option B: Batch triage with verdicts
./bin/objdiff-cli report analyze build/45410914/report.json \
  --min-percent 90 --limit 50 -f json-pretty | \
  jq '.results.LIKELY_FIXABLE'

# Option C: Find small functions (easier to match)
./bin/objdiff-cli report query build/45410914/report.json --functions \
  --min-percent 80 --max-size 300 --sort-by size --sort-order asc
```

### Verifying a Match

```bash
# Quick check
./bin/objdiff-cli report function build/45410914/report.json "Foo::Bar"

# Full verification
./bin/objdiff-cli diff -p . "Foo::Bar" --verdict
```

## Common Patterns

### Linker-Merged Functions (verify then accept)
Target calls `merged_*` functions. Before accepting as unfixable:
1. Look up what symbols share the merged address (`./bin/merged-symbols <addr>`)
2. Verify YOUR call target is in that set
3. If verified: accept current match, move on
4. If NOT in set: you may be calling the wrong function - investigate

### Bool Mask (usually unfixable)
Differences in `clrlwi`/`rlwinm` for bool return handling. Compiler optimization.

### Control Flow (often fixable)
Branch instruction differences (`beq` vs `bne`). Check:
- if/else ordering
- Loop structure
- Comparison operators (`>` vs `>=`)

### Register Allocation (sometimes fixable)
Consistent register swaps. Try:
- Reordering variable declarations
- Reordering struct members
- Changing parameter order (if confirmed via DWARF)

## diff_inspect — Deep Mismatch Analysis

**When:** `objdiff --verdict` tells you something is wrong but you need to understand WHY.

**Why:** Provides structured analysis of mismatch patterns that objdiff's verdict summarizes but doesn't break down.

### Direct Usage

```bash
# Root cause analysis (start here)
python3 scripts/analysis/diff_inspect.py --symbol "Foo::Bar" --diagnose

# With worktree support
python3 scripts/analysis/diff_inspect.py --symbol "Foo::Bar" --diagnose --project-dir /tmp/claude/my-branch

# From existing JSON
python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --diagnose
python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --clusters
python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --regswaps
python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --offsets
python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --replaces

# Compare two snapshots (before/after)
python3 scripts/analysis/diff_inspect.py --compare baseline.json current.json
```

### MCP Tool (for agents)

```
mcp__orchestrator__run_diff_inspect
  symbol: "Foo::Bar"
  mode: "diagnose"              # or clusters/regswaps/offsets/replaces/compare/save_baseline
  project_dir: "/tmp/worktree"
```

### Mode Selection Guide

| Mode | Use When | Output |
|------|----------|--------|
| `diagnose` | First analysis — don't know what's wrong | Root cause summary with actionable suggestions |
| `clusters` | Seeing scattered insert/delete mismatches | Contiguous mismatch groups with context |
| `regswaps` | Verdict mentions register allocation | GPR/FPR swap pairs and frequency |
| `offsets` | Seeing offset differences in memory ops | Offset shift histogram + outlier detection |
| `replaces` | Many "replace" diffs, unclear which matter | Categorizes noise (trivial) vs real (structural) |
| `compare` | Want to see if edits improved things | Delta table: match% change, mismatch deltas |
| `save_baseline` | About to start editing, want a reference point | Saves current state for later `compare` |
