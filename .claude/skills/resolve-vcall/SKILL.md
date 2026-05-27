---
name: resolve-vcall
description: Resolve a virtual function call through a sub-object vtable. Given (class, sub_object_offset, vtable_slot), identifies the actual function being called. Use when target assembly loads a vtable from (this+offset) and calls slot N.
argument-hint: "<class_name> <sub_object_offset> <vtable_slot>"
allowed-tools: Bash(python3 scripts/dump_vtable.py *), Read, Grep, Glob
---

# Resolve Virtual Call Skill

Resolve virtual function calls through sub-object pointers in MSVC PPC vtables.

## Arguments

`$ARGUMENTS` — three space-separated values:
- `class_name` — Most-derived class (e.g., `CharServoBone`)
- `sub_object_offset` — Byte offset from this-ptr to vtable load (e.g., `8`, `0x8`)
- `vtable_slot` — Slot index in the vtable (e.g., `1`). If >= 100, treated as byte offset and divided by 4.

## Steps

1. **Run the resolver**:
   ```bash
   python3 scripts/dump_vtable.py resolve $ARGUMENTS
   ```

2. **Present the results**:
   - **Resolved function**: The actual function that will be called
   - **Vtable symbol**: The MSVC mangled vtable name (encodes which base class)
   - **Base name**: Which base class's vtable is at this sub-object offset
   - **Confidence**: high (direct match), medium (ICF-merged symbol, positional), low (heuristic)
   - **All slots**: Full vtable dump for the matching sub-object

3. **If resolution fails**, the error will list available vtables with their sub-object offsets. Use this to find the correct offset.

## How It Works

1. Finds the .obj file containing the class's vtable
2. Enumerates all `??_7<class>@@6B<base>@@@` vtable symbols
3. Reads `??_R4` (RTTI Complete Object Locator) at the end of each vtable to get the authoritative sub-object offset
4. Matches the query offset to the correct vtable
5. Indexes into the matched vtable at the requested slot
6. Demangles and returns the target function

## Tips

- Sub-object offset comes from target assembly: `lwz rN, OFFSET(this_reg)` loading the vtable pointer
- Slot index comes from the vtable offset divided by 4: `lwz rN, SLOT_OFFSET(vtable_reg)` → slot = SLOT_OFFSET / 4
- ICF-merged entries (e.g., `OnlyReturns`) mean the function compiled to identical code as another — the positional name is the real function
- Primary vtable (offset 0) contains Hmx::Object virtuals; sub-object vtables contain base-specific virtuals
