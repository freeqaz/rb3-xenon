# W16-QI: the static-destructor (`??__F`) region at 0x82C43EB0+ (2026-10-06)

Lane W16-QI. Brief: attribute the `atexit` destructor stubs that retail links
after the initializer region to their owning TUs, the way W16-QH did for the
`??__E` rows (`W16QH_STATIC_INIT_REGION_2026-10-06.md`). Skip Quazal and XDK.
Worktree `~/tmp/wt-w16qi`, branch `w16-qi`, based on main `bd0b1c74c`.

## 1. Result

| | rows | B |
|---|---:|---:|
| stubs re-homed into their owner and named, byte-equal | **807** | 21,664 |
| stubs re-homed and named, dtk carve wrong (§4.2) | 6 | 120 |
| already pinned in a named unit; derived owner agrees (control) | 4 (+4 Quazal) | 172 |
| anonymous-initializer pairs (initializer + stub), re-homed and named | **65 pairs / 130 rows** | 4,848 |
| `gTransListAlloc` pair (Console) | 2 | 76 |
| `TokenRedemptionPanel::Poll` Message stubs | 5 | 140 |
| in scope, left unattributed, each explained in §5 | 25 (+4 carve fragments that ride along) | 480 |
| Quazal (not funded) | 375 | 8,892 |
| XDK `LEAPFX` initializers at the region head (not stubs) | 9 | 1,228 |

**Measured** with `tools/ab_measure.py` (`name_check` ruler, both legs at a
split fixed point, 0 recompiles), in two runs whose deltas compose:

| run | change | Δ`matched_functions` | Δ`matched_code` | Δcode% |
|---|---|---:|---:|---:|
| `--patch` of `bd0b1c74c..8359b9f02` (first three commits), run `20261006-130544-w16qi-lane-3309483` | 53,602 → 54,517 | **+915** | **+25,100 B** | +0.244926 pp |
| `--revert a7349830b` (fourth commit; leg A has it, leg B removes it), run `20261006-131055-w16qi-tokenpanel-3380651` | 54,522 → 54,517 | −5 ⇒ **+5** | −140 ⇒ **+140 B** | +0.001374 pp |
| **lane** | | **+920** | **+25,240 B** | +0.246300 pp |

`total_functions` and `total_code` do not move (68,909 / 10,247,792):
the lane reattributes rows; it adds and removes none. `masked_equal` falls by
23 because rows that paired by byte signature now pair by name.

Predictions, stated before each A/B from same-worktree builds: +805 / +21,684 B
(commit 1), +109 / +3,404 B (commit 2), +1 / +12 B (commit 3), +5 / +140 B
(commit 4). The A/B legs reproduced all four exactly (+915 / +25,100 for the
first three combined).

**Rows that went down: seven**, all byte-signature pairings that lost the
partner they had been borrowing (row diff of the two archived leg reports,
every `(unit, symbol)` on both sides):

| row | B | before (fuzzy / `mpn`) | what it was |
|---|---:|---|---|
| `UsbMidiGuitar fn_82C3FA20` | 12 | 98.3 / 100 | StageKit `Timer` initializer (no source, W16-QH §3.1), paired with UsbMidiGuitar's `??__FgCritSection`/`??__FgQueue` |
| `Voice fn_82C429E8`, `…42B10`, `…42B90`, `…42D68` | 4 × 52 | 99.2–99.6 / 100 | Quazal `CriticalSection` initializers in the Voice span, paired with Voice's own `gLockPendingLists`/`gVoiceGC` |
| `FileCache fn_82519FF8` | 12 | 60 / 60 | a 12 B `lwz` row, paired with FileCache's `??__FgCaches` |
| `FileMergerOrganizer fn_823DA748` | 12 | 78.3 / 80 | paired with `??__FgCatPriority` |

Five of these had `mpn` 100, so about five of the +920 offset false matches
that are now gone. Every other row change is re-homing: 1,364 rows left
their old (mostly `auto_*`) unit and the same 1,364 appeared in their owner.

Four units fell off 100% (`mpn`): CampaignSongInfoPanel and MainHubPanel gained
the carve-limited rows (§4.2), and Task and Timer each gained one of the three
fold-named stubs that read 98.3 (§4.1). These are denominator growth from
rows that belong to those units, not rows scoring lower.

The native gate was not run: the lane touches only `splits.txt` and
`target_symbol_map.json`, no source.

## 2. Correction to the brief

