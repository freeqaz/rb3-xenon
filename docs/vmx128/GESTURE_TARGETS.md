# Phase 5: Gesture/Skeleton Decomp Targets

VMX128-heavy functions in `system/gesture/` that are now approachable with the new Ghidra tooling.

## Progress Tracking

### Completed

| Function | File | Before | After | VMX128 | Notes |
|----------|------|--------|-------|--------|-------|
| `NuiTransformSkeletonToDepthImage(float*)` | JointUtl.cpp | 0% | **99.4%** | 1 | AT_LIMIT (commutative register swap) |
| `NuiTransformSkeletonToDepthImage(LONG*)` | JointUtl.cpp | 0% | **94.7%** | 1 | AT_LIMIT (64-bit load optimization) |
| `NuiTransformMatrixLevel` | xdk/nuiskeleton.cpp | 0% | **100%** | 0 | XDK function, complete |
| `JointScreenPos(Vector2)` | JointUtl.cpp | 100% | **93.1%** | 1 | Calls NuiTransformSkeletonToDepthImage |
| `JointScreenPos(Vector3)` | JointUtl.cpp | 37% | **95.5%** | 1 | Calls NuiTransformSkeletonToDepthImage |
| `FreestyleMotionFilter::UpdateFilters` | FreestyleMotionFilter.cpp | 40.3% | **81%** | 0 | AT_LIMIT (fmadds, merged calls) |

### In Progress

| Function | File | Before | Current | VMX128 | Notes |
|----------|------|--------|---------|--------|-------|
| `SkeletonFrame::Create` | Skeleton.cpp | 13% | **64%** | 25 | Missing `vmaddcfp128` intrinsic |
| `FastInterp` (Quat) | system/math/Rot.cpp | 68.6% | **99.6%** | 0 | Register swap in dot product, likely AT_LIMIT |

### Not Started

See tiers below for prioritized targets.

---

## Tier 2: Next Batch

Unimplemented, likely VMX128 usage.

| Function | File | Size | Notes |
|----------|------|------|-------|
| `SkeletonViz::SetCamera` | SkeletonViz.cpp | 1008b | Camera transform setup from skeleton frame |
| `SkeletonViz::DrawJoints` | SkeletonViz.cpp | 1468b | 3D joint rendering with transforms |
| `Skeleton::Poll` | Skeleton.cpp | 828b | Main skeleton update, uses SkeletonFrame data |
| `HandInvokeGestureFilter::CalcInPose` | HandInvokeGestureFilter.h | 1304b | Hand pose detection with joint positions |
| `BaseSkeleton::MakeCameraToPlayerXfm` | BaseSkeleton.cpp | 868b | Transform computation |
| `BaseSkeleton::LimbNormPos` | BaseSkeleton.cpp | 696b | Limb position normalization |
| `StandingStillGestureFilter::Update` | StandingStillGestureFilter.cpp | 1104b | Standing detection using joint positions |

---

## Tier 3: Complex but Approachable

Larger functions that were previously untouchable.

| Function | File | Size | Notes |
|----------|------|------|-------|
| `GestureMgr::DrawSkeletonKinectData` | GestureMgr.cpp | 1288b | Debug visualization |
| `GestureMgr::PostUpdate` | GestureMgr.cpp | 804b | Post-frame update processing |
| `StreamRenderer::DrawToTexture` | StreamRenderer.cpp | 3400b | Texture rendering with depth data |
| `DepthBuffer3D::DrawShowing` | DepthBuffer3D.cpp | 5188b | Large depth buffer rendering |
| `ArcDetector::UpdateOverlay` | ArcDetector.cpp | 2160b | Gesture overlay update |
| `SkeletonUpdate::InsertFakeArmPos` | SkeletonUpdate.cpp | 604b | Arm position estimation |

---

## VMX128 Instructions in Gesture Code

Common patterns found in these functions:

### Matrix-Vector Multiply
```asm
vspltw128 v12, v62, 0x0   # Splat X component
vspltw128 v13, v62, 0x1   # Splat Y component
vspltw128 v0, v62, 0x2    # Splat Z component
vmaddcfp128 v0, v61, v63  # row2*z + row3 (carryout)
vmaddfp128 v0, v13, v60   # + row1*y
vmaddfp128 v0, v12, v59   # + row0*x
```

### Vector Load/Store
```asm
lvx128 v1, r0, r11        # Load 16-byte aligned vector
stvx128 v0, r5, r7        # Store 16-byte aligned vector
```

### Available Intrinsics

From `src/xdk/LIBCMT/vectorintrinsics.h`:

```cpp
// Working
XMVECTOR __vmaddfp(XMVECTOR mul1, XMVECTOR mul2, XMVECTOR addend);
XMVECTOR __vspltw(XMVECTOR vSrcA, unsigned int uImmed);
static inline XMVECTOR __lvx(const void *base, int offset);
static inline void __stvx(XMVECTOR vSrc, void *base, int offset);

// Missing (generates function call)
XMVECTOR __vmaddcfp128(XMVECTOR mul1, XMVECTOR mul2, XMVECTOR addend);
```

---

## Workflow for New Functions

1. **Get analysis**:
   ```bash
   ./bin/analyze-function "FunctionName"
   ```

2. **Check current match**:
   ```bash
   ./bin/objdiff-cli diff -p . "FunctionName" --verdict -f markdown
   ```

3. **Implement using VMX128 intrinsics** where needed

4. **Build and iterate**:
   ```bash
   ninja build/45410914/src/system/gesture/File.obj
   ./bin/objdiff-cli diff -p . "FunctionName" --build --verdict
   ```

5. **Update this doc** with progress

---

## Statistics

- **Total targets identified**: 18
- **Completed/AT_LIMIT**: 7 (NuiTransformSkeletonToDepthImage x2, NuiTransformMatrixLevel, JointScreenPos x2, FreestyleMotionFilter::UpdateFilters)
- **In progress**: 2 (SkeletonFrame::Create, FastInterp)
- **Not started**: 9
