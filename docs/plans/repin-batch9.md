# Fingerprint carve — BATCH 9 (repin tail + named near-miss harvest)

Author: batch-9 foreman. Baseline: main @ **23,109** (993bbc28).
Final: main @ **24,781** (3edceadb). Batch-9 lanes: **+1,587**
(+1,580 repin, +7 near-miss); concurrent correlator r11 lane: +85
(4cb3ed83, pure map inserts, landed mid-wave at 23,563 — composed cleanly).
**0 strict losses across all 7 landings** (full strict-set gate each time).

Inputs: `~/tmp/repin_batch8.json` (297-row census) + batch-8 addendum seeds
(`docs/plans/repin-batch8.md` @ bcf31177). Open pool at start: 130 rows
dtkBIG≥10 not yet pinned (sum 2,369), minus 2 MasterAudio rows (another
lane's WIP) and 3 known batch-8 mirages (Mesh INTERNAL 72 / StorePanel 41 /
Movie 26) pre-routed to batch-10.

## WORKLIST A — repin tail: +1,580 strict, 85 TUs, 5 buckets

| bucket | landed | TUs kept | Δstrict | funded dtkBIG | kept dtkBIG | capture |
|---|---|--:|--:|--:|--:|--:|
| 1 (7fcbe52c→5cc9f6f5) | 14 TUs | 14 | +200 | 430 | 269 | 0.74× |
| 2 (bc4a4303) | 18 TUs | 18 | +369 | 426 | 414 | 0.89× |
| 3 (2b0147a1) | 17 TUs | 17 | +354 | 435 | 389 | 0.91× |
| 4 (5bd8e94c) | 19 TUs | 19 | +323 | 431 | 419 | 0.77× |
| 5 (1ad7f9d1) | 17 TUs | 17 | +334 | 430 | 402 | 0.83× |
| **total** | | **85** | **+1,580** | **2,152** | **1,893** | **0.83× kept / 0.73× funded** |

**Capture-rate calibration: 0.83× dtkBIG on kept spans** (batch-8: 0.79×) —
the rate is holding, not declining, once the three mirage screens gate the
funding. Game tier still over-performs (dormant map entries), engine tier
under-performs.

Top single-TU wins: MetaPerformer +78 (4 spans), PostProc +60, OvershellSlot
+69, DataFunc +53, RockCentral +53 (3 spans, `__unwind$`-clean), BandUser +50,
TransAnim +45, UILabel +39, CharClip +38, CharDriver +37, BandSongMetadata +37,
CharIKScale +35, PropAnim +34.

Screen hits (all pre-build, no wasted cycles):
- **Screen 3 (foreign-TU), 8 rows:** SetlistToStorePanel 82642450 → true owner
  **UploadErrorMgr**; StoreOfferProvider 82663050 → **SongSortNode**;
  WaitingUserGate 825ACEC0 → **CameraManager**; SongParser 827848CC →
  SongCollision (inlining-gated); Loader 827C6708 → Song.cpp overlap;
  BeatMatchController 8278FBC0 + StorePreviewMgr 827B1D60 → run_end overruns
  owner_span into next TU (trim-and-repin candidates); Mesh 82766B68 → foreign,
  owner unknown.
- **Screen 2 (base-less), 2 rows:** top-level `Synth.cpp` split block
  (duplicates system/synth/Synth.cpp range, no compiled base); FxSendDelay
  82720C78 (empty frag, no pairing).
- **Screen 1 (multi-frag re-carve): 0 hits** — StorePanel 82604840 pinned clean
  (+3, other fragments held); the batch-8 StorePanel mirage did not recur on
  this smaller span.
- Net-0 reverts: SongParser 82782B28/82783E90 (genuine spans, divergent bodies —
  5/54 fns compiled; route to bodyport), FxSendDelay.

Landing mechanics note: per-TU worker commits (14-19 per bucket) cascade-DEFER
in `land.sh` once main has moved (>5 union rounds); squash-per-bucket before
landing (worker content preserved, one union round). dtk .pdata auto-backfill
appears as post-verify dirt in main — commit it path-limited (f3559a4c).

