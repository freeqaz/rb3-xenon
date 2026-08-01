export const meta = {
  name: 'bodyport-batch',
  description: 'Body-port the genuine logic-divergent near-miss functions (50-95%, named) in game + tractable units. Each agent recons body-vs-regalloc, ports logic ones from the oracle to 100%, A/B, returns patch. Defers regalloc/FP-scheduling (permuter-stuck). Never commits to main.',
  phases: [ { title: 'Port', detail: 'one agent per unit: recon -> port logic fns -> A/B -> patch' } ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
// ── lane BX-4 (2026-07-30): a hardcoded BASELINE is DEAD DATA ────────────────
// Every workflow in this dir carried a literal baseline (4,661–6,568) frozen at
// the date it was written, while main had moved to 41,170. An agent computing
// `net_delta = after - BASELINE` from that literal would report a fabricated
// +34,000. Baselines are MEASURED, never remembered — same rule as
// tools/dead_index_guard.py. Read measures.matched_functions from
// build/45410914/report.json in the leg you are actually measuring.
const BASELINE = 'PREFERRED (2026-08-01): python3 tools/ab_measure.py --worktree <your worktree> --from-dirty runs the ENTIRE A/B protocol (settle-to-zero, report cache wipes, strict keys, refusal on broken runs) and cannot quote an unmeasured absolute — use it instead of the manual steps below. Manual fallback: MEASURE IT YOURSELF in your own worktree BEFORE your first edit: rm -f build/45410914/report.cache build/45410914/report.json, full build, then read measures.matched_functions. Do NOT use any number written in this prompt, and do NOT read the MAIN repo report.json — lanes land by patch without rebuilding main, so main artifact goes stale by hundreds of functions (measured 40,925 while main was 41,168). Cross-check against the headline in docs/plans/decomp-state-2026-07-19.md'

const UNITS = [
  { key: 'TourDescPanel', area: 'game', oracle: 'rb3-Wii ../rb3/src' },
  { key: 'TrainerGemTab', area: 'game', oracle: 'rb3-Wii ../rb3/src' },
  { key: 'DateTime',      area: 'util', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii' },
  { key: 'Crowd',         area: 'engine', oracle: 'DC3 ../dc3-decomp/src (cross-check rb3-Wii)' },
  { key: 'CameraShot',    area: 'engine', oracle: 'DC3 ../dc3-decomp/src' },
]

phase('Port')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' },
    ported: { type: 'array', items: { type: 'object', additionalProperties: false,
      properties: { fn: { type: 'string' }, before: { type: 'number' }, after: { type: 'number' } }, required: ['fn','after'] } },
    net_delta: { type: 'integer' },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    landable: { type: 'boolean' },
    patch_path: { type: ['string','null'] },
    files_changed: { type: 'array', items: { type: 'string' } },
    notes: { type: 'string' },
  },
  required: ['key','ported','net_delta','landable','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Body-port the genuine logic-divergent near-miss functions in rb3-xenon unit "${U.key}" (${U.area}) to 100% (matching MSVC-X360 PPC machine code). Oracle: ${U.oracle}.

A "body-port" = the function's LOGIC diverges from retail (different/missing instructions, wrong call sequence, wrong constants) because our source (often DC3, a newer engine) differs from what RB3 retail compiled. Fix = rewrite the C++ body to match retail's algorithm, using the oracle. This is DIFFERENT from:
- regalloc/scheduling residuals (register swaps, instruction reorder, FP fsel/fmadd ordering) -> permuter-class, NOT body-portable by hand, DEFER.
- funclet noise (subi r31,r12 frame-reconstruct) -> DROP.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/bp-${U.key} bp-${U.key} ; work inside it.
2. Find this unit's named (non-fn_) functions at 50-95%: grep report.json. For each, run diff_inspect (python3 scripts/analysis/diff_inspect.py --symbol "<sym>" --compare-asm --project-dir .) and CLASSIFY:
   - body_diff (different/missing real instructions, wrong callee, wrong constant) -> PORTABLE. Pull the oracle source (use the dc3-pair / rb3wii-pair skills or grep ${U.oracle}) and rewrite our body to match retail's algorithm.
   - regalloc/scheduling/funclet -> SKIP (note it).
3. For each portable fn: edit the .cpp, ./tools/ninja-locked, confirm the fn reaches 100% (or improves materially) via report.json, and that nothing else regressed. Iterate.
4. MEASURE whole-binary net vs baseline ${BASELINE} (worktree report.json vs ${REPO}/build/45410914/report.json).
5. cd worktree && mkdir -p ~/tmp/bp && git diff > ~/tmp/bp/${U.key}.patch
6. Return schema (ported = fns you flipped/improved with before/after). landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree.

Be efficient: prioritize the functions most likely to be clean logic-ports (clear oracle, modest size). If a function is regalloc-stuck after a reasonable attempt, move on. Honesty over optimism in the measured net.`,
    { label: `bp:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
