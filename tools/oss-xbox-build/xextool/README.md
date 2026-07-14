# xextool — xorloser's XexTool v6.3 (2011)

Windows binary; runs headless under `wine` (Linux). The one tool in-env that
can PRODUCE a valid compressed Xbox 360 XEX2 and recompute its page-hash chain.

Copied from `/srv/torrents/games/xbox/iso2god/` (2026-07-14) so it survives.

## Why we need it

Our from-source / spliced RB3Enhanced.dll repack ships as an **uncompressed**
XEX2 (`CompressionType = NONE`), which the retail RGH kernel's XexLoadImage
**rejects** (NONE is a Xenia-only convenience; the kernel only implements
BASIC/1 and NORMAL/2). Our repack also left a **stale page-hash chain**. XexTool
fixes both in one pass.

## Usage — make a repacked DLL loadable on hardware

    wine tools/oss-xbox-build/xextool/xextool.exe -c c -o out.dll in.dll
    #  -c c  = force NORMAL/LZX compression (also recomputes page hashes)
    # verify:
    tools/oss-xbox-build/L-importlibs/xex1tool -l out.dll
    #   -> "Compressed", base 0x84000000, imports intact, no hash warnings

Confirmed on the RGH console 2026-07-14: the compressed output maps at
0x84000000 (`modules` in XBDM) and RB3E init runs. See
`docs/plans/si-hw-fix/wave8/WAVE8-STATUS.md`.

`Iso2God.exe` is the GUI tool from the same bundle (ISO->GOD converter),
kept for completeness; not used in the DLL pipeline.
