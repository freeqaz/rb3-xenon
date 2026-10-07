# W16-UM: naming the retail addresses W16-UH proved our callee to be (2026-10-07)

> **STATUS (2026-10-07):** lane record. Branch `w16-um`, not merged. Every one
> of W16-UH's 236 PROVEN (address, callee) pairs has a commit or a recorded
> reason: per pair in `W16UM_PAIR_DISPOSITIONS_2026-10-07.json`, per address below.

## Question

W16-UH's census (`docs/decomp/W16UH_PLACEHOLDER_CALLEE_CENSUS_2026-10-07.md`,
`~/tmp/w16uh/census_after.json`) found 236 distinct pairs (A, F) where retail
calls an unnamed address A, we call F, and `icf_pair_adjudicate.chase(fn_<A>, F)`
holds. They cover **86 addresses**; template folds give one address up to 31
spellings. This lane names A in `scripts/target_symbol_map.json` wherever the
name lets A's retail row pair with a body we compile, and prices each wave with
`tools/ab_measure.py`.

## Method

`tools/name_proven_placeholders.py` (new).

- `plan` re-reads the census, finds A's retail unit (the dtk target obj that
  defines `fn_<A>`), and sorts each address:

  | class | rule | addresses | pairs |
  |---|---|---:|---:|
  | NAMEABLE | a census spelling is compiled in A's unit obj and is the map name nowhere else | 31 | 149 |
  | TWIN | no census spelling is compiled there, but the unit obj defines an unmapped, unpaired function of the same size that also chases PROVEN at A | 11 | 15 |
  | MAPPED-ELSEWHERE | the only spelling is the map name at another address | 1 | 1 |
  | NOT-IN-UNIT-OBJ | A's unit obj compiles neither a census spelling nor a twin | 28 | 49 |
  | NO-BASE-OBJ | A's unit has no compiled obj | 15 | 22 |

  All 236 pairs re-chased PROVEN on this tree (`8ed7f6b1d`, after W16-UK).
- `map` writes rows through `tools/gated_map_write.py` (absent keys) or replaces
  a deliberate `null` row in place, then runs the whole-map object-side P7 audit
  and `tools/map_name_injectivity.py`.
- `alias` (after a build, so the target objs carry the new name) admits each
  other PROVEN spelling of A to the alias group at A, so its call sites stay
  forgiven once A has a name. Admission needs `chase(S, F)` PROVEN with no
  undischarged slot and no assumed cycle except self-recursion (a recursive
  `_M_erase` re-entering a callee pair the chase already entered), F not the map
  name anywhere, and F not folded at another placed address. The invariants are
  the ones `tools/alias_locate_home.py` asserts. `--extra 0xADDR=F` admits a
  spelling at an address that already has a name, by the same rule; it was used
  for charges that a newly paired row exposed.
