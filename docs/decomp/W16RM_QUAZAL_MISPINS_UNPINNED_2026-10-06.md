# W16-RM: Quazal and net-messenger code unpinned from game and engine units (2026-10-06)

Branch `w16-rm`, worktree `~/tmp/wt-w16rm`, off main `f7cdbf7ca`. Follow-up to W16-RI §"Recorded, not written"
(`W16RI_ZERO_PERCENT_ROWS_2026-10-06.md`), which found Quazal code pinned at 0x823E9xxx–0x823F5xxx and at the Voice
initializers 0x82C42958–0x82C42D68 and left it for a map lane.

**Result:** 26 `.text` pins, 151 functions and 12,212 B moved out of 13 game and engine units into `auto_*` units.
No block fitted an existing Quazal unit, so nothing was re-homed into one. No source was written. Commit `532a781c0`.

## 1. How the blocks were found

All instruments read retail bytes (`orig/45410914/band.exe`, clean TU5). Scripts are in `~/tmp/w16rm/` and are not
committed.

| channel | what it flags |
|---|---|
| vtables (`tools/retail_rtti.py`) | a function that appears only in `@Quazal@@` vtables, or that installs one |
| call graph (`bl`/`b` over all of `.text`) | callers or callees in Quazal units (`network/{quazal,Core,Platform,Plugins,ObjDup}`) or in the unpinned `/Od` Quazal block 0x82A6D168–0x82B54190 |
| `__FILE__` strings | a function that references a source path whose basename is not its unit's |
| static data (lis + addi/D-form xref) | an initializer whose static is used only by Quazal code |

I ran these over every function in a pinned non-Quazal, non-XDK unit, grouped the hits by pinned block, and read each
flagged block by hand. Some hits are legitimate, and §4 lists the ones I kept. A hit counted as a mis-pin only when the
block lies away from its unit's own code, among other TUs' code, and something in the bytes ties it to another TU.

## 2. Blocks moved

### 2.1 Net-messenger layer, 0x823F3E24–0x823F56F8 (between MatchmakingSettings and NetSearchResult)

| unit | pins | B |
|---|---|---:|
| Matchmaker.cpp | `823F3E24-823F3EE0` | 188 |
| WavMgr.cpp | `823F3EE0-823F3EFC`, `823F3F00-823F4168` | 644 |
| TrackPanelDir.cpp | `823F4168-823F4268`, `823F43A0-823F43C0`, `823F43C0-823F4590` | 752 |
| ContextChecker.cpp | `823F4268-823F43A0` | 312 |
| CheatProvider.cpp | ten pins from `823F4590` to `823F5334` | 1,872 |
| UI.cpp | `823F4A30-823F4A58`, `823F4C38-823F4C74`, `823F4D58-823F4E0C`, `823F4FA0-823F52BC`, `823F5334-823F56F8` | 2,040 |

Evidence:
- `823F3E30`, `823F3E84`, `823F4498` and `823F4508` reference the string `".\MessageBrokerDDL_Xbox.cpp"`. That is the
  Xbox counterpart of rb3-Wii's `network/net/MessageBrokerDDL_Wii.cpp`, and no unit for it exists.
- 17 functions sit only in the vtables of `_DOC_MessageBroker@Quazal`, `_DO_MessageBroker@Quazal`,
  `DOClassTemplate<_DO_RootDO,DOClass>@Quazal`, `HarmonixGameDDLDeclarations@Quazal` or
  `RockBandDDLDeclarations@Quazal`. Six more install those vtables.
- `823F5270` is slot 0 of `.?AVMessageBroker@@` (vtable 0x8205982C). `823F4FA8` and `823F50B0` install it.
- Every caller of the stretch is inside it or in the net layer (`Net.cpp`, `NetSession.cpp`, `QuazalSession.cpp`,
  the 0x823EA4E8 run in §2.2, and the RockBandDDL code in §2.4). The real bodies of WavMgr, TrackPanelDir,
  CheatProvider, UI, Matchmaker and ContextChecker are hundreds of KB to 2 MB away.

