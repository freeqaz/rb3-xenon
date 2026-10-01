# W16-IF — the hamobj / gesture / flow pins are RB3 code under DC3 stand-in names (2026-10-01)

Branch `w16-if`, rebased onto main `78cb928ef`. Lever 3 of
`CAMPAIGN_STATE_2026-09-30.md` §4 (branch `coord-0930-leverage`): units sourced from
`src/system/{hamobj,gesture,flow}` hold RB3-retail code although RB3 has no Kinect /
dance code. W17-HCT (HamCamTransform = OutfitConfig's code) and W17-PRAC (230 DC-only
classes with no retail RTTI) found the same thing in two other places.

## Result

One `tools/ab_measure.py --patch` over `main..w16-if` (map, splits, source, configgen),
with the lane's `symbols.txt` over-carve hunk committed into leg A (`ab_measure` refuses
`symbols.txt` patches; W16-HZ's recipe). `name_check` ruler, both legs settled and at a
split fixed point (0 extra re-splits), leg B recompiled 60 TUs. Run dir archived to
`~/tmp/w16if/ab_run/`.

| | leg A (main `78cb928ef`) | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 46,895 | 47,019 | **+124** |
| masked_equal | 23,727 | 23,759 | +32 |
| honest | 23,168 | 23,260 | **+92** |
| matched_code_percent | 46.266563 | 46.395110 | **+0.128547 pp (+13,172 B)** |
| fuzzy_match_percent | 56.107414 | 56.317005 | **+0.209591 pp** |

