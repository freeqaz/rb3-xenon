# Technical Notes - Compiler Patterns & Lessons Learned

This document captures technical knowledge gained during decompilation sessions.

---

## Compiler Behavior (MSVC for Xbox 360)

### Inlined Functions

DC3's compiler inlines these standard library functions:
- `strcpy`
- `strlen`
- `strcmp`
- `strcat`

Also inlines:
- `DataArray::Int()`, `Float()`, `Str()`
- `std::vector` and `std::list` methods

**Pattern recognition:** Look for loop patterns that indicate inlined string operations.

### Static Symbol Initialization

MSVC uses bit flags for static local variable initialization.

```cpp
static Symbol foo("foo");  // Uses bit 0x1
static Symbol bar("bar");  // Uses bit 0x2
static Symbol baz("baz");  // Uses bit 0x4
```

**Assembly pattern:**
```asm
ori r11, r11, 0x1   ; First static
ori r11, r11, 0x2   ; Second static
ori r11, r11, 0x4   ; Third static
```

**Important:** Order of static Symbol declarations in source must match the original.

### Static Variable Scoping

Static variables must stay in their original scope. Moving them affects code generation.

```cpp
// CORRECT (99.6% match) - static in block scope
{
    static int _x = MemFindHeap("physical");
    MemHeapTracker mem(_x);
    // use mem...
}

// BROKEN (99.5% match) - static moved outside block
static int _x = MemFindHeap("physical");
{
    MemHeapTracker mem(_x);
    // use mem...
}
```

Even small scope changes affect the initialization guard patterns the compiler generates.

### Register Allocation Patterns

The compiler's register allocation is sensitive to:
- Local variable declarations
- Loop structure (while vs for)
- Parameter usage vs local copies
- Variable declaration order within blocks
- Literal types in initializer lists

**Example fix (FillModeArrayWithParentData):**
```cpp
// Before (97.5% match) - modifies parameter directly
while (a2->FindArray(sym)->...) {
    sym = ...;
}

// After (100% match) - uses local variable
for (Symbol s = sym; a2->FindArray(s)->...;) {
    s = ...;
}
```

### Initializer List Literals

Use `0` instead of `0.0f` or `false` for consistent register allocation:

```cpp
// Before (0% match) - different literal types
Shuttle::Shuttle() : mMs(0.0f), mEndMs(0.0f), mActive(false), mController(0) {}

// After (100% match) - all use integer literal 0
Shuttle::Shuttle() : mMs(0), mEndMs(0), mActive(0), mController(0) {}
```

### Boolean Index Expressions

Use arithmetic instead of comparison for boolean-to-index conversion:

```cpp
// Before (95.5% match) - generates cntlzw + extrwi instructions
label = mBAMColumns[side == 0]->Find<HamLabel>(...);

// After (improved) - simpler arithmetic
label = mBAMColumns[1 - side]->Find<HamLabel>(...);
```

### Control Flow Structure Must Match Exactly

Even logically equivalent control flow changes can break matches. Moving a conditional inside another block changes instruction ordering.

```cpp
// CORRECT (99.7% match) - threshold check outside HasSong block
if (thresh >= prereqNum) {
    return true;
}
if (HasSong(sym)) {
    // other logic
}

// BROKEN (99.3% match) - moved threshold check inside
if (HasSong(sym)) {
    if (thresh >= prereqNum) {
        return true;
    }
    // other logic
}
```

The compiler generates different branch structures even when the logic is equivalent.

### Ternary vs If-Else

Ternary operators often match better than if-else for simple conditionals:

```cpp
// Before - generates extra branches
bool ret;
if (progress) {
    ret = progress->IsEraComplete();
} else {
    ret = false;
}

// After - cleaner codegen
bool ret = progress ? progress->IsEraComplete() : false;
```

### Variable Declaration Order

The order of variable declarations affects register allocation:

```cpp
// Before (99.9% match) - iStarCount declared with iReqStars
int iReqStars = pEra->GetSongRequiredStars(song), iStarCount = 0;
CampaignEraProgress *pEraProgress = GetEraProgress(era);

// After (improved) - iStarCount declared after pointer
int iReqStars = pEra->GetSongRequiredStars(song);
CampaignEraProgress *pEraProgress = GetEraProgress(era);
int iStarCount = 0;
```

**Critical:** Even moving a simple `int total = 0` from before a pointer declaration to after it can break a 96% match down to 91%. The compiler assigns registers based on declaration order, and changing that order changes the entire register allocation scheme for the function.

```cpp
// CORRECT (96.1% match) - total declared before pointer
int total = 0;
CampaignEraSongProgress *pEraSongProgress = GetEraSongProgress(name);

// BROKEN (91.8% match) - total moved after pointer
CampaignEraSongProgress *pEraSongProgress = GetEraSongProgress(name);
int total = 0;  // This breaks the register allocation!
```

### Assignment in Function Arguments

Sometimes values need to be stored before use:

```cpp
// Before - separate assignment and call
era = pEra->GetName();
CampaignEraProgress *progress = GetEraProgress(era);

// After - assignment within call (matches stw instruction pattern)
CampaignEraProgress *progress = GetEraProgress(era = pEra->GetName());
```

---

## Merged Functions

"Merged functions" occur when the compiler generates identical machine code for different functions and combines them at the same address.

### Types of Merged Functions

1. **Scalar Deleting Destructors**
   - Compiler-generated for `delete` expressions
   - Pattern: `??_E<ClassName>@@...`
   - Not written in source code

2. **Virtual Function Thunks**
   - Generated for multiple inheritance
   - Adjust `this` pointer before calling actual implementation
   - Pattern: `??_E<ClassName>@@$4PPPPPPPM@...`

