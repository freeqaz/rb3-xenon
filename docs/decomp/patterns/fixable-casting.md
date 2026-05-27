# Fixable Patterns: Casting

Patterns related to type casting, float/double precision, and attributes.

---

## Explicit Float Cast

**Impact:** +35%
**Success Rate:** HIGH
**Time:** 5 minutes

Use explicit `(float)` casts and `floor()` (not `std::floor()`) for matching.

### Symptom

objdiff shows different function calls or extra precision conversion instructions.

### Why It Works

- `std::floor()` and `floor()` may resolve to different symbols
- Without explicit cast, compiler may promote to double

### Fix

```cpp
// Before
f1 = std::floor((f1 - mBStart) * mSamplesPerBeat + 0.5f);

// After
f1 = floor(((f1 - mBStart) * (float)mSamplesPerBeat) + 0.5f);
```

### Real Example

| Function | Before | After | Delta |
|----------|--------|-------|-------|
| ClipDistMap::CalcHeight | 64.4% | ~99% | +35% |

---

## noreturn Attribute

**Impact:** +38.5%
**Success Rate:** 100%
**Time:** 2 minutes

Add `__declspec(noreturn)` to functions that never return.

### Symptom

objdiff shows dead code after `exit()` or `abort()` calls that shouldn't be there.

### Why It Works

Tells the compiler that the function never returns, allowing it to eliminate dead epilogue code (stack cleanup, return instructions).

### Fix

```cpp
// In header or before use
__declspec(noreturn) void exit(int);

// Or for custom functions:
__declspec(noreturn) void FatalError(const char* msg);
```

### Real Example

| Function | Before | After | Delta |
|----------|--------|-------|-------|
| error_exit (jerror.c) | 61.5% | 100% | +38.5% |

---

## Float/Double Separation

**Impact:** +80%
**Success Rate:** 95%
**Time:** 10 minutes

Explicitly separate float and double operations with intermediate variables.

### Symptom

objdiff shows FPU register spillage or unexpected precision conversions.

### Why It Works

Mixed float/double operations cause FPU register spillage between precision modes. Separating them keeps operations in dedicated register sets.

### Fix

```cpp
// Before - mixed precision
float vorbis_fromdBlook(float a) {
    int i = vorbis_ftoi(a * ((float)INVSQ_LOOKUP_SZ) * 8.0f + 0.5f);
    // ... mixed operations
}

// After - separated precision
float vorbis_fromdBlook(float a) {
    float a8 = a * 8.0f + 0.5f;           // explicit float ops
    double dbl_val = 0.5 - (double)a8;     // explicit conversion
    // Separate float/double operations
}
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| vorbis_fromdBlook | 16.8% | 97.7% | +80.9% | Separated float and double intermediate vars |
| UpdateCache (RndShaderMgr) | 21.7% | 97.6% | +75.9% | Declared temp float vars for each load |
| PropKeys::Print | 99.95% | 100% | +0.05% | `frame = 0.0f` instead of `frame = 0` |

---

## Cast Placement Controls `fmul` vs `fmuls`

**Impact:** +10-15% when near-match, often final 0.5-5%
**Success Rate:** HIGH (for float math hotspots)
**Time:** 5-15 minutes

Moving the `(double)(float)` cast boundary can change only one opcode (`fmul` vs `fmuls`) while preserving behavior. This is especially useful in mixed float/double expressions where objdiff is down to a single real replace.

### Symptom

objdiff shows a lone real mismatch like:

- target: `fmuls`
- base: `fmul`

Everything else is already `diff_arg` noise.

### Why It Works

MWCC chooses single-precision vs double-precision FPU ops based on where the first promotion occurs. Wrapping the *entire* subexpression in `(double)(float)(...)` keeps the inner multiply/add chain in single precision.

### Fix

```cpp
// Before - final multiply promoted too early, emits fmul
double x = (double)(float)(a * b + c) * d;

// After - keep product in float, then promote, emits fmuls in inner chain
double x = (double)(float)((a * b + c) * d);
```

### Real Example

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| BinkReader::Seek | 85.9% | 98.9% | +13.0% | Float locals + cast placement removed all real mismatches |

---

## sizeof() Signedness

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 3 minutes

Cast `sizeof()` to `(int)` when used in signed arithmetic.

### Symptom

objdiff shows `srwi` (unsigned shift) vs `srawi+addze` (signed shift) mismatch.

### Why It Works

`sizeof()` returns `size_t` (unsigned), which promotes the entire expression to unsigned. This affects division codegen:
- **Unsigned division** by power of 2: `srwi` (shift right word immediate)
- **Signed division** by power of 2: `srawi` + `addze` (arithmetic shift + carry correction)

### Fix

```cpp
// Before - unsigned division, generates srwi
return (NumVerts() * 0x50 + NumFaces() * sizeof(Face)) / 1024;

