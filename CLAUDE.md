# rb3-xenon — Claude Context

Decompilation of **Rock Band 3** for **Xbox 360** (PowerPC). Goal: matching
machine code from C++ source. Target binary: vanilla retail XEX, title ID
`45410914`, at `orig/45410914/default.xex` (not committed).

**Optimization level: `/O1 /Oi /GR /EHsc` (retail size-optimized release, no LTCG).**
Verified for dc3 by reading `dc3-decomp/config/373307D9/config.json` (no `/GL` in
cflags, no `/LTCG` in ldflags). RB3 is the same Harmonix toolchain era and engine,
so likely the same flags. **Crucially, no whole-program optimization** — TU
spatial grouping in `.text` is preserved (empirically: the MasterAudio.cpp
cluster of 46 functions packs into 8 KB).

What `/O1` does: `/Ob2` aggressive within-TU inlining (so leaf math like SHA1's
K-constants disappear into callers), ICF (identical-COMDAT folding), but
*no cross-TU reordering or whole-program inlining*. dc3's linker map tags ~32k
functions `f i` — those are `/Ob2` inlines, not LTCG magic.

The asymmetry between binaries is **not** optimization level — it's that dc3 had
a leaked PDB/.map giving its functions names+addresses, while RB3's are
anonymous `fn_8XXXXXXX`. dc3 is therefore a **Rosetta Stone** for retail Milo
(same flags, named functions): match RB3 by transferring dc3's labels via
shared string content (`tools/fingerprint_match.py`) or structural similarity
(Ghidra+BinDiff).

## Source provenance (important)

Two sibling repos feed this one. Pick the right source per directory:

