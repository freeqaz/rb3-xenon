# objdiff CLI Usage

Quick reference for the extended objdiff-cli commands.

**See also:** [CLI_OPTIONS.md](./CLI_OPTIONS.md) for a quick reference card with all options, configuration properties, and pattern detection details.

## Setup

**Important:** The extended commands (`report analyze`, `report query`, `report trending`, etc.) are only available in the custom build, not the system `objdiff-cli`.

```bash
# Use the repo wrapper (recommended for agents):
./bin/objdiff-cli report analyze ...

# Or source setup-env.sh to add bin/ to PATH:
source ./setup-env.sh
objdiff-cli report analyze ...
```

All examples below assume `./bin/objdiff-cli` or the PATH is configured.

## report summary

Get aggregate statistics from a report.

```bash
objdiff-cli report summary build/45410914/report.json
objdiff-cli report summary build/45410914/report.json -f text        # Human-readable
objdiff-cli report summary build/45410914/report.json --category game # Filter by category
```

## report query

Filter and search report data.

```bash
# Find functions close to matching (good work targets)
objdiff-cli report query build/45410914/report.json --functions \
  --min-percent 90 --max-percent 99 \
  --sort-by size --sort-order asc --limit 20

# Find unimplemented functions in a specific unit
objdiff-cli report query build/45410914/report.json --functions \
  --unit "default/lazer/game/*" --unimplemented

# Get unit-level stats for a subsystem
objdiff-cli report query build/45410914/report.json --units \
  --unit "default/system/rndobj/*" -f json-pretty
```

**Filters:** `--min-percent`, `--max-percent`, `--unimplemented`, `--min-size`, `--max-size`, `--unit` (glob), `--function` (regex)

**Sorting:** `--sort-by` (name, match_percent, size), `--sort-order` (asc, desc), `--limit`

**Output:** `--functions`, `--units`, `--summary`

**Formats:** `json` (default), `json-pretty`, `csv` (for spreadsheets/scripts)

## report function

Direct function lookup.

```bash
objdiff-cli report function build/45410914/report.json "Game::Poll"
objdiff-cli report function build/45410914/report.json "Shuttle::Set.*"  # Regex (matches SetActive, etc.)
objdiff-cli report function build/45410914/report.json "Game::Poll" --exact  # Escapes regex chars
```

**Note:** Patterns match against both mangled and demangled names. Demangled names include visibility modifiers (e.g., `public: void __cdecl Game::Poll(void)`), so `^` anchors rarely work as expected.

## report analyze

Batch analyze functions from a report with fixability verdicts. This command runs the full analysis pipeline on multiple functions and groups results by verdict classification.

```bash
# Analyze near-match functions (90-99%)
objdiff-cli report analyze build/45410914/report.json \
    --min-percent 90 --max-percent 99 \
    --limit 50 \
    -f json-pretty

# Analyze with explicit project directory
objdiff-cli report analyze build/45410914/report.json \
    -p /path/to/project \
    --min-percent 95 \
    -o analysis_results.json
```

**Options:**
- `--min-percent` - Minimum match percentage (0-100)
- `--max-percent` - Maximum match percentage (0-100)
- `--limit` - Maximum number of functions to analyze
- `-p, --project` - Project directory (defaults to current directory)
- `-o, --output` - Output file
- `-f, --format` - Output format: `json`, `json-pretty`, `csv` (default: json)
- `-c, --config` - Configuration property (key=value)

**Output:**
```json
{
  "query": {
    "min_percent": 90,
    "max_percent": 99,
    "limit": 50
  },
  "summary": {
    "total_analyzed": 42,
    "by_verdict": {
      "LikelyFixable": 5,
      "MaybeFixable": 12,
      "AtLimit": 20,
      "NeedsInvestigation": 5
    }
  },
  "results": {
    "LIKELY_FIXABLE": [
      {
        "name": "?Poll@Game@@QAAXXZ",
        "demangled": "public: void __cdecl Game::Poll(void)",
        "unit": "default/lazer/game/Game",
        "fuzzy_match_percent": 97.5,
        "size": 1248,
        "primary_pattern": "CONTROL_FLOW",
        "suggestion": "Check branch conditions and if/else structure"
      }
    ],
    "MAYBE_FIXABLE": [...],
    "AT_LIMIT": [...],
    "NEEDS_INVESTIGATION": [...]
  }
}
```