3. **Identical Implementations**
   - Simple functions with same code (e.g., trivial destructors)
   - `RELEASE(mPtr)` pattern often merges

### How to Handle

- **Don't try to write these manually** - they're compiler artifacts
- **Keep source code as-is** - the actual functions are correct
- **Mark in build config** - already handled in splits.txt
- **Ignore 0% match** - expected for merged functions

### Known Merged Functions

**SongDB (1 function):**
- Destructor body (80 bytes) - `RELEASE(mSongData)` pattern

**HamUser (7 functions):**
- 3 scalar deleting destructors (User, LocalUser, HamUser)
- 4 virtual thunks for multiple inheritance

### ICF (Identical COMDAT Folding)

The linker uses ICF to merge functions with **identical machine code** to a single address. This is why:
- Multiple symbol names point to the same address
- objdiff shows `merged_<address>` instead of a function name

**Statistics:**
- 31,754 COMDAT-folded symbols in the binary
- 3,068 unique merged addresses
- Some addresses have 76+ symbols (e.g., `ObjRefConcrete<T>::GetObj()` template)

**Common ICF Patterns:**

| Pattern | Example | Count |
|---------|---------|-------|
| Destructor pairs | `??_G` + `??_E` (scalar/vector) | 1,206 |
| Template instantiations | `ObjRefConcrete<T>::GetObj()` | 1,407 |
| Simple getters | Empty or trivial returns | varies |

**Address-Based vs Named Merged Functions:**

- **Named**: `merged_Read4FloatStruct` - has a recognizable name
- **Address-based**: `merged_82331360` - linker generated address

**How to Look Up Merged Addresses:**

```bash
# CLI tool
./bin/merged-symbols 82331360

# MCP tool (for agents)
mcp__orchestrator__lookup_merged_symbol address="82331360"
```

**Example output:**
```
Address 0x82331360: 2 symbols merged by ICF

  1. ObjRef::`scalar deleting destructor'(unsigned int) (App.obj)
  2. ObjRef::`vector deleting destructor'(unsigned int) (App.obj)
```

**Why it matters:** When objdiff shows a call to `merged_82331360`, any of the symbols at that address is valid. The function is at its limit - you cannot control which merged symbol the linker chooses.

---

## Common Matching Issues

### Issue: Comparison operators

Subtle differences in comparison can affect codegen:
- `size >= 1` vs `size > 0`
- `depth < 1` vs `depth == 0`
- `n < depth` vs `depth > n`
- `i3 < 2` vs `i3 <= 1` - affects cmpwi immediate value

```cpp
// Before (98.9%) - generates cmpwi r27, 0x2 + bge
if (i3 < 2) {

// After (100%) - generates cmpwi r27, 0x1 + bgt
if (i3 <= 1) {
```

**Diagnosis:** Check objdiff for `cmpwi` immediate values and branch conditions (`bge` vs `bgt`).

### Issue: Unsigned Zero Comparisons (CONFIRMED)

For **unsigned** variables compared against zero, the compiler generates different branch instructions:

| Source Pattern | Our Build | Original | Fix |
|---------------|-----------|----------|-----|
| `if (x != 0)` | `cmpwi` + `beq` | `cmpwi` + `ble` | Use `if (x > 0)` |
| `if (x == 0)` | `cmpwi` + `bne` | `cmpwi` + `bgt` | Use `if (x <= 0)` or `if (!(x > 0))` |

```cpp
// Before - generates beq branch
unsigned int count = GetCount();
if (count != 0) {
    DoSomething();
}

// After - generates ble branch (matches original)
unsigned int count = GetCount();
if (count > 0) {
    DoSomething();
}
```

**Important:** This pattern ONLY applies to zero comparisons with unsigned types. Other transformations like `> X` to `>= X+1` make matches WORSE. Only use this for unsigned zero comparisons.

**Diagnosis:** Look for `beq`/`bne` vs `ble`/`bgt` differences in objdiff when comparing against zero.

### Issue: Function Argument Evaluation Order

The compiler evaluates function arguments **right-to-left**. For `func(a, b)`, it evaluates `b` first, then `a`. This affects the order of load instructions.

```cpp
// Before (99.1%) - mStr evaluated second (loaded second)
return strcmp(mStr, str.c_str()) == 0;

// After (100%) - mStr evaluated first (loaded first, matches target)
return strcmp(str.c_str(), mStr) == 0;
```

**Diagnosis:** Use objdiff to compare load instruction order. If target loads `this->member` before `param->member`, swap the arguments.

### Issue: sizeof() Returns Unsigned

`sizeof()` returns `size_t` (unsigned), which promotes the entire expression to unsigned. This affects division codegen:

- **Unsigned division** by power of 2: `srwi` (shift right word immediate)
- **Signed division** by power of 2: `srawi` + `addze` (arithmetic shift + carry correction)

```cpp
// Before (93.8%) - unsigned division, generates srwi
return (NumVerts() * 0x50 + NumFaces() * sizeof(Face)) / 1024;

// After (100%) - signed division, generates srawi + addze
return (NumVerts() * 0x50 + NumFaces() * (int)sizeof(Face)) / 1024;
```

**Diagnosis:** Look for `srwi` vs `srawi`/`addze` mismatch in objdiff. If return type is `int` but code generates `srwi`, cast the `sizeof()` to `int`.

### Issue: Bitwise Word-Aligned Formulas

Sometimes the compiler uses `clrrwi` (clear right word immediate) instead of division for computing word-aligned sizes:

