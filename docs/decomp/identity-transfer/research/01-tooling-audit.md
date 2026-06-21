# Identity-transfer tooling audit — end-to-end readiness

**Lane:** tooling-audit. **Date:** 2026-06-21. **Scope:** read-only audit of the
three existing tools + their data/integration seams; deliverable is the GAP LIST
for a fully-automatic `idtransfer harvest <TU>`.

**Verdict up front:** the *transport mechanism* is real, proven (RockCentral.cpp
+17), and well-defended against the wave-16 −14 regression class. But there is
**NO driver** — the pipeline is piecemeal. The three tools (`locator.py`,
`gen_game_target_map.py`, `identity_transfer.py`) are individually solid and
already share data contracts, yet running one TU end-to-end is ~8 manual steps
across two repos, a build, and a hand-audit. Worse, the locator's own ground
truth shows **0 CONFIRMED** on SongSortNode — so the safest gate
(`--require-class CONFIRMED`) carves *nothing*, and the realistic gate
(`CONFIRMED,RECON`) is unvalidated against actual byte-match outcomes. The
binding constraint is not tooling glue; it is **source-port body divergence**
(wave-16 BandProfile 0/64), which no tool here addresses.

---

## 1. The three tools

### 1.1 `tools/identity_transfer.py` (701 lines) — the N-range appender + case classifier

**Purpose** (`:1-85`): for one ICF-scattered TU, carve each scattered method into
that TU's target `.obj` by emitting N RAW `.text` micro-pins under one `Foo.cpp:`
header (jeff `ObjSplits::push`, never auto-merged), name them via
`gen_game_target_map`, and classify each method case-A / SELF / case-B.

**Inputs:**
- `--tu Foo.cpp` (required).
- `--oracle` = `unified_id_rb3wii.json` (default `:98`) — VA→name+size+sim.
- `--splits` = `config/45410914/splits.txt` (default `:96`) — existing pins.
- `--symbols` = `config/45410914/symbols.txt` (default `:97`) — retail fn sizes
  + all-symbol boundaries (bisect rejection).
- `--map` = `scripts/target_symbol_map.json` (default `:99`).
- `--obj-dir` = `build/45410914/src` (default `:259-260`) — the **compiled** obj,
  read to learn the MSVC-mangled defined symbols.
- `--report` = `build/45410914/report.json` (default `:100`) — the live 100-set
  for the truthful estimator.
- `--locator-gate <sidecar.json>` (`:279-283`) — the locator confidence gate.
- `--require-class` (default `CONFIRMED,RECON`, `:284`), `--min-confidence`
  (default `0.0`, `:288`), `--force` (`:290`), `--allow-span-coexist` (`:265`),
  `--deferred-out` (`:263`), `--apply` (`:269`).

**Outputs (only with `--apply`, must run in a worktree):**
- Appends `.text start:0xVA end:0xVA+sz` micro-range lines under the TU header in
  `splits.txt` (`:624-657`). Coalesces contiguous oracle runs (`:449-456`).
- Merges `{0xVA: mangled}` into the map **STRICT ADD-ONLY** — never overwrites an
  existing key, never re-sorts, never wholesale-regenerates (`:659-693`).
- Optional case-B eviction worklist JSON (`--deferred-out`, `:603-605`).
- Prints the next-step incantation (`rm stamp && touch config.yml &&
  fresh_report.sh`, `:694-695`).

**case-A vs case-B discriminator** (`:334-405`, the load-bearing logic):
a `covering_pin(addr)` bisect index over EVERY `.text` range in splits (`:335-342`,
multi-range aware) classifies each oracle body (size>0; size==0 = ICF alias,
dropped `:331`):
- `cov is None` → **CASE-A** (unowned `auto_*` blob) → micro-pin candidate
  (`:392-399`), *subject to the locator gate*.
- `tu_base(cov) == this TU` → **SELF** (already in own pin) → skip, reveal_sweep
  territory (`:400-401`).
