# Fixable Patterns: Operators

Patterns related to operator selection, FMA instructions, and expression structure.

---

## FMA Expression Order

**Impact:** +1-75%
**Success Rate:** 98%
**Time:** 5 minutes

Expression order determines which fused multiply-add variant is generated.

### Symptom

objdiff shows `fmsubs` vs `fnmsubs` mismatch.

### Why It Works

PowerPC has two FMA subtract variants:
- `(x*y - 1.0f)` generates `fmsubs` (fused multiply-subtract)
- `(1.0f - x*y)` generates `fnmsubs` (fused negate multiply-subtract)

### Fix

```cpp
// Before (generates fmsubs - wrong)
float targetAsp = widescreen ? 0.75f : 0.5625f;
float v1x = v1y + (targetAsp * realAspect - 1.0f) * 0.5f;

// After (generates fnmsubs - correct)
float targetAspect;
if (widescreen) {
    targetAspect = 0.75f;
} else {
    targetAspect = 0.5625f;
}
// fnmsubs requires: 1.0f - (x*y)
float v1x = v1y + (1.0f - targetAspect * realAspect) * 0.5f;
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| DxRnd::DrawSafeArea | 98.8% | 100% | +1.2% | Removed temp, `1.0f - targetAspect * realAspect` |
| InterpVector | 99.31% | 100% | +0.69% | `tmp0 = fcubed - fsq * 2.0f` for fnmsubs |
| RndLine::GetDistanceToPlane | 96.48% | 100% | +3.52% | Dot product split into t1, t2, t3 temps |

### Rule

- `(1.0f - x*y)` → `fnmsubs`
- `(x*y - 1.0f)` → `fmsubs`

Check which instruction the original uses and restructure accordingly.

---

## Operator Overload Selection

**Impact:** +1-2%
**Success Rate:** 100%
**Time:** 2 minutes

Use the correct operator to invoke the intended overload.

### Symptom

objdiff shows call to wrong overload (e.g., BinStream vs BinStreamRev).

### Fix

```cpp
// Before - calls BinStream::operator>>
d.stream >> mCrowds;

// After - calls BinStreamRev::operator>>
d >> mCrowds;
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| TransformArea::Load | 98.6% | 100% | +1.4% | `d >> mCrowds` not `d.stream >> mCrowds` |

---

## Inline Assignment

**Impact:** +1-2%
**Success Rate:** 95%
**Time:** 2 minutes

Inline assignment expression directly into function call.

### Symptom

objdiff shows different register allocation around function calls.

### Why It Works

Eliminates intermediate register assignment, changes allocation sequence.

### Fix

```cpp
// Before - separate assignment
unk300 = mtx;
Invert(unk300, unk340);

// After - inline assignment
Invert(unk300 = mtx, unk340);
```

Also works for function arguments:

```cpp
// Before - separate assignment and call
era = pEra->GetName();
CampaignEraProgress *progress = GetEraProgress(era);

// After - assignment within call (matches stw instruction pattern)
CampaignEraProgress *progress = GetEraProgress(era = pEra->GetName());
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| RndCam::SetViewProj | 98.04% | 100% | +1.96% | `Invert(unk300 = mtx, unk340)` |

---

## Boolean Index

**Impact:** Variable
**Success Rate:** MEDIUM
**Time:** 5 minutes

Use arithmetic instead of comparison for boolean-to-index conversion.

### Symptom

objdiff shows `cntlzw` + `extrwi` instructions for boolean indexing.

### Fix

```cpp
// Before - generates cntlzw + extrwi instructions
label = mBAMColumns[side == 0]->Find<HamLabel>(...);

// After - simpler arithmetic
label = mBAMColumns[1 - side]->Find<HamLabel>(...);
```

---

## Bitwise Alignment

**Impact:** Variable
**Success Rate:** HIGH
**Time:** 5 minutes

Use bitwise formula instead of division for word-aligned calculations.

### Symptom

objdiff shows `clrrwi` (clear right bits) in target vs division in decomp.

### Why It Works

The compiler uses `clrrwi` for certain alignment patterns:
- `clrrwi r4, r11, 2` clears the bottom 2 bits (`& ~3`)

### Fix

```cpp
// Before - standard division formula, generates srawi + addze
FixedSizeAlloc((x + 15) / 4, ...)

// After - bitwise formula, generates srawi + clrrwi
FixedSizeAlloc(((x + 15) >> 2) & ~3, ...)
```

---

## Dot Product Component Order

**Impact:** Variable
**Success Rate:** LOW
**Time:** 10 minutes

The order of arithmetic components can affect register allocation.

### Symptom

objdiff shows `fmuls`/`fadds` in different order.

### Fix

Try reordering components:

```cpp
// Standard order
x*q.x + y*q.y + z*q.z + w*q.w

