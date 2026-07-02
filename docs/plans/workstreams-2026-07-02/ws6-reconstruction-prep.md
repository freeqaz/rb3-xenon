# WS6 — Reconstruction-era prep (post-lever): fuzzy metric adoption + the manual workbench

**Written 2026-07-02. Status: PREP PLAN — this stream runs AFTER ws1–ws5 (sysnet
drain, worklist regen, option-C, round-3 repairs, case-B campaign). Nothing in
this doc executes now; it defines the tooling, metric, seeds, and triage for the
era when the cheap identity/lever veins are drained and per-function manual
reconstruction is the frontier.**

Master doc: `/home/free/code/milohax/rb3-xenon/docs/plans/frontier-workstreams-2026-07-02.md`
(stream 6 row). Read `CLAUDE.md` at the repo root first, especially the
git/worktree hard rules (shared main tree, never stash/checkout/reset; worktrees
via `scripts/setup_worktree.sh` under `~/tmp`).

---

## Objective

Three deliverables, in order:

1. **Adopt the tiered fuzzy metric as an official SECOND headline** (strict
   stays the immutable north star) — so a reconstruction that takes a function
   9% → 85% registers as measurable progress instead of zero.
2. **Build the per-function reconstruction workbench** — one dossier-assembling
   tool + an agentic reconstruct→permute→verify loop, distilled from
   `docs/decomp/plans/fuzzy-locator-reconstruction-design.md` into a concrete
   build spec (that doc's Stage-1 locator lessons are baked in as hard gates).
3. **Seed the manual era honestly**: the 146-fn `crossval_agree.json`
   high-precision labels as the anonymous-wall entry points, plus a triage that
   sends effort at the right first targets (spoiler: the "anonymous near-miss"
   pool is 93% ≤44 B ICF stubs — the real classes are elsewhere; see §5).

## Current state (all numbers LIVE-VERIFIED 2026-07-02 — re-derive before executing)

**The repo moves fast: the master doc (written earlier today) says main
@44f57c6, 10,870/65,596 fns, 8.32% strict code, 11.46% fuzzy. By the time this
doc was verified (same day), main was @f83045e and the live report read
10,934/65,607, 8.418%, 11.569%.** Every number below is a snapshot; a cold
agent MUST re-derive with the commands given rather than trust either figure.

Re-derivation command (run from `/home/free/code/milohax/rb3-xenon`):

```bash
python3 tools/fuzzy_progress.py            # the whole headline block
python3 tools/fuzzy_progress.py --json     # machine-readable
```

Verified snapshot (main @f83045e, `build/45410914/report.json`):

| Figure | Value |
|---|---|
| STRICT functions | 10,934 / 65,607 (16.666%) |
| STRICT code bytes | 931,004 / 11,059,780 (8.418%) |
| FUZZY-CODE whole binary | 11.569% |
| FUZZY-CODE wired set | 94.560% over 1,353,060 attempted bytes (n=13,327 fns) |
| Fuzzy-credit bytes (strict discards) | +348,537 |
| Staircase | ≥100: 10,934 · ≥95: 12,504 · ≥90: 12,783 · ≥80: 12,928 · ≥50: 13,151 |
| Near-miss [95,100) | 1,570 total = **324 named** (59 rb3-tier + 265 engine-tier) + **1,246 anonymous `fn_`** |
| Anonymous near-miss composition | **1,156 of 1,246 are ≤44 B** (ICF guard/thunk stubs @~99.8–99.9%); only **90 are >44 B real bodies** |
| RB3-specific tier (band3/network) | 5,854 fns · 3,279 wired · 2,714 @100 · wired-fuzzy 90.96% · 2,575 unwired |
| Engine tier (src/system) | 17,375 fns · 10,048 wired · 8,220 @100 · wired-fuzzy 95.67% · 7,327 unwired |
| Other tier (thirdparty/vendor/xdk) | 42,378 fns · **0 wired** |

File-existence verification (all confirmed present 2026-07-02):

- `tools/fuzzy_progress.py` — **tracked, committed** (`bdbaee5` + land-gate
  metric fix `2428db0`). 370 lines, NOT the "~40-line promotion TODO" the June
  frontier doc describes — it already has `--json`, `--baseline` (wave delta),
  `--by-unit` (fuzzy headroom ranking), `--min-size` (stub-excluded staircase
  preview), the rb3/engine/other tier sub-goals, the credit ledger, and
  anti-gaming documentation in the module docstring. **The tool is DONE; what
  remains is WIRING it into the reporting loop (§3).**
- `tools/fresh_report.sh` — the canonical fresh-report driver. Currently prints
  only strict counts (`measures.matched_functions`) + the warm-cache-trap
  warning. **Does NOT print the fuzzy block** — that is the §3 wiring gap.
- `docs/decomp/gameid/crossval_agree.json` + `VERDICT.json` — verified: 146
  agree fns; 93 unpinned / 53 already-pinned; **57 unpinned AND >44 B**
  (real-bodied seeds); sizes 68 ≤44 B / 78 >44 B. Meta: BinDiff(conf≥0.7) ∩
  BSim(sim≥0.5), stub+BandMachineMgr filtered, 0.95 per-fn precision (18/19 on
  the 25-pin calibration set).
- `unified_id_rb3wii.json` (9,301 pairs) + `ghidriff_identities.json` (978
  ACCEPT) — both at repo root.
- `tools/classify_nearmiss.py`, `tools/true_progress.py`,
  `tools/icf_alias_check.py`, `tools/identity_transfer.py`,
  `tools/reveal_sweep.py`, `tools/gen_game_target_map.py`, `scripts/recon.py`,
  `scripts/analysis/diff_inspect.py`, `scripts/target_symbol_map.json` — all present.
- `tools/locator.py` + `tools/topo_locate.py` — present, tracked; topo-locator
  is KILLED as a locator (held-out precision@1 = 0.13) but banked as a one-shot
  confirmer (precision 1.0 at vote_margin≥2 only) — see
  `docs/decomp/research/2026-06-30-topo-locator-design.md`.
- Ghidra MCP (port 8002): responding at verification time
  (`curl -s -m 2 -o /dev/null -w '%{http_code}' http://127.0.0.1:8002/mcp` → 406,
  i.e. server up, wants MCP headers). Restart via
  `tools/ghidra/pyghidra-service.sh` if down. **Single-process — serialize.**
- Permuter: `python -m decomp_synth` from
  `/home/free/code/milohax/decomp-synth/` (docs: `docs/permuter/INDEX.md`,
  `docs/permuter/guided-permuter.md`, `docs/permuter/bsf-engine.md`).
- `scripts/orchestrator/decomp.db` — at verification an **empty untracked
  placeholder (zero tables)**. Regenerate before workbench use:
  `venv/bin/python scripts/ingest_report.py build/45410914/report.json`.
  Consequence: **unicorn behavioral columns are unpopulated** — behavioral
  equivalence is NOT an available "done-fuzzy" criterion yet (matches the June
  doc's Rank-7 status; still true).

### Stale-claims ledger (June docs → now)

| June claim (frontier/design docs, 2026-06-21) | 2026-07-02 reality |
|---|---|
| 9,793 / 65,547 fns (14.94%), strict code 7.56%, fuzzy 10.07% | 10,934 / 65,607 (16.67%), 8.418%, 11.569% — **+1,141 fns landed since**, mostly the ghidriff identity pipeline + worklist drain (see memory `project_worklist_drain_close_2026-07-02.md`) |
| "promote `fuzzy_progress.py` (~40 lines)" | Tool fully built + committed (`bdbaee5`, `2428db0`); only reporting-loop wiring remains |
| Wired set n=11,744, wired-fuzzy 95.2% | n=13,327, 94.56% (denominator grew faster than fuzzy credit — new wirings start imperfect; this is expected and healthy) |
| "ALL seed channels refuted for class-B" (consolidated verdict) | **Partially superseded**: the forked ghidriff+BSim pipeline (built AFTER that verdict) produced 978 ACCEPT identities @0.900 precision and drained +232 band3 ids — a working identification channel the June verdict didn't have. Class-B *bulk* remains un-locatable, but the wall has a proven crack that ws2 (looser-tier regen) widens. Do NOT treat the June "unrecoverable" verdict as final until ws2 closes. |
| BandProfile 0/64, best 47.8% (wave-16) | Still the canonical body-divergence datum, BUT the design doc's pivotal finding stands: it was attempted on **mis-located** VAs (rb3-Wii BinDiff median sim 0.16 for BandProfile). Reconstruction economics on *confirmed* VAs are unmeasured — that measurement is this stream's pilot (§4.4). |

## Evidence & references (read before executing)

- `docs/decomp/fuzzy-reconstruction-frontier-2026-06-21.md` — the frontier
  writeup: wave-delta collapse (W9 +723 → W20 +1), 610 scattered TUs / 9,146
  fns, the §4 fuzzy-reframe spec (tiers, anti-gaming, acceptance bar), the
  EV-ranked path (§5).
- `docs/decomp/plans/fuzzy-locator-reconstruction-design.md` — the consolidated
  verdict: identification-not-reconstruction is the bottleneck; the SongSortNode
  pilot honest-negative (net 0; recon-gate validated); the class-A/class-B
  taxonomy; BSim∩BinDiff = the only 0.95-precision channel; topo-locator kill.
- `docs/decomp/gameid/VERDICT.json` — the crossval experiment record incl. the
  exact headless BinExport/BinDiff/BSim commands to regenerate the signals.
- `docs/decomp/handoff/wave-loop-SOP-2026-06-20.md` — the wave-close SOP the
  fuzzy block gets wired into.
- `docs/plans/meta_band-port-breaking-changes.md` — the MWCC→MSVC recurring-fix
  catalog (the workbench's port-fix checklist).
- `docs/decomp/MSVC_X360_REGALLOC.md`, `docs/decomp/patterns/` — codegen
  reference for the permuter-stuck class.
- Memory: `project_worklist_drain_close_2026-07-02.md` (v2 workflow: no
  whole-binary builds in lanes; warm-cache infra).

---

## Part 1 — Adopt the tiered fuzzy metric as the official second headline

### What "adopt" means (and does NOT mean)

- STRICT (`measures.matched_functions` + `matched_code` from
  `build/45410914/report.json`) **remains the only number called "matched"** —
  in commit messages, wave closes, memory, and the master doc. Immutable north star.
- FUZZY-CODE becomes a **co-equal reported line, never the success bar**: it
  appears wherever strict appears, always beside it, never alone.
- The **wired-denominator figure is the primary fuzzy GOAL** (94.56% now);
  whole-binary fuzzy (11.57%) is the secondary public figure. Rationale +
  anti-gaming guarantees are already written in `tools/fuzzy_progress.py`
  docstring lines 45–64 — treat that docstring as the normative spec.

### Step-by-step

1. **Determinism precondition (MUST pass before adoption).** The June doc
   flagged that `match_percent_normalized` stability across objdiff rebuilds is
   unchecked. Run:
   ```bash
   cd /home/free/code/milohax/rb3-xenon
   ./tools/fresh_report.sh && python3 tools/fuzzy_progress.py --json > ~/tmp/fz_a.json
   ./tools/fresh_report.sh && python3 tools/fuzzy_progress.py --json > ~/tmp/fz_b.json
   diff ~/tmp/fz_a.json ~/tmp/fz_b.json && echo DETERMINISTIC
   ```
   Expected: byte-identical. If not, diff the two `report.json` snapshots
   per-function to find the jitter source (objdiff fork nondeterminism) and fix
   in `../objdiff` before adopting. **Kill gate: do not adopt a jittery metric.**
   Caveat: run on an idle tree (concurrent agents recompiling objs will trip
   `fresh_report.sh`'s staleness warning — rerun if it fires).
2. **Wire the fuzzy block into `tools/fresh_report.sh`.** Append after the
   final strict summary (the `python3 - "$REPORT" ...` heredoc at the tail):
   ```bash
   python3 "$REPO/tools/fuzzy_progress.py" --report "$REPORT" | tee -a "$LOG"
   ```
   Keep strict printed FIRST. (~2-line patch; do it in a worktree, land via the
   normal patch flow — this file is shared by every agent.)
3. **Wave/lane close gets a fuzzy delta.** Update
   `docs/decomp/handoff/wave-loop-SOP-2026-06-20.md` close checklist: alongside
   the strict A/B, record
   `python3 tools/fuzzy_progress.py --baseline <pre-wave-report.json>` —
   report `STRICT functions Δ`, `fuzzy total_bytes Δ`, and `staircase ≥95/≥90 Δ`.
   **Gate on `fuzzy_code.total_bytes` (monotone-up-under-improvement) and the
   staircase — NEVER on `credit_bytes`**, which mechanically goes negative when
   a near-miss graduates to strict-100 (documented in `to_json()` comments,
   fixed in `2428db0`).
4. **Snapshot ledger.** Create `docs/decomp/progress-snapshots/` and at each
   wave/stream close commit `tools/fuzzy_progress.py --json` output as
   `YYYY-MM-DD-<label>.json` (~1 KB each). This is what makes "the fuzzy line
   moved" auditable across sessions without rebuilding old trees.
5. **Headline format everywhere** (master doc, memory, session closes) — three
   lines, exactly:
   ```
   STRICT     10,934 / 65,607 fns (16.67%) · 931,004 / 11,059,780 code B (8.42%)
   FUZZY      11.57% whole-binary · 94.56% wired (n=13,327) · +348,537 credit B
   STAIRCASE  ≥100: 10,934 | ≥95: 12,504 | ≥90: 12,783
   ```
6. **Acceptance-bar vocabulary** (from frontier doc §4.2, now official):
   - **strict-matched** — 100% normalized. The only thing counted in
     `matched_functions`.
   - **fuzzy-DONE** — RB3-specific fn ≥90% normalized whose residual diff is
     classified by `tools/classify_nearmiss.py` / `tools/true_progress.py` as
     regalloc / FP-scheduling / funclet / build-env (NOT logic). Tracked as a
     separate `logic-complete` tally in the snapshot ledger; never added to strict.
   - Engine (src/system) code gets **no fuzzy-DONE** — DC3 makes byte-exact
     achievable there; do not relax the bar where it is reachable.

### Honesty gates for Part 1

- Fuzzy never appears without strict beside it (no "we're at 94.5%!" quotes).
- Any *claimed* per-TU fuzzy gain must pass `tools/icf_alias_check.py`
  (commit `23bb6ee`) — ≤44 B ICF stub-folds read ~100% fuzzy without the pin
  owning the code; `--min-size 48` on `fuzzy_progress.py` is a preview, not the gate.
- The wired denominator is self-protecting (wiring junk at low % can only drop
  the wired figure), but watch for the inverse game: *un*-wiring bad units to
  raise wired-%. Rule: pins are removed only with a documented reason
  (the ws5 BaseSkeleton precedent).

---

## Part 2 — The reconstruction workbench (build spec)

### Design verdict baked in (do not re-litigate)

From `fuzzy-locator-reconstruction-design.md`: wave-16 failed because it
reconstructed against **mis-located** VAs (rb3-Wii BinDiff for class-B TUs is
near-random, median sim 0.16). The SongSortNode pilot proved the micro-pin +
fuzzy-measure mechanism works and the confidence gate correctly rejects fakes,
but identification from the Wii oracle alone is too weak. Therefore the
workbench **hard-gates on confirmed identity**: it refuses to open a dossier
for a function whose identity isn't from one of the proven channels (§2.2).

### 2.1 The tool: `tools/workbench.py`

One command, per function, that fuses the dossier and drives the loop. ~80%
component reuse — everything listed exists today at these paths:

**Inputs (the dossier):**

| # | Evidence | Source | Access |
|---|---|---|---|
| 1 | Confirmed identity (VA ↔ name ↔ source TU) | §2.2 channels | JSON lookups at repo root |
| 2 | rb3-Wii source body (logic template, game code) | `../rb3/src/<bindiff_src>` via `unified_id_rb3wii.json` `wii_name`+`bindiff_src` | file read; `mcp lookup_rb3wii` |
| 3 | DC3 source body (engine code ONLY — DC3 lacks band3/meta_band; false friend for game) | `../dc3-decomp/src` | `mcp lookup_dc3` |
| 4 | Ghidra retail decompilation (the ONLY behavioral ground truth for RB3-specific code) | `tools/ghidra/ghidra-decompile.py` → pyghidra MCP :8002, project `ghidra_projects/RB3Xenon/RB3Xenon` | **serialized** (see 2.4) |
| 5 | objdiff normalized diff of our compiled fn vs target | `mcp run_objdiff` / `run_analyze_function` (pass `project_dir=<worktree>`!) | per-fn, cheap |
| 6 | Per-instruction divergence buckets (NAME_RELOC/OFFSET/REG/OPCODE/…) | `tools/classify_nearmiss.py` | objdiff JSON post-process |
| 7 | Struct field-access map + workability | `scripts/recon.py` / `/recon` skill | per-fn |
| 8 | Stack-frame layout diff (decl-reorder candidates) | `/stack-layout` skill → `scripts/analysis/diff_inspect.py --stack-layout` | per-fn |
| 9 | Aligned asm with cluster/mismatch annotations | `/compare-asm` skill → `diff_inspect.py` | per-fn |
| 10 | Current fuzzy% + size | `build/45410914/report.json` | free |

**Output:** one markdown dossier per function at
`~/tmp/workbench/<mangled-or-addr>.md` (agent-consumable), plus a `--json` mode
for batch ranking.

**The loop (per function, in a `scripts/setup_worktree.sh` worktree under `~/tmp`):**

```
IDENTIFY (gate: §2.2 confirmed only)
  → RECONSTRUCT   edit MSVC source from the oracle template
                  (MWCC→MSVC fix catalog: docs/plans/meta_band-port-breaking-changes.md)
  → BUILD+DIFF    mcp run_objdiff with project_dir=<worktree>
  → CLASSIFY      classify_nearmiss buckets → route:
                    OFFSET cluster  → struct-lever (struct-info / lookup_struct_offset)
                    NAME_RELOC      → map/renamer fix (gen_game_target_map, renamer stamp refresh)
                    REG/scheduling  → PERMUTE: python -m decomp_synth --symbol … --source … --function …
                    OPCODE/logic    → back to RECONSTRUCT with Ghidra ground truth (#4)
  → VERIFY        icf_alias_check.py on the pin; whole-binary A/B via fresh_report.sh
                  in the worktree before any patch is proposed
  → RECORD        strict-match | fuzzy-DONE (+classify evidence) | WALLED (+root cause)
                  → report_result to the orchestrator MCP
```

**Never commits to main** — patches flow through the coordinator like every
bodyport skill.

### 2.2 Confirmed-identity channels (the hard gate)

A function may enter the workbench ONLY via:

1. **Already-pinned named near-miss** — it has a `scripts/target_symbol_map.json`
   entry and reads [50,100) in the report. Identity is settled; residual is
   body/codegen. (The 324 named near-misses; §5 class 1.)
2. **ghidriff ACCEPT tier** — `ghidriff_identities.json` (978 @0.900 precision)
   and its ws2 looser-tier successors, after the reviewer-reproduction gating
   ws1/ws2 use.
3. **BinDiff∩BSim crossval** — `docs/decomp/gameid/crossval_agree.json`
   (146 fns @0.95 precision; §4 below).
4. **topo_locate one-shot confirmation** — `tools/topo_locate.py` at
   `vote_margin>=2` ONLY (measured precision 1.0 there, useless below).
5. Raw rb3-Wii BinDiff sim ≥0.7 **with** ≥1 corroborating signal (string
   anchor or callee-set agreement). The sim~0.42 band is FORBIDDEN — it is the
   misattribution band that sank wave-16 and the SongSortNode pilot.

### 2.3 Build steps

1. [day] `tools/workbench.py` skeleton: dossier assembly from inputs 1–3, 5–10
   (all file/JSON reads + existing script invocations). `--fn <addr|symbol>`,
   `--json`, `--batch <list>`.
2. [day] Ghidra leg: reuse `tools/ghidra/mcp_client.py` (session cache
   `/tmp/claude/ghidra_mcp_session_rb3xenon.txt`) + `ghidra-decompile.py`;
   add the serialization wrapper (2.4).
3. [half-day] Route table: classify_nearmiss bucket → recommended next action
   (the CLASSIFY step above) printed in the dossier.
4. [day] Batch ranker: over a candidate list, emit an EV-ordered queue
   (size × fuzzy-headroom × identity-confidence) so agents drain best-first.
5. [pilot, §4.4] before any fan-out.

### 2.4 Ghidra serialization (required for fan-out)

Ghidra projects are single-process; concurrent agents get `ClosedException`.
Two acceptable patterns (pick one at build time):
- **flock wrapper**: `flock /home/free/tmp/ghidra8002.lock tools/ghidra/ghidra-decompile.py …`
  inside `workbench.py`'s Ghidra leg; or
- **one dedicated Ghidra agent** (the `gameid-crossval` skill precedent) that
  pre-exports decompilations for the whole batch via
  `tools/ghidra/batch_export.py` into `~/tmp/workbench/decomp/` before the
  reconstruction agents start (preferred — removes the runtime dependency).

---

## Part 3 — Seeding from `crossval_agree.json` (the 0.95-precision labels)

**What it is (verified):** `docs/decomp/gameid/crossval_agree.json` — 146
functions where BinDiff(conf≥0.7) and BSim(sim≥0.5) agree on the same source
stem, stub+BandMachineMgr-sink filtered; calibrated 18/19 correct on known
pins. Per `VERDICT.json`: useful as **per-fn labels for manual work, NOT
TU-span brackets** (max contiguous run = 3).

**The usable seed set:** 93 unpinned; of those **57 are >44 B real bodies**
(the rest are ICF-stub-sized — pin them and you match nothing you own). Top
stems in the 57: Buffer, DupSpaceOperation, FixedSizeSaveableStream,
RBDataDDL_Wii, UIPanel ×2 each; singletons incl. BandSongMgr, BandTrack,
BandWardrobe, BeatMatcher, GemManager, GemPlayer, CharBoneDir. The 53
already-pinned entries have verification value only.

### Procedure (per seed, inside the workbench)

1. **Dedup against ws1/ws2/ws5 first** (they run before this stream and will
   consume overlapping ids):
   ```bash
   python3 - <<'EOF'
   import json
   cv=json.load(open('docs/decomp/gameid/crossval_agree.json'))['agree_fns']
   tm=json.load(open('scripts/target_symbol_map.json'))
   gh={e.get('rb3_addr','').lower() for e in json.load(open('ghidriff_identities.json')) if isinstance(e,dict)} if True else set()
   live=[f for f in cv if not f['already_pinned'] and f['size']>44
         and f['addr'].lower().replace('0x','fn_8') not in {k.lower() for k in tm}]
   print(len(live)); [print(f['addr'],f['stem'],f['size']) for f in live]
   EOF
   ```
   (Adapt key formats after inspecting both files — `target_symbol_map.json`
   keys are `fn_<addr>` strings; verify the ghidriff entry schema before
   trusting the `gh` set. Re-check `already_pinned` against the CURRENT
   `config/45410914/splits.txt`, not the frozen 2026-06-09 flag.)
2. Micro-pin the VA (identity-transfer style N-range in `splits.txt` — see
   `tools/identity_transfer.py` + `docs/decomp/identity-transfer.md`; strict
   add-only map writes) in a worktree.
3. Locate/scaffold source: the stem names the TU; find the method in `../rb3`
   (game) or `../dc3-decomp` (engine stems like CharBoneDir, Buffer). Port with
   the breaking-changes catalog.
4. Name the pair: `tools/gen_game_target_map.py` entry (or manual
   `scripts/target_symbol_map.json` add) + the **mandatory renamer refresh**
   (`rm build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml`)
   or the new entry reads +0.
5. Reconstruct through the workbench loop; `icf_alias_check.py` + whole-binary
   A/B before proposing the patch.

**Expected:** 57 candidates → after ws1/ws2 dedup and port attrition, honest
estimate **+15–30 strict** plus fuzzy-DONE residue. These are the *only*
anonymous-wall entries with measured 0.95 identity precision — burn them in the
pilot (§4.4) before anything speculative.

---

## Part 4 — Triage: what per-function reconstruction attacks first

The prompt-level question: named 95–99.99% permuter-stuck vs the anonymous
identification wall. The verified composition answers it:

### Class 1 — named near-misses, [95,100): 324 fns (59 rb3 + 265 engine) — attack FIRST, but for LABELS not strict
Identity settled, source wired, residual is codegen. History says strict yield
is low: permuter-as-wave-driver was retired at wave-20 (0/all converged) and
the master doc lists it as a dead lever (spot tool only). So the play is:
run each through the workbench CLASSIFY step; the ones whose residual is
regalloc/FP-scheduling/funclet get **fuzzy-DONE / logic-complete** labels
(cheap, ~mechanical, makes the new metric immediately meaningful); the minority
with OFFSET/OPCODE residue are real bugs → fix to strict. Top units by
near-miss count (live): BandDirector 77, rndobj/Rnd 62, Dir 42, VocalPlayer 33,
BandWardrobe 31, Anim 28, LightPreset 27, PropKeys 27, HamCamTransform 26
(ws4's lever), FileMerger 26. Expected: **~200–300 logic-complete labels,
+10–30 strict** from the genuine-bug minority.

### Class 2 — anonymous near-misses, [95,100): 1,246 fns — mostly a MIRAGE, do not chase
Verified: **1,156 / 1,246 are ≤44 B** at ~99.8–99.9% — the ICF guard/thunk/
funclet wall (`EntityUploader` alone contributes dozens of 40 B `fn_824FB*`
stubs). Only **90 are >44 B**. Action: one afternoon to characterize the 90
(script: filter report.json for `fn_` + [95,100) + size>44, then
`lookup_merged_symbol` / reveal-sweep each — some are reveal-cascade freebies
where a byte-exact body just lacks a map entry). The 1,156 stubs are credited
correctly by the size-weighted fuzzy metric (~nothing) and must NEVER be
counted as reconstruction targets.

### Class 3 — the anonymous identification wall (52,246 fns @0) — gated on identity, in this order
1. **ws2 looser-tier ghidriff regen output** (identification is 10× cheaper
   than reconstruction; let ws2 finish before hand-work).
2. **The 57 crossval seeds** (§3) — the measured-precision entry points.
3. **Class-A string-anchored fresh cores** (~15 TUs, +15–40 honest est.) from
   the design doc's harvest list (GemPlayer 7-fn core @0x826966f0, ChordbookPanel
   5 @0x82691990, FreestylePanel, TrackPanelDirBase, RGTrainerPanel, PitchArrow,
   GemManager @0x82b6aac8 …) — note several stems overlap the crossval seeds
   (GemManager, GemPlayer): corroborated targets, do them together.
4. **Class-B bulk (SongSortNode/BandProfile/Campaign/panel STL)** — stays
   BANKED. Every locator channel measured to date fails on it (BinDiff
   near-random, BSim seed-prop degrades, strings absent, topology recall 0.13).
   Reopen ONLY if ws2's looser tiers or a future hand-seeded VT campaign
   produce confirmed VAs. Do not restart wave-16.

### Sequencing summary
Pilot (§4.4) → Class-1 label sweep (metric showcase) → Class-2 90-fn
characterization → Class-3 items 2–3 as identity supply allows. Class-3 item 4
banked.

### 4.4 The pilot (go/no-go for the whole workbench)

Before fan-out: take **10 functions** — 5 crossval seeds (>44 B, deduped) + 5
class-1 named near-misses with OPCODE/OFFSET residue — through the full loop,
one agent, one worktree. Success bar: **≥3 strict matches OR ≥6 fuzzy-DONE**
with zero regressions on the composed A/B. This measures reconstruction
economics on *confirmed* identities, the number wave-16 never produced.

---

## Honesty gates & verification (stream-wide)

1. Strict `measures.matched_functions` is the only "matched" claim; every
   composed patch needs a whole-binary A/B (`tools/fresh_report.sh` in the
   worktree; mind the warm-cache trap warning it prints).
2. `tools/icf_alias_check.py` (exit 1 = INFLATED) on every new/changed pin and
   every claimed fuzzy gain.
3. Fuzzy wave-gate = `fuzzy_code.total_bytes` + staircase deltas, never
   `credit_bytes` (negative-on-graduation artifact).
4. Determinism check (§Part 1 step 1) MUST pass before the metric is called
   official.
5. Identity gate: no reconstruction on anything outside the §2.2 channels; the
   sim~0.42 band is forbidden.
6. fuzzy-DONE requires attached `classify_nearmiss` output showing a non-logic
   residual; spot-audit 1-in-5 with `/compare-asm`.
7. Numbers in this doc are 2026-07-02 snapshots — re-derive all counts with the
   embedded commands before acting; other agents land daily.

## Kill criteria

- **Determinism check fails and the objdiff fix is non-trivial** → adopt only
  the staircase (integer counts, jitter-immune) as the second headline; defer
  the % lines.
- **Pilot (§4.4) under bar** (<3 strict AND <6 fuzzy-DONE from 10) → downgrade
  the workbench to a spot tool; the manual era's deliverable becomes the
  class-1 label sweep + metric only; class-3 items 2–3 return to the bank.
- **crossval seeds mostly consumed by ws1/ws2** (dedup in §3.1 leaves <15
  live) → skip Part 3, fold the survivors into the pilot.
- **Ghidra project lock contention unsolvable** → drop dossier input #4 to
  batch-pre-export-only (2.4 option b is then mandatory, not preferred).

## Expected yield

- Metric adoption: 0 fns directly; unlocks honest credit for ~348 K fuzzy-credit
  bytes and every future partial reconstruction. This is the era's steering
  instrument — highest EV per effort in the stream.
- Class-1 sweep: ~200–300 logic-complete labels, +10–30 strict.
- crossval seeds: +15–30 strict after attrition.
- Class-2 characterization: a handful of reveal freebies from the 90 real-bodied.
- Workbench: durable tooling that every subsequent manual session amortizes.
- Explicitly NOT promised: the June "~6–8k prize" (refuted; do not re-litigate),
  class-B bulk recovery.

## Open questions

1. The 90 real-bodied anonymous near-misses: reveal-cascade freebies, ICF
   merged bodies, or genuine unnamed bodies? (One-afternoon characterization,
   §4 class 2.)
2. Unicorn behavioral verifier: `decomp.db` empty here; is the DC3 runner
   portable in-repo? If yes, behavioral-EQUIVALENT becomes a third done
   criterion for MWCC-ported bodies (frontier doc Rank 7). Not load-bearing for
   this stream.
3. Does ws2's looser-tier regen change the class-3 seed pool enough to re-rank
   §4? (Re-run the §3 dedup after ws2 closes.)
4. Where does the fuzzy headline surface publicly (README vs master doc only)?
   Owner call; the anti-gaming rule (never without strict) applies either way.
5. `fresh_report.sh` logs to `/tmp/rb3_build_fresh_report.log` — contradicts the
   CLAUDE.md `~/tmp` rule; fix opportunistically when wiring §Part 1 step 2.
