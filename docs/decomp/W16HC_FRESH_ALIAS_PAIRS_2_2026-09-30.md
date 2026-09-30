# W16-HC: FRESH alias pairs, batch 2 (2026-09-30)

Lane W16-HC, worktree `/home/free/tmp/wt-w16-hc`, branch `w16-hc`, off main
`8809d58ba`. Input: `/home/free/tmp/w16hc_candidates.json` -- the next 80
"FRESH" alias pairs after W16-HA's 80 (ranked by honest bytes; 28,368 B /
152 rows total across the 80). Method and gate follow
`docs/decomp/W16HA_FRESH_ALIAS_PAIRS_2026-09-30.md` exactly: adjudicate each
candidate with `tools/icf_pair_adjudicate.py --chase`, admit only CHASED T1
== PROVEN with zero `CYCLE-ASSUMED` / `SLOT-REFUTED` / `BYTES-DIFFER` trace
labels and either >=1 relocation compared by name or `VACUOUS-BUT-IDENTICAL`.

**Reviewer tool-quirk note, addressed up front:** every one of the 80
adjudications passed `--survivor` **by mangled name**, never by address (the
`w16hc_chase_raw.log` header line for each pair is
`survivor : <mangled name>`, confirmed by grep). The
`MISSING(retail) -> REFUTED` failure mode the reviewer warned about (renamer
has already map-named that address, so the tool can't find it by address)
was never hit -- `grep -c MISSING(retail)` on the raw log is **0**. The 7
`MISSING(ours)` lines that do appear are an unrelated artifact inside
per-pair relocation-tally dumps (a callee referenced from a body that has no
corresponding symbol on our side) and did not affect any verdict; all 7 sit
inside pairs that were independently REFUTED at FLAT T1 for other reasons
(group A below). So: **survivor was used BY NAME in all 80 cases; the
by-name retry path was never needed.**

## Result summary

- **51 ADMIT** (installed), **29 DECLINE**.
- Admits installed into `scripts/symbol_aliases.json` across 6 commits of
  <=10 pairs each (1697 groups at branch start -> **1730 groups** at HEAD;
  33 new groups, 18 admits extended a pre-existing group).
- Predictions written and committed *before* measuring
  (`docs/decomp/W16HC_PREDICTIONS_2026-09-30.md`, commit `233277f11`).
- Forward + reverse whole-binary A/B measured, exact mirror image.
- `tools/icf_alias_finder.py --validate`: **PASS, 0 CONTRADICTED**, run
  before and after both A/B legs, byte-identical breakdown both times.
- `bash tools/native_build_gate.sh`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

## Install batches

| batch | commit | pairs | idx | predicted rows | predicted bytes |
|---|---|---:|---|---:|---:|
| 1 | `6664646a1` | 10 | 1,2,3,4,6,7,8,9,10,11 | 58 | 6,368 |
| 2 | `15cd75acb` | 10 | 13,14,15,16,19,20,21,22,23,25 | 16 | 4,176 |
| 3 | `5809a6a12` | 10 | 26,28,29,32,33,34,35,41,44,45 | 12 | 3,692 |
| 4 | `0b9005412` | 10 | 47,48,49,50,51,52,53,54,55,59 | 11 | 3,260 |
| 5 | `73acf3591` | 10 | 60,62,63,64,66,68,72,74,76,79 | 14 | 2,952 |
| 6 | `c6cc74913` | 1  | 80 | 1 | 276 |
| **total** | | **51** | | **112** | **20,724** |

(`full_bytes_honest` sum across the 51 admits is 18,488 B; 20,724 B is
`full_bytes`, the column W16-HA's own table used.)

## Full per-candidate table (all 80)

`flat` / `chased` are the T1 verdicts from `icf_pair_adjudicate.py`;
`disq(C/S/B)` is the count of `CYCLE-ASSUMED` / `SLOT-REFUTED` /
`BYTES-DIFFER` trace lines (any nonzero disqualifies regardless of verdict);
`named_relocs` is `n_relocs - tolerated_placeholder` at FLAT T1 (blank where
FLAT T1 was UNDECIDABLE/REFUTED and the chase supplied the evidence instead,
per-pair chase evidence class is folded into `reason`).

| idx | ours | survivor | flat | chased | disq(C/S/B) | named_relocs | decision | reason |
|---|---|---|---|---|---|---:|---|---|
| 1 | `??1?$list@PAVPostProcessor@@V?$StlNodeAllo` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 2 | `??1?$list@PAUMerger@FileMerger@@V?$StlNode` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 3 | `??1?$list@PAVUIResource@@V?$StlNodeAlloc@P` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 4 | `??$?5VFilePath@@V?$StlNodeAlloc@VFilePath@` | `??$?5VFilePath@@V?$StlNodeAlloc@VFilePath@` | PROVEN | PROVEN | 0/0/0 | 9 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 5 | `?GetCurrentScreenState@BandUI@@QAAXAAV?$ve` | `?InviteParty@PlatformMgr@@QAAXH@Z` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 6 | `?push_back@?$vector@VPracticeSection@@V?$S` | `?push_back@?$vector@VPlayerTrackConfig@@V?` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 7 | `??1?$list@VSymbol@@V?$StlNodeAlloc@VSymbol` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 8 | `??1?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 9 | `??1?$list@PAVCharClip@@V?$StlNodeAlloc@PAV` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 10 | `??1?$list@IV?$StlNodeAlloc@I@stlpmtx_std@@` | `??$__destroy_aux@UEntry@LocalePanel@@@stlp` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |
| 11 | `??1?$list@VKeyFrame@EventAnim@@V?$StlNodeA` | `??3RndAnimatable@@SAXPAX@Z` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | no named relocs anywhere in proof, but VACUOUS-BUT-IDENTICAL (masked bytes AND raw reloc lists literally identical) |
| 12 | `??4BoneOp@CharSignalApplier@@QAAAAU01@ABU0` | `??4HighlightObject@@QAAAAV0@ABV0@@Z` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 13 | `??$__uninitialized_copy@PAUIKTarget@CharIK` | `??$__uninitialized_copy@PBVDataEvent@@PAV1` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 14 | `??$__uninitialized_copy@PBUBoneDesc@RndMes` | `??$__uninitialized_fill_n@PAUBoneDesc@RndM` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 15 | `??1StoreArtLoaderPanel@@UAA@XZ` | `?FakeProfileFill@ProfileMgr@@QAAXXZ` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 16 | `??$__uninitialized_fill_n@PAV?$Key@VTexPtr` | `??$__uninitialized_fill_n@PAUDistEntry@@IU` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 17 | `??$__uninitialized_copy@PAUMicMappingData@` | `??$__uninitialized_fill_n@PAUWeightedEntry` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 18 | `?PreInit@RndShaderMgr@@UAAXXZ` | `?insert@?$list@UBreakpoint@RndConsole@@V?$` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 19 | `??$__uninitialized_copy@PAVDeltaArray@Band` | `??$__uninitialized_copy@PBV?$Key@V?$vector` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 20 | `??$__uninitialized_fill_n@PAVVocalNote@@IV` | `??$__uninitialized_fill_n@PAVPracticeStep@` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 21 | `??$__uninitialized_fill_n@PAVPatchLayer@@I` | `??$__uninitialized_fill_n@PAV?$Key@V?$vect` | PROVEN | PROVEN | 0/0/0 | 2 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 22 | `??$__uninitialized_copy@PAUSpotlightEntry@` | `??$__uninitialized_fill_n@PAVCamShotFrame@` | PROVEN | PROVEN | 0/0/0 | 2 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 23 | `??$__uninitialized_fill_n@PAULod@Character` | `??$__uninitialized_copy@PAVCamShotFrame@@P` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 24 | `??0?$ObjOwnerPtr@VWaypoint@@@@QAA@ABV0@@Z` | `??0?$ObjPtr@VSeqInst@@@@QAA@ABV0@@Z` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 25 | `??$__uninitialized_copy@PAV?$ObjPtr@VSeqIn` | `??$__uninitialized_copy@PAUUnlockable@?A0x` | PROVEN | PROVEN | 0/0/0 | 3 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 26 | `??$__uninitialized_copy@PAUActionElement@I` | `??$__uninitialized_copy@PBUActionElement@I` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 27 | `??0BandPatchMesh@@QAA@ABV0@@Z` | `??0TransformArea@@QAA@ABV0@@Z` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 28 | `??0?$vector@PBDV?$StlNodeAlloc@PBD@stlpmtx` | `??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 29 | `?push_back@?$vector@VUserGuid@@V?$StlNodeA` | `?push_back@?$vector@VGemInProgress@SongPar` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 30 | `?_M_fill_insert@?$vector@IV?$StlNodeAlloc@` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 31 | `??$sort@PAPAVRndTransformable@@P6A_NPBV1@0` | `??$sort@PAPAVRndPollable@@P6A_NPBV1@0@Z@st` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 32 | `?reserve@?$vector@EV?$StlNodeAlloc@E@stlpm` | `?reserve@?$vector@DV?$StlNodeAlloc@D@stlpm` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 33 | `??$DeleteAll@V?$list@PAVPropKeys@@V?$StlNo` | `??$DeleteAll@V?$list@PAVContent@@V?$StlNod` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 34 | `?_M_fill_insert@?$vector@VOverlay@OutfitCo` | `?_M_fill_insert@?$vector@VSampleMarker@@V?` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 35 | `??0?$vector@PAVUIScreen@@V?$StlNodeAlloc@P` | `??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 36 | `?_M_fill_insert@?$vector@VSampleZone@@V?$S` | `?_M_fill_insert@?$vector@ULocalizedName@Ha` | PROVEN | PROVEN | 0/0/0 | 0 | DECLINE | PROVEN with 0 named relocation evidence and no VACUOUS-BUT-IDENTICAL |
| 37 | `??0BandPatchMesh@@QAA@PAVObject@Hmx@@@Z` | `?resize@?$vector@VMeshFace@BandPatchMesh@@` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 38 | `??$MakeString@VString@@VSymbol@@PBDV1@@@YA` | `??$MiloStripEval@VString@@VSymbol@@PBDV1@@` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 39 | `??0TrainerGemTab@@QAA@XZ` | `??$__destroy_range_aux@V?$reverse_iterator` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 40 | `?GetMyMic@GameMic@@QAAPAVMic@@XZ` | `??__Fmsg@?BH@??ProcessRecoResult@SpeechMgr` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 41 | `??$DeleteAll@V?$vector@PAVCharCreatorPrefa` | `?delete_and_clear@AllocInfoVec@@QAAXXZ` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 42 | `?_M_fill_insert@?$vector@PAVBlock@@V?$StlN` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 43 | `?_outline_GetAccomplishmentProgress@@YAABV` | `?GetAccomplishmentProgress@BandProfile@@QB` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 44 | `??$_M_allocate_and_copy@PBUHamSupereasyMea` | `??$_M_allocate_and_copy@PAURndPointTest@Ng` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 45 | `??4?$vector@VViewport@ObjectDir@@V?$StlNod` | `??4?$vector@VTransform@@V?$StlNodeAlloc@VT` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 46 | `??1AccomplishmentConditional@@UAA@XZ` | `??1AccomplishmentTrainerConditional@@UAA@X` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 47 | `??$__uninitialized_copy@PBVOverlay@OutfitC` | `??$__uninitialized_copy@PBVSampleMarker@@P` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 48 | `?push_back@?$vector@VCategory@CameraManage` | `?push_back@?$vector@VVector2@@V?$StlNodeAl` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 49 | `??$__uninitialized_copy@PAVActionRec@@PAV1` | `??$__uninitialized_copy@PBVActionRec@@PAV1` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 50 | `??$__uninitialized_fill_n@PAVVocalScoreHis` | `??$__uninitialized_fill_n@PAV?$vector@VVec` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 51 | `??$__uninitialized_copy@PAUGoalAcquisition` | `??$__uninitialized_copy@PAUPropertyFilter@` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 52 | `??$__uninitialized_fill_n@PAUMarker@@IU1@@` | `??$_Destroy_Range@PAVCartRow@@@stlpmtx_std` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 53 | `?push_back@?$vector@UWaitInfo@@V?$StlNodeA` | `?push_back@?$vector@VVector2@@V?$StlNodeAl` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 54 | `??$__uninitialized_copy@PBVMatSwap@OutfitC` | `??$__uninitialized_copy@PAVTransformArea@@` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 55 | `??$DeleteAll@V?$vector@PAVUIListWidget@@V?` | `??$DeleteAll@V?$vector@PAVNetSavedSetlist@` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 56 | `?_M_fill_insert@?$vector@VSeam@MeshAO@Outf` | `?_M_fill_insert@?$vector@V?$Key@M@@V?$StlN` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 57 | `?_M_fill_insert@?$vector@VTransform@@V?$St` | `?_M_fill_insert@?$vector@VViewport@ObjectD` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 58 | `??1PeakDetector@Synapse@DSP@@QAA@XZ` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlp` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 59 | `?push_back@?$vector@VTimeSigChange@Measure` | `?push_back@?$vector@VGemInProgress@SongPar` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 60 | `?resize@?$vector@VDeltaArray@BandFaceDefor` | `?resize@?$vector@VTrackerPlayerDisplay@@V?` | PROVEN | PROVEN | 0/0/0 | 2 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 61 | `?RegisterKey@BeatMatchController@@QBAXH@Z` | `?OnVoiceProcessingPassEnd@CX2SourceVoice@X` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 62 | `??$_M_allocate_and_copy@PBU?$pair@MM@stlpm` | `??$_M_allocate_and_copy@PBUXUSER_ACHIEVEME` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 63 | `?_M_erase@?$vector@VVocalNote@@V?$StlNodeA` | `?_M_erase@?$vector@VFlowMathOp@@V?$StlNode` | PROVEN | PROVEN | 0/0/0 | 3 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 64 | `?_M_fill_assign@?$vector@PAU_Slist_node_ba` | `?_M_fill_assign@?$vector@HV?$StlNodeAlloc@` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 65 | `??4?$vector@VColor@Hmx@@V?$StlNodeAlloc@VC` | `??4?$vector@VVector3@@V?$StlNodeAlloc@VVec` | REFUTED | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 66 | `??0?$vector@VRangeSection@@V?$StlNodeAlloc` | `??0?$vector@VVector3@@V?$StlNodeAlloc@VVec` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 67 | `?_M_fill_insert@?$vector@PAU_Slist_node_ba` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 68 | `?SetProcAndLock@Rnd@@QAAX_N@Z` | `?SetEvenOddDisabled@Rnd@@QAAX_N@Z` | UNDECIDABLE | PROVEN | 0/0/0 |  | ADMIT | no named relocs anywhere in proof, but VACUOUS-BUT-IDENTICAL (masked bytes AND raw reloc lists literally identical) |
| 69 | `??_G?$vector@VVector3@@V?$StlNodeAlloc@VVe` | `??_GCommonPhraseCapturer@@QAAPAXI@Z` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 70 | `??2RootObject@Quazal@@SAPAXIPBDI@Z` | `XShowSocialNetworkImagePostUI` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 71 | `?_M_fill_insert@?$vector@PAVFaderGroup@@V?` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 72 | `?_M_erase@?$vector@V?$vector@V?$RangedData` | `?_M_erase@?$vector@V?$vector@VVector3@@V?$` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 73 | `?Add@?$Keys@UDircutEntry@BandDirector@@U12` | `?Add@?$Keys@VSymbol@@V1@@@QAAHABVSymbol@@M` | REFUTED | PROVEN | 1/0/0 |  | DECLINE | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 74 | `?push_back@?$vector@VFatFingerData@Keyboar` | `?push_back@?$vector@VSongSection@@V?$StlNo` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 75 | `?GetGameData@Net@@QAAPAVNetGameData@@XZ` | `?GetSize@FileLoader@@QAAHXZ` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 76 | `?push_back@?$vector@VWaitingMachine@WaitLi` | `?push_back@?$vector@VVector2@@V?$StlNodeAl` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 77 | `?KeyGreaterEq@?$Keys@VQuat@Hmx@@V12@@@QBAH` | `?KeyLessEq@?$Keys@VColor@Hmx@@V12@@@QBAHM@` | REFUTED | REFUTED | 0/0/1 |  | DECLINE | CHASED T1 = REFUTED |
| 78 | `?Validate@Movie@@SAXXZ` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlp` | UNDECIDABLE | REFUTED | 0/0/0 |  | DECLINE | CHASED T1 = REFUTED |
| 79 | `?clear@?$_List_base@VBoneState@BandCharact` | `?clear@?$_List_base@VSetlistArtRecord@Musi` | PROVEN | PROVEN | 0/0/0 | 1 | ADMIT | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 80 | `??4?$vector@USpotlightDrawerEntry@LightPre` | `??4?$vector@VVector3@@V?$StlNodeAlloc@VVec` | REFUTED | PROVEN | 0/0/0 |  | ADMIT | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |

## Declines: six-way breakdown

29 declines fall into six mechanically distinct classes. Full detail (verdicts,
disqualifying labels, exact symbols) is in the table above; this section adds
the classification and, for group C, why these read as possible **wrong-callee
source bugs** rather than alias candidates (per task step 3: "pairs of clearly
unrelated functions ... record as possible wrong-callee source bugs rather
than aliases").

### Group A -- no compiled obj on our side (7: #27, 37, 40, 58, 61, 70, 75)

`??0BandPatchMesh@@QAA@ABV0@@Z`, `??0BandPatchMesh@@QAA@PAVObject@Hmx@@@Z`,
`?GetMyMic@GameMic@@QAAPAVMic@@XZ`, `??1PeakDetector@Synapse@DSP@@QAA@XZ`,
`?RegisterKey@BeatMatchController@@QBAXH@`, `??2RootObject@Quazal@@SAPAXIPBDI@Z`,
`?GetGameData@Net@@QAAPAVNetGameData@@XZ` -- all REFUTED at FLAT T1
(UNDECIDABLE) and REFUTED at CHASED T1 because the tool cannot locate a
compiled object defining "ours" at all (these game classes are pinned in
`splits.txt` but the source file that would define this specific member is
absent, stubbed, or not yet ported). Not a wrong-callee candidate -- there is
no code on our side to be wrong. Not an alias candidate either: an alias
needs two bodies to compare, and only one exists.

### Group B -- CYCLE-ASSUMED template twins (8: #30, 31, 42, 56, 57, 67, 71, 73)

All 8 are `_M_fill_insert<vector<T>>` / `Keys<T>::Add` / `sort<...>` STL
template instantiations that CHASED T1 rates PROVEN, but the chase trace
carries exactly one `CYCLE-ASSUMED` label each -- the chase recursion needed
to walk through a **mutually-recursive pair of helper calls** (each side's
helper calls back into a slot the chase is already in the middle of proving),
and the adjudicator's cycle-breaker assumes the very fact being proved to
terminate. Per the strict gate this is an automatic DECLINE regardless of the
PROVEN label -- assumed evidence is not evidence. These remain plausible ICF
aliases (same template shape family as the 51 admits) but are correctly left
for a future chase-depth improvement or manual proof, not installed on
assumed reasoning.

### Group B2 -- REFUTED after chase (1: #65)

`??4?$vector@VColor@Hmx@@...>` <-> `??4?$vector@VVector3@@...>` -- `operator=`
on two vectors of same-size-but-different-typed elements (`Color` vs
`Vector3`, both flat structs of floats). FLAT T1 REFUTED (relocation target
names disagree with no fold explaining it) and CHASED T1 REFUTED (the chase
could not find a slot-level fold that reconciles the disagreement). Grouped
with the wrong-callee candidates below since the disagreement pattern
(same-shape-different-type operator=) is exactly the shape of the "possible
wrong-callee" class, even though the trigger here is a template
instantiation rather than an ordinary hand-written function.

### Group C -- possible wrong-callee source bugs (10: #5, 12, 17, 18, 24, 38, 39, 46, 69, 77)

All 10 are REFUTED at both FLAT T1 and CHASED T1 -- relocation target names
disagree and no fold reconciles it. Per task step 3 these are recorded as
**possible wrong-callee source bugs**, not aliases, sub-classified by how
confidently the two sides look related:

- **Clearly unrelated (3): #5, #18, #39.** `GetCurrentScreenState@BandUI`
  paired against `InviteParty@PlatformMgr`; `PreInit@RndShaderMgr` against
  `insert@list<Breakpoint@RndConsole>`; `TrainerGemTab` ctor against
  `__destroy_range_aux<reverse_iterator<...>>`. No shared class, no shared
  operation, no shared template family -- these three are almost certainly
  coincidental byte-shape collisions between our (wrong or divergent) body
  and an unrelated retail function, not evidence of anything to fix on our
  side. Recorded, not actioned.
- **Structurally related but unproven (6): #12, #17, #24, #38, #46, #69.**
  `??4BoneOp@CharSignalApplier@@` vs `??4HighlightObject@@` (both
  `operator=` on similarly-shaped small structs); `__uninitialized_copy` vs
  `__uninitialized_fill_n` on the same template family with different
  arguments; `ObjOwnerPtr<Waypoint>` copy-ctor vs `ObjPtr<SeqInst>`
  copy-ctor (both smart-pointer copy ctors, different pointee AND different
  smart-pointer template); `MakeString<String,Symbol,...>` vs
  `MiloStripEval<String,Symbol,...>` (same template arg list, different
  operation); `AccomplishmentConditional` dtor vs
  `AccomplishmentTrainerConditional` dtor (sibling classes, likely different
  vtable/member layout); `vector<Vector3>::_M_destroy` (via `??_G` scalar
  deleting destructor) vs `CommonPhraseCapturer::??_G`. These share enough
  structural resemblance (same template family, sibling classes, or matching
  operation-on-similar-shape) that a genuinely wrong callee on our side is
  plausible, but there is not enough evidence here to name which side is
  wrong -- these are handed off as leads, not fixed by this lane (editing
  `scripts/symbol_aliases.json` is this lane's only authorized file).
- **Known prior defect, cross-referenced (1): #77.**
  `KeyGreaterEq<Keys<Quat>>` vs `KeyLessEq<Keys<Color>>` -- this is the
  *same class of bug* as the previously-withdrawn `KeyLessEq`/`KeyGreaterEq`
  alias documented in `rb3-xenon/CLAUDE.md` under the GROUNDED-2 /
  `??_M_erase`-era Keys<Quat>::Remove note ("one where retail calls
  `KeyLessEq` and we call `KeyGreaterEq`" -- a confirmed real wrong-comparator
  bug, not an alias, that a prior lane withdrew from
  `symbol_aliases.json` after proving it out). This instance is a different
  template parameter (`Quat`/`Color` here vs whatever the withdrawn one was)
  but the exact same comparator-swap shape. **Flagging this explicitly for
  the reviewer**: it is independent evidence that `KeyGreaterEq`/`KeyLessEq`
  confusion is a recurring, not one-off, source defect in the `Keys<T>`
  template usage across multiple `T` instantiations, and is worth a
  dedicated sweep for every `Keys<T>::KeyGreaterEq`/`KeyLessEq` call site
  rather than fixing them one at a time as alias candidates surface them.

### Group D -- VACUOUS, body too small to adjudicate (2: #43, #78)

`?_outline_GetAccomplishmentProgress@@` vs
`?GetAccomplishmentProgress@BandProfile@@`; `?Validate@Movie@@SAXXZ` vs a
`StlNodeAlloc<_List_node<int>>` ctor. Both UNDECIDABLE at FLAT T1 ("body
under 4 words or over half the words masked -- compares equal to too much")
and REFUTED at CHASED T1 (the chase could not supply a slot-level fold to
compensate for the vacuity). Too small a body for byte-identity to mean
anything either way; neither admitted as an alias nor flagged as a
wrong-callee candidate -- there just isn't enough signal in 1-2 words to
say anything.

### Group E -- PROVEN but no reviewable evidence (1: #36)

`_M_fill_insert<vector<SampleZone>>` vs `_M_fill_insert<vector<LocalizedName>>`
-- CHASED T1 is a clean PROVEN with **zero** disqualifying labels, but the
gate still declines it because it carries **zero named-relocation evidence
of any kind** (no FLAT T1 named match, no `SLOT-FOLD-OK`, no
`VACUOUS-DESTINATION-FOLD-PROVEN`) and does not qualify for the
`VACUOUS-BUT-IDENTICAL` fallback either. This is the one case in this batch
where the strict gate is plausibly too conservative -- the template-twin
shape (`_M_fill_insert<vector<T>>`, the same family as 5 of the 8 Group-B
admits-that-would-be) looks exactly like the admitted population, but the
adjudicator simply produced no relocation evidence to check. Declined per
the letter of the gate; flagged below for reviewer attention as the
single best "maybe should have been admitted with more chase depth"
candidate in this batch.

## Flagged for reviewer double-check

- **#68** `SetProcAndLock@Rnd` (bool-param setter) <-> `SetEvenOddDisabled@Rnd`
  (bool-param setter) -- admitted (chased PROVEN, named-relocation evidence,
  0 disqualifying labels), but unlike the STL container template twins that
  dominate this batch, this pair has no template-parameter explanation for
  why two differently-named, differently-purposed setters on the same class
  would compile to identical machine code. Plausible (two trivial one-line
  bool-flag setters on the same object layout can legitimately fold) but
  deserves the same scrutiny W16-HA gave its own #47
  (`IsScrolling`/`SetSpeed`).
- **#11** and **#68** are the two admits in this batch with the thinnest
  named-relocation backing (1 named reloc each, no chase recursion needed).
- **#77** `KeyGreaterEq<Keys<Quat>>`/`KeyLessEq<Keys<Color>>` -- see Group C
  above; cross-references a previously-withdrawn real defect of the exact
  same shape (`GROUNDED-2`/`Keys<Quat>::Remove` comparator-swap, per
  `rb3-xenon/CLAUDE.md`). Worth a dedicated sweep across every
  `Keys<T>::KeyGreaterEq`/`KeyLessEq` call site, not just the two instances
  now on record.
- **#36** `_M_fill_insert<vector<SampleZone>>`/`<vector<LocalizedName>>` --
  see Group E above; clean PROVEN, zero disqualifying labels, declined only
  for lack of relocation evidence to point at. Best "reopen with deeper
  chase" candidate in the batch.
- **#65** `operator=<vector<Color>>`/`<vector<Vector3>>` -- see Group B2;
  REFUTED at both stages, but flagging alongside the wrong-callee leads
  because the same-shape-different-type `operator=` pattern recurs (compare
  #12's `operator=` pair) and might be worth a combined look.

## A/B measurement (forward)

Forward = `scripts/symbol_aliases.json` at main-base (1697 groups) -> branch
tip (1730 groups), via `ab_measure.py --patch <git diff 8809d58ba..HEAD --
scripts/symbol_aliases.json>`. `[classify] paths=['scripts/symbol_aliases.json']
kinds=['map']` confirmed -- forces a re-split on both legs. Split reached its
own fixed point on the first pass on both legs (sha chain `1e8375f9 ->
1e8375f9`, 0 extra forced re-splits needed). Settle: leg A 2 iterations to
quiescence, leg B 2 iterations (first iteration real: `msvc=0 split=1 patch=1
other=3`, `renamer_patched=1834`).

| | matched | masked_equal | honest | code% | fuzzy% |
|---|---:|---:|---:|---:|---:|
| leg A (base, 1697 groups) | 44,444 | 23,324 | 21,120 | 41.788190 | 50.514984 |
| leg B (tip, 1730 groups) | 44,505 | 23,324 | 21,181 | 41.991253 | 50.515210 |
| **delta** | **+61** | **+0** | **+61** | **+0.203063pp** | **+0.000226pp** |

`Δcode_bytes = +20,808`. `none`-ruler control: `Δmatched_code=+0 B`,
`Δcode%=+0.000000` (matched=45,585, code%=44.994450, identical both legs) --
default ruler moved, none stayed flat, so the tool printed `ALIAS_SUSPECT`
(expected/benign for a genuine map-only alias install, per house convention
-- not a defect).

47 units improved (net +61); top movers: `SongData` +3, `VocalNoteList` +3,
`BandFaceDeform` +3, `FileMerger` +2, `LightPreset` +2, `MeasureMap` +2,
`MeshDeform` +2, `OutfitConfig` +2, `PatchDir` +2, `PropAnim` +2, `UI` +2,
plus 36 more at +1 each. Units at 100% (mpn ruler): 209 -> 211 (+2, both
`MATCHED_ROSE`: `default/network/net/Synchronize` 16->17,
`default/system/meta/StoreArtLoaderPanel` 15->16). Units at 100% (all-rows-
fuzzy ruler): 183 -> 186 (+3: the same two `MATCHED_ROSE` units plus
`default/MetaMusicScene` 18->18, `UNEXPLAINED`).

Tree restored to pre-run (base) state on exit; confirmed clean via
`git status --short` and re-checked group count (1697).

## A/B measurement (reverse)

Reverse = tip (1730 groups) -> base (1697 groups), exact mirror image,
confirming the forward result is not an artifact of run order or leg
ordering.

| | matched | masked_equal | honest | code% | fuzzy% |
|---|---:|---:|---:|---:|---:|
| leg A (tip, 1730 groups) | 44,505 | 23,324 | 21,181 | 41.991253 | 50.515210 |
| leg B (base, 1697 groups) | 44,444 | 23,324 | 21,120 | 41.788190 | 50.514984 |
| **delta** | **-61** | **+0** | **-61** | **-0.203063pp** | **-0.000226pp** |

`Δcode_bytes = -20,808` -- exact negation of the forward run. `none`-ruler:
flat both legs, default ruler moved down -> tool printed `FLAT: none
UNMOVED and default not up -- consistent with a pure RE-name` (no
`ALIAS_SUSPECT`; that warning is direction-sensitive and only fires when
the default ruler moves *up* while none stays flat). Same 47 units
regressed by the mirrored amount; units at 100% (mpn): 211 -> 209 (-2, both
fall off: `Synchronize` 17->16, `StoreArtLoaderPanel` 16->15); units at 100%
(all-rows-fuzzy): 186 -> 183 (-3, all three fall off, same names).

Tree restored to pre-run (tip) state on exit; confirmed clean via `git status
--short` (HEAD=`8a08b47e0`) and re-checked group count (1730).

## Validator: `tools/icf_alias_finder.py --validate`

Run twice: immediately after the predictions commit (tree at tip, 1730
groups, before either A/B leg), and again after both A/B legs completed and
restored the tree to tip. **Byte-identical output both times**:

```
COVERAGE: 1730 groups classified (1730/1730 reached, 7423 member spellings looked up)
  target side  : 3113 live target objs, 28409 mangled names indexed
  compiled side: 1222 compiled objs, 843004 symbols indexed (floor: 1050 objdiff.json base objs, all reached)
  OK (MAP-CONSISTENT)     1479
  TOLERATED PLACEHOLDER_SURVIVOR    34
  TOLERATED STALE_SPELLING          89
  TOLERATED SURVIVOR_MISLABELED     27
  TOLERATED UNWITNESSED            100
  CONTRADICTION_EXEMPT       1
  CONTRADICTED (FATAL)       0
VALIDATE: PASS -- 1479 map-consistent, 250 tolerated (enumerated above), 0 contradicted, 1730 total
```

This confirms both required properties: PASS / 0 CONTRADICTED before and
after, and **no membership already on main was lost or altered** by the two
A/B measurement runs' apply/force-resplit/settle/restore cycles -- the
before and after breakdowns match in every category.

## Native build gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Full run: 18/18 targets `OK ... -- relinked this run`, 0 build errors, 0
linker diagnostics, 0 warnings. This lane only touches
`scripts/symbol_aliases.json`, which the native build does not consume, so a
PASS here is expected rather than diagnostic -- run anyway per the task
brief's explicit final-action instruction and house convention.

## Bookkeeping commits

Two small commits on `w16-hc` exist solely to bracket the A/B measurement
runs and are not part of the alias-install work itself:

- `a53f02c97` -- temporarily reverted `scripts/symbol_aliases.json` to
  main-base content (`git checkout 8809d58ba --`) so the worktree's tracked
  state matched the forward patch's "before" side (`ab_measure.py --patch`
  applies the given patch *on top of* whatever the worktree currently holds,
  it does not interpret the patch as already-applied).
- `8a08b47e0` -- restored `scripts/symbol_aliases.json` back to branch-tip
  content (`git checkout 233277f11 --`) immediately after the forward A/B
  leg, so the reverse leg's "before" side was correct and so the branch
  ends at its intended tip state (1730 groups, matching the 51 admits).

Both are plain, explained commits (not `stash`/`reset --hard`/force-push),
consistent with the standing "commit early and often, write real messages
including negative results, never rewrite history" convention. Final `HEAD`
(`8a08b47e0`) carries the full 1730-group alias database; `git status
--short` is clean.

## Summary

51 of 80 candidate pairs from `w16hc_candidates.json` admitted into
`scripts/symbol_aliases.json` under the same strict chased-T1 gate as
W16-HA (33 new groups + 18 extensions of existing groups; 1697 -> 1730
groups). 29 declined, mechanically classified into six groups: 7 with no
compiled object on our side, 8 template twins blocked only by
`CYCLE-ASSUMED` chase recursion, 1 further REFUTED-after-chase template
instantiation, 10 recorded as possible wrong-callee source bugs (3 clearly
unrelated, 6 structurally related but unproven, 1 -- `KeyGreaterEq`/
`KeyLessEq` on `Keys<Quat>`/`Keys<Color>` -- matching a previously-withdrawn
real comparator-swap defect and worth a dedicated sweep), 2 too small to
adjudicate (VACUOUS), and 1 clean PROVEN declined only for lack of
relocation evidence (best reopen candidate). All 80 adjudications passed
`--survivor` by mangled name per the reviewer's tool-quirk warning; the
`MISSING(retail)` failure mode never occurred. Forward A/B (base->tip):
`matched +61 / masked_equal +0 / honest +61 / code% +0.203063pp / code_bytes
+20,808 / fuzzy% +0.000226pp`, `none`-ruler flat (expected `ALIAS_SUSPECT`
shape for a map-only install). Reverse A/B is the exact mirror image
(`-61`/`+0`/`-61`/`-0.203063pp`/`-20,808`/`-0.000226pp`, `none` flat, correctly
printing the direction-sensitive "consistent with a pure RE-name" message
instead of `ALIAS_SUSPECT`). Units at 100% (mpn) 209->211, (all-rows-fuzzy)
183->186, both forward, mirrored in reverse. `icf_alias_finder.py --validate`
read PASS / 0 CONTRADICTED with byte-identical breakdowns both before and
after both A/B legs (1730 groups, 1479 map-consistent, 250 tolerated, 1
contradiction-exempt, 0 contradicted) -- no existing membership was
disturbed. `tools/native_build_gate.sh` reports
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0
partial=0 failed=0 rc=0`. Worktree ends clean at `8a08b47e0` on branch
`w16-hc`, not merged, not pushed, no attribution trailers.
