export const meta = {
  name: 'levers-permuter-structlever',
  description: 'Surgical permuter on specific regalloc-class near-misses (NO bulk sweep) + MetaPanel-family struct-lever',
  phases: [
    { title: 'Scout', detail: 'pick specific permuter-class targets in non-colliding units + assess MetaPanel struct-lever' },
    { title: 'Execute', detail: 'permuter on the few targets (batched 2-at-a-time, CPU-safe) + MetaPanel layout fix' },
  ],
}

// concurrent autonomous waves are active in: AppLabel, TrackPanelDir, GemManager,
// dc3-naming, sizedvec, and the rndenviron Hmx::Object base-layout cascade (affects
// rndobj-derived units). The scout must avoid those.
const AVOID = 'AppLabel, TrackPanelDir(Base), GemManager, dc3-naming, sizedvec, and the Hmx::Object base-layout cascade (rndobj-derived units in flux: Rnd, Dir, CharEyes, RndEnviron, etc.)'

const SCOUT_SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    permuter_targets: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        properties: {
          unit: { type: 'string' }, fn: { type: 'string' }, pct: { type: 'number' },
          divergence_class: { type: 'string' }, why_permuter_safe: { type: 'string' },
        },
        required: ['unit', 'fn', 'pct', 'divergence_class'],
      },
    },
    metapanel_tractable: { type: 'boolean' },
    metapanel_plan: { type: 'string' },
    notes: { type: 'string' },
  },
  required: ['permuter_targets', 'metapanel_tractable', 'metapanel_plan'],
}

phase('Scout')
const scout = await agent(
  `Read-only scout for a SURGICAL permuter wave + a struct-lever, in the rb3-xenon decomp (/home/free/code/milohax/rb3-xenon, main HEAD, ${'build/45410914/report.json'} baseline). Concurrent autonomous waves are running — you MUST pick work that does NOT collide with them. AVOID these units/levers: ${AVOID}.

## Part 1 — pick 4-5 SPECIFIC permuter targets (NOT a bulk sweep)
The user wants the permuter reserved for specific high-value targets only (it thrashes CPU). Find 4-5 functions that are:
- 98.0-99.99% matched (the permuter only closes the last regalloc/scheduling gap).
- divergence_class = REGALLOC or SCHEDULING ONLY (register-allocation swaps / instruction-scheduling reordering). NOT struct-offset (layout), NOT funclet/asm-misnest, NOT inlining-policy, NOT data-constant — the permuter cannot fix those. Confirm via the recon / compare-asm / diff_inspect tooling (mcp__orchestrator__run_diff_inspect, the /recon and /compare-asm skills) on each candidate.
- in units NOT in the AVOID set, and ideally not base-class-cascade-sensitive (a pure within-function regalloc gap is layout-immune, so even a derived-class TU is fine IF the specific function's only diff is regalloc).
Pick varied units (don't stack 5 in one). For each: unit, fn (mangled or fn_VA), pct, divergence_class, why_permuter_safe (the specific asm evidence it is regalloc-only).

## Part 2 — assess the MetaPanel-family struct-lever
My identity-transfer harvest found MetaPanel.cpp near-misses gated on axis-A struct-layout: ?Exiting@MetaPanel@@UBA_NXZ was 99.93% blocked ONLY by mMusic at this+0x60 (ours) vs this+0x64 (retail) = a single member-offset shift; ?Enter@ writes this+0x33c/0x340 implying retail MetaPanel object ~0x344B vs our header ~0xd8B. Investigate (Ghidra struct-info / asm / the header src/band3/meta_band/MetaPanel.h + its bases UIPanel/PanelDir): is there a TRACTABLE, CASCADING struct-lever here (a member added/resized before mMusic that, once corrected to retail layout, lands Exiting AND cascades to sibling panel methods), or is it a ~600B full-reconstruction (too big/risky)? Check it is NOT already being fixed by a concurrent agent. Return metapanel_tractable + a concrete metapanel_plan (the exact member/offset edit) or why-defer.

Return the schema. Read-only — no edits, no builds.`,
  { label: 'scout', phase: 'Scout', schema: SCOUT_SCHEMA }
)

