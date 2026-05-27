# Ghidra Setup for rb3-xenon

Ghidra provides binary analysis and decompilation for the original RB3 binary. Integrated via pyghidra-mcp (v0.1.6+) for AI-assisted workflows.

## Prerequisites

### Java Setup

Add to `~/.profile`:

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
export GHIDRA_INSTALL_DIR=/opt/ghidra
export GHIDRA_USER_HOME=/tmp/claude/ghidra_user
export PATH="$GHIDRA_INSTALL_DIR/support:$PATH"
```

**`GHIDRA_USER_HOME`**: Points to a writable directory for Ghidra's user settings and cache. Required when running in environments with read-only home directories or sandboxed processes. Avoids permission errors when Ghidra tries to write to `~/.ghidra/`.

### XEX Loader Extension

Xbox 360 executables require XEXLoaderWV. The extension must be installed in `$GHIDRA_INSTALL_DIR/Extensions/Ghidra/`. See [XEXLOADERWV.md](XEXLOADERWV.md) for build/install instructions and [PYGHIDRA_MCP_XEX_SUPPORT.md](../plans/PYGHIDRA_MCP_XEX_SUPPORT.md) for technical details on XEX handling.

## Symbol Lookup via Map File

**Important:** The binary is stripped - Ghidra won't have function names. Use the linker map file as the primary symbol source:

```bash
# Map file location
orig/45410914/ham_xbox_r.map    # 119K lines of symbols

# Find function address
grep "GetPresenceMode" orig/45410914/ham_xbox_r.map
# Output: 0005:00548b58  ?GetPresenceMode@PresenceMgr@@...  82878b58 f  game:PresenceMgr.obj
#                                                          ^^^^^^^^
#                                                          Use this address in Ghidra

# Then decompile by address in Ghidra MCP:
# decompile_function(binary_name, "0x82878b58")
```

### Map File Format

```
Section:Offset    MangledName                    Address    Type  Source
0005:00548b58     ?GetPresenceMode@PresenceMgr@@ 82878b58   f     game:PresenceMgr.obj
```

- `0005:` = .text section (code)
- `f` = function, `i` = inlined
- Address is absolute (base 0x82000000)

## Headless Analysis

Pre-analyze the RB3 binary (one-time, ~4 minutes):

```bash
/opt/ghidra/support/analyzeHeadless /tmp/pyghidra_mcp_projects/rb3-xenon rb3-xenon \
    -import orig/45410914/default.xex -max-cpu 4
