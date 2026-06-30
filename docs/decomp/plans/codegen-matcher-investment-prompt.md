# Codegen-matcher investment — dedicated-session kickoff prompt (option B)

Self-contained kickoff for a dedicated session exploring whether a stronger permuter / codegen-aware
tooling can crack the body-divergence wall. Written 2026-06-30 after a long session exhausted every
cheap/moderate matching vein. Copy the fenced block below into a fresh session (it also reads CLAUDE.md).

```
You are the coordinator for a focused, HIGH-RISK research investment in the rb3-xenon decomp
(/home/free/code/milohax/rb3-xenon). Read CLAUDE.md first. Use ultracode (the Workflow tool) and
delegate implementation to subagents — you keep selection/verification/landing. Current state:
main ~10664/65568 strict matched, build green.

## MISSION
Attack "body-divergence wall #2": many functions are near-miss (90–99.9% fuzzy = would-be strict
matches) but stuck on COMPILER CODEGEN differences — register allocation (GPR/FPR), instruction
scheduling, instruction-selection peepholes, inlining policy — that the existing SOURCE permuter
cannot reach. Determine whether a stronger permuter and/or codegen-aware tooling can close a
meaningful fraction, and build it if the diagnosis says yes.

## HONEST CONTEXT — read before committing effort; EV is UNCERTAIN and possibly LOW
This is the ONLY strict-matching lever left after the cheap frontier was rigorously exhausted. Hard
evidence that some of these walls are compiler-INTERNAL and unreachable by ANY source/flag/permuter:
- The decomp_synth permuter has a GPR-swap driver but gets ZERO wins on its own GPR targets (e.g.
  MidiParser::PushIdle 99.74%, r27<->r28). It has NO FPR-swap or scheduling driver.
- The inlined-strcpy NUL-test wall (retail `cmplwi rN,0x0` UNSIGNED vs our `extsb. rM,rN` SIGNED) was
  proven source-UNREACHABLE (~12 source forms), flag-UNREACHABLE (/J test KILL: -18 net, our base
  cflags already == the dc3 oracle exactly), and permuter-UNREACHABLE — it is internal to the X360
  strcpy intrinsic codegen. Full writeup: docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md.
So the open question is what FRACTION of near-misses is reachable, NOT whether the permuter "works."

## STEP 1 — DIAGNOSE FIRST (do NOT build blind). Ultracode fan-out:
1. Parse build/45410914/report.json for ALL functions at 90.0–99.99% (the field is
   `match_percent_normalized` or `fuzzy_match_percent`; recover VA from the `fn_82XXXXXX` name or via
   scripts/target_symbol_map.json — do NOT use the decimal `address` field).
2. Classify each (or a large stratified sample) via mcp__orchestrator__run_diff_inspect + the
   /compare-asm, /recon, /stack-layout skills into: STRUCT-OFFSET (header lever — OUT OF SCOPE),
   REGALLOC-GPR, REGALLOC-FPR, SCHEDULING, INSTRUCTION-SELECTION (peephole: extsb/cmplwi, mr vs cmplwi
   — likely compiler-internal/UNREACHABLE), INLINING-POLICY (callee inlined one side only),
   DATA/CONSTANT.
3. DELIVERABLE: counts per class + a reachability estimate per class. DECISION GATE: is there a class
   with (a) meaningful count AND (b) plausible reachability by a buildable transform? If the dominant
   classes are instruction-selection/compiler-internal -> KILL B, the inventory IS the deliverable
   (it definitively answers the strict-matching question).

## STEP 2 — BUILD (only if STEP 1 finds a reachable class). Candidate directions:
A. PERMUTER DRIVERS the current one lacks: FPR-swap, instruction-scheduling perturbation, expression
   reassociation, commutative-operand swap. Locate the machinery first: permuter_cache.db,
   permuter_targets.{json,txt}, tools/permuter_targets.py, tools/refresh_permuter_db.py, docs/permuter/,
   docs/plans/permuter-readiness.md, docs/plans/permuter-sweep-struct-cascades-2026-05-29.md.
B. OBJECT-LEVEL regswap/transplant: scripts/obj_regswap_patcher.py + scripts/obj_transplant_patcher.py
   already exist but are UNWIRED (see CLAUDE.md "Obj patchers"). They rewrite register allocation /
   transplant matched code at the COFF level — directly attacking GPR/FPR-regalloc where the SOURCE
   permuter fails. ⚠ FIRST confirm with the owner + the objdiff/dtk framework whether an obj-level
   regswap counts as an HONEST match (it post-processes the obj, not the source) — this is a judgment
   call, do not assume.
C. CODEGEN-AWARE MATCHER: a tool that, for a near-miss, identifies the minimal transform that closes
   it and reports reachable/unreachable.

## PILOT TARGETS (known near-misses, present in report.json today)
- INSTRUCTION-SELECTION (negative controls — expected UNREACHABLE): BandCharacter::OnChangeFaceGroup
  98.72%, FirstSortChar 98.85%, CharUtlFindBone 99.15% (all the strcpy extsb/cmplwi wall).
- FP-REGALLOC: Waypoint::ShapeDeltaBox 99.08%, Geo::CheckBSPTree 99.02%.
- GPR-SWAP: MidiParser::PushIdle 99.74% (r27<->r28) — the existing permuter's unbeatable target. If a
  new transform closes THIS, the approach has legs; if it can't, that's a strong KILL signal.

## KILL CRITERIA
- STEP-1 diagnosis dominated by compiler-internal/unreachable classes -> KILL, deliver the inventory.
- A built transform closes 0 of ~10 pilot targets (incl. MidiParser::PushIdle) -> KILL.
- A clean kill with a hard-numbers reachability inventory is a VALID, valuable deliverable.

## DISCIPLINE (non-negotiable)
- Composed whole-binary A/B before any claimed win: `rm -f build/45410914/*/target_symbol_renames.stamp;
  touch config/45410914/config.yml; tools/fresh_report.sh` re-run twice (run1==run2 deterministic), 0
  unexplained regressions. true-100 byte-equal only; NEVER commit a partial.
- tools/icf_alias_check.py (no <=44B stub-fold inflation). tools/fuzzy_progress.py for fuzzy context.
- CoW worktrees (scripts/setup_worktree.sh); NEVER mutate main; land via scripts/harvest/land.sh + the
  wave-loop SOP (docs/decomp/handoff/wave-loop-SOP-2026-06-20.md). After EVERY land, re-run configure.py
  and grep for "Missing configuration for <TU>" (the cross-agent objects.json-drop hazard — it silently
  zeroes a landed wave; detect/fix in docs/decomp/research/2026-06-22-classA-tupure-harvest-results.md).

## REFERENCE DOCS
- docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md — the strcpy wall + /J kill (START HERE).
- docs/decomp/MSVC_X360_REGALLOC.md, docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md,
  docs/decomp/TECHNICAL_NOTES.md, docs/decomp/PRAGMA_*.md, docs/decomp/patterns/ — MSVC X360 codegen.
- docs/decomp/research/2026-06-30-topo-locator-design.md — the identification-wall verdict (context for
  why this codegen vein is the only remaining strict lever).
- scripts/wf_bodyport_tails.js — the body-port near-miss workflow (scout->classify->port pattern to reuse).
- Memory: project_rb3_xenon_roadmap.md + the MEMORY.md "HARD-FRONTIER TOOLING INVESTMENT" line.
```
