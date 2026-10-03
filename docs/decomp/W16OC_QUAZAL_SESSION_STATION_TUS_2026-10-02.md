# W16-OC — the Quazal session/station TUs written from the retail asm (2026-10-02)

**Branch** `w16-oc`, rebased onto main `36d8bd9f3` (after W16-OE and W16-OD landed). **Ruler** `name_check` (graded). Permuter not run. Method and TU table:
`docs/decomp/W16NY_OD_BLOCK_PRICING_AND_LARGEST_TUS_2026-10-02.md`. Lanes W16-OD (transport) and W16-OE
(account/DO) ran in parallel on other Quazal TUs.

Seven TUs: JobJoinSession, Station, Session, StationDDL, SystemComponent, StationManager, JobConnectStation.
SystemComponent was written by the coordinator; the other six by one sub-lane each, in their own worktrees, then
merged here.

## 1. Extents

Every scaffold pin covered only the span of its `__FILE__` references. The true extents were found from the
`.rdata` layout (each TU's vtables and EH tables sit next to its path string, in link order) and the ctor that
stores each vtable.

| TU | old pin | retail extent | B | how the edges were found |
|---|---|---|---:|---|
| Session | `82A78008..82A78664` | `82A76B58..82A7ADD0` | 17,016 | starts right after DuplicatedObject (its vtables `8217DC00`/`8217DC48` follow DuplicatedObject's strings); ends where the next TU's vtable user `82A7ADD0` starts |
| Station | `82A7C6F0..82A7DE08` | `82A7B860..82A7E2E0` | 10,880 | ctor stores vtable `8217DEA0`; the three functions before it belong to the preceding TU (`82A7B818` is its one-slot vtable `8217DE98`); ends after the `PseudoGlobalVariable<DOHandle>` methods; `82A7E2E0` loads SystemClock strings |
| StationDDL | `82A82830..82A832B4` | `82A82830..82A847B8` | 8,072 | `_DOC_Station::Create` first; the TU's template tail ends with `map<uchar,UpdatePolicy*>::_M_find`; DOFilter subclass code starts at `82A847B8` |
| SystemComponent | `82AA7610..82AA78E4` | `82AA6B98..82AA78F0` | 3,416 | starts at the ctor (stores vtable `8217FC38`); `82AA6918..82AA6B98` is RefCountedObject (vtable `8217FC28`, a separate TU); ends at Buffer's ctor (vtable `8217FE10`, EH frame) |
| JobConnectStation | `82AB6A58..82AB7718` | `82AB4CA0..82AB7EF0` | 12,880 | ctor stores vtable `82181368`; tail is its `list<Step>` insert/erase/create_node |
| StationManager | `82AB87A0..82AB9504` | `82AB7EF0..82ABAB58` | 11,368 | ctor stores vtable `821815C0` and names `"StationManager"`; 17 STL helpers at the end; CallRegister's first function refs its vtable `82181630` |
| JobJoinSession | `82AC8748..82AC9F44` | `82AC8658..82ACAB10` | 9,400 | JoinCancelCallback ctor stores vtable `8218271C`; ends before the JoinSession operation class (vtable `82182970`) |
| **total** | 25,056 B pinned | | **73,032** | |

All seven fall inside W16-NY's [lo, hi] bracket, which now holds on 16 of 16 known extents. The ratio true/lo here
runs 1.54–10.45 (Session: one anchored function, 17 KB of TU), against 1.01–3.29 in W16-NY's nine, so the ~340 KB
estimate for the 105 path-anchored TUs (lo × 1.8) is low.

## 2. Per-TU results

Unit totals, main vs this branch (full builds of the lane worktree, `report.json`, graded; rows at fuzzy 100 /
rows, matched / unit B). Row counts include STL/template instantiations the TU emits.

| TU | flags | main | branch |
|---|---|---:|---:|
| Session | `/Od /Oi- /Ob1 /EHs-c- /GR-` | 0/1, 0 / 1,628 | 38/67, 4,580 / 16,892 |
| Station | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 0/27, 0 / 5,836 | 43/58, 6,216 / 10,744 |
| StationDDL | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 0/19, 0 / 2,672 | 43/46, 6,820 / 8,000 |
| SystemComponent | `/Od /Oi- /Ob1 /EHs-c-` | 0/1, 0 / 724 | 14/16, 2,028 / 3,368 |
| JobConnectStation | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 0/9, 0 / 3,240 | 24/34, 8,804 / 12,812 |
| StationManager | `/Od /Oi- /Ob1 /EHs-c- /GR-` + 3 allocator `/D` gates | 0/8, 0 / 3,424 | 32/41, 8,012 / 11,296 |
| JobJoinSession | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 0/15, 0 / 6,108 | 27/34, 5,568 / 9,316 |
| DuplicatedObject (side effect) | | 68/120, 12,136 | 74/120, 15,096 |
| **sum of matched** | | | **+44,988 B** |

Every TU has no EH state or funclets (`/EHs-c-`), and no vtable of any of the seven is preceded by an RTTI locator
(`/GR-`). SystemComponent's `objects.json` entry still lacks `/GR-`: the word before its vtable (`0x8217FC34`) is 0,
which was checked only after the A/B; the flag does not change `.text`.

### 2.1 SystemComponent (coordinator)

`/Od /Oi- /Ob1 /EHs-c-`. 16 rows, 14 at fuzzy 100; unit 2,028 of 3,368 B. `Use::Use`/`~Use` call two
out-of-line helpers (`0x82AA6D78`, `0x82AA6E08`; named `BeginUse`/`EndUse` here, not attested). Whole build in the lane worktree: 52,189 → 52,203 fns, 5,716,504 →
5,718,532 B, i.e. exactly the unit's own gain.

| addr | size | function | fuzzy |
|---|---:|---|---:|
| 82AA6B98 | 116 | SystemComponent::SystemComponent(const String&) | 100 |
| 82AA6C10 | 16 | SystemComponent::GetType | 100 |
| 82AA6C20 | 80 | SystemComponent::~SystemComponent | 100 |
| 82AA6C70 | 52 | SystemComponent::SetName | 100 |
| 82AA6CA8 | 144 | SystemComponent::SetState | 100 |
| 82AA6D38 | 64 | SystemComponent::SetParent | 100 |
| 82AA6D78 | 140 | SystemComponent::BeginUse | 100 |
| 82AA6E08 | 124 | SystemComponent::EndUse | 100 |
| 82AA6E88 | 596 | SystemComponent::ValidTransition | 100 |
| 82AA70E0 | 68 | SystemComponent::UseIsAllowed | 100 |
| 82AA7128 | 100 | SystemComponent::Trace | 100 |
| 82AA7190 | 368 | SystemComponent::Initialize | 100 |
| 82AA7300 | 616 | SystemComponent::Terminate | 93.77 |
| 82AA7568 | 92 | SystemComponent::Use::Use | 100 |
| 82AA75C8 | 68 | SystemComponent::Use::~Use | 100 |
| 82AA7610 | 724 | SystemComponent::WaitForTerminatedState | 99.84 |

What decided the frames (each found by a probe compile, `~/tmp/w16oc/probe/`):

- **An empty `if (name) {}` on an unused parameter takes one register and emits nothing.** `BeginUse`/`EndUse`
  start one register lower than a plain body (r10 instead of r11). Of about 25 candidate statements probed, only
  an empty `if` on a non-constant value did this (`if (this) {}` and `if (this != 0) {}` too, and `if (member) {}`
  takes two); `(void)x`, `(void)(cond)`, `if (0)`, `do {} while (0)`, an inlined empty call, a volatile member and
  the other spellings of the decrement do not). Read as a trace on the user's name compiled out of retail.
- **`switch (GetState())` through an inline accessor** gives retail's prologue: the inline's return temp (not yet
  written) is copied into the switch temp, then `mState` overwrites it. `_State s = mState; switch (s)` and
  `switch (s = mState)` do not.
- `SetState` is the plain early-return form (the if/else form adds a dead branch); `Use::Use` passes the ctor's
  `sc` parameter and the stored `mName`; the second wait loop folds `SpinOnce` into the `while` condition; the
  dispatching-thread early return has an explicit `else`.

Rows below 100:

- `Terminate` (93.77): its `/Od` switch is an inline absolute-address `bctr` table of 16 words. The target object
  holds them as raw words and ours as relocations, so they always differ (as for ObjDupProtocol's
  `ParseSpecificMessage`); the other 138 instructions match.
- `WaitForTerminatedState` (99.84): every local matches; only the two `Scheduler::GetInstance` temps sit 8 B
  higher in retail (`0xf8/0xfc` vs `0xf0/0xf4`) because retail reserves 20 B for the declined
  `Core::GetInstance` and the current header reserves 12 B. See §3.4.

### 2.2 Session (sub-lane)

38/67. Below 100: `OperationBegin` 94.18, `InvolvesLocalStation` 87.65, `OperationEnd` 99.79, `CreateSession` 98.30,
`CompleteCreation` 85.40, `JoinSessionImpl` 93.62, `SynchronizeTermination` 82.06, `JoinIsAllowed` 84.47,
`InitSessionDescription` 97.50, the PGV ctor/Allocate/Free/dtor, and the list copy/assign/splice helpers. Causes:
inlining retail declines that we expand or the reverse (`DOHandle(unsigned)` and the DOHandle copy nested in an
inline, `GetNbKeys`, `SelectionIterator::EndReached`, STLport `_M_empty_initialize`); the 12 dead bytes after each
`NetZ::GetInstance()`; `JoinIsAllowed`'s `&&` evaluating `GetTime()` first; folded callee names. Six
`map<String,String>` tree rows (1,160 B) are unwritten: nothing attested in Session.cpp instantiates them. Local
variable names in `CreateSession`/`JoinSessionImpl` were chosen by slot probing (95.5 → 98.3).

### 2.3 Station (sub-lane)

43/58. Below 100: `SetState` 99.74 and `GetLocalInstance`-style accessors (the `GetInstanceFromVector` phantom),
`ReleaseSystemReferences` 29.28 (retail expands `GetHandle()` at one of four sites only), `FlushBundle` 99.74 (callee
is an anonymous-namespace fold survivor), `FlushAllBundles` 88.89 and `InitiateFaultProcessingForStation` 89.9
(whether `SelectionIteratorTemplate`'s ctor and `EndReached` expand), `GetLocalStationHandle` 73.88,
`qResult::Equals(const bool&)` 0 (would need `Result.h` inline), PGV `AllocateExtraContexts` 90.10, two unnamed
COMDATs. **`DOID` no longer derives from `RootObject`**: with the base, `/Ob1` never expands `GetHandle()`; without
it, it expands exactly where retail does in this TU. That answers the question W16-NY §3.3 left open, and took six
DuplicatedObject rows to 100 (`AddToCachedDuplicationSet`, `Create`, `EmigrateTo`, `ExecuteOperation`,
`MigrationInProgress`, `Publish`; +2,960 B).

### 2.4 StationDDL (sub-lane)

43/46. Below 100: `_DOC_Station::DataSetsOperation` 98.38 and `UpdateProtocol::UpdateProtocol()` 0 share a cause:
retail declines to inline that ctor at its fourth use only (`/Ob1`'s per-function budget, measured; what fills it
sooner in retail is not identified, and no filler was added). `_DO_Station::CallSignalAsFaulty` 99.92 is a
fold-alias charge (`0x82B35690` is mapped to its ICF survivor). The four vtables at `0x8217E2A4..` are DOFilter
subclasses, not `BasicUpdateProtocol<>` as the coordinator's brief said; the real ones are `0x8217E188/1A0/1D0/1E8`.
The first version re-declared shared Quazal classes inside the .cpp (the coordinator's brief invited it); the lane
rewrote it onto the real headers with every score unchanged.

### 2.5 StationManager (sub-lane)

32/41. Below 100: `GetInstance` 99.68 (the `GetInstanceFromVector` phantom), `AddBootstrapStationURLs` 99.68,
`ActivateJob` 97.61, `GetTargetConnectionState` 99.15, `StateTransition` 89.62, `ConnectionIsPossible` 95.51 (all
call `0x823EA598`, the folded 4-byte copy ctor mapped as `reverse_iterator<Synchronizable**>`), `list::operator=`
87.91 and `list::insert` 0 (our allocator shape inlines `insert` into `operator=`; retail does not),
`_M_splice_insert_dispatch` 99.87. `SystemComponent::StateTransition` is a void empty virtual: slot 7 of the 28
SystemComponent vtables that do not override it is `0x826C3888`, a bare `blr`. Making `DORefTemplate::IsValid` an
inline the compiler declines reproduces retail's 12-byte reservation and took `ConnectStation` (796 B) to 100.

### 2.6 JobConnectStation (sub-lane)

24/34. Below 100: the dtor and `Execute/CancelQueuedJobs` (callee name charges on shared `list<Message*>` copies),
`ConnectionFailed` 98.75 (`0x823EA598`), `CheckExceptions`/`FindIncomingEndPoint`/`QueueOperation` (the
`GetInstanceFromVector` phantom), `ConnectOrphanStation` 75.78 (we expand `GetHandle()` there, retail calls it),
`CompleteConnection`/`ProcessConnectOrphanResult` (0x10 more frame before the first `operator->` in retail; cause
not found). `Core/Job.h` already matches retail (0x38 B), so only `StepSequenceJob` stays local (retail keeps a
0x10-byte Step at 0x48, which `StepSequenceJob.h` does not).

### 2.7 JobJoinSession (sub-lane)

27/34. The seven rows below 100 (`PrepareURL`, `InitiateConnection`, `SendGetParticipantsRequest`,
`WaitForJoinTermination`, `SignalCallContext`, `JoinSuccess`, `CompleteJob`, all 98.7–99.85) differ only in frame
slots and, in `JoinSuccess`, register numbering: writing `InitiateConnection`'s second appended word in retail's temp
shape makes `/Ob1` stop expanding `~ScopedCS` there, and the `GetInstanceFromVector` phantom. `StepSequenceJob`
stays local (retail: step start time 0x38, a word at 0x58 the ctor sets to 4, a 0x10-byte Step). JobJoinSession is
0xe0 bytes (URL list 0x60, EndPoint* 0x68, call id 0x70, cancel callback 0x74, CallContext 0x78, participant list
0xc8, qResult 0xd0).

## 3. Shared headers changed

### 3.1 By the coordinator (SystemComponent)

- **`Core/Scheduler.h`.** `unk90` was `qMap<Time, Job *>` (0x1c: `qMap` adds `RootObject` as a second, empty base,
  which MSVC does not collapse, and StationURL's retail `qMap`s really are 0x1c apart). Retail's Scheduler ctor
  (`0x82AC57F0`) builds a bare tree at `0x94` and stores `unka8` at `0xac`, the WaterMarks at `0xb0`/`0xe0`,
  ProfilingUnits at `0x110`/`0x180`, `SingleThreadCallPolicy` at `0x16c`, and the last bool at `0x1d9`. With a
  0x18 `std::multimap` every one of those offsets matches the compiler's layout report (`sizeof` 0x1e8 → 0x1e0);
  before it, every member after `0x94` was 4–8 B high. Only `Scheduler.cpp`'s unmatched ctor names those members.
  map vs multimap cannot be told from the ctor. Also declares `GlobalSingleThreadDispatch(unsigned)`.
- **`Core/SystemComponent.h`.** Adds `BeginUse`/`EndUse` and the inline `GetState()`/`GetUseCount()` accessors
  whose temps the retail frames show. No layout or vtable change.

### 3.2 By the sub-lanes, and how they were integrated

All six sub-lanes ended on the real shared headers. Job/StepSequenceJob stay local in JobConnectStation.cpp and
JobJoinSession.cpp (as in JBESL and ObjDupProtocol), by the coordinator's ruling, because `StepSequenceJob.h`'s
layout is wrong. Changes, by header:

| header | change | from | why |
|---|---|---|---|
| `ObjDup/DOID.h` | DOID no longer derives from `RootObject` | Station | `/Ob1` expands `GetHandle()` exactly where retail does |
| `ObjDup/DOHandle.h` | inline `operator=`; `GetValue()`; `operator>>` decl | Station, Session, JJS, JCS, StationDDL | retail assigns through a homed inline operator |
| `ObjDup/DORef.h` | `GetHandle()` returns `m_hReferencedDO`; `GetPtr()`, `IsAcquired()` | JCS | retail copies the member at JCS's sites |
| `ObjDup/DORefTemplate.h` | `IsValid()` in-class (declined inline, reads `GetDOPtr()` twice as `0x82A76640`); `operator->` | JCS (StationManager/Session had an equivalent out-of-class inline in `DuplicatedObject.h`, removed at merge) | retail callers reserve its 12-byte frame |
| `ObjDup/Station.h` | retail methods/members 0xb4..0x194, `MessageBundle` 8-aligned (`sizeof(Station)` = retail's 0x198), renames | Station, StationDDL | |
| `ObjDup/StationDDL.h`, `RootDODDL.h`, four DDL dataset headers | `_DO_Station` datasets at 0x70/0x98/0xa8/0xb0, `_DOC_Station`, `_DOC_RootDO`, user dataset classes | Station, StationDDL, Session | |
| `ObjDup/DOClass.h` | 24th virtual `FillDupSpacesInfo`, policy map at 0xc, `DOClassTemplate`, `GetWKHandle` | StationDDL, JJS | retail DO-class vtables have 24 slots |
| `ObjDup/DOOperation.h` | `CallMethodOperation` method id 0x32, result 0x40 | StationDDL | |
| new `ObjDup/UpdatePolicy.h`, `RMCContext.h`, `MethodIDGenerator.h` | | StationDDL | |
| new `ObjDup/StationManager.h`, `ObjDup/JobChangeConnection.h` | StationManager and its job classes (split at merge, below) | StationManager, coordinator | |
| `ObjDup/Session.h`, `SessionDDL.h` | Session members 0x600..0x618, `_DO_Session` datasets | Session, JCS | |
| `Core/SystemComponent.h` | `StateTransition` is `virtual void ... {}`; `Use::ComponentExists`, `IsTerminating`; `BeginUse`/`EndUse`/`GetState`/`GetUseCount` | StationManager, JCS, coordinator | slot 7 is a bare `blr` in retail |
| `Core/NetZ.h` | ConnectionManager* 0x1c, StationManager* 0x20, SessionDiscoveryTable* 0x2c, StationIdentification* 0x3c, Listener* 0x40, SystemComponent* 0x48 carved from padding | all | no offset moves |
| `Core/PseudoGlobalVariable.h` | retail `AllocateExtraContexts`/`FreeExtraContexts`, `SetValue`, out-of-class `GetValue` | Station, Session | |
| `Plugins/EndPoint.h` | PRUDPEndPoint's virtual order, stream at +4, address at +8 | JJS, JCS | |
| `Platform/qMemAllocator.h` | `RB3_QUAZAL_MEMALLOCATOR_DTOR` gate | StationManager | |
| declaration-only additions | `CallContext.h`, `CallRegister.h`, `DOCallContext.h`, `SelectionIterator.h`, `ObjDupProtocol.h`, `BundlingPolicy.h`, `Result.h`, `SystemError.h`, `StreamBundling.h`, `StreamSettings.h`, `ByteStream.h`, `UserContext.h`, `DuplicatedObject.h` (`Update`) | various | |

Integration fixes made on this branch (each measured on the merged tree):

- Parallel additions collided four times (`_DO_Station::Create`, `DOHandle::GetValue`, `DOClass.h`, `NetZ.h`'s pad);
  each kept both sides with no offset move.
- **`StationManager.h` split.** It pulled the real `StepSequenceJob.h` for its job classes, so JobJoinSession and
  JobConnectStation (local `StepSequenceJob`) declared StationManager stand-ins (`StationTable`, a local
  `StationManager`). The job classes now live in `ObjDup/JobChangeConnection.h`; both TUs use the real class.
  +1 fn / +272 B, no row down.
- **One spelling per retail function.** Callers whose placeholders became named rows were respelled to the owning
  TU's name: ObjDupProtocol (`Session::JoinIsAllowed`, `Session::GetInstance`, `JobConnectStation::QueueJob`, and
  StationDDL's/JobJoinSession's/Station's names), JobJoinSession (`StationManager::...`). ObjDupProtocol's local
  class misnamed `Session` was the type-4 (NetZ) instance; it is `NetZContext`.
- **PRUDPStream's `InetAddressList` holds `StationURL`s.** Retail's dtor clears it through
  `_List_base<StationURL>::clear` (`0x82A79DA8`, which runs `~StationURL`); the `qList<InetAddress>` spelling was
  hidden until Session named that row. `~InetAddressList` and `SendBroadcast` back to 100.
- **Not reconciled: `DORef::GetHandle`'s form is site-dependent.** DuplicatedObject's and JobConnectStation's sites
  want the member copied; Session's want `DOHandle(GetReferencedHandle())` (the out-of-line `DOHandle(unsigned)`),
  all on `MasterStationRef`. Moving the copy form to `DORefTemplate` only swaps which rows are higher; it measured
  Δ0 fns / Δ0 B (no row crosses 100 either way), so JCS's form was kept.

### 3.3 Reconciling with W16-OE and W16-OD (landed while this lane ran)

Rebasing onto main after W16-OE and W16-OD surfaced the same "one spelling per retail function" problem across lanes.
Each case was decided on retail bytes or on the landed map:

- **Main's names kept:** `CallRegister::GetInstanceRef()` returns a reference; `0x82ABB820` is
  `CallRegister::GetFetchContext`; `0x82A8B9E0` is `CallContext::InitiateCall`; `0x82ABB298` is
  `CallRegister::QueueCancelCallToStation`; ObjDupProtocol's type-4 instance is W16-OE's `DOCore`; `_DO_RootDO`'s
  extractor is `SpecificExtractADataset`, and `_DO_Station`'s now follows the same name (`DOClassTemplate`
  forwards to it). Callers on this branch were respelled.
- **One landed name corrected:** `0x82A99638` `_DOC_RootDO::DispatchRMCResult` returns `bool`, not `void`.
  `DOClass::DispatchRMCResult` (`0x82AB2278`) is `li r3,0; blr`, and `_DOC_Station`'s override (`0x82A83050`)
  returns 1 on one path and this function's value on the other. One virtual slot cannot be both, and `/Od` emits
  `return f(x);` as a bare `bl`, which is why the root's body reads like a void call. The row stays at 100
  under the corrected name.
- **A patcher gap, fixed.** `scripts/obj_anon_ns_patcher.py` matched only `?A0x<hash>@@` (global scope).
  W16-OD's `JobConnectEndPoint` callbacks live in `Quazal::{anon}` (`?A0x<hash>@Quazal@@`), so their hash was
  never rewritten and four W16-OD rows were 100 only while a cache-served object carried the hash their map
  names were taken from. This branch's header additions recompile that TU, and the four rows went to 0 / 0 /
  99.91 / 99.94 (−4 fns / −1,632 B in the first A/B on `36d8bd9f3`). The hash is not even stable per build
  root: compiling the unchanged file by hand from main and from a worktree gave `18540d5a` and `aa73134c`,
  against `8260cece` / `b4b23aec` in the ninja-built objects. The pattern now accepts `@<scope>` after the hash;
  global templates are unchanged, the hashless `?A@@` keeps its own marker. Result: exactly those four rows
  back to 100 and no other row moved; `--batch --check`, `verify_objs_patched.py --check` and
  `test_patch_state.py` (27/27) pass.

### 3.4 A probe of the open `Core`/`Scheduler` shape (W16-NY §3.3), not taken

Retail `0x823EA910` (`Core::GetInstance`'s surviving copy) is an optimised `/O1` body (`mr r5,r3`, no stack
temps): ICF kept a non-`/Od` copy, so it says nothing about the `/Od` source. Only the `/Od` callers' reserved
frames do. Retail `GetSystemLock` (`0x82A6F650`) and `WaitForTerminatedState` both reserve 20 B between the
inlined `Scheduler::GetInstance`'s `inst` and its temps. Probe (`GetSystemLock` emitted out of line, temps / frame):

| `Core::GetInstance` locals | `GetInstanceFromVector` | temps | frame |
|---|---|---|---|
| uiContext, inst, pCore (current) | `noinline` (current) | 0x60/0x64 | 0x70 |
| uiContext, inst, pCore | inline | 0x6c/0x70 | 0x80 |
| inst only (main before W16-NY) | inline | 0x64/0x68 | 0x80 |
| inst only | `noinline` | 0x58/0x5c | 0x70 |
| **uiContext, inst** | **inline** | **0x68/0x6c** | **0x80** (retail) |

So a declined inline's reservation is its own locals plus those of the inline it in turn declines. The last
shape reproduces both retail frames exactly. Measured whole-binary on this branch: **52,203 → 52,197 fns,
5,718,532 → 5,714,472 B (−6 / −4,060 B)**. `WaitForTerminatedState` and five ObjDupProtocol rows reach 100
(the five W16-NY's E2 found), while ten DuplicatedObject rows and six PRUDPStream rows leave 100 for 99.7–99.9.
Different retail call sites therefore reserve different amounts: 12 B at the DuplicatedObject sites and 20 B at
these. That points to **different inline helpers at different call sites** (for example a context-taking
`GetInstance(unsigned)` overload, or a direct `Core` accessor) rather than one missing shape for a single
function. Reverted; the build restored to 52,203 / 5,718,532 B exactly.

## 4. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-oc-ab --patch ab5.patch` (the whole `main..w16-oc` diff), in a
fresh `setup_worktree.sh` worktree on main **`36d8bd9f3`**, run dir
`~/tmp/wt-w16-oc-ab/.ab_measure_runs/20261003-001634-ab5-548696/`. `symbols.txt` is untouched by this branch.

**Prediction, written before the run** (this worktree's own build minus main's expected reading, main =
52,189 + W16-OE's +140 + W16-OD's +365): leg A 52,694 / 5,783,888 B; Δmatched **+230**, Δcode **+45,564 B**.

```
leg A: matched=52694 masked=25107 honest=27587 code%=56.440334  (recompiles: 0, settled)
leg B: matched=52924 masked=25107 honest=27817 code%=56.884956  (recompiles: 312, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+230  Δmasked_equal=+0  Δhonest=+230  Δcode%=+0.444622pp  Δcode_bytes=+45564
Δfuzzy=+0.681442pp   (legA 62.486965 -> legB 63.168407)
unit net (ALL units) = +230   vs whole-binary Δmatched = +230
units at 100% [mpn]: 498 -> 499 (DupSpace/MatchOperation)
```

**Measured exactly as predicted.** Every gained function is honest (`masked_equal` +0). Bytes by unit: the seven TUs
42,028 B, DuplicatedObject +2,960 B (six rows to 100 from the `DOID` change), and W16-OE's MatchOperation +576 B
(`ExecuteOperation` 99.875 → 100. Its frame was 0x10 short on main and the shortfall closes on this branch,
which is consistent with the declined-inline `IsValid` reservation; not isolated). No unit regressed.

Earlier runs of the same branch, kept for the record: on main `b2b2cb8ac` (before W16-OD) +227 / +44,556 B, predicted
exactly, before the W16-OE spelling fixes of §3.3 (+3 fns / +1,008 B); on `36d8bd9f3` +226 / +43,932 B, predicted
exactly, before the patcher fix (the four W16-OD rows).

## 5. Rows that went down (leg A → leg B, keyed by unit and row name)

None went down from 100, so no matched byte was lost.

| row | size | fuzzy | reason |
|---|---:|---|---|
| DuplicatedObject `ExecRemoveFromStore` | 456 | 92.386 → 71.272 | `DOID` without the `RootObject` base lets `/Ob1` expand `GetHandle()`; retail expands it in six DuplicatedObject rows (now at 100) but calls it out of line here |
| DuplicatedObject `SelectNewLocation` | 360 | 94.978 → 68.333 | same |
| DuplicatedObject `FaultRecoveryImpl` | 712 | 88.775 → 86.809 | JobConnectStation's `DORef::GetHandle()` copies the member; its charged instructions fall 110 → 103, but the frame shifts. `GetHandle`'s form is site-dependent (§3.2) |

Three rows are renamed, at their old scores: ObjDupProtocol `Send` (`qResult` is a class: `?AU` → `?AV`, 99.694),
`ProcessJoinRequest` (parameter type `_DS_StationIdentification`, 99.653) and `_DOC_RootDO::DispatchRMCResult`
(returns `bool`, 100).

## 6. Native gate

Run last, on the code at `8c4d1e15d` (only this docs edit follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 7. Not done

- **RefCountedObject** (`0x82AA6918..0x82AA6B98`, 640 B) is its own TU, unpinned and unwritten. So is the TU between
  Session and Station (`0x82A7ADD0..0x82A7B860`: protocol-name strings, a PGV vtable), the Protocol TU before
  RefCountedObject, and the DOFilter subclasses after StationDDL (`0x82A847B8..`).
- **`StepSequenceJob.h` does not match retail** (Step 0x10 bytes at 0x48, start time 0x38, a word at 0x58). It stays
  local to JobConnectStation.cpp and JobJoinSession.cpp, as in JBESL and ObjDupProtocol, and JobConnectStation.cpp
  also keeps a local `JobConnectStation`/`JobChangeConnection`, so those two classes are still defined twice
  (here and in `ObjDup/JobChangeConnection.h`).
- **The `Core::GetInstance` / `GetInstanceFromVector` frame question (§3.4) and `DORef::GetHandle`'s
  site-dependent form (§3.2)** are open; both look like different inline helpers at different retail call sites.
- **Fold/alias charges left for the alias lane:** `0x823EA598` (the 4-byte copy ctor, StationManager ×5, JCS ×1),
  `0x82B35690` (StationDDL `CallSignalAsFaulty`), the shared `list<Message*>` erase/clear copies (JCS ×3), the
  `GetStreamSettingsForContext` anonymous-namespace survivor (Station `FlushBundle`), `SetVolume` as
  `DOHandle(unsigned)` and `reverse_iterator` as the DOHandle copy (Session).
- `/Ob1` inline-budget residue: StationDDL's `UpdateProtocol` ctor declined at its fourth use only; JobJoinSession's
  `~ScopedCS`; Session's `GetNbKeys`/`EndReached`/`_M_empty_initialize`; StationManager's `list::insert`.
- Session's six `map<String,String>` tree rows (1,160 B): nothing attested in the TU instantiates them.
- SystemComponent's `objects.json` entry lacks `/GR-` (no `.text` effect; §2).
- Method names for functions that only exist out of line in this build are lane-chosen and not attested
  (`BeginUse`/`EndUse`, `SendImpl`, `FindIncomingEndPoint`, `PrepareURL`, and others listed per TU).
