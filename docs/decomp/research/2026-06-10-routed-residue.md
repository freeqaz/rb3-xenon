# Routed near-miss residue triage — MEMBER_DELTA + UNKNOWN buckets (2026-06-10)

Input: `/home/free/tmp/hasreal_routed_v2.json` (330 HAS_REAL entries; this doc covers the
12 `MEMBER_DELTA_CANDIDATE` + 15 `UNKNOWN` entries). All percentages below are
**`match_percent_normalized` from a fresh `build/45410914/report.json` at main `20590dd`
(6851/65544 matched)** — re-pulled per fn because the refill sweep landed after routing.
Diff evidence from `scripts/analysis/diff_inspect.py --symbol ... --unit ... --compare-asm/--diagnose`.

**Headline: 8 of 27 already matched (FileMerger×5, Rnd×2, ColorPalette — landed by the
06-09/10 wave). Of the 19 live, 6 actionable levers found (2 of them likely multi-fn
cascades, incl. one unit-level MIS-PIN worth +10–25), 8 parked on known walls, 5 new
classifier patterns identified.**

---

## 1. Status table (all 27)

| # | fn (short) | unit | report % | verdict | class |
|---|---|---|---|---|---|
| 1 | `?GetFrameMatchType@Singer@@QAAHXZ` | band3/game/Singer | 99.909 | PARK (optional bounded recon) | VocalPlayer base +4, no clean oracle |
| 2 | `?DrawStringScreen@Rnd@@...` | Rnd | **100 — drop** | done (Rnd vtable `30a4ae8`) | — |
| 3 | `_Destroy_Range<CamShotCrowd>` | CameraShot | 99.950 | **FIX: map-entry SWAP** | SWAPPED_MAP_LABELS (new) |
| 4 | `_Destroy_Range<CamShotFrame>` | CameraShot | 99.950 | **FIX: map-entry SWAP** | SWAPPED_MAP_LABELS (new) |
| 5 | `__uninitialized_fill_n<ColorSet>` | ColorPalette | **100 — drop** | done (`60eabed`) | — |
| 6 | `?DrawStartFinish@TrainerGemTab@@` | band3/game/TrainerGemTab | 99.923 | PARK | UI-MI wall (roadmap: "separate 15-slot UILabel/UIComponent-MI delta", `docs/plans/ui-base-layout-reconstruction.md`); delta +60 = 15 slots |
| 7 | `_M_fill_insert<FileMerger::Merger>` | FileMerger | **100 — drop** | done (`dc080dd`) | — |
| 8 | `?EndWorld@Rnd@@QAAXXZ` | Rnd | **100 — drop** | done (`30a4ae8`) | — |
| 9 | `_M_allocate_and_copy<Merger>` | FileMerger | **100 — drop** | done | — |
| 10 | `_M_allocate_and_copy<SampleZone>` | MidiInstrument | 99.960 | **FIX: unit re-pin** (see §3) | UNIT_MISPIN (new) |
| 11 | `_M_fill_insert<SampleZone>` | MidiInstrument | 99.964 | **FIX: unit re-pin** | UNIT_MISPIN |
| 12 | `?UpdateTarget@Target@BandCamShot@@` | BandCamShot | 99.962 | weak FIX (1 codegen + naming) | mixed; low priority |
| 13 | `resize<vector<Merger>>` | FileMerger | **100 — drop** | done | — |
| 14 | `resize<vector<SampleZone>>` | MidiInstrument | 99.938 | **FIX: unit re-pin** | UNIT_MISPIN |
| 15 | `?Load@CamShotCrowd@@QAAXAAVBinStream@@@Z` | CameraShot | 99.971 | **FIX: WorldCrowd member drop** (§5) | MEMBER_DELTA (real, +8) |
| 16 | `??1RndEnvironTracker@@QAA@XZ` | BandCharacter | 99.975 | PARK | RndEnviron 20-slot vtable/vbase wall (known, deferred) |
| 17 | `?SetupShader@NgMat@@QAAX_N0@Z` | Mat_NG | 99.906 | PARK | Mat_NG scrambled layout (`docs/decomp/matng-deferral.md`) |
| 18 | `_M_fill_insert_aux<Merger>` | FileMerger | **100 — drop** | done | — |
| 19 | `??0RndEnvironTracker@@QAA@PAVRndEnviron@@PBVVector3@@@Z` | BandCharacter | 99.984 | PARK | same as #16 (see §8 classifier gap) |
| 20 | `?Select@RndShaderSimple@@MAAX...` | Shader | 99.962 | PARK → matng | RndMat deltas +0x3c/+0x68 = the Mat_NG scramble; **add to matng-deferral validation set** |
| 21 | `vector<Merger>::operator=` | FileMerger | **100 — drop** | done | — |
| 22 | `??0Stream@@IAA@XZ` | Stream | 99.977 | **FIX (recon-first): Synth −0xc** (§6) | MEMBER_DELTA (real, +12 on TheSynth) |
| 23 | `?LocalizeFloat@@YAPBDPBDM@Z` | Locale | 99.952 | PARK | DATA_LBL_ONLY (new tooling class) |
| 24 | `??1AccomplishmentProgress@@UAA@XZ` | band3/meta_band/AccomplishmentProgress | 99.958 | **FIX: per-TU `/DRB3_RBTREE_0x1C`** (§4) | RBTREE 0x18→0x1c stair-step |
| 25 | `?LocalizeSeparatedInt@@YAPBDHAAVLocale@@@Z` | Locale | 99.992 | PARK | DATA_LBL_ONLY |
| 26 | `?Print@RndTex@@UAAXXZ` | Tex | 99.917 | **FIX: drop RndTex::unk2c** (§2) | MEMBER_DELTA (real, uniform +4) |
| 27 | `yylex` | DataFlex | 99.958 | PARK | GENERATED_CODE: flex DFA table divergence |

