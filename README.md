rb3-xenon
=========

Decompilation of **Rock Band 3** for the **Xbox 360** (PowerPC), targeting the
vanilla retail executable (title ID `45410914`). Built with the MSVC X360
toolchain (`16.00.11886.00`). Goal: produce matching machine code from C++
source, the same way the GC/Wii decomp projects do.

This project is scaffolded from two sibling repositories:

- **[`../rb3`](https://github.com/) (rb3-Wii)** — the established Wii RB3 decomp
  (~60% match, MWCC PowerPC). Source of **game code** (`src/band3/`,
  `src/network/`) — though that code is Wii-targeted and needs Wii→360 porting.
- **[`../dc3-decomp`](https://github.com/) (Dance Central 3, Xbox 360)** — a
  working 360 decomp of the **same Milo engine**. Source of **engine code**
  (`src/system/`), the XDK headers (`src/xdk/`), STLport, the MSVC toolchain,
  and matching tooling. dc3-decomp is *newer* than RB3, so its engine code can
  contain subtle behavioral differences — merge with rb3-Wii's intent in mind.

> **Why this is hard, precisely:** retail size-optimized release
> (`/O1 /Oi /GR /EHsc`, no `/GL`, no `/LTCG` — verified by reading dc3's
> `config.json`). `/Ob2` inlines leaf functions aggressively (SHA1 K-constants
> used 20× in source appear 0× in `.text`), but there's *no whole-program
> reordering* — TU spatial grouping is preserved (MasterAudio's 46 functions
> pack into 8 KB). dc3-decomp is the same engine compiled the same way; its
> advantage over us is a leaked PDB/.map giving its functions names, not a
> cleaner build. We exploit dc3 as a **Rosetta Stone**: transfer its labels to
> our anonymous `fn_8XXXXXXX` via shared string content or structural
> similarity.

Status
------

Two build tracks are alive:

**1. X360 decomp-matching build** (`ninja`). Runs end-to-end: dtk SPLIT → MSVC
compile → objdiff report → progress. Currently compiles the `system/math`
engine library plus `Main.cpp`. Match baseline: `0.00%` (the honest
whole-binary metric: 11.8 MB code / 66,003 functions, none matched yet).

**Splits-bootstrap proven (2026-05-26):** `MasterAudio.cpp` pinned in
`splits.txt`, dtk emits a real target `.obj` + per-object `.s` and auto-derives
the matching `.pdata` range. The pipeline to get from "identification" to
"registered match" is now open; remaining work is per-target porting +
wiring the obj patchers. See identification tooling below.

**2. Native engine build** (`native/`, x86_64 Linux + clang). A host build of
the shared Milo engine that actually *runs*. The `rb3-dta` tool boots the bare
engine and parses a real RB3 `songs.dta` into the engine's DataArray tree,
printing each song's id / name / artist — no GPU or audio. Verified loading the
138-song RB3 360 catalog. This is the foundation for bringing more of the engine
up natively (chart parsing, audio, eventually rendering).

See the project roadmap in Claude's memory for full phase tracking.

Building the native tool:

```sh
cd native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/rb3-dta path/to/songs.dta
```

Toolchain
---------

The MSVC X360 compiler (`cl.exe`), `wibo` (Win32-on-Linux shim), STLport, and
the XDK CRT headers (`LIBCMT`) are borrowed from `../dc3-decomp`. dtk (the
XEX splitter) is the local **jeff** fork at `../jeff`; `configure.py` defaults
`--dtk` to that checkout. objdiff is the local **freeqaz/objdiff** fork at
`../objdiff` (custom pattern detectors + normalized-diff changes); pass
`--objdiff ../objdiff` to `configure.py` to use it instead of the default
`v3.2.1` download.

Identification tooling
----------------------

`tools/fingerprint_match.py` indexes every RB3 function by its referenced
string literals (resolved via dtk's disassembly), then cross-references those
strings against `../rb3/src` (Wii dev decomp — richer source oracle, retains
asserts retail stripped) and `../dc3-decomp/src` (Xbox engine port,
~98% named via leaked PDB/.map). Subcommands:

```sh
python3 tools/fingerprint_match.py extract --out fingerprints.json   # 66,838 fns
python3 tools/fingerprint_match.py autoid   --out autoid.json        # source-file proposals
python3 tools/fingerprint_match.py report                            # human view
python3 tools/fingerprint_match.py identify <string-or-fn>           # one-off lookup
```

Outputs are gitignored / regenerable. The autoid table is the basis for
splits.txt pinning.

Cross-binary identification (Ghidra + BinDiff) is in progress for bulk-transfer
of dc3's named functions onto RB3's anonymous `fn_8XXXXXXX`.

Documentation
-------------

- [Dependencies](docs/dependencies.md)
- [Getting Started](docs/getting_started.md)
- [`symbols.txt`](docs/symbols.md)
- [`splits.txt`](docs/splits.md)
- [`config.json`](docs/config.md)
- [`objects.json`](docs/objects.md)

References
----------

- [objdiff](https://github.com/encounter/objdiff) (Local diffing tool)
- [decomp.me](https://decomp.me) (Collaborate on matches)
- [wibo](https://github.com/decompals/wibo) (Minimal Win32 wrapper for Linux)
- [jeff](https://github.com/rjkiv/jeff) (Xbox 360-targeted decomp-toolkit fork)
- [decomp-toolkit](https://github.com/encounter/decomp-toolkit) (Upstream of jeff)
- [BinDiff](https://github.com/google/bindiff) (Cross-binary function matching)
- [XEXLoaderWV](https://github.com/zeroKilo/XEXLoaderWV) (Ghidra Xbox 360 XEX loader)

Project structure
-----------------

- `configure.py` - Project configuration and build generator.
- `config/45410914/` - Configuration for the retail XEX (`config.yml`,
  `symbols.txt`, `splits.txt`, `objects.json`).
- `build/` - Build artifacts. Ignored by `.gitignore`.
- `orig/45410914/default.xex` - The retail executable (not committed).
- `src/system/` - Milo engine code (sourced from dc3-decomp).
- `src/band3/`, `src/network/` - RB3 game code (sourced from rb3-Wii).
- `src/xdk/` - Xbox 360 SDK headers (from dc3-decomp).
- `tools/` - Build scripts (project.py, defines_common.py, etc.).
- `scripts/` - MSVC object patchers, staged for the matching phase (dormant).
- `native/` - Host (x86_64 Linux + clang) engine build. CMake project +
  platform shims + the `rb3-dta` song-data loader.

Next steps
----------

Native engine (toward playing a song):

1. ✅ Parse `songs.dta` into the engine's DataArray and read song metadata.
2. Load a song's `.mid` chart (`src/system/midi` + `beatmatch`).
3. Decode `.mogg` audio (libvorbis / miniaudio, as in dc3's `native/src/audio`).
4. Venue render (`Rnd_Wgpu` + Dawn WebGPU) — the heavy lift.

Steps 2+ need a fuller engine boot (MemMgr heap, possibly `SystemInit` with a
config DTA) and more compiled subsystems in place of the current link stubs.

X360 matching build:

- Continue porting engine `.cpp` from dc3-decomp's `objects.json` menu (utl,
  obj, rndobj, …); then RB3 `band3/` game code from rb3-Wii.
- Pin real `splits.txt` address ranges so objects become diffable against the
  retail XEX (where LTO makes matching hard).
- Wire dc3's object patchers (`scripts/`) when matching begins.
