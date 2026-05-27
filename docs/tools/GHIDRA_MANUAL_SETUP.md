# Ghidra Setup for rb3-xenon (No MCP)

GUI-only Ghidra setup. No pyghidra, no MCP server. For the full AI-assisted pipeline, see [GHIDRA.md](GHIDRA.md).

After setup you get ~69,000 named functions with full demangled signatures, VMX128 disassembly, and correct MSVC switch table recovery.

```
BEFORE:  void FUN_823486e0(undefined4 param_1, undefined4 param_2)
AFTER:   void __thiscall CharBonesMeshes::PoseMeshes(CharBonesMeshes *this)
```

## 1. Ghidra Fork

Stock Ghidra is missing two things for Xbox 360 MSVC binaries:

- **VMX128**: 77 Xbox 360 SIMD opcodes — without these, vector math functions show `<UNDEFINED>`
- **MSVC switch recovery**: Ghidra's PPC switch analysis assumes GCC patterns. Three bugs in `PowerPCAddressAnalyzer` cause it to miss MSVC-style jump tables ([#8963](https://github.com/NationalSecurityAgency/ghidra/issues/8963))

Use our fork: [github.com/freeqaz/ghidra](https://github.com/freeqaz/ghidra) (`master` branch). Pre-built zips on the [releases page](https://github.com/freeqaz/ghidra/releases), or build with `gradle buildGhidra`.

## 2. XEXLoaderWV

[github.com/zeroKilo/XEXLoaderWV](https://github.com/zeroKilo/XEXLoaderWV) — standard Ghidra extension install. Needs **JDK** (not JRE) to build from source:

```bash
cd XEXLoaderWV/XEXLoaderWV
JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
  $GHIDRA_INSTALL_DIR/support/gradle/gradlew -PGHIDRA_INSTALL_DIR=$GHIDRA_INSTALL_DIR
```

When importing the XEX, enable **Process .pdata** in the loader options (adds function entries from exception data).

## 3. Import Map File Symbols

> **Note for rb3-xenon:** There is no leaked linker map file for RB3. The steps below are DC3-specific. For rb3-xenon, use `tools/fingerprint_match.py autoid` instead to identify functions via string/callee cross-referencing against the Wii DC3 oracle.

The binary is stripped. The DC3 linker map file has ~79K real symbol names (not present in rb3-xenon).

### Quick path (headless, does everything)

```bash
./tools/ghidra/import-xex.sh
# Creates ghidra_projects/RB3Xenon/ with full analysis + all symbols applied
```

### Manual path (GUI)

Add `tools/ghidra/` to your Script Manager directories. Import `orig/45410914/default.xex`, run analysis, then run **ImportMapFile** (Import category) and select `orig/45410914/ham_xbox_r.map`.

The script does three passes:

| Pass | What | Result |
|------|------|--------|
| Create functions | `CreateFunctionCmd` at map addresses where auto-analysis missed | ~6K new |
| Rename symbols | Replace `FUN_` auto-names with real mangled names | ~27K renamed |
| Demangle signatures | `MicrosoftDemangler` applies full CC + return type + all params | ~53K signatures |

Save after. One-time operation.

## 4. String Search with Xrefs

[`SearchString.java`](../../tools/ghidra/SearchString.java) — finds strings matching a pattern and shows which functions reference each one. More useful than Ghidra's built-in string search because it resolves xrefs inline instead of making you click through each string.

```
GUI:      Script Manager > Search > SearchString (prompts for pattern)
Headless: ./tools/ghidra/search-string.sh "CharBones"
```

Example output:
```
  828a1234: "CharBones::PoseMeshes"
    <- CharBones::PoseMeshes (82348700)
    <- CharBones::Save (82348a20)

  828a5678: "CharBones.milo"
    <- DirLoader::OpenFile (82401200)
```

Also searches raw memory for ASCII patterns that Ghidra's auto-analysis didn't identify as strings (skips duplicates from the defined strings pass).

## Reading PPC Decompilation

### LZCOUNT = `!x`

```c
LZCOUNT(x) >> 5              // == !x
(uint)LZCOUNT(x) >> 5        // == !x
(ulonglong)(LZCOUNT(x) << 0x20) >> 0x25  // == !x (64-bit variant)
```

PowerPC `cntlzw` returns 32 for zero, 0-31 otherwise. `>> 5` maps that to 1/0. This is how MSVC compiles boolean negation on PPC.

### Merged Symbols = ICF

`merged_82XXXXXX` = the linker folded multiple functions with identical machine code to one address (Identical COMDAT Folding). Usually scalar/vector deleting destructors, trivial getters, or empty stubs. Resolve via map:

```bash
grep "82331360" orig/45410914/ham_xbox_r.map
```

### Switch as If-Else

Some switches still decompile as if-else chains despite our fork fix. Look for `bctr` in the listing — that's the jump table dispatch.

## Map File Reference

```
0005:000186e0  ?PoseMeshes@CharBonesMeshes@@QAAXXZ  823486e0 f  char:CharBonesMeshes.obj
^^^^           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^ ^  ^^^^
section        mangled name                           VA       f=func
```

Section `0005:` = `.text`. Addresses are absolute (base `0x82000000`).

```bash
grep "@CharBones@@" orig/45410914/ham_xbox_r.map   # all methods in a class
grep "823486e0" orig/45410914/ham_xbox_r.map        # symbol at address
```

## Struct Layouts

2,100+ class layouts in `struct_db.sqlite` (from annotated headers). Compare against `include/` when investigating offset mismatches in the decompiler.

## See Also

- [GHIDRA.md](GHIDRA.md) — MCP server, type seeding pipeline, CLI analysis tools
- [XEXLOADERWV.md](XEXLOADERWV.md) — loader internals and troubleshooting
- [Ghidra fork](https://github.com/freeqaz/ghidra) — VMX128 + switch fix
- [pyghidra-mcp fork](https://github.com/freeqaz/pyghidra-mcp) — MCP server with map file ingestion
