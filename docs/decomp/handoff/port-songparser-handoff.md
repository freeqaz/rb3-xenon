# SongParser port — FINALIZE handoff (BUILD-BLOCKED, needs re-verify)

**Branch:** `wt-songparser` (worktree `/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-songparser`)
**Base:** main `7e296f9`
**TU:** `system/beatmatch/SongParser.cpp`
**Checkpoint commit:** `6b222cd` — `wip(SongParser): checkpoint salvaged port before verify`

## STATUS: PORT COMMITTED, VERIFICATION NOT COMPLETED (environmental build starvation)

The salvaged port is on disk and safely committed. The precision-gate build
(`tools/fresh_report.sh`) was launched successfully and ran, but the shared build
machine went into sustained CPU saturation (system load 200–218 on 32 cores,
worsening over 50+ minutes from concurrent agents) and **`SongParser.obj` never
compiled** within the available window. `.ninja_log` advanced by only ~17 s of
activity across ~15 min of wall time; total obj count frozen at 728. My ninja
(PID 121727) stayed **alive and healthy** the whole time (STAT `S`, not crashed,
not deadlocked) — it was simply starved of CPU. This is 100% an environmental
resource wall, NOT a defect in the port.

Because the build never produced the object, I could **not**:
- verify which of the 3 target addresses reach true-100% byte-equal,
- extract the MSVC-mangled symbols the compiler actually emitted,
- add `target_symbol_map.json` pins (deliberately left UNCHANGED — zero false pins),
- run the twice-deterministic re-check or `tools/icf_alias_check.py`.

## What IS on disk / committed (checkpoint `6b222cd`)
- `src/system/beatmatch/SongParser.cpp` — 65-line port of the 3 worklist members only
  (`GetNoStrumState`, `CheckDrumFillMarker`, `IsPartTrackName`). Header members/types
  all verified present in `src/system/beatmatch/SongParser.h` + `GemInfo.h` (NoStrumState
  enum) + `TrackType.h` (kTrackRealKeys=5 / kTrackRealGuitar=6 / kTrackRealGuitar22Fret=7).
  Port looks compile-clean by inspection (no missing include/type observed); compile was
  never reached to confirm.
- `config/45410914/objects.json` — `+ system/beatmatch/SongParser.cpp: NonMatching`.
- `config/45410914/splits.txt` — SongParser.cpp block with the 3 provisional `.text`
  ranges + 2 `.pdata` ranges (see below). These are the PRIOR agent's pins; they are
  PROVISIONAL and must be trimmed to `[VA, VA+size)` and dropped for any addr that
  does not verify true-100%.
- `scripts/target_symbol_map.json` — **UNCHANGED** (no SongParser pins added; correct,
  since none are verified).

## Target addresses (BSim identities — UNVERIFIED, some may be wrong)
| addr | intended fn | BSim simconf | provisional split .text |
|---|---|---|---|
| 0x8275dfd0 | `SongParser::GetNoStrumState(int, DifficultyInfo&)` | 39.9 | 0x8275DFD0–0x8275E028 |
| 0x8275f2c8 | `SongParser::CheckDrumFillMarker(int, bool)` | 22.5 | 0x8275F2C8–0x8275F390 |
| 0x8275f8b8 | `SongParser::IsPartTrackName(const char*, const char**) const` | 17.4 | 0x8275F8B8–0x8275F950 |

Provisional `.pdata`: 0x82235F18–0x82235F20, 0x82235F60–0x82235F68.
Note: only 2 `.pdata` entries for 3 fns — the prior agent may have expected one fn to
fold/inline. Re-derive `.pdata` from the built binary during finalize.

## TO FINALIZE (re-run when build machine load is sane, e.g. load < ~40)
1. `cp build/45410914/report.json /tmp/SongParser_base.json` (baseline = 10682).
2. `rm -f build/45410914/*/target_symbol_renames.stamp; touch config/45410914/config.yml; tools/fresh_report.sh`
   — must actually reach `SongParser.obj`. If it fails to compile, fix minimally
   (MSVC idiom / include), study a sibling ported `src/system/*.cpp` for house style.
3. For each of the 3 addrs, read its match in `build/45410914/report.json`. Keep ONLY
   true-100% byte-equal. For each kept: extract the emitted MSVC-mangled symbol via
   `scripts/extract_decomp_symbols.py` (or llvm-nm on the `.obj`; do NOT hand-guess),
   add `"0x<addr>": "<mangled>"` to `scripts/target_symbol_map.json` (ADD-ONLY), set its
   splits `.text` range to exactly `[VA, VA+size)`. DROP any pin for a non-matching addr.
4. Re-run the composed build TWICE (run1==run2). Require after_matched > 10682, 0
   baseline-100 regressions. Then
   `tools/icf_alias_check.py --worktree <this> --baseline-report /tmp/SongParser_base.json`
   (exit 1 ⇒ ICF-alias inflation, those pins are ≤44B stub-folds → report INFLATED, drop them).
5. Amend/extend the commit with the finalized state (per-fn size/pct, baseline→after,
   ICF verdict) and update this handoff.

## Verdict so far
- matched (verified true-100): **none yet** (build never reached the obj).
- dropped: **none yet** (nothing pinned in the symbol map — zero false pins by design).
- net delta: **0** (unverified; no map pins landed).
- ICF verdict: **N/A** (no build to check).
- leads: build machine was at load 200–218 during the entire session; retry when quiescent.