// After - signed division, generates srawi + addze
return (NumVerts() * 0x50 + NumFaces() * (int)sizeof(Face)) / 1024;
```

### Detection

Look for `srwi` vs `srawi`/`addze` mismatch in objdiff. If return type is `int` but code generates `srwi`, cast the `sizeof()` to `int`.

---

## Data Type Sizing

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 5 minutes

Member variable types affect store instruction selection.

### Symptom

objdiff shows `stw` (32-bit store) vs `sth` (16-bit store) mismatch.

### Fix

```cpp
// Before - stw instruction for mPort
class NetAddress {
    unsigned int mIP;
    unsigned int mPort;  // Wrong! Generates stw
};

// After - sth instruction for mPort
class NetAddress {
    unsigned int mIP;
    unsigned short mPort;  // Correct! Generates sth
};
```

### Detection

Check for `stw` vs `sth` or `stb` differences. This indicates the member type size is wrong.

---

## Wrapper Struct Padding

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 5 minutes

Check if wrapper structs duplicate existing padding.

### Symptom

Struct size mismatch causing template/array offset issues.

### Why It Works

Some types already have internal padding. Wrapping them with additional padding creates wrong sizes.

### Fix

```cpp
// Vector3 already has internal padding
class Vector3 {
    float x, y, z;
    u32 PAD;  // Already 16 bytes total
};

// Before - wrapper adds duplicate padding!
struct Vector3Pad {
    Vector3 v;    // 16 bytes
    float pad;    // 4 bytes extra - WRONG!
};  // 20 bytes - causes wrong struct sizes downstream

// After - use the type directly
typedef Vector3 Vector3Pad;  // 16 bytes
```

### Detection

If a struct has wrong size affecting templates/arrays, check if any member types already have internal padding.

---

## Avoid Unnecessary dynamic_cast (GetObj vs Obj<T>)

**Impact:** +6%
**Success Rate:** HIGH
**Time:** 2 minutes

Use direct accessor methods instead of template wrappers that add `dynamic_cast` when the target binary doesn't use one.

### Symptom

objdiff shows extra instructions for a `dynamic_cast` call that doesn't appear in the target. The target uses a simple load (`lwz` + null check) while our code generates a full RTTI-based dynamic cast sequence.

### Why It Works

`DataArray::Obj<Hmx::Object>(i)` calls `dynamic_cast<Hmx::Object*>(GetObj(i))` internally. If the target just calls `GetObj(i)` directly (no cast), the dynamic_cast generates ~10-15 extra instructions for the RTTI lookup.

### Fix

```cpp
// Before — Obj<T>() adds dynamic_cast the target doesn't have
Hmx::Object *obj = a->Obj<Hmx::Object>(2);

// After — direct accessor, no cast
Hmx::Object *obj = a->GetObj(2);
```

### Detection

- Ghidra decompilation shows a simple function call, not a cast sequence
- RB3 reference uses direct accessor
- objdiff shows `bl __dynamic_cast` in base but not target

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| Object::OnRemoveSink | 93.6% | 99.6% | +6.0% | Changed both `Obj<Hmx::Object>(2)` to `GetObj(2)` |
| Object::OnAddSink | 77.9% | ~85% | +7% | Part of larger fix, removed unnecessary cast |

---

## Signed Pointer Comparison Cast

**Impact:** +0.5-1%
**Success Rate:** HIGH
**Time:** 1 minute

Cast a pointer to `(int)` when the target uses `cmpwi` (signed) instead of `cmplwi` (unsigned) for a null check.

### Symptom

objdiff shows `cmpwi` in target vs `cmplwi` in base for a pointer null check. Both compare against zero, but use different comparison instructions.

### Fix

```cpp
// Before — generates cmplwi (unsigned pointer comparison)
if (loader) { ... }

// After — generates cmpwi (signed comparison)
if ((int)loader) { ... }
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| Object::FindPathName | 86.4% | 87.1% | +0.7% | Cast `mLoader` to `(int)` for null check |

### Note

