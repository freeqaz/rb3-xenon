# W16-HQ — 23 wrong-unit `.text` pins re-homed, 43 rows named on retail bytes (2026-09-30)

Branch `w16-hq`, rebased onto main `835ab0289`. Scope: `config/45410914/splits.txt` and
`scripts/target_symbol_map.json` only. No source was touched, including nothing under
`src/system/{bandobj,rndobj,char,rnddx9}`.

Input: the 25 "class R" rows from W16-HO (`~/tmp/w16ho/rehome_strong.json`,
`docs/decomp/W16HO_ANON_ROWS_CLASSIFIED_AND_REPAIRED_2026-09-30.md` §6). Also two engine mis-pins:
`0x823b99f8` in FlowValueCase's pin and `0x823c84d8` in Screenshot's.

## Result

One `tools/ab_measure.py --patch` run over `835ab0289..w16-hq` on a clean worktree, `name_check`
ruler, both legs settled and at a split fixed point, 0 recompiles:

| | matched | masked_equal | honest | matched_code | code% | fuzzy |
|---|---:|---:|---:|---:|---:|---:|
| leg A (main) | 45,183 | 23,495 | 21,688 | — | 43.435623 | 52.531937 |
| leg B (branch) | 45,217 | 23,513 | 21,704 | — | 43.467320 | 52.662395 |
| **Δ** | **+34** | +18 | **+16** | **+3,248 B** | +0.031697 pp | **+0.130458 pp** |

The in-tree build predicted +34 fns / +3,248 B before the rebase, and the A/B measured exactly that.
All 43 named rows were at fuzzy 0 in leg A. After: 14 of them read 100 (2,240 B), and the rest
read 19.9–99.9.

Units at 100% (mpn) went from 220 to 216. VocalTrainerPanel, ClosetPanel, MultiSelectListPanel and
QuestFilterPanel each gained real rows of their own that are not at 100 yet. That is a truer
denominator, not a regression.

Four 32-byte funclets lost a byte-signature pairing, 256 B in total. They paired by byte signature
in their old unit and do not re-pair in the new one:
- SongSort `0x825c0ae4` and `0x825c0b04`: 100 → 92.5.
- MultiSelectListPanel `0x82627918` and `0x82627938`: 100 → 92.5.
- AccomplishmentTourConditional `0x825a4488`, `0x825a46c4`, `0x825a46e4` and `0x825a4704`: 100 → 0.

These were `masked_equal` disclosures in the wrong unit, and the net above already includes them.

## Method (per move)

A move was taken only if all of the following held:

1. **Adjacency.** The range abuts or sits between the destination unit's own pins.
2. **Best candidate in the destination obj.** The retail row was scratch-renamed and diffed against
   every unclaimed function of the destination's base obj in a 0.5–2× size band, on the grader's
   ruler (`~/tmp/w16hq/runnerup.py`). The proposed name had to be the top candidate. For all 43
   named rows it was, except the two cases below.
3. **No identity contradiction on retail bytes.** `tools/anon_proposal_adjudicate.py` was run through
   a wrapper that pairs the **source** unit's target obj with the **destination** unit's base obj
   (`~/tmp/w16hq/adj_rehome.py`). The remaining RETAIL_ONLY / OURS_ONLY items were checked to be one
   of two things:
   - ICF fold-survivor spellings (`push_back<BeatCollisionData>`, `ObjPtrList<BandCamShot>::clear`).
   - Source divergences, for example retail's function-local static `Symbol`/`Message` (`atexit`)
     where our port uses globals. `BuildSongList` calls the free `IsLeaderLocal()` where ours calls
     `TheSessionMgr->IsLeaderLocal()`.

   None of them was a different-function signal.

Virtual rows got a further independent check: either retail RTTI vtable ownership, or an
already-matched vtordisp thunk of the destination that branches to the address.
- **RTTI ownership:** VocalTrainerPanel slots 7/16, GemTrackDir 32, Accomplishment 12,
  PerfectSectionTracker 13, BandTrack 28.
- **Thunk targets:**
  - `?Load@CharIKMidi@@$4…` at `0x823caaf0` → `b 0x823ca968`.
  - `?Handle@MultiSelectListPanel@@$4…` at `0x82627a48` → `0x826275f0`.
  - `?Handle@SpotlightEnder@@$4…` at `0x824ea088` → `0x824e9f28`.
  - `?ClassName@CharTransDraw@@$4…` at `0x823c88d0` → `0x823c88a0`.
  - `?Copy@CharWeightSetter@@$4…` at `0x823b9b18` → `0x823b9680`. This one brackets the
    FlowValueCase block with CharWeightSetter code on both sides.

Range edges are always the end of the previous function (from `symbols.txt`), so the preceding 8-byte
EH prefix travels with its function. The move is only the function plus its own trailing funclets,
unless the whole foreign block is bracketed by destination code. Moves never merge or reshape the
destination's existing blocks. `.pdata` was re-derived by the split (one extra build for the
split-guard fixed point).

## Moves: before → after

