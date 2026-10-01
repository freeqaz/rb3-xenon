# W16-NC — vtable-slot name audit, and the network classes in engine pins (2026-10-01)

**Branch** `w16-nc`, off main `5347dce70` (W16-MC landing). **Not merged to main**, per the brief.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).
**Leads** (1) W16-MA §7: audit virtual methods whose map-name class does not own the retail vtable slot
holding them (the shape that surfaced the `MakeInstImpl` swap), across *all* vtable-slot rows, and fix
names on retail-byte evidence after deciding base-obj pairability. (2) W16-MC §8 / W16-LA §9: the
network/game classes sitting in engine units' pins — `XboxSession`, `Net`, the session-job family
(including a `MakeSessionJob` whose layout differs from ours), and `BandPreloadPanel`.

Two forks did the source-heavy half of lead 2 in their own worktrees and were merged into this branch
with `--no-ff`: **F1** (session jobs) and **F2** (`Net`, `BandPreloadPanel`).

## 1. Whole-branch A/B

_(filled in §9 after F1 merged)_

## 2. Instrument — `tools/vtable_class_name_audit.py --slots`

W16-MA's tool asked "does the class in a ctor/dtor name agree with the vtable its body installs?". The
new `--slots` mode asks two independent questions of **every map row whose address is a retail vtable
slot** (7,732 rows at base, 7,735 at the tip):

