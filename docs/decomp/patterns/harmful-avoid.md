# Harmful Patterns: Avoid These

These patterns make matches **worse**. Based on systematic testing (BurnXfm case study, 2026-01-27).

**Action:** Do NOT use these patterns. If you've already applied them, revert.

---

## Member Aliasing

**Effect:** -6%

Creating local references to member variables hurts register allocation.

### What NOT to Do

```cpp
// BAD - creates alias to member
Transform& xfm = mLocalXfm;
// use xfm instead of mLocalXfm
```

```cpp
// BAD - pointer alias
Transform* pXfm = &mLocalXfm;
// use pXfm instead of &mLocalXfm
```

### What To Do Instead

Access members directly:

```cpp
// GOOD - direct member access
mLocalXfm.SetRotation(...);
```

### Test Results

| Variation | Match | Delta |
|-----------|-------|-------|
| Original (no alias) | 99.47% | baseline |
| Alias mLocalXfm (reference) | 93.62% | -5.85% |
| Alias mLocalXfm (pointer) | 93.62% | -5.85% |
| Both aliases | 93.62% | -5.85% |

---

## Child Pointer in Loop

**Effect:** -6.5%

Adding an intermediate pointer for iterator dereferencing inside loops.

### What NOT to Do

```cpp
// BAD - extra child pointer
for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
    auto* child = *it;  // DON'T DO THIS
    child->DoSomething();
}
```

### What To Do Instead

Dereference directly:

```cpp
// GOOD - direct dereference
for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
    (*it)->DoSomething();
}
```

### Test Results

| Variation | Match | Delta |
|-----------|-------|-------|
| Original (direct deref) | 99.47% | baseline |
| child pointer inside loop | 92.87% | -6.60% |

---

## Iterator Address-Of (`&*iter`)

**Effect:** -3-5% on the function passing the address

Taking the address of a dereferenced iterator (`&*it`) instead of passing
the iterator directly to a function generates different codegen — even
when the iterator's underlying type is `T*` and the function signature
accepts `T*`.

### What NOT to Do

```cpp
// BAD — extra deref-then-addr-of round trip
std::vector<MyType> values;
auto it = values.begin();
auto next = it + 1;
InsertHeaderRange(&*it, &*next);
```

### What To Do Instead

Pass the iterators directly:

```cpp
// GOOD — direct iterator pass
std::vector<MyType> values;
auto it = values.begin();
auto next = it + 1;
InsertHeaderRange(it, next);
```

### Why It Hurts

`&*it` decays to a `T*` via `operator*` then `operator&`. With STLPort's
vector iterator (which is itself a `T*` typedef), this looks like a
no-op — but the compiler emits the deref/addr round-trip in IR before
any optimization, and the resulting reg-allocation choices diverge from
target's direct-pass version.

### Detection

Look for `&*` in source (especially in calls to range-taking helpers
like `Insert`, `Range`, `MakeString` of vectors). Compare against
upstream's call shape — if upstream passes the iterator directly and
your code uses `&*`, fix it.

### Real Examples

| Function | Before | After | Notes |
|----------|--------|-------|-------|
| `FitnessCalorieSort::BuildTree` | 99.5% | 100% | Replaced `InsertHeaderRange(&*pBegin, &*pNext)` with `InsertHeaderRange(begin, it)` |

### Permuter Coverage

This pattern is automated by the `iter_address_of` permuter pattern:

```bash
python -m scripts.permuter --patterns iter_address_of \
    --symbol "<mangled>" --source <path> --function "<Class::Method>"
```

Generates variants in both directions (drop `&*`, wrap with `&*`) and a "drop all" combined variant when 2+ sites are present.

### Related

- [Child Pointer in Loop](#child-pointer-in-loop) — Similar family: extra
  pointer/iterator local that hurts register allocation.
- [Iterator Dereference Caching](fixable-declarations.md#iterator-dereference-caching) — Sometimes the *opposite* helps; verify per call site.

---

## End Iterator Explicit

**Effect:** -0.5%

Storing the end iterator in an explicit variable.

### What NOT to Do

```cpp
// BAD - explicit end variable (declared first)
auto end = mChildren.end();
for (auto it = mChildren.begin(); it != end; ++it) {
    ...
}

// BAD - explicit end variable (declared after it)
auto it = mChildren.begin();
auto end = mChildren.end();
for (; it != end; ++it) {
    ...
}
```

### What To Do Instead

Call end() in the loop condition:

```cpp
// GOOD - end() called each iteration
for (auto it = mChildren.begin(); it != mChildren.end(); ++it) {
    ...
}
```

### Test Results

| Variation | Match | Delta |
|-----------|-------|-------|
| Original (end() in condition) | 99.47% | baseline |
| end iterator explicit (first) | 98.94% | -0.53% |
| end iterator explicit (after it) | 98.94% | -0.53% |

---

## Constructor Zero-Init That Doesn’t Exist in Target

**Effect:** -2% to -6% (size mismatch + extra float stores)

Adding explicit zero-initialization to members that the original constructor **did not** initialize injects extra constant loads and stores (often `lis/lfs/stfs` for `0.0f`).

### What NOT to Do

```cpp
// BAD - adds lfs/stfs for 0.0f if target doesn't initialize it
CharacterTest::CharacterTest(Character* theChar)
    : /* ... */, unk90(0.0f) {
}
```

### What To Do Instead

Only initialize if the target does:

```cpp
// GOOD - leave uninitialized if target doesn't store
CharacterTest::CharacterTest(Character* theChar)
    : /* ... */ {
}
```

### Notes

In `CharacterTest::CharacterTest`, adding `unk90(0)` introduced a `lis/lfs/stfs` sequence and dropped the match from 100% to ~94–97%. Removing the init restored size parity and brought it back to 99%+.

---

## Patterns With No Effect

These patterns were tested and had no impact (neither positive nor negative):

| Pattern | Effect |
|---------|--------|
| While loop instead of for | No effect |
| Separated increment (`++it` in body) | No effect |
| Iterator declared outside loop | No effect |
| xfm declared outside loop | No effect |
| Alias mChildren (container itself) | No effect |
| `self = this` shim | No effect |
| `mesh = this` at function start | No effect |

---

## Key Takeaways

1. **Don't alias Transform members** - Creating local references to `mLocalXfm` consistently causes ~6% drop

2. **Don't add child pointers in loops** - Adding `auto* child = *it;` inside loops causes ~6.5% drop

3. **Don't cache end()** - Explicit end iterator variables cause ~0.5% drop

4. **Loop structure changes are neutral** - While vs for, separated increment - no effect

5. **Self/this shims are neutral** - No effect on register allocation

---

## See Also

- [fixable-declarations.md](fixable-declarations.md) - Patterns that DO help
- [unfixable-compiler.md](unfixable-compiler.md#register-allocation) - Understanding register allocation limits