---

## 2. LEVER A — RndTex `unk2c` drop (uniform +4, multi-fn) — HIGH confidence

**Evidence.** Three named near-misses in `default/Tex` carry a single uniform **+4
(ours−retail)** `this`-relative delta (gate-zero clean: base reg is `r3`-derived `this`):

- `?Print@RndTex@@UAAXXZ` 99.917 — `lwz r30, 0x50(r3)` ours vs `0x4c` retail; `0x54` vs `0x50`; `0x58` vs `0x54` (3×, plus ICF `TextStream<<Symbol`-vs-`<<PBD` name noise).
- `?Save@RndTex@@UAAXAAVBinStream@@@Z` 99.885 — diagnose: "dominant delta = +4 (9 instructions)", no inserts/deletes (retail serializes the SAME fields, just −4).
- `??1RndTex@@UAA@XZ` 99.903 — +4 ×3.
- `?SetBitmap@RndTex@@...` 89.985 — likely partial beneficiary (has other issues too).

**Root cause.** `src/system/rndobj/Tex.h:156` — `Hmx::CRC unk2c; // 0x2c` sits before
`RndBitmap mBitmap`. **rb3-Wii `../rb3/src/system/rndobj/Tex.h` has NO such member**
(`mBitmap // 0x1c` follows the prior block directly); it's a DC3-added field. Everything
from `mBitmap` (incl. `mMipMapK/mType/mWidth/mHeight/mBpp/mFilepath/...`) is +4 in ours.

**Fix.** Drop/gate `unk2c` exactly per the TexRenderer precedent (`9150f3c`) — comment-tag
or `#ifdef` gate (DC3-only member). The only code touching it is
`src/system/rndobj/Tex.cpp:91` `COPY_MEMBER(unk2c)` (gate that line too).
NOTE: `ShaderMgr.unk2c` hits in Shader.cpp/PostProc_NG.cpp are a DIFFERENT class — ignore.

**Blast radius.** `rndobj/Tex.h` is included by **84 files**, and the shift moves
`mBitmap`+everything after — any TU reading `tex->mWidth` etc. shifts too. That is the
POINT (cross-unit cascade like TexRenderer +6), but mandates the standard whole-binary
A/B with zero-regression gate. Est **+3 to +6** (3 Tex fns near-certain; SetBitmap +
cross-unit possible).

