# Inside MSVC's Xbox 360 Register Allocator

Reverse engineering the register allocation pipeline in Microsoft's Xbox 360 C++ compiler (`cl.exe` v16.00.11886, Visual Studio 2010 era) to understand why recompiled code produces different register assignments from the original binary.

## Background

The rb3-xenon decompilation (and its sibling DC3 decomp) targets a **retail release build** (`/O1`, no LTCG) for Xbox 360 (PowerPC). The compiler is MSVC for Xbox 360, targeting the PowerPC 970 (Xenon) architecture. When recompiling C++ source that produces semantically identical code, the compiler frequently assigns different registers to the same variables — causing functions to plateau at 90-97% match with no source-level fix. This research was conducted on DC3 but applies equally to rb3-xenon (same toolchain era, same flags).

These "register swap" mismatches (e.g., the compiler uses r29 where the original uses r31) are the single largest category of unfixable pattern. Understanding *why* they happen required instrumenting the compiler itself.

## The Compilation Pipeline

MSVC's `cl.exe` is an orchestrator that invokes two DLLs in sequence:

```
cl.exe
  → c1xx.dll  (frontend)  Parses C++ → writes CIL to temp files
  → c2.dll    (backend)   Reads CIL → optimizes → allocates registers → emits PPC
```

All three run in a single process. The frontend and backend communicate through temporary files written to disk:

| Suffix | Content |
|--------|---------|
| `_CL_<hash>ex` | Expression tree (intermediate operations) |
| `_CL_<hash>sy` | Symbol table (variable names, IDs, types) |
| `_CL_<hash>gl` | Globals / code structure |
| `_CL_<hash>in` | Include metadata |
| `_CL_<hash>db` | Debug info |

These files are written by c1xx.dll and immediately consumed (then deleted) by c2.dll. We intercepted them using strace with fault injection to prevent deletion:

```bash
strace -e trace=open,pwrite64,write,close,unlink -s 5000 -x \
  --inject unlink,unlinkat:retval=0 \
  wibo cl.exe /c /O1 test.cpp
```

## How Declaration Order Encodes Into the IL

We compiled two variants of a minimal function — identical except for the order of two local variable declarations — and binary-diffed the captured IL files.

**The `.sy` (symbol table) diff — 2 bytes changed:**

```
Offset 0x47: ef 09 → f0 09   (symbol ID for 'alpha')
Offset 0x78: f0 09 → ef 09   (symbol ID for 'beta')
```

The frontend assigns **monotonically increasing 16-bit symbol IDs** in declaration order. In variant A, `alpha` gets ID `0x09ef` and `beta` gets `0x09f0`. In variant B, the IDs swap. The first-declared variable always gets the lower ID.

**The `.ex` (expression tree) diff — 8 bytes changed:**

All changes are symbol ID references (`ef 09` ↔ `f0 09`) being swapped throughout the expression tree. The tree structure is byte-identical — only the ID references differ.

**The `.gl`, `.in`, `.db` files**: Identical (except source filename in `.gl`).

This establishes the first link in the chain: **declaration order → symbol IDs in the IL**.

## Finding the Decision Point in c2.dll

To trace how symbol IDs propagate through the backend, we profiled c2.dll at instruction granularity using Valgrind's callgrind tool. Two compilations (one per variant) produce callgrind profiles with per-address execution counts. Diffing these profiles reveals which c2.dll code paths diverge.

We ran four independent experiments:

| Experiment | Source | c2.dll Instructions | Divergent Addresses |
|------------|--------|---------------------|---------------------|
| Controlled (4-line function) | swap_a vs swap_b | 1.22B | 196 |
| Real TU (Tex.cpp, 12 functions) | reorder + cleanup swap | 3.85B | 12,595 |
| CharBones | FindOffset declaration reorder | 2.25B | 2,468 |
| Locale | 3-function reorders | 2.10B | 4,179 |

Cross-referencing all four experiments identified **177 addresses** that diverge consistently. For each, we computed the ratio of delta magnitude between experiments. This separates:

- **TU-scaling addresses** (ratio >> 1): Loops whose iteration count scales with source complexity. These are downstream effects — symbol table traversal, hash lookups, memory allocation.
- **Non-scaling addresses** (ratio ≈ 1): Code that executes roughly the same delta regardless of TU size. These are the actual decision points.

54 addresses had a ratio < 3× across all four experiments. These are the register allocator's core code paths.

## The Register Allocator Architecture

Disassembling c2.dll at the identified addresses revealed a complete register allocation subsystem. All RVA offsets below are relative to c2.dll's image base (`0x10b00000`).

### Register Bitsets

The allocator represents register sets as variable-length arrays of 64-bit words (stored as pairs of 32-bit `lo`/`hi` DWORDs). Two precomputed lookup tables provide single-bit masks:

- `0x10b014c0`: **Set masks** — `SET_MASK[n]` has bit `n` set (128 entries × 8 bytes, for bits 0-63)
- `0x10b016c0`: **Clear masks** — `CLR_MASK[n]` = `~SET_MASK[n]`

The `set_bit` function (RVA `0x026816`):

```asm
; ecx = bitset pointer, edx = register index
mov    ecx, [ecx]           ; dereference to data array
mov    eax, edx
shr    eax, 6               ; word_index = reg / 64
lea    eax, [ecx+eax*8]     ; word_ptr = &data[word_index]
and    edx, 0x3f            ; bit_pos = reg % 64
mov    ecx, [edx*8+0x10b014c0]  ; lo mask
or     [eax], ecx           ; set lo bit
mov    ecx, [edx*8+0x10b014c4]  ; hi mask
or     [eax+4], ecx         ; set hi bit
ret
```

`clear_bit` (RVA `0x026837`) is identical but uses AND with the clear mask table at `0x10b016c0`.

### Register Class State

Three global arrays, indexed by register class ID (0-8), track allocation state:

```
0x10c2e088[class*4]  — allocation state (purpose partially confirmed)
0x10c2e100[class*4]  — allocation state
0x10c2e178[class*4]  — free list head for bitset node recycling
```

The `clear_class` function (RVA `0x0267d6`) zeros all three for a given class:

```asm
; ecx = class ID
mov    eax, ecx
xor    ecx, ecx
mov    [eax*4+0x10c2e088], ecx
mov    [eax*4+0x10c2e100], ecx
mov    [eax*4+0x10c2e178], ecx
ret
```

GDB tracing observed classes 2, 3, 5, 7, and 8 being initialized during a simple compilation. The numbering likely corresponds to PPC register classes: volatile GPR, callee-saved GPR, FPR, condition registers, link/count registers, etc.

### The Interference Graph

Variable interference is stored as a **sparse sorted linked list** per variable. Each node covers a 64-register block:

```
struct InterferenceNode {
    uint32_t base;     // +0x00: base register number (multiple of 64)
    uint32_t lo_bits;  // +0x04: interference mask, registers [base..base+31]
    uint32_t hi_bits;  // +0x08: interference mask, registers [base+32..base+63]
    Node*    next;     // +0x0c: next block in sorted list
};
```

**Lookup** (RVA `0x026d68`) walks the list comparing `base` values:

```asm
; ecx = list head pointer, edx = register number
mov    ecx, [ecx]           ; dereference to first node
and    edx, 0xffffffc0      ; align to 64-register block
.loop:
test   ecx, ecx
jne    .check
xor    eax, eax             ; not found → return NULL
ret
.check:
cmp    [ecx], edx           ; compare node->base with target
jae    .found_or_past
mov    ecx, [ecx+0xc]       ; advance to next node
jmp    .loop
.found_or_past:
; branchless exact-match test:
mov    eax, [ecx]           ; node->base
sub    eax, edx             ; base - target
neg    eax                  ; negate
sbb    eax, eax             ; -1 if borrow (target > base), 0 if equal
not    eax                  ; 0 if not found, -1 if found
and    eax, ecx             ; NULL or node pointer
ret
```

