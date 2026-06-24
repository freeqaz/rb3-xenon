export const meta = {
  name: 'idt-classb-w1',
  description: 'Class-B ICF-scattered belt via identity-transfer micro-pins. Target the wave-8 TUs that validated as having REAL OWN methods but scattered (not span-pinnable): OvershellSlotState/SessionUsersProviders/Leaderboard. Per-method micro-pin (case-A: methods in unowned auto_ blobs), oracle_quality pre-screen + field-gate + icf_alias honesty gate.',
  phases: [
    { title: 'Harvest', detail: 'one agent per class-B TU: oracle_quality screen, port, micro-pin GOOD methods, measure real wins' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'
// wave-8 validated-OWN-but-scattered (real methods, no contiguous span) + known class-B panels.
// OvershellSlotState is ALREADY wired in objects.json (port stage just micro-pins).
const TARGETS = [
  { tu: 'band3/meta_band/OvershellSlotState.cpp', wired: true,  note: 'already wired NonMatching; HandleMsg/UpdateView/GetRemoteStatus/IsPartUnresolved + Mgr; base=Hmx::Object (no DC3-drift). Pure micro-pin, no port needed if .cpp already in src.' },
  { tu: 'band3/meta_band/SessionUsersProviders.cpp', wired: false, note: 'KickPlayerMsg NetMessage subclass + providers; 15-fn island at 0x826375B8 + scattered rest; port + micro-pin the string-confirmed methods.' },
  { tu: 'band3/meta_band/Leaderboard.cpp', wired: false, note: 'Leaderboard/LeaderboardShortcutProvider/LeaderboardRow; SetData + list accessors; 418 lines; port + micro-pin the string-pure cluster [0x8264EEF0,0x82651480).' },
]

const SCHEMA = {
  type: 'object', additionalProperties: false,
  required: ['tu', 'ported', 'methods_micropinned', 'real_net_delta', 'honest', 'icf_clean', 'verdict', 'branch', 'notes'],
  properties: {
    tu: { type: 'string' }, ported: { type: 'boolean' }, methods_micropinned: { type: 'integer' },
    real_net_delta: { type: 'integer' }, honest: { type: 'boolean' }, icf_clean: { type: 'boolean' },
    verdict: { type: 'string' }, branch: { type: 'string' }, body_divergence_killed: { type: 'integer' }, notes: { type: 'string' },
  },
}

function prompt(t) {
  const base = t.tu.split('/').pop().replace('.cpp', '')
  return `Identity-transfer micro-pin harvest of the CLASS-B ICF-scattered TU ${t.tu} (rb3-xenon, ${REPO}). This TU is NOT span-pinnable (wave-8 proved its methods are scattered among foreign code), but it has REAL OWN methods that per-method micro-pins can capture. ${t.note}
READ FIRST: docs/decomp/identity-transfer/PIPELINE-DESIGN.md + B2-FINDINGS-oracle-wall.md (the oracle-misattribution + body-divergence walls) + docs/decomp/identity-transfer.md. The pipeline is BUILT but THIN (prior harvest 0/10 on fresh TUs) — your job is an HONEST attempt: capture the methods that genuinely byte-reproduce, report +0 honestly if they body-diverge.

Work in your OWN CoW worktree (scripts/setup_worktree.sh /tmp/wt-idtb-${base} idtb-${base}; download_tool.py skip-guard + ln -sf ${REPO}/build/tools/wibo build/tools/wibo). NEVER edit main. Do NOT land — commit to your branch + RETURN it.

PROCEDURE:
1. baseline fresh_report (rm -f build/45410914/*/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked once) -> baseline matched.
2. ${t.wired ? 'The TU is ALREADY wired in objects.json and the .cpp is in src — skip porting unless it does not compile.' : `PORT ../rb3/src/${t.tu} -> src/${t.tu} (MWCC->MSVC X360; whole file compiles+defines; copy missing headers from oracle). Add "${t.tu}":"NonMatching" to config/45410914/objects.json; python3 configure.py; build.`}
3. PRE-SCREEN: python3 tools/oracle_quality.py --tu ${base}.cpp -> the GOOD methods (real-bodied >44B, size-consistent, not foreign-owned). FIELD-GATE: python3 tools/field_offset_gate.py --tu ${base}.cpp --oracle unified_id_rb3wii.json --emit-pin-only /tmp/fg-${base}.json.
4. PIN-SET = GOOD-oracle ∩ field-gate-clean ∩ methods-DEFINED-in-obj. DRY-RUN tools/identity_transfer.py first; keep only VAs it reports nameable (named>0).
5. MICRO-PIN: python3 tools/identity_transfer.py --tu ${base}.cpp --oracle unified_id_rb3wii.json --pin-only <intersected-set> --apply (STRICT add-only; never gen_game_target_map.py --apply on the whole map). python3 scripts/harvest/overlap_check.py config/45410914/splits.txt --text-only (abort on overlap).
6. BUILD+MEASURE: rm stamp + touch config.yml + ./tools/ninja-locked; real_net_delta = after - baseline.
7. AUDIT (HARD): python3 tools/icf_alias_check.py --worktree /tmp/wt-idtb-${base} --baseline-report <baseline copy>. honest=true ONLY if real_net_delta>0 AND icf_clean AND newly-100 are real-bodied (>44B) ${base} methods (NOT <=44B stub-folds — the dominant class-B false-positive). Count body_divergence_killed = GOOD methods that pinned clean but stayed <100%.
8. verdict = LANDABLE:+N or DEFER:<reason>. If honest win, commit to branch idtb-${base} (Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>).

HONESTY (decisive): byte-equality is the ONLY positive gate; sim is NOT a predictor. A clean +0 (methods pinned but body-diverged, or all stub-folds) is a VALID, important result — report body_divergence_killed + WHY. Do NOT mint fake matches.
Return SCHEMA.`
}

phase('Harvest')
const results = await parallel(TARGETS.map(t => () => agent(prompt(t), { label: `idtb:${t.tu.split('/').pop().replace('.cpp','')}`, phase: 'Harvest', schema: SCHEMA })))
const ok = results.filter(Boolean)
const landable = ok.filter(r => r.honest && r.icf_clean && r.real_net_delta > 0)
ok.forEach(r => log(`${r.tu}: ${r.verdict} delta=${r.real_net_delta} honest=${r.honest} (bodyDiv-killed ${r.body_divergence_killed || 0})`))
log(`IDT-CLASSB: ${landable.length}/${ok.length} landable, +${landable.reduce((s,r)=>s+r.real_net_delta,0)}`)
return {
  landable: landable.map(r => ({ tu: r.tu, branch: r.branch, delta: r.real_net_delta, methods: r.methods_micropinned })),
  deferred: ok.filter(r => !(r.honest && r.icf_clean && r.real_net_delta > 0)).map(r => ({ tu: r.tu, verdict: r.verdict, bodyDivKilled: r.body_divergence_killed })),
  recommendation: 'Coordinator: land honest winners (land.sh + configure.py drop-check + composed verify run1==run2). Expect THIN yield (body-divergence ceiling); a clean +0 across all is a valid characterization result.',
}
