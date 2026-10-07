# Campaign state — 2026-10-07c (after W16-TB … W16-TK)

Eleventh edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-07b.md` (W16-TA) for numbers
and priorities. It reruns W16-TA's method, with these changes, each stated where it is used:

- **New buckets.** This round's row lanes (W16-TE, TI, TK, TG, TH) get per-row buckets ahead of the older records
  (§4.1). Their verdicts are joined from each lane doc's table, not from a doc mention.
- **Execution column.** `tools/native_runtime_rank.py` (W16-PK, fixed by W16-TF) was rerun on this commit, so the
  native column now has a "compiled" and an "executed" figure (§4.3).
- **Movement base.** Row movement is measured against W16-TA's run (`~/tmp/w16ta-gap`), not W16-SD's.
- **Walk dtk.** The per-lane walk builds steps before W16-SY with the dtk 1.14.0 binary SY backed up at its fleet swap,
  and SY onward with live 1.15.0, so SY's split change lands on SY's own step (§3).

12 merges, 4 native pins, 1 native fix-up and 1 splits fix-up landed between the two measurements. Scope rules are
W16-OV's: the native port is the goal, XDK is hard-skipped, Quazal stays as landed. No source, map, splits, alias or
`symbols.txt` edit on this branch.

## 1. Measured

I used a fresh `scripts/setup_worktree.sh` worktree (`~/tmp/wt-w16tn`) at main **`b79d790b8`** (W16-TK) and ran a
full `./tools/ninja-locked`. The build ended `BUILD_RC=0` with `[pairing] 1056/1056 declared compiled objects pair
with a target` and `[patch-state] OK: tree is a fixed point of 6 post-compile passes`; `symbols.txt` did not move. I
then wiped `report.json` and `report.cache` and regenerated them (`Report cache: 0 hits, 3118 misses`). Ruler:
`name_check` (objdiff 4.2.9, tool commit `a5f0ea903ec1`, binary hash `5a51cd51fe0a353f`, read from `report.json`'s
`provenance`). dtk is 1.15.0 (`cd2495ca`), the binary W16-SY deployed.

**Prediction, written before reading `report.json`:** the merges' own A/B claims sum to **+45 fns / +7,440 B**:

| lane | claim |
|---|---|
| TB | +2 / +84 |
| TG | +9 / +3,564 |
| TI | +8 / +1,336 |
| SY | +1 / +448 |
| TH | +22 / +1,416 |
| TK | +3 / +592 |
| TD, TE, TF, TJ, cva-ram | Δ0 each |

So I expected `matched_code` 6,078,232 and `matched_functions` 54,939, with the ceiling unchanged unless TH's new
bodies lifted a scaffold unit over the cliff. **All three held exactly.**

```
python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main b79d790b8 (2026-10-07, W16-TK)"
  scaffold thr<=4 : 0 · thr<=5 : 0 · thr<=6 : 56 units, 289 rows, 57,448 B · thr<=7 : 56 (cliff, not fitted)
