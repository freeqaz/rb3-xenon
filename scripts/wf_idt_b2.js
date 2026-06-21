export const meta = {
  name: 'idtransfer-b2-warmup',
  description: 'B2 warm-up: port + wire + identity-transfer-harvest fresh scattered TUs end-to-end; prove the partial-port pipeline + seed calibration',
  phases: [
    { title: 'Warmup', detail: 'one agent per fresh TU: port MWCC->MSVC, wire NonMatching, field-gate, micro-pin head-only, build, measure, honesty-audit, verdict' },
  ],
}

// All verified this turn: un-wired, un-pinned, rb3-Wii source present.
const TARGETS = [
  { tu: 'band3/game/ChordPreview.cpp', sz: '4 methods / 88L', note: 'tiny guaranteed-source CONTROL' },
  { tu: 'band3/game/Scoring.cpp', sz: '5 / 366L', note: '' },
  { tu: 'band3/game/PerfectSectionTracker.cpp', sz: '6 / 395L', note: '' },
  { tu: 'band3/meta_band/SongSortNode.cpp', sz: '7 / 454L', note: 'canonical ICF-scattered class' },
  { tu: 'band3/tour/TourPerformerLocal.cpp', sz: '7 / 534L', note: '' },
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    tu: { type: 'string' },
    ported: { type: 'boolean' },
    wired_and_compiled: { type: 'boolean' },
    methods_defined: { type: 'number' },
    case_a: { type: 'number' }, case_b: { type: 'number' }, self_case: { type: 'number' },
    pin_set_size: { type: 'number' },
    poisoned_tail_excluded: { type: 'number' },
    matched_delta: { type: 'number' },
    real_matches: { type: 'array', items: { type: 'string' } },
    verdict: { type: 'string' },
    splits_edits_path: { type: 'string' },
    port_notes: { type: 'string' },
    pipeline_friction: { type: 'string' },
  },
  required: ['tu', 'ported', 'matched_delta', 'verdict', 'port_notes'],
}

function prompt(t) {
  return `You are running a B2 warm-up of the identity-transfer harvest pipeline on ONE fresh ICF-scattered TU: **${t.tu}** (${t.sz}; ${t.note}). Goal: prove the partial-port + field-gate pipeline END-TO-END and produce a calibration data point. Work entirely in your own CoW worktree; NEVER mutate /home/free/code/milohax/rb3-xenon main.

## Read first
- docs/decomp/identity-transfer/PIPELINE-DESIGN.md (the pipeline + the 10 honesty gates) and research/04-sourceport-bottleneck.md (the PORTING PLAYBOOK — divergence axes A/B/C/D, partial-port strategy).
- The new tools you will drive: tools/field_offset_gate.py (--tu, --D, --infer-d, --class, --emit-pin-only, --out), tools/identity_transfer.py (--tu, --oracle unified_id_rb3wii.json, --pin-only <json>, --apply), scripts/harvest/overlap_check.py, tools/icf_alias_check.py.
- Oracle: ../rb3/src/${t.tu} (MWCC source) + unified_id_rb3wii.json (VA+name). Engine headers: ../dc3-decomp + ../rb3.

## Procedure (do it in a buildable worktree)
1. \`scripts/setup_worktree.sh /tmp/wt-b2-${t.tu.split('/').pop().replace('.cpp','')} b2-${t.tu.split('/').pop().replace('.cpp','')}\`. In the worktree, add to tools/download_tool.py (right before \`print(f"Downloading {url} to {output}")\` in main()):  \`if output.exists() and (not output.is_dir() or any(output.iterdir())):\\n        print("present, skip"); return\`  so the build doesn't re-fetch compilers.
2. **PORT** ../rb3/src/${t.tu} -> src/${t.tu} (MWCC->MSVC X360). Apply the playbook: handle decomp.h FORCE_LOCAL_INLINE (->nothing under MSVC), pragmas, includes, MWCC-isms. PARTIAL PORT IS FINE: the whole file must COMPILE and DEFINE every method's symbol, but only the byte-matchable subset will be pinned. Do NOT hand-fix bodies to match — trust objdiff.
3. **WIRE**: add "${t.tu}": "NonMatching" to config/45410914/objects.json; \`python3 configure.py\`; build just this obj (\`./tools/ninja-locked build/45410914/src/${t.tu.replace('.cpp','.obj')}\` or full \`./tools/ninja-locked\`). Confirm the compiled obj DEFINES the methods.
4. **BASELINE**: full build, record measures.matched_functions from build/45410914/report.json.
5. **FIELD-GATE**: \`python3 tools/field_offset_gate.py --tu ${t.tu} --oracle unified_id_rb3wii.json --emit-pin-only --out /tmp/pinset.json\` (use --class / --D if a class's first member is array/embedded — see the tool's --infer-d output; prefer a conservative D). This yields the head-only pin-set.
6. **MICRO-PIN+MAP**: \`python3 tools/identity_transfer.py --tu ${t.tu} --oracle unified_id_rb3wii.json --pin-only /tmp/pinset.json --apply\` (in the worktree). STRICT add-only map; never gen_game_target_map.py --apply.
7. **OVERLAP**: \`python3 scripts/harvest/overlap_check.py config/45410914/splits.txt --text-only\` — abort if it fails.
8. **BUILD+MEASURE**: \`rm -f build/45410914/.../target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked\`; matched_functions delta vs step-4 baseline.
9. **AUDIT** (HARD honesty gate): \`python3 tools/icf_alias_check.py --worktree .\` (or the documented invocation) — the newly-100 must be REAL bodies (>44B), NOT <=44B ICF-stub folds. Confirm each real_match by size. If the delta is dominated by stub-folds => verdict DEFER:icf-inflation.
10. **VERDICT**: LANDABLE:+N (N = REAL non-stub matched delta, >0, no regressions) or DEFER:<reason>. Save the worktree's splits.txt + target_symbol_map.json diff to /tmp/b2-<tu>.patch and report its path.

## Honesty (decisive)
- A method counts ONLY if it byte-matches with its OWN real body (not an ICF-stub coincidence). sim is NOT a predictor — do not use it to include/exclude.
- If the port can't compile, or 0 real matches, that is a VALID, useful result — report ported=true/false, matched_delta, and WHY (which divergence axis killed it). Even +0 with a clean compile validates the transport.
- Report pipeline_friction: anything awkward about driving the tools on a FRESH (not-yet-wired) TU — this feeds the driver's fresh-TU mode.

Return the schema. Leave your worktree in place; report its path.`
}

phase('Warmup')
const results = await parallel(TARGETS.map((t) => () =>
  agent(prompt(t), { label: `b2:${t.tu.split('/').pop().replace('.cpp', '')}`, phase: 'Warmup', schema: SCHEMA })
))

const ok = results.filter(Boolean)
const landable = ok.filter((r) => /^LANDABLE/.test(r.verdict || ''))
const totalReal = landable.reduce((s, r) => s + (r.matched_delta || 0), 0)
ok.forEach((r) => log(`${r.tu}: ${r.verdict} (delta ${r.matched_delta}, ported=${r.ported})`))
log(`B2: ${landable.length}/${ok.length} landable, total real matched delta ${totalReal}`)
return { results: ok, landable: landable.map((r) => ({ tu: r.tu, delta: r.matched_delta, matches: r.real_matches, patch: r.splits_edits_path })), totalReal }
