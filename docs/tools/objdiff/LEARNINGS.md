# objdiff CLI Learnings & Improvement Ideas

This document captures patterns, insights, and potential tool improvements discovered through extensive use of objdiff CLI for decompilation work.

---

## Table of Contents

1. [Match Type Reference](#match-type-reference)
2. [Diagnostic Patterns](#diagnostic-patterns)
3. [Fixability Decision Tree](#fixability-decision-tree)
4. [Workflow: Fix Loop](#workflow-fix-loop)
5. [Troubleshooting](#troubleshooting)
6. [Useful Command Recipes](#useful-command-recipes)
7. [Example Outputs & Analysis](#example-outputs--analysis)
8. [Parallel Agent Strategy](#parallel-agent-strategy)
9. [Quick Reference Card](#quick-reference-card)
10. [Mining Logs for Patterns](#mining-logs-for-patterns)
11. [Appendix A: PowerPC Instructions](#appendix-a-powerpc-instruction-quick-reference)
12. [Appendix B: Code Patterns](#appendix-b-code-patterns-that-affect-matching)

---

## Match Type Reference

| Type | Meaning | Fixable? | Common Causes |
|------|---------|----------|---------------|
| `equal` | Instructions match perfectly | N/A | Correct code |
| `diff_arg` | Same opcode, different operand | **Sometimes** | Register allocation, linker merging, symbol addresses |
| `diff_op` | Different opcode | **Usually** | Wrong operation, type mismatch |
| `replace` | Completely different instruction | **Usually** | Logic error, wrong algorithm |
| `delete` | Extra instruction in decomp | **Sometimes** | Unnecessary operations, different optimization |
| `insert` | Missing instruction in decomp | **Sometimes** | Missing operations, different optimization |

### diff_arg Sub-Categories (Not Currently Exposed)

When analyzing `diff_arg`, the *content* of the argument matters:

| Argument Pattern | Meaning | Fixable? |
|------------------|---------|----------|
| `merged_*` function name | Linker merged identical functions | **No** |
| Register swap (r30↔r31) | Variable declaration order | **Maybe** |
| Branch target offset | Different code layout | **Usually No** |
| Symbol address | Relocation difference | **No** |
| Immediate value | Constant or offset difference | **Maybe** |

---

## Diagnostic Patterns

### Pattern 1: Linker-Merged Functions (VERIFY THEN ACCEPT)

**Symptom:** `diff_arg` where target shows `merged_*` or `OnlyReturns`:
```json
{
  "target": {"opcode": "bl", "args": "merged_Read4FloatStruct"},
  "base": {"opcode": "bl", "args": "??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z"},
  "match_type": "diff_arg"
}
```

**Why it happens:** The linker optimizes identical function bodies into a single merged function. This happens at link time, not compile time (ICF - Identical COMDAT Folding).

**Common merged patterns:**
- `merged_Read3FloatStruct` - Vector3 stream reads
- `merged_Read4FloatStruct` - Color/Vector4 stream reads
- `merged_SetObjConcrete` - ObjRef template instantiations
- `OnlyReturns` - Functions that just return a value

**Address-Based Merged Symbols:**

When objdiff shows `merged_82331360` (address-based rather than named):
1. The linker merged multiple functions with identical code to that address
2. Look up which symbols share the address:
   ```bash
   ./bin/merged-symbols 82331360
   # Or MCP: mcp__orchestrator__lookup_merged_symbol address="82331360"
   ```
3. Example: Address 0x82331448 has 76 `ObjRefConcrete<T>::GetObj()` instantiations

**Verification Workflow (IMPORTANT):**

Don't blindly accept LINKER_MERGED as unfixable. Verify first:

1. **Identify your call target** - What function does YOUR code call?
2. **Look up the merged address** - Use `./bin/merged-symbols <addr>`
3. **Check membership**:
   - **Your symbol IS in the set** → Correct code, accept at_limit (unfixable)
   - **Your symbol is NOT in the set** → Investigate! You may be calling the wrong function

**Example verification:**
```
objdiff shows: bl merged_82331360
Your code has: delete objRef;  // calls ObjRef::~ObjRef

Step 1: ./bin/merged-symbols 82331360
Output:
  1. ObjRef::`scalar deleting destructor'
  2. ObjRef::`vector deleting destructor'

Step 2: Your call target IS in the set (destructor)
Result: Code is correct, accept at_limit
```

**Why address-based patterns occur:**
- Template instantiations generate identical code for different types
- Destructor pairs (`??_G` scalar + `??_E` vector) have identical code
- The linker assigns the merged address; any symbol name is valid

**Statistics:** The binary contains 31,754 COMDAT-folded symbols across 3,068 unique merged addresses.

---

### Pattern 2: Bool Mask (OFTEN FIXABLE)

**Symptom:** Extra `clrlwi` (Clear Left Word Immediate) instruction masking to 8 bits:
```json
{"index": 33, "target": {"opcode": "clrlwi", "args": "r3, r11, 24"}, "match_type": "delete"}
```

**Key insight: Bool funniness usually means there's an inline.** The `clrlwi` mask appears at bool type boundaries. When the target has it but our build doesn't, it often means the original code went through a bool-typed intermediate that we're skipping.

**Fixes that WORK:**

```cpp
// Fix 1: Extract to local bool variable (forces mask at assignment)
// PartyModeMgr::SetSongAndDefaults - eliminated bool mask, 97.8% → 98.2%
bool isSpecialMode = mode == dance_battle || mode == strike_a_pose;
CalcCharacters(data, isSpecialMode, ...);  // instead of passing expression directly

// Fix 2: Explicit (bool) cast on ternary expressions
// RndTransformable::Handle - 99.9% → 100%
_msg->Size() > 3 ? (bool)(_msg->Int(3) != 0) : false  // instead of without cast
```

**Attempted fixes that did NOT work** (for the opposite case where *our* build generates extra `clrlwi`):

```cpp
bool GetEnabled() { return 1; }              // Still generates clrlwi
bool IsValid() { return ptr != NULL; }       // Still generates clrlwi
bool IsActive() { bool r = (flags & F); return r; }  // Still generates clrlwi
bool HasData() { return data ? true : false; }        // Still generates clrlwi
```

**When unfixable:** When our build generates `clrlwi` that the target doesn't have, and no source-level change removes it. See [detailed patterns doc](decomp/patterns/fixable-bool-mask.md#step-4-when-its-actually-unfixable).

---

### Pattern 3: Register Allocation Swap (SOMETIMES FIXABLE)

**Symptom:** Consistent register swaps throughout function:
```json
{"target": {"args": "r31, r3"}, "base": {"args": "r30, r3"}},
{"target": {"args": "r10, r31"}, "base": {"args": "r10, r30"}}
```

**Fix attempts with examples:**

```cpp
// Original (r31/r30 swapped vs target):
void Process(Object* obj) {
    Foo* foo = obj->GetFoo();  // Gets r31
    Bar* bar = obj->GetBar();  // Gets r30
    // ...
}

// Attempt 1: Swap declaration order
void Process(Object* obj) {
    Bar* bar = obj->GetBar();  // Now gets r31
    Foo* foo = obj->GetFoo();  // Now gets r30
    // ...
}

// Attempt 2: Move declaration into block
void Process(Object* obj) {
    Foo* foo = obj->GetFoo();
    {
        Bar* bar = obj->GetBar();  // Different scope may change allocation
        // ...
    }
}

// Attempt 3: Delay first use
void Process(Object* obj) {
    Foo* foo;
    Bar* bar = obj->GetBar();
    foo = obj->GetFoo();  // Assigned after bar is used
    // ...
}
```

**When it works:** When register allocation is determined by declaration order or first-use order. The compiler assigns callee-saved registers (r31, r30, r29...) in order of variable lifetime.

**When it doesn't work:** When the compiler's register allocator makes decisions based on liveness analysis that can't be influenced by source-level changes.

---

### Pattern 4: Control Flow Mismatch (OFTEN FIXABLE)

**Symptom:** `diff_op` with branch instructions, or `replace` with different branches:
```json
{"target": {"opcode": "bne"}, "base": {"opcode": "beq"}, "match_type": "diff_op"}
```

**Common fixes:**
- Invert condition logic
- Combine/split if conditions
- Change `if-else if` to `if { } else if { }`
- Use ternary instead of if-else (or vice versa)

**Example that worked (MatShaderFlagsOK):**
```cpp
// Before (95.3%):
if (curShader->CheckError(0)) {
    if (!mat->FadeOut()) { ... }
}

// After (98.2%):
if (curShader->CheckError(0) && !mat->FadeOut()) { ... }
```

---

### Pattern 5: Comparison Style Difference (SOMETIMES FIXABLE)

**Symptom:** `diff_op` with comparison, different immediate values:
```json
{"target": {"opcode": "cmpwi", "args": "cr6, r11, 0x2c"}, "base": {"opcode": "cmpwi", "args": "cr6, r11, 0x2d"}},
{"target": {"opcode": "ble"}, "base": {"opcode": "blt"}}
```

**What's happening:** `>= 0x2D` compiled as `cmpwi 0x2d; bge` vs `> 0x2C` as `cmpwi 0x2c; bgt`

**Fix:** Try both comparison styles - they're semantically equivalent but compile differently.

---

## Fixability Decision Tree

```
Start: Analyze non-equal instructions
│
├─ ALL are diff_arg?
│   ├─ Contains merged_* or OnlyReturns? → UNFIXABLE (linker limit)
│   ├─ All register swaps? → TRY variable reordering (30% success)
│   └─ All branch offsets? → UNFIXABLE (code layout)
│
├─ Has diff_op or replace?
│   ├─ Branch instructions? → TRY control flow changes (70% success)
│   ├─ Comparison instructions? → TRY comparison style (50% success)
│   ├─ Load/store instructions? → TRY type changes (40% success)
│   └─ Arithmetic? → CHECK algorithm/formula (60% success)
│
├─ High delete count (>10% of instructions)?
│   ├─ clrlwi for bool? → Likely UNFIXABLE (compiler bool handling)
│   └─ Other patterns? → CHECK for unnecessary operations
│
└─ High insert count (>10% of instructions)?
    └─ CHECK for missing operations in decomp
```

---

## Workflow: Fix Loop

The standard workflow for improving a near-match function:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. GET STATUS & VERDICT (single command!)                   │
│    objdiff-cli diff -p . "MyClass::MyFunc" -f json --verdict│
│    → Check verdict.classification                           │
│    → If AT_LIMIT → STOP, move on to another function        │
│    → If LIKELY/MAYBE_FIXABLE → Continue to step 2           │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. REVIEW PATTERNS & SUGGESTIONS                            │
│    → Check analysis.patterns for what's causing mismatches  │
│    → Check verdict.suggestions for recommended actions      │
│    → For details: add --include-instructions to see diffs   │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. EDIT SOURCE                                              │
│    → Apply fix based on pattern/suggestion                  │
│    → LINKER_MERGED: Nothing to do (unfixable)               │
│    → BOOL_MASK: Usually nothing to do (unfixable)           │
│    → REGISTER_SWAP: Try reordering variable declarations    │
│    → Control flow: Try restructuring if/else                │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. BUILD & CHECK                                            │
│    ninja build/.../File.obj && ninja build/.../report.json  │
│    → Re-run step 1 to check new match %                     │
│    → If improved: continue or done                          │
│    → If same/worse: revert, try different approach          │
└─────────────────────────────────────────────────────────────┘
                            ↓
              (repeat from step 1 or give up)
```

**Note:** The diff command now accepts demangled names like `"MyClass::MyFunc"` - no need to look up mangled names first!

### When to Give Up

Stop trying after:
- 3+ different approaches with no improvement
- All remaining diffs are `diff_arg` to merged functions
- All remaining diffs are register allocation that doesn't respond to reordering
- Match is 97%+ and only `clrlwi` bool mask differs

Mark as "at limit" and move on.

---

## Troubleshooting

### "Symbol not found" Error

**Problem:** `objdiff-cli diff -p . "SomeFunction"` fails with "Symbol not found"

**Possible causes:**
1. Function name is ambiguous (exists in multiple units)
2. Typo in function name
3. Function doesn't exist in target objects

**Solutions:**

```bash
# The diff command now supports demangled names!
objdiff-cli diff -p . "RndMat::GetRefractEnabled" -f json  # Works!

# If ambiguous, specify the unit:
objdiff-cli diff -p . -u src/system/rndobj/Mat "GetRefractEnabled" -f json

# Or use more specific name:
objdiff-cli diff -p . "RndMat::GetRefractEnabled" -f json  # More specific

# To find available matches:
objdiff-cli report function build/.../report.json "GetRefractEnabled"
```

**Note:** Both mangled (`?GetRefractEnabled@RndMat@@...`) and demangled (`RndMat::GetRefractEnabled`) names now work with the diff command.

---

### jq Escaping Issues

**Problem:** `jq 'select(.match_type != "equal")'` fails with syntax error

**Cause:** Bash escapes `!` in double quotes (history expansion)

**Solutions:**
```bash
# Option 1: Use single quotes for the whole expression
jq 'select(.match_type != "equal")'

# Option 2: Use == with negation
jq 'select(.match_type == "equal" | not)'

# Option 3: Use test() for pattern matching
jq 'select(.match_type | test("diff|replace|delete|insert"))'
```

---

### Checking Size Similarity

Before deep-diving into instruction diffs, check if sizes are similar:

```bash
objdiff-cli diff -p . "SYMBOL" -f json | jq '{target_size, base_size, diff: (.target_size - .base_size)}'
```

**Interpretation:**
- Sizes within ~5%: Likely same structure, optimization differences
- Sizes very different (>20%): Likely structural/logic differences, more fixable
- Target larger: Decomp has extra code (check for unnecessary operations)
- Base larger: Decomp missing code (check for missing operations)

---

### RB3 Reference Lookup

For shared engine code, check Rock Band 3 decomp for reference:

```bash
# Search RB3 for equivalent function
grep -rn "FunctionName" ~/code/milohax/rb3/src/

# Compare implementations
diff <(sed -n 'START,ENDp' dc3/src/file.cpp) <(sed -n 'START,ENDp' rb3/src/file.cpp)
```

**When useful:**
- System code (`system/rndobj/`, `system/obj/`, `system/math/`)
- Functions with same signature in both projects
- Understanding expected patterns/idioms

**When NOT useful:**
- DC3-specific code (`lazer/`, `hamobj/`)
- Different game logic even if similar names

---

## Useful Command Recipes

### Recipe 1: Quick Fixability Assessment (Automated)

```bash
# Use --verdict for automated triage (RECOMMENDED)
objdiff-cli diff -p . "SYMBOL" -f json --verdict | jq '{
  match: .fuzzy_match_percent,
  verdict: .verdict.classification,
  patterns: [.analysis.patterns[].pattern],
  recommendation: .verdict.recommendation
}'
```

**Manual alternative** (if you need raw counts):
```bash
objdiff-cli diff -p . "SYMBOL" -f json --summary | jq '.instruction_summary'
```

**Interpretation guide:**
- `AT_LIMIT` → Don't waste time, move on
- `LIKELY_FIXABLE` → Check control flow (if/else structure)
- `MAYBE_FIXABLE` → Try reordering variable declarations
- `NEEDS_INVESTIGATION` → Manual analysis required

---

### Recipe 2: Find All Mismatches

```bash
# List all non-matching instructions
objdiff-cli diff -p . "SYMBOL" -f json --include-instructions | \
  jq '[.instructions[] | select(.match_type != "equal")]'
```

---

### Recipe 3: Detect Linker-Merged Functions (Automated)

```bash
# Use --analyze for automated detection (RECOMMENDED)
objdiff-cli diff -p . "SYMBOL" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "LINKER_MERGED") | .details.merged_functions'
```

**Manual alternative:**
```bash
objdiff-cli diff -p . "SYMBOL" -f json --include-instructions | \
  jq '[.instructions[] | select(.match_type == "diff_arg" and .target.opcode == "bl" and (.target.args | test("merged|OnlyReturns")))]'
```

---

### Recipe 4: Find Bool Mask Issues (Automated)

```bash
# Use --analyze for automated detection (RECOMMENDED)
objdiff-cli diff -p . "SYMBOL" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "BOOL_MASK")'
```

**Manual alternative:**
```bash
objdiff-cli diff -p . "SYMBOL" -f json --include-instructions | \
  jq '[.instructions[] | select(.target.opcode == "clrlwi" or .base.opcode == "clrlwi")]'
```

---

### Recipe 5: Analyze Register Allocation (Automated)

```bash
# Use --analyze for automated detection (RECOMMENDED)
objdiff-cli diff -p . "SYMBOL" -f json --analyze | jq '
  .analysis.patterns[] | select(.pattern == "REGISTER_SWAP") | .details.swaps'
```

**Manual alternative:**
```bash
objdiff-cli diff -p . "SYMBOL" -f json --include-instructions | \
  jq '[.instructions[] | select(.match_type == "diff_arg" and (.target.opcode | test("mr|lwz|stw|lbz|stb")))]'
```

---

### Recipe 6: Quick Status Check

```bash
# One-liner to check function match %
objdiff-cli report function build/45410914/report.json "FuncName" | jq '.matches[0].fuzzy_match_percent'
```

---

## Example Outputs & Analysis

### Example 1: Function at Linker Limit (LoadOld 97.0%)

**Command:**
```bash
objdiff-cli diff -p . "?LoadOld@RndMat@@IAAXAAVBinStreamRev@@@Z" -f json --include-instructions | \
  jq '[.instructions[] | select(.match_type == "diff_arg" and .target.opcode == "bl")][:5]'
```

**Output:**
```json
[
  {
    "index": 16,
    "target": {"opcode": "bl", "args": "OnlyReturns"},
    "base": {"opcode": "bl", "args": "?CheckBlendMode@@YA?AW4Blend@BaseMaterial@@W412@PAV2@@Z"},
    "match_type": "diff_arg"
  },
  {
    "index": 20,
    "target": {"opcode": "bl", "args": "merged_Read4FloatStruct"},
    "base": {"opcode": "bl", "args": "??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z"},
    "match_type": "diff_arg"
  }
]
```

**Analysis:** All function calls are to linker-merged functions → UNFIXABLE

---

### Example 2: Function with Fixable Control Flow (MatShaderFlagsOK)

**Before fix - Match type distribution:**
```json
[
  {"type": "diff_arg", "count": 25},
  {"type": "equal", "count": 107},
  {"type": "replace", "count": 3}
]
```

**The `replace` instructions indicated control flow difference. After restructuring if-else:**

**After fix:**
```json
[
  {"type": "diff_arg", "count": 7},
  {"type": "equal", "count": 125}
]
```

**Result:** 95.3% → 98.2%

---

### Example 3: Bool Return Issue (GetRefractEnabled 97.1%)

**Command:**
```bash
objdiff-cli diff -p . "?GetRefractEnabled@RndMat@@QAA_N_N@Z" -f json --include-instructions | \
  jq '[.instructions[] | select(.match_type != "equal")]'
```

**Output:**
```json
[
  {"index": 30, "target": {"opcode": "li", "args": "r11, 0x1"}, "base": {"opcode": "li", "args": "r3, 0x1"}, "match_type": "diff_arg"},
  {"index": 32, "target": {"opcode": "li", "args": "r11, 0x0"}, "base": {"opcode": "li", "args": "r3, 0x0"}, "match_type": "diff_arg"},
  {"index": 33, "target": {"opcode": "clrlwi", "args": "r3, r11, 24"}, "match_type": "delete"}
]
```

**Analysis:**
- Decomp loads into r11, then masks with clrlwi to r3
- Original loads directly into r3
- This is compiler bool-return handling → UNFIXABLE without signature change

---

## Tool Improvement Ideas

See [OBJDIFF_WISHLIST.md](OBJDIFF_WISHLIST.md) for proposed objdiff-cli enhancements that don't exist yet.

---

## Parallel Agent Strategy

For batch processing multiple functions, launch parallel agents:

```bash
# Find 4 near-match targets
objdiff-cli report query build/.../report.json --functions \
  --min-percent 95 --max-percent 99 --limit 4

# Launch agents in parallel (Claude Code example)
# Each agent gets: function name, file path, current %, objdiff commands
```

**Agent prompt template:**
```
Fix FUNCTION_NAME (currently XX.X%) in FILE_PATH.

Commands:
- Check: objdiff-cli report function build/.../report.json "FUNCTION"
- Diff: objdiff-cli diff -p . "MANGLED_SYMBOL" -f json --include-instructions
- Build: ninja build/.../File.obj && ninja build/.../report.json

Patterns that work: [link to OBJDIFF_LEARNINGS.md]
Report final match % when done.
```

**Best practices:**
- Give each agent ONE function (avoid conflicts)
- Include the mangled symbol name in the prompt
- Set clear success criteria (100% or "confirmed at limit")
- Have agents report what they tried (for learning)

### Task-Based Parallel Agents (Tested 2026-01-23)

Instead of one agent per function, assign agents different *task types*:

| Agent | Task | Outcome |
|-------|------|---------|
| Agent 1 | Implement unimplemented functions via m2c | Added templates, found m2c limitations |
| Agent 2 | Fix sub-90% functions (likely bugs) | **PostWaitJump 88→99.9%** |
| Agent 3 | Identify near-complete units | Found 13 units ready for closeout |

**Key insight:** Sub-90% functions often have real logic errors (fixable), while 97%+ functions are usually at compiler limit (accept as-is).

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────┐
│                    OBJDIFF QUICK REFERENCE                  │
├─────────────────────────────────────────────────────────────┤
│ CHECK STATUS                                                │
│   objdiff-cli report function build/.../report.json "Func"  │
├─────────────────────────────────────────────────────────────┤
│ GET DIFF (supports demangled names!)                        │
│   objdiff-cli diff -p . "MyClass::MyFunc" -f json           │
├─────────────────────────────────────────────────────────────┤
│ AUTOMATED TRIAGE (RECOMMENDED)                              │
│   objdiff-cli diff -p . "Func" -f json --verdict            │
│   → Returns: classification, patterns, recommendation       │
├─────────────────────────────────────────────────────────────┤
│ PATTERN ANALYSIS                                            │
│   objdiff-cli diff -p . "Func" -f json --analyze            │
│   → Detects: LINKER_MERGED, BOOL_MASK, REGISTER_SWAP        │
├─────────────────────────────────────────────────────────────┤
│ INSTRUCTION SUMMARY                                         │
│   objdiff-cli diff -p . "Func" -f json --summary            │
│   → Counts: equal, diff_arg, diff_op, replace, del, ins     │
├─────────────────────────────────────────────────────────────┤
│ VERDICT CLASSIFICATIONS                                     │
│   COMPLETE         → 100% match, done                       │
│   LIKELY_FIXABLE   → Has control flow diffs, worth trying   │
│   MAYBE_FIXABLE    → Register swaps, try var reordering     │
│   AT_LIMIT         → Merged funcs or bool mask, move on     │
│   NEEDS_INVESTIGATION → Mixed signals, manual analysis      │
└─────────────────────────────────────────────────────────────┘
```

---

## Mining Logs for Patterns

When reviewing session logs, look for these indicators:

### Success Indicators
- "improved from X% to Y%"
- "Changed [code pattern] to [code pattern]"
- Match percentage increased after edit

### Failure Indicators
- "No change" or "Worse" after edit
- "reverted" or "revert"
- Multiple attempts on same function without improvement

### Pattern Discovery
- "The diff showed..." followed by JSON
- "This indicates..." or "This suggests..."
- "The original uses X while decomp uses Y"

### Tool Usage Patterns
- Which jq filters are used repeatedly?
- What information is extracted most often?
- Where does the user need to run multiple commands to get full picture?

---

## Appendix A: PowerPC Instruction Quick Reference

| Instruction | Meaning | Relevance to Decomp |
|-------------|---------|---------------------|
| `bl` | Branch and Link (function call) | Check for merged functions |
| `blr` | Branch to Link Register (return) | Function epilogue |
| `clrlwi` | Clear Left Word Immediate | Bool masking (bits 0-23 cleared) |
| `mr` | Move Register | Register allocation |
| `li` | Load Immediate | Constant values |
| `lis` | Load Immediate Shifted | Upper 16 bits of address |
| `addi` | Add Immediate | Address calculation, counters |
| `cmpwi` | Compare Word Immediate (signed) | Signed comparisons |
| `cmplwi` | Compare Logical Word Immediate (unsigned) | Unsigned comparisons |
| `beq/bne/blt/bgt/ble/bge` | Conditional branches | Control flow |
| `lwz/stw` | Load/Store Word (32-bit) | Memory access |
| `lbz/stb` | Load/Store Byte | Often for bool/char |
| `lfs/stfs` | Load/Store Float Single | Float operations |
| `mflr/mtlr` | Move from/to Link Register | Function prologue/epilogue |
| `stwu` | Store Word with Update | Stack frame setup |
| `cntlzw` | Count Leading Zeros Word | Often for `== 0` checks |
| `extrwi` | Extract and Right Justify | Bit field extraction |
| `srwi` | Shift Right Word Immediate | Division by power of 2 |

---

## Appendix B: Code Patterns That Affect Matching

### Patterns That Often Help

```cpp
// Combine conditions (reduces branches)
if (a && b) { ... }  // Better than nested if

// Ternary for simple assignments
x = cond ? val1 : val2;  // Often matches better than if-else

// Explicit casts for type mismatches
mPointLights = (bool)(x == 1);  // When you see clrlwi in BASE

// Separate counter variables
for (int i = 0; i < n; i++) {
    if (filter(items[i])) {
        output[outIdx++] = items[i];  // Separate outIdx, not reusing i
    }
}
```

### Patterns That Often Hurt

```cpp
// Local bool variable for return (usually worse)
bool ret = false;
if (cond) ret = true;
return ret;  // Generates extra instructions

// Unnecessary intermediate variables
RndTex* tex = ptr ? ptr : fallback;
if (tex) { ... }  // Sometimes worse than inline

// Max/Min chains (use Clamp instead)
x = Max(min_val, Min(max_val, x));  // Use Clamp(min, max, x)
```

### Version Comparison Equivalents

These are semantically equivalent but compile differently:
```cpp
if (rev >= 0x2D) { ... }  // cmpwi 0x2d; bge
if (rev > 0x2C) { ... }   // cmpwi 0x2c; bgt

if (rev < 0x2D) { ... }   // cmpwi 0x2d; blt
if (rev <= 0x2C) { ... }  // cmpwi 0x2c; ble
```

Try both styles when you see comparison mismatches.
