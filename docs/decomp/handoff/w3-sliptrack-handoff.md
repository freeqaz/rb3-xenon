# Port handoff — SlipTrack.cpp (rb3-xenon)

Lane: `port-SlipTrack`. Branch: `w3-sliptrack` (from `52ceb11` on main).
Worktree: `/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-w3-sliptrack`.
TU: `src/system/synth/SlipTrack.cpp` (~60-line MWCC-Wii oracle → MSVC PPC-Xenon),
wired NonMatching in `config/45410914/objects.json` engine module.

## Result
- Ported the FULL TU (9 functions: ctor, Init, ForceOn, Poll, VolumeOn,
  VolumeOff, SetSpeed, SetOffset, GetCurrentOffset). Compiles clean (only the
  pre-existing xdk `__va_start`/`__frsqrte` intrinsic warnings, shared by all TUs).
- **5 true-100 byte-equal pins** (4 worklist + 1 bonus neighbor `VolumeOff`).
- **1 confirmed near-miss** kept as NonMatching fuzzy (`Poll`, 99.46% — pure
  relocation-naming artifact, see below).
- ICF verdict: **HONEST** — `build/45410914/icf_aliases.map` (5 entries binary-wide;
  target is a debug build, ICF mostly off) contains NONE of my 6 addresses, so the
  100%s are genuine, not fold aliases. `icf_alias_check.py --tu SlipTrack.cpp` returns
  HONEST (empty set) because report.json wasn't regenerated (whole-binary build is
  off-limits on the shared machine); the direct map check is the real evidence.

## PINNED (true-100, `normalized_match_percent == 100.0`, byte-equal)
| VA | size | fn | .pdata | note |
|---|---|---|---|---|
| 0x82B7F4D8 | 0x70 | `VolumeOff` | 0x82255688 | **bonus** — not on worklist; neighbor pulled in to resolve Poll's `bl` |
| 0x82B7F548 | 0x70 | `SetSpeed`  | 0x82255690 | worklist |
| 0x82B7F5B8 | 0x70 | `SetOffset` | 0x82255698 | worklist |
| 0x82B7F628 | 0x1C | `GetCurrentOffset` | (none) | worklist; leaf getter, no RUNTIME_FUNCTION |
| 0x82B7F6B8 | 0x5C | `VolumeOn`  | 0x822556A8 | worklist |

Mangled names extracted from the built `SlipTrack.obj` COFF symtab (machine
0x01f2 = PPC BE, `cls==2` externals), NOT hand-guessed. `.pdata` BeginAddress
verified big-endian in `orig/45410914/band.exe` `.pdata` RUNTIME_FUNCTION table.

## KEPT AS FUZZY (near-miss, NOT a strict pin)
| VA | size | norm% | fn | why not 100 |
|---|---|---|---|---|
| 0x82B7F648 | 0x70 | 99.46 | `Poll` | relocation-naming only |

`Poll`'s only diffs are unresolved relocations, NOT real codegen:
- idx 5/8: the `0.0f` compare/store constant — target `lbl_82000D40` (external
  data addr, bytes not in the split obj so objdiff can't prove-equal) vs my base's
  MSVC COMDAT `__real@00000000`. Same value, different symbol name.
- idx 20: `bl VolumeOff` — now names `?VolumeOff@SlipTrack@@QAAXXZ` on BOTH sides
  (after pinning VolumeOff) yet objdiff still flags the branch reloc cosmetically.

All 28 instruction *bytes* are identical modulo those relocations; identity is
certain by adjacency (Poll sits inside the contiguous exact-match block
VolumeOff→SetSpeed→SetOffset→GetCurrentOffset→Poll→VolumeOn, all 100%). Kept in
splits+map so it lands as a correctly-attributed 99.46% partial. To reach true-100
a future pass could map the shared `0.0f` data label `0x82000D40 → __real@00000000`
(binary-wide blast radius; deferred — needs a full report to confirm no regression).

## MWCC→MSVC adaptation notes
- Headers `synth/SlipTrack.h` + `synth/Stream.h` already existed in rb3-xenon and
  matched the oracle (Stream.h is the other session's WIP — read-only for me, its
  slip virtuals were all present). Added `#include "obj/Data.h"` (for
  `DataArray::FindFloat`) and `#include "os/System.h"` (for `SystemConfig`) — the
  Wii oracle got these transitively; xenon's Stream.h includes don't pull them in.
  These are only used by `Init()` (not a worklist target), so no match impact.
- `DECOMP_FORCEFUNC(SlipTrack, Stream, SetStereoPair(0, 0))` kept verbatim; the
  macro exists in rb3-xenon `src/decomp.h` and compiles (cf. Player.cpp usage).
- STL: `std::vector<int>` iterators (`begin/end/front/push_back`) — xenon's
  stlpmtx_std handles these; the header already declared `mChannels` as
  `std::vector<int>`.

## DROPPED
None — all 5 worklist targets are accounted for (4 pinned + Poll fuzzy-kept), plus
the VolumeOff bonus pin.

## Verify method (per-function objdiff, no whole-binary build)
```
touch config/45410914/config.yml; rm -f build/45410914/target_symbol_renames.stamp
tools/ninja-locked build/45410914/config.json   # re-split -> obj/SlipTrack.obj
tools/ninja-locked pre-compile                   # renamer: fn_<addr> -> mangled
tools/ninja-locked build/45410914/src/system/synth/SlipTrack.obj   # base obj
build/tools/objdiff-cli diff -u default/SlipTrack '<mangled>' --format json -o /tmp/x.json
# read .normalized_match_percent  (NEVER -o /dev/stdout — ICF banner pollutes it)
```
Files touched: `src/system/synth/SlipTrack.cpp` (new), `config/45410914/objects.json`
(+1 line), `config/45410914/splits.txt` (+SlipTrack unit), `scripts/target_symbol_map.json`
(+6 entries). `tools/download_tool.py` + `global_fuzzy_pairs.json` in the worktree
are setup-script/tool artifacts — NOT staged.
