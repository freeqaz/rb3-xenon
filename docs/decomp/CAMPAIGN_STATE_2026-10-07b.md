# Campaign state — 2026-10-07b (after W16-SB … W16-SW)

Tenth edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-07.md` (W16-SD) for numbers and
priorities. It reruns W16-SD's method, with four changes, each stated where it is used:

- **New buckets.** This round's lanes get their own buckets ahead of the older records (§4.1).
- **Native column.** The native-compiled column now uses the compiler's answer (`tools/native_linked_tus.py`) instead
  of `build.ninja` compile edges. W16-SH showed the old rule undercounted (§4.3).
- **Movement base.** Row movement is measured against W16-SD's run, not 10-06's.
- **Per-lane split.** It comes from a rebuild walk of this round's 13 match-build steps (§3).

16 merges and 3 native pins landed between the two measurements. Scope rules are W16-OV's: the native port is the
goal, XDK is hard-skipped, Quazal stays as landed. No source, map, splits, alias or `symbols.txt` edit on this branch.

## 1. Measured

I used a fresh `scripts/setup_worktree.sh` worktree at main **`2fd722b8f`** (W16-SW) and ran a full
`./tools/ninja-locked`. The build ended `BUILD_RC=0` with `[pairing] 1056/1056 declared compiled objects pair with a
target` and `[patch-state] OK: tree is a fixed point of 6 post-compile passes`. I then wiped `report.json` and
`report.cache` and regenerated them. Ruler: `name_check` (objdiff 4.2.9, tool commit `a5f0ea903ec1`, binary hash
`5a51cd51fe0a353f`, read from `report.json`'s `provenance`). Main has since moved to `3d07cb6f2`, a native pin
(W16-SZ) that changes no match-build input.

**Prediction, written before reading `report.json`:** the merges' own A/B claims sum to **+139 fns / +22,896 B**:

| lane | claim |
|---|---|
| SG | +53 / +6,568 |
| SF | +14 / +3,240 |
| SN | +14 / +1,340 |
| ST | +22 / +4,368 |
| SO | +5 / +692 |
| SV | +2 / +652 |
| SW | +29 / +6,036 |
| SB, SC, SH, SJ, SK, SL, SM, SP | Δ0 each |

So I expected `matched_code` 6,070,792 and `matched_functions` 54,894. **Both held exactly.**

```
python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 2fd722b8f (2026-10-07, W16-SW)"
  scaffold thr<=4 : 0 · thr<=5 : 0 · thr<=6 : 56 units, 289 rows, 57,448 B · thr<=7 : 56 (cliff, not fitted)
