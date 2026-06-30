# Near-miss codegen inventory — body-divergence wall #2 (2026-06-30)

**Mission:** determine whether a stronger permuter / codegen-aware tooling can
close a meaningful fraction of the 90–99.99% near-miss pool that is stuck on
COMPILER CODEGEN differences (register allocation, scheduling, instruction
selection, inlining policy) the existing source permuter cannot reach. Build it
only if the diagnosis says a reachable class exists.

**STEP 1 is the deliverable: a hard-numbers reachability inventory.** A clean
KILL with that inventory is an explicitly valid outcome.

## Tooling built this pass (read-only, reusable)

- `tools/classify_nearmiss_codegen.py` — batch-runs the freeqaz objdiff fork's
  `--analyze --verdict` pattern detector over every near-miss, strips the
  source-immune NOISE patterns (LINKER_MERGED = ICF, ADDRESS_RELOCATION_NOISE =
  .text layout — which report.json already discounts), and assigns a PRIMARY
  codegen class from the residual + the fork's own fixability label. REGISTER_SWAP
  is split GPR/FPR (from `details.swaps` reg names) and volatile/callee (the
  fork's own Fixability split: callee-saved = decl-order/MaybeFixable, volatile =
  scheduling/RarelyHandFixable). Output: `/tmp/claude/nearmiss_inventory.jsonl`.
- `tools/enrich_unattributed.py` — for the no-pattern UNATTRIBUTED bucket, pulls
  the per-instruction diff and sub-buckets the residual into CALL_NAMING (unnamed
  target-side `bl fn_XXXX` — resolves by naming, not codegen), PEEPHOLE
  (opcode-level instruction selection), REGALLOC (missed reg-only diff), IMM_OFFSET
  (immediate/displacement), BODY (insert/delete = code divergence).
- `tools/split_imm_offset.py` — splits IMM_OFFSET into stack/struct/const context.

## The pool (report.json `match_percent_normalized`, strict 10664/65568)

1811 functions in [90, 99.99%). **1096 of them are size-40 funclet/stub
artifacts** (uniformly ~99.9%, no real body) — not real codegen near-misses.
**715 are real-bodied.** Inventory below is the 715.

## Real-bodied near-miss classification (715)

| primary class | count | named | reach label | in codegen scope? |
|---|---:|---:|---|---|
| UNATTRIBUTED (no fork pattern) | 562 | 220 | (sub-split below) | mixed |
| REGALLOC_GPR_CALLEE | 55 | 47 | permuter_decl | yes (in principle) |
| CONTROL_FLOW | 20 | 19 | scheduling | mostly no |
| REGALLOC_FPR_VOLATILE | 17 | 17 | scheduling | no |
| REGALLOC_GPR_VOLATILE | 15 | 13 | scheduling | no |
| BOOL_MASK | 8 | 7 | permuter_bool | yes |
| STRUCT_OFFSET (OFFSET_SWAP) | 7 | 7 | header_lever | no (header) |
| REGALLOC_MIXED_CALLEE | 6 | 6 | permuter_decl | partial |
| BUILD_ENV | 6 | 6 | obj-patcher | no |
| NOISE_ONLY (ICF/reloc residual) | 6 | 2 | source_immune | no |
| REGALLOC_FPR_CALLEE | 5 | 5 | permuter_decl | yes (in principle) |
| COMMUTATIVE | 3 | 3 | permuter_commute | yes |
| REGALLOC_MIXED_VOLATILE | 3 | 3 | scheduling | no |
| INSTR_SELECT_CMP | 2 | 0 | instr_select | no (internal) |

### UNATTRIBUTED (562) sub-split — `tools/enrich_unattributed.py`

| sub | count | named | nature |
|---|---:|---:|---|
| IMM_OFFSET | 359 | 131 | struct-member-offset / literal-const / stack-slot deltas |
| PEEPHOLE | 162 | 53 | opcode-level instruction selection (compiler-internal) |
| BODY | 25 | 24 | genuine code/logic divergence (body-port lever) |
| REGALLOC (detector-missed) | 14 | 10 | sub-threshold reg-only swap |
| OTHER/CALL | 2 | 2 | — |

PEEPHOLE opcode transitions (target→base), top: `rlwinm→clrrwi` 89,
`addi→lwz` 13, `cmplwi→extsb.` 9 (the known strcpy NUL-test wall),
`addi→li` 6, `lwz→mr` 5, `mr→li` 5, `cmplw→cmpw` 4 … — all equivalent
instruction-selection choices the compiler made internally.

