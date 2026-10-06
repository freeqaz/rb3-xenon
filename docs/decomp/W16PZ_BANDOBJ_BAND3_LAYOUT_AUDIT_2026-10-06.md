# W16-PZ — class-layout audit of the bandobj + band3 classes the native targets link

**Lane W16-PZ, 2026-10-06, worktree `~/tmp/wt-w16-pz`, branch `w16-pz`, rebased
onto main `a3f3339a8` (clean retail TU5 image, `band.exe` sha1 `5f3f667a…`).**

Question: which members of the classes the native build links have the wrong
offset, size or type in our headers, measured against retail bytes rather than
against our own `// 0xHEX` comments?

**Answer: no member offset, member type or base-subobject offset in this
population disagrees with retail. The defects are ODR splits: four classes had
two definitions of different sizes inside the match build, and one of them,
`MetaPerformer`, had the wrong size in 21 of 22 TUs.** All four are fixed in
`5eac96dc6`, measured Δ0 on every key with 0 of 68,909 rows moving.

## 1. Population

The native build compiles 16 `src/system/bandobj/` TUs (counted from
`native/build/build.ninja`, direct and scatter-included):

BandCamShot, BandCharacter, BandCharDesc, BandConfiguration, BandCrowdMeter,
BandDirector, BandFaceDeform, BandHeadShaper, BandIKEffector, BandList,
BandPatchMesh, BandRetargetVignette, BandWardrobe, CharKeyHandMidi,
OutfitConfig, SongSectionController (+ `bandobj/Band.cpp`, which is only
`BandInit()` and declares no class of its own).

It also compiles 45 `src/band3/` TUs (`game/` 22, `bandtrack/` 5,
`meta_band/` 18; list in §7). Every class complete in any of those 61 TUs was
laid out by the compiler: `class_layout_report.py --all-classes --json`, one
compile per TU, all 61 returned `status=OK` (zero `COMPILE_FAILED`/timeouts).
`bandobj/Band.cpp` has no X360 compile edge of its own; its contents were
audited through `BandCharacter.cpp`, which scatter-includes it.

## 2. Instruments, and what each one can and cannot see

Every instrument was chosen so the retail side comes from bytes, not names or
comments. Each one has a positive control showing it can fire.

| # | instrument | retail side | scope | result |
|---|---|---|---|---|
| A | `sizeof` vs retail allocation (`tools/retail_sizeof_witness.py`) | `li r3,N; bl alloc; bl ctor` immediates, class named by the vtable's RTTI | 447 classes with both a witness and a compiler layout | 435 agree, 12 differ, all adjudicated in §3 |
| B | factory/ctor/dtor/Copy/Save/Load match status | the bodies themselves (`fuzzy == 100` includes immediates) | 16 bandobj classes | all 100 except `BandDirector` ctor (98.27, §4) |
| C | vtable slot order (`tools/vtable_order_sweep.py --class`) | retail `.rdata` vtables via RTTI | 16 bandobj classes, 53 retail vtables | 0 PERMUTED, 0 SET_DIFFER |
| D | member-access multiset on every sub-100 named row | `(opcode, displacement)` over non-frame memory operands, order-free | 52 bandobj + 40 band3 rows | no offset or width/type difference (§4) |
| E | placeholder vtable / type-descriptor type check (new, §5) | RTTI name behind every `lbl_` vtable/TD the retail code references | 745 bandobj + 342 band3 sites | every disagreement is a known naming difference |
| F | base-subobject offsets vs retail RTTI `BaseClassDescriptor.mdisp` | `.rdata` class-hierarchy descriptors | 735 classes | 0 real disagreements; sabotage control flags 186 |
| G | cross-TU layout consistency (ODR) | — (internal: same class, different TUs) | every class in the 61 reports | found the four splits |

What none of these can see: the type of a member that retail only ever copies
wholesale (an `int` vs a `float` copied by `lwz`/`stw` looks the same), and the
order of two members that no retail function distinguishes. §6 lists where that
leaves real uncertainty.

## 3. Instrument A: retail `sizeof`

All 13 bandobj classes with a retail allocation site agree with the compiler:

| class | retail alloc | ours |
|---|---:|---:|
| BandCamShot | 536 | 0x218 |
| BandCharacter | 2088 | 0x828 |
| BandCharDesc | 608 | 0x260 |
| BandConfiguration | 872 | 0x368 |
| BandCrowdMeter | 756 | 0x2f4 |
| BandDirector | 364 | 0x16c |
| BandFaceDeform | 52 | 0x34 |
| BandIKEffector | 160 | 0xa0 |
| BandList | 908 | 0x38c |
| BandRetargetVignette | 80 | 0x50 |
| CharKeyHandMidi | 204 | 0xcc |
| OutfitConfig | 252 | 0xfc |
| SongSectionController | 156 | 0x9c |

`BandWardrobe` has no census witness, but `?NewObject@BandWardrobe@@` is at 100
with `li r3, 0xe0` = our 0xe0. `BandPatchMesh` (0x24) and `BandHeadShaper`
(0x28) are never heap-allocated by retail. They are embedded or stack locals,
and their ctors (`??0BandPatchMesh@@QAA@PAVObject@Hmx@@@Z`,
`??0BandHeadShaper@@QAA@XZ`) match at 100.

Across all 61 TUs, 435 witnessed classes agree and 12 differ:

| class | retail | ours | verdict |
|---|---:|---:|---|
| **MetaPerformer** | 940 | 952 in 21 TUs, 940 in its own | **real: ODR split, fixed (§3.1)** |
| **DialogDisplay** | 88 | 1 in the BandCharacter TU | **real: empty stub class, fixed (§3.2)** |
| **InstrumentDifficultyDisplay** | 432 | 1 | **same** |
| **MicInputArrow** | 500 | 1 | **same** |
| **PlayerDiffIcon** | 440 | 1 | **same** |
| **ScrollbarDisplay** | 452 | 1 (+ a 2nd stub in CameraTilt.cpp) | **same** |
| FadePanel | 200 | printed 196 | printing artifact, see below |
| GamePanel | 392 | printed 388 | printing artifact |
| MainHubPanel | 248 | printed 244 | printing artifact |
| BandMatchmaker | 168 | printed 164 in 5 TUs, 168 in 2 | printing artifact (presumed) |
| NetSavedSetlist | 120 and 136 | 120 | census attribution: the 136 B sites are `new BattleSavedSetlist` with an inlined ctor (`MusicLibraryNetSetlists.cpp` cases 1000–1002) |
| FilePath | (member-0 witness only, no size) | 12 | nothing to compare |

**The printing artifact.** For a class with a virtual base and 8-aligned
members, the `size(N)` line of `/d1reportSingleClassLayout` can omit the tail
padding. `FadePanel` is the proof: the `Game.cpp` TU prints `size(196)`, but
`?NewObject@FadePanel@@` compiled in the same TU emits `li r3, 0xc8` (200),
which matches retail. The same holds for `GamePanel` and `MainHubPanel`, whose
factories are at 100. `BandMatchmaker`, `UIManager` (172/176) and
`WiiFriendMgr` (300/304) print two sizes over one identical member layout. That
is the same shape, but I did not run a sizeof probe on them, so it is presumed
rather than shown. The tool already labels these sizes `printed-UNVERIFIED`;
this is a concrete case where that label matters.

### 3.1 MetaPerformer: 940 B in one TU, 952 B in 21

`MetaPerformer.h` gated its three Wii/dev-only members (`mWiiPending`,
`mLastVenue`, `mVenueOverride`) on `#ifndef RB3_NO_WII_META_MEMBERS`, and only
`MetaPerformer.cpp`'s compile carries `/DRB3_NO_WII_META_MEMBERS` (the only
TU in `build.ninja` that does). So:

- in `MetaPerformer.cpp`: 940 B, vbase `Hmx::Object` at 0x384. This is retail
  (`MetaPerformer::Init` allocates `0x3ac`, matched at 100);
- in 21 other measured TUs (`BandCharDesc`, `OutfitConfig`, `GemManager`,
  `TrackPanel`, …): 952 B, with the three members at 0x380/0x384/0x388 and the
  vbase at 0x390.

