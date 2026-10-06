# W16-PU — VIA-DC3 ring, rndobj + char: where retail RB3 behaves differently from our DC3-derived source (2026-10-06)

Branch `w16-pu`, worktree `~/tmp/wt-w16pu`, rebased onto main `60c34cf1a` (clean retail TU5 image,
W16-PY's `.bss` fixes, W16-PZ and W16-PW in the base). Ruler `name_check`, read from `report.json`.

## What this lane set out to do

Work the VIA-DC3 ring of `CAMPAIGN_STATE_2026-10-06.md` (files that exist in `dc3-decomp/src/system`),
starting with `src/system/rndobj` and `src/system/char`: find where RB3's retail bytes *behave* differently from
our source, fix our source to retail, largest gap first, and mirror any such fix into `milo-native-engine`
behind a DC3 gate.

## Result in one paragraph

Five functions behaved differently from retail (§2). Four fixes are on this branch: two reach 100
(`Rnd::DrawPreClear`, `CharacterTest::Handle`), and two stay below 100 with the behaviour fixed and a codegen residue
left (`RndText::WrapText`, `RndSoftParticleBuffer::BlurSurface`). The fifth, `Rnd::UpdateRate`, was found and fixed here
too, but W16-PW landed the identical fix first, so the rebase dropped my commit as already upstream. Every other row opened in
the rndobj+char gap turned out to be codegen: scheduling, FMA contraction form, register naming, struct-copy form,
block layout, or an ICF-folded callee name (§3). A whole-ring scan of string literals against retail (§4) found
the only string defect was `UpdateRate`'s, now fixed. No engine change was needed: the engine carries no copy of
any of the five functions (§5).

## 1. Method and instruments

All instruments live in `~/tmp/w16pu/` (scratch, not committed):

| instrument | what it does |
|---|---|
| `rows.py` | VIA-DC3 gap rows (`scope_ledger2.tier()`), scaffold units excluded → `via_rows.json` (753) / `rc_rows.json` (rndobj+char, 438 rows / 168,920 B) |
| `opsig2.py` | operation-multiset diff with registers, stack, branch targets, moves and relocation names masked; float constants compared **by value** (retail `lbl_` resolved through `pe.py` against `orig/45410914/band.exe`) |
| `seqdiff.py` | call / constant / immediate sequence diff for one row (constants decoded by value) |
| `sbs.py` | side-by-side target/base listing for one row |
| `fnbody.py`, `showdiff.py` | our body vs dc3-decomp's *current* body for one function |
| `strscan.py` | **new**: every `??_C@` string literal reference in every function of every VIA-DC3 unit (100% rows included), decoded and compared to retail's bytes at the address the retail instruction pair actually forms (§4) |
| Ghidra (`tools/ghidra/ghidra-decompile.py`) | retail decompile, used as the behaviour oracle for `DrawPreClear` and `WrapText` |

What actually found behaviour, in order of yield:
1. **DC3's current tree being newer than our snapshot** (`DrawPreClear`, `UpdateRate` — both already correct in
   `dc3-decomp`).
