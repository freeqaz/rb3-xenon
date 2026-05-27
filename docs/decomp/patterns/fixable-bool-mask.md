# Fixable Patterns: Bool Mask

**Prevalence:** 33 functions tagged (database)
**Often fixable** — do not skip this pattern without trying the steps below.

The compiler inserts `clrlwi rN, rN, 24` to mask a value to 8 bits at bool type boundaries.

## Step 1: Detect

Look for `clrlwi` with `, 24` in objdiff output:
```
| delete | clrlwi r29, r29, 24 |    ← target has it, our build doesn't
| insert |                     | clrlwi r3, r11, 24    ← our build has it, target doesn't
```

## Step 2: Determine Direction

- **Target has `clrlwi`, our build doesn't** (`delete`) → This is the fixable direction. Go to Step 3.
- **Our build has `clrlwi`, target doesn't** (`insert`) → Usually unfixable. Go to Step 4.

## Worked Example: `PartyModeMgr::SetSongAndDefaults`

objdiff showed a single `delete` at instruction 121:
```
[118] equal      cmplw  cr6, r25, r11          | cmplw  cr6, r25, r11
[119] equal      bne    cr6, 0x79e4            | bne    cr6, 0x1ec
[120] equal      li     r29, 0x1               | li     r29, 0x1
[121] delete     clrlwi r29, r29, 24           |                        ← target masks r29 to bool
[122] equal      bl     MetaPerformer::Current  | bl     MetaPerformer::Current
...
[131] equal      mr     r5, r29                | mr     r5, r29         ← r29 passed as bool arg
```

**Reading the context:** r29 is set to 1 by a comparison (instructions 107-120 are a `mode == dance_battle || mode == strike_a_pose` check). Then r29 is passed as argument r5 to `CalcCharacters`. The target masks it to bool width; our build doesn't.

**The source had:**
```cpp
MetaPerformer::Current()->CalcCharacters(data, mode == dance_battle || mode == strike_a_pose, ...);
```

**Fix:** Extract to local bool:
```cpp
bool isSpecialMode = mode == dance_battle || mode == strike_a_pose;
MetaPerformer::Current()->CalcCharacters(data, isSpecialMode, ...);
```
Result: `delete` count 1→0, sizes matched, BOOL_MASK pattern gone.

## Step 3: Fix (target has mask, we don't)

**Bool funniness usually means there's an inline.** The `clrlwi` appears at bool type boundaries — inline function returns, local bool assignments, explicit casts. If the original code went through a bool-typed intermediate and our code doesn't, we'll be missing the mask.

Try these in order. Rebuild and check after each one.

**3a. Find the instruction context.** Look at the ~5 instructions before the `clrlwi`. Identify what register holds the bool value and what produced it (comparison? function call? logical or/and?). Then look at what consumes it (passed as argument? stored? returned?).

**3b. Extract to a local `bool` variable.** If a bool expression is passed directly as a function argument or used inline, extract it to a named local. This forces the compiler to treat it as a bool at the assignment boundary.

```cpp
// BEFORE — bool expr passed directly as argument, no mask generated:
Func(data, mode == dance_battle || mode == strike_a_pose, ...);

// AFTER — local bool forces compiler to mask at assignment:
bool isSpecialMode = mode == dance_battle || mode == strike_a_pose;
Func(data, isSpecialMode, ...);
```
*Real fix: `PartyModeMgr::SetSongAndDefaults` — 97.8% → 98.2%, bool mask eliminated.*

**3c. Add explicit `(bool)` cast.** Works for ternary expressions where one branch isn't typed as bool.

```cpp
// BEFORE — no mask:
_msg->Size() > 3 ? _msg->Int(3) != 0 : false
// AFTER — cast forces mask:
_msg->Size() > 3 ? (bool)(_msg->Int(3) != 0) : false
```
*Real fix: `RndTransformable::Handle` — 99.9% → 100%.*

**3d. Check for a missing inline function.** The original code may have called a bool-returning inline that we're writing as a raw expression. To find it:
1. Use `lookup_rb3` to check if the RB3 decomp uses an inline helper at that point
2. Check headers for existing inline bool helpers (`streq()`, `IsAsciiNum()`, `PowerOf2()`, etc.)
3. Check if peer functions in the same translation unit use a common bool helper

**3e. Use `&&` chain instead of `||` with if/else.** When a bool function checks multiple conditions and returns true/false, the expression form matters critically:

```cpp
// BEFORE — generates bnelr (conditional return), no clrlwi, r3 used directly:
if (mState == val0 || mState == val2 || mState == val3) {
    return false;
}
return true;

// AFTER — generates intermediate r11 + clrlwi mask, matching target:
return mState != val0 && mState != val2 && mState != val3;
```

The `&&` chain as a single `return` expression forces the compiler to materialize the boolean result in an intermediate register (r11) and copy it to r3 via `clrlwi`. The `if/return false/return true` pattern allows the compiler to use `bnelr` (merging conditional branch with return), putting the result directly in r3 and skipping the mask.

*Real fix: `XboxMultipleItemsPurchaser::IsPurchasing` — 84.5% → 100%, BOOL_MASK eliminated.*

