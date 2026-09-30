# W16-HY — engine (`src/system/`) 1–90% fuzzy band, third pass

**Branch** `w16-hy`, rebased onto main `fcde67a1e`. **Ruler** `name_check` (graded,
from `report.json` `provenance.diff_config`). Continues W16-HM and W16-HP.
**Population:** named rows in `src/system/` units with `1 ≤ fuzzy < 90`, outside
`src/system/{bandobj,rndobj,char,rnddx9}` and the `obj/` template families
(ObjPtr/ObjVector/ObjOwnerPtr/ObjRef*/ObjDirPtr names). Ranked by
`size × (100 − fuzzy)` from the worktree's own built `report.json`: **234 rows** at the
start (`~/tmp/w16hy/rank.py`).

The work was split by directory. I took obj/ (non-template), ui/, beatmatch/ and
hamobj/. Four forks took the rest, each in its own worktree and branch:
M = math, utl, dsp, flow · S = synth, synth_xbox, midi, oggvorbis ·
W = world, track, gesture · O = os, net, meta.
Each fork was rebased onto `w16-hy` and merged with `--no-ff`, so its per-fix commits
are kept. The rule for every edit: run a full `./tools/ninja-locked` build, diff the
report snapshot against the previous build, and keep the edit only if no row dropped.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hy-base --patch <git diff main..w16-hy>`.
Leg A is main `fcde67a1e`. There was one run, over the whole branch diff: 67 files, patch
kinds map + source. objdiff-cli was pinned across both legs. Leg A settled with 0
recompiles; leg B had 523 recompiles and 2 settle iterations.

```
leg A: matched=46180 masked=23658 honest=22522 code%=45.384026
leg B: matched=46261 masked=23667 honest=22594 code%=45.507070
Δmatched=+81  Δmasked_equal=+9  Δhonest=+72  Δcode%=+0.123044pp  Δcode_bytes=+12608
Δfuzzy=+0.072998pp   (legA 54.259537 -> legB 54.332535)
units at 100% [mpn]: 230 -> 235 (5 reached, 0 fell off)   [all-rows-fuzzy]: 201 -> 206
```

**Prediction, written before the run:** net positive, about +80 functions and about
+0.07 pp fuzzy, with no row down except GONE/NEW pairs from map renames.
**It held.**

**Row-level diff of the two archived leg reports: 111 rows up, 0 rows down.** The six
"DOWN" lines in the diff are all GONE rows whose address now carries a new name, and
every one of those new rows reads 100 in leg B:

| GONE row | now named | leg B |
|---|---|---:|
| `XboxContentMgr::XboxContentMgr` (23.52) | `ContentMgr::ContentMgr` | 100 |
| `ReceiveUpstreamEEPROMWriteResponse` (6.71) | `ReceiveUpstreamBreedDataResponse` | 100 |
| `MidiReader::ReadNextEventImpl` (47.06) | `MidiReader::ReadNextEvent` | 100 |
| `fn_8274E7E8` | `ObjectDir::SaveProxy` | 100 |
| `fn_8278DDF8` | `TrimExcess<GameGem>` | 100 |
| `fn_8278DEE8` | `GameGemList::Finalize` | 100 |

The lane tip reproduces leg B exactly (46,261 / 54.332535). The validators also pass:
- `tools/map_name_injectivity.py`: OK, 30,922 applied rows, injective.
- `tools/icf_alias_finder.py --validate`: PASS, 1,481 map-consistent, 0 contradicted.

`symbols.txt` is untouched.

## 2. Rows, before → after

Before is leg A and after is leg B, both as `fuzzy_match_percent`. A **bold** after
value means the row reached 100. The table lists the band rows plus the neighbours
that moved with them.

### obj/ (mine)

| row | B | before | after | fix |
|---|---:|---:|---:|---|
| `DirLoader::SetupDir` | 440 | 21.54 | **100** | RB3 shape: one discarded `MakeString`, no proxy-dir branch, no MemPoint tracking |
| `PreloadSharedSubdirs` | 292 | 24.81 | **100** | rb3-Wii flat loop; `FilePath` built before `gPreloadIdx` is read (DC3 recursive `PreloadArray` native-only) |
| `DirLoader::SaveObjects(const char*)` | 252 | 78.24 | **100** | constant cache platform (Xbox/PC), 0x20000 buffer, no `gNullFiles`, no notify on failed open |
| `ReplaceObject` | 216 | 73.52 | 96.70 | no null test on `to`; the ring is drained inline through each owner's `Replace` |
| `DataMin` / `DataMax` | 188 | 43.38 / 43.28 | **100** | rb3-Wii two-operand forms (DC3 variadic native-only) |
| `DataMultiply` | 176 | 20.02 | **100** | same |
| `Object::SaveRest` | 176 | 4.61 | **100** | `TypeProps::Save` unconditional; only `mNote`'s pointer tested; string write inlined (strlen, int, bytes) |
| `Object::OnSet` | 172 | 48.21 | **100** | no parity assert, no type test on the array arm; symbol word read after `Evaluate(i+1)` |
| `ObjectDir::HasDirPtrs` | 144 | 48.47 | **100** | ring walk only, no `sDeleting` short-circuit |
| `ObjectDir::SaveProxy` (named) | 136 | 0 (`fn_`) | **100** | named from retail bytes (§4.2); no save for an empty proxy file, no Root fallback |
| `DataHandleRet` | 136 | 43.18 | **100** | no not-found test |
| `Object::RemoveFromDir` | 132 | 41.70 | **100** | no entry check before the store |
| `TypeProps::ReplaceObject` | 120 | 48.40 | **100** | the ring ref moves with the value (`from->Release(this)`, `to->AddRef(this)`) |
| `ObjectDir::Reserve` | 88 | 43.55 | **100** | no `MemTemp` |
| funclet `fn_8274E870` | 40 | 99.5 | **100** | re-paired once `SaveProxy` was named (§4.2) |

### ui/ (mine)

| row | B | before | after | fix |
|---|---:|---:|---:|---|
| `UIScreen::ReenterScreen` | 208 | 69.77 | **100** | no `AutoGlitchReport` |
| `UIListSlot::Load` | 132 | 42.52 | **100** | RB3 rev dialect (two initialised TU shorts, raw stream) |
| `UISlider::Save` | 112 | 78.54 | **100** | rev 1, no `mVertical` (DC3 rev-3 field) |
| `UIButton::PreLoad` | 108 | 30.48 | **100** | RB3 rev dialect |
| `UIPicture::PostLoad` | 76 | 73.68 | **100** | no edit-mode test |

### beatmatch/ (mine)

| row | B | before | after | fix |
|---|---:|---:|---:|---|
| `GameGemList::MergeChordGems` (was "Finalize") | 320 | 19.34 | **100** | map correction (§4.1), `size() == 0` test, OR through `GetSlots()` |
| `GameGemList::Finalize` (named) | 8 | — | **100** | `TrimExcess(mGems)` |
| `TrimExcess<GameGem>` (named) | 108 | 0 (`fn_`) | **100** | named |
| `KeyboardTrackWatcherImpl::CheckForFatFinger` | 300 | 84.60 | 95.73 | copies the `mPlayed` bit directly (no `!= 0` normalisation) |
| `VocalNoteList::GenerateLegalFreestyleSections` | 200 | 57.54 | **100** | iterator loop against `end()`; next start = ms + pad + duration |
| `VocalNoteList::NextNote` | 168 | 76.36 | **100** | `empty()` guard; `--it` / `++it` instead of `[-1]` |
| `VocalNoteList::GetNumPracticePhrases` | 140 | 73.37 | **100** | iterator loop against `end()` |
| `TickedInfoCollection<String>::CopyFrom` | 136 | 80.41 | **100** | `reserve(other.size())` before the range insert (`utl/TickedInfo.h`) |
| `RealGuitarTrackWatcherImpl::InTrill` | 112 | 85.71 | **100** | args inline, so the tick is computed before `Track()` is read |
| `PhraseData` ctor | 92 | 60.13 | **100** | loop bound `DIM()` (unsigned, `cmplwi`) |
| `PhraseAnalyzer::NumPhrases(int)` | 88 | 48.36 | **100** | `1 << mask` hoisted above the loop |
| `SongParser::HandleFillEnd` | 84 | 87.38 | **100** | explicit `!= 0` on the callee's bool (§4.3) |
| `SongParser::OnMidiMessageCommonOff` | 80 | 85.00 | **100** | same |
| `BeatMaster::IsLoaded` | 68 | 72.29 | **100** | `return a && b;`, no bool local |
| `MasterAudio::Fail` | 40 | 53.00 | **100** | null case first |
| `SongData::OnEndOfTrack` (caller) | 52 | 99.62 | **100** | follows the GameGemDB rename |
| funclets `fn_82774D18`, `fn_82772CF4` | 44 | 65.18 / 99.55 | 80.73 / **100** | follow CopyFrom |

### Fork M (math, utl)

| row | B | before | after |
|---|---:|---:|---:|
| `Locale::Init` | 1304 | 89.58 | 95.94 |
| `MemTrackHeapDump` | 312 | 98.08 | **100** |
| `MemPrintOverview` | 300 | 93.77 | 99.91 |
| `MemTruncate` | 284 | 86.34 | 87.73 |
| `FastInvert` | 232 | 30.53 | 99.50 |
| `MemTrackReport` | 232 | 60.17 | **100** |
| `MemAllocSize` | 232 | 29.43 | 90.84 |
| `MemFree` | 188 | 48.55 | 86.49 |
| `MemFindHeap` | 184 | 44.11 | 99.54 |
| `PrintAlloc` (MemHeap) | 180 | 60.40 | **100** |
| `NetCacheMgr::Poll` | 176 | 29.80 | **100** |
| `DataPoint::AddPair(Symbol, DataNode)` | 164 | 51.98 | 89.02 |
| `BufStream::ReadImpl` | 164 | 72.44 | 99.76 |
| `FormatString << float` | 128 | 75.63 | **100** |
| `FormatString << String` | 124 | 69.61 | **100** |
| `MemTrackLogDF` | 124 | 95.97 | **100** |
| `RandomFloat(float, float)` | 108 | 55.37 | **100** |
| `LoadMgr::RegisterFactory` | 96 | 42.58 | **100** |
| `OnSysPlatformSym` | 72 | 85.56 | **100** |
| funclets `fn_827BC758`, `fn_827CD434` | 40 | 99.9 / 99.5 | **100** |

Behaviour fixes:
- `MemTruncate` returned null after a successful heap truncate.
- `NetCacheMgr::SetState` now takes retail's recursive form.

### Fork S (synth, synth_xbox, midi)

| row | B | before | after |
|---|---:|---:|---:|
| `StandardStream::InitInfo` (moved with `MsToSamp`) | 872 | 97.17 | 98.75 |
| `PitchDetector::Detect` | 676 | 79.54 | 92.62 |
| `SpectralAnalysis::Analyze` | 496 | 70.40 | 70.72 |
| `StandardStream::setJumpSamplesFromMs` | 460 | 29.63 | **100** |
| `VorbisReader::TryDecode` | 336 | 88.44 | 98.68 |
| `SynthEmitter::Load` / `Save` | 288 / 244 | 73.69 / 84.16 | **100** / **100** |
| `MidiReader::ReadNextEvent` (renamed from `ReadNextEventImpl`) | 136 | 47.06 | **100** |
| `SynthSample` ctor | 124 | 81.94 | **100** |
| `MidiReader::ReadSomeEvents` / `ReadTrack` | 96 / 92 | 99.79 / 99.78 | **100** |
| `NoteVoiceInst::Start` | 80 | 74.70 | **100** |
| `SynapseAPO::DoProcess` (was an empty stub) | 24 | 16.67 | **100** |
| four StandardStream funclets | 64 | 0 | **100** |

### Fork W (world, track)

| row | B | before | after |
|---|---:|---:|---:|
| `WorldCrowd::DrawShowing` | 2072 | 86.98 | 88.67 |
| `WorldCrowd::Draw3DChars` | 1012 | 29.88 | **100** |
| `CamShot` ctor | 968 | 78.87 | 86.99 |
| `NgSpotlightDrawer::RenderBeams` | 416 | 88.25 | **100** |
| `TrackWidget::SyncImp` | 392 | 40.82 | 97.69 |
| `TrackDir::SetupKeyShifting` | 384 | 68.93 | **100** |
| `SharedGroup::TryEnter` | 224 | 53.54 | **100** |
| `Spotlight::Generate` | 140 | 10.31 | **100** |
| `WorldCrowd::CharData::Char3D` copy ctor | 84 | 15.33 | 99.76 |
| funclets (CameraShot ×3, Crowd, TrackWidget ×2, Shockwave) | 40–44 | 93.5–99.5 | 99.4–100 |

In `Draw3DChars`, the Character flag offsets were wrong: ours used +0x252/+0x251, and
retail uses +0x210/+0x212/+0x211.

### Fork O (os, net, meta)

| row | B | before | after |
|---|---:|---:|---:|
| `XLSPConnection::SetState` | 328 | 64.56 | 68.63 |
| `AsyncFileWin::_OpenAsync` | 324 | 67.63 | 95.56 |
| `JoypadClient::Init` | 256 | 77.11 | 99.92 |
| `FixedSizeSaveable::SaveFixedSymbol` | 200 | 57.46 | **100** |
| `AsyncFileWin::_ReadDone` | 188 | 42.13 | **100** |
| `ContentMgr::ContentMgr` (renamed, §4.1) | 184 | 23.52 | **100** |
| `SongPreview::PrepareFaders` | 168 | 33.57 | **100** |
| `ReceiveUpstreamBreedDataResponse` (renamed) | 164 | 6.71 | **100** |
| `StorePanel::HandleNetCacheLoaderFailure` | 164 | 47.20 | 99.88 |
| `FileQualifiedFilename` | 144 | 60.36 | **100** |
| `ReceiveUpstreamResponse` | 140 | 99.86 | **100** |
| `Achievements::Submit` | 136 | 30.88 | 93.38 |
| `CDReadDone` | 136 | 67.35 | **100** |
| `FixedSizeSaveable::PadStream` / `DepadStream` | 120 / 104 | 74.47 / 75.77 | **100** |
| `PlatformMgr::ShowPartyUI` / `ShowOfferUI` / `ShowFriendsUI` | 92 / 72 / 60 | 51.96 / 22.11 / 26.53 | **100** |

Behaviour fixes:
- `PrepareFaders` now sends crowd channels to the crowd-sing fader.
- `HandleNetCacheLoaderFailure`'s store-server case falls into the default arm.

## 3. Examined and left

**Wrong map names.** The retail body cannot be the named function. The map is
untouched for these, because removing a name costs fuzzy and naming one needs an
identification:
- `~vector<SongPattern>` (fill-insert code)
- `DataArray::Sym` in FilterVersion (a FilePath routine)
- `TransformArea` copy ctor
- `_M_insert_overflow<Symbol>` in DirLoader (a `this−0x1c` deleting thunk)
- `CameraManager::OnRandomSeed` in MoveMgr
- `Cheat` ctor (vendor code)
- fork M: `operator delete` @`0x82BC6B70`, `NetLoaderRef` copy ctor,
  `__uninitialized_copy<TimeSigChange>`, `CritSecTracker` ctor, and FlowNode's
  `DrivenPropertyEntry` fill (bandobj code pinned in flow/)
- fork S: `erase<JumpInstance>`, `SampleMarker` ctor, `FxSendCompress360` ctor,
  `ResMgr<void>::Get`, `_Destroy_Range<PitchCorrectedVoice>`, `allocate<LevelData>`
- fork W: `__uninitialized_copy<CamShotFrame>`, SpeechMgr `__uninitialized_copy<Grammar>`,
  DepthBuffer3D `_M_insert_overflow`
- fork O: `~Queue`, `MakeSessionJob` ctor, `DataNode::Obj<CharPollable>`

W also tried renaming `resize<SpotlightEntry>` to `resize<Spotlight*>`. It dropped the
row to 43.0 and knocked three callers off 100, because the current name is covering a
fold, so W reverted it.

**Carve artifacts.** Retail's extent excludes a tail our body includes, so only a
`symbols.txt` edit could fix these, and `ab_measure` refuses those:
`GameGem::RightHandTap` (the tail is carved as `fn_8278EB58`/`fn_8278EB64`),
`BeatMatcher::SetSyncOffset`, `ADSRImpl` ctor, `OggFree`.

**Out-of-scope headers.** These need `obj/` templates, `rndobj/` or `char/`:
- `UITransitionHandler` ctor, `UIPicture::HookupMesh` and `Spotlight` ctor: retail
  inlines the ObjPtr ctor and null-assign.
- `UIFontImporter::OnGetGennedBitmapPath`: RndFont layout.
- `LabelShrinkWrapper::UpdateAndDrawWrapper`: inline `SetLocalPos`.
- `Spotlight::DrawShowing` and `SpotlightDrawer::DrawShadow`: `Character::DrawShadow`.
- `ObjectDir::NextSubDir`: inline in `Dir.h`, and ObjDirItr inlines it everywhere.
- `Object::Object`: the `mRefs` init goes through a zeroed 8-byte temp, in `Object.h`.

**Tried and reverted.** Each was a Δ0 or worse spelling:
- `MasterAudio::FillSwing` / `SetButtonMashingMode`: retail keeps the vector base and
  the index across `GetTime()`; the rb3-Wii expression form scored 61.5 → 53.1.
- `CheckForFatFinger`'s inlined `TrackForgivesFatFingering` compare: three spellings.
- `SongData::TrimOverlappingGems`: retail calls the const `GetDiffGemList`, which is a
  fold name.
- fork S: `StreamReceiver360::Tag` (80.3 → 53.0), `MoggClip::SetupPanInfo`.
- fork M: `ThreadMemStack`, `Rand::Seed`, `MemCurrentHeap`.

**Reverted to meet the "no regressed rows" condition:**
`SpotlightDrawer::DrawAdditional` in-place walk (fork W, 61.41 → 100,
behaviour-identical). SpotlightDrawer.cpp is scatter-included into CameraManager.cpp.
The old by-value list copy emitted an EH funclet that was the byte-signature partner of
retail's anonymous `fn_825AD6D4` (40 B, masked_equal). Walking the list in place drops
that pairing from 99.3 to 93.9. The funclet's real owner is `fn_825AD4F8`, an
unidentified `Handle()` that answers only ButtonDownMsg and ButtonUpMsg inside the
CameraManager pin. Once that function is named and ported, the DrawAdditional fix can
come back.

**Not examined:**
- the DSP/FFT/Geo float tails
- `CSHA1::Transform` and `Multiply(Matrix3)` (HM measured them as scheduling-bound)
- hamobj `MeterDisplay::DrawShowing` and the SongLayout/SongCollision template rows
- ui `UIListState::Scroll`, `UILabel::{UpdateAndDrawHighlightMesh, LabelUpdate,
  SetTokenFmtImp}` (the last needs RB3's 3-argument `SuperFormatString`),
  `UIFontImporter::OnSyncWithResourceFile`, `~Screenshot`
- fork O's unexamined list: `UsbMidiKeyboard::Poll`, `ReadSingleXinputJoypad`,
  `XboxMapFile`, StorePanel `Handle`/`UpdateOffers`/`PopulateOffers`, and others

## 4. Findings worth reusing

### 4.1 Four map names settled from retail's call graph
- **GameGemDB's two loops were swapped.** `0x82793168` is called from SongData's
  post-load loop over `mGemDBs` (+0xb0), immediately before `mTempoMap->Finalize()`, so
  it is `GameGemDB::Finalize`. It calls `0x8278DEE8`, which is
  `addi r3,r3,4; b 0x8278DDF8`, and `0x8278DDF8` is `TrimExcess<GameGem>`
  (copy-construct, swap, destroy). That makes `0x8278DEE8` `GameGemList::Finalize`.
  `0x82793108` is `GameGemDB::MergeChordGems`, and it calls `0x8278E200`, whose body is
  the chord-merge loop. W16-HS's caller vote for "Finalize" at `0x8278E200` rested on
  the swapped DB names. Both DB rows read 100 either way, because each side was
  consistently wrong, which is why nothing flagged it.
- `0x825299B8` is `ReceiveUpstreamBreedDataResponse`: only the 0x82 case of retail's
  dispatcher calls it (fork O).
- `0x825213D0` is `ContentMgr::ContentMgr`: XboxContentMgr's initializer calls it and
  then stores its own vtable. `sizeof(ContentMgr)` is 0x74 (fork O).

### 4.2 A re-paired funclet can point to its parent
Removing `Reserve`'s `MemTemp` dropped the anonymous funclet `fn_8274E870` from 99.5 to
99.4. Its parent `fn_8274E7E8` turned out to be `ObjectDir::SaveProxy`, which was
anonymous. Naming it and porting the retail body brought the funclet to 100. The same
check is what exposed the DrawAdditional case in §3, where the parent (`fn_825AD4F8`)
could not be identified in this lane.

### 4.3 `!= 0` on a bool call result makes MSVC normalise it
Retail's `clrlwi; subic; subfe` after a `bl` to a bool-returning callee is reproduced
by writing `f() != 0`. MSVC does not drop the explicit compare. It closed two SongParser
rows. The reverse also holds: `CheckForFatFinger`'s `srwi 7` bit copy needs the raw
bitfield, not `GetPlayed()` (`mPlayed != 0`).

### 4.4 RB3 uses `vector.begin()..end()` loops where our DC3 port used `data() + size()`
The `data() + size()` spelling rebuilds end as `size * stride`, a divide and a multiply,
and retail never does that. Three VocalNoteList rows went to 100 on this alone. Two more
functions in the file, `GetPracticePhrases` and `GetPracticePhrases2`, have the same
spelling but are not mapped yet.

## 5. What I did not do

- Nothing in `src/system/{bandobj,rndobj,char,rnddx9}` or in the `obj/` template
  headers. `utl/TickedInfo.h` (a utl template) and `os/ContentMgr*.h` are the only
  headers touched outside the fork-owned directories.
- No `symbols.txt` edits and no new alias groups.
- Map edits, all with retail-byte evidence:
  - the GameGem set (§4.1, four rows plus the `TrimExcess<GameGem>` name)
  - `SaveProxy`
  - fork S's `ReadNextEvent` rename
  - fork O's two renames
- LightPreset.cpp: untouched. It was fenced for another session for part of the run.
  Its rows (`Copy` 37.7, `Load` 86.4, `SetFrameEx` 83.6) are still open, and `Copy`
  still needs the AddRef/Release ref model that HM noted.
- Permuter not run (standing directive).

## 6. Native gate

`tools/native_build_gate.sh` on the lane tip, run after the A/B as the last build action:

```
NATIVE_GATE_RESULT (filled in below)
```
