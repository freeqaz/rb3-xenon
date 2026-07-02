# Frontier work streams — master tracking doc (2026-07-02)

**State at writing:** main @44f57c6, **10,870 / 65,596 functions matched (16.6%)**,
8.32% code bytes strict, 11.46% fuzzy (`build/45410914/report.json` measures).
*(Same-day drift: by end of doc-prep day main was @385182b with 10,936 matched /
8.42% strict / 11.58% fuzzy — landing velocity, recompute at execution time.)*
Band3 ghidriff worklist vein FULLY DRAINED (memory:
`project_worklist_drain_close_2026-07-02.md`). This doc maps every remaining
lever before per-function manual reconstruction, in EV order, and links the
per-stream execution docs (written by the ws-doc workflow, same date).

## The identity pipeline (context for streams 1–2)

The last +1,000 matches came from a cross-binary identity pipeline built on two
local forks:

- **`../ghidra`** (branch `bsim-xenon-patches`): VMX128 SLEIGH (`vmx128`),
  Version-Tracking perf (`vt-parallel-ref-correlator`, `vt-perf-fixes`), BSim
  perf (`bsim-perf-candidatecap`, parallel Phase-B associate-map build, top-K
  candidate cap), and `powerpc-msvc-switch-fix` (3 PowerPCAddressAnalyzer bugs
  fixed for MSVC switch tables). Upstream status VERIFIED 2026-07-02 (ws2):
  issue **#8963 AND PR #8964 were ALREADY FILED 2026-02-12** (open, assigned
  emteere, no reviews) — the untracked draft files in that repo are leftovers;
  only status-check/follow-up remains (commands in ws2 doc).
- **`../ghidriff`** (fork): `--seed-matches`, scored VT reference correlators,
  string-hasher global 1:1-uniqueness gate, STL/template gates, per-match
  VT/Implied/BSim score export, `--matches-only`.

Pipeline output: `ghidriff_identities.json` (978 ACCEPT @ 0.900-precision tier)
→ `tools/gen_band3_port_worklist.py` (232 band3 ids, drained) +
`tools/gen_sysnet_port_worklist.py` (516 sysnet ids, 0.967 human-validated).
Warm BSim assets: `~/tmp/bsim_seed_work/` (`wii.gzf`, `rb3wii.bsim.mv.db`).

## Work streams (EV order)

