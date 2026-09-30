# W16-GZ predictions — packed local-static guard sweep, 13 named rows

Written and committed BEFORE the whole-binary A/B measurement, per house rule
("deltas compose, absolutes do not" / predict before you measure). All 13 rows
are committed on branch `w16-gz` (worktree `/home/free/tmp/wt-w16-gz`), off
main `9de0f3339`. Per-row numbers below are taken directly from each row's own
commit message (single-symbol `objdiff-cli diff -p . -u <unit> '<symbol>' -f
json`, full build both legs, no `--build` flag so all six post-compile object
patchers ran).

## Rows that reached fuzzy 100.0 (byte-exact) — 9 rows

These move from "not counted at all" (fuzzy well below 100, and in most cases
`target_size != base_size` before the fix, i.e. a real code-shape difference,
not just an arg/reloc diff) to fully counted: the whole row's bytes join
`matched_code` and the row joins `matched_functions`.

| row | commit | before fuzzy | after fuzzy | bytes (target=base after) |
|---|---|---:|---:|---:|
| AccomplishmentCategory::Configure | bcc8c2ba9 | 38.57 | 100.0 | 188 |
| BandSongMgr::RankTierToken | 052c68349 | 24.96 | 100.0 | 224 |
| BandSongMetadata::Rank | e9b0376f4 | 48.15 | 100.0 | 236 |
| BandDirector::GetModeInst | ffb92c924 | 53.30 | 100.0 | 332 |
| OutfitConfig::MeshAO PropSync | 9247e951e | 59.92 | 100.0 | 360 |
| GameConfig::GetController | 790b5cebe | 70.62 (regressed to 53.68 mid-attempt, then fixed) | 100.0 | 404 |
| BandDirector::EnterVenue | 09f2bcb1d | 61.43 | 100.0 | 604 |
| MicInputArrow::Handle | 01cac5408 | (pre-fix score not re-verified this pass; see caveat below) | 100.0 | 712 |
| VocalTrackDir::ApplyFontStyle | 2a4d6abbb | 74.32 (verified today, see below) | 100.0 | 1164 |

**Predicted `matched_code` delta from these 9 rows: +4,224 bytes.**
**Predicted `matched_functions` delta from these 9 rows: +9** (assuming each
was not already counted under `mpn` before the fix — plausible since all 9
show a real target/base size mismatch pre-fix, i.e. missing instructions, not
merely arg-only penalties).

⚠ Caveat on `MicInputArrow::Handle`: its 100.0/712-byte score is from the
prior session (not re-verified with a fresh single-symbol `objdiff-cli diff`
in this session) — flagged so the actual A/B measurement is the ground truth,
not this table, if it disagrees.

## Rows stopped short of fuzzy 100.0 per the stop rule — 4 rows

All four have `target_size == base_size` (the guard-layout/size gap is fully
closed) and were stopped because `run_diff_inspect mode=diagnose` showed every
remaining `diff_arg` instruction "Explained by root causes" (register swaps /
offset shifts / symbol relocations), "Unexplained: 0", and `diff_op: none`
(zero logic/opcode mismatches) — i.e. pure register-allocation/scheduling
noise, not a named source construct.

| row | commit | fuzzy | residual diff_arg count | diff_op |
|---|---|---:|---:|---|
| MetaPerformer::GetSetlistMaxVocalParts | d6682f9d5 | 90.21 | 22 | none |
| MetaPerformer::PartPlaysInSet | 99c52f0f9 | 90.82 | 25 | none |
| MetaPerformer::GetHighestDifficultyForPart | 2df2e01f5 | 91.96 | 23 | none |
| PlayerLeaderboard::OnSelectRow | 9a2ef621d | 92.70 | (diff_op: none, all diff_arg explained) | none |

**Predicted `matched_code` delta from these 4 rows: +0 bytes** — `matched_code`
sums bytes only for rows at `fuzzy == 100`, and none of these four reach it.

**Predicted `matched_functions` delta from these 4 rows: uncertain, possibly
+4.** Per project doc `hub_measurement.md`: `mpn` (`match_percent_normalized`,
what `matched_functions` counts on) *excludes arg-only penalties*, while
`fuzzy_match_percent` does not. Since all remaining charges on these 4 rows
are `diff_arg` (register swap / offset shift / symbol relocation — all
arg-level) with zero `diff_op` (logic) mismatches, it is plausible each of
these 4 rows already reads `mpn == 100` despite `fuzzy < 100`, which would
mean they were ALREADY counted as "matched functions" pre-fix (i.e. sizing
the gap to zero and clearing the diff_op mismatches was what flipped `mpn`,
not just their bit-guard fix in isolation) — or that they flip to `mpn == 100`
as a result of this fix with zero byte contribution. Genuinely uncertain
without pulling per-row `match_percent_normalized` from a full `report.json`,
which `objdiff-cli diff` does not emit at all. **Flagged explicitly as a
prediction with real uncertainty — the whole-binary A/B is the tiebreaker.**

## Combined prediction

- `matched_code` (bytes): **+4,224** (from the 9 crossed rows only)
- `matched_functions`: **+9 to +13** (9 certain from crossed rows, 0-4
  additional and uncertain from the stopped rows per the `mpn` caveat above)
- `masked_equal_functions`: **+0 expected** — no aliasing/map file touched by
  this lane (guardrail: `scripts/symbol_aliases.json` and
  `scripts/target_symbol_map.json` untouched), no reason for any ICF-fold
  disclosure count to move.
- No row is expected to **fall** — every edit only adds previously-missing
  local statics matching retail's own guard layout; no existing 100%-fuzzy
  row's source was touched by any of the 13 commits (verified per-row diffs
  above only touch the 13 named functions' own bodies).

Measurement to follow with `tools/ab_measure.py` (chosen invocation mode
documented in the final row report, since `--pick <ref>^..HEAD` across 13
commits is not supported by the tool per its `--help`).
