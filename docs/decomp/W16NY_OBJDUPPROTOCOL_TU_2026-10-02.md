# W16-NY — the ObjDupProtocol TU written from retail (2026-10-02)

**Branch** `w16-ny-objdup`, started on main `c42c979f8`. **Ruler** `name_check` (graded). Permuter not run. No alias
group and no `symbol_aliases.json` entry touched. `ab_measure` and the native gate were left to the coordinator.

## 1. Result

Graded `report.json` of this worktree's full build: unit `network/quazal/ObjDupProtocol` **81 rows, 49 at fuzzy 100,
6,824 B of 18,056 B** (unit fuzzy 96.01). Whole binary in the same build: `matched_code` 5,647,780 /
`matched_functions` 51,634 (W16-NW's leg B on the same base read 5,640,480 / 51,582). This is a hand reading, not an
A/B; the unit was a 7-line map scaffold with every row at 0 before.

Outside the TU, 4 rows rose and none fell against the base (compared row by row against main's `report.json`; the two
CharBlendBone rows that differ belong to W16-NZ, merged after this branch started): MemoryManager's ctor and
`GetDefaultMemoryManager` to 100, Scheduler's ctor and `BandwidthCounterMap::operator[]` up a little. All of it comes
from the map fix in §3.

## 2. The TU

Retail `.\ObjDupProtocol.cpp` is `0x82A937E0..0x82A97EF0`, not the `0x82A93AB8..0x82A96A30` the scaffold pinned.
- It starts at the ctor. The ctor stores the vtables at `0x8217EADC` (Protocol) and `0x8217EACC` (ObjDupProtocol),
  which sit in `.rdata` right before the file string. The two functions before it use NetZ.cpp's vtable.
- It ends at the 8-byte EH prefix of ByteStream's first function. The last nine functions are the
  JobExecuteDelayedRMC job and the STLport/qMap helpers this TU instantiates: list insert/erase/clear, `_M_find`,
  a map lookup, a locked lookup, and a locked `empty()`.
- `ParseSpecificMessage` (`0x82A976B0`) dispatches to every Parse* by type, and the `"DOPROTOCOL message"` string is
  its own case.

Flags are `/Od /Oi- /EHs-c- /Ob1`. No function has EH state, and see §4 for `/Ob1`. The Wii symbol table gives the
source order and 39 method names. The X360 build adds a per-message Parse*/Process* split that Wii inlined away,
plus `CreateMessage`, `TraceMessage`, `ShouldDispatch`, `ExtractMessageType`, `CheckSourceStation`, `QueueMessage`,
`StartToListen`, `IsListeningOnAnyPort` and the helpers. **Those names were chosen by this lane and are not attested
anywhere.** Nothing outside the TU calls them by name today. The class layouts in the source are only as far as the
TU reads them.

## 3. Map

- 82 entries for the TU (`0x82A937E0..0x82A97E00`).
- One fix outside it: **`0x82A6DE40` was `XShowSocialNetworkImagePostUI` and is `??2RootObject@Quazal@@SAPAXIPBDI@Z`.**
  The body is `MemoryManager::Allocate(GetDefaultMemoryManager(), size, file, line, 3)`. It is the
  `(size, file, line)` sibling of `0x82A6DDE8` (`??2RootObject@Quazal@@SAPAXI@Z`, type 3), and the new[] pair at
  `0x82A6DE88`/`0x82A6DEE0` uses type 4. The XDK name came from the bindiff-r2 anchored transfer `aa86fb412`. It
  charged every Quazal `new (__FILE__, line)` call site, which is where the rows outside the TU come from. It is a
  separate commit.

## 4. What reproduces retail's /Od frames (measured; reusable for other /Od Quazal TUs)

1. **An in-class (or `inline`, defined-before-use) function the compiler declines to inline still has its slots
   allocated in the caller's frame.** Retail's unexplained frame space is these "phantoms":
   - the 12-byte gap before the context temp in every type-4 accessor is
     `InstanceTable::GetInstanceFromVector`'s `this`/`ui`/`idx` (the real `Core/InstanceTable.h` body; a smaller
     body is not refused the same way);
   - the +0x40 in `ProcessRMCResponse`/`ProcessCallOutcome` is `GetCallContextRef`'s frame;
   - the 0x70 frame of `??_GJobExecuteDelayedRMC` comes from its dtor being in-class.

   Probe (`~/tmp/w16ny-objdup/probe/p5.cpp`): the same body called in-class gives a 0x80 frame and out of line a
   0x60 frame. A refusal that comes from size alone (a loop-free big body, `p3.cpp`) leaves no phantom.
