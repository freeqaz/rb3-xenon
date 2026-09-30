# W16-HJ — seven map rows settled on retail bytes (2026-09-30)

Branch `w16-hj`, worktree `~/tmp/wt-w16-hj`. Three rows were briefed
(`0x825a36b8`, and the two TrackWidget dtors W16-HB left unnamed). The
coordinator later added four W16-HE "wrong map name" rows (`FindFrameWithLeadIn`,
`__destroy_range_aux<NewReleaseEntry>`, the `_Slist_base` ctor, and the
`_List_iterator` ctor). Settling `0x825a36b8` turned up a source defect, and
fixing that touched two more rows (`0x825a0760`, `0x825a03c0`).

## Predictions (written before the A/B)

Row scores from main's `report.json`, compared with the branch build's rows.

| VA | before | after | Δmatched | Δcode | Δfuzzy B |
|---|---|---|---|---|---|
| `0x825a03c0` hash_map<Symbol,float> reader | 79.16 (mpn 79.81) | 100 | +1 | +124 | +25.8 |
| `0x8263b168` | 17.33 (mpn 18.79) | 100 | +1 | +96 | +79.4 |
| `0x825a36b8` | 99.70 (Leaderboard) | 99.75 (BandSongMetadata) | 0 | 0 | +0.04 |
| `0x825a0760` | 100 | 100 (renamed) | 0 | 0 | 0 |
| `0x8235fba8` (side effect, §1.4) | 99.77 | 0 (orphaned) | 0 | 0 | −175.6 |
| `0x827e2b30`, `0x827e2c98` | 0 / 0 | 83.37 / 83.37 | 0 | 0 | +126.7 |
| `0x82605c38` | 61.76 | 0 (nulled) | 0 | 0 | −42.0 |
| `0x8264e920` | 80.00 | 0 (nulled) | 0 | 0 | −9.6 |
| `0x826758f8` | 70.00 | 0 (unpaired in Game) | 0 | 0 | −5.6 |
| `0x8263b0b0` | 0 | 0 (unpaired in CharHair) | 0 | 0 | 0 |

**Predicted whole-binary: Δmatched_functions +2, Δmatched_code +220 B, Δfuzzy ≈ −1 B
(≈ 0 pp).** Call sites of renamed or nulled addresses are not modelled. The biggest
unknowns are the store-panel callers of `0x82605c38`, which lose a charged name and
gain a forgiven placeholder.
