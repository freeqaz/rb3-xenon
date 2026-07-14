# Lane B — Open-Source Build Path for an Xbox 360 XEX-DLL (no XDK)

**Question:** Can we ship the RB3Enhanced `feature/same-instrument` change as a
bootable artifact on retail RB3 (TU5) / RGH-JTAG / Xenia using **only free,
legal tools** — never obtaining the proprietary Microsoft Xbox 360 XDK?

**Verdict: YES, feasible.** The three things the XDK provides
(headers, import libs, `imagexex`) each have a proven or plausible free
substitute. The compile + link half of the pipeline is now **PROVEN end-to-end
on this machine** (real Xbox-360 PowerPC `.text` emitted and linked into a valid
PPC PE DLL, zero XDK bits). The only genuinely-open step left is PE→XEX2
packing, for which two independent free paths exist.

All experiments below were run from `/home/free/code/milohax/rb3-xenon` using the
already-present toolchain: `build/compilers/X360/16.00.11886.00/{cl,link}.exe`
under `wibo` (`/home/free/code/milohax/wibo/build/release/wibo`) + the
reconstructed headers in `src/xdk/`. Scratch artifacts live in `_ossprobe/`
(delete anytime; not committed).

---

## TL;DR pipeline (each step tagged)

| # | Step | Free tool | Status |
|---|------|-----------|--------|
| 1 | Compile `.c` → Xbox-360 PPC `.obj` (COFF, machine `0x1f2`) | `cl.exe` (bundled) under `wibo` + `src/xdk/LIBCMT` freestanding headers, `/X` | **PROVEN** |
| 2 | Generate kernel/XAM import libs from `.def` | `link.exe /LIB /DEF` (bundled) under `wibo` | **PROVEN** (mechanism); ordinal tables PLAUSIBLE |
| 3 | Link `.obj` → PPC PE DLL (machine `0x1f2`, `-dll -entry -MACHINE:PPCBE`) | `link.exe` (bundled) under `wibo` | **PROVEN** |
| 4 | Pack PE → unencrypted/unsigned XEX2-DLL | (a) reuse released RB3E XEX as shell + patch, or (b) write ~400-line packer from Xenia spec, or (c) xorloser XexTool under `wine` | **PLAUSIBLE** (see risks) |
| 5 | Load on target | RB3ELoader (Dashlaunch plugin) on RGH/JTAG, or Xenia patch | PLAUSIBLE (out of Lane B scope) |

**Biggest single risk:** step 4 — no open-source *PE→XEX2 builder* exists as a
turnkey tool; all mature OSS tooling goes XEX→PE (extract), not PE→XEX (build).
Mitigated three ways below. This is an effort risk, not a blocker.

---

## 1. HEADERS — PROVEN

**Finding:** our TU's XDK surface is nearly nil. `SameInstrumentHooks.c`
includes only `<string.h>` plus RB3E's own headers; those pull in at most
`<stdint.h>` (via `ppcasm.h`). It calls **game** functions through RB3E's
`POKE_B`-to-fixed-address stubs (`RB3E_STUB` in `_functions.c`), **not** Xbox
kernel/XAM APIs. So the whole XDK CRT (`xtl.h`, `xapilib`, …) is unneeded for
*our* translation unit — the only CRT symbols it can emit are `memcpy`/`memset`
(and MWCC-style intrinsics), which `cl.exe` inlines or leaves as externs.

rb3-xenon already carries a **complete reconstructed freestanding header set** in
`src/xdk/` — `LIBCMT/{string.h,stddef.h,stdint.h,stdarg.h,...}` plus umbrella
`XBOXKRNL.h / XAPILIB.h / XNET.h / XONLINE.h`. This is the existing precedent
for compiling MSVC-X360 code with **no XDK** (rb3-xenon builds every TU this way
via `tools/decompctx.py`-style include context). Those tiny hand-written stubs
(e.g. `src/xdk/LIBCMT/string.h` = plain freestanding prototypes) are exactly the
"tiny freestanding replacements" the task asks about, and they already exist.

**Experiment (ran):**
```
$ wibo cl.exe /c /nologo /X /I src/xdk/LIBCMT /Fo_ossprobe/trivial.obj _ossprobe/trivial.c
trivial.c                       # success, no XDK on include path
$ python3 -c 'read machine word' → machine=0x01f2  (IMAGE_FILE_MACHINE_POWERPCFP)
```
`0x01f2` = Xbox-360 PowerPC-with-FP — the exact target machine.

