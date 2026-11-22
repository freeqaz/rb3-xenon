# FileMakePath `strtok` and NewFile `operator new`: retail behaviour (2026-11-22)

Lane `fmp`, worktree `~/tmp/wt-fmp`, base `60479a8e6`. Follows the unaligned-rows
section of `UH2_PLACEHOLDER_CALLEE_RERUN_2026-11-22.md`, which listed `FileMakePath`
(792 B, fuzzy 94.70) and `NewFile` (424 B, fuzzy 84.39) as making a call that
retail does not.

## What I expected and what the bytes say

Expected: retail `FileMakePath` builds the path without `strtok`, and retail
`NewFile` has no `operator new` on a path our body takes.

Both predictions were wrong as stated. Read from `python3 tools/va_disasm.py`:

- **`FileMakePath` (0x82516B10) calls `strtok` too**: `bl 0x8282D8A0` at
  0x82516D84 (the map names 0x8282D8A0 `strtok`). Retail has one call site,
  entered by `b 0x82516D80` with `mr r3,r31` (the cursor `c`) and fed `li r3,0`
  on the back edge from 0x82516D7C. Ours has two call sites (first call peeled
  off the loop), so the census sees one extra `strtok` row. The calls made at
  run time are the same: `strtok(c,"/")` once, then `strtok(NULL,"/")` until it
  returns NULL. The loop body also matches retail: push unless the token starts
  with `.`; for exactly `..`, pop unless the stack is empty or the top starts
  with `.`, in which case push; a lone `.` or `.x` is dropped.
- **`NewFile` (0x825173E0) is our body minus the `if (gNullFiles) return new
  NullFile();` early-out**: retail's only `operator new` (`bl 0x827BCFF0`, size
  0x40) is the `ArkFile` allocation, which we also make. Our extra one is the
  `NullFile` allocation. Everything else matches retail: the `MainThread()`
  result is discarded, then the `FileLocalize` (mode bit 2), `FileIsLocal`,
  `FileCache::GetFileAll`, `ArkFile`/`AsyncFile::New`, unconditional `Fail()`,
  `delete` and capture-file steps.

## Can the difference change a result natively?

**NewFile: no.** `gNullFiles` is a zero-initialised global with no writer
anywhere in `src/`, `native/src` or `../milo-native-engine/src` (grep; the only
other reader is `DirLoader.cpp:761`). The branch never runs, natively or on
X360, so our `NewFile` already returns what retail's does.

**FileMakePath: the call sequence is retail's. The cursor is not.** Repeated
separators, `.`/`..` handling and writing into `static_buffer` are retail
behaviour, because retail tokenizes with the same `strtok`. Retail's CRT `strtok`
keeps its cursor **per thread**: at 0x8282D8BC it calls `bl 0x82830978` (the
per-thread data block) and at 0x8282D910 it loads the NULL-argument cursor from
`lwz r11,0x18(r3)`, i.e. `ptd->_token`. glibc's `strtok` keeps **one cursor for
the whole process**. So the same source behaves like retail natively only while
one thread tokenizes at a time. If a `ThreadCall` worker (pthreads,
`native/src/platform/ThreadCall_Native.cpp`) builds a path while the main thread
is in the middle of one, the main thread carries on in the worker's buffer.
Neither loop (`FileMakePath` and its DC3 twin `FileMakePathBuf`, which
`native/src/platform/File_Native.cpp` calls) calls anything between its `strtok`
calls. So a cursor local to the call gives exactly retail's per-thread result
for every caller. On the same thread the two differ only in what the cursor
holds after the call returns. No caller in the tree is in the middle of its own
`strtok` sequence when it calls these. The other users (`File.cpp:449`
`FileRelativePathBuf`, `DataArray.cpp:70`, `Instarank.cpp`) were checked for
nested calls into these two functions, and none makes one.

I have **not** shown that the native app today runs `FileMakePath` on two threads
at once. The hazard is the glibc semantics plus the existence of worker threads,
not a reproduced bug in a real run.

### Test

The test (source in the appendix; built as `clang++ -std=c++17 -O1 -pthread`) is FileMakePath's loop,
parameterised on `strtok` or `strtok_r`. The main thread takes one token and then
yields to a worker, which tokenizes its own path to completion, as a preempted
thread would. clang++ -O1, glibc, this box:

```
one thread a//b/./c/../d        strtok="a/b/d" strtok_r="a/b/d"
one thread ../../system/run     strtok="../../system/run" strtok_r="../../system/run"
one thread a/b/../../..         strtok=".." strtok_r=".."
one thread //x///y/             strtok="x/y" strtok_r="x/y"
two threads, strtok    main="songs" worker="sys/run/shaders"
two threads, strtok_r  main="songs/gen/song.mid" worker="sys/run/shaders"
```

The four single-thread lines show both tokenizers give the same results, including
`//` and `..`. The two-thread pair is the difference. With glibc `strtok` the
main thread's `songs/gen/song.mid` comes back as `songs`, because its next
`strtok(NULL)` resumed at the end of the worker's buffer. With a call-local
cursor the main thread gets its full path, which is what retail's per-thread
`_token` produces.

## Change (`src/system/os/File.cpp`)

The match build is unchanged. A pair of macros spells the tokenizer:
`strtok(s,"/")` when not `HX_NATIVE` (byte-identical source), and
`strtok_r(s,"/",&pathTokCursor)` with a call-local cursor under `HX_NATIVE`. Both
`FileMakePath` and `FileMakePathBuf` use them. `NewFile` is unchanged apart from
a comment recording the new evidence below.

## Matching the bytes: tried, not done

