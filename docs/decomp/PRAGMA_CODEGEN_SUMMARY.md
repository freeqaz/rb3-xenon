# Pragma Impact on Code Generation - Quick Reference

Summary of Xbox 360 compiler pragmas that directly affect instruction selection and code generation, critical for matching decompilation.

---

## Critical: `#pragma fp_contract`

**This is the most important pragma for matching floating-point code.**

### Quick Facts
- **Default:** ON
- **Scope:** File-level (must be outside functions)
- **Override:** `/fp:strict` disables and ignores pragma

### Code Generation Impact

| State | Source | Generated Instructions | Instruction Count |
|-------|--------|----------------------|-------------------|
| ON | `a * b + c` | `fmadds` | 1 instruction |
| OFF | `a * b + c` | `fmuls` then `fadds` | 2 instructions |
| ON | `a * b - c` | `fmsubs` | 1 instruction |
| OFF | `a * b - c` | `fmuls` then `fsubs` | 2 instructions |

### How to Detect in Disassembly

```assembly
# fp_contract ON:
fmadds  f1, f1, f2, f3    # f1 = (f1 * f2) + f3

# fp_contract OFF:
fmuls   f1, f1, f2        # f1 = f1 * f2
fadds   f1, f1, f3        # f1 = f1 + f3
```

### Usage Pattern

```cpp
// File: Character.cpp

// Default state - contraction enabled
void UpdatePosition(float x, float y, float scale) {
    position = x * scale + offset;  // Uses fmadds
}

// Disable for specific function requiring precision
#pragma fp_contract(off)
void PrecisePhysicsCalc(float mass, float accel, float friction) {
    force = mass * accel + friction;  // Uses fmuls + fadds
}
#pragma fp_contract(on)  // Re-enable

// Rest of file uses fused multiply-add
void UpdateVelocity(float v, float a, float t) {
    velocity = v + a * t;  // Uses fmadds
}
```

### Matching Strategy

1. **Identify the pattern:**
   - Look for `fmadds`, `fmadd`, `fmsubs`, `fmsub` in objdiff
   - If present → pragma is ON
   - If absent (separate multiply/add) → pragma is OFF

2. **Apply the pragma:**
   ```cpp
   // Add at top of file if entire file uses fused instructions
   #pragma fp_contract(on)

   // Or toggle for specific sections
   #pragma fp_contract(off)
   // Functions needing separate instructions
   #pragma fp_contract(on)
   ```

3. **Verify:**
   - Check objdiff for instruction match
   - All `a*b+c` patterns should now match assembly

---

## Important: `#pragma optimize`

**Controls function-level optimization behavior.**

### Xbox 360-Specific Options

| Option | Name | Impact |
|--------|------|--------|
| `u` | Prescheduling | Adds scheduling pass **before** register allocation |
| `z` | Inline ASM | Reorders `__asm` blocks for performance |

### Code Generation Impact

**Prescheduling (`u`):**
- Dramatically changes instruction ordering
- Interleaves independent computations
- Increases register pressure
- **Effect:** Same source, completely different instruction sequence

**Example without prescheduling:**
```assembly
lwz   r10, 0(r3)      # Load
addi  r10, r10, 1     # Increment
stw   r10, 0(r3)      # Store
lwz   r11, 4(r3)      # Load next
addi  r11, r11, 1     # Increment next
stw   r11, 4(r3)      # Store next
```

**Same code with prescheduling:**
```assembly
lwz   r10, 0(r3)      # Load first
lwz   r11, 4(r3)      # Load second (interleaved)
addi  r10, r10, 1     # Increment first
addi  r11, r11, 1     # Increment second
stw   r10, 0(r3)      # Store first
stw   r11, 4(r3)      # Store second
```

### Usage Pattern

```cpp
// Disable optimization for debugging
#pragma optimize("", off)
void DebugHelper() {
    // Unoptimized code
}
#pragma optimize("", on)

// Disable prescheduling for specific function
#pragma optimize("u", off)
void SpecificFunction() {
    // Code compiled without prescheduling
}
#pragma optimize("u", on)
```

### Matching Strategy

1. **Identify prescheduling:**
   - Look for unusual instruction interleaving
   - Independent operations mixed together
   - Instructions far from their dependencies

2. **Test:**
   - Try compiling with `/Ou` flag on/off
   - Compare objdiff instruction ordering
   - If `/Ou` matches, prescheduling was used

3. **Apply pragma:**
   - Rarely needed per-function
   - More common to use global `/Ou` flag
   - Use pragma only if specific functions differ from project setting

### Compiler Flag Requirements

**Critical:** These pragmas only work if base optimization is enabled:

```makefile
# These flags enable pragma optimize to work:
/O1    # Minimize size
/O2    # Maximize speed (DC3 likely uses this)
/Ox    # Full optimization

# Without above flags, these have NO EFFECT:
/Ou    # Prescheduling
/Oz    # Inline assembly optimization (tested: no effect on DC3 - no inline asm in codebase)
```

