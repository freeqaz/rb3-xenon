# PS2→USB guitar adapter — device identity + control map

Reverse-engineered on a Linux host (2026-07-19) so the on-console HID driver
(hiddriver port / vcontroller) can translate this adapter's HID reports into an
XInput **guitar** (SubType 0x06) for Rock Band 3. Companion to
[JRPC2-CONSOLE-CALLS.md](JRPC2-CONSOLE-CALLS.md) and the PS2-guitar plan
(`~/.claude/plans/rb3-xenon-ps2-guitar.md`).

## Adapter identity
- **VID:PID = `0e8f:0003`** — "GreenAsia Inc. / MaxFire Blaze2", `iProduct` = "USB Joystick", `bcdDevice` 1.07.
- Interface: **HID class 3, subclass 0, protocol 0** — exactly what hiddriver's
  `HidAddDeviceHook` gates on (`bInterfaceClass==0x03 && subClass==0 && protocol==0`).
- One endpoint: **EP 0x81 IN, Interrupt, 8-byte** reports, `bInterval` 17. No report ID.

## HID input report layout (8 bytes)
Decoded from the report descriptor
(`0501 0904 a101 a102 7508 9505 … 0509 1901 290c 8102 …`):

| Byte | Contents |
|---|---|
| 0 | Axis Z (0–255) |
| 1 | Axis Rz (0–255) ← **strum** |
| 2 | Axis X |
| 3 | Axis Y |
| 4 | Axis (vendor/undefined) |
| 5 | low nibble = hat/D-pad (0–7, 8=null); high nibble = Buttons 1–4 |
| 6 | Buttons 5–12 |
| 7 | 8 vendor bits |

12 buttons (usages Button 1–12) + hat + 5 analog axes.

## Captured control → HID map (clean, guitar held still, one control at a time)
Linux `js0` button index N = **HID Button N+1**. Strum is the Rz **axis**, not the hat.

| RB guitar control | js0 event | HID usage |
|---|---|---|
| **Green** fret | button 5 | Button 6 |
| **Red** fret | button 1 | Button 2 |
| **Yellow** fret | button 0 | Button 1 |
| **Blue** fret | button 2 | Button 3 |
| **Orange** fret | button 3 | Button 4 |
| **Strum UP** | axis 1 = −32767 | Rz min |
| **Strum DOWN** | axis 1 = +32767 | Rz max |
| **START** | button 9 | Button 10 |
| **BACK/SELECT** | button 8 | Button 9 |
| Whammy | *(not reported)* | — adapter does not pass the analog whammy |
| Tilt | *(digital; unresolved)* | likely a button — recapture if gameplay SP needed |

Resting axes: axis0 pinned −32767, axis6 pinned +32767, axis1 (strum) centered 0.

## Intended XInput guitar mapping (SubType 0x06) for RB3
Target `XINPUT_GAMEPAD.wButtons` bits the driver should set per HID input:

| Guitar control | HID source | XInput bit |
|---|---|---|
| Green | Button 6 | A `0x1000` |
| Red | Button 2 | B `0x2000` |
| Yellow | Button 1 | Y `0x8000` |
| Blue | Button 3 | X `0x4000` |
| Orange | Button 4 | LB `0x0100` |
| Strum up | Rz < ~0x40 | DPAD_UP `0x0001` |
| Strum down | Rz > ~0xC0 | DPAD_DOWN `0x0002` |
| Start | Button 10 | START `0x0010` |
| Back | Button 9 | BACK `0x0020` |

This is complete for menu navigation **and** note-hitting gameplay (frets + strum).
Whammy (SP charge on sustains) and tilt (SP activation) are deferred — the adapter
doesn't expose whammy, and SP can be triggered via Back/tilt later once the tilt
button is pinned down.

## How it was captured
Host `free@100.118.146.12` (GPD Pocket), guitar in the adapter. Read `/dev/input/js0`
(joystick protocol: `struct {u32 time; s16 value; u8 type; u8 number}`), logged
button-down + axis events with timestamps, guitar held still to isolate the tilt
accelerometer noise. hidraw is root-only on that host; `js0` was readable.