Fuzzy is the value after the move. Every named row was an anonymous fuzzy-0 row in the "from" unit
before.

| # | `.text` range moved | from (before) | to (after) | fns / B | rows named (fuzzy after) |
|---|---|---|---|---:|---|
| M1 | `0x826B9BAC–0x826BA880` | FreestylePanel | VocalTrainerPanel | 8 / 3,284 | CopyPhrasesImp 91.2; CopyTubes 99.5; Poll 83.9; StartSectionImpl 96.7; `0x826ba830` `??_E`→**`??_GVocalTrainerPanel`** 100.0 |
| M2 | `0x825C060C–0x825C0EB0` | BandMachine | SongSort | 21 / 2,212 | SongSort::BuildSongList 51.7; SetlistSort::BuildSetlistList 54.9 |
| M3 | `0x822EADD8–0x822EB1F8` | RockCentral | GemTrackDir | 3 / 1,056 | SetupSmasherPlate 84.4 |
| M4 | `0x82627238–0x82627A48` | NewAwardPanel | MultiSelectListPanel | 17 / 2,064 | OnMsg(UIComponentSelectMsg) 49.6; OnMsg(UIComponentScrollMsg) 60.1; FakeComponentSelect 93.0; FakeComponentScroll 93.0; Handle 81.9 |
| M5 | `0x82594084–0x82594538` | UIEventMgr | Accomplishment | 14 / 1,204 | CanBeLaunched 19.9 |
| M6 | `0x826DBC68–0x826DBEE4` | Stats | PerfectSectionTracker | 1 / 636 | Poll_ 92.1 (denylist lifted, below) |
| M7 | `0x825A4340–0x825A4778` | BandSongMetadata | AccomplishmentTourConditional | 9 / 1,080 | UpdateConditionOptionalData 63.8; InqConditionProgress 61.0; IsConditionMet 94.5 |
| M8 | `0x825EDE9C–0x825EE150` | AssetTypes | ClosetPanel | 6 / 692 | CycleCamera 97.1 |
| M9 | `0x825F5540–0x825F5700` | AccomplishmentPanel | CampaignGoalsLeaderboardChoicePanel | 3 / 448 | LoadIcons 95.3 |
| M10 | `0x82BB3288–0x82BB33D8` | Gem | VorbisReader | 1 / 336 | TryDecode 88.4 |
| M11 | `0x8257AA00–0x8257AD18` | MetaPerformer | BandSongMgr | 12 / 792 | `??0BandSongMgr` 97.4 |
| M12 | `0x826CF240–0x826CF37C` | PracticeSectionProvider | MultiplayerAnalyzer | 1 / 316 | OverrideBasePoints 99.9 |
| M13 | `0x823CA95C–0x823CAAF0` | Gem | CharIKMidi | 3 / 404 | CharIKMidi::Load 53.1 |
| M14 | `0x8235F84C–0x8235F9B8` | Game | Tour | 2 / 364 | Tour::Init 76.3 |
| M15 | `0x8268AA04–0x8268AC08` | GameConfig | BandUser | 5 / 516 | `??0BandUser` 79.9 |
| M16 | `0x824E9F20–0x824EA084` | BandUser | SpotlightEnder | 3 / 356 | SpotlightEnder::Handle **100.0** |
| M17 | `0x8234F8E0–0x8234FCA8` | RockCentral | BandTrack | 7 / 968 | SetPerformanceMode 95.7; SetTourMomentGoalText 84.9 |
| M18 | `0x82B7A06C–0x82B7A1A8` | GemManager | QuestFilterPanel | 3 / 316 | QuestFilterProvider::GetSetlistType 56.0 |
| M19 | `0x8235EF98–0x8235F160` | Leaderboard | Tour | 6 / 456 | `??1Tour` 90.0 |
| M20 | `0x8256B5DC–0x8256B8B0` | CharCache | AssetMgr | 9 / 724 | ConfigureAssetTypeToIconPathMap **100.0**; `hash_map<Symbol,Asset*>` ctor **100.0**; AssetMgr::Init **100.0** |
| M21 | `0x826588DC–0x826589E0` | CampaignLevel | PerformanceData | 2 / 260 | PerformanceData::Initialize 58.3 |
| E1 | `0x823B9744–0x823B9B18` | FlowValueCase | CharWeightSetter | 6 / 980 | `~ObjPtrList<CharWeightable>` **100.0**; `ObjPtrList<CharWeightable>::Load` (`0x823b99f8`) **100.0** |
| E2 | `0x823C82F4–0x823C88D0` | Screenshot | CharTransDraw | 12 / 1,500 | `ObjPtrList<Character>::Replace` **100.0**; `~ObjPtrList<Character>` **100.0**; `ObjPtrList<Character>::Load` (`0x823c84d8`) **100.0**; `PropSync<Character>(Character*&,…)` **100.0**; `??0CharTransDraw` **100.0**; `CharTransDraw::ClassName` **100.0** |

### The two engine mis-pins, settled

