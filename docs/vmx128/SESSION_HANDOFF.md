# VMX128 Ghidra Project - Session Handoff

## Context Docs (read these first)
- `docs/vmx128/README.md` - Project overview and status
- `docs/vmx128/GESTURE_TARGETS.md` - **Phase 5 decomp targets and progress**
- `docs/vmx128/PHASE4_TODO.md` - **Prioritized SLEIGH improvements with usage stats**
- `docs/vmx128/COMPARISON_REPORT.md` - Validation results
- `docs/vmx128/REFERENCE_SOURCES.md` - **Authoritative sources (QEMU, RPCS3, official docs)**
- `src/xdk/LIBCMT/vectorintrinsics.h` - Local XDK intrinsic surface (XMVECTOR == VMX reg)
- `docs/vmx128/VCMPBFP128_SEMANTICS.md` - vcmpbfp128 detailed semantics (2-bit bounds flags)

## Current State (2026-01-25)

**Phase 5 in progress** - Using VMX128 tooling to decomp gesture/skeleton code.

### What's Complete
- ✓ All 77 VMX128 opcodes disassemble correctly
- ✓ Extended registers (v32-v127) decode properly
- ✓ Lane-wise FP semantics for all arithmetic instructions
- ✓ Lane-wise compare masks for `vcmpgtfp128`, `vcmpeqfp128`, `vcmpgefp128`, `vcmpequw128`
- ✓ vcmpbfp128 bounds flags (bit31 = a > b, bit30 = a < -b, NaN -> 0xC0000000)
- ✓ Ghidra build with full VMX128 support (extracted to ghidra-test/)

### Lane-wise FP Instructions (Complete)
- `vaddfp128`, `vsubfp128`, `vmulfp128` - Basic FP ops
- `vmaddfp128` - vD = vA * vB + vD
- `vmaddcfp128` - vD = vA * vD + vB (carryout variant)
- `vnmsubfp128` - vD = vD - vA * vB
- `vmaxfp128`, `vminfp128` - Conditional selection via if-goto
- `vspltw128` - Dynamic lane extraction
- Logical ops (vand128, vor128, vxor128, etc.) - Native bitwise

### Decomp Progress

| Function | VMX128 | Before | After | Status |
|----------|--------|--------|-------|--------|
| `JointScreenPos(Vector2)` | 1 | 100% | 100% | ✅ Complete |
| `JointScreenPos(Vector3)` | 1 | 37% | **89%** | ✅ AT_LIMIT |
| `FreestyleMotionFilter::UpdateFilters` | 0 | 40.3% | **81%** | ✅ AT_LIMIT |
| `FastInterp` (Quat) | 0 | 68.6% | **99.6%** | ✅ AT_LIMIT |
| `SkeletonFrame::Create` | 25 | 13% | **64%** | 🔧 In progress |

**13 additional targets identified** - see [GESTURE_TARGETS.md](GESTURE_TARGETS.md)

### Remaining SLEIGH Priorities

From PHASE4_TODO.md (sorted by impact):

| Priority | Instructions | Impact | Status |
|----------|--------------|--------|--------|
| P3 HIGH | vcmpgtfp128, vcmpeqfp128, vcmpgefp128, vcmpequw128, vcmpbfp128 | 26.5% (9,807 insts) | DONE |
| P4 MED-HIGH | vmsum3fp128, vmsum4fp128 (dot products) | 966 insts | DONE |
| P5 MED | vrsqrtefp128, vrefp128 | 377 insts | DONE |
| P6 MED | vperm128, vpermwi128 | 2,650 insts | DONE |
| P10 LOW | vpkd3d128, vupkd3d128 | 278 insts | NOT STARTED |
| P11 LOW | vpk*/vupk* saturation | ~1,500 insts | NOT STARTED |

Comparison instructions now produce lane-wise masks and update CR6 for dot variants.

### Known Blockers

1. **VMX vs VMX128 instruction selection** - Compiler chooses internally based on register pressure
2. **fmadds vs fmuls+fadds** - Compiler optimization flag we can't control
3. **Merged function calls** - LTCG merges identical template instantiations
4. **Loop structure** - Compiler uses `blt` instead of `bdnz` (count register) loops

## Key Paths

| What | Path |
|------|------|
| SLEIGH file | `~/code/milohax/vmx128-research/ghidra-vmx128/Ghidra/Processors/PowerPC/data/languages/vmx128.sinc` |
| VMX128 intrinsics | `~/code/milohax/rb3-xenon/src/xdk/LIBCMT/vectorintrinsics.h` |
| Gesture targets doc | `~/code/milohax/rb3-xenon/docs/vmx128/GESTURE_TARGETS.md` |
| Extracted Ghidra | `~/code/milohax/vmx128-research/ghidra-test/ghidra_12.0_DEV/` |

## Quick Commands

### Decomp Workflow
```bash
# Analyze a function
./bin/analyze-function "FunctionName"

# Check match with verdict
./bin/objdiff-cli diff -p . "FunctionName" --verdict -f markdown

# Build specific object and check
ninja build/45410914/src/system/gesture/File.obj
./bin/objdiff-cli diff -p . "FunctionName" --build --verdict
```

### Ghidra Build (if editing SLEIGH)
```bash
export JAVA_HOME=/usr/lib/jvm/java-25-openjdk
export GRADLE_USER_HOME=/tmp/claude/gradle-home
cd ~/code/milohax/vmx128-research/ghidra-vmx128

# SLEIGH only (fast)
gradle --project-cache-dir=/tmp/claude/gradle-project :PowerPC:sleigh

# Full build
gradle --project-cache-dir=/tmp/claude/gradle-project buildGhidra

# Extract
cd ~/code/milohax/vmx128-research
unzip -o ghidra-vmx128/build/dist/ghidra_12.0_DEV_*_linux_x86_64.zip -d ghidra-test/
```

## Next Steps

1. **Rebuild SLEIGH + re-run validation** - confirm updated semantics in decompiler output
2. **NuiTransformMatrixLevel** - Last Tier 1 decomp target (0% currently)
3. **Tier 2 targets** - SkeletonViz::*, Skeleton::Poll, HandInvokeGestureFilter::CalcInPose
4. **Optional low-impact ops** - D3D pack/unpack and saturation pack/unpack if needed

## VMX128 Intrinsics Reference

```cpp
// Working intrinsics (in vectorintrinsics.h)
XMVECTOR __vmaddfp(XMVECTOR mul1, XMVECTOR mul2, XMVECTOR addend);  // -> vmaddfp (NOT vmaddfp128!)
XMVECTOR __vspltw(XMVECTOR vSrcA, unsigned int uImmed);            // -> vspltw (NOT vspltw128!)
static inline XMVECTOR __lvx(const void *base, int offset);         // -> lvx
static inline void __stvx(XMVECTOR vSrc, void *base, int offset);   // -> stvx

// Missing (generates function call - no user-accessible VMX128 intrinsics exist)
XMVECTOR __vmaddcfp128(...);  // vmaddcfp128
```

**Note**: Standard VMX intrinsics generate standard VMX instructions (v0-v31 only). The compiler chooses VMX128 variants automatically when register pressure requires extended registers (v32-v127).
