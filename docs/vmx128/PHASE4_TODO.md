# VMX128 Tooling Improvements

**Phase 4 Status**: Mostly complete. High-impact lane-wise semantics and CR6 updates are implemented. Remaining gaps are low-impact pack/unpack and D3D-specific ops.

## Key Gaps (Resolved in vmx128.sinc)

- **Dot products**: `vmsum3fp128`, `vmsum4fp128` now use lane-wise dot product math and splat.
- **Permute/shuffle**: `vperm128`, `vpermwi128` now implement byte/word permutations.
- **Reciprocal/sqrt estimates**: `vrefp128`, `vrsqrtefp128` now use lane-wise exact math.
- **Merge/shift/rotate**: `vmrghw128`, `vmrglw128`, `vslw128`, `vsraw128`, `vsrw128`, `vrlw128` now use explicit lane ops.
- **Rounding**: `vrfim128`, `vrfin128`, `vrfip128`, `vrfiz128` now use `floor/round/ceil/trunc`.
- **Select**: `vsel128` now uses `(vA & ~mask) | (vB & mask)`.
- **Compare `.` variants**: CR6 updates implemented; `vcmpbfp128.` sets CR6 EQ for all-in-bounds.

## Remaining Gaps (Low Impact)

- **D3D pack/unpack**: `vpkd3d128`, `vupkd3d128` still use pcodeop stubs.
- **Pack/unpack saturation**: `vpk*128`/`vupk*128` saturation variants still use pcodeop stubs.

Ghidra SLEIGH improvements and compiler intrinsic gaps discovered during decomp work.

## Instruction Usage Statistics (DC3)

From analysis of `ham_xbox_r.exe` (scanner: `tmp/vmx128/vmx128_scanner.py`):

| Category | Instructions | Percentage |
|----------|-------------|------------|
| Comparisons | 9,807 | 26.5% |
| Permute/Shuffle | 2,650 | 7.2% |
| Dot Products | 966 | 2.6% |
| Reciprocal/Sqrt | 377 | 1.0% |
| D3D Pack/Unpack | 278 | 0.8% |

---

## ~~Priority 1: Non-128 Partial Load/Store Instructions~~ ✓ FIXED

**Status**: Resolved 2025-01-25

**Solution**: Used standard `vrD`, `A`, `B`, `OP`, `XOP_0_10` token fields from `ppc_common.sinc` instead of vmx128-specific tokens. These Cell/Xenon instructions (opcode 31) now work correctly:
- `lvlx` / `lvlxl` - Load Vector Left Indexed
- `lvrx` / `lvrxl` - Load Vector Right Indexed
- `stvlx` / `stvlxl` - Store Vector Left Indexed
- `stvrx` / `stvrxl` - Store Vector Right Indexed

---

## ~~Priority 2: Lane-wise FP Arithmetic Semantics~~ ✓ COMPLETE

**Status**: Implemented 2026-01-25

**Solution**: Replaced pcodeop stubs with explicit 4-lane FP operations using SLEIGH `f+`, `f-`, `f*` operators.

**Instructions updated**:
- `vaddfp128` - 4-lane FP add using `f+`
- `vsubfp128` - 4-lane FP subtract using `f-`
- `vmulfp128` - 4-lane FP multiply using `f*`
- `vmaddfp128` - 4-lane FP multiply-add: `vD = vA * vB + vD`
- `vmaddcfp128` - 4-lane FP multiply-add carryout: `vD = vA * vD + vB` (note different operand order!)
- `vnmsubfp128` - 4-lane FP negative multiply-subtract: `vD = vD - vA * vB`
- `vmaxfp128` - 4-lane FP maximum using if-goto branches
- `vminfp128` - 4-lane FP minimum using if-goto branches
- `vspltw128` - Dynamic lane extraction and splat

**Lane mapping (Big-Endian PowerPC)**:
- Word 0 (most significant) = bits [96,32]
- Word 1 = bits [64,32]
- Word 2 = bits [32,32]
- Word 3 (least significant) = bits [0,32]

