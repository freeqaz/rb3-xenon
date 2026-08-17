# Decomp Tool Workflow

Workflow narratives and patterns for rb3-xenon decompilation. For tool selection and quick command reference, see [INDEX.md](INDEX.md).

> **STATUS (2026-08-17).** The `./bin/analyze-function`, `tools/decompile.sh` and
> `./bin/merged-symbols` entry points **no longer exist** (`bin/` holds only the
> gitignored `objdiff-cli` symlink). They are replaced below by the **orchestrator
> MCP tools** and **skills**, which are the live path. ⚠ **Always pass
> `project_dir` = your worktree** to any MCP tool, or you measure main instead of
> your edits.

## Workflows

### New Function (any match %)

```
1. Recon first — the /recon skill, or:
     mcp__orchestrator__run_analyze_function
       symbol: "Foo::Bar"
       project_dir: "<your worktree>"
   - match %, struct field access map, workability assessment
   - for the disassembly itself: /compare-asm, or run_diff_inspect mode=asm_listing

2. If 0% match and complex, get a source oracle rather than a decompiler:
   - mcp__orchestrator__lookup_dc3     — engine code (src/system/**): closest twin,
                                          same compiler + same flags
   - mcp__orchestrator__lookup_rb3wii  — game code (src/band3/**, src/network/**):
                                          named funcs + MILO_ASSERT path strings
   (There is no m2c step here: the oracles supply real source, not a reconstruction.)

3. Write/edit C++ code

4. Iterate:
     mcp__orchestrator__run_objdiff
       symbol: "Foo::Bar"
       project_dir: "<your worktree>"

5. Done when report.json scores the row at fuzzy == 100 — NOT when a mismatch
   count hits zero (see the ruler note under "Verifying a Match").
```

### Near-Match (90%+) Tweaking

```
1. mcp__orchestrator__run_diff_inspect  mode="diagnose"  project_dir="<worktree>"

2. Check the verdict/fixability classification:
   - LIKELY_FIXABLE: clear source-level edit path — hand-edit
   - MAYBE_FIXABLE:  try variable reordering, comparison tweaks (permuter is a
                     low-effort first try — but the permuter is OFF by directive)
   - AT_LIMIT:       see the caveat below BEFORE accepting

   ⛔ An `AT_LIMIT` label on a row whose ONLY penalties are relocation-name
   arguments carries NO information — objdiff's LINKER_MERGED detector emits
   "no source mutation can close them" whenever target calls A, we call B and
   both look like function names, which is bit-for-bit the definition of a
   WRONG CALLEE. Lane MPNGAP-1 fixed such rows by editing source (+6,304 B).
   Adjudicate on retail bytes: does the named callee's signature match the call
   site? Same disease as REGISTER_SWAP being a symptom, not a diagnosis.

3. If fixable, iterate: edit → run_objdiff → repeat.
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

# Full verification (agents: prefer mcp__orchestrator__run_objdiff — it resolves
# the grader's ruler and config automatically)
./bin/objdiff-cli diff -p . "Foo::Bar" --verdict
```

⚠ **Believe `report.json` for the score, and mind the ruler.** Since
`d04c83df` (2026-08-12) the shipped grading ruler is
`functionRelocDiffs=name_check`; `none` is the opt-in. A percentage means nothing
without its ruler — the ruler alone moves `matched_code` ~817 kB / 7.9 pp at an
unchanged tree. `report.json` self-declares its ruler in a `provenance` block;
`scripts/analysis/ruler.py` resolves it at runtime. See
[`RULER_CHANGE_name_check_2026-08-12.md`](../decomp/RULER_CHANGE_name_check_2026-08-12.md).

⚠ **"N/N instructions equal" is NOT a match.** Instruction equality is
instruction-level; relocation-name charges are argument-level (`diff_arg`) and
coexist with "all instructions equal" — one row reads "205 instructions | all
equal" while scoring 98.4% graded. **Price from `report.json`'s charged-site
list, never from a mismatch or equality count.**

## Common Patterns

### Linker-Merged Functions (verify then accept)
Target calls `merged_*` functions. Before accepting as unfixable:
1. Look up what symbols share the merged address —
   `mcp__orchestrator__lookup_merged_symbol  address: "82331360"`
2. Verify YOUR call target is in that set
3. If verified: accept current match, move on
4. If NOT in set: you may be calling the wrong function - investigate

⚠ ICF is real here (MSVC `/OPT:ICF`, verified on `band.exe`), but it folds only
COMDATs identical **including relocations and associated `.xdata`** — so
byte-*similar* bodies at distinct addresses are expected and do NOT prove a fold.
Do not accept a fold on the detector's say-so; adjudicate on retail bytes.

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

# With worktree support (use ~/tmp — /tmp is RAM-backed tmpfs, house rule)
python3 scripts/analysis/diff_inspect.py --symbol "Foo::Bar" --diagnose --project-dir ~/tmp/wt-my-branch

# From existing JSON
python3 scripts/analysis/diff_inspect.py ~/tmp/diff.json --diagnose
python3 scripts/analysis/diff_inspect.py ~/tmp/diff.json --clusters
python3 scripts/analysis/diff_inspect.py ~/tmp/diff.json --regswaps
python3 scripts/analysis/diff_inspect.py ~/tmp/diff.json --offsets
python3 scripts/analysis/diff_inspect.py ~/tmp/diff.json --replaces

# Compare two snapshots (before/after)
python3 scripts/analysis/diff_inspect.py --compare baseline.json current.json
```

### MCP Tool (for agents)

```
mcp__orchestrator__run_diff_inspect
  symbol: "Foo::Bar"
  mode: "diagnose"              # clusters/regswaps/offsets/replaces/compare/
                                # save_baseline/mismatches/asm_listing/stack-layout
  project_dir: "~/tmp/wt-my-branch"    # YOUR worktree, or you measure main
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
| `mismatches` | Want every mismatched instruction listed | Target/base detail per mismatch |
| `asm_listing` | Need source→register mapping | `/FAs` compile, source-annotated asm |
| `stack-layout` | Suspect a frame/decl-order issue | Slot diff (also the `/stack-layout` skill) |

### Related skills

`/recon` (full pre-work recon) · `/compare-asm` (side-by-side target vs base) ·
`/stack-layout` (frame diff with source variable names) · `/permute` (source
permuter — ⚠ **OFF by standing directive**; do not run without an explicit
instruction).