```cpp
// Before (97.93%) - standard division formula, generates srawi + addze
FixedSizeAlloc((x + 15) / 4, ...)

// After (100%) - bitwise formula, generates srawi + clrrwi
FixedSizeAlloc(((x + 15) >> 2) & ~3, ...)
```

`clrrwi r4, r11, 2` clears the bottom 2 bits (`& ~3`). This pattern appears in allocator code for word-aligned byte calculations.

### Issue: Loop Counter Signedness

Loop counters affect comparison instruction selection:

- **Signed loop counter**: `cmpwi` (compare word immediate)
- **Unsigned loop counter**: `cmplwi` (compare logical word immediate)

```cpp
// Before (98%) - signed comparison, generates cmpwi
int size;
bs >> size;
for (; size != 0; size--) { ... }

// After (100%) - unsigned comparison, generates cmplwi
unsigned int size;
bs >> size;
for (; size != 0; size--) { ... }
```

### Issue: String Iteration Signedness

When iterating over strings for hashing or byte operations, use `unsigned char` to avoid sign extension:

- **Signed char**: generates `extsb` (extend sign byte) instruction
- **Unsigned char**: no sign extension, uses `cmplwi` for null check

```cpp
// Before (84.7%) - signed char generates extsb
for (const char *p = str; *p != '\0'; p++) {
    hash = hash * mult + *p;
}

// After (100%) - unsigned char, no sign extension
for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
    hash = hash * mult + *p;
}
```

**Diagnosis:** Look for `extsb` instructions in string processing code. If the original doesn't have them, switch to unsigned char iteration.

### Issue: Data Type Sizing (Store Instructions)

Member variable types affect store instruction selection:

- **32-bit store**: `stw` (store word)
- **16-bit store**: `sth` (store half)

```cpp
// Before (98.7%) - stw instruction for mPort
class NetAddress {
    unsigned int mIP;
    unsigned int mPort;  // Wrong! Generates stw
};

// After (100%) - sth instruction for mPort
class NetAddress {
    unsigned int mIP;
    unsigned short mPort;  // Correct! Generates sth
};
```

### Issue: OBJ_MEM_OVERLOAD Line Numbers

The `OBJ_MEM_OVERLOAD` macro embeds a line number that must match the original binary exactly:

```cpp
// Before (99.95%) - wrong line number
OBJ_MEM_OVERLOAD(0x1D)  // Generates li r5, 0x1d

// After (100%) - correct line number
OBJ_MEM_OVERLOAD(0x1B)  // Generates li r5, 0x1b
```

**Diagnosis:** Look for `li rX, 0xNN` differences in objdiff where `0xNN` is a small number (likely a line number).

### Issue: Wrapper Struct Duplicate Padding

Check if wrapper structs duplicate existing padding in their member types:

```cpp
// Vector3 already has internal padding
class Vector3 {
    float x, y, z;
    u32 PAD;  // Already 16 bytes total
};

// Before (99.9%) - wrapper adds duplicate padding!
struct Vector3Pad {
    Vector3 v;    // 16 bytes
    float pad;    // 4 bytes extra - WRONG!
};  // 20 bytes - causes wrong struct sizes downstream

// After (100%) - use the type directly
typedef Vector3 Vector3Pad;  // 16 bytes
```

**Diagnosis:** If a struct has wrong size affecting templates/arrays, check if any member types already have internal padding that wrapper structs are duplicating.

### Issue: STL empty() vs size() == 0

`empty()` and `size() == 0` are semantically equivalent but generate **different code**:

- **`empty()`**: Pointer comparison (`cmplw begin, end`)
- **`size() == 0`**: Division-based count (`divw (end-begin)/sizeof(T)`)

```cpp
// Before (87.88%) - pointer comparison
if (mTempoPoints.empty()) {

// After (100%) - division-based size check
if (mTempoPoints.size() == 0) {
```

**Diagnosis:** Look for `divw` in target vs `cmplw` in decomp. If target calculates actual element count via division, use `size() == 0`.

### Issue: Thread Function Pointers

Don't call the function when passing to thread creation APIs:

```cpp
// Before (94%) - WRONG: calls function, casts return value to pointer!
mThread = CreateThread(
    0, 0, (LPTHREAD_START_ROUTINE)ThreadEntry(0), this, 4, 0
);

// After (99.7%) - Correct: pass function pointer directly
mThread = CreateThread(0, 0, ThreadEntry, this, 4, 0);
```

**Diagnosis:** If target has `lis`/`addi` loading a function address but decomp doesn't, you're probably calling instead of passing the function.

### Issue: Free-List Allocator Patterns

Pool allocators use pointer chains. Common bugs:

```cpp
// BUG 1: Not following the chain
int *old = mFreeList;
mFreeList = old;  // NO-OP! Should be: mFreeList = (int*)*old;

// BUG 2: Not linking freed blocks
void Free(void *v) {
    v = mFreeList;           // WRONG: overwrites parameter
    mFreeList = (int *)v;    // Then assigns same value back
}

// CORRECT free-list insertion:
void Free(void *v) {
    *(int **)v = mFreeList;  // Store old head in new block's next pointer
    mFreeList = (int *)v;     // New block becomes head
}
```

**Diagnosis:** If allocator functions are sub-90% match, check if free-list pointer chains are being followed correctly. These bugs make allocators completely non-functional.

### Issue: extrwi vs rlwinm Bit Test Encoding

When testing individual bits in flags, the compiler can generate two different encodings:
- `rlwinm. rA, rS, 0, MB, ME` — mask-in-place (result is 0 or the bit value)
- `extrwi. rA, rS, 1, N` = `rlwinm. rA, rS, rot, 31, 31` — extract to LSB (result is 0 or 1)

