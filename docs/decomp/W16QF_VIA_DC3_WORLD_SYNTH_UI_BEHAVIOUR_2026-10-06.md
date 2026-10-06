# W16-QF — VIA-DC3 ring, world/ synth/ ui/ meta/ movie/ flow/: open rows read against retail for behaviour (2026-10-06)

Branch `w16-qf` (worktree `~/tmp/wt-w16qf`), started off main `a0003899e`, rebased onto main `377f357a6`.
This lane is the sequel to `W16PU_VIA_DC3_RNDOBJ_CHAR_BEHAVIOUR_2026-10-06.md`. W16-PU covered `rndobj/` and `char/`; this lane covers the other six VIA-DC3 directories.
It skips the rows `W16QA_DC3_NEWER_BODY_SWEEP_2026-10-06.md` already settled. Those rows are listed below with W16-QA as their outcome.

## Result

- **Five behaviour differences fixed**, all against RB3 retail bytes:
  - `UIManager::PushScreen`
  - `MoggClipMap::operator=`
  - `UISlider`'s `SetTypeDef` slot
  - `SuperFormatString`'s ctor (RB3's three-argument form)
  - `UILabel::SetTokenFmtImp`, which follows from the SuperFormatString change
- **One native-only guard** in `UIComponent::UpdateResource`, needed by the UISlider fix. It is under `HX_NATIVE`, so the match build is unchanged.
- **One map name corrected.** Retail's thunk at `0x82809BD8` is `?SetTypeDef@UIComponent@@$4PPPPPPPM@BI@`, not `?SetTypeDef@UISlider@@$4PPPPPPPM@A@`.
- **One new pin.** `system/utl/SuperFormatString.cpp` over `0x82BBABEC–0x82BBB2E8`; it was previously the anonymous `auto_03_82BBABEC_text`.
- **A/B on the whole branch** (`tools/ab_measure.py --patch`, ruler `name_check`, fresh worktree off main `377f357a6`):
  **Δmatched +6, Δcode_bytes +384, Δfuzzy +0.018160 pp. No row went down.**
  Predicted before the run: +6 / +384 B. The only row that left the report is retail's `0x82809BD8` under its old name (73.75). It re-paired at 100 under the corrected name.
- **Native:** `native_build_gate.sh` PASS 18/18, 0 skipped. `native_health.sh` PASS, 77 gates, 0 crashed. Result lines are verbatim below.
- **Every other open row** (304 of 309) is codegen, placement, a wrong map name, unpaired identification work, or settled by W16-QA. None of them is a behaviour difference in our source. The per-row table is at the end.

## Population and method

The rows are the VIA-DC3 tier (`scope_ledger2.tier()`) restricted to `world/ synth/ ui/ meta/ movie/ flow/`, with `fuzzy_match_percent < 100`. Map-scaffold units (≤ 6 defined symbols) are excluded. All figures are at `a0003899e`.
**309 rows / 77,616 B** in total: synth 104, world 76, ui 65, meta 37, flow 23, movie 4.

- **Named rows: 119 / 63,128 B.**
- **Anonymous `fn_` rows: 190.** 120 sit at 0% (11,648 B: unpaired, so identification work). 70 are non-zero (2,840 B: EH funclets plus small thunks).

Instruments (scratch copies in `~/tmp/w16qf/`, the same set W16-PU used):

1. **`opsig2`** compares the opcode multiset, retail vs ours. Rows with an identical multiset differ only in registers, offsets or relocation names. opsig2 masks every `bl`, so it cannot see a callee difference on its own. That is why step 2 exists.
2. **`seqdiff` / `seqall`** compare the ordered call and constant sequence for every named row. Each call-name difference was read against our source and the fold groups. `DataNode::Int`/`Array`, getter/setter twins, `vector<T*>` instantiations and `operator delete` variants are ICF folds. A different element stride or allocation size is not a fold: it means a different instantiation, so a wrong map name.
3. **Retail-byte reads** (`pe.py`, Ghidra decompile, `run_diff_inspect mismatches` on the graded ruler) for every row whose opcode multiset differs (57 rows), and for every 0% named row.
4. **`strscan`** checks every string literal our code references against the bytes retail's code loads at the same instruction, across all 278 VIA-DC3 units.
   Result: **3,640 string refs checked, 0 mismatches.**
   Control: shifting every retail address by +1 byte (`CTRL_SHIFT=1`) gives **3,593 mismatches**, so the scan can fail.

## Fixes

### 1. `UIManager::PushScreen` (retail `0x828047E8`, 96 B): 0.00 → 100.00
Retail has no null test on `mCurrentScreen`. It cancels the transition, pushes the current screen (null included), clears it, and calls `GotoScreenImpl(screen, false, false)`.
Ours did nothing at all when no screen was current.
The push-depth report leaves no code in retail, so it stays `HX_NATIVE`-only. The `MILO_ASSERT(mCurrentScreen)` was dropped, because it would abort natively exactly where retail pushes null.

### 2. `MoggClipMap::operator=` (104 B): 88.08 → 100.00
Retail calls the out-of-line `??4Object@Hmx@@` before copying the members. Ours skipped the base assignment.

