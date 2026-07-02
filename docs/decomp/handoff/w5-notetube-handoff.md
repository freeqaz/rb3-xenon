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

---

## WAVE-5 AUDIT (independent re-verify) — VERDICT: CLEAR

Audited c258d89 in-worktree with the MAIN repo's objdiff-cli (base obj rebuilt
via ninja-locked in this CoW worktree; no owner contention). All five audit
tasks pass.

**1. Per-fn re-verify (objdiff-cli-direct, JSON to file):**

| symbol | tgt/base | norm% | score | verdict |
|--------|----------|-------|-------|---------|
| CurrentStartX@TubePlate | 12/12 | 100.00 | 0/300 | TRUE-100 ✓ |
| CurrentEndX@TubePlate | 20/20 | 100.00 | 0/500 | TRUE-100 ✓ |
| SetDeployTiming@NoteTube | 56/56 | 100.00 | 0/1400 | TRUE-100 ✓ |
| PollDeploy@TubePlate | 136/136 | 100.00 | 0/3400 | TRUE-100 ✓ |
| SetPointPos@NoteTube | 84/84 | 100.00 | 0/2100 | TRUE-100 ✓ |
| InitializePlate@NoteTube | 256/256 | 99.45 | 35/6400 | size-exact reloc-naming (see below) |

5 true-strict-100 REPRODUCE. InitializePlate's 7 `diff_arg` residuals are 100%
reloc-naming, verified against the target binary (orig/45410914/band.exe, PE
map validated: 0x82BF65D0 reads 7d8802a6 = `mflr r12`):
  - [13][24][34] `bl` to `fn_82405660`/`fn_823E66C8`/`fn_824410F0` — these target
    addresses are unnamed in symbols.txt + target_symbol_map; the base calls the
    correctly-demangled `SetGeomOwner@RndMesh`/`SetTransParent@RndTransformable`/
    `AddObject@RndGroup` with byte-identical register setup, and the *adjacent
    named* calls (`SetMat@RndMesh` [16], `SetDirty_Force@RndTransformable` [39])
    resolve and match. Correct-callee, name-only.
  - [40][41][43][44] float-pool loads: objdiff's `lbl_82000980` label is the @ha
    page base, NOT the real EA. Decoding the raw target words gives EA
    0x8200099C = **0x3f800000 = 1.0f** and 0x82000D6C = **0x00000000 = 0.0f** —
    byte-exact matches to the base's `__real@3f800000`/`__real@00000000`.
  HONEST correct identity, size-exact 256/256.
  ⚠ COORDINATOR NOTE: this composes to *exactly* true-100 only if those 3 engine
  callees get named in the map; they currently are NOT, so the composed report
  may read ~99.45. Landable regardless (SOP sanctions 99.4-99.7 size-exact
  reloc-naming; identity is proven, not guessed). Count as 5 strict + 1
  near-strict-honest, not 6 strict.

**2. ICF honesty gate:** none of the 6 addresses are in icf_aliases.map (15-line
map, all VocalTrack-region folds). 4 pins are real-bodied ≥56B (cannot be
stub-folds). The 2 leaf getters are DIFFERENT sizes (12B reads field 0xC; 20B
reads fields 0x10 + 0xC, distinct arithmetic) → not aliased siblings. Body-port
lane, HONEST.

**3. Map ADD-only + splits:** map = 6 pure additions; no collision with main's
pre-existing 0x82B6xxxx VocalTrack/deque<TubePlate*> refs. Splits overlap
self-check = 0/0; text [0x82bf6570,0x82bf6938) fits prev-end 0x82bb2180 / next
0x82bf7cb0, pdata [0x82257ae0,0x82257b00) fits prev-end 0x822566b0 / next
0x82257b68. symbols.txt split arithmetic 0x28+0xC=0x34 correct; main's region is
unchanged (still fn_82BF6548=0x34) → rebases clean.

**4. Compile gate (direct cl.exe 16.00.11886):** PASS, exit 0 (only the standard
__va_start/__frsqrte intrinsic warnings).

**5. MILO_DEBUG landmine:** N/A — NoteTube.h/.cpp have no dev-only members or
#ifdef MILO_DEBUG blocks; the size-exact matches (offset stores 0x124/0x1c/0x110/
0xc0) confirm the struct layout is correct as-is.

**Safety:** main repo confirmed clean of NoteTube in splits.txt/objects.json
(the lane's accidental main-repo edit revert holds). Commit c258d89 is
path-limited (6 files). global_fuzzy_pairs.json left untracked. No contamination.

— WAVE-5 auditor

---

## WAVE-5 AUDIT #2 (independent re-verify, fresh rebuild) — VERDICT: CLEAR

Second independent auditor. Rebuilt the base obj from the committed source
(`ninja-locked`, CoW worktree) and re-ran all 5 audit tasks against c258d89.
Confirms the prior audit; no downgrade.

1. **Per-fn re-verify (objdiff-cli-direct, JSON→file):** 5 of 6 reproduce
   strict — CurrentStartX(12/12), CurrentEndX(20/20), SetDeployTiming(56/56),
   PollDeploy(136/136), SetPointPos(84/84) all `raw=norm=fuzzy=100.0`, `score 0`.
   InitializePlate = 256/256 size-exact, `raw=norm=fuzzy=99.453125`, score
   35/6400. raw==norm here means objdiff does NOT auto-resolve the residual —
   it's 7 `diff_arg` on `bl` to *unnamed* engine callees (fn_82405660/823E66C8/
   824410F0) + float-pool loads; composes to true-100 only once those callees
   are named. HONEST, size-exact, landable per SOP (99.4-99.7 reloc residue).
   **Count = 5 strict + 1 near-strict, NOT 6 strict** (matches prior audit).

2. **ICF honesty:** none of the 6 addrs in build/45410914/icf_aliases.map;
   4 anchors ≥56B real bodies (not stub-folds); the 2 getters are `raw=100`
   byte-exact at DISTINCT sizes (12 vs 20) and distinct addrs → no
   sibling-aliasing. Source is real C++ (no ASM_BLOCK/__asm) → not a fake match.

3. **Map ADD-only + splits:** map = 6 pure EOF additions, no key collisions.
   Overlap scan (all splits vs new ranges): pdata [0x82257AE0,0x82257B00) and
   text [0x82BF6570,0x82BF6938) each hit ONLY their own entry, 0 foreign
   overlaps. symbols.txt split 0x28+0xC=0x34 arithmetic correct.

4. **Compile gate (wibo cl.exe 16.00.11886):** exit 0, obj produced
   (114727 B), only the expected __va_start/__frsqrte intrinsic warnings.

5. **MILO_DEBUG:** N/A — no dev-only members / #ifdef blocks in NoteTube.h/.cpp.

**Safety:** main repo confirmed clean of NoteTube (splits/objects grep = 0,
no unstaged status on the 3 config files). No contamination.

— WAVE-5 auditor #2
