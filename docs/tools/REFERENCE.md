# Tools Reference

Scripts, commands, and reference material for the rb3-xenon decompilation project. For agent tool selection, see [INDEX.md](INDEX.md).

## Project Scripts

> **STATUS (2026-08-17).** Three entries were removed from the table below —
> `tools/decompile.sh`, `tools/asm_to_m2c.py` and
> `scripts/build/rebuild_jeff_link.sh` — **verified absent from the tree**, as is
> `./bin/analyze-function` (`bin/` holds only the gitignored `objdiff-cli`
> symlink). The live entry points are the orchestrator **MCP tools** and
> **skills**; see the table under "Live entry points" and [INDEX.md](INDEX.md).

| Script | Description |
|--------|-------------|
| `tools/objdiff_to_m2c.py` | Convert objdiff JSON to m2c assembly format (with jump table resolution) |
| `tools/ghidra/export_types.py` | Export Ghidra types as m2c context headers |
| `tools/decompctx.py` | Generate context files for decomp.me |
| `configure.py` | Generate build files (ninja) |

### Live entry points (prefer these)

| Entry point | Use |
|---|---|
| `mcp__orchestrator__run_objdiff` | Build + diff one function; match% and verdict. ⚠ pass `project_dir` = your worktree |
| `mcp__orchestrator__run_analyze_function` | Enriched diff with struct-offset field names resolved |
| `mcp__orchestrator__run_diff_inspect` | Deep root-cause analysis (`diagnose`, `clusters`, `regswaps`, `offsets`, `replaces`, `mismatches`, `asm_listing`, `stack-layout`) |
| `mcp__orchestrator__lookup_struct_offset` | Which field sits at an offset — asks the **compiler** (`/d1reportSingleClassLayout`), not the header comments |
| `mcp__orchestrator__lookup_dc3` | DC3 source oracle — engine code (`src/system/**`), same compiler + flags |
| `mcp__orchestrator__lookup_rb3wii` | rb3-Wii source oracle — game code (`src/band3/**`, `src/network/**`) |
| `mcp__orchestrator__lookup_merged_symbol` | Symbols sharing an ICF-merged address |
| `/recon`, `/compare-asm`, `/stack-layout` | Skill wrappers for the above |
| `/permute` | Source permuter — ⚠ **OFF by standing directive** |
| `python3 tools/ab_measure.py` (or `/ab-measure`) | The **only** sanctioned way to price a change whole-binary |

## Symbol Lookup (No Map File for RB3)

**RB3 has no leaked linker map.** `orig/45410914/` contains only `default.xex`
and `band.exe` — no `.map` (verified on disk 2026-07-06). The `ham_xbox_r.map`
shown below is **DC3's** map, at `../dc3-decomp/orig/373307D9/ham_xbox_r.map`
(same Milo-engine toolchain, useful as a symbol-name *oracle* for engine code,
not a direct source of RB3 addresses). For RB3 identification, use:

- **`tools/fingerprint_match.py`** (extract/report/autoid/identify) — indexes
  all 69,227 RB3 functions by referenced strings/callees/constants and
  cross-refs against `../rb3/src` (Wii dev decomp, named functions) and
  `../dc3-decomp/src` (same engine, named functions) to propose source-file
  mappings. See `project_function_identification.md` in memory.
- **`decomp.db`** (SQLite, ingested from `build/45410914/report.json`) — the
  function database queried by the orchestrator MCP (`query_functions`,
  `get_attempts`, etc.) and directly via `sqlite3` (see below).
- **Ghidra + BinDiff** (planned/partial) — transfers DC3's named functions onto
  RB3's anonymous `fn_8XXXXXXX` by structural similarity; see
  `tools/ghidra/build_symbol_map.py` + `apply_symbols.py` for the
  objdiff-matched-symbol renaming pipeline that's actually in use today.