// May match better
((w*q.w + x*q.x) + z*q.z) + y*q.y
```

### Warning

This pattern is highly context-dependent. Success rate is low, and the "correct" order varies by function.

---

## Comparison Operand Order

**Impact:** +2%
**Success Rate:** HIGH
**Time:** 2 minutes

Operand order affects which register becomes the comparison base.

### Symptom

objdiff shows register operands in different order for comparison.

### Fix

```cpp
// Before
return mBuffer.size() == mTell ? EofType(1) : EofType(0);

// After
return mTell == mSize ? EofType(1) : EofType(0);
```

### Why It Works

PowerPC `cmpwi`/`cmplwi` instruction selection depends on operand ordering.

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| BufStream::Eof | 98.0% | 100% | +2.0% | `mTell == mSize` not `mSize == mTell` |

---

## Commutative Operand Order

**Impact:** +1-5%
**Success Rate:** 80%
**Time:** 5 minutes

Swap operand order in commutative operations to match the original.

### Symptom

objdiff detects `COMMUTATIVE_OP_ORDER` pattern with swapped source operands.

### Why It Works

For commutative operations (`add`, `fadd`, `mul`, `fmul`, `and`, `or`, `xor`), the result is mathematically identical, but the compiler may have chosen a specific operand order.

### Affected Instructions

- **Floating-point:** `fadd`, `fadds`, `fmul`, `fmuls`
- **Integer:** `add`, `addi`, `addis`, `and`, `andi.`, `andis.`, `or`, `ori`, `oris`, `xor`, `xori`, `xoris`

### Fix

```cpp
// Before - operands in wrong order
float result = a + b;

// After - swap operands
float result = b + a;
```

For operations with more than 2 operands:

```cpp
// Before
float result = x + y + z;

// After - try different groupings
float result = (y + x) + z;
float result = x + (z + y);
```

### Detection

objdiff shows `COMMUTATIVE_OP_ORDER` pattern with details like:
```
swapped_operands: [(r3, r4), (r5, r6)]
```

---

## Negation Splitting (fneg/frsp Scheduling)

**Impact:** +3-4%
**Success Rate:** HIGH
**Time:** 2 minutes

When a float function result is negated inline (`-func()`), the compiler may
generate `fneg` before `frsp` (float round to single). But the original code
may have done `frsp` then `fneg`. Splitting the negation into a separate
statement fixes the instruction order.

### Symptom

objdiff shows an insert/delete cluster around `fneg` and `frsp`:
```
insert: fneg f0, f1
insert: li   r11, 0x1
diff_arg: frsp f0, f1 → frsp f31, f0    (register + order change)
delete: li   r11, 0x1
delete: fneg f31, f0
```

The key tell: `frsp` and `fneg` are present in both target and base, but in
opposite order.

### Why It Works

`-func()` folds the negation into the double-precision result before rounding:
```
bl    func        ; returns double in f1
fneg  f0, f1      ; negate double
frsp  f31, f0     ; round to single
```

Splitting the negation forces round-then-negate:
```
bl    func        ; returns double in f1
frsp  f0, f1      ; round to single first
fneg  f31, f0     ; then negate single
```

### Fix

```cpp
// Before (generates fneg before frsp — wrong order)
float angle = -acos(Dot(dir1, dir2));

// After (generates frsp then fneg — matches target)
float angle = acos(Dot(dir1, dir2));
angle = -angle;
```

### Real Example

`HamSkeletonConverter::CalcRotzBone`: 96.1% → 99.9% with this single change.

### Automation

A permuter pattern `negation_split` could detect this by looking for
`-func()` expressions and splitting them into `f = func(); f = -f;`.

---

## Byte Mask Extraction (rlwimi)

**Impact:** +5-30%
**Success Rate:** HIGH
**Time:** 5 minutes

Extract byte-mask expressions to named variables to break rlwimi recognition.

### Symptom

objdiff shows `rlwimi` (rotate left word immediate then mask insert) in target vs `clrlwi + slwi + or` (separate shift/mask/or) in our build, or vice versa.

### Why It Works

MSVC PPC recognizes combined rotate-mask-insert patterns like `u8(w) | ((w << 8) & 0xFF00)` and emits a single `rlwimi` instruction. When the target uses separate instructions instead, extracting the byte mask to a named variable breaks the compiler's pattern recognition, forcing it to fall back to individual operations.

### Fix

```cpp
// Before (generates rlwimi — wrong)
unsigned long ret = u8(w) | ((w << 8) & 0xFF00);

