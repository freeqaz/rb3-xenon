# ws7 — Dead-lever re-audit: synthesis, reopened experiments, confirmed kills

**Date:** 2026-07-02. **Synthesized from:** 7 independent adversarial Fable
audits of the project's "dead lever" verdicts (one auditor per lever, each
reproduced the kill evidence on today's state before ruling).

**Master doc:** `docs/plans/frontier-workstreams-2026-07-02.md` (section
"Dead levers — presumed dead, pending the ws7 re-audit"). This doc is the
re-audit it was pending. Update guidance for that section is at the bottom.

## Objective

For every lever the project declared dead, either (a) confirm the kill with
reproduced evidence so no future agent wastes a lane on it, or (b) reopen the
specific flawed/untested variant with an exact, bounded, kill-gated retry
experiment. Reopens are listed first, ranked by expected value.

## Current state (verified 2026-07-02 by this synthesizer, not trusted from audits)

- main @ `385182b` (the master doc was written at `44f57c6`; the repo moved
  during the day — worklist pins are still landing).
- `build/45410914/report.json`: **10,936 / 65,607 functions matched**, 8.42%
  strict code bytes, 11.58% fuzzy. (Master doc's 10,870/8.32%/11.46% is the
  same-day earlier snapshot; drift is landing velocity, not error.)
- `config/45410914/splits.txt`: 1,083 `.text` spans.
- `ghidriff_identities.json`: 978 ACCEPT-tier ids (exists at repo root).
- `docs/decomp/gameid/crossval_agree.json`: 146 `agree_fns`.
- All evidence docs cited below exist at the stated paths (checked).
- Audit scratch artifacts exist under `~/tmp/ws7-audit/` (finder re-runs:
  `mdf2_2026-07-02.json` with exactly 4 MEMBER_DELTA candidates,
  `inline_candidates_2026-07-02.json` with 36 candidates) and
  `~/tmp/ws7audit/` (`plurality_repro2.py`). **`~/tmp` is ephemeral** — a cold
  agent should regenerate via `tools/member_delta_finder2.py` (~25 min) /
  `tools/inline_policy_finder.py` (~7 min) if these are gone, rather than
  trusting stale copies.
- Warm BSim assets verified present: `~/tmp/bsim_seed_work/{wii.gzf,
  rb3wii.bsim.mv.db, RB3Xenon.gpr, RB3.gpr}`.

Scoreboard: **7 levers audited → 2 CONFIRMED_DEAD, 5 PARTIAL_REOPEN, 0 full
FLAWED_REOPEN.** Every reopened item is a *variant or salvage*, not a
resurrection — each audit explicitly re-confirmed the core kill.

---

# Part 1 — REOPENS (ranked by expected value)

## R1. Member-delta apply mini-wave — highest EV, run first

- **Parent lever:** "inline-policy hunt exhausted + permuter retired as
  wave-driver" (kill docs: `docs/plans/decomp-state-and-roadmap-2026-06-09.md`
  lines 158–162 / 2030–2044;
  `docs/decomp/research/2026-06-30-nearmiss-codegen-inventory.md`).
