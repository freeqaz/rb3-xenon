# DC3↔RB3 BinDiff oracle BUILT (a great engine identifier) — but engine naming is DEAD for STRICT matches (ALIVE for fuzzy)

Date: 2026-06-22. Builds on `2026-06-21-dc3-engine-oracle-feasibility.md` (GO) and
`2026-06-21-dc3-engine-vein-yield-pilot.md` (LightPreset: paired engine near-misses diverge).

## The oracle (NEW reusable artifact: `dc3_oracle.json` + `tools/build_dc3_oracle.py`)
A DC3-VA ↔ RB3Xenon-VA BinDiff map, **same-compiler (Xbox-360/MSVC)** so the structural
pairing is strong: **33,987 pairs, 18,948 high-conf (sim≥0.7), engine sim MEDIAN 0.90**
(vs the 0.16 cross-arch rb3-Wii floor), 99.6% carry a DC3 mangled name + `dc3_tu`. This is
the **best engine *identifier* we have** — far better than the Wii oracle for `src/system/*`.
Row schema: `{dc3_va, rb3_va, dc3_name, dc3_tu, similarity, confidence}` sorted by sim desc.

### Build-recipe corrections (for future rebuilds — the feasibility recipe was slightly off)
- Ghidra program file names are NOT `default.xex` — DC3 = `default.xex-997567`, RB3Xenon =
  `default.xex-35adb6` (`-process` needs the exact name). Projects: DC3 =
  `dc3-decomp/ghidra_projects/DC3/DC3`; RB3Xenon = `rb3-xenon/ghidra_projects/RB3Xenon/RB3Xenon`
  (the 375 MB nested .rep the MCP uses, NOT the stale top-level `.rep`).
- BinExport ext was only under `~/.config/ghidra/ghidra_12.1_DEV/Extensions`; the build Ghidra
  is 12.2_DEV → installed it into `~/.config/ghidra/ghidra_12.2_DEV/Extensions/BinExport`
  (version-bumped). Export BOTH programs from the SAME Ghidra (`ghidra/build/ghidra`, 12.2) +
  same BinExport so basic-block boundaries align (no 12.1/12.2 SLEIGH skew).
- ⚠ **`rb3_va != dc3_va`**: the feasibility recipe's "same load addr ⇒ same VA" is REFUTED by
  the data (only 183/33,987 coincidentally align). DC3 and RB3 share preferred load 0x82000000
  but have DIFFERENT .text layouts (LightPreset @0x824981xx in RB3 vs 0x8283Dxxx in DC3). The
  oracle's value is the **BinDiff structural pairing**, not VA equality. Validation still PASS:
  known-100% engine methods are paired at sim=1.0 (LightPreset/Box methods).

## The naming pilot (MeshAnim) — DEAD for STRICT
Picked rndobj/MeshAnim.cpp (best real-bodied oracle coverage). Named all **16** unpaired
anonymous `fn_*` targets via the oracle (ADD-ONLY to target_symbol_map) → rebuilt → **0/16
byte-reproduced, real_net_delta = +0** (composed, deterministic 9888→9888):
- **6** are MeshAnim-own STL bodies that exist in our obj — once named they DO pair (no longer
  0%-no-body) but **byte-DIVERGE**: Vector2-deserialize 82.6%, 3× vector-deserialize 94.4%,
  resize/Key-deserialize 0%. Permuter-class STL-instantiation regalloc/scheduling — the
  LightPreset outcome, NOT GemManager's clean reproduction.
- **10** are FOREIGN bodies (RndTransAnim/RndFur/PhysicsVolume/ShaderMgr/…) — TUs MeshAnim
  doesn't compile, so no body to pair. The pinned span is a mixed/ICF-folded blob, not
  MeshAnim-own (the location wall again).

## Decisive conclusion
**Naming is NOT the engine bottleneck** — the oracle correctly names the targets and the
rename pipeline pairs them. The bottleneck is (a) **codegen divergence** (even own engine STL
bodies land at 82-94%, permuter-class) and (b) **mixed/ICF span location** (foreign bodies in
the span). Bulk-naming the engine converts 0%-no-body slots into 82-94% near-misses — it
**moves the FUZZY/WIRED metric but the STRICT matched count by +0**. Confirmed twice now
(LightPreset paired set + MeshAnim naming).

⇒ The **strict-match engine harvest is DEAD** via oracle naming. Do NOT bulk-name for strict.

## What the DC3 oracle IS good for (bank it)
1. **The FUZZY metric** — if the goal shifts to fuzzy_progress.py's WIRED/staircase (the user's
   "byte-exact is unrealistic, needs to be fuzzy"), bulk-naming engine anon targets via the
   oracle is the lever (0% → 82-94% en masse, refills the body-port/permuter pool visibly).
2. **Identification / a future locator** — sim-0.90 same-compiler pairing is the strongest
   engine-side identity signal; feed it into `tools/locator.py` as the engine CONFIRMED source
   (the analogue of what BSim∩BinDiff was for game code, but actually high-precision here).
3. **Per-method reconstruction on CONFIRMED VAs** — the oracle locates+names; the remaining
   work is per-method body-ports (permuter/inline/struct) on those confirmed targets.

Pilot branch `dc3-naming-pilot @f3169af` = +0 strict, NOT landed (kept as the documented
DEAD-for-strict / ALIVE-for-fuzzy record). Oracle + tool committed for reuse.