total_code            10,247,844
PAIRABLE               6,703,512 = 65.414%   (1,056 units, 56,318 rows)
− scaffold shells         57,448             (56 units, 289 rows)
= reachable ceiling    6,646,064 = 64.853%
matched_code           6,078,232 = 59.312% of total_code = 91.46% of ceiling
gap to ceiling           567,832
matched_functions         54,939 / 68,884   masked_equal 25,214   honest 29,725
fuzzy_match_percent       64.31659
```

Against 10-07b (`2fd722b8f`): **+7,440 B / +45 fns**. The share of the ceiling went 91.34% → **91.46%** and the gap
575,272 → **567,832 B (−7,440)**. The ceiling, `total_code`, `total_functions` and the row count did not move. Two
lanes re-homed `.text` between named units (TB, TH), which moved reach between rings but not the ceiling (§2).

| control | result |
|---|---|
| attribution | the round's claims sum to +45 / +7,440, residual **0 / 0** against the measured Δ (`claims2.py`) |
| lane walk, step 0 | `2fd722b8f` rebuilt here equals W16-TA's 10-07b ledger on every ring and its archived report `rep_2fd722b8f.json.gz` (`lane_rings3.py`) |
| lane walk, last step | `b79d790b8` from the walk equals this worktree's build on `matched_code`, `matched_functions` and every ring |
| independent baseline | main's own `build/45410914/report.json` (11:06, at `b79d790b8`) equals this build on all seven headline keys |
| population sums | gap rows (1,685) sum to 567,832 = ceiling − matched (`gengap.py`) |
| ring sums | tier totals sum to `total_code` and `matched_code` (`scope_ledger2.py` asserts both) |
| disposition tables | §4's buckets sum to 543 rows / 201,360 B = in-scope ceiling − matched (`dispo5.py` asserts); §5's to 442 rows / 192,072 B = the VIA-DC3 gap (`dispo_via5.py` asserts) |
| row movement | in scope: entered − left − size changes = −964 B = Δ in-scope gap; VIA-DC3: −6,476 B = its Δ gap (`cmp1007b.py` asserts both) |
| lane-verdict join | all 67 TE/TI/TK rows and all 70 TG rows join to a row whose name contains the doc's short name (`lanever.py` asserts counts; audit 0 mismatches) |
| placeholder correction | 24/24 placeholder-only N2 rows read identical fuzzy at `none` and graded |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail" |
| unpaired index | 200/200 paired named rows found in their own base obj |
| native set | `native_linked_tus.py` self-validation passed; its not-compiled in-scope remainder is the same **5 files / 2,032 B** as at 10-07b |
| stub match (§4.3) | the qualified-name matcher finds 204 in-scope rows at 100 that a live stub also defines, so its 0 for gap rows is not vacuous |
| runtime rank (§4.3) | all 18 targets `rc=0` with a non-empty profile (rb3-render 10.4 MB); the tool's self-validation passed (it exits 3 otherwise); its ring totals equal this build's ledger to the row and byte (543 / 201,360 and 442 / 192,072); every in-scope and VIA-DC3 gap row is present in its output (`execjoin.py`) |

## 2. Rings

From `cmp1007b.py` (this build's `ledger2.json` against W16-TA's).

| tier | reachable | matched | **gap** | share | gap at 10-07b | Δ matched | Δ fns | Δ reach |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 478,128 | 434,376 | **43,752** | 90.85% | 43,888 | +0 | +0 | −136 |
| IN-SOON | 2,440,392 | 2,340,588 | **99,804** | 95.91% | 100,336 | +452 | +2 | −80 |
| IN-RB3ENG | 927,308 | 869,504 | **57,804** | 93.77% | 58,100 | +540 | +8 | +244 |
| **in scope** | **3,845,828** | **3,644,468** | **201,360** | **94.76%** | 202,324 | **+992** | **+10** | +28 |
| VIA-DC3 (DC3 has the file) | 2,022,968 | 1,830,896 | 192,072 | 90.51% | 198,548 | +6,416 | +33 | −60 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 113,520 | 0 | 0 | 0 |
| OUT-NET | 76,936 | 73,068 | 3,868 | 94.97% | 3,868 | 0 | 0 | 0 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 | 0 |
| OUT-360-OTHER | 349,068 | 296,868 | 52,200 | 85.05% | 52,200 | 0 | 0 | 0 |
| UNKNOWN-other | 3,564 | 1,700 | 1,864 | 47.70% | 1,864 | +32 | +2 | +32 |

- **Native scope CORE+SOON:** 99.08% of functions and 95.08% of bytes (10-07b: 99.05% / 95.06%).
- **VIA-DC3 took 86% of the round:** +6,416 B against +992 B in scope. Its gap fell 6,476 B (−3.3%), the in-scope gap
  964 B (−0.5%). The VIA-DC3 gap (192,072) is now 9,288 B below the in-scope gap (201,360).
- **Reach moved between rings, net 0**, in exactly two walk steps:
  - W16-TB: IN-SOON −80, VIA-DC3 +80 (its three `.text` re-homes).
  - W16-TH: IN-CORE −136, VIA-DC3 −140, IN-RB3ENG +244, UNKNOWN-other +32 (its re-homes; the +32 is
    `SetFileChecksumData` and `SetSongMidiChecksumData`, now in the new root-level TU `src/ChecksumData_xbox.cpp`,
    which no ring classifier claims).
  - A re-home between named units does not change the ceiling, and it did not.

**Gap movement, keyed on (unit, symbol):**

- **In scope, left: 11 rows / 972 B** (`leftwhere.py`):

  | where the row is now | rows | bytes |
  |---|---:|---:|
  | same unit, fuzzy 100 (`GetJoypadExtraLagInits`, W16-SY's addend row) | 1 | 448 |
  | re-homed to another in-scope unit at 100 (`Find<RndCam>`, `~ObjPtr<BandStarDisplay>`, `SetTrackPanel`) | 3 | 288 |
  | renamed or re-homed out of its unit (TB/TH: `SetObjConcrete`, `Achievements::Terminate`, `SetSongMidiChecksumData`, the `UIProxy` thunk, the `map<int,float>` dtor, `BandRetargetVignette::Enter`) | 6 | 220 |
  | moved to UNKNOWN-other at 100 (`SetFileChecksumData`) | 1 | 16 |

  By their 10-07b disposition: dtk-addend row 1 / 448, unpaired named (U3/U4) 7 / 424, U2 3 / 100. **10-07b's
  open in-scope surface (the 8 unpaired rows, 524 B) is empty**: 7 left, and the 8th (`SendDataPoint<const char*,int>`,
  100 B) carries W16-TH's stop reason.
- **In scope, entered: 1 row / 8 B**, `fn_822DC828` (U2, 0%), the 8 B anonymous word W16-TH moved with
  `New<RndMesh>` into ChordShapeGenerator. No in-scope row still in the gap changed size; one rose, none fell.
- **VIA-DC3, left: 30 rows / 6,476 B, entered 0, size changes 0.** By 10-07b disposition: W16-PU/QF register class
  6 / 3,564 (TG's six rows at 100), source class 5 / 1,768 (TI's three and TK's two), unpaired named 13 / 804 (TH),
  U2 2 / 180, funclets 4 / 160 (TI's `DoFade` funclets). 24 rows still in the gap rose and 2 fell: `RndFlare::DrawShowing`
  98.53 → 97.27 (W16-TK §4.14, kept on purpose) and the AO funclet `fn_82492CEC` 99.9 → 99.8 (W16-TE §5, recorded).
- **Whole binary**, over the 68,855 rows present under the same (unit, name) in both trees: **42 rose, 2 fell**, the
  two above (`rowsdown2.py`, against W16-TA's archived report). No row fell from 100.

## 3. What landed, per lane

The lane worktrees are gone, so I rebuilt every step with W16-TA's `walk.sh`, repointed to a private worktree
(`~/tmp/wt-w16tn-walk`):
- **Steps:** step 0 (`2fd722b8f`), then every first-parent commit up to `b79d790b8` that touches a match-build input
  (`src/`, `config/`, the map and alias JSON, `configure.py`, `tools/project.py`).
- **W16-TB is measured at `f83a5d708`**, the splits commit that carries the four `.pdata` lines TB's merge lacked. TB's
  merge alone stops at the split guard (W16-TD's motivation).
- **Left out:** the native pins (`3d07cb6f2`, `d092a9458`, `b4d48c819`, `609330a5c`), `111e836f9` (native stub
  drop), and the W16-TA, W16-TD and cva-ram merges (docs and tools). None touches one of those paths.
- **Per step:** build until `symbols.txt` is unchanged by a build (every step took one), wipe `report.json` and
  `report.cache`, regenerate and snapshot. Clean TU5 image throughout. dtk 1.14.0 (`67e4a0f9`, the backup W16-SY
  saved) before `d5cbd3cbc`, live 1.15.0 from it on.
- **One rerun:** step 0's first attempt failed at configure, because the worktree's dtk-generated
  `build/45410914/config.json` was left from the setup build at `b79d790b8` and names W16-TH's new
  `ChecksumData_xbox.cpp` unit. Step 0 is the only step that moves backward in history. I reran it with that file
  removed, and it built and converged on the first build. Both attempts' logs are kept.

Controls, all predicted to hold before the walk ran, and all hold (`lane_rings3.py`):
- **Step 0** equals W16-TA's 10-07b ledger on every ring, and W16-TA's archived report on `matched_code`,
  `matched_functions` and every ring.
- **The last step** equals this worktree's build on `matched_code`, `matched_functions` and every ring.
- **The steps sum** to +45 fns / +7,440 B whole-binary, +992 B / +10 fns in scope and +6,416 B VIA-DC3, which is §2
  exactly.
- **Every lane equals its own A/B claim**, fns and bytes. `total_code` and the row count move on no step.

Three steps read zero on every ring, as their lanes measured: W16-TF, TE and TJ. They are left out of the table.

| lane | step | Δ fns | Δ matched_code | Δ in scope | CORE / SOON / RB3ENG | Δ VIA-DC3 | other rings |
|---|---|---:|---:|---:|---|---:|---|
| W16-TB | `f83a5d708` | +2 | +84 | **+4** | +0 / +0 / +4 | +80 | — |
| W16-TG | `e58ac757d` | +9 | +3,564 | **+0** | +0 / +0 / +0 | +3,564 | — |
| W16-TI | `ba4cbe0a2` | +8 | +1,336 | **+0** | +0 / +0 / +0 | +1,336 | — |
| W16-SY | `d5cbd3cbc` | +1 | +448 | **+448** | +0 / +448 / +0 | +0 | — |
| W16-TH | `c84b6bf3d` | +22 | +1,416 | **+540** | +0 / +4 / +536 | +844 | UNKNOWN-other +32 |
| W16-TK | `b79d790b8` | +3 | +592 | **+0** | +0 / +0 / +0 | +592 | — |
| **sum of steps** | | **+45** | **+7,440** | **+992** | +0 / +452 / +540 | **+6,416** | +32 |

- **VIA-DC3 row reads were most of the bytes:** TG, TI and TK are 5,492 of 7,440 B, all VIA-DC3, as briefed (10-07b
  levers 1 and 5). Between them and TE they recorded **8 behaviour fixes visible to native**, plus TE's latent
  `mGeomOwner` double dereference: TG's `Bloom_Blur` parameter roles and `ComputeFaceTangentBasis` notify; TE's `Tessellate` prints and `WrapText` line indices; TI's
  `WorldCrowd::SetFullness` splice end; TK's two `DecodeDxt5Alpha` defects and `EstimateDraw`'s six weights.
- **In scope, the round was 10-07b's lever 2 and the dtk addend:** TH's re-homes, respellings and three missing
  bodies (+540 B in scope), which exposed two callee bugs (OvershellSlot's six cues through `PlaySound` instead of
  `Synth::Play`, and out-of-line class `operator delete` in three classes), and SY's dtk 1.15.0 (+448 B, the one
  in-scope row whose only charge was a dropped relocation addend).
- **W16-TF and W16-TJ are native lanes at X360 Δ0.** They fixed the runtime ranker's blind spot on rb3-render, added
  55 gates (31 + 24), and fixed two native-only bugs (`MakeBSPTree` compiled to `return false`; `UpdateScrolling`'s
  one-past-the-end `&v[cursor]` abort). §4.3 measures where that leaves execution.

## 4. Where the in-scope gap stands: one disposition per row

Each in-scope gap row (543 / 201,360 B) goes to the first bucket that fits (`dispo5.py`). **"Native-compiled"**
means the row's source file is a TU of at least one target of the native build, as the compiler reports it (§4.3).

| disposition | rows | bytes | % | CORE | SOON | RB3ENG | native-compiled |
|---|---:|---:|---:|---:|---:|---:|---:|
| read for behaviour by W16-PQ/PR: no defect left, codegen residue | 107 | 96,028 | 47.7% | 21,680 | 45,924 | 28,424 | 94,888 |
| register/schedule: named in a later lane's doc, stopped | 52 | 38,200 | 19.0% | 5,968 | 19,964 | 12,268 | 38,200 |
| register/schedule: W16-PF's lever applied, stopped, no later record | 38 | 17,832 | 8.9% | 0 | 17,832 | 0 | 17,396 |
| source divergence: worked since 10-06, left with a stop reason | 27 | 11,340 | 5.6% | 2,752 | 2,868 | 5,720 | 11,340 |
| identification residue (U2) | 118 | 8,384 | 4.2% | 2,340 | 3,984 | 2,060 | 8,232 |
| name: pointer `_M_fill_insert` fold, settled not admissible (W16-OX) | 14 | 6,760 | 3.4% | 2,388 | 3,572 | 800 | 6,760 |
| EH funclet / anonymous fragment of a parent row | 158 | 6,488 | 3.2% | 1,008 | 3,604 | 1,876 | 6,436 |
| register: W16-PS's lever 4/5 applied, stopped | 11 | 5,464 | 2.7% | 2,736 | 1,248 | 1,480 | 5,464 |
| never-worked row taken by W16-PO, left with a stop reason | 9 | 5,188 | 2.6% | 2,356 | 0 | 2,832 | 4,936 |
| register/schedule: named in a W16-S* doc, stopped | 5 | 2,696 | 1.3% | 264 | 708 | 1,724 | 2,696 |
| **settled by W16-TH, left with its stop reason** (`SuperFormatString` ctor 1,608, `SendDataPoint<const char*,int>` 100) | 2 | 1,708 | 0.8% | 1,608 | 100 | 0 | 1,708 |
| source divergence: named in a W16-S* doc, left (`PreInitSystem`, W16-SM) | 1 | 652 | 0.3% | 652 | 0 | 0 | 652 |
| name: parked by W16-SW, no second channel (`MeshCacher::Sync`) | 1 | 620 | 0.3% | 0 | 0 | 620 | 620 |
| **total** | **543** | **201,360** | 100% | 43,752 | 99,804 | 57,804 | **199,328** |

Against 10-07b, four lines changed and the rest are byte-identical: U2 −2 rows / −92 B; W16-SG's `SuperFormatString`
record is replaced by W16-TH's newer one; the unpaired named bucket (8 / 524) and the dtk-addend bucket (1 / 448)
emptied.

### 4.1 How the buckets are decided

`dispo5.py` is W16-TA's `dispo4.py` with this round's records inserted where they are the most specific. The
classes, chase verdicts and older pools are unchanged and recomputed on this tree (`diffall` → `final3` → `tables4`,
W16-TA's chain, repointed). The additions:

1. **Per-row lane verdicts (`lanever.py` → `lanever.json`).** Each lane's population comes from a file on disk, and
   each verdict is joined from the lane doc's table on (short name, size), with the counts asserted:
   - W16-TE / TI / TK: 10-07b's VIA-DC3 bucket "source-class charge" (67 rows / 70,984 B, re-read from
     `~/tmp/w16ta-gap/dispo_via_rows.json`), split 10 / 20 / 37, each verdict from the lane's §2 or §3 result table.
   - W16-TG: `~/tmp/w16tg/pop70.json` (70 rows / 34,436 B) with `~/tmp/w16tg/dispo_rows.md`.
   - W16-TH: its three stop rows (`SendDataPoint`, the `SuperFormatString` ctor, `OggMap::OpenMogg`).
2. **Where they go.** The class-keyed buckets keep precedence, as before (TU5 image, funclets, name classes by chase
   verdict). Then:
   - `42` W16-TH stop row, ahead of U2/U3/U4 (so `SendDataPoint` takes TH's reason, not the class bucket).
   - `50` read against retail by TE/TI/TK, EQUAL (including TE's bare HOME / SCHED / LAYOUT rows); `51` FIXED or
     PARTIAL with the residue recorded; `52` TE's `SynthEmitter` ctor, inline blocked by the PCH.
   - `53` DC3 body compared by W16-TG and left.
   - Then W16-TA's order unchanged (W16-SF/SV reads, W16-SG, W16-PQ/PR, …).
3. **Doc mentions** now include the `W16T*_2026-10-07.md` docs, in the same W16-S* mention bucket. In scope no row
   lands there that was not already there.

### 4.2 How it adds up

- **Codegen residue with a behaviour or lever record: 160,220 B, 79.6%** (buckets 20, 23, 11, 22 and 25), unchanged
  to the byte since 10-07b. No lane this round worked a codegen row in scope.
- **Source-class rows with a stop reason: 18,888 B, 9.4%.** W16-PO's 9 rows, 27 rows worked since 10-06, W16-TH's two
  stop rows and `PreInitSystem`.
- **Settled, non-separable or parked: 22,252 B, 11.1%.** Funclets, W16-OX's fill_insert fold, W16-SW's
  `MeshCacher::Sync`, and the U2 residue (118 rows / 8,384 B).
- **Open: 0 B.** At 10-07b it was the 8 unpaired named rows (524 B). **Every in-scope gap row now carries a per-row or
  per-lever stop reason.**

### 4.3 Native relevance

**Compiled.** The native set is the compiler's answer (`tools/native_linked_tus.py`, `clang++ -M` over every
target's TUs, on a `native/build` I configured in this worktree), unioned with the build's compile edges for the
json-c `.c` files: **642 files** (10-07b: 620). The new 22 are W16-TJ's link closure (`ClosetMgr`, `PrefabMgr`,
`CharCache`, the `Asset*` and provider files, `StoreOffer`, `SlipTrack`, `FxSendDelay`/`FxSendSynapse`, …). The stray
`os/MasterAudio.cpp` 10-07b counted is gone.

| measure | in scope | VIA-DC3 |
|---|---:|---:|
| gap | 201,360 | 192,072 |
| **native-compiled** | **199,328 (99.0%)** | **177,964 (92.7%)** |
| at 10-07b | 200,292 (99.0%) | 182,140 (91.7%) |
| not compiled | 2,032 in 5 files | 14,108 in 24 files |
| **executed by a native target** | **68,168 (33.9%), 87 rows** | **22,212 (11.6%), 31 rows** |

- **In scope, the not-compiled remainder is unchanged:** `net_band/RockCentral.cpp` 664, `os/AsyncFile_Win.cpp` 460,
  `net_band/XboxEntityUploader.cpp` 436, `os/ThreadCall_Win.cpp` 252, `net_band/ContextWrapper.cpp` 220. W16-SH §4's
  reasons stand (a Quazal client, an XDK service, two Win32 backends).
- **The native-compiled in-scope files with the most gap:** VocalTrack.cpp 11,532, BandPatchMesh.cpp 7,584, Geo.cpp
  6,308, GemManager.cpp 5,492, CustomizePanel.cpp 5,428, BandDirector.cpp 4,620, SaveLoadManager.cpp 4,096,
  BandCamShot.cpp 3,792, Dir.cpp 3,624, NoteTube.cpp 3,576.
- **No in-scope gap row is replaced by a live native stub.** `tools/native_stub_census.py native/build stubs.json
  --authored`, run read-only on main's native build (linked 11:08, after `b79d790b8`), now lists both stub files 10-07b
  had to patch in by `nm` (W16-TF fixed `STUB_RE`); 1,016 live stub symbols. None matches an in-scope gap row by
  qualified name; 204 in-scope rows at 100 do. In VIA-DC3 the same two gap rows as at 10-07b match a stub in some
  target: `CacheResource` (304 B, 38.42) and `Fader::DoFade` (444 B, now 99.95).

**Executed.** I reran `tools/native_runtime_rank.py` on this commit. It builds all 18 targets with clang's profile
instrumentation in `native/build-prof` (here at `CMAKE_BUILD_PARALLEL_LEVEL=6`), runs them on `native_health.sh`'s
inputs, and joins each function's entry count to the report rows by demangled signature. A row is "executed" when a
target entered it at least once (`execjoin.py`).

| | in scope | VIA-DC3 |
|---|---:|---:|
| executed | 87 / 68,168 | 31 / 22,212 |
| joined to a native definition, never entered | 166 / 111,036 | 173 / 145,432 |
| anonymous `fn_` (cannot join by signature) | 276 / 14,872 | 206 / 11,200 |
| no native definition, or unparsed | 14 / 7,284 | 32 / 13,228 |

- **In scope this is the same 87 rows W16-TJ measured at `d5cbd3cbc`**, compared row by row. In VIA-DC3, TJ read
  32 / 22,284 B. The only difference is W16-TH's `New<RndMesh>` (72 B, executed 903 times), which reached 100.
- **Behaviour-class named rows in native-compiled files** (I3/I4/M1/M2/U3/U4): in scope 101 rows / 82,888 B, of which
  **32 / 33,376 executed**. Of them, W16-PQ/PR's EQUAL rows are 74 / 70,300 B, with 19 / 26,512 executed. VIA-DC3:
  94 / 85,472, of which **16 / 12,528 executed**.
- **The largest in-scope rows that native links but never enters:** `SaveLoadManager::SetState` 4,096,
  `BandDirector::OnFileLoaded` 3,816, `UIStats::MaybePublish` 2,604, `StoreOfferProvider::BuildList` 2,536,
  `CharKeyHandMidi::Poll` 2,236, `MusicLibraryNetSetlists::ParseDataResult` 1,968,
  `MetaPerformer::SelectRandomVenue` 1,924, `BandWardrobe::LoadMainCharacters` 1,776. In VIA-DC3: `Tessellate`
  4,796, `Spotlight::SyncProperty` 4,728, `DrawToTexture` 3,320, `RndScaleObject` 3,112, `LightPreset::Load` 3,024.
- **Executed is not gated.** The count says a row ran, not that a gate checked its result. W16-TF and W16-TJ added
  55 gates (31 + 24); TJ notes that several rows run only as callees of gated rows, with no gate of their own
  (`PrepareNoteTubes`, `VocalTrackDir::SetRange`, the GemManager ctor). No per-row "gated" column exists.

### 4.4 Class view (W16-OV's table, same rules)

| block | classes | rows | bytes | % | at 10-07b |
|---|---|---:|---:|---:|---:|
| register/scheduling only | P1 + P2 | 114 | 71,116 | 35.3% | 71,116 |
| reorder-shaped insert/delete | I1 + I2 | 35 | 24,204 | 12.0% | 24,204 |
| source divergence | I3 + I4 + M1 + M2 | 138 | 85,100 | 42.3% | 85,100 |
| relocation name | N1 + N2 | 137 | 12,456 | 6.2% | 12,904 |
| unpaired | U2 + U3 + U4 | 119 | 8,484 | 4.2% | 9,000 |

- **Only the name and unpaired blocks moved: −448 B (SY) and −516 B (TH), which is the in-scope Δ (−964) exactly.**
  The three code blocks are byte-identical to 10-07b.
- Of the 137 name rows, 122 are anonymous funclets at `mpn` 100, and 31 rows / 8,044 B have a cycle-assumed worst
  pair (14 of them W16-OX's fill_insert rows). 220 of the 543 rows are at `mpn` 100.
- **By directory:** `system/bandobj` 122 rows / 46,468 B, `band3/meta_band` 120 / 45,164, `band3/bandtrack`
  26 / 22,040, `band3/game` 55 / 17,860.
- The top 100 rows hold 67.3% of the gap, and the median row is 112 B. The largest rows are unchanged:
  `VocalTrack::UpdateScrolling` (8,948 B, 97.35), `CustomizePanel::Handle` (5,036, 99.92) and
  `SaveLoadManager::SetState` (4,096, 99.95).

## 5. The VIA-DC3 ring

Same rules as §4, with the ring's own lanes first (`dispo_via5.py`).

| disposition | rows | bytes | % | native-compiled |
|---|---:|---:|---:|---:|
| in W16-PU/QF's population, register/reorder class, no per-row read | 49 | 47,276 | 24.6% | 40,360 |
| **read against retail by W16-TE/TI/TK: EQUAL, codegen residue** | 44 | 44,848 | 23.3% | 44,044 |
| **DC3 body compared by W16-TG, left** | 64 | 30,872 | 16.1% | 30,492 |
| **read against retail by W16-TE/TI/TK: FIXED or PARTIAL, residue recorded** | 16 | 23,428 | 12.2% | 22,436 |
| read row by row by W16-SF, left SCHED | 19 | 8,032 | 4.2% | 8,032 |
| identification residue (U2) | 89 | 6,268 | 3.3% | 5,220 |
| read row by row by W16-SF, left PARTIAL | 9 | 6,232 | 3.2% | 6,232 |
| read row by row by W16-SV, left SCHED | 8 | 5,760 | 3.0% | 5,760 |
| EH funclet / anonymous fragment | 117 | 4,932 | 2.6% | 4,432 |
| name: pointer fill_insert fold (W16-OX) | 7 | 3,448 | 1.8% | 1,704 |
| read row by row by W16-SV, left PARTIAL | 4 | 3,096 | 1.6% | 3,096 |
| source divergence worked since 10-06 (`RndPropAnim::ForeachKeyframe`) | 1 | 2,256 | 1.2% | 2,256 |
| read row by row by W16-SV, left HOME | 2 | 1,544 | 0.8% | 1,544 |
| **settled by W16-TH, left with its stop reason** (`OggMap::OpenMogg`) | 1 | 1,004 | 0.5% | 0 |
| read row by row by W16-SV, left RECORDED | 3 | 728 | 0.4% | 728 |
| read row by row by W16-SF, left RECORDED | 3 | 576 | 0.3% | 576 |
| **read against retail by W16-TE: inline blocked by the PCH** (`SynthEmitter` ctor) | 1 | 496 | 0.3% | 0 |
| name: proven, installable (`Fader::DoFade`, W16-TI §4.13 "rest NAME") | 1 | 444 | 0.2% | 444 |
| read row by row by W16-SV, left NAME (`Rnd::TestPoint`) | 1 | 332 | 0.2% | 332 |
| name: proven, in W16-SW's set, still charged (its §6 #12 / #13, no channel) | 2 | 276 | 0.1% | 276 |
| register, named in a W16-S* doc (`OggMap::SetKey`, W16-SM) | 1 | 224 | 0.1% | 0 |
| **total** | **442** | **192,072** | 100% | **177,964** |

- **10-07b's source-class bucket (67 rows / 70,984 B) is gone.** Every row has a per-row verdict read against
  retail: 61 rows / 68,772 B are in buckets 50–52, 5 reached 100 (TI's `StreamReceiver::Poll`, `PollStream`,
  `SpotlightDrawer::DrawShowing`; TK's `EstimateDraw`, `MetaMusicLoader`), and `Fader::DoFade` now reads as a
  name-only row (TI fixed its shape; the residue is a fold-group membership, chase PROVEN, left to a name lane).
  - Bucket 51's largest rows: `Tessellate` 4,796 (74.68, rest HOME), `DrawToTexture` 3,320 (97.51, rest SCHED),
    `WrapText` 3,016 (84.29, rest SCHED), `CamShotFrame::Interp` 1,772, `SmoothResults` 1,672 (74.79, rest HOME).
- **10-07b's 70 unnamed register/reorder rows (34,436 B) are all dispositioned by W16-TG.** 6 reached 100; the 64 left
  split, by TG's own verdict: DC3 body worse 17 / 9,988, same as DC3 15 / 4,176, raised but not 100 8 / 4,580, not
  qualified-name-matchable and hand-compared 8 / 3,720, DC3-only API 3 / 1,872, OURS_ONLY 4 / 1,272, different
  revision 3 / 1,708, DC3 flat 2 / 1,544, other register-only 4 / 2,012. The largest left is `RndScaleObject`
  (3,112 B, 89.58; DC3's body scored 87.58).
- **W16-SF/SV's rows are unchanged:** 49 rows / 26,300 B in the gap, the same buckets to the byte as at 10-07b.
- **The one big bucket without a per-row read is now register/reorder class: 49 rows / 47,276 B** (40,360
  native-compiled). All 49 are named in a lane doc since 10-06 (W16-QA 26, RF 10, RD 8, QQ 5, PU 4, …) or
  QA-compared (4 / 548), but none was read row by row against retail. The largest: `LightPreset::Load` 3,024 (I1,
  99.71), `CharIKHand::IKElbow` 2,880 (P2, 97.92), `CharHair::SimulateInternal` 2,472, `RndParticleSys::Load` 2,436,
  `CharLookAt::Poll` 2,268, `NgSpotlightDrawer::SetupXSection` 2,104 (I2, 76.45).
- **By class** the ring is source divergence 154 rows / 90,184 B, register 105 / 66,664, reorder 23 / 22,276, name
  71 / 6,680, unpaired 89 / 6,268. **By directory:** rndobj 194 / 94,420, char 98 / 43,488, world 41 / 31,676, synth
  49 / 12,504.
- **Not native-compiled: 14,108 B in 24 files** (10-07b: 16,408 in 27; W16-TJ's link closure took `StoreOffer`,
  `SlipTrack` and the two `FxSend*` files in). The largest: `StandardStream.cpp` 2,160, `VorbisReader.cpp` 2,048,
  `MetaMusic.cpp` 1,572, `Movie.cpp` 1,568, `OggMap.cpp` 1,228, `StoreEnumeration.cpp` 856, `CompressionEffect.cpp`
  632, `Emitter.cpp` 536, `StorePanel.cpp` 528.

## 6. Next levers, ranked by native relevance, then size

Tiers as at 10-07b:
- **A:** the charge can encode behaviour, in a file native compiles today, and no per-row read exists.
- **B:** a behaviour-class charge in a file native does not compile, or behaviour native compiles but does not yet
  run.
- **C:** codegen-only or naming. A fix moves the metric but cannot change native behaviour.

**The class screen for tier A leaks, and this round measured it.** 10-07b put lever 5 (70 register/reorder rows) in
tier C. W16-TG found two behaviour defects there. `Bloom_Blur` was a pure-register P1 row at 99.90, and its defect
was a parameter-role swap, which shows up only as two swapped `mr`. So a register-class row without a per-row
read can still hide behaviour. Lever 1 is every population like that, in both rings.

1. **[A] Register and reorder rows whose record is a doc mention or a lever, not a per-row retail read.**
   - **In scope: 106 rows / 64,192 B** (63,756 B native-compiled, **36 / 17,468 B executed**), all P1/P2. These are §4
     buckets 23 (named in a later lane's doc, 52 / 38,200), 11 (W16-PF's lever, 38 / 17,832), 22 (W16-PS's lever,
     11 / 5,464) and 25 (named in a W16-S* doc, 5 / 2,696). The executed ones, largest first: `ParseNode` 2,432,
     `VocalNoteList::NotesDone` 1,836, `EQEffect::SetParameter` 1,644, `VocalTrack::PrepareNoteTubes` 1,160,
     `VocalPart::HandlePhraseEnd` 1,120, `VocalPart::SetDifficultyVariables` 768.
   - **VIA-DC3: 49 rows / 47,276 B** (40,360 B native-compiled, **4 / 4,740 B executed**: `RndParticleSys::Load`
     2,436, `NgEnviron::Select` 1,756, `UILabel::LabelUpdate` 464, `Normalize` 84). This is §5 bucket 30. Each row is
     named in a lane doc since 10-06 (W16-QA 26, RF 10, RD 8, QQ 5, …), as a mention, not a verdict. 8 are
     reorder-shaped (I1/I2).
   - Method: TE/TI/TK's per-row read. Check calls, then stores and loads on one side only, then constants. Use TK's
     value-flow evaluator to clear straight-line rows; it is not committed (lever 8). Take executed rows first, in
     scope first, then by size.
   - Price: TG found 2 behaviour defects in 70 rows of this kind. TE/TI/TK found 6 behaviour fixes in 67 source-class
     rows. The four lanes moved 5,492 B in all. Expect defects, not bytes. These are also the permuter's rows: a read
     that finds nothing leaves them exactly where they are.
2. **[B] Native execution of what is compiled: in scope 166 rows / 111,036 B joined but never entered, VIA-DC3
   173 / 145,432** (§4.3).
   - The in-scope behaviour-class rows are 101 / 82,888 B, and 69 / 49,512 B of them were not entered by any target.
   - Drive the largest unentered rows on shipped data and gate them, as W16-TJ did for its three:
     `SaveLoadManager::SetState`, `BandDirector::OnFileLoaded`, `UIStats::MaybePublish`, the StoreOffer providers.
   - TJ's own leftovers: the CustomizePanel arms that need a ClosetMgr, user or profile; `UpdateScrolling`'s
     range-shift lerp and lyric plates, which run ungated; HOPO force markers, checked on one song only.
   - VIA-DC3's executed rows are ranked but not gated (W16-TF §6).
   - Measured yield so far: 55 gates, **no behaviour difference found in a gated row**, and **2 native-only bugs found by
     running** (`MakeBSPTree`, `UpdateScrolling`'s abort). This lever finds native bugs. It does not move X360 bytes.
3. **[B] VIA-DC3 files native does not compile: 14,108 B in 24 files** (§5). `StandardStream.cpp` 2,160,
   `VorbisReader.cpp` 2,048, `MetaMusic.cpp` 1,572, `Movie.cpp` 1,568 and `OggMap.cpp` 1,228 are the largest. Several
   are codec or platform backends that native may replace by rule, as it does `_Win.cpp`. In scope this lever is
   drained (5 files / 2,032 B, all excluded with reasons).
4. **[B] `SendDataPoint<const char*,int>`'s caller: 100 B in the ceiling.** Its only retail caller is the unpinned
   `auto_03_825632BC_text` (35 rows / 4,392 B, outside the ceiling). That is TU5 RBN-audition code (`"rbn/audition/fail"`)
   with no source in DC3 or this tree (W16-TH). Closing the row needs that TU identified and written from
   retail bytes. Native value is low: it is telemetry.
5. **[C] Identification residue: 207 rows / 14,652 B** (in scope 118 / 8,384, VIA-DC3 89 / 6,268). This round's
   re-homes drained 4 rows / 272 B in passing. It is bookkeeping unless a new channel appears.
6. **[C] Recorded single-site leftovers from this round's lanes: 4,396 B**, each with its owner named in the lane doc:
   - `Fader::DoFade` 444: re-admit `list<FaderTask*>::insert` to fold group 1457 (chase PROVEN), a name lane (W16-TI
     §7). It is one of the two VIA-DC3 rows a live native stub also defines (§4.3).
   - `Plane(point, normal)`'s FP fold: `Spotlight::DrawShowing` 904 and `SpotlightDrawer::DrawShadow` 328. It is a
     header-wide change (W16-TI §7).
   - `~FaderGroup`'s shared `ObjPtrList::erase` home, 160 (W16-TI §7).
   - The `SynthEmitter` ctor, 496: inline blocked by the synth/ PCH (W16-TE §3.9). A PCH opt-out needs its own
     three-gate A/B, per the PCH rule in CLAUDE.md.
   - ANCHOR: `Character::PostLoad` 1,440 and `RndMatAnim::Load` 624. MSVC bases co-addressed statics on the other
     object; no spelling found (W16-TK §7).
7. **[C] Name leftovers: 896 B.** `MeshCacher::Sync` 620 has no Wii channel 2. W16-SW's #12/#13 (276 B) have no
   channel. W16-OX's fill_insert fold (10,208 B in both rings) stays settled not admissible. W16-TH's
   `Find<RndCam>` and `SetTrackPanel` homes are at 100 but are approximations: they pair byte-identically, but the home
   is not provably the obj retail's linker kept. That is a record, not a lever.
8. **[C] Tooling.**
   - W16-TK's value-flow evaluator (`~/tmp/w16tk_diffs/symeval.py`, `valdiff.py`) is uncommitted, and lever 1 would
     use it. It is linear and contraction-blind, so it points rows out for reading and is not a verdict.
   - `native_stub_census.py` cannot see a stub written as an `#ifdef HX_NATIVE` arm inside a real TU (W16-TF §1).
   - `native_runtime_rank.py` has no gated/ungated column, so lever 2 has to reconstruct which executed rows a gate
     covers. This is my suggestion, not a lane's finding.
   - A walk step that moves backward in history needs the stale `build/45410914/config.json` removed first (§3). This
     matters only for future editions' walks.

