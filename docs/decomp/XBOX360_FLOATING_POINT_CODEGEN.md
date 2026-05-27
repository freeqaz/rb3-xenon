# Xbox 360 Floating Point Code Generation Report

**Analysis Date:** 2026-01-29
**Target:** Xbox 360 PowerPC Compiler (Visual C++ 2010 Front-end)
**Source:** Xbox 360 XDK Documentation

---

## Executive Summary

The Xbox 360 PowerPC compiler provides fine-grained control over floating-point code generation through compiler flags and pragmas. The most critical setting for matching decompilation is the **floating-point contraction** behavior, which determines whether the compiler generates fused multiply-add instructions (`fmadds`) or separate multiply/add instructions (`fmuls` + `fadds`).

---

## 1. Floating Point Model Options

The Xbox 360 compiler supports four floating-point models via the `/fp:` flag:

### `/fp:fast` (Default for optimized builds)
- **Behavior:** Results are less predictable, allows aggressive optimizations
- **Contraction:** Enabled by default
- **Use Case:** Maximum performance when exact floating-point semantics are not critical

### `/fp:precise`
- **Behavior:** Results are predictable, follows IEEE 754 more strictly
- **Contraction:** Controlled by `#pragma fp_contract` (ON by default)
- **Use Case:** When reproducible results are required across different optimization levels

### `/fp:strict`
- **Behavior:** Strictest IEEE 754 compliance
- **Contraction:** **DISABLED** - ignores all `#pragma fp_contract` directives
- **Exception Handling:** Implies `/fp:except`
- **Use Case:** Scientific computing requiring exact IEEE 754 behavior

### `/fp:except[-]`
- **Behavior:** Enables reliable floating-point exception model
- **Default:** OFF (disabled)
- **Impact:** Exceptions raised immediately when triggered
- **Note:** Can be explicitly disabled with `/fp:except-`

---

## 2. Floating Point Contraction Control

### What is Contraction?

**Contraction** combines multiple floating-point operations into a single equivalent operation with rounding applied only at the end. The most common example is fused multiply-add:

```c
// Source code
float result = a * b + c;
```

**With contraction ON** (`#pragma fp_contract(on)`):
```asm
fmadds  f1, f2, f3, f4    ; Single instruction: (f2 * f3) + f4
```

**With contraction OFF** (`#pragma fp_contract(off)`):
```asm
fmuls   f1, f2, f3        ; First multiply
fadds   f1, f1, f4        ; Then add (separate rounding step)
```

### The `fp_contract` Pragma

```c
#pragma fp_contract(on)   // Enable contraction (default)
#pragma fp_contract(off)  // Disable contraction
```

**Rules:**
- Must appear **outside** function definitions
- Takes effect at the **first function defined** after the pragma
- **Default state:** ON (contraction enabled)
- **Overridden by:** `/fp:strict` compiler flag (which disables contraction completely)

**Example from XDK documentation:**

```c
#pragma fp_contract(off)
float TestContract1(float a, float b, float c)
{
    return a * b + c;
}
// Generates: fmuls + fadds (two instructions)

#pragma fp_contract(on)
float TestContract2(float a, float b, float c)
{
    return a * b + c;
}
// Generates: fmadds (one instruction)
```

---

## 3. PowerPC Floating Point Instructions

### Standard FPU Instructions

The Xbox 360 supports the full PowerPC FPU instruction set:

#### Basic Arithmetic
- `fabs` - Floating Point Absolute Value
- `fsqrt` - Floating Point Square Root (Double-Precision)
- `fsqrts` - Floating Point Square Root Single

#### Estimate Functions (Fast Approximations)
- `fres` - Floating Point Reciprocal Estimate Single
- `frsqrte` - Floating Point Reciprocal Square Root Estimate

#### Special Operations
- `fsel` - Floating Select (conditional without branching)

