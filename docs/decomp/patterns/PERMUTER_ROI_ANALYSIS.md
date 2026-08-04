# Permuter Pattern ROI Analysis

> Cross-reference of documented decomp patterns vs existing permuter automation.
> **Updated:** 2026-03-04

## Executive Summary

There are **22 permuter pattern implementations** covering ~72 documented fixable techniques. Cross-referencing pattern docs and session logs identified the highest-ROI gaps and implemented 8 new patterns (Phase 1+2 complete).

The highest-frequency detected patterns across all functions are address relocation (5,295), register swap (1,327), control flow (733), offset swap (445), and scope counter mismatch (430). Of these, control flow and offset swap have the best fixability prospects.

## ROI Rankings: New Patterns to Implement

### Tier 1: Trivial AST, High ROI

| Priority | Pattern | Impact | Success | Detection Signal | AST Work |
|----------|---------|--------|---------|-----------------|----------|
| **1** | **bitwise_accumulator** | +10-15% | HIGH | Short-circuit branches vs `and` | Find `result = result && expr`, try `result = result & expr` |
| **2** | **max_to_conditional** | +35% | HIGH | `bl Max` vs inline compare | Find `Max(a,b)` calls, replace with `if (a < b) a = b` |
| **3** | **sizeof_signed_cast** | +6% | HIGH | `srwi` vs `srawi+addze` | Wrap `sizeof()` with `(int)` in signed divisions |
| **4** | **initializer_literal** | +100% | HIGH | Constructor mismatch | Normalize `0.0f`/`false`/`NULL` → `0` in initializer lists |
| **5** | **alloca_intrinsic** | +5% | HIGH | ALLOCA_MISMATCH | Swap `alloca` ↔ `_alloca` |

### Tier 2: Moderate AST, High ROI

| Priority | Pattern | Impact | Success | Detection Signal | AST Work |
|----------|---------|--------|---------|-----------------|----------|
| **6** | **early_return_merge** | +15-40% | HIGH | Repeated cmp+return patterns | Combine guard returns into `||` chain |
| **7** | **bool_return_expr** | +5-15% | HIGH | Reg swap on return path | Convert `if(c) return false; return X` → `return !c && X` |
| **8** | **hoist_sret** | +6% | HIGH | Extra lwz/stw in loop on sret | Hoist sret variable declaration before loop |
| **9** | **fsel_template** | +5-44% | HIGH | Branch vs `fsel` instruction | Replace `if(x<y) x=y` with `Clamp()`/`Min()`/`Max()` template |
| **10** | **pragma_fp_contract** | +1-12% | HIGH | `fmadds` vs `fmuls`+`fadds` | Add/remove `#pragma fp_contract(on/off)` around expressions |
| **11** | **single_return** | +6% | MED | `beq` vs `bne` direction | Pre-initialize result, use single `return` at end |
| **12** | **lazy_call** | +0.4% | MED | Early getter before conditional | Move getter calls into conditional blocks where used |
| **13** | **bit_test_bool** | +5-7% | MED | `rlwinm.` vs `extrwi.` | Extract `(flags & MASK)` to `bool` before `&&` chain |
| **14** | **noreturn_attr** | variable | MED | Dead code after call | Add `__declspec(noreturn)` to never-returning functions |

### Tier 3: Already Implemented

| Existing Pattern | Win Rate | Covers Documented Patterns |
|-----------------|----------|---------------------------|
| `signed_unsigned` | 30% | Signed/Unsigned Cast, Loop Counter, String Iteration, sizeof() |
| `variable_extraction` | 42% | Variable Extraction, Local Bool Extraction |
| `comparison_equivalence` | 10% | Unsigned Zero Comparison, Comparison Style |
| `branch_polarity` | 5% | Branch Polarity Steering, Sequential If, Single Return |
| `declaration_reorder` | 20% | Variable Declaration Order (BSF-guided) |
| `declaration_movement` | — | Move declarations to change register allocation |
| `inline_assignment` | 22% | Inline Assignment |
| `fma_reorder` | 2% | FMA Expression Order |
| `comparison_flip` | 15% | Comparison Operand Order |
| `comma_split` | — | Split comma expressions into separate statements |
| `negation_split` | — | Split `-func()` into `f = func(); f = -f;` |
| `and_split` | — | Split `if (a && b)` into nested ifs |
| `bool_cast` | — | Wrap bool expressions with `bool()` or extract to local |
| `bitwise_accumulator` | — | Replace `&&` with `&` for bool accumulation |
| `max_to_conditional` | — | Replace `Max(a,b)` with `if (a < b) a = b` (and reverse) |
| `early_return_merge` | — | Combine guard returns into `\|\|` chain (and reverse) |
| `bool_return_expr` | — | Convert `if/return false/return true` → `return !cond` |
| `fsel_template` | — | Replace float conditionals with `Min()`/`Max()`/`Clamp()` |
| `ternary_swap` | 0/10 | Ternary vs If-Else |
| `argument_swap` | 5% | Argument Evaluation Order |
| `commutative_swap` | 0/143 | Regroups chains (0 wins — deprioritize) |
| `empty_size_swap` | 0/38 | `empty()` vs `size()==0` (0 wins — deprioritize) |

