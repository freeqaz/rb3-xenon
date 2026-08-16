# `decomp.db.functions.verdict` — the state machine

> **STATUS (2026-08-16):** current. Doc of record for the verdict column.
> Companion to
> [handoff/laneJ2-at-limit-clearance-2026-08-13.md](handoff/laneJ2-at-limit-clearance-2026-08-13.md)
> and `decomp-bench/archive/runs/icf-alias-collision-2026-08-13/REPAIR_LANDED.md`.

## The domain

`verdict` is a free-text `TEXT` column, but it has exactly four legal states.
The constants live in `scripts/orchestrator/database.py`; **import them, never
re-spell the strings** — a copy is how a state drifts out of sync with the
filters that enforce it.

| state | means | in work queue? | counts as Done? | percentage meaningful? |
|---|---|---|---|---|
| `NULL` | not adjudicated — **open work** | yes | no | yes |
| `COMPLETE` | byte-exact | no | **yes** | yes (100) |
| `AT_LIMIT` | our source is at its floor; the residual is **intrinsic** | no¹ | **yes** | yes |
| `IDENTITY_UNESTABLISHED` | the **target body is not established to be this function** | **never** | **no** | **no — must be NULL** |

¹ `AT_LIMIT` is suppressed by a caller flag (`exclude_at_limit`), which
**defaults to `False`** in `get_next_function` and `query_functions`. So
AT_LIMIT rows *are* offered by default. `IDENTITY_UNESTABLISHED` is different:
it has no such switch (see "Why two axes").

`''` (empty string) is **not** a state. Lane J2 wrote it on 2026-08-13 as the
only in-band way to say "not COMPLETE, not AT_LIMIT", because
`update_function_status` could not write `NULL`. It passed the
`verdict NOT IN (...)` filters and was excluded by the `verdict IS NULL` ones,
so three rows sat in **no bucket at all** while still feeding `avg_percent`.
`update_function_status` now raises `ValueError` on any verdict outside
`KNOWN_VERDICTS`, and `CLEAR` exists so clearing is expressible in band.

## `IDENTITY_UNESTABLISHED`

### What it asserts

That we cannot attribute a target body to this symbol. Not that the function is
hard; not that our source is wrong; that **the premise of the row is unsound**.

Two shapes produce it, both real, both currently populated:

- **Unadjudicable.** More than one candidate address and no instrument that
  separates them. `??$__destroy_aux@ULevelData@@...` (id 130132): both
  candidate VAs (`0x82b5b1d0`, `0x82b63ec8`) tail-jump the same unnamed
  `fn_82B69220`, so the relocation test that settled the sibling rows cannot
  discriminate. Both are on the `_denylist` in
  `scripts/target_symbol_map.json` — the map deliberately claims neither.
- **Located but not landed.** Evidence exists, but the map does not carry it,
  so nothing in the pipeline binds the name to a body.
  `?Null@Symbol@@QBA_NXZ` (id 130424): its home was located at `0x8227c70c`
  (the only body in `.text` with the required shape — offset-0 load, contents
  of `gNullStr`) and never applied. **Located is not established.**

### Why it needs to exist

Every other value gets these rows wrong, and one of them is dangerous:

- `AT_LIMIT` certifies a floor. All three 2026-08-13 rows were signed off
  AT_LIMIT against bodies later disproved. The verdict reached the right
  *outcome* for the wrong *reason*: the row could not improve because the
  target is a different function, not because our source is at its floor.
- `COMPLETE` asserts byte-exactness against a body that may not be the
  function.
- `NULL` returns the row to the work queue. This is the dangerous one.
  `?Null@Symbol@@QBA_NXZ` became the **top near-miss in `default/FilePath` at
  91.29%**, one edit from byte-exact against a body proven not to be that
  function — and **byte-exactness is this project's sole admission gate**. It
  would have said yes.

### Semantics — binding

1. **Not open work.** Excluded from every selector *unconditionally*, not
   behind a caller flag. There is no switch that re-enables it.
