export const meta = {
  name: 'ultracode-levers',
  description: 'Concurrent multi-lever harvest: (A) wire+port fresh band3 game TUs from the rb3-Wii oracle, (B) the RndHighlightable -0xC shared-base force-multiplier, (C) the Gem Tail -0x14 struct fix. Each agent works an isolated buildable worktree, whole-binary A/B, returns a net-positive verified patch. NEVER commit to main.',
  phases: [
    { title: 'Levers', detail: 'all levers concurrent: 10 game-TU ports + base-class -0xC + Gem Tail -0x14, each in its own worktree, whole-binary A/B' },
  ],
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

const GAME_TUS = [
  'band3/game/GemPlayer.cpp',
  'band3/game/VocalPart.cpp',
  'band3/bandtrack/GemManager.cpp',
  'band3/game/BandUser.cpp',
  'band3/game/BandUserMgr.cpp',
  'band3/meta_band/BandSongMgr.cpp',
  'band3/game/Performer.cpp',
  'band3/meta_band/ProfileMgr.cpp',
  'band3/tour/TourProgress.cpp',
  'band3/bandtrack/GemRepTemplate.cpp',
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    lever: { type: 'string' },          // 'gameport' | 'baseclass-0xC' | 'gem-tail'
    target: { type: 'string' },         // rel path / class
    landable: { type: 'boolean' },
    net_delta: { type: 'integer' },
    matched_fns: { type: 'array', items: { type: 'string' } },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    files_changed: { type: 'array', items: { type: 'string' } },     // repo-rel paths to copy to main (new + edited)
    objects_entry: { type: ['string','null'] },                     // gameport: objects.json line
    splits_block: { type: ['string','null'] },                      // gameport: splits.txt block
    map_entries: { type: 'object', additionalProperties: { type: 'string' } },  // gameport: addr->mangled
    patch_path: { type: ['string','null'] },
    root_cause: { type: 'string' },
    notes: { type: 'string' },
  },
  required: ['lever','target','landable','net_delta','root_cause','notes'],
}

phase('Levers')

const WT = (slug) => `.claude/worktrees/uc-${slug}`
const SETUP = (slug) => `cd ${REPO} && scripts/setup_worktree.sh ${WT(slug)} uc-${slug} ; then work ENTIRELY inside ${WT(slug)} — NEVER edit/build/commit the main repo. First just ./tools/ninja-locked; if dtk/configure trips, re-run: python3 configure.py --dtk /home/free/code/milohax/jeff/target/release/dtk --objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli --wrapper /home/free/code/milohax/wibo/build/release/wibo (memory project_worktree_dtk_trap).`

const AB = `WHOLE-BINARY A/B: record baseline matched_functions (${BASELINE}) from build/45410914/report.json measures BEFORE the change; after editing, rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ./tools/ninja-locked, then re-read measures.matched_functions. net_delta = after - baseline. Diff the 100%-matched fn SETS (match_percent_normalized==100) to list real regressions. LANDABLE iff net_delta > 0 AND no net real regressions (ICF/renamer naming-noise pairing flips that wash to 0 don't count — be conservative).`

function gamePrompt(rel) {
  const slug = rel.split('/').pop().replace('.cpp','')
  return `Wire + port the UNWIRED RB3 game TU "${rel}" into objdiff matches (MSVC-X360 PPC). It is NOT yet compiled in our build; rb3-Wii has the source oracle. ${SETUP(slug)}

STEPS:
1. Record baseline matched_functions (${BASELINE}).
2. DERIVE SPAN + SCAFFOLD: run \`python3 tools/fingerprint_pipeline.py scaffold ${rel}\` — copies the rb3-Wii .cpp into src/ AND prints an objects.json entry + a splits.txt block (oracle target span). Add the "${rel}": "NonMatching" line to config/45410914/objects.json (band3 group) and the block to config/45410914/splits.txt. If scaffold gives no usable span, SET landable=false AND STOP for this TU. (Previously this step told you to derive the span from unified_id_rb3wii.json — that is PINNING FROM NOISE and is now forbidden; **DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit).) A span may only come from a LIVE source, and both ends must snap to real fn boundaries from the catch-all asm (else dtk errors "ends within symbol"). If no clean non-pin-overlapping contiguous cluster exists, set landable=false, span report it (a bad span is a valid negative).
3. SPLIT + MAP: rm build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked 2>&1 | tee ~/tmp/uc_${slug}.log. Confirm dtk emitted a target obj with the expected named fns. Generate target_symbol_map.json entries (python3 tools/gen_game_target_map.py  [NOTE: this tool reads the DEAD rb3-Wii oracle and now HARD-FAILS by design — expect it to refuse, and hand-map instead. **DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit).], or hand-map fn_<addr>->mangled from the oracle) — without them the renamer reads false-0%.
4. PORT MWCC->MSVC until it COMPILES (NonMatching ok). Copy missing headers from ../rb3/src as needed. Notes: rev-system bs.PushRev/PopRev; 2-arg ObjectDir::FindObject (NOT 3-arg); MILO_ASSERT/MILO_FAIL already macro-gated; non-void empty virtuals need a return; mirror the include style of already-wired band3 TUs.
5. MATCH: objdiff each fn. Real-logic fns port to ~95-100%; trivial getters may be RETAIL-STUBBED breadcrumbs (won't match a stripped stub — note, don't chase). reveal_sweep + safe_name_merge byte-exact ones.
6. ${AB}
7. mkdir -p ~/tmp/uc && cd ${WT(slug)} && git add -A && git diff --cached > ~/tmp/uc/${slug}.patch (captures new files + edits; empty if nothing landable). Record files_changed (every new/edited repo-rel path), objects_entry, splits_block, map_entries. Return SCHEMA with lever='gameport', target='${rel}'. DO NOT commit to main; leave worktree.`
}

