# Fuzzy locator-first reconstruction — design (2026-06-21)

**Status: DESIGN / awaiting alignment before build.** Grounded in the hard-frontier-#2
diagnosis (wf_8dd364e0) + the findings report (docs/decomp/fuzzy-reconstruction-frontier-2026-06-21.md).

## The pivotal finding that reshapes everything

The RB3-specific scattered-TU wall (wave-16 "BandProfile 0/64, best 47.8%") was
**misdiagnosed as a codegen/struct wall.** The #2 probe overturned it:

- The rb3-Wii **BinDiff oracle's own similarity scores** for BandProfile's 152 methods are
  **near-random**: median **0.16**, max **0.65**, **ZERO entries ≥0.70**, 88/152 below 0.30.
- `identity_transfer.py` applied them **blind (no confidence gate)** → carved the **WRONG VAs**.
  6/9 probe methods were flat misattributions (`GetSongHighScore` → an unrelated 0xf0-frame
  loop fn; `SaveSize` → a constructor; `RenameCharacter` → FP geometry math).

⭐ **The real bottleneck is IDENTIFICATION (locating each method), not body reconstruction.**
We can't reconstruct a body we can't reliably locate. Cross-compiler (Wii/MWCC ↔ 360/MSVC)
BinDiff is too weak for RB3-specific game TUs. This is exactly the "better fuzzy match
system" need: **the *match* layer is what's broken.**

## The system: locator-first, three stages

### Stage 1 — HIGH-CONFIDENCE LOCATOR (the missing keystone, build this first)
Per scattered-TU method, fuse multiple weak signals into one high-confidence VA placement
(no single oracle suffices — the rb3-Wii BinDiff alone is median 0.16):
- rb3-Wii BinDiff candidate VA + similarity (the current weak signal).
- **String/constant fingerprint**: the method's referenced string/Symbol literals + float/int
  constants (from our compiled obj) cross-matched to the retail fn's rdata refs at the candidate VA.
- **Callee fingerprint**: the set of `bl` targets (resolved through the identity map) — a method
  that calls Foo/Bar/Baz should match a retail fn calling the same set.
- **Size + CFG shape**: fn byte-size band + basic-block count/branch structure (both PowerPC).
- **Ghidra/BSim semantic p-code** match as the confirmer (the VA-confirmation sweep prototype —
  docs/decomp/research/2026-06-21-songsortnode-va-confirmation.json — did this BY HAND for SongSortNode).
Output: a per-method CONFIRMED / RECON / MISATTRIBUTED / UNPLACEABLE table with a confidence score.
**Quick prerequisite fix:** add a confidence gate to `identity_transfer.py` (reject sim < ~0.7,
or require corroboration) so it stops minting wrong carves.

### Stage 2 — PER-METHOD RECONSTRUCTION (tractable ONLY on CONFIRMED VAs)
The reconstruction workbench: for each confirmed method, assemble the three-way aligned evidence —
(a) rb3-Wii source (logical template), (b) Ghidra retail decompilation (ground truth), (c) objdiff
of our compiled-from-Wii-source vs the retail target — and auto-classify the divergence
(struct-offset / inline-policy / wrong-callee / missing-block / DC3-vs-Wii-revision / pure-codegen-WALL).
An agent reads the dossier, edits MSVC source, rebuilds, re-objdiffs, converges. The body-divergence
classes ARE individually fixable (the session proved this: struct-levers, inline-policy, truncation);
they were just being attempted on mis-located functions.

### Stage 3 — FUZZY METRIC (DONE — tools/fuzzy_progress.py)
So reconstruction COUNTS: STRICT (north star) + FUZZY-CODE whole/wired + completion staircase.
A method reconstructed from 3% → 95% is real progress even before byte-exact. WIRED-fuzzy is
currently 95.5% — the honest "how close is the attempted set."

## Why this is the right shape (vs the wave-16 approach)
Wave-16 skipped Stage 1 entirely (trusted the raw oracle), so Stage 2 reconstructed against
mis-located targets → 0/64. With Stage 1 as a hard gate, Stage 2 only spends effort on
confidently-located methods. SongSortNode is the pilot (Stage-1 sweep done + scaffold ported on
branch hf2-begin1) — finishing it end-to-end validates the loop before scaling.