2. **Not complete, not at a floor.** Absent from `DONE_VERDICTS`. It gets its
   own row in `get_progress.py`, in **neither Done nor Remaining**.
3. **Its percentage is meaningless.** `current_percent` and `best_percent` are
   forced `NULL`, and `avg_percent` excludes the verdict *explicitly* rather
   than relying on the NULL. `best_percent` matters most here: its normal
   update is a monotone `MAX(COALESCE(best_percent,0), ?)`, which can raise a
   value and can **never lower one**, so a stale reading is otherwise
   unretractable.
4. **Never offered to any model, worklist, or Ghidra export.** The Ghidra case
   is not cosmetic: `tools/ghidra/build_symbol_map.py` pushes *names onto
   addresses*, i.e. it asserts identity. Exporting one of these rows would
   launder an open question into a stated fact in the analysis DB, downstream
   of nothing.
5. **Exit is EVIDENCE work, not source work.** Establish which address the
   symbol denotes, land it in `scripts/target_symbol_map.json`, *then* call
   `clear_identity_unestablished`. Do **not** edit source to close the
   residual. `clear_...` refuses to act on a row not in this state, so the
   inverse cannot silently un-exclude a funclet or a floor. Percentages stay
   `NULL` after a clear — they must be re-measured against the newly
   established body, never restored from readings taken against the wrong one.

## Why two axes (`verdict` **and** `excluded = 1`)

`mark_identity_unestablished()` sets both. That is deliberate, and it was
chosen on measurement, not taste.

This repo has **two** established "is this workable" idioms, and consumers
disagree about which they use:

- `(verdict IS NULL OR verdict NOT IN ('COMPLETE','AT_LIMIT'))` — 6 sites,
  including the primary work selectors. A new verdict value **passes** this.
- `excluded = 0` — 15 sites, including `query_functions_by_priority`,
  `query_divergent_logic`, `get_priority_stats`,
  `query_functions_by_merged_category`, `obj_regswap_patcher`. These do not
  look at `verdict` at all.
- `verdict IS NULL` — 2 sites. A new verdict value is correctly excluded here
  **by accident**; `excluded = 1` alone would be wrong here.

Measured on a scratch copy of the live DB, with the incident's own row restored
to its pre-repair 91.2857%, counting selectors that still offered it:

| shape | leaks |
|---|---|
| baseline (`verdict NULL`) | 12 |
| new verdict value, **unwired** | **12 — zero improvement** |
| `excluded = 1` alone, unwired | 11 |
| both, unwired | 11 |
| **both, wired (shipped)** | **0** |

Two conclusions, and the first is the important one:

- **The shape was never the deliverable; the wiring is.** A verdict value
  added without teaching the consumers behaves exactly like `''` while
  *looking* like it has tool support. That is the declared-but-unwired defect
  class, and it is why `_DEAD_VERDICTS` in `scripts/grind/worklist.py` is a
  **closed allow-list** that must be edited by hand.
- Given wiring is mandatory either way, set **both** axes: it is the only
  shape that an unaware future query gets right by accident *whichever* idiom
  it happened to use, and the row stays self-describing — someone reading it
  sees the state in the `verdict` field they actually look at, rather than
  `NULL` ("open, work it") plus a boolean two columns over.

`verdict` stays the **primary** axis because `NULL` means "not adjudicated"
and we *have* adjudicated — identity-unestablished is a positive finding.
`excluded` is a boolean that already means "not a real workable target" (set by
`scripts/grind/classify_funclets.py` for EH funclets); overloading it alone
would conflate a *permanent non-target* with an *exit-able unbound* one and
destroy the ability to count them separately. They stay distinct: 168 rows
carry `excluded = 1` alongside `COMPLETE`/`AT_LIMIT`, so the flag never
implied a verdict.

## How to write it

Never by hand-editing sqlite. Never through a bare `UPDATE`.

