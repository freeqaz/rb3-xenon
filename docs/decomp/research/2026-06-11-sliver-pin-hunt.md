# Sliver/Over/Displaced Pin Hunt — Binary-Wide Worklist (2026-06-11)

**Baseline:** main @ `e011140`, 7027 matched (report.json `measures.matched_functions`, fresh).
**Scope:** all 628 pinned units in `config/45410914/splits.txt`, cross-referenced against
589 compiled objs (`build/45410914/src/**`), `scripts/target_symbol_map.json` (12.2k VA-keyed
entries), and `build/45410914/report.json` per-unit measures/functions.
**Motivation:** wave-3 went 3-for-3 above estimate on this bug class
(UIComponent re-pin +38 vs est +5–9; AsyncFileHolmes/MusicLibrary boundary +19; MidiParser
tail +5). This doc is the systematic binary-wide sweep for the same three signatures.

Calibration prior from those precedents: **realized gain ≈ 0.3–0.5 × (real fns newly
covered by a correct pin)**, when the source is already compiled and map entries exist.

---

## 1. Method

Per pinned unit, computed (script rebuildable from this doc; see §6):

- **pin** `[text_lo, text_hi)` from splits.txt (`map_lint.parse_splits`).
- **report join** by unit basename + `metadata.source_path` disambiguation (39 units have
  pathed report names like `band3/game/VocalPlayer`; naive basename join silently zeroes them).
- **compiled obj stats** (COFF LE parse): total `.text*` raw size, defined fn symbols
  (storage class 2/3, type DT_FCN, noise-filtered `__unwind$`/`__ehfuncinfo$`/`__catch`),
  and the **own-class** subset (classes from file stem ± `_Xbox/_NG/_Win` suffix, matched via
  `map_lint.mangled_classes`).
- **map placement:** own-class `target_symbol_map.json` entries inside vs outside the pin;
  outside entries clustered (gap ≤ 0x3000) and each cluster's owner resolved (another pin
  vs **AUTO-BLOB** = unsplit `auto_03` real estate).
- **boundary runs:** head/tail 0%-fn runs from the report unit's `functions[]` array.

### Detector definitions

| Sig | Smell | Computed as |
|---|---|---|
| **A sliver/under-pin** | pin ≪ compiled own content | `obj_own_text > 2×pin_size + 0x800` |
| **B over-pin** | pin ≫ compiled total content | `pin_size > 1.5×obj_total_text + 0x1000` |
| **C displaced** | own-class map entries cluster OUTSIDE the pin | ≥4-entry cluster, span <0x10000 |

### False-positive filters learned during this sweep (IMPORTANT — bake into any tool)

1. **Free-function TUs defeat signature A/B own-class metrics.** `FFT.cpp` looked like a
   max-grade over-pin (pin 0x29F4 vs obj 0x940, 0/12 matched) — but the in-range map census
   shows all 12 targets are FFT.cpp's *own free functions* (`fft_altivec`, `fft_scalar`,
   `FFTComplex`, `CalculateSinCosTable`…). The pin is **correct**; the deficit is an
   unported VMX body. Same partial story for `MemHeap` (own `Mem*` free fns) and
   `keygen_xbox` (own `mash/roll/shuffle*` free fns). **Verdict requires a map-census of the
   range, not just byte arithmetic.**
2. **`mangled_classes` extracts argument/template classes too** → a unit "owning" a class
   that merely appears in parameter lists. E.g. 17 "Anim" entries inside EventTrigger's pin
   are EventTrigger methods taking `Anim*`/RndAnimatable templates, not a displaced Anim TU.
3. **Inline-scatter classes** (Symbol, DataArray, DataNode, BinStream, Object-ctors): hundreds
   of out-of-pin entries are legit per-TU inline/template copies *already matched in their
   host units*. Only tight, *substantive-method* clusters count (the dumps in §2 were
   eyeball-verified to contain real bodies: `?Selected@UIList@@`, `?GetGems@SongDB@@`,
   `?SetFocusInterest@CharEyes@@`…).
4. **Suffix-strip artifacts:** `Rnd_NG`/`Rnd_Xbox` "displaced clusters" are just class `Rnd`
   entries inside `Rnd.cpp`'s own pin. Same for MemcardMgr→MemcardMgr_Xbox,
   AsyncFile_Win→AsyncFile. Platform-split TUs legitimately define the base class.