```

## XEX Binary Support

pyghidra-mcp (v0.1.6+) includes automatic Xbox 360 XEX binary detection and handling:

- **Automatic Detection**: Recognizes XEX2 magic number (`0x58455832`) in binary headers
- **Language Specification**: Auto-sets `PowerPC:BE:64:Xenon` for XEX files
- **XEXLoaderWV Integration**: Uses extension from `$GHIDRA_INSTALL_DIR/Extensions/` for import
- **No Manual Configuration**: XEX files are handled transparently by pyghidra-mcp

See [PYGHIDRA_MCP_XEX_SUPPORT.md](../plans/PYGHIDRA_MCP_XEX_SUPPORT.md) for implementation details.

## pyghidra-mcp (MCP Integration)

Configured in `.mcp.json`. The MCP server runs on **port 8002** (default) using FastMCP with Uvicorn transport. The server provides these tools:

### Analysis Tools

| Tool | Description |
|------|-------------|
| `decompile_function` | Decompile function to pseudo-C by name or address |
| `search_symbols_by_name` | Search symbols (case-insensitive substring) |
| `search_code` | Semantic search over decompiled code |
| `list_cross_references` | Find xrefs to/from function or address |
| `gen_callgraph` | Generate mermaid.js call graph |

### Type Seeding Tools

| Tool | Description |
|------|-------------|
| `bulk_create_functions` | Create function objects at addresses where auto-analysis missed them |
| `apply_demangled_signatures` | Apply full signatures (CC, return type, all params) from MSVC mangled names |
| `create_structures` | Create struct types in DTM from struct_db definitions |
| `apply_this_types` | (Legacy) Set `this*` parameter only on member functions |

**Current limitation:** this pipeline improves function signatures and field layouts, but it does **not** yet recover or apply MSVC RTTI (`??_R0`..`??_R4`) into explicit class hierarchies. That work is planned in [PYGHIDRA_MCP_RTTI_RECOVERY.md](../plans/PYGHIDRA_MCP_RTTI_RECOVERY.md).

### Binary Inspection

| Tool | Description |
|------|-------------|
| `list_project_binaries` | Show loaded binaries and analysis status |
| `list_project_binary_metadata` | Architecture, format, hash info |
| `list_exports` / `list_imports` | Symbol tables (with regex filter) |
| `search_strings` | Find strings in binary |
| `read_bytes` | Read raw memory at address |

### Project Management

| Tool | Description |
|------|-------------|
| `import_binary` | Load new binary into project |
| `delete_project_binary` | Remove binary from project |

## Workflow: Decompiling Unknown Functions

1. **Find address from map file:**
   ```bash
   grep "YourFunction" orig/45410914/ham_xbox_r.map
   ```

2. **Decompile in Ghidra MCP:**
   ```
   decompile_function("/default.xex", "0x82XXXXXX")
   ```

3. **Find callers/callees:**
   ```
   list_cross_references("/default.xex", "0x82XXXXXX")
   ```

4. **Generate call graph:**
   ```
   gen_callgraph("/default.xex", "FUN_82XXXXXX", direction="calling")
   ```

## MCP Server Configuration

### Service Architecture

pyghidra-mcp (v0.1.6+) uses **FastMCP with Uvicorn transport** instead of the legacy custom SSE mode:

- **Transport**: Streamable HTTP (Uvicorn)
- **Port**: 8002 (rb3-xenon configured port)
- **Protocol**: MCP over HTTP streams
- **Benefits**: Better error handling, graceful shutdown, standard HTTP tooling

### Startup Command

```bash
pyghidra-mcp \
    --transport streamable-http \
    --project-name rb3-xenon \
    --project-directory /tmp/pyghidra_mcp_projects/rb3-xenon
```

**Key Parameters:**
- `--transport streamable-http`: Use Uvicorn transport (required for MCP server mode)
- `--project-name`: Ghidra project name (creates if doesn't exist)
- `--project-directory`: Directory for Ghidra project files

**Removed Parameters** (from older versions):
- `--port`, `--host`: Now handled by FastMCP/Uvicorn defaults
- `--wait-for-analysis`, `--no-force-analysis`: Analysis is automatic on import

### Environment Variables

Required for proper operation:

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra           # Ghidra installation
export GHIDRA_USER_HOME=/tmp/claude/ghidra_user # User settings/cache (writable!)
```

## Type Seeding Pipeline

Seeds Ghidra with type information from the linker map file, enabling the decompiler to show field names, calling conventions, and full parameter types instead of raw offsets.

### Running the Pipeline

```bash
# New pipeline (recommended): bulk function creation + demangled signatures
python3 tools/ghidra/batch_export_types.py --seed

# Legacy pipeline: struct creation + this-pointer only
python3 tools/ghidra/batch_export_types.py --seed --legacy

# Full pipeline: seed + extract enriched types back to struct_db
python3 tools/ghidra/batch_export_types.py --seed --extract
```

### What the Pipeline Does

| Step | Tool | Effect |
|------|------|--------|
| 1. Parse map | local | Extracts ~79K CODE-section symbols from `ham_xbox_r.map` |
| 2. Bulk create functions | `bulk_create_functions` | Creates ~27K function objects where auto-analysis missed them |
| 3. Demangle signatures | `apply_demangled_signatures` | Applies full signatures to ~53K functions (CC, return type, all params) |
| 4. Supplementary structs | `create_structures` | Creates ~2.1K struct layouts from struct_db |

### Before/After

| Metric | Before (legacy) | After (demangled) |
|--------|-----------------|-------------------|
| Functions in Ghidra | ~42,500 | ~69,400 |
| Signatures applied | ~7,000 (this-only) | ~53,000 (full) |
| "Function not found" rate | ~40% | ~0.01% |
| Calling conventions set | 0 | ~53,000 |
| Parameter types | `this*` only | All parameters |

