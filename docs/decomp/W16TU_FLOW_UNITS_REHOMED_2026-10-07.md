# W16-TU: the Flow units held other TUs' code; every row re-homed

Lane W16-TU, 2026-10-07. Worktree `~/tmp/wt-w16tu`, branch `w16-tu`, base main `c685bbcf1`.

W16-TR (`W16TR_DISCARDED_CALLERS_2026-10-07.md` §2) found that retail RB3 contains no Flow system, while
`config/45410914/splits.txt` pinned 11 Flow units whose rows were other code. This lane confirms the
absence on retail bytes, moves every row to the unit that owns it, removes the Flow units from the split,
and prices the change. It also runs the same probe over every other directory.

The short version:
- **Retail has no Flow system.** 0 of 29 derived classes declared under `src/system/flow/` has an RTTI
  type descriptor in `band.exe`. All 8 control classes are found (§1).
- **There were 13 Flow units, not 11.** `PropertyEventProvider` (1 row) and `DrivenPropertyEntry` (0 rows)
  are Flow-system classes too. All 13 are gone from `splits.txt`. They held 47 rows / 3,748 B.
- **46 of the 47 rows have a new home, and 1 is left unpinned with its reason recorded (§2).** Two homes needed a
  map rename, because the old name spelled a Flow type for an ICF-folded template.
- **Price, 3 A/Bs, deltas compose: +8 functions / +268 B** (§3). Each step's prediction is recorded.
  Units at 100% fall by 9. Six of those were Flow units that no longer exist. Three were real units
  (CharPollGroup, CharUpperTwist, MetaPanel) that gained a 0% row of their own.
- **Two more absent engine systems are pinned: `gesture` (Kinect) and `hamobj` (Dance Central)**,
  9 units / 14 rows / 1,592 B. They are recorded, not acted on (§4).

## 1. Retail does not contain the Flow system

The instrument scans retail `band.exe` (clean TU5) in Python; the shell `grep` cannot match binary files.
Retail is built `/GR`, so every polymorphic class that is linked in has an MSVC RTTI type descriptor,
`.?AV<Class>@@`. The image holds 1,194 distinct RTTI leaf names.

Controls, all found with RTTI: Fader, MidiInstrument, UIPanel, StorePanel, ContentMgr, Song,
TrackPanelDir, CharPollGroup.

Flow-system classes, all without RTTI: FlowNode, FlowIf, FlowCommand, FlowManager, FlowSlider, FlowSound,
FlowSetProperty, FlowMultiSetProperty, FlowSwitchCase, FlowValueCase, FlowQueueable, FlowLabel,
FlowTrigger, FlowSequence, PropertyEventProvider, DrivenPropertyEntry. The bare word `Flow` occurs once,
in the XDK shader-compiler string `Predicated Control Flow & Predicated ALU don't match`. `flow_` occurs
twice, both in song paths (`gowiththeflow_update.mid`, `evenflow_update.mid`). `FlowPtr`,
`PropertyEventListener` and `DrivenProperty` occur 0 times.

The same probe, applied per directory to every class a header declares with a base class (`class X :
public ...`):

| directory | derived classes | with retail RTTI |
|---|---:|---:|
| src/system/flow | 29 | **0** |
| src/system/gesture | 28 | **0** |
| src/system/hamobj | 74 | **0** |
| src/system/moviebink | 2 | 0 (false flag, see below) |
| src/system/dsp | 1 | 0 (false flag, see below) |
| src/system/bandobj | 63 | 63 |
| src/system/char | 59 | 55 |
| src/system/rndobj | 105 | 87 |
| src/band3/meta_band | 255 | 251 |
| src/band3/game | 90 | 88 |

(Every other `src/system` and `src/band3` directory is 62–100%.)
- `moviebink`: retail has `BINK`, `BINKCONS`, `BINKBSS` and `BINKDATA` sections, so Bink is linked. Its two classes
  simply carry no RTTI in our headers' sense.
- `dsp` is RB3's vocal pitch detection. Its one "derived class" is not polymorphic.
- `src/network/*` reads 0–15% almost everywhere. That is Quazal vendor code, and RTTI is not a valid
  probe there. Not adjudicated.

## 2. Disposition of every row

How an owner was chosen:
- **A non-inline member function** belongs to its class's TU. For example, `ContentMgr::Handle` and `Song::SetLoopStart`
  are each defined only in their own TU.
- **An unwind funclet** belongs to its parent, which is the function just before it. Where a body stores a vtable,
  the vtable's RTTI names the class: `fn_823C7000` stores `0x820501BC`, whose Complete Object Locator
  names `.?AVCharUpperTwist@@`, so it is CharUpperTwist's destructor.