2. **Size mismatches** (retail body larger than ours) read against the Ghidra decompile (`WrapText`).
3. **Reading retail constants/tables directly** (`BlurSurface`'s tap table and PS constant registers).
4. **Retail dispatches less** (`CharacterTest::Handle`).

What did *not* find behaviour: op-multiset differences. 201 of the 438 rows (81,884 B) are op-equal under
`opsig2` (codegen-only by construction). Every op-different row I opened, apart from the five in §2, resolved to
layout, scheduling or FMA form (§3).

## 2. Fixes (behaviour)

**Measured on the landing base** — `tools/ab_measure.py --patch` of `git diff main w16-pu -- src` (main `60c34cf1a`,
four files), run dir `~/tmp/w16pu/abruns/20261006-111042-w16pu-src-on-60c34cf1a-1503307`, both legs settled,
`name_check`. Predicted beforehand: +2 functions / +752 B (DrawPreClear 640 + CharacterTest 112); `WrapText` and
`BlurSurface` improve but stay below 100 and move no bytes; no other row moves.

```
leg A: matched=53529 masked=25193 honest=28336 code%=57.972137  (recompiles: 0, settled)
leg B: matched=53531 masked=25193 honest=28338 code%=57.979477  (recompiles: 11, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.007340pp  Δcode_bytes=+752
```

The prediction held exactly. **No row goes down**: comparing the two archived leg reports row by row
(68,909 rows on each side, none added or dropped), 0 rows fall on `fuzzy` or `mpn`, and exactly the four code rows
below rise (`DrawPreClear`, `CharacterTest::Handle`, `BlurSurface`, `WrapText`).

Earlier record, on the previous base `68ea81224` with my `UpdateRate` commit still present (run
`20261006-110318-w16pu-all-1423154`): predicted +3 / +1,176 B, measured **Δmatched +3, Δcode_bytes +1,176**, 0 rows
down, five rows up. The difference between the two runs is exactly `UpdateRate`'s 424 B, which W16-PW now owns.
Deltas only; the two runs' absolutes are on different bases and are not compared.

| row (retail VA) | size | fuzzy before → after | what retail does that ours did not |
|---|---:|---|---|
| `Rnd::DrawPreClear` (0x824158B8) | 640 | 80.78 → **100** | The texture-compress hand-off: when `sCompressDone`, finish into `sCompressData`, swap the compressed `DxTex` in for the queued `RndTex` (`ReplaceObject`), pop and delete the request and the old texture. Then, when no job is in flight, drop requests whose `tex` **or `callback`** is null (ours pruned on `alpha > 0`) and start the next one on the thread (`StartCompress`, `SetEvent(gRndTextureEvent)`). Ours was a garbled port that read and wrote `gRndTextureEvent` as data. Also: X360 draws `mReleaseImmediate ? mPreClearDraws : mDraws`, sets the flag at 0x108 (not `mWorldCamCopied` at 0x107) around the dispatch, and null-checks each entry. DC3's current tree already had this; our copy predates it. The HX_NATIVE draw-list default and its X21 env gate are unchanged. The `.bss` residue I had left (99.9625) was closed by W16-PY's zero-initialised statics, already in the base. |
| `Rnd::UpdateRate` (0x82410B88) — **landed by W16-PW**; my identical commit was dropped on rebase | 424 | 85.42 → **100** (first run) | The rate-overlay gate labels are `" gs "` (.rdata 0x8205EA04) and `" cpu"` (0x8205E9FC), and the reset is `"    "` (0x8205E9EC). Ours printed `"gs"`/`"cpu"` and reset to `""`. DC3 has the retail strings; the constructor already used `"    "`. |
| `CharacterTest::Handle` (0x823DD690) | 112 | 0.00 → **100** | Retail's handler dispatches nothing. The five handlers (`add_defaults`, `test_walk`, `recenter`, `get_filtered_clips`, `sync`) are now `#ifdef HX_NATIVE`, so native keeps them. |
| `RndSoftParticleBuffer::BlurSurface` (0x824A8490) | 660 | 66.11 → 85.96 | (a) Retail's tap table is (weight, offset) = (0.1,−1.5) (0.25,−0.5) (0.3,0.5) (0.25,1.5) (0.1,2.5), with weights summing to 1 (.data 0x82C70EC8 holds the leading 0.1). Ours had lost the leading 0.1, so each tap read its offset as a weight. (b) Source and target are swapped: retail samples `mSurfaces[pass&1]` and draws into `mSurfaces[(pass-1)&1]`, so the vertical pass lands in `mSurfaces[0]`, which `DoPost` binds. (c) PS constants are 0x1F+i (UV scale) and 0x2F+i (weight); 0x8A/0x9A are DC3's. Residue: retail keeps tap 0's weight in `.data` and stores nine floats where we store ten, plus FPR/GPR allocation (DC3 records the same residue for its copy). |
| `RndText::WrapText` (0x82457F70) | 3016 | 79.41 → 83.40 | (a) An unwrapped text (`mWrapWidth == 0`) returned before the alignment pass, so centred, right-aligned and vertically aligned text without a wrap width was never offset; retail stores the width and falls into the shared alignment tail (`li r28, 0x78` = sizeof(Line), then `b` to it). (b) `charCount` did not advance over markup tags, but `ComputeCharWidths` gives every markup byte a zero-width slot, so every width after a tag was read from the wrong slot (retail `r22 += parsed - cur`). (c) The break test read `text[byteIdx-1]`; retail reads the previously processed character's byte index, so a space before a tag still allows a break after it. (d) An overflowing candidate that is not the newest wrap point costs 2010 (`li r7, 0x7da`), not 10, in both break loops. (e) The wrap-point buffer is sized by `strlen`, not by the UTF-8 char count. Our body went 2928 → 2972 B against retail's 3016; the rest is loop rotation (retail tests `'<'` at the head of a rotated loop) and register allocation. `ComputeCharWidths` (360 B, 83.39) was read against retail at the same time and does the same steps (zero-fill over markup, negative width clamped to 0, per-font display count), so it is codegen. |


## 3. Rows opened and why each stops

Every row below was read against retail (side-by-side listing, sequence diff, or the Ghidra decompile). "Codegen" means the
same values reach the same places; only scheduling, register choice, FMA form or layout differs.

| row | size | fuzzy | verdict |
|---|---:|---:|---|
| `RndAmbientOcclusion::Tessellate` | 4796 | 68.13 | **Behaviour lead, blocked.** Retail's call list has no `MakeString`/`TextStream<<`: the four progress prints are `MILO_LOG`. I re-measured the earlier lane's trade on today's tree: converting them gives 68.13 → **70.37**, but funclet `fn_82492CEC` goes 99.9 → 99.8, because our frame becomes 0x4c0 against retail's 0x4e0 and the funclet addresses a `DataNode` by frame offset (`addi r3, r31, 0x1a8` retail vs `0x178` ours). That is one row down, so it is reverted under the no-row-down rule. The earlier note's two funclets at 99.3/99.4 no longer reproduce; now it is one funclet at −0.1. The remaining ~390 B retail has over us after conversion is `Face` copy form (+37 `lhz`, +24 `sth`, +35 `stw`), i.e. field-by-field struct copies, not extra work. Unblocks when the 0x20 frame gap is understood. |
| `RndAmbientOcclusion::SmoothResults` | 1672 | 68.02 | Codegen: retail keeps `fmuls`+`fadds` where we fuse into `fmadds`. The `vector<UIScreen*>` vs `vector<int>` base-ctor name is an ICF fold. |
| `NgLight::SphereConeTest` | 1236 | 60.35 | Codegen: scheduling; we save FPRs, retail does not. |
| `RndParticleSys::MoveParticles` | 1432 | 70.46 | Codegen: FMA form (retail `fmuls`+`fsubs` vs our `fnmsubs`; the colour/size update as temp-then-add), retail hoists the `0x27c` load and keeps the plane test in a bool temporary, and the normal is paired (f25,f26,f27) vs (f25,f27,f26) consistently. Same values. |
| `RndLine::UpdateLine` / `UpdateLinePair` | 1328 / 1076 | 70.66 / 68.52 | Codegen: same 0x48 `Point` stride, copies scheduled differently. Ours calls an out-of-line `vector::back` helper that retail inlines; same value. |
| `CharSleeve::Poll` | 1980 | 99.80 | Codegen: only operand order in commutative `fadds`/`fmuls`/`fmadds`. The "1-ulp constant" lead no longer shows as a difference. |
| `BoxMapLighting::ApplyQueuedLights` | 520 | 73.77 | Codegen: retail recomputes the `fneg`s per `Max(0,±v)` and spills FPRs; we CSE them. The `fsel` selections are identical. |
| `CharLipSyncDriver::Sync` | 224 | 96.32 | Codegen: `0x48(r30)` ≡ `0x8(r29)` with `r29 = r30+0x40`. |
| `CharBonesSamples::EvaluateChannel` | 944 | 85.77 | Codegen: halfword reads reordered. |
| `CharEyes::Poll` | — | — | Codegen: retail re-tests the pointer for a second ternary; identical values. |
| `NgMat::RefreshState`, `NgMat::SetRegularShaderConst` | — | — | Codegen: switch-tail block layout; the case→store and field→slot maps are identical. |
| `RndShader::Init` | 268 | 59.78 | Codegen: the slot→shader map is identical; scheduling. |
| `CacheResource` | 304 | 38.42 | Codegen: block placement plus an ICF-folded `MakeString` name. A branch-order swap was byte-identical (inert). |
| `CharLipSync::PlayBack::Poll`, `Character::CalcBoundingSphere` | — | — | Ours already does what retail does; this is a place where RB3 differs from DC3, and we have RB3's version. |
| `RndTexBlendController::GetBlendState` | — | — | Already settled by an earlier lane (`FLOAT_AND_ARGORDER_2026-09-13`): same expression, different FMA form. |
| `XfmSort` "0.0" constant | — | — | False positive: 0x82CC… is the `.data` variable `gUtlXfms`, not a float literal. |
| `CharHair::Point` / `Strand` ctors | 216 / 228 | 55.96 / 54.67 | Retail inlines the two-argument `ObjPtr` ctor. Measured with the per-TU `/DRB3_OBJPTR_INLINE_TWOARG_CTOR`: the ctors gain about +444 B, but `resize`/`PropSync` rows in the same TU lose about −870 B. Stopped and reverted. |
| immediate-diff rows (`RndFont::Load`, `CharIKHead::Poll`, `CharEyes::NextLook`, `CalculateAO`, `CalculateAOAtPoint`, `FillCompressedVertex`, `UtilDrawPlane`, `BurnXfm`, …) | — | — | Codegen: reordered loads and stores with the same field→slot mapping, or frame-offset shifts. |

Not opened in this pass: the rest of the size-mismatch list (`RndMesh::OnSync`/`Load`, `IKElbow`, `Relativize`,
`SortPolls`, `TestPoint`, `PatchVerts::HasVert`, `Font::SetCharInfo`, `NgDOFProc::Set`, `MakeMRU`, `DecodeDxt5Alpha`,
`ObjVector<EyeDesc>::resize`), the member-offset candidates `RndCam::GetViewProjectXfms`, `RndMorph::InterpWeight`,
`NgLight::BlurShadowRT`, `ApplyDynamicConstraint`, `SpliceKeys`, `Synth::UpdateOverlay`, `RndMatAnim::Load`,
`CharCollide::Deform`/`Highlight`, the 0%-scored anonymous `fn_` rows (identification, not behaviour), and the
world/synth/ui parts of the ring.


## 4. String literals across the whole VIA-DC3 ring

Under `name_check`, a retail `lbl_` target is a forgiven placeholder. So a row can score **100 with the wrong string**
(`UpdateRate` is that class). `strscan.py` checks every function of every VIA-DC3 unit (278 units), 100% rows
included, by batch-diffing each unit (`objdiff-cli diff --batch`, about 1 s a unit). For each instruction whose base
operand is a `??_C@` literal, it decodes the literal from its mangled name and compares it with the retail C string at
the address the retail `lis`/D-form pair actually forms.

**Instrument trap, caught by its own output.** The first version took the address from objdiff's label text
(`lbl_82044690@l`). objdiff does **not print relocation addends**, so five distinct strings in
`CharClip::BeatAlignString` all showed as `lbl_82044690` ("RealTime"), and the scan reported five false mismatches on
a row at 100. The fixed scan takes the function VA from the symbol map (or the `fn_` name), adds the instruction's
offset, and decodes retail's own instruction words.

| run | refs checked | mismatches |
|---|---:|---:|
| control: every retail address shifted +1 byte | 3,640 | **3,593** (the 47 survivors are strings whose shifted read happens to compare equal) |
| real | 3,640 | **0** |

The control shows the comparison can fail. The zero therefore means that, after the `UpdateRate` fix, every
`lis`/`addi`-addressed literal in the ring is retail's string. Float literals by value are lane W16-PW's audit and are
not repeated here.


## 5. Engine (milo-native-engine)

No engine branch. `milo-native-engine` (`e1b0c29`) carries no copy of `DrawPreClear`, `UpdateRate`, `WrapText`,
`BlurSurface` or `CharacterTest`. It calls our `Rnd::DrawPreClear()` (`Rnd_Wgpu.cpp:985`, `Rnd_Wgpu_RB3.cpp:2055`), so
the fixes reach native through our own compiled TUs:
- `WrapText` and `UpdateRate` change native behaviour to retail's (unwrapped aligned text is now offset; the
  rate overlay labels).