### What The Pipeline Does Not Yet Do

- Parse RTTI descriptors (`??_R0`..`??_R4`) from program memory
- Reconstruct base-class graphs from RTTI
- Associate recovered classes with `??_7*` vtables automatically
- Apply RTTI structs/comments back into the Ghidra project

For that missing work, use the implementation plan in [PYGHIDRA_MCP_RTTI_RECOVERY.md](../plans/PYGHIDRA_MCP_RTTI_RECOVERY.md).

### When to Re-seed

Re-run `--seed` after:
- Importing a new binary into Ghidra
- Restarting the pyghidra-mcp service with a fresh project
- Updating struct_db with new header-sourced types

The `--extract` step (batch decompilation) benefits from seeding — with full signatures, the decompiler produces much higher-quality inferred types.

## Service Management

The pyghidra-mcp service is managed via `tools/ghidra/pyghidra-service.sh`:

```bash
# Start (auto-cleans stale port, starts logging)
./tools/ghidra/pyghidra-service.sh start

# Stop (kills process, removes PID file, clears stale Ghidra locks)
./tools/ghidra/pyghidra-service.sh stop

# Check if running
./tools/ghidra/pyghidra-service.sh status

# Restart (clean stop + start, cleans stale port)
./tools/ghidra/pyghidra-service.sh restart

# Live tail of logs
./tools/ghidra/pyghidra-service.sh logs

# Run diagnostics (checks Ghidra install, Java, port, permissions)
./tools/ghidra/pyghidra-service.sh diagnose
```

### Hardening Features

- **Port cleanup**: On startup, kills stale processes occupying port 8002 (via `lsof`). Waits up to 5 seconds.
- **Health check**: `get_service_health` MCP tool returns status, uptime, version, and Ghidra readiness. Called automatically by `analyze-function` before analysis.
- **Logging**: Rotating file handler at `/tmp/claude/pyghidra-service.log` (10 MB max, 10 backup files). Logs startup, shutdown, port cleanup, decompilation requests, errors.
- **Diagnostics**: `--diagnose` flag checks Ghidra installation, Java, port status, temp directories, and recent log entries.

## Troubleshooting

### Common Issues (v0.1.6+)

| Error | Cause | Fix |
|-------|-------|-----|
| "No load spec found" | XEX loader not installed | See [XEXLOADERWV.md](XEXLOADERWV.md) |
| "Analysis incomplete" | Large binary still analyzing | Wait and retry |
| "Function not found" | Binary is stripped or no function object | Use address from map file; run `--seed` to bulk-create functions |
| Binary shows as x86/DOS | XEX loader not working | Reinstall extension to `$GHIDRA_INSTALL_DIR/Extensions/` |
| "Failed to init global cache" | `/var/tmp` not writable | Check permissions |
| Service won't start on port 8002 | Port already in use | Check `lsof -i :8002`, kill conflicting process, or change port |
| Permission denied on GHIDRA_USER_HOME | Directory not writable | Ensure `$GHIDRA_USER_HOME` points to writable location (e.g., `/tmp/claude/ghidra_user`) |
| XEX files not recognized | Missing XEXLoaderWV extension | Install to `$GHIDRA_INSTALL_DIR/Extensions/Ghidra/` |
| "Connection refused" to MCP server | Server not running or wrong port | Verify server started, check port 8002 (not 8765) |

### Debug Steps

1. **Verify environment variables:**
   ```bash
   echo $GHIDRA_INSTALL_DIR
   echo $GHIDRA_USER_HOME
   ```

2. **Check XEX extension:**
   ```bash
   ls -la $GHIDRA_INSTALL_DIR/Extensions/Ghidra/ | grep XEX
   ```

3. **Test port availability:**
   ```bash
   lsof -i :8002
   ```

4. **Check GHIDRA_USER_HOME permissions:**
   ```bash
   mkdir -p $GHIDRA_USER_HOME
   touch $GHIDRA_USER_HOME/test && rm $GHIDRA_USER_HOME/test
   ```

## CLI Analysis Tools