- Per wave: map rows, forced re-split, build (it stops at the alias gates), then
  `tools/alias_survivor_relabel.py --write` for placeholder-survivor groups,
  `alias`, `tools/alias_callee_name_drift.py --reprove --write`, rebuild to rc 0.
  The wave is committed and measured with `ab_measure --revert HEAD`, so leg A is
  the wave and leg B is its parent. The deltas below are written as the wave's
  effect (the tool's numbers with the sign flipped).

## Waves

All map kind. Each leg was settled and read at a split fixed point (0 extra
re-splits on both legs every time).

| wave | commit | addresses | Δfns | Δcode B | units at 100% (mpn) |
|---|---|---|---:|---:|---:|
| 1: one spelling each | `c4d84cfbf` | 15 | +14 | +2,124 | +2 (CharUpperTwist, VocalPlayer) |
| 2: two spellings, second folded | `7188511f8` | 10 | +10 | +988 | +2 (Msg, PanelDir) |
| 3: wide folds (5-31 spellings) | `ba085a822` | 6 | +2 | +368 | +5 (BandScoreboard, CharWeightSetter, CharacterCreatorPanel, DataUtl, Sfx) |
| 4: unit-local fold twins | `bd7c9c27a` | 11 | +11 | +416 | +3 (FriendsProvider, SongInfoCopy, TextFileStream) |
| 5: NgFur ctor moved | `cc9562c3d` | 1 | +1 | +60 | 0 |
| **total** | | **43** | **+38** | **+3,956** | **+12** |

The legs compose: wave 1's leg B is the base, 54,957 fns / 6,079,608 B, and
wave 5's leg A is 54,995 / 6,083,564. `masked_equal` was 25,223 on every leg.

Wave 1 predicted 2,252 B (the 15 rows' sizes) and measured 2,124: `resize<ObjPtr<SeqInst>>`
paired at 99.84, with one charge on `_M_erase<ObjPtr<SeqInst>>` (range
overload) where the map names `_M_erase<ObjPtr<Sequence>>` at 0x82708310. Wave 2
admitted that spelling there and the row reached 100.

### Charges the new names exposed

Naming an address turns its forgiven placeholder call sites into checked ones.
Each loss was read from a row diff of the legs:

| wave | row | before → after | reading | done |
|---|---|---|---|---|
| 2 | `fn_827767F0`, `fn_827FDB50` (UIList, masked pairs) | 100 → 99.69 / 99.55 | the paired base calls `__destroy_range<vector<Vector3>>` where retail calls 0x82774330 | admitted at 0x82774330 in wave 3 (chase PROVEN clean); back to 100 |
| 2 | `~_Rb_tree<TrackType,int>` (PerfectSectionTracker, 4 B) | 100 → 95 | W16-UH map defect: retail's row is `b 0x826DA438`, the clear of another tree type | kept |
| 3 | `_M_insert_overflow_aux<FilePath>` (PropSync, from wave 1) | 100 → 99.94 | calls `_M_clear_after_move<FilePath>`; not a census spelling of 0x822ce170, chase PROVEN clean | admitted in wave 3; back to 100 |
| 3 | `ObjPtrList<EventTrigger>::clear` / `~ObjPtrList` / `Load` (EventTrigger, SongSectionController; 468 B) | 100 → 99.79 / 99.76 / 99.93 | W16-UH map defect: retail calls 0x8271A138, an `Unlink` without the virtual-base step; our `Unlink<EventTrigger>` has the step and is already folded at 0x8227d0e8 | kept |
| 3 | `__ucopy_ptrs<_Slist_node_base**>` (InterstitialMgr, 4 B) | 100 → 95 | W16-UH map defect: a 4-byte `b 0x8260EED8` named by shape | kept |
| 4 | `__make_heap<Friend*>` (new row) | pairs at 99.81 | calls `__adjust_heap<Friend*>` where the map names `<RndPollable*>` at 0x823f78b0 | admitted there (chase PROVEN clean); 100 |

The kept charges cover five rows, 476 B. They scored 100 only because their
callee had no name. `ObjPtrList<EventTrigger>::clear` is the one census spelling
not folded at its address (0x823b8618), because it is the map name at 0x8249d1f0.

### Wave 5: a wrong map row

The census's MAPPED-ELSEWHERE pair was `(0x82B8B670, ??0NgFur@@IAA@XZ)`, with the
name mapped at 0x8282a080. Retail's body at 0x8282a080 (rtti span, 60 B) calls
the routine the map names `??0bad_cast@std@@QAA@PBD@Z` and stores vtable
`lbl_8212B4AC`, the one the `bad_typeid` / `__non_rtti_object` ctors store.
`chase(??0NgFur @ 0x8282a080, ours)` is REFUTED (BYTES-DIFFER, SLOT-REFUTED). At
0x82b8b670 it is PROVEN (the vtable RTTI slots read NgFur). The row was nulled,
with `_w16um_nulled_comment` in the map, and the name moved. The rtti row had
read 99.67 against the wrong name and is now unpaired. Its CRT identity is left open.

### Alias records

- 0x8246f020, 0x824ce758 and 0x828070e0 had W16-OU placeholder-survivor groups.
  `alias_survivor_relabel.py --write` set their survivors to the map name, and
  the lane fields it hard-codes were corrected to W16-UM (`rechase_w16um`). The
  first two fold nothing now: their one member became the survivor, and the
  placeholder label was dropped instead of being folded.
- 0x826da438 and 0x827690d0 correspond to address-less partition groups 170 and
  168, which render nothing. The new placed groups carry the census spellings.
  The address-less groups were left unchanged.
- `alias_callee_name_drift --reprove` re-proved every group whose recorded
  callees were renamed: 4, 12, 41 + 1 and 72 + 1 groups in waves 1-4. None
  regressed (the tool will not record a group in which a PROVEN member stops
  proving).

## Not named, with reasons

The 43 addresses below are 71 pairs. Naming them pairs no row: either the
retail row's unit compiles none of our spellings (the body's COMDAT survivor
sits in a different unit's span from every obj that compiles it), or the unit
has no obj at all. Naming them would only turn forgiven call sites into checked
ones.

### NOT-IN-UNIT-OBJ

| address | size | retail unit | census spelling (first) | pairs | where our spellings compile |
|---|---:|---|---|---:|---|
| `0x82272308` | 164 | MessageTimer | `??$Find@VUIScreen@@@ObjectDir@@QAAPAVUISc...` | 1 | MusicLibrary, MusicLibraryStore, SetlistMergePanel |
| `0x82272548` | 196 | MessageTimer | `??$PropSync@VUIPanel@@@@YA_NAAPAVUIPanel@...` | 1 | PanelDir, UISlider |
| `0x82272b90` | 72 | CalibrationPanel | `??1UIScreen@@UAA@XZ` | 1 | MetaPanel, UIScreen, band3/meta_band/SigninScreen |
| `0x822784a8` | 60 | InlineHelp | `??$_Copy_Construct@VPatchLayer@@@stlpmtx_...` | 2 | PatchDir |
| `0x822a2ea0` | 112 | SampleInst | `?_M_clear@?$vector@VOverlay@OutfitConfig@...` | 2 | ExternalMic, OutfitConfig, band3/bandtrack/Gem |
| `0x822b0d48` | 112 | BandCamShot | `??0?$ObjPtr@VEventTrigger@@@@QAA@PAVObjec...` | 1 | Accomplishment, BandCharDesc, BandCrowdMeter |
| `0x822bb3c8` | 128 | Geo | `??$__push_heap@PAPAVRndPollable@@HPAV1@P6...` | 1 | Anim, MatAnim, ShaderOptions |
| `0x822c9048` | 60 | CharCollide | `??$_Copy_Construct@UObjVersion@@@stlpmtx_...` | 2 | AmbientOcclusion, Anim, BandCamShot |
| `0x822cdb70` | 120 | BandStarDisplay | `??0?$ObjPtr@VRndMesh@@@@QAA@ABV0@@Z` | 1 | AmbientOcclusion, Anim, BandCamShot |
| `0x822dc828` | 8 | ChordShapeGenerator | `?clear@VertVector@RndMesh@@QAAXXZ` | 1 | Mesh, NoteTube, band3/bandtrack/Gem |
| `0x8230e5b8` | 76 | Group | `?clear@?$_List_base@VPracticeSectionMappi...` | 1 | CharIKFingers, SongSectionController |
| `0x82359680` | 224 | TrackPanelDirBase | `?insert_unique@?$_Rb_tree@KU?$less@K@stlp...` | 3 | QuestManager, XLSPConnection, band3/meta_band/SongRecord |
| `0x8235ba70` | 8 | band3/game/Game | `?GetTourProgress@Tour@@QBAPAVTourProgress...` | 2 | GameMicManager, Tour |
| `0x8235c9a0` | 224 | CharClip | `?insert_unique@?$_Rb_tree@VSymbol@@U?$les...` | 3 | AccomplishmentConditional, AccomplishmentDiscSongConditional, AccomplishmentPlayerConditional |
| `0x8239c5d8` | 92 | Text | `?_M_erase@?$vector@UColorSet@@V?$StlNodeA...` | 1 | ColorPalette, Spotlight |
| `0x823f1898` | 28 | MemMgr | `?GetUserData@AddUserRequestMsg@@QBAXAAVBi...` | 3 | SessionMessages |
| `0x824417d8` | 112 | system/rndobj/Utl | `?_M_erase@?$vector@V?$Key@VTexPtr@RndMatA...` | 1 | MatAnim, UITransitionHandler |
| `0x825122b0` | 104 | Archive | `??$MakeString@PBDPBDPBDPBDPBD@@YAPBDPBD00...` | 2 | ConnectionStatusPanel, HolmesClient, MemTracker |
| `0x825459e0` | 8 | CharServoBone | `?IsOnlineEnabled@NetSession@@QBA_NXZ` | 3 | GemSmasher, ProfileMgr, network/net/NetSession |
| `0x82667fc8` | 16 | band3/meta_band/CharProvider | `?GetUser@OvershellProfileProvider@@QBAPAV...` | 1 | OvershellSlot |
| `0x826fcce8` | 100 | system/synth/Synth | `??1?$ObjPtr@VSynthSample@@@@UAA@XZ` | 1 | CharMeshHide, JoypadMsgs, MidiInstrument |
| `0x82741340` | 8 | system/rnddx9/Mat | `?SetWaitForSplash@Splash@@QAAX_N@Z` | 1 | Splash |
| `0x8276f6f8` | 8 | system/beatmatch/BeatMaster | `?SetAutoVocals@PlayerTrackConfigList@@QAA...` | 1 | system/beatmatch/PlayerTrackConfigList |
| `0x82774148` | 104 | BandSongMgr | `??_GMeasureMap@@QAAPAXI@Z` | 8 | MeshAnim, MidiReader, SongData |
| `0x82782950` | 8 | VocalNoteList | `?ShaderPoolAlloc@RndShaderMgr@@IAAXH@Z` | 1 | system/rndobj/ShaderMgr |
| `0x827b74d0` | 96 | StorePanel | `??$__destroy_range_aux@V?$reverse_iterato...` | 1 | StandardStream |
| `0x82bb34a8` | 60 | band3/bandtrack/Gem | `??$_Copy_Construct@V?$vector@GV?$StlNodeA...` | 2 | Mesh |
| `0x82c30b58` | 8 | sharedbook | `OggRealloc` | 1 | VorbisMem |

### NO-BASE-OBJ

| address | size | retail unit | census spelling (first) | pairs | where our spellings compile |
|---|---:|---|---|---:|---|
| `0x822d65e8` | 116 | auto_03_822D6568_text | `??1?$ObjPtr@VRndTransAnim@@@@UAA@XZ` | 1 | no obj |
| `0x822d6730` | 116 | auto_03_822D6700_text | `??1?$ObjPtr@VRndPartLauncher@@@@UAA@XZ` | 1 | no obj |
| `0x822d6850` | 116 | auto_03_822D6700_text | `??1?$ObjPtr@VRndText@@@@UAA@XZ` | 1 | no obj |
| `0x822d6b00` | 232 | auto_03_822D6B00_text | `?Load@?$ObjPtr@VRndTransAnim@@@@QAA_NAAVB...` | 1 | no obj |
| `0x822d6cd0` | 232 | auto_03_822D6CD0_text | `?Load@?$ObjPtr@VRndPropAnim@@@@QAA_NAAVBi...` | 1 | no obj |
| `0x822d6db8` | 232 | auto_03_822D6CD0_text | `?Load@?$ObjPtr@VRndText@@@@QAA_NAAVBinStr...` | 1 | no obj |
| `0x823ea5a8` | 12 | auto_03_823EA4E8_text | `?GetAward@AccomplishmentGroup@@QBA?AVSymb...` | 3 | no obj |
| `0x82956038` | 8 | xdk/xgraphics/ucode/ssm/interface/ssmdevice | `?Title@BandSongMetadata@@QBAPBDXZ` | 2 | no obj |
| `0x82956040` | 8 | xdk/xgraphics/ucode/ssm/interface/ssmdevice | `?GetNumCompleted@AccomplishmentProgress@@...` | 2 | no obj |
| `0x82956048` | 8 | xdk/xgraphics/ucode/ssm/interface/ssmdevice | `?GetGamerpicReward@Accomplishment@@QBAHXZ` | 1 | no obj |
| `0x82956050` | 8 | xdk/xgraphics/ucode/ssm/interface/ssmdevice | `?GetAvatarAssetReward@Accomplishment@@QBAHXZ` | 1 | no obj |
| `0x82bac148` | 316 | auto_03_82BAC148_text | `??0Gem@@QAA@ABVGameGem@@IMM_NHH1@Z` | 1 | no obj |
| `0x82bd4eb0` | 8 | auto_03_82BD4B94_text | `?LengthMs@BandSongMetadata@@QBAHXZ` | 1 | no obj |
| `0x82bf5398` | 8 | auto_03_82BF49F4_text | `?GetGemManager@GemTrack@@QAAPAVGemManager...` | 2 | no obj |
| `0x82c1a608` | 8 | auto_03_82C19F4C_text | `?GetCurrentStreak@Stats@@QBAHXZ` | 3 | no obj |

What could move some of them (not done here; each is a pin or source change, not a name):

- `0x82bac148` (`Gem::Gem`, 316 B) and the six `ObjPtr<T>` dtor/`Load` bodies at
  0x822d65e8-0x822d6db8 sit in `auto_*` spans. They pair only if a pin gives those
  spans to a unit that compiles them.
- The four `ssmdevice` addresses are HMX 8-byte getters that retail's ICF folded
  into an XDK unit with no source.
- `0x823f1898` (MemMgr) and `0x825459e0` (CharServoBone) are 8- and 28-byte bodies
  folded across unrelated classes. The unit-local twin search found no
  same-size, chase-PROVEN function in those objs.

## Gates

`tools/native_build_gate.sh` in `~/tmp/wt-w16um` at `cc9562c3d`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

No `src/` file changed. The lane touched the map, `symbol_aliases.json`,
`alias_callee_names.json`, one tool and this doc.

## Not done

- No pin moves for the NO-BASE-OBJ / NOT-IN-UNIT-OBJ addresses.
- No fix for the three exposed map defects. They need vtable or tree-type
  identification, which W16-UH already recorded.
- The CRT ctor at 0x8282a080 is not identified.
