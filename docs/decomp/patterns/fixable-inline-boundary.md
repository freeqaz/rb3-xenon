# Fixable Patterns: Inline Boundary

Patterns where moving a function definition between an inline location (header)
and an out-of-line location (.cpp) changes downstream codegen — sometimes by
eliminating a `bl` call (when MSVC inlines), sometimes by *adding* one (when
the function is no longer visible to inlining), and sometimes by triggering
an ICF merge that influences register allocation in unrelated callers.

These patterns came out of the 2026-05-12 upstream merge gap-recovery pass,
where many functions matched at 100% in the upstream tree but not in ours
despite identical-looking source. The differences turned out to be the
inline boundary — not the function body itself.

---

## Inline Constructor in Header vs Out-of-Line in .cpp

**Impact:** +5-10% (typically a sort-class ctor at ~91% → 100%)
**Success Rate:** 100% when applied to ctors with no body
**Time:** 2 minutes

### Symptom

A bodyless or near-bodyless constructor (often a sort/comparator class deriving
from a base) is at 91-95% match. The mismatch is in the static-init guard
prologue — the guard variable mangling shows `?$S2` where target uses `??_B`
(or vice versa), or the guard bit number differs by 1.

This is the same symptom family as
[Function Definition Order ($S#)](fixable-declarations.md#function-definition-order-tu-wide-static-guard-counters)
but the trigger is different: it's where the ctor lives, not where its
declaration appears in the file.

### Why It Works

When a constructor is defined out-of-line in a `.cpp`, MSVC emits its own
TU-static initialization guard at the call site. When defined inline in the
header, the call site instead inlines the body directly — no guard is
generated for the construction itself, and the surrounding guard counter
slot is freed up for the next runtime-initialized static.

For ctors that are conceptually "always inline" (like sort comparator
classes whose only state is a base-class member init), upstream typically
defines them inline. Out-of-lining them in our tree shifts the guard slot
allocation across the entire translation unit.

### Fix

```cpp
// BEFORE — out-of-line in .cpp
// SongSortByDiff.h
class SongSortByDiff : public SongSort {
public:
    SongSortByDiff();
    ~SongSortByDiff();
};

// SongSortByDiff.cpp
SongSortByDiff::SongSortByDiff() : SongSort(by_diff) {}
SongSortByDiff::~SongSortByDiff() {}
```

```cpp
// AFTER — inline in header
class SongSortByDiff : public SongSort {
public:
    SongSortByDiff() : SongSort(by_diff) {}
    ~SongSortByDiff() {}
};
// (remove from .cpp)
```

### Real Examples

| Function | Before | After | Notes |
|----------|--------|-------|-------|
| `SongSortByDiff::SongSortByDiff` | 91.0% | 100% | Inlined ctor + dtor in `SongSortByDiff.h`, removed `.cpp` defs |
| `SongSortByLocation::SongSortByLocation` | 91.0% | 100% | Same pattern |
| `PlaylistSortByType::PlaylistSortByType` | 91.0% | 100% | Same pattern; also added `utl/Symbol.h` include to the header |

### Detection

Open both versions side-by-side and look for `inline ctor()` in upstream's
header where ours has the body in `.cpp`. The diff scope is small but the
guard-counter ripple can move multiple unrelated static-init sequences.

### When It Hurts

If the ctor body is non-trivial (>3 source lines, calls non-inlined functions),
moving it inline can *worsen* the match because the call sites change shape.
This pattern is specifically for bodyless ctors and trivial init-list-only
ctors that are obvious "always inline" candidates.

---

## Sort Comparator Inline Location (std::sort / std::__median)

**Impact:** +30-50% on the std::sort template instantiation
**Success Rate:** 100% when applied to a sort comparator with one-line `operator()`
**Time:** 5 minutes

### Symptom

A std::__median template instantiation (e.g.
`__median<StoreOffer*, SortCmp>`) matches very poorly (50-60%). The diff
shows a `bl` to `SortCmp::operator()` where target uses an inline call
sequence (`lwz / mtctr / bctrl` or just inlined comparison).

### Why It Works

`std::sort` and its helper templates (`__median`, `__partition`,
`__insertion_sort`) call the comparator via `operator()`. When the
comparator is defined inline in the header, MSVC sees the body during
template instantiation and inlines the comparison directly. When the
comparator's `operator()` is defined out-of-line in the .cpp, the
template can only emit a `bl` to the out-of-line definition.

The target binary was compiled with the comparator inline. To match it,
move the `operator()` body into the header.

### Fix

```cpp
// BEFORE — out-of-line in StoreOffer.cpp
// StoreOffer.h
class SortCmp {
    Symbol mSort;
public:
    SortCmp(Symbol s) : mSort(s) {}
    bool operator()(const StoreOffer *a, const StoreOffer *b) const;
};

// StoreOffer.cpp
bool SortCmp::operator()(const StoreOffer *a, const StoreOffer *b) const {
    return a->Compare(b, mSort) < 0;
}
```

```cpp
// AFTER — inline in header
class SortCmp {
    Symbol mSort;
public:
    SortCmp(Symbol s) : mSort(s) {}
    bool operator()(const StoreOffer *a, const StoreOffer *b) const {
        return a->Compare(b, mSort) < 0;
    }
};
// (remove from .cpp; check no other callers depend on out-of-line def)
```

### Real Examples

| Function | Before | After | Notes |
|----------|--------|-------|-------|
| `__median<StoreOffer*, SortCmp>` | 53.1% | 100% | `SortCmp::operator()` moved from `StoreOffer.cpp` to `StoreOffer.h` |

### Adjacent Fixes

When you make this change, also check the call site that constructs
`SortCmp` and passes it to `std::sort`. Upstream typically uses an
inline temporary (`std::sort(beg, end, SortCmp(s))`) rather than a named
local. Match that style.

### Caveats

- Verify no other translation unit references `SortCmp::operator()` as an
  external symbol. Inlining it removes the out-of-line emission.
- If the comparator captures non-trivial state (e.g., a heavyweight
  member like `std::vector`), inlining is a behavioral change worth a
  performance check, even if match-correct.

---

## Inline Boundary Cascade (ICF Merge of Out-of-Line Accessor)

**Impact:** Variable. Typically fixes a downstream caller's register-allocation
swap (5-50 instructions) by triggering an ICF merge.
**Success Rate:** Pattern is proven; finding it requires diagnose work
**Time:** 30 minutes (because the symptom is in a *different* function)

### Symptom

A function `Caller::Foo` has a residual register-allocation swap (e.g.
r28↔r29 across many instructions) that won't budge despite source-level
fixes. The function being called inside `Caller::Foo` is a trivial
inline accessor like `GetMotdFreq()` defined in a header.

### Why It Works

Out-of-line trivial accessors with identical bodies get folded by the
linker via ICF (Identical COMDAT Folding). When a target binary's
`GetMotdFreq()` is ICF-merged with another function (e.g.
`CVoiceSkin::GetGraphNode`), the call site uses a **real `bl`** to the
merged address. Our inline-in-header version emits the body directly
at the call site (`lbz r10, 0x6c(r11)`).

The presence or absence of the `bl` changes the function's register
pressure profile at the call site — which can cause MSVC to color the
surrounding callee-saved registers differently, fixing what looks like
unrelated register-allocation noise downstream.

### Fix (When Diagnosed)

Move the inline accessor from the header into a `.cpp` file:

```cpp
// BEFORE — RockCentral.h
class RockCentral {
public:
    int GetMotdFreq() const { return mMotdFreq; }
    // ...
};

// AFTER — RockCentral.h
class RockCentral {
public:
    int GetMotdFreq() const;  // declaration only
    // ...
};

// RockCentral.cpp
int RockCentral::GetMotdFreq() const { return mMotdFreq; }
```

After rebuild, the linker may merge `GetMotdFreq` with another
identically-bodied function. The call site in `Caller::Foo` now uses
`bl <merged_address>` instead of an inlined load — and the residual
register swap dissolves.

### Real Examples

| Function | Before | After | Cause |
|----------|--------|-------|-------|
| `MainMenuPanel::MotdInitializeTexts` | 92.6% | 100% | Moved `RockCentral::GetMotdFreq` from inline → out-of-line. Resulting ICF merge with `GetGraphNode` made the call site emit `bl`, dissolving 47 r28↔r29 swap mismatches and 7 instruction reorderings |

### Detection

This is hard to spot from the diff of the affected function alone. Signs:

1. The mismatched function calls a trivial accessor.
2. Upstream's matching version of the *same source* still emits a `bl` to
   that accessor (check by reading upstream's compiled .obj).
3. The residual mismatches are dominated by register swaps, not real
   logic differences.
4. `merged-symbols` lookup near the suspected accessor's address shows
   another function with the same body would merge with it.

### When It Doesn't Help

If the accessor's body is unique (no other function in the binary has
the same instructions), moving it out-of-line just adds a `bl` without
triggering ICF. In that case, the cascade fix won't apply — the source
of the register swap is elsewhere.

---

## See Also

- [verifiable-icf.md](verifiable-icf.md) — Linker-merged ICF as a verifiable
  pattern (when accepting at_limit)
- [fixable-declarations.md: Function Definition Order ($S#)](fixable-declarations.md#function-definition-order-tu-wide-static-guard-counters) — Related
  static-init guard slot pattern, triggered by definition order rather than
  inline location
- [fixable-declarations.md: Static Variable Naming](fixable-declarations.md#static-variable-naming) — Related guard mangling pattern
- [unfixable-compiler.md: Static Guard Naming `??_B` vs `$S`](unfixable-compiler.md#static-guard-naming-convention-_b-vs-s) — When the guard
  naming convention itself diverges (not fixable by inlining)
