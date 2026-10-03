# W16-OG — the Quazal core TUs written from the retail asm (2026-10-03)

**Branch** `w16-og`, rebased onto main `7431336d6` (after W16-OC landed). **Ruler** `name_check` (graded). Permuter not
run. No alias group and no `scripts/symbol_aliases.json` entry touched. `symbols.txt` is unchanged.

This lane applies W16-NY's method (`docs/decomp/W16NY_OD_BLOCK_PRICING_AND_LARGEST_TUS_2026-10-02.md`) to seven
scaffold TUs from its table: StringStream, ObjectThreadRoot, String, EventHandler, MutexPrimitive, WorkerThreads and
StepSequenceJob. It also covers the two unpinned neighbours W16-OD found, StationContactInfo
(`0x82B2F110..0x82B2FC38`) and IOCompletionNotifier's tail (`0x82B386B8..0x82B39320`). It then settles the
`Core::GetInstance` / `Scheduler::GetInstance` shape question (W16-NY §3.3, W16-OD §9).

The work was split into seven sub-lanes, one worktree each, as W16-OD did:

| sub-lane | TUs |
|---|---|
| `w16-og-a` | MutexPrimitive |
| `w16-og-b` | String |
| `w16-og-c` | ObjectThreadRoot, StringStream |
| `w16-og-d` | EventHandler, StepSequenceJob |
| `w16-og-e` | WorkerThreads |
| `w16-og-f` | StationContactInfo, IOCompletionNotifier, plus Inet |
| `w16-og-g` | the Core/Scheduler shape probe |

They were merged into `w16-og` with `--no-ff`, reconciled (§4), and then rebased onto `7431336d6` with
`git rebase -r`.

## 1. Result

`ab_measure --patch` (§5), main `7431336d6` → this branch: **+164 functions, +18 masked_equal, +146 honest,
+21,892 B** (+0.213631 pp). That matches the prediction exactly. Two existing rows went down, both from one folded
template's name (§5.1). Five units reach 100% on `mpn`: MutexPrimitive, StringStream, Inet, StepSequenceJob and
WorkerThreads. Four of them, all but StepSequenceJob, also reach 100% on all-rows-fuzzy.

| TU | flags | main: rows at 100 / rows / unit B | branch: rows at 100 / rows | branch: matched / unit B |
|---|---|---|---:|---:|
| String | `/Od /Oi- /Ob1 /EHs-c-` | 0 / 20 / 2,360 | 32 / 35 | 3,756 / 4,076 |
| ObjectThreadRoot | `/Od /Oi- /Ob1 /GR- /EHs-c-` | 0 / 1 / 200 | 32 / 34 | 3,868 / 4,076 |
| IOCompletionNotifier | `/Od /Oi- /Ob1 /GR- /EHs-c-` | 0 / 3 / 716 | 8 / 9 | 3,692 / 3,808 |
| StringStream | `/Od /Oi- /Ob1 /GR- /EHs-c-` | 0 / 1 / 160 | **26 / 26** | 2,264 / 2,264 |
| WorkerThreads | `/Od /Oi- /Ob1 /GR- /EHs-c-` | 0 / 1 / 604 | **8 / 8** | 1,784 / 1,784 |
| EventHandler | `/Od /Oi- /Ob1 /EHs-c-` | 0 / 3 / 1,020 | 8 / 11 | 1,584 / 2,604 |
| StepSequenceJob | `/Od /Oi- /Ob1 /GR- /EHs-c-` | 0 / 1 / 220 | 11 / 12 (12/12 mpn) | 1,224 / 1,492 |
| StationContactInfo (new unit) | `/Od /Oi- /Ob1 /GR-` | `auto_*` | 8 / 23 | 708 / 2,808 |
| MutexPrimitive | `/Od /Oi- /Ob1 /EHs-c-` | 0 / 1 / 72 | **6 / 6** | 376 / 376 |
| Inet (new unit) | `/Od /Oi- /Ob1 /GR-` | `auto_*` | **2 / 2** | 76 / 76 |
| PRUDPStream (shape, §3) | unchanged | 87 / 147 | 93 / 147 | 11,756 / 18,068 (+1,944) |
| UDPTransport (shape, §3) | unchanged | 88 / 152 | 90 / 152 | 11,772 / 19,348 (+656) |