DC3 map lookup (for reference — cross-repo, not RB3's own symbols):

```bash
# Find function address by name in DC3's map
grep "FastSin\|Pool::Alloc" ../dc3-decomp/orig/373307D9/ham_xbox_r.map

# Example output:
# 0005:002027e8       ?FastSin@@YAMM@Z           825327e8 f   math:Trig.obj
#                     ^ mangled name              ^ address    ^ source file
```

## Merged Symbol Lookup (ICF)

When objdiff shows `LINKER_MERGED` patterns with `merged_<address>` symbols, use the merged-symbols tool to identify the actual symbol names:

```bash
# Look up what symbols are at a merged address
./bin/merged-symbols 82331360

# Also accepts the merged_ prefix from objdiff output
./bin/merged-symbols merged_82331448 -v

# See statistics on all merged symbols
./bin/merged-symbols --stats -e

# Output as JSON
./bin/merged-symbols 82331360 --json
```

ICF (Identical COMDAT Folding) merges functions with identical machine code to save space. Common patterns:
- `??_G` / `??_E`: Scalar and vector deleting destructors (identical code)
- Template instantiations like `ObjRefConcrete<T>::GetObj()` (same code for different T)

## Function Database (decomp.db)

SQLite database tracking all functions, patterns, and scoring:

```bash
# Find high-priority reachable targets
sqlite3 decomp.db "SELECT symbol, current_percent FROM functions WHERE reachable_100=1 AND current_percent < 100 ORDER BY priority_score DESC LIMIT 10"

# Query functions by pattern
sqlite3 decomp.db "SELECT symbol, current_percent FROM functions WHERE has_linker_merged=1 ORDER BY current_percent DESC LIMIT 10"
```

See [../reference/DATABASE_SCHEMA.md](../reference/DATABASE_SCHEMA.md) for full schema documentation.

## Linking Tools

| Script | Description |
|--------|-------------|
| `scripts/build/link_test.py` | Standalone X360 link test (links split/hybrid .obj → PE) |
| `scripts/build/compare_pe.py` | Compare linked PE against original `ham_xbox_r.exe` |
| `scripts/build/fix_pdata.py` | Workaround for dtk .pdata splitting bug (integrated into `ninja link`) |

See [../sessions/2026-02-11-x360-linking-pipeline.md](../sessions/2026-02-11-x360-linking-pipeline.md) for full status and roadmap.

## Register Swap Patcher

Post-build tool that patches compiled `.obj` files to fix register allocation mismatches.
Uses objdiff's instruction-level diff as an oracle to identify register swaps, then
directly modifies the register fields in the PowerPC instructions.

**Not run by default** — must be invoked manually after `ninja`.

```bash
# Dry run: show what would be patched (no changes)
python3 scripts/obj_regswap_patcher.py --batch

# Apply patches to .obj files
python3 scripts/obj_regswap_patcher.py --batch --apply

# Regenerate report to see patched progress (without rebuilding)
build/tools/objdiff-cli report generate -o build/45410914/report.json
python3 configure.py progress
```

Note: `ninja` will overwrite patched `.obj` files on the next rebuild, so the patcher
must be re-run after each build. The patcher auto-reverts any function where patching
causes a regression.

## objdiff MakeString Array-Size Normalization

Built into objdiff's `reloc_eq()` comparison (no separate tool needed). Automatically treats
`MakeString<char[N], int, char[M]>` template instantiations as equivalent regardless of N/M,
since arrays decay to pointers and produce identical machine code.

This resolves `bl` `diff_arg` mismatches caused by `__FILE__` string length differences
between the original build environment and ours. See
[../plans/MAKESTRING_ICF_EQUIVALENCE.md](../plans/MAKESTRING_ICF_EQUIVALENCE.md) for details.

**Impact:** +8.66pp fuzzy match (45.40% → 54.06%), +601 complete units.

## Quick Commands

```bash
# Build the project
ninja

# Generate progress report
ninja build/45410914/report.json

# Link hybrid PE (requires wine)
ninja link

# Find near-match functions (90-99%)
objdiff-cli report query build/45410914/report.json --functions --min-percent 90 --max-percent 99

# Check a specific function (markdown output is default)
objdiff-cli diff -p . "Game::Poll" --verdict

# Diff with context around mismatches
objdiff-cli diff -p . "Game::Poll" --verdict -C 3

# Check function info from report
objdiff-cli report function build/45410914/report.json "Game::Poll"

# Full analysis (replaces the removed ./bin/analyze-function)
#   mcp__orchestrator__run_analyze_function  symbol="Game::Poll"  project_dir="<worktree>"
# Source oracle instead of a decompiler reconstruction:
#   mcp__orchestrator__lookup_dc3 / lookup_rb3wii

# m2c pipeline (with jump table support) — the surviving m2c path,
# now that tools/decompile.sh is gone
./bin/objdiff-cli diff -p . "Foo::Bar" -f json --include-instructions | \
    python3 tools/objdiff_to_m2c.py --project-dir . | \
    python3 ~/code/milohax/m2c/m2c.py -t ppc -

# Generate decomp.me context
python3 tools/decompctx.py src/path/to/file.cpp -I include -I src
```

## Compiler Documentation

| Doc | Description |
|-----|-------------|
| [PRAGMA_INDEX.md](../decomp/PRAGMA_INDEX.md) | Xbox 360 compiler pragma documentation index |
| [PRAGMA_MATCHING_CHECKLIST.md](../decomp/PRAGMA_MATCHING_CHECKLIST.md) | Step-by-step guide for using pragmas to match functions |
| [PRAGMA_CODEGEN_SUMMARY.md](../decomp/PRAGMA_CODEGEN_SUMMARY.md) | Quick reference for pragma impact on code generation |
| [XBOX360_PRAGMA_REFERENCE.md](../decomp/XBOX360_PRAGMA_REFERENCE.md) | Complete technical reference for all code-generation pragmas |

**Key pragmas for matching:**
- `#pragma fp_contract(on|off)` - Controls fused multiply-add instruction generation (fmadds)
- `#pragma optimize("u", on|off)` - Controls prescheduling (instruction ordering)
- `#pragma bitfield_order(msb_to_lsb|lsb_to_msb)` - Controls bitfield packing order

## Archived Tools

| Tool | Description | Doc | Notes |
|------|-------------|-----|-------|
| decomp-permuter | Original C permutation fuzzer | [permuter.md](permuter.md) | C only, uses pycparser which doesn't support C++ |

## Projects

| Project | Description | Doc |
|---------|-------------|-----|
| VMX128 Ghidra Support | Adding Xbox 360 SIMD instruction support to Ghidra | [../vmx128/README.md](../vmx128/README.md) |

## External Resources

- [objdiff GUI](https://github.com/encounter/objdiff) - Visual diff tool
- [m2c online](https://simonsoftware.se/other/m2c.html) - Browser-based m2c
- [decomp.me](https://decomp.me) - Collaborative decompilation scratches
