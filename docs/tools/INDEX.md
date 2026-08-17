# Tools Index — Agent Tool Selection

Which tool to use for decomp work. For scripts, commands, and reference material, see [REFERENCE.md](REFERENCE.md).

## Start Here

| Doc | Description |
|-----|-------------|
| **[WORKFLOW.md](WORKFLOW.md)** | **Decision guide: which tool to use when** |

> **STATUS (2026-08-17): tool table rebuilt from reality.** Removed three MCP
> tools that do not exist on this server (`lookup_rb3`, `get_rb3_pair`,
> `get_rb2_class_info` — those are DC3-side), five dead doc links
> (`ANALYZE_FUNCTION.md`, `objdiff.md`, `m2c.md`, `compiler-trace.md`,
> `HTTP_DEBUG_SERVER.md`), and dead commands (`./bin/orchestrate` — `bin/` holds
> only the gitignored `objdiff-cli` symlink; `scripts/analysis/function_health.py`,
> `regswap_classify.py`, `reclassify_at_limit.py`; all `msvc-src/` paths). Each
> was verified absent on disk before removal.

## MCP Orchestrator Tools (Primary Interface)

Server name: **`decomp`** (`.mcp.json` → `scripts/orchestrator/`). These **11
tools are the complete set** — anything else you have seen referenced is from
another repo. Prefer them for decomp analysis; use the CLI directly only for
flags not exposed through MCP.

⚠ **Pass `project_dir` = your worktree** to every build/diff tool, or you measure
main instead of your edits.

| Tool | Description |
|------|-------------|
| `run_objdiff` | Build + diff a function. Returns match%, verdict. Supports `full_listing` for complete instruction output. |
| `run_diff_inspect` | Deep mismatch analysis: `diagnose`, `mismatches`, `clusters`, `regswaps`, `offsets`, `replaces`, `compare`, `save_baseline`, `asm_listing`, `stack-layout` |
| `run_analyze_function` | Combined objdiff + struct offset resolution for field-level context |
| `query_functions` | Find workable functions by unit pattern, match range, unicorn verdict/class |
| `get_attempts` | Previous attempt history for a function — read before re-opening a row |
| `report_result` | Report task completion (`complete` / `at_limit` / `stuck` / `error`) |
| `mark_patch_result` | Mark a queued patch `applied` / `failed` / `skipped` |
| `lookup_dc3` | DC3 source oracle — engine code (`src/system/**`), same compiler + flags |
| `lookup_rb3wii` | rb3-Wii source oracle — game code (`src/band3/**`, `src/network/**`), named funcs + `MILO_ASSERT` path strings |
| `lookup_struct_offset` | Which struct field is at an offset — asks the **compiler** by default (`verify=true`); comment-derived answers are labelled UNVERIFIED |
| `lookup_merged_symbol` | Resolve `merged_<addr>` to actual symbol names (ICF) |

⚠ **`report.json` is the score of record**, and since 2026-08-12 the shipped
ruler is `functionRelocDiffs=name_check` — see
[`RULER_CHANGE_name_check_2026-08-12.md`](../decomp/RULER_CHANGE_name_check_2026-08-12.md).
Every percentage these tools print is labelled with its ruler.

## Skills

**25 skills** in `.claude/skills/` — the invocable wrappers around the above.
Decomp-relevant: `/recon`, `/compare-asm`, `/batch-check`, `/data-diff`,
`/stack-layout`, `/struct-info`, `/vtable`, `/resolve-vcall`, `/dc3-pair`,
`/rb3wii-pair`, `/progress`, `/ab-measure`, `/unicorn-query`, `/refactor-staff`,
`/permute` (⚠ **OFF by standing directive**). Ghidra: `/ghidra-decompile`,
`/ghidra-search`, `/ghidra-struct`. Native/asset/GPU: `/native-build`,
`/asset-extract`, `/screenshot`, `/gpu-capture`, `/gpu-inspect`, `/gpu-debug`,
`/xenia-gameplay`.

⚠ **`struct_info` is a SKILL (`/struct-info`), not an MCP tool** — it was listed
as one here until 2026-08-17. The MCP equivalent is `lookup_struct_offset`.

## Decompilation Tools

