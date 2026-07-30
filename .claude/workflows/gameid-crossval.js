export const meta = {
  name: 'gameid-crossval',
  description: 'Cross-validated game-TU LOCATION: BinDiff (structural, stub-masked + 627-pin fixed-point-seeded) AND BSim (Ghidra p-code semantic), calibrated on the 25 known game pins, intersected for high-confidence candidate game-TU .text spans to pin. Build artifacts + spans to ~/tmp/gameid/. Ghidra access serialized into one agent (projects are single-process; RB3Xenon may be locked by an in-progress import). NEVER commit to main.',
  phases: [
    { title: 'Foundations', detail: 'build bindiff (no ghidra) || serial Ghidra export+BSim-DB+query' },
    { title: 'Locate',      detail: 'bindiff-locate || bsim-locate, each calibrated on the 25 known pins' },
    { title: 'CrossValidate', detail: 'intersect bindiff + bsim spans -> high-confidence candidate game-TU pins' },
  ],
}

const MILO = '/home/free/code/milohax'
const REPO = `${MILO}/rb3-xenon`
const GHIDRA = `${MILO}/ghidra/build/ghidra/support`
const RB3X_PROJ = `${REPO}/ghidra_projects`          // RB3Xenon (target, anon) — MAY BE LOCKED (import)
const WII_PROJ = `${MILO}/rb3/ghidra_projects`        // RB3 (rb3-Wii, named game oracle) — unlocked
const OUT = '~/tmp/gameid'
const CALIB = '/tmp/known_game_pins.json'             // 25 known-good game TU spans = calibration ground truth

const FOUND_SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    ok: { type: 'boolean' }, what: { type: 'string' },
    artifacts: { type: 'object', additionalProperties: { type: 'string' } },  // name -> path
    blockers: { type: 'array', items: { type: 'string' } },
    notes: { type: 'string' },
  },
  required: ['ok','what','artifacts','blockers','notes'],
}
const LOC_SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    ok: { type: 'boolean' }, stream: { type: 'string' },
    calibrated: { type: 'boolean' },               // did it correctly locate the 25 known pins?
    calib_precision: { type: 'number' }, calib_recall: { type: 'number' },
    candidate_tus: { type: 'integer' }, candidate_fns: { type: 'integer' },
    spans_path: { type: ['string','null'] },        // JSON: [{unit,start,end,confidence,n_fns}]
    blockers: { type: 'array', items: { type: 'string' } },
    notes: { type: 'string' },
  },
  required: ['ok','stream','calibrated','candidate_tus','spans_path','blockers','notes'],
}

phase('Foundations')

const bindiffBuild = () => agent(`Build Google BinDiff from source so we can MODIFY its matcher (stub-mask + fixed-point seeding). Source: ${MILO}/bindiff (pristine clone); the sibling ${MILO}/binexport is already cloned (its CMake expects ../binexport). NO Ghidra needed here. Work in ${MILO}/bindiff (NEVER touch the rb3-xenon repo/build/git).

STEPS:
1. cmake configure+build bindiff (out-of-tree build dir, e.g. ${MILO}/bindiff/build). It FetchContents sqlite; needs protobuf + abseil. CAVEAT: system protobuf is 35.0.0 but bindiff wants 3.14+ — if the system protobuf breaks the build (likely API incompat), let bindiff's own deps mechanism fetch a compatible protobuf/abseil (check cmake/BinDiffDeps.cmake + CMakeLists for FetchContent/find_package toggles), or pass -DCMAKE_*; spend reasonable effort, don't flail.
2. If the source build SUCCEEDS: record the built bindiff binary path. We'll modify it in the Locate phase. Confirm it runs (--help).
3. If the source build FAILS after reasonable effort: that's OK — report it as a blocker. The prebuilt /usr/bin/bindiff handles STOCK passes; the stub-mask can be done by PREPROCESSING the .BinExport (drop stub functions) without source, and fixed-point seeding may be possible via the CLI/groundtruth file — investigate /usr/bin/bindiff --help options (--ground_truth? config?) and report what's possible without the source build.
Return FOUND_SCHEMA (what='bindiff-build'): artifacts={bindiff_bin: <path>}, blockers=[build failures], notes=[which protobuf/abseil path worked, or the prebuilt fallback plan].`,
  { label: 'bindiff-build', phase: 'Foundations', schema: FOUND_SCHEMA })

