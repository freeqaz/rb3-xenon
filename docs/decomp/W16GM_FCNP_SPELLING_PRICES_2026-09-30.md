# W16-GM: pricing the FABRICATED_CLOSURE_NOT_PARTITION alias spellings

**Worktree `~/tmp/wt-w16-gm`, branch `w16-gm`, forked from main `8af79551`.**
Extends `docs/decomp/W16GK_UTL_BLOCK_ADJUDICATION_2026-09-16.md`, whose one lesson
this lane operationalises: an alias membership pays at **every caller** of the
spelling, so a candidate must be priced over its whole caller population *before*
adjudication budget is spent on it. Every figure below was measured in this tree
at `8af79551` (settled: a full `./tools/ninja-locked` recompiled 381 TUs of PCH
churn and left every `report.json` measure byte-identical — 44,154 matched /
4,177,988 B / 40.772522 % / `masked_equal_functions` 23,323).

Tool: `tools/fcnp_spelling_price.py` (committed). Full table (all 42 rows, every
underlying row named): `~/tmp/w16gm_fcnp_prices_full.md`, machine-readable
`~/tmp/w16gm_fcnp_prices.json`. Input dump: `~/tmp/w16gm_sub100.jsonl` (150 MB,
regenerable in ~2 min — recipe in §1).

## 0. The brief's counts reproduce exactly

`scripts/symbol_aliases.json` at `8af79551`, `withdrawn[]` dict entries with
`class == FABRICATED_CLOSURE_NOT_PARTITION`:

| | measured |
|---|---:|
| FCNP memberships | **9,390** |
| distinct spellings | **738** |
| groups carrying ≥1 FCNP record | **357** |
| groups per spelling | 2–67 (218 spellings sit in 6 groups, 47 in 67) |
| FCNP spellings already LIVE somewhere (prior restorations) | 8 |

Also: 51 groups carry `address: null` (MakeString / `??_G` dtor families); none holds
an FCNP record, so keying them by survivor drops nothing.

## 1. Method

1. Population = every row of `report.json` with `fuzzy_match_percent < 100` that is
   paired (`fuzzy > 0` or `match_percent_normalized > 0`): **6,333 rows /
   1,081,956 B / 678 units**. All numerics coerced through `int()`/`float()`.
   No two rows share a mangled name, so one name-keyed batch covers all of them.
2. One `objdiff-cli diff -p . --batch --include-instructions -f json` over that list
   (no `--build`). 6,333/6,333 resolved, 0 errors, and the batch's
   `fuzzy_match_percent` **agrees with `report.json` on every row** — the dump is on
   the grader's ruler (`name_check`, from the report's `provenance`).
3. A charged site is a `match_type == "diff_arg"` argument with
   `arg_type == "symbol"` and typed `Symbol` values on both sides
   (retail = `target`, ours = `base`). ⚠ `diff_arg` alone is NOT a relocation-name
   charge — `?PathCompare@@` (register order, `add r31,r11` vs `add r11,r31`) is a
   `diff_arg` too. The tool REFUSES if that row prices to anything. Argument-type
   vocabulary measured over the whole population: `register` 28,913 · `immediate`
   8,790 · `symbol` 7,530 (7,386 Symbol/Symbol) · `branch_dest` 504 · `other` 217.
4. A site is attributable to (spelling S, group A) iff ours == S, S is FCNP-withdrawn
   from A, and retail's name is a member of A (survivor or live folded). **The
   retail-side name pins the address** — the closure put S in up to 67 groups, but
   a call site says which one retail actually used.
5. A row is FULLY clearable by (S, A) iff every non-equal instruction is a
   `diff_arg` whose every differing argument is attributable to that one (S, A).
   Price = `size` from `report.json`, split by `masked_equal`. Rows where (S, A)
   coexists with any other charge are PARTIAL. Rows whose charges are all FCNP but
   span >1 spelling are SET-clearable (one such row: `PanelDir::SyncObjects`, 336 B,
   needs both `list<UIComponent*>::insert` and `list<UITrigger*>::insert`).

Sanity: GK's 13 rows all read 100.0 here; `PathCompare` prices to nothing.

## 2. Headline — the surface is ~42 pairs, not 738 spellings

| | rows | bytes |
|---|---:|---:|
| sub-100 paired population | 6,333 | 1,081,956 |
| rows with NO FCNP-attributable site | 6,264 | — |
| FULLY clearable by one FCNP spelling | **51** | **14,512** (honest 14,468 / masked 44) |
| SET-clearable (two FCNP spellings) | 1 | 336 |
| PARTIAL (FCNP site + other charges) | 17 | 9,196 |