The `bool` type with a separate variable forces the extract-to-LSB encoding:

```cpp
// Generates rlwinm. r,r,0,30,30 (mask-in-place)
if ((mType & kRendered) && mNumMips) { ... }

// Also generates rlwinm. (!! and != 0 get optimized away inline)
if (((mType & kRendered) != 0) && mNumMips) { ... }

// Generates extrwi. / rlwinm. r,r,31,31,31 (extract-to-LSB)
bool isRendered = (mType & kRendered) != 0;
if (isRendered && mNumMips) { ... }
```

The C++ `bool` type forces the compiler to materialize a 0/1 boolean value, selecting the extract encoding. Inline `!!` and `!= 0` get optimized back to mask-in-place. The `bool(expr)` cast also works for inline use:

```cpp
// Also generates extrwi. form inline
if (bool(mType & kMovie) && (mType & 0x20)) { ... }
```

**Diagnosis:** Look for `extrwi.` vs `rlwinm.` with different rotate/mask in objdiff `replace` mismatches. If target uses `rlwinm. rA, rS, 31, 31, 31` (or similar non-zero rotate with mask 31,31), use `bool` type for the bit test.

### Issue: Bool Return Truncation (clrlwi 24)

When a function returns `bool`, MSVC may insert `clrlwi rA, rB, 24` (= `rlwinm rA, rB, 0, 24, 31`) to truncate the return value to a single byte. This is the bool-mask pattern.

**Two types of clrlwi in bool context:**
- `clrlwi r, r, 24` — **byte truncation** (keep bottom 8 bits). Bool ABI: move 0/1 from temp to return register.
- `clrlwi r, r, 31` — **LSB extraction** (keep bottom 1 bit = `& 1`). Part of arithmetic comparison results.

**Rules discovered from controlled experiments:**

| Source Pattern | clrlwi? | Assembly |
|---|---|---|
| `return x > 0;` (single-arg compare) | **None** | `neg/andc/srwi r3` (branchless, directly into r3) |
| `return (flags & 0x4) != 0;` (bit test) | **None** | `rlwinm r3,r3,30,31,31` |
| `return !x;` (negate int) | **None** | `cntlzw/rlwinm` into r3 |
| `return (a-b) > 0;` (subtraction sign) | **None** | `subf/neg/andc/srwi` into r3 |
| `return a > b;` (signed 2-arg compare) | **31** | `subfc/eqv/srwi/addze` + `clrlwi r3,r11,31` |
| `return a > b;` (unsigned 2-arg compare) | **31** | `subfc/subfe` + `clrlwi r3,r11,31` |
| `return x & 1;` (mask LSB) | **31** | `clrlwi r3,r3,31` (IS the operation) |
| `return x > y ? true : false;` (ternary) | **31** | `subfc/eqv/srwi/addze` + `clrlwi r3,r11,31` |
| `return a>0 && b>0 && c>0;` (&& chain, 3+) | **24** | `li r11,0/1` branches + `clrlwi r3,r11,24` |
| `bool r = (f&4) && c>0; return r;` (stored) | **24** | `li r11,0/1` branches + `clrlwi r3,r11,24` |
| `return (f&4) && c>0;` (inline && chain) | **24** | Same as stored bool |
| `if(a>0) return true; ... return false;` | **None** | `li r3,1/blr` per branch (directly into r3) |
| `if(a>0&&b>0) r=true; else r=false; return r;` | **None** | `li r3,1/bgtlr` + `li r3,0/blr` |
| `!bool_param` (negate bool parameter) | **24 at start** | `clrlwi r11,r3,24` truncates incoming param |

**Key findings:**

1. `clrlwi 24` (byte truncation) appears when the compiler assigns 0/1 to a **temp register** (r11) via `li` branches and then moves to r3 with truncation. This happens with `&&` short-circuit chains of 3+ conditions, or `bool` variables stored then returned.

2. `clrlwi 31` (LSB extraction) appears as part of **arithmetic comparison** sequences that compute a multi-bit intermediate. The `& 1` extracts the boolean result.

3. **No clrlwi** when the compiler can put 0/1 **directly into r3** — either via branchless arithmetic (neg/andc/srwi for single comparisons) or via direct `li r3, 0/1` + `blr` branches (if/else with explicit returns).

4. The `clrlwi 24` specifically appears when the optimizer fails to promote the 0/1 from r11 to r3, leaving a copy+truncate. This is a control flow structure issue, not a type issue.

**Fix strategy for mismatches:**
- **Target has `clrlwi 24`, we don't**: Target used `&&` chain or stored bool pattern. Our code probably branches directly to `li r3, 0/1`. Rewrite using `&&` chains: `return cond1 && cond2 && cond3;`
- **We have `clrlwi 24`, target doesn't**: Our code uses `&&` chain where the target branches directly. Rewrite with explicit `if (...) return true; ... return false;` pattern.
- **Target has `clrlwi 31`, we don't (or vice versa)**: Different arithmetic comparison encoding. The `clrlwi 31` comes from 2-argument signed/unsigned comparisons (`a > b`). Try different comparison forms.
- **Bool parameter truncation** (`clrlwi 24` at function start): Check if parameter should be `bool` vs `int`.

**Important: `clrlwi 24` vs `clrlwi 31` have DIFFERENT causes and fixes!**

**Diagnosis:** Run `python scripts/analysis/batch_pattern_scan.py --pattern bool_mask` to find all functions with this mismatch. Cross-reference with the assembly listing (`/FAs`) to see whether clrlwi is on the return path or parameter path.

### Issue: Boolean Negation Encoding (subic/subfe vs cntlzw/extrwi)

