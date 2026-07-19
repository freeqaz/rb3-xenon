#!/usr/bin/env python3
r"""jrpc.py — Linux-native JRPC2 client for a modded (RGH/JTAG) Xbox 360.

JRPC2 is a homebrew DashLaunch plugin (`JRPC2.xex`) that extends the XBDM debug
protocol (TCP 730) with a `consolefeatures` verb. With it loaded you can:

  * resolve a module export to its runtime VA   (module + ordinal -> address)
  * CALL an arbitrary console function by module+ordinal *or* raw address,
    marshalling typed args on and reading the typed return value back.

That "call any function + read the return" capability is what lets us poke the
console's own APIs (e.g. xam's XamInputGetState) without rebuilding a title —
the exact lever the PS2-guitar / virtual-controller work needs.

This is a pure-stdlib reimplementation of XeCLI's `Jrpc2Client` wire format
(`consolefeatures ver=2 ...`), so it runs anywhere python3 exists — no .NET, no
Windows. (XeCLI's own CLI is `net10.0-windows` and won't run on Linux; only the
console-side `JRPC2.xex` plugin is needed, not any host-side JRPC binary.)

Prerequisites on the console
----------------------------
  1. `JRPC2.xex` present on a drive (we keep it at `Hdd:\JRPC2.xex`).
  2. Registered as a DashLaunch plugin in `launch.ini` `[Plugins]`, e.g.:
         plugin1 = Usb:\RB3ELoader.xex
         plugin2 = Hdd:\xbdm.xex
         plugin3 = Hdd:\JRPC2.xex
  3. **A COLD POWER CYCLE** after editing `launch.ini` — DashLaunch reads the
     `[Plugins]` list only at boot; a warm `magicboot` will NOT load a new plugin.
  Verify it's live with `jrpc.py <host> ping` (resolves a known ordinal).

RpcDataType (JRPC2 arg/return type tags)
----------------------------------------
  Void=0 Int=1 String=2 Float=3 Byte=4 IntArray=5 FloatArray=6 ByteArray=7
  Uint64=8 Uint64Array=9

Library use
-----------
  from jrpc import Jrpc
  j = Jrpc("192.168.8.180")
  addr = j.resolve("xam.xex", 401)                 # XamInputGetState VA, 0 if fail
  ret  = j.call_int("xam.xex", 401, [3, 0x84806000])  # -> "0" or "48F" (hex str)
  ret  = j.call_int(None, 0, [1, 2], address=0x82012340)  # call by raw VA

CLI use
-------
  jrpc.py 192.168.8.180 ping                       # is JRPC2 loaded?
  jrpc.py 192.168.8.180 resolve xam.xex 401        # -> 81xxxxxx
  jrpc.py 192.168.8.180 call    xam.xex 401 3 0x84806000
  jrpc.py 192.168.8.180 calladdr 0x82012340 1 2    # call by absolute address
Integer args accept decimal or 0x-hex. Returns are printed as the raw JRPC hex
string (e.g. `0`, `48F`).
"""
import socket
import sys

# RpcDataType enum (mirrors XeCLI / JRPC2).
VOID, INT, STRING, FLOAT, BYTE, INTARR, FLOATARR, BYTEARR, U64, U64ARR = range(10)

DEFAULT_HOST = "192.168.8.180"
XBDM_PORT = 730


def _hex_ascii(s):
    """ASCII string -> hex-of-bytes (JRPC2 marshals strings this way)."""
    return "".join("%02X" % b for b in s.encode("ascii"))


