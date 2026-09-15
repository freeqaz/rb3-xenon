# W16-BW — DSP residue: IIR4PoleFilter layout, VibratoDetector::Detect, AnalyzeBlock, Time2IirA

**Lane:** W16-BW · **Branch:** `w16-bw` · **Base:** main `4ffe3db4fc08` (verified = `git merge-base main w16-bw`)
**Date:** 2026-09-15 · **Ruler:** `name_check` (graded), objdiff 4.2.9 · **Worktree:** `/home/free/tmp/wt-w16-bw`

---

## §0 — Baseline, tested literally

The brief's §Baseline table was re-read out of the live `report.json` in this
worktree (`int()`-coerced; sizes are JSON strings) **before** any edit. Every
figure survived; nothing in the brief was stale.

| row | unit | size | fuzzy | mpn | brief said | survived? |
|---|---|---:|---:|---:|---|---|
| `?Detect@VibratoDetector@@QAAHXZ` | `default/system/dsp/VibratoDetector` | 376 | 20.8617 | 22.4574 | 20.8617 / 22.46 | ✅ |
| `??0IIR4PoleFilter@@QAA@PAM0@Z` | `default/system/dsp/IIRFilter` | 312 | 10.1923 | 11.8590 | 10.1923 / 11.86 | ✅ |
| `?AnalyzeBlock@PitchDetector@@QAAXPBDPAFHMMAAM22@Z` | `default/system/dsp/PitchDetector` | 1780 | 85.8584 | 86.6000 | 85.8584 / 86.60 | ✅ |
| `?Time2IirA@?A0xa7b3dd7d@@YAMMM@Z` | `default/Synapse_dsp` | 92 | 100.0 | 100.0 | 100.0 / 100.0 | ✅ |

Whole-binary baseline, same read: **43,654 matched_functions / 4,055,524 matched_code B /
10,247,068 total_code = 39.577408 % / fuzzy 49.827557**. Matches the brief exactly.

`?ShiftedDotProduct@@YAXPBMHPAM_N@Z` was **not touched** — out of scope per the brief,
and I found no evidence against the documented VMX128 wall (see §6).

---

## §1 — Task 1: the `0x60..0xE0` identification (the xref width sweep)

### 1.1 The instrument, and the population it runs over

BT §8 named the instrument (an xref width sweep) but **not** the population. Running it
over the whole binary first is what makes the answer decisive, so that came first:

| question | method | result |
|---|---|---|
| how many retail bodies does `IIR4PoleFilter` have? | `.pdata`-bounded extents + `symbols.txt` | **exactly 2** |
| ctor | — | `0x82B815B8`, 312 B |
| `FilterSlow` | — | `0x82B81538`, 128 B |
| `Begin` / `End` | — | **ICF-folded empties** at `fn_826C3888` (no bodies of their own) |
| `bl` callers of the ctor | whole-`.text` `bl` sweep over `orig/45410914/band.exe` (Python, **not** the grep shim) | **1** — `0x82B811DC` (`PitchDetector` ctor) |
| `bl` callers of `FilterSlow` | same | **2** — `0x82B80AB0`, `0x82B80AE8` (both inside `AnalyzeBlock`) |
| how is the object reached? | xref of `PitchDetector::mFilter` | every dereference is `lwz r3,0(rX)` + `bl` to one of those two |

⇒ **The two bodies above are the only code in the binary that can touch an
`IIR4PoleFilter`.** That bounds the sweep to 440 bytes of retail code, which is why the
result is a proof and not a sample.

### 1.2 The width/stride table

Every instruction inside those two bodies that addresses `+0x60..+0xDF`:

| form | width | count | where | displacements |
|---|---:|---:|---|---|
| `std` (D-form) | 8 B | **4** | ctor `0x82B816D8..0x82B816E4` | `0x80`, `0x88`, `0xd0`, `0xd8` |
| VMX128 `stvx`-class (op4, X-form, indexed on `rA=r3`) | 16 B | **5** | ctor | `rB` ∈ `{0x70, 0x90, 0xa0, 0xb0, 0xc0}` (from `li`/`addi` into r4/r7/r9/r10/r11) |
| **any load of any width** | — | **0** | — | — |
| **anything at all at `+0x60`** | — | **0** | — | — |
| `FilterSlow`, anywhere in `0x60..0xDF` | — | **0** | — | — |

