# WAVE-5 lane handoff: system/bandobj/NoteTube (port + wire + pin)

Branch: `w5-notetube`  ·  Worktree: `/home/free/tmp/wt-w5-notetube`
Base: main `5cb96d4` (setup_worktree baseline).

## Result summary

Ported the full 517-line Wii `NoteTube.cpp` (NoteTube + TubePlate) into xenon,
wired NonMatching, micro-pinned the NoteTube cluster in the huge auto gap between
RockCentral.cpp .text end (0x82BB2180) and block.c start (0x82BF7CB0).

**6 pins landed (map ADD-only), all size-exact:**

| addr | symbol | size | cli norm | note |
|------|--------|------|----------|------|
| 0x82bf6570 | `?CurrentStartX@TubePlate@@QBAMM@Z` | 12 | 100.0 | leaf getter, NOT ICF-folded |
| 0x82bf6580 | `?CurrentEndX@TubePlate@@QBAMM@Z` | 20 | 100.0 | leaf getter, NOT ICF-folded |
| 0x82bf6598 | `?SetDeployTiming@NoteTube@@QAAXMM@Z` | 56 | 100.0 | real-bodied anchor |
| 0x82bf65d0 | `?InitializePlate@NoteTube@@QAAX...@Z` | 256 | 99.5 | report-norm-100 (reloc-naming only) |
| 0x82bf6818 | `?PollDeploy@TubePlate@@QAAXM@Z` | 136 | 100.0 | real-bodied anchor |
| 0x82bf68a0 | `?SetPointPos@NoteTube@@QAAXHVVector3@@@Z` | 84 | 100.0 | real-bodied anchor |

5 true-100 (size-exact) + 1 report-normalized-100 (InitializePlate: every residual
diff is `diff_arg` on `bl`/`lfs` to correctly-identified engine functions
(SetGeomOwner/SetTransParent/AddObject) + `__real@3f800000`/`__real@00000000` float
pool constants — pure reloc-naming residue; size-exact 256/256). The coordinator's
composed report will confirm it at 100.

## Files changed (path-limited; DO NOT fold in the untracked global_fuzzy_pairs.json)

- `src/system/bandobj/NoteTube.cpp` (new, ported)
- `config/45410914/objects.json` — `system/bandobj/NoteTube.cpp: NonMatching` (engine module, after GemTrackDir)
- `config/45410914/splits.txt` — new unit at EOF:
  ```
  NoteTube.cpp:
  	.pdata      start:0x82257AE0 end:0x82257B00
  	.text       start:0x82BF6570 end:0x82BF6938
  ```
- `config/45410914/symbols.txt` — split the dtk-merged `fn_82BF6548` (SetShowing+CurrentStartX,
  0x34) into `fn_82BF6548` (0x28) + `fn_82BF6570` (0xC) so the .text range start at
  0x82BF6570 lands on a real symbol boundary (dtk rejected the mid-symbol split otherwise).
- `scripts/target_symbol_map.json` — 6 ADD-only entries above.

Overlap self-check: `pdata 0 overlaps, text 0 overlaps`. No unit claims pdata in
[0x82257AE0,0x82257B00) (block.c's pdata starts at 0x82257B68). None of the 6 pinned
addresses are in `icf_aliases.map` (not ICF-folded → honest distinct bodies).

## What a lander must know

- The .text range [0x82BF6570, 0x82BF6938) carves 10 target functions; only the 6
  above are mapped. The 4 unmapped in-range fns (Bake@0x82bf6770, BakePlates@0x82bf68f8,
  + fn_82BF66D0/fn_82BF67B8) report as `fn_XXXX` None — harmless, all genuine
  NoteTube/TubePlate methods, available for a future refinement pass.
- MSVC 16.00.11886, flags `/O1 /Oi /GR /EHsc /TP` (engine module default).

## Port adaptations (MWCC Wii -> MSVC X360)

- Include `bandobj/NoteTube.h` + **`rndobj/Env.h`** (Group.h forward-declares RndEnviron
  but instantiates `ObjPtr<RndEnviron>` dtor → needs the full type; standalone TU must
  pull Env.h).
- xenon `RndMesh::Vert` field is `tex` (not Wii `uv`) and `color` is float `Hmx::Color`
  (not `Color32`): `.uv`->`.tex`, `v.color.SetAlpha(a)`->`v.color.alpha = a`.
- xenon `RndMesh::VertVector` is the simplified variant (no `reserve`/`capacity`,
  `resize` is 1-arg). Dropped `Verts().reserve(...)` in the TubePlate ctor and rewrote
  `TubePlate::AllocateVerts` to `verts.resize(newsize)` (xenon resize auto-reallocs).
  `Faces()` is a real `std::vector` so its reserve/resize are untouched.
- FLT_MAX: Wii's `3.40282346638528859812e38` / `3.4028235E+38f` both overflow MSVC
  float parsing (C2177 "constant too big" — `3.4028235e38` rounds *above* FLT_MAX).
  Use the `<float.h>` `FLT_MAX` macro (ctor + Reset).
- The `(int)` casts I initially added to `mPoints.size()`/`.capacity()` compares changed
  `SetPointPos` codegen from unsigned `cmplw` to signed `cmpw` (−3%). Removed the cast to
  match the Wii original `if (i < mPoints.size())` → back to 100. Keep it uncast.

## Refused / not pinned (honest)

- **`?BakePlates@NoteTube@@QAAXXZ`** (0x82bf68f8): 86.25%, base 72 vs target 64 (2 extra
  insns). Root cause = register allocation: the retail build keeps `this` in the
  **volatile r7** across both `Bake()` calls with no save/restore; our per-TU build uses
  non-volatile **r31** (adds `std/ld r31`). Same compiler+flags, opt-level /O2//Ox tested
  (no change). r7 surviving a call is only valid with cross-function clobber knowledge
  (whole-program opt / same-TU Bake), which per-TU compilation cannot reproduce.
  at_limit / source-immune. Left in-range + in source for a future pass; NOT mapped.
- **`?Bake@TubePlate@@QAAXXZ`** (0x82bf6770): 0%, base 144 vs target 72 (2×). Structural
  divergence (VertVector/Vert layout + Sync-call codegen). NOT mapped.