When negating a value to produce a boolean result (`!x`), the compiler uses different instruction sequences depending on whether the input is `bool` or `int`:

| Input Type | `!x` encoding | Instructions |
|---|---|---|
| `bool` (1-byte) | cntlzw + extrwi | `clrlwi r11,r3,24` / `cntlzw r11,r11` / `rlwinm r3,r11,27,31,31` |
| `int` (4-byte) | cntlzw + extrwi (no truncate) | `cntlzw r11,r3` / `rlwinm r3,r11,27,31,31` |
| `int` (via carry) | subic + subfe | `subic r11,r11,1` / `subfe r11,r11,r11` |

The `subic/subfe` pattern produces an all-ones (0xFFFFFFFF) or all-zeros mask, which is then used with `and` for conditional selection. This is the "carry-based negate" and typically appears in more complex expressions where the result feeds into further bitwise ops.

**Diagnosis:** Look for `replace` mismatches where one side has `subic/subfe` and the other has `cntlzw/extrwi`. The fix may require changing the variable type or expression structure to match the target's encoding choice.

### Issue: Loop structure

```cpp
// These generate different code:
while (condition) { ... }
for (; condition;) { ... }
for (Type x = init; condition;) { ... }
```

### Issue: Variable caching

```cpp
// Sometimes need to cache values:
int arrSize = arr->Size();  // Cache in local
for (int i = 0; i < arrSize; i++) { ... }

// vs direct call:
for (int i = 0; i < arr->Size(); i++) { ... }
```

### Issue: Intentional bugs in original

Sometimes the original code has bugs that must be preserved:

```cpp
// GameMode::SetMode line 152
if (parent_mode == campaign)  // Compares two different static Symbols
                              // Always false - but matches original!
```

---

## Debugging Techniques

### Offset Mismatch Diagnosis (`[off:-N]`)

When `run_diff_inspect` shows offset mismatches like `lfs f0, 0x54, r31` (target) vs `lfs f0, 0x50, r31` (base), you need to determine whether the base register (`r31`) is `this` (class member access) or a stack pointer (local variable access).

**Step 1: Determine if it's class layout vs stack layout**

Check other functions in the same unit. This is the key diagnostic:

- If OTHER functions accessing the same class also show `[off:-N]` → **class layout bug** (fixable by adding/reordering members in the header)
- If OTHER functions have NO offset mismatches → **stack frame layout** (usually unfixable — compiler allocates stack differently)

```
Example: StorePreviewMgr::PlayCurrentPreview had [off:-8] on 10 instructions.
But SetCurrentPreviewFile (same class) had ZERO offset mismatches.
→ Stack frame issue, not class layout. Unfixable.
```

**Step 2: For class layout bugs**

If the offset is consistent across functions (same `[off:-N]`):

1. Calculate the missing bytes: `N` bytes are missing before the accessed members
2. Use `mcp__orchestrator__lookup_struct_offset` or `ghidra-struct` to find what should be at the gap
3. Add the missing member(s) to the header

**Step 3: For stack layout bugs (small offsets like ±4)**

A `[off:-4]` on a struct field access via stack might indicate accessing the **wrong field** of a struct:

```
Example: HighFiveGestureFilter::Update had [off:-4] on two lfs instructions.
The function compared screenPos.x but Ghidra showed the target accessed screenPos.y.
Vector2.x is at offset 0, Vector2.y is at offset 4 — exactly the 4-byte difference.
Fix: Change .x to .y → 99.6% → 99.7%
```

**Quick Reference:**

| Offset | Likely Cause | Fixable? |
|--------|-------------|----------|
| ±4 | Wrong struct field (x vs y, etc.) | Often yes |
| ±8 | Missing member OR stack frame | Check other functions |
| ±0x10+ | Wrong base class size or missing inheritance | Usually yes |
| Consistent across unit | Class layout bug | Yes |
| Only in one function | Stack frame / compiler | Usually no |

### Using objdiff

```bash
# CLI comparison
objdiff-cli diff -u 45410914 -t PresenceMgr.cpp

# For detailed analysis, use objdiff GUI
# Shows side-by-side instruction comparison
```

### Reading Target Assembly

```bash
# Find function in target assembly
grep -n "FunctionName" build/45410914/asm/lazer/game/File.s

# View around a specific address
grep -A 50 "8287" build/45410914/asm/lazer/game/File.s
```

### Build Single File

```bash
# Faster iteration - build just one object
ninja build/45410914/src/lazer/game/GameMode.obj

# Then regenerate report
ninja build/45410914/report.json
```

### Check Specific Function Match

```bash
# Use objdiff-cli report function for direct lookup
./bin/objdiff-cli report function build/45410914/report.json "Game::Poll"

# Or query by unit pattern
./bin/objdiff-cli report query build/45410914/report.json --functions \
  --unit "default/lazer/game/GameMode" --min-percent 0 -f json-pretty
```

### Post-Compilation Register Swap Patcher

The compiler's register allocator sometimes assigns registers differently than the original build, causing 0.1-2% match gaps even when the code is functionally correct. The `obj_regswap_patcher.py` tool fixes these by directly patching register fields in compiled .obj files.

```bash
# Apply all known register swaps (uses scripts/regswap_manifest.json)
ninja && python3 scripts/obj_regswap_patcher.py --batch --apply

# Patch a single function
python3 scripts/obj_regswap_patcher.py "?Clamp@Box@@QAA_NAAVVector3@@@Z" --apply

# Dry run to see what would change
python3 scripts/obj_regswap_patcher.py --batch
```

