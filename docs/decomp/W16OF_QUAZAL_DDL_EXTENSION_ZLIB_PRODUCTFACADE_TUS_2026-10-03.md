# W16-OF — eight Quazal `/Od` TUs (DDLs, extensions, ZLib, ProductFacade) written from the retail asm (2026-10-03)

**Branch** `w16-of`, rebased onto main `24a47ebd2`. **Ruler** `name_check` (graded). Permuter not run. No
`scripts/symbol_aliases.json` entry, no `symbols.txt` line and **no shared header** touched.

Method and TU table: `docs/decomp/W16NY_OD_BLOCK_PRICING_AND_LARGEST_TUS_2026-10-02.md` (rows 21, 25, 26, 27, 29,
40, 42, 48). The sub-lane-per-region split is W16-OE's (`W16OE_QUAZAL_ACCOUNT_AND_OBJDUP_TUS_2026-10-02.md` §1).
Lanes W16-OC (session/station), W16-OD (transport) and W16-OG (core) worked other TUs of the block in parallel and
all three landed while this lane ran (`36d8bd9f3`, `7431336d6`, `24a47ebd2`). Every sub-lane rebased onto `7431336d6`
and re-read its rows. The integrated branch was then rebased onto `24a47ebd2` and measured there (§3).

## 1. How the work was split

The eight TUs lie in five address regions. I took each region's outer bound from W16-NY's anchor table
(`~/tmp/w16ny/tus.json`) and from the pins on main, so no two lanes shared a boundary. Two bounds were set by
W16-OD's then-unlanded pins (UDPTransport ending at `0x82B1A518`, JobConnectEndPoint starting at `0x82B2C698`); both
landed unchanged. Each sub-lane worked in its own `setup_worktree.sh` worktree, and I merged each branch into
`w16-of` with `--no-ff` as it finished.

| sub-lane | TUs | region given | fixed pins inside the region |
|---|---|---|---|
| A | ProductFacade | `0x82A8E148..0x82A91598` | — |
| B | SessionClockDDL, JobListenOnWellKnown | `0x82ACAB10..0x82AD5FA8` | Job.cpp `0x82ACF5A8`, LocalClock.cpp `0x82AD3138` |
| C | TournamentDDL, GameSessionDDL | `0x82AE02C8..0x82AE5E00` | `ssluse.c` `0x82AE51D0` |
| D | ZLibCompression | `0x82B1A518..0x82B1C2F0` | — |
| E | DupSpaceExtension, GlobalDiscoveryExtension | `0x82B26B20..0x82B2C698` | — |

## 2. Retail extents and flags

Every scaffold pin was an under-carve, and all eight extents fall inside W16-NY's [lo, hi] bracket (26 of 26
measured so far). Flags were read from each TU's asm: no EH prefixes or funclets ⇒ `/EHs-c-`; no RTTI locator
before the vtable ⇒ `/GR-`.

