# Xbox 360 Compiler Pragma Reference

Comprehensive analysis of all Xbox 360 C++ compiler pragmas affecting code generation, based on official XDK documentation.

## Table of Contents
1. [Floating-Point Pragmas](#floating-point-pragmas)
2. [Optimization Pragmas](#optimization-pragmas)
3. [Bitfield Pragmas](#bitfield-pragmas)
4. [Compiler Flag Interactions](#compiler-flag-interactions)

---

## Floating-Point Pragmas

### `#pragma fp_contract`

**Syntax:**
```cpp
#pragma fp_contract(ON | OFF)
```

**Default State:** ON

**What It Controls:**
Determines whether floating-point contraction occurs. Contraction combines multiple floating-point operations into one equivalent operation with rounding applied at the operation's end.

**Impact on Generated Assembly:**

When `fp_contract` is **OFF**:
```cpp
float TestContract1(float a, float b, float c) {
    return a * b + c;
}
```
Generates separate instructions:
- `fmuls` (floating multiply single)
- `fadds` (floating add single)

When `fp_contract` is **ON**:
```cpp
float TestContract2(float a, float b, float c) {
    return a * b + c;
}
```
Generates fused instruction:
- `fmadds` (floating multiply-add single)

**Scope:**
- Must appear outside a function
- Takes effect at the first function defined after the pragma
- Identical to Microsoft Visual C++ implementation

**Compiler Flag Interactions:**
- `/fp:strict` disables floating-point contraction by default and **IGNORES** `fp_contract` pragma specifications
- With `/fp:precise` or `/fp:fast`, pragma works as expected

**Critical for Matching:**
This is one of the most important pragmas for decompilation. Functions expecting `fmadds` will not match if the pragma is disabled. Conversely, functions requiring separate multiply and add operations need `fp_contract(OFF)`.

---

## Optimization Pragmas

### `#pragma optimize`

**Syntax:**
```cpp
#pragma optimize("[optimization-list]", on | off)
```

**Default State:** Depends on compiler flags

**What It Controls:**
Specifies optimizations to be performed on a function-by-function basis. Supports standard MSVC optimization flags plus Xbox 360-specific extensions.

**Xbox 360-Specific Optimization Types:**

| Parameter | Type of Optimization | Impact |
|-----------|---------------------|--------|
| `u` | Prescheduling | Performs additional code scheduling pass **before** register allocation phase |
| `z` | Inline assembly optimization | Reorders inline assembly instructions to minimize latencies |

**Standard MSVC Parameters:**
All standard MSVC optimization flags are supported (see Visual C++ documentation for complete list).

**Impact on Generated Assembly:**

**Prescheduling (`u`):**
- Adds extra instruction scheduling pass before register allocation
- Improves computation interleaving
- May increase register pressure (less flexibility in register assignment)
- Can significantly affect instruction order

**Inline Assembly (`z`):**
- Reorders `__asm` blocks to minimize pipeline latencies
- Interleaves inline assembly with surrounding code

**Usage Example:**
```cpp
#pragma optimize("u", on)   // Enable prescheduling
float ComputeValue(float x) {
    // Complex computation
}
#pragma optimize("u", off)  // Disable prescheduling
```

**Scope:**
- Must appear outside a function
- Takes effect at the first function defined after the pragma

**Compiler Flag Interactions:**
- `/Ou` enables prescheduling globally
- `/Oz` enables inline assembly optimization globally
- Both options only take effect if one of these optimizations is enabled:
  - `/O1` (minimize size)
  - `/O2` (maximize speed)
  - `/Ox` (full optimization)
  - `/Og` (deprecated global optimizations)

**For Decompilation:**
- Prescheduling can dramatically affect instruction ordering
- Functions compiled with different prescheduling settings will have different instruction sequences even with identical source
- Check `/Ou` flag in build configuration

---

## Bitfield Pragmas

### `#pragma bitfield_order`

**Syntax:**
```cpp
#pragma bitfield_order([show,] | [lsb_to_msb | msb_to_lsb])
```

**Default State:** `msb_to_lsb` (PowerPC convention)

**What It Controls:**
Specifies the packing order for bit fields in structures.

**Parameters:**

| Parameter | Effect |
|-----------|--------|
| `show` | Displays current bitfield order via warning message |
| `lsb_to_msb` | Pack from least-significant bit toward most-significant bit |
| `msb_to_lsb` | Pack from most-significant bit toward least-significant bit (default for PowerPC) |

**Impact on Generated Assembly:**
- Affects bit masking and shifting operations
- Changes memory layout of bitfield structures
- Affects structure member access code generation

**Enhanced Syntax (Push/Pop):**
```cpp
#pragma bitfield_order([push | pop,] [identifier,] [lsb_to_msb | msb_to_lsb])
```

**Push/Pop Semantics:**
- `push` - Stores current setting on internal compiler stack
- `pop` - Retrieves setting from top of stack
- `identifier` - Named stack entry for push/pop matching

**Usage Example (Header Files):**
```cpp
/* File: include1.h */
#pragma bitfield_order(push, enter_include1, lsb_to_msb)
/* Header code using x86-style bitfields */
#pragma bitfield_order(pop, enter_include1)
```

**Usage Example (Source Files):**
```cpp
#pragma bitfield_order(push, before_include1)
#include "include1.h"
#pragma bitfield_order(pop, before_include1)
```

**For Decompilation:**
- Xbox 360 uses `msb_to_lsb` by default (PowerPC convention)
- Unlikely to encounter `lsb_to_msb` unless interfacing with x86 data structures
- Stack semantics useful for preserving settings across header includes

---

### `#pragma reverse_bitfield` (DEPRECATED)

**Syntax:**
```cpp
#pragma reverse_bitfield(on | off)
```

**Status:** DEPRECATED - Use `bitfield_order` instead

**Mapping to `bitfield_order`:**
- `#pragma reverse_bitfield(on)` → `#pragma bitfield_order(lsb_to_msb)`
- `#pragma reverse_bitfield(off)` → `#pragma bitfield_order(msb_to_lsb)`

**Reason for Deprecation:**
- Term "reverse" is ambiguous
- `bitfield_order` supports push/pop semantics for safer code
- More maintainable

**For Decompilation:**
Do not use. If found in original headers, replace with `bitfield_order`.

---

## Compiler Flag Interactions

### Floating-Point Flags

| Flag | Effect on `fp_contract` | Default Contraction State |
|------|------------------------|---------------------------|
| `/fp:fast` | Pragma respected | ON |
| `/fp:precise` | Pragma respected | ON |
| `/fp:strict` | **Pragma IGNORED** | OFF (forced) |
| `/fp:except` | Pragma respected | ON |

**Critical Rule:**
`/fp:strict` disables floating-point contraction and **ignores all `fp_contract` pragma directives**.

### Optimization Flags

| Flag | Effect | Enables |
|------|--------|---------|
| `/O1` | Minimize size | Enables `/Ou` and `/Oz` pragmas |
| `/O2` | Maximize speed | Enables `/Ou` and `/Oz` pragmas |
| `/Ox` | Full optimization | Enables `/Ou` and `/Oz` pragmas |
| `/Ou` | Global prescheduling | Prescheduling for all functions |
| `/Oz` | Global inline ASM opt | Inline assembly optimization (no effect on DC3 - no inline asm) |

**Important:**
- `/Ou` and `/Oz` compiler flags have **no effect** without `/O1`, `/O2`, or `/Ox`
- `#pragma optimize("u", ...)` has no effect without one of the base optimization flags

---

## Decompilation Strategy

### 1. Identifying `fp_contract` State

**Signs that `fp_contract` is ON:**
- `fmadds`, `fmadd` instructions in disassembly
- Single instruction for `a * b + c` patterns
- Single instruction for `a * b - c` patterns

**Signs that `fp_contract` is OFF:**
- Separate `fmuls` + `fadds` sequences
- Separate `fmuld` + `faddd` sequences for doubles

**How to Match:**
1. Check disassembly for fused multiply-add instructions
2. Add `#pragma fp_contract(on)` before functions with `fmadds`
3. Add `#pragma fp_contract(off)` before functions with separate multiply/add
4. Place pragma at file scope, not function scope

### 2. Detecting Prescheduling

**Signs of Prescheduling (`/Ou` or `#pragma optimize("u", on)`):**
- Unusual instruction ordering (computations interleaved)
- Instructions separated from their dependencies
- Heavy register reuse with complex scheduling

**How to Test:**
1. Try compiling with and without `/Ou` flag
2. Compare instruction ordering in objdiff
3. If ordering matches with `/Ou`, function was prescheduled

### 3. Bitfield Considerations

**Default is Safe:**
- Xbox 360 uses `msb_to_lsb` by default
- Most code will not have explicit `bitfield_order` pragmas
- Only needed when interfacing with x86 structures

**When to Investigate:**
- Bitfield access generates wrong offsets
- Structure packing doesn't match
- Cross-platform code (PC/Xbox shared headers)

---

## Common Patterns in DC3

Based on DC3's compilation configuration:

### Expected Pragma Usage

**Floating-Point:**
```cpp
// Most functions - contraction enabled by default
float CalculateTransform(float a, float b, float c) {
    return a * b + c;  // Generates fmadds
}

// Specific functions may disable contraction for precision
#pragma fp_contract(off)
float PreciseCalculation(float a, float b, float c) {
    return a * b + c;  // Generates fmuls + fadds
}
#pragma fp_contract(on)
```

**Optimization:**
```cpp
// Rarely used per-function optimization control
#pragma optimize("", off)  // Disable all optimizations
void DebugFunction() {
    // Debugging code
}
#pragma optimize("", on)
```

### DC3 Build Configuration

Based on debug build characteristics:
- No LTCG (Link-Time Code Generation)
- `/fp:precise` or `/fp:fast` (not `/fp:strict`)
- `/O2` likely (optimization enabled)
- `/Ou` may be enabled (need to verify)
- `fp_contract` default is ON

---

## Reference

Source: Xbox 360 XDK Documentation (Version 21256.3)
- `dev_compiler_pragma_fp_contract.htm`
- `dev_compiler_pragma_optimize.htm`
- `dev_compiler_pragma_bitfieldorder.htm`
- `dev_compiler_pragma_reversebitfield.htm`
- `dev_compiler_ouoz.htm`
- `atoc_tools_compiler_options.htm`

For standard MSVC pragma documentation:
http://msdn.microsoft.com/en-us/library/d9x1s805(VS.100).aspx
