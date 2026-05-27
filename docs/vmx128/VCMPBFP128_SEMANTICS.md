# vcmpbfp128 - Vector Compare Bounds Floating Point

## Summary

`vcmpbfp128` checks whether each floating-point element in vA is within the bounds defined by vB (i.e., within the range `[-vB, +vB]`). Unlike other comparison instructions, it produces a **2-bit result code per lane**, not a full boolean mask.

**Important**: Earlier SLEIGH implementations used a boolean mask and were incorrect. Ensure your `vmx128.sinc` matches the [Correct Implementation](#correct-sleigh-implementation) below.

---

## Instruction Format

```
vcmpbfp128  vD, vA, vB     # Compare bounds, result to vD
vcmpbfp128. vD, vA, vB     # Compare bounds, result to vD, update CR6
```

**Encoding**: `0x18000180` (VX128 format)

---

## Per-Lane Operation

For each 32-bit floating-point element `i` (lanes 0-3):

```
LE = (vA[i] <= vB[i])     # Is a less-than-or-equal to upper bound?
GE = (vA[i] >= -vB[i])    # Is a greater-than-or-equal to lower bound?

vD[i][31] = !LE           # NLE bit: set if a > b (exceeds upper bound)
vD[i][30] = !GE           # NGE bit: set if a < -b (exceeds lower bound)
vD[i][29:0] = 0           # Bits 0-29 are always zero
```

### Result Values

| Condition | Bit 31 | Bit 30 | Result | Meaning |
|-----------|--------|--------|--------|---------|
| `-b <= a <= b` | 0 | 0 | `0x00000000` | In bounds |
| `a > b` | 1 | 0 | `0x80000000` | Exceeds upper bound |
| `a < -b` | 0 | 1 | `0x40000000` | Exceeds lower bound |
| NaN in operand | 1 | 1 | `0xC0000000` | Unordered (NaN) |

---

## NaN Handling

When either operand is NaN, the comparison is **unordered**:
- Both bit 31 and bit 30 are set
- Result is `0xC0000000` for that lane

This follows IEEE 754 semantics where NaN comparisons are always false:
- `NaN <= b` is false → bit 31 set
- `NaN >= -b` is false → bit 30 set

---

## Why Not a Standard Mask?

Unlike `vcmpgtfp128`, `vcmpeqfp128`, etc. which produce full boolean masks (`0xFFFFFFFF` or `0x00000000`), `vcmpbfp128` produces a 2-bit code. This is why GCC's altivec.h notes:

> "vec_cmpb (Vector Compare Bounds Floating-Point) is not supported, as the result is not a standard mask"

The instruction is designed for efficient bounds checking (e.g., 3D clipping) where you need to know *which* bound was exceeded, not just whether the value is in/out of bounds.

---

## Predicate Usage

The `.` variant updates CR6 for use with predicates:

| Intrinsic | CR6 Test | Meaning |
|-----------|----------|---------|
| `vec_all_in(a, b)` | CR6[EQ] = 1 | All lanes have result = 0 (all in bounds) |
| `vec_any_out(a, b)` | CR6[EQ] = 0 | Any lane has result != 0 (any out of bounds) |

---

## Difference from AltiVec vcmpbfp

`vcmpbfp128` (VMX128) and `vcmpbfp` (standard AltiVec) have **identical semantics**. The only difference is encoding:

| Variant | Register Range | Specifier Bits |
|---------|---------------|----------------|
| `vcmpbfp` | v0-v31 | 5-bit contiguous |
| `vcmpbfp128` | v0-v127 | 7-bit non-contiguous |

---

## Incorrect Implementation (Historical)

The current SLEIGH implementation incorrectly produces a full mask:

```sleigh
# WRONG - produces 0xFFFFFFFF or 0x00000000
local abs_0:4 = abs(a_0);
local mask_0:4 = 0;
if (abs_0 f<= b_0) goto <set_lane0>;
goto <lane1>;
<set_lane0>
mask_0 = 0xFFFFFFFF;  # WRONG: should be 0x00000000 when in bounds
```

This is incorrect because:
1. It produces a full mask instead of 2-bit code
2. The polarity is inverted (mask set when IN bounds, should be 0)
3. It doesn't distinguish upper vs lower bound violations

---

## Correct SLEIGH Implementation

```sleigh
:vcmpbfp128 vregD, vregA, vregB is ...
{
    # Lane 0
    local a_0:4 = vregA[96,32];
    local b_0:4 = vregB[96,32];
    local neg_b_0:4 = 0 f- b_0;

    local nle_0:4 = 0;
    local nge_0:4 = 0;

    # Check upper bound: a <= b
    if (a_0 f<= b_0) goto <check_ge_0>;
    nle_0 = 0x80000000;  # Bit 31: NOT (a <= b)
    <check_ge_0>

    # Check lower bound: a >= -b
    if (a_0 f>= neg_b_0) goto <done_0>;
    nge_0 = 0x40000000;  # Bit 30: NOT (a >= -b)
    <done_0>

    vregD[96,32] = nle_0 | nge_0;

    # Repeat for lanes 1, 2, 3...
    # (Lane 1 at bits [64,32], Lane 2 at [32,32], Lane 3 at [0,32])
}
```

**Note on NaN**: SLEIGH's `f<=` and `f>=` operators should return false for NaN comparisons, which would naturally set both bits. Verify this behavior with testing.

---

## Reference Sources

### Authoritative Implementation (QEMU)

Location: `target/ppc/int_helper.c` - `helper_vcmpbfp` / `vcmpbfp_internal`

```c
int le = le_rel != float_relation_greater;
int ge = ge_rel != float_relation_less;
r->u32[i] = ((!le) << 31) | ((!ge) << 30);
```

When either comparison is unordered (NaN), result is `0xC0000000`.

### Official Documentation

- [AltiVec Programming Environments Manual (ALTIVECPEM)](https://www.nxp.com/docs/en/reference-manual/ALTIVECPEM.pdf) - Chapter 6
- [Power ISA](https://openpowerfoundation.org/specifications/isa/) - Vector Facility

### Emulator References

- **QEMU**: `~/code/milohax/vmx128-research/qemu-reference/target/ppc/int_helper.c`
- **RPCS3**: `~/code/milohax/vmx128-research/rpcs3-reference/rpcs3/Emu/Cell/PPUInterpreter.cpp`

### Compiler Reference

- **GCC**: `~/code/milohax/vmx128-research/gcc-reference/gcc/config/rs6000/altivec.h` (vec_cmpb note about non-standard mask)

---

## Usage in DC3

vcmpbfp128 appears 172 times in `ham_xbox_r.exe`, primarily in:
- Collision detection (point-in-box tests)
- Animation blending (weight clamping)
- Physics bounds checking

---

## Related Documents

- [ISA_REFERENCE.md](ISA_REFERENCE.md) - Full instruction set reference
- [PHASE4_TODO.md](PHASE4_TODO.md) - Implementation priorities
- [SESSION_HANDOFF.md](SESSION_HANDOFF.md) - Quick context
