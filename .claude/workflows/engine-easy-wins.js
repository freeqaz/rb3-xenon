export const meta = {
  name: 'engine-easy-wins',
  description: 'Close out engine easy wins: per-unit struct/layout fixes (DC3-newer added/dropped member -> uniform member-offset shift) on the top STRUCT_WORK engine units, plus a couple unwired-engine-source wirings. Each agent: isolated worktree, recon-first, layout fix, whole-binary A/B, net-positive patch. SKIP permuter/regalloc. NEVER commit to main.',
  phases: [ { title: 'Engine', detail: 'one agent per engine unit: recon -> layout/wire fix -> build -> whole-binary A/B -> patch' } ],
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

// Top engine STRUCT_WORK units (true_progress), excluding already-worked (LightPreset/MidiInstrument/
// PostProc_NG/Mesh/HamCamTransform/SpotlightDrawer done in prior waves). Each = candidate layout force-multiplier.
const LAYOUT_UNITS = [
  'Rnd', 'Anim', 'Utl', 'Rnd_Xbox', 'EventTrigger', 'Geo',
  'SpotlightDrawer_NG', 'DirLoader', 'Group', 'LightHue', 'Dir', 'CubeTex',
]
// Unwired engine dc3 source to try wiring (UIListProvider-class +N reveals); agent vets RB3-real vs vendor/false-friend.
const WIRE_UNITS = [
  'system/char/CharClipDisplay.cpp', 'system/flow/PropertyEventListener.cpp',
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    kind: { type: 'string' },          // 'layout' | 'wire'
    unit: { type: 'string' },
    landable: { type: 'boolean' },
    net_delta: { type: 'integer' },
    files_changed: { type: 'array', items: { type: 'string' } },
    objects_entry: { type: ['string','null'] },
    splits_block: { type: ['string','null'] },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    patch_path: { type: ['string','null'] },
    root_cause: { type: 'string' },
    notes: { type: 'string' },
  },
  required: ['kind','unit','landable','net_delta','root_cause','notes'],
}

phase('Engine')

const WT = (s) => `.claude/worktrees/ee-${s.replace(/[\/.]/g,'_')}`
const SETUP = (s) => `cd ${REPO} && scripts/setup_worktree.sh ${WT(s)} ee-${s.replace(/[\/.]/g,'_')} ; work ENTIRELY inside it — NEVER edit/build/commit main. First ./tools/ninja-locked; if dtk/configure trips: python3 configure.py --dtk /home/free/code/milohax/jeff/target/release/dtk --objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli --wrapper /home/free/code/milohax/wibo/build/release/wibo.`
const AB = `WHOLE-BINARY A/B: baseline matched_functions (${BASELINE}) from build/45410914/report.json BEFORE; after edit, rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ./tools/ninja-locked, re-read measures. net_delta=after-baseline. Diff the 100%-normalized fn SETS for real regressions. LANDABLE iff net_delta>0 AND no net real regressions.`

const layoutThunks = LAYOUT_UNITS.map(u => () => agent(
  `Fix engine unit "${u}" near-misses via a STRUCT/CLASS LAYOUT lever in rb3-xenon (MSVC-X360 PPC). Many of its STRUCT_WORK near-misses likely share ONE root cause: DC3 (newer than RB3) added/removed/reordered a member, so every member access in the class's methods is shifted by a fixed offset (e.g. lwz r11,0x2c where retail has 0x28). Fixing the header to RB3's true layout flips all affected fns at once (force-multiplier). ${SETUP(u)}

STEPS:
1. Baseline matched_functions (${BASELINE}).
2. RECON: list the unit's <100% named fns (report.json / batch-check). objdiff 2-3 of them with --include-instructions; look for a UNIFORM member-offset delta across this-relative loads/stores (lwz/lfs/stw off r3/r31/this) = a layout bug. DISTINGUISH from: funclet noise (subi r31,r12 frame-recon — tooling, ignore), regalloc/scheduling (permuter-class — SKIP per instructions), and naming-only bl diffs (cascade, ignore here). If the residual is NOT a clean uniform layout delta, set landable=false and report root_cause (don't force it).
3. LOCATE the fix: identify the exact member that's off. Cross-check the class layout vs DC3 (../dc3-decomp/src) + Ghidra (ghidra-struct/struct_db) + rb3-Wii (../rb3/src). The fix is usually: re-add a DC3-dropped member, remove a DC3-added member, or correct a member width — matched to the asm offsets (retail is ground truth). Prefer a real named member from an oracle over a blind pad.
4. APPLY the header (and any ctor init the member needs). ${AB}
5. mkdir -p ~/tmp/ee && cd ${WT(u)} && git diff > ~/tmp/ee/${u}.patch (empty if nothing landable). files_changed = edited headers/cpp. Return SCHEMA (kind='layout', unit='${u}'). NEVER commit to main; leave worktree.`,
  { label: `layout:${u}`, phase: 'Engine', schema: SCHEMA }))

const wireThunks = WIRE_UNITS.map(rel => { const u=rel.split('/').pop().replace('.cpp',''); return () => agent(
  `Try to WIRE the unwired engine TU "${rel}" into objdiff matches (MSVC-X360 PPC). The .cpp already exists in src/ (from DC3) but isn't in the build. If its functions are byte-identical to RB3 retail (DC3 engine is faithful), wiring + pinning yields immediate matches (the UIListProvider +9 pattern). ${SETUP(u)}

STEPS:
1. Baseline matched_functions (${BASELINE}).
2. VET: is this real shared RB3 engine, or DC3-specific (gesture/Kinect/Dancer) / vendor / false-friend? Check whether RB3 retail has byte-similar fns for this TU (dc3_content_match-style: compare src/.../${rel} compiled bytes against the catch-all). If it's DC3-only-game or has no RB3 cluster, set landable=false and report (a clean negative).
3. Find the .text cluster: compute the span from the report fn_ addresses around the TU's functions (snap to fn boundaries, non-overlapping). Add "${rel}": "NonMatching" to config/45410914/objects.json (engine group) + the .text pin to config/45410914/splits.txt.
4. rm build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked. Add target_symbol_map.json entries for the matched fns (reveal_sweep + safe_name_merge for byte-exact ones). Port MWCC/DC3->MSVC only if needed to compile.
5. ${AB}
6. mkdir -p ~/tmp/ee && cd ${WT(u)} && git add -A && git diff --cached > ~/tmp/ee/${u}.patch. Record files_changed, objects_entry, splits_block. Return SCHEMA (kind='wire', unit='${rel}'). NEVER commit to main; leave worktree.`,
  { label: `wire:${u}`, phase: 'Engine', schema: SCHEMA }) })

const results = (await parallel([...layoutThunks, ...wireThunks])).filter(Boolean)
const landable = results.filter(r => r.landable && r.net_delta > 0)
log(`engine-easy-wins: ${landable.length}/${results.length} landable, total net +${landable.reduce((a,r)=>a+r.net_delta,0)}`)
return { results, landable }
