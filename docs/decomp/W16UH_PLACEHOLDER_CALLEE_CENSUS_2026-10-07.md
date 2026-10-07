# W16-UH: call sites whose retail callee is unnamed, checked on retail bytes (2026-10-07)

> **STATUS (2026-10-07):** lane record. Branch `w16-uh`, not merged. Two wrong
> callees fixed in source; every other non-proven site has a verdict below.

## Why

W16-UG found `SystemInit` calling `GlitchFinder::Init` where retail calls the
Stage Kit init at `0x82522608`, in a row scoring 100. The shipped ruler
(`functionRelocDiffs=name_check`) cannot charge a relocation whose retail target
is a placeholder (`fn_8XXXXXXX`), so whatever we call at such a site scores the
same. This lane asks the question for every such site: is our callee the
function at that retail address?

## Method

`tools/placeholder_callee_census.py` (new).

1. **Population.** Every `report.json` row whose name is defined in both the dtk
   target obj and our obj of its unit (a paired row). Rings come from
   `scripts/native_scope_map.classify`, split the way W16-OV's
   `scope_ledger2.tier` splits it. OUT-QUAZAL and OUT-XDK are skipped. IN-CORE,
   IN-SOON, IN-RB3ENG and VIA-DC3 are the requested scope. OUT-NET and
   OUT-360-OTHER were walked as well and are reported separately.
2. **Alignment.** Call relocations (type 0x06) are aligned slot by slot: STRICT
   (same size and same (offset, type) sequence) or BL (same number of call
   relocations, by index), as in `tools/wrong_callee_census.py`. Rows that align
   neither way are counted, along with the placeholder calls inside them.
3. **Sites.** An aligned slot whose retail name is `fn_<A>` and whose name on
   our side is a real symbol F.
4. **Verdict per distinct (A, F)**:
   `icf_pair_adjudicate.chase(fn_<A>, F)` (masked bytes equal, relocations
   agree recursively) gives PROVEN. Otherwise the verdict is MAPPED-ELSEWHERE
   (F is in the map at another address), NO-OURS (no compiled body for F),
   BODY-ELSEWHERE (F's body is a different retail function), BYTES-EQUAL (depth-0
   bytes and relocation shape equal, chase stopped deeper) or REFUTED. Everything
   that is not PROVEN was read by hand, side by side (retail body from the target
   obj, ours from our obj).