**CSV output** (for spreadsheets): `-f csv` exports `verdict,name,demangled,unit,fuzzy_match_percent,size,primary_pattern,suggestion`

## report trending

Compare multiple reports over time to track progress.

```bash
# Compare reports in argument order
objdiff-cli report trending reports/report_20260120.json reports/report_20260123.json -f text

# Compare using file modification times for ordering
objdiff-cli report trending reports/report_*.json --by-mtime -f text

# JSON output for scripting
objdiff-cli report trending reports/report_*.json -f json-pretty

# Filter to specific category
objdiff-cli report trending reports/report_*.json --category game -f text
```

**Options:**
- `--by-mtime` - Order reports by file modification time instead of argument order
- `--category` - Filter to a specific progress category
- `--limit` - Maximum number of reports to include (default: 30)
- `-o, --output` - Output file
- `-f, --format` - Output format: `json`, `json-pretty`, `text` (default: json)

**Text output:**
```
Progress Trend
============================================================

First report_20260120.json                      38.50%
      Functions: 21100/46958 (44.9%)  Code: 3450000/11326816 bytes
      Modified: 2026-01-20 00:00:00

Last  report_20260123.json                      39.03%  (+0.53%)
      Functions: 21264/46958 (45.3%)  Code: 3488096/11326816 bytes
      Modified: 2026-01-23 00:00:00

------------------------------------------------------------
Summary:
  Fuzzy match: 38.50% -> 39.03%  (+0.53%) ^
  Functions:   21100 -> 21264  (+164)
  Matched code: 3450000 -> 3488096 bytes  (+38096 bytes)
  Trend: IMPROVING
```

**JSON output:**
```json
{
  "report_count": 2,
  "reports": [
    {
      "path": "reports/report_20260120.json",
      "mtime": "2026-01-20 00:00:00",
      "fuzzy_match_percent": 38.5,
      "matched_code_percent": 30.4,
      "matched_functions": 21100,
      "total_functions": 46958,
      "matched_code": 3450000,
      "total_code": 11326816
    },
    {
      "path": "reports/report_20260123.json",
      "mtime": "2026-01-23 00:00:00",
      "fuzzy_match_percent": 39.03,
      "matched_code_percent": 30.8,
      "matched_functions": 21264,
      "total_functions": 46958,
      "matched_code": 3488096,
      "total_code": 11326816,
      "delta_fuzzy": 0.53,
      "delta_functions": 164,
      "delta_code": 38096
    }
  ],
  "summary": {
    "first_fuzzy_match_percent": 38.5,
    "last_fuzzy_match_percent": 39.03,
    "total_delta_fuzzy": 0.53,
    "first_matched_functions": 21100,
    "last_matched_functions": 21264,
    "total_delta_functions": 164,
    "first_matched_code": 3450000,
    "last_matched_code": 3488096,
    "total_delta_code": 38096,
    "trend": "improving"
  }
}
```

**Trend values:** `improving` (delta > 0.1%), `declining` (delta < -0.1%), `stable`

## diff (JSON output)

Get machine-readable diff output for a function.

```bash
# Basic diff
objdiff-cli diff -1 target.o -2 base.o "MyFunc" -f json-pretty

# With instruction-level details
objdiff-cli diff -1 target.o -2 base.o "MyFunc" -f json --include-instructions

# Project-based (finds unit automatically)
objdiff-cli diff -p . "Game::Poll" -f json-pretty

# With explicit unit (use unit name from report, not source path)
objdiff-cli diff -p . -u default/lazer/game/Game "Game::Poll" -f json

# Output to file
objdiff-cli diff -p . "Game::Poll" -f json -o /tmp/diff.json

# Rebuild before diffing (runs ninja on base object)
objdiff-cli diff -p . "Game::Poll" --build -f json --verdict
```

**Output formats:** `markdown` (default), `tui` (interactive), `json`, `json-pretty`