---

## 3. LEVER B — `default/MidiInstrument` is MIS-PINNED (unit-level, biggest upside)

**Evidence chain:**

1. The three "vector<SampleZone>" near-misses (99.938–99.964) all diff ONLY in the element
   stride: retail `li r8, 0x1c` / `mulli r11, r11, 0x1c` vs ours `0x50`.
   Our compiled `sizeof(SampleZone)` = 0x50 (`src/system/synth/SampleZone.h`; rb3-Wii has
   the same member list → retail SampleZone canNOT be 0x1c).
2. `sizeof(CharSignalApplier::BoneOp)` = **exactly 0x1c** with the poly-ObjPtr layout
   (`src/system/char/CharSignalApplier.h:13`: ObjPtr 0xc + int + 3 floats).
3. The pinned range (`splits.txt:2166`: `MidiInstrument.cpp .text 0x822B0C60–0x822B3D28`)
   contains **foreign named symbols**: `?ApplyConstraints@BandIKEffector@@...` (0% @ size
   544) and `?_M_erase@?$vector@UBoneOp@CharSignalApplier@@...` (0%). BandIKEffector is an
   rb3-Wii bandobj class (`../rb3/src/system/bandobj/BandIKEffector.h`) — this region is
   the **BandIKEffector/CharSignalApplier char-IK TU**, not MidiInstrument.
4. The TRUE MidiInstrument cluster sits in **`default/auto_03_826F42A8_text` (169 fns, 0
   matched)**: `?MakeNoteInst@MidiInstrument@@...` @0x826F5AF8, `operator<<(BinStream&,
   vector<SampleZone>)` @0x826F5CC0, `??0SampleZone copy` @0x826F5E20 (all named in
   `scripts/target_symbol_map.json`, all 0%).
5. The wrong unit's match profile corroborates: `deallocate<StlNodeAlloc<SampleZone>>`
   4.2%, `__destroy_range<SampleZone>` 0.5%, `_M_erase` 61%, `operator=` 88% — only the
   stride-insensitive template bodies score high.

**Fix (relocate_engine-class job, the +1566-campaign machinery):**
- Re-derive MidiInstrument.cpp's true `.text` span by content-matching our compiled
  `build/45410914/src/system/synth/MidiInstrument.obj` against the 0x826F4xxx region
  (`tools/` fuzzy/content match kit, then `pin_identified`/`relocate_engine_splits` flow).
- Re-attribute 0x822B0C60–0x822B3D28 to its true owner (likely wire
  `bandobj/BandIKEffector.cpp` from the rb3-Wii oracle + CharSignalApplier; note
  CharSignalApplier.cpp's own pin at `splits.txt:1011` is a suspicious 0x44-byte sliver —
  re-derive it too).
- Regenerate target map labels for both ranges; run the refill loop after
  (`tools/refill_loop.sh` honesty A/B — the current false unit holds ~25 "matched" fns,
  mostly anon funclets + ClassName, which the A/B must account for).

Est **+10 to +25** net (MidiInstrument TU compiles today and its template family already
byte-near-matches a *wrong* target; against the right target the named family + accessors
should land; some of the 169 auto-unit fns become reachable). Medium-high confidence the
mis-pin is real (multiple independent proofs); medium on net size.

Bonus reveals seen along the way: `fn_82700BE0` = `operator<<(BinStream&,
ObjRefConcrete<SynthSample>)` (callee in `?Save@SampleZone@@` 93.65, default/SampleZone).

---

## 4. LEVER C — AccomplishmentProgress per-TU `/DRB3_RBTREE_0x1C` — HIGH confidence (proven idiom)

**Evidence.** `??1AccomplishmentProgress@@UAA@XZ` 99.958: successive map-clear bases show
**stair-step** deltas — `addi r3, r30, 0x610` ours vs `0x614` retail (−4), `0x628` vs
`0x630` (−8), `0x640` vs `0x64c` (−12): each successive `_Rb_tree` member is 4 bytes
bigger in retail = the documented per-TU ODR split (`project_rbtree_4byte_deficit`,
AccomplishmentManager +28 precedent). Remaining bl diffs are ICF label noise
(`clear<_List_base<ObjectDir*>>` vs `clear<_List_base<GamerAwardStatus*>>` — retail
ICF-folded; may partially survive normalization).