Row counts include EH funclets.

## 2. Extents

Each extent comes from the W16-OD §2 evidence: the ctor's vtable store or EH prefix, the `.rdata` order of the
object, and the next object's first function. All eight path-anchored extents fall inside W16-NY's [lo, hi] bracket.

| TU | scaffold pin | retail extent | true B | lo | hi | true / lo |
|---|---|---|---:|---:|---:|---:|
| String | `82A873C0..82A87D2C` | `82A86ED0..82A87F20` | 4,176 | 2,360 | 19,660 | 1.77 |
| ObjectThreadRoot | `82AABE98..82AABF60` | `82AABE98..82AACDE0` + init blocks | 3,912 | 200 | 31,136 | 19.56 |
| IOCompletionNotifier | `82B383E0..82B386B4` | `82B383E0..82B392D0` | 3,824 | 716 | 10,752 | 5.34 |
| EventHandler | `82AF2388..82AF2784` | `82AF2388..82AF2DC8` | 2,624 | 1,020 | 20,860 | 2.57 |
| StringStream | `82AB3648..82AB36E8` | `82AB34C0..82AB3DC8` | 2,312 | 160 | 32,572 | 14.45 |
| WorkerThreads | `82B00210..82B0046C` | `82B000E8..82B007F0` | 1,800 | 604 | 19,948 | 2.98 |
| StepSequenceJob | `82AF9190..82AF926C` | `82AF8F50..82AF9538` | 1,512 | 220 | 20,552 | 6.87 |
| MutexPrimitive | `82A801F0..82A80238` | `82A801F0..82A80378` | 392 | 72 | 18,772 | 5.44 |
| StationContactInfo | — | `82B2F110..82B2FC38` | 2,856 | — | — | — |
| Inet | — | `82B392D0..82B39320` | 80 | — | — | — |

As W16-OD found, the true/lo ratio is large for TUs with a small anchor span (ObjectThreadRoot 19.6, StringStream
14.5).

Boundary notes:

