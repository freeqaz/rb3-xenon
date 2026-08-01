export const meta = {
  name: 'gameport1',
  description: 'Wire + port fresh UNWIRED band3 game TUs (rb3-Wii oracle, tight high-conf spans) to objdiff matches. Each agent: scaffold source, pin refined span, gen map, port MWCC->MSVC, match, measure whole-binary net. Returns additions for the orchestrator to integrate onto main. NEVER commits to main.',
  phases: [ { title: 'GamePort', detail: 'one agent per unwired band3 TU' } ],
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

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║ ⛔ lane BX-4 (2026-07-30): EVERY SPAN BELOW IS DEAD. DO NOT PIN AS-IS.   ║
// ╚══════════════════════════════════════════════════════════════════════════╝
// These start/end pairs were derived from tools/game_splits.py, which reads the
// TU0-era unified_id_rb3wii.json. Main flipped TU0->TU5 on 2026-07-15 and .text
// was re-laid-out, so the addresses no longer denote anything. MEASURED against
// the 69,209 .text function starts in config/45410914/symbols.txt:
//   * 0 of 16 `start` values is a real function boundary (dtk will error
//     "ends within symbol", or silently carve a wrong unit);
//   * 5 of 16 spans contain ZERO live functions at all;
//   * the `conf: 0.99` figures are confidences in a dead pairing — they are
//     the most misleading part of this table, not a reason to trust it.
// They are retained ONLY as a record of which TUs were once candidates. The span
// numbers themselves must be re-derived from a LIVE source before any pin.
// Verify for yourself: python3 tools/dead_index_guard.py --audit
// Refined tight spans from tools/game_splits.py manifest (boundary-aligned, conf>=0.88, src present in rb3-Wii)
const UNITS = [
  { rel: 'band3/game/VocalPart.cpp',          start: '0x824203FC', end: '0x8242057C', prim: 7, conf: 0.99 },
  { rel: 'band3/game/TrainerPanel.cpp',       start: '0x8241B6D0', end: '0x8241B7E8', prim: 5, conf: 0.99 },
  { rel: 'band3/tour/TourDesc.cpp',           start: '0x8256D788', end: '0x8256D828', prim: 5, conf: 0.98 },
  { rel: 'band3/meta_band/ViewSetting.cpp',   start: '0x82553344', end: '0x825533E4', prim: 4, conf: 0.97 },
  { rel: 'band3/meta_band/Matchmaker.cpp',    start: '0x824ED900', end: '0x824ED980', prim: 4, conf: 0.88 },
  { rel: 'band3/meta_band/CharData.cpp',      start: '0x824CBBC8', end: '0x824CBC48', prim: 3, conf: 0.97 },
  { rel: 'band3/game/RealGuitarGemPlayer.cpp',start: '0x823AC9F0', end: '0x823ACA78', prim: 3, conf: 0.98 },
  { rel: 'band3/game/BandUserMgr.cpp',        start: '0x82398108', end: '0x82398168', prim: 3, conf: 0.97 },
]

phase('GamePort')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    rel: { type: 'string' },
    span_valid: { type: 'boolean' },          // did dtk split the span into a sane target obj with the expected fns?
    matched_fns: { type: 'array', items: { type: 'string' } },
    net_delta: { type: 'integer' },           // whole-binary matched_functions delta vs baseline
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    worktree_path: { type: 'string' },
    scaffolded_files: { type: 'array', items: { type: 'string' } },   // repo-rel paths of new .cpp/.h to copy to main
    objects_entry: { type: 'string' },        // the exact objects.json band3 line: "band3/...cpp": "NonMatching",
    splits_block: { type: 'string' },         // the exact splits.txt block (refined span, possibly snapped)
    map_entries: { type: 'object', additionalProperties: { type: 'string' } },  // addr -> mangled name for target_symbol_map.json
    landable: { type: 'boolean' },
    notes: { type: 'string' },
  },
  required: ['rel','span_valid','matched_fns','net_delta','worktree_path','landable','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Wire + port the UNWIRED RB3 game TU "${U.rel}" into an objdiff match in rb3-xenon (MSVC-X360 PPC). It is NOT yet in our build; rb3-Wii has the source oracle. Work ENTIRELY in an isolated worktree — NEVER edit/commit/push the main repo.

TARGET SPAN (refined, boundary-aligned, conf ${U.conf}, ~${U.prim} primary fns) from tools/game_splits.py:
    ${U.rel}:
        .text       start:${U.start} end:${U.end}

STEPS:
0. ⛔ SPAN IS DEAD — VALIDATE BEFORE YOU PIN. The start/end handed to you below came from the TU0-era rb3-Wii oracle and is provably stale (0 of 16 such starts is a real function boundary; the conf figure is a confidence in a DEAD pairing). Before doing anything else, check the start against the live universe:\n   grep -i ':0x<START>;' config/45410914/symbols.txt   # must show a type:function line\nIf the start is NOT a real .text function start, set span_valid=false, landable=false, and STOP — report it as a dead span. Do NOT snap/nudge it into place and do NOT re-derive it from unified_id_rb3wii.json (that file is informationless: 4.27% live vs ~2-3% by chance, and no rebase fixes it). Deriving a pin from it produces a plausible-looking WRONG unit. Verify: python3 tools/dead_index_guard.py --audit\n1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/gp-${U.rel.split('/').pop().replace('.cpp','')} gp-${U.rel.split('/').pop().replace('.cpp','')} ; work inside it. (If configure/dtk trips in the worktree, re-run configure.py with explicit absolute --dtk ../jeff --objdiff ../objdiff --wrapper flags per memory project_worktree_dtk_trap. First just try ./tools/ninja-locked.)
2. RECORD BASELINE matched_functions: ./tools/ninja-locked then read build/45410914/report.json measures.matched_functions (should be ${BASELINE}).
3. SCAFFOLD: python3 tools/fingerprint_pipeline.py scaffold ${U.rel}  (copies rb3-Wii .cpp into src/). Then add to config/45410914/objects.json 'band3' group's "objects" dict: "${U.rel}": "NonMatching",  and add the SPAN ABOVE to config/45410914/splits.txt.
4. PIN + SPLIT: rm build/45410914/target_symbol_renames.stamp ; touch config/45410914/config.yml ; ./tools/ninja-locked 2>&1 | tee ~/tmp/gp-${U.rel.split('/').pop()}.log. dtk should emit build/45410914/asm/${U.rel.split('/').pop().replace('.cpp','')}.s + a target obj. VERIFY span_valid: the target .s/.obj contains the expected named functions (see report or asm). If dtk mis-nests / the span captures wrong fns, SNAP start/end to the nearest symbol boundaries (use the asm + nm of neighbors) and re-split. If the span is unsalvageable, set span_valid=false and STOP (report it — a bad span is a valid negative).
5. MAP: generate target_symbol_map.json entries for the TU's fn_<addr> -> MSVC-mangled names. Try python3 tools/gen_game_target_map.py  [NOTE: this tool reads the DEAD rb3-Wii oracle and now HARD-FAILS by design — expect it to refuse, and hand-map instead. **DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit).] (rb3-Wii oracle); else hand-map from the asm fn addresses to the demangled names listed in the span. Without map entries the renamer can't pair target<->base and they read 0%. rm stamp + touch config.yml + rebuild after map edits.
6. PORT the source MWCC->MSVC until it COMPILES (NonMatching ok). Copy any missing headers from rb3-Wii (../rb3/src/${U.rel.replace('.cpp','.h')} and deps) into src/ as needed. MWCC->MSVC porting notes: rev-system bs.PushRev/PopRev, Color32/value-ref TheRnd, 2-arg ObjectDir::FindObject (NOT 3-arg), MILO_ASSERT/MILO_FAIL are macro-gated already (src/system/os/Debug.h), non-void empty virtuals need a return. Mirror the include style of the 63 already-wired band3 TUs.
7. MATCH: objdiff each function. EXPECT BIMODAL yield: real-logic fns (Handle, Find*, vector ops, scoring) match ~95-100%; pure trivial getters may be RETAIL-STUBBED coverage breadcrumbs (oracle mirage — our ported accessor will NOT match a stripped stub; do NOT chase those, note them). Port/iterate the real-body ones to 100%. reveal_sweep+gate+merge byte-exact ones into the map.
8. MEASURE whole-binary NET = after - ${BASELINE}. List regressions (a TU wiring shouldn't regress others; if it does, your span overlaps a pinned TU — fix the span).
9. Copy final artifacts so the orchestrator can integrate onto main: leave them in the worktree AND record in the schema: scaffolded_files (repo-rel paths of every new .cpp/.h you added), objects_entry (the exact line), splits_block (the exact final splits.txt block), map_entries (addr->name dict you added).

Return the schema. landable = net_delta>0 AND span_valid AND no real regressions. Leave the worktree in place (do not remove). Honest measured net; if everything in the span is a retail stub (net 0), report span_valid + net 0 + that the TU is stub-mirage (valid result). NEVER commit to main / push.`,
    { label: `gp:${U.rel.split('/').pop()}`, phase: 'GamePort', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