**Key principle:** De Morgan's law (`!(a || b) == !a && !b`) is semantically equivalent but produces different codegen. If the target has `clrlwi` with `bne` (forward branch), try the `&&` chain form. If the target has `beq` with early returns, try the `||` form.

## Step 4: When It's Actually Unfixable

If our build generates `clrlwi` that the target doesn't have (`insert` direction), or if all Step 3 approaches fail, accept the gap. These source-level changes have been tried and do not remove an unwanted `clrlwi`:

- `return 1` instead of `return true`
- Direct condition return (`return ptr != NULL`)
- Ternary (`return x ? true : false`)

**Typical unfixable gap:** ~1-3%.

---

## extrwi vs rlwinm Bit Test Encoding

**Prevalence:** Functions with flag/enum bit tests
**Fixable** — use `bool` type to select the correct encoding.

When testing individual bits in flags, the compiler can generate two different `rlwinm` encodings:

| Encoding | Assembly | Result |
|----------|----------|--------|
| Mask-in-place | `rlwinm. rA, rS, 0, MB, ME` | 0 or bit value (e.g. 0 or 2) |
| Extract-to-LSB (extrwi) | `rlwinm. rA, rS, rot, 31, 31` | 0 or 1 |

Both are `rlwinm` machine instructions — `extrwi` is an assembler alias for `rlwinm` with rotate+mask that isolates a single bit to the LSB.

### Symptom

objdiff shows `replace` mismatch:
```
[21] replace: extrwi.  r10, r11, 1, 30   vs   rlwinm.  r10, r11, 0, 30, 30
```

The rotate and mask fields differ — target rotates and extracts to LSB; our code masks in place.

### Root Cause (Confirmed via Callgrind-Diff)

Callgrind profiling of c2.dll showed **8,214 divergent addresses across 559 clusters** between the two encodings — the `bool` type triggers a fundamentally different optimization path through UTC (the compiler backend), not just a trivial encoding table selection.

The C++ `bool` type (1-byte, always 0/1) forces the compiler to **materialize a boolean value** in the IR. This propagates through the optimizer and selects the extract-to-LSB encoding. All other forms get optimized back to mask-in-place.

### Source Pattern Matrix

| Source Pattern | Encoding | Why |
|---------------|----------|-----|
| `flags & MASK` | rlwinm (mask) | Direct truth test, no bool materialization |
| `(flags & MASK) != 0` | rlwinm (mask) | Comparison optimized away inline |
| `!!(flags & MASK)` with `int` | rlwinm (mask) | `int` type, `!!` optimized away |
| **`bool b = (flags & MASK) != 0;`** | **extrwi (extract)** | **`bool` type forces 0/1** |
| **`bool(flags & MASK)`** | **extrwi (extract)** | **Cast to `bool` forces 0/1** |

### Fix Patterns

**Pattern A: Extract to local `bool` variable (when result is reused)**
```cpp
// BEFORE — generates rlwinm. r,r,0,30,30 (mask-in-place)
if ((mType & kRendered) && mNumMips) { ... }

// AFTER — generates extrwi. r,r,31,31,31 (extract-to-LSB)
bool isRendered = (mType & kRendered) != 0;
if (isRendered && mNumMips) { ... }
```

**Pattern B: Inline `bool()` cast (when result is used once)**
```cpp
// BEFORE — generates rlwinm. (mask-in-place)
if ((mType & kMovie) && (mType & 0x20)) { ... }

// AFTER — generates extrwi. (extract-to-LSB)
if (bool(mType & kMovie) && (mType & 0x20)) { ... }
```

### Permuter Pattern

The `bit_test_bool` permuter pattern automatically tests both `bool` casts and variable extractions on bitwise AND expressions to match target bit-test encoding (`extrwi` vs `rlwinm`).

### Worked Example: DxTex::ResetSurfaces

```cpp
// Before (96.5% match, 2 replace mismatches):
if (((mType & kRendered) && mNumMips) || ((mType & kMovie) && (mType & 0x20))
    || (mType & kScratch) || (mType & kRegularLinear)) {

// After (98.4% match, 0 replace mismatches):
bool isRendered = (mType & kRendered) != 0;
if ((isRendered && mNumMips) || (bool(mType & kMovie) && (mType & 0x20))
    || (mType & kScratch) || (mType & kRegularLinear)) {
```

Both `replace` mismatches eliminated. Remaining differences are register swaps only.

### Detection

Look for `replace` mismatches in objdiff where:
- Target has `rlwinm.` with non-zero rotate and mask `31, 31` (or `extrwi.` alias)
- Our build has `rlwinm.` with rotate 0 and single-bit mask (e.g. `0, 30, 30`)

The rotate value in the extrwi form equals `32 - bit_position`, so `extrwi. rA, rS, 1, 30` = `rlwinm. rA, rS, 31, 31, 31` (extracting bit 1 = value 0x2).

---

## See Also

- [fixable-declarations.md](fixable-declarations.md#variable-extraction) - Variable extraction (related technique)
- [fixable-casting.md](fixable-casting.md) - Other casting fixes
- [unfixable-compiler.md](unfixable-compiler.md) - Compiler patterns (register allocation now has mechanism details)
