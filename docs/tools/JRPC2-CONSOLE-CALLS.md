# Calling console functions live — JRPC2 over XBDM

Companion to the [live-debug runbook](LIVE-DEBUG-RUNBOOK.md). JRPC2 lets us
**call an arbitrary console function and read its return value** without
rebuilding a title — resolve an export (module + ordinal → VA), marshal typed
args, invoke, read the result. It's the lever for probing the console's own APIs
(e.g. xam input) from Linux.

Tools live in `tools/oss-xbox-build/`:

| File | Purpose |
|---|---|
| [`jrpc.py`](../../tools/oss-xbox-build/jrpc.py) | Linux-native JRPC2 client (library + CLI). Stdlib only. |
| [`vc_diag.py`](../../tools/oss-xbox-build/vc_diag.py) | Worked example: the virtual-controller `XamInputGetState(3)` diagnostic. |

`jrpc.py` is a pure-stdlib reimplementation of XeCLI's `Jrpc2Client` wire format
(`consolefeatures ver=2 …` over XBDM `:730`) — verified byte-identical to XeCLI.
XeCLI's own CLI is `net10.0-windows` and won't run on Linux; only the
console-side `JRPC2.xex` plugin is needed, not any host-side JRPC binary.

## One-time console setup (JRPC2 as a DashLaunch plugin)

`JRPC2.xex` is a third-party homebrew plugin, **not** built by this repo (source
copies at `../debugging/JRPC/JRPC2.xex`, md5 `7f286c6c1db65d20fddda33c09b60577`,
73728 B). To load it:

1. **FTP is up only in Aurora.** If the console is in a title, reboot to the
   dashboard first: `python3 tools/oss-xbox-build/xbdm_cmd.py <host> 'magicboot cold'`
   then wait ~10 s for FTP.
2. Copy the plugin to the drive root: `Hdd:\JRPC2.xex` (FTP path `/Hdd1/JRPC2.xex`).
3. Add it to `Hdd:\launch.ini` `[Plugins]` in a **free slot**, alongside the
   existing entries (don't disturb `xbdm.xex`/`RB3ELoader.xex`):

   ```ini
   [Plugins]
   plugin1 = Usb:\RB3ELoader.xex
   plugin2 = Hdd:\xbdm.xex
   plugin3 = Hdd:\JRPC2.xex
   ```

   launch.ini is **CRLF / MS-DOS**; preserve line endings and back up the
   original (e.g. `Hdd:\launch.ini.prejrpc`) before overwriting.
4. **COLD POWER CYCLE.** DashLaunch reads `[Plugins]` only at boot — a warm
   `magicboot` will *not* load a newly added plugin.
5. Confirm it's live: `python3 tools/oss-xbox-build/jrpc.py <host> ping`
   (`JRPC2 ONLINE` = the plugin resolved an export).

## CLI usage

```bash
cd tools/oss-xbox-build
python3 jrpc.py 192.168.8.180 ping                    # is JRPC2 loaded?
python3 jrpc.py 192.168.8.180 resolve xam.xex 401     # export VA (0 = fail)
python3 jrpc.py 192.168.8.180 call    xam.xex 401 3 0x84806000   # call by module+ordinal
python3 jrpc.py 192.168.8.180 calladdr 0x82012340 1 2 # call by absolute VA
```

Integer args accept decimal or `0x`-hex. The return prints as the raw JRPC hex
string (`0`, `48F`, …). Library API: `from jrpc import Jrpc` →
`j.resolve(mod, ord)`, `j.call_int(mod, ord, [args], address=…, system=…)`.

### RpcDataType tags
`Void=0 Int=1 String=2 Float=3 Byte=4 IntArray=5 FloatArray=6 ByteArray=7
Uint64=8 Uint64Array=9`. `jrpc.py` currently marshals int/uint args and int
returns (what the input-probe work needs); extend `_encode_int_args` for
float/string/array types.

### Useful ordinals
| Module | Ord | Function |
|---|---|---|
| `xam.xex` | 400 / 401 / 402 | XamInputGetCapabilities / **GetState** / SetState |
| `xam.xex` | 685 | XamInputGetCapabilitiesEx |
| `xboxkrnl.exe` | 344 | XboxKrnlVersion (stable liveness probe) |
| `xboxkrnl.exe` | 486 | XInputdReadState (the hook target for the vc work) |

## Worked example — `vc_diag.py`

Decides the virtual-controller question: does xam's `XamInputGetState(3)` route
into our hooked `XInputdReadState`, or short-circuit because no real HID device
object exists? Non-destructive (saves/restores a scratch VA).

```bash
# after {rb3e_vc_connect} returns 3, with JRPC2 loaded:
python3 tools/oss-xbox-build/vc_diag.py 192.168.8.180
```

- `return 0` (ERROR_SUCCESS) → xam sees user 3 → bug is **RB3-side** (poll /
  enumeration / `joypad_is_connected` padnum↔user mapping).
- `return 0x48F` (DEVICE_NOT_CONNECTED) → bind callback alone is insufficient →
  **Phase 1**: port hiddriver's `HidAddDevice` path (real USB adapter) or
  synthesize a device object.

See the PS2-guitar plan at `~/.claude/plans/rb3-xenon-ps2-guitar.md` (outside the
repo) for the full phase breakdown.