**How it works:**
1. Runs objdiff to get instruction-level JSON diff for each function
2. For each `diff_arg` instruction with register mismatches, finds the exact 5-bit register field in the instruction word
3. Patches the field to match the target, handling PowerPC format-specific field ordering (logical vs arithmetic ops, D-form vs X-form, pseudo-instructions like `mr`)
4. Safety check: reverts automatically if match percentage decreases

**Results:** 17 functions fixed to 100%, 679+ functions improved. The manifest (`scripts/regswap_manifest.json`) lists all 709 patchable functions.

**Important:** Patches are lost on rebuild. Run the patcher after each `ninja` build.

### Post-Compilation Atexit Scope Counter Patcher

MSVC assigns sequential scope counters (e.g. `?BJ` vs `?CJ`) to every `{}` block in a function. These counters are embedded in the mangled names of static locals, guard variables, dynamic initializers, and atexit destructors (`??__F<var>@?<counter>?<containing>@YAXXZ`). Even small brace differences between our source and the original cause ALL atexit destructors in the affected function to get different mangled names, making objdiff unable to find the base counterpart and reporting 0% match — even though the 12-28 byte function bodies are byte-identical.

The `obj_atexit_scope_patcher.py` tool fixes this via fuzzy symbol matching:

```bash
# Rename base .obj atexit destructors to match target scope counters
python3 scripts/obj_atexit_scope_patcher.py --batch --apply

# Verify and auto-mark newly matching ??__F destructors as COMPLETE
python3 scripts/atexit_fuzzy_verify.py --apply

# Run the self-tests for the canonical key parser
python3 scripts/obj_atexit_scope_patcher.py --selftest
```

**How it works:**
1. Parses `??__F*` symbols in both target and base .obj files
2. Computes a canonical key by stripping the `?<counter>` scope token
3. For canonical keys present on both sides:
   - 1:1 case: renames the base symbol to match the target's scope counter
   - m:n case: pairs positionally by sorted scope counter value
4. Verifies byte-equality of the function bodies before renaming
5. Symbol renames only (machine code + relocations unchanged); storage class stays STATIC so the linker never sees these names, preserving link integrity

`atexit_fuzzy_verify.py` then runs objdiff with `functionRelocDiffs=none` to ignore remaining address-relocation noise (target uses `lbl_<addr>` vs base uses `?<var>@?<scope>@...` for the static-local data pointer). When `instruction_summary.equal_percent == 100.0` and `base_size > 0`, it marks the function as COMPLETE with `verdict_reason='atexit_fuzzy_scope_match'`.

**Results:** 264 atexit symbols renamed across 118 files, 262 functions auto-promoted to COMPLETE (remaining 89 are genuine stubs where our C++ source is missing the static declaration entirely). Registered as a post-compile step in `configure.py` so it runs automatically on every `ninja` build.

---

## Class Layout Reference

### PresenceMgr (DC3)
```
Offset  Type          Member
0x2c    DataArray*    mPresenceModes
0x30    DataArray*    mPresenceModeContexts
0x34    DataArray*    mInstrumentPlayModeContexts
0x38    Symbol        unk38
0x3c    int           mSongID
0x40    bool          mInGame
```

### PresenceMgr (RB3) - For Comparison
```
Offset  Type          Member
0x1c    DataArray*    unk1c (presence modes)
0x20    DataArray*    unk20 (contexts)
0x24    int           unk24
0x28    Symbol        unk28
0x2c    vector<Sym>   unk2c
0x34    int           unk34
0x38    bool          unk38 (in game)
0x39    bool          unk39 (override flag)
0x3c    int           unk3c (override value)
```

### HamUser Inheritance
```
    Object (Hmx)
        ↓
      User (virtual base)
        ↓
   LocalUser (virtual inheritance)
        ↓
    HamUser
```

---

## Lessons Learned

