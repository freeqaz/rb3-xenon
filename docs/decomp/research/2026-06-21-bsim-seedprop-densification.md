# VT-BSim Seed-Propagation Densification — Experiment Verdict (2026-06-21)

**Question:** Does the fork's VT-BSim **seed-propagation** correlator
(`USE_ACCEPTED_MATCHES_AS_SEEDS` + call-graph propagation), seeded with
high-confidence Wii↔Xenon anchors, **densify** per-TU function identification for
the scattered RB3-specific game layer (turn singletons into clusters)?

## VERDICT: **NO-GO**

VT seed propagation does **not** densify per-TU clusters and it **degrades**
precision relative to a plain BSim query. Neither method recovers the prior
BinDiff∩BSim 95%-precision regime. **No scattered game TU reaches the GO bar**
(≥8 newly-located, unpinned, real-bodied fns at ≥90% calibrated precision). The
densest game cluster propagation produced is 3–4 functions, and those are
themselves low-precision.

This is a **clean negative**: the experiment ran end-to-end (Stage 0–4 complete,
both baseline and treatment measured); the result is honest and not inflated.

---

## What was built (all reproducible; scripts in `tools/ghidra/`)

### Stage 0 — Mechanism recon (decisive design finding)
Reading the fork sources
(`BSimProgramCorrelator{,Matching,Factory}.java`) established that the BSim
correlator is a **Version Tracking program-correlator**, *not* an H2-DB query:

- It computes BSim feature vectors **in-session** via the decompiler for **both**
  the source (Wii) and destination (RB3Xenon) programs.
- It reads **ACCEPTED `VTAssociation`s as seeds** (`findAcceptedSeeds`) and
  extends them along the **call graph** — round 0: Children/Parents; round 1:
  GrandChildren/Siblings/Spouses/GrandParents; plus single-parent hole-patching.
- **No external H2 BSim DB is needed for the treatment.** The H2 DB is only
  needed for the plain-query **baseline**.
- **Cross-architecture is supported:** `getWeightsFile(PowerPC:BE:64:A2ALT-32addr,
  PowerPC:BE:32:Gekko_Broadway)` returns `lshweights_nosize.xml` (different sizes,
  both ∈ {32,64} → size-independent weights). The MWCC-Wii ↔ MSVC-Xenon pair runs
  fine — **not** a blocker.

### Setup (isolation-safe)
- Both live MCP-locked projects (`RB3Xenon` :8002, `RB3` :8001) were left
  untouched. Worked entirely on **btrfs-reflinked copies** under
  `/home/free/tmp/bsim_seed_work` (`/tmp` is tmpfs, too small for the 3.8 GB Wii
  rep). The unlocked top-level `RB3Xenon.rep` was used for the target.
- The Wii oracle program (`band_r_wii.elf-781439`) was exported to a 135 MB gzf
  (`ExportToGzf.java`) and imported into the RB3Xenon project copy as `/wii` (no
  re-analysis).

### Seeds
150 high-confidence Wii↔Xenon anchors from `unified_id_rb3wii.json`
(conf ≥ 0.85 ∧ sim ≥ 0.70 ∧ size ≥ 32) ∪ crossval agree-fns; **146 applied as
ACCEPTED** (4 dest functions missing). The anchors are sparse and scattered
across 110 stems (mostly singletons) — already a warning sign for call-graph
leverage.

### Treatment (`VTSeedPropDriver.java`)
`new VTSessionDB(src=Wii, dst=RB3Xenon)` → add seeds as ACCEPTED manual matches →
`BSimProgramCorrelatorFactory` with `USE_ACCEPTED_MATCHES_AS_SEEDS=true` →
`correlate` (119 s) → export all matches.
- 394,861 potential pairs discovered → **3,735 result matches**.
- **0** new matches reached ACCEPTED status (all `AVAILABLE`/`BLOCKED`; the 67
  ACCEPTED rows are the seeds re-listed).
- 163 NEW non-seed real-bodied (>44 B) matches at sim ≥ 0.90 (127 at ≥ 0.95).
  Category mix of the high-sim new claims: **Quazal/network 61, game/other 50,
  STL 7, engine 5, zlib 2** — i.e. dominated by shared middleware, not the
  RB3-specific game code that is the actual target.

### Baseline (`BSimQueryToJson.java` + H2 DB)
Plain BSim query of RB3Xenon against a freshly-built `rb3wii.bsim`
(`medium_nosize`) H2 DB of the Wii oracle. Signed 35,056 fns, results for 33,561.
(Commit hit a JDK-26 XML entity-size limit; fixed by re-running `bsim commitsigs`
with `JAVA_TOOL_OPTIONS=-Djdk.xml.maxGeneralEntitySizeLimit=0`.)

---

## The decisive numbers

### Cross-tool precision (vs the independent BinDiff oracle as ground truth)
Non-seed matches whose Xenon VA is also covered by BinDiff(conf ≥ 0.9); "agree" =
same Wii class or same normalized name.

| sim ≥ | Treatment precision | Baseline (plain query) precision |
|------:|--------------------:|---------------------------------:|
| 0.50  | 0.26                | 0.04                             |
| 0.70  | 0.30                | 0.10                             |
| 0.90  | 0.24 (broad-n203: 0.19) | 0.14 (robust subset: 0.39)   |
| 0.95  | 0.33 (n=3, thin)    | 0.17                             |

