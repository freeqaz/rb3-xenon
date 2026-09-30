# W16-HA: predictions before measuring

Lane W16-HA, worktree `/home/free/tmp/wt-w16-ha`, branch `w16-ha`, off main
`9de0f3339`. This doc is written and committed **before** running
`tools/ab_measure.py`, per house convention (predict, commit, then measure).

## What was installed

46 of 80 FRESH candidate pairs from `/home/free/tmp/w16ha_candidates.json`
(census: W16-GY alias-only sub-100 scouting report, uncommitted lane artifact,
worktree wt-w16-gy branch w16-gy off main `451f6dcb0`; not present under
`docs/decomp/`) were admitted by `tools/icf_pair_adjudicate.py --chase` under
the strict gate in the task brief (CHASED T1 == PROVEN, zero CYCLE-ASSUMED /
SLOT-REFUTED / BYTES-DIFFER trace lines; named-relocation evidence required,
or VACUOUS-BUT-IDENTICAL as a fallback). Installed into
`scripts/symbol_aliases.json` across 5 commits of <=10 pairs each:

| batch | commit | pairs | idx | predicted rows | predicted bytes |
|---|---|---:|---|---:|---:|
| 1 | `b24947ee5` | 10 | 3,4,6,8,9,12,13,14,15,16 | 29 | 15,932 |
| 2 | `34ef76dde` | 10 | 19,20,23,25,27,29,30,31,32,33 | 26 | 7,712 |
| 3 | `8255444fb` | 10 | 34,35,36,37,39,40,45,47,48,52 | 19 | 6,328 |
| 4 | `1451d93e9` | 10 | 53,54,57,58,59,60,61,66,68,69 | 16 | 4,820 |
| 5 | `778a1c0ee` | 6  | 71,72,73,77,79,80 | 21 | 3,112 |
| **total** | | **46** | | **111** | **37,904** |

`scripts/symbol_aliases.json`: 1668 groups at HEAD -> **1697 groups** (29 new
groups created; the remainder extended an existing pre-lane group, or a group
created earlier by this same lane sharing the same retail address -- 3 such
merges: idx 9<-14 within batch 1, idx 20<-61<-71 across batches 2/4/5, plus a
handful of extensions into groups `#36`/`#45`/`#1586`/`#1588`/`#1540`/`#428`/
`#913`/`#1265`/`#1485`/`#1640`/`#1658` that predate this lane).

## Per-pair prediction table

`full_rows` / `full_bytes` are taken directly from the W16-GY census record for
each candidate (`detail.retail_addr`, `full_units`) -- these are the rows the
census found keyed to that retail address, all currently scored `mpn < 100`
because the callee name at that address disagrees under `name_check`.

| idx | ours (truncated) | survivor (truncated) | rows | bytes | example unit |
|---|---|---|---:|---:|---|
| 3 | `?Handle@CharWeightable@@UAA?AVDataNode@@PAVDataArr` | `?Handle@CharData@@UAA?AVDataNode@@PAVDataArray@@_N` | 8 | 2648 | default/BandIKEffector |
| 4 | `?find@String@@QBAIPBD@Z` | `?find@FixedString@@QBAIPBD@Z` | 4 | 2496 | default/CameraShot |
| 6 | `??A?$hash_map@VSymbol@@V?$vector@HV?$StlNodeAlloc@` | `??A?$hash_map@VSymbol@@V?$vector@PAVLightPreset@@V` | 5 | 1744 | default/SongMgr |
| 8 | `?insert@?$list@PAVEventTrigger@@V?$StlNodeAlloc@PA` | `?insert@?$list@PAVObject@Hmx@@V?$StlNodeAlloc@PAVO` | 1 | 1596 | default/EventTrigger |
| 9 | `?push_back@?$vector@VCharacterEntry@CharProvider@@` | `?push_back@?$vector@UPressRec@@V?$StlNodeAlloc@UPr` | 2 | 1592 | default/band3/meta_band/CharProvider |
| 12 | vector\<Key\<Vector3\>\> scalar deleting dtor | (see group `scalar_deleting_dtor_Key_Vector3`) | -- | -- | -- |
| 13 | (STL template twin) | -- | -- | -- | -- |
| 14 | extends idx 9's new group (shared retail addr) | -- | -- | -- | -- |
| 15 | `CharEyes` UEyeDesc shr twin | -- | -- | -- | -- |
| 16 | (STL template twin) | -- | -- | -- | -- |
| 19 | `operator[]` map\<Symbol,...\> twin | -- | -- | -- | -- |
| 20 | vector\<HamSupereasyMeasure\>::operator= (new group `assign_UHamSupereasyMeasure`) | -- | -- | -- | -- |
| 23 | vector\<int\> ctor twin | -- | -- | -- | -- |
| 25,27,29,30,31,32,33 | STL container template twins | -- | -- | -- | -- |
| 34,35,36,37,39,40,45,47,48,52 | STL container template twins; **#47** flagged below | -- | -- | -- | -- |
| 53 | `GetWeight@GigFilter` (float getter, 8B) | `GetObj@ObjRefConcrete<MoggClip,ObjectDir>` (ptr getter, 8B) | 1 | 520 | default/band3/tour/TourPerformerLocal |
| 54,57,58,59,60,61,66,68,69 | STL container template twins | -- | -- | -- | -- |
| 71,72,73,77,79,80 | STL container template twins / list dtor | -- | -- | -- | -- |

