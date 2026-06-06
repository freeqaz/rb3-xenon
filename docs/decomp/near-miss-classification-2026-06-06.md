# Near-Miss Root-Cause Classification & Ranked Lever List (2026-06-06)

Handoff doc. Synthesizes a per-unit near-miss root-cause classification across
~36 sampled units (game, rndobj-mesh, render-xbox, world, bandobj, char-midi,
ham-data) into an executable, ranked lever list. The next agent should be able to
pick up cold from the **NEXT ACTIONS** section.

All struct/header claims below were spot-verified against the live source on
2026-06-06 (Player.h, Movie.h, Mesh.h, FlowSound.h, HamCamTransform.h,
SampleZone.h). The reveal mechanism + reveal_sweep self-validation contract were
verified against `scripts/target_symbol_map.json` (format: `"0xADDR" -> "?mangled"`,
11,980 entries) and `tools/reveal_sweep.py`.

---

## The two reusable primitives (read first)

Almost every lever below is one of two shapes:

### A. struct_offset / coupled-base layout fix
A single header edit grows/shrinks a class (or an embedded member/base subobject)
by N bytes, shifting every member read at/after a boundary by a uniform `[off:±N]`.
Self-evidence = the offset deltas in the diff are **consistent** (one N). When they
**vary** per-function it's NOT a layout lever (it's funclet frame noise or a
per-fn body diff). Fix in the header, rebuild, measure. Flips many fns at once.

### B. reveal (symbol_pairing) — `target_symbol_map.json` entry
objdiff pairs target↔base **by name only**. A target `fn_<addr>` with no map entry
stays anonymous → 0% even when our compiled bytes are identical. Adding
`"0xADDR": "?mangled"` reveals the pairing. **Self-validating**: a wrong address
can't produce a byte-exact normalized match, so any entry the build confirms at
100% is correct (`tools/reveal_sweep.py` emits only word_eq==1.0 candidates).