1. **Read the assembly first** - Understand what you're matching against
2. **Check RB3 for patterns** - Even if not identical, shows intent
3. **Small changes matter** - One variable declaration can change everything
4. **Preserve bugs** - If original has bugs, match them
5. **Merged functions are OK** - Compiler artifacts, don't fight them
6. **Use local variables** - Often needed to match register allocation
7. **Order matters** - Static declarations, member order, variable declarations
8. **Prefer ternary operators** - Cleaner codegen than if-else for simple cases
9. **Use `0` in initializers** - Not `0.0f` or `false`, affects register allocation
10. **Arithmetic over comparison** - `1 - side` not `side == 0` for boolean-to-int
11. **Run parallel agents** - 15+ subagents can work simultaneously on different targets
12. **Argument eval is right-to-left** - `func(a, b)` evaluates `b` first; swap args to change load order
13. **Cast sizeof() for signed math** - `sizeof()` is unsigned; use `(int)sizeof(T)` for signed division
14. **Free-list allocators follow pointers** - `mFree = *(Type **)ptr`, not `mFree = nullptr`
15. **Unsigned char for string iteration** - Avoids `extsb` sign extension; use `(const unsigned char *)str`
16. **Check wrapper struct padding** - Types like `Vector3` may already have internal padding; don't duplicate it
17. **Static const for float comparisons** - `static const float zero = 0.0f` forces memory load vs immediate
18. **Manual sqrt decomposition** - `sqrt(zz + yy + xx)` may match when `Length(v)` helper doesn't
19. **`empty()` vs `size() == 0`** - Different codegen; `empty()` compares pointers, `size()` uses division
20. **Don't call function pointers** - `CreateThread(..., ThreadFunc, ...)` not `CreateThread(..., ThreadFunc(), ...)`
21. **Sub-90% often means real bugs** - Low matches frequently have broken logic, not just codegen differences
22. **Load ordering via locals** - Declare variables in order you want them loaded; `int a = x; int b = y;` loads x then y
23. **Variable declaration position matters** - Moving `int x = 0` to a different line can break register allocation entirely
24. **Control flow must match exactly** - Moving a conditional block inside another (even if logically equivalent) changes instruction order
25. **Keep static variables in original scope** - Static declarations affect code generation; don't move them between scopes
26. **Unsigned zero comparisons** - Use `x > 0` instead of `x != 0` for unsigned types to match `ble` vs `beq` branches
27. **Sequential if + return vs if-else** - `if (x) { y; return; } if (z) {...}` may match when `if (x) { y; } else if (z) {...}` doesn't
28. **Dot product component order** - `((w*q.w + x*q.x) + z*q.z) + y*q.y` may match when `x*q.x + y*q.y + z*q.z + w*q.w` doesn't
29. **Commutative register swaps are unfixable** - `fmuls f11, f0, f13` vs `fmuls f11, f13, f0` - same result, different register order
30. **VMX128 XMVECTOR parameters** - Passed in v1 register, stored to stack via `stvx128`, then read as scalar floats at offsets +0x10 (x), +0x14 (y), +0x18 (z), +0x1c (w)
31. **64-bit to 16-bit extraction patterns** - Target may use `lhz` to load 16-bit slice from stored `__int64`; our compiler uses `ld` + bit masking (unfixable ~5% gap)
32. **`bool` type for extrwi encoding** - `bool b = (flags & MASK) != 0;` generates `extrwi.` (extract-to-LSB), while inline `flags & MASK` generates `rlwinm.` (mask-in-place)
33. **Multiple early returns → `||` chain** - 6 separate `if (cond) return false;` blocks generate 6 full inline+branch sequences; combining into `if (a || b || c || d || e || f) return false;` shares the branch target (+40% on operator>(Sphere, Frustum))
34. **Bool return expression** - `if (A || B) return false; return true;` → `return !(A || B);` or `return !A && !B;` changes bool materialization from branches to branchless `neg/andc/srwi` (+14.5% on HasKinectSharePrvilege, to 100%)
35. **Bitwise `&` for bool accumulator** - `acc = acc & boolVal;` generates `and` instruction; `acc = acc && boolVal;` generates short-circuit branches. Use `&` when accumulating bools in a loop
36. **Local bool for complex conditions** - Extracting `bool b = (complex || condition);` before `if (b)` forces `clrlwi 24` bool truncation matching target's materialization pattern (+7% on ShouldHoldDisplayInPlace)

---

## Known Unfixable Issues

Some mismatches are caused by linker or compiler behaviors we cannot reproduce. These represent hard limits on match percentages for affected functions.

### Linker-Level Optimizations (UNFIXABLE)

The target binary is a **debug build** (XBDM present in static libraries) and does **not** use LTCG (`/GL` + `/LTCG`). However, the linker still applies **ICF (Identical COMDAT Folding)** via `/OPT:ICF`, which merges functions with identical machine code to a single address.

**We compare at the .obj level**, so linker-level optimizations like float constant pooling and ICF will never match. This explains persistent 0.5-1% gaps on some functions that are otherwise correct.

### Float Constant Pooling (UNDER INVESTIGATION)

The original linker places float literals adjacent to static arrays, allowing a single base register for both:

```asm
# Original (single base for floats and static data)
addi r29, r10, base@l
lfs f0, 0x0(r29)      ; float constant
addi r7, r29, 0x8     ; gRevs[0]
```

Our build uses separate `.rdata` symbols for each float, requiring extra `lis` instructions:

```asm
# Our build (separate addresses)
lis r11, float1@ha
lfs f0, float1@l(r11)
lis r10, gRevs@ha
addi r7, r10, gRevs@l
```

This causes 1-2 instruction differences in functions with float literals.

### ASSERT_REVS Instruction Scheduling (UNFIXABLE)

The `ASSERT_REVS` macro's second `MILO_FAIL` call shows consistent scheduling differences:
- Target computes `gRevs[2]` address before stack variables
- Our build computes stack variables before `gRevs[2]`
- Same instructions, different order - compiler heuristic difference

This causes ~0.8-0.9% mismatch on all `Load` functions using `ASSERT_REVS`. These are considered effectively matched.

### fmadds vs fmuls+fadds (PARTIALLY FIXABLE)

The Xbox 360 compiler generates fused multiply-add instructions (`fmadds`, `fmsubs`, `fnmadds`, `fnmsubs`) when `#pragma fp_contract` is ON (the default). Controlled experiments confirmed:

| `#pragma fp_contract` | `a * b + c` | Dot product (3-term) |
|---|---|---|
| ON (default) | `fmadds` (1 instr) | `fmuls + 2x fmadds` (3 instrs) |
| OFF | `fmuls + fadds` (2 instrs) | `3x fmuls + 2x fadds` (5 instrs) |

**The pragma is file-scoped** and can be toggled multiple times within a file. Adding `#pragma fp_contract(off)` before functions will prevent fma fusion.

**Three categories of FMA mismatch** (from batch scan of 500 functions):

1. **Pure "need OFF"** — Our code generates fmadds where target has fmuls+fadds. Fix: add `#pragma fp_contract(off)` to the file. Found in: BustAMovePanel::PlayIntroVO, Multiply(Vector3,Quat,Vector3), CharClip::FindNode, BinkReader::Seek.

2. **Pure "need ON"** — Target has fmadds where our code doesn't fuse. Since fp_contract is ON by default, this means the expression structure in our code prevents fusion (e.g., operations too far apart in IR). Fix: restructure expressions to place multiply and add adjacent. Found in: ClipDistMap::CalcWidth, ArcDetector::PrintJointPath, Profiler::Stop, LoopVizCallback::UpdateOverlay, RndParticleSys::UpdateParticles.