**Benefit**: Ghidra decompiler now shows actual lane operations instead of opaque function calls, enabling better dataflow analysis and constant propagation.

**SLEIGH Technique for Conditional Selection (vmaxfp128/vminfp128)**:

SLEIGH comparison operators (`f>=`, `f<`, etc.) return 1-bit booleans that cannot be used in arithmetic. The solution is to use `if-goto` branches:

```sleigh
# max(a, b) - CORRECT approach
local res:4 = a;
if (a f>= b) goto <done>;
res = b;
<done>

# WRONG - size mismatch (boolean * float)
local res:4 = (a f>= b) * a + (a f< b) * b;
```

**Note on vmaddcfp128 vs vmaddfp128**:
- `vmaddfp128`: `vD = vA * vB + vD` (vD is addend and destination)
- `vmaddcfp128`: `vD = vA * vD + vB` (vD is multiplicand and destination, vB is addend)
- The "carryout" variant is useful for chained matrix-vector multiplies

**Tested on**: `SkeletonFrame::Create` - matrix-vector multiply patterns now visible

---

## Priority 3: Lane-wise Comparison Semantics

**Status**: COMPLETE (all compare masks implemented; vcmpbfp128 uses 2-bit bounds flags)
**Impact**: HIGH - 26.5% of all VMX128 usage (9,807 instructions)

Currently all comparison instructions use pcodeop stubs. They should produce lane-wise masks (0xFFFFFFFF for true, 0x00000000 for false).

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vcmpgtfp128` | 8,020 | lane-wise mask | DONE |
| `vcmpeqfp128` | 997 | lane-wise mask | DONE |
| `vcmpgefp128` | 268 | lane-wise mask | DONE |
| `vcmpequw128` | 350 | lane-wise mask | DONE |
| `vcmpbfp128` | 172 | 2-bit bounds flags | DONE - see [VCMPBFP128_SEMANTICS.md](VCMPBFP128_SEMANTICS.md) |

**Note**: Matching bounds-flag semantics were also applied to standard AltiVec `vcmpbfp` in `altivec.sinc`.

**Implementation Pattern**:
```sleigh
# For vcmpgtfp128 - produce 0xFFFFFFFF if true, 0x00000000 if false per lane
local a_0:4 = vregA[96,32];
local b_0:4 = vregB[96,32];
local mask_0:4 = 0;
if (a_0 f> b_0) goto <set_lane0>;
goto <lane1>;
<set_lane0>
mask_0 = 0xFFFFFFFF;
<lane1>
# ... repeat for lanes 1-3
vregD[96,32] = mask_0;
# ... etc
```

**Note**: The `.` suffix variants (e.g., `vcmpgtfp128.`) should also update CR6, but that's lower priority (see Priority 7).

---

## Priority 4: Dot Product Instructions

**Status**: COMPLETE
**Impact**: MEDIUM-HIGH - 966 instructions, heavily used in gesture/skeleton code

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vmsum3fp128` | 429 | lane-wise dot + splat | DONE |
| `vmsum4fp128` | 537 | lane-wise dot + splat | DONE |

**Source References**:
- Xenia vmx128.txt: lines 315-328
- These are VMX128-specific (not in QEMU/standard AltiVec)

**Encoding** (from Xenia vmx128.txt):
```
vmsum3fp128:  |0 0 0 1 0 1|  VD128  |  VA128  |  VB128  |A|0 1 1 0|a|1|VDh|VBh|
vmsum4fp128:  |0 0 0 1 0 1|  VD128  |  VA128  |  VB128  |A|0 1 1 1|a|1|VDh|VBh|
```

**Semantics**:
- vmsum3fp128: `vD = splat(vA.x*vB.x + vA.y*vB.y + vA.z*vB.z)` (ignores w lane)
- vmsum4fp128: `vD = splat(vA.x*vB.x + vA.y*vB.y + vA.z*vB.z + vA.w*vB.w)`
- Result is splatted to ALL 4 lanes (not just one)

