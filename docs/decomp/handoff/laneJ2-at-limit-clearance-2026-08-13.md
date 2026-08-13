# laneJ2 — three stale AT_LIMIT floor certifications cleared (2026-08-13)

> **STATUS (2026-08-13):** current. Records a local `decomp.db` state change,
> which is gitignored and therefore not recoverable from git. Companion to
> [MAP_NAME_INJECTIVITY.md](../MAP_NAME_INJECTIVITY.md) and commit `6092f524`.

## What an AT_LIMIT verdict asserts, and why these three were wrong

`AT_LIMIT` certifies that a row's residual is **intrinsic** — that our source is
at its floor and the remaining delta cannot be closed. All three rows below were
signed off against a target body whose name claim this lane has since disproved
(`Symbol::Null`, `DataDir`) or shown to be unadjudicable (`__destroy_aux`).

The verdict reached the right *outcome* for the wrong *reason*: the row could not
improve because **the target is a different function**, not because our source is
at its floor. That is the "at-limit floor sign-off" seam in the correctness
boundary, arrived at from a direction the floor doctrine does not screen for — a
floor certification pattern-matched on a residual whose real cause was a false
identity claim in `scripts/target_symbol_map.json`.

## The rows

| id | symbol | unit | pct | was |
|---|---|---|---|---|
| 130424 | `?Null@Symbol@@QBA_NXZ` | `default/FilePath` | 91.2857 | AT_LIMIT |
| 130115 | `?DataDir@UIPanel@@$4PPPPPPPM@EM@AAPAVObjectDir@@XZ` | `default/StorePanel` | 99.75 | AT_LIMIT |
| 130132 | `??$__destroy_aux@ULevelData@@@stlpmtx_std@@YAXPAULevelData@@ABU__false_type@0@@Z` | `default/system/synth_xbox/FxSendMeterEffect` | 99.5 | AT_LIMIT |

The `DataDir` row is a **third** case beyond the two the investigation named; it
surfaced only after the `$4` thunk-callee test disproved both `DataDir` VAs.
Whole-table `AT_LIMIT` count went 2,978 → 2,975.

## What was changed, and how

Through the supported API — `scripts/orchestrator/database.update_function_status`
— **not** by hand-editing sqlite:

- `verdict` `AT_LIMIT` → `''` (the schema's only non-certified state)
- `verdict_reason` → a paragraph naming the cause, the map commit, and an
  explicit instruction not to re-certify a floor or close the residual here

Nothing else was touched.

## What was deliberately NOT changed

`current_percent` and `best_percent` are left at their pre-repair readings
(91.2857 / 99.75 / 99.5). They are **measured** fields owned by
`scripts/sync_match_percent.py`, and the map repair is on `laneJ2-map-injectivity`
and not yet on `main`, so `main`'s `report.json` still reads the old values.
Writing a number ahead of the measurement that produces it would be the same
class of error this lane is repairing.

**Action for whoever lands the branch:** run
`ninja build/45410914/report.json && python3 scripts/sync_match_percent.py`
afterwards. All three rows will fall to 0.0, because the target symbols no longer
carry those names.

## The schema gap, stated plainly

There is **no verdict state in this schema that means "target identity
unestablished"**. Only three values exist across all 86,675 rows — `''`,
`COMPLETE`, `AT_LIMIT` — and the worklist queries exclude exactly
`COMPLETE` and `AT_LIMIT`. So:

- every value that keeps a row out of the worklists asserts something we now
  know to be false, and
- every value that tells the truth puts the row back in the queue.

Inventing a fourth value would not help: the exclusion lists are literal string
comparisons, so an unrecognised verdict behaves exactly like `''` while
*looking* like it has tool support — a declared-but-unwired safeguard, which is
the same defect class as the `_denylist` the loader ignored until `f3fe9ab1`.

Consequence to be aware of **until the branch lands and the resync runs**: these
three rows are now workable AND still read 91–99.75%, which is more inviting than
they were. The `verdict_reason` is the only thing standing in front of that, and
it is why it is written as an instruction rather than a note. If this pattern
recurs, the fix is a real fourth verdict state wired into the exclusion queries —
not a convention.