**Options:**
- `--include-instructions` - Add instruction-level diff
- `--summary` - Add instruction match type counts
- `--analyze` - Detect mismatch patterns (implies --summary)
- `--verdict` - Add fixability classification (implies --analyze)
- `--build` - Rebuild object file before diffing (runs `ninja` on base object)
- `-C, --context <N>` - Show N instructions before/after each mismatch (like grep -C)
- `--full-listing` - Show all instructions, not just mismatches (implies --include-instructions)

### JSON Output Schema

```json
{
  "symbol": "?Poll@Game@@QAAXXZ",
  "demangled": "public: void __cdecl Game::Poll(void)",
  "target_size": 1248,
  "base_size": 1252,
  "fuzzy_match_percent": 97.5,
  "diff_score": {"score": 150, "max_score": 13900},
  "instructions": [
    {
      "index": 0,
      "target": {"address": "0x0", "opcode": "lwz", "args": "r7, 0x0, r4"},
      "base": {"address": "0x0", "opcode": "lwz", "args": "r8, 0x0, r4"},
      "match_type": "diff_arg"
    }
  ]
}
```

**Note:** The `unit` field is not included in diff output by default.

**match_type values:** `equal`, `diff_op`, `diff_arg`, `replace`, `delete`, `insert`

## Markdown Output (Default for Non-TUI)

Markdown is now the **default output format** (unless using TUI mode). It produces human-readable reports ideal for:
- Agent-based workflows where structured prose is more useful than JSON
- Documentation and progress tracking
- Sharing analysis results

```bash
# Full analysis report in markdown (default format)
objdiff-cli diff -p . "Game::Poll" --verdict --include-instructions

# With context around mismatches (like grep -C)
objdiff-cli diff -p . "Game::Poll" --verdict -C 3

# Full instruction listing (all instructions, not just mismatches)
objdiff-cli diff -p . "Game::Poll" --verdict --full-listing
```

**New Features:**
- **Match Guidance**: Shows contextual hints based on match percentage (what to try next)
- **Pattern Doc Links**: Each detected pattern links to its documentation in `docs/decomp/patterns/`
- **Analysis Summary**: Shows patterns checked and unattributed mismatches
- **Verdict Factors Table**: Shows the factors that contributed to the verdict classification
- **Context Mode** (`-C N`): Shows N instructions before/after each mismatch for context
- **Full Listing** (`--full-listing`): Shows all instructions, not just mismatches

**Example output:**
```markdown
# Diff: public: void __cdecl Game::Poll(void)

- **Symbol**: `Game::Poll`
- **Demangled**: `public: void __cdecl Game::Poll(void)`
- **Match**: 97.5%
  - 💡 Check for unfixable patterns; if none, try variable reorder/inline assignment
- **Target Size**: 1248 bytes
- **Base Size**: 1252 bytes

## Instruction Summary

| Type | Count | Percent |
|------|------:|--------:|
| equal | 285 | 91.3% |
| diff_arg | 18 | 5.8% |
| diff_op | 3 | 1.0% |
| replace | 6 | 1.9% |
| **Total** | 312 | 100.0% |

## Patterns Detected

- **LINKER_MERGED**: 8 instruction(s), Unfixable
  - `merged_Read4FloatStruct`: 5 call(s)
  - `OnlyReturns`: 3 call(s)
  - 📖 [Pattern docs](docs/decomp/patterns/verifiable-icf.md#linker-merged-icf)
- **CONTROL_FLOW**: 3 instruction(s), LikelyFixable
  - Index 45: beq vs bne (diff_op)
  - Index 102: blt vs bge (diff_op)
  - 📖 [Pattern docs](docs/decomp/patterns/fixable-control-flow.md)

### Analysis Summary

- **Patterns Checked**: LINKER_MERGED, BOOL_MASK, REGISTER_SWAP, COMPARISON_STYLE, CONTROL_FLOW, COMMUTATIVE_OP_ORDER, OFFSET_SWAP
- **Unattributed Mismatches**: 7 (not explained by detected patterns)

## Verdict: LikelyFixable (Medium confidence)

3 control flow difference(s) detected with low merged ratio (26.7%).

### Verdict Factors

| Factor | Value | Threshold | Result |
|--------|-------|-----------|--------|
| bool_mask_detected | false | - | not_detected |
| merged_call_ratio | 0.27 | 0.8 | below_threshold |
| control_flow_diffs | 3 | 1.0 | detected |

**Recommendation**: Investigate control flow structure.

### Suggestions

1. Check branch at index 45, 102
2. Check branch conditions and if/else structure
3. Try equivalent comparison operators (>= vs >, etc.)

## Instruction Mismatches

| Index | Target | Base | Match |
|------:|--------|------|-------|
| 12 | `lwz r7, 0x10, r4` | `lwz r8, 0x10, r4` | diff_arg |
| 45 | `beq 0x120` | `bne 0x120` | diff_op |
| ... | ... | ... | ... |
```