3. **Mixed direction (UNFIXABLE by pragma alone)** — Same function has some expressions that need ON and others that need OFF. This is the compiler's scheduling heuristic deciding differently. Found in: ClipCollide::SyncWaypoint, QuatSpline, Intersect, MultiTempoTempoMap::TimeToTick, GetLightPosition.

**Strategy**: When implementing math-heavy functions, check FMA direction via objdiff. If ALL mismatches point the same direction, apply the pragma. If mixed, accept the gap.

**Impact**: ~1-3% mismatch on math-heavy functions. 14 functions affected across 800 scanned.

### Linker Merged Functions (ICF - Identical COMDAT Folding)

Functions with identical machine code are merged by the linker (Identical COMDAT Folding):

```
merged_Read4FloatStruct:
  ; operator>>(BinStream&, Color&) merged with
  ; operator>>(BinStream&, Rect&)
  ; Both read 4 floats, so linker combined them
```

Known merged patterns:
- `RELEASE(mPtr)` in destructors
- Simple 4-float struct reads
- Trivial virtual function implementations

**Lookup tool:** When objdiff shows `merged_82331360` (address-based), use the lookup tool to see which symbols share that address:
```bash
./bin/merged-symbols 82331360
# Or MCP: mcp__orchestrator__lookup_merged_symbol address="82331360"
```

**Verification workflow:** Don't blindly accept as unfixable. After lookup:
1. Check if YOUR call target is in the merged set
2. If yes → Unfixable, accept at_limit (code is correct)
3. If no → Investigate! You may be calling the wrong function

**Unfixable** only after verification confirms your code is correct. These show as 0% match in reports.

### Compiler Version Notes

- Xbox 360 SDK uses **MSVC 16.00.11886.00** (Visual Studio 2010)
- Linker: **LINK 10.0.11886.0**
- XDK SDK: **v2.0.21173.0**
- Target binary: **debug build** (XBDM present, no LTCG)
- `/Gw` (Optimize Global Data) is **NOT available** (added in VS2013)
- Current flags: `/O1 /Oi /GR /EHsc` (empirically confirmed correct — `/O2` breaks matches, `/fp:fast` has no effect)
- Rich header: 1871 C++ objects + 465 C objects compiled with cl.exe 16.00.11886, plus 3 objects from VS2005 (Bink middleware)
- Xbox 360 `/O1` = `/Oy /Ob2 /GF` (differs from standard MSVC; `/O2` = `/Oi /Oy /Ob2 /GF`)
- `/fp:fast` is the **default** on Xbox 360 (per XDK docs `xenon_compiler_technology.htm`)
- `#pragma fp_contract` is **ON by default** — controls fmadds generation
- Xbox 360-specific flags tested and rejected: `/Ou` (prescheduling — breaks matches), `/Oc` (disable traps — no effect)
- **`??_C@` string literal hashes**: The `??_C@` mangled name includes a CRC-32 hash (reflected polynomial `0xEDB88320`, init `0xFFFFFFFF`, no final XOR) computed over the **string content bytes including null terminator**. `cl.exe` calls `SigForPbCb` from `mspdbXX.dll` for this. The hash is encoded using A-P nibbles (A=0, B=1, ..., P=15). All `??_C@` hashes now match between decomp and original (0 mismatches out of 121 compared). Fixed via: (1) proper `SigForPbCb` in wibo's `mspdb_dll.cpp`, (2) `WIBO_PATH_MAP` with two source roots (`system/src/` and `lazer/src/`), (3) absolute mapped include paths, (4) 5 source string bug fixes.

---

## MSVC Mangled Number Encoding

MSVC encodes array sizes in mangled names (e.g., `MakeString<char[N], int, char[M]>`) using two schemes:

**Single digit (values 1-10):** Character `'0'`-`'9'` where `'0'`=1, `'1'`=2, ..., `'9'`=10.

**Multi-digit (values > 10):** Hexadecimal digits using letters A-P, terminated by `@`:
- A=0, B=1, C=2, D=3, E=4, F=5, G=6, H=7, I=8, J=9, K=0xA, L=0xB, M=0xC, N=0xD, O=0xE, P=0xF

Each letter represents one hex digit, read left-to-right: `BE@` → 0x14 → 20 decimal.

### Examples

| Mangled | Decoded | String |
|---------|---------|--------|
| `$$BY07$$CBD` | char const[8] | 7-char string + null |
| `$$BY0BE@$$CBD` | char const[20] | "StorePreviewMgr.cpp" (19+1) |
| `$$BY0CD@$$CBD` | char const[35] | 34-char condition string |
| `$$BY0O@$$CBD` | char const[14] | "mStreamPlayer" (13+1) |

### Application to MILO_ASSERT

`MILO_ASSERT(cond, line)` expands to `MakeString(kAssertStr, __FILE__, line, #cond)` which instantiates `MakeString<char[N], int, char[M]>` where:
- N = `strlen(__FILE__) + 1`
- M = `strlen(#cond) + 1`

Decoding both target and base mangled names reveals whether the mismatch is due to `__FILE__` length (unfixable build path difference) or `#cond` length (wrong assert expression — fixable).

---

## See Also

- [RB3_REFERENCE.md](RB3_REFERENCE.md) - Using RB3 as reference
- [LOW_HANGING_FRUIT.md](LOW_HANGING_FRUIT.md) - Prioritized function list
- [SUBAGENT_STRATEGY.md](SUBAGENT_STRATEGY.md) - Parallel AI agent workflow
- [../WORKSESSION.md](../WORKSESSION.md) - Main session notes
