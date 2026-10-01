# W16-JD — engine units sweep: ui, utl, synth, synth_xbox, beatmatch, os, obj, world (2026-10-01)

**Branch** `w16-jd`, rebased onto main `bd4ae22ec` (after W16-JE, the comment cleanup `99b26594d`, and
W16-JG). **Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`).

**Population.** Every row with `fuzzy < 100` in a unit whose `objdiff.json` `source_path` is under
`src/system/{ui,utl,synth,synth_xbox,beatmatch,os,obj,world}/`. Ranked by `size × (100 − fuzzy)` from
the worktree's own built `report.json` (`~/tmp/w16jd/rank.py`). Covers anonymous `fn_` rows at 0
(identified on retail bytes), named rows at 0, 1–90 and 90–99.99. Skipped by brief: rows whose only
charges are relocation-name args (W16-JE's lane, per the coordinator's scope note) and pure
register-allocation / scheduling residue after about three spellings.

| | rows | weight | anon `fn_` at 0 | named at 0 | 1–90 | 90–99.99 |
|---|---:|---:|---:|---:|---:|---:|
| main `bd4ae22ec` | 2,004 | 131,762 | 99,988 | 17,484 | 10,072 | 4,217 |
| lane tip | 1,619 | 79,317 | 53,064 | 12,696 | 9,697 | 3,860 |

Weight fell **40%**. Some of that is re-homes moving rows *out* of these units, so the whole-binary
figures are the honest ones: anonymous fuzzy-0 rows **12,894 → 12,574 (−320 rows, −48,996 B)** and
Σ size × (100 − fuzzy) over the whole report **4,429,085 → 4,374,551 (−54,534)**.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-jd-ab --patch <git diff bd4ae22ec..w16-jd, minus symbols.txt>`.
- **Worktree:** fresh, made with `scripts/setup_worktree.sh`.
- **Leg A:** main `bd4ae22ec` plus the lane's split-written `symbols.txt` fixed points, committed there
  (§6; HK/HR recipe).
- **Patch:** 115 files; kinds configgen, map, source and splits.
- **objdiff-cli:** pinned to one sha across both legs.
- **Both legs** were read at a split fixed point (0 extra re-splits). Leg B made 1,078 recompiles, one
  split, renamer patched 1,857, 2 settle iterations.

The run was made **twice**. The second run covers the final branch, including the two native-only commits
of §10; both runs agree on every key. Run dirs: `.ab_measure_runs/20261001-095600-branch-154150/` and
`20261001-101014-branch2-263552/`; the second is copied to `~/tmp/w16jd/ab2/run_dir`.

