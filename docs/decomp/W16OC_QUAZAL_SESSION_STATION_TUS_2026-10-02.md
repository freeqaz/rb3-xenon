# W16-OC — the Quazal session/station TUs written from the retail asm (2026-10-02)

**Branch** `w16-oc`, on main `1505d4c74`. **Ruler** `name_check` (graded). Permuter not run. Method and TU table:
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
| SystemComponent | `82AA7610..82AA78E4` | `82AA6B98..82AA78F0` | 3,416 | starts at the ctor (stores vtable `8217FC38`); `82AA6918..82AA6B98` is RefCountedObject (vtable `8217FC28`, its own TU on Wii); ends at Buffer's ctor (vtable `8217FE10`, EH frame) |

## 2. Per-TU results

### 2.1 SystemComponent (coordinator)

`/Od /Oi- /Ob1 /EHs-c-`. 16 rows, 14 at fuzzy 100; unit 2,028 of 3,368 B. Wii names and order hold except that
two Use helpers Wii inlined into `Use::Use`/`~Use` are out of line on X360 (`0x82AA6D78`, `0x82AA6E08`; named
`BeginUse`/`EndUse` here, not attested). Whole build in the lane worktree: 52,189 → 52,203 fns, 5,716,504 →
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
  `Core::GetInstance` and the current header reserves 12 B. See §3.2.

## 3. Shared headers changed

### 3.1 By the coordinator (SystemComponent)

- **`Core/Scheduler.h`.** `unk90` was `qMap<Time, Job *>` (0x1c: `qMap` adds `RootObject` as a second, empty base,
  which MSVC does not collapse, and StationURL's retail `qMap`s really are 0x1c apart). Retail's Scheduler ctor
  (`0x82AC57F0`) builds a bare tree at `0x94` and stores `unka8` at `0xac`, the WaterMarks at `0xb0`/`0xe0`,
  ProfilingUnits at `0x110`/`0x180`, `SingleThreadCallPolicy` at `0x16c`, and the last bool at `0x1d9`. With a
  0x18 `std::multimap` every one of those offsets matches the compiler's layout report (`sizeof` 0x1e8 → 0x1e0);
  before it, every member after `0x94` was 4–8 B high. Only `Scheduler.cpp`'s unmatched ctor names those members.
  map vs multimap cannot be told from the ctor. Also declares `GlobalSingleThreadDispatch(unsigned)` (Wii-attested).
- **`Core/SystemComponent.h`.** Adds `BeginUse`/`EndUse` and the inline `GetState()`/`GetUseCount()` accessors
  whose temps the retail frames show. No layout or vtable change.

### 3.2 A probe of the open `Core`/`Scheduler` shape (W16-NY §3.3), not taken

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

## 5. Rows that went down

## 6. Native gate

## 7. Not done
