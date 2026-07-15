# Live-debugging runbook — RB3Enhanced on Xbox 360 hardware

The operational guide for a live session against the real console: rebuild the
from-source `RB3Enhanced.dll`, get it running, observe it, inspect live game
state, capture crashes, and recover from wedged states. Everything here was
hardware-verified 2026-07-15.

Companion docs (don't duplicate, read when needed):

| Doc | What it covers |
|---|---|
| [`tools/oss-xbox-build/BUILD-AND-DEPLOY.md`](../../tools/oss-xbox-build/BUILD-AND-DEPLOY.md) | Build/pack pipeline internals + the 8 build gotchas |
| [`docs/plans/si-hw-fix/DEBUG-WORKFLOW.md`](../plans/si-hw-fix/DEBUG-WORKFLOW.md) | The crash→analyze→hook-fix loop in depth + SI crash ledger |
| [`docs/plans/http-bringup-and-rb3eloader-fix-2026-07-15.md`](../plans/http-bringup-and-rb3eloader-fix-2026-07-15.md) | Why the raw container didn't load (root-cause record) |
| `../xex-patcher/docs/WINE-FREE-PACK.md` | XEX2 packing internals (signing, hash chains, xexlint) |

---

## 0. Console facts

- **`192.168.8.180`, wired directly into the LAN — no relay hop.**
  ⚠️ **Trap:** `tools/oss-xbox-build/xbox.sh` is the *retired relay-era* wrapper
  (ssh `.54` → `192.168.9.180`) and is banner-deprecated; the current console
  driver is **`../xex-patcher/tools/xbox.sh`** (direct, `XBOX` env override).
- **XBDM** TCP `:730` — modules/memory/reboot/notifications. Survives title
  changes; the only channel that works while the game is running or frozen.
- **FTP** `xboxftp:xboxftp` — **only while the Aurora dashboard runs.**
  Launching a title unloads Aurora and kills FTP.
- **RB3E HTTP** TCP `:21070`, **RB3E events** UDP broadcast `:21070`
  (`ALIVE` packet at init = hard proof the DLL loaded).
- The console **ignores ICMP** — "is it up?" = does XBDM (or FTP) answer, never ping.
- The running game is a **patched RB3DX TU5 `default.xex`** at load base
  `0x82000000`; the DLL links at `0x84000000`. Game-side addresses live in
  `RB3Enhanced/include/ports_xbox360.h`; DLL-side statics **move every relink** —
  re-derive from `K-link/RB3Enhanced.map` + `0x84000000`, never reuse old ones.

## 1. The edit → run loop (one command)

```bash
cd rb3-xenon/tools/oss-xbox-build
./build-si.sh            # compile 51 TUs + link + pack (offline gates only)
./build-si.sh --deploy   # ... + boot Aurora, FTP the DLL, sha-verify on-drive
./build-si.sh --launch   # ... + magicboot RB3, capture boot log, wait for ALIVE
```

Offline gates (compile hard-fail → `xexlint` PASS pre- and post-compress) catch
most mistakes before a hardware cycle — **hardware cycles are the scarce
resource**. The pack step's final `xextool -m d -c c` (LZX-compress + devkit
re-sign, under wine) is **load-critical**: the raw container is byte-correct but
`XexLoadImage` rejects it. `pack-si-dll.sh` does this automatically.

A successful launch shows, in order:
`modload RB3Enhanced.dll` → `[RB3E:MSG] Loaded!` → (≈25 s) `HTTP server
running!` → `ALIVE` UDP packet.

## 2. Verify what's actually running

Trust nothing after "it built" — stale-obj links and stale on-drive DLLs have
both burned real time.

```bash
X=../xex-patcher/tools/xbox.sh
$X cmd modules                 # is RB3Enhanced.dll mapped? check psize, NOT timestamp
$X sha '/Usb0/Games/rb3/RB3Enhanced.dll'   # on-drive bytes == local build?
$X cmd 'getmem addr=0x84000000 length=0x40'  # DLL image head, live
```

- The packer **zeroes the XEX timestamp** — `timestamp=0x00000000` is normal.
  "Did my build land" = `psize` change + the boot version string + on-drive sha.
- If behavior contradicts your source edit, suspect the build before the logic:
  rebuild and redeploy first (a v2/v3 cycle here returned wrong `/execute`
  results from a stale link; an identical-logic rebuild fixed it).

## 3. Observability channels

Three independent channels; use them together, they fail independently.

**XBDM notify stream** (DbgPrint — `[RB3E:MSG]`, `[RB3E:DBG]`, modload,
thread events). Run it *before* the action you want to observe:

```bash
python3 tools/oss-xbox-build/xbdm_notify.py 192.168.8.180 60   # seconds
# or: $X notify 60   (backgrounds + logs; $X notify-tail to follow)
```

**RB3E event broadcasts** (UDP 21070 — ALIVE, screen/venue/song/score events):

```bash
python3 tools/oss-xbox-build/rb3e_alive_listen.py 60
```

**RB3E HTTP server** (TCP 21070) — works with no debugger attached:

```bash
curl -s http://192.168.8.180:21070/           # index
```

## 4. Live game-state introspection

### `/execute` — run DTA in the live game, get the result back

