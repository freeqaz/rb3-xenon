---
name: unicorn-query
description: Query unicorn behavioral test results from the database. Filter functions by verdict (EQUIVALENT/DIVERGENT), divergence class (logic/build_env/regalloc), and unit. Use to find functions with real behavioral bugs to fix.
argument-hint: "[--class logic|build_env|regalloc] [--verdict DIVERGENT|EQUIVALENT] [--unit pattern] [--status workable|complete|all] [--limit N] [--summary-only]"
allowed-tools: Bash(python3 scripts/unicorn/query.py *)
---

# Unicorn Query Skill

Query the decomp database for unicorn behavioral test metadata. Surfaces functions
by their runtime behavioral equivalence status.

## Arguments

`$ARGUMENTS`

## Steps

1. **Build the command** from the arguments and run:
   ```bash
   python3 scripts/unicorn/query.py $ARGUMENTS
   ```

   If no arguments were provided, default to `--verdict DIVERGENT` to show all divergent functions.

2. **Present results** to the user. Key things to call out:
   - **logic** class: real behavioral bugs, fixable from source — primary work targets
   - **build_env** class: unfixable from source (linker/environment differences like `__FILE__` strings, merged symbols)
   - **regalloc** class: rare register allocation edge cases, usually not worth pursuing
   - Functions marked COMPLETE but with `logic` divergence may have hidden behavioral bugs

## Common Queries

- `/unicorn-query --class logic` — all logic-divergent functions (primary work targets)
- `/unicorn-query --class logic --status workable` — logic bugs not yet marked done
- `/unicorn-query --class logic --unit system/char/*` — logic bugs in a specific subsystem
- `/unicorn-query --verdict EQUIVALENT --status workable` — behaviorally correct but asm not matching yet
- `/unicorn-query --summary-only` — just show counts by verdict/class
- `/unicorn-query --class build_env --summary-only` — count of environment-only issues

## Note

The unicorn DB columns are in `decomp.db`. This skill is available for when unicorn behavioral testing is added to the rb3-xenon pipeline.