- **MutexPrimitive** has the four members (ctor, dtor, EnterImpl, LeaveImpl), then two COMDAT helpers that allocate
  and free the `RTL_CRITICAL_SECTION` (`0x82A802D0`, `0x82A80338`). Only the ctor and dtor call them. The next object
  starts at `CriticalSection::CriticalSection(unsigned)` `0x82A80378`. Its `.rdata` is the single path string, between
  `.\SessionClock\SessionClock.cpp` (a path W16-NY's table does not list) and `(vResult=Wait(0))==false`.
- **String**: the `.rdata` begins with a `basic_string` literal (`0x8217E378`), then five `""` literals used, in
  order, by `String()`, `IsEqual` (twice) and `operator<` (twice), then two copies of the path. The next object's ctor
  `0x82A87F20` stores vtable `0x8217E3B8`, which sits right after those strings. The functions before `0x82A86ED0` are
  getters over vtable `0x8217D980` and form the previous object's COMDAT tail.
- **ObjectThreadRoot** starts at the ctor, after a 4-byte `blr` at `0x82AABE90`. Its `.rdata` is the path, then the
  ObjectThreadRoot vtable (2 slots), then the `ThreadVariable` vtable (3 slots). BadEvents' path follows, and its
  ctor is `0x82AACDE0`. The TU's two `??__E` (`0x82C41F68..0x82C41FF0`) and two `??__F`
  (`0x82C4ADB8..0x82C4AE10`) blocks are pinned with it.
- **StringStream** starts at its ctor `0x82AB34C0`. The `.rdata` is the path, then the format strings, ending at
  `"\n"` (`0x821810F0`). DOClassesTable's path follows, and its ctor is `0x82AB3DC8`. **This start is the least
  certain boundary in the lane:** `0x82AB34A8` is not a StringStream method and was left out.
- **EventHandler** starts at the ctor, after the StringConverter class's dtor `0x82AF2328`. It ends at a PerfCounter
  ctor `0x82AF2DC8`. The next `.rdata` item after EventHandler's strings is `"PerfCounter %s…"`.
- **StepSequenceJob** starts at the ctor `0x82AF8F50`, which calls `Job(DebugString)` and stores vtable `0x821861A4`.
  It is the base ctor for JobJoinSession, JobBackEndServicesLogin and others. The `.rdata` is the SSJ vtable, the
  `Callback` vtable `0x821861D0`, then the path `0x821861DC`. The next object's vtable `0x821861F4` is stored by
  `0x82AF9538`, which calls SSJ's ctor.
- **WorkerThreads** starts at the ctor `0x82B000E8`, which stores vtable `0x821873DC` directly in front of the path
  string. The TU ends with the out-of-line `vector<ObjectThread<WorkerThreads,int>*>::clear` that `Stop` calls.
  `0x82B007F0` (a 64-bit time helper called from Scheduler, UDPTransport and BerkeleySocketDriver) belongs to another
  TU. `ObjectThread<WorkerThreads,int>` has no code here: its vtable is folded into `ObjectThread<UDPTransport,void*>`'s.
- **IOCompletionNotifier**: the `.rdata` holds two copies of the path, right after the previous object's EH tables.
  **The start is uncertain:** `0x82B37F70..0x82B383E0` (helpers called from UDPTransport and `0x82B3EF38`) is left
  unpinned.
- **Inet** is the two path-less functions at `0x82B392D0..0x82B39320` that W16-OD counted under IOCompletionNotifier.
  They are `Inet::Initialize`/`Terminate`, called from UDPTransport's `Initialize` and dtor. They forward to
  `0x82A8ECE0` (XNetStartup + WSAStartup) and `0x82A8EE10` (WSACleanup + XNetCleanup). The unit path
  `network/quazal/Stack/Core/Inet.cpp` is chosen, not attested.
- **StationContactInfo** is confirmed as W16-OD bounded it: one object from its own `basic_string` literal
  (`0x8218C290`) to TSG's path. It has no `__FILE__`, so the unit path
  `network/quazal/Transport/Interface/StationContactInfo.cpp` is chosen. Its own functions end at `Trace`
  (`0x82B2F99C`). `0x82B2F9A0..0x82B2FC38` is a COMDAT tail nothing in the TU calls (an `m_uiFirst`/`m_uiLast` DDL
  pair, and a class with vtable `0x8218C3B8` constructed from `0x82AF8028`), and it is not written.

dtk re-derived every `.pdata` line. Each split was a fixed point: ab_measure ran 0 extra re-splits on either leg.

## 3. Flags, and what the asm decided

All ten units use `/Od /Oi- /Ob1`, as W16-NY found for the block.

- **`/EHs-c-`** on eight units. No `.pdata` record has the EH bit and there is no FuncInfo, although String
  destroys temporaries, EventHandler and IOCompletionNotifier hold a `ScopedCS`, and WorkerThreads and the
  ObjectThreadRoot ctor hold a String across a `new`. Built with EH on, MutexPrimitive's ctor/dtor emit
  `__unwind` funclets that retail does not have. StationContactInfo and Inet keep EH on: StationContactInfo has EH
  tables, and Inet has no evidence either way.
- **`/GR-`** where a vtable exists with no complete-object locator in front of it: ObjectThreadRoot, WorkerThreads,
  StepSequenceJob and StationContactInfo's tail class. For units with no vtable (StringStream, IOCompletionNotifier,
  Inet), `/GR-` cannot be checked and follows the neighbouring TUs. String, EventHandler and MutexPrimitive keep the
  default.

Source facts the asm decided:

- **String:** `/Oi-` holds, because retail calls `strlen`, `strcpy` and `strcmp` out of line. The CRT's fixed-argument
  `__va_start` crashes cl at `/Od` (as W16-NW saw), so the TU declares the CRT functions and a minimal
  `MemoryManager` locally. A `#line` that moves the line number backwards made cl hang, so the file's prologue comment
  is short and the long comment is at the end.