The branchless ending is notable — `sub` / `neg` / `sbb` / `not` / `and` produces either the node pointer (exact match) or NULL (no match) without a conditional branch.

**Insert** (RVA `0x026d39`) follows the same traversal but creates a new node (via allocator at `0x10b26cd4`) when the target block doesn't exist, splicing it into the sorted list.

### The Interference Test

The `interferes` function (RVA `0x026f37`) checks whether a specific register conflicts with a variable's interference set:

```asm
; ecx = interference list, edx = register number
push   esi
mov    esi, edx
call   lookup_interference   ; → eax = node or NULL
test   eax, eax
je     .no_interference
and    esi, 0x3f            ; bit position within block
mov    ecx, [esi*8+0x10b014c0]  ; SET_MASK.lo
mov    edx, [esi*8+0x10b014c4]  ; SET_MASK.hi
and    ecx, [eax+0x4]       ; test against node->lo_bits
and    edx, [eax+0x8]       ; test against node->hi_bits
or     ecx, edx
je     .no_interference
mov    al, 1                ; return TRUE (interferes)
pop    esi
ret
.no_interference:
xor    al, al               ; return FALSE
pop    esi
ret
```

### find_first_set — The BSF Primitive

The `find_first_set` function (RVA `0x026780`) extracts the lowest-numbered register from a 64-bit set:

```asm
; [esp+4] = lo bits, [esp+8] = hi bits
; Returns: bit index (0-63) or -1 if empty
mov    eax, [esp+0x4]       ; lo
test   eax, eax
je     .try_hi
xor    ecx, ecx             ; offset = 0
jmp    .scan
.try_hi:
mov    eax, [esp+0x8]       ; hi
push   0x20
pop    ecx                  ; offset = 32
.scan:
bsf    eax, eax             ; find first set bit
je     .empty
add    eax, ecx             ; adjust for hi word
jmp    .done
.empty:
or     eax, 0xffffffff      ; return -1
.done:
ret    0x8
```

This is called from five sites in the allocator, all in the RVA `0x026b`–`0x027428` range. All five follow the same pattern:

1. Push `lo`/`hi` from the current register set
2. Call `find_first_set`
3. If result ≠ -1, add the block's base offset (`add eax, [node]`)
4. Clear the found bit from the set
5. Return the register number

### The Allocation Iterator

The primary register selection function (RVA `0x027225`) iterates through a linked list of interference entries, calling `find_first_set` on each block's available bits:

```asm
; ecx = interference list, edx = iterator state (on stack)
; Iterator state: [edx+0] = current node, [edx+4] = lo, [edx+8] = hi
test   ecx, ecx
je     .use_cached_bits     ; no interference → use pre-loaded bits
mov    eax, [ecx]           ; first node
mov    [edx], eax           ; cursor = first node
; ... load bits from node ...
.try_bsf:
push   [edx+0x8]            ; hi
push   [edx+0x4]            ; lo
call   find_first_set
cmp    eax, -1
je     .exhausted
mov    ecx, [edx]           ; current node
add    eax, [ecx]           ; register = bsf_result + node->base
; ... clear bit, check bounds, return register ...
```

## Proof: Declaration Order Controls Allocation

### GDB Tracing

