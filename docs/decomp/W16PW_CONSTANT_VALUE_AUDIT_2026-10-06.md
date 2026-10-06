# W16-PW — constant-value audit: what our functions load vs what retail loads (2026-10-06)

**Lane:** W16-PW · **branch:** `w16-pw` · **tool:** `tools/const_value_audit.py`
**Target image:** clean retail TU5 (`default.xex` sha1 `d56e7f31…`, `band.exe`
sha1 `5f3f667a…`), with the RB3DX image (`orig/45410914/rb3dx-archive/band.exe`)
as the cross-check (`--alt-image`).

## Why this lane existed

The grader compares a float/double/string literal load by the **name** of the
label it relocates to. When retail's label is anonymous (`lbl_8200xxxx`) and ours
is `__real@…` or `??_C@…`, `name_check` forgives the pair as a placeholder and
**never looks at the value**. So a wrong literal can sit in a row that scores
100.0. W16-PQ found one by reading (a byte-swapped `9.0e9f` in
`VocalPlayer::Poll`); W16-PR found more by hand with two prototypes
(`~/tmp/w16pr/constcheck2.py` position-paired, `constmulti.py` value-set). This
lane turned them into one whole-binary instrument and fixed what it found.

## The tool

`python3 tools/const_value_audit.py --jobs 24 --alt-image orig/45410914/rb3dx-archive/band.exe --out ~/tmp/cva.json`
(`--unit U` / `--rows SYM…` for a slice). A full run takes ~90 s on 24 jobs
and needs a **built** tree (the renamer and the six patchers are part of what it reads).

For every paired row it takes objdiff's aligned instruction listing
(`objdiff-cli diff --include-instructions --batch`, at the graded config) and, for every
`@l` load (`lfs/lfd/lwz/ld/…` and the `addi` that forms a string address):

- **retail value** — read from `band.exe` at the EA **rebuilt from retail's own
  instruction** (the `lis` high half nearest the label + the instruction's
  16-bit immediate), not from the split obj's relocation. The function VA comes
  from `symbols.txt` + `target_symbol_map.json` and is **verified** by matching
  the target obj's non-relocated words against `band.exe`.
- **our value** — read from our compiled obj's section bytes at the relocation
  target + addend. Writable (`.data`/`.bss`) targets are skipped as mutable.
- strings are compared as C strings (each side read to its own NUL, empty
  `??_C@` literals included); floats/doubles/words by raw bytes.

Three comparisons per function:

| check | what it compares | catches | blind to |
|---|---|---|---|
| POS | the value at each aligned load row | wrong literal, swapped literals | — (flags reorders, so see "cleared") |
| SET | values loaded anywhere in the function, both sides | wrong literal even when the load is unaligned | swaps |
| USE | the value reaching each aligned consumer: an aligned row running the **same opcode** on both sides, a call's argument register, or a **store keyed by its memory slot** (op, offset, base) | swaps of values between consumers | consumers that are themselves unaligned |

A POS mismatch is **cleared** (a load reorder) only if both loads reach an
aligned consumer that sees the same value on both sides. Verdicts:
`VALUE` (a value one side loads and the other never does, involved in a mismatch),
`SWAP` (same value set, different consumer), `POS_UNRESOLVED`, `REORDER` (cleared),
`SETONLY` (set differs but no aligned evidence), `IMAGE_PATCH` (the mismatch
disappears on `--alt-image`), `CLEAN`. MSVC switch byte tables (`$T…` in a
function with `bctr`) are skipped.

### Two instrument findings worth keeping

1. **dtk drops relocation addends on split target objs.** The target obj
   relocates a load to the *containing* label and loses the addend:
   `std::exception::what` is `lis 0x8201 / addi -0x1500` = `0x8200EB00`, but the
   reloc reads `lbl_8200EAF8+0`. Reading retail through the reloc gives the
   wrong 8 bytes. Hence the EA is rebuilt from retail's instruction word. The
   same effect hid this lane's best find: `GetJoypadExtraLagInits` displays as
   `__real@41600000` (14.0) in objdiff while retail actually loads `0x82091FC0`
   = **74.0** (the neighbour label is `0x82091FBC`, the +4 is gone).
