# Fixable: Bodyless Copy Constructor Declarations

## Pattern

A copy constructor is **declared** in a header but never **defined** (no inline body, no out-of-line definition in .cpp). This suppresses the compiler's implicit copy constructor generation, causing template instantiations that need to copy the type to silently disappear.

## Symptoms

- Functions drop from 100% to **0%** match
- Affected functions are always **template instantiations**: `std::vector` constructors, `__uninitialized_copy`, `__uninitialized_fill_n`, copy constructors of containing types
- The regressed functions reference the type with the bodyless copy ctor (e.g., `vector<Key<TexPtr>>`, `ObjPtr<RndMesh>`)
- Multiple functions regress simultaneously in the same TU

## Example

```cpp
// BAD — suppresses implicit copy ctor
class Pose {
    Pose(Hmx::Object *owner) : mesh(owner) {}
    Pose(const Pose &);  // DECLARED but no body!
    ObjPtr<RndMesh> mesh;
    Keys<float, float> weights;
};

// Result: 4 functions drop to 0%:
//   _Vector_base<Key<float>>::_Vector_base  (104 B)
//   ObjPtr<RndMesh>::ObjPtr                 (92 B)
//   vector<Key<float>>::vector              (116 B)
//   Pose::Pose(const Pose&)                 (56 B)
```

```cpp
// GOOD — let compiler auto-generate
class Pose {
    Pose(Hmx::Object *owner) : mesh(owner) {}
    // No copy ctor declaration → compiler generates implicit one
    ObjPtr<RndMesh> mesh;
    Keys<float, float> weights;
};
```

## Why It Happens

When merging decomp work, copy constructors are sometimes added as declarations to match a symbol seen in the target binary, but the body is never filled in. The declaration alone prevents the compiler from auto-generating the copy constructor, which breaks:

1. `std::vector::resize()` — needs to copy-construct elements
2. `std::uninitialized_copy` — explicit copy construction
3. `push_back` with copy — needs copy ctor
4. Any containing struct's implicit copy ctor — cascades upward

## Detection

```bash
# Find copy ctor declarations in headers
grep -rn '^\s*\w\+(\s*const\s\+\w\+\s*&\s*);' src/system/*.h src/lazer/*.h
```

Then verify no body exists:
```bash
# Check if body exists in corresponding .cpp
grep -rn 'ClassName::ClassName(const' src/system/path/ClassName.cpp
```

## Fix

**Option A** (preferred): Remove the declaration entirely. Let the compiler generate the implicit copy ctor.

**Option B**: Add an explicit body, either inline or out-of-line.

## Verified Instances (2026-03-08 session)

| Header | Class | Functions Recovered | Bytes |
|--------|-------|-------------------:|------:|
| Morph.h | `Pose` | 4 | 368 B |
| Sfx.h | `SfxMap` | 4 | 344 B |
| HamCamTransform.h | `TransformArea` | 1 | 100 B |
| PracticeChoosePanel.h | `StepMoves` | 1 | 80 B |
| DancerSkeleton.h | `DancerSkeleton` | 1 | 96 B |
| DebugGraph.h | `DebugGraph` | 0 | 0 B |
| Cache_Xbox.h | `CacheIDXbox` | 0 | 0 B |
| **Total** | | **11** | **988 B** |

## Related Patterns

- [fixable-declarations.md](fixable-declarations.md) — variable declaration order
- [fixable-struct-layout.md](fixable-struct-layout.md) — struct member type changes
