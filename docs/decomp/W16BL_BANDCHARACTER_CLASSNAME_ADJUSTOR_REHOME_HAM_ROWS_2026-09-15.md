# W16-BL — `0x82289748` is `BandCharacter::ClassName`'s adjustor thunk, and CY-1 moved it to `Line.cpp:` because a masked byte test on a thunk is content-free

Lane W16-BL. Worktree `~/tmp/wt-w16-bl`, branch `w16-bl`, based on main `312da843`.
Ruler `name_check` (shipped default), objdiff 4.2.9.

## 0. Baseline — verified in-tree, reproduces the briefed ledger figure exactly

Settled to a zero-work build first (build 0 did 397 edges — a fresh reflinked
worktree's split/renamer chain; build 1 did **5**, all always-run check/progress
phonies, `symbols.txt` clean). Only then read:

| key | value |
|---|---|
| `matched_functions` | 43,552 |
| `matched_code` | 4,040,380 |
| `matched_code_percent` | 39.42962 |
| `fuzzy_match_percent` | 49.699745 |
| `total_functions` | 69,240 |
| `total_code` | 10,247,068 |
| `masked_equal_functions` | 23,076 |
| rows at `fuzzy == 100` | 40,887 |

`rowset_snapshot.py` leg A vs the briefed baseline copy `~/tmp/rows_w16bl_base.json`
(a copy of `~/tmp/rows_w16bh_main.json`, original untouched): **40,887 rows both
sides, 0 gained, 0 lost.** The briefed baseline is therefore verified literally,
not inherited.

## 1. PRE-REGISTERED PREDICTION (written and committed BEFORE the build)

Edit: move splits `.text 0x82289748–0x82289754` from `Line.cpp:` to
`BandCharacter.cpp:` (by merging it back into the block CY-1 carved it out of),
and set map `0x82289748` → `?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`.

| measure | before | predicted after | Δ |
|---|---|---|---|
| `matched_functions` | 43,552 | **43,553** | **+1** |
| `matched_code` | 4,040,380 | **4,040,392** | **+12 B** |
| `matched_code_percent` | 39.42962 | ≈39.429632 | ≈+0.000012 pp |
| `total_functions` | 69,240 | 69,240 | **0** (re-home, the row exists either way) |
| `total_code` | 10,247,068 | 10,247,068 | **0** |
| `masked_equal_functions` | 23,076 | 23,076 | **0** (the row pairs by NAME, not byte signature) |
| `default/Line` | 77 fns / 13,040 B / 66 matched / 8,360 B | 76 / 13,028 / 66 / 8,360 | −1 fn, −12 B denominator |
| `default/BandCharacter` | 619 fns / 70,680 B / 536 matched / 48,312 B | 620 / 70,692 / 537 / 48,324 | +1 fn, +12 B both sides |

Set-diff prediction: **exactly one GAINED row**,
`default/BandCharacter::?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`
(12 B), and **zero LOST rows** — `fn_82289748` sits at `fuzzy 0` today so it was
never a member of the `fuzzy == 100` set.

If the new row lands below 100 the only possible charge is the relocation NAME of
the branch target (an adjustor thunk is `lwz`/`subf`/`b` and the `b` is the sole
relocated word); both sides spell `?ClassName@BandCharacter@@UBA?AVSymbol@@XZ`, so
I expect no charge.

## 2. Why the prediction is safe to make — three preconditions tested, not assumed

(filled in below)

