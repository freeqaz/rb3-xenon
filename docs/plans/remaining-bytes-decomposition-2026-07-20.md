# Remaining-bytes decomposition + path-to-100 review (2026-07-20)

**Snapshot:** live `build/45410914/report.json` @ 18,882 strict (mid-wave).
Measured by this session's analysis (fields: `match_percent_normalized==100.0`
strict, `fuzzy_match_percent` fuzzy — `match_percent` does NOT exist).
Companion: `docs/plans/paths-to-100/README.md` (RFC index),
`docs/plans/decomp-state-2026-07-19.md` (state), `tools/scope_map.py report`.

## The whole binary, partitioned (69,202 fn entries / 10.69 MB code)

| Segment | fns | bytes | % of remaining bytes | Nature |
|---|---|---|---|---|
| **Strict-matched** | 18,882 | 1.87 MB | — | done |
| Near-miss (fuzzy>0, wired TUs) | 2,281 | 0.52 MB | 5.9% | grind pool — the ONLY part current waves touch |
| 0% inside wired TUs | 6,625 | 1.27 MB | 14.4% | 5,981 anon `fn_` (identification gap) + 369 named-0% + 275 CRT/STL |
| Uncarved blobs: vendor-named | ~3,732 | 1.99 MB | 22.5% | **D3DX shader compiler = 1.9 MB of it** + X* APIs; static-lib code, no oracle |
| Uncarved blobs: game/engine-named | 1,389 | 0.40 MB | 4.5% | names known, TUs not carved — immediately actionable |
| Uncarved blobs: **anonymous** | 35,648 | 4.61 MB | 52.3% | the opaque core; 79% are <128 B (funclets/accessors/dtors/thunks) |
| BINK blob | 141 | 0.07 MB | 0.7% | RAD codec, no oracle |

Cross-check `tools/scope_map.py report`: oracle-backed core (game+engine+3P)
= 3.76 MB @ **46.5% matched bytes** (game 51.6%, engine 44.7%, 3P 42.4%);
vendor no-oracle 1.97 MB @ ~0%; 4.47 MB not yet attributed to any source TU.

Near-miss band detail (non-strict, fuzzy>0): 99–100: 1,225 fns/118 KB
(survivor-wall dominated); 95–99: 195; 90–95: 272; **70–90: 265 (the productive
band, measured 24.1%)**; <70: 324. Formal AT_LIMIT verdicts: 190 fns / 131 KB.

## Answers

**Is the rest just manual grind? NO — grind's remaining surface is ~6% of
remaining bytes.** At measured flip rates the grind tail is worth roughly
+400–600 more fns (~19k ceiling, as forecast). The other 94% is not grindable
in its current state: it needs *identification* (names), *carving* (TU
boundaries), and *transfer* (vendor lib bytes) before any per-function work is
even possible.

**How much is truly opaque?** 4.61 MB (43% of the binary) is anonymous —
un-identified, not unknowable. Its structure is favorable: ~16.8k EH funclets
(recarve census) resolve with their parents; ~22k tiny (<64 B) fns are
accessors/dtors/thunks that identification resolves in bulk once neighborhoods
are named. The genuinely unknown residue is whatever survives BinDiff transfer
+ fleet correlation — unmeasurable until those run at scale.

## Path to 100% — phases and the tooling each needs

1. **Grind tail (running, self-driving)** → ~19k strict. Tooling: reprice
   loop already live. Auto-stop trigger armed.
2. **Identification at fleet scale** — the single biggest unlock (makes the
   4.61 MB anon mass workable):
   - a. **Generalize invcorr reloc-masked byte-identity** over ALL unmapped
     targets × ALL compiled base objs (`invcorr_mispair_repoint.py` +
     `tu5_reloc_masked_correlate.py`). ZERO-UNMAPPED measured 3/4 — these are
     bytes we ALREADY compile correctly, unpaired. Free flips + names.
   - b. **DC3→RB3 BinDiff/Ghidra structural transfer at scale** — planned
     since project start, never executed. Ghidra bank ready (:8002, TU5 target
     ~15.1k named; BinDiff at /usr/bin/bindiff; XEXLoaderWV built for the 12.2
     fork). DC3 has the leaked .map = names for the same engine on the same
     platform. This is THE unexecuted force-multiplier for the anon core.
   - c. Correlator round-5 (+1,000-name gate) — fed by (a)+(b)+body flips.
3. **Carving automation** — turn named neighborhoods into splits.txt TUs:
   recarve pipeline (`scripts/recarve/{scan,climb,funclets}.py`, prologue
   screen mandatory) + `fingerprint_match.py autoid` clustering. Also land the
   **jeff asm mis-nest fix** (known +50–200 strict, still open —
   `project_jeff_asm_misnest`).
4. **Vendor byte-transfer (1.99 MB without decompiling)** — D3DX/XGRAPHICS/
   XAudio are static-lib objs, byte-identical across titles linked against the
   same XDK. We have the compiler but NO XDK link libs on disk (checked:
   only `_ossprobe/xboxkrnl.lib`). **Acquire era-correct XDK libs → dtk
   lib-obj transplant** instead of hand-matching 1.9 MB of Microsoft's shader
   compiler. If unacquirable, this is the highest-cost tail in the binary.
   BINK (65 KB): proprietary RAD, same transfer logic if libs surface.
5. **The honest residual** — regalloc/FPR-scheduling/stack-layout walls
   (permuter-class, banned) + AT_LIMIT verdicts scale with the pool. Expect
   low-single-% of bytes to resist strict matching under current rules.
   100.00% strict is asymptotic; the engineering target is high-90s% bytes
   with a documented at_limit ledger.

## Tooling investment ranking (MB unlocked per effort)

| # | Investment | Unlocks | Status |
|---|---|---|---|
| 1 | BinDiff DC3→RB3 transfer at scale | identification over ~4.6 MB anon | infra ready, never run at scale |
| 2 | Fleet-scale reloc-masked correlator | free names+flips from already-compiled bytes (ZERO-UNMAPPED 5,981 in-TU + blob twins) | tool exists, needs generalization pass |
| 3 | Blob→TU carving automation | converts names → workable TUs | recarve pipeline exists, needs autoid-cluster driver |
| 4 | XDK lib acquisition + transplant | 1.99 MB vendor without decomp | acquisition blocked on user |
| 5 | jeff mis-nest fix | +50–200 direct strict | diagnosed, unfixed |
| 6 | reprice/router loop | grind-tail efficiency | live, self-sharpening |