- **A template COMDAT** goes to the adjacent or enclosing unit whose compiled object defines that
  spelling. Retail places a COMDAT with the first object in link order that defines it. In the pre-`main` template
  run (`0x82271800`–`0x82272E3C`) the existing pins are pairing choices, not ownership facts, so these
  homes are the best available and not proven.
- **4–12 B slivers** are zero padding or the 8 B EH prefix (a `.text` pointer and an `.rdata` pointer)
  in front of a function. They hold no rows and go to the unit of the function that follows.

The funclets give an independent check. A funclet's bytes encode its parent's frame offsets. Nine funclets
that read 99.3–99.9, or 0, as Flow rows read exactly 100 at the unit chosen here.

Before = main's `report.json` at the base, measured in step 1's leg A. After = step 3's leg B.

| old unit | .text block | new unit | rows: fuzzy before -> after (B) | evidence |
|---|---|---|---|---|
| FlowIf | `823C6FF4-823C7000` (12 B) | CharUpperTwist | (no rows) | 4 B pad + 8 B EH prefix of the following CharUpperTwist dtor |
| FlowIf | `823C7000-823C70E8` (232 B) | CharUpperTwist | `823C7000` 136: 0 -> 0<br>`823C7088` 48: 99.4167 -> 100<br>`823C70B8` 48: 99.4167 -> 100 | fn_823C7000 stores vtables 0x820501BC/0x82050164, RTTI .?AVCharUpperTwist@@ (CharUpperTwist dtor); fn_823C7088/70B8 are its unwind funclets |
| FlowIf | `823C72DC-823C7308` (44 B) | CharBoneOffset | `823C72DC` 40: 100 -> 100 | fn_823C72DC unwind funclet (~DataNode) following CharBoneOffset::Handle |
| FlowIf | `823C7314-823C7318` (4 B) | CharUpperTwist | (no rows) | 4 B pad before a CharUpperTwist thunk |
| FlowIf | `823C7324-823C7328` (4 B) | CharBoneOffset | (no rows) | 4 B pad before a CharBoneOffset thunk |
| FlowIf | `823C75C4-823C75C8` (4 B) | CharUpperTwist | (no rows) | 4 B pad before CharUpperTwist code |
| FlowMultiSetProperty | `822728E0-82272A60` (384 B) | CalibrationPanel | `822728E0` 104: 100 -> 100<br>`82272948` 168: 100 -> 100<br>`822729F0` 112: 100 -> 100 | std::sort<float*> helpers + vector<float>::push_back; CalibrationPanel.obj defines all, adjacent unit already holds sort_heap<float*> |
| FlowMultiSetProperty | `82272A60-82272AD0` (112 B) | **unpinned** | `82272A60` 108: 100 -> 0 | vector<Hmx::Object*>::_M_fill_insert: no compiled non-Flow obj instantiates it; owner of this COMDAT run (pre-main template soup) unidentified |
| FlowMultiSetProperty | `82272AD0-82272B20` (80 B) | CalibrationPanel | `82272AD0` 80: 100 -> 100 | __insertion_sort<float*>; CalibrationPanel.obj defines it, contiguous with CalibrationPanel's 0x82272B20 |
| Flow | `822A6F68-822A6F70` (8 B) | OutfitConfig | (no rows) | 8 B EH prefix of the following OutfitConfig function |
| Flow | `822D8D48-822D8D70` (40 B) | StreakMeter | `822D8D48` 40: 99.5 -> 100 | fn_822D8D48 unwind funclet following a StreakMeter function; enclosed by StreakMeter |
| Flow | `82574D90-82574E1C` (140 B) | MetaPanel | `82574D90` 100: 0 -> 0<br>`82574DF4` 40: 100 -> 100 | fn_82574D90 = new + NextSongPanel ctor (NewObject factory), fn_82574DF4 its funclet; follows ??_GNextSongPanel in MetaPanel, enclosed by MetaPanel |
| Flow | `825754AC-825754B0` (4 B) | BandSongMgr | (no rows) | 4 B pad before BandSongMgr |
| FlowManager | `824F9050-824F90A8` (88 B) | DataNode | `824F9050` 88: 100 -> 100 | map<Symbol,DataNode>::_M_erase; DataNode.obj defines it (gDataVars), adjacent following unit |
| FlowSetProperty | `827C6690-827C6708` (120 B) | Song | `827C6690` 120: 100 -> 100 | Song::SetLoopStart, a non-inline Song member; Song.obj defines it; enclosed by Song |
| FlowSwitchCase | `823B08D0-823B0914` (68 B) | CharPollGroup | `823B08D0` 68: 0 -> 0 | fn_823B08D0 = CharPollGroup deleting-dtor thunk (this-0x50, calls ~CharPollGroup); enclosed by CharPollGroup |
| FlowSwitchCase | `823B0AA8-823B0B00` (88 B) | CharPollGroup | `823B0AA8` 40: 99.4 -> 99.5<br>`823B0AD0` 40: 99.9 -> 100 | unwind funclets following CharPollGroup::SortPolls (~map<Object*,CharPollableSorter::Dep>) |
| FlowValueCase | `823AEF8C-823AEF98` (12 B) | CharWeightable | (no rows) | 4 B pad + 8 B EH prefix of the following CharWeightable function |
| FlowValueCase | `823B9B24-823B9B28` (4 B) | CharWeightSetter | (no rows) | 4 B pad, enclosed by CharWeightSetter |
| FlowValueCase | `823B9D30-823B9D38` (8 B) | CharWeightSetter | (no rows) | 8 B EH prefix of the following CharWeightSetter function |
| FlowValueCase | `823BA5AC-823BA674` (200 B) | CharWeightSetter | `823BA5AC` 40: 99.3 -> 100<br>`823BA5D4` 40: 99.4 -> 100<br>`823BA5FC` 40: 99.3 -> 100<br>`823BA624` 40: 0 -> 100<br>`823BA64C` 40: 0 -> 100 | 5 unwind funclets following CharWeightSetter::Load (~ObjPtr<CharWeightSetter>, ~ObjPtrList<CharWeightable>) |
| FlowValueCase | `82630750-82630778` (40 B) | band3/meta_band/RetryAudioPanel | `82630750` 40: 100 -> 100 | unwind funclet (~DataNode) following RetryAudioPanel::Handle; enclosed by RetryAudioPanel |
| FlowQueueable | `82487824-82487828` (4 B) | EnvAnim | (no rows) | 4 B pad, enclosed by EnvAnim |
| FlowQueueable | `82487834-82487838` (4 B) | EnvAnim | (no rows) | 4 B pad, enclosed by EnvAnim |
| FlowCommand | `8251E928-8251EB38` (528 B) | ContentMgr | `8251E928` 60: 100 -> 100<br>`8251E964` 44: 100 -> 100<br>`8251E990` 116: 100 -> 100<br>`8251EA08` 172: 100 -> 100<br>`8251EAC0` 72: 100 -> 100<br>`8251EB10` 40: 100 -> 100 | ContentMgr members + CallbackFile list templates; ContentMgr.obj defines every named row; enclosed by ContentMgr |
| FlowCommand | `8251EB70-8251EBA8` (56 B) | ContentMgr | `8251EB70` 44: 100 -> 100 | ContentMgr::UnregisterCallback |
| FlowCommand | `8251EBA8-8251F0C8` (1312 B) | ContentMgr | `8251EBA8` 144: 100 -> 100<br>`8251EC38` 40: 100 -> 100<br>`8251EC68` 740: 100 -> 100<br>`8251EF4C` 32: 100 -> 100<br>`8251EF6C` 32: 100 -> 100<br>`8251EF8C` 32: 100 -> 100<br>`8251EFAC` 32: 100 -> 100<br>`8251EFCC` 40: 100 -> 100<br>`8251EFF4` 32: 100 -> 100<br>`8251F014` 40: 100 -> 100<br>`8251F03C` 32: 100 -> 100<br>`8251F060` 100: 100 -> 100 | ContentMgr::OnAddContent, ContentMgr::Handle + its funclets, list<CallbackFile>::insert |
| FlowNode | `8230846C-82308478` (12 B) | TrackPanelDir | `82308470` 4: 0 -> 0 | 4 B b-thunk fn_82308470 + pads, enclosed by TrackPanelDir |
| FlowNode | `82308C38-82308C88` (80 B) | TrackPanelDir | `82308C38` 40: 100 -> 100<br>`82308C60` 40: 100 -> 100 | 2 unwind funclets (~DataNode) following a TrackPanelDir function |
| FlowNode | `82308D5C-82308D60` (4 B) | TrackPanelDir | (no rows) | 4 B pad, enclosed by TrackPanelDir |
| FlowSlider | `82666160-826661CC` (108 B) | FriendsProvider | `82666160` 108: 100 -> 100 | pointer __linear_insert with fn-pointer comparator, inside FriendsProvider's own code (between FriendsProvider::Mat and its ctor); FriendsProvider defines FriendCmp(const Friend*,const Friend*) and the only such instantiation; map renamed FlowNode -> Friend spelling |
| PropertyEventProvider | `82768F98-82768FF4` (92 B) | DataUtl | `82768F98` 92: 100 -> 100 | map<Symbol,4-byte POD>::_M_erase inside DataUtl's span (DataPopVar..DataInit); DataUtl.obj instantiates map<Symbol,DataArray*>::_M_erase (macro table); map renamed <Symbol,float> -> <Symbol,DataArray*> spelling |
| DrivenPropertyEntry | `82781654-82781660` (12 B) | VocalNoteList | (no rows) | 4 B pad + 8 B EH prefix of the following VocalNoteList function |

