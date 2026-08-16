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
- **Absent from the map, and its only candidate refuted.**
  `?Null@Symbol@@QBA_NXZ` (id 130424). No address claims the name. The one home
  ever proposed, `0x8227c70c`, was **refuted** by lane R (`9fe65045`): it is
  interior code of `??8Symbol@@QBA_NPBD@Z` at `0x8227c6d0` — `0x8227c708`
  branches *into* it, which cannot cross a COMDAT boundary under `/Gy`, and it
  has zero inbound `bl` against 297 for `0x8227c6d0`, so an out-of-line COMDAT
  there would have been `/OPT:REF`'d away.

  **This is the sharpest illustration of why the state exists.** Every
  sub-claim of the original argument was true — `gNullStr` really is a pointer
  variable, the 28 bytes really are `return mStr == gNullStr;`, and that byte
  string really does occur exactly once in `.text`. And our build *does* emit a
  standalone `?Null@Symbol@@QBA_NXZ` COMDAT byte-identical to that retail
  fragment. Re-homing it would have paired and scored **a clean byte-exact 100%
  against a non-function** — the headline hazard, arriving with a correct body
  argument and a uniqueness proof attached. A shape match, even a unique one,
  is not a function-identity proof.

### Reading the map: check it through the loader, not the JSON

`scripts/target_symbol_map.json` has three distinct states for a name, and the
raw JSON shows only two of them. Through the renamer's own
`load_address_map()` — the view the gate actually scores — the three rows sit
like this:

| symbol | raw JSON | through `load_address_map` |
|---|---|---|
| `?DataDir@UIPanel@@$4…` (130115) | present | **claimed** at `0x826412e0` |
| `??$__destroy_aux@ULevelData@@…` (130132) | present ×2 | **denylisted** — claims neither |
| `?Null@Symbol@@QBA_NXZ` (130424) | absent | **absent** |

So "two of the three names are present in the map" — which is what a raw
`json.load` tells you — is **wrong in the way that matters**: one is *claimed*,
one is *denied*, one is *absent*, and only the loader distinguishes the first
two. `_denylist` entries carry a real string value and are filtered out at load
time. Always read the map through `load_address_map`.

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
4. **Never offered to any model, worklist, suggestion surface, or Ghidra
   export.** "Suggestion surface" is in that list because it was the one this
   lane missed: `search_functions_by_name` is not a worklist, but it backs
   `mcp_server._suggest_similar_symbols`, reached from the **`run_objdiff` MCP
   tool** whenever an agent's symbol lookup fails — so the server would answer
   "did you mean …?" with the one target the agent must not work. Severity is
   moderate, not fatal: every downstream WRITE seam refuses
   (`report_result`, `batch_check`, `atexit_fuzzy_verify`,
   `sync_match_percent`, `refresh_permuter_db`), so an agent could be pointed
   at the row but could not bank a crack on it. The false-crack gate held; the
   "never offered" claim did not. The Ghidra case
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
| baseline (`verdict NULL`) | 13 |
| new verdict value, **unwired** | **13 — zero improvement** |
| `excluded = 1` alone, unwired | 12 |
| both, unwired | 12 |
| **both, wired (shipped)** | **0** |

⚠ **13 is a FLOOR, not a total.** It is the count of consumers *this lane
enumerated*, and the first enumeration said 12 — adversarial review found a
13th (`search_functions_by_name`, below) that the sweep missed because it is
not a worklist. The "0" is likewise 0-of-the-known-13. Treat any future
"complete consumer list" claim here with the same suspicion.

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

## Side effect: a unit holding one of these can never be promoted to `Matching`

`sync_match_percent.find_complete_units` treats a unit as done only when every
row in it is `COMPLETE`/`AT_LIMIT`, and it now also requires zero
`IDENTITY_UNESTABLISHED` rows. So while a row is in this state, its unit is
**permanently blocked** from being promoted to `Matching` in `objects.json`.

Today that is `default/FilePath` and
`default/system/synth_xbox/FxSendMeterEffect`.

This is intended — a unit containing a function we cannot even attribute to a
target body is not finished — but it **outlives the incident**, so it is
written down here rather than left to be rediscovered as a mystery when someone
asks why `FilePath` will not promote. The unblock is the same as the exit:
establish the identity, land the map entry, clear the state.

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
- **`scripts/recon.py` reported no DB info for any symbol** — and it took two
  passes to actually fix, which is the instructive part. Its query selected
  `exclusion_reason` (nonexistent); `_load_db_info` catches `Exception` and
  returns `None`, so the failure was silent and every `db`-keyed branch in
  `_assess` was unreachable. Removing that column was **not sufficient**: the
  same SELECT also asked for `ease_score`, `impact_score` and
  `confidence_score`, which exist in neither a fresh v19 build nor the live DB,
  so it kept raising and the fix was believed-done while the tool stayed blind.
  Both are removed now, and the blanket `except` carries a warning. Verified by
  execution, not inspection — `recon` is the tool an agent runs to decide
  whether to work a function, so a silent blind spot there is the worst place
  for one.
- **`get_stats.avg_percent` counts dead and excluded rows.** 518 `excluded=1`
  and 2,370 `live=0` rows carry percentages; the shipped average is 67.74 vs
  72.10 with them removed. Only the `IDENTITY_UNESTABLISHED` exclusion was
  added here — the `excluded`/`live` pollution is pre-existing and out of scope.
- **`get_progress.py`'s "Excluded (SDK)" denominator is `unit LIKE '%xdk%'`,
  which matches zero rows** in the current DB, and it ignores the `excluded`
  column entirely, so all 520 excluded rows sit in its denominator.
