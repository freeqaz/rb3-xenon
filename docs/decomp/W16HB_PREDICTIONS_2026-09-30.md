# W16-HB predictions, written before measuring (2026-09-30)

## Batch 1 (commit `3111589f4`) — Part A, 3 TrackWidgetImp map rows

Map-only naming batch. Unit: `default/TrackWidget` (splits heading for
`src/system/track/TrackWidgetImp.cpp`). All three rows previously read as
unnamed `fn_<addr>` at 0% (unpairable — target had no name to pair by).

| address | name | target size (B) | prediction |
|---|---|---:|---|
| `0x827e2a50` | `?AddInstance@ImmediateWidgetImp@@UAAHVTransform@@M@Z` | 216 | jumps to 100% fuzzy — retail disasm at this address matches our compiled `ImmediateWidgetImp::AddInstance` byte-for-byte in the sampled instruction stream |
| `0x827e2b30` | `??1?$TrackWidgetImp@UInstance@RndMultiMesh@@@@UAA@XZ` | 76 | jumps to 100% fuzzy — matches the `TrackWidgetImp<RndMultiMesh::Instance>` dtor thunk shape |
| `0x827e2c98` | `??1?$TrackWidgetImp@VMeshInstance@@@@UAA@XZ` | 76 | jumps to 100% fuzzy — matches the `TrackWidgetImp<MeshInstance>` dtor thunk shape |

**Predicted**: +3 functions (mpn), +368 B `matched_code` (all three at fuzzy
100), `masked_equal` unchanged (none of these are ICF-fold survivors), 0
recompiles (map-only patch, no source touched) but a forced re-split (map
patch must re-split on both legs per `ab_measure` rules).

No source or symbol_aliases.json changes in this batch. Part B and Part C
predictions will be appended here in their own sections before each is
measured.

## Batch 1 outcome, before measuring (recorded 2026-09-30, pre-A/B)

The Batch 1 prediction above FAILED on the in-worktree report: the two
`??1?$TrackWidgetImp<T>` rows read **fuzzy 20.79**, not 100, and
`AddInstance` reads **fuzzy 93.13 / mpn 93.22**. The dtor names are wrong
(retail bodies are derived-class dtors -- see the deliverable doc) and are
reverted in `819b78c68`.

## Branch-vs-main prediction (net diff = one map row, 0x827e2a50 AddInstance)

MidiParser naming adds **zero** rows (all candidates refused on relocation
evidence -- see deliverable doc). So the whole branch diff is one map line.
Predicted: **Δmatched 0, Δmatched_code 0 B, Δmasked_equal 0,
Δfuzzy ≈ +216 x 0.9313 ≈ +201 B ≈ +0.0020 pp**, map-only (re-split, 0
recompiles).
