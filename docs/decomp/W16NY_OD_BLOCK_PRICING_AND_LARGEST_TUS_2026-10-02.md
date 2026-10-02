# W16-NY — pricing the Quazal `/Od` block as a lever, and writing its largest TUs (2026-10-02)

**Branch** `w16-ny`, on main `c42c979f8`. **Ruler** `name_check` (graded). Permuter not run. No alias group and no
`scripts/symbol_aliases.json` entry was touched (lane W16-NX owns those).

W16-NW (`docs/decomp/W16NW_STRINGCONVERSION_TU_AND_OD_RECARVE_2026-10-02.md`) wrote one `/Od` Quazal TU from the
retail asm and repaired the carve over the whole block `0x82A6D168..0x82B54190`. This lane prices the block and then
writes the largest TUs it can identify.

## 1. Pricing the block

### 1.1 What the block holds today (main `c42c979f8`, `report.json`)

| class | bytes | rows | matched B |
|---|---:|---:|---:|
| `auto_*` (unattributed) `.text` units starting in the block | 706,292 | 4,680 | 0 |
| pinned units overlapping the block (110 units) | 227,692 | — | 12,416 |
| of which ≤8-line map-only scaffolds (102 units) | 178,540 | — | 532 |
| **block total** | **933,984** | | **12,416 (1.33%)** |

The 12,416 matched bytes are all in the six units that have real source (MD5 6,792, StringConversion 4,424,
KeyedChecksumAlgorithm 532, BandwidthCounter 332, DuplicatedObject 312, MemoryManager 24). Every other pinned
unit in the block is a `namespace Quazal {}` scaffold pinned over the span of its `__FILE__` references.

So the block is 9.1% of `total_code` (10,247,792 B) and is 1.33% matched. Because 102 scaffold units already have
an object, their rows are **pairable now**; they score 0 because the object defines nothing. Writing a scaffold's
functions converts 0% rows to scored rows with no pin work. The `auto_*` 706 KB needs pins first.

### 1.2 Identifying the TUs

Scan of every `lis`/`addi`-style pair in the block that resolves to a `.cpp`/`.h` path string in `.rdata`
(`~/tmp/w16ny/paths.py`, retail `band.exe` bytes): **105 distinct `.cpp` path strings**, referenced from 269
functions, and 0 functions referencing two different `.cpp` paths. Each path string's referencing functions form
one contiguous run in `.text` (0 TUs seen in two separate runs). Header paths (`../Core/PseudoGlobalVariable.h`,
`../ObjDup/SelectionIteratorTemplate.h`, `../ObjDup/DOClassesTable.h`, `../Core/SystemComponents.h`,
`../Plugins/Checksum/MD5/MD5Checksum.h`) come from inline code and are not TUs.

The 105 path-anchored TUs do not cover the block. Their anchored functions span 189,560 B of 934,416 B; the rest is
their own unanchored functions plus TUs that reference no path string at all (six are already pinned by name:
DOClass, DOCoreTypes, RMCContext, LocalClock, KeyedChecksumAlgorithm, and the curl `ssluse.c` unit).

### 1.3 Extent bounds, and a control that rejected one estimator

For each TU, **lo** = bytes from its first to its last anchored function (a lower bound: these are certainly its
own), **hi** = bytes from the end of the previous TU's last anchor to the start of the next TU's first anchor (an
upper bound under the contiguity observed above). Neighbouring hi values share their gap, so **hi does not sum**.

Controls, on the two TUs whose true extent is known:

| TU | lo | true | hi |
|---|---:|---:|---:|
| StringConversion (W16-NW, exact) | 1,040 | 5,112 | 5,380 |
| MD5 (hand-extended pin) | 184 | 7,092 | 12,412 |

Both truths fall inside [lo, hi]. A third estimator was tried and **rejected by this control**: attribute each
unanchored function to the neighbouring TU whose own `.rdata` span contains its `.rdata` references (each TU's
`.rdata` is laid out in the same link order as its `.text`, verified by a monotone median over the block). It left
MD5 at 184 B and assigned four StringConversion functions to LANSessionDiscovery, so it is not used.

### 1.4 Table (sorted by the bracket midpoint)

`pinned B` and `matched B` are the unit's current `report.json` totals; `src lines` is the line count of the file
the unit compiles (7–8 = map-only scaffold).

