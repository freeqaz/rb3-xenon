# Fixable Patterns: fsel Intrinsic and FMA Pragma

Patterns involving PowerPC floating-point select (`fsel`) instructions and fused multiply-add (`fmadds`/`fmsubs`) control via `#pragma fp_contract`.

---

## fsel via Clamp/Min/Max Templates

**Impact:** +5-44%
**Success Rate:** HIGH (when only fsel mismatches present)
**Time:** 5 minutes

Replace branched float comparisons with the float-specialized `Min`/`Max`/`Clamp` templates from `math/Utl.h`, which generate `fsel` instructions.

### Symptom

objdiff shows branched float clamp pattern in decomp vs branchless `fsel` in target:

```asm
# Target (branchless - fsel)
fneg    f0, f1
fsel    f1, f0, f1, f12    # if f1 >= 0: f1, else: f12 (0.0f)

# Our build (branched)
fcmpu   cr0, f1, f12
bge     .L1
fmr     f1, f12             # f1 = 0.0f
.L1:
```

### Why It Works

PowerPC `fsel` is a conditional select instruction: `fsel(cond, a, b)` returns `a` if `cond >= 0.0`, else `b`. The original Xbox 360 compiler generates `fsel` for `Min`/`Max`/`Clamp` patterns. Our MSVC cross-compiler does NOT automatically generate `fsel` from `if` statements or ternaries — it needs explicit help.

### Fix: Use Float Templates from Utl.h

The float specializations in `src/system/math/Utl.h` generate `fsel`:

```cpp
#include "math/Utl.h"

// Clamp float to [0, 1]
// Before - generates fcmpu + branch
if (val < 0.0f) val = 0.0f;
if (val > 1.0f) val = 1.0f;

// After - generates fsel chain
val = Clamp(0.0f, 1.0f, val);
```

The templates work because they use subtraction-comparison patterns:
```cpp
template <> inline float Min(float x, float y) { return (x - y < 0) ? x : y; }
template <> inline float Max(float x, float y) { return (x - y < 0) ? y : x; }
template <> inline float Clamp(float min, float max, float value) {
    return Min(Max(min, value), max);
}
```

### Real Examples

| Function | Before | After | Delta | Fix |
|----------|--------|-------|-------|-----|
| **DebugGraph::Draw** | 46.8% | **100%** | +53.2% | `__fsel` intrinsic for branchless clamp (see below) |
| HiResScreen::CurrentTileRect | 40.0% | 83.6% | +43.6% | `Clamp(0.0f, 1.0f, val)` for 8 float clamps |
| Fader::SynthPoll | — | 89.1% | — | Uses `__fsel` directly (see below) |

### Case Study: DebugGraph::Draw (46.8% → 100%)

This function demonstrated that `__fsel` intrinsic can achieve 100% match when replacing branched float clamps.

**Original code (branched):**
```cpp
float x = someValue;
if (x < 0.0f) x = 0.0f;
if (x > 1.0f) x = 1.0f;
```

This generates:
```asm
fcmpu   cr0, f1, f12      ; compare with 0.0f
blt     .Lclamp_low
fmr     f1, f12           ; x = 0.0f
.Lclamp_low:
fcmpu   cr0, f1, f13      ; compare with 1.0f
bgt     .Lclamp_high
fmr     f1, f13           ; x = 1.0f
.Lclamp_high:
```

**Fixed code (branchless using `__fsel`):**
```cpp
#include "xdk/LIBCMT/ppcintrinsics.h"

// Clamp x to [0, 1] using fsel
float clamped = (float)__fsel(-x, x, 0.0f);        // max(x, 0): if -x >= 0 (x <= 0), return 0, else x
float result = (float)__fsel(clamped - 1.0f, 1.0f, clamped);  // min(result, 1): if clamped-1 >= 0, return 1, else clamped
```