### 3. `SuperFormatString`: RB3's three-argument form (retail ctor `0x82BBABF8`, 1,608 B): unpaired → 94.33
RB3 retail's ctor takes `(fmt, const DataArray *, bool)`. Ghidra's decompile and the graded diff agree on each point:

- **None of DC3's additions.** No locale/language arguments, no `%`-tracking (`mTokensOnly`, `mHasPercentFormat`), and no `FinalStr()` `"%s"` append.
- **`sep_int`** calls the one-argument `LocalizeSeparatedInt(int)`. Retail passes only `r3` to `0x827C97C8`.
- **`token`** localises `Symbol(phInfo)` with `Localize(sym, 0)`.
- **`_snprintf`.** Retail calls the CRT `_snprintf` directly, four call sites. That is a behaviour difference on truncation: `_snprintf` does not re-terminate, while `Hx_snprintf` does. Applied under the `MakeString.cpp` house pattern: `_snprintf` in the match build, `Hx_snprintf` under `HX_NATIVE`.
- **Unchanged and confirmed by retail:** `{missing:%s}` (`.rdata 0x821A61C4`), `{badfmt:%s` (`0x821A61A8`), and the `p[1] != '{'` escape test.

Every placeholder type check and output path was compared clause by clause with retail and is the same. The remaining 5.7 pp are register/stack-slot allocation.
One codegen-only spelling was kept because it measured higher: `nodeBad = t != kDataString && t != kDataSymbol` instead of the nested `if`, 92.06 → 94.33.
The unit was previously the anonymous `auto_03_82BBABEC_text`. It is now pinned as `system/utl/SuperFormatString.cpp` (`.pdata` derived by dtk), and `0x82bbabf8` is named via `tools/gated_map_write.py`. Its three EH funclets read 0 → 100.
milo-native-engine's `tests/dc3_runtime_sources.cmake` compiles DC3's own `SuperFormatString.cpp`, not ours, so it is unaffected.

### 4. `UILabel::SetTokenFmtImp` (retail `0x827F2D78`, 288 B): 75.93 → 99.93
It follows fix 3:

- builds the three-argument `SuperFormatString(localized, da1, b)`;
- re-reads `Size()` every iteration;
- localises `Symbol` tokens with `Localize(sym, 0)`;
- when `b` (tokens only) is set, displays the **raw format string** through `RawFmt()`. Before, it went through `Str()`.

Retail calls `RawFmt()` out of line, so it moved out of the header (96.25 → 99.93).
The last charge is that call's name. Retail's callee is the ICF survivor `??BDataArrayPtr@@QBAPAVDataArray@@XZ` (`0x8274a9a8`), whose body is the same 8-byte `lwz r3,0(r3)` accessor.
No alias was installed. `tools/ourside_fold_sweep.py` withholds zero-relocation bodies that small by design, because an unimplemented stub compiles to the same bytes.

### 5. `UISlider` inherits `UIComponent::SetTypeDef` (retail thunk `0x82809BD8`, 16 B): 73.75 → 100.00
On retail bytes, `0x82809BD8` reads:
```
lwz r11,-4(r3); subf r3,r11,r3; addi r3,r3,-0x18; b 0x827FE658
```
`0x827FE658` is `?SetTypeDef@UIComponent@@UAAXPAVDataArray@@@Z`. So RB3's UISlider has **no `SetTypeDef` override**. A typedef change runs UIComponent's default-type reset, then `Object::SetTypeDef`, then `UpdateResource()`.
Ours had DC3's override (`Object::SetTypeDef` + `Update()`), and it is removed.
The map called that thunk `?SetTypeDef@UISlider@@$4PPPPPPPM@A@` (adjust 0), which its own body contradicts. It is renamed to `?SetTypeDef@UIComponent@@$4PPPPPPPM@BI@` (adjust 0x18), which our `UISlider.obj` now emits.
`gated_map_write.py` only inserts rows, so this was one exact textual substitution. `--audit-objects` reports 0 object-side collisions.

**Native consequence, measured.** After this fix, `native_health.sh` FAILED with `rb3-render:crash`, SIGSEGV, 22 gates. The gdb backtrace:
```
list<UIResource*>::begin ← UIManager::FindResource ← UIManager::Resource ← UIComponent::UpdateResource ← UIComponent::SetTypeDef ← UISlider::SetType ← Object::LoadType
```
The render driver never boots a UIManager, so `TheUI` is null. `native/src/milo_object_factories.cpp` already records two crashes of this kind.
Fix: `UIComponent::UpdateResource` takes no resource when `TheUI` is null, **under `HX_NATIVE` only**. The match build is unchanged: `UIComponent.obj` recompiled after the edit, and `report.json` measures and every row were identical. Health went back to PASS, 77 gates.

## Findings recorded, not fixed (not behaviour in our source, or out of this lane's scope)

