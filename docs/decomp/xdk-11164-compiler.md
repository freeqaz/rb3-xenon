# The XDK 2.0.11164 compiler (cl build 10224) — retail RB3's actual compiler

**Status: installed, verified, OPT-IN. It is NOT the default and must not become
the default without a separate, deliberate decision.** The fleet default remains
`X360/16.00.11886.00`; every landed match% figure is measured against it.

Lane CA-2, 2026-07-30.

## Why

Lane CA-1 (`scripts/harvest/rich_header.py`, commit `8a19fe90`) read the MSVC
Rich header out of retail `band.exe` and found retail Rock Band 3 was compiled
with **cl build 10224** (XDK 2.0.11164.0), while we compile with **11886**
(XDK 2.0.21173.0) — the build DC3 retail used, which is what calibrated the
method. One in-generation bump apart; PE linker version is 10.00 on both sides.

Lane BZ-1 separately found two *fixed* instruction-selection differences between
retail and our output. This lane settles whether the compiler explains them.

## The measured build number

CA-1's "11164 ships cl 10224" was an inference from one title and was explicitly
unconfirmed. It is now confirmed three independent ways:

| instrument | result |
|---|---|
| PE version resource of `cl.exe`, `c1.dll`, `c1xx.dll`, `c2.dll`, `clui.dll` | FileVersion `16.0.10224.0` |
| the compiler's own banner under wibo | `Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 16.00.10224.00 for PowerPC` |
| **`@comp.id` stamped into an object it produces** (authoritative) | **`0x00AB27F0` → prodid `0x00AB`, build `0x27F0` = 10224** |

The product id is `0x00AB` — byte-identical to what our normal 11886 objects
carry (`0x00AB2E6E`). Only the build number differs. As CA-1 warned, the public
product-id name table labels `0x00AB` "POGO"; that name is wrong for the XDK
branch. **Trust the build number, not the name.**

## Where the bits came from

Installer: `/home/free/toolchains/xbox360-xdk/XDKSetupXenon11164.3.exe`
(1,038,530,264 bytes, sha256 `2954bb94…7180e`). It is an InstallShield
self-extractor holding **11 concatenated CAB archives**, not one — the first CAB
ends at byte 201,591,893 and ten more tile the remaining ~837 MB:

| cab @ offset | size | contents |
|---|---|---|
| 438272 | 201 MB | `XDK/bin/win32`, `XDK/bin/x64` — **the compiler lives here** |
| 201591893 – 434867995 | 42–76 MB each | `XDK/lib/xbox` (PPC link libraries) |
| 477092641 | 21 MB | `XDK/lib/win32`, `XDK/lib/x64` |
| 497671251 | 120 MB | `XDK/lib/*`, `XDK/art/*` |
| 617788986 – 819165340 | 38–219 MB | `XDK/art`, `XDK/Source/Samples`, `XDK/doc`, `XDK/bin` |

The installer was **never executed**; files were extracted with `7z x`. Only the
16 compiler files were extracted (8.4 MB) to
`/home/free/toolchains/xbox360-xdk/extract-11164/XDK/bin/win32/`. The XDK
headers and `XDK/lib/xbox` link libraries are present in the CABs but were *not*
extracted — pull them from there if a link-time experiment ever needs them.

## Installed location

`build/compilers/X360/16.00.10224.00/` — same 14 files as
`16.00.11886.00`, verified by an exact `find`-list diff (**FILE LISTS
IDENTICAL**), 6.3 MB.

Of the 14, 11 differ from their 11886 counterparts; three are byte-identical
because they are the shared VS2010 redistributable runtime: `msvcp100.dll`,
`msvcr100.dll`, `tlbref.dll`. `c1.dll`, `c1xx.dll`, `c2.dll` all differ in size,
so this is a genuinely different compiler, not a re-stamp.

`build/` is gitignored, so these binaries are **not committed**. A fresh worktree
sees them automatically: `scripts/setup_worktree.sh` **symlinks**
`build/compilers` at main's directory (see its "read-only toolchain" block), so
every existing and future worktree already resolves `16.00.10224.00` with no
extra work.

## How to use it (opt-in)

```bash
python3 configure.py --x360-compiler-version X360/16.00.10224.00
#   or
RB3_X360_COMPILER_VERSION=X360/16.00.10224.00 python3 configure.py
```