The brief said these stubs are "mostly scored against whatever unit's span
happens to cover them". Measured: of the 1,282 retail functions from
`0x82C44000` to the BINK section (`0x82C4D000`), **8** sat in a named unit's
span (all already at 100: Net, HiResScreen, ProfileMgr, MemcardMgr and four
Quazal). The other 1,274 were in `auto_*` units (`auto_03_82C438BC`,
`…82C45014`, `…82C45EC4`, `…82C49E1C`, `…82C4A920`, `…82C4AF18`, …):
unattributed, in the denominator, and unpairable at 0%. So the lever was the
same as W16-QH's (a pin plus a name), but the rows came out of `auto_*`, not
out of a covering unit. That is also why almost nothing went down.

## 3. Method

### 3.1 The stubs that have a named referencer (813 rows)

Every stub in the region has **at most one** referencer in retail (an
`addi fn_82C4…@l` that hands it to `atexit`); 1,020 of 1,305 have one, and none
has two:

- 242 are registered by an initializer in the `??__E` region
  (`0x82C3E000`–`0x82C44000`), i.e. a file-scope or class static;
- 778 are registered from inside a TU's own code, i.e. a function-local static
  (`static Message msg(...)` is most of them: the 28 B shape that stores
  `Message`'s vtable and tail-calls `DataArray::Release`).

When the referencer is named in the map, the owner is **the unit that pins the
referencer**, and the stub's name comes from **that unit's own base obj**:

- `??__E` referencer: the owning obj's initializer names its stub in a
  relocation.
- Function-local referencer: when a function owns several statics, retail's
  `addi` order inside the function is paired with our obj's relocation order
  inside the same function. Our objs carry stray copies of some of these
  symbols (BandDirector's `Enter`/`EnterVenue` locals are also defined in
  Stats.obj, Font.obj, Rot.obj and a dozen others), so a bare name search is
  not enough. Restricting to the referencer's own unit is what makes the
  result unique.

**Verification.** 821 rows resolved. For each, retail bytes were compared with
our compiled `??__F` body, masking only the fields our obj relocates, and the
branch target's map name was compared with our relocation target:

| | rows |
|---|---:|
| bytes equal, callee name equal | 795 |
| bytes equal, retail callee anonymous | 4 |
| bytes equal, callee name differs (ICF fold, §4.1) | 16 |
| dtk carve differs from the true 28 B body (§4.2) | 6 |
| **total** (4 of them Quazal, already pinned, not edited) | **821** |

None of the proposed names was already in the map at another address, and none
repeats. 55 addresses already carried exactly the derived name (named but never
pinned). **Control:** the 8 stubs that were already pinned in named units
(Net `TheNet`, HiResScreen, ProfileMgr, MemcardMgr, ProductFacade,
ObjectThreadRoot ×2, DOCore) all resolve to the unit they were already in.

Link order makes each TU's stubs contiguous, so 813 rows merged into **202
`.text` blocks across 200 headings**. Each block runs `[first stub, next
function)` so the inter-stub padding travels with it; no block end borders an
8-byte EH prefix. dtk derived 4 `.pdata` lines (the 60 B stubs with frames).

### 3.2 Stubs whose initializer is itself anonymous (65 pairs)

These sit in `auto_*` stretches of the init region that W16-QH did not reach
(`0x82C3E5B4`, `0x82C3EBF0`, `0x82C3F138`, `0x82C3F5D0`, `0x82C40F7C`,
`0x82C438BC`), or in DataNode/PropKeys/Voice spans. The stub's owner is the
initializer's owner, so both rows of each pair were attributed and pinned
together. Two rules, each requiring retail-byte equality of **both** bodies
(relocations masked):

1. **Signature + link order + readers.** The initializer's callees, vtables
   and strings plus its stub's callees must match one of our `??__E`/`??__F`
   pairs of equal size, and exactly one candidate obj must be both (a) in
   link order between the owners of the neighbouring attributed initializers
   and (b) a reader of the initialized global from its main `.text`. Example:
   `sFacingPos`/`sFacingRotAndPos` have 32 candidate objs; only CharClip is
   in order **and** reads them. For type-unique singletons, whose readers are
   spread game-wide, a single candidate obj in the whole tree, in order, was
   accepted (QuestManager, NetLog, XboxServer, NetMessageFactory,
   XboxEntityUploader, TheDebug, `FilePath::sRoot`).
2. **Reader obj + bytes.** When an ICF fold hides the stub's callee name
   (retail spells `_List_base<SynthPollable*>::clear` where ours spells
   `_List_base<RndOverlay*>::clear`), the reading unit's obj supplies the
   candidates of equal size, and both bodies must be byte-equal.

Within one obj, several same-shaped statics are assigned in declaration order
(the order MSVC emits initializers). Two were decided by hand:

- `0x82C40A30` → DataFunc `gDataFuncs`: DataNode's only 132 B initializer was
  already taken by `0x82C40750`, and DataFunc reads the global six times to
  DataNode's once.
- `0x82C3F4F8` → `world/Dir.cpp` `gOldChars`: both PropSync.obj and Dir.obj
  define it and both lie in the link-order bracket; every surviving source
  defines `std::vector<FilePath> gOldChars` in `world/Dir.cpp`, so PropSync's
  is a stray copy.

| owner | initializers (stub rows travel with them) |
|---|---|
| system/bandobj/BandHeadShaper.cpp | `82C3E7F0` gHeadMaleMapping; `82C3E800` gHeadFemaleMapping |
| BandCamShot.cpp | `82C3E978` `BandCamShot::sCache` |
| CharClip.cpp | `82C3EA68` `FacingSet::sFacingPos`; `82C3EAB0` `sFacingRotAndPos` |
| Mesh.cpp | `82C3EEA0` gPatchVerts |
| system/rndobj/Utl.cpp | `82C3F138` gChildPolys; `82C3F190` gParentPolys |
| Console.cpp | `82C3F2F8` `RndMultiMesh::sProxyPool` (§3.3) |
| Shader.cpp | `82C3F400`…`82C3F4A0`: gShaderSimple, Particles, Multimesh, Standard, PostProc, DrawRect, UnwrapUV, Velocity, VelocityCamera, DepthVolume |
| LightPreset.cpp | `82C3F4B0` `LightPreset::sManualEvents` |
| system/world/Dir.cpp | `82C3F4F8` gOldChars |
| SpotlightDrawer.cpp | `82C3F508` sLights; `82C3F518` sCans; `82C3F528` sShadowSpots |
| FileChecksum.cpp | `82C3F538` gChecksumData |
| Debug.cpp | `82C3F690` gNotifyThreadSync; `82C3F6C8` gNotifyThreadSec; `82C3F700` TheDebug; `82C3F738` gNotifies |
| System.cpp | `82C3F770` TheSystemArgs |
| Timer.cpp | `82C3F780` `AutoTimer::sTimers` |
| QuestManager.cpp | `82C3E9F8` TheQuestMgr |
| FileMergerOrganizer.cpp | `82C3EAF8` gCatPriority |
| network/net/NetLog.cpp | `82C3EC00` NetLog (`"netlog-%05d.txt"`) |
| Server.cpp | `82C3EC40` gXboxServer |
| network/net/NetMessage.cpp | `82C3EC78` TheNetMessageFactory |
| Overlay.cpp | `82C3EDF0` `RndOverlay::sOverlays` |
| band3/net_band/XboxEntityUploader.cpp | `82C3F610` gXboxEntityUploader |
| ContextWrapper.cpp | `82C3F658` gContextCrit |
| DataNode.cpp | `82C40750` gDataVars |
| DataArray.cpp | `82C40840` gDataArrayConditional |
| system/obj/Utl.cpp | `82C408E8` sFilePaths; `82C40940` sFiles |
| Object.cpp | `82C40998` `Hmx::Object::sFactories` |
| DataFunc.cpp | `82C40A30` gDataFuncs |
| DataUtl.cpp | `82C40B20` gMacroTable |
| FilePath.cpp | `82C40DF0` `FilePath::sRoot` |
| ChunkStream.cpp | `82C40F80` gDecompressionCritSec; `82C40FB8` gDataProcessedEvt; `82C40FF0` gDataReadyEvt |
| XLSPConnection.cpp | `82C411C0` `XLSPConnection::mXLSPRefCountMap` |
| TempoMap.cpp / BeatMap.cpp | `82C41038` gDefaultTempoMap / `82C41048` gDefaultBeatMap |
| MidiParser.cpp | `82C41248` `MidiParser::sParsers` |
| Voice.cpp (synth_xbox) | `82C43100` gLockPendingLists; `82C43138` gVoiceGC; `82C43170` gPendingVoices; `82C431C8` gPendingSyncVoices; `82C43220` gInProgressVoices; `82C43278` gInProgressSyncVoices; `82C432D0` s_voiceGC |
| PostProc_NG.cpp | `82C438C0` `NgPostProc::sBloom` |
| VorbisReader.cpp | `82C43948` gReaders; `82C439A0` gNewReaders; `82C439F8` gLock |

All 130 rows read fuzzy 100 in the same-worktree build.

### 3.3 Console owns MultiMesh's statics

`Console.cpp` ends with `#include "rndobj/MultiMesh.cpp"` (and
`world/Crowd.cpp`), so retail's MultiMesh code is in the Console TU here:
`RndMultiMesh::Terminate` (`0x824686D0`, the reader of `sProxyPool`) is pinned
under Console, while MultiMesh.cpp's own heading is a 12 B stub at
`0x8230DF14`. `sProxyPool` and `gTransListAlloc` therefore go to Console.
`gTransListAlloc`'s stub is byte-equal; its initializer reads **99.94**: see §4.3.

### 3.4 `TokenRedemptionPanel::Poll` (5 stubs)

`0x82C48048`–`0x82C480C8` are registered from `0x82641670`, an **unpinned**
1,708 B function 12 B past TokenRedemptionPanel's last pinned block, carrying
exactly `Poll`'s strings (`token_redemption_msg`, `token_offers_ready`,
`checkout_finished`, …). Our obj has exactly five Poll-local
`static Message`s. Pairing by `Poll`'s reference order gives the same
assignment as the obj's symbol order against stub address order, and the five
sit directly before the unit's two already-attributed stubs
(`EnumerateOffers`, `OnMsg`). All five are byte-equal. `Poll` itself was not
pinned: that is a main-`.text` identification, outside this lane.

