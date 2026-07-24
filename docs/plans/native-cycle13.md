# Cycle-13 map-recovery foreman — results (2026-07-24)

Foreman: cycle-13 MAP-RECOVERY. Baseline main **24,899** → final main **24,928**
(`measures.matched_functions`, composed full A/B, report.cache cleared each leg).
**Net +29 strict, 0 strict regressions.** 4 Opus workers (map-recovery lanes),
foreman landed serial via `scripts/harvest/land.sh` union-resolve + full-rebuild
verify.

## Landed (per worker)

| worker | mechanism | Δstrict | detail | commit |
|---|---|--:|---|---|
| D | continuation-span pins | **+15** | VocalPlayer: 5 *inter-fragment* unpinned gaps (`0x826E381C..0x826E4BD0`) held byte-perfect VocalPlayer COMDATs; `size_order_automap` recovered 16 EXACT/STRONG pairings, 15 flipped strict. 0 source edits. | 67526740 |
| C | unwired-owner scatter-include | **+10** | `SongSortByReview` `#include "SongSortByPlays.cpp"` (+8, virtual node factories) + `VocalTrackDir` `#include "AccomplishmentProgress.cpp"` (+2), both guarded. | fc8221f9 |
| B | struct-relayout → map-recovery | **+4** | BandSongMgr member reorder (uniform +16 offset shift: `mContentAltDirs`/`mMaxSongCount` precede `mUpgradeMgr`/`mLicenseMgr`) unlocked 4 byte-identical pairings (ClearCachedContent/Terminate/GetUpgradeData/HasLicense). | e7abeae3 |
| A | continuation-span pins | 0 | SaveLoadManager/RockCentral/NextSongPanel — negative (no continuation span). | — |

No accepted fuzzy slips this cycle (all four lands were purely additive).

## Mechanism economics (measured this cycle)

- **Continuation-span pins**: 1/4 units paid (VocalPlayer +15). The mechanism
  fires ONLY when a TU has genuinely unpinned .text holes — either past its last
  pin OR *between* its existing fragments (VocalPlayer's win was inter-fragment
  swiss-cheese gaps, a NEW shape vs SongParser's tail span). `repin_census.py` is
  the authoritative discovery tool: if a unit never appears as a run *owner*
  (only as a `next_tu` neighbor), it has no continuation span. SaveLoadManager /
  RockCentral / NextSongPanel / UIStats / SessionMgr / MoveMgr all confirmed
  butt-against-neighbor with 0 recoverable gap → their remaining headroom is
  body-divergence, not map recovery.
- **Unwired-owner scatter-include**: 2 wins / ~15 screened. Productive shape =
  **real named-function scatter with a distinct, self-contained owner TU**. DUD
  classes (pre-screen these OUT): STL-template-only scatter (folded template
  instances don't pair into the includer — CharLipSync/Mic/Song/VocalPart/
  FileMerger/Mesh all +0); shared-header double-defs (MemHeap←MemMgr gHeaps);
  cross-TU anon-namespace collisions (CameraShot←BandCamShot DebugGraph);
  local 2-arg `INIT_REVS`/`gRev`-as-class-static owners (all hamobj: CharHair/
  MeshAnim/HamCamTransform/PracticeSection); global operator-template redefs
  (LicenseMgr `operator<<`); self-owner proposals (owner_obj==unit = map mispair,
  not scatter). **Always verify with a CLEAN FULL rebuild** — an incremental gave
  a false +12 (Game←StoreSongSortNode) that was +0 on full rebuild.
- **Map population (pure reloc-correlate)**: near-DEAD on mature units — every
  byte-identical real body is already mapped; the correlator's "UNIQUE" hits are
  EH funclets that objdiff auto-pairs (applying them REGRESSES). The productive
  variant is **struct-relayout-THEN-map** (Worker B): a member-offset fix makes a
  fn byte-identical, then the map entry flips it.

## SongInfoCopy::GetTracks — NATIVE MILESTONE verdict

**Located.** `SongInfoCopy::GetTracks` (mangled `_ZNK12SongInfoCopy9GetTracksEv`,
the first `native/src/dta_link_stubs.s` weak stub) is defined at
**`src/system/char/CharBoneDir.cpp:17`** — a one-line accessor
`const std::vector<TrackChannels>& SongInfoCopy::GetTracks() const { return mTrackChannels; }`
placed cross-unit (documented at `SongInfoCopy.cpp:15`). That TU is **already
wired + pinned** (`config/45410914/{objects.json,splits.txt}`, `default/CharBoneDir`
at 54.5%). So the decomp-side location is done; the native-stub unblock is a
**native-build inclusion task** (compile CharBoneDir.cpp into `native/`), not a
decomp map-recovery item. `SongInfoCopy.cpp` itself is wired/pinned at 82.1%.

## Remaining map-recovery mass (fresh scan, this baseline)

`comdat_scatter_scan.py` on the 24,928 report:
- **SCATTER pool** (bytes emitted by some obj → include/pin recoverable):
  ~314 proposals / ~381 funcs / ~50 KB. But after this cycle's screen, the
  *productive* fraction is small — most are STL-template / shared-header /
  gRev-class-static duds. Realistic residual yield **~+10..+30** across the whole
  pool, harvested a few funcs at a time.
- **UNWIRED pool** (no obj emits): 177 units — gameport work (needs source
  location), NOT map recovery.

## Batch-14 seeds (ranked)

1. **`repin_census.py`-driven continuation pins** — the discovery tool, not blind
   TU guessing. Run it, take units that appear as run *owners* with unpinned gaps
   (VocalPlayer-style inter-fragment holes count). Skip anything that only shows
   as `next_tu`.
2. **Scatter-includes, distinct-owner filter only** — pre-screen the scan for
   owners that are (a) not STL-template symbols, (b) no shared header with the
   includer, (c) no local `INIT_REVS`/`gRev` class-static. Candidates left after
   this cycle's rejections: check SongUpgradeMgr←LicenseMgr (rejected for operator
   template — revisit with targeted body-dup instead of whole-file include),
   AccomplishmentManager←PracticeSection (blocked by include guard — add one).
3. **Struct-relayout → map-recovery** (Worker B shape) — the `build-forcemult-finder`
   member-delta finder ranks uniform this-relative offset shifts; each fix can make
   several fns byte-identical and unlock pairings. Higher yield than pure correlate.
4. **BandSongMetadata 4-arg ctor** `fn_825a0b28` (~39.5%, ~8KB) — deferred filler;
   real RE-level body port (retail HasPart is a different algorithm than the
   rb3-Wii oracle — local-static Symbols), not a pairing.

DO-NOT-FUND (re-confirmed dead this cycle): pure reloc-correlate map population on
mature units (funclet-only hits regress), continuation pins on units with no
census-owned gap (SaveLoadManager/RockCentral/NextSongPanel/UIStats/SessionMgr/
MoveMgr), STL-template scatter-includes, self-owner scatter proposals, per-fn
near-miss, anon-`fn_` automap on existing pins.

decomp.db re-ingested at 24,928 (5,299 complete / 65.4% avg).
