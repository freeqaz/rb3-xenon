# W16-KC — char second pass + hamobj/gesture/meta/midi/net (2026-10-01)

**Branch** `w16-kc`, rebased onto main `1b8903f35` (after W16-KE and W16-JH).
**Ruler** `name_check` (graded, read from `report.json` `provenance.diff_config`).
**Scope** every unit whose source is under `src/system/char/`, `hamobj/`, `gesture/`,
`meta/`, `midi/` or `net/`. Rows were ranked by `size × (100 − fuzzy)` from the worktree's own
built `report.json`, and the 90–99.99 band was read first.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-kc-ab --patch <git diff main..w16-kc, docs excluded> --label w16-kc-branch`,
**one run**:
- **Worktree:** fresh, at main `1b8903f35`.
- **Patch:** 43 files, kinds map + source + splits; `symbols.txt` untouched.
- **Legs:** both read at a split fixed point (0 extra re-splits each). Leg B had 995 recompiles.
- **Run dir:** archived to `~/tmp/w16kc/ab_run/20261001-132057-w16-kc-branch-1866300/`.

```
leg A: matched=48934 masked=24223 honest=24711 code%=49.680088
leg B: matched=49017 masked=24233 honest=24784 code%=49.896263
Δmatched=+83  Δmasked_equal=+10  Δhonest=+73  Δcode%=+0.216175pp  Δcode_bytes=+22152
Δfuzzy=+0.064778pp   (legA 58.335632 -> legB 58.400410)
units at 100% [mpn]: 347 -> 350 (CharNeckTwist, CharTransDraw, MotionBlur; 0 fell off)
units at 100% [all-rows-fuzzy]: 293 -> 293 (MotionBlur reached; CharTransCopy left, see below)
unit net = +83 over 41 units up / 3 down
```

**Prediction, written before the run** from the in-tree increments on the pre-rebase base
(`e762a9298`: 48,897 → 48,980 functions, 5,083,956 → 5,106,108 B): **+83 (±5) fns, +22.1 KB
(±0.5 KB), 0 rows off 100. Measured: +83 / +22,152 B.**

**Row-level diff of the two archived leg reports (`fuzzy_match_percent`): 67 rows up, 5 down,
0 off 100; 57 rows reached 100.** The 38 GONE/NEW pairs are the renames and re-homes of §4. Every
GONE row that was at 100 reappears at 100 under its new name or unit, except
`?Init@CharTaskMgr@@SAXXZ` (60 B). That row was a false identity (§4), and its address now sits in
an `auto_*` unit. This is the one deliberate −1 fn / −60 B in the total.

**On the `mpn` ruler (`match_percent_normalized`, which `matched_functions` counts), exactly one row
moved off 100:** StorePreviewMgr funclet `fn_827B2660` (40 B), mpn 100 → 99.9, while its fuzzy
*rose* 99.5 → 99.9. Its only charge is the frame offset in `subi r31, r12, 0xd0` (ours `0xc0`).
It inherits StorePreviewMgr::Handle's 0x10 frame residue (§8). Our old `MsgSource::Handle`
forward, which retail does not make, happened to give our frame retail's size. The handler-map fix
is kept on retail evidence. This funclet is the StorePreviewMgr −1 in the unit list.

Unit-list notes:
- **CharNeckTwist −2:** a DENOMINATOR_SHRANK completion. Five rows (the CharTransCopy ctor and its
  funclets) moved to CharTransCopy, and the unit now sits at 100 on mpn.
- **CharTransCopy:** left the all-rows-fuzzy 100 list because those two 99.5 funclets joined it.
  They were already at 99.5 when they sat in CharNeckTwist.
- **CharTaskMgr −1:** the removed false row.

## 2. Population

At lane start (main `e762a9298`, settled worktree build), the scope held **1,209 sub-100 rows, weight
70,395**, of which **600 rows / 105,652 B** sat at fuzzy 90–99.99. The W16-JB classifier, re-scoped
(`~/tmp/w16kc/classify.py`; objdiff-cli `--batch --include-instructions` at the graded ruler), split
the 334 *named* sub-100 rows:

| class | rows | bytes | weight |
|---|---:|---:|---:|
| INSN (insert/delete/replace/diff_op) | 176 | 72,668 | 12,251 |
| RELOCNAME (only relocation-name args) | 82 | 17,148 | 126 |
| ARGMIX | 40 | 11,968 | 46 |
| REGONLY (register args only — skipped) | 24 | 17,760 | 36 |
| NOCHARGE | 12 | 948 | 948 |

The other ~875 rows were anonymous `fn_`. **hamobj and gesture are Dance Central classes that RB3
does not have**: their rows exist only as ICF-folded template names pinned into those units, so no
source work there can be checked against retail. They were not worked.

Method for every edit: full `./tools/ninja-locked` (forced re-split to a `symbols.txt` fixed point
after any map or splits edit), then a whole-report row diff against the previous build. An edit was
kept only if no row fell from 100. The exceptions (rows that went down but not from 100) are listed
in §6.

## 3. Shared templates (`obj/ObjPtr_p.h`)

Every `PropSync(ObjPtrList<T>&)` instantiation in the binary was below 100: 14 rows at 420 B /
92.95 and 6 at 448 B / 86.43–92.28.

- **`ObjPtrList::insert` is inlined everywhere in retail.** Each `kPropInsert` arm is
  `PoolAlloc(0xc)`, a store of `obj` into the node's first word, then an out-of-line
  `Link(it, node)`. No out-of-line `insert` exists in the image. `insert` is now `__forceinline` and
  default-initialises the node (`new Node`, as `push_back` already did; `Link` writes next/prev).
  UIFontImporter's two hand specialisations used `new Node()` and stored zeros; fixed the same way.
  **+16 fns / +6,776 B, 21 rows up, 0 down.**
- **`ObjPtrList::Set` is an inline candidate.** The four 448 B instantiations
  (`CharBone`, `Sequence`, `RndTexBlendController`, `Hmx::Object`) inline `Set`; the 420 B ones keep
  it out of line. The split is the virtual base: the four have `Hmx::Object` as a non-virtual base,
  so `Release`/`AddRef` need no adjustment and the body is small. Plain `inline` reproduced exactly
  that split. **+4 fns / +1,792 B, 0 down.**

## 4. Map and pin defects found on retail bytes

Each was read off the retail body (callees and string literals), not off the map.

| address | was | is | evidence |
|---|---|---|---|
| `0x823A1D38` | `Sfx::Init` | `CharMeshHide::Init` | registers CharMeshHide (`StaticClassName@CharMeshHide`); called from CharInit's slot |
| `0x8264D058` | `CharMeshHide::Init` (pinned in CharMeshHide) | `AppMiniLeaderboardDisplay::Init`, re-pinned | registers AppMiniLeaderboardDisplay; sits between that unit's two blocks |
| `0x823F4810` | `CharTaskMgr::Init` | nulled; whole CharTaskMgr pin removed | registers `"set_bandwidth_logging"`, called from NetSession's static init, next to Quazal code |
| `0x823C7768` | `??0CharNeckTwist` (pinned in CharNeckTwist) | `??0CharTransCopy`, re-pinned | called by `NewObject@CharTransCopy` |
| `0x823CE4C8` | (anonymous) | `??0CharNeckTwist` | called by `NewObject@CharNeckTwist` |
| `0x8236BFE8` | `NewObject@CharNeckTwist` | `NewObject@CharTransCopy` | calls `StaticClassName@CharTransCopy` |
| `0x8236AFA8–0x8236B4C0` | FileMerger pin | Char pin | Char.cpp's factory thunks + `CharTerminate` |
| `0x823AE884–0x823AE918` | CharEyes pin, anonymous | CharWeightable pin, `CharWeightable::Replace` | casts to `.?AVCharWeightable@@`; sits just before CharWeightable's own pin |
| `0x8239FF50` | `DeleteRef(ObjRef*, bool&)` | `DeleteRef(ObjRef*)` | no bool is written, and the caller sets up no third argument |

**Thirteen `NewObject` thunks** in the Char region were named, each from its `StaticClassName` and
ctor callees plus CharInit's registration slot (`~/tmp/w16kc/newobj_id.py`). Also named:
`CharTerminate` (`0x8236B020`), `CharClipGroup::RandomizeIndex` (`0x8238DC68`),
`Character::SyncShadow` (`0x82371B80`), and two `VectorRemove` instantiations
(`0x82370D20` `ObjPtr<RndGroup>`, `0x82370D98` `RndGroup*`; each equals our COMDAT except at the
relocated `_M_erase` call).

**One alias membership withdrawn.** Group `0x8264D058` folded `?Init@AppMiniLeaderboardDisplay@@SAXXZ`
into survivor `?Init@CharMeshHide@@SAXXZ`. Its byte proof was sound but proved identity, not a fold:
the address *is* AppMiniLeaderboardDisplay::Init, and the survivor label was the map misname above.
With both names mapped at their own addresses the validator read it as CONTRADICTED. The membership
is withdrawn with a record; nothing pruned; the report is byte-identical before and after.

## 5. Source fixes

### 5.1 CharInit / CharTerminate (`0x8236CE30` / `0x8236B020`)

Retail CharInit has no `TheCharDebug.Init()`, `CharBonesMeshes::Init()` or `CharLipSync::Init()`
call; `"char_debug"` and `"toggle_char_task_graph"` occur **0 times** in the image, and nothing
allocates CharLipSync's map. CharTerminate calls only `RemoveExitCallback`, `Character::Terminate`
(empty) and `CharBoneDir::Terminate`. Those calls, and `CharTaskMgr::Init`'s `DataRegisterFunc`, are
now HX_NATIVE-only. CharTransCopy is registered between CharTransDraw and CharUpperTwist (the class
exists in this tree; the old comment said it did not).

Six classes (CharIKRod, CharPosConstraint, CharLipSyncDriver, CharGuitarString, CharBoneTwist,
CharPollGroup) moved to the `OBJ_MEM_OVERLOAD` forms: retail's `NewObject` evaluates
`StaticClassName()` and calls `MemAlloc` inline. Their header comments said retail's NewObject calls
a folded `??2CriticalSection` thunk; that reading was of a thunk that is not theirs (the map had no
`NewObject` row for any of the six).

### 5.2 Handler maps read off retail strings

Scanner (`~/tmp/w16kc/handler_scan.py`): every HANDLE*/SYNC_PROP* name whose
NUL-terminated string is absent from the image.

| function | change | before → after |
|---|---|---|
| `Character::Handle` | `merge_draws` (0 occurrences) native-only | 89.86 → 100 |
| `CharEyes::Handle` | `toggle_force_focus`, `toggle_interest_overlay` native-only | 39.86 → 100 |
| `StorePanel::Handle` | no enum-complete message arms in retail | 79.59 → 97.14 |
| `StorePreviewMgr::Handle` | no `MsgSource` superclass forward | 90.27 → 99.95 |
| `CharClipGroup::Handle` | `get_clip` calls `GetClip()`; `randomize_index` arm + `RandomizeIndex()` written | 88.56 → 100 |

### 5.3 The raw-stream rev aggregate (the largest single source lever)

Retail's char `Load` bodies keep **no `BinStreamRev`**: they store `getHmxRev`/`getAltRev` into one
aligned file-scope aggregate (two `sth`), test it with `lhz` + unsigned compares, and read every field
from the raw stream — the shape CharBone/CharTransCopy already used. Detector
(`~/tmp/w16kc/binrev_scan.py`): every in-scope row whose base references `??_7BinStreamRev` and whose
target does not. It found 17 `Load` rows. Sixteen were converted in one batch by
`~/tmp/w16kc/conv_revs.py`, which refuses any other use of `d`: **+17 fns / +4,456 B**, 14 rows to
100, CharCuff to 99.95 (to 100 once its `Symbol("")` temporary was named) and CharWeightSetter
77.39 → 92.81. The seventeenth, CharTransDraw::Load, had a hand-written header and was converted
with the Pre/PostLoad batch below (55.04 → 100). Re-running the detector over all sub-100 rows
in scope afterwards finds none left.

CharBoneDir::PreLoad/PostLoad and CharClipSet::PreLoad use the same aggregate with the rev stack:
retail pushes **before** `ObjectDir::PreLoad` and pops **after** `ObjectDir::PostLoad`. Ours did the
mirror order (also LIFO-balanced). CharClipSet's two separate PostLoad statics are now the one
aggregate both functions address (`lbl_82CBF924`). 17.19 / 54.75 / 38.86 → 100.

### 5.4 Other bodies

- **CharNeckTwist::Poll** — no NaN guard in retail (79.49 → 100); guard native-only.
- **CharTransCopy ctor** — per-TU ObjPtr two-arg inline lever (69.57 → 100).
- **StorePanel::Load** — retail tests `ThePlatformMgr.IsUserSignedIn(StoreUser()->GetLocalUser())`
  (User virtual base, slot 0x64), not `IsSignedIntoLive(GetPadNum())`. A behaviour fix (95.21 → 100).
- **XLSPConnection::Poll** — case-2 polarity on `status == 3` (99.64 → 99.79; mpn 100).
- **CharDriver::Replace** — CharWeightable::Replace first, then `DeleteRef(from)` (5.86 → 100).
- **CharEyes::Replace** — retail walks mEyes/mInterests, retargets with
  `SetOwnerObj(dynamic_cast<…>(obj))` and erases emptied entries (25.60 → 100). The address-arithmetic
  body is the native ring's convention and stays as the HX_NATIVE body.
- **Character::SetShadow / SyncShadow / SyncObjects** — retail removes through the
  `ObjPtr<RndGroup>` and `RndGroup*` instantiations of `VectorRemove`, and SyncShadow has no old-gfx
  rebinding pass (never calls `GetGfxMode`). SetShadow 14.71 → 100, SyncObjects 64.27 → 96.47.

### 5.5 Alias memberships added (all PROVEN, 0 CYCLE)

`tools/fold_thunk_gate.py` then `tools/icf_pair_adjudicate.py --chase`: **9/9 CHASED T1 PROVEN, no
CYCLE leaf**. Seven `??3<Char class>@@SAXPAX@Z` → `??3BinStream@@SAXPAX@Z` (FT1: `b MemFree` with the
resolved target compared); `?Init@CharTaskMgr@@SAXXZ` and `?Terminate@Character@@SAXXZ` →
`0x826C3888` (FT-EMPTY: retail calls an empty function at both sites). `??3CharEyeDartRuleset` was
REFUSED by the gate (retail has its own body at `0x823BEEA8`) and not added.

## 6. Rows that went down (none from 100)

From the archived legs (fuzzy):

| row | before → after | why |
|---|---|---|
| StorePreviewMgr `fn_825C227C` (40 B) | 99.3 → 40.9 | funclet of `0x825C21D0`, a different function. It had byte-signature-paired with the cleanup funclet of our `MsgSource::Handle` call, which retail does not make (§5.2) |
| StorePreviewMgr `fn_827B26F0`, `fn_827B2738` | 99.9 → 99.8 | Handle funclets re-paired, same cause |
| StorePanel `fn_827B5B38` | 99.9 → 99.4 | Handle funclet re-paired after the enum-complete arms left (§5.2) |
| CharCuff `fn_8239F58C` | 99.4 → 99.3 | funclet re-paired after the Load conversion (§5.3) |

Plus the mpn-only row in §1 (`fn_827B2660`).

## 7. Tried and reverted (inert)

- **MemcardMgr::ThreadDone** — folding `kS_None` into `default` to get retail's `< 1` range lowering.
  MSVC still emits `== 0`. Inert.
- **user-declared `~ProfileSwappedMsg`** for StorePanel::Handle's temporary: 285 objects recompiled,
  report byte-identical. Inert.
- **`RB3_OBJPTR_INLINE_TWOARG_CTOR` in CharWeightSetter.cpp** — shadowed by the owner-ctor lever
  already there (`obj/Object.h`'s `#ifdef/#elif` chain). Inert.

