# Native LP64 and C-runtime triage (lane lp64, 2026-11-22)

Branch `lp64`. The lane's first run stalled at 14:18 UTC with its edits uncommitted;
the coordinator committed them as found (`85fa449c9`, rebased to `1bf18603b`). This
record covers that WIP, the retail check of each change, the finished snprintf item,
and the rb3-milo gate (`native/src/lp64_crt_phase.cpp`).

## Question

The native build is LP64 on glibc. Retail is a 32-bit PowerPC image linked against
the XDK's MSVC CRT. Where does engine or game code depend on retail's CRT
behaviour or on a 4-byte `long` or `wchar_t`, so that the native port runs it
differently?

## Census

`~/tmp/lp64/sites.tsv` (927 rows, not committed) is every clang
`-Wshorten-64-to-32` warning in a native build, classified by
`~/tmp/lp64/classify.py`. Classes: container 491, vendor (oggvorbis) 232, other 69,
ptrdiff 67, strlen 52, sizeof 5, POINTER? 4, time? 3, fileoff? 2, strtol 1. The
four POINTER? rows (CharClip.cpp/.h) take the difference of two pointers into one
buffer, so the result fits. The census did not find most of the defects below:
they are CRT-behaviour differences and element-size literals, which produce no
narrowing warning. They were found by reading the call sites of `strtok`, `rand`,
`atoi`, `wcslen`, `qsort` and `vsnprintf`.

## Changes, each checked against band.exe

Addresses are clean-TU5 VAs, read with `python3 tools/va_disasm.py`.