- **`0x823b99f8` is `ObjPtrList<CharWeightable>::Load`**, a template COMDAT that CharWeightSetter's
  TU emitted. It is not CharWeightable code. The whole FlowValueCase block `0x823B9744–0x823B9B18`
  sits between `CharWeightSetter::Copy` and that function's own vtordisp thunk. Its rows score 100
  against `CharWeightSetter.obj`: the list dtor, `??_G` and `Load`. The best FlowValueCase
  candidate reads 31.5.
- **`0x823c84d8` is `ObjPtrList<Character>::Load`.** It belongs to CharTransDraw's contiguous region
  `0x823C82F4–0x823C88D0`, together with Replace, the dtor, PropSync, the CharTransDraw ctor and
  ClassName. All six are 100 against `CharTransDraw.obj`. `Screenshot::Handle` and its two funclets
  (`0x823C8190–0x823C82F4`) stay in Screenshot, where they are already at 100.

### Map changes beyond plain inserts

- **`0x826ba830` `??_EVocalTrainerPanel` → `??_GVocalTrainerPanel`.** VocalTrainerPanel.obj defines
  no `??_E` body; its `??_G` reads 100.0 against the retail 68-byte body. The body calls
  `??1VocalTrainerPanel` and `??1Object`.
- **`0x826dbc68` lifted from `_denylist`.** The denial refuted `vector<float>::_M_fill_insert` and
  described the body as `T::M(float)`: a member at +0x58 compared with `lfs`, and virtual dispatch
  through +0x48. That is `PerfectSectionTracker::Poll_(float)`:
  - RTTI slot 13.
  - Fuzzy 92.1, where the next candidate reads 21.2.
  - The address is flush against PerfectSectionTracker's pins.

  The lift is recorded in `_denylist_comment`, following the W16-AB / W21-CARVE precedent.

### Moved but deliberately not named

- `0x82627470`, a 20-byte `OnMsg` stub. It scores 100 as `OnMsg(ButtonDownMsg)`, but a matched
  retail caller binds the address as `UIStats::OnMsg(UIComponentFocusChangeMsg)`. It is an ICF fold
  survivor, so naming it would be a guess.
- `0x823b9828`, PropSync over an `ObjPtrList`. It reads 93, but ours is 396 B against retail's 420 B
  and the element type is unproven.
- Funclets and unidentified neighbours inside moved ranges: `fn_8257AB40`, `fn_8268AB40`, and the
  `PropSync` bodies `0x8234FAC8`/`0x8234FBC8`, which are 100 on two folded instantiations. They stay
  `fn_`.

### Not moved

- The `_Vector_base<unsigned short>` COMDAT at `0x82BB3228` (Gem, 100).
- The `ObjVector<ObjPtr<RndDir>>` `>>` COMDAT at `0x823CA8F8` (Gem, 99.8; CharIKMidi.obj is
  DC3-shaped and does not instantiate it).
- The `Stats` copy-ctor/`operator=` COMDATs before PerformanceData::Initialize.
- ThreeDSoundManager's `0x8256B4C8–0x8256B570`, which sits inside AssetMgr's run but has no
  adjudicated identity.
- The small `UIEventMgr` rows `0x82593f48–0x82594084`.

Each of these is a lead, not a finding.

## Remaining residue (named, below 100) — where the next work is

- **CharIKMidi::Load 53.1.** Retail is rb3-Wii's shape (`gRev`,
  `ObjVector<ObjPtr<RndTransformable>>`). Ours is DC3's (`BinStreamRev`, `ObjPtrVec`). The fix is
  under `src/system/char/`, which is out of this lane's scope.
- **Accomplishment::CanBeLaunched 19.9.** Retail builds function-local static `Symbol`s (776 B with
  atexit thunks); ours compares globals (228 B).
- **SongSort::BuildSongList 51.7 / BuildSetlistList 54.9,
  MultiSelectListPanel::OnMsg ×2 49.6/60.1, QuestFilterProvider::GetSetlistType 56.0,
  PerformanceData::Initialize 58.3.** These are body ports now that the rows pair.

## Gates

- `tools/map_name_injectivity.py`: `OK: 30084 applied rows, 30083 distinct names, injective`.
- `tools/icf_alias_finder.py --validate`: `PASS -- 1482 map-consistent, 254 tolerated, 0 contradicted`.
- Both were run on the rebased branch after a full build.
- `tools/native_build_gate.sh`: run last; the result line is in the lane report.

## Reproduce

```
python3 ~/tmp/w16hq/facts.py                     # pin / map / DEFINED facts per row
python3 ~/tmp/w16hq/mkprops.py && python3 ~/tmp/w16hq/adj_rehome.py ~/tmp/w16hq/props.json ~/tmp/w16hq/adj.json
python3 ~/tmp/w16hq/runnerup.py                  # destination-obj candidate ranking
python3 ~/tmp/w16hq/apply_splits.py config/45410914/splits.txt ~/tmp/w16hq/moves.json config/45410914/symbols.txt
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json ~/tmp/w16hq/map_rows_gated.json
python3 tools/ab_measure.py --worktree <clean wt at 835ab0289> --patch <835ab0289..w16-hq>
```
