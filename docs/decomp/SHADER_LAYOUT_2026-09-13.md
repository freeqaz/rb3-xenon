# ShaderMgr register audit, renderer struct adjudications, and a landed
# splits re-home — lane W7-C, 2026-09-13

Branch `w7-shader-layout`, worktree `~/tmp/wt-w7-c`, based on main `3a6bfe40`
(asserted ancestor). Ruler: **`name_check` (graded)**, read from `report.json`
`provenance.diff_config` — not assumed. Baseline at this lane's first full
build (full build first; reflinked target objs are pre-renamer):

```
matched_functions   42,676
matched_code        3,859,912 B
matched_code_percent  37.672540
fuzzy_match_percent   49.090370
total_code          10,245,956
total_functions      69,219
masked_equal         22,937
```

`verify_objs_patched.py --verify-manifest` → `OK: 1205 decomp, 3084 target
objects match` (rc=0) before any edit.

⚠ **W6-C's baseline (42,658 / 3,851,188 B) does NOT reproduce, and that is
correct, not a discrepancy**: main moved +18 fns / +8,724 B between W6-C's base
`50fd112d` and this lane's `3a6bfe40` — exactly the amount `3a6bfe40`'s own
ledger commit records for the close of wave 6. Re-measured rather than
inherited, per the standing rule that these figures move in both directions.

---

## 1. `ShaderMgr.h` register audit — **no further DC3 renumbering exists**

### 1.1 Why every register was a suspect

Our `src/system/rndobj/ShaderMgr.h` enums are **byte-identical to
`../dc3-decomp/src/system/rndobj/ShaderMgr.h`** except for the two values W6-C
already corrected (`kVS_RimColor`/`kPS_RimColor`, 0x3d → 0x3f) and one constant
we dropped (`kVShader_SplineMaxCtrlPoints`). So every remaining value is an
*inherited DC3 number with no independent RB3 evidence* — which is precisely
the condition under which the rim bug existed. The audit therefore had to cover
all of them, not spot-check.

**56 named constants are referenced outside the header.** Every one is
adjudicated below; the three classes sum to exactly 56 with no row dropped or
invented (self-validated in-script against the use census).

### 1.2 The instruments, and the two that turned out to be unsound

Three instruments were used, in increasing cost:

1. **The metric itself.** `li r4, 0x3f` vs `0x3d` is an *immediate* difference.
   `fuzzy` charges immediates, and `mpn` charges them too (it excludes only
   *non*-immediate arg diffs). ⇒ **a constant used anywhere inside a
   `fuzzy == 100` row is proven correct by the grader.** Cheap and rigorous.
2. **A whole-corpus census** of every `TheShaderMgr` upload site: a
   `lwz r3, lbl_82C76CE0` singleton load, an `li`/`addi` into `r4`, and a
   `lwz r11, <slot>(r11)` vtable dispatch, over all 69,172 functions the split
   emits (of 69,219 — i.e. effectively whole-binary).
3. **Direct reads of the retail body** of each calling function, compared
   positionally against our source.

