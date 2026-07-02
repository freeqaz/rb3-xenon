# Landing plan: wt-bt-land (BandTrack +7) — 2026-07-02

**Verdict: LAND.** Minimal delta = **ONE line** in `config/45410914/objects.json`.
Everything else the lane commit (`ab9ad9f`) introduced is ALREADY on main.

---
## POSTSCRIPT — LANDED 2026-07-02 (+7)

Executed exactly as planned. Single line added to `config/45410914/objects.json`
(`"system/bandobj/BandTrack.cpp": "NonMatching"`, unique anchor between
BandSongPref/BandWardrobe). No other tracked file touched.

- Compile gate (direct cl.exe on main's BandTrack.cpp): PASS, exit 0, 219530-byte obj, warnings-only.
- Splits overlap self-check: CLEAN (no new splits added; block already present).
- Composed verify (`fresh_report.sh`, no WARN, single run): `measures.matched_functions` **10862 -> 10869 (+7)**. Matched@100% units 10819 -> 10824.
- A/B vs `/tmp/land_baseline_2026-07-02.txt` (unique names >=99.99%): newly matched = EXACTLY the 7 BandTrack identity fns; regressions (names lost) = **0**.
- Honesty: the substantial fns are among the 100% matched — DropIn 0x60(96B), DropOut 0x80(128B), Deploy 0x94(148B), plus SetMaxMultiplier 0x18, PlayerDisabled 0x24, SetBandMultiplier 0x1C, CombineStreakMultipliers 0x14. GetTrackIcon/UserName (0x54 each) stay at 46.86% (out of scope). Not tiny-stub-only.
- Committed objects.json only. Lane branch `wt-bt-land`/worktree left untouched for the coordinator to prune.

Planner: read-only verification pass. The lander applies the edit, runs the
composed verify, A/Bs vs baseline, then commits. Do NOT git-merge/rebase the
lane branch — main has diverged and merging its objects.json would revert work
(see "Why not merge the branch" below).

---

## 1. What the lane commit `ab9ad9f` changed vs. what main already has

Lane commit: `ab9ad9f` "system/bandobj: port+pin BandTrack +7", 2026-06-23.
Files it touched and their status on main tip (`70dc072`, report
`build/45410914/report.json` = **10862 matched**):

| File | Lane change | State on main (`70dc072`) | Action needed |
|---|---|---|---|
| `src/system/bandobj/BandTrack.cpp` | +1011 lines (new ported source) | **Tracked, byte-IDENTICAL** to lane (`git diff ab9ad9f HEAD -- <file>` empty) | none |
| `config/45410914/splits.txt` | +`system/bandobj/BandTrack.cpp:` block (5 .pdata + 9 .text ranges) | **Present at line 3114, identical** (only diff is a trailing blank-line separator) | none |
| `config/45410914/symbols.txt` | split `fn_8233AA60` blob→`fn_8233AA60`(0x8)/`fn_8233AA68`(0x18)/`fn_8233AA80`(0x28); promote `lbl_8233ABB0`→`fn_8233ABB0`(0x14) | **All 4 entries present** (lines 143924-143932), match lane intent | none |
| `scripts/target_symbol_map.json` | +9 addr→MSVC-mangled BandTrack pins | **All 9 pins present** (verified each addr) | none |
| `config/45410914/objects.json` | +`"system/bandobj/BandTrack.cpp": "NonMatching"` | **MISSING** (`grep -c BandTrack.cpp objects.json` = 0) | **ADD 1 line** |

The report already lists unit `default/system/bandobj/BandTrack` with all 9 target
functions **correctly named** but `fuzzy_match_percent=None` — because with no
`objects.json` entry, no base `.obj` is compiled, so the renamer/objdiff never
pairs the named target fns against a base body. Adding the objects.json line
compiles the base object → the 7 identity fns pair at 100% → they count as matched.

## 2. The exact edit (the ONLY tracked-file change)

File: `config/45410914/objects.json`
Insert one line, 3-space indent, between `BandSongPref.cpp` (line 54) and
`BandWardrobe.cpp` (line 55) — same alphabetical/sibling slot the lane used and
the same top-level bandobj module dict:

```
   "system/bandobj/BandSongPref.cpp": "NonMatching",
   "system/bandobj/BandTrack.cpp": "NonMatching",
   "system/bandobj/BandWardrobe.cpp": "NonMatching",
```

Recommended mechanism (Edit tool, unique anchor):
- old: `   "system/bandobj/BandSongPref.cpp": "NonMatching",\n   "system/bandobj/BandWardrobe.cpp": "NonMatching",`
- new: `   "system/bandobj/BandSongPref.cpp": "NonMatching",\n   "system/bandobj/BandTrack.cpp": "NonMatching",\n   "system/bandobj/BandWardrobe.cpp": "NonMatching",`

No other tracked file is edited. `symbols.txt`, `splits.txt`,
`target_symbol_map.json`, `BandTrack.cpp` are already correct on main.

### Why NOT merge/rebase the branch
`git diff HEAD ab9ad9f -- config/45410914/objects.json` is LARGE: the lane's
objects.json predates 9 days of main additions (BandLeadMeter, CheckboxDisplay,
StarDisplay, BandPatchMesh, GemTrackDir, TrackWidget, JoypadController, SongData,
TrackWatcherImpl, SongParser, BeatMatchController, BeatMatcher, ...). A branch
merge / `git checkout ab9ad9f -- objects.json` would DELETE those entries and
silently zero many units. Hand-add the single line only.

## 3. Compile gate — PASS

Ran main's `src/system/bandobj/BandTrack.cpp` through the direct cl.exe gate
(note: `wibo` is not on PATH; use `build/tools/wibo`):

```
cd /home/free/code/milohax/rb3-xenon && build/tools/wibo \
  build/compilers/X360/16.00.11886.00/cl.exe \
  /I src/system/stlport /I src/xdk/LIBCMT /I src /I src/system /I src/system/oggvorbis \
  /I src/band3 /I src/network /I src/system/speex/include \
  /nologo /wd4355 /wd4164 /c /GR /O1 /Oi /EHsc /TP /Fo/tmp/bt_gate.obj \
  src/system/bandobj/BandTrack.cpp
```

Result: **exit 0**, produced `/tmp/bt_gate.obj` (219530 bytes). Only warnings, no
errors — 9 days of header drift (Synth.h/StarDisplay.h/CharEyes.h etc.) did NOT
break it. Benign warnings: C4392/C4391 (xdk intrinsic decls), C4005 ObjMacros/
Object.h macro redefs, C4003 Part.h MemAlloc, C4068 unknown-pragma at
BandTrack.cpp:968/969/1011 (MWCC `#pragma`s MSVC ignores). All pre-existing /
present when the lane originally landed +7.

The build's actual ninja `cflags` for sibling bandobj TUs (e.g. BandWardrobe,
build.ninja:198-209) are **byte-identical** to the gate command above, so the
full composed build compiles this TU with the same flags → same clean result.

