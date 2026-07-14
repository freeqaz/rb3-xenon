# SI hardware debug workflow (RB3DX / Xbox 360)

The end-to-end loop for fixing a same-instrument (SI) song-load crash: build the
from-source RB3Enhanced DLL, pack it loadable, deploy, capture the live crash,
map it against the real console image, find the null/bad object, add a hook, repeat.

Two purpose-built tools remove almost all the manual toil:
- **`tools/oss-xbox-build/pack-si-dll.sh`** — PE → loadable, xexlint-gated DLL (one command).
- **`tools/xdbg.py`** — live-crash capture + symbolized disasm against the real image.

Console: **`192.168.8.180`** (FTP `xboxftp:xboxftp`, XBDM :730, RB3E ALIVE UDP :21070).
FTP is only up while Aurora runs; launching a title unloads it (cold-reboot to restore).

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
XDK_OSS=$(pwd)/../H-headers/xdk-oss ./build_xbox_ossp.sh all     # 51/51 TUs, link rc=0
```

- **`XDK_OSS` must point at `H-headers/xdk-oss`** (has `xtl.h`); the script's default
  (`src/xdk`) lacks it and silently drops `rb3enhanced.c` + the networking TUs,
  linking a stale obj without your hook wiring. Check obj timestamps if unsure.
- The link needs `obj/_xdk_stubs.obj` (no-op stubs for 34 XDK import symbols the
  reconstructed import libs export under `NetDll_*` names — networking/content/
  keyboard/relaunch, none used by the SI fix). Regenerate if missing:
  `grep -oE 'unresolved external symbol \w+' logs/link_full.log | awk '{print $4}' | sort -u`
  → one `int NAME(){return 0;}` each (return `1` for `XCloseHandle` /
  `XHasOverlappedIoCompleted`), compile with the same CFLAGS into `obj/_xdk_stubs.obj`.
- Verify your hook is in: `grep YourHookName RB3Enhanced.map`.

## 2. Pack it loadable (one command, xexlint-gated)

```bash
rb3-xenon/tools/oss-xbox-build/pack-si-dll.sh            # -> RB3Enhanced.fromsource.compressed.dll
rb3-xenon/tools/oss-xbox-build/pack-si-dll.sh --deploy   # + FTP to the console
```

Runs: xex2pack (basic compress, correct-VA import block via `--import-map`) →
`xextool -e d -c b` (flat base) → `fix_thunks` (repair the type-1 import thunks the
packer emits wrong — the load-blocker) → splice fixed base back → `xextool -m d -c c`
(recompress + rehash + devkit re-sign) → **`xexlint` must PASS (0 reject)** before
hardware. Entry point is re-derived from the PE each run (it moves every relink).

## 3. Deploy + launch

```bash
cd rb3-xenon/tools/oss-xbox-build
./xbox.sh deploy RB3Enhanced.fromsource.compressed.dll      # -> GAME:\RB3Enhanced.dll + sha verify
./xbox.sh launch                                            # magicboot the title
```

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
| 3 | `0x82B998D4` | null-dir propagated: `Find('smasher.trans')`→NULL → `r31->[0x170]` on the not-found path | see `CRASH3-TRACE.md` (in progress) |