---

## Minimal: `#pragma bitfield_order`

**Rarely affects decompilation for DC3.**

### Default Behavior
- PowerPC default: `msb_to_lsb` (most-significant bit first)
- Xbox 360 uses this by default
- No pragma needed for typical code

### Only Relevant When:
- Interfacing with x86 data structures
- Cross-platform shared headers (PC/Xbox)
- Bitfield access generates wrong masks/shifts

### Quick Test
If bitfield code doesn't match:
```cpp
struct Flags {
    unsigned int flag1 : 1;
    unsigned int flag2 : 3;
    unsigned int flag3 : 4;
};

// Try default first (usually correct)
// If wrong, try:
#pragma bitfield_order(lsb_to_msb)
struct Flags {
    // Same definition
};
#pragma bitfield_order(msb_to_lsb)  // Restore default
```

---

## Interaction Matrix

### Compiler Flags vs Pragmas

| Flag | `fp_contract` Behavior | `optimize("u")` Behavior |
|------|----------------------|-------------------------|
| `/fp:fast` | Pragma works | Requires `/O1`/`/O2`/`/Ox` |
| `/fp:precise` | Pragma works | Requires `/O1`/`/O2`/`/Ox` |
| `/fp:strict` | **IGNORED** (forced OFF) | Requires `/O1`/`/O2`/`/Ox` |
| `/O2` | No effect on pragma | Enables pragma |
| `/Ou` | No effect | Global prescheduling |

### Critical Rules

1. **`/fp:strict` completely disables `fp_contract`**
   - Pragma is ignored
   - No fused multiply-add possible
   - DC3 almost certainly does NOT use `/fp:strict`

2. **Optimization pragmas require base optimization**
   - `#pragma optimize("u", on)` needs `/O1` or `/O2` or `/Ox`
   - Without base optimization, pragma has no effect

3. **Pragma scope is file-level**
   - Must appear outside functions
   - Takes effect at next function definition
   - Not function-scoped like attributes

---

## DC3 Specific Guidance

### Expected Configuration

Based on debug build characteristics:
```makefile
/O2              # Speed optimization (likely)
/fp:precise      # or /fp:fast (NOT /fp:strict)
/Ou              # Prescheduling (TBD - test to confirm)
```

### Default Pragma States

| Pragma | Expected Default |
|--------|-----------------|
| `fp_contract` | ON (fused multiply-add enabled) |
| `bitfield_order` | `msb_to_lsb` (PowerPC default) |
| Prescheduling | Per `/Ou` flag (unknown) |

### Common Scenarios

**Scenario 1: Function uses `fmadds` but won't match**
```cpp
// Add at top of file:
#pragma fp_contract(on)
```

**Scenario 2: Function needs separate multiply/add**
```cpp
#pragma fp_contract(off)
void SpecificFunction() { /* ... */ }
#pragma fp_contract(on)
```

**Scenario 3: Instruction ordering completely wrong**
- Test with `/Ou` flag on/off
- Check if prescheduling was used
- Rarely needs per-function pragma

---

## Quick Decision Tree

```
Does function use fmadds/fmadd instructions?
├─ YES → Ensure fp_contract is ON
│         #pragma fp_contract(on)
└─ NO → Check if multiply + add are separate
          ├─ YES → Ensure fp_contract is OFF
          │         #pragma fp_contract(off)
          └─ NO → Not a multiply-add pattern

Is instruction ordering completely scrambled?
├─ YES → Test with /Ou flag
│         ├─ Matches with /Ou → Prescheduling enabled
│         └─ Still wrong → Other issue
└─ NO → Ordering is fine

Do bitfield accesses use wrong offsets?
├─ YES → Try #pragma bitfield_order(lsb_to_msb)
└─ NO → Leave at default (msb_to_lsb)
```

---

## Testing Workflow

1. **Baseline Test:**
   - Compile with current settings
   - Check objdiff output
   - Note instruction differences

2. **Test `fp_contract`:**
   - Add `#pragma fp_contract(on)` at top of file
   - Rebuild and check objdiff
   - If worse, try `#pragma fp_contract(off)`

3. **Test Prescheduling:**
   - Modify build flags to toggle `/Ou`
   - Rebuild and check objdiff
   - If `/Ou` improves match, prescheduling was used

4. **Verify:**
   - Check instruction count matches
   - Check instruction opcodes match
   - Check register allocation matches

---

## Reference Locations

Full documentation:
- `docs/decomp/XBOX360_PRAGMA_REFERENCE.md`

Source files:
- `/tmp/claude/xdk_docs/dev_compiler_pragma_fp_contract.htm`
- `/tmp/claude/xdk_docs/dev_compiler_pragma_optimize.htm`
- `/tmp/claude/xdk_docs/dev_compiler_ouoz.htm`
