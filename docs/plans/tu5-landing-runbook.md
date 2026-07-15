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
  Was ALL untracked → **COMMITTED on branch tu5-migrate = `72254ce0`** (38 files,
  33 MB; note `*.map` is gitignored — `tu5_icf_folds_harvested.map` needed
  `git add -f`).
- **wt-tu5-p4** (`~/tmp/wt-tu5-p4`, branch tu5-p4-gate): the applied P4 gate
  VA-layer state → **COMMITTED on branch tu5-p4-gate = `88794af4`**. Gate report
  copy: `~/tmp/tu5_landing/report.tu5.p4gate.json` (matched_functions=14818).
- **Apply step SOLVED (was "known gap"):** there is no missing script. The step
  is: (1) copy `tu5_valayer/{splits.txt,symbols.txt}` → `config/45410914/` and
  `target_symbol_map.json` → `scripts/` (byte-identical, sha1 edd88864);
  (2) flip the single `object:` line in config.yml to `default_tu5.xex`;
  (3) `touch config.yml && ninja` — the standard dtk split pass, run against the
  TU5 binary, re-derives all `.pdata` pins and all 129,150 non-.text symbols,
  augments .text symbols (+1875/−320), drops 10 .text-less TU headers.
  `.text` split pins carry verbatim (2406 == 2406). P3_remap emits .text-only
  by design (the map is .text-only; dtk owns the rest). Full accounting:
  `~/tmp/tu5_landing/apply-step-reconstruction.md`.

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

## 2. The gate (RECOMPUTED 2026-07-15 vs 15,852 — `~/tmp/tu5_floor/FLOOR.md`)

