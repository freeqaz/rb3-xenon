# Port handoff — SongData.cpp (rb3-xenon)

Branch: `wt-songdata` (based on main 7e296f9). Worktree:
`/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-songdata`.
TU: `src/system/beatmatch/SongData.cpp` (1312 lines, MWCC → MSVC PPC-Xenon port),
wired NonMatching in `config/45410914/objects.json`.

## Result
- Baseline matched (true pre-port): **10682**.
- After (pinned true-100 only): **10687** → **net +5 strict**.
- ICF verdict: **HONEST** (`icf_alias_check.py --tu SongData.cpp`):
  5 matched = 2 real-bodied anchors + 3 own-getter stub-folds;
  **0 of the 3 stubs oracle-attribute to a foreign TU** → not ICF-alias inflation.

## PINNED (true-100, `match_percent_normalized == 100.0`, byte-equal)
| VA | size | pct | fn |
|---|---|---|---|
| 0x82753FD0 | 496 (0x1F0) | 100.0 | `SongData::Poll` — real body, has .pdata 0x82235728 |
| 0x8274DA90 | 300 (0x12C) | 100.0 | `SongData::MakeBackupTracks` — real body, has .pdata 0x822350B8 |
| 0x8274C500 | 24 (0x18)  | 100.0 | `SongData::GetPhraseList` — leaf getter, no .pdata |
| 0x8274CB58 | 24 (0x18)  | 100.0 | `SongData::GetSubmixes` — leaf getter, no .pdata |
| 0x8274BB20 | 20 (0x14)  | 100.0 | `SongData::GetGemListByDiff` — leaf getter, no .pdata |

Compiler-emitted mangled names verified against the built `SongData.obj` COFF
symbol table (machine 0x01f2 = PPC BE); they match `target_symbol_map.json`
exactly. Only Poll + MakeBackupTracks carry `__ehfuncinfo$/__unwindtable$`,
consistent with keeping exactly those two .pdata records (BeginAddress read from
`orig/45410914/band.exe` `.pdata` RUNTIME_FUNCTION table).

## DROPPED (norm < 100 — honest negatives, pins removed)
| VA | size | norm% | fn | reason |
|---|---|---|---|---|
| 0x8274C230 | 720 | 56.2 | `SongData::ValidateVocalSPPhrases` | body diverges; largest gap |
| 0x8274C518 | 184 | 45.4 | `SongData::AddMultiGem` (virtual) | body diverges |
| 0x8274BF50 | 656 | 96.4 | `SongData::UnflipGems` | near-miss (96.4%), NOT byte-equal — dropped |

Their `.text` + `.pdata` split ranges and `target_symbol_map.json` entries were
removed. `SongData.cpp` source (all 8 functions) is retained — dropping a pin
only removes the byte-equal *claim*, not the ported code.

## Leads (next agent)
- `UnflipGems` (96.4%) is the highest-ROI residual — likely a single regswap /
  branch-shape / bool-materialization delta. Run `/permute` or objdiff
  `run_diff_inspect` on it before hand-editing.
- `ValidateVocalSPPhrases` (56.2%) and `AddMultiGem` (45.4%) are structural gaps
  (control-flow/inlining divergence) — diagnose before re-attempting.
- New headers added by the port: `DrumMixDB.h`, `GameGemDB.h`, `TimeSpanVector.h`.
  `TickedInfo.h::operator=` got a real fix (`return *this;` was missing).

## Verify method
`rm target_symbol_renames.stamp; touch config.yml; tools/fresh_report.sh`
(precision gate). Per-function `match_percent_normalized` read from
`build/45410914/report.json` (functions keyed by mangled name; the record
`address` field is unit-relative and unusable as a VA — correlate by name via
`target_symbol_map.json`).