const ghidraExport = () => agent(`Own ALL Ghidra access SERIALLY (Ghidra projects are single-process; do NOT run concurrent headless sessions on one project). Produce the inputs both game-ID streams need. Ghidra headless: ${GHIDRA}/analyzeHeadless ; BSim: ${GHIDRA}/bsim ; BinExport plugin is installed at /opt/bindiff/extra/ghidra/BinExport. Projects: rb3-Wii named oracle = ${WII_PROJ} (project 'RB3', UNLOCKED); target anon = ${RB3X_PROJ} (project 'RB3Xenon', possibly LOCKED by an in-progress import — RB3Xenon.lock present). mkdir -p ${OUT}.

STEPS (do the UNLOCKED rb3-Wii side FIRST so we always have oracle artifacts):
1. rb3-Wii .BinExport: headless-export every function of project RB3 to ${OUT}/rb3wii.BinExport (via the BinExport plugin's headless exporter / ExportFunctionsScript).
2. BSim DB from rb3-Wii: ${GHIDRA}/bsim createdatabase file:${OUT}/rb3wii.bsim medium_nosize ; ${GHIDRA}/bsim generatesigs ghidra:${WII_PROJ}/RB3/RB3 <sigdir> --bsim file:${OUT}/rb3wii.bsim --commit (medium_nosize tolerates size/arch deltas). This is the named-game signature DB.
3. RB3Xenon (target) — PROBE THE LOCK FIRST: check ${RB3X_PROJ}/RB3Xenon/.../*.lock and try a quick read-only analyzeHeadless open. (a) If accessible: export RB3Xenon -> ${OUT}/rb3xenon.BinExport AND run a BSim query of every anonymous RB3Xenon fn against ${OUT}/rb3wii.bsim -> ${OUT}/bsim_query.json (ranked named-match+similarity per fn). (b) If LOCKED/inaccessible after a short retry (don't busy-wait long): set that as a blocker, DO NOT disrupt the running import, and report — the rb3-Wii-side artifacts are still delivered.
Return FOUND_SCHEMA (what='ghidra-export'): artifacts={rb3wii_binexport, rb3wii_bsim, rb3xenon_binexport?, bsim_query?} with the paths that succeeded; blockers=[RB3Xenon locked, etc.]; notes.`,
  { label: 'ghidra-export', phase: 'Foundations', schema: FOUND_SCHEMA })

const [build, ghidra] = await parallel([bindiffBuild, ghidraExport])

phase('Locate')

const bindiffLocate = () => agent(`Locate candidate game-TU .text spans with BinDiff, calibrated on the 25 known game pins. Foundations results: bindiff build = ${JSON.stringify(build).slice(0,500)} ; ghidra artifacts = ${JSON.stringify(ghidra && ghidra.artifacts).slice(0,400)}. Calibration ground truth: ${CALIB} (25 pinned band3 TUs with spans). NO rb3-Wii oracle cross-ref is available. **DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit). Coverage-stub detector: ~/tmp/recon/common.py (verified). mkdir -p ${OUT}. NEVER touch rb3-xenon git/build/main.

PREREQ: need ${OUT}/rb3wii.BinExport AND ${OUT}/rb3xenon.BinExport. If the RB3Xenon export is a blocker (locked import), set ok=false, calibrated=false, blockers=['target BinExport unavailable'] and STOP (valid — we resume when the import clears).

STEPS:
1. STOCK baseline: run bindiff (built binary if available else /usr/bin/bindiff) on rb3wii.BinExport vs rb3xenon.BinExport -> matches.
2. CALIBRATE on the 25 known pins: for each known pinned game TU, do BinDiff's matches cluster correctly inside its [start,end) span? Compute precision/recall of locating those 25 TUs. This is the GATE.
3. MODIFY for our case: (a) STUB-MASK — drop the 32/40-byte coverage-breadcrumb functions (use the common.py detector) from the BinExport/matching so they stop creating false anchors/aliases; (b) FIXED-POINT SEED — feed our 627 known pins (+ high-conf oracle hits) as fixed points so the call-graph MD-index top-down/bottom-up + string-reference passes propagate from them. (If the source build succeeded, modify the matcher; else do (a) by preprocessing the .BinExport and (b) via the CLI ground-truth/config if supported.)
4. RE-CALIBRATE on the 25 pins — did precision/recall improve? Only trust the method if calibration is good (don't emit confident-wrong spans).
5. If calibrated: emit candidate game-TU spans for the UNPINNED scattered TUs (GemManager/GemPlayer/VocalPart/etc.) -> ${OUT}/bindiff_spans.json = [{unit,start,end,confidence,n_fns}], snapped to fn boundaries, non-overlapping with the 627 pins.
Return LOC_SCHEMA (stream='bindiff').`,
  { label: 'bindiff-locate', phase: 'Locate', schema: LOC_SCHEMA })

