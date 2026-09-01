# Unicorn matched-but-wrong worklist (SCREENED)

Rows whose emulated behaviour differs from retail while the match metric scores them **100%**.

- report.json objdiff-cli **4.2.8** commit `358c715835cc` binary `9b2bb6f1f3a21062` (NOT the binary deployed after the 2026-09-01 08:59 rebuild -- a row's 100% status is a tool-dependent claim)
- ruler: `functionRelocDiffs=name_check`
- `cap_exhausted*` excluded as INDETERMINATE (emulator instruction cap, not a divergence)

## Tiers

| tier | meaning | rows |
|---|---|---:|
| TIER1_STRONG | the mock-region artifact cannot explain it (call_count / decomp_error / scalar return) | 5 |
| TIER2_CANDIDATE | arg/memory differs WITHIN a mock region | 3 |
| TIER3_SUSPECT | cross-region pointer -- artifact-suspect, do NOT fund as bugs | 1009 |

> ⚠ TIER1 is *strong*, not *proven*. `call_count` is also produced by an INLINE-POLICY difference (retail inlined a helper, we call it out of line) -- identical behaviour, different call count. Adjudicate on retail bytes before fixing.


## TIER1_STRONG (5)

| # | symbol | unit | size | fuzzy | mpn | class | why |
|---|--------|------|-----:|------:|----:|-------|-----|
| 1 | `??0SoundTouch@soundtouch@@QAA@XZ` | default/SoundTouch | 132 | 100.00 | 100.00 | error | one side errors, the other does not |
| 2 | `?Handle@CharWeightable@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | default/CharWeightable | 12 | 95.00 | 100.00 | error | our side errors:  |
| 3 | `?Handle@MultiSelectListPanel@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | default/band3/meta_band/MultiSelectListPanel | 12 | 95.00 | 100.00 | error | our side errors:  |
| 4 | `??1AppMiniLeaderboardDisplay@@UAA@XZ` | default/band3/meta_band/AppMiniLeaderboardDisplay | 284 | 99.93 | 100.00 | call_count | differing NUMBER of calls |
| 5 | `?finalize@MD5@Quazal@@QAAXXZ` | default/network/Plugins/Checksum/MD5/MD5 | 228 | 100.00 | 100.00 | call_count | differing NUMBER of calls |

## TIER2_CANDIDATE (3)

| # | symbol | unit | size | fuzzy | mpn | class | why |
|---|--------|------|-----:|------:|----:|-------|-----|
| 1 | `?InterpTangent@@YAXABVVector3@@000MAAV1@@Z` | default/Color | 280 | 99.57 | 100.00 | return_value | return value differs |
| 2 | `??0InvExpInterpolator@@QAA@MMMMM@Z` | default/Interp | 92 | 100.00 | 100.00 | object_memory | 2 object word(s) differ within region |
| 3 | `?getKeyImpl@@YAXPAEPAD0@Z` | default/keygen_xbox | 68 | 100.00 | 100.00 | call_arg | arg r6 differs within scalar: 0x8 vs 0x0 |

## TIER3_SUSPECT (1009)

| # | symbol | unit | size | fuzzy | mpn | class | why |
|---|--------|------|-----:|------:|----:|-------|-----|
| 1 | `?Handle@SaveLoadManager@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/band3/meta_band/SaveLoadManager | 2576 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 2 | `?Handle@TrainingMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/band3/meta_band/TrainingMgr | 2044 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 3 | `?LocalizeOrdinal@@YAPBDHW4LocaleGender@@W4LocaleNumber@@_N@Z` | default/LocaleOrdinal | 1744 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 4 | `?Handle@UIScreen@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/UIScreen | 1452 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 5 | `?Handle@PracticeSectionProvider@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/band3/game/PracticeSectionProvider | 912 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 6 | `?Handle@TokenRedemptionPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/band3/meta_band/TokenRedemptionPanel | 832 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 7 | `?Format@DateTime@@QBAXAAVString@@@Z` | default/DateTime | 784 | 99.87 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 8 | `?Handle@CampaignSongInfoPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/CampaignSongInfoPanel | 772 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 9 | `??0PlayerParams@@QAA@XZ` | default/band3/game/Player | 720 | 100.00 | 100.00 | call_arg | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- arg r4 crosses mock regions (rdata->globals) |
| 10 | `?Copy@RndPostProc@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | default/PostProc | 664 | 100.00 | 100.00 | call_arg | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- arg r5 crosses mock regions (rdata->globals) |
| 11 | `?Handle@ModifierMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/band3/meta_band/ModifierMgr | 660 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 12 | `?Handle@SynthSample@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/system/synth/SynthSample | 652 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 13 | `?Handle@LightPresetManager@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/LightPresetManager | 648 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 14 | `?InitShaderOptions@@YAXXZ` | default/ShaderOptions | 644 | 100.00 | 100.00 | call_arg | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- arg r4 crosses mock regions (rdata->globals) |
| 15 | `?NewSongNode@SongSortByArtist@@UBAPAVStoreSongSortNode@@PAVStoreOffer@@@Z` | default/SongSortByArtist | 640 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 16 | `?Reskin@BandHeadShaper@@QAAXXZ` | default/system/bandobj/BandHeadShaper | 612 | 100.00 | 100.00 | call_arg | arg r5 crosses mock regions (rdata->globals) |
| 17 | `?Update@ScrollbarDisplay@@UAAXXZ` | default/ScrollbarDisplay | 560 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 18 | `?SetParameters@CompressionEffect@@QAAXABUParams@1@@Z` | default/CompressionEffect | 488 | 100.00 | 100.00 | object_memory | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- 4 object word(s) differ within region |
| 19 | `?ReEvaluateState@SigninScreen@@QAAXXZ` | default/band3/meta_band/SigninScreen | 484 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 20 | `?Poll@UIListState@@QAAXM@Z` | default/UIListState | 468 | 100.00 | 100.00 | object_memory | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- 1 object word(s), all cross-region |
| 21 | `?Handle@RndTexBlendController@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/TexBlendController | 460 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 22 | `?RecentTypeToOrigin@RecentCmp@@SA?AVSymbol@@W4RecentType@1@@Z` | default/SongSortByRecent | 420 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 23 | `?SyncLimits@CharLookAt@@IAAXXZ` | default/CharLookAt | 408 | 100.00 | 100.00 | object_memory | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- 1 object word(s), all cross-region |
| 24 | `?GetPrefabPortraitPath@@YAPBDPAVPrefabChar@@@Z` | default/CharData | 400 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 25 | `?Handle@RndAnimFilter@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/AnimFilter | 392 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 26 | `?Handle@SessionUsersProvider@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/SessionUsersProviders | 380 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 27 | `?Configure@CampaignLevel@@UAAXPAVDataArray@@@Z` | default/CampaignLevel | 380 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 28 | `?OnMsg@UGCPurchasePanel@@QAA?AVDataNode@@ABVSigninChangedMsg@@@Z` | default/band3/meta_band/UGCPurchasePanel | 372 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 29 | `?Init@TrainerProgressMeter@@QAAXPAVRndDir@@H@Z` | default/band3/game/TrainerProgressMeter | 368 | 100.00 | 100.00 | call_arg | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- arg r4 crosses mock regions (rdata->globals) |
| 30 | `?SendEndStreak@Tracker@@QAAXPAVPlayer@@MH@Z` | default/band3/game/Tracker | 368 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 31 | `??0DxParticleSys@@IAA@XZ` | default/system/rnddx9/CubeTex | 364 | 100.00 | 100.00 | object_memory | 5 object word(s), all cross-region |
| 32 | `?Handle@RndSet@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | default/Set | 360 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 33 | `?Init@TourGameRules@@UAAXPBVDataArray@@@Z` | default/band3/tour/TourGameRules | 344 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 34 | `?OnSetFrustum@RndCam@@IAA?AVDataNode@@PBVDataArray@@@Z` | default/Cam | 344 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 35 | `?GrantAward@Award@@QAAXABVAwardEntry@@PAVBandProfile@@@Z` | default/Award | 328 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 36 | `??0TambourineDetector@@QAA@AAVTambourineManager@@PAVSinger@@@Z` | default/TambourineDetector | 312 | 100.00 | 100.00 | call_arg | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- arg r4 crosses mock regions (rdata->globals) |
| 37 | `?Text@TrainerProvider@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | default/band3/meta_band/TrainerProvider | 312 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |
| 38 | `??0UIButton@@IAA@XZ` | default/UIListState | 308 | 100.00 | 100.00 | object_memory | 3 object word(s), all cross-region |
| 39 | `?CharUtlIsAnimatable@@YA_NPAVRndTransformable@@@Z` | default/CharUtl | 304 | 100.00 | 100.00 | call_arg | BYTE-IDENTICAL body; divergence is reloc-target mocking only -- arg r5 crosses mock regions (rdata->globals) |
| 40 | `?GetBookmarks@Song@@AAA?AVDataNode@@XZ` | default/Song | 292 | 100.00 | 100.00 | call_arg | arg r4 crosses mock regions (rdata->globals) |

_(969 further rows omitted; full set in the CSV.)_
