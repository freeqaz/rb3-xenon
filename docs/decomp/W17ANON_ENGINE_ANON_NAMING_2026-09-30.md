# W17-ANON — anonymous fn_ rows in engine units (bandobj / rndobj / char / rnddx9), 2026-09-30

Branch `w17-anon`, rebased onto main `1c9ccf51`. Scorer: `tools/anon_candidate_scorer.py`, with a
new top-level `--scope` flag (plus `--skip-addr`, `--scratch-dir` and `evaluate --by-dir`).
Rule: the W16-HD rule unchanged (T = 75, M = 1, spatial sandwich + occupancy, bijective).
`0x82438F40` (rndobj/Utl) was skipped because another lane owns it.

## 1. Population

- 193 units, of which 50 bandobj, 77 rndobj, 59 char and 7 rnddx9.
- 1,718 target rows / 350,272 B.
- Scored: 1,679. Skipped: 26 deliberately `null`, 12 with no in-band candidate, 1 owned by another lane.

## 2. Own negative control, per directory

Method: `control2` was run once per directory, stratified to that directory's own target sizes
(seed 20260930, k = 6). Leg OUT removes the true name from the pool, so any fire is a false
positive. Figures are at the chosen rule, with the named / anon brackets identical unless noted.

| dir | rows | in-pool precision | recall | **FP (true name removed)** | score-only FP at T=75 |
|---|---|---|---|---|---|
| bandobj | 1,001 | 201/201 = 100% | 20.1% | **2 / 1,001 = 0.20%** (0 at T ≥ 80) | 12.1% |
| rndobj | 1,000 | 100/101 = 99.01% | 10.0% | **1 / 1,000 = 0.10%** | 12.2% |
| char | 1,001 | 70/70 = 100% | 7.0% | **0 / 1,001** | 11.0% |
| rnddx9 | 210 (all eligible) | 23/23 = 100% | 11.0% | **0 / 210** | 8.1% |

Every FP event is the known error mode, a same-shape sibling or template twin:

