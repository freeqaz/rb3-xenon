# XEX File Format Documentation

> **Source:** https://free60.org/System-Software/Formats/XEX/
> **Fetched:** 2026-01-25
> **Note:** This is a local copy of the Free60 Wiki XEX documentation for reference purposes.

---

## Overview

XEX is the executable file format used by the Xbox 360 operating system. It functions as a cryptographic and packing container for PowerPC PE executable files, comparable to UPX or TEEE Burneye.

The format supports on-demand decryption and decompression rather than extracting the entire file at once.

## Cryptography

While most XEX files use encryption, some unencrypted variants exist in the wild. The documentation includes references to C programs for dumping hash tables from default.xex files, starting at offset 0x288.

## XEX Header Structure

**Total length:** 24 bytes
**Byte ordering:** Big Endian

| Offset | Length | Type | Information |
|--------|--------|------|-------------|
| 0x0 | 0x4 | ASCII string | "XEX2" magic |
| 0x4 | 0x4 | Bitfield | Module flags |
| 0x8 | 0x4 | unsigned int | PE data offset |
| 0xC | 0x4 | unsigned int | Reserved |
| 0x10 | 0x4 | unsigned int | Security Info Offset |
| 0x14 | 0x4 | unsigned int | Optional Header Count |

### Module Flags Bitfield

| Bit | Description |
|-----|-------------|
| 0 | Title Module |
| 1 | Exports To Title |
| 2 | System Debugger |
| 3 | DLL Module |
| 4 | Module Patch |
| 5 | Patch Full |
| 6 | Patch Delta |
| 7 | User Mode |

## Optional Headers

Each header is 12 bytes (0x4 for ID + 0x8 for data/offset).

**Data interpretation rules:**
- If `(ID & 0xFF) == 0x01`: Header Data field contains actual data
- If `(ID & 0xFF) == 0xFF`: Header Data contains size information
- Otherwise: Value represents entry size in DWORDs (multiply by 4 for bytes)

### Header ID Reference

| Value | Description |
|-------|-------------|
| 0x2FF | Resource Info |
| 0x3FF | Base File Format |
| 0x405 | Base Reference |
| 0x5FF | Delta Patch Descriptor |
| 0x80FF | Bounding Path |
| 0x8105 | Device ID |
| 0x10001 | Original Base Address |
| 0x10100 | Entry Point |
| 0x10201 | Image Base Address |
| 0x103FF | Import Libraries |
| 0x18002 | Checksum Timestamp |
| 0x18102 | Enabled For Callcap |
| 0x18200 | Enabled For Fastcap |
| 0x183FF | Original PE Name |
| 0x200FF | Static Libraries |
| 0x20104 | TLS Info |
| 0x20200 | Default Stack Size |
| 0x20301 | Default Filesystem Cache Size |
| 0x20401 | Default Heap Size |
| 0x28002 | Page Heap Size and Flags |
| 0x30000 | System Flags |
| 0x40006 | Execution ID |
| 0x401FF | Service ID List |
| 0x40201 | Title Workspace Size |
| 0x40310 | Game Ratings |
| 0x40404 | LAN Key |
| 0x405FF | Xbox 360 Logo |
| 0x406FF | Multidisc Media IDs |
| 0x407FF | Alternate Title IDs |
| 0x40801 | Additional Title Memory |
| 0xE10402 | Exports by Name |

## Program/Section Content

Content typically starts at offset 0x2000 and contains an encrypted/packed PE file.

- **Encryption:** Section contents are encrypted with CBC AES, with the key changing for each file
- **Compression:** Uses Microsoft's proprietary LDIC algorithm

## Extended File Header Structure (Security Info)

| File Offset | Description |
|-------------|-------------|
| 0x0 | Header Size (4 bytes) |
| 0x4 | Image Size (4 bytes) |
| 0x8 | RSA Signature (260 bytes) |
| 0x10C | Resulting Image Size (4 bytes) |
| 0x110 | Load Address |
| 0x140 | Media ID (16 bytes) |
| 0x150 | AES Key Seed (16 bytes) |
| 0x164 | SHA Input (20 bytes) |
| 0x178 | Region (4 bytes) |
| 0x17C | SHA Hash (20 bytes) |
| 0x180 | Image Data Count (4 bytes) |
| 0x184+ | Image Data entries (24 bytes each) |

## Miscellaneous Findings

### Strings Located in XEX Files

**Directories:**
- XAdu
- $UPDATES
- MEDIA

**Paths:**
- `\Device\CdRom0\default.xex`

**Executables:**
- installupdate.exe
- xboxkrnl.exe
- xam.xex

**Library includes:**
- XUIRNDR
- XAUD
- XGRAPHC
- XRTLLIB
- XAPILIB
- LIBCMT
- XBOXKRNL
- D3D9
- XUIRUN

### Known XEX Files in the Wild

1. Original Xbox Game Support (November 2005)
2. Original Xbox Game Support (December 2005)
3. Windows XP Media Center Edition Update Rollup 2 (contains XboxMcx.xex)
4. Xbox 360 HD DVD Update

### Tools for XEX Manipulation

- **xextools** - Library and command-line tools for XEX manipulation
- **xexdump** (Perl version) - Information dumper
- **xexdump** (Windows version) - Information dumper

---

*This documentation is preserved from the Free60 Wiki for offline reference in the rb3-xenon decompilation project.*
