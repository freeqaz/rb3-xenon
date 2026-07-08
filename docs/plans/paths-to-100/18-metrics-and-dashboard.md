# Metrics of record and progress dashboard — vein ROI accounting

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: infra

## Summary

We have excellent *state* metrics (strict `matched_functions`, `fuzzy_progress.py`
WIRED% + staircase, `icf_alias_check.py` honesty gate) but no *ROI accounting*:
no machine-readable ledger of which lever produced which +N at what cost, no
time-series across commits, no per-unit drill-down. This RFC proposes the
**minimum** that changes decisions: an append-only `metrics.jsonl` written at each
landing, a vein-tagged git trailer convention, a ~150-line static dashboard
generator, and a per-unit band-histogram view driven from the existing
`report.json`. Explicit non-goal: enterprise dashboards. The one subtlety that
*must* be surfaced is the **WIRED-denominator move** — pins grow the denominator,
so a raw fuzzy delta can mislead unless denominator changes are shown alongside.

## Motivation

The project's decision loop is "pick the next vein, spend agent-hours, measure net
strict delta, keep or kill the vein." Today the *keep/kill* half is recorded only
in prose — MEMORY.md topic files and commit bodies. Concretely:

- The record of "class-A TU-pure span harvest gave +403 one session, now
  EXHAUSTED (wave-8 +0)" lives in a memory file, not a queryable ledger. So does
  "member-delta lever CLOSED," "topo-locator killed at precision 0.13,"
  "grind loop +22 landed." Each is a *vein verdict with an ROI*, but there is no
  structured place they accumulate. A new agent (or the human) re-derives vein
  economics by grep-reading commit prose (`git log | grep -i lever` today returns
  freeform sentences, verified — no trailer convention exists).
- There is no time-series. To answer "what did the last 20 commits net, and which
  vein tags dominated?" you must diff `report.json` snapshots by hand.
  `scripts/harvest/measure_delta.py` computes a *single* A/B net between two
  `report.json` files but persists nothing.
