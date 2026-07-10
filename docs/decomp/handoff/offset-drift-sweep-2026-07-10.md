# Offset-drift sweep — systematic layout-drift detection (2026-07-10)

Generalization of the round-5 header-candidates campaign: instead of waiting
for handoff docs to name header suspects, mine EVERY fuzzy near-miss for the
layout-drift fingerprint mechanically, then spend recon effort only on ranked
clusters.

## Method (repeatable)

```bash
python3 scripts/harvest/gen_nearmiss_pool.py --min 85 --max 99.99 -o ~/tmp/pool.json
python3 scripts/harvest/offset_drift_sweep.py ~/tmp/pool.json -o ~/tmp/drift.json
```

`offset_drift_sweep.py` (new, this round) runs objdiff per pool function and
extracts every immediate/offset arg diff via `diff_inspect.parse_breakdowns`,
classifying each memory operand by base register:

- **stack** (`r1`) — frame-slot moves, usually a consequence not a cause;
- **frame** (`r30`/`r31`) — ambiguous (frame copy vs `this`);
- **struct** (any other base reg) — struct-member offset drift candidate;
- **global** (trailing reloc symbol) — member/ordering drift of TU statics
  or a global object.

Interpretation guide (validated this round):

- **Uniform-signed dominant delta across ≥3 struct/global diffs = layout
  drift** (a member added/removed/resized in a shared header). GO for recon.
- **Balanced ±N pairs = access-order / regalloc divergence** — usually
  permuter-class or at-limit, NOT a header fix (e.g. `CheckBSPTree`'s
  std-order reversal, `Rot::Multiply`'s ±4/±8 pairs).
- **`oris`/`ori` constant ladders where ours = 2× retail** = an enum used as
  `1 << k` gained an extra enumerator before the used ones. One header enum
  fix can span several functions (seen in PracticePanel + OvershellPanel).
- **delta semantics**: in sweep examples, TGT = retail, SRC = ours;
  `delta = ours − retail`.

The 2026-07-10 run: 349 functions swept in ~12 s (all objdiff-cached), 91
flagged, 59 with ≥2 struct/global diffs.

## Recon lanes launched 2026-07-10 (dossiers at ~/tmp/hdr2_recon_*.txt)

1. **panels** — CustomizePanel::Handle (31× −4, cleanest witness in sweep),
   PracticePanel::Handle (zoned −4/−8/−12 + oris ladder), OvershellPanel
   (−20 ×3 + doubled oris ladder + frame reorg), CharacterCreatorPanel (±4).
2. **synapse** — DSP::SynapseAPO +4 before this+0x170 (DC3-newer member
   suspect, SortNodes-pattern).
3. **locale** — Locale.cpp TU statics in wrong .data ORDER vs retail
   (LocalizeFloat 99.95 / LocalizeSeparatedInt 97.39); + Locale::Init
   MemPushTemp site.
4. **patchmesh** — BandPatchMesh::MeshVert stride 0x1e (retail) vs 0x1a
   (ours); 2 witnesses in TU.
5. **gametail** — Game +4 tail (mMuckWithPitch), per
   `game-layout-followups-2026-07-10.md` Follow-up 2.
6. **memtemp** — 8 MemPushTemp/MemPopTemp sites (DataFile:69, Locale:186,
   Text:765/1087, Rnd:1271, Mesh:1692/1767, ObjPtrVec_impl.h:45) vs retail
   MemTemp RAII (proven divergence, DataArray::Load 78fdc92).
7. **misc** — FlowIf::~FlowIf −96 (suspect pairing artifact),
   UsbMidiKeyboard::Poll byte-order, CharEyes::Poll +312, RndLine −12.

## Unclaimed candidates (not in any recon lane — future rounds)

From the ranked table (`~/tmp/drift_candidates_table.md`, regenerable):

- `UtilDrawPlane` (93.06, rndobj/Utl): 8 diffs, 75% uniform +4 — Plane or
  arg-struct +4? Best unclaimed LAYOUT candidate.
- `BandUI::WipeIn/WipeOutIfNecessary` (both 99.97): identical 2× +76 struct
  pattern in both — one BandUI member block off by 76 = single fix, 2 closes.
- `LabelNumberTicker::SyncProperty` (99.96): 2× −8 uniform.
- `AppInlineHelp::Handle` (99.95): 2× −16 uniform.
- `RGGemMatcher::FretMatchImpl` (99.55): 2× −24 uniform.
- `RandomGroupSeqInst` ctor (91.65): 2× +124 — Sequence family layout.
- `Splash::EndSplasher` (99.97), `NgPostProc::CheckPosterizeAndKaleidoscope`
  (99.93), `NgLight::SetAndClearShadowViewport` (99.95): 1-2 diffs each,
  cheap follow-ups if their structs get touched anyway.
- `ParseNode` (DataFile, 98.69, 24 global diffs, non-uniform): DataFile TU
  statics ordering — same class as the Locale finding; bigger but messier.
- `BandCharacter::Multiply` / `Spotlight::BuildNGQuad` / `Rot::*` /
  `CharIKFingers::*` / `TransformKeys`: balanced-± = ordering/regalloc,
  permuter-class, do NOT header-recon these.

Full sweep JSON: `~/tmp/drift_sweep_full.json` (regenerable in ~15 s).

## Caveats

- The sweep sees only PAIRED near-misses. Unpinned TUs and 0%-paired
  functions with layout drift are invisible — pin expansion feeds this.
- `frame`-class diffs (r31/r30) are ambiguous; a big frame-class count with
  small struct-class count (OvershellPanel: 64 frame vs 7 struct) means
  local/temporary layout divergence — usually inlining or local-type size,
  investigate before blaming the class header.
- Deltas on `li`/`cmpwi`/`addi` immediates (class `?`) can be enum values,
  sizeof operands (`li r3, 0x50` vs `0x4c` = operator-new size = layout
  drift witness for the ALLOCATED class!), or loop bounds — the examples
  list them; read before dismissing.
