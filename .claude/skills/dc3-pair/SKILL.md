---
name: dc3-pair
description: Find DC3 source for a given symbol/string. DC3 is the closest source oracle for rb3-xenon (same compiler, same flags, same engine version family). Use this BEFORE rb3wii-pair when looking for engine code; use rb3wii-pair for game-code logic that DC3 doesn't share.
argument-hint: "[unit-name-or-symbol] [--source]"
allowed-tools: Bash(python3 -c *), Read, Grep, Glob
---

# DC3 Pair Skill

Find the corresponding DC3 (Dance Central 3) source file for an rb3-xenon unit and show it as a port reference.

DC3 is the **closest source oracle** for rb3-xenon:
- Same MSVC X360 compiler (`/O1 /Oi /GR /EHsc`)
- Same Milo engine family (same era, same RTL/XDK headers)
- Already 360-ported — uses `RTL_CRITICAL_SECTION`, `xdk/XBOXKRNL.h`, etc.
- DC3's leaked PDB (`ham_xbox_r.map`) gives named functions — the asymmetric advantage

**Use DC3 BEFORE rb3-Wii for engine code** (`src/system/`). Use `/rb3wii-pair` for game code (`src/band3/`, `src/network/`) that DC3 doesn't share.

## Arguments

`$ARGUMENTS` - A unit name or symbol (e.g., `CharBones`, `system/char/CharBones`, `RndWind::Handle`). Add `--source` to include the full DC3 source code.

## Steps

1. **Parse the arguments.** `$0` is a unit name or symbol. Check if `--source` flag is present.

2. **Look up the DC3 source file.** Try the MCP tool first (fastest):
   ```
   mcp__orchestrator__lookup_dc3
   ```
   with query = `$0`.

   If the MCP tool isn't available, fall back to filesystem search:
   ```bash
   python3 -c "
   import sys
   from pathlib import Path

   unit = '$0'
   dc3_root = Path.home() / 'code/milohax/dc3-decomp/src'

   # Normalize: strip path prefixes, try direct match
   name = unit.split('/')[-1]
   for pattern in [f'**/{name}.cpp', f'**/{name}.h', f'**/{name.lower()}.cpp']:
       matches = list(dc3_root.glob(pattern))
       if matches:
           for m in matches:
               print(f'DC3 File: {m}')
           break
   else:
       print(f'No DC3 file found for: {unit}')
   "
   ```

3. **If `--source` is requested and a file was found**, read the DC3 source file with the Read tool.

4. **Caveat**: DC3 is *newer* than RB3. Engine code may have subtle behavioral differences. When a file misbehaves, cross-check against rb3-Wii's equivalent (`/rb3wii-pair`) and merge intent — do not assume DC3's version is correct for RB3.

## Tips

- The DC3 source is at `~/code/milohax/dc3-decomp/src/`
- Use `mcp__orchestrator__lookup_dc3` MCP tool to grep DC3 for specific symbols or strings
- DC3 engine files are in `~/code/milohax/dc3-decomp/src/system/` — mirrors `src/system/` here
- For Milo engine classes, DC3 is the direct port source: copy → adapt → match
- DC3's `objects.json` uses lowercase variants for some files (`vec.cpp`, `mtx.cpp`) — check casing before assuming
- DC3's `ham_xbox_r.map` named functions are the Rosetta Stone: same binary layout as rb3-xenon `fn_8XXXXXXX`