- `Keys<ObjectStage>::Add` vs `Keys<DircutEntry>::Add`
- `SetHeight` vs `SetPrefab@BandCharDesc`
- `SetEvenOddDisabled` vs `SetProcAndLock@Rnd` (this is also rndobj's one in-pool miss, even at T = 100)

The spatial gate carries the rule: score alone at T = 75 is 8–12% FP.

## 3. Proposals

`apply --threshold 75 --margin 1 --spatial` produced **43 proposals / 18,092 B**, with 0 dropped as
non-bijective. rnddx9 produced 0, because the spatial gate rarely finds named neighbours there.

## 4. W16-HF adjudicator (`tools/anon_proposal_adjudicate.py`) and its control

- **Per-proposal results**, the same with and without `--independent`: **25 CONTRADICTED /
  16 SUPPORTED / 2 UNANCHORED**. The table below gives each verdict.
- **`--control 200` in these units prints `WRONG neg>0 = 200/200`, but that figure is VACUOUS.**
  `run_control`'s `NEG` tuple still includes `NAME_MAPPED_ELSEWHERE`. That check fires on
  **200/200 WRONG and 0/200 TRUE** rows by construction, because every sibling is mapped.
  W16-HF's doc says this was removed from the control's negative count, but the code still counts it.
- **Recomputed on content negatives only** (`RETAIL_ONLY` / `OURS_ONLY` / `CALLER_CONTRA`, with
  ≥ 1 positive, which is the doc's accept rule):
  - TRUE 108/200 (54%)
  - **WRONG 10/200 (5.0%)**, against W16-HF's 0.6% in the game layer.
- All 10 WRONG accepts are template-instance twins or a near-identical override, scoring 97–100:
  - MeshAnim `Key<vector<…>>` readers and `resize` instances
  - a `list<EventCall>` ctor
  - `ListPollChildren`

  ⇒ In engine units a mechanical SUPPORTED is not decisive for template names.
- Most CONTRADICTED verdicts are not identity evidence. They are:
  - retail inlining an `ObjPtr<X>` ctor, so the `ObjPtr<X>` vtable reads as RETAIL_ONLY
  - fold-named `??_8` vbtables
  - retail inlining `operator new` as `MemAlloc`, and similar.

  The three real contradictions are caller contradictions, and they are the three rejects.

## 5. Independent retail-byte checks

- **Constructors (12 rows).** `vtable[-1] → COL → TypeDescriptor` on the retail image
  (`orig/45410914/band.exe`) names exactly the proposed class in every case. Examples:
  `.?AVBandLeadMeter@@`, `.?AVCharacterTest@@`, `.?AVRndCamAnim@@`.
- **Virtuals.** The retail RTTI vtable slot equals our slot:
  - `CollidePlane`: PlayerDiffIcon slot 8
  - `SetInterestFilterFlags`: BandCharacter slot 18
  - `GetDistanceToPlane`: RndText slot 1
- **Retail call edges that cross-validate proposals:**
  - `??0Character` calls `0x823DDA08`, which is the proposed `??0CharacterTest`.
  - `NewObject@BandList` calls `0x8233E028`, which is `??0BandList`.
  - `UpdateFocusAndPulseAnims` calls `0x8233DAC0`, which is `UpdatePulseAnim`.
- **Rejected (3):**
  - `0x8233f5a0` `PropSync<ObjVector<BoneOp>>`. Retail's inlined element ctor zeroes all three
    floats from one `0.0f` constant. That is `HighlightObject(o)`, not BoneOp's
    `0 / 1.0f / -30.0f`. The CharSignalApplier `.text` pin `0x8233F468–0x8233F7A0` inside
    BandList's span looks like a **mis-pin**. It is not touched here, because re-homing is not
    metric-neutral.
  - `0x8233de90` `vector<HighlightObject>::_M_insert_overflow_aux`. Matched callers call the
    BoneOp instance, and retail's callee is bound to `__uninitialized_copy<BoneOp>`.
  - `0x8227d4c0` `ObjPtrList<CharCollide>::sort<ByRadius>`. Retail's bytes equal our
    `sort<SortCollides@CharHair>` exactly, and matched callers of both names reach this address.
    That makes it an ICF fold, and naming it would charge the CharHair caller. An alias would need
    its own proof.

**Survivors: 40 / 17,244 B.**

## 6. Carve fix and A/B

- **Carve fix.** Naming `0x824A5830` (`RedundantState`) made jeff's Class-4 merge absorb
  `fn_824A58BC` and `fn_824A58C4`, taking the row from 0x8C to 0xA8. On retail bytes both
  fragments are reached only by the function's own eleven `bc` branches. The merged file is a
  split fixed point under the old map. It is committed separately (`21330738`) so the map A/B
  has it as its base.
- **A/B** (`tools/ab_measure.py --from-dirty`, map-only, both legs at a split fixed point):
  - **Δmatched +4 · Δhonest +4 · Δcode_bytes +444 · Δcode% +0.004335 pp · Δfuzzy +0.144932 pp**
  - The `none` control is also +444 B.
  - **All 40 named rows rose, and 0 rows fell anywhere in the binary.**
  - Rows reaching fuzzy 100: `RedundantState`, `map<uint,MeshInfo>::operator[]`, `OnSetText`,
    `CollidePlane`.
- **ICF validation.** `python3 tools/icf_alias_finder.py --validate` on the built tree:
  **PASS — 1,479 map-consistent, 250 tolerated, 0 CONTRADICTED** (1,730 groups).

**Re-measured after rebasing onto main `ff21521f0`** (W16-HE and W16-HG had landed in between):

- Measurement: `ab_measure --revert <map commit>`, predicted to be the exact negation.
- **Result: −4 matched / −444 B / −0.004334 pp code% / −0.144918 pp fuzzy.** The prediction held.
- `icf_alias_finder --validate` was run again on the rebased built tip: PASS, 0 CONTRADICTED.

## 7. Per-proposal verdicts

| addr | B | proposed | fuzzy | HF pre-verdict (dep / indep) | decision |
|---|---|---|---|---|---|
| `0x8227d4c0` | 128 | `??$sort@UByRadius@@@?$ObjPtrList@VCharCollide@@VObjectDir@@@@QAAXAB...` | 99.7 | CONTRADICTED / CONTRADICTED | REJECT: ICF fold with sort<SortCollides@CharHair> (retail bytes equal that one) |
| `0x8227e5c0` | 32 | `?SetInterestFilterFlags@BandCharacter@@UAAXH@Z` | 75.0 | SUPPORTED / SUPPORTED | accept: same offsets + tail-call Character::; vtable .?AVBandCharacter@@ slot 18 = ours |
| `0x82290930` | 120 | `?Replace@?$ObjDirPtr@VRndDir@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | 77.5 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8229f960` | 1012 | `?Deform@Piercing@OutfitConfig@@QAAXPAVSyncMeshCB@@@Z` | 77.7 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x822b4040` | 148 | `?Freeze@BandCamShot@@QAAXXZ` | 85.1 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x822b7eb8` | 1588 | `?Load@BandCamShot@@UAAXAAVBinStream@@@Z` | 80.0 | CONTRADICTED / CONTRADICTED | accept: 15 agree; retail-only = old-rev evntanm conversion our source lacks |
| `0x822bc580` | 108 | `?GetPeakValue@BandCrowdMeter@@QAAMXZ` | 76.4 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x822c5658` | 256 | `?Load@BandIKEffector@@UAAXAAVBinStream@@@Z` | 89.0 | CONTRADICTED / CONTRADICTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x822c9ce0` | 716 | `??0BandLeadMeter@@QAA@XZ` | 81.2 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x822ce860` | 564 | `??0BandScoreboard@@QAA@XZ` | 83.9 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x822d5168` | 740 | `??0EndingBonus@@QAA@XZ` | 81.4 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x822dc0b0` | 700 | `??0OverdriveMeter@@QAA@XZ` | 77.6 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x822e1388` | 424 | `?GetCrossSection@ChordShapeGenerator@@QAAXMAAVCrossSec@1@@Z` | 75.5 | CONTRADICTED / CONTRADICTED | accept: same skeleton (+0xc set clear, +0x24 store, Face stride 6); we add 2 insert_unique |
| `0x822e2280` | 424 | `?ConnectVertProfiles@ChordShapeGenerator@@QAAXPAVRndMesh@@ABV?$map@...` | 80.6 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8231f478` | 268 | `?SyncProperty@ReviewDisplay@@UAA_NAAVDataNode@@PAVDataArray@@HW4Pro...` | 93.9 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x82324fe8` | 28 | `?CollidePlane@PlayerDiffIcon@@UAAHABVPlane@@@Z` | 100.0 | UNANCHORED / UNANCHORED | accept: byte-exact 7-insn forwarder; retail vtable .?AVPlayerDiffIcon@@ slot 8 = our slot 8 |
| `0x8232afb0` | 120 | `?MostImportantHuman@BandWardrobe@@SAHPBVSlotInfo@1@@Z` | 90.0 | UNANCHORED / UNANCHORED | accept: 30/30 retail words exact |
| `0x8233d8e0` | 320 | `?StartFocusAnim@BandList@@QAAXHW4AnimState@1@@Z` | 89.2 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8233dac0` | 268 | `?UpdatePulseAnim@BandList@@QAAXHAAVTransform@@@Z` | 92.6 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8233de90` | 328 | `?_M_insert_overflow_aux@?$vector@VHighlightObject@@V?$StlNodeAlloc@...` | 99.9 | CONTRADICTED / CONTRADICTED | REJECT: BoneOp fold, caller-contradicted |
| `0x8233e028` | 1128 | `??0BandList@@QAA@XZ` | 89.7 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x8233ec90` | 396 | `?UpdateFocusAndPulseAnims@BandList@@QAAXHAAVTransform@@@Z` | 94.4 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8233f220` | 112 | `?NewObject@BandList@@SAPAVObject@Hmx@@XZ` | 86.9 | CONTRADICTED / CONTRADICTED | accept: retail StaticClassName@BandList + calls proposed ??0BandList; op new inlined |
| `0x8233f5a0` | 392 | `??$PropSync@UBoneOp@CharSignalApplier@@@@YA_NAAV?$ObjVector@UBoneOp...` | 86.1 | CONTRADICTED / CONTRADICTED | REJECT: sibling (HighlightObject element ctor; callers call PropSync<HighlightObject>) |
| `0x82343558` | 112 | `?NewObject@BandHighlight@@SAPAVObject@Hmx@@XZ` | 86.9 | CONTRADICTED / CONTRADICTED | accept: retail StaticClassName@BandHighlight; op new inlined |
| `0x82350fd8` | 696 | `?PopupHelp@BandTrack@@QAAXVSymbol@@_N@Z` | 97.1 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x82371f48` | 716 | `??0Character@@QAA@XZ` | 84.9 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x823780e8` | 176 | `?PlayGroup@CharDriver@@QAAPAVCharClipDriver@@PBDHMMM@Z` | 93.1 | CONTRADICTED / CONTRADICTED | accept: retail inlines PlayGroup(CharClipGroup*) (Find<CharClipGroup> anchors the PBD overload) |
| `0x823c4230` | 288 | `??0CharBlendBone@@IAA@XZ` | 86.3 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x823d7d70` | 152 | `??0CharBone@@IAA@XZ` | 83.6 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x823dda08` | 524 | `??0CharacterTest@@QAA@PAVCharacter@@@Z` | 90.1 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x823fa038` | 1580 | `?Load@RndTransformable@@UAAXAAVBinStream@@@Z` | 84.0 | CONTRADICTED / CONTRADICTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8241dc80` | 1304 | `?OnCompareEdgeVerts@RndMesh@@IAA?AVDataNode@@PBVDataArray@@@Z` | 82.8 | CONTRADICTED / CONTRADICTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x82455f50` | 176 | `?GetDistanceToPlane@RndText@@UAAMABVPlane@@AAVVector3@@@Z` | 80.7 | SUPPORTED / SUPPORTED | accept: same offsets + vcall + tree walk; vtable .?AVRndText@@ slot 1 = ours |
| `0x824574a8` | 284 | `?NumCharsInBytes@RndText@@QAAHABVString@@ABVStyle@1@AAMH@Z` | 100.0 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x824575d0` | 176 | `??A?$map@IVMeshInfo@RndText@@U?$less@I@stlpmtx_std@@V?$StlNodeAlloc...` | 100.0 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8245a088` | 72 | `?OnSetText@RndText@@QAA?AVDataNode@@PAVDataArray@@@Z` | 100.0 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8245f658` | 312 | `??0RndTransAnim@@IAA@XZ` | 89.7 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x8246b680` | 192 | `?AllocShader@RndShaderMgr@@QAAPAXXZ` | 76.2 | SUPPORTED / SUPPORTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x8246fe80` | 312 | `??0RndMeshAnim@@IAA@XZ` | 90.4 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x82472200` | 320 | `?Load@RndLightAnim@@UAAXAAVBinStream@@@Z` | 78.6 | CONTRADICTED / CONTRADICTED | accept: identity anchored by agreeing named relocs; negatives are source divergence |
| `0x82486440` | 240 | `??0RndCamAnim@@IAA@XZ` | 87.5 | CONTRADICTED / CONTRADICTED | accept: retail vtable RTTI names this class; HF negatives = inlined ObjPtr ctor vtables + fold-named ??_8 vbtables |
| `0x824a5830` | 140 | `?RedundantState@RndShader@@IAA_NPBVRndMat@@W4ShaderType@@_N22@Z` | 80.0 | CONTRADICTED / CONTRADICTED | accept: 10 matched callers agree; retail gModTime ref is source divergence; carve merged 0x8C->0xA8 |
