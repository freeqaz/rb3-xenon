# Campaign state — 2026-10-03 (lane W16-OV)

> **SUPERSEDED** for numbers and priorities by [CAMPAIGN_STATE_2026-10-03b.md](CAMPAIGN_STATE_2026-10-03b.md) (main `83e92ed07`: in-scope gap 295,552 B, 88.96% of the ceiling). Kept as the method record; 10-03b reruns this method unchanged.

Sixth edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-02.md` for numbers and
priorities. **New in this edition: the ranking is scoped to the native port**, per the standing user directive
(memory `feedback_scope_native_port_2026-07-24`): XDK is hard-skipped, Quazal is low value and not to be funded,
and the in-scope pool is the band3 game layer plus the system pieces the native runtime uses. The 10-02 row-by-row
method is reused unchanged and applied per scope ring. Quazal bytes landed since 10-02 are stated separately (§3).
No source, map, splits, alias or `symbols.txt` edit on this branch.

## 1. Measured

Full `./tools/ninja-locked` in a fresh `scripts/setup_worktree.sh` worktree at main **`99ee35830`** (W16-OU merged),
`BUILD_RC=0`, last edge `[patch-state] OK: tree is a fixed point of 6 post-compile passes`. Ruler `name_check`
(objdiff 4.2.9, tool commit `a5f0ea903ec1`, binary hash `5a51cd51fe0a353f`, read from `report.json` `provenance`).
Ceiling from `python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 99ee35830 (2026-10-03)"`:

```
  scaffold thr<=4 : 0 units · thr<=5 : 0 · thr<=6 : 57 units, 290 rows, 57,664 B · thr<=7 : 57 (cliff, not fitted)
total_code            10,247,792
PAIRABLE               6,680,212 = 65.187%   (1,055 units, 55,539 rows)
− scaffold shells         57,664             (57 units, 290 rows)
= reachable ceiling    6,622,548 = 64.624%
matched_code           5,880,692 = 57.385% of total_code = 88.80% of ceiling
gap to ceiling           741,856
matched_functions         53,269 / 68,913   masked_equal 25,144   honest 28,125
fuzzy_match_percent       63.571
```

`matched_code` and `matched_functions` equal W16-OU's own leg A (5,880,692 B, 53,269 fns) exactly.

**The ceiling moved +2.879 pp (61.745% → 64.624%, +295,288 B), and that move is almost entirely Quazal.**
Today's Quazal lanes (W16-NY/OC/OD/OE/OF/OG) wrote real bodies into TUs that were 7-line map scaffolds, so
the scaffold set shrank from 103 to 57 units (−121,000 B). Quazal's `total_code` also grew 178,872 B as
previously-`auto_*` code was pinned to its TUs (UNKNOWN-anon fell 173,864 B over the same span). I did not predict this. I expected the ceiling to stay within ±0.1 pp, as it had for a
month (`project_ceiling_raising_is_futile_2026-08-17`). That memory is about *raising* the ceiling as a
lever. Today the ceiling rose as a side effect of funded Quazal work. **The whole-binary share of the ceiling (88.36% → 88.80%) therefore
understates progress outside Quazal, and the gap *grew* by 5,576 B while the non-Quazal gap fell 71,456 B** (§3).

**Reproduction control on the 10-02 numbers.**
- *Method:* I rebuilt main `2b066acc8` in a second worktree. `--cold-cache` still copied main's
  dtk `config.json`, so I deleted `build/45410914/{config.json,obj,asm}` and re-split from the base's own
  `splits.txt`. The split-guard fired once, with "split with: None" because the worktree had no recorded hash.
  `git diff` was empty, and the retry was a fixed point.
- *Prediction:* the 10-02 figures exactly.
- *Measured:* ceiling **6,327,260 = 6,327,260** and scaffold 103 units / 178,664 B, both exact. `matched_code`
  read **5,590,980 vs 5,590,840 (+140 B)** and the gap 2,673 rows vs 2,674. The one differing row is
  `fill<_Bit_iter>` in `ChordShapeGenerator`: 140 B, 99.714 recorded on 10-02, 100 on the rebuild, same objdiff binary.
- *Use:* every "since 10-02" delta below is taken against the rebuild, so both legs share one method.

## 2. Scope rings and native-scope coverage