IMM_OFFSET examples: `EntityUploader::ctor` = `addi r3,r31,0x40` vs `0x41`
(one struct member 1 byte off = header lever); `BandCamShot` fns = `0x1b4` vs
`0x1b8` member offset. These are struct-layout / constant diffs, **not**
regalloc/scheduling/instruction-selection — i.e. the header/struct lever that is
explicitly OUT of the codegen-mission scope (and largely already mined).

## Permuter capability — empirical record (`permuter_cache.db`)

The decomp_synth permuter is **far richer than the mission brief assumed**: 143
`*Pattern` driver classes, INCLUDING the ones the brief thought were missing —
`commutative_swap`, `fpr_cascade_operand_hoist`, `fma_reorder`,
`declaration_reorder`, `declaration_movement`, `statement_reorder`,
`assignment_reorder`, `first_use_reorder`, `fpr_declaration_reorder`,
`ternary_swap`, `argument_swap`. (`fpr_declaration_reorder` and
`first_use_reorder` exist as classes but are NOT in the active scan set → 0 runs.)

History: 821 climbs, 88,542 variants tried, 11,465 per-driver pattern_runs.
**Only 11 functions have EVER been closed to strict-100%** — and every one was a
`statement_reorder` / `variable_extraction` / `assignment_reorder` win on a
**structural** near-miss starting at 81–96% (deltas 3.6–18.3% = body/source
fidelity, not fine regalloc). Per-driver wins toward 100 in the fine band:

| driver | runs | wins | max Δ |
|---|---:|---:|---:|
| statement_reorder | 485 | 38 | 12.73 |
| variable_extraction | 404 | 19 | 100.0 |
| assignment_reorder | 83 | 10 | 4.12 |
| declaration_reorder | 206 | 8 | 6.58 |
| argument_swap | 254 | 5 | 6.04 |
| commutative_swap | 38 | 1 | 6.36 |
| fma_reorder | 91 | 1 | 5.61 |
| fpr_cascade_operand_hoist | 2 | 1 | 3.25 |
| fpr_declaration_reorder | 0 | 0 | — |
| first_use_reorder | 0 | 0 | — |

**No driver has ever closed a 99%+ fine callee-register-swap near-miss to
strict-100.** The codegen-specific drivers (FPR/commutative/fma) exist and have
been exercised, with ~0 strict-100 wins.

## Pilot reachability test (DONE — full permuter, no-apply, real budget ~800s)

| pilot | swap | runs(all-time) | result |
|---|---|---:|---|
| MidiParser::PushIdle | r27↔r28 GPR callee | 185 | **NO improvement** (99.74% ceiling) |
| BandCharacter::FastInvert | f30↔f31 FPR callee | — | **NO improvement** |
| Geo::CheckBSPTree | 7 FPR pairs | 231 | **+0.009%** (99.024→99.033; ≈0, never >99.03%) |
| SongUpgradeMgr::ContentDiscovered | r26↔r27 GPR | — | did not resolve (skipped) |

**0 of the pilots reached strict-100%**, including the canonical PushIdle — the
mission's explicit KILL signal. Note "BSF mode: fallback — no GPR swap pairs"
on the FPR pilots: the best-swap-finder only models GPR swaps, not FPR.

## VERDICT — KILL (high confidence). The inventory IS the deliverable.

**No codegen class clears the decision gate** (meaningful count AND plausible
reachability by a NEW buildable transform that does not already exist-and-fail).
Independently validated by 5 read-only sampling agents (PEEPHOLE 25, IMM_OFFSET
23, REGALLOC 18, COMMUTATIVE/BOOL/CONTROL/BODY 18) + permuter-history recon.

Consolidated reachability of the 715 real-bodied near-misses (+1096 funclet
artifacts = 1811 total). "reach-by-NEW-codegen-tool" is the mission's question:

| class | count | reach by NEW codegen tool | what it actually is |
|---|---:|---:|---|
| FUNCLET_ARTIFACT (size-40) | 1096 | 0 | body-less ICF/funclet pairing artifact — not codegen |
| IMM_OFFSET | 359 | 0 | struct member-offset deltas = EXISTING header lever in disguise (23/23 sampled off a pointer reg, never r1/stack) |
| PEEPHOLE | 162 | 0 | 89 = `rlwinm→clrrwi` guard-thunk ICF mispair vs FOREIGN static (dead); 9 = strcpy `cmplwi→extsb.` wall (dead); ~50 scheduling/signedness (≈0 yield); ~8 mislabeled struct/body → existing levers |
| REGALLOC_GPR_CALLEE | 55 | 0 | swap is a SYMPTOM; ~30% ICF-fold aliases ("symbol not found in target", the r23↔r24 `_M_insert_overflow_aux` family = ONE folded STL body ×15 units); rest = live-range/pressure fixed points (decl-reorder changes ORDER not COUNT) |
| BODY | 25 | 0 | genuine oracle body divergence → EXISTING bodyport-batch workflow, not codegen |
| CONTROL_FLOW | 20 | 0 | scheduling / mislabel |
| REGALLOC_FPR_VOLATILE | 17 | 0 | volatile f0-f13, scheduling-driven, source-immune |
| REGALLOC_GPR_VOLATILE | 15 | 0 | scheduling-driven |
| BOOL_MASK | 8 | 0 | redundant `clrlwi` — same value-range family as the strcpy wall |
| STRUCT_OFFSET | 7 | 0 | header lever |
| REGALLOC_FPR_CALLEE | **5** | **0–3** | the ONLY new-tool-shaped class; f14-f31 are sequential-by-float-decl-order (no graph coloring) so theoretically solvable — but volatile-f0-f13-contaminated + multi-pair over-constrained ⇒ realistic ~0 |
| (others: MIXED/COMMUTATIVE/INSTR_SELECT/BUILD_ENV/NOISE) | ~40 | 0 | mislabel / patcher / source-immune |

**Why KILL (hard numbers):**
1. The big-count classes are **not codegen**: IMM_OFFSET+STRUCT_OFFSET (366) are
   the existing header-struct/DC3-drift lever (out of scope, thinning,
   regression-gated); 1096 are body-less funclet artifacts.
2. The codegen-shaped classes are **either compiler-internal-dead** (PEEPHOLE: 98/162
   hard-dead = guard-thunk ICF + strcpy wall; BOOL_MASK; INSTR_SELECT signedness)
   **or permuter-class with proven near-zero yield**.
3. The fine-regalloc GPR residue is **mechanistically source-UNREACHABLE**: it lives
   in c2.dll's coalescing/recoloring phase keyed on live-range, which declaration
   order provably cannot perturb (`declaration_reorder` scored 0/10 on the
   both-stuck bucket; PushIdle 185 runs / 0 wins / 99.74% ceiling).
4. The REGALLOC_*_CALLEE counts are **ICF-inflated** (~30% folded-alias artifacts).
5. The only new-tool-shaped allocator class (REGALLOC_FPR_CALLEE) has **count 5** —
   not meaningful — with realistic yield 0–3.

**The permuter premise in the mission brief was false.** decomp_synth already has
130 wired drivers (incl. `commutative_swap`, `fma_reorder`, `fpr_cascade_operand_hoist`,
`declaration_reorder`, `statement_reorder`); across 88k variants / 11,465
pattern_runs it has closed only ~11 functions to strict-100, ALL on 81–96%
STRUCTURAL near-misses — **zero in the 99% fine-regalloc band.** A new codegen
driver for wall #2 would have an addressable population of ~0.

## Recommended follow-ups (NOT a new codegen tool; for the owner / matching waves)

1. **Re-route the ~16–22 MISLABELED near-misses** (PEEPHOLE/IMM_OFFSET/REGALLOC
   entries whose real residual is a struct-size/member-type delta or a logic/guard
   body divergence) into the EXISTING `grind-execute` (header-struct lever) and
   `bodyport-batch` workflows. This is the only real residual matching value and
   it is a few-line classifier reroute, not a build. The cached inventory
   (`/tmp/claude/nearmiss_inventory.jsonl` + `unattributed_enriched.jsonl`) already
   tags them.
2. **Optional, low-EV, do-no-harm:** one bounded pass wiring the two dormant
   permuter drivers `fpr_declaration_reorder` + `first_use_reorder` (0 runs each)
   into the active scan set, targeting the 5 REGALLOC_FPR_CALLEE (CheckBSPTree,
   FastInvert, Rot::MakeScale…). Predicted harvest 0–3; touches the shared
   `../decomp-synth` package, so isolate + do-no-harm validate. After it, declare
   the strict-codegen lever formally exhausted.

**Do NOT build a new codegen matcher/permuter driver for this wall.**
