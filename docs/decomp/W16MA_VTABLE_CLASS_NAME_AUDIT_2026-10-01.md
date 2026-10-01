# W16-MA — map names vs the vtable the retail body installs (2026-10-01)

**Branch** `w16-ma`, off main `87266c1b6` (W16-LB landing). **Not merged to main**, per the brief.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).

W16-LB found that `0x82270298` was named `??1ObjRef@@QAA@XZ` while its retail body installs the vtable
whose RTTI is `.?AVObjRef@@` — which in our build is `ObjRefOwner` (our `ObjRef` has no vtable in the
match build). That one name was worth 743 rows / +29,704 B. This lane asks the same question of the whole
`scripts/target_symbol_map.json`: **does the class in every ctor / dtor / `??_G` / `??_E` name agree with
the class of the vtable the retail body installs?**

## 1. Instrument — `tools/vtable_class_name_audit.py`

Read-only; retail bytes only for the verdict.

- **Extent.** Retail `.pdata`; for a leaf with no `.pdata` entry, a straight-line scan to the first
  `blr`/`bctr`/`b`, labelled `LEAF`. ⚠ `0x82270298` itself is such a leaf: `retail_rtti.py installs`
  *refuses* it (UNBOUNDED), so a `.pdata`-only census is blind to the very defect it is looking for.
- **Witness 1 (store).** A this-tracking decoder follows r3 and its `mr` copies and records every
  `stw <vtable>, d(this)`. A **ctor's** own class is its **last** offset-0 store (inlined base ctors store
  first); a **dtor's** (`??1`, `??_G`, `??_E`) is its **first**. The class comes from the vtable's COL.
- **Witness 2 (ownership).** Which retail vtables hold the address as a slot — names the class of a
  virtual special member independently of its code.
- **Witness 3 (our build).** The `??_7` relocations of our compiled body of the same name. A class
  disagreement our own body reproduces is consistent with our source, not a wrong name.
- **Comparison.** Both sides demangled by `llvm-undname` (template back-references compare correctly),
  then normalised: a defaulted trailing `ObjectDir` template argument is dropped on both sides
  (`ObjPtr<T>` ours vs `ObjPtr<T,ObjectDir>` retail, DTOR-A's TRAP 5), and retail's class is translated
  into ours **directionally** (`ObjRef` → `ObjRefOwner`).
- **Refinement** against retail's own hierarchy (every COL's base list): `STORE_IS_BASE_OF_NAME`,
  `NAME_CLASS_NOT_IN_RETAIL`, `UNRELATED`, …

`--selftest`: `0x82270298` reads **DISAGREE** under its old `??1ObjRef` name, **AGREE** under
`??1ObjRefOwner`, DISAGREE under a sabotage name; with the directional translation switched off the old
name reads **AGREE** (the instrument is blind without it); and `0x8262c028` (`bl __savegprlr_28` at +4)
must still be seen storing StickerProvider's vtable.

### 1.1 Three failed predictions that shaped the instrument

1. **A symmetric `ObjRefOwner`↔`ObjRef` rename made the W16-LB defect read AGREE.** The map name is a
   pairing key against *our* build, so retail's spelling must be translated into ours, never the reverse.
2. **The CRT's `bad_typeid`/`bad_cast` ctors read DISAGREE (retail = `std::exception`).** Their final
   store is a vtable at `0x8213B4AC` with **no COL** (the CRT is `/GR-`); the ctor rule had skipped to the
   last *resolvable* store, the inlined base. Now: an unlabelled decisive store ⇒ `UNLABELLED_VTABLE`
   (undecidable), never a fall-back to a base.
3. **Swapping the `LayerProvider`/`StickerProvider` `??_G` pair dropped both rows 100 → 99.74.** The
   `??_G` at `0x8262c280` calls the `??1` *named* LayerProvider, which in fact installs StickerProvider's
   vtables — the `??1` pair was swapped too and the census had missed it, because every `bl` killed r3,
   including the prologue `bl __savegprlr_N`. Exempting the save/restore helpers (target body begins with
   an r1-based `std`/`ld`) moved **`??0` AGREE 522 → 1,101 and `??1` AGREE 418 → 540**: a third of the
   special members had been invisible. DISAGREE moved only 128 → 136 — the map is right on the
   overwhelming majority of the newly visible rows.

