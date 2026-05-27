# Fixable Patterns: Loop Condition Subtraction

Patterns related to loop condition comparison instructions.

---

## Subtract-and-Record (`subf.`) Loop Condition

**Impact:** +2-3%
**Success Rate:** High (when symptom matches)
**Time:** 1 minute

When a `while` or `for` loop condition compares two variables with `>=`,
the compiler can emit either `cmpw` (direct compare) or `subf.`
(subtract-and-record to CR0). The target binary may use `subf.` while
our compiler emits `cmpw`.

### Symptom

objdiff shows a 2-instruction `replace` cluster in a loop back-edge:

```
[18] replace: subf.    r10, r8, r11   vs   cmpw     cr6, r11, r8
[19] replace: bge      0x20           vs   bge      cr6, 0x20
```

Key indicators:
- `subf.` (with record bit) vs `cmpw` — same comparison, different encoding
- The `bge` uses implicit CR0 (target) vs explicit CR6 (base)
- The `subf.` result register is dead (overwritten before next use)
- Always appears at the loop back-edge, not the initial entry

### Why It Works

`while (high >= low)` emits `cmpw cr6, high, low` — a dedicated compare.

`while (high - low >= 0)` emits `subf. rN, low, high` — the subtraction
sets CR0 as a side effect. The compiler uses the CR0 flags directly for
the branch, eliminating the separate compare instruction.

Both are semantically identical for signed integers: `high >= low` is
equivalent to `high - low >= 0`.

### Fix

```cpp
// Before — generates cmpw (direct compare)
while (high >= low) {
    int mid = (low + high) >> 1;
    // ...binary search body...
}

// After — generates subf. (subtract-and-record)
while (high - low >= 0) {
    int mid = (low + high) >> 1;
    // ...binary search body...
}
```

Also applies to `for` loops:

```cpp
// Before — generates cmpw
for (int i = 0; i + 1 < cap;) { ... }

// After — generates subf. (if applicable)
for (int i = 0; cap - i - 1 >= 0;) { ... }
```

### Real Examples

| Function | Before | After | Delta |
|----------|--------|-------|-------|
| Locale::FindDataIndex | 97.2% | 100.0% | +2.8% |

### Detection

Look for these in objdiff output:
1. A `replace` pair where target has `subf.` and base has `cmpw`/`cmplw`
2. The `bge`/`ble`/`bgt`/`blt` after uses implicit CR0 (no `crN` operand)
   vs explicit CR6/CR7

### Limitations

- Only works for signed integer comparisons (unsigned uses `cmplw`)
- The subtraction form must be semantically equivalent (no overflow risk)
- Only fixes cases where the compiler uses `subf.` — not all `>=` loops
  benefit from this transformation