const baseClassPrompt = `Investigate + fix the RndHighlightable -0xC shared-base layout deficit (MSVC-X360 PPC). EVIDENCE: Save@CharSleeve (99.7%) and Save@CharIKSliderMidi (99.4%) both show a UNIFORM -0xC (12-byte) shift on every \`this\`-relative member read (CharSleeve mInertia retail lfs f0,-0x28(r31) vs ours -0x1c; CharIKSliderMidi mTarget retail -0xa0 vs ours -0x94). Both derive from a \`public virtual Hmx::Object\` base (RndHighlightable / CharWeightable). Our base subobject is 0xC too SMALL vs retail. ${SETUP('baseclass')}

APPROACH (analysis-first, regression-gated — a shared base ripples across many units):
1. Record baseline matched_functions (${BASELINE}).
2. VERIFY with objdiff --include-instructions on Save@CharSleeve + Save@CharIKSliderMidi that the -0xC is uniform on REAL member reads (NOT funclet subi r31,r12 noise — that's tooling, not layout). Identify the EXACT base/vbptr/virtual-base region that is 0xC short. Cross-check the class layout vs DC3 (../dc3-decomp/src) + Ghidra (ghidra-struct / struct_db) — find what retail has that our header lacks (DC3-removed member, vbptr/virtual-base pad, ObjPtr width).
3. If a clean fix exists (add the missing 0xC in the shared base header), APPLY it.
4. ${AB} A 0xC base change WILL ripple. ONLY landable if net_delta>0 with no NET regressions. If it regresses >= it fixes (likely for a base change), REVERT and report the exact root cause as a negative for a future targeted fix.
5. mkdir -p ~/tmp/uc && cd ${WT('baseclass')} && git diff > ~/tmp/uc/baseclass.patch. files_changed = edited headers. Return SCHEMA (lever='baseclass-0xC', target='RndHighlightable'). NEVER commit to main; leave worktree.`

const gemTailPrompt = `Fix the Gem::AddRep near-miss via the Tail struct size (MSVC-X360 PPC). EVIDENCE: ?AddRep@Gem@@... is 99.99% — the SOLE residual is \`new Tail(repTemp)\` emitting \`li r3, 0x530\` (ours) vs target \`li r3, 0x544\` (retail): our Tail (src/band3/bandtrack/Tail.h) is 0x14 (20 bytes) too SMALL; every other instruction matches. ${SETUP('gemtail')}

APPROACH:
1. Record baseline matched_functions (${BASELINE}).
2. Read src/band3/bandtrack/Tail.h. Make sizeof(Tail)==0x544. Cross-check Tail's layout vs DC3 (../dc3-decomp/src) + rb3-Wii (../rb3/src) + Ghidra to find the missing 0x14 (likely a DC3-removed member or an embedded-object/base width). Place the addition where retail has it so other Tail/Gem field offsets stay correct — prefer a real named member from the oracle over a blind trailing pad (a blind pad is acceptable only if it provably doesn't shift any accessed member).
3. ${AB} Verify AddRep@Gem reads 100% and matched_functions strictly increased; watch for regressions in other Tail/Gem-using fns.
4. mkdir -p ~/tmp/uc && cd ${WT('gemtail')} && git diff > ~/tmp/uc/gemtail.patch. files_changed=['src/band3/bandtrack/Tail.h', ...]. Return SCHEMA (lever='gem-tail', target='Gem::AddRep'). NEVER commit to main; leave worktree.`

const thunks = [
  ...GAME_TUS.map(rel => () => agent(gamePrompt(rel), { label: `gameport:${rel.split('/').pop().replace('.cpp','')}`, phase: 'Levers', schema: SCHEMA })),
  () => agent(baseClassPrompt, { label: 'struct:baseclass-0xC', phase: 'Levers', schema: SCHEMA }),
  () => agent(gemTailPrompt,   { label: 'struct:gem-tail',      phase: 'Levers', schema: SCHEMA }),
]

const results = (await parallel(thunks)).filter(Boolean)
const landable = results.filter(r => r.landable && r.net_delta > 0)
log(`ultracode-levers done: ${landable.length}/${results.length} landable, total net ${landable.reduce((a,r)=>a+r.net_delta,0)}`)
return { results, landable }
