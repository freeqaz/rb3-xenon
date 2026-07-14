# Getting Started

> **STATUS (2026-07-06):** SUPERSEDED for this repo — this was the
> dtk-template's "bootstrap a brand-new project" checklist. rb3-xenon has
> already been bootstrapped (title ID `45410914`, vanilla retail XEX); none of
> the steps below apply anymore. Kept for historical/reference value only.

This file was inherited from the dtk-template scaffold and describes how to
turn a *blank* template checkout into a working project (renaming
`orig/GAMEID`, filling out `config.yml`, running the initial `ninja` analysis
pass to generate `symbols.txt`/`splits.txt` from scratch, etc.). That
one-time setup already happened for rb3-xenon a while ago. What actually
applies now:

- **Read `CLAUDE.md` first** — it's the authoritative, current-state doc for
  this repo (build commands, toolchain, source provenance, worktree rules).
- The config for this project's one target lives at `config/45410914/`
  (`config.yml`, `config.json`, `objects.json`, `splits.txt`, `symbols.txt`) —
  already populated, not a template to fill in.
- To build: `python3 configure.py` then `./tools/ninja-locked` (see CLAUDE.md
  "Two build tracks" for the full recipe, including the sibling-fork
  toolchain each requires — `../jeff`, `../objdiff`, the `freeqaz/wibo` fork,
  and `objcache`).
- To add a new file to the matching effort: add it to
  `config/45410914/objects.json` as `"NonMatching"`, then pin a `.text` range
  in `config/45410914/splits.txt` — see the "Splits-bootstrap recipe" in
  `CLAUDE.md`.
- `docs/splits.md` and `docs/symbols.md` (not touched by this pass) describe
  the `splits.txt`/`symbols.txt` file formats and remain accurate reference
  material for those two files.
