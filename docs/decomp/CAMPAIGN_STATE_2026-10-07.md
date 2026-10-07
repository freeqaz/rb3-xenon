# Campaign state — 2026-10-07 (after W16-PG … W16-RV)

Ninth edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-06.md` for numbers and
priorities. It reruns 10-06's method (itself 10-03b's and W16-OV's) with three changes, each stated where it is used:
the "opened" overlay also reads the W16-Q*/W16-R* docs and keys free functions by bare name (§4.1), the disposition
buckets are updated for the lanes that landed (§4), and the VIA-DC3 ring gets its own disposition table because the
user funded it on 10-06 (§5). 45 lanes moved or touched the match build between the two measurements. Scope rules are
W16-OV's: the native port is the goal, XDK is hard-skipped, Quazal stays as landed. No source, map, splits, alias or
`symbols.txt` edit on this branch.

## 1. Measured

I used a fresh `scripts/setup_worktree.sh` worktree at main **`24d250690`** (W16-SA, the last native pin after
W16-RV) and ran a full `./tools/ninja-locked`. The build ended `BUILD_RC=0` with the last edges
`[pairing] 1056/1056 declared compiled objects pair with a target` and `[patch-state] OK: tree is a fixed point of 6
post-compile passes`. Ruler: `name_check` (objdiff 4.2.9, tool commit `a5f0ea903ec1`, binary hash
`5a51cd51fe0a353f`, read from `report.json`'s `provenance`). Main has since moved to `029f16e04` (W16-SB), which
changes `native/src/main_render.cpp` and the engine pin only.

```
python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 24d250690 (2026-10-07, W16-SA)"
  scaffold thr<=4 : 0 · thr<=5 : 0 · thr<=6 : 56 units, 289 rows, 57,448 B · thr<=7 : 56 (cliff, not fitted)
