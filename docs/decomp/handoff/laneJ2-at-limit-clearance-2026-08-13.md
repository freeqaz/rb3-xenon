# laneJ2 — three stale AT_LIMIT floor certifications cleared (2026-08-13)

> **STATUS (2026-08-13):** current. Records a local `decomp.db` state change,
> which is gitignored and therefore not recoverable from git. Companion to
> [MAP_NAME_INJECTIVITY.md](../MAP_NAME_INJECTIVITY.md) and commits `03946970`
> (map repair) + `202a7859` (gate), merged as `bc9c6bd3`. *(This doc first cited
> `6092f524`; that is the pre-rebase lane commit and is not an ancestor of
> `main`.)*
>
> **Revised 2026-08-13 by lane J3.** Two claims below were wrong and are
> corrected in place: the resync instruction (`sync_match_percent.py` **cannot**
> reset these rows — see "How the percents were actually cleared"), and the
> verdict-schema account (`''` was not a schema state, it was a fourth value
> this lane invented — see "The schema gap"). The percents and verdicts have
> since been cleared; nothing here is outstanding.

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

| id | symbol | unit | pct (was) | verdict (was) |
|---|---|---|---|---|
| 130424 | `?Null@Symbol@@QBA_NXZ` | `default/FilePath` | 91.2857 | AT_LIMIT |
| 130115 | `?DataDir@UIPanel@@$4PPPPPPPM@EM@AAPAVObjectDir@@XZ` | `default/StorePanel` | 99.75 | AT_LIMIT |
| 130132 | `??$__destroy_aux@ULevelData@@@stlpmtx_std@@YAXPAULevelData@@ABU__false_type@0@@Z` | `default/system/synth_xbox/FxSendMeterEffect` | 99.5 | AT_LIMIT |

All three now read `current_percent` NULL, `best_percent` NULL, `verdict` NULL;
the percents above are the pre-repair readings, kept for the record.

The `DataDir` row is a **third** case beyond the two the investigation named; it
surfaced only after the `$4` thunk-callee test disproved both `DataDir` VAs.
Whole-table `AT_LIMIT` count went 2,978 → 2,975.

## What was changed, and how

Lane J2 wrote, through `scripts/orchestrator/database.update_function_status`
rather than by hand-editing sqlite:

- `verdict` `AT_LIMIT` → `''`
- `verdict_reason` → a paragraph naming the cause, the map commit, and an
  explicit instruction not to re-certify a floor or close the residual here

`current_percent` and `best_percent` were left at their pre-repair readings
(91.2857 / 99.75 / 99.5), on the reasoning that they are **measured** fields
owned by `scripts/sync_match_percent.py` and the repair had not yet landed —
writing a number ahead of the measurement that produces it would be the same
class of error this lane was repairing. That reasoning was sound; the resync
instruction it implied was not.

## How the percents were actually cleared (and why the resync instruction was false)

**Superseded instruction, kept so nobody retries it.** This doc told the lander
to run `ninja build/45410914/report.json && python3 scripts/sync_match_percent.py`
and said "all three rows will fall to 0.0". **They do not fall at all.** Two
independent reasons, either sufficient:

1. **The updater only visits symbols the report names.** Its loop is
   `for symbol, report_info in report_funcs.items()` (`sync_match_percent.py:269`);
   a DB row whose symbol is absent from the report is counted into
   `stats["not_in_report"]` (line 336) and **never written**. After the repair
   all three names are absent: they are not values in the applied map at all —
   verified by loading `scripts/target_symbol_map.json` through the renamer's own
   `load_address_map`, the same view the gate scores — so the renamer cannot
   stamp them onto any target obj, and the report enumerates target-obj symbols.
   (Evidence that it enumerates the target and not our source: pre-repair,
   `?Null@Symbol@@QBA_NXZ` was listed under `default/ADSR` and
   `default/MetaMusic`, units whose sources define no such function. It was
   there because the map put the name there.)
2. **`best_percent` is monotone by construction.** The write is
   `best_percent = MAX(COALESCE(best_percent, 0), ?)` (`sync_match_percent.py:344`,
   and identically in `database.update_function_status:1239`). It can raise a
   value and can never lower one, so no resync of any kind clears a stale
   `best_percent`.

**What works, and what was run.** The pattern already used by
`scripts/reset_false_complete.py:44-49` — raw SQL, straight at the columns:

```sql
UPDATE functions
   SET current_percent = NULL,
       best_percent    = NULL,
       verdict         = NULL
 WHERE id IN (130424, 130115, 130132);
```

