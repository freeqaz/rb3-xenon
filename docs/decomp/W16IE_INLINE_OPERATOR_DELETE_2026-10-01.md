# W16-IE — inline the class `operator delete` where retail does (2026-10-01)

Lane W16-IE. Branch `w16-ie`, worktree `~/tmp/wt-w16-ie`, rebased onto main
`18f408adc`. Not merged.

## What the lane set out to do

`CAMPAIGN_STATE_2026-09-30.md` §4 lever 2 (branch `coord-0930-leverage`): in
`next-leverage-queue-2026-09-30.tsv`, **105 near-miss rows in 94 classes** carry
one charge, `name_pairs` = `?MemFree@@YAXPAX@Z->??3<Class>@@SAXPAX@Z` — retail
calls `MemFree` where we call the class's out-of-line `operator delete`. The fix
is the per-class inlinable-delete switch that `utl/MemMgr.h` already carries as
`OBJ_MEM_OVERLOAD_INLINE_DEL` (lanes `3f559685`, `50b50096`, `2f02cc69`,
`0cac8b3d` used it for 69 classes plus RndMat/GemTrackDir/VocalTrackDir).
§4 asked for per-class retail evidence and per-class pricing rather than a bulk
flip, because rows already at 100 that call `??3<Class>` were never screened.

## Retail evidence (before any edit was priced)

All 105 rows are `??_G` scalar deleting destructors. For each row I read the dtk
**target** object named by `objdiff.json` for its unit and listed the
relocations inside that symbol's extent (`~/tmp/w16ie_retail_census.txt`):

| retail `??_G` delete callee | rows |
|---|---:|
| `?MemFree@@YAXPAX@Z` only | **105** |
| any `??3…` | 0 |

Spot-checked on the split asm, keyed on `.fn fn_<addr>` (never the address
column): `fn_82319280` (`??_GMicInputArrow`), `fn_822ABE70`
(`??_GOutfitConfig`), `fn_824684C8` (`??_GFixedSizeAlloc`) all `bl fn_827BC430`,
which the map names `?MemFree@@YAXPAX@Z`.

The queue's class key is the `??3` name, not the `??_G` owner: e.g. the BandLabel
row is `??_GAppLabel` (MetaPanel), which inherits BandLabel's delete, and the
three `RndLight` rows are `??_GRndLight`, `??_GDxLight`, `??_GNgLight`.

## The switch

`operator new` was left exactly as it was in every class; only the delete side
moved. Three macro families needed three spellings:

| was | classes | now |
|---|---:|---|
| `OBJ_MEM_OVERLOAD(n)` | 61 | `OBJ_MEM_OVERLOAD_INLINE_DEL(n)` (existing) |
| `NEW_OVERLOAD; DELETE_OVERLOAD;` | 26 | `NEW_OVERLOAD; DELETE_OVERLOAD_INLINE;` (**new**) |
| `MEM_OVERLOAD(C, n)` (5 `char/` + TexMovie) | 6 | `MEM_OVERLOAD_INLINE_DEL(C, n)` (**new**) |
| FixedSizeAlloc's hand-rolled delete | 1 | `__declspec(noinline)` dropped |

The two new macros keep the family's `operator new` byte-for-byte (so the
`NEW_OVERLOAD` / `MEM_OVERLOAD` classes' out-of-line, ICF-folded `new` — which
lanes AT-f4 and NEWOBJ-1 established from retail `NewObject` bytes — is not
disturbed), and alias the old macro under `HX_NATIVE`, exactly as
`OBJ_MEM_OVERLOAD_INLINE_DEL` already does.

⚠ Deliberately **not** done: switching the 26 `NEW_OVERLOAD` classes to
`OBJ_MEM_OVERLOAD_INLINE_DEL` wholesale. That would also change their `operator
new` to ObjMacros.h shape (b). For most of them retail's `NewObject` is unpaired
(ObjMacros.h: "metric-invisible"), so the `new` side would be an unmeasured
change riding on a measured one. It is a separate lever and needs its own
retail-byte read per class.

## Measurement

