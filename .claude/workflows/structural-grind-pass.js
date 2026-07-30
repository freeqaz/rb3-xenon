// ============================================================================
// STATUS 2026-07-30: DEAD AS WRITTEN -- DO NOT RUN WITHOUT REWORK.
// This workflow drives 'decomp_synth' (scan_and_permute), which lives in a
// separate private repo and is NOT present here, so the commands below will
// fail. Separately, the source-permuter is OFF by standing user directive:
// do not route to /permute until the user re-opens it.
// Kept under version control deliberately: an agent-facing prompt that is
// wrong silently steers every future lane, and while these were untracked
// nothing prevented that. Fix or delete in review -- do not shadow-edit.
// ============================================================================
export const meta = {
  name: 'structural-grind-pass',
  description: 'Best-effort pass over the 7 coupled-base + body-port layout targets. Each agent works a buildable worktree, ports/fixes, builds+measures, permutes, returns a patch + structured result. Agents NEVER commit to main; the main loop lands net-positive patches one at a time.',
  phases: [
    { title: 'Grind', detail: '7 parallel agents — one per work item, each in its own worktree' },
  ],
}

const BASELINE = '/home/free/tmp/grind/baseline.report.json' // frozen main@e735b85 = 4094

const PREAMBLE = `
rb3-xenon decomp (Xbox 360 Rock Band 3, MSVC X360 PowerPC, /O1 /Oi /GR /EHsc).
We match retail machine code from C++ source. You are executing ONE structural
matching work item end-to-end in an ISOLATED worktree.

# READ FIRST — the methodology
docs/plans/coupled-base-and-body-port-playbook.md and
docs/plans/structural-readiness-2026-06-03.md (verified target evidence). Read the
sections relevant to YOUR item before editing.

# HARD RULES
- Work ONLY inside your own worktree. NEVER edit, build, commit, or git-anything in
  the main repo (/home/free/code/milohax/rb3-xenon). Other agents share it.
- NEVER commit. Your deliverable is a PATCH file + a structured result.
- Use ~/tmp (real disk), never /tmp.
- Do NOT touch configure.py or tools/scope_map.py.
- TIME-BOX yourself: a bounded number of edit→build→measure iterations (≈6-10). If you
  are not converging, STOP and return PARTIAL/BLOCKED with a precise diagnosis. Partial
  progress + a clear next-step is a SUCCESS for this pass ("see how far we can get").

# SETUP (do this first)
1. Your worktree is ALREADY created for you at ~/tmp/wt-grind-<KEY> (branch grind-<KEY>),
   a buildable CoW reflink that includes the committed tools. cd into it and do ALL work
   there. Do NOT run setup_worktree.sh (it exists already) and do NOT create another.
2. Establish baseline: the frozen main baseline (4094) is at ${BASELINE}. Confirm your
   worktree builds to it (this warms the cache; later builds are incremental & fast):
     cd ~/tmp/wt-grind-<KEY> && python3 tools/ab_measure.py --worktree . --baseline ${BASELINE} --build
   (should print NET +0). If the build fails for an unrelated reason, report it in
   blocked_reason rather than fighting it.

# THE MATCHING LOOP (per the playbook)
- ORACLES: game code (band3/, world/) -> rb3-Wii ../rb3/src (named, RB3-correct, MWCC);
  engine -> dc3 ../dc3-decomp/src (byte-twin, but a FALSE FRIEND when newer than RB3).
  grep -rn "ClassName" ~/code/milohax/rb3/src/ (or dc3-decomp).
- RETAIL TRUTH: Ghidra (port 8002, SHARED with the other 6 agents -> it WILL be slow;
  prefer NAME-based calls (cached), pass long timeouts, retry once on timeout, and lean
  on objdiff deltas + the two source oracles when Ghidra stalls). Read absolute field
  offsets from a ctor/dtor:
     python3 tools/ghidra/ghidra-decompile.py "Class::Method"
- EMPIRICAL DELTAS: python3 scripts/analysis/diff_inspect.py --symbol "<sym>" --compare-asm --project-dir .
  ( ~ lwz r3,0x40(r30) [off:+4] means target=0x40, ours=0x3c -> we are +4 too big there.)
- MEASURE every change with: python3 tools/ab_measure.py --worktree . --baseline ${BASELINE} --build
  Read the per-unit regressions/improvements. NET is the truth.
- PERMUTE the codegen last-10% once layout+body are right:
     venv/bin/python -m decomp_synth.scan_and_permute --symbol '<sym>' --max-rounds 10 --no-apply
  Run with --no-apply first; only re-apply a variant that hits TRUE 100% (decomp-synth
  will otherwise commit plausible-but-wrong variants at non-100% — see
  project-permuter-correctness-model).

# COUPLED-BASE items specifically (Flow, PanelDir, MicManager)
The fix shifts EVERY derived/embedding class. Before editing the base:
  python3 tools/layout_family.py <BaseClass>
to see the whole family + which members already match (those will REGRESS) vs near-miss
(those IMPROVE). After the edit, judge on the WHOLE-FAMILY net from ab_measure, NOT the
base unit alone. If net-negative, diagnose the top regressors (their compensating bug)
and EITHER fix them too (converge) or return NET_NEGATIVE with the diagnosis — do NOT
leave a net-negative patch as "landable".

# DELIVERABLE (do this at the end)
1. Write your patch:  cd <worktree> && git add -A && git diff --cached > ~/tmp/grind/<KEY>.patch
2. Return the structured result (schema). Set landable=true ONLY if net_delta>0 AND no
   unexplained regressions. Leave the worktree in place (do not remove it).
`;

