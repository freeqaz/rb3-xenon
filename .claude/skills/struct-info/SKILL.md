---
name: struct-info
description: Get class/struct layout info including members, offsets, parents, and inheritance chain. Also look up which field is at a specific offset. Use when debugging struct alignment or verifying class layouts. Asks the COMPILER (/d1reportSingleClassLayout) for ground truth; the struct DB is a fallback whose offsets come from hand-written header comments and are wrong in places.
argument-hint: "[class-name] [offset]"
allowed-tools: Bash(python3 scripts/harvest/class_layout_report.py *), Bash(python3 tools/struct_db.py *), Read, Grep, Glob
---

# Struct Info Skill

Get class/struct layout information. **Ask the compiler, not the comments.**

## ★ The authoritative source

`cl.exe /d1reportSingleClassLayout<Class>` is an undocumented MSVC flag that
**works through our wibo-wrapped X360 compiler**. It prints, for every class whose
name starts with `<Class>`: the real `sizeof`, every member at its real byte
offset nested by base class, explicit `<alignment member>` **padding** rows, the
vtable slot-by-slot with the class that supplies each slot, and the `this`
adjustor per virtual. Wrapped as:

```bash
python3 scripts/harvest/class_layout_report.py <Class> --exact --no-vtable --project-dir .
python3 scripts/harvest/class_layout_report.py <Class> --offset 0x118 --project-dir .   # which member is here?
python3 scripts/harvest/class_layout_report.py <Class> --project-dir .                  # + vtable + adjustors
python3 scripts/harvest/class_layout_report.py <Class> --check-header --project-dir .   # audit the // 0xHEX comments
python3 scripts/harvest/class_layout_report.py <Class> --tu src/foo/Bar.cpp --raw --project-dir .
```

★ **Always pass `--project-dir <your worktree>`** so the report reflects *your*
header edits, not main's.

## ⚠ Why the struct DB is only a fallback

`struct_db.sqlite` (and therefore `tools/struct_db.py` and the MCP
`lookup_struct_offset` tool) is parsed from the hand-written `// 0xHEX` comments
in our headers. Those comments are **measurably wrong** in several places —
`CharEyes.h` carried 20 wrong offsets, `SaveLoadManager.h`'s are uniformly +4
stale — and they never encoded vtables or `this` adjustors at all. Treat any
comment-derived offset as a hypothesis, never as ground truth. `lookup_struct_offset`
now consults the compiler by default and labels comment-derived answers
**UNVERIFIED**; if you see that banner, run the script above before acting.

## Arguments

`$ARGUMENTS`

## Steps

1. **Parse the arguments.** `$0` is a class name. Optional `$1` is an offset to look up a specific field.

2. **Ask the compiler first.**
   ```bash
   python3 scripts/harvest/class_layout_report.py $0 --exact --project-dir .
   # with an offset:
   python3 scripts/harvest/class_layout_report.py $0 --exact --offset $1 --project-dir .
   ```
   If it reports **no output**, the class is incomplete in the auto-resolved TU
   (forward-declared only, or all-inline and unreferenced — the `SaveLoadManager`
   case). Pass `--tu <a .cpp that really instantiates it>`. If the class is a
   template instantiation, drop `--exact` and read the mangled instantiation rows.

3. **Then cross-check the DB** (it carries the inheritance chain and the
   `#if`-gated layout forks, which the compiler report flattens away):
   ```bash
   python3 tools/struct_db.py info $0
   python3 tools/struct_db.py lookup $0 $1
   ```
   Where the two disagree, **the compiler is right** — and say so in your report,
   including file + line, so the stale comment gets fixed rather than re-read by
   the next lane (`--check-header` prints exactly this).

4. **Present the results** showing:
   - File path where the class is defined
   - Parent classes and inheritance chain
   - Member table with offsets, types, and names — marked VERIFIED vs UNVERIFIED
   - Padding runs and, for vtable/`this`-adjustor questions, those sections

## Layout truths already measured — reuse, don't re-derive

- **`vector<T*>` is 8 bytes in this tree** (STLport pointer specialisation). This
  is the root of uniform −4 shifts — **check it first on any −4 unit**.
- `MsgSource`'s non-virtual subobject is 0x18, with its tail pad reused by derived
  members.
- Retail sometimes **strips profiling members (`Timer`) entirely**.
- 8-byte `vector<T,unsigned short>` size-hint vectors exist in retail meta_band
  classes.
- **DC3 is newer than RB3**, so a DC3-only member tail must often be *dropped*.
- ★ **Reconstruct a layout from the retail CONSTRUCTOR** — it initialises every
  member in declaration order, so the init-value sequence pins the assignment 1:1.
  Corroborate with a tiny accessor that only goes byte-identical once the member
  moves.
- ★ A layout change is **fleet-wide**: every consumer of the class recompiles, so
  A/B whole-binary and be ready to revert — an isolated flip can be net-negative.

## Tips

- Offsets can be hex (`0x48`) or decimal (`72`)
- `/d1reportSingleClassLayout` is a **prefix** match: `...LayoutSynth` also reports
  `SynthSample`. Use `--exact` to filter to one class.
- The report costs one TU compile (~10–60 s) and writes its `.obj` to a scratch
  temp, so it is safe to run while a build is in flight in the same worktree.
- If the struct DB is missing, rebuild it: `python3 tools/struct_db.py build src/`
  (but remember it only re-parses the same possibly-wrong comments)
- Use `python3 tools/struct_db.py list --pattern '*Rnd*'` to search for classes by name pattern
- Use `/dc3-pair` to find the DC3 source as a cross-reference (same MSVC compiler, same flags — best oracle for engine classes)
- Compare with `/ghidra-struct` to cross-check against Ghidra's analysis of `orig/45410914/default.xex`