⛔ **Instrument 2 is POSITIVE-ONLY, and I nearly published a false negative
from it.** The census reported `0x50` at **zero** ShaderMgr sites, which would
have condemned `kVS_BoxMapLight0`/`kPS_BoxMapLight0`. It is wrong: retail's
`NgEnviron::UpdateApproxLighting` @`0x82B863F8` hoists the constant into a loop
**induction variable** — `li r29, 0x50` before the loop, then `mr r4, r29` at
each call — which no `li r4`/`addi r4` pattern can see. The value is **0x50,
confirmed**. The census's positives all stand (it does see the `addi r4, r27,
0x83` form, so the addressing-mode coverage is real); **its absences carry no
information at all** and none is relied on anywhere below.

⚠ **Instrument 2 also produced a red herring that a control killed.** It found
`addi r4, rN, 0x21` and `addi r4, rN, 0x31` in `Lit_NG.s` and I initially read
them as a V/P BoxMapLight base pair. Reading the function shows a **5**-iteration
loop with `r31` starting at −2 (so `r4` = 0x1f..0x23 and 0x2f..0x33) whose
**both** calls go through slot 0x40 (`SetPConstant`) — not a V+P pair, and not
6 iterations. Unrelated code.

⚠ **A per-unit coverage check is mandatory before reading any absence as
evidence.** `TexProc.s` contains **1** pinned function, `Spline.s` **3**,
`StreamRenderer.s` **0**. An absence in those units measures *pinning*, not
retail.

### 1.3 CONFIRMED on retail bytes — 46 registers

Slot numbers are `RndShaderMgr` vtable offsets (0x18 `SetVConstant(Matrix4)`,
0x24 `SetVConstant(Vector4)`, 0x3c `SetPConstant(RndTex*)`, 0x40
`SetPConstant(Vector4)`).

| constant | our value | retail witness |
|---|---|---|
| `kVS_Color` / `kPS_Color` | 0 | `NgMat::SetRegularShaderConst` @`0x824A9918`, `li r4, 0x0` → slots 0x24 / 0x40 / 0x3c |
| `kVS_AmbientColor` / `kPS_AmbientColor` | 1 | `NgSpotlightDrawer` V+P pair, `li r4, 0x1` → 0x24 / 0x40 |
| `kVS_Specular` / `kPS_Specular` | 2 | Mat_NG, `li r4, 0x2` → 0x24 / 0x40 / 0x3c |
| `kPS_EmissiveTex` | 3 | Mat_NG `li r4, 0x3` → 0x3c; `NgPostProc::CheckGradientMap` @`0x82B889C0` |
| `kPS_EnvironMap` | 4 | Mat_NG (cube, slot 0x38); `RndSoftParticleBuffer::DoPost` @`0x824A8BE8` `li r4, 0x4` |
| `kPS_ShaderCost` | 4 | census, `SetPConstant` |
| `kVS_ViewProjMatrix` | 4 | census, `SetVConstant(Matrix4)` (slot 0x18) |
| `kPS_Texture` | 5 | Mat_NG `li r4, 0x5`; `NgRnd::SetShadowMap` @`0x82B874A0` |
| `kPS_BloomParams` | 7 | `NgPostProc::SetBloomColor` @`0x82B88148` `li r4, 0x7`; `DoBloom` @`0x82B884A0` |
| `kPS_Posterize` | 8 | `CheckPosterizeAndKaleidoscope` @`0x82B88018` `li r4, 0x8`; also DOFProc_NG |
| `kPS_SpotlightTex` | 0xB | `NgSpotlightDrawer::SetXSectionTexture` @`0x824D1F68` `li r4, 0xb`; `DoBloom` |
| `kPS_FurAlpha` | 0xB | `NgFur::Shell` @`0x82B8B340` `li r4, 0xb` |
| `kPS_FurDetail` | 0xC | `NgFur::Prep` @`0x82B8B2E8` `li r4, 0xc` |
| `kPS_FurColor` | 0xC | `NgFur::Shell` `li r4, 0xc` |
| `kPS_Anisotropy` | 0xd | Mat_NG `li r4, 0xd`; `NgPostProc::CheckNoise` @`0x82B89100` |
| `kPS_DeNormal` | 0xe | Mat_NG `li r4, 0xe` ×2 (Vector4 + RndTex\* forms) |
| `kPS_NgMatCustom` | 0xf | Mat_NG ×2 (incl. the `mRimMap` site that pins the rim source line); `DoBloom` |
| `kVS_Specular2` / `kPS_Specular2` | 0x13 | Mat_NG `li r4, 0x13` → 0x24 / 0x40 |
| `kVS_TexTransform` | 0x14 | Mat_NG `li r4, 0x14` → slot 0x18 |
| `kPS_FurGeometry` | 0x32 | `NgFur::Shell` `li r4, 0x32` → 0x24 |
| `kPS_FurShell` | 0x33 | `NgFur::Shell` `li r4, 0x33` → 0x24 |
| **`kVS_RimColor` / `kPS_RimColor`** | **0x3f** | Mat_NG @`0x824A9D48` (→0x24) and @`0x824A9DB4` (→0x40); **W6-C's fix independently reconfirmed** |
| `kVS_BoxMapLight0` / `kPS_BoxMapLight0` | 0x50 | `NgEnviron::UpdateApproxLighting` @`0x82B863F8`: **`li r29, 0x50`** then `mr r4, r29` into slot 0x24 **and** slot 0x40 in *both* loop bodies. Same register for V and P ⇒ the two enums are equal, as our header has them |
| `kVS_WorldTransform` | 0x5c | `RndShaderMgr::SetTransform` @`0x8246B940`; `NgPostProc::ModulateColorXfm` @`0x82B892C0` |
| `kPS_MotionBlur` | 0x69 | `CheckMotionBlur` @`0x82B88BC8` |
| `kPS_DetailNormal` | 0x6a | Mat_NG `li r4, 0x6a` |
| `kPS_ShadowColor` | 0x6B | `NgRnd::SetShadowMap` |
| `kPS_ShadowCamDir` | 0x6C | `NgRnd::SetShadowMap` |
| `kPS_NoiseSeeds` | 0x70 | `CheckNoise` (×2) |
| `kPS_NoiseParams` | 0x71 | `CheckNoise` |
| `kPS_HallOfTimeParams` / `kPS_HallOfTimeColor` | 0x73 / 0x74 | `CheckHallOfTime` = `fn_82B88A70` (unnamed in the map; identified by unit + adjacency) |
| `kPS_Kaleidoscope` | 0x75 | `CheckPosterizeAndKaleidoscope` |
| `kPS_GradientMap` | 0x76 | `CheckGradientMap` |
| `kPS_RefractStrength` | 0x77 | Mat_NG; `CheckRefract` @`0x82B88CC0` |
| `kPS_RefractPanning` | 0x78 | `CheckRefract` |
| `kPS_ChromaticAberration` | 0x79 | `CheckChromaticAberration` @`0x82B88E78` |
| `kPS_Vignette` | 0x7b | `CheckVignette` @`0x82B88B40` |
| `kPS_BlendPrevious` | 0x7d | `CheckBlendPrevious` @`0x82B88C60` |
| `kPS_ColorMod0` | 0x83 | Mat_NG @`0x824A9E24`, **`addi r4, r27, 0x83`** inside the 0x30-bounded loop |

### 1.4 UNAUDITABLE at current pinning — 8 registers

Not "probably right": **no witness exists in the tree**, because the only
functions that use them are absent from `target_symbol_map.json` *and* their
units are pinned to slivers.

| constants | user | why there is no witness |
|---|---|---|
| `kVS_SplineData1` 0x19, `kVS_SplineData2` 0x1A | `RndSpline::PrepareShader` | not in the map; `Spline.cpp` pins one 216-byte `.text` block, 3 functions |
| `kVS_ShockwavePos` 0x1E, `kVS_ShockwaveNormal` 0x1F, `kVS_ShockwaveParams` 0x20 | `RndShockwave::PrepareShader` | not in the map; `Shockwave.s`'s 45 pinned functions contain only `li r4` 0x0/0x1 |
| `kPS_TexProcFrequency` 0x40, `kPS_TexProcAmplitude` 0x41, `kPS_TexProcPhase` 0x42 | `TexProc::SetRegisters`, `StreamRenderer::DrawToTexture` | neither is in the map; `TexProc.cpp` is pinned to **4 bytes** (1 function) and `StreamRenderer.s` has **0** |

⇒ This is an **identification gap, not a source gap** — it is fixed by pinning
those three TUs, not by editing the header. Handoff §6.

### 1.5 COMPILED OUT — 2 registers, and no witness can ever exist

- **`kPS_HueConverge` 0xde.** Its only use is inside
  `#ifdef RB3_HAS_HUE_CONVERGE`, which is not defined. The *positive* check
  that this is consistent rather than missed: `PostProc_NG.s` is fully covered
  (46 functions, `li r4` values 0x0..0x7d) and retail's function order runs
  `CheckGradientMap` @`0x82B889C0` → `CheckHallOfTime` @`0x82B88A70` →
  `CheckVignette` → `CheckMotionBlur` → `CheckBlendPrevious` → `CheckRefract`
  → `CheckChromaticAberration`, with **no `CheckHueConverge` anywhere**. Retail
  genuinely does not have the function.
  ⚠ The unit *does* carry an unclaimed `0x7a`, which for a while looked like
  the "real" HueConverge value. It is not: `0x7a` belongs to
  `NgPostProc::DoVelocity` @`0x82B89878`, and **our source already passes a raw
  `0x7A` there**.
