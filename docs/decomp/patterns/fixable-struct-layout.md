# Struct Layout Mismatches

Struct layout issues cause **systematic offset mismatches** across all functions that access the wrong-sized members. Unlike stack-frame offset diffs (compiler artifacts), struct layout bugs affect every function touching the struct.

## How to Identify

### 1. Check offset comments against sizeof

For any struct with offset comments, verify the math:

```
Expected next offset = current offset + sizeof(member type)
```

Common type sizes (ILP32 / Xbox 360):
- `bool`: 1 byte (aligned to 1)
- `int`, `float`, pointer: 4 bytes
- `Vector3`: 12 bytes (3 floats)
- `Vector4` / `XMVECTOR`: 16 bytes (4 floats)
- `Transform`: 64 bytes (4x Vector3 + padding varies)
- `ObjPtr<T>`: 20 bytes (0x14)
- `String`: 8 bytes (0x8)
- `Symbol`: 4 bytes
- `std::vector<T>`: 12 bytes (0xC) — start, end, capacity

### 2. The gap test

If the gap between two consecutive offset comments is **larger** than the expected sizeof for the member between them, the member type has the wrong size or needs padding.

**Example — 16-byte stride Vector3 arrays:**
```cpp
Vector3 mPositions[20]; // 0x4
int mFlags[20];         // 0x144  ← GAP: 0x144 - 0x4 = 0x140 = 320
                        //   But 20 * sizeof(Vector3) = 20 * 12 = 240 = 0xF0
                        //   320 ≠ 240 → STRIDE MISMATCH (need 16-byte stride)
```

### 3. objdiff offset patterns

When struct layout is wrong, `run_diff_inspect mode=offsets` shows:
- **Consistent deltas on member access** (e.g., all `+4` or `+0x50` on the same base register)
- Deltas that are multiples of the per-element size difference
- Applies to `lwz`, `stw`, `lfs`, `stfs` with struct base registers (not r1/stack)

**Contrast with stack layout diffs:** Stack offsets are r1-relative and indicate local variable placement (unfixable compiler artifact). Struct offsets use other registers (r3, r4, etc.) and indicate incorrect type sizes.

### 4. Cross-reference with RB2 DWARF

Use `mcp__orchestrator__get_rb2_class_info` to check actual member offsets from RB2 debug info (shared Milo engine classes often have the same layout).

## Common Cases

### Padded Vector3 arrays (16-byte stride)

The most common struct layout issue. NUI/Kinect skeleton data uses 16-byte-aligned joint position arrays (matching `XMVECTOR` stride), but the decomp headers may use `Vector3` (12 bytes).

**Symptom:** Array of `Vector3[kNumJoints]` where offset comments show 16-byte-per-element stride (gap of `kNumJoints * 16`, not `kNumJoints * 12`).

**Fix:** Use a padded wrapper struct:
```cpp
struct PaddedJointPos {
    float x, y, z, _pad;
    PaddedJointPos() {}
    PaddedJointPos(const Vector3 &v) : x(v.x), y(v.y), z(v.z), _pad(0) {}
    operator Vector3 &() { return *(Vector3 *)&x; }
    operator const Vector3 &() const { return *(const Vector3 *)&x; }
    PaddedJointPos &operator=(const Vector3 &v) {
        x = v.x; y = v.y; z = v.z; return *this;
    }
};
```

The reference conversion operators (`operator Vector3&`) allow seamless use with existing APIs that take `Vector3&` parameters or return `const Vector3&`.

