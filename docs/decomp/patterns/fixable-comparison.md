# Fixable Patterns: Comparison

Patterns related to comparison operators, signedness, and conditional expressions.

---

## Unsigned Zero Comparison

**Impact:** +0.4-1.3%
**Success Rate:** 95%
**Time:** 2 minutes

For unsigned variables compared against zero, use `> 0` instead of `!= 0`.

### Symptom

objdiff shows `beq`/`bne` vs `ble`/`bgt` branch mismatch when comparing unsigned to 0.

### Why It Works

MSVC generates different branch instructions:
- `!= 0` → `cmpwi` + `beq` (branch if equal)
- `> 0` → `cmpwi` + `ble` (branch if less or equal)

The original binary uses `ble`. For unsigned types, `x > 0` is mathematically equivalent to `x != 0`.

### Fix

```cpp
// Before - generates beq branch
if (d.rev != 0)
    bs >> mBlinkClipLeftName;

// After - generates ble branch (matches original)
if (d.rev > 0)
    bs >> mBlinkClipLeftName;
```

### Real Examples

| Function | Before | After | Delta |
|----------|--------|-------|-------|
| CharFaceServo::Load | 98.8% | 99.5% | +0.7% |
| UIListArrow::Load | 94.2% | 95.5% | +1.3% |
| RndTransformable::Load | 92.1% | 92.5% | +0.4% |

### Important

This pattern ONLY applies to:
- Zero comparisons (`!= 0` or `== 0`)
- Unsigned types

Other transformations like `> X` to `>= X+1` make matches WORSE.

---

## Signed/Unsigned Cast

**Impact:** +1-50%
**Success Rate:** 100%
**Time:** 5 minutes

Force specific comparison instruction by casting to signed or unsigned.

### Symptom

objdiff shows `cmplwi` (unsigned) vs `cmpwi` (signed) mismatch.

### Why It Works

Pointer/integer null checks can generate either instruction type. The original code may have used explicit casts.

### Fix

```cpp
// Force signed comparison (cmpwi)
if ((int)ip != 0) { ... }
if (!obj) { ... }  // null check uses signed

// Force unsigned comparison (cmplwi)
if ((unsigned long)ptr != 0) { ... }
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| OSCMessenger::Connect | 98.57% | 100% | +1.43% | `(int)ip != 0` forces cmpwi |
| ObjectDir::RemoveSubDir | 98.13% | 100% | +1.87% | Direct mObject pointer access for cmplw |
| PhysicsVolume::DestroyPhysicsVolume | 98.0% | 100% | +2.0% | `(unsigned)(void*)` cast in RELEASE macro |
| HamNavList::SetHighButtonMode | 17.0% | 100% | +83% | Virtual call + cast fix |
| FlowPtrGetLoadingDir | 97.78% | 100% | +2.22% | `(unsigned long)flow->Loader() > 0` |
| HollaBackMinigame::WinShoutOut | 98.21% | 100% | +1.79% | `(int)mWinShoutouts.size()` for clrrwi. |

---

## Loop Counter Signedness

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 3 minutes

Loop counter type affects comparison instruction selection.

### Symptom

objdiff shows `cmpwi` (signed) vs `cmplwi` (unsigned) in loop termination.

### Fix

```cpp
// Before - signed comparison, generates cmpwi
int size;
bs >> size;
for (; size != 0; size--) { ... }

// After - unsigned comparison, generates cmplwi
unsigned int size;
bs >> size;
for (; size != 0; size--) { ... }
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| SfxInst::Stop | 98.57% | 100% | +1.43% | Extracted `MoggClip*` to local var changes cmpwi/cmplwi |
| (loop counter) | 97.1% | 100% | +2.9% | `(int)size()` in loop condition |

### Detection

Look for `cmpwi` vs `cmplwi` differences in loop comparison code.

---

## String Iteration Signedness

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 3 minutes

Use `unsigned char` for string iteration to avoid sign extension.

### Symptom

objdiff shows extra `extsb` (extend sign byte) instruction in string loops.

### Why It Works

- `char*` iteration: generates `extsb` for sign extension
- `unsigned char*` iteration: no sign extension needed

### Fix

```cpp
// Before - signed char generates extsb
for (const char *p = str; *p != '\0'; p++) {
    hash = hash * mult + *p;
}

// After - unsigned char, no sign extension
for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
    hash = hash * mult + *p;
}
```

### Detection

Look for `extsb` instructions in string processing code. If the original doesn't have them, switch to unsigned char.

---

## empty() vs size()

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 2 minutes

`empty()` and `size() == 0` generate different code.

### Symptom

objdiff shows `cmplw` (pointer compare) vs `divw` (division) mismatch.

### Why It Works

- `empty()`: Compares begin/end pointers (`cmplw begin, end`)
- `size() == 0`: Calculates element count via division (`divw (end-begin)/sizeof(T)`)

### Fix

```cpp
// Before - pointer comparison
if (mTempoPoints.empty()) {

// After - division-based size check
if (mTempoPoints.size() == 0) {
```

Also applies to Eof checks:

```cpp
// Before - generates wrong subtraction order
virtual EofType Eof() { return (EofType)(mBuffer.size() == mTell); }

// After - explicit (end - begin) - Tell matches PowerPC register order
virtual EofType Eof() { return (EofType)(mBuffer.end() - mBuffer.begin() - mTell == 0); }
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| MemStream::Eof | 98.75% | 100% | +1.25% | `end() - begin() - mTell == 0` |
| BufStream::Eof | 98.0% | 100% | +2.0% | `mTell == mSize` operand order |
| ArkFile::Eof | 98.0% | 100% | +2.0% | Moved inline to .cpp, `mTell == mSize` |

### Detection

Look for `divw` in target vs `cmplw` in decomp. If target calculates actual count, use `size() == 0`.

---

## Comparison Style

**Impact:** Variable
**Success Rate:** MEDIUM
**Time:** 5 minutes

Equivalent comparisons may generate different code.

### Symptom

objdiff shows `cmpwi` immediate value off by 1, with different branch condition.

### Fix

Try equivalent comparison forms:

```cpp
// These are logically equivalent but may generate different code:
if (x >= 5) ...
if (x > 4) ...
if (!(x < 5)) ...

// Example fix:
// Before - generates cmpwi r27, 0x2 + bge
if (i3 < 2) {

// After - generates cmpwi r27, 0x1 + bgt
if (i3 <= 1) {
```

### Detection

Check objdiff for `cmpwi` immediate values and branch conditions (`bge` vs `bgt`).

---

## Argument Evaluation Order

**Impact:** Variable
**Success Rate:** MEDIUM
**Time:** 5 minutes

Function argument evaluation order is unspecified in C++.

### Symptom

objdiff shows load instructions in wrong order near function calls.

### Why It Works

C++ does not guarantee argument evaluation order. The compiler may evaluate right-to-left, left-to-right, or any other order.

### Fix

```cpp
// Before - mStr loaded second
return strcmp(mStr, str.c_str()) == 0;

// After - mStr loaded first (matches target)
return strcmp(str.c_str(), mStr) == 0;
```

### Detection

Use objdiff to compare load instruction order before `bl` calls. If target loads `this->member` before `param->member`, swap the arguments.

---

## IsNaN vs Threshold Check

**Impact:** +3-5%
**Success Rate:** HIGH
**Time:** 5 minutes

`IsNaN(x)` and `x < threshold` generate completely different code.

### Symptom

objdiff shows:
- `fcmpu cr6, fN, fN` (compare register with itself) for IsNaN
- `fcmpu cr6, fN, fM` (compare with loaded constant) for threshold
- Different branch conditions (`bne` for IsNaN vs `bge`/`blt` for threshold)

### Detection

Look at the float comparison in objdiff:

```asm
# IsNaN pattern - compares register with itself
fcmpu cr6, f30, f30
bne cr6, ...         ; NaN if f30 != f30

# Threshold pattern - compares with constant
lfs f0, __real@b8d1b717, r11   ; load -0.0001
fcmpu cr6, f30, f0
bge cr6, ...         ; branch if f30 >= -0.0001
```

### Why It Matters

`IsNaN(x)` generates a self-comparison (`x != x` is true only for NaN), while a threshold check loads a constant and compares against it. These are semantically different and generate different code.

### Fix

Check Ghidra decompilation to see which pattern the target uses:

```cpp
// Before - IsNaN check (self-comparison)
if (IsNaN(score)) {
    MILO_NOTIFY("error: bad score %f", score);
}

// After - threshold check (comparison with constant)
if (score < -0.0001f) {
    MILO_NOTIFY("error: bad score %f", score);
}
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| CharInterest::ComputeScore | 91.6% | 94.6% | +3.0% | Changed `IsNaN(f7)` to `f7 < -0.0001f` |

### Note

The threshold value can be determined from the float constant in objdiff. Common values:
- `__real@b8d1b717` = -0.0001f (approximately)
- Check the IEEE 754 hex representation to decode the exact value

---

## rlwimi Peephole Avoidance (+ instead of |)

**Impact:** +2-3%
**Success Rate:** MEDIUM (mitigates but doesn't fully fix)
**Time:** 5 minutes

The compiler recognizes bit-field merge patterns like `(x >> 16) | (y & 0x7FFF0000)` and emits `rlwimi` (rotate left word immediate then mask insert, 1 instruction) instead of the target's `rlwinm` + `or` (2 instructions). Using `+` instead of `|` prevents the pattern recognition.

### Symptom

objdiff shows `rlwimi` in our build vs `rlwinm` + `or` in target. The `rlwimi` is a single instruction that combines a rotate/mask with an insert into an existing register — a peephole optimization for non-overlapping bit-field merges.

### Why It Works (Partially)

The compiler's peephole optimizer recognizes `|` between non-overlapping bit ranges as a rotate-and-insert. Using `+` (addition) is mathematically equivalent when bit ranges don't overlap, but the compiler doesn't recognize `+` as a merge pattern and emits separate `rlwinm` + `add` instead.

**Trade-off:** `+` generates `add` instead of `or`. This is a Catch-22 — `|` triggers rlwimi, `+` avoids it but uses the wrong combine instruction. The `+` form is typically better overall because it eliminates the structural mismatch (1 vs 2 instructions), leaving only an opcode difference (`add` vs `or`).

### Fix

```cpp
// Before (82.2% match) — compiler emits rlwimi (1 instr) instead of rlwinm+or (2 instr)
mRandTable[i] = ((j >> 16) & 0xFFFF) | (s & 0x7FFF0000);

// After (84.8% match) — + avoids rlwimi; pair with unsigned cast for shift direction
mRandTable[i] = (((unsigned int)j >> 16) & 0xFFFF) + (s & 0x7FFF0000);
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| Rand::Seed | 82.2% | 84.8% | +2.6% | Also fixed `srawi` → `srwi` with `(unsigned int)` cast. Remaining gap: `add` vs `or` + register swaps |

### When to Use

- objdiff shows `rlwimi` in our build vs `rlwinm` + `or` in target
- Bit-field merge with non-overlapping ranges (low bits OR'd with high bits)
- Always pair with signedness fix if the shift direction also mismatches (`srawi` vs `srwi`)

### Detection

Look for `rlwimi` in a `replace` or structural mismatch where the target has two separate instructions (`rlwinm` for the mask + `or` for the combine).

---

## Iterator Index Comparison

**Impact:** +40-100% (0→100 on entire TU)
**Success Rate:** HIGH (when pattern applies)
**Time:** 5 minutes

Compare iterators via index subtraction instead of direct pointer comparison.

### Symptom

objdiff shows `subf` + `clrrwi` (index computation + alignment mask) in target vs simpler `subfc` + `subfe` (direct pointer subtraction) in our build. Often accompanied by delete clusters containing `eqv`, `srwi`, `addze` — the target's signed index comparison sequence.

### Why It Works

MSVC PPC generates different code for these two semantically-equivalent iterator comparisons:

```
// Direct pointer compare → subfc + subfe (2 instructions)
return it1 < it2;

// Index compare → subf + clrrwi + subfc + eqv + srwi + addze (6 instructions)
return (it1 - vec.begin()) < (it2 - vec.begin());
```

The index form computes `(ptr - base)` for each iterator, masks to element alignment (`clrrwi` clears low 2 bits for 4-byte elements), then does a signed comparison on the indices. The target binary used the index form.

### Fix

```cpp
// Before (60.5% match) — direct iterator comparison
template <>
bool VectorSort<RndMesh *>::operator()(RndMesh *item1, RndMesh *item2) {
    std::vector<RndMesh *>::const_iterator it1 =
        std::find(vector.begin(), vector.end(), item1);
    std::vector<RndMesh *>::const_iterator it2 =
        std::find(vector.begin(), vector.end(), item2);
    return it1 < it2;
}

// After (100% match) — index-based comparison
template <>
bool VectorSort<RndMesh *>::operator()(RndMesh *item1, RndMesh *item2) {
    std::vector<RndMesh *>::const_iterator it1 =
        std::find(vector.begin(), vector.end(), item1);
    std::vector<RndMesh *>::const_iterator it2 =
        std::find(vector.begin(), vector.end(), item2);
    return (it1 - vector.begin()) < (it2 - vector.begin());
}
```

### Cascade Effect

When a comparator body is visible to the compiler, MSVC PPC may **inline** it into all STL sort template instantiations (`__unguarded_partition`, `__linear_insert`, `__push_heap`, etc.). If the inlined body doesn't match, ALL sort templates in the TU are affected.

In the VectorSort case, fixing the comparison from direct pointer to index-based recovered **14 sort template functions to 100%** — the correct body inlines correctly.

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| VectorSort::operator() | 60.5% | 100% | +39.5% | Direct fix |
| __unguarded_partition | 3.9% | 100% | +96.1% | Cascade from inlined body |
| __linear_insert | 46.3% | 100% | +53.7% | Cascade |
| __adjust_heap | 66.1% | 100% | +33.9% | Cascade |
| __push_heap | 43.2% | 100% | +56.8% | Cascade |
| __partial_sort | 64.0% | 100% | +36.0% | Cascade |
| __median | 0% | 100% | +100% | Cascade |
| __unguarded_linear_insert | 0% | 100% | +100% | Cascade |
| + 6 more sort templates | — | 100% | — | Cascade |

### Detection

1. Look for `clrrwi` in target's delete clusters — indicates alignment masking on pointer differences
2. Look for `eqv` + `srwi` + `addze` sequence — signed index comparison
3. The compared variables are iterators from `std::find`, `begin()`, or similar
4. Permuter pattern: `iterator_index_compare`

### When to Use

- Comparator functors that compare iterator positions (e.g., `VectorSort`, custom sort predicates)
- Functions where two iterators from the same container are compared with `<`, `>`, `<=`, `>=`
- The container's `begin()` is accessible in the comparison scope

---

## See Also

- [fixable-casting.md](fixable-casting.md) - Type casting patterns
- [unfixable-compiler.md](unfixable-compiler.md) - When comparison fixes don't work