const RESULT_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  properties: {
    key: { type: 'string' },
    item_type: { type: 'string', enum: ['body-port', 'coupled-base'] },
    status: { type: 'string', enum: ['MATCHED_GAIN', 'PARTIAL', 'NET_NEGATIVE', 'BLOCKED', 'NO_CHANGE'] },
    net_delta: { type: 'integer', description: 'whole-family net matched_functions delta from ab_measure' },
    improvements: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit', 'delta'] } },
    regressions: { type: 'array', items: { type: 'object', additionalProperties: false, properties: { unit: { type: 'string' }, delta: { type: 'integer' } }, required: ['unit', 'delta'] } },
    files_changed: { type: 'array', items: { type: 'string' } },
    patch_path: { type: 'string', description: 'path to the written .patch, or "" if none' },
    worktree: { type: 'string' },
    approach: { type: 'string', description: 'what you actually did (layout pinned, body ported, etc.)' },
    landable: { type: 'boolean', description: 'true ONLY if net_delta>0 with no unexplained regressions' },
    blocked_reason: { type: 'string', description: 'if BLOCKED/PARTIAL/NET_NEGATIVE: precise reason + what is needed next' },
    notes: { type: 'string', description: 'findings for the grind doc / next session' },
  },
  required: ['key', 'item_type', 'status', 'net_delta', 'improvements', 'regressions', 'files_changed', 'patch_path', 'worktree', 'approach', 'landable', 'blocked_reason', 'notes'],
}