## 4. Residuals in attributed rows (rows pinned and named, below 100)

### 4.1 Fold-named callees (3 rows at 98.3)

16 stubs call a retail callee whose map name differs from ours: ICF
folded identical bodies. 13 of them already read 100 because
`scripts/symbol_aliases.json` covers the fold. Three do not:

| row | retail callee (map) | ours |
|---|---|---|
| `0x82C49AD0` Movie `gOpenMovies` | `_List_base<SynthPollable*>::clear` | `_List_base<Movie::Impl*>::clear` |
| `0x82C49B00` Task `TheTaskMgr` | `Hmx::Object::~Object` | `TaskMgr::~TaskMgr` |
| `0x82C4A108` Timer `sConditionalTimersEnabled` | `_List_base<DecompressTask>::clear` | `_List_base<Symbol>::clear` |

No alias was added: a fold must be proven on retail bytes before it may
forgive anything, and that is a separate adjudication. The `TheTaskMgr` row is
the one to check first, since a `TaskMgr` dtor folding into `Object`'s would
mean `TaskMgr` destroys nothing of its own.

### 4.2 dtk carve differs from the true stub (6 rows)

Each is the same 28 B `Message` stub as its neighbours, carved by dtk as 12 +
16 B (`BandSongMgr 0x82C460A8`, `AppLabel 0x82C46568`,
`CampaignSongInfoPanel 0x82C46DC8`), 20 + 8 B (`MainHubPanel 0x82C47948`), or 28 B
plus 4 B of trailing padding (`CampaignSongInfoPanel 0x82C46DA8`,
`TrackerDisplay 0x82C49500`). Each block covers the whole true body, so the
fragment rides along in the owner. The two padded ones read 100; the four
split ones read 0 / 60. Fixing the carve means `symbols.txt`, which
`ab_measure` refuses outright; not done.