- **`kPS_WorldProjection` 0xdc.** Only use is inside `#ifdef RB3_DC3_MAT`
  (off), consistent with `NgMat::SetRegularShaderConst` going straight from
  `kPS_Specular2` (0x13) to `kVS_TexTransform` (0x14).

### 1.6 Verdict

**`kVS_RimColor`/`kPS_RimColor` were the only renumbered registers.** All 46
auditable constants reconcile with retail bytes; 8 have no witness at current
pinning; 2 are behind disabled `#ifdef`s. **No source change was made and none
is warranted** — item 1 is a Δ0 result, and the value delivered is that the
"maybe others were renumbered too" risk is now closed with evidence rather than
left open.

---

## 2. `RndEnviron` — identity confirmed, and it is a REVISION gap, not an offset shift

`?Save@RndEnviron@@UAAXAAVBinStream@@@Z` @`0x824302B0`, 1,056 B, fuzzy **43.95**,
204 charged sites.

**Identity audited first, because the same row shape fooled W6-C once already
(§4).** The body calls `?Save@RndColorXfm@@QBAXAAVBinStream@@@Z` (`fn_82465468`)
and `?Save@Object@Hmx@@UAAXAAVBinStream@@@Z` (`fn_8275AB90`) — which are exactly
the two distinctive calls in *our* `RndEnviron::Save` (`mColorXfm.Save(bs)` and
`SAVE_SUPERCLASS(Hmx::Object)`). **The map row is correct**; the audit could
have refuted it and did not.

