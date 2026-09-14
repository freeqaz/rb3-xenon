# W16-AU — DirectInstrument heading, AR-1/AR-2 re-homes, AQ's unhomed thunk at 0x82682688

**Lane:** W16-AU (opus) · **branch:** `w16-au`, based on main at `6c35e385` · **date:** 2026-09-14
**Scope:** `config/45410914/splits.txt` only. No `src/` edit, no map edit, no alias edit.
**Ruler:** `name_check` (shipped default), objdiff **4.2.9**, `tool_binary_hash` `5a51cd51fe0a353f`,
read from `report.json`'s own `provenance` block rather than assumed.

## Headline

| | matched_functions | matched_code | matched_code_percent | total_code |
|---|---:|---:|---:|---:|
| lane start (= main `11506937`) | 43,470 | 4,028,008 | 39.312965 | 10,246,004 |
| lane end | **43,472** | **4,028,224** | **39.315075** | 10,246,004 |
| **delta** | **+2** | **+216 B** | +0.002110 pp | **0** |

Four items, four separate full-build set-diff measurements, one per commit. `total_code` never
moved, so nothing here is a denominator effect. The baseline reproduced the brief's figures to
the digit before any edit, which is the only reason the deltas below are worth anything.

**The bytes are not the point of this lane.** Two of the four items measured exactly Δ0, and both
are landed deliberately: one converted a 328 B row from *structurally unpairable* into a 97.56 %
near-miss with a named two-instruction defect, and the other completed a unit and unblocked a map
rename. That is the "accuracy beats headline %" case the brief pre-authorised, and each is priced
and labelled rather than asserted.

---

## Item 1 — `band3/game/DirectInstrument.cpp` gets a heading

### Geometry read before the edit

`band3/game/DirectInstrument.cpp` is declared in `objects.json:1214` as `NonMatching` and compiles,
but had **no `splits.txt` heading**, so its retail bodies sat in neighbours. Every briefed address
and size was confirmed against **two** independent instruments before carving — retail `.pdata`
(`tools/pdata_extent.py`) and dtk's own carve in `config/45410914/symbols.txt` — plus
`scripts/target_symbol_map.json` for names:

| retail | size | what it is | pinned to (before) |
|---|---:|---|---|
| `0x826CCA78` | 12 | `?Handle@TrainerPanel@@$4PPPPPPPM@A@AA…` — a **vcall adjustor thunk, genuinely TrainerPanel's** | TrainerPanel |
| `0x826CCA88` | 100 | `?Disable@DirectInstrument@@` | TrainerPanel |
| `0x826CCAF0` | 16 | `?InTransition@UIManager@@QAA_NXZ` — **ICF survivor from another TU** | PracticePanel |
| `0x826CCB00` | 28 | `?NoteOn@` | TrainerPanel |
| `0x826CCB20` | 12 | `?NoteOff@` | TrainerPanel |
| `0x826CCB30` | 24 | `?PlayNote@` | TrainerPanel |
| `0x826CCB48` | 8 | thunk, map-named `??$_Destroy@UGrammar@SpeechMgr@@…` (**wrong — see AU-1**) | TrainerPanel |
| `0x826CCB50` | 12 | `?PostLoad@` | TrainerPanel |
| `0x826CCB60` | 168 | `?Enable@` | TrainerPanel |
| `0x826CCC08` | 8 | `except_data` (ctor's EH prefix) | TrainerPanel |
| `0x826CCC10` | **328** | `??0DirectInstrument@@` | **`Splash.cpp`** |
| `0x826CCD58` | 44 | ctor EH funclet | TrainerPanel |
| `0x826CCD84` | 40 | ctor EH funclet | TrainerPanel |
| `0x826CCDB0` | 8 | `except_data` (dtor's EH prefix) | TrainerPanel |
| `0x826CCDB8` | 68 | `??1DirectInstrument@@` | TrainerPanel |
| `0x826CCDFC` | 44 | dtor EH funclet | TrainerPanel |

Two things the brief did not say, both of which changed the carve:

- `0x826CCA78` is **TrainerPanel's own** vcall adjustor, so the new span must start at `0x826CCA88`,
  not earlier.
- `0x826CCAF0` is a 16 B ICF hole **in the middle of the DirectInstrument run**, already pinned
  separately. splits blocks are a partition, so DirectInstrument **cannot be one contiguous block**.

⚠ The six small bodies (`NoteOn`/`NoteOff`/`PlayNote`/the 8 B thunk/`PostLoad`) have **no `.pdata`
entries at all** — they sit inside a 116 B "gap" between `Disable`'s end (`0x826CCAEC`) and
`Enable` (`0x826CCB60`). That is the sub-`.pdata` leaf-stub stratum CLAUDE.md describes (an 8 B leaf
touches neither stack nor LR, so it gets no unwind record). **`.pdata` is not the oracle for them;
dtk's carve is.** A carve driven by `.pdata` alone would have silently swallowed them.

### ⛔ The brief's size-divergence claim is REFUTED — it is the STLPORT-1 instrument error

The brief said: *"ctor (ours 420 vs 328) and dtor (ours 120 vs 68) are not [size-identical] — record
the source divergence."* **There is no source divergence.** That comparison put our **COMDAT span,
including the EH prefix and the EH funclets**, against retail's **bare function extent**. The two
sides were never the same measurement — bit-for-bit the disease CLAUDE.md records under STLPORT-1.

Measured from our object's COFF section headers and symbol table:

| | our `.text` section | decomposition | retail span | retail bytes |
|---|---:|---|---|---:|
| ctor | **420 B** (sec 111) | 8 EH prefix + **328** ctor (sym val 8) + 44 `__unwind$64186` (val 336) + 40 `__unwind$64187` (val 380) | `0x826CCC08–0x826CCDAC` | **420 B** |
| dtor | **120 B** (sec 121) | 8 EH prefix + **68** dtor (sym val 8) + 44 `__unwind$64260` (val 76) | `0x826CCDB0–0x826CCE28` | **120 B** |

⇒ **All eight bodies are size-identical to retail**, not six. The prize ceiling was 748 B of function
extents, not 344 B. And the three "unnamed" bodies the brief could not account for —
`fn_826CCD58` (44), `fn_826CCD84` (40), `fn_826CCDFC` (44) — are **the ctor's and dtor's own EH
funclets**, which is why they sit exactly where they do.

### The edit

```
band3/game/TrainerPanel.cpp:   .text 0x826CB654-0x826CCAF0  ->  0x826CB654-0x826CCA88
                               .text 0x826CCB00-0x826CCC10  ->  (deleted, absorbed)
                               .text 0x826CCD58-0x826CD438  ->  0x826CCE28-0x826CD438
Splash.cpp:                    .text 0x826CCC10-0x826CCD58  ->  (deleted, absorbed)
band3/game/DirectInstrument.cpp:  NEW
                               .text 0x826CCA88-0x826CCAF0
                               .text 0x826CCB00-0x826CCE28
```

Only `.text` was edited. dtk re-derived `.pdata` on the first build — the `[split-guard]` fired
exactly as designed ("THE SPLIT REWROTE ITS OWN INPUT"), the rewrite was inspected (Splash loses
`.pdata 0x82234B00-0x82234B08`, TrainerPanel's two records shrink, DirectInstrument gains
`0x82234AF0-0x82234AF8` and `0x82234AF8-0x82234B28`), and the retry was a fixed point.
`symbols.txt` was unchanged throughout, so no re-split iteration was needed.

### Predicted vs measured

**Predicted:** the rows become pairable; ceiling 748 B; the 8 B thunk will *not* pair because its map
name is a SpeechMgr fold. **Measured** (set-diff, full build):

```
matched_functions 43,470 -> 43,470   (+0)
matched_code   4,028,008 -> 4,028,008 (+0)
total_code    10,246,004 -> 10,246,004 (unchanged)
fuzzy_match_percent 49.6506 -> 49.65365  (+0.00305 pp)
CROSSED IN : 10 rows, 540 B   (all into default/band3/game/DirectInstrument)
FELL OUT   : 10 rows, 540 B   (all out of default/band3/game/TrainerPanel)
```

### ⛔ Why Δ0 — the second refuted premise, and the more useful one

"A mis-homed row cannot pair" is **only half true**, and the half that is false is why this measured 0.
The seven TrainerPanel-pinned rows were **already at fuzzy 100** before the edit. The reason is in the
source: a prior lane worked around the missing heading by **scatter-including the whole TU**:

```
src/band3/game/TrainerPanel.cpp:465
  // laneO-wrongunit scatter-include (default/TrainerPanel <- band3/game/DirectInstrument.cpp).
  // Retail put DirectInstrument's COMDATs inside TrainerPanel's .text span
  // (~DirectInstrument @0x826ccdb8, Enable @0x826ccb60); DirectInstrument.cpp has
  // no splits block of its own.
  #include "band3/game/DirectInstrument.cpp"
```

so `TrainerPanel.obj` **itself defines all 15 DirectInstrument symbols** (verified in its COFF symbol
table) and they paired by name inside TrainerPanel's unit.

⇒ **Pairing fails only when the base obj cannot define the name.** That is exactly the *Splash* half:
`Splash.cpp:`'s compiled object is `build/45410914/src/system/movie/Splash.obj` — the **engine**
`system/movie` TU — which defines **zero** DirectInstrument symbols (verified). So `??0DirectInstrument`
sat in `default/Splash`'s denominator at a permanent **fuzzy 0**, unreachable by any source work.

**That row is the actual result of item 1.** It is now in its own unit at **fuzzy 97.56 %**, 328 B
target / 328 B base — i.e. converted from *structurally unpairable* to *a closable near-miss*. The
+0.00305 pp move in aggregate `fuzzy_match_percent` is the visible trace of that conversion.

### The 328 B is now behind exactly two instructions — a source lane's to collect

Priced from the charged-site list (`objdiff-cli diff` at the graded ruler, read-only, no `--build`),
**not** from a mismatch count. 83 instructions, **two** charges:

```
68 | addi r3, r31, 0x60                      | addi r3, r31, 0x60          |
69 | bl ??0FilePath@@QAA@PBD0@Z              | bl ??0FilePath@@QAA@PBD0@Z  |
70 | mr r4, r3                               | -                           | delete
71 | li r8, 0x0  ... 74 | li r5, 0x1         |  (equal)                    |
75 | -                                       | addi r4, r31, 0x60          | insert
76 | mr r3, r29                              | mr r3, r29                  |
77 | bl fn_82270848                          | bl ?LoadFile@?$ObjDirPtr@VObjectDir@@@@… |   (not charged — placeholder target is forgiven)
```

Retail threads the **`FilePath` constructor's return value** (MSVC returns `this` in `r3`) straight
into the `const FilePath&` argument with `mr r4, r3`; we recompute the address with
`addi r4, r31, 0x60`. That is the signature of a **temporary constructed in the argument position**.
Our source (`src/band3/game/DirectInstrument.cpp:15-16`) uses a named local:

```cpp
FilePath fp(".", path);
mDir.LoadFile(fp, 1, true, kLoadFront, false);      // ours
mDir.LoadFile(FilePath(".", path), 1, true, kLoadFront, false);   // the shape retail emits
```

Destruction timing is already identical on both sides (instructions 78–82 agree), so the rewrite does
not move the `~String` call. **I did not make this edit — `src/` is outside this lane's bar.** Filed
for a source lane; predicted **+328 B / +1 fn**, unmeasured.

**Commit `b03866d2`.**

---

## Item 2 — AR-1: `0x823C38E8` Msg.cpp → CharBlendBone.cpp

### Geometry read before the edit

The pin was **circular**: the single-function `Msg.cpp` block existed *because of* the old, wrong map
name, so W16-AR's corrected name (`?erase@?$list@UConstraintSystem@CharBlendBone@@…@Z`) landed in a
unit whose base obj cannot define it ⇒ permanent fuzzy 0. `src/system/char/CharBlendBone.obj` does
define it (verified in its symbol table).

`CharBlendBone.cpp` was already adjacent on **both** sides — `.text 0x823C3828-0x823C38E8` and
`.text 0x823C3954-0x823C40F0` — so the row was a hole punched out of CharBlendBone. Extended the lower
block's start down to `0x823C38E8` and deleted `Msg.cpp`'s line.

### ⛔ Both of AR-1's stated facts were wrong; both were checked before editing

1. **"144 B" is wrong arithmetic.** `0x823C3954 − 0x823C38E8 = 0x6C = 108 B`.
2. **"This drains `Msg.cpp`'s LAST `.text` block … the whole entry must be removed in the SAME edit"
   is FALSE.** `Msg.cpp` carries **34 `.text` blocks** and 33 `.pdata` lines; removing one leaves 33.
   No unit was drained, so no entry was deleted and the 42-byte-obj / `Invalid COFF/PE section headers`
   hazard the proposal warned about never applied. Had I followed the brief literally I would have
   deleted a 67-line entry covering ~33 unrelated address ranges.

### Predicted vs measured

**Predicted +1 fn / +144 B** (the brief's figure). **Measured +1 fn / +108 B** — which is the *whole*
row, so nothing is missing; the prediction was wrong only because the briefed size was.

```
matched_functions 43,470 -> 43,471
matched_code   4,028,008 -> 4,028,116   (+108)
total_code    10,246,004 -> 10,246,004  (unchanged)
CROSSED IN : 1 row, 108 B  default/CharBlendBone::?erase@?$list@UConstraintSystem@CharBlendBone@@…
FELL OUT   : 0 rows
```

dtk moved `.pdata 0x82205E28-0x82205E30` from Msg to CharBlendBone; `symbols.txt` unchanged.
**Commit `d7f0663e`.**

---

## Item 3 — AR-2: `0x822A8BD0` HamCamTransform.cpp → OutfitConfig.cpp

### Geometry read before the edit (AR left this uncomputed — this is the part AR asked for)

`OutfitConfig.cpp` (44 `.text` blocks) and `HamCamTransform.cpp` (48) are **heavily interleaved**
across this whole region. OutfitConfig's nearest blocks to `0x822A8BD0` are `0x822A68E0-0x822A6A48`
and `0x822ABA10-0x822ABA1C` — far away on **both** sides. ⇒ **No contiguous extension is possible;
this required a new block.** That is the answer to AR-2's open question.

The 128 B HamCamTransform block `0x822A8BC0-0x822A8C40` is **not** one function. Carve:

| retail | size | what |
|---|---:|---|
| `0x822A8BC0` | 4 | `??1?$ObjVector@ULabelStyle@UILabel@@@@QAA@XZ` — **not OutfitConfig's**, fuzzy 100, left alone |
| `0x822A8BC8` | 8 | `except_data` — EH prefix |
| `0x822A8BD0` | **68** | `?resize@?$ObjList@VOldMatOption@@@@QAAXI@Z` — **fuzzy 0** |
| `0x822A8C14` | 40 | `__unwind$` funclet — fuzzy 99.5, **mpn 100** |

Our `OutfitConfig.obj` section 5593 is **116 B** with `resize` at offset 8 and `__unwind$423269` at
offset 76 — **byte-exact** to retail's `0x822A8BC8-0x822A8C3C`. So what moves is the **COMDAT**, not
the row:

```
HamCamTransform.cpp  .text 0x822A8BC0-0x822A8C40  ->  0x822A8BC0-0x822A8BC8   (keeps the 4 B dtor)
OutfitConfig.cpp     NEW BLOCK                        0x822A8BC8-0x822A8C40
```

### ⛔ "328 B" is not the row, the block, or the COMDAT

AR-2 quoted a 328 B ceiling. The body is **68 B**; the whole COMDAT is 116 B; the block was 128 B.
Nothing here is 328 B. The realisable ceiling was **108 B** of countable rows (68 + 40).

### Predicted vs measured

**Predicted:** ceiling **+108 B / +1 to +2 fns**; downside bounded because the funclet books 0 bytes
today. **Measured: +1 fn / +108 B — the ceiling, exactly.**

```
matched_functions 43,471 -> 43,472
matched_code   4,028,116 -> 4,028,224   (+108)
total_code    10,246,004 -> 10,246,004  (unchanged)
CROSSED IN : 2 rows, 108 B  (?resize… 68 B, fn_822A8C14 40 B)
FELL OUT   : 0 rows
```

**+1 fn rather than +2** because the funclet already counted as a matched *function* in
HamCamTransform (fuzzy 99.5 but **mpn 100.0**) while contributing **0** matched *bytes* — the
mpn/fuzzy split doing precisely what CLAUDE.md's DB-4 finding says it does. Predicting the byte and
function deltas separately is what made that legible rather than surprising.

dtk moved the covering `.pdata 0x821F4F40-0x821F4F50` across; `symbols.txt` unchanged.
**Commit `18AA053B`** (`18aa053b`).

---

## Item 4 — AQ's unhomed 28 B thunk at `0x82682688`

### Resolving the two RTTI descriptors

Read out of `.rdata` in Python (never bare `grep` — the shell's `grep` is a ugrep `-I` shim and is
binary-blind). The 28 B body decodes to:

```
lis  r11, 0x82c7
lis  r10, 0x82c7
addi r6,  r11, 0x2528    -> 0x82C72528  ??_R0  .?AVLocalBandUser@@   (TargetType)
addi r5,  r10, -0x1950   -> 0x82C6E6B0  ??_R0  .?AVLocalUser@@       (SrcType)
li   r7,  0              -> isReference = 0
li   r4,  0              -> VfDelta = 0
b    0x8282A0C8          -> __RTDynamicCast   (tail call)
```

⇒ the body **is** `dynamic_cast<LocalBandUser*>(LocalUser*)`.

### The home

`src/band3/game/BandUserMgr.cpp:88-89` is verbatim that function, and `BandUserMgr.h:63` declares it
**`static`**, which fixes the mangling to `SA` (not `QAA`):
`?GetLocalBandUser@BandUserMgr@@SAPAVLocalBandUser@@PAVLocalUser@@@Z`. Our `BandUserMgr.obj` defines
it at **28 B** — retail's size.

**Independent corroboration from geometry alone:** `0x82682688` is *sandwiched between its own two
siblings* — `0x82682668` `?GetBandUser@BandUserMgr@@SAPAVBandUser@@PAVUser@@@Z` (fuzzy 100) and
`0x826826A8` `?GetRemoteBandUser@BandUserMgr@@SAPAVRemoteBandUser@@PAVRemoteUser@@@Z` (fuzzy 100).
Three consecutive 28 B `__RTDynamicCast` thunks; the middle one can only be the Local one. Two
instruments that share no arithmetic agree.

### The edit

```
band3/game/BandUserMgr.cpp:  .text 0x82682668-0x82682684  +  0x826826A8-0x82684E88
                          ->  .text 0x82682668-0x82684E88   (merged, absorbing the thunk + two 4 B pads)
JsonMemory.cpp:              .text 0x82682688-0x826826A4  ->  (deleted)
```

`JsonMemory.cpp` keeps its `0x82C30B08` block, so **no unit was drained** (checked before editing).
This build needed no `.pdata` re-derivation and passed first try; `symbols.txt` unchanged.

### Predicted vs measured

**Predicted Δ0 fns / Δ0 B** — the row was never earning: as `JsonRealloc` it read fuzzy 97.14 /
**mpn 99.29**, i.e. 0 matched bytes *and* 0 matched functions. **Measured exactly that:**

```
matched_functions 43,472 -> 43,472  (+0)
matched_code   4,028,224 -> 4,028,224 (+0)
total_code    10,246,004 -> 10,246,004 (unchanged)
CROSSED IN : 0 rows.  FELL OUT : 0 rows.
fuzzy_match_percent 49.65536 -> 49.655098  (-0.000262 pp)
default/JsonMemory   1/2 fns, 80/108 B  ->  1/1 fns, 80/80 B  = UNIT COMPLETED
```

The −0.000262 pp is the honest cost of moving a spuriously-97 % row into a unit where it reads 0, and
it is bought back many times over by the map rename it unblocks (**AU-2**, predicted +28 B / +1 fn).

### ★ Why that 97.14 % is the most useful thing in this lane

Our `JsonRealloc` (`src/system/net/JsonMemory.cpp:18`) compiles to:

```
lis r11,0 / lis r10,0 / addi r7,r11,0 / addi r5,r10,0 / li r8,0 / li r6,0 / b MemRealloc
```

retail's thunk is:

```
lis r11,0x82c7 / lis r10,0x82c7 / addi r6,r11,0x2528 / addi r5,r10,-0x1950 / li r7,0 / li r4,0 / b __RTDynamicCast
```

**The same seven-opcode sequence**, differing only in register fields and relocation-masked
immediates. A memory reallocator and an RTTI cast thunk are not remotely the same function, yet they
score **97.14 fuzzy / 99.29 mpn** against each other.

⇒ **A near-100 score is not evidence that an identification is correct.** Worse, this row was two
register fields away from being "ground to 100" — which would have produced a byte-perfect match of
the **wrong function**, permanently blessing a wrong map name with the strongest evidence the project
has. Small relocation-heavy thunks are where the score is least informative, because the relocations
that carry all the meaning are exactly what the ruler masks.

**Commit `2909DEC1`** (`2909dec1`).

---

## Item 5 — self-validation of the final splits state

| check | result |
|---|---|
| `total_functions` key vs exact row count | 69,217 == 69,217 **OK** |
| `total_code` key vs exact Σ row sizes | 10,246,004 == 10,246,004 **OK** |
| `matched_functions` key vs rows at `mpn == 100` | 43,472 == 43,472 **OK** |
| `matched_code` key vs Σ sizes of rows at `fuzzy == 100` | 4,028,224 == 4,028,224 **OK** |
| `scripts/verify_objs_patched.py --check` | **rc=0** — fixed point of all 6 post-compile passes |
| object↔target pairing | **1051/1051 declared compiled objects pair (100.0 %)**; 0 objects declared by >1 unit |

Zero rows dropped, and the two headline measures re-derive from the rows on their two *different*
rulers. The pairing count rose to 1,051 with DirectInstrument added, and `--check` reports **0**
objects declared by more than one unit — the doubled-heading defect the brief named
(`UIStats`/`AccomplishmentProgress`/`Game`) does not surface here; those units were left untouched
regardless, as instructed.

---

## Proposals filed, not applied

`docs/decomp/W16AU_MAP_PROPOSALS_FOR_W16AV.json` — two map renames for **W16-AV**, both adjudicated on
retail bytes, neither on map consistency or on a match percentage. No alias is proposed: neither
finding is a fold, so a rename is the honest instrument and an alias would forgive the charge instead
of fixing it.

| id | address | now | proposed | predicted |
|---|---|---|---|---|
| **AU-1** | `0x826CCB48` (8 B) | `??$_Destroy@UGrammar@SpeechMgr@@…` | `?IsLoaded@DirectInstrument@@QAA_NXZ` | +8 B / +1 fn |
| **AU-2** | `0x82682688` (28 B) | `JsonRealloc` | `?GetLocalBandUser@BandUserMgr@@SAPAVLocalBandUser@@PAVLocalUser@@@Z` | +28 B / +1 fn |

Both addresses were re-homed by this lane, so each unit's base obj now *does* define the proposed
name — the rename is the only remaining blocker.

**AU-1's caller adjudication** is the part worth reusing: a whole-`.text` scan for branches targeting
`0x826CCB48` finds **exactly one**, a `bl` at `0x82694C04` inside `?IsLoaded@GamePanel@@UBA_NXZ`.
`GamePanel::IsLoaded` calling `DirectInstrument::IsLoaded` is coherent; calling `_Destroy<Grammar>`
is not. So the *current* name is financed by **zero** call sites, and the un-pairing risk that
normally dominates a map edit (CLAUDE.md: 80.5 % of a map edit's delta) is structurally absent here
rather than merely hoped absent. ⚠ The caller side lands in **GamePanel, which W16-AT owns** — flagged
in the JSON.

---

## NOT done, and why

1. **The `??0DirectInstrument` 328 B fix was not made.** It is a one-line `src/` change (named local →
   temporary in the argument position) and `src/` edits are outside this lane's concurrency bar. The
   two charged instructions, the surrounding context and the exact source line are recorded above so
   a source lane does not have to re-derive them. *What would change this:* a brief that grants
   `src/band3/game/DirectInstrument.cpp`.
2. **The obsolete scatter-include in `TrainerPanel.cpp:465` was left in place.** Now that
   DirectInstrument has its own heading and pairs natively, `#include "band3/game/DirectInstrument.cpp"`
   in TrainerPanel.cpp is redundant — its COMDATs are now unpaired base-side extras, which is harmless
   for the metric (no target row ⇒ no charge) but is dead weight. Removing it is a `src/` edit and
   also touches the second scatter-include in `src/system/hamobj/RhythmDetector.cpp:926`, whose span I
   did **not** analyse. *What would change this:* a lane that owns both files and can A/B the removal;
   I would not remove it blind, because a scatter-include that looks redundant can still be supplying
   a COMDAT some other unit's target rows pair against.
3. **The map renames were not applied** — `scripts/target_symbol_map.json` is W16-AV's. Filed as JSON.
4. **AU-2's callers were not enumerated.** AU-1's were, and its identity partly rests on them; AU-2's
   identity rests on the RTTI descriptors plus the sibling geometry, neither of which needs callers.
   Recorded in the JSON as a known gap rather than implied complete.
5. **The three doubled-heading units (`UIStats`, `AccomplishmentProgress`, `Game`) were not touched** —
   a separately filed defect, per the brief.
6. **`0x826CCA78`'s `$4` vcall-adjustor row was not investigated.** It is TrainerPanel's and stayed
   there; it was read only far enough to prove the DirectInstrument span must not start before
   `0x826CCA88`.
7. **`?Enabled@DirectInstrument@@` and `?SetVolume@DirectInstrument@@`** are defined by our obj but
   have no retail address in this region — not pinned, not investigated. They are presumably inlined
   into callers or folded elsewhere. *What would change this:* a byte-signature search for their
   COMDATs across retail `.text`.
8. **No `src/` file, no map file and no alias file was modified by this lane.** `git diff --stat`
   against the lane base touches `config/45410914/splits.txt` and two new `docs/decomp/` files only.

---

## Gates

Run in the worktree in the brief's order, native gate last.

| gate | result |
|---|---|
| full build | **`rc=0`** — final measures 43,472 / 4,028,224 B / 39.315075 %, `total_code` 10,246,004 |
| `scripts/verify_ruler_agreement.py --check` | **`rc=0`** — both objdiff-cli entry points resolve the same ruler (all four keys pinned: `functionRelocDiffs=name_check`, `combineDataSections`, `combineTextSections`, `ppc.calculatePoolRelocations=false`) |
| `scripts/verify_objs_patched.py --verify-manifest` | **`rc=0`** — 1,215 decomp + 3,114 target objects match, `tree_sha256=1ef7674f3eb6f7fb`; denylist OK (6 addresses, none named in 3,114 target objs over 495,651 symbols) |
| `scripts/verify_objs_patched.py --check` | **`rc=0`** — fixed point of all 6 post-compile passes; 1051/1051 declared objects pair |
| `tools/native_build_gate.sh` | **PASS, 0 SKIPs** — line below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`symbols.txt` is unmodified at the end of the lane (every split reached a fixed point on its retry;
none needed more than one). The only tracked file this lane changed outside `docs/` is
`config/45410914/splits.txt`. The native gate was run as the last build-affecting action; the commit
that follows it touches `docs/` only, which the native build does not read.

## Commits

| sha | item |
|---|---|
| `b03866d2` | item 1 — DirectInstrument heading (Δ0, accuracy; 328 B row made closable) |
| `d7f0663e` | item 2 — AR-1 re-home (+1 fn / +108 B) |
| `18aa053b` | item 3 — AR-2 COMDAT re-home (+1 fn / +108 B) |
| `2909dec1` | item 4 — thunk `0x82682688` homed to BandUserMgr (Δ0; unit completed, rename unblocked) |