## 8. Left, by blocker

- **XboxPurchaser** (`meta/StorePurchaser.cpp`): `Poll` (304 B at 1.3), `Initiate`, `IsPurchasing`,
  the dtor and an analytics call (`fn_827B2AF8` → `fn_827B2A18`, `"store/purchase"`) all need the
  class-static `XOVERLAPPED` at `0x82E0684C` modelled. A multi-function port, not attempted.
- **`0x825C21D0`** is mapped `??0PreviewDownloadCompleteMsg` but builds a `RemoteMachineUpdatedMsg`;
  its funclet re-paired after StorePreviewMgr::Handle lost the MsgSource call (§6). Not renamed: the
  real PreviewDownloadCompleteMsg ctor was not located.
- **Scheduling-only residue**: CharIKFingers::SetName (2,076 B at 99.23: two `addi` placed after a
  `mr` in 2 of 40 identical `Find` sites), Character::PostLoad (a dead stack store + an `fmuls`
  operand order; the in-tree comment records an earlier attempt).
- **Frame-size residue (0x10)**: StorePanel::Handle and StorePreviewMgr::Handle — retail reserves one
  more 16 B slot; no source cause found.
- **Large FP/regalloc rows not attempted**: CharIKHand::IKElbow (2,880 B at 94.0), CharHair::
  SimulateInternal, CharEyes::Poll/LidTrackAndClampingUpdate, CharBonesSamples::Relativize/
  EvaluateChannel, json_tokener_parse_ex.
