# objdiff CLI Quick Reference

Quick reference card for objdiff-cli options and configuration.

## diff Command

```bash
./bin/objdiff-cli diff [OPTIONS] [SYMBOL]
```

### Basic Options

| Option | Short | Description |
|--------|-------|-------------|
| `--target <path>` | `-1` | Target (reference) object file |
| `--base <path>` | `-2` | Base (decompiled) object file |
| `--project <path>` | `-p` | Project directory |
| `--unit <name>` | `-u` | Unit name within project |
| `--output <path>` | `-o` | Output file ("-" for stdout) |
| `--format <fmt>` | `-f` | Output format (see below) |
| `--config <k=v>` | `-c` | Configuration property |

### Output Formats (`-f`)

| Format | Description |
|--------|-------------|
| `markdown` | Human-readable markdown (default, ideal for agents) |
| `tui` | Interactive terminal UI |
| `json` | Compact JSON |
| `json-pretty` | Pretty-printed JSON with typed arguments |

### Analysis Flags

| Flag | Description |
|------|-------------|
| `--include-instructions` | Include instruction-level diff |
| `--include-data` | Include data section diff |
| `--summary` | Include instruction match type counts |
| `--analyze` | Detect mismatch patterns (implies --summary) |
| `--verdict` | Include fixability verdict (implies --analyze) |
| `-C, --context <N>` | Show N instructions before/after each mismatch (like grep -C) |
| `--full-listing` | Show all instructions, not just mismatches (implies --include-instructions) |

### Build Flags

| Flag | Description |
|------|-------------|
| `--build` | Rebuild object file before diffing (runs ninja) |
| `--full-build` | Full project build instead of incremental |
| `--incremental` | Incremental build targeting specific .obj |
| `--map-file <path>` | MSVC linker map for ICF symbol equivalence |

---

## Configuration Options (`-c key=value`)

### General Options

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `functionRelocDiffs` | `none`, `name_address`, `data_value`, `all` | `name_address` | How relocation targets are diffed |
| `demangler` | `auto`, `none`, `codewarrior`, `itanium`, `msvc`, `gnu_legacy` | `auto` | Symbol demangling format |
| `spaceBetweenArgs` | `true`, `false` | `true` | Space between instruction args |
| `combineDataSections` | `true`, `false` | `false` | Combine data sections with equal names |
| `combineTextSections` | `true`, `false` | `false` | Combine all text sections into one |

### PowerPC-Specific Options

| Key | Values | Default | Description |
|-----|--------|---------|-------------|
| `ppc.calculatePoolRelocations` | `true`, `false` | `true` | Show pooled data refs as fake relocations |
| `analyzeDataFlow` | `true`, `false` | `false` | **(Experimental)** Data flow analysis |
| `showDataFlow` | `true`, `false` | `true` | Show data flow results in register names |

### Example Usage

```bash
# Enable experimental data flow analysis
./bin/objdiff-cli diff -u default/system/char/CharacterTest \
  '?Sync@CharacterTest@@IAAXXZ' \
  -c analyzeDataFlow=true -f markdown --verdict

# Disable pool relocation calculation
./bin/objdiff-cli diff -u default/lazer/game/Game \
  'Game::Poll' -c ppc.calculatePoolRelocations=false -f json
```

---

## Pattern Detection

When using `--analyze`, these patterns are automatically detected:

| Pattern | Fixability | Description |
|---------|------------|-------------|
| `LINKER_MERGED` | Unfixable | Calls to ICF-merged functions (`merged_*`, `OnlyReturns`, MSVC dtors) |
| `BOOL_MASK` | UsuallyUnfixable | Bool return masking with `clrlwi`/`rlwinm` (bit 24 or 31) |
| `REGISTER_SWAP` | MaybeFixable | Consistent register allocation differences (e.g., r30↔r31) |
| `COMPARISON_STYLE` | LikelyFixable | Comparison immediate differs by 1 (`>` vs `>=`) |
| `CONTROL_FLOW` | LikelyFixable | Branch instruction differences (`beq` vs `bne`, etc.) |
| `COMMUTATIVE_OP_ORDER` | LikelyFixable | Operand order swap in `fadd`/`fmul`/`add`/`and`/`or`/`xor` |
| `OFFSET_SWAP` | LikelyFixable | Two offsets swapped between adjacent instructions |

### Verdict Classifications

| Verdict | Meaning | Action |
|---------|---------|--------|
| `COMPLETE` | 100% match | None needed |
| `LIKELY_FIXABLE` | Control flow diffs, low merged ratio | Investigate if/else structure |
| `MAYBE_FIXABLE` | Register swaps or comparison style | Try reordering variables |
| `AT_LIMIT` | High merged ratio or bool mask | Accept current %, move on |
| `NEEDS_INVESTIGATION` | Mixed signals | Manual analysis required |

