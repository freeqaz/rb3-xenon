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
  name: 'permuter-sweep',
  description: 'Run decomp_synth permuter on the VERIFIED permuter-class named functions (regalloc/scheduling, not funclet/struct). Apply only TRUE-100% wins. One agent per unit in a shared worktree.',
  phases: [
    { title: 'Permute', detail: 'per-unit permuter runs on named near-miss functions; apply true-100% only' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'

// verified permuter-class units + their NAMED (non-funclet) near-miss targets
const UNITS = [
  { unit: 'Rnd_Xbox', targets: ['?Present@DxRnd@@QAAXXZ','?SetupGamma@DxRnd@@AAAXXZ','?SetDefaultRenderStates@DxRnd@@QAAXXZ'] },
  { unit: 'Geo', targets: ['?Intersect@@YA_NABVPlane@@ABVBox@@@Z','?Intersect@@YA_NABVSegment@@ABVTriangle@@_NAAM@Z','?Clip@@YAXABVPolygon@Hmx@@ABVRay@2@AAV12@@Z','??O@YA_NABVSphere@@ABVFrustum@@@Z'] },
  { unit: 'CharHair', targets: ['?SetRoot@Strand@CharHair@@QAAXPAVRndTransformable@@@Z'] },
  { unit: 'StreakMeter', targets: ['?SyncObjects@StreakMeter@@UAAXXZ'] },
  { unit: 'CharBone', targets: ['?StuffBones@CharBone@@QBAXAAV?$list@UBone@CharBones@@V?$StlNodeAlloc@UBone@CharBones@@@stlpmtx_std@@@stlpmtx_std@@H@Z'] },
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
    patch_path: { type: ['string','null'] },
    notes: { type: 'string' },
  },
  required: ['unit','wins','applied_count','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Run the source permuter (decomp_synth) on VERIFIED permuter-class functions in rb3-xenon unit "${U.unit}". These are named near-miss (93-99%) functions whose residual is regalloc/scheduling (NOT struct-offset or funclet-noise), so the permuter is the right tool.

SETUP: create an isolated worktree:  cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/perm-${U.unit} perm-${U.unit}  then work inside it.

TARGETS (${U.unit}): ${U.targets.map(t => '\n  - ' + t).join('')}

For EACH target, run (inside the worktree):
  venv/bin/python -m decomp_synth.scan_and_permute --symbol '<symbol>' --max-rounds 10 --max-variants 100 --plateau-limit 3 --no-apply 2>&1 | tee /tmp/perm_${U.unit}.log
Read the result. If a variant reaches TRUE 100% (match_percent_normalized == 100, not just a rounded fuzzy %), RE-RUN the same command WITHOUT --no-apply to write the winning source. VERIFY by rebuilding the unit obj and confirming the function lands at 100% in report.json. Discard anything that only improves but doesn't hit a verified 100% (per the correctness model: commit only true-100% wins, never an unguarded rounded fuzzy).

After all targets: cd the worktree, mkdir -p ~/tmp/perm && git diff > ~/tmp/perm/${U.unit}.patch  (empty if no wins). Return the schema with verified wins only. DO NOT commit to main. Leave the worktree.`,
    { label: `permute:${U.unit}`, phase: 'Permute', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