- The WIRED denominator subtlety is a live footgun. `fuzzy_progress.py` reports
  WIRED% over "attempted bytes" (currently **1,392,316 B, n=13,584 fns**,
  verified). Micro-pinning a new TU *adds* its bytes to that denominator. A pin
  that lands a high-fuzzy TU can therefore *lower* the WIRED% even while adding
  strict matches (the CharClipGroup gate in `d696b52` is exactly "net-WIRED-
  positive since pinning grows the WIRED denominator"). A dashboard that shows the
  fuzzy % without showing the denominator move will actively mislead.

The value of fixing this is **not** more matches directly — it is *avoiding
wasted agent-spend on already-killed veins* and *routing spend toward the veins
with the best measured ROI*. In a project that has burned whole waves on dead
levers (topo-locator, member-delta, enrichment packet), an accurate vein ledger
pays for itself if it prevents even one re-attempt of a known-dead vein.

## Current state (verified)

Checked against main @a1312de, `build/45410914/report.json` (mtime Jul 6 17:33),
and the live tools on 2026-07-08:

**Metrics that exist and work:**

- **STRICT north star** — `report.json` `measures`:
  `matched_functions = 11240 / 65619 (17.129%)`,
  `matched_code = 962656 / 11074108 (8.693%)`. Read directly by `fuzzy_progress.py`.
- **`tools/fuzzy_progress.py`** — prints STRICT, FUZZY-CODE (whole 11.895% /
  **WIRED 94.602%** over 1,392,316 B / n=13,584), STAIRCASE
  (`>=100: 11240 | >=95: 12743 | >=90: 13021 | >=80: 13175 | >=50: 13407` — all
  match the brief exactly), a per-band HISTOGRAM, and SUB-GOALS split
  band3/network vs engine vs other. This is the richest single view we have.
- **`tools/icf_alias_check.py`** — honesty gate; `--tu` / `--range` / `--worktree`;
  exit 0 = HONEST, 1 = ICF-ALIAS INFLATION. Reads `report.json` +
  `build/45410914/icf_aliases.map`. Gates waves against fake stub-fold matches.
- **`scripts/harvest/measure_delta.py`** — A/B net between two `report.json`
  snapshots (strict net + fuzzy-regression scan). The land gate's measurement
  half. **Persists nothing** — pure stdout.
- **`tools/fresh_report.sh`** — forces a guaranteed-fresh full `report.json`
  (builds all objs, deletes + regenerates the report, verifies mtimes).
- **`scripts/ingest_report.py` + `scripts/orchestrator/database.py`** — ingest
  `report.json` into `decomp.db` (SQLite). Schema has a `functions` table with
  per-function match% and **unicorn columns** (`unicorn_verdict`, `unicorn_class`,
  `unicorn_confidence`, `unicorn_reason`, added in migrations v8/v9), plus
  `attempts`, `worktrees`, `patch_queue` tables. **Note:** the *tracked*
  `scripts/orchestrator/decomp.db` is a **0-byte placeholder** (verified) — the
  live DB is regenerated locally by `ingest_report.py`, so any dashboard must
  regenerate it, not assume it is populated.

**What does NOT exist (the gaps this RFC targets), verified:**

- **No `metrics.jsonl` or any time-series ledger.** `find` for
  `*metric*`/`*dashboard*`/`*timeseries*` returns nothing. The only `.jsonl`
  ledgers in the repo are `docs/decomp/matng-abandoned.jsonl` (3 lines,
  per-function deferral notes) and `tools/testdata/mdgrind_gt48.jsonl` (test
  fixture) — precedent that jsonl ledgers are an accepted format here, but
  neither is an ROI/time-series ledger.
- **No vein-tag git trailer convention.** Commits describe levers in freeform
  prose (`git log | grep -i 'vein\|lever\|cost'` → English sentences, no
  `Vein:`/`Lever:`/`Cost:` trailers). The Co-Authored-By trailer is the only
  structured trailer in use.
- **No dashboard generator** (HTML or markdown) and **no per-unit drill-down page**.
  `report.json` has per-unit function lists (2,456 units) but nothing renders them
  as a band histogram per unit.
- **No queue-depth / agent-utilization view.** `decomp.db` has `worktrees` and
  `patch_queue` tables that *could* back this, but nothing surfaces them.

## Proposal

Four components, each independently shippable, ordered by ROI. **Recommendation:
build components 1 and 2 only; treat 3 and 4 as opt-in.** The whole thing is
tooling for a solo+agents project — it must stay under ~400 LOC total or it is not
worth the maintenance.

### Component 1 — `metrics.jsonl`, the vein ROI ledger (the core)

An append-only newline-delimited JSON file at `docs/decomp/metrics.jsonl` (sits
next to the existing `matng-abandoned.jsonl`, follows repo jsonl precedent).
**One record per landing** (a commit that lands a net change to main). Written by
a thin wrapper invoked at land time; never edited retroactively.

Schema (all fields verifiable at land time from `report.json` + git):

```json
{
  "ts": "2026-07-08T14:22:03Z",
  "commit": "a1312de",
  "vein": "grind-loop",             // controlled vocab, see below
  "strict_fns_before": 11218,
  "strict_fns_after": 11240,
  "strict_fns_delta": 22,
  "strict_code_before": 961_xxx,
  "strict_code_after": 962656,
  "wired_bytes_before": 1_390_xxx,  // WIRED denominator BEFORE
  "wired_bytes_after": 1_392_316,   // WIRED denominator AFTER (denominator move!)
  "wired_pct_before": 94.58,
  "wired_pct_after": 94.602,
  "regressions": 0,
  "icf_verdict": "HONEST",          // from icf_alias_check.py
  "cost_agent_runs": 5,             // best-of-N passes, or agent count; honest estimate
  "cost_note": "hy3 best-of-5 + close-out",
  "units": ["default/Setlist", "default/Campaign"]
}
```

The **denominator fields are mandatory** (`wired_bytes_before/after`) — this is the
one design decision that distinguishes an honest fuzzy ledger from a misleading
one. Every fuzzy delta in the ledger is interpretable only against its denominator
move. A pin that grows `wired_bytes` by 3 KB and moves `wired_pct` from 94.60 →
94.55 is *net-positive work* (it added a high-fuzzy TU), not a regression — the
ledger makes that legible instead of alarming.

**Controlled vein vocabulary** (seed set, derived from the proven levers in
CLAUDE.md / MEMORY.md, extensible):
`tu-pure-span` · `grind-loop` · `micro-pin` · `objptr-flip` · `local-static-symbol` ·
`member-delta` · `body-port` · `permuter` · `save-rev` · `guard-thunk` ·
`unicorn-equiv` (see `17-unicorn-equivalence-lane.md`) · `middleware`
(see `10-middleware-and-denominator.md`) · `other`.

**Writer:** a ~60-line `tools/log_landing.py` that takes a baseline report and the
current report (the same two files `measure_delta.py` already consumes), reads git
`HEAD`, and appends one record. Wire it as an **optional final step in
`scripts/harvest/land.sh`** after the composed verify passes — the land script
already computes the before/after deltas, so this reuses that data with near-zero
marginal cost. It must be *append-only and failure-tolerant*: a broken logger must
never block a land.

Backfill: seed the ledger by parsing the last ~50 commit bodies for their stated
`+N` and vein (best-effort, marked `"backfilled": true`) so the time-series has
history from day one. This is a one-shot script, not maintained.

### Component 2 — vein-tagged git trailer + reconciliation

Adopt a `Vein:` git trailer convention so a commit *is* the ledger's provenance:

```
match: +22 strict via grind close-out

Vein: grind-loop
Cost: 5 agent-runs, hy3 best-of-5 + permuter close-out
Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>
```

`tools/log_landing.py` reads the `Vein:`/`Cost:` trailers from the landing commit
(via `git interpret-trailers --parse`) instead of guessing. This keeps the ledger
authoritative-by-construction: the commit and the ledger can't disagree because the
ledger is *generated from* the commit + `report.json`. A tiny
`tools/vein_lint.py` (optional pre-push / CI check) warns if a commit that moved
`matched_functions` lacks a `Vein:` trailer.

### Component 3 (opt-in) — static dashboard generator

`tools/gen_dashboard.py` → writes `build/45410914/dashboard.md` (and optionally a
single self-contained `dashboard.html` with inline SVG sparklines, no JS
framework, no server). Sections:

1. **Headline** — current STRICT + WIRED (from `fuzzy_progress.py`'s own
   functions, imported, not re-implemented — single source of truth).
2. **Time-series** — cumulative `matched_functions` over commits from
   `metrics.jsonl`, as an ASCII/SVG sparkline; a second series for
   `wired_bytes` (the denominator) plotted *underneath* so denominator growth is
   visually coupled to fuzzy movement.
3. **Vein ROI table** — group `metrics.jsonl` by `vein`: total +N, total
   cost, N/cost ratio, last-landing date, and a `LIVE`/`EXHAUSTED`/`DEAD` status.
   This is the decision artifact — "which vein has the best remaining ROI."
4. **Per-unit drill-down** — top-N units by remaining fuzzy headroom
   (unmatched-but-attempted bytes), each with its 7-band histogram from
   `report.json`. Answers "where is the next micro-pin worth it."

Regeneration is manual (`python3 tools/gen_dashboard.py`) or optionally appended to
`fresh_report.sh`. It reads only `report.json` + `metrics.jsonl`; no new data
source. Target < 200 LOC.

### Component 4 (opt-in, lowest ROI) — queue/utilization view

Add two `decomp.db` **views** (not tables) computed from existing
`worktrees` + `patch_queue`: `v_queue_depth` (pending/applied/failed patch counts)
and `v_agent_activity` (worktrees by last-touched). Surface them as a small table
in the dashboard. This only earns its keep during multi-agent fleet runs
(see `12-grind-fleet-v2.md`, `16-auto-landing-pipeline.md`); for solo work it is
noise. **Recommend deferring until the fleet is cron-driven.**

### Data flow (components 1–3)

```
land.sh (composed verify passes)
   │  reads BASE.json + build/45410914/report.json  (already in hand)
   │  reads Vein:/Cost: trailers from HEAD
   ▼
tools/log_landing.py  ──append──►  docs/decomp/metrics.jsonl   (committed)
                                          │
tools/fuzzy_progress.py ─(imported)─┐     │
build/45410914/report.json ─────────┼─────┤
                                    ▼     ▼
                          tools/gen_dashboard.py
                                    │
                                    ▼
                 build/45410914/dashboard.md / .html  (gitignored, regenerable)
```

## Alternatives considered

- **Do nothing; keep prose in MEMORY.md.** This is the honest baseline and it has
  *worked* — the project reached 11,240 matches on prose bookkeeping. The argument
  for change is purely marginal: prose doesn't aggregate, so vein ROI is
  re-derived by grep. If agent-spend were cheap this would be fine. It isn't, and
  killed veins get silently re-attempted. Component 1+2 is the cheapest thing that
  stops that.
- **Full time-series DB (per-commit `report.json` snapshots in `decomp.db`).**
  Rejected: `report.json` is 10 MB; storing one per commit is wasteful, and the
  ingest is already lossy for our purpose (we want *deltas and veins*, not full
  per-fn state history). `metrics.jsonl` (one ~500-byte record per landing) is
  ~1000× smaller and directly answers the ROI question.
- **Grafana / Prometheus / any service.** Rejected outright per scope rule — this
  is a solo+agents repo with no running services; a scrape target and a dashboard
  server are pure overhead. Static files regenerated on demand are the right
  altitude.
- **Compute ROI purely from git + `report.json` at query time, no ledger.**
  Rejected: cost (`agent-runs`) is *not* recoverable from git after the fact, and
  the WIRED denominator at each historical commit requires rebuilding that commit.
  A ledger captures both at land time, when they're free.
- **Reuse `measure_delta.py` output directly as the ledger.** It's the right data
  but it's ephemeral stdout with no vein/cost/denominator fields. Component 1 is
  essentially "persist `measure_delta`'s numbers + 3 fields."

## Effort & expected value

**Effort (honest):**
- Component 1 (`log_landing.py` + schema + `land.sh` hook + backfill): ~4–6 hours,
  ~120 LOC. Low risk (append-only, failure-tolerant).
- Component 2 (trailer convention + `git interpret-trailers` wiring + `vein_lint`):
  ~2–3 hours, ~40 LOC + a docs paragraph.
- Component 3 (`gen_dashboard.py`): ~6–8 hours, ~200 LOC. Most of the cost is the
  per-unit histogram + sparkline rendering.
- Component 4: ~3 hours, ~60 LOC — but recommend deferring.

**Expected value (deliberately not inflated):** this vein produces **zero direct
matches**. Its EV is *decision quality*: preventing re-spend on dead veins and
routing spend to high-ROI ones. Anchoring to comparable past waste in this repo —
the enrichment-packet round (`50481427`) spent ~$0.29 + agent time to conclude
"statistical wash"; the topo-locator and member-delta veins each consumed a full
analysis wave before being killed. If a maintained vein ledger prevents *one*
re-attempt of a known-dead vein per month, it repays its ~10-hour build cost
quickly. But that saving is **diffuse and unmeasurable** — treat the EV as
"modest, indirect, front-loaded" and build the minimum (1+2) accordingly.

The single highest-value line item is the **WIRED-denominator column** in the
ledger: it removes a real, recurring misread that already required a hand-crafted
gate rule (CharClipGroup `d696b52`). That is a concrete correctness win, not just
convenience.

## Risks & failure modes

- **Metric theater.** A dashboard invites optimizing the number instead of the
  work. Mitigation: STRICT stays the only north star; the dashboard *displays*
  fuzzy/ROI but the ledger records regressions and denominator moves so nobody can
  quietly inflate. Do not add any headline metric that isn't already an accepted
  north star.
- **Ledger drift / staleness.** If `log_landing.py` is skippable, the ledger will
  have holes and become untrustworthy. Mitigation: `vein_lint` warns on
  match-moving commits without a `Vein:` trailer; the backfill script can re-derive
  gaps from commit prose (lossy). Accept that the ledger is best-effort, not
  audited — mark backfilled/estimated records explicitly.
- **Cost field is subjective.** `cost_agent_runs` is an estimate (best-of-N count,
  agent count) with no clean unit across veins. Mitigation: record it as a coarse
  integer + free-text `cost_note`; never present cost as precise. ROI ratios are
  order-of-magnitude signals, not accounting.
- **Denominator confusion persists if the dashboard is read carelessly.** The whole
  point fails if someone reads WIRED% without the denominator. Mitigation: never
  render `wired_pct` without `wired_bytes` adjacent; in the vein table, flag any
  landing where `wired_pct` fell but `strict_fns_delta > 0` as
  `denominator-growth (expected)`.
- **Maintenance rot.** Any of these scripts can bit-rot if `report.json`'s shape
  changes. Mitigation: import from `fuzzy_progress.py` (which already parses the
  report) rather than re-parsing; keep total LOC small.

## Kill criteria

- **Kill Component 3/4 if** after building Component 1+2 the ledger shows the team
  never actually consults the dashboard to pick a vein (check: no commit message
  or plan cites the dashboard within ~4 weeks). Then the jsonl ledger alone is the
  deliverable; delete the generator.
- **Kill the whole RFC if** the trailer + ledger discipline isn't kept for 2
  consecutive waves (holes in `metrics.jsonl`) — an unmaintained ledger is worse
  than none because it looks authoritative while being wrong. Revert to prose.
- **Kill Component 2 if** `git interpret-trailers` adds friction agents skip — fall
  back to `log_landing.py` inferring the vein from the changed units + a manual
  `--vein` flag.
- **Do NOT build this at all if** the immediate priority is direct match-count
  gains and infra can wait: this vein's EV is indirect. It should be scheduled in a
  lull, not ahead of a live match-producing vein (see
  `03-master-sequencing-roadmap.md`).

## Open questions

1. Should `metrics.jsonl` be committed (provenance, diffable history) or gitignored
   + regenerable? **Lean: commit it** — it's small, append-only, and its value is
   the durable cross-session record. The generated dashboard stays gitignored.
2. Is one record *per landing commit* the right granularity, or per *wave* (a wave
   lands several commits)? Per-commit is simpler and composes; a `wave` field can
   group them. Lean per-commit + optional `wave` tag.
3. Should the WIRED denominator be recomputed at every landing (requires a fresh
   report both sides) or only when a pin is added? Pins are the only thing that
   move it, so we *could* record denominator only on `micro-pin`/`tu-pure-span`
   veins — but recording it always is cheaper to reason about. Lean: always.
4. Does `17-unicorn-equivalence-lane.md` want a *secondary credit* series in the
   ledger (EQUIVALENT-but-not-byte-exact count over time)? If that lane ships, add
   a `unicorn_equiv_delta` field. Coordinate schema before either lands.
5. Backfill fidelity: commit prose states `+N` inconsistently (some say "+22",
   some "+15 gained 0 regressed"). Accept lossy backfill or hand-curate the last
   ~50? Lean: lossy + `"backfilled": true`.

## References

Verified live in repo (main @a1312de) unless marked:

- `tools/fuzzy_progress.py` — STRICT/FUZZY/STAIRCASE/HISTOGRAM/SUB-GOALS reporter;
  the single source of truth to *import*, not re-implement. WIRED 94.602% /
  1,392,316 B / n=13,584 confirmed.
- `tools/icf_alias_check.py` — ICF-alias honesty gate (`--tu`/`--range`/
  `--worktree`, exit 0/1). Reads `build/45410914/icf_aliases.map`.
- `scripts/harvest/measure_delta.py` — A/B strict-net + fuzzy-regression scan;
  the data source Component 1 persists.
- `scripts/harvest/land.sh` — wave land script; proposed hook point for
  `log_landing.py`.
- `tools/fresh_report.sh` — forces a fresh full `report.json`; optional dashboard
  regen hook.
- `scripts/ingest_report.py`, `scripts/orchestrator/database.py` — `report.json` →
  `decomp.db`; schema has `functions` (with unicorn cols), `attempts`,
  `worktrees`, `patch_queue`. Tracked `decomp.db` is a 0-byte placeholder;
  regenerate locally.
- `build/45410914/report.json` — authoritative `measures`
  (`matched_functions 11240/65619`, `matched_code 962656/11074108`); 2,456 units,
  per-unit function lists. Top-level `fuzzy_match_percent` is `None` — the fuzzy
  figures come from per-function `fuzzy_match_percent`, computed by
  `fuzzy_progress.py`.
- `docs/decomp/matng-abandoned.jsonl` — existing per-function deferral jsonl
  ledger; format precedent for `metrics.jsonl`.
- `docs/INDEX.md` — audited docs master index (read for stale-doc banners).
- MEMORY.md topic files (`~/.claude/projects/-home-free-code-milohax-rb3-xenon/
  memory/`) — current prose home of the vein verdicts this RFC would structure.
- Sibling RFCs: `17-unicorn-equivalence-lane.md` (secondary-credit series to
  coordinate on), `16-auto-landing-pipeline.md` + `12-grind-fleet-v2.md`
  (queue/utilization consumers, Component 4), `10-middleware-and-denominator.md`
  (denominator vein tag), `03-master-sequencing-roadmap.md` (where this infra vein
  sequences vs match-producing veins).
- `CLAUDE.md` — proven-levers list that seeds the controlled vein vocabulary.
