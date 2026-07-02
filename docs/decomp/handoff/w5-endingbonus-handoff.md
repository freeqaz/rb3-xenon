# WAVE-5 lane handoff — EndingBonus (port + wire + pin)

Branch: `w5-endingbonus`  Worktree: `/home/free/tmp/wt-w5-endingbonus`
Base: main `5cb96d4`. Commits: `f3521b6` (port + 3 worklist targets), `3d8480a`
(cascade Reset -> true-100), plus a docs commit.

## What landed

Ported `src/system/bandobj/EndingBonus.cpp` (Wii MWCC -> MSVC X360), wired
`system/bandobj/EndingBonus.cpp: NonMatching` into `config/45410914/objects.json`
(engine module, mirrors BandCharDesc), added a bounded splits unit + 5 ADD-ONLY
`scripts/target_symbol_map.json` VA->mangled entries. Compile-gate clean.

### Header fix (REQUIRED — a lander must keep this)
`src/system/bandobj/EndingBonus.h` was pre-ported but did NOT compile standalone:
it uses `DECLARE_REVS` / `HANDLE_CHECK` / `REGISTER_OBJ_FACTORY_FUNC`, which live in
`obj/ObjMacros.h`, NOT `obj/Object.h`. Added `#include "obj/ObjMacros.h"` as the
first include (mirrors BandScoreboard.h). Because the include guard fires before
`bandobj/UnisonIcon.h` is pulled, this ALSO fixes UnisonIcon.h's identical latent
missing-include (UnisonIcon.h still lacks its own include — harmless while only
EndingBonus.cpp consumes it, but a future UnisonIcon.cpp port should add it there).
The C4005 macro-redefinition warnings (ObjMacros.h vs Object.h) are the SAME benign
pattern BandScoreboard.cpp already emits — not errors.

## Per-id outcomes

COMPOSED FULL REPORT (fresh `report.json`, all_source rebuilt, 779 objs) — the
authoritative gate. **EndingBonus unit = 5/5 matched, unit fuzzy = 100.0%.**
Whole-binary matched_functions 10897 -> **10902 (+5, zero regressions)** — neighbours
MoveMgr 24/123, StreakMeter 3/14, BandScoreboard 15/21 all unchanged; all 5 pins
report in their OWN unit (default/EndingBonus).

| symbol | VA | size | interactive cli norm% | report norm% | verdict |
|---|---|---|---|---|---|
| `?UnisonEnd@EndingBonus@@QAAXXZ` | 0x822C1F18 | 92 | 100.0000 | **100.0000** | strict (worklist) |
| `?Reset@EndingBonus@@QAAXXZ` | 0x822C2610 | 128 | 100.0000 | **100.0000** | strict (BONUS) |
| `?UnisonStart@EndingBonus@@QAAXH@Z` | 0x822C2FE8 | 124 | 99.8387 | **100.0000** | strict / report-norm-100 size-exact (worklist) |
| `?Failed@MiniIconData@EndingBonus@@QAAXXZ` | 0x822C1EA0 | 68 | 99.7059 | **100.0000** | strict / report-norm-100 size-exact (worklist) |
| `?Reset@MiniIconData@EndingBonus@@QAAXXZ` | 0x822C25C0 | 80 | 99.5000 | **100.0000** | strict / report-norm-100 size-exact (BONUS, load-bearing) |

All 5 are SIZE-EXACT (target_size==base_size) and identity-certain. The interactive
cli sub-100 for three of them is pure reloc/callee-name/callee-divergence residue on
byte-identical branches — the composed report normalizes it to 100 (exactly the
"cli 99.4-99.7 size-exact = report-normalized 100" recipe rule). ICF honesty: none
of the 5 addrs appear in icf_aliases.map; the two >44B true-cli-100 anchors
(UnisonEnd 92B, Reset 128B) are real bodies. Interactive-cli residual detail below
(useful for a future UnisonIcon lane):

- **UnisonStart** residual = 1 `bl` to `SetIconOrder` (0x822C2B58), which is REAL
  divergence (target 528B vs base 492B, 79.65% — permuter-class, NOT pinned). A
  bl to a size-MISMATCHED callee propagates a diff into the caller, so UnisonStart
  cannot reach 100 until SetIconOrder is matched (out of reach here).
- **Failed** residual = 1 `bl fn_822C1130` = `UnisonIcon::Fail` (cross-TU, unnamed).
  A future UnisonIcon.cpp lane naming 0x822C1130 flips Failed to 100.
- **MiniIconData::Reset** residual = `bl` to `MiniIconData::SetUsed` (same-TU,
  unnamed) + `UnisonIcon::Reset` (cross-TU, unnamed).
- **Reset reached true-100** precisely because its only same-TU callee
  (MiniIconData::Reset) is SIZE-EXACT: a bl to a size-exact callee matches even if
  that callee has its own internal name-residuals. That is the mechanism a lander
  should understand — Reset's map entry DEPENDS on MiniIconData::Reset's map entry
  existing (drop it and Reset falls back to 99.5%).

## What a lander must know
- Map is ADD-ONLY; all 5 entries are correct identities (verified vs the compiled
  EndingBonus.obj COFF symtab + confirmed by direct `bl` targets from UnisonStart/
  Reset). Two are true-100 (UnisonEnd, Reset). The other three are size-exact fuzzy
  (report-normalized-100 candidates); keep them (worklist targets + Reset's
  dependency).
- Splits: single EndingBonus.cpp unit, 5 disjoint `.text` ranges + 5 `.pdata`.
  Overlap self-check = 0/0. dtk re-serializes `.pdata` on split; ranges are already
  tight to real function extents (Reset 0x822C2610..0x822C2690 = dtk's 128B).
- NOT pinned: SetIconOrder (0x822C2B58) — real 528/492 divergence, left unclaimed.
- No MILO_DEBUG landmine in this TU (asserts gate on HX_NATIVE = off in the match
  build; MILO_ASSERT -> (void)(cond), MILO_WARN -> (void)sizeof()).
- The other ~25 EndingBonus methods + the stlport vector<MiniIconData> template
  instantiations live in the same gap (0x822C1EA0..~0x822C3068 for the real methods)
  but are unclaimed — a follow-up could pin the leaf MiniIconData setters (each calls
  exactly one UnisonIcon:: fn) once a UnisonIcon lane names those cross-TU callees.

## Placement
Clean gap between MoveMgr.cpp `.text` end 0x822C1E58 and StreakMeter.cpp start
0x822C4930. No carving of a neighbour unit. All claimed ranges < 0x822C4930.