The command-line flag beats the environment variable, which beats the default.
Both print a `NOTE:` to stderr naming the non-default compiler. An unknown
version exits with the list of installed ones.

**When neither is set, generated output is byte-identical to before the switch
existed** — verified by running pristine `configure.py` (from `HEAD`) and the
patched one back-to-back with no `ninja` between them, and comparing:

```
IDENTICAL  build.ninja            (1069972 bytes)
IDENTICAL  objdiff.json           (1343134 bytes)
IDENTICAL  compile_commands.json  ( 827914 bytes)
```

`build/45410914/config.json` was hashed before and after and did not move, so
this is not the false positive you get from snapshotting across a re-split.

### ⚠ The non-obvious half of the switch

`config.linker_version` alone is **not** sufficient. `config/45410914/objects.json`
pins `"mw_version": "X360/16.00.11886.00"` on each of its 7 library groups, and a
non-`None` library option beats `tools/project.py`'s
`set_default("mw_version", config.linker_version)`. Setting only
`config.linker_version` flips the single global `mw_version` in `build.ninja` and
leaves **all 1094 per-object edges on the old compiler** — a switch that looks
like it works and silently does nothing. `configure.py` therefore also retargets
those explicit pins, but *only* on the opt-in path (so the default path has zero
behavioural delta). Correct result: 1096 references to `10224`, **0** to `11886`.

## Does it run under wibo? Yes

It compiles real TUs through the normal `ninja` path under the freeqaz wibo fork
with no wibo changes at all — including PCH TUs (`utl`, `obj`), so the
`/Yc`//`/Yu` path works too. Objects were confirmed to come from it by
`@comp.id`. No unimplemented-API blocker was hit.

**objcache was kept off for every mixed-compiler build** (`OBJCACHE=off`, which
makes the wrapper a passthrough). Verified by the counters: across the whole
lane, `hits` and `misses` were unchanged at 1,291,284 / 193,330 and only
`passthrough` advanced. Nothing built with 10224 entered the shared cache. Note
that objcache keys on compiler-DLL identity anyway, so 10224 objects would key
separately — but do not rely on that alone.

## The idiom test — one confirmed, one refuted

13 TUs compiled with both compilers, identical flags, comparing only `.text`
bytes. 11 of 13 differ. (`HttpGet` and `Symbol` are byte-identical; so is
`MasterAudio`, whose only differences anywhere in the object are the timestamp,
the version strings and the `@comp.id`.) `SHA1` and `Str` differ in `.text`
*length* — 10224 emits slightly **shorter** code.

**BZ-1 idiom 1 — the inlined strcpy/byte-loop null test: CONFIRMED.**

```
        lbz    10, 0(3)              lbz    10, 0(3)
 10224: cmplwi 10, 0          11886: extsb. 9, 10
        stbx   10, 11, 3             stbx   10, 11, 3
        addi   3, 3, 1               addi   3, 3, 1
        bf     2, .-16               bf     2, .-16
```

Retail uses `cmplwi`; we emit `extsb.`; **the 10224 compiler emits `cmplwi`.** In
all 11 differing TUs the older compiler emits fewer `extsb.` and correspondingly
more `cmplwi`, and in 10 of 11 the substitution is exactly 1:1 (the sum is
preserved). Totals: `extsb.` 22 vs 48, `cmplwi` 1596 vs 1572. In the smallest
case (`Sorting`) a full mnemonic histogram diff of the two objects shows the
*only* difference is `cmplwi` 9↔7 and `extsb.` 0↔2 — nothing else in codegen
moved. The compiler build is the cause.

**BZ-1 idiom 2 — the power-of-2 size test (`srawi.` vs `clrrwi.`): NOT
reproduced.** Counts are identical in every one of the 11 TUs (50 `srawi.` and
292 `clrrwi.`/`rlwinm.` on both sides). Either this sample does not contain the
specific construct BZ-1 saw, or that difference has another cause — it is *not*
explained by the compiler bump on this evidence. Do not assume both idioms come
free with a compiler swap.

## What this does NOT establish

No whole-binary A/B was run and none should be inferred. The upside was sized
only as "idiom 1 is compiler-caused"; how many currently-failing functions that
converts is unmeasured. A real evaluation would need a full build on 10224, which
would also mean rebuilding the PCH and keeping objcache out of the way — and it
would move every match% number in the project, so it needs its own lane and an
explicit decision.
