# Unit-completion frontier — RE-CENSUS at `2e589b9b` (lane EB-3, 2026-08-03)
Supersedes `UNIT_COMPLETION_FRONTIER_2026-08-03.md` (lane DS-4, taken at 239 units)
and lane DT-1's post-lane figures. **Do not carry any per-unit claim forward from
those docs without re-measuring** — DS-4's `RealGuitarGemPlayer 32/33` survived its
own headline correction and was briefed as a target that did not exist (it reads 33/33).

## Provenance

Settled build in worktree `~/tmp/laneEB3/wt` off `2e589b9b`: first build did 390
edges, four subsequent builds did only the always-run `PROGRESS` phony (zero work).
`report.json` + `report.cache` wiped before the read (`0 hits, 1998 misses`).
Baseline reproduces the briefed figures exactly.

| measure | value |
|---|---:|
| `matched_functions` | 43,848 |
| `total_functions` | 69,304 |
| `matched_code` | 4,233,200 |
| `total_code` | 10,688,948 |
| `matched_code_percent` | 39.603523 |
| `masked_equal_functions` | 22,724 |
| honest (`matched − masked_equal`) | 21,124 |

## ★★★ THE SOURCE-ONLY CEILING MOVED: 253 → 293 (+40). It was not wrong.

| | DS-4 | DT-1 (post) | **EB-3 (now)** |
|---|---:|---:|---:|
| `AT_100` | 221 | 222 | **254** |
| `COMPLETABLE` | 32 | 38 | **39** |
| **ceiling** = AT_100+COMPLETABLE | **253** | **260** | **293** |
| `ANON_BLOCKED` | — | — | 170 |
| `MIXED` | — | — | 552 |
| `OD_REGION` | — | — | 10 |
| pairable units | 1,023 | 1,023 | 1,025 |

**Attribution.** AT_100 rose +32 (222→254). Had nothing entered the ceiling,
`COMPLETABLE` would have fallen 38→6; it reads **39**, so **~33 units newly
crossed INTO the ceiling** by driving their unpaired-anon count to zero. That is
the ceiling predicate operating exactly as DT-1 derived it, and it is corroborated
independently of the census: of the **69 commits** between `eb8f73d7` (DT-1) and
`2e589b9b`, **29 touched `target_symbol_map.json` / `splits.txt` / `symbols.txt`**,
and the map grew **27,946 → 28,041 rows (+95)**.

⇒ **The old 253 figure was CORRECT FOR ITS TREE.** DT-1 had already reproduced
DS-4's 253 exactly. The ceiling is not a constant — it is a function of
identification coverage, and ~100 commits of carve/map work moved it. Treat any
quoted ceiling as perishable in the same way `total_code` is.

⚠ Consequence for planning: we are at **254** of a **293** ceiling, i.e. **39 units of
source-only headroom remain**, NOT the ~6 you would infer by holding the old
ceiling fixed. A lane briefed off the stale 260 would have concluded the frontier
was nearly exhausted. It is not.

## Anon-row split (the ceiling predicate's input)

| | rows |
|---|---:|
| unpaired (mpn 0, GOVERNS the ceiling) | 16,771 |
| paired sub-100 (source-reachable) | 1,657 |
| masked (mpn 100) | 22,724 |

## One-away units (61)

`COMPLETABLE` 23 · `ANON_BLOCKED` 35 · `OD_REGION` 3

Sub-classification of the one-away `ANON_BLOCKED` (ceiling-raising candidates):

- `NO_CLASS_ANCHOR` — 12
- `NO_SAMECLASS` — 2
- `MAP_FIXABLE_UNADJUDICABLE` — 4
- `SAMECLASS_DIFFSIZE` — 5
- `AUTO_03_NO_OBJ` — 12

⚠ **17 of the 25 single-function units with matched=0 are `ANON_BLOCKED`** = DG-2's
structurally-impossible class: a boundary move drains the unit's only function, so it
**VANISHES** rather than reaching 100%.

## The COMPLETABLE work list (all 39, ranked by blockers then fuzzy)

This is the actionable frontier: every blocker is source-reachable.