#### Load/Store with Update
- `lfsu` - Load Floating-point Single with Update
- `lfdu` - Load Floating-point Double with Update
- `stfsu` - Store Floating-point Single with Update
- `stfdu` - Store Floating-point Double with Update
- `stfiwx` - Store Floating-point as Integer Word Indexed

#### Conversion Operations
- `fcfid` - Floating Convert from Integer Double Word
- `fctid` - Floating Convert to Integer Double Word
- `fctidz` - Floating Convert to Integer Double Word with Round toward Zero
- `fctiw` - Floating Convert to Integer Word
- `fctiwz` - Floating Convert to Integer Word with Round toward Zero
- `__frnd` - Floating Point Round (combines fctid + fcfid)

### Fused Multiply-Add Variants

**Critical for Decompilation Matching:**

The PowerPC architecture provides fused multiply-add/subtract instructions:
- `fmadd[s]` - Floating Multiply-Add
- `fmsub[s]` - Floating Multiply-Subtract
- `fnmadd[s]` - Floating Negative Multiply-Add
- `fnmsub[s]` - Floating Negative Multiply-Subtract

The `[s]` suffix indicates single-precision versions (e.g., `fmadds`).

**Key insight:** Whether the compiler generates `fmadds` vs `fmuls + fadds` is controlled by:
1. The `/fp:` model flag
2. The `#pragma fp_contract` state
3. Code structure and optimization level

---

## 4. PowerPC Intrinsics

Xbox 360 provides intrinsics for direct access to floating-point operations:

### Math Operations
```c
double __fabs(double fval);
float  __fabs(float fval);
double __frsqrte(double fval);          // Reciprocal square root estimate
float  __fres(float fval);              // Reciprocal estimate
double __fsel(double fComparand, double fValGE, double fLT);  // Select
double __fsqrt(double fval);
float  __fsqrts(float fval);
```

### Conversion Operations
```c
double __fcfid(double i64Param);        // From integer
double __fctid(double fParam);          // To integer
double __fctidz(double fParam);         // To integer, round toward zero
double __fctiw(double fParam);          // To word
double __fctiwz(double fParam);         // To word, round toward zero
double __frnd(double fRoundee);         // Round using current mode
```

**Header:** `PPCIntrinsics.h`

---

## 5. VMX128 Vector Floating Point

The Xbox 360 CPU includes VMX128 vector processing with 128 vector registers (128-bit wide).

### Vector Floating Point Operations

**Arithmetic:**
- `vaddfp` - Vector Add Floating Point
- `vsubfp` - Vector Subtract Floating Point
- `vmulfp` - Vector Multiply Floating Point (VMX128 extension)
- `vmaddfp` - Vector Multiply Add Floating Point
- `vnmsubfp` - Vector Negate Multiply-Subtract Floating Point
- `vmaxfp` / `vminfp` - Vector Maximum/Minimum Floating Point

**Dot Products (VMX128-specific):**
- `vmsum3fp` - 3-operand Dot Product
- `vmsum4fp` - 4-operand Dot Product

**Estimates:**
- `vrefp` - Vector Reciprocal Estimate Floating Point
- `vrsqrtefp` - Vector Reciprocal Square Root Estimate Floating Point
- `vexptefp` - Vector 2^x Estimate Floating Point
- `vlogefp` - Vector Log2 Estimate Floating Point

**Rounding/Conversion:**
- `vrfim` - Vector Round to Floating-point Integer toward Minus Infinity
- `vrfin` - Vector Round to Floating-point Integer Nearest
- `vrfip` - Vector Round to Floating-point Integer toward Plus Infinity
- `vrfiz` - Vector Round to Floating-point Integer toward Zero
- `vcfsx` / `vcfux` - Vector Convert from Signed/Unsigned Fixed-Point Word
- `vctsxs` / `vctuxs` - Vector Convert to Signed/Unsigned Fixed-Point Word Saturate

**Comparison:**
- `vcmpbfp` - Vector Compare Bounds Floating Point
- `vcmpeqfp` - Vector Compare Equal-to Floating Point
- `vcmpgefp` - Vector Compare Greater-Than-or-Equal-to Floating Point
- `vcmpgtfp` - Vector Compare Greater-Than Floating Point