### 2.2 Quazal template run, 0x823EA4E8–0x823EAD68 (between SyncStore and SessionSearcher)

| unit | pins | B |
|---|---|---:|
| SyncStore.cpp | `823EA468-823EA958` shrunk to `823EA468-823EA4E8` (keeps `Load@SyncObjMsg`, 100%) | 1,136 moved |
| SongData.cpp | `823EA958-823EA984` | 44 |
| WavMgr.cpp | `823EA988-823EAD68` (its last block, so the heading is removed) | 992 |

Evidence:
- `823EA758` and `823EA7CC` reference `c:\prj\band3_patch5\network\src\ObjDup/SelectionIteratorTemplate.h`.
  Quazal's own TUs include that header by a relative path (`../ObjDup/...`, as at `82A76970`). The absolute path
  means a Harmonix TU instantiated the template, and that TU is not SyncStore, which includes no Quazal header.
- The helpers are called mostly by Quazal code. For example, `823EA910` has 232 callers and `823EA8A0` has 59.
- `823EAB88` is called 12 times by NetSession. It reads `Station::GetLocalInstance` and `GetStationID` and writes a
  `MemStream`. It is a net-layer function, not a WavMgr one.
- The SyncStore TU ends at `823EA4E4`: `Load@SyncObjMsg` is the last row there that our SyncStore.obj pairs at 100.

### 2.3 RB protocol DDL clients, 0x8250A510–0x8250AF88 (between XboxEntityUploader and DataResults)

`RockCentral.cpp`, pin `8250A510-8250AF88`, 2,680 B.
- It holds the vtable-only methods of `RBDataClient@Quazal` (`8250A510`), `RBBinaryDataClient@Quazal` (`8250AAA8`)
  and `RBTestClient@Quazal` (`8250AF08`), plus their marshalling helpers.
- rb3-Wii puts these in separate TUs: `RBDataDDL_Wii.cpp`, `RBBinaryDataDDL_Wii.cpp` and `RBTestDDL_Wii.cpp`.
- RockCentral's own code is at 0x824F64D0–0x824FB3D0. In this pin only the 40 B EH funclets scored, and they paired by
  byte signature.

### 2.4 RockBandDDLDeclarations, 0x82B8FED8–0x82B8FF80

`band3/bandtrack/TrackPanel.cpp`, pin `82B8FED8-82B8FF80`, 168 B.
- `82B8FED8` is the only slot of `RockBandDDLDeclarations@Quazal`'s vtable that is not shared.
- `82B8FF20` is called only by ProductFacade, and it registers the `"RockBand"` product (rb3-Wii:
  `band3/net_band/RockBandDDF_Wii.cpp`).

### 2.5 Quazal static initializers, 0x82C42910–0x82C42DA0

| unit | pin | B |
|---|---|---:|
| System.cpp | `82C42910-82C42958` | 72 |
| Voice.cpp | `82C42958-82C42DA0` | 1,096 |

Evidence:
- Each function constructs one static in `.data` at 0x82E11C90–0x82E11E98 and registers its `atexit`. The static
  names include `"PRUDP Sliding Window"`, `"Packet Quantity"` and `"PRUDP Dispatch Queue"`. The constructors are in the
  Quazal block.
- Every non-initializer user of each static is in the unpinned Quazal block (0x82B25F78–0x82B54168). One static,
  0x82E11D30, is also read by `UDPTransport.cpp`. But its initializer sits between two whose statics belong to the
  0x82B37xxx and 0x82B46xxx TUs, so its definer is the unpinned 0x82B3Cxxx TU, not UDPTransport. No pinned Quazal unit
  owns any of them.