| TU | old pin | retail extent | B | true/lo | flags | edge evidence |
|---|---|---|---:|---:|---|---|
| ProductFacade | `82A8FDF0..82A90C08` | `82A8FDF0..82A90C88`, plus `s_csGlobalLock`'s `??__E` `82C417E8` / `??__F` `82C4A8F8` | 3,736 | 1.04 | `/Od /Oi- /EHs-c- /Ob1 /GR-` | `.rdata` (file string `0x8217E874`, vtable `0x8217E894`) follows MatchMakingClient's; the old pin left out `DeleteUtilitySubsystem` at `0x82A90C08`; code on both sides is never called from this TU |
| SessionClockDDL | `82AD2548..82AD2B6C` | `82AD2548..82AD30D0` | 2,952 | 1.89 | `/Od /Oi- /EHs-c- /Ob1` | `0x82AD30D0`/`0x82AD3130` are LocalClock's ctor/dtor, called only by LocalClock |
| ZLibCompression | `82B1B6D0..82B1B80C` | `82B1B660..82B1BE58` | 2,040 | 6.46 | `/Od /Oi- /Ob1 /GR-` (EH on) | `0x82B1B600` is the previous TU's deleting dtor (vtable `0x8218A0E8` precedes this TU's file string); `0x82B1B660`/`0x82B1B698` are the zlib allocator hooks; ends at the next TU's 8-byte EH prefix; `.rdata` `0x8218A0F8..0x8218A1F0` |
| DupSpaceExtension | `82B26DD8..82B26FFC` | `82B26B20..82B270E8` | 1,480 | 2.70 | `/Od /Oi- /EHs-c- /Ob1 /GR-` | extension, its `DupSpaceOperationCallback`, and the `MethodCallJob<DuplicationSpaceTable,int,PeriodicJob>` `GetTraceInfo`/`Execute`; `0x82B270E8` is DuplicationSpace's ctor |
| JobListenOnWellKnown | `82ACE5B0..82ACE660` | `82ACE5B0..82ACE828` | 632 | 3.59 | `/Od /Oi- /EHs-c- /Ob1 /GR-` | |
| GlobalDiscoveryExtension | `82B2C030..82B2C0E0` | `82B2BE80..82B2C0E0` | 608 | 3.45 | `/Od /Oi- /EHs-c- /Ob1 /GR-` | `0x82B2C0E0` starts a "Quazal Net-Z" product descriptor from another TU |
| GameSessionDDL | `82AE5BB0..82AE5C34` | `82AE5B50..82AE5D80` | 560 | 4.24 | `/Od /Oi- /EHs-c- /Ob1 /GR-` | 8 functions, `0x230` B; DynamicGatheringDDL's Add/Extract follow at `0x82AE5D80` |
| TournamentDDL | `82AE10A0..82AE1124` | `82AE10A0..82AE1210` | 368 | 2.79 | `/Od /Oi- /EHs-c- /Ob1 /GR-` | between the Competition ctor (`0x82AE0820`) and the Gathering ctor (`0x82AE1210`), both out-of-line functions of other files |

**TournamentDDL's other four functions were folded by the linker.** Its Add, Extract, StreamIn and StreamOut are
byte-identical to RankingDDL's, so Tournament's vtable `0x8217E5C4` points at RankingDDL's copies
(`0x82AE15A8`/`0x82AE15D8`). The source still defines them in order; our object is `0x230` B, as GameSessionDDL's
is.

**The true/lo ratio is not 1.8 for small TUs.** W16-NY §1.5 estimated the 105 anchored TUs at ≈ 1.8 × lo from nine
mostly large TUs. Here the median is ~3.1 and the range 1.04–6.46; the small TUs (lo ≤ 548 B) run 2.7–6.5. The
≈ 340 KB block estimate therefore probably undercounts what the small path-anchored TUs hold. This is eight more
points, not a re-fit.

## 3. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-of-ab2 --patch ~/tmp/w16of/ab2.patch`, in a fresh
`setup_worktree.sh` worktree at main `24a47ebd2`. The patch is the whole branch diff without this doc
(`git diff 24a47ebd2 w16-of -- . ':!docs'`; kinds configgen, map, source, splits). Run dir copied to
`~/tmp/w16of/ab2_run_dir/`.

**Prediction, written before the run** (the sum of the five sub-lane A/Bs; C and D measured on `36d8bd9f3`, the
others on `7431336d6`): C +12 / +920 B, D +9 (masked +2) / +2,000 B, E +20 / +2,060 B, B +25 / +2,916 B, A +11 /
+3,024 B ⇒ **Δmatched +77, Δmasked_equal +2, Δhonest +75, Δmatched_code +10,920 B**, totals unchanged, 0 rows
down. After rebasing C and D onto `7431336d6`, an integration build of main + C + D read their three units
unchanged (4/4, 8/8, 9/9, 2,920 B).

