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
