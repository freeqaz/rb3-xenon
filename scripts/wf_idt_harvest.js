export const meta = {
  name: 'idtransfer-harvest-v1',
  description: 'Harvest GOOD-oracle scattered TUs (oracle_quality-selected): port, pin only good-oracle methods, measure real wins',
  phases: [
    { title: 'Harvest', detail: 'one agent per good-oracle TU: port, wire, pin oracle_quality GOOD methods ∩ field-gate ∩ obj-defined, build, measure, audit' },
  ],
}

// good-oracle ∧ has-source ∧ not-pinned (from tools/oracle_quality.py). Mix of
// meta_band/ui/tour/network to test portability across families.
const TARGETS = [
  { tu: 'band3/meta_band/OvershellPanel.cpp', good: 18 },
  { tu: 'band3/meta_band/MetaPanel.cpp', good: 11 },
  { tu: 'band3/meta_band/AppLabel.cpp', good: 11 },
  { tu: 'band3/tour/TourProgress.cpp', good: 9 },
  { tu: 'band3/meta_band/MetaPerformer.cpp', good: 6 },
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    tu: { type: 'string' },
    good_oracle_predicted: { type: 'number' },
    ported: { type: 'boolean' }, wired_and_compiled: { type: 'boolean' },
    methods_defined_in_obj: { type: 'number' },
    pin_set_size: { type: 'number' },
    matched_delta: { type: 'number' },
    real_matches: { type: 'array', items: { type: 'string' } },
    near_misses: { type: 'array', items: { type: 'string' } },
    verdict: { type: 'string' },
    splits_edits_path: { type: 'string' },
    body_divergence_killed: { type: 'number' },
    notes: { type: 'string' },
  },
  required: ['tu', 'good_oracle_predicted', 'ported', 'matched_delta', 'verdict', 'notes'],
}

