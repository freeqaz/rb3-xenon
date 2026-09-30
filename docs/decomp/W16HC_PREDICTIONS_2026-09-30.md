# W16-HC: predictions before measuring

Lane W16-HC, worktree `/home/free/tmp/wt-w16-hc`, branch `w16-hc`, off main
`8809d58ba`. This doc is written and committed **before** running
`tools/ab_measure.py`, per house convention (predict, commit, then measure).

## What was installed

51 of the 80 FRESH candidate pairs from `/home/free/tmp/w16hc_candidates.json`
(the next-80 continuation of W16-HA's ranked-by-honest-bytes FRESH list,
28,368 B / 152 rows across the 80 candidates) were admitted by
`tools/icf_pair_adjudicate.py --chase` under the same strict gate used by
W16-HA (CHASED T1 == PROVEN, zero CYCLE-ASSUMED / SLOT-REFUTED / BYTES-DIFFER
trace lines; named-relocation evidence required -- either a clean FLAT-T1
name match, or a `SLOT-FOLD-OK` / `VACUOUS-DESTINATION-FOLD-PROVEN` chase
label -- or `VACUOUS-BUT-IDENTICAL` as the only fallback). 29 candidates were
declined (see the deliverable doc for the full breakdown, including 10 pairs
recorded as possible wrong-callee source-bug candidates rather than aliases).

Installed into `scripts/symbol_aliases.json` across 6 commits of <=10 pairs
each (batches are a plain sequential chunking of the 51 admits in ascending
candidate-idx order -- this lane routed extend-vs-new-group programmatically
by matching each admit's `survivor` against every existing group's
`survivor` field, rather than hand-curating the split as W16-HA did):

| batch | commit | pairs | idx | predicted rows | predicted bytes |
|---|---|---:|---|---:|---:|
| 1 | `6664646a1` | 10 | 1,2,3,4,6,7,8,9,10,11 | 58 | 6,368 |
| 2 | `15cd75acb` | 10 | 13,14,15,16,19,20,21,22,23,25 | 16 | 4,176 |
| 3 | `5809a6a12` | 10 | 26,28,29,32,33,34,35,41,44,45 | 12 | 3,692 |
| 4 | `0b9005412` | 10 | 47,48,49,50,51,52,53,54,55,59 | 11 | 3,260 |
| 5 | `73acf3591` | 10 | 60,62,63,64,66,68,72,74,76,79 | 14 | 2,952 |
| 6 | `c6cc74913` | 1  | 80 | 1 | 276 |
| **total** | | **51** | | **112** | **20,724** |

