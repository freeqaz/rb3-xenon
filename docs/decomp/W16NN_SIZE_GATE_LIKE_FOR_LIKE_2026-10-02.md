# W16-NN — the naming size gate measures like against like; one row installed, one alias membership withdrawn (2026-10-02)

**Branch** `w16-nn` off main `4f29d4495`. **Ruler** `name_check` (graded; `report.json`
`provenance.diff_config`). Follows W16-NK §4 (`W16NK_GAME_ANON_ROWS_2026-10-02.md`).

## 1. Where the defect actually was

The brief said `tools/icf_pair_adjudicate.py` refuses EH-bearing template pairs ("392 vs ours 440").
**Measured: it does not.** Its reader (`collect` → `coff_bodies_ext.function_bodies_ext`) already reads
our side of all six W16-NK rows at the retail size (392/392, 328/328, 100/100, 96/96, …), with masked
bodies equal. The refusals came from W16-NF/W16-NK's lane-local `decide.py`
(`abs(retail - ours) > 8`), whose "ours" was the in-tree **`anon_candidate_scorer.coff_function_sizes`**:
the whole COMDAT **section**, i.e. `[8 B EH prefix][body][8 B EH prefix][__unwind$ funclet]`
(440 = 8 + 392 + 8 + 32). The scorer's second reader, `coff_functions_full`, sliced to the next function
symbol but still billed the interior 8-byte EH prefix to the function above it — the STLPORT-1
`EH_PREFIX_SUFFIX_ARTIFACT` that `coff_bodies_ext` had already fixed.

**Calibration** — the 26,399 named rows `report.json` scores at fuzzy 100 (same function by
definition), retail row size vs our size of the same name in the unit's base obj:

| measure of "ours" | equal to retail |
|---|---:|
| `coff_bodies_ext` extent (now `function_extent_sizes`) | **26,390 (99.966%)** |
| `coff_functions_full` (old) | 25,952 (98.307%, uniform +8) |
| `coff_function_sizes` (old, what the gate used) | 18,827 / 26,284 (71.629%) |

The 9 extent residuals are ours +4 in `/Od` objects (`keygen_xbox`, Quazal MD5). Over all our objects,
the old section measure over-states 41,912 of 557,594 symbol instances (never under); the old slice
over-states 6,576 by exactly +8.

**The legacy rule was wrong in BOTH directions** (21,383 names both sides define, non-vacuous):

| like-for-like truth | legacy accepted | legacy refused |
|---|---:|---:|
| extents equal (or retail = ours + zero pad) | 15,241 | **5,843** (27.7%; 5,692 masked-byte-identical) |
| retail genuinely larger | **145** (45%) | 177 |
| ours larger | 74 | 130 |

So the fix is a like-for-like measure, **not** a wider tolerance — the ±8 was compensating for the
unlike measure, and it admitted retail bodies up to 8 B larger.

## 2. What changed

- `tools/coff_bodies_ext.py`: `function_extent_sizes(path)` — the one size to compare with retail.
- `tools/anon_candidate_scorer.py`: `coff_function_sizes` and `coff_functions_full` measure that extent
  (both 99.966% on the calibration); the old measure survives only as `comdat_section_sizes`.
  `--selftest` passes.
- `tools/icf_pair_adjudicate.py`: `size_gate(rt, ob, our_name)` — accept iff retail's extent equals one
  of our compiled extents for that name, or retail = ours + zero alignment words (`retail_tail_pad`);
  otherwise refuse and name the direction. `--size` prints it per pair; a retail side spelled `0xADDR`
  resolves to its target-obj name.