| # | retail TU | first anchor | lo (anchor span) | hi (to neighbour anchors) | pinned B | src lines | matched B |
|---:|---|---|---:|---:|---:|---:|---:|
| 1 | `.\DuplicatedObject.cpp` | `82A72080` | 8,552 | 41,632 | 25,056 | 98 | 312 |
| 2 | `.\Transport\Interface\NATTraversalEngine.cpp` | `82B036B8` | 8,680 | 40,672 | 8,680 | 7 | 0 |
| 3 | `.\JobJoinSession.cpp` | `82AC8748` | 6,108 | 34,968 | 6,108 | 7 | 0 |
| 4 | `.\Transport\PRUDP\PRUDPEndPoint.cpp` | `82B31C78` | 12,992 | 28,056 | 12,992 | 7 | 0 |
| 5 | `.\Transport\Interface\HighLevelStream.cpp` | `82B4EDE8` | 524 | 40,104 | 524 | 7 | 0 |
| 6 | `.\AccountManagement\Client\JobLoginOrCreateAccount.cpp` | `82B0E468` | 1,192 | 36,136 | 1,192 | 7 | 0 |
| 7 | `.\Station.cpp` | `82A7C6F0` | 5,836 | 31,244 | 5,836 | 8 | 0 |
| 8 | `.\Transport\PRUDP\PRUDPStream.cpp` | `82AFC448` | 8,888 | 28,136 | 8,888 | 7 | 0 |
| 9 | `.\ObjDupProtocol.cpp` | `82A93AB8` | 12,060 | 24,408 | 12,060 | 7 | 0 |
| 10 | `.\Transport\Routing\RoutingStream.cpp` | `82B0A5D0` | 352 | 35,096 | 352 | 7 | 0 |
| 11 | `.\Session.cpp` | `82A78008` | 1,628 | 33,664 | 1,628 | 8 | 0 |
| 12 | `.\RootDODDL.cpp` | `82A99368` | 1,156 | 34,040 | 1,156 | 8 | 0 |
| 13 | `.\DOCallContext.cpp` | `82AA0E48` | 1,928 | 32,008 | 1,928 | 7 | 0 |
| 14 | `.\Core\StringStream.cpp` | `82AB3648` | 160 | 32,572 | 160 | 8 | 0 |
| 15 | `.\Transport\Interface\StationURL.cpp` | `82AA1658` | 7,960 | 24,248 | 7,960 | 8 | 0 |
| 16 | `.\Transport\UDP\UDPTransport.cpp` | `82B15FA0` | 6,184 | 25,960 | 6,184 | 7 | 0 |
| 17 | `.\StationDDL.cpp` | `82A82830` | 2,672 | 28,700 | 2,672 | 8 | 0 |
| 18 | `.\Core\ObjectThreadRoot.cpp` | `82AABE98` | 200 | 31,136 | 200 | 8 | 0 |
| 19 | `.\AccountManagement\Client\JobCreateAccount.cpp` | `82B13598` | 5,024 | 25,516 | 5,024 | 7 | 0 |
| 20 | `.\DupSpace\MatchOperation.cpp` | `82B48768` | 188 | 28,776 | 188 | 7 | 0 |
| 21 | `.\SessionClock\SessionClockDDL.cpp` | `82AD2548` | 1,560 | 26,276 | 1,560 | 8 | 0 |
| 22 | `.\SystemComponent.cpp` | `82AA7610` | 724 | 27,052 | 724 | 8 | 0 |
| 23 | `.\Core\BandwidthCounter.cpp` | `82AF71D8` | 320 | 26,848 | 932 | 54 | 332 |
| 24 | `.\Transport\UDP\QueuingSocket.cpp` | `82B39AB8` | 2,488 | 21,756 | 2,488 | 7 | 0 |
| 25 | `.\Competition\Protocol\TournamentDDL.cpp` | `82AE10A0` | 132 | 22,576 | 132 | 8 | 0 |
| 26 | `.\GlobalDiscovery\GlobalDiscoveryExtension.cpp` | `82B2C030` | 176 | 22,168 | 176 | 7 | 0 |
| 27 | `.\DupSpace\DupSpaceExtension.cpp` | `82B26DD8` | 548 | 21,636 | 548 | 7 | 0 |
| 28 | `.\Core\String.cpp` | `82A873C0` | 2,360 | 19,660 | 2,360 | 8 | 0 |
| 29 | `.\JobListenOnWellKnown.cpp` | `82ACE5B0` | 176 | 21,812 | 176 | 7 | 0 |
| 30 | `.\Core\EventHandler.cpp` | `82AF2388` | 1,020 | 20,860 | 1,020 | 8 | 0 |
| 31 | `.\Core\MemoryManager.cpp` | `82A6D428` | 1,860 | 19,924 | 2,036 | 90 | 24 |
| 32 | `.\Authentication\Client\TicketManager.cpp` | `82B3F530` | 1,060 | 20,520 | 1,060 | 7 | 0 |
| 33 | `.\StepSequenceJob.cpp` | `82AF9190` | 220 | 20,552 | 220 | 8 | 0 |
| 34 | `.\ChangeDupSetOperation.cpp` | `82ABDE90` | 232 | 20,384 | 232 | 7 | 0 |
| 35 | `.\CallRegister.cpp` | `82ABAC98` | 1,924 | 18,684 | 1,924 | 7 | 0 |
| 36 | `.\WorkerThreads.cpp` | `82B00210` | 604 | 19,948 | 604 | 8 | 0 |
| 37 | `.\DOCore.cpp` | `82AC0470` | 2,056 | 18,384 | 2,056 | 7 | 0 |
| 38 | `.\Stack\Core\BerkeleySocketDriver.cpp` | `82B3DD00` | 96 | 20,308 | 96 | 8 | 0 |
| 39 | `.\Transport\Interface\TransportSignatureGenerator.cpp` | `82B2FC40` | 612 | 19,436 | 612 | 7 | 0 |
| 40 | `.\MatchMaking\Protocol\GameSessionDDL.cpp` | `82AE5BB0` | 132 | 19,528 | 132 | 8 | 0 |
| 41 | `.\JobConnectStation.cpp` | `82AB6A58` | 3,240 | 15,964 | 3,240 | 7 | 0 |
| 42 | `.\Compression\ZLib\ZLibCompression.cpp` | `82B1B6D0` | 316 | 18,780 | 316 | 8 | 0 |
| 43 | `.\Core\MutexPrimitive.cpp` | `82A801F0` | 72 | 18,772 | 72 | 8 | 0 |
| 44 | `.\SecureTransport\JobConnectSecureEndPoint.cpp` | `82B42E30` | 1,116 | 16,856 | 1,116 | 7 | 0 |
| 45 | `.\StationManager.cpp` | `82AB87A0` | 3,424 | 13,592 | 3,424 | 7 | 0 |
| 46 | `.\Transport\Interface\JobConnectEndPoint.cpp` | `82B2C748` | 1,984 | 14,896 | 1,984 | 7 | 0 |
| 47 | `.\Scheduler.cpp` | `82AC57F0` | 948 | 15,896 | 8,500 | 31 | 0 |
| 48 | `.\Foundation\ProductFacade.cpp` | `82A8FDF0` | 3,592 | 13,224 | 3,592 | 7 | 0 |
| 49 | `.\ProtocolRequestBroker.cpp` | `82AAA140` | 184 | 16,340 | 184 | 7 | 0 |
| 50 | `.\Job.cpp` | `82ACF5A8` | 532 | 15,852 | 532 | 8 | 0 |
| 51 | `..\Services\Facades\Client\JobBackEndServicesLogin.cpp` | `82ADB5F8` | 6,688 | 9,456 | 6,688 | 7 | 0 |
| 52 | `.\Transport\UDP\PacketOut.cpp` | `82B23558` | 552 | 15,436 | 552 | 7 | 0 |
| 53 | `.\XboxLSP\Client\JobLSPLoginBypassSG.cpp` | `82AD5FA8` | 1,044 | 14,916 | 1,044 | 7 | 0 |
| 54 | `.\Transport\UDP\PacketIn.cpp` | `82B52708` | 248 | 15,564 | 248 | 7 | 0 |
| 55 | `.\Core\Result.cpp` | `82A8A058` | 660 | 14,552 | 0 |  | 0 |
| 56 | `.\SessionDiscovery\Interface\SessionDiscoveryTable.cpp` | `82AC2820` | 996 | 13,592 | 996 | 7 | 0 |
| 57 | `.\DupSpace\DefaultCellDDL.cpp` | `82B463A8` | 1,444 | 12,948 | 1,444 | 7 | 0 |
| 58 | `.\SessionDDL.cpp` | `82A91598` | 2,344 | 11,840 | 2,344 | 8 | 0 |
| 59 | `.\SessionDiscovery\LAN\LANSessionDiscovery.cpp` | `82AE73A8` | 3,468 | 10,608 | 3,468 | 7 | 0 |
| 60 | `.\SessionClock\SessionClockExtension.cpp` | `82B26A40` | 220 | 13,708 | 220 | 7 | 0 |
| 61 | `.\IDGeneratorDDL.cpp` | `82AF0140` | 1,552 | 12,344 | 1,552 | 7 | 0 |
| 62 | `.\Checksum\Interface\ChecksumAlgorithm.cpp` | `82B229E0` | 764 | 12,912 | 280 | 87 | 0 |
| 63 | `.\Transport\Interface\XboxConnectivityTester.cpp` | `82B36DF0` | 544 | 13,096 | 544 | 7 | 0 |
| 64 | `.\MatchMaking\Client\MatchMakingClient.cpp` | `82A8DDB0` | 920 | 12,548 | 920 | 8 | 0 |
| 65 | `.\Core\ByteStream.cpp` | `82A97EF8` | 2,208 | 10,388 | 2,208 | 8 | 0 |
| 66 | `.\Checksum\MD5\MD5.cpp` | `82B43BA0` | 184 | 12,412 | 7,080 | 245 | 6,792 |
| 67 | `.\PromotionRefereeDDL.cpp` | `82AC4290` | 1,540 | 11,048 | 1,540 | 7 | 0 |
| 68 | `.\Core\Log.cpp` | `82AEA8D0` | 1,356 | 11,180 | 1,356 | 8 | 0 |
| 69 | `.\Transport\Interface\Network.cpp` | `82AD8A40` | 4,848 | 7,080 | 4,848 | 7 | 0 |
| 70 | `.\Stack\Core\IOCompletionNotifier.cpp` | `82B383E0` | 716 | 10,752 | 716 | 7 | 0 |
| 71 | `.\JobProcessJoinRequest.cpp` | `82AEEBA0` | 432 | 10,848 | 432 | 7 | 0 |
| 72 | `.\MatchMaking\Protocol\GatheringDDL.cpp` | `82B20168` | 96 | 11,032 | 96 | 8 | 0 |
| 73 | `.\Transport\Interface\ConnectionManager.cpp` | `82AEC4C0` | 780 | 10,008 | 780 | 7 | 0 |
| 74 | `.\XboxLSP\Client\LSPBackEndServices.cpp` | `82A880A8` | 1,856 | 8,928 | 1,856 | 7 | 0 |
| 75 | `.\DupSpace\DuplicationSpaceTable.cpp` | `82B46FA0` | 3,100 | 7,588 | 3,100 | 7 | 0 |
| 76 | `.\CallContext.cpp` | `82A8C168` | 608 | 9,732 | 608 | 8 | 0 |
| 77 | `.\Foundation\JobTerminateFacade.cpp` | `82AE96E8` | 384 | 9,932 | 384 | 7 | 0 |
| 78 | `.\ChangeMasterStationOperation.cpp` | `82AB4808` | 200 | 10,068 | 200 | 7 | 0 |
| 79 | `.\AccountManagement\Client\AccountManagementClient.cpp` | `82AD7510` | 4,444 | 5,752 | 4,444 | 8 | 0 |
| 80 | `.\ParticipationManager.cpp` | `82AED5F0` | 164 | 9,056 | 164 | 7 | 0 |
| 81 | `.\XboxLSP\Client\JobLSPLogin.cpp` | `82AD6628` | 3,428 | 4,404 | 3,428 | 7 | 0 |
| 82 | `.\Core\BitStream.cpp` | `82B1E2E0` | 232 | 7,552 | 232 | 8 | 0 |
| 83 | `.\Core\StringConverter.cpp` | `82AF1E00` | 356 | 7,092 | 356 | 7 | 0 |
| 84 | `.\Core\Platform.cpp` | `82AAB9B0` | 100 | 7,248 | 100 | 8 | 0 |
| 85 | `.\Authentication\Client\KerberosAuthentication.cpp` | `82B1F6F8` | 768 | 6,548 | 768 | 7 | 0 |
| 86 | `.\Competition\Client\CompetitionClient.cpp` | `82A8C970` | 704 | 6,548 | 704 | 7 | 0 |
| 87 | `.\Authentication\Client\JobTicketManagerLogin.cpp` | `82B52D70` | 2,100 | 5,016 | 2,100 | 7 | 0 |
| 88 | `.\SecureTransport\SecureEndPoint.cpp` | `82B1D710` | 524 | 6,488 | 524 | 7 | 0 |
| 89 | `.\Core\StringConversion.cpp` | `82AE6898` | 1,040 | 5,380 | 5,052 | 379 | 4,424 |
| 90 | `.\Authentication\Client\AuthenticationClient.cpp` | `82B1C490` | 1,152 | 4,780 | 1,152 | 7 | 0 |
| 91 | `..\Services\Facades\Client\JobBackEndServicesLogout.cpp` | `82ADEC80` | 1,624 | 4,220 | 1,624 | 7 | 0 |
| 92 | `.\CallContextRegister.cpp` | `82AE0138` | 400 | 5,248 | 400 | 8 | 0 |
| 93 | `.\Core.cpp` | `82ADA2B0` | 2,032 | 3,500 | 2,032 | 8 | 0 |
| 94 | `.\SecureTransport\SecureStream.cpp` | `82ADD290` | 1,924 | 3,044 | 1,924 | 7 | 0 |
| 95 | `.\Encryption\RC4\RC4Encryption.cpp` | `82B14E68` | 296 | 4,504 | 296 | 7 | 0 |
| 96 | `.\DOClassesTable.cpp` | `82AB4148` | 376 | 4,304 | 376 | 7 | 0 |
| 97 | `.\Foundation\Client\ClientStreamManager.cpp` | `82ADDC50` | 196 | 3,916 | 196 | 7 | 0 |
| 98 | `.\Foundation\Protocol\NotificationEventManager.cpp` | `82ADE9B0` | 100 | 3,884 | 100 | 7 | 0 |
| 99 | `..\Services\Facades\Client\JobBackEndServicesTerminate.cpp` | `82ADFAB8` | 284 | 3,616 | 284 | 7 | 0 |
| 100 | `.\Authentication\Client\JobTicketManagerAcquireTicket.cpp` | `82B53BA0` | 468 | 3,028 | 468 | 7 | 0 |
| 101 | `.\SystemComponents.cpp` | `82B1C2F0` | 288 | 3,152 | 288 | 8 | 0 |
| 102 | `.\Foundation\Transport\StreamManager.cpp` | `82ADAB58` | 508 | 2,876 | 508 | 7 | 0 |
| 103 | `.\MatchMaking\Protocol\DynamicGatheringDDL.cpp` | `82AE5E00` | 96 | 3,140 | 96 | 8 | 0 |
| 104 | `.\Competition\Protocol\CompetitionDDL.cpp` | `82B1FDA0` | 96 | 1,884 | 96 | 8 | 0 |
| 105 | `.\Foundation\Protocol\DataDDL.cpp` | `82B14D48` | 116 | 1,284 | 116 | 8 | 0 |