- **Mid-size INSN rows not attempted**: CharLipSync::PlayBack::Poll (564 B at 30.7),
  Character::DrawLodOrShadow (656 B at 52.3), CharClipDriver::ExecuteEvent, CharClipSet::LoadCharacter/
  PostSave/SetBpm/ResetPreviewState, CharBonesMeshes::ReallocateInternal.

## 9. Gates

Run on the rebased tip `e97af9c11`, before this docs-only commit:

- `python3 tools/map_name_injectivity.py`: **OK**, 32,790 applied rows, 32,789 distinct names,
  injective (+1 enumerated internal-linkage exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,609 map-consistent, 268 tolerated,
  **0 contradicted**, 1,878 total. Before the §4 withdrawal it read **FAIL**: group `0x8264D058`
  became CONTRADICTED once both of its names were mapped at their own addresses.
- `tools/native_build_gate.sh`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
- **Alias additions:** 9 memberships, each `CHASED T1: PROVEN` under
  `tools/icf_pair_adjudicate.py --chase`, with **0 CYCLE** lines in the run
  (`~/tmp/w16kc/chase.log`).
- **`symbols.txt` is unchanged** on the branch (`git diff main..w16-kc -- config/45410914/symbols.txt`
  is empty). Every map/splits edit was built with a forced re-split, and the tree stayed clean.

## 10. Rebase

Branched at `e762a9298`; rebased onto `1b8903f35` (W16-KE's 34 mis-carve fixes and W16-JH's alias
decisions). The rebase applied with no conflicts. It was then re-checked:
- a forced re-split full build: clean tree, no `splits.txt`/`symbols.txt` drift;
- both map gates;
- the native gate.

The A/B in §1 is against `1b8903f35`.

## 11. Leads outside this lane's scope

The `BinStreamRev`-only-on-our-side detector (`~/tmp/w16kc/binrev_scan_all.py`) was run tree-wide.
It crashed partway (a unit with no `functions` key) after reporting seven rows with the §5.3 shape:

| row | size | fuzzy |
|---|---:|---:|
| `RndDir::PreLoad` | 148 | 38.86 |
| `RndDir::PostLoad` | 428 | 64.16 |
| `RndAnimatable::Load` | 1,304 | 91.81 |
| `WorldDir::PostLoad` | 1,372 | 60.49 |
| `BandList::PreLoad` | 136 | 28.59 |
| `StreakMeter::PreLoad` | 616 | 73.32 |

The seventh is a 40 B Anim funclet. **The census is incomplete:** the remaining units were not
scanned.