### 2.1 `scripts/native_scope_map.py` (run unchanged on both trees)

| class | 10-02 bytes (strict %) | 10-03 bytes (strict %) | 10-03 fns (strict %) |
|---|---:|---:|---:|
| NATIVE-CORE (obj/utl/os/math + `rndobj/Anim`) | 479,452 (85.57%) | 477,388 (**86.99%**) | 3,978 (94.47%) |
| NATIVE-SOON (band3, beatmatch, midi, track, hamobj, meta_ham) | 2,421,576 (93.24%) | 2,425,000 (**93.89%**) | 21,651 (98.46%) |
| NATIVE-VIA-DC3 | 2,950,008 (86.71%) | 2,943,940 (88.17%) | 24,421 (96.36%) |
| 360-ONLY (incl. all `src/network`, XDK) | 651,876 (55.85%) | 830,872 (71.25%) | 5,471 (85.01%) |
| UNKNOWN-anon | 3,741,444 (0%) | 3,567,580 (0%) | 13,374 (0%) |

**Headline (the tool's own line): native scope (CORE+SOON) 97.4% fns / 92.0% bytes on 10-02 → 97.8% fns /
92.8% bytes on 10-03**, against 77.3% fns / 57.4% bytes whole-binary. "Strict" is the tool's unit `matched_code /
total_code`. CORE and SOON have no scaffold and no unpairable units, so it equals the reachable share there. The
ledger in §2.3 reproduces 92.76% independently.

### 2.2 Correction to the tool's scope: ~95 KB of "VIA-DC3" has no DC3 counterpart

`native_scope_map.py` files `src/system/bandobj/` under NATIVE-VIA-DC3, but **`../dc3-decomp/src/system/` has no
`bandobj` at all.** DC3 cannot supply RB3's band objects (BandTrack, BandDirector, BandCharacter, BandWardrobe,
VocalTrackDir, ChordShapeGenerator, …).

I checked every VIA-DC3 gap row's source file against `../dc3-decomp` (case-insensitive path match). The rows
with no counterpart are 94,804 B of gap:

| dir | gap with no DC3 file | gap with a DC3 file |
|---|---:|---:|
| bandobj | 83,468 | 0 |
| synth (`EQEffect`, `VoiceBeat`, `Flanger`/`Wah`/`Delay`/`Distortion` effects) | 6,096 | 18,636 |
| dsp (`PitchDetector`, `SndAnalysis`, `VibratoDetector`) | 3,784 | 0 |
| ui (`UIProxy`, `UIGridProvider`) | 724 | 11,600 |
| char (`CharMeshCacheMgr`) / world (`EventAnim`) | 732 | 100,396 |
| rndobj, meta, movie, flow | 0 | 122,372 |

`VoiceBeat`, `VibratoDetector` and the pitch detection are on the vocal path that the native M10 milestone
already runs. This lane treats these rows as a third in-scope ring, **IN-RB3ENG**. It did not edit the tool,
because this lane changes nothing. The tool's VIA-DC3 rule should test for a DC3 counterpart, not just the directory.

### 2.3 Scope ledger (both trees, same method)

Tier = `native_scope_map.classify()` imported verbatim. Two refinements: VIA-DC3 is split by DC3-counterpart (§2.2),
and 360-ONLY is split into QUAZAL (`src/network/*` except `net`), NET (`src/network/net`, RB3's net layer over
Quazal, the "net panels" the directive also calls low value), XDK and 360-OTHER (rnddx9, synth_xbox, codecs,
`*_Xbox`, keygen). Scaffold = `ceiling_recompute.coff_symcount ≤ 6`, imported. The script asserts that its rows sum
to `total_code` and its fuzzy==100 rows sum to `matched_code`, on both trees.

| tier | reachable | matched | **gap 10-03** | share | gap 10-02 | Δ matched since 10-02 | Δ matched fns |
|---|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 477,388 | 415,268 | **62,120** | 86.99% | 69,184 | +5,000 | +24 |
| IN-SOON | 2,425,000 | 2,276,944 | **148,056** | 93.89% | 163,616 | +18,984 | +102 |
| IN-RB3ENG | 916,324 | 821,520 | **94,804** | 89.65% | 109,600 | +20,100 | +102 |
| **in scope (3 rings)** | **3,818,712** | **3,513,732** | **304,980** | **92.01%** | 342,400 | **+44,084** | **+228** |
| VIA-DC3 (DC3 has the file) | 2,027,176 | 1,774,172 | 253,004 | 87.52% | 282,104 | +17,728 | +72 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 36,488 | **+222,840** | **+1,582** |
| OUT-NET | 76,864 | 70,896 | 5,968 | 92.24% | 6,008 | +88 | +1 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 |
| OUT-360-OTHER | 349,084 | 289,868 | 59,216 | 83.04% | 64,112 | +4,972 | +10 |
| UNKNOWN-other (`App`, root) | 3,012 | 792 | 2,220 | 26.29% | 2,220 | 0 | 0 |
| **total** | 6,622,548 | 5,880,692 | 741,856 | 88.80% | 736,280 | +289,712 | +1,893 |

(UNKNOWN-anon is unpairable, 0 reachable B, so it is not a row here: 3,567,580 B of `total_code`, down from 3,741,444.)

## 3. Quazal, stated separately

Since 10-02 (rebuilt `2b066acc8` → `99ee35830`), measured by the ledger above:

- **Quazal matched_code +222,840 B and matched functions +1,582. That is 76.9% of all matched bytes (+289,712) and
  83.6% of all matched functions (+1,893) gained since 10-02.** The memory note's "+1,558 fns / +218,416 B" was a
  partial count taken mid-run; this figure is the whole span, keyed on source path.
- Quazal's reachable bytes grew 44,480 → 344,352 (+299,872): scaffold shells became real objs (−121,000 B of
  scaffold) and `auto_*` code was pinned to Quazal TUs (+178,872 B of Quazal `total_code`). Its gap therefore
  **grew** 36,488 → 113,520 B.
- Everything outside Quazal: gap 699,792 → **628,336 B (−71,456)**, matched +66,872 B.
- The in-scope rings gained +44,084 B (15.2% of today's matched bytes).

The directive was violated by the coordinator on 10-02/03 and that is recorded in the memory note. Whether to keep
the landed Quazal work is the user's call. **Nothing below funds Quazal.** Its 113,520 B gap is reported, not ranked.

## 4. Partition of the in-scope gap

### 4.1 Method (10-02's, reused)

1. **Population:** every reachable row with `fuzzy_match_percent < 100`. `~/tmp/w16ov/gengap.py` imports
   `ceiling_recompute`'s scaffold rule. Ring per row = §2.3's tier of its unit source path.
2. **Charge class:** every one of the 2,614 gap rows is diffed once with `objdiff-cli diff` under the project config
   (graded ruler). The scripts are **W16-NP's `diffall.py`, `unpaired.py`, `insdel_shape.py` and `tables.py klass()`,
   copied unchanged**: W16-NA's classifier, W16-NH's SCHED rule, the I1–I4 opcode-multiset split, and the
   U1–U4 COFF name index.
3. **One measured correction (new):** an N2 row whose every symbol pair has a **placeholder retail target**
   (`fn_`/`lbl_`/…) is moved to P1. `name_check` forgives placeholder targets. I diffed all 33 such rows (whole
   binary) again at `functionRelocDiffs=none`, and **33/33 read the same fuzzy as graded**, so their symbol args are
   uncharged and they are register-only rows. W16-NA's classifier counts any differing symbol arg, so 10-02's N2
   (40 rows / 20,948 B) carried this artifact too. Whole binary: 33 rows / 17,256 B. In scope: 16 rows / 11,864 B.
4. **Relocation names:** W16-NH's `pairs.py` + `chase_all.py`, rerun on this tree over the 1,059 gap rows with a
   symbol arg (945 distinct pairs). A row takes its **worst non-placeholder pair**. 10-02's bucketing code was not
   saved, so the buckets are re-implemented from its doc. Order from easiest to hardest: PROVEN free <
   PROVEN conflict (our spelling withdrawn, in another group, or map-resident) < PROVEN with a CYCLE-ASSUMED leaf
   (free, then conflicted) < REFUTED, undecidable (vacuous/template-twin) < REFUTED, bodies differ (BYTES-DIFFER,
   RELOC-COUNT/SHAPE, MISSING) < survivor outside every pinned span < our spelling in no compiled obj.
5. **Overlay (coarser than 10-02's, labelled):** "opened" = a W16-NP per-row adjudication carried forward by exact
   (unit, name), **or** the row's short name in backticks anywhere in a W16-N*/W16-O* doc. Pool membership = exact
   name in a sweep population on disk (W16-NA/NH/NL, W16-NB's rule, W16-OK `cls_base`, W16-OL `cls`, W16-OO `pop`,
   W16-OQ `pop`). Today's O-lanes swept by directory, so nearly every in-scope row was in some pool. "No per-row
   record" does **not** mean unopened, only that nobody wrote the row down.

Scripts and JSON: `~/tmp/w16ov/` (`gengap.py`, `scope_ledger.py`, `scope_ledger2.py`, `diffall.py`, `unpaired.py`,
`overlay3.py`, `insdel_shape.py`, `tables3.py`, `tables4.py`, `pairs.py`, `chase_all.py`, `namecls.py`,
`blocks_in3.py`; per-row table `final4.json`). Not committed.

### 4.2 Controls

| control | result |
|---|---|
| population sums | reachable rows sum to the ceiling **6,622,548 = 6,622,548**. Their fuzzy==100 rows sum to `matched_code` **5,880,692 = 5,880,692**. Gap = ceiling − matched exactly. The scope ledger asserts the same on both trees. |
| diff vs grader | graded fuzzy from `objdiff-cli diff` = `report.json`'s on **1,890 / 1,890** paired rows, 0 disagreements. All 724 UNPAIRED rows read fuzzy 0 in the report. **0** diff errors. |
| COFF name index | finds **200 / 200** sampled paired names in their own base obj |
| placeholder correction | 33/33 placeholder-only N2 rows read identical fuzzy at `none` and graded (the correction can fail; it did not) |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail", rc 0 |
| two independent coverage reads | ledger CORE+SOON 92.76% vs `native_scope_map` 92.8%. In-scope class rows sum to 304,980 = ledger. Class deltas vs 10-02 sum to −37,560, which is the ledger's −37,420 minus the 140 B reproduction residual (§1). |
| 10-02 reproduction | ceiling exact; matched_code +140 B / 1 row (§1) |

### 4.3 The whole gap by class × ring

| id | IN-CORE | IN-SOON | IN-RB3ENG | VIA-DC3 | OUT-QUAZAL | OUT-NET | OUT-XDK | OUT-360-OTHER | UNKNOWN | total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P1 | 7,984 | 50,612 | 11,020 | 40,860 | 17,960 | 912 | 0 | 4,792 | 0 | 134,140 |
| P2 | 812 | 7,268 | 6,236 | 9,240 | 0 | 544 | 0 | 2,656 | 0 | 26,756 |
| I1 | 4,316 | 4,108 | 12,512 | 18,820 | 2,132 | 0 | 0 | 3,416 | 0 | 45,304 |
| I2 | 1,832 | 2,708 | 7,556 | 9,804 | 0 | 0 | 0 | 3,156 | 0 | 25,056 |
| I3 | 11,244 | 28,420 | 15,284 | 50,708 | 10,256 | 1,028 | 0 | 8,684 | 0 | 125,624 |
| I4 | 12,428 | 20,972 | 20,052 | 53,860 | 30,156 | 1,088 | 0 | 12,820 | 0 | 151,376 |
| M1 | 5,400 | 4,652 | 7,368 | 28,688 | 10,832 | 684 | 0 | 5,992 | 80 | 63,696 |
| M2 | 5,152 | 1,456 | 3,252 | 4,480 | 3,848 | 0 | 0 | 3,560 | 0 | 21,748 |
| N1 | 3,480 | 16,196 | 5,164 | 17,104 | 18,500 | 120 | 144 | 2,088 | 0 | 62,796 |
| N2 | 1,504 | 0 | 92 | 224 | 4,348 | 0 | 0 | 2,372 | 0 | 8,540 |
| U1 | 0 | 0 | 0 | 0 | 14,888 | 0 | 0 | 0 | 0 | 14,888 |
| U2 | 6,872 | 10,168 | 5,844 | 17,392 | 0 | 1,516 | 88 | 2,992 | 2,064 | 46,936 |
| U3 | 112 | 152 | 192 | 1,004 | 120 | 0 | 0 | 116 | 0 | 1,696 |
| U4 | 636 | 528 | 232 | 820 | 480 | 76 | 2,716 | 6,572 | 0 | 12,060 |
| S0 | 348 | 816 | 0 | 0 | 0 | 0 | 0 | 0 | 76 | 1,240 |
| **all** | **62,120** | **148,056** | **94,804** | **253,004** | **113,520** | **5,968** | **2,948** | **59,216** | **2,220** | **741,856** |
| rows | 268 | 483 | 301 | 779 | 535 | 48 | 13 | 178 | 9 | 2,614 |

Class ids as in 10-02 §2.3: P1 register/stack-slot only · P2 pure reschedule · I1 insert/delete with equal opcode
multisets · I2 only register moves differ · I3 1–4 opcodes differ · I4 more · M1 immediate · M2 opcode replace /
branch dest / other · N1 relocation name only · N2 name + register · U1 anonymous unpaired Quazal · U2 anonymous
unpaired, everything else · U3 named, defined in another obj · U4 named, defined in no obj · S0 TU5 image patch.
P1/N2 include the §4.1(3) correction.

### 4.4 The in-scope table (IN-CORE + IN-SOON + IN-RB3ENG: 1,052 rows / 304,980 B)

"At mpn 100" means the row already counts in `matched_functions`, so closing it buys bytes only. "Opened" uses
the coarse overlay of §4.1(5).

| id | rows | bytes | % of in-scope gap | at mpn 100 | anon rows / B | opened rows / B | no per-row record rows / B |
|---|---:|---:|---:|---:|---:|---:|---:|
| **P1** | 109 | 69,616 | 22.8% | 103 | 1 / 32 | 90 / 56,908 | 19 / 12,708 |
| **P2** | 16 | 14,316 | 4.7% | 0 | 0 / 0 | 14 / 13,608 | 2 / 708 |
| **I1** | 32 | 20,936 | 6.9% | 0 | 0 / 0 | 5 / 7,696 | 27 / 13,240 |
| **I2** | 27 | 12,096 | 4.0% | 0 | 0 / 0 | 12 / 9,124 | 15 / 2,972 |
| **I3** | 103 | 54,948 | 18.0% | 0 | 2 / 84 | 37 / 33,584 | 66 / 21,364 |
| **I4** | 84 | 53,452 | 17.5% | 0 | 7 / 340 | 20 / 31,416 | 64 / 22,036 |
| **M1** | 103 | 17,420 | 5.7% | 0 | 77 / 3,180 | 4 / 5,100 | 99 / 12,320 |
| **M2** | 24 | 9,860 | 3.2% | 3 | 6 / 168 | 4 / 4,576 | 20 / 5,284 |
| **N1** | 234 | 24,840 | 8.1% | 150 | 150 / 5,904 | 3 / 2,624 | 231 / 22,216 |
| **N2** | 2 | 1,596 | 0.5% | 0 | 0 / 0 | 1 / 92 | 1 / 1,504 |
| **U2** | 287 | 22,884 | 7.5% | 0 | 287 / 22,884 | 0 / 0 | 287 / 22,884 |
| **U3** | 10 | 456 | 0.1% | 0 | 0 / 0 | 0 / 0 | 10 / 456 |
| **U4** | 17 | 1,396 | 0.5% | 0 | 0 / 0 | 0 / 0 | 17 / 1,396 |
| **S0** | 4 | 1,164 | 0.4% | 0 | 0 / 0 | 0 / 0 | 4 / 1,164 |
| **total** | **1,052** | **304,980** | 100% | 256 | | | |

**Rolled up:**

| block | classes | rows | bytes | % of in-scope gap |
|---|---|---:|---:|---:|
| register allocation / scheduling only (the permuter's market) | P1 + P2 | 125 | 83,932 | **27.5%** |
| reorder-shaped insert/delete | I1 + I2 | 59 | 33,032 | 10.8% |
| source divergence | I3 + I4 + M1 + M2 | 314 | 135,680 | **44.5%** |
| relocation name | N1 + N2 | 236 | 26,436 | 8.7% |
| unpaired (identification, missing body, home) | U2–U4 | 314 | 24,736 | 8.1% |
| structurally unmatchable (TU5 image) | S0 | 4 | 1,164 | 0.4% |

**Change since 10-02, in scope** (10-02's own `final.json` rows, same verbatim `klass()`, before the §4.1(3)
correction): I3 −8,368 · I4 −8,248 · N1 −11,432 · U2 −2,912 · I1 −2,096 · P1 −1,188 · U3 −1,076 · U4 −820 ·
N2 −612 · M2 −424 · P2 −204 · I2 −120 · M1 −60 · S0 0. **Total 342,540 → 304,980 (−37,560).**
- N1's drop is today's chase/alias work (W16-OM learned RTTI-named vtable slots and the dead `blr`; W16-OS/OU re-chased survivors).
- I3/I4's drop is the O-lanes' source fixes.
- P1/P2 barely moved, as expected with the permuter deferred.

**Sub-splits:**

- **By directory:**

  | dir | rows | bytes |
  |---|---:|---:|
  | system/bandobj | 280 | 83,468 |
  | band3/meta_band | 172 | 53,608 |
  | band3/game | 96 | 29,908 |
  | band3/bandtrack | 44 | 26,844 |
  | system/beatmatch | 57 | 18,156 |
  | system/obj | 61 | 16,860 |
  | system/os | 75 | 16,840 |
  | system/utl | 95 | 16,448 |
  | system/math | 34 | 11,440 |
  | band3/net_band | 36 | 8,848 |
  | system/synth (RB3-only) | 8 | 6,096 |
  | system/track | 24 | 5,428 |
  | system/dsp | 5 | 3,784 |
  | system/midi | 37 | 3,164 |
  | band3/tour | 15 | 1,996 |
  | other | 13 | 2,092 |

- **Concentration:**
  - The top 10 rows hold 14.0% of the in-scope gap, the top 50 hold 36.9%, the top 100 hold 53.2% and the top 500 hold 92.0%.
  - The median row is 92 B.
  - The largest row is `VocalTrack::UpdateScrolling`: 8,948 B, I4, 96.97. W16-NL, W16-NQ and W16-OK each opened it and left it.
- **Fuzzy band:**
  - P1 is 66,092 B at 99–99.99, M1 17,224 B and N1 24,564 B.
  - I4 is the only class with real mass below 90: 23,412 B.
  - U2/U3/U4 read 0 by construction.
- **EH funclets:** 150 of N1's rows (5,904 B) and 77 of M1's (3,180 B) are anonymous funclets paired by byte signature.
  Their charges follow the parent function, so they are not separate work items. N1's 150 at mpn 100 are exactly those funclets.
- **U2 in scope:**
  - 287 anonymous rows / 22,884 B. 71 of them are ≤ 16 B (824 B).
  - By directory: bandobj 68, meta_band 36, utl 32, game 32, obj 23, os 23, midi 23, net_band 22.
  - W16-NF/NK already worked this class: whether a row is a mis-carve or unidentified is not mechanically separable.

**Relocation-name rows in scope, by worst real pair** (N1 + N2 = 236 rows / 26,436 B):

| verdict | rows | bytes | what it means |
|---|---:|---:|---|
| PROVEN, our spelling free | 14 | 888 | alias installable; tiny, the free lever stays drained |
| PROVEN, our spelling conflicted (withdrawn / other group / map-resident) | 7 | 528 | survivor conflict |
| PROVEN with a CYCLE-ASSUMED leaf, free | 47 | 9,800 | needs W16-JE's two-channel witness |
| PROVEN with a CYCLE-ASSUMED leaf, conflicted | 18 | 7,056 | as above, plus the conflict |
| REFUTED, undecidable (vacuous body, template twins) | 94 | 4,732 | needs a type witness |
| REFUTED, bodies differ | 47 | 1,552 | **a wrong callee or wrong map name: a defect** |
| our spelling in no compiled obj | 9 | 1,880 | the name we call has no body |

The CYCLE-ASSUMED mass concentrates on four retail survivors, sized across **all** rings because one adjudication
decides every row that leans on it:

| retail survivor | our spellings that resolve to it | rows (whole binary) | bytes (whole binary) | in scope |
|---|---|---:|---:|---:|
| `vector<Hmx::Object*>::_M_fill_insert` | 10 pointer-element `_M_fill_insert` instantiations | 30 | 18,788 | 6,760 |
| `sort<RndPollable**>` | `sort<RndDrawable**>`, `sort<VocalPart**>` | 5 | 4,008 | 3,284 |
| `vector<MidiParser::Note>::_M_fill_insert` | `GemInProgress`, `RGTrill`, … | 5 | 3,832 | 2,440 |
| `_Rb_tree<Symbol>::clear` | `_Rb_tree<unsigned short>`, `<ScoreType>`, … | 7 | 1,328 | 1,216 |

The first survivor is the "pointer-element `_M_fill_insert` family" that W16-OO §6 and W16-JE/W16-OK parked on purpose.
A row's bytes collect only if **all** of its pairs clear, so these are upper bounds.

The "no body" bucket's largest member is **`HDCache::Init` (1,504 B) plus one sibling (220 B)**. Both call
`HDCache::Flush`, which no compiled obj defines. Retail's call lands on an `StlNodeAlloc<_List_node<int>>`
converting-ctor body, which looks like an ICF fold of an empty `Flush`. Not verified. The other "no body" rows are
`??_G` (scalar deleting dtor) in retail against our `??_E` (vector deleting dtor), five of them, 8–16 B each.

## 5. What today's in-scope levers returned (for pricing)

Whole-binary `ab_measure` results, graded ruler. Pool = the lane's own starting population (from its doc).

| lane | lever | pool | Δ matched_code | yield |
|---|---|---:|---:|---:|
| W16-NS | P1/P2 rows with no record, by hand | 16,040 B | +4,572 | 28.5% |
| W16-NQ | named insert/delete tail, 90–99.99 | 223,264 B | +16,200 | 7.3% |
| W16-OK | band3 sub-100, largest first | 127,500 B | +6,688 | 5.2% |
| W16-OQ | char/world sub-100 + W16-OO map leftovers | 97,012 B | +3,436 | 3.5% |
| W16-OL | bandobj/char/world/beatmatch sub-100 | 208,276 B | +4,320 | 2.1% |
| W16-OO | os/utl/obj/meta/ui/math/synth/midi/flow/track sub-100 | 125,144 B | +392 | **0.3%** |
| W16-OP / W16-OR | class layout + vtable audits | — | +216 / +2,240 | real defects: 7 + 3 |

**The directory-sweep source lever is close to exhausted in scope.** Yields fell from 5.2% to 0.3% across one day
of sweeps, and the one cheap hand slice (W16-NS, 28.5%) came from a population that the classifier had separated
out. In-scope rows whose one-sided blocks were never scanned (W16-OL's `blocks.py` rule, rerun unchanged here:
fuzzy > 30, ≥ 3 insert/delete, a ≥ 3-instruction run only one side has): **11 rows / 17,620 B**, 8,948 B of them
`VocalTrack::UpdateScrolling`. 59 more rows / 32,692 B were already scanned by W16-OL/OO.

## 6. Next in-scope levers, ranked

Out of ranking by directive: Quazal (113,520 B gap), XDK (2,948 B), the rest of 360-only (59,216 B), and NET
(5,968 B). VIA-DC3 with a DC3 counterpart (253,004 B) is the second ring. DC3 supplies that code to the native
build, so it is not ranked here.

1. **Register-only rows hidden as name rows: 15 in-scope rows / 11,688 B with no record** (33 rows / 17,256 B whole
   binary, §4.1(3)).
   - Largest: `SaveLoadManager::SetState` 4,096 B · `RGGetChordName` 1,652 · `TrackDir::DrawShowing` 1,252 ·
     `AssetMgr::GetTypeFromName` 692 · `Tour::OnMsg` 596 · `MemTracker::DiffDump` 500 · `MidiParser::PushIdle` 484 ·
     `VocalPart::GetNoteSliceWeight` 484.
   - All are at mpn 100, so the payout is bytes.
   - No register sweep ever saw these rows, because W16-NA's classifier filed them as N2.
   - Hand-sweep them with W16-NA/NS's lever list. Measured yield on that class is 18.6% (NA+ND) to 28.5% (NS).
   - Fix the classifier first: drop placeholder-target symbol args, then re-derive P1. That change is also owed to
     10-02's numbers.
2. **Name-class adjudication in scope, decidable on bytes: about 20 KB.**
   - (a) One two-channel witness (W16-JE) for `vector<Hmx::Object*>::_M_fill_insert`: up to 18,788 B whole binary,
     6,760 B in scope. Then the next three survivors in the §4.4 table.
   - (b) The 47 REFUTED-bodies-differ rows (1,552 B) and the 9 no-body rows (1,880 B) are defects: wrong callees,
     wrong map names, `??_G`/`??_E` mismatches, and `HDCache::Flush`, which has no body. Small in bytes, but each one
     is a native-correctness bug of the MemAlloc/`KeyGreaterEq` kind.
   - Never install an alias on a name_check-up / none-flat signature without retail-byte proof.
3. **Finish the class-layout / vtable audit (native correctness; the directive weights this ESPECIALLY high).**
   - What W16-OP §7 / W16-OR §6 left:
     - secondary (non-primary) vtables;
     - `vtable_order_sweep.py` not re-run on the engine dirs;
     - the 55 `no_base` / 10 `parent_no_primary` / 3 unreadable tables;
     - `retail_sizeof_witness`'s aggregate-first-member limit;
     - the IN-RB3ENG synth effects / dsp classes that neither audit scoped.
   - Bytes are small (+216 / +2,240 B for the two audits). Every defect found is a layout bug that the native port
     would otherwise inherit.
4. **Source divergence with a cheap selector.**
   - (a) The 11 in-scope rows / 17,620 B with an unscanned one-sided block (§5).
   - (b) Then the in-scope I1/I3/I4 rows with no per-row record: 157 rows / 56,640 B, at today's measured 2–7%,
     so roughly 1–4 KB.
   - Expect behaviour bugs. Every O-lane found several.
   - Do not re-fund a directory sweep: W16-OO measured 0.3%.
5. **In-scope identification tail: U2 287 rows / 22,884 B, plus U3/U4 1,852 B.**
   - bandobj holds 68 of the U2 rows.
   - Returns are falling (W16-NF 12.6%, W16-NK 32.8% on a smaller pool).
   - Mis-carve versus unidentified is still checked row by row on byte geometry.
6. **Permuter pilot. Deferred by directive; priced only.**
   - In-scope market: P1 + P2 = 125 rows / 83,932 B (27.5% of the in-scope gap, 0.82 pp of `total_code`).
   - 70 rows / 45,348 B are filed W16-NA PERMUTER_ONLY after hand attempts. That is the honest pilot population.
   - 16 rows / 12,160 B are W16-ND "left" and 7 / 5,948 OTHER_BLOCKER.
   - The two recorded whole-binary permuter runs returned ≤ 0, and one was reverted for 23 behaviour defects.
     Any pilot needs the semantic gate.

**Unmatchable in scope:** S0, 4 rows / 1,164 B (TU5/DX image-patch rows).

## 7. Roadmap (standing user directives)

Native is the real goal. Rank by native relevance, then by size × (100 − fuzzy).
- Vtable/struct work is especially valuable.
- Cleanup comes before grind.
- Raising the ceiling stays out of scope as a lever. Today's ceiling move was a side effect of Quazal work (§1).
- XDK is hard-skipped.
- **Re-read the scope memory before dispatching anything under `src/network` or `0x82A40000–0x82B60000`.**
- Run `tools/native_build_gate.sh` before landing any shared-`src/` change.

## 8. Not done

- No source, map, splits, alias or `symbols.txt` edits. Nothing was A/B-measured, because this lane changes no
  code. The native gate was not run, because no `src/` file changed.
- `scripts/native_scope_map.py` was not changed for the bandobj/dsp/synth-effects scope error (§2.2). This doc
  carries the corrected ring instead.
- W16-NA's classifier was not patched for the placeholder artifact (§4.1(3)). This lane applies the correction at
  table time.
- The "opened" overlay is coarser than 10-02's: short-name mentions in docs, not fork `result.json` verdicts. Only
  the carried-forward 10-02 records are per-row exact.
- The relocation-name buckets re-implement 10-02's from its doc; the code was not on disk.
- The `HDCache::Flush` fold, the `??_G`/`??_E` rows and the CYCLE-ASSUMED survivors are flagged, not adjudicated.
- The 10-02 reproduction differs by one row (140 B) on an identical tree and tool binary. I did not chase why.
