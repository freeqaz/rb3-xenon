# Decomp playbooks — the subagent formulas

Battle-tested, self-contained recipes a fresh agent can execute without access to the
coordinator's memory. Each playbook = target selection + wall-recognition checklist +
the work loop + measurement honesty rules + landing protocol. The executable
counterparts live in `.claude/workflows/*.js`; playbooks hold the rationale and the
judgment calls a workflow prompt can't.

| Playbook | Pool | When to run |
|---|---|---|
| [`bodyport-wave.md`](bodyport-wave.md) | Named fns 40–95%, logic-divergent | Standing campaign; refills the reveal cascade + inline-policy pool |
| [`hasreal-grind.md`](hasreal-grind.md) | Near-miss [90,100) HAS_REAL (~330 fns): real struct-offset/codegen bugs | After body-port waves; per-fn grind |

Shared invariants (every playbook assumes these):
- Work in a worktree from `scripts/setup_worktree.sh`; never mutate the main repo's
  index/working tree; agents never commit to main — the orchestrator lands.
- Whole-binary A/B from a fresh full report; land only net>0 with zero unexplained
  regressions.
- Always `tee` builds to `/tmp/`. Classify against `docs/decomp/patterns/` before
  inventing a source-level cause.
- Verdicts (positive AND negative) go to `docs/plans/decomp-state-and-roadmap-*.md`
  so refuted levers are never re-attempted.
