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
  name: 'grind-execute2',
  description: 'Batch-2 coupled-base layout levers (Part, MoviePanel[game], FlowSound, VocalPlayer). Each agent verifies direction via objdiff FIRST, fixes one unit in a worktree, A/B whole-binary, returns net-positive patch. Never commits to main.',
  phases: [ { title: 'Execute', detail: 'one agent per lever' } ],
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
const BASELINE = 'PREFERRED (2026-08-01): python3 tools/ab_measure.py --worktree <your worktree> --from-dirty runs the ENTIRE A/B protocol (settle-to-zero, report cache wipes, strict keys, refusal on broken runs) and cannot quote an unmeasured absolute — use it instead of the manual steps below. Manual fallback: MEASURE IT YOURSELF in your own worktree BEFORE your first edit: rm -f build/45410914/report.cache build/45410914/report.json, full build, then read measures.matched_functions. Do NOT use any number written in this prompt, and do NOT read the MAIN repo report.json — lanes land by patch without rebuilding main, so main artifact goes stale by hundreds of functions (measured 40,925 while main was 41,168). Cross-check against the headline in docs/plans/decomp-state-2026-07-19.md'

const LEVERS = [
  { key: 'part', area: 'engine', est: 3, hintfile: 'grep for class RndParticleSys (src/system/rndobj/Part*.h)',
    fix: 'RndParticleSys member layout: near-miss accessors SetSubSamples/ExplicitParticles/InitParticle show [off:+16] at ~0x2c4 and [off:+24] at ~0x360 (compound). An embedded member/sub-type in RndParticleSys is mis-sized. VERIFY the exact target|ours offsets + direction via objdiff before editing. Cross-check rb3-Wii ../rb3/src + DC3 ../dc3-decomp/src.' },
  { key: 'moviepanel', area: 'game', est: 5, hintfile: 'src/system/movie/Movie.h (embedded Movie in MoviePanel)',
    fix: 'Embedded Movie is too small: currently mFaderGroup@0x0 + mImpl@0x4 = 8 bytes; retail wants ~12 so mSubtitlesLoader lands at 0x64. Add one 4-byte member after mImpl in Movie.h. VERIFY direction/offset via objdiff (target|ours) first. Cross-check rb3-Wii Movie layout. Movie is embedded in MoviePanel — re-measure MoviePanel + any other Movie embedder.' },
  { key: 'flowsound', area: 'engine', est: 5, hintfile: 'src/system/flow/FlowSound.h + FlowNode.h',
    fix: 'sizeof(FlowSound) is too BIG: header mCurrentIntensity@0xa0 implies 0xa4, but our build is 0xcc (+0x28). A base subobject (FlowNode / FlowLabelProvider) or FlowPtr<Sound> mSound is oversized by 0x28. VERIFY exact delta + which base via objdiff first. This is COUPLED across the Flow* family (FlowSay, FlowSubdir share the base) — re-measure all of them; a base shrink may help or hurt siblings, the build decides.' },
  { key: 'vocalplayer', area: 'game', est: 6, hintfile: 'src/band3/game/Player.h + Performer.h + MsgSource.h',
    fix: 'CORRECTED from batch-1: our build is 4 bytes too BIG (SHRINK by 4, do NOT widen — widening made it worse +4->+8). The real shift boundary is mParams@0x258 (target) where our build has 0x25c, NOT 0x260. Root cause: the shared Hmx::Object virtual-base subobject reached via the Player DOUBLE virtual inheritance (class Player : public Performer, public MsgSource) is 4 bytes too big in our layout. Investigate Performer/MsgSource/Hmx::Object (Hmx/Object.h) and the vbase ordering. This is COUPLED across ALL Player-derived instruments (Guitar/Bass/Drum/RealGuitar/Keyboard/VocalPlayer) — re-measure the whole family; if the vbase is genuinely 4-too-big it affects every class with that exact Performer+MsgSource combo. If the true fix is too broad/risky (regresses the family), report it as deferred with the precise root cause rather than landing a regression.' },
]

phase('Execute')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' }, applied: { type: 'boolean' }, net_delta: { type: 'integer' },
    improvements: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    landable: { type: 'boolean' }, patch_path: { type: ['string','null'] }, files_changed: { type: 'array', items: { type: 'string' } },
    approach: { type: 'string' }, blocked_reason: { type: ['string','null'] },
  },
  required: ['key','applied','net_delta','landable','approach'],
}

const results = await parallel(LEVERS.map(L => () =>
  agent(`Execute ONE coupled-base layout lever in rb3-xenon (matching MSVC-X360 PPC). Make a cluster of near-miss functions flip to 100% by fixing a struct/header layout error, WITHOUT regressing other units. The whole-binary build is the only arbiter.

LEVER "${L.key}" (${L.area}, est +${L.est}). File hint: ${L.hintfile}.
FIX: ${L.fix}

CRITICAL META-LEARNING from batch-1: the classification doc's grow-vs-shrink DIRECTIONS were frequently INVERTED. Before ANY edit, run objdiff/diff_inspect on 2-3 near-miss functions and read the offset annotations in the format "target | ours": if ours reads a HIGHER offset than target, our struct is too BIG (shrink); if LOWER, too small (grow). Do not trust the hypothesis direction — derive it from the asm.

Full evidence per unit is in ${DOC} (read your unit's section).

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/g3-${L.key} g3-${L.key}  ; work inside it.
2. RECON: confirm the exact offset deltas + DIRECTION via  python3 scripts/analysis/diff_inspect.py --symbol "<fn>" --compare-asm --project-dir .  on this unit's 99.x% functions. Cross-check correct layout against rb3-Wii (../rb3/src, game/shared) or DC3 (../dc3-decomp/src, engine).
3. Apply the minimal header edit. Prefer real oracle fields over anonymous padding.
4. ./tools/ninja-locked > ~/tmp/b2_${L.key}.log 2>&1  (near-full rebuild).
5. MEASURE whole-binary vs baseline ${BASELINE}: compare worktree report.json per-unit measures.matched_functions to ${REPO}/build/45410914/report.json. net_delta + improvements + regressions. Iterate to maximize net / zero regressions. A coupled base may regress siblings — that's real signal.
6. Optionally permute residual 99.x% regalloc near-misses: venv/bin/python -m decomp_synth.scan_and_permute --symbol '<fn>' --max-rounds 6 --no-apply (apply only TRUE-100%).
7. cd worktree && mkdir -p ~/tmp/grind3 && git diff > ~/tmp/grind3/${L.key}.patch
8. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree.

If net<=0 or only regresses, landable=false, explain in blocked_reason, write whatever diff exists. Be honest about measured net.`,
    { label: `g3:${L.key}`, phase: 'Execute', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
