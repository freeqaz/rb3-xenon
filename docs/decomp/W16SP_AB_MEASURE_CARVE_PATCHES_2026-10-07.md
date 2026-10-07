# W16-SP: `ab_measure` measures carve patches (2026-10-07)

Lane W16-SP. Brief: `tools/ab_measure.py` refused the pin and carve patches of
W16-QP, QZ, QI and RC, so each lane hand-ran its A/B
(`CAMPAIGN_STATE_2026-10-07.md` §6 lever 8). The task was to make the tool
measure these patches honestly, show with controls that it still refuses what it
should, then measure and land the blocked carve rows.
Worktree `~/tmp/wt-w16sp`, branch `w16-sp`, based on main `a9dae92cf`.

## 1. Result

- **Tool change:** `1d3e435a6`. Selftest ALL PASS. All 12 new checks drive
  their branch both ways.
- **Blocked carve rows to land: none.** All six W16-QI §4.2 `Message` stubs and
  W16-QZ's `SemitoneToWhiteKey` were already repaired by W16-RC (`36e8e4d6a`,
  merged in `5aa1d53b0`). On main all six stubs are `size:0x1C`, at fuzzy 100.
  W16-RC §5 leaves only benign pad-over carves and out-of-scope `auto_*`/XDK
  carves. So this lane lands no splits, map or symbols edits.

## 2. The two refusals, and the fix

| refusal | where | cause | fix |
|---|---|---|---|
| patch touches `symbols.txt` (W16-RC, QI) | `classify`, before any build | blanket rule: "symbols.txt is derived drift" | new `symbols` kind (below) |
| pin whose split rewrites `symbols.txt` (W16-QP) | leg B's first build, rc=1 | the split-guard (`verify_split_current.py --complete`, added after ABSPLIT-1) fails the SPLIT edge when the split rewrites its input, so `resplit_fixed_point`, which exists for exactly this case, never ran | for map/splits/symbols kinds the tool owns the fixed-point verdict and sets the guard's own `SPLIT_GUARD_NO_FIXED_POINT_CHECK=1` |

How the fix works:
- **`symbols` kind.** It is measured like a splits patch: a forced re-split and
  both legs read at a fixed point.
  - `apply_patch` restores the committed `symbols.txt` *before* `git apply`.
    The old order restored it after the apply, which would have reverted the
    patch's own hunks.
  - `check_symbols_fixed_point` refuses a symbols-only patch whose leg B
    converges back to leg A's fixed point. That is the case the old rule
    guarded against: the split undid the edit.
- **The guard switch.** It is set only when both legs are iterated to a fixed
  point, and non-convergence or oscillation is still refused.
  - Every other kind runs with the guard **live**. `build_env` scrubs a
    caller's own setting.
  - A guard failure is reported as stage `split-guard` with a reason that says
    what to fix.
- **Landing.** When leg B's fixed point is not the patched file, the run saves
  `legB_symbols_fixed_point.diff` (vs HEAD) and prints `LANDING`. Landed
  without that diff, the patch makes main's next build fail the guard.
- **Two smaller fixes:**
  - Preflight saves discarded `symbols.txt` drift to the run dir. A
    `--from-dirty` caller's carve edit used to vanish silently.
  - TreeGuard restores `symbols.txt` to HEAD. R0 below shows the old tool
    handing back splits/map at leg A next to `symbols.txt` at leg B.

## 3. Measurements (all on the `name_check` ruler, run dirs `~/tmp/wt-w16sp/.ab_measure_runs/20261007-<id>`)

Scratch trees, local branches deleted after the runs:
- **QP scratch:** `d97a90c5b` (S0) un-merges `fn_82C294D8`/`fn_82C29544`
  (reverse of `a87e436fa`). `c5c9237a8` (S1) removes QP's four pins and names
  (reverse of `00b977b2b`, 3-way). `--revert S1` therefore re-adds pins and
  names only, which is the patch QP could not measure.
- **RC scratch:** `573bff58e` is a 3-way revert of `36e8e4d6a`, except the
  `0x827B8690` Achievements hunk. That hunk is entangled with `03caf1f01`'s
  Achievements_Xbox re-home, so it is kept at tip state on both legs.
  `--revert` of it gives leg B = the branch tip exactly.

