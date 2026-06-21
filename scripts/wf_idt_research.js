export const meta = {
  name: 'idtransfer-research',
  description: 'Research + design the identity-transfer micro-pin pipeline; write research docs to disk',
  phases: [
    { title: 'Research', detail: '4 parallel lanes: tooling audit, objdiff case-B fork, scattered-TU backlog, source-port bottleneck — each writes a research doc' },
    { title: 'Synthesize', detail: 'master design + implementation plan doc from the 4 research docs' },
  ],
}

const DOCDIR = 'docs/decomp/identity-transfer/research'

const COMMON = `Repo: /home/free/code/milohax/rb3-xenon. Sibling repos: ../objdiff (objdiff fork), ../jeff (dtk fork), ../rb3 (rb3-Wii dev decomp oracle), ../dc3-decomp (engine twin).
GROUNDING FACTS (already established — verify, don't re-derive):
- Identity-transfer carves each ICF-scattered method into its TU obj by individual VA via N RAW multi-range .text micro-pins (jeff ObjSplits::push, never auto-merged). PROVEN: RockCentral.cpp +17. Doc: docs/decomp/identity-transfer.md.
- jeff needs NO changes for the mechanism (arbitrary-N multi-range already works). Case-A (method in unowned auto_ blob) works with stock tools. Case-B (method inside a FOREIGN pin) needs the objdiff global-byte-equality fork.
- objdiff fork: ../objdiff branch caseb-global-byteeq @b1c92be (report.rs global byte-eq 2nd pass + oracle gate). Built+proven, NOT integrated (shared ../objdiff/target/release/objdiff-cli still stock). Doc: docs/decomp/handoff/objdiff-caseb-fork-banked.md.
- The gating cost is PORTING the scattered TU's MWCC source so the compiled obj DEFINES each method byte-exact. wave-16: ported MWCC->MSVC bodies DIVERGE from retail (BandProfile 0/64 reached 100%).
- Existing tools: tools/identity_transfer.py (700L, --tu --oracle --apply, case-A/B classify, STRICT add-only map), tools/locator.py (721L, high-conf per-method VA locator, 96.2% agreement w/ hand table), tools/gen_game_target_map.py (472L, VA->mangled). Oracle data: unified_id_rb3wii.json.
Write your research doc with concrete file:line references, real numbers, and a "GAPS / what to build" section. Markdown. Do NOT edit code or build anything that mutates the main tree; analysis + your one doc file only.`