- **One `strtok` call site.** Lane W7-A recorded four loop spellings as
  byte-identical. I compiled a standalone copy of the function with cl 10224
  `/O1 /Oi /GR /EHsc` (no PCH; `~/tmp/fmp_scratch/`) in seven spellings: the
  current `while`; `while ((p = strtok(arg,"/")) != 0) {...; arg = 0;}`;
  `for (s = c; (p = strtok(s,"/")); s = 0)`; `for(;;)` with break;
  `goto` into a `do/while`; `goto` into a `for(;;)`; and an early-`continue`
  body. All seven give 2 `bl strtok` sites. The peel is not reachable from the
  shapes tried.
- **Dropping the NullFile branch.** It is still blocked on the old finding:
  removing it costs −4 matched / −88 B, because it is the only thing that makes
  our `File.obj` emit `??_7File` with `??1File` (16), `Filename` (48) and
  `WriteDone` (16), which retail's File.obj owns (0x82516AC0/AD0/B00). New
  evidence on where retail gets them from:
  - Retail `.rdata` has **no NullFile vtable**. The only vtables holding
    `File::Filename` (0x82516AD0) are 0x82087C1C and 0x8208806C. Every pure
    slot in 0x82087C1C is `_purecall`, so it is File's own vtable; 0x8208806C is
    FileCacheFile's. NullFile does not override `Filename`, so any NullFile vtable
    would contain that pointer.
  - I tried moving `~File` out of line into File.cpp and **refuted it from the
    bytes before measuring**. Retail stores File's vptr inline in other units'
    dtors (0x82518C38 FileCacheFile, 0x8252D820/0x8252E2B4 AsyncFile/ArkFile,
    0x826FCF38 BufFile). Without LTCG that is possible only if `~File` is
    header-inline. The four `bl 0x82516AC0` sites are ctor EH funclets and dtors
    that chose not to inline it.
  - What is left: something in retail File.cpp referenced File's vtable from a
    function the linker later dropped (`/OPT:REF` keeps the COMDAT choice
    by link order even when the referencing function goes), or the COMDAT came
    from elsewhere. Not settled. I did not add an unreferenced function to fake
    the emission.

## Measurement

Predicted Δ0 on every ruler, because the X360 macro expands to the old `strtok`
calls. `python3 tools/ab_measure.py --worktree ~/tmp/wt-fmp --from-dirty`
(ruler `functionRelocDiffs=name_check`, objdiff-cli sha256 `ec3badc0f33323e9`;
leg B recompiled exactly one obj, File.obj), against legs measured in the same
run:

```
leg A: matched=55160 masked=25310 honest=29850 code%=59.488922
leg B: matched=55160 masked=25310 honest=29850 code%=59.488922
Δmatched=+0  Δhonest=+0  Δcode_bytes=+0  Δfuzzy=+0.000000pp  (none ruler: +0 B)
```

This is a native correctness fix. It cannot score on X360, by construction.

## Not done

No merge, push or `tools/native_health.sh --selftest` (the coordinator runs
these). I did not build the native tree. `strtok_r` is in glibc and emscripten
libc. The other `strtok` users (`FileRelativePathBuf`, `DataArray.cpp:70`,
`Instarank.cpp`) carry the same process-global cursor hazard natively. They are
outside this brief and unchanged.

## Appendix: tok_test.cpp

```cpp
// FileMakePath's tokenizer loop (src/system/os/File.cpp), parameterised on the
// tokenizer, run on a worker thread interleaved with the main thread.
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

static std::mutex mu; static std::condition_variable cv; static int phase = 0;
static void wait_for(int p) { std::unique_lock<std::mutex> l(mu); cv.wait(l, [&] { return phase >= p; }); }
static void go(int p) { { std::lock_guard<std::mutex> l(mu); phase = p; } cv.notify_all(); }

// Same loop as FileMakePath; `pause` makes the calling thread hand control to
// the other one after the first token, as a preempted thread would.
template <bool R> static std::string Collapse(const char *in, bool pause) {
    char c[256]; strcpy(c, in);
    const char *dirs[32]; const char **endDir = dirs; char *cursor = nullptr;
    char *p = R ? strtok_r(c, "/", &cursor) : strtok(c, "/");
    if (pause) { go(1); wait_for(2); }
    while (p) {
        if (*p != '.') *endDir++ = p;
        else if (p[1] == '.' && p[2] == '\0') {
            if (endDir != dirs && *endDir[-1] != '.') endDir--; else *endDir++ = p;
        }
        p = R ? strtok_r(nullptr, "/", &cursor) : strtok(nullptr, "/");
    }
    std::string out;
    for (const char **d = dirs; d != endDir; d++) { if (d != dirs) out += '/'; out += *d; }
    return out;
}

template <bool R> static void Run(const char *label) {
    phase = 0; std::string mainOut, workerOut;
    std::thread w([&] { wait_for(1); workerOut = Collapse<R>("sys/run/shaders", false); go(2); });
    mainOut = Collapse<R>("songs/gen/song.mid", true);
    w.join();
    printf("%-22s main=\"%s\" worker=\"%s\"\n", label, mainOut.c_str(), workerOut.c_str());
}

int main() {
    // Parity on one thread: repeated separators, ".", "..", leading "..".
    const char *cases[] = {"a//b/./c/../d", "../../system/run", "a/b/../../..", "//x///y/"};
    for (const char *s : cases)
        printf("one thread %-20s strtok=\"%s\" strtok_r=\"%s\"\n", s,
               Collapse<false>(s, false).c_str(), Collapse<true>(s, false).c_str());
    Run<false>("two threads, strtok");
    Run<true>("two threads, strtok_r");
}
```