- **Wrong map names (5 rows).** The retail body at the named address is a different instantiation. Its element stride or size cannot fold with ours:

  | Row | Retail | Ours |
  |---|---|---|
  | `__destroy_range<Flow::DynamicPropertyEntry>` | stride 0x30, calls `~OutfitConfig::MeshAO` | 0x88 |
  | `__destroy_mv_srcs<vector<Vector3>>` | stride 0x10, calls `pair<Symbol,vector<int>>` dtor | 0xc |
  | `list<DataNode>::_M_create_node` | 0x28-byte nodes | 0x10 |
  | `vector<BattleStep>` | stride 16, calls `_M_fill_insert_aux<SampleMarker>` | 32 |
  | `__uninitialized_copy<CamShotFrame>` | stride 0x1c, calls `_Copy_Construct<Character::Lod>` | 0x108 |

  Re-naming these is identification work. The renamed row is only pairable if some base obj defines the retail name, so it was not done here.
- **`SampleMarker::SampleMarker` (64 B, 67.19): wrong map name.** Retail's body is a DxRnd `PreDeviceReset`/call/`PostDeviceReset` wrapper.
- **`CamShot::CamShot` (968 B, 87.43): unused member.** Retail leaves offset 0x40 unconstructed. That is DC3's `mCrowdStateOverride`; the string `"crowd_state_override"` is absent from retail, and Copy/Save (100%) never touch it. It is a DC3-only member, not a behaviour difference. Removing it would shift the layout and was not attempted.
- **`Movie::Impl::PlatformCacheFile` is declared but never defined in our tree.** Retail's callee (`0x82533618`) is `li r3,1; blr`, i.e. it returns true, ICF-folded with `ObjDirPtr<ObjectDir>::IsDirPtr`. Retail calls it out of line, so its definition lived outside the calling TU's inlining reach. No native target links `Movie::Impl::Begin`.
- **Funclet outliers.** Non-zero anonymous funclets pair by byte signature (`masked_equal`), so their residue is against a nearest-neighbour funclet, not "ours of the same". Three do not look like plain frame-offset noise:
  - `ui/CheatProvider.cpp` `fn_82700738`: retail `mulli 0x44`, ours `0x1c`, an element stride.
  - `synth/SampleInst.cpp` `fn_822A2E78`: 60.8.
  - `world/CameraManager.cpp` `fn_825AD700`: 16 B, 68.75.

  Their parents are not identified, so they were not adjudicated.

## Measurement

**A/B, final branch.** Full tool output: `~/tmp/w16qf/ab2.log`. The scratch A/B worktree was removed afterwards, along with its run dir.
```
leg A: matched=53577 masked=25201 honest=28376 code%=58.072998  (recompiles: 0, settled)
leg B: matched=53583 masked=25204 honest=28379 code%=58.076744  (recompiles: 19, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+6  Δmasked_equal=+3  Δhonest=+3  Δcode%=+0.003746pp  Δcode_bytes=+384
units at 100% [mpn ruler]: legA 567 -> legB 569  (MoggClipMap, UISlider; 0 fell off)
```
**Row diff, leg A vs leg B.**
- Up: MoggClipMap::operator= (88.08 → 100), PushScreen (0 → 100), SetTokenFmtImp (75.93 → 99.93), the corrected UISlider thunk (→ 100), the SuperFormatString ctor (→ 94.33), and three funclets (→ 100).
- Leaving the report: the four `auto_03_82BBABEC_text` rows (re-homed, see above), and `0x82809BD8` under its old name.
- **No row's score went down.**

An earlier A/B on the first four fixes, before the UISlider fix, measured +5 / +368 B, as predicted.
The `HX_NATIVE` guard commit (`6d8756243`) is after the A/B patch. It was measured match-neutral directly: full rebuild, UIComponent.obj recompiled, every report row identical.

