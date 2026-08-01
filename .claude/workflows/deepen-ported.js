export const meta = {
  name: 'deepen-ported',
  description: 'Deepen the just-ported RB3 game/bandobj TUs: match more remaining real-bodied near-misses via per-fn body-port from rb3-Wii. Defer regalloc/vtable/funclet walls. Never commits to main.',
  phases: [ { title: 'Deepen', detail: 'one agent per ported TU' } ],
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
  { key: 'BandCharacter', wii: 'system/bandobj/BandCharacter.cpp', have: '86/244' },
  { key: 'VocalTrackDir', wii: 'system/bandobj/VocalTrackDir.cpp', have: '29/101' },
  { key: 'TrackPanelDir', wii: 'system/bandobj/TrackPanelDir.cpp', have: '21/65' },
  { key: 'BandCharDesc', wii: 'system/bandobj/BandCharDesc.cpp', have: '17/39' },
  { key: 'BandWardrobe', wii: 'system/bandobj/BandWardrobe.cpp', have: '5/24' },
]

phase('Deepen')

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
  agent(`Deepen the already-ported RB3 TU "${U.key}" (currently ${U.have} matched) — match MORE of its remaining real-bodied near-miss functions. It is already sourced (src/${U.wii}) + compiling + partially matched. Oracle: rb3-Wii /home/free/code/milohax/rb3/src/${U.wii} (RB3 game/bandobj code; the body logic should match retail since this is RB3-specific code, unlike DC3).

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/dp-${U.key} dp-${U.key} ; work inside it.
2. List ${U.key}'s functions below 100% (named AND fn_) from report.json. For each real-bodied one (size>48), diff_inspect (python3 scripts/analysis/diff_inspect.py --symbol "<sym>" --compare-asm --project-dir .) and classify:
   - body_diff vs rb3-Wii (our port's body diverges from retail / has a porting bug) → FIX from the oracle. This is the main lever — the first port pass got the byte-exact + easy ones; the rest are real bodies that need the algorithm matched (loop forms, call sequences, rev-gates, constants).
   - For anonymous fn_ at 0% that our obj emits a real body for: it just needs a target_symbol_map entry → run reveal_sweep / safe_name_merge and merge into the map (extract+merge into CURRENT map; don't apply a stale hunk).
   - DEFER: regalloc/scheduling reg-swaps, funclet noise (subi r31,r12), MSVC vtable-folding/devirt, per-TU ODR, /Ob2 inline-vs-outline.
3. After edits: ./tools/ninja-locked (for map changes: rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml first). Confirm each fn hits 100% + no regressions.
4. Whole-binary net vs baseline ${BASELINE}.
5. cd worktree && mkdir -p ~/tmp/dp && git add -A && git diff HEAD > ~/tmp/dp/${U.key}.patch
6. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Be honest; skip walls fast; ${U.key=='BandCharacter'?'BandCharacter has 158 left — match as many clean ones as you can, do not chase the whole 158.':'aim for the clean body-ports + reveal-harvest.'}`,
    { label: `dp:${U.key}`, phase: 'Deepen', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
