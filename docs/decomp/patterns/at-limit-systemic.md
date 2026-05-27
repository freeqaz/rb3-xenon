# Systemic AT_LIMIT Patterns

These are project-wide issues that cause regressions across many functions. Fixing them would require macro/header changes affecting many translation units.

## 1. `__FILE__` Path Difference (FIXED)
- **Was**: Decomp `__FILE__` expanded to `"src/system/obj/DirLoader.cpp"`, target had original build paths
- **Fix**: `WIBO_PATH_MAP` in `configure.py` remaps source paths to original Windows paths (`e:/lazer_build_gmc1/...`)
- **Result**: `__FILE__` strings now match original, `??_C@_` string literal hashes match (121/121)

## 2. LINKER_MERGED / ICF (Identical COMDAT Folding)
- **What**: Linker merges functions with identical machine code to a single address
- **Impact**: Call targets differ (`bl merged_XXXXXXXX` vs `bl ActualFunction`)
- **Common merged functions**: `MakeString` templates, `String` ctor, `_M_find`, template instantiations
- **Fix**: Unfixable - this is a linker optimization we cannot replicate
- **Lookup**: Use `mcp__orchestrator__lookup_merged_symbol` to identify what's at a merged address

## 3. `DoneLoading` → `OnlyReturns` ICF
- **What**: `DoneLoading` virtual function (returns void, does nothing) gets merged with `OnlyReturns` in target
- **Impact**: Functions calling `DoneLoading` show `bl OnlyReturns` in target vs `bl DoneLoading` in ours
- **Affects**: DirLoader destructor, LoadObjs, and any function calling DoneLoading on a Loader

## 4. FormatString Wrapper in MILO_NOTIFY (UNFIXABLE)
- **Target's MILO_NOTIFY**: Has a `FormatString` construction wrapping MakeString result (~4KB stack buffer)
- **Our MILO_NOTIFY**: Passes string directly via `MakeString` → `DebugNotifier::operator<<`
- **Impact**: Stack frame differences (e.g., SetupDir: 0x1190 vs 0x170)
- **Attempted fixes**:
  - `__forceinline` on MakeString templates: REGRESSED (87.2% from 96.7%) — inlines MakeString differently than target
  - FormatString wrapper in `DebugNotifier::operator<<`: REGRESSED (91.6%) — adds SECOND FormatString, doubling stack usage
- **Root cause**: Target compiler appears to optimize FormatString reuse across control flow paths in a way we can't reproduce
- **Status**: UNFIXABLE with current understanding

## 5. `_MemAllocTemp` vs `MemAlloc` in ChunkStream (FIXED)
- **Target**: ChunkStream's `operator new` uses `_MemAllocTemp`
- **Our build**: Used `MemAlloc` via `MEM_OVERLOAD` macro
- **Fix applied**: Replaced `MEM_OVERLOAD(ChunkStream, 0x31)` with explicit `operator new` using `_MemAllocTemp`
- **Result**: OpenFile improved 93.8% → 93.9%

## 6. `_MemAllocTemp` vs `MemAlloc` in ArkFile (FIXED)
- **Target**: ArkFile's `operator new` uses `_MemAllocTemp`
- **Fix applied**: Changed `MemAlloc` → `_MemAllocTemp` in ArkFile_p.h
- **Note**: No current `new ArkFile` call sites (allocation done via placement new in File.cpp), so no functional impact yet

## 7. Block Sinking / Cold Code Relocation (UNFIXABLE)
- **What**: 361 functions (1.5%) in the target have basic blocks placed AFTER the function return, with backward branches to the join point. Typically null-check patterns where the non-null block (vbase conversion, member loads) is "sunk" past the epilogue.
- **Pattern**: `bne cr6, [past_return]` → null-path falls through → epilogue → return → [non-null block] → `b [back_to_join]`
- **Affects**: FlowPtr<T>::operator= (all specializations with vbase: RndAnimatable, Sound, Flow, ObjectDir), STL algorithms, D3D shader code, error-return paths
- **Root cause investigation (c2.dll RE)**:
  - **PEEP branch pair reorder** (`FUN_10bacf2b` in c2.dll) reorders conditional/unconditional branch pairs but is gated behind `DAT_10c3de20 == 2` (PGO optimize mode)
  - `DAT_10c3de20` is set from `DAT_10c6f1c8` which is only set to 2 when PGO options (`-PogoSafeMode` etc.) are active
  - **Xenon scheduler** (`0x10b71d8f`) also has PGO-gated block layout code paths
  - Binary patching: forced PGO mode 2 (patched all 10 loads of DAT_10c6f1c8) → **no effect** — PGO code paths need actual profiling data (branch weights) to make different block layout decisions
  - Tested with /O1, /O2, /Ox, various source patterns (if/else, defaults+if, ternary, different complexity levels) — **identical output** in all cases
  - No PGO symbols (`__PogoProbeVector`, `__PogoRuntimeVector`) in target's `ham_xbox_r.map` — target was NOT compiled with standard PGO
  - No BBT section splitting in target binary (single `.text` section)
- **Possible explanations**: Different c2.dll build variant, linker-level BBT with branch trace data, or unknown compiler mechanism
- **Status**: UNFIXABLE — cannot reproduce block sinking with our compiler under any conditions