| Tool | Description | Doc |
|------|-------------|-----|
| diff_inspect | Deep mismatch analysis (diagnose, clusters, regswaps, offsets, replaces, compare) | [WORKFLOW.md](WORKFLOW.md#diff_inspect) |
| objdiff | Assembly diffing / function matching. Local fork at `../objdiff`; `bin/objdiff-cli` symlinks its release binary | [WORKFLOW.md](WORKFLOW.md) |
| m2c pipeline | Machine code → C, via `tools/objdiff_to_m2c.py` + `~/code/milohax/m2c` | [REFERENCE.md](REFERENCE.md) |
| [Ghidra + pyghidra-mcp](GHIDRA.md) | Binary analysis, decompilation, and type seeding via MCP | [GHIDRA.md](GHIDRA.md) |
| [Ghidra Manual Setup](GHIDRA_MANUAL_SETUP.md) | GUI-only Ghidra setup (no MCP) — symbol import, fork install | [GHIDRA_MANUAL_SETUP.md](GHIDRA_MANUAL_SETUP.md) |
| [XEXLoaderWV](XEXLOADERWV.md) | Ghidra extension for Xbox 360 XEX files | [XEXLOADERWV.md](XEXLOADERWV.md) |

## Ghidra CLI Analysis Tools

Require running pyghidra-mcp service (`./tools/ghidra/pyghidra-service.sh start`) and seeded DTM (`python3 tools/ghidra/batch_export_types.py --seed`). See [GHIDRA.md](GHIDRA.md#cli-analysis-tools).

| Tool | Description | Usage | Skill |
|------|-------------|-------|-------|
| `struct_check.py` | Compare header struct layouts vs Ghidra DTM | `python3 tools/ghidra/struct_check.py HamDirector` | `/ghidra-struct` |
| `pcode_inspect.py` | Switch table + cast analysis from decompiled output | `python3 tools/ghidra/pcode_inspect.py "Class::Method" --switches` | `/ghidra-decompile` |
| `code_search.py` | Semantic search over 42K+ decompiled functions (auto-filters `__unwind$` noise) | `python3 tools/ghidra/code_search.py "iterate list delete"` | `/ghidra-search` |

## Dynamic Analysis

| Tool | Description | Doc |
|------|-------------|-----|
| [Unicorn Function Runner](UNICORN_FUNCTION_RUNNER.md) | Differential function execution (Unicorn PPC32 BE) — compare decomp vs original behavior | [UNICORN_FUNCTION_RUNNER.md](UNICORN_FUNCTION_RUNNER.md) |

```bash
# Combined diagnosis with SKIP/FIX recommendations
python3 -m scripts.unicorn_runner.diagnose --unit system/meta/Profile --batch

# Multi-input probing for higher confidence
python3 -m scripts.unicorn_runner.probe --unit DirLoader --batch --runs 8
```

Find functions with real behavioral bugs (logic divergences) via the **`/unicorn-query`
skill**, or `mcp__orchestrator__query_functions` with
`unicorn_verdict="DIVERGENT"` and `unicorn_class="logic"`.
*(`./bin/orchestrate` no longer exists — removed 2026-08-17.)*

⚠ Divergence classes split into **real bugs** (`logic`, `call_count`, `call_arg`,
`return_value`, `object_memory`) and **unfixable artifacts** (`build_env`,
`regalloc`, `merged_call`, `merged_arg`, `stack_layout`, `fpr_precision`). Filter
on the former.

## Compiler Analysis

⚠ **The `msvc-src/` tools (c2 Decompile, IL Parser, IL Annotate, IL Diff) and
`compiler-trace.md` were removed 2026-08-17 — `msvc-src/` does not exist in this
repo** and the doc was a dead link. `tools/compiler_trace` itself survives:

| Tool | Description |
|------|-------------|
| `tools/compiler_trace` | c2.dll instrumentation: asm diff, IL capture, perf profiling |

```bash
# Compare assembly for two source variants (detects register swaps)
python -m tools.compiler_trace diff-asm test_a.cpp test_b.cpp

# Capture compiler IL temp files (~/tmp — /tmp is RAM-backed tmpfs)
python -m tools.compiler_trace capture-il test.cpp --output-dir ~/tmp/il_out

# Profile and diff c2.dll execution paths
python -m tools.compiler_trace callgrind-diff test_a.cpp test_b.cpp
```

## Post-Build Tools

| Tool | Description | Doc |
|------|-------------|-----|
| Register Swap Patcher | Patches .obj register fields using objdiff diff as oracle (manual, not run by default) | [REFERENCE.md](REFERENCE.md#register-swap-patcher) |

## Analysis & Diagnostic Tools

⚠ **Removed 2026-08-17, all verified absent:** Function Health / Batch Health
(`scripts/analysis/function_health.py`), Regswap Classify
(`scripts/analysis/regswap_classify.py`), Reclassify AT_LIMIT
(`scripts/analysis/reclassify_at_limit.py`). Their live replacements:

| Need | Use |
|------|-----|
| Unified per-function diagnostic (match%, mismatch breakdown, verdict) | `mcp__orchestrator__run_analyze_function`, or `/recon` |
| Scan/rank functions by unit + match range | `mcp__orchestrator__query_functions`, or `/progress` |
| Classify register swaps | `run_diff_inspect  mode="regswaps"` — ⚠ a `REGISTER_SWAP` label is a **symptom, not a diagnosis**; see [`fixable-liveness.md`](../decomp/patterns/fixable-liveness.md) |
| Re-open an `AT_LIMIT` row | `get_attempts` first; ⛔ an `AT_LIMIT` label on a relocation-name-only row carries **no information** |
| Whole-binary price of a change | `python3 tools/ab_measure.py`, or `/ab-measure` |
| Behavioural divergence | `/unicorn-query`, or `query_functions unicorn_verdict="DIVERGENT"` |

## Analysis engine

`scripts/analysis/diff_inspect.py` (backs `/compare-asm`, `/stack-layout` and the
MCP `run_diff_inspect`) and `scripts/analysis/ruler.py` (resolves the grader's
ruler at runtime from `report.json` provenance — never hardcode a ruler).

## Code Transformation Tools

| Tool | Description | Doc |
|------|-------------|-----|
| C++ Permuter | Tree-sitter based source permutation for register allocation issues | [../permuter/INDEX.md](../permuter/INDEX.md) |