⚠ **Methodological note — the displacement sweep alone is BLIND to most of this table.**
VMX128 `stvx` is **indexed**: the offset lives in `rB`, not in an immediate field. A sweep
keyed on D-form displacements returns only the 4 `std` and would have reported
"0x70/0x90/0xa0/0xb0/0xc0 are never touched" — a false negative shaped exactly like a
decisive result. The X-form half must be recovered by tracking the `li`/`addi` immediates
that feed `rB`. Both halves are in the table above; neither alone is the answer.

### 1.3 The verdict — BT §8's hypothesis is REFUTED

BT §8 hypothesised: *"if the sweep shows only `lvx`/`stvx`-width access it is scratch the
ctor may legitimately not initialise … the row is **not** layout-blocked at all."*

The sweep shows the opposite of the load-bearing half. The tail **is** written at
`lvx`/`stvx` width — but it is **never read, by any instruction, at any width, anywhere in
the binary**. It is not scratch that the ctor may skip; it is a **dead SIMD mirror of the
scalar coefficients that the ctor is REQUIRED to write** (and that our ctor was not
writing at all, which is the entire 80-byte shortfall). The row **was** layout-blocked.

★ The general shape worth carrying: *"never read"* and *"need not be written"* are
different claims, and only the first one follows from an xref sweep. A write-only region
is still fully load-bearing for a **matching** target, because matching prices the writes.

### 1.4 The layout, confirmed by the compiler

`src/system/dsp/IIRFilter.h` now declares (0x00..0x5F unchanged — pinned by the
byte-identical `FilterSlow`, per BR §9):

| offset | member | evidence |
|---|---|---|
| 0x00..0x5F | `mB0[4] mState1[4] mGain[4] mB0NegA[4] mNegA[4] mAccum[4]` | BR §9, byte-identical `FilterSlow` |
| 0x60 | `__vector4 mV60` | **never written, never read** — pure padding-by-declaration |
| 0x70 | `__vector4 mVState1` | indexed `stvx`, rB=0x70 |
| 0x80 | `IIRQuad mQ80` | `std` pair 0x80/0x88 |
| 0x90 | `__vector4 mVB0` | indexed `stvx`, rB=0x90 |
| 0xa0 | `__vector4 mVGain` | indexed `stvx`, rB=0xa0 |
| 0xb0 | `__vector4 mVB0NegA` | indexed `stvx`, rB=0xb0 |
| 0xc0 | `__vector4 mVNegA` | indexed `stvx`, rB=0xc0 |
| 0xd0 | `IIRQuad mQD0` | `std` pair 0xd0/0xd8 |

`scripts/harvest/class_layout_report.py IIR4PoleFilter` (the compiler, authoritative over
`// 0xHEX` comments): **`sizeof = 224 (0xe0)`**, every member at its intended offset.
Independently corroborates BR §9's `li r3,0xE0` at the `new` site.

### 1.5 The four sub-defects that actually closed the row

Layout alone was necessary and **not sufficient**. In landing order:

1. **Retail ROLLS poles 1..3.** Our ctor was straight-line for all four; retail emits a
   real loop for `i = 1..3` with pole 0 peeled.
2. **Store order is load-bearing** — zeroes (`mAccum`, `mState1[0]`) before ones
   (`mState1[1..3]`), not source-natural order.
3. **`IIRQuad` needs 16-byte alignment.** Without it the two zero quads compile to
   `lwz`/`stw` quads; with `__declspec(align(16))` they become the `std` pairs retail has
   — which additionally removed a spurious `__savegprlr_28` from the prologue.
4. **Assignment order 0xd0 before 0x80**, and single-element aggregate init `{ 0.0f }`
   (spelling all four elements changes codegen).

