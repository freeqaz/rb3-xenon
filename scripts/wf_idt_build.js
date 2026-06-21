export const meta = {
  name: 'idtransfer-build',
  description: 'Build the identity-transfer pipeline tooling: B1 field_offset_gate + --pin-only (keystone), then B3 driver + overlap_check',
  phases: [
    { title: 'B1', detail: 'field_offset_gate + identity_transfer --pin-only; validate against RockCentral 17' },
    { title: 'B3', detail: 'idtransfer_harvest.py driver + overlap_check.py (gated on B1 passing)' },
  ],
}

const ROOT = '/home/free/code/milohax/rb3-xenon'
const GROUND = `Repo: ${ROOT}. The master design is docs/decomp/identity-transfer/PIPELINE-DESIGN.md — READ IT FIRST (esp. the section relevant to your task), plus the cited research docs in docs/decomp/identity-transfer/research/. Existing tools to reuse/extend: tools/identity_transfer.py (701L), tools/locator.py (722L), tools/gen_game_target_map.py (473L), tools/icf_alias_check.py, scripts/setup_worktree.sh, tools/fresh_report.sh, scripts/harvest/land.sh. Oracle data: unified_id_rb3wii.json. Wii asm for the static field scan: ../rb3/build/SZBE69_B8/asm. CRITICAL CONSTRAINTS from the design: STRICT add-only target_symbol_map (never gen_game_target_map.py --apply on a scattered TU = POISON); preserve the span-pin HARD GATE / FIX-1 collision drop / boundary-snap in identity_transfer.py; sim>=0.5 is DROPPED as a predictor (byte-equality + own-TU basename are the gates). Write clean, documented Python matching the existing tools' style. Do NOT commit. Do NOT run a full ninja build (B1 validation is static; the driver is built but not executed end-to-end here). You MAY write tool files into tools/ and scripts/ (additive). Return a precise report of files created/modified + how you validated.`

const B1_SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    built: { type: 'boolean' },
    files: { type: 'array', items: { type: 'string' } },
    validation_pass: { type: 'boolean' },
    rockcentral_retained: { type: 'number' },
    rockcentral_landed_total: { type: 'number' },
    excluded_landed: { type: 'array', items: { type: 'string' } },
    how_validated: { type: 'string' },
    notes: { type: 'string' },
  },
  required: ['built', 'validation_pass', 'files', 'how_validated', 'notes'],
}

phase('B1')
const b1 = await agent(
  `${GROUND}

## TASK B1 (the keystone — build with care)
Implement, per PIPELINE-DESIGN.md §3 Phase 5, §9 B1, §10 gate 5, and research/04-sourceport-bottleneck.md:

1. **tools/field_offset_gate.py** — a static analyzer. \`field_offset_gate(TU, D)\` where D = first member offset whose retail layout diverges from Wii (default: first embedded/array-heavy member; flat-struct TU => D=infinity = nothing poisoned). It scans each of the TU's Wii method bodies in ../rb3/build/SZBE69_B8/asm for any \`this\`-relative load/store (\`lwz/lwzx/lfs/lfd/stw/stfs ... off(rN)\` where rN holds \`this\`) with off >= D, tagging that method POISONED-TAIL. Reuse locator.py's asm-walk primitives where possible (read locator.py to find them; import or mirror, don't duplicate sloppily). Output the clean pin-set = methods that are real(>44B body) AND not MISATTRIBUTED AND not WALL/::Handle AND not POISONED-TAIL. Provide a CLI (\`--tu X [--D 0xNNN] [--oracle unified_id_rb3wii.json]\`) that prints the pin-set + the excluded set with reasons. Make D inference best-effort with a clear default and a manual override.

2. **identity_transfer.py --pin-only <list>** — add a flag so a partial port pins ONLY an explicit subset of methods (comma-separated VAs or a path to a file/JSON list from field_offset_gate). It must compose with the existing case-A/SELF/case-B classification, the span-pin HARD GATE, FIX-1 collision drop, boundary-snap, and STRICT add-only map. Additive change; do not regress existing behavior.

3. **VALIDATE (hard gate):** RockCentral.cpp landed +17 (its case-A pin set is in config/45410914/splits.txt under RockCentral.cpp + the matching target_symbol_map entries; the 17 are the methods that reached 100%). Run field_offset_gate on RockCentral and CONFIRM it does NOT exclude any of the 17 landed-100% methods (the gate must retain every proven win). Report rockcentral_retained / rockcentral_landed_total and list any excluded_landed (must be empty to pass). Determine the 17 landed methods from the report.json (default/RockCentral functions at 100% whose VA is a micro-pin) or the identity-transfer commit — explain your method in how_validated.

Return the schema. validation_pass=true ONLY if zero landed-100% RockCentral methods are excluded by the gate.`,
  { label: 'B1:field-gate+pin-only', phase: 'B1', schema: B1_SCHEMA }
)

