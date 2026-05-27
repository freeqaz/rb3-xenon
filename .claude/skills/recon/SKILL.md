---
name: recon
description: Run full reconnaissance on a function before starting work. Shows match%, unicorn behavioral verdict, divergence class, struct field access map, and workability assessment. Use before decomp work to understand what you're dealing with.
argument-hint: "[symbol-or-function]"
allowed-tools: Bash(python3 scripts/recon.py *), Read, Grep, Glob
---

# Recon Skill

Run unified function reconnaissance to get a full picture of a function's state
before starting decomp work.

## Arguments

`$ARGUMENTS`

## Steps

1. **Resolve the function.** The argument `$0` can be:
   - A mangled symbol (e.g., `?Seek@AsyncFile@@UAAHHH@Z`)
   - A qualified C++ name (e.g., `AsyncFile::Seek`)
   - A partial name (e.g., `Seek`) — search decomp.db to find the best match

2. **Find the mangled symbol** if not already mangled:
   ```bash
   python3 -c "
   import sqlite3, json
   conn = sqlite3.connect('decomp.db')
   conn.row_factory = sqlite3.Row
   row = conn.execute('''
       SELECT symbol, demangled, unit, current_percent, verdict
       FROM functions WHERE symbol = ? OR demangled LIKE ?
       ORDER BY current_percent DESC LIMIT 1
   ''', ('$0', '%$0%')).fetchone()
   if row:
       print(json.dumps(dict(row)))
   else:
       print('NOT FOUND')
   "
   ```

3. **Run recon:**
   ```bash
   python3 scripts/recon.py --symbol 'MANGLED_SYMBOL'
   ```

   This produces a unified report with:
   - **DB status**: match%, verdict, attempt count, priority score
   - **Unicorn verdict**: behavioral equivalence (EQUIVALENT/DIVERGENT)
   - **Divergence class**: build_env, regalloc, or logic
   - **Field access map**: which struct offsets the function reads/writes
   - **Assessment**: actionable recommendation (work on it, skip it, AT_LIMIT, etc.)

4. **Present the results** to the user clearly. Key things to call out:
   - If the function is AT_LIMIT or excluded, say so immediately
   - If unicorn says EQUIVALENT but asm doesn't match, it's register allocation
   - If the divergence class is `build_env`, it's unfixable from source
   - Field access map shows what the function actually does structurally

## Flags

- Add `--no-unicorn` to skip the live behavioral comparison (faster, uses cached data only)
- Add `--no-field-access` to skip field access probing
- Add `--json` for machine-readable output

## Tips

- Always run recon before spending time on a function — it takes seconds and saves hours
- Functions with unicorn EQUIVALENT + <100% match are register allocation issues
- Functions with `build_env` class divergence are __FILE__ or merged symbol differences
- The field access map helps understand what struct members to focus on when debugging offset mismatches