The four rows still at 0 now sit in their real owner instead of a Flow unit. They are
CharUpperTwist's destructor (136 B), a CharPollGroup deleting-destructor thunk (68 B), a NextSongPanel
factory in MetaPanel (100 B) and a 4 B branch thunk in TrackPanelDir. That is why three real units no
longer read 100%: they never were, and the Flow pins hid it.

**The one unpinned row.** `0x82272A60` is `vector<Hmx::Object*>::_M_fill_insert`, 108 B. No compiled
object outside the Flow TUs instantiates it, and it sits in the pre-`main` template run, whose owning TU
is unidentified. Pinning it anywhere would assert an owner the evidence does not support. It now reads 0
in `auto_03_82272A60_text` and stays in the denominator.

**The two renames** (`scripts/target_symbol_map.json`):

| address | old name | new name | why |
|---|---|---|---|
| `0x82666160` | `__linear_insert<FlowNode**, FlowNode*, bool(*)(FlowNode*,FlowNode*)>` | `__linear_insert<Friend**, Friend*, bool(*)(const Friend*,const Friend*)>` | It sits inside FriendsProvider's own code, between `FriendsProvider::Mat` and its constructor. FriendsProvider defines `FriendCmp(const Friend*, const Friend*)` and the only pointer/function-comparator `__linear_insert` nearby. |
| `0x82768F98` | `_Rb_tree<Symbol, pair<const Symbol,float>>::_M_erase` | `_Rb_tree<Symbol, pair<const Symbol,DataArray*>>::_M_erase` | It sits inside DataUtl's span (`DataPopVar` … `DataInit`). DataUtl's macro table is a `map<Symbol,DataArray*>`. No compiled non-Flow object emits the `<Symbol,float>` spelling. |