⛔ **`src/system/rndobj/Env.h`'s header comment cites TU0-era addresses and
must not be trusted.** It names `RndEnviron::Save` as `fn_823F51C0`,
`OnRemoveAllLights` as `fn_823F5430` and the ctor as `fn_823F5BB8`. **`0x823F51C0`
is not a function at all in this tree** — no map entry, no body. Main has
targeted TU5 since 2026-07-15, which invalidates every TU0 address. Its
*layout* claims (mAmbientFogOwner@0x74, tail floats @0x198..0x1a4,
mUseToneMapping@0x1a8) are unverified against TU5 and disagree with what the
TU5 body actually streams. Left in place but flagged — correcting that comment
is a documentation task for whoever ports the class.

**The adjudication.** This is not a uniform member shift and not a load-order
permutation:

| | retail @`0x824302B0` | ours |
|---|---|---|
| stream revision | **`li r11, 0x25`** (37) | `bs << 0x10` (16) |
| distinct `this` offsets streamed | 49, from 0x30 to **0x208** | ~20, stopping at 0x1a8 |
| body size | 1,056 B | — |

Retail's offsets: `0x30 0x40 0x44 0x48 0x49 0x4c 0x50 0x54 0x64 0xf8 0xfc 0x100
0x104 0x108 0x10c 0x110 0x114 0x11c 0x130 0x138 0x13c 0x140 0x141 0x144 0x150
0x154 0x168 0x174 0x178 0x17c 0x18c 0x190 0x1a0 0x1a4 0x1a8 0x1b4 0x1b8 0x1bc
0x1c0 0x1c4 0x1d0 0x1d4 0x1dc 0x1e4 0x1ec 0x1f0 0x1f4 0x1f8 0x208`.

Two stream helpers were mis-inferred on the first pass and are recorded
correctly here because they change how the field list reads:
`fn_827C5098` is **`BinStream::WriteEndian(const void*, int)`** and
`fn_827C4F58` is **`BinStream::Write(const void*, int)`** — each scalar is
staged into the `0x54(r1)` scratch slot with `li r5, 0x4`, which is why
*floats and ints share one callee* (that shared callee initially looked like
evidence of a type confusion and is not). `fn_822BB448` is
`operator<<(BinStream&, const Vector4&)` and `fn_823343F8` is
`operator<<(BinStream&, const Vector2&)` — **not** the ObjPtrList helpers the
field order would suggest if you assumed our source's shape.

⇒ **Retail's `RndEnviron` is a materially larger class at a 21-revision newer
save format.** No offset edit can close this row; it is a full-class port with
`Load` and `SyncProperty` moving in lockstep. **Not attempted** — correctly a
lane of its own, as W6-C said.

**What the native runtime does wrong today:** our `Save` emits a rev-`0x10`
stream, and the mirror `Load` is written against the same reduced field set, so
**every RB3 `.milo` environment (rev `0x25`) is parsed against a 20-field
schema and desynchronises after the first absent field** — fog, colour-xfm and
every tail float read from the wrong stream position. This is the largest
single renderer-correctness item found by W6-C or this lane, and it is
*unfixable by an offset tweak*, which is the useful part of the finding.

---

## 3. `Spotlight::DrawShowing` — one real logic bug found and fixed; the +0x10 shift NOT adjudicated

`?DrawShowing@Spotlight@@UAAXXZ` @`0x824D95B0`, 904 B (report) / 968 B (asm
extent), fuzzy **79.34**, 67 charged sites.

### 3.1 The draw-mode constant — LANDED

Retail, immediately after the null test on `mBeam.mBeam` and immediately before
the slot-0x14 (`DrawShowing`) `bctrl`:

```
lwz  r3,  0x1b4(r30)          ; mBeam.mBeam
cmplwi cr6, r3, 0x0
beq  cr6, .L_824BD6A0
lwz  r11, lbl_82C76B68@l(r28) ; TheRnd
lwz  r11, 0xfc(r11)           ; TheRnd.mDrawMode
cmpwi cr6, r11, 0x4           ; <-- 4
beq  cr6, .L_824BD6A0         ; skip the draw when mode == 4
```

Our source had `TheRnd.DrawMode() != 5`. The function contains **exactly two
`cmpwi` immediates, `0x0` and `0x4`; `0x5` appears nowhere in it.** Changed to
`!= Rnd::kDrawOcclusion` (which *is* 4 in our own enum, so the named spelling
and the literal emit the same instruction).

| | predicted | measured |
|---|---|---|
| Δmatched_functions | **0** | **+0** |
| Δmatched_code | **0 B** | **+0 B** |
| Δfuzzy (whole-binary) | ~0 | +0.000000 pp |
| row `DrawShowing` fuzzy | +0.0221 pp (the `5/N` screen) | **+0.004424 pp** (79.340706 → 79.345130) |
| units at 100% | 0 | +0 on both rulers, **0 fell off** |
| leg B recompiles | ≥1 | **1** (patch was live, not absent-vs-absent) |

⚠ **Screen calibration, worth carrying forward: `one charge costs 5/N pp,
N = size/4` OVER-PREDICTS ~5× for an IMMEDIATE.** It is calibrated for a
*relocation-name* charge, which is a full instruction mismatch; a differing
immediate on an otherwise byte-identical instruction keeps most of its credit,
so it is worth a small fraction of the quoted figure. Do not price an
immediate-only fix with it.

**Native effect:** `kDrawOcclusion` (4) is the pass that renders occluders for
lens-flare visibility queries. Retail excludes the spotlight beam from it; we
included it — so **a spotlight's own volumetric beam occluded its own lens
flare**, dimming or killing flares on every spotlight with a beam. Symmetrically
we were *skipping* the beam in `kDrawOcclusionDepth` (5), where retail draws it.

### 3.2 The "+0x10 member shift" — **REFUTED. There is no shift.**

W6-C reported a uniform +0x10 member shift across this row and listed it as one
of "the two largest genuine struct divergences seen here". **It is not a struct
divergence at all.**

`scripts/harvest/class_layout_report.py Spotlight` (compiler-authoritative,
`cl.exe /d1reportSingleClassLayoutSpotlight`, `sizeof = 848 / 0x350`) against
every distinct `r30` offset retail's `DrawShowing` touches:

| retail `r30` | our layout | |
|---|---|---|
| `0xc0`  | `mDirty` [RndTransformable] | ✅ |
| `0xe8`  | `mSpotMaterial.mObject` (ObjPtr @0xe0, +8) | ✅ |
| `0xec`  | `mFlare` | ✅ |
| `0x19c` | `mColorOwner.mObject` (ObjOwnerPtr @0x194, +8) | ✅ |
| `0x1b0` | `mLensMaterial.mObject` (ObjPtr @0x1a8, +8) | ✅ |
| `0x1b4` | `BeamDef mBeam` — and the asm is literally `lwz r3, 0x1b4(r30)` = `mBeam.mBeam` | ✅ |
| `0x23c` | `mLightCanMesh.mObject` (ObjPtr @0x234, +8) | ✅ |
| `0x28c` | `mTarget.mObject` (ObjPtr @0x284, +8) | ✅ |
| `0x290` | `mTargetLoaded` | ✅ |
| `0x2a4` | `mTargetShadow` — used by name at this exact site in our source | ✅ |
| `0x2e8` | inside `mAdditionalObjects` (ObjPtrList @0x2e0, 20 B) — the `FOREACH` | ✅ |

**13 of 13 distinct offsets match. Zero shifted.**

★ **The +0x10 was measured against `r31`, which is the FRAME POINTER, not
`this`.** The tell was visible in the asm before the layout report was run:
`stw r11, 0x50(r31)` *stores* a `this`-derived pointer **into** `r31+0x50`, and
`r31` carries 9 accesses at the single offset 0x50 — a spill slot, not a member.
This is exactly the `BandDirector::Enter` false positive W6-C itself documented
("whose 20 'uniform +8' sites are all `r31` = frame pointer"), and exactly the
hazard its own `RndParticleSys::UpdateRelativeXfm` note warns about: **an
offset-shaped charge is not proof of an offset bug.** The screen that produced
it cannot tell `this` from the frame pointer, and here it did not.

⇒ The residue on this 904 B row is **frame layout + register allocation**, i.e.
codegen, not source. Do not open it as a struct lane. What *was* real in this
row is the draw-mode constant in §3.1 — one immediate, found by reading the
bytes rather than by counting offset-shaped charges.

---

## 4. `NgStats::mSpotlights` — the writer does not exist, and neither does any other

W6-C established the field and left the writer open, having ruled out
`SpotlightDrawer::DrawWorld`. Settled here.

**The struct is confirmed twice over, by two instruments that share no
arithmetic:**

1. **The allocator/clear pair.** `fn_82B871A0` selects one of three stats
   blocks at `gNgStats + 0x00 / +0x38 / +0x70`, stores the chosen pointer into a
   current-stats global `lbl_82CA8ED4`, and tail-calls `memset` with
   **`li r5, 0x38`, `li r4, 0x0`**. ⇒ **`sizeof(NgStats) == 0x38` = 14 words,
   last field at 0x34** — independent of any print loop.
2. **The overlay print table**, re-derived rather than inherited. The
   two-column branch pairs 14 distinct format strings with offsets 0x0..0x34
   and ends `0x34 → "spotlights %d %d\n"` (@`0x8219B664`). **W6-C's table is
   confirmed.**
   ⚠ My first extraction of the *one-column* branch mis-paired by one (it
   showed `"faces %d"` twice and no `"spotlights"`), because the scheduler puts
   the `addi r3, <fmt>` *after* the `lwz r4, <off>` at some sites and before it
   at others. A naive last-seen state machine is not safe on this pattern; the
   two-column branch happens to be scheduled cleanly and is the one to read.

**The answer: nothing writes `mSpotlights`, and nothing writes any other
NgStats field either.** Only **3 functions in the entire binary** reference
either `gNgStats` (`lbl_82E4B918`) or the current-stats pointer
(`lbl_82CA8ED4`), plus `EstimateDraw`:

| function | stores |
|---|---|
| `fn_82B871A0` (block selector) | 0 |
| `fn_82B871DC` (clear) | 1 — the *pointer*, then `memset(.., 0, 0x38)` |
| `?EstimateDraw@@YAMH@Z` @`0x82B87200` | **0** (pure reads) |
| `?UpdateOverlay@NgRnd@@` @`0x82B87598` | 3 — two frame spills and one write to the *pointer* global |

⇒ **In retail RB3 the whole NgStats block is only ever memset to zero and
printed.** The counter-increment sites were compiled out of the retail build;
the overlay printer survived.

★ **The control is what makes this negative worth anything:** `faces`, `parts`
and every other counter have no writer either. Had only `mSpotlights` come up
empty, the right conclusion would have been "my scan is broken" — the overlay
would print zeros for everything, which is exactly what retail does. W6-C's
call to leave the running max as a native-only static is therefore right:
**there is no retail writer to port.**

---

## 5. `0x823C8908` re-home — LANDED, +1 fn / +316 B, predicted exactly

W6-C's warning was tested literally before being acted on, by scanning the
compiled objects for the mangled names (after a full build — reflinked target
objs are pre-renamer, and a name lookup on an unbuilt tree reads "absent" for
everything):

| object | `CharTransDraw` syms | `CharBlendBone` syms |
|---|---|---|
| `build/45410914/src/system/char/CharBlendBone.obj` | **0** | 122 |
| `build/45410914/src/system/char/CharTransDraw.obj` | 43 (incl. `?SetType@CharTransDraw@@UAAXVSymbol@@@Z`) | 0 |
| `build/45410914/obj/CharTransDraw.obj` (target) | **did not exist** — no splits entry | — |

objdiff pairs target↔base **by name per unit**, so a bare map rename would have
pointed the target obj at a name `CharBlendBone.obj` cannot define ⇒ 0% forever.
**W6-C was right.**

**The accuracy argument, which is the real reason to do this:** before the fix
our `CharBlendBone::SetType` was scored against `CharTransDraw`'s retail body
and read **fuzzy 99.78** — a confident near-match between two different
functions. That is the metric hiding a real mis-attribution.

The fix is the re-home: `CharTransDraw.cpp` gets its own `splits.txt` entry with
the standalone block `0x823C8908–0x823C8A64` (its `.text` neighbours are
`Screenshot`/`RndDrawable` thunks and the preceding block ends exactly at
`0x823C8908`, so it is adjacent and non-overlapping), removed from
`CharBlendBone.cpp`'s entry — which keeps 7 other `.text` blocks, so nothing is
drained and the "single-function unit vanishes" trap does not apply.

⚠ **The first A/B REFUSED**, correctly: `.pdata` is derived output, and dtk
re-derived the range `0x82206330–0x82206340` and moved it to the new unit,
rewriting its own input. That is the documented split-guard path; recovery is
one build, commit what dtk wrote, rerun. Recorded because the refusal looks
like a failure and is not.

| | predicted | measured |
|---|---|---|
| Δmatched_functions | **+1** | **+1** (42,676 → 42,677) |
| Δmatched_code | **+316 B** | **+316 B** (Δcode% +0.003085 pp) |
| Δhonest | — | +1 |
| Δfuzzy | — | +0.000008 pp |
| `none` control | — | **+316 B** — moves *with* the graded ruler, so this is a real pairing gain, not alias forgiveness |
| units at 100% | — | **+1 on both rulers**, mechanism `NEW_UNIT` (`default/CharTransDraw`, 2 rows); **0 fell off** |

### 5.1 ⛔ The same defect exists as a FABRICATED ICF ALIAS, and it is still in the tree

`scripts/symbol_aliases.json` carries a group at **exactly this address**:

```json
{ "name": "SetType",
  "address": "0x823c8908",
  "survivor": "?SetType@CharBlendBone@@UAAXVSymbol@@@Z",
  "folded":  ["?SetType@CharTransDraw@@UAAXVSymbol@@@Z"],
  "evidence": "... Evidence tier(s) T1. T1 = RB3 retail bytes at the survivor
               address are byte-identical (modulo relocated fields, >=4 words,
               >=50% unmasked) to our compiled body for the folded spelling ..." }
```

**The T1 measurement is correct and its conclusion is inverted.** Retail's bytes
at `0x823C8908` really are byte-identical to our compiled `CharTransDraw::SetType`
— *because the address simply **is** `CharTransDraw::SetType`.* That is identity,
not folding. The builder assumed the map's name for the address
(`CharBlendBone`) was right and reached for a fold to explain the match with a
different spelling; W6-C's vbase evidence forecloses the fold outright
(`CharTransDraw` vbase adjustor **0x3c**, `CharBlendBone` **0x34** — different
code, so ICF cannot have merged them).

★ This is the campaign's own "count right, cause wrong" pattern, and the
CLAUDE.md warning that **a fold is what a wrong name looks like**, caught from
the opposite direction: the *map defect* and the *fabricated alias* are one
error with two symptoms, and fixing only the map leaves the other half in place.

**State after this lane's re-home:** the group's `survivor` no longer matches the
map's name for that address, so the group is now **map-inconsistent as well as
fabricated**. Nothing was resting on it — the A/B moved `matched_code` by +316 B
with the `none` control moving *identically*, which is the signature of a real
pairing gain rather than alias forgiveness, and `ab_measure` raised no
ALIAS_SUSPECT. But the declared fold can now *forgive a genuine wrong callee* at
real `CharBlendBone::SetType` call sites, which is an integrity hazard rather
than a scoring one.

**NOT withdrawn by this lane**, deliberately: withdrawing a group is its own
change needing its own A/B and its own gate run, and the house rule is to record
a withdrawal (`folded: []` plus a `withdrawn` record) rather than prune. Handoff
§7.1.

---

## 6. Negatives, and what this lane did NOT do

- **Did not change `ShaderMgr.h`.** The audit's finding is that no further
  renumbering exists (§1.6). A Δ0 audit that closes a risk is the deliverable.
- **Did not attempt the `RndEnviron` port** (§2) — a 21-revision, ~2× field-count
  gap, not an offset fix.
- **Did adjudicate `Spotlight`'s +0x10 shift and it is REFUTED** (§3.2) — that
  handoff is closed, not deferred. One of W6-C's two "largest genuine struct
  divergences" was a frame-pointer artifact.
- **Did not touch the `_S_sort` alias group** (W6-C handoff 2, 11 rows /
  4,712 B) — alias work needs retail-byte fold proofs and is an integrity
  hazard done casually; it is a different instrument from body work.
- **Did not run a native driver.** Both landed changes (a splits/map re-home and
  a draw-mode constant) have no checked-in fixture. The native gate is the only
  native check taken.
- **Did not pin `TexProc.cpp` / `Spline.cpp` / `Shockwave.cpp`** to close the
  8 unauditable registers — that is a splits lane, and pin work interacts with
  the metric in ways that need their own A/B.

## 7. Handoffs

**7.1 (do this first) — withdraw the fabricated `0x823c8908` alias group**
(§5.1). Evidence is complete and the fold is *disproved*, not merely
unsupported. Use the neutrality-preserving form: keep the group, set
`folded: []`, add a `withdrawn` record citing the vbase 0x3c/0x34 argument and
this lane's re-home; do not prune. Expect Δ≈0 (nothing rested on it) — land it
for the integrity, not the bytes. ⚠ Its T1 evidence string is *not* wrong about
the bytes, so a re-run of `icf_alias_build.py` will regenerate it: the generator
needs the map fix to propagate, or it will re-fabricate the group.

2. **Pin `TexProc.cpp`, `Spline.cpp`, `StreamRenderer.cpp`, `Shockwave.cpp`
   `.text`** — this is the *only* thing blocking the last 8 shader registers
   (§1.4). `RndSpline::PrepareShader`, `RndShockwave::PrepareShader`,
   `TexProc::SetRegisters` and `StreamRenderer::DrawToTexture` are all absent
   from `target_symbol_map.json`; identifying them also makes those registers
   auditable for free.
3. **`RndEnviron` full-class port** (§2) — rev `0x10` → `0x25`, ~20 → ~49
   streamed fields, `Save`/`Load`/`SyncProperty` together. Highest-value
   renderer-correctness item outstanding. Start from the offset list in §2; do
   **not** start from `Env.h`'s TU0-era comment.
4. ~~`Spotlight` +0x10~~ — **CLOSED, refuted** (§3.2). 13/13 offsets match; the
   shift was the frame pointer. Anyone re-running W6-C's MEMBER_OFFSET screen
   should add a base-register filter first, or it will re-manufacture this.
5. **Correct `Env.h`'s header comment** — it cites three TU0 addresses, one of
   which (`fn_823F51C0`) is not a function in TU5 at all.
6. **`_S_sort` alias group** — still open, still the best-concentrated alias
   target (W6-C handoff 2).