Fix: the header derives one macro,
`RB3_META_PERFORMER_WII_MEMBERS = defined(HX_NATIVE) && !defined(RB3_NO_WII_META_MEMBERS)`,
and the header and `.cpp` gate on it. Every match-build TU now gets the retail
940 B. Native defines `HX_NATIVE` and not the opt-out, so it keeps the members
and the venue-override handlers exactly as before. The per-TU
`/DRB3_NO_WII_META_MEMBERS` in `objects.json` is now redundant but harmless. I
left it alone so the documented `/D` census does not shift under this lane.

### 3.2 Band.cpp's five 1-byte stub classes

`bandobj/Band.cpp` declared
`class DialogDisplay { public: static void Init(); };` and the same for
`InstrumentDifficultyDisplay`, `MicInputArrow`, `PlayerDiffIcon` and
`ScrollbarDisplay`. The comment said "TUs not yet ported", which is no longer
true: all five have real headers and compiled TUs. Because `Band.cpp` is
scatter-included into `BandCharacter.cpp`, that TU carried a second, 1-byte
definition of each class. Native also compiles that TU, so the native program
held two definitions too. The real headers give 88/432/500/440/452 B, which
equals retail's factory allocations (re-measured with
`class_layout_report.py --exact --tu src/system/bandobj/<X>.cpp`). Each real
header declares `static void Init();` out of line, so `BandInit()`'s call
sequence cannot change. The fix replaces the stubs with `#include`s.

The same class had a third definition in `gesture/CameraTilt.cpp`: a lane-AE
force-emit `class ScrollbarDisplay { OBJ_CLASSNAME(ScrollbarDisplay); }`, kept
to emit `?StaticClassName@ScrollbarDisplay@@` before the class was ported.
`default/ScrollbarDisplay` now matches that row at 100 from the real TU, and
CameraTilt has no `objdiff.json` unit, so the stub contributed nothing except
the conflicting definition. Removed.

## 4. Instruments B and D: member offsets and types in code

**B.** For the 16 bandobj classes, every factory, ctor, dtor, `Copy`, `Save`,
`Load` and `Init` row found is at fuzzy 100, except `??0BandDirector@@QAA@XZ`
(98.27, 1088 B). A matched ctor fixes the offset and store width of every
member it initializes. The BandDirector residue is one store moved: retail
initializes `unk108` (0x114) from an unnamed `.rdata` float `lbl_8200EDA8`,
which equals -1.0f (read from `auto_00_82000400_rdata.s`). Ours uses the pooled
`__real@bf800000` and schedules the store 7 instructions earlier. Same offset,
same width, same value, so this is not a layout defect.

Members a ctor does *not* touch (uninitialized PODs) were listed per class and
followed to their uses. The ones that mattered, such as `BandCharacter::unk6f0`
/ `unk6f4[64]`, are read and written only by
`OnPortraitBegin`/`OnPortraitEnd`, both at 100. `BandHeadShaper`'s eight
members are set in `Start()`/`Init()`. `BandWardrobe`'s `mTargets[4]`,
`unk78`, `unk7c` and `mPlayerForcedFocuses[4]` are covered by its 100% ctor,
`Load` and `SyncProperty` bodies.

**D.** I compared each sub-100 named row as an **order-free multiset** of
`(opcode, displacement)` over memory operands whose base register is not the
frame (`r1`, or any register set by `subi/addi/mr rX, r1`). Scheduling and
register allocation cancel out; a wrong member offset, or a width/type change
(`lwz`↔`lfs`, `stb`↔`stw`), survives. My first positional scan flagged 30+
"deltas", every one of them scheduling or frame-slot shifts. The order-free form
removes that noise without hiding a real move.

- bandobj, 52 rows: 9 rows have a residue, each one extra or one fewer *same-offset*
  reload (CSE differences). Examples: `AddOverlays` reloads `0x98` because
  retail rotates the loop and tests `begin != end` first; `Poll@BandRetargetVignette`
  re-reads `TheBandWardrobe`. The `PatchPair` ctor residue is retail calling the
  `ObjPtr<RndTex>` ctor out of line. That is an inlining difference, not layout.
- band3, 40 rows: 4 rows with a residue, all single same-offset reloads
  (`SetupGems` `0x23f8` ×6, `DrawTrackMasks` `0x12c`, …).

No row shows a member at a different offset, or accessed with a different width
or type.

## 5. Instrument E: types hidden behind placeholder relocations (new)