### 1.5 The bracket against the seven extents this lane established

Every TU worked in §3 ended with a retail extent found by its lane. All seven fall inside [lo, hi], as did the two
controls in §1.3 (9 of 9):

| TU | lo | true extent | true B | hi |
|---|---:|---|---:|---:|
| StationURL | 7,960 | `82AA1658..82AA6270` | 19,208 | 24,248 |
| JobBackEndServicesLogin | 6,688 | `82ADB5F8..82ADD088` | 6,748 | 9,456 |
| NATTraversalEngine | 8,680 | `82B03480..82B07280` | 15,584 | 40,672 |
| PRUDPEndPoint | 12,992 | `82B31AB8..82B358E8` | 15,720 | 28,056 |
| PRUDPStream | 8,888 | `82AFB6E0..82AFFEC0` | 18,076 | 28,136 |
| ObjDupProtocol | 12,060 | `82A937E0..82A97EF0` | 18,056 | 24,408 |
| DuplicatedObject | 8,552 | `82A6FC78..82A76B58` | 28,164 | 41,632 |

true/lo runs 1.01–3.29 (median ≈ 1.8), so the 105 path-anchored TUs hold roughly 189,560 × 1.8 ≈ 340 KB of the
block. That figure is only as good as a 9-point ratio. Every scaffold pin is an under-carve: it covers only the span
of the `__FILE__` references, and all seven lanes had to extend theirs.

