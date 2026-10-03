# W16-OQ — W16-OO leftovers, then sub-100 rows in `src/system/{char,world}` (2026-10-03)

**Branch** `w16-oq`, started off main `8f4ba3da1`, rebased onto `08777873d` (the W16-OP merge) with no conflicts.
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`). The permuter was not run.
`src/network`, the Quazal block, `src/band3` and the `src/system/bandobj` *sources* were not edited. Two map/splits
changes do move rows **into** bandobj units (BandScoreboard, §2.3) and DirUnloader (obj); no bandobj source
line changed.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16oq-ab --patch <git diff main w16-oq>` on a fresh
`scripts/setup_worktree.sh` worktree at main `08777873d`. Patch kinds: map + source + splits, 9 paths. Forced
re-split on both legs. Run dir `~/tmp/wt-w16oq-ab/.ab_measure_runs/20261003-041054-ab_branch-2465914/`, `rc=0`.

```
leg A: matched=53226 masked=25132 honest=28094 code%=57.326530  (recompiles: 0, settled)
leg B: matched=53247 masked=25139 honest=28108 code%=57.360065  (recompiles: 132, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+21  Δmasked_equal=+7  Δhonest=+14  Δcode%=+0.033535pp  Δcode_bytes=+3436
Δfuzzy=+0.008083pp   (legA 63.545033 -> legB 63.553116)
unit improvements: 7 unit(s), sum +24
unit net (ALL units) = +21   vs whole-binary Δmatched = +21
units at 100% [mpn ruler]: legA 524 -> legB 524 (0 reached 100, 0 fell off; pairable units 1732->1730)
[control none] Δmatched_code=+2968 B -- NOT_APPLICABLE (patch carries source)
```

**Prediction, written before the run** (in-tree row diff against the pre-lane `report.json`): **+21 fns /
+3,436 B, 0 rows down. Measured: +21 / +3,436 B, identical.** A row diff of the two legs' own `report.json`s
reads `up=10 down=0 OFF100=0`.

- **No row went down.** The unit-sum gap (+24 vs +21) is re-homing only: DrivenPropertyEntry's 2 matched rows
  moved to BandScoreboard (DrivenPropertyEntry now has no function rows), one matched UIList funclet moved with
  `__uninitialized_fill_n` to DirUnloader, and NetCacheMgr/UIList/WavMgr each lost the row they never owned.
  Pairable units −2 = DrivenPropertyEntry emptied + `auto_03_82845FF0_text` merging into its neighbour once the
  WavMgr pin is gone.
- Δmasked_equal +7: EH funclets of the re-homed vector COMDATs now pair by byte signature in BandScoreboard.
- Per unit (matched fns / matched bytes): BandScoreboard 70→83 / +1,568 B · PropSync 156→161 / +1,236 B ·
  Waypoint +1 / +328 B · DirUnloader +2 / +152 B · CharLipSyncDriver +1 / +88 B · Character +1 / +84 B ·
  Msg +1 / +84 B. `operator>>(BitmapOverride)` 87.3 → 97.9 and WorldCrowd::DrawShowing 88.67 → 88.68 move
  fuzzy only.

## 2. W16-OO's leftovers (§6 of `W16OO_SYSTEM_RUNTIME_DIRS_SUB100_2026-10-03.md`)

### 2.1 `operator>>(BinStream&, WorldDir::BitmapOverride&)` — done, plus two world ctors

| row | B | before → after | what retail does |
|---|---:|---|---|
| `operator>>(BinStream&, BitmapOverride&)` | 380 | 87.31 → 97.90 | the empty-name arm is `if (mObject) { Release(this); mObject = 0; }` (`ReleaseObjConcrete()`), not a `SetObjConcrete` call |
| `??0WorldDir` | 1,024 | 88.40 → 100 | `mHUD`, `mTestLightPreset1/2` are built with the inlined owner-only ObjPtr ctor (three stores + the EH temp store): `ObjPtrInlineOwner` |
| `??0MatOverride` | 92 | 54.96 → 100 | `mesh` inline (vftable 0x82011184 = `ObjPtr<RndMesh>`), both `RndMat` ptrs out of line |
| three WorldDir unwind funclets | 3 × 40 | 99.4 → 100 | follow the ctor |

