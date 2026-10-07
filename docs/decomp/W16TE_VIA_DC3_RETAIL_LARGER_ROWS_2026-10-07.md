# W16-TE: the ten VIA-DC3 source-class rows where retail's body is larger

Lane W16-TE, 2026-10-07, worktree `~/tmp/wt-w16te`, branch `w16-te`, base main `e0cca0a7f`.
Brief: `CAMPAIGN_STATE_2026-10-07b.md` §6 lever 1, "start with the ten rows where retail's body is
≥16 B larger (18,720 B)". Find what retail's body does that ours does not, fix the source where
retail's bytes support it, price it with `tools/ab_measure.py`, and give every row a disposition.

Ruler: the shipped graded `name_check`. Per-row numbers are `fuzzy_match_percent` from the
worktree's `report.json` after a full `tools/ninja-locked`, before vs after, same worktree. The
whole-binary price is in §5.

## 1. Method

For each row I diffed retail against ours with objdiff's full listing (`--include-instructions
--full-listing`, graded ruler) and checked three things in order:

1. **Calls.** Every `bl`/`b` to a symbol that differs, with folds checked against
   `scripts/symbol_aliases.json` before calling anything a wrong callee.
2. **Stores and loads that exist on one side only.** A field retail never writes, or a value
   retail re-reads from memory, is a statement about the source.
3. **Constants.** The multiset of float constants each side loads, with retail's values read from
   `orig/45410914/band.exe`.

What was left after that was register allocation, scheduling, or block layout, and I recorded it
as such without a codegen fight.

## 2. Result

| row | size | retail larger by | fuzzy before | after | disposition |
|---|---:|---:|---:|---:|---|
| `RndAmbientOcclusion::Tessellate` | 4,796 | +204 | 67.40 | 74.68 | **FIXED** (behaviour + source shape), rest HOME |
| `RndMesh::Load` | 3,452 | +20 | 99.41 | 99.41 | HOME |
| `RndText::WrapText` | 3,016 | +44 | 83.40 | 84.29 | **FIXED** (behaviour), rest SCHED |
| `Spotlight::BuildNGCone` | 1,692 | +16 | 74.90 | 74.90 | SCHED |
| `RndAmbientOcclusion::SmoothResults` | 1,672 | +160 | 68.02 | 74.79 | PARTIAL (source shape), rest HOME |
| `CharBonesSamples::Relativize` | 1,448 | +24 | 96.47 | 96.47 | LAYOUT |
| `RndLine::UpdateLine(Point*,Point*)` | 1,328 | +40 | 70.66 | 73.21 | PARTIAL (scaffold call removed), rest SCHED |
| `BoxMapLighting::ApplyQueuedLights` | 520 | +44 | 73.77 | 73.77 | SCHED |
| `SynthEmitter` ctor | 496 | +16 | 86.13 | 86.13 | INLINE, blocked by PCH |
| `NgDOFProc::Set` | 300 | +20 | 81.40 | 89.57 | PARTIAL (source shape), rest SCHED |

Two behaviour fixes, both visible to native: `Tessellate` no longer prints four progress lines that
retail does not print, and `WrapText` no longer sets `Line::startIdx`/`endIdx`, which retail leaves
at 0. One latent defect: `Tessellate` read faces and vertices through a second `mGeomOwner`
dereference that retail does not do.

No row reached 100, so no bytes were expected to move (§5).

## 3. Per row

Classes: **FIXED** (a retail-attested change applied), **PARTIAL** (a source-shape change applied;
what remains is given), **HOME** (retail has dead stores into one frame slot that we do not emit),
**SCHED** (register / FP-order / scheduling only; no call, store or constant difference names a
source construct), **LAYOUT** (block placement only), **INLINE** (retail inlines a callee we call).

### 3.1 `RndAmbientOcclusion::Tessellate` (4,796 B, +204 B): FIXED + HOME, 67.40 → 74.68

- **Prints (behaviour).** Ours printed four progress lines through `TheDebug << MakeString(...)`.
  Retail's call list contains no `MakeString` and no `TextStream::operator<<`, so retail prints
  nothing. They are now `MILO_LOG`, which is `((void)(...))` in the match build and still prints
  under `HX_NATIVE`.
- **Double dereference (latent defect).** Ours cached `geomOwner = mesh->GetGeomOwner()` and then
  called `geomOwner->Faces()` / `geomOwner->Verts(i)`. `RndMesh::Faces()` and `Verts(int)` already
  go through `mGeomOwner` (`Mesh.h` lines 249-252), so ours read `+0x110` twice (`lwz 0x110` ×2);
  retail reads it once. The two agree only when the owner owns itself. All accessors in the range now
  go through `mesh->`. `mesh->GetGeomOwner()->NumVerts()` stays because it is a single dereference.
- **Face builds.** The 23 `f.v1 = a; f.v2 = b; f.v3 = c;` builds are `f.Set(a, b, c)`. Retail homes
  each `int` argument of the inlined `Face::Set` once per use into `0x50(r31)`: 9 in phase 2, 9 in the
  three-way split, 6 in the two-way split.
- **What remains: HOME.** Retail has **61** dead stores into `0x50(r31)`; ours had 21 and now has 38.
  Retail also homes the 16-bit face indices with `sth`, which I did not reproduce. The home count is a
  count of inlined accessor calls in retail's source, so the gap says retail's source made about 23
  more accessor calls than ours. I did not find which ones.
- Side effect: the unwind funclet `fn_82492CEC` moved 99.9 → 99.8. It was already below 100.

### 3.2 `RndMesh::Load` (3,452 B, +20 B): HOME, 99.41, no edit

Retail has exactly 5 more instructions. Four are `this` homes: `mPatches.clear()`, `mVerts.begin()`
×2, and `mBones.empty()` (via `IsSkinned`). The construct that produces them is inside the inlined
callee bodies (`VertVector`, STLport), not in `Load`. Behaviour is equal. Not reproduced.

### 3.3 `RndText::WrapText` (3,016 B, +44 B): FIXED (behaviour), 83.40 → 84.29

- Retail never sets `Line::startIdx` / `endIdx` on the lines it emits. It never loads
  `WrapPoint::charIdx` and never stores to the line temporary's `+0x2c` / `+0x30` frame slots; the only
  stores there were ours. The fields therefore keep `Line()`'s 0. The two stores are removed.
- The trim loop reads `mEnd[-1]` before the `mEnd > mStart` test, and the walk advances through
  `wps[wp->nextIdx]`, both in retail's order.
- The `??2` / `??_U` and `??3` / `??_V` callee differences are installed folds (groups `0x827bd2f0`
  and `0x8240ddb0`), not behaviour.
- What remains: in the DP loop, retail re-reads the spilled `this`. Register and scheduling only.

### 3.4 `Spotlight::BuildNGCone` (1,692 B, +16 B): SCHED, 74.90

No call differences. Both sides load the same 6 float constants. Nothing names a source construct.

### 3.5 `RndAmbientOcclusion::SmoothResults` (1,672 B, +160 B): PARTIAL + HOME, 68.02 → 74.79

- Retail has 30 `geomOwner` homes and ours had 0. Those 30 are one per `mesh->Verts(i)` /
  `mesh->Faces(i)` call: 7 in phase 1, 2 per weld iteration, 3 per matched corner in phase 3. Ours
  hoisted `Vert *verts = &mesh->Verts(0)` and indexed it.
- Phase 1 now reads the face, then each of the three corners through `mesh->Verts(idx)`. Phase 2 reads
  both vertices in the inner loop. Phase 3 reads `cur` / `next` / `prev` through `&mesh->Verts(...)`.
  The hoisted pointers are gone. Behaviour is equal.
- What remains: we now emit 24 homes. The missing 6 are the `sth` homes of the 16-bit indices, and our
  home slot is `0x54` where retail's is `0x50`.

### 3.6 `CharBonesSamples::Relativize` (1,448 B, +24 B): LAYOUT, 96.47

Retail gives the compression-0 quaternion loop its own 6-instruction setup block, where we
tail-merge it with the other compression path. The loop bodies are equal and behaviour is equal.

### 3.7 `RndLine::UpdateLine(Point*, Point*)` (1,328 B, +40 B): PARTIAL, 70.66 → 73.21

Ours called a `__declspec(noinline)` helper `_outline_back` (a scaffold used nowhere else); retail
calls nothing there. Retail reads `mPoints`' end inline inside the phase-4 branch, and re-reads begin
and end after the copy, before `MapVerts`. The helper is removed, phase 4 uses `&mPoints.back()`
inline, and the begin/end locals moved to phase 5. Behaviour is equal, because phase 4 does not
resize `mPoints`. What remains is scheduling.

### 3.8 `BoxMapLighting::ApplyQueuedLights` (520 B, +44 B): SCHED, 73.77

I checked all 18 accumulators against retail and they are equal. Retail emits 9 `fneg` where we emit
6, uses one more FPR (`__savefpr_14` vs our `_15`), and spills. FP codegen only.

### 3.9 `SynthEmitter` ctor (496 B, +16 B): INLINE, blocked by PCH, 86.13

- Retail inlines `mListener`'s owner-only `ObjPtr` ctor as three stores at `+0xf8`, in the plain
  order. We call it.
- The float constants match retail exactly: 10, 100, 0, −40.
- The tree's switch for this, `RB3_OBJPTR_INLINE_OWNER_CTOR` (`Object.h` lines 515-600), is a per-TU
  `#define`. `synth/` is a PCH directory and `Object.h` is baked into the PCH, so the define cannot
  take effect in `synth/Emitter.cpp`, and there is no per-object PCH opt-out. I tried it, confirmed it
  was inert, and reverted it. Fixing this needs a PCH opt-out, which is outside this lane.