5. **DC3-only classes** (Ham\*, Move\*, RhythmBattle\*, HollaBack\*, GestureMgr,
   SkeletonClip, PoseFatalities…): huge compiled objs (dc3 sources wired for engine reuse)
   with deliberate one-off content-transfer pins. **No in-binary map cluster ⇒ no evidence a
   real TU exists in RB3 ⇒ not candidates.** (HamDirector own_text 0x1279C tops the raw A
   list — skip it.)
6. **Stub-farm mirage:** the Accomplishment (20 fns / 0x468) and AccomplishmentProgress
   (14 fns / 0x1C0) AUTO-BLOB clusters at 0x82439E34–0x8243A5F4 are uniform ~0x20–0x38-byte
   bodies — the retail coverage-breadcrumb stub farm (`project_game_code_instrumentation`).
   Pinning them will read 0% against our real accessor bodies. LOW priority despite clean
   clustering.
7. **Duplicate split unit names** (`CubeTex`, `FxSendReverb`, `FxSendCompress`,
   `FxSendPitchShift` each appear twice — synth vs synth_xbox): any tooling keyed by unit
   basename mis-joins these. Use src-path keys.

### Raw signature counts

| Signature | Raw hits | Genuine after filters |
|---|---|---|
| A sliver/under-pin | 169 | ~30 actionable (rest: DC3-only, free-fn TUs, RockCentral-style template surplus) |
| B over-pin | 10 | 6 carves (TexBlender, StreamNull, UIGuide, MemHeap, Mic, CharEyeDartRuleset) + 1 sparse-scatter (AsyncFileHolmes residual) + 2 reclassified (FFT, GuitarController = port-depth, correct pins) |
| C displaced w/ located cluster | 95 | ~40 genuine (overlaps A; the actionable core of this doc) |

---

## 2. TIER 1 — ranked candidates (full evidence)

EV = expected matched-fn gain, calibrated on the wave-3 precedents. "AUTO-BLOB" means the
target range is currently unsplit (free real estate — the UIComponent case, lowest-risk
move). All `mf/tf` from the current report.

### 1. UIList — relocate sliver to its real TU next to UIComponent. EV **+15–30**. Opus-light.
- Current pin: `0x82559D10–0x82559D68` (0x58, 1 fn, the classic ICF-displaced
  `StaticClassName` one-off; mf 1/1).
- Real cluster: **0x827D2998–0x827D86F8+** (AUTO-BLOB), 16 map entries incl. substantive
  bodies (`Selected`, `NumProviderData`, `AutoScroll`, `Refresh`, `CollidePlane`,
  `OnMsg(ButtonDownMsg)`, `OnSetData`, `FinishValueChange`, `BoundingBoxTriangles`) + own
  STL instantiations (`TrackConfig`, map<int,float> helpers).
- Fit: compiled `UIList.obj` own content 0x6118 / 90 own fns ≈ cluster span 0x5D60 — near-perfect.
- Adjacency: sits in the unpinned gap between **UIScreen** (hi 0x827CC520) and
  **UIComponent** (lo 0x827D8DC8). Propose pin `[0x827D2998, 0x827D8DC8)` (extend-to-next-pin
  upward is clean; head boundary needs a quick .s recon — 0x6478 of other UI TUs sits below).
- This is the *exact* UIComponent-class case (same neighborhood, same mechanics, bigger obj).
  Map entries already exist for the named bodies; gen the rest via `tools/gen_game_target_map.py`
  conventions + `map_lint --check obj_orphan`.

### 2. UIListDir region survey — UILabel/UIListWidget/UIListState carve. EV **+15–30** combined. Opus.
- **UILabel** sliver pin `0x82339998` (0x5C, 0/1) but 10 substantive UILabel fns at
  **0x827E5A18–0x827E7F78** — *interleaved with the UIListDir pin* `0x827E57A0–0x827E77A8`
  (mf 41/74) and spilling into the unpinned 0x12F8 gap above it. UILabel.obj: **64 own fns /
  0x451C** — the largest unharvested UI obj.