- **TU variants** (found by this lane's own re-run, §4): one COMDAT name can compile to different bodies
  in different TUs, and `collect()` keeps the first in sorted path order. 311 of our 96,954 names have
  more than one extent; 41 of 21,720 same-name pairs match retail only through a non-first variant. The
  gate checks every variant and names the obj.

T1 itself is unchanged: inside `adjudicate` a size gate is implied by masked-body equality. The gate's
consumers are naming gates that score by fuzzy.

## 3. Controls (`--chasetest`), each shown to fail

Picked from the live tree; the documented W16-NK rows are preferred while still valid.

| control | expect | normal | red under |
|---|---|---|---|
| EH-FUNCLET POSITIVE — `vector<ObjPtr<GemTrackDir>>::_M_fill_insert_aux`, retail 392 / our section 440 | ACCEPT | ACCEPT | `eh` |
| RETAIL-LARGER DECOY — +4 B, single variant, no funclet | REFUSE | REFUSE | `tol` |
| RETAIL-LARGER W16-NK — `fn_82697FE8` 136 vs `__unguarded_partition<ObjEntry*>` 116 | REFUSE | REFUSE | `tol` |
| OURS-LARGER DECOY — +4 B, single variant, our section == extent | REFUSE | REFUSE | `tol`, `onesided` |
| VARIANT POSITIVE — `PropSync<Piece>`, retail 392, first def 360 (Gem.obj), OutfitConfig.obj 392 | ACCEPT | ACCEPT | `firstdef`, `eh` |

`--self-break-size {eh,tol,onesided,firstdef}` re-introduces one wrong rule (eh = section length, the
W16-NF/NK measure; tol = ±64 B; onesided = ours-larger admitted; firstdef = first TU variant only) and
exits 0 only if **exactly** the declared controls go red. Measured: eh → 2 red, tol → 3, onesided → 1,
firstdef → 1, all rc=0; plain `--chasetest` rc=0, "selftest PASSED". The five older self-breaks
(`--self-break`, `-slots`, `-tailpad`, `-rename`, `-overcarve`) all still report OK.
**Meta-check:** with the `eh` break neutered (section lookup monkeypatched to return an extent) the
harness reports `FAILED` and returns 1, naming the EH control `MISSING`.

## 4. Re-run over every pair the old gate refused

62 distinct (retail address, proposed name) pairs refused on size (W16-NK 15, W16-NF 47; both lanes
ran the identical `decide.py`). Script: `~/tmp/w16nn_rerun.json` (not committed).

| state at main | gate ACCEPT | gate REFUSE | undecidable |
|---|---:|---:|---:|
| installed since (map name == proposal) | **39** | 0 | |
| mapped to another name by a later lane | 3 | | 1 (our spelling not compiled) |
| still anonymous | 4 | 15 | |

**Prediction failure, recorded:** the first cut (first-definition only) read **REFUSE on 2 installed rows
the grader scores at fuzzy 100** — `ObjVector<ObjPtr<RndDir>>::resize` (84 vs 72) and `PropSync<Piece>`
(392 vs 360). Our extent ended in `0x00000000` while retail's tail was a normal epilogue: the 72/360-B
bodies come from `Gem.obj`/`ExternalMic.obj`, the 84/392-B ones from `OutfitConfig.obj`/
`BandStarDisplay.obj`, and retail kept the latter. That is the TU-variant rule in §2. After it, every
installed row reads ACCEPT.

The 15 still refused differ genuinely (+16…+344 B either way): `BandDirector::ReadyForMidiParsers`,
`~TrackPanelDirBase`, `ObjVector<Character::Lod>::resize` (retail +16), `FileMerger::AppendLoader`,
`RangedDataCollection::FindRangeAtTick`, `~Automator` (retail +40), `EventTrigger::StartAnim`,
`JoypadTerminateCommon`, `__unguarded_partition<ObjEntry*>` (retail +20), `DataInit`,
`PreloadPanel::SetTypeDef`, `UTF8ToLower`, `UTF8ToUpper`, `Quazal::MemoryManager::Free`, `OggRealloc`.

The 4 that pass the size gate, carried through the rest of the evidence:

| addr | proposal | size | retail witness | verdict |
|---|---|---|---|---|
| `0x824CD6C8` | `_Copy_Construct<WorldDir::PresetOverride>` | 60 = 60 | `_M_create_node<PresetOverride>` @`0x824CDC68` → it; ours calls this name | **INSTALLED**: T1 + chase PROVEN, scratch fuzzy 100 / mpn 100, MidiSynth base defines it |
| `0x822CDFC0` | `vector<FlowMathOp>::_M_fill_insert_aux` | 392 = 392 | `vector<FlowMathOp>::_M_fill_insert` @`0x822CECE0` → it | **REFUTED on bytes**: retail `mulli …,0xc` vs ours `0x34`; retail element dtor folds to `~ObjPtr<RndMesh>`, assignment to `ObjPtr<BandCharacter>::SetObjConcrete` — a 12-B `ObjPtr<T>` element, not `FlowMathOp` (52 B). Scratch fuzzy 94.4. The caller's map name (scores 99.96) is itself a template-twin suspect |
| `0x824CB940` | `WorldDir::PresetOverride` ctor | 96 = CharBlendBone.obj variant | `PropSync<PresetOverride>`, `ObjList<PresetOverride>::resize` → it | not installed: the row's unit (PropSync) compiles a **64-B** variant, so naming pairs against the wrong body; a re-home into a 96-B unit is far away |
| `0x822DC828` | `RndMesh::VertVector::clear` | 8 = 8 | `~RndMesh`, `SetKeepMeshData` → it | not installed: vacuous (8 B), the row sits in FlowOnStop, and Mesh.obj compiles an **80-B** variant (the 8-B one is Gem/GemRepTemplate/NoteTube's) |

Size + a named caller is necessary, not sufficient: `0x822CDFC0` passed both and the bytes said no.

## 5. A wrong alias group, found by the install

`scripts/symbol_aliases.json` group `$_Copy_Construct` (address `0x822A5448`) had survivor
`_Copy_Construct<PresetOverride>` with `_Copy_Construct<OldMatOption>` folded. Retail keeps **two**
60-B bodies:

| retail | reloc +0x24 | T1 vs PresetOverride | T1 vs OldMatOption |
|---|---|---|---|
| `0x822A5448` (mapped `_Copy_Construct<OldMatOption>`) | `??0OldMatOption@@QAA@ABV0@@Z` | REFUTED | PROVEN |
| `0x824CD6C8` (anonymous until now) | `??0PresetOverride@WorldDir@@QAA@ABU01@@Z` | PROVEN | REFUTED |

Different relocation targets cannot ICF-fold. The group survived because its survivor was never
map-resident, so the validator tolerated it; mapping `0x824CD6C8` would have made it a contradiction.
The membership is withdrawn (`folded: []`, `withdrawn` record class `TEMPLATE_TWIN_NOT_FOLD`), nothing
pruned. Predicted cost of the withdrawal: 0 — both `_M_create_node` callers already call the right twin.

## 6. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-nn-ab --patch ~/tmp/w16nn_install.patch`
(the install commit `608db8b10` only — map entry + alias withdrawal; the tool commits do not touch the
build). Run dir `~/tmp/wt-w16-nn-ab/.ab_measure_runs/20261002-130642-w16nn_install-142102/`.

**Prediction, written before the run:** +1 matched / +60 B, 0 rows down, withdrawal costs 0.

```
leg A: matched=51161 masked=24557 honest=26604 code%=54.259907  (recompiles: 0, settled)
leg B: matched=51162 masked=24557 honest=26605 code%=54.260490  (recompiles: 0, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000583pp  Δcode_bytes=+60
Δfuzzy=+0.000586pp   (legA 60.172234 -> legB 60.172820)
units at 100% [mpn]: 473 -> 473 (0 reached, 0 fell off); [all-rows-fuzzy]: 419 -> 419
[control none] +60 B -- REAL_PAIRING (first naming of an anon address)
```

**Measured identical to the prediction.** Leg A equals W16-NK's leg B (51,161 / 54.259907), so the
baseline is the landed main. Row-level diff of the two archived legs by name: **0 up, 0 down**, one row
gone (`default/MidiSynth fn_824CD6C8`, 0 / 0) and one new (`_Copy_Construct<PresetOverride>`, 100 / 100,
60 B) — the same row renamed.

## 7. Gates (final tree)

Worktree `~/tmp/wt-w16-nn`, full build after a forced re-split (symbols.txt at a fixed point on the
first build; renamer `[APPLIED] 3093 files checked`):
- `python3 tools/map_name_injectivity.py`: **OK**, 33,716 applied rows, injective (+1 enumerated exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,797 map-consistent (was 1,796), 290 tolerated
  (was 291), **0 contradicted**, 2,088 total — the withdrawn group moved from tolerated to map-consistent.
- `python3 tools/icf_pair_adjudicate.py --chasetest`: rc=0, "selftest PASSED -- the instrument can both pass
  and fail"; `--self-break-size eh|tol|onesided|firstdef`: all OK (2 / 3 / 1 / 1 red), rc=0.
- `python3 tools/anon_candidate_scorer.py --selftest`: all checks passed.
- `python3 scripts/verify_objs_patched.py --verify-manifest`: OK (1,258 decomp, 3,093 target objects).
- `tools/native_build_gate.sh` (run last on the final code; only this doc line follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 8. Leads left alone

- `0x822A5448` `_Copy_Construct<OldMatOption>` is mapped but sits in the **Character** unit, whose base
  obj does not define it (Gem / OutfitConfig / ExternalMic do), so it reads unpaired.
- `0x822CECE0` mapped `vector<FlowMathOp>::_M_fill_insert` calls a 12-B-element `_M_fill_insert_aux`;
  the name is a template-twin suspect.
- **TU-variant COMDATs (311 names)** are an ODR-shaped divergence in our build: the same inline/template
  function compiles to different code in different TUs (`ObjVector<ObjPtr<RndDir>>::resize` 72 vs 84,
  `PresetOverride` ctor 64/76/96, `VertVector::clear` 8 vs 80). Which body the linker keeps depends on
  link order, so any tool keyed on "our body for name N" must say which TU it read.
- The 9 `/Od` rows at ours +4 (§1) were not examined.