**Header:** `VectorIntrinsics.h`
**Data Type:** `__vector4` (maps to VMX128 register)

### VMX Register Reservation

**`/QVMXReserve` Compiler Flag:**
- Excludes VMX registers VR64-VR95 (32 registers) from compiler allocation
- Allows manual control via `__VMXSetReg()` and `__VMXGetReg()`
- **Warning:** Xbox 360 libraries (D3D, XUI) are **NOT** compiled with this flag
- Callbacks cannot safely use reserved registers

---

## 6. Denormal Number Handling

### IEEE 754 Denormal Numbers

**Denormal (subnormal) numbers** are non-zero values smaller than the smallest normal number:

| Precision | Smallest Normal | Greatest Normal |
|-----------|----------------|-----------------|
| Single | 2^-126 | 1.99999988 × 2^127 |
| Double | 2^-1022 | 1.99999999 × 2^1023 |

### FPU Denormal Support
- **Full IEEE 754 support** for denormals
- **Performance penalty:** ~60 cycles per denormal operation
- Used for scalar floating-point operations

### VMX Denormal Behavior
- **No denormal support** (for performance)
- Denormal results are **flushed to signed zero**
- This is a **permanent hardware configuration** on Xbox 360
- May cause unexpected arithmetic results in edge cases

**Impact on matching:** Code that generates denormals may have different behavior between FPU and VMX paths.

---

## 7. 64-bit Integer Conversion

PowerPC does not have hardware instructions for converting between `uint64` and floating point. The compiler generates **emulation function calls** instead:

| Function | Conversion |
|----------|-----------|
| `__u64tod` | uint64 → double |
| `__u64tos` | uint64 → single |
| `__dtou64` | double → uint64 |
| `__stou64` | single → uint64 |

**Note:** Signed 64-bit conversions use hardware instructions (`fcfid`, `fctid`).

---

## 8. Optimization Flags Affecting Floating Point

### `/O1` - Optimize for Size
- Smaller code generation
- May disable some aggressive FP optimizations

### `/O2` - Optimize for Speed (Default)
- Maximum performance
- Enables aggressive FP optimizations

### `/Oi` - Generate Intrinsic Functions
- Replaces CRT calls with intrinsic implementations
- Can affect FP library functions

### `/fp:fast` vs `/fp:precise` Impact
- `/fp:fast` allows reordering and contraction without restrictions
- `/fp:precise` ensures reproducible results, honors `#pragma fp_contract`

---

## 9. Decompilation Matching Strategies

### Strategy 1: Match Contraction Behavior

**Symptom:** Code generates `fmadds` when you expect `fmuls + fadds` (or vice versa)

**Solution:**
```c
// Force separate multiply and add
#pragma fp_contract(off)
float CalculateValue(float a, float b, float c) {
    return a * b + c;  // Generates: fmuls, fadds
}

// Re-enable for other code
#pragma fp_contract(on)
```

### Strategy 2: Verify Compiler Flags

**Check your build configuration:**
- Ensure `/fp:precise` or `/fp:fast` matches the original build
- **Never use `/fp:strict`** for game code (too restrictive)
- Debug builds may use `/fp:precise` while release uses `/fp:fast`

### Strategy 3: Identify Fused Instructions

When analyzing objdiff output, look for:
- `fmadds` / `fmsubfp` = multiply-add/subtract
- `fmuls` + `fadds` = separate operations
- `fnmadds` / `fnmsubfp` = negated multiply-add/subtract

### Strategy 4: Local Contraction Control

Use pragma scope control for fine-grained matching:

```c
// File-level default
#pragma fp_contract(on)

// Disable for specific function
#pragma fp_contract(off)
void ProblemFunction() {
    // ...
}

// Re-enable after
#pragma fp_contract(on)

void RestOfCode() {
    // ...
}
```

