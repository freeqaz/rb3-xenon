# Native critical-path cycle 12 — results (2026-07-24)

Foreman: native critical-path cycle 12. Baseline main **24,834** → final main
**24,899** (`measures.matched_functions`, composed full A/B, report.cache cleared).
**Net +65 strict, 0 strict regressions.** 5 Opus workers (M0/M1 lanes + LANE-2
census), foreman landed serial via `scripts/harvest/land.sh` union-resolve.

## Landed (per worker)

| TU(s) | milestone | Δstrict | mechanism | commit |
|---|---|--:|---|---|
| rndobj/Anim.cpp + obj/Dir.cpp | M0 | **+28** | Anim +27 = unwired-owner scatter-include `#include "rndobj/Dir.cpp"` (guarded gRev rename) pairs whole RndDir COMDAT+funclet cluster; Dir +1 = rb3-Wii `ObjectDir::Load` body restore | c71ac07c |
| SongParser 3rd span `0x827848CC..0x82788288` | (song) | **+16** | pin continuation span + carve 2 interleaved SongCollision holes → `size_order_automap` recovers 12 pairings (source already complete; pure MAP recovery, 0 source edits) | 3239ee21 |
| obj/DataFunc.cpp | M0 | **+5** | pure map-pairing — 5 byte-perfect `DEF_DATA_FUNC` statics were unmapped; `tu5_reloc_masked_correlate.py` → `target_symbol_map.json` entries | 1cc61dc7 |
| meta_band/BandSongMetadata.cpp | M1 | **+12** | function-local-static Symbol lever on 12 accessors + companion VA→name map pairings + Ghidra code-shape fixes | ab3bdd01 |
| meta_band/MusicLibrary.cpp | M1 | **+2** | body-only eval-order reorders (SkipToNextShortcut, DeleteHighlightedSetlist) | 5dd4c83a |
| obj/DirLoader.cpp | M0 | **+1** | drop 2 dc3-newer-than-retail blocks in LoadHeader (MILO_ASSERT DCE + rev>0x1c dead branch) | 2ecf080e |
| math/Geo.cpp | M0 | **+1** | retail `Normalize(Plane)` all-zero early-return restore (rb3-Wii) + map pin | 305c2fb2 |

Accepted slip (documented, not strict): `default/TrackDir fn_827DF248` 99.8→94.0
fuzzy-only — Anim's new RndDir COMDATs shifted the global ICF-alias map so objdiff
pairs it against a different 94% ICF twin; its own bytes are unchanged.

## Strategic findings (reshape the frontier)

1. **The scope-map / decomp.db percentages were badly stale** — DirLoader read
   50.9%/114-rem but was actually ~88%/27-rem. **decomp.db re-ingested this cycle**
   (`scripts/ingest_report.py build/45410914/report.json`; 5,299 complete / 65.4%
   avg). Re-ingest before pricing the next wave.
2. **Full-file-port stub vein is ~98% DRAINED** (census: 294/412 CORE+SOON TUs
   already ported; only 6 oracle-fuller candidates remain, the fullest —
   BeatMatchController — a known no-pairing wall). Est residual yield ~+3..+12.
   See `docs/plans/repin-batch11-stub-census.md` + `scripts/harvest/stub_census.py`.
3. **The real successor lever = MAP RECOVERY, not source porting.** The two
   biggest wins (SongParser +16, DataFunc +5) added ZERO source — they paired
   already-byte-perfect target functions that lacked `target_symbol_map.json`
   entries, via **continuation-span pins + `comdat_scatter_scan.py`** (SongParser)
   and **`tu5_reloc_masked_correlate.py`** (DataFunc). Anon-`fn_`
   `size_order_automap`-recovery on *existing* pinned units was **falsified**
   (0 pairings on DataFunc/UIStats/SessionMgr/MoveMgr/NextSongPanel) — SongParser's
   win came from a *new* byte-matching span, not residue.
4. **Unwired-owner scatter-include still pays big** (Anim +27) — a pinned span can
   contain scattered COMDATs from a fully-unwired in-tree `.cpp`; a guarded
   `#include "<owner>.cpp"` pairs the whole cluster.

## M0/M1 completion status (native runtime)

- **M0 (obj/utl/os/math + Anim):** DirLoader/Dir/DataFunc/Geo all improved; the
  remainder is near-miss walls (strcpy-intrinsic `cmplwi/extsb`, EH funclet-echoes,
  guard-thunk `??__F`, FP-scheduling/regalloc — permuter-off). Anim still 51.1%
  (funclet/vtable-thunk heavy). M0 source is hardened; no new stub unblocked in
  `native/src/dta_link_stubs.s` this cycle.
- **M1 (song load/metadata):** MusicLibrary 82.7%, BandSongMetadata 69.4%.
  BandSongMgr + MetaPerformer deferred (walls: EH catch-funclet parent-frame
  coupling, regalloc cascades, ~15 unmapped `fn_` VAs = a map-population lever).
- **`SongInfoCopy::GetTracks` native stub NOT unblocked** — `SongInfoCopy` is a
  separate class none of the M1 TUs reference; its TU must be located/wired first.

## Batch-12 seeds (ranked — pivot to map-recovery)

1. **Continuation-span pins via `comdat_scatter_scan.py`** on high-`rem` already-
   ported TUs (the SongParser mechanism, now proven twice): SaveLoadManager,
   RockCentral, NextSongPanel, UIStats, VocalPlayer, SessionMgr, MoveMgr, plus the
   CORE boot-path TUs (DirLoader/Anim/DataFunc). Pin new byte-matching spans →
   `size_order_automap`/reloc-correlate recovers pairings, 0 source risk.
2. **`target_symbol_map.json` population lever** — BandSongMgr's ~15 unmapped `fn_`
   VAs + BandSongMetadata HasPart(2-arg)/Rank identity: bytes already compile, only
   name-pairing missing (invcorr / `tu5_reloc_masked_correlate.py`). Several free flips.
3. **More unwired-owner scatter-includes** (Anim +27 shape) — scan pinned spans for
   scattered COMDATs owned by fully-unwired in-tree `.cpp` files.
4. **BandSongMetadata 4-arg ctor** (`fn_825a0b28`, 39.5%, ~8 KB, retail ~2× larger,
   likely 30+ FIND_WITH_BACKUP local-static field symbols) — a large but tractable
   body port.
5. **208 unwired in-scope oracle TUs** = gameport lever (needs span location;
   band3 subset highest-confidence).

DO-NOT-FUND (re-confirmed dead): per-fn near-miss harvest, layout/class-RE bank,
StorePanel multi-frag, Movie/BeatMatchController, vendor/Quazal/360-render,
anon-`fn_` automap-recovery on existing pinned units.
