export const meta = {
  name: 'classa-harvest-w3',
  description: 'Class-A TU-pure span harvest, wave-3: broaden the string-anchored-core scan past the harvested 9 winners + rejected set, validate span TU-purity, port-then-pin the OWN+compilable spans (GemManager/AppLabel +35/+52 recipe), honest composed A/B for landing',
  phases: [
    { title: 'Scan', detail: 'wider string-anchored-core scan (>=2-fn cores, all game dirs) excluding done/known-mixed' },
    { title: 'Validate', detail: 'per candidate: ownership purity + compile-feasibility (read-only)' },
    { title: 'Port', detail: 'port-then-pin OWN+compilable spans in isolated worktrees; composed A/B + ICF audit' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
const SOP = `${REPO}/docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`
// done (landed) OR proven-mixed/rejected across waves 1-3. NOTE: MetaPanel.cpp deliberately OMITTED
// (wave-3 deferred it on tmpfs-quota env failure, not a port outcome — it is the retry candidate).
const DONE_OR_MIXED = ['GemManager.cpp','AppLabel.cpp','Matchmaker.cpp','PitchArrow.cpp','OvershellSlot.cpp','TrackPanelDirBase.cpp','TrackPanel.cpp','GemPlayer.cpp','ChordbookPanel.cpp','FreestylePanel.cpp','RGTrainerPanel.cpp','TrainerPanel.cpp','BandwidthCounter.cpp','TournamentDDL.cpp','RockCentral.cpp','Defines.cpp','BandCrowdMeter.cpp','GameMicManager.cpp','PatchDir.cpp','GemTrackDir.cpp','ChordShapeGenerator.cpp','BandScoreboard.cpp','VocalTrainerPanel.cpp','ClosetMgr.cpp','MainHubPanel.cpp','OverdriveMeter.cpp','TrainerProgressMeter.cpp','TrainerGemTab.cpp','StreakMeter.cpp',
  // wave-3 (main @25ed686): landed winners + honest rejects
  'TrackDir.cpp','TrackerDisplay.cpp','StoreInfoPanel.cpp','NetworkEmulator.cpp','BandUserMgr.cpp','OutfitConfig.cpp','AccomplishmentPanel.cpp','NetSession.cpp',
  // wave-4 (main @a38ef1b): landed PatchPanel/CampaignLevel; rejects (FOREIGN/MIXED/no-oracle ICF-stub)
  'PatchPanel.cpp','CampaignLevel.cpp','BandMachineMgr.cpp','CharacterCreatorPanel.cpp','BandStorePanel.cpp','BandHeadShaper.cpp','UIProxy.cpp','Game.cpp',
  // wave-5 (main @70d60d5): landed UIPanel/FocusTracker/TrackWidget/EntityUploader/StarDisplay (+152);
  // rejects incl MetaPanel (now FOREIGN frun=124, supersedes wave-3 deferral), BandLabel/NewAwardPanel FOREIGN
  'UIPanel.cpp','FocusTracker.cpp','TrackWidget.cpp','EntityUploader.cpp','StarDisplay.cpp','AccomplishmentOneShot.cpp','BandSongMetadata.cpp','BandLabel.cpp','NewAwardPanel.cpp','MetaPanel.cpp',
  // wave-6 (main @895b7e9): landed MetaMusic/CheckboxDisplay/BandWardrobe (+50); rejects NextSongPanel
  // (not TU-pure), MicInputArrow/BandButton/BandTrack/Utl/Loader/GuitarController FOREIGN/MIXED
  'MetaMusic.cpp','CheckboxDisplay.cpp','BandWardrobe.cpp','NextSongPanel.cpp','MicInputArrow.cpp','BandButton.cpp','BandTrack.cpp','Utl.cpp','Loader.cpp','GuitarController.cpp',
  // wave-7 (main @39a2199): landed BandLeadMeter/UIStats (+22); rejects ClosetPanel (not TU-pure),
  // DirectInstrument/GameConfig FOREIGN, Performer MIXED (vbase wall), + scanned-not-OWN
  'BandLeadMeter.cpp','UIStats.cpp','ClosetPanel.cpp','DirectInstrument.cpp','GameConfig.cpp','Performer.cpp','TourBand.cpp','BandHighlight.cpp','SongSectionController.cpp','SongSortByRank.cpp']

const SCAN_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['candidates','notes'],
  properties: {
    candidates: { type: 'array', items: { type: 'object', additionalProperties: false,
      required: ['name','tu','wii_path','dst_path','core_va','gap_kb','core_fns'],
      properties: { name:{type:'string'}, tu:{type:'string'}, wii_path:{type:'string'}, dst_path:{type:'string'}, core_va:{type:'string'}, gap_kb:{type:'integer'}, core_fns:{type:'integer'} } } },
    notes: { type: 'string' },
  },
}
const VAL_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['name','ownership_verdict','proposed_start','proposed_end','named_method_count','max_foreign_zero_run','compile_feasible','confidence','notes'],
  properties: { name:{type:'string'}, ownership_verdict:{type:'string',enum:['OWN','MIXED','FOREIGN']}, proposed_start:{type:'string'}, proposed_end:{type:'string'}, named_method_count:{type:'integer'}, max_foreign_zero_run:{type:'integer'}, compile_feasible:{type:'boolean'}, confidence:{type:'number'}, notes:{type:'string'} },
}
const PORT_SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['name','branch','baseline_matched','after_matched','real_net_delta','honest','icf_clean','blocker','notes'],
  properties: { name:{type:'string'}, branch:{type:'string'}, baseline_matched:{type:'integer'}, after_matched:{type:'integer'}, real_net_delta:{type:'integer'}, honest:{type:'boolean'}, icf_clean:{type:'boolean'}, blocker:{type:'string'}, notes:{type:'string'} },
}