`name_check` **forgives** a relocation whose retail-side name is a placeholder
(`lbl_`/`fn_`/…). Retail vtables and RTTI type descriptors are often unnamed.
So a member declared `ObjPtr<RndMesh>` where retail has `ObjPtr<RndTex>`, or a
`dynamic_cast<T>` to the wrong `T`, scores 100 whatever we write. This is the
one place a wrong member *type* could hide inside a 100% ctor.

Instrument: for every `lbl_XXXXXXXX@l` in the unit's retail asm that RTTI
resolves (`RetailRtti.class_of_vtable` for a vtable, `td_name` for a type
descriptor), take the base-side symbol at the same objdiff instruction and
compare the class names.

- bandobj: **745** sites resolved. 53 disagree, all explained: 48 are
  `.?AVObjRef@@` vs our `ObjRefOwner` (the engine's known renamed root), 4 are
  the normalizer missing `ObjPtr<ObjectDir, ObjectDir>`'s `V1@` back-reference,
  and 1 is `fn_822B0D48`, an unpaired anonymous row.
- band3: **342** sites, 6 disagree: 5 anonymous-namespace hash spellings
  (`KickPlayerMsg`, `MainHubAdvanceMsg`) and 1 `ObjRef`/`ObjRefOwner`.

So no embedded `ObjPtr`/`ObjPtrList`/`ObjOwnerPtr` member carries the wrong
template argument, and no `dynamic_cast` names the wrong type. The 53 and 6 are
also the positive control: the comparator fires on any name difference.

## 6. Instrument F: base-subobject offsets

Compared every retail `BaseClassDescriptor` with `pdisp == -1` (non-virtual
base), using its `mdisp`, against the offset at which that base's first member
appears in our layout. Normalization: `ObjRefOwner`↔`ObjRef`, and template
names compared up to the first `@`.

- **735 classes checked, 2 flagged, both not ours**: `NetZCallback` (Quazal,
  out of scope) and `Node` (a bare-name collision between two unrelated `Node`
  classes in the reports).
- Sabotage control: adding +4 to every nonzero offset on our side flags
  **186** classes (e.g. `WorldDir | RndDrawable@160 ours[164] …`). The
  comparator can fail.

Example of the agreement, `BandCharacter`: RndDrawable 0xa0, RndAnimatable
0xc4, RndTransformable 0xd4, RndPollable 0x188, MsgSource 0x190, BandCharDesc
0x268, MergeFilter 0x49c, CompressTextureCallback 0x4a8. All eight equal
retail's `mdisp`.

### What remains genuinely unverified

- **Type of copy-only members.** A member that retail only copies (`lwz/stw`)
  or zero-fills can be `int` or `float` and no instrument here distinguishes
  them. Example: `MeshVert::unk4/unk10/unk1c` in `BandPatchMesh` are typed by
  their arithmetic in `AddUV`/`Normalize`, so they are covered; plain
  `int unk…` fields stored only by the ctor are not.
- **`BandPatchMesh::MeshVert` is modelled as a fixed 0x3c struct plus raw byte
  offsets** (`kMVFaceList = 0x3a`, `kMVTwinFlag`, `kMVSlotBase`). Retail lays
  out a variable-length per-vertex record with an inline `unsigned short`
  face list. The offsets are right, since every access matches, but the type
  is a cast-based model. It could be expressed as a flexible trailing array
  without changing codegen. I left it alone: it is a representation cleanup,
  not a layout error.
- Retail vtables for classes in §3's "printed artifact" rows were not
  re-probed with a direct sizeof compile.

## 7. Instrument G: cross-TU consistency, and the list of what was audited

For every class name appearing in more than one of the 61 reports, I compared
the member lists `(offset, name)` and the sizes. 47 names have more than one
shape. Apart from the four real splits above, they fall into three groups:

- **bare-name collisions** between unrelated classes: `BandCamShot::Target` vs
  `HamCamShot::Target`, `TargetCache`, `CharData`, `TrackData`, `Weight`,
  `Node`, `Entry`, `String` (`Quazal::String`), `Stats`, `PracticeSection`;
- **parse noise**, where compiler warnings interleave the report (`redefinition`
  or `pragma` read as a member name: `ProfileMgr`, `RockCentral`, `TourChar`,
  `Automator`, `ModifierMgr`, `RndMultiMesh`, `RndTransProxy`);
- **vbase printed-size** pairs with one identical layout (`BandMatchmaker`,
  `UIManager`, `WiiFriendMgr`).

Per-TU `/D` flags that change layout were checked separately. `build.ninja`
now carries none except `RB3_NO_WII_META_MEMBERS`; the old `RB3_MAP_0x1C`
map-size switch no longer appears in any compile line.

**Classes audited (main class of each TU; nested classes were covered by the
same reports and by their 100% vector/copy-ctor/dtor rows):**

| TU / class | sizeof | ctor | vtable order (C) | sub-100 named rows (D) | finding |
|---|---|---|---|---|---|
| BandCamShot | ✓ 536 | 100 | SAME ×2 (1 slot withheld: slot 10, a non-virtual fold name) | 6, no offset/type delta | clean |
| BandCharacter | ✓ 2088 | 100 | SAME ×7, UNRESOLVED ×3 | 3, none | clean; its TU carried the 5 stub classes via Band.cpp (fixed) |
| BandCharDesc | ✓ 608 | 100 | UNRESOLVED ×2 | 2, none | clean |
| BandConfiguration | ✓ 872 | 100 | SAME ×1 | 0 | clean |
| BandCrowdMeter | ✓ 756 | 100 | SAME ×1, UNRESOLVED ×6 | 1, none | clean |
| BandDirector | ✓ 364 | 98.27 (constant scheduling, §4) | SAME ×3, UNRESOLVED ×1 | 5, none | clean |
| BandFaceDeform | ✓ 52 | 100 | SAME ×1 | 1, none | clean |
| BandHeadShaper | n/a (stack local) | 100 | no vtable | 2, none | clean |
| BandIKEffector | ✓ 160 | 100 | SAME ×2, UNRESOLVED ×2 | 5, none | clean |
| BandList | ✓ 908 | 100 | SAME ×4, UNRESOLVED ×5 | 1, none | clean |
| BandPatchMesh (+MeshPair/PatchPair/WorkVerts/MeshVert) | n/a (embedded) | 100 | no vtable | 12, none | clean; MeshVert modelling noted in §6 |
| BandRetargetVignette | ✓ 80 | 100 | SAME ×2 | 2, none | clean |
| BandWardrobe | ✓ 0xe0 (factory) | 100 | SAME ×1 | 4, none | clean |
| CharKeyHandMidi | ✓ 204 | 100 | SAME ×2, UNRESOLVED ×2 | 3, none | clean |
| OutfitConfig (+MatSwap/Piercing/Piece/Overlay/MeshAO) | ✓ 252 | 100 | SAME ×2, UNRESOLVED ×1 | 2, none | clean |
| SongSectionController | ✓ 156 | 100 | SAME ×2 | 3, none | clean |
| MetaPerformer (band3/meta_band) | ✓ 940 in its own TU only | 100 | — | — | **ODR split, fixed (§3.1)** |
| DialogDisplay, InstrumentDifficultyDisplay, MicInputArrow, PlayerDiffIcon, ScrollbarDisplay | ✓ (real headers) | — | — | — | **1-byte stub redefinitions, fixed (§3.2)** |

"none" in column D means the order-free multiset found no member at a
different offset and no access at a different width or type. UNRESOLVED in
column C is the sweep's own label for a retail vtable whose slots the map does
not name well enough to compare. It is absence of evidence, not agreement.

band3 native TUs audited through instruments A, D, E, F and G (every class
complete in each): `bandtrack/{GemManager,GemRepTemplate,Tail,TrackConfig,TrackPanel}`,
`game/{Band,ChordbookPanel,CommonPhraseCapturer,CrowdRating,Defines,DirectInstrument,Game,GemPlayer,MultiplayerAnalyzer,NetGameMsgs,Performer,PlayerBehavior,Player,Scoring,Singer,SongDB,Stats,TambourineDetector,TambourineManager,VocalPart,VocalPlayer,VocalScoreHistory}`,
`meta_band/{AccomplishmentManager,AccomplishmentPlayerConditional,AccomplishmentProgress,AccomplishmentSongListConditional,AccomplishmentTrainerConditional,AppLabel,BandSongMetadata,BandSongMgr,CriticalUserListener,GameplayOptions,LicenseMgr,MainHubPanel,SessionUsersProviders,SongSortByPlays,SongSortMgr,StandIn,Utl}`.
`meta_band/Utl.cpp` has no split asm, so instrument E did not cover it.

## 8. Measurement

`tools/ab_measure.py --worktree ~/tmp/wt-w16-pz --from-dirty` on the three
fixes, before committing them as `5eac96dc6`:

```
leg A: matched=53516 masked=25193 honest=28323 code%=57.941143  (recompiles: 0, settled)
leg B: matched=53516 masked=25193 honest=28323 code%=57.941143  (recompiles: 112, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
units at 100% [mpn]: 553 -> 553 ; [all-rows-fuzzy]: 492 -> 492
```

A per-row diff of the two archived reports gives **68,909 rows on both legs,
0 down, 0 up, 0 appearing or vanishing**. I predicted Δ0 before the run: no
other TU touches the dropped `MetaPerformer` members or its `sizeof`, and
`BandInit` calls each `Init()` by the same mangled name either way. That is
what was measured. The value of these fixes is correctness: the match build
now has one definition of each class, and it is retail's.

### 8.1 Whole branch, rebased onto main `c1f6bafef`

After the native fix in §9 and a rebase onto `c1f6bafef` (W16-PX merged,
clean retail TU5 image, sha-checked identical to main's), I measured the whole
branch as one patch: `git diff main..w16-pz`, with the worktree detached at
main, then `tools/ab_measure.py --patch`. I predicted Δ0 again because the only
new change is inside `#ifdef HX_NATIVE`.

```
leg A: matched=53528 masked=25193 honest=28335 code%=57.967995  (recompiles: 0, settled)
leg B: matched=53528 masked=25193 honest=28335 code%=57.967995  (recompiles: 113, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp ; units at 100% [mpn] 554 -> 554 ; [all-rows-fuzzy] 492 -> 492
control (none ruler): Δmatched_code=+0 B
```

Per-row diff of the two archived reports: **68,909 rows on both legs, 0 down,
0 up, 0 appearing or vanishing** (`fuzzy_match_percent` and
`match_percent_normalized` both compared). Run dir:
`~/tmp/wt-w16-pz/.ab_measure_runs/20261006-105619-w16pz-branch-1365849`.

## 9. Native verification

The first native gate run on `5eac96dc6` **failed**:

```
NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=16 skipped=0 partial=0 failed=2 rc=1
```

`rb3-milo` and `rb3-render` could not compile `BandCharacter.cpp`. Replacing the
`PlayerDiffIcon` stub in `Band.cpp` with the real header exposed that header's
retail-shaped allocator to clang: an `operator new(unsigned int)` (clang
requires `size_t`) and the X360-only two-argument `MemAlloc`. This is the same
class of break the gate exists to catch, since the X360 build cannot see it. The
other four headers that are now included (DialogDisplay,
InstrumentDifficultyDisplay, MicInputArrow, ScrollbarDisplay) use
`OBJ_MEM_OVERLOAD*`, which already has a native branch in `utl/MemMgr.h`, so
they compiled cleanly.

The fix (`PlayerDiffIcon.h`) leaves the X360 block unchanged and adds an
`#ifdef HX_NATIVE` branch that spells the class allocator as
`OBJ_MEM_OVERLOAD(0x22)`, giving the native `size_t` / five-argument form.

On the rebased tip `e7cb7ae39`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=73 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

0 SKIPs, 73/73 runtime gates. The scatter counts (16 unlinked, 17 multi-host)
come from main after W16-PX. Before the rebase, the same branch read 17 and 20
with 57 gates. That move belongs to W16-PX, not to this branch.

## 10. Reproducing

The scratch instruments are under `~/tmp/w16pz/` (`offset_multiset.py`,
`placeholder_vt.py`, `base_cmp.py`, `size_cmp.py`). The inputs are
`class_layout_report.py --all-classes --json` per TU and
`tools/retail_sizeof_witness.py --json`. They are lane scratch, not committed
tools. §2 states the method of each closely enough to rebuild it, and the
sabotage control for F is the line `+(4 if off else 0)` applied to our side.