**Fix.** `config/45410914/objects.json:680` — change
`"band3/meta_band/AccomplishmentProgress.cpp": "NonMatching"` to the object form with
`"extra_cflags": ["/DRB3_RBTREE_0x1C"]` (exact pattern already used elsewhere; see memory
`project_rbtree_4byte_deficit`). Zero blast radius (per-TU opt-in).

**Targets in-unit:** the dtor (may not fully flip if ICF bl-name diffs count — sibling
gains still apply), `??$LoadStdPtr@VGamerAwardStatus@...` 99.983/232B,
`??0GamerAwardStatus@@QAA@XZ` 71.0/108B, plus 6 anon 99.9 fns. Est **+1 to +3**.

---

## 5. LEVER D — WorldCrowd: drop `mCharForceLod` + `unkd0` (+8) — HIGH confidence

**Evidence.** `?Load@CamShotCrowd@@QAAXAAVBinStream@@@Z` (default/CameraShot, 99.971):
the inlined `mCrowd->GetModifyStamp()` reads `lwz r9, 0x98(r11)` ours vs **0x90 retail**
(+8). `WorldCrowd::mModifyStamp` is the LAST member; immediately before it our header has
`LODType mCharForceLod; int unkd0;` (`src/system/world/Crowd.h:149-151`) = exactly 8 bytes.
**rb3-Wii oracle (`../rb3/src/system/world/Crowd.h:131-132`): `mFocus` → `mModifyStamp`
directly, NO mCharForceLod/unkd0; `../rb3/src/system/world/Crowd.cpp` has zero
`char_force_lod` references.** Classic DC3-added-member (FileMerger `Merger::filler`
precedent `dc080dd`).

**Fix.** Gate both members + every use in `src/system/world/Crowd.cpp` (ctor init @110,
`SYNC_PROP(char_force_lod,...)` @200, `bs << mCharForceLod; bs << unkd0;` @228-229 — check
the surrounding rev-branch when removing from Save/Load @380-383, `COPY_MEMBER` @251-252,
LOD-apply logic @679-683, @1153-1157, @1338-1342). Use the `#ifdef`+`extra_cflags` idiom or
HX_NATIVE-style gate (native may want the DC3 behavior).

**Blast radius.** `world/Crowd.h` included by only 8 files; members used only in Crowd.cpp.
Also shrinks `sizeof(WorldCrowd)` by 8 (fixes `operator new` immediates in factory
funclets). Other diffs in CamShotCrowd::Load are name noise (unresolved
`fn_824AB710` = `?Load@ObjRefConcrete<WorldCrowd,ObjectDir>` — reveal candidate; ICF
`clear<vector<Vector2>>`-vs-`clear<vector<pair<int,int>>>` label).
Est **+1 to +4** (Load near-certain; Crowd.cpp ctor/Save/Copy currently fail on this too,
but they sit lower and may have additional issues).

---

## 6. LEVER E — CameraShot `_Destroy_Range` map-label SWAP (+2) — HIGH confidence

