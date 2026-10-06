# W16-QR — NoteTube bodies and the three suspect pins from W16-QP (2026-10-06)

Branch `w16-qr` off main `cecef177b`. Worktree `~/tmp/wt-w16qr`.
Brief: the two NoteTube partials W16-QP §4.2 left as body work, and the three suspect pins
W16-QP §7 recorded without fixing.

| item | before | after | how |
|---|---|---|---|
| `?AllocateFaces@TubePlate@@QAAXH_N@Z` (272 B) | 86.74 | **100.0** | source, `728639516` |
| `?DrawToPlate@NoteTube@@QAAXPAVTubePlate@@@Z` (3,184 B) | 84.54 | **99.61** fuzzy / 99.67 mpn | source, `1d1adcfa0` |
| `LabelNumberTicker` `.text` upper edge | `0x82829218` | `0x828290A0` | pin, `f788b7ad1` |
| `?Run@App@@QAAXXZ` map address | `0x82829368` | `0x822703D0` | map, `f788b7ad1` |
| stray `RhythmDetector` block `0x822703A8`–`0x82270414` | RhythmDetector | `App.cpp` | pin, `f788b7ad1` |

Measured whole-binary with `tools/ab_measure.py`, graded `name_check` ruler, §4:
- source **+1 fn / +272 B**, as predicted;
- pins **+0 / +0 B**, as predicted, and **+2 units at 100** (mpn ruler).

## 1. `TubePlate::AllocateFaces` — 86.74 → 100

Three defects, all visible in the retail listing:

1. **No warning.** Retail has no `MILO_WARN` call. In the match build `MILO_WARN` is
   `MiloStripEval(...)`, which still *evaluates* its arguments (`faces.capacity()`), so the
   call was not codegen-neutral. The sibling `AllocateVerts` was already at 100 with no warning,
   which made it the control.
2. **Re-reads after the `ceil` call.** Retail loads `mAllocationCount` and the vector's
   capacity again after `ceil` returns, instead of keeping the values from before the call.
   Our source cached both in locals (`count`, `cap`). The body now spells
   `mAllocationCount * steps + faces.capacity()`.
3. **Operand order of the size sum.** `faces.size() + num`, with `cap` read first.

## 2. `NoteTube::DrawToPlate` — 84.54 → 99.61

The W16-QP flag was "constant-order SWAP". The real defects were structural, and the SWAP
rows disappeared once they were fixed. Retail-driven changes (commit `1d1adcfa0`):

- **Pitched branch:**
  - The allocation is `(numPoints + 2) * 2`.
  - Points are read as `Vector3` copies (`cur = mPoints[i]`, `next = mPoints[i + 1]`).
  - The loop bound is the unsigned `mPoints.size()` compare.
  - Vertex slots are computed from `i` (`vertStart + i*2 + 2/3`) instead of a running index.
  - The half-width is `unk_0x30 * tan(atan(dz/dx) * 0.5f)`.
  - The closing vertices are relative to `lastVert = vertStart + numPoints*2`.
  - The closing x value is `(baseX + (2*unk_0x30 + last.x)) - 1/64`.
- **Column branch:**
  - The loop has no running `vertIdx`; slots are `vertStart + i*2 (+1)`.
  - `u` is `(uvScale * (1/length) * i) * kMaxQuadSize`. The reciprocal is grouped, so
    `/fp:fast` hoists it.
- **8-vertex branch:**
  - `numVerts = 8` is a local.
  - The four closing writes are relative to `lastVert = vertStart + numVerts`.
  - `x0 ± 0.05f` is written inline. The `x0High` local is gone: it moved a temp number and
    flipped an `fadds`.

### Residual: 10 rows, all floor-class

These are listing indices from `objdiff-cli diff --full-listing` on the graded ruler:

| idx | retail | ours | class |
|---|---|---|---|
| 296 | `add r5, r8, r11` | `add r5, r11, r8` | commutative operand order |
| 412 | `fmuls f13, f28, f10` | `fmuls f13, f10, f28` | commutative operand order |
| 422 | `fmadds f9, f0, f12, f9` | `fmadds f9, f12, f0, f9` | commutative operand order |
| 687/690/693 | `lwz r11, 0xd8(r28)` before the `add` | after | scheduling (one load) |
| 744/745 | `stfs f11, -0x20` then `-0x1c` | reversed | store order of two `tex` fields |
| 773, 777 | `add r7/r5, r27, r11` | `r11, r27` | commutative operand order |

