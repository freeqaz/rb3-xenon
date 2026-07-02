# land plan: wt-vtd-land (VocalTrackDir +2) — 2026-07-02

**Verdict: PRUNE.** The branch `wt-vtd-land` (commit `74582d8`) is a **pure duplicate**.
Every hunk it introduces is already present in main verbatim, both target functions
already report 100% and are already counted in the landing baseline. Expected
`matched_functions` delta if landed = **0**. The coordinator may delete the branch
and worktree.

Adversarial verification below — the PRUNE verdict fails if even one hunk is missing
from main; none is.

---

## 1. What the lane commit introduced (complete enumeration)

`git show 74582d8 --stat` → **exactly 2 files**, 5 insertions / 3 deletions. No
source changes, no `objects.json` changes, no `symbols.txt` changes.

`git diff-tree --name-only -r 74582d8`:
```
config/45410914/splits.txt
scripts/target_symbol_map.json
```

### Hunk A — `config/45410914/splits.txt` (VocalTrackDir.cpp range extension)
Lane changed the block from:
```
VocalTrackDir.cpp:
	.pdata      start:0x821F1FA8 end:0x821F22A0   -> end:0x821F2330
	.text       start:0x822E4180 end:0x822E847C   -> end:0x822E95FC
```
(extended `.pdata` and `.text` ends to cover the VocalTrackDir continuation block
so `SetupNetVocals` @0x822E9598 falls inside the TU span).

### Hunk B — `scripts/target_symbol_map.json` (2 new pins, add-only)
```
0x822E6D08  ?OnSetDisplayMode@VocalTrackDir@@QAA?AVDataNode@@PAVDataArray@@@Z
0x822E9598  ?SetupNetVocals@VocalTrackDir@@QAAXXZ
```

There are no other hunks.

---

## 2. Per-hunk proof that main already contains it

### Hunk A — PRESENT in main (byte-identical to the lane's *post-change* state)
`grep -n -A3 '^VocalTrackDir.cpp:' config/45410914/splits.txt` on main:
```
278:VocalTrackDir.cpp:
279:	.pdata      start:0x821F1FA8 end:0x821F2330
280:	.text       start:0x822E4180 end:0x822E95FC
```
Main already has the extended `.pdata end:0x821F2330` and `.text end:0x822E95FC` —
i.e. main = the lane's *result*, not the lane's *pre-state*. Nothing to add.

### Hunk B — PRESENT in main (both pins, add-only map)
`grep -n -e '0x822E6D08' -e '0x822E9598' scripts/target_symbol_map.json` on main:
```
13338: "0x822E6D08": "?OnSetDisplayMode@VocalTrackDir@@QAA?AVDataNode@@PAVDataArray@@@Z",
13339: "0x822E9598": "?SetupNetVocals@VocalTrackDir@@QAAXXZ",
```
Both entries already live in main's map. (The lane appended them at the tail of the
file; main carries them mid-file after a later re-serialization/union — same
address→symbol pairs, so re-applying is a no-op.)

Both hunks present ⇒ the branch's entire content is already in main. The parent
`8108fde` is confirmed an ancestor of HEAD (`git merge-base --is-ancestor` = YES),
consistent with the content having re-landed via a later map/splits union.

---

## 3. report.json confirms both functions at 100%

Build `45410914`, report timestamp Jul 2 03:27, unit `default/VocalTrackDir`:
```
100.0  ?OnSetDisplayMode@VocalTrackDir@@QAA?AVDataNode@@PAVDataArray@@@Z
100.0  ?SetupNetVocals@VocalTrackDir@@QAAXXZ
```
Both are already at 100.0% fuzzy_match_percent on the current main build.

## 4. Both functions are already in the landing baseline (→ delta 0)

`grep` of `/tmp/land_baseline_2026-07-02.txt` (the 10,817-name >=99.99% snapshot)
returns **both**:
```
?OnSetDisplayMode@VocalTrackDir@@QAA?AVDataNode@@PAVDataArray@@@Z
?SetupNetVocals@VocalTrackDir@@QAAXXZ
```
They are already counted in the baseline matched set. Landing the branch cannot add
them a second time — **expectedDelta = 0**.

---

## 5. Refuted-pins cross-check — CLEAN (nothing to exclude)

The documented VocalTrackDir ICF mis-pins are `PreLoad` / `Deploy` / `TutorialReset`
(docs/plans/decomp-state-and-roadmap-2026-06-09.md ~line 563: PreLoad pinned onto
except_data 0x8; Deploy onto a "slider.sld" fn; TutorialReset onto a static-Symbol fn).

The lane commit does **NOT** touch any of them — its only two map additions are
`OnSetDisplayMode` and `SetupNetVocals`. In the current report those three remain
un-pinned and below 100% (PreLoad 59.8%, Deploy 72.9%, TutorialReset 46.2%),
confirming the branch carries none of the refuted mis-pins. No explicit exclusion
is required.

---

## Verdict

**PRUNE.** `wt-vtd-land` / `74582d8` is a pure duplicate: both files it edits already
match main (splits ranges + both map pins), both target functions already report
100% and are already in the landing baseline, and it carries none of the refuted
mis-pins. Landing it yields 0 new matched functions and 0 map/splits changes.

- No build required (nothing to verify — the composed verify would show 10,862
  unchanged).
- Do not commit, do not merge. Coordinator may delete branch `wt-vtd-land` and
  worktree `/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-vocaltrackdir`.
