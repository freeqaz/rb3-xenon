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