## Notes / scope
- DC3-as-body-oracle helps ONLY shared engine code (src/system/*); DC3 lacks meta_band/band3/network.
- Build order: (0) identity_transfer confidence gate [quick] → (1) the locator [the keystone] →
  (2) workbench + the SongSortNode pilot → scale to LockStepMgr/MainHubPanel/BandProfile.
- The objdiff case-B fork (banked) only pays off AFTER Stage 1+2 produce byte-matching bodies; keep banked.

## UPDATE (2026-06-21) — the confirmer ALREADY EXISTS, and the prize is huge

The Ghidra/BSim survey (wf_237928af) settled the Stage-1 locator design with on-disk evidence:

### The semantic confirmer is built + validated
- `../ghidra` (branch `bsim-xenon-patches`) ships a complete, runnable **BSim + Version-Tracking
  "Function Matching" correlator** (LSH p-code feature seeds + `USE_ACCEPTED_MATCHES_AS_SEEDS` +
  call-graph propagation), with freeqaz's scale/determinism patches making it tractable on a 65k-fn
  binary. Built dist at `../ghidra/build/ghidra-dist/.../support/bsim` (embedded H2, no postgres,
  VMX128 p-code, MSVC-switch CFG fixes). **No build needed.**
- Prior experiment `docs/decomp/gameid/{VERDICT.json,crossval_agree.json}` (2026-06-09): BSim ALONE
  is degenerate (per-fn precision 0.16–0.36; BandMachineMgr sink absorbs 6759 fns; ~6112 ≤32B stubs
  at sim=1.0). **But BinDiff(conf≥0.7) ∩ BSim(sim≥0.5) = 0.95 per-FUNCTION precision** (18/19 pins),
  producing `crossval_agree.json` = 146 high-precision per-method labels. The old "negative" verdict
  was ONLY against TU-span bracketing — it is **POSITIVE for per-method confirmation**, the exact
  locator-first use case. Do not re-litigate BSim-alone.

### Stage-1 LOCATOR = a weighted-fusion scorer over EXISTING maps (wiring + calibration)
Candidate maps already on disk: `unified_id_rb3wii.json` (BinDiff, 9301), `ghidriff_identities.json`
(978; 913 BSim-derived), `unified_id_callgraph.json` (1555 topology), `fingerprints.json` (string).
`tools/locator.py` = fuse {BinDiff conf, string-fingerprint overlap, callgraph-neighbor agreement,
size ratio, BSim sim} → {va, method, fused_score, class}. **Calibrate weights against the 25 known
game pins + the SongSortNode hand table — do NOT guess.** Tier-1 auto-accept = the crossval ∩ set.
MANDATORY filters: drop ≤32B coverage stubs + the BandMachineMgr sink. Seeds for VT-BSim =
`tools/ghidra/rb3_symbol_map.json` (1139 anchors). The MCP (port 8002) does NOT expose p-code/BSim —
drive headless (`support/bsim` CLI + `GenerateSignatures`/`BSimQueryToJson.java`), serialized (single-process projects).

### THE PRIZE (honest EV)
**589 of 590 game TUs are ICF-scattered (9,126 fns); exactly 1 is contiguous.** A per-method locator
is structurally required for the WHOLE RB3 game layer. Honest recoverable ceiling **~6,000–8,000 fns
(~doubling matched_functions from 9,801)**: per the SongSortNode bands, ~28% RECON (real-bodied,
oracle supplies body) ≈ **2,500–2,700 strongly-recoverable own-method matches**, ~43% UNPLACEABLE-stub
(coverage stubs — fuzzy/denominator only), rest mixed. This is REAL reconstruction effort (15 body-ports
per TU class), not cheap reveals — but it is the endgame lever, not a marginal one.

### EV-ranked build steps (from the survey)
1. [hours] tools/locator.py JSON-fusion over existing maps + the mandatory stub/BandMachineMgr filters.
2. [hours] `generatesigs` on the imported RB3Xenon + rebuild the rb3-Wii H2 BSim DB (VERDICT.json cmds)
   + a ~30-LOC `BSimQueryToJson.java` → a static BSim-sim JSON the fusion consumes. Run once, serialized.
3. [medium] the per-method reconstruction (the SongSortNode pilot is proving this loop now).
4. [med-high] calibrate the fusion weights against the 25 pins + SongSortNode ground truth.
