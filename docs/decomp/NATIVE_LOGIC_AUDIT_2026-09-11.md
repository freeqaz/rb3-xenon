# Native-linked sub-100 BODY audit — lane W4-A, 2026-09-11

Branch `w4-native-logic`, worktree `~/tmp/wt-w4-a`, based on main `3ab3f494`
(asserted ancestor). Ruler: `name_check` (graded), read from `report.json`
`provenance.diff_config`. Baseline at the lane's first full build:
`matched_functions` 42,505 / `matched_code` 3,834,712 B / 37.42659 % /
fuzzy 48.995964 / `total_code` 10,245,956 / `total_functions` 69,219.

**Goal.** W3-A drained the ≤3-charged-site stratum of the 471 native-linked
TUs (every divergence there was the MAP). This lane opens the other stratum:
named rows in those TUs with `fuzzy < 100` and **more than three charged
sites** — bodies whose logic may differ — and adjudicates each opened row on
retail bytes (dtk `.s`, keyed on `.fn fn_<ADDR>`), using rb3-Wii / DC3 only as
oracles. Classes: CODEGEN (same semantics, different shape — not ground),
STRUCT (field/offset/type), LOGIC (branch / constant / callee / statement —
the bug class), MISSING_BODY (ours absent or stubbed).

## 1. Population (provenance: graded ruler, `crossing_worklist.diff_many` over the shared cache `~/tmp/crossing_worklist/diffs`, 1,937 rows diffed, 0 misses; `~/tmp/w4a/pop.py`)

The 471 TUs map to **415** `objdiff.json` units via `metadata.source_path`
(56 TUs are compiled but have no unit — unpinned, e.g. `meta_band/Utl.cpp`,
`beatmatch/TrackType.cpp`, `bandobj/Band.cpp`, most `hamobj/*` — not
auditable; W3-A counted 416/55 at `814e3a60`, one unit drifted between bases).

