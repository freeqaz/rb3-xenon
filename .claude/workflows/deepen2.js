export const meta = {
  name: 'deepen2',
  description: 'Deepen round 2 (post Character-tail-fix): implement remaining stub functions + close near-misses in the ported RB3 game/bandobj TUs from rb3-Wii. Character base layout is now correct. Never commits to main.',
  phases: [ { title: 'Deepen', detail: 'one agent per TU' } ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
// ── lane BX-4 (2026-07-30): a hardcoded BASELINE is DEAD DATA ────────────────
// Every workflow in this dir carried a literal baseline (4,661–6,568) frozen at
// the date it was written, while main had moved to 41,170. An agent computing
// `net_delta = after - BASELINE` from that literal would report a fabricated
// +34,000. Baselines are MEASURED, never remembered — same rule as
// tools/dead_index_guard.py. Read measures.matched_functions from
// build/45410914/report.json in the leg you are actually measuring.
const BASELINE = 'MEASURE IT YOURSELF in your own worktree BEFORE your first edit: rm -f build/45410914/report.cache build/45410914/report.json, full build, then read measures.matched_functions. Do NOT use any number written in this prompt, and do NOT read the MAIN repo report.json — lanes land by patch without rebuilding main, so main artifact goes stale by hundreds of functions (measured 40,925 while main was 41,168). Cross-check against the headline in docs/plans/decomp-state-2026-07-19.md'

const UNITS = [
  { key: 'BandCharacter', wii: 'system/bandobj/BandCharacter.cpp', note: 'Character base layout is NOW FIXED (tail +0x20 drift removed) — previously-blocked stubs should now match when ported. Many of the ~131 remaining are UNIMPLEMENTED stubs (0 bytes) needing their bodies ported from rb3-Wii.' },
  { key: 'VocalTrackDir', wii: 'system/bandobj/VocalTrackDir.cpp', note: 'Re-check: prior deepen claimed a RndDir vtable +4 gate — VERIFY that against target asm (the BandCharacter "layout gate" claim turned out to be stubs, so be skeptical). Port unimplemented stubs / close real near-misses.' },
  { key: 'TrackPanelDir', wii: 'system/bandobj/TrackPanelDir.cpp', note: 'Port remaining stubs / close near-misses.' },
  { key: 'BandCharDesc', wii: 'system/bandobj/BandCharDesc.cpp', note: 'Port remaining stubs / close near-misses.' },
]

phase('Deepen')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' },
    ported: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { fn: { type: 'string' }, before: { type: 'number' }, after: { type: 'number' } }, required: ['fn','after'] } },
    net_delta: { type: 'integer' },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    landable: { type: 'boolean' }, patch_path: { type: ['string','null'] }, files_changed: { type: 'array', items: { type: 'string' } }, notes: { type: 'string' },
  },
  required: ['key','ported','net_delta','landable','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Deepen the ported RB3 TU "${U.key}" — implement remaining UNIMPLEMENTED STUB functions (our obj emits 0 bytes; port their bodies from rb3-Wii) and close real near-misses. ${U.note}

Oracle: rb3-Wii /home/free/code/milohax/rb3/src/${U.wii}. The TU is already sourced + compiling. Ghidra MCP is UP (port 8002) for decompiles. dtk asm build/45410914/asm/<unit>.s = offset ground truth.

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/d2-${U.key} d2-${U.key} ; work inside it.
2. Find ${U.key} functions below 100%. For UNIMPLEMENTED ones (our obj has 0 bytes / objdiff shows all-target-inserted): PORT the body from rb3-Wii (MWCC->MSVC: rev-system bs.PushRev/PopRev, Color32/TexXfm()/LocalXfm()/3-arg FindObject/value-ref TheRnd; non-void empty virtuals need return). For implemented near-misses: diff_inspect + fix oracle-divergence. After porting, reveal_sweep+gate+merge into the CURRENT map (extract+merge, not stale hunk) to harvest byte-exact.
3. DEFER: regalloc/scheduling, funclet (subi r31,r12), MSVC vtable-folding/devirt, ICF-clone template thunks (un-disambiguatable). If a real base-class layout/vtable gate blocks many, VERIFY it against asm before claiming it (don't repeat the false "layout gate" — it may just be unimplemented stubs).
4. Build (./tools/ninja-locked; for map: rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml first). Confirm 100% + no regressions.
5. Whole-binary net vs baseline ${BASELINE}.
6. cd worktree && git add -A && git diff HEAD > ~/tmp/d2/${U.key}.patch
7. Return schema. landable = net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Honest measured net; ${U.key=='BandCharacter'?'BandCharacter has ~131 left — port as many clean stubs as you can this session, do not chase all.':'port the clean subset.'}`,
    { label: `d2:${U.key}`, phase: 'Deepen', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