- `82C42910` stores two Quazal vtables with no COL (`/GR-`), 0x8217EADC (in ObjDupProtocol's `.rdata`) then
  0x8218A0E8. It is not System.cpp's initializer; System.cpp's are at `82C3F770` and `82C458A0`.
- The matching `??__F` thunks (0x82C4B4E0–0x82C4B7D8) were already unpinned. Voice's own `??__F` run starts at
  `82C4B7E8` on Voice's data at 0x82E120D4.
- `82C42D68` initializes a `CPartyLib` static. That static is used only by code at 0x82B54100–0x82B54168, inside the
  Quazal block, so it is not Voice's either.

### 2.6 Quazal code in the curl unit, 0x82AE51D0–0x82AE52A8

`ssluse.c`, pin `82AE51D0-82AE52A8`, 216 B. This was the unit's only block, so the heading is removed. W16-OF flagged
it.
- It allocates through Quazal's `MemoryManager` and stores vtable 0x82184894.
- That vtable sits in `.rdata` after RankingDDL's file string and a `"DynamicData"` name. It sits *before*
  GameSessionDDL's file string, and in this layout a TU's file string precedes its vtable (W16-OF §2). So the owner is
  the unpinned TU between RankingDDL and GameSessionDDL, not GameSessionDDL. A naive string read of 0x82184894 runs into
  `".\MatchMaking\Protocol\GameSessionDDL.cpp"`, and taking that at face value would have re-homed the block to the
  wrong Quazal unit.

## 3. Map corrections (moved rows only)

- **`0x823f5270`: `??_GAutomator@@UAAPAXI@Z` → `??_GMessageBroker@@UAAPAXI@Z`.** Retail has no `.?AVAutomator@@`
  descriptor (0 occurrences, against 1 for `.?AVMessageBroker@@`), and this address is slot 0 of MessageBroker's
  vtable.
  - The ICF alias group at this address has `folded: []`, so it forgives nothing.
  - `tools/alias_survivor_relabel.py --write` relabelled its survivor; the build's survivor-drift check requires that.
    The old spelling is held as UNDECIDABLE.
  - I corrected the tool's hard-coded `"lane": "W16-OS 2026-10-03"` to this lane.
- **`0x823ea958`: `?GetDataAtTick@?$RangedDataCollection@I@@QAAIHH@Z` withdrawn.** The body is
  `p = fn_823EA910(); return p ? p->+8 : 0`. That callee has 232 callers, mostly Quazal, and this function's only
  caller is at 0x82B30648 in the Quazal block. The row now reads `fn_823EA958`.

Neither address has a `bl` caller in a paired unit, so neither edit moves a call-site charge.

## 4. Flagged but kept (not mis-pins)

- **RockCentral `824F6680-824F6C10` and `824F6EC4-824F70B0`.** The `RBBinaryBuffer`/`_DDL_RBBinaryBuffer`
  ctor/dtor/`??_G` and `ClientProtocol` inlines here are header-inline COMDATs emitted by RockCentral's own TU. They are
  interleaved with RockCentral's code, and our RockCentral.obj pairs them at 100.
- **QuazalSession `823F2A60`/`823F2A70`.** These are the inline `Quazal::OperationCallback` base ctor/dtor emitted in
  QuazalSession's TU.
- **ContextWrapper `8250BFB0-8250C0C8`.** It is HMX code that calls Quazal; every row is at 100.
- **Server, NetSession, NetSession_Xbox, SessionJobs_Xbox, RockCentral and DataResults rows that only call Quazal.**
  This is HMX net code. The three `??__F` thunks of function-local `Quazal::String` statics (`82C44E68`, `82C44FE8`,
  `82C455C0`) belong to their HMX functions.
- **Shared ICF fold stubs that appear in both Quazal and non-Quazal vtables** (`GetRate@RndAnimatable`,
  `IsDirPtr@ObjDirPtr`, the 4-byte `StlNodeAlloc` stub, and others). These are fold survivors, and their position says
  nothing about Quazal.
- **Not Quazal, so left alone:**
  - ProfileMgr `82B8FC88-82B8FED8` (UGCNetResource vtable methods next to the RockBandDDL block).
  - The small `?Type@…Msg`/STL pins in the net region (RockCentral `823EC310`, ProfileMgr `823EC398`, SongDB
    `823F14C0`, MemMgr `823F1898`, Watcher `823F2130`).

## 5. Measured

**Whole binary.**

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16rm --from-dirty --label w16rm`. Run dir:
`~/tmp/wt-w16rm/.ab_measure_runs/20261006-220738-w16rm-822480/`. Kinds: map, splits. Both legs were at the split fixed
point, objdiff was stable, and the ruler was `name_check`.

```
leg A: matched=54750 masked=25220 honest=29530 code%=58.883920
leg B: matched=54730 masked=25203 honest=29527 code%=58.876617
Δmatched=-20  Δmasked_equal=-17  Δhonest=-3  Δcode%=-0.007303pp  Δcode_bytes=-748
units at 100% [mpn]: 590 -> 591 (ContextChecker, DENOMINATOR_SHRANK)
```

**Expected sign.** Moving rows out of paired units can only lose pairings, and every block above was there by
misattribution.

**Row diff of the archived legs.**
- 164 rows left and 164 appeared (14,456 B each way). 14 of these are existing `auto_*` units renamed at new gap
  edges.
- 0 rows changed score, and no row that appeared scores above 0.
- The 19 rows that were at fuzzy 100 (748 B) were:
  - 16 EH funclets (40 B, or 12/44 B) that paired by byte signature, all `masked_equal`;
  - `??_GAutomator` at `823F5270` (76 B);
  - `GetDataAtTick<unsigned>` at `823EA958` (44 B);
  - `reverse_iterator<Synchronizable**>` copy ctor at `823EA598` (12 B, a generic fold survivor called from Msg,
    Locale and Quazal code).
- The three non-funclet rows are the Δhonest −3. Each was a byte-twin name pairing a body from another TU.

`total_code` (10,247,844) and `total_functions` (68,884) are unchanged.

**Ceiling and in-scope gap** (W16-PN's `scope_ledger2.py`/`gengap.py`, copied to `~/tmp/w16rm/ledger/`; run on
builds whose headline keys equal leg A and leg B exactly). In-scope = IN-CORE + IN-SOON + IN-RB3ENG.

| | before (leg A) | after (leg B) | Δ |
|---|---:|---:|---:|
| reachable ceiling | 6,657,580 | 6,646,064 | −11,516 |
| gap to ceiling | 623,248 | 612,480 | −10,768 |
| **in-scope gap** | **227,264** | **223,476** | **−3,788** |
| IN-SOON gap / reach | 115,108 / 2,441,924 | 112,064 / 2,438,636 | −3,044 / −3,288 |
| IN-RB3ENG gap / reach | 67,812 / 926,748 | 67,140 / 926,036 | −672 / −712 |
| IN-CORE gap / reach | 44,344 / 478,244 | 44,272 / 478,172 | −72 / −72 |
| VIA-DC3 gap | 218,948 | 214,104 | −4,844 |
| OUT-360-OTHER gap | 53,260 | 52,200 | −1,060 (ssluse.c scaffold −216 B also left) |
| OUT-NET gap | 5,444 | 4,368 | −1,076 |

The in-scope share went from 94.09% to 94.18% of the in-scope ceiling (3,619,652 / 3,846,916 → 3,619,368 / 3,842,844). This change is a correction to the denominator
and does not come from matching anything.

## 6. Not done

- No Quazal source or reverse engineering, and no new unit headings. The TU identities in §2 are recorded for a future
  pinning lane: MessageBrokerDDL_Xbox.cpp is attested by its file string; the RB*DDL, RockBandDDF and "DynamicData"
  identities rest on rb3-Wii file names and `.rdata` adjacency.
- No alias admitted or withdrawn beyond the survivor relabel in §3.
- Native gate not run. The change touches only `splits.txt`, `target_symbol_map.json` and `symbol_aliases.json`;
  no `src/` file and no header.
- No merge, no push.
