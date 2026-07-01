export const meta = {
  name: 'classa-ports-w3',
  description: 'Port-only stage for the 6 pre-validated class-A OWN candidates (NetworkEmulator/BandUserMgr/TrackerDisplay/StoreInfoPanel/TrackDir/MetaPanel). Validations already done (run wf_a3609f90-ac5); this skips Scan+Validate to dodge the rate-limit storm and just ports+pins, batched 2-at-a-time (CPU/API-modest). Honest composed A/B + ICF audit per port.',
  phases: [
    { title: 'Port', detail: 'port-then-pin each validated OWN span in its own worktree, batched 2-at-a-time; commit honest winners to branches' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
const SOP = `${REPO}/docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`

// Pre-validated OWN candidates (from run wf_a3609f90-ac5 validate stage). Confidence-ordered;
// MetaPanel last (god-object, ~100-header closure = the real guard).
const CANDS = [
  { name:'NetworkEmulator', tu:'network/net/NetworkEmulator.cpp', wii:'/home/free/code/milohax/rb3/src/network/net/NetworkEmulator.cpp', start:'0x823D8F00', end:'0x823D97F8', conf:0.90,
    hint:'11 contiguous fns; src/network/net/NetworkEmulator.h + all includes already present. Handle@0x823D9530 dispatches 6 method bodies at 0x823D8F00..0x823D91A0 (Enable/Disable/SetBandwidth/SetJitter/SetLatency/SetPacketDropProbability) — pin them together. Only nontrivial body = ctor@0x823D9288 inlining a Quazal InstanceControl context block (mInDevice=ptr+0x4ac/mOutDevice=ptr+0x490). VERIFY member offsets vs the CURRENT Object base (commit 44fae9c reconstructed Hmx::Object — header 0x28 comment may be stale).' },
  { name:'BandUserMgr', tu:'band3/game/BandUserMgr.cpp', wii:'/home/free/code/milohax/rb3/src/band3/game/BandUserMgr.cpp', start:'0x826660E0', end:'0x82666D48', conf:0.86,
    hint:'Handle@0x826660E0 (set_slot) + ctor/dtor (profile_pre_delete_msg/signin_changed) + own EH funclets + ForEachUser flag-thunks. 547 lines. Ownership demonstrably extends DOWN to ~0x82664fb0 (ForEachUser 832B + flag thunks Handle calls) — consider widening the start if the whole gap [0x826648AC,0x82666EF0) ports clean (GemManager-style own-funclet reproduction), but UNDER-claim if uncertain.' },
  { name:'TrackerDisplay', tu:'band3/game/TrackerDisplay.cpp', wii:'/home/free/code/milohax/rb3/src/band3/game/TrackerDisplay.cpp', start:'0x826B3268', end:'0x826B4B30', conf:0.82,
    hint:'SetPercentageProgress (set_progress/tracker_percentage) core + MsToMinutesSeconds + ctor (tracker vtable) + SendMsg helpers. 298 lines, header src/band3/game/TrackerDisplay.h complete, 8 includes resolve. Conservative under-claim span; the upper tracker tail (set_display_style ~0x826B4E80+) is deliberately excluded.' },
  { name:'StoreInfoPanel', tu:'band3/meta_band/StoreInfoPanel.cpp', wii:'/home/free/code/milohax/rb3/src/band3/meta_band/StoreInfoPanel.cpp', start:'0x8261C660', end:'0x8261E020', conf:0.82,
    hint:'Recommendations panel (?pid=%u / recommendations_ready / fetch_recommendations). 178 lines, header byte-identical to rb3-Wii, 12 includes present. If the tail [0x8261D930,0x8261E020) regresses, fall back to the safe string-bracketed core [0x8261C660,0x8261D930).' },
  { name:'TrackDir', tu:'system/track/TrackDir.cpp', wii:'/home/free/code/milohax/rb3/src/system/track/TrackDir.cpp', start:'0x827B89B0', end:'0x827BB078', conf:0.82,
    hint:'SyncObjects/ClearAllGemWidgets/BEGIN_HANDLERS/BEGIN_PROPSYNCS. 101/102 own. The +N depends on ~60 small PROPSYNC/HANDLE dispatch funclets (0x28/0x2C/0x30) byte-reproducing from MSVC macro expansion — if MSVC orders/folds them differently than retail, yield drops (this is the port-stage risk). 565 lines; copy 3 missing includes from rb3-Wii (track/TrackTest.h [MILO_DEBUG only], obj/ObjVersion.h, utl/ClassSymbols.h). header src/system/track/TrackDir.h present. DC3 lacks track/ — rb3-Wii is sole oracle.' },
  { name:'MetaPanel', tu:'band3/meta_band/MetaPanel.cpp', wii:'/home/free/code/milohax/rb3/src/band3/meta_band/MetaPanel.cpp', start:'0x825595F8', end:'0x8255DE88', conf:0.62,
    hint:'GOD-OBJECT panel, 215 fns in span, ~100-header dependency closure = the REAL guard (compile cost). header src/band3/meta_band/MetaPanel.h present. The lone missing include is meta/MemcardMgr_Wii.h (TheMemcardMgr.Init() ~line 284) — replace with the Xbox equivalent src/system/meta/MemcardMgr.h. The span deliberately starts AFTER a mid-gap Meta.cpp pin (0x825595A0-0x825595F8) — do NOT cross it. If the include closure is intractable in a reasonable effort, report DEFER honestly rather than forcing it.' },
]

const PORT_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['name','branch','baseline_matched','after_matched','real_net_delta','honest','icf_clean','blocker','notes'],
  properties: { name:{type:'string'}, branch:{type:'string'}, baseline_matched:{type:'integer'}, after_matched:{type:'integer'}, real_net_delta:{type:'integer'}, honest:{type:'boolean'}, icf_clean:{type:'boolean'}, blocker:{type:'string'}, notes:{type:'string'} },
}

function portPrompt(c) {
  return `Port-then-pin ${c.tu} (class-A TU-pure harvest wave-3, rb3-xenon). This TU was ALREADY VALIDATED OWN (confidence ${c.conf}); the validated span is [${c.start}, ${c.end}). Validator hint: ${c.hint}
rb3-Wii source: ${c.wii} -> dst src/${c.tu}. Recipe = GemManager/AppLabel +35/+52 (port-then-pin; the TU's own STL/funclet bodies byte-reproduce even while anonymous). Full detail: ${SOP} + ${REPO}/CLAUDE.md.

HARD RULES: buildable worktree ONLY (NEVER the main tree); do NOT land to main — commit to your branch + RETURN it. Concurrent agents are active; do NOT touch /tmp/wt-dc3drain, /tmp/wt-dc3naming, /tmp/wt-rndmat, or worktrees you did not create.

STEPS:
1. \`cd ${REPO} && scripts/setup_worktree.sh /tmp/wt-cA3-${c.name} cA3-${c.name}\`; work there. Fresh-worktree friction: if a build re-downloads compilers (SSL fail in sandbox), add an early skip-guard to the worktree's tools/download_tool.py (\`if output.exists() and (not output.is_dir() or any(output.iterdir())): print("skip"); return\` before the Downloading print) and \`ln -sf ${REPO}/build/tools/wibo build/tools/wibo\`.
2. BASELINE: rm -f build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked once; record baseline_matched from build/45410914/report.json. Save a copy of the baseline report for the icf audit.
3. PORT ${c.wii} -> src/${c.tu} (MWCC->MSVC X360: a wired sibling TU in the same dir is the template; .mStr->.Str(), decomp.h macros are MSVC no-ops, fix include paths, copy any missing by-value-member headers from the oracle). Add \`"${c.tu}":"NonMatching"\` to config/45410914/objects.json.
4. PIN \`.text start:${c.start} end:${c.end}\` under a \`${c.tu.split('/').pop()}:\` header in config/45410914/splits.txt (use just the basename header like the existing entries). \`python3 scripts/harvest/overlap_check.py config/45410914/splits.txt --text-only\` MUST report 0 overlaps (the span sits in a verified-unpinned gap; if overlap, your start/end is wrong — fix to fn boundaries strictly inside the gap).
5. \`venv/bin/python3 tools/gen_game_target_map.py --tu ${c.tu.split('/').pop()}\` (ADD-ONLY; NEVER --apply on the whole map) + reveal.
6. \`python3 configure.py\`; rebuild (rm stamp + touch config.yml + ./tools/ninja-locked once); after_matched from report.json; real_net_delta = after - baseline. Debug compile blockers (the whole TU must compile + define its methods) or report the blocker honestly.
7. AUDIT (HARD): \`python3 tools/icf_alias_check.py --worktree /tmp/wt-cA3-${c.name} --baseline-report <your baseline copy>\` (exit 1 = ICF-stub-fold inflation). honest=true ONLY if real_net_delta>0 AND icf_clean AND the newly-100 fns are real-bodied (>44B) ${c.name} methods, NOT <=44B stub-folds. A clean +0 (compiles but body-diverges) is a VALID honest outcome — report the blocker.
8. If honest win: commit to branch cA3-${c.name} (end the message with: Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>). Do NOT merge to main.

Return PORT_SCHEMA. If you hit a transient API rate-limit, that's the ongoing server storm — note it in blocker so the coordinator can retry just your TU.`
}

phase('Port')
// batch 2-at-a-time to cap concurrent worktree builds + API burst at 2 (CPU/storm-kind)
const results = []
for (let i = 0; i < CANDS.length; i += 2) {
  const batch = CANDS.slice(i, i + 2)
  log(`Batch ${i/2+1}: ${batch.map(c=>c.name).join(' + ')}`)
  const r = await parallel(batch.map((c) => () =>
    agent(portPrompt(c), { label: `port:${c.name}`, phase: 'Port', schema: PORT_SCHEMA })
  ))
  r.filter(Boolean).forEach((x) => results.push(x))
}

const landable = results.filter(p => p.honest && p.icf_clean && p.real_net_delta > 0)
const failed = results.filter(p => !(p.honest && p.icf_clean && p.real_net_delta > 0))
results.forEach(p => log(`${p.name}: delta=${p.real_net_delta} honest=${p.honest} icf_clean=${p.icf_clean} ${p.blocker || ''}`))
log(`PORTS: ${landable.length} honest winners, +${landable.reduce((s,p)=>s+p.real_net_delta,0)} — ${landable.map(p=>`${p.name}(+${p.real_net_delta})`).join(', ') || '(none)'}`)
return {
  landable: landable.map(p => ({ name: p.name, branch: p.branch, delta: p.real_net_delta })),
  deferred: failed.map(p => ({ name: p.name, blocker: p.blocker, delta: p.real_net_delta })),
  recommendation: 'Coordinator: rebase each honest winner branch onto latest main, composed-verify (run1==run2), land via scripts/harvest/land.sh, re-check whole-binary A/B for 0 regressions. Retry any TU whose blocker = rate-limit storm.',
}
