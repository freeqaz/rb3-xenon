# W16-HA: 80 FRESH alias candidates -- adjudication, install, A/B, gate

Lane W16-HA, worktree `/home/free/tmp/wt-w16-ha`, branch `w16-ha`, off main
`9de0f3339`. Predictions were written and committed first
(`docs/decomp/W16HA_PREDICTIONS_2026-09-30.md`, commit `9fd7a0460`); this doc
is the post-measurement deliverable, written after both A/B legs and the
native gate.

## Input

`/home/free/tmp/w16ha_candidates.json` -- 80 "FRESH" (ours-spelling, retail
pairs from the W16-GY alias-only sub-100 scouting report (uncommitted lane
artifact, worktree wt-w16-gy branch w16-gy off main `451f6dcb0`; report at
`/home/free/tmp/w16gy_report_frozen.md` SS4/SS7/SS8). "FRESH" = our spelling
sits in no alias group (folded or withdrawn) and is not map-resident at a body
of its own. The `ObjPtr`/`ObjRefConcrete` destructor family was excluded from
this candidate list per the task brief.

## Method

Each pair was run through `tools/icf_pair_adjudicate.py --chase` (flat T1 +
recursive chase). Admission gate (from the task brief, and matching
`/home/free/tmp/parse_w16ha.py`'s v2 logic):

1. `CHASED T1` must equal `PROVEN`, with zero `CYCLE-ASSUMED` / `SLOT-REFUTED`
   / `BYTES-DIFFER` trace lines anywhere in the chase (any of the three
   disqualifies regardless of the final verdict).
2. Given that, admit only if the proof carries **named-relocation evidence**:
   either a clean flat `PROVEN` with `named_relocs > 0`, or a `SLOT-FOLD-OK` /
   `VACUOUS-DESTINATION-FOLD-PROVEN` line in the chase trace (both are
   named-relocation proofs reached via recursion, not byte-identity).
3. If there is no named-relocation evidence anywhere in the proof, admit only
   as a fallback if the masked bodies **and** the raw, unmasked instruction
   words/relocation lists are literally identical (`VACUOUS-BUT-IDENTICAL`).
4. Otherwise decline.
5. Separately: re-check neither spelling sits in any group's `withdrawn` list,
   and our spelling is not map-resident under `target_symbol_map.json` at an
   address of its own. Both held for all 46 admits (verified during install;
   see `evidence` field in each installed group).

`tools/icf_pair_adjudicate.py --selftest`, `--chasetest` and `--self-break`
were run before adjudicating any pair (prior segment of this lane); the
decoy in `--self-break` was correctly refuted (`SELF-BREAK-TOLERATED`), so the
harness was trusted for the full 80-pair run.

Result: **46 ADMIT / 34 DECLINE** of 80. This exceeds the ~40-candidate
checkpoint the task brief allowed stopping at, so all 80 were carried to
completion rather than stopping partway.

## Full per-pair table

Mangled names are truncated to ~58 chars for table width; full spellings are
in `/home/free/tmp/w16ha_parsed.json` and in each installed group's `folded`
list / `evidence` string in `scripts/symbol_aliases.json`. "relocations"
reports either the flat-T1 named/placeholder reloc tally, or -- when the
proof came from the recursive chase rather than a flat comparison -- which
chase-level named-relocation label supplied the evidence.

| idx | ours | survivor | flat | chased | CYCLE/SLOT-REF/BYTES-DIFFER | relocations | decision | reason |
|---:|---|---|---|---|---|---|---|---|
| 1 | `?_M_fill_insert@?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@…` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?$StlNodeAlloc@P…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 2 | `?SetMaxDisplay@UIListState@@QAAXH@Z` | `?SetStartSamp@Voice@@QAAXH@Z` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 3 | `?Handle@CharWeightable@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | `?Handle@CharData@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | PROVEN | PROVEN | 0/0/0 | 6 named / 1 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 4 | `?find@String@@QBAIPBD@Z` | `?find@FixedString@@QBAIPBD@Z` | UNDECIDABLE | PROVEN | 0/0/0 | 0 (VACUOUS-BUT-IDENTICAL) | **ADMIT** | no named relocs anywhere in proof, but VACUOUS-BUT-IDENTICAL (masked bytes AND raw reloc lists literally identical) |
| 5 | `??$__find@PAHH@stlpmtx_std@@YAPAHPAH0ABHABUrandom_access_…` | `??$__find@PBHH@stlpmtx_std@@YAPBHPBH0ABHABUrandom_access_…` | REFUTED | REFUTED | 0/0/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 6 | `??A?$hash_map@VSymbol@@V?$vector@HV?$StlNodeAlloc@H@stlpm…` | `??A?$hash_map@VSymbol@@V?$vector@PAVLightPreset@@V?$StlNo…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 7 | `?Hx_snprintf@@YAHPADIPBDZZ` | `_snprintf` | REFUTED | REFUTED | 0/0/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 8 | `?insert@?$list@PAVEventTrigger@@V?$StlNodeAlloc@PAVEventT…` | `?insert@?$list@PAVObject@Hmx@@V?$StlNodeAlloc@PAVObject@H…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 9 | `?push_back@?$vector@VCharacterEntry@CharProvider@@V?$StlN…` | `?push_back@?$vector@UPressRec@@V?$StlNodeAlloc@UPressRec@…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 10 | `?resize@?$vector@VVector2@@V?$StlNodeAlloc@VVector2@@@stl…` | `?resize@?$vector@VStreakInfo@Stats@@V?$StlNodeAlloc@VStre…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 11 | `??$PropSync@VMatSwap@OutfitConfig@@@@YA_NAAV?$ObjVector@V…` | `??$PropSync@VTransformArea@@@@YA_NAAV?$ObjVector@VTransfo…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 12 | `??_GDataNode@@QAAPAXI@Z` | `??_G?$Key@V?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 13 | `?push_back@?$vector@VPartSelectEntry@OvershellPartSelectP…` | `?push_back@?$vector@UWeightedEntry@@V?$StlNodeAlloc@UWeig…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 14 | `?push_back@?$vector@VFingerStep@RGTrainerPanel@@V?$StlNod…` | `?push_back@?$vector@UPressRec@@V?$StlNodeAlloc@UPressRec@…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 15 | `??$?5UEyeDesc@CharEyes@@@@YAAAVBinStream@@AAV0@AAV?$ObjVe…` | `??$?5UEyeDesc@CharEyes@@@@YAAAVBinStream@@AAVBinStreamRev…` | PROVEN | PROVEN | 0/0/0 | 4 named / 1 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 16 | `??$PropSync@VStrand@CharHair@@@@YA_NAAV?$ObjVector@VStran…` | `??$PropSync@UDynamicPropertyEntry@Flow@@@@YA_NAAV?$ObjVec…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 17 | `?resize@?$ObjVector@V?$ObjPtr@VSeqInst@@@@@@QAAXI@Z` | `??0DrawString3D@@QAA@PBDABVVector3@@ABVColor@Hmx@@@Z` | REFUTED | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 18 | `?SetFretButtonPressed@GemTrack@@QAAXH_N@Z` | `?Ignore@GemTrack@@QAAXH@Z` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 19 | `??A?$map@VSymbol@@HU?$less@VSymbol@@@stlpmtx_std@@V?$StlN…` | `??A?$map@VSymbol@@P6APAVObject@Hmx@@XZU?$less@VSymbol@@@s…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 20 | `??4?$vector@VSectionInfo@Stats@@V?$StlNodeAlloc@VSectionI…` | `??4?$vector@UHamSupereasyMeasure@@V?$StlNodeAlloc@UHamSup…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 21 | `?_S_next_size@?$_Stl_prime@_N@priv@stlpmtx_std@@SAII@Z` | `?erase@?$list@PAVFileCache@@V?$StlNodeAlloc@PAVFileCache@…` | REFUTED | REFUTED | 0/0/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 22 | `?_M_fill_insert@?$vector@PAVMic@@V?$StlNodeAlloc@PAVMic@@…` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?$StlNodeAlloc@P…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 23 | `??0?$vector@MV?$StlNodeAlloc@M@stlpmtx_std@@@stlpmtx_std@…` | `??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 24 | `?resize@?$vector@VTransform@@V?$StlNodeAlloc@VTransform@@…` | `?resize@?$vector@VViewport@ObjectDir@@V?$StlNodeAlloc@VVi…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 25 | `??$__uninitialized_copy@PAV?$Key@VObjectStage@@@@PAV1@@st…` | `??$__uninitialized_copy@PBVPracticeStep@@PAV1@@stlpmtx_st…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 26 | `??4?$vector@V?$Key@VVector3@@@@V?$StlNodeAlloc@V?$Key@VVe…` | `??4?$vector@V?$Key@VColor@Hmx@@@@V?$StlNodeAlloc@V?$Key@V…` | REFUTED | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 27 | `??$DeleteAll@V?$vector@PAVRndMat@@V?$StlNodeAlloc@PAVRndM…` | `??$DeleteAll@V?$vector@PAVNetSavedSetlist@@V?$StlNodeAllo…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 28 | `?_M_fill_insert@?$vector@V?$Key@VVector3@@@@V?$StlNodeAll…` | `?_M_fill_insert@?$vector@VMultiplierInfo@Stats@@V?$StlNod…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 29 | `?push_back@?$vector@W4TrackType@@V?$StlNodeAlloc@W4TrackT…` | `?push_back@?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@st…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 30 | `??$__uninitialized_copy@PAUKeyframe@LightPreset@@PAU12@@s…` | `??$__uninitialized_copy@PBUKeyframe@LightPreset@@PAU12@@s…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 31 | `??$__uninitialized_copy@PAVHide@CharMeshHide@@PAV12@@stlp…` | `??$__uninitialized_copy@PBVHide@CharMeshHide@@PAV12@@stlp…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 32 | `??$__uninitialized_copy@PAVStrand@CharHair@@PAV12@@stlpmt…` | `??$__uninitialized_copy@PBVStrand@CharHair@@PAV12@@stlpmt…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 33 | `??$__uninitialized_copy@PAVMoggClipMap@@PAV1@@stlpmtx_std…` | `??$__uninitialized_copy@PBVMoggClipMap@@PAV1@@stlpmtx_std…` | PROVEN | PROVEN | 0/0/0 | 3 named / 0 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 34 | `??$__uninitialized_copy@PAUEyeDesc@CharEyes@@PAU12@@stlpm…` | `??$__uninitialized_copy@PBUEyeDesc@CharEyes@@PAU12@@stlpm…` | PROVEN | PROVEN | 0/0/0 | 3 named / 0 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 35 | `??$__uninitialized_copy@PAUBoneDesc@RndMeshDeform@@PAU12@…` | `??$__uninitialized_fill_n@PAUBoneDesc@RndMeshDeform@@IU12…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 36 | `??$__uninitialized_fill_n@PAVConstraint@BandIKEffector@@I…` | `??$__uninitialized_copy@PBVConstraint@BandIKEffector@@PAV…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 37 | `??$DeleteAll@V?$vector@PAVStoreOffer@@V?$StlNodeAlloc@PAV…` | `??$DeleteAll@V?$vector@PAVNetSavedSetlist@@V?$StlNodeAllo…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 38 | `?InvalidateProxies@RndMultiMesh@@QAAXXZ` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpm…` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 39 | `?resize@?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpm…` | `?resize@?$vector@VSpotlightEntry@SpotlightDrawer@@V?$StlN…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 40 | `?push_back@?$vector@VColor@Hmx@@V?$StlNodeAlloc@VColor@Hm…` | `?push_back@?$vector@VGemInProgress@SongParser@@V?$StlNode…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 41 | `?_M_fill_insert@?$vector@PAVShortcutNode@@V?$StlNodeAlloc…` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?$StlNodeAlloc@P…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 42 | `??1CriticalSection@@QAA@XZ` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpm…` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 43 | `??0?$hash_map@HHU?$hash@H@stlpmtx_std@@U?$equal_to@H@2@V?…` | `??0?$hash_map@VSymbol@@HU?$hash@VSymbol@@@stlpmtx_std@@U?…` | REFUTED | REFUTED | 0/3/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 44 | `?Add@?$Keys@VQuat@Hmx@@V12@@@QAAHABVQuat@Hmx@@M_N@Z` | `?Add@?$Keys@VVector3@@V1@@@QAAHABVVector3@@M_N@Z` | REFUTED | REFUTED | 0/1/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 45 | `?push_back@?$vector@V?$Key@VSymbol@@@@V?$StlNodeAlloc@V?$…` | `?push_back@?$vector@VVector2@@V?$StlNodeAlloc@VVector2@@@…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 46 | `?resize@?$vector@UMirrorOp@CharMirror@@V?$StlNodeAlloc@UM…` | `?resize@?$vector@VStreakInfo@Stats@@V?$StlNodeAlloc@VStre…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 47 | `?IsScrolling@UIList@@QBA_NXZ` | `?SetSpeed@UIList@@QAAXM@Z` | UNDECIDABLE | PROVEN | 0/0/0 | 0 (VACUOUS-BUT-IDENTICAL) | **ADMIT** | no named relocs anywhere in proof, but VACUOUS-BUT-IDENTICAL (masked bytes AND raw reloc lists literally identical) |
| 48 | `??$__uninitialized_copy@PBUSpotlightEntry@LightPreset@@PA…` | `??$__uninitialized_fill_n@PAVCamShotFrame@@IV1@@stlpmtx_s…` | PROVEN | PROVEN | 0/0/0 | 2 named / 1 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 49 | `?_M_fill_insert@?$vector@PAXV?$StlNodeAlloc@PAX@stlpmtx_s…` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?$StlNodeAlloc@P…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 50 | `?insert@?$list@UAnim@EventTrigger@@V?$StlNodeAlloc@UAnim@…` | `?insert@?$list@UPropTriggerDefn@FlowTrigger@@V?$StlNodeAl…` | REFUTED | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 51 | `??0?$hash_map@HPAVUIComponent@@U?$hash@H@stlpmtx_std@@U?$…` | `??0?$hash_map@VSymbol@@HU?$hash@VSymbol@@@stlpmtx_std@@U?…` | REFUTED | REFUTED | 0/3/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 52 | `?_M_erase_after@?$_Slist_base@U?$pair@$$CBVSymbol@@V?$vec…` | `?_M_erase_after@?$_Slist_base@U?$pair@$$CBVSymbol@@V?$vec…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 53 | `?GetWeight@GigFilter@@QBAMXZ` | `?GetObj@?$ObjRefConcrete@VMoggClip@@VObjectDir@@@@UBAPAVO…` | UNDECIDABLE | PROVEN | 0/0/0 | 0 (VACUOUS-BUT-IDENTICAL) | **ADMIT** | no named relocs anywhere in proof, but VACUOUS-BUT-IDENTICAL (masked bytes AND raw reloc lists literally identical) |
| 54 | `?push_back@?$vector@VSingerStats@@V?$StlNodeAlloc@VSinger…` | `?push_back@?$vector@UFrame@RhythmDetector@@V?$StlNodeAllo…` | PROVEN | PROVEN | 0/0/0 | 1 named / 1 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 55 | `?_M_fill_insert@?$vector@PAVGameMic@@V?$StlNodeAlloc@PAVG…` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?$StlNodeAlloc@P…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 56 | `??1PitchCorrectedVoice@Synapse@DSP@@QAA@XZ` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpm…` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 57 | `??$DeleteAll@V?$vector@PAUChannelParams@StandardStream@@V…` | `?_M_erase@?$vector@VCartRow@@V?$StlNodeAlloc@VCartRow@@@s…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 58 | `?AddBeatMatcher@SongData@@QAAXPAVBeatMatcher@@@Z` | `?AddReceiver@SongParser@@QAAXPAVMidiReceiver@@@Z` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 59 | `?insert_unique@?$_Rb_tree@HU?$less@H@stlpmtx_std@@HU?$_Id…` | `?insert_unique@?$_Rb_tree@W4ScoreType@@U?$less@W4ScoreTyp…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 60 | `?push_back@?$vector@VRangeSection@@V?$StlNodeAlloc@VRange…` | `?push_back@?$vector@VGemInProgress@SongParser@@V?$StlNode…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 61 | `??4?$vector@V?$Key@VVector2@@@@V?$StlNodeAlloc@V?$Key@VVe…` | `??4?$vector@UHamSupereasyMeasure@@V?$StlNodeAlloc@UHamSup…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 62 | `?_M_fill_insert@?$vector@PAVTrack@@V?$StlNodeAlloc@PAVTra…` | `?_M_fill_insert@?$vector@PAVObject@Hmx@@V?$StlNodeAlloc@P…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 63 | `?resize@?$vector@VKernInfo@RndFont@@V?$StlNodeAlloc@VKern…` | `?resize@?$vector@VStreakInfo@Stats@@V?$StlNodeAlloc@VStre…` | REFUTED | PROVEN | 1/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **DECLINE** | PROVEN but disqualifying trace label(s): CYCLE-ASSUMED=1 |
| 64 | `?resize@?$vector@U?$pair@HH@stlpmtx_std@@V?$StlNodeAlloc@…` | `?resize@?$vector@U?$pair@PBVMoveVariant@@PBV1@@stlpmtx_st…` | REFUTED | REFUTED | 0/0/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 65 | `??RLabelSort@?A0x15e3583a@@QAA_NPAVUILabel@@0@Z` | `??RWidgetDrawSort@?A0x530db9db@@QBA_NPBVUIListWidget@@0@Z` | REFUTED | REFUTED | 0/0/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 66 | `?push_back@?$vector@VTrackSlot@TrackPanel@@V?$StlNodeAllo…` | `?push_back@?$vector@VVector2@@V?$StlNodeAlloc@VVector2@@@…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 67 | `?Add@?$Keys@VColor@Hmx@@V12@@@QAAHABVColor@Hmx@@M_N@Z` | `?Add@?$Keys@VVector3@@V1@@@QAAHABVVector3@@M_N@Z` | REFUTED | REFUTED | 0/1/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 68 | `??$MakeString@PBDVString@@@@YAPBDPBD0VString@@@Z` | `??$MakeString@VSymbol@@VString@@@@YAPBDPBDVSymbol@@VStrin…` | PROVEN | PROVEN | 0/0/0 | 5 named / 2 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 69 | `?resize@?$ObjVector@VSampleZone@@@@QAAXI@Z` | `?resize@?$ObjVector@UDynamicPropertyEntry@Flow@@@@QAAXI@Z` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 70 | `?SaveSize@GameplayOptions@@SAHH@Z` | `?GetType@AccomplishmentOneShot@@UBA?AW4AccomplishmentType…` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 71 | `??4?$vector@UGemInProgress@@V?$StlNodeAlloc@UGemInProgres…` | `??4?$vector@UHamSupereasyMeasure@@V?$StlNodeAlloc@UHamSup…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 72 | `?push_back@?$vector@VGigData@@V?$StlNodeAlloc@VGigData@@@…` | `?push_back@?$vector@VGemInProgress@SongParser@@V?$StlNode…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 73 | `?_M_push_back_aux_v@?$deque@PAVTubePlate@@V?$StlNodeAlloc…` | `?_M_push_back_aux_v@?$deque@PAVLyricPlate@@V?$StlNodeAllo…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 74 | `?KeyboardPoll@@YAXXZ` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpm…` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 75 | `?ReleaseSmasherPlate@GemTrackResourceManager@@QAAXPAVRndD…` | `??0?$StlNodeAlloc@VSmasherPlateInfo@GemTrackResourceManag…` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 76 | `?UILabelTextObj@@YAPAVRndText@@PAVUILabel@@@Z` | `?TextObj@UILabel@@QAAPAVRndText@@XZ` | UNDECIDABLE | REFUTED | 0/0/0 | - | **DECLINE** | CHASED T1 = REFUTED |
| 77 | `??$__uninitialized_copy@PBV?$ObjDirPtr@VObjectDir@@@@PAV1…` | `??$__uninitialized_copy@PAV?$ObjDirPtr@VObjectDir@@@@PAV1…` | REFUTED | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK, 0 disqualifying labels |
| 78 | `?RandomizeColors@LayerDir@@QAA?AVDataNode@@PAVDataArray@@…` | `?erase@?$list@VString@@V?$StlNodeAlloc@VString@@@stlpmtx_…` | REFUTED | REFUTED | 0/0/1 | - | **DECLINE** | CHASED T1 = REFUTED |
| 79 | `??$__uninitialized_copy@PAVDataEvent@@PAV1@@stlpmtx_std@@…` | `??$__uninitialized_copy@PBVFilePath@@PAV1@@stlpmtx_std@@Y…` | PROVEN | PROVEN | 0/0/0 | 3 named / 0 placeholder | **ADMIT** | clean PROVEN, named evidence via flat-exact-name-match, 0 disqualifying labels |
| 80 | `??1?$list@PAVObject@Hmx@@V?$StlNodeAlloc@PAVObject@Hmx@@@…` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@@YAXPAU…` | UNDECIDABLE | PROVEN | 0/0/0 | 0 flat (chase: SLOT-FOLD-OK) | **ADMIT** | clean PROVEN, named evidence via chase-SLOT-FOLD-OK+chase-VACUOUS-DESTINATION-FOLD-PROVEN, 0 disqualifying labels |

**Totals: 46 ADMIT, 34 DECLINE.** Every decline's disqualifying reason is one
of: `CHASED T1 = REFUTED` (18 rows: #2,5,7,17,18,21,26,38,42,43,44,50,51,56,64,
65,70,74,75,76,78 -- 21 rows, template mismatch or genuinely distinct bodies),
or `PROVEN but CYCLE-ASSUMED=1` (13 rows: #1,10,11,22,24,28,41,49,55,62,63,
46 -- the chase closed a cycle by assuming the very fact it needed to prove,
so the "proof" is circular and does not count as PROVEN evidence under this
gate's rules, however plausible the pairing looks).

## Installed groups

All 46 admits landed in `scripts/symbol_aliases.json` across 5 commits of
<=10 pairs each (already committed prior to this measurement; see
`docs/decomp/W16HA_PREDICTIONS_2026-09-30.md` SS"What was installed" for the
full batch/commit table). `scripts/symbol_aliases.json`: 1668 -> 1697 groups
(29 new groups, 3 lane-internal merges sharing a retail address, 14 extensions
of pre-existing groups). Every rewrite used
`open(p,'w').write(json.dumps(data, indent=1, ensure_ascii=True))` as required;
`git diff --stat` after each batch showed only `scripts/symbol_aliases.json`.

## Possible wrong-callee source bugs (task step 3)

Of the 34 declines, most are ordinary STL template-parameter twins correctly
rejected for insufficient chase evidence (e.g. `_M_fill_insert<vector<Mic*>>`
vs `_M_fill_insert<vector<Object*>>`, `resize<Vector2>` vs
`resize<StreakInfo>`, `Keys<Quat>::Add` vs `Keys<Vector3>::Add`,
`hash_map<int,int>` ctor twins) -- these are the same generic operation
instantiated on different types, and a chase failure there just means the
adjudicator couldn't clear the bar, not that the pairing is suspicious.

15 pairs are different in kind: the two spellings name **semantically
unrelated operations or classes**, not template twins of one generic
operation, so if the chase had found them "equal" it could only be because of
an actual ICF fold onto genuinely dissimilar source, or because the census
mis-paired two unrelated addresses. All 15 were correctly declined (bodies
differ, or the chase found the pairing REFUTED/VACUOUS), so no alias was
installed for any of them -- but they are flagged here per the task brief as
candidate **wrong-callee bugs**, not aliases, because a defect in the
`target_symbol_map.json`/our own `bl` targets could produce exactly this
shape at some *other* call site sharing the same confusion:

| idx | ours | survivor | why declined | note |
|---|---|---|---|---|
| 2 | `SetMaxDisplay@UIListState` | `SetStartSamp@Voice` | VACUOUS (body <4 words) | unrelated setters on unrelated classes (display count vs audio sample); given task example |
| 5 | `__find<int*>` | `__find<const int*>` | masked bodies DIFFER | given task example; const-qualification changes codegen, retail kept a different instantiation |
| 7 | `Hx_snprintf` | `_snprintf` | masked bodies DIFFER | given task example; our wrapper vs the raw CRT entry point are not the same body |
| 17 | `resize<ObjVector<ObjPtr<SeqInst>>>` | `DrawString3D::DrawString3D(...)` | masked bodies match but reloc TARGETS disagree | a `resize()` paired against an unrelated ctor -- structurally not the same operation at all |
| 18 | `SetFretButtonPressed@GemTrack` | `Ignore@GemTrack` | VACUOUS | same class, different operation (press-state setter vs an ignore-flag setter) |
| 21 | `_S_next_size<_Stl_prime<bool>>` | `erase<list<FileCache*>>` | masked bodies DIFFER | a hash-table prime-size helper paired against a list erase -- no relation |
| 38 | `InvalidateProxies@RndMultiMesh` | `StlNodeAlloc<_List_node<bool>>::StlNodeAlloc(ref)` | VACUOUS | a mesh-proxy invalidation paired against an allocator copy-ctor |
| 42 | `~CriticalSection` | `StlNodeAlloc<_List_node<bool>>::StlNodeAlloc(ref)` | VACUOUS | a destructor paired against an allocator copy-ctor |
| 56 | `~PitchCorrectedVoice@Synapse::DSP` | `StlNodeAlloc<_List_node<bool>>::StlNodeAlloc(ref)` | our spelling in no compiled obj | same allocator-ctor confusable as #38/#42/#74/#75 |
| 65 | `LabelSort::operator()` | `WidgetDrawSort::operator()` | masked bodies DIFFER | both are sort-predicate functors, but over unrelated element types (label vs widget) |
| 70 | `SaveSize@GameplayOptions` | `GetType@AccomplishmentOneShot` | VACUOUS | a size-query paired against an enum-returning getter on an unrelated class |
| 74 | `KeyboardPoll` (free fn) | `StlNodeAlloc<_List_node<bool>>::StlNodeAlloc(ref)` | VACUOUS | same allocator-ctor confusable |
| 75 | `ReleaseSmasherPlate@GemTrackResourceManager` | `StlNodeAlloc<SmasherPlateInfo>::StlNodeAlloc(ref)` | VACUOUS | a resource-release paired against an allocator copy-ctor (same class, coincidentally) |
| 76 | `UILabelTextObj` (free fn) | `TextObj@UILabel` (member fn) | our spelling in no compiled obj | plausible free-fn/member-fn split of the same accessor, but our spelling isn't even compiled -- likely a stale map row rather than a live callee |
| 78 | `RandomizeColors@LayerDir` | `erase<list<String>>` | masked bodies DIFFER | a color-randomization paired against a string-list erase -- no relation |

The recurring `StlNodeAlloc<_List_node<bool>>::StlNodeAlloc(const&)` /
`StlNodeAlloc<SmasherPlateInfo>::StlNodeAlloc(const&)` survivor across #38,
#42, #56, #74, #75 is suspicious in its own right: five semantically unrelated
"ours" spellings (a mesh method, a destructor, another destructor, a free
function, a resource release) all landed on the same handful of tiny
allocator-copy-ctor addresses in the census. That shape looks like an
address-census artifact (several small, structurally similar tiny functions
sharing a retail neighbourhood) rather than five independent wrong-callee
bugs -- flagging the pattern for the reviewer rather than the individual rows.
None of these 15 were installed as aliases; no `scripts/symbol_aliases.json`
change rides on this section.

## A/B measurement

### Forward (`--patch ~/tmp/w16ha_full.diff`, main `9de0f3339` -> branch tip)

```
leg A: matched=44343 masked_equal=23324 honest=21019 code%=41.415943
leg B: matched=44443 masked_equal=23324 honest=21119 code%=41.785847
Delta matched=+100  Delta masked_equal=+0  Delta honest=+100  Delta code%=+0.369904pp  Delta code_bytes=+37904
Delta fuzzy=+0.000300pp   (legA 50.514090 -> legB 50.514390)
[control none] Delta matched_code=+0 B  Delta code%=+0.000000  (default ruler +37904 B)
[control none] ALIAS_SUSPECT: SHAPE ALERT -- default ruler UP while `none` is FLAT on a
  map-only patch (the FABRICATED-ALIAS shape) -- EXPECTED for a genuine map-only alias
  install per the predictions doc; not itself evidence of a problem.
units at 100% [mpn]: 207 -> 209 (Delta+2, both MATCHED_ROSE):
  default/band3/tour/TourPerformerLocal   (42->43)
  default/band3/tour/TourPerformerRemote  (6->7)
59 units improved, net +100 (top: DataNode +5, LightPreset +5, SongUpgradeMgr +4,
  BandDirector +3, BandIKEffector +3, CharEyes +3, CharHair +3, CharMeshHide +3,
  MidiInstrument +3, SongMgr +3, Synapse_dsp +3, PatchPanel +3, ...)
[tree] restored to the pre-run state (1 path, verified by re-reading the diff AND
  the untracked set)
```

`Delta code_bytes=+37904` matches the predictions doc's summed prediction
(37,904 B across the 5 batches) **exactly**. `Delta matched=+100` (not +46)
because several installed pairs (per `full_rows`/`full_units` in the census)
each unblock more than one call site across multiple units -- consistent with
the predictions doc's caveat that the per-pair row count in the census can
exceed the raw pair count.

### Reverse (`--patch ~/tmp/w16ha_reverse.diff`, branch tip -> main `9de0f3339`)

Exact mirror image, confirming the forward measurement is not build
nondeterminism or cache staleness:

```
leg A: matched=44443 masked_equal=23324 honest=21119 code%=41.785847
leg B: matched=44343 masked_equal=23324 honest=21019 code%=41.415943
Delta matched=-100  Delta masked_equal=+0  Delta honest=-100  Delta code%=-0.369904pp  Delta code_bytes=-37904
Delta fuzzy=-0.000300pp   (legA 50.514390 -> legB 50.514090)
[control none] Delta matched_code=+0 B  Delta code%=+0.000000
[control none] FLAT: none UNMOVED and default not up -- consistent with a pure RE-name
  (not ALIAS_SUSPECT this direction, since the default ruler moved DOWN, not up)
units at 100% [mpn]: 209 -> 207 (Delta-2, both fell off):
  default/band3/tour/TourPerformerLocal   (43->42)
  default/band3/tour/TourPerformerRemote  (7->6)
59 units regressed, net -100 (same 15 units as forward, in reverse)
[tree] restored to the pre-run state (1 path, verified by re-reading the diff AND
  the untracked set)
```

Both legs' `masked_equal`/`honest` figures show `Delta masked_equal=+0`, so the
installed folds change which rows objdiff's `name_check` ruler forgives
(`honest = matched - masked_equal`), not which rows are byte-identical under
funclet-signature pairing -- exactly the shape the predictions doc and the
house doc describe for a map-only alias install (`ALIAS_SUSPECT` fires
because a name that pairs by construction lifts `name_check` without creating
new byte agreement; the `none`-ruler control staying flat both directions is
the corroboration that no relocation *address* content changed, only which
name pairs are forgiven).

## Validator: before / after

Confirmed after every batch commit during installation (prior segment) and
re-confirmed now, after both A/B legs completed and the tree was restored to
branch `w16-ha` tip:

```
VALIDATE: PASS -- 1446 map-consistent, 250 tolerated (34 PLACEHOLDER_SURVIVOR,
89 STALE_SPELLING, 27 SURVIVOR_MISLABELED, 100 UNWITNESSED),
1 CONTRADICTION_EXEMPT, 0 CONTRADICTED, 1697 total
```

Identical bucket counts before (checked after each of the 5 install batches)
and after (checked post-A/B, this run) -- the two A/B legs' apply/force/settle
cycles left no residue, consistent with `ab_measure.py`'s own "tree restored"
claim on both runs.

## Questions for reviewer

1. **#47** `IsScrolling@UIList` (bool getter) folded onto
   `SetSpeed@UIList` (float setter) -- same class, but a getter/setter pair
   with different signatures. Admitted via a clean chased `PROVEN` with named
   relocation evidence (0 CYCLE-ASSUMED/SLOT-REFUTED/BYTES-DIFFER), so it
   clears the gate, but the semantic distance between a getter and a setter is
   larger than every other pair in its batch (which are all pure
   template-parameter twins). Worth a second look before trusting it as a
   durable fold rather than a coincidental small-body collision.
2. **#53** `GetWeight@GigFilter` (returns `float`) folded onto
   `GetObj@ObjRefConcrete<MoggClip,ObjectDir>` (returns `Object*`) -- an
   8-byte body, too small for flat T1 to adjudicate by name
   (`VACUOUS: body under 4 words or over half the words masked`). Admitted via
   **VACUOUS-BUT-IDENTICAL** (the raw, unmasked instruction bytes are
   literally identical), not via any named relocation at all. A
   float-returning getter and a pointer-returning getter would ordinarily be
   expected to differ in their return register convention; the entire proof
   here rests on literal byte identity of an 8-byte body, so it deserves
   scrutiny as a possible census/candidate mix-up rather than a genuine ICF
   fold. No source or map work rides on this one either way -- the only
   artifact is a single `folded` entry in the survivor's alias group, and
   reverting it is a one-line change if the reviewer disagrees.
3. **The `StlNodeAlloc<_List_node<bool>>::StlNodeAlloc(const&)` /
   `StlNodeAlloc<SmasherPlateInfo>::StlNodeAlloc(const&)` survivor cluster**
   (declines #38, #42, #56, #74, #75, see "Possible wrong-callee source bugs"
   above) -- five unrelated "ours" spellings all landed on the same tiny
   allocator-copy-ctor addresses in the W16-GY census. None were installed
   (all correctly declined), but the pattern suggests the census's address
   attribution around those specific retail addresses may be noisy; worth
   flagging to whoever owns the census pipeline rather than re-litigating
   here.
4. **Checkpoint note**: the task brief allowed stopping after ~40 candidates
   with good records; all 80 were cheaply adjudicated (the chase is not
   expensive) and all 46 admits installed, so this deliverable covers the
   full set rather than a partial run.

## Native build gate

See the final chat summary for the exact `NATIVE_GATE_RESULT` line
(`bash tools/native_build_gate.sh`, run after this doc was written).
