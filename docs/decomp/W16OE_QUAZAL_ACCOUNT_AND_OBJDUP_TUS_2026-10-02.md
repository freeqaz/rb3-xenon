# W16-OE — nine Quazal `/Od` TUs (account jobs, tickets, ObjDup core) written from the retail asm (2026-10-02)

**Branch** `w16-oe`, rebased onto main `1505d4c74`. **Ruler** `name_check` (graded). Permuter not run. No
`scripts/symbol_aliases.json` entry and no `symbols.txt` line touched.

Method and TU table: `docs/decomp/W16NY_OD_BLOCK_PRICING_AND_LARGEST_TUS_2026-10-02.md` (rows 6, 12, 13, 19, 20,
32, 34, 35, 37). Lanes W16-OC (session/station) and W16-OD (transport) worked other TUs of the same block in
parallel.

## 1. How the work was split

The nine TUs lie in five address regions. I fixed the outer bound of each region from W16-NY's anchor table
(`~/tmp/w16ny/tus.json`), and gave TUs that share a region to the same sub-lane, so that no two lanes argued over a
boundary. Each sub-lane worked in its own `setup_worktree.sh` worktree on a branch off `w16-oe`. I merged each one
with `--no-ff` as it finished, and the branch keeps every sub-lane commit.

| sub-lane | TUs | region given | branch |
|---|---|---|---|
| C | JobLoginOrCreateAccount, JobCreateAccount | `0x82B0A738..0x82B14D48` | `w16-oe-c` |
| D | TicketManager, MatchOperation | `0x82B3DD60..0x82B42E30`, `0x82B47BC8..0x82B4EDE8` | `w16-oe-d` |
| B2 | ChangeDupSetOperation, DOCore | `0x82ABD4B8..0x82AC2820` | `w16-oe-b2` |
| A | RootDODDL, DOCallContext | `0x82A987D0..0x82AA1658` | `w16-oe-a` |
| B1 | CallRegister | `0x82AB9508..0x82ABDC98` | `w16-oe-b1` |

Source order and most method names are attested names. No source for
any of these TUs exists in either sibling repo. Every TU builds `/Od /Oi- /EHs-c- /Ob1`; the two account jobs add
`/GR-`, and so does CallRegister. In each case the flag was read from the TU's asm: no EH prefixes or funclets,
and no RTTI locator before the vtable.

## 2. Retail extents

Each scaffold pin was an under-carve, as W16-NY §1.5 predicted. All nine retail extents fall inside W16-NY's
[lo, hi] bracket, which brings that bracket to 18 of 18 extents measured.

| TU | old pin | retail extent | bytes | edge evidence |
|---|---|---|---:|---|
| JobLoginOrCreateAccount | `82B0E468..82B0E914` | `82B0E148..82B0EA88` | 2,368 | ctor stores vtable `0x82188C14`, the TU's first `.rdata` object; ends before `GuestCustomCreateAccountCommand`'s ctor (vtable `0x82188D28`) |
| JobCreateAccount | `82B13598..82B14948` | `82B13598..82B14AA0` | 5,384 | ctor follows JobManageAccount's code; ends before an `AccountManagementCommand` subclass ctor (vtable `0x821890E0`) |
| TicketManager | `82B3F530..82B3F958` | `82B3F4C8..82B3FBF0` | 1,832 | previous class's vtable `0x8218EAE0` precedes TicketManager's file string; ends at `clear`, before `_M_erase` at `0x82B3FBF0`, which stays pinned with BandwidthCounter |
| MatchOperation | `82B48768..82B48824` | `82B481D0..82B48824` | 1,620 | ctor is the first store of vtable `0x8218F118`; `0x82B48828` is another class (vtable `0x8218F150`, called only from DupSpace code) |
| ChangeDupSetOperation | `82ABDE90..82ABDF78` | `82ABDC98..82ABDF78` | 736 | `.rdata` (vtable `0x82181724`, `"ChangeDupSet"`, file string) ends where FaultProcessingContext's begins |
| DOCore | `82AC0470..82AC0C78` | `82AC0470..82AC1808`, plus `??__E` `82C420A0..82C420E0` and `??__F` `82C4AEF0..82C4AF18` | 5,084 (rows) | ends at HasStartedTermination; `0x82AC1B70` is SessionDiscoveryTable's ctor, and `0x82AC1808..0x82AC1B68` is called only from other TUs |
| RootDODDL | `82A99368..82A997F4` | `82A99368..82A99C48` | 2,272 | starts at `_DOC_RootDO::Create`; ends where `DOClass::DOClass` stores vtable `0x8217EFE8`, which sits right before the `.\DOClass.cpp` string |
| DOCallContext | `82AA0E48..82AA15E8` | `82AA06E0..82AA1650` | 3,952 | ctor follows DDLDeclarations' static initialisers; ends at StationURL's 8-byte EH prefix |
| CallRegister | `82ABAC98..82ABB42C` | `82ABAB58..82ABD488` | 10,544 | ctor stores vtable `0x82181630`, the first entry after StationManager's path string; ends at `_M_create_node` |

