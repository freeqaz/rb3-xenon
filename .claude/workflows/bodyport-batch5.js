export const meta = {
  name: 'bodyport-batch5',
  description: 'Body-port round 5: engine units with tractable body-ports (Spotlight, CharClip, MidiReader, CharBones, Draw). Engine fixes can unblock game (shared base classes). Recon body-vs-wall, port from oracle, A/B, patch. Never commits to main.',
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
const BASELINE = 'PREFERRED (2026-08-01): python3 tools/ab_measure.py --worktree <your worktree> --from-dirty runs the ENTIRE A/B protocol (settle-to-zero, report cache wipes, strict keys, refusal on broken runs) and cannot quote an unmeasured absolute — use it instead of the manual steps below. Manual fallback: MEASURE IT YOURSELF in your own worktree BEFORE your first edit: rm -f build/45410914/report.cache build/45410914/report.json, full build, then read measures.matched_functions. Do NOT use any number written in this prompt, and do NOT read the MAIN repo report.json — lanes land by patch without rebuilding main, so main artifact goes stale by hundreds of functions (measured 40,925 while main was 41,168). Cross-check against the headline in docs/plans/decomp-state-2026-07-19.md'

const UNITS = [
  { key: 'Spotlight', oracle: 'DC3 ../dc3-decomp/src (cross-check rb3-Wii)' },
  { key: 'CharClip', oracle: 'DC3 ../dc3-decomp/src' },
  { key: 'MidiReader', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii' },
  { key: 'CharBones', oracle: 'DC3 ../dc3-decomp/src' },
  { key: 'Draw', oracle: 'DC3 ../dc3-decomp/src (RndDrawable/RndTransformable base — shared, ripples to game drawables)' },
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
  agent(`Body-port genuine logic-divergent near-miss functions in rb3-xenon engine unit "${U.key}" to 100% (MSVC-X360 PPC match). Oracle: ${U.oracle}.

body-port = LOGIC diverges from retail (DC3 newer-engine added/changed a field/member/API/save-gate/value RB3 lacks). Fix = match retail's algorithm via oracle. DEFER (note, don't force): regalloc/scheduling reg-swaps, funclet noise (subi r31,r12), MSVC vtable-folding/devirtualization, /Ob2 inline-vs-outline, per-TU ODR. Per-TU MILO_ASSERT/MILO_NOTIFY arg-eval override is available if a side-effect arg is suppressed.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/bp5-${U.key} bp5-${U.key} ; work inside it.
2. Named (non-fn_) 50-95% fns in report.json; diff_inspect; classify; port body_diff ones from oracle (dc3-pair / rb3wii-pair skills).
3. Per fn: edit, ./tools/ninja-locked, confirm 100% + no regressions. Iterate.
4. Whole-binary net vs baseline ${BASELINE}.
5. cd worktree && mkdir -p ~/tmp/bp5 && git diff > ~/tmp/bp5/${U.key}.patch
6. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Be efficient; skip walls fast; honest net.`,
    { label: `bp5:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