- **MutexPrimitive:** the allocation helper needs a named `T *pObject` temp (91.8 → 100).
- **WorkerThreads:** `Stop`'s first `ScopedCS` is block-scoped, because retail destroys it before the loop.
- **StepSequenceJob:** the Step is 0x10 bytes at `0x48` and holds an 8-byte pointer-to-member, so it needs
  `__multiple_inheritance`. The shared `StepSequenceJob.h` puts it at `0x44`, so the class stays local, as it does in
  JBESL, JCS and JJS.
- **ObjectThreadRoot:** retail's thread handle is a single HANDLE, while `ObjectThread.h`'s is three members, so the
  classes are local. `ThreadVariable::ResetValues` is an inline that `/Ob1` declines: the dtor and its callers
  reserve its frame.

## 4. Integration

### 4.1 The Core/Scheduler shape (sub-lane g): settled

W16-NY recorded that DuplicatedObject's `Core::GetInstance` shape (three locals and a `?:`) left 12 PRUDPStream rows at
99.6–99.96. W16-OD saw UDPTransport and JCEP reserve 12 B too much. W16-OC measured the shape that reproduces retail's
`GetSystemLock` (`0x82A6F650`) 20-byte frame at −6 / −4,060 B. Sub-lane g scratch-compiled every TU that expands or
reserves these inlines: 23 TUs, W16-OC's seven included. It diffed every row against main `7431336d6` for each
candidate.

