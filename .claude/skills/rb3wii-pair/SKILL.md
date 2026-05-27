---
name: rb3wii-pair
description: Get rb3-Wii source file pairing info for an rb3-xenon unit. Shows compatibility score, function overlap, and optionally the rb3-Wii source code. Use to leverage rb3-Wii reference implementations for shared RB3 game code (band3/, network/) that DC3 doesn't share.
argument-hint: "[unit-name] [--source]"
allowed-tools: Bash(python3 -c *), Read, Grep, Glob
---

# RB3-Wii Pair Skill

Find the corresponding rb3-Wii source file for an rb3-xenon unit and show compatibility info.

## Arguments

`$ARGUMENTS` - A unit name (e.g., `BandDirector`, `band3/BandDirector`). Add `--source` to include the full rb3-Wii source code.

## When to Use vs `/dc3-pair`

- **Use `/dc3-pair` first** for engine code (`src/system/`) — DC3 is the same MSVC/X360 compiler and flags, so its code is a direct port target.
- **Use `/rb3wii-pair`** for RB3 game code (`src/band3/`, `src/network/`) that DC3 doesn't share — rb3-Wii is the Wii dev build with MILO_ASSERT source-path strings and named functions, giving richer cross-binary identification.

## Steps

1. **Parse the arguments.** `$0` is a unit name. Check if `--source` flag is present.

2. **Look up the rb3-Wii pairing:**
   ```bash
   python3 -c "
   import sys, json
   sys.path.insert(0, 'scripts')
   from orchestrator.rb3_pairing import find_rb3_file
   from pathlib import Path

   unit = '$0'
   rb3_root = Path.home() / 'code/milohax/rb3/src'

   # Try finding directly
   rb3_file = find_rb3_file(unit, rb3_root)
   if rb3_file:
       print(f'RB3-Wii File: {rb3_file}')
   else:
       print(f'No rb3-Wii file found for: {unit}')
   "
   ```

3. **If `--source` is requested and a file was found**, read the rb3-Wii source file with the Read tool.

4. **For function overlap**, also check the database:
   ```bash
   python3 -c "
   import sys
   sys.path.insert(0, 'scripts')
   from orchestrator.database import get_connection

   unit = '$0'
   conn = get_connection('decomp.db')
   # Try various unit path formats
   for prefix in ['', 'default/', 'default/system/']:
       row = conn.execute(
           'SELECT rb3_file, rb3_score FROM units WHERE unit = ?',
           (prefix + unit,)
       ).fetchone()
       if row:
           print(f'Unit: {prefix + unit}')
           print(f'RB3-Wii File: {row[0]}')
           print(f'Score: {row[1]}')
           break
   else:
       print('Not found in units table')
   "
   ```

## Tips

- The rb3-Wii source is at `~/code/milohax/rb3/src/`
- Use `mcp__orchestrator__lookup_rb3` MCP tool to grep rb3-Wii for specific symbols
- rb3-Wii is the *Wii dev* build — needs Wii→360 porting (MWCC→MSVC, `revolution/OS.h`→`xdk/XBOXKRNL.h`)
- Compatibility score indicates how much the code overlaps
- For engine code, prefer `/dc3-pair` (same compiler, already 360-ported)