- `BlurSurface`'s table and src/dst fixes are behavioural on native. Its register indices are inert there, because
  the engine's `WgpuShaderMgr::SetPConstant` and `Rnd_Stub` overrides are empty `{}`.
- `CharacterTest`'s handlers stay on native (`#ifdef HX_NATIVE`).
- `DrawPreClear`'s compress block is `#ifndef HX_NATIVE`. The native draw-list default and its X21 env gate are kept.

So there is nothing to gate for DC3 and no `MILO_ENGINE_PIN` to touch.


## 6. Deliberately not done

- No engine edits, no pin bump (nothing to mirror, §5).
- `Tessellate`'s `MILO_LOG` conversion is **not** landed, although it is correct. It costs one funclet 0.1, and the
  brief forbids any row going down (§3).
- No permuter, and no attempt at the register, FMA-form or loop-rotation residues in `WrapText`/`BlurSurface`. Those
  are codegen, not behaviour.
- The `CharHair` inline-ctor define, the `.bss`-order and `= 0` experiments on the `Rnd` statics, and a
  `CacheResource` branch swap were all tried and reverted. W16-PY landed the `.bss` fix independently.
- Float constants by value across the ring are left to W16-PW.


## 7. Native checks

Run in the lane worktree on the five fixes rebased onto `68ea81224`; re-run on the landing base `60c34cf1a` as the
last actions. Each check was run once before this doc was committed,
and both were run again as the lane's last actions (results in the lane report). The summary lines, verbatim:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=73 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

dc3-decomp's native build was not run: no engine branch exists (§5), so nothing it builds changed.
