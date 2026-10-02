# Campaign state — 2026-10-02 (lane W16-NP)

Fifth edition of the single current-state doc. Supersedes `CAMPAIGN_STATE_2026-10-01.md` for numbers
and priorities. New in this edition: **the whole remaining gap to the reachable ceiling is partitioned
row by row by what blocks it today**, so the next decision, including whether to lift the permuter
deferral, can be priced. No source, map, splits or `symbols.txt` edits on this branch.

## 1. Measured

Full `./tools/ninja-locked` in a fresh `scripts/setup_worktree.sh` worktree at main **`2b066acc8`**
(W16-NL merged). The build's last edge reports `[patch-state] OK: tree is a fixed point of 6 post-compile passes`.
Ruler `name_check` (objdiff 4.2.9, tool commit `a5f0ea903ec1`, read from `report.json`
`provenance.diff_config`). Ceiling from
`python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main 2b066acc8 (2026-10-02)"`.

```
total_code            10,247,368
PAIRABLE               6,505,924 = 63.489%   (1,055 units, 54,426 rows)
− scaffold shells        178,664             (103 units, 906 rows; cliff thr≤4 = 0, ≤5 = 0, ≤6 = 103, ≤7 = 103)
= reachable ceiling    6,327,260 = 61.745%
matched_code           5,590,840 = 54.559% of total_code = 88.36% of ceiling
gap to ceiling           736,420
matched_functions         51,375 / 68,926   masked_equal 24,629   honest 26,746
fuzzy_match_percent       60.389
```

**Prediction, written before the re-measure:** I had first measured main `119c4e270` (gap **750,692 B**,
88.14% of ceiling). The coordinator then reported that W16-NL had landed. Its A/B was +14,272 B, map and source only,
so the ceiling should not move and the gap should be 750,692 − 14,272 = **736,420 B**. **Measured: 736,420 B,
ceiling unchanged at 6,327,260 B.** `matched_functions` 51,375 equals W16-NL's leg B.

Versus 2026-10-01 (`a95d5c525`): ceiling 61.537% → **61.745%** (+0.208 pp, +21,500 B, from today's new
pins: W16-NM/NO's XboxServer/XMAReader units and W16-NE's re-homes). matched 45.849% → **54.559%**
(+8.71 pp). Share of the ceiling 74.51% → **88.36%** (+13.85 pp). Gap **1,607,576 → 736,420 B (−54%)**.

## 2. Partition of the gap

### 2.1 Method

1. **Population.** Every row of every unit that has a base obj and is not a scaffold shell. The
   scaffold set is `ceiling_recompute.py`'s own (defined symbols ≤ 6), imported, not re-derived. A gap row is
   one with `fuzzy_match_percent < 100`.
2. **Charge class.** Every gap row is diffed once with `objdiff-cli diff` under the project config
   (graded ruler). Classifier: **W16-NA's `cls.py` rules verbatim**. SCHED: **W16-NH's rule
   verbatim** (insert/delete multisets equal with registers masked, nothing replaced).
   `base_size == 0` is objdiff's own "unpaired".
3. **Insert/delete shape.** STRUCT_INSDEL rows are split by the difference between the target-only and
   base-only opcode multisets (args ignored): equal (I1), only `mr`/`fmr`/`or` moves (I2), 1–4 opcodes (I3),
   more (I4).
4. **Unpaired.** Each row is checked as anonymous or named. For named rows, a COFF symbol index over all
   1,055 declared base objs says whether another obj defines the name (U3) or none does (U4). Anonymous
   rows in `src/network/{quazal,ObjDup,Core,Platform,Plugins,…}` units are split off as U1.
5. **Adjudication overlay** (§2.4). Per-row records left by today's lanes: W16-NA fork `result.json`s
   (exact names and verdicts), W16-NH and W16-NL fork `result.json`s, and the "rows left" sections of the
   W16-ND/NB/NH/NL/NM/NO docs (short names). Sweep-pool membership comes from W16-NA/NH's population files,
   W16-NL's slices, and W16-NB's rule (every non-Quazal row at 1 ≤ fuzzy < 90).
