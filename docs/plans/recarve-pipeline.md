# Recarve pipeline — programmatic TU-attribution repair

**Status (2026-07-12): Stage A LANDED + VALIDATED; Stage B built, E2E test in
progress.** Stage A scanner: `scripts/recarve/scan.py`; Stage B climber:
`scripts/recarve/climb.py`.

Stage A validation (2026-07-12, main @ 15,822 matched): the scanner
retroactively reproduces the wave-16 *human* candidate scan — BandDirector,
MeshAnim, VocalPlayer, BandCamShot, Rnd, VocalTrack, DirLoader, BandCharacter,
HamCamTransform all in the top ranks — and corroborates wave-23's independent
findings (VocalPlayer 242 unmapped = the "mass map-gap" refutation; VocalTrack
in top 25). Fresh discovery: **Waypoint.cpp has a 25-real-fn unowned EXTEND
run** (the +497/+116 pin audits did not fully drain the vein — most other
ext_fns are 0-3, so IN-UNIT/MAP-GAP signals now dominate the ranking, matching
where the manual waves ended up too). Queue: `~/tmp/recarve/scan.json`
(720 candidates).

Ops note (2026-07-12): `../objdiff/target/` was found deleted (breaks
setup_worktree.sh AND main's next build — build.ninja bakes the absolute
binary path); rebuilt via `touch objdiff-core/build.rs && cargo build
--release -p objdiff-cli` (the stale-build-script gotcha from
`project_objdiff_fork.md`).

### Stage B E2E findings (Waypoint.cpp, 2026-07-12)

The first full climb (worktree ~/tmp/recarve-wt-waypoint) proved the
mechanics — worktree setup, splits edit, re-split, fresh_report, per-fn
evaluation, measure_delta gate, evidence JSON, zero regressions — and taught
three lessons now baked into the tools:

1. **The funclet screen must be PROLOGUE-based, not size-based.** Waypoint's
   "25-real-fn run" was 24 EH unwind funclets (48B each — outside the wave-17
   40B band) + 1 genuine fn. `scripts/recarve/funclets.py` now detects the
   r12 parent-frame establisher (`subi/addi rX, r12, imm` as FIRST
   instruction) across the whole target asm tree: **16,821 funclet VAs
   (~24% of all functions!)** cached at `~/tmp/recarve/funclet_vas.json`.
   That population size explains why the "hi-fuzzy quick closes" mirage
   recurred across so many waves. Both scan.py (EXTEND run walk) and
   climb.py (walk + eval + trim glue) consume the set.
2. **Never reconstruct VAs from report.json fn offsets** — dtk compacts
   excluded symbols so offsets drift (observed -0x30 skew mid-unit). climb's
   unit_eval now pairs by NAME (mapped VA → mangled name via
   target_symbol_map, else `fn_<VA>`). This fix *demonstrably killed a false
   positive*: the offset-based first run reported `??_GWaypoint` at 99.95%
   "captured outside the pin" — but that fn is actually already pinned at
   0x822C8CA8 (Waypoint's second range); the offset skew had slid an
   already-matched fn into the evaluation window. The corrected run
   evaluates the same extension honestly: 24 funclets + 1 unmapped 0% fn =
   nothing good.
3. **Verdicts are graded**: KEEP (strict net > 0, landable), EVIDENCE
   (good non-funclet fns captured but net <= 0; boundary kept in worktree,
   yield pends Stage C), DEFER (no good non-funclet fns; extension
   reverted). Waypoint's final verdict is **DEFER** — both candidate ends
   processed, both reverted, net +0, zero regressions. The correct next
   probe for its runs is Stage C identity work (who owns fn_822C8B24 and
   the 24 funclets' parents), not a boundary move.

Stage C labeled set exported: `~/tmp/recarve/map_verify_labeled.csv`
(9,516 audited map entries; 8,819 ok, rest = MISPAIR/SUSPECT/… classes) via
`map_verify.py --all --csv`.

## Why this exists

The 2026-07-10 mega-session (~11,660 → 15,428 strict matches) showed that the
dominant wall is **attribution, not codegen**: retail functions are anonymous
(`fn_8XXXXXXX`), and the biggest single levers were all attribution repairs —

- **TU recarve**: BandWardrobe +174 (`ec0cd881`), StoreOffer +83 (`0064e7eb`),
  BandDirector +32 (`7589a980`), SongSort clusters +168/+118.
- **Wired-unpinned pin audits**: +497 (`ed45168c`), +116 (`8ae9244e`).
- **Map hygiene**: `scripts/map_verify.py` found 81 MISPAIR / 53 SUSPECT
  tree-wide; deleting wrong BinDiff pairings unlocked anonymous thunks
  (waves 20–22).

A *recarve* = re-cutting `splits.txt` `.text` boundaries and repairing
`scripts/target_symbol_map.json` identities so each retail span is attributed
to its real TU owner. The machine code was already correct; the tooling just
couldn't see it. This doc turns the manual recipe (worked examples:
`~/tmp/closeout16/reports/s2-storeoffer-getdata.md`, memory topic files
`project_wave16/17_2026-07-10.md`) into a programmatic pipeline.

## The manual recipe being automated

1. **Candidate scan** — pinned units with many hi-90%-fuzzy fns adjacent to a
   wall of 0%-matched fns ("correct code, wrong boundary" signature).
2. **Pin-extend** the `.text` end to the TU's true terminus (orphaned tail sat
   in `auto_03_*` at 0%). BandWardrobe's +174 was one boundary shift
   (0x82322DA0→0x82320E00) that also swept in BandCharDesc (+74).