```
leg A: matched=53088 masked=25125 honest=27963 code%=57.098587  (recompiles: 0, settled)
leg B: matched=53165 masked=25127 honest=28038 code%=57.205140  (recompiles: 8, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+77  Δmasked_equal=+2  Δhonest=+75  Δcode%=+0.106553pp  Δcode_bytes=+10920
Δfuzzy=+0.119869pp   (legA 63.387966 -> legB 63.507835)
unit net (ALL units) = +77   vs whole-binary Δmatched = +77
units at 100% [mpn ruler]: legA 504 -> legB 510  (Δ+6; 6 reached 100, 0 fell off)
```

**Measured exactly as predicted.** `total_functions` (68,914) and `total_code` (10,247,792) do not move; the new
pins only move rows out of `auto_*` units. The +2 `masked_equal` is ZLibCompression's two EH funclets, which pair by
byte signature, so the honest gain is +75. Units reaching 100%: JobListenOnWellKnown, TournamentDDL,
GameSessionDDL, ZLibCompression, DupSpaceExtension, GlobalDiscoveryExtension.

An earlier identical run on main `7431336d6` (log `~/tmp/w16of/ab_run.log`) read the same deltas on every key. On
`24a47ebd2` one row had dropped before the fix in §5.1 (ProductFacade `CreateUtilitySubsystem`, 100 → 99.76).

### 3.1 Rows that went down

**None.** Row comparison of the two archived leg reports, keyed by unit and row name:
- 0 rows down and 0 rows up outside the eight TUs.
- 333 rows vanish and 333 appear (58,364 B each way, 0 resized). Every vanished row is a fuzzy-0 `fn_` placeholder:
  either named in its TU now, or (304 of them) an `auto_*` row whose unit name changed at the new pin edges.
- No row scoring above 0 vanished.

## 4. Per-TU results (graded, leg B `report.json`)

| TU | main: rows at 100 / rows | main: matched / unit B | branch: rows at 100 / rows | branch: matched / unit B |
|---|---:|---:|---:|---:|
| ProductFacade | 0 / 9 | 0 / 3,592 | 11 / 12 | 3,024 / 3,812 |
| SessionClockDDL | 0 / 13 | 0 / 1,560 | 20 / 23 | 2,284 / 2,912 |
| ZLibCompression | 0 / 1 | 0 / 316 | 9 / 9 | 2,000 / 2,000 |
| DupSpaceExtension | 0 / 2 | 0 / 548 | 13 / 13 | 1,460 / 1,460 |
| JobListenOnWellKnown | 0 / 1 | 0 / 176 | 5 / 5 | 632 / 632 |
| GlobalDiscoveryExtension | 0 / 1 | 0 / 176 | 7 / 7 | 600 / 600 |
| GameSessionDDL | 0 / 1 | 0 / 132 | 8 / 8 | 556 / 556 |
| TournamentDDL | 0 / 1 | 0 / 132 | 4 / 4 | 364 / 364 |
| **total** | | **0** | **77 / 81** | **10,920** |

ZLibCompression's rows include its two EH funclets (40 B each, both at 100). TournamentDDL's unit is 364 B because
its other four functions are RankingDDL's folded copies (§2).

### 4.1 Rows below 100

| row | B | fuzzy | why |
|---|---:|---:|---|
| ProductFacade `Terminate(CallContext*)` | 788 | 99.939 | Stack frame only: the second `Scheduler::GetInstance` expansion reserves 24 B where retail reserves 20, shifting 12 offsets by 4. It appears only after `GetInstanceFromVector` has been declined at an earlier direct call in the same function. Six variants did not move it; none was fitted blind. |
| SessionClockDDL `CallSyncRequest`, `CallSyncResponse` | 264, 312 | 99.924, 99.936 | Their only charge is the call to `0x82B35690`, mapped `PRUDPEndPoint::GetMaxSilenceTime`. Both bodies are "return the field at +0x98" and were folded. This needs an alias entry. |
| SessionClockDDL `fn_82AD2CB8` | 52 | 0 | One folded copy of three identical empty result handlers. Two other DDL files also call it, so it is left unnamed (naming it would turn forgiven placeholder call sites into charges). |