const ITEMS = [
  {
    key: 'datafile-parsearray', type: 'body-port',
    title: 'DataFile::ParseArray — split merged global into two file-statics (EASY, do thoroughly)',
    body: `Unit default/DataFile. Verified finding: ParseArray's mismatch is a SOURCE-SHAPE
difference — retail uses TWO separate file-static variables (an array + a node) where
our DataFile.cpp uses ONE merged global. Fix = declare them as two separate file-static
variables in src/system/obj/DataFile.cpp (find ParseArray + the static(s) it uses).
Cross-check rb3-Wii ../rb3/src (DataFile / Data.cpp) and dc3 for the exact two statics.
Also the -16 funclet deltas in the unit are NOISE (DROP) — ignore them; target ParseArray.
Build+measure DataFile; permute ParseArray to 100% if close.`,
  },
  {
    key: 'char3d-mhandle', type: 'body-port',
    title: 'WorldCrowd::CharData::Char3D mHandle -> int index (BIGGEST cascade, ~20 fns)',
    body: `Unit default/Crowd. Verified: retail Char3D sizeof=0x50 (Transform@0x0, int mIdx@0x40,
mColors@0x44, NO WorldCrowd3DCharHandle* mHandle); ours is 0x54 (mHandle ptr@0x50). The
-4 cascades across ~20 std::vector<Char3D> template fns (element stride 0x54 vs 0x50).
ROOT: dc3 (newer) added a WorldCrowd3DCharHandle OBJECT system; RB3 retail stores a plain
int instance-index in that slot (see src/system/world/Crowd.cpp:754 & :842 casting mHandle
to int). PORT to RB3's int-index model: pull rb3-Wii ../rb3/src world/Crowd.{h,cpp} as the
oracle; Ghidra-decompile the WorldCrowd 3D-char fns (Create/Update/the comparison sites) to
confirm the int-index algorithm; change Char3D.mHandle from WorldCrowd3DCharHandle* to the
int index, remove the handle-object usage, reconcile the ctors (mHandle(0)/(nullptr)) and
the ~10 Crowd.cpp usage sites. Char3D shrinks 0x54->0x50 and the vector fns flip. This is
the hardest body port — TIME-BOX; if the subsystem is too entangled, land the layout shrink
that is safe and return PARTIAL with exactly which usage sites block.`,
  },
  {
    key: 'micmanager', type: 'coupled-base',
    title: 'MicManagerXbox +8 (self-contained, LOW risk — no class derives from it)',
    body: `Unit default/Mic. Verified (direction corrected): retail places the std::vector<ChatBuffer>
at 0x28; ours at 0x20 (off:-8) -> we are MISSING 8 bytes (2 fields) BEFORE that vector.
MicManagerXbox is no class's base (self-contained), so blast radius is just this unit -> low
risk. Ghidra-decompile MicManagerXbox::MicManagerXbox (ctor) to identify the two ~4-byte
fields stored before the ChatBuffer vector; cross-check rb3-Wii ../rb3/src (Mic). Add them to
the header (src/.../Mic.h) before the vector. Build+measure Mic; permute if close. Run
layout_family.py MicManagerXbox to confirm nothing derives from it before landing.`,
  },
  {
    key: 'accomplishment', type: 'body-port',
    title: 'AccomplishmentProgress (+4 field) & AccomplishmentManager (+40 gap) — field reconstruction',
    body: `Units default/band3/meta_band/AccomplishmentProgress and .../AccomplishmentManager.
Verified real layout bugs (NOT funclet noise):
- Progress: a 4-byte field is missing before mGamerAwardStatusList (retail @0x54, ours @0x50);
  everything after shifts +4.
- Manager: a 40-byte gap — mGoalAcquisitionInfos retail @0x170 vs ours @0x148,
  mGoalProgressionInfos @0x17c vs @0x154.
Ghidra-decompile the AccomplishmentProgress and AccomplishmentManager CTORS to enumerate the
member stores and identify the missing field(s) (could be added members, or a map/list sized
0x1c vs 0x18). Cross-check rb3-Wii ../rb3/src/band3/meta_band/Accomplishment* (game oracle,
named). Add the missing fields to the headers. Build+measure both units. These are SEPARATE
units; do whichever converges. Return PARTIAL with the exact missing-field identity if you
can pin it but not fully match.`,
  },
  {
    key: 'flow-collections', type: 'coupled-base',
    title: 'Flow ObjPtr-collection sizes too big (BROAD cascade if cracked; pin sizes FIRST)',
    body: `Units default/FlowNode, FlowIf, FlowQueueable, FlowSound. Verified our non-vbase regions are
too big: FlowNode 0x38 retail vs 0x64 ours (+0x2c); FlowQueueable 0x50 vs 0x70 (+0x20);
FlowIf 0x30 vs 0x90 (+0x60). ROOT: our ObjPtrVec/ObjPtrList/ObjVector CONTAINER sizes differ
from retail. This is HIGH-RISK (these collections are embedded across the engine).
STEP 1 (do this before any edit): PIN the retail sizeof of ObjPtrVec<T>, ObjPtrList<T>,
ObjVector<T>. Ghidra-decompile a ctor that constructs each (or a FlowNode/FlowIf ctor) and
read the member spans; compare to our src/system/obj/Object.h definitions; also run
  python3 tools/layout_family.py ObjPtrVec   (and ObjPtrList, ObjVector)
to see the full embedding family + current match states. STEP 2: if a bounded container-size
fix exists, apply it and measure the WHOLE family. If net-positive, great. If net-negative or
the change is unbounded (touches dozens of already-matching classes), DO NOT force it — return
NET_NEGATIVE/BLOCKED with the pinned retail sizes + the family impact analysis (that pinning is
itself the deliverable for a future converge session).`,
  },
  {
    key: 'paneldir', type: 'coupled-base',
    title: 'PanelDir mComponents -8 (base of ~8 classes; converge or diagnose)',
    body: `Unit default/PanelDir. Verified: mComponents at 0x1f8 (ours) vs 0x200 (retail) [off:+8];
mTriggers@0x1f0 MATCHES both builds (so the RndDir base is correct) -> the 8-byte deficit is
PanelDir-LOCAL, between mTriggers and mComponents where std::list<Flow*> mFlows lives.
Effectively our mTriggers+mFlows occupy 0x8 where retail uses 0x10 -> mFlows contributes 0
bytes in our build (root unresolved: STLport std::list size, or a list collapse). The header
annotations (0x218/0x220/0x228) are STALE vs compiled reality.
PanelDir is the base of ~8 classes (run layout_family.py PanelDir). STEP 1: Ghidra-decompile a
PanelDir ctor for the ABSOLUTE mTriggers/mFlows/mComponents offsets; determine the real
STLport std::list<T> sizeof in this build (decompile any std::list member access). STEP 2: if
mFlows genuinely should occupy 8 bytes and doesn't, fix it (src/system/ui/PanelDir.h) and
measure the WHOLE PanelDir family. If net-positive, land. If net-negative, diagnose the
regressors and return NET_NEGATIVE with the pinned offsets. Heed the UIPanel precedent
(net -9 from sibling regression).`,
  },
  {
    key: 'uimanager', type: 'body-port',
    title: 'UIManager body port (full UI.cpp; biggest effort — do as far as time allows)',
    body: `Unit default/UI. The header virtual-base migration is already applied; the BODY diverges:
RB3 has a std::list mResources@0x34 + a DTA-driven resource loader (retail fn_827E0448) that
dc3 LACKS. Matching requires porting Init/Poll/Terminate/GotoScreen/PushScreen/PopScreen from
the binary MERGED with rb3-Wii ../rb3/src/system/ui/UI.cpp (dc3 is a FALSE FRIEND here — it
lacks the resource list). Read docs/plans/ui-base-layout-reconstruction.md + the
project-engine-baseclass-layout-wall memory UIManager section (full layout banked there).
Ghidra-decompile UIManager::Init/Poll/Terminate/PushScreen/PopScreen for the real algorithm.
This is the largest port — TIME-BOX hard; port what you can (e.g. the field map + one or two
fns), measure, and return PARTIAL with a precise remaining-work list. Any clean per-fn gain is
worth landing.`,
  },
]

function promptFor(it) {
  return `${PREAMBLE}

# ===== YOUR WORK ITEM: ${it.key} (${it.type}) =====
${it.title}

${it.body}

Your pre-created worktree is ~/tmp/wt-grind-${it.key} (branch grind-${it.key}); write your
patch to ~/tmp/grind/${it.key}.patch. Begin with SETUP (cd + verify baseline), then the
matching loop, then write the patch and return the structured result.`
}

phase('Grind')
const results = await parallel(
  ITEMS.map(it => () => agent(promptFor(it), {
    label: `grind:${it.key}`, phase: 'Grind', schema: RESULT_SCHEMA,
  }))
)
const done = results.filter(Boolean)
const gains = done.filter(r => r.landable && r.net_delta > 0)
log(`grind pass done: ${done.length}/${ITEMS.length} returned; ${gains.length} landable (net +${gains.reduce((s, r) => s + r.net_delta, 0)})`)
return { baseline_matched: 4094, items: done }