### 3.10 `NgDOFProc::Set` (300 B, +20 B): PARTIAL, 81.40 → 89.57

- Each depth computation loads both camera planes at its own top. The second pair is re-read after
  the `mDepthOfFieldScale` store. Block 1 is `if (z < near) 0 else …`, as retail's
  `bge`-to-compute layout shows.
- **Failed prediction:** calling the accessors at every use measured 81.40 → **76.01**. Per-block
  locals reached 79.44, and inverting block 1 reached 89.57.
- What remains: retail re-reads `mMaxBlur` (`0x44`) and `mBlurDepth` (`0x3c`) from memory, where we
  forward the stored value.

### 3.11 Side check

`RndMultiMesh::UpdateGeometryBuffers` uses the same `GetGeomOwner()->Verts()` double-dereference
spelling, but it is at 99.90, so retail does the double read there. This is not a tree-wide defect
class.

## 4. The HOME pattern

In both AmbientOcclusion rows, retail's extra bytes are mostly dead stores into one frame slot,
`0x50(r31)`. Each is the home of an inlined call's `this` or `int` argument. Every `mesh->Verts(i)` /
`mesh->Faces(i)` call homes `geomOwner` once, and every `Face::Set(int,int,int)` homes its
arguments. **The number of homes counts the accessor calls in retail's source.** That makes it a
usable instrument for "retail called the accessor here; we hoisted it". It produced +7.3 pp and
+6.8 pp on the two rows with no other change in kind. The 16-bit `sth` index homes (retail-only) were
not reproduced by any spelling I tried.

