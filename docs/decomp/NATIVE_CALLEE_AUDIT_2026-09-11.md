# Native-linked wrong-callee audit — lane W3-A, 2026-09-11

Branch `w3-native-callee`, worktree `~/tmp/wt-w3-a`, based on main `814e3a60`.
Ruler: `name_check` (graded), read from `report.json` `provenance.diff_config`.
Baseline at the lane's first full build: `matched_functions` 42,439 /
`matched_code` 3,821,092 B / 37.29366 % / `total_code` 10,245,956 /
`total_functions` 69,219.

**Goal.** The `mpn` ruler is argument-blind: a caller that names the wrong
callee scores 100 before and after the fix. Hunt that class in the TUs the
native executables actually link, adjudicate every charged relocation-name
site on RETAIL BYTES, fix source or map, refuse folds.

## 1. The native-linked TU set (471 TUs)

`tools/native_linked_tus.py` (new) = for each of the 18 `rb3-*` targets, the
compiled primary sources from `native/build/build.ninja`
(`scatter_audit.target_sources`) ∪ the compiler-truth scatter closure
(`scatter_emitted_truth.deps_of`, `clang++ -M` with each target's own
defines). 3,629 preprocess jobs, 0 failures. Union = **471 `src/` TUs**
(`docs/decomp/native_linked_tus_2026-09-11.txt`). Per target: rb3-milo /
rb3-render 428, rb3-harmony / rb3-vocal2 184, … , **rb3-frame 0** (it links
only `native/src/main_frame.cpp` + `libmilo-engine.a`).

⚠ The brief's self-validation probe `src/system/synth_xbox/Voice.cpp` is
**NOT native-linked**: `synth_xbox` has 0 mentions in `native/build/build.ninja`
and `native/CMakeLists.txt` lists "synth_xbox x3" among excluded sources; the
native targets link `system/synth/*`. The tool's probe list uses
`system/synth/Synth.cpp` instead and exits 3 if any probe is absent. Tested
literally, as the brief asked.

416 of the 471 TUs have a `report.json` unit; the other 55 are compiled but
unpinned (mostly `hamobj/*`, `gesture/*`, DC3-only code with no RB3 retail
counterpart) — not audited.

## 2. Population (provenance: graded ruler, `crossing_worklist.diff_many`, cache `~/tmp/crossing_worklist/diffs`, 2,137 named sub-100 rows diffed, 0 misses)

| population | rows | bytes |
|---|---:|---:|
| named rows in native units with `fuzzy < 100` | 2,137 | 670,628 |
| **A**: `mpn == 100`, `fuzzy < 100`, named | 90 | 38,052 |
| … of which carry a relocation-NAME charge | 7 | — |
| **B**: `mpn < 100`, ≤ 3 charged sites, ≥ 1 relocation-NAME charge | 1,209 | 251,108 |
| distinct (retail callee, our callee) pairs over A ∪ B | 1,065 | — |

(The 1,945 placeholder-named rows in A are not adjudicable and are already
forgiven by `name_check`.)

Pair-level screen (`~/tmp/w3a/pair_census.py`): a pair whose two names are
BOTH map-resident at DIFFERENT retail addresses cannot be an ICF fold unless
the map itself is wrong — that is the hunting class:

| class | pairs | sites | caller bytes |
|---|---:|---:|---:|
| BOTH_NAMED_DISTINCT, different method | 132 | 176 | 19,064 |
| BOTH_NAMED_DISTINCT, same method (template siblings / sig text) | 167 | 234 | 46,532 |
| OURS_UNNAMED (fold possible; template siblings dominate) | 725 | 961 | 201,900 |
| NEITHER / RETAIL_UNNAMED | 38 | 102 | 33,512 |

Every BOTH_NAMED_DISTINCT ∧ different-method pair was read; the ones below
are the ones that were not trivially "4-byte `operator delete` stub" or
"`_bijection_arbitrary`" noise.

## 3. Per-row adjudication

Verdict key: MAP_WRONG = retail body proves the map name wrong, our source
right; SOURCE_WRONG = our source diverges from retail bytes; FOLD = ICF
survivor name, refused (no alias installed); UNDECIDED = evidence
insufficient.