- else → **CASE-B** (inside a FOREIGN unit's pin) → skip to deferred worklist
  (`:402-404`). The comment at `:46-50` is explicit: a `covering_pin` bug
  returning None for a foreign-owned addr would silently break the build, so the
  SKIP is fail-closed.

**The three defenses (all load-bearing, all post-RockCentral hardening):**
1. **Boundary snap + bisect rejection** (`:406-446`): a micro-range start must be
   a non-bisected fn start (`:416`); the end is snapped to the tightest
   non-bisecting boundary among {oracle end, next any-symbol start, next fn
   start} (`:428-444`), validated against OVERLAPPING symbol spans via
   `bisects_any` (`:371-384`, scans back over stale-CFA-oversized neighbors).
   Rejects (`rejected_bisect`) anything that can't snap cleanly.
2. **FIX-1 collision-safety** (`:61-74`, `:344-357`, `:490-531`): the wave-16 −14
   root cause. If the TU already has a wide SPAN pin (`>= SPAN_PIN_MIN = 0x800`,
   `:107`), the span renamer already mints every in-span method's mangled name; a
   case-A micro-pin OUTSIDE the span whose mangled NAME duplicates a span-carved
   method (ICF alias / overload-arity ambiguity) mints a SECOND target symbol that
   STEALS pairing. FIX-1b builds `span_names` + an intra-batch `seen_names` set and
   drops the WHOLE colliding range (`:509-524`). FIX-1a HARD-GATEs: a span-pinned
   TU emits NOTHING unless `--allow-span-coexist` (`:526-531`, `:614-622`).
3. **Locator confidence gate** (`:271-323`, `:386-405`): when `--locator-gate` is
   supplied (and not `--force`), a case-A VA is carved only if its locator class
   is in `--require-class` AND confidence ≥ `--min-confidence`; unclassified VAs
   are DROPPED (fail-closed, `:298-302`). This is the wave-16 mis-carve guard
   (BandProfile 0/64 = mis-located, not un-matchable).

**Truthful estimator** (`:185-247`, FIX 2): joins carved case-A bodies against
`report.json`'s live 100-set ON THE MANGLED NAME (report `address` is
section-relative decimal, not a VA — `:170-182`). Reports a CONSERVATIVE floor
(sim≥0.5) and OPTIMISTIC ceiling (no sim gate); both require >44B, nameable,
survives the FIX-1 filter, and NOT already matched.

### 1.2 `tools/locator.py` (722 lines) — per-method VA-placement classifier

**Purpose** (`:1-61`): the oracle is near-random for scattered TUs (BandProfile
median sim 0.16; ZERO ≥0.70). The locator FUSES multiple weak signals into one
confidence score and classifies each method CONFIRMED / RECON / WALL /
UNPLACEABLE / MISATTRIBUTED. The oracle gives ONE candidate VA per method, so this
is **CONFIRM-OR-DEMOTE, not search**.

**Inputs:** `--tu`; `--oracle` (`unified_id_rb3wii.json`); `--identity`
(`unified_id.json` `:73`, for callee-name resolution); `--fingerprints`
(`fingerprints.json` `:74`, retail per-fn callees/imms/strings/size);
`--symbols` (retail pdata sizes); `--retail-asm` (`build/45410914/asm` `:76`,
the `auto_*_text.s` slices); `--wii-asm` (`../rb3/build/SZBE69_B8/asm` `:77-78`);
`--report`; ablation flags `--no-callee/--no-cfg/--no-string/--no-wii/
--size-gate-only`; `--ghidra` (optional p-code confirmer, `:604-616`, a thin
+0.05 nudge, NOT load-bearing).

**Outputs:** `--out` (full table JSON); **`--emit-gate sidecar.json`**
(`:591-596`) = `{VA: {class, confidence}}` — the exact contract
`identity_transfer.py --locator-gate` consumes; `--validate <gt.json>` (replay vs
hand table, `:633-690`).

**How it finds VAs:** it does NOT search. The oracle's `rb3_addr` IS the candidate
VA (`:18-19`). The locator's job is to CONFIRM that VA is the right named method
or DEMOTE it. `true_size = sizes.get(va)` from `symbols.txt` (`:534`) — `None`
means no pdata fn-start (ICF-folded leaf), an immediate UNPLACEABLE.

**Confidence model — the decision cascade** (`:427-493`, first match wins, order
matters):
1. `true_size is None` → UNPLACEABLE (0.95).
2. `true_size <= 44` → UNPLACEABLE (0.95) — guard-thunk; runs BEFORE sim because
   guard-thunks have deceptively high sim ~0.41 (`:21-22`).
3. accessor name + body ≥512B + ratio>20× → MISATTRIBUTED (0.90).
4. ctor name whose body is a memberwise COPY (≥5 parallel load/store pairs, 0
   branches, ≤1 callee) → MISATTRIBUTED (0.80) — rests on signal D (`:459-473`).
5. sim<0.02 AND S<0.15 → MISATTRIBUTED (0.60).
6. sim≥0.50 AND S≥0.60 → CONFIRMED (0.85).
7. sim≥0.12 OR S≥0.45 → RECON (0.55–0.75).
8. else → WALL (0.50).