- **rel** — is the name's class an `OWNER` of a vtable holding the address, a retail `BASE_OF_OWNER`
  (inherited, not overridden; MSVC's Base Class Array lists every ancestor), `UNRELATED` to every
  owner, or a `NAME_CLASS_NOT_IN_RETAIL` (no COL anywhere in the image)?
- **ours** — does **our** vtable of the same class, joined offset-to-offset through our own COLs
  (`vtable_order_sweep.our_vtable_by_offset`), hold the same name at the same slot?

`??_G` and `??_E` of one class compare equal: our vtables reference the vector deleting dtor where
retail's slot body is named for the scalar one (~580 rows, all at 100 — a convention, not a defect).

`--slots --selftest` runs six controls drawn from this lane's own fixes; each defect must disagree under
its old name and agree under its corrected one, and a sabotage class (`RndMesh` on `0x822c0b50`) must
read `UNRELATED`.

### 2.1 Census

| rel / ours | base `5347dce70` | tip |
|---|---:|---:|
| OWNER / OURS_SAME | 6,176 | 6,793 |
| BASE_OF_OWNER / OURS_SAME | 635 | 638 |
| OWNER / OURS_DIFF | 620 | 29 |
| OWNER / OURS_NONE (our vtable not readable) | 216 | 216 |
| UNRELATED | 36 | 29 |
| NAME_CLASS_NOT_IN_RETAIL | 38 | 25 |

(The OWNER/OURS_DIFF drop is mostly the `??_G`/`??_E` normalisation, which was added after the base
census; the rest is the fixes below.)

### 2.2 What a disagreement is — the triage rule

A disagreement is a **candidate, never a verdict**. Of ~115 candidates at base, most are ICF folds of
4–20-byte bodies (`lwz r3,N(r3); blr` getters, `blr`, `b` thunks) whose map spelling is one valid member
of the fold — e.g. `?GetProfile@Tour` sitting in an `AccomplishmentTrainerCategoryConditional` slot.
Those were left alone. A row was renamed only when **retail bytes** decided it:

1. **the branch target** of a thunk body (`addi r3,r3,N; b X` or `lwz r11,-4(r3); subf; [addi;] b X`);
2. **the class-name literal** a `ByteCode` body constructs (`String("SyncAllMsg")`);
3. **a body shape the old name contradicts** (a float-returning name on a body that returns a pointer
   in r3; a C COM `AddRef` on a body dispatching through vtable offset 0x28).

…and then **the address's direct callers were checked**, because the rename can be right for the slot and
wrong for the call sites (§4).

### 2.3 Three failed predictions

1. **"The OURS_DIFF bucket is the defect population."** 620 rows; ~580 were `??_G`-vs-`??_E` spellings
   of the same class. Normalised in the tool.
2. **"Naming the anonymous SongInfoCopy getters is free."** It took 12 caller rows off 100, and one more down from 99.86 (§4.1).
3. **"`Object::PreLoad` at `0x8269d940` reads BASE_OF_OWNER."** It reads OWNER — `Hmx::Object`'s own
   vtable holds it too. A wrong expectation in the selftest, not a tool defect; the control was replaced
   by `SongSort::NewShortcutNode` (`0x825bf308`, held only by its subclasses' vtables).

## 3. Lead 1 fixes (this branch's own commits)

### 3.1 Swapped `FxSend*360` overrides (8 rows, plus 6 new)

Every `FxSend*360` override of `Recreate`/`UpdateMix`/`OnParametersChanged` is `addi r3,r3,<FxSend360
offset>; b <target>`. The targets are decisive: `fn_82B69FC8` walks a `vector<FxSend*>&` with
`__RTDynamicCast` (= `FxSend360::Refresh`), `fn_82B693F0` converts volumes through `DbToRatio`
(= `UpdateVolumes`), `fn_82B69220` = `SyncEffectParams`. Slot 22 always reaches Refresh, 26 UpdateVolumes,
27 SyncEffectParams — our vtable order was right, **the map had the names swapped** in Delay
(`0x82b61d08/10`), EQ (`0x82b63188/90`), Wah (`0x82b644b8/c0`) and Flanger (`0x82b63988/90`). All read 100
before and after (the branch targets are forgiven placeholders), so this is an accuracy fix.

The same thunks name six more rows: `FxSendMeterEffect360` (+0x64, `0x82b63ec8/d0/d8`) and
`FxSendSynapse360` (+0x78, `0x82b5b1d0/d8/e0`, an ICF fold with `FxSendCompress360`'s). Two of those
addresses were on `_denylist` as `__destroy_aux<LevelData>` with the instruction "adjudicate on retail
bytes before reinstating either"; both are slot 27 of their classes' vtables and tail-call
SyncEffectParams, so they were lifted with that record appended. **Source:** `FxSendMeterEffect360`
declared the three overrides and nothing defined them — defined in `FxSendMeterEffect.cpp`;
`FxSendSynapse360` inherited the base no-ops — the overrides are now inline in its header (retail emits
them in Synth.cpp's code right after its `ClassName`). 6 rows 0/70 → 100.

### 3.2 `ByteCode` bodies (4 rows, re-homed)

| addr | map said | literal | re-homed |
|---|---|---|---|
| `0x823e0fa0` | `NetPopScreenMsg::ByteCode` | `SyncAllMsg` | BandUI → NetSession |
| `0x823e1320` | `NetPushScreenMsg::ByteCode` | `UserLeftMsg` | BandUI → NetSession |
| `0x825aaca0` | `BasicStartLockMsg::StaticByteCode` | `LockResponseMsg` | SetlistMergePanel → LockStepMgr |
| `0x825aade0` | `SetlistSubmissionMsg::ByteCode` | `EndLockMsg` | SetlistMergePanel → LockStepMgr |

All four stay at 100 (header-inline COMDATs; NetSession / LockStepMgr define the true names). The other
22 `StaticByteCode@X` rows in vtable slots carry their own class's literal and sit in X's slot 6 — valid
fold spellings, left alone.

### 3.3 Thunk / tail-call targets and contradicted shapes

| addr | old | new | evidence | before → after |
|---|---|---|---|---|
| `0x822d9db0` | `Copy@RndTransformable$4…@M@` | `Export@RndDir$4…@BGA@` | `addi -0x160; b Export@RndDir` | 98.5 → 100 (Waypoint → StreakMeter) |
| `0x826a8ab8` | `ClassName@MsgSource$4` | `ClassName@FadePanel$4` | `b ClassName@FadePanel` | 98.3 → 100 (Msg → FadePanel) |
| `0x826a8a88` | `ClassName@FadePanel` | same | pinned in Player | 0 → 100 (Player → FadePanel) |
| `0x823beea8` | `??3CharEyeDartRuleset` | `PollDeps@CharIKFoot` | `b PollDeps@CharIKHand`, CharIKFoot slot 4 | 95 → 100 (re-homed) |
| `0x826b47c8` | `__destroy_aux<GameGem>` | `IsLoaded@ChordbookPanel` | `b IsLoaded@UIPanel`, slot 12 | 95 → 100 |
| `0x8257b418` | `__ucopy_aux<BandProfile>` | `HasSyncPermission@QuickplayPerformerImpl` | `b IsLeaderLocal` | 95 → 100 |
| `0x8228d790` | `GetDistanceToPlane@RndDrawable` (float) | `StateName@VenueLoader` | returns a string pointer in r3 | 80 → 100 |
| `0x82b870e0` | `??__F sBloom` (atexit dtor) | `ReInit@NgRnd` | `lwz` (not `addi`) of a global; NgRnd slot 23 | 80 → 100 |
| `0x827297b0` | `CharAdvance@RndFontBase` | `GetSampleRate@MicNull` | `li r3,48000`; Mic slot 31 | 78.3 → 100 (OutfitConfig → MicNull) |
| `0x82701c88` | `ParseITN@NUISPEECH` | `EnableReads@StandardStream` | StandardStream slot 13 | 0 → 100 (+ source, §5) |
| `0x82754a70` | `_M_insert_overflow<Symbol>` | `Replace@DirLoader` | ObjRefOwner-subobject entry, 48 B | 10.7 → 100 (+ source) |
| `0x8251ff70` | `DataNode::Obj<CharPollable>` | `Location@XboxContent` | XboxContent slot 3 | 31.8 → 100 (+ source) |
| `0x8269d940` | `XShaderPDBBuilder_AddRef` | `PreLoad@Object@Hmx` | tail call through vtable +0x28 (= Load); slot 18 of 30 vtables | 0 → 100 |
| `0x827d10f0` | `GetIntegralImage@FaceDetector` (DC3) | `GetVols@SongInfoCopy` | SongInfoCopy slot | 0 → 100 (re-homed) |
| `0x82516b00` | `ReadDone@NullFile` | `WriteDone@File` | slot 14 of File/ArkFile/BufFile; no NullFile COL | 100 → 100 |
| `0x82b5ada8/b0` | `AddRef`/`QueryInterface` W-thunks | `QueryInterface`/`Release` | `b` to a GUID-compare / to `Release@CXAPOBase` | 100 → 100 |
| `0x825aad68` | anonymous | `??0EndLockMsg()` | stores EndLockMsg's vtable, constructs String at +4 | 0 → 100 (re-homed) |
| `0x825ab0f0` | `NewNetMessage@MainHubAdvanceMsg@?A0xfb94c5e0` | `NewNetMessage@EndLockMsg` | `new(0x14)` then the EndLockMsg ctor | 100 → 100 (OvershellPanel → LockStepMgr) |
| `0x822c0b50` | `Copy@CrazeHollaback` (DC3) | `Copy@BandSongPref` | BandSongPref slot 9 | 100 → 100 (§3.4) |
| `0x82b5ac08` | `PreLoad@Object` | `OnSetParameters@CSampleXAPOBase<CompressionEffect>` | tail call through +0x40, XAPO slot 15 | **99.75 → 0** (accuracy) |
| `0x8271a0b8` | `__ucopy_aux<FaderGroup>` | `SynthPoll@Sfx` | `b SynthPoll@Sequence`, Sfx slot 2 | **95 → 0** (accuracy) |

The last two fall from below 100 to 0: both were false pairings under names their bodies contradict, and
no unit near them defines the true name (`Sfx`'s units are not adjacent; our build compiles no
`CSampleXAPOBase::OnSetParameters` spelling). The `0x825ab0f0` defect was **exposed by naming the ctor**
(`0x825aad68`): the old row read 100 only because its callee was an anonymous placeholder.

`NewNetMessage`/`NewObject` rows were then checked as a class (117 whose callee ctor installs a vtable):
the only other mismatches were three false positives of the check itself (an inlined base ctor called
first) and `BasicStartLockMsg::NewNetMessage`, which our source also writes as constructing a
`StartLockMsg` (reads 100; consistent).

### 3.4 Two Dance Central units' whole pins were RB3 code

`CrazeHollaback` is a Dance Central class with no COL in the retail image. Its pin
(`0x822C0B50–0x822C0BD0` + a 4-byte tail) sat between BandSongPref's `ClassName` and `SetType`. Both
blocks return to BandSongPref and the empty heading is gone.

`HamIKSkeleton.cpp` (also DC3, no retail COL) had one block, `0x822C2220–0x822C24B8`, inside
BandIKEffector: `ObjPtr<BandIKEffector>` template members — `0x822C22C8` is slot 2 of retail vtable
`ObjPtr<BandIKEffector,ObjectDir>` (`Replace`, spelled `ObjPtr<HamCharacter>` in the map), `0x822C2348` its
dtor, `0x822C2438` its `??_G` (slot 0, no retail `bl` callers). Moved, heading deleted, `Replace`
respelled, `??_G` named: +2 fns / +192 B (dtor and `??_G` 0 → 100). The two `ObjOwnerPtr<CharWeightable>`
dtor-family rows in that block stay anonymous (W16-LA §9: a template fold with no proven alias).

A census of **all** units whose source is
under `hamobj/` or `gesture/` (DC3-only directories) finds **36 units / 374 rows / 35,988 B** of retail
code pinned to them, 18,208 B of it at 100 through shared templates/inlines — a re-home lead for a later
lane, not worked here.

### 3.5 Alias groups

| group | change | class |
|---|---|---|
| `0x822d9dd0` | survivor `Export@RndDir$4…BGA` withdrawn; `Replace@RndDir$4…BGA` (the map name) is survivor | `CONTRADICTED_THUNK_BRANCH_TARGET` — the Export thunk is its own body at `0x822d9db0` |
| `0x822c0b50` | `Copy@CrazeHollaback` withdrawn; `Copy@BandSongPref` survivor | `CONTRADICTED_NO_RETAIL_RTTI` |
| `0x82516b00` | `ReadDone@NullFile` withdrawn; `WriteDone@File` survivor | `CONTRADICTED_NO_RETAIL_RTTI` |
| `0x8269d940` | `XShaderPDBBuilder_AddRef` withdrawn; `PreLoad@Object@Hmx` survivor (`--chase`: CHASED T1 PROVEN, retail body twins 1), `GetTotalStars@Performer/Player` kept | `CONTRADICTED_VTABLE_SLOT_OWNER` |

No new membership was added in lead 1.

### 3.6 Class rename

The retail image carries `.?AVSongUpsellViewSetting@@` and no `MusicLibraryUpsellViewSetting`; the class
(W16-MA §4's open row) and its five map rows are renamed. All five stay at 100.

## 4. Tried and reverted

### 4.1 Naming 8-byte getter folds charges their callers

`0x827d10e8/f8/1100/1108` sit in SongInfoCopy's slots 11/14/15/16 and were anonymous. Naming them for
SongInfoCopy took **12 rows off 100** (and `AccomplishmentManager::IsAvailable` from 99.86; the wave-1 commit message's "13 off 100" miscounted it) — `PracticePanel::Poll` (1,656 B), `ProfileMgr::PushAllOptions`
(1,268 B), `EntityUploader::BuildProfileUploadOps`, `RockCentral::SyncSetlists`, … — every one a caller
that `bl`s the fold under another member's spelling. Unnamed, the placeholder target was forgiven.
Reverted; the block still moves to SongInfoCopy (they stay `fn_`).

### 4.2 `GemTrack::SetSmasherGlowing`

`0x82b93f58` is slot 92 of GemTrack (`lwz r3,0x90(r3); b GemManager::SetSmasherGlowing`), but its
GemPlayer callers spell the non-virtual `SetFretButtonPressed`, a byte-identical fold member. Renaming
took three GemPlayer rows off 100; reverted to the callers' spelling.

## 5. Source fixes from retail bytes

- `StandardStream::EnableReads` (`0x82701C88`, 20 B): no null test of `mRdr` in retail; guard is
  native-only.
- `DirLoader::Replace` (`0x82754A70`, 48 B): no test of `from`; clears `mProxyDir`/`mProxyName` and deletes
  itself (72.5 → 100).
- `XboxContent::Location` (`0x8251FF70`): retail keeps a branch (`li r3,0; cmplwi 2; beq; li r3,1`). The
  if/else-return compiled branchless (81.3); a switch read 62.3; a default-then-override local → 100.

## 6. Lead 2 — network/game classes in engine pins

### 6.1 F2: `Net` and `BandPreloadPanel` (written, re-homed)

_(from the fork report)_ `src/network/net/Net.cpp` is retail TU `0x823E02F0–0x823E08A0` (`??_G`,
`TerminateTheNet`, `QuazalMemAlloc/Free`, `Poll`, `SetGameData`, `Handle`, `Init`, `Terminate`, funclets,
TheNet's initializer `0x82C3EB80` and atexit `0x82C44C90`), taken out of Str's, CharBonesSamples' and
NetSession's pins; `Net.h` reshaped to retail's 0x28–0x3c layout. `BandPreloadPanel.cpp` is retail TU
`0x826047A0–0x826050A8`, out of StorePanel's and CharServoBone's pins, 27/27 rows at 100. Four wrong
thunk / deleting-dtor names fixed; two chased-PROVEN fold admissions (`??_GSyncStore`,
`BandPreloadPanel::OnMsg(LockStepStartMsg)`). Fork A/B: **+35 fns / +3,464 B**, 41 up (39 to 100), 0
down. Left: `Net::Init` 99.86 (installs the shared empty function `0x826C3888`; adjudicator UNDECIDABLE,
no alias).

### 6.2 F1: session jobs

_(filled in after F1 merges)_

### 6.3 The Xbox platform layer has no source anywhere — identified, not ported

Retail RTTI places a whole Xbox network platform layer at `0x823EC980–0x823F0A70`: `XboxServer`'s tail
(our `Server.cpp` has the head at 100), `XSessionSearcher`, `XboxSession`, `XSessionData`/`SessionData`.
None of the three has source in this tree, DC3 or rb3-Wii. Inventory at the tip:

| unit holding the code | rows | bytes | at 100 |
|---|---:|---:|---:|
| `auto_03_823ED458` (XboxServer[1,2,4–6,8], XSessionSearcher, XboxSession[2,4,5–9,12,14,16], XSessionData[1,4]) | 74 | 8,056 | 0 |
| CheatProvider (XboxSession[1,6,11,13,15], ctor `0x823EFD28`, dtor `0x823EFFA8`) | 29 | 3,268 | 264 |
| CharIKSliderMidi (XboxSession[0,10,17]) | 16 | 1,824 | 116 |
| Server (XboxServer tail) | 15 | 1,584 | 692 |
| `auto_03_823ECD58` (`ServerStatusChangedMsg`/`UserLoginMsg` ctors, XboxServer[3]) | 13 | 1,136 | 0 |
| ContextChecker (XSessionData[2,3]) | 4 | 336 | 0 |

The engine pins were **not** un-pinned: the at-100 rows in them are EH funclets pairing by byte
signature, and moving them to an `auto_` unit would take them off 100 with no source to pair against.
The Quazal `MessageBroker` DDL (`_DOC_MessageBroker`, `_DO_MessageBroker`, `DOClassTemplate<_DO_RootDO>`,
`MessageBroker`, `HarmonixGameDDLDeclarations`) is likewise spread over UI, CheatProvider, TrackPanelDir,
WavMgr, ContextChecker and Matchmaker pins at `0x823F3E30–0x823F5700` (~6 KB).

## 7. Left open

- The 85 `--slots` candidates at the tip: StaticByteCode/ByteCode folds (22), leaf getter folds,
  `EnterFlowMsg`/`JoinEntryPointEvent` (retail has them in an anonymous namespace; ours are not),
  `ObjPtr<HamCharacter>::Replace` at `0x822c22c8` (a DC3 class; owner `ObjPtr<BandIKEffector>`),
  `??_GAutomator` = `MessageBroker` (not emitted), the CXAPO `AddRef` W-thunk (unlocated).
- DC3-only units holding RB3 code (§3.4).
- The Xbox platform layer and MessageBroker DDL (§6.3).

## 8. Gates

_(filled in after the final build)_

## 9. A/B detail

_(filled in after the final run)_
