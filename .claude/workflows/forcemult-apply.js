export const meta = {
  name: 'forcemult-apply',
  description: 'Apply the force-multiplier finder candidates: member-delta (DC3 dropped/added member -> add/remove to match retail layout) + inline-policy tail (flip method inline/out-of-line). Each agent applies ONE candidate in an isolated worktree, whole-binary A/B, returns a net-positive verified patch. NEVER commit to main.',
  phases: [ { title: 'Apply', detail: 'one agent per force-multiplier candidate: identify member/method -> fix -> whole-binary A/B -> patch' } ],
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

// member-delta: {class, kind:'member'}; inline: {target, kind:'inline'}. From ~/tmp/forcemult/*.json
const CANDS = [
  { slug: 'VocalPlayer',    kind: 'member', detail: 'class VocalPlayer, C=-4 @ ~0x278 (band3/game, rb3-Wii oracle), n=4 high' },
  { slug: 'NgPostProc',     kind: 'member', detail: 'class NgPostProc, C=-16 (0x10), n=2 high (engine, DC3 oracle)' },
  { slug: 'GameMode',       kind: 'member', detail: 'class GameMode, C=-84 (0x54), n=1 high — large delta, likely one big embedded member or base' },
  { slug: 'RndTexRenderer', kind: 'member', detail: 'class RndTexRenderer, C=-12 (0xc), n=1 high (engine, DC3 oracle)' },
  { slug: 'Player',         kind: 'member', detail: 'class Player, C=-4 (0x4), n=4 med (band3/game, rb3-Wii oracle)' },
  { slug: 'math-inline',    kind: 'inline', detail: 'inline-policy tail: MakeShortAng, FastSin (free math fns, currently DECL-only in their header — retail INLINES them, we call bl). Also OvershellSlotState::HandleMsg (INLINE, decl-only). Make each inline ONLY if it nets positive.' },
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    slug: { type: 'string' }, kind: { type: 'string' },
    landable: { type: 'boolean' }, net_delta: { type: 'integer' },
    files_changed: { type: 'array', items: { type: 'string' } },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    patch_path: { type: ['string','null'] },
    root_cause: { type: 'string' },
    tooling_gaps: { type: 'array', items: { type: 'string' } },
    notes: { type: 'string' },
  },
  required: ['slug','kind','landable','net_delta','root_cause','notes'],
}

phase('Apply')

const WT = (s) => `.claude/worktrees/fa-${s}`
const SETUP = (s) => `cd ${REPO} && scripts/setup_worktree.sh ${WT(s)} fa-${s} ; work ENTIRELY inside it — NEVER edit/build/commit main. First ./tools/ninja-locked; if dtk/configure trips: python3 configure.py --dtk /home/free/code/milohax/jeff/target/release/dtk --objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli --wrapper /home/free/code/milohax/wibo/build/release/wibo.`
const AB = `WHOLE-BINARY A/B: baseline matched_functions (${BASELINE}) BEFORE; after edit rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ./tools/ninja-locked; re-read measures. net_delta=after-baseline. Diff the 100%-normalized fn SETS for real regressions. LANDABLE iff net_delta>0 AND no net real regressions.`

const thunks = CANDS.map(c => () => agent(
  c.kind === 'member'
  ? `Apply the member-delta force-multiplier for ${c.detail}. The class is one or more members too SMALL/large vs retail (DC3 newer dropped/added a member), so every this-relative member access is shifted by the uniform delta. ${SETUP(c.slug)}
STEPS:
1. Baseline matched_functions (${BASELINE}). Read the candidate detail in ~/tmp/forcemult/member_candidates.json (offset, delta, fix_hint).
2. CONFIRM direction with objdiff --include-instructions on the class's near-miss fns: is there a UNIFORM this-relative offset delta (NOT funclet/stack noise)? Find the exact offset where the member is missing/extra. delta<0 means OUR class is SMALLER than retail (add a member); delta>0 means larger (remove one).
3. IDENTIFY the member via the oracle: DC3 (../dc3-decomp/src), rb3-Wii (../rb3/src), Ghidra (ghidra-struct/struct_db). Prefer a real named member from the oracle over a blind pad; place it at the right offset so other member offsets stay correct + add any ctor init it needs.
4. APPLY the header (+ ctor). ${AB} Watch for regressions in OTHER users of the class. If no clean uniform delta exists (coupled-base/vbase wall, or DxRnd-style real-delta-but-no-flip), set landable=false + report.
5. mkdir -p ~/tmp/fa && cd ${WT(c.slug)} && git diff > ~/tmp/fa/${c.slug}.patch. Return SCHEMA (slug='${c.slug}', kind='member'). NEVER commit to main; leave worktree. Report tooling_gaps you hit.`
  : `Apply the inline-policy tail candidates: ${c.detail}. These are methods where retail INLINES the body but we emit an out-of-line bl (or vice versa). ${SETUP(c.slug)}
STEPS:
1. Baseline matched_functions (${BASELINE}). Read ~/tmp/forcemult/inline_candidates.json (+ _wide) for the precise callee + header + current_form + fix_hint. Use tools/inline_policy_finder.py --sym '<mangled>' to inspect.
2. For EACH candidate method, find its decl/def. INLINE direction (retail inlines, we call out-of-line): move the body from the .cpp into the header as an inline definition (the MakeShortAng/FastSin/OvershellSlotState case). VERIFY the header form is currently decl-only (actionable). For free fns (MakeShortAng/FastSin) be careful: inlining changes ALL callers binary-wide — A/B decides.
3. ${AB} Apply each candidate INDEPENDENTLY and keep ONLY those that net positive (a free-fn inline can regress other callers retail kept out-of-line). Revert any that don't help.
4. mkdir -p ~/tmp/fa && cd ${WT(c.slug)} && git diff > ~/tmp/fa/${c.slug}.patch (only the net-positive flips). Return SCHEMA (slug='${c.slug}', kind='inline'), net_delta = sum of kept flips. NEVER commit to main; leave worktree. Report tooling_gaps.`,
  { label: `${c.kind}:${c.slug}`, phase: 'Apply', schema: SCHEMA }))

const results = (await parallel(thunks)).filter(Boolean)
const landable = results.filter(r => r.landable && r.net_delta > 0)
log(`forcemult-apply: ${landable.length}/${results.length} landable, total net +${landable.reduce((a,r)=>a+r.net_delta,0)}`)
return { results, landable }
