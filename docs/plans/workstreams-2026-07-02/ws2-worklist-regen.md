# WS2 — Worklist regen at a looser accept tier (+ Ghidra upstream PR status)

**Written 2026-07-02** by the ws-doc workflow. Master doc:
`docs/plans/frontier-workstreams-2026-07-02.md` (stream #2). Cold-start
executable: every path/command/number below was re-verified on 2026-07-02
against the live trees.

---

## Objective

Mine a **fresh identity tranche below the harvested 0.900-precision ACCEPT
tier** of the Wii→Xenon ghidriff/BSim identity pipeline, and surface it as
new porting-worklist rows for band3 + system/network. The 978 ACCEPT
identities (`ghidriff_identities.json`) are consumed: band3's 232 are fully
drained (memory `project_worklist_drain_close_2026-07-02.md`), sysnet's 516
are ws1. The lever is that the **v2 harvest workflow's reviewer-reproduction
gating** (reviewer independently re-runs objdiff; ≤44 B stub-fold guard;
composed whole-binary A/B at integration) now absorbs precision loss
downstream — a wrong identity costs porter time but can never mint a fake
match. That makes a ~0.85-precision band economically consumable where the
original 0.900 bar was chosen for un-gated handoff.

Secondary item: the "file the upstream Ghidra PR" task from the master doc —
**verified already done** (see §7; the master doc's "NOT yet filed" claim is
stale).

Two execution options, cheapest first:

- **Option A (primary, no Ghidra run, hours):** re-vet + re-ingest the
  **archived run-3 artifacts** at BSim `simconf ≥ 10` instead of `≥ 15`.
  The run-3 CAUTION pool already contains the entire next band on disk —
  verified: **805 net-new identities vs the live map** (314 band3 across
  107 TUs, 282 system, 205 network, 4 main).
- **Option B (secondary, full ghidriff re-run "run 5", ~1 day wall):**
  rebuild `--seed-matches` from today's ~13.7k-entry production pairing map
  (vs 1,213 seeds at run 3) and re-run the forked ghidriff cascade. Gate on
  Option A's measured outcomes first.

---

## Current state (verified 2026-07-02)

- Main repo `main @44f57c6`. Live `build/45410914/report.json` measures:
  **10,936 / 65,607 functions matched, 8.42% strict code bytes, 11.58%
  fuzzy** (the master doc's 10,870/8.32% is already a few hours stale —
  re-read report.json before computing deltas).
- `scripts/target_symbol_map.json` (the production pairing set): **13,674
  keys** (uppercase-hex `0X…` keys → MSVC mangled names).
- `/home/free/code/milohax/rb3-xenon/ghidriff_identities.json`: 978 rows,
  all `tier=ACCEPT`, all `source=ghidriff-run3` (mtime Jun 11). Categories:
  system 438 / band3 306 / network 216 / null 14 / main 4. Match types:
  BSIM 913 (simconf 15.00–104.41), ExactInstructionsFunctionHasher 54,
  Implied Match 8, SwitchSigHasher 3.
- **The whole pipeline lives in the sibling `../rb3` repo** (not rb3-xenon):
  - Runner: `/home/free/code/milohax/rb3/tools/ghidra/run_ghidriff_xenon.sh`
  - Vetting/tiering: `/home/free/code/milohax/rb3/tools/ghidra/vet_xenon_identities.py`
  - Ingest to rb3-xenon: `/home/free/code/milohax/rb3/tools/ghidra/ingest_ghidriff_accepts.py`
  - Seeds: `/home/free/code/milohax/rb3/tools/ghidra/build_xenon_seeds.py`,
    `build_accept_seeds.py`; eval: `eval_xenon_matches.py`, holdout growth:
    `grow_xenon_holdout.py`.
  - Master reference doc (read this first for the full arc):
    `/home/free/code/milohax/rb3/docs/decomp/xenon-identity-porting-OVERVIEW.md`
    (runs 1–4, precision measurements, what's dead). Depth:
    `xenon-precision-hardening-2026-06-10.md`, `xenon-hardening/round2/`,
    `xenon-hardening/round3/` (the 0.967 sysnet judging), and
    `ghidriff-calibration-2026-06-09.md` in the same docs tree.
- **Warm run-3 archive (immutable, all inputs for Option A on disk):**
  `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive/`
  - `vetted_identities.json` — 8,527 tiered entries: **ACCEPT 2,207**
    (of which 1,210 SeedMatch-only), **CAUTION 5,881**, FILTERED_VT 233,
    REJECT 206.
  - `json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json`
    (4.9 MB — the flat `function_matches` list with per-match
    VT/Implied/BSim scores) and the big
    `…ghidriff.json` (209 MB — carries both sides' decompiled `code`,
    needed for `--sibling-check`).
  - `eval_report.json`, `ghidriff.log` (full engine-args record).
- **Warm run inputs for Option B:**
  - p1 gzf: `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff/gzfs/bank8_target.elf-42264e.gzf` (79 MB, analyzed Wii Bank-8 Gekko ELF).
  - p2 gzf: `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf` (59 MB, analyzed Xenon XEX export, 12.2-format — opens ONLY under the fork dist). No re-export needed; both binaries are unchanged.
  - Seeds: `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/xenon-seeds/`
    — `seeds.json` (1,213 pairs `{p1_addr: 0x80…, p2_addr: 0x82…}`),
    `holdout.json` (158 judged entries, MUST stay excluded from seeds),
    `known_negatives.json` (3 judged-wrong pairs), `seeds_accept_run3.json`
    (the run-4 ACCEPT-only seed set).
  - Python: `/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python`
    (verified present; editable install of `/home/free/code/milohax/ghidriff`
    branch `rb3-improvements @e52d935`).
  - Ghidra: fork dist `/home/free/code/milohax/ghidra/build/ghidra`
    (12.2_DEV, `bsim-xenon-patches` jar-swapped — ships
    `PowerPC:BE:64:Xenon`, VT Tier-1+2, BSim top-K cap + parallel Phase-B).
- **`~/tmp/bsim_seed_work/` warm assets — clarification:** `wii.gzf`
  (136 MB, full `band_r_wii.elf-781439` Wii program export),
  `rb3wii.bsim.mv.db` (33 MB H2 BSim DB of the Wii oracle, `medium_nosize`
  template), `xmldir/sigs_c4216d2…` (signature XML), plus reflinked
  `RB3.rep`/`RB3Xenon.rep` copies. These belong to the **plain-BSim-query /
  seed-propagation experiment** (2026-06-21, NO-GO — see
  `docs/decomp/research/2026-06-21-bsim-seedprop-densification.md`), **not**
  to the ghidriff pipeline. They are NOT needed for this stream; they are
  reusable only if you want an independent plain-BSim confirm signal on
  loose-band candidates (query recipe in that doc's §Reproduce).
- **Consumers in rb3-xenon** (all read `ghidriff_identities.json`):
  `tools/gen_band3_port_worklist.py` (net-new = not in live
  `scripts/target_symbol_map.json`; verifies every `wii_symbol` resolves in
  the CW map, exits non-zero on failure),
  `tools/gen_sysnet_port_worklist.py` (same, `category ∈ {system,network}`),
  `tools/fn_resolver.py` T4b (`ghidriff_wii_b8` tier),
  `tools/band3_worklist_pin.py` (`--worklist PATH` consumes any file with
  the same row schema — pins + names only wired TUs, add-only).
- **v2 harvest workflow** (the reviewer-reproduction gate this stream leans
  on): `/home/free/.claude/projects/-home-free-code-milohax-rb3-xenon/a74fccf0-44cb-415b-9808-137561334027/workflows/scripts/band3-worklist-port-harvest-v2.js`
  — key properties (memory `project_worklist_drain_close_2026-07-02.md`):
  no whole-binary builds in port lanes (per-symbol objdiff only), ONE
  composed A/B at coordinator integration, reviewer re-runs objdiff itself,
  ≤44 B stub-fold guard, branch diff-hygiene check.

## How run 3 was actually invoked (recovered — exact, not reconstructed)

The wrapper is `rb3/tools/ghidra/run_ghidriff_xenon.sh` (`--dry-run` prints
the command). Engine args as recorded in the run-3 archive
`ghidriff.log` (2026-06-10/11, fork dist GHIDRA 12.2_DEV build
2026-Jun-09):

```
env GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
    GHIDRA_USER_HOME=/tmp/claude/ghidra_user_bank8 \
  /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python -m ghidriff \
    /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff/gzfs/bank8_target.elf-42264e.gzf \
    /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf \
    --engine VersionTrackingDiff \
    --output-path  /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon \
    --project-location …/ghidriff-xenon/proj --project-name rb3-wii-xenon-diff \
    --force-diff \
    --seed-matches /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/xenon-seeds/seeds.json \
    --bsim \
    --vt-ref-correlators --vt-ref-min-score 9.5 \
    --min-func-len 16 --implied-min-ratio 0.9 \
    --skip-correlators "BulkBasicBlockMnemonicHash,SigCallingCalledHasher,StructuralGraphExactHash" \
    --no-decomp-correlate --decompiler-timeout 20 \
    --log-level INFO --log-path …/ghidriff-xenon/ghidriff.log
```

(Flag rationale is documented line-by-line in the wrapper's header. Run 4
added `RB3_XENON_MATCHES_ONLY=1` → `--matches-only`, which skips the
~107-min report stage; matching itself was ~8 min once seeded.)

**ACCEPT-tier derivation** (matches.json → `ghidriff_identities.json`):

1. `vet_xenon_identities.py` tiers every match: ACCEPT = {SeedMatch,
   ExactInstructionsFunctionHasher, SymbolsHash, Implied Match,
   SwitchSigHasher} ∪ {BSIM with `similarity×confidence ≥
   --min-bsim-simconf`} (run 3 used **15** — holdout-calibrated 0.933 @ 922;
   **10 → 0.887 @ 1,969**); VTCombinedReference → FILTERED_VT/CAUTION
   (dead: judged 0.109–0.236, do NOT admit); string hashers → REJECT.
   Optional `--sibling-check on` (needs `--diff-json`, the 209 MB file)
   downgrades small near-identical same-TU bodies that differ on one
   call-arg/store literal — the dominant ~10% failure mode; calibrated
   recall 2/2 known-negatives @ 0/27 false positives.
2. `ingest_ghidriff_accepts.py` filters vetted ACCEPT → drops
   SeedMatch-only (already in the map), `category=sdk` (measured precision
   0.000), null `wii_symbol`, the 3 round-2 judged-WRONG xenon addrs
   (hardcoded `JUDGED_WRONG_XENON`), **and hard-floors BSIM at simconf ≥ 15
   (Gate 7 + a post-assert)** — this floor is what Option A must
   parameterize. Emits rows
   `{rb3_addr, wii_addr_bank8, wii_symbol, wii_symbol_demangled, tier,
   match_types, tu, category, bsim_simconf, source:"ghidriff-run3"}` to
   `../rb3-xenon/ghidriff_identities.json`.

## The looser-tier tranche, sized from the archive (verified numbers)

Run-3 CAUTION BSIM entries: 5,017. Simconf bands (joined archive
`vetted_identities.json` × `matches.json` on `p2_addr`):

| simconf band | count |
|---|---|
| 12–15 | 403 |
| 10–12 | 644 |
| 8–10 | 468 |
| 5–8 | 747 |
| <5 | 2,753 |

**Chosen admit band: 10 ≤ simconf < 15** (the calibrated 0.887-@-≥10
operating point; marginal precision of the band alone ≈
(0.887·1969 − 0.933·922)/1047 ≈ **0.85**). After the ingest-style filters
(non-sdk, named): **994 candidates**, of which **805 are net-new vs the
LIVE 13,674-key map** (2026-07-02): **band3 314 across 107 TUs, system 282,
network 205, main 4**. Free precision boosters already in the archive rows:
`rb3wii_check` = absent 648 / **contradicted 140** / confirmed 17 —
excluding the BinDiff-contradicted rows leaves ~665 and should push
effective precision toward ~0.87–0.90 before the sibling vet.

Do NOT admit: the 8–10 band (hold in reserve pending measured 10–15
outcomes), FILTERED_VT/VT anything (dead lever, refuted twice), string
hashers, sdk.

---

## Step-by-step procedure

### Phase 0 — scratch + safety

All scratch under `~/tmp/ws2-regen/` (NEVER `/tmp`). No edits to rb3-xenon
main-repo code/config except the final additive artifacts named below; the
`../rb3` repo tool patches (Phase 1) are normal commits in THAT repo (it is
not covered by rb3-xenon's shared-tree freeze, but check `cd ../rb3 && git
status` for other agents' in-flight work first).

```bash
mkdir -p ~/tmp/ws2-regen
RB3=/home/free/code/milohax/rb3
VENVPY=$RB3/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python
ARCH=$RB3/build/SZBE69_B8/ghidra/ghidriff-xenon/run3-archive
```

### Phase 1 (Option A) — re-vet the archive at simconf ≥ 10, sibling-check ON

```bash
cd $RB3
$VENVPY tools/ghidra/vet_xenon_identities.py \
  --run-dir $ARCH \
  --min-bsim-simconf 10 \
  --sibling-check on --sibling-action REJECT \
  --diff-json $ARCH/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.json \
  --out ~/tmp/ws2-regen/vetted_simconf10.json
```

Expected: ACCEPT grows from 2,207 by roughly the 10–15 BSIM band (1,047)
minus sibling-check downgrades. Sanity-diff the ACCEPT set at ≥ 15 against
the archived `vetted_identities.json` (should be identical minus
sibling-downgrades; if the sibling check also removes rows from the
*existing* 978, record them — that is a free precision repair, see Open
questions).

### Phase 2 (Option A) — patch + run the ingest for the loose band

Patch `$RB3/tools/ghidra/ingest_ghidriff_accepts.py` (small, mechanical):

1. Add `--vetted PATH` (default: the archive file) and `--matches PATH`
   overrides — currently hardcoded to `run3-archive/`.
2. Add `--bsim-floor FLOAT` (default 15.0) replacing the literal `15.0` in
   Gate 7 (~line 251) and the post-assert (~line 333).
3. Add `--source-tag STR` (default `ghidriff-run3`) replacing the
   `SOURCE_TAG` constant.
4. Add `--only-band LO,HI` (optional but recommended): keep only BSIM rows
   with `LO ≤ simconf < HI`, so the output is the *incremental* tranche and
   the existing 978-row file is not re-emitted.
5. Carry the vet row's `rb3wii_check` field through to the output record
   (so worklists can flag `contradicted`).

Then:

```bash
$VENVPY tools/ghidra/ingest_ghidriff_accepts.py --gate full \
  --vetted ~/tmp/ws2-regen/vetted_simconf10.json \
  --matches $ARCH/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json \
  --bsim-floor 10 --only-band 10,15 \
  --source-tag ghidriff-run3-simconf10 \
  --out /home/free/code/milohax/rb3-xenon/ghidriff_identities_loose.json
```

Expected: ~950–990 rows (994 minus sibling-downgrades/judged-wrong).
**Write to a SEPARATE file** — do not overwrite `ghidriff_identities.json`
while ws1 (sysnet drain) is live: the gen scripts' tracked `.md` outputs
(`docs/plans/{band3,sysnet}-port-worklist.md`) are ws1's active checklists
and must not be regenerated under it.

### Phase 3 (Option A) — spot-judge BEFORE handoff (the honesty gate)

Draw a 20-pair stratified sample (10 from 12–15, 10 from 10–12; mix
band3/system/network; exclude `rb3wii_check=contradicted` rows from the
handoff set entirely, judge 3 of them separately to confirm they are as bad
as expected). Judge with the round-2/3 protocol: for each pair, decompile
both sides (`cd $RB3 && bin/analyze-function <wii_symbol>` for the Wii
ground truth; rb3-xenon Ghidra MCP port 8002 or
`tools/ghidra/ghidra-decompile.py` for the Xenon body) and compare strings,
resolved callees, vtable-slot/type-tag immediates, node sizes. Protocol +
evidence-pack format:
`/home/free/code/milohax/rb3/docs/decomp/xenon-hardening/round2/` and
`round3/`. Record verdicts in
`~/tmp/ws2-regen/loose-band-judging.json` and summarize in this doc's
directory when done.

- **≥ 0.80 measured** → hand off the full band with per-fn
  confirm-on-consume (the sysnet 15–20 protocol, see
  `docs/plans/sysnet-port-worklist.md` §Safe-first).
- 0.70–0.80 → hand off only `rb3wii_check≠contradicted` ∧ simconf ≥ 12.
- **< 0.70 → BLOCKED** (same bar as the original ingest's `--gate blocked`);
  write the blocker note and stop Option A.

### Phase 4 (Option A) — generate the loose worklists

Patch `tools/gen_band3_port_worklist.py` + `tools/gen_sysnet_port_worklist.py`
in rb3-xenon (additive, does not touch the live worklists):

1. Add `--ident PATH` (default unchanged) and `--out-suffix STR` so the
   loose run emits `band3_port_worklist_loose.json` +
   `docs/plans/band3-port-worklist-loose.md` (same for sysnet).
2. Extend `confidence_label()` with a `bsim10-15` label (currently
   everything below 20 mislabels as `bsim15-20`) and rank it below
   `bsim15-20`; surface the `rb3wii_check` flag as a column.

```bash
cd /home/free/code/milohax/rb3-xenon
python3 tools/gen_band3_port_worklist.py  --ident ghidriff_identities_loose.json --out-suffix _loose
python3 tools/gen_sysnet_port_worklist.py --ident ghidriff_identities_loose.json --out-suffix _loose
```

Expected: band3 ~314 rows / 107 TUs; sysnet ~487 rows (both scripts
re-derive net-new vs the LIVE map at run time, so counts will have drifted
down by however much ws1 has landed). Both scripts hard-verify every
`wii_symbol` resolves to its claimed Bank-8 addr — a non-zero exit means a
join bug, not noise.

### Phase 5 (Option A) — consume via the proven v2 harvest workflow

Run the same machinery as the band3/sysnet drains, pointed at the loose
worklist: `tools/band3_worklist_pin.py --worklist band3_port_worklist_loose.json`
for wired TUs; the v2 port-harvest workflow for lanes
(`band3-worklist-port-harvest-v2.js`, path in Current state). Mandatory
lane rules (unchanged from v2): per-symbol objdiff only in lanes, reviewer
reproduces numbers, ≤44 B stub-fold guard, composed A/B at integration,
never inject CW names into `target_symbol_map.json`. Extra rule for this
band: **per-fn confirm-on-consume** — before porting a loose id, diff
vtable-slot/type-tag/node-size immediates + strings + resolved callees
against the Wii body (the sibling-aliasing failure mode is exactly what
this catches; it is how the sysnet 15–20 band is already handled).

### Phase 6 (Option B) — full re-run ("run 5") with 10× seeds — GATED

Run ONLY if Phase 3 measured ≥ 0.80 AND Phase 5's first wave converts (see
Kill criteria), i.e. the marginal band is real; otherwise the pool below 15
is exhausted and a re-run mostly re-discovers it.

1. **Rebuild seeds from today's anchors.** Patch
   `$RB3/tools/ghidra/build_xenon_seeds.py`: add `--target-map
   /home/free/code/milohax/rb3-xenon/scripts/target_symbol_map.json` as an
   additional Xenon-side name source (13,674 MSVC-mangled names keyed by
   Xenon addr — vs the 1,213 seeds run 3 had). Reuse the existing machinery
   verbatim: llvm-undname demangle → normalize (scope-sans-template-order,
   method, argcount, const) → join vs the CW map
   (`$RB3/orig/SZBE69_B8/files/band_r_wii.map`) → **drop any non-1:1 key**
   → CRT blocklist → **exclude the 158-entry holdout**
   (`xenon-seeds/holdout.json`) and `known_negatives.json`. Also union in
   `seeds_accept_run3.json` (the `build_accept_seeds.py` output — its
   anti-leak filters are already correct; note 85 of the 978 ACCEPTs
   overlap the holdout and MUST stay excluded, per the OVERVIEW §What NOT
   to do). Expect several thousand seeds; write to
   `xenon-seeds/seeds_run5.json` + stats sidecar.
2. **Run:**
   ```bash
   cd $RB3
   RB3_XENON_SEEDS=$RB3/build/SZBE69_B8/ghidra/xenon-seeds/seeds_run5.json \
   RB3_XENON_MATCHES_ONLY=1 \
   ./tools/ghidra/run_ghidriff_xenon.sh --dry-run   # inspect, then rerun without --dry-run
   ```
   Machine etiquette: the JVM takes `max_ram_percent 60`; run when no other
   heavy jobs (builds, Ghidra imports) are active. It does NOT touch the
   live MCP-locked Ghidra projects (own `proj/` dir + gzf inputs). Budget:
   ~10–30 min matching with `--matches-only` (run-3/4 precedent: ~8 min
   matching, 107-min report skipped); analysis is skipped (gzfs are
   pre-analyzed).
3. **Archive immediately** (the run dir is mutable):
   `mkdir run5-archive && cp -a json ghidriff.log run5-archive/` inside the
   ghidriff-xenon dir.
4. **Eval + vet + ingest** exactly as run 3: `eval_xenon_matches.py
   --run-dir … --seeds seeds_run5.json` (MUST pass the same seeds file or
   the holdout math is wrong), then Phase 1–4 of this doc against the new
   matches.json (both at simconf ≥ 15 → net-new ACCEPT tranche, and ≥ 10 →
   loose tranche). Net-new is always re-derived vs the live map by the gen
   scripts.

### Phase 7 — Ghidra upstream PR (see §7 below — status only, nothing to file)

---

## Honesty gates & verification

1. **No `target_symbol_map.json` injection of CW names — ever.** The
   worklists are targeting oracles; the map only takes proven MSVC symbols
   of compiled TUs (OVERVIEW §What NOT to do; the GemPlayer scatter
   disaster is the precedent, see `band3_worklist_pin.py` header).
2. **Spot-judge gate (Phase 3)** with the round-2/3 protocol before any
   handoff; < 0.70 = blocked, full stop.
3. **Reviewer reproduction** in every consumption lane: reviewer re-runs
   objdiff itself; lane readings are per-symbol; the ONE composed
   whole-binary A/B happens at coordinator integration and must be
   net-positive with 0 regressions.
4. **≤44 B stub-fold guard**: tiny bodies ICF-fold byte-identically across
   TUs; a ≤44 B "match" from a loose id proves nothing about identity.
5. **Provenance separation**: loose rows carry
   `source=ghidriff-run3-simconf10` and live in `_loose` files; the
   0.900-tier artifacts stay byte-identical while ws1 drains them.
   `fn_resolver.py` T4b currently grades all BSim rows 0.93 — do NOT feed
   the loose file into T4b until its confidence is graded by simconf
   (follow-up patch; 0.85 for the 10–15 band).
6. **Exclusions stay excluded**: 3 `JUDGED_WRONG_XENON` addrs, 3
   known-negative pairs, sdk category, 158-entry holdout (from any future
   seed set).
7. **Verification one-liners** (from OVERVIEW §4, adapted): net-new
   re-derivation and the expected-count check are embedded in the gen
   scripts (non-zero exit on any unresolvable `wii_symbol`). The archive
   tranche numbers in this doc reproduce with a ~15-line json join of
   `vetted_identities.json` × `matches.json` × the live map (band counts:
   403/644/…; net-new 805; by-category 314/282/205/4).

## Kill criteria

- **Phase 3 judged precision < 0.70** → Option A blocked; write blocker doc
  with the confusion matrix; the 8–10 band and Option B die with it.
- **First consumption wave converts < ~30%**: of the first 20–30 loose ids
  actually consumed by porter lanes, if fewer than ~30% reach a reviewed
  TRUE-100 (or verified fuzzy-pin) AND the failures are identity errors
  (wrong function, sibling alias) rather than the known MWCC→MSVC
  body-divergence wall → stop consuming the band; keep the worklist as a
  labels-only hint oracle.
- **Option B new-ACCEPT yield < 150 net-new** (at simconf ≥ 15, after vet +
  ingest, vs live map) → the vein is exhausted at this oracle pair; do not
  iterate run 6; the remaining frontier is ws6 manual reconstruction.
- Standing dead-ends (do not resurrect inside this stream): VT as an ACCEPT
  source (refuted twice, incl. the run-4 ACCEPT-only reseed), plain-BSim /
  seed-propagation densification (2026-06-21 NO-GO), DC3 net-new identities
  (3 remaining at strict conf).

## Expected yield

- **Option A**: ~805 net-new rows (2026-07-02 snapshot; shrinks as ws1
  lands) at ~0.85 raw / ~0.87–0.90 filtered precision. Using the harvested
  0.900-tier band3 conversion as the prior (232 ids → 121 landed/named +
  24 case-B + 12 unresolvable + WIP), a similar-shape funnel at 0.85
  precision suggests **~100–150 eventual strict matches** across the loose
  band3+sysnet rows — spread thinner (107 band3 TUs), so per-TU port cost
  dominates. The band3 slice (314) is the irreplaceable part (DC3 cannot
  supply it).
- **Option B**: unknown; the OVERVIEW warns "do not expect a step-change"
  (recall already 63.8% of holdout), but that advice predates 10× seed
  growth — more seeds shrink the unmatched pool and root implied/BSim
  propagation better. Treat >150 net-new ACCEPTs as success, anything less
  as confirmation of exhaustion. Cost is low (~30 min compute + the seed
  patch) once Option A validated the consumption path.
- **§7 PR**: zero new matches; community/maintenance value; already filed.

---

## 7. Upstream Ghidra PR — STATUS CORRECTION: already filed, nothing to do

The master doc (`frontier-workstreams-2026-07-02.md` §identity pipeline)
says the #8963 issue/PR were "drafted but NOT yet filed". **Verified
2026-07-02: both were filed on 2026-02-12** by `freeqaz`:

- Issue: <https://github.com/NationalSecurityAgency/ghidra/issues/8963> —
  "[PowerPC] Switch table analysis fails for MSVC-generated code patterns".
  OPEN, assigned `emteere`, labels Feature:Analysis /
  Feature:Processor/PowerPC / Status:Triage.
- PR: <https://github.com/NationalSecurityAgency/ghidra/pull/8964> — from
  `freeqaz:powerpc-msvc-switch-fix` (local branch
  `/home/free/code/milohax/ghidra` `powerpc-msvc-switch-fix @8b7cf690e4`,
  exactly the PR's single commit). OPEN, no reviews yet; triaged/assigned
  to `emteere` 2026-03-18.
- The untracked `commit-message.txt` / `pr-body.md` / `issue-body.md` in
  `/home/free/code/milohax/ghidra/` (dated Feb 12) are the **source drafts
  of the already-filed items**, not pending work. Safe to leave or archive;
  do not re-file.

Remaining actions (status-checking only, safe to run anytime):

```bash
gh issue view 8963 -R NationalSecurityAgency/ghidra
gh pr view 8964 -R NationalSecurityAgency/ghidra --json state,reviews,statusCheckRollup,comments
gh pr checks 8964 -R NationalSecurityAgency/ghidra
```

Optional follow-ups, in order of value: (a) respond promptly if `emteere`
reviews (watch via `gh pr view 8964 --comments`); (b) if upstream master
has drifted far since Feb (branch base predates the local `master
@9434f1c110` of Jun 09), offer a rebase in a PR comment BEFORE force-pushing
`origin powerpc-msvc-switch-fix` (remote `origin` =
`git@github.com:freeqaz/ghidra.git`); (c) the OTHER fork value — BSim top-K
cap + parallel Phase-B (`bsim-perf-candidatecap`), VT parallel reference
correlator (`vt-parallel-ref-correlator`, `vt-perf-fixes`), and the
ghidriff-fork features (`--seed-matches`, scored VT export, string-hasher
1:1 gate, `--matches-only`) — is **unfiled** upstream (Ghidra and
`clearbluejar/ghidriff` respectively) and worth separate PRs; that is new
scope, not this stream's deliverable.

---

## Open questions

1. **Does `--sibling-check on` retro-downgrade any of the existing 978?**
   Run 3's vet predates the calibrated sibling check. Re-vetting at ≥ 15
   with the check on is free while doing Phase 1 — if it flags rows already
   consumed as landed matches, those landed via byte-equality anyway (safe),
   but unconsumed flagged rows should be pulled from ws1's remainder.
2. **`rb3wii_check=contradicted` (140 rows)**: exclude from handoff (this
   doc's default) or judge a sample to see whether the BinDiff
   contradiction or the BSim match wins more often? A 5-pair spot-judge is
   cheap and would settle the band's true ceiling.
3. **fn_resolver T4b grading** for loose rows (0.85 vs the current flat
   0.93 for BSim) — needed before `ghidriff_identities_loose.json` is ever
   merged into the main identities file.
4. **Option B seed quality**: the target-map join goes through MSVC-name →
   CW-name normalization; template-heavy STL names are the main 1:1-drop
   risk. If the join yields < ~3k seeds, check the normalizer against the
   drops before concluding the map can't seed.
5. **Wii-side coverage ceiling**: run 3 matched from a 65,548-function
   Xenon pool against ~35k Wii functions; the loose band + a run 5 may
   still leave the class-B string-poor panels unmatched (they are the ws6
   wall, not a tier-threshold problem).
6. Should the 8–10 band (468 rows, expected ≪ 0.85) ever be surfaced as a
   labels-only hint file for the ws6 reconstruction workbench (never a
   worklist)? Defer until ws6 exists.