## 2. Census (main `87266c1b6` → branch tip, same instrument)

4,527 map rows are special members or store a vtable at `0(this)`:

| kind | AGREE | DISAGREE | NO_STORE | UNLABELLED_VTABLE | UNBOUNDED |
|---|---:|---:|---:|---:|---:|
| `??0` | 1,101 → 1,105 | 35 → 22 | 561 | 97 | 1 |
| `??1` | 540 → 555 | 53 → 43 | 674 | 26 | 0 |
| `??_G` | 655 → 682 | 36 → 13 | 277 | 5 | 0 |
| `??_E` | 423 → 429 | 12 → 2 | 18 | 0 | 0 |

(AGREE includes AGREE-by-ownership; NO_STORE = no offset-0 vtable store and no ownership evidence — e.g.
the 76-byte deleting-dtor thunk that calls `??1`, which DTOR-A covers.)

### 2.1 Disposition of all 136 base-map disagreements

| disposition | rows | evidence |
|---|---:|---|
| **FIXED** (renamed) | **57** | store + ownership + caller shape (§3) |
| consistent: dtor dead-store elision | 51 | store is a retail **base** of the named class, and our same-named body installs that base first too (e.g. `~RockCentralOpCompleteMsg` stores `Message`, fan-in 76) |
| consistent: non-polymorphic struct | 19 | the class has no retail RTTI; the offset-0 store is a smart-pointer **member** (`Key<ObjectStage>`, `CharEyes::EyeDesc`, `FaderGroup`, …); 3 read OURS_STORES_NONE only because our build calls the member ctor out of line |
| deferred: true name known, unpairable | 2 | §4 |
| no symbol in our build for the retail class | 7 | §4 |

Plus 2 renames the store witness cannot see (`MakeInstImpl`, ownership only): **59 renames**.

### 2.2 Fan-in: no second ObjRef

All 59 renamed rows together have **46** retail `bl` call sites (`tools/retail_callers.py`), against
**843** for W16-LB's single `0x82270298`. Largest: `0x823dd0e0` 10, `0x8227c880` 6, `0x82675a70` /
`0x826fbd08` / `0x827f79a8` 4. The census found no second ObjRef-scale defect; the class's value was
concentrated in the one row W16-LB already fixed.

## 3. Fixes

### 3.1 Leaf destructors named as constructors (11)

`lis/addi/stw rX,0(r3)/blr` installs one class's vtable. For a class with no members the trivial ctor and
dtor are the same four words, so the bytes fix the **class** but not ctor-vs-dtor. **Every retail caller
of all eleven is a 40/44-byte EH unwind funclet** (`addi r31,r12,…; lwz r3,…(r31); bl target`) — an
unwind path destroys, so all eleven are `??1`. Nine had been named as ctors, and every one for the wrong
class: `??0Symbol` → `~RndOverlay::Callback`, `~CompressTextureCallback` → `~MergeFilter`,
`~Interpolator` → `~BeatMasterSink`, `??0UIListProvider` → `~UIListStateCallback`, `??0SkeletonHistory`
→ `~Mic`, `??0BeatMatchSink` → `~HxAudio`, `??0Callback@Loader` → `~ScrollSelect`,
`??0Callback@ContentMgr` → `~RndOcclusionQueryMgr`, `??0SongInfo` → `~TempoMap`, `??0BaseSkeleton(copy)`
→ `~ByteGrinder`. Map corroboration: `0x8227c8a0`, next to `0x8227c880`, was already `??_GMergeFilter`.

### 3.2 Deleting destructors (23)

