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
  name: 'nearmiss-classify',
  description: 'Classify the near-miss (95-99.99%) function pool by ROOT CAUSE per unit, to separate systematic repeat-pattern levers (one fix -> many funcs/units) from one-off permuter-class noise. Read-only.',
  phases: [
    { title: 'Classify', detail: 'parallel agents sample near-misses per unit-group, classify dominant cause' },
    { title: 'Synthesize', detail: 'rank systematic levers across units, write handoff doc' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'

// unit groups (game first per priority). near-miss counts from the 2026-06-06 scan.
const GROUPS = [
  { label: 'game', units: ['NetSync','StorePanel','VocalPlayer','StreakMeter','MoviePanel','RockCentral'] },
  { label: 'rndobj-mesh', units: ['Rnd','Mesh','MeshAnim','TexBlender','Group','PartAnim'] },
  { label: 'render-xbox', units: ['Rnd_Xbox','TexRenderer','DepthBuffer3D','PostProc_NG','Geo'] },
  { label: 'world', units: ['LightPreset','Spotlight','SpotlightDrawer_NG','Crowd','CameraShot'] },
  { label: 'bandobj', units: ['BandDirector','Part','EventTrigger'] },
  { label: 'char-midi', units: ['MidiParser','MidiInstrument','CharHair','CharBone'] },
  { label: 'ham-data', units: ['HamCamTransform','Gen','DataFile','DataFunc','ContentMgr_Xbox','WaveFile','FlowSound'] },
]

const RUBRIC = `
ROOT-CAUSE BUCKETS (assign the DOMINANT one per unit, judging from sampled near-misses):
- struct_offset : near-misses are uniform single-immediate offset deltas ([off:+N]/[off:-N]) on member accesses, same/consistent N across the unit => an embedded-type-size or member-layout error. SYSTEMATIC: one header/layout fix flips many. (This is how the rbtree +29 lever looked.)
- vtable_slot   : a vtable load/branch uses the wrong slot offset (lwz r,0xNN(vtbl) off by a slot) => coupled vtable layout. Systematic across a class family.
- symbol_pairing: diff is a [sym] call-rename only -- our 'bl lbl_<addr>'/'bl fn_<addr>' vs target 'bl ??1Foo@@' (a NAMED function), bytes otherwise identical => the callee just needs a target_symbol_map reveal entry OR the callee itself isn't matched yet. SYSTEMATIC + cheap (reveal sweep / pin the callee).
- funclet_noise : the ONLY diffs are in an EH-cleanup funclet: 'subi r31, r12, FRAMESIZE' frame-reconstruct delta and/or the funclet's single dtor 'bl'. These are objdiff FALSE POSITIVES / parent-frame artifacts -- usually NOT independently fixable. Flag as DROP unless paired with a real cause.
- regalloc      : register-swap diffs ([reg:rN->rM]), same opcodes, different registers => PERMUTER-class (decl reorder / cast). Not a systematic lever.
- scheduling    : same instructions in a different ORDER (insert/delete pairs that are reorders) => PERMUTER-class.
- body_diff     : genuinely different instructions/logic (extra/missing real work, different constants, different callee that ISN'T just a rename) => BODY-PORT (DC3/rb3-Wii source divergence). Per-function grind.
- mixed         : no single dominant cause.

For each unit also judge: systematic = would ONE fix (a header/layout edit, a vtable fix, a reveal entry, pinning a shared callee) flip MANY of this unit's near-misses (and possibly other units)? vs one-off permuter/body grind.
`

const CMD = `Run per function:  cd ${REPO} && timeout 90 python3 scripts/analysis/diff_inspect.py --symbol "<fn-or-mangled>" --compare-asm --project-dir .
Read the markers: ~ diff_arg (same op, diff args -> check [off:]/[reg:]/[sym]), ! diff_op, +/- insert/delete (reorder or real body), X replace. The "Diagnosis Summary" gives dominant offset delta + regswap pairs. Funclets look like a ~10-instruction function starting with 'subi r31, r12, 0xNNN'.`

phase('Classify')

const UNIT_SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    group: { type: 'string' },
    units: { type: 'array', items: {
      type: 'object', additionalProperties: false,
      properties: {
        unit: { type: 'string' },
        near_count: { type: 'integer' },
        sampled: { type: 'integer' },
        dominant_cause: { type: 'string', enum: ['struct_offset','vtable_slot','symbol_pairing','funclet_noise','regalloc','scheduling','body_diff','mixed'] },
        systematic: { type: 'boolean' },
        lever_hypothesis: { type: 'string', description: 'the concrete fix that would flip many, or "none (permuter/body grind)"' },
        cross_unit: { type: 'string', description: 'does this same pattern likely affect OTHER units? which?' },
        est_recoverable: { type: 'integer' },
        sample_evidence: { type: 'string', description: 'cite 2-3 functions + the exact diff signature observed' },
      },
      required: ['unit','dominant_cause','systematic','lever_hypothesis','est_recoverable','sample_evidence'],
    } },
  },
  required: ['group','units'],
}

