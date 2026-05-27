# Ghidra Setup for rb3-xenon

> **Note for rb3-xenon:** There is no equivalent leaked .map file for RB3. `tools/fingerprint_match.py` fills this role for function identification (string/callee cross-referencing). The "Symbol Lookup via Map File" and "Type Seeding Pipeline" sections reference DC3's `ham_xbox_r.map` which does not exist for RB3; skip those steps.

Ghidra provides binary analysis and decompilation for the original RB3 binary. Integrated via pyghidra-mcp for AI-assisted workflows.

## Prerequisites

### Java Setup

Add to `~/.profile`:

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk
export GHIDRA_INSTALL_DIR=/opt/ghidra
export PATH="$GHIDRA_INSTALL_DIR/support:$PATH"
```

### XEX Loader Extension

Xbox 360 executables require XEXLoaderWV. See [XEXLOADERWV.md](XEXLOADERWV.md) for build/install instructions.

## pyghidra-mcp Service

### Architecture

The pyghidra-mcp service provides Ghidra integration via the Model Context Protocol (MCP). The service uses **FastMCP** for transport, enabling AI-assisted binary analysis.

**Key Components:**
- **Transport**: FastMCP with Uvicorn HTTP server
- **Port**: 8002 (rb3-xenon configured port)
- **Version**: pyghidra-mcp 0.1.6+
- **XEX Support**: Automatic detection for Xbox 360 binaries

### Environment Variables

Critical environment configuration for XEX support:

```bash
# Java environment
export JAVA_HOME="/usr/lib/jvm/java-17-openjdk"

# VMX128-enabled Ghidra fork (not stock /opt/ghidra)
# After cloning freeqaz/ghidra, build with: gradle buildGhidra
# Then extract the zip from build/dist/ into ghidra-install/
export GHIDRA_INSTALL_DIR="../ghidra/build/ghidra"  # relative to rb3-xenon

# Writable temp directory for Ghidra user home (avoids read-only filesystem issues)
export GHIDRA_USER_HOME="/tmp/claude/ghidra_user"
```

**Important**: `GHIDRA_USER_HOME` must be set to a writable directory to avoid "Read-only file system" errors when importing binaries.

### Service CLI Parameters

The new pyghidra-mcp uses simplified parameters:

```bash
pyghidra-mcp [OPTIONS] BINARY_PATH

Required:
  BINARY_PATH               Path to the binary to analyze

Options:
  --transport TYPE          Transport type (stdio, streamable-http)
  --project-name NAME       Ghidra project name
  --project-directory PATH  Directory containing Ghidra project
```

**Removed Parameters** (from older versions):
- `--port` - now controlled by transport (default 8000 for Uvicorn; rb3-xenon uses 8002)
- `--host` - bound to 127.0.0.1 automatically
- `--wait-for-analysis` - automatic in new version
- `--no-force-analysis` - automatic in new version
- `--project-path` - replaced by `--project-directory`

### Starting the Service

Use the service wrapper script:

```bash
# Start service
./tools/ghidra/pyghidra-service.sh start

# Check status
./tools/ghidra/pyghidra-service.sh status

# View logs
./tools/ghidra/pyghidra-service.sh logs

# Stop service
./tools/ghidra/pyghidra-service.sh stop

# Restart service
./tools/ghidra/pyghidra-service.sh restart
```

**Service Details:**
- Runs on `http://127.0.0.1:8002`
- PID file: `/tmp/claude/pyghidra-mcp-rb3.pid`
- Log file: `/tmp/claude/pyghidra-mcp-rb3.log`
- Project: `ghidra_projects/RB3Xenon`

Expected output when starting:
```
Starting pyghidra-mcp service...
  Project: /home/free/code/milohax/rb3-xenon/ghidra_projects/RB3Xenon
  Binary: /home/free/code/milohax/rb3-xenon/orig/45410914/default.xex
  Log: /tmp/claude/pyghidra-mcp-rb3.log
Started with PID: 12345

Note: The new pyghidra-mcp version uses FastMCP transport.
Server is starting in the background...
Check logs with: ./tools/ghidra/pyghidra-service.sh logs
Service process is running (PID: 12345)
```

### XEX Binary Support

The service now includes **automatic XEX detection**:

**Features:**
- Detects XEX2 magic number (`0x58455832`) in binary header
- Automatically selects `PowerPC:BE:64:Xenon` language specification
- Uses XEXLoaderWV extension for proper Xbox 360 binary loading
- No manual language specification required

**Detection Log Messages:**
```
INFO:pyghidra_mcp.server:Detected XEX binary, using language: PowerPC:BE:64:Xenon
INFO:pyghidra_mcp.context:Importing new program: default.xex-997567
INFO:pyghidra_mcp.context:Analysis for default.xex-997567 complete
INFO:pyghidra_mcp.context:Analysis % complete: 100.0
```

**Supported Architectures:**
- PowerPC:BE:64:Xenon (Xbox 360)
- Auto-detection for ELF and PE formats as well

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

## MCP Tools

Configured in `.mcp.json`. The MCP server provides these tools:

### Analysis Tools

| Tool | Description |
|------|-------------|
| `decompile_function` | Decompile function to pseudo-C by name or address |
| `search_symbols_by_name` | Search symbols (case-insensitive substring) |
| `search_code` | Semantic search over decompiled code |
| `list_cross_references` | Find xrefs to/from function or address |
| `gen_callgraph` | Generate mermaid.js call graph |

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

## Troubleshooting

| Error | Cause | Fix |
|-------|-------|-----|
| "No load spec found" | XEX loader not installed | See [XEXLOADERWV.md](XEXLOADERWV.md) |
| "Analysis incomplete" | Large binary still analyzing | Wait and retry |
| "Function not found" | Binary is stripped | Use address from map file |
| Binary shows as x86/DOS | XEX loader not working | Reinstall extension |
| "Failed to init global cache" | `/var/tmp` not writable | Check permissions |
| "Read-only file system" | GHIDRA_USER_HOME not writable | Set `GHIDRA_USER_HOME=/tmp/claude/ghidra_user` |
| Service won't start | Port 8002 already in use | Check for existing service, use `stop` first |
| "Connection refused" | Service not running | Check logs, restart service |

### Debugging Service Issues

```bash
# Check logs for errors
tail -50 /tmp/claude/pyghidra-mcp-rb3.log

# Check if process is running
ps aux | grep pyghidra

# Check if port 8002 is in use
lsof -i :8002

# Try manual start to see errors
source venv/bin/activate
pyghidra-mcp --transport streamable-http \
    --project-name "RB3Xenon" \
    --project-directory "ghidra_projects/RB3Xenon" \
    orig/45410914/default.xex
```

## Version Information

- **pyghidra-mcp**: 0.1.6+ (with FastMCP support)
- **Ghidra**: 12.0 DEV (with VMX128 extensions)
- **XEXLoaderWV**: Custom build for Xbox 360 support
- **Transport**: FastMCP over Uvicorn HTTP server
- **Port**: 8002 (rb3-xenon configured port)

## See Also

- [XEXLOADERWV.md](XEXLOADERWV.md) - XEX loader build/install
- [PYGHIDRA_MCP_XEX_SUPPORT.md](../plans/PYGHIDRA_MCP_XEX_SUPPORT.md) - XEX integration details
- [GHIDRA_MCP_INTEGRATION.md](../plans/GHIDRA_MCP_INTEGRATION.md) - MCP workflow strategies
- [objdiff.md](objdiff.md) - Assembly comparison workflow
- [INDEX.md](INDEX.md) - Quick command reference