This is the opposite of the pattern documented in [unfixable-compiler.md](unfixable-compiler.md#requires-tooling-c2dll-patching-or-custom-pragmas) (cmplwi vs cmpwi). In some cases the `(int)` cast does work to force signed comparison.

---

## MakeString Template Type Mismatch (MILO Macro Arguments)

**Impact:** +5-21%
**Success Rate:** HIGH
**Time:** 5 minutes

When `MILO_LOG`, `MILO_NOTIFY`, `MILO_WARN`, or `MILO_FAIL` receive `Symbol` objects but the target passes `const char*`, the `MakeString<>` template instantiation changes, producing a different `bl` target.

### Symptom

objdiff shows a `diff_arg` on a `bl` instruction to `MakeString` where the template parameters differ in argument types:

```
Target: bl MakeString<const char*, const char*, const char*, unsigned char>
Base:   bl MakeString<Symbol, Symbol, Symbol, bool>
```

The `char[N]` size in the mangled name may also differ (this is the `__FILE__` string length — a separate issue, [resolved in objdiff](../../plans/MAKESTRING_ICF_EQUIVALENCE.md) via array-size normalization).

### Why It Works

`MakeString` is a variadic template. Each argument type is encoded in the mangled symbol name. Passing `Symbol` produces a different template instantiation than passing `const char*`. The target binary was compiled with explicit `.Str()` conversions, so it instantiates the `const char*` version.

### Detection

1. Look for `diff_arg` on `bl ??$MakeString@...` instructions
2. Decode the template parameters in both target and base mangled names
3. If target has `PBD` (pointer to `const char`) where base has `VSymbol@@`, the arguments need `.Str()` conversion

### Fix

```cpp
// Before — Symbol objects passed directly, generates MakeString<Symbol, Symbol, Symbol, bool>
Symbol symSong = TheGameData->GetSong();
Symbol symDefault = TheHamSongMgr.GetCharacter(symSong);
Symbol symPrimary = TheGameData->Player(0)->Char();
bool ret = symPrimary == symDefault;
MILO_LOG("... '%s' ... '%s' ... '%s' ret = %d\n",
    symSong, symDefault, symPrimary, ret);

// After — explicit .Str() on each, generates MakeString<const char*, const char*, const char*, unsigned char>
const char *songStr = symSong.Str();
const char *defaultStr = symDefault.Str();
const char *primaryStr = symPrimary.Str();
MILO_LOG("... '%s' ... '%s' ... '%s' ret = %d\n",
    songStr, defaultStr, primaryStr, ret);
```

### Important

- Keep the `Symbol` variables for any non-MILO uses (method calls, comparisons). Only convert to `const char*` for the MILO macro arguments.
- The `bool` → `unsigned char` change in the template is a side effect of the same conversion — `ret` as `bool` maps to `_N` (bool) in mangling, while `ret` as expression result maps to `unsigned char`.

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| Game::IsSongDefaultPlayerPlaying | 78.1% | 98.6% | +20.5% | 3 Symbol args → `const char*` via `.Str()` |

### Sub-pattern: Static Variable Type in MakeString Args

When a function passes a `static const T sVar = ...` to `MakeString`,
the variable's address gets a type annotation in the mangled symbol —
and `MakeString<int>` differs from `MakeString<EnumType>` even when
the values are identical. To pick the right type, decode target's
mangled `MakeString` template arg list and match it.

```cpp
// Before — passes static int, mangles as MakeString<..., int>
static const int sVersion = 0x1c;
mStream.MakeString("version=%d", sVersion);  // doesn't match target

// After — passes static enum, mangles as MakeString<..., SaveLoadManager::State>
static const SaveLoadManager::State sVersion = (SaveLoadManager::State)0x1c;
mStream.MakeString("version=%d", sVersion);  // matches target
```

The literal value the compiler stores is identical (`0x1c`); the only
difference is the template instantiation MakeString picks based on
`sVersion`'s declared type.

| Function | Before | After | Notes |
|----------|--------|-------|-------|
| `ProfileMgr::LoadGlobalOptions` | 98.5% | 100% | `sVersion` typed as `SaveLoadManager::State` instead of `int` so the `MakeString<int, SaveLoadManager::State>` template matches |

### Sub-pattern: Anonymous-Namespace Mangling on `MakeString` Globals

If a global helper used inside `MakeString`'s call chain (e.g. a
`ModalCallbackFunc *gRealCallback` shared across cases) sits inside
an anonymous namespace, its symbol mangles as
`?gRealCallback@?A0x...@@`, with a hash that depends on the TU's
contents. The same global at file scope mangles cleanly. If target
binary uses the file-scope form and we use the anonymous-namespace
form, every relocation referencing it differs.

```cpp
// Before — anonymous namespace, mangling-hashed
namespace {
    ModalCallbackFunc *gRealCallback;
}

// After — true file-scope global, clean mangling
ModalCallbackFunc *gRealCallback;
```