- **UIListWidget** sliver `0x827FBA80` (0x88, 1/3); 11 own fns at 0x827E57A0–0x827E85E0.
- **UIListState** pin `0x827E8AA0–0x827E9678` (16/21) already in-region; 13 own fns spread
  0x827E5C28–0x827E9C58 (9 unpinned, rest in UIListDir's pin).
- Reading: the region `UISlider(0x827E4368)…UIButton(0x827EA3B8)` holds UIListDir + UILabel +
  UIListWidget (+UIListState overflow) TUs, but only UIListDir/UIListState have real pins, and
  UIListDir's boundary swallows its neighbors' heads. /O1 grouping says the TUs are contiguous —
  one .s/vtable recon pass can draw all four boundaries at once.
- Risk: UIListDir's 41 existing matches must not regress — boundary moves only transfer
  attribution, but A/B per boundary. NOT a Sonnet job.

### 3. Object.cpp — Dir/DirLoader/Object triple boundary. EV **+10–25**. Opus (regression-sensitive).
- Object pin: `0x82737FE8–0x82738160` (0x178, **0/2**) — a sliver for the engine-core
  `Hmx::Object` TU. Compiled `Object.obj`: **146 own fns / 0x4D34**.
- 13 substantive Object-class entries at **0x82730FA0–0x82738458**: 11 inside DirLoader's pin
  (`0x8272FF10–0x82737FE8`, mf 93/233), 2 *above* Object's pin hi (unpinned).
- Reading: the real Object.cpp TU plausibly spans ~0x82730FA0 → 0x82738458+; DirLoader's pin
  over-extends into it, and Object's pin only caught the tail sliver. Note the obj-orphan
  worklist classified DirLoader's Object/Symbol/DataNode orphans as CLEANUP-SAFE
  ("STL-attributed") — **this sweep's evidence (cluster adjacency + Object's own sliver pin
  abutting it) contradicts that verdict; re-examine before any map cleanup deletes them.**
- Action: per-fn .s recon of 0x82730FA0–0x82738458, then shared-boundary move
  DirLoader.hi ↓ / Object.lo ↓ + Object.hi ↑ (next pin FlowEventListener @0x827418D0 leaves
  0x9770 headroom). DirLoader's 93 matches are the regression watch.

### 4. CharEyes — relocate sliver to full TU. EV **+8–16**. Sonnet-able (with honesty gate).
- Current pin `0x82780AF0–0x82780B44` (0x54, 0/1). Real cluster **0x82371018–0x82377138**
  (AUTO-BLOB; 38 map entries; substantive: `SetFocusInterest`, `EyesOnTarget`, `Enter`,
  `UpdateOverlay`, `Save`, `PropSync(EyeDesc)`, EyeDesc/CharInterestState STL family).
- Fit: obj own 0x5774 / 47 fns ≈ cluster span 0x6120. Oracle: rb3-Wii + DC3 both have
  `char/CharEyes.cpp`; source already compiles.
- Adjacency: unpinned gap CharClip.hi (0x8236E060) → Morph.lo (0x82377FC8) = 0x9F68.
  Propose `[0x82371018, 0x82377FC8)` (extend-to-next-pin), revert if foreign-run gate fails.
  (0x2FB8 below cluster-lo is some other char TU — acceptable inside the gate, or trim lo to
  first own fn.)

### 5. SongDB — relocate; current pin looks like fuzzy-scatter junk. EV **+8–18**. Opus.
- Current pin `0x8274BB34–0x8274E870` (mf 13/114, fuzzy 8.2): census of its map entries =
  HamMoveKey, HamListRibbonDrawState, XUSER_ACHIEVEMENT, Unlockable, RndPointTest… only 2
  SongDB entries. This pin is a graveyard of one-off content-transferred fns, not the TU.
- Real cluster: **0x82666FE0–0x8266A420** (AUTO-BLOB; 30 map entries) with real scoring
  bodies: `GetGems`, `GetMBT`, `GetCommonPhraseTracks`, `SetupPhrases`, `RebuildPhrases`,
  `ClearTrackPhrases`, plus SongDB's own template families (RecordedFrame, PracticeSection,
  MultiplayerAnalyzer::Data, StepMoves) — consistent single-TU signature.
- Adjacency: unpinned gap ClipGraphGen.hi (0x82666F0C) → GameMode.lo (0x82671A60) = 0xAB54.
- Fit: obj total 0x77BC / 67 own fns. rb3-Wii `band3/game/SongDB.cpp` is the oracle.
- Risk: relocating drops up to 13 currently-matched scatter fns (judge net via whole-binary
  A/B). Opus should check what those 13 are before moving (some may be re-pinnable one-offs).