**Affected structs in DC3:**
| Struct | Header | Members fixed |
|--------|--------|---------------|
| SkeletonData | gesture/Skeleton.h | mRawPositions, mJointPositions |
| ArchiveSkeleton | gesture/ArchiveSkeleton.h | mJointPoses |
| DetectFrame | hamobj/DetectFrame.h | mBestNodeErrors, mNodeComponentWeights |
| RndVelocityBuffer | rndobj/VelocityBuffer.h | mFrustumCorners |
| RhythmDetector | hamobj/RhythmDetector.h | unkaac |
| DancerSkeleton | hamobj/DancerSkeleton.h | mCamJointPositions, mCamJointDisplacements |
| HamSkeletonConverter | hamobj/HamSkeletonConverter.h | mJointPositions |
| ErrorFrameInput | hamobj/ErrorNode.h | mJointDisps, mBaseJointDisps, mJointPositions, mBaseJointPositions |
| RecordedFrame | gesture/SkeletonClip.h | unk2c |

### Missing or extra padding between members

**Symptom:** Gap between two members is 1-3 bytes larger than expected, suggesting alignment padding the compiler inserts but the header doesn't account for.

**Fix:** Add explicit padding bytes or reorder members to match alignment.

### Inheritance offset shift

**Symptom:** All member offsets are shifted by a fixed amount from the base class size.

**Fix:** Check parent class size. Virtual inheritance adds vbptr (4 bytes on PPC). Multiple inheritance adds vtable pointers.

## Verification Checklist

After fixing a struct:
1. **Build succeeds** — fix type conversion errors with casts where needed
2. **Existing COMPLETE functions stay COMPLETE** — run `batch_check` on affected units
3. **Partial functions improve** — check if offset mismatches decrease
4. **Header changes need touch** — `touch src/path/file.cpp && ninja` since ninja doesn't track header deps

## Distinguishing Struct vs Stack Offset Diffs

| Characteristic | Struct layout bug | Stack layout (unfixable) |
|---------------|-------------------|--------------------------|
| Base register | r3, r4, etc. (param/member) | r1 (stack pointer) |
| Consistency | Same delta across ALL functions using the struct | Varies per function |
| Fix | Change struct member types/padding | Cannot fix (compiler artifact) |
| Diagnosis tool | `run_diff_inspect mode=offsets` | Same tool, but check register |

## Tooling

### Integrated validator (`struct_db.py validate`)

The primary tool. Built into the struct database, with RB2 DWARF cross-validation:

```bash
# Show only stride mismatches (most actionable)
python3 tools/struct_db.py validate --stride-only

# Show only confirmed 16-byte stride issues
python3 tools/struct_db.py validate -t stride_16

# Store results in layout_issues table for querying
python3 tools/struct_db.py validate --stride-only --store

# All issues (noisy — many false positives from hidden members)
python3 tools/struct_db.py validate
```

When `--store` is used, issues go into `struct_db.sqlite`'s `layout_issues` table:
```sql
SELECT c.name, li.member_name, li.issue_type, li.details
FROM layout_issues li
JOIN classes c ON c.id = li.class_id
WHERE li.issue_type = 'stride_16';
```

### Legacy scanner (`tools/find_struct_gaps.py`)

Standalone script, more verbose output but not integrated with the DB.

### MCP tools

```bash
# Check a class layout from RB2 DWARF
mcp__orchestrator__get_rb2_class_info class_name="ClassName"

# Look up what field is at a specific offset
mcp__orchestrator__lookup_struct_offset class_name="ClassName" offset="0x48"

# Check offset mismatches in a function
mcp__orchestrator__run_diff_inspect symbol="..." mode=offsets project_dir="."
```

## Common Pitfalls

### Name collisions
`PaddedJointPos` was originally named `JointPos`, which collided with `BaseSkeleton::JointPos()` (a virtual method). MSVC resolves the method name over the struct type in derived class scope. Always check for method name collisions when naming structs.

### `operator[]` not inherited through conversion
MSVC doesn't apply implicit `operator Vector3&()` for `operator[]` calls. If code uses `padded[i][j]`, you need an explicit `operator[]` on PaddedJointPos.

### Pointer conversion needs explicit cast
`operator Vector3&()` handles reference conversion but NOT pointer conversion. When passing `PaddedJointPos*` to `Vector3*`, use `(Vector3 *)array` or `(const Vector3 *)array`.
