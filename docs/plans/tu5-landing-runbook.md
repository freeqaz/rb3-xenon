# TU5 Re-base Landing Runbook

**Status:** ACTIVE — this is the driving doc for the flip. Owner: Fable coordinator
(session 2026-07-15, user go received: "take ownership of tu5-migrate and get that
landed"). Supersedes nothing; executes `docs/plans/roadmap-2026-07-12.md` §(d)
with post-P4-gate corrections.

**Read first:** `roadmap-2026-07-12.md` (GO decision + sequencing),
`tu5-execution-status.md` (P0/P1 keystone), memory
`project_tu5_rebase_decision_2026-07-12.md` (P4 gate result + mis-pairing root
cause).

---

## 1. State at runbook creation (2026-07-15, recon-verified)

| Fact | Value | Evidence |
|---|---|---|
| Live base for all flip work | main `aeccc7c9` (+ keystone `03557b71`) | tu5-p4-gate (ff839c46) is a full ancestor of main; zero unique commits |
| Current baseline | **15,852** matched / 69,366 (TU0 report) | `build/45410914/report.json`, mtime 2026-07-13 20:14, written after last VA-layer edit |
| Freeze | `config/45410914/.EDIT_FREEZE` LIVE on main | ff839c46; violated once (cc31ef0b +23 map entries) — tolerated, entries re-mapped below |
| VA-layer drift since gate baseline | only `scripts/target_symbol_map.json` +23 entries (TU0 VAs, cc31ef0b) | splits/symbols/config.yml/objects.json byte-identical ff839c46..aeccc7c9 |
| P4 gate result | TU5-matched **14,818** vs 15,852 → ~1,034 under floor | `~/tmp/wt-tu5-p4/build/45410914/report.json` (copy: `~/tmp/tu5_landing/report.tu5.p4gate.json`) |
| Gap decomposition | ~559 objdiff mis-pairing (fixable) + ~460 plausibly-genuine TU5 change | memory 2026-07-13 root-cause; being re-measured (lever a1) |
| Keystone tooling | LANDED on main `03557b71` (tu5_va, tu5_map_build, tu5_skel_recover, va_disasm_tu5, xex_binpatch_tu5) | roadmap action #2 done |
| Runtime risk class | RETIRED — RB3E DLL loads on HW, `/execute` returns DTA results on TU5 (2026-07-15) | memory `project_rb3e_dll_load_solved_2026-07-15` |
| TU5 binary | `orig/45410914/default_tu5.xex` sha1 `c5a17091cb44c0119424390a1738d161995e430e` (13,971,456 B, entry 0x8283CD20) | present in main orig/ (gitignored) |
| Rollback anchor (git half) | tag `target/tu0-frozen` = e589bf5b | + physical TU0 binary restore (see §5 rollback) |

### Artifact locations (durability status)

- **tu5-migrate worktree** (`.claude/worktrees/tu5-migrate`, branch tu5-migrate):
  `_tu5probe/tu5_migrate/` = base→TU5 map (61,629 entries), 478-entry changed
  worklist, **P2_classify.py / P3_remap.py generators**, `tu5_valayer/` outputs
  (incl. `nearmiss_anon_reloc_pairs.json`, `tu5_icf_folds_harvested.map`),
  `valayer_baseline_main/` (frozen ff839c46 inputs + PROVENANCE.txt).
  Was ALL untracked → being committed onto branch tu5-migrate (preservation
  agent, in flight; hash recorded in §6 when done).
- **wt-tu5-p4** (`~/tmp/wt-tu5-p4`, branch tu5-p4-gate): the applied P4 gate
  VA-layer state (4 modified tracked files) + the 14,818 gate report. Was
  uncommitted → being committed onto branch tu5-p4-gate (same agent).
- **KNOWN GAP:** gate splits.txt (6,236 lines) ≠ P3_remap output (4,082 lines) —
  an *apply/merge step* between generator output and gate config exists in no
  script. Reconstruction in flight → `~/tmp/tu5_landing/apply-step-reconstruction.md`;
  must be scripted before the flip re-run (§3 step F3).

### Load-bearing technical lessons (do not relearn)

1. **dtk treats TU5 `.pdata` as the AUTHORITATIVE function-boundary source**
   (overrides symbols.txt sizes). All regenerated boundaries must be
   derived/snapped from decoded `.pdata` (`FunctionLength=((word>>8)&0x3FFFFF)*4`,
   BE). P3_remap.py already does this; any hand edit must too.
2. **Flat `0x3000+VA` addressing is WRONG on TU5** (uniform −0x8000 error). Only
   section-mapped VAs via `tools/tu5_va.py`. `_tu5probe/FINDINGS.md` flat
   addresses are quarantined-wrong.
3. **The report pairs symbols BY NAME** (objdiff-core `find_symbol`/
   `find_symbol_by_name`); in the report's `FunctionRelocDiffs::None` mode,
   reloc-name mismatches do NOT degrade paired functions — the TU5 gap is
   symbol-level pairing failure, not reloc scoring. Fixing bl-target names moves
   nothing; fixing PAIRING is the lever.
4. **The objdiff fork already has the reorder-immune content-pairing pass**:
   `reconcile_global_byte_matches` (objdiff-core/src/diff/mod.rs:1082) — opt-in
   via `objdiff-cli report generate --global-byte-eq
   --global-byte-eq-oracle <unified_id json>`. Conservative gates: >44B,
   retail-signature injectivity, oracle own-TU attribution (oracle must carry
   TU5 VAs — remap through `base_to_tu5_map.full.json`).
5. **ICF super-folds** (e.g. fn_82270560 = 196 folded dtors) are correctly
   REJECTED by the injectivity gate — they cannot and should not be "recovered";
   they are excluded from honest matching on both TU0 and TU5.
6. Target-side-only renaming (target_symbol_map repair) measured **+0** — it
   cannot fix fn-level mispair (exact name equality is required on BOTH sides,
   and folds are many-names-to-one).

---

## 2. The gate (recomputed)

Floor accounting is being recomputed against 15,852 (agent in flight →
`~/tmp/tu5_floor/FLOOR.md`). Prior figures (15,816 baseline): 48 of 81 MISS
bodies matched → guaranteed floor 15,768, worst 15,595.

**Gate to flip:** after the mis-pairing lever, the TU5 report must show
`matched_functions ≥ floor(15,852)` with every drop inside the enumerated
changed-set (`expected_drops.json`). **NO-GO:** unexplained loss > ~1%
(~158 fns) → halt, keep TU0, debug `base_to_tu5_map.full.json` (roadmap §d).

## 3. Flip sequence (F-steps, ordered, gated)

- **F0 — Preserve + measure (IN FLIGHT, parallel):**
  (a) commit at-risk artifacts on their branches; (b) lever (a1) experiment:
  TU5-remapped oracle + `--global-byte-eq` report re-run in wt-tu5-p4, measure
  promotions + bucket residue (≤44B / non-injective / no-oracle-attribution);
  (c) recompute floor vs 15,852 + remap the 23 cc31ef0b pins to TU5 VAs
  (`~/tmp/tu5_floor/new_pins_tu5.json`). ✅ keystone cherry-picked (03557b71).
- **F1 — Decide lever depth:** if (a1) recovers the bulk of the ~559 → proceed.
  If a Rule-3-shaped residue dominates → small report-driver relaxation flag in
  the fork (report.rs:525-575 seam only; A/B on the TU0 report to prove zero
  baseline shift). Per-unit pairing code (`diff_objs`) stays untouched.
- **F2 — Script the apply step** from the reconstruction note; commit the script
  (branch tu5-p4-gate or tools/) so P2→P3→apply is fully reproducible.
- **F3 — Re-run the full pipeline against CURRENT main:** re-freeze
  `valayer_baseline_main` from main HEAD (now = ff839c46 VA-layer + 23 pins —
  splits/symbols identical, so mostly a provenance refresh), fold in
  `new_pins_tu5.json`, run P2_classify → P3_remap → apply → build → TU5 report
  **with the lever flags**. Spot-check MasterAudio/Object read 100%.
- **F4 — GATE:** compare vs `FLOOR.md`. Every drop must be in
  `expected_drops.json`. Unexplained > ~158 → NO-GO.
- **F5 — Atomic flip commit on main** (single commit, path-limited):
  regenerated `config/45410914/{splits.txt,symbols.txt}`, `scope_map.json`,
  `scripts/target_symbol_map.json` (TU5-keyed), `config.yml` (KEEP
  `object: orig/45410914/default.xex` so ~30 hardcoded tools auto-follow; rewrite
  the TU0 header comment), objdiff report args gain the lever flags
  (TU5-gated), **DELETE `.EDIT_FREEZE`**. Pre-commit: re-verify VA-layer
  quiescence (mtimes + git status) — the freeze was already violated once.
- **F6 — Out-of-band binary swap (lockstep with F5, NOT in git):** replace
  `orig/45410914/default.xex` with the TU5 xex (sha1 c5a17091…), regenerate
  `band.exe` via dtk, in main AND every active worktree that will keep building;
  coordinate CI container rebuild (`ghcr.io/rjkiv/rb3-xenon-build:main` `/orig`
  is TU0 — owner rjkiv, external) BEFORE the first post-flip CI run. Broadcast
  to concurrent agents. **Split-brain (TU5 splits vs TU0 bytes) is the #1
  external hazard.**
- **F7 — Downstream reseed:** `scripts/ingest_report.py` on the TU5 report
  (decomp.db fn_ rows re-key; named rows survive by name); landing_snapshot
  reseed (currently empty — low risk); Ghidra TU5 import
  (`tools/ghidra/import-xex.sh`, port 8002, keep TU0 program for BinDiff);
  regenerate fingerprints/autoid; RB3Enhanced `ports_xbox360.h` base/TU5 VA
  hygiene fix.
- **F8 — Open P5** as the standing worklist: 81 MISS bodies triaged per
  `tu5-rewritten-functions-analysis.md` §4 (~13 SKIP SDK/middleware, ~18 trivial
  ripple re-match ≥90% skel, ~25 genuine re-decomp: RockCentral::UpdateChar,
  GemTrainerPanel::Poll, GameMode::SetMode, Overshell* cluster, MidiParser*).
  The heavy per-unit losers (VocalPlayer −43, Game −42, BandDirector −32,
  SongSort* cluster, LightPreset −25, Matchmaker −22) route HERE, not to
  pairing fixes.

## 4. What breaks at flip (same-window fixes)

- CI container /orig (TU0) → mass mismatch until rebuilt (F6).
- Any warm worktree with TU0 `orig/` reflinks → same split-brain; stale
  worktrees should be re-seeded or retired.
- decomp.db fn_<addr> rows (57,550) stale until F7 reseed; 12,191 named rows fine.
- Evidence JSONs / scan caches keyed by TU0 VAs (recarve `~/tmp/recarve/*`,
  fingerprints.json, autoid.json) → regenerate post-flip.
- `tools/xex_binpatch.py` (flat-image) is WRONG on TU5 — use
  `tools/xex_binpatch_tu5.py` (landed 03557b71).

## 5. Rollback (two-part, rehearse before F6)

`git revert <flip-commit>` (restores TU0 VA layer + re-adds .EDIT_FREEZE) +
physical TU0 `default.xex`/`band.exe` restore on all checkouts + CI, + decomp.db
reseed from a TU0 report. Git half anchored at `target/tu0-frozen` (e589bf5b).
Keep a TU0 copy of `orig/45410914/{default.xex,band.exe}` under
`orig/45410914/tu0-archive/` before F6.

## 6. Landing log (append as steps complete)

- 2026-07-15: Recon workflow (4 Opus lanes) complete; findings folded into §1.
- 2026-07-15: Keystone tooling cherry-picked to main = `03557b71` (roadmap #2 ✅).
- 2026-07-15: F0 agents dispatched: preservation, lever (a1), floor recompute.
