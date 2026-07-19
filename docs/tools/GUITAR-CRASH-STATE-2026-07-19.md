# Guitar session — crash state dump (2026-07-19)

State captured live off the devkit (192.168.8.180) right after the guitar
worked (frets registered in RB3) but before/around a hang. Console was in a
tight infinite fault loop and then dropped off the network (needed a power
cycle → came back on fresh Aurora). Raw capture:
`docs/tools/_dbg/crash_0x8275b3cc_2026-07-19.log`.

## Crash signature (fresh, current session)
```
exception code=0xc0000005  thread=0xf9000000  address=0x8275b3cc  read=0x00000008  first
```
Repeated **478,825 times in a 12 s XBDM notify capture** — a tight loop.

| Field | Value | Meaning |
|---|---|---|
| code | `0xC0000005` | STATUS_ACCESS_VIOLATION |
| PC (address) | `0x8275b3cc` | faulting instruction, in RB3 `default.xex` `.text` (base 0x82000000) |
| read | `0x00000008` | faulting data address = **NULL + 0x8** → null-pointer field deref (`something->field@0x8`) |
| thread | `0xf9000000` | RB3 main thread |

The infinite loop is an artifact of RB3Enhanced's exception handler: it catches
the AV and resumes execution at the same PC, which immediately re-faults. One
underlying single null deref → endless spin → the flood eventually starved the
XBDM/network stack (`No route to host`).

## Earlier (stale, 09:40 same day — different site)
```
exception code=0xc0000005 thread=0xf9000000 address=0x825bf710 read=0x0000070c
```
NULL + 0x70c deref at a different PC. Recorded for reference; not the current fault.

## Interpretation
- Fault is in **RB3 game code**, not our injected DLL (which loads ~0x84000000).
  So RB3 itself is dereferencing a null pointer at `+0x8` on the main thread.
- Most-likely class: RB3 per-frame input / UI navigation walking a controller
  or player/UI object that is null for our injected virtual-guitar slot (e.g. a
  slot bound to a user index with no signed-in profile, or a menu element that
  expects a fully-wired Joypad object). Strumming (D-pad nav) is the prime
  trigger candidate — frets (A/B/X/Y) don't navigate, which is consistent with
  "frets work, then it dies."
- Not yet symbolicated: `0x8275b3cc` needs a Ghidra lookup against the retail
  image to name the function. TODO next time the Ghidra service is cooperative
  (`:8002`, binary `/default.xex-35adb6`).

## Safety note (persisted constraint)
Kernel patches are RAM-only; **nothing on-disk (NAND/HDD kernel) was patched**.
Only files written to the drive are `JRPC2.xex`, `launch.ini`, and
`RB3Enhanced.dll`. A power cycle fully clears all in-RAM patches/hooks.

## Follow-ups
1. Symbolicate `0x8275b3cc` (+0x8 field) to identify the null object.
2. The next instrumented build logs raw HID reports (`[RB3E:MSG] HID rpt ..`);
   watch whether a specific input immediately precedes the fault to correlate
   the crash with strum/nav.
3. Consider guarding the injected slot so RB3 never navigates with a
   profile-less virtual user (candidate mitigation once the null is identified).