Both are **far below** the prior BinDiff∩BSim 95%. High-sim verifiable subsets are
statistically thin (the propagated NEW matches rarely land on VAs BinDiff opined
on); the broad-threshold measurement (n=55–203) confirms ~**18–24%** for the
treatment. Disagreements are real errors — `_restgpr_23/25/26` compiler stubs
propagated onto unrelated functions, `BandCharacter::StartLoadClips` mapped where
BinDiff says `DirectInstrument::NoteOff`, many cross-class mismatches.

### Per-TU densification (non-Quazal game stems; NEW located real-bodied @ sim ≥ 0.90)

| | stems ≥ 8 | stems ≥ 5 | stems ≥ 3 | densest |
|---|---:|---:|---:|---|
| **Treatment (seed-prop)** | **0** | **0** | 6 | NetGameMsgs 3, GemManager 3 |
| Baseline (plain query) | 8 | 11 | 16 | BandOffline 36, GemManager 21 |

**Seed propagation produced *less* dense per-stem clusters than the plain query**,
and both are low-precision. Propagation did **not** turn singletons into clusters.
The plain query is denser but ~40%-precision contaminated, so its clusters are
unusable without an intersection filter (which the prior experiment already
showed yields only ~146 fns binary-wide, mostly singletons).

---

## Important methodology correction (why pinned-range calibration is invalid here)

`splits.txt` `.text`-range **stem-equality cannot measure precision for scattered
game TUs.** Proof: the known-correct seed `GemTrainerPanel::GemTrainerPanel()`
(a BinDiff conf ≥ 0.85 pair) sits at Xenon `0x82266818`, **inside** the
`CharBonesMeshes` pin `[0x82265608, 0x82266ce0)`. `GemTrainerPanel` is unpinned
and **physically interleaved** into another TU's carved span — exactly the
ICF-scatter wall. 39/40 seeds-in-pinned-ranges "failed" the range test → the
**test** is wrong, not the seeds. Precision was therefore measured against the
independent BinDiff oracle, not pinned ranges. (This is itself a useful artifact:
it demonstrates that range-containment is not ownership for scattered TUs.)

---

## Why it fails / what would change it

- The 146 anchors are sparse and scattered (110 stems, mostly singletons), so the
  call graph offers **little dense local structure** to propagate through.
- Call-graph extension trades precision for a small, noisy recall bump: it drags
  in wrong neighbors (stubs, sibling-class methods) faster than correct ones.
- **What would actually help** (the standing roadmap reframe, not this): a
  high-confidence **LOCATOR** that *fuses* multiple independent signals
  (BinDiff + string/callee fingerprint + Ghidra BSim + size/CFG) and only then
  reconstructs per-method on confirmed VAs. VT seed propagation is **not** that
  locator. Bootstrapping propagation with many correct same-TU anchors would need
  the locator first — circular. The `BinDiff(≥0.7) ∩ BSim ∩ third-signal`
  intersection restores ~95% precision but adds no recall to the ~146 the prior
  crossval already gives.

**Recommendation:** do not pursue VT-BSim seed propagation as the
scattered-game-TU locator. The `fuzzy-locator-reconstruction-design.md`
multi-signal locator remains the right path; this experiment removes one
candidate (call-graph seed propagation) from the menu with hard numbers.

---

## Reproduce

Fork dist: `/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV`
Scratch: `/home/free/tmp/bsim_seed_work` (heavy) + `/tmp/bsim_seed` (artifacts+scripts)

```bash
# 0. reflink the UNLOCKED top-level RB3Xenon.rep + the Wii RB3.rep into /home/free/tmp/bsim_seed_work
# 1. export Wii program to gzf, import into RB3Xenon project copy as /wii
analyzeHeadless /home/free/tmp/bsim_seed_work RB3 -process band_r_wii.elf-781439 \
  -noanalysis -readOnly -postScript ExportToGzf.java .../wii.gzf
analyzeHeadless /home/free/tmp/bsim_seed_work RB3Xenon -import .../wii.gzf -noanalysis

# 2. BASELINE (H2 DB plain query)
bsim createdatabase file:.../rb3wii.bsim medium_nosize --name rb3wii
bsim generatesigs 'ghidra:/home/free/tmp/bsim_seed_work/RB3Xenon?/wii' .../xmldir \
  --bsim file:.../rb3wii.bsim --commit --overwrite      # commit may hit JDK26 XML limit; then:
JAVA_TOOL_OPTIONS=-Djdk.xml.maxGeneralEntitySizeLimit=0 bsim commitsigs file:.../rb3wii.bsim .../xmldir
analyzeHeadless /home/free/tmp/bsim_seed_work RB3Xenon -process default.xex -noanalysis -readOnly \
  -postScript BSimQueryToJson.java file:.../rb3wii.bsim baseline_matches.json 5 0.0 0.0

# 3. TREATMENT (VT seed propagation)
analyzeHeadless /home/free/tmp/bsim_seed_work RB3Xenon -process default.xex -noanalysis \
  -postScript VTSeedPropDriver.java seeds.json /wii seedprop_matches.json 0.0 0.0
```

Scripts (left for review, **not committed**): `tools/ghidra/VTSeedPropDriver.java`,
`tools/ghidra/BSimQueryToJson.java`, `tools/ghidra/ExportToGzf.java`,
`tools/ghidra/SpotCheck.java`. Measurement python + intermediate artifacts under
`/tmp/bsim_seed/`.
