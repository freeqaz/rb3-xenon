# The 42 unicorn `call_arg` survivors, adjudicated on retail bytes (2026-11-22)

Lane **CALLARG**, branch `callarg`, worktree `~/tmp/wt-callarg` (from main `3fa90c0b5`).
Brief: the 42 `call_arg` rows that `docs/decomp/UNIDIV_UNICORN_LOGIC_NATIVE_2026-11-22.md`
§4 left untriaged. For each one in native scope, decide from retail bytes and the current
objdiff diff whether our source passes a different argument than retail does, and fix the
real ones.

## 1. Population and method

The population is `docs/decomp/unicorn_matched_but_wrong_2026-09-10.csv`,
`div_class == call_arg` and `screen == SURVIVES_BOTH_SCREENS`: **42 rows** (as briefed). The
corpus carries no per-call detail (which call, which register), so each row was re-derived:

1. Resolve each 09-10 symbol to its address through the map as of 09-10 (`9314a27e7`, the
   last map commit on 09-10), then read the address's **current** name and score from a
   fresh full build of this worktree (`report.json`, ruler `name_check`, 55,265 matched). If
   the 09-10 name has moved to a new address, score it there too (53 rows in all).
2. Re-diff every such row with `bin/objdiff-cli diff -f json --include-instructions -c
   functionRelocDiffs=all` and list every instruction whose opcode, operands or relocation
   target differs, including the placeholder targets that `name_check` forgives.
3. Read retail bytes (`tools/va_disasm.py`, plus a PE reader for `.rdata`) behind each
   difference that could feed an argument register.
4. For renamed addresses, bisect the map history (1,086 commits since `9314a27e7`) for the
   commit that changed the name.

Prediction before measuring: like UNIDIV's `return_value`/`error` rows, most would be wrong
map names corrected since, and few if any real argument bugs would remain.

## 2. What the current diff shows

- **51 of 53 rows are fuzzy 100 at `name_check`.** Under `functionRelocDiffs=all` the only
  differences in them are (a) placeholder data targets and (b) named-callee pairs that the
  alias table forgives (proven ICF folds, e.g. `hash_map` ctors, `MemOrPoolAlloc` vs
  `MemOrPoolAllocSTL`, `operator delete` vs `RndCubeTex::operator delete`).