### 6. VocalTrack — extend DOWN 0x4C78 + Gem-tail boundary. EV **+8–16**. Opus-light.
- Pin `0x82B727B8–0x82B7A2A0` (59/148). 22 own-class map fns at **0x82B6D688–0x82B72300**,
  all AUTO-BLOB, immediately below pin lo (prev pin GemTrack.hi = 0x82B64D38, so 0xDA80 of
  unpinned headroom).
- Action (two independent edits): (a) move pin lo ↓ to ≤0x82B6D688; (b) the known Gem-orphan
  tail (`Gem::OnScreen`, `~Gem` at 0x82B78F10–0x82B79008; Gem pin lo = VT hi = 0x82B7A2A0
  adjacent) — per-fn check of 0x82B79008–0x82B7A2A0, then shared-boundary VT.hi ↓ / Gem.lo ↓
  to 0x82B78F10.

### 7. CameraManager — relocate sliver. EV **+6–12**. Sonnet.
- Pin `0x826E20E8–0x826E2170` (0x88, 0/1). Cluster **0x824A6D08–0x824A9AC0** (AUTO-BLOB,
  **32 map entries in span**, 9 own-class). Obj own 0x3AC4 / 45 fns ≈ span 0x2DB8.
  Oracle: rb3-Wii + DC3 `world/CameraManager.cpp`. Neighbors: HamMaster pin below,
  CameraShot pin above — pin the gap slice covering the cluster.

### 8. TexBlender — carve out AmbientOcclusion. EV **+6–14**. Opus.
- TexBlender pin `0x82477160–0x824822D8` (0xB178!) vs obj 0x58DC — biggest genuine over-pin.
- In-range census: RndTexBlendController(15), BlendSorter(12), **RndAmbientOcclusion(12)**
  (VAs 0x82479B60–0x82480548), vectorSort helpers(13), kdTreeNode(4) — the AO/mesh-sort
  machinery of `rndobj/AmbientOcclusion.cpp`, which is **already compiled**
  (`build/.../rndobj/AmbientOcclusion.obj`, 384KB) but pinned to a 0x80 sliver at
  0x822C2610 (0/1).
- Caveat: AO VAs span the *middle* of TexBlender's pin and TexBlendController has its own
  unit too — needs .s recon to draw 2–3 boundaries (TexBlender | AO | ?TexBlendController).
  Not mechanical.