// After (generates clrlwi + slwi + or — matches target)
unsigned long bw = u8(w);
unsigned long ret = bw | (bw << 8);
```

By computing `u8(w)` into a named variable first, the compiler no longer sees the combined rotate-mask-insert pattern.

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| ByteGrinder::op2 | ~93% | 100% | +7% | rlwimi + arg read order |
| ByteGrinder::op3 | ~93.6% | 100% | +6.4% | rlwimi + arg read order |
| ByteGrinder::op10 | 71.3% | 100% | +28.7% | rlwimi + XOR pre-mask |
| ByteGrinder::op11 | 77.2% | 100% | +22.8% | rlwimi + XOR pre-mask |
| ByteGrinder::op12 | 93.3% | 100% | +6.7% | rlwimi only |
| ByteGrinder::op13 | 93.2% | 100% | +6.8% | rlwimi only |

### Rule

When you see `rlwimi` mismatches in bitwise byte-manipulation code:
1. Find the byte-mask expression (`u8(x)`, `(unsigned char)(x)`, `x & 0xFF`)
2. Extract it to a named `unsigned long` variable
3. Use the variable in place of the original expression

### Automation

The `byte_mask_extraction` permuter pattern (opt-in) automates this transformation.

---

## NOR Peephole Prevention (u32 Widening)

**Impact:** +1-14%
**Success Rate:** HIGH
**Time:** 5 minutes

Widen narrow-type values to u32 before XOR to prevent the compiler from using NOR.

### Symptom

objdiff shows `nor` in target vs `xori` in our build, or vice versa, around XOR-with-all-ones patterns.

### Why It Works

The compiler converts `u8_value ^ all_ones_mask` into a bitwise NOT (`nor` instruction) when the XOR mask covers all bits of the operand type. For example, `(u8)(w >> 3) ^ 0x1F` on a 5-bit result, or `u8_w ^ 0xFF` on an 8-bit value.

By widening to u32 first, the mask no longer covers all 32 bits, so the compiler can't apply the NOR optimization.

### Fix

```cpp
// Before (generates nor — compiler sees XOR-with-all-ones)
u8 w = msg->Int(2);
u32 tmp = (u8)(w >> 3) ^ 0x1F;

// After (generates extrwi + xori — matches target)
u8 w = msg->Int(2);
u32 w32 = w;
u32 tmp = (w32 >> 3) ^ 0x1F;
```

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| ByteGrinder::op32 | 95.9% | 100% | +4.1% | u32 widening before XOR |
| ByteGrinder::op60 | 86.7% | 100% | +13.3% | u32 widening before XOR |
| ByteGrinder::op63 | 86.7% | 100% | +13.3% | u32 widening before XOR |

### Rule

When you see `nor` mismatches in bitwise code:
1. Check if a narrow type (u8, u16) is being XOR'd with a mask that covers all its bits
2. Widen the value to u32 before the XOR
3. Remove explicit narrow-type casts on intermediate values

---

## u8 Intermediate Variables for Shift Scheduling

**Impact:** +0.7-1%
**Success Rate:** MEDIUM
**Time:** 10 minutes

Use u8 intermediate variables to force instruction scheduling order for independent shift operations.

### Symptom

objdiff shows `extrwi`/`clrlslwi` (shift-right/shift-left) instructions in swapped order. Same instructions, different sequence.

### Why It Works

The compiler schedules independent shift-right and shift-left operations in a fixed internal order. When the target uses the opposite order, creating `u8` intermediate variables for each half creates data dependencies (via truncation) that force the compiler's hand.

### Fix

```cpp
// Before (compiler schedules freely — wrong order)
u32 tmp = ((u8)(w >> 3) ^ 6) | ((w & 7) << 5);

// After (u8 intermediates force extrwi-first scheduling)
u32 w32 = w;
u8 tmp1 = u8((w32 >> 3) ^ 6);    // shift-right half first
u8 tmp2 = u8(((w32 & 7) << 5));   // shift-left half second
u8 combined = u8(tmp1 | tmp2);
```

### Key Details

- Always put shift-right part as the first declaration — compiler schedules `extrwi` first
- The `u8()` cast on each intermediate forces completion before moving on
- May need to combine with XOR constant pairing correction (see session doc)

### Real Examples

| Function | Before | After | Delta | Notes |
|----------|--------|-------|-------|-------|
| ByteGrinder::op45 | 99.3% | 100% | +0.7% | u8 intermediates |
| ByteGrinder::op46 | 99.3% | 100% | +0.7% | u8 intermediates |
| ByteGrinder::op47 | 99.3% | 100% | +0.7% | u8 intermediates |
| ByteGrinder::op50 | 99.0% | 100% | +1.0% | u8 intermediates + XOR constant swap |
| ByteGrinder::op52 | 99.0% | 100% | +1.0% | u8 intermediates + XOR constant swap |
| ByteGrinder::op55 | 99.0% | 100% | +1.0% | u8 intermediates + XOR constant swap |
| ByteGrinder::op56 | 99.0% | 100% | +1.0% | u8 intermediates + XOR constant swap |

### Warning

This pattern is specific to bit-rotation-with-XOR code. Two functions remain unfixable:
- **op14**: Pure `srwi`/`slwi` swap with no XOR constants to manipulate
- **op61**: Scheduling and XOR constants are coupled — fixing one breaks the other

---

## See Also

- [fixable-control-flow.md](fixable-control-flow.md) - Branch structure patterns
- [unfixable-compiler.md](unfixable-compiler.md#fmadds-vs-separate-ops) - When FMA fixes don't work
