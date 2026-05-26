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

> **Why this is a hard target:** unlike dc3-decomp (a dev/debug build with no
> link-time codegen), the RB3 retail XEX was built with **LTO/LTCG enabled**.
> Whole-program optimization reorders and inlines aggressively, so byte-exact
> matching is substantially harder than on a non-LTO build.

Status
------

Two build tracks are alive:

**1. X360 decomp-matching build** (`ninja`). Runs end-to-end: dtk SPLIT → MSVC
compile → objdiff report → progress. Currently compiles the `system/math`
engine library plus `Main.cpp`. Everything is `NonMatching` — we're establishing
breadth (what compiles) before depth (what matches against the retail XEX).

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
`--dtk` to that checkout.

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