Both are ICF-folded template bodies, so retail's own name for them cannot be recovered. The old names
were the arbitrary fold spellings of a system retail lacks.

Alias groups 792 and 2139 had those spellings as survivors. They were relabelled by
`tools/alias_survivor_relabel.py --write`: folded members kept, old Flow spellings held as records and not
folded. The tool took its verdicts before the build, so it recorded UNDECIDABLE `MISSING(retail)`, because
the renamer had not yet applied the new names to the retail objects. Re-adjudicated on the built tree with
`tools/icf_pair_adjudicate.py --chase`, all four memberships are **CHASED T1 PROVEN**, each with 1 retail
body twin. These verdicts are stored on each group as `postbuild_rechase_w16tu`. Flat T1 refutes three of
them only because each body's recursive self-call names its own spelling.

**One alias membership added** (group 102, `0x82670F88`): `__unguarded_linear_insert<Friend**>`. FLAT and
CHASED T1 PROVEN, 84 B vs 84 B, relocation targets agree, 1 retail body twin against 8 identical spellings
in our build. Witness: retail `0x82666160` calls `bl 0x82670F88`, and our FriendsProvider.obj spells that
call `<Friend**>`. The held `FlowNode**` spelling used to forgive this site.
`tools/icf_alias_finder.py --validate`: PASS, 0 contradicted. `tools/alias_survivor_drift.py`: OK.

## 3. Price

Three `tools/ab_measure.py --from-dirty` runs, one change each, in this worktree, on the `name_check`
ruler. Every leg settled to zero work and was read at a `symbols.txt` split fixed point. Run dirs are
under `~/tmp/wt-w16tu/.ab_measure_runs/`.

| step | commit | kind | predicted | measured | units at 100% |
|---|---|---|---|---|---|
| 1. re-home 13 units | `4eb1fa6c2` | splits | −3 fns / −308 B | **+6 fns / +68 B** | 626 → 617 |
| 2. two renames + relabel | `8b62b3c0e` | map | +2 / +200 B | **+1 / +92 B** | 617 → 617 |
| 3. Friend fold in group 102 | `62ddc11a1` | map | +1 / +108 B | **+1 / +108 B** | 617 → 617 |
| total | | | | **+8 fns / +268 B** | **−9** |

