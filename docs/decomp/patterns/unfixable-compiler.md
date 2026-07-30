# Hard Patterns: Compiler

These patterns are caused by compiler optimizations or heuristics that resist simple source-level changes. Each section documents what would be needed to fix them.

**Action:** Confirm the pattern actually applies (and that no fixable issues are mixed in). Try the documented fixes before moving on. For patterns requiring c2.dll patching, see the DC3-side writeup at `../dc3-decomp/docs/plans/compiler-instrumentation.md` (the GDB-tracing/register-coloring experiments this suite was ported from — not copied into this repo). rb3-xenon's own `docs/plans/instrumentation-patcher-experiment.md` covers a related but distinct experiment (an obj-patcher for coverage-stub functions; verdict: dead end).

---

> ## ⚠ STATUS (2026-07-30, lane CB-3): THIS DOC WAS WRITTEN AGAINST THE WRONG COMPILER
>
> **This file is `dc3-decomp`'s document, imported verbatim (commit `d97d0985`).**
> DC3 retail was built with X360 `cl.exe` build **16.00.11886.00** — which was also
> *our* fleet default until 2026-07-30. **RB3 retail was built with build
> `16.00.10224.00`** (XDK 2.0.11164; identified from the Rich header, see
> `scripts/harvest/rich_header.py`). Commit `f149a4b7` flipped the fleet default to
> 10224 (+26 matched functions, 0 losses).
>
> So this doc was **authored in a project where 11886 is the correct compiler and
> imported into one where it is the wrong one.** Nothing in the original text
> signalled that change of frame — it never names a compiler build at all. Every
> verdict below silently carried DC3's toolchain assumption across the import.
>
> **Which claims are suspect, and which are not:**
> - ⚠ **Instruction-selection verdicts are the SUSPECT class.** Which opcode the
>   compiler picks for a given source construct is exactly what differs between
>   compiler builds. Any "unfixable" verdict of the form *"target emits X, we emit
>   Y"* may simply have been the wrong compiler and must be re-measured before it is
>   trusted.
> - ✅ **Register-allocation, instruction-scheduling and ICF *mechanisms* remain
>   universal.** The BSF-coloring model, the interference-graph reasoning, and the
>   ICF/COMDAT-folding sections describe how the allocator works, not which opcode
>   it chose; they survive the compiler change. (Their *per-function verdicts* may
>   still have moved — the mechanism is sound, the AT_LIMIT labels are stale.)
>
> **Measured under 10224 (lane CB-3), previously listed here as unfixable:**
> `SfxInst::IsRunning` → **100.0%** · `StreamReceiver360::Poll` → **100.0%** ·
> `ChunkStream::PollDecompressionWorker` → **100.0%** · `FastInvert` (listed
> AT_LIMIT) → **100.0%**. Line ~248's claim that the compiler versions *"differ
> only in tie-breaking … not in fundamental allocation strategy"* is **directly
> falsified** (the flip moved 48 functions on zero source changes).
>
> ⚠★★ **ATTRIBUTION CORRECTION (coordinator, 2026-07-30) — these four are NOT
> the flip's doing.** They are at 100.0% today, but **none of them appears
> anywhere in the flip's 48-function delta** (26 gained / 19 improved / 3
> regressed — checked against lane CB-3's own `cb3_delta.json` and corroborated by
> lane CB-2's independent 26-gainer census, whose units are BandCharacter,
> inflate, BinStream, DirLoader, CharUtl, vorbis `info`, deflate, SHA1, DataFunc,
> obj/Dir, synth/Synth, OvershellPanel, printbuf, BandDirector, Sorting,
> SongParser, rndobj/Utl, MicNull — no Sfx, no StreamReceiver360, no ChunkStream,
> no FastInvert). They were closed by **earlier body-port work**, which means these
> rows had been stale for some time *before* the flip.
> ★ **The doc's claims are falsified either way — but "stale for months" and
> "fixed by the compiler" are different lessons**, and only the second one would
> tempt a future lane to expect a toolchain change to clear regswap walls. It
> won't: CB-3 measured **0 of 2,576 AT_LIMIT rows** reaching 100 via the flip.
>
> **Two cited claims SURVIVED re-measurement** — this doc is not wrong in only one
> direction: `StreamReceiver360::Tag` **70.166%** and `SpotlightDrawer::Init`
> **94.107%** are still short, and their sections still describe real walls.
>
> **⚠ 12 of the 21 functions cited in this doc do not exist in the RB3 binary at
> all** — several are Dance Central code that never shipped in RB3
> (`HamPhotoDisplay::SyncProperty`, `SkeletonChooser::ShouldWaitForRecovery`,
> `MoveCandidate::Adjacency`). Treat every example table here as
> *possibly-about-a-different-game* until the symbol is confirmed present.
>
> **⚠ The c2.dll RVA `0x026780` cited throughout indexes 11886's DLL.** It is not a
> valid offset for our current 10224 toolchain and must be re-derived before any
> binary-patching work.
>
> Statistics in this file are unsourced and undated, and have been wrong before (a
> stale "80% success rate" claim was corrected earlier). Treat all percentages as
> suspect.

---

## ASSERT_REVS / INIT_REVS Scheduling

**Prevalence:** Functions with ASSERT_REVS or INIT_REVS macro (~10%)
**Typical Gap:** ~0.8-0.9%

Instruction scheduling differs around revision assertion macros.

### Symptom

Same instructions, different order around assert code. Also manifests as `subi 4` vs `addi 4` for the `gRevs` pair base pointer.

### Root Cause

Two related issues:

1. **Instruction scheduling**: The compiler schedules instructions differently:
   - Target computes `gRevs[2]` address before stack variables
   - Our build computes stack variables before `gRevs[2]`
   - Same instructions, different order — compiler scheduling heuristic

2. **Base pointer choice**: For the `gRevs` pair (two consecutive globals), the target compiler sometimes uses `base = &gRevs[1]; access gRevs[0] via base-4` (`subi 4`) while our compiler uses `base = &gRevs[0]; access gRevs[1] via base+4` (`addi 4`). Both access the same memory, but the instruction encoding differs.

### Detection

- Instruction count matches but order differs around assert code in second MILO_FAIL call
- `subi r11, r11, 0x4` vs `addi r11, r11, 0x4` near `gRevs` references

### What Would Fix It

- c2.dll instruction scheduler patch to match the original compiler's scheduling priority
- All Load functions with ASSERT_REVS/INIT_REVS will have ~0.8-0.9% gap from this alone

### Real Examples

