# Carve-pilot: BinDiff-hint → wired TU loop, measured (2026-07-20)

Goal: prove the Phase-3 carve loop end-to-end on `scripts/harvest/bindiff_r1_carving_hints.json`
(563 identified-but-uncompiled fns) and measure the **per-TU cost** so the coordinator
can size a campaign over the 4.6 MB anonymous `.text` mass.

Branch: `carve-pilot` (worktree `~/tmp/wt-carve-pilot`). Never touches main.

## TL;DR — the hint set does NOT scale to a carve campaign

Classifier: `scripts/harvest/carve_pilot_classify.py` (reproducible, buckets all 563 hints
by source-availability × wiring). Result:

| n | bucket |
|---|--------|
| 243 | NO SOURCE — XDK/CRT/middleware library internal (D3D, sockapi, shader-microcode, CRT EH, fread…) |
| 165 | XDK/kernel import thunk (xboxkrnl/xam) — no source |
| 45 | GAME wii-twin, ALREADY WIRED |
| 37 | ANON — no dc3 unit |
| 32 | DC3-only game class (`lazer/meta_ham`) — false-ID / absent in RB3 |
| **24** | ENGINE dc3-src UNWIRED (*mostly false basename matches — see below*) |
| **12** | GAME wii-twin UNWIRED (carvable) |
| 5 | ENGINE dc3-src ALREADY WIRED |

**~72 % (408/563) of hints have no portable source** — they are Xbox XDK static-lib
internals (kernel/xam import thunks, D3D `texture.obj`, `sockapi.obj`, `c30swprogram.obj`
shader-microcode emitters, `trnsctrl.obj` CRT EH, `fread.obj`). Contiguous and `sim≈1.0`,
but the source does not exist in `../dc3-decomp/src` or `../rb3/src` (only headers ship).
So the tight, high-confidence clusters are exactly the ones we **cannot compile**.

Of the nominally-carvable 36 (24 engine + 12 game), when you require **≥2 RB3-VA-contiguous
hits + a real (non-false-basename) source file**, the pool collapses to **one clean TU**:

- `shader` (19), `main` (3), `system` (2), `PlatformMgr_Xbox` (2) are **false basename
  matches** — e.g. `shader.obj` is the XDK XGraphics microcode builder, not
  `rndobj/ShaderMgr.cpp`; the 19 hits span 4.7 MB (not contiguous).
- The rest are **singletons** (1 hit — cannot anchor a TU span).
- **`SaveLoadManager`** (game, `band3/meta_band`, wii-twin, unwired, 2 hits, span 0x64c8)
  is the only clean contiguous carvable-with-source cluster.

**Campaign-sizing conclusion:** the BinDiff-hint→carve vein on *this* hint set is nearly
exhausted at ~1 TU. Future carving effort should switch identification method
(string-fingerprint clusters in the game `.text`, or Ghidra structural on `band3` regions)
rather than mining more BinDiff library-function hints. The 4.6 MB anonymous mass is
dominated by source-less XDK/CRT/middleware, not decompilable game/engine code.

## Pilot executed: SaveLoadManager (the one clean cluster)

`band3/meta_band/SaveLoadManager.cpp` — fills the 36 KB gap between pinned neighbors
`ProfileMgr.cpp` (ends 0x8254B070) and `PrefabMgr.cpp` (starts 0x82553F28); all are
`band3/meta_band` manager TUs, so identity is corroborated by neighborhood.

Loop executed (steps 1–2, 4–5 fully; step 3 source-port scoped, not completed):

1. **Pin** `.text start:0x8254B070 end:0x82553F28` in `splits.txt`; wire
   `"band3/meta_band/SaveLoadManager.cpp": "NonMatching"` in `objects.json`.
2. **`touch config.yml && configure.py && ninja config.json`** → dtk SPLIT emits
   `build/45410914/{asm,obj}/band3/meta_band/SaveLoadManager.{s,obj}` and
   **auto-derives the `.pdata`** (recipe confirmed).
3. Source port — **not done** (see cost below). Minimal stub committed so the unit
   compiles and is objdiff-measurable.
4. **Map** — `IsReasonToAutoload@0x82550728` was already a forward-provision entry
   (of the 267 landed on main); no fragment needed.
5. **Measure** (full `ninja-locked`, `rm report.cache`):
   - Unit `default/band3/meta_band/SaveLoadManager`: **405 target functions**, 36 224 B
     `.text`, `.pdata` present, **0 % matched** (stub — full port pending).
   - Whole-binary: **no regression** (only this unit changed; mapped % held at 54 %,
     4.64 MB unmapped, identical to baseline). **named-LOST == 0** by construction
     (no other unit's source/pin touched).

### Blocker found: gap is thunk-dense, not a clean class

The 36 KB gap carves into **405 functions, of which 364 are ≤0x40-byte stubs**
(vcall `??_9` thunks / `??_G`/`??_E` deleting-dtors) and only **~10 big + 32 mid** are
real method bodies. A message-heavy UI class (SaveLoadManager has ~40 methods + ~40
`OnMsg` handlers) emits a large thunk skirt. Matching requires the full class + all its
mixins present, so a partial port yields few matches. `SaveLoadManager.h` **already
exists** in-tree (header ported from rb3-Wii), which lowers the port cost — the bodies
(rb3-Wii `SaveLoadManager.cpp`, 2261 lines, MWCC→MSVC) remain.

## Measured per-TU cost recipe (times on warm objcache worktree)

| step | action | cost |
|------|--------|------|
| 0 | cluster selection (classifier + contiguity + Ghidra/neighbor sanity) | ~amortized; classifier run < 1 min |
| 1 | pin `.text` in splits.txt + wire objects.json | ~2 min editing |
| 2 | `configure.py` (regen build.ninja) | **~10 s** |
| 2 | `ninja config.json` (dtk SPLIT emits target obj/.s + auto `.pdata`) | **~0.3 s** |
| 3 | **source port** (MWCC→MSVC, Wii→360) | **DOMINANT** — hours for a 2 k-line thunk-dense game TU; less if header exists + small TU |
| 5 | full `ninja-locked` build + `rm report.cache` + report.json | **~30 s** |

**The infra (pin→emit→measure) is ~40 s + a few min editing and is source-independent.
The entire per-TU cost is the source port (step 3).** For SaveLoadManager-class TUs
(large, thunk-dense, message-heavy) that is a multi-hour deep-grind, not a quick pilot.
Small leaf TUs with existing headers would be far cheaper — but this hint set contains
essentially none that are both carvable-with-source and small.

## Artifacts

- `scripts/harvest/carve_pilot_classify.py` — reproducible bucket classifier.
- `splits.txt` / `objects.json` — SaveLoadManager pinned + wired.
- `src/band3/meta_band/SaveLoadManager.cpp` — stub placeholder for the port.