| run | tool | input | predicted | measured |
|---|---|---|---|---|
| R0 `053426-r0-oldtool-qp` | main's (old) | QP pins on S1 | REFUSED at leg B build (split-guard) | **REFUSED**, stage `build`, log carries `THE SPLIT REWROTE ITS OWN INPUT`; tree left with `symbols.txt` modified |
| R1 `053608-r1-newtool-qp` | new | same | +2 fns / +212 B (QP's figure); leg B 1 extra re-split; fixed-point diff == `a87e436fa` | **+3 / +484 B**; leg A 0 extra re-splits, leg B 1; fixed-point diff **== `a87e436fa`**; `LANDING` printed |
| R2-old `054013-r2-oldtool-rc` | main's (old) | RC carve on RC scratch | REFUSED at classify | **REFUSED**, stage `classify` |
| R2 `054018-r2-newtool-rc` | new | first RC scratch (conflict resolved wrongly) | — | **REFUSED**, stage `build`: `Split 0x827B869C..0x827B87D0 overlaps with previous split`. Correct: my leg-A tree was broken |
| R2b `054123-r2b-newtool-rc` | new | RC carve on fixed scratch | +1 / +228 B (RC's hand-run) | **+4 / +508 B**; leg A **2** extra re-splits, leg B 0; "patch is complete as written" |
| R3 `054653-r3-src-on-nonfixedpoint` | new | comment-only source patch on S0 (tree not a fixed point) | REFUSED, guard live | **REFUSED**, stage `split-guard` |
| R4 `054715-r4-inert-symbols` | new | symbols-only un-merge on the tip | REFUSED, split undoes it | **REFUSED**, stage `symbols-fixed-point`: leg B `f37724ba → d60644c4` = leg A's `d60644c4` |

### 3.1 Both byte predictions failed; the row diffs account for every byte

**R1, +3 / +484 B.** The row diff of the archived leg reports shows exactly
these changes:
- **At 100:** `SetGlowLevel` 12, `LookupPitchedUVCoordinates` 200 (one merged
  row, so the Class-4 merge took effect before the read) and
  `TubePlate::AllocateFaces` 272. That is 12 + 200 + 272 = 484.
- **Not at 100:** `DrawToPlate` 3,184 B, at 99.61.
- **Left their `auto_*` units:** the five anonymous rows. Nothing else moved.

QP measured `AllocateFaces` at 86.74. It has been brought to 100 since, so the
extra +1 / +272 B is later work, not a tool defect.

**R2b, +4 / +508 B.** The row diff splits into:
- **RC's carve as RC measured it: +1 / +228.** That is the 4 `Message` stubs
  (+112), `SemitoneToWhiteKey` 96@50 → 144@100 (+144), and the 4 withdrawn
  false names `NullFile::Write`, `RndDrawable::CamOverride`,
  `GameGem::operator new` and `_Destroy_Range` (−28).
- **+2 / +72:** `BeatInfoCmp` 28 and `EventTrigger::StartAnim` 44. These are
  names RC's follow-up commit gave to carve-freed rows. They pair only once
  carved, so on this A/B their value belongs to the carve.
- **+1 / +208:** `AllocAlign`, now at 100. RC §5.4 left its body unwritten.

The total is +4 / +508 B exactly.

A new fact from R2b: the uncarved tree is **not a split fixed point** today.
Leg A converged away from it in 2 extra re-splits; the log shows jeff's Class-4
merge rejoining `AllocType` to `0x3d8`. So part of RC's carve is something the
splitter now does on its own. The tool says so (`leg A's COMMITTED symbols.txt
is NOT a split fixed point`). The old tool could not have measured this leg at
all.

## 4. Native gate

The lane touches no `src/` file, so this is a confirmation rather than a risk
check. `tools/native_build_gate.sh` in `~/tmp/wt-w16sp` at `78170c46c`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 5. Not done

- No splits, map or symbols edits landed (none were left blocked, §1).
- `--from-dirty` still treats a dirty `symbols.txt` as drift: there it cannot
  tell drift from intent. The run now saves what it discards.
- The oscillation and non-convergence refusals were not re-exercised on a real
  build. They are unchanged code, driven by the existing ABSPLIT-1 selftest
  chains.
- `CAMPAIGN_STATE_2026-10-07.md` §6 lever 8's second bullet is not edited
  (coordinator's document). It is closed by this lane.
- No merge, no push (coordinator).
