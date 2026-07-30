export const meta = {
  name: 'bodyport-batch3',
  description: 'Body-port round 3: wide-ripple engine types (Group, Bitmap) + fresh units (PartAnim, Joypad_Xbox, SpotlightDrawer_NG). Port logic-divergent fns to 100% to also refill the reveal cascade. Defer walls. Never commits to main.',
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
  { key: 'Group', area: 'engine', oracle: 'DC3 ../dc3-decomp/src (RndGroup; widely used — fixes may ripple reveals)' },
  { key: 'Bitmap', area: 'engine', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii (RndBitmap; widely used)' },
  { key: 'PartAnim', area: 'engine', oracle: 'DC3 ../dc3-decomp/src' },
  { key: 'Joypad_Xbox', area: 'engine', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii' },
  { key: 'SpotlightDrawer_NG', area: 'engine', oracle: 'DC3 ../dc3-decomp/src' },
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
  agent(`Body-port genuine logic-divergent near-miss functions in rb3-xenon unit "${U.key}" (${U.area}) to 100% (MSVC-X360 PPC match). Oracle: ${U.oracle}.

body-port = LOGIC diverges from retail (DC3 newer-engine added a field/member/API-arg/save-gate RB3 lacks). Fix = match retail's algorithm via oracle (gate behind HX_NATIVE or remove; cross-check rb3-Wii). DEFER (note, don't force): regalloc/scheduling, funclet noise (subi r31,r12), MSVC vtable-folding/devirtualization, static-init guard-bit-packing, per-TU ODR. MILO_ASSERT now evaluates its cond (landed).

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/bp3-${U.key} bp3-${U.key} ; work inside it.
2. Named (non-fn_) 50-95% fns in report.json; diff_inspect; classify; port body_diff ones from oracle.
3. Per fn: edit, ./tools/ninja-locked, confirm 100% + no regressions. Iterate.
4. Whole-binary net vs baseline ${BASELINE} (worktree report.json vs ${REPO}/build/45410914/report.json).
5. cd worktree && mkdir -p ~/tmp/bp3 && git diff > ~/tmp/bp3/${U.key}.patch
6. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Be efficient; skip walls fast; honesty over optimism.`,
    { label: `bp3:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
