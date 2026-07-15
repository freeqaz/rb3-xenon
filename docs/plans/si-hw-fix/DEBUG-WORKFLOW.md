# SI hardware debug workflow (RB3DX / Xbox 360)

> General live-session operations (observability channels, `/execute` DTA
> introspection, recovery ladder, verification) live in the canonical runbook:
> [`docs/tools/LIVE-DEBUG-RUNBOOK.md`](../../tools/LIVE-DEBUG-RUNBOOK.md).
> This doc is the SI-campaign crash→analyze→hook-fix loop in depth.

The end-to-end loop for fixing a same-instrument (SI) song-load crash: build the
from-source RB3Enhanced DLL, pack it loadable, deploy, capture the live crash,
map it against the real console image, find the null/bad object, add a hook, repeat.

Purpose-built tools remove almost all the manual toil (steps 1–3 below are all
wrapped by **`tools/oss-xbox-build/build-si.sh [--deploy|--launch]`** — one command
from edited C source to launched-and-verified on the console):
- **`tools/oss-xbox-build/pack-si-dll.sh`** — PE → loadable, xexlint-gated DLL (one command).
- **`tools/xdbg.py`** — live-crash capture + symbolized disasm against the real image.
- **`../xex-patcher/tools/xbox.sh`** — console driver; `redeploy`/`aurora`/`wait-ftp`/`verify`/`screen`/`alive` one-liners.

Console: **`192.168.8.180`** (FTP `xboxftp:xboxftp`, XBDM :730, RB3E ALIVE UDP :21070).
FTP is only up while Aurora runs; launching a title unloads it — `xbox.sh aurora`
boots Aurora back to restore FTP (`xbox.sh wait-ftp` blocks until it answers).

---

## The image that matters

The console runs a **patched RB3DX `default.xex`** (load `0x82000000`, ~0xEF0000)
that is **not** `orig/45410914/band.exe` — addresses differ by build, and some
functions are absent from band.exe. **Always disassemble crash VAs against the real
console image**, cached by `xdbg` at `tools/oss-xbox-build/_dbg/default.base`
(auto-pulled + `xextool -b`-extracted on first use). The port-header addresses in
`RB3Enhanced/include/ports_xbox360.h` (e.g. `PORT_OBJECTDIRFINDOBJECT 0x82750188`)
match this image; band.exe does not.

---

## 1. Build the DLL (from source, XDK-free)

```bash
cd rb3-xenon/tools/oss-xbox-build/K-link
./build_xbox_ossp.sh all     # 51/51 TUs, link rc=0
```

- `XDK_OSS` now **defaults to `H-headers/xdk-oss`** (has `xtl.h`) and the build
  **hard-fails on any TU compile error** — the old silent stale-obj-link failure
  mode is closed. If you override `XDK_OSS`, it must still have `xtl.h`.
- `K-link/_xdk_stubs.c` is compiled in the main loop and now **wraps the real
  xam `NetDll_*` winsock exports** (HTTP server + UDP events work from-source);
  only genuinely unused entrypoints remain `return -1` stubs. If a feature
  misbehaves only on the from-source build, check whether it hits a remaining stub.
- Verify your hook is in: `grep YourHookName RB3Enhanced.map`.

## 2. Pack it loadable (one command, xexlint-gated)

```bash
rb3-xenon/tools/oss-xbox-build/pack-si-dll.sh            # -> RB3Enhanced.fromsource.dll
rb3-xenon/tools/oss-xbox-build/pack-si-dll.sh --deploy   # + FTP to the console
```

Runs: xex2pack (`--compress none`, correct-VA import block via `--import-map`) →
`../xex-patcher/tools/pack-loadable.sh`, which reads the raw base at
`SizeOfHeaders`, runs `fix_thunks` (repair the type-1 import thunks the packer
emits wrong), then the native `xex-patcher --modified --raw` rebuilds the
FileDataDescriptor + page-hash chain + **import-digest chain** + HeaderHash +
devkit RSA signature, gated on **`xexlint` (must PASS, 0 reject)** — and finally
the **load-critical step [3]: `xextool -m d -c c`** (LZX-compress + devkit
re-sign, under wine). The raw container is byte-correct but `XexLoadImage`
**rejects it at image-map time**; only the compressed container loads (proven on
HW 2026-07-15 — this overturned the earlier "wine recipe retired / thunks were
the blocker" claim; the import table was byte-correct all along, see
`../http-bringup-and-rb3eloader-fix-2026-07-15.md`). Entry point is re-derived
from the PE each run (it moves every relink). Packing internals:
`../xex-patcher/docs/WINE-FREE-PACK.md`.

## 3. Deploy + launch

One command does the whole hot-reload cycle — boot Aurora (restores FTP, which a
title launch kills), wait for FTP, deploy + sha-verify, then magicboot RB3:

```bash
xex-patcher/tools/xbox.sh redeploy rb3-xenon/tools/oss-xbox-build/RB3Enhanced.fromsource.dll
```