2. **objdiff's combined-section addresses are not symbol offsets** — the
   instruction VA is `fn_va + (row_addr − first_row_addr)`, per side.

### Control (planted, built, audited, restored — run twice)

| plant | tool verdict | `report.json` fuzzy / mpn, planted vs clean |
|---|---|---|
| `VocalPlayer::Poll` `frameMinPitch` 9.0e9f → 9985.578125f (the PQ byte swap) | `VALUE` row 32, retail `50061c46` ours `461c0650` | 94.98465 / 95.27391 **both** |
| `VocalTrack::MissTambourineGem` `"miss"` → `"mist"` | `VALUE` row 17 | 100.0 / 100.0 **both** |
| `Frustum::Set` `back.Set(0,-1,0,far)` → `(-1,0,0,far)` (swap two stored values) | `SWAP` at store slots `0x10(r3)` / `0x14(r3)` | **99.236115 / 100.0 planted vs 99.208336 / 99.97222 clean — the wrong code scores HIGHER** |

The first two prove the metric is blind to the class; the third proves it can
reward it. Evidence: `~/tmp/w16pw_control_evidence.json`,
`~/tmp/w16pw_control2_evidence.json`; both runs are recorded in commit messages.

## Run totals (whole binary, 1,046 units, 55,535 functions)

| run | image | tree | position pairs | unequal | VALUE | SWAP | POS_UNRES | SETONLY |
|---|---|---|---:|---:|---:|---:|---:|---:|
| 1 | RB3DX | main before fixes | 28,038 | 257 | 145 | 10 | 4 | 185 |
| 2 | clean TU5 | batch 1 fixed, rebased | 27,998 | 195 | 113 | 11 | 4 | 186 |
| final | clean TU5 | all fixes except ProfileMgr; tool refined | 27,998 | 193 | 111 | 8 | 5 | 186 |

99.3% of compared position pairs agree. After the ProfileMgr fix, `default/ProfileMgr` re-audits with no flag. Of the final 111 `VALUE` rows, **102 are
in `src/network`** and every one is a Quazal `__FILE__` path string (out of
scope by directive, left alone). The remaining in-scope rows are all
adjudicated below.

## Fixed — the code now loads retail's value

The address is the retail `.rdata`/`.data` EA the tool read.

