# objdiff case-B global-byte-equality fork — audit, do-no-harm validation, integration plan

**Lane:** objdiff-fork. **Date:** 2026-06-21. **Author:** research subagent.
**Scope:** Audit the banked fork, prove the do-no-harm strict-superset gate empirically,
and produce a concrete integration + validation checklist. Analysis only — no code
mutated in the main tree; the only build artifacts are in `/tmp`.

---

## 0. TL;DR

- The fork is a **single commit `b1c92be`** on `../objdiff` branch `caseb-global-byteeq`,
  on top of merge-base **`e5987fb`** (the `??__E/??__F` funclet-pairing commit). +654/-1 lines
  across exactly two files: `objdiff-cli/src/cmd/report.rs` (+119), `objdiff-core/src/diff/mod.rs` (+536).
  (The handoff doc's "`code.rs:101-131`" reference is stale — the gate lives in `diff/mod.rs`.)
- **DO-NO-HARM STRICT-SUPERSET GATE: PASSES.** Forked `objdiff-cli` run WITHOUT `--global-byte-eq`
  produces a report.json **byte-identical** (same sha256 `a688ac22…`) to a binary built from the
  merge-base. Confirmed by `cmp` + `sha256sum`. See §3.
- **Honest oracle-gated pass on the current repo state = +0 promotions** (byte-identical to stock),
  exactly as the handoff claims. The empirical funnel is captured in §3.3.
- **The handoff's biggest factual error:** it says "the shared `objdiff-cli` was NOT rebuilt
  (still dated Jun 11) → all current builds use stock objdiff." That is **false for the binary
  rb3-xenon actually runs.** rb3-xenon builds its **own** `build/tools/release/objdiff-cli` via a
  cargo ninja edge from `../objdiff` source, and that binary (mtime 2026-06-21 03:08, 15 min after
  the fork commit) **already contains `--global-byte-eq`.** The fork is *already wired into the main
  build* — it is inert only because nothing passes the flag (`$objdiff_report_args` is empty). The
  Jun-11 binary the doc points at is `../objdiff/target/release/objdiff-cli`, a *different* path that
  rb3-xenon does **not** use for its report edge. This is safe (do-no-harm holds), but the integration
  is "wire the flag," not "rebuild the binary" — the binary is already forked in-tree.

---

## 1. Where it lives & exact diff surface

| Item | Value |
|---|---|
| Repo | `/home/free/code/milohax/objdiff` (sibling of rb3-xenon) |
| Branch | `caseb-global-byteeq`, HEAD `b1c92be` |
| Merge-base vs `main` | `e5987fb` ("diff: add `??__E/??__F` global init/dtor to funclet pairing") |
| Diff | `git diff e5987fb..b1c92be --stat` → `report.rs` +119, `diff/mod.rs` +536, 654 ins / 1 del |
| Working-tree state | clean except one untracked file `modify_url.py` (unrelated; safe to ignore/remove) |
| Built-to-/tmp binary | `/tmp/objdiff-fork-target/release/objdiff-cli` (cargo build cached, `report generate --help` shows all 3 flags) |

The fork chain on the branch (top-down): `b1c92be` (this) → `e5987fb` (funclet ??__E/??__F) →
`72b553f` (FunctionRelocDiffs::NameOnly) → `444096c`/`bc59814` (v4.2.3 bump). So the case-B
commit sits on top of the funclet-pairing work, which is the relevant merge-base for the
superset test.

---

## 2. What the code actually does

### 2.1 CLI surface (`report.rs:164-181`)

Three new args on `report generate`, all opt-in (stock semantics unless `--global-byte-eq` passed):

- `--global-byte-eq` (`argp switch`) — enable the second pass.
- `--global-byte-eq-oracle <path>` (`argp option`, path) — **REQUIRED** with the switch.
  Points at `unified_id_rb3wii.json`. The pass **`with_context`-errors and refuses to run** if
  the switch is set but this is absent (`report.rs:519-523`). Verified empirically (§3.2).
- `--global-byte-eq-log <path>` (`argp option`, path) — optional; writes one JSON object per
  promoted VA (`{unit, symbol, virtual_address, size, base_unit}`) for `icf_alias_check.py` re-audit.

### 2.2 Driver seam (`report.rs:514-575`)

The pass runs **only in `generate()`** — the single place that enumerates every unit's
target+base obj paths — and *after* the per-unit cache is reconstituted (`cache.save`, line 511),
*before* the whole-binary `measures`/`categories` aggregation (`report.rs:577-588`). Steps:

1. Load oracle via `load_va_oracle` (`report.rs:595-625`): parses the `unified_id_rb3wii.json`
   list into `VaOracle = HashMap<u64, (src_basename_lowercased_no_ext, similarity_f32)>`.
   On duplicate VA, highest-similarity wins. In the current repo it loads **9301** entries.
2. Re-read every unit's target + base obj in parallel (`par_iter`) into `Vec<diff::UnitObjs>`.
   This is a second full obj read (the per-unit pass already read them, but discarded the
   `Object`s) — the cost is one extra parallel obj-read pass (~negligible vs the ~30 s diff).
3. Call `diff::reconcile_global_byte_matches(&mut units, &unit_objs, &symbol_equivalences, &oracle)`.
4. Optionally serialize the returned `Vec<GlobalPromotion>` to `--global-byte-eq-log`.

Because the promotion mutates per-unit `measures` *before* line 577's
`units.iter().flat_map(|u| u.measures)` aggregation and `report.calculate_progress_categories()`
(line 588), **a promotion correctly propagates to the binary-wide total and every progress
category** through the stock aggregation path — not a bolt-on. (Confirmed by reading
`report.rs:577-588` + `bindings/report.rs:294-300` accumulate logic.)

### 2.3 The honesty predicate (`diff/mod.rs:813-1349`, the +536 block)

Core entry: `reconcile_global_byte_matches` (`diff/mod.rs` ~`#[cfg(feature="std")]`). Algorithm:

**Signature primitive — `named_symbol_signature` (mod.rs:885-915).** Built on the pre-existing
`funclet_signature` (made `pub(crate)` — the *one* change to existing code, mod.rs:784) which
zeroes all relocation-targeted bytes (`masked_bytes`). The new code adds an ordered, offset-keyed,
**name-resolved** reloc descriptor list (`RelocDesc { off_from_sym, flags, target_name, addend }`).
This closes the masking hole: two >44 B fns with identical instruction *shape* but different
callees/strings mask-equal under `funclet_signature` alone, but differ once reloc-target NAMES
are carried. Reloc target names are canonicalized through the ICF equivalence map
(`canonical_reloc_name` / `canonicalize_sig`, mod.rs:921-955) = lex-smallest member of the
symbol's equivalence group, so an ICF-folded callee compares equal to its named sibling.

**The five rules (as enforced):**

1. **Real-bodied (`CASEB_STUB_MAX = 44`).** Both index side and base side require `size > 44`.
   Methods ≤44 B are ICF-folding stubs/thunks (73% of the oracle pool); byte-equality asserts
   nothing about ownership. (mod.rs index loop + decision loop.)
2. **Injective on BOTH sides.** Retail side: a signature carried by >1 *distinct retail VA* is
   rejected at lookup (`distinct_vas.len() != 1`). Base side: a retail VA / signature claimed by
   ≥2 distinct base mangled names → drop ALL (`va_decided` / `sig_decided` maps). N-to-1 in
   either direction = ambiguous identity = no promotion.
3. **Oracle own-TU gate (DECISIVE).** The retail VA must be oracle-named with `similarity ≥ 0.5`
   (`CASEB_ORACLE_SIM_MIN`) **AND** the oracle's `bindiff_src` basename must equal the *claiming
   unit's* source-file basename. A VA absent from the oracle has no asserted identity → reject.
   The base method must also be a real MSVC-mangled name — `is_anonymous_or_funclet` excludes
   `fn_<8hex>`, `__unwind$`, `__catch$`, `__unwind__merged_`, **and** `??__E`/`??__F` (init/dtor
   thunks that ICF-fold widely).
4. **Per-VA dedup (no double-count).** A retail VA already counted as matched (the pre-pass
   "already-matched-VA" set, keyed by retail VA — collision-free, unlike name-keyed) is never
   re-promoted. Belt-and-braces `retail_va_claimed` guard at apply time. The retail VA is read
   from `symbol.virtual_address` (split_meta) **or** parsed from the `fn_<8hex>` name
   (`parse_fn_va`) — in the carved COFF objs, `virtual_address` is `None` and the hex VA lives in
   the name. A body some unit already matched was renamed by the pre-compile renamer (no longer
   `fn_<VA>`), so indexing by `fn_<VA>` intrinsically excludes already-claimed bodies.
5. **Reloc structural equality (rule 5).** The `NamedSig` equality (masked bytes + canonicalized
   ordered relocs) is the match key — `funclet_signature`'s name-dropping hole is closed.

**Apply (mod.rs, decision-apply loop):** FOO-monotonic — only ever sets a still-<100% item to
100% (`match_percent_normalized = Some(100.0)`, `fuzzy_match_percent = 100.0`), bumps
`measures.matched_functions += 1`, `matched_code += size`, and calls the local
`recalc_unit_measure_percents` (mod.rs). Never clears an existing match.

**Debug/demo env vars (verified in §3):**
- `OBJDIFF_CASEB_DEBUG=1` — prints the index stats and the gate funnel to stderr.
- `OBJDIFF_CASEB_UNSAFE_NO_ORACLE=1` — bypasses Rule 3 ONLY (still enforces rules 1/2/4/5).
  Demonstration/diagnostic; never for a real measure.

---

## 3. DO-NO-HARM VALIDATION (empirical)

Built two binaries to isolated `/tmp` target dirs (main tree untouched):
- **fork** `b1c92be` → `/tmp/objdiff-fork-target/release/objdiff-cli`
- **merge-base** `e5987fb` (via `git worktree add --detach /tmp/objdiff-mergebase e5987fb`, since
  removed) → `/tmp/objdiff-mergebase-target/release/objdiff-cli`

Each `report generate` was run from the rb3-xenon repo root against the **current** built objs
(`build/45410914/`, same `objdiff.json`, same `icf_aliases.map`, 1708 units, 0 cache hits both
sides — fresh compute), writing to `/tmp/report_*.json`.

### 3.1 Strict superset (the gate the integration checklist demands)

```
fork  WITHOUT --global-byte-eq   sha256 = a688ac22…e29146
merge-base (stock)               sha256 = a688ac22…e29146
cmp → BYTE-IDENTICAL  ✅ PASS
```
**The fork is a strict superset: off-flag = stock, byte-for-byte.** (The in-tree
`build/45410914/report.json` has a *different* sha only because it was built at a later obj state
this afternoon — not a fork artifact.)

### 3.2 Oracle is non-negotiable

`report generate --global-byte-eq` *without* `--global-byte-eq-oracle` →
`Failed: --global-byte-eq requires --global-byte-eq-oracle (...)` and **non-zero exit, no report
written**. ✅ (Note: it errors *after* the ~30 s diff compute, not at arg-parse — a UX wart, not a
correctness issue, §5.)

### 3.3 Honest oracle-gated pass = +0 (do-no-harm holds even WITH the flag, current state)

```
$ OBJDIFF_CASEB_DEBUG=1 objdiff-cli report generate --global-byte-eq \
    --global-byte-eq-oracle unified_id_rb3wii.json --global-byte-eq-log /tmp/promos_honest.json ...
Loaded 9301 oracle VA-attribution entries
[caseb] target code syms>44B=34662 of which have_va(or fn_VA name)=23229
[caseb] retail_index sigs=22747 total_entries=23229 already_matched_va=5901
[caseb] funnel: named_unmatched>44B=1617 have_base_body=626 have_sig=626
        sig_in_retail_index=4 unique_retail_va=4 not_already_matched=4
        oracle_own_tu_ok=0 -> decisions=0
Case-B global byte-equality pass promoted 0 method(s)
```
Resulting report.json sha256 = `a688ac22…` → **byte-identical to stock.** ✅ +0 confirmed.

### 3.4 Mechanism works; the gate is doing real work

`OBJDIFF_CASEB_UNSAFE_NO_ORACLE=1` (bypass Rule 3) → **4 promotions**, the exact 4 the funnel
shows the oracle rejecting:
```
?_M_create_node@?$list@UAnim@EventTrigger@@…   va=0x8248d6c8 sz=72  unit=EventTrigger
??0?$_Vector_base@VColor@Hmx@@…                va=0x8274be38 sz=104 unit=auto_03_8274BA10_text
??0?$_Vector_base@USpotlightEntry@LightPreset@@ va=0x82499858 sz=104 unit=LightPreset
??0?$_Vector_base@VString@@…                   va=0x827c75c0 sz=104 unit=DataEventList
```
All four are **STL template instantiations** (`_M_create_node`, `_Vector_base<T>`) — byte-identical
across TUs, no asserted source identity, exactly the `icf_alias_check.py` misattribution class.
The oracle gate drops them 4→0 in the honest run. So: the byte-eq + injectivity transport works
end-to-end (the documented "+4 unsafe"), and Rule 3 is the load-bearing honesty gate, both proven.

### 3.5 Plumbing sanity

- Oracle JSON shape matches `load_va_oracle`: list of `{rb3_addr, bindiff_src, similarity, …}`
  (e.g. `0x82260000 → band3/src/tour/TourProgress.cpp, sim 0.9222`). 9301 entries.
- ICF equivalences are fed from `objdiff.json`'s `map_file → build/45410914/icf_aliases.map`
  (5 synthetic ICF groups today), which `generate()` already parses into
  `mapping_config.symbol_equivalences` and the fork threads in unchanged.

---

## 4. INTEGRATION CHECKLIST (when a case-B harvest wave is greenlit)

The binary is **already forked in `build/tools/`**, so most of the doc's checklist is *already done*.
The real remaining work is wiring the flag behind an opt-in and keeping default measures stock.

- [x] **Strict-superset do-no-harm test** — DONE/PASS (this doc, §3.1).
- [x] **Fork binary is the in-tree default `build/tools/release/objdiff-cli`** — already true (cargo
      ninja edge `build.ninja:33-37` rebuilds it from `../objdiff` source via depfile; current
      binary already exposes the 3 flags). No separate "rebuild the shared binary" step needed — and
      it's harmless because off-flag = stock.
- [ ] **Pin/commit the `../objdiff` fork state.** It's parked on `b1c92be` with an untracked
      `modify_url.py`. Decide: merge `caseb-global-byteeq` into the fork's `main`, or pin
      rb3-xenon's `configure.py:172` objdiff path to this branch explicitly. Remove/commit
      `modify_url.py` so the worktree is clean (avoids "dirty objdiff" surprises for other agents).
- [ ] **Wire the flag behind opt-in via the existing seam, NOT the default.** Two clean options:
  - **(A) Per-config flag (preferred).** `tools/project.py:210` already has
    `self.progress_report_args: Optional[List[str]]` → emitted as `$objdiff_report_args` in the
    `report` ninja rule (`build.ninja:9`, currently empty). Add a NEW config flag (e.g.
    `config.caseb_harvest=False`) that, when set, makes `configure.py` populate
    `progress_report_args = ["--global-byte-eq", "--global-byte-eq-oracle", "unified_id_rb3wii.json",
    "--global-byte-eq-log", "build/45410914/caseb_promos.json"]`. Default off ⇒ `$objdiff_report_args`
    stays empty ⇒ stock report.json. A harvest wave runs `configure.py` with the flag, builds, audits
    the promo log, then reverts. **This keeps the everyday `report.json` / progress number stock.**
  - **(B) Separate harvest target.** Add a distinct ninja edge (e.g. `report_caseb`) that writes
    `report_caseb.json` with the flags, leaving the canonical `report.json` edge stock. Cleaner
    separation (the canonical number can never silently include promotions) at the cost of a second
    ~30 s report build. Recommended if the project number must stay provably stock.
- [ ] **Wire into `tools/fresh_report.sh` behind the same flag** (handoff item) — only matters if the
      harvest is driven through that script rather than ninja directly.
- [ ] **PROCESS GATE on every landing:** every entry in `--global-byte-eq-log` must pass
      `tools/icf_alias_check.py` (real-bodied >44 B, oracle-correct, own-TU) AND a whole-binary
      composed A/B before the count is trusted. Land only real-bodied promotions.
- [ ] **Baseline note:** the in-tree regression baseline (`build/45410914/baseline.json`,
      `report changes` edge `build.ninja:12504-12509`) is stock. If a harvest run writes promotions
      into the *canonical* report.json (option A while the flag is on), `report changes` will show
      them as gains — fine during a deliberate harvest, but a reason to prefer option B for the
      everyday measure so baseline comparisons stay apples-to-apples.

---

## 5. RISKS / GAPS — what to build / watch

1. **The fork is ALREADY the in-tree binary (silent capability).** Not a bug today (off-flag =
   stock, proven), but the handoff's mental model ("stock until we rebuild") is wrong. Anyone who
   adds `--global-byte-eq` to `progress_report_args` and forgets the audit gate ships inflation.
   Mitigation: prefer integration **option B** (separate `report_caseb.json` target) so the
   canonical number is structurally stock; gate any merge of promotions through `icf_alias_check.py`.

2. **Oracle gate errors LATE (after ~30 s diff), not at arg-parse.** `--global-byte-eq` without
   `--global-byte-eq-oracle` wastes a full report compute before failing. Low priority; a one-line
   early validation in `GenerateArgs` handling would fix it.

3. **`recalc_unit_measure_percents` (fork-local) recomputes only `matched_code_percent` /
   `matched_functions_percent`** — it does NOT touch the unit's `complete_code` /
   `complete_code_percent` or the unit-level `fuzzy_match_percent` *average* that stock
   `Measures::calculate_percent` (`bindings/report.rs:249-276`) maintains. The whole-binary
   aggregation (report.rs:577) re-derives top-line numbers from `matched_code`/`matched_functions`,
   so the **project total is correct**, but a promoted unit's reported `complete_code_percent` could
   be slightly stale vs a from-scratch recompute. Verify the chosen progress metric reads
   `matched_*` (it does — that's the dc3-comparable number) and not `complete_*`. Worth a targeted
   check on the first real promotion. **Recommend:** when a harvest lands, re-run stock `report
   generate` on the now-100% (truly compiled-and-matched) state to get a from-scratch, fully
   consistent report rather than trusting the in-place-mutated one.

4. **The +150–220 ceiling is gated entirely upstream, not on this fork.** The fork promotes only
   methods that (a) are oracle-named sim≥0.5 to the claiming unit AND (b) are *defined byte-exact by
   our compiled base obj* (i.e. the scattered TU's source was ported and matches retail). Today
   that intersection is **empty** (funnel: 4 sig-matches, 0 oracle-own-TU → because no scattered TU
   with case-B bodies has been ported to byte-exactness yet). wave-16 showed ported MWCC→MSVC bodies
   *diverge* from retail (BandProfile 0/64). **So the binding constraint is source-porting attrition,
   not objdiff.** Do not greenlight integration as a "matching win" — greenlight it as plumbing that
   pays off *only* coupled to a successful case-B source-port wave.

5. **Cache staleness (acknowledged in-code, report.rs:516-520).** The per-unit cache keys only on a
   unit's own two objs; a cross-unit promotion is stale if an *unrelated* obj changes. Accepted for
   one-shot report builds. A harvest must run with a cold/fresh report (the run above had 0 cache
   hits — good), not an incremental one.

6. **`modify_url.py` untracked in `../objdiff`.** Cosmetic; clean it up so other agents don't trip
   on a dirty objdiff worktree (CLAUDE.md warns the objdiff repo is shared).

---

## 6. RECOMMENDATION

The fork is **sound, already wired, and do-no-harm-verified** (strict byte-identical superset off-flag;
+0 honest promotions in the current state; the oracle gate empirically rejects the 4 STL-fold demos).
**Integration is low-risk and mostly already done** — the only real step is choosing how to expose the
flag: **prefer a separate `report_caseb.json` harvest target (option B)** so the canonical progress
number stays provably stock, with `icf_alias_check.py` + composed A/B gating every promotion.

**But do not integrate as a standalone win.** Its entire +150–220 payoff is gated on a successful
case-B *source-port* wave (port scattered TUs to byte-exact, then identity-transfer micro-pin, then
this pass). Sequence it as the *last* step of such a wave, not before. Until a port lands a real
case-B body, the honest output is +0 — which this audit confirms it correctly produces.