const findRemaining = `To get this unit's current near-misses: cd ${REPO} && python3 -c "import json; r=json.load(open('build/45410914/report.json')); [print(f['name'],round(f['match_percent_normalized'],1)) for u in r['units'] if u['name'].split('/')[-1]=='UNIT' for f in u['functions'] if 95<=f.get('match_percent_normalized',0)<100]"  (replace UNIT).`

const classified = await parallel(GROUPS.map(g => () =>
  agent(`You are classifying the ROOT CAUSE of near-miss (95-99.99%) functions in rb3-xenon (matching MSVC-X360 PPC machine code) to find SYSTEMATIC levers (one fix -> many matches) vs permuter-class one-offs. This directly informs what we work next; GAME units are higher priority than engine, but engine layout fixes often unblock game code.

${RUBRIC}

${CMD}

${findRemaining}

YOUR UNIT GROUP "${g.label}": ${g.units.join(', ')}
For EACH unit: list its current 95-99.99% near-misses, sample 3-4 of them with diff_inspect, assign the dominant_cause + systematic flag + a concrete lever_hypothesis + est_recoverable (how many of its near-misses one fix would flip) + cross_unit note + cited evidence. Return the schema. Analysis only -- do NOT edit or build.`,
    { label: `classify:${g.label}`, phase: 'Classify', schema: UNIT_SCHEMA })
))

phase('Synthesize')

const flat = classified.filter(Boolean).flatMap(c => c.units.map(u => ({ group: c.group, ...u })))

const synth = await agent(`You are synthesizing a near-miss root-cause classification for rb3-xenon into a RANKED LEVER LIST + a handoff doc. Here is the per-unit classification (JSON):

${JSON.stringify(flat, null, 1)}

Produce:
1. A ranked list of SYSTEMATIC levers (dominant_cause that repeats across MANY units, where one fix-type flips many functions). For each: lever name, root-cause bucket, affected units, total est_recoverable, whether it's game/engine/mixed, and the concrete first action. Rank by (est_recoverable x confidence), with a GAME-priority tiebreak.
2. A clearly separated list of units that are PERMUTER-class (regalloc/scheduling) -> candidates for a /permute sweep, with total count.
3. Units that are FUNCLET-NOISE -> DROP.
4. Units that are BODY-PORT grind -> per-function, lower priority.

Then WRITE the full analysis to ${REPO}/docs/decomp/near-miss-classification-2026-06-06.md (create it; use the Write tool) as a handoff doc: include the ranked levers, the per-unit table, and a "NEXT ACTIONS" section the next agent can execute cold. Return a compact JSON summary {top_levers:[{name,bucket,units,est,area,first_action}], permuter_units:[...], permuter_total:int, drop_units:[...], bodyport_units:[...], doc_path}.`,
  { label: 'synthesize', phase: 'Synthesize', schema: {
    type: 'object', additionalProperties: false,
    properties: {
      top_levers: { type: 'array', items: { type: 'object', additionalProperties: false, properties: {
        name: { type: 'string' }, bucket: { type: 'string' }, units: { type: 'array', items: { type: 'string' } },
        est: { type: 'integer' }, area: { type: 'string' }, first_action: { type: 'string' } },
        required: ['name','bucket','units','est','area','first_action'] } },
      permuter_units: { type: 'array', items: { type: 'string' } },
      permuter_total: { type: 'integer' },
      drop_units: { type: 'array', items: { type: 'string' } },
      bodyport_units: { type: 'array', items: { type: 'string' } },
      doc_path: { type: 'string' },
    },
    required: ['top_levers','permuter_units','permuter_total','drop_units','bodyport_units','doc_path'],
  } })

return synth
