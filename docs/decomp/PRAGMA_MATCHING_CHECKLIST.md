# Pragma Matching Checklist

Practical step-by-step guide for using pragmas to achieve matching decompilation.

---

## Before You Start

### Check Your Build Configuration

```bash
# View current compiler flags
ninja -v | grep -o '\-[A-Z][^ ]*' | sort -u

# Look for:
# /O1 or /O2 or /Ox  → Optimization level
# /fp:fast or /fp:precise or /fp:strict  → Floating-point model
# /Ou  → Prescheduling enabled
# /Oz  → Inline assembly optimization (no effect on DC3 - no inline asm)
```

### Known DC3 Configuration

```makefile
# Expected flags (verify in build system):
/O2              # Speed optimization
/fp:precise      # Standard floating-point (likely)
/Ou              # Prescheduling (TO BE CONFIRMED)
```

---

## Checklist 1: Floating-Point Contraction

### When to Use

Apply this checklist if your function:
- Contains floating-point multiply and add operations
- Shows `fmadds`, `fmadd`, `fmsubs`, or `fmsub` in disassembly
- Has mismatched instruction count in objdiff

### Steps

- [ ] **Step 1: Check disassembly for fused multiply-add**
  ```bash
  ./bin/objdiff-cli disasm <symbol> | grep -E "fmadd|fmsub"
  ```

- [ ] **Step 2: Identify the pattern**
  - If `fmadds` present → contraction is ON
  - If separate `fmuls` + `fadds` → contraction is OFF

- [ ] **Step 3: Check your source for `a * b + c` patterns**
  ```cpp
  // These patterns trigger multiply-add:
  result = a * b + c;       // fmadds if contraction ON
  result = a * b - c;       // fmsubs if contraction ON
  result += a * b;          // fmadds if contraction ON
  result = c + a * b;       // fmadds if contraction ON (commutative)
  ```

- [ ] **Step 4: Apply pragma at file scope**
  ```cpp
  // At the TOP of the .cpp file (outside all functions):

  #pragma fp_contract(on)   // If disassembly shows fmadds
  // OR
  #pragma fp_contract(off)  // If disassembly shows separate fmuls + fadds
  ```

- [ ] **Step 5: Rebuild and verify**
  ```bash
  ninja
  ./bin/objdiff-cli diff <symbol>
  ```

- [ ] **Step 6: Check instruction count**
  - With `fp_contract(on)`: Each `a*b+c` = 1 instruction
  - With `fp_contract(off)`: Each `a*b+c` = 2 instructions

### Common Issues

**Issue:** Added pragma but still doesn't match
- **Check:** Are you using `/fp:strict`? This IGNORES the pragma
- **Fix:** Change to `/fp:precise` or `/fp:fast` in build config

**Issue:** Some functions match, others don't
- **Check:** Pragma must be at file scope, not inside functions
- **Fix:** Move pragma outside all function definitions

**Issue:** Need different settings for different functions
- **Solution:** Toggle pragma between functions
  ```cpp
  #pragma fp_contract(on)
  void Function1() { /* uses fmadds */ }

  #pragma fp_contract(off)
  void Function2() { /* uses separate fmuls + fadds */ }

  #pragma fp_contract(on)
  void Function3() { /* uses fmadds */ }
  ```

---

## Checklist 2: Instruction Ordering (Prescheduling)

### When to Use

Apply this checklist if your function:
- Has correct instructions but wrong order
- Shows unusual interleaving of operations
- Has independent computations mixed together

### Steps

- [ ] **Step 1: Confirm instructions are correct**
  ```bash
  ./bin/objdiff-cli diff <symbol> | grep "opcode"
  # Look for: same opcodes, different order
  ```

- [ ] **Step 2: Test with prescheduling flag**
  ```bash
  # Modify build config to add /Ou if not present
  # OR remove /Ou if present
  ninja clean
  ninja
  ./bin/objdiff-cli diff <symbol>
  ```

- [ ] **Step 3: Document results**
  - If `/Ou` improves match → prescheduling WAS used in original
  - If removing `/Ou` improves match → prescheduling was NOT used

- [ ] **Step 4: Apply globally vs per-function**

  **Global (recommended):**
  ```makefile
  # In build config:
  CXXFLAGS += /Ou    # Enable prescheduling for all files
  ```

  **Per-function (rare):**
  ```cpp
  #pragma optimize("u", off)
  void NoPrescheduling() {
      // This function compiled without prescheduling
  }
  #pragma optimize("u", on)
  ```