**Evidence.** A *mirrored* ±232 pair (the routing's +232/−232) across two template fns:

- map `0x824B0B90 = _Destroy_Range<CamShotCrowd>` but the retail fn there strides
  **0x108** (= our `sizeof(CamShotFrame)`) and `bl`s the dtor at 0x824ACBD0.
- map `0x824B2320 = _Destroy_Range<CamShotFrame>` but the retail fn there strides
  **0x20** (= our `sizeof(CamShotCrowd)`) and `bl`s unresolved `fn_824B05B8`.

The two entries in `scripts/target_symbol_map.json` are simply **swapped**.

**Fix (3 edits + A/B):**
1. Swap the names at `0x824B0B90` ↔ `0x824B2320`.
2. Add reveal `0x824B05B8 = ??1CamShotCrowd@@QAA@XZ` (it sits inside the CamShotCrowd
   sub-cluster, between `_Copy_Construct<CamShotCrowd>` @0x824B0548 and
   `??0CamShotCrowd(Hmx::Object*)` @0x824B0718; verify bytes vs our compiled dtor first).
3. The 0x108-stride fn's callee `0x824ACBD0` is currently labeled `??1DataPointMgr@@QAA@XZ`
   — but it sits mid-CamShotFrame-cluster (between `??4CamShotFrame` @0x824AC5C8 and
   `?UpdateTarget@CamShotFrame` @0x824ADC88); the label is a T4 ICF-ambiguous transfer
   (real DataPointMgr code lives near 0x827A7DD8). Relabel to `??1CamShotFrame@@QAA@XZ`
   after byte-check; grep other diffs for `??1DataPointMgr` references before renaming
   (whole-binary A/B is the gate).

Est **+2** (both _Destroy_Range), possibly +1–2 more from the two dtor reveals.
Mind `project_split_rule_mtime_loadbearing`: force resplit via
`rm target_symbol_renames.stamp` after map edits.

---

## 7. LEVER F — Synth −0xc member delta (recon-first) — MEDIUM confidence

**Evidence.** `??0Stream@@IAA@XZ` (default/Stream, 99.977): the ONLY real code diff is
`lwz r4, 0x74(r11)` ours vs **0x68 retail**, where r11 = `?TheSynth@@3PAVSynth@@A` →
`Synth::mMasterFader` (confirms the playbook §4 note: comment says 0x80, build says 0x74,
retail 0x68). Everything else is DATA_LBL noise (`lbl_82DA0017` vs named statics).

**Oracle.** rb3-Wii `../rb3/src/system/synth/Synth.h:154-156`: `mMuted` →
`ObjDirPtr unk40` → `mMasterFader` — **no `std::list<ObjectDir*> unk5c`** (our
`src/system/synth/Synth.h:171`, DC3-added, ZERO uses anywhere in our tree) and no
`mZombieInsts` either (but ours uses mZombieInsts at 6 sites in Synth.cpp, and Wii's
header is a 60%-decomp — don't trust its absences blindly).

**Fix path (bracket first, then gate).** −0xc is not explained by dropping one 8-byte list
alone — the agent must bracket the divergence point: pull `--offsets` on other
Synth-member-touching near-misses (Synth.cpp fns using `mZombieInsts` @lines 209/235/547+,
`mCommonBank`, `mMicClientMapper`) to find the first diverging member, then drop/gate
(candidates: `unk5c` list + 4 pad, or an ObjDirPtr-size divergence). Blast radius:
Synth.h included by synth TUs; members used only in Synth.cpp. Est **+1 to +3**.

---

## 8. Parked items — evidence on file (do not re-grind)

- **RndEnvironTracker ctor+dtor** (BandCharacter, 99.984/99.975): the real diff is
  `lwz r11, 0x0(r30)` → `lwz r11, 0x54(r11)` retail vs `0x4(r11)` ours → `mtctr/bctrl` =
  **virtual-slot delta of exactly 20 slots (−0x50)** on an RndEnviron vcall = the
  documented RndEnviron secondary-base/vbase vtable wall (roadmap "Deferred from this
  lever"). Rest is DATA_LBL noise (`RndEnviron::sCurrent/sCurrentPos/sCurrentPosSet` all
  anchor to one `lbl_82C8ED88`). **Classifier gap:** wall_classify's vtable-slot gate
  missed it because the `lwz 0x0` vtable load is 3+ instructions before the slot load with
  interleaved `mr`s — gate needs simple def-use tracking, not adjacency.
- **TrainerGemTab::DrawStartFinish** (99.923, delta +60 = 15 slots × 4): the documented
  UILabel/UIComponent-MI 15-slot delta (roadmap; `docs/plans/ui-base-layout-reconstruction.md`).
- **NgMat::SetupShader** (99.906, −0x3c) **and `?Select@RndShaderSimple@@` (Shader,
  99.962)**: Select's deltas are `lwz/stw 0x1c4` ours vs `0x188` retail (+0x3c) and
  `0xa4` vs `0x3c` (+0x68) on a RndMat* — i.e. **Select is a second validation function
  for the Mat_NG scrambled-layout reconstruction** (`docs/decomp/matng-deferral.md`).
  Add it to that doc's validation set; do not fix standalone.
- **Locale `LocalizeFloat`/`LocalizeSeparatedInt`** (99.952/99.992): 100% of diffs are
  data-label/sym-name (`lbl_82DA0017` vs local statics `?sSep@?1??...`, `_snprintf` vs
  `?Hx_snprintf`, unresolved `fn_8227CAA0` = `MakeString<int>`). Zero code diffs.
  **DATA_LBL_ONLY class** — fixable only by objdiff-fork reloc-name normalization (the
  known FP-anchor gap) — LBL renaming itself is REFUTED (memory). Two reveal-ish nibbles
  (`fn_8227CAA0`) won't flip them alone.
- **`yylex`** (DataFlex, 99.958): real diffs are flex DFA TABLE deltas — retail `yy_chk`
  base 0xc40 vs ours 0xc00 (−0x40), `yy_def` +8, `yy_nxt` +16, state-count compare
  `cmplwi 0x206` vs `0x1db` (retail has 43 more DFA states), boundary `0x7a` vs `0x7b`.
  Retail's `.l` lexer source had extra/changed rules. **GENERATED_CODE class** — needs the
  original lexer spec + same flex version; deep, single fn, PARK.
- **Singer::GetFrameMatchType** (99.909): `*(this+0)` (VocalPlayer*) → `lwz 0x394` ours vs
  `0x390` retail = VocalPlayer-layout +4 upstream of `mVocalParts`. Our VocalPlayer.h is a
  near-verbatim rb3-Wii port (both comment `mVocalParts // 0x358` — stale), so the +4 is
  NOT visible by source diff; Player/Performer carry vbase MI (§3a pilot). Optional
  bounded recon: bracket the +4 via other VocalPlayer-offset near-misses; if it brackets
  into Performer/Player base → vbase wall, park for good.
- **BandCamShot `Target::UpdateTarget`** (99.962): one real codegen diff — retail reloads
  the Symbol temp from the stack slot (`lwz r11, 0x50(r1)`) where ours reads through the
  ctor's returned `this` (`lwz r11, 0x0(r3)`) — try a named-local source shape; plus map
  label `?Store@Target@HamCamShot@@` (DC3 "Ham" name) needs relabel to BandCamShot.
  Single fn, two coupled fixes, LOW priority.

## 9. Classifier feedback (feed wall_classify v3)

1. **SWAPPED_MAP_LABELS**: mirrored ±N dominant deltas across TWO same-shape template fns
   in one unit, where N = |sizeof(T1)−sizeof(T2)| of the two instantiations and each side's
   `bl` callee belongs to the *other* class → target_symbol_map swap, not a member delta.
2. **UNIT_MISPIN / MISLABELED_TRANSFER**: template-fn stride (`mulli`/`li` feeding a ptr
   walk) ≠ our `sizeof(T)` while non-stride bytes match → check the unit for foreign named
   symbols (here: BandIKEffector inside "MidiInstrument") before trusting ANY label in it.
3. **RBTREE stair-step**: monotone −4/−8/−12 deltas on successive container bases in one
   fn → route to RBTREE_0x1C per-TU flag, not MEMBER_DELTA.
4. **vtable-slot gate def-use**: track the slot-load base register to its `lwz rX, 0x0(rY)`
   def across interleaved instructions (RndEnvironTracker escaped the v2 gate).
5. **DATA_LBL_ONLY**: 100% of diff_args are sym-class on data labels/ICF names → tooling
   bucket (objdiff normalization), never a source fix.

## 10. Verification protocol (every lever)

Standard playbook: worktree via `scripts/setup_worktree.sh`, baseline
`measures.matched_functions`, apply ONE lever, full rebuild (`ninja -j 12` on fresh CoW),
net delta + zero-regression gate (`tools/ab_measure.py` / `tools/refill_loop.sh` for the
re-pin), commit path-limited. Rank by report `match_percent_normalized` only.
