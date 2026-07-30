export const meta = {
  name: 'rbtree-triage',
  description: 'R-B tree +4: triage the units that REGRESS when _Rb_tree grows 0x18->0x1c. Per-unit asm diagnosis to recover lost matches without breaking the gains. Read-only analysis.',
  phases: [
    { title: 'Triage', detail: 'one agent per regressed unit -> diagnosis + precise recovery edit (or irreducible)' },
  ],
}

const WT = '/home/free/code/milohax/rb3-xenon/.claude/worktrees/stlmap'
const MAIN = '/home/free/code/milohax/rb3-xenon'

// regressed units with measured deltas (worktree-with-member vs base). CharClip excluded (false positive: vector<map>).
const TARGETS = [
  { unit: 'default/band3/meta_band/AccomplishmentProgress', cls: 'AccomplishmentProgress', delta: -12,
    header: 'src/band3/meta_band/AccomplishmentProgress.h',
    hint: '9 maps. unk50 already unwound. 10 near-misses are pure-4 (should improve) but 12 previously-matched fns broke. Find what ADDITIONAL hidden compensation (mis-sized member, reorder, explicit offset) the previously-matched post-map accessors depended on at 0x18.' },
  { unit: 'default/Song', cls: 'Song', delta: -4, header: 'src/Song.h',
    hint: '1 map (mSongSections@0x24), 0 pads. 4 matched fns broke. Classic single-map regressor: a previously-matched function that touches a post-map member was tuned to 0x18.' },
  { unit: 'default/MusicLibrary', cls: 'MusicLibrary', delta: -1, header: 'src/band3/meta_band/MusicLibrary.h',
    hint: '1 map, complex class with many unk fields. 1 fn broke.' },
  { unit: 'default/DirLoader', cls: 'DirLoader', delta: -1, header: null,
    hint: 'predicted regressor (1 map, 47 matched). Verify if it actually regresses and why.' },
]

const BACKGROUND = `
CONTEXT — rb3-xenon decomp (matching MSVC-X360 PPC machine code). We are growing STLport \`_Rb_tree\` from 0x18 to 0x1c by adding one 4-byte member (\`size_type _M_unused;\`) after \`_M_key_compare\` in src/system/stlport/stl/_tree.h:316. This is the retail layout (proven: AccomplishmentManager gained +28, its 41 near-misses were all pure multiple-of-4 offset deltas). Every std::map/set/multimap/multiset member is 4 bytes larger, so members declared AFTER an embedded map/set member move +4.

THE WORKTREE: ${WT} has the +4 member applied AND fully built. So objdiff/diff_inspect there shows the POST-change residual mismatch for every function. The pristine baseline (WITHOUT the member) is the main repo report at ${MAIN}/build/45410914/report.json -- valid for the units below (none were touched by other agents' recent commits).

YOUR JOB: for ONE regressed unit, explain WHY previously-matched functions broke when the map grew, and find the precise SOURCE EDIT that recovers them WITHOUT breaking the functions that improved. The pad census already proved there are NO more simple unk*/pad* compensation members to remove (only AccomplishmentProgress::unk50, already unwound). So a recovery here means finding HIDDEN compensation: a member with a wrong size/type, a wrong declaration ORDER, an explicit padding array, or a member that should/shouldn't exist -- something that made the OLD 0x18 layout coincidentally match for these functions.

METHOD:
1. Find which functions regressed: compare per-function match_percent_normalized for this unit between baseline (${MAIN}/build/45410914/report.json) and worktree (${WT}/build/45410914/report.json). Functions that dropped from ~100 to <100 are your targets; functions that rose are the gains you must NOT break.
2. For 2-4 regressed functions, run in the worktree:  cd ${WT} && python3 scripts/analysis/diff_inspect.py --symbol "<mangled-or-name>" --compare-asm --project-dir .   Read the [off:+N]/[off:-N] annotations: they pinpoint which member offset is now wrong and by how much. A residual +4 means our member is now 4 too HIGH (over-shifted -> something needs REMOVING/shrinking); a residual that was 0 before and is now -4 means under-shifted.
3. Cross-reference the class layout against the rb3-Wii named oracle (${MAIN}/../rb3/src) and DC3 engine twin (${MAIN}/../dc3-decomp/src) to identify the true retail field layout. Decide the exact edit.
4. CRITICAL: verify your proposed edit would not break the IMPROVED functions (those touching members further down). State the risk.

If the regression is a FALSE POSITIVE (e.g. the 'map' is a nested vector<map> value type with no own-layout effect, like CharClip), say so. If it is REAL but irreducible without a full body-port, say so with the reason. Be concrete and cite the diff_inspect offsets you observed.
`

phase('Triage')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    unit: { type: 'string' },
    verdict: { type: 'string', enum: ['recoverable', 'false_positive', 'irreducible'] },
    regressed_fns: { type: 'array', items: {
      type: 'object', additionalProperties: false,
      properties: { name: { type: 'string' }, before_pct: { type: 'number' }, after_pct: { type: 'number' },
        offset_delta: { type: 'string' } },
      required: ['name', 'after_pct'] } },
    diagnosis: { type: 'string' },
    edits: { type: 'array', items: {
      type: 'object', additionalProperties: false,
      properties: { file: { type: 'string' }, line_hint: { type: 'integer' },
        action: { type: 'string', enum: ['remove', 'add', 'reorder', 'resize', 'none'] },
        detail: { type: 'string' } },
      required: ['file', 'action', 'detail'] } },
    expected_recovery: { type: 'integer' },
    breaks_gains_risk: { type: 'string' },
    confidence: { type: 'string', enum: ['high', 'medium', 'low'] },
  },
  required: ['unit', 'verdict', 'diagnosis', 'edits', 'expected_recovery', 'confidence'],
}

const results = await parallel(TARGETS.map(t => () =>
  agent(`${BACKGROUND}

YOUR UNIT: ${t.unit}  (class ${t.cls}, measured delta ${t.delta})
${t.header ? 'Header: ' + t.header : 'Header: locate it (grep for class ' + t.cls + ').'}
Specific hint: ${t.hint}

Produce the schema. If recoverable, give the EXACT edit(s) with file + line + before/after. expected_recovery = how many functions you expect to flip back to 100%. Remember: do NOT edit or build anything -- this is analysis only.`,
    { label: `triage:${t.cls}`, phase: 'Triage', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