**Result: `??0IIR4PoleFilter@@QAA@PAM0@Z` 10.1923 → 100.0000 fuzzy / 11.8590 → 100.0000 mpn.**
`FilterSlow` held as the control — byte-identical, still 100.0, untouched.
`default/system/dsp/IIRFilter` is now a **unit at 100%** (2/2 fns, 436/436 B).

---

## §2 — Task 2: `?Detect@VibratoDetector@@QAAHXZ`

### 2.1 Two briefed claims, both refuted

objdiff's own read was *"missing implementation or wrong skeleton"* (diff score 7,439/9,400),
and BT filed the row as a **from-scratch reconstruction**. Both are wrong. The logic was
already essentially correct: all five thresholds (`3.0`, `8.0`, `2.0`, `0.1`, `1.2`) were
verified as floats out of `.rdata` and all five already matched. The skeleton was right;
one construct was wrong.

### 2.2 The single defect

A separate iteration counter let MSVC prove `d == 4` at the loop exit and constant-fold
both uses of it:

- `total / (float)d` → `fmuls f7, f8, 0.25f` (a multiply by a literal, no divide at all)
- the second loop's bound `i < d` → `cmpwi r10, 0x4`

Retail does **not** fold either, so retail's `d` is genuinely not provably 4 at that point.
Reusing the same counter across both loops restores the dependency and both foldings
disappear.

**Result: 20.8617 → 96.5426 fuzzy / 22.4574 → 98.2979 mpn.**

### 2.3 Two levers tried on the residue — one regression, one inert (both reverted)

| experiment | prediction | measured | disposition |
|---|---|---|---|
| hoist `int bufIdx = mBufIdx;` to the top | inert (same value, fewer loads) | **80.4149** — a **16 pp REGRESSION** | reverted |
| `int d = 0;` at function top, `for (; d < 4; d++)` | inert | 96.5426 — **exactly** inert | reverted |

The first is the informative one: hoisting the member read looks free and costs 16 pp,
because it changes which register holds `mBufIdx` across the whole body.

### 2.4 What the residual 24 charges are

All 24 charged sites on the graded ruler are **one register rotation** (`mBufIdx` in r9
where retail uses r10) plus the single `mr` and the single `li` re-ordering that the
rotation forces. There is no second defect underneath: idx 11 (`replace`) and idx 16
(`insert`) are consequences of the rotation, not independent sites.

⇒ This is the **permuter's** class, and the permuter is **OFF by standing directive**.
Recorded, not ground.

---

## §3 — Task 3: `?AnalyzeBlock@PitchDetector@@QAAXPBDPAFHMMAAM22@Z`

Two landed changes, plus one from §4:

| change | fuzzy |
|---|---:|
| baseline | 85.8584 |
| integer cluster at idx 94–111: `(mDecimRate - mIdx) % mDecimRate` | → 90.2404 |
| dev-only `dump()` under the house `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` | (included above) |
| `Time2IirA` real body (§4) | → **93.2404** |

mpn 86.6000 → **93.8584**.

**The integer cluster.** Our source spelled the modulo by hand as
`(mDecimRate - mIdx) - (mDecimRate - mIdx) / mDecimRate * mDecimRate`. Retail emits MSVC's
**checked signed `%`** sequence — `twllei` (divide-by-zero trap), `divw`/`mullw`/`subf`,
`twi 5` (overflow trap). The hand-spelled form omits both traps, so it can never match;
writing `%` restores them.

**The `dump()` block.** `MILO_DEBUG` is force-defined tree-wide (`src/macros.h:3`), so an
inherited `#ifdef MILO_DEBUG` is live in the match build where retail compiled it out. Both
the `void dump(float*,int)` body and the `if (sDump) { dump(mDecimBuf, 192); }` call site
are now under the house pattern, which keeps native behaviour.

### 3.1 BT §8's second prediction is also REFUTED

BT §8 predicted the f28/f29 swap *"would dissolve once the integer cluster was fixed"*,
citing `_M_insert_overflow`. It did not. The swap **persists** at idx 7, 120, 121 and 169
after the integer cluster closed, and again after §4. It is not a downstream symptom of
the integer defect.

