# Divergence triage — priced buckets

| Bucket | Basis | Fns | Tgt KB | 99.5+ | 90-99.5 | 75-90 | <75 | Fleet | Flip likelihood |
|---|---|---:|---:|---:|---:|---:|---:|---|---|
| MISPAIR | VALIDATED | 191 | 148.1 | 0 | 13 | 39 | 139 | renamer/map fix (tooling, not source) | n/a (re-pair then reclassify) |
| LEVER-SYMBOL | VALIDATED | 9 | 7.2 | 0 | 1 | 4 | 4 | local-static Symbol lever (mechanical) | ~90% |
| LEVER-STRING | VALIDATED | 42 | 21.3 | 0 | 8 | 15 | 19 | dev-vs-retail string divergence lever | ~85% |
| BODY-LEVER | MEASURED | 240 | 115.6 | 0 | 42 | 99 | 99 | LLM grind FIRST (shape-screened, per-stratum priced) | 25% in 70-90 live (MEASURED 2/8); <=5% elsewhere (MEASURED 0/22) |
| BODY-PORT | ESTIMATE | 173 | 9.2 | 0 | 1 | 6 | 166 | LLM grind (75-92 band best ROI) | 45% in 78-96 band; <=10% above 97.5 (survivor bias) |
| STL-CONTAM | MEASURED | 7 | 0.6 | 0 | 0 | 1 | 6 | skip (stlport-version divergence / template-twin mispairs) | ~0% (MEASURED 0/6) |
| BODY-MISSING-PORT | ESTIMATE | 7 | 0.6 | 0 | 0 | 1 | 6 | port missing source first (not a lever grind) | unmeasured — needs source work |
| STRUCT-ARTIFACT | ESTIMATE | 175 | 46.5 | 69 | 69 | 25 | 12 | struct stream | 50-70% — UNMEASURED estimate; run a 20-30-fn calibration wave before funding |
| FORM-DIVERGENCE | ESTIMATE | 146 | 51.3 | 1 | 104 | 31 | 10 | crack-farm / pattern families | ~30% — UNMEASURED estimate; calibrate before funding |
| WALL-VTORDISP | VALIDATED | 60 | 38.9 | 0 | 25 | 15 | 20 | skip (GameMode vtordisp wall) | ~0% (4 independent confirms) (validated) |
| WALL-DEADARG | VALIDATED | 7 | 1.5 | 0 | 6 | 1 | 0 | skip (dead-arg scheduling wall) | ~0% (3 confirms) (validated) |
| RELOC-COLOC | VALIDATED | 160 | 23.6 | 95 | 65 | 0 | 0 | skip (at-limit) | ~0% (validated) |
| UNRELIABLE-EVIDENCE | n/a | 231 | 28.3 | 121 | 77 | 23 | 10 | re-verify with cache-cleared report before any routing | n/a |
| NEEDS-REVIEW | n/a | 221 | 49.6 | 5 | 138 | 47 | 31 | manual triage | case-by-case |
| ZERO-UNMAPPED | n/a | 5770 | 2368.8 | - | - | - | - | splits/mapping work | case-by-case |
| ZS-STL-HELPER | VALIDATED | 84 | 13.3 | - | - | - | - | skip (ICF-merged STL) | ~0% (validated) |
| ZS-MISSING-INSTANTIATION | PROBE | 17 | 1.8 | - | - | - | - | forced-instantiation one-liners (probe-verified 2026-07-19) | high (probe: 2/2 strict flips, +2 report, no collateral) |
| ZS-OTHER | n/a | 183 | 39.6 | - | - | - | - | manual triage | case-by-case |

Basis: MEASURED = calibrated vs decomp.db outcomes; PROBE = probe-verified dry run; VALIDATED = repeat independent confirms; ESTIMATE = unmeasured prior, calibrate before funding.

Note: the "diff_arg (lever)" evidence shape is NECESSARY BUT NOT SUFFICIENT — it screens shape, not root cause. Regswap-cascade, FPR-scheduling, and CSE/FMA walls all hide inside the same reloc-explained-indel shape, which is why the measured BODY-LEVER flip rate collapsed from the ~80% shape-prior to 25%/≤5% per stratum.

## Ground-truth calibration (24-fn grind campaign, 2026-07-19)

| Predicted | FLIP | IMPROVED | AT_LIMIT | STUCK |
|---|---:|---:|---:|---:|
| BODY-PORT | 4 | 2 | 2 | 0 |
| FORM-DIVERGENCE | 1 | 0 | 3 | 0 |
| NEEDS-REVIEW | 2 | 1 | 8 | 0 |
| RELOC-COLOC | 0 | 0 | 1 | 0 |

NEEDS-REVIEW at_limit mass sits at 95.9-98.6% live — the survivor band; vtordisp/dead-arg detectors (R5a/R5b) now auto-skip most of it. 3 pool entries were absent from decomp.db (get_attempts not-found) — coordinator to re-ingest.

## Fundable-fleet expected strict flips

**BODY-LEVER (MEASURED, per-stratum):** BODY-LEVER 113x0.25 (70-90) + 127x0.05 (rest) = 34.6. (non-STL 70-90 count = 113; rest = 127; total BODY-LEVER = 240.)

**Bankable subtotal (MEASURED / PROBE / VALIDATED evidence only):** BODY-LEVER 113x0.25 (70-90) + 127x0.05 (rest) = 34.6; LEVER-SYMBOL 9x0.90=8.1; LEVER-STRING 42x0.85=35.7; ZS-MISSING-INSTANTIATION 17x0.90=15.3; STL-CONTAM 7x0.00=0.0; BODY-PORT(78-96) 6x0.45=2.7. BANKABLE TOTAL expected strict flips: 96.4.

**Unpriced upside (ESTIMATE — calibrate before funding):** FORM-DIVERGENCE 146x0.30=43.8; STRUCT-ARTIFACT 175x0.60=105.0. Estimate-only expected flips (do NOT bank): 148.8.
