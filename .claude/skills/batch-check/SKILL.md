---
name: batch-check
description: Batch-check all untracked functions in a unit. Runs objdiff on each, auto-reports 100% matches as COMPLETE. Returns summary with counts and partial-match details. Use this instead of manual query+objdiff+report loops.
argument-hint: "[unit-pattern] [--dry-run] [--skip-boilerplate]"
allowed-tools: Bash(python3 scripts/batch_check.py *), Read, Grep, Glob
---

# Batch Check Skill

Sweep an entire unit (or glob of units) for already-matching functions and auto-mark them
COMPLETE in the database. Replaces the tedious query+objdiff+report loop.

## Arguments

`$ARGUMENTS`

## Steps

1. **Parse the arguments.** `$0` is the unit pattern. It can be:
   - A full unit path: `default/system/char/CharBones`
   - A glob pattern: `default/system/char/*`
   - A short name: `system/char/*` (the script auto-normalizes)

2. **Run the batch check:**
   ```bash
   python3 scripts/batch_check.py '$0'
   ```
   - Add `--dry-run` if the user wants to preview without updating the DB
   - Add `--skip-boilerplate` to skip atexit/MakeString/thunks

3. **Present the results.** The script returns:
   - **Checked**: total functions diffed
   - **Newly COMPLETE**: functions that matched 100% and were marked COMPLETE
   - **Partial**: functions with >0% but <100% match (listed with percentages)
   - **Unimplemented**: functions with no decomp object (base_size=0)
   - **Failed**: symbols not found by objdiff

## Tips

- Run without `--dry-run` to auto-mark 100% matches as COMPLETE in one shot
- Use broad globs like `system/*` to sweep entire subsystems
- Partial matches in the output are good candidates for manual decomp work
