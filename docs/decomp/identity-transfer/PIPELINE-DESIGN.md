# Identity-Transfer Harvest Pipeline — Master Design + Implementation Plan

**Date:** 2026-06-21. **Author:** synthesis pass over the 4 research lanes
(`research/01-tooling-audit.md`, `02-objdiff-caseb-fork.md`,
`03-backlog-inventory.md`, `04-sourceport-bottleneck.md`). **Status:** design;
no code mutated. **Verified live** this pass: tool flags, fork branch state,
build wiring, baseline (`build/45410914/report.json` = **9834 / 65544** matched
at write time).

---

## 0. Executive summary (the decision)

The identity-transfer **transport** (micro-pin → carve → name → objdiff-pair) is
**solved and proven** (RockCentral.cpp +17). Everything downstream of "the
compiled obj defines each scattered method byte-exact" works. The pipeline is NOT
gated on tooling glue, NOT on jeff, NOT on the objdiff fork. **It is gated on the
SOURCE PORT** — making the compiled body byte-identical to retail (wave-16
BandProfile: a *faithful* 1013-line port, 4 trivial edits, still **0/64** at
100%). That single fact reorders the entire build plan:

1. **Build the partial-port machinery, not a whole-TU porter.** The headline new
   tool is **`field_offset_gate`** (a static "does this method touch a
   layout-divergent tail field?" analyzer) feeding **`--pin-only`** on
   `identity_transfer.py`. This converts the wave-16 *0/64 whole-TU loss* into a
   *small-but-positive head-only win* (the RockCentral shape: pin the carvable
   subset, skip the doomed tail). This is the force multiplier.
2. **Build the driver second** (`idtransfer_harvest.py`) — chains the 8 manual
   steps into one gated command in a CoW worktree, with the honesty gates as
   callable hard-fails. Without it, wide multi-TU waves are attrition-bound.
3. **DROP the `sim≥0.5` oracle gate as a portability predictor** — empirically
   disproven: the proven RockCentral win has **0 methods at sim≥0.5** (median
   0.099). Byte-equality after compile is the only valid gate. Keep a *low* sim
   floor (≥0.02) solely to kill the zero-corroboration MISATTRIBUTED class.
4. **Defer case-B + the objdiff fork.** It is sound, do-no-harm-verified, and
   already wired into the in-tree binary (inert), but its honest output is **+0**
   until a case-B *body* is ported byte-exact. Integrate it as the LAST step of a
   case-B wave, behind a separate `report_caseb.json` target — never the default.
5. **jeff: NO CHANGES NEEDED. Confirmed (§7).** Arbitrary-N RAW multi-range
   `.text` micro-pins already work (`ObjSplits::push`, never auto-merged);
   RockCentral carries 81 `.text` + 77 `.pdata` micro-ranges through stock jeff.

**Recommended build order (EV-ranked, §9):** (B1) `field_offset_gate` +
`--pin-only` → (B2) warm-up validation on 3-4 tiny high-sim network TUs → (B3)
`idtransfer_harvest.py` driver → (B4) locator-gate calibration CSV → (B5)
retail-Stats struct-lever wave (unblocks BandProfile/Game/GemPlayer family) →
(B6) case-B objdiff-fork integration (last, gated on B1-B5 yielding real wins).

---

## 1. Ground truth (verified this pass — do not re-derive)

| Claim | Status | Evidence |
|---|---|---|
| Transport solved, case-A proven | ✅ | RockCentral +17; `docs/decomp/identity-transfer.md` |
| jeff needs NO change | ✅ | RockCentral splits.txt = 81 `.text` + 77 `.pdata` micro-ranges through stock jeff; arbitrary-N already works (§7) |
| `identity_transfer.py` exists, STRICT add-only map, FIX-1 collision guard | ✅ | `tools/identity_transfer.py:107 SPAN_PIN_MIN=0x800`, `:338 covering_pin`, `:526 HARD GATE`, `:473 imports build_tu_entries/find_obj only` |
| `locator.py` exists, `--emit-gate`/`--validate`, 96.2% class agreement, **0 CONFIRMED** | ✅ | `tools/locator.py:499 run`, `:591 emit_gate`; live validate 51/53 |
| `gen_game_target_map.py` exists; its `main --apply` is POISON (wholesale sort+override) | ✅ | `tools/gen_game_target_map.py:454-465`; `identity_transfer.py` deliberately does NOT call it |
| objdiff fork = single commit, do-no-harm strict-superset | ✅ | `../objdiff` HEAD `b1c92be` on `caseb-global-byteeq`; off-flag = byte-identical sha256 to merge-base `e5987fb` |
| Fork ALREADY in the in-tree build binary (inert) | ✅ | `build.ninja:33-37` builds `build/tools/release/objdiff-cli` from `../objdiff` source; `:9 objdiff_report_args = ` (empty) |
| `progress_report_args` opt-in seam exists | ✅ | `tools/project.py:209,479,1334` |
| Honest case-B output = +0 today | ✅ | fork funnel: 1617 named-unmatched>44B → 4 sig-match → **0** oracle-own-TU |
| `sim≥0.5` gate would reject the proven win | ✅ | RockCentral 26 real bodies, 0 at sim≥0.5, median 0.099 |
| `field_offset_gate`, `--pin-only`, `idtransfer_harvest.py`, `overlap_check.py` | ❌ NOT BUILT | `grep` confirms none exist — all are proposed |

**The case-A/B/SELF discriminator (the load-bearing logic, do not reinvent):**
`identity_transfer.py:338 covering_pin(addr)` bisects every `.text` range in
splits → `None`=**CASE-A** (carve), self-TU=**SELF** (skip; reveal_sweep
territory), foreign-TU=**CASE-B** (skip → deferred worklist, needs fork).

---

## 2. The end-to-end pipeline (target architecture)

```
                        SOURCE PORT (the wall — §6)
                        port MWCC→MSVC so obj DEFINES each method
                                      │
   ┌──────────────────────────────────┴───────────────────────────────────┐
   │                       idtransfer_harvest.py <TU>                        │
   │                    (one gated command, CoW worktree)                    │
   │                                                                          │
   │  PREFLIGHT  wired? obj exists? fingerprints fresh? (G3)                  │
   │  WORKTREE   setup_worktree.sh  (CoW; NEVER mutate main — CLAUDE.md)      │
   │  BASELINE   fresh_report.sh → read measures.matched_functions  (G7)     │
   │     │                                                                    │
   │  ┌──┴── IDENTIFY ──┐  oracle = unified_id_rb3wii.json                    │
   │  │ classify case-A / SELF / case-B per method (covering_pin)             │
   │  └──┬──────────────┘                                                     │
   │  ┌──┴── LOCATE ────┐  locator.run --emit-gate sidecar.json   (exists)    │
   │  │ CONFIRM/RECON/WALL/UNPLACEABLE/MISATTRIBUTED per VA                    │
   │  └──┬──────────────┘                                                     │
   │  ┌──┴── FIELD-GATE ┐  field_offset_gate (NEW B1): drop tail-touchers      │
   │  │ pin-set = real(>44B) ∧ ¬MISATTRIB ∧ ¬WALL/Handle ∧ ¬tail-poison       │
   │  └──┬──────────────┘                                                     │
   │  ┌──┴── MICRO-PIN+MAP ┐ identity_transfer --pin-only <set> --locator-gate │
   │  │ append .text micro-ranges + STRICT add-only map  (exists +NEW --pin-only)│
   │  └──┬──────────────────┘                                                  │
   │  OVERLAP   overlap_check.py (NEW B3) → ABORT on splits overlap (G6)       │
   │  BUILD     rm stamp && touch config.yml && fresh_report.sh   (G4)         │
   │  MEASURE   read measures.matched_functions delta vs BASELINE  (G7)        │
   │  AUDIT     icf_alias_check.py --worktree → ABORT on stub-fold (G6)        │
   │  VERDICT   "LANDABLE:+N"  or  "DEFER:<reason>"                            │
   └──────────────────────────────────────────────────────────────────────────┘
                                      │ (case-B only, deferred)
                        report_caseb.json target (forked objdiff --global-byte-eq)
                        + per-promotion icf_alias_check + composed A/B
                                      │
                          scripts/harvest/land.sh  (composed verify → main)
```

Each box is one phase. Boxes marked **(exists)** reuse current tools verbatim;
**(NEW Bn)** is built per the §9 backlog; the honesty gates (OVERLAP, MEASURE,
AUDIT, VERDICT) are hard-fails.

---

## 3. Phase-by-phase spec + honesty gate at each step

### Phase 0 — SOURCE PORT (human/agent, the binding constraint)
- Port the rb3-Wii MWCC source → MSVC X360, wire `objects.json` NonMatching.
- **Replace whole-TU porting with partial porting** (§6): port the whole file so
  the obj *defines* every symbol, but only ever PIN the subset that can byte-match.
- **Gate:** none here — Phase 0 produces an obj; correctness is proven downstream
  by byte-equality, not by inspection. (The wave-16 lesson: a faithful port can
  still be 0/64; do not trust the port, trust objdiff.)

### Phase 1 — PREFLIGHT (driver)
- Assert: TU is in `objects.json`; the compiled obj exists in
  `build/45410914/src/`; **`fingerprints.json` mtime ≥ `symbols.txt` mtime**
  (G3 — stale fingerprints weaken locator signals B/C; regenerate via
  `tools/fingerprint_match.py extract` if stale, else warn).
- **Gate:** missing obj or stale fingerprints → abort with a clear message.

### Phase 2 — WORKTREE + BASELINE (driver)
- `scripts/setup_worktree.sh` → CoW worktree (CLAUDE.md HARD RULE: never mutate
  main; never `git stash`/`checkout`/`reset` files in main).
- `fresh_report.sh`; record `measures.matched_functions` as **baseline-in-worktree**
  (G7 — measure incrementally in the SAME tree; the wave-9 lesson is that a fixed
  external baseline double-counts foundational levers).
- **Gate:** worktree setup failure → abort.

### Phase 3 — IDENTIFY (reuse `identity_transfer.py` classify path)
- `covering_pin` over all `.text` ranges → case-A / SELF / case-B per method.
- HARD GATE preserved: if the TU has an own SPAN pin ≥ `SPAN_PIN_MIN = 0x800`,
  emit NOTHING unless `--allow-span-coexist` (the wave-16 −14 collision root).
- **Gate:** span-pinned + no override → abort (fail-closed).

### Phase 4 — LOCATE (reuse `locator.py --emit-gate`)
- Confirm-or-demote each case-A VA → CONFIRMED/RECON/WALL/UNPLACEABLE/MISATTRIB.
- **Gate (REVISED per §4):** use locator only as a **SKIP list** — drop
  MISATTRIBUTED (wrong-VA accessors) and WALL (`::Handle`/STL). Do NOT gate IN on
  `--require-class CONFIRMED` (0 CONFIRMED on scattered TUs → carves nothing). The
  default `--require-class CONFIRMED,RECON` plus the field-gate is the real filter.

### Phase 5 — FIELD-GATE (NEW B1 — the highest-leverage build)
- `field_offset_gate(TU, D)` where `D` = first member offset whose retail layout
  ≠ Wii (default `D` = first embedded-heavy/array member; flat-struct TU ⇒ `D=∞`).
- Static scan of each Wii method body (`../rb3/build/SZBE69_B8/asm`) for any
  `this`-relative `lwz/lfs/stw off(this)` with `off ≥ D` → tag POISONED-TAIL.
- **Pin-set = real(>44B) ∧ ¬MISATTRIB ∧ ¬WALL/Handle ∧ ¬POISONED-TAIL.**
- **Gate:** a method failing any predicate is excluded from `--pin-only`. This is
  the mechanism that raises hit-rate above wave-16's 0/64.

### Phase 6 — MICRO-PIN + MAP (reuse `identity_transfer.py` + NEW `--pin-only`)
- `identity_transfer.py --tu X --pin-only <set> --locator-gate s.json --apply`
  (in the worktree). Appends `.text` micro-ranges under the TU header (coalescing
  contiguous oracle runs); merges `{VA: mangled}` **STRICT ADD-ONLY** into the map.
- **CRITICAL SEAM (preserve):** never invoke `gen_game_target_map.py --apply` on a
  scattered TU — its `main` sort-rewrites + overrides (POISON,
  `gen_game_target_map.py:454-465`). The driver imports only `build_tu_entries` +
  `find_obj` and does the add-only merge, exactly as `identity_transfer.py` does.
- **Gate:** boundary-snap/bisect-reject (`:406-446`) + FIX-1 name-collision drop
  (`:490-531`) — both already enforced; the driver must not bypass them.

### Phase 7 — OVERLAP (NEW B3 `scripts/harvest/overlap_check.py`)
- Lift the SOP splits-overlap snippet (currently copy-paste prose in
  `scripts/harvest/README.md`) into a callable script shared by the driver AND
  `land.sh`. Independently-developed adjacent pins can collide and break the build
  (wave-9 CriticalUserListener/ViewSetting).
- **Gate:** any overlap → abort BEFORE build.

### Phase 8 — BUILD + MEASURE (reuse `fresh_report.sh`)
- `rm -f build/.../target_symbol_renames.stamp && touch config.yml &&
  fresh_report.sh` (missing the stamp `rm` = stale renamer = silently wrong
  measure — the driver must wrap this so it can't be fat-fingered, G4).
- Read post-apply `measures.matched_functions`; delta vs Phase-2 baseline.
- **Gate:** build failure or net ≤ 0 → `DEFER:<reason>`.

### Phase 9 — AUDIT (reuse `tools/icf_alias_check.py --worktree`)
- The ≤44B-stub-fold honesty gate: newly-100 must be REAL bodies, not the
  `??_E`/`_Vector_base<T>` folds that byte-match across unrelated TUs (wave-14 +57
  refutation; wave-16 ICF-alias self-check). Body-ports are exempt by design.
- **Gate:** stub-fold inflation detected → `DEFER:icf-inflation` (exit 1, abort
  land). This is a HARD gate, not advisory.

### Phase 10 — VERDICT + LAND
- Driver emits `LANDABLE:+N` or `DEFER:<reason>` (the `land.sh` contract).
- Landing = composed whole-binary A/B (the soft-rule gate from CLAUDE.md: net>0,
  zero unexplained regressions) → `scripts/harvest/land.sh`.

### Phase 11 — CASE-B (deferred; §5)
- Only after ≥3 case-A ports land real wins. Forked objdiff `--global-byte-eq`
  into a separate `report_caseb.json` target; every promotion in
  `--global-byte-eq-log` re-audited via `icf_alias_check.py` + composed A/B.

---

## 4. The `sim≥0.5` gate decision (DROP it as a predictor)

**Decision: remove `sim≥0.5` as a port/match predictor; keep a `sim≥0.02` floor
only for MISATTRIBUTED demotion.**

Evidence (lane 04, cross-checked against `unified_id_rb3wii.json`):
- Proven **RockCentral +17**: 26 real bodies, **0 at sim≥0.5**, median 0.099,
  max 0.433. The locator's `CONFIRMED` gate (`sim≥0.5 ∧ S≥0.60`) AND the case-B
  fork's `--global-byte-eq-oracle sim≥0.5` gate would each have **rejected the
  entire proven win.**
- Already-matched game TUs (MusicLibrary 0.042 / AccomplishmentManager 0.041 /
  SongSortMgr 0.049 median) sit far below 0.5 — sim≥0.5 is *not required* to match.
- The oracle sim measures rb3-Wii (MWCC PPC) vs retail (MSVC X360) = two
  compilers → a large constant noise floor (~0.41) unrelated to portability.

**Where this lands:**
- `locator.py`: keep CONFIRMED/RECON/WALL classes for the SKIP list (MISATTRIB +
  WALL), but the *driver* must not gate IN on CONFIRMED (0 of them exist).
- case-B fork (`--global-byte-eq-oracle`): the `CASEB_ORACLE_SIM_MIN = 0.5`
  sub-gate (`diff/mod.rs`) is too strict. When case-B is greenlit (B6), lower it
  to a floor (≥0.02) so the **own-TU basename match + byte-equality + reloc-NAME
  equality** carry the honesty (those are sound), not sim. The own-TU basename
  gate stays — it is the load-bearing identity check.

**Caveat:** this is a *recall* fix, not a *precision* hole. Byte-equality
(case-A) and masked-bytes+reloc-NAME equality (case-B) remain the decisive gates;
the own-TU basename gate remains. We are removing a noise floor that suppresses
true positives, not removing the honesty check.

---

## 5. objdiff-fork integration decision (DEFER; option B when greenlit)

**Decision: do NOT integrate now. When a case-B source-port wave is greenlit,
wire it as a SEPARATE `report_caseb.json` target (option B), never the default.**

Rationale (lane 02):
- Fork is sound: single commit `b1c92be`, do-no-harm strict-superset PROVEN
  (off-flag report.json byte-identical to merge-base `e5987fb`).
- Fork is **already the in-tree binary** (`build/tools/release/objdiff-cli` built
  from `../objdiff` source via `build.ninja:33-37`), inert because
  `$objdiff_report_args` is empty (`build.ninja:9`). Integration = "wire the
  flag," not "rebuild the binary."
- Honest output **= +0 today** (funnel: 4 sig-matches, 0 oracle-own-TU). The
  +150–220 ceiling is gated ENTIRELY upstream on a successful case-B *source
  port* (the same wall as case-A). Building case-B plumbing for a +0 capability
  is premature.

**When greenlit (B6):**
- Use the existing `progress_report_args` seam (`tools/project.py:209,479,1334`)
  but via a SECOND ninja edge writing `report_caseb.json` (option B), so the
  canonical `report.json` / progress number stays **provably stock**. The
  baseline-regression edge (`report changes`) then stays apples-to-apples.
- Flags: `--global-byte-eq --global-byte-eq-oracle unified_id_rb3wii.json
  --global-byte-eq-log build/45410914/caseb_promos.json`.
- LOWER the fork's `CASEB_ORACLE_SIM_MIN` from 0.5 to ~0.02 (per §4) so it
  doesn't suppress true case-B bodies — the own-TU basename + reloc-NAME equality
  carry the honesty.
- HARD gate: every entry in `caseb_promos.json` through `icf_alias_check.py` +
  composed A/B before the count is trusted.
- After any landing, re-run STOCK `report generate` from scratch (the fork's
  in-place `recalc_unit_measure_percents` skips `complete_*`/unit fuzzy-avg —
  project total is correct, per-unit `complete_code_percent` may be stale).
- Housekeeping: clean the untracked `../objdiff/modify_url.py`; pin
  `configure.py`'s objdiff path to `caseb-global-byteeq` or merge it to the
  fork's `main` so other agents don't trip on a "dirty objdiff" worktree.

---

## 6. The source-port bottleneck — what the design does about it

This is the ceiling. The pipeline's value is **bounded by how many methods a port
makes byte-exact**, and a faithful whole-TU port can be 0/64 (wave-16). The design
attacks it three ways:

1. **Partial porting (built into B1+ `--pin-only`).** Pin only methods that can
   byte-match. Converts 0/64 into "the head-only accessors" — the RockCentral
   shape (pinned 104 case-A of 129).
2. **`field_offset_gate` (B1).** The dominant killer is **struct-layout drift
   amplified by array/embedded members** (BandProfile `PerformanceData[50]` × a
   ~65B Wii-vs-retail `Stats` delta = 0xcc0 = 3264B tail shift poisoning every
   tail-touching method). The gate statically excludes tail-touchers so the port
   wins on the head before the layout is fixed.
3. **Retail-Stats struct-lever wave (B5).** The §2A root: Wii `Stats` is ~65B
   fatter than retail. One struct-lever on `src/band3/game/Stats.h` cascades
   through `PerformanceData/BandProfile/Game/GamePanel/GemPlayer` — the wave-17
   OnlineID reveal-cascade pattern. This unblocks the whole performance-data
   family *at once* and is the true force multiplier for the big TUs. It is a
   **struct-lever lane job, not a port-lane job** — hand it off, but it gates the
   high-realA backlog (GemPlayer 169fn, Game 103fn, BandProfile 104fn).

**The four divergence axes (defer or mitigate, do not fight):**
| axis | mechanism | handling |
|---|---|---|
| A struct-layout + array amplification | Wii layout ≠ retail, ×N array | `field_offset_gate` skip; struct-lever fix (B5) |
| B MWCC vs MSVC inlining | `FORCE_LOCAL_INLINE`→nothing under MSVC (`src/decomp.h:5-12`) | permuter-class; prefer methods w/ no force-inlined callees |
| C `BEGIN_HANDLERS`/`::Handle` | macro+version divergence, lowest sim | **always defer** (WALL skip list) |
| D baseline MWCC↔MSVC codegen | regalloc/scheduling | permuter pass on >97% residual |

---

## 7. The jeff question — NO CHANGES NEEDED (confirmed)

**Verdict: jeff requires ZERO changes for the identity-transfer mechanism.
CONFIRMED, not assumed.**

Evidence:
- The grounding fact ("jeff needs NO changes; arbitrary-N multi-range already
  works") is corroborated by the live splits: **RockCentral.cpp carries 81
  `.text` + 77 `.pdata` micro-ranges** under one TU header and is the proven +17
  win — produced through the **stock** jeff `ObjSplits::push` (RAW push, never
  auto-merged), which is the N-range generalization of the Part/PropKeys
  dual-range pattern.
- `identity_transfer.py`'s entire output is *textual splits + map* lines that dtk
  (jeff) consumes via the existing RAW-push path. No new dtk feature is exercised.
- The case-B path also needs no jeff change — it is purely an **objdiff** problem
  (pairing methods *within* a foreign-owned pin), solved by the objdiff fork, not
  by dtk.
- The only open jeff item in the project is the **funclet-truncation fix** (wave-18B,
  asm-misnest on GemTrack::See/Award ctor/LicenseMgr) — that is a *separate*
  recurring dtk bug, **NOT** part of the identity-transfer pipeline. Do not couple
  them.

**Refutation check:** is there ANY identity-transfer step that needs jeff? The
SELF case (method in own pin) is reveal_sweep, not jeff. The case-A carve is
splits-text, which jeff already handles. The case-B pairing is objdiff. The
honesty audit is `icf_alias_check.py`. **No path touches jeff.** Confirmed.

---

## 8. What to BUILD vs what to REUSE

| Component | Status | Action |
|---|---|---|
| `identity_transfer.py` (701L) | EXISTS | REUSE; add `--pin-only <list>` (B1) |
| `locator.py` (722L) | EXISTS | REUSE `--emit-gate`; re-task as SKIP list (§4) |
| `gen_game_target_map.py` (473L) | EXISTS | REUSE `build_tu_entries`+`find_obj` ONLY; never `--apply` on scattered TU |
| `icf_alias_check.py` | EXISTS | REUSE as hard AUDIT gate |
| `scripts/setup_worktree.sh` | EXISTS | REUSE (CoW worktree) |
| `tools/fresh_report.sh` | EXISTS | REUSE (BUILD+MEASURE) |
| `scripts/harvest/land.sh` | EXISTS | REUSE (LAND) |
| objdiff fork `b1c92be` | EXISTS, inert | REUSE behind option-B target (B6) |
| **`field_offset_gate`** | **NEW** | **BUILD (B1, ~150 LOC, reuses locator asm-walk)** |
| **`--pin-only` on identity_transfer** | **NEW** | **BUILD (B1, trivial)** |
| **`idtransfer_harvest.py` driver** | **NEW** | **BUILD (B3, the force multiplier)** |
| **`overlap_check.py`** | **NEW** | **BUILD (B3, lift SOP snippet)** |
| **locator-gate calibration CSV** | **NEW** | **BUILD (B4, realA→landed yield)** |
| **retail-Stats struct-lever** | **NEW (struct-lever lane)** | **HAND OFF (B5)** |
| **`report_caseb` ninja target** | **NEW** | **BUILD (B6, deferred)** |

---

## 9. PRIORITIZED, SEQUENCED IMPLEMENTATION BACKLOG

Ordered for an implementation workflow. EV uses the lane-03 yield band (blended
0.4–0.6 for un-ported TUs; 0.85 best-case for clean ports). Realistic case-A
ceiling ≈ **+236–295** (220 with-src TUs, blended); cheap near-term subset ≈
**+58–73** (28 TUs <700L rb3-Wii source). Discount the 155 NO-SRC network TUs
(realA=385, Quazal/PRUDP/Station/DDL — unreachable without new source).

### B1 — `field_offset_gate` + `identity_transfer.py --pin-only` (BUILD FIRST)
- **Why first:** it is the mechanism that beats wave-16's 0/64. Everything else
  amplifies a capability that does not yet exist without it.
- **Spec:** `field_offset_gate(TU, D)` static-scans `../rb3/build/SZBE69_B8/asm`
  for `this`-relative loads/stores ≥ `D`, tags POISONED-TAIL; emits the clean
  pin-set = real(>44B) ∧ ¬MISATTRIB ∧ ¬WALL ∧ ¬POISONED-TAIL. Add `--pin-only
  <list>` to `identity_transfer.py` so a partial port pins an explicit subset.
  Reuses `locator.py`'s asm-walk primitives (~150 LOC).
- **EV:** unlocks the entire case-A backlog; without it the cheap subset stays at
  ~RockCentral-only. **Gate-validate** against the RockCentral 104-pin set (the
  gate must NOT exclude any of the 17 that landed).

### B2 — Warm-up validation on tiny high-sim network TUs
- **Why:** prove the partial-port + byte-equality pipeline end-to-end on EASY mode
  before multi-hour big-TU ports. (NOTE: these are the rare NO-SRC-stack TUs that
  DO have rb3-Wii names — verify source exists per-TU; if not, substitute the
  smallest with-src cheap-subset TU, e.g. ChordPreview 4/88L.)
- **Targets:** `SharedSessionDescription` (sim 0.63), `StationState` (0.54),
  `KerberosEncryption` (0.55), `StationIdentificationDDL` (0.67) — 3–10 methods
  each. Plus `ChordPreview.cpp` (4/88L, with-src) as a guaranteed-source control.
- **EV:** +5–15, but the real payoff is pipeline validation + the first
  calibration data point beyond RockCentral (feeds B4).

### B3 — `idtransfer_harvest.py` driver + `overlap_check.py`
- **Why third:** once B1+B2 prove the mechanism, the driver makes WIDE waves
  feasible (turns 8 manual steps into one gated command). Building it before B1
  would automate a capability that loses 0/64.
- **Spec:** chains Phases 1–10 (§2–3) in a CoW worktree; hard-fail gates at
  OVERLAP/MEASURE/AUDIT; emits `LANDABLE:+N`/`DEFER:<reason>`. Lift the SOP
  overlap snippet into `scripts/harvest/overlap_check.py` (shared with `land.sh`).
- **EV (force multiplier):** turns the +58–73 cheap subset from an attrition-bound
  manual grind into a parallelizable wave; raises *realized* fraction of the
  ceiling, not the ceiling itself.

### B4 — locator-gate + realA calibration CSV
- **Why:** the gate is wired but UNVALIDATED against byte outcomes (0 CONFIRMED →
  the safe gate carves nothing; RECON hit-rate unknown). Today N=1 (RockCentral
  0.85). The tool's "truthful EV" returns 0 for unwired TUs → cannot pre-rank.
- **Spec:** after each B2/B3 port, record `realA_predicted` vs `landed` per
  locator class into a CSV; sort the cheap subset by mean realA `similarity` as a
  cheap divergence pre-screen (NOT a hard gate — §4). Tighten the yield band from
  N=1 to N≥5.
- **EV:** indirect — makes B3 waves pick the right TUs (avoids multi-hour ports of
  divergence-doomed TUs). Run continuously alongside B2/B3.

### B5 — Retail-Stats struct-lever wave (HAND TO STRUCT-LEVER LANE)
- **Why:** unblocks the highest-realA backlog (GemPlayer 29, BandProfile 15, Game
  12, MetaPerformer 15, TrackPanel 15) all at once via reveal-cascade. The §2A
  root: `src/band3/game/Stats.h` is the Wii layout (~65B fatter than retail).
- **Spec:** fix `Stats.h` to retail layout → cascades through
  `PerformanceData/BandProfile/Game/GamePanel/GemPlayer`. Gate on whole-binary
  composed A/B (shared-header soft-rule). This is a STRUCT-LEVER job, not a port
  job — but it is the prerequisite for ~+90 of the with-src ceiling.
- **EV:** high (unblocks the performance-data family), but RISK = shared-header
  ripple; must be composed-verified. Sequence after B1–B4 prove the harvest path
  is worth feeding.

### B6 — case-B objdiff-fork integration (LAST, gated on B1–B5)
- **Why last:** honest +0 until a case-B body is ported byte-exact (§5). Only
  worth its do-no-harm validation after ≥3 case-A ports land real wins.
- **Spec:** separate `report_caseb.json` ninja target (option B) via the
  `progress_report_args` seam; lower `CASEB_ORACLE_SIM_MIN` to ~0.02 (§4); hard
  gate every promotion via `icf_alias_check.py` + composed A/B; re-run stock
  report from scratch after landing. Clean `../objdiff/modify_url.py`; pin the
  branch in `configure.py`.
- **EV:** case-B ceiling ≈ +216–367 (realB=432, blended) — but UNVERIFIED
  end-to-end and gated on the same source-port wall. Bank nothing until a real
  case-B body matches.

### Sequencing summary
```
B1 (field_offset_gate + --pin-only)   ← BUILD FIRST, the mechanism
  └─ B2 (warm-up validation, 4-5 tiny TUs)   ← prove + first calibration
       └─ B3 (driver + overlap_check)        ← scale to wide waves
            └─ B4 (calibration CSV)          ← run continuously w/ B2/B3
                 └─ B5 (retail-Stats lever)  ← struct-lever lane, unblocks big TUs
                      └─ B6 (case-B fork)     ← LAST, gated on real case-A wins
```

### Cheap case-A first targets (for B2/B3, ranked realA/line-cost)
`ChordPreview 4/88L · MainHubPanel 11/590L · SongSortNode 7/454L ·
PerfectSectionTracker 6/395L · Scoring 5/366L · TourPerformerLocal 7/534L ·
SessionMgr 7/524L`. **Defer** BandProfile/Game/GemPlayer (large + the §2A Stats
wall — wait for B5). **Skip** the 155 NO-SRC network TUs (no source).

---

## 10. Honesty gates — the complete checklist (every gate is a HARD fail)

1. **Span-pin HARD GATE** (`identity_transfer.py:526`) — span-pinned TU emits
   nothing unless `--allow-span-coexist`. (wave-16 −14 collision root.)
2. **FIX-1 name-collision drop** (`:490-531`) — micro-pin whose mangled name dups
   a span-carved method is dropped whole. (steals pairing otherwise.)
3. **Boundary-snap/bisect-reject** (`:406-446`) — micro-range must start at a
   non-bisected fn start; end snapped to tightest non-bisecting boundary.
4. **STRICT add-only map merge** — never overwrite/re-sort/wholesale-regenerate;
   NEVER `gen_game_target_map.py --apply` on a scattered TU (POISON).
5. **`field_offset_gate`** (B1) — drop POISONED-TAIL methods (struct-drift).
6. **locator SKIP list** — drop MISATTRIBUTED + WALL/`::Handle`; do NOT gate IN on
   CONFIRMED (0 exist on scattered TUs).
7. **overlap_check** (B3) — abort BEFORE build on any splits overlap.
8. **byte-equality** — the ONLY positive match gate (not sim — §4).
9. **`icf_alias_check.py --worktree`** — abort on ≤44B stub-fold inflation
   (body-ports exempt).
10. **composed whole-binary A/B** — net>0, zero unexplained regressions, before
    `land.sh`. (CLAUDE.md soft-rule.)

The recurring failure mode this guards against: **byte-match ≠ ownership under
ICF folding** (wave-14 +57, wave-16 self-refutes). Gates 1/2/5/9 are the specific
counters; gate 8 (byte-equality, not sim) is why the pipeline is honest at all.

---

## 11. Bottom line

- The tooling is good and the seams already exist; the missing piece is the
  **partial-port machinery (B1)** then the **driver (B3)**.
- The true ceiling is the **source port** — `field_offset_gate` (B1) + the
  **retail-Stats struct-lever (B5)** are the only two things that move it.
- **jeff: no changes.** **objdiff fork: defer (option B, last).** **sim≥0.5:
  drop as predictor, keep byte-equality + own-TU basename.**
- Honest near-term EV: **+58–73** (cheap case-A subset, 28 TUs <700L), realistic
  case-A ceiling **+236–295**, gated on B5 for the big TUs. case-B +216–367 is a
  ceiling, banked at +0 until a real case-B body ports byte-exact.