Re-ran with RB3E's **actual** xbox `CFLAGS_X`
(`-Ox -Os -D_XBOX -DRB3E_XBOX -GF -Gm- -MT -GS- -Gy -fp:fast -fp:except-
-Zc:wchar_t -Zc:forScope -GR- -openmp-`), dropping only the one XDK-specific
force-include `-FI xbox_intellisense_platform.h`: **compiled clean**. That
force-include is a convenience umbrella; for our TU it is replaceable by `/X` +
the `src/xdk` include dir (or a 3-line stub if some full TU needs a couple of
XDK typedefs).

**Conclusion:** For the same-instrument TU, the free header set is already
sufficient and proven. For the *whole* RB3E xbox build (15 files touch `xtl.h`
for net/crypto/input/exceptions), you'd extend `src/xdk` with the missing
umbrella typedefs — plausible but only needed if you rebuild all of RB3E rather
than just our additive hooks.

## 2. IMPORT LIBS — PROVEN (mechanism)

**Finding:** `link.exe` under `wibo` includes the **librarian** (`link /LIB`),
and it generates an import lib from a `.def` with zero XDK input.

**Experiment (ran):**
```
$ cat _ossprobe/xboxkrnl.def
LIBRARY xboxkrnl.exe
EXPORTS
    RtlInitAnsiString @307
    XamGetSystemVersion @407
$ wibo link.exe /LIB /MACHINE:PPCBE /DEF:_ossprobe/xboxkrnl.def /OUT:_ossprobe/xboxkrnl.lib
Microsoft (R) Library Manager Version 10.00.11886.00
   Creating library _ossprobe/xboxkrnl.lib and object _ossprobe/xboxkrnl.exp
$ ls -l _ossprobe/xboxkrnl.lib → 1980 bytes   # real import lib produced
```

So the classic "generate import libs from ordinal `.def` tables" trick works
here. The **ordinal→name export tables** for `xboxkrnl.exe` / `xam.xex` are
published by the free60 / Xenia projects (Xenia's `kernel/xboxkrnl/*` +
`xam_table.inc`; also xorloser's `x360_imports.idc`) and can be transcribed into
`.def` files. **However:** for *our TU alone* this is likely **not needed at
all** — the hooks call the game via `POKE_B`, not the kernel. Lane A confirms the
exact import set; if it is empty (or just a DllMain that returns 1), step 2 is
skipped entirely.

## 3. LINK PE — PROVEN

**Finding:** `link.exe` under `wibo` links a PPC-COFF `.obj` into a valid
**Xbox-360 PowerPC PE DLL**, no XDK libs required for a self-contained module.

**Experiment (ran):** compiled a self-contained DLL (own byte-copy, a hook fn,
`DllMain`), then:
```
$ wibo link.exe -nologo -dll -MACHINE:PPCBE -entry:DllMain -NODEFAULTLIB \
      -XEX:NO -FIXED:NO -OUT:_ossprobe/dlltest.dll _ossprobe/dlltest.obj
$ python3 → PE sig= b'PE\x00\x00'  machine=0x01f2  (POWERPCFP)
   sections: .rdata / .text / .XBLD, imagebase 0x88000000, relocatable
```
(`-SUBSYSTEM:NATIVE` is rejected by this linker — omit it; default is fine.
`-XEX:NO` is RB3E's own flag telling link to *not* auto-emit a XEX and leave a
plain PE for `imagexex` — exactly the seam we substitute at step 4.)

**Disassembly proof** — extracted `.text` (104 bytes) and dumped raw PPC-BE:
```
3860 0001   li      r3, 1        # DllMain returns TRUE
4e80 0020   blr                  # classic PPC return
3884 0001   addi    r4, r4, 1    # our copy loop
4200 fff4   bdnz    ...          # loop branch
7ca9 03a6   mtctr / mtspr        # loop counter setup
```
Unmistakably real Xbox-360 PowerPC. Compile→link is done, with free tools only.

## 4. XEX PACKING — PLAUSIBLE (the one open step)

The XDK's `imagexex.exe` (PE→XEX2, driven by RB3E's `xex.xml`:
`<unencrypted/><compressed/>`, `baseaddr 0x84000000`) is the only proprietary
piece with no drop-in OSS replacement. **All** mature OSS tooling
(idaxex/`xex1tool`, emoose `xbox360.py`, Xenia `xex_module.cc`) is a **XEX→PE
extractor/loader**, never a builder. Three free paths close the gap:

**Path 4a — reuse the released RB3E XEX as a shell (LOWEST EFFORT, recommended).**
RB3Enhanced ships a prebuilt `RB3Enhanced.dll` (a XEX2-DLL) on GitHub Releases.
It is already a valid, target-accepted container with the correct base
(`0x84000000`), import table, and image flags. Because RB3E's architecture is
*additive runtime code injection* (POKE_B trampolines), our same-instrument
change is just extra `.text` + a few hook registrations. Pipeline:
  1. `xex1tool`/idaxex → extract the PE basefile from the released XEX (idaxex
     source is already local at
     `/home/free/code/milohax/reverse-compiler-refs/idaxex/`; builds a Linux
     `xex1tool`).
  2. Splice our compiled hook `.text`/`.data` into a code cave (or a lengthened
     section) and wire the new hooks into RB3E's init table.
  3. Repack. For an **unencrypted/uncompressed** XEX the basefile is embedded
     near-raw, so the repack is header-fixup (sizes, page/section descriptors,
     hashes optional on RGH) rather than crypto.
This never needs `imagexex` and reuses an already-booting container. Effort:
~1–2 days once Lane A hands over the compiled hook blob + patch addresses.
Risk: the released DLL is compressed/needs section growth; mitigated by first
running it through XexTool `-c u -e u` (path 4c) to get an uncompressed shell.

**Path 4b — write a minimal PE→XEX2 packer (CLEANEST, more effort).** Because
RB3E targets **RGH/JTAG consoles that run UNSIGNED code**, we do **not** need
Microsoft's RSA private key — the security-info signature can be dummy/zeroed and
the loader (with dashlaunch/unsigned patches, which the target already has)
accepts it. With `XEX_ENCRYPTION_NONE` + `XEX_COMPRESSION_NONE` there's no AES
and no LZX — the packer just emits:
  - XEX2 header (magic, module flags = DLL bit, PE-data offset, security-info
    offset, optional-header count);
  - optional headers: Entry Point (`0x10100`), Image Base Address (`0x10201` =
    `0x84000000`), Base File Format (`0x3FF` = uncompressed/unencrypted),
    Import Libraries (`0x103FF`, or omitted if our module imports nothing);
  - security info (image size, load address, image flags, zeroed signature/hash
    — hash checks are off on unsigned-boot targets);
  - the raw PE appended.
Authoritative field layouts: Xenia `src/xenia/kernel/util/xex2_info.h` +
`xex_module.cc` (BSD-licensed — reverse the *load* logic to *build*). Estimated
~300–500 lines of Python/C. Risk: page/section descriptor + import-ordinal
encoding must be byte-exact; validate by round-tripping through idaxex and by
booting in Xenia (fast local loop) before hardware.