phase('Scan')
const scan = await agent(
`READ-ONLY broadened scan for class-A TU-pure span-pin candidates (rb3-xenon decomp at ${REPO}, main HEAD ~10027 matched). The class-A method (proven: GemManager +35, AppLabel +52, TrackPanel +22): an UNPINNED game/engine TU whose methods form a CONTIGUOUS TU-PURE span in retail (preserved by /O1 spatial grouping, no LTCG), found via DISTINCTIVE string anchors, pinned + ported -> its own STL/funclet bodies byte-reproduce even while anonymous. Two prior waves are harvested (9 winners, +184).

BROADEN: re-run the string-anchored-core scan with (a) a LOWER threshold (>=2-fn cores) and (b) ALL rb3-Wii game/engine source dirs (band3/*, network/*, system/bandobj, system/world, system/ui, system/track, etc.). Reuse the method from ${REPO}/docs/decomp/research/2026-06-21-string-anchor-recall-probe.md (fingerprints.json string->VA index, rarity<=3, contiguous core in an unpinned splits gap, bounded by neighbour pins in config/45410914/splits.txt). For each candidate emit {name, tu, wii_path (abs ../rb3/src/...), dst_path (rel src/...), core_va, gap_kb, core_fns}.

EXCLUDE (already done or known-MIXED): ${JSON.stringify(DONE_OR_MIXED)} — and drop any whose gap is HUGE (>80KB = mixed multi-TU region, e.g. the BandwidthCounter/TournamentDDL class) or whose core sits in the 0x8269xxxx .game panel belt (proven mixed). Prefer TIGHT gaps (<=20KB) with the core well-distributed across exclusive strings = the TU-pure signature. Return up to 10 ranked candidates (tightest+most-exclusive-string first). Return SCAN_SCHEMA.`,
  { schema: SCAN_SCHEMA, label: 'scan:classA-w3', phase: 'Scan' }
)
const cands = (scan?.candidates || []).filter(c => !DONE_OR_MIXED.includes(c.tu)).slice(0, 10)
log(`Scan: ${cands.length} fresh candidates — ${cands.map(c=>c.name).join(', ') || '(none)'}`)

phase('Validate')
const vals = cands.length ? await parallel(cands.map(c => () => agent(
`READ-ONLY ownership-purity + compile-feasibility validation for class-A candidate ${c.tu} (core ${c.core_va}, gap ~${c.gap_kb}KB) in rb3-xenon (${REPO}). Exemplar = GemManager (+35, TU-pure) vs GemPlayer (REJECTED, 96-fn foreign run). rb3-Wii source = ${c.wii_path}.
DO (read-only): (1) bound the unpinned gap around ${c.core_va} (neighbour pins in config/45410914/splits.txt); (2) attribute each gap fn via unified_id_rb3wii.json (NEAR-RANDOM oracle — discount it), DISTINCTIVE strings cross-matched to ${c.wii_path} vs OTHER TUs, and intra-cluster bl call-topology; compute max_foreign_zero_run; ownership_verdict=OWN only if max_foreign_zero_run<8 AND the core is ${c.name}'s own; propose the tightest OWN [start,end) on fn boundaries strictly inside the gap. (3) compile feasibility: ${c.wii_path} exists + its #includes/by-value-member headers are present-or-cheaply-copyable in ${REPO}/src. Return VAL_SCHEMA. Be honest — most candidates are MIXED; the validate stage OVER-CLAIMS OWN as the pool thins (string-content over-attributes), so the port stage is the real guard. Better to under-claim.`,
  { schema: VAL_SCHEMA, label: `val:${c.name}`, phase: 'Validate' }
))) : []