`S` = 0.40·callee_jaccard + 0.25·cfg_shape + 0.15·string_overlap +
0.20·size_band (`:406`). Signals: B callee-set jaccard (retail callees via
`unified_id` ↔ Wii body callees, `:348-367`), D cfg shape (basic-block/branch
count, retail asm re-walk vs Wii, `:369-385`), C string overlap (`:387-397`),
size-band consistency (`:399-404`). bindiff sim (E) is NOT in S — it is the
dedicated RECON/WALL split + CONFIRMED corroboration.

**Relation to the BinDiff oracle:** the locator is a *trust layer* on top of the
oracle. The oracle supplies VA+name+sim+size; the locator independently
corroborates with retail-side fingerprints + Wii-side body asm and outputs a
class the consumer can gate on. It degrades gracefully: missing Wii asm zeroes
B/C/D (`:523-526`), leaving size+sim only.

**Validated agreement** (re-run live this audit):
`tools/locator.py --tu SongSortNode.cpp --validate <gt.json>` →
**51/53 = 96.2% overall (PASS)**, MISATTRIBUTED 2/2 (PASS, safety-critical),
RECON/WALL 27/27 = 100% (PASS), CONFIRMED emitted 0 (PASS by design),
UNPLACEABLE 22/23 (one FAIL). Two disagreements: `0x82a51b60`
ShortcutNode::IsActive truth=UNPLACEABLE pred=RECON (180B, sim 0.235, S 0.51);
`0x82aeef10` OwnedSongSortNode::GetAlbum truth=RECON pred=UNPLACEABLE (44B stub).
Ground truth = 53 rows, **0 CONFIRMED** (23 UNPLACEABLE / 15 RECON / 13 WALL /
2 MISATTRIBUTED).

### 1.3 `tools/gen_game_target_map.py` (473 lines) — VA→mangled namer

**Purpose** (`:1-58`): pinning a game TU's `.text` gives +0 because objdiff pairs
by name and the target's `fn_<addr>` symbols never equal the compiled obj's
MSVC-mangled names. This tool generates the `{0xADDR: mangled}` map the
pre-compile renamer consumes.

**How it pairs** (`:24-46`, demangle-and-match, no re-mangling): parse each
oracle `wii_name` into `(class, method, argcount)` (`parse_wii_name`,
`:184-227`); decode the *defined* MSVC-mangled symbols in the compiled obj into
`(class, method, kind)` (`msvc_class_method`, `:120-157`); match by
`class::method`, disambiguate overloads by arg count where determinable
(`msvc_argcount`, `:160-181`, conservative — bails unless `XZ` void). Only
section>0 (defined) symbols are eligible (`:282-291`); scoped to the TU's dominant
class set to reject neighbor-TU interlopers (`:294-301`, `:333-336`).

**Inputs:** `--oracle`, `--map`, `--obj-dir`, `--spans` (`/tmp/candidate_spans.json`
for span scoping, `:381`), `--purity` (default 0.70), `--area` (default
`meta_band`), `--tu` (repeatable), `--apply`. **Outputs:** `{hexaddr: mangled}`
merged into the map. **CAUTION:** standalone `main()` `--apply` writes the map
with `sort_keys=True, indent=2` and **game entries OVERRIDE existing** (`:454-465`)
— this is the wholesale-rewrite path that the roadmap flags as POISON-adjacent.
`identity_transfer.py` deliberately does NOT call this `main`; it imports only
`build_tu_entries` + `find_obj` (`:473`) and does its own STRICT add-only merge.
That is the **critical seam to preserve**: never let the harvest driver invoke
`gen_game_target_map.py --apply` on a scattered TU.

---

## 2. The integration seams (what already connects)

1. **locator → identity_transfer:** `locator --emit-gate sidecar.json`
   (`locator.py:591-596`) → `identity_transfer --locator-gate sidecar.json`
   (`identity_transfer.py:279-323`). Contract = `{VA: {class, confidence}}`. This
   seam EXISTS and is wired, but is **not exercised by any driver** and is
   optional (off when no sidecar passed).
2. **gen_game_target_map → identity_transfer:** `identity_transfer` imports
   `build_tu_entries` + `find_obj` (`:473`) to name carved VAs from the COMPILED
   obj's defined symbols, then does its own add-only merge. Correct.
3. **identity_transfer → build/measure:** purely textual — it prints the
   incantation `rm -f build/.../target_symbol_renames.stamp && touch config.yml &&
   tools/fresh_report.sh` (`:694-695`). Nothing automates this.
