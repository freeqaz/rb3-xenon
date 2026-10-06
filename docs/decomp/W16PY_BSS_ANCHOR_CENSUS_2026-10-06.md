# W16-PY — `.bss` anchor census and the rows the declaration-order knob reaches (2026-10-06)

Lane W16-PY, branch `w16-py`, worktree `~/tmp/wt-w16-py`, rebased onto main `a3f3339a8` (clean retail TU5 image).
Brief: lever 5 of `CAMPAIGN_STATE_2026-10-06.md`. Build an instrument that lists in-scope rows whose only charge is
an offset off a shared `.bss` anchor, then fix the rows that W16-PG §2.1's mechanism reaches. That mechanism is an
explicit `= 0`, which makes declaration order the ascending `.bss` order.

W16-PS (`c8e56eba1`) landed while this lane ran and took part of the same lever. Its anchor census covered the 95
rows from levers 4 and 5. This census covers the whole gap. §3 compares the two.

## 1. Result

```
================ A/B RESULT (MEASURED) ================
  patch: w16py-rnd-utl (2459647428c56f04)  kinds: ['source']
  leg A: matched=53516 masked=25193 honest=28323 code%=57.941143  (recompiles: 0, settled)
  leg B: matched=53519 masked=25193 honest=28326 code%=57.949657  (recompiles: 8, split=0, patch_steps=6, settle iterations: 2)
  Δmatched=+3  Δmasked_equal=+0  Δhonest=+3  Δcode%=+0.008514pp  Δcode_bytes=+872
  units at 100% [mpn ruler]: legA 553 -> legB 553
```

- The run was `tools/ab_measure.py --worktree ~/tmp/wt-w16-py --revert <tmp>`. The tmp commit put main's
  `Rnd.cpp`/`Lit.cpp`/`Utl.cpp` back, so leg A is main `a3f3339a8` and leg B is this branch. Ruler `name_check`.
  Run dir `.ab_measure_runs/20261006-103514-w16py-rnd-utl-979533`.
- **Prediction before the run:** +3 fns / +872 B (664 + 100 + 108). **Measured:** exactly that.
- Row-level diff of the archived legs: **3 UP, 1 DOWN, 0 GONE, 0 NEW.**

| row | size | fuzzy before → after |
|---|---:|---|
| `Rnd::Modal` | 664 | 99.976 → **100** |
| `CompressThread` (Rnd.cpp) | 100 | 99.880 → **100** |
| `RndUtlTerminate` | 108 | 99.926 → **100** |
| `Rnd::DrawPreClear` | 640 | 80.794 → 80.775 (**DOWN**, 0 bytes) |

- **The DOWN row is a coincidence that went away.** Our `DrawPreClear` body (inside `#ifndef HX_NATIVE`) uses
  `gRndTextureEvent` where retail tests `sCompressDone`, stores `sTexture` and reads `sCompressData`. Before the fix,
  one of those accesses happened to carry the same displacement as retail's while naming a different static. With
  the statics in retail's order, that coincidental match is gone. The row is a behaviour divergence (§5), not a
  layout row.
