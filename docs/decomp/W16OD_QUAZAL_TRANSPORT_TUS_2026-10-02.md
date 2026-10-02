# W16-OD — the Quazal transport TUs written from the retail asm (2026-10-02)

**Branch** `w16-od`, on main `1505d4c74`. **Ruler** `name_check` (graded). Permuter not run. No alias group and no
`scripts/symbol_aliases.json` entry touched. `symbols.txt` is unchanged.

This lane applies W16-NY's method (`docs/decomp/W16NY_OD_BLOCK_PRICING_AND_LARGEST_TUS_2026-10-02.md`) to eight
transport TUs from its table: HighLevelStream, RoutingStream, UDPTransport, QueuingSocket, BerkeleySocketDriver,
TransportSignatureGenerator, JobConnectEndPoint and JobConnectSecureEndPoint. Each was a 7–8-line `namespace Quazal {}`
scaffold pinned over its `__FILE__` references. The work was split across five sub-lanes, one worktree each
(`w16-od-udp`, `w16-od-qsock`, `w16-od-jcep`, `w16-od-streams`, `w16-od-jcsep`). Adjacent TUs went to the same
sub-lane. The sub-lanes were merged here with `--no-ff` and reconciled (§4).

## 1. Result

`ab_measure --patch` (§5), main `1505d4c74` → this branch: **+365 functions, +203 masked_equal, +162 honest,
+39,520 B** (+0.385643 pp). That is the prediction to the byte, and **0 rows went down**.

| TU | flags | main: rows at 100 / rows | main: unit B | branch: rows at 100 / rows | branch: matched / unit B |
|---|---|---:|---:|---:|---:|
| UDPTransport | `/Od /Oi- /Ob1 /GR-` | 0 / 42 | 6,184 | 88 / 152 | 11,116 / 19,348 |
| QueuingSocket | `/Od /Oi- /Ob1 /GR-` | 0 / 9 | 2,488 | 60 / 80 | 6,648 / 9,504 |
| JobConnectEndPoint | `/Od /Oi- /Ob1 /GR-` | 0 / 13 | 1,984 | 70 / 91 | 7,920 / 10,660 |
| BerkeleySocketDriver | `/Od /Oi- /Ob1 /GR- /EHs-c-` | 0 / 1 | 96 | **22 / 22** | 5,252 / 5,252 |
| JobConnectSecureEndPoint | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 0 / 3 | 1,116 | 15 / 18 | 3,884 / 5,020 |
| RoutingStream | `/Od /Oi- /Ob1 /GR-` | 0 / 3 | 352 | 17 / 18 | 2,184 / 2,224 |
| TransportSignatureGenerator | `/Od /Oi- /Ob1 /GR-` | 0 / 4 | 612 | 14 / 18 | 1,408 / 1,568 |
| HighLevelStream | `/Od /Oi- /Ob1 /GR-` | 0 / 2 | 524 | 10 / 11 | 1,108 / 1,352 |
| **total** | | | | | **39,520** |

Row counts include EH funclets. Three units reach 100% on `mpn` (BerkeleySocketDriver, TSG, RoutingStream), and one
on all-rows-fuzzy (BerkeleySocketDriver).

## 2. Extents

Each sub-lane found the retail extent from the ctor's vtable store and EH prefix, from the `.rdata` order of each
object (path strings, then the TU's single `basic_string` literal, then FuncInfo tables in `.text` order with
vtables between them), and from the next TU's first function. All eight extents fall inside W16-NY's [lo, hi]
bracket. Counting W16-NY's nine, that is 17 of 17.