---

## 10. Common Pitfalls

### Pitfall 1: `/fp:strict` Disables All Pragmas
**Problem:** You set `#pragma fp_contract(on)` but still get separate instructions
**Cause:** `/fp:strict` overrides all pragma settings
**Solution:** Use `/fp:precise` instead

### Pitfall 2: Pragma Placement
**Problem:** Pragma doesn't affect the intended function
**Cause:** Pragma must appear **before** function definition, **outside** the function
**Solution:**
```c
// WRONG
void MyFunc() {
    #pragma fp_contract(off)  // Too late!
    return a * b + c;
}

// CORRECT
#pragma fp_contract(off)
void MyFunc() {
    return a * b + c;
}
```

### Pitfall 3: Mixing FPU and VMX
**Problem:** Inconsistent behavior between scalar and vector code
**Cause:** VMX flushes denormals to zero, FPU does not
**Solution:** Avoid operations that produce denormals, or use consistent pathways

### Pitfall 4: Assuming Default State
**Problem:** Contraction behavior differs between files
**Cause:** Different files may have different pragma states
**Solution:** Explicitly set `#pragma fp_contract` at the top of each file

---

## 11. Quick Reference Table

| Scenario | Instruction | Pragma Setting | Flag Requirement |
|----------|-------------|----------------|------------------|
| Fused multiply-add | `fmadds` | `fp_contract(on)` | NOT `/fp:strict` |
| Separate multiply+add | `fmuls`, `fadds` | `fp_contract(off)` | Any `/fp:` mode |
| Strict IEEE 754 | Various | N/A | `/fp:strict` |
| Maximum performance | Various | Default | `/fp:fast` |
| Reproducible results | Various | As needed | `/fp:precise` |

---

## 12. Recommendations for DC3 Decompilation

Based on the Xbox 360 XDK documentation:

1. **Use `/fp:fast` for Release builds** (likely original setting)
2. **Default state:** Assume `#pragma fp_contract(on)` unless evidence suggests otherwise
3. **Per-function matching:** Use `#pragma fp_contract(off)` before functions that clearly use separate instructions
4. **Watch for patterns:**
   - `a * b + c` → likely `fmadds` with contraction ON
   - Complex expressions → may need experimentation
5. **Debug builds:** May use `/fp:precise` for predictability
6. **Never use `/fp:strict`** (games don't need this level of IEEE compliance)

---

## 13. Example Build Configuration

**Release Configuration (Matching Original):**
```
/O2                  # Optimize for speed
/fp:fast             # Fast floating-point model
/Oi                  # Generate intrinsics
(default contraction ON)
```

**Debug Configuration:**
```
/Od                  # No optimization
/fp:precise          # Predictable results
/fp:except-          # No FP exceptions
(default contraction ON, but controllable via pragma)
```

---

## 14. Further Reading

- **Compiler Options:** `/home/free/code/milohax/rb3-xenon/docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md` (this file)
- **PowerPC Intrinsics:** `dev_compiler_ppc_intrinsics.htm`
- **Vector Intrinsics:** `dev_compiler_vector_intrinsics.htm`
- **fp_contract Pragma:** `dev_compiler_pragma_fp_contract.htm`
- **Floating Point Support:** `callstd_floating_point_emulation.htm`
- **PowerPC Instructions:** IBM PowerPC Programming Environments Manual (included with XDK)

---

## Glossary

- **Contraction:** Combining multiple FP operations into one with single rounding
- **Denormal:** Number smaller than the smallest normal representable value
- **FPU:** Floating Point Unit (scalar operations)
- **VMX128:** Xbox 360 vector processing unit (128 registers, 128-bit each)
- **Fused Multiply-Add:** Single instruction performing `(a * b) + c` with one rounding
- **IEEE 754:** Standard for floating-point arithmetic
- **Intrinsic:** Compiler built-in function mapping to hardware instructions

---

**Document Version:** 1.0
**Last Updated:** 2026-01-29