| quantity | value |
|---|---|
| baseline | 15,852 |
| MISS-we-match (guaranteed drops, enumerated) | **48** (same set as the roadmap's — all +36 baseline gains landed outside the changed set) |
| AMBIG-we-match | 178 |
| **GUARANTEED FLOOR** | **15,804** |
| worst case | 15,625 |
| NO-GO unexplained-loss budget (~1%) | **≈159** |

Enumerated drops: `~/tmp/tu5_floor/expected_drops.json` (48 fns; 17 ≥256 B;
largest OvershellSlot::UpdateState 2,132 B) — also the P5 seed. The 23
post-freeze pins (cc31ef0b) all remap cleanly: `~/tmp/tu5_floor/new_pins_tu5.json`
(append at flip; the valayer map predates them and holds 15,073/15,206 older
symbols, 133 dropped to re-anchoring ambiguity per its `dropped_pins.json`).

**Gate to flip:** post-lever TU5 report must show `matched_functions ≥ 15,804`
with every drop inside `expected_drops.json`. **NO-GO:** unexplained loss > ≈159
→ halt, keep TU0, debug `base_to_tu5_map.full.json` (roadmap §d).

## 3. Flip sequence (F-steps, ordered, gated)

- **F0 — Preserve + measure (IN FLIGHT, parallel):**
  (a) commit at-risk artifacts on their branches; (b) lever (a1) experiment:
  TU5-remapped oracle + `--global-byte-eq` report re-run in wt-tu5-p4, measure
  promotions + bucket residue (≤44B / non-injective / no-oracle-attribution);
  (c) recompute floor vs 15,852 + remap the 23 cc31ef0b pins to TU5 VAs
  (`~/tmp/tu5_floor/new_pins_tu5.json`). ✅ keystone cherry-picked (03557b71).
- **F1 — ROOT CAUSE RESOLVED (forensics 2026-07-15,
  `~/tmp/tu5_forensics/F1_DECISION.md`):** the three conflicting measurements
  were masks over the same bytes differing in D-form-immediate treatment
  (= struct-offset shifts). Reconciled decomposition of the 1,034 net deficit:
  - **B ≈ 600 (450–700): genuine-but-small TU5 struct member-offset shifts**
    (Harmonix moved/inserted class members; e.g. GameMode 0x18→0x1c,
    User::SyncSave 0x70→0x74, GemPlayer 0x3ac→0x3b0; 92% of the 191 named
    regressions carry IMM shifts). Fix = source-side struct re-basing per
    shared header (one header cascades to all readers) — real decomp knowledge,
    the post-flip headline campaign.
  - **C ≈ 250 (180–350): real divergence** (rewrites/call-target/overload
    changes/removals; includes the 48 sanctioned drops, 43 census-confirmed
    GONE).
  - **A ≈ 100 (50–250): pure tooling/map-gap** (base→TU5 map covers ~91% of
    named VAs; 839/1,062 unmapped bodies provably present via name-free masked
    scan).
  - The 8,468/7,513 name churn is ~95% benign anon-VA relabeling (8,040 traced
    1:1) — not loss. dtk reloc-deficit hypothesis DISPROVEN (reloc counts equal
    ±1). 0/191 wrong-body pairings. The 07-13 "1,120 normalized-identical"
    population is real but DISJOINT from the deficit (already inside 14,818).
    objdiff content-pairing recovers 0 (GBE-proven); do NOT relax Rule-3.
  - **GATE VERDICT: 15,804 pre-flip is unreachable** (it presumes recovering
    B+C, which is the post-flip campaign). Tooling-only landing ≈14,900–15,050.
    **The flip is LEGAL**: the NO-GO gate counts UNEXPLAINED losses, and full
    per-function attribution (P5 manifest, below) drives unexplained ≈ 0 ≪ 159.
- **F2 — Apply step: RESOLVED** (no script needed — copy valayer outputs, flip
  `object:` line, standard dtk split pass; see §1). Reproducibility is proven by
  the committed gate state (`88794af4`).
- **F3 — Re-run the full pipeline against CURRENT main (IN FLIGHT, agent):**
  worktree `~/tmp/wt-tu5-flip` (branch tu5-flip): re-freeze baseline from main
  HEAD (incl. 23 pins, which P3 remaps automatically), generalize P2/P3
  hardcoded paths, P2→P3→apply→build→report; stage the flip payload as a
  commit on tu5-flip. No lever flags (measured 0).
- **F3b — P5 manifest (IN FLIGHT, agent):** per-function attribution of every
  loss into RELABEL / A_TOOLING / B_STRUCT_OFFSET / C_DIVERGED / UNRESOLVED
  with evidence → `~/tmp/tu5_forensics/P5_manifest.json` +
  `docs/plans/tu5-p5-manifest.md`. All 48 sanctioned drops in C; arithmetic
  must close to 1,034.
- **F4 — GATE (REVISED):** matched ≈ 14,84x–15,05x expected; every loss must be
  attributed in the P5 manifest; **UNRESOLVED (unexplained) < 159** is the
  binding condition. NEW losses vs the P4 gate (regen noise) also need
  attribution. Fail → NO-GO, debug map.
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
- **F8 — Open P5** as the standing worklist, now THREE tracks from the manifest:
  - **P5-A (~100, tooling):** map-anchoring for the 839 provably-present
    unmapped bodies (census `unmapped_exist.json` FOUND set, uniqueness-gated)
    + splits tail-truncation fixes. Cheap, data-only, do first.
  - **P5-B (~600, struct re-basing — the headline):** per-shared-header TU5
    struct-offset campaign seeded by the manifest's offset-pair table
    (GameMode, User, GemPlayer, Singer, …). One header fixes all readers.
    This is genuine TU5 layout knowledge — successor to the wave cadence.
  - **P5-C (~250, body work):** real divergence incl. the 48 sanctioned drops;
    triage per `tu5-rewritten-functions-analysis.md` §4 (~13 SKIP SDK, ~18
    trivial ripple, ~25 genuine re-decomp). Heavy per-unit losers (VocalPlayer,
    Game, BandDirector, SongSort*, LightPreset, Matchmaker) live here.

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
- 2026-07-15: F0(c) floor recomputed vs 15,852: **floor 15,804 / worst 15,625 /
  budget ≈159**; same 48-drop set as roadmap; 23 pins remapped 0-loss.
- 2026-07-15: F0(a) preservation done: `72254ce0` (tu5-migrate, 33 MB map data +
  generators) and `88794af4` (tu5-p4-gate, applied gate VA-layer). Apply step
  reconstructed = seed-config + object-flip + dtk split pass (no missing script).
- 2026-07-15: F0(b) lever (a1) measured **0 promotions** (oracle-gated; 30 false
  STL folds with gate bypassed). Diagnosis disputed → forensics workflow
  dispatched (byte census + scoring forensics + churn decomposition).
- 2026-07-15: F1 DECIDED — forensics reconciled the gap: **B≈600 struct-offset
  shifts + C≈250 real divergence + A≈100 tooling; churn 95% benign relabel;
  flip legal via enumeration** (`~/tmp/tu5_forensics/F1_DECISION.md`).
  F3 (flip payload regen, wt-tu5-flip) + F3b (P5 manifest) agents dispatched.