- The branch's own full build reads 53,519 / 5,938,560 B, which is leg B exactly.
- Native gate: `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

All three rows are in the VIA-DC3 ring (`rndobj/`). The user's 2026-10-06 answer funds that ring alongside in-scope
work. The edits only change `.bss` placement: each static still starts at zero, so native behaviour is unchanged.

## 2. The instrument: `tools/bss_anchor_census.py`

**Population.** Reachable gap rows (`tools/ceiling_recompute.py`'s scaffold rule), tiered with W16-OV's ring split
(`scripts/native_scope_map.py` plus the Quazal/NET/XDK/VIA-DC3 split). `--scope in` keeps IN-CORE, IN-SOON and
IN-RB3ENG; `--scope all` keeps every reachable gap row.

**Per row.** The tool runs the graded `objdiff-cli diff` (objdiff.json's own options) and classifies every charged
instruction:

- **ANCHOR_OFF:** a `diff_arg` whose differing arguments are all immediates, on a load, store or `addi` whose base
  register traces back, on *both* sides, to an `addi rA,rX,SYM@l`. The trace is a linear backward walk to the
  register's last definition and follows `mr`.
- **ANCHOR_SYM:** a `diff_arg` on the symbol of the anchor-forming `addi`/`lis` itself, with both sides data symbols.
- **OTHER:** everything else.

A row is **PURE** if every charge is ANCHOR_* and at least one is ANCHOR_OFF. It is **MIXED** if it has some
ANCHOR_OFF charges and some OTHER.

**Which static, at which retail address.** For each anchored site the tool resolves two things:

- which of *our* symbols lives at `our_anchor + off`, from the object's COFF symbol table;
- the retail address the same instruction reached, from `symbols.txt` or the address encoded in a placeholder name,
  plus `off`.

**Sub-class.** For each site, the slide is `retail address − our section offset`. Uncharged anchored accesses are
collected as well, because they witness the slide for statics that already agree.

| sub-class | rule | reading |
|---|---|---|
| **LAYOUT** | each of our symbols has one slide, but symbols disagree | our statics sit in a different order or spacing than retail's. This is the only class the `.bss` knob can reach. |
| **ANCHOR_CHOICE** | one slide for everything | same layout; the compiler picked a different static as the anchor |
| **FIELD** | one of our symbols is reached at more than one slide | retail reads different offsets inside one object (member order, expression order, or a different variable) |

**Controls**

- **`--selftest`.** Builds the ChordShapeGenerator shape by hand and requires:
  - it fires there and classifies LAYOUT;
  - it is silent on three negatives: an unanchored base register, an immediate on a compare, and an anchor
    register redefined in between;
  - the charged sites alone read ANCHOR_CHOICE. This guards the first version's defect, described below.
  - the XfmSort shape (one `Vector3`, components swapped) reads FIELD.
- **Real-data positive control.** `PreInitSystem` (W16-PG §3: retail `0x0(r28)`, ours `0x8(r28)`) comes out PURE
  ANCHOR_CHOICE. Retail anchors on `gUsingCD` and reaches `gSystemConfig` at −8; we anchor on `gSystemConfig`.
- **The tracer is not vacuous.** I checked this because the first in-scope run reported **0 MIXED**. At main
  `7f41fe33e`, the in-scope gap held **667** charged immediate diffs on a load/store/`addi`-shaped instruction:
  - 10 anchored on both sides;
  - 106 directly off `r1`;
  - 87 not memory-shaped;
  - **464** traced to a non-anchor definition, which are other classes:
    - frame pointers (`subi rX,r1`): 249;
    - `addi` without `@l` (member or sub-object pointers): 72;
    - `subf`: 52;
    - other `subi`: 28;
    - parameter registers with no definition: 27;
    - `mr` from a non-anchor: 21;
    - loaded pointers: 10;
    - `add`: 4.
- **Direct accesses cannot carry this charge.** For `SYM@l(rA)`, the offset lives in the relocation, and
  `name_check` forgives it against a placeholder retail name.

**Two defects found in the instrument, both fixed**

1. **The first sub-classifier used charged sites only.** Then a single moved static always reads as one slide, which
   is the ChordShapeGenerator shape mis-read as ANCHOR_CHOICE. The selftest caught it. Uncharged anchored accesses
   are now collected as witnesses.
2. **MSVC emits no COFF symbol for an internal static that the TU reaches only off another static's anchor.**
   Examples: `sSphereDir` in Utl.obj, and `sCompressDone`/`sCompressData` in Rnd.obj before the fix. The resolver
   then bills those bytes to the preceding symbol as `sym+N`, which can turn a LAYOUT row into a FIELD row.
   - **Measured precision of FIELD on the final base census: 1 false of 4.** `CompressThread` reads FIELD and was
     fixed to 100 by static order.
   - The other three FIELD rows were checked by hand and are genuine (§4).
   - ⇒ **Hand-check every FIELD row before closing it.** This is a property of the object file and cannot be fixed
     from COFF alone.

## 3. Population

**Final base census**, run on the A/B's leg A tree (main `a3f3339a8` source, clean TU5 image, 53,516 fns /
5,937,688 B). The census was the only process running in the worktree.

```
python3 tools/bss_anchor_census.py --worktree . --scope all
population: 2348 rows / 684860 B (gap 2348 rows, reach 6622548, matched-in-reach 5937688 = report matched_code)
```

| class | rows | bytes |
|---|---:|---:|
| PURE LAYOUT | 2 | 772 |
| PURE ANCHOR_CHOICE | 2 | 880 |
| PURE FIELD | 2 | 560 |
| MIXED LAYOUT | 1 | 3,024 |
| MIXED ANCHOR_CHOICE | 1 | 1,440 |
| MIXED FIELD | 2 | 756 |
| **total** | **10** | **7,432** |

**In scope: 2 rows / 880 B.** Both are `PreInitSystem` (652) and `InitSystem` (228), PURE ANCHOR_CHOICE in
`os/System.cpp`. At main `7f41fe33e`, before W16-PS, the in-scope census was **828 rows / 257,812 B → 3 PURE rows /
976 B**. The third row, `MemFindAddrHeap`, was then fixed by W16-PS (§3.1).

**Every row, with its disposition**

| row | ring | size | fuzzy | class | disposition |
|---|---|---:|---:|---|---|
| `Rnd::Modal` | VIA-DC3 | 664 | 99.976 | PURE LAYOUT | **fixed** (§4) |
| `RndUtlTerminate` | VIA-DC3 | 108 | 99.926 | PURE LAYOUT | **fixed** (§4) |
| `CompressThread` | VIA-DC3 | 100 | 99.880 | PURE FIELD (false; really LAYOUT) | **fixed** (§4) |
| `PreInitSystem` | IN-CORE | 652 | 99.982 | PURE ANCHOR_CHOICE | stop: W16-PG (5 knobs) and W16-PS (12 variants) measured it inert; the layout is already correct |
| `InitSystem` | IN-CORE | 228 | 99.895 | PURE ANCHOR_CHOICE | same |
| `EstimateDraw` | VIA-DC3 | 460 | 99.948 | PURE FIELD | out of mechanism: `gNgStats` is an `NgStats[3]`, and retail reads `mParts` first where ours reads `mLightsApprox` first (expression order inside one object) |
| `LightPreset::Load` | VIA-DC3 | 3,024 | 99.595 | MIXED LAYOUT (17 anchored, 66 other) | stop after 3 variants (§4) |
| `Character::PostLoad` | VIA-DC3 | 1,440 | 99.647 | MIXED ANCHOR_CHOICE | not tried. Same layout, different anchor (retail `gRevs`, ours `gCharMe`), the class two lanes measured inert |
| `Rnd::DrawPreClear` | VIA-DC3 | 640 | 80.794 | MIXED FIELD | behaviour lead (§5) |
| `XfmSort` | VIA-DC3 | 116 | 96.931 | MIXED FIELD | out of mechanism: retail reads `gUtlXfms` +8 where ours reads +4 (a `Vector3` component), with 10 other charges |

### 3.1 Against W16-PS's census

- W16-PS's `anchor_census.py` (scratch, not landed) classed the **95** rows of levers 4 and 5 and found 2
  ANCHOR-only rows, `MemFindAddrHeap` and `InitSystem`. It called `PreInitSystem` MIXED, counting its stack
  immediates; this tool counts only anchored sites, so it reads PURE here.
- This census runs over **every** reachable gap row, 828 in scope and 2,348 whole-binary. **In scope it finds the
  same rows and no others**, so W16-PS's population was not missing anything there. In scope, the lever is drained:
  `MemFindAddrHeap` is fixed, and the remaining two are anchor choice, which is inert.
- What W16-PS's population could not see is the **VIA-DC3 ring**: 8 rows / 6,552 B. Three of them were reachable
  and are fixed here.
- I had independently fixed `MemFindAddrHeap` with a 5-word stand-in between `gHeaps` and `gNumHeaps`, the same fix
  as W16-PS. It read 100 graded and was dropped as a duplicate when W16-PS landed.

## 4. What the mechanism did and did not reach

**Rnd.cpp (fixed `Modal`, `CompressThread`).**
- Retail's block at `0x82CC2410` is:
  - `sTexture` +0, `sCompressDone` +4;
  - `gNotifyKeepGoing`/`gFailKeepGoing`/`gFailRestartConsole` at +5/+6/+7. Modal reaches them at −2/−1/0 off
    `gFailRestartConsole`.
  - `sCompressData` +8, `gRndHandles` +0xC/+0x10.
- Ours (plain statics, code generator's order): `sTexture` 0, `gRndHandles` 4, `gRevs_Lit` 0xC,
  `gFailRestartConsole` 0x12, `gFailKeepGoing` 0x13, then the two compress statics (no symbols) and
  `gNotifyKeepGoing` 0x19.
- **Step 1:** zero-initialise all seven and declare them in retail's order.
  - Result: declaration order held, but Rnd.obj placed `gRevs_Lit` (6 B, from the Lit.cpp scatter include at the
    end of Rnd.cpp) **first**, and packed two of the bools into its 2-byte tail hole.
  - Modal 99.976 → 99.988; CompressThread 99.880 → **99.840**.
- **Step 2:** zero-initialise `gRevs_Lit` too.
  - It is then defined at its (last) declaration, and the block comes out byte-for-byte retail's relative layout.
  - Both rows reach 100. `RndLight::Load`/`RndLightAnim::Load` and their thunks stay at 100.

**Utl.cpp (fixed `RndUtlTerminate`).**
- Retail keeps `sSphereDir` in the word below `sSphereMesh` (`0x82CC2AD0`/`0x82CC2AD4`). Ours had it at
  `sSphereMesh − 0x24`.
- `= nullptr` on the two puts them adjacent in declaration order, and the row reaches 100.

**LightPreset::Load (stopped).**
- Retail: `sPresetAltRev` at `0x82CC6E8C`, an **unlabelled, unreferenced hole** at +2, `sPresetRev` at +4,
  `sLoading` at +6 in `rev`'s tail.
- Ours, already zero-initialised and declared altRev, rev, sLoading: altRev 0x24, `sLoading` **0x26**, rev 0x28.
  **So `= 0` does not make declaration order the `.bss` order in this TU.** W16-PG's rule is not universal.

| variant | layout | Load | other rows |
|---|---|---|---|
| (A) `sLoading` declared first | 0x24 / 0x28 / 0x2c, each in its own 4-byte slot | 99.595 (unchanged) | unchanged |
| (B) `{altRev, pad, rev}` aggregate + `sLoading` after (the `gRevs_Lit` pattern) | **retail's exactly** (0x24 aggregate, `sLoading` 0x2a) | 99.595 (unchanged) | **−3 rows at 100.** Retail reads `rev` with its own relocation (`lbl_82CC6E90@l`), so the aggregate is **refuted on retail bytes** |
| (C) no initialisers | `sLoading` 0x1, `rev` 0x20, `altRev` symbol-less | 99.595 (unchanged) | unchanged |

- Reverted to the committed source.
- The 66 other charges cap the row below 100 anyway. A layout that puts something else into the +2 hole first (as
  retail evidently had) was not found.

**Measured placement facts, for the next lane**

- Zero-initialised statics took declaration order in Rnd.obj and Utl.obj. They did **not** in LightPreset.obj.
- A small object fills an earlier alignment hole: two bools went into `gRevs_Lit`'s tail, and `sLoading` into
  `altRev`'s.
- An `unsigned short` static gets its own 4-byte slot, so `altRev`/`rev` sit 4 apart, as in retail.

## 5. Leads (not this lever)

- **`Rnd::DrawPreClear` (640 B, 80.8%) is a garbled port.** Inside `#ifndef HX_NATIVE`, the body:
  - tests `(unsigned char)gRndTextureEvent`;
  - passes the event handle to `ReplaceObject`;
  - casts it to a `CompressTextureCallback`.

  Retail's anchored accesses are `sCompressDone`, `sTexture` and `sCompressData`. It needs a body port read off
  retail bytes. Native compiles the other arm, so this is metric and correctness of the match build only.
- `EstimateDraw` (460 B): the evaluation order of the twelve terms. A source-order fix is plausible, but it is an
  expression-order question, not a `.bss` question.

## 6. Not done

- No permuter. Nothing in `src/network` or `src/xdk`.
- `Character::PostLoad` (ANCHOR_CHOICE) was not tried, per the inert verdicts on the same class from W16-PG and
  W16-PS.
- FIELD rows were hand-checked here, but the tool cannot settle them alone (§2, defect 2).
- Scratch: census outputs `~/tmp/w16py_census_*.{txt,json}` (not committed). The final base run is
  `w16py_census_final_base.*`.
  - **`w16py_census_all3.*` is contaminated:** it ran concurrently with a Rnd.cpp rebuild. Do not cite it.