### 4.3 `gTransListAlloc(0x48, …)` — a source finding, not edited

Retail's initializer at `0x82C3F2B8` passes `li r4, 0x48`; our
`src/system/rndobj/MultiMesh.cpp:12` (inherited from dc3-decomp, which has the
same line) passes `0x4C`. It is the `"InstanceListNode"` allocator's node size,
so retail's `RndMultiMesh::Instance` list node is 4 bytes smaller than ours.
That is a layout question for `RndMultiMesh::Instance`, not a one-constant
fix, so it was left for a struct lane; this lane's brief limits source edits
to missing globals.

## 5. In-scope rows left unattributed (25 rows, 480 B)

| stub(s) | initializer / referencer | why |
|---|---|---|
| `0x82C45F18`–`0x82C45FF8` (9) | functions at `0x82561EA0`–`0x825632F0` (`audition_main_screen`, `lost_connection`, `on_validation_success`, `rbn/audition/fail`) | AuditionMgr: link order puts these stubs between UIStats and ClosetMgr, where W16-QH placed AuditionMgr's initializer; TU5-only, no source. (One is a carve fragment.) |
| `0x82C46018` | `0x82C3FD60` (AuditionMgr initializer, W16-QH §3.1) | same TU |
| `0x82C45A40` | `0x82C3FA60` | Joypad's unnamed constant-initialized `ObjPtr<Hmx::Object>` (W16-QH §3.1) |
| `0x82C43ED0` | `0x82C3E7E0` | `MemOrPoolFreeSTL` vector at `.data 0x82CBC664`, no readers; 40 same-shaped candidates and nothing to choose between them |
| `0x82C440A0` | `0x82C3E818` | constant-initialized `ObjDirPtr<ObjectDir>` array at `.data 0x82C6C660`, read by BandHeadShaper; our BandHeadShaper.obj has no such static |
| `0x82C44E30` | `0x82C3EBF0` | 4 B `blr`: a trivially destructible static; it references nothing |
| `0x82C44FC8`, `0x82C44FD8` | `0x82C3ED10`, `0x82C3ED48` | a `CriticalSection` and one other static read by UI; our UI.obj defines neither |
| `0x82C45028` | `0x82C3EE48` | `std::list<Plane>`, no readers |
| `0x82C45188` | `0x82C3F258` | constant-initialized `ObjPtr<RndCam>` at `.data 0x82C707C4`, no readers |
| `0x82C45198` | `0x82C3F1F8` | a second `list<pair<RndMultiMeshProxy*,int>>`-shaped list just before Console's, no readers |
| `0x82C45310` | `0x82C3F470` | the 8th `RndShader` global (16 B initializer where our 12 are 12 B); Shader reads it once; our Shader.obj has no 16 B candidate |
| `0x82C45370` | `0x82C3F378` | `std::set<Symbol>`, no readers |
| `0x82C455B0` | `0x82C3F5D0` | a `String` built from a 1-character literal at `.data 0x82CC8F54`, no readers |
| `0x82C458E0` | `0x82C3F7F0` | a pointer list, no readers (link position RockCentral…PlatformMgr) |
| `0x82C46BC8` | `0x825EAFF8` (unpinned; handles `session_data_changed`) | referencer is unattributed code; no stub in our objs fits |
| `0x82C49D00` | `0x82C40AC0` | constant-initialized `ObjPtr<Hmx::Object>` at `.data 0x82C77308`, read once by DataFunc; our DataFunc.obj has no such static |