**Native.** Final runs, in `~/tmp/wt-w16qf`, after the last source commit:
```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=77 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

## Deliberately not done

- **No register-allocation or scheduling grinding.** That covers the SuperFormatString ctor residue and the codegen rows; the permuter is out of scope.
- **No map re-naming** of the five stride-mismatched template rows or SampleMarker. Those are identification work.
- **No alias** for `RawFmt` → `??BDataArrayPtr` (8 B zero-relocation body, see fix 4).
- **The 120 unpaired anonymous 0% rows** (WavMgr 26, CheatProvider 17, UI 6, OggMap 5, …) were excluded as identification work, as W16-PU did.
- **BuildNGCone** was not independently re-derived; it rests on the w8-q survey.
- **Not pushed, not merged into main.**

## Per-row outcomes: named rows (119)

Columns: unit source, function, size, fuzzy at the base (`a0003899e`), fuzzy on the final A/B leg B, outcome.
"codegen" means same values: register choice, scheduling, stack/static addressing, FMA/compare/fsel forms or evaluation order.
"callee names differ only by ICF fold" means every differing `bl` target was read and is a fold twin or an unnamed placeholder.

| Unit | Function | Size | Base | After | Outcome |
|---|---|---|---|---|---|
| flow/Flow.cpp | `std::vector<String, std::StlNodeAlloc<String> >::~vector<String, std::StlNodeAlloc<String> >(void)` | 136 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| flow/Flow.cpp | `void std::__destroy_range<Flow::DynamicPropertyEntry *, Flow::DynamicPropertyEntry>(Flow::DynamicPropertyEn...` | 80 | 99.70 | 99.70 | wrong map name - retail stride 0x30 calls ~OutfitConfig::MeshAO; ours is 0x88 |
| flow/FlowCommand.cpp | `std::_List_node_base * std::list<DataNode, std::StlNodeAlloc<DataNode> >::_M_create_node(DataNode const &)` | 72 | 99.67 | 99.67 | wrong map name - retail allocates 0x28-byte nodes; list<DataNode> nodes are 0x10 |
| flow/FlowOnStop.cpp | `static RndMesh * Hmx::Object::New<RndMesh>(void)` | 72 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| meta/Achievements.cpp | `XUSER_ACHIEVEMENT * std::vector<XUSER_ACHIEVEMENT, std::StlNodeAlloc<XUSER_ACHIEVEMENT> >::_M_allocate_and_...` | 100 | 99.80 | 99.80 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| meta/Achievements.cpp | `void Achievements::Submit(LocalUser *, Symbol, int)` | 136 | 93.53 | 93.53 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| meta/MoviePanel.cpp | `void MetaInit(void)` | 176 | 99.77 | 99.77 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| meta/StoreEnumeration.cpp | `virtual void XboxEnumeration::Poll(void)` | 452 | 98.94 | 98.94 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| meta/StoreEnumeration.cpp | `virtual void XboxEnumeration::Start(void)` | 328 | 99.39 | 99.39 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| meta/StoreOffer.cpp | `StoreOffer::StoreOffer(DataArray *, SongMgr *)` | 948 | 99.68 | 99.68 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| meta/StorePanel.cpp | `DataNode StorePanel::OnMsg(SigninChangedMsg const &)` | 172 | 98.44 | 98.44 | codegen - identical opcode multiset (registers/offsets only) |
| meta/StorePanel.cpp | `void StorePanel::CheckOut(StorePurchaseable *)` | 260 | 96.52 | 96.52 | codegen - identical opcode multiset (registers/offsets only) |
| meta/StorePreviewMgr.cpp | `virtual DataNode StorePreviewMgr::Handle(DataArray *, bool)` | 704 | 99.95 | 99.95 | codegen - stack offset (QA: DC3 body does not compile here) |
| meta/StorePreviewMgr.cpp | `virtual void * StorePreviewMgr::`vector deleting destructor'(unsigned int)` | 68 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| movie/Movie.cpp | `bool Movie::Impl::Begin(char const *, float, bool, bool, bool, bool, int, BinStream *)` | 592 | 99.97 | 99.97 | codegen; callee names are folds. Note: Movie::Impl::PlatformCacheFile is only DECLARED in our tree; retail body (0x82533618) is li r3,1;blr (returns true), ICF-folded with ObjDirPtr::IsDirPtr. No native target links Movie::Impl::Begin; not defined here |
| movie/Movie.cpp | `static MovieInternalBuffers * MovieInternalBuffers::New(std::vector<BINK *, std::StlNodeAlloc<BINK *> >)` | 992 | 99.55 | 99.55 | codegen - 4-word copy addressed from a different base register; Bink callees are placeholders; operator delete fold |
| movie/Movie.cpp | `void Movie::Impl::DiscContentionPublish(void)` | 196 | 98.78 | 98.78 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| movie/Movie.cpp | `void Movie::Impl::Draw(void)` | 380 | 99.58 | 99.58 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/ADSR.cpp | `static void ADSR::operator delete(void *)` | 4 | 95.00 | 95.00 | codegen - identical opcode multiset (registers/offsets only) |
| synth/ByteGrinder.cpp | `DataNode op0(DataArray *)` | 88 | 99.55 | 99.55 | codegen - identical opcode multiset (registers/offsets only) |
| synth/ByteGrinder.cpp | `DataNode op6(DataArray *)` | 92 | 99.57 | 99.57 | codegen - identical opcode multiset (registers/offsets only) |
| synth/ByteGrinder.cpp | `DataNode op9(DataArray *)` | 96 | 99.58 | 99.58 | codegen - identical opcode multiset (registers/offsets only) |
| synth/ByteGrinder.cpp | `void std::vector<BattleStep, std::StlNodeAlloc<BattleStep> >::_M_fill_insert(BattleStep *, unsigned int, Ba...` | 108 | 99.44 | 99.44 | wrong map name - retail stride 16 calls _M_fill_insert_aux<SampleMarker>; a different instantiation |
| synth/CompressionEffect.cpp | `void CompressionEffect::Process(float *, int, int)` | 632 | 99.94 | 99.94 | codegen - identical opcode multiset (registers/offsets only) |
| synth/Emitter.cpp | `SynthEmitter::SynthEmitter(void)` | 496 | 86.13 | 86.13 | settled by W16-QA (retail inlines ObjPtr(owner,ptr) ctor; PCH-gated) |
| synth/Faders.cpp | `FaderGroup::~FaderGroup(void)` | 160 | 97.50 | 97.50 | codegen |
| synth/Faders.cpp | `virtual Fader::~Fader(void)` | 112 | 99.82 | 99.82 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/Faders.cpp | `void Fader::DoFade(float, float)` | 444 | 93.09 | 93.09 | codegen - callee names are ICF folds (Interpolator::operator new vs ::new, list<FaderTask*> vs list<Object*> insert) |
| synth/MetaMusic.cpp | `MetaMusicLoader::MetaMusicLoader(File *, int &, unsigned char *, int)` | 132 | 96.06 | 96.06 | codegen |
| synth/MetaMusic.cpp | `void MetaMusic::UpdateMix(void)` | 1572 | 99.85 | 99.85 | codegen - DataNode::Int vs DataNode::Array are one folded accessor |
| synth/MidiInstrument.cpp | `NoteVoiceInst::NoteVoiceInst(MidiInstrument *, SampleZone *, unsigned char, unsigned char, int, int, float)` | 364 | 97.55 | 97.55 | codegen - identical opcode multiset (registers/offsets only) |
| synth/MidiSynth.cpp | `BinStream & operator>>(BinStream &, WorldDir::BitmapOverride &)` | 380 | 97.89 | 97.89 | codegen - Find<RndTex> call placed in a different block order; same calls |
| synth/MoggClipMap.cpp | `MoggClipMap & MoggClipMap::operator=(MoggClipMap const &)` | 104 | 88.08 | 100.00 | **FIXED** - retail assigns the Hmx::Object base (out-of-line ??4Object) before the members |
| synth/SampleData.cpp | `SampleMarker::SampleMarker(void)` | 64 | 67.19 | 67.19 | wrong map name - retail body at that address is a DxRnd PreDeviceReset/call/PostDeviceReset wrapper, not a SampleMarker ctor |
| synth/SampleData.cpp | `void SampleData::Load(BinStream &, FilePath const &)` | 440 | 99.95 | 99.95 | codegen - identical opcode multiset (registers/offsets only) |
| synth/SampleInst.cpp | `SampleMarker * std::__uninitialized_copy<SampleMarker const *, SampleMarker *>(SampleMarker const *, Sample...` | 96 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| synth/Sequence.cpp | `void std::vector<ObjPtr<SeqInst>, std::StlNodeAlloc<ObjPtr<SeqInst> > >::_M_insert_overflow_aux(ObjPtr<SeqI...` | 328 | 99.94 | 99.94 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/Sequence.cpp | ``anonymous namespace'::Unlockable * std::__uninitialized_copy<`anonymous namespace'::Unlockable *, `anonymo...` | 96 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| synth/Sfx.cpp | `SfxInst::SfxInst(Sfx *)` | 432 | 99.72 | 99.72 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/Sfx.cpp | `Sequence * Synth::Find<Sequence>(char const *, bool)` | 128 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| synth/Sfx.cpp | `void std::_Destroy<MidiChannel::Note>(MidiChannel::Note *)` | 4 | 95.00 | 95.00 | codegen - identical opcode multiset (registers/offsets only) |
| synth/StandardStream.cpp | `void StandardStream::Init(float, float, Symbol, bool)` | 672 | 99.97 | 99.97 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/StandardStream.cpp | `int StandardStream::ConsumeData(void **, int, int)` | 616 | 96.29 | 96.29 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/StandardStream.cpp | `JumpInstance * std::vector<JumpInstance, std::StlNodeAlloc<JumpInstance> >::erase(JumpInstance *, JumpInsta...` | 92 | 21.48 | 21.48 | codegen - retail inlines _M_erase into erase; we call it |
| synth/StandardStream.cpp | `void StandardStream::InitInfo(int, int, bool, int)` | 872 | 98.78 | 98.78 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/StandardStream.cpp | `void StandardStream::PollStream(void)` | 408 | 98.07 | 98.07 | codegen - equivalent compare forms |
| synth/StreamNull.cpp | `StreamNull::StreamNull(float)` | 288 | 99.93 | 99.93 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/StreamNull.cpp | `virtual void Sfx::SynthPoll(void)` | 4 | 0.00 | 0.00 | placement - 4 B COMDAT, 0% (no base symbol in this TU) |
| synth/StreamReceiver.cpp | `virtual void StreamReceiver::Poll(void)` | 632 | 99.26 | 99.26 | codegen - equivalent compare forms |
| synth/Synth.cpp | `virtual void * ObjPtr<SynthSample>::`scalar deleting destructor'(unsigned int)` | 76 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| synth/Synth.cpp | `void Synth::DrawMeterScale(float &)` | 344 | 98.95 | 98.95 | codegen - 0.2 is sMeterLayout[0] |
| synth/SynthSample.cpp | `static void SynthSample::Init(void)` | 72 | 99.44 | 99.44 | codegen - identical opcode multiset (registers/offsets only) |
| synth/VorbisReader.cpp | `bool VorbisReader::CheckHmxHeader(void)` | 632 | 99.99 | 99.99 | codegen - stack offset; operator delete fold |
| synth/VorbisReader.cpp | `virtual VorbisReader::~VorbisReader(void)` | 316 | 97.41 | 97.41 | codegen - operator delete / delete[] fold names, SetEvent placeholder |
| synth/VorbisReader.cpp | `virtual void VorbisReader::Poll(float)` | 784 | 99.95 | 99.95 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/VorbisReader.cpp | `unsigned long `anonymous namespace'::DecodeThreadEntry(void *)` | 272 | 99.85 | 99.85 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/VorbisReader.cpp | `void std::__destroy_range_aux<std::reverse_iterator<std::vector<short, std::StlNodeAlloc<short> > *> >(std:...` | 100 | 99.80 | 99.80 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| synth/tomcrypt/ctr.c | `int ctr_encrypt_fast(unsigned char const *, unsigned char *, unsigned long, Symmetric_CTR *)` | 244 | 99.84 | 99.84 | codegen - identical opcode multiset (registers/offsets only) |
| ui/CheatProvider.cpp | `virtual void Synth::Init(void)` | 840 | 99.98 | 99.98 | codegen - retail callee is an empty-body fold (StlNodeAlloc ctor), consistent with our empty MidiInstrumentMgr::Init; vector<Mic*> fold names |
| ui/InlineHelp.cpp | `??_GInlineHelp@@$4PPPPPPPM@A@AAPAXI@Z` | 12 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| ui/InlineHelp.cpp | `virtual void InlineHelp::SyncLabelsToConfig(void)` | 308 | 99.94 | 99.94 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| ui/LabelNumberTicker.cpp | `virtual void LabelNumberTicker::Poll(void)` | 380 | 99.47 | 99.47 | codegen - identical opcode multiset (registers/offsets only) |
| ui/LabelShrinkWrapper.cpp | `??_GLabelShrinkWrapper@@$4PPPPPPPM@A@AAPAXI@Z` | 12 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| ui/UI.cpp | `virtual void UIManager::PushScreen(UIScreen *)` | 96 | 0.00 | 100.00 | **FIXED** - retail has no null test on mCurrentScreen; always cancels, pushes, clears, goes to the screen |
| ui/UI.cpp | `void std::__destroy_aux<LocalePanel::Entry>(LocalePanel::Entry *, std::__false_type const &)` | 4 | 95.00 | 95.00 | codegen - identical opcode multiset (registers/offsets only) |
| ui/UIComponent.cpp | `virtual void UIComponent::Update(void)` | 1192 | 98.56 | 98.56 | codegen |
| ui/UILabel.cpp | `void UILabel::LabelUpdate(bool, bool)` | 464 | 99.57 | 99.57 | codegen - setter/getter folds (Singer::SetFrameMicPitch vs RndFont::SetBaseKerning, MemcardXbox::GetContainerName vs UIColor::GetColor) |
| ui/UILabel.cpp | `void UILabel::SetTokenFmtImp(Symbol, DataArray const *, DataArray const *, int, bool)` | 288 | 75.93 | 99.93 | **FIXED** - RB3 3-arg SuperFormatString, Localize(sym,0), RawFmt when only tokens were substituted; residue = 1 callee-name charge (RawFmt vs fold survivor ??BDataArrayPtr) |
| ui/UIList.cpp | `std::vector<Vector3, std::StlNodeAlloc<Vector3> > * std::vector<std::vector<Vector3, std::StlNodeAlloc<Vect...` | 292 | 99.86 | 99.86 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| ui/UIList.cpp | `void std::__destroy_mv_srcs<std::vector<Vector3, std::StlNodeAlloc<Vector3> > *, std::vector<Vector3, std::...` | 84 | 99.71 | 99.71 | wrong map name - retail stride 0x10 calls pair<Symbol,vector<int>> dtor |
| ui/UIListDir.cpp | `void UIListDir::BuildDrawState(UIListWidgetDrawState &, UIListState const &, enum UIComponent::State, float...` | 1444 | 93.02 | 93.02 | codegen - getter folds (ArkFile::Size/Tell vs UIListState::MinDisplay/Provider), same-size vector _M_erase fold |
| ui/UIListSlot.cpp | `virtual void UIListSlot::StartScroll(int, bool)` | 128 | 99.84 | 99.84 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| ui/UIListState.cpp | `void UIListState::Scroll(int, bool)` | 652 | 99.97 | 99.97 | codegen - identical opcode multiset (registers/offsets only) |
| ui/UISlider.cpp | `?SetTypeDef@UISlider@@$4PPPPPPPM@A@AAXPAVDataArray@@@Z` | 16 | 73.75 | 100.00 | **FIXED** - retail UISlider has no SetTypeDef override (slot = thunk into UIComponent::SetTypeDef); map name corrected, row re-pairs at 100 |
| world/CameraManager.cpp | `void CameraManager::RandomizeCategory(ObjPtrList<CamShot, ObjectDir> &)` | 428 | 99.95 | 99.95 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/CameraManager.cpp | `virtual void * TransitionEvent::`scalar deleting destructor'(unsigned int)` | 88 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| world/CameraShot.cpp | `CamShotFrame * std::__uninitialized_copy<CamShotFrame *, CamShotFrame *>(CamShotFrame *, CamShotFrame *, Ca...` | 96 | 85.92 | 85.92 | wrong map name - retail stride 0x1c calls _Copy_Construct<Character::Lod>; a different instantiation |
| world/CameraShot.cpp | `CamShot::CamShot(void)` | 968 | 87.43 | 87.43 | not behaviour - retail leaves 0x40 (DC3-only mCrowdStateOverride; its "crowd_state_override" string is absent from retail) unconstructed, and Copy/Save (100%) never touch it. Unused member; not fixed |
| world/CameraShot.cpp | `void CamShot::Shake(float, float, Vector2const &, Vector3&, Vector3&)` | 1148 | 99.86 | 99.86 | codegen - identical opcode multiset (registers/offsets only) |
| world/CameraShot.cpp | `bool CamShot::SetPos(CamShotFrame &, RndCam *)` | 920 | 99.87 | 99.87 | codegen - identical opcode multiset (registers/offsets only) |
| world/CameraShot.cpp | `virtual void CamShot::Load(BinStream &)` | 2996 | 99.88 | 99.88 | codegen - stack offsets; Key<float>/Vector2, ObjPtr<T> and ObjPtrList<T> callee names are folds |
| world/CameraShot.cpp | `virtual void CamShot::StartAnim(void)` | 552 | 99.93 | 99.93 | codegen - identical opcode multiset (registers/offsets only) |
| world/CameraShot.cpp | `void CamShotFrame::BuildTransform(RndCam *, Transform &, bool) const` | 1024 | 96.28 | 96.28 | codegen - 0.0 literal materialised via addi; same values |
| world/CameraShot.cpp | `void CamShotFrame::Interp(CamShotFrame const &, float, float, RndCam *)` | 1772 | 99.06 | 99.06 | codegen |
| world/CameraShot.cpp | `void std::_Destroy<CamShotCrowd>(CamShotCrowd *)` | 4 | 95.00 | 95.00 | codegen - identical opcode multiset (registers/offsets only) |
| world/ColorPalette.cpp | `void std::vector<ColorSet, std::StlNodeAlloc<ColorSet> >::resize(unsigned int, ColorSet const &)` | 124 | 99.84 | 99.84 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/Crowd.cpp | `bool PropSync<WorldCrowd::CharData>(ObjList<WorldCrowd::CharData> &, DataNode &, DataArray *, int, enum Pro...` | 396 | 97.63 | 97.63 | codegen - identical opcode multiset (registers/offsets only) |
| world/Crowd.cpp | `WorldCrowd::CharData::CharData(Hmx::Object *)` | 92 | 80.13 | 80.13 | codegen |
| world/Crowd.cpp | `virtual void WorldCrowd::DrawShowing(void)` | 2072 | 88.68 | 88.68 | codegen - identical call skeleton; retail extra bytes are dead stores of temporaries to slot 0x50, equivalent fsel forms, a member re-dereference |
| world/Crowd.cpp | `void WorldCrowd::CharDef::Load(BinStream &)` | 132 | 96.52 | 96.52 | codegen - address form (lhz SYM@l vs base+offset) |
| world/Crowd.cpp | `void WorldCrowd::SetFullness(float, float)` | 612 | 94.50 | 94.50 | codegen - scheduling / evaluation order |
| world/Instance.cpp | `void WorldInstance::SavePersistentObjects(BinStream &)` | 676 | 97.63 | 97.63 | settled by W16-QA (DC3 spelling measured down); residue = one dead stb + list clear fold name |
| world/Instance.cpp | `virtual void * BandRetargetVignette::`vector deleting destructor'(unsigned int)` | 68 | 0.00 | 0.00 | placement - 0% COMDAT/template/thunk with no base symbol in this TU |
| world/LightPreset.cpp | `void LightPreset::Animate(float)` | 772 | 99.84 | 99.84 | codegen - identical opcode multiset (registers/offsets only) |
| world/LightPreset.cpp | `virtual void LightPreset::Copy(Hmx::Object const *, enum Hmx::Object::CopyType)` | 936 | 99.98 | 99.98 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/LightPreset.cpp | `virtual void LightPreset::Load(BinStream &)` | 3024 | 99.60 | 99.60 | codegen - static block addressed from a different base (lbl_82CC6E8C vs sLoading), stack offsets, vector<T*> fold names |
| world/Spotlight.cpp | `void Spotlight::BuildNGCone(Spotlight::BeamDef &, int)` | 1692 | 74.90 | 74.90 | codegen - FP form / loop shape; covered by the w8-q survey, not independently re-derived here |
| world/Spotlight.cpp | `void Spotlight::BuildNGQuad(Spotlight::BeamDef &, enum RndTransformable::Constraint)` | 964 | 86.61 | 86.61 | codegen - fmadds vs fsubs forms; agrees with the w7-ae/w7-bw survey |
| world/Spotlight.cpp | `void Spotlight::BuildNGSheet(Spotlight::BeamDef &)` | 1232 | 98.56 | 98.56 | settled by W16-QA (DC3 body adopted, 95.13 -> 98.56) |
| world/Spotlight.cpp | `void Spotlight::SetColor(int)` | 176 | 91.25 | 91.25 | codegen |
| world/Spotlight.cpp | `void Spotlight::UpdateTransforms(void)` | 1340 | 99.66 | 99.66 | codegen - identical opcode multiset (registers/offsets only) |
| world/Spotlight.cpp | `virtual bool Spotlight::SyncProperty(DataNode &, DataArray *, int, enum PropOp)` | 4728 | 99.39 | 99.39 | settled by W16-QA (DC3 body measured down); residue = SetObjConcrete fold name + stack offset |
| world/Spotlight.cpp | `virtual void Spotlight::Copy(Hmx::Object const *, enum Hmx::Object::CopyType)` | 716 | 99.64 | 99.64 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/Spotlight.cpp | `virtual void Spotlight::DrawShowing(void)` | 904 | 93.89 | 93.89 | codegen - scheduling / register choice |
| world/Spotlight.cpp | `virtual void Spotlight::Poll(void)` | 556 | 96.49 | 96.49 | codegen |
| world/Spotlight.cpp | `void Spotlight::BeamDef::Load(BinStream &)` | 440 | 98.73 | 98.73 | codegen - lhz SYM@l vs base+offset address form; >>Color/>>Vector4 and >>Key<float>/>>Vector2 are folds |
| world/SpotlightDrawer.cpp | `virtual void SpotlightDrawer::DrawShadow(void)` | 328 | 94.18 | 94.18 | codegen - inlined Plane(pos,(0,0,1)): retail -(x*0+y*0+z) vs ours -((x+y)*0+z); identical result for finite input |
| world/SpotlightDrawer.cpp | `virtual void SpotlightDrawer::DrawShowing(void)` | 136 | 80.59 | 80.59 | codegen; W16-QA measured the DC3 body down (80.59 -> 32.94) |
| world/SpotlightDrawer.cpp | `static void SpotlightDrawer::DrawLight(Spotlight *)` | 616 | 99.71 | 99.71 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/SpotlightDrawer.cpp | `static void SpotlightDrawer::Init(void)` | 112 | 94.11 | 94.11 | codegen |
| world/SpotlightDrawer.cpp | `void SpotlightDrawer::ApplyLightingApprox(BoxMapLighting &, float) const` | 444 | 92.97 | 92.97 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/SpotlightDrawer.cpp | `void SpotlightDrawer::ClearLights(void)` | 192 | 99.79 | 99.79 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/SpotlightDrawer.cpp | `void std::sort<SpotlightDrawer::SpotlightEntry *, ByColor>(SpotlightDrawer::SpotlightEntry *, SpotlightDraw...` | 112 | 99.82 | 99.82 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/SpotlightDrawer_NG.cpp | `void std::vector<SpotlightDrawer::SpotMeshEntry, std::StlNodeAlloc<SpotlightDrawer::SpotMeshEntry> >::_M_fi...` | 396 | 97.12 | 97.12 | codegen - element count of the inlined copy_backward computed via an extra divwu./add in ours |
| world/SpotlightDrawer_NG.cpp | `SpotlightDrawer::SpotMeshEntry * std::vector<SpotlightDrawer::SpotMeshEntry, std::StlNodeAlloc<SpotlightDra...` | 96 | 87.29 | 87.29 | codegen - register allocation, divw placement |
| world/SpotlightDrawer_NG.cpp | `virtual void NgSpotlightDrawer::ClearPostProc(void)` | 160 | 99.75 | 99.75 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/SpotlightDrawer_NG.cpp | `void NgSpotlightDrawer::RenderConeDefs(Spotlight *, Hmx::Color const &)` | 1324 | 95.27 | 95.27 | codegen - identical opcode multiset; callee names differ only by ICF fold / placeholder |
| world/SpotlightDrawer_NG.cpp | `void NgSpotlightDrawer::SetupXSection(Spotlight *, Spotlight::BeamDef const &)` | 2104 | 76.45 | 76.45 | codegen - FP association order (fmuls/fadds/fmsubs) and register pressure; body identical to DC3 (W16-QA) |
| world/SpotlightDrawer_NG.cpp | `void NgSpotlightDrawer::RenderScene(void)` | 588 | 99.59 | 99.59 | codegen - srawi. vs clrrwi. equivalent test; constant 0.125 |

## Per-row outcomes: anonymous rows (190)

| Class | Rows | Bytes | Outcome |
|---|---:|---:|---|
| `fn_` at 0% (unpaired; no base symbol carries the address) | 120 | 11,648 | identification work, out of scope (W16-PU's rule). By unit: WavMgr 26, CheatProvider 17, UI 6, OggMap 5, SongMgr 4, Sequence 4, LabelNumberTicker 4, HAQManager 4, MidiInstrument 4, StoreEnumeration 3, tomcrypt/aes.c 3, others ≤ 2 |
| `fn_` non-zero with an identical opcode multiset | 18 | – | codegen (frame offsets / registers) |
| `fn_` non-zero, opcode multiset differs (EH funclets, ≤ 60 B) | 52 | – | frame-offset differences (`subi r?, r?, frame` / member offset), except the three outliers above |