2. **Inlining is one level deep.** An inline called from an inline is called, not expanded. So
   `push_front`→`insert` and `pop_front`→`erase` stay out of line, as retail's `0x82A97C18`/`0x82A97CB0` are.
   Calling `insert(begin(), x)` directly inlines it.
3. **A parameter of an inlined function gets a temp home** (a slot right after the inline's own locals). Retail's
   context temp is that home: `GetInstance()` → `GetInstance(PseudoSingleton::GetCurrentContext())`.
4. **Retail's inline returns are `if (...) return a; else return b;`.** The double `b` after the first return comes
   from this form; the early-return form drops one branch.
5. **The class-type rules are what the code was written to.** A small class with ctor and dtor but no copy ctor
   (`StreamPosition`; `probe/p7.cpp` H3) is returned by sret and passed by value in a register. `DOHandle` has a copy
   ctor, so it goes by pointer to a temp. An empty class with a user ctor (`DebugString() {}`) is passed by address
   with no zeroing store.
6. Local slot order is the W16-NW hash walk. `probe/order.py` sorts candidate names. Block-scoped locals go above
   the enclosing scope's (ProcessBundleMessage's `pSubMsg` had to move out of the loop).

## 5. Rows below 100 that cannot reach it (structural, not source)

| row | why |
|---|---|
| `ParseSpecificMessage` (752 B, 92.98) | /Od emits an inline absolute-address `bctr` table (22 words). The target obj carries the resolved addresses with no relocations, and ours carries relocations, so the 22 words always differ; the other 166 instructions match. |
| `ProcessDeleteMessage`, `ProcessFetchRequestMessage`, `ShouldGrabWellKnown` | call `0x823EA598`, a 12-byte copy ICF-folded under `reverse_iterator<Synchronizable**>` (alias group FOLDPROVE1); this lane may not touch aliases |
| `ListenOnWellKnown`, `ListenOnAnyPort`, `StopToListen`, `AddLocalURLs` | call `0x823EBC90`, named `?GetInstanceType1Delegator@?A0x50e10b5e@@` (a NetworkEmulator anon-namespace function folded with Quazal's) |
| `ProcessActionMessage` | calls `0x82A707E8`, mapped `??1ScopedCS@Quazal@@QAA@XZ`, but retail calls it with a class id and uses the result as a `DOClass*` — the map name looks wrong (not changed here) |

The other sub-100 rows are frame-layout residue of the same kind as §4 (phantom sizes and temp order), plus
`Receive`'s one-register shift in the else branch, which seven variants did not move.

## 6. Per-function result (report.json, graded)

| addr | size | function | fuzzy |
|---|---:|---|---:|
| 82A937E0 | 188 | ObjDupProtocol::ObjDupProtocol | 100.000 |
| 82A938A0 | 80 | Protocol::`scalar deleting dtor' | 100.000 |
| 82A938F0 | 72 | ObjDupProtocol::`scalar deleting dtor' | 100.000 |
| 82A93938 | 88 | ObjDupProtocol::~ObjDupProtocol | 100.000 |
| 82A93990 | 56 | ObjDupProtocol::AddMessageType | 100.000 |
| 82A939C8 | 172 | ObjDupProtocol::FaultDetection | 100.000 |
| 82A93A78 | 64 | ObjDupProtocol::PeerDisconnected | 100.000 |
| 82A93AB8 | 120 | ObjDupProtocol::CreateMessage | 100.000 |
| 82A93B30 | 112 | ObjDupProtocol::ReleaseMessage | 100.000 |
| 82A93BA0 | 480 | ObjDupProtocol::Receive | 99.500 |
| 82A93D80 | 288 | ObjDupProtocol::Send | 100.000 |
| 82A93EA0 | 240 | ObjDupProtocol::QueueMessageFromLocalStation | 100.000 |
| 82A93F90 | 232 | ObjDupProtocol::QueueMessage | 99.862 |
| 82A94078 | 116 | ObjDupProtocol::TraceMessage | 100.000 |
| 82A940F0 | 248 | ObjDupProtocol::Dispatch | 99.694 |
| 82A941E8 | 312 | ObjDupProtocol::ShouldDispatch | 99.654 |
| 82A94320 | 248 | ObjDupProtocol::ExtractMessageType | 100.000 |
| 82A94418 | 360 | ObjDupProtocol::CheckSourceStation | 96.956 |
| 82A94580 | 44 | ObjDupProtocol::CreateDOProtocolMessage | 100.000 |
| 82A945B0 | 276 | ObjDupProtocol::ProcessDOProtocolMessage | 89.217 |
| 82A946C8 | 52 | ObjDupProtocol::CreateGetParticipantsRequest | 100.000 |
| 82A94700 | 120 | ObjDupProtocol::ParseGetParticipantsRequest | 100.000 |
| 82A94778 | 248 | ObjDupProtocol::ProcessGetParticipantsRequest | 100.000 |
| 82A94870 | 52 | ObjDupProtocol::CreateGetParticipantsResponse | 100.000 |
| 82A948A8 | 104 | ObjDupProtocol::ParseGetParticipantsResponse | 100.000 |
| 82A94910 | 88 | ObjDupProtocol::ProcessGetParticipantsResponse | 100.000 |
| 82A94968 | 188 | ObjDupProtocol::CreateJoinRequest | 88.830 |
| 82A94A28 | 508 | ObjDupProtocol::ParseJoinRequestMessage | 100.000 |
| 82A94C28 | 196 | ObjDupProtocol::ProcessJoinRequest | 99.653 |
| 82A94CF0 | 244 | ObjDupProtocol::CreateJoinResponse | 99.574 |
| 82A94DE8 | 132 | ObjDupProtocol::ParseJoinResponseMessage | 100.000 |
| 82A94E70 | 424 | ObjDupProtocol::ProcessJoinResponse | 92.764 |
| 82A95018 | 108 | ObjDupProtocol::GetJoinResponseObserver | 100.000 |
| 82A95088 | 128 | ObjDupProtocol::CreateUpdateMessage | 100.000 |
| 82A95108 | 264 | ObjDupProtocol::ParseUpdateMessage | 86.955 |
| 82A95210 | 76 | ObjDupProtocol::CreateDeleteMessage | 100.000 |
| 82A95260 | 224 | ObjDupProtocol::ParseDeleteMessage | 94.161 |
| 82A95340 | 432 | ObjDupProtocol::ProcessDeleteMessage | 91.324 |
| 82A954F0 | 96 | ObjDupProtocol::CreateActionMessage | 100.000 |
| 82A95550 | 360 | ObjDupProtocol::ParseActionMessage | 93.433 |
| 82A956B8 | 228 | ObjDupProtocol::ProcessActionMessage | 90.281 |
| 82A957A0 | 288 | ObjDupProtocol::CreateRMCCallMessage | 99.833 |
| 82A958C0 | 284 | ObjDupProtocol::ParseRMCCallMessage | 97.070 |
| 82A959E0 | 592 | ObjDupProtocol::ProcessRMCCallMessage | 99.912 |
| 82A95C30 | 172 | JobExecuteDelayedRMC::Execute | 100.000 |
| 82A95CE0 | 72 | JobExecuteDelayedRMC::`scalar deleting dtor' | 100.000 |
| 82A95D28 | 212 | JobExecuteDelayedRMC::~JobExecuteDelayedRMC | 100.000 |
| 82A95E00 | 96 | ObjDupProtocol::CreateRMCResponseMessage | 100.000 |
| 82A95E60 | 132 | ObjDupProtocol::ParseRMCResponseMessage | 100.000 |
| 82A95EE8 | 120 | ObjDupProtocol::ProcessRMCResponse | 99.667 |
| 82A95F60 | 188 | ObjDupProtocol::CreateFetchRequestMessage | 99.851 |
| 82A96020 | 240 | ObjDupProtocol::ParseFetchRequestMessage | 89.650 |
| 82A96110 | 712 | ObjDupProtocol::ProcessFetchRequestMessage | 81.416 |
| 82A963D8 | 268 | ObjDupProtocol::CreateMigrationMessage | 99.388 |
| 82A964E8 | 108 | ObjDupProtocol::CreateCallOutcomeMessage | 100.000 |
| 82A96558 | 208 | ObjDupProtocol::ParseCallOutcomeMessage | 100.000 |
| 82A96628 | 140 | ObjDupProtocol::ProcessCallOutcome | 100.000 |
| 82A966B8 | 524 | ObjDupProtocol::ProcessBundleMessage | 99.611 |
| 82A968C8 | 76 | ObjDupProtocol::CreateEOSMessage | 100.000 |
| 82A96918 | 280 | ObjDupProtocol::QueueEOS | 99.943 |
| 82A96A30 | 136 | ObjDupProtocol::ParseEOSMessage | 100.000 |
| 82A96AB8 | 176 | ObjDupProtocol::ProcessEOS | 99.636 |
| 82A96B68 | 636 | ObjDupProtocol::ShouldGrabWellKnown | 90.686 |
| 82A96DE8 | 76 | ObjDupProtocol::GetInstance | 100.000 |
| 82A96E38 | 428 | ObjDupProtocol::ListenOnWellKnown | 69.252 |
| 82A96FE8 | 100 | ObjDupProtocol::StartToListen | 100.000 |
| 82A97050 | 376 | ObjDupProtocol::ListenOnAnyPort | 90.766 |
| 82A971C8 | 448 | ObjDupProtocol::StopToListen | 88.312 |
| 82A97388 | 16 | ObjDupProtocol::IsListeningOnWellKnown | 100.000 |
| 82A97398 | 16 | ObjDupProtocol::IsListeningOnAnyPort | 100.000 |
| 82A973A8 | 212 | ObjDupProtocol::IsListening | 100.000 |
| 82A97480 | 452 | ObjDupProtocol::AddLocalURLs | 79.637 |
| 82A97648 | 104 | ObjDupProtocol::ParseMessage | 100.000 |
| 82A976B0 | 752 | ObjDupProtocol::ParseSpecificMessage | 92.979 |
| 82A979A0 | 192 | StationURLList::IsEmpty | 100.000 |
| 82A97A60 | 272 | CallContextRegister::GetCallContextRef | 100.000 |
| 82A97B70 | 168 | CallContextRegister::FindCallContext | 100.000 |
| 82A97C18 | 148 | list<Message*>::insert | 100.000 |
| 82A97CB0 | 172 | list<Message*>::erase | 100.000 |
| 82A97D60 | 156 | _List_base<Message*>::clear | 100.000 |
| 82A97E00 | 240 | map<ushort,CallContext*> _Rb_tree::_M_find | 100.000 |

## 7. Not done
- ab_measure, native gate (coordinator).
- The `/Od` jump-table relocation gap in jeff (would unlock `ParseSpecificMessage`).
- `0x82A707E8`'s map name.
- Dispatch's 0x80 of phantom space: making ShouldDispatch/ExtractMessageType/CheckSourceStation inline-refused
  callees gets it to 0x70 of 0x80.

Scratch: `~/tmp/w16ny-objdup/` (`try.py` per-row scorer, `pvar.py` parallel variants, `probe/order.py`, `probe/p*.cpp`).