4. **measure → honesty:** entirely manual. `tools/icf_alias_check.py` (22 KB,
   exists) + per-unit A/B + `report.json` `measures.matched_functions` is the
   wave-loop SOP (`docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`,
   `scripts/harvest/{land.sh,resolve_*_union.py}`).
5. **shared primitives:** both consumers import `load_sizes`/`compute_starts`/
   `parse_splits` from `pin_identified.py` and `parse_wii_name` from
   `gen_game_target_map.py` — no reimplementation drift.

**Data files (all present, verified):** `unified_id_rb3wii.json` (3.4 MB, 9301
rows — note: 9301, not "every fn"; it is the BinDiff-matched subset),
`unified_id.json` (5.4 MB), `fingerprints.json` (12.8 MB, May 26 — **stale**, see
gaps), `scripts/target_symbol_map.json` (1.1 MB), `report.json` (9.9 MB, Jun 21),
`symbols.txt` (16.5 MB, Jun 21), `splits.txt` (Jun 21). Oracle coverage of the
named scattered TUs: BandProfile 104, RockCentral 135, SongSortNode 53,
MainHubPanel 42, LockStepMgr 26. RockCentral (the proven win) currently carries
**81 `.text` + 77 `.pdata` micro-ranges** in splits.txt.

---

## 3. What is MANUAL today (the actual end-to-end run)

There is **no `idtransfer harvest`**. `scripts/wf_idt_research.js` is the
orchestrator workflow that produced THIS doc — it is research-only, not a harvest
driver. Running one scattered TU is this hand-sequence:

| # | Step | Tool | Automated? |
|---|------|------|-----------|
| 0 | Port the MWCC source so the obj DEFINES each method | (human) | **NO — the real gate** |
| 1 | Wire the TU (`objects.json` NonMatching) | (human edit) | NO |
| 2 | Build once so the compiled obj exists | ninja/fresh_report | manual invoke |
| 3 | Run locator, emit gate sidecar | `locator.py --emit-gate` | tool exists, manual |
| 4 | Eyeball locator classes / set `--require-class`,`--min-confidence` | (human) | NO |
| 5 | `identity_transfer --tu X --locator-gate s.json --apply` (in a worktree) | `identity_transfer.py` | tool exists, manual |
| 6 | Overlap self-check on splits | inline python in SOP README | manual snippet |
| 7 | Composed verify: rm stamp + touch config.yml + fresh_report.sh | `fresh_report.sh` | manual invoke |
| 8 | Honesty audit (icf_alias_check + per-unit A/B + read matched count) | `icf_alias_check.py` | manual |
| 9 | (case-B only) rebuild report with FORKED objdiff `--global-byte-eq` | `/tmp/objdiff-fork-target` | NOT integrated |
| 10 | Land via `scripts/harvest/land.sh` | `land.sh` | exists |

So: ~3 of 10 steps have a tool but every transition is a human-typed command, and
steps 0/1/4/9 have no automation at all. It is **piecemeal**.

---

## 4. GAPS / what to build

### G1 — NO driver. Build `tools/idtransfer_harvest.py <TU>` (the headline gap).
A single command should chain steps 2–8 (assuming the source is already ported +
wired). Phases: (a) build-if-needed → ensure the compiled obj exists; (b)
`locator.run` in-process, emit gate sidecar; (c) `identity_transfer.main` with the
gate, dry-run by default, `--apply` to write **in a worktree it sets up via
`scripts/setup_worktree.sh`**; (d) overlap self-check (lift the SOP snippet into a
function — it is currently copy-paste prose in `scripts/harvest/README.md`); (e)
invoke `fresh_report.sh`; (f) read `measures.matched_functions` delta vs a saved
baseline; (g) run `icf_alias_check.py --worktree` and FAIL if stub-fold inflation
detected. Output a one-line `LANDABLE:+N` / `DEFER:<reason>` verdict like
`land.sh` does. **EV: this is the force multiplier** — it turns an 8-step
attrition-prone manual run into one gated command and makes wide multi-TU waves
feasible.

### G2 — The locator gate is wired but UNVALIDATED against byte outcomes.
The gate filters carves by class, but ground truth shows **0 CONFIRMED** on
SongSortNode (re-run live: 0/53). So `--require-class CONFIRMED` carves NOTHING,
and the realistic `CONFIRMED,RECON` default has **never been correlated with
actual 100%-match outcomes** — we have GT for the *class* (96.2% vs hand table)
but NOT for "does a RECON-class carve actually byte-match after porting?" The
RockCentral +17 predates the gate entirely. **Build:** a calibration pass that,
for the one TU we DID land (RockCentral) + the next ported TU, joins
`locator.class` against the post-build `report.json` 100-set and reports a
per-class hit-rate. Without this, `--min-confidence` is a guess. **Risk if
skipped:** the gate gives false confidence; a `CONFIRMED,RECON` wave could carve
WRONG VAs exactly as wave-16 did.

