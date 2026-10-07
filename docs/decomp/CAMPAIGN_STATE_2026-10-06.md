# Campaign state — 2026-10-06 (after W16-PB…PL)

> **SUPERSEDED** for numbers and priorities by [CAMPAIGN_STATE_2026-10-07.md](CAMPAIGN_STATE_2026-10-07.md) (main `24d250690`: in-scope gap 209,164 B, 94.56% matched; 91.00% of the ceiling). Kept as a method record; 10-07 reruns this method with updated disposition buckets and adds a VIA-DC3 table.

Eighth edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-03b.md` for numbers and
priorities. That doc and `CAMPAIGN_STATE_2026-10-03.md` (W16-OV) remain the method record, and this edition reruns
their method unchanged. Nine lanes landed in between. Five moved the match build (W16-PB, PC, PF, PH, PI), and four
were native-only with Δ0 on the match build (W16-PD, PE, PJ, PL). Scope rules are W16-OV's (the native port is the
goal): XDK is hard-skipped, and Quazal stays as landed with no new Quazal lanes funded. No source, map, splits,
alias or `symbols.txt` edit on this branch.

**W16-PG landed on main as `614420e78` after this measurement.** It is reported with its own figures in §3 and is
not in any number in §1–§2.

## 1. Measured

I used a fresh `scripts/setup_worktree.sh` worktree at main **`7e5a99800`** (W16-PL merged) and ran a full
`./tools/ninja-locked`. The build ended `BUILD_RC=0` with the last edge `[patch-state] OK: tree is a fixed point of
6 post-compile passes`, and pairing covered 1056/1056 declared objects. Ruler: `name_check` (objdiff 4.2.9, tool
commit `a5f0ea903ec1`, `provenance.diff_config` read from `report.json`).

```
python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 7e5a99800 (2026-10-06, W16-PL)"
  scaffold thr<=4 : 0 · thr<=5 : 0 · thr<=6 : 57 units, 290 rows, 57,664 B · thr<=7 : 57 (cliff, not fitted)