## 5. Shared files changed

**Shared headers: none.** Every TU declares the classes it needs locally, so nothing here can change another TU's
codegen. W16-OC's header changes moved one row here, in SessionClockDDL (§5.1). Other shared files:

- `config/45410914/objects.json`: per-TU `extra_cflags` for the eight files.
- `config/45410914/splits.txt`: the eight `.text` pins plus ProductFacade's `??__E`/`??__F` pins; dtk re-derived
  `.pdata`.
- `scripts/target_symbol_map.json`: **78 entries appended; 0 existing entries removed or changed** (checked
  against `7431336d6` after every merge). The sub-lanes' appends all conflicted at end of file; each resolution
  kept both sides and was checked for duplicate keys (0).

### 5.1 Integration notes

- **ProductFacade after W16-OG.** OG maps `0x82AAC608` as `ObjectThreadRoot::GetCurrentThreadName`. ProductFacade
  stored it as a free `Quazal::GetThreadName` in the thread-name resolver, which cost `CreateUtilitySubsystem`
  168 B (100 → 99.76) after the rebase. It now uses OG's name through a local declaration, and the row is back at
  100. Same code; one spelling per retail function.
- **SessionClockDDL after W16-OC.** OC's header changes grew `DOOperation` by 0x2C, which dropped
  `DispatchRMCCall` to 99.943. Its `CallMethodOperation` view is now a standalone local layout with retail's
  offsets, and the row is back at 100. The same changes let `DuplicatedObject::GetHandle` expand inline as it does
  in retail, which took the two caller stubs from ~55–62 to 99.92/99.94.
- **One spelling per retail function.** W16-OC's Station.cpp calls `0x82ACE5B0` as `JobListenOnWellKnown::Activate`.
  B's first map entry said `Launch`, which cost `Station::~Station` 596 B (100 → 99.966) in B's first A/B. It is
  `Activate` now, and that row is back at 100.
- **`0x82A99638` (`_DO_RootDO::DispatchRMCResult`) returns bool.** B and W16-OC found this independently. After
  the rebase this branch carries no RootDODDL change.

### 5.2 Source shapes that decided frames (all `/Od /Ob1`)

- `GetInstanceFromVector` is a **plain inline that `/Ob1` declines**, not the shared header's `noinline`. That
  holds for JobListenOnWellKnown (entry frame 0x10 short with the shared one) and ProductFacade, the third and fourth
  independent confirmations after W16-OE §6. The shared header is unchanged (W16-NY §4.4's E2 trade still stands).
- `Core::GetInstance` / `Scheduler::GetInstance` / `SystemComponents::GetInstance` spellings (if-return vs
  if-assign vs ternary) move callers' reservations without changing the helpers' own code. ProductFacade needs
  `Core *pCore = 0; if (inst) pCore = …`. NATTraversalEngine's form fixed its destructor but cost its constructor
  12 B. DupSpaceExtension's `Register` needs an if-return with no `else`. This is the same "no single shape"
  family as W16-NY §3.3, so each TU declares the shape it needs locally.
- Constructors pass the extension name as a string literal converted to `String`; an explicit `String(...)` did not
  match (E).
- `DuplicationSpaceTable` needs a virtual destructor. A vtable on an empty base makes MSVC use the 8-byte
  member-function pointer retail stores at `+0x40` (E).
- ZLibCompression's ctor needs an inline `ZLibStreams() {}` (retail's third temp after the `new`), and
  `DecompressImpl`'s four locals were ordered by name (W16-NW §2.5), screened by probe compile.
