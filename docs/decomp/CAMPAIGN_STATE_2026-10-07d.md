# Campaign state — 2026-10-07d (after W16-TM … W16-UQ)

Twelfth edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-07c.md` (W16-TN) for numbers and
priorities, and with it `CAMPAIGN_STATE_2026-10-07.md` (W16-SD) and `-07b.md` (W16-TA).

**Why this edition exists.** W16-UQ was briefed off §6 of the *first* 10-07 edition while 10-07c already existed,
and found its levers 2 and 3 settled by W16-SG and W16-TH (`W16UQ_LEVERS_2_3_ALREADY_SETTLED_2026-10-07.md`). By then
10-07c was itself about 300 commits old: 28 lanes (W16-TM … W16-UQ) had landed on its levers. Every lever below was
re-derived on the current tree, from this tree's `report.json`, objs, native build and map, not from an earlier doc.
Where a lane doc is the source of a disposition, the row population it covers was re-derived and asserted equal to
the lane's own population file.

Method as in 10-07c (W16-TN's pipeline, copied and repointed), with these changes, each stated where it is used:

- **New buckets 60/61/62** for W16-TO, TP and TQ's per-row retail reads (§4.1).
- **Carry-forward of 10-07c's per-row dispositions**, keyed by (unit, name), then name, then retail address, with a
  control that reports every row whose bucket changed (§4.1).
- **Native execution rerun** on this tree (§4.3). Two tools needed fixes first; both are in this branch (§8).
- **Constant-value audit rerun** (`tools/const_value_audit.py`), answering W16-TP's recommended census (§6.1).
- **No per-lane walk** (§3), because of memory. Attribution reconciles at residual 0 / 0 without it.

No source, map, splits, alias or `symbols.txt` edit on this branch. Scope rules are W16-OV's: the native port is the
goal, XDK is hard-skipped, Quazal stays as landed.

## 1. Measured

Worktree `~/tmp/wt-w16ur` (a `scripts/setup_worktree.sh` tree), first at **`65dc0dbc4`** (W16-UQ), full
`./tools/ninja-locked`, then rebased onto main **`12d4ebdfe`** (W16-UO) and rebuilt. Both builds ended `BUILD_RC=0`,
`[pairing] 1029/1029 declared compiled objects pair with a target`, `[patch-state] OK: tree is a fixed point of 6
post-compile passes`. Before each read I wiped `report.json` and `report.cache` (`Report cache: 0 hits, 3093
misses`). Ruler: `name_check` (objdiff 4.2.9, tool commit `a5f0ea903ec1`, binary hash `5a51cd51fe0a353f`, read from
`report.json`'s `provenance`). dtk 1.15.0 (`cd2495ca`).

**Prediction 1, written before reading `report.json` at `65dc0dbc4`:** the merges' own A/B claims since 10-07c's
`b79d790b8` sum to **+99 fns / +9,144 B**:

| lane | claim | lane | claim |
|---|---|---|---|
| TP | −1 / −460 (an accuracy fix) | UK | +9 / +1,060 |
| TT | +1 / +488 | UM | +38 / +3,956 |
| TU | +8 / +268 | UN | +32 / +2,632 |
| TX | −1 / −116 | UP | +9 / +904 |
| TZ | +1 / +60 | UQ | +2 / +276 |
| UH | +1 / +76 | all others (TM TO TQ TR TS TV TW TY UA UB UC UD UF UG UI UJ UL) | Δ0 each |

So I expected `matched_functions` 55,038 and `matched_code` 6,087,376. **Both held exactly.**

**Prediction 2, before the rebuild at `12d4ebdfe`:** W16-UO claims X360 Δ0 (its `src/` edits are native-only plus
`MemMgr.h`), so every key and every row should be unchanged. The rebuild recompiled 1,128 TUs (the `MemMgr.h` and
`obj/Dir.h` PCH cascade). **It held: all seven headline keys equal, and 68,884 / 68,884 rows equal on (unit, name,
fuzzy, mpn, size), 0 changed, 0 added, 0 removed** (archived `~/tmp/w16ur-gap/rep_65dc0dbc4.json.gz` against the
rebuilt report). Everything below is therefore the state of `12d4ebdfe`. The native columns (§4.3) were measured on
`65dc0dbc4`'s native build. W16-UO changed native code since, so they are one merge older (§8).

```
python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 12d4ebdfe (2026-10-07, W16-UO)"
  scaffold thr<=4 : 0 · thr<=5 : 0 · thr<=6 : 56 units, 289 rows, 57,448 B · thr<=7 : 56 (cliff, not fitted)