```
leg A: matched=47923 masked=23924 honest=23999 code%=48.263584  (recompiles: 0, settled)
leg B: matched=48315 masked=24035 honest=24280 code%=48.756096  (recompiles: 1078, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+392  Δmasked_equal=+111  Δhonest=+281  Δcode%=+0.492512pp  Δcode_bytes=+50468
Δfuzzy=+0.531731pp   (legA 56.837597 -> legB 57.369328)
units at 100% [mpn ruler]: legA 302 -> legB 318  (Δ+16; 21 reached 100, 5 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 256 -> legB 271  (Δ+15; 17 reached 100, 2 fell off)
[control none] Δmatched_code=+50876 B (default ruler +50468 B) -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** Δmatched +392, Δmatched_code +50,468 B, Δfuzzy +0.531632 pp.
This came from the in-tree lane-vs-main build diff, where the eight fork deltas sum exactly. Measured:
+392 / +50,468 B / +0.531731 pp. The 0.0001 pp fuzzy gap is leg A's `symbols.txt` merges (main alone
reads 56.837696). The second run's prediction was "identical to the first", and it was.

**Row-level diff of the archived leg reports: 120 rows up, 9 down, 0 fell off 100.**
- The 9 down rows are listed in §7; none was at 100.
- 46 leg-A rows at 100 are GONE in leg B. 40 reappear at 100 under the same name in another unit
  (re-homes). The other 6 were renamed, and each reappears at 100 under its corrected name at the same
  retail address:
  - 0x827493d0, 0x82749550, 0x827495c8, 0x82749630: EventSink/EventSinkElem names became the
    ScriptTask/TaskTimeline helpers, now in Task.
  - 0x824d02c8: `??_DRndShockwave` became `??_DWorldDir`, now in PropSync.
  - 0x82b6e108: `StopImpl@SampleInst360` was re-mangled to the no-argument form.
- **Units:** 21 units reached 100. 5 are DENOMINATOR_SHRANK, where a wrongly attributed row left. The 5
  that fell off (CharTransDraw, MeterEffect, MicInputArrow, NetLoader_Xbox, FillInfo) did so because a
  re-homed sub-100 row joined their denominator, not because any row fell.
- Leg B equals the lane tip's in-tree build exactly (48,315 / 4,996,076 B / 57.369328).

Per-fork contributions are in §3.

## 2. Method

1. **Eight forks, one per directory**, each in its own worktree and branch
   (`w16-jd-{ui,utl,synth,sx,bm,os,world,obj}`) off `169512b3e`, each owning only its directory's
   source plus the map rows and `.text` split lines of its units.
2. **Per-fix check.** A full `./tools/ninja-locked` build and a whole-report row diff
   (`~/tmp/w16jd/snap.py`) against the previous build. An edit was kept only if no row fell off 100
   and nothing dropped unexplained. Never `run_objdiff` alone.
3. **Integration.** `w16-jd` was fast-forwarded to main `99b26594d`. Each fork was then rebased onto
   the lane tip, rebuilt, and its snapshot diffed against the previous lane state, before
   `git merge --no-ff`. The lane was finally rebased onto `bd4ae22ec` with `--rebase-merges`, keeping
   the eight merges.
4. **Identification** on retail bytes only: RTTI (vtable[−1] → COL → TypeDescriptor), vtable slots read
   against our compiled vtable, sole callers, `$4` thunk branch targets, strings, jump tables, and body
   rank. Map inserts go through `tools/gated_map_write.py`; renames are textual value edits.
5. **Re-homes** move `.text` lines only. `.pdata` is re-derived by the split.

## 3. Per-fork results (measured on the rebased fork tip against the lane state before it)

| fork | Δfns | ΔB | up | down | off 100 |
|---|---:|---:|---:|---:|---:|
| world | +42 | +8,344 | 16 | 1 | 0 |
| beatmatch | +64 | +7,052 | 15 | 0 | 0 |
| synth_xbox | +33 | +4,724 | 23 | 0 | 0 |
| synth | +55 | +5,604 | 3 | 1 | 0 |
| utl | +35 | +4,560 | 7 | 1 | 0 |
| ui | +57 | +7,372 | 26 | 0 | 0 |
| obj | +48 | +5,424 | 15 | 6 | 0 |
| os | +58 | +7,388 | 16 | 0 | 0 |
| **sum** | **+392** | **+50,468** | | | **0** |

"up/down" counts same-name rows; renamed and re-homed rows show as GONE/NEW pairs. Every GONE row that
was at 100 reappears at 100 under its new name or unit. The eight fork deltas sum exactly to the
in-tree lane-vs-main figure (+392 / +50,468 B), and the rebase onto `bd4ae22ec` changed neither.

### 3.1 world
- **LightPreset:** `Save` (0x824b0cf0, 744 B; rev 0x15, writes `Name()` and `mLegacyFadeIn`), `Clear`
  (512 B), the four `Remove*`, `Keyframe::Save`/`Load`, `SetFrameEx`, `SymToPstKeyframe`
  ("next"/"prev"/"first"), `PlatformOk`: all to 100. `LegacyLoadP9` and `LegacyLoadStageKit` had their
  names shifted; the retail callers (`Keyframe::Load` vs `LightPreset::Load`) decide which is which.
- **LightPresetManager:** its RB3 API (SetPresetsEquivalent, GetPresets, SchedulePstKey, StompPresets,
  SendLightingMessage, PickRandomPreset, Interp) ported from retail bodies, 0 → 100 each. The
  placeholder stubs in `bandobj/BandDirectorStubs.cpp` are removed (only `LegacyFadeIn` and
  `StaticResetEvents` remain there).
- **CameraManager** ctor 0 → 99.92, `PickCameraShot`, `OnRandomSeed`, the `CamShot**` sort family;
  `~CamShot`; `WorldDir::PreLoad` (400 B); `SpotlightDrawer::DrawLight` 0 → 99.68.
- `New<RndTransProxy>`'s old key 0x824c4500 pointed at a pad word inside `fn_824C44A8`; nulled and the
  name moved to 0x824bd938.

### 3.2 beatmatch
- **New TU `Playback.cpp`** (retail 0x82792538–0x82792D50, vtable 0x8210F5EC): `AddSink`,
  `DoCommand` (912 B; seven Symbols SWING/UP/DOWN/TRACK/HOPO/FLIP/FFLIP at 0x8210F5F0–0x8210F61C),
  `Jump`, `Poll`, `LoadFile`, `GetPlaybackNum`, `~Playback`, `??_G`, 7 guard funclets: 0 → 100.
- **`TrackType.cpp` pinned** (0x8277B490–0x8277B5A0): `TrackTypeToSym` → 100, `SymToTrackType` 23.48.
- **BeatMatchController:** ctor and `NewController` (760 B) identified by their strings and the seven
  controller ctors; 0x8278FAE0 renamed from `CX2SourceVoice::OnVoiceProcessingPassEnd` (an XAudio2
  name) to `RegisterKey`.
- VocalNoteList `DetermineFreestyleSections`, `PitchAt`, `GetPracticePhrases`, `TrimExcess<VocalNote>`;
  `NotesDone` 91.39 → 98.65. Interface `??1`/`??_G` by RTTI.

### 3.3 synth_xbox
- **Mic.cpp declared the whole XHV chat path and defined none of it.** `Init`, `OnDataReady`,
  `StartPlayback`, `StopPlayback`, `DataReadyCallback` → 100; `AddToBuffer` 99.92, `AddRemoteMic` 93.65,
  `ReadChatBuffer` 85.53. `MicXbox::Poll` 92.72 → 100. 0x82B60258 renamed from a Kinect name
  (`Gesture::GestureManager_GetFrame`) to `MicManagerXbox::DataReadyCallback`.
- `GranularSynth` ctor 0 → 99.17; `Synapse` ctor 85.74 → 98.38; XAPO rows by RTTI; `FxSendSynapse360`
  `CreateFx` and `SyncEffectParams` real; `FxSendCompress360` ctor / `FxSendChorus360::CreateFx`
  un-swapped by RTTI (0x82B62C60 sits in slot 5 of both the Chorus and Flanger vtables).
- **FFT finding:** `fft_altivec` (3,044 B), `fft_recursive` (2,128 B), `fft_real_forward_altivec`
  (964 B) read 0 because `FFT.cpp` **declares and never defines** them. This is absent VMX128 source,
  not a wrong name. `SquareComplexTransposeVector` was written (0 → 79.24).

### 3.4 synth
- `Stream::GetJumpBackTotalTime` (retail slot 15) and `SampleInst::Stop`/`StopImpl` take **no
  argument** in retail; call sites in `hamobj` and `SampleInst360` follow.
- **New TU `MidiInstrumentMgr.cpp`.** 0x827145f0 renamed from `ATanInterpolator::Reset` (a BinDiff
  transfer) to `MidiInstrument::KillAllVoices`, closing HZ §6's "probable separate TU".
- Fader 0x50 / MoggClip 0x88 retail sizes, with the DC3 members under `HX_NATIVE`. 12 `NewObject<X>`
  thunks named by their ctor callee. Four SeqInst/Sequence `ObjPtr` names un-swapped by callee vtable.
- `SynthSample::SyncProperty` (736 B) by RTTI slot 7, moved out of ByteGrinder's pin.

### 3.5 utl
- **Retail MemHeap/MemTracker/AllocInfo is the lean pre-DC3 version**: AllocInfo 0x18, MemTracker
  0x18194, `MemInit` reads six config keys and sets `gNumHeaps = Size() − 1`, `Report` prints "N/A"
  for the physical heap. The DC3 extras are `HX_NATIVE`-only. `MemHeap::Free` returns `bool`.
- 0x827cfcb0 is `~NetLoaderStub` by RTTI, not `~NetLoaderXbox`. The `/DRB3_HTTPGET_VIRTUAL_DTOR` flag had
  been inferred from that misname, so it is removed from `objects.json` and `HttpGet.h`'s comment is
  corrected.
- Kinect names at 0x82745c50 (`??1MovieLoader@Impl@Movie@@`, by RTTI) and 0x82745b40 (null) replaced.

### 3.6 ui
- UILabelDir `SyncObjects` (slot 3), accessors, `GetStateColor`, and the UIColor vector helpers. Two of
  those carried ChallengeRow names (a band3 type).
- UIFontImporter `FontImporterSyncObjects` (924 B), `OnSyncWithResourceFile` 75.93 → 99.90. A swapped
  `OnImportSettings`/`OnAttachToImportFont` pair was undone using the Handle dispatch strings.
- UIListDir 6-arg `DrawWidgets` (524 B); UISlider `PreLoad`/`PostLoad`/`Update`/`DrawShowing`/
  `CollideShowing`; UIScreen `Draw`/`UnloadPanels`; UIListCustom/SubList/Mesh `Load`;
  `LabelShrinkWrapper::UpdateAndDrawWrapper` 47.97 → 100.
- 0x823C8A88–0x823C8F90 re-homed from Screenshot to CharTransDraw by its `$4` thunk targets.
  0x82803540 is `UITransitionCompleteMsg`'s ctor (RTTI), not `EventDialogDismissMsg`'s.

### 3.7 obj
- `TypeProps::Save`, `DirLoader::LoadObjs`/`LoadDir`/`StateName` (seven state-name strings),
  `ObjectDir::LoadSubDir`, `Object::OnGet`, the three Task `Replace` methods, `TaskMgr::Terminate`:
  0 → 100. `ObjectDir::PreLoad` (3,092 B) 92.76 → 99.61; `Save` (2,108 B) 90.75 → 95.96.
- WorldDir ctor/dtor/`??_D`/`Save`/`PostLoad` named by RTTI and `$4` thunk targets; 21 WorldDir ctor
  funclets to 100.

### 3.8 os
- Functions declared but never defined, written from retail bodies: Joypad raw-HID
  (`ReadSingleJoypad`, `SendRawData`, `ParseRawData`, `RunXinputJoypadLoop`), `JoypadGetCalbertValue`,
  `JoypadKeepAlive`, the File descriptor API (C linkage), UsbMidiKeyboard ctor/`Init`/`Terminate`,
  `PlatformMgr::Init`/`GetOnlineID`/`RegionInit`/`InviteParty`, `GetSystemLanguage`.
- New `VirtualKeyboard_Xbox.cpp` heading (the file compiled but was never pinned).
- 0x8253A7A8 carried `InviteParty`, but its body pushes object handles; it is now nulled. That freed two
  callers to 100.
- **Descriptive, not recovered, names** (retail is stripped and no oracle names them): `JoypadSetRumble`,
  `JoypadSwapPads`, `PlatformMgr::SwapUserPads`, `PlatformMgr::SetUserContext`/`SetUserProperty`/
  `SetUserPresence`, `JoypadSetXinputCalbertMode`/`JoypadInvalidateXinputCaps`/`JoypadSetXinputActuators`.
  Each sits at a body-proven address, but the spelling is ours.
- **Negative result, reverted:** naming 0x825150C8/0x82515110 `ProfileSwappedMsg::GetUser1/2` cost
  **−28 fns / −12,988 B**. Both addresses are ICF survivors for every `Obj<LocalUser>(2|3)` accessor.

## 4. Behaviour bugs fixed

- **LightPreset:** `Clear` and `Remove*` release refs; Keyframe Save/Load no longer drop the post-proc and
  StageKit LED data; `NgSpotlightDrawer` beam intensity is 8.0f (retail `.data`), not 1.0f;
  `CameraManager::OnRandomSeed` no longer re-randomizes; the ctor calls `DOFProc::Init()`.
- **beatmatch:** `DetermineFreestyleSections` pads the final open section with `mFreestylePad`, not
  `mFreestyleMinDuration`; `NotesDone` resumes the tambourine phrase search where the previous gem
  stopped; `JoypadController::GetWhammyBar` gains its `IsNullUser` test.
- **synth:** `SfxInst::StartImpl` sets the clip's controller volume and starts samples with `Start`;
  `FxSend::Replace` clears `mNextSend` (it set it to `to`); `SampleInst::Stop` tests `IsPlaying()`;
  `SynthSample::SyncProperty` exposes the loop properties again.
- **synth_xbox:** the XHV chat path exists; `MeterEffect::DoProcess` computes RMS and peak;
  `StartRemoteProcessingModes` takes the 64-bit XUID (`xvh2.h`).
- **ui:** `UISlider::CollideShowing` returned null; `UIListCustom::CreateElement` deep-copied UIComponent
  clones that retail `ResourceCopy`s; `DataProvider::Text` blanked inactive entries; `InlineHelp` builds a
  BandLabel; `OnAttachToImportFont` imported settings instead of attaching the font.
- **obj:** `ScriptTask::Replace` deletes the task on null replacement instead of retargeting `mThis`;
  `TaskMgr`'s ctor no longer allocates the timelines (`Init` does); `ObjectDir::Save` writes
  `mInlineProxy` directly.
- **os:** `RegionInit` picks Europe only for 0x101/0x201/0x2FE/0x2FF (we picked it for every region but
  0xFF); `ShowKeyboardUI` takes a `LocalUser*` (we passed `Int(2)`); `PlatformPoll` frees both keyboard
  buffers; `AsyncFile::ReadAsync` sets `mFail` on a negative size.
- **utl:** `MemHeap::Free` returns `bool`.

## 5. ICF fold memberships

**Added (four).** Each was re-chased by the lane on the rebased tip, under W16-JG's tightened
`tools/icf_pair_adjudicate.py`:

| group | survivor ← spelling | FLAT T1 | CHASED T1 | CYCLE | placeholder-slot audit |
|---|---|---|---|---|---|
| 0x824b1818 (new) | `_M_insert_overflow_aux<CharClipDisplay>` ← `<EnvLightEntry>` | REFUTED (template twin) | **PROVEN** | 0 | CLEAN |
| 0x8240ddb0 | `??3BinStream` ← `??3HttpGet` | UNDECIDABLE | **PROVEN** | 0 | CLEAN |
| 0x826ab088 (new) | `vector<GameGem>::_M_clear_after_move` ← `_M_clear` | PROVEN | **PROVEN** | 0 | CLEAN |
| 0x827cfca0 (new) | `NetLoader::SetFailType` ← `VocalScoreHistory::SetOctaveOffset` | UNDECIDABLE | **PROVEN** | 0 | CLEAN |

The `--chase` log for all four contains no CYCLE leaf of any kind. `0x826ab088` also has a call-site
proof: four retail `vector<GameGem>` call sites branch to it, and our source spells two different names
at them. Naming the address with either spelling alone knocked the other spelling's callers off 100.

**Withdrawn (with records; groups kept, nothing pruned).**
- `_Copy_Construct<TaskInfo>` (group 0x823958A8) and `_Copy_Construct<ScriptTask::Var>` (0x8229EE78):
  `CALL_CHAIN_PROVES_OWN_ADDRESS`. Both are now mapped at their own retail addresses. Cost −1 fn / −84 B
  in the obj fork: `_Rb_tree<Symbol,DataNode>::_M_create_node` 100 → 99.76. That 100 existed only
  through the wrong membership.
- Groups 0x827cfcb0 and 0x82554cb0 were keyed on wrong survivor names; they are re-keyed with their
  memberships withdrawn (measured Δ0).
- `__uninitialized_copy<ObjPtr<SeqInst>>` from 0x827070f0, by the lane after the rebase (commit
  `6907ef30c`). `tools/alias_placeholder_slot_audit.py` reads it **CONTRADICTED** on the lane tip and
  CLEAN on main. With the synth fork's corrected SeqInst/Sequence names, the strict chase reaches a
  copy-ctor slot whose retail vtable's RTTI is `ObjPtr<Sequence>`, while ours is `ObjPtr<SeqInst>`. The
  membership had passed only through the swapped names. Predicted and measured **Δ0** (the dependent
  Sequence row already sits at 99.90). The audit's other CONTRADICTED membership (`ObjDirItr<UILabel>`,
  0x823d1a78) is pre-existing on main and is left alone.

**Rebase merge of 0x8240ddb0.** The utl fork's `??3HttpGet` addition and W16-JE's `OggFree` restoration
touched the same group and were disjoint. They were merged field by field (both kept). Rebase conflicts
in `symbol_aliases.json` were resolved by `~/tmp/w16jd/resolve_alias.py`. It splices the fork's
group-level changes into upstream's text verbatim, field-merges a group only when the two edits are
disjoint, and refuses a scalar both sides changed. It never fired a refusal. Map conflicts used W16-ID's
`resolve_map.py` and never refused.

## 6. `symbols.txt`

No hand edits. The branch carries two split-written fixed points, committed because the split guard
refuses a split that rewrites its own input:
- **beatmatch:** three over-carve tails merged behind newly named heads (0x826F16E0
  `VocalNote::PitchAt`, 0x82790318, 0x82792538 `Playback::AddSink`), and on the second pass the
  `TrackTypeToAudioType` jump table at 0x8210F2D0.
- **obj:** four tails merged into 0x82757998 `DirLoader::StateName`.

For the A/B both were committed into leg A (HK/HR recipe). In leg A those rows are anonymous and
unpaired, so the merges move nothing there.

## 7. Rows that went down

From the in-tree lane diff against main (9 rows; none was at 100):

| row | B | before → after | why |
|---|---:|---:|---|
| `remove_if<FilePath*>` (system/obj/Dir) | 88 | 99.95 → 0 | paired only against an instantiation our obj unit emitted from a DC3-only path, now `HX_NATIVE` |
| `remove_copy_if<FilePath*>` | 88 | 87.18 → 0 | same |
| `__find_if<FilePath*>` | 252 | 83.92 → 0 | same |
| `ObjDirItr<ObjectDir>::operator++` (TypeProps) | 88 | 78.32 → 0 | same (DC3 `TypeProps::Save` path) |
| funclet `fn_8273CB80` (DirUnloader) | 44 | 93.91 → 0 | same family |
| funclet `fn_824B8758` (LightPreset) | 40 | 99.90 → 99.40 | funclet re-paired when `LightPreset::Load` rose |
| `_M_fill_insert_aux<ObjPtr<Sequence>>` | 392 | 99.95 → 99.90 | relocation-name only (W16-JE's class) |
| `AddHeap` | 344 | 81.10 → 81.05 | its callees are now named, at sites that already differed |
| `_M_allocate_and_copy<FlowMathOp>` (UIList) | 96 | 50.58 → 50.38 | scored against a wrongly placed helper |

The first five are game-code instantiations: retail calls them from VocalPlayer and
GemTrackResourceManager. **No compiled object defines any of those names any more** (checked over
`build/45410914/src/**/*.obj`), so no re-home can recover them. They need VocalPlayer's source to
instantiate them.

## 8. Rows left, by blocker

- **Absent large VMX source:** `fft_altivec`, `fft_recursive`, `fft_real_forward_altivec` (6,136 B).
- **No source in any repo:** CheatProvider's pin at 0x823EF5F8–0x823F0310 is XboxSession code (RTTI,
  about 13 rows). 0x823F4590–0x823F4810 and 0x823F4A30–0x823F56F8 are Xbox voice-chat code. OggMap's TU5
  mogg decryption (1,004 + 516 + 516 B) calls `ctr_decrypt`, which nothing in our tree has. The
  XAudio2-internal block at 0x82BF5DD0–0x82BF6B78 is pinned to UILabel and carries LEAPCORE names; it
  is XDK and out of scope.
- **Needs a PCH-input header (`obj/Object.h`):** retail ref tracking is a `std::list<ObjRefOwner*>` at
  `this+0x20`, but ours is DC3's ring. This blocks `Object::AddRef`/`Release`/ctor, `MergeObject` and
  `WorldInstance::DeleteTransientObjects` (480 B). `UnloadInstrument` needs `SetObjConcrete(0)` inlined.
- **dtk mis-carves (`symbols.txt`):** `GetMaxSlots`, `GameGem::operator=`, `__median<float>`,
  `??0ADSRImpl`, `DeJitter::NewMs`, `KeylessHash::Remove` tails, `??1CharTransDraw` (four rows).
- **Register allocation / scheduling only (about three spellings each):** `DrawLight`, `SetLighting`,
  `LightPreset::Load`, the CamShot ctor, `NotesDone`, `GetVirtualSlot`, `SymToTrackType` (loop rotation),
  MemHeap `Init`/`Alloc`, the MemTracker ctor, `ArkFile::ReadAsync` 98.97, `PlatformMgr::Init` 98.09,
  `ObjectDir::PreLoad`/`Save`/`HasSubDir`, `VoiceBeat::Analyze`, the Flanger/Wah/Delay `Process`
  functions, `MeterEffect::DoProcess`, `ReadChatBuffer` (counter loop vs `bdnz`).
- **Relocation-name only (W16-JE):** `FillInfo::LanesAt`, `JoypadGetBreedString`,
  `OnSyncWithResourceFile` 99.90, four ui `??_G` rows at 99.74, `??_GDirUnloader`, several STL helpers.
- **Wrong names found and not fixed (owned elsewhere):**
  - 0x82466298 is `DOFProc::Init`, misnamed and pinned to Console.
  - 0x8235C2E0, mapped as `Object::SyncProperty` in Tour, is RndMultiMeshProxy's `SyncProperty` thunk target.
  - 0x82745d08 is mapped `??_GWavMgr` but calls the MovieLoader dtor.
  - 0x82B5BBA8 is mapped `Synth360::RequirePushToTalk`, but it is a non-virtual `(int, bool)` method;
    the real virtual is the anonymous `fn_82B5BC50`.
  - `erase<JumpInstance>` in StandardStream erases 16-byte POD elements.
- **Pin problems seen, not moved:** Crowd `fn_82325668` (a UIComponent ctor); world/Dir `fn_826C23A0`
  (`PlayerTrackConfig` fill_insert); `fn_824CA290` (`??_GEventAnim`, pinned in DefaultPhysicsManager); the
  unidentified TU at 0x82744F60–0x82745B00 spanning DataNode's and WavMgr's pins (`movie`,
  `is_timed_movie`); `??__ETheBeatMatchPlayback` pinned to FilePath.
- **Outside these directories:** `??0MicInputArrow`'s first `push_back` passes `true` where retail
  passes `false` (bandobj); `ProfileSwappedMsg` ctor needs `meta/Profile.h` to take raw pointers;
  `SpotlightDrawer::DrawShadow` needs `Character::DrawShadow(const Transform&, const Plane&)`.
- **Still anonymous:** about 53 KB of weight in these units with no caller, feature, RTTI or
  neighbour-body signal (static-initializer fragments, EH funclets, fold soups).

## 9. Rebase onto main, and comments

- **The comment cleanup on main (`99b26594d`).** Every comment-only conflict took main's wording
  (`~/tmp/w16jd/take_head_comments.py` refuses any hunk carrying code). Two conflicts carried code and
  kept the fork side: `BeatMatchController.cpp`, where main's comment said the ctor was not ported and
  the fork ported it, and `UISlider.cpp`, where main's comment said `CollideShowing` already matched with
  the simpler body. One main-worded comment is now stale by instruction: `ui/UILabelDir.h` still says
  "DECLARATION-ONLY … Bodies land when UILabelDir itself is ported", and the bodies now exist.
- **No added comment cites rb3-Wii or "the oracle".** I grepped the branch diff under `src/` and
  `native/`. The only hit is "Wii keytar", a controller name. The beatmatch fork rewrote six Wii-address
  comments and two "oracle" comments in files it touched to cite retail addresses.

## 10. Native gate break and fix

The first native gate run on the rebased tip **FAILED 16/18** (`rb3-render`, `rb3-frame` NOBINARY). The
synth fork moved `SampleInst::Stop`/`StopImpl` to retail's no-argument form, but the shared engine's
`SampleInstNative` (`milo-native-engine/src/platform/SampleInst_Native.h`) overrides
`StopImpl(bool)`. Under `HX_NATIVE`, `SampleInst` also derives `PlayableSample`, whose `Stop(bool)` is
pure. The engine is pinned and lanes do not edit it, so the fix is on our side:
- `b69b31e46`: under `HX_NATIVE` the slot-28 declaration keeps `StopImpl(bool)`, and `Stop()` passes
  `false`.
- `190130846`: a native-only `Stop(bool)` forwards to `Stop()`. `SampleInstNative::StopImpl(bool)`
  ignores its argument, so native behaviour is unchanged.

Each commit was predicted and measured **Δ0** in a full match build. The second gate run was
`PASS 18/18, 0 skipped`.

**Engine change request (for the coordinator, who owns the pin):** `SampleInstNative` should override
RB3's `StopImpl()`. `PlayableSample::Stop(bool)` can then follow, and both native-only shims here can go.

## 11. Gates (rebased tip)

On `190130846`, after a full build:
```
[map-injectivity] OK: 32457 applied rows, 32456 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1615 map-consistent, 260 tolerated (enumerated above), 0 contradicted, 1876 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
The native gate ran after the last code change. Only this docs-only commit follows it, and it touches no
build input.

Also run, beyond the required gates:
- `tools/icf_pair_adjudicate.py --chase` on the four new memberships under W16-JG's updated tool: all
  CHASED T1 PROVEN, 0 CYCLE.
- `tools/alias_placeholder_slot_audit.py`: the four new memberships are CLEAN. Tree-wide it reports 1
  CONTRADICTED (pre-existing on main) after the §5 withdrawal.

## 12. Not done

- No hand edits to `symbols.txt`; the mis-carves in §8 are recorded, not fixed.
- The permuter was not run.
- Rows whose only charges are relocation names were left to W16-JE. The few fold memberships added
  here were needed to keep a newly named address from knocking its other spelling's callers off 100;
  per the coordinator's note they are kept and listed in §5.
- `obj/Object.h` and `os/Debug.h` (PCH inputs) were not edited.
- **Not merged to main**, per the brief.