Measured on the worktree at `aa61db0aa` after a full build (54,947 fns /
59.314644%, the same as W16-UG's leg).

## Controls (`--controls`)

| control | what it shows | result |
|---|---|---|
| K1 known answer | the walk finds SystemInit's site at `0x82522608`, and W16-UG's wrong callee `GlitchFinder::Init` against it must not read PROVEN | site found (now naming `StageKitInit`); **REFUTED**, retail 480 B vs ours 156 B |
| K2 positive | aligned slots with a named retail callee N equal to ours, N's row at fuzzy 100, 1,500 sampled: (N, N) must read PROVEN | **1,438 / 1,500 PROVEN (95.9%)** |
| K3 negative | the same 1,500 retail callees, each paired with the nearest-size other callee | **0 / 1,500 PROVEN** |
| K3b (measurement) | depth-0 byte equality alone on the K3 pairs | **38 / 1,500 (2.5%)**: every `ClassName` body is identical except the static it loads |

The check can pass (K2), can fail (K3), and fails on the known wrong callee
(K1). K3b is why BYTES-EQUAL is reported as undecided and never counted as a
right callee.

The 62 K2 misses are the chase's strictness, not false refutations: 38 stop on a
callee two levels down that it cannot discharge, 5 on a non-address placeholder,
3 on a vtable whose class is renamed in retail, 3 on a slot refuted below, 2
CD-9 mapped-vs-placeholder, 10 have byte differences (e.g. a `vector::resize`),
and 1 is vacuous. So about 4% of REFUTED/BYTES-EQUAL verdicts are expected to
be right callees, which is why every non-PROVEN pair was read by hand.

## Census

| ring | paired rows | STRICT | BL | unaligned | rows with sites | of them at fuzzy 100 | sites | PROVEN | NO-OURS | other |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 2,502 | 2,440 | 26 | 36 | 84 | 81 | 100 | 36 | 53 | 11 |
| IN-SOON | 10,997 | 10,853 | 99 | 45 | 316 | 305 | 398 | 250 | 73 | 75 |
| IN-RB3ENG | 4,169 | 4,083 | 70 | 16 | 100 | 99 | 140 | 122 | 10 | 8 |
| VIA-DC3 | 9,025 | 8,822 | 165 | 38 | 291 | 274 | 386 | 206 | 114 | 66 |
| **requested scope** | **26,693** | 26,198 | 360 | 135 | **791** | **759** | **1,024** | **614** | **250** | **160** |
| OUT-NET | 468 | 464 | 4 | 0 | 61 | 58 | 123 | 7 | 113 | 3 |
| OUT-360-OTHER | 1,825 | 1,762 | 48 | 15 | 251 | 221 | 478 | 12 | 451 | 15 |
| UNKNOWN-other | 19 | 19 | 0 | 0 | 3 | 2 | 7 | 1 | 5 | 1 |

Over all walked rings there are 1,632 sites in 647 distinct (A, F) pairs:
**236 PROVEN, 354 NO-OURS, 49 REFUTED, 6 MAPPED-ELSEWHERE, 1 BYTES-EQUAL,
1 BODY-ELSEWHERE**. 759 of the 791 in-scope rows with such a site score fuzzy
100, so this is the population the ruler cannot see.

**Blind spot.** 150 paired rows align neither way, and three of them hold
placeholder calls (17 slots). They were read by hand (§Unaligned rows).

## Verdicts

### Wrong callees, fixed in source

| site(s) | retail calls | we called | fix | A/B |
|---|---|---|---|---|
| 10 sites: `Profile::Handle` (VIA-DC3); `LocalSavedSetlist::GetOwner`, `BandProfile::GetBandName`, `SaveLoadManager::GetDialogMsg`, `Tour::Handle`, `BandUser::ProfileName`, `BandMachineMgr::RefreshPrimaryProfileInfo`, `AppLabel::SetIconAndProfileName`, `CharProvider::Text`, `LocationCmp::LocationCmp` (IN-SOON) | `0x827A5018`: `GetLocalUserFromPadNum(mPadNum)->UserName()`, a vcall through the User virtual base | `Profile::GetName`, whose body returned the `LocalUser*` cast to `const char*` | `GetName` returns the user's name; map `0x827A5018` → `GetName`, `0x827A4F38` → `GetLocalUser`; the W16-NH-NAME alias group at `0x827A4F38` respelled to `GetLocalUser`, membership withdrawn with a record | **+1 fn / +76 B** (predicted +76 B), `default/Profile` reaches 100% |
| `NgPostProc::DoVelocity` (VIA-DC3) | `0x823B8618`: `ObjPtrList<T>::clear()`, folded over T | `merged_ObjPtrListPopBack((u8*)this + 0x21C)`, an extern that no source defines | `if (mMotionBlurDrawList.size() != 0) mMotionBlurDrawList.clear();` (`0x21C` is the `ObjPtrList<RndDrawable>`, `0x220` its size). Our `clear<RndDrawable>` is **PROVEN** equal to `0x823B8618` | **Δ0 on every measure** (predicted Δ0: the slot is a forgiven placeholder); leg B recompiled 3 TUs |

How `GetName` went wrong: the 07-18 byte-identity sweep (`469ea464d`) named
`0x827A4F38` `GetName`. `f3ec9592d` (wave4) then rewrote `GetName` to match that
address. W16-NH-NAME's alias then "proved" `GetLocalUser == GetName` against the
rewritten body, so the proof was circular. Every caller of `GetName`, which
includes the `get_name` script handler, got a `LocalUser*` back as a string,
natively as well.

### Map defects (no source change can fix them; recorded)

| row / address | evidence | correct identity |
|---|---|---|
| `0x823CFAD0`, mapped `?SyncProperty@CharDriver@@$4PPPPPPPM@A@...` (vtordisp thunk) | the thunk branches to `fn_823CEDE8`, which chase-PROVES equal to our `CharSleeve::SetName` (the only one of 8 `SetName` overrides casting to `Character` that does) | `CharSleeve::SetName`'s vtordisp{-4,0} thunk. The `CharDriver` span covers `CharSleeve` code. |
| `0x824EB260`, mapped `?PreSave@WorldInstance@@$4PPPPPPPM@A@...` | the thunk branches to `fn_824EA298` = `this -= 0x14; b RndDir::Replace`. W15-D already withheld this spelling because the map gives it a second address. | a `Replace` vtordisp thunk in WorldInstance's vtable; needs vtable geometry |
| `0x825AEE78`, mapped `??$__ucopy_ptrs<_Slist_node_base**>` | a 4-byte `b fn_8260EED8`; `fn_8260EED8` is an slist clear (frees nodes in a loop, zeroes the head), called as such from 5 other sites | a vacuous tail-call thunk named by shape |
| `__u64tod` mapped at `0x8282EF50` | `0x8282EF50` is `lfd f1,-8(r1)`, the second-to-last instruction of the routine at `0x8282EF30` (`cntlzd ... blr`), which `StandardStream::GetRawTime` calls | `__u64tod` is `0x8282EF30` |
| `?clear@?$ObjPtrList@VEventTrigger@@...` (row) | retail calls `fn_8271A138`, an `Unlink` that releases the node's object with no virtual-base step; our `Unlink<EventTrigger>` is byte-equal to retail's named `Unlink<RndMesh>`, which has the step. `EventTrigger`'s `Object` base is virtual. | the row is a fold member for a T whose Object base is not virtual |
| `??1?$_Rb_tree<TrackType,...>` (row) | calls `fn_826DA438`, a `clear` whose `_M_erase` is a different function from the one `clear<TrackType,int>` reaches at `0x827690D0` (`~PerfectSectionTracker` calls that one) | a fold member for another tree type |

### Right callee, our body differs from retail's

Our callee is the function retail calls. Its body diverges, so it will not pair
when the address is named. These are recorded, not fixed (bodies, not callees):

| retail | our callee | difference |
|---|---|---|
| `0x822E36B8` (16 B) | `GemTrackDir::UpdateFingerFeedback` | we test `mFingerShape` for null; retail does not |
| `0x8230C628` (220 B), 16 spellings, 55 sites | `ObjPtrList<T>::operator=` | retail assigns through `Set(iterator, obj)`; ours is 264 B with the link logic inlined |
| `0x823399C0` | `BandCharDesc::NewObject` | retail calls `StaticClassName` and `MemAlloc(0x260, 0)`; ours calls `BandCharDesc::operator new` |
| `0x8236D3F8` (12 B) | `Character::Exit` | retail: `mState = 4; RndDir::Exit()` as a tail call. Ours wraps it in an `sCurrent` save/restore (60 B). |
| `0x82373F80` | `ObjVector<Character::Lod>::resize` | same calls, different frame (ours uses `__savegprlr_29`) |
| `0x823924F8` (212 B) | `FileMerger::AppendLoader` | ours is 528 B |
| `0x8246B740` (44 B) | `RndShaderMgr::InitShaders` | retail: clear `0x6C`, `RndShader::Init`, `RndShaderProgram::InitModTime`. Ours adds the `cache_shaders` config read. |
| `0x824CB940` | `WorldDir::PresetOverride::PresetOverride` | retail inlines the `ObjPtr<LightPreset>` ctor |
| `0x8251A018` | `Queue::~Queue` | same calls; our `CritSecTracker` is spilled to the stack. `RndCubeTex::operator delete` and `MidiMessage::operator delete[]` are both `b MemFree`, so the delete slot is a fold. |
| `0x825219A0` (396 B) | `FileEnumerate` | ours adds the `UsingCD` / Holmes branches (552 B) |
| `0x8252A8A0` (4 B) | `Memcard::Terminate`, `VirtualKeyboard::Terminate` | retail is `b <an empty function>`; ours are `blr`. One retail address serves both: a fold. |
| `0x82697FE8` | `__unguarded_partition<pair<int,float>, PartPercentageSorter>` | the comparator is compiled through a bool temporary in retail |
| `0x826C8470` (2,704 B) | `GemPlayer::GemPlayer` | ours 2,708 B, small differences |
| `0x827B36E0` (8 B) | `PreloadPanel::SetTypeDef` (2 thunk sites) | retail is a bare `UIPanel::SetTypeDef` tail call; ours adds the `max_cache_size` read and four `CheckTypeDef` calls |
| `0x82B81F40` (8 B) | `JsonObject::Str` | retail `return json_object_get_string(mObj)`; ours adds a null test and `json_object_get_type`. `GetObjectAsString` reaches the same address. |
| `0x823EA3E0` (OUT-NET) | `SyncStore::AddSyncObj` | retail only `push_back`s; ours searches for a duplicate tag first |
| `0x823F2F08` (OUT-NET) | `QuazalSession::QuazalSession` | retail is the full ctor; ours is a stub |
| `0x82524838` (OUT-360-OTHER) | `JoypadTerminateCommon` | ours also releases `gKeyboardExporter` |
| `0x82725298` (OUT-360-OTHER) | `opaquePredicate` | address formation differs |
| `0x82B699E8` (OUT-360-OTHER) | `FxSend360::~FxSend360` | retail 372 B, ours 240 B |

### Right callee, bodies equal (chase stopped on a deeper or vacuous slot)

`0x825150C8` (5 `GetUser*` spellings, 57 sites: one folded `GetObj` +
`dynamic_cast<LocalUser>`), `0x82519FF8` `UsbMidiGuitar::E3CheatGetMinVelocity`,
`0x823199C8` `~MiniLeaderboardDisplay` (retail vtables are placeholders),
`0x826758E8` `MetaPerformer::GetVenue`, `0x827A28E0` `FixedSizeSaveable::Init`,
`0x826DA438` `_Rb_tree<TrackType,...>::clear` (BYTES-EQUAL; only `_M_erase` is
a placeholder), `0x826F0F18` `~RGTutor`, `0x826F8F68` `Singer::Rollback` (the
nested `vector::erase` names differ by fold spelling), `0x823EA8A0`
`InstanceTable::GetInstanceFromVector` (Quazal placeholders only).

### Callee outside the scope

`0x82A8AF30` against `Quazal::Session::JoinSession` (site
`MakeSessionJob::IsFinished`, BL-aligned at fuzzy 91.7, so the slots may be
misaligned) and `0x82BC3940` against `CreateXAudio2Object` (XDK): not
adjudicated.

### NO-OURS: 354 pairs, no compiled body to compare

Bytes cannot decide these. By callee kind: **244 C-API** (XDK, kernel, CRT,
D3D: `GetCurrentThreadId`, `D3DDevice_*`, `XUser*`, ...), **44 Quazal**, **66
mangled Harmonix names**. Two screens:

- **Dispersion.** A callee of ours that stands for two retail addresses across
  all aligned sites (named or placeholder) is wrong at one of them. The screen
  fires on 5 pairs: `Unlink<EventTrigger>`, `_Rb_tree<TrackType>::clear`,
  `CharDriver::SyncProperty`, `__ucopy_trivial` (the map defects above) and one
  PROVEN pair. It fires on **0** NO-OURS pairs. It can fire, since it found the
  four map defects independently.
- **Name review of the 66 mangled.** One is fabricated:
  `merged_ObjPtrListPopBack` (fixed above). The rest name the function retail
  plausibly calls and are not compiled in the match build. They are: the 8
  Stage Kit functions W16-UG ported (`StageKit.cpp` is not in `objects.json`);
  12 vector-deleting destructors `??_E*`; `AuditionSessionBuilder` ×8;
  `BandNetGameData` ×2; `AssetOffer` ×2; `PlayerCampaign*Leaderboard` ×3;
  `OggValidator` ×3; `FxSend360` ×4; `NetMessenger` ×4; `QuazalSession` ×3;
  `Hmx::Object`'s copy ctor; `UnhookAllParents` (retail `0x82BB1588` matches our
  `GraphicsUtl.cpp` on the disassembly: group-unhook loop at `0x82BB1540`, then
  `dynamic_cast<RndTransformable>` and `SetTransParent(NULL, false)`); and
  single CRT/XDK-adjacent helpers.