| population (415 native units) | rows | bytes |
|---|---:|---:|
| named rows with `0 < fuzzy < 100` | 1,937 | 648,452 |
| … with 1–3 charged sites (W3-A's stratum) | 1,303 | 275,508 |
| … with **> 3 charged sites (this lane)** | **634** | **372,944** |
| … with 0 charged sites | 0 | 0 |

By native-path tier (rows / bytes): **T1** loading, DTA, utl/obj/os/beatmatch/midi
**128 / 62,316** · **T2** game scoring/bandtrack **37 / 25,964** · **T3** `synth/`
**5 / 1,288** · T4 rndobj/char/world/bandobj/math/flow 413 / 265,492 · T5
ui/meta_band/meta/hamobj 51 / 17,884. Dominant charged-arm class over the
634: STRUCTURAL 345 rows / 163,212 B · REGALLOC 167 / 117,616 · IMMEDIATE 55 /
41,468 · SYMBOL 55 / 40,128 · ARITH_COMMUTE 11 / 9,824 · BRANCH 1 / 696.

## 2. Ranking (tier, then size-if-it-crosses; `~/tmp/w4a/ranking.txt`)

### Tier 1 — song/DTA/milo loading, utl, obj, os, beatmatch, midi (top 40)
| # | size B | charged | fuzzy | mpn | dominant arm | unit | symbol |
|---:|---:|---:|---:|---:|---|---|---|
| 1 | 8068 | 1110 | 70.53 | 71.9 | STRUCTURAL | `DataFunc` | `?DataInitFuncs@@YAXXZ` |
| 2 | 3112 | 7 | 99.96 | 100.0 | SYMBOL | `PlatformMgr` | `?Handle@PlatformMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z` |
| 3 | 3092 | 412 | 84.71 | 86.5 | REGALLOC | `system/obj/Dir` | `?PreLoad@ObjectDir@@UAAXAAVBinStream@@@Z` |
| 4 | 2108 | 168 | 90.75 | 92.1 | REGALLOC | `system/obj/Dir` | `?Save@ObjectDir@@UAAXAAVBinStream@@@Z` |
| 5 | 1836 | 99 | 91.37 | 92.0 | STRUCTURAL | `VocalNoteList` | `?NotesDone@VocalNoteList@@QAAXABVTempoMap@@_N@Z` |
| 6 | 1504 | 10 | 99.77 | 100.0 | ARITH_COMMUTE | `HDCache` | `?Init@HDCache@@QAAXXZ` |
| 7 | 1472 | 4 | 99.95 | 99.9 | SYMBOL | `system/obj/Utl` | `?CopyTypeProperties@@YAXPAVObject@Hmx@@0@Z` |
| 8 | 1304 | 116 | 89.58 | 91.2 | REGALLOC | `Locale` | `?Init@Locale@@QAAXXZ` |
| 9 | 1244 | 34 | 98.56 | 99.1 | REGALLOC | `system/os/UsbMidiGuitar` | `?Poll@UsbMidiGuitar@@SAXXZ` |
| 10 | 1192 | 69 | 95.44 | 96.1 | REGALLOC | `Song` | `?SyncState@Song@@QAAXXZ` |
| 11 | 1164 | 95 | 85.74 | 86.2 | STRUCTURAL | `UsbMidiKeyboard` | `?Poll@UsbMidiKeyboard@@SAXXZ` |
| 12 | 1156 | 333 | 32.07 | 35.5 | STRUCTURAL | `PropSync` | `?PropSync@@YA_NAAVMatrix3@Hmx@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` |
| 13 | 1104 | 16 | 96.25 | 96.3 | STRUCTURAL | `SongParser` | `?StartVocalNote@SongParser@@QAAXHEPBD@Z` |
| 14 | 972 | 17 | 99.93 | 99.9 | IMMEDIATE | `MidiReader` | `?ReadMetaEvent@MidiReader@@AAAXHEAAVBinStream@@@Z` |
| 15 | 960 | 4 | 99.92 | 99.9 | SYMBOL | `SongParser` | `?Reset@SongParser@@QAAXXZ` |
| 16 | 932 | 17 | 96.91 | 97.1 | STRUCTURAL | `ChunkStream` | `?Eof@ChunkStream@@UAA?AW4EofType@@XZ` |
| 17 | 892 | 79 | 85.87 | 87.0 | REGALLOC | `File` | `?RecursePatternInternal@@YAXPBDP6AX00@Z_N2@Z` |
| 18 | 812 | 86 | 81.83 | 82.9 | STRUCTURAL | `Joypad_Xinput` | `?ReadSingleXinputJoypad@@YA?AW4JoypadType@@HHPAIPAD11111QAM2QAE@Z` |
| 19 | 792 | 197 | 5.67 | 6.0 | STRUCTURAL | `File` | `FileMakePath` |
| 20 | 780 | 17 | 95.68 | 95.9 | STRUCTURAL | `PropSync` | `?DrawShowing@WorldDir@@UAAXXZ` |
| 21 | 720 | 10 | 99.56 | 100.0 | ARITH_COMMUTE | `SongData` | `?ValidateVocalSPPhrases@SongData@@QAAXXZ` |
| 22 | 696 | 56 | 96.38 | 96.8 | IMMEDIATE | `NetCacheMgr` | `?AddLoaderRef@NetCacheMgr@@IAAPAUNetLoaderRef@@PBDW4RefType@1@W4NetLoaderPos@@@Z` |
| 23 | 696 | 6 | 98.68 | 98.8 | BRANCH | `SongParser` | `?ParseText@SongParser@@QAAXHPBD@Z` |
| 24 | 656 | 43 | 95.70 | 96.8 | REGALLOC | `SongData` | `?UnflipGems@SongData@@QAAXHHH@Z` |
| 25 | 648 | 147 | 44.25 | 46.3 | STRUCTURAL | `SongInfoCopy` | `??0SongInfoCopy@@QAA@XZ` |
| 26 | 644 | 161 | 2.57 | 2.7 | STRUCTURAL | `MemMgr` | `?MemAlloc@@YAPAXHH@Z` |
| 27 | 632 | 8 | 96.61 | 96.7 | STRUCTURAL | `AsyncFile` | `?PrintDiscFile@@YAXPBD@Z` |
| 28 | 632 | 182 | 2.09 | 3.0 | STRUCTURAL | `DirLoader` | `?OpenFile@DirLoader@@AAAXXZ` |
| 29 | 624 | 38 | 97.95 | 99.2 | REGALLOC | `MemHeap` | `?Print@MemHeap@@QAAXAAVTextStream@@_N@Z` |
| 30 | 532 | 12 | 98.31 | 98.6 | REGALLOC | `system/obj/Dir` | `?ResetViewports@ObjectDir@@QAAXXZ` |
| 31 | 520 | 6 | 99.77 | 99.8 | SYMBOL | `File` | `FileInit` |
| 32 | 516 | 58 | 92.35 | 94.3 | REGALLOC | `Archive` | `?Merge@Archive@@QAAXAAV1@@Z` |
| 33 | 508 | 83 | 68.13 | 68.8 | STRUCTURAL | `TrackWatcherImpl` | `?Load@RndParticleSysAnim@@UAAXAAVBinStream@@@Z` |
| 34 | 500 | 23 | 99.00 | 100.0 | REGALLOC | `MemTracker` | `?DiffDump@MemTracker@@QAAXAAVTextStream@@@Z` |
| 35 | 484 | 14 | 98.02 | 98.3 | IMMEDIATE | `MidiParser` | `?PushIdle@MidiParser@@AAAXMMHVSymbol@@@Z` |
| 36 | 484 | 72 | 77.99 | 80.0 | REGALLOC | `Archive` | `?Enumerate@Archive@@QAAXPBDP6AX00@Z_N0@Z` |
| 37 | 464 | 100 | 38.68 | 39.8 | STRUCTURAL | `DirLoader` | `??0DirLoader@@QAA@ABVFilePath@@W4LoaderPos@@PAVCallback@Loader@@PAVBinStream@@PAVObjectDir@@_N@Z` |
| 38 | 444 | 71 | 58.81 | 60.3 | STRUCTURAL | `BandSongMgr` | `?SongAudioData@BandSongMgr@@UBAPAVSongInfo@@H@Z` |
| 39 | 440 | 90 | 60.54 | 62.6 | STRUCTURAL | `system/obj/Dir` | `?Iterate@ObjectDir@@IAAXPAVDataArray@@_N@Z` |
| 40 | 432 | 4 | 97.92 | 98.0 | STRUCTURAL | `Archive` | `?GetFileInfo@Archive@@QAA_NPBDAAHAA_K11@Z` |

### Tier 2 — game scoring / bandtrack (top 12 of 37)
| # | size B | charged | fuzzy | mpn | dominant arm | unit | symbol |
|---:|---:|---:|---:|---:|---|---|---|
| 1 | 3436 | 94 | 97.16 | 97.5 | SYMBOL | `ChordbookPanel` | `?DisplayChord@ChordbookPanel@@QAAXI@Z` |
| 2 | 3388 | 174 | 93.83 | 94.6 | REGALLOC | `VocalPlayer` | `?Poll@VocalPlayer@@UAAXMABVSongPos@@@Z` |
| 3 | 2724 | 8 | 99.90 | 100.0 | REGALLOC | `GemPlayer` | `?Hit@GemPlayer@@UAAXHMHIW4GemHitFlags@@@Z` |
| 4 | 2332 | 71 | 97.94 | 98.6 | REGALLOC | `VocalPlayer` | `?HandlePhraseEnd@VocalPlayer@@QAAXM@Z` |
| 5 | 1584 | 8 | 99.40 | 99.5 | REGALLOC | `GemManager` | `?UpdateLeftyFlip@GemManager@@QAAX_N@Z` |
| 6 | 1432 | 30 | 97.72 | 97.9 | IMMEDIATE | `GemTrack` | `?DrawTrackElements@GemTrack@@QAAXHH@Z` |
| 7 | 1064 | 199 | 65.67 | 68.1 | STRUCTURAL | `Tail` | `?UpdateVerts@Tail@@QAAXM_N@Z` |
| 8 | 908 | 10 | 99.78 | 99.8 | SYMBOL | `Performer` | `??0Stats@@QAA@ABV0@@Z` |
| 9 | 792 | 8 | 99.28 | 99.5 | SYMBOL | `band3/game/Game` | `??1Game@@UAA@XZ` |
| 10 | 752 | 43 | 96.88 | 98.0 | REGALLOC | `band3/bandtrack/TrackPanel` | `?Poll@TrackPanel@@UAAXXZ` |
| 11 | 748 | 9 | 97.26 | 97.3 | STRUCTURAL | `GemPlayer` | `?UpdateCrowdMeter@GemPlayer@@QAAXMH@Z` |
| 12 | 684 | 13 | 95.73 | 95.8 | STRUCTURAL | `GemTrack` | `?DrawBeatLine@GemTrack@@QAAXVSymbol@@HH_N@Z` |

### Tier 3 — `synth/` (all 5)
| # | size B | charged | fuzzy | mpn | dominant arm | unit | symbol |
|---:|---:|---:|---:|---:|---|---|---|
| 1 | 440 | 5 | 99.95 | 100.0 | IMMEDIATE | `SampleData` | `?Load@SampleData@@QAAXAAVBinStream@@ABVFilePath@@@Z` |
| 2 | 324 | 4 | 99.75 | 99.8 | SYMBOL | `SampleData` | `?_M_insert_overflow_aux@?$vector@VSampleMarker@@V?$StlNodeAlloc@VSampleMarker@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAVSampleMarker@@ABV3@ABU__false_type@2@I_N@Z` |
| 3 | 240 | 25 | 92.55 | 94.9 | REGALLOC | `system/synth/FxSend` | `?BuildChainVector@FxSend@@UAAXAAV?$vector@PAVFxSend@@V?$StlNodeAlloc@PAVFxSend@@@stlpmtx_std@@@stlpmtx_std@@@Z` |
| 4 | 220 | 20 | 74.05 | 74.1 | STRUCTURAL | `Sequence` | `?Load@Sequence@@UAAXAAVBinStream@@@Z` |
| 5 | 64 | 7 | 67.19 | 67.2 | STRUCTURAL | `SampleData` | `??0SampleMarker@@QAA@XZ` |

## 3. Per-row adjudication

Every row below was read against retail bytes; "fix" means source changed
in this lane. Predicted Δ is the pre-registration written before each
`ab_measure --from-dirty` run; measured Δ is that run's whole-binary result.

| row | unit | class | evidence (retail bytes) | fix | predicted Δ | measured Δ | native effect |
|---|---|---|---|---|---|---|---|
| `DirLoader::OpenFile` (632 B, 2.10 %) | DirLoader | **LOGIC** (DC3 moved code) | 0x82755A38 opens with `lbz *mFile; beq → mRoot = FilePath::sRoot (lbl_82E06E14 via String::operator=); else FileGetPath (fn_82516550: copies into static lbl_82CCA0B0) → strip "/gen" (lbl_82106BA0 = 2F 67 65 6E) → FileMakePath(FileRoot(), buf)`; then the host-file/ChunkStream block we already had. Restores with `li r3,1; bl SetUsingCD` — never calls `UsingCD()` (rb3-Wii `#ifdef MILO_DEBUG`). | mRoot derivation moved from ctor to OpenFile (rb3-Wii spelling); `using_cd` = `true` unless `MILO_DEBUG && HX_NATIVE` | part of +1 / +464 | 2.10 → 93.83 (28 B residue: our in-TU `PathName` duplicate is inlined, retail `bl PathName`) | a sub-DirLoader constructed inside a parent `LoadDir()` `FilePathTracker` captured the PARENT's root at construction; retail resolves `FileRoot()` when polled |
| `DirLoader::DirLoader` (464 B, 38.68 %) | DirLoader | **LOGIC + STRUCT-init** | 0x82755AF8 stores nothing at 0x3c/0x40 (`mRev`/`mCounter`) nor 0xa2/0xa3 (`mHasEditorDir`/`mSubDir`, absent from rb3-Wii); calls `fn_8275BD08` = `Hmx::Object::AddRef(ObjRefOwner*)` (`RefOwner()` vcall at slot 1, compare with this, list insert at Object+0x20, after the MSVC virtual-base adjust `lwz 4(r11); lwz 4(r10); add`) on `mProxyDir` before `SetLoader(this)` | drop `mRev(0), mCounter(0)`; `mHasEditorDir/mSubDir` initialisers + their two use sites (`SetSubDirFlag` in LoadDir, `ReadEditorDirDead` rev>0x1f in LoadObjs) `HX_NATIVE`-only; X360-only `mProxyDir->AddRef(this)` / dtor `Release(this)` | +1 / +464 | **38.68 → 100.00**; run: Δmatched **+3** (+2 `masked_equal` funclets) / Δhonest +1 / **+504 B** / +0.004917 pp | none (both members are stream-read before use; natively `mProxyDir` is an `ObjOwnerPtr`) |
| `FileMakePath` (792 B, 5.67 %) | File | CODEGEN (inline structure) | retail = our `FileMakePathBuf` body (same static-buffer aliasing guard) inlined into `FileMakePath` after `MainThread()`; retail File unit has no named `FileMakePathBuf` (candidate unnamed 904 B row `fn_82517718`) | none | — | — | none |
| `MemAlloc(int,int)` (644 B, 2.57 %) | MemMgr | **MISSING_BODY** (known) | ours is a 20 B stub under `#ifndef HX_NATIVE`; SRCPORT-1 handoff comment in `utl/MemMgr.cpp` carries the full retail reconstruction | none (native uses the malloc path; match-only work, out of this lane's budget) | — | — | none |
| `DataInitFuncs` (8,068 B, 70.53 %) | DataFunc | **LOGIC** (registry) + CODEGEN residue | retail registers **154** builtins in one order; ours registered 151. Registry diffed name-by-name from `.rdata` bytes (`~/tmp/w4a/datafuncs.json`): missing `notify_beta` (retail fn folded onto `??0DataNode` ⇒ body is `return 0`), `match_pattern` (fn_82761D08), `match_any_pattern` (fn_82761D60). The seven stack-built names are `"O64".."O69","O70"` and ours already builds them (`magic[]`). Order of all common names identical. | three `DEF_DATA_FUNC`s + registrations at retail positions (after `notify`; between `has_any_substr` and `find_substr`) | not crossing | 70.53 → 71.45 | **natively `{match_pattern}` / `{match_any_pattern}` / `{notify_beta}` were unknown DTA functions** |
| `DataMatchPattern` (88 B), `DataMatchAnyPattern` (164 B) | DataFunc | new bodies from bytes | `Str(1)`/`Str(2)` (right-to-left) → `fn_82757FC0`; any-variant loops `Array(2)` (`0x8274b0f8` is mapped `DataNode::Int` but reads `mSize` at +8 of its result ⇒ it is `Array`, an ICF fold) | as read; map names added | +2 / +252 | **100.000 / 100.000**, both crossed in run 2 | as above |
| `StringMatchesFilter` (184 B, unnamed `fn_82757FC0` in the DirLoader TU) | DirLoader | new body (rb3-Wii `obj/Utl.cpp:382`, dropped by DC3) | `String a(s), b(pat); ToLower both; find('*')==npos ? a.contains(b) : FileMatch(a,b)` | added to `obj/Utl.{h,cpp}` (native) + DirLoader reunification block (X360); map name | +1 mpn / +0 B | 99.891 % on BOTH keys (a wrong-callee price): retail spells the call `?contains@FixedString@@`, ours `?contains@String@@` — our `String::contains` (`find(str,0)`) is a distinct definition from `FixedString::contains` (`find(str)`); did not cross | provides the predicate above |
| `DataInt` (124 B, 63.97 % once named `0x8275F8B0`) | DataFunc | **LOGIC** (DC3-newer) | retail: `type==kDataSymbol(5) → atoi(UncheckedStr)`; `4/0 → UncheckedInt`; else `LiteralFloat`. Ours also accepted `kDataString (0x12)` via checked `Str()` | rb3-Wii body | +1 / +124 | **100.000**, crossed in run 2 | natively `{int "12"}` returned 12; retail sends a string node to `LiteralFloat` (fails) — fidelity, not a crash class |
| `ObjectDir::Iterate` (440 B, 60.54 %) | Dir | **LOGIC** (MILO_DEBUG force-define + DC3 memo) | retail has no `SystemConfig("objects")`/`FindArray` and no `sSuperClassMap::_M_find`; one `bl` per object (IsASubclass) then `Type()==sym2`, `*var = node` restore | house pattern on the config lookup; `IsASubclass` direct for X360, DC3 memo kept `HX_NATIVE` | +1 / +440 (part of run 3) | **60.54 → 100.000**, crossed in run 3 | none (memo is a cache) |
| `SongInfoCopy::SongInfoCopy()` (648 B, 44.25 %) | SongInfoCopy | **LOGIC** (missing init) | 0x827D1628 after the String inits: `li r10,1` + zero stores + `SystemConfig()->FindArray("beatmatcher")` reads; ours was `{ mName = gNullStr; }` | rb3-Wii body | +1 / +648 (part of run 3) | **44.25 → 100.000**, crossed in run 3 | **natively a default-constructed `SongInfoCopy` had uninitialised `mNumVocalParts`, `mHopoThreshold`, mute volumes** |
| `BandSongMgr::SongAudioData` (444 B, 58.81 %) | BandSongMgr | LOGIC (RB3-360-specific) | ours == rb3-Wii, but retail 360 adds a content-package path: `SongMetadata::?` (map says `WebSvcRequest::GetBaseURL`, a wrong name) → `Symbol` → `TheSongMgr` vcall 0x74 → `ContentNameRoot` → `FileMakePath` → `fn_8259EC18` → vcall 0x64 (`AddExtraMidiFile`) | none — needs `fn_8259EC18` and two vcalls identified (handoff) | — | — | 360 upgrade-MIDI path resolution differs; native drivers do not reach it |
| `Game::LoadSong` (408 B, 54.49 %) | Game | LOGIC (RB3-360-specific) | retail: `ContentName(sym,true)` → `Symbol` → `lbl_82C71A14` vcall 0x88 → validation 0/1/2; **no** `SetPracticeMode`. Ours (== rb3-Wii) calls `MasterAudio::SetPracticeMode(TheGameMode->InMode("practice"))` | none (handoff: identify `lbl_82C71A14`) | — | — | off the native drivers' path |
| `ObjectDir::PreLoad` (3,092 B, 84.71 %) | Dir | STRUCT / DC3-newer machinery | ours builds a `BinStreamRev` wrapper (`??0BinStream(bool)`, `??_7BinStreamRev`, `sObjectDirRev` global); retail keeps a packed 16-bit rev (`sth/lhz 0x4(r15)`, `cmplwi`) | none (own lane) | — | — | same semantics for RB3 revs |
| `PropSync(Matrix3&…)` (1,156 B, 32.07 %) | PropSync | CODEGEN | static-local `Symbol pitch/roll/yaw` guard word (`$S3`) re-read pattern + 3 vs 1 FPR saves | none | — | — | none |
| `VocalNoteList::NotesDone`, `Song::SyncState`, `ChunkStream::Eof`, `SongParser::StartVocalNote`, `Locale::Init`, `NetCacheMgr::AddLoaderRef`, `ObjectDir::Save` | — | CODEGEN | loop count-down vs index; `mFastSync` static (defaults false ⇒ same path); store scheduling with identical offsets; same four fields loaded in another order; register-dominated | none | — | — | none |

## 4. Measurement ledger (`ab_measure --from-dirty`, both legs settled)

| run | kinds | pre-registered | measured Δmatched / Δcode | notes |
|---|---|---|---|---|
| 1 DirLoader (`ff73e833`, was `9d8cbac3` pre-rebase) | source | +1 / +464 B | **+3 / +504 B** (Δmasked_equal +2, Δhonest +1, +0.004917 pp) | ctor crossed exactly; the extra +2/+40 B are funclet rows now pairing |
| 2 DataFunc + StringMatchesFilter + DataInt + 4 map names (`31c20826`, was `1ad056d5` pre-rebase) | source+map | +4 / +376 B | **+3 / +296 B** (Δhonest +3, +0.002893 pp; `none` control +560 B, NOT_APPLICABLE with source present) | the three DataFunc rows crossed (+376 B); `StringMatchesFilter` did not (99.89 on both keys); two 40 B placeholder funclets in DirLoader re-paired 100 → 99.5 fuzzy (mpn 100) after the TU gained a function, −80 B |
| 3 Iterate + SongInfoCopy (`df8ca4b6`) | source | +2 / +1,088 B | **+3 / +1,128 B** (Δmasked_equal +1, Δhonest +2, +0.011008 pp) | both principal rows exact (440 + 648); one 40 B funclet in Dir now pairs |
| **total** | | +7 / +1,928 B | **+9 / +1,928 B** | 42,505 → 42,514 matched; 37.42659 % → 37.445408 % |

## 5. What this lane did NOT do, and why

- Did not open the 413 tier-4 rows (rndobj/char/world/bandobj) nor tier 5:
  ranked below the loading/DTA/game path by the brief; they are 71 % of the
  stratum's bytes and remain the largest unaudited surface.
- Did not port `MemAlloc` (644 B): fully reconstructed in a prior handoff,
  native-inert, matching-only.
- Did not chase `DataInitFuncs`' 1.7 kB residue past two experiments: a
  `static` helper for the alias block was NOT inlined (68.8 %); a
  `__forceinline` helper DID reproduce the mechanism — distinct 8-byte
  `Symbol` temp slots for the seven alias registrations (ours 0xc0..0xf0 vs
  retail 0x134..0x164) — but at +0.02 pp, with retail's main frame still
  0x74 B larger and one more callee-save; reverted (speculative source for
  no measurable gain). The registry itself is now identical.
- Did not touch `ObjectDir::PreLoad`'s `BinStreamRev` machinery (3 kB): a
  lane of its own; it is the biggest tier-1 prize.
- Did not identify `fn_8259EC18` / the 0x64 and 0x74 vcalls in
  `SongAudioData`, nor `lbl_82C71A14` in `Game::LoadSong` — both are
  RB3-360-specific bodies where rb3-Wii is the WRONG oracle.
- Did not run a native driver before/after: no `.milo_xbox` fixture is
  checked in and the changed paths (nested DirLoader root, DTA builtins,
  SongInfoCopy default ctor) have no dedicated driver; the native gate is
  the only native check taken (section 7).
- Did not name `fn_82517718` (candidate `FileMakePathBuf`) — unverified.

## 6. Handoffs

1. `ObjectDir::PreLoad` / `BinStreamRev` vs retail's packed rev (STRUCT, 3 kB).
2. `BandSongMgr::SongAudioData` and `Game::LoadSong` retail-360 bodies
   (identify `fn_8259EC18`, `lbl_82C71A14`, the vcall slots).
3. `DataInitFuncs` alias-block frame: mechanism half-proven (inlined-callee
   temp slots), origin of the extra 0x74 B of frame unknown.
4. `DirLoader` unit: `LoadObjs`, `CreateObjects`, `LoadResources`, `LoadDir`,
   `~DirLoader` are all **unnamed 0 % rows** — the retail-order fixes landed
   here cannot score until a map lane names them.
5. `FileMakePath`: decide whether retail had a `FileMakePathBuf` at all
   (`fn_82517718`, 904 B) or a single body.
6. `Song::mFastSync` (DC3-newer static in `SyncState`): gate `HX_NATIVE` if a
   lane wants the row.
7. `String::contains` vs `FixedString::contains`: retail's `StringMatchesFilter`
   call site resolves to the base-class method; ours has a separate `String`
   override. Whether RB3's `String` had that override at all is a Str-class
   question (`utl/Str.h:145`), not a per-row one.

## 7. Native gate

Run 1 (after the three A/B-measured commits, rebased on main `22014600`):

```
NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=1 skipped=0 partial=0 failed=17 rc=1
```

`DataFunc.cpp:793: use of undeclared identifier 'DebugBeta'` — this lane's
`DataNotifyBeta` used `MILO_NOTIFY_BETA` inside its `HX_NATIVE` branch, and
the native `Debug.h` expands that to `DebugBeta() << …` with no `DebugBeta`
declared (the X360 branch is a no-op, so the match build never saw it).
Fixed in `75526f76` (native branch notifies through `TheDebug.Notify`, like
`DataNotify`; X360 object unchanged, rows re-checked at 100). Run 2:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

This document was committed after that run; it lives under `docs/` and is not
an input to the native link.

## 8. State at hand-off

Rebased onto main `22014600` (clean; main had only touched
`scripts/target_symbol_map.json` among this lane's files). Full X360 build
after the rebase: rc=0, `verify_objs_patched --verify-manifest` OK, all six
fixed rows re-read at 100 (ctor, DataMatchPattern, DataMatchAnyPattern,
DataInt, Iterate, SongInfoCopy()); whole tree 42,595 / 3,844,528 B /
37.522392 % (this is main's own movement plus this lane's +9 / +1,928 B — not
re-composed here against main's exact figure at `22014600`).
Scratch (population JSON, 127 offline side-by-sides, registry diff, A/B logs):
`~/tmp/w4a/`; gate logs `~/tmp/rb3_native_gate_w4a{,_2}.log`.