total_code            10,247,844
PAIRABLE               6,703,404 = 65.413%   (1,029 units, 56,317 rows)
− scaffold shells         57,448             (56 units, 289 rows)
= reachable ceiling    6,645,956 = 64.852%
matched_code           6,087,376 = 59.402% of total_code = 91.60% of ceiling
gap to ceiling           558,580
matched_functions         55,038 / 68,884   masked_equal 25,227   honest 29,811
fuzzy_match_percent       64.38907
```

Against 10-07c: **+9,144 B / +99 fns**. The share of the ceiling went 91.46% → **91.60%**. The gap went 567,832 →
**558,580 B (−9,252)**, which is the +9,144 B matched plus a 108 B fall in the ceiling. The 108 B is the
`_M_fill_insert` row `0x82272A60` that W16-TU/TX unpinned. It is now in `auto_03_82272A60_text`, outside the
ceiling. Pairable units went 1,056 → 1,029: TU, TX and TZ removed the Flow, gesture and hamobj units that held other
TUs' code. `total_code`, `total_functions` and the row count did not move.

| control | result |
|---|---|
| attribution | claims +99 / +9,144 against measured +99 / +9,144: residual **0 / 0** |
| second build | `12d4ebdfe` equals `65dc0dbc4` on every key and every row (prediction 2) |
| population sums | 1,576 gap rows sum to 558,580 = ceiling − matched (`gengap.py`) |
| ring sums | tier totals sum to `total_code` and `matched_code` (`scope_ledger2.py` asserts both) |
| disposition tables | §4 sums to 495 rows / 197,368 B = in-scope gap; §5 to 386 / 186,956 = VIA-DC3 gap (`dispo6.py`, `dispo_via6.py` assert) |
| lane pools | TO's 57 rows, TP's 49 and TQ's 49 re-derived from 10-07c's per-row files and asserted equal to `~/tmp/w16to/rows57.json`, `~/tmp/w16tp/rows.json`, `~/tmp/w16tq/rows49.json` (`overlay6.py`) |
| carry control | in scope 387 rows same bucket, 106 upgraded by TO/TP, 1 new, 1 changed; VIA-DC3 335 same, 49 upgraded by TQ, 2 new. Every change is named in §4/§5 |
| row movement | in scope and VIA-DC3: entered − left − size changes = Δ ring gap (`cmp1007c.py` asserts both) |
| placeholder correction | 24/24 placeholder-only N2 rows read identical fuzzy at `none` and graded |
| native set | `native_linked_tus.py` self-validation passed (686 TUs) |
| runtime rank | all 18 targets `rc=0`, rb3-render profile 11.6 MB, the tool's self-validation passed |
| stub census | rc 0 after classifying 10 new native objects (its own exit-3 guard fired first, §8); the matcher finds 205 in-scope rows at 100 that a live stub also defines, so its 0 for gap rows is not vacuous |
| constant audit | 1,023 units, 56,317 functions, 28,947 position pairs compared (W16-PW's final run: 27,998) |

## 2. Rings

From `cmp1007c.py` (this build's `ledger2.json` against W16-TN's).

| tier | reachable | matched | **gap** | share | gap at 10-07c | Δ matched | Δ fns | Δ reach |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 479,648 | 437,248 | **42,400** | 91.16% | 43,752 | +2,872 | +33 | +1,520 |
| IN-SOON | 2,440,280 | 2,341,972 | **98,308** | 95.97% | 99,804 | +1,384 | +21 | −112 |
| IN-RB3ENG | 928,636 | 871,976 | **56,660** | 93.90% | 57,804 | +2,472 | +29 | +1,328 |
| **in scope** | **3,848,564** | **3,651,196** | **197,368** | **94.87%** | 201,360 | **+6,728** | **+83** | +2,736 |
| VIA-DC3 (DC3 has the file) | 2,018,832 | 1,831,876 | **186,956** | 90.74% | 192,072 | +980 | +2 | −4,136 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 113,520 | 0 | 0 | 0 |
| OUT-NET | 76,964 | 73,136 | 3,828 | 95.03% | 3,868 | +68 | +2 | +28 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 | 0 |
| OUT-360-OTHER | 349,052 | 296,956 | 52,096 | 85.08% | 52,200 | +88 | +2 | −16 |
| UNKNOWN-other | 4,844 | 2,980 | 1,864 | 61.52% | 1,864 | +1,280 | +10 | +1,280 |

- **Native scope CORE+SOON:** 99.21% of functions and 95.18% of bytes (10-07c: 99.08% / 95.08%).
- **This round went to the in-scope ring:** +6,728 B against VIA-DC3's +980 B (10-07c's round was the other way).
  The in-scope gap fell 3,992 B, the VIA-DC3 gap 5,116 B. Most of VIA-DC3's fall is reach, not matching: TU/TX/TZ
  moved 4,136 B of VIA-DC3 reach out (the Flow, gesture and hamobj units held in-scope and engine code), and that
  code landed in IN-CORE, IN-RB3ENG and UNKNOWN-other. The VIA-DC3 gap is still 10,412 B below the in-scope gap.
- **Reach moved net −108 B**, the one unpinned row above. Everything else is a re-home between rings.

**Gap movement, keyed on (unit, symbol):**

- **In scope, left: 51 rows / 4,212 B.** By 10-07c disposition: U2 identification residue 47 / 4,040 (W16-UM, UN
  and UP named the addresses W16-UH proved), EH funclets 4 / 172. By directory: `system/bandobj` 10 / 1,064,
  `system/obj` 6 / 964, `system/beatmatch` 8 / 688, `band3/meta_band` 8 / 552, `band3/game` 6 / 396.
- **In scope, entered: 3 rows / 220 B**, none a re-home: Tour's `vector<Symbol>` copy ctor (116 B, U3, W16-TX
  §2.3: the instantiation is missing), MetaPanel `fn_82574D90` (100 B, U2, W16-TU's NextSongPanel factory) and
  TrackPanelDir `fn_82308470` (4 B, U2, W16-TU).
- **In scope, still in the gap, fell: 2.** `RockCentral::DataPointToQString` 91.426 → 91.383 (W16-UK fixed
  `JsonObject::Str`, so its `Str` site is now checked) and `fn_82C3FA94` 78.5 → 0 (W16-UK's `Queue::~Queue` fix
  took the body it had been pairing against; UK §2 records it). One rose.
- **VIA-DC3, left: 60 rows / 5,724 B.** U2 35 / 3,700, funclets 20 / 848, W16-TG rows 2 / 456, `Fader::DoFade` 444
  (W16-TT), W16-SW's #12/#13 276 (W16-TT/TV).
- **VIA-DC3, entered: 4 rows / 608 B.** `EstimateDraw` (460 B, 100 → 99.98: W16-TP's accuracy fix, −460 B on
  purpose), the AO funclet `fn_8248F818` (40 B, 100 → 99.5, W16-TZ §4 records it), and two CharPollGroup rows
  W16-TU re-homed (`fn_823B08D0` 68 B U2, `fn_823B0AA8` 40 B).
- **Whole binary**, over the 68,696 rows present under the same (unit, name) in both trees: **12 rose, 4 fell**
  (`rowsdown3.py`, against W16-TN's archived report). The four are `DataPointToQString`, `EstimateDraw`,
  `fn_8248F818` and `fn_82C3FA94`, each recorded by its lane above.

## 3. What landed, per lane

No per-lane walk this edition: rebuilding 28 steps needs a second worktree and a build per step, and the box was
short on memory. The lane claims sum exactly to the measured Δ (§1, residual 0 / 0), so no step is unaccounted for
in total. What a walk would add, per-lane ring attribution, is not available. The claims below are the lanes' own
`tools/ab_measure.py` results.

| lane | what it did | X360 claim |
|---|---|---|
| TM | the 27 VIA-DC3 files no native target compiled: linked into native targets | Δ0 |
| TO | 10-07c lever 1, in-scope buckets 23 and 25: 57 rows read against retail, **57 EQUAL** | Δ0 |
| TP | lever 1, in-scope buckets 11 and 22: 49 rows, **49 EQUAL**; side fix `EstimateDraw`'s six misplaced weights | −1 / −460 |
| TQ | lever 1, VIA-DC3 bucket 30: 49 rows, **49 EQUAL** | Δ0 |
| TR | callers for the seven files rb3-render discarded; XboxEnumeration/XboxPurchaser aborting stubs | Δ0 |
| TS | lever 2: `SetState`, `OnFileLoaded`, `MaybePublish` and the song/modifier filters gated natively | Δ0 |
| TT | this round's fold-group leads adjudicated on retail bytes; `Fader::DoFade` | +1 / +488 |
| TU | the Flow units held other TUs' code; every row re-homed | +8 / +268 |
| TV | the two alias-proof gaps closed; the 74 refuted memberships adjudicated | Δ0 |
| TW | lever 2: `Locale::Init`, `FileMakePath`, `SaveObjects`, `JoypadPollCommon`, `WahEffect::Process`, `FindXfm` gated | Δ0 |
| TX | the gesture and hamobj units held other TUs' code; every row re-homed | −1 / −116 |
| TY | lever 2, VIA-DC3: 42 of 173 unentered rows taken, 23 gates, 3 native-only bugs | Δ0 |
| TZ | rows that paired only through code retail does not contain | +1 / +60 |
| UA, UD | native targets read retail's system config, including the ten no-disc drivers | Δ0 |
| UB | X360 literal sizes and offsets in native-compiled source (LP64) | Δ0 |
| UC | every native file read resolves the way retail resolves it | Δ0 |
| UF | lever 2 finished in scope: the 40 rows below TW's #22 and TW's 5 deferred rows | Δ0 |
| UG | script functions retail registers that native reached without one | Δ0 |
| UH | call sites whose retail callee is unnamed, checked on retail bytes; `Profile::GetName` | +1 / +76 |
| UI, UJ | every shipped venue milo loads natively; the native clamp's NaN | Δ0 |
| UK | UH's "right callee, body differs" rows, fixed or explained (17 bodies) | +9 / +1,060 |
| UL | the eight empty venue renders, the venue unload leak | Δ0 |
| UM | naming the retail addresses UH proved our callee to be | +38 / +3,956 |
| UN | map defects and pin moves from UH / UM | +32 / +2,632 |
| UO | native targets under AddressSanitizer, every finding fixed | Δ0 (re-confirmed here, §1) |
| UP | the eight addresses UN left to source | +9 / +904 |
| UQ | levers 2 and 3 of the first 10-07 edition were already settled; one `RndMeshAnim` fold | +2 / +276 |

## 4. Where the in-scope gap stands: one disposition per row

Each in-scope gap row (495 / 197,368 B) goes to the first bucket that fits (`dispo6.py`). **"Native-compiled"**
means the row's source file is a TU of at least one native target, as the compiler reports it (§4.3).

| disposition | rows | bytes | % | CORE | SOON | RB3ENG | native-compiled |
|---|---:|---:|---:|---:|---:|---:|---:|
| read for behaviour by W16-PQ/PR: no defect left, codegen residue | 107 | 96,028 | 48.7% | 21,680 | 45,924 | 28,424 | 94,888 |
| **read against retail by W16-TO (value flow, calls, constants): EQUAL, codegen residue** | 57 | 40,896 | 20.7% | 6,232 | 20,672 | 13,992 | 40,896 |
| **read against retail by W16-TP (`row_valueflow.py`): EQUAL, codegen residue** | 49 | 23,296 | 11.8% | 2,736 | 19,080 | 1,480 | 22,860 |
| source divergence: worked since 10-06, left with a stop reason | 27 | 11,340 | 5.7% | 2,752 | 2,868 | 5,720 | 11,340 |
| name: pointer `_M_fill_insert` fold, settled not admissible (W16-OX) | 14 | 6,760 | 3.4% | 2,388 | 3,572 | 800 | 6,760 |
| EH funclet / anonymous fragment of a parent row | 153 | 6,276 | 3.2% | 968 | 3,432 | 1,876 | 6,224 |
| never-worked row taken by W16-PO, left with a stop reason | 9 | 5,188 | 2.6% | 2,356 | 0 | 2,832 | 4,936 |
| identification residue (U2) | 74 | 4,488 | 2.3% | 1,028 | 2,544 | 916 | 4,336 |
| settled by W16-TH, left with its stop reason (`SuperFormatString` ctor 1,608, `SendDataPoint<const char*,int>` 100) | 2 | 1,708 | 0.9% | 1,608 | 100 | 0 | 1,708 |
| source divergence: named in a W16-S* doc, left (`PreInitSystem`, W16-SM) | 1 | 652 | 0.3% | 652 | 0 | 0 | 652 |
| name: parked by W16-SW, no second channel (`MeshCacher::Sync`) | 1 | 620 | 0.3% | 0 | 0 | 620 | 620 |
| unpaired named (U3): Tour's `vector<Symbol>` copy ctor, instantiation missing (W16-TX §2.3) | 1 | 116 | 0.1% | 0 | 116 | 0 | 116 |
| **total** | **495** | **197,368** | 100% | 42,400 | 98,308 | 56,660 | **195,336** |

Against 10-07c: the four "named in a doc / lever applied" register buckets (23, 11, 22, 25: 106 rows / 64,192 B)
are gone. All 106 rows now carry a per-row retail verdict, in buckets 60 and 61. U2 fell 118 → 74 rows
(8,384 → 4,488 B). The funclet bucket fell by 5 rows. The Tour row is new.

### 4.1 How the buckets are decided

`dispo6.py` is W16-TN's `dispo5.py` with three buckets inserted ahead of the TH stop rule:

- `60` W16-TO, `61` W16-TP, `62` W16-TQ (VIA-DC3, §5). Each lane's population is 10-07c's rows with the bucket
  prefix the lane was briefed on (TO 23+25, TP 11+22, TQ 30), re-read from `~/tmp/w16tn-gap/dispo_rows.json` /
  `dispo_via_rows.json` and asserted equal, row for row, to the population file the lane itself wrote. All three
  lanes report every row EQUAL, so the bucket is the whole population.
- **Carry-forward.** Each current row is joined to its 10-07c row by (unit, name). If that fails, the name alone is
  used. If that fails too, the retail address is used. The address comes from `fn_<addr>` or the inverted
  `scripts/target_symbol_map.json`, at `b79d790b8` for the old side and at HEAD for the new side, so a row renamed
  by UM/UN since still finds its old record. The carry control (§1) prints every row whose bucket changed. Only one
  changed: `fn_82C3FA94`, funclet → U2, because its parent pairing is gone (§2).
- Lane-doc mentions now include the `W16U*` docs, in the same mention bucket. No in-scope row landed there that was
  not already there.

### 4.2 How it adds up

- **Codegen residue with a per-row behaviour read: 160,220 B, 81.2%** (buckets 20, 60, 61). This is the same
  160,220 B as 10-07c's codegen block, but every row now has a per-row read. At 10-07c, 64,192 B of it rested on a
  doc mention or a lever. **No defect was found in the 106 rows TO and TP read.**
- **Source-class rows with a stop reason: 18,888 B, 9.6%.** W16-PO's 9 rows, 27 rows worked since 10-06, W16-TH's
  two stop rows and `PreInitSystem`. Unchanged to the byte.
- **Settled, non-separable or parked: 18,144 B, 9.2%.** Funclets 6,276, W16-OX's fill_insert fold 6,760,
  `MeshCacher::Sync` 620, U2 4,488.
- **Open: 116 B**, the Tour copy ctor, which entered this round with a known cause.

### 4.3 Native relevance

**Compiled.** The native set is `tools/native_linked_tus.py`'s answer (686 TUs, `clang++ -M` over every target, on a
`native/build` I configured in this worktree), unioned with the json-c `.c` compile edges: **717 files** (10-07c:
642). The new files are W16-TM and W16-TR's link closure.

| measure | in scope | VIA-DC3 |
|---|---:|---:|
| gap | 197,368 | 186,956 |
| **native-compiled** | **195,336 (99.0%)** | **186,100 (99.5%)** |
| at 10-07c | 199,328 (99.0%) | 177,964 (92.7%) |
| not compiled | 2,032 in 5 files | **856 in 1 file** |
| **executed by a native target** | **99,072 (50.2%), 122 rows** | **84,188 (45.0%), 87 rows** |
| at 10-07c | 68,168 (33.9%), 87 rows | 22,212 (11.6%), 31 rows |

- **In scope, the not-compiled remainder is the same 5 files / 2,032 B as at 10-07b and 10-07c:**
  `net_band/RockCentral.cpp` 664, `os/AsyncFile_Win.cpp` 460, `net_band/XboxEntityUploader.cpp` 436,
  `os/ThreadCall_Win.cpp` 252, `net_band/ContextWrapper.cpp` 220 (W16-SH §4's reasons stand).
- **VIA-DC3's not-compiled remainder fell 14,108 B / 24 files → 856 B / 1 file** (W16-TM, W16-TR). The one file is
  `meta/StoreEnumeration.cpp`, which native replaces by rule: W16-TR's `XboxEnumeration` stubs abort if reached.
- **Native-compiled in-scope files with the most gap:** VocalTrack.cpp 11,532, BandPatchMesh.cpp 7,528, Geo.cpp
  6,180, GemManager.cpp 5,492, CustomizePanel.cpp 5,428, BandDirector.cpp 4,620, SaveLoadManager.cpp 4,096, Dir.cpp
  3,624, BandCamShot.cpp 3,580, NoteTube.cpp 3,576.
- **Stubs.** `tools/native_stub_census.py native/build stubs.json --authored` (after the fix in §8): 1,061 live stub
  symbols. **No in-scope gap row is replaced by a live stub.** In VIA-DC3, 3 rows / 1,084 B are:
  `XboxEnumeration::Poll` 452 and `::Start` 328 (W16-TR's stubs, by rule as above), and `CacheResource` 304 (38.42,
  as at 10-07c).

**Executed.** `tools/native_runtime_rank.py` rerun on `65dc0dbc4` (profile build in `native/build-prof`, all 18
targets, joined by demangled signature):

| | in scope | VIA-DC3 |
|---|---:|---:|
| executed | 122 / 99,072 | 87 / 84,188 |
| joined to a native definition, never entered | 131 / 80,132 | 135 / 92,432 |
| anonymous `fn_` (cannot join by signature) | 227 / 10,764 | 154 / 6,800 |
| no native definition, or unparsed | 15 / 7,400 | 10 / 3,536 |

- Executed in scope by bucket: PQ/PR 50 / 56,288, TO 29 / 20,104, TP 14 / 5,676, worked-since-10-06 17 / 7,992,
  PO 8 / 4,936. VIA-DC3: TQ 19 / 22,704, TE/TI/TK EQUAL 16 / 21,920, TG 21 / 13,108, SF/SV 21 / 11,308.
- **Behaviour-class named rows in native-compiled files** (I3/I4/M1/M2/U3/U4), `execlever.py`:
  - **In scope: 102 rows / 83,004 B, 57 / 50,368 executed.** Of the 45 not entered, 42 / 32,200 were taken by
    W16-TS, TW or UF (gated or given a recorded reason). **3 rows / 436 B are left:** UF's DataNode map
    `operator[]` 220, the Tour copy ctor 116 (no body) and `SendDataPoint` 100 (no caller in any TU).
  - **VIA-DC3: 99 rows / 88,224 B, 40 / 44,228 executed.** Of the 59 / 43,996 not entered, 10 / 16,124 were taken by
    W16-TY, TM or TR. **49 / 27,872 B are not taken** (33 rows ≥ 300 B, 7 ≥ 1,000 B). **Every one of the 49 already
    has a per-row retail read** (buckets 40, 50, 51, 52, 53). So running them looks for native bugs and cannot move
    X360 bytes.
- **Executed is still not gated.** W16-TY: 31 executed-but-ungated VIA-DC3 rows from 10-07c (22,212 B) were not
  gated, and its AO / TessellateMesh gates check invariants, not retail's float results.

### 4.4 Class view (W16-OV's table, same rules)

| block | classes | rows | bytes | % | at 10-07c |
|---|---|---:|---:|---:|---:|
| register/scheduling only | P1 + P2 | 114 | 71,116 | 36.0% | 71,116 |
| reorder-shaped insert/delete | I1 + I2 | 35 | 24,204 | 12.3% | 24,204 |
| source divergence | I3 + I4 + M1 + M2 | 137 | 85,060 | 43.1% | 85,100 |
| relocation name | N1 + N2 | 133 | 12,284 | 6.2% | 12,456 |
| unpaired | U2 + U3 + U4 | 76 | 4,704 | 2.4% | 8,484 |

- **The register and reorder blocks are byte-identical to 10-07c.** TO and TP read all of them and changed none.
  The movement is in the unpaired block (−3,780 B, UM/UN/UP's naming) and the name block (−172 B).
- 216 of the 495 rows are at `mpn` 100. Of the 133 name rows, 118 are anonymous funclets at `mpn` 100.
- **By directory:** `system/bandobj` 113 rows / 45,408 B, `band3/meta_band` 113 / 44,712, `band3/bandtrack`
  25 / 21,980, `band3/game` 49 / 17,464, `system/os` 37 / 13,232.
- The top 100 rows hold 68.6% of the gap. The median row is 136 B.

## 5. The VIA-DC3 ring

Same rules as §4, with the ring's own lanes first (`dispo_via6.py`).

| disposition | rows | bytes | % | native-compiled |
|---|---:|---:|---:|---:|
| **read against retail by W16-TQ (value flow, both windows): EQUAL, codegen residue** | 49 | 47,276 | 25.3% | 46,496 |
| read against retail by W16-TE/TI/TK: EQUAL, codegen residue | 44 | 44,848 | 24.0% | 44,848 |
| **DC3 body compared by W16-TG, left (register, revision or DC3 worse); no retail read** | 62 | 30,416 | 16.3% | 30,416 |
| read against retail by W16-TE/TI/TK: fixed, residue recorded (now incl. TP's `EstimateDraw`) | 17 | 23,888 | 12.8% | 23,888 |
| read row by row by W16-SF, left SCHED | 19 | 8,032 | 4.3% | 8,032 |
| read row by row by W16-SF, left PARTIAL | 9 | 6,232 | 3.3% | 6,232 |
| read row by row by W16-SV, left SCHED | 8 | 5,760 | 3.1% | 5,760 |
| EH funclet / anonymous fragment | 99 | 4,164 | 2.2% | 4,164 |
| name: pointer fill_insert fold (W16-OX) | 7 | 3,448 | 1.8% | 3,448 |
| read row by row by W16-SV, left PARTIAL | 4 | 3,096 | 1.7% | 3,096 |
| identification residue (U2) | 55 | 2,636 | 1.4% | 2,560 |
| source divergence worked since 10-06 (`RndPropAnim::ForeachKeyframe`) | 1 | 2,256 | 1.2% | 2,256 |
| read row by row by W16-SV, left HOME | 2 | 1,544 | 0.8% | 1,544 |
| settled by W16-TH, left with its stop reason (`OggMap::OpenMogg`) | 1 | 1,004 | 0.5% | 1,004 |
| read row by row by W16-SV, left RECORDED | 3 | 728 | 0.4% | 728 |
| read row by row by W16-SF, left RECORDED | 3 | 576 | 0.3% | 576 |
| read against retail by W16-TE: inline blocked by the PCH (`SynthEmitter` ctor) | 1 | 496 | 0.3% | 496 |
| read row by row by W16-SV, left NAME (`Rnd::TestPoint`) | 1 | 332 | 0.2% | 332 |
| register, named in a W16-S* doc (`OggMap::SetKey`, W16-SM; W16-RG: pure register) | 1 | 224 | 0.1% | 224 |
| **total** | **386** | **186,956** | 100% | **186,100** |

- **10-07c's bucket 30 (49 / 47,276 B, no per-row read) is now bucket 62.** W16-TQ read all 49 against retail and
  found every one EQUAL in behaviour.
- **The only VIA-DC3 register population without a retail read is W16-TG's bucket 53: 62 rows / 30,416 B**, all
  native-compiled, **21 / 13,108 B executed**. By class P1 44, P2 8, I1 7, I2 2, M1 1. Largest: `RndScaleObject`
  3,112 (I1, 89.58, executed 608×), `CharBones::RotateBy` 1,420 (P1, 99.94), `RndTransAnim::MakeTransform` 1,160
  (P1, 99.59, 931×), `Rnd::DrawTimers` 1,080 (M1), `CharHair::Hookup` 1,012, `MakeTangentsLate` 952,
  `CharIKFoot::DoFSM` 892, `Hmx::operator*` 848 (I1, 77.14). TG compared each with DC3's body, which is a
  different binary.
- 10-07c's two name rows (`Fader::DoFade` 444, W16-SW #12/#13 276) are gone (W16-TT, TV). U2 fell 89 → 55 rows.
- **By directory** the ring is unchanged in shape: rndobj, then char, world, synth.

## 6. Open levers, ranked by native relevance, then size

Every lever below was checked on this tree: its rows were selected from this build's gap files, and its leftovers
were looked up by address in this build's map, splits and `report.json` (`chk/where.py`). Tiers as at 10-07c:

- **A:** the charge can encode behaviour, in a file native compiles, and no per-row retail read exists.
- **B:** behaviour work that is native-only, or a verification of behaviour fixes that the ruler cannot yet see.
- **C:** codegen-only or naming. A fix moves the metric but cannot change native behaviour.

**What is gone.** In scope, no row is left in tier A. Every in-scope gap row has a per-row read or a stop reason.
The in-scope execution lever is drained to 3 rows / 436 B, each with a known reason. The VIA-DC3 not-compiled lever
is down to one file replaced by rule.

1. **[A] VIA-DC3 bucket 53: 62 rows / 30,416 B** (all native-compiled, **21 / 13,108 B executed**). These are W16-TG's
   register/reorder rows, compared with DC3's body and never read against retail (§5). Method: TO/TP/TQ's per-row
   read (calls, one-sided loads/stores, constants, value flow; `tools/row_valueflow.py` is committed by W16-TP).
   Take the executed rows first. **Price it low.** TO, TP and TQ read 155 rows of this class and found 0 defects in
   them. TG found 2 behaviour defects in its 70 by comparing with DC3. Expect at most one or two defects and few
   bytes. It is the last register population in either ring without a retail read.
2. **[B] Pin the `auto_*` runs that hold bodies W16-UK already fixed: 51 rows / 8,908 B, outside the ceiling
   today.** UK fixed these bodies against retail by chase, but they sit in unpinned runs, so no row pairs and the
   ruler cannot confirm a single one (UK §3 lists them as "in an `auto_*` unit"):

   | run | rows | bytes | holds | blocker |
   |---|---:|---:|---|---|
   | `auto_03_825219A0_text` | 26 | 3,616 | `FileEnumerate` 396 + the StageKit TU | `src/system/os/StageKit.cpp` is ported but not in `objects.json`; it sits between File_Win.cpp and DateTime.cpp |
   | `auto_03_826C8468_text` | 10 | 3,108 | `GemPlayer::GemPlayer` 2,704 | directly after GemPlayer.cpp's pins |
   | `auto_03_82B6985C_text` | 13 | 2,056 | `~FxSend360` 372, `FxSend360::Cleanup` 332 | `Cleanup` has no body anywhere (UK §5); between `synth_xbox/FxSend.cpp` and FxSendPitchShift |
   | `auto_03_823399C0_text` | 1 | 112 | `BandCharDesc::NewObject` | sandwiched hole |
   | `auto_03_822E36B8_text` | 1 | 16 | `GemTrackDir::UpdateFingerFeedback` | sandwiched hole |

   Four more UK bodies sit in **another unit's span**, so they need a re-home, which is not metric-neutral:
   `ObjPtrList<T>::operator=` 220 (SongSectionController, 0%), `JoypadTerminateCommon` 104 (OnlineID, 0%),
   `RndShaderMgr::InitShaders` 44 (MeshAnim, 70.45), `PreloadPanel::SetTypeDef` 8 (DeJitterPanel, 0%). That is
   4 rows / 376 B. A wider census (`chk/sandwich.py`) finds 159 `auto_*` units / 747 rows / 193,800 B sandwiched
   between two pins of one unit. They are mostly XDK. The non-XDK part is 19 units / 63 rows / 4,560 B, of which
   788 B is Quazal session code and two rows are the 128 B above. A pin that pairs at 100 raises the ceiling and
   `matched_code` together, so Δgap is 0. A pin that does not pair at 100 grows the gap, and that is an accuracy
   win. Either way the value is verification of UK's fixes, not bytes.
3. **[B] VIA-DC3 native execution, what W16-TY left.** 49 behaviour-class rows / 27,872 B are native-compiled and
   never entered, and no lane took them (§4.3). The largest are `RndLine::UpdateLine` 1,328,
   `NgSpotlightDrawer::RenderConeDefs` 1,324, `NgLight::SphereConeTest` 1,236, `SpliceKeys` 1,096,
   `Rnd::DrawTimers` 1,080, `RndLine::UpdateLinePair` 1,076 and `CamShotFrame::BuildTransform` 1,024. Every one has
   a per-row retail read already, so this finds native bugs only. The yield so far: the TF/TJ/TS/TW/TY/UF gates found
   **0 X360 behaviour differences**, and every bug they found was native-only (TF/TJ 2, TS 1, TW 4, TY 3, UF 3, by
   each doc's §5). TY's wider remainder is 131 of 173 rows / 70,784 B (all
   classes, all < 1,332 B), plus the 31 executed-but-ungated rows. **In scope this lever is drained** (3 rows /
   436 B, §4.3).
4. **[B] Native-only leftovers the UA–UO lanes recorded.** These are not X360 rows. Each count is the lane's own,
   and none has been touched since:
   - W16-UB: 1,161 `-Wshorten-64-to-32` sites counted, not triaged. Byte order was not swept as a class. A byte-order
     lane (W16-UE) was running at the time of writing and has not landed.
   - W16-UJ/UL: rb3-render still skips 9 factory classes (`Sfx`, `SynthSample`, `MoggClip`, `BandCamShot`, …). DC3's
     async-unload change `b01fd8ba7` is not ported. Each venue keeps about 13 MB of RSS. The `video_04`–`07`
     byte-identical renders are not investigated.
   - W16-UG: 122 of retail's 284 script names, from 37 registrars, are absent natively.
   - W16-UC: the title-update archive (`gen/patch_xbox.hdr`) is not audited, and the compiled-in scoring/crowd config
     was never compared with `config/gen/scoring.dtb`.
   - W16-UA: no native driver runs the real `PreInitSystem`. `config/band_keep.dta` is read nowhere.
   - W16-UO: LeakSanitizer is off, ASan globals are off, `ObjRef::SafeReleaseFromRing` is still unsanitized, and
     `native_asan.sh` is not wired into a gate.
5. **[C] U2 identification residue: 129 rows / 7,124 B** (in scope 74 / 4,488, VIA-DC3 55 / 2,636). 10-07c had
   207 / 14,652. UM/UN/UP drained 78 rows. In scope 30 rows / 312 B are ≤ 16 B. Bookkeeping unless a new channel
   appears.
6. **[C] Name and pin leftovers, each looked up on this tree:**
   - W16-UM's 13 NO-BASE-OBJ addresses (ObjPtr dtor/Load ×6 at `0x822D65E8`… in StreakMeter holes, `GetAward`
     `0x823EA5A8` 12 B, `Gem::Gem` `0x82BAC148` 316 B in a Gem.cpp hole, three 8 B getters in XDK-adjacent runs,
     ssmdevice ×4) are all still unnamed at 0% in `auto_*`. This is identification, not pins (UN §Not done).
   - The CRT ctor `0x8282A080` (unnamed, 0%, UM). W16-TV's lead G846 `0x824DFA50` (36 B, Spotlight, 0%). Its other
     lead, G971, is now named and at 100.
   - `0x82272A60` fill_insert, 108 B, unpinned (TU/TX), outside the ceiling.
   - The probable VorbisReader rows `0x82BB33D8` / `0x82BB3430` / `0x82BB3510` are paired at 100 under
     StreamReceiver360, Gem and Mesh. Moving them is a re-home, which UP declined.
   - TU's new MetaPanel `fn_82574D90` 100 B and TrackPanelDir `fn_82308470` 4 B (U2, in scope). `fn_82C3FA94` 40 B
     (fell to 0, §2).
   - W16-TZ's `ObjRefOwner` → `ObjRef` rename covers 23 rows, all at 100. It is naming only.
   - W16-TV's 886 PROVEN memberships resting on a `SAMENAME-UNVERIFIED` slot are a bound, not a finding. They need
     callee source work, not alias work.
7. **[C] Single-site leftovers, all still in the gap, each with an owner in its lane doc: 4,672 B.**
   - `Plane(point, normal)`'s FP fold: `Spotlight::DrawShowing` 904 (93.89) and `SpotlightDrawer::DrawShadow` 328
     (94.18). A header-wide change.
   - `~FaderGroup` 160 (97.50).
   - `SynthEmitter` ctor 496: PCH-blocked. A PCH opt-out needs its own three-gate A/B.
   - ANCHOR `Character::PostLoad` 1,440 (99.65) and `RndMatAnim::Load` 624 (98.11).
   - `MeshCacher::Sync` 620 (no channel 2).
   - `SendDataPoint<const char*,int>` 100. Its only caller is the unpinned TU5 RBN-audition run
     `auto_03_825632BC_text`, which has no source.
8. **[C] Tooling.**
   - W16-TO's and W16-TQ's evaluators are not committed. W16-TP's `row_valueflow.py` is, and lever 1 can use it.
   - `native_runtime_rank.py` and `native_stub_census.py` were each broken by a later lane and needed a fix before
     this edition could run them (§8). Both failed loudly (KeyError, exit 3), which is the good failure mode. But each
     edition finds a new break, so a smoke test in `native_build_gate.sh` would catch it at the lane that causes it.
     This is my suggestion, not a lane's finding.
   - `native_runtime_rank.py` still has no gated/ungated column (10-07c lever 8).

**Not ranked, for the user to decide (unchanged):** the permuter pilot.
- In scope: buckets 20, 60 and 61 hold 160,220 B of codegen residue, every row now read for behaviour, and 216 rows
  are at `mpn` 100.
- VIA-DC3: the rows read and left as codegen (buckets 50, 53, 62 and the SF/SV SCHED rows) add 136,332 B.
- Recorded single-instruction prizes are unchanged: `App::App` (1,864 B), `SaveLoadManager::SetState` (4,096 B) and
  `ParseNode` (2,432 B).

**Out of ranking by directive:** Quazal 113,520 B, XDK 2,948 B, 360-only 52,096 B, NET 3,828 B.

### 6.1 10-07c's levers, now

| 10-07c lever | taken by | result | state |
|---|---|---|---|
| 1. register/reorder rows with no per-row retail read: in scope 106 / 64,192 B, VIA-DC3 49 / 47,276 B | W16-TO (57), TP (49), TQ (49) | **155 / 155 EQUAL**. No source change kept; TP's side fix `EstimateDraw` (a TK regression at fuzzy 100) | **drained**; TG's bucket 53 is the remaining twin, lever 1 above |
| 2. native execution: in scope 166 / 111,036, VIA-DC3 173 / 145,432 | in scope TS, TW, UF; VIA-DC3 TY | in scope: behaviour-class unentered 45 → 3 rows / 436 B. VIA-DC3: TY took 42 rows (every row ≥ 1,332 B, plus 9 smaller), 23 gates, 3 native-only bugs. Executed bytes 68,168 → 99,072 in scope, 22,212 → 84,188 VIA-DC3 | in scope **drained**; VIA-DC3 lever 3 above |
| 3. VIA-DC3 files native does not compile, 14,108 B / 24 | W16-TM, TR | 856 B / 1 file (StoreEnumeration, replaced by rule) | **drained** |
| 4. `SendDataPoint`'s caller, 100 B | none | unchanged | lever 7 above |
| 5. U2 residue, 207 / 14,652 B | W16-UH → UM, UN, UP | 129 / 7,124 B | open, lever 5 above |
| 6. single-site leftovers, 4,396 B | W16-TT (`DoFade`) | `DoFade` 444 left the gap; the rest unchanged | open, lever 7 above |
| 7. name leftovers, 896 B | W16-TT, TV (SW #12/#13) | 276 B left the gap; `MeshCacher::Sync` unchanged | lever 6/7 above |
| 8. tooling | W16-TP (`row_valueflow.py` committed), W16-TV (alias proof gaps) | TO/TQ evaluators still uncommitted; two tools broken by later lanes, fixed here | lever 8 above |

**W16-TP's recommended constant census, answered here.** TP found that `name_check` forgives a wrong float constant
when the literal-pool target is a placeholder `lbl_`. It recommended a census of fuzzy-100 rows. That census already
exists as W16-PW's `tools/const_value_audit.py`, so I reran it on this tree:

| run | position pairs | unequal | VALUE | SWAP | POS_UNRES | SETONLY |
|---|---:|---:|---:|---:|---:|---:|
| W16-PW final (2026-10-06) | 27,998 | 193 | 111 | 8 | 5 | 186 |
| this tree | 28,947 | 176 | 110 | 6 | 2 | 155 |

- **In scope and VIA-DC3, no VALUE or SWAP row is open.** Every one is adjudicated in a lane doc:
  - `BandCrowdMeter::Poll`, `Spotlight::BuildNGQuad`, `GetBlendState`, `ResetViewports`, `WahEffect::Process`,
    `VoiceBeat::Analyze` and `EstimateDraw` are equal in W16-PW's table. `EstimateDraw`'s SWAP is the summation
    order, and TP verified the weights per counter.
  - `OggMap::SetKey` (W16-RG/SM) and RockCentral `fn_824F6768` (W16-SM) are misalignment.
  - POS_UNRESOLVED `MemTracker::DiffDump` and `BlurSurface` are equal (PW, TO).
- **No fuzzy-100 row in either ring carries a VALUE or SWAP flag.** The four fuzzy-100 SETONLY rows in those rings
  (`PrintDiscFile`, `DataDirname`, `RecursePatternInternal`, `LensSym_to_FOV`) each show a retail-only `ffffffff`
  with an unresolved operand on our side. That is W16-PW's extern-load class (`FixedString::npos`), not a value.
- Outside the rings: 102 VALUE rows are Quazal `__FILE__` strings, and `DxParticleSys::Init` /
  `DxRnd::DrawRectDepth` are PW's real-but-out-of-scope `D3DDECL_END` stream value. Both are left by directive.
- **Bound:** the audit compares position-paired operands. A wrong constant in an instruction it cannot pair is
  invisible to it, as PW §Coverage states. `EstimateDraw`'s TK regression was found by TP's per-counter evaluator,
  not by this tool.

## 7. Reading the numbers

- **In-scope match share is 94.87%** (10-07c: 94.76%). The whole-binary share of the ceiling is **91.60%**.
- **Every in-scope gap row now has a per-row read or a stop reason, and the register rows found nothing.** TO and TP
  read the 106 rows 10-07c called the leakiest population and found 0 defects. So 160,220 B, 81% of the in-scope
  gap, is codegen residue read row by row. The behaviour-class rows are either fixed, stopped with a reason, or
  (3 rows / 436 B) known to be unreachable natively.
- **VIA-DC3 has one unread register population left: TG's 62 rows / 30,416 B.** Everything else in the ring has a
  per-row retail read.
- **Native now executes half the in-scope gap bytes (50.2%) and 45% of VIA-DC3's.** At 10-07c it was 34% and 12%.
  This round's gates found native bugs, and no X360 behaviour difference.
- **What is left that moves X360 bytes is mostly codegen (the permuter question), naming, and pinning.** The pinning
  lever (lever 2) does not move the gap either; it verifies fixes the ruler cannot see yet.

## 8. Not done

- **No source, map, splits, alias or `symbols.txt` edits.** Nothing was A/B-measured, because this branch changes no
  match-build input. The native gate was not run (no `src/` change).
- **Two tool fixes are on this branch**, needed to run the native columns at all:
  - `tools/native_runtime_rank.py`: W16-UA/UD added `--config-dump "$CFG_ARK"` to rb3-ark's line in
    `native_health.sh`, and the ranker raised `KeyError: native_health.sh uses $CFG_ARK`. It now derives `CFG_ARK`
    from LOGDIR/SLUG the way `native_health.sh` does.
  - `tools/native_stub_census.py`: its exit-3 guard fired on 10 unclassified objects. W16-TR's `w16tr_link_stubs.cpp`
    and W16-TS's `w16ts_link_support.cpp` are stubs. The `w16t[mrswy]_phase.cpp` and `w16u[bfj]_phase.cpp` files are
    gate drivers.
- **No per-lane walk** (§3). Ring attribution per lane is not available this edition.
- **Native columns are one merge old.** They were measured on `65dc0dbc4`'s native build. W16-UO changed native code
  (ASan fixes, one allocator family for `Hmx::Object`) after that. Its own health run reports all 18 targets
  ASan-clean.
- **The overlays are coarse, as before.** A lane verdict joins by population file and carry-forward, not by
  re-reading each row. "Taken by a lane" in `execlever.py` means the row's short name appears in backticks in that
  lane's doc.
- I did not rerun `icf_pair_adjudicate.py --chasetest` or `tools/placeholder_callee_census.py`. The chase verdicts
  feeding the name buckets were recomputed with the same instrument 10-07c validated. UH's census was last run at
  `b070a9c6d`, and UK/UM/UN/UP changed callees since.

Scripts and JSON are in `~/tmp/w16ur-gap/` and are not committed. Reproduce in this order, from that directory, with
`W=~/tmp/wt-w16ur` built at `12d4ebdfe`. W16-TN's 10-07c outputs must exist in `~/tmp/w16tn-gap/` and the three lane
population files in `~/tmp/w16to`, `~/tmp/w16tp` and `~/tmp/w16tq`.

```
git -C $W show b79d790b8:scripts/target_symbol_map.json > map_b79d.json
python3 gengap.py $W gap.json                      # 1,576 gap rows, asserts ceiling - matched
python3 scope_ledger2.py $W ledger2.json           # ring ledger, asserts total_code and matched_code
python3 diffall.py $W gap.json cls.json            # one objdiff diff per gap row
python3 unpaired.py $W                             # U2/U3/U4 split
python3 lanever.py                                 # 10-07c's lane verdicts (TE/TI/TK/TG/TH)
python3 overlay6.py                                # overlay5 + 10-07c carry-forward + TO/TP/TQ pools (asserted) + W16U* mentions
python3 insdel_shape.py $W
python3 tables3.py > tables3.txt
python3 mkcensus.py && python3 pairs.py && python3 chase_all.py
python3 namecls.py && python3 final3.py            # placeholder-only control (none == graded)
python3 tables4.py > tables4.txt                   # §4.4
(cd $W/native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)   # configure only
(cd $W && python3 tools/native_linked_tus.py . --out ~/tmp/w16ur-gap/native_linked.txt --jobs 8)
                                                   # native_src.json = that list ∪ the .c compile edges (717 files)