**Executed 2026-08-13** against the primary checkout's `decomp.db`, backup taken
first. This is **done, not outstanding** — do not run it again, and do not run
the resync above expecting it to do anything. Verified afterwards: all three
rows read NULL/NULL/NULL; none appears in the near-miss pool, in the Ghidra
export selection, or in any selection predicated on a non-null percent;
whole-table `AT_LIMIT` is 2,975; zero empty-string verdicts remain.

A resync (`ninja build/45410914/report.json && python3 scripts/sync_match_percent.py`)
is still worth running after landing — it refreshes every *other* row the repair
moved — but it is not what clears these three.

## The schema gap, stated plainly

**Corrected 2026-08-13.** This section originally called `''` "the schema's only
non-certified state" and said only three values exist. Both are wrong, and the
error is not cosmetic — it describes a row as sitting in a supported state when
it was sitting in a state nothing in the codebase knows about.

The actual distribution over all 86,675 rows, at the moment of the review:

| verdict | rows |
|---|---:|
| `NULL` | 77,164 |
| `COMPLETE` | 6,533 |
| `AT_LIMIT` | 2,975 |
| `''` | **3** — these rows, and nothing else in the table |

The non-certified state is `NULL`, held by 77,164 rows. `''` was **invented by
this lane** and held by exactly the three rows it wrote. After the clear above:
`NULL` 77,167 / `COMPLETE` 6,533 / `AT_LIMIT` 2,975, zero `''`.

`''` is worse than a synonym for `NULL`; it is a value that half the codebase
cannot see:

- It **passes** the worklist filters, which are written as
  `(verdict IS NULL OR verdict NOT IN ('COMPLETE', 'AT_LIMIT'))` — e.g.
  `database.py:1795`, `:1881`, `scripts/batch_check.py:51`.
- It is **excluded** by the readers that key on the *absence* of a verdict —
  `scripts/get_progress.py:94` (`WHERE verdict IS NULL`) and
  `database.query_divergent_logic` (`AND verdict IS NULL`,
  `scripts/orchestrator/database.py:1951`). In SQL, `'' IS NULL` is false.
- Meanwhile `avg_percent` is computed as
  `SELECT AVG(current_percent) FROM functions WHERE current_percent IS NOT NULL`
  (`database.py:1489`) — **verdict-agnostic**. So while holding `''` with their
  pre-repair percents, these three rows appeared in **no** verdict bucket and
  still contributed to the headline average.

### The forcing function, which is the real finding

Lane J2 did not choose `''` over `NULL`. **`update_function_status` cannot write
`NULL`.** Its signature defaults every field to `None`, and `None` is its
sentinel for *"caller did not ask to update this"*:

```python
if current_percent is not None:      # database.py:1234
    updates.append("current_percent = ?")
...
if verdict is not None:              # database.py:1242
    updates.append("verdict = ?")
```

Passing `verdict=None` is therefore indistinguishable from not passing it, and
the only in-band way to express "clear this" through the supported API is a
value that is not `None` — i.e. `''`. That is why **every** clearing tool in
this repo bypasses the API and issues raw SQL: `scripts/reset_false_complete.py:44-49`
does exactly that, and so did the clear recorded above.

So "use the supported API, not raw sqlite" is the wrong rule for this operation,
and following it is what produced a fourth verdict value. The durable fixes, in
order of preference:

1. Give `update_function_status` an explicit clear — a sentinel distinct from
   `None` (a module-level `CLEAR` object, or per-field `clear_verdict=True`
   flags) — so that clearing is expressible in band.
2. Only then consider a real fourth verdict state such as
   `IDENTITY_UNESTABLISHED`, and **wire it into the exclusion queries** in the
   same change. A value added without wiring behaves exactly like `''` while
   *looking* like it has tool support — the declared-but-unwired defect class,
   same as the `_denylist` the loader ignored until `f3fe9ab1`.

Until (1) exists, a raw `UPDATE ... SET col = NULL` is the correct tool for a
clear, and should be recorded in a handoff like this one rather than hidden.

There is still **no verdict state meaning "target identity unestablished"**, and
that gap is real: every value that keeps a row out of the worklists asserts
something we now know to be false, and every value that tells the truth puts the
row back in the queue. These three rows are now workable, which is intended —
and with `current_percent`/`best_percent` NULL they no longer advertise 91–99.75%
to a chooser. The `verdict_reason` remains the thing standing in front of a
re-certification, which is why it is written as an instruction rather than a note.