Pairing any of these would need a static our source does not have, or a pin of
the unattributed code that registers it. Inventing variable names was not done.

## 6. Quazal and XDK (not edited)

- 375 rows / 8,892 B are Quazal: 266 cleanup/unwind bodies with no referencer
  at `0x82C4B6C4`–`0x82C4CCBC` (`.data 0x82E1…`, callees in the `/Od` band);
  the stubs of the Quazal initializers at `0x82C40F7C`, `0x82C41820`,
  `0x82C41FF0`, `0x82C420E0` and the Voice span (callees in `0x82A6D168`–
  `0x82B54190`); function-local statics registered from inside that band; and
  the stubs of the initializers W16-QH attributed to Quazal (`0x82C41310`,
  `0x82C41430` `Session`, `0x82C41468`, `0x82C42158`, `0x82C42910`,
  `0x82C42958`, `0x82C42A68`). The four Quazal stubs that were already pinned
  (ProductFacade, ObjectThreadRoot ×2, DOCore) agree with the method and were
  left as they were.
- 9 rows / 1,228 B at `0x82C438F8`–`0x82C43E20` are XDK `LEAPFX`
  registration-property initializers (`CAudioVolumeMeter::sm_Registration…`),
  not stubs.

## 7. Not done

- No source edits, so no native gate run. `gTransListAlloc`'s `0x48` (§4.3) is
  the one source divergence found.
- No `symbols.txt` edits (the six carve rows in §4.2).
- No alias added for the three fold-named stubs (§4.1).
- `TokenRedemptionPanel::Poll` (`0x82641670`) and the other unpinned
  referencers (§5) were not pinned.
- No merge, no push (coordinator).

Scratch (not committed): `~/tmp/w16qi/` (`refidx.py`, `attr.py`,
`resolve.py`, `verify.py`, `plan.py`, `apply.py`, `witness.py`, `phase2.py`,
`phase2b.py`, `plan_p2.py`, `apply2.py`, `final_account.py`, A/B logs).
