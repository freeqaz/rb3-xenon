# SongParser port — FINALIZE handoff (VERIFIED, COMPLETE)

**Branch:** `wt-songparser` (worktree `/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-songparser`)
**Base:** main `7e296f9`
**TU:** `system/beatmatch/SongParser.cpp`  (unit `default/SongParser`)
**Checkpoint commit:** `6b222cd` (port on disk); this finalize extends it.

## STATUS: COMPILES CLEAN · VERIFIED · 1 strict-100 pin + 1 confirmed-identity near-miss pin + 1 fuzzy kept

Verification was done **without** `tools/fresh_report.sh` (it stalls under machine
load). Instead: built only the TU's base obj via `tools/ninja-locked`, then diffed
each target with `bin/objdiff-cli` against the dtk-split target obj (renamed via
`obj_target_symbol_renamer.py`).

## Build
`tools/ninja-locked build/45410914/src/system/beatmatch/SongParser.obj` → **compiles clean**
(only the expected xdk `__va_start`/`__frsqrte` intrinsic warnings). No source edits
were needed to compile — the checkpoint port was already compile-correct.

## Verified results (objdiff-cli, base vs renamed target obj)

| addr | fn | tgt/base size | raw match | verdict |
|---|---|---|---|---|
| 0x8275DFD0 | `SongParser::GetNoStrumState(int, DifficultyInfo&)` | 88 / 88 | **100.00%** (score 0/2200) | **STRICT PIN** — byte-equal, leaf, zero relocs |
| 0x8275F8B8 | `SongParser::IsPartTrackName(const char*, const char**) const` | 152 / 152 | **99.47%** (score 20/3800) | **PIN** — code byte-identical; residual is 2 unnamed rodata string labels |
| 0x8275F2C8 | `SongParser::CheckDrumFillMarker(int, bool)` | 192 / 132 | 65.83% | **FUZZY KEPT** — real MILO_WARN codegen divergence, no pin |

### GetNoStrumState — strict 100%
Leaf, 88 B, no external relocations. All 22 instructions byte-equal. Unambiguous pin.

### IsPartTrackName — 99.47%, code byte-identical
All 38 instructions are `equal` **except** the symbol names of two rodata string
constants: target references `lbl_82108308` / `lbl_82106A68`; our base references the
named MSVC constants `??_C@_04...PART` / `??_C@_04...HARM`. Verified from `band.exe`:
the "PART\0" literal sits at VA 0x82108310 and "HARM\0" at VA 0x82106A78 (the labels are
the enclosing data symbols). The code (two `strncmp(...,4)` calls + PART/HARM branch
structure) is identical. This is a *data-symbol-naming* residual, not a codegen
difference — a confirmed-identity near-miss. Pinned per the "partial matches count /
confirmed high-confidence identity" policy; reaches strict-100 once those two rodata
labels are named in symbols.txt.

### CheckDrumFillMarker — 65.83%, fuzzy kept (NOT pinned)
Target is 192 B with a full frame + `String` temp copy-construction
(`??0String@@QAA@ABV0@@Z` / `??1String@@UAA@XZ`) feeding the `MILO_WARN` for the
Keyboards-C8 warning; our base compiled to a 132 B leaf with that String/warn path
collapsed (MILO_WARN expansion differs on rb3-xenon). This is a genuine source/macro
divergence, not a codegen near-miss — identity is plausible but not byte-confirmable,
so left unpinned (`When unsure, DON'T pin — just keep the source`). Source rides in as
NonMatching.

## What was committed (this finalize, on top of `6b222cd`)
- `scripts/target_symbol_map.json` — **ADD-ONLY**, 2 entries:
  - `"0x8275dfd0": "?GetNoStrumState@SongParser@@QAA?AW4NoStrumState@@HAAVDifficultyInfo@1@@Z"`
  - `"0x8275f8b8": "?IsPartTrackName@SongParser@@QBA_NPBDPAPBD@Z"`
  - (mangled names extracted from the built COFF symtab via `extract_decomp_symbols`'s
    parser, not hand-guessed.)
- `config/45410914/splits.txt` — SongParser.cpp block trimmed to only the two pinned fns:
  ```
  SongParser.cpp:
      .pdata      start:0x82235F60 end:0x82235F68   # IsPartTrackName unwind (RUNTIME_FUNCTION)
      .text       start:0x8275DFD0 end:0x8275E028   # GetNoStrumState  [VA, VA+0x58)
      .text       start:0x8275F8B8 end:0x8275F950   # IsPartTrackName  [VA, VA+0x98)
  ```
  Dropped the provisional CheckDrumFillMarker `.text` (0x8275F2C8) and its `.pdata`
  (0x82235F18) since it is not pinned. GetNoStrumState is a **leaf** — confirmed no
  RUNTIME_FUNCTION entry in `band.exe` `.pdata`, so it gets no `.pdata` range.
- (from checkpoint, unchanged) `src/system/beatmatch/SongParser.cpp` — all 3 members
  kept; `config/45410914/objects.json` — `SongParser.cpp: NonMatching`.

## Self-checks
- **Splits overlap:** my 3 ranges have no overlap with any of the other 1720 ranges
  (nor each other); sizes match objdiff target sizes exactly (0x58 / 0x98 / pdata 8).
- **ICF alias:** HONEST. Both pins are real-bodied anchors far above the 44 B stub-fold
  threshold (88 B branching enum logic; 152 B strncmp PART/HARM) with distinctive,
  non-generic bodies — no ICF-fold ambiguity. `icf_alias_check.py --tu SongParser.cpp`
  returns HONEST (empty set) against the not-yet-rebuilt whole-binary report; the manual
  size/body reasoning is the operative verdict.

## Net
- strict-100 pins: **1** (GetNoStrumState).
- confirmed-identity near-miss pins: **1** (IsPartTrackName, code-100 / 99.47% raw).
- fuzzy kept (source only, no pin): **1** (CheckDrumFillMarker, 65.83%).
- false pins: **0**. ICF: HONEST.
