# Xbox 360 Compiler Pragma Documentation Index

Complete documentation suite for understanding and using compiler pragmas in DC3 decompilation.

---

## Overview

This documentation covers all Xbox 360 compiler pragmas that affect code generation, based on official XDK documentation (Version 21256.3). The goal is to help match decompiled functions by understanding how pragmas change the generated assembly.

---

## Documentation Structure

### 1. Comprehensive Reference
**File:** [XBOX360_PRAGMA_REFERENCE.md](XBOX360_PRAGMA_REFERENCE.md)

**Purpose:** Complete technical documentation of all code-generation pragmas

**Contents:**
- Detailed pragma syntax
- Default states
- Impact on generated assembly
- Compiler flag interactions
- Usage examples
- Scope and placement rules

**Use When:**
- Need complete technical details
- Researching pragma behavior
- Understanding compiler flag interactions
- Writing technical documentation

### 2. Quick Summary
**File:** [PRAGMA_CODEGEN_SUMMARY.md](PRAGMA_CODEGEN_SUMMARY.md)

**Purpose:** Quick reference for pragmas that affect instruction selection

**Contents:**
- Critical pragma impact summary
- Code generation comparison tables
- Before/after assembly examples
- Quick decision trees
- Testing workflows

**Use When:**
- Need quick lookup during decompilation
- Comparing assembly differences
- Making fast decisions about pragma usage
- Understanding instruction selection

### 3. Practical Checklist
**File:** [PRAGMA_MATCHING_CHECKLIST.md](PRAGMA_MATCHING_CHECKLIST.md)

**Purpose:** Step-by-step guide for applying pragmas to match functions

**Contents:**
- Checklists for each pragma type
- Testing procedures
- Troubleshooting guides
- Common patterns
- Documentation templates

**Use When:**
- Actively working on matching a function
- Function has instruction count mismatches
- Instruction ordering is wrong
- Need systematic debugging approach

---

## Quick Navigation

### By Problem Type

