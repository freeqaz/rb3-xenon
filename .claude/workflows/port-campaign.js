export const meta = {
  name: 'port-campaign',
  description: 'Port the src=NONE pinned RB3-specific TUs from the rb3-Wii oracle (MWCC->MSVC X360): BandCharacter, TrackPanelDir, BandCharDesc, BandWardrobe, GuitarController. Compile, reveal_sweep, match real-bodied fns, A/B. Never commits to main.',
  phases: [ { title: 'Port', detail: 'one agent per pinned-sourceless TU' } ],
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

const UNITS = [
  { key: 'TrackPanelDir', wii: 'system/bandobj/TrackPanelDir.cpp', n: 65 },
  { key: 'BandCharDesc', wii: 'system/bandobj/BandCharDesc.cpp', n: 39 },
  { key: 'BandWardrobe', wii: 'system/bandobj/BandWardrobe.cpp', n: 24 },
  { key: 'GuitarController', wii: 'system/game/GuitarController.cpp', n: 12 },
  { key: 'BandCharacter', wii: 'system/bandobj/BandCharacter.cpp', n: 244 },
]

const LESSONS = `
REUSABLE LESSONS (from the VocalTrackDir port that landed +29):
1. REV-SYSTEM is the first hurdle. Use the rb3-Wii literal idiom: in PreLoad do
   bs.PushRev(packRevs(gAltRev, gRev), this); in PostLoad do
   int revs=bs.PopRev(this); gRev=getHmxRev(revs); gAltRev=getAltRev(revs);
   PushRev/PopRev are BinStream members; getHmxRev/getAltRev/packRevs are in utl/BinStream.h.
   Our obj/ObjMacros.h single-arg INIT_REVS(Class)/DECLARE_REVS/LOAD_REVS->gRev/gAltRev wins
   when the include chain pulls ObjMacros.h after Object.h.
2. API divergences (DC3 tree vs rb3-Wii oracle), all clean fixes: Hmx::Color32 -> #include
   "math/Color32.h"; TheRnd is a value ref (use . not ->); ObjectDir::FindObject is 3-arg
   (name,parentDirs,subDirs=true); Symbol::mStr -> .Str(); BaseMaterial::mTexXfm -> TexXfm();
   RndTransformable::mLocalXfm -> LocalXfm()/DirtyLocalXfm(); RndText color API differs
   (add decl-only RB3-era methods to rndobj/Text.h like the existing convention).
3. reveal_sweep is the highest-EV step AFTER it compiles: tools/reveal_sweep.py (--include-static)
   -> tools/safe_name_merge.py --gate -> merge into scripts/target_symbol_map.json ->
   rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ninja.
   Keep only entries that land 100%. The gen_game_target_map oracle is DEAD and the tool now hard-fails (**DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit).); it also never covered system/bandobj,
   so reveal is the only auto-pairing path. Harvests the byte-exact subset for free.
4. Empty-body non-void virtuals from the rb3-Wii header need real returns (return this;) — MSVC C4716.
5. Shared-header additions must be codegen-neutral (decl-only methods / unused inlines). VERIFY by
   checking whole-binary net == in-unit net (no other unit moved).
6. Expect genuine oracle-divergence in game-logic load/config bodies (retail RB3 != rb3-Wii dev
   build) — those need per-fn RE; defer them. Match the clean ones + reveal-harvested ones.
`

phase('Port')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    key: { type: 'string' },
    compiled: { type: 'boolean' },
    matched: { type: 'integer', description: 'unit matched_functions achieved (from 0)' },
    net_delta: { type: 'integer', description: 'whole-binary' },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit','delta'] } },
    landable: { type: 'boolean' }, patch_path: { type: ['string','null'] }, files_changed: { type: 'array', items: { type: 'string' } },
    blocked: { type: 'string' }, notes: { type: 'string' },
  },
  required: ['key','compiled','matched','net_delta','landable','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Port the pinned-but-sourceless RB3 TU "${U.key}" into rb3-xenon (matching MSVC-X360 PPC). Report unit default/${U.key} has ~${U.n} functions, currently 0 matched because our src/ has NO ${U.key}.cpp (pinned in splits.txt for coverage, never sourced). Oracle: rb3-Wii /home/free/code/milohax/rb3/src/${U.wii} (MWCC PowerPC, named RB3 game code — needs Wii->360 porting). DC3 LACKS this TU (RB3-specific). Cross-check DC3 ../dc3-decomp/src only for engine BASE classes it inherits.

GOAL: get ${U.key}.cpp compiling under MSVC X360 (/O1 /Oi /GR /EHsc), wire it, and match as many real-bodied functions as you can. Getting it to COMPILE is the main hurdle — a compile-only result is still useful progress.

${LESSONS}

STEPS:
1. cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/pc-${U.key} pc-${U.key} ; work inside it.
2. Read the rb3-Wii ${U.wii} + its header, and OUR existing src header for ${U.key} (confirm the .h exists; it usually does since the class is referenced). grep config/45410914/objects.json + splits.txt for ${U.key} (it's pinned; you may need to add the .cpp to objects.json and path-qualify the split header if the basename is ambiguous).
3. Create src/${U.wii.replace('system/','system/')} (i.e. src/${U.wii}) by porting the rb3-Wii source; adapt per the LESSONS. configure.py if you edited objects.json. ITERATE to a clean compile (./tools/ninja-locked > ~/tmp/pc_${U.key}.log 2>&1).
4. reveal_sweep -> safe_name_merge -> merge map -> rebuild (harvests byte-exact). Then diff_inspect the remaining real-bodied near-misses and fix oracle-divergences; defer regalloc/funclet/vtable/ODR walls.
5. Whole-binary net vs baseline ${BASELINE} (worktree report.json vs ${REPO}/build/45410914/report.json). MUST be non-negative (verify shared-header edits are codegen-neutral).
6. cd worktree && mkdir -p ~/tmp/pc && git add -A && git diff HEAD > ~/tmp/pc/${U.key}.patch  (captures the NEW .cpp).
7. Return schema. landable = compiled AND net_delta>0 AND no unexplained regressions. NEVER commit to main / push. Leave worktree. Be honest about how far you got; ${U.key=='BandCharacter'?'BandCharacter is LARGE (244 fns) — getting it to compile + reveal-harvesting is a great result, do not expect all 244.':'aim for compile + reveal-harvest + the clean body matches.'}`,
    { label: `port:${U.key}`, phase: 'Port', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
