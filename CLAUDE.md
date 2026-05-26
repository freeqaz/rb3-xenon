# rb3-xenon — Claude Context

Decompilation of **Rock Band 3** for **Xbox 360** (PowerPC). Goal: matching
machine code from C++ source. Target binary: vanilla retail XEX, title ID
`45410914`, at `orig/45410914/default.xex` (not committed).

**This is a hard target: the retail XEX was built with LTO/LTCG enabled.**
Whole-program optimization inlines and reorders aggressively, so matching is
considerably harder than on a non-LTO build like dc3-decomp's debug target.

## Source provenance (important)

Two sibling repos feed this one. Pick the right source per directory:

- **`src/system/` (Milo engine) ⟵ `../dc3-decomp`.** Dance Central 3 is the
  *same Milo engine*, already ported to 360 (uses `RTL_CRITICAL_SECTION`,
  `xdk/XBOXKRNL.h`, etc. — not Wii's `revolution/OS.h`). Its engine headers and
  `.cpp` files compile under the 360 toolchain, so they are the correct base.
- **`src/band3/`, `src/network/` (RB3 game code) ⟵ `../rb3` (rb3-Wii).** Game
  logic lives in the Wii decomp. It's Wii-targeted (MWCC PowerPC) and needs
  Wii→360 porting.
- **`src/xdk/`, `src/system/stlport/`, toolchain ⟵ `../dc3-decomp`.**

**Caveat (from the project owner):** dc3-decomp is *newer* than RB3. Its engine
code may have subtle behavioral differences or version incompatibilities. When a
file misbehaves, cross-check against rb3-Wii's equivalent and merge intent — do
not assume dc3's version is correct for RB3.

## Key commands

```bash
ninja                    # dtk SPLIT -> MSVC compile -> objdiff report -> progress
python3 configure.py     # regenerate build.ninja (after editing objects.json/splits.txt)
```

dtk is the local **jeff** fork at `../jeff`; `configure.py` defaults `--dtk`
there. After editing `config/45410914/config.yml` or `splits.txt`, `touch`
`config.yml` to force a re-SPLIT (ninja doesn't track splits.txt as a dep).

## Build wiring

- `tools/defines_common.py` — include paths. **STLport must come first**, then
  `src/xdk/LIBCMT` (C CRT), then `src`, `src/system`.
- `config/45410914/objects.json` — declares which `.cpp` files to compile and
  their match status. New files: add here as `NonMatching`.
- `tools/project.py` — patched so objects in `objects.json` get a compile edge
  even without a `splits.txt` address range (compile-only scaffolding).
- `src/` include style mirrors rb3-Wii: `#include "math/Vec.h"` resolves via
  `/I src/system`. **Beware include shadowing:** `/I src` precedes
  `/I src/system`, so a file at `src/os/Foo.h` will shadow `src/system/os/Foo.h`.
  (A stub `src/os/Debug.h` once shadowed the real engine `Debug.h` and broke
  every macro-using header — don't reintroduce stubs at `src/` root.)

## Known issues / expected noise

- dtk SPLIT prints `WARN` lines about UTF-16 strings, `PpcRel`/`PpcAddr16`
  relocations, and unaligned symbols. These are **tolerated** — jeff was patched
  to downgrade asm-write failures to warnings (see jeff `src/cmd/xex.rs`), so
  `config.json` is still emitted and the build proceeds.
- `Progress: 0.00% matched` is expected. Everything is `NonMatching`; we're
  scaffolding compile coverage, not matching yet.
- **Case sensitivity:** dc3's tree has case-variant files (e.g. `vec.cpp` vs
  `Vec.cpp`) that collide on Windows but coexist on Linux. Use the name dc3's
  `objects.json` actually builds (lowercase `vec.cpp`, `mtx.cpp`).

## Matching phase (later)

`scripts/` holds dc3's MSVC object patchers (anon-ns, guard, bool-mangle,
atexit-scope, dynamic-init, regswap, transplant) — copied but **not wired**.
See `scripts/README.md`. Activate by mirroring dc3's `configure.py`
`custom_build_steps` once we start matching against real `splits.txt` addresses.

## Phase tracking

Detailed roadmap (what's done / next) lives in Claude's auto-memory at
`~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_rb3_xenon_roadmap.md`.