- **`src/system/` (Milo engine) ⟵ `../dc3-decomp`.** Dance Central 3 is the
  *same Milo engine*, already 360-ported (uses `RTL_CRITICAL_SECTION`,
  `xdk/XBOXKRNL.h`, etc. — not Wii's `revolution/OS.h`). Its engine headers and
  `.cpp` files compile under the 360 toolchain, so they are the correct base.
  Compiled with the same `/O1 /Oi /GR /EHsc` retail flags as us; the leaked
  PDB/.map (`ham_xbox_r.map`) gives it named functions — that's *our* asymmetric
  advantage, not a cleaner build.
- **`src/band3/`, `src/network/` (RB3 game code) ⟵ `../rb3` (rb3-Wii **DEV**
  decomp).** Important: `../rb3` is the Wii *development* build's decomp, not
  retail. It retains `MILO_ASSERT` source-path strings and named functions that
  the retail Xbox build stripped — a **richer source oracle** for cross-binary
  identification. Wii-targeted (MWCC PowerPC), needs Wii→360 porting.
- **`src/xdk/`, `src/system/stlport/`, toolchain ⟵ `../dc3-decomp`.**

**Caveat (from the project owner):** dc3-decomp is *newer* than RB3. Its engine
code may have subtle behavioral differences or version incompatibilities. When a
file misbehaves, cross-check against rb3-Wii's equivalent and merge intent — do
not assume dc3's version is correct for RB3.

## Two build tracks

**1. X360 decomp-matching build** — compile-to-match the retail XEX (MSVC X360).

```bash
ninja                    # dtk SPLIT -> MSVC compile -> objdiff report -> progress
python3 configure.py     # regenerate build.ninja (after editing objects.json/splits.txt)
```

dtk is the local **jeff** fork at `../jeff`; `configure.py` defaults `--dtk`
there. **objdiff is also a local fork** at `../objdiff` (freeqaz/objdiff,
with custom pattern-detector work and normalized-diff changes) — pass
`--objdiff ../objdiff` to use it instead of the default v3.2.1 download.
After editing `config/45410914/config.yml` or `splits.txt`, `touch`
`config.yml` to force a re-SPLIT (ninja doesn't track splits.txt as a dep).

**2. Native engine build** (`native/`, x86_64 Linux + clang) — runs the engine
on the host. Currently boots headlessly and loads RB3 `songs.dta`.

```bash
cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build && ./build/rb3-dta <songs.dta>
```

Full native-port build recipe, hard-won lessons, and link-stub regeneration are
in Claude's memory: `project_native_port.md`. Highlights: needs dc3's dual-target
`types.h` (LP64 int-width vs Xbox ILP32 long); `Symbol::Init()` mandatory before
interning; rb3-xenon's RB3 engine is more complete than DC3's so several dc3
`_Native` shims are redundant; ~74 rendering/MIDI/synth/Win32 symbols off the DTA
path are satisfied by weak stubs in `native/src/dta_link_stubs.s`.

**Architectural goal:** the native engine should eventually be **extracted into a
standalone `../milo-native-engine`** repo — a shared native Milo runtime consumed
by the per-game decomps (rb3-xenon, dc3-decomp, …) rather than each carrying its
own `native/`. For now it lives in `native/` and borrows from `../dc3-decomp`.

## Build wiring

- `tools/defines_common.py` — include paths. **STLport must come first**, then
  `src/xdk/LIBCMT` (C CRT), then `src`, `src/system`.
- `config/45410914/objects.json` — declares which `.cpp` files to compile and
  their match status. New files: add here as `NonMatching`.
- `config/45410914/splits.txt` — pins per-object section ranges so dtk emits
  per-unit target `.obj` + `.s` for objdiff to compare against. Pin **just
  `.text`** for a new cluster; on next `ninja` (after `touch config.yml`) dtk
  auto-derives and back-fills the matching `.pdata` range. Other sections
  (`.rdata`, `.data`) need manual pinning if the TU has them.
- `tools/project.py` — patched so objects in `objects.json` get a compile edge
  even without a `splits.txt` address range (compile-only scaffolding).
- `tools/fingerprint_match.py` — function-identification tool (extract / report
  / autoid / identify subcommands). Indexes all 66,838 RB3 functions by
  referenced strings/callees/constants; cross-refs strings against
  `../rb3/src` + `../dc3-decomp/src` to propose source-file mappings. Generates
  `fingerprints.json` + `autoid.json` (gitignored, regenerable). See
  `project_function_identification.md` in Claude's memory.
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
- `Progress: 0.00% matched` is the current baseline. Denominator is the **whole
  binary** (11,790,708 code bytes / 66,003 functions), so this is the honest
  dc3-comparable metric — there's no denominator gaming. Matches register only
  when a unit has both (a) pinned section ranges in `splits.txt` and (b) a
  compiled `.obj` that objdiff equates byte-for-byte with the dtk target `.obj`.
- **Case sensitivity:** dc3's tree has case-variant files (e.g. `vec.cpp` vs
  `Vec.cpp`) that collide on Windows but coexist on Linux. Use the name dc3's
  `objects.json` actually builds (lowercase `vec.cpp`, `mtx.cpp`).

## Matching phase (active)

The pipeline is proven end-to-end on `MasterAudio.cpp` (2026-05-26): pinning a
real `.text` range in `splits.txt` produces a dtk target `.obj` + per-object
`.s`. The remaining work is per-target: derive splits → port source so it
compiles → diff via objdiff. See `project_rb3_xenon_roadmap.md` Phase 5 and
`project_function_identification.md` in memory for state.

### Splits-bootstrap recipe (per new cluster)

1. Run `tools/fingerprint_match.py autoid` to get source-file proposals.
2. For each tight cluster (≥3% density, ≥3 corroborating strings, NOT
   `Symbols*.cpp` which is a systematic FP), compute `[min(fn), max(fn)+size)`
   = the `.text` span. Cross-check the strings against `../rb3/src` or
   `../dc3-decomp/src` to confirm cluster identity.
3. Add to `splits.txt`:
   ```
   FooBar.cpp:
       .text       start:0xAAAAAAAA end:0xBBBBBBBB
   ```
4. `touch config/45410914/config.yml && ninja`. dtk emits
   `build/45410914/asm/FooBar.s` + `build/45410914/obj/FooBar.obj` and
   auto-derives the matching `.pdata` range (back-filled into `splits.txt`).

### Obj patchers (dormant — wire when matching pressure grows)

`scripts/` holds dc3's MSVC object patchers (anon-ns, guard, bool-mangle,
atexit-scope, dynamic-init, regswap, transplant) — copied but **not wired**.
See `scripts/README.md`. Activate by mirroring dc3's `configure.py`
`custom_build_steps["post-compile"]` (dc3 `configure.py:301-357`) when a real
match-vs-target diff is blocked on MSVC symbol-naming quirks.

### Identification tooling

- `tools/fingerprint_match.py` — see Build wiring above. Generates the
  identification table used to derive splits.
- Cross-binary identification (planned): Ghidra + BinDiff transfer dc3's
  named functions (from leaked `ham_xbox_r.map`) onto RB3's anonymous
  `fn_8XXXXXXX` by structural similarity. BinDiff installed at
  `/usr/bin/bindiff`; BinExport plugin ships at `/opt/bindiff/extra/ghidra/`;
  XEXLoaderWV source cloned at `/home/free/code/milohax/XEXLoaderWV/` (needs
  rebuild for Ghidra 12.1 — installed prebuilt is 12.0.1).

## Phase tracking

Memory files at `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/`:
- `project_rb3_xenon_roadmap.md` — overall phase tracking + current state.
- `project_function_identification.md` — the fingerprint-match approach.
- `project_native_port.md` — native host engine build (separate from matching).
- `project_jeff_fork.md` — local jeff dtk fork (RB3-retail fixes).
- `feedback_verify_assumptions.md` — verify load-bearing claims via Opus
  subagent before committing to them (killed the `.xidata`-lever plan
  pre-emptively this way).
- `feedback_plans_with_refs.md` — cross-session plans must embed
  doc/file/URL references for cold pickup.
- `feedback_autonomy.md` — execute autonomously on rb3-xenon.

Live plan files (per-stream, written by planning agents) live at
`~/.claude/plans/rb3-xenon-*.md`.