## WORKLIST B — named near-miss harvest: +7 strict. **50% flip-rate policy FALSIFIED for this pool.**

Pool: 106 named fns (fuzzy 70-99.9, norm<100) in the 32 batch-8-repinned units
+ LayerDir; 84 tractable after filtering STL helpers/giant dispatchers; ~60
reconned in depth by 3 workers (render/char/game clusters).

**Measured flip rate ≈ 3% (2 levers / ~60 reconned)** vs the standing 50%
policy — the post-repin 70-99.9 band in these units is **exhausted-residue**:
prior waves took the body-ports; what's left is regalloc/FP-scheduling
cascades, STLport container codegen, strcpy-intrinsic walls, and class-layout
RE. **Do not fund per-fn near-miss harvest in freshly-repinned units by
default**; route to the force-multiplier seeds below instead.

Flips landed:
- **BandCharacter unk6d8 relayout** (db4ff52d): 2-line header move (+ ctor
  init-order) → **+6** (SaveBoneAndChildren, TextureCompressed, +4 anon
  accessors). Verified full-rebuild, 0 regressions.
- **RndMesh::Save** (3edceadb): +1.

Named walls confirmed (do-not-grind additions): strcpy-intrinsic
`cmplwi`↔`extsb.` ×3 (BandCharacter On* handlers), STLport
`_Rb_tree::_M_increment` inline-policy (InterstitialMgr ×2 byte-identical to
oracle), `BEGIN_HANDLERS` DataNode return-temp stack packing (Handle fns),
ObjPtr two-ctor (RndPartLauncher, known), c2 tie-break (BandCharacter::
SyncObjects — same class as PitchArrow).

## Batch-10 seeds (ranked)

1. **True-owner re-pins** (repin-shaped, near-free): UploadErrorMgr (30),
   SongSortNode (24), CameraManager (16), DxRnd/DxMovie/DxCubeTex (the Mesh
   INTERNAL 72 span), Mesh 82766B68 owner-ID.
2. **Trimmed re-pins**: BeatMatchController 8278FBC0 clip to 827900C8 (28);
   StorePreviewMgr 827B1D60 clip to 827B1FBC (18).
3. **TU-wiring lane**: Movie bare-block (26), top-level Synth.cpp dup block
   (12), FxSendDelay (13).
4. **StorePanel 82605100 multi-fragment re-pin** (41, batch-8 mirage).
5. **MasterAudio 8277B828 (44) + 8277EE84 (34)** — once the other lane's
   src/system/os/MasterAudio.cpp WIP lands.
6. **Force-multiplier/class-RE levers** (from near-miss recon; each ripples
   multiple fns, needs full-rebuild A/B): Character vtable slot +4
   (Teleport 0x2c vs 0x30) + Character vbase −4; ObjRef/ObjOwnerPtr ring model
   (`OwnerRef()`=this, ring off 0x4 — 8+ OutfitConfig/char fns); ProfileMgr +4
   member drift (mDataResults 0x6c + DataResultList 0x14→0x18, compensating);
   DancerSequence base RndAnimatable→UIPanel; Profile virtual-base
   (StorePanel::Load vbptr slot 0x64); Stats layout (mVocalPartPercentages
   @0x8); STLport `_M_increment` out-of-line flip (fleet-wide, high ripple
   risk — dedicated campaign only).
7. **Tooling +1**: BandCharDesc::Head::operator== target under-carved by 4 B
   (trailing blr) — splits/jeff truncation fix, no source change.
8. **Bodyport**: SongParser 82782B28/82783E90 spans (score 1.0, divergent
   bodies).

## Remaining-mass health

The dtkBIG≥10 clean-extend vein is **DRAINED** — of the 130-row open pool,
~119 rows were funded (85 TUs kept), the rest are the screens/seeds above.
What remains census-wide: the <10 dtkBIG tail (~101 rows, low sum, est
~0.3-0.5× capture), the seeds above (~330 dtkBIG total incl. MasterAudio), and
the Quazal 82A-82B scatter (not repin-shaped; scatter-wiring//Od lane).
Batch-10 should be seed-execution + the <10 tail as filler, not a fresh
census.