### 3.2 The residual 79 charged sites, classified

| class | sites | fixable by source? |
|---|---:|---|
| f28/f29 float-register rotation | 4 | no — regalloc |
| r28/r29 + r27/r24 rotation in the inner loop | ~14 | no — regalloc |
| `kPropFilter` base-register rotation (r26/r27) | 3 | no — regalloc |
| ICF fold-alias `bl` names (`StlNodeAlloc` ctor ↔ `IIR4PoleFilter::Begin`/`End`) | 2 | **no source work possible** — relocation-name class, needs alias adjudication |
| `/fp:fast` fuse: retail `fmuls`+`fsubs`, ours `fmsubs f13,f1,f29,f31` | 2 | possibly — parenthesisation barrier |
| tail int→float idiom (`std`/`lfd` via `0x58(r31)`) + a `0xc(r30)` vs `0x24(r30)` field read | ~6 | **open — see §6** |
| remainder (inserts/deletes cascading from the above) | ~48 | no |

⚠ The two `bl` sites are the documented **fold-alias conflation**: objdiff's `name_check`
charges a folded callee exactly as it charges a wrong one. `?Begin@IIR4PoleFilter@@` and
`?End@IIR4PoleFilter@@` are the ICF-folded empties found in §1.1 — retail's surviving
spelling at that address is the `StlNodeAlloc` ctor. **No source mutation can close these**;
they need an adjudicated alias, which is out of this lane's scope.

Realistically this 1,780 B row will not cross. It is 93.2404 and the remaining distance is
~80% regalloc + fold-alias.

---

## §4 — Task 4: `Time2IirA` — the out-of-lining, SOLVED (and BR §10 explained)

BR §10 left open whether our tree inlines `Time2IirA` into `AnalyzeBlock` where retail
emits a real `bl`, and proposed a declaration-here / definition-there split; BR measured
that split at **Δ0** and stopped.

**The split was never the mechanism.** Reading retail's body at `0x82B6EA08` (92 B, from
`build/45410914/asm/Synapse_dsp.s`, keyed on the `.fn` symbol — never the synthetic
address column) decodes to:

```
fcmpu f1, 0.0 ; ble -> return 1.0      guard: time > 0.0f
fmuls f13, f1, f2                      time * rate
fdivs f1, -1.0, f13                    -1.0f / (time * rate)
bl    0x8282ED70                       exp    (named `exp` in the map)
frsp  f13, f1                          narrow to float FIRST
fsubs f1, 1.0, f13                     then subtract, in float
```

i.e.

```cpp
float Time2IirA(float time, float rate) {
    if (time > 0.0f) return 1.0f - (float)exp(-1.0f / (time * rate));
    return 1.0f;
}
```

Ours was `{ return exp(-1.0f / (time * rate)); }` — **missing both the guard and the
subtraction** (the caller carried the `1.0f -`), and dividing by zero at `time == 0`. The
`frsp` **before** the `fsubs` is what says the cast is explicit and the subtraction happens
in float.