**Agentic workflow example:**
```bash
# Agent can request markdown for easy parsing and reasoning
ANALYSIS=$(objdiff-cli diff -p . "$FUNC" -f markdown --verdict)

# The markdown contains all the context needed:
# - Match percentage in a consistent location
# - Pattern explanations in prose
# - Actionable suggestions
# - Instruction table for specific fixes
```

## Analysis & Verdict

Automated diagnosis of why a function doesn't match and whether it's fixable.

### Quick Summary (--summary)

Get instruction match type counts without full analysis:

```bash
objdiff-cli diff -p . "RndMat::LoadOld" -f json --summary | jq '.instruction_summary'
```

Output:
```json
{
  "total": 618,
  "equal": 568,
  "diff_arg": 28,
  "diff_op": 1,
  "replace": 8,
  "delete": 11,
  "insert": 2,
  "equal_percent": 91.91,
  "mismatch_percent": 8.09
}
```

### Pattern Analysis (--analyze)

Detect known mismatch patterns:

```bash
objdiff-cli diff -p . "RndMat::LoadOld" -f json --analyze | jq '.analysis'
```

Output:
```json
{
  "patterns": [
    {
      "pattern": "LINKER_MERGED",
      "confidence": "high",
      "instruction_count": 14,
      "fixability": "unfixable",
      "details": {
        "merged_functions": [
          {"name": "merged_Read4FloatStruct", "count": 7},
          {"name": "merged_Read3FloatStruct", "count": 4}
        ]
      }
    }
  ],
  "patterns_checked": ["LINKER_MERGED", "BOOL_MASK", "REGISTER_SWAP", "COMPARISON_STYLE", "CONTROL_FLOW", "COMMUTATIVE_OP_ORDER", "OFFSET_SWAP"],
  "unattributed_mismatches": 33
}
```

**Detected Patterns:**

| Pattern | Description | Fixability |
|---------|-------------|------------|
| `LINKER_MERGED` | Calls to `merged_*`, `OnlyReturns`, MSVC dtors | Verify then accept (see lookup workflow) |
| `BOOL_MASK` | `clrlwi`/`rlwinm` for bool masking | Often fixable (see [fixable-bool-mask.md](decomp/patterns/fixable-bool-mask.md)) |
| `REGISTER_SWAP` | Consistent register allocation differences | Maybe fixable (try reordering vars) |
| `COMPARISON_STYLE` | `cmpwi`/`cmplwi` with values differing by 1 (e.g., `>= 5` vs `> 4`) | Maybe fixable |
| `CONTROL_FLOW` | `diff_op`/`replace` on branch instructions | Likely fixable |
| `COMMUTATIVE_OP_ORDER` | Operand order swap in `fadd`/`fmul`/`add`/`and`/`or`/`xor` | Likely fixable |
| `OFFSET_SWAP` | Two offsets swapped between adjacent instructions | Likely fixable |

### Fixability Verdict (--verdict)

Get automated triage classification:

```bash
objdiff-cli diff -p . "RndMat::GetRefractEnabled" -f json --verdict | jq '.verdict'
```

Output:
```json
{
  "classification": "AT_LIMIT",
  "confidence": "high",
  "explanation": "Bool mask pattern detected - compiler bool return handling cannot be matched.",
  "factors": [
    {"name": "bool_mask_detected", "value": true, "result": "detected"}
  ],
  "recommendation": "Accept current match (97.1%). This is a compiler optimization difference.",
  "suggestions": []
}
```

**Verdict Classifications:**

