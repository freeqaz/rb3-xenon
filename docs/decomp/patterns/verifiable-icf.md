# Verifiable Patterns: ICF (Identical COMDAT Folding)

These patterns are caused by linker ICF merging identical function bodies after compilation. We compare at the object file level, so the call target mismatch is inherent to our comparison method.

**Note:** The target binary is a debug build (no LTCG), but ICF is still enabled.

**Action:** Use `objdiff-cli diff --analyze --verdict` to confirm LINKER_MERGED is present, then use `merged-symbols` / `lookup_merged_symbol` to verify your call target is in the merged set. If verified, accept at_limit. If not in set, investigate — your code may be calling the wrong function.

---

## Linker Merged (ICF)

**Prevalence:** ~350 AT_LIMIT functions with LINKER_MERGED as dominant blocker (avg ~96%)
**Typical Gap:** 0.5-3%

The linker merges identical function bodies at link time (Identical COMDAT Folding).

### Symptom

objdiff shows `bl` instructions to `merged_*` or `OnlyReturns` functions.

### Why It Shows as a Mismatch

- Original binary was linked with `/OPT:ICF` (enabled even in debug builds)
- Identical function bodies are merged to the same address
- We compare at object level, before linking
- Different call targets = permanent mismatch in objdiff
- **But**: tooling can verify whether your code is correct

### Known Merged Functions

```
merged_Read3FloatStruct  - Vector3 stream reads
merged_Read4FloatStruct  - Color/Vector4 stream reads
merged_SetObjConcrete    - ObjRef template instantiations
merged_DataArrayNode     - DataArray access patterns
merged_ObjDirPtr->       - ObjDirPtr operations
OnlyReturns              - Functions that just return a value
```

### Detection

Check for `bl` instructions to `merged_*` in objdiff output:

```
| bl merged_Read4FloatStruct | bl ReadColor |
```

**For address-based merged symbols** (e.g., `merged_82331360`):
```bash
# Look up what symbols share that address
./bin/merged-symbols 82331360

# MCP tool (for agents)
mcp__orchestrator__lookup_merged_symbol address="82331360"
```

**Finding merged functions in decomp.db:**
```sql
-- All functions flagged with ICF-merged calls
SELECT symbol, current_percent
FROM functions
WHERE has_linker_merged = 1
  AND excluded = 0
ORDER BY current_percent DESC;

-- Count: ~1,572 functions currently flagged
```

### Verification Workflow

**IMPORTANT**: Don't blindly accept LINKER_MERGED as unfixable. Verify first:

0. **Confirm the verdict/pattern** - Run `objdiff-cli diff --analyze --verdict` and ensure LINKER_MERGED is actually present.
1. **Identify your call target** - What function does YOUR code call?
2. **Look up the merged address** - What symbols share that address?
3. **Check membership**:
   - **Your symbol IS in the set** → Correct code, accept at_limit
   - **Your symbol is NOT in the set** → Investigate! You may be calling the wrong function

**Example:**
```
objdiff shows: bl merged_82331360
Your code has: bs >> color;  // calls operator>>(BinStream&, Color&)

Step 1: ./bin/merged-symbols 82331360
Output:
  1. operator>>(BinStream&, Color&)
  2. operator>>(BinStream&, Rect&)

Step 2: Your call target IS in the set
Result: Code is correct, accept at_limit
```

### Real Examples

| Function | Match | Pattern |
|----------|-------|---------|
| UIList::StartScroll | 99.83% | merged_825F9518, merged_ObjDirPtr |
| CharCuff::Load | 98.3% | merged_Read4FloatStruct |

### What To Do

After verification:
- **Verified correct**: Report status as `at_limit`, document the merged function
- **Not in set**: Investigate what function you SHOULD be calling
- **Named merged** (e.g., `merged_Read4FloatStruct`): Usually obvious, verify anyway

---

## MakeString Array-Size ICF (Resolved)

**Status:** Resolved in objdiff (2026-03-03). No longer causes match% loss.

`MakeString<char[N], int, char[M]>` template instantiations produce identical machine code regardless of N/M (arrays decay to pointers). The original linker ICF-merged all variants to one address, but our pre-link `.obj` files reference per-file instantiations with different array sizes.

### Previous Impact

- **2,550+ functions** across **546 units** had `bl` `diff_arg` mismatches
- Each mismatch cost ~1-5% per function

### Solution

objdiff's `reloc_eq()` now normalizes MSVC mangled array sizes before comparing symbol names. Two symbols like:
- `??$MakeString@$$BY07$$CBDH$$BY0CD@$$CBD@@...` (char[8], int, char[36])
- `??$MakeString@$$BY07$$CBDH$$BY0BJ@$$CBD@@...` (char[8], int, char[26])

...are recognized as equivalent because they differ only in array dimension sizes (`Y<digit><size>` patterns).

This is distinct from the [MakeString type mismatch](fixable-casting.md#makestring-template-type-mismatch-milo-macro-arguments) pattern, which involves different argument **types** (e.g., `Symbol` vs `const char*`) and requires source-level fixes.

### Details

See [../../plans/MAKESTRING_ICF_EQUIVALENCE.md](../../plans/MAKESTRING_ICF_EQUIVALENCE.md) for full implementation details.

---

## LTCG/Global Pooling

> **Note:** This pattern likely does NOT apply to DC3. The target binary is a debug build
> without LTCG. Keeping this section for reference in case it's needed for other projects.

**Prevalence:** Varies (retail builds with LTCG)
**Typical Gap:** 0.5-1%

Link-Time Code Generation pools addresses differently than object-level compilation.

### Symptom

Extra `lis` instructions for address loading in our build.

### Why Unfixable

Only applies to builds with `/GL` + `/LTCG`:
- Cross-module inlining
- Global data address pooling at link time
- Dead code elimination across translation units

### Detection

Compare instruction counts for `lis` (load immediate shifted):

```
Target: 15 instructions
Decomp: 17 instructions (2 extra lis)
```

### What To Do

Accept the 0.5-1% gap as permanent (if applicable).

---

## Float Constant Pooling

> **Note:** This pattern likely does NOT apply to DC3. The target binary is a debug build
> without LTCG. Keeping this section for reference in case it's needed for other projects.

**Prevalence:** Common (retail builds with LTCG)
**Typical Gap:** 1-2 instructions

The linker places float literals adjacent to static arrays for optimized addressing.

### Symptom

Different base register for float/static array access.

### Why Unfixable

Original linker (with LTCG):
```asm
# Single base for floats and static data
addi r29, r10, base@l
lfs f0, 0x0(r29)      ; float constant
addi r7, r29, 0x8     ; gRevs[0]
```

Our build:
```asm
# Separate addresses
lis r11, float1@ha
lfs f0, float1@l(r11)
lis r10, gRevs@ha
addi r7, r10, gRevs@l
```

### Detection

Look for `lis` instruction count differences near float literal loading.

### What To Do

Accept 1-2 instruction differences as permanent (if applicable).

---

## See Also

- [unfixable-compiler.md](unfixable-compiler.md) - Compiler patterns (some fixable, some not)
- [../TECHNICAL_NOTES.md](../TECHNICAL_NOTES.md#known-unfixable-issues) - Full technical details