⇒ Retail's helper **has control flow and ours did not.** `/O1 /Ob2` inlines a branch-free
one-liner unconditionally, so no amount of moving the *definition* could ever have changed
anything — which is exactly why BR measured Δ0. Restoring the real body (and dropping the
call site's now-duplicated `1.0f -`) makes the call survive as a `bl`.

**Result: `AnalyzeBlock` 90.2404 → 93.2404 fuzzy / 90.9146 → 93.8584 mpn.
`?Time2IirA@?A0xa7b3dd7d@@YAMMM@Z` stays at 100.0/100.0.**

⛔ `__declspec(noinline)` was **not** used — it is metric-chasing per the brief, and the
real body made it unnecessary. The anon-ns hash `?A0xa7b3dd7d` was **not** separately
identified to a TU, because the out-of-lining was explained without it; that thread is left
open in §6.

★ The durable lesson: **"is it inlined?" is a question about the callee's SHAPE, not its
placement.** A prior lane's Δ0 on a placement change is evidence the mechanism is
elsewhere, not evidence the phenomenon isn't real.

---

## §5 — Per-row before/after

| row | size | fuzzy before | fuzzy after | mpn before | mpn after | charged sites after |
|---|---:|---:|---:|---:|---:|---:|
| `??0IIR4PoleFilter@@QAA@PAM0@Z` | 312 | 10.1923 | **100.0000** | 11.8590 | **100.0000** | **0** |
| `?Detect@VibratoDetector@@QAAHXZ` | 376 | 20.8617 | **96.5426** | 22.4574 | **98.2979** | 24 (all regalloc) |
| `?AnalyzeBlock@PitchDetector@@QAAXPBDPAFHMMAAM22@Z` | 1780 | 85.8584 | **93.2404** | 86.6000 | **93.8584** | 79 |
| `?Time2IirA@?A0xa7b3dd7d@@YAMMM@Z` | 92 | 100.0 | 100.0 | 100.0 | 100.0 | 0 |
| `?FilterSlow@IIR4PoleFilter@@QAAMM@Z` (control) | 124 | 100.0 | 100.0 | 100.0 | 100.0 | 0 |

Charged-site counts are from `run_diff_inspect mode=mismatches ruler=graded` (the same
ruler `report.json` grades on), not from an instruction-equality count.

⚠ **Only the IIR4PoleFilter row moves `matched_code`.** `matched_code` is all-or-nothing
at `fuzzy == 100`, so `Detect` at 96.54 and `AnalyzeBlock` at 93.24 contribute **0 bytes**
despite +75.7 pp and +7.4 pp respectively. They move the aggregate `fuzzy_match_percent`
only. This is the documented two-rulers behaviour, not a disappointment — but it means the
lane's headline ΔB is carried by one 312-byte row.

---

## §6 — NOT done, and the exact evidence that would change it

| # | thing not done | why not | **evidence that would overturn this** |
|---|---|---|---|
| 1 | `?ShiftedDotProduct@@YAXPBMHPAM_N@Z` (356 B @ 25.01) | Out of scope by the brief; documented VMX128 wall at `0x82B81758..0x82B817C0`. I did **not** attempt it and found **no** evidence against the wall. | A `__vmaddfp`/`__lvx`/`__stvx` spelling in `xdk/LIBCMT/vectorintrinsics.h` (or `ppcintrinsics.h`) that reproduces the `0x82B81758..0x82B817C0` block — i.e. a compiled probe emitting that exact instruction sequence. The wall is a claim about our *intrinsics coverage*, so a counter-example is a source file, not an argument. |
| 2 | `Detect`'s last 24 charges (96.54 → 100) | All 24 are one register rotation; permuter OFF by standing directive. Two principled source levers tried: one **regressed 16 pp**, one exactly inert (§2.3). | A source construct that changes which register holds `mBufIdx` *without* changing the emitted operation count — e.g. a different loop induction shape. A third inert lever is **not** evidence; a lever that moves the rotation *at all*, in either direction, reopens this. |
| 3 | `AnalyzeBlock`'s 2 ICF fold-alias `bl` sites | Relocation-name class. `Begin`/`End` are ICF-folded empties (§1.1) whose surviving retail spelling is the `StlNodeAlloc` ctor. **No source mutation can close these** — this is the documented `name_check` fold-vs-wrong conflation. | Retail-byte adjudication showing the callee at that address is **not** a fold of our `Begin`/`End` — e.g. the call site passing an argument or consuming a return that an empty `void f()` cannot supply. That would make it a genuine wrong-callee (fixable) rather than a fold. |
| 4 | `AnalyzeBlock`'s `0xc(r30)` vs `0x24(r30)` field read (idx 221) and the `std`/`lfd 0x58(r31)` int→float idiom (idx 224–226) | **Genuinely open.** This is the one residual class in the row that is *not* regalloc and *not* fold-alias: retail reads a different `PitchDetector` member than we do at the tail, and does the int→float conversion differently. Ran out of budget before adjudicating it. | `scripts/harvest/class_layout_report.py PitchDetector --offset 0xc` and `--offset 0x24` naming two different members, plus the retail tail's use of the loaded value. If retail's `0xc` member is the one the tail arithmetic needs, this is a **real field bug** worth ~6 charges. **This is the highest-value unexamined thread in the lane.** |
| 5 | `AnalyzeBlock`'s `/fp:fast` fuse (retail `fmuls`+`fsubs`, ours `fmsubs`) | 2 charges; the documented lever is a parenthesisation barrier, untried for budget. | A parenthesisation of the `f13 = f1*f28 - f31` expression that splits the fuse. The `/fp:fast` paren-barrier pattern is documented as real, so this is a *probable* fix that simply was not attempted. |
| 6 | TU identification of anon-ns hash `?A0xa7b3dd7d` | Not needed — §4 explained the out-of-lining by the callee's **shape**, so the placement question became moot. | Nothing about it is refuted; it is simply unexamined. If a future lane finds a second out-of-lining anomaly in `Synapse_dsp` that shape does **not** explain, the hash→TU mapping becomes load-bearing again. |
| 7 | `__declspec(noinline)` on `Time2IirA` | Explicitly forbidden by the brief as metric-chasing — and unnecessary once the real body landed. | Nothing. This one should stay not-done. |

---

## §7 — Whole-binary Δ (`tools/ab_measure.py`)

Measured with `tools/ab_measure.py --worktree /home/free/tmp/wt-w16-bw --patch <lane.diff>`,
the lane squashed to a single patch against the merge-base.

⚠ **A trap worth recording, because it would have produced a wrong number silently.**
`git diff main..w16-bw` is **NOT** this lane's patch: `main` moved during the lane (base
`4ffe3db4fc08` → `3185bd28`, picking up W16-BY and others), so that diff renders *other
lanes'* landed work as **reversals** inside mine — it listed `BandProfile.cpp`, `Tour.cpp`,
`TourPerformer.cpp`, `BandWardrobe.cpp`, `VocalTrackDir.cpp`, none of which this lane
touched and all of which are explicitly out of its scope. Pricing that would have
attributed five other lanes' work to W16-BW with the sign flipped. **Always diff against
`git merge-base`, never against the moving branch tip.**