| file | function | was | retail | retail EA |
|---|---|---|---|---|
| `band3/game/GameMicManager.cpp` | SetPitchCorrectionTarget, HookUpFxForMicId | `"mic.send"`, 8.1758f | `"synapse.send"`, 8.1757989f (0x4102D013) | 0x820DE688 / 0x820DE680 |
| `band3/game/Metronome.cpp` | SetVolume, GetVolume | fader range −20..0 dB | −10..0 dB (10.0, −10.0) | 0x82001098, 0x820F38D8 |
| `band3/game/TrainerGemTab.cpp` | Draw | 60 / 120 | 50 / 100 | 0x82020480, 0x8200F4A0 |
| `band3/meta_band/MetaPanel.cpp` | FinishLoad | `"fade"` | `"background_music_level.fade"` | 0x82092174 |
| `band3/meta_band/ProfileMgr.cpp` | GetJoypadExtraLagInits (PS3/Wii real guitar, VCal) | 14.0 | **74.0** | 0x82091FC0 |
| `band3/meta_band/SongUpgradeMgr.cpp` | ContentPattern | `"&upgrades.dta"` | `"upgrades.dta"` | 0x820D1A58 |
| `system/bandobj/Band.cpp` | BandSong::StaticClassName | `"BandSong"` | `"Song"` (no `BandSong` string in band.exe) | 0x82010000 |
| `system/bandobj/BandCharacter.cpp` | SetState | `"realtime_idle"` | `"sit"` | 0x82010E18 |
| `system/bandobj/BandCharacter.cpp` | BandCharacter ctor (SetAngRadius) | 0x3E32B8C2 | 0x3E32B8C3 = (float)PI/18 | 0x82012CC4 |
| `system/bandobj/BandPatchMesh.cpp` | WorkVerts::SetSameVerts | 0.01f (0x3C23D70A) | 0.1f*0.1f (0x3C23D70B) | 0x820398F0 |
| `system/bandobj/VocalTrackDir.cpp` | TypeToString | lower-case `int`, `float`, … | `Int`, `Float`, `Var`, `Func`, `Object`, `Sym`, … | 0x82028EC8.. |
| `system/beatmatch/MasterAudio.cpp` | RestoreDrums | 250 ms | 125 ms | 0x8210CED4 |
| `system/beatmatch/MasterAudio.cpp (PitchMucker.h)` | PitchMucker::UpdatePitch | 0x3F7F7D1F | 0.99800289f (0x3F7F7D1E) | 0x8210C7E8 |
| `system/beatmatch/VocalNoteList.cpp` | DetermineFreestyleSections | 64.0f * pad | 2.0f * pad | 0x820F3A40 |
| `system/char/CharEyes.cpp (math/Easing.h)` | ProceduralBlinkUpdate (EaseInExp) | pow(t, 3.03f) | pow(t, 3.76f) | 0x82045610 |
| `system/char/CharSleeve.cpp` | Poll | 0xC076EDDD | −98/25.4 (0xC076EDDC) | 0x8204AC48 |
| `system/dsp/PitchDetector.cpp` | PitchDetector ctor | full-precision IIR coefficients | the 5-digit coefficients (0.046583, 0.18633, …) | 0x8219B058..0x8219B070 |
| `system/obj/DirLoader.cpp` | SetupDir | `"… class %s %s"` | `"… class %s s"` | 0x82106860 |
| `system/os/OnlineID.cpp` | ToString | `"%0x16llx"` | `"%016llx"` | 0x8208A360 |
| `system/rnddx9/RenderState.cpp` | SetDepthFunc, SetStencilFunc | tf2cf in enum order | {7,1,3,2,5,4,6,0, 7,4,6,2,5,1,3,0} | 0x821019C8 |
| `system/rndobj/Rnd.cpp` | YRatio, PreInit | kRatio[5] with 0.6 | kRatio[4] = {1, 0.75, 0.5625, 0.5625} | 0x8205E1E0 |
| `system/rndobj/Rnd.cpp` | UpdateRate | `"gs"`, `"cpu"`, `""` | `" gs "`, `" cpu"`, `"    "` | 0x8205E9EC.. |
| `system/synth_xbox/ExternalMic.cpp` | GetRequiredGain | 0.0 when no mic | 1.0 | 0x820009FC |
| `system/synth_xbox/Mic.cpp` | ChatReceiver::ProcessChatData | 0x39CDE32B | PI/8000 (0x39CDE32E) | 0x82194D58 |
| `system/synth_xbox/Synapse_dsp.cpp` | Synapse::DSP ctor | 7902.13 / 0.70710 / 8.1177e-3 | 7862.0 / 0.707 / 8.16f*0.001f | 0x82198028, 0x82198408 |
| `system/synth_xbox/Synth.cpp` | Synth360::PreInit bus labels | `front_left`, … | `"  FL"`, `"  FR"`, `"   C"`, `" LFE"`, … | 0x82194AE8..0x82194B20 |
| `system/synth_xbox/Voice.cpp` | createOrReuse (MaxFrequencyRatio) | 4.0f | 10.0f from a writable `.data` float | 0x82CA69C4 |
| `system/world/CameraManager.cpp` | GetFreeCam | FreeCamera(…, 0.05f) | 0.2f | 0x82024D44 |

## Stays — with the reason

**In scope, value-equal or not a source defect (21 rows: every non-SETONLY in-scope flag in the final run):**

