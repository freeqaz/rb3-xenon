#!/usr/bin/env python3
"""vc_diag.py — decisive virtual-controller diagnostic (JRPC2 + XBDM).

Context: our RB3Enhanced DLL binds a *virtual guitar* to user 3 via xam's
XamUserBindDeviceCallback. xam accepts the bind (`{rb3e_vc_connect}` -> 3), but
RB3 shows no connected pad. The single question that decides the whole approach:

  Does xam's XamInputGetState(user=3) route into our hooked XInputdReadState
  (=> reports CONNECTED), or does it short-circuit with DEVICE_NOT_CONNECTED
  because no real HID device object exists?

This calls XamInputGetState(3, &scratch) directly via JRPC2 and reads the return:

  return 0        -> ERROR_SUCCESS: xam SEES user 3 as connected. Our xam/kernel
                     hooks work; the bug is RB3-SIDE (poll loop / enumeration /
                     joypad_is_connected padnum<->user mapping).
  return 0x48F    -> ERROR_DEVICE_NOT_CONNECTED (1167): xam short-circuits BEFORE
                     our hook. The bind callback alone is INSUFFICIENT; we need a
                     real (or synthesized) device object -> Phase 1 (HidAddDevice).

Non-destructive: the scratch bytes are saved and restored.

Prereqs:
  * RB3 running with our DLL, and `{rb3e_vc_connect}` already issued (returns 3).
  * JRPC2.xex loaded (see jrpc.py header; `jrpc.py <host> ping` to confirm).

usage: vc_diag.py [HOST] [USER]   (defaults: 192.168.8.180, user 3)

Pass the user index that `{rb3e_vc_connect}` returned — xam may assign a
different slot per boot (seen 3 one boot, 1 the next), so query the slot the
bind actually landed on.
"""
import os
import sys

# Import the repo-local JRPC2 client (same directory as this file).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from jrpc import Jrpc, Xbdm  # noqa: E402

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.8.180"
USER = int(sys.argv[2]) if len(sys.argv) > 2 else 3
GETSTATE_ORD = 401       # xam.xex: 400 caps, 401 GetState, 402 SetState, 685 capsEx
# Scratch VA candidates (writable, unused): DLL BSS tail first, then high scratch.
SCRATCH_CANDIDATES = [0x84858800, 0x84858000, 0x83000000, 0x40000000]


def getmem(x, addr, n):
    code, msg, lines = x.send("getmem addr=0x%08X length=0x%X" % (addr, n))
    if not lines:
        return None
    h = "".join(lines).replace("?", "")
    try:
        return bytes.fromhex(h[: n * 2])
    except ValueError:
        return None


def setmem(x, addr, data):
    h = "".join("%02X" % b for b in data)
    code, _, _ = x.send("setmem addr=0x%08X data=%s" % (addr, h))
    return code == 200


def find_scratch(x):
    """Return (va, orig_bytes) for a VA where 20 bytes round-trip (restored)."""
    for va in SCRATCH_CANDIDATES:
        orig = getmem(x, va, 20)
        if orig is None:
            continue
        test = bytes(range(1, 21))
        if not setmem(x, va, test):
            continue
        back = getmem(x, va, 20)
        setmem(x, va, orig)  # restore immediately
        if back == test:
            return va, orig
    return None, None


def main():
    x = Xbdm(HOST)
    j = Jrpc(HOST)

    print("== JRPC2 liveness (resolve XamInputGetState) ==")
    va = j.resolve("xam.xex", GETSTATE_ORD)
    print("  xam.xex ord %d -> %08X" % (GETSTATE_ORD, va))
    if va == 0:
        print("  !! resolve failed -> JRPC2 not loaded (cold-boot after launch.ini"
              " edit?), or wrong ordinal. Aborting.")
        return 1

    print("== find safe scratch buffer (save/restore) ==")
    scratch, orig = find_scratch(x)
    if scratch is None:
        print("  !! no writable scratch found; edit SCRATCH_CANDIDATES.")
        return 1
    print("  scratch VA = 0x%08X (writable, will be restored)" % scratch)

    setmem(x, scratch, bytes(20))
    print("== call XamInputGetState(user=%d, &state=0x%08X) ==" % (USER, scratch))
    ret = j.call_int("xam.xex", GETSTATE_ORD, [USER, scratch])
    print("  return = 0x%s  (%s)" % (ret, ret))
    try:
        rv = int(ret, 16)
    except ValueError:
        rv = None
    state = getmem(x, scratch, 20)
    setmem(x, scratch, orig)  # restore

    print("== verdict ==")
    if rv == 0:
        print("  ERROR_SUCCESS -> xam SEES user %d as connected." % USER)
        print("  => Our xam/kernel hooks work. Bug is RB3-SIDE (poll loop /")
        print("     enumeration / notification). Next: make RB3 re-enumerate slot %d," % USER)
        print("     or check RB3E joypad_is_connected mapping vs xam user index.")
        if state:
            pkt = int.from_bytes(state[0:4], "little")
            btn = int.from_bytes(state[4:6], "little")
            print("     state.packet=%d wButtons=0x%04X (nonzero packet confirms our"
                  " hook filled it)" % (pkt, btn))
    elif rv in (0x48F, 1167):
        print("  ERROR_DEVICE_NOT_CONNECTED (0x48F) -> xam short-circuits BEFORE our hook.")
        print("  => Bind-callback alone is INSUFFICIENT. xam requires a real device object.")
        print("     Next: Phase 1 -- port hiddriver's HidAddDevice path (real USB adapter),")
        print("     or synthesize a device object so xam's presence check passes.")
    else:
        print("  Unexpected return 0x%s -- inspect manually." % ret)
    return 0


if __name__ == "__main__":
    sys.exit(main())