This generates:
```asm
fneg    f0, f1            ; -x
fsel    f1, f0, f1, f12   ; if -x >= 0: f1, else: 0.0f (max)
fsubs   f0, f1, f13       ; clamped - 1.0f
fsel    f1, f0, f13, f1   ; if (clamped-1) >= 0: 1.0f, else: clamped (min)
```

**Why `Clamp<float>` template didn't work here:** In this specific case, the `Clamp` template generated slightly different register allocation. The direct `__fsel` intrinsic matched the target's exact instruction sequence.

**Key insight:** When `Clamp<float>` doesn't match exactly, try the raw `__fsel` pattern:
1. `max(x, min_val)` → `__fsel(-(x - min_val), x, min_val)` or `__fsel(-x, x, 0.0f)` for min=0
2. `min(x, max_val)` → `__fsel(x - max_val, max_val, x)`

### When Register Pressure Blocks This

If the float templates cause the compiler to allocate extra callee-saved FPRs (changing prologue/epilogue), the match may worsen. This happens because `fsel` keeps intermediate values alive longer than branched code, increasing register pressure.

| Function | Result | Issue | What Would Fix It |
|----------|--------|-------|-------------------|
| DebugGraph::Draw | Worsened | fsel via Clamp used 4 FPRs (f28-f31) vs target's 2 (f30-f31) | c2.dll register allocator patch to match FPR assignment, or finding an expression form that uses fewer live FPRs |

---

## fsel via __fsel Intrinsic

**Impact:** +1-10%
**Success Rate:** MEDIUM
**Time:** 5 minutes

Use the `__fsel` compiler intrinsic directly for custom conditional patterns.

### Intrinsic Signature

Declared in `src/xdk/LIBCMT/ppcintrinsics.h`:

```cpp
double __fsel(double fComparand, double fValGE, double fValLT);
```

**Semantics:** Returns `fValGE` if `fComparand >= 0.0`, else `fValLT`.

**Cast to float** for single-precision:
```cpp
float result = (float)__fsel(condition, val_if_ge_zero, val_if_lt_zero);
```

### When to Use

Use `__fsel` directly when:
- The Clamp/Min/Max templates don't fit the pattern
- You need a custom conditional (not simple min/max/clamp)
- The pattern is `fneg` + `fsel` in the target

### Examples in Codebase

From `src/system/synth/Faders.cpp`:
```cpp
// Clamp to [0, 1]
levelEase = (float)__fsel(-levelEase, 0.0f, levelEase);         // clamp negative to 0
levelEase = (float)__fsel(levelEase - 1.0f, 1.0f, levelEase);   // clamp > 1 to 1
```

### Register Pressure Consideration