| row (fuzzy) | verdict | why it stays |
|---|---|---|
| `BandCrowdMeter::Poll` (97.64) | VALUE | equal: retail folds the sign into `fmadds`/`fnmsubs`; W16-PQ read it the same way |
| `RndSoftParticleBuffer::BlurSurface` (66.11) | VALUE | equal: retail loads the `kBlurOffsets` values as immediates, ours from a mutable static table; no value only we have |
| `MoggClip::SetupPanInfo` (78.71) | VALUE | equal: `−(x*0.5)` canonicalisation, a documented codegen wall in the source |
| `Spotlight::BuildNGQuad` (86.61) | VALUE | equal: `a + b*-1` strength-reduced (the in-source W7-AE note) |
| `RndTexBlendController::GetBlendState` (95.34) | VALUE | equal: retail keeps `-2.0f` and `fmadds`; our compiler folds the sign (`2.0f` + `fmsubs`). Both spellings tried, 95.34 both |
| `RndRenderState::SetAlphaFunc` (100.0) | VALUE | extent artefact: retail has a separate 8-entry alpha table at `0x82101A08` with bank 0's values; the read runs past it |
| `CharacterTest::Handle` (0.0) | VALUE | mispaired row (fuzzy 0), not a value comparison |
| `DxParticleSys::Init`, `DxRnd::DrawRectDepth` (100.0) | VALUE | **real, out of scope**: the vertex-decl end marker's `Stream` is `0x00FF` in retail and `0xFFFF` in ours, from `src/xdk/d3d9i/d3d9types.h:544` `#define D3DDECL_END() { -1, 0, -1, … }`. The one-line fix is `Stream` = `0xFF`; it is in `src/xdk`, so it is recorded, not made |
| `fft_real_forward_scalar` (86.24) | SWAP | equal: retail evaluates `sin(πx)` before `sin(2πx)`; the values match. Swapping the source order dropped the row to 72.59, reverted |
| `EstimateDraw` (99.95) | SWAP | equal: all twelve field×weight pairs match (traced by register); only the summation order differs. A left-to-right rewrite dropped it to 10.07, reverted |
| `ObjectDir::ResetViewports` (98.35) | SWAP | equal: ours computes `z*0 − y*768` (`fmsubs`), retail `z*0 + y*(−768)`; the same negation wall |
| `ChatReceiver::ProcessChatData` (99.81) | SWAP | equal: the ±32767 clamp registers are permuted (`f9`/`f10`), the `fsel` chain is the same; the "call args" are live registers across `DbToRatio`, not arguments |
| `WahEffect::Process` (91.76) | SWAP | equal: three constants loaded in a different order; each multiplies the same field |
| `VoiceBeat::Analyze` (86.55) | SWAP | equal: the hoisted filter coefficients land in different stack slots and the delay-line indices shift with them; each coefficient still multiplies the same `yv[k]` |
| `CharKeyHandMidi::Poll` (84.28) | SWAP | equal: traced by register — −0.4, 1/14, 1/28, 0.5 and −1.0 each multiply the same operands; the listing alignment drifts |
| `PatchLayer::Draw` (87.35) | POS_UNRESOLVED | equal: load reorder; W16-PR already fixed this row's values |
| `MemTracker::DiffDump` (99.04) | POS_UNRESOLVED | equal: `"free"`/`"alloc"` loaded in the other order; each reaches the same `ColatedPrint` |
| `RndText::WrapText` (79.41) | POS_UNRESOLVED | equal: retail loads `-0.5f` for centring where ours does `w*0.5` then a shared `fneg`; a `width * -0.5f` spelling dropped the row to 79.16, reverted |
| `CharIKHand::Poll` (98.26) | POS_UNRESOLVED | equal: 0.001 and 144 loaded in the other order into the same registers |
| `UIStats::MaybePublish` (99.59) | POS_UNRESOLVED | cleared by W16-PR with a register map; retail's `"%s:%s"` sits after non-ASCII data |