### Tier 4: Not Automatable (Manual Only)

These require semantic understanding, header changes, or whole-file restructuring:

| Pattern | Why Not Automatable |
|---------|-------------------|
| Explicit Destructor | Requires knowing which classes need `~T() {}` |
| ObjPtr DeferOwner | Requires template/header changes |
| Early Return for Destructor Path | Requires RAII semantic analysis |
| Struct Layout / Padding | Requires header modifications |
| Static Variable Scope | Requires understanding brace intent |
| Function Definition Order | Requires whole-file reordering with $S# tracking |
| Pre-Compute Refs Before Calls | Requires data flow analysis |
| Float/Double Separation | Requires type analysis across expression trees |
| dynamic_cast Replacement | Requires knowing GetObj() availability |

## Detected Pattern Counts (from DB)

Pattern counts are across **all non-excluded functions** (not just AT_LIMIT):

| Pattern | Count | Fixability | Notes |
|---------|------:|------------|-------|
| ADDRESS_RELOCATION | 5,295 | Unfixable | Positional drift from .text size delta |
| REGISTER_SWAP | 1,327 | Partially fixable | Via `declaration_reorder`/`declaration_movement` |
| CONTROL_FLOW | 733 | Fixable | Via `branch_polarity` + `and_split` |
| OFFSET_SWAP | 445 | Best fixability | Field reorder, `variable_extraction` |
| PROLOGUE_MISMATCH | 439 | Unfixable | Compiler prologue generation quirks |
| SCOPE_COUNTER_MISMATCH | 430 | Unfixable | `$S` guard naming/numbering |
| ANON_NAMESPACE_HASH | 337 | Unfixable | `?A0x<hash>` path-dependent (patched post-build) |
| LINKER_MERGED | 263 | Unfixable | ICF merged identical functions |
| COMMUTATIVE_OP_ORDER | 172 | Low | 0 wins from `commutative_swap` |
| BOOL_MASK | 101 | Low | Via `bool_cast` (newly added) |
| STATIC_GUARD_COUNTER | 48 | Unfixable | Needs whole-file function reorder |
| COMPARISON_STYLE | 30 | Covered | Via `comparison_equivalence` |
| MAKESTRING_MISMATCH | 27 | AT_LIMIT | Ease assert stripping, unfixable |
| ALLOCA_MISMATCH | 7 | Trivial | `alloca_intrinsic` pattern |
| DEAD_STORE_ELIMINATION | 7 | Low | Compiler optimization quirk |
| FSEL_TERNARY | 6 | Fixable | Replace branch with `Clamp()`/`Min()`/`Max()` |
| BOOLEAN_NEGATION | 2 | Covered | Via `negation_split` |
| FLOAT_PRECISION | 2 | Manual | Cast placement (`0.001` vs `0.001f`) |
| FLOAT_INT_FLOAT | 1 | Manual | Type conversion chain |

### AT_LIMIT Breakdown

| Category | Count |
|----------|-------|
| Total AT_LIMIT | 2,469 |

## Existing Patterns with 0 Wins

These 3 patterns should be reviewed:

| Pattern | Attempts | Issue | Recommendation |
|---------|----------|-------|----------------|
| `commutative_swap` | 143 | Regroups chains but PPC compiler doesn't vary on this | Keep but deprioritize |
| `empty_size_swap` | 38 | `empty()` vs `size()==0` rarely the root cause | Keep but deprioritize |
| `ternary_swap` | 10 | Low attempt count; the pattern IS valid but rare | Keep, increase coverage |

## Objdiff Detection → Permuter Pattern Mapping