`operator>>`'s remaining charge is `&c.replacement` computed before the `Find<RndTex>` call in retail and after it
in ours. Two spellings tried (a `RndTex *` local; a reference) were byte-identical to the original. Left.

### 2.2 Misnamed map rows — identified on retail bytes, renamed

| address | was | is | evidence |
|---|---|---|---|
| 0x82766828 (84 B) | `??0NetLoaderRef@@QAA@ABU0@@Z` | `list<MsgSource::EventSinkElem>::erase(iterator)` | unlinks `*it`, frees a 0x14-byte node via `MemOrPoolFreeSTL`, returns next: `erase` for a trivially destructible 12-byte T (`EventSinkElem` = `Sink` 8 B + `Symbol`). Sits in Msg.cpp's `.text` span; callers `EventSink::Remove`, `PropSync<list<EventSinkElem>>`, `MsgSource::Export` (and `GemManager::PruneHitGems` through the fold). Re-homed NetCacheMgr.cpp → Msg.cpp; the row reads 100. |
| 0x82845F78 (120 B) | `?Get@?$ResMgr@X@@QAAPAXVCRC@Hmx@@@Z` | unnamed, unpinned | a lookup (`fn_82845370`) then `(p - base) >> 12` indexes 8-byte entries and decrements the byte at +4; returns 1/0. **Only vendor code calls it** (0x82847FD8, 0x8284936C), beside `XapiInitHeap`: XAPI heap code. Name and WavMgr pin dropped (both from lane AD's `6c48bb674` fixpoint); the block returns to `auto_03`. Not ported (XDK). |

`fn_82766880` (100 B, now in Msg) is the matching `list<12-byte T>::insert(pos, val)`. It is **left unnamed**: its
other callers are `GemManager::Hit`/`PartialHit` and a Mesh-unit row named `list<int>`'s copy ctor at 0x82766EF8 —
an address that is itself inside Msg's span and probably misnamed. Naming it would put a bet on two other units.

### 2.3 The `vector<FlowMathOp>` rows are BandScoreboard's `vector<ObjPtr<RndMesh>>`

Retail RB3 has no FlowMathOp. The 0x822CDC18…0x822CEDA0 rows sit between BandScoreboard's own functions, and
`_Param_Construct` (0x822CDC18) calls `fn_822CDB70`, a copy ctor that stores vftable 0x82011184, whose slot 0 is
`??_G?$ObjPtr@VRndMesh@@`. DrivenPropertyEntry.cpp's pins held nothing else except a 12-byte block at 0x82781654.

- Renamed `_Param_Construct`, `__uninitialized_fill_n`, `__uninitialized_copy`, `_M_insert_overflow_aux`,
  `_M_fill_insert`, `resize` to their `ObjPtr<RndMesh>` spellings (each verified defined in our BandScoreboard.obj).
- Named 0x822CDFC0 (392 B) `_M_fill_insert_aux`: its only callers are itself and `_M_fill_insert`.
- `fn_822CE170` (116 B, 30 callers across 15 units, a folded vector-dtor helper) stays anonymous and unpaired.
- Re-homed every range DrivenPropertyEntry.cpp → BandScoreboard.cpp; dtk moved the `.pdata` itself.
- Alias group 0x822cdc18: survivor relabelled (`survivor_renamed` record). `_Copy_Construct<ObjPtr<RndMesh>>`
  admitted on its own pair proof (§3). The 10 pre-existing DebugGraph / `map<Symbol,String>` members are
  referenced by no compiled obj (STALE_SPELLING) and re-chase REFUTED on MISSING(ours); left in place, not pruned.

The last one, UIList's `_M_allocate_and_copy<FlowMathOp>` at 0x8276CB20, is **DirUnloader's
`__uninitialized_fill_n<ObjPtr<Hmx::Object>>`**: it loops `_Copy_Construct<ObjPtr<Object>>` (0x8276C970) with a
0xC stride, fills the hole between DirUnloader's `.text` ranges, and its one caller is DirUnloader's
`_M_insert_overflow_aux` (0x8276CC68). W16-LB had already proven our spelling equal to it and folded it under the
FlowMathOp name. That spelling is now the map name and group survivor (`folded` emptied, group kept as the record);
the range moved UIList.cpp → DirUnloader.cpp and reads 100.

### 2.4 Group 0x8240ddb0's `contradiction_exempt` — deleted

After W16-OO dropped the colliding `??3@YAXPAX@Z` row only the survivor is named in the target objs, so the
exemption was inert. Deleted; `icf_alias_finder.py --validate` stays PASS with 0 contradicted.

## 3. Aliases: every new or re-parented membership, chased

All with `tools/icf_pair_adjudicate.py --chase`. Admission rule (W16-LB's): CHASED PROVEN, **0 CYCLE-ASSUMED**,
no contradiction with an existing group.

| group | spelling | verdict | note |
|---|---|---|---|
| 0x82766828 | `erase<GemManager::HitGem>` | flat + chased PROVEN, 84 = 84 B | re-parented under the new survivor |
| 0x82766828 | `erase<ReadRequest@?A0x33d3fe85>` | flat + chased PROVEN, 84 = 84 B | replaces the stale `?A0x7d2eca9f` spelling (in no compiled obj); group moved STALE_SPELLING → MAP-CONSISTENT |
| 0x822cdc18 | `_Copy_Construct<ObjPtr<RndMesh>>` | flat PROVEN (placeholder callee), **chased PROVEN** | callee slot discharged by RTTI as `ObjPtr<RndMesh>`'s copy ctor |
| 0x823dac48 | `_Copy_Construct<ObjOwnerPtr<Waypoint>>` | flat PROVEN with the relocation **name** agreeing (`reloc_tally {}`), chased PROVEN | |
| 0x82788308 | `push_back<Hmx::Rect>` | chased PROVEN, 0 cycle-assumed (7 FOLD-OK, 3 DATA-ACCEPTED) | WorldCrowd::DrawShowing |

The last two came from a sweep: every charged `(retail callee, our callee)` pair in the char/world sub-100 rows
(72 function pairs) went through `--chase`. **Chased PROVEN but NOT installed**, with the reason:

- `vector<T*>::_M_fill_insert` → `vector<Object*>` (SpotlightDrawer, Spotlight, RndEnviron, RndLight, CamShot),
  `resize` → `StreakInfo` (MirrorOp, SpotlightEntry), `resize<ColorSet>`, `_M_fill_insert<SingerResultsData>`,
  `__introsort_loop<Category>`: **one CYCLE-ASSUMED leaf each** — the class W16-JE/W16-OK parked.
- `_Destroy_Range<IKTarget>` → `__destroy_range<IKTarget>` (0x823977b0), `_M_fill_insert<IKTarget>` →
  `_M_fill_insert<Key<Weight>>` (0x82398798), `_Param_Construct<Lod>` → `_Copy_Construct<Lod>` (0x8236f588):
  each of our spellings is **already folded at a different address** (groups 926 / 2044 / 897). Admitting would
  put one spelling at two addresses. `resize<IKTarget>`'s proof rests on the `_M_fill_insert<IKTarget>` slot and
  inherits the conflict. One side of each pair is wrong; that needs its own adjudication.
- `StlNodeAlloc<_List_node<int>>` ctor ← `get_allocator<ConstraintSystem>`: flat T1 VACUOUS.

## 4. `src/system/{char,world}` sub-100 rows

Population measured on the branch after the §2 commits (pre-rebase `ca5975a63`, so the WorldDir/MatOverride
rows are already gone from it): **194 rows / 97,012 B** with 0 < fuzzy < 100 (char
121 / 57,644 B, world 73 / 39,368 B), plus 59 rows at fuzzy 0. Charge classes (W16-NA's classifier):
STRUCT_INSDEL 67 / 54,904 · REG_ONLY 22 / 19,740 · IMMEDIATE 46 / 12,488 · NAME_ONLY 48 / 6,396 ·
STACK_REG 2 / 1,632 · OPCODE:replace 8 / 1,620 · NAME+REG 1 / 232. The large rows (Spotlight::SyncProperty,
LightPreset::Load, CamShot::Load, CharIKHand::IKElbow, CharHair::SimulateInternal) carry prior AT_LIMIT records
or are FP-scheduling residue and were not reopened.

Fixed (both behaviour-shaped):

| row | B | before → after | defect |
|---|---:|---|---|
| `Character::CopyBoundingSphere` | 84 | 80.95 → 100 | retail (0x8236F178) tests `c->mSphereBase` first; both arms tail-call `SetOwnerObj`, one with the pointer, one with 0 |
| `CharLipSyncDriver::SetLipSync` | 88 | 83.41 → 100 | retail (0x8238D1C0) returns **void** — no `r3` on either path. Header, body and the map row now say void; hamobj's `HamCharacter` (DC3 layer, the only consumer of the bool) compares `LipSync()` before the call |

## 5. Tried and reverted (negative results)

1. **`Character::CalcBoundingSphere` (1,340 B, 6 charges).** The L-clavicle `Distance` computes y before z and the
   R-clavicle one (same expression, same liveness) matches. Giving the L block the R block's `auto dist` form,
   and the reverse, were both byte-identical. Scheduler residue.
2. **`CharLipSyncDriver::Sync` (224 B, 3 charges).** Retail reads `mBlinkClip.mObject` as `0x48(this)` in the
   release arm. Direct member access in both arms: 96.3 → 94.6. Reference for `Find`, member for release: 96.3
   (unchanged). Reverted.
3. **`NgSpotlightDrawer::RenderScene` (588 B, 1 charge).** Retail tests `(end - begin) >> 3` (`srawi.`); ours folds
   it to `& ~7` (`clrrwi.`). `sLights.size()` into the int local, and `sLights.size() != 0` inline, were both inert.
4. **`SpotlightDrawer::Init`** — already recorded in the source as a negative result (INSDEL-2); not retried.
5. **`CharClip::Transitions::Resize` (200 B, 1 charge)**: retail stores `mNodeStart` to frame slot 0x50 before
   `MemRealloc` (a dead store); no source construct found. **`PlayBack::Poll` (564 B)**: one extra no-op
   `clrrwi r11,r11,0` after `mFrame++` plus an `lbzx` operand order. **`CharEyes::Enter` (356 B)**: store order
   of independent member stores. Left.
6. **`WorldCrowd::CharDef::Load` / `Spotlight::BeamDef::Load`**: retail addresses the rev word as its own
   `sym+4` relocation in these small loaders, while ours uses `base + 4`. The source comment in both TUs records
   that the `Load` functions need the aggregate. This is per-function addressing, not a layout change; left.

## 6. Leads (not done)

- **The three conflicting IKTarget/Lod fold pairs** (§3): retail calls `__destroy_range<IKTarget>`,
  `_M_fill_insert<Key<Weight>>` and `_Copy_Construct<Lod>` where existing groups place our spellings elsewhere.
  Either those groups are wrong, or our STLport calls a different helper than retail's (e.g.
  `vector<IKTarget>::operator=` reaching `_Destroy_Range` where retail reaches `__destroy_range`). Adjudicate per
  address before touching either group.
- **`fn_82766880`** (`list<12-byte T>::insert`) and **0x82766EF8** (map: `list<int>` copy ctor, inside Msg's span):
  identify together.
- **`??_GCharDriverMidi` (88 B, 58%)**: retail calls `??1CharDriver` on `this-0xa4+0x90` then adjusts; the
  deleting-dtor shape differs from ours. Needs CharDriverMidi's base/vbase layout checked with the compiler.
- **`CharClip::Transitions::Resize`'s dead store** (§5.5).

## 7. Gates

- A/B: §1.
- Build-time checks on the final tree: `CHECK RULER AGREEMENT` passed; `[patch-state] OK: tree is a fixed point of
  6 post-compile passes`.
- `icf_alias_finder.py --validate`: PASS, 0 contradicted (1799 map-consistent / 310 tolerated).
- No added source line cites rb3-Wii or "the oracle" (`git diff main -- src | grep -E '^\+' | grep -iE 'wii|oracle'`: no hits).
- Native gate: __GATE__
