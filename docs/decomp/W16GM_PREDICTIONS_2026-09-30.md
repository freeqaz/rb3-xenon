# W16-GM pre-registered predictions (written BEFORE any `ab_measure` run)

Source of every number: `tools/fcnp_spelling_price.py` at `8af79551`
(`docs/decomp/W16GM_FCNP_SPELLING_PRICES_2026-09-30.md`). A miss is recorded as a
miss in the adjudication doc; nothing here is edited after measurement.

Baseline (settled worktree, `report.json`): matched_functions **44,154** ·
matched_code **4,177,988 B** · matched_code_percent **40.772522** ·
masked_equal_functions **23,323** · total_code 10,247,068 · alias map
`Loaded 6108 ICF equivalence entries`.

## Batch 1 — six `list<T>::insert` memberships restored into group `0x823d14c0`

Each predicted independently; the batch prediction is the plain sum (no row is
priced to two of these spellings — `multi_address_spellings = {}` and the only
set-clearable row needs `UIComponent*`/`UITrigger*`, neither in this batch).

| spelling (element) | Δcode B | Δrows→100 | units | masked_equal rows |
|---|---:|---:|---:|---:|
| `list<TrackType>::insert` | +1,632 | +1 | 1 (SongParser) | 0 |
| `list<RndMat*>::insert` | +1,488 | +7 | 7 | 0 |
| `list<SortNode*>::insert` | +1,108 | +4 | 2 | 0 |
| `list<FileMerger::Merger*>::insert` | +1,056 | +3 | 1 | 0 |
| `list<Triangle*>::insert` | +1,040 | +1 | 1 | 0 |
| `list<RndMesh*>::insert` | +940 | +2 | 2 | 0 |
| **batch 1** | **+7,264 B** | **+18 rows** | 12 distinct | **0** |

Derived predictions:

- Δ`matched_code` = **+7,264 B** exactly; Δ`matched_code_percent` = 7,264 /
  10,247,068 = **+0.070889 pp**.
- Δ`matched_functions` = **+18** (every priced row has `mpn == fuzzy < 100`, i.e. the
  reloc-name charge is vetted and counts against `mpn` too, so crossing `fuzzy`
  also crosses `mpn`). If some rows are already `mpn == 100`, Δfunctions reads lower
  than +18 with Δbytes unchanged — that would be the DB-4 two-rulers shape, not a
  miss on bytes.
- Δ`masked_equal_functions` = **0**; Δ`total_code` = 0; Δ`total_functions` = 0.
- Partial rows improve but do NOT cross: `WorldCrowd::Mats` (636 B),
  `kdTreeNode::Pack` (972 B).
- Alias map: `GEN ICF-ALIAS MAP` symbol lines **+6**, objdiff `Loaded 6108` →
  **6114** ICF equivalence entries.
- `ab_measure` control: `ALIAS_SUSPECT` fires on the forward run (map-only patch,
  default ruler up, `none` flat) and is silent on the revert run (GK §4).
- Revert direction: exact mirror, **−7,264 B / −18**.
- Validator: PASS before and after, 0 contradicted, group total 1,659;
  `alias_withdrawal_audit.py` live-and-withdrawn stays **4**.

## Declined (no measurement; recorded so the decline is auditable)

- `list<MsgSource::EventSink>::insert` @ `0x82767518` (424 B): CHASED T1 **REFUTED**
  (`MAPPED-VS-PLACEHOLDER fn_827674A0`) AND our spelling is map-resident at its own
  100-byte body `0x82749630` — validator would report CONTRADICTED (FATAL).

## If a batch 2 is run

Same rule: Δ = Σ full_bytes of the restored spellings from the price table, rows
likewise, `masked_equal` 0 except `~ObjPtrList<UILabel>` (one 44 B `masked_equal`
row, `fn_827FD814`), and +336 B / +1 row extra iff BOTH `list<UIComponent*>` and
`list<UITrigger*>` are restored (`PanelDir::SyncObjects`).

## Batch 2 — 22 memberships (written BEFORE its `ab_measure` run; batch 1 had already measured exactly)

Baseline for batch 2 = the committed batch-1 tree: matched **44,172** · matched_code
**4,185,252 B** · code% **40.843410** · masked_equal **23,323** · `Loaded 6114`.

### 2a — seventeen more `list<T>::insert` into `0x823d14c0` (all CHASED T1 PROVEN 0/0/0)

| element | Δcode B | Δrows |
|---|---:|---:|
| `RndPollable*` | +904 | +7 |
| `PropKeys*` | +780 | +1 |
| `VorbisReader*` | +508 | +1 |
| `DataArray*` | +464 | +1 |
| `UIResource*` | +428 | +1 |
| `int` | +400 | +3 |
| `Waypoint*` | +400 | +1 |
| `GamerAwardStatus*` | +364 | +2 |
| `RndDir*` | +296 | +1 |
| `Job*` | +156 | +1 |
| `PassiveMessage*` | +132 | +1 |
| `RndGroup*` | +124 | +1 |
| `SynthPollable*` | +116 | +1 |
| `Voice*` | +108 | +1 |
| `ContentMgr::Callback*` | +56 | +1 |
| `UIComponent*` + `UITrigger*` (jointly: `PanelDir::SyncObjects`) | +336 | +1 |
| **2a** | **+5,572** | **+25** |

### 2b — five husk groups (FLAT T1 PROVEN strict + CHASED PROVEN; group address == survivor map address)

| spelling | group | Δcode B | Δrows | masked_equal rows |
|---|---|---:|---:|---:|
| `vector<ObjPtr<RndTex>>::_M_fill_insert` | 0x822a4fb8 | +368 | +1 | 0 |
| `operator>>(BinStream&, ObjVector<ObjOwnerPtr<Waypoint>>&)` | 0x823dcde0 | +356 | +1 | 0 |
| `operator<<(BinStream&, const list<LayerDir::Layer>&)` | 0x82327878 | +152 | +1 | 0 |
| `~ObjPtrList<UILabel>` | 0x827fa298 | +120 | +2 | **1** (`fn_827FD814`, 44 B) |
| `list<OldMatOption>::list(const list&)` | 0x822a7f60 | +84 | +1 | 0 |
| **2b** | | **+1,080** | **+6** | 1 |

### Batch 2 totals

- Δ`matched_code` = **+6,652 B**; Δ`matched_code_percent` = 6,652 / 10,247,068 =
  **+0.064916 pp**.
- Δ`matched_functions` = **+31**; Δ`masked_equal_functions` = **+1** (the one
  disclosure row crosses); Δhonest = **+30**.
- Alias map: symbol lines **6,930 → 6,952**; `Loaded 6114 → 6136`.
- Validator PASS, 0 contradicted, 1,659 groups; live-and-withdrawn stays 4.
  Five husk groups go from `folded: []` to one live member each.
- `ALIAS_SUSPECT` fires forward, silent on revert; revert = **−6,652 B / −31 / −1**.
- Partial rows improve but do not cross (e.g. `TrackData::FillChannelListWithInactiveSlots` 424 B).
