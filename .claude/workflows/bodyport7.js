export const meta = {
  name: 'bodyport7',
  description: 'Body-port round 7: logic-divergent named near-misses in UI + util units, possibly freshly unblocked by the Find<T>/MILO_FAIL change. Port from oracle to 100%, A/B, patch. Defer regalloc/funclet/stub walls. Never commits to main.',
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
  { key: 'UIScreen', oracle: 'rb3-Wii ../rb3/src/system/ui (Exit/OnMsg flagged regswap+insert — check for a portable logic component now that Find<T>/MILO_FAIL are fixed)' },
  { key: 'UISlider', oracle: 'rb3-Wii ../rb3/src/system/ui' },
  { key: 'UIEvent', oracle: 'rb3-Wii ../rb3/src/system/ui (UIEvent/UIEventMgr ~92.5%)' },
  { key: 'Utl', oracle: 'DC3 ../dc3-decomp/src/system/rndobj/Utl.cpp + rb3-Wii (11 named 40-92% residuals)' },
  { key: 'MemTracker', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii' },
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
  agent(`Port logic-divergent named near-miss functions in rb3-xenon unit "${U.key}" to 100% (MSVC-X360 PPC match). Oracle: ${U.oracle}. Note: the binary-wide Find<T>/MILO_FAIL fix (2-arg FindObject + MILO_FAIL=(void)(args)) just landed — some functions that were blocked by Find<>/fail-path divergence may now be cleanly portable; re-check.

body-port = LOGIC diverges (different/missing instructions, wrong callee/constant) — fix from oracle. DEFER (note, don't force): pure regalloc/scheduling reg-swaps, funclet noise (subi r31,r12), MSVC vtable-folding/devirt, ICF-clone thunks, retail-stubbed accessors (oracle mirage = our body matches oracle but target is a stripped stub). Per-TU MILO_ASSERT/MILO_NOTIFY (void)(args) override available if a side-effect arg is suppressed.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/b7-${U.key} b7-${U.key} ; work inside it.
2. Named (non-fn_) 40-95% fns in report.json; diff_inspect each; classify; port the body_diff ones from oracle.
3. Per fn: edit, ./tools/ninja-locked, confirm 100% + no regressions. Iterate. reveal_sweep+gate+merge for byte-exact (rm build/45410914/target_symbol_renames.stamp + touch config/45410914/config.yml before rebuild).
4. Whole-binary net vs baseline ${BASELINE}.
5. cd worktree && git add -A && git diff HEAD > ~/tmp/b7/${U.key}.patch
6. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Honest net; skip walls fast — if everything is a wall (regalloc/stub/funclet), report net 0 + that the unit is at-limit (valid result).`,
    { label: `b7:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