Prediction, written before the first build: an upper bound of **+105 fns /
+8,088 B** (every target row crossing), less 40 B NewObject-unwind-funclet
losses in at least BandLabel, MicInputArrow, ScrollbarDisplay and OutfitConfig,
whose headers said to keep delete noinline for exactly that reason.

### Run 1 — all 94 classes at once, per-row attribution

`tools/ab_measure.py --from-dirty` on main `a95d5c525`, ruler `name_check`
(from `report.json` provenance), both legs settled, leg B 2,118 recompiles
(`MemMgr.h` is near-universal):

```
leg A: matched=46539 masked=23699 honest=22840 code%=45.849037
leg B: matched=46643 masked=23699 honest=22944 code%=45.927265
Δmatched=+104  Δmasked_equal=+0  Δhonest=+104  Δcode%=+0.078228pp  Δcode_bytes=+8016
units at 100% (mpn) 243 -> 255, 0 fell off; (all-rows-fuzzy) 215 -> 220, 0 fell off
```

One run was priced for 94 classes, so "measure each class" was done by diffing
every row of the two archived leg reports (`legA/legB_report.json.gz`), keyed
on (unit, symbol), and attributing each changed row to the class whose delete it
calls:

- **106 rows changed, all upward. 0 rows went down anywhere in the binary**, so
  no class had a cost for this attribution to hide, and no interaction between
  classes could mask one.
- **104 rows crossed fuzzy 100 = 8,016 B** — matches `Δcode_bytes` and
  `Δmatched` exactly: 103 target rows plus `??_GFxSendSynapse` in
  `system/synth/FxSendSynapse`, the second unit that compiles that class (the
  queue counted only the `synth_xbox` row).
- **2 target rows rose without crossing**, `??_GRndText` 99.50 → 99.75 and
  `??_GDialogDisplay` 99.41 → 99.71. The queue's "one charge only" was wrong for
  both: each carries a second destructor-name charge unrelated to this lever
  (RndText: retail `??1RndText@@UAA@XZ` vs our `??_DRndText@@QAAXXZ`, i.e. we
  emit a vbase destructor retail does not; DialogDisplay: retail's dtor is the
  ICF survivor `??1CharUpperTwist@@MAA@XZ`). Both delete charges are gone, so
  both classes are kept.

⇒ **All 94 classes paid and none cost a row; all 94 are kept.** Per-class table
below.

### The failed prediction

