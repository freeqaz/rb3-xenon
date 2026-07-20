# BinDiff DC3→RB3 identification spike — GO (2026-07-20)

**Verdict: GO.** Operating point **similarity ≥0.9 AND confidence ≥0.95**,
boilerplate filtered. First executed end-to-end run of the project's
longest-planned lever (see `docs/plans/remaining-bytes-decomposition-2026-07-20.md`
Phase 2). Artifacts: `~/tmp/bindiff_spike/` (BinExports, .BinDiff sqlite,
`run_export.sh`, `ExportBinExport.java`, `evaluate*.py`, LOG.md).

## Results (measured against 16,300 ground-truth pairs)

- 36,967 BinDiff match pairs total (RB3-TU5 primary vs DC3 secondary).
- Precision by confidence: **≥0.95 → 89.8%**; 0.90–0.95 → 25.4%; ≤0.80 → 2–7%
  (confidence is the sharp discriminator — do not relax below 0.95).
- At the operating point: all named 92.9% (n=9,782); strict-100 oracle subset
  89.0%; **non-boilerplate real names 96.4% (n=7,673)**.
- Residual errors: templated-STL type-param swaps, ICF dtor-flavor collisions,
  sibling near-misses (CompileWithTableFog{Linear,Exp,Exp2}); wild ~3%, tiny fns.

## Yield

**1,650 anonymous RB3 VAs receive a named transfer at the operating point;
940 non-boilerplate → ~906 expected-correct new names (~96%).** Names are
spatially clustered per TU → cheap spot-verification, feeds splits carving.
This alone ≈ satisfies correlator round-5's +1,000-name gate.

## Repro recipe (the part that was never proven before)

1. Both Ghidra projects are LOCKED by live pyghidra services (:8000/:8002) —
   **reflink-clone the .gpr/.rep, delete \*.lock\*, export from the clone.**
2. BinExport 10.3.3 (patched version=12.2) is already installed in
   `~/.config/ghidra/ghidra_12.2_DEV/Extensions/` — do NOT install a second
   copy (module-name collision aborts Ghidra).
3. `analyzeHeadless <clone> <proj> -process <program> -readOnly -noanalysis
   -postScript ExportBinExport.java <out>` with isolated `GHIDRA_USER_HOME`.
   Programs: RB3 `default_tu5.xex-c5a170`, DC3 `default.xex-997567` (leaked map).
4. `bindiff --primary=rb3_tu5.BinExport --secondary=dc3.BinExport
   --output_format=bin` (~3 min; "Could not find basic block" warnings benign).
5. DC3 Ghidra names are BARE method names — evaluation/transfer needs a
   depth-aware name parser (naive splitting undercounts precision ~5 pts).

## Campaign plan

1. Extract transfers at sim≥0.9 & conf≥0.95, drop boilerplate (`_M_*`, `__*`
   algos, insert/erase/clear/resize/push_back, template instantiations, dtor
   flavors, vtordisp/adjustor thunks), keep only anonymous RB3 VAs.
2. Reconstruct full MSVC mangled names where possible (DC3 map join); bare
   names that can't be mangled go in as high-confidence carving hints, not map
   entries.
3. Batch-insert via `tu5_map_apply_fragment.py` (textual), full-rebuild gate,
   named-LOST==0, spot-verify per-TU clusters.
4. Second pass (optional): re-run BinDiff seeded with existing pins as manual
   anchors to lift the 0.85–0.9 band.
5. Downstream: names → recarve/splits (Phase 3) → new near-miss pools → grind.
