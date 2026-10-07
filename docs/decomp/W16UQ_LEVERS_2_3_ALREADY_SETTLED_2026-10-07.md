# W16-UQ: levers 2 and 3 of CAMPAIGN_STATE_2026-10-07 were already settled (2026-10-07)

Branch `w16-uq`, worktree `~/tmp/wt-w16uq`, base `59834d564` (W16-UP merged).

The brief asked for §6 levers 2 and 3 of `CAMPAIGN_STATE_2026-10-07.md`: the 33 wrong-callee rows
(2,812 B) and the 4 in-scope no-record source rows. Those levers were taken earlier the same day:

- **W16-SG** (`c83fa7001`, merged `41af27809`) settled all 37 rows on retail bytes for +53 fns / +6,568 B. It
  left 36 of them at 100 and deferred `SuperFormatString::SuperFormatString`.
- **W16-TH** (`8b668aca1`, doc `W16TH_LEVER2_SMALL_BEHAVIOUR_ROWS_2026-10-07.md`) took that ctor from 94.33 to
  97.96 and recorded a codegen stop reason.

The 10-07 doc predates both lanes; 300 commits have landed since its snapshot (`24d250690`).

So this lane did two things:
1. It checked each of the 37 rows against the current tree, keyed on the row's **retail address**, not its name.
   Names are unreliable here because W16-SG renamed most of these rows.
2. It fixed the one residue it found in the same family: the two MeshAnim rows below, +2 fns / +276 B.

## 1. Every row, now

Method:
- Each row's name comes from `~/tmp/w16sd-gap/dispo_rows.json` and `dispo_via_rows.json` (dispositions `06` and
  `16`, in scope).
- Its retail address comes from `scripts/target_symbol_map.json` at `24d250690`.
- The table gives the name main's map has at that address now, and that row's `fuzzy_match_percent` in this
  worktree's `report.json` (built at `59834d564`).

A name change in the "now named" column is a W16-SG correction of a map-name error. Most were masked-`b` false
matches on four-byte thunks, or STL helpers spelled with the wrong instantiation.