const go = vals.filter(Boolean).map(v => ({ v, c: cands.find(x => x.name === v.name || x.tu === v.name) }))
  .filter(p => p.c && p.v.ownership_verdict === 'OWN' && p.v.compile_feasible && p.v.confidence >= 0.45)
log(`Validate: ${go.length}/${cands.length} OWN+feasible — ${go.map(p=>p.v.name).join(', ') || '(none)'}`)

phase('Port')
const ports = go.length ? await parallel(go.map(({ v, c }) => () => agent(
`Port-then-pin ${c.tu} (class-A TU-pure harvest wave-3, rb3-xenon). Validated OWN span [${v.proposed_start}, ${v.proposed_end}) (${v.named_method_count} methods). rb3-Wii source ${c.wii_path} -> ${c.dst_path}. GemManager/AppLabel +35/+52 recipe; full detail in ${SOP} + ${REPO}/CLAUDE.md.
HARD RULES: buildable worktree ONLY (NEVER the main tree); do NOT land to main — commit to your branch + RETURN it. Other agents are working concurrently; do not touch /tmp/wt-dc3drain, /tmp/wt-dc3naming, or any worktree you did not create.
STEPS: (1) \`cd ${REPO} && scripts/setup_worktree.sh /tmp/wt-cA3-${c.name} cA3-${c.name}\`; work there. Fresh worktree friction: if a build re-downloads compilers (SSL fail in sandbox), add an early skip-guard in the worktree's tools/download_tool.py (\`if output.exists() and (not output.is_dir() or any(output.iterdir())): print("skip"); return\` before the Downloading print) and \`ln -sf ${REPO}/build/tools/wibo build/tools/wibo\`. (2) baseline fresh_report (rm -f build/45410914/*/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked once) -> baseline_matched from build/45410914/report.json. (3) port ${c.wii_path} -> ${c.dst_path} (MWCC->MSVC X360: a wired sibling TU is the template; .mStr->.Str(), decomp.h macros are MSVC no-ops, fix include paths, copy any missing by-value-member headers from the oracle); add \`"${c.tu}":"NonMatching"\` to config/45410914/objects.json. (4) pin \`.text start:${v.proposed_start} end:${v.proposed_end}\` under a \`${c.tu}:\` header in config/45410914/splits.txt; \`python3 scripts/harvest/overlap_check.py config/45410914/splits.txt --text-only\` must report 0 overlaps. (5) \`venv/bin/python3 tools/gen_game_target_map.py --tu ${c.tu}\` (ADD-ONLY; never --apply on the whole map) + reveal. (6) \`python3 configure.py\` + fresh_report (rm stamp + touch config.yml + ninja once) -> after_matched; real_net_delta = after - baseline; debug compile blockers or report honestly. (7) \`python3 tools/icf_alias_check.py --worktree /tmp/wt-cA3-${c.name} --baseline-report <baseline.json copy>\` (exit1 = ICF-stub-fold inflation); honest=true ONLY if real_net_delta>0 AND icf_clean AND the newly-100 fns are real-bodied (>44B) ${c.name} methods, NOT <=44B stub-folds. +0 is a valid HONEST outcome (report blocker). (8) commit to branch cA3-${c.name} (end the message with: Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>); do NOT merge to main.
Return PORT_SCHEMA.`,
  { schema: PORT_SCHEMA, label: `port:${c.name}`, phase: 'Port' }
))) : []

const landable = ports.filter(Boolean).filter(p => p.honest && p.icf_clean && p.real_net_delta > 0)
log(`Port: ${landable.length} honest winners, +${landable.reduce((s,p)=>s+p.real_net_delta,0)} — ${landable.map(p=>`${p.name}(+${p.real_net_delta})`).join(', ') || '(none)'}`)
return { scanned: cands.length, validations: vals.filter(Boolean), ported: ports.filter(Boolean),
  landable: landable.map(p => ({ name: p.name, branch: p.branch, delta: p.real_net_delta })),
  recommendation: 'Coordinator: rebase each honest winner branch onto latest main, composed-verify (run1==run2), land via scripts/harvest/land.sh, re-check whole-binary A/B for 0 regressions.' }
