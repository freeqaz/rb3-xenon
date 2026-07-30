export const meta = {
  name: 'verify-stage-wave',
  description: 'Coordinator verify-and-stage: for each pre-edited ready worktree (Stream-1 struct fixes + option-C DC3-cluster ports), do an in-worktree whole-binary A/B, and if net-positive with 0 regressions, COMMIT only the match-relevant files (exclude the download_tool.py build helper + regenerable artifacts) on the branch so the coordinator can land via scripts/harvest/land.sh. NEVER touch main; NEVER land.',
  phases: [ { title: 'Verify', detail: 'one agent per ready worktree: A/B -> commit match files if landable' } ],
}

// {wt: absolute worktree path, branch, kind, note}. CharEyes handled by coordinator directly.
const WORKTREES = [
  { wt: '/home/free/tmp/wt-s1-Character',      branch: 'wt-s1-Character',   kind: 'struct', note: 'Character +8 (dc3 char BASE class — may cascade wide; watch regressions carefully). match files: src/system/char/Character.h (NOT tools/download_tool.py).' },
  { wt: '/home/free/tmp/wt-s1-CreditsPanel',   branch: 'wt-s1-CreditsPanel',kind: 'struct', note: 'CreditsPanel +4 (rb3-Wii game). match: src/system/meta/CreditsPanel.cpp/.h.' },
  { wt: '/home/free/tmp/wt-s1-GamePanel',      branch: 'wt-s1-GamePanel',   kind: 'struct', note: 'GamePanel +24 (rb3-Wii game, 7 uniform fns). match: src/band3/game/GamePanel.cpp (NOT tools/download_tool.py).' },
  { wt: '/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-oc-AccomplishmentProgress', branch: 'oc-AccomplishmentProgress', kind: 'port', note: 'option-C DC3-cluster port-then-pin. match files may include src + objects.json + splits.txt + target_symbol_map.json.', wtAlt: '/tmp/wt-oc-AccomplishmentProgress' },
  { wt: '/tmp/wt-oc-CharClipGroup',   branch: 'oc-CharClipGroup',  kind: 'port', note: 'option-C port. match: src + objects.json + splits + tsm.' },
  { wt: '/tmp/wt-oc-MoggClip',        branch: 'oc-MoggClip',       kind: 'port', note: 'option-C port. match: src + objects.json + splits + tsm.' },
  { wt: '/tmp/wt-oc-NavListNode',     branch: 'oc-NavListNode',    kind: 'port', note: 'option-C port. match: src + objects.json + splits + tsm.' },
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    branch: { type: 'string' }, wt: { type: 'string' },
    built_ok: { type: 'boolean' },
    net_delta: { type: 'integer' },
    regressions: { type: 'array', items: { type: 'string' } },
    landable: { type: 'boolean' },
    committed: { type: 'boolean' },              // committed match files on the branch (only if landable)
    match_files: { type: 'array', items: { type: 'string' } },
    excluded_files: { type: 'array', items: { type: 'string' } },   // cruft NOT committed (download_tool.py etc.)
    tooling_gaps: { type: 'array', items: { type: 'string' } },
    notes: { type: 'string' },
  },
  required: ['branch','built_ok','net_delta','landable','committed','notes'],
}

phase('Verify')

const thunks = WORKTREES.map(W => () => agent(
  `Verify + stage the pre-edited ready worktree "${W.branch}" (${W.kind}). ${W.note} Worktree path: ${W.wt}${W.wtAlt ? ' (if that path is missing, try '+W.wtAlt+')' : ''}. NEVER touch the main repo tree; NEVER land; work only inside this worktree.

STEPS:
1. cd the worktree. \`git status --short\` — identify the MATCH-RELEVANT changed files (the .cpp/.h and, for a port, config/45410914/objects.json + config/45410914/splits.txt + scripts/target_symbol_map.json). EXCLUDE non-match cruft: tools/download_tool.py (an offline-build helper), any untracked *.json artifacts (global_fuzzy_pairs.json), *.obj. Record match_files + excluded_files.
2. IN-WORKTREE WHOLE-BINARY A/B (the baseline drifts on main, so measure IN the worktree):
   a. Stash ONLY the match files: \`git stash push -- <match files>\`. Keep download_tool.py present (it helps the build).
   b. Build baseline: \`rm -f build/45410914/*/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked build/45410914/report.json 2>&1 | tee ~/tmp/vs_${W.branch}_base.log\`. Save the 100%-normalized fn SET + matched_functions.
   c. \`git stash pop\` (restore match files). For a port, also re-run \`python3 configure.py --dtk /home/free/code/milohax/jeff/target/release/dtk --objdiff /home/free/code/milohax/objdiff/target/release/objdiff-cli --wrapper /home/free/code/milohax/wibo/build/release/wibo\` if objects.json/splits changed.
   d. Build after: same ninja command (+ rm stamp + touch config.yml). Save the after SET.
   e. net_delta = after - baseline (matched_functions). regressions = fns 100% in baseline SET but NOT in after SET (by (unit,name)). If unsure the incremental build is coherent, re-run once (deterministic) or use tools/fresh_report.sh.
3. LANDABLE iff net_delta > 0 AND zero real regressions (ICF/renamer naming-noise that nets to 0 doesn't count — be conservative; a port that overlaps an existing pin or drops an objects.json entry is NOT landable).
4. If landable: COMMIT ONLY the match files on the branch: \`git add <match files>; git commit -m "${W.branch}: verified +<net> @100%, 0 regr"\`. Set committed=true. DO NOT commit download_tool.py or artifacts. If NOT landable, revert cleanly (git checkout the match files), committed=false, report why.
Return SCHEMA. Report tooling_gaps (e.g. build-helper friction, stale struct_db). NEVER land to main.`,
  { label: `verify:${W.branch}`, phase: 'Verify', schema: SCHEMA }))

const results = (await parallel(thunks)).filter(Boolean)
const landable = results.filter(r => r.landable && r.committed)
log(`verify-stage: ${landable.length}/${results.length} landable+committed: ${landable.map(r=>r.branch+'(+'+r.net_delta+')').join(', ')}`)
return { results, landable }