- Leg B equals the branch's in-tree build exactly (47,019 / 46.395110).
- **Predicted vs measured.** The in-tree step deltas on the pre-rebase base summed to
  +123 / +13,060 B (step 1 +15 / +2,564; CharKeyHandMidi +69 / +6,952; UnisonIcon +
  BandPatchMesh +8 / +208; Poll +1 / +40; Tambourine/BufFile +12 / +804; SampleZone +5 /
  +1,144; Singer +13 / +1,348; TrackPanelDir 0 / 0). I expected slightly *less* after the
  rebase because main's W17-BPM3 had already taken the three BandPatchMesh rows; the
  measurement came out +1 / +112 B higher. I did not chase the difference (main also
  landed W16-IE's inline operator deletes and W17-ANON round 2 under these rows).
- Rows: **0 renamed addresses score lower** (checked by address: leg-A name's score vs
  leg-B name's score). By name, three 32–44 B EH funclets lose their masked pairing
  (`fn_826FA1A4` 100 → 0 after moving with TambourineManager::PostLoad; `fn_8227B5A0`,
  `fn_822775C4` 100 → 99.5 re-paired).
- Units at 100 (mpn) 252 → 251: CrazeHollaback and FlowTrigger completed because rows
  left them (DENOMINATOR_SHRANK); TambourineDetector, TourSavable and VocalScoreHistory
  fell off because re-homed rows joined their denominators below 100. Truer
  denominators, not lost matches.
- `none`-ruler control: +14,492 B, NOT_APPLICABLE (patch carries source).
- DC-only units (57 with rows): pinned **77,692 → 49,212 B** (28,480 B of retail code
  re-homed to its real TU), anonymous-at-0 **236 rows / 35,224 B → 129 / 15,044 B**.

## 1. What the pins are

Lane start (main `a95d5c525`): 59 units, **79,916 B** of pinned retail code, 26,256 B at
100, 37,324 B in anonymous rows at fuzzy 0. Every pinned block was attributed with
evidence that does not read the map: the non-DC units whose pins bracket it, the pinned
units of its retail `bl` callers and callees, the RTTI class of any vtable slot that
points into it, and the `.rdata` strings it forms (`~/tmp/w16if/attrib.py`,
`survey.py`). The dominant shape: **a DC block is a hole inside one real TU, bracketed on
both sides by that TU's pins at distance 0** — the DC unit only "owns" it because its obj
(often through a scatter `#include`) defines a byte-identical template instance under a
DC3 type name.

| DC unit | what its pins really were |
|---|---|
| MoveMgr (13,236 B, 56 anon) | **CharKeyHandMidi's whole TU** (0x822CF888–0x822D2BA8, no source existed), UnisonIcon's small methods, three BandPatchMesh `WorkVerts` bodies, a 12-B XDK thunk |
| DepthBuffer3D (8,468 B, 31 anon) | **Singer** (ctor, AddAmbiguousPart, its `vector<VocalScoreCache / VocalScoreHistory / SingerResultsData / AmbiguousData>` helpers under DepthBuffer3DAttachment / CameraManager / CharBones / FlowMathOp names), **TambourineManager** (13 members), VocalScoreHistory ×4, TambourineDetector, `BufFile` virtuals (Synth.obj), JumpInstance (StandardStream) |
| HamMove | MidiInstrument's `vector<SampleZone>` family under `HamMove::LocalizedName` names; generic `_Rb_global`/`map<int,float>` COMDATs (left) |
| FlowIf / FlowTrigger / FlowValueCase / FlowSwitchCase / FlowWhile / FlowOnStop / CrazeHollaback / CameraInput / FilterVersion | single overrides of real classes (CharIKHead/BandSongPref::SetType, RndFont/RetryAudioPanel::Handle, BandHighlight::SyncProperty, CharWeightSetter::Load, TourSavable ctor, CharPollGroup::SortPolls, …) |
| FlowNode | TrackPanelDir::ConfigureTracks (mapped as `__uninitialized_fill_n<DrivenPropertyEntry>`, 4.8) and Reset |

At the branch tip the DC-only units pin **49,212 B (from 77,692 B on main `78cb928ef`; 79,916 B at lane start)** (the rest re-homed by this lane or by
W17-ANON / W17-BPM3 on main in the meantime) and **15,044 B (129 rows)** of anonymous code at 0.

## 2. Identification passes (each rule fixed before its output was read)

| pass | method | rows |
|---|---|---:|
| 1 | neighbour-obj body rank: every anonymous row ≥ 32 B scored (graded ruler, scratch rename) against the unclaimed functions of the objs of the nearest non-DC unit on each side of its block and of its callers' units; accept **top ≥ 60 and top − runner-up ≥ 15**; then `tools/anon_proposal_adjudicate.py` (re-home mode): 57 → 34 SUPPORTED / 21 CONTRADICTED / 2 UNANCHORED | 30 + Singer::Singer |
| 2 | the same without the ≤ 2-definition filter (scatter includes had hidden Singer), templates excluded: 10 → 3 / 5 / 2 | 9 |
| CKHM | port, then rank every row of the new unit against our `CharKeyHandMidi.obj`; the two `sort<KeyboardKey*>` families by call graph | 55 |
| families | rank a DC block's rows (stand-in-named rows included) against the bracketing TU's obj; candidate names already present in the **target** obj excluded | 9 + 14 + 8 |
| by hand | small rows by caller + body (TambourineManager Start / SetPaused / GemHit / GemProcessed / Jump / PostDynamicAdd, UnisonIcon ×8, BufFile ×5 by RTTI slot, TrackPanelDir ×2) | ~25 |

Acceptance beyond the mechanical rule: a CONTRADICTED verdict was accepted only when its
sole contradiction is a known non-identity class (a retail-inlined `ObjPtr<T>` vtable, a
fold-named `??_8` vbtable) or a caller that itself carries a stand-in name (proved on
retail bytes in each case below). Refused: DC-only types (`list<SongSectionController::
PracticeSectionMapping>`), template twins without a caller witness, every `??_G`/`??_E`
conflict.

## 3. CharKeyHandMidi — a whole RB3 TU with no source

Retail evidence: strings `ik_object`/`first_spot`/`second_spot`/`is_right_hand`/
`fingers_up`/`fingers_down`/`run_test`/`end_test`, `StaticClassName@CharKeyHandMidi`,
RTTI vtables `.?AVCharKeyHandMidi@@` and `.?AV?$ObjPtr@VCharIKFingers@@…` whose slots
point into the block. The TU starts in BandScoreboard's pin (FindPreferredFinger,
IsBlackKey, SetName, the `ObjPtr<CharIKFingers>` virtuals) and ends where UnisonIcon
begins. `0x822D16F8` (`vector<Symbol>::push_back`, the 92-caller 4-byte push_back fold)
sits inside it and stays where it is.

`src/system/bandobj/CharKeyHandMidi.cpp` is new, from rb3-Wii's oracle with retail
winning where they differ — each site carries its retail address:
- `Highlight` is retail's shared empty `blr` (RndHighlightable slot 0 → `0x826C3888`); the
  debug draw stays under `HX_NATIVE`.
- `KeyFinger`: signed range test and a loop (oracle: unsigned test, five unrolled compares).
- `IsBlackKey`: linear compare chain (the oracle's switch compiles to a binary search).
- `Load`: two internal-linkage rev statics, altRev first (retail stores both halves through
  one base register — BandCamShot.cpp's shape), not Object.h's `BinStreamRev`.
- `OBJ_MEM_OVERLOAD_INLINE_DEL`: retail `NewObject` (0x8227B918) and `??_G` call
  `MemAlloc`/`MemFree` inline.
- `Poll`: retail builds the key layout from stack `Vector3` objects (a 16-B copy of the
  first spot updated in place, products in named vectors, never fused, whole-vector
  stores). Reshaped with `Scale`/`Add`/`+=`: **75.7 → 84.3**. The rest is component order
  inside `Subtract`/`Length` (scheduling) plus two fold-alias callee names.

`CharIKFingers` gains `SetFinger`/`ReleaseFinger` (retail 0x823B1190 / 0x823B1290, both
100) on its DC3-era member names, and friends CharKeyHandMidi. `symbols.txt`:
ReleaseFinger's straight-line body was split in three by dtk with no branch between the
pieces, so the over-carve merge cannot fire; its extent was set to 0x40 by hand and the
split holds it as a fixed point. Native: the TU is compiled from `native/CMakeLists.txt`
and `x7_band_stubs.cpp`'s empty virtuals are gone.

Unit: 12,748 B, **8,536 B at 100**. Stand-ins replaced: the `sort<int*>` and
`sort<CamShot**,NameSort>` families (the only retail caller is `CharKeyHandMidi::Poll`),
`CameraManager::OnRandomSeed` (= OnFingersDown), `vector<SpotlightEntry>` overflow.

## 4. Retail-byte adjudications worth keeping

- **CharBoneOffset::Handle = CharUpperTwist::Handle (ICF).** The `$4` thunk at 0x823C7328
  is in *both* classes' retail vtables (slot 6); the body at 0x823C7210 is the fold
  survivor and takes the thunk's spelling. Pass 1's CALLER_CONTRA was this thunk.
- **TambourineManager::PostDynamicAdd** is the 4-B `b Restart` at 0x826FB880, mapped as a
  stand-in `_Destroy_Moved<map<int,float>>`; renaming it lifted VocalPlayer::PostDynamicAdd
  99.9 → 100 and resolved Restart's contradiction.
- **BufFile::ReadAsync** at 0x826FBDB0 was an alias-group survivor under a stand-in
  `vector<map<int,float>>::_M_insert_overflow` (23.3); RTTI BufFile slot 3 → 100.
- **`??_GTextStream` has its own address.** 0x822716F8 stores vtable 0x82000E8C (COL
  `.?AVTextStream@@`), whose slot 0 points back at it; group 0x82BF6F58
  (`??_GCMemoryManagedUnknown`) listed it as folded — withdrawn as
  `RTTI_PROVES_OWN_ADDRESS` (W16-HZ's `??_GMic` precedent), measured 0 on every key.
- Alias survivors rebound from stand-ins to the identified spellings at 16 addresses
  (CKHM sorts ×5, SampleZone ×3, Singer ×7, BufFile ×1), stand-ins withdrawn as
  `SURVIVOR_WAS_A_DIFFERENT_FUNCTION`. Nothing pruned. Seven of the Singer groups
  already listed the Singer spelling as *folded* under the stand-in survivor.
- Over-carve heads named so the split merged the tail: FindPreferredFinger, GemHit,
  UnisonIcon::SetProgress.

## 5. Rebase

Main moved while the lane ran (W17-ANON round 2, W16-IE, W17-BPM3). Every step is a
script over move lists and name rows whose writers assert the old value / refuse an
existing key, so each conflicting commit was resolved by taking main's generated files
and re-running that step's operations. One overlap: **W17-BPM3 independently landed the
same three BandPatchMesh names and re-homes** step 3 had (SetMeshVertAndTwins 99.9,
`vector<PatchPair>::_M_clear_after_move` 99.8, TryAddFace 64.6) — that half was dropped
and the commit message says so. Everything else replayed with no collision.

## 6. Gates

- `tools/map_name_injectivity.py`: `OK: 31970 applied rows, 31969 distinct names, injective (+1 enumerated internal-linkage exception(s))`
- `tools/icf_alias_finder.py --validate`: `VALIDATE: PASS -- 1492 map-consistent, 255 tolerated (enumerated above), 0 contradicted, 1748 total` (it FAILED once mid-lane on the `??_GTextStream` fold, fixed in step 6, §4)
- `tools/native_build_gate.sh`: `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` (run on the rebased branch tip; CharKeyHandMidi.cpp is compiled natively and the x7 stub virtuals are gone)

## 7. Left, and where

- **`SingerResultsData` copy semantics.** HamNavList's 0x826F7CC8
  (`_M_fill_insert_aux<HamListRibbonDrawState>`, 100 against HamNavList.obj) is
  `_M_fill_insert_aux<SingerResultsData>` by its only caller, but ours scores 53.3:
  retail copies each 0x20-B element with `memcpy`, our hand-written word-copy
  `operator=` copies inline (the `RndText::Line` pattern W17-PRAC fixed). Changing it
  touches already-matched Singer rows; renaming without it costs 392 B. Left unrenamed.
- **`??_G` vs `??_E`.** PracticePanel, CharPollGroup: retail's 68-B body matches our `??_G`
  but a matched thunk calls it as `??_E`; CharUpperTwist: retail's `??_G` calls a
  different destructor than the map's `~CharUpperTwist` (0x8232A2C0, whose own caller is
  `??_GDialogDisplay`). Not touched.
- **FindPreferredFinger 92.4**: retail branches (`cmpwi`/`bgelr` to the shared return)
  where every spelling tried compiles to MSVC's branchless `max(x, 0)`.
- **TrackPanelDir ConfigureTracks 92.4 / Reset 89.5**, TambourineManager Handle 66.9 /
  LocalTambourineSoloEnd 60.9 / Jump 46.8, Singer ctor 74.8 (824 vs our 996 B): body
  divergences now pairable where they were anonymous.
- Stand-in names that pair at 100 and were not chased (accuracy only): HamMove's
  `_Rb_global` / `map<int,float>` COMDAT block, FlowCommand's ContentMgr block,
  DrivenPropertyEntry/FlowNode `vector<DrivenPropertyEntry>` rows inside TrackPanelDir.
- Still anonymous in DC units: 129 rows / 15,044 B, largest HamNavProvider (16 rows),
  FreestyleMoveRecorder (ClosetPanel/AssetStore region), RhythmDetector, FitnessFilter.

## Tools (scratch, `~/tmp/w16if/`, not committed)

`attrib.py` (per-block bracket/caller/RTTI/string attribution), `survey.py`, `nb.py` /
`nb2.py` (neighbour body rank), `rank.py` (rows × obj, target-obj names excluded),
`plan.py` / `apply_moves.py` (HV's range rule), `set_null.py`, `rename_map.py` (old-value
asserting map splice), `rebind_alias.py`.