### Unaligned rows

| row (ring) | reading |
|---|---|
| `ThreadMemStack` (IN-CORE, fuzzy 88.2) | 5 × `fn_8283D580` ↔ 5 × `GetCurrentThreadId`, the address 43 aligned sites give; ours has one extra `CriticalSection::Exit` |
| `DxRnd::FinishPostProcess` (OUT-360-OTHER) | placeholders pair in sequence with `D3DDevice_SetSamplerState_*`, `SetRenderTarget_External`, `SetDepthStencilSurface`, `Clear`, at the addresses their aligned sites give |
| `DxMesh::DrawShowing` (OUT-360-OTHER) | retail calls `fn_82418DC0` between `SetTransforms` and `DrawFur`; ours has no call there: a missing call, not a wrong one (out of scope, recorded) |

### Seen outside the census

`PlatformMgr_Xbox.cpp` declares two more fabricated externs:
`merged_82610090` (called in `DtaToJsonHelper` with a format string, most likely
a `MakeString` instantiation) and `merged_DataArrayNode` (declared, unused). The
calling row is OUT-360-OTHER and not in the aligned population, so it was left
alone.

## After the fixes

Re-run on the final tree (`b070a9c6d`, 54,948 fns / 6,078,548 B):

| | before | after |
|---|---:|---:|
| distinct (A, F) pairs | 647 | 645 |
| PROVEN | 236 | 236 (DoVelocity's site joins the existing `(0x823B8618, clear<RndDrawable>)` pair, with 22 other callers) |
| MAPPED-ELSEWHERE | 6 | 5 (the `GetName` pair is gone: its 10 sites are now named on both sides and agree) |
| NO-OURS | 354 | 353 (`merged_ObjPtrListPopBack` is gone) |
| IN-SOON / VIA-DC3 sites | 398 / 386 | 389 / 385 |
| controls | PASS | PASS (K1 REFUTED; K2 1,444/1,500; K3 0/1,500; K3b 41/1,500) |

A/B, both from `tools/ab_measure.py --from-dirty` in `~/tmp/wt-w16uh`:

```
Profile (kinds map+source)
  leg A: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 0, settled)
  leg B: matched=54948 masked=25223 honest=29725 code%=59.315384  (recompiles: 1, split=1, patch_steps=7, settle iterations: 2)
  Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000740pp  Δcode_bytes=+76
  units at 100% [mpn ruler]: legA 607 -> legB 608  (+100% default/Profile, MATCHED_ROSE)

NgPostProc::DoVelocity (kind source)
  leg A: matched=54948 masked=25223 honest=29725 code%=59.315384  (recompiles: 0, settled)
  leg B: matched=54948 masked=25223 honest=29725 code%=59.315384  (recompiles: 3, split=0, patch_steps=6, settle iterations: 2)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
```

## Gates

`tools/native_build_gate.sh` in `~/tmp/wt-w16uh` at `b070a9c6d` (both `src/`
fixes in):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`DoVelocity`'s fix is in the `#ifndef HX_NATIVE` arm, so it does not reach
native. `Profile::GetName` does, and the native `get_name` handler now returns
the user's name instead of a `LocalUser*`.

## Not done

- No map edits beyond the two `Profile` addresses. The thunk and fold-pick rows
  above need vtable geometry or a span repair (`CharSleeve` code inside the
  `CharDriver` span), and renaming them would unpair rows.
- No body ports for "right callee, body differs". Those rows' retail addresses
  are unnamed, so a port pays only once the address is named and pinned.
- No naming of PROVEN placeholder addresses (236 pairs). Naming is a bet that
  pays in bug exposure (MAPID-1), and it was not this lane's question.