### G3 — `fingerprints.json` is STALE (May 26; symbols/report are Jun 21).
The locator's signals B (callee jaccard) and C (string overlap) read
`fingerprints.json` for retail-side data. A 26-day-old fingerprint index predates
~16 waves of new pins/splits; retail VAs whose ownership changed will mis-resolve
callee names, weakening S. **Build/run:** regenerate via
`tools/fingerprint_match.py extract` before any locator-gated wave, OR have the
harvest driver assert `fingerprints.json` mtime ≥ `symbols.txt` mtime and warn.

### G4 — Build/measure is unautomated and uses the slow full path.
Every iteration is `rm stamp && touch config.yml && fresh_report.sh` (full
all_source build + report regen, NINJA_JOBS-capped to avoid OOM). For a per-TU
inner loop this is heavy. **Build:** the driver should (a) do this in a CoW
worktree (so it doesn't block concurrent agents — CLAUDE.md hard rule), and (b)
consider a scoped measure if/when objdiff supports per-unit report deltas. At
minimum, wrap the incantation so it can't be fat-fingered (missing the stamp rm =
stale renamer = silent wrong measure).

### G5 — case-B path is NOT integrated end-to-end.
`identity_transfer` correctly SKIPS case-B to a worklist (`--deferred-out`), but
counting case-B needs the FORKED objdiff (`../objdiff` branch `caseb-global-byteeq`,
built only to `/tmp/objdiff-fork-target`, shared binary still stock —
`docs/decomp/handoff/objdiff-caseb-fork-banked.md`). The harvest driver has no
hook to (a) select the forked binary, (b) pass `--global-byte-eq
--global-byte-eq-oracle unified_id_rb3wii.json --global-byte-eq-log`, (c)
re-audit every promotion via `icf_alias_check.py`. **Decision needed:** keep
case-B fully out of the driver (case-A only for now — honest +0 today on case-B
anyway) OR wire the fork behind a `--case-b` flag with the mandatory oracle gate +
promotion audit. Recommend **case-A only** until a source-ported TU produces real
case-A wins (don't build case-B plumbing for a +0 capability).

### G6 — The honesty audit is manual + the overlap check is prose.
`icf_alias_check.py` exists and is in the SOP, but invoking it + interpreting its
exit code is a human step; the splits-overlap check lives as a copy-paste snippet
in `scripts/harvest/README.md` (not a callable function). **Build:** fold both
into the driver as hard gates (exit 1 = abort the land), and expose the overlap
check as `scripts/harvest/overlap_check.py` so both the driver and `land.sh` call
the same code.

### G7 — No baseline-delta measurement in the loop.
The driver must capture `measures.matched_functions` BEFORE applying and AFTER, in
the SAME worktree, to report the true per-TU net (the wave-9 lesson: measuring vs
a fixed baseline double-counts; measure incrementally in-worktree).

### What a robust `idtransfer harvest <TU>` needs (synthesis):
```
idtransfer harvest <TU> [--apply] [--require-class CONFIRMED,RECON]
                        [--min-confidence X] [--case-b]
  preflight : assert TU wired + obj exists + fingerprints fresh (G3)
  worktree  : setup_worktree.sh (CoW; never mutate main — CLAUDE.md) (G1,G4)
  baseline  : fresh_report.sh; read measures.matched_functions (G7)
  locate    : locator.run --emit-gate -> sidecar                    (exists)
  classify  : print per-class counts; honor gate flags              (G2)
  carve     : identity_transfer --locator-gate --apply (case-A)     (exists)
  overlap   : overlap_check.py (splits) -> abort on overlap         (G6)
  measure   : fresh_report.sh; delta vs baseline                    (G4,G7)
  audit     : icf_alias_check.py --worktree -> abort on stub-fold   (G6)
  verdict   : LANDABLE:+N  or  DEFER:<reason>                       (G1)
  (case-b)  : forked objdiff --global-byte-eq + per-promo audit     (G5, deferred)
```

**Bottom line:** the tools are good and the seams already exist; the missing
piece is the **driver (G1)** plus the **gate calibration (G2)** — and neither
removes the true ceiling, which is the **source-port step (#0)** that no tool here
touches. Recommend building the case-A-only driver + the gate calibration first;
defer case-B (G5) until a ported TU yields real case-A matches to justify it.