total_code            10,247,792
PAIRABLE               6,680,212 = 65.187%   (1,056 units, 55,535 rows)
− scaffold shells         57,664             (57 units, 290 rows)
= reachable ceiling    6,622,548 = 64.624%
matched_code           5,925,876 = 57.826% of total_code = 89.48% of ceiling
gap to ceiling           696,672
matched_functions         53,484 / 68,909   masked_equal 25,192   honest 28,292
fuzzy_match_percent       63.6879
```

**Prediction, written before reading the report:**
- `matched_code` = 10-03b's 5,891,704 + the five match-moving lanes' own A/B claims (412 + 11,820 + 6,960 + 12,260 + 2,720 = 34,172) = **5,925,876 B**.
- `matched_functions` = 53,484, which is W16-PI's leg B.
- The ceiling stays within ±0.01 pp. Only W16-PC and W16-PI moved pins, and only between pairable units.

**Measured:** both headline numbers exactly as predicted, and the ceiling **byte-identical** to 10-03b's (6,622,548 B).

| control | result |
|---|---|
| attribution | Δ vs 10-03b = **+34,172 B / +135 fns**, equal to the sum of the lanes' own whole-binary A/B claims (§3) |
| lane chain | each lane's archived leg A equals the previous lane's leg B on `matched_code`, `matched_functions` and every ring (checked for PC→PH, PH→PF, PF→PI). The last step, W16-PI leg B → this build, is **Δ0 on every ring**: the native lanes did not move the match build |
| independent baseline | main's own `build/45410914/report.json` (built at `7e5a99800`, 08:53) equals this build on every headline key |
| population sums | reachable rows sum to the ceiling, and fuzzy==100 rows to `matched_code`. The gap rows sum to ceiling − `matched_code` = 696,672 (`gengap.py` asserts it) |
| ring sums | tier totals sum to `total_code` and `matched_code` (`scope_ledger2.py` asserts both). In-scope per-lane Δ sums to the in-scope Δmatched, +31,768 B (§3) |
| disposition table | §4's buckets sum to 843 rows / 263,644 B = in-scope ceiling − matched (`dispo2.py` asserts it) |
| placeholder correction | 27/27 placeholder-only N2 rows read identical fuzzy at `none` and graded (10-03b: 26/26) |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail" |
| census rule | the reconstructed `census.json` rule reproduces 10-03b's file 1021/1021 rows, same order, from that run's `final.json` |
| unpaired index | 200/200 paired named rows found in their own base obj |

## 2. Rings

| tier | reachable | matched | **gap** | share | gap at 10-03b | Δ matched | Δ fns | Δ reach |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| IN-CORE | 477,296 | 422,572 | **54,724** | 88.53% | 60,748 | +5,928 | +27 | −96 |
| IN-SOON | 2,425,168 | 2,299,664 | **125,504** | 94.82% | 142,940 | +17,392 | +46 | −44 |
| IN-RB3ENG | 916,588 | 833,172 | **83,416** | 90.90% | 91,864 | +8,448 | +53 | 0 |
| **in scope** | **3,819,052** | **3,555,408** | **263,644** | **93.10%** | 295,552 | **+31,768** | **+126** | −140 |
| VIA-DC3 (DC3 has the file) | 2,026,836 | 1,777,580 | 249,256 | 87.70% | 251,520 | +2,404 | +9 | +140 |
| OUT-QUAZAL | 344,352 | 230,832 | 113,520 | 67.03% | 113,520 | 0 | 0 | 0 |
| OUT-NET | 76,864 | 70,896 | 5,968 | 92.24% | 5,968 | 0 | 0 | 0 |
| OUT-XDK | 3,348 | 400 | 2,948 | 11.95% | 2,948 | 0 | 0 | 0 |
| OUT-360-OTHER | 349,084 | 289,968 | 59,116 | 83.07% | 59,116 | 0 | 0 | 0 |
| UNKNOWN-other | 3,012 | 792 | 2,220 | 26.29% | 2,220 | 0 | 0 | 0 |

- **Native scope CORE+SOON:** 98.29% of functions and 93.79% of bytes. At 10-03b it was 98.00% / 92.98%, and the same formula reproduces 10-03b's 98.00%.
- **Reach moved between rings:** in-scope −140 B, VIA-DC3 +140 B, net 0. That is pin reattribution from W16-PC's re-homes and W16-PI's MeasureMap→Part move, not a ceiling change. So the in-scope gap fell 31,908 B against an in-scope Δmatched of 31,768 B.

**Gap movement, 10-03b → now**, keyed on (unit, symbol):
- **Left the in-scope gap:** 137 rows / 32,208 B.
- **Entered:** 7 rows / 300 B, net −31,908 B (reconciles exactly).
  - Five of the entrants are re-homes: their name left one in-scope unit and re-entered in another (208 B).
  - The other two are the `AutoplayAuditionUser` deleting-destructor rows (92 B). W16-PI renamed them on retail
    vtable evidence. The class is TU5-era and has no source in any repo.
- **Still in the gap:** 35 rows rose and **0 went down**.

The rows that left, by 10-03b class: P1 13 / 12,260 · I3 31 / 5,684 · I4 33 / 5,652 · N1 18 / 5,304 ·
M1 21 / 2,068 · U2 18 / 956 · I1, U4, M2 1 each. Top directories: `system/bandobj` 49 rows / 8,448 B,
`band3/game` 7 / 5,620, `band3/net_band` 4 / 4,796, `band3/meta_band` 9 / 3,120.

⚠ `report.json`'s function `address` field is **unit-relative**, not an absolute address. Keying a cross-build
comparison on it collided 103 address values, which read as 10 false "DOWN" rows and 6,760 B that did not reconcile. Key on
(unit, symbol) and pair re-homes by name, as above.

## 3. What landed, per lane

The per-ring split comes from each lane's archived `ab_measure` legs, classified with this doc's ring rule. W16-PB's
split is W16-PC's leg A minus the 10-03b report; its run dir was removed with its worktree.

| lane | brief (10-03b §5 / follow-up) | Δ fns | Δ matched_code | in-scope (CORE / SOON / RB3ENG) | VIA-DC3 | what else it settled |
|---|---|---:|---:|---|---:|---|
| W16-PB | class-layout/vtable audit remainder | +2 | +412 | +72 (0 / 72 / 0) | +340 | new `tools/vtable_undefined_slots.py` (every slot of every table) and `tools/vcall_slot_census.py` (vtable order from call-site immediates), each with a selftest. Three real defects fixed: SongInfoCopy's 7 getters existed only under `HX_NATIVE`, `BandUser::IsInSession` was defined nowhere, `InlineHelp::mTextLabels` is `BandLabel*` |
| W16-PC | the 146 I1/I3/I4 rows with no record (in flight at 10-03b) | +98 | +11,820 | +11,776 (1,908 / 2,436 / 7,432) | +44 | **12 native-visible behaviour bugs**: BandCharacter ×4, LayerDir, SemitoneToWhiteKey, MakeBSPTree, ReadSingleXinputJoypad ×6 mappings, NewFile; `Character::RemoveFromPoll` written. Rev statics are a file-local adjacent pair (11 classes) |
| W16-PH | lever 3: cycle-assumed survivors (29 rows / 10,364 B) | +20 | +6,960 | +5,180 (1,060 / 3,904 / 216) | +1,780 | 11 pairs **PROVEN** on two-channel retail evidence and installed; 10 **UNDECIDABLE** (no retail name types the call), 0 refuted. **10-03b's "`sort<T**>` will be not-admissible" was wrong**: the comparator type carries `T` |
| W16-PF | lever 1: IR-temporary count over the hand-tried register rows (114 / 78,372) | +0 | +12,260 | +12,260 (692 / 10,880 / 688) | 0 | 13 rows to 100, 3 partials, 98 stopped. The lever also decides **commutative operand order**, and **member reads count** as temporaries, not only call results |
| W16-PI | the five defects W16-PC left | +15 | +2,720 | +2,480 (2,268 / 100 / 112) | +240 | `Object::mRefs` is `std::list<ObjRefOwner*>` (whole-tree PCH change, 0 rows down); MemMgr `.bss` laid out as retail; RemoteBandUser's Wii-only `Handle` removed; MeasureMap mis-pin; WaveFile namespace hash |
| W16-PD | native stub reconciliation | 0 | 0 | — | — | stub definitions 1,481 → 1,221; **rb3-vocal2/harmony first-frame segfault (since 2026-08-03) fixed**; every stub a native run executes has a verdict |
| W16-PE | native runtime check | 0 | 0 | — | — | `tools/native_health.sh` now runs **18/18** targets on real data (was 4) and fails on crash, hang or nonzero exit; it reproduces PD's crash when that is reintroduced |
| W16-PJ | link the real SongDB, CommonPhraseCapturer, `meta_band/Utl.cpp` | 0 | 0 | — | — | stub definitions 1,220 → 981; the synthetic SongDB is gone |
| W16-PL | W16-PJ's four native gaps | 0 | 0 | — | — | the real `BandUser::GetDifficulty` exposed **vocal2/harmony scoring every part at Easy** (fixed); unison path runs on a shipped chart; m1 stubs 24 → 14 |
| **sum** | | **+135** | **+34,172** | **+31,768** | **+2,404** | |

**Landed after this measurement (not in §1–§2):**
- **W16-PG** (`614420e78`) took 10-03b's items 2 and 4: the M1/M2/I2 rows with no record, and the correctness sweep.
  - Its own A/B, against its branch base `6f1d1d794`, not this tree: **+14 fns / +4,944 B, 0 rows down**.
  - Fixed defects:
    - map defects: the FileCache heap trio, which carried Dance Central `MoveDetectorCmp` names; the
      `VocalPhrase`/`MemDiffEntry` overflow mis-pin; `RndText::DeferUpdateText`;
    - the ChordShapeGenerator `.bss` order;
    - PatchDir's `Symbol` hash, the wrong-callee row;
    - String and WorldDir.
  - It recorded a stop reason for every other row it opened.
  - New mechanism: for plain internal statics, **an explicit `= 0` is what makes declaration order decide `.bss`
    order**. Declaration order, reference order and renaming were each inert.
- **W16-PK** has unlanded commits on its branch (ObjectDir `NextSubDir`/`PreLoad`) and is not described here.

**Pricing what came back.** Yield = in-scope bytes newly at 100 ÷ pool bytes.

| lane | yield on its pool |
|---|---:|
| W16-PC | 21.7% (11,776 / 54,164) |
| W16-PF | 15.6% (12,260 / 78,372) |
| W16-PH | 50% of its brief, the 15 proven rows (5,180 / 10,364), plus 1,780 B outside it |

As at 10-03b, every byte came from a targeted mechanism or a per-row read, and none from a directory sweep. Each lane
that read rows for behaviour (W16-PB, PC, PI, and on the native side PD and PL) found real defects. W16-PH found no
wrong type, and W16-PF's fixes are spellings.

## 4. Where the in-scope gap stands: one disposition per row

Each in-scope gap row (843 / 263,644 B) goes to the first bucket that fits, as in 10-03b, with buckets updated for
what W16-PC/PF/PH settled. "Opened" is 10-03b's coarse overlay (doc mentions + sweep-pool membership), extended with
W16-PC's pools, W16-PF's 114 rows and W16-PH's 29 rows. **"Native-compiled"** means the row's source file is
compiled into at least one target of main's native build (`native/build/build.ninja`, regenerated after W16-PL):
418 `src/` files.

| disposition | rows | bytes | % | CORE | SOON | RB3ENG | native-compiled |
|---|---:|---:|---:|---:|---:|---:|---:|
| source divergence, opened and left on record | 84 | 93,136 | 35.3% | 12,372 | 46,540 | 34,224 | 30,436 |
| register/scheduling: W16-PF's lever applied, stopped | 101 | 66,112 | 25.1% | 7,152 | 42,424 | 16,536 | 21,200 |
| I1/I3/I4: worked by W16-PC and left | 59 | 31,760 | 12.0% | 12,664 | 8,244 | 10,852 | 19,348 |
| identification residue (U2) | 225 | 17,744 | 6.7% | 5,928 | 8,624 | 3,192 | 8,112 |
| M1/M2/I2 with no record — **taken by W16-PG after this build** | 45 | 14,956 | 5.7% | 4,512 | 3,604 | 6,840 | 6,536 |
| EH funclet / anonymous fragment of a parent row | 211 | 8,364 | 3.2% | 1,472 | 4,496 | 2,396 | 3,148 |
| name: pointer `_M_fill_insert` fold, settled not admissible (W16-OX) | 14 | 6,760 | 2.6% | 2,388 | 3,572 | 800 | 3,496 |
| name: cycle survivor parked UNDECIDABLE by W16-PH | 14 | 5,184 | 2.0% | 0 | 3,760 | 1,424 | 3,060 |
| **I1/I3/I4 in W16-PC's pool, recorded "not worked"** | 14 | 5,024 | 1.9% | 4,104 | 0 | 920 | 3,852 |
| **register-only after W16-PC's fix; IR-temporary lever never applied** | 8 | 4,496 | 1.7% | 1,376 | 0 | 3,120 | 3,992 |
| I1/I3/I4 with no per-row record | 4 | 2,044 | 0.8% | 132 | 0 | 1,912 | 132 |
| register/scheduling, no record | 6 | 1,880 | 0.7% | 1,172 | 708 | 0 | 1,392 |
| unpaired named (U3/U4) — taken by W16-PG §4 | 28 | 1,848 | 0.7% | 748 | 676 | 424 | 896 |
| name: proven, installable or conflicted | 11 | 1,692 | 0.6% | 184 | 1,176 | 332 | 964 |
| TU5/DX image patch (S0) | 4 | 1,164 | 0.4% | 348 | 816 | 0 | 1,164 |
| name: undecidable, needs a type witness | 7 | 1,044 | 0.4% | 172 | 864 | 8 | 176 |
| name: wrong callee / no body — taken by W16-PG | 8 | 436 | 0.2% | 0 | 0 | 436 | 20 |
| **total** | **843** | **263,644** | 100% | 54,724 | 125,504 | 83,416 | **107,924** |

**How it adds up.**
- **Hard core: 191,008 B, 72.4%.** These are the three largest buckets. Three or more lanes have opened each row, and
  their records say scheduling, block placement or register numbering. W16-PF's lever has been applied to every
  hand-tried register row.
- **Settled, non-separable or parked:** 40,260 B, 15.3%. That is S0, funclets, the OX fold, PH's undecidable pairs,
  the type-witness names, and the U2 residue (17,744 B, which W16-OZ's pipeline already ran over).
- **Proven, installable or conflicted names:** 1,692 B, 0.6%. These are small and open.
- **Never attempted on record: 13,444 B, 5.1%.** Two buckets are new in this edition:
  - **W16-PC's "not worked" rows.** Its g3/g2 tables list them as not worked (turn budget) or FP-only. They are
    matched here by exact demangled signature, and Intersect(Triangle, Box) is excluded because W16-PC did work it.
  - **The 8 rows W16-PC took from source divergence down to register-only.** No register lever has touched them yet.
- **W16-PG's brief:** 17.2 KB at this build. A coarse name overlay of W16-PG's doc names 19 of the 45 M1/M2/I2 rows
  (9,176 B) and 22 of the 28 unpaired rows. Treat those buckets as worked once W16-PG is in the measured tree.

**Native relevance, measured three ways:**
- **107,924 B (41%) of the in-scope gap is in a file the native build compiles today.** The rest is in-ring code
  native does not link yet. The largest such files are VocalTrack.cpp (13,720 B), BandPatchMesh (7,868),
  BandDirector (7,284), GemManager (7,276) and ChordShapeGenerator (6,968).
- **One gap row is replaced by a live native stub.** I ran `tools/native_stub_census.py --authored` read-only on
  main's native build: 961 distinct stub symbols are the live definition in at least one target. Only
  `VocalTrack::RebuildHUD` matches an in-scope gap row by qualified name, and that row is register-only. The stub
  list and the match gap are nearly disjoint, so stub work and gap work do not substitute for each other.
- **Register/scheduling rows (116 rows / 72,520 B) cannot change what native does.** Their instructions are the same
  operations in another order or another register. Nor can the name-settlement buckets: W16-PH checked our element
  type against the surrounding code for all ten undecidable pairs and found it agrees.

**Class view (W16-OV's table, same rules):**

| block | classes | rows | bytes | % | at 10-03b |
|---|---|---:|---:|---:|---:|
| register/scheduling only | P1 + P2 | 116 | 72,520 | 27.5% | 80,100 |
| reorder-shaped insert/delete | I1 + I2 | 58 | 32,644 | 12.4% | 33,032 |
| source divergence | I3 + I4 + M1 + M2 | 211 | 116,800 | 44.3% | 135,232 |
| relocation name | N1 + N2 | 201 | 20,924 | 7.9% | 25,472 |
| unpaired | U2 + U3 + U4 | 253 | 19,592 | 7.4% | 20,552 |
| TU5 image | S0 | 4 | 1,164 | 0.4% | 1,164 |

By directory the gap still concentrates in `system/bandobj` (205 rows / 72,148 B) and `band3/meta_band`
(152 / 48,908), then `band3/bandtrack` (36 / 26,352) and `band3/game` (90 / 24,300). The top 100 rows hold 57.6% of
the in-scope gap, and the median row is 92 B. The largest row is still `VocalTrack::UpdateScrolling` (8,948 B,
96.97, opened by four lanes).

## 5. Next levers, ranked by native relevance, then size

Native relevance has three tiers:
- **A:** the charge can encode behaviour (source divergence, immediates, a missing body), in a file native compiles today.
- **B:** a behaviour-class charge in an in-scope file native does not link yet.
- **C:** codegen-only or naming. A fix moves the metric but cannot change native behaviour.

Within a tier, levers are ranked by bytes. The buckets W16-PG took are not re-ranked.

1. **[A] The never-worked source-divergence rows: 18 rows / 7,068 B; 4,236 B CORE; 3,984 B native-compiled.**
   - W16-PC's 14 "not worked" rows. They are the engine loading and I/O paths every native target runs:
     - `DirLoader::SaveObjects` 1,044, `DirLoader::~DirLoader` 356
     - `DataStringFlags` 296, `BlockMgr::AddTask` 284, `ThreadCallPoll` 252, `LoadDtz` 188
     - `DataPoint::AddPair` 164, `JoypadClient::Poll` 116, `CDReadExternal` 104, `ObjectDir::HasSubDir` 96
     - math: `Clip` 512, `Intersect(Segment, Triangle)` 432, `Quat::Set(Vector3)` 260
     - `FindCCPeak` 920 (dsp)
   - The 4 I-class rows nobody has recorded: `FlangerEffect::Process` 768, `WahEffect::Process` 760,
     `DelayEffect::Process` 384, `RndDir::EndFrame` 132.
   - Price at W16-PC's 21.7% (≈1.5 KB). The point is behaviour: W16-PC's 12 native-visible bugs came from reading
     rows of exactly this kind for retail's behaviour.
   - Coordinate with W16-PK, which has unlanded ObjectDir commits.
2. **[A] A behaviour-only read of the hard-core rows in native-compiled files: 49,784 B.** That is 30,436 B
   "opened and left" plus 19,348 B "worked by W16-PC and left", in `system/{obj,os,utl,math}` and the native-linked
   band3/bandobj files.
   - Expect almost no bytes: W16-OZ's targeted pass on this class returned 1.1%.
   - The case for it is that "opened" is an upper bound. A row named once in a matching lane's doc may never have
     been read for semantics, and W16-PC's bugs were all found by reading for retail's behaviour, not by grinding
     codegen.
   - Scope it as an audit that records "behaviour identical, codegen residue" or fixes a defect, with no target bytes.
3. **[B] The same audit on in-scope files native does not link yet: 75,112 B of the same two buckets.**
   - Mostly VocalTrack, BandDirector, GemManager, BandPatchMesh and CustomizePanel.
   - Rank it below lever 2 because native cannot observe these bugs until the files are linked.
4. **[C] W16-PF's lever on the 14 register rows it never reached: 14 rows / 6,376 B, 5,384 native-compiled.**
   - 8 are rows W16-PC took to register-only (`BandIKEffector::DoFancyElbow` 1,040, `ReadSingleXinputJoypad` 812,
     two BandCharDesc PropSyncs 656 + 480, `BandWardrobe::AddDircut` 504, `NeutralLocalXfm` 440, `operator>` 372,
     `Multiply` 192).
   - 6 have no record (`AddChordLevel` 488, `MemTruncate` 284, `InterpTangent` 280, …).
   - Price at W16-PF's 15.6%. Fresh rows may do better, because W16-PF's population had already been hand-tried.
5. **[C] A W16-PG §2.1 `.bss` sweep.**
   - Mechanism: explicit `= 0` makes declaration order decide `.bss` placement of internal statics. Without it,
     the code generator picks the order.
   - The population is unmeasured. The first step is an instrument: list rows whose only charge is the offset off a
     shared `.bss` anchor.
   - W16-PG measured `PreInitSystem`/`InitSystem` inert to it, so do not assume it reaches every anchor row.
6. **[C] W16-PF's open half on its 101 stopped rows (66,112 B): pairwise site sweeps.**
   - W16-PF §5 records that combinations of two or more sites were swept only by hand. With ~1,200 member-read sites,
     a full pairwise sweep is quadratic and needs a pruning rule first.
   - This is the permuter's market by another route, so it needs the same semantic gate.
7. **[C] Name settlements: W16-PH's 10 undecidable pairs (14 rows / 5,184 B) and the 7 type-witness names (1,044 B).**
   - W16-PH records the only path: a new witness rule, e.g. "same-member operation" for `Keys<DircutEntry>`, with
     its own controls.
   - Our types were checked to agree, so this cannot change native behaviour.
8. **[C] Identification residue (U2): 225 rows / 17,744 B**, of which 67 rows are ≤16 B. W16-OZ's pipeline already
   ran over this population, so it is bookkeeping unless a new identification channel appears.

**Not ranked, for the user to decide (unchanged from 10-03b):**
- **The VIA-DC3 ring (249,256 B).** Native takes this code from DC3, so its value is metric, not native correctness.
- **The permuter pilot** (P1+P2, 116 rows / 72,520 B), deferred by directive.

**Out of ranking by directive:**
- Quazal: 113,520 B.
- XDK: 2,948 B.
- 360-only: 59,116 B.
- NET: 5,968 B.

## 6. Reading the numbers

- In-scope match share is **93.10%** (10-03b: 92.26%). The whole-binary share of the ceiling is 89.48%. Since
  10-03b the in-scope gap fell **31,908 B (−10.8%)**, against +34,172 B whole-binary.
- **The open in-scope surface that is not hard core, not settled, and not W16-PG's brief is now 13,444 B**, plus
  1,692 B of proven names.
  10-03b put the "not yet attempted" share at ~32 KB. Almost everything left either has a per-row record or is
  codegen-only.
- **41% of the in-scope gap sits in files native compiles today.** The remaining 59% becomes native-relevant only as
  more band3/bandobj files are linked into native targets. That is the native lanes' work, not the matching lanes'.
- **The ceiling did not move.** It moves only when scaffold TUs gain bodies or pins re-attribute code across the
  pairable boundary, and no lane since 10-03b did either.

## 7. Not done

- **No source, map, splits, alias or `symbols.txt` edits.** Nothing was A/B-measured, because this branch changes no
  code. The native gate was not run (no `src/` change).
- **W16-PG is not in the measured tree.** Its fixes will show in the next full build.
- **The overlays are coarse:**
  - The "opened" overlay counts doc mentions and pool membership, not per-row verdicts.
  - The W16-PG overlay matches by doc name only.
  - The native-compiled column is file-level. A row in a natively compiled file may still be dead code on every
    native run.
- `scripts/native_scope_map.py` still files bandobj/dsp/synth effects under VIA-DC3 (W16-OV §2.2). This doc carries
  W16-OV's corrected ring.
- **W16-PK and W16-PM** were in flight or not started at the time of writing and are not described.

Scripts and JSON are in `~/tmp/w16pn-gap/` and are not committed.
- 10-03b's pipeline, copied and repointed at this worktree: `gengap`, `scope_ledger2`, `diffall`, `unpaired`,
  `overlay3` (plus the W16-PC/PF/PH pools), `insdel_shape`, `tables3`, `pairs`, `chase_all`, `namecls`, `final3`,
  `tables4`.
- New in this edition:
  - `mkcensus.py`: the census rule, verified against 10-03b's file.
  - `cmp1003b.py`: ring deltas and row movement.
  - `lane_rings.py`: the per-lane ring split from archived A/B legs, with chain checks.
  - `dispo2.py`: §4's buckets and the native-compiled column.
  - `native_src.json`: the native build's `src/` file list.
  - `stubs.json`: the stub census.
- Per-row table: `dispo_rows.json`.
