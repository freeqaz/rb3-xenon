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
  name: 'grind-execute',
  description: 'Execute the independent per-unit struct_offset layout levers from the near-miss classification. Each agent fixes ONE unit layout in an isolated worktree, A/B whole-binary, returns a net-positive patch. Agents NEVER commit to main.',
  phases: [
    { title: 'Execute', detail: 'one agent per lever: worktree -> layout fix -> build -> whole-binary A/B -> patch' },
  ],
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

const LEVERS = [
  { key: 'mesh', area: 'engine', file: 'src/system/rndobj/Mesh.h', est: 18,
    fix: 'RndMesh +4/+8 cascade: grow VertVector 0x10->0x14 (an int/alignment member ~unkc) for +4 near ~0xe0, and add a 2nd 4-byte member near mPatches/mBones for +8. DC3==ours so this is an our-vs-RETAIL error; cross-check rb3-Wii ../rb3/src Mesh/RndMesh. Verify by re-diffing the 99.x% accessors show [off:+4]/[off:+8] resolved.' },
  { key: 'hamcamtransform', area: 'engine', file: 'src/system/hamobj/HamCamTransform.h', est: 15,
    fix: 'TransformArea 0x70->0x50: an embedded ObjPtrList/ObjVector/ObjPtr in TransformArea is OVER-sized by 0x20. Shrink TransformArea so sizeof==0x50. Verify target emits li r10,0x50 in the _M_erase/element-stride sites. Cross-check rb3-Wii ../rb3/src.' },
  { key: 'lightpreset', area: 'engine', file: 'src/system/world/LightPreset.h', est: 12,
    fix: 'LightPreset +0x3C/0x40 shortfall: the LightPreset base/embedded block is 0x3C too SMALL (diagnosis: uniform +60=0x3C across 7 accessors). Grow it by 0x3C so post-block members shift up. Cross-check rb3-Wii ../rb3/src LightPreset (RndLightPreset).' },
  { key: 'midiinstrument', area: 'engine', file: 'src/system/synth/SampleZone.h', est: 10,
    fix: 'MidiInstrument SampleZone element 0x1c->0x50: SampleZone (ObjPtr mSample@0x0, mVolume@0x14, mADSR@0x34) element size is wrong; realign so the std::vector<SampleZone> stride is 0x50. Cross-check rb3-Wii ../rb3/src (ObjPtr=0xc, mADSR@0x2c there). Edit SampleZone.h members only; do NOT touch MemOrPoolAllocSTL (separate lever).' },
  { key: 'postproc', area: 'engine', file: 'src/system/rndobj/PostProc.h', est: 9,
    fix: 'PostProc_NG RndPostProc base -12: the RndPostProc base prefix is 12 bytes (3 words) TOO LARGE. Remove 3 words from the pre-NgPostProc members (or the PostProcessor/Hmx::Object base region) so NgPostProc members shift down by 12. Confirm the CheckXXX accessors flip. Cross-check DC3 ../dc3-decomp/src + rb3-Wii.' },
  { key: 'vocalplayer', area: 'game', file: 'src/band3/game/Player.h', est: 6,
    fix: 'VocalPlayer / Player base +4 @0x260: members at >=0x260 are 4 bytes too LOW. Widen unk25c or unk260 (a vector) by 4 around Player.h:218-219 so members >=0x260 shift +4. THIS IS A COUPLED BASE: re-measure ALL Player-derived instruments (Guitar, Bass, Drum, RealGuitar, Keyboard, VocalPlayer). Cross-check rb3-Wii ../rb3/src Player.h (game oracle). Net must include the whole family.' },
]

phase('Execute')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' },
    applied: { type: 'boolean' },
    net_delta: { type: 'integer', description: 'whole-binary matched_functions delta vs baseline' },
    improvements: { type: 'array', items: { type: 'object', additionalProperties: false,
      properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false,
      properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    landable: { type: 'boolean', description: 'true iff net_delta>0 AND no unexplained regressions' },
    patch_path: { type: ['string','null'] },
    files_changed: { type: 'array', items: { type: 'string' } },
    approach: { type: 'string' },
    blocked_reason: { type: ['string','null'] },
  },
  required: ['key','applied','net_delta','landable','approach'],
}

const results = await parallel(LEVERS.map(L => () =>
  agent(`You are executing ONE coupled-base layout lever in the rb3-xenon decomp (matching MSVC-X360 PPC machine code). Goal: fix a struct/header layout error so a cluster of near-miss (99.x%) functions flips to 100%, WITHOUT regressing other units. The build is the only arbiter.

LEVER "${L.key}" (${L.area}):  target file ${L.file}  (est +${L.est})
FIX HYPOTHESIS: ${L.fix}

Full evidence for this unit (offset deltas, sample functions) is in the handoff doc: ${DOC} -- READ your unit's section first.

STEPS:
1. Create an isolated buildable worktree:  cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/g2-${L.key} g2-${L.key}   (waits ~1-3 min; CoW reflink + prime). Then work entirely inside .claude/worktrees/g2-${L.key}.
2. RECON: cd into the worktree, read ${L.file}, and confirm the layout error: run  python3 scripts/analysis/diff_inspect.py --symbol "<near-miss fn>" --compare-asm --project-dir .  on 2-3 of this unit's 99.x% functions (find them via report.json) to see the exact [off:+N]/[off:-N] member deltas. Cross-check the CORRECT layout against the oracle named in the fix (rb3-Wii ../rb3/src for game / shared RB3 code, DC3 ../dc3-decomp/src for engine).
3. APPLY the minimal header edit (grow/shrink/realign members) that makes our member offsets match the target. Prefer named real fields from the oracle over anonymous padding when the oracle shows them.
4. BUILD:  ./tools/ninja-locked > ~/tmp/build_${L.key}.log 2>&1  (header change => near-full rebuild, several min). Confirm it builds.
5. MEASURE whole-binary: baseline matched_functions == ${BASELINE}. Compare your worktree's new build/45410914/report.json per-unit measures.matched_functions against the main repo's ${REPO}/build/45410914/report.json. Compute net_delta + list every unit that went UP (improvements) and DOWN (regressions). A COUPLED base may regress siblings -- if so, that's real signal: either the layout is wrong, or a sibling needs the same/opposite fix. Iterate the edit to maximize net (zero regressions ideal).
6. If a few targeted functions remain at 99.x% after the layout fix (residual regalloc), you MAY run the permuter on them: venv/bin/python -m decomp_synth.scan_and_permute --symbol '<fn>' --max-rounds 6 --no-apply  (apply only TRUE-100% wins by re-running without --no-apply).
7. Write the patch:  cd .claude/worktrees/g2-${L.key} && mkdir -p ~/tmp/grind2 && git diff > ~/tmp/grind2/${L.key}.patch
8. Return the schema. landable = net_delta>0 AND no unexplained regressions. Set patch_path. DO NOT commit to main and DO NOT git push. Leave the worktree in place.

If the fix nets <=0 or only regresses, set landable=false, explain in blocked_reason, and still write whatever diff you have (or empty). Be rigorous and honest about the measured net.`,
    { label: `grind:${L.key}`, phase: 'Execute', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