const lanes = [
  {
    key: 'tooling-audit', file: '01-tooling-audit.md',
    task: `Audit the EXISTING identity-transfer tooling for end-to-end readiness. Read fully: tools/identity_transfer.py, tools/locator.py, tools/gen_game_target_map.py. Document for each: purpose, inputs/outputs, the exact case-A vs case-B discriminator logic, how locator.py finds VAs + its confidence model + how it relates to the BinDiff oracle (unified_id_rb3wii.json), and the precise integration seams. Then the KEY deliverable: the GAP LIST — what is missing to run a single scattered TU fully automatically end-to-end (identify methods -> locate VAs -> emit micro-pins + map -> build -> measure -> honesty-gate). Which steps are still manual? Is there a driver script or is it piecemeal? What would a robust 'idtransfer harvest <TU>' command need.`,
  },
  {
    key: 'objdiff-fork', file: '02-objdiff-caseb-fork.md',
    task: `Audit the objdiff case-B fork and produce a concrete integration + validation plan. Read docs/decomp/handoff/objdiff-caseb-fork-banked.md, then inspect the actual diff in ../objdiff branch caseb-global-byteeq vs its merge-base (git -C ../objdiff diff <base>..caseb-global-byteeq --stat, and read the report.rs / code.rs / diff mod.rs changes). Document: exactly what the global byte-eq 2nd pass does, the honesty gate (masked-bytes + reloc-NAME + oracle sim>=0.5), the CLI surface (--global-byte-eq[-oracle][-log]). Then the DO-NO-HARM VALIDATION: build the fork to /tmp (cargo build --release --target-dir /tmp/objdiff-fork-target -p objdiff-cli in ../objdiff) and verify forked objdiff-cli WITHOUT --global-byte-eq produces byte-identical report.json to stock on the current rb3-xenon build (this is the strict-superset gate). Report whether it passes. Produce the step-by-step integration checklist (what to rebuild/wire, behind which flag) and the risks.`,
  },
  {
    key: 'backlog-inventory', file: '03-backlog-inventory.md',
    task: `Inventory the ICF-scattered TU backlog and estimate EV. Using the oracle (unified_id_rb3wii.json) + tools/locator.py + tools/identity_transfer.py (dry-run, no --apply) + Ghidra/BinDiff if useful, enumerate the scattered game TUs (known: BandProfile, SongSortNode, SongSort, LockStepMgr, MainHubPanel — find MORE). For each TU report: total methods, case-A count (unowned auto_ blob) vs case-B count (inside a foreign pin), oracle coverage (how many have a VA+name), estimated REAL-BODIED methods (>44B, not ICF-stub class), and whether rb3-Wii source exists to port. Rank by EV (real-bodied case-A first = cheapest). Build a table. State the honest total ceiling and the cheap near-term subset.`,
  },
  {
    key: 'sourceport', file: '04-sourceport-bottleneck.md',
    task: `Analyze the SOURCE-PORT bottleneck — the real gate. wave-16 showed ported MWCC->MSVC bodies diverge from retail (BandProfile 0/64 at 100%). Investigate WHY and how to mitigate. Read the rb3-Wii oracle sources (../rb3/src) for 2-3 scattered TUs (e.g. SongSortNode, BandProfile), compare against what retail expects. Document: the divergence root causes (STL container choice, inlining policy, struct layout, MWCC vs MSVC codegen), which TUs are MOST portable (smallest/least-STL = highest yield), whether per-method partial porting helps (port only the methods that match, skip divergent ones), and whether locator.py lets us skip porting for some cases. Deliverable: a portability ranking + a concrete porting playbook to raise the hit-rate above wave-16's 0/64.`,
  },
]

phase('Research')
const research = await parallel(lanes.map((l) => () =>
  agent(
    `${COMMON}\n\n## YOUR LANE: ${l.key}\n${l.task}\n\nWrite your doc to ${DOCDIR}/${l.file} (mkdir -p the dir). Return a 6-12 line summary of your key findings + the gap/recommendation, for the synthesis step.`,
    { label: `research:${l.key}`, phase: 'Research' }
  ).then((txt) => ({ key: l.key, file: l.file, summary: txt }))
))

const ok = research.filter(Boolean)
ok.forEach((r) => log(`research lane done: ${r.key}`))

phase('Synthesize')
const summaries = ok.map((r) => `### Lane ${r.key} (${DOCDIR}/${r.file})\n${r.summary}`).join('\n\n')
const design = await agent(
  `${COMMON}\n\n## SYNTHESIS\nThe 4 research lanes are done. Their docs are in ${DOCDIR}/ and their summaries follow. Read all 4 docs, then write the MASTER DESIGN + IMPLEMENTATION PLAN to docs/decomp/identity-transfer/PIPELINE-DESIGN.md.\n\nThe design must specify the end-to-end 'identity-transfer harvest pipeline': the tool/workflow architecture, every phase (identify -> locate -> micro-pin+map -> build -> case-B forked-objdiff -> honesty-audit -> composed-verify -> land), the honesty gates at each step, what NEW code to build vs what already exists (reuse identity_transfer.py/locator.py/gen_game_target_map.py), the objdiff-fork integration decision, and an EXPLICIT statement on the jeff question (no jeff changes needed — confirm or refute with evidence). End with a PRIORITIZED, SEQUENCED implementation backlog (what to build first, with EV) suitable for driving an implementation workflow. Be concrete and decisive.\n\n=== research summaries ===\n${summaries}\n\nReturn a tight executive summary (the recommended build order + the jeff verdict) for the orchestrator.`,
  { label: 'synthesize:design', phase: 'Synthesize' }
)
return { researchLanes: ok.map((r) => r.key), design }