## 4. Splits overlap self-check — CLEAN (baseline is good)

SOP step-4 snippet against main's current `config/45410914/splits.txt`:
```
pdata 0 overlaps over 955 ranges
text  0 overlaps over 1039 ranges
```
No adjacent-pin collision; landing adds no new splits (block already present).

## 5. Landing steps (for the lander)

1. Apply the section-2 objects.json edit (hand-edit; `git add` ONLY
   `config/45410914/objects.json`). Do not touch any other tracked file, and do
   not touch the untracked in-flight files (TourSavable.cpp, dc3_name_eligible.py,
   auto_*.obj, global_fuzzy_pairs.json, 'Z:tmp*', '9').
2. Composed verify (the only truth):
   ```
   cd /home/free/code/milohax/rb3-xenon
   rm -f build/45410914/target_symbol_renames.stamp
   touch config/45410914/config.yml
   NINJA_JOBS=12 tools/fresh_report.sh
   ```
   Re-run once if a splits-only-divergence WARN appears (known false positive).
3. Read `measures.matched_functions` from `build/45410914/report.json`.
   - Baseline: **10862**. Expected after: **10869 (+7)** — possibly +8/+9 if
     header drift nudged GetTrackIcon/UserName over the line (lane had them at
     47.1%, out of scope; treat +7 as the target, anything <+7 is a regression to
     investigate).