log(`B1: built=${b1?.built} validation_pass=${b1?.validation_pass} retained=${b1?.rockcentral_retained}/${b1?.rockcentral_landed_total}`)

let b3 = null
if (b1 && b1.built && b1.validation_pass) {
  phase('B3')
  b3 = await agent(
    `${GROUND}

## TASK B3 (the driver — depends on B1, which is now built + RockCentral-validated)
B1 just landed tools/field_offset_gate.py + identity_transfer.py --pin-only (validated: it retains all RockCentral landed pins). Now build the orchestration per PIPELINE-DESIGN.md §2 (architecture diagram), §3 (Phases 1-10), §9 B3, §10 (the 10 hard-fail gates):

1. **scripts/harvest/overlap_check.py** — lift the splits-overlap check (currently prose in scripts/harvest/README.md) into a callable script: given the worktree splits.txt, ABORT (exit 1) on ANY two .text ranges overlapping. Importable by both the driver and scripts/harvest/land.sh.

2. **scripts/idtransfer_harvest.py** — the driver. Chains Phases 1-10 for one TU in a CoW worktree (scripts/setup_worktree.sh; NEVER mutate main — CLAUDE.md hard rule): PREFLIGHT (wired? obj exists? fingerprints fresh?) -> WORKTREE+BASELINE (fresh_report.sh, record matched_functions) -> IDENTIFY (identity_transfer classify) -> LOCATE (locator --emit-gate, SKIP list only) -> FIELD-GATE (field_offset_gate -> pin-set) -> MICRO-PIN+MAP (identity_transfer --pin-only --apply, STRICT add-only) -> OVERLAP (overlap_check, abort on overlap) -> BUILD+MEASURE (rm target_symbol_renames.stamp; touch config.yml; fresh_report.sh; delta vs baseline) -> AUDIT (icf_alias_check.py --worktree, abort on stub-fold) -> VERDICT (\`LANDABLE:+N\` or \`DEFER:<reason>\`). Every gate is a HARD fail. Take \`--tu X\`, optional \`--D\`, \`--dry-run\` (stop before MICRO-PIN+MAP and print the planned pin-set). Do NOT auto-land; emit the verdict for a human/land.sh.

Make it robust and well-logged (each phase prints a banner + result). Do NOT execute a full harvest run here (no build) — just build the driver and do a \`--dry-run\`-style smoke test if cheap (e.g. argument parsing + the dry-run plan for one TU that needs no build). Return a report of files + the smoke-test result.`,
    { label: 'B3:driver+overlap', phase: 'B3' }
  )
}

return {
  b1: b1 ? { built: b1.built, validation_pass: b1.validation_pass, files: b1.files, retained: `${b1.rockcentral_retained}/${b1.rockcentral_landed_total}`, excluded_landed: b1.excluded_landed, notes: b1.notes } : null,
  b3_built: !!b3,
  b3_report: b3 || (b1 && !b1.validation_pass ? 'SKIPPED: B1 validation failed' : 'SKIPPED: B1 not built'),
}