- [ ] **Step 5: Verify optimization base is enabled**
  - Prescheduling pragma needs `/O1`, `/O2`, or `/Ox`
  - Check build config has one of these flags

### Common Issues

**Issue:** Pragma has no effect
- **Check:** Do you have `/O1`, `/O2`, or `/Ox` enabled?
- **Fix:** Prescheduling requires base optimization

**Issue:** Only some functions need different scheduling
- **Solution:** Use pragma to override global setting
  ```cpp
  // If global /Ou is enabled, but specific function needs it off:
  #pragma optimize("u", off)
  void SpecialFunction() { /* ... */ }
  #pragma optimize("u", on)
  ```

**Issue:** Can't tell if prescheduling is the issue
- **Symptoms:** Instructions are scrambled, operations interleaved
- **Test:** Toggle `/Ou` and compare objdiff output

---

## Checklist 3: Bitfield Order

### When to Use

Apply this checklist if your function:
- Accesses bitfield struct members
- Shows incorrect bit masks or shifts
- Has wrong offsets for bitfield access

### Steps

- [ ] **Step 1: Confirm function uses bitfields**
  ```cpp
  // Look for structures like:
  struct Flags {
      unsigned int flag1 : 1;
      unsigned int flag2 : 3;
      unsigned int flag3 : 4;
  };
  ```

- [ ] **Step 2: Check default is usually correct**
  - Xbox 360 default: `msb_to_lsb` (PowerPC standard)
  - Most DC3 code will use this
  - Only x86 interop code needs `lsb_to_msb`

- [ ] **Step 3: Test with alternate order (if needed)**
  ```cpp
  #pragma bitfield_order(lsb_to_msb)
  struct MyFlags {
      // Bitfield definition
  };
  #pragma bitfield_order(msb_to_lsb)  // Restore default
  ```

- [ ] **Step 4: Use push/pop for header includes**
  ```cpp
  // In header file (cross-platform):
  #pragma bitfield_order(push, my_header, lsb_to_msb)
  struct CrossPlatformFlags {
      // Definition
  };
  #pragma bitfield_order(pop, my_header)
  ```

- [ ] **Step 5: Verify in objdiff**
  - Check bit masking operations
  - Check shift amounts
  - Verify member offsets

### Common Issues

**Issue:** Still getting wrong bit positions
- **Check:** Are you using the pragma on the DEFINITION, not usage?
- **Fix:** Pragma must precede struct definition

**Issue:** Works in one file, fails in another
- **Solution:** Use push/pop in header
  ```cpp
  // In shared header:
  #pragma bitfield_order(push)
  #pragma bitfield_order(lsb_to_msb)
  struct SharedFlags { /* ... */ };
  #pragma bitfield_order(pop)
  ```

---

## Checklist 4: General Optimization

### When to Use

Apply this checklist if:
- Function is optimized differently than expected
- Need to disable optimization for debugging
- Specific function has unique optimization needs

### Steps

- [ ] **Step 1: Identify optimization mismatch**
  ```bash
  ./bin/objdiff-cli analyze <symbol>
  # Look for optimization-related differences
  ```

- [ ] **Step 2: Test with optimization disabled**
  ```cpp
  #pragma optimize("", off)
  void DebugFunction() {
      // Compiled with optimization disabled
  }
  #pragma optimize("", on)
  ```

- [ ] **Step 3: Test specific optimization toggles**
  ```cpp
  // Standard MSVC optimization flags:
  #pragma optimize("g", off)   // Disable global optimizations
  #pragma optimize("s", on)    // Favor code size
  #pragma optimize("t", on)    // Favor code speed

  // Xbox 360-specific:
  #pragma optimize("u", off)   // Disable prescheduling
  #pragma optimize("z", off)   // Disable inline assembly optimization
  ```

- [ ] **Step 4: Document which optimizations affect function**
  ```cpp
  // Example findings:
  // - Function1 matches with prescheduling OFF
  // - Function2 needs fp_contract ON
  // - Function3 matches with default settings
  ```

---

## Quick Reference

### Pragma Syntax Summary

```cpp
// Floating-point contraction
#pragma fp_contract(on)          // Enable fused multiply-add
#pragma fp_contract(off)         // Disable fused multiply-add

// Optimization control
#pragma optimize("", off)        // Disable all optimizations
#pragma optimize("", on)         // Enable all optimizations
#pragma optimize("u", off)       // Disable prescheduling
#pragma optimize("u", on)        // Enable prescheduling
#pragma optimize("z", off)       // Disable inline assembly optimization
#pragma optimize("z", on)        // Enable inline assembly optimization

// Bitfield order
#pragma bitfield_order(msb_to_lsb)              // PowerPC default
#pragma bitfield_order(lsb_to_msb)              // x86 style
#pragma bitfield_order(push, id, msb_to_lsb)    // Save current, set new
#pragma bitfield_order(pop, id)                 // Restore previous
```

