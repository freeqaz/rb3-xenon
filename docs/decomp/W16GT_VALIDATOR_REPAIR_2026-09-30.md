# W16-GT — repair two CONTRADICTED alias groups on main (2026-09-30)

## Why

`python3 tools/icf_alias_finder.py --validate` reported **FAIL, 2 CONTRADICTED (FATAL)** on main
`025aeb87`, after two landings that each added a map name the alias file had folded elsewhere:

| group | folded spelling | now map-resident at | introduced by |
|---|---|---|---|
| `0x827e4880` (survivor `Sort@TrackWidgetImp<RndMultiMesh::Instance>`) | `?Sort@?$TrackWidgetImp@VMeshInstance@@@@UAAXXZ` | `0x827e48c0` (pairs at 100.0) | W16-GP `025aeb87` |
| `0x826e3ce8` (survivor `DoneWithSong@VocalPlayer`) | `?SongSectionOnly@VocalPlayer@@QBA_NAAM0@Z` | `0x826e4208` (pairs at ~71.52) | W17-CLEAN-VP `63f5467a` |

Two distinct resident retail bodies cannot be one ICF fold. For `Sort`, the bodies also call different
`_S_sort` instantiations, and the group's own 2026-08-19 `UNDER_PARTITIONED_ICF_CLOSURE` repair note
already recorded that its members disagree on resolved operands.

**Process defect, owned by the coordinator:** the W16-GP gate script printed the validator verdict but
did not gate the push on it, and the GP review searched alias groups only for the four `_S_sort`
spellings, not for the three `Sort()` names the lane also added. The W17 landing did not run the
validator. Rule going forward: any landing that adds map names runs the validator, and the push is
conditional on it.

## Change

Both memberships moved from `folded` to `withdrawn` with class `NOT_A_FOLD_DISTINCT_RETAIL_ADDRESSES`.
Nothing pruned; groups kept (now `folded: []`).

## Prediction (written before measuring)

Withdrawing forgiveness can only hold or lower the score. Expected **Δ0 functions / Δ0 bytes /
masked Δ0**: `Sort` is virtual (reached through the vtable, no `bl` charge to expose), and
`SongSectionOnly` callers resolve to a correctly-named retail address either way. Validator must go
**FAIL (2 contradicted) → PASS (0 contradicted)**, group total unchanged at 1,663. Any exposed charge
is a real wrong-callee the alias was hiding and is recorded as measured.

## Measured

`tools/ab_measure.py --from-dirty`, map class, forced re-split on both legs (leg B split=1,
renamer_patched=1,834), both legs at a `symbols.txt` fixed point, 0 recompiles:

| | leg A | leg B | Δ |
|---|---|---|---|
| matched_functions | 44,237 | 44,237 | **+0** |
| masked_equal | 23,323 | 23,323 | **+0** |
| honest | 20,914 | 20,914 | **+0** |
| code% | 41.125480 | 41.125480 | **+0.000000** |
| fuzzy | 50.507107 | 50.507107 | **+0.000000** |
| units at 100% (mpn / fuzzy) | 201 / 177 | 201 / 177 | 0 / 0 |

Prediction held exactly: neither withdrawal exposed a charge. The validator result is recorded in the
merge commit (run on main after landing, with the push conditional on it).
