export const meta = {
  name: 'bodyport-batch6',
  description: 'Body-port round 6: heavily-used util + remaining engine units (Utl, Str, GameMode, OvershellSlot, Shader). Restore DC3-relocated functions (LimitAng-style 0-byte stubs) + port logic-divergent near-misses. A/B, patch. Never commits to main.',
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
  { key: 'Utl', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii (utility; check for DC3-relocated funcs that belong in this TU)' },
  { key: 'Str', oracle: 'DC3 ../dc3-decomp/src + rb3-Wii (String/MakeString; heavily used)' },
  { key: 'GameMode', oracle: 'rb3-Wii ../rb3/src if band3/, else DC3' },
  { key: 'OvershellSlot', oracle: 'rb3-Wii ../rb3/src (game)' },
  { key: 'Shader', oracle: 'DC3 ../dc3-decomp/src' },
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
  agent(`Recover matches in rb3-xenon unit "${U.key}" (MSVC-X360 PPC). Oracle: ${U.oracle}. TWO veins to check:

(A) DC3-RELOCATED functions (LimitAng-style, CHEAP): a function whose canonical COMDAT lives in THIS unit's .text range in retail, but our .cpp doesn't define it (DC3, the newer engine, moved the source to another TU), so our obj emits a 0-byte stub and it reads 0%. Find these (named 0% fns whose body our obj doesn't emit) and RESTORE the definition to this .cpp from the oracle. /Ob2 will outline if it has a non-inlinable call.
(B) body-port near-misses (50-95% named): LOGIC divergence vs retail — fix from oracle. DEFER regalloc/funclet/vtable-folding/ODR/inlining walls.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/bp6-${U.key} bp6-${U.key} ; work inside it.
2. List this unit's named (non-fn_, has @) functions at 0% AND at 50-95% from report.json. For 0%: check if our obj emits a body (objdiff: if target side has instrs but ours is empty = DC3-relocated/unported → restore from oracle). For 50-95%: diff_inspect + classify.
3. Restore/port from oracle; ./tools/ninja-locked; confirm 100% + no regressions. Iterate.
4. Whole-binary net vs baseline ${BASELINE}.
5. cd worktree && mkdir -p ~/tmp/bp6 && git diff > ~/tmp/bp6/${U.key}.patch
6. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Honesty over optimism; skip walls fast.`,
    { label: `bp6:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