function prompt(t) {
  const base = t.tu.split('/').pop().replace('.cpp', '')
  return `Harvest the GOOD-oracle scattered TU **${t.tu}** through the identity-transfer pipeline. oracle_quality.py predicts ${t.good} good-oracle real-bodied methods here (vs the B2 warm-up which failed on LOW-good-oracle TUs). Goal: confirm the reframe — do GOOD-oracle methods actually land? Produce a calibration point (good_oracle_predicted vs landed). Work in your own CoW worktree; NEVER mutate main (/home/free/code/milohax/rb3-xenon).

## Read first
- docs/decomp/identity-transfer/PIPELINE-DESIGN.md (pipeline + the 10 honesty gates).
- The B2 finding (this matters): the rb3-Wii->retail oracle MISATTRIBUTES many VAs (retail fn at the oracle VA is 5-25x the oracle size, or already owns a foreign name). tools/oracle_quality.py --tu ${base}.cpp lists the GOOD methods (size-consistent, not foreign-owned) — PIN ONLY THESE. Do not pin oracle rows that oracle_quality flags mis-size/foreign (they are +0 misattributions).

## Procedure (buildable worktree)
1. \`scripts/setup_worktree.sh /tmp/wt-h-${base} h-${base}\`. In the worktree: (a) add the download skip-guard to tools/download_tool.py (before \`print(f"Downloading...\`: \`if output.exists() and (not output.is_dir() or any(output.iterdir())):\\n        print("skip"); return\`); (b) **fresh worktrees lack build/tools/wibo — symlink it from main: \`ln -sf ${'/home/free/code/milohax/rb3-xenon'}/build/tools/wibo build/tools/wibo\`** (and build/binutils if a full build needs it). B2 friction: building a SINGLE .obj target avoids the 'tools' phony's sjiswrap/binutils downloads.
2. \`python3 tools/oracle_quality.py --tu ${base}.cpp\` → record the GOOD method VAs (the pin candidates).
3. **PORT** ../rb3/src/${t.tu} -> src/${t.tu} (MWCC->MSVC X360). Whole file must COMPILE + DEFINE the methods; do NOT hand-match bodies (trust objdiff). Apply the playbook (decomp.h macros are no-ops under MSVC; fix include paths; API-compat deltas vs DC3 headers).
4. **WIRE**: add "${t.tu}":"NonMatching" to config/45410914/objects.json; \`python3 configure.py\`; build (\`./tools/ninja-locked\`). Confirm obj DEFINES the methods (record methods_defined_in_obj).
5. **BASELINE**: matched_functions from build/45410914/report.json.
6. **FIELD-GATE**: \`python3 tools/field_offset_gate.py --tu ${base}.cpp --oracle unified_id_rb3wii.json --emit-pin-only /tmp/fg-${base}.json\` (use --D / --class if a class's first member is a real array-of-heavy-member; for FLAT structs keep the safe D=inf default — do NOT let --infer-d over-poison, the B2 Scoring trap).
7. **PIN-SET = GOOD-oracle VAs (step 2) ∩ field-gate-clean (step 6) ∩ methods-DEFINED-in-obj.** This triple intersection is the B2 fix (don't pin misattributed or undefined VAs). DRY-RUN identity_transfer first; keep only VAs it reports as nameable (named>0).
8. **MICRO-PIN+MAP**: \`python3 tools/identity_transfer.py --tu ${base}.cpp --oracle unified_id_rb3wii.json --pin-only <intersected-set> --apply\` (STRICT add-only; never gen_game_target_map.py --apply).
9. **OVERLAP**: \`python3 scripts/harvest/overlap_check.py config/45410914/splits.txt --text-only\` (abort on overlap).
10. **BUILD+MEASURE**: \`rm -f build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked\`; delta vs baseline.
11. **AUDIT** (HARD): \`python3 tools/icf_alias_check.py --worktree .\` — newly-100 must be REAL (>44B) bodies, not <=44B stub-folds. Count body_divergence_killed = GOOD-oracle methods that pinned cleanly but stayed <100% (real-bodied near-misses = the body-divergence wall, distinct from misattribution).
12. **VERDICT**: LANDABLE:+N (N = REAL non-stub matched delta, no regressions) or DEFER:<reason>. Save the splits.txt + target_symbol_map.json diff to /tmp/h-${base}.patch.

## Honesty (decisive)
- byte-equality is the ONLY positive gate; sim is NOT a predictor.
- A clean +0 (good-oracle methods that pinned but body-diverged) is a VALID, important calibration result — report body_divergence_killed and WHY (axis B inlining / D regalloc / C Handle). This tells us the real hit-rate on good-oracle methods (RockCentral was 17/18 = 94%).

Return the schema. Leave the worktree; report its path + the patch path.`
}

phase('Harvest')
const results = await parallel(TARGETS.map((t) => () =>
  agent(prompt(t), { label: `harvest:${t.tu.split('/').pop().replace('.cpp', '')}`, phase: 'Harvest', schema: SCHEMA })
))
const ok = results.filter(Boolean)
const landable = ok.filter((r) => /^LANDABLE/.test(r.verdict || ''))
const totalReal = landable.reduce((s, r) => s + (r.matched_delta || 0), 0)
const predicted = ok.reduce((s, r) => s + (r.good_oracle_predicted || 0), 0)
const landed = ok.reduce((s, r) => s + (/^LANDABLE/.test(r.verdict || '') ? r.matched_delta : 0), 0)
const divKilled = ok.reduce((s, r) => s + (r.body_divergence_killed || 0), 0)
ok.forEach((r) => log(`${r.tu}: ${r.verdict} delta=${r.matched_delta} (predicted ${r.good_oracle_predicted} good; bodyDiv-killed ${r.body_divergence_killed || 0})`))
log(`HARVEST: ${landable.length}/${ok.length} landable, +${totalReal}; good-oracle hit-rate ${landed}/${predicted}, body-divergence killed ${divKilled}`)
return { results: ok, landable: landable.map((r) => ({ tu: r.tu, delta: r.matched_delta, matches: r.real_matches, patch: r.splits_edits_path })), totalReal, predicted, landed, divKilled }