6. **Relocation names** (§2.3). W16-NH's `pairs.py` + `chase_all.py`, rerun unchanged on this tree over
   every gap row that carries a charged symbol arg (989 rows, 1,045 distinct pairs). A row takes its
   **worst** pair, since its name charges clear only if every pair clears.

Scripts and intermediate JSON: `~/tmp/w16np/` (`diffall.py`, `unpaired.py`, `overlay.py`,
`insdel_shape.py`, `tables.py`, `pairs.py`, `chase_all.py`; final per-row table `final.json`). Not committed.

### 2.2 Controls

| control | result |
|---|---|
| population sums | reachable rows sum to the ceiling **6,327,260 = 6,327,260**; their fuzzy==100 rows sum to `matched_code` **5,590,840 = 5,590,840** (no matched byte lies outside the reachable set); gap = ceiling − matched **exactly** |
| diff vs grader | graded fuzzy from `objdiff-cli diff` equals `report.json`'s on **1,790 / 1,790** paired rows, **0** disagreements; every UNPAIRED row reads fuzzy 0 in the report; **0** diff errors |
| COFF name index | finds **200 / 200** sampled paired names in their own base obj |
| overlay join | **0 of W16-NA's 61 FIXED records** remain in the gap, while 130 of its 142 PERMUTER_ONLY rows do (the other 12 were closed by W16-ND/NH/NL). Short-name collisions inside the gap: 2 rows / 308 B |
| chase instrument | `icf_pair_adjudicate.py --chasetest`: "selftest PASSED -- the instrument can both pass and fail" |
| re-measure | gap predicted 736,420 B on the new tip from W16-NL's A/B; measured 736,420 B |

### 2.3 The table

Every gap row falls in exactly one class. Bytes are "would count if the row crossed" (`matched_code` is
all-or-nothing at fuzzy == 100). "At mpn 100" means rows already counted in `matched_functions`: closing
them buys bytes and no functions.

| id | what blocks the row today | rows | bytes | % of gap | at mpn 100 |
|---|---|---:|---:|---:|---:|
| **P1** | register / stack-slot arguments only | 186 | 103,360 | 14.0% | 173 |
| **P2** | pure reschedule (W16-NH SCHED) | 44 | 25,656 | 3.5% | 0 |
| **I1** | insert/delete, opcode multisets equal (reorder plus operand differences) | 61 | 44,268 | 6.0% | 0 |
| **I2** | insert/delete, only extra or missing register moves | 48 | 25,612 | 3.5% | 0 |
| **I3** | insert/delete, 1–4 opcodes differ | 260 | 132,032 | 17.9% | 0 |
| **I4** | insert/delete, more than 4 opcodes differ | 224 | 138,004 | 18.7% | 0 |
| **M1** | immediate (constant, member offset or frame offset) | 293 | 53,312 | 7.2% | 0 |
| **M2** | opcode replace, branch destination, other argument | 62 | 19,556 | 2.7% | 8 |
| **N1** | relocation name only (fold, wrong callee or wrong map name) | 567 | 65,008 | 8.8% | 316 |
| **N2** | relocation name plus register | 40 | 20,948 | 2.8% | 32 |
| **U1** | unpaired, anonymous, Quazal-family unit | 117 | 33,612 | 4.6% | 0 |
| **U2** | unpaired, anonymous, everything else | 644 | 54,788 | 7.4% | 0 |
| **U3** | unpaired, named, another compiled obj defines the name | 54 | 4,056 | 0.6% | 0 |
| **U4** | unpaired, named, no compiled obj defines the name | 69 | 14,968 | 2.0% | 0 |
| **S0** | TU5 image-patch rows (`DataSet`, `SetDiskError`, `IsDemo`, `AddSongData`, `main`) | 5 | 1,240 | 0.2% | 0 |
| | **total** | **2,674** | **736,420** | 100% | 529 |

**Rolled up:**

