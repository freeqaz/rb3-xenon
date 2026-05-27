# VMX128 Ghidra Support Project

This folder contains documentation for adding VMX128 (Xbox 360 SIMD) support to Ghidra, enabling proper decompilation of vector-heavy code in DC3.

## Why This Matters

Large chunks of DC3 code were previously unintelligible because Ghidra didn't understand VMX128 instructions. This blocked decomp work on:
- Kinect skeleton tracking (`system/gesture/`)
- Graphics/rendering code
- Physics and math-heavy routines
- Any code using `XMVECTOR` types

**With VMX128 support, these areas are now approachable.**

## Documents

| Document | Description |
|----------|-------------|
| [PLAN.md](PLAN.md) | Project plan and phases |
| [COMPARISON_REPORT.md](COMPARISON_REPORT.md) | Validation results (stock vs modified Ghidra) |
| [TESTING.md](TESTING.md) | Headless testing guide and scripts |
| [PHASE4_TODO.md](PHASE4_TODO.md) | Tooling improvements and missing intrinsics |
| [GESTURE_TARGETS.md](GESTURE_TARGETS.md) | **Phase 5 decomp targets** |
| [ISA_REFERENCE.md](ISA_REFERENCE.md) | VMX128 instruction set reference |
| [REGISTER_ENCODING.md](REGISTER_ENCODING.md) | How 128 registers are encoded |
| [GHIDRA_IMPLEMENTATION.md](GHIDRA_IMPLEMENTATION.md) | Technical implementation notes |
| [DC3_VMX128_USAGE.md](DC3_VMX128_USAGE.md) | DC3 instruction usage analysis |

## Quick Links

- [Ghidra Issue #2094](https://github.com/NationalSecurityAgency/ghidra/issues/2094) - Upstream VMX128 request
- [0dinD's Fork](https://github.com/0dinD/ghidra/tree/vmx128) - Base implementation we extended
- Local research repos: `~/code/milohax/vmx128-research/`
- Local XDK intrinsics: `src/xdk/LIBCMT/vectorintrinsics.h`

## Status

| Phase | Status | Description |
|-------|--------|-------------|
| Phase 1: Audit | ✓ Complete | 37,020 VMX128 instructions found in DC3, 77 opcodes used |
| Phase 2: Fix Fork | ✓ Complete | Register size fix, immediate extractions added |
| Phase 3: Pcode | ✓ Complete | All 77 instructions have semantics (full or pcodeop stubs) |
| Phase 4: Test | ✓ Complete | **13,836 VMX128 instructions validated**, core semantics implemented; low-impact pack/unpack stubs remain |
| **Phase 5: Decomp** | **In Progress** | Apply VMX128 tooling to gesture/skeleton code |
| Phase 6: Upstream | Pending | Submit PR to Ghidra project |

### Validation Results (2026-01-25)

Headless comparison of stock Ghidra vs modified build on DC3:

| Metric | Stock Ghidra | Modified Ghidra |
|--------|--------------|-----------------|
| Total instructions | 1,427,266 | **2,821,231** |
| VMX128 recognized | 0 | **13,836** |
| Extended registers | 0 | **64** |

See [COMPARISON_REPORT.md](COMPARISON_REPORT.md) for full details.

**Build location:**
```
~/code/milohax/vmx128-research/ghidra-vmx128/build/dist/ghidra_12.0_DEV_20260125_linux_x86_64.zip
```

---

## Phase 5: VMX128 Decomp Progress

Using the new VMX128 tooling to decomp previously-blocked functions.

### Completed Functions

| Function | VMX128 Instrs | Before | After | Status |
|----------|---------------|--------|-------|--------|
| `NuiTransformSkeletonToDepthImage(float*)` | 1 | 0% | **99.4%** | ✅ AT_LIMIT (commutative swap) |
| `NuiTransformSkeletonToDepthImage(LONG*)` | 1 | 0% | **94.7%** | ✅ AT_LIMIT (64-bit load opt) |
| `NuiTransformMatrixLevel` | 0 | 0% | **100%** | ✅ Complete |
| `JointScreenPos(Vector2)` | 1 | 100% | **93.1%** | ✅ Calls NuiTransform |
| `JointScreenPos(Vector3)` | 1 | 37% | **95.5%** | ✅ Calls NuiTransform |
| `FreestyleMotionFilter::UpdateFilters` | 0 | 40.3% | **81%** | ✅ AT_LIMIT (fmadds) |

### In Progress

| Function | VMX128 Instrs | Before | Current | Status |
|----------|---------------|--------|---------|--------|
| `SkeletonFrame::Create` | 25 | 13% | **64%** | 🔧 Missing `vmaddcfp128` |
| `FastInterp` (Quat) | 0 | 68.6% | **99.6%** | 🔧 AT_LIMIT likely |

### Key Findings

1. **Extended registers work** - v59-v63 decode correctly in Ghidra
2. **Matrix-vector multiply visible** - `vmaddfp128`, `vmaddcfp128`, `vspltw128` patterns now recognizable
3. **Lane-wise semantics complete** - `vmaddcfp128` now has full 4-lane FP semantics in SLEIGH
4. **Intrinsic limitation understood** - Standard VMX intrinsics (`__vmaddfp`) generate standard VMX instructions (v0-v31), not VMX128 variants (v0-v127). The compiler chooses VMX128 instructions internally based on register pressure; there are no user-accessible VMX128-specific intrinsics
5. **stvx128 parameter passing works** - XMVECTOR parameters passed in v1, stored to stack via `stvx128`, then read as scalar floats

### Remaining Targets

**18 functions identified** in gesture/skeleton code - see [GESTURE_TARGETS.md](GESTURE_TARGETS.md)

### Tooling Gaps

See [PHASE4_TODO.md](PHASE4_TODO.md) for:
- Missing `__vmaddcfp128` intrinsic
- D3D pack/unpack semantics (`vpkd3d128`, `vupkd3d128`)
- Pack/unpack saturation semantics (`vpk*128`/`vupk*128`)
