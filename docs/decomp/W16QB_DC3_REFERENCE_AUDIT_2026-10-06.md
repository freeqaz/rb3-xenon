# W16-QB: what DC3 can close by reference, beyond function bodies (2026-10-06)

Lane W16-QB. Brief: audit `../dc3-decomp` (current source and its named map
`orig/373307D9/ham_xbox_r.map`) for anything that could close open rb3-xenon
rows by reference **other than** `src/system` function bodies (W16-QA has
those outside `rndobj/` and `char/`, W16-PU has `rndobj/` and `char/`):
header differences, anonymous rows DC3's map can name, shared template and STL
instantiations. Price a sample of each category with `tools/ab_measure.py`,
fix what is fixable along the way, and rank the categories.

Worktree `~/tmp/wt-w16qb`, branch `w16-qb`, based on main `90a3e94d2`. Every
A/B below is `ab_measure.py --from-dirty`, `name_check` ruler, both legs at a
split fixed point, leg A = the previous commit. Leg A of the first A/B:
`matched_functions 53,528` / `code% 57.967995`; leg B of the last kept one:
53,536 / 57.975770 (+8 fns, chained through every leg).

## 1. Ranking

"Measured" means an `ab_measure` A/B in this lane. Native relevance uses the
CAMPAIGN_STATE_2026-10-06 rings: IN-CORE / IN-SOON / IN-RB3ENG are the
in-scope rings, VIA-DC3 is code the native build takes from DC3, OUT-* is out
of scope. A map or pin change never changes code, so the metric-only
categories carry no native value beyond the identification itself.