4. A/B honesty check vs `/tmp/land_baseline_2026-07-02.txt` (10816 unique names
   >=99.99%). Regenerate the post-build matched-name snapshot the same way the
   baseline was made, then:
   ```
   comm -13 <(sort /tmp/land_baseline_2026-07-02.txt) <(sort /tmp/post_bt.txt)
   ```
   MUST be exactly the BandTrack identity fns and nothing foreign. Expect these 7
   MSVC-mangled symbols to appear (none are in the baseline today — verified all 9
   grep to 0):
   - `?SetMaxMultiplier@BandTrack@@QAAXH@Z`
   - `?SetBandMultiplier@BandTrack@@QAAXH@Z`
   - `?CombineStreakMultipliers@BandTrack@@QAAX_N@Z`
   - `?PlayerDisabled@BandTrack@@QAAXXZ`
   - `?DropIn@BandTrack@@QAAXXZ`
   - `?DropOut@BandTrack@@QAAXXZ`
   - `?Deploy@BandTrack@@UAAXXZ`
   And `comm -23` (names LOST vs baseline) MUST be empty — no foreign-unit
   regression. If any non-BandTrack name appears/disappears, STOP and diagnose.
5. Honesty note: this is a body-port + objects.json-wiring land (splits/pins
   already on main, unchanged by this edit), so it is exempt from the
   `icf_alias_check` span-pin gate per SOP. The lane's own commit already recorded
   `icf_alias_check --tu BandTrack.cpp = HONEST, 0 foreign-stub folds`. The
   section-4 A/B is the operative honesty gate here.

## 6. Commit message (lander, after verify passes)

```
system/bandobj: wire BandTrack.cpp base object (+7)

BandTrack.cpp source, splits ranges, symbols split, and 9 target_symbol_map
pins all already on main (from stale lane ab9ad9f, 2026-06-23). Only the
objects.json wiring line was missing, so the base object was never compiled and
the 9 named target fns reported fuzzy=None. Add
"system/bandobj/BandTrack.cpp": "NonMatching" -> base object pairs the 7 identity
fns at 100%.

Composed verify: 10862 -> 10869 matched_functions (+7). A/B vs
land_baseline_2026-07-02: only the 7 BandTrack identity fns added, 0 lost.
```
(Adjust the +N and final count to the measured result. No Co-Authored-By line.)

## 7. Rollback notes

- The only tracked change is one line in `config/45410914/objects.json`. To undo,
  **hand-delete that line** (never `git checkout`/`restore`/`stash` — concurrent
  agents have uncommitted work).
- After undo, `rm -f build/45410914/target_symbol_renames.stamp && touch
  config/45410914/config.yml && NINJA_JOBS=12 tools/fresh_report.sh` returns the
  count to 10862.
- The stale lane branch/worktree (`wt-bt-land` / `.claude/worktrees/wt-bandtrack`)
  is left untouched; the coordinator prunes it.

## Appendix — companion lane (wt-vtd-land) is likely a PRUNE
Not this task, but noted from the coordinator's brief: both VocalTrackDir pins
(`?OnSetDisplayMode@...` 0x822E6D08, `?SetupNetVocals@...` 0x822E9598) are already
in main's map/splits and both fns report 100% in `default/VocalTrackDir` → pure
duplicate, prune not merge. (Refuted ICF mis-pins PreLoad/Deploy/TutorialReset
must NOT be resurrected.)