| row at 10-07 (unit) | B | then | retail addr | now named | unit now | now |
|---|---:|---:|---|---|---|---:|
| `list<RndDrawable*>::_M_splice_insert_dispatch` (BandCamShot) | 156 | 99.74 | `0x822b6880` | `list<BandCamShot::Target>` splice | BandCamShot | 100 |
| `??_EBandSong@@WBA` (BandCharacter) | 8 | 97.50 | `0x8227b038` | same, folded onto `??_GSong` | BandCharacter | 100 |
| `PatchSticker::MakeLoader` (PatchDir) | 92 | 98.70 | `0x822738d8` | same (source fix) | PatchDir | 100 |
| `_Destroy<EventAnim::EventCall>` (PatchDir) | 4 | 95.00 | `0x82278a18` | `~hash_map<Symbol,vector<PatchSticker*>>` | PatchDir | 100 |
| `??_DRndPollable` (BandRetargetVignette) | 8 | 97.50 | `0x822c6988` | `~pair<Object*,CharPollableSorter::Dep>` | BandRetargetVignette | 100 |
| `LocaleChunkSort::FastSort<3>` (StringTable) | 72 | 79.72 | `0x827c9540` | same (source fix) | StringTable | 100 |
| `??__FsConditionalTimersEnabled` (Timer) | 12 | 98.33 | `0x82c4a108` | `??__FgDecompressionQueue` | ChunkStream | 100 |
| `list<MidiParser*>` copy ctor (Song) | 116 | 97.24 | `0x8230ebe0` | `list<ContentPoolMapping>` range ctor | SongSectionController | 100 |
| `SuperFormatString::SuperFormatString` | 1,608 | 94.33 | `0x82bbabf8` | same | SuperFormatString | **97.96** |
| `list<CharClip*>::sort<ObjNameSort>` (TrackWidget) | 4 | 95.00 | `0x827e2998` | `~list<MeshInstance>` | TrackWidget | 100 |
| `RndAnimatable::operator delete` (EventAnim) | 4 | 95.00 | `0x824c9b78` | `~list<EventAnim::KeyFrame>` | EventAnim | 100 |
| `__adjust_heap<GameGem>` (GameGemList) | 264 | 99.82 | `0x8278ce50` | same (source fix, `GameGem::operator<`) | GameGemList | 100 |
| MeshAnim `Key<vector<…>>` `_M_erase` | 112 | 99.64 | `0x82442df8` | `Key<vector<Color>>` `_M_erase` | MeshAnim | 100 |
| MeshAnim `_M_insert_overflow_aux` (Vector2) | 324 | 99.75 | `0x8246fb50` | `Key<vector<Color>>` | MeshAnim | 100 |
| MeshAnim `_M_insert_overflow_aux` (Color) | 324 | 99.69 | `0x8246fce8` | `Key<vector<Vector2>>` | MeshAnim | 100 |
| MeshAnim `_M_fill_insert` (Vector2) | 108 | 99.81 | `0x82470648` | `Key<vector<Color>>` | MeshAnim | 100 |
| MeshAnim `_M_fill_insert` (Color) | 108 | 99.81 | `0x824706b8` | `Key<vector<Vector2>>` | MeshAnim | 100 |
| MeshAnim `resize` (Vector3) | 124 | 99.84 | `0x824707b8` | `Key<vector<Vector2>>` | MeshAnim | 100 |
| `_Destroy<LocalePanel::Entry>` (UI) | 4 | 95.00 | `0x828043a8` | `~list<UIResource*>` | UI | 100 |
| `_Destroy<WorldDir::BitmapOverride>` (Character) | 4 | 95.00 | `0x822a6b20` | `~list<OldMatOption>` | OutfitConfig | 100 |
| `_Destroy<Character::Lod>` (Character) | 4 | 95.00 | `0x82371ab0` | `~ObjVector<Character::Lod>` | Character | 100 |
| `uninitialized_copy<char>` (Character) | 4 | 95.00 | `0x823748d8` | `~ObjVector<ObjVector<Lod>>` | Character | 100 |
| `__destroy_range_aux<vector<short>>` (VorbisReader) | 100 | 99.80 | `0x82773e70` | `__destroy_range_aux<vector<RangedData…>>` | SongData | 100 |
| `_Destroy<BSPFace>` (Mesh) | 4 | 95.00 | `0x8241b058` | `~list<Plane>` | Mesh | 100 |
| `_Copy_Construct<vector<ushort>>` (Mesh) | 60 | 99.67 | `0x8241bb98` | `_Param_Construct<list<int>>` | Mesh | 100 |
| `RndMesh::Vert::operator delete` (Mesh) | 4 | 95.00 | `0x8241e488` | `~list<BSPFace>` | Mesh | 100 |
| `vector<ObjOwnerPtr<Waypoint>>::_M_erase` (Waypoint) | 116 | 99.83 | `0x822cb828` | `vector<ObjPtr<EventTrigger>>::_M_erase` | BandStarDisplay | 100 |
| `vector<ObjPtr<SeqInst>>::resize` (Waypoint) | 128 | 99.69 | `0x823dcca0` | `vector<ObjOwnerPtr<Waypoint>>::resize` | Waypoint | 100 |
| `vector<Key<Weight>>::_M_fill_insert` (Morph) | 108 | 99.63 | `0x82398798` | `vector<CharIKHand::IKTarget>` | CharIKHand | 100 |
| `CharHair::CharHair` | 476 | 99.92 | `0x823aa6f0` | same; vbtable data fold with PreloadPanel's | CharHair | 100 |
| `_Destroy<CamShotCrowd>` (CameraShot) | 4 | 95.00 | `0x824c5ae0` | `~ObjVector<CamShotCrowd>` | CameraShot | 100 |
| `_Destroy<MidiChannel::Note>` (Sfx) | 4 | 95.00 | `0x8271c1d8` | `~ObjVector<SfxMap>` | Sfx | 100 |
| `vector<vector<Vector3>>::_M_erase` (UIList) | 292 | 99.86 | `0x82775b70` | `_M_erase<vector<RangedData<I>>>` | SongData | 100 |
| `ADSR::operator delete` (ADSR) | 4 | 95.00 | `0x82700af8` | `~list<MidiChannel::Note>` | Sfx | 100 |
| `_Destroy<CharEyes::EyeDesc>` (CharEyes) | 4 | 95.00 | `0x82388538` | `~ObjVector<CharInterestState>` | CharEyes | 100 |
| `_M_allocate_and_copy<XUSER_ACHIEVEMENT>` (Achievements) | 100 | 99.80 | `0x82772208` | `_M_allocate_and_copy<pair<float,float>>` | SongData | 100 |
| `__ucopy_ptrs<int*>` (CharCollide) | 4 | 95.00 | `0x822c9038` | `~ObjVersion` | BandLeadMeter | 100 |

Result: 36 of 37 at 100. The other row is `SuperFormatString`'s ctor, at 97.96 with W16-TH's recorded stop
reason: three codegen residues (int/float tail merge direction, the escape-path reload, and one store order), and
the permuter is off by directive. I did not reopen it. W16-TH's grid covered seven spelling axes plus all 720 local
declaration orders, so this lane had no new axis to bring.

## 2. The residue: RndMeshAnim's `Key<vector<Vector3>>` helpers (+2 fns / +276 B)

