export const meta = {
  name: 'bodyport-tails-w1',
  description: 'Body-port the near-miss tails (92-99.99%) in already-wired+pinned TUs — reconstruct the MWCC->MSVC body-divergent methods to byte-match retail. Scout classifies divergence (body-fixable vs permuter-regalloc vs struct-lever), ports only the body-fixable ones, composed A/B per port.',
  phases: [
    { title: 'Scout', detail: 'read report.json: list 92-99.99% near-misses in wired TUs, classify divergence, pick body-fixable' },
    { title: 'Port', detail: 'one agent per target: reconstruct the divergent body in a worktree, composed A/B, commit honest winners' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
// session-landed class-A TUs whose tails the honesty gate flagged (highest-prior near-misses)
const PRIOR = 'FocusTracker.cpp, MetaMusic.cpp, EntityUploader.cpp, UIStats.cpp, BandLeadMeter.cpp, TrackWidget.cpp, TrackDir.cpp, StoreInfoPanel.cpp, PatchPanel.cpp, TrackerDisplay.cpp, StarDisplay.cpp, CheckboxDisplay.cpp'

const SCOUT_SCHEMA = {
  type: 'object', additionalProperties: false, required: ['targets', 'notes'],
  properties: {
    targets: { type: 'array', items: { type: 'object', additionalProperties: false,
      required: ['tu', 'fn', 'pct', 'divergence_class', 'why_bodyfixable'],
      properties: {
        tu: { type: 'string' }, fn: { type: 'string' }, pct: { type: 'number' },
        divergence_class: { type: 'string', enum: ['BODY', 'STRUCT', 'REGALLOC', 'MIXED'] },
        why_bodyfixable: { type: 'string' },
      } } },
    notes: { type: 'string' },
  },
}

phase('Scout')
const scout = await agent(
`READ-ONLY scout for BODY-PORTABLE near-miss tails in the rb3-xenon decomp (${REPO}, main HEAD, build/45410914/report.json). Goal: find functions at 92-99.99% that are CLOSE to matching and whose remaining gap is a fixable SOURCE BODY divergence (MWCC->MSVC port artifact: a missing/extra inlined helper, a wrong constant/branch, a struct-member access at the wrong offset that is LOCAL to this TU's header, a Handle/MILO macro form, a by-value-vs-ref param), NOT a permuter-class regalloc/scheduling gap (proven 0-win) and NOT a base-class struct-layout lever (out of scope here).

PROCEDURE:
1. Parse build/45410914/report.json for functions with fuzzy match in [92.0, 99.99). PRIORITIZE these recently-landed class-A TUs (their tails were flagged by the honesty gate): ${PRIOR}. Then broaden to other wired TUs if you have budget.
2. For each candidate, use the diff tooling to CLASSIFY the divergence: mcp__orchestrator__run_diff_inspect (diagnose/clusters modes), the /compare-asm and /recon skills, /stack-layout. Decide divergence_class:
   - BODY = a source-level difference you can reconstruct (missing inlined call, wrong immediate, extra/missing branch, wrong member offset fixable in THIS TU's own header, macro form). THESE ARE THE TARGETS.
   - STRUCT = needs a shared/base-class layout change (out of scope — skip).
   - REGALLOC = only register allocation / instruction scheduling differs (permuter-class, source-unreachable — skip).
   - MIXED/unclear = skip unless clearly body-dominated.
3. Return up to 8 BODY-class targets, highest pct first (closest to matching = cheapest port). For each: tu, fn (mangled or fn_VA), pct, divergence_class, why_bodyfixable (the SPECIFIC asm evidence + what the source fix is).

Be honest and selective — a BODY classification must have concrete asm evidence of a source-fixable difference. Return SCOUT_SCHEMA. Read-only, no edits/builds.`,
  { label: 'scout:bodyport', phase: 'Scout', schema: SCOUT_SCHEMA }
)
const targets = (scout?.targets || []).filter(t => t.divergence_class === 'BODY').slice(0, 8)
log(`Scout: ${targets.length} BODY-fixable targets — ${targets.map(t => `${t.fn}@${t.pct}`).join(', ') || '(none)'}`)

phase('Port')
const PORT_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['tu', 'fn', 'landed_100', 'before_pct', 'after_pct', 'branch', 'verified_composed', 'notes'],
  properties: {
    tu: { type: 'string' }, fn: { type: 'string' }, landed_100: { type: 'boolean' },
    before_pct: { type: 'number' }, after_pct: { type: 'number' }, branch: { type: 'string' },
    verified_composed: { type: 'boolean' }, notes: { type: 'string' },
  },
}
function portPrompt(t) {
  return `BODY-PORT one near-miss to TRUE 100% in the rb3-xenon decomp: ${t.fn} in ${t.tu} (currently ${t.pct}%). The scout classified this as a fixable SOURCE BODY divergence: ${t.why_bodyfixable}
The TU is ALREADY wired + pinned in main, so this is pure body reconstruction (no new pin/wire). Recipe = docs/decomp/playbooks/bodyport-wave.md + CLAUDE.md.
HARD RULES: work in your OWN CoW worktree (scripts/setup_worktree.sh /tmp/wt-bp-${t.fn.replace(/[^A-Za-z0-9]/g,'').slice(0,20)} bp-${t.fn.replace(/[^A-Za-z0-9]/g,'').slice(0,20)}); add the download_tool.py skip-guard + ln -sf ${REPO}/build/tools/wibo build/tools/wibo if a fresh build re-downloads. NEVER edit main. Do NOT land — commit to your branch + RETURN it.
STEPS: (1) baseline fresh_report (rm -f build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked once) -> before_pct for ${t.fn}. (2) reconstruct the divergent body in the TU's .cpp/.h (use the rb3-Wii oracle ../rb3/src + DC3 ../dc3-decomp/src to merge intent; the diff tooling mcp__orchestrator__run_diff_inspect / /compare-asm to converge). Iterate until objdiff shows ${t.fn} at TRUE 100% byte-equal. (3) WHOLE-BINARY composed A/B (rm stamp + touch config.yml + fresh_report, read measures.matched_functions) must be net >= +1 with ZERO unexplained regressions (if your edit touches a shared header, gate the JUDGMENT on the whole-binary number). (4) tools/icf_alias_check.py --worktree <wt>. (5) If TRUE-100 + net>=+1 + 0 regressions: commit to your branch (end msg: Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>) and set landed_100=true, verified_composed=true. If you cannot reach TRUE 100% or it regresses, report after_pct + landed_100=false (do NOT commit a partial). A clean +0 with the reason is a valid honest result.
Return PORT_SCHEMA.`
}
const results = []
for (let i = 0; i < targets.length; i += 3) {
  const batch = targets.slice(i, i + 3)
  log(`Port batch ${i/3+1}: ${batch.map(t=>t.fn).join(', ')}`)
  const r = await parallel(batch.map(t => () => agent(portPrompt(t), { label: `bp:${t.fn.slice(0,20)}`, phase: 'Port', schema: PORT_SCHEMA })))
  r.filter(Boolean).forEach(x => results.push(x))
}
const wins = results.filter(r => r.landed_100 && r.verified_composed)
log(`BODY-PORT: ${wins.length}/${results.length} TRUE-100 — ${wins.map(r=>`${r.fn}(${r.before_pct}->100)`).join(', ') || '(none)'}`)
return {
  winners: wins.map(r => ({ tu: r.tu, fn: r.fn, branch: r.branch, before: r.before_pct })),
  refuted: results.filter(r => !(r.landed_100 && r.verified_composed)).map(r => ({ fn: r.fn, after: r.after_pct, notes: r.notes })),
  recommendation: 'Coordinator: land each winner branch (land.sh or cherry-pick the body diff), configure.py drop-check, composed verify run1==run2, 0 regressions.',
}