total_code            10,247,844
PAIRABLE               6,703,512 = 65.414%   (1,056 units, 56,318 rows)
− scaffold shells         57,448             (56 units, 289 rows)
= reachable ceiling    6,646,064 = 64.853%
matched_code           6,047,896 = 59.016% of total_code = 91.00% of ceiling
gap to ceiling           598,168
matched_functions         54,755 / 68,884   masked_equal 25,203   honest 29,552
fuzzy_match_percent       64.2679
```

Against 10-06 (`7e5a99800`): **+122,020 B / +1,271 fns**, ceiling **+23,516 B** (64.624% → 64.853%), share of the
ceiling 89.48% → **91.00%**, gap 696,672 → **598,168 B (−98,504)**. `total_code` moved +52 B and `total_functions`
−25 (W16-RC removed 24 phantom rows).

⚠ **I did not write a prediction for the headline before reading it**: `ceiling_recompute.py` prints `matched_code`
with the ceiling. The attribution row below was therefore computed afterwards and is a reconciliation, not a test of a
prediction. The walk controls (step 0, last step) were predicted before they ran.

| control | result |
|---|---|
| attribution | the 45 lanes' own whole-binary A/B claims (merge subjects; lane docs where the subject has none) sum to **+1,271 fns / +122,020 B**, residual **0 / 0** against the measured Δ (`claims.py`) |
| lane walk, step 0 | rebuilding `7e5a99800` from scratch on the RB3DX image reproduces 10-06's 5,925,876 B / 53,484 fns **and every ring** of its ledger (`lane_rings.py`: `equals 10-06 ledger on every ring: True`) |
| lane walk, last step | see §3 |
| independent baseline | main's own `build/45410914/report.json` (built at 01:08, after W16-RV) equals this build on all seven headline keys |
| population sums | reachable rows sum to the ceiling and fuzzy==100 rows to `matched_code`; gap rows (1,875) sum to 598,168 = ceiling − matched (`gengap.py`) |
| ring sums | tier totals sum to `total_code` and `matched_code` (`scope_ledger2.py` asserts both) |
| disposition tables | §4's buckets sum to 599 rows / 209,164 B = in-scope ceiling − matched (`dispo3.py` asserts); §5's to 573 rows / 214,104 B = the VIA-DC3 gap (`dispo_via.py` asserts) |
| row movement | left − entered − size changes = −54,480 B = Δ in-scope gap (`cmp1006.py` asserts) |
| placeholder correction | 21/21 placeholder-only N2 rows read identical fuzzy at `none` and graded (10-06: 27/27) |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail" |
| unpaired index | 200/200 paired named rows found in their own base obj |
| stub match (§4.3) | the qualified-name matcher finds 198 in-scope rows that a live stub also defines (all at 100), so its 0 for gap rows is not vacuous |

## 2. Rings

From `cmp1006.py` (this build's `ledger2.json` against 10-06's).

| tier | reachable | matched | **gap** | share | gap at 10-06 | Δ matched | Δ fns | Δ reach |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 478,172 | 433,900 | **44,272** | 90.74% | 54,724 | +11,328 | +131 | +876 |
| IN-SOON | 2,438,636 | 2,332,976 | **105,660** | 95.67% | 125,504 | +33,312 | +633 | +13,468 |
| IN-RB3ENG | 926,036 | 866,804 | **59,232** | 93.60% | 83,416 | +33,632 | +170 | +9,448 |
| **in scope** | **3,842,844** | **3,633,680** | **209,164** | **94.56%** | 263,644 | **+78,272** | **+934** | +23,792 |
| VIA-DC3 (DC3 has the file) | 2,026,288 | 1,812,184 | 214,104 | 89.43% | 249,256 | +34,604 | +241 | −548 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 113,520 | 0 | 0 | 0 |
| OUT-NET | 76,632 | 72,264 | 4,368 | 94.30% | 5,968 | +1,368 | +32 | −232 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 | 0 |
| OUT-360-OTHER | 349,068 | 296,868 | 52,200 | 85.05% | 59,116 | +6,900 | +49 | −16 |
| UNKNOWN-other | 3,532 | 1,668 | 1,864 | 47.23% | 2,220 | +876 | +15 | +520 |

- **Native scope CORE+SOON:** 98.95% of functions and 94.86% of bytes (10-06: 98.29% / 93.79%).
- **The VIA-DC3 gap is now larger than the in-scope gap** (214,104 vs 209,164 B), for the first time. The in-scope
  gap fell 54,480 B (−20.7%), VIA-DC3's 35,152 B (−14.1%).
- **Reach grew in scope by 23,792 B and the ceiling by 23,516 B.** That is code leaving `auto_*` (UNKNOWN-anon
  −23,248 B) for named units: W16-QI's static-destructor stubs, W16-QH's initialisers, and the W16-QM / QP / QR / QF
  pins, net of W16-RM's unpins back into `auto_*`. Rows that arrive this way at 100 raise reach and matched together,
  so they do not show up in the gap. The scaffold set lost one unit (−216 B), `ssluse.c`, whose only block W16-RM
  moved to `auto_*`.

**Gap movement, 10-06 → now,** keyed on (unit, symbol), with a name that leaves one in-scope unit and enters another
paired as a re-home:

- **Left the in-scope gap:** 255 rows / 59,692 B. Where each went, looked up in this tree (`leftwhere.py`, then
  placeholders resolved through `scripts/target_symbol_map.json`):

  | where the row is now | rows | bytes |
  |---|---:|---:|
  | same unit, fuzzy 100 | 109 | 48,444 |
  | placeholder `fn_` now named, at 100 | 70 | 4,240 |
  | moved to `auto_*` (unpinned; W16-RM's RockCentral run, W16-RI's DataNode drop) | 43 | 5,160 |
  | renamed (map corrections, e.g. W16-PG's FileCache heap trio) | 17 | 1,420 |
  | placeholder whose address left the map (W16-RC carve withdrawals) | 6 | 224 |
  | named now, still below 100 | 3 | 104 |
  | re-homed to another unit: at 100 (4 in scope, 1 VIA-DC3) / below 100 (2) | 7 | 100 |
  | **total** | **255** | **59,692** |

  So 52,768 B left by reaching 100 and 5,384 B by leaving the denominator.
- **Entered:** 11 rows / 5,020 B. Two are re-homes (`fn_827E6178`/`fn_827E6180`, MidiParser → TrackWidgetImp,
  16 B). The rest are new pins or names, none a regression: `NoteTube::DrawToPlate` 3,184 (pinned out of `auto_*` by
  W16-QP/QR), `SuperFormatString`'s three-argument ctor 1,608 (W16-QF's new pin), `fn_8239C558` 84 (W16-QB withdrew
  its name), `LocaleChunkSort::FastSort<3>` 72, `fn_82272E50` 16 (W16-QV left it unnamed), a NullLocalBandUser
  vtordisp thunk 16 and `SetTrackPanel` 8 (W16-RI), `??__FsConditionalTimersEnabled` 12 (W16-QI), and the TrackWidget
  sort thunk 4 (W16-QB).
- Three rows still in the gap changed size (+192 B, W16-RC re-carves). Net −54,480 B, which reconciles exactly.
- **Still in the gap:** 41 rows rose and 2 fell, `FileCache fn_82519FF8` 60 → 0 and `UsbMidiGuitar fn_82C3FA20`
  98.33 → 0 (12 B each). Both are W16-QI's "byte-signature pairing lost its borrowed partner" rows.
- **Whole binary**, over the 67,038 rows present under the same (unit, name) in both trees: 295 rose, 5 fell, all
  ≤12 B and all recorded by their lane (the two above, W16-QI's `FileMergerOrganizer fn_823DA748`, W16-RI's
  `SetTrackPanel` 100 → 0, W16-QB's sort thunk 100 → 95). Two of the five fell from 100: 12 B.

## 3. What landed, per lane

10-06 split each lane's Δ by ring from its archived `ab_measure` legs. Most of this round's lane worktrees, and their
run dirs, were already removed. So I rebuilt every step instead (`walk/walk.sh`, `walk/walk2.sh`, run in two
private worktrees):
- **Steps:** the 51 first-parent commits from `7e5a99800` to `4a14afeb3` (W16-RV) that are neither a native pin nor
  a CLAUDE.md-only commit. Pins and CLAUDE.md changes are not match-build inputs, and `24d250690` is a pin on top of
  `4a14afeb3`.
- **Per step:** check out the commit; put in the image it targets (RB3DX before `a3f3339a8`, clean TU5 from then
  on); run `configure.py`; build until `symbols.txt` is unchanged by a build (every step took one build); wipe
  `report.json` and `report.cache`; regenerate the report and snapshot it.
- **Ring:** `scope_ledger2.tier()` of the unit's source path, the same classifier for every step.
- **A lane's Δ** is its merge's report minus the previous step's report.

Controls, all predicted to hold before the walk ran, and all hold (`lane_rings.py`):
- **Step 0** (`7e5a99800`) reproduces 10-06's ledger on every ring and on both headline keys.
- **The last step** (`4a14afeb3`) equals this worktree's build on `matched_code`, `matched_functions` and every ring.
- **The steps sum** to +1,271 fns / +122,020 B whole-binary, +78,272 B / +934 fns in scope, +34,604 B VIA-DC3, and
  each other ring's §2 delta.
- **Every one of the 45 lanes in `claims.py` equals its own A/B claim exactly**, fns and bytes. The three merges
  without a claim (W16-RT, RW, RX) read +0.

Eleven steps read zero on every ring and are left out of the table: W16-PN (10-06's doc), PQ (Δ0, as priced), PX,
PZ, QD, RT, RW, RX, RY, the clean-TU5 README commit and the alias-placeholder commit `377f357a6`.

| lane | merge | Δ fns | Δ matched_code | Δ in scope | CORE / SOON / RB3ENG | Δ in-scope fns | Δ VIA-DC3 | other rings |
|---|---|---:|---:|---:|---|---:|---:|---|
| W16-PG | `614420e78` | +14 | +4,944 | **+4,928** | +724 / +448 / +3,756 | +13 | +16 | — |
| W16-PK | `409cc160e` | +7 | +1,648 | **+264** | +156 / +108 / +0 | +3 | +1,384 | — |
| W16-PM | `dc238f637` | +0 | +0 | **-104** | +0 / -104 / +0 | -1 | +104 | — |
| W16-PR | `7f41fe33e` | +2 | +624 | **+624** | +0 / +0 / +624 | +2 | +0 | — |
| W16-PS | `c8e56eba1` | +4 | +3,356 | **+3,028** | +280 / +972 / +1,776 | +3 | +328 | — |
| W16-PT | `a3f3339a8` | +5 | +1,240 | **+1,164** | +348 / +816 / +0 | +4 | +0 | UNK +76 |
| W16-PO | `9480cb75e` | +9 | +1,880 | **+1,880** | +1,880 / +0 / +0 | +9 | +0 | — |
| W16-PY | `90a3e94d2` | +3 | +872 | **+0** | +0 / +0 / +0 | +0 | +872 | — |
| W16-PW | `60c34cf1a` | +1 | +424 | **+0** | +0 / +0 / +0 | +0 | +424 | — |
| W16-PU | `c2e3a92f6` | +2 | +752 | **+0** | +0 / +0 / +0 | +0 | +752 | — |
| W16-QC | `2b9c29c43` | +7 | +1,604 | **+1,604** | +0 / +316 / +1,288 | +7 | +0 | — |
| W16-QA | `d8aef7dcf` | +31 | +7,184 | **+1,552** | +1,472 / +80 / +0 | +14 | +1,336 | 360 +4,296 |
| W16-QB | `5fcd005d9` | +8 | +796 | **+704** | +80 / +624 / +0 | +7 | +92 | — |
| W16-QH | `bd0b1c74c` | +25 | +1,688 | **+1,004** | +468 / +536 / +0 | +11 | +436 | 360 +248 |
| W16-QF | `9fd8de7b1` | +6 | +384 | **+168** | +168 / +0 / +0 | +3 | +216 | — |
| W16-QG | `c4eae7348` | +46 | +2,796 | **+2,208** | +828 / +712 / +668 | +36 | +512 | 360 +8, NET +68 |
| W16-QJ | `0ec8e12d4` | +7 | +1,472 | **+80** | +0 / +0 / +80 | +1 | +1,164 | 360 +228 |
| W16-QI | `2d3582fa3` | +920 | +25,240 | **+19,720** | +1,780 / +15,168 / +2,772 | +730 | +3,892 | 360 +812, NET +788, UNK +28 |
| W16-QK | `bb82b4cae` | +12 | +2,948 | **+1,692** | +0 / +1,408 / +284 | +3 | +1,256 | — |
| W16-QL | `ad2929d31` | +2 | +76 | **+12** | +12 / +0 / +0 | +1 | +64 | — |
| W16-QM | `1059a558f` | +12 | +952 | **+840** | +0 / +40 / +800 | +10 | +112 | — |
| W16-QO | `ca61b767f` | +22 | +7,720 | **+1,840** | +548 / +0 / +1,292 | +3 | +5,356 | NET +524 |
| W16-QP | `cecef177b` | +2 | +212 | **+212** | +0 / +0 / +212 | +2 | +0 | — |
| W16-QR | `014662e57` | +1 | +272 | **+272** | +0 / +0 / +272 | +1 | +0 | — |
| W16-QU | `76ff16e11` | +5 | +492 | **+0** | +0 / +0 / +0 | +0 | +0 | UNK +492 |
| W16-QQ | `442f01984` | +35 | +9,864 | **+3,312** | +0 / +0 / +3,312 | +12 | +6,616 | 360 -64 |
| W16-QW | `1fe4ab41c` | +4 | +144 | **+0** | +0 / +0 / +0 | +0 | +80 | 360 +64 |
| W16-QV | `d5d873c95` | +8 | +280 | **+0** | +0 / +0 / +0 | +0 | +0 | UNK +280 |
| W16-QX | `d76ab0ca4` | +4 | +5,960 | **+5,928** | +0 / +2,228 / +3,700 | +2 | +32 | — |
| W16-RB | `b5a56416b` | +5 | +2,664 | **+2,664** | +0 / +248 / +2,416 | +5 | +0 | — |
| W16-QZ | `1b62177bd` | +3 | +4,512 | **+4,512** | +0 / +2,276 / +2,236 | +3 | +0 | — |
| W16-RC | `5aa1d53b0` | +4 | +300 | **+264** | +20 / +100 / +144 | +3 | +0 | 360 +36 |
| W16-RD | `f883478b5` | +2 | +4,248 | **+2,108** | +2,108 / +0 / +0 | +1 | +2,140 | — |
| W16-RG | `6a7e96479` | +8 | +1,792 | **+0** | +0 / +0 / +0 | +0 | +1,792 | — |
| W16-RF | `dd88a0ee7` | +4 | +5,800 | **+0** | +0 / +0 / +0 | +0 | +5,800 | — |
| W16-RI | `f7cdbf7ca` | +36 | +3,316 | **+1,764** | +456 / +1,176 / +132 | +28 | +280 | 360 +1,272 |
| W16-RM | `e5a8ad4b3` | -20 | -748 | **-284** | +0 / -244 / -40 | -7 | -452 | NET -12 |
| W16-RK | `f22f71f15` | +8 | +5,228 | **+5,228** | +0 / +4,244 / +984 | +8 | +0 | — |
| W16-RQ | `774f9fe86` | +7 | +2,376 | **+2,376** | +0 / +264 / +2,112 | +7 | +0 | — |
| W16-RV | `4a14afeb3` | +10 | +6,708 | **+6,708** | +0 / +1,896 / +4,812 | +10 | +0 | — |
| **sum of steps** | | **+1,271** | **+122,020** | **+78,272** | +11,328 / +33,312 / +33,632 | +934 | +34,604 | 360 +6,900, NET +1,368, UNK +876 |

Columns:
- `360` is OUT-360-OTHER, `NET` is OUT-NET, `UNK` is UNKNOWN-other.
- `Δ in-scope fns` counts `mpn == 100` rows in the three in-scope rings.
- W16-PM's step moves 104 B from IN-SOON to VIA-DC3 at Δ0. That is the matched row at `0x82449930`, which it
  re-pinned to `Part.cpp` as `vector<RndParticleSys::Burst>::_M_erase` (its merge message: "Δ0, one row re-homed").
  It is not a regression.
- `Δ total_code` is +52 B (W16-RC's re-carves) and `total_functions` −25 (W16-RC −24 phantom rows, W16-QP −1); no
  other step moves either.

Where the in-scope bytes came from:
- **W16-QI is a sixth of all in-scope bytes (19,720 of 78,272).** Its static-dtor stubs are tiny and many: 920 of
  the round's 1,271 fns.
- **Row lanes take the next tier:** W16-RV 6,708, QX 5,928, RK 5,228, PG 4,928, QZ 4,512, QQ 3,312.
- **The behaviour readers that 10-06 ranked 1–4** (PO, PQ, PR, PS) bought 5,532 B together, as priced. The defects
  they fixed are mostly score-invisible.
- **VIA-DC3 gains are spread out:** QQ 6,616, RF 5,800, QO 5,356, QI 3,892, RD 2,140, RG 1,792, PK 1,384, QA 1,336,
  and only 752 from W16-PU.

## 4. Where the in-scope gap stands: one disposition per row

Each in-scope gap row (599 / 209,164 B) goes to the first bucket that fits (`dispo3.py`). **"Native-compiled"** means
the row's source file is compiled into at least one target of main's native build (`native/build/build.ninja`,
regenerated 01:44 today): **451 `src/` files**, 418 at 10-06 + 36 linked by W16-PX − 3 that a native lane moved to an
explicit exclude list (`GemRepTemplate`, `AccomplishmentProgress`, `HamLabel`).

| disposition | rows | bytes | % | CORE | SOON | RB3ENG | native-compiled |
|---|---:|---:|---:|---:|---:|---:|---:|
| read for behaviour by W16-PQ/PR: no defect left, codegen residue | 107 | 96,028 | 45.9% | 21,680 | 45,924 | 28,424 | 54,172 |
| register/schedule: named in a later lane's doc, stopped | 56 | 40,188 | 19.2% | 6,232 | 19,964 | 13,992 | 16,508 |
| register/schedule: W16-PF's lever applied, stopped, no later record | 39 | 18,540 | 8.9% | 0 | 18,540 | 0 | 6,308 |
| source divergence: worked since 10-06, left with a stop reason | 28 | 11,992 | 5.7% | 3,404 | 2,868 | 5,720 | 8,476 |
| identification residue (U2) | 120 | 8,476 | 4.1% | 2,356 | 4,064 | 2,056 | 5,004 |
| name: pointer `_M_fill_insert` fold, settled not admissible (W16-OX) | 14 | 6,760 | 3.2% | 2,388 | 3,572 | 800 | 3,756 |
| EH funclet / anonymous fragment of a parent row | 164 | 6,744 | 3.2% | 1,008 | 3,860 | 1,876 | 3,404 |
| register: W16-PS's lever 4/5 applied, stopped | 12 | 5,684 | 2.7% | 2,736 | 1,468 | 1,480 | 4,924 |
| never-worked row taken by W16-PO, left with a stop reason | 9 | 5,188 | 2.5% | 2,356 | 0 | 2,832 | 2,104 |
| name: cycle survivor parked UNDECIDABLE by W16-PH | 11 | 3,664 | 1.8% | 0 | 2,524 | 1,140 | 2,584 |
| name: proven, installable or conflicted | 13 | 2,540 | 1.2% | 184 | 2,024 | 332 | 2,032 |
| source divergence / immediates: no record | 4 | 2,060 | 1.0% | 1,796 | 264 | 0 | 2,060 |
| unpaired named (U3/U4) | 8 | 524 | 0.3% | 120 | 104 | 300 | 424 |
| name: dtk drops the reloc addend on the target obj, our constant is retail's | 1 | 448 | 0.2% | 0 | 448 | 0 | 0 |
| name: wrong callee / no body | 8 | 288 | 0.1% | 12 | 4 | 272 | 36 |
| name: undecidable, needs a type witness | 5 | 40 | 0.0% | 0 | 32 | 8 | 32 |
| **total** | **599** | **209,164** | 100% | 44,272 | 105,660 | 59,232 | **111,824** |

### 4.1 How the buckets are decided

The class (`kc`) is W16-NP's `klass()` with W16-OV's placeholder correction, unchanged. Name-class rows (N1/N2) take
their bucket from the worst non-placeholder retail target's chase verdict, as before. For the code classes the most
specific record wins, in this order:

1. **W16-PQ / W16-PR's pools.** 10-06 levers 2 and 3. Each row was read against retail for behaviour and given a
   verdict: EQUAL, or FIXED and re-measured. W16-PQ found 8 defects in 6 rows and W16-PR 10 in 7; every one was fixed.
   The bucket keeps PQ's one deliberate exception, `NewFile` (424 B), whose dead `gNullFiles` branch costs
   −4 fns / −88 B to remove. Pools: 10-06's dispositions 08/09 split on native-compiled, which is how both lanes
   built them (W16-PQ reproduced 65 rows / 49,784 B; W16-PR's file is `~/tmp/w16pr/rows_after.json`).
2. **W16-PO's pool** (10-06 lever 1, `dispo` 08a + 10).
3. **W16-PS's pool**, register rows only: its 14 lever-4 rows plus the 9 rows its anchor census classed STACK-only or
   ANCHOR-only (W16-PS §method: "86 MIXED, 7 STACK-only, 2 ANCHOR-only"; it says the MIXED rows "were not opened
   beyond the lever-4/5/6 populations", so they are not counted here).
4. **A later lane names the row.** Its short name appears in backticks in a W16-PG … W16-RV doc, or W16-QA compared
   its body with current DC3 (classes DIFF / SAME / PARTIAL / OURS_ONLY / DC3_ONLY of `~/tmp/w16qa/body_cmp.json`).
   This is as coarse as 10-06's overlay: a mention is not a per-row verdict. **New in this edition:** a piece with no
   `::` is also keyed by its bare name when it is an identifier of six or more characters. That adds free functions
   such as `JoypadPollCommon`, `ParseNode` and `MakeBSPTree`, which W16-QQ and W16-RD worked and the old overlay could
   not see. Mentioned rows went 461 → 851 of 1,875.
5. Then 10-06's own record: W16-PF's pool, W16-PC's pool, older mentions.

`06b` is new. `ProfileMgr::GetJoypadExtraLagInits` (448 B, 99.91) has one charge, retail `__real@41600000` (14.0)
against our `__real@42940000` (74.0). W16-PW §"two instrument findings" shows retail loads 74.0 at `0x82091FC0`:
dtk keeps the containing label `0x82091FBC` and drops the +4 addend. Our constant is right and the row cannot
close by source. It is the only in-scope row whose sole non-placeholder name charge is `__real@` against `__real@`;
whole-binary there are two more, both FFT rows in OUT-360-OTHER (1,620 B).

### 4.2 How it adds up

- **Codegen residue with a behaviour or lever record: 160,440 B, 76.7%.** These are buckets 20, 23, 11 and 22. Half
  is rows W16-PQ/PR read line by line against retail and found to do what retail does. The rest are register or
  scheduling rows that W16-PF's IR-temporary lever, W16-PS's levers 4–6, or a named row lane since 10-06 (W16-QK,
  QO, QQ, QX, QZ, RB, RD, RF, RK, RQ, RV) stopped on.
- **Source-class rows with a stop reason: 17,180 B, 8.2%.** W16-PO's 9 rows and 28 rows worked since 10-06. Their
  records are mostly codegen too (FP order, a `clrrwi` zero-extend, copy coalescing, operand order).
- **Settled, non-separable or parked: 26,132 B, 12.5%.** Funclets, W16-OX's fill_insert fold, W16-PH's undecidable
  pairs, type-witness names, the dtk-addend row, and the U2 residue that W16-OZ's pipeline and this round's
  identification lanes (QH, QI, QM, RI, RC, QG) have already run over.
- **Open: 5,412 B, 2.6%.** 13 proven or installable names (2,540), 4 source rows with no record (2,060), 8 unpaired
  named rows (524) and 8 wrong-callee rows (288).
  - The 4 no-record rows: `SuperFormatString`'s ctor 1,608 is in fact W16-QF's (it wrote RB3's three-argument form
    from retail, unpaired → 94.33; its doc names it without `::`). The others are `LocaleChunkSort::FastSort<3>` 72
    (W16-QB category 3 named it and left it at 79.7) and two STLport template rows (264, 116).

At 10-06 the never-attempted surface was 13,444 B. Now nothing in scope is unattempted except those three small
template rows (452 B).

### 4.3 Native relevance

- **111,824 B (53.5%) of the in-scope gap is in a file the native build compiles today** (10-06: 41%). The share rose
  because W16-PX linked 36 track and vocal drawing files, not because those rows moved. The native-compiled files
  with the most gap are VocalTrack.cpp (11,532 B), Geo.cpp (6,308), Dir.cpp (3,624), NoteTube.cpp (3,576),
  ChordShapeGenerator.cpp (3,536) and VocalPart.cpp (3,496).
- **97,340 B sits in 96 in-scope files native does not compile yet.** The largest are BandPatchMesh (7,584 B),
  GemManager (5,492), CustomizePanel (5,428), BandDirector (4,620), BandCamShot (4,124) and SaveLoadManager (4,096).
- **No in-scope gap row is replaced by a live native stub.** `tools/native_stub_census.py native/build stubs.json
  --authored`, run read-only on main's native build, finds 964 distinct live stub symbols. None matches an in-scope
  gap row by qualified name, while 198 in-scope rows at 100 do match, so the matcher works. At 10-06 the one match
  was `VocalTrack::RebuildHUD`, which W16-QX took to 100. In VIA-DC3 two gap rows are stubbed natively:
  `CacheResource` (304 B, 38.42) and `Fader::DoFade` (444 B, 93.09).
- **What native can still observe in scope** is the source-class remainder (17,180 B with a record, 2,060 B
  without) and the wrong-callee rows (288 B). The 160,440 B of codegen residue cannot change what native does: its
  instructions are the same operations in another order or register.

### 4.4 Class view (W16-OV's table, same rules)

| block | classes | rows | bytes | % | at 10-06 |
|---|---|---:|---:|---:|---:|
| register/scheduling only | P1 + P2 | 115 | 71,336 | 34.1% | 72,520 |
| reorder-shaped insert/delete | I1 + I2 | 36 | 24,276 | 11.6% | 32,644 |
| source divergence | I3 + I4 + M1 + M2 | 140 | 85,480 | 40.9% | 116,800 |
| relocation name | N1 + N2 | 180 | 19,072 | 9.1% | 20,924 |
| unpaired | U2 + U3 + U4 | 128 | 9,000 | 4.3% | 19,592 |
| TU5 image | S0 | 0 | 0 | 0.0% | 1,164 |

- S0 is gone: W16-PT retargeted to clean TU5 and the five DX rows went to 100.
- 227 of the 599 rows are already at `mpn` 100: their only charges are argument-level (register or name).
- By directory the gap concentrates in `system/bandobj` (136 rows / 47,880 B) and `band3/meta_band` (128 / 46,280),
  then `band3/bandtrack` (26 / 22,040) and `band3/game` (63 / 19,032).
- The top 100 rows hold 65.0% of the gap, and the median row is 104 B. The largest row is still
  `VocalTrack::UpdateScrolling` (8,948 B, 97.35): W16-PR fixed two score-invisible defects in it, and W16-QX and
  W16-RQ stopped on deque-size scheduling.

## 5. The VIA-DC3 ring

The user funded this ring on 10-06 ("fund it alongside in-scope work", 1–2 lanes a round, rndobj/char first, native
behaviour first). W16-PU (rndobj + char), W16-QA (src/system outside rndobj/char, bodies against current DC3), W16-QF
(world/synth/ui/meta/movie/flow) and W16-QJ worked it, and its gap fell 249,256 → 214,104 B. Same rules as §4, with
the ring's own lanes first (`dispo_via.py`):

| disposition | rows | bytes | % | native-compiled |
|---|---:|---:|---:|---:|
| in W16-PU/QF's population, source-class charge | 149 | 103,460 | 48.3% | 57,004 |
| in W16-PU/QF's population, register/reorder class | 119 | 81,712 | 38.2% | 48,592 |
| identification residue (U2) | 98 | 7,128 | 3.3% | 3,892 |
| EH funclet / anonymous fragment | 128 | 5,412 | 2.5% | 2,856 |
| name: pointer fill_insert fold (W16-OX) | 7 | 3,448 | 1.6% | 1,704 |
| name: proven, installable or conflicted | 15 | 2,936 | 1.4% | 1,024 |
| name: wrong callee / no body | 25 | 2,524 | 1.2% | 2,192 |
| source divergence worked since 10-06 (`RndPropAnim::ForeachKeyframe`, paired by W16-QM) | 1 | 2,256 | 1.1% | 2,256 |
| name: cycle-assumed survivor outside W16-PH's set | 9 | 2,060 | 1.0% | 1,100 |
| name: undecidable, needs a type witness | 7 | 1,136 | 0.5% | 960 |
| source divergence, no record (`OggMap::OpenMogg`, W16-RG: stack-temp slot sharing) | 1 | 1,004 | 0.5% | 0 |
| unpaired named (U3/U4) | 13 | 804 | 0.4% | 328 |
| register, no record (`OggMap::SetKey`, W16-RG: pure register allocation) | 1 | 224 | 0.1% | 0 |
| **total** | **573** | **214,104** | 100% | **121,908** |

"In W16-PU/QF's population" is not "read". W16-QF gave a stop reason to all 309 of its rows ("none of them is a
behaviour difference in our source"). W16-PU's §3 table opens about 25 of its 438 and lists the rest as not opened.
Split by whether any doc since 10-06 names the row:

- **Source-class: 82 of the 149 rows / 32,476 B (18,852 native-compiled) are named by no lane since 10-06**, all in
  rndobj, char, world and synth. The largest are `RndTexBlender::DrawShowing` 1,880 (retail 16 B larger),
  `BuildVisit` 1,344 (+44), `MakeNormals` 1,332, `RndMeshDeform::Reskin` 1,232,
  `RndTransformable::ApplyDynamicConstraint` 1,224, `TessellateMesh` 1,176, `RndLine::UpdateLinePair` 1,076 (+56),
  `kdTreeNode::Pack` 972 (+24) and `RndText::ParseMarkup` 924.
- **Register/reorder: 76 of the 119 / 36,520 B (21,860 native-compiled) are named by none**, led by
  `RndScaleObject` 3,112 and `CharBones::RotateBy` 1,420. W16-QK names the latter without `::`, so the count is an
  upper bound.
- Retail's body is at least 16 B larger than ours on 19 VIA-DC3 rows / 26,868 B, against 6 rows / 4,344 B in scope.
  W16-PU ranks size mismatches second among what found behaviour in this ring (`RndText::WrapText`).
- The ring's gap by directory: rndobj 245 rows / 104,236 B, char 137 / 50,320, world 49 / 32,568, synth 69 / 15,056.

## 6. Next levers, ranked by native relevance, then size

Native relevance has three tiers, as at 10-06:
- **A:** the charge can encode behaviour (source divergence, immediates, a missing body, a wrong callee), in a file
  native compiles today.
- **B:** a behaviour-class charge in a file native does not link yet.
- **C:** codegen-only or naming. A fix moves the metric but cannot change native behaviour.

VIA-DC3 rows are ranked with in-scope rows now that the ring is funded.

1. **[A] VIA-DC3 source-class rows no lane has opened: 82 rows / 32,476 B, 18,852 B native-compiled** (§5).
   - Mostly rndobj. Start with the native-compiled rows where retail's body is larger:
     `RndTexBlender::DrawShowing` 1,880 (+16), `RndLine::UpdateLinePair` 1,076 (+56), `BoxMapLighting::CacheData`
     316 (+16) and `NgDOFProc::Set` 300 (+20). Then the member-offset candidates W16-PU listed and did not open
     (`ApplyDynamicConstraint`, `RndCam::GetViewProjectXfms`, `CharCollide::Highlight`, `NgLight::BlurShadowRT`,
     `CharClipGroup::MakeMRU`).
   - Method, from W16-PU §1's yield order: current DC3 first, then size mismatches against the Ghidra decompile, then
     retail tables. Op-multiset differences found nothing.
   - Price: W16-PU found behaviour in 5 of about 25 rows it opened, for +752 B. Expect defects, not bytes.
2. **[A] Wrong-callee rows: 33 rows / 2,812 B (2,228 B native-compiled).** 8 in scope (288 B) and 25 in VIA-DC3
   (2,524 B). The charged callee's retail body differs from the one we call (chase REFUTED_BODIES_DIFFER) or we have no
   body (OURS_NO_BODY).
   - Examples: `PatchSticker::MakeLoader` casts to `FileLoader` where retail's RTTI operand is `Loader` (92 B); two
     `RndMeshAnim` `_M_insert_overflow_aux` rows copy `Key<vector<Vector2>>` where retail copies
     `Key<vector<Vector3>>` (324 B each); `CharHair::CharHair` references another class's vbtable (476 B).
   - Some will be folds the chase cannot prove. The ones that are wrong instantiations are type errors native
     executes. W16-QJ fixed seven of this kind.
3. **[A] The 4 in-scope no-record source rows: 2,060 B, all native-compiled** (§4.2). Effectively
   `SuperFormatString::SuperFormatString` (1,608, 94.33) and three template rows. Small, and the last unattempted
   in-scope surface.
4. **[B] The same read as lever 1 on VIA-DC3 files native does not compile: 13,624 B** of the 82 rows.
5. **[B] Linking the 96 in-scope files native does not compile: 97,340 B of the in-scope gap** (§4.3). This is the
   native lanes' work, not the matching lanes', and it is how the remaining in-scope behaviour becomes observable.
   W16-PX showed what it buys: 37 files linked, one host crash and three missing bodies found by W16-QC's follow-up.
6. **[C] Identification residue: 218 rows / 15,604 B** (in scope 120 / 8,476, VIA-DC3 98 / 7,128). W16-QI left 25
   in-scope rows / 480 B unattributed with reasons (TU5-only AuditionMgr, an unnamed Joypad ObjPtr, statics our
   source lacks). This is bookkeeping unless a new channel appears; W16-QP measured the global body matcher at 0 of 3
   on the target.
7. **[C] Name settlements: 28 rows / 5,476 B proven or installable** (in scope 13 / 2,540, VIA-DC3 15 / 2,936), then
   W16-PH's 11 undecidable pairs (3,664 B), VIA-DC3's 9 cycle-assumed survivors (2,060 B) and 12 type-witness rows
   (1,176 B). The proven ones need W16-PH's two-channel install. The rest need a new witness rule with its own
   controls.
8. **[C] Tooling.**
   - dtk drops relocation addends on split target objs (W16-PW). That is 1 in-scope row (448 B) plus 2 OUT rows
     (1,620 B) whose last charge is the artifact. It also misleads any reader of retail through relocations.
   - `ab_measure` refuses pins that make `symbols.txt` merge (W16-QP, QZ, QI, RC). Each of those lanes hand-ran the
     measurement. W16-QI lists 6 carve rows blocked on it.

**Not ranked, for the user to decide (unchanged):** the permuter pilot. In scope, buckets 20, 23, 11 and 22 hold
160,440 B of codegen residue, and 227 rows are at `mpn` 100. Several single-instruction prizes are recorded:
`App::App` (1,864 B, one commutative `add`, W16-QV), `SaveLoadManager::SetState` (4,096 B, "same interference graph,
different colour choice", W16-RB) and `ParseNode` (2,432 B, one `add` swap, W16-RD). In VIA-DC3 the
register/reorder bucket adds 81,712 B.

**Out of ranking by directive:** Quazal 113,520 B, XDK 2,948 B, 360-only 52,200 B, NET 4,368 B.

### 6.1 Earlier levers, now drained

| 10-06 lever | taken by | result | state |
|---|---|---|---|
| 1. never-worked source divergence, 18 rows / 7,068 B | W16-PO | 9 to 100 (+9 fns / +1,880 B), 2 raised; 3 behaviour defects; 9 rows / 5,188 B left with stop reasons | **drained** |
| 2. behaviour read, native-compiled hard core, 49,784 B | W16-PQ | 64 rows read: 57 EQUAL, 6 FIXED (8 defects), 1 deliberate; Δ0 B, as priced | **drained** |
| 3. behaviour read, non-native hard core, 75,112 B | W16-PR | 78 rows: 10 defects in 7 rows; +2 fns / +624 B (≈1%, as priced) | **drained** |
| 4. W16-PF's lever on the 14 unreached register rows | W16-PS | 4 fixed; 10 stopped, inert or STLport | **drained** |
| 5. `.bss` `= 0` sweep | W16-PS lever 5, W16-PY | 3 fixed (W16-PS), 3 VIA-DC3 rows to 100 (W16-PY). PY's census: "in scope, the lever is drained"; `= 0` is not universal (LightPreset) | **drained** |
| 6. pairwise site sweeps on W16-PF's stopped rows | W16-PS lever 6 | 157 pairs over 11 rows, 0 better | **drained** for sensitive pairs; insensitive pairs and triples untested, which is the permuter's market |
| 7. name settlements (PH undecidable, type witness) | none | — | open, §6 lever 7 |
| 8. identification residue, 225 rows / 17,744 B | W16-QH, QI, QM, RI, RC, QG | 120 rows / 8,476 B left in scope | **mostly drained**; remainder carries per-row reasons |
| not ranked: VIA-DC3 ring | user funded it; W16-PU, QA, QF, QJ, PY | +34,604 B; string scan of the whole ring finds 0 wrong literals (W16-PU, QF) | open, §6 levers 1, 2, 4 |

Levers this round opened and closed in the same window:
- W16-QO/QQ's frame-size census: 49 in-scope rows found, 42 worked.
- W16-QA's DC3-newer body sweep of src/system outside rndobj/char: 967 rows, 29 to 100.
- W16-QH/QI's static-init and static-dtor regions: +945 fns. 25 in-scope rows / 480 B left unattributed with reasons.
- W16-QM's single-owner `auto_*` attribution: fixed point in one round. W16-QP: 0.68% of the `auto_*`-only components
  is held source.
- W16-RC's dtk mis-carves: done in scope; the 127 remaining hits are XDK or `auto_*` and move no metric.
- W16-PW's constant-value audit: whole binary, 28 values fixed, 99.3% of 27,998 pairs agree.
- W16-PZ and W16-QD's class-layout checks: no member wrong in the bandobj/band3 classes native links; layout ODR
  splits fixed and gated in the native gate.
- The row lanes W16-QK, QX, QZ, RB, RD, RF, RK, RQ and RV each left a stop reason per row; their residue is buckets
  23 and 24.

## 7. Reading the numbers

- In-scope match share is **94.56%** (10-06: 93.10%). The whole-binary share of the ceiling is **91.00%**. Since
  10-06 the in-scope gap fell **54,480 B (−20.7%)**, against +122,020 B whole-binary. 52,768 B of that left by
  reaching 100 and 5,384 B by leaving the denominator.
- **In scope, nothing is unattempted except 452 B.** 76.7% of the gap is codegen residue on rows someone has read or
  levered, and 8.2% is source-class rows whose stop reasons are also mostly codegen. What separates the remaining
  in-scope bytes from 100 is the permuter's market, and that is the user's call.
- **The behaviour-class work left is in VIA-DC3**: 82 source-class rows / 32,476 B no lane has opened, and 33
  wrong-callee rows. For native behaviour that is now the richer ring.
- **53.5% of the in-scope gap is in files native compiles today.** The other 46.5% becomes native-relevant only as
  native links more band3/bandobj files.
- **The ceiling moved +23,516 B**, all from code that left `auto_*` for named units. Pinning over `auto_*` raises
  the ceiling by exactly the bytes it moves into pairable units. This is the same mechanism as the 10-02/03 jump, at
  1/12 the size.

## 8. Not done

- **No source, map, splits, alias or `symbols.txt` edits.** Nothing was A/B-measured, because this branch changes no
  code. The native gate was not run (no `src/` change).
- **The overlays are coarse:**
  - "Named in a later lane's doc" is a doc mention, not a per-row verdict. Bare-name keying can also match a free
    function another doc names in passing.
  - W16-PU's population is its 438-row file, and "named by no lane since 10-06" is the instrument for what it did
    not open, not its own list.
  - The native-compiled column is file-level. A row in a natively compiled file may still be dead code on every
    native run; `tools/native_runtime_rank.py` (W16-PK) is the instrument for that and was not rerun.
- **W16-QA's 967-row pool covers ANON rows it says nothing about**, so only its body-compared classes count as a
  record.
- I did not check whether the 33 wrong-callee rows are folds or wrong instantiations. That is §6 lever 2's job.

Scripts and JSON are in `~/tmp/w16sd-gap/` and are not committed. Reproduce in this order, from that directory, with
`W=~/tmp/wt-w16sd` built at `24d250690`:

```
python3 gengap.py $W gap.json                      # 1,875 gap rows, asserts ceiling - matched
python3 scope_ledger2.py $W ledger2.json           # ring ledger, asserts total_code and matched_code
python3 diffall.py $W gap.json cls.json            # one objdiff diff per gap row, W16-NA classifier
python3 unpaired.py $W                             # U2/U3/U4 split; 200/200 index control
python3 overlay3.py                                # pools, mentions (now Q*/R* docs + bare names), 10-06 dispositions
python3 insdel_shape.py $W                         # opset shapes
python3 tables3.py > tables3.txt                   # klass(), rings, final.json
python3 mkcensus.py && python3 pairs.py            # rows with a symbol-kind charge, their name pairs
python3 chase_all.py                               # icf_pair_adjudicate chase per pair
python3 namecls.py && python3 final3.py            # name verdicts; placeholder-only control (none == graded)
python3 tables4.py > tables4.txt                   # in-scope partition, class blocks, dirs
python3 dispo3.py > dispo3.txt                     # §4 table (asserts)
python3 dispo_via.py > dispo_via.txt               # §5 table (asserts)
python3 cmp1006.py > movement.log                  # §2 rings vs 10-06, row movement (asserts)
python3 leftwhere.py                               # where the left rows went
python3 claims.py                                  # §1 attribution
python3 lane_rings.py                              # §3, from walk/rep_<commit>.json.gz
```

- 10-06's pipeline, copied and repointed: `gengap`, `scope_ledger2`, `diffall`, `unpaired`, `overlay3` (extended),
  `insdel_shape`, `tables3`, `mkcensus`, `pairs`, `chase_all`, `namecls`, `final3`, `tables4`.
- New in this edition:
  - `dispo3.py`, `dispo_via.py`: §4 and §5.
  - `cmp1006.py`, `leftwhere.py`: ring deltas and row movement.
  - `claims.py`: the attribution.
  - `walk/walk.sh`, `walk/walk2.sh`, `lane_rings.py`: the per-lane walk.
  - `native_src.json`: 451 files from main's native `build.ninja`.
  - `stubs.json`: the stub census.
- Per-row tables: `dispo_rows.json` (in scope) and `dispo_via_rows.json` (VIA-DC3).