**SLEIGH Implementation**:
```sleigh
# vmsum3fp128: 3D dot product, result splatted to all lanes
local a0:4 = vA[96,32]; local b0:4 = vB[96,32];
local a1:4 = vA[64,32]; local b1:4 = vB[64,32];
local a2:4 = vA[32,32]; local b2:4 = vB[32,32];
local sum:4 = (a0 f* b0) f+ (a1 f* b1) f+ (a2 f* b2);
vD[96,32] = sum; vD[64,32] = sum; vD[32,32] = sum; vD[0,32] = sum;

# vmsum4fp128: 4D dot product, result splatted to all lanes
local a0:4 = vA[96,32]; local b0:4 = vB[96,32];
local a1:4 = vA[64,32]; local b1:4 = vB[64,32];
local a2:4 = vA[32,32]; local b2:4 = vB[32,32];
local a3:4 = vA[0,32];  local b3:4 = vB[0,32];
local sum:4 = (a0 f* b0) f+ (a1 f* b1) f+ (a2 f* b2) f+ (a3 f* b3);
vD[96,32] = sum; vD[64,32] = sum; vD[32,32] = sum; vD[0,32] = sum;
```

---

## Priority 5: Reciprocal/Sqrt Estimate Instructions

**Status**: COMPLETE
**Impact**: MEDIUM - 377 instructions

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vrsqrtefp128` | 327 | lane-wise `1.0 / sqrt(x)` | DONE |
| `vrefp128` | 50 | lane-wise `1.0 / x` | DONE |

**Source References**:
- QEMU `target/ppc/int_helper.c`:
  - `helper_vrefp`: lines 1526-1533
  - `helper_vrsqrtefp`: lines 1553-1562
- Xenia vmx128.txt: lines 448-452 (vrefp128), lines 503-507 (vrsqrtefp128)

**Encoding** (from Xenia vmx128.txt):
```
vrefp128:      |0 0 0 1 1 0|  VD128  |0 0 0 0 0|  VB128  |1 1 0 0 0 1 1|VDh|VBh|
vrsqrtefp128:  |0 0 0 1 1 0|  VD128  |0 0 0 0 0|  VB128  |1 1 0 0 1 1 1|VDh|VBh|
```

**Semantics** (from QEMU):
```c
// vrefp - reciprocal estimate
for (i = 0; i < 4; i++) {
    r->f32[i] = 1.0f / b->f32[i];  // Actually exact division in QEMU
}

// vrsqrtefp - reciprocal square root estimate
for (i = 0; i < 4; i++) {
    float t = sqrt(b->f32[i]);
    r->f32[i] = 1.0f / t;
}
```

**Technical Note**: Hardware produces ~12-bit mantissa precision estimates. QEMU/emulators use exact math for simplicity. For decompiler analysis, exact semantics are fine - the precision difference doesn't affect dataflow understanding.

**SLEIGH Implementation**:
```sleigh
# vrefp128: lane-wise reciprocal estimate
local b0:4 = vB[96,32]; local b1:4 = vB[64,32];
local b2:4 = vB[32,32]; local b3:4 = vB[0,32];
local one:4 = 0x3F800000;  # 1.0f in IEEE 754
vD[96,32] = one f/ b0; vD[64,32] = one f/ b1;
vD[32,32] = one f/ b2; vD[0,32]  = one f/ b3;

# vrsqrtefp128: lane-wise reciprocal sqrt estimate
local b0:4 = vB[96,32]; local b1:4 = vB[64,32];
local b2:4 = vB[32,32]; local b3:4 = vB[0,32];
local one:4 = 0x3F800000;
vD[96,32] = one f/ sqrt(b0); vD[64,32] = one f/ sqrt(b1);
vD[32,32] = one f/ sqrt(b2); vD[0,32]  = one f/ sqrt(b3);
```

---

## Priority 6: Permute/Shuffle Operations

**Status**: COMPLETE
**Impact**: MEDIUM - 2,650 instructions

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vperm128` | 1,961 | byte-wise permute | DONE |
| `vpermwi128` | 689 | word-wise permute | DONE |