log(`scout: ${scout?.permuter_targets?.length || 0} permuter targets; metapanel_tractable=${scout?.metapanel_tractable}`)

// ---- Execute: permuter batched 2-at-a-time (CPU-safe) + optional struct-lever ----
phase('Execute')
const targets = (scout?.permuter_targets || []).slice(0, 5)
const PERMUTE_SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    unit: { type: 'string' }, fn: { type: 'string' },
    landed_100: { type: 'boolean' }, before_pct: { type: 'number' }, after_pct: { type: 'number' },
    patch_path: { type: 'string' }, verified_composed: { type: 'boolean' }, notes: { type: 'string' },
  },
  required: ['unit', 'fn', 'landed_100', 'notes'],
}
function permutePrompt(t) {
  return `SURGICAL permuter on ONE function: ${t.fn} in ${t.unit} (currently ${t.pct}%, divergence ${t.divergence_class}). Goal: close the last regalloc/scheduling gap to TRUE 100%. Use the source permuter (the /permute skill or scripts/decomp_synth / m2c-permuter machinery). Work in an ISOLATED worktree (scripts/setup_worktree.sh; symlink build/tools/wibo from main if a fresh build needs it; download skip-guard). Apply a variation ONLY if it reaches TRUE 100% (objdiff byte-equal) AND a whole-binary composed A/B shows net >=+1 with ZERO regressions. If no variation hits 100%, report the best and landed_100=false (do NOT apply partial). KEEP CPU MODEST — do not fan out massive permuter parallelism; this is one targeted function. Save any net-positive splits/source diff to /tmp/perm-<fn>.patch. Return the schema. Do NOT commit to main.`
}
// batch into pairs to cap concurrent permuter load at 2
const permuteResults = []
for (let i = 0; i < targets.length; i += 2) {
  const batch = targets.slice(i, i + 2)
  const r = await parallel(batch.map((t) => () =>
    agent(permutePrompt(t), { label: `permute:${t.fn.slice(0, 24)}`, phase: 'Execute', schema: PERMUTE_SCHEMA })
  ))
  r.filter(Boolean).forEach((x) => permuteResults.push(x))
}

let structLever = null
if (scout?.metapanel_tractable) {
  structLever = await agent(
    `MetaPanel-family STRUCT-LEVER. Per the scout's plan: ${scout.metapanel_plan}\n\nIn an ISOLATED worktree (composed-verify against current main HEAD), apply the struct-layout fix to the MetaPanel/panel-class header so the retail member offsets are correct (the scout identified the specific edit). Verify the gated near-misses (esp. ?Exiting@MetaPanel@@ at 99.93%) reach 100% AND a whole-binary composed A/B is net >=+1 with ZERO unexplained regressions (shared-header soft-rule: gate the JUDGMENT on the whole-binary A/B). Re-check no concurrent agent already fixed this (no-op if so). Save the patch to /tmp/metapanel-structlever.patch. Return a short report: landed delta, the exact edit, verified_composed bool, any regressions. Do NOT commit to main.`,
    { label: 'structlever:MetaPanel', phase: 'Execute' }
  )
}

const wins = permuteResults.filter((r) => r.landed_100 && r.verified_composed)
log(`permuter: ${wins.length}/${permuteResults.length} landed TRUE-100; structlever=${structLever ? 'attempted' : 'skipped'}`)
return {
  permuter_wins: wins.map((r) => ({ unit: r.unit, fn: r.fn, delta: `${r.before_pct}->100`, patch: r.patch_path })),
  permuter_refuted: permuteResults.filter((r) => !r.landed_100).map((r) => ({ fn: r.fn, best: r.after_pct, notes: r.notes })),
  structlever: structLever,
}