total_code            10,247,844
PAIRABLE               6,703,512 = 65.414%   (1,056 units, 56,318 rows)
− scaffold shells         57,448             (56 units, 289 rows)
= reachable ceiling    6,646,064 = 64.853%
matched_code           6,070,792 = 59.240% of total_code = 91.34% of ceiling
gap to ceiling           575,272
matched_functions         54,894 / 68,884   masked_equal 25,210   honest 29,684
fuzzy_match_percent       64.28836
```

Against 10-07 (`24d250690`): **+22,896 B / +139 fns**. The share of the ceiling went 91.00% → **91.34%** and the gap
598,168 → **575,272 B (−22,896)**. The ceiling, `total_code` and `total_functions` did not move. No lane this round
pinned code out of `auto_*`, so every byte that crossed was already reachable.

| control | result |
|---|---|
| attribution | the 16 merges' claims sum to +139 / +22,896, residual **0 / 0** against the measured Δ (`claims.py`) |
| lane walk, step 0 | rebuilding `24d250690` from scratch here equals W16-SD's 10-07 ledger on every ring and its archived report (`lane_rings2.py`) |
| lane walk, last step | see §3 |
| independent baseline | main's own `build/45410914/report.json` (07:24, at `2fd722b8f`) equals this build on all seven headline keys |
| population sums | gap rows (1,725) sum to 575,272 = ceiling − matched (`gengap.py`) |
| ring sums | tier totals sum to `total_code` and `matched_code` (`scope_ledger2.py` asserts both) |
| disposition tables | §4's buckets sum to 553 rows / 202,324 B = in-scope ceiling − matched (`dispo4.py` asserts); §5's to 472 rows / 198,548 B = the VIA-DC3 gap (`dispo_via4.py` asserts) |
| row movement | in scope: left − entered − size changes = −6,840 B = Δ in-scope gap (`cmp1007.py` asserts); VIA-DC3: −15,556 B = its Δ gap |
| placeholder correction | 22/22 placeholder-only N2 rows read identical fuzzy at `none` and graded |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail" |
| unpaired index | 200/200 paired named rows found in their own base obj |
| native set | `native_linked_tus.py` self-validation passed; its not-compiled in-scope remainder is **5 files / 2,032 B**, W16-SH §4's list to the byte |
| stub match (§4.3) | the qualified-name matcher finds 194 in-scope rows at 100 that a live stub also defines, so its 0 for gap rows is not vacuous |

## 2. Rings

From `cmp1007.py` (this build's `ledger2.json` against W16-SD's).

| tier | reachable | matched | **gap** | share | gap at 10-07 | Δ matched | Δ fns | Δ reach |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 478,264 | 434,376 | **43,888** | 90.82% | 44,272 | +476 | +6 | +92 |
| IN-SOON | 2,440,472 | 2,340,136 | **100,336** | 95.89% | 105,660 | +7,160 | +36 | +1,836 |
| IN-RB3ENG | 927,064 | 868,964 | **58,100** | 93.73% | 59,232 | +2,160 | +23 | +1,028 |
| **in scope** | **3,845,800** | **3,643,476** | **202,324** | **94.74%** | 209,164 | **+9,796** | **+65** | +2,956 |
| VIA-DC3 (DC3 has the file) | 2,023,028 | 1,824,480 | 198,548 | 90.19% | 214,104 | +12,296 | +67 | −3,260 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 113,520 | 0 | 0 | 0 |
| OUT-NET | 76,936 | 73,068 | 3,868 | 94.97% | 4,368 | +804 | +7 | +304 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 | 0 |
| OUT-360-OTHER | 349,068 | 296,868 | 52,200 | 85.05% | 52,200 | 0 | 0 | 0 |
| UNKNOWN-other | 3,532 | 1,668 | 1,864 | 47.23% | 1,864 | 0 | 0 | 0 |

- **Native scope CORE+SOON:** 99.05% of functions and 95.06% of bytes (10-07: 98.95% / 94.86%).
- **VIA-DC3 took more than half the round:** +12,296 B against +9,796 B in scope. Its gap fell 15,556 B (−7.3%), the
  in-scope gap 6,840 B (−3.3%). The VIA-DC3 gap (198,548) is now 3,776 B below the in-scope gap (202,324) again.
- **Reach moved between rings, net 0**: VIA-DC3 −3,260 B, in scope +2,956, OUT-NET +304. These are re-homes:
  - W16-SG and W16-SN moved STL helper pins to the unit whose obj defines the retail instantiation, and those units
    sit in other rings.
  - `fn_82766F6C` (40 B, 99.5) went from Mesh to Msg.
  - Moving a pin between named units does not change the ceiling, and it did not.

**Gap movement, keyed on (unit, symbol):**

- **In scope, left: 48 rows / 6,920 B** (`leftwhere.py`):

  | where the row is now | rows | bytes |
  |---|---:|---:|
  | same unit, fuzzy 100 | 37 | 6,564 |
  | renamed (W16-SG/SN/ST/SW map corrections; the new name is not in the in-scope gap) | 10 | 316 |
  | re-homed to another in-scope unit, below 100 (`fn_8230EC54`) | 1 | 40 |

  By their 10-07 disposition:

  | 10-07 disposition | rows / bytes |
  |---|---:|
  | 03 PH-undecidable | 10 / 3,044 |
  | 07 proven names | 13 / 2,540 |
  | 16 no-record source rows | 3 / 452 |
  | 01 funclets | 7 / 296 |
  | 06 wrong callee | 8 / 288 |
  | 22 PS register | 1 / 220 |
  | 04 type witness | 5 / 40 |

  10-07's open in-scope name buckets 07, 06 and 04 emptied. Bucket 03 kept one row (`MeshCacher::Sync`, parked by
  W16-SW) and bucket 16 kept one (`SuperFormatString`, deferred by W16-SG).
- **In scope, entered: 2 rows / 80 B**, both re-homes at the same fuzzy (`fn_82766F6C` from VIA-DC3's Mesh, and the
  `fn_8230EC54`, Song → SongSectionController, both 99.5). No in-scope row still in the gap changed size or score.
- **VIA-DC3, left: 101 rows / 15,556 B, entered 0.**

  | where the row is now | rows | bytes |
  |---|---:|---:|
  | same unit at 100 | 56 | 11,636 |
  | renamed | 37 | 3,472 |
  | re-homed to an in-scope or OUT-NET unit at 100 | 7 | 408 |
  | re-homed below 100 | 1 | 40 |

  19 rows still in the gap rose and 1 fell.
- **Whole binary**, over the 68,823 rows present under the same (unit, name) in both trees: **114 rose, 1 fell**
  (`rowsdown.py`). The one that fell is `RndAmbientOcclusion::Tessellate` (4,796 B), 68.13 → 67.40, which W16-SV
  recorded and kept (§3.1 of its doc: retail supports the `unsigned short` type in that function too). No row fell
  from 100.

## 3. What landed, per lane

The lane worktrees are gone, and only W16-ST, SV and SW archived their `ab_measure` run dirs. So I rebuilt every
step instead, with W16-SD's `walk.sh` repointed to a private worktree (`~/tmp/wt-w16ta-walk`):
- **Steps:** step 0 (`24d250690`), then the 13 first-parent merges up to `2fd722b8f` that touch a match-build
  input (`src/`, `config/`, the map JSON, `configure.py`, `tools/project.py`).
- **Left out:** the three native pins, W16-SB's merge, W16-SD's doc merge and W16-SP's (tools and docs only). None
  touches one of those paths.
- **Per step:** the same procedure as W16-SD's: build until `symbols.txt` is unchanged by a build (every step took
  one), wipe `report.json` and `report.cache`, regenerate and snapshot. Every step is after `a3f3339a8`, so the
  clean TU5 image throughout.

Controls, all predicted to hold before the walk ran, and all hold (`lane_rings2.py`):
- **Step 0** (`24d250690`, rebuilt here) equals W16-SD's 10-07 ledger on every ring, and W16-SD's archived report
  (`rep_4a14afeb3.json.gz`) on `matched_code`, `matched_functions` and every ring.
- **The last step** (`2fd722b8f`) equals this worktree's build on `matched_code`, `matched_functions` and every ring.
- **The steps sum** to +139 fns / +22,896 B whole-binary, +9,796 B / +65 fns in scope and +12,296 B VIA-DC3, which is
  §2 exactly.
- **Every lane equals its own A/B claim**, fns and bytes. `total_code` and the row count move on no step.

Six steps read zero on every ring, as their lanes measured: W16-SC, SH, SJ, SK, SL and SM, the Δ0 accuracy and
native-link lanes. None of them moved a row between rings either. They are left out of the table.

| lane | merge | Δ fns | Δ matched_code | Δ in scope | CORE / SOON / RB3ENG | Δ VIA-DC3 | other rings |
|---|---|---:|---:|---:|---|---:|---|
| W16-SG | `41af27809` | +53 | +6,568 | **+3,680** | +200 / +2,084 / +1,396 | +2,888 | — |
| W16-SF | `89bd48e30` | +14 | +3,240 | **+0** | +0 / +0 / +0 | +3,240 | — |
| W16-SN | `82ff27dca` | +14 | +1,340 | **+1,052** | +92 / +880 / +80 | +176 | NET +112 |
| W16-ST | `37157fb31` | +22 | +4,368 | **+1,892** | +184 / +1,552 / +156 | +2,476 | — |
| W16-SO | `41f09053a` | +5 | +692 | **+0** | +0 / +0 / +0 | +0 | NET +692 |
| W16-SV | `379611b73` | +2 | +652 | **+0** | +0 / +0 / +0 | +652 | — |
| W16-SW | `2fd722b8f` | +29 | +6,036 | **+3,172** | +0 / +2,644 / +528 | +2,864 | — |
| **sum of steps** | | **+139** | **+22,896** | **+9,796** | +476 / +7,160 / +2,160 | **+12,296** | NET +804 |

- **Name settlement was most of the round:** W16-SG, ST and SW together are 16,972 of 22,896 B, and all of the
  in-scope bytes except W16-SN's 1,052 B. W16-SN's are renames too.
- **W16-SF and W16-SV are VIA-DC3 only, as briefed.** They took 3,892 B. SF recorded eight behaviour fixes, four of
  them on rows that reached 100, and SV two native-only ones.
- **W16-SO's bytes are all OUT-NET:** its four SessionMessages MemStream helpers sit in `src/network/net`. It followed
  a lead W16-SN found on two rows that SF had filed as VIA-DC3 FOLD rows.

## 4. Where the in-scope gap stands: one disposition per row

Each in-scope gap row (553 / 202,324 B) goes to the first bucket that fits (`dispo4.py`). **"Native-compiled"**
means the row's source file is a TU of at least one target of main's native build, as the compiler reports it
(§4.3).

| disposition | rows | bytes | % | CORE | SOON | RB3ENG | native-compiled |
|---|---:|---:|---:|---:|---:|---:|---:|
| read for behaviour by W16-PQ/PR: no defect left, codegen residue | 107 | 96,028 | 47.5% | 21,680 | 45,924 | 28,424 | 94,888 |
| register/schedule: named in a later lane's doc, stopped | 52 | 38,200 | 18.9% | 5,968 | 19,964 | 12,268 | 38,200 |
| register/schedule: W16-PF's lever applied, stopped, no later record | 38 | 17,832 | 8.8% | 0 | 17,832 | 0 | 17,396 |
| source divergence: worked since 10-06, left with a stop reason | 27 | 11,340 | 5.6% | 2,752 | 2,868 | 5,720 | 11,340 |
| identification residue (U2) | 120 | 8,476 | 4.2% | 2,356 | 4,064 | 2,056 | 8,324 |
| name: pointer `_M_fill_insert` fold, settled not admissible (W16-OX) | 14 | 6,760 | 3.3% | 2,388 | 3,572 | 800 | 6,760 |
| EH funclet / anonymous fragment of a parent row | 158 | 6,488 | 3.2% | 1,008 | 3,604 | 1,876 | 6,436 |
| register: W16-PS's lever 4/5 applied, stopped | 11 | 5,464 | 2.7% | 2,736 | 1,248 | 1,480 | 5,464 |
| never-worked row taken by W16-PO, left with a stop reason | 9 | 5,188 | 2.6% | 2,356 | 0 | 2,832 | 4,936 |
| register/schedule: named in a W16-S* doc, stopped | 5 | 2,696 | 1.3% | 264 | 708 | 1,724 | 2,696 |
| settled by W16-SG on retail bytes, left with its stop reason (`SuperFormatString` ctor) | 1 | 1,608 | 0.8% | 1,608 | 0 | 0 | 1,608 |
| source divergence: named in a W16-S* doc, left (`PreInitSystem`, W16-SM) | 1 | 652 | 0.3% | 652 | 0 | 0 | 652 |
| name: parked by W16-SW, no second channel (`MeshCacher::Sync`) | 1 | 620 | 0.3% | 0 | 0 | 620 | 620 |
| unpaired named (U3/U4) | 8 | 524 | 0.3% | 120 | 104 | 300 | 524 |
| name: dtk drops the reloc addend on the target obj, our constant is retail's | 1 | 448 | 0.2% | 0 | 448 | 0 | 448 |
| **total** | **553** | **202,324** | 100% | 43,888 | 100,336 | 58,100 | **200,292** |

### 4.1 How the buckets are decided

`dispo4.py` is W16-SD's `dispo3.py` with this round's records inserted where they are the most specific. The
classes, chase verdicts and older pools are unchanged and recomputed on this tree (`diffall` → `final3` → `tables4`,
W16-SD's chain, repointed). In order:

1. **Class-keyed buckets, as before:** TU5 image, funclets, name classes by chase verdict, U2, U3/U4. Two new name
   buckets come first among the name rows:
   - `03w`: a cycle-assumed or undecidable row in W16-SW's set, which its §6 parked with a reason.
   - `07s`: a proven row in W16-SG/ST/SW's sets that is still charged.
   In scope the second is empty.
2. **W16-SF / W16-SV per-row reads (new).** Their disposition tables give every one of SF's 82 rows a class
   (FIXED / PARTIAL / SCHED / HOME / RECORDED / FOLD / NAME). SV re-read the ones still open: its text says 19, its
   table has 20 rows, and the parser takes the table. `sfcls.py` parses both
   tables, SV's reading winning, and keys rows by the name SF read, W16-SN's renames of the FOLD rows, and SV's current
   names. In scope this bucket is empty: all 82 rows are VIA-DC3.
3. **W16-SG's 37-row population (new).** Its file is `~/tmp/w16sg/rows37.json`. One row is left in the gap: the
   `SuperFormatString` ctor, which SG deferred as structure.
4. W16-SD's own order: W16-PQ/PR pools, W16-PO, W16-PS register rows.
5. **A W16-S* doc names the row (new).** W16-SD's backtick overlay, extended to the `W16S*_2026-10-07.md` docs. It sits
   ahead of the W16-P/Q/R mention buckets because it is the newer record, and it is as coarse as they are.
6. Then W16-SD's later-mention buckets, W16-PF / W16-PC pools and older mentions, unchanged.

### 4.2 How it adds up

- **Codegen residue with a behaviour or lever record: 160,220 B, 79.2%** (buckets 20, 23, 11, 22 and 25). In
  scope this round crossed only name-class and no-record rows, so this mass is W16-SD's (160,440 B) less 220 B.
  - Half of it is rows W16-PQ/PR read line by line against retail and found to do what retail does.
  - The rest are register or scheduling rows that a lever or a named row lane stopped on.
- **Source-class rows with a stop reason: 18,788 B, 9.3%.** These are W16-PO's 9 rows, 27 rows worked since 10-06,
  `SuperFormatString` (W16-SG: "structure, deferred") and `PreInitSystem` (named by W16-SM).
- **Settled, non-separable or parked: 22,792 B, 11.3%.** This covers:
  - funclets;
  - W16-OX's fill_insert fold;
  - the dtk-addend row;
  - W16-SW's `MeshCacher::Sync`. The chase is PROVEN and channel 1 holds, but the Wii link has no `MeshCacher::Sync`,
    so there is no channel 2;
  - the U2 residue (120 rows / 8,476 B, unchanged since 10-07).
- **Open: 524 B, 0.3%.** Only the 8 unpaired named rows are left: `RndCam>` 164, `BandStarDisplay>` 116,
  `SetObjConcrete` 104, an `int>` row 100, `SetFileChecksumData` 16, the `UIPicture::Load` vtordisp thunk 12,
  `TrackPanelDirBase::SetTrackPanel` 8 and one 4 B row.
- **At 10-07 the open in-scope surface was 5,412 B.** The 13 proven names, 8 wrong-callee rows and 3 template rows
  have all left the gap, at 100 or under a corrected name. Every in-scope row except those 8 unpaired rows carries a per-row or per-lever stop reason.

### 4.3 Native relevance

**The instrument changed, and the change is larger than this round's movement.**
- **Old rule.** W16-SD's column was the compile edges of `native/build/build.ninja`.
- **What it missed.** W16-SH §1 showed it misses scatter guests: a `.cpp` file `#include`d into another TU, like
  `GemManager.cpp` into `CharBonesMeshes.cpp`. By its count 8 of SD's 96 "not compiled" files (28,376 B) were
  already compiled.