**Source References**:
- QEMU `target/ppc/int_helper.c`: `helper_VPERM` lines 1237-1253
- Xenia vmx128.txt: lines 362-365 (vperm128), lines 369-372 (vpermwi128)

**Encoding** (from Xenia vmx128.txt):
```
vperm128:    |0 0 0 1 0 1|  VD128  |  VA128  |  VB128  |A|0| VC  |a|0|VDh|VBh|
vpermwi128:  |0 0 0 1 1 0|  VD128  |  PERMl  |  VB128  |0|1|PERMh|0|1|VDh|VBh|
```

Note: `vperm128` uses only VC bits [2:0] (vr0-vr7 only) for permutation control.

**Semantics** (from QEMU `helper_VPERM`):
```c
// vperm - byte-wise permutation from concatenated vA||vB
for (i = 0; i < 16; i++) {
    int s = c->VsrB(i) & 0x1f;  // 5-bit selector
    int index = s & 0xf;         // byte index (0-15)
    if (s & 0x10) {
        result.VsrB(i) = b->VsrB(index);  // Select from vB
    } else {
        result.VsrB(i) = a->VsrB(index);  // Select from vA
    }
}
```

**vpermwi128 Semantics**:
Word-wise permutation using 8-bit immediate (PERMh<<5 | PERMl):
- 2 bits per word select which source word goes to each destination word
- Immediate encodes 4 x 2-bit selectors

**SLEIGH Implementation** (vpermwi128 - simpler):
```sleigh
# vpermwi128 vD, vB, PERM
# PERM encodes 4 x 2-bit selectors: [d3:d2:d1:d0]
# Each 2-bit value selects which word from vB goes to that position
local perm:1 = PERM;
local sel0:1 = (perm >> 0) & 3;
local sel1:1 = (perm >> 2) & 3;
local sel2:1 = (perm >> 4) & 3;
local sel3:1 = (perm >> 6) & 3;
# ... complex word selection based on selectors
```

**Technical Note**: `vperm128` now implemented via 16 per-byte selectors (low 5 bits) choosing from vA/vB.

---

## Priority 7: CR6 Update for Comparison `.` Variants

**Status**: COMPLETE
**Impact**: LOW - neither current targets nor standard AltiVec implement this

**Problem**: The `.` variants of comparison instructions should update CR6:
- `vcmpgtfp128.` - Compare Greater-Than FP with CR update
- `vcmpeqfp128.` - Compare Equal FP with CR update
- `vcmpgefp128.` - Compare Greater-Equal FP with CR update
- etc.

**Current**: Dot variants update CR6. For `vcmpbfp128.`, CR6[EQ] is set when all lanes are in-bounds (all result lanes = 0).

**CR6 fields for vector comparisons**:
- **CR6[0] (LT)**: All elements true (all 1s in result)
- **CR6[1] (GT)**: Reserved (0)
- **CR6[2] (EQ)**: All elements false (all 0s in result)
- **CR6[3] (SO)**: Reserved (0)

**Note**: Standard AltiVec comparisons in `altivec.sinc` also have `# TODO change CR6` comments - this is an upstream Ghidra issue.

---

## Priority 8: Merge/Shift/Rotate Operations

**Status**: COMPLETE
**Impact**: LOW - ~1,200 instructions combined

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vmrghw128` | 257 | lane-wise interleave | DONE |
| `vmrglw128` | 185 | lane-wise interleave | DONE |
| `vslw128` | 185 | lane-wise shift left | DONE |
| `vsraw128` | 109 | lane-wise shift right (arith) | DONE |
| `vsrw128` | 108 | lane-wise shift right (logic) | DONE |
| `vrlw128` | 91 | lane-wise rotate left | DONE |

These are implemented as straightforward lane-wise operations (shift amounts masked to 5 bits).

---

## Priority 9: Rounding Instructions

**Status**: COMPLETE
**Impact**: VERY LOW - 50 instructions

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vrfiz128` | 24 | trunc | DONE |
| `vrfin128` | 16 | round | DONE |
| `vrfip128` | 5 | ceil | DONE |
| `vrfim128` | 5 | floor | DONE |

