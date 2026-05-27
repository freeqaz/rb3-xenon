---
name: ghidra-decompile
description: Decompile and analyze a function from Ghidra. Shows decompiled C code, switch statements, and cast operations. Use when investigating function structure or debugging type issues. Automatically resolves symbol names to correct functions. Need to skip sandbox to access Ghidra over network.
argument-hint: "[function-name-or-address]"
allowed-tools: Bash(python3 tools/ghidra/pcode_inspect.py *)
---

# Ghidra Decompile

Decompile a function and analyze its structure for switch statements, casts, and type operations.

NOTE: You must skip the sandbox to talk to localhost. Ghidra is likely already running but you will see a failure if you use the sandbox's proxy!

## Arguments

`$ARGUMENTS`

## Usage

```bash
python3 tools/ghidra/pcode_inspect.py "FUNCTION" [--switches] [--casts]
```

The Ghidra MCP server for rb3-xenon runs on port **8002**.

## Steps

1. **Resolve the function** - The argument can be:
   - A qualified C++ name (e.g., `RndWind::Handle`)
   - A mangled symbol (e.g., `?Handle@RndWind@@UAA?AVDataNode@@PAVDataArray@@_N@Z`)
   - An address (e.g., `0x826703b0`)

2. **Run decompilation:**
   ```bash
   python3 tools/ghidra/pcode_inspect.py "FunctionName"
   ```

3. **For switch analysis only:**
   ```bash
   python3 tools/ghidra/pcode_inspect.py "FunctionName" --switches
   ```

4. **For cast/type analysis only:**
   ```bash
   python3 tools/ghidra/pcode_inspect.py "FunctionName" --casts
   ```

5. **Present results** including:
   - Decompiled C code from Ghidra
   - Switch statement locations and case counts
   - Cast operations (signed/unsigned extensions)

## Ghidra Project

- Project path: `ghidra_projects/RB3Xenon/RB3Xenon`
- Binary: `orig/45410914/default.xex`
- MCP port: **8002**

## What It Detects

### Switch Statements
- Jump table addresses
- Number of cases
- Comparison register used

### Cast Operations
- `INT_SEXT` - Signed extension (extsb, extsh, extsw)
- `INT_ZEXT` - Zero extension (rlwinm masks)
- Sub-register extracts (SUB41, etc.)
- Register concatenation (CONCATnn)

## Tips

- The tool searches symbols first to find the correct function (skips thunks)
- Use `--address` flag to treat input as raw address (skip symbol search)
- Use `--no-decompile` to only analyze raw bytes
- Large functions (>3000 bytes) may take longer to analyze
- Skip the sandbox for network operations if you notice Ghidra is unavailable. It's likely already running.