* (spelling, address) pairs appearing at **any** charged site: **42**
* spellings with a non-zero FULL price: **29**
* concentration: **top 10 = 9,920 B = 68.4 %**, top 50 = 100 % (there are only 29)
* **24 of the 29 priced spellings are `list<T*>::insert` folded to `0x823d14c0`** —
  the very group GK restored two members of. The value is one vein, not 738.
* no spelling is attributable at two different retail addresses
  (`multi_address_spellings = {}`), i.e. no call-site evidence of a spelling
  being used against two survivors.

⚠ **This corrects the brief's framing.** "Price the ~738 spellings" presupposed the
class was broad; **~636 of the 738 spellings have no charged site anywhere in the
sub-100 population** — their callers are already at 100 (charge forgiven by another
live alias or the callee is a placeholder), unpaired, or not compiled. A "top ~40"
is the whole list.

### Why only 208 of 7,386 symbol-pair sites involve an FCNP spelling at all

Of the 208 sites where OUR side is an FCNP spelling, only **86** are attributable
(retail's name is a member of a group the spelling was withdrawn from). The other
**122 sites / 62 pairs / 107 rows** charge an FCNP spelling against a retail name in
**none** of its FCNP groups — e.g. our `~vector<ObjPtr<RndPropAnim>>` vs retail
`~vector<String>` (16 sites). Those say the closure had the wrong *address* for the
spelling, not necessarily the wrong *fold*; they are **new** membership claims (at
the retail name's address), not restorations, and are out of this lane's scope.
Priced the same way (rows fully clearable by that single pair): **44 pairs / 82
rows / 12,712 B** — nearly as large as the whole restorable FCNP surface. Filed in
§5 with the top pairs.

## 3. Price list (all 42 pairs; full table in `~/tmp/w16gm_fcnp_prices_full.md`)

`full B` = bytes of rows that reach `fuzzy == 100` if this spelling alone is restored;
`part` = rows/bytes it only partly clears. `map` = is OUR spelling map-resident at its
own address (GK criterion 2 — a `yes` predicts `CONTRADICTED (FATAL)`).

| # | full B | rows | units | part rows / B | address | map | our spelling (element type) |
|---:|---:|---:|---:|---|---|---|---|
| 1 | 1,632 | 1 | 1 | 0 | 0x823d14c0 | no | `list<TrackType>::insert` (enum) — `SongParser::SongParser` |
| 2 | 1,488 | 7 | 7 | 1 / 636 | 0x823d14c0 | no | `list<RndMat*>::insert` — seven `::Mats` overrides |
| 3 | 1,108 | 4 | 2 | 0 | 0x823d14c0 | no | `list<SortNode*>::insert` |
| 4 | 1,056 | 3 | 1 | 0 | 0x823d14c0 | no | `list<FileMerger::Merger*>::insert` |
| 5 | 1,040 | 1 | 1 | 1 / 972 | 0x823d14c0 | no | `list<Triangle*>::insert` — `RndAmbientOcclusion::BuildTrees` |
| 6 | 940 | 2 | 2 | 0 | 0x823d14c0 | no | `list<RndMesh*>::insert` |
| 7 | 904 | 7 | 7 | 0 | 0x823d14c0 | no | `list<RndPollable*>::insert` — `ListPollChildren` family |
| 8 | 780 | 1 | 1 | 0 | 0x823d14c0 | no | `list<PropKeys*>::insert` — `RndPropAnim::AddKeys` |
| 9 | 508 | 1 | 1 | 0 | 0x823d14c0 | no | `list<VorbisReader*>::insert` |
| 10 | 464 | 1 | 1 | 0 | 0x823d14c0 | no | `list<DataArray*>::insert` — `RndDir::OnSupportedEvents` |
| 11 | 428 | 1 | 1 | 0 | 0x823d14c0 | no | `list<UIResource*>::insert` — `UIManager::InitResources` |
| 12 | 424 | 1 | 1 | 1 / 316 | 0x82767518 | **YES @ 0x82749630** | `list<MsgSource::EventSink>::insert` vs survivor `list<MsgSinks::EventSink>` |
| 13 | 400 | 3 | 3 | 1 / 424 | 0x823d14c0 | no | `list<int>::insert` |
| 14 | 400 | 1 | 1 | 0 | 0x823d14c0 | no | `list<Waypoint*>::insert` — `Waypoint::Waypoint` |
| 15 | 392 | 1 | 1 | 0 | 0x822b97c8 | no | `~ObjRefConcrete<RndPartLauncher>` (survivor map @ 0x822b96a8) |
| 16 | 368 | 1 | 1 | 0 | 0x822a4fb8 | no | `vector<ObjPtr<RndTex>>::_M_fill_insert` |
| 17 | 364 | 2 | 1 | 1 / 232 | 0x823d14c0 | no | `list<GamerAwardStatus*>::insert` |
| 18 | 356 | 1 | 1 | 0 | 0x823dcde0 | no | `operator>>(BinStream&, ObjVector<ObjOwnerPtr<Waypoint>>&)` |
| 19 | 296 | 1 | 1 | 0 | 0x823d14c0 | no | `list<RndDir*>::insert` — `Splash::ShowNext` |
| 20 | 156 | 1 | 1 | 0 | 0x823d14c0 | no | `list<Job*>::insert` |
| 21 | 152 | 1 | 1 | 0 | 0x82327878 | no | `operator<<(BinStream&, list<LayerDir::Layer>&)` |
| 22 | 132 | 1 | 1 | 0 | 0x823d14c0 | no | `list<PassiveMessage*>::insert` |
| 23 | 124 | 1 | 1 | 0 | 0x823d14c0 | no | `list<RndGroup*>::insert` |
| 24 | 120 | 2 | 1 | 1 / 408 | 0x827fa298 | no | `~ObjPtrList<UILabel>` (44 B of it `masked_equal`) |
| 25 | 116 | 1 | 1 | 0 | 0x823d14c0 | no | `list<SynthPollable*>::insert` |
| 26 | 116 | 1 | 1 | 0 | 0x824ce130 | no | `list<WorldDir::BitmapOverride>::insert` (survivor map @ 0x822b55e0) |
| 27 | 108 | 1 | 1 | 0 | 0x823d14c0 | no | `list<Voice*>::insert` |
| 28 | 84 | 1 | 1 | 0 | 0x822a7f60 | no | `list<OldMatOption>::list(const list&)` |
| 29 | 56 | 1 | 1 | 0 | 0x823d14c0 | no | `list<ContentMgr::Callback*>::insert` |
| 30–42 | 0 | 0 | 0 | 13 rows / 7,152 B partial only | mixed | — | `list<CharData*>` (1,352 B partial), `~vector<ObjVector<Lod>>` (1,440), `~vector<OldColorOption>` (1,120), `_Deque_base<RangeShift>` (868), `list<NetLoaderRef>::insert` (696), `list<FaderTask*>` (444), `list<UIComponent*>` + `list<UITrigger*>` (the 336 B set row), … |

Σ full = **14,512 B / 51 rows**; 50 of 51 rows `masked_equal == False`.

## 4. What this buys and what it does not

* **Upper bound on the whole FCNP-restore lever: 14,512 B = 0.142 pp of
  `total_code`** (+336 B if the set row's two spellings both restore). That is the
  most any FCNP adjudication campaign can move `matched_code`, before evidence.
* 24 of 29 candidates share one survivor and one chain (GK's
  `_M_create_node<list<Dep*>>` → `MemOrPoolAlloc` → `PoolAlloc` → operator-new
  thunk), so the marginal cost per spelling is low — but each is still adjudicated
  and restored **one at a time with its own evidence** (Phase 2 doc).
* #12 fails criterion 2 by construction (our spelling has its own 100-byte body at
  `0x82749630`, same size as the survivor's); expected decline + map-lane filing.
* The enum (#1) and `int` (#13) elements are the interesting cases: a 4-byte
  trivially-copyable element gives the same `stw` node copy as a pointer, so a
  genuine retail fold is plausible, but it is exactly the kind of claim the chase
  must settle, not the price list.

## 5. Filed leads (not this lane's scope)

**FCNP spelling charged against a NON-member retail name** — 44 pairs / 82 rows /
12,712 B fully clearable; a *new* alias claim at the retail name's address in each
case, to be adjudicated on retail bytes like any other. Top pairs (ours → retail):

| B | rows | ours → retail |
|---:|---:|---|
| 1,080 | 1 | `vector<GemInProgress>::_M_fill_insert` → `vector<MidiParser::Note>::_M_fill_insert` |
| 876 | 8 | `~vector<ObjPtr<UIColor>>` → `~vector<String>` |
| 856 | 2 | `list<ScriptTask::Var>::insert` → `list<MsgSource::EventSink>::insert` |
| 736 | 1 | `list<PanelRef>::insert` → `list<CharBone::WeightContext>::insert` |
| 608 | 1 | `list<AwardEntry>::insert` → `list<CharBone::WeightContext>::insert` |
| 608 | 5 | `__destroy_range<TrackChannels*>` → `__destroy_mv_srcs<vector<Vector3>*>` |
| 560 | 3 | `~vector<FilePath>` → `~vector<String>` |
| 492 | 4 | `~vector<ObjPtr<EventTrigger>>` → `~vector<String>` |

⚠ Several of these pair an element type of one size with a survivor of another
(`ScriptTask::Var` vs `MsgSource::EventSink`); some will be wrong-instantiation
source bugs rather than folds. The `~vector<X>` → `~vector<String>` family (16+8+7+4
sites) is the largest single shape and is worth a lane of its own.