| candidate | rows vs main |
|---|---|
| 5 `Core::GetInstance` locals (reproduces `0x82A6F650`'s body) | 101 down, 6 up |
| 4 / 2 / 1 / 0 locals | 90 / 58 / 66 / 70 down (DuplicatedObject −10 in each) |
| 3 locals, `?:` (main) | — (baseline) |
| 3 locals, if-assignment only | +2 (UDPTransport) |
| **3 locals, if-assignment + braced `if (pCore == 0) { return 0; } else { … }`** | **+16, 0 down** |
| + `!pCore`, or unbraced if/else | PRUDPStream −12 |
| + early return | PRUDPStream −14, DuplicatedObject −13 |
| unbraced `if` in `Core::GetInstance` | PRUDPStream −12 |

**One shape is right for every TU on main, so it is now the shared one.** It is the shape W16-OD's JCEP, NAT and JCSEP
local copies already used. It is applied to `Core.h` and `Scheduler.h`, and to the TU-local copies in JobCreateAccount,
JobLoginOrCreateAccount, CallRegister, DOCallContext, DOCore and UDPTransport. Measured on its own on `7431336d6`:
+16 fns / +11 masked / +2,600 B. The rows that rise are PRUDPStream `FindEndPointByCID`/`PID` and
`RemoveCID`/`PIDEndPointAssociation`, UDPTransport `ServiceIOCompletions`, and 11 EH funclets.

What the shape does **not** reproduce: retail's out-of-line `GetSystemLock` (`0x82A6F650`) has a 0x80 frame, against
our 0x70. In our compiler a caller's reservation always equals the body's local count, and retail's callers reserve
what ours do, so no single definition gives both. That row is unpaired (`auto_*`) and scores nothing either way.
Per-site mismatches remain under every shape tried, so their cause is local to each site:

- PRUDPStream `CreateEndPoint`/`ReleaseEndPoint` and UDPTransport `FindSocket` reserve 3 words too much.
- The UDPTransport ctor is 1 word off.
- PRUDPEndPoint `SetKeepAliveTimeout` is 6 words short.

W16-OC's "different inline helpers at different call sites" remains the best reading of these.

### 4.2 Shared-header edits

**Additions:**

- `Platform/String.h` (declarations only): `String(const wchar_t*)`, `operator=(const wchar_t*)`, `operator+=`,
  `Left`, `CreateCopy(wchar_t**)`, `ReleaseCopy(wchar_t*)`, `ToUpper`, `ToLower`, `Find`, `FindNoCase`, and the free
  `operator+(const char*, const String&)`.
- `Platform/StringStream.h`:
  - The member `operator<<` overloads retail has: `unsigned long`, `long`, `bool`, `double`, `float`, `const void*`,
    `(unsigned) long long`, `const StringStream&`.
  - `GetLength`, `Reset`, `FreeBuffer`, `ReleaseBuffer`, `Resize`, `ShowBase`.
  - The manipulators `endl`, `showbase`, `noshowbase`, `boolalpha`, `noboolalpha`.
  - Inline `operator<<(int)` / `operator<<(unsigned int)` forwarding to `long` / `unsigned long`. Retail has no int
    overloads, but CallRegister's `ss << int` is ambiguous without them.

**Not additions:**

- `Platform/StringStream.h`: the unused `static void hex/dec(StringStream&)` became non-static and return
  `StringStream&`, as retail's callers use them. Nothing in the tree used the old declarations.
- `Platform/ObjectThread.h`: `ObjectThreadRoot::Launch` now returns `bool`. Retail `0x82AAC1C8` returns 1/0 and is
  mapped `QAA_NXZ`. `ObjectThread<T,A>::Update`, which WorkerThreads::Start expands, ignores the value, so codegen is
  unchanged. With `void`, the call site named a function retail does not have.
- `Core/Core.h`, `Core/Scheduler.h`: the inline bodies in §4.1, which is the conflict the brief allowed to be settled.

Edits outside the lane's own TUs:

- `band3/net_band/DataResults.cpp`: the wide `operator=` makes `str = 0` ambiguous. `DataResultList::Clear` now
  passes `(const wchar_t *)0`, because retail calls the wide overload there. The row stays at 100.
- `Transport/UDP/UDPTransport.cpp`, three edits, all spelling-only:
  - Its local ObjectThreadRoot declares `bool Launch()`, and calls `MethodStarted` by its mapped name `ReadyToRun`.
  - `Inet::Terminate` returns `bool`.
  - It uses the §4.1 shape.

The other six sub-lanes declare their classes locally where the shared header's layout is not retail's:
EventHandler, StepSequenceJob, ObjectThreadRoot, IOCompletionNotifier (an `IOCompletionContext` with inline accessors
at a 0x3c layout), and StationContactInfo.

### 4.3 One spelling per retail function

- Map: **149 entries added, all at addresses that had no name; none changed, none removed.** By TU: MutexPrimitive 6,
  WorkerThreads 8, EventHandler + StepSequenceJob 23, String 34, ObjectThreadRoot + StringStream 59,
  IOCompletionNotifier + Inet + StationContactInfo 19.
- Names are taken from the existing callers wherever they spell one:
  - DOCallContext's `DeleteEventObject` and `WaitForEvent(unsigned, Event**) const`.
  - JCEP/JBESL's `StepSequenceJob` methods.
  - QueuingSocket/UDPTransport's IOCompletionNotifier methods.
  - The `String` members, which a reference census over every built object found consistent.
  - JBESL/JobCreateAccount's `qNewArray<char>` / `qDeleteArray<char>`.
- **Chosen here, not retail-attested:**
  - MutexPrimitive: `qNewPOD` / `qDeletePOD`.
  - EventHandler: `GetEventIndex`, `GetEventHandle`, `IsSignaled`, `WaitForEventImpl`.
  - StepSequenceJob: `CreateCallResultCallback`, `TraceStep`.
  - String: `Left`, `ToUpper`, `ToLower`, `Find`, `FindNoCase`, the wide overloads, `_Copy`, `CopyString`.
  - The StringStream method names.
  - IOCompletionNotifier: `GetOverlappedResult`, `WaitForEvents`, `PollSocketDriver`.
  - All of `Inet`.
  - StationContactInfo: `SetRVConnectionID`, `AddURL`, `SortAndFilterTarget`.
- **The one integration loss:** `0x82A87E98` is the single retail body of every `qDeleteArray<T>`. Delete[] of a POD
  is the same code for any `T`, so the linker folded them. It is named `qDeleteArray<char>`, the spelling most objects
  use (5 objects, against 2 for `void*`, 2 for `wchar_t` and 1 for `Event*`). Callers that spell another `T` are now
  charged:
  - `~EventHandler` (248 B) and `~IOCompletionNotifier` (116 B), which each sub-lane had at 100 before String's
    names merged (99.84 and 99.83 now).
  - String's own `ReleaseCopy(wchar_t*)` (99.6).
  - StringConversion `FreeWide` and `Char8ToUtf8` (§5.1).

  Respelling those callers as `qDeleteArray<char>` would fit the source to a fold, so it was not done. This is the
  alias lane's class.

The integrated tree read the sub-lanes' sum minus exactly those two destructors (+166 / +22,256 B summed;
+164 / +21,892 B integrated; 248 + 116 = 364 B).