| TU | scaffold pin | retail extent | true B | lo | hi | true / lo |
|---|---|---|---:|---:|---:|---:|
| UDPTransport | `82B15FA0..82B17810` | `82B15808..82B1A518` | 19,728 | 6,184 | 25,960 | 3.19 |
| JobConnectEndPoint | `82B2C748..82B2CF08` | `82B2C698..82B2F110` | 10,872 | 1,984 | 14,896 | 5.48 |
| QueuingSocket | `82B39AB8..82B3A488` | `82B39328..82B3B918` | 9,712 | 2,488 | 21,756 | 3.90 |
| BerkeleySocketDriver | `82B3DD00..82B3DD60` | `82B3CD20..82B3E1C0` | 5,280 | 96 | 20,308 | 55.0 |
| JobConnectSecureEndPoint | `82B42E30..82B43294` | `82B421C8..82B43590` | 5,064 | 1,116 | 16,856 | 4.54 |
| RoutingStream | `82B0A5D0..82B0A730` | `82B0A518..82B0ADF0` | 2,264 | 352 | 35,096 | 6.43 |
| TransportSignatureGenerator | `82B2FC40..82B2FEA4` | `82B2FC40..82B30278` | 1,592 | 612 | 19,436 | 2.60 |
| HighLevelStream | `82B4EDE8..82B4EFF4` | `82B4EAC8..82B4F040` | 1,400 | 524 | 40,104 | 2.67 |

**The true/lo ratio here runs 2.6–6.4, with BerkeleySocketDriver at 55, against W16-NY's 1.01–3.29.** The
difference is selection: these TUs had small anchor spans (lo), and small-lo TUs have proportionally more
unanchored code. So W16-NY's "≈1.8× median ⇒ ~340 KB in the 105 path-anchored TUs" figure is low for the small-lo
end of its table.

Boundary notes:

- **UDPTransport** starts at the ctor's EH prefix (vtable `0x82189BE8`, whose 14 slots match the Wii UDPTransport
  vtable) and ends at the `Socket` ctor `0x82B1A518`, which only QueuingSocket's ctor calls. Its own functions end
  at `FindSocket` (`0x82B184D0`); the rest is the template and inline code it instantiates.
- **JobConnectEndPoint** starts with the file-local `ConnectCancelCallback` (vtable `0x8218C250`, next to JCEP's
  `0x8218C25C`). The sub-lane first pinned it to `0x82B2FC38` and then corrected the end to `0x82B2F110`. The span
  `0x82B2F110..0x82B2FC38` has its own `basic_string` literal (`0x8218C290`), and its functions follow the Wii
  `StationContactInfo.cpp` order (ctor, ctor(qList), dtor, SortAndFilterTarget, Trace). It is now
  `auto_03_82B2F110_text` (2,808 B), unpinned.
- **TransportSignatureGenerator** ends at the EH prefix of the "PRUDP Timeout Queue" TU at `0x82B30280`.
- **QueuingSocket** starts at the ctor `0x82B39328`, the first FuncInfo after the TU's seven path strings. It ends
  where the next object's `basic_string` (`0x8218E0C0`) and the channel ctor at `0x82B3B920` begin. Its own functions
  run to `0x82B3A85C`, followed by the COMDAT tail for the network-emulation queues. `0x82B386B8..0x82B39320`
  (IOCompletionNotifier methods, no EH) stays unpinned.
