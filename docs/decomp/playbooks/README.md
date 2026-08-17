# Decomp playbooks — the subagent formulas

Battle-tested, self-contained recipes a fresh agent can execute without access to the
coordinator's memory. Each playbook = target selection + wall-recognition checklist +
the work loop + measurement honesty rules + landing protocol. The executable
counterparts live in `.claude/workflows/*.js`; playbooks hold the rationale and the
judgment calls a workflow prompt can't.

> **STATUS (2026-08-17): the FORMULAS are current; the ROUND COUNTERS are
> historical.** The parenthetical yields below — *"+28 over rounds 1–4"*,
> *"+31 over rounds 1–2"*, and the **DC..DG** wave framing — are from the
> **2026-08-02/03 era** and were measured on the **`@none` ruler** (pre-flip; see
> [`../RULER_CHANGE_name_check_2026-08-12.md`](../RULER_CHANGE_name_check_2026-08-12.md)).
> Many more rounds have run since. **Regenerate the pool from a fresh
> `report.json` and re-measure the yield — do not treat these counters as the
> current state of any pool.**
>
> Live tree for scale: **44,444 matched / 69,227 functions**, `matched_code`
> **36.080082% @name_check**. Start-here state doc:
> [`../CAMPAIGN_STATE_2026-08-17.md`](../CAMPAIGN_STATE_2026-08-17.md).

| Playbook | Pool | When to run |
|---|---|---|
| [`bodyport-wave.md`](bodyport-wave.md) | Named fns 40–95%, logic-divergent | Standing campaign; refills the reveal cascade + inline-policy pool |
| [`hasreal-grind.md`](hasreal-grind.md) | Near-miss [90,100) HAS_REAL (~330 fns): real struct-offset/codegen bugs | After body-port waves; per-fn grind |
| [`nearmiss-harvest.md`](nearmiss-harvest.md) | Named real-bodied 96–99.99%: evaluation-order sculpting, local-.cpp-only Fable lanes | Recurring wave; regen pool from fresh report.json each round (+28 over rounds 1–4) |
| [`offset-drift-sweep.md`](offset-drift-sweep.md) | Whole 85–99.99% band with shared-header/TU-static **layout** drift (the header-edit complement of nearmiss-harvest) | Recurring; mechanical sweep + recon-before-edit; one header fix closes many fns (+31 over rounds 1–2) |
| [`levers-that-pay.md`](levers-that-pay.md) | The **eight matching levers of waves DC..DG**, each with precondition / trap / worked example and a LIVE-DRAINED-REFUTED state; plus the reachable-ceiling targeting partition | Read FIRST when picking a lane's target — it says what to try AND what is already drained. Pairs with [`../INSTRUMENT_DESIGN.md`](../INSTRUMENT_DESIGN.md) (how to build a control that can fail) |

Shared invariants (every playbook assumes these):
- Work in a worktree from `scripts/setup_worktree.sh`; never mutate the main repo's
  index/working tree; agents never commit to main — the orchestrator lands.
- Whole-binary A/B from a fresh full report; land only net>0 with zero unexplained
  regressions.
- Always `tee` builds to `~/tmp/` (e.g. `~/tmp/rb3_build_<task>.log`) — **never** `/tmp`
  (RAM-backed tmpfs, no btrfs reflink; has broken builds before). Classify against
  `docs/decomp/patterns/` before inventing a source-level cause.
- Verdicts (positive AND negative) go to
  [`docs/decomp/CAMPAIGN_STATE_2026-08-17.md`](../CAMPAIGN_STATE_2026-08-17.md)
  — the current verdict sink — so refuted levers are never re-attempted.
  *(Was `docs/plans/decomp-state-and-roadmap-*.md`; that file is dated 2026-06-09
  and is now a historical record, not the sink. Its drained/refuted ledger is
  still worth reading before opening a lever.)*
