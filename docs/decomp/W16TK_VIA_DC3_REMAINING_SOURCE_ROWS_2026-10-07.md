# W16-TK: the other 37 VIA-DC3 source-class rows, read against retail

Lane W16-TK, 2026-10-07, worktree `~/tmp/wt-w16tk`, branch `w16-tk`, base main `c84b6bf3d`.
Brief: `CAMPAIGN_STATE_2026-10-07b.md` §6 lever 1, the part W16-TE and W16-TI did not cover. Read
each row against retail, fix the source where it differs in behaviour or shape, give every row a
verdict (EQUAL or FIXED, with the evidence), price it with `tools/ab_measure.py`.

Ruler: the shipped graded `name_check`. Per-row numbers are `fuzzy_match_percent` from the
worktree's `report.json` after a full `tools/ninja-locked`, before vs after, same worktree. The
whole-binary price is in §5.

## 1. The population

Lever 1 is the 67-row bucket "31 in W16-PU/QF population, source-class charge" of
`~/tmp/w16ta-gap/dispo_via_rows.json`. W16-TE took the ten rows where retail's body is ≥16 B
larger and W16-TI the twenty W16-QA `DIFF` rows. The remaining **37 rows / 35,280 B** are this
lane's. Re-derived, not transcribed: the 37 names (`~/tmp/w16tk_rows.json`) all carry
`dispo == '31 …'`, and 67 − 10 − 20 = 37.

## 2. Method

As W16-TE and W16-TI: objdiff full listing per row (`objdiff-cli diff --full-listing`, graded
ruler, no `--build`), then

1. **Calls**: multiset of `bl`/`b` targets per side. An uncharged callee difference is an
   installed fold and is not looked at further; a placeholder (`fn_`/`lbl_`) callee is forgiven
   by the ruler and was checked by hand.
2. **Loads/stores on one side only**, and **immediates / constants**. Retail's `lfs`/`lfd`
   constants were read out of `band.exe` (8 bytes for `lfd`; a first version of the reader took
   4 bytes for every load and produced a false `PrepShadow` mismatch, π/8 as a double).
3. **New this lane: a value-flow check.** A small symbolic evaluator
   (`~/tmp/w16tk_diffs/symeval.py`, `valdiff.py`) walks each side's straight-line code, builds an
   expression for every register (`fadds`/`fmuls`/`fmadds` normalised as commutative sums and
   products, loads keyed on base+offset, calls numbered, `__savegpr`/`__restgpr` helpers ignored)
   and compares the multiset of **stored values**. Equal multisets mean every store writes the
   same expression on both sides, so a permuted load order is only scheduling.
   - **Control that it can fail:** swapping one operand of one `fmadds` in `BurnXfm`'s base
     column reads `store-diff 1`; the unmodified row reads `0`.
   - **Limits, measured:** it is linear, so a branchy function evaluates both arms in sequence
     (it read the buggy `DecodeDxt5Alpha` as equal, and produced a false `RenderConeDefs` diff
     from a `fsel` + else arm). It cannot see `fmadds` vs `fmuls`+`fadds` contraction, which it
     normalises away. Stack-slot addresses were blanked when comparing frames of different size.
     It was used to clear straight-line rows and to point at the rows to read by hand, never as a
     verdict on its own.
4. Then the residue was read by hand. A charge that named no source construct is recorded as
   SCHED / HOME / LAYOUT and not fought.

Edits were batched (every edit is in a different function), with one full build per batch.

## 3. Result