python3 dispo6.py > dispo6.txt                     # §4 table + carry control (asserts)
python3 dispo_via6.py > dispo_via6.txt             # §5 table + carry control (asserts)
(cd $W && CMAKE_BUILD_PARALLEL_LEVEL=6 python3 tools/native_runtime_rank.py . --out ~/tmp/w16ur-gap/rank.tsv --json ~/tmp/w16ur-gap/rank.json)
(cd $W && python3 tools/native_stub_census.py native/build ~/tmp/w16ur-gap/stubs.json --authored)
python3 stubmatch.py; python3 execlever.py         # §4.3
python3 cmp1007c.py > movement.log                 # §2 (asserts)
python3 rowsdown3.py > rowsdown.log                # §2 whole-binary rose/fell
(cd $W && python3 tools/const_value_audit.py --out ~/tmp/w16ur-gap/cva.json)   # §6.1
python3 chk/where.py <addr> ...; python3 chk/sandwich.py; python3 chk/nbr.py <addr>   # §6 lookups
```

- **New in this edition:** `overlay6.py`, `dispo6.py`, `dispo_via6.py` (buckets 60/61/62 and the carry control),
  `cmp1007c.py`, `rowsdown3.py`, `execlever.py`, and `chk/where.py`, `chk/nbr.py`, `chk/sandwich.py`.
- Per-row tables: `dispo_rows.json` (in scope) and `dispo_via_rows.json` (VIA-DC3). Each row carries its 10-07c
  disposition (`d1007c`) and its retail address (`addr_retail`).