Why this is recorded rather than ground further:

- **MSVC fixes commutative operand order by internal temp numbering, so swapping the source
  operands does nothing** (`docs/decomp/patterns/fixable-liveness.md`, Triage Split).
- What was measured with a 0.6 s scratch compile, which agreed exactly with the full-build
  scores on every probe:
  - a single-toggle sweep of 14 independent spellings;
  - all 125 combinations of face-index spellings, every one inert;
  - an exhaustive 256-combination sweep over 8 local-introduction toggles, none better than
    the landed form;
  - about 20 hand variants of the `u` expression.
- One variant regressed hard: a named `invLength` local dropped the row to 93.67.
- Every lever that moved these rows also moved others. The landed form is the maximum found.

`matched_code` is all-or-nothing per row, so this function earns **no bytes** until it reaches
100. The work is still a correctness gain: the old body allocated the wrong vertex count in the
pitched branch and indexed vertices from a separate counter.

`?CreateMeshes@NoteTube@@QAAXXZ` (80.71) was out of scope and is untouched.

## 3. The three suspect pins

### 3.1 `LabelNumberTicker` ran 0x178 bytes into RAD and CRT code

The pin was `.text 0x82827A20`–`0x82829218`. The last four functions inside it are not
LabelNumberTicker's:

| addr | size | what it is | evidence |
|---|---|---|---|
| `0x828290A0` | 20 | `RADSetMemory`-shaped | stores two function pointers into `lbl_82E07E98` (the alloc/free hook pair) |
| `0x828290B8` | 196 | `radmalloc`-shaped | 64-byte aligned allocation through the hook, or `fn_8282B170`, with a type-3 tag |
| `0x82829180` | 56 | `radfree`-shaped | frees through the hook |
| `0x828291C0` | 88 | `type_info` deleting dtor | its vtable `0x8212B484` is the one every `.?AV…@@` TypeDescriptor in `.data` points at (`.?AVFilePath@@`, `.?AVString@@`); it calls `fn_8282F9B8` under lock `0xe` |

- The only callers of the first three are in `auto_04_82C4D000_BINK`: 11, 4 and 1 sites.
- `0x82829218` onward is the CRT `__savegprlr`/`__restgprlr` sled.
- **Fix:** the upper edge moves to `0x828290A0`. The unit drops 63 → 59 rows; all four were
  at 0%. The four become `auto_*`, which is where unattributed RAD/CRT code belongs.
- They were **not named**. Naming them would be guesswork about RAD's private symbols.

### 3.2 `?Run@App@@QAAXXZ` was on a CRT stub; `App::Run` is `0x822703D0`

`0x82829368` is `b fn_828292D0` plus a pad word. The `.rdata` object `lbl_8212B488`, directly
after the `type_info` vtable, points at it. `fn_828292D0` fills the CRT function table at
`lbl_82C79D00`. Nothing about it is `App::Run`.

The real `App::Run` follows from `main` (`Main.s`, `fn_82272E68`), which calls, in order:
1. `fn_82270E68` (`App::App`);
2. `fn_822703D0`;
3. `fn_82270000` (`App::~App`).

`fn_822703D0` (68 B):
- stores `this` to `lbl_82CBC600`;
- calls `fn_8283C6B0(fn_822703A8)`, which is `SetUnhandledExceptionFilter`: it swaps
  `lbl_82E5A2A8`, which the CRT filter invoker calls with an `== -1` check;
- stores the previous filter to `lbl_82CBC60C`;
- executes `*(int *)0 = 1`.

That is the `__try`-less "install a filter, then fault into it" idiom. `fn_822703A8` (36 B),
the filter, restores the old filter and calls `fn_82270080(app)`; no epilogue follows the call.
`fn_82270080` (264 B) is an infinite loop: `SystemPoll(false)`, the manager polls, then
`DrawRegular` (`0x82270018`). That loop is `App::RunWithoutDebugging`, which `src/App.h:17`
declares. DC3's `os/Debug.cpp:388` installs its handler the same way
(`SetUnhandledExceptionFilter(&HmxGlobalHandler)`).

**Fix:** the map entry moved to `0x822703d0`. The row reads 0% because `src/App.cpp` does not
define `App::Run` yet.

### 3.3 The stray `RhythmDetector` block was App's filter and `App::Run`