const bsimLocate = () => agent(`Locate candidate game-TU .text spans with BSim (Ghidra p-code semantic similarity), calibrated on the 25 known game pins. Foundations ghidra artifacts = ${JSON.stringify(ghidra && ghidra.artifacts).slice(0,400)}. Need ${OUT}/bsim_query.json (per-anon-fn ranked named matches vs the rb3-Wii BSim DB). Calibration: ${CALIB}. NO rb3-Wii oracle is available. **DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit). Stub detector: ~/tmp/recon/common.py. This step is NON-Ghidra (analyzes the query results) — do NOT open Ghidra projects (the ghidra-export agent owns that). mkdir -p ${OUT}.

PREREQ: ${OUT}/bsim_query.json must exist. If it's a blocker (RB3Xenon locked, no query), set ok=false, calibrated=false, blockers=['bsim query unavailable'] and STOP.

STEPS:
1. CALIBRATE on the 25 known pins: for each, do the BSim hits for fns inside its span correctly name that TU's .cpp (and reject the coverage stubs)? Compute precision/recall vs the byte-oracle baseline (which had ~0 self-agreement). This is the GATE — measure whether cross-arch (Wii-PPC vs X360-VMX128) BSim is trustworthy.
2. If BSim calibrates well: for the UNPINNED scattered game TUs, group BSim-named hits by oracle src-file, take the densest CONTIGUOUS run (sliding window), exclude stubs, corroborate with call-cohesion -> candidate [start,end) spans -> ${OUT}/bsim_spans.json = [{unit,start,end,confidence,n_fns}].
3. If BSim does NOT calibrate (cross-arch p-code too divergent), report calibrated=false with the measured precision/recall (a valid, important negative).
Return LOC_SCHEMA (stream='bsim').`,
  { label: 'bsim-locate', phase: 'Locate', schema: LOC_SCHEMA })

const [bd, bsim] = await parallel([bindiffLocate, bsimLocate])

phase('CrossValidate')

let xval = null
if (bd && bd.ok && bd.calibrated && bsim && bsim.ok && bsim.calibrated) {
  xval = await agent(`Cross-validate the two game-ID streams into high-confidence candidate game-TU pins. BinDiff spans: ${bd.spans_path}. BSim spans: ${bsim.spans_path}. Calibration ground truth: ${CALIB}. NEVER touch main.
STEPS: (1) Load both span sets. (2) INTERSECT: a candidate TU span is HIGH-confidence iff BOTH streams independently propose it with overlapping/agreeing boundaries (snap to the agreed fn boundaries). (3) Also list MEDIUM-confidence (one stream only, high internal confidence). (4) Sanity vs the 627 existing pins (no overlap) + the 25 known pins (both streams should re-find them = a final validation). (5) Emit ${OUT}/crossval_spans.json = ranked [{unit,start,end,confidence,streams,n_fns}] ready to add to splits.txt. Report how many HIGH-confidence game TUs/fns are newly locatable.
Return LOC_SCHEMA (stream='crossval').`,
    { label: 'crossval', phase: 'CrossValidate', schema: LOC_SCHEMA })
} else {
  log(`CrossValidate skipped: bindiff.ok=${bd&&bd.ok}/calib=${bd&&bd.calibrated}, bsim.ok=${bsim&&bsim.ok}/calib=${bsim&&bsim.calibrated}`)
}

return { build, ghidra, bindiff: bd, bsim, crossval: xval }