```
================ A/B RESULT (MEASURED) ================
  patch: w16bw-dsp (d830e8097fc29daf)  kinds: ['source']
  leg A: matched=43654 masked=23107 honest=20547 code%=39.577408  (recompiles: 0, settled)
  leg B: matched=43655 masked=23107 honest=20548 code%=39.580452  (recompiles: 3, split=0, settle iterations: 2)
  Delta matched=+1  Delta masked_equal=+0  Delta honest=+1  Delta code%=+0.003044pp  Delta code_bytes=+312
  Delta fuzzy=+0.006729pp   (legA 49.827557 -> legB 49.834286)
  unit improvements: 1 unit(s), sum +1
      +1  default/system/dsp/IIRFilter  (1->2)
  units at 100% [mpn ruler]:             legA 186 -> legB 187  (+1 reached 100, 0 fell off)
  units at 100% [all-rows-fuzzy ruler]:  legA 166 -> legB 167  (+1 reached 100, 0 fell off)
    mechanism: MATCHED_ROSE=1  -- default/system/dsp/IIRFilter  matched 1->2, rows 2->2
========================================================
```

| measure | leg A | leg B | Delta |
|---|---:|---:|---:|
| `matched_functions` | 43,654 | 43,655 | **+1** |
| `matched_code` (B) | 4,055,524 | 4,055,836 | **+312** |
| `matched_code_percent` | 39.577408 | 39.580452 | **+0.003044 pp** |
| `fuzzy_match_percent` | 49.827557 | 49.834286 | **+0.006729 pp** |
| `masked_equal_functions` | 23,107 | 23,107 | +0 |
| honest (`matched - masked_equal`) | 20,547 | 20,548 | **+1** |
| units at 100% (mpn) | 186 | 187 | **+1** |

