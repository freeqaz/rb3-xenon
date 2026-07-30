export const meta = {
  name: 'bodyport-batch2',
  description: 'Body-port round 2: fresh tractable units (MemTracker, SpotlightDrawer, LightPreset residual, Gem, DateTime-retry). Recon body-vs-regalloc, port logic-divergent fns from oracle to 100%, A/B, patch. Defer walls. Never commits to main.',
  phases: [ { title: 'Port', detail: 'one agent per unit' } ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
// ── lane BX-4 (2026-07-30): a hardcoded BASELINE is DEAD DATA ────────────────
// Every workflow in this dir carried a literal baseline (4,661–6,568) frozen at
// the date it was written, while main had moved to 41,170. An agent computing
// `net_delta = after - BASELINE` from that literal would report a fabricated
// +34,000. Baselines are MEASURED, never remembered — same rule as
// tools/dead_index_guard.py. Read measures.matched_functions from
// build/45410914/report.json in the leg you are actually measuring.
const BASELINE = 'MEASURE IT YOURSELF in your own worktree BEFORE your first edit: rm -f build/45410914/report.cache build/45410914/report.json, full build, then read measures.matched_functions. Do NOT use any number written in this prompt, and do NOT read the MAIN repo report.json — lanes land by patch without rebuilding main, so main artifact goes stale by hundreds of functions (measured 40,925 while main was 41,168). Cross-check against the headline in docs/plans/decomp-state-2026-07-19.md'

const UNITS = [
  { key: 'MemTracker', area: 'engine', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii ../rb3/src' },
  { key: 'SpotlightDrawer', area: 'engine', oracle: 'DC3 ../dc3-decomp/src' },
  { key: 'LightPreset', area: 'engine', oracle: 'rb3-Wii ../rb3/src (residual after the std::vector struct fix already landed)' },
  { key: 'Gem', area: 'game', oracle: 'rb3-Wii ../rb3/src' },
  { key: 'DateTime', area: 'util', oracle: 'rb3-Wii ../rb3/src — RETRY: the 4-arg LocalizeOrdinal/2-arg Localize body-port reached Format 99.87%; MILO_ASSERT now evaluates conds (landed). Re-attempt; if still blocked only by the MakeString symbol-map label ambiguity, defer.' },
]

phase('Port')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' },
    ported: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { fn: { type: 'string' }, before: { type: 'number' }, after: { type: 'number' } }, required: ['fn','after'] } },
    net_delta: { type: 'integer' },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    landable: { type: 'boolean' }, patch_path: { type: ['string','null'] }, files_changed: { type: 'array', items: { type: 'string' } }, notes: { type: 'string' },
  },
  required: ['key','ported','net_delta','landable','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Body-port the genuine logic-divergent near-miss functions in rb3-xenon unit "${U.key}" (${U.area}) to 100% (MSVC-X360 PPC match). Oracle: ${U.oracle}.

A body-port = LOGIC diverges from retail (different/missing instructions, wrong callee, wrong constant) because our source (often DC3, newer engine) differs from RB3 retail. Fix = rewrite the body to match retail's algorithm using the oracle. The common shape is DC3 added a field/member/API-arg/save-gate that RB3 retail lacks (gate behind HX_NATIVE or remove; cross-check rb3-Wii). NOT body-portable (DEFER, note it): regalloc/scheduling (reg swaps, instr reorder, FP fsel/fmadd ordering), funclet noise (subi r31,r12), MSVC vtable-folding/devirtualization, static-init guard-bit-packing.

NOTE: MILO_ASSERT now EVALUATES its cond (landed) — asserts with side-effect calls (NumData() etc.) now keep them, so re-check any fn that was blocked by that.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/bp2-${U.key} bp2-${U.key} ; work inside it.
2. Find this unit's named (non-fn_) 50-95% functions in report.json. diff_inspect each; classify; for body_diff ones pull the oracle (dc3-pair / rb3wii-pair skills or grep ${U.oracle}) and rewrite to match.
3. Per portable fn: edit .cpp/.h, ./tools/ninja-locked, confirm 100% in report.json, no regressions. Iterate.
4. Whole-binary net vs baseline ${BASELINE} (worktree report.json vs ${REPO}/build/45410914/report.json).
5. cd worktree && mkdir -p ~/tmp/bp2 && git diff > ~/tmp/bp2/${U.key}.patch
6. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Be efficient — skip regalloc-stuck fns quickly; honesty over optimism on net.`,
    { label: `bp2:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