- **BerkeleySocketDriver** runs from the BerkeleySocket ctor (right after Packet's last unwind funclet) to the
  BerkeleySocket scalar deleting dtor. Its `.rdata` is the path string plus two vtables and no EH tables.
- **JobConnectSecureEndPoint** starts at the ctor (vtable `0x8218EB34`, right before the TU's path string
  `0x8218EB60`) and ends after Trace and the list-init helper `0x82B43568`. **Least certain boundary in this lane:**
  `0x82B41EE0..0x82B421C4` (`ConnectionData` list helpers) was left out. They are first called from the unpinned job
  at `0x82B41688`, but the `ConnectionData` vtable `0x8218EB30` sits just in front of JCSEP's.
- **HighLevelStream** runs from the ctor's EH prefix (vtable `0x8218FD20`) through its scalar deleting dtor.
  **RoutingStream** ends at the EH prefix of `RoutingTable::RoutingTable(unsigned)`, whose `.rdata` opens with its
  own `basic_string` at `0x82188790`.

dtk re-derived every `.pdata` line, and every split was a fixed point (ab_measure: 0 extra re-splits on either leg).

## 3. Flags

All eight use `/Od /Oi- /Ob1`, as W16-NY found for the block.

- **`/GR-` on all eight.** No vtable in these TUs has a complete-object locator in front of it. ⚠ For
  TransportSignatureGenerator, which defines no vtable, this cannot be checked, and `/GR` changes no `.text`.
  UDPTransport measured `/GR` and `/GR-` identical on every row, so `/GR-` rests on the retail `.rdata` evidence
  alone. NAT and PRUDP have the same missing locators and still build with `/GR`; this lane did not change them.
- **`/EHs-c-`** on BerkeleySocketDriver (every function holds a `ScopedCS`, yet the TU has no EH tables) and
  JobConnectSecureEndPoint (no `.pdata` record has the EH bit). The other six have FuncInfo/funclets and keep EH on.

## 4. Integration

### 4.1 Shared headers

**No shared Quazal header was changed** (nothing under `Core/`, `Platform/`, `qStd.h`, `RootObject.h`,
`Scheduler.h`, `ScopedCS.h`, `qMemAllocator.h`). Every TU declares what it needs locally, as PRUDPStream and NAT
do. JobConnectEndPoint carries its own `MemAllocator`/`qList`, because retail's shape there needs a `qList()` ctor
and an allocator with no dtor, which `qStd.h` does not provide.

Edits outside the eight TUs' own files, all spelling-only (codegen unchanged):

- **`Transport/PRUDP/PRUDPStream.h` + `PRUDPStream.cpp`.** PRUDPStream's member at `0xe0` is renamed
  `SessionIDTable m_oSessionIDs` → `TransportSignatureGenerator m_oSignatureGenerator`, and its call becomes
  `ComputeSourceSignature`, the name TSG's TU maps. The header is included only by `PRUDPStream.cpp`
  (`grep -rln PRUDPStream.h src native`), so it is shared in name only. Without this edit, mapping TSG cost two
  PRUDPStream funclets 100 → 99.5.
- **`Transport/PRUDP/PRUDPEndPoint.cpp`**, two edits:
  - The same `0xe0` member and call are renamed.
  - The `GetSettings` accessor moved from the local `ConnectionOrientedStream` onto a local `Stream` base with the
    same layout. Its out-of-line copy is the COMDAT at `0x82B0AB88` in RoutingStream's extent, which the map now
    names `Stream::GetSettings`. Without the move, five PRUDPEndPoint rows fall 100 → 99.92–99.96.

### 4.2 One spelling per retail function

The UDPTransport and QueuingSocket sub-lanes named QueuingSocket's methods differently. After both merged, the
QueuingSocket map names charged four UDPTransport rows: BindSocket 100 → 99.96, `Receive(QueuingSocket*,…)`
100 → 99.87, ServiceIOCompletions, and DeliverOutgoing. UDPTransport now calls them by QueuingSocket's names. Each
name was decided by the retail `bl` target at the call site:

| UDPTransport's old spelling | retail target | name now |
|---|---|---|
| `Recv(BandwidthCounter*)` | `0x82B3A1D0` | `Recv(unsigned int)` |
| `GetReceivedBuffer` | `0x82B3A688` | `CompleteBufferRecv` |
| `FilterIncoming` | `0x82B3A4B0` | `FillPacketQueueFromBuffer` |
| `SendCompleted` | `0x82B3A0D0` | `CompleteSend` |
| `PrepareOutgoing(ProtectedPacketQueue*, void*)` | `0x82B39AB8` | `CreateBufferFromPacketQueue(PacketQueue*, unsigned)` |
| `SendTo` / `SendToEmulated` / `FlushEmulated` | `0x82B39810` / `…880` / `…8F8` | `Send` / `Queue` / `Flush` |

**One type was wrong, not just a name.** `RootTransport+0x8` (copied to `QueuingSocket+0x94`) was declared
`BandwidthCounter*`. Retail passes it to `QueuingSocket::Recv`, which does `new Buffer(uiSize)` with it, and
nothing dereferences it. It is now `unsigned int m_uiRecvBufferSize`. The second argument of
`CreateBufferFromPacketQueue` comes from `0x82AD0CC0`, which returns `this->[0x68]->[0x8]`. That accessor was
declared `void *GetRoutingTable()` and is now `unsigned int GetMaxBufferSize()`. The name is not attested, and the
address is unmapped, so no call site is checked against it. Measured: all four rows returned to their pre-merge
scores (+2 fns / +708 B), and nothing else moved.

The integrated tree then read the five sub-lanes' sum exactly (+365 / +39,520 B).

### 4.3 Names

Wii-attested names (`../rb3/config/SZBE69_B8/symbols.txt`) are used wherever the Wii build has the function; the
sub-lanes followed the Wii source order. These method names were chosen by the sub-lanes and are not
retail-attested:

- UDPTransport: `FindSocket`, `DispatchIncoming`, `Receive(QueuingSocket*, …)`, and the `qSortedVector`, `qVector`,
  `ProtectedPacketQueue`, `ProfilingScope` and `VirtualPort` helper classes.
- QueuingSocket: the X360-only helpers and the emulation-queue classes.
- JobConnectSecureEndPoint: `ExecuteStep`, `RequestCompletionCallback`, `CheckConnectionData`,
  `IsConnectionDataAvailable`, `PrepareConnectionRequest`, `GetConnectionURLs`, `ProcessConnectionResult`,
  `CompleteConnection`.
- RoutingStream / HighLevelStream: `Stream::GetSettings`, `ExtractRoutingHeader`, `IsValidRoutingPayload`,
  `GetHeaderSize`, `GetPerfCounters`, `VirtualPort`.

Map: 188 new entries, all at addresses that had no name before. **No existing entry changed, none removed.**

## 5. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-od-ab --patch ~/tmp/w16od/ab.patch`, where the patch is
`git diff 1505d4c74 <branch code head>` (map, splits, `objects.json`, sources). The A/B worktree is fresh on main
`1505d4c74`, and the run dir is
`~/tmp/wt-w16-od-ab/.ab_measure_runs/20261002-233850-ab-156744/`.

**Prediction, written before the run:** Δmatched +365, Δmasked_equal +203, Δhonest +162, Δmatched_code +39,520 B,
0 rows down. These came from the integrated tree's `report.json` against main's.

```
leg A: matched=52189 masked=24904 honest=27285 code%=55.782787  (recompiles: 0, settled)
leg B: matched=52554 masked=25107 honest=27447 code%=56.168430  (recompiles: 10, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+365  Δmasked_equal=+203  Δhonest=+162  Δcode%=+0.385643pp  Δcode_bytes=+39520
Δfuzzy=+0.533630pp   (legA 61.627780 -> legB 62.161410)
unit net (ALL units) = +365   vs whole-binary Δmatched = +365
units at 100% [mpn ruler]: legA 491 -> legB 494
```

**Measured exactly as predicted.** Per unit: UDPTransport +127, JobConnectEndPoint +82, QueuingSocket +73,
BerkeleySocketDriver +22, TSG +18, RoutingStream +18, JCSEP +15, HighLevelStream +10. `total_code` and
`total_functions` are unchanged (10,247,792 / 68,914). The pins only move `auto_*` rows into named units.

### 5.1 Rows that went down

**None.** Leg A → leg B, keyed by row name (`~/tmp/w16ny/rowdiff.py` on the archived legs):

- 221 rows up, **0 down**.
- 188 vanish/appear pairs: placeholder `fn_` rows that took their mapped names. 45,300 B on each side, and every
  vanished row was at fuzzy 0 / mpn 0.
- 363 rows changed unit name only (`auto_*` ranges renamed when the pins moved).

No row outside the eight TUs changed score.

### 5.2 Other ruler (reading only)

ab_measure's `none` control: Δmatched_code +46,108 B against +39,520 B graded. The patch carries source, so this is
expected, and the tool labels it NOT_APPLICABLE. The 6,588 B gap between the rulers is the relocation-name class:
rows whose remaining charges include a callee name, e.g. the fold names in §6. It was not itemized.

## 6. Rows below 100

Named rows (funclets are summarized after the tables).

**UDPTransport** (46/57 named rows at 100):

| addr | B | row | fuzzy | what is left |
|---|---:|---|---:|---|
| 82B15810 | 336 | ctor | 93.750 | Retail calls the allocator-proxy ctor `0x82A9D1E0` out of line; ours expands it. No `MemAllocator` shape gives both (StationURL's ctors have the same problem). |
| 82B15B18 | 600 | dtor | 99.787 | frame 0x24 too large around the member dtors |
| 82B16630 | 828 | StartListen | 99.807 | retail reserves 12 B for `pop_front` → list `erase`'s locals |
| 82B16C58 | 840 | StopListen(us) | 99.733 | same 12-byte reservation |
| 82B17238 | 708 | Send(us, …) | 99.898 | order of the VirtualPort temporaries |
| 82B17968 | 616 | ServiceIOCompletions | 99.799 | `Scheduler::GetSystemLock`'s reserved area 12 B too large |
| 82B17C70 | 344 | TransportThread | 96.337 | retail skips one register number after the first call |
| 82B17FE0 | 764 | DeliverOutgoing | 95.738 | GetDestination temp missing; temp-slot order |
| 82B184D0 | 368 | FindSocket | 99.870 | same GetSystemLock reservation |
| 82B18A78 | 52 | `~qSortedVector` | 75.462 | retail frame 0xc0 with r31, ours 0xb0 |
| 82B19228 | 404 | `vector::reserve` | 99.950 | callee-name only: retail's one body at `0x82B19558` serves both clear helpers (a fold) |

**QueuingSocket** (37/40):

| addr | B | row | fuzzy | what is left |
|---|---:|---|---:|---|
| 82B394A8 | 248 | `~QueuingSocket` | 99.565 | body identical; frame 0x10 larger (space reserved for the refused `~EmulationQueue` inlines) |
| 82B39AB8 | 1,416 | CreateBufferFromPacketQueue | 99.972 | only the `0x823EBC90` fold-name charge |
| 82B3A4B0 | 472 | FillPacketQueueFromBuffer | 99.915 | same |

**JobConnectEndPoint** (26/35):

| addr | B | row | fuzzy | what is left |
|---|---:|---|---:|---|
| 82B2D180 | 124 | MustAbort | 99.710 | frame one slot short; no single `Core::GetInstance` shape fits MustAbort, SetCallContextState and DisconnectCallback (3 variants measured; the one keeping DisconnectCallback, 272 B, at 100 is kept) |
| 82B2D6D0 | 156 | SortURLs | 99.795 | `0x823EBC90` charge + frame |
| 82B2DB48 | 208 | CanRouteTo | 64.346 | `0x823EBC90` charge + order of evaluation |
| 82B2DC48 | 796 | TestCurrentURL | 99.975 | `0x823EBC90` charge only |
| 82B2E060 | 164 | PrepareNATTraversal | 99.610 | charge + frame |
| 82B2E998 | 264 | ProcessConnectionFailure | 99.924 | charge only |
| 82B2EAF8 | 228 | UpdateCurrentURL | 99.632 | charge + frame |
| 82B2ED18 | 124 | SetCallContextState | 99.548 | frame one slot short (see MustAbort) |
| 82B2EF08 | 180 | Trace | 72.844 | an unused `end()` object before the loop, not reproduced |

**JobConnectSecureEndPoint** (15/17):

| addr | B | row | fuzzy | what is left |
|---|---:|---|---:|---|
| 82B421C8 | 472 | ctor | 91.305 | Retail expands the member list's `_List_base` ctor but calls `_M_empty_initialize` out of line; ours expands both. Retail `CallContext`'s ctor `0x82A8AF30` has the same shape. `#pragma inline_depth(2)` broke four rows and was not kept. |
| 82B43020 | 628 | PerformConnect | 98.758 | frame only. The helpers are `inline` to get the out-of-line list ctor (92.28 → 98.76); retail does not reserve their frames. |

**HighLevelStream** (5/6): the ctor (`82B4EAD0`, 244 B) is at 99.918. Its only charge is the `0x823EBC90` call.

**BerkeleySocketDriver, RoutingStream, TransportSignatureGenerator:** every named row is at 100.

**Funclets.** Most of the remainder are EH funclets at 99.2–99.9 that call `0x826C3888`, the `StlNodeAlloc` ctor
fold survivor, where our source calls the empty `~RootObject`/`~Time`. That is the known fold-name class W16-NY §6
lists for the alias lane. JCSEP's `0x82B43568` (36 B, list-init helper, also called from `0x82A8AF30`,
`0x82AF4458` and `0x82C413C0`) is left unmapped; our object does not emit it, and it looks like a fold survivor.

## 7. Reusable findings

- **MSVC puts overloaded virtuals into the vtable in reverse declaration order.** UDPTransport declares
  `StopListen(us)` before `StopListen()` and `Send(us, …)` before `Send(StationURL*, …)`, so that the slots land
  where retail has them.
- **A class holding a `Time` pads its vfptr to 8.** NAT found this, and JCSEP's `CallContext` confirms it: six rows
  were fixed at once.
- **A compiled-out block can leave its test.** JCSEP `Execute` reads the state once with no effect, written as an
  empty `if (GetState() == Complete) {}`. QueuingSocket's `DeleteContext` tests `WaitForIOCompletion`'s result the
  same way.
- **`__declspec(noinline)` on `Time()` in QueuingSocket.** Retail calls the COMDAT `Time()` at `0x82B3AF20` and never
  expands it in this TU, and no other construct reproduced that. DuplicatedObject uses the same device. Without it,
  `EmulationQueue::Queue` (616 B) and `Time()` fall below 100.
- **A declined inline still reserves its frame** (W16-NY §2). UDPTransport keeps the sorted vector's `find` in-class,
  because retail reserves it in `erase` and `FindSocket`, and defines `insert` out of class, because BindSocket
  reserves nothing for it.
- **`TransportDelegator::GetInstance` has five locals in HighLevelStream's copy.** That count is fitted to the ctor's
  frame. The only retail body is the `/O1` copy it was folded into, so it cannot be read. The source says so.

## 8. Native gate

Run last, on the code at `74b466f79` (only this docs edit follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 9. Not done

- **Fold-name charges (alias lane):** `0x823EBC90` (`GetInstanceType1Delegator` anon name; JCEP ×6, QueuingSocket ×2,
  HighLevelStream ctor), `0x826C3888` (StlNodeAlloc ctor survivor; most sub-100 funclets), `0x82B19558`
  (UDPTransport's two clear helpers).
- **The `Core::GetInstance` / `Scheduler::GetSystemLock` shape question** from W16-NY §3.3, now also seen in JCEP
  (MustAbort/SetCallContextState) and UDPTransport (ServiceIOCompletions/FindSocket reserve 12 B too much).
- **The `MemAllocator` proxy ctor question** (UDPTransport ctor, as for StationURL's).
- **Unpinned neighbours found along the way:** `0x82B2F110..0x82B2FC38` (StationContactInfo, 2,808 B),
  `0x82B386B8..0x82B39320` (IOCompletionNotifier), and `0x82B41EE0..0x82B421C4` (`ConnectionData` helpers, see §2).
- **NAT/PRUDP `/GR`:** their vtables lack locators too, but this lane did not change their flags.
- Not merged to main.

Scratch: `~/tmp/w16od/` (`fnmap.py`, `resolve_map.py`, `ab.patch`, `ab_run.log`, `legA.json`, `legB.json`,
`below100.md`); the sub-lanes' tools are in `~/tmp/w16od-udp/`, `~/tmp/w16qs/`, `~/tmp/w16od-jcep/`,
`~/tmp/w16od-jcsep/` and the streams worktree.