## 5. Whole-binary price

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16te --patch <e0cca0a7f..f820a9c89> --label w16te`,
with the worktree detached at base `e0cca0a7f` so leg A is main and leg B is both lane commits. Run
dir: `~/tmp/wt-w16te/.ab_measure_runs/20261007-082612-w16te-3723679`.

**Prediction, recorded before the run:** Δmatched_functions 0 and Δmatched_code 0 B, because no row
reached fuzzy 100. Fuzzy should rise slightly. Any nonzero byte delta would mean some other row
moved.

```
leg A: matched=54896 masked=25210 honest=29686 code%=59.240520  (recompiles: 0, settled)
leg B: matched=54896 masked=25210 honest=29686 code%=59.240520  (recompiles: 12, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.005210pp   (legA 64.289185 -> legB 64.294395)
units at 100% [mpn ruler]: legA 620 -> legB 620  (Δ+0)
units at 100% [all-rows-fuzzy ruler]: legA 543 -> legB 543  (Δ+0)
```

**The prediction held.** Diffing the two archived reports by row shows exactly **6 changed rows**:
the five edited functions plus the AO unwind funclet `fn_82492CEC` (99.9 → 99.8, 40 B, below 100 on
both legs). Nothing else in the binary moved.

The lane is worth 0 B on the metric. What it buys is two behaviour fixes (§3.1 prints, §3.3
`startIdx`/`endIdx`), one latent dereference defect (§3.1), and five rows whose remaining distance is
now named: HOME, SCHED, or the PCH block.

## 6. Native gate

`tools/native_build_gate.sh` in `~/tmp/wt-w16te` at `f820a9c89`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 7. Not done

- The HOME residue in Tessellate (23 homes) and SmoothResults (6 `sth` homes plus the `0x54`/`0x50`
  slot) and `RndMesh::Load`. No spelling found.
- `SynthEmitter` ctor: needs a per-object PCH opt-out or a PCH-level `ObjPtr` switch. Outside this lane.
- SCHED and LAYOUT rows: no codegen fight, by design.
- Not merged and not pushed.
