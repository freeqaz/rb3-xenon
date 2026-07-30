// ============================================================================
// STATUS 2026-07-30: DEAD AS WRITTEN -- DO NOT RUN WITHOUT REWORK.
// This workflow drives 'decomp_synth' (scan_and_permute), which lives in a
// separate private repo and is NOT present here, so the commands below will
// fail. Separately, the source-permuter is OFF by standing user directive:
// do not route to /permute until the user re-opens it.
// Kept under version control deliberately: an agent-facing prompt that is
// wrong silently steers every future lane, and while these were untracked
// nothing prevented that. Fix or delete in review -- do not shadow-edit.
// ============================================================================
export const meta = {
  name: 'permuter-sweep-fresh',
  description: 'Run the m2c/BSF-enabled decomp_synth permuter on CURRENT real-logic CODEGEN_WORK near-misses (>=97%, non-STL/non-ICF). Apply only TRUE-100% wins. One agent per unit in an isolated worktree; never commit to main.',
  phases: [
    { title: 'Permute', detail: 'per-unit permuter runs on real-logic near-miss fns; apply true-100% only' },
  ],
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

// Fresh real-logic permuter targets (true_progress.py CODEGEN_WORK >=97%, STL/ICF filtered).
// m2c + BSF backends are now installed (commit 9763683), so guidance is live.
const UNITS = [
  { unit: 'Utl',         targets: ['?LinearizeKeys@@YAXPAVRndTransAnim@@MMMMM@Z'] },
  { unit: 'Gem',         targets: ['?AddRep@Gem@@QAAXAAVGemRepTemplate@@PAVRndGroup@@VSymbol@@ABVTrackConfig@@_N@Z'] },
  { unit: 'Locale',      targets: ['?LocalizeSeparatedInt@@YAPBDHAAVLocale@@@Z'] },
  { unit: 'LightPreset', targets: ['?FillSpotPresetData@LightPreset@@IAAXPAVSpotlight@@AAUSpotlightEntry@1@H@Z'] },
  { unit: 'GameMode',    targets: ['??0GameMode@@QAA@XZ'] },
  { unit: 'Rnd_Xbox',    targets: ['?BeginTiling@DxRnd@@AAAXABVColor@Hmx@@MI@Z'] },
  { unit: 'PostProcer',  targets: ['?Save@PostProcer@@UAAXAAVBinStream@@@Z'] },
  { unit: 'Wind',        targets: ['?SetWindOwner@RndWind@@QAAXPAV1@@Z'] },
  { unit: 'Geo',         targets: ['??O@YA_NABVSphere@@ABVFrustum@@@Z'] },
  { unit: 'CharUtl',     targets: ['?CharUtlFindBoneTrans@@YAPAVRndTransformable@@PBDPAVObjectDir@@@Z','?CharUtlFindBone@@YAPAVCharBone@@PBDPAVObjectDir@@@Z'] },
]

phase('Permute')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    unit: { type: 'string' },
    wins: { type: 'array', items: { type: 'object', additionalProperties: false,
      properties: { fn: { type: 'string' }, before: { type: 'number' }, after: { type: 'number' }, pattern: { type: 'string' } },
      required: ['fn','before','after'] } },
    applied_count: { type: 'integer' },
    net_delta: { type: 'integer' },
    patch_path: { type: ['string','null'] },
    m2c_loaded: { type: 'boolean' },
    bsf_available: { type: 'boolean' },
    notes: { type: 'string' },
  },
  required: ['unit','wins','applied_count','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Run the source permuter (decomp_synth) on VERIFIED real-logic permuter-class functions in rb3-xenon unit "${U.unit}". These are named near-miss (>=97%) functions whose residual is regalloc/instruction-scheduling (NOT struct-offset, NOT funclet noise, NOT STL/ICF folding), so the permuter is the right tool. The m2c + BSF guidance backends are now INSTALLED (commit 9763683): expect "m2c: loaded (...)" in the log, and BSF-guided declaration reorder is available.

SETUP: create an isolated buildable worktree:
  cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/pf-${U.unit} pf-${U.unit}
then work ENTIRELY inside it. NEVER edit/build/commit the main repo. If configure/dtk trips, re-run configure.py with explicit absolute --dtk ../jeff --objdiff ../objdiff --wrapper ../wibo/build/release/wibo per memory project_worktree_dtk_trap. First just ./tools/ninja-locked.

RECORD BASELINE matched_functions (${BASELINE}) from build/45410914/report.json measures.

TARGETS (${U.unit}): ${U.targets.map(t => '\n  - ' + t).join('')}

For EACH target, run (inside the worktree):
  venv/bin/python -m decomp_synth.scan_and_permute --symbol '<symbol>' --max-rounds 10 --max-variants 100 --plateau-limit 3 --no-apply 2>&1 | tee /tmp/pf_${U.unit}.log
Confirm the log shows "m2c: loaded" (record m2c_loaded). If a function is already 100% (my target list may be slightly stale), skip it. If a variant reaches TRUE 100% (match_percent_normalized == 100, NOT just a rounded fuzzy %), RE-RUN the same command WITHOUT --no-apply to write the winning source, then rebuild (rm build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked) and CONFIRM the function reads 100% in report.json AND whole-binary matched_functions strictly increased. Discard anything that only improves but does not hit a verified whole-binary-positive TRUE-100% (correctness model: commit only true-100% wins, never an unguarded rounded fuzzy).

After all targets: cd the worktree, mkdir -p ~/tmp/permf && git diff > ~/tmp/permf/${U.unit}.patch (empty if no wins). Compute net_delta = final matched_functions - ${BASELINE}. Return the schema with VERIFIED wins only (each with before/after match%). DO NOT commit to main. Leave the worktree in place.`,
    { label: `permute:${U.unit}`, phase: 'Permute', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