| blk | tier | unit | top blocker | mpn | fuzzy | size |
|---:|---|---|---|---:|---:|---:|
| 1 | ENG | `system/gesture/SkeletonDir` | `?TestClip@SkeletonDir@@QBAPAVSkeletonClip@@XZ` | 99.5 | 99.50 | 8B |
| 1 | ? | `Main` | `main` | 96.8 | 96.84 | 76B |
| 1 | ENG | `PitchCorrectedVoice` | `?GetCorrection@PitchCorrectedVoice@Synapse@DSP@@QAAMXZ` | 97.7 | 96.67 | 528B |
| 1 | ENG | `EQEffect` | `?Process@EQEffect@@QAAXPAMHH@Z` | 98.2 | 95.69 | 476B |
| 1 | ENG | `FilterCoeffs` | `?LowpassCoefficients@DSP@@YAXQAMMMM@Z` | 95.3 | 95.35 | 344B |
| 1 | ENG | `FileChecksum` | `?_M_insert_overflow_aux@?$vector@UChecksumData@@V?$StlNodeAlloc@` | 93.8 | 93.79 | 412B |
| 1 | ENG | `DelayEffect` | `?Process@DelayEffect@@QAAXPAMHH@Z` | 90.3 | 86.51 | 384B |
| 1 | ENG | `HamPhotoDisplay` | `?Save@HamPhotoDisplay@@UAAXAAVBinStream@@@Z` | 84.3 | 83.19 | 128B |
| 1 | ENG | `system/meta/MemcardMgr` | `?SaveLoadAllComplete@MemcardMgr@@QAAXXZ` | 83.3 | 82.60 | 120B |
| 1 | ENG | `PitchDetector` | `?Detect@PitchDetector@Synapse@DSP@@QAAXI@Z` | 81.5 | 79.60 | 676B |
| 1 | ENG | `system/synth/MoggClip` | `?SetupPanInfo@MoggClip@@QAAXMM_N@Z` | 78.7 | 78.71 | 124B |
| 1 | ENG | `FlangerEffect` | `?Process@FlangerEffect@@QAAXPAMHH@Z` | 81.1 | 77.93 | 768B |
| 1 | ENG | `FIRFilter` | `?setCoefficients@FIRFilter@soundtouch@@UAAXPBMII@Z` | 77.3 | 77.21 | 152B |
| 1 | ENG | `SoftParticleBuffer` | `?BlurSurface@RndSoftParticleBuffer@@AAAXXZ` | 68.8 | 66.23 | 660B |
| 1 | ENG | `Rnd_NG` | `??$MakeString@HH@@YAPBDPBDHH@Z` | 59.9 | 58.61 | 92B |
| 1 | ENG | `SHA1` | `?Transform@CSHA1@@AAAXPAIPBE@Z` | 62.0 | 55.70 | 5856B |
| 1 | ENG | `HamDriver` | `?Clear@HamDriver@@QAAXXZ` | 46.7 | 46.67 | 12B |
| 1 | ENG | `StorePurchaser` | `??$_M_allocate_and_copy@PB_K@?$vector@_KV?$StlNodeAlloc@_K@stlpm` | 33.3 | 31.33 | 60B |
| 1 | ENG | `FilterQueue` | `?CancelJob@FilterQueue@@QAAXXZ` | 29.2 | 29.15 | 52B |
| 1 | ENG | `PreloadPanel` | `?Load@PreloadPanel@@UAAXXZ` | 16.9 | 14.89 | 384B |
| 1 | GAME | `OvershellPartSelectProvider` | `??$__uninitialized_fill_n@PAURndPointTest@NgRnd@@IU12@@stlpmtx_s` | 0.0 | 0.00 | 56B |
| 1 | ENG | `FlowQueueable` | `?Deactivate@FlowQueueable@@UAAX_N@Z` | 1.0 | 0.00 | 168B |
| 1 | ? | `auto_03_82402F68_text` | `?Replace@RndDir@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | 0.0 | 0.00 | 56B |
| 2 | ENG | `FftIpp` | `??1FftIpp@@QAA@XZ` | 98.0 | 97.96 | 196B |
| 2 | ENG | `system/gesture/DrawUtl` | `fn_82605944` | 93.9 | 93.90 | 40B |
| 2 | ENG | `RealGuitarTrackWatcherImpl` | `?InTrill@RealGuitarTrackWatcherImpl@@UBA_NH@Z` | 85.7 | 85.71 | 112B |
| 2 | ENG | `IPP_basicmath_xbox` | `?Add_InPlace@IPP@@YAXIPBMPAM@Z` | 83.3 | 82.08 | 48B |
| 3 | ENG | `HamRibbon` | `?_M_fill_insert@?$vector@V?$Key@VTransform@@@@V?$StlNodeAlloc@V?` | 100.0 | 99.96 | 112B |
| 3 | ENG | `ScrollbarDisplay` | `?NewObject@ScrollbarDisplay@@SAPAVObject@Hmx@@XZ` | 100.0 | 99.96 | 112B |
| 3 | ENG | `PropertyEventProvider` | `??_DPropertyEventProvider@@QAAXXZ` | 7.8 | 0.00 | 36B |
| 4 | ENG | `Mat_NG` | `?NewObject@NgMat@@SAPAVObject@Hmx@@XZ` | 100.0 | 99.96 | 112B |
| 4 | ENG | `MicInputArrow` | `?NewObject@MicInputArrow@@SAPAVObject@Hmx@@XZ` | 100.0 | 99.96 | 112B |
| 4 | GAME | `UIStats` | `fn_8256041C` | 99.9 | 99.90 | 40B |
| 4 | ENG | `MidiReader` | `?ReadMetaEvent@MidiReader@@AAAXHEAAVBinStream@@@Z` | 99.4 | 99.44 | 972B |
| 5 | ENG | `HolmesClient` | `?push_back@?$vector@URecurseInfo@@V?$StlNodeAlloc@URecurseInfo@@` | 100.0 | 99.97 | 116B |
| 5 | ENG | `StorePreviewMgr` | `fn_827B26A8` | 99.9 | 99.90 | 40B |
| 5 | ENG | `BoxMap` | `?ApplyLight@BoxMapLighting@@ABAXABV?$BoxLightArray@ULightParams_` | 85.2 | 81.52 | 168B |
| 9 | ENG | `FFT` | `?fft_matrix_inverse_columnwise@@YAHPAMJ0@Z` | 85.9 | 84.13 | 1160B |
| 10 | GAME | `AccomplishmentPlayerConditional` | `fn_8235FE64` | 99.9 | 99.91 | 44B |

**Tier split of COMPLETABLE:** ? 2, ENG 34, GAME 3 — consistent with the settled finding that **the ceiling lives in the ENGINE tier**.

⛔ `system/synth/MoggClip` (`SetupPanInfo`, 78.71%) appears on this list but is a
KNOWN codegen wall — commit `51c3c615` refuted a fourth source form. It is
COMPLETABLE by the census predicate (its blocker is source-reachable) but is not
cheap. Do not re-fund it on the strength of its position here.