**Problem: Instruction count is wrong**
→ Start with: [PRAGMA_CODEGEN_SUMMARY.md - fp_contract section](PRAGMA_CODEGEN_SUMMARY.md#critical-pragma-fp_contract)
→ Then use: [PRAGMA_MATCHING_CHECKLIST.md - Checklist 1](PRAGMA_MATCHING_CHECKLIST.md#checklist-1-floating-point-contraction)

**Problem: Instruction order is scrambled**
→ Start with: [PRAGMA_CODEGEN_SUMMARY.md - optimize section](PRAGMA_CODEGEN_SUMMARY.md#important-pragma-optimize)
→ Then use: [PRAGMA_MATCHING_CHECKLIST.md - Checklist 2](PRAGMA_MATCHING_CHECKLIST.md#checklist-2-instruction-ordering-prescheduling)

**Problem: Bitfield access is wrong**
→ Start with: [PRAGMA_CODEGEN_SUMMARY.md - bitfield section](PRAGMA_CODEGEN_SUMMARY.md#minimal-pragma-bitfield_order)
→ Then use: [PRAGMA_MATCHING_CHECKLIST.md - Checklist 3](PRAGMA_MATCHING_CHECKLIST.md#checklist-3-bitfield-order)

**Problem: Need technical details**
→ Read: [XBOX360_PRAGMA_REFERENCE.md](XBOX360_PRAGMA_REFERENCE.md)

### By Pragma Type

**`#pragma fp_contract`**
- Summary: [PRAGMA_CODEGEN_SUMMARY.md](PRAGMA_CODEGEN_SUMMARY.md#critical-pragma-fp_contract)
- Reference: [XBOX360_PRAGMA_REFERENCE.md - Floating-Point Pragmas](XBOX360_PRAGMA_REFERENCE.md#floating-point-pragmas)
- Checklist: [PRAGMA_MATCHING_CHECKLIST.md - Checklist 1](PRAGMA_MATCHING_CHECKLIST.md#checklist-1-floating-point-contraction)

**`#pragma optimize`**
- Summary: [PRAGMA_CODEGEN_SUMMARY.md](PRAGMA_CODEGEN_SUMMARY.md#important-pragma-optimize)
- Reference: [XBOX360_PRAGMA_REFERENCE.md - Optimization Pragmas](XBOX360_PRAGMA_REFERENCE.md#optimization-pragmas)
- Checklist: [PRAGMA_MATCHING_CHECKLIST.md - Checklist 2](PRAGMA_MATCHING_CHECKLIST.md#checklist-2-instruction-ordering-prescheduling)

**`#pragma bitfield_order`**
- Summary: [PRAGMA_CODEGEN_SUMMARY.md](PRAGMA_CODEGEN_SUMMARY.md#minimal-pragma-bitfield_order)
- Reference: [XBOX360_PRAGMA_REFERENCE.md - Bitfield Pragmas](XBOX360_PRAGMA_REFERENCE.md#bitfield-pragmas)
- Checklist: [PRAGMA_MATCHING_CHECKLIST.md - Checklist 3](PRAGMA_MATCHING_CHECKLIST.md#checklist-3-bitfield-order)

### By Task

**Task: Understanding how pragmas work**
→ Read: [XBOX360_PRAGMA_REFERENCE.md](XBOX360_PRAGMA_REFERENCE.md)

**Task: Matching a specific function**
→ Use: [PRAGMA_MATCHING_CHECKLIST.md](PRAGMA_MATCHING_CHECKLIST.md)

**Task: Quick lookup during work**
→ Use: [PRAGMA_CODEGEN_SUMMARY.md](PRAGMA_CODEGEN_SUMMARY.md)

**Task: Researching compiler flags**
→ Read: [XBOX360_PRAGMA_REFERENCE.md - Compiler Flag Interactions](XBOX360_PRAGMA_REFERENCE.md#compiler-flag-interactions)

---

## Key Concepts

### Most Important: `fp_contract`

**What it does:** Controls whether multiply-add operations are fused into single instructions

**Why it matters:**
- Most common cause of instruction count mismatches
- Default is ON (fused multiply-add enabled)
- `/fp:strict` disables and ignores pragma

**Common pattern:**
```cpp
#pragma fp_contract(on)  // Most DC3 code uses this
```

### Important: Prescheduling (`/Ou` flag, `optimize("u")` pragma)

**What it does:** Adds extra instruction scheduling pass before register allocation

**Why it matters:**
- Completely changes instruction ordering
- Can make identical source generate different assembly
- Global flag is more common than per-function pragma

**Test approach:**
```bash
# Toggle /Ou flag in build config
ninja clean && ninja
./bin/objdiff-cli diff <symbol>
```

### Rarely Needed: `bitfield_order`

**What it does:** Controls bitfield packing order (MSB first vs LSB first)

**Why it usually doesn't matter:**
- Xbox 360 default (`msb_to_lsb`) is correct for most code
- Only needed for x86 interop or cross-platform structs

---

## Workflow Integration

### During Initial Function Matching

1. Write initial C++ implementation
2. Compile and run objdiff
3. If instruction count is wrong:
   - Check for multiply-add patterns (`a * b + c`)
   - Look for `fmadds` in disassembly
   - Apply `fp_contract` pragma per [Checklist 1](PRAGMA_MATCHING_CHECKLIST.md#checklist-1-floating-point-contraction)
4. If instruction order is wrong:
   - Test `/Ou` flag toggle
   - Apply findings per [Checklist 2](PRAGMA_MATCHING_CHECKLIST.md#checklist-2-instruction-ordering-prescheduling)
5. Document pragma choices in comments

### During Code Review

1. Check pragma usage is documented
2. Verify pragma placement (must be outside functions)
3. Confirm pragma choices match disassembly
4. Ensure comments explain why pragma is needed

### During Batch Work

1. Identify common pragma patterns per file/module
2. Apply file-level pragmas at top of file
3. Use per-function toggles only when needed
4. Document in module-level comments

---

## DC3-Specific Information

### Expected Configuration

```makefile
# Build flags (verify in ninja -v output):
/O2              # Speed optimization
/fp:precise      # Standard floating-point (NOT /fp:strict)
/Ou              # Prescheduling (TO BE CONFIRMED)
```

### Common Pragma Patterns

**Most C++ files:**
```cpp
// src/system/*/SomeFile.cpp
#pragma fp_contract(on)

// Rest of file uses fused multiply-add by default
```

**Math-heavy files:**
```cpp
// May toggle fp_contract for specific functions requiring precision
#pragma fp_contract(on)
void FastApproximation() { /* uses fmadds */ }

#pragma fp_contract(off)
void PreciseCalculation() { /* uses separate fmuls + fadds */ }

#pragma fp_contract(on)
```

**Bitfield structures:**
```cpp
// Usually no pragma needed - default is correct
struct Flags {
    unsigned int active : 1;
    unsigned int mode : 3;
    // PowerPC default (msb_to_lsb) is used
};
```

---

## Known Limitations

### What Pragmas Can't Fix

1. **Merged symbols** - Functions merged by linker (ICF)
2. **ASSERT_REVS functions** - Inherent scheduling differences (~0.8-0.9% mismatch)
3. **Struct offset issues** - Wrong struct layouts need header fixes
4. **VMX128 intrinsics** - Compiler version differences

### What Pragmas Can Fix

1. **Instruction count** - Via `fp_contract`
2. **Instruction ordering** - Via prescheduling control
3. **Bitfield layouts** - Via `bitfield_order`
4. **Function-level optimization** - Via `optimize` pragma

---

## Testing Pragmas

### Systematic Approach

For each non-matching function:

```bash
# 1. Get baseline
./bin/objdiff-cli diff <symbol> > baseline.txt

# 2. Test fp_contract
# Edit source: Add #pragma fp_contract(on) at top
ninja && ./bin/objdiff-cli diff <symbol> > test_fp_on.txt

# Edit source: Change to #pragma fp_contract(off)
ninja && ./bin/objdiff-cli diff <symbol> > test_fp_off.txt

# 3. Test prescheduling (modify build config)
# Toggle /Ou flag
ninja clean && ninja && ./bin/objdiff-cli diff <symbol> > test_no_Ou.txt

# 4. Compare results
diff baseline.txt test_fp_on.txt
diff baseline.txt test_fp_off.txt
diff baseline.txt test_no_Ou.txt

# 5. Apply best match
# Update source with winning pragma
# Document choice in comments
```

---

## Best Practices

### Pragma Placement

```cpp
//
// File: src/system/Example.cpp
//

// GOOD: File-level pragmas at top, before any functions
#pragma fp_contract(on)

void Function1() { /* ... */ }

// GOOD: Toggle between functions for different behavior
#pragma fp_contract(off)
void Function2() { /* ... */ }
#pragma fp_contract(on)

void Function3() { /* ... */ }

// BAD: Don't put pragmas inside functions
void BadExample() {
    #pragma fp_contract(on)  // WRONG! Has no effect!
    float x = a * b + c;
}
```

### Documentation

```cpp
//
// Module: Math Operations
// Pragma Configuration:
//   - fp_contract: ON (default for fast multiply-add)
//   - Prescheduling: Per global /Ou flag
//
// Notes:
//   - Most functions use fmadds for multiply-add operations
//   - HighPrecisionCalc disables fp_contract for accuracy
//

#pragma fp_contract(on)

float FastCalc(float a, float b, float c) {
    // Uses fmadds: efficient but less precise
    return a * b + c;
}

// Disable contraction for this specific function
#pragma fp_contract(off)
float HighPrecisionCalc(float a, float b, float c) {
    // Uses separate fmuls + fadds: slower but more precise
    return a * b + c;
}
#pragma fp_contract(on)
```

### Commit Messages

```
Match MathOps::Calculate with fp_contract pragma

- Disassembly shows fmadds instructions for multiply-add
- Added #pragma fp_contract(on) at file scope
- All 5 functions in file now match 100%

Verified with:
  ./bin/objdiff-cli diff MathOps__Calculate
```

---

## Troubleshooting Guide

### Pragma has no effect

**Symptom:** Added pragma but code generation unchanged

**Checks:**
1. Is pragma outside all functions? (File scope required)
2. For `optimize`: Is `/O1`, `/O2`, or `/Ox` enabled?
3. For `fp_contract`: Using `/fp:strict`? (It ignores pragma)
4. Did you rebuild? (`ninja`)

### Match got worse

**Symptom:** Match percentage decreased after adding pragma

**Solution:**
1. Try opposite setting (on → off, off → on)
2. Remove pragma entirely (default may be correct)
3. Document what you tried

### Can't achieve 100% match

**Possible causes:**
1. Merged symbol (check for `merged_<addr>` name)
2. ASSERT_REVS function (expect ~0.8-0.9% mismatch)
3. Wrong struct offsets (fix headers, not pragmas)
4. Unfixable compiler differences

**Next steps:**
1. Run `./bin/objdiff-cli analyze <symbol>`
2. Check verdict for "at_limit" status
3. Document findings
4. Move to next function

---

## Additional Resources

### XDK Documentation Source

Files analyzed from `/tmp/claude/xdk_docs/`:
- `dev_compiler_pragma_fp_contract.htm`
- `dev_compiler_pragma_optimize.htm`
- `dev_compiler_pragma_bitfieldorder.htm`
- `dev_compiler_pragma_reversebitfield.htm`
- `dev_compiler_ouoz.htm`
- `atoc_tools_compiler_options.htm`
- `atoc_tools_compiler_pragmas.htm`

### Related DC3 Documentation

- [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md) - General compiler quirks
- [RB3_REFERENCE.md](RB3_REFERENCE.md) - Rock Band 3 reference implementations
- [docs/tools/WORKFLOW.md](../tools/WORKFLOW.md) - Overall decomp workflow

### External Resources

- [MSVC Pragma Directives](http://msdn.microsoft.com/en-us/library/d9x1s805(VS.100).aspx) - Standard MSVC pragma reference
- Xbox 360 XDK Documentation - Version 21256.3 (2013)

---

## Summary

### Three-Document System

1. **[XBOX360_PRAGMA_REFERENCE.md](XBOX360_PRAGMA_REFERENCE.md)** - Complete reference
2. **[PRAGMA_CODEGEN_SUMMARY.md](PRAGMA_CODEGEN_SUMMARY.md)** - Quick lookup
3. **[PRAGMA_MATCHING_CHECKLIST.md](PRAGMA_MATCHING_CHECKLIST.md)** - Step-by-step guide

### Most Important Points

1. **`fp_contract` is critical** - Controls fused multiply-add instruction generation
2. **Pragmas are file-scoped** - Must appear outside functions
3. **Default is usually correct** - Only add pragmas when necessary
4. **Document your choices** - Explain why pragma was needed

### Quick Start

For a function that won't match:
1. Check instruction count → Try `fp_contract` pragma
2. Check instruction order → Test `/Ou` flag
3. Document what worked
4. Move on to next function

---

**Last Updated:** 2026-01-29
**XDK Version:** 21256.3 (2013)
**Project:** DC3 Decompilation