3. **Ghidra-verify every identity per address** — positional guessing was 5/30
   wrong. Evidence: StaticClassName xrefs, vtable slots, callee shape.
4. **Remove provably-false stale pins** (fingerprint FPs squatting on ranges).
5. **Fix layout drift** with blast-radius check; **reconstruct retail-only
   methods** from Ghidra where the rb3-Wii oracle diverged (StoreOffer:
   retail kept the DataArray form the Wii dev branch had refactored away).

Screens learned the hard way (wave-17):
- **EH-funclet screen FIRST**: ~40B hi-fuzzy fns with `subi r31,r12`-family
  prologues are unwind funclets — parent-dependent, zero standalone yield.
- Map edits are **APPEND-ONLY** (wholesale re-sorts fail to merge); map
  **deletions need a `config.yml` re-split** (the renamer can't un-rename).
- Block-placement walls (retail hoists blocks; every source lever
  canonicalizes identically) are NOT recarve targets — defer.

## Alignment with ../decomp-synth

Two orthogonal search spaces over the same objective (objdiff byte-match):

| | decomp-synth | recarve pipeline |
|---|---|---|
| Searches | **source-space** (AST edits, beam/hill-climb) | **attribution-space** (boundaries, owners, names) |
| Assumes | correct (target-fn ↔ our-fn) pairing | nothing — it *produces* the pairing |
| Scored by | objdiff per-fn % | newly-revealed matches, whole-binary A/B |

Recarve is strictly **upstream**: mis-attribution poisons decomp-synth by
manufacturing fake near-misses the permuter grinds on forever (RockCentral's
mid-fuzzy band was 21/23 mis-pairs — see `map_verify.py` docstring; also
memory `project_permuter_correctness_model.md`). So the pipeline doubles as a
**verified-clean work-queue generator** for decomp-synth: after Stages B+C,
surviving near-misses are guaranteed real pairings.

Long-term home: prototype here (`scripts/recarve/`), keep the Stage B/C
interface project-agnostic (dtk splits + objdiff + a symbol-evidence
provider), then extract into decomp-synth as its missing "carving" front-end
stage. NB: decomp-synth's existing `attribution.py` is *instruction→source-line*
attribution (a different layer) — name the new stage "carving" to avoid
collision.

## Pipeline design (4 stages, descending automation)

### Stage A — Scan & rank (deterministic, read-only) — `scripts/recarve/scan.py`
Merge three signals per pinned TU:
1. **EXTEND signal** (from `scripts/find_underpins.py` logic): pin end abuts an
   unowned contiguous run of REAL (>44B, non-ICF-stub) functions.
2. **IN-UNIT signature** (from `build/45410914/report.json`): count matched/
   hi-fuzzy (≥90) vs zero (≤5) functions inside the unit — the wave-16
   recarve signature.
3. **MAP-GAP signal**: pinned-range `fn_` addresses missing from
   `target_symbol_map.json` (renamer can't pair → false 0%; ~630 such in game
   units per wave-22).
Apply the **funclet screen** (size ≈ 0x28, `r12` prologue check against
`build/45410914/asm/<unit>.s`) before counting hi-fuzzy fns.
Output: ranked JSON queue with per-candidate evidence + expected-yield proxy.

### Stage B — Boundary hill-climb (automatic, empirical) — `scripts/recarve/climb.py`
For each EXTEND candidate, in a pooled worktree
(`scripts/setup_worktree.sh`, objcache makes rebuild+rediff ~seconds):
```
while True:
    extend .text end to the NEXT function boundary (symbols.txt)
    rebuild + objdiff the unit
    newly captured fn matches or hi-fuzzy, or has oracle name-parity → keep
    else → revert last step, stop
```
Guards baked in as invariants: `scripts/find_truncated_splits.py` check after
every step (never cut a fn mid-body), `scripts/harvest/check_regression_lock.py`
+ 0-loss whole-binary A/B (`scripts/harvest/measure_delta.py`) before emitting.
Output: proposed splits.txt diff + per-fn evidence, NOT auto-landed.

### Stage C — Identity assignment (semi-automatic, confidence-tiered)
For each newly captured `fn_`, propose a name from three independent sources:
fingerprint strings (`tools/fingerprint_match.py`), oracle name-parity
(rb3-Wii TU function order), Ghidra static evidence (batched pyghidra, port
8002 — the evidence kinds `map_verify.py` already checks). **2-of-3 agreement
→ auto-append** to `target_symbol_map.json`; less → agent review queue.
Calibrate the auto-tier against the ~80 known MISPAIR cases from `map_verify`
as a labeled test set before trusting it.

### Stage D — Residual handoff
- Genuine near-misses (verified pairing, codegen delta) → decomp-synth queue.
- Retail-only / oracle-divergent bodies → agent wave with the Ghidra
  reconstruction recipe (StoreOffer worked example).
- Documented walls (block-placement, VBASE, EH-funclet) → tagged in
  `decomp.db`, skipped.

## Existing tooling inventory (what each stage reuses)

| Piece | Path | Role |
|---|---|---|
| Under-pin pre-filter | `scripts/find_underpins.py` | Stage A signal 1 (incl. ICF-stub-farm screen) |
| Truncation detector | `scripts/find_truncated_splits.py` | Stage B guard |
| Map audit | `scripts/map_verify.py` | Stage C evidence checks + labeled MISPAIR set |
| Layout drift | `scripts/harvest/offset_drift_sweep.py` | Stage D adjacent loop |
| A/B + landing | `scripts/harvest/{measure_delta.py,ab_supervise.sh,land.sh,check_regression_lock.py}` | Stage B/C gating |
| Worktrees | `scripts/setup_worktree.sh`, `scripts/orchestrator/worktree_pool.py` | Stage B isolation |
| Identification | `tools/fingerprint_match.py`, Ghidra MCP (port 8002) | Stage C |

## Data-format notes (for cold pickup)

- `splits.txt` TU header `Foo.cpp:` / `band3/game/Bar.cpp:` ↔ report unit
  `default/Foo` / `default/band3/game/Bar`.
- `build/45410914/report.json`: per-unit `functions[]` with `name` (mangled),
  `size` (str), `fuzzy_match_percent`, `match_percent_normalized`; `address`
  is unit-relative. Retail VAs come from `config/45410914/symbols.txt`.
- `scripts/target_symbol_map.json`: `{"0x<va>": "<msvc mangled name>"}`,
  15.2k entries. APPEND-ONLY; deletions require `touch config.yml` re-split.

## Next steps

1. **[in progress] Stage A scanner** — build, then validate the ranking:
   wave-23 independently identified VocalTrack blocks 3–5 (Gem/TrackConfig
   recarve) and ~630 anon-zero map-gaps as leads; the scanner should surface
   these without being told (corroboration check).
2. **Stage B climber** — single-candidate CLI first (`climb.py --tu X`),
   verify on the scanner's top candidate, then batch mode over the queue.
3. **Stage C calibration** — export `map_verify` MISPAIR/SUSPECT verdicts as
   the labeled set; measure 2-of-3 evidence precision before enabling
   auto-append.
4. **Stage D wiring** — emit decomp-synth-consumable queue (unit + symbol +
   verified-pairing flag); coordinate schema with `../decomp-synth`
   (`decomp_synth/batch_*.py` entry points).
5. **Extraction (later)** — once B/C are proven here, propose the "carving"
   stage upstream into decomp-synth with the project-specific bits
   (splits format, evidence providers) behind an interface.

## References

- Memory: `project_wave16_2026-07-10.md` (recipe origin, +83 worked example),
  `project_wave17_2026-07-10.md` (+212 campaign, ops lessons),
  `project_wave2{0,1,2}_2026-07-10.md` (map-hygiene arc).
- Worked example report: `~/tmp/closeout16/reports/s2-storeoffer-getdata.md`.
- Docs: `docs/decomp/playbooks/` (drift sweep), `docs/plans/paths-to-100/`.
- decomp-synth: `../decomp-synth/README.md`, `decomp_synth/attribution.py`
  (instruction-level — different layer), `crates/revcomp-*`.