---

## Common Workflows

### Quick Triage
```bash
# Get verdict to decide if worth investigating
./bin/objdiff-cli diff -p . "MyFunc" -f json --verdict | jq '{
  match: .fuzzy_match_percent,
  verdict: .verdict.classification,
  recommendation: .verdict.recommendation
}'
```

### Find Blocking Patterns
```bash
# See all detected patterns
./bin/objdiff-cli diff -p . "MyFunc" -f json --analyze | jq \
  '.analysis.patterns[] | {pattern, instruction_count, fixability}'
```

### Check Specific Pattern Details
```bash
# Control flow differences
./bin/objdiff-cli diff -p . "MyFunc" -f json --analyze | jq \
  '.analysis.patterns[] | select(.pattern == "CONTROL_FLOW") | .details.branch_diffs'

# Register swaps
./bin/objdiff-cli diff -p . "MyFunc" -f json --analyze | jq \
  '.analysis.patterns[] | select(.pattern == "REGISTER_SWAP") | .details.swaps'

# Merged function calls
./bin/objdiff-cli diff -p . "MyFunc" -f json --analyze | jq \
  '.analysis.patterns[] | select(.pattern == "LINKER_MERGED") | .details.merged_functions'
```

### Build-and-Diff Cycle
```bash
# Edit code, rebuild, and get verdict in one command
./bin/objdiff-cli diff -p . "MyFunc" --build -f json --verdict | jq '{
  match: .fuzzy_match_percent,
  verdict: .verdict.classification
}'
```

### Full Analysis for Agent
```bash
# Get everything in markdown for AI consumption
./bin/objdiff-cli diff -u default/system/char/CharacterTest \
  '?Sync@CharacterTest@@IAAXXZ' \
  --analyze --verdict -f markdown
```

---

## JSON Output Structure

### With `--analyze --verdict`

```json
{
  "symbol": "?Sync@CharacterTest@@IAAXXZ",
  "demangled": "protected: void __cdecl CharacterTest::Sync(void)",
  "unit": "default/system/char/CharacterTest",
  "target_size": 848,
  "base_size": 836,
  "fuzzy_match_percent": 95.9,
  "diff_score": { "score": 876, "max_score": 21200 },
  "instruction_summary": {
    "total": 214,
    "equal": 194,
    "diff_arg": 10,
    "diff_op": 1,
    "replace": 2,
    "delete": 5,
    "insert": 2,
    "equal_percent": 90.7,
    "mismatch_percent": 9.3
  },
  "analysis": {
    "patterns": [
      {
        "pattern": "CONTROL_FLOW",
        "confidence": "medium",
        "instruction_count": 1,
        "fixability": "likely_fixable",
        "details": {
          "branch_diffs": [
            { "index": 188, "target_opcode": "bgt", "base_opcode": "ble", "match_type": "diff_op" }
          ]
        }
      }
    ],
    "patterns_checked": ["LINKER_MERGED", "BOOL_MASK", "REGISTER_SWAP", "COMPARISON_STYLE", "CONTROL_FLOW", "COMMUTATIVE_OP_ORDER", "OFFSET_SWAP"],
    "unattributed_mismatches": 19
  },
  "verdict": {
    "classification": "LIKELY_FIXABLE",
    "confidence": "medium",
    "explanation": "3 control flow difference(s) detected with low merged ratio (0.0%).",
    "factors": [
      { "name": "bool_mask_detected", "value": false, "result": "not_detected" },
      { "name": "merged_call_ratio", "value": 0.0, "threshold": 0.8, "result": "below_threshold" },
      { "name": "control_flow_diffs", "value": 3, "threshold": 1.0, "result": "detected" }
    ],
    "recommendation": "Investigate control flow structure.",
    "suggestions": [
      { "action": "Check branch at index 188" },
      { "action": "Check branch conditions and if/else structure" },
      { "action": "Try equivalent comparison operators (>= vs >, etc.)" }
    ]
  }
}
```

### With `--include-instructions`

Each instruction includes typed arguments:

```json
{
  "index": 5,
  "target": {
    "address": "0x365c",
    "opcode": "li",
    "args": "r29, 0x0",
    "typed_args": [
      { "type": "Register", "value": "r29" },
      { "type": "Signed", "value": 0 }
    ]
  },
  "base": { ... },
  "match_type": "equal"
}
```

**TypedArg types:** `Signed`, `Unsigned`, `Register`, `Symbol`, `BranchDest`, `Other`