**Implementation**: SLEIGH has `trunc`, `ceil`, `floor`, `round` operators.

---

## Priority 10: D3D Pack/Unpack Instructions (Xbox 360 Specific)

**Status**: NOT STARTED
**Impact**: VERY LOW - 278 instructions, only relevant for graphics decomp

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vpkd3d128` | 163 | pcodeop | Pack to D3D vertex format |
| `vupkd3d128` | 115 | pcodeop | Unpack from D3D vertex format |

**Technical Challenge**: These handle Xbox 360-specific packed formats (D3DCOLOR, FLOAT16_2, UDEC3, DEC3N, etc.). Requires D3DType decode tables.

---

## Priority 11: Pack/Unpack Saturation Operations

**Status**: NOT STARTED
**Impact**: VERY LOW - ~1,500 instructions but complex saturation logic

8 `vpk*128` variants and 4 `vupk*128` variants for signed/unsigned half-word/word saturation. Low frequency individually.

---

## Priority 12: Select Operation

**Status**: COMPLETE
**Impact**: VERY LOW - 78 instructions

| Instruction | Count | Current | Needed |
|-------------|-------|---------|--------|
| `vsel128` | 78 | bitwise mux | DONE |

Simple but rarely used.

---

## Already Native (No Changes Needed)

These instructions already have native SLEIGH semantics:

- `vand128`, `vandc128`, `vor128`, `vxor128`, `vnor128` - Bitwise operations
- `lvx128`, `lvxl128`, `stvx128`, `stvxl128` - Aligned load/store
- `vsldoi128` - Shift left double by octet immediate

---

## Missing Compiler Intrinsics - ANALYSIS COMPLETE

**Status**: Understood - fundamental compiler limitation

The key issue is that **standard VMX intrinsics generate standard VMX instructions** (limited to v0-v31), not VMX128 instructions (v0-v127):

- `__vmaddfp` → generates `vmaddfp` (standard VMX, v0-v31 only)
- Target binary uses `vmaddfp128` with extended registers (v32-v127)

### Working Intrinsics

These are in `src/xdk/LIBCMT/vectorintrinsics.h`:

```cpp
XMVECTOR __vmaddfp(XMVECTOR mul1, XMVECTOR mul2, XMVECTOR addend);  // -> vmaddfp (NOT vmaddfp128!)
XMVECTOR __vspltw(XMVECTOR vSrcA, unsigned int uImmed);            // -> vspltw (NOT vspltw128!)
static inline XMVECTOR __lvx(const void *base, int offset);         // -> lvx
static inline void __stvx(XMVECTOR vSrc, void *base, int offset);   // -> stvx
```

**Note**: These generate **standard VMX** instructions, not VMX128 variants. The compiler chooses VMX128 variants automatically when register pressure requires it.

### Missing Intrinsics (Generate Function Calls)

| Intrinsic | Instruction | Issue | Workaround |
|-----------|-------------|-------|------------|
| `__vmaddcfp128` | `vmaddcfp128` | Not a compiler builtin | None - no VMX128-specific intrinsics exist |
| `__vnmsubfp128` | `vnmsubfp128` | Not a compiler builtin | None |
| `__vpermwi128` | `vpermwi128` | Not a compiler builtin | None |

### Conclusions

1. **No direct fix exists** - VMX128 instruction selection is compiler-internal
2. **Inline assembly might work** - but MSVC-style `__asm` blocks for VMX128 are undocumented
3. **Accept functional equivalence** - when `vmaddfp` produces correct results despite different codegen
4. **Accept mismatch** - some functions using VMX128 extended registers will have irreducible differences

---

## Loop Structure Optimization

**Status**: Nice-to-have

The Xbox 360 compiler sometimes uses `bdnz` (branch-decrement-not-zero) loops with the count register, while our code generates `blt` (compare-branch) loops.

**Example from SkeletonFrame::Create**:
```asm
# Target uses count register loop
mtctr r9           # Set loop count
loop:
  ...
  bdnz loop        # Decrement CTR and branch if not zero