### Scope Rules

| Pragma | Scope | Placement |
|--------|-------|-----------|
| `fp_contract` | File | Outside functions |
| `optimize` | File | Outside functions |
| `bitfield_order` | File | Before struct definition |

**Critical:** All these pragmas must appear OUTSIDE function definitions and take effect at the next function/struct defined after them.

---

## Testing Workflow

### For Each Function That Doesn't Match

1. **Identify the issue type:**
   - [ ] Instruction count mismatch → Check `fp_contract`
   - [ ] Instruction order mismatch → Check prescheduling
   - [ ] Bitfield access wrong → Check `bitfield_order`
   - [ ] Other → Check general optimization

2. **Apply relevant checklist above**

3. **Test change:**
   ```bash
   ninja
   ./bin/objdiff-cli diff <symbol>
   ```

4. **Document results:**
   ```cpp
   // Add comment explaining pragma choice:

   // Function uses fused multiply-add (fmadds) for performance
   #pragma fp_contract(on)

   void MyFunction() {
       // Implementation
   }
   ```

5. **Commit when matching:**
   ```bash
   git add src/path/to/file.cpp
   git commit -m "Match MyFunction with fp_contract pragma"
   ```

---

## Common Patterns in DC3

Based on analysis, expect these patterns:

### Pattern 1: Most floating-point code uses contraction
```cpp
// At top of typical source file:
#pragma fp_contract(on)

// Most functions compiled with fused multiply-add enabled
```

### Pattern 2: Prescheduling may be global
```makefile
# In build config:
CXXFLAGS += /Ou
```
Per-function overrides would be rare.

### Pattern 3: Bitfield order is default
```cpp
// No pragma needed - PowerPC default (msb_to_lsb) is correct
struct GameFlags {
    unsigned int active : 1;
    unsigned int mode : 3;
    // ...
};
```

### Pattern 4: Debug functions may disable optimization
```cpp
#pragma optimize("", off)
void AssertHandler(const char* msg) {
    // Unoptimized for better debugging
}
#pragma optimize("", on)
```

---

## Troubleshooting

### Problem: Pragma seems to have no effect

**Check:**
1. Is pragma outside function definitions? (Must be file scope)
2. For `optimize` pragmas: Is base optimization enabled? (Need `/O1`/`/O2`/`/Ox`)
3. For `fp_contract`: Are you using `/fp:strict`? (This ignores pragma)

### Problem: Function matches without any pragmas

**This is good!** It means:
- Default compiler settings match original build
- No special pragma handling needed
- Document this for future reference

### Problem: Can't get function to match at all

**Try:**
1. Check if it's a merged symbol (`merged_<addr>` name)
2. Check if it has ASSERT_REVS (expect ~0.8-0.9% mismatch)
3. Use `./bin/objdiff-cli analyze <symbol>` for detailed analysis
4. Check for unfixable patterns (struct offsets, merged calls)

### Problem: Match percentage got worse after adding pragma

**Solution:**
- Remove the pragma
- Try the opposite setting (on → off, or off → on)
- Document that default is correct

---

## Documentation Template

When you successfully match a function with pragmas, document it:

```cpp
//
// File: src/system/math/MathOps.cpp
//
// Pragma Configuration:
// - fp_contract: ON (uses fmadds for multiply-add operations)
// - Prescheduling: Per global /Ou flag
//

#pragma fp_contract(on)

float CalculateTransform(float a, float b, float c) {
    // Generates fmadds instruction
    return a * b + c;
}

// Special case: High-precision calculation needs separate operations
#pragma fp_contract(off)
float HighPrecisionCalc(float x, float y, float z) {
    // Generates fmuls + fadds for better precision
    return x * y + z;
}
#pragma fp_contract(on)
```

---

## Next Steps

After applying pragmas:
1. Rebuild: `ninja`
2. Verify: `./bin/objdiff-cli diff <symbol>`
3. Document: Add comments explaining pragma choices
4. Commit: Include pragma rationale in commit message

For more details, see:
- `docs/decomp/XBOX360_PRAGMA_REFERENCE.md`
- `docs/decomp/PRAGMA_CODEGEN_SUMMARY.md`
