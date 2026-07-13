# Same-Instrument deploy — from-source RB3Enhanced.dll on **RB3 Deluxe (RB3DX)**

This is the **from-source, XDK-free** RB3Enhanced.dll (Strategy B) with the
same-instrument feature compiled in, pinned to the **RB3DX** stack. It is the
**same DLL** as `../deploy-si/` — there is no address retarget, because RB3DX's
`default.xex` (sha1 `c5a17091`) is byte-identical to clean TU5 except ~170 bytes
in unnamed regions, with identical section tables. Every TU5 `PORT_` VA and all
four same-instrument hooks target RB3DX unchanged.

> Byte-proof: `rb3-xenon/docs/plans/clean-tu5-vs-rb3dx-divergence.md`
> Regression gate (re-prove against any future xex): `rb3-xenon/tools/oss-xbox-build/rb3dx_port_audit.py`

## What's in this dir
- `RB3Enhanced.dll` — the from-source XEX2-DLL (sha256 `b0d06f86…`, 8.7M).
  Packed by `xex2pack.py --compress basic` from the K-link PE
  (`tools/oss-xbox-build/K-link/RB3Enhanced.exe`, entry 0x8401CF90, base
  0x84000000, unsigned/uncompressed — RGH skips HV hash checks). SameInstrument-
  Hooks.c is linked in; `InitSameInstrument` installs all four detours when
  `SameInstReady()` passes (all support addrs pinned in ports_xbox360.h).
- `rb3.ini` — `AllowSameInstrument = true` (arms the hook bodies; hooks install
  regardless but no-op as pass-through unless this is true).
- `RB3Enhanced.map` — the linker map for this exact DLL. Hook/flag VAs below are
  extracted from it and are required by the Xenia SI harness (`--si_init_va`,
  `--si_force_allow_va`).

## In-DLL VAs (from RB3Enhanced.map; base 0x84000000)
| Symbol | VA |
|---|---|
| `InitSameInstrument` | `0x84019830` |
| `IsActiveHook` (Layer A) | `0x840191A8` |
| `ResolveWaitStatesHook` (Layer B) | `0x840191E8` |
| `ProcessConfigHook` (H1) | `0x84019780` |
| `RecalcGemListHook` (H2) | `0x84019450` |
| `config` (global base) | `0x84829540` |
| `config.AllowSameInstrument` (byte) | `0x84829590` (= base + 0x50) |

`AllowSameInstrument` field offset 0x50 is compiler-confirmed (offsetof probe;
`sizeof(RB3E_Config)` = 0x28C). Under Xenia's `--si_load_dll` (which loads with
`call_entry=false`, so DllMain/config-load never runs) the flag stays 0 and the
hooks run pass-through — poke `0x84829590 = 1` via `--si_force_allow_va` to arm
them.

## The full stack (all three required)
1. **default.xex** = **RB3DX** `rock-band-3-deluxe/platform/xbox/default.xex`
   (sha1 `c5a17091cb44c0119424390a1738d161995e430e`). *(Clean TU5 xex `6639ce25`
   is equally valid per the divergence study — same DLL either way.)*
2. **RB3DX ARK** with the `dx_check_for_dupe → TRUE` DTA edit:
   `rock-band-3-deluxe/out/xbox/gen/{patch_xbox.hdr (6a5b174a…), patch_xbox_0.ark (aacfbb9d…)}`.
   On RB3DX the dupe *selection* gate lives in DTA (`dx_check_for_dupe`), so this
   ARK edit re-allows *selecting* a taken instrument; the DLL hooks then prevent
   the song-load crash and note-stealing.
3. **RB3Enhanced.dll** (this dir) + **rb3.ini** (`AllowSameInstrument = true`).

Why all three: ARK re-allows selection; H1 (`ProcessConfig`) stops the 2nd
same-type player from hitting the unguarded `vector[-1]` at TrackNum=-1 (the
song-load crash); H2 (`RecalcGemList`) gives each watcher a private gem-list
clone (no note-stealing).

## Deploy (idempotent — only replace the DLL + ini)
On the mounted drive at `/mnt/xbox-drive/Games/rb3/` (remount when attached):
```
cp RB3Enhanced.dll  /mnt/xbox-drive/Games/rb3/RB3Enhanced.dll
cp rb3.ini          /mnt/xbox-drive/Games/rb3/rb3.ini
sync
```
Ensure the RB3DX `default.xex` (c5a17091) and the dupe_allowed ARK (6a5b174a /
aacfbb9d) are in place; do NOT re-copy the 714M ARK unless it changed.

## Rollback
The prior deploy left rollback copies on the drive:
`RB3Enhanced_pre_si.dll`, `gen/patch_xbox_0.ark.bak` (+`.hdr.bak`), `rb3_old.ini`,
`default_pre_si.xex`. Revert the DLL only:
```
cp /mnt/xbox-drive/Games/rb3/RB3Enhanced_pre_si.dll /mnt/xbox-drive/Games/rb3/RB3Enhanced.dll
```
This from-source DLL (sha256 `b0d06f86…`) is a different, cleaner artifact than
the old spliced code-cave DLL (`60389001…`). Keep the old one as a second
rollback point.

## First-boot test (on hardware — the definitive test)
1. Boot RB3. Confirm RB3Enhanced loaded (version line / `[alive]` UDP if
   `[Events] EnableEvents = true`).
2. Two controllers → both pick **Guitar** in the same session (SI selection,
   allowed by the DX ARK).
3. Start a song. **The crash at song load is exactly what H1 fixes** — if it now
   loads and both play with independent gems/scoring, the from-source SI build
   works. Symptom→lane: crash@load = H1 (or T5 = DLL never loaded), "notes
   already gone" = H2, forced-shared-difficulty = R5 (accepted v1 residual).

## Xenia note
Xenia boots the RB3DX `default.xex` (with `--rb3dx_skip_calibration`) to the
`main_hub_screen`, then halts at a deterministic hub-scene-load crash
(PC 0x82BCEFE4) *before* song select — so the 2-guitar → song-load path cannot be
driven end-to-end there yet (that is a Xenia-bringup frontier, not a DLL defect;
the hardware path never had the hub crash). Hook *installation* can still be
validated pre-hub via `--si_load_dll --si_init_va=0x84019830 --si_hook_verify`.
See `rb3-xenon/docs/plans/strategy-b/RB3DX-RETARGET-PLAN.md` Phases 3–5.