| # | category | surface found | measured in this lane | native relevance | verdict |
|---|---|---|---|---|---|
| 1 | **Mis-homed instantiation pins** (row pinned in a TU whose obj lacks the body; the neighbouring TU's obj has a byte-identical one) | 45 rows / 2,704 B flank-matched; 9 done, **36 rows / 2,232 B left** (456 B in scope) | **+5 fns / +368 B** over two batches (+4/+192, +1/+176). Two naming mistakes measured and withdrawn: **−1/−5,584 B** and **−1/−648 B** | metric only | **Fund, small.** Best B per row of any category, but every name must pass the fold-bait check in §3.2 |
| 2 | **DC3-shaped phantom names on template instantiations** (map spells the instantiation with a DC3/wrong type, so no base obj defines it) | 50 named rows / 3,408 B unpaired in their own obj; 3 fixed, **47 rows / 2,980 B left** (1,420 B in scope) | **+3 fns / +428 B** (VocalPlayer's `remove_if` / `remove_copy_if` / `__find_if` spelled with Dir's `FilePath`) | metric only | **Fund, small.** Same tooling as #1; fold into one lane |
| 3 | **Anonymous rows DC3's map can name** (unique masked-hash twin in DC3's retail objs) | 61 unique anon twins / 2,836 B; after removing EH funclets, the init region and 8 B fold bait, **~10 real candidates / ~800 B** | **Δ0 fns / Δ0 B** (FastSort<3>: row 0 → 79.7 fuzzy, kept) | metric only; the names help body lanes | **Do not fund as a byte lever.** A DC3 hash is a *shape* witness, not a *type* witness (§3.3) |
| 4 | **Header differences** (inline policy, member layout, vtable order) | 668 headers differ textually; signals: vtable-slot 16 rows / 10,224 B (368 B in scope), uniform member delta 16 / 14,664 B (3,088 B in scope), callee in/out-of-line 30 rows / 24,552 B (5,916 B in scope) | Control: adopt DC3's inline `DataArray::Release` → **Δ0 fns / Δ0 B** with 1,034 TUs recompiled (prediction was negative) | **High where real**, because these change code. But DC3 is newer, so its headers mostly drift *away* from RB3 | **Do not fund a DC3-header sweep.** Fund per row only where retail bytes, not DC3, say our header is wrong (§3.4) |
| 5 | **Shared STL / template instantiations where retail == DC3's retail body and DC3-decomp matches 100%** | 15 same-name rows / 2,508 B (244 B in scope); 8 differently-named rows / 688 B; 31 same-name rows where DC3 is also <100 / 10,476 B | not measured here: these are bodies, W16-QA/W16-PU territory | VIA-DC3 mostly | **Hand to W16-QA/PU** (list §3.5) |
| 6 | **Static-init region `??__E` rows** (`0x82C3F000`+) | 60 anon rows / 4,508 B, **42 / 3,384 B IN-CORE** | not measurable by naming: DC3's names here are false (§3.6) | IN-CORE | **Fund as identification**: name by relocation targets, and add the missing `beatmatch/Output.cpp` TU |
| 7 | **DC3 map as fold witness for name charges** (DC3 places our name and retail's name at one VA) | 6 pairs / 7 rows / 796 B | not measured: W16-PH's rule needs a *retail* type witness; DC3's map cannot type an RB3 call | mixed | **Do not fund.** List kept for alias lanes (§3.7) |
| 8 | **STLport** | no drift vs dc3-decomp's `stlport/` | Δ0 by construction (identical files) | — | closed |

**Whole lane re-measured on top of main `a0003899e`** (after W16-QA and
W16-PU landed), one `ab_measure --patch` of the lane's map/splits/alias diff:
prediction +8 / +796 B, **measured +8 fns / +796 B** (leg A 53,569 /
58.065230% → leg B 53,577 / 58.072998%). Two rows moved down, both named in
this doc: TrackWidget's 4 B `list<CharClip*>::sort` thunk (100 → 95, §3.2) and
`Locale::Init` (96.84 → 96.81, §3.3). GuitarFx, WaveFile, MidiParser and
MusicLibraryNetSetlists reach 100% on `mpn` because a wrongly attributed row
left each one.

Ordered by measured bytes per category: #2 +428 B, #1 +368 B, #3 0 B,
#4 Δ0 (header control), the rest unmeasured by design. The whole lane bought
**+8 fns / +796 B** (sum of the four kept A/Bs; FastSort is Δ0).

## 2. Method and controls

- **Gap population.** Every `report.json` row with `fuzzy < 100` in a unit
  that has a base obj: 2,336 rows / 682,108 B, ring-tagged with W16-PN's
  `scope_ledger2.tier()`.
- **DC3 retail bodies.** The canonical named DC3 obj tree
  (`../.dc3_text_scratch/named/obj`, via `tools/dc3_obj_source.py`) read with
  `dc3_content_match.read_coff_functions` (relocation-masked body hash). A row
  is a "unique twin" when exactly one DC3 symbol carries its hash.
- **Flank matching.** For each anon or unpaired row, the TU pinned on each
  side of it in `splits.txt`, and whether that TU's *our* obj defines a body
  with the row's masked hash. Then `tools/icf_pair_adjudicate.py --chase` on
  `[fn_<addr>, candidate]` against retail bytes.
- **Callee position** (`callpos.py`): from per-row objdiff instruction diffs,
  `bl` targets present on one side only, grouped by callee.
- **Immediates** (`imm.py`): `diff_arg` immediates on loads/stores (member
  offsets, excluding `r1`) and on `lwz` followed by `mtctr` (vtable slots).
- **Control on the naming instrument.** Two naming batches were run *with*
  the risky names and then without them, so the fold-bait cost is measured,
  not assumed (§3.2).
- **Lane boundaries respected.** No `src/system` function body was edited.
  The only `src/` edit is the header control in §3.4, which was measured and
  reverted.

## 3. Categories

### 3.1 DC3-shaped phantom names (#2)

Rows whose map name is not defined by their own base obj, so they read 0%
however correct our code is. 50 rows / 3,408 B at the lane's start.

**Fixed (`f5995339b`).** `0x826e4348`, `0x826e58d0`, `0x826e6070` were mapped
as `__find_if` / `remove_copy_if` / `remove_if` over `FilePath*` (Dir's
instantiation) and pinned in `system/obj/Dir.cpp`. Retail's body sits between
VocalPlayer functions and our VocalPlayer obj holds byte-identical
`VocalPart*` / `mem_fun_t<bool,VocalPart>` instantiations. Renamed, the three
`.text` lines moved to `VocalPlayer.cpp`, alias group `0x826e6070` relabelled
(survivor VocalPart, Singer folded).
Prediction +3 fns / +428 B; **measured +3 / +428 B**, 0 rows down.

**Left: 47 rows / 2,980 B** (in scope 25 / 1,420 B):

| tier | B | unit | row (demangled, truncated) |
|---|---:|---|---|
| IN-CORE | 104 | Msg | `public: void __cdecl ObjPtr<class Hmx::Object>::SetObjConcrete(class Hmx::Object *)` |
| IN-CORE | 88 | Msg | `private: struct stlpmtx_std::_Rb_tree_node_base * __cdecl stlpmtx_std::_Rb_tree<class Symbol, struct stlpmtx_s` |
| IN-CORE | 80 | DataPointMgr | `void __cdecl GameModeTerminate(void)` |
| IN-CORE | 24 | Joypad | `void __cdecl TrueColor::NuipTrueColorSetPlayer(enum _NUIP_CAMERA_OWNER, unsigned long)` |
| IN-CORE | 16 | Object | `void __cdecl SetFileChecksumData(void)` |
| IN-CORE | 8 | User | `public: virtual class RemoteUser * __cdecl RemoteUser::GetRemoteUser(void)` |
| IN-RB3ENG | 164 | GemTrackDir | `public: class RndCam * __cdecl ObjectDir::Find<class RndCam>(char const *, bool)` |
| IN-RB3ENG | 116 | GemTrackResourceManager | `protected: void __cdecl stlpmtx_std::vector<class GemTrackResourceManager::SmasherPlateInfo, class stlpmtx_std` |
| IN-RB3ENG | 116 | BandStarDisplay | `public: virtual __cdecl ObjPtr<class BandStarDisplay>::~ObjPtr<class BandStarDisplay>(void)` |
| IN-RB3ENG | 12 | VocalTrackDir | `?Copy@UILabel@@$4PPPPPPPM@A@AAXPBVObject@Hmx@@W4CopyType@23@@Z` |
| IN-RB3ENG | 12 | system/ui/UIProxy | `?Load@UIPicture@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` |
| IN-RB3ENG | 4 | system/bandobj/BandHeadShaper | `public: __cdecl MemHeapTracker::~MemHeapTracker(void)` |
| IN-SOON | 100 | MetaPerformer | `void __cdecl SendDataPoint<char const *, int>(char const *, char const *, int)` |
| IN-SOON | 96 | band3/bandtrack/Gem | `void __cdecl stlpmtx_std::__destroy_range_aux<class stlpmtx_std::reverse_iterator<struct 'anonymous namespace'` |
| IN-SOON | 88 | band3/game/PerfectSectionTracker | `private: struct stlpmtx_std::_Rb_tree_node_base * __cdecl stlpmtx_std::_Rb_tree<int, struct stlpmtx_std::less<` |
| IN-SOON | 80 | BandUser | `public: virtual void * __cdecl AutoplayAuditionUser::'scalar deleting destructor'(unsigned int)` |
| IN-SOON | 72 | Tour | `public: virtual bool __cdecl Hmx::Object::SyncProperty(class DataNode &, class DataArray *, int, enum PropOp)` |
| IN-SOON | 68 | Accomplishment | `public: void __cdecl ObjPtrList<class RndGroup, class ObjectDir>::push_back(class RndGroup *)` |
| IN-SOON | 68 | band3/game/Stats | `public: virtual void * __cdecl TrainerPanel::'vector deleting destructor'(unsigned int)` |
| IN-SOON | 60 | SongData | `void __cdecl stlpmtx_std::_Param_Construct<class stlpmtx_std::vector<short, class stlpmtx_std::StlNodeAlloc<sh` |
| IN-SOON | 16 | MetaPanel | `?Highlight@UIComponent@@$4PPPPPPPM@3AAXXZ` |
| IN-SOON | 12 | BandUser | `??_EAutoplayAuditionUser@@$4PPPPPPPM@A@AAPAXI@Z` |
| IN-SOON | 8 | band3/game/BandPerformer | `public: void __cdecl DSP::Synapse::PitchCorrectedVoice::SetReleaseSmoothing(float)` |
| IN-SOON | 4 | SongSortMgr | `public: __cdecl stlpmtx_std::map<int, float, struct stlpmtx_std::less<int>, class stlpmtx_std::StlNodeAlloc<st` |
| IN-SOON | 4 | band3/meta_band/AssetMgr | `public: __cdecl stlpmtx_std::_Rb_tree<int, struct stlpmtx_std::less<int>, struct stlpmtx_std::pair<int const, ` |
| VIA-DC3 | 136 | Character | `public: __cdecl stlpmtx_std::vector<struct 'anonymous namespace'::Unlockable, class stlpmtx_std::StlNodeAlloc<` |
| VIA-DC3 | 136 | Flow | `public: __cdecl stlpmtx_std::vector<class String, class stlpmtx_std::StlNodeAlloc<class String> >::~vector<cla` |
| VIA-DC3 | 128 | Sfx | `public: class Sequence * __cdecl Synth::Find<class Sequence>(char const *, bool)` |
| VIA-DC3 | 112 | Shockwave | `public: static class Hmx::Object * __cdecl WorldDir::NewObject(void)` |
| VIA-DC3 | 100 | CharClipSet | `public: struct stlpmtx_std::_List_iterator<class Hmx::Object *, struct stlpmtx_std::_Nonconst_traits<class Hmx` |
| VIA-DC3 | 96 | SampleInst | `class SampleMarker * __cdecl stlpmtx_std::__uninitialized_copy<class SampleMarker const *, class SampleMarker ` |
| VIA-DC3 | 96 | Sequence | `struct 'anonymous namespace'::Unlockable * __cdecl stlpmtx_std::__uninitialized_copy<struct 'anonymous namespa` |
| VIA-DC3 | 88 | CameraManager | `public: virtual void * __cdecl TransitionEvent::'scalar deleting destructor'(unsigned int)` |
| VIA-DC3 | 84 | CharServoBone | `public: class ObjDirItr<class Hmx::Object> & __cdecl ObjDirItr<class Hmx::Object>::operator++(void)` |
| VIA-DC3 | 76 | system/synth/Synth | `public: virtual void * __cdecl ObjPtr<class SynthSample>::'scalar deleting destructor'(unsigned int)` |
| VIA-DC3 | 72 | FlowOnStop | `public: static class RndMesh * __cdecl Hmx::Object::New<class RndMesh>(void)` |
| VIA-DC3 | 68 | Instance | `public: virtual void * __cdecl BandRetargetVignette::'vector deleting destructor'(unsigned int)` |
| VIA-DC3 | 68 | MeshAnim | `public: virtual void * __cdecl NetSession::'vector deleting destructor'(unsigned int)` |
| VIA-DC3 | 68 | CharIKHead | `public: virtual void * __cdecl CharIKHead::'vector deleting destructor'(unsigned int)` |
| VIA-DC3 | 68 | CharMirror | `public: virtual void * __cdecl CharMirror::'vector deleting destructor'(unsigned int)` |
| VIA-DC3 | 68 | StorePreviewMgr | `public: virtual void * __cdecl StorePreviewMgr::'vector deleting destructor'(unsigned int)` |
| VIA-DC3 | 60 | Character | `void __cdecl stlpmtx_std::_Copy_Construct<class OldMatOption>(class OldMatOption *, class OldMatOption const &` |
| VIA-DC3 | 12 | LabelShrinkWrapper | `??_GLabelShrinkWrapper@@$4PPPPPPPM@A@AAPAXI@Z` |
| VIA-DC3 | 12 | InlineHelp | `??_GInlineHelp@@$4PPPPPPPM@A@AAPAXI@Z` |
| VIA-DC3 | 4 | StreamNull | `public: virtual void __cdecl Sfx::SynthPoll(void)` |
| VIA-DC3 | 4 | system/rndobj/Rnd | `public: static void __cdecl BinStream::operator delete(void *)` |
| VIA-DC3 | 4 | CharLipSyncDriver | `unsigned long __cdecl CurrentThreadId(void)` |

How to work a row: find which obj holds a body with the retail row's masked
hash (§2), adjudicate with `icf_pair_adjudicate.py --chase`, rename, move the
pin, run `alias_survivor_relabel.py --write` if the build's survivor-drift
check fires, then A/B. The `$4PPPPPPPM` rows are vtordisp thunks and 4–8 B
rows are fold bait (§3.2); do those last and one at a time.

### 3.2 Mis-homed instantiation pins (#1)

45 rows / 2,704 B were anonymous or unpaired rows whose pinned TU's obj lacks
the body while the flanking TU's obj has it (another 35 rows / 3,520 B have no
body in any of our objs, 3 / 428 B had it in a non-flank obj; those were #2).

**Batch 1 (`d12579f9f`), +4 fns / +192 B, 0 rows down.** Pins moved and rows
named: `0x827e53e0` `_List_base<TextInstance>::clear` and the 4 B MidiParser
thunks `0x827e4960` / `0x827e5c48` → TrackWidgetImp; `0x826f5710`
`StatCollector::StatCollector(GemPlayer&)` → StatCollector; `0x827d5bb0`
`__uninitialized_fill_n<MemDiffEntry>` → MemTracker (a `null` row replaced;
alias group relabelled, both members re-PROVEN).
First run read +2 / +180 against a prediction of +3 / +184: a MidiParser 4 B
thunk branches to TextInstance's `clear`, so it fell off until it too was
renamed and re-homed.

**Negative result, batch 2: −1 fn / −5,584 B.** Also naming `0x827e6178`
(`CharWidgetImp::SetDirty`) and `0x827e6180` (`CharWidgetImp::Instances`),
8 B bodies retail shares across many types, charged every caller that reaches
them under another type: `GemPlayer::ConfigureBehavior`, Performer's
`SetCrowdMeterActive` / `LoseGame` / `Handle`, and three SongDB rows fell off
100. The `none` control read **+208 B** on the same run — the shape of
forgiveness lost, not a wrong body. Both names withdrawn; the rows stay
anonymous.

**Batch 3 (`dc27de352`), +1 fn / +176 B, one 4 B row down.** Predicted with
all three names: +3 / +264 B. Measured **−1 / −648 B**: the three rows reached
100, but naming `0x8239C558` `__uninitialized_copy<SyncMeshCB::Vert>` charged
SongData's two `RangedData<RGRollChord>` vector rows and Singer's
`SingerResultsData` `_M_fill_insert_aux` (516 + 392 B), which call the same
retail body under their own type. With that name withdrawn (pin kept, the body
does live in CharMeshCacheMgr) the prediction +1 / +176 B was **measured
exactly**. The one row down is TrackWidget's 4 B `list<CharClip*>::sort` thunk
(100 → 95), whose retail branch target is now named MeshInstance's `clear`.

**The rule this measured:** a name is safe when retail's body is unique *and*
no caller of that address in another TU expects another type. The
adjudicator's `retail_bodytwins == 1` does not say the second thing. Check the
retail callers of the address (who `bl`s it) before naming; any caller whose
own type differs will be charged.

**Left: 36 rows / 2,232 B** (in scope 7 / 456 B), with this lane's
adjudication:

| addr | B | tier | pinned in | body in (flank) | best adjudication (FLAT / CHASED / retail twins) | candidate name |
|---|---:|---|---|---|---|---|
| `0x82B992A0` | 88 | IN-CORE | Msg.cpp | GemManager.cpp | UNDECIDABLE / REFUTED / ? | `??$_M_find@VSymbol@@@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@s` |
| `0x823F1898` | 28 | IN-CORE | MemMgr.cpp | SessionMessages.cpp | PROVEN / PROVEN / 1 | `?GetUserData@NewUserMsg@@QBAXAAVBinStream@@@Z` |
| `0x825107C0` | 24 | IN-CORE | Debug.cpp | System.cpp | UNDECIDABLE / REFUTED / 1 | `?OnSystemLanguage@@YA?AVDataNode@@PAVDataArray@@@Z` |
| `0x82359680` | 224 | IN-RB3ENG | TrackPanelDirBase.cpp | QuestManager.cpp | PROVEN / PROVEN / 10 | `?insert_unique@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx` |
| `0x826CB610` | 68 | IN-SOON | band3/game/Stats.cpp | band3/game/TrainerPanel.cpp | UNDECIDABLE / REFUTED / ? | `??_GTrainerPanel@@UAAPAXI@Z` |
| `0x8252A610` | 16 | IN-SOON | AccomplishmentPanel.cpp | Memcard_Xbox.cpp | UNDECIDABLE / PROVEN / 72 | `??1MCContainer@@UAA@XZ` |
| `0x82782950` | 8 | IN-SOON | VocalNoteList.cpp | SongParser.cpp | UNDECIDABLE / PROVEN / 1 | `?OnNewTrack@SongParser@@UAAXH@Z` |
| `0x827390F8` | 48 | OUT-360-OTHER | Rnd_Xbox.cpp | Env_NG.cpp | UNDECIDABLE / REFUTED / ? | `?ClassName@NgEnviron@@UBA?AVSymbol@@XZ` |
| `0x82279710` | 16 | OUT-360-OTHER | Memory_Xbox.cpp | PatchDir.cpp | PROVEN / PROVEN / 11 | `?Highlight@RndDir@@$4PPPPPPPM@DM@AAXXZ` |
| `0x82279720` | 16 | OUT-360-OTHER | Memory_Xbox.cpp | PatchDir.cpp | PROVEN / PROVEN / 7 | `?Print@RndTransformable@@$4PPPPPPPM@JA@AAXXZ` |
| `0x82279730` | 16 | OUT-360-OTHER | Memory_Xbox.cpp | PatchDir.cpp | PROVEN / PROVEN / 11 | `?PreLoad@RndDir@@$4PPPPPPPM@DM@AAXAAVBinStream@@@Z` |
| `0x82279740` | 16 | OUT-360-OTHER | Memory_Xbox.cpp | PatchDir.cpp | REFUTED / REFUTED / 11 | `?Highlight@RndDir@@$4PPPPPPPM@DM@AAXXZ` |
| `0x82279750` | 16 | OUT-360-OTHER | Memory_Xbox.cpp | PatchDir.cpp | REFUTED / REFUTED / 11 | `?Highlight@RndDir@@$4PPPPPPPM@DM@AAXXZ` |
| `0x8249B200` | 4 | OUT-360-OTHER | VorbisMem.cpp | EventTrigger.cpp | UNDECIDABLE / REFUTED / 392 | `??1MemHeapTracker@@QAA@XZ` |
| `0x82775C98` | 292 | VIA-DC3 | UIList.cpp | SongData.cpp | PROVEN / PROVEN / 6 | `?_M_erase@?$vector@V?$vector@VRangeSection@@V?$StlNodeAlloc@` |
| `0x827C67D8` | 232 | VIA-DC3 | FlowSetProperty.cpp | Song.cpp | PROVEN / PROVEN / 7 | `?insert_unique@?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$` |
| `0x827C6708` | 204 | VIA-DC3 | FlowSetProperty.cpp | Song.cpp | REFUTED / PROVEN / 7 | `?_M_insert@?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHV` |
| `0x824D05A8` | 112 | VIA-DC3 | Shockwave.cpp | system/world/Dir.cpp | PROVEN / PROVEN / 24 | `?_M_fill_insert@?$vector@VFilePath@@V?$StlNodeAlloc@VFilePat` |
| `0x822C5758` | 96 | VIA-DC3 | MidiInstrument.cpp | BandIKEffector.cpp | PROVEN / PROVEN / 3 | `??4?$ObjVector@VConstraint@BandIKEffector@@@@QAAXABV0@@Z` |
| `0x827B74D0` | 96 | VIA-DC3 | StorePanel.cpp | system/meta/StoreArtLoaderPanel.cpp | PROVEN / PROVEN / 4 | `??$__destroy_range_aux@V?$reverse_iterator@PAVArtEntry@Store` |
| `0x823CD060` | 76 | VIA-DC3 | CharDriverMidi.cpp | CharMirror.cpp | REFUTED / PROVEN / 577 | `??_G?$ObjPtr@VCharServoBone@@@@UAAPAXI@Z` |
| `0x822C6A40` | 68 | VIA-DC3 | Instance.cpp | BandRetargetVignette.cpp | UNDECIDABLE / REFUTED / ? | `??_GBandRetargetVignette@@UAAPAXI@Z` |
| `0x823C75D8` | 68 | VIA-DC3 | FlowIf.cpp | CharUpperTwist.cpp | PROVEN / PROVEN / 4 | `??_GCharUpperTwist@@MAAPAXI@Z` |
| `0x823E46D0` | 68 | VIA-DC3 | MeshAnim.cpp | network/net/NetSession.cpp | UNDECIDABLE / REFUTED / ? | `??_GNetSession@@UAAPAXI@Z` |
| `0x823CED00` | 68 | VIA-DC3 | CharDriver.cpp | CharNeckTwist.cpp | REFUTED / REFUTED / 4 | `??_GCharNeckTwist@@UAAPAXI@Z` |
| `0x823793C8` | 68 | VIA-DC3 | CharForeTwist.cpp | CharDriver.cpp | REFUTED / REFUTED / 1 | `??_GCharDriver@@UAAPAXI@Z` |
| `0x823B08D0` | 68 | VIA-DC3 | FlowSwitchCase.cpp | CharPollGroup.cpp | REFUTED / REFUTED / 4 | `??_GCharPollGroup@@UAAPAXI@Z` |
| `0x826B2918` | 68 | VIA-DC3 | FlowOnStop.cpp | PracticePanel.cpp | REFUTED / PROVEN / 1 | `??_GPracticePanel@@UAAPAXI@Z` |
| `0x8268C978` | 16 | VIA-DC3 | PropKeys.cpp | BandUser.cpp | PROVEN / PROVEN / 1 | done by W16-QA (`d8aef7dcf`): `?SyncProperty@BandUser@@$4PPPPPPPM@PPPPPPCE@AA_NAAVDataNode@` |
| `0x82471D10` | 12 | VIA-DC3 | TransAnim.cpp | LitAnim.cpp | UNDECIDABLE / REFUTED / 1197 | `?SyncProperty@RndAnimatable@@$4PPPPPPPM@A@AA_NAAVDataNode@@P` |
| `0x82766F98` | 8 | VIA-DC3 | Mesh.cpp | Msg.cpp | UNDECIDABLE / PROVEN / 43 | `??1EventSink@MsgSource@@QAA@XZ` |
| `0x825459E0` | 8 | VIA-DC3 | CharServoBone.cpp | ProfileMgr.cpp | UNDECIDABLE / PROVEN / 1 | `?GetSecondPedalHiHat@ProfileMgr@@QBA_NXZ` |
| `0x822D4088` | 4 | VIA-DC3 | CharMeshHide.cpp | EndingBonus.cpp | UNDECIDABLE / PROVEN / 392 | `??1MiniIconData@EndingBonus@@QAA@XZ` |
| `0x82343940` | 4 | VIA-DC3 | UIComponent.cpp | system/bandobj/BandButton.cpp | UNDECIDABLE / PROVEN / 392 | `??1UIButton@@UAA@XZ` |
| `0x82308470` | 4 | VIA-DC3 | FlowNode.cpp | TrackPanelDir.cpp | UNDECIDABLE / REFUTED / 392 | `?Print@String@@UAAXPBD@Z` |
| `0x826DF788` | 4 | VIA-DC3 | TexProc.cpp | StreakTracker.cpp | UNDECIDABLE / REFUTED / 392 | `?uninitialized_copy@stlpmtx_std@@YAPADPBD0PAD@Z` |

### 3.3 Anonymous rows DC3's map can name (#3)

61 anonymous gap rows have exactly one DC3 retail symbol with the same masked
body hash (2,836 B). They split:

- **EH funclets (`__unwind$…`)**, 30 rows: the hash is the funclet template;
  the DC3 symbol is an unrelated function's funclet. Not names.
- **4–8 B bodies**, 10 rows: getters and thunks whose hash matches an
  arbitrary DC3 function (`ShaderPoolAlloc`, `EnableReads`, …). Fold bait
  even if right.
- **Init region**, 2 rows: false, §3.6.
- **Real instantiation candidates**, ~10 rows / ~800 B. But DC3 names them
  with **DC3's types**: `0x822C5758` is `ObjVector<CharMeshHide::Hide>::operator=`
  in DC3, while its flank TU in RB3 is BandIKEffector and our obj's twin is
  `ObjVector<BandIKEffector::Constraint>`. The hash proves the shape, not
  the type.

**Sample: `0x827c9540` `FastSort<3>` (`c828ef19b`).** Unique twin in DC3's
`system/utl/Locale.obj`; our StringTable obj defines the same instantiation
(72 B retail vs 68 B ours). Prediction: pairs, stays below 100, Δ0 / Δ0.
**Measured Δ0 fns / Δ0 B, Δfuzzy +0.000587 pp; the row reads 79.7 / mpn
83.3.** Kept: the name is right and turns a placeholder into a row a body
lane can work. The one cost is a newly checked name: `Locale::Init` (already
below 100) moves 96.84 → 96.81, 0 B. Its reference to this address is now
compared by name instead of being forgiven as a placeholder, so whatever it
calls there is worth a look by the Locale body lane.

Anonymous rows whose unique DC3 twin DC3-decomp matches at 100% (17 rows /
956 B). `0x827C9540` is now named; `0x827E6178` was measured as fold bait
(§3.2); `0x822C5758` carries DC3's type, not RB3's:

| tier | B | fuzzy | unit | our row | DC3 retail name (unique masked-hash twin) |
|---|---:|---:|---|---|---|
| IN-CORE | 128 | 0.0 | Geo | `fn_822BB3C8` | `??$__push_heap@PAPAVRndTransformable@@HPAV1@P6A_NPBV1@0@Z@stlpmtx_std@@YAXPAPAVR` |
| IN-CORE | 72 | 0.0 | StringTable | `fn_827C9540` | `??$FastSort@$02@LocaleChunkSort@@YAHPBX0@Z` |
| IN-CORE | 72 | 0.0 | System | `fn_82C42910` | `?ClearSlowFrame@Timer@@SAXXZ` |
| IN-CORE | 60 | 0.0 | FilePath | `fn_82C40D68` | `?BinkInit@@YAXXZ` |
| IN-SOON | 8 | 0.0 | MidiParser | `fn_827E6178` | `?SetDisabled@Debug@@QAAX_N@Z` |
| IN-SOON | 8 | 0.0 | system/beatmatch/BeatMaster | `fn_8276F6F8` | `?EnableReads@VorbisReader@@UAAX_N@Z` |
| IN-SOON | 8 | 0.0 | VocalNoteList | `fn_82782950` | `?ShaderPoolAlloc@RndShaderMgr@@IAAXH@Z` |
| VIA-DC3 | 116 | 0.0 | CheatProvider | `fn_823F52C0` | `?push_back@?$vector@UDebugGraph@?A0xb39b74bf@@V?$StlNodeAlloc@UDebugGraph@?A0xb3` |
| VIA-DC3 | 100 | 0.0 | Flow | `fn_82574D90` | `?NewObject@FlowDistance@@SAPAVObject@Hmx@@XZ` |
| VIA-DC3 | 96 | 0.0 | PanelDir | `fn_828070E0` | `??$__uninitialized_copy@PBVDataEvent@@PAV1@@stlpmtx_std@@YAPAVDataEvent@@PBV1@0P` |
| VIA-DC3 | 96 | 0.0 | MidiInstrument | `fn_822C5758` | `??4?$ObjVector@VHide@CharMeshHide@@@@QAAXABV0@@Z` |
| VIA-DC3 | 80 | 0.0 | system/ui/UILabelDir | `fn_82812418` | `??_GHamCharacter@@UAAPAXI@Z` |
| VIA-DC3 | 60 | 0.0 | CharCollide | `fn_822C9048` | `??$_Param_Construct@V?$ObjVector@ULod@Character@@@@V1@@stlpmtx_std@@YAXPAV?$ObjV` |
| VIA-DC3 | 24 | 0.0 | UIFontImporter | `fn_828027A8` | `?Init@Movie@@SAXXZ` |
| VIA-DC3 | 12 | 0.0 | StoreEnumeration | `fn_827B8680` | `?GetAchievementData@Achievements@@AAA?AUXUSER_ACHIEVEMENT@@HH@Z` |
| VIA-DC3 | 8 | 0.0 | UIComponent | `fn_828012D8` | `?Mat@UIListProvider@@UBAPAVRndMat@@HHPAVUIListMesh@@@Z` |
| VIA-DC3 | 8 | 0.0 | CharServoBone | `fn_825459E0` | `?IsRunning@RandomIntervalGroupSeqInst@@UAA_NXZ` |

### 3.4 Header differences (#4)

668 headers differ textually between `src/system` and dc3-decomp's
(`hdr_diff.json`: Object.h 1,763 lines differ, ObjPtr_p.h 1,105, Text.h 702,
Mtx.h 474, BaseMaterial.h 458, …). Headers reached by band3 units emit about
19 header-COMDAT rows / 4,284 B below 100. Three signal populations from the
diff instrument:

- **vtable-slot immediates** (`lwz rX, N(rY)` → `mtctr`, N differs): 16 rows
  / 10,224 B, 7,588 B of it Quazal. In scope: `MasterAudio::SetButtonMashingMode`
  (88 B, slot 20 vs 80) and `BandRetargetVignette::Poll` (280 B, 32 vs 0).
- **uniform member delta** (every differing load/store offset shifted by one
  constant): 16 rows / 14,664 B. In scope: `Song::SyncState` (+8 ×16; these
  are stack-frame offsets, not members), `PreInitSystem` / `InitSystem`
  (−8), `SongData::TrimOverlappingGems` (+4 ×2), `ClosetPanel::CycleCamera`
  (−16 ×2). None is a member offset after reading the rows.
- **callee in/out of line**: 30 rows / 24,552 B (5,916 B in scope).

W16-PZ (landed `68ea81224`) checked every bandobj/band3 class layout against
retail and found no wrong member, so the member-layout half of this category
is already closed for band3.

**Control: what a DC3 header adoption costs.** DC3 inlines
`DataArray::Release` in `obj/Data.h`; ours declares it out of line. Retail
RB3 has it out of line at `0x82270510`, so the prediction is negative.
**Prediction failed: measured Δ0 fns / Δ0 B / Δ0 fuzzy, with 1,034 TUs
recompiled in leg B** (`ab_measure`, source patch, both legs settled). The
`?Release@DataArray@@QAAXXZ` row stays at 100 in DataArray on both legs: under
`/O1` the compiler declines to inline a body that ends in `delete this`, emits
the same COMDAT, and every caller still `bl`s it. So the textual difference was
inert. A textual header diff does not show that codegen differs; only a
retail-byte disagreement in a row shows that. The control was reverted.
`DataArray::Release` is a useful example in its own right: retail RB3 keeps it
out of line at `0x82270510` (it has a map row), DC3 inlines it, and ours already
follows RB3.

So a DC3 header shows what a *later* Milo did, and even where it differs from
ours the difference is often codegen-inert. The levers in this category are
rows where retail bytes disagree with *our* header. The signal tables below
find those rows; DC3 does not.

Vtable-slot rows:

| tier | B | fuzzy | unit | row | signal (target,base) |
|---|---:|---:|---|---|---|
| IN-RB3ENG | 280 | 90.11 | BandRetargetVignette | `public: virtual void __cdecl BandRetargetVignette::Poll(void)` | [[32, 0]] |
| IN-SOON | 88 | 86.50 | MasterAudio | `public: void __cdecl MasterAudio::SetButtonMashingMode(int, bool)` | [[20, 80]] |
| OUT-360-OTHER | 588 | 85.95 | system/synth_xbox/Synth | `private: void __cdecl Synth360::SetupHeadsetSubmixes(void)` | [[192, 0], [0, 84], [76, 0]] |
| OUT-QUAZAL | 2912 | 97.29 | network/quazal/Transport/PRUDP/PRUDPEndPoint | `public: void __cdecl Quazal::PRUDPEndPoint::ServiceIncomingPacket(class Quazal::` | [[88, 92], [364, 284], [104, 116]] |
| OUT-QUAZAL | 1208 | 97.64 | network/quazal/Transport/PRUDP/PRUDPStream | `public: virtual bool __cdecl Quazal::PRUDPStream::ReceiveIncomingPacket(unsigned` | [[1204, 1188], [1204, 1188], [1204, 1188]] |
| OUT-QUAZAL | 840 | 99.73 | network/quazal/Transport/UDP/UDPTransport | `public: virtual bool __cdecl Quazal::UDPTransport::StopListen(unsigned short)` | [[244, 208]] |
| OUT-QUAZAL | 828 | 99.81 | network/quazal/Transport/UDP/UDPTransport | `public: virtual bool __cdecl Quazal::UDPTransport::StartListen(unsigned short, u` | [[204, 192]] |
| OUT-QUAZAL | 708 | 99.90 | network/quazal/Transport/UDP/UDPTransport | `public: virtual bool __cdecl Quazal::UDPTransport::Send(unsigned short, enum Qua` | [[292, 300]] |
| OUT-QUAZAL | 440 | 99.69 | network/quazal/Transport/PRUDP/PRUDPStream | `public: void __cdecl Quazal::PRUDPStream::ReleaseEndPoint(class Quazal::PRUDPEnd` | [[540, 556], [540, 556]] |
| OUT-QUAZAL | 280 | 99.53 | ChecksumAlgorithm | `public: class Quazal::Key __cdecl Quazal::ChecksumAlgorithm::DeriveKey(class Qua` | [[188, 204], [188, 204]] |
| OUT-QUAZAL | 208 | 64.35 | network/quazal/Transport/Interface/JobConnectEndPoint | `public: bool __cdecl Quazal::JobConnectEndPoint::CanRouteTo(class Quazal::Statio` | [[240, 232]] |
| OUT-QUAZAL | 164 | 99.61 | network/quazal/Transport/Interface/JobConnectEndPoint | `public: void __cdecl Quazal::JobConnectEndPoint::PrepareNATTraversal(void)` | [[124, 116]] |
| VIA-DC3 | 640 | 80.78 | system/rndobj/Rnd | `protected: virtual void __cdecl Rnd::DrawPreClear(void)` | [[0, 4]] |
| VIA-DC3 | 492 | 73.19 | VelocityBuffer | `public: void __cdecl RndVelocityBuffer::DrawMesh(class RndMesh *) const` | [[56, 0]] |
| VIA-DC3 | 288 | 75.93 | UILabel | `protected: void __cdecl UILabel::SetTokenFmtImp(class Symbol, class DataArray co` | [[88, 0]] |
| VIA-DC3 | 260 | 55.66 | StorePanel | `public: void __cdecl StorePanel::CheckOut(class StorePurchaseable *)` | [[124, 0]] |

Uniform-delta rows:

| tier | B | fuzzy | unit | row | signal (target,base) |
|---|---:|---:|---|---|---|
| IN-CORE | 1192 | 96.72 | Song | `public: void __cdecl Song::SyncState(void)` | {'8': 16} |
| IN-CORE | 652 | 99.98 | System | `void __cdecl PreInitSystem(char const *)` | {'-8': 3} |
| IN-CORE | 228 | 99.89 | System | `void __cdecl InitSystem(char const *)` | {'-8': 6} |
| IN-SOON | 536 | 87.41 | SongData | `public: void __cdecl SongData::TrimOverlappingGems(int, int, int)` | {'4': 2} |
| IN-SOON | 480 | 99.98 | band3/meta_band/ClosetPanel | `public: void __cdecl ClosetPanel::CycleCamera(void)` | {'-16': 2} |
| OUT-NET | 1088 | 97.07 | Server | `public: virtual void __cdecl XboxServer::Poll(void)` | {'4': 8} |
| OUT-NET | 428 | 94.64 | network/net/NetSession | `public: virtual void __cdecl NetSession::Poll(void)` | {'8': 10} |
| OUT-QUAZAL | 1052 | 96.35 | network/quazal/Transport/Interface/StationContactInfo | `public: void __cdecl Quazal::StationContactInfo::SortAndFilterTarget(class Quaza` | {'-8': 6} |
| OUT-QUAZAL | 368 | 99.87 | network/quazal/Transport/UDP/UDPTransport | `public: class Quazal::QueuingSocket * __cdecl Quazal::UDPTransport::FindSocket(u` | {'-12': 12} |
| OUT-QUAZAL | 156 | 99.79 | network/quazal/Transport/Interface/JobConnectEndPoint | `public: void __cdecl Quazal::JobConnectEndPoint::SortURLs(void)` | {'8': 3} |
| OUT-QUAZAL | 52 | 75.46 | network/quazal/Transport/UDP/UDPTransport | `public: __cdecl Quazal::qSortedVector<unsigned short, class Quazal::QueuingSocke` | {'16': 2} |
| VIA-DC3 | 4728 | 99.39 | Spotlight | `public: virtual bool __cdecl Spotlight::SyncProperty(class DataNode &, class Dat` | {'384': 4} |
| VIA-DC3 | 1448 | 96.47 | CharBonesSamples | `public: void __cdecl CharBonesSamples::Relativize(class CharClip *)` | {'40': 2} |
| VIA-DC3 | 1440 | 99.65 | Character | `public: virtual void __cdecl Character::PostLoad(class BinStream &)` | {'8': 17} |
| VIA-DC3 | 704 | 99.95 | StorePreviewMgr | `public: virtual class DataNode __cdecl StorePreviewMgr::Handle(class DataArray *` | {'16': 6} |
| VIA-DC3 | 112 | 99.82 | Spline | `protected: class RndSpline::CtrlPoint * __cdecl stlpmtx_std::vector<class RndSpl` | {'-52': 2} |

Callee in/out of line, by callee:

| callee retail calls out of line (we inline it, or the reverse) | rows | row bytes | tiers | units |
|---|---:|---:|---|---|
| `??6TextStream@@QAAAAV0@PBD@Z` | 4 | 19184 | VIA-DC3 | AmbientOcclusion |
| `??$MakeString@M@@YAPBDPBDM@Z` | 2 | 9592 | VIA-DC3 | AmbientOcclusion |
| `?Multiply@@YAXABVVector3@@ABVTransform@@AAV1@@Z` | 2 | 5668 | VIA-DC3 | AmbientOcclusion, system/rndobj/Utl |
| `printbuf_memappend` | 1 | 4872 | OUT-360-OTHER | json_tokener |
| `??$MakeString@PAD@@YAPBDPBDPAD@Z` | 1 | 4796 | VIA-DC3 | AmbientOcclusion |
| `?DistanceSH@RndAmbientOcclusion@@IBAMABVVector4@@ABVVector3@@01@Z` | 1 | 4796 | VIA-DC3 | AmbientOcclusion |
| `??0Vert@RndMesh@@QAA@XZ` | 1 | 4796 | VIA-DC3 | AmbientOcclusion |
| `??$MakeString@KKJ@@YAPBDPBDKKJ@Z` | 1 | 4796 | VIA-DC3 | AmbientOcclusion |
| `?FillAt@FillInfo@@QBA_NHAAUFillExtent@@_N@Z` | 2 | 3304 | IN-SOON | GemTrack |
| `?GetLoopTick@@YAHHAAH@Z` | 1 | 1652 | IN-SOON | GemTrack |
| `??0Symbol@@QAA@PBD@Z` | 6 | 1420 | IN-SOON, VIA-DC3 | CharacterTest, GemTrack, UILabel |
| `?ShowState@OvershellSlot@@QAAXW4OvershellSlotStateID@@@Z` | 1 | 1416 | IN-SOON | OvershellPanel |
| `?SignalError@SystemError@Quazal@@SAXPADIII@Z` | 3 | 1400 | OUT-QUAZAL | DuplicatedObject, network/quazal/JobConnectStation |
| `??$_outline_back@V?$vector@VPoint@RndLine@@V?$StlNodeAlloc@VPoint@RndLine@@@stlpmtx_std@@@` | 1 | 1328 | VIA-DC3 | Line |
| `?SetSongAndArtistName@AppLabel@@QAAXPBVSongSortNode@@@Z` | 1 | 1292 | IN-SOON | MusicLibrary |
| `?SetSongName@AppLabel@@QAAXPBVSongSortNode@@@Z` | 1 | 1292 | IN-SOON | MusicLibrary |
| `?SystemConfig@@YAPAVDataArray@@VSymbol@@@Z` | 2 | 840 | VIA-DC3 | Splash |
| `?IsValid@FetchRef@Quazal@@QAA_NXZ` | 1 | 712 | OUT-QUAZAL | network/quazal/ObjDupProtocol |
| `?Current@StationSelection@Quazal@@QAAPAVStation@2@XZ` | 1 | 636 | OUT-QUAZAL | network/quazal/ObjDupProtocol |
| `__RTDynamicCast` | 2 | 592 | VIA-DC3 | CharClipSet, CharacterTest |
| `?IsValid@?$DORefTemplate@VStation@Quazal@@@Quazal@@QBA_NXZ` | 1 | 584 | OUT-QUAZAL | network/quazal/JobConnectStation |
| `?GetTime@Time@Quazal@@SA?AV12@XZ` | 1 | 520 | OUT-QUAZAL | network/quazal/Session |
| `?GetWellKnownPort@Quazal@@YAGXZ` | 1 | 428 | OUT-QUAZAL | network/quazal/ObjDupProtocol |
| `??2@YAPAXI@Z` | 1 | 424 | IN-CORE | File |
| `?MainThread@@YA_NXZ` | 1 | 424 | IN-CORE | File |
| `??1String@@UAA@XZ` | 1 | 408 | VIA-DC3 | CharLipSync |
| `??$Find@VRndTex@@@ObjectDir@@QAAPAVRndTex@@PBD_N@Z` | 1 | 380 | VIA-DC3 | MidiSynth |
| `?GetDOPtr@?$SelectionIteratorTemplate@VStation@Quazal@@@Quazal@@QAAPAVStation@2@XZ` | 1 | 360 | OUT-QUAZAL | DuplicatedObject |
| `?RemoveAllDuplicasOnLeavingStation@DuplicatedObject@Quazal@@SAXVDOHandle@2@@Z` | 1 | 316 | OUT-QUAZAL | network/quazal/Station |
| `?GetInstance@StationManager@Quazal@@SAPAV12@XZ` | 1 | 316 | OUT-QUAZAL | network/quazal/Station |
| `?MovieExtension@@YAPBDPBDW4Platform@@@Z` | 1 | 304 | VIA-DC3 | system/rndobj/Utl |
| `FileGetBase` | 1 | 304 | VIA-DC3 | system/rndobj/Utl |
| `FileGetPath` | 1 | 304 | VIA-DC3 | system/rndobj/Utl |
| `??$MakeString@PBDPBDPBD@@YAPBDPBD000@Z` | 1 | 304 | VIA-DC3 | system/rndobj/Utl |
| `?ForceConcealed@BandList@@QAAXHAAVTransform@@@Z` | 1 | 228 | IN-RB3ENG | BandList |
| `??0DataNode@@QAA@ABV0@@Z` | 1 | 220 | IN-CORE | DataNode |
| `?Release@DataArray@@QAAXXZ` | 1 | 220 | IN-CORE | DataNode |
| `??0?$_List_base@P6AXXZV?$MemAllocator@P6AXXZ@Quazal@@@stlpmtx_std@@QAA@ABV?$MemAllocator@P` | 1 | 180 | OUT-QUAZAL | network/quazal/Session |
| `?Sym@DataNode@@QBA?AVSymbol@@PBVDataArray@@@Z` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?AddDefaults@CharacterTest@@QAAXXZ` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?Walk@CharacterTest@@QAAXXZ` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?Recenter@CharacterTest@@QAAXXZ` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?OnGetFilteredClips@CharacterTest@@IAA?AVDataNode@@PAVDataArray@@@Z` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?Sync@CharacterTest@@IAAXXZ` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?PathName@@YAPBDPBVObject@Hmx@@@Z` | 1 | 112 | VIA-DC3 | CharacterTest |
| `?push_back@?$vector@PAVUIScreen@@V?$StlNodeAlloc@PAVUIScreen@@@stlpmtx_std@@@stlpmtx_std@@` | 1 | 96 | VIA-DC3 | UI |
| `?MemOrPoolFreeSTL@@YAXHPAX@Z` | 2 | 88 | VIA-DC3 | LitAnim |

### 3.5 Shared instantiations: retail == DC3 retail, DC3-decomp at 100%, ours below (#5)

These are the rows DC3's *source* already closes. They are bodies, so they
belong to W16-QA (outside `rndobj/`, `char/`) or W16-PU (inside). The lists
were measured at `90a3e94d2`; W16-QA (`d8aef7dcf`) and W16-PU (`c2e3a92f6`)
landed afterwards and may have closed some of them, so re-read `report.json`
before funding a row. Same name,
DC3 at 100 (15 rows / 2,508 B):

| tier | B | fuzzy | unit | our row | DC3 retail name (unique masked-hash twin) |
|---|---:|---:|---|---|---|
| IN-CORE | 164 | 77.0 | MultiTempoTempoMap | `public: virtual int __cdecl MultiTempoTempoMap::GetLoopTick(int, int &) const` | `?GetLoopTick@MultiTempoTempoMap@@UBAHHAAH@Z` |
| IN-SOON | 80 | 90.0 | MidiReader | `float __cdecl pow(float, int)` | `?pow@@YAMMH@Z` |
| VIA-DC3 | 288 | 99.9 | StreamNull | `public: __cdecl StreamNull::StreamNull(float)` | `??0StreamNull@@QAA@M@Z` |
| VIA-DC3 | 272 | 99.9 | VorbisReader | `unsigned long __cdecl 'anonymous namespace'::DecodeThreadEntry(void *)` | `?DecodeThreadEntry@?A0xcb0871ef@@YAKPAX@Z` |
| VIA-DC3 | 252 | 95.0 | Mesh | `public: void __cdecl PatchVerts::Add(int, class RndMesh::VertVector &, class Vec` | `?Add@PatchVerts@@QAAXHAAVVertVector@RndMesh@@AAVVector3@@@Z` |
| VIA-DC3 | 216 | 99.9 | PropKeys | `public: int __cdecl Keys<bool, bool>::Add(bool const &, float, bool)` | `?Add@?$Keys@_N_N@@QAAHAB_NM_N@Z` |
| VIA-DC3 | 200 | 80.5 | Character | `protected: void __cdecl CharPollableSorter::AddDeps(struct CharPollableSorter::D` | `?AddDeps@CharPollableSorter@@IAAXPAUDep@1@ABV?$list@PAVObject@Hmx@@V?$StlNodeAll` |
| VIA-DC3 | 192 | 99.8 | SpotlightDrawer | `public: void __cdecl SpotlightDrawer::ClearLights(void)` | `?ClearLights@SpotlightDrawer@@QAAXXZ` |
| VIA-DC3 | 176 | 99.8 | MoviePanel | `void __cdecl MetaInit(void)` | `?MetaInit@@YAXXZ` |
| VIA-DC3 | 160 | 99.8 | SpotlightDrawer_NG | `protected: virtual void __cdecl NgSpotlightDrawer::ClearPostProc(void)` | `?ClearPostProc@NgSpotlightDrawer@@MAAXXZ` |
| VIA-DC3 | 124 | 78.7 | system/synth/MoggClip | `public: void __cdecl MoggClip::SetupPanInfo(float, float, bool)` | `?SetupPanInfo@MoggClip@@QAAXMM_N@Z` |
| VIA-DC3 | 112 | 98.7 | system/rndobj/ShaderMgr | `public: void __cdecl RndShaderMgr::UpdateCache(class Transform const &, int)` | `?UpdateCache@RndShaderMgr@@QAAXABVTransform@@H@Z` |
| VIA-DC3 | 108 | 80.7 | Mesh | `public: bool __cdecl PatchVerts::HasVert(int) const` | `?HasVert@PatchVerts@@QBA_NH@Z` |
| VIA-DC3 | 92 | 95.4 | AmbientOcclusion | `protected: class Triangle * __cdecl stlpmtx_std::vector<class Triangle, class st` | `?_M_erase@?$vector@VTriangle@@V?$StlNodeAlloc@VTriangle@@@stlpmtx_std@@@stlpmtx_` |
| VIA-DC3 | 72 | 93.3 | MicNull | `public: virtual short * __cdecl MicNull::GetRecentBuf(int &)` | `?GetRecentBuf@MicNull@@UAAPAFAAH@Z` |

Different name at the same retail body, DC3 at 100 (8 rows / 688 B; the
names differ because of ICF or a wrong map name, adjudicate before porting):

| tier | B | fuzzy | unit | our row | DC3 retail name (unique masked-hash twin) |
|---|---:|---:|---|---|---|
| IN-SOON | 176 | 99.9 | system/beatmatch/Submix | `protected: void __cdecl stlpmtx_std::vector<class stlpmtx_std::list<int, class s` | `?_M_insert_overflow_aux@?$vector@V?$vector@V?$ObjPtr@VUIColor@@@@V?$StlNodeAlloc` |
| IN-SOON | 8 | 0.0 | band3/game/BandPerformer | `public: void __cdecl DSP::Synapse::PitchCorrectedVoice::SetReleaseSmoothing(floa` | `?SetMinIntegrationTime@ExposureRecipe@TrueColor@@QAAXM@Z` |
| VIA-DC3 | 112 | 88.5 | MeshAnim | `private: void __cdecl stlpmtx_std::vector<class Key<class stlpmtx_std::vector<cl` | `?_M_fill_insert@?$vector@VSpotMeshEntry@SpotlightDrawer@@V?$StlNodeAlloc@VSpotMe` |
| VIA-DC3 | 96 | 0.0 | Sequence | `struct 'anonymous namespace'::Unlockable * __cdecl stlpmtx_std::__uninitialized_` | `??$__uninitialized_copy@PBVDataEvent@@PAV1@@stlpmtx_std@@YAPAVDataEvent@@PBV1@0P` |
| VIA-DC3 | 92 | 72.3 | MeshAnim | `public: void * __cdecl Key<class stlpmtx_std::vector<class Vector3, class stlpmt` | `??_GVocalEvent@MidiParser@@QAAPAXI@Z` |
| VIA-DC3 | 68 | 0.0 | Instance | `public: virtual void * __cdecl BandRetargetVignette::'vector deleting destructor` | `??_GSharedGroup@@UAAPAXI@Z` |
| VIA-DC3 | 68 | 0.0 | MeshAnim | `public: virtual void * __cdecl NetSession::'vector deleting destructor'(unsigned` | `??_GFlowSequence@@UAAPAXI@Z` |
| VIA-DC3 | 68 | 0.0 | CharIKHead | `public: virtual void * __cdecl CharIKHead::'vector deleting destructor'(unsigned` | `??_GCharIKHand@@UAAPAXI@Z` |

Same name, DC3 also below 100 (31 rows / 10,476 B, 3,500 B in scope; DC3
is no oracle here, listed so nobody re-derives it):

| tier | B | fuzzy | unit | our row | DC3 retail name (unique masked-hash twin) |
|---|---:|---:|---|---|---|
| IN-CORE | 680 | 74.8 | mtx | `void __cdecl Multiply(class Hmx::Matrix3const &, class Hmx::Matrix3const &, clas` | `?Multiply@@YAXABVMatrix3@Hmx@@0AAV12@@Z` |
| IN-CORE | 432 | 97.2 | Geo | `bool __cdecl Intersect(class Segment const &, class Triangle const &, bool, floa` | `?Intersect@@YA_NABVSegment@@ABVTriangle@@_NAAM@Z` |
| IN-CORE | 372 | 99.1 | Geo | `public: void __cdecl Sphere::GrowToContain(class Sphere const &)` | `?GrowToContain@Sphere@@QAAXABV1@@Z` |
| IN-CORE | 372 | 99.9 | Geo | `bool __cdecl operator>(class Sphere const &, class Frustum const &)` | `??O@YA_NABVSphere@@ABVFrustum@@@Z` |
| IN-CORE | 296 | 98.9 | Geo | `bool __cdecl Intersect(class Vector3const &, class Vector3const &, class Triangl` | `?Intersect@@YA_NABVVector3@@0ABVTriangle@@AAM@Z` |
| IN-CORE | 280 | 99.6 | Color | `void __cdecl InterpTangent(class Vector3const &, class Vector3const &, class Vec` | `?InterpTangent@@YAXABVVector3@@000MAAV1@@Z` |
| IN-CORE | 256 | 98.1 | Geo | `void __cdecl Multiply(class Plane const &, class Transform const &, class Plane ` | `?Multiply@@YAXABVPlane@@ABVTransform@@AAV1@@Z` |
| IN-CORE | 248 | 89.4 | Rot | `void __cdecl MakeRotQuat(class Vector3const &, class Vector3const &, class Hmx::` | `?MakeRotQuat@@YAXABVVector3@@0AAVQuat@Hmx@@@Z` |
| IN-CORE | 196 | 94.3 | Rot | `void __cdecl MakeScale(class Hmx::Matrix3const &, class Vector3&)` | `?MakeScale@@YAXABVMatrix3@Hmx@@AAVVector3@@@Z` |
| IN-CORE | 192 | 85.7 | Rot | `void __cdecl Multiply(class Vector3const &, class Hmx::Quat const &, class Vecto` | `?Multiply@@YAXABVVector3@@ABVQuat@Hmx@@AAV1@@Z` |
| IN-CORE | 88 | 0.0 | Msg | `private: struct stlpmtx_std::_Rb_tree_node_base * __cdecl stlpmtx_std::_Rb_tree<` | `??$_M_find@VSymbol@@@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@` |
| IN-SOON | 88 | 0.0 | band3/game/PerfectSectionTracker | `private: struct stlpmtx_std::_Rb_tree_node_base * __cdecl stlpmtx_std::_Rb_tree<` | `??$_M_find@H@?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHVSongStatus@@@2@U?$` |
| VIA-DC3 | 1344 | 93.3 | system/rndobj/Utl | `void __cdecl BuildVisit(class BSPNode *)` | `?BuildVisit@@YAXPAVBSPNode@@@Z` |
| VIA-DC3 | 944 | 85.8 | CharBonesSamples | `public: void __cdecl CharBonesSamples::EvaluateChannel(void *, int, int, float)` | `?EvaluateChannel@CharBonesSamples@@QAAXPAXHHM@Z` |
| VIA-DC3 | 632 | 99.9 | CompressionEffect | `public: void __cdecl CompressionEffect::Process(float *, int, int)` | `?Process@CompressionEffect@@QAAXPAMHH@Z` |
| VIA-DC3 | 544 | 63.4 | AmbientOcclusion | `public: static void __cdecl RndAmbientOcclusion::BlendVert(class RndMesh::Vert c` | `?BlendVert@RndAmbientOcclusion@@SAXABVVert@RndMesh@@0AAV23@@Z` |
| VIA-DC3 | 504 | 75.6 | Bitmap | `void __cdecl DecodeDxt5Alpha(unsigned char *, int, int, unsigned char &)` | `?DecodeDxt5Alpha@@YAXPAEHHAAE@Z` |
| VIA-DC3 | 396 | 97.1 | SpotlightDrawer_NG | `private: void __cdecl stlpmtx_std::vector<class SpotlightDrawer::SpotMeshEntry, ` | `?_M_fill_insert_aux@?$vector@VSpotMeshEntry@SpotlightDrawer@@V?$StlNodeAlloc@VSp` |
| VIA-DC3 | 340 | 99.8 | system/rndobj/Utl | `void __cdecl BuildSphereStratified(unsigned int, class stlpmtx_std::vector<class` | `?BuildSphereStratified@@YAXIAAV?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@stl` |
| VIA-DC3 | 332 | 81.3 | BoxMap | `private: void __cdecl BoxMapLighting::ApplyLight(class BoxLightArray<struct BoxM` | `?ApplyLight@BoxMapLighting@@ABAXABV?$BoxLightArray@ULightParams_Point@BoxMapLigh` |
| VIA-DC3 | 316 | 80.0 | BoxMap | `public: bool __cdecl BoxMapLighting::CacheData(struct BoxMapLighting::LightParam` | `?CacheData@BoxMapLighting@@QAA_NAAULightParams_Spot@1@@Z` |
| VIA-DC3 | 244 | 99.8 | ctr | `int __cdecl ctr_encrypt_fast(unsigned char const *, unsigned char *, unsigned lo` | `?ctr_encrypt_fast@@YAHPBEPAEKPAUSymmetric_CTR@@@Z` |
| VIA-DC3 | 232 | 96.6 | PostProc | `private: unsigned int __cdecl ProcCounter::SetEmulateFPS(int)` | `?SetEmulateFPS@ProcCounter@@AAAIH@Z` |
| VIA-DC3 | 216 | 79.5 | VelocityBuffer | `private: bool __cdecl RndXfmCache::CacheXfms(unsigned int *, class RndMesh &vola` | `?CacheXfms@RndXfmCache@@AAA_NPIBVRndMesh@@PIBMIAAI@Z` |
| VIA-DC3 | 184 | 94.4 | AmbientOcclusion | `protected: void __cdecl RndAmbientOcclusion::TransformNormal(class Vector3const ` | `?TransformNormal@RndAmbientOcclusion@@IBAXABVVector3@@ABVMatrix3@Hmx@@AAV2@@Z` |
| VIA-DC3 | 176 | 73.6 | AmbientOcclusion | `protected: float __cdecl RndAmbientOcclusion::DistanceSH(class Vector4const &, c` | `?DistanceSH@RndAmbientOcclusion@@IBAMABVVector4@@ABVVector3@@01@Z` |
| VIA-DC3 | 164 | 94.7 | Mesh | `class Vector3 __cdecl TransformNormal(class Vector3const &, class Hmx::Matrix3co` | `?TransformNormal@@YA?AVVector3@@ABV1@ABVMatrix3@Hmx@@@Z` |
| VIA-DC3 | 124 | 93.5 | system/rndobj/Rnd | `protected: class DataNode __cdecl Rnd::OnToggleHeap(class DataArray const *)` | `?OnToggleHeap@Rnd@@IAA?AVDataNode@@PBVDataArray@@@Z` |
| VIA-DC3 | 96 | 83.1 | VelocityBuffer | `private: bool __cdecl RndXfmCache::GetXfms(unsigned int *, class RndMesh &volati` | `?GetXfms@RndXfmCache@@ABA_NPIBVRndMesh@@IIAAPBM@Z` |
| VIA-DC3 | 96 | 87.3 | SpotlightDrawer_NG | `protected: class SpotlightDrawer::SpotMeshEntry * __cdecl stlpmtx_std::vector<cl` | `?_M_erase@?$vector@VSpotMeshEntry@SpotlightDrawer@@V?$StlNodeAlloc@VSpotMeshEntr` |
| VIA-DC3 | 92 | 21.5 | StandardStream | `public: struct JumpInstance * __cdecl stlpmtx_std::vector<struct JumpInstance, c` | `?erase@?$vector@UJumpInstance@@V?$StlNodeAlloc@UJumpInstance@@@stlpmtx_std@@@stl` |

### 3.6 Static-initializer region (#6)

`0x82C3F000` onward holds every TU's `??__E` dynamic initializers. Their
bodies are near-identical (`lis/addi/bl ctor/…/bl atexit`), so a masked hash
matches some DC3 initializer by accident. Both DC3 names that hit here are
false: `0x82C40D68` "`BinkInit`" is beatmatch's `LogFile`
initializer (rb3 Wii: `LogFile TheBeatMatchOutput("beatmatch-%05d.rec");` in
`beatmatch/Output.cpp`, a TU we do not compile), and `0x82C42910`
"`Timer::ClearSlowFrame`" initializes a vtable pointer.

60 anon rows / 4,508 B, 42 / 3,384 B IN-CORE, pinned into units whose span
covers other TUs' initializers (UsbMidiGuitar, BlockMgr, DataNode, FilePath,
System). Identify each by the string and ctor its relocations reach, re-pin
to the owning TU, and add `beatmatch/Output.cpp`:

| row | B | fuzzy | tier | pinned unit |
|---|---:|---:|---|---|
| `fn_82C3F080` | 12 | 98.3 | VIA-DC3 | PropKeys |
| `fn_82C3F8F0` | 12 | 98.3 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3F900` | 84 | 0.0 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3F9D8` | 72 | 0.0 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FA20` | 12 | 98.3 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FA30` | 40 | 0.0 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FA60` | 52 | 0.0 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FA94` | 40 | 78.5 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FAC0` | 56 | 0.0 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FAF8` | 52 | 99.6 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FB30` | 12 | 98.3 | IN-CORE | system/os/UsbMidiGuitar |
| `fn_82C3FC28` | 52 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FC60` | 12 | 98.3 | IN-CORE | BlockMgr |
| `fn_82C3FC70` | 12 | 98.3 | IN-CORE | BlockMgr |
| `fn_82C3FC80` | 52 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FCB8` | 56 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FCF0` | 56 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FD28` | 52 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FD60` | 172 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FE10` | 132 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FE98` | 52 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FED0` | 16 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FEE0` | 56 | 0.0 | IN-CORE | BlockMgr |
| `fn_82C3FFB0` | 12 | 98.3 | IN-CORE | DataNode |
| `fn_82C3FFC0` | 12 | 98.3 | IN-CORE | DataNode |
| `fn_82C40080` | 20 | 0.0 | IN-CORE | DataNode |
| `fn_82C40098` | 1372 | 0.0 | IN-CORE | DataNode |
| `fn_82C405F8` | 52 | 0.0 | IN-CORE | DataNode |
| `fn_82C40630` | 72 | 0.0 | IN-CORE | DataNode |
| `fn_82C40678` | 12 | 98.3 | IN-CORE | DataNode |
| `fn_82C40688` | 52 | 0.0 | IN-CORE | DataNode |
| `fn_82C40718` | 52 | 0.0 | IN-CORE | DataNode |
| `fn_82C40898` | 76 | 0.0 | IN-CORE | DataNode |
| `fn_82C40A20` | 12 | 98.3 | IN-CORE | DataNode |
| `fn_82C40BF8` | 52 | 0.0 | IN-CORE | FilePath |
| `fn_82C40C30` | 20 | 0.0 | IN-CORE | FilePath |
| `fn_82C40C48` | 84 | 0.0 | IN-CORE | FilePath |
| `fn_82C40CA0` | 132 | 0.0 | IN-CORE | FilePath |
| `fn_82C40D28` | 12 | 98.3 | IN-CORE | FilePath |
| `fn_82C40D38` | 44 | 0.0 | IN-CORE | FilePath |
| `fn_82C40D68` | 60 | 0.0 | IN-CORE | FilePath |
| `fn_82C40DA8` | 12 | 98.3 | IN-CORE | FilePath |
| `fn_82C42910` | 72 | 0.0 | IN-CORE | System |
| `fn_82C42958` | 68 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C429A0` | 72 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C429E8` | 52 | 99.2 | OUT-360-OTHER | Voice |
| `fn_82C42A20` | 72 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42A68` | 96 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42AC8` | 72 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42B10` | 52 | 99.2 | OUT-360-OTHER | Voice |
| `fn_82C42B48` | 72 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42B90` | 52 | 99.2 | OUT-360-OTHER | Voice |
| `fn_82C42BC8` | 72 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42C10` | 60 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42C50` | 64 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42C98` | 92 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42CF4` | 40 | 93.9 | OUT-360-OTHER | Voice |
| `fn_82C42D20` | 72 | 0.0 | OUT-360-OTHER | Voice |
| `fn_82C42D68` | 52 | 99.6 | OUT-360-OTHER | Voice |
| `fn_82C43780` | 52 | 0.0 | VIA-DC3 | SpotlightDrawer |

