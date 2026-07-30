export const meta = {
  name: 'xenon-finalize-wave2',
  description: 'Finalize 3 checkpoint-committed rb3-Xenon ports (TrackWatcherImpl/SongParser/BeatMatchController): build TU obj, objdiff-cli-verify targets, pin true-100, keep fuzzy source',
  phases: [{ title: 'Finalize2', detail: 'per TU: build own obj only, objdiff-cli each target addr, pin 100% matches, commit; coordinator runs final composed regression check' }],
}
const REPO = '/home/free/code/milohax/rb3-xenon'
const RESULT_SCHEMA = {
  type:'object', additionalProperties:false,
  required:['tu','committed','strict_pins','icf_verdict','notes'],
  properties:{
    tu:{type:'string'}, branch:{type:'string'}, committed:{type:'boolean'}, commit_sha:{type:'string'}, worktree:{type:'string'},
    strict_pins:{type:'array',items:{type:'object',properties:{fn:{type:'string'},addr:{type:'string'},size:{type:'string'},pct:{type:'number'}}}},
    fuzzy_kept:{type:'array',items:{type:'object',properties:{fn:{type:'string'},addr:{type:'string'},pct:{type:'number'}}}},
    dropped:{type:'array',items:{type:'object',properties:{fn:{type:'string'},addr:{type:'string'},reason:{type:'string'}}}},
    compiles:{type:'boolean'}, icf_verdict:{type:'string',enum:['HONEST','INFLATED','N/A']}, handoff_doc:{type:'string'}, notes:{type:'string'},
  },
}
const ITEMS = [
  { tu:'TrackWatcherImpl', wt:'.claude/worktrees/wt-trackwatcherimpl2', branch:'wt-trackwatcherimpl2', src:'src/system/beatmatch/TrackWatcherImpl.cpp',
    note:'Checkpoint-committed with ~72 functions ported (nearly the whole TU) but 0 map pins yet. HIGH VALUE: even beyond the 11 targets, the whole ported TU is recovered NonMatching source. 11 targets: 0x82771cb8=CheckForAutoplay 0x82771328=OnHit 0x8276fd08=CheckForCodaLanes 0x82770428=SendHit 0x827700f8=InSlopWindow 0x827714f8=OnMiss 0x8276fbb0=RecalcGemList 0x82770900=SendWhammy 0x827720d8=KillSustainForSlot 0x827704e8=SendMiss 0x8276fd78=EndSustainedNote' },
  { tu:'SongParser', wt:'.claude/worktrees/wt-songparser', branch:'wt-songparser', src:'src/system/beatmatch/SongParser.cpp',
    note:'Checkpoint-committed (3 target members) but NEVER COMPILED (prior pass was CPU-starved). First confirm it compiles. 3 targets: 0x8275dfd0=GetNoStrumState 0x8275f2c8=CheckDrumFillMarker 0x8275f8b8=IsPartTrackName' },
  { tu:'BeatMatchController', wt:'.claude/worktrees/wt-beatmatchcontroller', branch:'wt-beatmatchcontroller', src:'src/system/beatmatch/BeatMatchController.cpp',
    note:'Checkpoint-committed (5 functions) but 0 map pins. 4 targets: 0x8276b388=ButtonToSlot 0x82675148=RegisterHit 0x8276ade8=RegisterRGStrum 0x8276ae60=IsOurPadNum' },
]
function prompt(it){
  return `You are a decomp FINALIZE agent for **rb3-xenon** (RB3 Xbox360, MSVC PowerPC-Xenon). **${it.tu}** is already PORTED and checkpoint-committed in an isolated worktree. Finalize it: make it compile, verify which target functions byte-match, pin the true-100 ones, KEEP the fuzzy source. Work ONLY in the worktree; commit to its branch; coordinator lands.

## WORKTREE (port on disk, checkpoint-committed)
\`${REPO}/${it.wt}\` — branch \`${it.branch}\`. Target file: \`${it.src}\`.
${it.note}

## POLICY (owner directive: PARTIAL MATCHES COUNT)
Recovered faithful source is the goal; strict-100 is one proxy. So:
- KEEP the whole ported .cpp even if only some/none of the targets hit strict-100 — it rides in as NonMatching source (real progress + fuzzy%).
- PIN (add to scripts/target_symbol_map.json) an addr ONLY at true-100 byte-equal OR a confirmed high-confidence identity that is a codegen near-miss. NEVER pin a guessed/low-confidence identity fuzzy (false pairing = poison). When unsure, DON'T pin — just keep the source.

## VERIFICATION — use objdiff-cli DIRECTLY, do NOT run tools/fresh_report.sh
The whole-binary fresh_report.sh stalls / transiently aborts (wibo DLL-init) under shared machine load. Instead:
1. Build ONLY your TU's obj (objdiff-cli builds it via custom_make, or \`tools/ninja-locked build/45410914/<path-to-your>.obj\`). If it fails to COMPILE, fix minimally (missing include/type/MSVC idiom — study a sibling ported src/system/*.cpp). Iterate until it compiles clean. Report compiles:true/false.
2. For EACH target addr, verify match% via objdiff-cli directly (see how the BandPatchMesh finalize did it, ~27s/fn — the repo has build/tools/objdiff-cli; unit name derives from the TU). Read each target's match_percent / byte-equality.
3. For each TRUE-100 target: extract the MSVC-mangled symbol YOUR compiler emitted from the built .obj COFF symtab (llvm-nm/objdump or scripts/extract_decomp_symbols.py — NOT hand-guessed), add \`"0x<addr>":"<mangled>"\` to scripts/target_symbol_map.json (ADD-ONLY), set its splits .text range to exactly [VA, VA+size), and add a .pdata range ONLY if the fn has unwind info (check RUNTIME_FUNCTION BeginAddress in orig/45410914/band.exe; leaf getters usually have none). Record fuzzy targets (<100%) in fuzzy_kept with their pct (source stays, no pin).
4. ICF sanity on your strict pins: \`tools/icf_alias_check.py --no-oracle --tu ${it.tu}.cpp\` (or --worktree ${REPO}/${it.wt}); if a pinned addr is a ≤44B stub-fold with no real-bodied anchor, report INFLATED and drop that pin. ⚠ --no-oracle is REQUIRED because the rb3-Wii oracle is dead (**DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit).). In that DEGRADED mode the FOREIGN-attribution signal — the strongest inflation tell — is UNAVAILABLE, so this check can only ever FIND inflation, NEVER rule it out: a clean/HONEST result here is NOT a pass and must not be reported as one.
5. Splits overlap self-check (no .text/.pdata overlaps in your block).

## COMMIT
Amend/extend the commit on \`${it.branch}\` (stage ONLY files you changed: the .cpp, any new headers, objects.json, splits.txt, target_symbol_map.json — \`git add <path>\`, never -A/commit -a). Message: each strict pin (fn+size+pct), fuzzy kept (fn+pct), compiles y/n, ICF verdict. Write handoff \`docs/decomp/handoff/port-${it.tu.toLowerCase()}-handoff.md\`.

## HARD RULES
ONLY this worktree. No push / no merge to main / don't touch other worktrees. NEVER git stash/checkout <file>/restore/reset --hard. Stage only your files. target_symbol_map.json ADD-ONLY (never regenerate). Zero false pins. Use tools/ninja-locked not bare ninja. Don't kill services.

## RETURN structured: tu, branch="${it.branch}", committed(bool), commit_sha, worktree="${REPO}/${it.wt}", compiles(bool), strict_pins[{fn,addr,size,pct}], fuzzy_kept[{fn,addr,pct}], dropped[{fn,addr,reason}], icf_verdict, handoff_doc, notes(short). Compact.`
}
phase('Finalize2')
log(`Wave-2 finalize: ${ITEMS.length} checkpoint-committed ports (objdiff-cli-direct verify)`)
const results = await parallel(ITEMS.map(it => () =>
  agent(prompt(it), { label:`fin2:${it.tu}`, phase:'Finalize2', schema:RESULT_SCHEMA })
    .then(r => ({ ...(r||{}), tu: it.tu, _dead: !r }))
))
const ok = results.filter(r => r && !r._dead)
const landable = ok.filter(r => r.committed && r.compiles && r.icf_verdict !== 'INFLATED')
const strict = landable.reduce((s,r)=>s+((r.strict_pins||[]).length),0)
log(`Wave-2 done. ${ok.length}/${ITEMS.length} returned; ${landable.length} landable (compile-clean), ${strict} strict pins total`)
return {
  returned: ok.length,
  dead: results.filter(r=>!r||r._dead).map(r=>r&&r.tu).filter(Boolean),
  landable: landable.map(r=>({tu:r.tu,branch:r.branch,commit:r.commit_sha,strict:(r.strict_pins||[]).length,fuzzy:(r.fuzzy_kept||[]).length,icf:r.icf_verdict})),
  not_landable: ok.filter(r=>!landable.includes(r)).map(r=>({tu:r.tu,compiles:r.compiles,committed:r.committed,icf:r.icf_verdict,notes:r.notes})),
  total_strict_pins: strict,
  full: ok,
}