# Our code generates compare loop
loop:
  ...
  addi r9, r9, 1
  cmpwi r9, 20
  blt loop
```

**Potential Solutions**:
1. Try `#pragma` directives for loop optimization
2. Use different loop constructs (`do-while` vs `for`)
3. Accept the mismatch if functionally equivalent

---

## Build Commands

```bash
cd ~/code/milohax/vmx128-research/ghidra-vmx128
export JAVA_HOME=/usr/lib/jvm/java-25-openjdk
export GRADLE_USER_HOME=/tmp/claude/gradle-home  # If ~/.gradle is read-only

# SLEIGH only (fast iteration) - use this when editing vmx128.sinc
gradle --project-cache-dir=/tmp/claude/gradle-project :PowerPC:sleigh

# Full Ghidra build (needed to test with analyzeHeadless)
gradle --project-cache-dir=/tmp/claude/gradle-project buildGhidra

# After full build, extract to test location
cd ~/code/milohax/vmx128-research
unzip -o ghidra-vmx128/build/dist/ghidra_12.0_DEV_*_linux_x86_64.zip -d ghidra-test/
```

---

## Headless Testing

### Quick Validation (no analysis, just instruction decode)

```bash
# Run VMX128 validation script on DC3
~/code/milohax/vmx128-research/ghidra-test/ghidra_12.0_DEV/support/analyzeHeadless \
    /tmp/ghidra_vmx128_test TestProject \
    -process ham_xbox_r.exe \
    -postScript VMX128Validate.java \
    -scriptPath ~/code/milohax/vmx128-research/ghidra-scripts \
    -noanalysis
```

### Fresh Import + Full Analysis

```bash
mkdir -p /tmp/ghidra_vmx128_test

~/code/milohax/vmx128-research/ghidra-test/ghidra_12.0_DEV/support/analyzeHeadless \
    /tmp/ghidra_vmx128_test TestProject \
    -import ~/code/milohax/rb3-xenon/orig/45410914/ham_xbox_r.exe \
    -processor "PowerPC:BE:64:Xenon" \
    -postScript VMX128Validate.java \
    -scriptPath ~/code/milohax/vmx128-research/ghidra-scripts \
    -overwrite
```

### Decompile Specific Function

```bash
# Decompile a function to verify SLEIGH semantics appear correctly
~/code/milohax/vmx128-research/ghidra-test/ghidra_12.0_DEV/support/analyzeHeadless \
    /tmp/ghidra_vmx128_test TestProject \
    -process ham_xbox_r.exe \
    -postScript DecompileFunction.java "SkeletonFrame::Create" \
    -scriptPath ~/code/milohax/vmx128-research/ghidra-scripts \
    -noanalysis
```

### Test Scripts Location

```
~/code/milohax/vmx128-research/ghidra-scripts/
├── VMX128Validate.java    # Instruction count validation (works in headless)
├── DecompileFunction.java # Decompile specific function
├── vmx128_validate.py     # Python version (needs PyGhidra)
├── vmx128_decompile.py    # Decompiler output test
└── vmx128_compare.sh      # Stock vs modified comparison
```

---

## Key Files to Edit

```
~/code/milohax/vmx128-research/ghidra-vmx128/Ghidra/Processors/PowerPC/data/languages/
├── vmx128.sinc            # VMX128 instruction definitions - EDIT THIS
├── ppc_64_xenon.slaspec   # Xenon processor variant
├── ppc.ldefs              # Language definitions
└── ppc_common.sinc        # Common PPC definitions (reference)
```

The main file for SLEIGH work is `vmx128.sinc`.

---

## Related Docs

- [GESTURE_TARGETS.md](GESTURE_TARGETS.md) - Phase 5 decomp targets using VMX128
- [SESSION_HANDOFF.md](SESSION_HANDOFF.md) - Quick context for new sessions
- [TESTING.md](TESTING.md) - Detailed testing procedures
- [COMPARISON_REPORT.md](COMPARISON_REPORT.md) - Validation results
