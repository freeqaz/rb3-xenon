# W16-GV predictions — FCNP-tail install (2026-09-30)

Written and committed **before** running `tools/ab_measure.py`, per the lane's
standing protocol: a prediction recorded after the measurement is not a
prediction.

## What was installed

18 admitted pairs from the W16-GQ FCNP-nonmember scout's 44-pair population
(rows 14, 16-44 minus the 12 declined below), across **8 group changes** in
`scripts/symbol_aliases.json`:

- 3 extensions of pre-existing groups (rows 20/26/27/37 -> `0x823d6fa8`;
  rows 17/18/19/21/22/28/32/38 -> `0x822d8cc0`; row 36 -> `0x82b9b590`)
- 5 brand-new groups (row 14 -> `0x827e16a0`; row 24 -> `0x82634690`;
  row 29 -> `0x82370c80`; row 30 -> `0x824071e0`; row 34 -> `0x8251f060`)

Pairs #1, #3, #11, #12 (declined by W16-GQ) were left untouched. Rows
16/23/33/25/31/35/39/40/41/42/43/44 were re-adjudicated by me and declined
on their own merits (see the per-pair table in
`docs/decomp/W16GV_FCNP_TAIL_2026-09-30.md`).

## Price-table ceiling for the 18 admitted rows

From the frozen W16-GQ scout price table (`full_bytes` / `full_rows` /
`full_units` fields, cross-checked against
`/home/free/tmp/w16gq_nonfcnp_full_table.json`):

| row | ours (short) | retail (short) | call sites | full_bytes | full_bytes_masked | unit(s) |
|---|---|---|---:|---:|---:|---|
| 14 | insert<list<BoneState@BandCharacter>>> | insert<list<MeshInstance>>> | 2 | 364 | 0 | BandCharacter |
| 17 | ObjPtr<Sequence>::~vector | ~vector<String> | 1 | 272 | 0 | Sequence |
| 18 | ObjPtr<RndGroup>::~vector | ~vector<String> | 1 | 260 | 0 | BandCrowdMeter |
| 19 | ObjOwnerPtr<Waypoint>::~vector | ~vector<String> | 1 | 248 | 0 | Waypoint |
| 20 | insert<OrganizedFileMerger@FileMergerOrganizer> | insert<list<WeightContext@CharBone>>> | 1 | 208 | 0 | FileMergerOrganizer |
| 21 | ObjPtr<RndTex>::~vector | ~vector<String> | 2 | 208 | 0 | OutfitConfig |
| 22 | ObjOwnerPtr<RndTransformable>::~vector | ~vector<String> | 1 | 200 | 0 | CharBonesMeshes |
| 24 | __destroy_range<pair<vector<int,...>,int>> | __destroy_range<DistEntry> | 2 | 176 | 64 | SetlistMergePanel |
| 26 | insert<pair<RndMultiMesh*,Instance@RndMultiMesh>> | insert<list<WeightContext@CharBone>>> | 1 | 156 | 0 | CameraShot |
| 27 | insert<pair<Symbol,Symbol>> | insert<list<WeightContext@CharBone>>> | 1 | 156 | 0 | AccomplishmentProgress |
| 28 | ObjPtr<SeqInst>::~vector | ~vector<String> | 1 | 144 | 0 | Sequence |
| 29 | list<Job*>::list(const&) | list<Dep@CharPollableSorter*>::list(const&) | 1 | 140 | 0 | JobMgr |
| 30 | insert<list<Collision@RndDrawable>>> | insert<list<DecompressTask>>> | 1 | 136 | 0 | Draw |
| 32 | ObjPtr<Object@Hmx>::~vector | ~vector<String> | 2 | 128 | 44 | DirUnloader |
| 34 | insert<list<CallbackFile@ContentMgr>>> | insert<list<DataNode>>> | 1 | 120 | 0 | ContentMgr |
| 36 | _Param_Construct<Gem> | _Param_Construct<Entry@LocalePanel>/Char3D | 1 | 96 | 0 | GemManager |
| 37 | insert<ScreenParams@Splash> | insert<list<WeightContext@CharBone>>> | 1 | 96 | 0 | Splash |
| 38 | ObjOwnerPtr<CharClip>::~vector | ~vector<String> | 1 | 88 | 0 | CharEyes |
| **total** | | | **22** | **3,196** | **108** | **17 distinct units** |

## Predicted deltas (with reasoning, per the ledger's own established mechanism)

This is the same mechanism sized whole-binary by lane ALIASAUDIT-1/ALIAS-2 and
re-confirmed by the PATCH-LIVE ablation: adding a `symbol_aliases.json`
membership makes objdiff's `SymbolEquivalences` forgive a relocation-**name**
diff at qualifying call sites. That is a `fuzzy_match_percent` / `matched_code`
(bytes) effect, **not** a `match_percent_normalized` (function-count) effect,
and it is a *different* code path from the funclet-byte-signature pairing that
`masked_equal_functions` counts.

- **`matched_functions` (mpn-based count): predicted Δ = 0.** Under
  `name_check`, `mpn` already excludes arg-only (relocation-name) penalties for
  vetted wrong-callee-shaped diffs; the whole-binary ablation of the entire
  alias mechanism (1,528 groups / 818,416 B) measured `matched_functions`
  **exactly +0**. No row in this batch is expected to break that pattern.
- **`matched_code` (fuzzy-based bytes): predicted Δ ≈ +3,088 to +3,196 B.**
  3,196 B is the scout's **ceiling** price (all 22 call sites, if every one
  reaches `fuzzy == 100` and every byte of its owning row counts). 108 B of
  that (rows 24 and 32) is flagged `full_bytes_masked` in the frozen table,
  meaning those two rows' bytes may already be credited via the *other*
  (funclet-signature) forgiveness path, in which case the alias buys no
  *additional* bytes there. I am not claiming which figure is correct —
  per the project's own measurement doctrine, "never trust a displayed price;
  measure" — this range is the prediction, `ab_measure.py` is the check.
- **`masked_equal_functions`: predicted Δ = 0.** This counter increments on
  funclet-byte-signature pairing, a mechanism untouched by adding a name
  alias. If this is wrong, it falsifies my understanding of the mechanism and
  must be reported as a surprise, not quietly reconciled.
- **Units**: no unit is predicted to newly reach 100% AT_100 membership from
  this batch alone (each affected row sits inside a much larger multi-function
  unit); Δ on unit-AT_100 count predicted 0, to be confirmed by the same
  set-diff method DT-3 established.

## Honesty note

The 3,196 B figure is a **scout-table ceiling**, not a measured delta — it was
computed before any chase/adjudication and does not know whether a given row's
other (non-aliased) mismatches would still block it from `fuzzy == 100`. The
actual number comes from `tools/ab_measure.py --worktree /home/free/tmp/wt-w16-gv
--from-dirty`, run next.