| row | size | fuzzy before | after | verdict |
|---|---:|---:|---:|---|
| `RndTexRenderer::DrawToTexture` | 3,320 | 96.66 | 97.51 | **FIXED** (shape: zero-term factoring), rest SCHED |
| `RndParticleSys::InitParticle` | 2,492 | 98.21 | 98.21 | EQUAL (SCHED) |
| `ResetNormals` | 1,980 | 99.88 | 99.88 | EQUAL (SCHED; callee diffs are uncharged twin folds) |
| `NgMat::SetRegularShaderConst` | 1,896 | 99.51 | 99.51 | EQUAL (SCHED) |
| `CharEyes::NextLook` | 1,684 | 99.67 | 99.67 | EQUAL (SCHED) |
| `NgMat::RefreshState` | 1,456 | 96.59 | 98.57 | **FIXED** (shape: one `Vector4` member), rest SCHED |
| `Character::PostLoad` | 1,440 | 99.65 | 99.65 | EQUAL (ANCHOR + one HOME store) |
| `RndParticleSys::MoveParticles` | 1,432 | 98.01 | 98.01 | EQUAL (SCHED; retail reloads a just-stored value) |
| `Character::CalcBoundingSphere` | 1,340 | 99.90 | 99.90 | EQUAL (SCHED) |
| `Spotlight::UpdateTransforms` | 1,340 | 99.69 | 99.69 | EQUAL (frame layout; values equal) |
| `NgSpotlightDrawer::RenderConeDefs` | 1,324 | 95.27 | 95.27 | EQUAL (SCHED / frame slots) |
| `NgLight::SphereConeTest` | 1,236 | 60.35 | 60.35 | EQUAL (frame layout / regalloc; values equal) |
| `UIComponent::Update` | 1,192 | 98.56 | 98.56 | EQUAL (SCHED; retail recomputes the vbase address) |
| `RndFlare::DrawShowing` | 1,124 | 98.53 | 97.27 | **FIXED** (shape: one bool expression); score down, see §4 |
| `SpliceKeys` | 1,096 | 98.87 | 98.87 | EQUAL (SCHED) |
| `MovieInternalBuffers::New` | 992 | 99.55 | 99.65 | **FIXED** (shape: no reference local), rest SCHED |
| `Spotlight::BuildNGQuad` | 964 | 86.61 | 86.61 | EQUAL (recorded floor: `-1.0f` strength reduction) |
| `RndShadowMap::PrepShadow` | 940 | 91.12 | 99.34 | **FIXED** (shape: zero-term factoring), rest SCHED |
| `UtilDrawPlane` | 756 | 99.90 | 99.90 | EQUAL (SCHED) |
| `BurnXfm` | 680 | 98.67 | 98.67 | EQUAL (SCHED) |
| `RndSoftParticleBuffer::BlurSurface` | 660 | 85.96 | 85.96 | EQUAL (SCHED; tap table identical) |
| `RndMatAnim::Load` | 624 | 96.62 | 98.11 | **FIXED** (shape: `sOwner` file static), rest ANCHOR |
| `NgSpotlightDrawer::RenderScene` | 588 | 99.59 | 99.59 | EQUAL (one `srawi.`/`clrrwi.`, same test) |
| `CharLipSync::PlayBack::Poll` | 564 | 99.22 | 99.93 | **FIXED** (shape: `for` loop), `mpn` 100 |
| `FillCompressedVertex` | 556 | 99.85 | 99.85 | EQUAL (SCHED) |
| `DecodeDxt5Alpha` | 504 | 75.60 | 98.41 | **FIXED** (behaviour ×2) |
| `NgLight::BlurShadowRT` | 488 | 98.41 | 98.41 | EQUAL (SCHED; strength-reduced loop index) |
| `EstimateDraw` | 460 | 99.95 | **100** | **FIXED** (behaviour: six weights) |
| `RndCam::GetViewProjectXfms` | 420 | 89.74 | 89.74 | EQUAL (SCHED: reload vs CSE; values equal) |
| `RndTexBlendController::GetBlendState` | 408 | 95.34 | 99.36 | **FIXED** (shape: float/bool pair), rest SCHED |
| `CacheResource` | 304 | 38.42 | 38.42 | EQUAL (LAYOUT; three spellings inert) |
| `RndShader::Init` | 268 | 59.78 | 59.78 | EQUAL (SCHED; all 26 slots identical) |
| `RndMorph::InterpWeight` | 216 | 92.09 | 92.09 | EQUAL (SCHED; DC3's recorded negative reproduced) |
| `Spotlight::SetColor` | 176 | 91.25 | 91.25 | EQUAL (SCHED; TI §4.1 pattern) |
| `MetaMusicLoader::MetaMusicLoader` | 132 | 96.06 | **100** | **FIXED** (shape: implicit `FilePath`) |
| `XfmSort` | 116 | 96.93 | 96.93 | EQUAL (SCHED) |
| `SpotlightDrawer::Init` | 112 | 94.11 | 94.11 | EQUAL (SCHED) |

**Eleven FIXED, twenty-six EQUAL.** Two rows reach fuzzy 100 (`EstimateDraw`, the
`MetaMusicLoader` ctor) and `PlayBack::Poll` reaches `mpn` 100. One row went down (`DrawShowing`,
kept on purpose, §4.14).

**Two behaviour fixes, both in native-globbed `rndobj/` files:**

- `DecodeDxt5Alpha` read the wrong byte for the high bits of a 3-bit alpha index that straddles a
  byte, and returned 0 / 0xFF for codes 6 and 7 in the 8-value mode, where DXT5 interpolates
  (§4.26).
- `EstimateDraw` put six of its twelve per-counter weights on the wrong `NgStats` counter, so the
  "est draw" overlay figure was wrong (§4.28).

Classes as W16-TI: **FIXED** (a retail-attested change applied), **SCHED** (register / FP order /
scheduling; no call, store or constant difference names a source construct), **HOME** (an
inlined argument homed to a frame slot on one side only), **LAYOUT** (branch targets /
tail-merge / block order only), **ANCHOR** (the base symbol MSVC picks for two co-addressed
statics), **NAME** (the only charge is a relocation name).

## 4. Per row

Value-flow column: `stores equal` means the evaluator in §2 found the same multiset of stored
expressions on both sides (stack slots blanked where the frames differ).

### 4.1 `RndTexRenderer::DrawToTexture` (3,320 B): FIXED (shape), 96.66 → 97.51

- The impostor pull-back `Multiply(Vector3(0, -f34, 0), tfc8.m, tfc8.v)`: retail emits
  `fmuls f10, f10, f31` / `f9` / `f8` on m.z.c (0xd8/0xd4/0xd0(r31), f31 = 0.0f), folds the
  `-f34` term in with `fmadds`, then `fmadds f13, f3, f31, f13` for m.x.c. Ours opened with
  `fadds` of two matrix elements: /fp:fast factored `m.x.c*0 + m.z.c*0` into `(m.x.c+m.z.c)*0`.
- Fix: the accumulator form DC3 carries for this site (`outY = m.z.y*0; outY += m.y.y*negDist;
  outY += m.x.y*0;` etc.). After it the block differs only in two `fadds` operand orders, and the
  structural mismatch count (replace/insert/delete) went 25 → 17.
- Everything else: stores equal; register numbering and a 0x60 larger frame (0x3d0 vs 0x370).

### 4.2 `RndParticleSys::InitParticle` (2,492 B): EQUAL (SCHED), 98.21

Same calls, same constants, stores equal. Residue: one extra `lfs 0x8` on our side and operand
order. DC3 records (w7-e, in its `Mtx.h`) that the accumulator rewrite of this function's aliasing
`Multiply(particle->Vel3(), xfm->m, particle->Vel3())` drops the whole function (99.29 → 98.5) by
re-colouring ~600 instructions earlier. Not repeated.

### 4.3 `ResetNormals` (1,980 B): EQUAL (SCHED), 99.88

Stores equal. The callee differences are template twins with no charge (installed folds).

### 4.4 `NgMat::SetRegularShaderConst` (1,896 B): EQUAL (SCHED), 99.51

Stores equal; same constant pairing per store; load order only.

### 4.5 `CharEyes::NextLook` (1,684 B): EQUAL (SCHED), 99.67

Stores equal; uncharged twin callees; reordered sum terms.

### 4.6 `NgMat::RefreshState` (1,456 B): FIXED (shape), 96.59 → 98.57

- The second `mBlend` switch (fade state at 0x234..0x244). Decoded per case from both listings:

  | case | retail | ours (before) |
  |---|---|---|
  | kBlendSrcAlpha, kPreMultAlpha | four `stfs f31` (0.0) then `stw r28` (1) | `stw r28`, then a `b` into the shared four-zero-store tail |
  | kBlendMultiply | four `stfs f30` (1.0), then `b` to the shared `stw r26` (2) | inline `stw r26` between the float stores |
  | kBlendAdd, SrcAlphaAdd, Subtract | four `stfs f31`, `stw r26` | `stw r26`, four `stfs f31` |

  Same values in every case, so not a behaviour bug. As four scalar floats MSVC hoists the
  0x234 store and tail-merges the zero-fill arms. Retail keeps the floats first and merges
  `mFadeOut = 2`, which is the shape a single `Vector4` written through `Set()` gives.
- Fix: `int mFadeOut; Vector4 mFadeParams;` in `Mat_NG.h`, `mFadeParams.Set(...)` in the arms.
  `mFadeOut` is the name every `CalcShaderOpts` already writes it into
  (`opts.mFadeOut = mat->mFadeOut`). The five `Shader.cpp` readers build their `Vector4` from the
  four components exactly as before, and none of their rows moved (§5).
- Rest: two `li` schedule swaps, one `lwz 0x44` placed before vs after, stack slots.

### 4.7 `Character::PostLoad` (1,440 B): EQUAL (ANCHOR + HOME), 99.65

Retail co-addresses `gRevs` / `gCharMe` off `gRevs` (`sth 0x4(r21)`, `stw 0x8(r21)`); ours off
`gCharMe` (`-0x4`, `0x0`). Same layout, same values. One retail-only home store (`stw r11,
0x54(r31)`). The same anchor choice appears in `RndMatAnim::Load` (§4.22), where three spellings
were measured and none moved it.

### 4.8 `RndParticleSys::MoveParticles` (1,432 B): EQUAL (SCHED), 98.01

Stores equal. Retail is 12 B larger. After storing a vector's three components it reloads one of
them (`lfs f12, 0x8(r11)`) where we keep the register. That is an alias decision, not a
construct. The other retail-only lines (`lfs f12, 0x0(r29)`, `fmr f0, f12`) are the same.

### 4.9 `Character::CalcBoundingSphere` (1,340 B): EQUAL (SCHED), 99.90

Stores equal; reordered terms.

### 4.10 `Spotlight::UpdateTransforms` (1,340 B): EQUAL (frame layout), 99.69

Stores equal once stack slots are blanked. Frame 0x1b0 (retail) vs 0x1e0. The literal-zero
`Multiply` in this function already carries the written-out fix.

### 4.11 `NgSpotlightDrawer::RenderConeDefs` (1,324 B): EQUAL (SCHED / frame slots), 95.27

The evaluator's 3-store difference was a false alarm from its linear walk over the `fsel` +
`else` arm. By hand: retail `fsel f18, f12, f0, f13` with `f12 = top - bot` is
`min(top, bot)`, and ours `fsel f21, f13, f17, f0` is the same. The offset is
`min * mLength / (bot - min)` on both sides, with the same `0 < bot` guard and zero else arm.
`NGRadii` lands at 0x80 vs 0x70.

### 4.12 `NgLight::SphereConeTest` (1,236 B): EQUAL (frame layout / regalloc), 60.35

Stores equal, all 66 of them, once the frame slots are blanked. The in-file record already
explains the residue: ours saves r24–r31 + f26–f31 in 0x140, retail r27–r31 + f30/f31 in 0x100,
and w7-as / w17-lit measured the reorderings. The 60% is that frame shift.

### 4.13 `UIComponent::Update` (1,192 B): EQUAL (SCHED), 98.56

Retail computes the virtual-base address twice; ours computes it once (CSE). No store or call
difference.

### 4.14 `RndFlare::DrawShowing` (1,124 B): FIXED (shape), 98.53 → 97.27 (down, kept)

- Retail: `li r26, 0` … `lbz 0x65` / `bne` → `li r11, 1` / `lbz 0x64` / `bne` / `mr r11, r26`,
  then `clrlwi r28, r11, 24`. It builds a bool in a scratch register and narrows it into the
  callee-saved home. Ours (`if (mOcclusionPending || (useOccResult = true, !mOcclusionReady))
  useOccResult = false;`) wrote `li r27, 1` straight into the home.
- Fix: `useOccResult = !mOcclusionPending && mOcclusionReady;`. That block now matches
  instruction for instruction, and the frame size now equals retail's (0x130; it was 0x120).
- **Failed prediction:** I expected the row to rise. It fell 1.26 pp. With retail's frame, every
  stack operand in the rest of the function sits 0x10 off retail's, and the int→float
  conversion pair (`lwa 0x48/0x4c`, `std`, `lfd`) now takes a second slot (0x50 and 0x60 where
  retail reuses 0x50). Same values.
- Kept: it is the construct retail shows, and the row is not at 100 on either side, so no bytes
  ride on it.

### 4.15 `SpliceKeys` (1,096 B): EQUAL (SCHED), 98.87

Alias analysis of the `anim1` home reload. A prior lane tuned the `VecRaw` form.

### 4.16 `MovieInternalBuffers::New` (992 B): FIXED (shape), 99.55 → 99.65

Retail addresses the first merged field and `BinkRegisterFrameBuffers`' argument through `r4`
(`addi r4, r30, 0x34`) and the other fields as `0x38(r30)`, …. That is `bufs->mBuffers.X`
written directly, not a `BINKFRAMEBUFFERS &b` reference local. Rest: four load-order swaps
(bufs field before `info` field).

### 4.17 `Spotlight::BuildNGQuad` (964 B): EQUAL (recorded floor), 86.61

The in-file SURVEY (w7-ae) and RESIDUAL (w7-bw) notes cover it. The vertex multiply is term for
term the same; retail multiplies by a hoisted `-1.0f` register where ours strength-reduces
`a + b * -1.0f` to `a - b`. The face loop's IV shape was measured five ways. Constants all match.

### 4.18 `RndShadowMap::PrepShadow` (940 B): FIXED (shape), 91.12 → 99.34

- The same zero-term factoring as §4.1, on `Multiply(Vector3(0.0f, -dist, 0.0f), lightXfm.m,
  offset)`. Retail: `fmuls f12, f12, f31` on m.z.x (0xa0(r1)), `fmadds` the `-dist` term, then
  `fmadds f0, f6, f31, f0` on m.x.x. x seeds from m.z.x, and y/z seed from m.x.c (0x84 / 0x88).
- Fix: the per-component accumulators DC3 carries for this site.
- **Failed attempt:** writing each product as `v.c * m.r.c` (to match retail's `fmadds f0, f30,
  f9` operand order) was inert. MSVC canonicalises the commutative multiply, as DC3's `Mtx.h`
  note records.
- Rest: the light pointer arrives in `r10` and is copied to `r27` later in retail; ours loads it
  into `r27` directly.

### 4.19 `UtilDrawPlane` (756 B), 4.20 `BurnXfm` (680 B): EQUAL (SCHED), 99.90 / 98.67

Stores equal. These are the straight-line rows where the evaluator's control was run (`BurnXfm`:
sabotaged operand → `store-diff 1`). `BurnXfm`'s written-out normal transforms are tuned
(98.2 → 98.7) per its source note.

### 4.21 `RndSoftParticleBuffer::BlurSurface` (660 B): EQUAL (SCHED), 85.96

`kBlurTaps` is identical: tap0.x = 0.1 static in `.data`, nine dynamic. Retail reloads 0.5 inside
the loop and computes `!(pass & 1)` with `nor` / `clrlwi`. No store, call or constant difference.

### 4.22 `RndMatAnim::Load` (624 B): FIXED (shape), 96.62 → 98.11

- Retail stores the TexPtr owner as `stw r11, 0x4(r27)`, with `r27` = `lbl_82CC5060` (the
  revision word `gRev_MatAnim`). MSVC co-addresses like that only for internal-linkage objects in
  one TU. Ours had `RndMatAnim::sOwner`, a class static with its own `lis`/`stw` pair, and nothing
  outside `MatAnim.cpp` uses it.
- Fix: `static Hmx::Object *sOwner = nullptr;` in `MatAnim.cpp`, declared right after
  `gRev_MatAnim`, and dropped from the class.
  - **The `= nullptr` is load-bearing.** Without it, this TU's `.bss` put `sOwner` among the
    uninitialised objects (0x28) and `gRev_MatAnim` among the zero-initialised ones (0x34), with
    `gRevs_MotionBlur` between them. Read off the obj's symbol table.
- **Measured and not kept:**
  - One `struct { int rev; Hmx::Object *owner; }`: Load 98.54, but the two `TexPtr` ctors went
    100 → 94.17 and `Copy` 100 → 97.91. Retail's ctors load `lwz r4, lbl_82CC5064@l(r11)` by
    the owner's own address, so it is not one aggregate.
  - Swapping the declaration order: the layout follows it, but the anchor stays on `sOwner`.
    Inert.
- Rest: ANCHOR. Ours bases the pair on `sOwner` (`gRev` at −4), and retail on `gRev`. This is the
  same choice as `Character::PostLoad` (§4.7).

### 4.23 `NgSpotlightDrawer::RenderScene` (588 B): EQUAL, 99.59

The only charged line is retail's `srawi. r11, r11, 3` against our `clrrwi. r11, r11, 3` on
`sLights.end() - sLights.begin()` (8-byte entries), followed by the same `beq`. That is the same
test. `sLights.size() != 0` was measured, and is inert. `sFogScale`'s 0.125 is a function static
on both sides.

### 4.24 `CharLipSync::PlayBack::Poll` (564 B): FIXED (shape), 99.22 → 99.93, `mpn` 100

Retail has a `clrrwi r11, r11, 0` after `mFrame`'s increment store, before the `cmpw` / `blt` back
edge. That is the shape of `for (; mFrame < frameIdx; mFrame++)`, which is DC3's spelling. We had
a `do … while` with `mFrame++` in the body. Our `>= mFrames` clamp is kept, since retail confirms
it. The callee `fn_8282C610` is the forgiven ceil placeholder. Rest: one `lbzx` operand order.

### 4.25 `FillCompressedVertex` (556 B): EQUAL (SCHED), 99.85

Stores equal once slots are blanked; same pairing.

### 4.26 `DecodeDxt5Alpha` (504 B): FIXED (behaviour ×2), 75.60 → 98.41

- **Index byte.** A 3-bit code that straddles a byte reads its high bits from the next byte,
  swizzled the same way as the first (the 360 stores the alpha index bytes swapped within each
  16-bit word). Retail: `next = byte + 1`, decremented if odd and incremented if even. Ours
  computed `byte + 2` / `byte + 3`, i.e. one byte too far in both cases. Every alpha pixel whose
  code straddles a byte boundary decoded wrongly.
- **8-value mode.** Retail's `bgt` (a0 > a1) lands directly on `((8-code)*a0 + (code-1)*a1 + 3)
  / 7`. Ours special-cased codes 6 and 7 as 0 and 0xFF there, which is the 6-value mode's rule.
- Fix: DC3's body, with comments citing RB3 retail instructions. Rest: one `clrlwi` placed early
  vs late.

### 4.27 `NgLight::BlurShadowRT` (488 B): EQUAL (SCHED), 98.41

Stores equal. Our loop strength-reduced `0x21 + i` and `0x31 + i` into running registers.

### 4.28 `EstimateDraw` (460 B): FIXED (behaviour), 99.95 → 100

Each `lwax` at a fixed `NgStats` offset feeds one `fmadds` constant. `UpdateOverlay`'s format
strings name those offsets: 0x4 parts, 0x8 part_sys, 0x14 bones, 0x24 lights (approx), 0x2c
flares, 0x30 motion blur. Six of the twelve weights were on the wrong counter:

| counter | retail weight | ours (before) |
|---|---|---|
| parts | 0.01 | 0.000233 |
| part_sys | 0.000233 | 0.005 |
| bones | 0.005 | 0.00126 |
| lights (approx) | 0.00126 | 0.01 |
| flares | 0.003 | 0.017 |
| motion blur | 0.017 | 0.003 |

The overlay's "est draw" number was wrong. Before the fix, the row's only charges were the
constant relocations naming the wrong `__real@` value at those six sites, which is why it read
99.95 and not lower.

### 4.29 `RndCam::GetViewProjectXfms` (420 B): EQUAL (SCHED), 89.74

Stores equal. Retail reads `mFarPlane` after the FOV compare and re-reads `mScreenRect.w` / `.h`
(0x2d8 / 0x2d4) for the scale terms, where ours keeps them from the first read. No call or
constant difference, and no source construct names the reload. A `const float &` spelling was
considered and not tried.

### 4.30 `RndTexBlendController::GetBlendState` (408 B): FIXED (shape), 95.34 → 99.36

Retail's false arm carries a dead `fmr f0, f31` (refDist = 0.0f) beside `li r11, 0`. So the
reference distance is copied into a float/bool pair by an if/else, and the bool is tested after,
which is DC3's spelling. Rest: the smoothstep's `fmadds` vs `fmsubs`, which DC3 records as
inert across four spellings.

### 4.31 `CacheResource` (304 B): EQUAL (LAYOUT), 38.42

Retail lays out the `gen/` block as the fall-through of the "png" test, with the movie block
after it. Ours puts the movie block first. Same calls, same constants (the two `MakeString`
names are installed folds), same `res` values. Three spellings compile byte-identically and were
reverted:

- `== || ==` with an else
- `!= && !=` with the movie arm first
- an early return

The 38% is the block order shifting every instruction.

### 4.32 `RndShader::Init` (268 B): EQUAL (SCHED), 59.78

All 26 shader-table slots were decoded through RTTI (vtables at 0x82C70D14..3C in `.data`), and the
table is identical. Store order and callee-saved registers differ.

### 4.33 `RndMorph::InterpWeight` (216 B): EQUAL (SCHED), 92.09

- Retail reads `prev->value` once, above the `mSpline` test, and both arms use f0.
- **Failed prediction:** hoisting `float prevVal = prev->value;` moved that load to the right
  place, but the row fell to 87.87. MSVC then also hoisted the `next` pointer load above the
  branch, where retail loads it in each arm.
- That is DC3's recorded negative exactly (w7-az: 94.41 → 90.74, "do not retry"). I found the
  record after measuring. Reverted.

### 4.34 `Spotlight::SetColor` (176 B): EQUAL (SCHED), 91.25

The `SetColorIntensity` self-copy pattern in W16-TI §4.1.

### 4.35 `MetaMusicLoader::MetaMusicLoader` (132 B): FIXED (shape), 96.06 → 100

Retail passes the `FilePath` temporary's frame address (`addi r4, r31, 0x50`) to `Loader::Loader`.
Ours passed the ctor's returned `r3` (`mr r4, r3`). That is `Loader("", kLoadFront)`, with the
temporary made by implicit conversion as in `FileMergerOrganizerLoader`, not
`Loader(FilePath(""), …)`.

### 4.36 `XfmSort` (116 B), 4.37 `SpotlightDrawer::Init` (112 B): EQUAL (SCHED), 96.93 / 94.11

`XfmSort`: same comparison; FP association differs; the odd constant is the `gUtlXfms` data
global. DC3 records w8-h / w9-f negatives. `SpotlightDrawer::Init`: recorded negatives in our tree
(INSDEL-2) and in DC3 (w8-h, w8-q, w18-b).

## 5. Whole-binary price

Two runs of `python3 tools/ab_measure.py`, ruler `name_check` (objdiff-cli sha256
`c1b7d95240a35cd6` on all four legs).

**Run 1: the first seven commits** (`255e4fa89`..`b90eb3d23`), measured as
`--patch ~/tmp/w16tk_lane.patch` (= `git diff c84b6bf3d b90eb3d23`) on a temporary revert commit,
which was dropped afterwards. Run dir `.ab_measure_runs/20261007-105207-w16tk-1205352/`.

Recorded before the run: **+2 fns / +592 B** (`EstimateDraw` 460 B and the `MetaMusicLoader`
ctor 132 B to 100 on both rulers), and no other row moving.

```
leg A: matched=54936 masked=25214 honest=29722 code%=59.306526  (recompiles: 0, settled)
leg B: matched=54939 masked=25214 honest=29725 code%=59.312300  (recompiles: 125, settle iterations: 2)
Δmatched=+3  Δmasked_equal=+0  Δhonest=+3  Δcode%=+0.005774pp  Δcode_bytes=+592
units at 100% [mpn]: 625 -> 626 (+MetaMusic, MATCHED_ROSE)
```

Bytes were exact, at +592. **Functions came out one over the prediction (+3, not +2).**
`PlayBack::Poll` reached `mpn` 100 (its fuzzy is 99.93, so it carries no bytes). Diffing the two
legs' `report.json` row by row gives exactly ten changed rows, the ten FIXED rows of run 1 with
the values in §3, and nothing else. `Flare::DrawShowing` is the only one that went down. None of
the five `Shader.cpp` `CalcShaderOpts` readers of `mFadeOut` / `mFadeParams` moved.

**Run 2: `DrawToTexture`** (`12f0f102f`), measured as `--revert HEAD`, so the signs are inverted
(leg B is without the fix). Run dir `.ab_measure_runs/20261007-105609-w16tk-dtt-1245467/`.

Recorded before the run: **Δ0 / Δ0**, exactly one row changing.

```
leg A: matched=54939 masked=25214 honest=29725 code%=59.312300  (recompiles: 0, settled)
leg B: matched=54939 masked=25214 honest=29725 code%=59.312300  (recompiles: 1, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
```

The row diff shows one row: `DrawToTexture` 97.51 (leg A) → 96.66 (leg B).

**Lane total: +3 fns / +592 B / +1 unit at 100 (`MetaMusic`), Δmasked_equal 0.** Of the
592 B, 460 B is a behaviour fix (`EstimateDraw`). `DecodeDxt5Alpha`'s behaviour fix carries no
bytes, because the row is at 98.41.

## 6. Native gate

`tools/native_build_gate.sh` in the worktree at `12f0f102f` (every source edit included), run
after the last build:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 7. Not done

- **ANCHOR** (`Character::PostLoad`, `RndMatAnim::Load`): MSVC bases our co-addressed static
  pairs on the later object and retail on the earlier one. Two spellings in MatAnim did not move
  it, and an aggregate is refuted by retail's `TexPtr` ctors. No spelling found.
- **`CacheResource`'s block order** (38% on 304 B): three spellings inert. Its source has an
  `#ifdef HX_NATIVE` twin with the PS3 path, which was not touched.
- **`Flare::DrawShowing`'s frame slots** after the bool fix: not chased.
- **`GetViewProjectXfms`**: the `mFarPlane` / `mScreenRect` reloads; the `const float &` idea
  was not tried.
- **SCHED rows**: no codegen fight, by design, and no permuter. The value-flow evaluator lives in
  `~/tmp/w16tk_diffs/` and is not committed. It is linear (branch-blind) and contraction-blind
  (§2), so it is a pointer for reading, not a verdict tool.
- Not merged and not pushed.