## 5. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-og-ab --patch ~/tmp/w16og/ab.patch`. The patch is
`git diff 7431336d6 HEAD -- . ':(exclude)docs'`: map, splits, `objects.json`, sources and headers. The A/B worktree
is a fresh `setup_worktree.sh` tree at main `7431336d6`. Run dir:
`~/tmp/wt-w16-og-ab/.ab_measure_runs/20261003-005302-ab-861100/`.

**Prediction, written before the run:** Δmatched +164, Δmasked_equal +18, Δhonest +146, Δmatched_code +21,892 B.
Two rows down (`FreeWide` 100 → 99.5, `Char8ToUtf8` 95.211 → 95.141). It came from the rebased tree's settled
`report.json` against `7431336d6`'s.

```
leg A: matched=52924 masked=25107 honest=27817 code%=56.884956  (recompiles: 0, settled)
leg B: matched=53088 masked=25125 honest=27963 code%=57.098587  (recompiles: 313, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+164  Δmasked_equal=+18  Δhonest=+146  Δcode%=+0.213631pp  Δcode_bytes=+21892
Δfuzzy=+0.219559pp   (legA 63.168407 -> legB 63.387966)
unit net (ALL units) = +164   vs whole-binary Δmatched = +164
units at 100% [mpn ruler]: legA 499 -> legB 504
units at 100% [all-rows-fuzzy ruler]: legA 441 -> legB 445
```

**Measured exactly as predicted.**

- Per unit (mpn): ObjectThreadRoot +32, String +32, StringStream +26, PRUDPStream +14, StationContactInfo +13,
  StepSequenceJob +12, EventHandler +10, IOCompletionNotifier +8, WorkerThreads +8, MutexPrimitive +6, Inet +2,
  UDPTransport +2, StringConversion −1.
- `total_code` and `total_functions` are unchanged (10,247,792 / 68,914). The pins only move `auto_*` rows into named
  units.

### 5.1 Rows that went down

Leg A → leg B, keyed by row name (`~/tmp/w16nw/rowdiff.py` on the archived legs): 24 rows up, **2 down**. There are
149 vanish/appear pairs: placeholder rows that took their mapped names, 22,268 B on each side, every vanished row at
fuzzy 0 / mpn 0. Another 432 rows changed unit name only.

| row | size | fuzzy | reason |
|---|---:|---|---|
| StringConversion `FreeWide(wchar_t*)` | 40 | 100 → 99.5 | Calls `qDeleteArray<wchar_t>`. The retail body `0x82A87E98` is the fold survivor of every `qDeleteArray<T>` and is now named `qDeleteArray<char>` (§4.3). |
| StringConversion `Char8ToUtf8` | 284 | 95.211 → 95.141 | Same call. It was already below 100. |

No other row outside the lane's units changed score.

### 5.2 Other ruler (reading only)

ab_measure's `none` control read Δmatched_code +22,876 B, against +21,892 B graded. The patch carries source, so the
tool labels this NOT_APPLICABLE. The 984 B gap is the relocation-name class: the `qDeleteArray` fold and the
`~RootObject`/`0x826C3888` funclet charges.

## 6. Remaining sub-100 rows