The single most powerful probe: parses + executes an arbitrary DTA script **on
the main game thread** and returns the evaluated result as the HTTP body
(int/float/symbol/string serialized; objects/arrays as `<type 0xADDR>`).

```bash
s='{+ 2 3}'
curl -s "http://192.168.8.180:21070/execute?script=$(python3 -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))" "$s")"
# -> 5
```

Verified examples: `{+ 2 3}`→`5`, `{sprint "beat: " {+ 10 20}}`→`beat: 30`,
`{if_else 1 hello goodbye}`→`hello`. Two `[RB3E:MSG] DTA:` debug lines per call
appear on the notify stream (parse structure + result node).

⚠️ **Input is trusted.** A malformed (unbalanced-brace) script — `{{{` — runs
into `DataReadString` with no guard and **wedges all RB3E networking** (HTTP +
UDP die; the title keeps running). Recovery: relaunch RB3 (§6). Brace-balance
scripts client-side before sending.

### `xdbg` — symbolized disasm + live memory against the real image

```bash
source ../xex-patcher/.venv/bin/activate      # capstone
tools/xdbg.py mem   0x45191170 0x180          # live memory dump (words + ascii)
tools/xdbg.py deref 0x45191170 0xC8 0x4       # follow a pointer chain, flags NULLs
tools/xdbg.py str   0x8219ED08                # read a C string
tools/xdbg.py dis   0x82B998BC 12             # disasm at VA, branch targets symbolized
tools/xdbg.py fn    0x82B998D4                # whole function containing VA
tools/xdbg.py xref  0x82750188                # every bl call site of a function
tools/xdbg.py sym   PORT_DATAARRAYEXECUTE     # name <-> VA
```

Symbols come from `ports_xbox360.h` (game) + `K-link/RB3Enhanced.map` (DLL).
**Always disassemble against the real console image** (auto-cached at
`tools/oss-xbox-build/_dbg/default.base`) — `orig/45410914/band.exe` is a
different build with different addresses.

### Raw XBDM when you need it

```bash
python3 tools/oss-xbox-build/xbdm_cmd.py 192.168.8.180 'getmem addr=0x824f7e50 length=0xd0'
python3 tools/oss-xbox-build/xbdm_cmd.py 192.168.8.180 'threads' 'getexecstate'
```

`getmem` returns hex text — pipe through capstone for ad-hoc disasm of
functions xdbg doesn't know.

## 5. Crash capture + triage

On a fault the console freezes at the first-chance exception — **XBDM stays up,
FTP does not.** Capture *before* rebooting:

```bash
source ../xex-patcher/.venv/bin/activate
tools/xdbg.py crash        # fault PC + region, nonzero regs (symbolized),
                           # faulting instruction in context, stack return chain
```

Then map fault → object → field with `xdbg dis/fn/mem/deref/str` (worked
examples: `docs/plans/si-hw-fix/CRASH-2same-instrument-2026-07-14.md`,
`CRASH3-TRACE.md`). The fix loop (hook wiring in RB3Enhanced, guard-vs-root-cause
judgment) is [`DEBUG-WORKFLOW.md`](../plans/si-hw-fix/DEBUG-WORKFLOW.md) §5–6.

RB3E also writes its own crash dumps (`crash_*.exc`); parse with
`tools/oss-xbox-build/parse_rb3e_exc.py <dump.exc>`.

## 6. Recovery ladder (least → most disruptive)

1. **RB3E networking wedged** (HTTP refused, no UDP, title alive — e.g. after a
   malformed `/execute`): relaunch the title —
   ```bash
   python3 tools/oss-xbox-build/xbdm_cmd.py 192.168.8.180 \
     'magicboot title=\Device\Mass0\Games\rb3\default.xex directory=\Device\Mass0\Games\rb3'
   ```
2. **Need FTP back / neutral state**: boot Aurora — `$X aurora` then
   `$X wait-ftp` (a title launch always kills FTP; this restores it).
3. **Title frozen at a fault**: capture with `xdbg crash` FIRST (§5), then
   reboot: `xbdm_cmd.py 192.168.8.180 'magicboot cold'`.
   (`reboot` is **not** a valid XBDM command on this console — 407.)
4. **XBDM itself unresponsive**: hard power-cycle — **requires the human**;
   ask rather than retrying forever.

- **Never magicboot the Hdd Aurora path directly** — it crash-loops the console.
  Use the `$X aurora` wrapper.
- **`launch.ini` edits need a cold boot.** Warm reboots don't reload DashLaunch
  config — `magicboot cold` (or power-cycle), not a dashboard bounce.

## 7. Session-tested gotchas

- **FTP only under Aurora** — deploy scripts boot Aurora for you; don't fight it.
- **Two `xbox.sh` files exist**; only `../xex-patcher/tools/xbox.sh` is current (§0).
- **Long-running listeners** (notify, ALIVE): run each as its own background
  task writing to a log file, then read the log — don't foreground them.
- **DLL statics move every relink** — re-derive addresses from the fresh
  `RB3Enhanced.map` before poking memory.
- **The build gotcha list** (stale objs, `_xdk_stubs.c` socket wraps, `-RELEASE`
  drop, …) lives in `BUILD-AND-DEPLOY.md` — read it before touching the pipeline.