**Rows crossed IN: 1** — `??0IIR4PoleFilter@@QAA@PAM0@Z` (312 B).
**Rows that fell OUT: 0** (both rulers, explicitly reported by the tool).
**Units crossed IN: 1** — `default/system/dsp/IIRFilter`, mechanism `MATCHED_ROSE` (matched 1->2, rows 2->2, so it is a genuine completion and not a denominator shrink). **Units that fell out: 0.**

The `none` control moved +312 B as well; the tool correctly labels this
`NOT_APPLICABLE` rather than a signal — for a **source** patch, movement on `none` is
expected and the alias-shape guard is only adjudicable on a map-only patch.

⚠ The entire +312 B is the one IIR4PoleFilter row. `Detect` (+75.7 pp) and `AnalyzeBlock`
(+7.4 pp) contribute **0 bytes** because neither reached `fuzzy == 100`. Their value is
real but is carried by `fuzzy_match_percent` (+0.006729 pp, more than double what the
+312 B alone would give) and by the defect knowledge in §2–§4.

---

## §8 — Native gate

Run from the worktree (`/home/free/tmp/wt-w16-bw`) as the lane's last build-affecting
action, after every source change had landed. Verbatim:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`NATIVE GATE: PASS  (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`

**`skipped=0`** as required — the run vouches for full coverage, not merely `PASS`
(the INCOMPLETE verdict is one space away from the full-pass line and has been relayed
wrongly before, which is why the machine-readable line is pasted rather than paraphrased).
The only action taken after the gate was committing this documentation file, which
contains no `src/` change and cannot affect the native link.

---

## §9 — Files touched

| file | what |
|---|---|
| `src/system/dsp/IIRFilter.h` | `0x60..0xDF` declared: 5 × `__vector4` + 2 × 16-byte-aligned `IIRQuad` + the never-touched `mV60`; `sizeof` 0xe0 confirmed by the compiler |
| `src/system/dsp/IIRFilter.cpp` | ctor rewritten (rolled poles 1..3, store order, assignment order) → 100.0 |
| `src/system/dsp/VibratoDetector.cpp` | `Detect()` — single shared loop counter → 96.54 |
| `src/system/dsp/PitchDetector.cpp` | `%` for the integer cluster; `dump()` under the house `MILO_DEBUG && HX_NATIVE` pattern; `Time2IirA` real body + call-site fix |

Commits on `w16-bw` (oldest first):

```
d7a2281e  IIRFilter: identify 0x60..0xE0 as the dead SIMD mirror; roll poles 1..3
bf5a6319  IIRFilter: ??0IIR4PoleFilter reaches 100.0 -- 10.1923 -> 100.0, 312 B
0a1ad8d8  VibratoDetector::Detect: 20.8617 -> 96.5426 -- the folded `d` was the wall
ffb928d5  PitchDetector::AnalyzeBlock: 85.8584 -> 90.2404 (integer cluster + dev-only dump)
b3d97ba3  dsp: Time2IirA had the wrong BODY, not the wrong home -- AnalyzeBlock 90.24 -> 93.24
```

---

## §10 — Three corrections to the in-tree record

1. **BT §8's "the row is not layout-blocked at all" is REFUTED** (§1.3). The tail is
   written at `stvx` width and never read — but *never read* does not imply *need not be
   written*, and for a matching target it never does.
2. **BT §8's "the f28/f29 swap will dissolve once the integer cluster is fixed" is
   REFUTED** (§3.1). It persists at idx 7, 120, 121, 169 through two subsequent fixes.
3. **BR §10's Δ0 on `Time2IirA` placement is EXPLAINED, not contradicted** (§4). The
   measurement was right; the mechanism was the callee's shape, not its placement.

Also worth recording against the brief itself: objdiff's *"missing implementation or wrong
skeleton"* on `Detect` (diff score 7,439/9,400) was **wrong** — the skeleton was correct
and one construct (a foldable loop counter) accounted for 75.7 pp. A low diff score is not
evidence that a body is absent.