class Xbdm:
    """One-shot XBDM text-protocol connection (connect, send, read, bye)."""

    def __init__(self, host=DEFAULT_HOST, port=XBDM_PORT, timeout=8.0):
        self.host, self.port, self.timeout = host, port, timeout

    def send(self, cmd):
        """Send one command line; return (status_code, message, extra_lines).

        status_code : int XBDM status (200 OK, 202 multiline, 4xx error, -1 parse)
        message     : text after the status code on the status line
        extra_lines : body lines for a 202 multiline reply (sans the '.' terminator)
        """
        with socket.create_connection((self.host, self.port), timeout=self.timeout) as s:
            s.settimeout(self.timeout)
            f = s.makefile("rb")
            banner = f.readline().decode("ascii", "replace").strip()
            if not banner.startswith("201"):
                raise ConnectionError("bad XBDM banner: %r" % banner)
            s.sendall(cmd.encode("ascii") + b"\r\n")
            status = f.readline().decode("ascii", "replace").strip()
            code = int(status[:3]) if status[:3].isdigit() else -1
            msg = status[4:] if len(status) > 4 else ""
            lines = []
            if status.startswith("202"):  # multiline body follows, '.'-terminated
                while True:
                    ln = f.readline().decode("ascii", "replace").rstrip("\r\n")
                    if ln == ".":
                        break
                    lines.append(ln)
            try:
                s.sendall(b"bye\r\n")
            except OSError:
                pass
            return code, msg, lines


class Jrpc:
    """JRPC2 calls over XBDM (needs JRPC2.xex loaded on the console)."""

    def __init__(self, host=DEFAULT_HOST):
        self.host = host
        self.x = Xbdm(host)

    # -- export resolution -------------------------------------------------
    def resolve(self, module, ordinal):
        """XexGetProcedureAddress(module, ordinal) -> runtime VA (int), 0 on fail."""
        cmd = ('consolefeatures ver=2 type=%d params="A\\0\\A\\2\\'
               '%d/%d\\%s\\%d\\%d\\"'
               % (U64ARR, STRING, len(module), _hex_ascii(module), INT, ordinal))
        code, msg, _ = self.x.send(cmd)
        if code != 200:
            return 0
        parts = msg.split()
        if not parts:
            return 0
        try:
            return int(parts[-1], 16)
        except ValueError:
            return 0

    # -- argument marshalling ---------------------------------------------
    @staticmethod
    def _encode_int_args(args):
        """Encode int/uint args (both wire-encoded as RpcDataType.Int)."""
        sb = []
        for a in args:
            sb.append("%d\\%d\\" % (INT, a & 0xFFFFFFFF))
        return len(args), "".join(sb)

    # -- function call -----------------------------------------------------
    def call_int(self, module, ordinal, args, address=0, system=False):
        """Call a function; return its int return value as a raw hex string.

        Provide EITHER (module, ordinal) OR address=<absolute VA> (module=None).
        `system=True` runs the call in system context. Returns e.g. "0", "48F",
        or "ERR(<code>): <msg>" on an XBDM-level failure.
        """
        argc, ptext = self._encode_int_args(args)
        modpart = ' module="%s" ord=%d' % (module, ordinal) if module else ""
        syspart = " system" if system else ""
        cmd = ('consolefeatures ver=2 type=%d%s%s as=0 params="A\\%X\\A\\%d\\%s"'
               % (INT, syspart, modpart, address, argc, ptext))
        code, msg, _ = self.x.send(cmd)
        if code != 200:
            return "ERR(%d): %s" % (code, msg)
        parts = msg.split()
        return parts[-1] if parts else msg

    # -- liveness ----------------------------------------------------------
    def ping(self):
        """True if JRPC2 answers a resolve (proof the plugin is loaded)."""
        # ord 344 = XboxKrnlVersion in xboxkrnl.exe — a stable, always-present export.
        return self.resolve("xboxkrnl.exe", 344) != 0


def _parse_int(tok):
    return int(tok, 0)  # accepts 0x.. hex or decimal


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    host, sub = argv[1], argv[2]
    j = Jrpc(host)
    if sub == "ping":
        ok = j.ping()
        print("JRPC2 %s on %s" % ("ONLINE" if ok else "NOT RESPONDING", host))
        return 0 if ok else 1
    if sub == "resolve" and len(argv) >= 5:
        va = j.resolve(argv[3], _parse_int(argv[4]))
        print("%08X" % va)
        return 0 if va else 1
    if sub == "call" and len(argv) >= 5:
        mod, ordn = argv[3], _parse_int(argv[4])
        args = [_parse_int(a) for a in argv[5:]]
        print(j.call_int(mod, ordn, args))
        return 0
    if sub == "calladdr" and len(argv) >= 4:
        addr = _parse_int(argv[3])
        args = [_parse_int(a) for a in argv[4:]]
        print(j.call_int(None, 0, args, address=addr))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