### 3.7 DC3 map as a fold witness (#7)

Six name-charge pairs where DC3's map puts retail's spelling and ours at one
address (7 rows / 796 B). DC3 folding two spellings shows they *can* fold, not
that RB3's call site meant ours. W16-PH requires a retail type witness, so
none was admitted:

| B | unit | row | retail target | our callee | existing group |
|---:|---|---|---|---|---|
| 80 | NetCacheMgr | `fn_827CE740`, `fn_827CE7F8` | `??1FilePath@@UAA@XZ` | `??1NetLoaderRef@@QAA@XZ` | `0x827bea28` (9 folded) |
| 112 | SpotlightDrawer | `sort<SpotlightEntry,ByColor>` | `__introsort_loop<CameraManager::Category>` | `__introsort_loop<SpotlightEntry,ByColor>` | none |
| 384 | Trans | `RndTransformable::DistributeChildren` | `sort<RndPollable*>` | `sort<RndTransformable*>` | `0x822bed18` (4 folded) |
| 40 | DataFunc | `fn_82763988` | `??1MergeFilter@@UAA@XZ` | `??1DataMergeFilter@@UAA@XZ` | none |
| 84 | DataNode | `_Rb_tree<Symbol,DataNode>` row | `_Copy_Construct<ScriptTask::Var>` | `_Copy_Construct<pair<Symbol,DataNode>>` | ours already in a group |
| 96 | Character | `__uninitialized_copy<Character::Lod>` | `_Copy_Construct<Character::Lod>` | `_Param_Construct<Character::Lod>` | none |