| Function | Match | Gap | Notes |
|----------|-------|-----|-------|
| RndShockwave::Load | 97.7% | ~2.3% | `subi 4` vs `addi 4` for gRevs pair |
| Most ::Load functions | 99-99.2% | 0.8-0.9% | Scheduling noise only |

---

## fmadds vs Separate Ops

**Prevalence:** Functions with float math
**Typical Gap:** 1-3%

Compiler chooses fused vs separate floating-point operations.

### Symptom

`fmadds` in target vs `fmuls` + `fadds` in our build (or vice versa).

### Example

```asm
# Original (fused multiply-add)
fmadds f0, f11, f11, f0

# Our build (separate multiply and add)
fmuls f11, f11, f11
fadds f0, f0, f11
```

### Try These Fixes First

Before accepting this gap, try the source-level fixes documented in **[fixable-fsel-fma.md](fixable-fsel-fma.md#fma-control-via-pragma-fp_contract)**:

1. **`#pragma fp_contract(off/on)`** — controls FMA fusion at file scope
2. **Expression restructuring** — bring multiply and add adjacent for fusion
3. **FMA expression order** — `(1.0f - x*y)` generates `fnmsubs`, `(x*y - 1.0f)` generates `fmsubs`

### When Source-Level Fixes Don't Work

**Mixed-direction FMA**: Same function needs both ON and OFF. Source-level options:
- Split into helper functions with different pragma settings
- Use `volatile float` intermediates to selectively block fusion
- c2.dll binary patch to match per-expression fusion heuristic

**Pragma ignored**: Compiler overrides `#pragma fp_contract(off)` under `/O1` for some expressions. Options:
- `volatile` intermediate variable to force separation
- c2.dll binary patch to respect the pragma unconditionally

---

## fsel Register Pressure

**Prevalence:** Functions with float clamping/min/max
**Typical Gap:** 5-20%

Using `__fsel` intrinsic or `Clamp<float>` template generates correct `fsel` instructions but increases FPR register pressure.

### Symptom

After adding fsel-based code, the prologue changes from 2 callee-saved FPRs to 4, cascading stack offsets throughout the function.

### Root Cause

`fsel` keeps intermediate float values alive longer (no branch to break live ranges), so the register allocator spills more to callee-saved FPRs. The original compiler's allocator handles this with fewer registers.

### What Would Fix It

- Restructure surrounding code to reduce live float variables before/after the fsel
- c2.dll register allocator binary patch (see [Register Allocation](#register-allocation) for mechanism)
- Find an alternative expression form that achieves fsel with fewer simultaneously-live floats

### Real Example

| Function | fsel Match | Branch Match | Issue |
|----------|-----------|--------------|-------|
| DebugGraph::Draw | Worse (more FPRs) | 46.8% | Clamp used f28-f31 (4 FPRs) vs target's f30-f31 (2 FPRs) |

---

## Register Allocation

**Prevalence:** ~250 AT_LIMIT functions with REGISTER_SWAP as dominant blocker
**Typical Gap:** 1-3%
**Status:** Mechanism fully understood (Experiments 1-9). Source-level fixes work ~30% of the time. Binary patching of c2.dll coloring loop is a viable path to fix the remaining 70%.

### Symptom

Consistent register swaps (e.g., r30 vs r31, f30 vs f31) throughout function.

### Common Swaps

- r10 ↔ r11 (volatile GPR)
- r27 ↔ r28 (callee-saved GPR)
- r30 ↔ r31 (callee-saved GPR)
- f30 ↔ f31 (callee-saved FPR)

### Detection

All mismatches are `diff_arg` with register operands swapped:

```
| fmr f31, f1 | fmr f30, f1 |
| fmr f30, f1 | fmr f31, f1 |
```

**Finding register swap functions in decomp.db:**
```sql
SELECT symbol, current_percent
FROM functions
WHERE primary_pattern = 'REGISTER_SWAP'
  AND excluded = 0
ORDER BY current_percent DESC;
```

### Root Cause: c2.dll Register Allocator Mechanism

**Fully characterized via GDB tracing of c2.dll** (Experiments 1-9 in `../dc3-decomp/docs/plans/compiler-instrumentation.md` — not present in this repo; same compiler/toolchain, so the mechanism transfers verbatim):

The MSVC Xbox 360 backend (c2.dll) uses graph-coloring register allocation:

1. **Interference graph building**: Each live range becomes a node. Nodes that overlap get interference edges.
2. **BSF-based coloring** (at c2.dll RVA `0x026780` — ⚠ **this RVA indexes build 11886's c2.dll, NOT the 10224 DLL we now compile with**; re-derive before use, lane CB-3 2026-07-30): The allocator iterates variables by **symbol ID** (which follows declaration order in source). For each variable, it uses x86 `BSF` (Bit Scan Forward) on a bitmask of available colors to find the lowest-numbered free color.
3. **Color→Register mapping**: Colors map to PPC registers with direction depending on register class:
   - **Volatile GPR**: top-down (first color → r11, next → r10)
   - **Callee-saved GPR**: bottom-up (first color → r29, next → r30, r31)
   - **FPR**: follows similar pattern

**Key insight**: Each variable gets a **deterministic color** based on interference constraints — colors are consistent regardless of declaration order. But the **color→register mapping** depends on allocation ORDER (= declaration order). Swapping declaration order of two variables swaps which color maps to which register, but the colors themselves don't change.

This is why:
- **Source reordering works ~30% of the time**: When interference constraints allow, reordering declarations changes the color→register mapping to match the target.
- **Source reordering fails ~70% of the time**: When interference constraints force the same colors regardless of order, or when the correct mapping requires a specific symbol ID sequence that doesn't correspond to any valid declaration order.

### Evidence

| Experiment | Test | Finding |
|-----------|------|---------|
| Exp 1-3 | swap_a vs swap_b (volatile) | Declaration order determines r10↔r11 assignment |
| Exp 4-5 | callee_a vs callee_b (saved) | Declaration order determines r29↔r31 assignment |
| Exp 6-7 | callgrind-diff on BSF | Identical instruction traces except divergent BSF calls |
| Exp 8 | Full BSF trace (389 calls) | Only 6 of 389 BSF calls differ; colors consistent, mapping changes |

### Variable Reordering Heuristics

When REGISTER_SWAP is detected, try these strategies (~30% success rate overall):

**1. Group by usage pattern**
Variables used together should be declared together:
```cpp
// Before - scattered declarations
float x = GetX();
int count = 0;
float y = GetY();  // Used with x but declared far apart

// After - grouped by usage
float x = GetX();
float y = GetY();  // Now adjacent to x
int count = 0;
```

**2. Order by first use**
Declare variables in the order they're first read:
```cpp
// If function does: read a, read b, read c, write a
// Declare in order: a, b, c
```

**3. Separate integer and float declarations**
The compiler allocates GPRs and FPRs from separate pools:
```cpp
// Before - interleaved
int a; float f1; int b; float f2;

// After - grouped by type
int a; int b;
float f1; float f2;
```

**4. Try reverse order**
Callee-saved allocates bottom-up, volatile top-down:
```cpp
// If nothing else works, try reversing declaration order
float z, y, x;  // Instead of x, y, z
```

### What To Do

Try [Variable Declaration Order](fixable-declarations.md#variable-declaration-order) with the heuristics above. If 10+ reordering attempts don't help, the register assignment is fixed by interference constraints.

**Future**: Binary patching of c2.dll's coloring loop (RVA `0x026780`) could reverse the BSF scan direction or reorder the color assignment, fixing all register swap functions at once.

> ⚠ **STALE ADDRESS (lane CB-3, 2026-07-30).** `0x026780` was derived against build
> **11886**'s c2.dll (DC3's toolchain, and our former default). We now compile with
> build **10224**, whose c2.dll is a different binary — the offset is **not valid**
> and must be re-derived before any patching attempt. Note also that some of the
> register-swap functions this plan was meant to fix (e.g. `FastInvert`) have since
> reached 100% — ⚠ **not via the compiler flip** (`FastInvert` is absent from its
> 48-function delta), but by earlier body-port work — so re-scope the target
> population first. See `../dc3-decomp/docs/plans/compiler-instrumentation.md` for the full mechanism and address map (not present in this repo).

### Statistical Analysis (1,288 functions scanned)

Analysis of cached objdiff results reveals the scope of callee-saved register swaps:

**Most common adjacent swap pairs** (by function count):
| Pair | Functions |
|------|----------|
| r27↔r28 | 44 |
| r29↔r30 | 43 |
| r28↔r29 | 40 |
| r26↔r27 | 26 |
| r30↔r31 | 24 |
| f30↔f31 | 16 |

Adjacent register pairs dominate (85%+ of all callee-saved swaps), confirming the compiler versions differ only in tie-breaking within the coloring loop, not in fundamental allocation strategy.

> ⚠ **FALSIFIED (2026-07-30, lane CB-3).** This sentence was written comparing DC3's
> build against build 11886. RB3 retail is build **10224**, and switching to it moved
> 48 functions with **zero source changes** (26 of them all the way to an exact
> match — e.g. `BinStream::Read`, `CSHA1::Final`, `ObjectDir::SetPathName`). The
> compiler builds therefore differ by **more** than coloring tie-breaks. The BSF
> *mechanism* described above still stands; the inference "therefore only tie-breaking
> differs" does not.

**Variable classification of swapped entities** (173 single-pair swap functions):
| Category | Count | % | Description |
|----------|-------|---|-------------|
| Both compiler-internal | 49 | 28% | addr_compute×addr_compute, member_load×member_load — no user-declared variable to reorder |
| One user + one temp | 104 | 60% | param_save×addr_compute — can't reorder a parameter against a compiler temp |
| Both user-controllable | 20 | 12% | param_save×param_save — only these are candidates for declaration reorder |

**Key finding**: Only ~12% of callee-saved swaps involve two user-declared variables. The other 88% swap a parameter or local against a compiler-generated temporary (member loads, address computations, vtable pointers). This is why declaration reorder has a ~30% success rate overall — it can only help the 12% where both entities are reorderable, and even then interference constraints may prevent it.

**No consistent allocation order**: Definition order analysis shows 50/50 split between target-allocates-ascending vs target-allocates-descending. The allocation depends on interference graph structure, not a simple ordering heuristic.

### Real Example

| Function | Match | Attempts | Result |
|----------|-------|----------|--------|
| FastInvert | ~~99.45%~~ **100.0%** | 10+ | ✅ **No longer AT_LIMIT** (re-measured 2026-07-30). ⚠ **NOT the compiler flip** — absent from its 48-function delta; closed by earlier body-port work, so this row was already stale before the flip |
| CharBonesMeshes::PoseMeshes | 99.24% | 5+ | AT_LIMIT (r10/r9, r28/r30) |
| DxTex::ResetSurfaces | 98.4% | verified | AT_LIMIT (r28/r29 swap after extrwi fix) |

---

## Boolean Negation: subfic vs subic

**Prevalence:** Functions that negate a pointer-to-bool conversion
**Typical Gap:** ~3-8%

Different code generation for `!x` depending on whether `x` is a `bool` or a pointer type.

### Symptom

objdiff shows `subfic r11, r3, 0x0` in the target vs `subic r11, r3, 0x1` in our build, followed by different `subfe` patterns:

```asm
# Target — pointer negation (3 instructions)
subfic  r11, r3, 0x0         ; r11 = 0 - r3, CA = (r3 == 0)
subfe   r11, r11, r11        ; r11 = (r3 != 0) ? -1 : 0
and     r28, r11, r28        ; mask previous value

# Our build — bool negation (2 instructions)
subic   r11, r3, 0x1         ; r11 = r3 - 1, CA = (r3 >= 1)
subfe   r28, r11, r3         ; r28 = normalized bool
```

### Root Cause

When a function stores a pointer return value into a `bool` before negating it, MSVC generates the `subic` (bool-specific) pattern. When the value is kept as a pointer and negated, it generates the `subfic` (general integer/pointer) pattern. Both compute `!x` correctly but produce different instruction sequences.

The target code likely kept the pointer type:
```cpp
// Target pattern — pointer negation generates subfic
Skeleton *skel0 = (Skeleton *)1;  // non-null sentinel
if (id > 0) skel0 = GetSkeletonByTrackingID(id);
if (!skel0) ...  // subfic: 0 - ptr

// Our pattern — bool negation generates subic
bool skel0 = true;
if (id > 0) skel0 = GetSkeletonByTrackingID(id);  // implicit ptr->bool
if (!skel0) ...  // subic: bool - 1
```

### Why It's Unfixable

Changing the variable type from `bool` to pointer changes the entire code generation pattern — not just the negation. The compiler makes different choices for initialization, conditional branches, and register allocation with pointer types vs bools. In practice, switching to pointer type often makes things worse overall even though it fixes the specific `subfic`/`subic` mismatch.

### Real Examples

| Function | Match | Pattern | Notes |
|----------|-------|---------|-------|
| ~~SkeletonChooser::ShouldWaitForRecovery~~ | n/a | 2x subfic/subic + regswap | ⚠ **NOT PRESENT IN THE RB3 BINARY** — Dance Central code, inherited with this doc from dc3-decomp (lane CB-3, 2026-07-30) |

---

## Commutative Register Swap

**Prevalence:** Float operations
**Typical Gap:** <1%

Commutative operation with swapped operand registers.

### Symptom

Same operation, operands in different order:

```
| fmuls f11, f0, f13 | fmuls f11, f13, f0 |
```

### Root Cause

Same mathematical result, different register order. The compiler's expression evaluator picks a canonical operand order.

### What Would Fix It

- c2.dll expression evaluator patch to reverse operand canonicalization for commutative FP ops
- Sometimes swapping the source-level operand order helps (see [fixable-operators.md](fixable-operators.md#commutative-operand-order))

### Source-Level Operand Reordering Is Almost Always A No-Op (Wave 1 / F1)

For >99% of single-instruction `COMMUTATIVE_OP_ORDER` mismatches (`fmadds`, `fmuls`, `fadds`, `xor`, `add`, `mullw`, etc.), rewriting source-level expression order — e.g. `(a + b)` → `(b + a)`, or swapping operand order in an `__fsel`/`Min`/`Max` call — does **not** change the emitted assembly. The MSVC backend picks operand registers based on liveness and the register allocator's color→register mapping at the point of the op, not on the syntactic order in the AST.

Wave 1 of the AT_LIMIT audit confirmed this across dozens of attempts: agents kept rewriting `(b + a)` vs `(a + b)`, `(x * y) - z` vs `z - (x * y)` (only legal for true commutative ops), etc., and the resulting `.obj` was byte-identical to the previous build in every case. Skip this rewrite — it is a wasted cycle. The actionable lever for a `COMMUTATIVE_OP_ORDER` mismatch is **upstream**: reorder the variables that feed the op so the register allocator assigns the operands to the registers the target chose. See [Register Allocation](#register-allocation) — the swap is a regalloc artifact, not an expression-evaluator artifact.

---

## 64-bit Extraction

**Prevalence:** Rare
**Typical Gap:** ~5%

Different extraction methods for 64-bit to 16-bit conversions.

### Symptom

Target uses `lhz` (load halfword) vs our `ld` + bit masking.

### Root Cause

Compiler optimization choice for how to extract 16-bit slices from 64-bit values.

### What Would Fix It

- c2.dll codegen patch to prefer `lhz` (load halfword) over `ld` + bit masking for 16-bit extracts from 64-bit values

---

## Branch Offsets

**Prevalence:** Common
**Typical Gap:** 0%

Branch target addresses differ due to code layout.

### Symptom

Branch instructions have different immediate offset values.

### Root Cause

Branch offsets are calculated based on code layout. Different instruction placement = different offsets.

### Impact

These don't affect match percentage in objdiff scoring. No fix needed.

---

## Stack Spill Scheduling

**Prevalence:** Functions with high register pressure
**Typical Gap:** ~1-2%

The target binary spills a local variable to the stack frame, but our code keeps it in a register.

### Symptom

objdiff shows 1-3 `delete` instructions that are all `stw rN, offset(rFP)` to the stack frame. The stored register contains a local variable that's used later in the function.

### Root Cause

Stack spill decisions are made by the register allocator based on register pressure, estimated spill/reload cost, and scheduling heuristics.

### Detection

- 1-3 `delete` instructions, all `stw` to stack frame offsets
- The function is otherwise very close (97%+)
- Removing/adding code doesn't change the spill pattern

### What Would Fix It

- Restructure code to reduce register pressure in the hot region (reduce live variables)
- c2.dll spill heuristic patch to match the original compiler's spill/reload cost model

### Real Example

| Function | Match | Gap | Notes |
|----------|-------|-----|-------|
| PhysicsManager::HarvestCollidables | 97.4% | ~2.6% | Target spills `owner` to stack 0x54 twice |
| ObjPtrVec\<T\>::sort instantiations | ~99% | <1% | Target spills `size()` to stack 0x54, never reloads it; single `stw r5, 0x54, r31` delete around index ~21 (Wave 1 / F5) |

### Sub-Pattern: ObjPtrVec<T>::sort `stw r5, 0x54, r31`

Several `ObjPtrVec<T>::sort` template instantiations show a single-instruction delete at roughly index 21 where the target emits `stw r5, 0x54, r31` (spilling the result of `size()` into stack slot `0x54`) and then never reads that slot again. The function is otherwise 99%+. This is the same regalloc/spill-heuristic pattern as the parent section — the target compiler defensively spills the size around the recursive `__introsort_loop` call boundary, our compiler proves the value dead-on-return. No source-level workaround; not worth chasing. See [Real Examples](#real-example-1) above for the table entry.

---

## OFFSET_SWAP Auto-Classifier False Positives (Wave 1 / F3)

**Status:** Not a real pattern — auto-classifier noise. Don't chase these without first verifying they're a real struct-member swap.

The `primary_pattern='OFFSET_SWAP'` classification fires on three categories of mismatch that look syntactically like an offset swap but are NOT what the "real" OFFSET_SWAP fix (field reorder / variable extraction; see [fixable-declarations.md](fixable-declarations.md#offset-swap)) addresses:

### False Positive 1 — Stack-Local (r1-Based) Offset Swap

Paired `stw r?, offN, r1` / `stw r?, offM, r1` where the swapped offsets live on the stack frame. This is **scheduling/spill ordering**, not field layout. Source-level field reorder cannot move stack slots — the regalloc and frame-builder pick those.

### False Positive 2 — Inlined memcpy/memset Word-Store Ordering

The MSVC backend inlines small `memcpy` / `memset` / aggregate-init / POD-copy as a sequence of `lwz`/`stw` pairs at increasing word offsets. When the target's scheduler interleaves the loads and stores differently from ours, the diff produces a string of swapped offsets — but the offsets are byte positions inside a flat memory copy, not struct fields.

### False Positive 3 — Adjacent-Field Write-Order Picked Independently By The Scheduler

When two adjacent struct fields are written in the same statement (or by a constructor's member-init list), the compiler is free to schedule the two stores in either order regardless of source order. The instruction scheduler reorders these based on register pressure at the moment of emit, not on the AST. Rewriting the constructor's init order, splitting the writes across statements, or adding a `volatile` barrier does not reliably move the schedule.

### Detection — Distinguishing Real From False

A **real** OFFSET_SWAP from struct-member layout has paired `lwz`/`stw`/`stfs`/etc. instructions where:

- The **base register is the `this` pointer** (typically `r3` for `this` on entry, `r4` for `that` in `operator=`, or a callee-saved like `r30`/`r31`) — NOT `r1` and NOT a scratch register holding a temporary.
- The two offsets correspond to **actual struct field offsets** that you can find in the class definition.
- Swapping the source-level field declaration order in the class definition is a meaningful (if usually unfixable) lever.

If the base register is `r1`, or if the offsets are inside a memcpy/memset region, or if the offsets don't correspond to declared fields, the auto-classifier mis-fired. Skip the function or re-classify before spending agent cycles on it.

### Why It Matters

Wave 1 of the AT_LIMIT audit had multiple agents try field-reorder and variable-extraction fixes on functions classified as OFFSET_SWAP, with no movement, because the underlying pattern was one of the three false positives above. The classifier should be tightened to check the base register before tagging.

---

## Store-then-Reload Scheduling

**Prevalence:** Functions that store to a global and immediately use the value
**Typical Gap:** 0.5-1%

The target binary stores a value to a global variable, then reloads it from memory for a subsequent call. Our compiler keeps the value in a register.

### Symptom

objdiff shows 1-2 extra `stw` + `lwz` instructions in the target that store a register to a global address and immediately reload it. Our build skips the store-then-reload and passes the register directly.

### Root Cause

The target compiler's instruction scheduler inserts a store-then-reload sequence as a scheduling fence or to satisfy aliasing constraints. Our compiler optimizes this away, keeping the value in a register. This is a fundamental scheduling heuristic difference.

### Detection

Look for patterns like:
```asm
# Target (has store-then-reload)
stw  r3, sDefault@l(r11)    ; store to global
lwz  r3, sDefault@l(r11)    ; reload immediately
bl   Select                   ; call with reloaded value

# Our build (optimized away)
stw  r3, sDefault@l(r11)    ; store to global
bl   Select                   ; call with original register
```

### What Would Fix It

- c2.dll instruction scheduler patch
- Source-level reordering does NOT fix this — tested moving the global store before/after the call, using the global directly (`sDefault->Select()` instead of `ptr->Select()`) — all attempts either don't change the pattern or make things worse

### Real Examples

| Function | Match | Gap | Notes |
|----------|-------|-----|-------|
| SpotlightDrawer::Init | **94.107%** ✅ still a wall | ~6% | `sDefault = ptr; ptr->Select()` — target reloads sDefault for Select call. ✅ **SURVIVED re-measurement under 10224** (lane CB-3, 2026-07-30). |

---

## Address Relocation Noise

**Prevalence:** ~150 AT_LIMIT functions (dominant pattern in final triage)
**Typical Gap:** 0.5-2%

Functions that reference many globals show `diff_arg` on `lis`/`addi`/`lwz` instruction pairs due to different global variable addresses between builds.

### Symptom

objdiff shows many `diff_arg` mismatches where both sides have identical instructions but with different immediate operand values for `lis` (load immediate shifted) and `addi`/`lwz`/`stw` with address low-half operands. These appear in pairs:

```asm
# Both instructions identical in structure, different addresses
| lis  r11, 0x82A4   | lis  r11, 0x82B1   |  ← diff_arg (high half differs)
| lwz  r3, 0x1234(r11) | lwz  r3, 0x5678(r11) | ← diff_arg (low half differs)
```

### Root Cause

Our `.text` section is ~18KB larger than the original (0xBBB4D4 vs 0xBB6B14), so all global variables end up at slightly different addresses. Each global reference generates a `lis`+`offset` pair, and both halves show as `diff_arg`. Functions with many global references (10+ pairs) accumulate 20-40 diff_arg mismatches that dominate the match percentage.

### Impact

These mismatches are **cosmetic** — the machine code is functionally identical, just referencing the same globals at different addresses. They inflate the mismatch count but don't represent real code differences.

### Detection

- High `diff_arg` count (20+) with low `diff_op` count (0-2)
- All mismatches are `lis`/`addi`/`lwz`/`stw` with different immediates
- No structural differences (same instruction count, same opcodes)

### What Would Fix It

- Getting our `.text` section to match the original size exactly (eliminating the ~18KB delta)
- objdiff scoring that ignores relocation-only differences (not currently implemented)

### Real Examples

| Function | Match | diff_arg | diff_op | Notes |
|----------|-------|----------|---------|-------|
| SongSortMgr::MoveOn | 94.4% | 28 | 3 | 28 lis/addi pairs + 3 register swaps |
| ~~HamPhotoDisplay::SyncProperty~~ | n/a | 18 | 2 | ⚠ **NOT PRESENT IN THE RB3 BINARY** — Dance Central code (`Ham*`), inherited with this doc from dc3-decomp (lane CB-3, 2026-07-30) |

---

## Static Guard Naming Convention (`??_B` vs `$S`)

**Prevalence:** TUs with few-static functions (e.g., MoveVariant, ClipCollide)
**Typical Gap:** 1-3%

The original MSVC compiler uses a different guard variable naming convention than our MSVC under wibo, causing systematic `$S#` counter offsets that resist reordering fixes.

### Symptom

`run_diff_inspect` shows STATIC_GUARD_COUNTER mismatches where the `$S#` number is consistently off by a fixed amount (e.g., target `$S4` vs base `$S3`) across ALL static-bearing functions in the TU. The pattern detector reports "LikelyFixable: reorder TU definitions" — but reordering does NOT fix it.

### Root Cause

The original MSVC uses two different guard variable types:
- **`??_B` guards** (char-type, `@51` suffix): Used for functions with few static locals (≤8 typically)
- **`$S` guards** (unsigned int, `@4IA` suffix): Used for functions with many statics

These two guard types use **separate counter series**. Under wibo, our MSVC always uses `$S` (uint-type) guards regardless of static count. This means:

```
Target:                         Our build:
  Adjacency → ??_B (not $S)      Adjacency → $S1
  (something) → $S1              MoveVariant ctor → $S2
  (something) → $S2              IsRest → $S3
  MoveVariant ctor → $S3
  IsRest → $S4
```

The `??_B` guards don't consume `$S` slots in the target, so our `$S` numbering is shifted by however many functions use `??_B` instead of `$S`.

### Detection

1. Run `run_diff_inspect mode=mismatches` and check the guard variable names
2. If target uses `??_B?1??FuncName...@51` while base uses `?$S1@?1??FuncName...@4IA` for the SAME function → this pattern
3. If ALL subsequent functions in the TU have the same fixed offset in `$S#` numbering → confirmed unfixable

### Why Reordering Doesn't Help

The shift is caused by a compiler behavior difference (guard type selection), not by function ordering. No amount of function reordering can change which guard type the compiler chooses. Adding dummy statics would shift numbers but create other mismatches.

### What Would Fix It

- Patching wibo's MSVC to use `??_B` (char-type) guards for functions with few statics, matching the original compiler's behavior
- Or patching `c2.dll` to always use `$S` guards (would need to rebuild original)

### Real Examples

| Function | Match | Guard Target | Guard Base | Notes |
|----------|-------|-------------|-----------|-------|
| MoveVariant::IsRest | 98.0% | `$S4` | `$S3` | Adjacency uses ??_B in target |
| ~~MoveCandidate::Adjacency~~ | n/a | `??_B...@51` | `$S1` | ⚠ **NOT PRESENT IN THE RB3 BINARY** — Dance Central code, inherited with this doc from dc3-decomp (lane CB-3, 2026-07-30) |
| ClipCollide::SyncWaypoint | 98.8% | `lbl_82F5ED14` | `$S2` | Same pattern, different TU |

---

## When Hard Patterns May Still Move

Large functions (especially 95%+ matches) often look dominated by compiler noise, but there is usually a small actionable subset.

### Practical Triage

1. **Separate global noise from local structure**
- Treat broad `diff_arg` drift (register swaps, symbol relocation, global stack deltas) as background.
- Prioritize `diff_op`, then small insert/delete clusters, then offset swaps.

2. **Fix one local shape at a time**
- Apply a single control-flow rewrite in one region.
- Re-run objdiff immediately.
- Keep changes that reduce `diff_op` or `diff_score` even if match% is unchanged.

3. **Use branch-polarity steering before declaration churn**
- Try compare viewpoint swaps (`a > b` vs `b < a`), condition inversion, and if/else body flips.
- Only after that, try variable declaration reordering for register swaps.

4. **Stop based on signal, not effort alone**
- If 3-5 branch-shape attempts do not reduce `diff_op`/`diff_score`, that region is likely compiler-fixed.
- Move to the next actionable region rather than broad refactors.

### Why This Matters

Functions tagged `AT_LIMIT` can still improve incrementally. A common pattern is:
- one control-flow inversion fixed,
- rounded match% unchanged,
- but diff score and mismatch quality improve.

That is real progress and lowers the chance of regressions in future attempts.

---

---

## Dead Store Elimination / Destructor Merging

**Prevalence:** Functions using RAII wrappers (CritSecTracker, etc.)
**Typical Gap:** ~1-2% (2-3 instructions)

The target compiler merges explicit cleanup calls with destructor code, eliminating the need to null-out RAII wrapper members.

### Symptom

objdiff shows 1-2 extra instructions (`li rN, 0x0` + `stw rN, offset(rFP)`) that null a member of an RAII wrapper to suppress its destructor. The target doesn't have these because the compiler merged the explicit cleanup with the destructor.

### Root Cause

When code explicitly calls a cleanup method (e.g., `critSec.Exit()`) and then nulls the wrapper's pointer to suppress the destructor's redundant cleanup, the target compiler recognizes the redundancy:

1. Places the destructor's inlined `Exit()` call **only on the false path**
2. On the true path, the explicit `Exit()` stands in for the destructor
3. Never needs the null write since the destructor is structurally bypassed

Our compiler always emits the null store.

### Detection

```cpp
// This generates 2 extra instructions our compiler can't eliminate:
tracker.mCritSec = 0;       // li r11, 0x0 + stw r11, offset(r31)
gDecompressionCritSec.Exit();
```

### What Would Fix It

- c2.dll dead store elimination patch
- No source-level workaround — removing the null causes the destructor to double-Exit

### Real Example

| Function | Match | Gap | Notes |
|----------|-------|-----|-------|
| ChunkStream::PollDecompressionWorker | ~~88.7%~~ **100.0%** | ~11% | ✅ **No longer a wall** (re-measured 2026-07-30). ⚠ **NOT the compiler flip** (absent from its 48-function delta) — the "dead-store elimination" diagnosis was superseded by earlier work; true cause unattributed |

---

## Anonymous Namespace Hash Mismatch

**Prevalence:** Common (any function referencing anonymous namespace symbols)
**Typical Gap:** 0.5-3% (all symbol relocation noise)

Anonymous namespace symbols have different hash values between target and our build.

### Symptom

objdiff shows `diff_arg` on `lis`/`addi`/`bl` instructions where both sides reference the same-named function but with different anonymous namespace hashes:

```
Target: ?A0x7ea4e606@@...
Base:   ?A0x00000000@@...
```

### Root Cause

The Xbox 360 MSVC compiler uses a hash of the source file path for anonymous namespace mangling (`?A0x<hash>@@`). Our build environment produces a different (or null) hash because the file path differs.

### Impact

These mismatches are **purely cosmetic** — the machine bytes are identical, only the linker relocation symbol names differ. They inflate the `diff_arg` count but don't represent real code differences.

### What Would Fix It

- Compiling from the exact same file path as the original build
- c2.dll patch to use a fixed hash value

### Real Examples

| Function | Match | Noise Instructions | Notes |
|----------|-------|-------------------|-------|
| ChunkStream::PollDecompressionWorker | ~~88.7%~~ **100.0%** | 11 of 13 mismatches | ✅ **No longer a wall** (re-measured 2026-07-30). ⚠ **NOT the compiler flip** (absent from its 48-function delta) — most plausibly the **wired `obj_anon_ns_patcher.py`**, which neutralizes exactly this anon-namespace hash noise |
| RndMat::UpdatePropertiesFromMetaMat | 96.6% | Most mismatches | `?A0x53432a53` vs `?A0x00000000` |

---

## Build Environment Regression from Unrelated Headers

**Prevalence:** Rare but impactful
**Typical Gap:** 5-10% regression from a previously higher match

Changes to unrelated headers can regress a function's match% even when the function's own source is unchanged.

### Symptom

A function that was previously at 98%+ drops to 90% without any changes to its source code. The regression correlates with header changes in the same translation unit.

### Root Cause

The compiler's register allocation is sensitive to the total compilation context — inlined function sizes, vtable layouts, template instantiation counts, etc. Changes to headers included by the TU alter this context, which can flip register allocation heuristics for unrelated functions.

### Detection

1. `git log` shows no changes to the function's source
2. `git log` shows header changes in the same commit/timeframe
3. Object file size differs significantly between good and bad builds
4. Register swap (e.g., r30/r31) is the dominant mismatch pattern

### What Would Fix It

- Reverting the header changes (may not be desirable)
- c2.dll register allocator patch
- Sometimes: find the specific header change that triggered the flip and find an alternative formulation

### Real Example

| Function | Before | After | Cause |
|----------|--------|-------|-------|
| NgRnd::UpdateOverlay | 98.6% | 90.0% | ShaderMgr.h vtable reorder, Draw.h deinline, Str.h base class change |

### Note

This pattern is insidious because the regression appears to have no cause. Always check `git log` for header changes when a function regresses without source changes.

---

## Requires Tooling (c2.dll Patching or Custom Pragmas)

These patterns resist source-level changes but are fixable with compiler binary modifications or custom tooling.

### Large Offset Addressing (lis+ori+lwzx vs addis+subi)

**Typical Gap:** ~30%
**Status:** Hard — no known source fix, may yield to compiler binary patching

When struct members are at offsets > 0x7FFF from the base pointer, the compiler must use multi-instruction addressing. The target compiler chose `lis+ori+lwzx` (build address in register, indexed load), while ours uses `addis+subi` (adjusted base, displacement load). Both reach the same memory, different instruction sequence.

| Function | Match | Root Cause |
|----------|-------|------------|
| StreamReceiver360::Tag | **70.166%** ✅ still a wall | mVoice at offset 0x803c uses lwzx vs addis+subi — ✅ **SURVIVED re-measurement under 10224** (lane CB-3, 2026-07-30). This section is still valid. |

### Scalar Deleting Destructor (??_G vs ~T + operator delete)

**Typical Gap:** ~10%
**Status:** Hard — compiler-generated function pattern

The target generates a "scalar deleting destructor" (??_G) wrapper for `delete obj`, while our compiler emits separate `~T()` + `operator delete()` calls. This is a compiler code generation choice for polymorphic delete expressions.

| Function | Match | Root Cause |
|----------|-------|------------|
| StreamReceiver360::Poll | ~~90.9%~~ **100.0%** | ✅ **No longer a wall** (re-measured 2026-07-30). ⚠ **NOT the compiler flip** (absent from its 48-function delta) — so the `??_G` scalar-deleting-destructor claim is falsified as a *current* wall, but its cause is unattributed; do not record it as a compiler-build artifact |

### cmplwi vs cmpwi for Pointer Null Checks

**Typical Gap:** ~1.5%
**Status:** Hard — compiler type-sensitivity for pointer comparisons

The target uses `cmplwi` (unsigned compare) for pointer null checks, while our compiler generates `cmpwi` (signed compare). ~~Explicit casts to `(unsigned int)` do not affect the compare instruction selection. May require compiler-level changes.~~

> ⚠ **FALSIFIED TWICE (2026-07-30, lane CB-3).** This is a pure
> **instruction-selection** verdict — the suspect class — and both halves are wrong:
> 1. **The "may require compiler-level changes" half:** `SfxInst::IsRunning`, the
>    example cited below, now matches at **100.0%**. ⚠ But it is **absent from the
>    compiler flip's 48-function delta**, so do *not* read this as "the compiler
>    version fixed it" — it was closed by earlier work, and the row had been stale
>    for some time. What is falsified is the claim that it is *currently* a wall.
> 2. **The "casts don't help" half:** in `RGTrainerPanel::HandleChordLegend`, a
>    `cmplwi`-vs-`cmpwi` mismatch on an *unsigned-returning* accessor
>    (`RGState::GetFret`, mangled `…QBAIH@Z`) was closed by writing
>    `if ((int)…GetFret(x))`. The cast **does** change compare selection — note the
>    doc only ever tried casting to `(unsigned int)`, i.e. toward the type the value
>    already had, which is a no-op. Casting toward **signed** is the lever. That
>    function reached 100.0% (verified whole-binary: +1 matched, 0 lost).
>
> Signed-vs-unsigned compare selection follows the *type of the expression as
> written*. Check the callee's mangled return type before assuming a wall.

| Function | Match | Root Cause |
|----------|-------|------------|
| SfxInst::IsRunning | ~~98.5%~~ **100.0%** | ✅ **No longer a wall** (re-measured 2026-07-30). ⚠ **NOT the compiler flip** — absent from its 48-function delta; closed by earlier work |

---

## Virtual Base Block Sinking

**Prevalence:** Template functions with virtual base conversion (FlowPtr, ObjPtr with virtual inheritance)
**Typical Gap:** ~40% (59.6% vs potential 100%)

The target compiler sinks the non-null virtual base conversion block after the function return, creating a backward-jump layout that our compiler cannot reproduce.

### Symptom

The diff shows massive `replace` clusters (19+ instructions) where ALL instructions are present in both builds but in completely different positions. The target layout is:

```
[null-check: bne → non-null] [null-path] [common → return] [non-null: vbase conversion + backjump]
```

Our compiler produces:

```
[null-check: beq → null] [non-null: vbase conversion] [b → common] [null-path] [common → return]
```

### Root Cause

When a template like `FlowPtr<T>` is instantiated with a type that virtually inherits from `Hmx::Object` (e.g., `RndAnimatable`), the `T* → Object*` conversion requires a vbtable lookup + null check. The target compiler:

1. **Inverts the condition** to `bne` (branch if non-null) — null path falls through
2. **Sinks the non-null block** (5 instructions: vbtable lookup + Name() load) past the return
3. **Eliminates redundant `li r4, 0`** — knows r4 is already 0 from the null check

This places the common code directly after the null path (zero-cost fallthrough), saving one branch instruction. The non-null path ends with a backward jump to the common code.

For instantiations WITHOUT virtual base (e.g., `FlowPtr<Hmx::Object>`), the target uses the standard `beq` + source-order layout because the non-null path is small (no vbtable lookup).

### What Was Tried

| Approach | Result | Why |
|----------|--------|-----|
| `if (obj)` with hmxObj | 59.6% | Wrong polarity (beq), source-order blocks |
| `if (!obj)` with hmxObj | 6.7% | Right polarity (bne), right instructions, wrong block order |
| goto + backward jump | 59.6% | Compiler normalized goto back into if/else |
| Pre-init + implicit conv (v6) | 57.8% | Changed register allocation, didn't merge checks |
| Ternary + implicit conv | 26.7%–57.8% | Two separate null checks generated |
| `/O1` vs `/Ox` flags | No change | Block ordering identical between optimization levels |

### Key Finding: Compiler Normalizes Gotos

The MSVC front-end (c1xx.dll) normalizes all goto patterns into structured if/else before generating IL. This means:

```cpp
if (obj) goto L_nonnull;
null_body;
L_common: common; return;
L_nonnull: non_null_body; goto L_common;
```

Is compiled identically to `if (obj) { non_null } else { null }; common;` — the compiler detects the structured pattern and regenerates it.

### Why `if (!obj)` Gives 6.7% Despite Correct Instructions

The `if (!obj)` pattern produces **identical instructions** to the target — the diff shows 14 "equal" matches vs 7 for `if (obj)`. But the common block is at a different position (after non-null instead of after null), causing the diff algorithm to report 14 deletes + 13 inserts for the shifted block. The 6.7% score is a diff alignment artifact, not a real quality difference.

### What Would Fix It

- c2.dll block layout pass that sinks large else-blocks past the function return
- This optimization appears to trigger only when the else-block exceeds a size threshold (vbase conversion = 5 insns triggers it; simple pointer copy = 2 insns does not)
- No source-level, optimization flag, or goto pattern can replicate this behavior

### Real Examples

| Function | With `if(obj)` | With `if(!obj)` | Notes |
|----------|---------------|----------------|-------|
| FlowPtr\<RndAnimatable\>::operator= | 59.6% | 6.7% | Block sinking, virtual base |
| FlowPtr\<Hmx::Object\>::operator= | 93.2% | 80.2% | No block sinking, no virtual base |
| FlowPtr\<Sound\>::operator= | 26.7% | untested | Virtual base, likely same pattern |

---

## Self-Copy of 16-byte Member (Regalloc Coin Flip)

**Prevalence:** Occasional — Color / Vector4 / 16-byte struct member assignments
**Typical Gap:** ~0.2-0.5% (8 instructions per site)
**Status:** Confirmed AT_LIMIT 2026-05-26 (Spotlight::SyncProperty triage). Two source rewrites regressed.

When a function inlines a self-copy of a 16-byte member (e.g. `mColorOwner->mColor = c` where `mColor` is `Hmx::Color`), the MSVC register allocator freely picks between two equivalent addressing modes: (a) keep the base in one register and use `+0x1b0` displacement on every store, or (b) materialize `&this->mColor` into a separate register and store via `0(rN)`.

### Symptom

Functions reach 99.x% AT_LIMIT with a residual ~8-instruction cluster in an inlined member-assignment. `run_diff_inspect mode=mismatches` shows the target uses split registers (e.g. r9 = `mColorOwner`, r11 = `mColorOwner + 0x1b0`); base uses single-register + displacement. Both forms are valid PowerPC code; the difference is pure allocator choice.

### Root Cause

The interference graph is identical in both forms, but BSF coloring at c2.dll RVA `0x026780` (⚠ 11886-era offset, see banner; mechanism still valid — see [Register Allocation root cause](#root-cause-c2dll-register-allocator-mechanism)) picks a different color when the inlined sequence's surrounding live ranges shift. Any source-level rewrite that successfully *moves* the swap also tends to introduce *new* clusters elsewhere.

### Detection

Orchestrator classifies as `REGISTER_SWAP (Unfixable) + AtLimit (High)`. If two source-level rewrites both regress, accept and move on.

### Real Example

| Function | Match | Notes |
|----------|-------|-------|
| Spotlight::SyncProperty | 99.8% | 8-instr cluster in inlined `SetIntensity → SetColorIntensity → mColorOwner->mColor = c`. Tried RB3's `Hmx::Color(Color())` temp-copy → 98.8% (regressed +11 inserts + OFFSET_SWAP). Tried local-pointer cache `Spotlight *owner = mColorOwner; owner->mColor = c;` → 99.5% (regressed +4 deletes). Both reverted. |

---

## BEGIN_HANDLERS Static-Init Guard Elision

**Prevalence:** Common — `Handle()` methods with many `_NEW_STATIC_SYMBOL` / `HANDLE_*` macros
**Typical Gap:** 1 instruction (~0.05-0.1%)
**Status:** No source-level fix on MSVC. RB3 (MetroWerks) controlled via `#pragma dont_inline on`; MSVC has no equivalent.

### Symptom

A `Handle()` method with many handlers is at 99.x% AT_LIMIT. The single missing/extra instruction is a `lwz r8, ?$S1@...@4IA, r30` — a function-local static-init guard load before using one of the `static Symbol _s` constants. Target keeps the load; ours elides it (or vice versa).

### Root Cause

Each `static Symbol _s("name");` inside a function gets a static-init guard variable (`?$S1@...@4IA`). The first reference loads the guard, checks if init ran, runs it once, sets the guard. Subsequent references should skip the load — but MSVC's analysis isn't perfect:

- After a sibling handler's `HANDLE_EXPR` (or any expression with a temporary that gets `Release`d on the way out), MSVC may decide the cleanup path "proves" the next static is already initialized and elide the next guard load.
- The decision depends on alias-analysis state at the dispatch point, which depends on the *full set* of `static Symbol` declarations seen so far in the function.

Target was compiled at a point in the codebase's history with slightly different surrounding handlers; the alias-analysis decision differs.

### Why It's Unfixable

- No `#pragma` or `__declspec` disables this specific elision in MSVC.
- Reordering handlers shifts the elision to a different `_s` symbol (the gap moves but doesn't shrink).
- Adding handlers can shift the elision in or out, but DC3 handlers are DTA-load-bearing and can't be removed.
- RB3 controlled this via MetroWerks `#pragma dont_inline on` — no MSVC equivalent.

### Detection

In `run_diff_inspect mode=mismatches`, the residual cluster is a single instruction inside a `BEGIN_HANDLERS` body:

```
delete: lwz r8, ?$S1@?2??Handle@MyClass@@...@4IA, r30
```

The function has many `_NEW_STATIC_SYMBOL` / `HANDLE_*` macros (DC3 norm: 20+).

### Real Example

| Function | Match | Notes |
|----------|-------|-------|
| RndPropAnim::Handle | 99.9% normalized (98.7% raw) | 26 `_NEW_STATIC_SYMBOL` declarations; single instruction at idx 669 elided after a sibling's `DataArray::Release` cleanup |

---

## See Also

- [verifiable-icf.md](verifiable-icf.md) - ICF/linker-side verifiable patterns
- [fixable-declarations.md](fixable-declarations.md#variable-declaration-order) - When register issues are fixable
- [fixable-control-flow.md](fixable-control-flow.md#branch-polarity-steering-beqbne-blebge) - Branch-shape steering tactics