## 2. Flags: `/Od /Ob1`, and a prediction of mine that was right after all

A probe compile (`~/tmp/w16ny/probe/p.cpp`) showed that plain `/Od` never expands an `inline` or `__forceinline`
helper, while `/Od /Ob1` reproduces the retail shape (an inline function's argument stored to a temp slot and
reloaded at every use, e.g. the StationURL ctor at `0x82AA1658`). Five earlier Quazal ports already carried `/Ob1`.

I then predicted that `/Ob1` would hold or gain on DuplicatedObject, which was plain `/Od`, and measured unit matched
bytes **312 → 204**. On that reading I told the lanes `/Ob1` was per-TU. **That reading was wrong.** The DuplicatedObject
lane showed that retail expands `map::end`, the `DOHandle` copy, `ScopedCS` and `Scheduler::GetInstance` in place. The
108 B were three rows that paired only because plain `/Od` emitted out-of-line copies: `_S_right` at `0x82AB0850`,
`map::end` at `0x82AFC2A8` (really `PRUDPStream::Teardown`), and `~ScopedCS` at `0x82A707E8` (really
`DuplicatedObject::GetDOClass`). **All seven TUs here build with `/Od /Oi- /Ob1`.** Three add `/EHs-c-`
(DuplicatedObject, ObjDupProtocol, JobBackEndServicesLogin) and JobBackEndServicesLogin adds `/GR-`, each read from
the TU's asm (EH records, RTTI locators). Only StringConversion (free functions, no inline helpers) cannot tell
`/Ob1` from plain `/Od`.