**CRITICAL funclet caveat.** EH-cleanup funclets typically have TWO diffs:
(1) a `subi r31,r12,0xNNN` *frame-reconstruct* immediate that mirrors the PARENT
function's frame size, and (2) a single dtor `bl` to an unrevealed callee. A reveal
fixes (2) but **(1) is a parent-frame artifact that survives** (documented objdiff
FP, see `project_engine_baseclass_layout_wall.md`). So a reveal takes a funclet to
100% *only if the funclet is otherwise byte-exact* (frame already matches). When the
sampled funclets show **inconsistent** subi deltas across the unit, the parents'
frames differ and the reveal will NOT flip them — those are DROP. reveal_sweep is
self-gating against this (won't emit a non-byte-exact candidate), so running it is
always safe; just don't *count* funclets whose frames diverge.

---

## 1. RANKED SYSTEMATIC LEVERS

Ranked by `est_recoverable × confidence`, GAME-priority tiebreak. "est" is the
classifier's recoverable count for that lever's scope.

| # | Lever | Bucket | Area | Units | est | Confidence | First action |
|---|-------|--------|------|-------|-----|-----------|--------------|
| 1 | **HamCamTransform TransformArea 0x70→0x50** | struct_offset | engine | HamCamTransform | 15 | HIGH (consistent 0x50 everywhere) | Shrink `TransformArea` (`src/system/hamobj/HamCamTransform.h`, embedded `ObjPtrList`/`ObjVector`/`ObjPtr` over-sized) so `sizeof==0x50`; rebuild |
| 2 | **MemTemp RAII inline (MidiParser)** | inline/codegen | engine | MidiParser (+CharBone, MidiInstrument funclets) | 14 | HIGH | Make `MemTemp{MemPushTemp()/~MemPopTemp()}` (`src/system/utl/MemMgr.h`) inline at call sites so it lowers to bare `bl ?MemPushTemp@@YAXXZ`/`?MemPopTemp@@`; shrinks parent frame +16/+48 → flips funclet deltas. Add reveal `0x822605C0 → ??1DataNode@@QAA@XZ` |
| 3 | **LightPreset +0x3C/0x40 shortfall** | struct_offset | engine | LightPreset | 12 | HIGH (Diagnosis "+60 7x") | Grow `LightPreset` base/embedded block by 0x3C (`src/system/world/LightPreset.h`); rebuild. (Secondary: `mSpotlights` stride 4→16 + MemOrPoolAllocSTL — see #4/#11) |
| 4 | **MemOrPoolAllocSTL allocator-entrypoint (binary-wide)** | body_diff | engine | LightPreset, Spotlight, Crowd, CharHair, MeshAnim, EventTrigger, MidiInstrument | ~10+ | MED (needs its own investigation) | STLport `_M_insert_overflow_aux`/vector-grow path: target calls 1-arg `?MemOrPoolAllocSTL@@YAPAXH@Z` + `_Copy_Construct`; ours calls 4-arg `?MemOrPoolAlloc@@YAPAXHPBDH0@Z` + `_Param_Construct`. Find the STLport node-alloc mapping and route grow path through the 1-arg STL allocator. CROSS-CUTS many units |
| 5 | **MidiInstrument SampleZone 0x1c→0x50** | struct_offset | engine | MidiInstrument | 10 | MED (compound, ADSRImpl/ObjPtr) | Realign `SampleZone` (`src/system/synth/SampleZone.h`: `ObjPtr mSample@0x0`, `mVolume@0x14`, `mADSR@0x34`) to element size 0x50; cross-check rb3-Wii (`ObjPtr=0xc, mADSR@0x2c`) |
| 6 | **PostProc_NG / RndPostProc base −12** | struct_offset | engine | PostProc_NG | 9 | HIGH (dead-consistent −0xC) | Remove 12 bytes (3 words) from `RndPostProc` base prefix (`src/system/rndobj/PostProc.h` — pre-NgPostProc members or PostProcessor/Hmx::Object base); rebuild |
| 7 | **RndMesh +4/+8 cascade** | struct_offset | engine | Mesh | 18 | MED-HIGH (two-step +4) | In `src/system/rndobj/Mesh.h`: `VertVector` (modeled 0x10) likely 0x14 in retail (`unkc` int / alignment) → +4 at ~0xe0; a 2nd 4-byte member near `mPatches`/`mBones` → +8 by compressed-vert region. Cross-check rb3-Wii Mesh layout. DC3 annotations match ours, so this is OUR error vs retail |
| 8 | **VocalPlayer / Player base +4 @0x260** | struct_offset | **GAME** | VocalPlayer (+ all instrument players) | 6 | HIGH | Widen `unk25c` or `unk260` vector by 4 in `src/band3/game/Player.h:218-219` so members ≥0x260 shift +4 (mBandEnergy 0x26c→0x270, mSingers 0x350→0x354…). MAJOR coupled base: Guitar/Bass/Drum/RealGuitar/Keyboard/Vocal all derive Player |
| 9 | **FlowSound 0xcc→0xa4 (−0x28) coupled base** | vtable_slot | engine | FlowSound (+ flow/ family) | 5 | HIGH (header says 0xa4 exact) | Shrink `FlowNode`/`FlowLabelProvider` base subobject (or `FlowPtr<Sound> mSound`) by 0x28 in flow headers so `sizeof(FlowSound)==0xa4`; flips `??_GFlowSound` thunk + through-base funclets. Multi-unit (FlowSay/FlowSubdir) |
| 10 | **EventTrigger frame −0x70/−0x60 + Anim node 0x24→0x3c** | mixed | engine | EventTrigger | 5 | MED | (a) Match Handle/PropSync parent frame (`-0x60` not `-0x70`, one extra spilled local/by-value temp). (b) `_M_create_node` `li r3,0x24`→`0x3c` (+0x18 too small) + MemOrPoolAllocSTL (see #4) |
| 11 | **RockCentral map<Symbol,String>→map<Symbol,DataNode>** | body_diff | **GAME** | RockCentral | 5 | HIGH | In `UpdateFriendList`, the local/static `dataPoint` map value type is wrong: it's a `DataPoint = pair<Symbol,DataNode>`, not `Symbol→String`. Correct the type → re-emits correct node dtor, flips 5 funclets. (Parent itself stays mixed: also regalloc + bne/beq logic split) |
| 12 | **ContentMgr OggFree↔Content::~/operator delete ICF reveal** | symbol_pairing | engine | ContentMgr_Xbox (+ FlowSound) | 5 | HIGH | oggvorbis `_ogg_free→OggFree` ICF-merged with `Content::operator delete`/`~Content`; objdiff resolves merged COMDAT to `OggFree`. Add merged-symbol reveal mapping that addr → `??3Content@@SAXPAX@Z`/`??1Content@@UAA@XZ` |
| 13 | **MoviePanel embedded Movie 8→12 bytes** | struct_offset | **GAME** | MoviePanel | 5 | HIGH | `src/system/movie/Movie.h` has only `mFaderGroup@0x0 + mImpl@0x4` (8 bytes); needs one more 4-byte member after `mImpl` so `mSubtitlesLoader` lands at 0x64 not 0x60. Flips MoviePanel fields ≥0x60 |
| 14 | **DataFunc ICF DataNode/String dtor reveal** | symbol_pairing | engine | DataFunc (+ DataFile, Gen funclets) | 4 | MED (subi noise remains) | `fn_827E2560` is ICF-merged `??1DataNode@@`+`??1String@@`; one reveal pairs the `bl` in all 8 funclets. Per-funclet subi delta survives (FP) |
| 15 | **DataNode/DataArray dtor reveal sweep (binary-wide [sym] half)** | symbol_pairing | mixed | NetSync, StorePanel, BandDirector, Rnd, Part, Spotlight, CameraShot, Crowd, MidiParser, MidiInstrument, CharBone, Gen, DataFile, +~690 fns | partial | LOW-as-counter | Run `tools/reveal_sweep.py` repo-wide (`0x822605C0→??1DataNode@@`, `0x82260320/288→??3DataArray@@/??1MessageTimer`). Self-validating; flips ONLY byte-exact funclets. Treat as cleanup, NOT a count lever (subi caveat) |

### Notes on ranking
- **#1, #6, #8, #9, #13** are the cleanest single-N flips (highest confidence per fn).
- **#8, #11, #13** are GAME-priority (matching effort policy favors `src/band3/`).
- **#2 (MemTemp)** and **#4 (MemOrPoolAllocSTL)** are codegen/STLport levers that
  cross-cut many units — the highest *breadth* but #4 needs its own investigation
  before it pays off (MED confidence).
- **#15** is the binary-wide reveal sweep. It's listed last because the funclet
  subi-delta caveat means it mostly fixes the cosmetic `[sym]` half; it is
  measurement-noise cleanup, not a reliable count lever. Run it anyway (safe,
  self-gating) — it will pick up whatever funclets are already frame-matched.

---

## 2. PERMUTER-CLASS (regalloc / scheduling) → /permute sweep

These have no header/reveal lever; the residual diffs are register swaps,
commutative operand reorders, or decl/scheduling order. Candidates for a
`/permute` sweep (per-function source-variation search).

| Unit | Group | What's permuter-class | near_count (sampled fns) |
|------|-------|-----------------------|--------------------------|
| **Geo** | render-xbox | float regswaps (f12↔f13, f25↔f27), commutative fmadds/fmuls reorders, Vec component x/y load-order swaps (math expr ordering) | 7 (whole unit) |
| **MeshAnim** (named STL fns) | rndobj-mesh | `_M_insert_overflow_aux`×2, operator>>/<<: stable r23↔r24 swap (6×) + allocator body | 2 of 15 (rest funclets) |
| **StreakMeter / SyncObjects** | game | 18 regswaps (r26↔r27 10×, r15↔r17…), insert/delete scheduling clusters | 1 of 7 |
| **CharBone / StuffBones** | char-midi | uniform local-packing shift (stack temps 8/4 tighter) = decl-reorder | 1 of 6 |
| **CharHair / _M_insert_overflow_aux** | char-midi | r23↔r24 cascade + STLport allocator [sym] | 1 of 8 |
| **Rnd_Xbox / BeginTiling, SetupGamma** | render-xbox | fctidz/stfd reorder; ld+rldicl vs lhz body+regalloc | part of 12 |

**Permuter unit total: 6** (Geo, MeshAnim, StreakMeter, CharBone, CharHair,
Rnd_Xbox). Geo is the densest pure-permuter target — run `/permute` per function
in `math/Geo.cpp` first.

---

## 3. FUNCLET-NOISE → DROP

Units whose near-miss pool is dominated by EH-cleanup funclets with **inconsistent**
`subi r31,r12` frame-reconstruct deltas (per-parent objdiff false positives). The
reveal sweep (#15) cleans the cosmetic `[sym]` half but cannot flip them to 100%
because the parent frames differ. `est_recoverable ≈ 0`. Filter these from the
near-miss working pool.

| Unit | Group | near_count | est_recoverable | Why DROP |
|------|-------|-----------|-----------------|----------|
| **TexBlender** | rndobj-mesh | 10 | 0 | subi deltas +64/−96/−16/+48 (incoherent) |
| **PartAnim** | rndobj-mesh | 3 | 0 | inconsistent frame deltas |
| **DepthBuffer3D** | render-xbox | 20 | 0 | deltas −316/+172/−80/−48/+32 |
| **BandDirector** | bandobj | 29 | 0 | all 29 funclets, subi −4..−112 varies |
| **Rnd** (funclet half) | rndobj-mesh | 20 of 22 | 2 (OnClearColorR/G only) | subi +80/−16 varies; 2 real DataArray −4 fns survive |

Plus the funclet *majority* inside otherwise-mixed units (Part 26, MeshAnim ~11,
CameraShot 3, DataFile 20, Gen ~13, Crowd ~12, Spotlight most) — work the REAL
sub-population, drop the funclet remainder.

**DROP unit total (whole-unit funclet noise): 5** (TexBlender, PartAnim,
DepthBuffer3D, BandDirector, Rnd).

---

## 4. BODY-PORT GRIND → per-function, lower priority

Real but one-off: missing super-calls, wrong branch logic, template/overload
mismatches. No shared lever; each is its own port. Lower priority than the
systematic levers above.

| Unit | Function(s) | Fix |
|------|-------------|-----|
| **Group** | `SetFrame@RndGroup` (96.6%) | Add missing `bl RndAnimatable::SetFrame` super-call; load mFrame from r30 not r3. Grep other RndAnimatable subclasses' SetFrame for the same pattern |
| **MeshAnim** | `_M_insert_overflow_aux`, operator>>/<< | allocator-entrypoint body (folds into #4) + regalloc |
| **CharHair** | `SetRoot` (86.4%) | bne→beq branch flip + inserted `fmr f0,f31; b` (float/NaN-guard control flow) |
| **Rnd_Xbox** | `Present`, `DoPostProcess`, `SetupGamma` | DxRnd −4 layout (unit-local) + XDK D3DDevice_* reveals + real body (subic/subfe bool idiom, 64-bit ld vs lhz) |
| **DataFile** | `DataWriteFile` (98.29%) | per-callee reveals + possible `TextStream::operator<<(Symbol)` vs `(PBD)` overload |
| **Gen** | `RndGenerator::Generate` | reveal float-pool + named callees (RandomFloat/MakeRotMatrix/WorldXfm_Force/AllocParticle) + 1 scheduling cluster |
| **TexRenderer** | `InitTexture` | unit-local RndTexRenderer +4 (0x48-0x58) + +12 (0x6e) — targeted header audit, flips ~1 |
| **RockCentral** | `UpdateFriendList` parent | after #11 type fix: still regalloc r25↔r21 + bne/beq logic split |
| **Crowd** | `BuildBillboard` (96.51%) | wrong vector template (vector<RndBone> vs vector<Face@RndMesh>) + scheduling reorder + RndMesh +4 (folds into #7) |

---

## NEXT ACTIONS (execute cold, in order)

Each is a worktree task (`scripts/setup_worktree.sh`), build with
`./tools/ninja-locked 2>&1 | tee /tmp/rb3_build_<task>.log`, measure with objdiff.
Do header edits and per-fn ports in isolated worktrees; land net-positive patches
to main one at a time. Never `git stash`/`checkout files` in the main tree.

### Tier 1 — high-confidence single-N struct flips (do these first)
1. **HamCamTransform** (#1, est 15): shrink `TransformArea` embedded ObjPtr/
   ObjPtrList/ObjVector so `sizeof==0x50`. Verify with `?_M_erase@?$vector@VTransformArea`
   target `li r10,0x50`. Cross-ref `project_objptr_relayout_migration.md`.
2. **PostProc_NG** (#6, est 9): remove 12 bytes from RndPostProc base prefix in
   `src/system/rndobj/PostProc.h`. Confirm all CheckXXX accessors flip to `[off:0]`.
3. **VocalPlayer/Player** (#8, est 6, **GAME**): +4 in `src/band3/game/Player.h:218`.
   After landing, re-measure ALL Player-derived units (Guitar/Bass/Drum/RealGuitar/
   Keyboard) — coupled base may flip more than the 6 VocalPlayer fns.
4. **FlowSound** (#9, est 5): −0x28 in FlowNode/FlowLabelProvider/FlowPtr so
   `sizeof(FlowSound)==0xa4`. Then re-measure flow/ family (FlowSay, FlowSubdir).
5. **MoviePanel** (#13, est 5, **GAME**): +4 member after `mImpl` in
   `src/system/movie/Movie.h`. Cross-check rb3-Wii Movie layout for the real member.

### Tier 2 — GAME body fixes + ICF reveals (fast, GAME-priority)
6. **RockCentral** (#11, est 5, **GAME**): fix `UpdateFriendList` map value type
   to `DataNode`. Use `mcp__orchestrator__lookup_rb3wii` for the real container type.
7. **ContentMgr_Xbox** (#12, est 5): merged-symbol reveal OggFree→Content dtor/delete.
   Use `mcp__orchestrator__lookup_merged_symbol`.
8. **DataFunc** (#14, est 4): merged-symbol reveal `fn_827E2560`→DataNode/String dtor.

### Tier 3 — bigger struct fixes (cross-check rb3-Wii first)
9. **Mesh** (#7, est 18): VertVector 0x10→0x14 + 2nd +4 near mBones. Verify rb3-Wii
   Mesh layout before editing (DC3==ours, so it's our-vs-retail error).
10. **LightPreset** (#3, est 12): +0x3C base/embedded grow. Then Spotlight/Crowd
    re-measure (shared DataNode funclet reveals + member deltas).
11. **MidiInstrument** (#5, est 10): SampleZone→0x50; cross-check rb3-Wii ADSR/ObjPtr.

### Tier 4 — codegen/STLport levers (need investigation, broad payoff)
12. **MemTemp inline** (#2, est 14): make MemTemp RAII inline in
    `src/system/utl/MemMgr.h`; re-measure MidiParser + CharBone + MidiInstrument funclets.
13. **MemOrPoolAllocSTL** (#4, est 10+): investigate STLport node-alloc mapping;
    route vector-grow through 1-arg STL allocator. Touches LightPreset, Spotlight,
    Crowd, CharHair, MeshAnim, EventTrigger, MidiInstrument. Spike first.

### Tier 5 — permuter + cleanup
14. **/permute sweep**: Geo (densest), then MeshAnim/StreakMeter/CharBone/CharHair/
    Rnd_Xbox named fns. Use the `/permute` skill per function; keep TRUE-100 only.
15. **Repo-wide reveal sweep** (#15): `tools/reveal_sweep.py` (self-validating).
    Cleans byte-exact funclets; do NOT count diverged-frame funclets.

### Drop / deprioritize
- DROP whole-unit funclet noise: TexBlender, PartAnim, DepthBuffer3D, BandDirector, Rnd.
- Body-port grind (§4) is per-fn, lower priority than Tiers 1-3.

### Verification contract
- Every struct edit: rebuild + objdiff; offset deltas must collapse to `[off:0]`
  for the affected accessors. If deltas STAY varied, it was funclet noise — revert.
- Every reveal: build must confirm 100% (reveal_sweep won't emit non-byte-exact).
- Coupled-base fixes (Player, FlowSound, Mesh, RndPostProc): re-measure the WHOLE
  derived family, not just the sampled unit — that's where the leverage compounds.