(Or run the steps individually: `xbox.sh aurora` → `xbox.sh wait-ftp` →
`xbox.sh deploy <dll>` → `xbox.sh launch`.)

**Confirm the module actually loaded** in one command with `xbox.sh launch-watch` —
it launches the title, captures the XBDM DbgPrint stream *across* the magicboot reset
(`--follow` reconnects after the reset drops XBDM), and waits for the RB3E `ALIVE` UDP
signal, exiting non-zero if it never arrives:

```bash
xex-patcher/tools/xbox.sh launch-watch        # magicboot + boot-log + ALIVE, all in one
```

This replaces the old hand-assembled "background `xbdm_notify.py` + `launch` +
poll loop". To confirm *which* build is running (not just that one loaded), use
`xbox.sh verify` (prints the running `RB3Enhanced.dll` module line):

```bash
xex-patcher/tools/xbox.sh verify
```

Note: the wine-free packer **zeroes the XEX header timestamp**, so `timestamp=0x00000000`
is normal and is NOT a build marker. Read "did it land" from **`psize`** (the `.pdata`
size shifts when code is added — e.g. `0x538` → `0x548`) plus the boot `DbgPrint`
version string, not the timestamp.

Then two controllers pick the **same instrument** and start a song → the SI crash.
The console freezes at the first-chance fault (XBDM stays up; FTP does not).

## 4. Capture the crash

```bash
source ../xex-patcher/.venv/bin/activate         # capstone  (TODO: repoint to xex-patcher's final path)
rb3-xenon/tools/xdbg.py crash                     # fault PC, symbolized regs, fault instr, stack
```

`xdbg crash` prints the fault PC + region (default.xex / RB3Enhanced.dll), every
nonzero register (symbolized), the faulting instruction in context, and the stack
return-address chain. That is the whole crash picture in one command.

## 5. Analyze — map the fault to an object/field

```bash
rb3-xenon/tools/xdbg.py dis   0x82B998BC 12       # disasm at a VA (branch targets symbolized)
rb3-xenon/tools/xdbg.py fn    0x82B998D4           # whole function containing a VA
rb3-xenon/tools/xdbg.py xref  0x82750188           # every `bl` call site of a function
rb3-xenon/tools/xdbg.py str   0x8219ED08           # read a string (e.g. an object name)
rb3-xenon/tools/xdbg.py mem   0x45191170 0x180     # live XBDM memory dump (words + ascii)
rb3-xenon/tools/xdbg.py deref 0x45191170 0xC8 0x4  # follow a live pointer chain, flags NULL hops
```

Typical read: a `lwz r11, 0xNN(rX)` fault with `rX = 0` (or a small offset) is a
**method/field access on a null object**. Use `xdbg mem`/`deref` on the live `this`
(a register value in the crash dump) to walk the field chain and find *which* pointer
is null and *why*. `xdbg str` on the name being looked up (loaded into a reg via
`lis`/`addi`) tells you *what* object failed to resolve.

## 6. Fix — add a hook in RB3Enhanced

Most SI crashes are the base game dereferencing an object that doesn't exist on the
never-officially-supported dup-instrument path. Fix at the **DLL hook layer** (no
game-xex patching needed):

1. `include/ports_xbox360.h`: `#define PORT_YOURFN 0x........  // Class::Method`
2. `source/_functions.c`: `RB3E_STUB(YourFn)` (trampoline to the original)
3. a header proto + the hook body (guard/populate as needed), e.g. in `DataDebug.c`
4. `source/rb3enhanced.c` InstallHooks: `HookFunction(PORT_YOURFN, &YourFn, &YourFnHook);`

`HookFunction` relocates only the **first** instruction, so the target's `orig[0]`
must be relocatable (a `mflr r12` prologue is fine; a relative `b/bl` first insn is not).

> **Guarding vs root-causing.** A `return NULL` guard on a lookup stops *that* crash
> but the null propagates to the next unguarded consumer (that is exactly how crash #2's
> `ObjectDir::FindObject` guard surfaced crash #3 one call deeper). When guards cascade,
> stop and trace *why* the object is null upstream (`xdbg deref` on the live `this`),
> and prefer populating the real object (or short-circuiting the whole degenerate path)
> over guarding each consumer.

## 7. Repeat

Back to step 1. The offline gates (build link rc=0 → `xexlint` PASS) catch most
mistakes before a hardware cycle; hardware cycles are the scarce resource.

---

## Crash ledger (this campaign)

| # | Fault PC | What | Fix |
|---|---|---|---|
| 1 | `0x8279DA90` (stock repack) | `vector[-1]` at TrackNum=-1 | SI `ProcessConfigHook` (H1) |
| 2 | `0x8274E584` | `ObjectDir::FindObject` on a null dir (this+8 deref) | `ObjectDirFindObjectHook` null-dir guard |
| 3 | `0x82B998D4` | null-dir propagated: `Find('smasher.trans')`→NULL → `r31->[0x170]` on the not-found path | missing per-instrument smasher plate populated; FIXED on HW — see `CRASH3-TRACE.md` |
