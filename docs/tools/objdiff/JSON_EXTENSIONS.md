# objdiff-cli JSON extensions (milohax fork)

`bin/objdiff-cli` is the milohax fork of objdiff (built from
`~/code/milohax/objdiff`, symlinked into `bin/`). Beyond the JSON diff fields the
scripts already consume (`verdict`, `analysis`, `instruction_summary`, …) it adds
two structured capabilities. Verify you have the fork build:

```bash
bin/objdiff-cli diff --help | grep -q -- --include-data && echo "fork build OK"
```

> **MCP note:** the `mcp__orchestrator__run_objdiff` wrapper does **not** pass
> `--include-data`, so for *data* symbols (below) call `bin/objdiff-cli` directly
> (the `/data-diff` skill does this). Code-symbol analysis stays on the MCP tools.

## Data-symbol diffs (`--include-data`)

Stock objdiff diffs *code*. The fork also emits a structured byte/relocation diff
for **data** symbols — vtables (`??_7Class@@6B@`), RTTI, pointer/jump tables,
string pools, static initializers.

```bash
bin/objdiff-cli diff -p . -u <unit> "<data_symbol>" -f json --include-data
```

A `data_diff` object is added for data symbols (omitted for code symbols, so the
flag is a safe no-op on functions):

```jsonc
"data_diff": {
  "match_percent": 75.0,
  "mismatch_byte_count": 4,
  "total_byte_count": 24,
  "segments": [
    { "offset": 0,  "size": 16, "kind": "equal" },
    { "offset": 16, "size": 4,  "kind": "replace",
      "bytes": "0000a1b0", "base_bytes": "0000a1c4" }
  ],
  "relocations": [
    { "offset": 8, "size": 4, "kind": "equal",
      "target_symbol": "??_GFilePath@@UAEPAXI@Z" },
    { "offset": 12, "size": 4, "kind": "replace",
      "target_symbol": "?Print@String@@QBEXPBD@Z",
      "base_target_symbol": "?Print@FilePath@@QBEXPBD@Z" }
  ]
}
```

- **`segments[]`** — contiguous byte runs. `kind` ∈ `equal`/`replace`/`insert`/
  `delete`. `bytes` is the **target** side (hex); `base_bytes` is the **base**
  (decompiled) side, present only when it differs. Equal runs carry no bytes. Use
  for string-literal typos and wrong initializer values.
- **`relocations[]`** — the actionable signal for vtables/pointer tables.
  `target_symbol` is where the **target** reloc points; `base_target_symbol` names
  where the **base** build points when it differs — `kind: "replace"` + a
  `base_target_symbol` means **this vtable slot resolves to the wrong function**.
  A base-only relocation surfaces as `kind: "insert"` with an empty
  `target_symbol`; `addend`/`base_addend` flag pointer-into-symbol offset mismatches.

## Control-flow (branch) graph

With `--include-instructions`, each instruction row carries the per-side branch
graph objdiff already computes (the GUI's branch arrows):

```jsonc
{ "index": 13, "match_type": "equal",
  "target_branch_to":   { "target_index": 20, "branch_idx": 0 } }
{ "index": 20, "match_type": "equal",
  "target_branch_from": { "source_indices": [13], "branch_idx": 0 } }
```

- `*_branch_to` `{ target_index, branch_idx }` — the row this row branches to.
- `*_branch_from` `{ source_indices: [...], branch_idx }` — rows that branch here.

Fields exist for both `target_` and `base_` sides. All indices reference the
`index` field of rows in the same `instructions` array; `branch_idx` is objdiff's
per-branch color/group id. Use this for control-flow-aware analysis (loop bodies,
branch reordering) without re-deriving the CFG from raw asm.

## Reference

Source of truth: the fork at `~/code/milohax/objdiff`
(`objdiff-cli/src/cmd/diff.rs`, `docs/research/next-work.md`). See also the
`/data-diff` skill.