| Objdiff Signal | Current Permuter | Gap Pattern |
|---------------|-----------------|-------------|
| `CONTROL_FLOW` | `branch_polarity`, `and_split`, `bool_return_expr`, `early_return_merge` | ✓ well covered |
| `REGISTER_SWAP` | `declaration_reorder`, `declaration_movement` | **wrong axis — see [correction](#register_swap--declaration_reorder-is-the-wrong-axis-correction) below**; missing class is liveness/scheduling |
| `OFFSET_SWAP` | (none) | Could trigger `variable_extraction` or field reorder |
| `COMPARISON_STYLE` | `comparison_equivalence` | ✓ covered |
| `COMMUTATIVE_OP_ORDER` | `commutative_swap` | ✓ covered (0 wins though) |
| `BOOL_MASK` | `bool_cast` | ✓ covered |
| `BOOLEAN_NEGATION` | `negation_split` | ✓ covered |
| `FLOAT_PRECISION_MISMATCH` | (none) | `sizeof_signed_cast`, `initializer_literal`, cast placement |
| `ALLOCA_MISMATCH` | (none) | `alloca_intrinsic` |
| `FSEL_TERNARY` | `fsel_template` | ✓ covered |
| `MAKESTRING_TEMPLATE_MISMATCH` | (none) | Needs `.Str()` insertion |
| `STATIC_GUARD_COUNTER` | (none) | Needs whole-file function reorder |
| `PROLOGUE_MISMATCH` | (skip — unfixable) | — |
| `SCOPE_COUNTER_MISMATCH` | (skip — unfixable) | — |
| `LINKER_MERGED` | (skip — unfixable) | — |
| `ADDRESS_RELOCATION_NOISE` | (skip — unfixable) | — |

## `REGISTER_SWAP` → `declaration_reorder` is the wrong axis (correction)

*Added 2026-08-04. Evidence measured in **dc3-decomp** (identical `base` cflags —
`/nologo /wd4355 /wd4164 /c /GR /O1 /Oi /EHsc`), n = 4 functions; one control leg measured
here. Full writeup: [fixable-liveness.md](fixable-liveness.md).*

The `REGISTER_SWAP → declaration_reorder` row above is the historical mapping and it is
measured to **under-fire**, structurally rather than as a tuning problem.

| Function | Result | Declaration-axis evidence |
|---|---|---|
| `ObjectDir::Iterate` (DC3) | 99.4% → **100%** from a one-line liveness change | 6 reorder variants **byte-identical**, 2 regressed to ~95.8%, 65-candidate beam search **0 improvements** |
| `RndText::FitTextScroll` (DC3) | 92.7% → 98.2% via call-through-the-local + block scoping | reorder variants: no movement |
| `RndText::SizeCheck` (DC3) | 96.5% → 99.1% via scheduling then comparison polarity | not the declaration axis |
| `LabelShrinkWrapper::UpdateAndDrawWrapper` (DC3) | 68.1% → **99.9%** from naming four unnamed temps | 6 commutative/reorder variants **byte-identical**; 56-candidate beam search 0 improvements |

**Why the mapping fails.** MSVC's colouring assigns colours from the interference graph and
those colours are **invariant to declaration order**; order only permutes the
colour→register mapping, and only where the constraints leave slack. A byte-identical
`.obj` from a reorder is the *expected* outcome whenever the constraints are tight — the
mutation cannot change the program the allocator sees. To move a register swap you must
change the **interference graph**: what is live across a call, or where a value is
materialised.

**Tooling implication.** `declaration_reorder` / `declaration_movement` are
*stack-and-packing* mutations that occasionally hit registers as a side effect. The missing
class is liveness/scheduling mutations. Candidates (all unimplemented, n=1 evidence each —
hypotheses, not roadmap):

| Proposed pattern | Transform |
|---|---|
| `aggregate_projection` | `f(a, b)` ↔ `f(agg.first, agg.second)` where `agg` was just built from `a`,`b` and is unmodified |
| `member_call_through_local` | `obj->mField->Method(...)` ↔ `local->Method(...)` where `local` already caches `obj->mField` |
| `out_param_init_strip` | drop `= 0` / `= 0.0f` on a local whose first use is as an out-param the callee writes unconditionally |
| `product_hoist` | collapse `a = f(); b = g(); … use(a*b)` ↔ `p = f()*g(); … use(p)` to move the multiply's schedule slot |
| `decl_scope_into_block` | move a declaration into the inner block that uses it (stack packing, not registers) |
| `name_call_arg_temporary` | `f(T(x,y,z))` ↔ `T t(x,y,z); f(t);` — lengthens a temporary's live range so the frame packer sees it |

`out_param_init_strip` and `member_call_through_local` need safety gates (the callee must
write the out-param on every reachable path; the local must provably still alias the member
at the call site), so they belong in the guarded-transform tier alongside
`variable_extraction`, not the free-mutation tier.

**Sweep-budgeting note.** A zero-gain sweep over declaration-axis mutations is **not**
evidence that a function is at a register floor. `ObjectDir::Iterate` produced exactly that
result and then went to 100% from one line.

**`PROLOGUE_MISMATCH` is mis-listed as unfixable above** for the save-count sub-case: a
`__savegprlr_NN` delta is the fingerprint of a live-set difference, which is the most
tractable signal in this whole class. The "skip" verdict holds for the
`_RtlCheckStack12` / `alloca` sub-case only.

## Instruction Scheduling

**Coverage: none.** No permuter pattern mutates where a value is *materialised* relative to
its consumer. This is the second half of the `REGISTER_SWAP` gap above, and it is the half
that shows up as **volatile**-register swaps (`r0`, `r3`-`r12`, `f0`-`f13`) rather than
callee-saved ones — a volatile register cannot be live across a call, so a swap between two
of them is a scheduling or operand-order question, never a live-across-call question. That
exclusion is an ABI consequence, not a statistical claim.

Worked example, DC3's `RndText::SizeCheck` 96.5% → 99.1%: the target computes a product
*before* the `fcmpu` that consumes it,

```
fmuls  f12, f30, f1
...
fcmpu  cr6, f13, f0
bge    ...
```

while our build computed it inside a later `if` condition, putting the `fmuls` in a
different slot and leaving the compare reading a different register (`fcmpu cr6, f0, f12` /
`ble`). Collapsing the two operand locals into one product local fixed the schedule and all
nine FPR swaps resolved on their own. Only *then* did flipping the compares to the target's
operand order become productive.

**Ordering constraint for any implementation: schedule first, polarity second.** Flipping a
comparison before the producing arithmetic is in the right slot just moves the swap to the
other side of the compare, which scores as a neutral wash and teaches a beam search that the
polarity mutation is useless. A scheduling pattern must be sequenced *ahead* of
`comparison_equivalence` / `branch_polarity`, not composed freely with them.

Full pattern writeup:
[fixable-liveness.md: Lever 3](fixable-liveness.md#lever-3--fix-the-schedule-first-then-the-comparison-polarity).

> **Anchor contract.** objdiff-cli emits
> `docs/decomp/patterns/PERMUTER_ROI_ANALYSIS.md#instruction-scheduling` as one of its
> `REGISTER_SWAP` hint URLs, and this repo resolves as `DocProject::Dc3` (marker file:
> `PERMUTER_ROI_ANALYSIS.md`). **Do not rename this heading** — objdiff renders only the
> first URL per pattern, so a renamed anchor silently degrades tool output. Verify with
> `python3 ../objdiff/scripts/check_doc_links.py --dc3 . --allow-missing`.

## Implementation Plan

### Phase 1: Done
`negation_split`, `and_split`, `bool_cast` — implemented with tests.

### Phase 2: Done
`bitwise_accumulator`, `max_to_conditional`, `early_return_merge`, `bool_return_expr`, `fsel_template` — implemented with tests. All 112 tests pass.

### Phase 3: Data-driven priorities (from commit history mining, 2026-03-09)

Commit history analysis of 956 function improvements across 11 baselines revealed:

**Fix existing patterns (highest ROI — proven wins in history but 0% permuter rate):**
1. **Fix `ternary_swap` relevance** — 32.4% of human improvements involve ternary changes, but permuter has 0/10 wins. Root cause: `relevant()` fires on any branch opcode mismatch (too broad), wasting budget. Fix: tighten to ternary-specific signals, boost when Ghidra shows ternary patterns.
2. **Fix `empty_size_swap` relevance** — 6.9% of improvements, 0 permuter wins. Root cause: only fires on `divw`/`divwu` signal, but real codegen difference includes `cmplw` vs `subf`+`clrrwi`.

**New patterns (from unclassified improvements):**
3. `reference_elimination` — inverse of `member_ref_bind`: remove `auto& ref = m[i]; ref.foo` → use `m[i].foo` directly. Easy AST transform.
4. `const_ref_swap` — `Type copy = expr` ↔ `const Type& ref = expr`. Affects copy ctor codegen.
5. `static_init_explicit` — add explicit `= nullptr/false/0` to file-scope statics. Trivial.
6. `find_operand_order` — swap `end() != find()` to `find() != end()`. Comparison order matters.

**Existing priorities (still valid):**
7. `pragma_fp_contract` — add/remove `#pragma fp_contract(on/off)` around expressions
8. `hoist_sret` — hoist loop variable for sret register matching
9. `alloca_intrinsic` — swap `alloca` ↔ `_alloca`
10. `noreturn_attr` — `__declspec(noreturn)` insertion

### Commit History Mining Tool

`scripts/analysis/mine_patterns.py` — mines cached baselines for pattern validation and discovery. See `docs/sessions/2026-03-09-commit-history-pattern-mining.md` for full analysis.
