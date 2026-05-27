---
name: vtable
description: Dump vtable layout for a class from original COFF .obj files. Maps each slot to the actual virtual function symbol, resolving ICF-merged entries. Use when debugging vtable offset mismatches in objdiff.
argument-hint: "[class-name] [--offset 0xNN]"
allowed-tools: Bash(python3 scripts/dump_vtable.py *), Bash(python3 -c *), Read, Grep, Glob
---

# Vtable Dump Skill

Dump vtable layouts from original .obj files to identify which virtual function is at each slot. Essential for debugging vtable offset mismatches where ICF merging makes symbol names misleading.

## Arguments

`$ARGUMENTS`

## Steps

1. **Parse the arguments.** The argument `$0` is a class name (e.g., `RndFontBase`). Optional `--offset 0xNN` to look up a specific slot.

2. **If an offset is specified**, look up the specific slot:
   ```bash
   python3 -c "
   import sys; sys.path.insert(0, 'scripts')
   from dump_vtable import lookup_vtable_offset
   entry = lookup_vtable_offset('$0', 0xNN)
   if entry:
       print(f'Slot {entry[\"slot\"]}: {entry[\"demangled\"]}')
       print(f'Symbol: {entry[\"symbol\"]}')
   else:
       print('Not found')
   "
   ```

3. **For full vtable dump**, run the script:
   ```bash
   python3 scripts/dump_vtable.py $0 --demangle
   ```

4. **Present the results** as a table mapping slot → offset → virtual function name.

5. **If the user has a vtable offset mismatch** (e.g., objdiff shows `lwz r12, 0x7c(r12)` vs `lwz r12, 0x6c(r12)`):
   - Look up both offsets to identify which virtual functions they correspond to
   - Explain which method the original code calls vs what the decomp calls
   - Suggest the fix (change the virtual method call in source)

## Tips

- ICF-merged entries (like `OnlyReturns` or `CharAdvance`) appear when multiple virtual functions have identical machine code. The slot position determines which function it actually is.
- Object base class virtuals occupy the first ~22 slots (dtor, RefOwner, Replace, ClassName, SetType, Handle, SyncProperty, InitObject, Save, Copy, Load, PreSave, PostSave, Print, Export, SetTypeDef, ObjectDef, SetName, DataDir, PreLoad, PostLoad, FindPathName)
- Use `--obj path/to/file.obj` if auto-detection doesn't find the right .obj file
- Vtable offsets are byte offsets (each slot is 4 bytes on PPC)

## When to Use

- objdiff shows `diff_arg` on `lwz` instructions with different offsets in vtable load patterns
- You need to identify which virtual function corresponds to a vtable slot
- ICF-merged symbols make it unclear which virtual function is being called
- Before/after changing virtual function calls to verify the slot is correct
