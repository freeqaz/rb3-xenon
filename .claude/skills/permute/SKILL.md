---
name: permute
description: Run the source permuter on a function to find signed/unsigned and variable extraction improvements. Use when working on a function that isn't matching 100% and you want to automatically try source variations.
argument-hint: "[symbol-or-function] [--unit GLOB] [--dry-run] [--no-apply]"
allowed-tools: Bash(venv/bin/python *), Bash(ninja *), Read, Grep, Glob
---

# Permuter Skill

Automatically tries source-level pattern transformations (signed/unsigned casts, variable
extraction, loop rewrites, etc.) and scores each variant with objdiff. Uses AST scanning
to identify relevant patterns first, then hill-climbs only those — much faster than brute force.

All advanced features (Ghidra guidance, chain search, adaptive boosting, constrained synthesis)
are **on by default**. You only need `--no-*` flags to disable them.

## rb3-xenon setup

The permuter is the installed **`decomp_synth`** package (`pip install -e` into the
shared venv; rb3-xenon's `venv` symlinks to it). It is fully config-driven: it reads
`decomp-synth.json` at the repo root, which describes rb3-xenon (title `45410914`,
MSVC PPC `cl.exe`, `.obj`/`/Fo`, flat target-obj layout `build/45410914/obj/Rot.obj`).
Invoke `decomp_synth.scan_and_permute` as-is — no shim.

Notes:
- Variants compile to `/tmp` (never the main build dir); the apply step only writes
  source when a variant *beats* baseline, so a no-improvement run leaves source clean.
- "BSF mode: unavailable (tools.compiler_trace not found)" is benign — that's DC3-only
  IL-capture tooling rb3-xenon doesn't have. The scan/score path is unaffected.
- Always `tee` to a log: `... 2>&1 | tee /tmp/rb3_permuter_<fn>.log`.

## Arguments

`$ARGUMENTS`

## Usage

### Single function (most common)

```bash
venv/bin/python -m decomp_synth.scan_and_permute --symbol '$0' --max-rounds 10 --max-variants 100 --plateau-limit 3 --chain-depth 5
```

Accepts mangled symbols, qualified names, or partial names:
- `--symbol '?Seek@AsyncFile@@UAAHHH@Z'`
- `--symbol 'AsyncFile::Seek'`
- `--symbol 'Seek'`

### Unit scan

```bash
venv/bin/python -m decomp_synth.scan_and_permute --unit 'system/obj/*'
```

### Bulk scan (all functions)

```bash
venv/bin/python -m decomp_synth.scan_and_permute --jobs 16 --limit 200
```

## Key flags

| Flag | Default | What it does |
|------|---------|--------------|
| `--symbol NAME` | — | Target one function (repeatable) |
| `--unit GLOB` | all | Scope to units matching glob |
| `--max-rounds N` | 5 | Hill-climbing rounds per function (use 10 for single fn) |
| `--max-variants N` | 50 | Variants per round (use 100 for single fn) |
| `--plateau-limit N` | 2 | Stop after N rounds w/o improvement (use 3 for single fn) |
| `--chain-depth N` | 5 | Max chain depth for N-stage composition |
| `--max-pct N` | 99.99 | Skip functions already above N% |
| `--min-pct N` | 0 | Skip functions below N% |
| `--limit N` | unlimited | Cap number of functions processed |
| `--jobs N` | 1 | Parallel workers (by source file) |
| `--dry-run` | off | Scan only — show hits, don't climb |
| `--no-apply` | off | Score variants but don't write changes |
| `--patterns X,Y` | all | Only run specific patterns |
| `--no-ghidra` | — | Disable Ghidra-guided patterns |
| `--no-chain` | — | Disable multi-stage pattern chains |
| `--no-adaptive` | — | Disable adaptive pattern suppression |
| `--no-constrained` | — | Disable constraint-directed synthesis |
| `--json` | off | Machine-readable output |

## What to report

After running, tell the user:
- How many functions were scanned and how many improved
- For each improvement: function name, old% → new%, winning pattern(s)
- Whether changes were applied to source (default: yes)