We set a software breakpoint at `find_first_set` (RVA `0x026780`) in c2.dll and ran both variants of a controlled test case through the compiler under GDB. Despite being limited to ~5 breakpoint hits before SIGSEGV (due to wibo's 64/32-bit mode switching), the captured traces are definitive.

**Simple test** (two volatile locals, produces r10↔r11 swap):

```
swap_a (alpha declared first):        swap_b (beta declared first):
  BSF lo=0x00000004 hi=0x00000000       BSF lo=0x00000002 hi=0x00000000
  BSF lo=0x00000002 hi=0x00000000       BSF lo=0x00000004 hi=0x00000000
  BSF lo=0x000c0000 hi=0x00000000       BSF lo=0x000c0000 hi=0x00000000
  BSF lo=0x000c0000 hi=0x00000000       BSF lo=0x000c0000 hi=0x00000000
  BSF lo=0x00000000 hi=0x00000003       BSF lo=0x00000000 hi=0x00000003
```

The first two calls swap: `0x4, 0x2` vs `0x2, 0x4`. Calls 3-5 are identical. The declaration order directly determines which BSF call receives which available-register mask.

**Callee-saved test** (three locals surviving across function calls, produces r29↔r31 swap):

The allocator processes three register classes sequentially. The first two classes show reversed allocation order:

| Class A | callee_a (α,β,γ) | callee_b (γ,β,α) |
|---------|-------------------|-------------------|
| Call 1 | bit 8 | bit 10 |
| Call 2 | bit 9 | bit 9 |
| Call 3 | bit 10 | bit 8 |

| Class B | callee_a | callee_b |
|---------|----------|----------|
| Call 4 | bit 3 | bit 1 |
| Call 5 | bit 2 | bit 2 |
| Call 6 | bit 1 | bit 3 |

Remaining calls (7-11) are identical between variants.

### The Single-Bit Observation

Every BSF call in the trace has **exactly one bit set** in the available mask (`0x2`, `0x4`, `0x100`, `0x200`, etc. — all powers of two or isolated bits). The register for each variable is predetermined before `find_first_set` is called. BSF is not searching a multi-register available set — it is extracting a bit index from a pre-determined single-register result.

This means the actual allocation decision happens upstream, in the interference graph coloring phase. The coloring algorithm processes variables in symbol ID order, and each variable's register assignment constrains subsequent allocations through the interference graph. By the time BSF runs, the result is already determined.

## The Complete Chain

```
C++ source: declaration order of local variables
         ↓
c1xx.dll: assigns monotonic 16-bit symbol IDs in declaration order
         ↓
IL .sy file: symbol IDs encode declaration order
IL .ex file: expression tree references symbol IDs
         ↓
c2.dll: processes variables in symbol ID order during coloring
         ↓
Interference graph: each allocation constrains subsequent ones
         ↓
Register assignment: determined by coloring order
         ↓
find_first_set: extracts pre-determined register from single-bit mask
         ↓
PPC codegen: emits instructions using assigned registers
```

Changing declaration order changes symbol IDs, which changes coloring order, which changes register assignments. The effect is deterministic and mechanical. Two variables that compete for the same register will be assigned differently depending on which one is colored first — and coloring order is symbol ID order — and symbol IDs are declaration order.

## Why Source-Level Fixes Cannot Work

We tested every plausible source-level mitigation:

- **Declaration reorder**: Changes the swap pattern but cannot eliminate it. If variable A must have r29 and variable B must have r31 in the original, no declaration order of our source will reproduce that specific assignment unless our declaration order happens to match the original's (which we don't know).

- **Explicit null variables**: Attempting to "reserve" a register by declaring `int null_var = 0;` before the real variables. The compiler optimizes this away entirely — the IL files are byte-identical with and without the null variable.

- **Comparison/expression rewriting**: Flipping `a > b` to `b < a`, reordering operands, etc. These don't affect symbol ID assignment, so they don't affect register allocation.

- **Exhaustive permutation**: For a function with 4 local declarations, we tested all 16 permutations. None improved the register match. Several made it significantly worse (91% vs 95%).

The register allocation is locked to the symbol ID chain. No amount of C++ source manipulation can independently control the coloring order without also changing the symbol IDs, and the mapping from source changes to symbol ID changes is not invertible — we cannot work backward from a desired register assignment to a required declaration order without knowing the original source's declaration order.

## Methodology Notes

### Tooling

All experiments used automated tooling in `tools/compiler_trace/`:

- **diff-asm**: Compiles two source variants with `/FAs` assembly listings, normalizes filenames, detects register swaps via consistent substitution patterns, and reports semantic differences after normalization.
- **capture-il**: Intercepts IL temp files via strace fault injection on `unlink`/`unlinkat` syscalls (returns success without deleting), then hex-dumps and diffs the captured files.
- **callgrind-diff**: Runs both variants under Valgrind's callgrind (requires 32-bit wibo to avoid mode-switching issues), parses the `positions: instr line` format, and diffs per-address execution counts within c2.dll's `.text` section.
- **annotate**: Disassembles c2.dll regions via `objdump` and overlays callgrind execution counts and cross-experiment observation data from a persistent JSON knowledge base.