Code inside the regions that belongs to none of these nine TUs was left unpinned:
- `0x82B0EA88..0x82B13598`: GuestCustomCreateAccountCommand, SandboxConnectionInfo helpers, and JobManageAccount.
- `0x82B14AA0..0x82B14D48`.
- `0x82ABD488..0x82ABDC98`: 11 functions, 2,064 B of DOSelections/DOFilter code, called from DOCore and DuplicatedObject but never from CallRegister.
- `0x82ABDF78..0x82AC0470`: FaultProcessingContext, FetchContext, FaultRecovery, MigrationContext and CreateMaster.
- `0x82AC1808..0x82AC2820`.
- `0x82A99118..0x82A99368`: VirtualRootObject allocation operators.

`0x82A99C48..0x82A9A220`, which retail evidence puts at the start of DOClass, is still `auto_*`. The DOClass,
DOCoreTypes and RMCContext pins were not moved.

## 3. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-oe-ab --patch ~/tmp/w16oe/ab.patch`. The worktree was
fresh from `setup_worktree.sh` at main `1505d4c74`, and the patch is the whole branch diff (`git diff 1505d4c74
w16-oe`; kinds configgen, map, source, splits). Run dir:
`~/tmp/wt-w16-oe-ab/.ab_measure_runs/20261002-233620-ab-133674/`.

**Prediction, written before the run:** the sum of the five sub-lanes' own readings, each taken on its own base:
C +23 / +7,708, D +15 / +2,700, B2 +28 / +3,756, A +43 / +5,832, B1 +31 / +7,868. That gives **Δmatched +140,
Δmatched_code +27,864 B, Δmasked_equal 0**, with totals unchanged.

```
leg A: matched=52189 masked=24904 honest=27285 code%=55.782787  (recompiles: 0, settled)
leg B: matched=52329 masked=24904 honest=27425 code%=56.054688  (recompiles: 296, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+140  Δmasked_equal=+0  Δhonest=+140  Δcode%=+0.271901pp  Δcode_bytes=+27864
Δfuzzy=+0.325550pp   (legA 61.627780 -> legB 61.953330)
unit net (ALL units) = +140   vs whole-binary Δmatched = +140
units at 100% [mpn ruler]: legA 491 -> legB 495  (Δ+4; 4 reached 100, 0 fell off)
```

**Measured exactly as predicted.** The lanes' gains add without interaction. An intermediate integration build of
C + D + B2 had already read +66 / +14,164 B, also exactly the sum of those three lanes. `total_functions`
(68,914) and `total_code` (10,247,792) do not move: the new pins only move rows out of `auto_*` units. Because
`masked_equal` is +0, the whole gain is honest. These TUs have no EH funclets to pair by byte signature.

Units that reach 100%: JobCreateAccount, JobLoginOrCreateAccount, ChangeDupSetOperation, RootDODDL.

### 3.1 Rows that went down

**None.** Row comparison of the two archived leg reports, keyed by row name:
- 0 rows down; 153 rows vanish and 153 appear (34,096 B each way, 0 resized).
- Every vanished row except three was a fuzzy-0 `fn_` placeholder now named in its unit.
- The other three are ObjDupProtocol rows that B1 respelled from `CallContextRegister` to `CallRegister`/`DOCallContext` (§5). They read 99.868 / 100 / 100 under both names.
- **One row outside the nine TUs goes up**, `ObjDupProtocol::ProcessRMCResponse`, 99.667 → 100, from lane A's callee corrections.

## 4. Per-TU results (leg B `report.json`, graded)

| TU | main: rows at 100 / rows | main: matched / unit B | branch: rows at 100 / rows | branch: matched / unit B |
|---|---:|---:|---:|---:|
| CallRegister | 0 / 8 | 0 / 1,924 | 31 / 39 | 7,868 / 10,464 |
| JobCreateAccount | 0 / 11 | 0 / 5,024 | 13 / 13 | 5,360 / 5,360 |
| DOCallContext | 0 / 10 | 0 / 1,928 | 19 / 20 | 3,468 / 3,912 |
| DOCore | 0 / 1 | 0 / 2,056 | 20 / 21 | 3,028 / 5,084 |
| JobLoginOrCreateAccount | 0 / 3 | 0 / 1,192 | 10 / 10 | 2,348 / 2,348 |
| RootDODDL | 0 / 14 | 0 / 1,156 | 23 / 23 | 2,244 / 2,244 |
| TicketManager | 0 / 2 | 0 / 1,060 | 7 / 9 | 1,664 / 1,816 |
| MatchOperation | 0 / 1 | 0 / 188 | 8 / 9 | 1,036 / 1,612 |
| ChangeDupSetOperation | 0 / 1 | 0 / 232 | 8 / 8 | 728 / 728 |
| **total** | | **0** | **139 / 152** | **27,744** |

