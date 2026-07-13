# Same-Instrument deploy — from-source RB3Enhanced.dll (RGH Xbox 360)

This is the **from-source, XDK-free** RB3Enhanced.dll (Strategy B) with the
same-instrument feature compiled in, replacing the old raw-spliced code-cave DLL.

## What's in this dir
- `RB3Enhanced.dll` — the from-source XEX2-DLL (sha256 `d39d4ec6b500…`, 8.7M).
  Built from `rb3-xenon/tools/xex2pack/work/boot.xex`. Targets **360 TU5**
  (base 0x84000000, unsigned/uncompressed — RGH skips HV hash checks).
  SameInstrumentHooks.c is linked in; `InitSameInstrument` installs all four
  detours (Layer A IsActive@0x826684C0, Layer B ResolvePartWaitStates@0x825B6488,
  H1 ProcessConfig@0x8276FA08, H2 RecalcGemList@0x82794740) when
  `SameInstReady()` passes (all support addrs pinned — verified).
- `rb3.ini` — `AllowSameInstrument = true` (the flag that arms the hook bodies;
  hooks install regardless, but no-op as pass-through unless this is true).

## The full stack (all three required)
1. **default.xex** = clean TU5 (sha `6639ce25…`) — unchanged, already on drive.
2. **RB3DX ARK** with the `dx_check_for_dupe → TRUE` DTA edit:
   `rock-band-3-deluxe/out/xbox/gen/{patch_xbox.hdr (6a5b174a…), patch_xbox_0.ark (714M)}`
   — already built + already on drive from the 2026-07-09 deploy.
3. **RB3Enhanced.dll** (this dir) + **rb3.ini** (AllowSameInstrument=true).

Why all three: RB3DX moved the dupe gate into DTA (`dx_check_for_dupe`), so the
ARK edit re-allows *selecting* a taken instrument; the DLL hooks prevent the
song-load crash (H1: 2nd same-type player reuses the track# instead of the
unguarded `vector[-1]` at TrackNum=-1) and the note-stealing (H2: per-watcher
gem-list clone).

## Deploy (idempotent — only replace the DLL + ini)
On the mounted drive at `/mnt/xbox-drive/Games/rb3/` (remount when attached):
```
cp RB3Enhanced.dll  /mnt/xbox-drive/Games/rb3/RB3Enhanced.dll
cp rb3.ini          /mnt/xbox-drive/Games/rb3/rb3.ini
sync
```
The DX ARK + clean TU5 default.xex are already in place from 2026-07-09 — do NOT
re-copy the 714M ARK unless it changed.

## Rollback
The prior deploy left rollback copies on the drive:
`RB3Enhanced_pre_si.dll`, `gen/patch_xbox_0.ark.bak` (+`.hdr.bak`), `rb3_old.ini`,
`default_pre_si.xex`. To revert the DLL only:
```
cp /mnt/xbox-drive/Games/rb3/RB3Enhanced_pre_si.dll /mnt/xbox-drive/Games/rb3/RB3Enhanced.dll
```
NOTE: this new DLL is the from-source build (sha `d39d4ec6…`), a DIFFERENT and
cleaner artifact than the old spliced DLL (`60389001…`). Keep the old one as a
second rollback point in case the from-source container behaves differently on
your RGH kernel.

## First-boot test (on hardware — the definitive test)
1. Boot RB3. Confirm RB3Enhanced loaded (version line / `[alive]` UDP if you set
   `[Events] EnableEvents = true`).
2. Two controllers → both pick **Guitar** in the same session (SI selection).
3. Start a song. **The crash you're seeing at song load is exactly what H1
   fixes** — if it now loads and both play with independent gems/scoring, the
   from-source SI build works. Symptom→lane: crash@load = H1, "notes already
   gone" = H2, forced-shared-difficulty = R5 (accepted v1 residual).

## Xenia note (corrected 2026-07-13)
This TU5-targeted DLL **also targets RB3DX** — no separate build needed. RB3DX's
`default.xex` (sha1 c5a17091) is byte-identical to clean TU5 except ~170 bytes in
unnamed regions, with **identical section tables and all 7 same-instrument
functions at the same VAs** (see `docs/plans/clean-tu5-vs-rb3dx-divergence.md` +
`rb3dx_port_audit.py`). The earlier "RB3DX is relocated / mismatches" claim was
relocation vs the *decomp base*, not vs TU5, and is retracted.

Xenia still cannot drive the full 2-guitar→song-load path end to end: RB3 boot in
Xenia halts at a deterministic **hub-scene-load crash (PC 0x82BCEFE4)** *before*
song select — a Xenia bring-up frontier, **not** an SI bug (stock RB3DX hits it
with no DLL). Hardware never sees that crash, so **hardware is the real test for
the gameplay path**. Xenia CAN validate that the DLL loads and the 4 SI hooks
install at the correct VAs on an RB3DX boot (see the RB3DX validation work in
`docs/plans/strategy-b/`).