### GDB Under Wibo

Wibo is a Windows PE loader for Linux. The 64-bit build uses far jumps (`jmp fword ptr`) for 64-to-32-bit mode switching. GDB software breakpoints (INT3 insertion) work in the PE guest address space, but the single-step mechanism (temporary INT3 removal → step → re-insertion) corrupts the instruction stream in mixed-mode execution. We reliably captured ~5 breakpoint hits per run before SIGSEGV.

The 32-bit wibo build eliminates mode-switching but maps PE sections as READ+EXEC (no WRITE), preventing INT3 insertion entirely. The c2.dll is also loaded lazily — it is not mapped at `call_EntryProc` time, only after the frontend finishes parsing.

### Compiler Flags

The target binary was compiled with:

```
/O1 /Oi /GR /EHsc
```

On Xbox 360, `/O1` expands to `/Oy /Ob2 /GF` (different from standard x86 MSVC). `/fp:fast` is the default per XDK documentation, and `#pragma fp_contract` is ON by default.

Hidden flags confirmed working on this compiler version: `/Bd` (show internal command lines), `/Bt` (per-phase timing: c1xx ~12ms, c2 ~2ms for small files), `/Bv` (module versions), `/d1reportAllClassLayout` (class memory layouts).

## c2.dll Function Reference

Summary of reverse-engineered functions in the register allocator. All RVAs relative to image base `0x10b00000`.

| RVA | Signature | Description |
|-----|-----------|-------------|
| `0x0266d0` | `int popcount64(uint32_t lo, uint32_t hi)` | Parallel bit count (Hamming weight) using 0x55555555/0x33333333/0x0f0f0f0f constants |
| `0x026763` | `Node* alloc_node(int class)` | Pop from per-class free list; falls back to 8-byte allocation |
| `0x026780` | `int find_first_set(uint32_t lo, uint32_t hi)` | BSF on 64-bit value; returns bit index or -1 |
| `0x0267a2` | `Bitset* create_bitset(int nregs, Ctx* ctx)` | Allocate `ceil(nregs/64) * 8` bytes via context allocator |
| `0x0267d6` | `void clear_class(int class)` | Zero three global arrays for a register class |
| `0x0267f0` | `Bitset* alloc_and_init(Ctx* ctx)` | Wrapper: `alloc_node` + `create_bitset` |
| `0x026804` | `void free_node(Node* node)` | Push onto per-class free list at `0x10c2e178` |
| `0x026816` | `void set_bit(Bitset* bs, int reg)` | OR with precomputed mask from `0x10b014c0` |
| `0x026837` | `void clear_bit(Bitset* bs, int reg)` | AND with complement mask from `0x10b016c0` |
| `0x026858` | `void and_inplace(Bitset* dst, Bitset* src)` | Set intersection, in-place loop over 64-bit words |
| `0x02687e` | `void and_copy(Bitset* a, Bitset* b, Bitset* dst)` | Three-operand set intersection |
| `0x026cd4` | `Node* alloc_interference_node()` | Allocate a 16-byte interference graph node |
| `0x026d39` | `Node* insert_interference(List* list, int reg)` | Insert/find node for register's 64-block in sorted list |
| `0x026d68` | `Node* lookup_interference(List* list, int reg)` | Find node; branchless exact-match via `sub`/`neg`/`sbb`/`not`/`and` |
| `0x026efb` | `void remove_interference(List* list, int reg)` | Clear bit; remove empty node from list |
| `0x026f37` | `bool interferes(List* list, int reg)` | Test single register against interference set |
| `0x027225` | `int next_register(List* interf, State* iter)` | Primary allocation iterator; walks interference list calling BSF |
| `0x027290` | `int next_register_global(List* interf)` | Same pattern using global state at `0x10c2e1f0`–`0x10c2e1fc` |