### 3.8 STLport (#8)

`src/system/stlport/` is identical to dc3-decomp's. Nothing to import.

### 3.9 Relocation-name charges, for scale

704 gap rows (108,068 B; 24,748 B in scope) carry a `bl` whose target name
differs. Of the 309 distinct pairs, 91 are CHASED-PROVEN folds already under
the W16-PH type-witness rule; 48 rows / 23,376 B and 35 / 14,256 B are
masked-equal fold shapes; 163 rows / 9,912 B have no evidence either way. DC3
adds only §3.7's six pairs to this.

## 4. Predictions vs measurements

| A/B | prediction | measured | kept |
|---|---|---|---|
| VocalPlayer phantom names | +3 / +428 B | **+3 / +428 B** | yes `f5995339b` |
| flank batch 1, first try | +3 / +184 B | +2 / +180 B (thunk target) | fixed |
| flank batch 1 | +4 / +192 B | **+4 / +192 B** | yes `d12579f9f` |
| flank batch 2 (with 8 B names) | positive | **−1 / −5,584 B** (`none` +208 B) | no, names withdrawn |
| FastSort<3> name | Δ0 / Δ0, fuzzy up | **Δ0 / Δ0**, +0.000587 pp fuzzy | yes `c828ef19b` |
| flank batch 3, all names | +3 / +264 B | **−1 / −648 B** | no |
| flank batch 3, SyncMeshCB name withdrawn | +1 / +176 B | **+1 / +176 B** | yes `dc27de352` |
| whole lane on main `a0003899e` | +8 / +796 B | **+8 / +796 B**, 2 rows down (4 B thunk, Locale::Init 0 B) | landing |
| DC3 inline `DataArray::Release` (control) | negative | **Δ0 / Δ0 B, 1,034 recompiles** | no, reverted |

## 5. Not done

- No `src/system` body edits (W16-QA, W16-PU).
- No alias admissions from DC3's map (§3.7).
- The 36 flank rows and 47 phantom-name rows left in §3.1–3.2 were not all
  worked: each needs its callers checked for fold bait, which is per-row work
  for a follow-up lane.
- No init-region identification (§3.6); it needs relocation reading, not DC3.
- Quazal rows appear in the tables for completeness; per the standing
  directive none should be funded.

Scratch data (not committed): `~/tmp/w16qb/` (`gap_t.json`,
`flankmatch.json`, `adj_flank2.log`, `dc3hash_classes.json`, `imm.json`,
`callpos.json`, `nameadj2.json`). A/B run dirs:
`~/tmp/wt-w16qb/.ab_measure_runs/`.