- **Auditor ruling:** PARTIAL_REOPEN (high confidence). Inline-policy and
  permuter-as-wave-driver stay DEAD (re-verified on today's pool, see Part 2).
  What reopens is the **member-delta finder**, which was swept into the
  "forcemult exhausted" verdict without ever running on a pool containing the
  July-ported game TUs.
- **Flaws found:**
  1. Provenance: the master doc cites weak wave-20 header-grep lanes as the
     inline-policy kill; the strong evidence (objdiff-driven
     `tools/inline_policy_finder.py` @a8716c7, drained 4x, all n=1) predates it.
     Kill correct, citation wrong.
  2. Protocol lapse: both finders carry an explicit "re-check after the pool
     refills" condition (roadmap line 162) that was honored through Jun 16
     (~8,180 matched) then skipped across the project's largest refill
     (+2,700 → 10,934+). Re-runs cost 7 min / 25 min.
  3. Scope conflation: `member_delta_finder2.py` last reported "0 actionable"
     on the ~8,17x-era pool — GemPlayer, OvershellSlot, CameraManager, BinkClip
     did not exist as compiled near-misses then.
- **Fresh evidence (audit re-ran both finders on today's pool):**
  - Inline-policy: 422 named [90,100) near-misses → 36 candidates, ALL n=1
    (`~/tmp/ws7-audit/inline_candidates_2026-07-02.json`). Vein confirmed dry.
  - Member-delta: **4 MEMBER_DELTA classes fire**
    (`~/tmp/ws7-audit/mdf2_2026-07-02.json`, verified by this synthesizer):
    | class | delta | boundary | n_affected | conf |
    |---|---|---|---|---|
    | GemPlayer | −0x10 | ~0x400 (mode 0x434) | 3 | high, consistency 1.0 |
    | BinkClip | +0x4 | 0x4c | 2 | medium |
    | OvershellSlot | −0x8 | 0x34 | 3 | low, coupled-base warn |
    | CameraManager | −0x30 | 0x28 | 2 | low, coupled-base warn |
    GemPlayer spot-verified by direct objdiff:
    `?PlayMissSound@GemPlayer@@QAAXH@Z` retail `lbz 0x434` vs base `0x444` —
    uniform delta above ~0x400, boundary directly observed between 0x27c and
    0x400.
- **Retry experiment (forcemult-apply pattern, one lane per candidate):**
  - Per candidate: `scripts/setup_worktree.sh ~/tmp/wt-md-<class>` → recon
    direction via `mcp run_objdiff` (pass `project_dir`!) → cross-check member
    identity against the `../rb3` oracle header (and `../dc3-decomp` for
    BinkClip, an engine-adjacent class) → one-line header add/remove →
    composed whole-binary A/B in the worktree.
  - Priority order: GemPlayer → BinkClip → OvershellSlot → CameraManager
    (the two coupled-base-warned ones last; a coupled-base recon FIRST, since
    that class historically walls).
  - **Cost:** 4 lanes × 1–2 h (~1 agent-day total).
  - **Success bar:** composed net ≥ +6 strict matches across the 4
    (n_affected sums to 10; historic member-delta class yield +1..+4 each plus
    reveal cascade).
  - **Kill criterion:** net ≤ +1 after all 4 verified A/Bs, OR ≥2 of 4 recon
    as vbase/coupled-base walls → close member-delta **permanently**.
  - **Institutional fix regardless of outcome:** add both finder re-runs
    (inline 7 min, mdf2 25 min) to the post-refill checklist after every
    ≥ +500 strict gain.

## R2. MetaPanel/AppLabel axis-A struct-lever-then-reharvest

- **Parent lever:** "blind identity-transfer harvest (0/10 fresh TUs)"
  (kill doc: `docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md`).
- **Auditor ruling:** PARTIAL_REOPEN (high confidence). Blind
  (unscreened, family-agnostic) harvest waves stay DEAD regardless of outcome.
- **Flaws found:**
  1. The kill doc's own conditional reopen (lines 87–92: "when a struct-lever
     lands a panel-class layout, re-harvest that family") was never attempted —
     `src/band3/meta_band/MetaPanel.h` untouched since scaffold @8b28623.
  2. Conflated denominator: 3–4/10 of the "0/10" TUs failed on oracle VA
     misattribution (a screening bug since fixed by `tools/oracle_quality.py`),
     not body divergence; true body-divergence sample ≈ 6, of which 2
     (MetaPanel, AppLabel) are the fixable single-member axis-A class.
  3. Cost asserted, not measured: the "~600B header reconstruction" dismissal
     vs the reproduced reality — a single uniform **+4 shift** (3 `diff_arg`
     mismatches on `?Unload@MetaPanel@@UAAXXZ`, 99.9% today). Minimal fix is
     one 4-byte member insertion; `tools/field_offset_gate.py --D` pins
     head-region methods without tail reconstruction.
  4. Stale vs today: wave-4 already wired MetaPanel/AppLabel/OvershellPanel
     into `config/45410914/objects.json` (lines ~788/814/825 — port cost sunk)
     and landed the analogous Synth.h layout lever (7d887d0, 0 regressions
     across 24 TUs), proving the workflow cheap.
  5. Honest downgrade (cuts against the reopen): `oracle_quality.py --tu
     MetaPanel.cpp` shows 8/11 GOOD rows are foreign `NewObject()`
     boilerplate; only Exit/Enter/Exiting are genuine. Size at **+3–8 strict
     total**, not the 22/55 the axis table implies.
- **Retry experiment:**
  - **Lane 1 (MetaPanel, decisive cheap probe):** CoW worktree under `~/tmp`;
    stack-layout/struct-offset recon of the +4 divergence on
    `?Unload@MetaPanel@@UAAXXZ`; insert the missing 4-byte member in
    `src/band3/meta_band/MetaPanel.h` before `mMusic` (0x5c region);
    per-symbol objdiff must flip Unload 99.9 → 100 strict — **if it does not,
    kill immediately.** Then re-harvest MetaPanel with PIN-SET = GOOD-oracle
    (`tools/oracle_quality.py`) ∩ `field_offset_gate --tu MetaPanel.cpp --D`
    ∩ obj-defined, targeting Exiting (99.93%), Exit, Enter. STRICT add-only;
    byte-equality is the only positive gate.
  - **Lane 2 (AppLabel, contingent):** same procedure on its 11 genuine
    `AppLabel::Set*` GOOD rows **iff Lane 1 nets ≥2 strict**.
  - **Cost:** 1–2 lanes, warm single-obj builds only (v2 workflow — no
    whole-binary builds inside lanes), one composed A/B at integration;
    ~half an agent-day.
  - **Success bar:** ≥ +3 net strict across MetaPanel+AppLabel, 0 regressions
    in the composed A/B.
  - **Kill criterion:** minimal MetaPanel.h fix fails to flip Unload to
    strict-100, OR post-fix re-harvest nets <2 strict → record the panel
    axis-A wall as body-divergence-in-disguise, close permanently.

## R3. `span_confirm.py` — oracle-plurality span-identity confirmer (tooling leverage)

- **Parent lever:** option-C NO-GOs (string-free topology purity, DC3-map span
  transfer, map-augmentation) —
  `docs/decomp/research/2026-06-30-option-C-scan-directions.md`.
- **Auditor ruling:** PARTIAL_REOPEN (high confidence). All three named
  NO-GOs stay DEAD (Part 2). What reopens is the **flagged-but-never-built
  salvage**: a reverse plurality vote over the committed `dc3_oracle.json`.
- **Flaws found:**
  1. Raw reports of the 4 Opus probes were never committed — only the
     synthesis doc carries the numbers, in a pipeline with a documented
     precedent of a wrong self-reported panel number (topo design's held-out
     17/28 = 0.61 collapsed to 3/23 = 0.13 when actually built).
  2. The "reverse plurality vote 6/7" salvage number was small-n and
     implicitly margin-gated. Audit reproduced at scale
     (`~/tmp/ws7audit/plurality_repro2.py`, all ~717 pinned splits.txt TUs vs
     `dc3_oracle.json`): **50% raw** plurality accuracy overall, 67–71% on
     DC3-shared TUs; a margin gate (n≥5 in-span votes, top ≥ 2× second, top
     ≥ 3) recovers **84% precision (132/158) at 63% coverage**.
  3. Circularity never noted: ws3 spans are *located by* dc3_oracle clusters,
     so confirming them with a vote over the same rows is the signal voting
     for itself. The confirmer is only independent for spans located by
     **non-oracle** means (ws5 case-B, stub-filtered contiguity, ghidriff/ws2
     worklist candidates).
- **Retry experiment:** build `tools/span_confirm.py` (~100–150 LOC, read-only
  over committed `dc3_oracle.json`). Input: candidate `.text` span `[lo,hi)` +
  claimed TU basename. Output: CONFIRM / CONTRA / ABSTAIN via margin-gated
  plurality of in-span oracle rows' `dc3_tu` basenames (gate: n≥5 in-span
  rows, top ≥ 2× second AND ≥ 3; normalize ham↔band twin names, e.g.
  BandCamShot/hamcamshot, hamdirector/banddirector). Calibrate on today's
  pinned splits.txt TUs as ground truth: must show ≥80% CONFIRM precision at
  ≥50% coverage on DC3-shared TUs (measured today: 84% @ 63%).
  - **Deploy:** fused pre-filter in ws5 case-B target selection, the
    stub-filtered-contiguity scan, and ws2 worklist-regen triage. **NEVER** as
    sole gate; **NOT** for ws3 oracle-cluster targets (circular). CONTRA =
    flag for manual review, not auto-reject.
  - **Cost:** ~half a day. **Yield:** indirect — agent-lane savings + fewer
    mis-located pin attempts, not direct matches.
  - **Kill criterion:** if across one full target-selection wave it flips zero
    decisions (no CONTRA surviving manual check, no measurable savings), bank
    as permanently dead.

## R4. Crossval per-fn hint salvage — near-free ws2 rider

- **Parent lever:** "gameid crossval TU-span bracketing"
  (`docs/decomp/gameid/VERDICT.json`, `docs/decomp/gameid/crossval_agree.json`).
- **Auditor ruling:** PARTIAL_REOPEN (high confidence). The span-bracketing
  core stays CONFIRMED_DEAD (over-determined — see Part 2 for the reproduced
  kill numbers and the protocol failures that don't change it).
- **Flaws found (that motivate only this salvage):**
  1. The cheap per-fn hint deliverable was shelved together with the dead span
     goal. Recount today: 146 agree fns → 93 unpinned at verdict → 28 since
     pinned by other routes (~80–86% stem-correct on those — note this is
     BELOW the claimed 0.95, which rested on an n=19 sample), 65 still
     unpinned, 27 duplicated by ghidriff/sysnet worklists, **38 residual
     uncovered** (~2.8 KB, half stub-shaped ≤32B; 24 of 38 in the Quazal
     network region 0x82A40000–0x82B30000, only 3.7% pinned).
  2. Network stems (35 of the surviving 65) were never calibrated — the 25-pin
     calibration set was band3-only. Tag accordingly.
- **Retry experiment:** do **NOT** re-run BinDiff/BSim or the blocked
  fixed-point-seeding variant (superseded by the operational 0.967-precision
  ghidriff pipeline). Only: fold the 38 surviving hints into ws2
  worklist-regen as **low-tier stem-only candidate seeds**. Inputs:
  `docs/decomp/gameid/crossval_agree.json` filtered against (a) current
  `config/45410914/splits.txt` `.text` spans, (b) rb3_addr sets of
  `ghidriff_identities.json` + `sysnet_port_worklist.json` +
  `band3_port_worklist.json` — a ~20-line recount script, **re-run at regen
  time** (all four inputs move; the "38" is a today-number). Tag each hint
  "stem-only, ~80–86% empirical precision, network stems uncalibrated".
  - **Cost:** <1 hour, no Ghidra, no builds.
  - **Success bar:** ≥10 of the 38 corroborated (same TU stem) by regenerated
    ghidriff/BSim identities, or consumed as named pins during the regen wave.
  - **Kill criterion:** ghidriff regen independently labels >30 of the 38, or
    <5 corroborate after one wave → delete `crossval_agree.json` from the
    active-levers list permanently.

## R5. BSim dense-seed rerun — bounded last shot, ws2 rider only

- **Parent lever:** "BSim seed-propagation NO-GO"
  (`docs/decomp/research/2026-06-21-bsim-seedprop-densification.md` + `.json`).
- **Auditor ruling:** PARTIAL_REOPEN (**medium** confidence — the only
  non-high-confidence reopen; treat as optional).
- **Flaws found:**
  1. Seed-source protocol error: only 146 BinDiff-crossval seeds were used
     when thousands of byte-matched anchors were derivable; the doc's "more
     seeds needs the locator first — circular" dismissal is factually wrong
     (matched-decomp anchors come from byte-matching, not any locator).
  2. Headline overclaim: "degrades precision vs plain BSim" is contradicted by
     the doc's own like-for-like table (treatment 0.26/0.30/0.24/0.33 vs
     baseline 0.04/0.10/0.14/0.17 at sim ≥0.5/0.7/0.9/0.95); the claim rests
     on an n=18 subset cell.
  3. Densification zero-result is reachability-confounded: 146 seeds cover
     <1% of 23k real-bodied fns, so ≤2-hop propagation geometrically cannot
     form ≥5-fn stems outside seed neighborhoods.
  4. Decisive precision cells were n=3–41.
  - **Counterweight (why PARTIAL, not FLAWED):** the lever's role is largely
    superseded — ghidriff run3 + BSim fusion already delivered 978 pairs at
    0.900 judged precision *including* call-graph Implied correlators, and the
    documented residue failure mode (same-TU sibling aliasing) is exactly
    where BSim vectors are blind regardless of seed count.
- **Retry experiment (auxiliary signal folded into ws2, not a standalone
  locator):** seeds = `ghidriff_identities.json` 978 ACCEPT Wii↔Xenon addr
  pairs ∪ matched-anchor roundtrip (`scripts/target_symbol_map.json` VA→MSVC
  entries restricted to matched fns, name-normalized against
  `../rb3/config/SZBE69/symbols.txt` for Wii addrs), filtered size ≥44B;
  expect ≥4,000 seeds (≥30× original). Warm assets at `~/tmp/bsim_seed_work/`
  (verified present). Run `tools/ghidra/VTSeedPropDriver.java seeds_dense.json
  /wii seedprop_dense.json 0.0 0.0` (119 s at 146 seeds; budget 4 h wall,
  ghidra_12.2_DEV fork dist). Measure precision by the **consume-side
  protocol** (human-judge 30–50 sampled net-new claims, like ghidriff run3's
  0.900 calibration, and/or wire-and-build gate), NOT BinDiff-agreement;
  dedupe net-new against `target_symbol_map.json` AND the ghidriff ACCEPT tier.
  - **Cost:** 0.5–1 agent-day; no Ghidra-MCP lock conflict (reflinked copies).
  - **Success bar:** ≥1 scattered game TU with ≥8 net-new real-bodied fns at
    ≥90% judged precision (the original GO bar), OR ≥50 net-new fns
    binary-wide at ≥85% precision usable as ws2 loose-tier corroboration.
  - **Kill criteria:** (a) correlate >4 h / OOM; (b) first 30-claim judgment
    batch of net-new sim≥0.90 claims <60% precision; (c) net-new post-dedupe
    high-sim real-bodied count <25 binary-wide → write CONFIRMED_DEAD and
    never revisit.

---

# Part 2 — CONFIRMED KILLS (do not retry; reproduced on today's state)

## K1. Topo-locator (callee-set topological locator) — CONFIRMED_DEAD (high)

- **Original:** `tools/topo_locate.py` @e318789 built and killed at held-out
  precision@1 = 3/23 = 0.13 vs 0.55 bar
  (`docs/decomp/research/2026-06-30-topo-locator-design.md`, BUILD VERDICT).
- **Audit reproduction on TODAY's anchors (10,934):** precision@1 = **3/26 =
  0.115** (Wilson 95% CI upper ~0.29 — cannot reach the 0.55 bar), 21/26
  no-candidate. Anchor growth (+270) grew the pool 23→26 with **zero** new hits.
- **The audit went beyond the original and killed every rescue variant:**
  - Engine-anchor keying via MSVC-name demangling of target_symbol_map
    (the one genuinely untested variant): 1,235→3,539 keys, pool 26→62,
    held-out top1 = **1/62** — makes precision *worse* (popular engine callees
    yield 43–63-candidate sets).
  - Binary-level ICF-twin expansion (size+n_insns proxy): +0 recoveries.
  - 2-hop retail closure: +0. Caller-side signal: 3/26 / 1/26 — worse.
- **Structural root cause verified:** 60% of anchors (6,566/10,934) are
  anonymous `fn_` names (unkeyable — chicken-and-egg with identification
  itself); forward ground truth shows only 4/26 true VAs call ≥2
  oracle-anchored callees, so even a perfect ranker caps at ~0.15. Two minor
  code bugs found (self-consistency guard at `tools/topo_locate.py:364`
  compares demangled vs mangled = always-true; dup-counting of multi-TU Wii
  copies) — both conservative-direction, neither changes the verdict.
- **Probes (ephemeral):** `~/tmp/topo_audit_probe{,2,3}.py`,
  `~/tmp/topo_audit_validate.json`.

## K2. CollideListSubParts de-virtualization — CONFIRMED_DEAD (high)

- **Original:** Bundle 1 of
  `docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md` (lines
  17–38; branch `followup/round3-full-batch` @3879248): devirt DISPROVEN,
  broke 15 matches.
- **Audit reproduction (2026-07-02, devirt reverted on main):**
  `Character::EnableBlinks` = 100.0% and `PanelDir::PanelNav` = 96.8% via mcp
  run_objdiff. Isolation internally consistent (Bundle-2's BandWardrobe note
  proves the probe reverted only Bundle 1). The regression signature (uniform
  vcall −4 across 15+ RndDir/Character-family functions) is mechanically
  unique to vtable-slot removal — misattribution impossible. Decisively:
  15+ functions byte-match retail at 100% **with the virtual slot present**
  (`src/system/rndobj/Dir.h:41`, `src/system/char/Character.h:89`), directly
  proving retail's vtable contains the slot. The sole intended beneficiary
  (PanelNav) gained nothing even with devirt applied; its residual is a
  retail-source body divergence. **No variant** (e.g. devirt + padding slot)
  can both preserve downstream offsets and deliver the claimed benefit.
- One flaw noted for the record: the original lane-C band.exe vtable-read
  claim was wrong — but the composed A/B methodology itself caught it. The
  kill process worked as designed.

## K3–K7. Core kills inside the PARTIAL_REOPEN levers (stay dead)

These stay dead even though a salvage/variant reopened above:

- **K3. gameid crossval TU-span bracketing (span goal)** — over-determined
  kill despite two real protocol failures found by the audit (the advertised
  627-pin fixed-point-seeded variant NEVER ran — blocked by the rb3-Wii Ghidra
  project lock, self-documented as `missing_lever` in
  `~/tmp/gameid/bindiff_calibration.json`; and the Jun 23 BSim calibration
  reused a stale query). Why still dead: (1) stub-masking WAS tested and span
  location stayed dead — dominant-cluster located 5/25 calibration pins, mean
  IoU 0.088, `bindiff_spans.json` = `[]`; (2) the untested seeding remedy is
  **superseded** by the ghidriff+BSim pipeline (978 ACCEPT @0.900, sysnet
  0.967 with full CW mangled signatures vs the hints' stem-only labels);
  (3) the project no longer needs TU-span brackets — splits.txt grew ~625→1,083
  spans via per-fn micro-pinning. Do NOT re-run BinDiff/BSim span experiments.
  Also for the record: the 0.95 hint-precision claim (n=19) empirically
  resolved to ~0.80–0.86 on the 28 since-pinned hints.
- **K4. String-free topology as locator OR purity validator** — locator
  reproduced dead at 0.115 today (K1); the purity-validator variant was never
  explicitly tested but is **dominated**: the same Ob2-inlining root cause
  applies to validation, topo_locate's vote_margin≥2 mode (precision 1.0 at
  ~12% recall) is already banked as a one-shot confirmer, and R3's
  oracle-plurality vote gives span-purity checking at far higher coverage.
- **K5. DC3-map span transfer + map-augmentation** — dead
  (`2026-06-30-option-C-scan-directions.md`; map-augmentation's +0–4 could not
  be independently reproduced — no committed probe artifact — but is small,
  bounded, and consistent with the independently-documented version-divergence
  wall). Adjacent string-anchor numbers reproduced within ≤3% regeneration
  drift (3,789/1,061/1,594 vs documented 3,921/1,067/1,602).
- **K6. Inline-policy hunt** — re-verified dry on TODAY's refilled pool:
  36 candidates, all n=1 (R1 evidence). No force-multiplier remains; per-fn
  n=1 flips are ordinary near-miss work, not a lever.
- **K7. Permuter as wave-driver** — re-verified: `permuter_cache.db` shows 11
  distinct symbols ever →100 across 822 climbs / 88,622 variants (all
  structural starts), **0/23** post-retirement climbs (Jun 21–30, incl. the
  2026-06-30 full-budget pilots) reached 100. The audit-prompt's own premise
  that the permuter hadn't been re-exercised was wrong — it had, on the
  current pool. Stays a per-function spot tool. *Optional bounded residual
  (not a reopen):* the two never-run drivers `fpr_declaration_reorder` /
  `first_use_reorder` against the 5 REGALLOC_FPR_CALLEE fns, expected 0–3,
  do-no-harm isolate; take only if an agent is otherwise idle.
- **K8. Blind identity-transfer harvest waves** — family-agnostic, unscreened
  harvest stays dead regardless of R2's outcome; the "0/10" was partially a
  screening artifact, but screened-and-still-diverged TUs (~4 of the true ~6)
  confirm the MWCC→MSVC body-divergence wall for the non-axis-A remainder.

---

# Honesty gates & verification (for whoever runs the reopens)

- Every reopen above carries its own success bar and kill criterion — they are
  the contract. **Do not soften a kill criterion mid-experiment.** If a bar is
  missed, write the CONFIRMED_DEAD line into this doc's lever entry and stop.
- All match claims must be **strict byte-equality** via objdiff with
  `project_dir` pointed at the worktree; composed whole-binary A/B before any
  patch is handed to the coordinator; 0-regression requirement is absolute.
- The `~/tmp/ws7-audit/*` artifacts are inputs to R1 — if absent, regenerate
  with the committed finders (`tools/member_delta_finder2.py`,
  `tools/inline_policy_finder.py`) and require the SAME 4 MEMBER_DELTA
  candidates to re-fire before spending lanes (if they don't re-fire on the
  then-current pool, R1's premise expired; re-audit before running).
- R4's "38 residual hints" and ws counts are today-numbers; recompute at
  execution time (the repo moved twice during doc-writing day alone).
- Per project memory `feedback_verify_assumptions.md`: any load-bearing claim
  imported from this doc into a new plan gets independently re-verified first.

# Expected yield (summary)

| Reopen | Cost | Expected strict yield | Confidence |
|---|---|---|---|
| R1 member-delta mini-wave | ~1 agent-day (4 lanes) | +6–10 (bar ≥ +6) | high |
| R2 MetaPanel/AppLabel axis-A | ~0.5 agent-day | +3–8 (bar ≥ +3) | high |
| R3 span_confirm.py | ~0.5 day | 0 direct; lane-savings for ws2/ws5 | high |
| R4 crossval-hint fold-in | <1 h | 0 direct; ≤38 low-tier ws2 seeds | high |
| R5 BSim dense-seed rerun | 0.5–1 day | 0–50 ids (likely low; hard kill) | medium |

Total direct ceiling ≈ +18 strict from R1+R2 — comparable to a mid-tier ws4
bundle — plus triage leverage from R3/R4 and a bounded option on R5.

# Recommendation (for the master doc)

R1 (member-delta mini-wave) and R2 (MetaPanel axis-A probe) deserve immediate
execution slots alongside ws1–ws5: together they cost ~1.5 agent-days for an
expected +9–18 strict at high confidence, both with decisive first-hour
go/no-go probes (GemPlayer objdiff direction check; Unload 99.9→100 flip), which
is better EV-per-lane than ws5's near-term +24 and competitive with ws4 —
schedule them right after ws1's drain, before or parallel to ws3. R3
(span_confirm.py) should be built once, as a half-day tooling task, *before*
ws5 case-B target selection and the ws2 triage so both consume it as a fused
pre-filter (never for ws3's oracle-located targets — circular). R4 is a <1-hour
rider that belongs inside ws2's regen script, and R5 should run only if/when
ws2's regen executes anyway, as its auxiliary signal with the hard three-way
kill. Nothing else reopens: the master doc's dead-lever section should be
updated to mark topo-locator and CollideListSubParts as **re-audit-confirmed
dead** (kill evidence reproduced 2026-07-02 on current state), replace the
inline-policy citation (wave-20 grep lanes → the a8716c7 objdiff-driven finder
drained 4x), correct the crossval salvage precision from "0.95" to "~0.80–0.86
empirical, network stems uncalibrated", and add the institutional rule that
both force-multiplier finders re-run after every ≥ +500 strict refill —
that protocol lapse, not any methodology flaw, is what left R1's yield sitting
on the table.

# Open questions

1. GemPlayer's 3-way threshold histogram (0x434/0x400/0x404) — single member
   or two adjacent edits? First lane's recon settles it before header edit.
2. Do OvershellSlot/CameraManager coupled-base warnings indicate a shared base
   class delta (one fix, two classes) or independent walls? Recon lane 3
   before opening lane 4.
3. R5 seed-roundtrip name normalization (MSVC↔CW across
   `target_symbol_map.json` and `../rb3/config/SZBE69/symbols.txt`) has no
   committed tool; budget the first hour of R5 for it or reuse
   `tools/gen_game_target_map.py` internals.
4. Master doc still says main @44f57c6 / 10,870 — needs a state-refresh pass
   once today's landing wave stops (10,936 @385182b as of this writing).

# R1 execution results (exec/r1-member-delta-0702, 2026-07-02)

R1 member-delta apply mini-wave executed and reviewed (handoff:
`docs/decomp/handoff/exec-r1-member-delta-run-2026-07-02.md`). Composed A/B:
**10,936 → 10,941 (+5 strict, 0 regressions)**.

- **GemPlayer −0x10: LANDED (+3).** Phantom = the Wii guitar-FX-core block
  (unk39c/unk3a0/unk3a4-a8); retail Xbox drops it and routes FX via
  mPitchShift. Verified by direct retail disassembly, not oracle inference.
- **BinkClip +0x4: LANDED (+2).** New retail-360-only 4-byte member at 0x3c
  (absent in both rb3-Wii and DC3 — DC3 lacks BinkClip entirely).
- **OvershellSlot −0x8: KILLED (coupled-code).** All drop candidates are used
  by the compiled .cpp; needs a full Wii→360 body port, no retail oracle.
- **CameraManager −0x30: KILLED (coupled-base).** DC3 promoted it to
  Hmx::Object (+0x24) and inserted a 12-byte blend block; retail is a
  non-Object class embedded by value in WorldDir — architectural wall.

Answers to open questions 1-2 above: (1) GemPlayer was a single contiguous
16-byte used-member block, settled by retail disasm; (2) the two walls are
INDEPENDENT (one coupled-code in game UI, one coupled-base in engine world),
not a shared base delta. **Member-delta lever now CLOSED** — all 4 mdf2
candidates dispositioned; re-run the finder only after the next ≥ +500
strict refill.

# R2 MetaPanel axis-A — RESULTS (exec/r2-metapanel-0702, 2026-07-02)

**LANDED-QUALITY: +5 net strict (report +8), 0 regressions.** Composed A/B:
10,936 → 10,944 matched (only `default/MetaPanel` moved). Details in
`docs/decomp/handoff/exec-r2-metapanel-run-2026-07-02.md`.

- The lever was real but not the doc's "insert before mMusic" — the phantom
  `int unk44` (referenced nowhere) did not exist in retail; deleting it zeroed
  every offset diff. Final retail layout: 0x3c mTour … 0x50 mRecentIndices,
  0x5c unk58, 0x60 mMusic, 0x64 mSongPreview, 0xd8 unkd4 (UIPanel base → 0x3c).
- Strict wins: Unload, PickLoopIndex (two body rewrites: single-counter inner
  loop + early `return idx`), Exiting (fn_8255A980), Enter (fn_8255A8F0 — the
  planner's fn_8255AA48 candidate was wrong), SyncGameTimer (fn_8255A9F8).
- Exit (fn_8255A940) = 81.2%: retail-only extra call (thunk fn_8250916C /
  XamGetSystemVersion path) absent from the rb3-Wii DEV oracle. Mapped, not counted.
- AppLabel: NO axis-A lever — 52/100 already perfect, layout retail-correct;
  oracle GOOD rows are VA-misattributed outside the span. Remaining 48@0% =
  manual ID grind, deferred.