Plus ObjDupProtocol's `ProcessRMCResponse` (+120 B), which gives the measured +27,864 B.

### 4.1 The 13 rows below 100

| row | B | fuzzy | why |
|---|---:|---:|---|
| `DOCore::DOCore` | 2,056 | 99.981 | Its only charges are calls to `0x823EBC90` and `0x823E2480`, which the map names after functions in other TUs' anonymous namespaces (fold survivors). This is an alias-lane question. |
| CallRegister `GetFetchContext`, `MigrationInProgress` | 608, 552 | 95.98, 94.75 | They call `0x823EA598`, the out-of-line `DOHandle` copy ctor, which ICF folded under `reverse_iterator<Synchronizable**>` (W16-NY §6). Alias-lane question. |
| CallRegister `Register`, `Trace`, `CancelCallToStation`, `CancelPendingCalls`, `CancelExpiredCalls`, `SignalRelevantFetchContextes` | 1,436 | 99.78–99.97 | The code matches; only frame size differs. Declined inline helpers reserve 4–16 B more or less than retail. B1 found that the helpers' spelling (if/else instead of `?:`, `p == 0` instead of `!p`) moves callers' frames without changing the helpers' own code. That is how `Unregister` reached 100; the rest is open. |
| `DOCallContext::Wait` | 444 | 99.955 | Retail reserves 8 more bytes for the Scheduler lookup. Of ten `Core`/`Scheduler::GetInstance` shapes, the one that fixes `Wait` breaks `InternalCancel`, and the reverse. This is the same "no single shape" problem as W16-NY §3.3. |
| `MatchOperation::ExecuteOperation` | 576 | 99.875 | Retail's frame is 0x10 larger: a 12-byte reservation sits between the handle temps and the lock-release temp. Any declined inline call next to `oRef.Get()` reproduces it, but that would add a call retail does not have, so none was invented. |
| TicketManager `fn_82B3F508` (dtor), `fn_82B3FB80` (`clear`) | 40, 112 | 0 | Deliberately left unnamed. Both are ICF survivors that 12 and 16+ outside call sites land on. Naming them dropped four PRUDPStream funclets 100 → 99.5, the PRUDPStream dtor and a Voice funclet. Scratch builds read them at 100 and 99.82 when named. They need alias entries. |

## 5. Shared files changed, and why

Headers:
- **`src/network/Core/CallContext.h`** (D, additive only):
  - `bool InitiateCall()` declares `0x82A8B9E0`.
  - Inline `GetID()`: retail evaluates the job ctor's first argument into a temp.
  - Inline `SetTimeout(Time)`, taking the value by copy: this makes AcquireTicket's second `Time` temp appear.
  - Without the two inlines, Login and AcquireTicket stay at about 96.9 and 98.3.
- **`src/network/ObjDup/DOHandle.h`** (D): a user-declared inline `operator=`. Retail's assignments go through an inline with `this` stored in a temp, both in the MatchOperation ctor and in `DORef(DOHandle)` at `0x82A80540`. B1 needed the same operator for `QueueCancelCallToStation`, and A's local copy agreed with it. No other row moved.
- **`src/network/ObjDup/CallRegister.h`** (B1):
  - `static CallRegister *GetInstance()` → `static CallRegister &GetInstanceRef()`. `0x82ABAC68` is `CallRegister::GetInstanceRef`.
  - Declares `SignalRelevantFetchContextes` (`0x82ABB700`) and `MigrationInProgress` (`0x82ABBA80`).
- **`src/network/ObjDup/DOCallContext.h`** (B1): an `_Outcome` enum, for that signature.
- **`src/network/ObjDup/RootDODDL.h`** (A): declares `RemoveFromCachedDuplicationSet_OnDuplicas`.
- **`src/network/ObjDup/DOCore.h`** (B2, new): the DOCore declaration for callers. Only DuplicatedObject.cpp includes it.