### 9. SongMgr — relocate sliver; evict MovieSys sliver squatting on its head. EV **+6–12**. Opus-light.
- SongMgr pin `0x824E6220–0x824E6274` (0x54, 0/1). Cluster **0x827839C8–0x82785070**
  (10 own fns, 9 AUTO-BLOB) — and the **MovieSys** sliver pin (0x827839C8–0x82783A00, 0/1)
  sits exactly on the cluster head. Obj own 48 fns. Compound: relocate/park MovieSys's
  sliver (find MovieSys's real home near BinkMovieImpl 0x82785668 first, or drop to one-off),
  then pin SongMgr `[0x827839C8, 0x82785070+]`. Ceiling: BinkMovieImpl.lo 0x82785668.

### 10. MemHeap/Str — shared-boundary move. EV **+4–9**. Opus-light.
- MemHeap pin `0x82796440–0x827989F0` (5/62, 58-fn zero tail) directly abuts Str
  (`0x827989F0–0x82799A00`, 33/48). MemHeap head is genuinely its own (Mem* free fns +
  MemHeap methods), but the tail census shows String(4)/FixedString(2)/MakeString — Str.cpp
  classes. Find the first String-run VA in MemHeap's .s, move the shared boundary down.
  Str.obj's String/FixedString coverage is historically strong.

### 11. Waypoint — relocate sliver. EV **+5–10**. Sonnet.
- Pin `0x822C8CA8–0x822C8CF8` (0x50, 0/1). Cluster **0x823C7CC8–0x823CA598** (9 own,
  13 map total, AUTO-BLOB; gap CharBonesBlender.hi 0x823C67B8 → FitnessFilter.lo 0x823CA668).
  Obj own 0x3434 / 140 own-fn symbols (ObjPtr templates included). Oracle: rb3-Wii
  `char/Waypoint.cpp`. Propose `[0x823C7CC8, 0x823CA668)`.

### 12. Character + TypeProps compound — two relocations, one region. EV **+6–12**. Opus.
- **TypeProps** sliver `0x8235B0F0–0x8235B138` (0/1) sits at the head of the **Character**
  cluster (**0x8235B1D0–0x8235CCA0**, 7 own + 17 map total, AUTO-BLOB). TypeProps's own real
  cluster is at **0x82740E40–0x82741A08** (4 fns, AUTO-BLOB, obj 16 own fns) near
  Object/SkeletonDir.
- Sequence: relocate TypeProps → 0x82740E40 region; then pin Character
  `[0x8235B0F0ish, 0x8235F180)` (next pin CharBlendBone; span 0x3FB0 ≈ obj own 0x4D14).
  Character.cpp is the shared engine base — watch the known Character-layout history but the
  TU itself was never properly pinned.

### 13. StreamNull — carve MoggClipMap (boundary-fit is elegant). EV **+4–9**. Opus-light.
- StreamNull pin `0x826FBD98–0x82700E18` (0x5080) vs obj 0xDA0; 95-fn zero tail.
- MoggClipMap's 12 own fns sit at **0x826FC900–0x826FF108** inside it; StreamNull's head
  `0x826FBD98–0x826FC900` = 0xD68 ≈ obj 0xDA0 (near-exact fit). MoggClipMap.obj compiled
  (22.5KB; own coverage thin — 8 fns — gains capped until synth port deepens).
- Tail 0x826FF108–0x82700E18: junky census (CamShotFrame×2, Sfx, resize) — leave for recon;
  Sfx/SfxMap have their own (sliver) pins elsewhere; `Sfx.obj` is NOT compiled.
- Head caveat: head census also shows SfxInst(3)/FaderGroup/ADSRImpl orphans — purge via
  obj_orphan after the boundary lands.

### 14. UIGuide carve + LabelNumberTicker relocate. EV **+5–10**. Opus-light.
- UIGuide pin `0x82801070–0x82804770` (19/166, 72-fn zero tail) vs obj 0xCF4. Head
  0x82801070–0x82802F30 is genuinely UIGuide (4 own entries). LNT's 6 substantive fns at
  **0x82802F30–0x828036E8** (LNT pin today: 0x58 sliver @0x82582F78; LNT.obj compiled, 34
  own fns / 0x1744). Tail 0x82803750–0x82804770 = EH-runtime/CRT helpers (`?Run@App`,
  `_GetEstablisherFrame`…) → return to auto-blob.
- Action: UIGuide.hi ↓ 0x82802F30; relocate LabelNumberTicker `[0x82802F30, ~0x82803750)`;
  drop the orphan-doc's "delete the 6 LNT entries from UIGuide" idea — they become LNT's
  pairing map instead. (Supersedes that cleanup item.)

### 15. Spotlight triple — SpotlightDrawer_NG / SpotlightDrawer / Spotlight boundaries. EV **+5–10**. Opus.
- Pins: _NG `0x824BE7A0–0x824C19F8` (26/55) | Drawer `0x824C1C38–0x824C4D58` (44/82) |
  Spotlight `0x824C4ED8–0x824CCEC0` (128/203).
- 10 _NG own fns sit inside Drawer's pin (0x824C19F8–0x824C4C98); 7 Spotlight own fns sit
  inside both Drawer pins (0x824BE880–0x824C4E18). The three internal boundaries are drawn
  wrong (and there are small unpinned slivers between the pins: 0x824C19F8–0x824C1C38,
  0x824C4D58–0x824C4ED8). One .s recon pass over 0x824BE7A0–0x824C4ED8 redraws all three.

### 16–20. Extension micro-batch (Sonnet, wave-1 recipe + REVERT honesty gate)

| Unit | Pin | Evidence | Action | EV |
|---|---|---|---|---|
| **Stats** (band3/game) | `0x82678FF8–0x8267A138` (15/38) | 10 own fns 0x8267A270–0x8267B750 AUTO-BLOB just past hi (next pin nuidetroit 0x8267E6E0) | extend hi ↑ ~0x1700 | +4–8 |
| **FileMerger** | `0x82381AE8–0x82382A88` (29/39) | 13 own fns 0x8237E778–0x82382BB0 bracket the pin both sides (prev HAQManager.hi 0x8237DEA4) | extend lo ↓ + hi ↑ | +4–8 |
| **CharHair** | `0x82394620–0x82397610` (67/107) | 7 own fns 0x82391B80–0x82394370 below lo (prev CharBoneOffset.hi 0x82391A28) | extend lo ↓ | +3–7 |
| **Song** | `0x827A0820–0x827A1708` (29/35) | 5 own fns 0x827A0660–0x827A2D08 both sides; prev BinStream.hi 0x827A0424 | extend lo ↓ + hi ↑ (next StringTable 0x827A3A08) | +2–5 |
| **MusicLibrary** | `0x82527920–0x8252CD38` (89/160) | 4 own fns 0x8252CEF0–0x8252E5A0 past hi, AUTO-BLOB | extend hi ↑ | +2–4 |
| **ButtonHolder** | `0x82793998–0x82793A24` (1/1) | 5 own fns 0x82793A28–0x82794A78 immediately past hi (next MetaMusicScene 0x82794F10) | extend hi ↑ to 0x82794F10 | +2–4 |
| **CharClip** | `0x8236A918–0x8236E060` (73/103) | 4 own fns 0x8236A4B8–0x8236A8C0 just below lo, AUTO-BLOB | extend lo ↓ 0x460 | +2–4 |

### 21–30. Small relocations (Sonnet batch — all AUTO-BLOB targets, compiled objs exist)

| Unit | Sliver pin | Real cluster (AUTO-BLOB) | Obj own | EV |
|---|---|---|---|---|
| **UIColor** | 0x827DCA50 (0xFC, 2/3) | 0x827FD068–0x827FD868 (10 fns, span 0x800) | 13 fns | +4–8 |
| **UITrigger** | 0x826EEF28 (0x70, 0/1) | 0x827FFC70–0x82800428 (7 fns; below UIGuide.lo) | 29 fns | +4–8 |
| **UIListSlot** | 0x82B605A0 (0x98, 0/1) | 0x827EFB08–0x827F06E8 (8 fns; UIListMesh→UIPicture gap) | 23 fns | +3–6 |
| **CharIKFingers** | 0x822FE498 (0x44, 0/1) | 0x8239E5A0–0x823A06B0 (7 fns) | 28 fns | +3–6 |
| **CharIKHand** | 0x823188A8 (0x44, 0/1) | 4 out-fns (cluster small — verify span in .s first) | 25 fns | +2–5 |
| **HeldButtonPanel** | 0x8261C5C8 (0x44, 0/1) | 0x82788338–0x827894D8 (4 fns) | 13 fns | +3–6 |
| **LightPresetManager** | 0x82596468 (0x58, 0/1) | 0x824A59D8–0x824A5F20 (5 fns) | 14 fns | +3–6 |
| **CharMeshHide** | 0x822C1E58 (0x44, 0/1) | 0x8238E770–0x8238FC68 (4 fns) | 14 fns | +2–5 |
| **SongMetadata** | 0x82785848 (0x448, 0/1) | 0x826C1928–0x826C2440 (4 fns; below VocalPlayer) | 12 fns | +2–5 |
| **UIListWidget** | (see #2 — fold into region survey) | | | |

Sonnet recipe per relocation: re-pin in splits.txt → `rm build/45410914/target_symbol_renames.stamp`
→ `touch config/45410914/config.yml` → build → `tools/fresh_report.sh` → keep iff
matched_functions strictly increases AND no ≥8-contiguous-foreign-0% run in the new range
(wave-1 honesty gate); else revert. Map entries for newly covered named fns may need
generating (gen_game_target_map conventions for game TUs; DC3 transfer names for engine).

### 31–35. Boundary micro-trims (Sonnet, from obj-orphan worklist, re-confirmed here)

| Pair | Shared boundary move | EV |
|---|---|---|
| MidiSynth/StreamNull | 0x826FBD98 → 0x826FBD28 (2 StreamNull fns at MidiSynth tail) | +1–2 |
| Task/DataNode | 0x82725DA8 → 0x82725988 (DataNode ctors at Task tail; Task.obj defines none) | +2–3 |
| UI/PanelDir | 0x827E1458 → 0x827E0ED0 (PanelDir::EnableComponent/DrawShowing/Save + 4 UIComponent-class fns at UI tail — per-fn check the UIComponent ones, may be ICF twins) | +2–5 |
| keygen_xbox/ByteGrinder | 0x827091A8 → 0x827090A8 (2 ByteGrinder fns at keygen tail; pins adjacent) | +1–2 |
| NetCacheMgr/DataPointMgr | NetCacheMgr's 6 own fns at 0x827A83D8–0x827A8940 inside DataPointMgr's pin (pins nearby) — boundary recon then shift | +2–5 |

---

## 3. INVESTIGATE / DEFERRED (needs recon or gated on porting)

- **AsyncFileHolmes residual** (`0x825221D0–0x82527920`, 0x5750, 7/212, obj 0x29C): the
  remaining over-pin after the wave-3 MusicLibrary fix. Map census of the range is *sparse
  scatter* (singleton RndSoftParticleBuffer, DOFProc, MetaPerformer, CurrentScreenChangedMsg…)
  — not one coherent foreign TU; likely many small TUs/one-offs. Needs
  fingerprint/Ghidra-driven identification, not a boundary move. tf=212 small fns. High
  effort, unclear EV — defer behind Tier 1.
- **GuitarController** (15/166, pin 0x5900 vs obj 0x2ADC): NOT an over-pin — in-range census
  is GuitarController's own + stale DC3-NUI orphans (FacePipelineDetect etc. — purge those
  map entries). The deficit is port depth + the known static-Symbol/stub walls. Reclassified
  out of signature B.
- **FFT** (0/12): pin CORRECT (own free fns); needs VMX body porting. Removed from B.
- **Mic** (28/110, pin 0x36B8 vs obj 0x21E4): MicXbox(7)/MicManagerXbox(3)/ChatBuffer(6)
  entries in-range; no `Mic_Xbox.obj` exists (unit src is `synth_xbox/Mic.cpp`). Either those
  classes belong to an unwired TU inside the pin (carve+wire) or to Mic.cpp itself under
  different file split in DC3. Opus recon.
- **CharEyeDartRuleset** (28/74, pin 0x2998 vs obj 0x158C): CharInterest(3) entries in-range;
  CharInterest itself is a 0x50 sliver at 0x8229D760. Likely CharInterest TU inside — small
  carve, Opus-light. EV +2–5.
- **Synth-region relocate+port campaign** (gated on synth porting, relocation alone ≈ +0):
  `Stream` real TU at **0x82909D20–0x8290EAA8** (24 own map fns, 88 total entries in span,
  AUTO-BLOB) but Stream.obj is 0x2C4 of stubs; `Sequence` (8 fns @0x826E7A18–0x826E95D0,
  no obj), `StandardStream` (14 fns @0x826E3B20–0x826E7000, no obj), `Synth` (7 fns
  @0x826DEAB0–0x826E3460), `MoggClip` (7 fns @0x826EF9D8–0x826F0DA8, no obj). The whole
  0x826DE000–0x826F1000 belt is nearly unpinned and identified — a future synth wave should
  pin+port together. Current relocation EV ≈ 0 (nothing compiled to pair).
- **Accomplishment / AccomplishmentProgress stub farms** (0x82439E34–0x8243A5F4): mirage-risk
  (filter #6). Only worth it if someone verifies the bodies aren't breadcrumb stubs.
- **VocalPlayer / BandWardrobe / VocalTrackDir / Player / OvershellSlot under-pins**
  (A-signature, no out-of-pin map evidence — TU may continue into the gap after pin hi):
  VocalPlayer (own 0x79C8 vs pin 0x291C, gap-next 0x752C), BandWardrobe (0x5B98 vs 0x18D0,
  gap 0x4B48), VocalTrackDir (0x8A34 vs 0x42FC, gap 0xA9EC), Player (0x5270 vs 0x2860, gap
  0x5858 — the +4-layout wall caps match-rate), OvershellSlot (0xB6E8 vs 0x296C — 8-byte
  layout wall, defer). Run as blind extensions with the honesty gate; EV +2–6 each,
  uncertain.
- **PropKeys second cluster** (7 fns @0x8240ED70–0x82410808 AUTO-BLOB next to Poll/PropAnim,
  pin elsewhere at 0x82649C38 with 17/51): two PropKeys-ish TUs (obj/ vs rndobj/?) — Opus
  recon before touching; the existing 17 matches must not move.
- **PlatformMgr secondary cluster** (6 own fns @0x82508EC0–0x82509570, far from pin): can't
  dual-pin one unit; likely platform-callback block — recon only.
- **Anim-in-EventTrigger (17 entries)**: argument-class FP (filter #2). No action.

---

## 4. Top-10 summary (by EV)

| # | Candidate | Sig | Action | EV | Agent |
|---|---|---|---|---|---|
| 1 | UIList → 0x827D2998 | A+C | relocate (AUTO-BLOB, abuts UIComponent) | +15–30 | Opus-light |
| 2 | UIListDir/UILabel/UIListWidget/UIListState region | B+C | multi-boundary survey 0x827E4368–0x827EA3B8 | +15–30 | Opus |
| 3 | Object/DirLoader/Dir | A+B+C | triple boundary; Object TU ≈ 0x82730FA0–0x82738458 | +10–25 | Opus |
| 4 | CharEyes → 0x82371018 | A+C | relocate into CharClip→Morph gap | +8–16 | Sonnet |
| 5 | SongDB → 0x82666FE0 | C | relocate; current pin = scatter junk | +8–18 | Opus |
| 6 | VocalTrack lo↓ + Gem tail | A+C | extend-down 0x4C78 + shared boundary | +8–16 | Opus-light |
| 7 | CameraManager → 0x824A6D08 | A+C | relocate | +6–12 | Sonnet |
| 8 | TexBlender / AmbientOcclusion carve | B | redraw 2–3 boundaries; AO.obj already compiled | +6–14 | Opus |
| 9 | SongMgr → 0x827839C8 (+ MovieSys eviction) | A+C | relocate compound | +6–12 | Opus-light |
| 10 | Character + TypeProps compound | A+C | two relocations, ordered | +6–12 | Opus |

Tier-1 totals (items 1–15): **≈ +110–210**. Sonnet micro-batches (16–35): **≈ +35–70**.
Combined ceiling of this vein ≈ **+145–280** — consistent with wave-3's "systematically
undervalued" finding.

---

## 5. Batch tool vs per-unit agents

**Detection: yes, tool it.** This entire dossier is mechanical given the four inputs
(splits, report, compiled-obj COFF, target map). Recommend landing it as
`tools/pin_audit.py` with the §1 detectors and ALL SEVEN FP filters (free-fn TUs via
in-range map census, arg-class extraction, inline-scatter classes, suffix-strip, DC3-only
gating via oracle-presence check, stub-farm size profile, dup-name src-path keys). Output:
the §2/§3 tables as JSON. Re-run after each landing wave — relocations change adjacency for
the next ones.

**Application: per-unit agents, not a blind batch.** Unlike the wave-1 extension vein
(append-only, one legal move), these edits relocate pins and move shared boundaries —
each needs (a) map entries for the new range, (b) the renames-stamp purge, (c) whole-binary
A/B with the honesty gate, (d) per-fn .s checks at contested boundaries. Sonnet can run the
clearly-bounded AUTO-BLOB relocations (#4, #7, 11, 16–30, 31–35) one-at-a-time with the gate;
Opus should take #1–3, 5, 8, 12, 15 (boundary recon, regression watch on the donor unit's
existing matches).

**pdata note:** all proposed moves are whole-pin relocations, extensions into unpinned space,
or shared-boundary shifts between adjacent pins — the three moves proven safe in wave-3
(UIComponent re-pin 57910ac, AsyncFileHolmes/MusicLibrary 386fe70, MidiParser 5f05b23). dtk
re-derives .pdata after `touch config.yml`; never leave two pins overlapping.

---

## 6. Reproduction

Analysis scripts (throwaway) at `/tmp/sliver_hunt/{analyze.py,master2.json,clusters2.json}`;
all use `tools/map_lint.py` parsers + a 60-line LE-COFF reader (section sizes + fn symbols,
noise-filtered). Key joins: report units by basename **plus** `metadata.source_path` (39
units mis-join otherwise); compiled obj resolved from `source_path` (basename rglob picks
`ui/Utl.obj` over `rndobj/Utl.obj`). Judge every landing ONLY by
`report.json measures.matched_functions` after `tools/fresh_report.sh`.