| block | classes | rows | bytes | % of gap |
|---|---|---:|---:|---:|
| register allocation / scheduling only (the permuter's market) | P1 + P2 | 230 | 129,016 | **17.5%** |
| reorder-shaped insert/delete (upper-bound extension of that market) | I1 + I2 | 109 | 69,880 | 9.5% |
| source divergence | I3 + I4 + M1 + M2 | 839 | 342,904 | **46.6%** |
| relocation name | N1 + N2 | 607 | 85,956 | 11.7% |
| unpaired (identification, missing body, home) | U1–U4 | 884 | 107,424 | 14.6% |
| structurally unmatchable (TU5 image) | S0 | 5 | 1,240 | 0.2% |

**Sub-splits that change how a class should be read:**

- **EH funclets.** 244 paired anonymous rows (10,096 B) sit inside M1/M2/I3/I4, and 316 more (12,824 B) inside N1.
  These are funclets paired by byte signature. Their charges are frame offsets and destructor names that
  follow the parent function. They are not separate work items. The 316 N1 funclets are exactly N1's 316 rows at
  mpn 100.
- **Fuzzy band.** 99.0–99.99 holds **97,468 B of P1**, 51,120 B of M1 and 64,660 B of N1. I4 is the only class
  concentrated below 90 (76,932 B at <90).
- **Concentration.** The top 10 rows hold 6.5% of the gap, the top 100 hold 30.4% and the top 500 hold 68.7%. The median gap row is
  96 B.
- **Family.** Engine (`src/system`) holds 551,792 B (74.9%), game 139,112 B (18.9%), Quazal 36,488 B and
  root/xdk 9,028 B. Family is by unit source path, so `bandobj`/`track` count as engine.

**Relocation-name rows by worst chase verdict** (N1 + N2 = 607 rows / 85,956 B):

| verdict of the row's worst pair | rows | bytes | what it means |
|---|---:|---:|---|
| PROVEN, our spelling free | 47 | 2,104 | alias installable; the free lever is drained |
| PROVEN, our spelling **withdrawn** by an earlier lane | 29 | 6,636 | conflict with a recorded withdrawal |
| PROVEN, our spelling in another group / map-resident | 33 | 4,992 | survivor conflict |
| PROVEN with a CYCLE-ASSUMED leaf, spelling free | 64 | 16,564 | needs W16-JE's two-channel witness per row |
| PROVEN with a CYCLE-ASSUMED leaf, withdrawn / conflicted | 44 | 14,372 | as above, plus the conflict |
| REFUTED, masked bodies differ | 150 | 11,908 | retail calls a different body: **a wrong callee or a wrong map name, i.e. a defect** |
| REFUTED by chase (vacuous small body, template-twin targets) | 191 | 9,008 | undecidable on bytes; needs a type witness |
| survivor outside every pinned span | 30 | 16,248 | pin the survivor, then chase |
| our spelling in no compiled obj | 19 | 4,124 | the name we call has no body |

The 64 rows / 19,736 B where chase now **proves** a membership that a prior lane withdrew deserve an audit
before anyone overrides them. W16-NN showed today that the old naming size gate compared unlike sizes,
and STLPORT-1 found the same artifact earlier, so some withdrawals may rest on it. Each one must be re-read on
retail bytes. Chase PROVEN is not enough on its own.

**Unpaired rows:**

- **U1** (Quazal-family, 33,612 B) is exactly W16-NK's "`src/network/{ObjDup,Core,Platform,Plugins}`" set,
  117 rows. Their callers are other anonymous Quazal code (W16-NK §2).
- **U2** is engine 40,924 B / 493 rows, game 10,124 / 128, `Memory_Xbox`/`App`/`keygen` 3,652 / 21 and xdk 88 / 2.
  W16-NF §8 and W16-NK §8 already adjudicated it: about 380 engine rows / ~31.7 KB have no paired caller and no vtable slot.
  172 rows are ≤16 B (1,792 B of thunks, getters and carve tails). Mis-carve is not mechanically separable
  from "unidentified" here. The IsGameOver-class carve is checked only by byte geometry, row by row.
- **U3** (4,056 B) is mostly COMDAT instantiations pinned where our TU does not instantiate them, which is
  a re-home lever. Re-homing is not metric-neutral, and it pays.
- **U4** (14,968 B):
  - `synth_xbox/FFT.cpp`'s three AltiVec routines, 6,136 B.
  - `xdk/LIBCMT` `rtti`/`osfinfo`, 2,716 B.
  - **19 rows under `ui/UILabel.cpp` whose map names are `LEAPCORE::…`/`NUISPEECH::…`**, 2,328 B. They look like a speech-library
    block pinned under UILabel. Flagged, not checked.
  - About 40 single game/engine rows whose map name our tree does not define.

### 2.4 What today's lanes already adjudicated, per class

"Opened, left" means a per-row record names the row and why it stayed. "Pool, unopened" means it was in some
lane's population with no per-row record. For the paired classes, rows outside every pool total ≤ 3 KB per
class: **every paired class has been swept at least once.**

| id | opened, left (rows / B) | pool, unopened (rows / B) | verdicts among the opened |
|---|---|---|---|
| P1 | 151 / 93,100 | 35 / 10,260 | **113 / 67,104 PERMUTER_ONLY** (W16-NA), 10 / 5,308 OTHER_BLOCKER, 6 / 5,608 improved, 22 / 15,080 left by NH/ND/NL |
| P2 | 29 / 19,876 | 15 / 5,780 | **17 / 11,240 PERMUTER_ONLY**, 3 / 5,016 OTHER_BLOCKER, 2 improved, 7 left |
| I1 | 10 / 16,136 | 48 / 26,456 | left (scheduling/stack-slot residue per W16-NH/NL) |
| I2 | 13 / 12,416 | 34 / 12,724 | left |
| I3 | 83 / 74,516 | 171 / 56,944 | left |
| I4 | 34 / 53,572 | 173 / 81,500 | left |
| M1 | 12 / 12,980 | 267 / 38,960 | left (192 of the 267 are funclets) |
| M2 | 10 / 7,460 | 50 / 11,532 | left |
| N1/N2 | 2 / 676 | 600 / 84,676 | every pair re-chased here (table above). W16-NH chased all 627 pairs of its population, so "unopened" here means not hand-opened |

**Named insert/delete and immediate rows nobody has opened (no per-row record, not funclets):**
I3 167 / 57,104 B, I4 175 / 83,704 B, M1 77 / 31,856 B, **total 419 rows / 172,664 B (23.4% of the gap)**.
Most of the I4 part is the 50–90 band that W16-NB's sweep did not reach.

## 3. What each lever has returned today (measured, for pricing)

All are whole-binary `ab_measure` results, graded ruler. "Pool" is each lane's own starting population.

| lane | lever | pool | Δ matched_code | yield |
|---|---|---:|---:|---:|
| W16-NB | 1–90% rows, largest gap first | 170,012 B (band) | +43,024 | 25.3% |
| W16-NA + W16-ND | register/scheduling rows by hand (source levers, **no permuter**) | 143,660 B | +26,724 | 18.6% |
| W16-NK | anonymous game rows (identification) | 19,360 B (game side) | +6,352 | 32.8% |
| W16-NF | anonymous engine rows (identification) | 57,180 B | +7,176 | 12.6% |
| W16-NH | named 90–99.99 by charge class | 531,356 B | +35,156 | 6.6% |
| W16-NL | named insert/delete 90–99.99 | 241,332 B | +14,272 | 5.9% |
| W16-NM + W16-NO | Xbox net / XMA code written from retail asm, pinned | — | +16,300 | — |
| permuter, last whole sweep (`843c7e98`, 08-31, reverted) | permuter transforms | — | **−4 B, −1 fn, 23 behaviour defects** | < 0 |
| `permuter-sweep-fresh` (10 units) | permuter transforms | — | 0 transform wins | 0 |

Hand work on the register-only class also found **7 behaviour bugs** (W16-NA §3.1). A permuter cannot
find these, because it optimises bytes and is blind to meaning.

## 4. Pricing the permuter deferral

- **Market.** P1 + P2 = **230 rows / 129,016 B = 17.5% of the gap = 1.26 pp of `total_code`.**
  Counting reorder-shaped insert/delete rows (I1 + I2) as reachable too gives an upper bound of 339 rows / 198,896 B
  (27.0%, 1.94 pp). Neither figure is a forecast.
- **Function count.** 173 of 186 P1 rows are already at mpn 100, so the permuter's ceiling on
  `matched_functions` is about +13 (P1) plus P2's 44. Its payout is bytes.
- **Already exhausted by hand.** The rows filed PERMUTER_ONLY after hand attempts are 130 rows / 78,344 B (P1 113 + P2 17).
  This is the honest permuter-only market. 50 P1/P2 rows / 16,040 B have no per-row record yet, and
  W16-NA/ND's hand levers returned 18.6% on this class. **Hand-sweep those 50 rows first.**
- **History.** The two recorded permuter runs returned ≤ 0. The whole sweep was reverted for behaviour defects
  that compiled clean, so any lift needs the semantic commit gate
  (`project_permuter_correctness_model`, `project-permuter-semantic-blindspot-measured-2026-08-31`).

**Verdict for the coordinator:**
- Lifting the deferral now would aim at **at most ~78 KB (0.76 pp)** of `matched_code`, at a historical yield
  of zero.
- The named insert/delete/immediate rows nobody has opened are **172 KB**. At today's measured 6–25% source
  yield, that is ~10–40 KB, and it finds behaviour bugs along the way.
- If the permuter is reopened, run it as a **capped A/B-measured pilot on the 130 filed PERMUTER_ONLY rows** with the
  semantic gate, and do not run it as a sweep.

## 5. Next levers, ranked

1. **Named insert/delete + immediate rows nobody opened: 419 rows / 172,664 B (23.4%).**
   - The bytes: I4 at 50–90 (W16-NB's 25% band), then I3 and M1 at 90–99.99 (W16-NH/NL's 6%).
   - Carry W16-NL's levers: a loop bound written in the loop condition, implicit conversion instead of an explicit
     temporary, a named reference local behind a dead frame store, and `<cmath>` double routines on floats.
   - Expect behaviour bugs. Every lane today found several.
2. **Name-class adjudication: about 50 KB that is decidable on bytes.**
   - (a) Pin the 30 unpinned survivors, then chase: 16,248 B.
   - (b) Two-channel witnesses for the 64 free CYCLE-ASSUMED rows: 16,564 B.
   - (c) Re-read on retail bytes the 64 rows where chase proves a withdrawn membership, in case the withdrawal was the old size
     gate's artifact: 19,736 B.
   - (d) The 150 REFUTED-bytes rows (11,908 B) are wrong callees or names. They are small in bytes but each one is a correctness
     defect, the same class as the MemAlloc/TempAlloc and `KeyGreaterEq` finds.
   - Never install an alias on a name_check-up / none-flat signature without retail-byte proof.
3. **Hand-sweep the 50 P1/P2 rows with no per-row record (16,040 B)** with W16-NA's lever list. Cheap,
   18.6% measured yield, and it settles which of them really are permuter-only.
4. **Permuter pilot (only if 1–3 are drained or blocked).** 130 filed rows / 78,344 B, with the semantic gate, as an
   A/B-measured pilot (§4).
5. **Identification tail: U2 54,788 B + U3 4,056 B.** W16-NF/NK left most of U2 without a witness, so
   returns are falling. U3 is a re-home job, and re-homing pays. Also look at the UILabel `LEAPCORE`/`NUISPEECH` block (2,328 B,
   likely a mis-pin).

**Out of reach or out of scope:**
- U1 Quazal-family anonymous code, 33,612 B.
- S0 TU5 image-patch rows, 1,240 B.
- The vendor part of U4: FFT AltiVec and LIBCMT, 8,852 B.

Together these are 43,704 B (5.9% of the gap). With them excluded, the gap is **692,716 B**.

## 6. Roadmap (standing user directives, unchanged)

Breadth (rank by size × (100 − fuzzy)); cleanup-before-grind; vtable/struct work is high value; game over
engine; native is the real goal (`tools/native_build_gate.sh` before landing shared `src/`). Raising the
ceiling stays out of scope (Δgap exactly 0, measured 08-17). The permuter stays deferred unless the coordinator
acts on §4.

## 7. Not done

- No source, map, splits or alias edits. Nothing was A/B-measured, because this lane changes no code.
- U2's mis-carve versus unidentified split is not mechanised. The UILabel speech block and the 64 withdrawn-but-
  proven memberships are flagged, not adjudicated.
- The overlay's "opened, left" column joins on short names for the W16-NH/NL/ND/NB/NM/NO records. Only
  W16-NA's records are exact. 2 short-name collisions were found (308 B).
- The native gate was not run, because no shared `src/` changed.