Every lane independently found two `/Od /Ob1` facts that decide frames:

- **A declined inline still reserves its frame in the caller.** When `/Ob1` will not expand an in-class function
  (it has named locals, or it is a second level of inlining), the call is out of line, but the caller still
  reserves the callee's parameter and local slots. This explains every "unexplained gap" in these frames.
- **Local names decide slot order** (W16-NW §2.5).

## 3. Per-TU results

Seven TUs, dispatched to one lane each, largest guaranteed size (lo) first: PRUDPEndPoint, ObjDupProtocol,
PRUDPStream, NATTraversalEngine, DuplicatedObject, StationURL and JobBackEndServicesLogin. They were then merged
here. Per-lane write-ups: `W16NY_OBJDUPPROTOCOL_TU_2026-10-02.md`, `W16NY_DUPLICATEDOBJECT_TU_2026-10-02.md` (the
other lanes' per-function tables are in their final reports, summarized below).

### 3.1 Units, main vs this branch (integrated tree, `report.json`, graded ruler)

| TU | flags | main: rows at 100 / rows | main: matched / unit B | branch: rows at 100 / rows | branch: matched / unit B |
|---|---|---:|---:|---:|---:|
| StationURL | `/Od /Oi- /Ob1` + 2 per-TU `/D` | 0 / 71 | 0 / 7,960 | 105 / 122 | 16,296 / 19,208 |
| NATTraversalEngine | `/Od /Oi- /Ob1` | 0 / 69 | 0 / 8,680 | 72 / 147 | 12,040 / 15,580 |
| DuplicatedObject | `/Od /Oi- /Ob1 /EHs-c-` | 8 / 95 | 312 / 25,056 | 68 / 120 | 12,136 / 29,940 |
| PRUDPEndPoint | `/Od /Oi- /Ob1` | 0 / 64 | 0 / 12,992 | 67 / 98 | 10,372 / 15,716 |
| PRUDPStream | `/Od /Oi- /Ob1` + `/DRB3_QUAZAL_MEMALLOCATOR_CTORS` | 0 / 69 | 0 / 8,888 | 87 / 147 | 9,812 / 18,068 |
| JobBackEndServicesLogin | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 0 / 21 | 0 / 6,688 | 20 / 22 | 6,136 / 6,748 |
| ObjDupProtocol | `/Od /Oi- /EHs-c- /Ob1` | 0 / 53 | 0 / 12,060 | 44 / 81 | 5,700 / 18,056 |
| **total** | | | **312** | | **72,492** |

Row counts include EH funclets. Most funclets pair by byte signature, which is the +259 `masked_equal` in §4.

### 3.2 What each lane established

- **StationURL** (`0x82AA1658..0x82AA6270`): the TU continues past the last `__FILE__` reference through
  Parse/ParseParam, the key accessors and the `qMap<String,unsigned>` / `qMap<String,String>` tree code. StationURL
  is 0x64 bytes with no vtable. The four constructors stay at 42–80%: retail expands the three map members
  differently in each one, and no allocator variant reproduced all three.
- **NATTraversalEngine** (`0x82B03480..0x82B07280`): self-contained, with local declarations at the retail layouts. A
  class with a vfptr and a `Time` member pads the vfptr to 8. `GetPublicURL` calls `0x823EBC90`, whose map name
  is an anonymous-namespace function (a fold question).
- **DuplicatedObject** (`0x82A6FC78..0x82A76B58`, plus the outlying `0x82AB0850` block): written by four sub-lanes.
  Three map names were wrong and are corrected, each from its retail callers: `0x82A707E8` is
  `GetDOClass(unsigned)`, not `~ScopedCS`; `0x82A75288` is `AddToCachedDuplicationSet`; `0x82A75458` is
  `RemoveFromDuplicationSet`, and the real `IsInDuplicationSet` is `0x82A75390`. Ten rows are capped by calls to
  fold-survivor names (`0x823EA598`, `0x82A478D0`), and five by inline jump-table words that the target object holds
  as raw data.
- **PRUDPEndPoint** (`0x82B31AB8..0x82B358E8`): `0x82B35778`'s carve swallowed a pad word (0x38 → 0x34).
  `ServiceIncomingPacket` (2,912 B, 97.27) has a nested handler inline that our compiler will not expand.
  `SetKeepAliveTimeout` is six reserved words short.
- **PRUDPStream** (`0x82AFB6E0..0x82AFFEC0`): the TU's own functions end at `Trace`, followed by its COMDAT tail.
  `0x82AFC2A8` (mapped `map<DOHandle,…>::end`, 100 on main) is retail vtable slot 21 and returns `0x10001`, so it is
  `PRUDPStream::Teardown`. The re-split dropped `fn_82AFEF08`, an 8-byte phantom over an EH prefix.
- **JobBackEndServicesLogin** (`0x82ADB5F8..0x82ADD088`): 21 of 22 at 100 once `0x82A6DE40` is named.
  `Complete(qResult)` needs a 12-byte reservation from an inline `BackEndServices::Logout` whose body we do not hold,
  and the lane refused to fit one.
- **ObjDupProtocol** (`0x82A937E0..0x82A97EF0`): **renamed `0x82A6DE40` from `XShowSocialNetworkImagePostUI` to
  `RootObject::operator new(size_t, const char*, unsigned)`**, because its body allocates with file and line. Every
  Quazal TU calls it, and the rename also moved four rows in MemoryManager, Scheduler and BandwidthCounter.
  `ParseSpecificMessage` holds an inline jump table of 22 absolute addresses, which the target has as raw words.

### 3.3 Integration: shared headers and one spelling per function

Seven lanes writing against the same Quazal headers in parallel produced conflicts. Each was settled by measurement
on the merged tree:

- **`qMemAllocator.h`.** PRUDPStream removed `~MemAllocator` and switched to converting constructors for all X360
  TUs. That made JobBackEndServicesLogin's `ConnectStream` inlinable (100 → 0, not emitted) and cost its ConnectTo*
  rows and StationURL's constructors. Main's shape is now the default, and PRUDPStream opts in with
  `/DRB3_QUAZAL_MEMALLOCATOR_CTORS`. StationURL already gated its own variant behind `/DRB3_QUAZAL_RETAIL_MEMALLOCATOR`.
- **`ScopedCS.h` / `CriticalSection.h`**: DuplicatedObject's version (`critSec->Enter()` with an inline gate). The
  other lanes adopted it with identical scores.
- **`Core.h` / `Scheduler.h`.** Two shapes, measured whole-binary on the merged tree:
  DuplicatedObject's (`Core::GetInstance` with locals, if/else `Scheduler::GetInstance`) **52,181 / 5,714,832 B**
  vs main's **52,178 / 5,711,320 B**. Main's shape gains PRUDPStream rows and costs 13 DuplicatedObject rows
  (SendStubMessage, UndeleteMainRef, the ctor and others). DuplicatedObject's shape is kept. Retail's
  `0x82A6F650` body also matches it, while main's ternary-plus-dead-code does not. **No single shape satisfies both
  that body and PRUDPStream's caller reservations**, so 12 PRUDPStream rows (and their funclets) sit at 99.6–99.96
  for want of a third shape. Gating the two shapes per TU was not done: that would give one inline function two
  definitions, which is not a source.
- **`RootObject.h`: `~RootObject() {}` is kept.** Two lanes found retail's unwind actions calling an empty
  RootObject destructor. Removing it (E1) measured **+1 fn / +3,248 B**: five DuplicatedObject rows reach 100
  (`MigrationInProgress`, `EmigrateTo`, `Publish`, `Create`, `AddToCachedDuplicationSet`, where retail expands
  `GetHandle()` and ours stops expanding it). But it sends 16 PRUDPEndPoint/PRUDPStream funclets to 0, because
  their call disappears, and moves four StationURL funclets 99.5 → 73.5. That trade removes a retail-evidenced
  construct for a headline gain, so it was not taken. Two follow-ups did not find the mechanism: E3 (`DOHandle` not
  derived from `RootObject`) and E4 (no explicit `~DOHandle`) both measured Δ0. **What lets retail inline
  `GetHandle` with that destructor present is open.**
- **One spelling per retail function.** Callees were placeholders (forgiven) on main. Once a TU's own map named
  them, every other lane's guessed spelling became a charge. Callers now use the name the owning TU maps: StationURL
  (`SetRVConnectionID` is the `"RVCID"` setter at `0x82AA3018`, decided by the key string each setter loads),
  `PRUDPStream::Send`/`ReleaseEndPoint` (Wii-attested, in source order), `DORefTemplate<T>::IsValid`, and
  DuplicatedObject ↔ ObjDupProtocol method names. **One of these was a real logic defect:** NAT's
  `GetUpdatedURL` called `0x82AA3218` as `IsEqual`, but that body is `strcmp(...) != 0` (`operator!=`), so the
  name stated the inverse of what the code does. It now reads `m_oURL != url`. The same applies outside the block:
  `SessionJobs_Xbox`'s join-target static is a `Quazal::StationURL`.

## 4. Whole-binary A/B

`ab_measure` refuses a patch that touches `symbols.txt`, so, as in W16-NW, the change is measured in two legs in one
fresh `setup_worktree.sh` worktree (`~/tmp/wt-w16-ny-ab`) on main **`871b44290`**, after the branch was rebased
onto it. An earlier identical pair on main `d792b486f` (before the SessionJobs and comment commits) gave the same
deltas.

### 4.1 Leg 0: main → main + this branch's `symbols.txt`

The change is two lines: the phantom `fn_82AFEF08` is removed, and `0x82B35778` goes 0x38 → 0x34. Settled
builds (the settle build did no compile or split work), `report.json` + `report.cache` wiped before each read.

**Prediction:** Δmatched 0 and Δmatched_code 0, `total_functions` −1, `total_code` −12. **Measured: Δ0 on every key**
(51,607 / 24,645 masked / 5,643,848 B / 68,914 rows / 10,247,792 B on both legs, and 0 rows moved). The totals
prediction was wrong because `report.json` already billed `fn_82B35778` at 52 B and never listed `fn_82AFEF08` as
a row.

### 4.2 Leg 1: `ab_measure --patch` (pins, `objects.json`, sources, headers, map) on the leg-0 base

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ny-ab --patch ~/tmp/w16ny/ab2.patch`, run dir
`~/tmp/wt-w16-ny-ab/.ab_measure_runs/20261002-215541-ab2-3345087/`.

**Prediction, written before the run:** Δmatched +582, Δmatched_code +72,656 B, from the integrated tree's reading
against the first A/B.

```
leg A: matched=51607 masked=24645 honest=26962 code%=55.073800  (recompiles: 0, settled)
leg B: matched=52189 masked=24904 honest=27285 code%=55.782787  (recompiles: 1412, split=1, patch_steps=13, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+582  Δmasked_equal=+259  Δhonest=+323  Δcode%=+0.708987pp  Δcode_bytes=+72656
Δfuzzy=+1.134963pp   (legA 60.492817 -> legB 61.627780)
unit net (ALL units) = +582   vs whole-binary Δmatched = +582
```

**Measured exactly as predicted.** Per unit: NATTraversalEngine +146, PRUDPStream +114, StationURL +112,
PRUDPEndPoint +83, DuplicatedObject +60, ObjDupProtocol +45, JobBackEndServicesLogin +20, and MemoryManager +2
(the `0x82A6DE40` rename). `masked_equal` +259 is mostly EH funclets pairing by byte signature, so the honest
gain is **+323 functions**.

### 4.3 Rows that went down (leg A → leg B, keyed by unit and row name)

| row | size | fuzzy | reason |
|---|---:|---|---|
| `NetSession::Poll` | 428 | 98.944 → 94.645 | Already below 100. It holds a `Quazal::Time` temp, so it feels the retail-evidenced `~RootObject() {}` (§3.3). |
| DuplicatedObject `_Rb_tree<DOHandle,…>::_S_right` @ `0x82AB0850` | 16 | 100 → 0 | Outside the TU's retail span and never called from it. It paired only because plain `/Od` emitted an out-of-line copy. |
| DuplicatedObject `fn_82AB0CDC` | 40 | 100 → 74 | EH funclet of STLport string code in the same outlying block. It paired by byte signature with funclets of the old `/EHsc` build, and the TU is `/EHs-c-` now. |
| DuplicatedObject `fn_82AB0D44` | 44 | 99.9 → 0 | Same. |

Two rows at 100 disappear under their old names and are at 100 under the corrected ones: `??1ScopedCS` @
`0x82A707E8` → `DuplicatedObject::GetDOClass` (40 B), and `map<DOHandle,…>::end` @ `0x82AFC2A8` →
`PRUDPStream::Teardown` (52 B). Net cost of the downs: 56 B (`_S_right` + `fn_82AB0CDC`), already inside the
+72,656 B.

### 4.4 Measured alternatives not taken (on the merged tree, before the final A/B)

| change | Δ fns / Δ B | why not |
|---|---|---|
| main's `Core.h`/`Scheduler.h` instead of DuplicatedObject's | −3 / −3,512 | §3.3 |
| remove `~RootObject() {}` (E1) | +1 / +3,248 | removes a retail-evidenced destructor; 16 funclets lose their call (§3.3) |
| `DOHandle` without `RootObject` base (E3) | 0 / 0 | no effect |
| no explicit `~DOHandle` (E4) | 0 / 0 | no effect |
| `GetInstanceFromVector` without `__declspec(noinline)` (E2) | −5 / +216 | 5 ObjDupProtocol rows reach 100; 3 named PRUDPStream rows (Open/CloseEndPoint), 2 PRUDPStream funclets and DuplicatedObject `DeleteDuplicaMainRef` leave it |

## 5. Native gate

Run last, on the code at `6d661c067` (only this docs edit follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 6. Not done

- **The rest of the block.** 98 path-anchored TUs and every path-less TU remain scaffold or `auto_*`. The block is
  now ~9.1% of `total_code` at ~9% matched.
- **Fold/alias charges, left for the alias lane:**
  - 132 funclet call sites name `~RootObject`/`~Time`/`~ScopedCS`, where the map has the empty-body fold survivor
    `StlNodeAlloc` ctor.
  - `0x823EBC90`/`GetInstanceType1Delegator` (ObjDupProtocol ×5, NAT ×3, PRUDPStream, PRUDPEndPoint).
  - `0x823EA598` (`reverse_iterator`, DuplicatedObject ×4).
  - `0x82AFF0F0` (`~InetAddressList` vs NAT's `~qList<StationURL>`, 2 NAT funclets at 99.5).
  - `_List_base<T*>::clear` instantiations.
- The third `Core`/`Scheduler` shape (§3.3), and the `~RootObject`/`GetHandle` question.
- StationURL's four constructors; `ServiceIncomingPacket`; `BackEndServices::Logout`'s body;
  DuplicatedObject's `Trace` (unwritten).
- `InstanceTable::GetInstanceFromVector` is `__declspec(noinline)` (DuplicatedObject's lane). ObjDupProtocol's
  frames want it inline-but-declined (12 bytes): five of its rows sit at 99.7–99.87 here. E2 (§4.4) trades them for
  PRUDPStream/DuplicatedObject rows, so it is unresolved. Like §3.3, a single shape satisfying both is missing.
- Names: the method names for X360-only functions are chosen by these lanes and are not retail-attested. Names
  that are Wii-attested are marked as such in the lane reports.