(`full_bytes_honest` sum across the 51 admits is **18,488 B**; the 20,724 B
figure is `full_bytes`, matching the column W16-HA's table used.)

`scripts/symbol_aliases.json`: 1697 groups at branch start (post-W16-HA) ->
**1730 groups** at HEAD (33 new groups created across the 6 batches; the
remaining 18 admits extended an existing group -- 7 into pre-existing
`_M_destroy_range`/`~list` group `#1540`, plus extensions into groups
`#1676`/`#36`/`#1586`/`#1640`/`#1647` that predate or were created earlier
in this lane, e.g. idx 44 was folded into a group idx 51 also targets, since
both share the CameraManager::PropertyFilter `__uninitialized_copy` retail
address).

## Per-pair prediction table

`full_rows` / `full_bytes` are taken directly from
`/home/free/tmp/w16hc_candidates.json` for each candidate idx
(`detail.retail_addr`, `full_units`) -- these are the rows currently scored
`mpn < 100` at that retail address because the callee name disagrees under
`name_check`.

| idx | ours (truncated) | survivor (truncated) | rows | bytes | example unit |
|---|---|---|---:|---:|---|
| 1 | `??1?$list@PAVPostProcessor@@V?$StlNodeAlloc@PAVPos` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 5 | 616 | default/system/rndobj/Rnd |
| 2 | `??1?$list@PAUMerger@FileMerger@@V?$StlNodeAlloc@PA` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 5 | 616 | default/FileMerger |
| 3 | `??1?$list@PAVUIResource@@V?$StlNodeAlloc@PAVUIReso` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 4 | 572 | default/UI |
| 4 | `??$?5VFilePath@@V?$StlNodeAlloc@VFilePath@@@stlpmt` | `??$?5VFilePath@@V?$StlNodeAlloc@VFilePath@@@stlpmt` | 1 | 428 | default/UISlider |
| 6 | `?push_back@?$vector@VPracticeSection@@V?$StlNodeAl` | `?push_back@?$vector@VPlayerTrackConfig@@V?$StlNode` | 1 | 428 | default/band3/game/PracticeSectionProvider |
| 7 | `??1?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpm` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 22 | 1348 | default/BandDirector +more |
| 8 | `??1?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 12 | 888 | default/BandCharacter +more |
| 9 | `??1?$list@PAVCharClip@@V?$StlNodeAlloc@PAVCharClip` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 3 | 504 | default/CharClipSet |
| 10 | `??1?$list@IV?$StlNodeAlloc@I@stlpmtx_std@@@stlpmtx` | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@` | 3 | 504 | default/Crowd |
| 11 | `??1?$list@VKeyFrame@EventAnim@@V?$StlNodeAlloc@VKe` | `??3RndAnimatable@@SAXPAX@Z` | 2 | 464 | default/EventAnim |
| 13 | `??$__uninitialized_copy@PAUIKTarget@CharIKHand@@PA` | `??$__uninitialized_copy@PBVDataEvent@@PAV1@@stlpmt` | 1 | 424 | default/FileMerger |
| 14 | `??$__uninitialized_copy@PBUBoneDesc@RndMeshDeform@` | `??$__uninitialized_fill_n@PAUBoneDesc@RndMeshDefor` | 2 | 424 | default/MeshDeform |
| 15 | `??1StoreArtLoaderPanel@@UAA@XZ` | `?FakeProfileFill@ProfileMgr@@QAAXXZ` | 6 | 556 | default/band3/meta_band/StoreInfoPanel +more |
| 16 | `??$__uninitialized_fill_n@PAV?$Key@VTexPtr@RndMatA` | `??$__uninitialized_fill_n@PAUDistEntry@@IU1@@stlpm` | 1 | 416 | default/MatAnim |
| 19 | `??$__uninitialized_copy@PAVDeltaArray@BandFaceDefo` | `??$__uninitialized_copy@PBV?$Key@V?$vector@VColor@` | 1 | 396 | default/system/bandobj/BandFaceDeform |
| 20 | `??$__uninitialized_fill_n@PAVVocalNote@@IV1@@stlpm` | `??$__uninitialized_fill_n@PAVPracticeStep@@IV1@@st` | 1 | 392 | default/VocalNoteList |
| 21 | `??$__uninitialized_fill_n@PAVPatchLayer@@IV1@@stlp` | `??$__uninitialized_fill_n@PAV?$Key@V?$vector@VVect` | 1 | 392 | default/PatchDir |
| 22 | `??$__uninitialized_copy@PAUSpotlightEntry@LightPre` | `??$__uninitialized_fill_n@PAVCamShotFrame@@IV1@@st` | 1 | 392 | default/LightPreset |
| 23 | `??$__uninitialized_fill_n@PAULod@Character@@IU12@@` | `??$__uninitialized_copy@PAVCamShotFrame@@PAV1@@stl` | 1 | 392 | default/Character |
| 25 | `??$__uninitialized_copy@PAV?$ObjPtr@VSeqInst@@@@PA` | `??$__uninitialized_copy@PAUUnlockable@?A0xf8e4b4b5` | 1 | 392 | default/Sequence |
| 26 | `??$__uninitialized_copy@PAUActionElement@InlineHel` | `??$__uninitialized_copy@PBUActionElement@InlineHel` | 1 | 392 | default/InlineHelp |
| 28 | `??0?$vector@PBDV?$StlNodeAlloc@PBD@stlpmtx_std@@@s` | `??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpm` | 1 | 388 | default/ContextChecker |
| 29 | `?push_back@?$vector@VUserGuid@@V?$StlNodeAlloc@VUs` | `?push_back@?$vector@VGemInProgress@SongParser@@V?$` | 2 | 384 | default/SessionMessages +more |
| 32 | `?reserve@?$vector@EV?$StlNodeAlloc@E@stlpmtx_std@@` | `?reserve@?$vector@DV?$StlNodeAlloc@D@stlpmtx_std@@` | 1 | 384 | default/SongDB |
| 33 | `??$DeleteAll@V?$list@PAVPropKeys@@V?$StlNodeAlloc@` | `??$DeleteAll@V?$list@PAVContent@@V?$StlNodeAlloc@P` | 2 | 380 | default/PropAnim |
| 34 | `?_M_fill_insert@?$vector@VOverlay@OutfitConfig@@V?` | `?_M_fill_insert@?$vector@VSampleMarker@@V?$StlNode` | 1 | 372 | default/OutfitConfig |
| 35 | `??0?$vector@PAVUIScreen@@V?$StlNodeAlloc@PAVUIScre` | `??0?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpm` | 1 | 368 | default/UI |
| 41 | `??$DeleteAll@V?$vector@PAVCharCreatorPrefab@Prefab` | `?delete_and_clear@AllocInfoVec@@QAAXXZ` | 1 | 348 | default/band3/meta_band/PrefabMgr |
| 44 | `??$_M_allocate_and_copy@PBUHamSupereasyMeasure@@@?` | `??$_M_allocate_and_copy@PAURndPointTest@NgRnd@@@?$` | 1 | 340 | default/PartAnim |
| 45 | `??4?$vector@VViewport@ObjectDir@@V?$StlNodeAlloc@V` | `??4?$vector@VTransform@@V?$StlNodeAlloc@VTransform` | 1 | 336 | default/system/obj/Dir |
| 47 | `??$__uninitialized_copy@PBVOverlay@OutfitConfig@@P` | `??$__uninitialized_copy@PBVSampleMarker@@PAV1@@stl` | 1 | 336 | default/OutfitConfig |
| 48 | `?push_back@?$vector@VCategory@CameraManager@@V?$St` | `?push_back@?$vector@VVector2@@V?$StlNodeAlloc@VVec` | 1 | 328 | default/CameraManager |
| 49 | `??$__uninitialized_copy@PAVActionRec@@PAV1@@stlpmt` | `??$__uninitialized_copy@PBVActionRec@@PAV1@@stlpmt` | 1 | 328 | default/ButtonHolder |
| 50 | `??$__uninitialized_fill_n@PAVVocalScoreHistory@@IV` | `??$__uninitialized_fill_n@PAV?$vector@VVector3@@V?` | 1 | 328 | default/band3/game/Singer |
| 51 | `??$__uninitialized_copy@PAUGoalAcquisitionInfo@@PA` | `??$__uninitialized_copy@PAUPropertyFilter@CameraMa` | 1 | 328 | default/band3/meta_band/AccomplishmentManager |
| 52 | `??$__uninitialized_fill_n@PAUMarker@@IU1@@stlpmtx_` | `??$_Destroy_Range@PAVCartRow@@@stlpmtx_std@@YAXPAV` | 1 | 328 | default/StandardStream |
| 53 | `?push_back@?$vector@UWaitInfo@@V?$StlNodeAlloc@UWa` | `?push_back@?$vector@VVector2@@V?$StlNodeAlloc@VVec` | 1 | 324 | default/Joypad |
| 54 | `??$__uninitialized_copy@PBVMatSwap@OutfitConfig@@P` | `??$__uninitialized_copy@PAVTransformArea@@PAV1@@st` | 1 | 324 | default/band3/bandtrack/Gem |
| 55 | `??$DeleteAll@V?$vector@PAVUIListWidget@@V?$StlNode` | `??$DeleteAll@V?$vector@PAVNetSavedSetlist@@V?$StlN` | 1 | 320 | default/UIListDir |
| 59 | `?push_back@?$vector@VTimeSigChange@MeasureMap@@V?$` | `?push_back@?$vector@VGemInProgress@SongParser@@V?$` | 2 | 316 | default/MeasureMap |
| 60 | `?resize@?$vector@VDeltaArray@BandFaceDeform@@V?$St` | `?resize@?$vector@VTrackerPlayerDisplay@@V?$StlNode` | 2 | 312 | default/system/bandobj/BandFaceDeform |
| 62 | `??$_M_allocate_and_copy@PBU?$pair@MM@stlpmtx_std@@` | `??$_M_allocate_and_copy@PBUXUSER_ACHIEVEMENT@@@?$v` | 1 | 308 | default/SongData |
| 63 | `?_M_erase@?$vector@VVocalNote@@V?$StlNodeAlloc@VVo` | `?_M_erase@?$vector@VFlowMathOp@@V?$StlNodeAlloc@VF` | 2 | 308 | default/VocalNoteList |
| 64 | `?_M_fill_assign@?$vector@PAU_Slist_node_base@priv@` | `?_M_fill_assign@?$vector@HV?$StlNodeAlloc@H@stlpmt` | 3 | 304 | default/PatchDir +more |
| 66 | `??0?$vector@VRangeSection@@V?$StlNodeAlloc@VRangeS` | `??0?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@s` | 1 | 296 | default/SongData |
| 68 | `?SetProcAndLock@Rnd@@QAAX_N@Z` | `?SetEvenOddDisabled@Rnd@@QAAX_N@Z` | 1 | 292 | default/PropSync |
| 72 | `?_M_erase@?$vector@V?$vector@V?$RangedData@I@?$Ran` | `?_M_erase@?$vector@V?$vector@VVector3@@V?$StlNodeA` | 1 | 288 | default/SongData |
| 74 | `?push_back@?$vector@VFatFingerData@KeyboardTrackWa` | `?push_back@?$vector@VSongSection@@V?$StlNodeAlloc@` | 1 | 284 | default/KeyboardTrackWatcherImpl |
| 76 | `?push_back@?$vector@VWaitingMachine@WaitList@LockS` | `?push_back@?$vector@VVector2@@V?$StlNodeAlloc@VVec` | 1 | 284 | default/band3/meta_band/LockStepMgr |
| 79 | `?clear@?$_List_base@VBoneState@BandCharacter@@V?$S` | `?clear@?$_List_base@VSetlistArtRecord@MusicLibrary` | 1 | 276 | default/BandCharacter |
| 80 | `??4?$vector@USpotlightDrawerEntry@LightPreset@@V?$` | `??4?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@s` | 1 | 276 | default/LightPreset |

## Flagged for reviewer double-check (not a normal template twin)

- **#68** `SetProcAndLock@Rnd` (bool-param setter) <-> `SetEvenOddDisabled@Rnd`
  (bool-param setter) -- same class (`Rnd`), same signature shape (`void
  (bool)`), but two semantically distinct setters, not a template
  instantiation family. Admitted via chased PROVEN with named-relocation
  evidence and zero disqualifying labels. Flagging because unlike the STL
  container template twins that dominate this batch, this pair has no
  template-parameter explanation for why two differently-named, differently-
  purposed setters on the same class would compile to identical machine code
  -- it is plausible (two trivial one-line bool-flag setters on the same
  object layout can legitimately fold), but it deserves the same scrutiny
  W16-HA gave its own #47 (`IsScrolling`/`SetSpeed`) getter/setter pair.
- **#11** and **#68** (see per-pair table) are the two admits in this batch
  with the fewest named-relocation labels backing them (1 named reloc each,
  no chase recursion) -- listed explicitly in the deliverable doc's
  reviewer-questions section together with the `VACUOUS-BUT-IDENTICAL`-only
  admits.

## Predicted A/B outcome

- **Forward** (`--patch`, main..branch-tip): `matched_functions` (`mpn`
  ruler) should rise by up to +51 (one per admitted pair, each pair's own row
  reaching `mpn == 100`) -- could be less if some rows also carry other
  unrelated charged sites, and idx 7/8/15 in particular touch rows with
  multiple sites (22, 12, and 6 rows respectively) where only some sites may
  be attributable to this specific aliased callee. `matched_code` (`fuzzy`
  ruler) should rise by some fraction of the 20,724 B predicted above -- a
  row only pays out in full when the aliased call site is its ONLY remaining
  charge, per every prior alias-install lane (W16-GM/W16-GV/W16-HA). Expect
  `ALIAS_SUSPECT` to fire on the forward leg's `control_none_shape()` check
  (map-only patch, no `source` changes) -- expected, not a defect. `none`-
  ruler control should be flat (Delta 0) since none of these changes touch
  relocation-**address** content, only which pairs `name_check` forgives.
- **Reverse** (revert leg): mirror-image negative deltas; `masked_equal`
  should also move for pairs that newly participate in a `masked_equal` fold
  population (extends into pre-existing groups may already be counted).
- **Validator**: `icf_alias_finder.py --validate` should read **PASS, 0
  CONTRADICTED** both before (1697 groups) and after (1730 groups) --
  already confirmed clean after every batch commit during installation.