```bash
# preview (default; writes nothing)
python3 scripts/mark_identity_unestablished.py --id 130424 --reason '...'

# apply
python3 scripts/mark_identity_unestablished.py --id 130424 --reason '...' --apply

# audit
python3 scripts/mark_identity_unestablished.py --list

# inverse, once identity IS established and the map entry has landed
python3 scripts/mark_identity_unestablished.py --clear --id 130424 \
    --reason 'homed to 0x8227c70c, map commit <sha>' --apply
```

`--reason` is **required and must be non-empty**: it is the only evidence trail
for why a row left the work queue.

★ **Run it against a scratch copy first** (`--db /path/to/copy.db`). Peer agents
are usually live in the shared `decomp.db`, and the recorded failure this state
closes is precisely *a clearance written to shared state ahead of the repair
that justified it*.

## Clearing a field (the `CLEAR` sentinel)

`update_function_status` uses `None` to mean "caller did not ask to update this
field", so `None` cannot express a clear. Pass `CLEAR`:

```python
from orchestrator.database import CLEAR, update_function_status
update_function_status(fid, verdict=CLEAR)                 # verdict -> NULL
update_function_status(fid, best_percent=CLEAR)            # retract a stale best
```

This is strictly additive — no existing caller can reach it — and it is the
supported replacement for the raw SQL that every clearing tool previously used.

## Adding a fifth state

Do all of this, or you have shipped `''` again:

1. Add the constant to `KNOWN_VERDICTS`, and to **either** `DONE_VERDICTS` or
   `UNWORKABLE_VERDICTS`. A test asserts every known verdict is in one of them,
   because a value in neither sits in no bucket.
2. If it means "not work", add the lower-cased string to `_DEAD_VERDICTS` in
   `scripts/grind/worklist.py`. That list is closed and will not pick it up.
3. Decide its progress bucket in `scripts/get_progress.py`. `Remaining` is
   computed by *subtraction*, so a state you forget silently lands there.
4. Decide whether the writers may stamp over it: `scripts/batch_check.py`,
   `scripts/sync_match_percent.py` (`--promote`),
   `tools/refresh_permuter_db.py`, `scripts/atexit_fuzzy_verify.py`
   (`--mark-at-limit`), `mcp_server._report_result`.
5. Add tests to `scripts/orchestrator/test_verdict_identity.py` — one per
   filter, not one in aggregate.

## Known defects in neighbouring code (found 2026-08-16, NOT fixed here)

Recorded so nobody re-derives them. None is caused by this state; all were
surfaced by testing it.

- **`query_functions_by_priority` and `query_functions_for_unit_completion`
  are dead code.** Both `SELECT ease_score, impact_score, confidence_score`,
  which exist in neither a freshly-migrated DB nor the live one — they raise
  `no such column: ease_score` on any call. Their `IDENTITY_UNESTABLISHED`
  wiring is in place and tested (the test provisions the columns) but is inert
  until the selectors are repaired.
- **`functions.excluded` was never in the schema.** It was added by hand to the
  one database that matters; a DB built fresh by `init_database()` did not have
  it, so all 15 `excluded = 0` queries raised on a clean clone. Fixed here by
  **migration v19**, which is idempotent against the live DB.
- **`scripts/recon.py` reported no DB info for any symbol.** Its query selected
  `exclusion_reason`, a column that does not exist, and `_load_db_info` catches
  `Exception` and returns `None` — so the failure was silent and its
  `COMPLETE`/`AT_LIMIT` assessment branches were unreachable. Fixed here.
- **`get_stats.avg_percent` counts dead and excluded rows.** 518 `excluded=1`
  and 2,370 `live=0` rows carry percentages; the shipped average is 67.74 vs
  72.10 with them removed. Only the `IDENTITY_UNESTABLISHED` exclusion was
  added here — the `excluded`/`live` pollution is pre-existing and out of scope.
- **`get_progress.py`'s "Excluded (SDK)" denominator is `unit LIKE '%xdk%'`,
  which matches zero rows** in the current DB, and it ignores the `excluded`
  column entirely, so all 520 excluded rows sit in its denominator.