| Verdict | Meaning | Action |
|---------|---------|--------|
| `COMPLETE` | 100% match | None needed |
| `LIKELY_FIXABLE` | Has control flow diffs, low merged ratio | Investigate if/else structure |
| `MAYBE_FIXABLE` | Register swaps or comparison style | Try reordering variables |
| `AT_LIMIT` | High merged ratio or bool mask | Accept current %, move on |
| `NEEDS_INVESTIGATION` | Mixed signals | Manual analysis required |

### Workflow Examples

**Triage a near-match function:**
```bash
# Get verdict to decide if worth investigating
objdiff-cli diff -p . "MyFunc" -f json --verdict | jq '{
  match: .fuzzy_match_percent,
  verdict: .verdict.classification,
  recommendation: .verdict.recommendation
}'
```

**Find what's blocking a match:**
```bash
# See all detected patterns
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '.analysis.patterns[] | {pattern, instruction_count, fixability}'
```

**Check for linker-merged function calls:**
```bash
# List merged functions being called
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "LINKER_MERGED") | .details.merged_functions'
```

**Verify merged symbol correctness:**
```bash
# After finding merged calls, verify your target is in the set
./bin/merged-symbols 82331360
# Check if the function YOU'RE calling appears in the output
# If yes: unfixable, accept at_limit
# If no: investigate - you may be calling wrong function
```

**Identify register swap candidates:**
```bash
# See which registers are swapped
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "REGISTER_SWAP") | .details.swaps'
```

**Check for comparison style differences:**
```bash
# See comparison instructions with off-by-one values (> vs >=)
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "COMPARISON_STYLE") | .details.comparisons'
```

**Check for control flow differences:**
```bash
# See branch instruction mismatches
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "CONTROL_FLOW") | .details.branch_diffs'
```

**Check for commutative operation order:**
```bash
# See operand order swaps in fadd/fmul/add/and/or/xor
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "COMMUTATIVE_OP_ORDER") | .details.swaps'
```

**Check for offset swaps:**
```bash
# See pairs of swapped offsets between instructions
objdiff-cli diff -p . "MyFunc" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "OFFSET_SWAP") | .details.swaps'
```

**Edit-rebuild-diff workflow:**
```bash
# Make changes, rebuild, and get verdict in one command
objdiff-cli diff -p . "MyFunc" --build -f json --verdict | jq '{
  match: .fuzzy_match_percent,
  verdict: .verdict.classification
}'
```

**Batch triage with report analyze:**
```bash
# Analyze all near-match functions and find actionable ones
objdiff-cli report analyze build/45410914/report.json \
    --min-percent 90 --max-percent 99 --limit 100 -f json-pretty \
    | jq '.results.LIKELY_FIXABLE[] | {name: .demangled // .name, percent: .fuzzy_match_percent, suggestion}'
```

## Configuration Options

The `-c key=value` flag allows setting configuration properties. Multiple `-c` flags can be used.

### PowerPC-Specific Options

```bash
# Enable experimental data flow analysis (shows register contents)
objdiff-cli diff -p . "MyFunc" -c analyzeDataFlow=true -f markdown --verdict

# Disable pool relocation calculation
objdiff-cli diff -p . "MyFunc" -c ppc.calculatePoolRelocations=false
```

### Common Configuration Properties

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `analyzeDataFlow` | `true`/`false` | `false` | **(Experimental)** Data flow analysis |
| `ppc.calculatePoolRelocations` | `true`/`false` | `true` | Show pooled data refs as relocations |
| `demangler` | `auto`/`none`/`msvc`/`itanium`/etc | `auto` | Symbol demangling format |
| `spaceBetweenArgs` | `true`/`false` | `true` | Space between instruction args |

See [CLI_OPTIONS.md](./CLI_OPTIONS.md) for the complete list of configuration options.

## Common Patterns

```bash
# Find mismatched instructions in a near-match function
# (use unique function name or add --unit to avoid ambiguity)
objdiff-cli diff -p . "MyFunc" -f json --include-instructions \
  | jq '[.instructions[] | select(.match_type | test("diff|replace"))]'

# Check if a function is done
objdiff-cli report function build/45410914/report.json "MyFunc" \
  | jq '.matches[0].fuzzy_match_percent'

# List all 100% functions in a unit (use unit name from report)
objdiff-cli report query build/45410914/report.json --functions \
  --unit "default/lazer/game/Game" --min-percent 100
```