| Function | Before | After | Notes |
|----------|--------|-------|-------|
| `DebugModal` (call sites referencing `gRealCallback`) | n/a | aligned | Moved `gRealCallback` from anonymous namespace to file scope; idx 0-37 now match target byte-for-byte |

---

## Float-to-Int-to-Float Reconversion

**Impact:** +2-5%
**Success Rate:** HIGH
**Time:** 2 minutes

When passing a float value into a function or storing it, if the target binary uses `fctiwz` (float to int with zero round) followed immediately by `stfd` (store float double) or `fmr` (float move) to the same register, it's casting to `int` and then implicitly or explicitly casting back to `float`.

### Symptom

objdiff shows a sequence like:
```
Target: fctiwz f0, f1
        stfd f0, 0x58(r31)
Base:   stfs f1, 0x58(r31)
```

The target converts a float to an integer in the FPU register before saving it, while your code just saves the float directly. 

### Why It Works

The original code probably read a float from a data node, casted it to an integer (perhaps originally expecting an int), but then stored it in a float variable or passed it to a function expecting a float. 

### Fix

```cpp
// Before - direct float use
float num_stars_val = pNode->Float();
dataP.AddPair(perf_current_stars, num_stars_val); // Function expects float

// After - cast to int and back to match target's truncation
float num_stars_val = (float)(int)pNode->Float();
dataP.AddPair(perf_current_stars, (int)num_stars_val); 
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| GameEndedDataPointJob ctor | ~77% | 85.8% | ~8% | Added `(float)(int)` round-trip cast to `pNode->Float()` result |

---

## sizeof() Signed Materialization

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 1 minute

Cast `sizeof()` to `(int)` when the compiler uses signed division for a power-of-2 divisor.

### Symptom

objdiff shows `srawi` (signed arithmetic shift) vs `srwi` (unsigned logical shift) for division by a constant that is a power of 2.

### Why It Works

`sizeof()` is `size_t` (unsigned), so `value / sizeof(Type)` generates unsigned division (`srwi`). Casting to `(int)sizeof(Type)` forces signed division, which emits `srawi` for negative values in the dividend.

### Fix

```cpp
// Before - unsigned division
return (bytes / sizeof(int));

// After - signed division
return (bytes / (int)sizeof(int));
```

### Permuter Pattern

The `sizeof_signed_cast` permuter pattern automatically inserts `(int)` casts on `sizeof()` expressions in arithmetic contexts to test both signed and unsigned paths.

---

## Bool→Int Normalization via `b != 0`

**Impact:** +1-2% per site
**Success Rate:** HIGH
**Time:** 1 minute

Force MSVC to emit the bool→int normalize (`subic`/`subfe` pair) at an assignment site by writing `member = b != 0` instead of `member = b`.

### Symptom

A `bool` (or `char` used as boolean) is stored into a non-bool member — typically `int`, `unsigned`, or an enum. objdiff shows the target binary emits a `subic`/`subfe` normalize pair (canonical 0/1 lowering) immediately before the store; ours skips the normalize and stores directly.

### Why It Works

MSVC treats explicit `b != 0` as a distinct conversion site and emits the canonical bool→int normalize (`subic r0, rB, 1; subfe rR, r0, rB` → 0 or 1). Direct `member = b` is sometimes elided when the front-end can prove `b` is already a 0/1 value, even though the target binary kept the normalize. The explicit `!= 0` is a semantic no-op that forces the canonical lowering.

### Fix

```cpp
// Before — front-end may elide the normalize
void Foo::Load(BinStream& bs) {
    char b;
    bs >> b;
    mAlwaysInlined = b;                   // sometimes loses the subic/subfe pair
    proxyType = (InlineDirType)b;         // same
}

// After — explicit != 0 keeps the normalize
void Foo::Load(BinStream& bs) {
    char b;
    bs >> b;
    mAlwaysInlined = b != 0;
    proxyType = (InlineDirType)(b != 0);
}
```

### Detection

Run `run_diff_inspect mode=mismatches`. If inserts in target are `subic`/`subfe` pairs immediately preceding a store to a member offset, this pattern applies.

### Real Example

| Function | Before | After | Delta |
|----------|--------|-------|-------|
| ObjectDir::PreLoad | 98.6% | 99.6% | +1.0% (commit `e9bbdbf1`, applied at two sites: `mAlwaysInlined`, `proxyType`) |

---

## See Also

- [fixable-comparison.md](fixable-comparison.md) - Signedness comparison patterns
- [unfixable-compiler.md](unfixable-compiler.md) - When casting fixes don't work