68-byte `??_G` bodies with an inlined trivial dtor, plus 76/88-byte ones: the store class and the
vtable-slot owner agree in every case. Where several classes own the slot (the `RndShader*` family,
`File`/`BufFile`, `TempoMap`/`SimpleTempoMap`) the body is an ICF fold and the stored class is the
survivor's. Chains resolved as sets: `??_GCallback@ContentMgr` moves `0x827394a8` → `0x8253a840`;
`??_Gbad_alloc` moves `0x825ad710` → `0x822744e0`; `??_GByteGrinder` / `??_GCallback@Loader` (§3.5).
`0x826fcf20` is named `??_GBufFile` (survivor of group #698), not `??_GFile` (its folded member).

### 3.3 Swapped pairs (12)

`BoolKeys`/`FloatKeys` (`??1`, `??_G`); `LayerProvider`/`StickerProvider` (`??1`, `??_G`, `??_E`
`W3` thunks); `ParallelGroupSeqInst`/`SerialGroupSeqInst` ctors **and their callers**
`?MakeInstImpl@{Parallel,Serial}GroupSeq` — retail puts `0x8270b620` in SerialGroupSeq's (and SfxSeq's)
slot 21 and `0x8270b710` in ParallelGroupSeq's. Four `??_E` this-adjustor thunks by slot ownership
(`Game`→`SetlistProvider`, `SongSortByStars`→`SongSortByRank`, `GroupSeq`→`RandomGroupSeq`,
`WaitSeq`→`GroupSeq`; `0x82709400` is in five `*GroupSeq`/`SfxSeq` vtables, a fold, named for the base).

### 3.4 Vtable-slot bodies under non-virtual names (4)

`0x8260d168` `~_Rb_tree<…>`, `0x82659a30` `~list<SortNode*>`, `0x827e5d78` `~list<MidiParser*>`,
`0x8279b2e0` `_STLP_alloc_proxy` ctor each sit in a retail vtable slot — impossible for a non-virtual
special member. Named from **our** vtable at the aligned slot (`scripts/dump_vtable.py` on our obj;
ours = retail + 1 because our dump puts the COL at `[0]`, confirmed on the named neighbours):
`?FinishLoad@CharacterCreatorPanel` (UIPanel sub-vtable [14]; reached 100 — our body is also a single
`b` and the target **name** agrees), `?OnActivate@NonDestructiveTransitionEvent` [7], `?Clear@CharWidgetImp`
[6], `?SetCymbalConfiguration@JoypadController` [36] (`stw r4,0x54(r3)`).

### 3.5 Re-homes (12 rows, 9 `.text` moves)

Eleven rows had a known true name but sat at 100 in a unit whose base obj does not define it. Each sits
**at the edge of its pinned unit, directly beside the unit of the class retail RTTI names, and that unit's
base obj defines the name** — the off-by-one-function pin W16-LB's `MemTrackReportDF` re-home also was.
`.text` only; dtk re-derived the `.pdata` (the split guard surfaced it as a one-time rewrite).