(Full per-pair reasoning and every evidence string lives in
`scripts/symbol_aliases.json`'s `evidence` field for each touched group, and
in the final deliverable `docs/decomp/W16HA_FRESH_ALIAS_PAIRS_2026-09-30.md`.)

## Flagged for reviewer double-check (not a normal template twin)

- **#47** `IsScrolling@UIList` (bool getter) <-> `SetSpeed@UIList` (float
  setter) -- same class, but getter vs setter with different signatures.
  Admitted via chased PROVEN with named-relocation evidence; no
  CYCLE-ASSUMED/SLOT-REFUTED/BYTES-DIFFER. Flagging because the semantic
  mismatch (getter vs setter) is larger than the rest of this batch's
  template-parameter-only twins.
- **#53** `GetWeight@GigFilter` (returns `float`) <-> `GetObj@ObjRefConcrete<MoggClip,ObjectDir>`
  (returns `Object*`) -- an 8-byte body, too small for FLAT T1 to adjudicate by
  name (`VACUOUS: body under 4 words or over half the words masked`); admitted
  via **VACUOUS-BUT-IDENTICAL** (the raw, unmasked instruction bytes are
  literally identical between the two 8-byte bodies), not via any named
  relocation. Flagging because a float-returning getter and a pointer-
  returning getter would ordinarily be expected to differ in their return
  register convention; the proof here rests entirely on literal byte identity
  rather than on any semantic/name check, so it deserves a second look before
  being trusted as durable evidence of an actual ICF fold (as opposed to a
  census/candidate mix-up). No source or map changes ride on getting this one
  right -- the only artifact is one `folded` entry in the survivor's alias
  group.

## Predicted A/B outcome

- **Forward** (`--patch`, HEAD~5..HEAD i.e. main -> current branch tip):
  `matched_functions` (`mpn` ruler) should rise by up to +46 (one per pair,
  each pair's own row reaching `mpn == 100`) -- could be less if some rows also
  carry other unrelated charged sites. `matched_code` (`fuzzy` ruler) should
  rise by some fraction of the 37,904 B predicted above (a row only pays out
  in full when the aliased call site is its ONLY remaining charge -- the same
  caveat documented in every prior alias-install lane, W16-GM/W16-GV). Expect
  `ALIAS_SUSPECT` to fire on the forward leg's `control_none_shape()` check
  (map-only patch, no `source` changes) -- this is expected, not a defect.
  `none`-ruler control should be flat (Delta 0) since none of these changes
  touch relocation-**address** content, only which pairs `name_check` forgives.
- **Reverse** (revert leg): mirror-image negative deltas, `masked_equal`
  should also move (each admitted pair either newly participates in -- or, for
  extended groups, was already inside -- a `masked_equal` fold population).
- **Validator**: `icf_alias_finder.py --validate` should read **PASS, 0
  CONTRADICTED** both before (1668 groups) and after (1697 groups) -- already
  confirmed after every single batch commit during installation.