| row(s) | unit | retail callee (map name) | our callee | verdict | evidence (retail bytes) | predicted Δ | measured Δ |
|---|---|---|---|---|---|---|---|
| `CommonPhraseCapturer::HandlePhraseNote/LocalFail/LocalHitLastGem`, `GemManager::IsSpotlightGem` | game | `SongDB::GetNumOverdrivePhrases` @0x82684fb8 | `SongDB::IsUnisonPhrase` | **MAP_WRONG** (names swapped) | 0x82684fb8 tail-calls `fn_8278B638` = `lbz elem[idx].+6` (bool per phrase); 0x82684fc8 tail-calls `fn_8278B678` = loop counting phrases with `mask & (1<<track)` (int count) | +4 fns / +1,300 B (LocalHitLastGem uncertain) | **+4 fns** (LocalHitLastGem on mpn only) / +1,088 B |
| `Player::FinalizeStats` | game | `SongDB::IsUnisonPhrase` | `GetNumOverdrivePhrases` | MAP_WRONG (same swap) | as above | +1 / +468 | **+1 / +468** |
| `??0VocalNote`, `??4VocalNote` rows; `SongParser::EndVocalNote`; `vector<VocalNote>::_M_fill_insert_aux` | VocalTrainerPanel / SongParser / VocalNoteList | `??0VocalNote` @0x826b8448 | `??4VocalNote` | **MAP_WRONG** (swapped) | 0x826b8448 copies its `String` member with `String::operator=` (`fn_827BE0E0`); 0x82685278 with `String::String(const&)` (`fn_827BE608`) | +2 / +392 (EndVocalNote +360, fill_insert +392 uncertain) | **+2 / +392**; EndVocalNote and fill_insert did not cross (other charges) |
| 15 `Crowd` STLport template rows (`swap`, `__push_heap`, `_M_erase`, `vector::operator=`, …) + `??4Char3D` | Crowd | `??0Char3D` @0x824e2038 / `??4Char3D` @0x824e1e08 | the other | **MAP_WRONG** (swapped) | 0x824e1e08 copies the `vector<Vector3>` member with `vector::vector(const&)` and is what `_Copy_Construct<Char3D>` calls ⇒ copy ctor; 0x824e2038 uses `vector::operator=` | +16 / +2,340 | **+15 / +2,256** (`??4Char3D` 84 B did not cross — its twin row reads 15 %, body differs beyond the callee) |
| `SongParser::SongParser`, `VocalNoteList::VocalNoteList`, `CheatsInit` + 14 other retail callers | beatmatch / game | `??__EgNotifies` @0x8250fef8 | `SystemConfig()` | **MAP_WRONG + PIN WRONG** | 0x8250fef8 is `lis/lwz r3,lbl_82CC9990; blr`, an accessor, NOT a dynamic initializer; it sits at `SetUsingCD 0x8250fee8 → [it] → SystemConfig(Symbol) 0x8250ff08 → (Symbol,Symbol) → … → SystemLocale → SystemTitles`, i.e. System.cpp's source order; all 17 retail callers chain into `DataArray::FindArray/FindData`. The address the map called `SystemConfig` (0x82569e78) sits between `LessonMgr::GetCompletedCountFromTrainer` and `AssetMgr::GetTypeFromName`, and all 36 of its callers pass its result as `this` to AssetMgr methods ⇒ it is `AssetMgr::GetAssetMgr()`. | +3 / +556 (CheatsInit +264 uncertain) | **in-scope exactly +3**: Debug +1 (`SystemConfig`), AssetMgr +1 (`GetAssetMgr`), System −1, VocalNoteList +1, AccomplishmentManager +1; CheatsInit did not cross. Whole-binary **+22 / +5,316 B** — the other +19 are non-native meta_band asset-UI callers (below) |
| 36 asset-UI callers (`ClosetMgr`, `AssetProvider`, `CustomizePanel`, `Award`, `AccomplishmentManager::EquipAward`, …) | meta_band | `SystemConfig()` @0x82569e78 | `AssetMgr::GetAssetMgr` | MAP_WRONG (same) | as above | EquipAward +1 / +132; others out of scope (>3 charges) | EquipAward +1/+132 as predicted; PLUS 18 unpriced meta_band rows crossed (CustomizePanel +2, AssetProvider +2, ClosetMgr +2, CurrentOutfitProvider +2, NewAwardPanel +2, …), 3 units reached 100 % (EyebrowsProvider, FaceHairProvider, InstrumentFinishProvider) |
| `ColorKeys::Load/Save`, `QuatKeys::Load`, `FloatKeys::Save/Load`, `BoolKeys::Load`, `Vector3Keys::Load` | PropAnim | `PropKeys::Save` where we call `PropKeys::Load` (and v.v.) | — | **MAP_WRONG on 7 rows** | `PropKeys::Save` @0x82423320 (writes: `Write(&x,4)`, `<< ObjPtr`, `<< Symbol`, 1-byte bool) and `PropKeys::Load` @0x82423af8 (reads with `gRev` gating) are correctly named; each `*Keys::Load/Save` row's identity is its `<<`/`>>` callee: aa00→`<< Key<float>` (FloatKeys::Save), aeb8→`<< Key<ObjectStage>` (ObjectKeys::Save), b7a8→`<< Key<Vector3>` (Vector3Keys::Save), e3e8→`>> Key<float>` (FloatKeys::Load), e438→`>> Key<bool>` (BoolKeys::Load), e488→`>> Key<Vector3>` (Vector3Keys::Load), e4d8→`>> Key<Color>` (ColorKeys::Load). Retail has 6 Saves + 6 Loads for 7 Keys types: `Key<Color>` and `Key<Quat>` are both 4 floats + frame, so their `<<`/`>>` fold and `ColorKeys::Save ≡ QuatKeys::Save`, `QuatKeys::Load ≡ ColorKeys::Load` fold transitively (refused, not aliased). | +7 / +560 | **+7 / +560** (PropAnim 228→235), exact |
| `CharClipSet::SortGroups` | char | `CharClipGroup::Randomize` @0x8238ea60 | `CharClipGroup::Sort` | **MAP_WRONG** (row is `RandomizeGroups`) | 0x8238ea60 is a Fisher-Yates loop (`RandomInt` + `swap<ObjOwnerPtr<CharClip>>`) = Randomize; 0x8238fee8 is `sort(begin,end,Alphabetically)` = Sort; rb3-Wii and our source both define `RandomizeGroups()` as the identical `ObjDirItr` loop calling `Randomize()`. The retail row named `SortGroups` (0x823d1c78) calls Randomize ⇒ it is `RandomizeGroups`; the unnamed `fn_823D1CC0` 72 B later calls Sort ⇒ the real `SortGroups`. | +2 / +144 (SortGroups has 1 other charge) | **+1 / +612**: `CharClipSet::Handle` (612 B) crossed via its `randomize_groups` action site; `SortGroups`/`RandomizeGroups` each keep one unrelated charge (99.72) |
| `DataBasename` | DataFunc | `DataNode::Int(const DataArray*)` @0x8274b0f8 | `DataNode::Str(const DataArray*)` | **MAP_WRONG** (row is `DataLocalizeSeparatedInt`) + **SOURCE (signature)** | the retail row @0x82760158 calls the `Evaluate(); return value` accessor (alias group `DataNodeAssertOnlyAccessor` = Int ≡ Array) then `fn_827C97C8` = `?LocalizeSeparatedInt@@YAPBDH@Z`, then `DataNode(const char*)` ⇒ it is `DataLocalizeSeparatedInt`. Instrument check: every 100 % DataFunc row using `Int/Array` in source calls 0x8274b0f8 and every one using `Str` calls 0x8274b000 — zero exceptions. Our `DataLocalizeSeparatedInt` calls the DC3-newer 2-arg `LocalizeSeparatedInt(int, Locale&)`; rb3-Wii and retail use the 1-arg form. `FileGetBase` is not in the map, so the real DataBasename row could not be located. | +1 / +72; DataBasename row unpairs (Δ0, was 99.4) | **+1 / +72**, DataBasename unpaired at Δ0, exact |
| `??_GCharUpperTwist` | CharUpperTwist | `~RndMovie` @0x824787c8 | `~CharUpperTwist` | **MAP_WRONG + PIN WRONG** | the row the map calls `??_GCharUpperTwist` (0x82479100) sits right after Movie.cpp's last block and calls `~RndMovie` then `~Object` ⇒ it is `??_GRndMovie`; the row the map calls `??_GRndPostProcMgr` (0x8232ac50, PostProcMgr.cpp's ONLY `.text` block, physically after `DialogDisplay::Init`) calls `~CharUpperTwist` @0x8232a2c0 (inside CharUpperTwist's own block, our dtor pairs at 99.7) ⇒ it is `??_GCharUpperTwist`. | +2 / +136; PostProcMgr.cpp unit vanishes (single-block unit) | **+1 / +12 B**, pairable units 1728→1727 as predicted: the 12-B `??_ECharUpperTwist$2…` vector-deleting thunk crossed; `??_GCharUpperTwist` 99.41→99.71 and `??_GRndMovie` newly paired at 99.71, each with one residual relocation charge (the over-prediction). First split failed the split-input contract (`.pdata` re-derived) — committed as dtk wrote it. |
| `??__FsFrames` (handed over by ATEXIT-RULER) | SkeletonClip | `~ObjDirPtr<ObjectDir>` @0x822709d8 | `~vector<RecordedFrame>` | **MAP_WRONG (name fabricated by transfer); owner UNDECIDED** | retail 0x82c440a0 = `__ehvec_dtor(lbl_82C6C660, size 0xC, count 4, dtor fn_822709D8)`; fn_822709D8 is a real virtual dtor (two vtable stores + `SetObj(0)`-style call, 87 callers), not a vector dtor. It is the ONLY `(0xC, 4)` ehvec thunk in the binary — there is NO thunk destroying four `vector<RecordedFrame>`, so retail has no such static. No version of SkeletonClip (ours = DC3) declares an `ObjDirPtr<ObjectDir>[4]`; rb3-Wii has no SkeletonClip (no Kinect). The 0x800 B "SkeletonClip" pin at 0x82C438BC–0x82C440BC is a grab-bag of unrelated `.text$yd` thunks (NgPostProc `BloomTextures` ctors, XAudio `sm_RegistrationProperties` memcpy initialisers, 14 `DataArray::Release` atexit thunks, a `CriticalSection`) — name-driven from this one label. | drop the name: Δ0 (row is 98.57) | 0 B; the anonymous row re-pairs under placeholder forgiveness (`mpn` 100, `masked_equal` +1, fuzzy unchanged) |
| `Char3D` `_Copy_Construct` vs our `_Param_Construct`; `list<CharClip*>::insert` vs `list<RndDrawable*>::insert` (13) / `<Symbol>` (11) / `<RndPollable*>` (8) / `<RndMat*>` (8); `??1ObjPtrList<CharInterest>` vs `<Object>` (11); `list<BitmapOverride>::erase/clear` → `~EventCall` vs `~BitmapOverride` (72 B twins); `Keys<Vector3>::Add` vs `Keys<Color>::Add`; `find@FixedString` vs `find@String`; `??_G Key<vector<Vector3>>` vs `??_GDataNode`; the 4-byte `X::operator delete` ↔ `BinStream::operator delete` / `MemFree` family | many | template siblings / trivial bodies | — | **FOLD — refused** | same-shape bodies at one surviving address; `s1_fold_family --pairs` returns UNDECIDED/UNRESOLVED-RELOC on the RTTI-discriminated ones (probed on `ObjDirItr<BandList>::Advance` vs `<CharClipGroup>`). No alias installed by this lane. | 0 | 0 |
| `GemTrack::FillHit` / `Ignore` / `ST::ExtractBodyPart` @0x82b93f40..58 | GemPlayer | 8-byte stubs | — | FOLD (already in `_bijection_arbitrary`) | — | 0 | 0 |
| `$4PPPPPPPM@A@` adjustor thunks: `BandList::PreLoad→Load`, `WorldReflection::SetType→Load`, `CharIKRod::Copy→SetType`, `SyncProperty→Copy` | ui / world / char | thunk names shifted by one vtable slot | — | UNDECIDED — handoff | consistent off-by-one in the map's thunk naming; not a native runtime bug (native compiles our headers consistently) | — | not attempted |
| `MemFree` ↔ `RndLight::operator delete` etc. (inline-policy) | Lit, Console, … | retail inlined `operator delete` → `MemFree` | our out-of-line `NEW_OVERLOAD` delete | not a behavioural divergence | same callee one inline level down | — | not attempted |

## 4. What this lane did NOT do, and why

- Did not alias any fold. Every template-sibling pair (the majority of the
  1,065) is a fold *candidate*; `s1_fold_family` refuses the RTTI-discriminated
  ones and the lane installed nothing. The 7.9 pp alias mechanism is not
  touched.
- Did not chase the 33 remaining `GetAssetMgr` callers or the 14 non-native
  `SystemConfig()` callers past the rename: their other charges are unrelated.
- Did not re-home the SkeletonClip thunk band or the Debug/System split
  boundary (the `SystemConfig(Symbol…)` overloads live in Debug.cpp in both
  retail and our tree, so the boundary is only cosmetically wrong).
- Did not audit the 67-row `??_G` census beyond the two rows fixed
  (`~/tmp/w3a/scalar_deleting_dtor_census.txt`, copied below): most entries
  are legitimate inlined-derived-dtor cases; the ones whose callee is NOT a
  base class (`??_GDxMesh → ??_DMeterDisplay`, `??_GNetCacheMgrXbox →
  ~AccomplishmentConditional`, `??_GLocalePanel → ~TourChar`,
  `??_GTrainerChallenge → ~SortNode`, `??_GContentLoadingPanel →
  ~CreditsPanel`, `??_GQuestFilterPanel → ~TexLoadPanel`,
  `??_GEnvelopeGenerator → ~CXAPOParametersBase`) are the same disease and
  belong to a map lane.
- Did not touch the adjustor-thunk slot shift.
- No genuine SOURCE_WRONG behavioural bug was found in the native-linked
  set beyond the `LocalizeSeparatedInt` signature (DC3-newer overload). Every
  other divergence adjudicated on retail bytes resolved to the MAP being wrong
  and our source being right — which is itself the finding: on this stratum
  the map, not the source, is the dominant error source.

## 5. Handoffs

1. `??_G` map census (67 rows) — adjudicate by base-class relation; the
   non-base ones are misnamed rows.
2. Adjustor-thunk (`$4PPPPPPPM@A@`) off-by-one naming for BandList,
   WorldReflection, CharIKRod, DialogDisplay families.
3. The SkeletonClip `.text` band 0x82C438BC–0x82C440BC is not SkeletonClip's;
   re-home to `auto_*` or per-owner.
4. `FileGetBase` is unnamed in the map; the real `DataBasename` row is
   therefore unlocatable by callee and stays unpaired.
5. The System.cpp/Debug.cpp split boundary (0x825100C8) is ~0x1D0 too high
   for retail's layout; harmless today because Debug.cpp defines those
   functions in our tree too.

## 6. Measurement ledger (ab_measure, `--from-dirty`, both legs at a split fixed point every run)

| run | kinds | pre-registered | measured Δmatched / Δcode | notes |
|---|---|---|---|---|
| P1 swaps (SongDB, VocalNote, Char3D) | map | +22 / +4,032 | **+22 / +3,948** | ALIAS_SUSPECT fired (map-only); evidence is retail bodies of two distinct named addresses |
| P2 SystemConfig / GetAssetMgr | map+source+splits | +3 / +556 (native scope) | **+22 / +5,316** | in-scope exactly +3; +19 non-native meta_band callers; 3 units → 100 % |
| P3 PropKeys | map | +7 / +560 | **+7 / +560** | exact |
| P4+P5+P7 | map+source | +2..3 / +144..216 | **+3 / +684** (masked_equal +1) | CharClipSet::Handle 612 B crossed instead of SortGroups; DataLocalizeSeparatedInt +72; SkeletonClip thunk re-pairs anonymously |
| P6 `??_G` | map+splits | +2 / +136 | **+1 / +12** | first attempt REFUSED (split rewrote `.pdata`), re-run from the fixed point |
| **total** | | | **+55 fns / +10,520 B** | 42,439 → 42,494 matched; 37.29366 % → 37.396336 % |

## 7. Appendix — `??_G` census (first callee is another class's dtor)

```
??_G rows whose first callee is ANOTHER class dtor: 67 (includes legitimate inlined-derived-dtor cases; adjudicate by base-class relation)
  0x8232ac50 ??_GRndPostProcMgr@@UAAPAXI@Z -> ??1CharUpperTwist@@MAA@XZ
  0x823cc500 ??_GCharDriverMidi@@UAAPAXI@Z -> ??1CharDriver@@UAA@XZ
  0x823d9518 ??_GFileMergerOrganizerLoader@@UAAPAXI@Z -> ??1Loader@@UAA@XZ
  0x823f7180 ??_GHiResScreen@@UAAPAXI@Z -> ??1String@@UAA@XZ
  0x8242aaf0 ??_GFloatKeys@@UAAPAXI@Z -> ??1BoolKeys@@UAA@XZ
  0x8242b338 ??_GBoolKeys@@UAAPAXI@Z -> ??1FloatKeys@@UAA@XZ
  0x8245dcc0 ??_GRndWind@@UAAPAXI@Z -> ??1NetCacheMgrXbox@@UAA@XZ
  0x82479100 ??_GCharUpperTwist@@MAAPAXI@Z -> ??1RndMovie@@UAA@XZ
  0x824816a8 ??_GRndScreenMask@@UAAPAXI@Z -> ??_DRndMultiMeshProxy@@QAAXXZ
  0x8248b930 ??_GMeterDisplay@@UAAPAXI@Z -> ??_DRndTexBlender@@QAAXXZ
  0x824e9ed0 ??_GRemoteBandUser@@UAAPAXI@Z -> ??_DSpotlightEnder@@QAAXXZ
  0x82535780 ??_GAsyncFileWin@@UAAPAXI@Z -> ??1AsyncFileHolmes@@UAA@XZ
  0x825374e8 ??_GNetPushScreenMsg@@UAAPAXI@Z -> ??1StartTransitionMsg@@UAA@XZ
  0x8253b348 ??_GSetlistProvider@@UAAPAXI@Z -> ??_GSetUserDifficultyMsg@@UAAPAXI@Z
  0x8258b420 ??_GStandIn@@UAAPAXI@Z -> ??1AccomplishmentSongConditional@@UAA@XZ
  0x82596590 ??_GInternalSavedSetlist@@UAAPAXI@Z -> ??1SavedSetlist@@UAA@XZ
  0x8259b860 ??_GNetSyncScreenMsg@@UAAPAXI@Z -> ??1StartTransitionMsg@@UAA@XZ
  0x825d00c0 ??_GBattleSavedSetlist@@UAAPAXI@Z -> ??1NetSavedSetlist@@UAA@XZ
  0x825e5900 ??_GAccomplishmentSetlist@@UAAPAXI@Z -> ??1Accomplishment@@UAA@XZ
  0x825e5bc0 ??_GAccomplishmentOneShot@@UAAPAXI@Z -> ??1AccomplishmentTrainerConditional@@UAA@XZ
  0x825ed890 ??_GClosetPanel@@UAAPAXI@Z -> ??1DeJitterPanel@@UAA@XZ
  0x825f4598 ??_GTourDescPanel@@UAAPAXI@Z -> ??1TexLoadPanel@@UAA@XZ
  0x825fb388 ??_GAccomplishmentPanel@@UAAPAXI@Z -> ??1TexLoadPanel@@UAA@XZ
  0x82607dc8 ??_GBandStoreShortcutProvider@@UAAPAXI@Z -> ??1DataProvider@@UAA@XZ
  0x82613350 ??_GContentDeletePanel@@UAAPAXI@Z -> ??1UIPanel@@UAA@XZ
  0x82614098 ??_GCreditsPanel@@EAAPAXI@Z -> ??1ContentLoadingPanel@@UAA@XZ
  0x8261ef60 ??_GGameTimePanel@@UAAPAXI@Z -> ??1UIPanel@@UAA@XZ
  0x8261f8e0 ??_GBackdropPanel@@UAAPAXI@Z -> ??1DeJitterPanel@@UAA@XZ
  0x82627f58 ??_GNewAwardPanel@@UAAPAXI@Z -> ??1TexLoadPanel@@UAA@XZ
  0x8262c280 ??_GStickerProvider@@UAAPAXI@Z -> ??1LayerProvider@@UAA@XZ
  0x8262c530 ??_GLayerProvider@@UAAPAXI@Z -> ??1StickerProvider@@UAA@XZ
  0x8262dea0 ??_GPatchSelectPanel@@UAAPAXI@Z -> ??1UIPanel@@UAA@XZ
  0x8262f190 ??_GParentalControlPanel@@UAAPAXI@Z -> ??1UIPanel@@UAA@XZ
  0x82632920 ??_GSelectDifficultyPanel@@UAAPAXI@Z -> ??1UIPanel@@UAA@XZ
  0x82637898 ??_GSongSelectPanel@@UAAPAXI@Z -> ??1HeldButtonPanel@@UAA@XZ
  0x82662ed0 ??_GSubheaderSortNode@@UAAPAXI@Z -> ??1HeaderSortNode@@UAA@XZ
  0x82669f20 ??_GAccomplishmentSongConditional@@UAAPAXI@Z -> ??1AccomplishmentTrainerConditional@@UAA@XZ
  0x8266b088 ??_GAccomplishmentConditional@@UAAPAXI@Z -> ??1AccomplishmentTrainerConditional@@UAA@XZ
  0x82673398 ??_GPlayerLeaderboard@@UAAPAXI@Z -> ??1Leaderboard@@UAA@XZ
  0x82675900 ??_GTrainerChallenge@@UAAPAXI@Z -> ??1SortNode@@UAA@XZ
  0x826930e8 ??_GFillsHitStatMemberTracker@@UAAPAXI@Z -> ??1StatMemberTracker@@UAA@XZ
  0x826d06d8 ??_GStatMemberTracker@@UAAPAXI@Z -> ??1Tracker@@UAA@XZ
  0x826e0290 ??_GScoreTracker@@UAAPAXI@Z -> ??1Tracker@@UAA@XZ
  0x826e1ed0 ??_GOverdriveTimeTracker@@UAAPAXI@Z -> ??1Tracker@@UAA@XZ
  0x826e2438 ??_GAccuracyTracker@@UAAPAXI@Z -> ??1Tracker@@UAA@XZ
  0x8270a718 ??_GParallelGroupSeqInst@@UAAPAXI@Z -> ??1GroupSeqInst@@UAA@XZ
  0x8270a778 ??_GRandomGroupSeqInst@@UAAPAXI@Z -> ??1GroupSeqInst@@UAA@XZ
  0x8270aeb8 ??_GSerialGroupSeqInst@@UAAPAXI@Z -> ??1GroupSeqInst@@UAA@XZ
  0x8271fbe0 ??_GFxSendReverb@@UAAPAXI@Z -> ??1FxSend@@UAA@XZ
  0x82720f10 ??_GFxSendDelay@@UAAPAXI@Z -> ??1FxSend@@UAA@XZ
  0x82721c38 ??_GFxSendCompress@@UAAPAXI@Z -> ??1FxSend@@UAA@XZ
  0x827226a8 ??_GFxSendEQ@@UAAPAXI@Z -> ??1FxSend@@UAA@XZ
  0x82738ab8 ??_GDxMesh@@UAAPAXI@Z -> ??_DMeterDisplay@@QAAXXZ
  0x8276e3e8 ??_GBeatMasterLoader@@UAAPAXI@Z -> ??1Loader@@UAA@XZ
  0x82780188 ??_GDrumTrackWatcherImpl@@UAAPAXI@Z -> ??1TrackWatcherImpl@@UAA@XZ
  0x8279ef80 ??_GRealGuitarTrackWatcherImpl@@UAAPAXI@Z -> ??1BaseGuitarTrackWatcherImpl@@UAA@XZ
  0x827a0710 ??_GJoypadTrackWatcherImpl@@UAAPAXI@Z -> ??1TrackWatcherImpl@@UAA@XZ
  0x827a1090 ??_GGuitarTrackWatcherImpl@@UAAPAXI@Z -> ??1BaseGuitarTrackWatcherImpl@@UAA@XZ
  0x827a1638 ??_GBaseGuitarTrackWatcherImpl@@UAAPAXI@Z -> ??1TrackWatcherImpl@@UAA@XZ
  0x827a3350 ??_GDataArraySongInfo@@UAAPAXI@Z -> ??1SongInfoCopy@@UAA@XZ
  0x827af108 ??_GContentLoadingPanel@@UAAPAXI@Z -> ??1CreditsPanel@@EAA@XZ
  0x827d00a8 ??_GNetLoaderStub@@UAAPAXI@Z -> ??1NetLoaderXbox@@UAA@XZ
  0x827d7560 ??_GNetCacheMgrXbox@@UAAPAXI@Z -> ??1AccomplishmentConditional@@UAA@XZ
  0x82b6d558 ??_GEnvelopeGenerator@@UAAPAXI@Z -> ??1CXAPOParametersBase@@UAA@XZ
  0x82b7b0b0 ??_GQuestFilterPanel@@UAAPAXI@Z -> ??1TexLoadPanel@@UAA@XZ
  0x82b801d8 ??_GLocalePanel@@UAAPAXI@Z -> ??1TourChar@@UAA@XZ
  0x82c16e88 ??_GCAPOXHV@@UAAPAXI@Z -> ??1CXAPOParametersBase@@UAA@XZ
```
