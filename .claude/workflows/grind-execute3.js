export const meta = {
  name: 'grind-execute3',
  description: 'Batch-3 recon-and-decide on remaining mixed near-miss units (EventTrigger, CalibrationPanel[game], DancerSequence[game], Gen, MeshAnim). Each agent diagnoses, fixes if a clean struct_offset/layout lever exists, defers with root cause otherwise. Never commits to main.',
  phases: [ { title: 'Execute', detail: 'one agent per unit: recon -> fix-or-defer -> A/B -> patch' } ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
const DOC = `${REPO}/docs/decomp/near-miss-classification-2026-06-06.md`
// ── lane BX-4 (2026-07-30): a hardcoded BASELINE is DEAD DATA ────────────────
// Every workflow in this dir carried a literal baseline (4,661–6,568) frozen at
// the date it was written, while main had moved to 41,170. An agent computing
// `net_delta = after - BASELINE` from that literal would report a fabricated
// +34,000. Baselines are MEASURED, never remembered — same rule as
// tools/dead_index_guard.py. Read measures.matched_functions from
// build/45410914/report.json in the leg you are actually measuring.
const BASELINE = 'MEASURE IT YOURSELF in your own worktree BEFORE your first edit: rm -f build/45410914/report.cache build/45410914/report.json, full build, then read measures.matched_functions. Do NOT use any number written in this prompt, and do NOT read the MAIN repo report.json — lanes land by patch without rebuilding main, so main artifact goes stale by hundreds of functions (measured 40,925 while main was 41,168). Cross-check against the headline in docs/plans/decomp-state-2026-07-19.md'

const UNITS = [
  { key: 'eventtrigger', area: 'engine', hint: 'classified mixed est+5: (1) Handle/PropSync guard stubs allocate stwu -0x70 vs target -0x60 (+16 frame); (2) EventTrigger::Anim _M_create_node li 0x24 vs 0x3c node-size + MemOrPoolAlloc vs MemOrPoolAllocSTL. The frame-size half may be a struct/by-value-temp size; the node half is STLport-coupled (likely not fixable without the allocator lever). Find any CLEAN local layout/struct fix.' },
  { key: 'calibrationpanel', area: 'game', hint: 'unclassified, 13 near-miss incl an 8-fn cluster at 92.5% (fn_825EED0C..EC strided ~0x20) + UpdateAnimation 93.1%. Determine if struct_offset (uniform member delta), funclet, or body. Fix if clean layout error.' },
  { key: 'dancersequence', area: 'game', hint: 'unclassified, 8 near-miss, 3 matched. Determine root cause; fix if clean layout/struct.' },
  { key: 'gen', area: 'engine', hint: 'classified mixed est+3 (some body-port). Find any clean struct_offset/layout component.' },
  { key: 'meshanim', area: 'engine', hint: 'classified mixed/body-port est+2. Check for a clean layout lever (RndMeshAnim/keys vector element size?); else defer.' },
]

phase('Execute')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' }, applied: { type: 'boolean' }, net_delta: { type: 'integer' },
    verdict: { type: 'string', enum: ['landable','deferred','no-op'] },
    improvements: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    patch_path: { type: ['string','null'] }, files_changed: { type: 'array', items: { type: 'string' } },
    root_cause: { type: 'string' },
  },
  required: ['key','applied','net_delta','verdict','root_cause'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Recon-and-decide ONE near-miss unit in rb3-xenon (matching MSVC-X360 PPC). Find whether a CLEAN struct_offset/layout fix flips a cluster of its near-miss functions to 100% WITHOUT regressing others. If yes, apply + A/B + patch. If the residual is funclet-noise / permuter-class / body-port / STLport-allocator-coupled, DEFER with a pinned root cause (do not force a regressing change).

UNIT "${U.key}" (${U.area}). Hint: ${U.hint}

DIRECTION RULE (batch-1/2 learning): derive grow-vs-shrink from objdiff "target | ours" — if ours reads a HIGHER offset, struct is too BIG (shrink); LOWER = too small (grow). Don't trust hints' direction. Oracles: rb3-Wii ../rb3/src (game/shared RB3), DC3 ../dc3-decomp/src (engine). Handoff doc: ${DOC}.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/g4-${U.key} g4-${U.key} ; work inside it.
2. RECON: find this unit's 90-99.99% functions in report.json; diff_inspect 3-4 (python3 scripts/analysis/diff_inspect.py --symbol "<fn>" --compare-asm --project-dir .). Classify the dominant residual (uniform offset delta=struct_offset; subi r31,r12 funclet=noise/drop; reg swaps=permuter; different logic=body; bl ...AllocSTL=allocator-coupled).
3. If a clean struct_offset/layout fix exists: apply minimal header edit, ./tools/ninja-locked, measure whole-binary vs ${BASELINE} (compare worktree report.json to ${REPO}/build/45410914/report.json), iterate to net>0 zero-regression.
4. If no clean fix: verdict=deferred, root_cause = precise diagnosis.
5. git diff > ~/tmp/grind4/${U.key}.patch (mkdir -p first; empty if deferred).
6. Return schema. verdict=landable only if net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree.`,
    { label: `g4:${U.key}`, phase: 'Execute', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
