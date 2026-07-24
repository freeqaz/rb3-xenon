# Fingerprint carve — BATCH 10 (batch-9 seed execution)

Author: batch-10 foreman. Baseline: main @ **24,781** (4df07844).
Final: main @ **24,834** (2060836c). Batch-10 lanes: **+53** (0 strict losses
across all landings; full strict-set gate + overlap self-check each time).
Concurrent net-neutral commit 712350e1 (native-port scope-map docs+tools)
composed cleanly mid-wave.

This was **seed execution** of the batch-9 ranked seeds (docs/plans/repin-batch9.md
§"Batch-10 seeds"), not a fresh census. 5 Opus workers on 3 lanes; foreman
landed serial.

## Landed (per-lane)

| lane | worker seed | Δstrict | commit |
|---|---|--:|---|
| 1 (mechanical) | true-owner fwd-extends: UploadErrorMgr / SongSortNode / CameraManager | **+23** | c023fedf |
| 1 (mechanical) | trimmed re-pin: StorePreviewMgr `0x827B1D60–0x827B1FBC` | **+1** | 4ba68064 |
| 3 (bodyport) | SongParser full-file port + ParseAndStripLyricText loop bodyport | **+29** | 2060836c |
| **total** | | **+53** | |

### Reverted / no-go (priced, no commit)

- **LANE 1 — Movie fwd-extend** (0x824781B0, dtkBIG 26): reverted, 0 pairings.
- **LANE 1 — BeatMatchController trim** (0x8278FBC0→0x827900C8, dtkBIG 28):
  dtk emits the target fns but automap proposed **0 pairings** (compiled source
  has no matching bodies for that gap) → net 0, reverted.
- **LANE 1 — StorePanel multi-frag** (0x82605100, dtkBIG 41, score 0.417):
  would gain +4 but the batch-8 "mirage" map entries create a `??_GStorePanel`
  name collision that **fuzzy-regressed two old-span pairs** (fn_827B75A0
  63.9→0, fn_827B729C 99.9→93.9). Real fuzzy regressions → reverted. The
  batch-8 StorePanel mirage is now **confirmed a permanent wall** (name-collision
  class, not a pin problem) — retire it.
- **LANE 1 — Mesh INTERNAL 72** (0x8273CC08, "D3D(phys):CubeTex"): identified as
  a **DX9 render TU** (DxCubeTex family) — **OUT of native-port scope** per the
  mid-wave scoping directive (renderer is DC3-supplied). Not scaffolded. Owner-ID
  recorded for reference only; do not fund.
- **LANE 1 — Mesh 82766B68** (dtkBIG 15): owner stayed ambiguous (4 strings,
  'event/handler/mode/sinks'); skipped rather than force-pin.
- **LANE 1 — BandCharDesc::Head::operator== under-carve**: the 4-byte tail is an
  **ICF tail-merge**, not a splits truncation — the trailing `blr` is folded with
  a sibling COMDAT; a splits end-bump collides. Not a free tooling fix; retire.
- **LANE 2 — force-multiplier / class-RE (W4)**: **NO-GO across the board.** Every
  seeded lever (ProfileMgr +4 drift, Character vtable+4/vbase−4, Stats layout,
  DancerSequence base) was either already-landed, oracle-contradicted, or net
  ≤0 after the fuzzy-regression count. The unk6d8 precedent (+6) did **not**
  generalize — the class-RE frontier in these units is drained.

## Lane economics (flips per worker-run; prices the endgame)

| lane class | landed | wall/revert | read |
|---|--:|--:|---|
| **mechanical repin (L1)** | +24 (2 seeds) | 5 seeds | Clean gap-fill fwd-extends still pay (+23 from 3 TUs), but the trim/mirage/foreign-owner tail is now **mostly walls**. The clean-extend vein is confirmed **drained** (batch-9 already called it). |
| **bodyport (L3)** | +29 (1 seed) | 5/6 fns walled | **Best lane this batch.** The win is the **full-file source port** (stub → complete .cpp pairs 8 real fns + funclet cascade = +28), not per-fn bodyport (only 1 of ~13 divergent bodies was tractable: ParseAndStripLyricText loop idiom). Divergent bodies here are genuine retail-vs-Wii logic/data-table skew (data-table walls, macro/build walls, coupled-to-matched-sibling), not portable regalloc. |
| **class-RE (L2)** | 0 | all levers | **Drained.** Force-multiplier layout levers do not generalize from the unk6d8 win. Full-rebuild A/B cost with ~0 yield. |

**Economic takeaway:** the highest-yield remaining lever is **full-file source
ports of stubbed-but-pinned TUs** (the SongParser shape: a TU already pinned with
a complete header but a 3-fn stub body — porting the whole .cpp from the oracle
pairs the whole COMDAT cluster at once). This is the batch-11 primary. Per-fn
near-miss harvest and class-RE are both confirmed exhausted-residue.

## Remaining-seed health

- Batch-9 mechanical seeds are now **executed**: 4 landed/kept, the rest are
  walls (StorePanel mirage, BeatMatchController no-pairing, Mesh DX out-of-scope,
  Mesh 82766B68 ambiguous, BandCharDesc ICF).
- Force-multiplier seed bank: **drained** (do not re-fund the batch-9 list).
- SongParser third span 0x827848CC..0x82788288 (dtkBIG 53, score 0.444) is
  **untouched** — mixed-owner, lower confidence; candidate for a careful pin +
  full-file-port pass.

## Batch-11 seeds (ranked)

1. **Stub-body full-file port vein** (the SongParser force-multiplier, highest
   yield): enumerate pinned-but-stubbed TUs — TUs with a `.text` pin + complete
   header but a skeleton .cpp — and full-file-port each from the oracle
   (rb3-Wii for band3/beatmatch/song game code, dc3 for engine). One port pairs
   a whole COMDAT cluster. Scanner target: `objects.json` NonMatching TUs whose
   compiled obj has ≪ dtk-target fn count. **In native-port scope** (band3 +
   system song/obj/utl/meta/ui subsystems).
2. **SongParser 0x827848CC span** (dtkBIG 53): pin + full-file-port continuation
   of the SongParser port already landed (header/source now complete in-tree).
3. **The <10-dtkBIG repin tail** (~101 census rows, est 0.3-0.5× capture) as
   low-priority filler — but screen for the same walls (foreign-owner, mirage,
   no-pairing) that ate most of batch-10's mechanical tail.
4. **SongParser deferred data-table walls** (CheckKeyboardRangeMarker, PitchToSlot):
   if the retail static lookup tables (lbl_82E06410/lbl_82E06408) can be
   reconstructed as `.rdata` and the inline compute swapped for a table read.
   Data-diff / rdata-pin work, not a body-port.

## Honest frontier statement

The repin/mechanical frontier is **substantially walled**: batch-10's mechanical
lane landed +24 but hit 5 walls, and the force-multiplier/class-RE frontier is
**drained** (0 yield, priced). The mass class the frontier now sits in is
**source completeness, not carve mechanics** — the biggest remaining lever is
**porting whole stubbed-but-pinned .cpp files** from the rb3-Wii/dc3 oracles
(SongParser paid +29 this way, vs +24 from all mechanical repins combined).
Batch-11 should pivot from "pin more spans" to "fill in the stub bodies of TUs
already pinned," concentrated in the native-port CORE+SOON scope. Per-fn
near-miss harvest and layout-RE remain confirmed exhausted-residue — do not
re-fund.