Leg A of step 1: matched 54,938, `matched_code_percent` 59.307816. Leg B of step 3: 54,946, 59.310425.

**Step 1's prediction failed.** The three predicted losses happened: −308 B from the unpinned row and the two
unrenamed fold rows. I had not predicted that nine funclets pair better at their true parents: +376 B
from CharUpperTwist ×2, StreakMeter, CharPollGroup and CharWeightSetter ×5, two of which went 0 → 100.
Matched bytes over the moved rows went 3,024 → 3,092, which equals the whole-binary +68 exactly. The split
re-derived 18 `.pdata` lines, and they are committed with the step.

**Step 2 came in at half its prediction.** DataUtl's row reached 100. FriendsProvider's reached 99.81,
with one charged site: retail's callee at `0x82670F88` is spelled `<RndDrawable**>`. Step 3 is the fix,
and it came in exactly at its prediction. Its ALIAS_SUSPECT shape alert (name_check up, `none` flat on a
map-only patch) is answered by the retail-byte proof in §2.

Not run: the native gate. No `src/` file changed, only `splits.txt`, the target map and `symbol_aliases.json`.

## 4. Other absent systems pinned in `splits.txt`

`gesture` (Kinect skeleton tracking) and `hamobj` (Dance Central game objects) are DC3-only, and §1 shows
retail has none of their classes. Nine of their units are pinned, holding 14 rows / 1,592 B, all at 100%.
They follow the Flow pattern: ICF-folded templates, or fold survivors spelled with DC3-only types, sitting
inside RB3 units' spans.

| unit | .text | rows | sits inside |
|---|---|---|---|
| RhythmDetector | `82270690`, `82270848`, `8227173C` | 7 rows, `ObjDirPtr<ObjectDir>` members + 2 funclets (1,016 B) | CommonPhraseCapturer / BandRetargetVignette / TextFileStream |
| CameraInput | `822740C8` | `CameraInput::NewFrame` (8 B stub) | Object |
| BustAMoveData | `82274538` | `operator<<(BinStream&, const BAMPhrase&)` (100 B) | Accomplishment |
| SongLayout | `822C76E4`, `822D16F8`, `8235CAE0`, `8235CB54` | `vector<Symbol>` push_back / copy-ctor + funclet (268 B) | BandFaceDeform / CharKeyHandMidi / CharClip |
| HamRibbon | `822F0408` | `ObjPtrList<RndTransformable>::DeleteAll` (116 B) | Label3d |
| HamIKEffector | `823C3300` | `operator<<(BinStream&, const HamIKEffector::Constraint&)` (84 B) | CharPosConstraint |
| DepthBuffer3D, SkeletonClip, FitnessFilter | 8 B slivers | none | VocalScoreHistory / StandardStream / Faders |

These need the same treatment as this lane, in a separate lane. The BAMPhrase, Constraint and CameraInput
names are DC3-only spellings, so those rows also need a rename to an RB3 instantiation, adjudicated the
same way.

One more Flow spelling remains in the map, outside the 13 units: `0x82768FF8`
`_Rb_tree<FlowNode*, pair<FlowNode* const, FlowQueueable::QueueState>>::_M_create_node`. It is pinned to
`band3/meta_band/ViewSetting` and sits inside DataUtl's span. It pairs through a chain of scatter
includes: ViewSetting includes `CriticalUserListener.cpp`, which includes `flow/FlowManager.cpp`
(`CriticalUserListener.cpp:100`). Both objects define the spelling. So Flow source is still compiled into
two retail-present units, as pairing scaffolding.

Per-class misses that are not whole systems (`HAQManager`, `ChordPreview`, `LocalePanel`, `PhysicsVolume`,
…) came from a loose "polymorphic" test and were not adjudicated. `HAQManager` is already known absent
(W16-TR §2).

## 5. Not done

- The scatter includes that existed only to pair Flow-unit rows are now dead weight, left in place to
  keep this lane free of `src/` edits:
  - `FlowCommand.cpp` → `os/ContentMgr.cpp`
  - `FlowMultiSetProperty.cpp` → `band3/game/GemPlayer.cpp` and `band3/game/Band.cpp`
  - `FlowSetProperty.cpp`'s body-dup of `Song::SetLoopStart`

  The 13 Flow `.cpp` files are still compiled from `objects.json`. With no split they are out of
  `objdiff.json`, like FlowOnStop already was. Removing the includes is a source change and needs the
  native gate.
- No change to W16-TM's Flow gates in the native build. They test DC3 text, not retail (W16-TR §2).
- The `0x82272A60` owner, `vector<Hmx::Object*>::_M_fill_insert`, is unidentified.

Merging and pushing are left to the coordinator.