To check that the family was closed, I listed every sub-100 row in the units these rows touch. Two MeshAnim rows
charged a `Key<vector<Color>>` callee where we call the `Key<vector<Vector3>>` spelling:

| row | B | before | charge |
|---|---:|---:|---|
| `vector<Key<vector<Vector3>>>::_M_clear` | 112 | 99.82 | `bl __destroy_range_aux<Key<vector<Color>>>`, ours Vector3 |
| `RndMeshAnim::~RndMeshAnim` | 164 | 99.76 | 2 × `bl ~vector<Key<vector<Color>>>`, ours Vector3 |

How they arose: W16-SN (`c45656e7e`) relabelled `0x827ebad0` from the Vector3 `destroy_range_aux` to
`MidiParser::VocalEvent`'s, because retail's body there is VocalEvent's, and withdrew the Vector3 label. That left
the Vector3 spelling with no retail home.

Retail bytes:
- `tools/retail_callers.py 8246eb78`: 2 sites. `0x8246ef74` is inside `_M_clear` at `0x8246ef38`, whose alias
  group already folds the Color and Vector3 `_M_clear`. `0x8246f248` is inside `~vector<Key<vector<Color>>>` at
  `0x8246f200`.
- `tools/retail_callers.py 8246f200`: 3 sites, `0x8247036c`, `0x8247037c` and `0x82470384`, all in `~RndMeshAnim`.
  RndMeshAnim destroys two Vector3 key vectors (points, normals) and one Color key vector; the Vector2 one goes
  elsewhere. Our `~RndMeshAnim` calls the Vector3 spelling at the first two of those sites (objdiff idx 15 and 17).
- Retail has 5 masked twins of `destroy_range_aux` for our 6. The Color and Vector3 instantiations are one body in
  retail.

`tools/icf_pair_adjudicate.py --chase`, survivor against every twin:

| our spelling | PROVEN against | REFUTED against |
|---|---|---|
| `__destroy_range_aux<Key<vector<Vector3>>>` | `Key<vector<Color>>` (`0x8246eb78`) | `pair<vector<int>,int>`, `Key<vector<Vector2>>`, `StreakList`, `TrackChannels` |
| `~vector<Key<vector<Vector3>>>` | `Key<vector<Color>>` (`0x8246f200`) | `Key<vector<Vector2>>`, `Key<ObjectStage>`, `IKTarget`, `CharInterestState`, `ObjVersion`, `TypeCreatorPair`, `pair<vector<int>,int>` |

FLAT T1 is REFUTED in both cases only because the relocation names differ (Vector3 vs Color helpers). The chase
resolves those slots to folds.

Change (`d6c190283`):
- `scripts/symbol_aliases.json`: the Vector3 `destroy_range_aux` is added to group `0x8246eb78`, and a new group
  `0x8246f200` is created (survivor Color `~vector`, folded Vector3). Each has an `admitted` record with the
  evidence above.
- `scripts/alias_callee_names.json`: the build's `CHECK ALIAS CALLEE NAMES VS MAP` edge refused both groups as
  "not recorded" until `tools/alias_callee_name_drift.py --reprove --write` re-proved them (2 PROVEN).

**A/B** (`tools/ab_measure.py --from-dirty`, ruler `name_check`, both legs at a split fixed point):

```
leg A: matched=55036 masked=25227 honest=29809 code%=59.398834
leg B: matched=55038 masked=25227 honest=29811 code%=59.401530
Δmatched=+2  Δhonest=+2  Δcode_bytes=+276  Δcode%=+0.002696pp
```

Predicted +2 / +276 B. Measured +2 / +276 B. The row diff of the two legs' `report.json` shows only the two rows
above moved, both to 100, and no row went down.

`ab_measure` flagged **ALIAS_SUSPECT**: `name_check` rose while the `none` control stayed flat on a map-only patch.
That is what any real fold looks like on this ruler, so the flag cannot clear or refute this change. The case rests
on the retail call sites and the unique PROVEN twin above.

## Not done

- **`SuperFormatString`'s ctor.** Not reopened. The stop reason is W16-TH's.
- **`OggMap::OpenMogg`.** The fifth no-record row is VIA-DC3, so it is outside this brief. W16-TH also recorded
  its stop reason.
- **The other sub-100 rows in these units.** Not opened, because they belong to other buckets: EH funclets at
  99.5, `list<RndMultiMesh::Instance>::operator=` and similar.
- **Merge and push.** Left to the coordinator.

## Native gate

The change is map/alias only and touches no `src/`. I ran the gate anyway as the last action, because the brief
requires it:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Run on `d6c190283`, log `~/tmp/w16uq_native_gate.log`.