**SETONLY (48 in scope):** 32 are fuzzy-0 rows (mispaired or unpaired, so the sets
compare unrelated code) and 2 are anonymous `fn_` rows. Of the 14 at fuzzy ≥ 50,
one was a real defect and is fixed (`GetJoypadExtraLagInits`, 74.0). The rest:
retail-only `ffffffff` / `82c16330` / `82c16a80` are loads of externs defined in
another object (`FixedString::npos`, `_xhv_voicechat_mode`), unresolved on our
side; `FinishPostProcess` (retail 0.3, 1.0) is the documented out-of-line
`MakeColor(Color(0,0,0.3f))` our build constant-folds; `AccomplishmentProvider::Mat`
(ours 0.25) is equal — retail reads 0.25 at `0x820BC0E0 − 0x1c` through a
co-addressed base, which the tool does not evaluate; `RndAmbientOcclusion::Tessellate`
(our printf strings) is being converted to `MILO_LOG` by lane W16-PU.

**Out of scope:** 102 `src/network` VALUE rows, every one a Quazal `__FILE__`
path string; 137 `src/network` SETONLY rows; 1 `src/network` SWAP; 1 `src/xdk`
SETONLY. Left alone by directive.

## A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16pw --patch <git diff main w16-pw -- src/>`,
worktree detached at main `c1f6bafef`, **both legs on the clean TU5 image**
(`default.xex` d56e7f31), ruler `name_check`, 26 source files, 624 recompiles in leg B.

| | leg A (main) | leg B (+ this lane) | Δ |
|---|---:|---:|---:|
| matched_functions | 53,528 | 53,529 | **+1** |
| honest (matched − masked_equal) | 28,335 | 28,336 | +1 |
| matched_code % | 57.967995 | 57.972137 | **+0.004142 pp (+424 B)** |
| fuzzy % | 63.699642 | 63.700340 | +0.000698 pp |
| units at 100 % (mpn / all-fuzzy) | 554 / 492 | 554 / 492 | 0 |

Per row, from the two archived leg reports: **0 rows down, 2 up** —
`Rnd::UpdateRate` 85.42 → **100.0** (the fixed-width rate-gate strings) and
`ProfileMgr::GetJoypadExtraLagInits` 97.28 → 99.91 (the 74.0 un-merges the
tail block). **Every other fix is invisible to the metric — as predicted, since
that is the defect class.** `none`-ruler control: +872 B, reading only (a
source patch is expected to move it).

## What this lane did not do

- **No `src/network` or `src/xdk` edits.** The 102 Quazal `__FILE__` strings and
  the `D3DDECL_END` stream byte (below) are recorded, not fixed.
- No grinding of the codegen-only rows (negation canonicalisation, load order,
  stack-slot hoisting). Three cheap source reorders were *tried and reverted*
  because they lowered their row: `EstimateDraw` left-to-right sum
  (99.95 → 10.07), `fft_real_forward_scalar` sin order (86.24 → 72.59),
  `GetHorizontalAlignOffset` as `width * -0.5f` (79.41 → 79.16).
  `GetBlendState` `t2*3 + t3*-2` was neutral (95.34 both) and also reverted.
- SETONLY rows at fuzzy 0 (32 in scope) are mispaired/unpaired rows; their value
  sets compare unrelated code and were not adjudicated one by one.

## Tool limits (known, measured)

- A load through an `addi`-built base plus displacement (MSVC global
  co-addressing, e.g. `AccomplishmentProvider::Mat` reading 0.25 at
  `0x820BC0E0 − 0x1c`) is not evaluated; it shows as SETONLY.
- A load whose our-side target is defined in another object (`FixedString::npos`,
  `_xhv_voicechat_mode`) is unresolved on our side; SETONLY.
- USE needs aligned consumers; in low-fuzzy rows (`CharKeyHandMidi::Poll` 84%,
  `WahEffect::Process`) the pairing drifts and the SWAP verdict has to be read by
  hand (both came out equal).