- **New rule.** This edition uses `tools/native_linked_tus.py`, which asks the preprocessor (`clang++ -M`) over every
  target's TUs. I took the union with the compile edges, because the tool lists only C++ TUs and the json-c `.c`
  files have edges. The result is 620 files.
- **Self-check.** The tool's own validation passed (all three known-linked TUs present).
- **Known artifact.** The union includes main's 0-byte, gitignored stray `src/system/os/MasterAudio.cpp` (W16-SH
  §5.1). It owns no report unit, so it touches no gap row.

| measure | in scope | VIA-DC3 |
|---|---:|---:|
| gap | 202,324 | 198,548 |
| native-compiled, compile-edge rule (W16-SD's) | 172,368 (85.2%) | 112,740 (56.8%) |
| **native-compiled, compiler's answer** | **200,292 (99.0%)** | **182,140 (91.7%)** |
| at 10-07, W16-SD's rule | 111,824 (53.5%) | 121,908 (56.9%) |
| not compiled | 2,032 in 5 files | 16,408 in 27 files |

- **In scope, native now compiles everything but 5 files / 2,032 B:** `net_band/RockCentral.cpp` 664,
  `os/AsyncFile_Win.cpp` 460, `net_band/XboxEntityUploader.cpp` 436, `os/ThreadCall_Win.cpp` 252 and
  `net_band/ContextWrapper.cpp` 220. That is W16-SH §4's not-linked list, with its reasons: a Quazal client, an XDK
  service and two Win32 backends native replaces by rule. On the compile-edge rule, W16-SH's link work moved the
  share from 53.5% to 85.2%: 99 files are new on that rule, W16-SH's 83 plus 16 of its 17 dependencies. Scatter
  guests make up the rest of the 99.0%.
- **The native-compiled in-scope files with the most gap:** VocalTrack.cpp 11,532, BandPatchMesh.cpp 7,584, Geo.cpp
  6,308, GemManager.cpp 5,492, CustomizePanel.cpp 5,428, BandDirector.cpp 4,620, SaveLoadManager.cpp 4,096,
  BandCamShot.cpp 3,792, Dir.cpp 3,624, NoteTube.cpp 3,576.
- **Compiled is not executed.** W16-SH: "About 7,564 B of in-scope gap rows now execute under a gate. The rest of the
  83 files are linked but not driven." A scatter guest can also sit behind a conditional edge in some hosts. Neither
  `tools/native_runtime_rank.py` (W16-PK) nor any coverage run has been repeated since W16-SH, so the column is
  "compiled into a native target", not "runs".
- **No in-scope gap row is replaced by a live native stub.** `tools/native_stub_census.py native/build stubs.json
  --authored`, run read-only on main's native build, finds 956 live stub symbols. ⚠ **Its `STUB_RE` does not list two
  stub TUs** that main's native build now links: `bandtrack_link_stubs.cpp` (26 definitions) and `w16sh_link_support.cpp` (10). I added them
  by `nm` (`stubmatch.py`), for 992 symbols. None matches an in-scope gap row by qualified name; 194 in-scope rows at
  100 do. In VIA-DC3 the same two gap rows as at 10-07 are stubbed in some target: `CacheResource` (304 B, 38.42)
  and `Fader::DoFade` (444 B, 93.09). A third hit, a 4 B `delete` in Rnd, is a bare operator name, so it is a name
  collision, not a stub.
- **What native can still observe in scope:** the 83,312 B of named source-class rows (I3/I4/M1/M2/U3/U4) in compiled
  files. 70,300 B of it is rows W16-PQ/PR read for behaviour and found EQUAL, so a defect left there would have to be
  one those reads missed. The rest is 12,488 B of stop-reason rows (the native-compiled part of §4.2's 18,788 B) and the 524 B of
  unpaired rows.

### 4.4 Class view (W16-OV's table, same rules)

| block | classes | rows | bytes | % | at 10-07 |
|---|---|---:|---:|---:|---:|
| register/scheduling only | P1 + P2 | 114 | 71,116 | 35.1% | 71,336 |
| reorder-shaped insert/delete | I1 + I2 | 35 | 24,204 | 12.0% | 24,276 |
| source divergence | I3 + I4 + M1 + M2 | 138 | 85,100 | 42.1% | 85,480 |
| relocation name | N1 + N2 | 138 | 12,904 | 6.4% | 19,072 |
| unpaired | U2 + U3 + U4 | 128 | 9,000 | 4.4% | 9,000 |

- **The name block fell 6,168 B, and the code blocks did not move.** This round's in-scope work was name settlement.
- Of the 138 name rows, 122 are anonymous funclets at `mpn` 100, and 31 rows / 8,044 B have a cycle-assumed worst pair.
  14 of those are W16-OX's fill_insert rows. 220 of the 553 rows are already at `mpn` 100.
- **By directory** the gap concentrates in `system/bandobj` (125 rows / 46,752 B) and `band3/meta_band` (122 /
  45,616), then `band3/bandtrack` (26 / 22,040) and `band3/game` (55 / 17,860).
- The top 100 rows hold 66.9% of the gap, and the median row is 112 B. The largest row is still
  `VocalTrack::UpdateScrolling` (8,948 B, 97.35), followed by `CustomizePanel::Handle` (5,036, 99.92) and
  `SaveLoadManager::SetState` (4,096, 99.95).

## 5. The VIA-DC3 ring

Same rules as §4, with the ring's own lanes first (`dispo_via4.py`). W16-SF and W16-SV's per-row reads come before
W16-PU/QF's populations, because they are per-row records and those are not.

| disposition | rows | bytes | % | native-compiled |
|---|---:|---:|---:|---:|
| in W16-PU/QF's population, register/reorder class | 119 | 81,712 | 41.2% | 73,468 |
| in W16-PU/QF's population, source-class charge | 67 | 70,984 | 35.8% | 67,520 |
| read row by row by W16-SF, left SCHED | 19 | 8,032 | 4.0% | 8,032 |
| identification residue (U2) | 91 | 6,448 | 3.2% | 5,392 |
| read row by row by W16-SF, left PARTIAL | 9 | 6,232 | 3.1% | 6,232 |
| read row by row by W16-SV, left SCHED | 8 | 5,760 | 2.9% | 5,760 |
| EH funclet / anonymous fragment | 121 | 5,092 | 2.6% | 4,592 |
| name: pointer fill_insert fold (W16-OX) | 7 | 3,448 | 1.7% | 1,704 |
| read row by row by W16-SV, left PARTIAL | 4 | 3,096 | 1.6% | 3,096 |
| source divergence worked since 10-06 (`RndPropAnim::ForeachKeyframe`) | 1 | 2,256 | 1.1% | 2,256 |
| read row by row by W16-SV, left HOME | 2 | 1,544 | 0.8% | 1,544 |
| source divergence, no record (`OggMap::OpenMogg`, W16-RG: stack-temp slot sharing) | 1 | 1,004 | 0.5% | 0 |
| unpaired named (U3/U4) | 13 | 804 | 0.4% | 632 |
| read row by row by W16-SV, left RECORDED | 3 | 728 | 0.4% | 728 |
| read row by row by W16-SF, left RECORDED | 3 | 576 | 0.3% | 576 |
| read row by row by W16-SV, left NAME (`Rnd::TestPoint`: fold installed by W16-SW, one `stb` scheduled) | 1 | 332 | 0.2% | 332 |
| name: proven, in W16-SW's set, still charged (its §6 #12 / #13, no channel) | 2 | 276 | 0.1% | 276 |
| register, named in a W16-S* doc (`OggMap::SetKey`, W16-SM) | 1 | 224 | 0.1% | 0 |
| **total** | **472** | **198,548** | 100% | **182,140** |

- **W16-SF/SV's 82 rows are all dispositioned.** 49 rows / 26,300 B are still in the gap:
  - **SCHED:** 27 rows / 13,792 B.
  - **PARTIAL:** 13 rows / 9,328 B. A fix was applied, and what remains is recorded.
  - **HOME:** 2 rows / 1,544 B. Retail stores into one frame slot that it never reads back: `BuildVisit` and
    `Transitions::Resize`.
  - **RECORDED:** 6 rows / 1,304 B.
  - **NAME:** 1 row / 332 B.

  The other 33 rows are out of the gap: SF's 13 and SV's 2 at 100, plus the 18 FOLD rows, which W16-SG/SN renamed.
  SN left two MemStream helpers at 91.67; W16-SO then moved the SessionMessages MemStream helpers to 100.
- **The two big buckets are W16-PU/QF's populations, and a doc mention is still not a per-row verdict.**
  - **Source-class, 67 rows / 70,984 B (67,520 native-compiled).** Every row is named in some lane doc since 10-06.
    29 rows / 33,236 B are named in W16-PU's doc, and 31 / 25,992 B were body-compared with current DC3 by W16-QA
    (20 DIFF, 7 SAME, 3 OURS_ONLY, 1 PARTIAL). Only 7 rows / 11,756 B carry no more than a passing mention.
    **Ten of the 67 have retail's body at least 16 B larger than ours (18,720 B), nine of them native-compiled:**

    | row | size | fuzzy | retail larger by |
    |---|---:|---:|---:|
    | `RndAmbientOcclusion::Tessellate` | 4,796 | 67.40 | +204 |
    | `RndMesh::Load` | 3,452 | 99.41 | +20 |
    | `RndText::WrapText` | 3,016 | 83.40 | +44 |
    | `Spotlight::BuildNGCone` | 1,692 | 74.90 | +16 |
    | `RndAmbientOcclusion::SmoothResults` | 1,672 | 68.02 | +160 |
    | `CharBonesSamples::Relativize` | 1,448 | 96.47 | +24 |
    | `RndLine::UpdateLine` | 1,328 | 70.66 | +40 |
    | `BoxMapLighting::ApplyQueuedLights` | 520 | 73.77 | +44 |
    | `SynthEmitter` ctor (not native-compiled) | 496 | 86.13 | +16 |
    | `NgDOFProc::Set` | 300 | 81.40 | +20 |

  - **Register/reorder, 119 rows / 81,712 B.** 45 rows / 46,728 B are named since 10-06 and 4 / 548 B were
    QA-compared. **70 rows / 34,436 B are named by no lane and were not body-compared by W16-QA**, which covered
    `src/system` outside rndobj and char. 49 are in rndobj and 20 in char; 34,056 B of the 70 rows are native-compiled.

    | row | size | class | fuzzy |
    |---|---:|---|---:|
    | `RndScaleObject` | 3,112 | I1 | 89.58 |
    | `CharBones::RotateBy` | 1,420 | P1 | 99.94 |
    | `RndTransAnim::MakeTransform` | 1,160 | I1 | 95.06 |
    | `CharIKFingers::CalculateFingerDest` | 1,100 | I1 | 89.15 |
    | `Rnd::DrawTimers` | 1,080 | P2 | 97.97 |
    | `CharHair::Hookup` | 1,012 | P1 | 99.88 |
    | `Hmx::operator*` | 848 | I1 | 77.14 |
    | `ComputeFaceTangentBasis` | 812 | I1 | 77.72 |

    W16-SD noted that `RotateBy` is in fact named by W16-QK and W16-RF in a form the overlay does not key on, so 70
    is an upper bound.
- The ring's gap by directory: rndobj 200 rows / 96,992 B, char 104 / 45,352, world 43 / 31,880, synth 61 / 14,236.

## 6. Next levers, ranked by native relevance, then size

Tiers as at 10-07:
- **A:** the charge can encode behaviour (source divergence, immediates, a missing body, a wrong callee), in a file
  native compiles today.
- **B:** a behaviour-class charge in a file native does not compile, or behaviour native compiles but does not yet
  run.
- **C:** codegen-only or naming. A fix moves the metric but cannot change native behaviour.

1. **[A] VIA-DC3 source-class rows in W16-PU/QF's population: 67 rows / 70,984 B, 67,520 B native-compiled** (§5).
   This is the VIA-DC3 analogue of 10-06's levers 2 and 3 (W16-PQ/PR). Read each row against retail for behaviour
   and give it a verdict, EQUAL or FIXED. Every row has been mentioned.
   - W16-QA's 31 are a comparison with current DC3, not with retail.
   - W16-PU opened about 25 of its 438 rows. This overlay does not key which of its 29 named here were among them.
   - Start with the ten rows where retail's body is ≥16 B larger (18,720 B, table in §5). W16-PU ranks size
     mismatches second among what found behaviour in this ring, and `Tessellate` (+204) and `SmoothResults` (+160)
     are the largest unexplained size gaps left in scope or VIA-DC3.
   - Then QA's 20 DIFF rows, which QA classed as differing from current DC3's body.
   - Price: W16-PQ/PR found 18 defects in 13 of 142 rows for +624 B. W16-SF found 8 behaviour fixes in 82 rows for
     +3,240 B. Expect defects, not bytes.
2. **[A] The last small behaviour-class rows: 3,940 B.**
   - In scope: the 8 unpaired named rows (524 B, all native-compiled) and `SuperFormatString`'s three-argument
     ctor (1,608 B, 94.33). W16-SG's merge message calls it "structure, deferred", and SG left no lane doc.
   - In VIA-DC3: the 13 unpaired named rows (804 B) and `OggMap::OpenMogg` (1,004 B, not native-compiled).
   - The unpaired rows are either a home question (the name is defined in another obj) or a missing body. Neither
     needs a codegen fight.
3. **[B] Native execution, now that compiling is solved.** 99.0% of the in-scope gap and 91.7% of VIA-DC3's is
   in compiled TUs, but W16-SH counts only about 7,564 B of in-scope gap rows under a runtime gate.
   - The instrument for the rest is `tools/native_runtime_rank.py` (W16-PK). It has not been rerun since W16-SH linked
     83 files, and it needs an instrumented build of all 18 targets.
   - Rank the 83,312 B of named in-scope source-class rows (§4.3) and lever 1's 67,520 B by whether native executes
     them, then gate what runs.
   - This is native lanes' work, and it is the step that turns "compiled" into "observed".
4. **[B] VIA-DC3 files native does not compile: 16,408 B in 27 files.** The largest are `StandardStream.cpp` 2,568,
   `VorbisReader.cpp` 2,048, `MetaMusic.cpp` 1,704, `Movie.cpp` 1,568 and `OggMap.cpp` 1,228, then the `Store*`
   files. Several are platform or codec backends, which native may replace by rule as it does `_Win.cpp`. In scope
   the same lever is drained (5 files / 2,032 B, all excluded with reasons).
5. **[C] VIA-DC3 register/reorder rows no lane has named or body-compared: 70 rows / 34,436 B**, rndobj and char
   (§5). W16-QA's DC3-newer body sweep took 29 of 967 rows to 100 outside rndobj/char. It was never run on these.
   It is cheap, and it is the only DC3-body pass with a measured yield (about 3%). These rows are register or
   reorder class, so a fix is expected to be codegen. That makes it tier C.
6. **[C] Identification residue: 211 rows / 14,924 B** (in scope 120 / 8,476, unchanged since 10-07; VIA-DC3 91 /
   6,448). This is bookkeeping unless a new channel appears. W16-QI left the in-scope rows with per-row reasons.
7. **[C] Name and pin leftovers: about 1,350 B.** All but the dtk-addend row are from W16-SW §6:
   - `MeshCacher::Sync` 620: no Wii channel 2.
   - VIA-DC3 #12/#13, 276: CodeWarrior inlines the destructors, so no channel.
   - Two pin leads, which first need a splits re-home: `BandRetargetVignette::Enter` (`0x822c5880`, inside the
     BandIKEffector pin) and `fn_827A2400` = `Achievements::Terminate` (inside the GuitarController pin). Naming
     either in place reads 0%, because those base objs cannot define the names.
   - The dtk-addend row, 448.
   - W16-OX's fill_insert fold (10,208 B in both rings) stays settled not admissible.
8. **[C] Tooling.**
   - dtk drops relocation addends on split target objs (W16-PW). It is the only charge on 1 in-scope row (448 B) and
     the only name charge on 2 OUT rows.
   - `native_stub_census.py`'s `STUB_RE` does not list `bandtrack_link_stubs.cpp` or `w16sh_link_support.cpp`, so a
     stub census run without the `nm` patch in §4.3 misses 36 definitions.
   - `ab_measure`'s carve refusal is fixed (W16-SP).

**Not ranked, for the user to decide (unchanged):** the permuter pilot.
- In scope, buckets 20, 23, 11, 22 and 25 hold 160,220 B of codegen residue, and 220 rows are at `mpn` 100.
- Recorded single-instruction prizes are unchanged: `App::App` (1,864 B), `SaveLoadManager::SetState` (4,096 B) and
  `ParseNode` (2,432 B).
- In VIA-DC3, the register/reorder bucket and the SF/SV SCHED rows add 95,504 B.

**Out of ranking by directive:** Quazal 113,520 B, XDK 2,948 B, 360-only 52,200 B, NET 3,868 B.

### 6.1 10-07's levers, now

| 10-07 lever | taken by | result | state |
|---|---|---|---|
| 1. VIA-DC3 source rows no lane had opened, 82 / 32,476 B | W16-SF, then W16-SN (its 18 FOLD rows) and W16-SO (the MemStream pair) | SF: 13 to 100 and 15 raised, 8 behaviour fixes (+14 / +3,240). SN: 11 relabels, 9 to 100 (+14 / +1,340). SO: +5 / +692. Every row has a disposition | **drained** |
| 2. wrong-callee rows, 33 / 2,812 B | W16-SG | mostly map-name errors, plus 3 source shapes (`MakeLoader` argument order, `FastSort<3>` stride, `GameGem::operator<`); 36 of 37 rows to 100 together with lever 3 (+53 / +6,568) | **drained** |
| 3. 4 in-scope no-record source rows, 2,060 B | W16-SG | 3 left the gap (2 to 100, 1 under a corrected name); `SuperFormatString` (1,608) deferred as structure | **drained** but one row, now §6 lever 2 |
| 4. VIA-DC3 non-native source rows, 13,624 B | W16-SV | 2 to 100 (+2 / +652), 4 raised, two native-only behaviour fixes (`Edge::operator<`, `kdTreeNode` bit-field order) | **drained** |
| 5. link the 96 in-scope files native did not compile, 97,340 B | W16-SH | 8 were already compiled as scatter guests; 83 linked into rb3-render with 17 dependencies; 5 not linked, with reasons; X360 Δ0 | **drained** |
| 6. identification residue, 218 / 15,604 B | none | 211 / 14,924 B: in scope unchanged, VIA-DC3 −7 rows / −680 B | open, §6 lever 6 |
| 7. name settlements, 28 proven + PH/cycle/type-witness | W16-ST, W16-SW | ST: 19 pairs installed (+22 / +4,368). SW: a new Wii call-site channel (91 agree / 2 disagree on settled pairs), +29 / +6,036 | **drained** but §6 lever 7's leftovers |
| 8. tooling | W16-SP | `ab_measure` measures carve patches; dtk addend open | half drained, §6 lever 8 |

Accuracy lanes at Δ0, all measured with `ab_measure`:
- W16-SC: retail's empty EH states traced to `IsModifierUnlocked`.
- W16-SJ: `kJoypadNumTypes` is 47 in RB3, not DC3's 49.
- W16-SK: engine count constants audited against retail, comments only.
- W16-SL: spotlight beam rotation and the NG drawer statics set to retail values.
- W16-SM: writable statics set to retail's initial values; `CamShot::GetCam` casts to PanelDir.

None moved a byte, and none was meant to.

## 7. Reading the numbers

- **In-scope match share is 94.74%** (10-07: 94.56%). The whole-binary share of the ceiling is **91.34%**.
- **This round was settlement, not codegen.** In scope, every byte that crossed was a name-class or no-record row,
  plus the funclets that followed them. The code-class blocks moved by 0 to 380 B.
- **In scope, 99.7% of the gap carries a stop reason.** The exception is 524 B of unpaired names. The codegen
  residue (160,220 B, 79.2%) is the permuter's market, and that is the user's call.
- **Native compiles 99.0% of the in-scope gap.** The behaviour question is no longer "is it linked?" but "does it
  run?", and that instrument (`native_runtime_rank.py`) is due.
- **The behaviour-class work left is in VIA-DC3**: 67 source-class rows / 70,984 B with mentions but no per-row
  verdict, 67,520 B of it native-compiled.
- **The ceiling did not move.** No lane pinned `auto_*` code, so the denominator held to the byte.

## 8. Not done

- **No source, map, splits, alias or `symbols.txt` edits.** Nothing was A/B-measured, because this branch changes no
  code. The native gate was not run (no `src/` change).
- **The overlays are coarse:**
  - "Named in a lane doc" is a doc mention, not a per-row verdict. Bare-name keying can also match a free function
    another doc names in passing.
  - The W16-SF/SV bucket keys on names. A row renamed by a lane other than W16-SN or W16-SV falls through to the
    older buckets.
  - The native column is TU-level. A row in a compiled TU may be behind a conditional scatter edge in some hosts,
    and may be dead code on every native run. `native_runtime_rank.py` was not rerun.
- I did not fix `native_stub_census.py`'s `STUB_RE`; §4.3 patches its output with `nm` instead.
- The per-lane walk does not cover W16-SB, W16-SD's doc merge, W16-SP or the native pins. None touches a
  match-build input.

Scripts and JSON are in `~/tmp/w16ta-gap/` and are not committed. Reproduce in this order, from that directory, with
`W=~/tmp/wt-w16ta` built at `2fd722b8f`:

```
python3 gengap.py $W gap.json                      # 1,725 gap rows, asserts ceiling - matched
python3 scope_ledger2.py $W ledger2.json           # ring ledger, asserts total_code and matched_code
python3 diffall.py $W gap.json cls.json            # one objdiff diff per gap row, W16-NA classifier
python3 unpaired.py $W                             # U2/U3/U4 split; 200/200 index control
python3 overlay4.py                                # overlay3 + W16-S* pools, W16-S* doc mentions, 10-07 dispositions
python3 insdel_shape.py $W                         # opset shapes
python3 tables3.py > tables3.txt                   # klass(), rings, final.json
python3 mkcensus.py && python3 pairs.py            # rows with a symbol-kind charge, their name pairs
python3 chase_all.py                               # icf_pair_adjudicate chase per pair
python3 namecls.py && python3 final3.py            # name verdicts; placeholder-only control (none == graded)
python3 tables4.py > tables4.txt                   # in-scope partition, class blocks, dirs
(cd ~/code/milohax/rb3-xenon && python3 tools/native_linked_tus.py . --out ~/tmp/w16ta-gap/native_linked.txt)
                                                   # then native_src.json = that list ∪ native_src_edges.json (620 files)
python3 dispo4.py > dispo4.txt                     # §4 table (asserts)
python3 dispo_via4.py > dispo_via4.txt             # §5 table (asserts)
python3 natrel.py; python3 viabreak.py             # §4.3 / §5 native and bucket breakdowns
python3 stubmatch.py                               # §4.3 stub match (census + nm of the two unlisted stub TUs)
python3 cmp1007.py > movement.log                  # §2 rings vs 10-07, in-scope row movement (asserts)
python3 leftwhere.py; python3 rowsdown.py          # where left rows went; whole-binary rose/fell
python3 claims.py                                  # §1 attribution
(cd walk && bash walk.sh); python3 lane_rings2.py  # §3
```

- **W16-SD's pipeline, copied and repointed:** `gengap`, `scope_ledger2`, `diffall` (8 threads instead of 16, for
  memory; results are per-row and order-independent), `unpaired`, `insdel_shape`, `tables3`, `mkcensus`, `pairs`,
  `chase_all`, `namecls`, `final3`, `tables4`, `leftwhere`, `walk.sh`.
- **New in this edition:**
  - `overlay4.py`, `sfcls.py`, `dispo4.py`, `dispo_via4.py`: §4 and §5.
  - `cmp1007.py`, `rowsdown.py`: movement.
  - `natrel.py`, `viabreak.py`, `stubmatch.py`: native relevance.
  - `lane_rings2.py`: §3.
  - `native_src.json` (620 files), `native_src_edges.json` (550 files, the old rule).
- Per-row tables: `dispo_rows.json` (in scope) and `dispo_via_rows.json` (VIA-DC3).
