# Unicorn Function Runner — Design Document

## Overview

A Python tool that loads individual compiled functions into [Unicorn Engine](https://www.unicorn-engine.org/) (a QEMU-based CPU emulator library), executes them in isolation with mocked external dependencies, and compares output state between the original binary and the decomp build.

**Purpose**: Determine whether a decompiled function is *semantically equivalent* to the original, independent of whether the assembly matches instruction-for-instruction.

**Scope**: Single-function differential testing. Not full-program execution. Not a replacement for Xenia runtime validation — this is complementary.

## Value Proposition

| Problem | How this solves it |
|---------|-------------------|
| objdiff says 95% match — is the remaining 5% semantic or just codegen? | Run both versions with same inputs, compare outputs |
| About to mark a function AT_LIMIT — is it behaviorally correct? | Differential test confirms equivalence despite assembly differences |
| Permuter found a variant with higher match% — did it break semantics? | Execution test catches regressions that objdiff can't see |
| Large function with complex state machine — hard to reason about statically | Concrete execution with fixture data exercises actual paths |

### Where It Fits in the Lifecycle

```
[Ghidra/m2c] → write C++ → [objdiff] → tune assembly match
                                ↓
                    90%+ match, residual diffs
                                ↓
                   ┌────────────────────────┐
                   │  Unicorn Function Runner │ ← THIS TOOL
                   │  "Are the diffs semantic │
                   │   or just codegen noise?" │
                   └────────────────────────┘
                                ↓
                    Equivalent → mark AT_LIMIT with confidence
                    Divergent → shows exactly where and why
```

## Architecture

```
                    ┌──────────────┐
                    │  Test Fixture │
                    │  (YAML/JSON)  │
                    │  - register   │
                    │    inputs     │
                    │  - object     │
                    │    memory     │
                    │  - mock       │
                    │    returns    │
                    └──────┬───────┘
                           │
              ┌────────────┴────────────┐
              │                         │
              v                         v
   ┌──────────────────┐     ┌──────────────────┐
   │  Decomp Instance  │     │ Original Instance │
   │                    │     │                    │
   │  Source: .obj COFF │     │  Source: binary    │
   │  Extract: symbol   │     │  Extract: VA from  │
   │    table + relocs  │     │    map file        │
   │                    │     │                    │
   │  ┌──────────────┐ │     │  ┌──────────────┐ │
   │  │ Unicorn PPC  │ │     │  │ Unicorn PPC  │ │
   │  │ - code page  │ │     │  │ - code page  │ │
   │  │ - stack      │ │     │  │ - stack      │ │
   │  │ - object mem │ │     │  │ - object mem │ │
   │  │ - call hooks │ │     │  │ - call hooks │ │
   │  └──────────────┘ │     │  └──────────────┘ │
   └────────┬──────────┘     └────────┬──────────┘
            │                         │
            v                         v
   ┌──────────────────┐     ┌──────────────────┐
   │  Output Capture   │     │  Output Capture   │
   │  - r3 return val  │     │  - r3 return val  │
   │  - modified mem   │     │  - modified mem   │
   │  - call log       │     │  - call log       │
   │  - FPR state      │     │  - FPR state      │
   └────────┬──────────┘     └────────┬──────────┘
            │                         │
            └────────────┬────────────┘
                         v
                ┌─────────────────┐
                │   Comparator     │
                │  - strict/eps    │
                │  - call sequence │
                │  - field diffs   │
                └─────────────────┘
                         │
                         v
              EQUIVALENT / DIVERGENT
              (with first mismatch detail)
```

## Components

### 1. COFF Function Extractor

**Input**: `.obj` file path + mangled symbol name
**Output**: raw PPC bytes + relocation list

The MSVC Xbox 360 compiler produces COFF `.obj` files. Each contains:
- **Symbol table**: maps mangled names → (section index, offset, size)
- **Sections**: `.text` (code), `.data`, `.rdata`, `.bss`
- **Relocation table**: per-section list of (offset, symbol, type)

Relocation types we need to handle (IMAGE_REL_PPC_*):

| Type | Code | Meaning | What to do |
|------|------|---------|------------|
| `REL24` | 0x06 | 24-bit relative branch (`bl target`) | Patch to trampoline or inline target |
| `REFHI` | 0x10 | Upper 16 bits of address (`lis rN, sym@ha`) | Patch with mapped address high half |
| `REFLO` | 0x11 | Lower 16 bits of address (`addi/lwz rN, rN, sym@l`) | Patch with mapped address low half |
| `PAIR` | 0x12 | Paired with REFHI/REFLO | Marks previous reloc as part of a pair |
| `ADDR32` | 0x02 | Absolute 32-bit address | Data pointer, vtable entry |

Note: MSVC Xbox 360 uses `REFHI`/`REFLO` (not `ADDR16_HI`/`ADDR16_LO`). No `TOCREL16`, `REL14`, or `GPREL` relocations were observed in any analyzed .obj file.

**Implementation**: Use Python's `struct` module or the `coff` package to parse COFF headers. Alternatively, shell out to `objdump -t -r` for a quick prototype.

**Key insight**: The relocations ARE the dependency map. Every external function call and every global variable access shows up as a relocation. This is how we know what to mock.

### 2. Original Binary Extractor

**Input**: binary path + function VA + size (from map file + `symbols.txt`)
**Output**: raw PPC bytes

Simpler than COFF extraction — the binary is already linked, so code is at absolute addresses with all relocations resolved. Just read N bytes starting at the file offset corresponding to the VA.

The map file (`orig/45410914/ham_xbox_r.map`) gives symbol → VA mapping.
The symbols file (`config/45410914/symbols.txt`) gives function sizes.

For the original binary, external call targets are resolved to absolute VAs. We identify them by:
- Disassembling with Capstone
- Finding `bl` instructions
- Checking if the target is inside or outside the function's address range
- Looking up the target VA in the map file to get the symbol name

### 3. Unicorn Execution Engine

**Memory Map** (as implemented):

```
0x10000000 - 0x1000FFFF : Stack (64KB)
0x20000000 - 0x2000FFFF : Object memory ('this' pointer region, 64KB)
0x30000000 - 0x3000FFFF : Global data region (64KB)
0x80000000 - 0x8000FFFF : Code region (64KB, matches typical XEX base)
0x80010000 - 0x8001FFFF : Trampoline/mock region (64KB, must be within ±32MB of code for REL24)
0xDEAD0000              : Sentinel (unmapped, catch function return via blr)
```

**Register Setup** (PowerPC calling convention):

| Register | Purpose | Initial value |
|----------|---------|---------------|
| r1 | Stack pointer | `STACK_BASE + 0x8000` (middle of stack) |
| r2 | TOC pointer (unused — no TOC-relative addressing in MSVC Xbox 360) | `0` |
| r3 | First arg / `this` pointer | `OBJECT_BASE` (or fixture value) |
| r4-r10 | Arguments 2-8 | From fixture |
| f1-f13 | Float arguments | From fixture |
| LR | Link register (return address) | Sentinel `0xDEAD0000` (detect function return) |
| CR | Condition register | 0 |

**Termination**: The function returns via `blr`, which jumps to LR. Since LR = `0xDEAD0000` (unmapped), Unicorn will fault. We catch this fault and treat it as normal function return.

### 4. Mock Framework

#### External Call Mocking

When a `bl` instruction targets a function outside the loaded code:

1. **During setup**: Process relocations (decomp) or disassembly (original) to identify all external call sites
2. **Patch targets**: Redirect each external `bl` to a unique trampoline stub in the trampoline region
3. **Trampoline stubs**: Each stub is 8 bytes:
   ```asm
   li r3, <mock_return_value>   # or load from mock table
   blr                          # return to caller
   ```
4. **Hook trampolines**: Register `UC_HOOK_CODE` on the trampoline region to record calls

**Call log entry**:
```python
{
    "callee_symbol": "Symbol::Find",
    "callee_ordinal": 3,        # nth unique external call
    "args": {
        "r3": 0x20000000,       # this
        "r4": 0x30000100,       # arg2
        # ...
    },
    "mock_return": 0,            # what we returned in r3
    "call_index": 7,             # sequential call number
}
```

**Mock return values**: Configurable per-symbol in the fixture:
```yaml
mocks:
  "Symbol::Find":
    return_r3: 0x30000200       # pointer to mock Symbol
  "DataNode::Int":
    return_r3: 42               # integer value
  "Hmx::Object::ClassName":
    return_r3: 0x30000300       # pointer to mock string
  _default:
    return_r3: 0                # default: return NULL/0
```

#### Memory Access Mocking

For reads/writes to global data:

1. **Relocations** tell us which globals the function accesses
2. **Map those globals** into the global data region at their original VAs (or redirect via relocation patching)
3. **Pre-populate** with fixture data
4. **Hook unmapped accesses** via `UC_HOOK_MEM_READ_UNMAPPED` / `UC_HOOK_MEM_WRITE_UNMAPPED` as a safety net — log and provide zero/default values

For `this` pointer fields:

1. **Pre-populate** object memory from fixture data at known offsets
2. The fixture describes the class layout:
```yaml
object:
  _class: "RhythmBattle"
  _size: 0x200
  fields:
    0x30: { name: "mPlayerOne", type: "ptr", value: 0x20001000 }
    0x44: { name: "mPlayerTwo", type: "ptr", value: 0x20002000 }
    0xFA: { name: "mFinale", type: "bool", value: 0 }
    0xFB: { name: "mActive", type: "bool", value: 1 }
    0x104: { name: "mStartBeat", type: "float", value: 16.0 }
    0x108: { name: "mEndBeat", type: "float", value: 32.0 }
```
3. **After execution**, read back the entire object region and diff against pre-execution state

### 5. Comparator

Compare two execution results:

**Strict comparison**:
- `r3` return value (integer return)
- `f1` return value (float return)
- Call log: same functions called, same order, same arguments

**Epsilon comparison**:
- Float registers and float fields: `abs(a - b) < 1e-6`
- Pointer fields: only compare null/non-null (not exact values, since addresses differ between decomp and original)

**Ignored**:
- r1 (stack pointer — may differ in frame layout)
- Internal temporary registers (r11, r12)
- Pointer identity (mock addresses will differ)

**Output format**:
```
RESULT: EQUIVALENT
  Calls: 5 matched (Symbol::Find, DataNode::Int, ...)
  Return: r3 = 0x00000001 (both)
  Object mutations: 2 fields changed (both identical)
```
or:
```
RESULT: DIVERGENT
  First mismatch: call #3
    Decomp called: SetInTheZone(r3=0x20001000, r4=1)
    Original called: SetActive(r3=0x20000000, r4=0)
  State at divergence:
    r3: 0x20001000 vs 0x20000000
    field 0xFB: 1 vs 0
```

### 6. Fixture System

Fixtures define the initial state for a test run. One fixture can be reused for both decomp and original.

```yaml
# fixtures/RhythmBattle_OnBeat_nonfinale.yaml
function:
  symbol: "?OnBeat@RhythmBattle@@AAAXXZ"
  decomp_obj: "build/45410914/src/system/hamobj/RhythmBattle.obj"
  original_va: 0x824E0B40
  original_size: 0x407C

registers:
  r3: "$OBJECT_BASE"              # this pointer
  r4: 0                            # no additional args for OnBeat

object:
  _class: "RhythmBattle"
  _size: 0x200
  fields:
    0xFA: { name: "mFinale", type: "bool", value: 0 }
    0xFB: { name: "mActive", type: "bool", value: 1 }
    # ... more fields

mocks:
  "?Find@Symbol@@SAPAV1@PBD@Z":
    return_r3: 0x30000200
  "?Int@DataNode@@QBEHPAV?$DataArray@@XZ@Z":
    return_r3: 0
  _default:
    return_r3: 0

comparison:
  float_epsilon: 1.0e-6
  ignore_pointer_identity: true
  check_call_order: true
  check_call_args: true
```

### 7. CLI Interface

```bash
# Basic: run one function, compare decomp vs original
python3 tools/unicorn_runner.py \
    --symbol "?OnBeat@RhythmBattle@@AAAXXZ" \
    --obj build/45410914/src/system/hamobj/RhythmBattle.obj \
    --binary orig/45410914/ham_xbox_r.exe \
    --map orig/45410914/ham_xbox_r.map \
    --fixture fixtures/RhythmBattle_OnBeat_nonfinale.yaml

# Quick mode: auto-generate minimal fixture (zeroed state, all mocks return 0)
python3 tools/unicorn_runner.py \
    --symbol "?Poll@Game@@UAEXXZ" \
    --obj build/45410914/src/system/meta/Game.obj \
    --auto-fixture

# Batch: run all functions in a compilation unit
python3 tools/unicorn_runner.py \
    --obj build/45410914/src/system/meta/Game.obj \
    --auto-fixture \
    --batch

# Permuter integration: score a variant
python3 tools/unicorn_runner.py \
    --symbol "?Poll@Game@@UAEXXZ" \
    --obj /tmp/claude/permuter_variant_042.obj \
    --reference-obj build/45410914/src/system/meta/Game.obj \
    --auto-fixture \
    --score
```

## Implementation Phases

### Detailed Phase Designs

- **Phase 1**: [docs/unicorn_runner/PHASE1_DESIGN.md](../unicorn_runner/PHASE1_DESIGN.md) — COFF extraction, relocation patching, trampoline mocking, execution-sequence comparison

### DONE: Phase 0 — Proof of Concept (Feb 11, 2026)

**Verdict: PPC32 BE is fully viable. No showstoppers.**

Research script: `scripts/unicorn_runner/research.py`

- Proved Unicorn PPC32 BE executes MSVC-compiled Xbox 360 code correctly
- Built reusable `COFFParser` class for COFF `.obj` parsing
- Cataloged all 5 relocation types used (REL24, REFHI, REFLO, PAIR, ADDR32)
- Executed 3 real functions from .obj files in Unicorn
- Benchmarked: ~90 us/function execution including instance creation

### DONE: Phase 1 — Functional Prototype (Feb 11, 2026)

**Implemented**: 8 modules, 1021 lines, all features working.

**What's implemented**:
- COFF extraction from both .obj flavors (decomp multi-symbol, original COMDAT)
- Relocation patching for all 5 types (REL24, REFHI, REFLO, PAIR, ADDR32)
- Trampoline-based call mocking with call log recording
- Execution-sequence comparison (call count, args r3-r6, return value, memory diffs)
- Auto-fixture mode (zeroed state, all mocks return 0)
- `--list-functions` with eligibility filtering
- Exit codes: 0=EQUIVALENT, 1=DIVERGENT, 2=ERROR, 3=SKIPPED

**Test results**: 19/19 functions tested EQUIVALENT across Skeleton.obj and FileChecksum.obj

### DONE: Phase 2 — std/ld Rewriting + Batch Mode (Feb 11, 2026)

**What's new**:
- **PPC64 instruction rewriting**: `std`/`ld` (opcodes 62/58) rewritten to `stw`/`lwz` (opcodes 36/32) before execution. Same-size 4B→4B replacement. Both sides get identical rewriting. Unblocks ~1,079 functions.
- **`--batch` mode**: Test all eligible functions in a unit
- **`--batch-all` mode**: Test all units in objdiff.json

```bash
# Compare a single function
python3 -m scripts.unicorn_runner.run \
    --symbol "?ElapsedMs@Skeleton@@UBAHXZ" \
    --unit system/gesture/Skeleton

# List eligible functions in a unit
python3 -m scripts.unicorn_runner.run \
    --unit system/gesture/Skeleton --list-functions

# Verbose mode with call trace
python3 -m scripts.unicorn_runner.run \
    --symbol "?TiltAngle@SkeletonFrame@@QBAMXZ" \
    --unit system/gesture/Skeleton --verbose

# Batch: all eligible functions in a unit
python3 -m scripts.unicorn_runner.run \
    --unit system/gesture/Skeleton --batch

# Batch-all: all units in objdiff.json
python3 -m scripts.unicorn_runner.run --batch-all
```

**Known limitations**:
- Functions with `bctr`/`bctrl` (indirect branches/calls) are skipped — need vtable/jump table mocking
- Auto-fixture only tests the null/zero execution path

**Test results**: Skeleton.obj batch: 22 equivalent, 8 divergent, 30 total (up from 22 eligible in Phase 1)

### DONE: Phase 4 — Divergence Classification (Feb 13, 2026)

**What's new**: Automatic classification of DIVERGENT results into root-cause categories, eliminating noise from unfixable build-environment artifacts.

**`classify_divergence()` in `comparator.py`** categorizes every DIVERGENT result:

| Category | Detection | Actionable? |
|----------|-----------|-------------|
| `build_env` | Args point to GLOBAL_BASE (\_\_FILE\_\_ strings), `merged_` in call target warnings, globals-only memory diffs | No — unfixable compiler/linker artifacts |
| `regalloc` | Same call count, <=2 calls differ, values are small non-pointer integers | No — compiler register allocation quirk |
| `logic` | Everything else (errors, FPR mismatches, call count diffs, pointer-valued arg diffs) | Yes — real behavioral difference |

**Pipeline refactoring**: `_run_comparison_core()` extracted from `run_comparison_inner()`, returns `ComparisonBundle` dataclass with raw results. `diagnose_single()` uses this directly for classification.

**Output changes**:
```
# diagnose --batch now shows classified FIX types:
  FIX(build_env)    90.4%  DirLoader::FixClassName
  FIX(regalloc)     95.2%  SomeOtherFunc
  FIX               99.8%  MakeString<Symbol, ...>

# Summary includes breakdown:
  Summary: 13 DONE, 37 SKIP [...], 5 FIX [4 logic, 1 build_env]
  Unfixable divergences: 1 (1 build_env, 0 regalloc)
```

**Tests**: 12 unit tests covering all classification categories.

### DONE: Phase 5 — Multi-Input Probing (Feb 13, 2026)

**What's new**: Run each function N times with varied fill patterns for higher-confidence equivalence testing.

**`prober.py`** — new module with `probe_function()` that runs N comparisons with varied inputs:
- Run 0: zero fill (baseline)
- Run 1: 0xCD (MSVC debug fill)
- Run 2+: random byte patterns from seeded RNG

Each run gets full comparison + divergence classification. Results aggregated into `ProbeResult`:
- `stable_equiv` (all runs equivalent) → confidence: `high`
- `stable_divergent` (all runs divergent) → confidence: `stable_divergent`
- `input_sensitive` (mixed) → confidence: `input_sensitive`

**CLI**:
```bash
# Single function
python3 -m scripts.unicorn_runner.probe --unit DirLoader \
    --symbol "?FixClassName@@YA?AVSymbol@@V1@@Z" --runs 16

# Batch (all eligible functions)
python3 -m scripts.unicorn_runner.probe --unit DirLoader --batch --runs 8
```

**Results** (DirLoader, 55 functions, 4 runs): 48 stable equiv, 2 stable div, 5 input-sensitive. Vs dual-fixture which found only 1 fixture-sensitive — multi-input probing provides finer granularity.

**Tests**: 12 unit tests with mocked comparison core.

**See also**: [Structural Probing Plan](../plans/unicorn-structural-probing.md) for the full roadmap (Phase 2: struct field access probing, Phase 3: mock return variation).

### Phase 3: Hardening (planned)

**Research completed**: [docs/sessions/2026-02-11-unicorn-phase3-research.md](../sessions/2026-02-11-unicorn-phase3-research.md)

**Static analysis** (25,680 common functions across 971 units):
- 21,804 eligible (84.9%), 3,876 blocked by bctrl/bctr (15.1%)
- bctrl (virtual dispatch): 3,704 blocked — needs generic vtable mocking
- bctr (switch tables + tail calls): 182 blocked — needs .rdata loading
- 585 (2.3%) return floats — f1 comparison not yet implemented
- 12,937 (50.4%) have intra-TU calls — deprioritized (improves fidelity, not coverage)
- ~75% equivalence rate with auto-fixture on partial batch-all run

**Prioritized implementation plan**:

| Priority | Item | Effort | Functions Unblocked |
|----------|------|--------|-------------------|
| P1 | FPR (f1) return comparison | 1h | 585 float-returning |
| P2 | bctr: switch tables + tail calls | 4-8h | 182 |
| P3 | Permuter guard rail | 2-4h | — (quality improvement) |
| P4 | bctrl: virtual dispatch mocking | 8-16h | 3,704 |
| P5 | Batch-all parallelization | 2-4h | — (CI enablement) |

**Deprioritized**: Intra-TU call co-loading, custom fixtures, CI integration

## Finding Divergent Functions

After running unicorn tests, query the database for functions with real behavioral bugs:

```bash
# Find DIVERGENT logic functions (real bugs, not build_env/regalloc artifacts)
./bin/orchestrate divergent --limit 20

# Run batch to fix them
./bin/orchestrate batch --strategy divergent --limit 10
```

**Divergence Classes**:
- `logic` — real bugs to fix (wrong code)
- `build_env` — `__FILE__` path differences, unfixable
- `regalloc` — register allocation quirks, usually unfixable

## Dependencies

| Dependency | Purpose | Status |
|------------|---------|--------|
| unicorn (Python) | PPC CPU emulation | Local build from `/home/free/code/milohax/unicorn/` — PPC32 BE verified working |
| capstone (Python) | PPC disassembly (for original binary analysis) | `pip install capstone` — PPC64 BE supported |
| pyyaml | Fixture file parsing | `pip install pyyaml` |
| struct (stdlib) | COFF parsing | Built-in — verified working with MSVC PPC BE COFF (machine 0x01F2) |

Local repos:
- `/home/free/code/milohax/unicorn/` — Unicorn source, built with `cmake -DUNICORN_ARCH=ppc`. Set `LIBUNICORN_PATH=/home/free/code/milohax/unicorn/build` and add `bindings/python` to `PYTHONPATH`.
- `/home/free/code/milohax/capstone/` — Capstone source

## Known Limitations

### Unicorn PPC Status (verified Feb 11, 2026)

| Capability | Status | Verified |
|------------|--------|----------|
| PPC32 Big-Endian | Works — all tests pass | Yes, 11/11 tests |
| PPC64 Big-Endian | Broken — every insn raises UC_ERR_EXCEPTION | Yes, confirmed |
| GPR r0-r31 | Available, read/write works | Yes |
| FPR f0-f31 | Available, requires MSR FP bit (0x2000) | Yes |
| CR, LR, CTR, XER, MSR, FPSCR | Available | Yes (LR, MSR tested) |
| Integer instructions (addi, lwz, stw, cmpwi, subfic, mulli, etc.) | Work | Yes |
| Float instructions (lfs, stfs, fsubs) | Work (with MSR FP bit) | Yes |
| Branch instructions (b, bl, blr, bne, beq) | Work | Yes |
| Prolog/epilog (mflr, mtlr, stwu, stack frame) | Work | Yes |
| UC_HOOK_CODE | Reliable, fires every instruction | Yes |
| Vector registers (vr0-vr31) | NOT exposed via API | Not tested |
| Standard AltiVec instructions | Internally emulated, registers not accessible | Not tested |
| VMX128 instructions | Not supported | Not tested |

**Implication**: PPC32 BE mode is production-ready for scalar PPC functions. Functions using `std`/`ld` (PPC64 load/store — common for callee-saved register preservation on Xenon) are handled via byte-rewriting to `stw`/`lwz` (Phase 2). PPC64 mode is unusable. AltiVec/VMX128 functions cannot be tested until Unicorn exposes vector register access.

### What This Cannot Validate

- **Full program state coherence**: Only tests one function at a time. Cross-function state corruption won't be caught.
- **Timing-dependent behavior**: Execution is single-threaded and deterministic. Race conditions or timing-sensitive code won't manifest.
- **Hardware interaction**: Anything that touches real hardware (GPU, audio, input) must be mocked. Mock fidelity limits confidence.
- **C++ exception unwinding**: Unicorn doesn't support structured exception handling. Functions that throw/catch need special handling.
- **Virtual dispatch accuracy**: vtable layouts must be manually specified in fixtures. If the vtable is wrong, calls go to wrong mocks.

### Comparison with Other Dynamic Analysis Approaches

| Aspect | Unicorn Runner | Xenia Runtime | angr Symbolic |
|--------|---------------|---------------|---------------|
| Speed | Microseconds/call | Seconds (boot) | Seconds-minutes |
| Scope | Single function | Full program | Single function |
| Input space | Concrete (fixture) | Concrete (gameplay) | Symbolic (all paths) |
| Confidence | Medium (mock fidelity) | High (real execution) | Highest (formal) |
| Effort to add function | Low (write fixture) | Medium (scenario design) | High (SimProcedures) |
| VMX128 | No | Yes | No |

## Open Research TODOs

These items need dedicated investigation sessions before or during implementation.

### DONE: Unicorn PPC Viability Testing (Feb 11, 2026)

**Verdict: PPC32 BE is fully viable. No showstoppers.**

Research script: `scripts/unicorn_runner/research.py`
Unicorn source: `/home/free/code/milohax/unicorn/` (built with PPC-only: `cmake -DUNICORN_ARCH=ppc`)

#### PPC32 Big-Endian Results (all PASS)

| Test | Result | Notes |
|------|--------|-------|
| Basic execution (`li r3, 42; blr`) | PASS | Sentinel-based return detection works perfectly |
| Arithmetic + memory (`lwz`, `addi`) | PASS | Big-endian memory access correct |
| Conditional branching (`cmpwi`, `bne`) | PASS | All branch conditions tested |
| MSVC prolog/epilog (`mflr/mtlr`, `stwu`, stack frame) | PASS | Standard calling convention works |
| `UC_HOOK_CODE` reliability | PASS | Fires on every instruction, correct addresses |
| `bl` interception via trampoline | PASS | Branch-and-link + trampoline return works |
| Real MSVC function: `Symbol::operator==` (24B, integer) | PASS | Loads, compares, subfe/cntlzw idiom |
| Real MSVC function: `Subtract` (52B, float) | PASS | `lfs`, `fsubs`, `stfs` — all FP ops work |
| Real MSVC function: `HandJoint` (32B, member access) | PASS | `subfic`, `mulli`, `add` — arithmetic chain |

#### PPC64 Big-Endian: BROKEN

- Instance creation succeeds, but every instruction triggers `UC_ERR_EXCEPTION`
- PC advances normally but exception fires after each instruction
- This matches Unicorn's own assessment: PPC64 has zero test coverage
- **Not a blocker**: DC3 is 32-bit code, PPC32 mode handles all instructions correctly

#### Key Findings

- **MSR FP enable bit is required**: Must set `MSR |= 0x2000` before execution, otherwise any float instruction (`lfs`, `stfs`, `fsubs`, etc.) raises `UC_ERR_EXCEPTION`. This is because the PPC FP unit is disabled by default in the MSR.
- **Performance**: ~90 us average per function execution (1000-iteration loop benchmark), including Unicorn instance creation. Fast enough for batch testing thousands of functions.
- **Hook reliability**: `UC_HOOK_CODE` fires reliably on every instruction. Region-scoped hooks (`begin`/`end` params) work for trampoline interception.
- **Sentinel return detection**: Setting LR to unmapped address `0xDEAD0000` and catching `UC_ERR_FETCH_UNMAPPED` works perfectly for detecting function return via `blr`.

### DONE: MSVC COFF PPC Relocation Types (Feb 11, 2026)

**Verdict: Only 5 relocation types used. Simpler than expected.**

Analyzed 3 .obj files: `FileChecksum.obj` (78 sections), `Skeleton.obj` (289 sections), `SongMgr.obj` (981 sections).

#### Relocation Types Actually Used

Only 5 types appear across all analyzed .obj files:

| Type | Code | Count (SongMgr) | Purpose |
|------|------|-----------------|---------|
| `REL24` | 0x06 | 812 | 24-bit relative branch (`bl target`) — function calls |
| `ADDR32` | 0x02 | 641 | 32-bit absolute address — data pointers, vtables |
| `PAIR` | 0x12 | 573 | Paired with REFHI/REFLO to complete address |
| `REFLO` | 0x11 | 304 | Lower 16 bits of address (`addi rN, rN, sym@l`) |
| `REFHI` | 0x10 | 269 | Upper 16 bits of address (`lis rN, sym@ha`) |

**Notable absences**: No `ADDR16_HI`/`ADDR16_LO` (MSVC uses `REFHI`/`REFLO` instead), no `TOCREL16` (no TOC-relative addressing), no `REL14` (no 14-bit conditional branches to external targets), no `GPREL`.

#### Hi/Lo Address Pair Pattern

MSVC materializes 32-bit addresses using REFHI+PAIR then REFLO+PAIR, always in sequence:

```
offset=0x000000  REFHI   sym=target_symbol    # patches lis instruction
offset=0x000000  PAIR    sym=@comp.id          # marks as paired
offset=0x000004  REFLO   sym=target_symbol    # patches addi instruction
offset=0x000004  PAIR    sym=@comp.id          # marks as paired
```

The PAIR relocations always reference `@comp.id` (a compiler-internal sentinel). HI and LO counts don't always match exactly (REFLO slightly outnumbers REFHI) — some patterns use a single `lis` with multiple `lwz`/`stw` instructions sharing the same high half.

#### COFF Structure Notes

- Machine type: `0x01F2` (`IMAGE_FILE_MACHINE_POWERPCBE`)
- **Each function gets its own `.text` section** (COMDAT folding). A 981-section .obj is normal.
- Standard `objdump` doesn't recognize the format — need custom COFF parser (Python `struct` works fine)
- Symbol names use `$M` and `$T` prefixes for compiler-generated internal labels
- `.pdata` sections contain exception/unwind info (8 bytes each, 1 reloc)

#### Round-Trip Test Result

Successfully extracted 3 real MSVC-compiled functions from .obj, loaded raw bytes into Unicorn PPC32 BE, and executed them to completion — **all returned normally**. Functions with zero relocations work out of the box. Functions with relocations need patching before execution (the unresolved address fields contain zeros).

### DONE: Call Interception Strategy (Feb 11, 2026)

**Verdict: Relocation-based patching + trampoline stubs works perfectly.**

- Phase 1 uses COFF relocations (both sides) to identify external call sites
- REL24 relocations are patched to point at trampoline stubs
- REFHI/REFLO/ADDR32 relocations are patched to point at allocated global slots
- `UC_HOOK_CODE` on the trampoline region logs call arguments
- No Capstone needed — original .obj files also have relocations

### TODO: Original Binary Function Extraction (lower priority)

Phase 1 uses the original .obj files (from `build/45410914/src/`) instead of extracting from the linked binary. This is simpler and provides relocations for patching. Direct binary extraction is only needed if .obj files are unavailable.

### RESEARCHED: Virtual Dispatch and vtable Mocking (Feb 11, 2026)

**Verdict**: 3,704 functions blocked by bctrl. Generic vtable mocking is feasible.

- 3,694 bctrl-only + 10 bctrl+bctr functions, median size 328B
- Approach: Write generic vtable at OBJECT_BASE+0, each slot → unique trampoline
- No per-class vtable knowledge needed for zeroed auto-fixture
- Detailed findings: [Phase 3 research](../sessions/2026-02-11-unicorn-phase3-research.md)

### DONE: PPC64 Instruction Support (Feb 11, 2026)

**Verdict: Byte-rewriting std→stw and ld→lwz works perfectly.**

- `rewrite_ppc64_insns()` in `patcher.py` replaces opcodes in-place (4B→4B, no size change)
- DS-form (14-bit field × 4) converted to D-form (16-bit signed offset) — all offsets in range
- No `stdu`/`ldu` variants found in codebase (only simple std/ld with XO=0)
- Both sides get identical rewriting, preserving equivalence testing validity
- Unblocked ~1,079 functions (Skeleton.obj: 22→30 eligible, Char.obj: 65→205 eligible)

### RESEARCHED: Float Precision Investigation (Feb 11, 2026)

**Verdict**: Unicorn FPU is bit-identical for equivalence testing. No epsilon needed for auto-fixture.

- 585 functions (2.3%) return floats via f1
- `fsubs` and `fmadds` produce bit-identical results between decomp and original
- Unicorn may not implement true FMA, but this doesn't matter — both sides get the same treatment
- For custom fixtures with real float data, use `1e-6` relative epsilon
- Adding f1 comparison is ~15 lines of code (P1 priority)

### TODO: Fixture Design Patterns
- [ ] Pick 3-5 representative functions spanning different complexity levels
- [ ] For each, manually create a fixture and document the process — how painful is it?
- [ ] Investigate auto-fixture generation: can we derive a useful initial fixture from COFF relocations + class headers alone?
- [ ] Determine minimum fixture quality needed to detect real divergences (vs false positives from bad mocks)

### TODO: Integration with Existing Tooling
- [ ] Can objdiff's function extraction code be reused? (it already reads .obj files and extracts function bytes)
- [ ] Can the permuter's build system produce .obj files that this tool can consume directly?
- [ ] What's the right integration point with ninja? (custom build target? standalone script?)
- [ ] Should results feed back into decomp.db? (mark functions as "execution-verified")

### TODO: Scope Boundary — When NOT to Use This
- [ ] Define criteria for when Unicorn testing is overkill (e.g., tiny functions, pure getters/setters)
- [ ] Define criteria for when Unicorn testing is insufficient (e.g., need full-program Xenia validation)
- [ ] Document known false-positive patterns (where decomp and original diverge in Unicorn but are equivalent in practice)

---

## Prior Art and References

### Internal
- `scripts/unicorn_runner/` — Phase 1 implementation (8 modules) + Phase 0 research script
- `docs/sessions/2026-02-09-tooling-review-code-authoring.md` — Section 5: Capstone + Unicorn micro-testing
- `docs/sessions/2026-02-08-onbeat-runtime-validation-tooling-handoff.md` — Section 4: Mocked differential harness
- `docs/sessions/2026-02-11-qemu-dynamic-analysis-research.md` — QEMU/Unicorn research and TCG plugin API assessment

### External
- Unicorn Engine: https://www.unicorn-engine.org/
- Unicorn Python bindings: https://github.com/unicorn-engine/unicorn/tree/master/bindings/python
- Capstone PPC: https://www.capstone-engine.org/
- COFF format (PE/COFF spec): https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- D-Helix (decompiler correctness via recompilation): https://github.com/purseclab/D-helix