**Not ranked, for the user to decide (unchanged):** the permuter pilot.
- In scope, buckets 20, 23, 11, 22 and 25 hold 160,220 B of codegen residue, and 220 rows are at `mpn` 100.
- Recorded single-instruction prizes are unchanged: `App::App` (1,864 B), `SaveLoadManager::SetState` (4,096 B) and
  `ParseNode` (2,432 B).
- In VIA-DC3, the rows read row by row and left as codegen (buckets 50, 53 and the SF/SV SCHED rows) add 89,512 B,
  plus bucket 30's 47,276 B. Lever 1 would give the in-scope register buckets and bucket 30 a behaviour verdict
  first. They are in the 160,220 B and 47,276 B already.

**Out of ranking by directive:** Quazal 113,520 B, XDK 2,948 B, 360-only 52,200 B, NET 3,868 B.

### 6.1 10-07b's levers, now

| 10-07b lever | taken by | result | state |
|---|---|---|---|
| 1. VIA-DC3 source-class rows, 67 / 70,984 B | W16-TE (10), W16-TI (20), W16-TK (37) | TE Δ0, 5 rows raised, 2 behaviour fixes + 1 latent defect; TI +8 / +1,336, 6 FIXED / 14 EQUAL, 1 behaviour fix; TK +3 / +592, 11 FIXED / 26 EQUAL, 2 behaviour fixes (3 defects). Every row has a per-row verdict | **drained** |
| 2. the last small behaviour-class rows, 3,940 B | W16-TH | 20 of 21 unpaired rows to 100 by re-home, respelling or a missing body; 2 callee bugs exposed and fixed; `SuperFormatString` 94.33 → 97.96 and `OpenMogg` 97.51 stopped as codegen; `SendDataPoint` stopped (no TU). +22 / +1,416 | **drained** |
| 3. native execution | W16-TF, W16-TJ | TF: the ranker never counted rb3-render (empty profile), fixed; 31 gates; `MakeBSPTree` native bug. TJ: the three largest unentered rows run on shipped data under 24 gates; `UpdateScrolling` native abort fixed. No gate found a behaviour difference | **half drained**, §6 lever 2 |
| 4. VIA-DC3 files native does not compile, 16,408 B / 27 | none by name (W16-TJ's link closure took 3 files in passing) | 14,108 B / 24 files | open, §6 lever 3 |
| 5. VIA-DC3 register/reorder rows no lane had named, 70 / 34,436 B | W16-TG | 14 raised, 6 to 100, 0 lowered; 2 behaviour fixes; +9 / +3,564 | **drained** |
| 6. identification residue, 211 / 14,924 B | W16-TB, W16-TH in passing | 207 / 14,652 B (in scope −2 rows / −92 B, VIA-DC3 −2 / −180) | open, §6 lever 5 |
| 7. name and pin leftovers, about 1,350 B | W16-TB (both pin leads), W16-SY (the dtk-addend row) | TB +2 / +84; SY +1 / +448. `MeshCacher::Sync` and SW #12/#13 unchanged | **drained** but §6 lever 7's leftovers |
| 8. tooling | W16-SY, W16-TF, W16-TD, cva-ram | dtk 1.15.0 carves interior `@l` labels; stub census lists both stub files and refuses an unlisted one; `ab_measure` saves the re-derived `.pdata` lines; `const_value_audit` bounded memory | **drained** but §6 lever 8 |

## 7. Reading the numbers

- **In-scope match share is 94.76%** (10-07b: 94.74%). The whole-binary share of the ceiling is **91.46%**.
- **In scope, 100% of the gap now carries a stop reason or a per-row record.** The last open rows were 10-07b's 524 B
  of unpaired names, and W16-TH closed them. The code-class blocks did not move at all this round.
- **VIA-DC3's behaviour-class rows are read.** Of its 98 named I3/I4/M1/M2 rows (87,764 B), 96 have a per-row read
  against retail (W16-SF/SV/TE/TI/TK). The other two are one row W16-TG compared with DC3's body and
  `RndPropAnim::ForeachKeyframe` (2,256 B, worked since 10-06, with a stop reason). TE/TI/TK read the 67 source-class
  rows against retail, and TG compared the 70 unnamed register rows with DC3's body. Together the four lanes found 8
  behaviour defects visible to native, and moved 5,492 B.
- **The behaviour work left is in two places.** The register/reorder rows no one has read row by row against retail
  (in scope 106 / 64,192 B, VIA-DC3 49 / 47,276 B; TG showed such rows can hide behaviour), and execution. Native
  enters 33.9% of the in-scope gap bytes and 11.6% of VIA-DC3's, and a row that is compiled but never run has not been
  observed.
- **The ceiling did not move.** No lane pinned `auto_*` code, so the denominator held to the byte.

## 8. Not done

- **No source, map, splits, alias or `symbols.txt` edits.** Nothing was A/B-measured, because this branch changes no
  code. The native gate was not run (no `src/` change). The runtime rank built `native/build-prof` in this worktree
  only.
- **The overlays are coarse:**
  - The lane-verdict join keys on (short name, size) from each lane doc's table. It is asserted on counts and audited
    on names, but a row renamed after its lane read it falls through to the older buckets.
  - "Named in a lane doc" is still a doc mention, not a per-row verdict (bucket 30, the W16-S* mention buckets).
  - The native "executed" column is a function-entry count joined by signature. It inherits
    `native_runtime_rank.py`'s join tiers; anonymous `fn_` rows cannot join at all.
- I did not open `~/tmp/wt-w16tm`, a worktree with a 27-file list that may be working 10-07b's lever 4. Nothing from
  it has landed, and §5 / §6 measure main only.
- The per-lane walk does not cover the native pins, `111e836f9`, or the TA/TD/cva-ram merges. None touches a
  match-build input.

Scripts and JSON are in `~/tmp/w16tn-gap/` and are not committed. Reproduce in this order, from that directory, with
`W=~/tmp/wt-w16tn` built at `b79d790b8`:

```
python3 gengap.py $W gap.json                      # 1,685 gap rows, asserts ceiling - matched
python3 scope_ledger2.py $W ledger2.json           # ring ledger, asserts total_code and matched_code
python3 diffall.py $W gap.json cls.json            # one objdiff diff per gap row, W16-NA classifier (8 threads)
python3 unpaired.py $W                             # U2/U3/U4 split; 200/200 index control
python3 lanever.py                                 # this round's per-row lane verdicts -> lanever.json (asserts counts)
python3 overlay5.py                                # overlay4 + TE/TI/TK/TG/TH pools, W16T* mentions, 10-07b dispositions
python3 insdel_shape.py $W                         # opset shapes
python3 tables3.py > tables3.txt                   # klass(), rings, final.json
python3 mkcensus.py && python3 pairs.py            # rows with a symbol-kind charge, their name pairs
python3 chase_all.py                               # icf_pair_adjudicate chase per pair
python3 namecls.py && python3 final3.py            # name verdicts; placeholder-only control (none == graded)
python3 tables4.py > tables4.txt                   # in-scope partition, class blocks, dirs
(cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)   # in $W, configure only
(cd $W && python3 tools/native_linked_tus.py . --out ~/tmp/w16tn-gap/native_linked.txt --jobs 8)
                                                   # native_src.json = that list ∪ $W/native/build compile edges (642 files)
python3 dispo5.py > dispo5.txt                     # §4 table (asserts)
python3 dispo_via5.py > dispo_via5.txt             # §5 table (asserts)
python3 natrel.py; python3 viabreak.py             # §4.3 / §5 native and bucket breakdowns
(cd ~/code/milohax/rb3-xenon && python3 tools/native_stub_census.py native/build ~/tmp/w16tn-gap/stubs.json --authored)
python3 stubmatch.py                               # §4.3 stub match
(cd $W && CMAKE_BUILD_PARALLEL_LEVEL=6 python3 tools/native_runtime_rank.py . --out ~/tmp/w16tn-gap/rank.tsv --json ~/tmp/w16tn-gap/rank.json)
python3 execjoin.py                                # §4.3 executed column, per bucket
python3 cmp1007b.py > movement.log                 # §2 rings vs 10-07b, in-scope and VIA-DC3 row movement (asserts)
python3 leftwhere.py; python3 rowsdown2.py         # where left rows went; whole-binary rose/fell
python3 claims2.py                                 # §1 attribution
(cd walk2 && bash walk.sh && bash walk_step0.sh); python3 lane_rings3.py   # §3
```

- **W16-TA's pipeline, copied and repointed:** `gengap`, `scope_ledger2`, `diffall`, `unpaired`, `insdel_shape`,
  `tables3`, `mkcensus`, `pairs`, `chase_all`, `namecls`, `final3`, `tables4`, `natrel`, `viabreak`, `stubmatch`,
  `leftwhere`.
- **New in this edition:** `lanever.py`, `overlay5.py`, `dispo5.py`, `dispo_via5.py` (§4, §5); `cmp1007b.py`,
  `rowsdown2.py` (movement); `execjoin.py` (§4.3); `claims2.py`; `walk2/walk.sh`, `walk2/walk_step0.sh`,
  `lane_rings3.py` (§3).
- Per-row tables: `dispo_rows.json` (in scope) and `dispo_via_rows.json` (VIA-DC3).
