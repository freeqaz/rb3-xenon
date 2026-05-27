---
name: data-diff
description: Diff a DATA symbol (vtable ??_7Class@@6B@, RTTI, pointer/jump table, string pool, static initializer) between the target and the decompiled build, showing byte differences and — for each relocation slot — which function/symbol each side points to. Use when a data symbol is below 100%, or to find which vtable slot resolves to the wrong function. The MCP run_objdiff wrapper does not expose data diffs, so this calls objdiff-cli directly.
argument-hint: "<symbol> [--unit UNIT]"
allowed-tools: Bash(bin/objdiff-cli *), Bash(nm *), Read, Grep, Glob
---

# Data Diff

Structured byte- and relocation-level diff of a **data** symbol, target vs the
decompiled build. objdiff's normal output (and the `mcp__orchestrator__run_objdiff`
tool) diffs *code*; `--include-data` (a milohax-fork feature) handles **data**:
vtables, RTTI, pointer/jump tables, string pools, static initializers.

This answers "**where does my decompiled build diverge from the target**" for a
data symbol — slot by slot, byte by byte. It's the data analogue of `/compare-asm`.

## Arguments

`$ARGUMENTS`

- First positional: the data symbol, MSVC-mangled — e.g. a vtable `??_7FilePath@@6B@`,
  RTTI `??_R4...`, or a string/array symbol. Quote it (the names contain `?@`).
- `--unit UNIT` — objdiff unit. Usually needed for data symbols, which are local to
  a translation unit.

## Steps

1. **Run the data diff** (directly via `bin/objdiff-cli`, since the MCP wrapper
   doesn't pass `--include-data`):

   ```bash
   bin/objdiff-cli diff -p . -u <unit> "$0" -f json-pretty --include-data
   ```

   If you don't know the unit, find which object defines the symbol, or use
   `report query` to locate the data symbol's unit.

2. **Read `data_diff.relocations`** — the actionable part for vtables/pointer
   tables. Each entry is one pointer slot:

   - `target_symbol` — function/symbol the **target** slot points to.
   - `base_target_symbol` — where the **decompiled** build points, shown only when
     it differs. `kind: "replace"` + a `base_target_symbol` means **this slot
     resolves to the wrong function** — the highest-signal vtable mismatch.
   - A slot present only in the base build is `kind: "insert"` (empty
     `target_symbol`, a `base_target_symbol`) — an extra/misordered entry.
   - `addend` / `base_addend` flag a pointer-into-symbol offset mismatch.

3. **Read `data_diff.segments`** for raw (non-pointer) bytes — string contents,
   enum tables, packed structs. `bytes` is the target side, `base_bytes` the
   decompiled side (present only when they differ); equal runs carry no bytes. Use
   for string-literal typos and wrong initializer values.

4. **Act on it.** A `replace` reloc → the source references the wrong
   function/global, or the class declaration has the wrong virtual-function order.
   A `replace` segment → wrong literal/init value at that offset.

## Example

```
$ bin/objdiff-cli diff -p . -u <unit> "??_7FilePath@@6B@" -f json-pretty --include-data
  data_diff.relocations:
    +0x08  equal    ??_GFilePath@@UAEPAXI@Z
    +0x0c  replace  target=?Print@String@@QBEXPBD@Z  base=?Print@FilePath@@QBEXPBD@Z
```

Slot +0x0c resolves to the wrong method on the decompiled side — fix the source
reference or the virtual-function order so it points to `String::Print`.

## When to Use

- A vtable / RTTI / data symbol is stuck below 100% and you need to know *which* slot.
- Verifying a class's virtual-function order produced the right compiled vtable.
- A string-pool or static-initializer data symbol mismatches and you need the
  differing bytes.

## Tips

- `--include-data` is a no-op on code symbols (no `data_diff` emitted), so it's safe
  to add when unsure whether a symbol is code or data.
- For *code* analysis keep using `mcp__orchestrator__run_objdiff` /
  `mcp__orchestrator__run_diff_inspect`; this skill covers the data gap the wrappers
  don't.
- Full schema (including the instruction branch graph): `docs/tools/objdiff/JSON_EXTENSIONS.md`.
