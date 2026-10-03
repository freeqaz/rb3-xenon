# W16-OL — sub-100 rows in `src/system/{bandobj,char,world,beatmatch}` (2026-10-03)

**Branch** `w16-ol`, started off main `8bba58005`, rebased onto `92074833f` (Merge W16-OK) after the first A/B.
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`). The permuter was not run, and no
alias was added (`git diff main w16-ol -- scripts/symbol_aliases.json` is empty). `src/network`, the Quazal block
and `src/band3` (lane W16-OK) were not edited.

## 1. Population

Rows with `fuzzy < 100` whose unit's `base_path` (from `objdiff.json`) lies under the four directories, on main's
`report.json`: **620 rows / 208,276 B** (bandobj 87,752 · char 61,412 · world 40,956 · beatmatch 18,156). That
reproduces the brief's "about 210 KB". 357 rows / 157,228 B sit at fuzzy ≥ 90.

⚠ Keying on the unit *name* finds only **55 rows / 12,324 B**, because most of these units have bare split
headings (`default/Spotlight`, not `default/system/world/Spotlight`). The census must key on `base_path`.

Charge classes from W16-NA's classifier (`~/tmp/w16na/cls.py`), run over all 620 rows:

| class | rows | bytes |
|---|---:|---:|
| STRUCT_INSDEL | 348 | 134,752 |
| REG_ONLY | 41 | 29,224 |
| IMMEDIATE | 85 | 20,088 |
| NAME_ONLY | 122 | 13,876 |
| OPCODE:replace | 15 | 5,032 |
| STACK_REG / NAME+REG / other | 9 | 5,304 |

The plan was to fan the work out to five forks. The harness refused it (concurrent-subagent cap), so the lane
worked the population serially. It worked through the rows in this order:
- every row with ≤ 6 charged instructions;
- then a scan for **one-sided blocks**: runs of ≥ 3 consecutive instructions that only retail or only we have
  (`~/tmp/w16ol/blocks.py`, 75 rows). This scan found almost every real fix below.

## 2. Whole-binary A/B

**Final, on the rebased branch** (main `92074833f`, fresh worktree, run dir
`~/tmp/wt-w16ol-ab2/.ab_measure_runs/20261003-022640-branch2-1611554/`):

```
leg A: matched=53193 masked=25130 honest=28063 code%=57.270410  (recompiles: 0, settled)
leg B: matched=53215 masked=25132 honest=28083 code%=57.308697  (recompiles: 596, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+22  Δmasked_equal=+2  Δhonest=+20  Δcode%=+0.038287pp  Δcode_bytes=+3924
unit REGRESSIONS: default/band3/bandtrack/GemRepTemplate (35->34)
```

Predicted, before the run, as unchanged from the first A/B: W16-OK touched none of these rows. **Measured:
identical deltas.**

First run, before the rebase:

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16ol-ab --patch <git diff main w16-ol>`, run on a fresh
`scripts/setup_worktree.sh` worktree at main `8bba58005`. Patch kinds: map + source, 19 paths. Forced re-split on
both legs; both read at a `symbols.txt` fixed point after 0 extra splits. Run dir
`~/tmp/wt-w16ol-ab/.ab_measure_runs/20261003-021951-branch-1563072/`.

```
leg A: matched=53165 masked=25127 honest=28038 code%=57.205140  (recompiles: 0, settled)
leg B: matched=53187 masked=25129 honest=28058 code%=57.243430  (recompiles: 596, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+22  Δmasked_equal=+2  Δhonest=+20  Δcode%=+0.038290pp  Δcode_bytes=+3924
Δfuzzy=+0.010243pp   (legA 63.507835 -> legB 63.518078)
unit net (ALL units) = +22   vs whole-binary Δmatched = +22
unit REGRESSIONS: default/band3/bandtrack/GemRepTemplate (35->34)
units at 100% [mpn]: 510 -> 510 (CharIKFoot reached, GemRepTemplate fell off)
```

**Prediction, written before the run:** the in-tree row diff against main's `report.json`, which read
+22 fns / +3,924 B with one row down. **Measured: +22 / +3,924 B, identical.** Leg A equals main's own
`report.json` (53,165 / 57.205140). The `none` control does not apply, because the patch carries source.

**Row level** (in-tree row diff, `~/tmp/w16na/rowdiff.py`): **21 rows up**, of which 19 reach fuzzy 100.
**1 row down** (§4). 3 anonymous rows were renamed to the functions written here (all now 100).

## 3. What closed, and why (each is one commit)

Behaviour fixes first. Each fix changes what the code does.

| row(s) | B | before → after | defect, read off retail bytes |
|---|---:|---|---|
| `LightPreset::ApplyState` + new `Keyframe::ApplyStageKit` (0x824AAE10) | 108 + 148 | 92.59 → 100, 0 → 100 | **Keyframes never reached the StageKit.** Retail ends ApplyState with a call that pushes the keyframe's LED pattern/state per channel (0 blue, 1 green, 2 yellow, 3 red), the strobe, then a commit (0x82521BF0/B98/22028/1E20). Ours dropped the call. A file-static helper got inlined; retail keeps it out of line, so it is written as a `Keyframe` member. |
| `TubePlate` ctor, new `TubePlate::AllocateVerts` (0x82C29728), new `VertVector::reserve` (0x82418388) | 164 + 160 + 52 | 87.78 → 100, 0 → 100, 0 → 100 | Retail reserves the vert buffer in the ctor, and AllocateVerts grows it in `ceil`'d `mAllocationCount` steps. Ours only resized. `reserve` is written from its 52 B retail body. |
| `PatchDir::SaveFixed` / `LoadFixed` | 400 + 360 | 87.07 → 100, 80.38 → 100 | **Save format:** retail writes and reads no has-layers byte (both sides re-derive the branch from `HasLayers()`). Ours wrote and read one, so our fixed-size patch save was one byte off retail's. The byte count is a u16. |
| `PatchDir::DrawShowing` | 148 | 43.43 → 100 | Retail draws the patch layers with sampler 0's MipMapLodBias set to −1.0f (`.data` 0x82C6B648), saved and restored around the loop (the XDK helpers rnddx9/Mesh.cpp already uses). Ours drew with whatever bias was current. |
| `LayerDir::Layer` ctor | 256 | 88.75 → 100 | Retail initialises `mColor` to white; ours left it uninitialised. |
| `TrackPanelDirBase::SetConfiguration` | 276 | 84.06 → 100 | Retail returns after `Handle(apply)`: no HUD re-show. That is the same shape ReapplyConfiguration already documents; ours is now native-only. |
| `SpotlightDrawer::DrawAdditional` | 88 | 61.41 → 100 | `Spotlight::GetAdditionalObjects` returned the `ObjPtrList` **by value** (copy plus destroy on every draw). Retail walks the member list in place, so it now returns a `const&` (its only caller). |
| `BandCrowdMeter::Reset` (via `InitialCrowdRating`) | 232 | 75.21 → 100 | Retail `InitialCrowdRating` uses a function-local `static Symbol("easy")`. That keeps it out of line, and ours had been inlined into Reset. |

Shape and spelling fixes. Each of these is the same behaviour spelled the way retail's codegen shows:

| row | B | before → after | lever |
|---|---:|---|---|
| `CharIKFoot::DoFSM` | 892 | 98.86 → 99.96 / **mpn 100** | finger x/y copied as one 8-byte aggregate (retail uses `lwz/stw`, not FPRs) |
| `OverdriveMeter::StopDeploy` / `EnergyReady` | 176 + 272 | 84.09 / 82.26 → 100 | function-local `static Symbol` moved **inside** the `if (mPulseAnimGroup)` |
| `GemTrackDir::CrashFill` | 196 | 81.63 → 100 | same: `static Message` inside the `mParent` check |
| `VocalTrackDir::ShowPhraseFeedback` | 328 | 81.61 → 100 | `std::min(n, singers)` (retail's reference select and slot order), and the `perfect_harmony` static inside its branch |
| `BandCrowdMeter::GetPeakValue` | 108 | 76.44 → 100 | one `1.0f` local reused as return and threshold; loop nested under the peaks check |
| `operator>>(BinStream&, LayerDir::Layer&)` | 316 | 68.84 → 100 | read `mBitmapList` through the list `operator>>` template (retail makes one out-of-line call, 0x82328800) |
| `VocalTrackDir::GetLyricColor` | 160 | 99.40 → 100 | unlit colour is `Hmx::Color(0)` (the packed ctor stores alpha first, as retail does) |
| `SongSectionController::Load` (CharIKFingers unit) | 232 | 94.48 → 99.40 / **mpn 100** | `gRev`/`gAltRev` made file statics. See §5.2 for the aggregate attempt that regressed. |
| `GemTrackDir SemitoneToWhiteKey` | 96 | 45.79 → 50.00 | no switch case for C/C# (retail's table starts at kNoteD). The rest is a carve artefact, see §6. |

**Levers worth carrying forward:**
1. **A function-local static declared before a null check.** Retail constructs it inside the guarded branch.
   This held in 4 of 4 rows here.
2. **"Ours has the call inlined, retail calls it"** usually means retail's callee has a guarded static local
   (InitialCrowdRating) or is a member rather than a file-static (ApplyStageKit).
3. **One-sided-block scan.** A run of ≥ 3 retail-only instructions found 8 behaviour bugs. Ranking by charge
   class or fuzzy would never have surfaced them.

## 4. The row that went down

`?CreateTail@GemRepTemplate@@QAAPAVRndMesh@@XZ` (band3, 104 B): 100 → 99.81. **Exposure, not a regression.**
Naming 0x82418388 `VertVector::reserve` turned the forgiven placeholder into a checked name. Retail CreateTail
calls `VertVector::reserve` where our source calls `resize`, which is a real wrong-callee bug. `src/band3`
belongs to lane W16-OK, so it is **not fixed here**. Checked on retail bytes: every other instruction of the
row is equal, and the only charge is the callee. The fix is the one-word change
`verts.reserve(GetRequiredVertCount(count))` at `src/band3/bandtrack/GemRepTemplate.cpp:132`. Today our new tail
mesh starts with N live (garbage) vertices instead of N reserved.

## 5. Tried and reverted (negative results)

1. **ChordShapeGenerator `vertIt`/`faceIt` (+4 vs retail −4).** Neither declaration order nor linkage (static →
   external) moves `.bss` order (measured on scratch compiles). **Renaming does**: `aVertIt` moves to offset 0.
   So the order follows the symbol-name hash. Retail's names are the oracle's `vertIt`/`faceIt`, so the
   difference comes from a different symbol set in the TU, not the names. Inventing names to hit the layout
   would be fitting, so nothing landed. BuildChordMesh (1,680 B) and BuildContourCap (1,752 B) stay at 99.99.
2. **SongSectionController revs as one aligned aggregate.** Load reached 100, but
   `operator>>(PracticeSectionMapping)` fell 100 → 98.5, because retail reads `rev` through its own symbol.
   File statics are right, and their residual order is the same hash artefact.
3. **WorldCrowd revs as file statics.** CharDef::Load (132 B) crossed, but WorldCrowd::Load (1,140 B) fell to
   99.96 (`rev` landed at +0xC). Kept the aggregate. Crowd.cpp's own comment already says so.
4. **`RB3_TU_OBJPTR_FORCEINLINE_CTOR` on CharHair.cpp.** Strand/Point ctors crossed (+444 B), but four
   PropSync/resize rows fell off 100 (−832 B). The switch is TU-wide and too blunt.
5. **Inert spellings:** KeyboardTrackWatcherImpl `TrackForgivesFatFingering` defined before its caller;
   NgSpotlightDrawer::RenderScene `sLights.size()`; CharIKFoot `Scale()` in place of `*=`.

## 6. Leads left (not done)

- **`MemRealloc` / `MemTruncate` release ABI.** Retail CharClip::Transitions::Resize (200 B, 79.8) calls
  `MemTruncate(ptr, size)` and `MemRealloc(ptr, size, 0)` with no debug strings. That is the same ABI as the
  existing `!HX_NATIVE` `MemAlloc(int, int)` overload. Fixing it means new MemMgr overloads plus a map rename,
  which is outside these four directories.
- **`InitialCrowdRating` (0x822BB300, 124 B)** sits inside CrowdMeterIcon's `.text` pin. It pairs only after
  being re-homed to BandCrowdMeter.
- **`SemitoneToWhiteKey`:** retail's case stubs are carved as their own 48 B row `fn_822E3630` after the
  96 B `.pdata` extent. That is a split carve fix, not source.
- **0x82328800**, the LayerDir list `operator>>` instance (120 B), is left unnamed. PanelDir already pairs a
  different 124 B retail instance under the same mangled name.
- **`Find<T>(name, false)` argument scheduling** (StreakMeter::SyncObjects 1,328 B, OverdriveMeter::SyncObjects
  332 B at 69%, PitchArrow, CharIKFingers::SetName 2,076 B). This is the wall STRUCT_HEADS_2026-09-10 already
  recorded. Every site in OverdriveMeter differs, and `false` → `0` is inert.
- **Fold-shaped NAME_ONLY rows** (`_M_fill_insert<…>`, `push_back<…>`, `resize<…>`): not adjudicated here, and no
  aliases were added.
- Most of the remaining ~200 KB is scheduling, register or stack-slot residue at fuzzy 98–99.99 (e.g.
  Spotlight::SyncProperty's whole-function r24/r25 swap, CamShot::Load's 16-byte frame delta).

## 7. Gates

- A/B: §2 (`rc=0`, tree restored and verified by the tool).
- Native gate, run last, on the final source tree (only this doc changed afterwards):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`
