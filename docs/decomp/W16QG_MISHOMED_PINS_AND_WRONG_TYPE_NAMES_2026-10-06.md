# W16-QG: W16-QB categories 1 and 2, worked through (2026-10-06)

Lane brief: work the row lists of categories 1 (mis-homed instantiation pins)
and 2 (template instantiations named with the wrong type) from
[W16QB_DC3_REFERENCE_AUDIT_2026-10-06.md](W16QB_DC3_REFERENCE_AUDIT_2026-10-06.md)
§1/§3.2. Before naming anything, apply that doc's rule: a name is safe only
when no other retail caller of the address expects another type.

**Result: 45 of 79 addresses fixed, in three batches. Measured total
+46 fns / +2,796 B (+0.027287 pp code%). 0 rows went down in any batch.**
Each batch was predicted from an in-tree build before its A/B, and each A/B
measured exactly the prediction. The other 34 addresses are skipped, each
with a reason (§4). This is map/splits/alias work only: no source changed,
so it has no native value beyond the identifications.

| batch | commit | what | predicted | measured (`ab_measure --from-dirty`) |
|---|---|---|---|---|
| A | `50d659a33` | 33 addresses: 15 rename+pin, 8 rename, 10 pin only | +34 / +2,420 B | **+34 fns / +2,420 B**, units at 100% (mpn) 567 → 573, 0 fell off |
| B | `e6ebdb93b` | 5 deleting dtors renamed `??_G`, plus 5 compiler-declared `??_E` alias groups | +5 / +340 B | **+5 fns / +340 B** |
| C | `f4f4dcdb0` | 7 rows of 4–8 B (the audit's fold bait), run as a separate A/B | +7 / +36 B | **+7 fns / +36 B**; BandPerformer completes by denominator shrink |

The legs chain with no gaps: matched 53,577 → 53,611 → 53,616 → 53,623, and
`matched_code` 5,951,200 → 5,953,620 → 5,953,960 → 5,953,996. Run dirs are
`.ab_measure_runs/20261006-123100-w16qg-batchA`, `…123823-w16qg-batchB`,
`…124749-w16qg-batchC` in the lane worktree.

**How "no rows went down" was checked.** The leg A and leg B `report.json`
of each run were compared row by row, keyed on (unit, name) and on name
alone (so a moved pin counts as one row). Result: **0 same-key drops and 0
name-level drops in all three runs.** Every key that disappeared was a 0%
row under its old name, and each was replaced by the same address at 100
under its new one (A: 34 gone / 34 new, B: 5/5, C: 7/7).
`ab_measure`'s per-unit list showed `Memory_Xbox −1` in batch A. That is
`?Highlight@RndDir@@UAAXXZ` (8 B, 100%). It shared one `.text` block with the
five RndDir `$4` thunks that moved to PatchDir, so it moved with them, and it
reads 100/100 in PatchDir. It is a reattribution, not a regression.

## 1. Population

- Category 1: the 36 addresses from W16-QB's `flankmatch` list that were
  still unfixed.
- Category 2: W16-QB's 50 unpaired named rows, minus the 3 VocalPlayer
  `FilePath` rows it fixed, leaves 47 addresses.
- 4 addresses appear in both lists, so there are **79 distinct addresses**.

## 2. Method

1. **Find the body.** Each target obj, and each of our compiled objs, was
   read with a COFF reader that hashes each function's bytes with
   relocation fields masked. That gives the candidate set: functions in the
   **pinned** unit's obj or a **flanking** unit's obj (a neighbouring
   `.text` block on either side) whose masked bytes equal retail's at that
   address.
2. **Require relocation-exact.** A candidate must have the same relocation
   types and counts as retail, and each relocation target must be either
   name-equal, in the same alias bucket, or a placeholder (forgiven) on the
   retail side.
3. **Check the callers (the W16-QB rule).** For every retail function that
   relocates against the address, find our same-named function and align
   the relocation site by its order among relocations of the same type.
   Then compare our callee spelling at that site with the proposed name.
   Verdicts are MATCH (same name or alias bucket), CHARGE, or inert
   (anonymous caller, or no our-side body). Any CHARGE or unknown blocks
   the rename.
   - **Control:** the check reproduces W16-QB's measured −648 B case.
     Naming `0x8239C558` charges SongData's two `RangedData<RGRollChord>`
     rows and Singer's results row, the same rows QB measured falling. It
     also reads MATCH on every name QB kept.
4. **Adjudicate on retail bytes.** Each pair was run through
   `tools/icf_pair_adjudicate.py --pairs … --chase`. A REFUTED verdict on a
   rename blocks it, with two exceptions settled by caller context because
   the refutation was the "VACUOUS-PLACEHOLDER-SLOT" kind (the body is too
   small for the adjudicator to tell which global it loads):
   - `0x825107C0`: retail `PreInitSystem` references it at the two offsets
     where ours references `OnSystemLanguage`, and its global is
     `gSystemLanguage`.
   - `0x82524A60`: retail `JoypadResetXboxPC` calls it where ours calls
     `AssociateUserAndPad`, and its global is `gJoypadData`. The old name,
     `TrueColor::NuipTrueColorSetPlayer`, is DC3 Kinect code.
5. **Skip fold hubs.** Addresses with more than 200 retail call sites were
   skipped without evaluation (`??3`, `list<Object*>::insert`,
   `ObjPtr<Object>::SetObjConcrete`).
6. **Apply.** Edits went to the map and `splits.txt`. A pin moved with its
   function's extent; if the block left fewer than 12 B before the function
   or 8 B after it, those bytes went too. After any rename that hit an alias
   group's survivor, `alias_survivor_relabel.py --write` was run.

## 3. What changed

### Batch A (`50d659a33`, +34 / +2,420 B)

| address | B | change | old → new |
|---|---|---|---|
| `0x82B992A0` | 88 | rename + pin Msg → GemManager | `_M_find<Symbol>` of `map<Symbol,bool>` → `map<Symbol,TrackWidget*>` |
| `0x825107C0` | 24 | rename + pin Debug → System | `fn_` → `OnSystemLanguage` |
| `0x8252A610` | 16 | rename + pin AccomplishmentPanel → Memcard_Xbox | `fn_` → `??1MCContainer@@UAA@XZ` (retail RTTI at the vtable it loads: `.?AVMCContainer@@`; the first pick `??0UIListProvider` was REFUTED and dropped) |
| `0x82279710`–`0x82279750` | 5 × 16 | rename + pin Memory_Xbox → PatchDir | five `fn_` → RndDir/RndTransformable `$4PPPPPPPM@DM`/`@JA` vtordisp thunks (`Highlight`, `Print`, `PreLoad`, `PostLoad`, `Export`) |
| `0x827C67D8`, `0x827C6708` | 232, 204 | rename + pin FlowSetProperty → Song | `fn_` → `_Rb_tree<int,pair<int,Symbol>>::insert_unique` / `_M_insert` |
| `0x822C5758` | 96 | rename + pin MidiInstrument → BandIKEffector | `fn_` → `ObjVector<BandIKEffector::Constraint>::operator=` |
| `0x823CD060` | 76 | rename + pin CharDriverMidi → CharMirror | `fn_` → `??_G ObjPtr<CharServoBone>` |
| `0x82471D10` | 12 | rename + pin TransAnim → LitAnim | `fn_` → `?Print@RndLightAnim@@$4PPPPPPPM@A@…` |
| `0x822A3010` | 136 | rename + pin Character → OutfitConfig | `~vector<Unlockable>` → `~vector<BandPatchMesh::MeshPair::PatchPair>` |
| `0x822D8CC0` | 136 | rename + pin Flow → StreakMeter | `~vector<String>` → `~vector<ObjPtr<RndPartLauncher>>` |
| `0x822750D0` | 68 | rename + pin Accomplishment → PatchDir | `ObjPtrList<RndGroup>::push_back` → `PatchDir::FindEmptyLayer` |
| `0x824D05A8` | 112 | rename (Shockwave) | `fn_` → `vector<FilePath>::_M_fill_insert` |
| `0x82356430` | 116 | rename (GemTrackResourceManager) | `vector<SmasherPlateInfo>::_M_clear` → `_M_clear_after_move` |
| `0x82524A60` | 24 | rename (Joypad) | `TrueColor::NuipTrueColorSetPlayer` → `AssociateUserAndPad` |
| `0x827CD628` | 80 | rename (DataPointMgr) | `GameModeTerminate` → `NetCacheMgrTerminate` (DataPointMgr.cpp includes `utl/NetCacheMgr.cpp`, so its obj defines it) |
| `0x822A25E8` | 96 | rename (Gem) | `__destroy_range_aux<Unlockable>` → `<PatchPair>` |
| `0x82772B08` | 60 | rename (SongData) | `_Param_Construct<vector<short>>` → `_Copy_Construct<vector<RangedData<RGTrill>>>` |
| `0x826D98D8` | 88 | rename (PerfectSectionTracker) | `_M_find<int>` of `map<int,SongStatus>` → `map<int,float>` |
| `0x8235C2E0` | 72 | rename (Tour) | `Hmx::Object::SyncProperty` → `Tour::SyncProperty` (an earlier lane kept the Object name on purpose; this address sits inside Tour's block, and the callers all read MATCH through alias group 390) |
| `0x827390F8` | 48 | pin Rnd_Xbox → Env_NG | name unchanged |
| `0x827F42B8` | 12 | pin VocalTrackDir → UILabel | name unchanged |
| `0x822A5448` | 60 | pin Character → band3/bandtrack/Gem | name unchanged |
| `0x823587B0` | 84 | pin CharServoBone → Rot | name unchanged |
| `0x824D0800` | 112 | pin Shockwave → system/world/Dir | name unchanged; its 8 B EH prefix at `0x824D07F8` moved with it |
| `0x82319C20` | 16 | pin MetaPanel → MiniLeaderboardDisplay | name unchanged |
| `0x825AD710` | 88 | pin CameraManager → WaitingUserGate | name unchanged (see below) |
| `0x825238F0` | 8 | pin User → BandUser | name unchanged |
| `0x822AF1C8` | 4 | pin BandHeadShaper → BandCamShot | name unchanged |

The relabel tool re-checked 11 drifted groups: 56 PROVEN members kept, 2
UNDECIDABLE kept, 0 withdrawn, 0 contradictions.

On `0x825AD710`: the chase REFUTED a fold here (its COMDAT reader gives our
body 92 B against retail's 88 B), but the pin-only change pairs the
existing name at 100/100 in WaitingUserGate. The pin needs no fold claim,
so nothing was aliased. The 4 B size gap is unexplained. It may be the
known successor-EH-prefix artifact in COMDAT span readers, but that was not
checked.

### Batch B (`e6ebdb93b`, +5 / +340 B)

The five addresses are `0x826CB610` TrainerPanel, `0x823C75D8`
CharUpperTwist (`MAA`), `0x823E46D0` NetSession, `0x826B2918` PracticePanel
and `0x827B2770` StorePreviewMgr.

The map named three of them `??_E<X>`. In our objs `??_E<X>` is an
`IMAGE_SYM_CLASS_WEAK_EXTERNAL` (sec 0) aliasing `??_G<X>`, so it defines
nothing, nothing paired, and the row read 0%. The other two were `fn_`
placeholders.

Each address was renamed `??_G<X>`, and the four mis-pinned ones moved out
of Stats, FlowIf, MeshAnim and FlowOnStop into their own units. Each also
got a **COMPILER-DECLARED ALIAS** group (survivor `??_G<X>`, folded
`??_E<X>`) in the format lane W16-BR set. That keeps the class's own
adjustor thunk name-equal; its 12–16 B rows stay at 100. The evidence for
each group:

- the weak-external record read from our compiled obj;
- `--chase` reads PROVEN;
- a scan of retail `.text` for branches finds exactly one per address, a
  `b` from the class's `??_E…$4`/`$2` thunk;
- the only data reference is the address's own `.pdata` entry, so no vtable
  points at it.

### Batch C (`f4f4dcdb0`, +7 / +36 B, the small rows)

| address | B | change |
|---|---|---|
| `0x82766F98` | 8 | `fn_` → `??1EventSink@MsgSource@@QAA@XZ`, Mesh → Msg (the 8 B EH prefix at `0x82766FA0` belongs to Msg's next function and moved with it) |
| `0x822D4088` | 4 | `fn_` → `??1MiniIconData@EndingBonus@@QAA@XZ`, CharMeshHide → EndingBonus |
| `0x82343940` | 4 | `fn_` → `??1UIButton@@UAA@XZ`, UIComponent → system/bandobj/BandButton |
| `0x826DF788` | 4 | `fn_` → `??1 map<TrackerPlayerID,StreakTracker::PlayerStreakData>`, TexProc → StreakTracker. TexProc.cpp's heading had no other block, so it was deleted (an empty unit emits a 42-byte obj that `report.json` cannot open; this lane hit that failure and fixed it) |
| `0x8238B7E8` | 4 | `?CurrentThreadId@@YAKXZ` → `??1Weight@PlayBack@CharLipSync@@QAA@XZ` (group 1938 survivor swapped) |
| `0x8256AEF8` | 4 | `~_Rb_tree<int,String>` → `~hash_map<int,String>` (group 1935 survivor swapped; the `hash_map<Symbol,String>` membership stays PROVEN) |
| `0x826EE4D8` | 8 | `PitchCorrectedVoice::SetReleaseSmoothing` → `?Poll@CrowdRating@@QAAXM@Z`, BandPerformer → CrowdRating (group 1755 survivor swapped; both retail callers MATCH) |

Where more than one exact candidate existed (all ICF-identical 4–8 B tail
calls), the `??1` spelling was preferred over `_Destroy` / `__destroy_aux`,
because none of the others' callers could tell them apart.

**Negative result: a caller the check classed "inert" was charged.**
`0x826DF788` was first named `??1_Rb_tree<TrackerPlayerID,…>`. Its two
retail callers are anonymous (`fn_826DF81C`, `fn_826DF930`), so the caller
check read them "inert". But objdiff pairs both by masked byte signature
with our StreakTracker funclets, and those call `??1map<…>`. Both 44 B rows
fell 100 → 99.545 (−88 B, so the in-tree read was +7 fns but **−52 B**).
Switching to the `??1map` spelling brought both back to 100. **Lesson for
the check:** an anonymous retail caller is inert only if objdiff does not
pair it. A caller that is paired by byte signature has an our-side spelling
and gets charged. A future version of the check should resolve such a
caller through objdiff's pairing rather than by name.

## 4. Skipped (34 addresses), with reasons

**Every candidate charges a caller of another type (8).** Naming these
needs a proven alias group, not a map edit:

- `0x823F1898` (28 B): three message `Get*Data` candidates
- `0x82359680` (224 B): `insert_unique<Symbol,…>` QuestManager vs other
  users
- `0x82775C98` (292 B): `vector<vector<…>>::_M_erase`
- `0x827B74D0` (96 B): ArtEntry vs Marker
- `0x822CD918` (116 B): `~ObjPtr<BandStarDisplay>`
- `0x822E4A30` (164 B): `ObjectDir::Find<RndCam>`, 12 candidates
- `0x82782950` (8 B) and `0x825459E0` (8 B): 7 charges on the latter

**Our body differs (4).** At `0x822C6A40`, `0x823CED00`, `0x823793C8` and
`0x823B08D0` (68 B each), retail's deleting dtor calls `MemFree`, while ours
calls `??3`. These are codegen or source differences, not naming problems.

**No exact body in the pinned or flanking objs (15).**

- `0x8249B200`, `0x82272E50`, `0x822A2E10`, `0x823C1798`, `0x826FE428`,
  `0x827070F0`, `0x82826E28`, `0x823CDCB0`, `0x82316428`, `0x82563258`,
  `0x8268ED70`, `0x8268EF78`
- `0x8271A0B8` (3 exact bodies elsewhere), `0x822DC830` (38 elsewhere),
  `0x82308470` (35 elsewhere). These are generic shapes, so a match outside
  the neighbourhood is not evidence of the type.

**Fold hubs (3).** `0x823D14C0` (289 call sites), `0x8240DDB0` (2,310) and
`0x8238B130` (411).

**The chase REFUTED the rename (3).**

- `0x826FCD78`: `??_GMicClientMapper` was the proposed name.
- `0x82823728`: `Load@UIProxy` is 12 B against retail's 16 B.
- `0x825971F0`: 4 B.

**Already fixed (1).** `0x8268C978`, by W16-QA.

Check: 45 fixed + 8 + 4 + 15 + 3 + 3 + 1 = 79.

## 5. Not done

- No alias groups were opened for the 8 "charges a caller" addresses. Each
  needs per-membership retail-byte proof, which belongs to an alias lane.
- No `none`-ruler reading is quoted as a control for function counts. The
  `none` byte reading was printed by `ab_measure` (batch A: +2,248 B) and
  has the same sign.
- Not merged or pushed. The coordinator does both.