Call sites in other TUs, where a callee name stated the wrong function. Each change keeps one spelling per retail
function:
- **DuplicatedObject.cpp:**
  - Two `OperationErrorNotifier::GetInstance()->NotifyError(h, code)` calls are retail's `CallRegister::GetInstanceRef().SignalRelevantFetchContextes(h, code)`.
  - `MigrationInProgress` uses `GetInstanceRef()`.
  - `ValidOperation`'s `DOSelections::GetCurrentInstance()->IsAvailable()` is `DOCore::...->HasStartedTermination()` (`0x82AC1790`).
  - `ForgetDuplicaOn` is `_DO_RootDO::RemoveFromCachedDuplicationSet_OnDuplicas`, which `ExecChangeDupSet` calls.
- **ObjDupProtocol.cpp:**
  - Its local `Session` class is DOCore: `IsTerminating` → `IsTerminated` (`0x82AC1720`) and `IsJoining` → `HasStartedTermination`.
  - Its local `CallContextRegister` is CallRegister, since its ctor calls `0x82ABAB58`; `CancelAll` → `CancelPendingCalls`.
  - `OutcomeToString`, `CallContext::SetOutcome` and `SetResponseMessage` are `DOCallContext::GetOutcomeString`, `SignalOutcome` and `SignalResponse`.
- **Map:** 3 existing entries respelled to match (`0x82A97A60`, `0x82A97B70`, `0x82A97E00`). Their scores are unchanged.
- **Measured effect:** all of these together moved one row, `ProcessRMCResponse` +120 B. Before the respellings, naming the new TUs had dropped six ObjDupProtocol/DuplicatedObject rows; the lanes measured that and restored them.

The now-unused declarations `OperationErrorNotifier` (`StationConnections.h`), `DOSelections::GetCurrentInstance`
/ `IsAvailable` (`DOSelections.h`) and `DuplicatedObject::ForgetDuplicaOn` were left in place, to keep the
shared-header diff minimal.

Not touched: `qMemAllocator.h`, `ScopedCS.h`, `CriticalSection.h`, `Core.h`, `Scheduler.h`, `RootObject.h`,
`InstanceTable.h`, `String.h`. TUs that need a different shape declare it locally.

## 6. One finding for the open `InstanceTable` question (W16-NY §6)

Two sub-lanes found, independently, that retail's `GetInstanceFromVector` is a plain inline that `/Ob1` declines,
while the shared `Core/InstanceTable.h` marks it `__declspec(noinline)`:
- **DOCore** (`SetToCorruptedState`) needs the plain inline to keep its three argument slots: 100 with a local
  declaration, 99.769 with the shared one.
- **JobLoginOrCreateAccount/JobCreateAccount** (`CompleteJob`) need it to reproduce a 0x18-byte frame gap.
- Lane C suggests the same mechanism explains JobBackEndServicesLogin's 12-byte `Complete` gap (99.73). That is untested.

B2 tried removing `noinline` from the shared header and reverted the change. The trade between DuplicatedObject
and ObjDupProtocol in W16-NY §4.4 (E2) still stands, so the shared header is unchanged. These TUs use local
declarations.

## 7. Names

The method names are attested names wherever one exists. This lane chose the following names,
and nothing else attests them:
- Account jobs: `JobCreateAccount::CompleteJob(qResult)` and the `SandboxConnectionInfo` copy ctor.
- DOCore: `IsTerminating`.
- RootDODDL / DOCallContext: the RootDODDL Callee stubs, DOCallContext's vtable slot 9, and the file-local helpers `DOHandleValue` and `IsAKindOfDuplicatedObject`.
- CallRegister: `GenerateCallID`, `CancelExpiredCalls`, `CallContext::HasTimedOut`, the `ItemRegister<DOCallContext>` and Iterator member names, `SignalFault`, and `GetObjectHandle`.

## 8. Not done

- **Alias entries** for:
  - `0x823EBC90` and `0x823E2480` (DOCore ctor);
  - `0x823EA598` (two CallRegister rows);
  - TicketManager's `0x82B3F508` and `0x82B3FB80`;
  - `0x82B3FBF0` (`_M_erase`, spelled with the BandwidthCounter name).
- The frame-reservation residue in §4.1, and the third `Core`/`Scheduler` shape.
- Pinning the neighbours found on the way: DOClass's head at `0x82A99C48`, the VirtualRootObject operators, `0x82ABD488..0x82ABDC98`, the operation classes between ChangeDupSetOperation and DOCore, GuestCustomCreateAccountCommand, JobManageAccount, and the `0x82B14AA0` command class.

## 9. Native gate

Run last, on the code at `75a9b6072` (only this docs edit follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Scratch: `~/tmp/w16oe/` (`region.py`, `ab.patch`, `legA.json`, `legB.json`, `report_main.json`,
`report_int1.json`), sub-lane scratch in `~/tmp/w16oec/`, `~/tmp/w16oed/`, `~/tmp/w16oe_b2/`, build logs
`~/tmp/rb3_build_w16oe*.log`.