- Retail's DDL `Clone` stores the same vtable pointer twice. That needs `Tournament : _DDL_Tournament : Competition`,
  which the shared `src/network/Services/*.h` headers do not have, so C declares the chain locally.

## 6. Names

Attested names are used where they exist (ProductFacade's ctor, dtor, `Terminate(CallContext*)`,
`DecrementDOCoreRefCount`, its statics and the two `SystemComponents::Create*Group`; the extension and DDL class
members; the unmangled `QuazalCZlibAlloc`/`QuazalCZlibFree`). Chosen by these lanes and attested by nothing:
ProductFacade `Terminate()`, `CreateUtilitySubsystem`, `DeleteUtilitySubsystem` and the inline
`CallContext::SignalSuccess`. ZLibCompression's `DecompressImpl` returns `int` (retail returns the 4-byte local
with no bool conversion); `CompressImpl` returns `bool`, which the bytes cannot distinguish from `int` because
retail always returns 0.

## 7. Not done

- **Alias entries** for: `0x82B35690` (two SessionClockDDL stubs); `0x82AD2CB8` (shared empty result handler);
  Gathering's `IsA`/`IsAKindOf`, folded at `0x82B20200` (unnamed today, so GameSessionDDL's call is free; naming it
  `IsA` would charge GameSession's `IsAKindOf`); `_DO_SessionClock::SpecificUpdate` (`0x82AD2C18`), the kept copy that
  PromotionRefereeDDL's forwarder at `0x82AC5380` calls (a name charge once PromotionRefereeDDL is written).
- **Neighbours found and left unpinned:**
  - **RankingDDL.cpp** `0x82AE13D8..0x82AE1608`. It is **missing from W16-NY's 105-TU table**; its path string is at
    `0x82184834`, and it has the same 8 functions as GameSessionDDL. An easy 8-row follow-up.
  - **Gathering.cpp** `0x82AE1210..0x82AE13D8` (ctor, deleting dtors, `~Gathering` at `0x82AE1388`). Competition's
    ctor/dtor code starts at `0x82AE0820`.
  - **DynamicGatheringDDL** should start at `0x82AE5D80`, not `0x82AE5E00`. Its Add/Extract (`0x82AE5D80`/`0x82AE5DC0`,
    called only by its own StreamIn/StreamOut) sit in `auto_03_82AE5D80_text`.
  - **The `ssluse.c` pin at `0x82AE51D0..0x82AE52A8` is Quazal code, not curl.** It allocates through Quazal's
    MemoryManager and stores vtable `0x82184894`. It was left untouched as instructed; it needs re-homing.
  - `0x82A8E148..0x82A8FDF0` and `0x82A90C88..0x82A91598` (around ProductFacade); `0x82B1A518..0x82B1B660` (socket
    code) and `0x82B1BE58..0x82B1C2F0` (an XNADDR/StationURL class) around ZLib; `0x82B2B940..0x82B2BE80` (a
    `PseudoGlobalVariable` template copy belonging to DuplicationSpace), `0x82B2C0E0..0x82B2C1D8` (product
    descriptor) and `0x82B2C1D8..0x82B2C698` (an EH-on TU) around GlobalDiscoveryExtension.
- The ProductFacade frame residue (§4.1) and the shared-header question of a single `Core`/`Scheduler`/
  `GetInstanceFromVector` shape.

## 8. Native gate

Run last, on the code at `ba04bfee6` (only this docs edit follows it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Scratch: `~/tmp/w16of/` (`ab.patch`, `ab_run.log`, the map-merge helper `~/tmp/w16of_mapfix.py`); sub-lane scratch in
`~/tmp/w16ofb/`, `~/tmp/w16ofc/`, `~/tmp/w16ofd/`, `~/tmp/w16ofe/`; sub-lane worktrees `~/tmp/wt-w16-of-{a,b,c,d,e}`.