- Every placeholder that can reach an argument was checked on retail bytes:
  `lbl_82088830` = `"content_installed"` (the `RemoveSink`/`AddSink` symbol in the
  MusicLibraryStore ctor and `ClearPreview`); `lbl_820009FC` = 1.0f, `lbl_82000D78` = 0.0f,
  `lbl_8200EDA8` = -1.0f; the remaining `lbl_` targets are vtables, vbtables, RTTI type
  descriptors (`dynamic_cast` arguments in `BandUserMgr::GetBandUser`/`GetLocalBandUser`,
  `UISlider::Copy`) and globals (`ThePlatformMgr`, `gNullStr`, `TheShaderMgr`,
  `TheSplasher`, `sBloom`, MemcardMgr's local static). All agree with our named symbol.
  Three callees are unidentified placeholders, with byte-identical argument setup:
  `fn_8246B740` (ours `RndShaderMgr::InitShaders`), `fn_828406B8` (ours
  `XBackgroundDownloadSetMode`), `fn_82B693F0` (ours `FxSend360::UpdateVolumes`).
- **Two rows differ:** 0x822734d8 (§3) and `NetCacheMgrXbox::~NetCacheMgrXbox` at
  0x827d7360 (fuzzy 85.19: a vtable store retail has and we do not; `_Xbox`/network,
  360-ONLY, out of scope, not touched).

## 3. The one live defect: 0x822734d8 is a wrong map name (fixed)

The map named 0x822734d8 `??2Node@?$ObjPtrList@VRndGroup@@VObjectDir@@@@SAPAXI@Z`. Retail
bytes:

```
822734d8  mr  r4, r5
822734dc  b   0x827bcd38        ; ?MemAlloc@@YAPAXHH@Z  (size, align)
```

That is a **three-argument** function `f(size, x, align) -> MemAlloc(size, align)`. A byte
scan of `.text` finds exactly one caller, 0x82ab12a0, in Quazal /Od STL code, which loads
`r3 = n`, `r4 = 0x82180EB8` (`"__new_alloc"`) and `r5 = 0`. `Node::operator new(size_t)` takes
one argument, and in the match build is `PoolAlloc(s, s)` (`src/system/obj/ObjPtr_p.h`,
backed by the `PoolAlloc(0xc,0xc)` call in AddObject). So the 09-10 `call_arg` report was
real only as a pairing defect: our body passes `(s, s)` to `PoolAlloc`, and the retail
function at that address passes `(r3, r5)` to `MemAlloc`, but they are different functions.
**Our source is right; the map was wrong.** The 08-17 allocator audit called this address
an 8-byte ICF fold survivor of the operator new and recommended leaving it. The bytes rule
that out, because the two bodies differ in both callee and argument.

The fix (commit `c6fe40ef6`) sets the entry to `null` rather than renaming it, because the
true name is unidentified and naming is a bet.

**ab_measure** (`--from-dirty`; leg A = without, leg B = with; ruler `name_check`, objdiff
`sha256:ec3badc0f33323e9`, both legs at a split fixed point, 0 recompiles):

| | leg A (without) | leg B (with) | change (B − A) |
|---|---:|---:|---:|
| matched_functions | 55,265 | 55,265 | **0** |
| honest (matched − masked_equal) | 29,905 | 29,905 | 0 |
| code% | 59.622105 | 59.622105 | 0 |
| fuzzy | 64.704510 | 64.704445 | −0.000065 pp (the false 95% row is unpaired) |
| `none` ruler | 55,321 / 60.015200 | 55,320 / 60.015200 | −1 fn, 0 B (not a function-count control) |

Pre-registered Δ0 (or +1 if the Quazal caller had been charged for the name). Measured Δ0.
This is an accuracy fix: it removes a false partial pairing. Run directory:
`~/tmp/wt-callarg/.ab_measure_runs/20261122-185611-callarg_null_822734d8-2935213/`, log
`~/tmp/callarg_ab1.log`.

## 4. Verdicts, all 42 rows

Scope is `scripts/native_scope_map.py` classify() on the 09-10 unit's source path. "Now" is
fuzzy/mpn at `name_check` in this build. WRONG MAP NAME means that the 09-10 emulator diffed
our body against a different retail function. The rename commit is given in each such row.
Where the old name is "unplaced", its true retail address is not in the map today, so the
09-10 row carries no evidence about our body.

| # | 09-10 row (unit) | addr | scope | now | verdict |
|---|---|---|---|---|---|
| 0 | `??_EBandCharDescTest` vtordisp thunk (BandCharacter) | 0x8227b028 | VIA-DC3 | `Song::Load` thunk 100 | WRONG MAP NAME, fixed `3ffdbbf79` (09-14 vtordisp permutation); old name unplaced |
| 1 | `SongMgr::SongMgr` (BandSongMgr) | 0x82579c08 | SOON | 100 / 100 | MATCHES RETAIL now: placeholder vtables only; hash_map ctor callees alias-forgiven |
| 2 | `BandUser::SyncProperty` thunk | 0x8268ed50 | SOON | `NullLocalBandUser::CanSaveData` thunk 100 | WRONG MAP NAME, fixed `8af030c48`; old name at 0x8268c978 is 100 |
| 3 | `vector<ActionRec>` copy ctor (ButtonHolder) | 0x827b8f50 | VIA-DC3 | 100 / 100 | MATCHES RETAIL; two alias-forgiven callees |
| 4 | `??_GCharDriverMidi` | 0x823cc500 | VIA-DC3 | 100 / 100 | MATCHES RETAIL (byte-identical, no reloc diffs) |
| 5 | `??__FsLoadedFile` (CharLipSync) | 0x82682668 | VIA-DC3 | `BandUserMgr::GetBandUser` 100 | WRONG MAP NAME, fixed `a79b773fc`; old name unplaced |
| 6 | `CharWeightable::CharWeightable` | 0x823aeb30 | VIA-DC3 | 100 / 100 | MATCHES RETAIL; was already fuzzy 100 at 09-10 (harness artifact); `lbl_820009FC` = 1.0f |
| 7 | `Cheat` ctor (CheatProvider) | 0x823f4618 | VIA-DC3 | nulled | WRONG MAP NAME, removed `ab38d5c03` (W16-NU false `vector<Cheat>` names) |
| 8 | `DataSyncReloadLocale` (DataFunc) | 0x827c4aa0 | CORE | `MemTrackReportDF` 100 | WRONG MAP NAME, fixed `7965d2eb8`; old name unplaced |
| 9 | `UIList::Speed` (DepthBuffer3D) | 0x826ebf20 | (unit gone) | `RealGuitarGemPlayer::SetRGState` 100 | WRONG MAP NAME, fixed `dd2c1320b`; old name unplaced |
| 10 | `??__F sBloom` (Env_NG) | 0x82b870e0 | VIA-DC3 | `NgRnd::ReInit` 100 | WRONG MAP NAME, fixed `ed1f5fdd4`; old name at 0x82c4baf0 is 100 |
| 11 | `vector<DrivenPropertyEntry>::clear` (Font) | 0x82475a00 | VIA-DC3 | `RndFont::GetKerning` 100 | WRONG MAP NAME, fixed `0e2342c74`; old name unplaced |
| 12 | `JsonRealloc` (JsonMemory) | 0x82682688 | 360-ONLY | `BandUserMgr::GetLocalBandUser` 100 | WRONG MAP NAME, fixed `a4923ec44`; `JsonRealloc` at 0x82c30b58 is 100 |
| 13 | `RndTransformable::Print` thunk (LightHue) | 0x824e9390 | VIA-DC3 | `WorldReflection::Highlight` thunk 100 | WRONG MAP NAME, fixed `3ffdbbf79`; old name unplaced |
| 14 | `RndDir::Save` thunk (MatAnim) | 0x822d3908 | VIA-DC3 | `EndingBonus::Save` 100 | WRONG MAP NAME, fixed `066b80f57`; old name at 0x824048b8 is 100 |
| 15 | `ObjPtrList<RndGroup>::Node::operator new` (Memory_Xbox) | 0x822734d8 | 360-ONLY unit, engine name | was 95 | **WRONG MAP NAME, STILL LIVE: fixed here** (§3, `c6fe40ef6`); our source is right |
| 16 | `~AccomplishmentConditional` (MeshAnim) | 0x827d7360 | VIA-DC3 | `~NetCacheMgrXbox` 85.19 | WRONG MAP NAME, fixed `047191778`; old name at 0x8266af90 is 100. The 85.19 row now there is 360-ONLY, out of scope |
| 17 | `vector<DataArrayPtr>::_M_insert_overflow_aux` (MetaMusic) | 0x82710dc8 | VIA-DC3 | `vector<ObjDirPtr>` variant 100 | WRONG MAP NAME, fixed `82b3391d3`; old name at 0x8242c070 is 100 |
| 18 | `??_DMeterDisplay` | 0x82738a48 | (unit gone) | `??_DDxMesh` 100 | WRONG MAP NAME, fixed `3340d7139`; old name at 0x8231ad28 is 100 |
| 19 | `__destroy_aux<MidiChannel>` (MidiSynth) | 0x82719c88 | VIA-DC3 | `StreamNull::Play` 100 | WRONG MAP NAME, fixed `24cef13c8`; old name unplaced |
| 20 | `operator>>(BinStreamRev&, Key<Weight>&)` (Morph) | 0x82482f50 | VIA-DC3 | `BinStream&` overload 100 | WRONG MAP NAME (overload), fixed `080e7845f` |
| 21 | `??_GSetUserDifficultyMsg` (MusicLibrary) | 0x8253ab38 | SOON | `~SetlistProvider` 100 | WRONG MAP NAME, fixed `53aea21a4`; old name at 0x82677f28 is 100 |
| 22 | `MusicLibraryStore::MusicLibraryStore` | 0x825bd458 | SOON | 100 / 100 | MATCHES RETAIL; `"content_installed"`, `gNullStr`, `ThePlatformMgr` verified |
| 23 | `MusicLibraryStore::ClearPreview` | 0x825bc908 | SOON | 100 / 100 | MATCHES RETAIL; `RemoveSink(this, "content_installed")` verified; `fn_828406B8` callee unidentified, args identical |
| 24 | `NetLoaderRef` copy ctor (NetCacheMgr) | 0x82766828 | CORE | `list<EventSinkElem>::erase` 100 | WRONG MAP NAME, fixed `67e7d0cb4`; old name at 0x827cd8b8 is 100 |
| 25 | `~NetCacheMgrXbox` (NetCacheMgr_Xbox) | 0x8245d4c0 | 360-ONLY | `~RndWind` 100 | WRONG MAP NAME, fixed `047191778`; the re-homed old row is 85.19 (out of scope, see §2) |
| 26 | `PracticeSection::ClearSteps` | 0x8250d4c0 | (unit gone) | `EntityUploader::RecordSubmissionTime` 100 | WRONG MAP NAME, fixed `95b41ad8a`; old name unplaced |
| 27 | `ResumeFunc` (Splash) | 0x82742c98 | VIA-DC3 | `TaskMgrDeltaSeconds` 100 | WRONG MAP NAME, fixed `617375706`; old name at 0x82741c38 is 100 |
| 28 | `StlNodeAlloc<String>::allocate` (Str) | 0x827be570 | CORE | `String::operator=(Symbol)` 100 | WRONG MAP NAME, fixed `f9b0446eb`; old name unplaced |
| 29 | `TexMovie::TexMovie` | 0x826b03d8 | VIA-DC3 | `RGTrainerPanel` ctor 100 | WRONG MAP NAME, fixed `757a3091f`; old name at 0x82746550 is 100 |
| 30 | `TexMovie::Reset` | 0x822b9308 | VIA-DC3 | `CrowdMeterIcon::SetIcon` 100 | WRONG MAP NAME, fixed `27ee22636`; old name unplaced |
| 31 | `TrackWatcherImpl::KillSustainForSlot` | 0x82796c10 | SOON | 100 / 100 | MATCHES RETAIL (no diffs) |
| 32 | `TrackWatcherImpl::SustainedGemToKill` | 0x82796b90 | SOON | 100 / 100 | MATCHES RETAIL (no diffs) |
| 33 | `UIList::IsScrolling` | 0x826662e0 | VIA-DC3 | `FriendsProvider::Reload` 100 | WRONG MAP NAME, fixed `9fcd8cde4`; old name at 0x827f7f08 is 100 |
| 34 | `UIListWidget::UIListWidget` | 0x82822c90 | VIA-DC3 | 100 / 100 | MATCHES RETAIL; 1.0f/0.0f constants verified |
| 35 | `UISlider::Copy` | 0x82809808 | VIA-DC3 | 100 / 100 | MATCHES RETAIL; `dynamic_cast` RTTI args verified |
| 36 | `UISlider::SetTypeDef` thunk | 0x82809bd8 | VIA-DC3 | `UIComponent::SetTypeDef` thunk 100 | WRONG MAP NAME, fixed `f15a11bfd` (UISlider inherits it) |
| 37 | `TokenRedemptionPanel::Unload` | 0x8263fde8 | SOON | 100 / 100 | MATCHES RETAIL; one alias-forgiven callee |
| 38 | `getKeyImpl` (keygen_xbox) | 0x82725388 | 360-ONLY | 100 / 100 | OUT OF SCOPE; byte-identical anyway |
| 39 | `MemcardMgr::SaveLoadAllComplete` | 0x827ab850 | VIA-DC3 | 100 / 100 | MATCHES RETAIL; local-static placeholders only |
| 40 | `OggMap::Read` | 0x82bb1d68 | VIA-DC3 | 100 / 100 | MATCHES RETAIL (no diffs) |
| 41 | `StlNodeAlloc<LevelData>::allocate` (FxSendMeterEffect) | 0x82b63ed8 | 360-ONLY | `FxSendMeterEffect360::UpdateMix` 100 | WRONG MAP NAME, fixed `ed1f5fdd4`; out of scope |

**Summary: 0 source bugs where our code passes a different argument than retail.**

- 27 rows were wrong map names that other lanes corrected between 09-11 and 10-06.
- 1 row, #15, was a wrong map name still live today. It is nulled here, Δ0.
- 13 rows have kept their name and now match retail bytes. 6 of them (`CharWeightable` 100,
  `OggMap::Read` 99.96, `UIListWidget` ctor 99.91, `vector<ActionRec>` copy ctor 99.83,
  `SustainedGemToKill` 99.68, `KillSustainForSlot` 99.29) were already at or near fuzzy 100
  on 09-10, so their 09-10 divergence was most likely a harness artifact. The other 7
  reached 100 through source work since then.
- 1 row (#38) is out of scope and byte-identical.

The only sub-100 row left in this set, `~NetCacheMgrXbox`, is out of scope.

## 5. What I did not do

- Did not re-run the unicorn harness. The verdicts rest on retail bytes, the map history
  and the current objdiff diff. `decomp.db`'s unicorn columns were not rewritten.
- Did not find the true names of the old symbols marked "unplaced" or of 0x822734d8 (a
  Quazal-side `(size, name, align)` allocator shim). Naming is a bet and neither is in
  native scope.
- Did not touch `~NetCacheMgrXbox` (360-ONLY) or confirm the three placeholder callees
  (`fn_8246B740`, `fn_828406B8`, `fn_82B693F0`). Their argument setup is byte-identical.
- No `src/` change, so no native gate was needed. I ran no native binaries.
- Did not merge to main.
