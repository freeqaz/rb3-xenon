---
name: ghidra-struct
description: Check struct/class layout against Ghidra's Data Type Manager. Compares rb3-xenon header definitions with Ghidra's analysis to find offset mismatches. Use when debugging struct alignment issues or verifying class layouts.
argument-hint: "[class-name ...]"
allowed-tools: Bash(python3 tools/ghidra/struct_check.py *)
---

# Ghidra Struct Check

Compare rb3-xenon header struct definitions against Ghidra's Data Type Manager.

## Arguments

`$ARGUMENTS` - One or more class names to check

## Usage

```bash
python3 tools/ghidra/struct_check.py ClassName [ClassName2 ...]
```

The Ghidra MCP server for rb3-xenon runs on port **8002**.

## Steps

1. **Run the check:**
   ```bash
   python3 tools/ghidra/struct_check.py BandDirector RndWind
   ```

2. **Review output** showing:
   - Offset comparison table
   - Field names from both sources
   - Status: OK or MISMATCH

3. **Investigate mismatches** by:
   - Checking header file for the class
   - Looking for missing padding or inheritance issues
   - Comparing with DC3 via `/dc3-pair` if available

## Output Format

```
======================================================================
  BandDirector
======================================================================
  Offset     Our Field                 Ghidra Field              Status
  ---------- ------------------------- ------------------------- ----------
  0x0048     unk48                     unk48                     OK
  0x005c     mSongAnims                mSongAnims                OK
  0x0150     mWorldPostProc            mWorldPostProc            OK
```

## Ghidra Project

- Project path: `ghidra_projects/RB3Xenon/RB3Xenon`
- Binary: `orig/45410914/default.xex`
- MCP port: **8002**

## Tips

- Check multiple classes at once for efficiency
- Use with `/dc3-pair` to get DC3 source as reference (same MSVC compiler, same flags)
- Mismatches often indicate:
  - Missing base class members
  - Wrong member order
  - Incorrect padding
  - Wrong type sizes

## When to Use

- Before fixing offset mismatches in objdiff
- When adding new class members
- After making struct layout changes
- To verify inheritance chain is correct