| Change | Retail evidence | Verdict |
|---|---|---|
| `strtok` -> `strtok_r` under HX_NATIVE: DataNode::Print, Instarank, File.cpp FileRelativePath/FileRelativePathBuf; DataArray's native-only ResolveContextPath | strtok 0x8282D8A0: `bl _getptd` at 0x8282D8BC, cursor read from ptd+0x18 at 0x8282D910. The cursor is per thread. glibc's strtok has one static cursor per process. | kept |
| `HX_CRT_RAND` (utl/CrtCompat.h): FlowNode, FlowPickOne, PseudoRandomPicker, PitchMucker | rand 0x8282E898: `bl _getptd`, seed at ptd+0x14, `mullw` by 0x343FD (214013), `+0x269EC3` (2531011), `rlwinm r3,r11,16,17,31` = bits 16..30. _getptd_noexit stores 1 to ptd+0x14 for a new thread (0x8283091C). The src tree has no `srand` call. | kept |
| `HX_CRT_ATOI`: DataFile ParseNode int token, DataInt on a symbol | atoi 0x8282D9B8 is `li r5,10; li r4,0; b strtol`, a 32-bit long, so it clamps to INT_MAX/INT_MIN. ParseNode calls it at 0x8276B7B0 (token 3, kDataTokenInt); DataInt at 0x8275F8E4. glibc's `atoi` is `(int)strtol` on a 64-bit long, so it truncates. | kept |
| qsort element size `8` -> `sizeof(DataNode)` natively: DataArray::SortNodes, BandWardrobe::OnSortTargets | SortNodes 0x8274CC90: `li r5,8` at 0x8274CCA4, `b qsort`. OnSortTargets: `li r5,8` at 0x8232C610. 8 is sizeof(DataNode) on X360; a native DataNode is 16 bytes. | kept |
| UTF-16 length in 16-bit units (EditSetlistPanel's native arm) | wcslen 0x8282D640: `lhz`, `addi r11,r11,2`, `srawi 1`. It counts 2-byte units; a Linux wchar_t is 4 bytes. | kept, now through `HX_CRT_WCSLEN`; also applied to PlatformMgr::SetPadProperty and XboxEntityUploader (native-compiled, same pattern) |
| MemMgr ThreadMemStack walks gThreadIds 4 bytes at a time | `lwz r11,0(r28)` / `addi r28,r28,4` at 0x827BB92C..0x827BB940; `lwz r3,0(r28)` / `addi r28,r28,4` at 0x827BB970..0x827BB988 | kept, rewritten to `DWORD *` (the same type as `unsigned long` on X360), matching main's MakeString fix `79633851d` instead of the WIP's `#ifdef` typedef |
| MakeString NextBuf walk | main already fixed it (`79633851d`, DWORD*) | WIP hunk dropped in the rebase, main's file taken whole |
| Hx_snprintf returns MSVC's results natively | see below | finished |
| FileGetPath copies with `memmove` natively (coordinator item: ASan strcpy-param-overlap at File.cpp:163, from BandSongMgr::AddSongData's `FileGetPath(FileGetPath(...))`) | FileGetPath 0x82516550 inlines the strcpy as a forward byte loop (`lbz`/`stbx`/`addi`, 0x82516564..0x82516578), so a source equal to static_path is an identity copy. glibc's strcpy on overlapping buffers is undefined. The overlap exists only natively. | added; `memmove` gives retail's result for any source at or after static_path |

No WIP change was wrong. The WIP's snprintf arm also returned -1 on an exact fit,
which retail does not do; that is fixed (next section).

## Hx_snprintf

Native code uses Hx_snprintf where retail calls the CRT `_snprintf` directly
(FormatString in MakeString.cpp, SuperFormatString, TextStream, LocalizeSeparatedInt).
Retail's `_snprintf` (0x8282D540) and `vsnprintf` (0x8282D750 -> `_vsnprintf_l`
0x8282D668) print into a string FILE whose count is the buffer size:

- output longer than the count: `_output` returns -1;
- an exact fit: returns the count, and the terminator is not written (the
  `--_cnt >= 0` store at 0x8282D60C..0x8282D61C fails and falls to `_flsbuf`);
- shorter output: terminated, returns the length.

glibc returns the length it would have written. A retail caller depends on the
difference: FormatString::operator<< (0x827C40E8) does `subf` of the result from
mBufSize and stores it (0x827C4140..0x827C4144). With glibc's value, mBufSize goes
negative after a truncation, and the next write lands past the end of the buffer.
With retail's -1 it moves by one byte.

The native arm now returns -1 on truncation and terminates the buffer, as Hx_snprintf's
own X360 arm does. On an exact fit it formats again into a `ui + 1` scratch buffer,
copies all `ui` characters and returns `ui`. A zero count returns -1 for non-empty
output and 0 for empty output.

## Gate: rb3-milo `lp64_crt_phase`

`native/src/lp64_crt_phase.cpp` has 13 defect gates and 11 controls. The 11th
control, `control-file-get-path-nested`, was added with the FileGetPath fix. It
checks the nested call's result (`"songs"`). Without a sanitizer, glibc's
overlapping strcpy happens to produce the same bytes, so this gate cannot detect
the UB. Only an ASan build can, and I did not rerun one. It uses only
APIs that exist without the fixes, so it can be built against main's src. The sort
and ThreadMemStack cases run in a forked child with a 20 s alarm, and the parent
gates on its exit status.

Run: `no610m.sh env RB3_ASSETS=<xbox-zip> ./rb3-milo <xbox-zip> ui/track/gen/tracksystem_meshes.milo_xbox`.

| | controls pass | defect gates pass | rb3-milo RESULT |
|---|---|---|---|
| before (branch src reverted to main with `git apply -R`, gate kept) | 10/10 | 0/13 | FAILED (13) |
| after (fixes at `0d9a1c286`, before the FileGetPath control) | 10/10 | 13/13 | ALL GATES PASSED (0) |
| after (branch tip, rebased on main `98f66d185`) | 11/11 | 13/13 | ALL GATES PASSED (0) |

Before-run values, which show each defect:
- snprintf: `"abcdef"` into 4 returned 6; `"abc"` into 3 left `61 62 00`; `"abc"` into 0 returned 3.
- strtok: after a worker thread's Print, the main thread's `strtok(NULL)` returned `""`; after a worker's FileRelativePath it returned NULL.
- rand: PseudoRandomPicker mode 3 on a fresh thread picked `1 5 3 5 2 1`, where retail's seed-1 LCG picks `6 1 1 6 4 3`; a second fresh thread did not repeat the first.
- atoi: `(3000000000)` -> -1294967296, `(-3000000000)` -> 1294967296, `{int '5000000000'}` -> 705032704.
- sort: `(3 1 2)` -> `<type 1> 3 2`; `(c a b d)` died on SIGSEGV.
- ThreadMemStack: child exit 2, meaning the worker re-registered after the main thread's lookup.

An earlier version of the gate ran the sort cases in-process. That run ended with
SIGSEGV (rc 139) in `lp64-sort-symbols`, which is why sort now runs in a forked child.
The first after-run also failed `boot-macros`. That was my setup: I had passed
`RB3_BOOT_MACRO_DROP=HX_XBOX`, which native_health uses for a different target.
Without it, boot-macros passes.

Classified NON_STUB in `tools/native_stub_census.py`.

## X360 match build

Every X360-side change either sits in an HX_NATIVE arm or expands to the original
tokens (`HX_CRT_RAND()` -> `rand()`, `HX_CRT_ATOI(s)` -> `atoi(s)`,
`HX_CRT_WCSLEN(s)` -> `wcslen(s)`, `DWORD` = `unsigned long`). The expected Δ is 0.
The ab_measure result is in the section below.

## Not done

- BandWardrobe::OnSortTargets and Instarank are fixed but not gated (rb3-milo does
  not link them). EditSetlistPanel::VerifyStrings, PlatformMgr::SetPadProperty and
  XboxEntityUploader's `HX_CRT_WCSLEN` sites are not gated either.
- Other `atoi` sites (DateTime, FlowSetProperty, SongParser, ClipCollide,
  PlatformMgr_Xbox, HttpGet) and ClosetPanel's `strtol` still use glibc. They parse
  small values, where truncation and clamping agree.
- STLport's `random_shuffle` (`__random_number` 0x8253B1D8) was not checked for
  `rand` use natively.
- The FileGetPath fix was not confirmed under ASan.
- `native_health --selftest` was not run; the coordinator's landing gate runs it.

## Measurement

(filled from ab_measure below)
