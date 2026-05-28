# Unified oracle → pin ranker (`tools/pin_candidates.py`)

Designed by `docs/plans/execution-schedule.md` §2 (item **[B]**). Turns the five
oracle sources into the next splits.txt pin wave. Pure read-only data; **no
build, no main-tree mutation.**

## What it does

```
merge oracles by addr → consensus tiers → source-present gate
  → group by TU (drop already-pinned) → dominant-cluster span snap → rank/emit
```

1. **Merge** `unified_id.json` (bindiff + autoid), `unified_id_callgraph.json`,
   `unified_id_rtti.json`, `unified_id_vtable.json` by `rb3_addr`
   (13,558 records → 13,491 unique addrs; 67 carry ≥2 oracles, all agree).
2. **Consensus tiers** (weights from the measured per-oracle precision in
   `exploratory-techniques.md` §1):
   - `S` multi-oracle agreement OR bindiff `similarity==1.0` (~0.98)
   - `A` callgraph `cg_tier==multi` (0.94)
   - `B` rtti `HIGH` (0.80; raw 71%, ~85% post-ICF)
   - `C` callgraph single / bindiff sim<1.0 / autoid-only (0.75)
   - `C-` rtti `LOW` / vtable-only / contested → **review-only, excluded from auto-pin**

   Multi-oracle agreement promotes to `S` (+0.15); name disagreement (−0.30) → review.
3. **Source-present gate.** Keeps only addrs whose dc3 `bindiff_src` resolves
   into OUR `src/` tree. Resolution is **path-tail first** (disambiguates the 32
   basename collisions — `ShaderMgr.cpp` rndobj vs rnddx9, `Utl.cpp` ×6, …),
   then **unambiguous-basename** fallback (dc3 `lazer/meta_ham/*` → our
   `band3/meta_band/*`); ambiguous basenames go to the review bucket.
4. **Group by TU, drop pinned.** Pinned = a basename present in **`splits.txt`
   OR `objects.json`** (the latter catches wired-but-unpinned TUs, the D2/D3
   waves' territory, which are not "NEW").
5. **Dominant-cluster span snap.** Oracle hits for one dc3 TU are NOT one tight
   cluster — there is a dominant contiguous cluster plus scattered ICF/inline
   outliers across the whole binary (e.g. Rnd_Xbox: 27 of 42 hits in 0x35cc,
   15 outliers across 8 MB). A bare `[min,max]` hull is meaningless, so the tool
   gap-splits (default 64 KiB) and pins the **dominant cluster only**, snapped to
   `symbols.txt` symbol boundaries (`start := largest sym-start ≤ min`,
   `end := smallest sym-end ≥ max`). Outliers are counted, not spanned.
6. **Rank** by consensus-weighted oracle-fn count of the dominant cluster
   (Σ per-fn confidence), tighter span as tiebreak. **Emit** the wave.

## CLI

```bash
venv/bin/python tools/pin_candidates.py rank
# --basename-match   reproduce execution-schedule.md S2.4's inflated basename grouping (audit)
# --cluster-gap 0x10000   gap that splits a TU's hits into clusters (default 64 KiB)
# --oracles / --symbols / --splits / --objects / --src-root / --out / --report / --review / ...
```

Outputs (all gitignored, regenerable):
- `pin_candidates.json` — ranked machine-readable wave (consumed by the bulk-wire pass).
- `pin_candidates_report.txt` — human-readable ranking.
- `pin_candidates.splits.append` — paste-ready splits.txt blocks (ranked, annotated).
- `pin_candidates.objects.append` — objects.json `NonMatching` entries, grouped.
- `pin_candidates_review.json` — ambiguous-src + name-disagreement + C- queue (manual).

## Verified numbers (2026-05-28, live data)

- **185 NEW-pinnable TUs / 857 oracle fns** (620 in dominant clusters, 618
  auto-pin tier S/A/B/C; 237 scattered outliers excluded from spans).
- Source-present gate passes **6,972** addrs in `--basename-match` mode —
  bit-for-bit the plan's S2.4 figure. **25** TUs `jeff_blocked`.
- **Divergence from the plan's 194/981 is explained, not a bug:** the plan's
  S2.4 snippet grouped purely by basename (lumping XDK `xgraphics/main.cpp`
  +`nuispeech/main.cpp` into our `Main.cpp`, conflating rndobj/rnddx9
  `Utl/Movie/ShaderMgr`) and only excluded `splits.txt` (not `objects.json`).
  The 194 → 177 → 185 chain: 194 (plan, splits-only basename) − 17
  (objects.json-wired TUs) = 177 (compat); collision-aware tail resolution
  recovers the correctly-attributed TUs back to 185, with the same 857 oracle
  fns. The tail-resolved count is the **correct, pinnable** one.

## symbols.txt freshness (auto-detected)

The tool detects whether `symbols.txt` predates the jeff phantom-symbol prune
(`project_jeff_asm_misnest.md`) by the presence of the documented `fn_82BF8E48`
phantom, and labels its output STALE vs FRESH. As of this writing symbols.txt is
**FRESH** (a post-fix re-SPLIT ran; 66,838 → 65,104 fn syms, 1,734 phantoms
pruned). 1,354 **residual** overlap regions remain (legit tail-call /
pdata-anchored mis-sizing, not phantoms); spans touching them are flagged
`jeff_blocked` for hand-verification. All spans are PROVISIONAL.

## Recommended pin wave

Pin the highest-confidence subset first: the **top ~40 TUs with density ≥ 25%
and not `jeff_blocked`** (Archive 64.6%, MidiSynth 66.7%, ShaderMgr 44.7%,
Cache_Xbox, DataFile, Voice, BinStream, …). These are tight, high-S-tier
clusters. Skip low-density TUs (e.g. PlatformMgr_Xbox 1.7%) — their hits are
spread, so the span will pull in many unrelated fns and tick few matches.
Expected first-wave yield, using the wave-1 ~1.7 byte-match fns/TU rate
discounted for normalized-match rejecting reg-swaps/ICF: **+60–150 matched fns**;
the rest become permuter feed + source coverage.