| range | rows | from | to |
|---|---|---|---|
| `0x827D24A8–0x827D2500` | `~TempoMap`, `??_GTempoMap` | SongInfoCopy | TempoMap |
| `0x8265EEA8–0x8265EEEC` | `??_GCampaignKey` | SongSortByRank | CampaignKey |
| `0x825EA8E8–0x825EA92C` | `??_GAccomplishmentGroup` | FixedSizeSaveable | AccomplishmentGroup |
| `0x827B27B8–0x827B27FC` | `??_GStorePurchaser` | MeshAnim | StorePurchaser |
| `0x82641610–0x82641664` | `??_GTokenRedemptionPanel` | FlowSlider | TokenRedemptionPanel |
| `0x827F20EC–0x827F213C` | `??_GUIScreen` | UILabel | UIScreen |
| `0x827E5D74–0x827E5D88` | `?Clear@CharWidgetImp` (+ pad, + `_S_sort`'s EH prefix) | MidiParser | TrackWidgetImp |
| `0x826FBD08–0x826FBD60` | `~ByteGrinder`, `??_GByteGrinder` | DepthBuffer3D | Synth |
| `0x8228D310–0x8228D354` | `??_GCallback@Loader` | HamDirector | BandDirector |

No source unit was drained (each keeps 2–41 other `.text` blocks).

### 3.6 Alias groups

Three placed groups had a survivor the retail bytes refute — the body installs the *folded* spelling's
class, so the old survivor cannot be this body and cannot ICF-fold with it. In each the folded spelling
becomes the survivor, `folded` becomes empty, and the old survivor is kept as a `withdrawn` record
(`class: CONTRADICTED_SURVIVOR_VTABLE_CLASS`), so `tools/alias_withdrawals.py` denies its regrowth:

| group | old survivor (withdrawn) | new survivor | retail store |
|---|---|---|---|
| `0x82709f30` | `??0ParallelGroupSeq` | `??0SfxSeq` | `0x820F80E4`/`0x820F80D4`, both `.?AVSfxSeq@@`; sole caller `?NewObject@SfxSeq` |
| `0x82739568` | `??1CSequentialReadMem@NUISPEECH` (DC3 Kinect, no retail RTTI) | `??1DxRndOcclusionQueryMgr` | `.?AVDxRndOcclusionQueryMgr@@` |
| `0x825c21d0` | `??0PreviewDownloadCompleteMsg` | `??0RemoteMachineUpdatedMsg(RemoteBandMachine*,uchar)` | `.?AVRemoteMachineUpdatedMsg@@` |

**Not done:** the null-address groups #698 (`??_GBufFile`), #699 (`??_GCallback@RndOverlay`) and #703
(`??_GRndShader`) now have their survivor named in the map, but their `address` was left `null`. Setting
it would admit their folded members (e.g. `??_GNullFile`) into a live bucket without a per-pair proof — the
alias-fabrication hazard. Retail ownership does corroborate #703: `0x824a5ab0` is slot 0 of `RndShader` and
11 of its 12 folded subclasses.

The map's `_bijection_arbitrary` loses the 15 addresses this lane resolved (994 → 979) and
`_icf_arbitrary` loses `0x82709f30` (39 → 38): their identity is now established by the installed vtable, not by a byte-class bijection.
The map carries `_w16ma_vtable_class_comment` as the in-file record.

## 4. Left open

| row | current name | true name / class | why not renamed |
|---|---|---|---|
| `0x826bbc28` (fan-in 4) | `??0BeatMatchControllerSink` | `??1BeatMatchSink@@UAA@XZ` | at 100 in FreestylePanel; only GemPlayer's base obj defines it, not adjacent |
| `0x82535708` (1) | `??1AsyncFileHolmes` | `??1AsyncFileWin@@UAA@XZ` | at 100; survivor of alias #750; retail has **no** `AsyncFileHolmes` RTTI, so the whole AsyncFileHolmes pin needs its own look |
| `0x823f6198` (9) / `0x823f61f8` (2) | `XboxSessionJob` ctor / dtor | class `XboxJob` | our build has no `XboxJob`; retail's class name differs from ours (source lead) |
| `0x825d4498` (1) | `??0MusicLibraryUpsellViewSetting` | class `SongUpsellViewSetting` | no such class in our build |
| `0x823f5270` | `??_GAutomator` | `MessageBroker` | not emitted by our build; at 100 |
| `0x82604d70` | `??_EDancerSequence@@$4…` | `BandPreloadPanel` vtordisp thunk | not emitted by our build |
| `0x82b8c8c8` | `??_E StandardEffect<CompressionEffect>` | `UGCNet_Server` (Quazal) | not emitted; `auto_*` unit |
| `0x822cd918` | `??1ObjRefConcrete<BandStarDisplay>` | `ObjPtr<BandStarDisplay>` dtor | our build emits neither spelling |

Two rows fell to 0 from below 100 because the true name's base obj is not the pinned unit's:
`0x823dd0e0` (85.0, Waypoint) and `0x825ad710` (99.77, CameraManager); likewise `0x82659a30` (95) and
`0x8279b2e0` (97). None was at 100; each was a false pairing. Re-home leads, not settled.

## 5. Results

### 5.1 Step ledger (full builds, whole-report row diff keyed by retail address, `~/tmp/w16jc/rowdiff.py`)

| step | Δfns | ΔB | rows up (to 100) / down |
|---|---:|---:|---|
| §3.1–3.3, 3.6 (45 renames + 3 alias groups) | +7 | +1,660 | 39 (38) / 2 from <100 |
| §3.4 (3 vtable-slot renames) | +1 | +4 | 1 (1) / 2 from <100 |
| §3.5 (re-homes + 11 renames) | +1 | +144 | 4 (4) / 0 |
| **sum, base `87266c1b6`** | **+9** | **+1,808** | 44 (43) / 4, **0 off 100** |

The first wave's first build read +5 / +1,508 with **two rows off 100** (§1.1 item 3); fixing the `??1`
swap restored both before anything was committed.

### 5.2 Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ma-ab --patch <git diff main..w16-ma -- splits.txt
target_symbol_map.json symbol_aliases.json>` (the audit tool itself is not a build input and was left out).
- **Worktree:** fresh `setup_worktree.sh` at main `87266c1b6`. **Patch kinds:** map + splits; no `symbols.txt`.
- **Both legs** read at a split fixed point (0 extra re-splits each).
- **Run dir:** `~/tmp/wt-w16-ma-ab/.ab_measure_runs/20261001-173029-branch-3923556/`.

```
leg A: matched=50055 masked=24403 honest=25652 code%=52.278416  (recompiles: 0, settled)
leg B: matched=50064 masked=24403 honest=25661 code%=52.296060  (recompiles: 0, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+9  Δmasked_equal=+0  Δhonest=+9  Δcode%=+0.017644pp  Δcode_bytes=+1808
Δfuzzy=+0.002659pp   (legA 59.280975 -> legB 59.283634)
units at 100% [mpn ruler]: legA 401 -> legB 401  (Δ+0; 0 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 346 -> legB 346  (Δ+0; 0 reached 100, 0 fell off)
[control none] Δmatched_code=+296 B -- NOT_APPLICABLE (kinds=map,splits)
```

**Prediction, written before the run** (from in-tree full builds of main and the tip): +9 fns / +1,808 B,
44 rows up (43 to 100), 4 down, all from below 100. **Measured: exactly that, on every key.**

**Row level** (archived leg reports, keyed by retail address): **44 up (43 to 100), 4 down — `0x823dd0e0`
85.0, `0x825ad710` 99.77, `0x8279b2e0` 97.0, `0x82659a30` 95.0, each to 0 — and 0 rows off 100 on the
fuzzy ruler and 0 on the `mpn` ruler** (a row missing from leg B would count as off, so the check can fail).
The tool's nine "unit regressions" are exactly the nine re-home source units, whose rows moved out.

The `none` control moved (+296 B), as it must for a splits patch. The alias edits cannot fabricate
forgiveness in any case: all three only emptied a `folded` list; no membership was added.

## 6. Gates

On the final code tip `32875a263`, after a full build equal to A/B leg B (50,064 / 5,358,888 B), every
exit code read directly:

```
[map-injectivity] OK: 33157 applied rows, 33156 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1666 map-consistent, 279 tolerated (enumerated above), 0 contradicted, 1946 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
tools/vtable_class_name_audit.py --selftest: SELFTEST PASS
```

W16-LB's tip read 1,663 map-consistent / 282 tolerated: the three groups whose survivor now names its
address moved from tolerated to map-consistent. The first VALIDATE on this branch REFUSED (exit 2) on a
tree whose map had changed after its build — correct behaviour; re-run after the build. The native gate ran
last on the final code; only this docs-only commit follows.

## 7. Not done

- Not merged to main, per the brief.
- No source edited; no `symbols.txt` edit.
- The null-address alias groups were not activated (§3.6).
- The broader "virtual method named for a class whose vtable does not own it" audit (the shape that found
  the `MakeInstImpl` swap) was not run beyond the two rows the ctor swap pointed at — it is a larger,
  fold-heavy population and a natural next lane.
