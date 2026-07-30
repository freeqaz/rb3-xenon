export const meta = {
  name: 'saverev-sweep',
  description: 'Fix RB3-retail-vs-DC3 save/load REVISION constant mismatches in Save/Load near-miss functions (the PostProcer SAVE_REVS(1,0)->(0,0) pattern). Ground-truth data-constant corrections; verify TRUE-100% + whole-binary positive. Never commit to main.',
  phases: [ { title: 'SaveRev', detail: 'one agent per Save/Load near-miss; correct rev constant to retail ground truth' } ],
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

// Save/Load named near-misses (>=98%) — candidate rev-constant mismatches.
const UNITS = [
  { unit: 'UIListDir',        sym: '?Save@UIListDir@@UAAXAAVBinStream@@@Z' },
  { unit: 'CharSleeve',       sym: '?Save@CharSleeve@@UAAXAAVBinStream@@@Z' },
  { unit: 'CharIKSliderMidi', sym: '?Save@CharIKSliderMidi@@UAAXAAVBinStream@@@Z' },
  { unit: 'SampleData',       sym: '?Load@SampleData@@QAAXAAVBinStream@@ABVFilePath@@@Z' },
  { unit: 'CameraShot',       sym: '?Load@CamShotCrowd@@QAAXAAVBinStream@@@Z' },
]

phase('SaveRev')

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    unit: { type: 'string' }, sym: { type: 'string' },
    landed_100: { type: 'boolean' },
    net_delta: { type: 'integer' },
    root_cause: { type: 'string' },   // what the residual was (rev const? something else?)
    is_rev_fix: { type: 'boolean' },  // true if it was a SAVE_REVS/LOAD_REVS constant correction
    patch_path: { type: ['string','null'] },
    notes: { type: 'string' },
  },
  required: ['unit','sym','landed_100','net_delta','root_cause','notes'],
}

const results = await parallel(UNITS.map(U => () =>
  agent(`Fix the rb3-xenon near-miss function ${U.sym} in unit "${U.unit}" — likely a SAVE/LOAD REVISION constant mismatch (DC3 source is NEWER than RB3 and bumped a save/load rev; RB3 retail = ground truth). This is the PostProcer pattern: SAVE_REVS(1,0)->SAVE_REVS(0,0) flipped a single 'li rN, <rev>' to match retail and took Save@PostProcer to 100%.

SETUP: isolated buildable worktree, NEVER touch main:
  cd ${REPO} && scripts/setup_worktree.sh .claude/worktrees/sr-${U.unit} sr-${U.unit}
work inside it. ./tools/ninja-locked first (if dtk trips, configure.py with absolute --dtk ../jeff --objdiff ../objdiff --wrapper ../wibo/build/release/wibo). Record baseline matched_functions (${BASELINE}).

DIAGNOSE: run objdiff on the function to find the EXACT differing instruction(s):
  bin/objdiff-cli diff -p . '${U.sym}' -f json --include-instructions  (or use diff_inspect / the recon skill).
The signature of a rev mismatch: the ONLY real difference is a single 'li rN, <const>' where the constant differs by a small amount — that is packRevs(alt,rev) emitted by SAVE_REVS/LOAD_REVS/ASSERT_REVS in the BEGIN_SAVES/BEGIN_LOADS/INIT_REVS macro block of the unit's .cpp. The RETAIL value is ground truth.

FIX: locate the macro in src/.../${U.unit}.cpp (SAVE_REVS / LOAD_REVS / ASSERT_REVS / INIT_REVS args are (rev, altrev) -> packRevs). Adjust the rev so our emitted constant equals retail's. CROSS-CHECK against rb3-Wii (../rb3/src) if it has the unit, to confirm the older rev (per the dc3-is-newer caveat). Do NOT blindly zero it — match the exact retail constant.

If the residual is NOT a rev constant (e.g. struct-size 'li r3,0xNNN' for a new T, or regalloc), set is_rev_fix=false, record root_cause, make NO change (out of scope), and report it as a negative.

VERIFY: rebuild (rm build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml; ./tools/ninja-locked). Confirm the function reads 100% (match_percent_normalized==100) in report.json AND whole-binary matched_functions strictly increased (>${BASELINE}). Only then is it landable. mkdir -p ~/tmp/saverev && git diff > ~/tmp/saverev/${U.unit}.patch (empty if no fix). Return schema. NEVER commit to main; leave worktree.`,
    { label: `saverev:${U.unit}`, phase: 'SaveRev', schema: SCHEMA })
))

return { results: results.filter(Boolean) }
