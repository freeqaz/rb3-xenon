# W16-GK: adjudicating the `obj/Utl` block on retail bytes

**Worktree `~/tmp/wt-w16gk`, branch `w16-gk`, forked from main `2d49cc75`.**
Brief: `docs/decomp/W16_UTL_BLOCK_TARGETING_2026-09-16.md`. Every figure below was
re-measured in this tree; nothing is inherited.

## 0. The brief's census reproduces EXACTLY

`build/45410914/report.json`, unit `default/system/obj/Utl`, all numerics read
through `int()`/`float()`; 0 of 31 rows lack `fuzzy_match_percent` and 0 lack
`match_percent_normalized`, so no test here keys on a field's absence.

| row | B | fuzzy | mpn | `masked_equal` |
|---|---:|---|---|---|
| `?CopyTypeProperties@@` | 1,472 | 99.94566 | 99.945656 | False |
| `?ObjectList@@` | 504 | 99.96032 | 99.96032 | False |
| `?WalkProps@@` | 408 | 99.95098 | 99.95098 | False |
| `?PathCompare@@` | 244 | 99.83607 | **100.0** | False |
| 5 × `fn_8275*` | 5×40 | 99.5 | 100.0 | **True** |

9 rows / 2,828 B. Honest block 3 rows / 2,384 B; disclosure 200 B; excluded 244 B.
Charged-site census (`objdiff-cli diff`, no `--build`, graded ruler) confirms every
pairing the brief tabulated, and that the three honest rows carry **zero**
instruction-byte differences — 4×`bl` on one pair for `CopyTypeProperties`, 1×`bl`
each for the other two.

⚠ A screen that could not fire: my first charge census keyed on a `diff_kind`
field that does not exist in this objdiff's JSON and reported **`charged rows = 0`**
on rows that are provably below 100 — a clean, decisive-looking zero. The real key
is `match_type` (`equal` / `diff_arg`). Recorded because the shape is the
family tell, not because the slip was interesting.

## 1. Instrument gates — run in THIS tree, not inherited

- `icf_pair_adjudicate.py --selftest` → **PASS**, positive control PROVEN, negative REFUTED.
- `--chasetest` → all six controls land as specified, **in-family decoy REFUTED**.
- `--self-break` → with the destination proof removed the vacuous decoy flips to a
  wrong PROVEN, and the run reports `self-break OK -- the control discriminates`.

So the chase can both admit and refuse here.

## 2. Verdicts

*(filled in below as each target is adjudicated)*