No funclet row moved. With the inlinable delete, our `BandLabel.obj` NewObject
section still relocates against `??3BandLabel@@SAXPAX@Z` while
`??_GBandLabel@@UAAPAXI@Z` relocates against `?MemFree@@YAXPAX@Z` — MSVC does
not inline the class delete into the EH unwind funclet, which is exactly retail's
split (retail funclet `fn_82342068`: `bl fn_8240DDB0` = `??3BinStream@@SAXPAX@Z`).
The "keep plain `OBJ_MEM_OVERLOAD` because of the funclet" notes in BandLabel.h,
MicInputArrow.h, ScrollbarDisplay.h and OutfitConfig.h (lane W16-BE) were
predictions that the funclet would inline too; the four funclet rows sat at the
same fuzzy in both legs. Those comments are rewritten. (StarDisplay.h records a
*measured* funclet loss under a different edit — its own `operator new` plus
delete — and is outside this lane's set; it was not touched.)

### Run 2 — whole branch on current main

Main moved to `18f408adc` (W17-PIERCE, which edits OutfitConfig) during the lane;
the branch was rebased onto it and the whole branch priced as one patch
(`--patch` of `git diff main...w16-ie`, worktree detached at main):

```
leg A: matched=46548 masked=23703 honest=22845 code%=45.890923
leg B: matched=46652 masked=23703 honest=22949 code%=45.969154
Δmatched=+104  Δmasked_equal=+0  Δhonest=+104  Δcode%=+0.078231pp  Δcode_bytes=+8016
Δfuzzy=+0.000224pp; units at 100% (mpn) 243 -> 255 (+12), 0 fell off
none-ruler control +0 B (NOT_APPLICABLE for a source patch: name_check-up /
none-flat is the wrong-callee-fix signature here, not an alias signature)
```

Row diff of run 2's leg reports: again 106 up, **0 down**, the same 104 crossings.
A fresh build of the branch tip reads 46,652 / 45.969154% — leg B exactly.

## Gates

- `tools/map_name_injectivity.py`: `OK: 31262 applied rows, 31261 distinct names,
  injective (+1 enumerated internal-linkage exception(s))`.
- `tools/icf_alias_finder.py --validate`: `VALIDATE: PASS -- 1484 map-consistent,
  261 tolerated (enumerated above), 0 contradicted, 1746 total`.
  (This lane touches neither the map nor `symbol_aliases.json`; both gates are
  run as the standing check, not because the change could move them.)
- `tools/native_build_gate.sh` on the branch tip `e0bdb8969` (all source in place):

  ```
  NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
  ```

## Deliberately not done / follow-ups

- **`operator new` for the 26 `NEW_OVERLOAD` classes** (bandobj 23, `Fader`,
  `MoggClip`, `RndEnvAnim`). Retail's `??_G` for all 26 inlines `MemFree`, which
  is the `OBJ_MEM_OVERLOAD`-family delete policy, and NEWOBJ-1 found retail
  contradicting the inherited `NEW_OVERLOAD` spelling 4/4 on the `new` side
  (LayerDir, UnisonIcon, OverdriveMeter, BandRetargetVignette; then BandLabel,
  OutfitConfig, MicInputArrow, ScrollbarDisplay in W16-BE). That makes shape (b)
  likely for these too, but their `NewObject` rows are mostly unpaired, so it needs
  a retail-byte read per class before it is anything more than a guess.
- **The two second charges** (`??_GRndText`: we emit `??_DRndText`, a vbase
  destructor, where retail calls `??1RndText`; `??_GDialogDisplay`: retail's dtor
  is the ICF survivor `??1CharUpperTwist@@MAA@XZ`). Neither was examined further.
- **Rows already at 100 that call `??3<Class>`** were not screened up front, as
  §4 warned. The whole-binary row diff is the screen: 0 rows fell, in both runs.
- No map, splits, or alias edits.

## Files

- `src/system/utl/MemMgr.h` — two new macros (`MEM_OVERLOAD_INLINE_DEL`,
  `DELETE_OVERLOAD_INLINE`), native aliases, and a note on the per-class delete
  exception.
- 94 class headers (one line each), plus rewritten comments in BandLabel.h,
  MicInputArrow.h, ScrollbarDisplay.h, OutfitConfig.h, PoolAlloc.h (FixedSizeAlloc),
  TexMovie.h and the five `char/` `MEM_OVERLOAD` headers.
- Run dirs: `~/tmp/wt-w16-ie/.ab_measure_runs/20261001-001449-w16ie-batch-all94-3855583`
  (run 1) and `…/20261001-011929-w16ie-whole-branch-330326` (run 2).

## Per-class attribution (run 1)

Every row below was the class's queue row, or a row that inherits its delete.
`Δbytes at 100` = size of rows that crossed fuzzy 100.

| class | header | switched to | rows (unit: symbol, fuzzy A -> B) | Δbytes at 100 |
|---|---|---|---|---:|
| BandButton | `system/bandobj/BandButton.h` | DELETE_OVERLOAD_INLINE | system/bandobj/BandButton: `??_GBandButton` 99.750 -> 100.000 | 80 |
| BandCamShot | `system/bandobj/BandCamShot.h` | DELETE_OVERLOAD_INLINE | BandCamShot: `??_GBandCamShot` 99.706 -> 100.000 | 68 |
| BandCharDesc | `system/bandobj/BandCharDesc.h` | DELETE_OVERLOAD_INLINE | BandCharDesc: `??_GBandCharDesc` 99.762 -> 100.000 | 84 |
| BandCrowdMeter | `system/bandobj/BandCrowdMeter.h` | DELETE_OVERLOAD_INLINE | BandCrowdMeter: `??_GBandCrowdMeter` 99.750 -> 100.000 | 80 |
| BandDirector | `system/bandobj/BandDirector.h` | DELETE_OVERLOAD_INLINE | BandDirector: `??_GBandDirector` 99.750 -> 100.000 | 80 |
| BandFaceDeform | `system/bandobj/BandFaceDeform.h` | DELETE_OVERLOAD_INLINE | system/bandobj/BandFaceDeform: `??_GBandFaceDeform` 99.737 -> 100.000 | 76 |
| BandHighlight | `system/bandobj/BandHighlight.h` | DELETE_OVERLOAD_INLINE | BandHighlight: `??_GBandHighlight` 99.750 -> 100.000 | 80 |
| BandLabel | `system/bandobj/BandLabel.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MetaPanel: `??_GAppLabel` 99.737 -> 100.000 | 76 |
| BandLeadMeter | `system/bandobj/BandLeadMeter.h` | DELETE_OVERLOAD_INLINE | BandLeadMeter: `??_GBandLeadMeter` 99.750 -> 100.000 | 80 |
| BandStarDisplay | `system/bandobj/BandStarDisplay.h` | DELETE_OVERLOAD_INLINE | BandStarDisplay: `??_GBandStarDisplay` 99.750 -> 100.000 | 80 |
| BandSwatch | `system/bandobj/BandSwatch.h` | DELETE_OVERLOAD_INLINE | BandSwatch: `??_GBandSwatch` 99.737 -> 100.000 | 76 |
| BandTrack | `system/bandobj/BandTrack.h` | DELETE_OVERLOAD_INLINE | system/bandobj/BandTrack: `??_GBandTrack` 99.706 -> 100.000 | 68 |
| BandWardrobe | `system/bandobj/BandWardrobe.h` | DELETE_OVERLOAD_INLINE | BandWardrobe: `??_GBandWardrobe` 99.706 -> 100.000 | 68 |
| CamShot | `system/world/CameraShot.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | CameraShot: `??_GCamShot` 99.706 -> 100.000 | 68 |
| CharBoneTwist | `system/char/CharBoneTwist.h` | MEM_OVERLOAD_INLINE_DEL | CharBoneDir: `??_GCharBoneTwist` 99.706 -> 100.000 | 68 |
| CharGuitarString | `system/char/CharGuitarString.h` | MEM_OVERLOAD_INLINE_DEL | CharGuitarString: `??_GCharGuitarString` 99.706 -> 100.000 | 68 |
| CharIKRod | `system/char/CharIKRod.h` | MEM_OVERLOAD_INLINE_DEL | CharIKRod: `??_GCharIKRod` 99.706 -> 100.000 | 68 |
| CharLipSyncDriver | `system/char/CharLipSyncDriver.h` | MEM_OVERLOAD_INLINE_DEL | CharLipSyncDriver: `??_GCharLipSyncDriver` 99.706 -> 100.000 | 68 |
| CharPosConstraint | `system/char/CharPosConstraint.h` | MEM_OVERLOAD_INLINE_DEL | CharPosConstraint: `??_GCharPosConstraint` 99.706 -> 100.000 | 68 |
| CharTransDraw | `system/char/CharTransDraw.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | CharTransDraw: `??_GCharTransDraw` 99.750 -> 100.000 | 80 |
| CheckboxDisplay | `system/bandobj/CheckboxDisplay.h` | DELETE_OVERLOAD_INLINE | CheckboxDisplay: `??_GCheckboxDisplay` 99.750 -> 100.000 | 80 |
| ChordShapeGenerator | `system/bandobj/ChordShapeGenerator.h` | DELETE_OVERLOAD_INLINE | ChordShapeGenerator: `??_GChordShapeGenerator` 99.737 -> 100.000 | 76 |
| CrowdMeterIcon | `system/bandobj/CrowdMeterIcon.h` | DELETE_OVERLOAD_INLINE | CrowdMeterIcon: `??_GCrowdMeterIcon` 99.750 -> 100.000 | 80 |
| DialogDisplay | `system/bandobj/DialogDisplay.h` | DELETE_OVERLOAD_INLINE | system/bandobj/DialogDisplay: `??_GDialogDisplay` 99.412 -> 99.706 | 0 |
| EndingBonus | `system/bandobj/EndingBonus.h` | DELETE_OVERLOAD_INLINE | EndingBonus: `??_GEndingBonus` 99.750 -> 100.000 | 80 |
| Fader | `system/synth/Faders.h` | DELETE_OVERLOAD_INLINE | Faders: `??_GFader` 99.737 -> 100.000 | 76 |
| FixedSizeAlloc | `system/utl/PoolAlloc.h` | hand-rolled (noinline dropped) | Console: `??_GFixedSizeAlloc` 99.706 -> 100.000<br>ConnectionStatusPanel: `??_GReclaimableAlloc` 99.737 -> 100.000 | 144 |
| FxSendChorus | `system/synth/FxSendChorus.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/synth_xbox/FxSendChorus: `??_GFxSendChorus360` 99.737 -> 100.000 | 76 |
| FxSendDistortion | `system/synth/FxSendDistortion.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/synth_xbox/FxSendDistortion: `??_GFxSendDistortion360` 99.737 -> 100.000 | 76 |
| FxSendFlanger | `system/synth/FxSendFlanger.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/synth_xbox/FxSendFlanger: `??_GFxSendFlanger360` 99.737 -> 100.000 | 76 |
| FxSendPitchShift | `system/synth/FxSendPitchShift.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/synth_xbox/Synth: `??_GFxSendPitchShift360` 99.737 -> 100.000 | 76 |
| FxSendSynapse | `system/synth/FxSendSynapse.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/synth_xbox/Synth: `??_GFxSendSynapse360` 99.737 -> 100.000<br>system/synth/FxSendSynapse: `??_GFxSendSynapse` 99.737 -> 100.000 | 152 |
| FxSendWah | `system/synth/FxSendWah.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | FxSendWah: `??_GFxSendWah360` 99.737 -> 100.000 | 76 |
| MicInputArrow | `system/bandobj/MicInputArrow.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MicInputArrow: `??_GMicInputArrow` 99.750 -> 100.000 | 80 |
| MoggClip | `system/synth/MoggClip.h` | DELETE_OVERLOAD_INLINE | system/synth/MoggClip: `??_GMoggClip` 99.737 -> 100.000 | 76 |
| ObjectDir | `system/obj/Dir.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/obj/Dir: `??_GObjectDir` 99.706 -> 100.000 | 68 |
| OutfitConfig | `system/bandobj/OutfitConfig.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | OutfitConfig: `??_GOutfitConfig` 99.750 -> 100.000 | 80 |
| OvershellDir | `system/bandobj/OvershellDir.h` | DELETE_OVERLOAD_INLINE | OvershellDir: `??_GOvershellDir` 99.750 -> 100.000 | 80 |
| PitchArrow | `system/bandobj/PitchArrow.h` | DELETE_OVERLOAD_INLINE | PitchArrow: `??_GPitchArrow` 99.750 -> 100.000 | 80 |
| RndAmbientOcclusion | `system/rndobj/AmbientOcclusion.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | TexBlender: `??_GRndAmbientOcclusion` 99.737 -> 100.000 | 76 |
| RndAnimFilter | `system/rndobj/AnimFilter.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | AnimFilter: `??_GRndAnimFilter` 99.706 -> 100.000 | 68 |
| RndAnimatable | `system/rndobj/Anim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Accomplishment: `??_GRndAnimatable` 99.853 -> 100.000 | 136 |
| RndCam | `system/rndobj/Cam.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/rnddx9/CubeTex: `??_GDxCam` 99.750 -> 100.000<br>Cam: `??_GRndCam` 99.750 -> 100.000 | 160 |
| RndCamAnim | `system/rndobj/CamAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | CamAnim: `??_GRndCamAnim` 99.706 -> 100.000 | 68 |
| RndCubeTex | `system/rndobj/CubeTex.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/rndobj/CubeTex: `??_GRndCubeTex` 99.737 -> 100.000<br>system/rnddx9/CubeTex: `??_GDxCubeTex` 99.737 -> 100.000 | 152 |
| RndDir | `system/rndobj/Dir.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Anim: `??_GRndDir` 99.750 -> 100.000 | 80 |
| RndDrawable | `system/rndobj/Draw.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Draw: `??_GRndDrawable` 99.833 -> 100.000 | 120 |
| RndEnvAnim | `system/rndobj/EnvAnim.h` | DELETE_OVERLOAD_INLINE | EnvAnim: `??_GRndEnvAnim` 99.706 -> 100.000 | 68 |
| RndFlare | `system/rndobj/Flare.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Group: `??_GRndFlare` 99.750 -> 100.000 | 80 |
| RndGenerator | `system/rndobj/Gen.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Gen: `??_GRndGenerator` 99.750 -> 100.000 | 80 |
| RndGroup | `system/rndobj/Group.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Group: `??_GRndGroup` 99.750 -> 100.000 | 80 |
| RndLight | `system/rndobj/Lit.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Lit: `??_GRndLight` 99.750 -> 100.000<br>system/rnddx9/CubeTex: `??_GDxLight` 99.750 -> 100.000<br>Lit_NG: `??_GNgLight` 99.750 -> 100.000 | 240 |
| RndLightAnim | `system/rndobj/LitAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | LitAnim: `??_GRndLightAnim` 99.706 -> 100.000 | 68 |
| RndLine | `system/rndobj/Line.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Line: `??_GRndLine` 99.750 -> 100.000 | 80 |
| RndMatAnim | `system/rndobj/MatAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MatAnim: `??_GRndMatAnim` 99.706 -> 100.000 | 68 |
| RndMesh | `system/rndobj/Mesh.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Mesh: `??_GRndMesh` 99.750 -> 100.000 | 80 |
| RndMeshAnim | `system/rndobj/MeshAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MeshAnim: `??_GRndMeshAnim` 99.706 -> 100.000 | 68 |
| RndMeshDeform | `system/rndobj/MeshDeform.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MeshDeform: `??_GRndMeshDeform` 99.737 -> 100.000 | 76 |
| RndMorph | `system/rndobj/Morph.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Morph: `??_GRndMorph` 99.706 -> 100.000 | 68 |
| RndMotionBlur | `system/rndobj/MotionBlur.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/rndobj/MotionBlur: `??_GRndMotionBlur` 99.750 -> 100.000 | 80 |
| RndMovie | `system/rndobj/Movie.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/rndobj/Movie: `??_GRndMovie` 99.706 -> 100.000<br>system/rnddx9/Movie: `??_GDxMovie` 99.706 -> 100.000 | 136 |
| RndMultiMesh | `system/rndobj/MultiMesh.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MeshAnim: `??_GRndMultiMesh` 99.750 -> 100.000<br>system/rnddx9/CubeTex: `??_GDxMultiMesh` 99.750 -> 100.000 | 160 |
| RndMultiMeshProxy | `system/rndobj/MultiMeshProxy.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | MultiMeshProxy: `??_GRndMultiMeshProxy` 99.750 -> 100.000 | 80 |
| RndPartLauncher | `system/rndobj/PartLauncher.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | PartLauncher: `??_GRndPartLauncher` 99.706 -> 100.000 | 68 |
| RndParticleSys | `system/rndobj/Part.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Part: `??_GRndParticleSys` 99.750 -> 100.000<br>system/rnddx9/CubeTex: `??_GDxParticleSys` 99.750 -> 100.000 | 160 |
| RndParticleSysAnim | `system/rndobj/PartAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | PartAnim: `??_GRndParticleSysAnim` 99.706 -> 100.000 | 68 |
| RndPollAnim | `system/rndobj/PollAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | PollAnim: `??_GRndPollAnim` 99.750 -> 100.000 | 80 |
| RndPostProc | `system/rndobj/PostProc.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | PostProc: `??_GRndPostProc` 99.737 -> 100.000<br>PostProc_NG: `??_GNgPostProc` 99.737 -> 100.000 | 152 |
| RndPropAnim | `system/rndobj/PropAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | PropAnim: `??_GRndPropAnim` 99.706 -> 100.000 | 68 |
| RndScreenMask | `system/rndobj/ScreenMask.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | ScreenMask: `??_GRndScreenMask` 99.750 -> 100.000 | 80 |
| RndSet | `system/rndobj/Set.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Set: `??_GRndSet` 99.737 -> 100.000 | 76 |
| RndSoftParticles | `system/rndobj/SoftParticles.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/rndobj/SoftParticles: `??_GRndSoftParticles` 99.750 -> 100.000 | 80 |
| RndTex | `system/rndobj/Tex.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Tex: `??_GRndTex` 99.737 -> 100.000 | 76 |
| RndTexBlendController | `system/rndobj/TexBlendController.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | TexBlendController: `??_GRndTexBlendController` 99.737 -> 100.000 | 76 |
| RndTexBlender | `system/rndobj/TexBlender.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | TexBlender: `??_GRndTexBlender` 99.750 -> 100.000 | 80 |
| RndTexRenderer | `system/rndobj/TexRenderer.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Rnd_Xbox: `??_GDxTexRenderer` 99.750 -> 100.000<br>TexRenderer: `??_GRndTexRenderer` 99.750 -> 100.000 | 160 |
| RndText | `system/rndobj/Text.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Text: `??_GRndText` 99.500 -> 99.750 | 0 |
| RndTransAnim | `system/rndobj/TransAnim.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | TransAnim: `??_GRndTransAnim` 99.706 -> 100.000 | 68 |
| RndTransProxy | `system/rndobj/TransProxy.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | TransProxy: `??_GRndTransProxy` 99.750 -> 100.000 | 80 |
| RndTransformable | `system/rndobj/Trans.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | CharBone: `??_GRndTransformableRemover` 99.750 -> 100.000 | 80 |
| RndWind | `system/rndobj/Wind.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Wind: `??_GRndWind` 99.737 -> 100.000 | 76 |
| ScrollbarDisplay | `system/bandobj/ScrollbarDisplay.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | ScrollbarDisplay: `??_GScrollbarDisplay` 99.750 -> 100.000 | 80 |
| SongSectionController | `system/bandobj/SongSectionController.h` | DELETE_OVERLOAD_INLINE | CharIKFingers: `??_GSongSectionController` 99.706 -> 100.000 | 68 |
| SpotlightDrawer | `system/world/SpotlightDrawer.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | SpotlightDrawer: `??_GSpotlightDrawer` 99.737 -> 100.000<br>SpotlightDrawer_NG: `??_GNgSpotlightDrawer` 99.737 -> 100.000 | 152 |
| SpotlightEnder | `system/world/SpotlightEnder.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | SpotlightEnder: `??_GSpotlightEnder` 99.750 -> 100.000 | 80 |
| StreakMeter | `system/bandobj/StreakMeter.h` | DELETE_OVERLOAD_INLINE | StreakMeter: `??_GStreakMeter` 99.750 -> 100.000 | 80 |
| SynthEmitter | `system/synth/Emitter.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Emitter: `??_GSynthEmitter` 99.750 -> 100.000 | 80 |
| TexMovie | `system/movie/TexMovie.h` | MEM_OVERLOAD_INLINE_DEL | TexMovie: `??_GTexMovie` 99.750 -> 100.000 | 80 |
| TrackPanelDir | `system/bandobj/TrackPanelDir.h` | DELETE_OVERLOAD_INLINE | TrackPanelDir: `??_GTrackPanelDir` 99.750 -> 100.000 | 80 |
| TrackPanelDirBase | `system/bandobj/TrackPanelDirBase.h` | DELETE_OVERLOAD_INLINE | TrackPanelDirBase: `??_GTrackPanelDirBase` 99.750 -> 100.000 | 80 |
| WorldCrowd | `system/world/Crowd.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Crowd: `??_GWorldCrowd` 99.750 -> 100.000 | 80 |
| WorldDir | `system/world/Dir.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | system/world/Dir: `??_GWorldDir` 99.750 -> 100.000 | 80 |
| WorldInstance | `system/world/Instance.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | Instance: `??_GWorldInstance` 99.750 -> 100.000 | 80 |
| WorldReflection | `system/world/Reflection.h` | OBJ_MEM_OVERLOAD_INLINE_DEL | LightHue: `??_GWorldReflection` 99.750 -> 100.000 | 80 |

Total rows crossing fuzzy 100: 104, bytes 8016