`__fsel` keeps float values live across the select (no branch to break live ranges), which can increase FPR register pressure. If the function's prologue changes (more callee-saved FPRs), you need to restructure the surrounding code to reduce live float variables, or patch the compiler's register allocator (see [unfixable-compiler.md](unfixable-compiler.md#register-allocation) for the c2.dll coloring mechanism).

---

## FMA Control via #pragma fp_contract

**Impact:** +1-12%
**Success Rate:** HIGH for pure-direction
**Time:** 5 minutes

Control whether the compiler generates fused multiply-add instructions (`fmadds`, `fmsubs`) or separate operations (`fmuls` + `fadds`).

> **See also — contraction is not reassociation.** This section controls *whether* a
> multiply fuses into an add. A separate axis controls **in what order the products are
> accumulated**: under `/fp:fast` MSVC reassociates a sum chain, and the lever is explicit
> **parentheses as a reassociation barrier**, not term order. If the instruction shape is
> already right but the operands sit in the wrong slots, see
> [fixable-fp-reassociation.md](fixable-fp-reassociation.md).

### Symptom

objdiff shows FMA fusion mismatch:

```asm
# Target (separate ops)
fmuls   f0, f1, f2
fadds   f0, f0, f3

# Our build (fused)
fmadds  f0, f1, f2, f3
```

Or the reverse direction:

```asm
# Target (fused)
fmadds  f0, f1, f2, f3

# Our build (separate)
fmuls   f0, f1, f2
fadds   f0, f0, f3
```

### Pragma Syntax

```cpp
#pragma fp_contract(off)   // Disable FMA fusion - separate fmuls + fadds

void FuncNeedingSeparateOps() {
    float result = a * b + c;  // Generates: fmuls + fadds
}

#pragma fp_contract(on)    // Re-enable FMA fusion (this is the default)

void FuncNeedingFused() {
    float result = a * b + c;  // Generates: fmadds
}
```

### Important Rules

1. **File-scoped**: The pragma must appear OUTSIDE function definitions
2. **Affects all functions** between pragma pairs — bracket individual functions if needed
3. **Default is ON**: FMA fusion is enabled by default under `/fp:fast` (DC3's setting)
4. **Not affected by** `/fp:fast` or `/fp:precise` — only `/fp:strict` disables the pragma (DC3 does not use strict)

### Three Categories

#### Category 1: Pure OFF (our code fuses, target doesn't)

Fix: Add `#pragma fp_contract(off)` before the function.

```cpp
#pragma fp_contract(off)
void MyFunction() {
    // All a*b+c expressions generate separate fmuls + fadds
}
#pragma fp_contract(on)
```

#### Category 2: Pure ON (target fuses, our code doesn't)

Fix: Restructure expressions so multiply and add are adjacent. The compiler can only fuse `a * b + c` when the multiply result flows directly into the add.

```cpp
// Won't fuse - intermediate variable breaks the chain
float temp = a * b;
// ... other code ...
float result = temp + c;

// Will fuse - multiply feeds directly into add
float result = a * b + c;
```

Also try the FMA expression order patterns from [fixable-operators.md](fixable-operators.md#fma-expression-order).

#### Category 3: Mixed Direction

When the same function has BOTH "need ON" and "need OFF" patterns, a single pragma can't fix both. Options:

1. **Split the function** into separate helpers (one with pragma OFF, one with ON) if the mixed regions are separable
2. **Use volatile intermediates** to selectively prevent fusion: `volatile float temp = a * b; result = temp + c;` prevents fusion for that expression only while leaving others fusable
3. **Patch c2.dll** to match the original compiler's per-expression fusion heuristic (requires understanding when the original compiler chose to fuse vs not)

### Real Examples

| Function | Before | After | Delta | Category | Fix |
|----------|--------|-------|-------|----------|-----|
| Burst::Emit | 87.1% | 98.6% | +11.5% | Expression restructure | `ret2 * 3.0f - ret3 * 2.0f` for fmsubs |
| DxRnd::DrawSafeArea | 98.8% | 100% | +1.2% | Expression order | `1.0f - targetAspect * realAspect` |
| NgFur::Shell | — | 96.3% | — | Mixed | Needs per-expression control (ON for alpha, OFF for color) |

### Detection: Which Direction?

Run `mcp__orchestrator__run_diff_inspect` with mode=`mismatches` and look at the FMA instructions:

- **Our code has `fmadds`, target has `fmuls`+`fadds`** → Try `#pragma fp_contract(off)`
- **Our code has `fmuls`+`fadds`, target has `fmadds`** → Restructure expressions for fusion
- **Both directions in same function** → Mixed direction (see Category 3 above)

### Pragma Ignored by Compiler

In some cases, the MSVC PPC compiler ignores `#pragma fp_contract(off)` for certain expression patterns under `/O1` optimization. If the pragma changes the object file hash but doesn't change the target instructions, the compiler is overriding the pragma. To fix this requires either:
- Different optimization level for that file (risky — may break other functions)
- `volatile` intermediate to force separation
- c2.dll binary patch to respect the pragma unconditionally

---

## Combined fsel + FMA Issues

Some functions have BOTH fsel and FMA mismatches. Fix them independently:

1. First try `Clamp<float>` / `__fsel` for the branched-vs-fsel mismatches
2. Then try `#pragma fp_contract` for the FMA mismatches
3. Check if the fsel fix changes FPR register pressure (may affect FMA alignment)

### Decision Tree

```
Float comparison mismatch (fcmpu+branch vs fsel)?
  ├─ Simple min/max/clamp → Use Clamp<float>/Min<float>/Max<float> from Utl.h
  ├─ Custom conditional → Use __fsel intrinsic
  └─ Extra FPR saves → Restructure to reduce live floats, or c2.dll allocator patch

FMA mismatch (fmadds vs fmuls+fadds)?
  ├─ All same direction (need OFF) → #pragma fp_contract(off)
  ├─ All same direction (need ON) → Restructure expressions for fusion
  ├─ Mixed directions → Split function, volatile intermediates, or c2.dll patch
  └─ Pragma ignored → volatile intermediate or c2.dll patch
```

---

## See Also

- [fixable-operators.md](fixable-operators.md#fma-expression-order) — FMA expression order (`fmsubs` vs `fnmsubs`)
- [unfixable-compiler.md](unfixable-compiler.md#fmadds-vs-separate-ops) — Current compiler limitations for FMA
- [unfixable-compiler.md](unfixable-compiler.md#register-allocation) — Register allocator mechanism and c2.dll patching
- `docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md` — Full Xbox 360 FP reference
- `docs/decomp/XBOX360_PRAGMA_REFERENCE.md` — Pragma syntax and scope rules

## fsel via Explicit Ternary Subtraction/Negation

**Impact:** +5-15%
**Success Rate:** HIGH
**Time:** 5 minutes

When `Clamp<float>` or `__fsel` intrinsics don't yield the right register allocation, you can coax the MSVC compiler into emitting an `fsel` by writing a ternary expression that mirrors the `fsel` logic: `condition >= 0.0f ? val_if_ge : val_if_lt`. 

### Symptom
Target assembly contains an `fsel` following an `fsub` or `fneg` instruction, but using the `math/Utl.h` templates or `__fsel` intrinsic results in register allocation mismatches or extra FPR saves.

### Why It Works
The compiler recognizes a specific ternary pattern `(expr) >= 0.0f ? a : b` (where `expr` is a negation or subtraction) and maps it directly to an `fsel` instruction without needing explicit intrinsics.

### Fix
```cpp
// Target generates:
// fneg f0, f1
// fsel f1, f0, f12, f1   (if -x >= 0.0f then 0.0f else x)
float val = -x >= 0.0f ? 0.0f : x; 

// Target generates:
// fsubs f0, f1, f13
// fsel f1, f0, f13, f1   (if x - 1.0f >= 0.0f then 1.0f else x)
float val2 = val - 1.0f >= 0.0f ? 1.0f : val;
```

### Real Examples

| Function | Before | After | Delta | Fix |
|----------|--------|-------|-------|-----|
| GameEndedDataPointJob ctor | ~77% | 85.8% | ~8% | Used `-streamMs >= 0.0f ? 0.0f : streamMs` instead of if-statements for float clamping |

---

## Reciprocal CSE: two `fdivs` + four `fmuls` where the target has four `fdivs`

**Impact:** +3-4% on the affected function
**Success Rate:** HIGH (deterministic; it is a source-shape rule, not a heuristic)
**Time:** 10 minutes

### Symptom

The target divides a constant by a variable several times and emits one real
`fdivs` per division. We instead emit a synthesized `1.0f` constant
(`__real@3f800000`), one `fdivs` computing `1.0f/x`, and an `fmuls` per use:

```
TGT  fdivs f13, f29, f13      ; 0.5f / h
SRC  fmuls f13, f13, f28      ; (1.0f/h) * -0.5f
```

The giveaway is a `lfs fN, __real@3f800000` in our output that has no `1.0f`
anywhere in the source.

### Why It Happens

**The Xbox 360 (Xenon) MSVC front end defaults to `/fp:fast`, not
`/fp:precise`.** Under `/fp:fast` it strength-reduces `a/x` and `b/x` into
`t = 1.0f/x; a*t; b*t` whenever two or more divisions name the **same
source-level divisor variable**. Measured on a standalone probe:

| flags | output |
|-------|--------|
| *(none — project default)* | 2 `fdivs` + 4 `fmuls` |
| `/fp:fast` | 2 `fdivs` + 4 `fmuls` (identical to default) |
| `/fp:precise` | 4 `fdivs` |
| `/fp:strict` | 4 `fdivs` |
| `/Op` | 4 `fdivs` |

Reproduced identically on `X360/16.00.10224.00` (RB3) and
`X360/16.00.11886.00` (DC3), so it is the Xenon default rather than a
compiler-build artifact.

### Fix

Do **not** add `/fp:precise` to the project cflags — see the negative result
below. The trigger keys on *source-level variable identity of the divisor*,
not on the value, so give each division its own temp:

```cpp
// BEFORE - `w` and `h` are each named twice as a divisor -> reciprocal CSE
int w = mDiffuseTex->Width();
int h = mDiffuseTex->Height();
mTexHalfPixelY    =  0.5f / h;
mTexHalfPixelX    =  0.5f / w;
mTexHalfPixelNegY = -0.5f / h;
mTexHalfPixelNegX = -0.5f / w;

// AFTER - four distinct divisor variables -> four real fdivs
float fw1 = mDiffuseTex->Width();
float fh1 = mDiffuseTex->Height();
float fw2 = mDiffuseTex->Width();
float fh2 = mDiffuseTex->Height();
mTexHalfPixelX    =  0.5f / fw1;
mTexHalfPixelY    =  0.5f / fh1;
mTexHalfPixelNegX = -0.5f / fw2;
mTexHalfPixelNegY = -0.5f / fh2;
```

A later CSE still merges the identical `int`->`float` conversions, so the temps
cost nothing. Two secondary levers:

- Reading the accessor directly into each temp is what produces the extra
  conversion; routing through `int w`/`int h` locals first collapses them to two.
- **Use order, not declaration order, decides which operand's conversion is
  duplicated.** Reordering the four `float` declarations changes nothing at all.
  The pair used *first* is CSE'd into one conversion; the pair used second is
  emitted twice and scheduled first.

### Not a behavioural difference

`1.0f/x` then `* 0.5f` is bit-identical to `0.5f/x` for all normal `x`: scaling
by a power of two is exact, and exact power-of-two scaling commutes with
round-to-nearest. The two forms can only differ at denormal or overflow
boundaries. Treat this as a match fix and check the constants involved before
claiming a runtime bug.

### NEGATIVE RESULT — do not add `/fp:precise` project-wide

Tested with a full rebuild and `measure_progress.sh`: overall fuzzy
**53.85% -> 53.36%, -624 matched functions**; 311 units regress against 6 that
improve (`system/math` -10.4%, `DoubleExponentialSmoother` -56.6%). It is a net
loss even inside the one TU that motivated it — `Mat_NG.cpp`: `RefreshState`
93.25 -> 95.29 but `MakeTex3` 100 -> 84.6 and `SetRegularShaderConst`
99.7 -> 94.6. The codebase really was built at the `/fp:fast` default; any fix
has to be source-level.

### Real Examples

| Function | Before | After | Delta | Fix |
|----------|--------|-------|-------|-----|
| `NgMat::RefreshState` (DC3) | 93.25% | 96.8% | +3.5% | Four distinct float temps; assign in X, Y, NegX, NegY order |
| `NgMat::RefreshState` (RB3) | 92.2% | 96.4% | +4.2% | Same fix, same file |