Standalone scripts that query the running pyghidra-mcp server. Require service to be running and DTM seeded (see [Setup](#prerequisites) and [Type Seeding Pipeline](#type-seeding-pipeline)).

**Skill shortcuts:** These tools are also available as Claude Code skills:
- `/ghidra-struct` — Struct validation
- `/ghidra-decompile` — Pcode inspection
- `/ghidra-search` — Semantic code search

### Struct Validation — `tools/ghidra/struct_check.py`

Compares our C++ header struct layouts (from `struct_db.sqlite`) against Ghidra's DTM. Catches offset misalignments, missing fields, and size discrepancies.

```bash
# Compare a single class
python3 tools/ghidra/struct_check.py HamDirector

# Compare all classes referenced in a translation unit
python3 tools/ghidra/struct_check.py --unit system/char/CharBones

# Compare multiple classes
python3 tools/ghidra/struct_check.py HamDirector RndPostProc BaseMaterial

# JSON output (for CI/scripting)
python3 tools/ghidra/struct_check.py --json HamDirector
```

Output shows field-by-field comparison with OK / GHIDRA_MISSING / OURS_MISSING status. Exit code 1 if any mismatches found.

**When to use:** After changing class layouts. When objdiff shows offset mismatches across many functions in the same class. Before submitting PRs that modify headers.

**Note:** After seeding, data persists to disk and survives restarts. Only need to seed once (or after importing a new binary).

### Pcode Inspection — `tools/ghidra/pcode_inspect.py`

Analyzes Ghidra decompilation output + raw PPC bytes for switch tables and cast operations. Automatically resolves symbol names to the correct function (skips thunks).

```bash
# Full analysis (switches + casts + decompiled output)
python3 tools/ghidra/pcode_inspect.py "Hmx::Object::Handle"

# By address (skip symbol search)
python3 tools/ghidra/pcode_inspect.py "0x82878b58" --address

# Switch statements only
python3 tools/ghidra/pcode_inspect.py "DataNode::Handle" --switches

# Cast/extension operations only
python3 tools/ghidra/pcode_inspect.py "DataNode::Handle" --casts
```

Detects:
- **Switch patterns**: PPC jump tables (cmplwi/lwzx/mtctr/bctr) and comparison chains, with case counts
- **Cast operations**: PPC sign/zero extensions (extsb, extsh, extsw, rlwinm), Ghidra cast patterns ((int), (uint), SUBn(), SEXTn(), ZEXTn())

**When to use:** When a function has switch statements that don't match. When objdiff shows branch structure differences. When signed/unsigned confusion is suspected.

### Semantic Code Search — `tools/ghidra/code_search.py`

Vector search over all 42K+ decompiled functions via ChromaDB. Find functions with similar patterns by natural language description or code snippet. Automatically filters noise (`__unwind$`, `__ehhandler$`, etc.).

```bash
# Search by description
python3 tools/ghidra/code_search.py "iterate list and delete each element"

# Search by code pattern
python3 tools/ghidra/code_search.py --code "for (i = 0; i < count; i++) { arr[i]->Save(bs); }"

# Search string literals
python3 tools/ghidra/code_search.py --strings "CharBones"

# Limit results
python3 tools/ghidra/code_search.py "poll timer update" --limit 5

# Add custom exclude patterns
python3 tools/ghidra/code_search.py "wind" --exclude "thunk" --exclude "vtable"

# Disable default noise filtering
python3 tools/ghidra/code_search.py "query" --no-default-exclude
```

Returns ranked results with function name, address, similarity score, and full decompiled code.

**When to use:** Starting work on a new function and want to see similar implementations. Looking for how the codebase handles a pattern (null checks, container iteration, error paths). Finding string references.

**Note:** ChromaDB indexes on first startup (~10 min). If results are empty, the index may need rebuilding — delete `ghidra_projects/RB3Xenon/chromadb/` and restart.

## See Also

- **[GHIDRA_MANUAL_SETUP.md](GHIDRA_MANUAL_SETUP.md) - GUI-only setup guide (no MCP server needed)**
- [XEXLOADERWV.md](XEXLOADERWV.md) - XEX loader build/install
- [objdiff.md](objdiff.md) - Assembly comparison workflow
- [INDEX.md](INDEX.md) - Quick command reference