| unit | row | B | fuzzy | what is left |
|---|---|---:|---:|---|
| EventHandler | ctor | 360 | 98.56 | Register numbering from the loop's second statement; slots match. |
| EventHandler | `CreateEventObject` | 412 | 97.28 | The same register-numbering offset, from the `new`'s `__FILE__` `lis`. |
| EventHandler | dtor | 248 | 99.84 | `qDeleteArray` fold name (§4.3). |
| StepSequenceJob | `Execute` | 268 | 95.82 (mpn 100) | One register of numbering offset before `CheckExceptions`. |
| String | `operator<` | 256 | 99.94 | The two conversion temporaries sit in swapped slots (retail: oString's at 0x64, `*this`'s at 0x60). |
| String | `ReleaseCopy(wchar_t*)` | 52 | 99.6 | `qDeleteArray` fold name. |
| String | `fn_82A87B78` | 12 | 0 | An empty two-argument function with no `bl` or pointer reference anywhere in the binary; deliberately unwritten. |
| ObjectThreadRoot | `??__Es_tvCurrentThread` | 64 | 99.875 | Frame 0xD0 against retail's 0xF0; four variants did not move it. |
| ObjectThreadRoot | `fn_82AACD50` | 144 | 0 | A folded `_Rb_tree::erase` at the TU's end. PRUDPStream and CallRegister call it as their own instantiations, so naming it would charge them. |
| IOCompletionNotifier | dtor | 116 | 99.83 | `qDeleteArray` fold name. |
| StationContactInfo | `StationContactInfo(const qList<StationURL>&)` | — | 68.3 | Retail calls `list(const allocator&)` (`0x82A8FB10`) out of line; ours expands it. |
| StationContactInfo | `SortAndFilterTarget` | — | 96.4 | Retail expands `push_back` in both fill loops; ours expands the first only (about 8 B of frame). |
| StationContactInfo | five EH funclets | — | 99.3–99.5 | Fold survivors `0x826C3888` / `0x82AFF0F0` (alias lane). |
| StationContactInfo | COMDAT tail | 652 | 0 | Not written (§2). |

About 25 variants did not move the EventHandler/StepSequenceJob register rows.

## 7. Native gate

Run last, on the code at the commit before this doc's gate edit.

```
(filled in by the gate run)
```

## 8. Not done

- **Fold-name charges (alias lane):**
  - `0x82A87E98` `qDeleteArray<T>` (5 rows, §4.3).
  - `0x826C3888` / `0x82AFF0F0` (StationContactInfo funclets).
  - `fn_82AACD50`, the folded `_Rb_tree::erase`.
- **Unwritten neighbours found along the way:**
  - The CriticalSection TU starting at `0x82A80378`: ctor(unsigned), a second ctor `0x82A803B8`, dtor, and
    EnterImpl/LeaveImpl testing `s_bNoOp` at `0x82E10320`. Its end is not established.
  - StringConverter's class tail `0x82AF1F70..0x82AF2388` (still `auto_*`).
  - The PerfCounter TU from `0x82AF2DC8`.
  - IOCompletionNotifier's possible head `0x82B37F70..0x82B383E0`.
  - StationContactInfo's COMDAT tail.
  - The `ThreadVariable` fold targets in `0x82A81xxx` (`GetValueRef` is also called from `0x82A81B18/B48/B78`;
    `0x82A81BF0` is the folded `SetValue`).
  - A `.\SessionClock\SessionClock.cpp` path at `0x82A7F820` that W16-NY's table does not list.
- **The per-site `Core`/`Scheduler` reservations** listed in §4.1, and `GetSystemLock`'s 0x80 frame.
- **`StepSequenceJob.h` / `ObjectThread.h` / `EventHandler.h` / `IOCompletionContext.h`** do not have retail's
  layouts. The new TUs declare local classes rather than change them.
- Not merged to main.

Scratch: `~/tmp/w16og/` (`jresolve.py` resolves `objects.json` / map conflicts at key level during a rebase;
`rebase_loop.sh`; `ab.patch`, `ab_run.log`, `legA.json`, `legB.json`, `report_int1.json`). Sub-lane tools are in
`~/tmp/w16og-{a..g}/`.
