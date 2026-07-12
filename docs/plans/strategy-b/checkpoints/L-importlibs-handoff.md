# Lane L — Import Libraries (XDK-free) — HANDOFF

**Status: PROVEN.** XDK-free import libs for RB3Enhanced.dll are reconstructed,
generated, and verified end-to-end (compile → link → valid PPCBE PE with a
correct import table).

Artifacts dir: `/home/free/code/milohax/rb3-xenon/tools/oss-xbox-build/L-importlibs/`

## What the stock DLL actually imports (GROUND TRUTH)

Dumped from the stock 0.7 `RB3Enhanced.dll` (itself an **XEX2 DLL-module**) with a
standalone build of idaxex `xex1tool -i`. Full dump: `stock_imports.txt`.

**Only TWO host modules** — not the four the XDK Makefile lists:

| Module | # imports | Notes |
|---|---|---|
| `xam.xex` | 44 | ALL Net/XNet/Upnp/Xam. NetDll_* sockets @1-27, XNet @51-78, Upnp @251-264, Xam @420-714 |
| `xboxkrnl.exe` | 55 | Nt*/Rtl*/Ke*/Ex*/Ob*/Xe*/Xex* kernel |

Kernel version: target **2.0.21256.0**, min 2.0.1861.0 (ordinals are version-keyed;
these are correct for the exact DLL we replace).

**Key structural finding:** the XDK build links `xapilib.lib + xboxkrnl.lib +
xnet.lib + xonline.lib`, but `xapilib/xnet/xonline` all **forward to `xam.xex`** at
runtime. So the OSS build needs just **two** import libs: `xam.lib` (→`xam.xex`) and
`xboxkrnl.lib` (→`xboxkrnl.exe`). They cover 100% of the real import surface.

Cross-checked every sampled ordinal against source (a) `xbox-reversing/x360_imports.py`
(xorloser namegen) — **(a) and (b) agree exactly.**

## Deliverables

- `xam.def` — `LIBRARY xam.xex`, 44 exports (name @ordinal)
- `xboxkrnl.def` — `LIBRARY xboxkrnl.exe`, 55 exports
- `xam.lib`, `xboxkrnl.lib` — the import archives (+ `.exp`)
- `xex1tool` — standalone idaxex import-dumper (g++ build), reusable
- `verify_imports.{c,obj,dll}` — the verification TU + its linked PE

## Verification (PROVEN)

`verify_imports.c` is XDK-free (declarations only) and calls 5 imports across both
modules. Compiled with `cl.exe`, linked against both generated libs → a valid PE:

- machine **0x01F2** (IMAGE_FILE_MACHINE_POWERPCBE), MZ/PE intact
- import descriptors: `xam.xex → {51, 642}`, `xboxkrnl.exe → {3, 300, 407}`
- every ordinal matches the ground-truth dump

## Recipes (all under `wibo`)

```
WIBO=/home/free/code/milohax/wibo/build/release/wibo
X360=/home/free/code/milohax/rb3-xenon/build/compilers/X360/16.00.11886.00

# dump stock imports (ground truth)
./xex1tool -i .../_rb3e07/rb3e07/RB3Enhanced.dll

# generate an import lib from a .def
$WIBO $X360/link.exe /LIB /MACHINE:PPCBE /DEF:xam.def /OUT:xam.lib

# compile an XDK-free TU
$WIBO $X360/cl.exe /c /nologo /X /Foverify_imports.obj verify_imports.c

# link a PPCBE PE DLL against the OSS import libs
$WIBO $X360/link.exe -nologo -dll -MACHINE:PPCBE -entry:DllMain -NODEFAULTLIB \
      -XEX:NO -FIXED:NO -OUT:verify_imports.dll verify_imports.obj xam.lib xboxkrnl.lib
```

Regenerate the `.def`s from the dump reproducibly with the parser used here (module
lines matched as `# <name>.(xex|exe) v...`, ordinal lines as `N) Symbol`).

## IMPORTANT: wibo patch (blocker for ALL OSS import linking)

`link.exe` calls **`GetTempPathW`** the moment it builds a *real* import table.
The old probe (`dlltest.c`) never hit this because it **calls no imports** — so the
import-table code path was never exercised. Any TU that actually references imports
aborts with `wibo: call reached missing import GetTempPathW`.

Fixed by adding `GetTempPathW` + `GetTempFileNameW` (thin wrappers over the existing
A variants + `stringToWideString`) to wibo:
- `/home/free/code/milohax/wibo/dll/kernel32/fileapi.cpp`
- `/home/free/code/milohax/wibo/dll/kernel32/fileapi.h`

Rebuilt `wibo/build/release/wibo` (`cmake --build build/release --target wibo`; the
header change auto-regenerates the kernel32 thunk table). **Change is additive and
uncommitted.** If a fresh wibo is built elsewhere without this patch, OSS import
linking will fail — carry this patch forward (or commit it in the wibo repo).

## Residual / next lane

- This lane proves the **import-lib mechanism and ordinals**; it does not pack to
  XEX. Boot validation is the packer lane's job.
- Verified 5 representative ordinals; the `.def`s contain the complete 99-symbol
  ground-truth set, so a full-DLL link is covered by construction (do it in the
  link/pack lane once all RB3E TUs compile).
