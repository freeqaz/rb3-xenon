# W16-NY — the whole DuplicatedObject TU, written from retail asm (2026-10-02)

**Branch** `w16-ny-dupobj` (integration branch) with four sub-lane branches merged into it:
`w16-ny-dupobj-B` (0x82A71548..0x82A72D58), `-C` (0x82A72D58..0x82A74220), `-D` (0x82A74220..0x82A75688),
`-E` (0x82A75688..0x82A76B58). The main lane wrote 0x82A6FC78..0x82A71548 and integrated.
**Ruler** `name_check` (graded), measured per row with `objdiff-cli diff` on a scratch compile and confirmed by full
builds' `report.json`. No `ab_measure`, no native gate (the coordinator runs both). The permuter was not run.
`scripts/symbol_aliases.json` and every alias group are untouched.

## 1. The retail TU and the pin

The unit was pinned from 0x82A70400. The retail TU starts at the constructor, **0x82A6FC78** (0x82A6FC60/68 read a
different global and belong to the previous TU), and runs past the old end 0x82A75F64 through the remaining state
functions and the end-of-TU template/implicit-member instantiations that only this TU references, to **0x82A76B58**
(0x82A76B58 is referenced only from elsewhere and opens a new vtable'd class). `splits.txt` now has one `.text`
block 0x82A6FC78..0x82A76B58; dtk re-derived `.pdata` (0x82250BE8..0x82250EE8) and the re-split is a fixed point.
The two outlying blocks (0x82AB0850.., 0x82AFC2A8..) were left pinned as they were (§5).

## 2. Flags: `/Od /Ob1 /EHs-c-`

The brief said to keep plain `/Od`. Retail's own code rules that out: IsInCachedDuplicationSet (0x82A751F8) expands
`map::end()` and the DOHandle copy inline while calling `find` out of line, and every ScopedCS user expands the
ScopedCS ctor, `CriticalSection`'s `s_bNoOp` gate and `Scheduler::GetInstance()` inline. Plain `/Od` cannot inline
anything — a probe with `__forceinline` at `/Od` still calls the ctor/dtor out of line — so this TU was built with
inlining on. `/Ob1` (inline only what is marked inline) fits every function: `.cpp`-defined members such as
SetFlag/ClearFlag stay out of line even when called from the same TU. No function in the TU has EH state, so
`/EHs-c-`.

Measured on the original source before any body was written: `/Ob1 /EHs-c-` lifted the ctor 50 -> 79,
AcquireMainReference 44 -> 86 and the dtor 72 -> 100, and lost exactly the 108 B the brief predicted (§5).

## 3. Shared-header changes (all re-measured: a full build shows no row outside the unit moving)

- `Platform/CriticalSection.h`: inline `Enter()` / `Leave()` (`if (!MutexPrimitive::s_bNoOp) EnterImpl()`).
  `Platform/ScopedCS.h` calls them through `critSec`, which is the temp shape retail shows (critSec reloaded into a
  temp, then EnterImpl).
- `Core/Scheduler.h`: `GetInstance()` lost a dead ternary (`/Od` emitted both bodies) and reads the scheduler
  through an inline `Core::GetScheduler()`.
- `Core/Core.h`: `Core::GetInstance()` is written with three locals, and (sub-lane C)
  `InstanceTable::GetInstanceFromVector` is `__declspec(noinline)`. `/Ob1` attempts `Core::GetInstance`, rejects
  it, and keeps the rejected attempt's slots as dead frame space; retail leaves exactly 12 dead bytes after
  Scheduler's `inst` in every ScopedCS user. Three locals give 12; with GFV still an inline candidate the nested
  attempt added a variable amount (20 in C's smallest user). Retail never inlines GFV (its `/O1` users call it too).
- `ObjDup/DOHandle.h`: inline copy ctor (retail copies by-value handles into a temp through it), `GetDOClassID`,
  `GetID`, `IsA` (Wii's `DOHandle::IsA`), `==`, `!=`, `GetDOID`.
- `net/QuazalSession.h` now includes the new `Core/NetZ.h` instead of declaring `NetZ` itself.
- New/extended ObjDup headers: `DOClass.h` (virtual order from the Wii build's DOClassTemplate vtable: slot 13
  SpecificUpdate, 14 SpecificRefresh, 15 IsAKindOf — the slots retail calls), `DOOperation.h` (DOOperation is
  Operation's 0x14 bytes plus a DORef at 0x14, from the retail ctor 0x82AF2128; every operation's layout from its
  retail ctor), `DORefTemplate.h`, `SelectionIterator.h`, `Platform/LogicalClock.h`, `Core/OperationManager.h`,
  `DOID.h`, `DOSelections.h`, and others listed per sub-lane in the merge commits.

## 4. Map-name corrections

- 0x82A707E8 was `~ScopedCS`. Every retail caller (this TU, ObjDupProtocol, CheatProvider, five `auto_` units)
  passes the DOHandle class id `(h & 0xFFC00000) >> 22` and dispatches through the RESULT's vtable, so it is
  `static DuplicatedObject::GetDOClass(unsigned int)`. It stays at 100 under the new name; the other callers are in
  unpaired units.
- 0x82A75288 was `InstanceTable::GetInstanceFromVector`; it is `AddToCachedDuplicationSet(const Station*)`.
- 0x82A75458 was `IsInDuplicationSet`; it calls `Selection::Remove` and is `RemoveFromDuplicationSet`. The
  find != end body is at 0x82A75390.

## 5. Rows that go down, with reasons

| row | before | after | reason |
|---|---:|---:|---|
| `_S_right` @0x82AB0850 | 100 (16 B) | unpaired | outside the retail TU span and never called from it; it paired only because plain `/Od` emitted an out-of-line copy |
| `map::end` @0x82AFC2A8 | 100 (52 B) | unpaired | same |
| `fn_82AB0CDC` | 100 (40 B) | 74 | an EH unwind funclet of STLport string/exception code in the 0x82AB0850 block; it paired by funclet byte signature against the old `/EHsc` build's funclets (masked_equal −1) |
| `fn_82AB0D44` | 99.9 | 0 | same |

## 6. Per-function results

Measured on the integrated branch with a forced re-split and a full `./tools/ninja-locked` (rc 0), read from
`report.json` (`name_check`). "Before" is main's `report.json` at the start of the lane (rows outside the old pin read
from their `auto_` units). Unit totals: **8/95 functions, 312/25,056 B -> 72/121, 13,828/29,992 B**. Whole report
against the same main report: `matched_functions` **+64**, `matched_code` **+13,516 B**, `masked_equal_functions`
−1 (`fn_82AB0CDC`, §5), and **no row outside the unit changes** (row-keyed diff over every unit). This is a
whole-report comparison, not an `ab_measure` A/B; main has moved 12 commits since the fork.

Who wrote what: main lane 0x82A6FC78..0x82A71548 (+ SetFlag/ClearFlag, integration); B 0x82A71548..0x82A72D58;
C 0x82A72D58..0x82A74220; D 0x82A74220..0x82A75688; E 0x82A75688..0x82A76B58.

| addr | size | function | fuzzy before | after |
|---|---:|---|---:|---:|
| 82A6FC78 | 372 | DuplicatedObject::ctor | 0 | 100 |
| 82A6FDF0 | 168 | DuplicatedObject::AcquireMainReference | 0 | 100 |
| 82A6FE98 | 72 | DuplicatedObject scalar deleting dtor | 0 | 100 |
| 82A6FEE0 | 92 | DuplicatedObject::dtor | 0 | 100 |
| 82A6FF40 | 60 | DuplicatedObject::SetStationSpecialRelevance | 0 | 100 |
| 82A6FF80 | 152 | DuplicatedObject::IsAKindOf | 0 | 100 |
| 82A70018 | 52 | DuplicatedObject::SetMasterStation | 0 | 100 |
| 82A70050 | 512 | DuplicatedObject::UpdateImpl | 0 | 100 |
| 82A70250 | 404 | DuplicatedObject::RefreshImpl | 0 | 100 |
| 82A703E8 | 20 | DuplicatedObject::SpecificExtractADataset | 0 | 100 |
| 82A70400 | 92 | DuplicatedObject::SpecificRefresh | 0 | 100 |
| 82A70460 | 108 | DuplicatedObject::SpecificUpdate | 0 | 100 |
| 82A704D0 | 84 | DuplicatedObject::CallApproveFaultRecovery | 0 | 100 |
| 82A70528 | 104 | DuplicatedObject::CallApproveEmigration | 0 | 100 |
| 82A70590 | 600 | (unmapped) fn_82A70590 | 0 | 0 |
| 82A707E8 | 40 | DuplicatedObject::GetDOClass | 100 | 100 |
| 82A70810 | 80 | DuplicatedObject::CreateStubMessage | 0 | 100 |
| 82A70860 | 940 | DuplicatedObject::SendStubMessage | 0 | 100 |
| 82A70C10 | 120 | DuplicatedObject::RemoveFromStore | 0 | 100 |
| 82A70C88 | 116 | DuplicatedObject::AddToStoreAsDuplica | 0 | 100 |
| 82A70D00 | 112 | DuplicatedObject::AddToStoreAsMaster | 0 | 100 |
| 82A70D70 | 700 | DuplicatedObject::UndeleteMainRef | 0 | 100 |
| 82A71030 | 668 | DuplicatedObject::ChangeMasterStation | 0 | 87.587 |
| 82A712D0 | 40 | DORef::GetHandle | 0 | 0 |
| 82A712F8 | 384 | DuplicatedObject::UpdateDatasets | 0 | 95.562 |
| 82A71478 | 80 | DuplicatedObject::GetCurrentOperation | 0 | 99.7 |
| 82A714C8 | 124 | DuplicatedObject::GetOperationManager | 0 | 99.677 |
| 82A71548 | 1100 | DuplicatedObject::ExecuteOperation | 0 | 99.985 |
| 82A71998 | 228 | DuplicatedObject::PerformOperation | 0 | 85.263 |
| 82A71A80 | 456 | DuplicatedObject::ExecRemoveFromStore | 0 | 71.272 |
| 82A71C48 | 276 | DuplicatedObject::ReleaseReference | 0 | 100 |
| 82A71D60 | 664 | DuplicatedObject::ExecAddToStore | 0 | 83.861 |
| 82A71FF8 | 132 | DuplicatedObject::Refresh | 0 | 100 |
| 82A72080 | 1684 | DuplicatedObject::ExecChangeMasterStation | 0 | 84.354 |
| 82A72718 | 884 | DuplicatedObject::ExecChangeDupSet | 0 | 49.855 |
| 82A72A90 | 712 | DuplicatedObject::FaultRecoveryImpl | 0 | 88.775 |
| 82A72D58 | 96 | DuplicatedObject::DispatchRMCCall | 0 | 100 |
| 82A72DB8 | 232 | DuplicatedObject::ExecUpdateDataSet | 0 | 100 |
| 82A72EA0 | 80 | DuplicatedObject::SendConnectOrphanRequest | 0 | 100 |
| 82A72EF0 | 224 | DuplicatedObject::PerformFaultRecovery | 0 | 100 |
| 82A72FD0 | 64 | DuplicatedObject::SendToAllDuplicas | 0 | 100 |
| 82A73010 | 280 | DuplicatedObject::SendToSomeDuplicas | 0 | 100 |
| 82A73128 | 108 | BundlingPolicy::GetInstance | 0 | 100 |
| 82A73198 | 144 | DuplicatedObject::IsGlobal | 0 | 100 |
| 82A73228 | 208 | DuplicatedObject::EmigrateTo | 0 | 100 |
| 82A732F8 | 212 | DuplicatedObject::MigrationInProgress | 0 | 100 |
| 82A733D0 | 168 | DuplicatedObject::AttemptEmigration | 0 | 99.881 |
| 82A73478 | 380 | DuplicatedObject::PrepareToLeave | 0 | 99.779 |
| 82A735F8 | 360 | DuplicatedObject::SelectNewLocation | 0 | 62.089 |
| 82A73760 | 132 | DuplicatedObject::IsADuplica | 0 | 100 |
| 82A737E8 | 248 | DuplicatedObject::IsADuplicationMaster | 0 | 100 |
| 82A738E0 | 44 | DuplicatedObject::IsAWellKnownDO | 81.364 | 100 |
| 82A73910 | 72 | DuplicatedObject::GetMasterID | 0 | 100 |
| 82A73958 | 212 | DuplicatedObject::ReleaseMainReference | 0 | 100 |
| 82A73A30 | 220 | DuplicatedObject::DecreaseRefCount | 0 | 100 |
| 82A73B10 | 128 | DuplicatedObject::CompleteDecreaseRefCount | 0 | 100 |
| 82A73B90 | 40 | DuplicatedObject::SetFlag | 0 | 100 |
| 82A73BB8 | 44 | DuplicatedObject::ClearFlag | 0 | 100 |
| 82A73BE8 | 332 | DuplicatedObject::DeleteMainRef | 0 | 100 |
| 82A73D38 | 220 | DuplicatedObject::DeleteDuplicaMainRef | 0 | 100 |
| 82A73E18 | 276 | DuplicatedObject::DeleteMainRefImpl | 0 | 100 |
| 82A73F30 | 748 | DuplicatedObject::ConnectOrphanDuplica | 0 | 93.064 |
| 82A74220 | 1156 | DuplicatedObject::Publish | 0 | 100 |
| 82A746A8 | 172 | DuplicatedObject::FillDuplicaStationsList | 0 | 82.442 |
| 82A74758 | 464 | DuplicatedObject::CreateWellKnown | 0 | 100 |
| 82A74928 | 276 | DuplicatedObject::Create | 0 | 100 |
| 82A74A40 | 112 | DuplicatedObject::Create | 0 | 100 |
| 82A74AB0 | 564 | DuplicatedObject::CreateMasterImpl | 0 | 99.631 |
| 82A74CE8 | 244 | DuplicatedObject::CreateDuplica | 0 | 100 |
| 82A74DE0 | 356 | DuplicatedObject::ValidOperation | 0 | 97.348 |
| 82A74F48 | 20 | DuplicatedObject::ComputeDistance | 100 | 100 |
| 82A74F60 | 44 | DuplicatedObject::ReleaseReferenceToMaster | 100 | 100 |
| 82A74F90 | 80 | DuplicatedObject::AcquireReferenceToMaster | 100 | 100 |
| 82A74FE0 | 536 | DuplicatedObject::IsDuplicatedOn | 0 | 100 |
| 82A751F8 | 144 | DuplicatedObject::IsInCachedDuplicationSet | 0 | 100 |
| 82A75288 | 172 | DuplicatedObject::AddToCachedDuplicationSet | 31.442 | 100 |
| 82A75338 | 84 | DuplicatedObject::RemoveFromCachedDuplicationSet | 0 | 100 |
| 82A75390 | 144 | DuplicatedObject::IsInDuplicationSet | 0 | 100 |
| 82A75420 | 52 | DuplicatedObject::AddToDuplicationSet | 0 | 100 |
| 82A75458 | 84 | DuplicatedObject::RemoveFromDuplicationSet | 1.238 | 100 |
| 82A754B0 | 368 | DuplicatedObject::RemoveAllDuplicasOnLeavingStation | 0 | 92.609 |
| 82A75620 | 100 | DuplicatedObject::IsASettledMaster | 0 | 100 |
| 82A75688 | 28 | DuplicatedObject::SetInitialState | 10.571 | 100 |
| 82A756A8 | 20 | DuplicatedObject::InvalidState | 100 | 100 |
| 82A756C0 | 272 | DuplicatedObject::ValidState | 59.603 | 100 |
| 82A757D0 | 512 | DuplicatedObject::InitialState | 0 | 94.367 |
| 82A759D0 | 428 | DuplicatedObject::DuplicationMasterState | 0 | 92.71 |
| 82A75B80 | 208 | DuplicatedObject::UnpublishedMasterState | 0 | 100 |
| 82A75C50 | 168 | DuplicatedObject::UnidentifiedMasterState | 0 | 100 |
| 82A75CF8 | 496 | DuplicatedObject::InStoreMasterState | 0 | 94.137 |
| 82A75EE8 | 124 | DuplicatedObject::DeletedMasterState | 0 | 100 |
| 82A75F68 | 352 | DuplicatedObject::DuplicaState | 0 | 91.136 |
| 82A760C8 | 480 | DuplicatedObject::InStoreDuplicaState | 0 | 94 |
| 82A762A8 | 456 | DuplicatedObject::OrphanDuplicaState | 0 | 93.158 |
| 82A76470 | 20 | DuplicatedObject::ConnectedDuplicaState | 0 | 100 |
| 82A76488 | 192 | DuplicatedObject::DeletedDuplicaState | 0 | 100 |
| 82A76548 | 32 | LogicalClockTmpl<uchar>::ctor | 0 | 0 |
| 82A76568 | 212 | DORefTemplate<DuplicatedObject>::IsValid | 0 | 100 |
| 82A76640 | 224 | DORefTemplate<Station>::IsValid | 0 | 100 |
| 82A76720 | 224 | DORefTemplate<Session>::IsValid | 0 | 100 |
| 82A76800 | 92 | (unmapped) fn_82A76800 | 0 | 0 |
| 82A76860 | 68 | (unmapped) fn_82A76860 | 0 | 0 |
| 82A768A8 | 28 | LogicalClockTmpl<uchar>::ctor | 0 | 0 |
| 82A768C8 | 168 | LogicalClockTmpl<uchar>::operator>= | 0 | 99.667 |
| 82A76970 | 152 | (unmapped) fn_82A76970 | 0 | 0 |
| 82A76A08 | 164 | LogicalClockTmpl::Compare | 0 | 93.659 |
| 82A76AB0 | 168 | (unmapped) fn_82A76AB0 | 0 | 0 |
| 82AB0850 | 16 | _Rb_tree<DOHandle>::_S_right | 100 | 0 |
| 82AB0860 | 132 | (unmapped) fn_82AB0860 | 0 | 0 |
| 82AB08E8 | 100 | (unmapped) fn_82AB08E8 | 0 | 0 |
| 82AB0950 | 140 | (unmapped) fn_82AB0950 | 0 | 0 |
| 82AB09E0 | 156 | (unmapped) fn_82AB09E0 | 0 | 0 |
| 82AB0A80 | 172 | (unmapped) fn_82AB0A80 | 0 | 0 |
| 82AB0B30 | 144 | (unmapped) fn_82AB0B30 | 0 | 0 |
| 82AB0BC0 | 148 | (unmapped) fn_82AB0BC0 | 0 | 0 |
| 82AB0C60 | 124 | (unmapped) fn_82AB0C60 | 0 | 0 |
| 82AB0CDC | 40 | (unmapped) fn_82AB0CDC | 100 | 74 |
| 82AB0D10 | 52 | (unmapped) fn_82AB0D10 | 0 | 0 |
| 82AB0D44 | 44 | (unmapped) fn_82AB0D44 | 99.909 | 0 |
| 82AB0D70 | 508 | (unmapped) fn_82AB0D70 | 0 | 0 |
| 82AFC2A8 | 52 | map<DOHandle>::end | 100 | 0 |

Unmapped and unwritten: Trace (0x82A70590, §8), the SelectionIteratorTemplate<RootDO> ctor/InitFilter and
<Station> ctor instantiations (0x82A76800, 0x82A76860, 0x82A76970; nothing defines them yet), 0x82A76AB0 (an
`_Rb_tree` increment body whose name is already mapped at 0x822717C0), and the 0x82AB0850 block's STLport
string/exception code. `DORef::GetHandle` (0x82A712D0) and the two LogicalClockTmpl ctors (0x82A76548, 0x82A768A8)
are mapped but our build inlines every call, so nothing pairs with them.

## 7. Why rows stay below 100

- **Fold-survivor callees.** Several functions call ICF fold survivors whose map names no source here can spell:
  0x823EA598 `??0?$reverse_iterator<Synchronizable**>` (a one-word copy: the DOHandle copy retail calls out of
  line), 0x82A478D0 `?SetVolume@DirectInstrument@@` (`DOHandle(unsigned)`), 0x82A6DE40
  (`RootObject::operator new(size_t, const char*, unsigned int)`, being renamed on the integrated branch by the
  coordinator). `name_check` charges those call sites whatever the source does.
- **`/Ob1` call-site choices.** Retail keeps some inlines out of line at some call sites and expands them at others
  in the same function (`DORef::GetHandle`, the DOHandle copy, `DOHandle(unsigned)`); ours expands all of them.
  The pattern is consistent with the inliner attempting and rejecting a call (its `this` temp is materialised before
  the out-of-line call), but no source spelling tried reproduces the rejection.
- **Inline jump tables** (PerformOperation 0x82A71998): the 14 inline `/Od` jump-table words are raw words in the
  dtk target object and relocated in ours, so objdiff charges every word although the code is byte-identical.

## 8. Not done

- Not merged to main. The branch carries merge commits from the four sub-lanes, so it was not rebased; a test merge
  against current main conflicts only in `scripts/target_symbol_map.json` (both sides append at the tail).
- LogicalClockTmpl's ctor takes `unsigned int` (sub-lane D: its call sites truncate the literal at run time), but
  retail's out-of-line ctor at 0x82A768A8 homes its argument with `stb`, i.e. takes `unsigned char`; the map keeps
  that row's retail-true name. The row is unemitted either way.
- `Station::GetLocalStation()` (main lane) and `Station::GetLocalStationHandle()` (C) both name the 0x82A7CB30
  accessor; both are unmapped placeholders, left as two declarations.

- Trace (0x82A70590, 600 B): unwritten. Its cached-set loop calls the 0x823EA598 fold survivor, so it is capped,
  and it needs the SelectionIterator/String output API.
- The 0x82AB0850 block holds STLport string/exception COMDATs from some EH-enabled `/Od` TU, not
  DuplicatedObject code; its pin was left alone (re-homing is not neutral).