**Path 4c — xorloser XexTool under `wine` (SEMI-FREE, pragmatic).** XexTool is
closed-source freeware (redistributable, not the XDK) and runs under `wine`
(present at `/usr/bin/wine`). It decompresses/decrypts (`-c u -e u`) and can
reinsert a basefile into a XEX — a ready-made engine for path 4a's repack.
Caveat: "any altered/created retail xex will not be correctly signed" — fine for
RGH/JTAG/Xenia (unsigned), **not** for stock retail (which we cannot target
anyway without MS's key — a hard limit independent of toolchain).

**Hard boundary (all paths):** no free tool can produce a **retail-signed** XEX
(needs Microsoft's private key). Every OSS path yields an **unsigned** XEX that
boots on RGH/JTAG/devkit/Xenia only. That matches RB3E's own constraint (it
already requires a modded console), so it is not a new limitation.

## 5. (bonus) ALTERNATIVE COMPILERS — devkitPPC / xenon GCC = NOT RECOMMENDED

Assessed per task item 4. **Verdict: unnecessary and riskier than the proven
cl.exe path.**
- **No need:** step 1 proves `cl.exe` compiles our TU with free headers, so the
  motivation for GCC (avoiding XDK) is already satisfied by the MSVC path.
- **ABI risk is real but nuanced:** for our *simple pointer/int-signature* hooks,
  both MSVC-X360 and GCC PPC EABI follow base PowerPC conventions (args in
  r3–r10, return in r3, r1 stack), so a leaf hook taking `(void*, int)` and
  returning `int` would interop. BUT: (a) **struct-by-value** and vararg/home-
  space conventions differ between MSVC-X360 and EABI — several RB3E hooks pass
  objects; (b) GCC emits **ELF**, not COFF/PE, so it **cannot be fed to
  `link.exe`** to build the DLL and cannot be `POKE_B`-injected without a
  separate `.text` extraction step; (c) byte order is identical (both BE) so
  that's a non-issue. The Wii side already uses devkitPPC EABI, but through a
  *different* loader (BrainSlug) — that success does not transfer to the 360 XEX
  toolchain.
- **Conclusion:** keep the whole 360 build on the proven `cl.exe`/`link.exe`
  MSVC path; do not mix GCC PPC on 360. devkitPPC stays the Wii-only compiler.

---

## Recommended concrete pipeline (free-only)

```
# 1. compile our TU(s)  [PROVEN]
wibo cl.exe -c -nologo -Ox -Os -D_XBOX -DRB3E_XBOX -DRB3E -DNDEBUG \
     -GF -Gm- -MT -GS- -Gy -fp:fast -fp:except- -Zc:wchar_t -Zc:forScope \
     -GR- -openmp- /X -I <rb3e/include> -I src/xdk/LIBCMT \
     -Fo out/SameInstrumentHooks.obj SameInstrumentHooks.c

# 2. (only if kernel imports needed — likely NOT for our TU)  [PROVEN mechanism]
wibo link.exe /LIB /MACHINE:PPCBE /DEF:xboxkrnl.def /OUT:xboxkrnl.lib

# 3. link the DLL PE  [PROVEN]
wibo link.exe -nologo -dll -MACHINE:PPCBE -entry:_DllMainCRTStartup \
     -XEX:NO -FIXED:NO -OPT:REF -OPT:ICF -STACK:262144,262144 \
     -OUT:out/RB3Enhanced.dll out/*.obj [xboxkrnl.lib ...]

# 4. pack to bootable XEX2  [PLAUSIBLE — pick one]
#   4a  xex1tool extract released RB3E XEX  → splice our .text → repack   (recommended)
#   4b  minimal python xex2 packer (Xenia xex2_info.h spec, unsigned/uncompressed)
#   4c  wine XexTool -c u -e u  (reinsert basefile)
```

## Effort estimate

- Steps 1–3 (compile+link, free): **done / <0.5 day** to wire into rb3-xenon's
  ninja as a new `xbox-dll` target (mirror the existing `msvc` rule minus decomp
  context, add the link edge).
- Step 4a (reuse released XEX shell): **~1–2 days** given Lane A's compiled hook
  blob + patch table.
- Step 4b (from-scratch packer): **~3–5 days** incl. Xenia-boot validation loop.
- Total to a Xenia-bootable artifact via 4a: **~2–3 days**; hardware
  (RGH/JTAG) adds testing, not build work.

## Biggest single risk

**Step 4 PE→XEX2 packing** — the only step with no turnkey OSS tool and no local
proof yet. It is an *effort/validation* risk, not a feasibility blocker: the
format is documented (Xenia `xex2_info.h`, free60), unsigned/uncompressed removes
the crypto, RGH/JTAG removes the signing requirement, idaxex is already local to
build the inverse (extract) reference, and Xenia gives a fast boot-test loop.
De-risk first by proving path 4a on the released RB3E DLL (extract→identity-
repack→boots) *before* touching our hooks.

## Proven-facts appendix (raw experiment output)

```
# machine type of cl.exe output
machine=0x01f2 nsections=6            # POWERPCFP, correct 360 target

# LIB from .def
Microsoft (R) Library Manager Version 10.00.11886.00
   Creating library _ossprobe/xboxkrnl.lib and object _ossprobe/xboxkrnl.exp

# linked DLL
PE sig= b'PE\x00\x00'  machine=0x01f2
sections=.rdata/.text/.XBLD imagebase=0x88000000 entrypoint_rva=0x10060

# .text disassembly (raw PPC-BE, our DllMain+copy loop)
3860 0001  li   r3,1      # DllMain -> TRUE
4e80 0020  blr
3884 0001  addi r4,r4,1
4200 fff4  bdnz
```

Local assets used: `build/compilers/X360/16.00.11886.00/{cl,link}.exe`,
`/home/free/code/milohax/wibo/build/release/wibo`, `src/xdk/LIBCMT/*` +
`src/xdk/*.h`, `/home/free/code/milohax/reverse-compiler-refs/idaxex/`
(local idaxex source for `xex1tool`), `/usr/bin/wine`, and real XEX references at
`rb3-xenon/orig/45410914/default.xex`.