`.text 0x822703A8`–`0x82270414` under `RhythmDetector.cpp` covered exactly the filter
(`0x822703A8`) and `App::Run` (`0x822703D0`) above.
- RB3's `band.exe` contains **0** occurrences of the string `RhythmDetector`;
  `RhythmDetector.cpp` is DC3 `hamobj` code.
- **Fix:** the block moves to `App.cpp`, together with the App gap `0x82270000`–`0x82270188`
  (`~App`, `DrawRegular`, the 264 B loop).
- dtk re-derived `.pdata` for both blocks (`0x821F1600`–`0x821F1610` and
  `0x821F1638`–`0x821F1648`, now under App).

What stays under RhythmDetector, deliberately:
- The seven `ObjDirPtr<ObjectDir>` COMDAT rows, all at 100. W16-BA kept them there on
  purpose via a DirectInstrument scatter-include; moving them costs −6 rows / −344 B, and
  W16-NE §6 left them open.
- `0x8227173C`–`0x82271748` is a pad word plus the 8-byte EH prefix of the function at
  `0x82271748`. It has no row, so it was left alone.

After the move the App unit holds five rows at 0%:

| row | size |
|---|---|
| `??1App@@QAA@XZ` | 20 |
| `?DrawRegular@App@@IAAXXZ` | 104 |
| `fn_82270080` | 264 |
| `fn_822703A8` | 36 |
| `?Run@App@@QAAXXZ` | 68 |

All five are source work for a later lane.

## 4. Measurements

Both runs used `tools/ab_measure.py` on the worktree, graded `name_check` ruler. Both legs
were settled and measured in-run. Both runs were taken **in the revert direction**, so the
forward delta is the negation of the tool's printout.

| change | how measured | predicted (forward) | measured (forward) |
|---|---|---|---|
| source `728639516` + `1d1adcfa0` | `--patch` of the reverse diff `f788b7ad1..cecef177b -- NoteTube.cpp` | +1 fn / +272 B (AllocateFaces only; DrawToPlate stays below 100 on both rulers) | **+1 fn / +272 B**, Δcode% +0.002651 pp, Δfuzzy +0.004440 pp, all in `default/NoteTube` (22 → 23) |
| pins `f788b7ad1` | `--revert f788b7ad1` (map + splits; forced re-split, `symbols.txt` fixed point on both legs) | +0 / +0 B (the moved rows are 0% on both sides) | **+0 fn / +0 B / +0.000000 pp**. Units at 100: mpn ruler **+2** (`LabelNumberTicker` 63 → 59 rows, `RhythmDetector` 9 → 7 rows, both now all-100); fuzzy ruler **+1** (`RhythmDetector`) |

- Both unit completions come from the **denominator shrinking**: foreign 0% rows leaving a
  unit. No row's score moved.
- The pairable-unit count drops by one (1,753 → 1,752) because units merged, not because
  coverage changed. A unit-set diff of the two archived legs shows:
  - `auto_03_82270000_text` was absorbed into `App` (17 → 22 rows: 3 from it, 2 from
    RhythmDetector);
  - `auto_03_82829218_text` and the LabelNumberTicker tail re-split as
    `auto_03_828290A0_text`;
  - the `.pdata` auto units renamed to follow.
- The tool's absolutes at leg A were 54,632 matched / code% 58.500015 / fuzzy 64.126660. They
  are quoted for orientation only: deltas compose, absolutes do not.

Native gate, run on this worktree with the final `src/`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The pins commit needed one extra build. The split-guard failed the first build because dtk
re-derived the `.pdata` lines for the three touched units. That rewrite was inspected (only
`.pdata` moved, exactly following the `.text` edits) and committed together with the pins.
The next build was rc=0, and `symbols.txt` did not change.

## 5. Deliberately not done

- **`App::Run`, the exception filter and `RunWithoutDebugging` were not written.** The filter
  has no epilogue, which needs `RunWithoutDebugging` to be no-return, and the draw path in
  `src/App.cpp` is unwritten. This is a source lane of its own, and the bodies above give it
  its shape.
- **No RAD or CRT row was named.**
- **The RhythmDetector `ObjDirPtr` COMDATs were not moved**, per W16-BA.
- **`CreateMeshes` (80.71) was not touched.**
- **No permuter.** The DrawToPlate residual is the operand-order and scheduling class that the
  permuter policy defers.
