Rock Band 3 (Xbox 360) — AI-Assisted Fork
=========================================

> ⚠️ **Unofficial fork.** This repository is **not** the canonical RB3 Xbox 360
> decomp. It is a personal experiment by
> [@freeqaz](https://github.com/freeqaz) exploring how far AI agents can push a
> clean-room Xbox 360 decompilation project. Code, commits, decisions, tooling,
> and most of the documentation in this fork are **AI-assisted** (primarily via
> Claude Code). Do not assume anything here represents the views, code quality
> bar, or roadmap of the upstream maintainers.
>
> The **canonical, human-curated** project lives at
> **[rjkiv/rb3-xenon](https://github.com/rjkiv/rb3-xenon)**. If you want to
> contribute to or follow the official effort, please go there instead.
>
> No game assets, no Xbox 360 assembly, and no copyrighted binaries are stored
> in this repo. An existing copy of the game is required to do anything useful
> with it.

A decompilation of **Rock Band 3** for the **Xbox 360** (PowerPC Xenon),
targeting the vanilla retail executable (title ID `45410914`). Built with the
MSVC X360 toolchain (`16.00.11886.00`) at the retail size-optimized release
flags `/O1 /Oi /GR /EHsc` (no `/GL`, no `/LTCG`). Goal: produce byte-matching
machine code from C++ source, the same way the GC/Wii decomp projects do.

This is the third project in a series, and it sits at the **intersection** of
the other two — same *game* as the Wii decomp, same *platform and compiler* as
the Dance Central 3 decomp:

| | DC3 | RB3 (Wii) | **RB3 (Xenon)** |
|---|---|---|---|
| Repo | [freeqaz/dc3-decomp](https://github.com/freeqaz/dc3-decomp) | [freeqaz/rb3](https://github.com/freeqaz/rb3) | **this repo** |
| Platform | Xbox 360 (PPC Xenon) | Wii (PPC Gekko/Broadway) | Xbox 360 (PPC Xenon) |
| Compiler | MSVC for Xbox 360 | MetroWerks CodeWarrior 4.3 | MSVC for Xbox 360 |
| Game | Dance Central 3 | Rock Band 3 | Rock Band 3 |
| Engine | Milo | Milo | Milo |
| Symbols | leaked PDB/.map (named) | full DWARF (named) | **none — anonymous `fn_8XXXXXXX`** |
| Role here | engine + toolchain oracle | game-code oracle | the target |

DC3 is the **same Milo engine on the same hardware and compiler** as us, and the
Wii RB3 decomp is the **same game**. Neither sibling is the target, but together
they are a pair of Rosetta Stones — see below.

Sister projects
---------------

- **[freeqaz/rb3](https://github.com/freeqaz/rb3) — RB3 on Wii.** The same game,
  decompiled from the Wii *development* build (MWCC PowerPC, named functions,
  retains `MILO_ASSERT` source-path strings that the retail Xbox build stripped).
  It is the **game-code oracle**: `src/band3/` and `src/network/` logic is
  ported from there (Wii→360).
- **[freeqaz/dc3-decomp](https://github.com/freeqaz/dc3-decomp) — Dance Central
  3 on Xbox 360.** The same Milo engine, already 360-ported, compiled with the
  *same* `/O1 /Oi /GR /EHsc` flags, with named functions courtesy of a leaked
  PDB/.map. It is the **engine oracle**: `src/system/`, the XDK headers
  (`src/xdk/`), STLport, and the MSVC toolchain all come from there. DC3 is also
  where the AI-assisted methodology below was first built and proven, on the
  harder target (no DWARF, ICF, link-time pragmas).

> **Why this is hard, precisely.** The asymmetry between us and DC3 is **not**
> optimization level — both are `/O1`, no whole-program optimization, so TU
> spatial grouping in `.text` is preserved (MasterAudio's 46 functions pack into
> 8 KB). `/Ob2` inlines leaf math aggressively (SHA1 K-constants used 20× in
> source appear 0× in `.text`), but there is no cross-TU reordering or
> whole-program inlining. The difference is that DC3 had a leaked PDB giving its
> functions names+addresses, while RB3's are anonymous `fn_8XXXXXXX`. So we use
> DC3 as a **Rosetta Stone for the engine** and Wii-RB3 as a **Rosetta Stone for
> the game**, transferring their labels onto our anonymous functions via shared
> string content (`tools/fingerprint_match.py`) or structural similarity
> (Ghidra + BinDiff).

Decomp priority: the GAME, not the engine
-----------------------------------------

Matching/porting effort goes into RB3's **game layer** (`src/band3/`,
`src/network/`), not the Milo engine (`src/system/`). The engine is effectively
pre-solved: DC3 is the same engine on the same platform, and DC3's
already-decompiled engine **loads and renders RB3-360 `.milo_xbox` assets** with
zero rb3-xenon code (same texture tiling, vertex compression, endianness; DC3's
loaders keep backward-compat parse branches for RB3's older revisions). A 3-way
`rndobj` cross-check shows RB3-360 ≈ DC3 on every divergence point; only the Wii
build (its `rndwii`/GX branch) is the outlier. The renderer, materials,
textures, and mesh/skeleton loading are all supplied by DC3 — the
RB3-specific value concentrates in the game code. Full evidence and the
asset-render experiment are in
[`docs/plans/engine-reuse-and-asset-rendering.md`](docs/plans/engine-reuse-and-asset-rendering.md).

Status
------

Two build tracks are alive:

**1. X360 decomp-matching build** (`ninja`). Runs end-to-end: dtk SPLIT → MSVC
compile → objdiff report → progress. The splits-bootstrap pipeline is proven
(2026-05-26): pinning `MasterAudio.cpp` in `splits.txt` makes dtk emit a real
target `.obj` + per-object `.s` and auto-derive the matching `.pdata` range. The
path from "identification" to "registered match" is open; remaining work is
per-target porting + obj-patcher wiring. **Match baseline: `0.00%`** — and that
is the honest whole-binary metric (11.8 MB code / 66,003 functions, none matched
yet, no denominator gaming), directly comparable to DC3's number.

**2. Native engine build** (`native/`, x86_64 Linux + clang). A host build of
the shared Milo engine that actually *runs*. The `rb3-dta` tool boots the bare
engine and parses a real RB3 `songs.dta` into the engine's DataArray tree —
verified on the 138-song RB3 360 catalog, no GPU or audio yet. The architectural
goal is a native target that injects DC3's `rndobj` + only RB3's game code, and
eventually a shared `../milo-native-engine` runtime consumed by all the per-game
decomps.

How the AI tooling works
========================

This fork is organized around a small set of services that give LLM agents
structured, reproducible access to the binary, the build, and the prior-art
sibling codebases. Agents don't "look at assembly and guess" — they call typed
tools that report match percentages, struct offsets, and cross-references. Most
of this was built on DC3 and ported here verbatim (same MSVC X360 toolchain).

Decomp services
---------------

- **Orchestrator MCP** (`scripts/orchestrator/`, server name `decomp`) — the
  central tool surface, backed by `decomp.db` (SQLite, 66k functions seeded from
  `report.json`). 11 tools: `report_result`, `query_functions`, `get_attempts`,
  `run_objdiff`, `run_diff_inspect`, `run_analyze_function`,
  `lookup_struct_offset`, `lookup_merged_symbol`, `mark_patch_result`, plus the
  two sibling oracles: **`lookup_rb3wii`** (greps the Wii RB3 decomp — game code)
  and **`lookup_dc3`** (greps the DC3 decomp — engine code).
- **Ghidra MCP** (`tools/ghidra/`, pyghidra over HTTP on port **8002**; DC3 owns
  8000, Wii-RB3 owns 8001) — headless Ghidra serving decompiled C, switch/cast
  analysis, semantic search across the binary's functions, and DTM-vs-header
  struct diffs. Uses a VMX128 SLEIGH fork for correct Xenon vector decode.
- **`tools/fingerprint_match.py`** — function identification. Indexes every one
  of the 66,838 RB3 functions by referenced strings/callees/constants, then
  cross-refs those strings against `../rb3/src` and `../dc3-decomp/src` to
  propose source-file mappings. This table is what drives `splits.txt` pinning.
- **Analysis engine** (`scripts/analysis/diff_inspect.py`) — modes for
  `diagnose`, `clusters`, `regswaps`, `offsets`, `stack-layout`, etc., backing
  the `/compare-asm` and `/stack-layout` skills and the MCP `run_diff_inspect`
  tool.
- **Cross-binary identification** (in progress) — Ghidra + BinDiff to
  bulk-transfer DC3's named functions onto RB3's anonymous `fn_8XXXXXXX` by
  structural similarity.

Agent harness
-------------

- **`.claude/skills/`** — 24 slash-command skills wrapping the tools above into
  agent-callable verbs: `/recon`, `/permute`, `/batch-check`,
  `/ghidra-decompile`, `/ghidra-struct`, `/struct-info`, `/vtable`,
  `/resolve-vcall`, `/stack-layout`, `/compare-asm`, `/data-diff`,
  `/refactor-staff`, `/progress`, and the two pairing oracles **`/dc3-pair`**
  (engine) and **`/rb3wii-pair`** (game code). Native-port skills too:
  `/native-build`, `/asset-extract`, `/screenshot`, `/gpu-capture`,
  `/gpu-debug`, `/gpu-inspect`, `/xenia-gameplay`.
- **Worktree pool** (`scripts/setup_worktree.sh`, `worktree_pool.py`) —
  concurrent agents work on isolated git worktrees built via btrfs CoW reflinks,
  sharing the warm build cache, tools, and target objects without serializing on
  one working tree.
- **Persistent memory** (`~/.claude/projects/.../memory/`) — an index of
  prior-session findings (vtable layouts, divergence classes, known bugs,
  workflow lessons, project roadmap) that future sessions read before starting.
- **`CLAUDE.md`** — the full agent context: build wiring, source provenance per
  directory, the matching workflow, and the hard rules for concurrent agents
  sharing the working tree.

Toolchain
=========

The MSVC X360 compiler (`cl.exe`), `wibo` (Win32-on-Linux shim), STLport, and
the XDK CRT headers (`LIBCMT`) are borrowed from `../dc3-decomp`.

Two tools are **local forks that must be checked out as siblings under `../`** —
the build prefers them over any upstream release:

- **dtk** (the XEX splitter) — [`freeqaz/jeff`](https://github.com/freeqaz/jeff)
  checked out at **`../jeff`**. Our fork of
  [`rjkiv/jeff`](https://github.com/rjkiv/jeff) (itself the Xbox 360 fork of
  [`encounter/decomp-toolkit`](https://github.com/encounter/decomp-toolkit)),
  carrying the RB3-retail XEX fixes. `configure.py` auto-discovers `../jeff` and
  defaults `--dtk` there; without it the build silently falls back to the
  upstream dtk release, which lacks jeff's retail overlap-tolerance patch.
- **objdiff** (the diff engine) —
  [`freeqaz/objdiff`](https://github.com/freeqaz/objdiff) checked out at
  **`../objdiff`**. Our fork of
  [`encounter/objdiff`](https://github.com/encounter/objdiff) with custom
  pattern detectors + normalized-diff changes. Pass `--objdiff ../objdiff` to
  build from it instead of the default download.

Building
========

The X360 matching build is Linux-first (it drives the Windows `cl.exe` through
`wibo`). Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages),
then:

```sh
# Place the retail executable at orig/45410914/default.xex (not committed).
python3 configure.py            # regenerate build.ninja
./tools/ninja-locked            # ALWAYS use this wrapper, never bare `ninja`
```

> `./tools/ninja-locked` takes an `flock` so concurrent agents don't corrupt the
> shared build dir. Always `tee` build output to a log — dtk/MSVC failures are
> invisible without it. After editing `splits.txt`/`objects.json`, `touch
> config/45410914/config.yml` to force a re-SPLIT, then re-run configure/ninja.

Native engine build:

```sh
cd native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/rb3-dta path/to/songs.dta
```

Diffing
=======

After the initial build, an `objdiff.json` exists in the project root. Build the
[`freeqaz/objdiff`](https://github.com/freeqaz/objdiff) fork at `../objdiff` (or,
for a quick look, download an upstream release from
[encounter/objdiff](https://github.com/encounter/objdiff)), set **Project
directory** to this repo under **Project → Settings**, and the configuration
loads automatically. Selecting an
object from the left sidebar starts diffing against the retail XEX; edits to
source, headers, `configure.py`, `splits.txt`, or `symbols.txt` trigger
automatic rebuilds.

Identification tooling
======================

`tools/fingerprint_match.py` is the entry point for turning anonymous functions
into source-file proposals:

```sh
python3 tools/fingerprint_match.py extract --out fingerprints.json   # 66,838 fns
python3 tools/fingerprint_match.py autoid   --out autoid.json        # source proposals
python3 tools/fingerprint_match.py report                            # human view
python3 tools/fingerprint_match.py identify <string-or-fn>           # one-off lookup
```

Outputs are gitignored / regenerable. The `autoid` table is the basis for
`splits.txt` pinning (see the splits-bootstrap recipe in `CLAUDE.md`).

Project structure
=================

- `configure.py` — project configuration and build generator.
- `config/45410914/` — config for the retail XEX (`config.yml`, `symbols.txt`,
  `splits.txt`, `objects.json`).
- `orig/45410914/default.xex` — the retail executable (not committed).
- `src/system/` — Milo engine code (sourced from dc3-decomp).
- `src/band3/`, `src/network/` — RB3 game code (sourced from rb3-Wii).
- `src/xdk/`, `src/system/stlport/` — Xbox 360 SDK + STLport (from dc3-decomp).
- `tools/` — build scripts + identification/struct/vtable tooling.
- `scripts/` — orchestrator MCP, analysis engine, MSVC object patchers (the
  patchers are staged for the matching phase, dormant).
- `native/` — host (x86_64 Linux + clang) engine build.
- `.claude/skills/` — agent slash-command skills.
- `docs/` — dependencies, getting-started, splits/symbols/config/objects format
  docs, MSVC X360 pattern catalog, and plan files.

References
==========

- [freeqaz/objdiff](https://github.com/freeqaz/objdiff) — **the diffing fork we
  use** (checked out at `../objdiff`); forked from
  [encounter/objdiff](https://github.com/encounter/objdiff)
- [freeqaz/jeff](https://github.com/freeqaz/jeff) — **the dtk/XEX-splitter fork
  we use** (checked out at `../jeff`); forked from
  [rjkiv/jeff](https://github.com/rjkiv/jeff), the Xbox 360 fork of
  [encounter/decomp-toolkit](https://github.com/encounter/decomp-toolkit)
- [wibo](https://github.com/decompals/wibo) — minimal Win32 wrapper for Linux
- [BinDiff](https://github.com/google/bindiff) — cross-binary function matching
- [XEXLoaderWV](https://github.com/zeroKilo/XEXLoaderWV) — Ghidra Xbox 360 XEX loader
- [decomp.me](https://decomp.me) — collaborate on matches

Acknowledgments
===============

- [rjkiv/rb3-xenon](https://github.com/rjkiv/rb3-xenon) — the canonical RB3
  Xbox 360 decomp this fork tracks upstream.
- [freeqaz/dc3-decomp](https://github.com/freeqaz/dc3-decomp) /
  [rjkiv/dc3-decomp](https://github.com/rjkiv/dc3-decomp) — the Milo engine
  oracle and where the AI-assisted methodology was first developed.
- [freeqaz/rb3](https://github.com/freeqaz/rb3) /
  [DarkRTA/rb3](https://github.com/DarkRTA/rb3) — the Wii RB3 decomp, source of
  shared game-code ground truth.
- The wider [milohax](https://discord.gg/milohax) community.