| # | Stream | Expected yield | Doc |
|---|--------|----------------|-----|
| 1 | **Sysnet worklist drain** — 384/516 identities still net-new vs live `scripts/target_symbol_map.json` (verified 2026-07-02); ~105 already pinned @f9f0d23. Run the proven v2 harvest workflow. | large (band3-twin, higher precision, DC3-shared bodies) | `workstreams-2026-07-02/ws1-sysnet-drain.md` |
| 2 | **Worklist regen at looser tier** — rerun forked ghidriff/BSim below the harvested 0.900 ACCEPT tier; reviewer-reproduction gating (proven in v2) absorbs the precision loss. Also: file the ../ghidra upstream PR (#8963). | fresh tranche for band3+sysnet | `workstreams-2026-07-02/ws2-worklist-regen.md` |
| 3 | **Option-C oracle-cluster port-then-pin** — the 2026-06-30 GO verdicts: DC3-oracle engine body harvest (+40–70) + stub-filtered contiguity (+30–70). Ranked targets in `docs/decomp/research/2026-06-30-option-C-scan-directions.md`; MetaMusic (+37 @7c7823d) and CharClipGroup already consumed. | +60–100 | `workstreams-2026-07-02/ws3-optionc-port-then-pin.md` |
| 4 | **Round-3 banked repairs** — Bundle 2 (FileLoader + ObjDirItr DC3-drift reverts, preserved on `followup/round3-full-batch @3879248`, composed +55/−23 with ~6 repairable stubs) + HamCamTransform lever (needs target-map renaming first; edits at `~/tmp/hct_edited.{h,cpp}`). Source: `docs/decomp/handoff/round3-shared-header-followups-2026-07-02.md`. | +30–50 net | `workstreams-2026-07-02/ws4-round3-banked-repair.md` |
| 5 | **Case-B campaign** — port the 17 case-B TUs (24 ids inside foreign pinned spans, verified real ICF blobs) + activate the banked objdiff fork (`../objdiff` branch `caseb-global-byteeq @b1c92be`, handoff `docs/decomp/handoff/objdiff-caseb-fork-banked.md`, +150–220 ceiling). Includes trivia: remove the 0-yield BaseSkeleton span `[0x82693C20,0x826940A0)` to free `PracticePanel::ToggleGuidePart`. | +24 near-term; +150–220 ceiling | `workstreams-2026-07-02/ws5-caseb-campaign.md` |
| 6 | **Reconstruction-era prep** (after levers) — adopt the tiered fuzzy metric (`tools/fuzzy_progress.py`; fuzzy 11.46% vs strict 8.32% is currently discarded from the headline) + the reconstruction workbench. Sources: `docs/decomp/fuzzy-reconstruction-frontier-2026-06-21.md`, `docs/decomp/plans/fuzzy-locator-reconstruction-design.md`. | metric honesty + the manual era's tooling | `workstreams-2026-07-02/ws6-reconstruction-prep.md` |
| 7 | **Dead-lever re-audit** — DONE 2026-07-02 (7 adversarial Fable audits, all kill evidence reproduced on current state). Scoreboard: 2 confirmed dead, **5 PARTIAL_REOPENs**: **R1** member-delta finder mini-wave (4 live candidates: GemPlayer −0x10, BinkClip +0x4, OvershellSlot −0x8, CameraManager −0x30; the finder was never re-run after the +2,700 refill — protocol lapse, not methodology flaw), **R2** MetaPanel/AppLabel axis-A struct-lever-then-reharvest (the kill doc's own conditional reopen, never attempted; decisive probe = MetaPanel::Unload 99.9→100 flip), **R3** build `tools/span_confirm.py` (oracle-plurality span confirmer, 84% precision @ 63% coverage measured; feeds ws2/ws5 triage, NEVER ws3 — circular), **R4** fold the 38 surviving crossval hints into ws2 as low-tier seeds (<1 h), **R5** BSim dense-seed rerun (medium conf, optional ws2 rider; original used 146 seeds when ≥4,000 derivable). | R1+R2 ≈ +9–18 strict @ ~1.5 agent-days; R3/R4 = triage leverage | `workstreams-2026-07-02/ws7-dead-lever-reaudit.md` |

**Scheduling per ws7:** run R1+R2 right after ws1's drain (better EV-per-lane
than ws5 near-term); build R3 once *before* ws5 target selection and ws2 triage;
R4 rides inside ws2's regen script; R5 only if/when ws2 runs anyway.
**Institutional rule (new):** re-run BOTH force-multiplier finders
(`tools/inline_policy_finder.py` ~7 min, `tools/member_delta_finder2.py` ~25 min)
after every ≥ +500 strict refill — the R1 yield sat on the table because this
wasn't done.

## Dead levers — ws7 re-audit outcomes (2026-07-02; evidence docs)

Full detail per lever in `workstreams-2026-07-02/ws7-dead-lever-reaudit.md`.

- **Topo-locator**: **RE-AUDIT-CONFIRMED DEAD** (reproduced 2026-07-02 on 10,934
  anchors: precision@1 = 3/26 = 0.115; every rescue variant — engine-anchor
  keying 1/62, ICF-twin expansion +0, 2-hop +0, caller-side worse — also killed;
  60% of anchors are anonymous `fn_` = unkeyable)
  (`docs/decomp/research/2026-06-30-topo-locator-design.md`).
- **CollideListSubParts de-virtualization**: **RE-AUDIT-CONFIRMED DEAD** —
  15+ functions byte-match retail at 100% *with the virtual slot present*;
  regression signature (uniform vcall −4) mechanically unique to slot removal
  (`round3-shared-header-followups-2026-07-02.md` Bundle 1).
- **BSim seed-propagation as standalone locator**: dead, but see ws7 **R5**
  (dense-seed rerun as an optional ws2 auxiliary signal — the original run's
  146-seed protocol was underpowered and its "degrades precision" headline is
  contradicted by its own like-for-like table)
  (`2026-06-21-bsim-seedprop-densification.{md,json}`).
- **BinDiff/BSim TU-span bracketing**: dead, over-determined — and superseded
  (splits.txt grew ~625→1,083 spans via micro-pinning; ghidriff pipeline
  delivers 0.900–0.967 precision with full mangled names). CORRECTION: the
  crossval hint precision is **~0.80–0.86 empirical** (measured on the 28
  since-pinned hints), not the claimed 0.95 (n=19); network stems uncalibrated.
  Salvage = ws7 **R4** (`docs/decomp/gameid/VERDICT.json`).
- **String-free topology purity scan / DC3-map span transfer / map-augmentation /
  bulk DC3 oracle naming**: dead (`2026-06-30-option-C-scan-directions.md`,
  `2026-06-22-dc3-oracle-built-engine-naming-dead.md`). Salvage = ws7 **R3**
  (`span_confirm.py`, the flagged-but-never-built oracle-plurality confirmer).
- **Inline-policy binary hunt**: dead — CITATION CORRECTED: the real kill is the
  objdiff-driven `tools/inline_policy_finder.py` (@a8716c7) drained 4x, not the
  wave-20 header-grep lanes; re-verified dry on today's pool (36 candidates,
  all n=1). The sibling **member-delta finder is NOT dead** — see ws7 **R1**.
- **Permuter as wave-driver**: dead, re-verified (`permuter_cache.db`: 0/23
  post-retirement climbs reached 100, incl. the 2026-06-30 full-budget pilots);
  per-function spot tool only. Optional do-no-harm residual: the two never-run
  drivers `fpr_declaration_reorder`/`first_use_reorder` on the 5
  REGALLOC_FPR_CALLEE fns (expected 0–3).
- **Blind identity-transfer harvest waves**: dead for the family-agnostic form;
  the "0/10" was partially a screening artifact (3–4 were oracle-misattribution,
  since fixed by `oracle_quality.py`). The axis-A panel subset reopens as ws7
  **R2** (`docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md`).

## The wall (what remains after streams 1–5)

Class-B string-poor ICF-scattered game panels are un-locatable by every tested
oracle, and ported MWCC→MSVC game bodies diverge (BandProfile 0/64). That's the
manual-reconstruction frontier stream 6 preps for.
