# W16-QJ — W16-QF's leftovers: six wrong-instantiation rows, `Movie::Impl::PlatformCacheFile`, CamShot's 0x40 member (2026-10-06)

Branch `w16-qj` (worktree `~/tmp/wt-w16qj`). Started off main `9fd8de7b1` and rebased onto main `a15e472fa`.
This lane resolves the three items `W16QF_VIA_DC3_WORLD_SYNTH_UI_BEHAVIOUR_2026-10-06.md` recorded but did not fix ("Findings recorded, not fixed").
Ruler: `name_check`, read from `report.json`.

## Result

| item | outcome | evidence |
|---|---|---|
| Six rows named as a different instantiation, or an unrelated function | **Fixed.** Renamed to the function retail's body actually is. Five `.text` pins re-homed to the unit that holds and calls the body. Five alias groups relabelled. | Retail body matched word for word against our compiled body. Callee relocations agree with retail's callees. Retail caller located. |
| `Movie::Impl::PlatformCacheFile` declared but never defined | **Fixed.** Defined (`return true`) in a new compile-only TU, `system/movie/Movie_Xbox.cpp`. | Retail body `li r3,1; blr`. Retail `Begin`'s scheduling shows the callee was defined outside `Begin`'s TU. |
| CamShot's DC3-only `Symbol mCrowdStateOverride` at 0x40 | **Fixed.** The slot stays, as `int unk40` (never constructed). | Retail's ctor skips 0x40. No retail CamShot/BandCamShot code touches it. `class_layout_report`: sizeof 0x1c8 unchanged. |

**A/B, whole branch:** `tools/ab_measure.py --patch`, ruler `name_check`, in a fresh worktree off main `a15e472fa`, both legs at a split fixed point.
**Δmatched +7, Δcode_bytes +1,472, Δfuzzy +0.001436 pp. No row went down.**
Predicted before the run: +7 / +1,432 B. The +40 B over the prediction is the moved catch funclet `0x82371388`, which reaches 100 in Character (details below).

**Native:** `tools/native_build_gate.sh` **PASS 18/18, 0 skipped** on the final tree (result line below). Its class-layout ODR check (W16-QD) passes, and it covers both header changes here.

## 1. The six rows (plus the seventh their chain needed)

W16-QF read these rows as "wrong map name": retail's body has a different element stride or allocation size, so it cannot fold with ours.
Each of them was also **pinned into the wrong unit**. The pin had followed the wrong name, so renaming alone would leave the row unpairable.

### Method

1. **Body identity.** `~/tmp/w16qj/bodymatch.py`, a scratch tool. It takes retail's body from `band.exe` (extent from big-endian `.pdata`) and compares it word for word with every function body in every compiled obj (`tools/coff_bodies_ext.py`). Words our obj relocates, and retail `bl` words, are masked.
   The masking makes many same-shape instantiations tie. The tie is broken by reading the candidate's relocation targets against the map name of retail's actual callee.
2. **Caller.** The retail functions that `bl` to the address, scanned across every `.s` by `.fn` symbol, never by the synthetic address column.
3. **Home.** The split unit that surrounds the address and whose obj defines the correct name (`tools/objsym_find.py`). The map row is written exactly as that obj spells the symbol.
4. **Spelling.** `__destroy_range<T>` and `_Destroy_Range<T>` compile to identical bodies. The spelling chosen is the one our own callers reference, so no caller picks up a name charge.

### Rows

| addr | old name (wrong) | retail body is | why | retail caller | pin |
|---|---|---|---|---|---|
| `0x822A6528` | `__destroy_range<Flow::DynamicPropertyEntry>` (stride 0x88) | `_Destroy_Range<OutfitConfig::MeshAO>` | stride 0x30; calls `0x822A4060` = `~MeshAO` | OutfitConfig | Flow → **OutfitConfig** |
| `0x827A37B0` | `__destroy_mv_srcs<vector<Vector3>>` (stride 0xc) | `__destroy_range<TrackChannels>` | stride 0x10; calls `??_G` at `0x827A3598`, already alias-grouped with `??_GTrackChannels` | DataArraySongInfo, SongInfoCopy | UIList → **DataArraySongInfo** |
| `0x8251EAC0` | `list<DataNode>::_M_create_node` (0x10 node) | `list<ContentMgr::CallbackFile>::_M_create_node` | `li r3,0x28`; calls `0x8251E928` = `_Copy_Construct<CallbackFile>` | `0x8251F060` | FlowCommand (unchanged) |
| `0x8251F060` | `list<DataNode>::insert` | `list<ContentMgr::CallbackFile>::insert` | calls `0x8251EAC0`; only caller is `ContentMgr::AddCallbackFile` | ContentMgr | FlowCommand (unchanged) |
| `0x82729008` | `vector<BattleStep>::_M_fill_insert` (stride 32) | `vector<SampleMarker>::_M_fill_insert` | stride 16; calls `0x82728E38` = `_M_fill_insert_aux<SampleMarker>` | SampleData | ByteGrinder → **SynthSample** (sits between SynthSample blocks) |
| `0x82371320` | `__uninitialized_copy<CamShotFrame>` (stride 0x108) | `__uninitialized_fill_n<Character::Lod>` | stride 0x1c, count loop; calls `0x8236F588` (`Lod` copy ctor) | Character | CameraShot → **Character**, with its catch funclet `0x82371388` |
| `0x82733C70` | `SampleMarker::SampleMarker()` | `DxRnd::ResetDevice()` | `PreDeviceReset` / `D3DDevice_Reset(0x1c4, this+0x1d4)` / `PostDeviceReset` | Rnd_Xbox (`SetShrinkToSafeArea`, `SetAspect`) | SampleData → **rnddx9/Rnd** (contiguous with Rnd's block) |

`0x8251F060` was not in W16-QF's list. It joined because of what held the wrong names in place: callers whose own retail names were equally wrong, so the two errors cancelled.
- `list<DataNode>::insert` read 100% **because** the map also called its callee `list<DataNode>::_M_create_node`.
- Renaming `0x8251EAC0` alone would have charged that row. So both are named together.
- `0x82371388` is `__uninitialized_fill_n<Lod>`'s `catch(...)` funclet: `_Destroy_Range` of the built prefix, then `_CxxThrowException(0,0)`. It moves with its parent.

All seven names were unused in the map. Rows were changed by exact textual substitution. `tools/gated_map_write.py --audit-objects`: 0 object-side collisions.

### The alias groups that were forgiving the wrong names

Five of these addresses carried an alias group whose **survivor** was the wrong name and whose folded member was the right one:
- `0x822a6528`, `0x82371320`, `0x82729008`: CHASED T1, from lanes W16-HC and W16-NH;
- `0x827a37b0`: W16-GQ;
- `0x8251f060`: W16-GV.

The bytes did match the folded spelling. The survivor spelling was never possible: a 0x88-stride body cannot fold with a 0x30-stride one.
The build's `CHECK ALIAS SURVIVORS VS MAP` edge flagged all five once the map changed. `tools/alias_survivor_relabel.py --write` relabelled each survivor to the map name and re-chased every membership on retail bytes:

- **4 old labels REFUTED and withdrawn:** `BODY:BYTES-DIFFER`, anchored by the new survivor being PROVEN at the same address. Those are DynamicPropertyEntry, `vector<Vector3>` mv_srcs, CamShotFrame and BattleStep.
- **1 kept:** `__destroy_range<MeshAO>` stays folded at `0x822a6528` (PROVEN).
- **1 withdrawn by hand:** `list<DataNode>::insert` at `0x8251f060`.
  - The tool read it UNDECIDABLE (`CALLEE-UNANCHORED:BYTES-DIFFER`). Our depth-1 callee `list<DataNode>::_M_create_node` no longer has a map address of its own, so the tool's anchor rule cannot fire. It would have carried the member as folded.
  - The difference is positive. Retail's callee allocates 0x28-byte nodes. Our `FlowCommand.obj` `list<DataNode>::_M_create_node` allocates `li r3,0x10`, and our `list<CallbackFile>` version allocates `li r3,0x28`. Different-size COMDATs cannot fold.
  - It is withdrawn with a `FORMER_SURVIVOR_LABEL_REFUTED_AT_DEPTH_1` record. Nothing was pruned.

### Prediction, then measurement (map + splits + aliases only, in the lane worktree)

Predicted, before the first build:

| row | predicted | measured |
|---|---|---|
| `_Destroy_Range<MeshAO>` (80 B) | 100 | 100.0 |
| `__destroy_range<TrackChannels>` (84 B) | 100 (callee forgiven by group `0x827a3598`) | 100.0 |
| `list<CallbackFile>::_M_create_node` (72 B) | 100 | 100.0 |
| `list<CallbackFile>::insert` (100 B) | stays 100 | 100.0 |
| `DxRnd::ResetDevice` (64 B) | 100 | 100.0 |
| `DxRnd::SetShrinkToSafeArea` / `SetAspect` (84 + 80 B) | 99.76 / 99.75 → 100 (their callee stops being charged) | 100.0 / 100.0 |
| `vector<SampleMarker>::_M_fill_insert` (108 B) | < 100, one name charge (retail's overflow callee is named `TypeCreatorPair`) | 99.81 |
| `__uninitialized_fill_n<Lod>` (96 B) | < 100, one name charge (`_Copy_Construct` vs our `_Param_Construct`) | 99.79 |

Every row landed as predicted. The two rows that stay under 100 each carry one callee-name charge. Each retail callee there is a fold survivor under another type's name. No alias was added: a fold has to be proven, not assumed.
The rows that left the report (old names: Flow 99.70, UIList 99.71, ByteGrinder 99.44, CameraShot 85.92, SampleData 67.19, FlowCommand 99.67) were all below 100, so no bytes and no function counts were lost.

## 2. `Movie::Impl::PlatformCacheFile`

Retail's callee at `0x82533618` is `li r3,1; blr` (ICF-folded with `ObjDirPtr<ObjectDir>::IsDirPtr`): the Xbox player never needs a file cached before opening it.
Our tree only declared the method.

**It cannot live in Movie.cpp.** Measured with `tools/gate_liveness.py` (OBJCACHE=off, same `/Fo` on both legs) on a definition gated behind a temporary `/D`:

| in-TU definition | `?Begin@Impl@Movie@@` words changed |
|---|---|
| plain | **189** (inlined) |
| under `#pragma auto_inline(off)` | **3**: the `mPreloaded` store `stb r29,0x18(r30)` sinks past the two argument `mr`s before the call |

Retail's `Begin` (`0x82745d90`, offset `0x6c`) has `stb` **first**, then `mr r4,r28; mr r3,r30`. That is the order our current build produces when the callee is an undefined external.
So the callee's body was not visible to `Begin`'s TU in retail. This is the same escape/memory-effect mechanism documented at `src/band3/game/VocalPlayer.cpp:1739`: `auto_inline(off)` stops inlining, not MSVC's intra-TU reasoning about a callee.

**Fix.** The Bink SDK structs, `MovieInternalBuffers` and the `Movie::Impl` class declaration moved verbatim into `src/system/movie/Movie_Xbox.h`. `src/system/movie/Movie_Xbox.cpp` (new, compile-only, `NonMatching`, no split range) defines the method.
Neutrality was checked twice, before and after the rebase onto W16-QD's `BinkSdk.h`. Both times our `Movie.obj` (OBJCACHE=off) and main's `Movie.obj` had **338 of 338 shared function bodies identical in bytes and relocation names**.
The only difference is one EH funclet's compiler-counter name (`__catch$70971` → `$70972`), whose 40 B body is identical. Funclets pair by byte signature.
Native does not compile this tree's `movie/` sources; `Movie_Xbox.cpp` is also `#ifndef HX_NATIVE`.

## 3. CamShot's member at 0x40

`class_layout_report.py CamShot` (compiler) before the change: `0x3c Symbol mCategory`, `0x40 Symbol mCrowdStateOverride`, `0x44 mAnims`, sizeof 0x1c8.

Retail evidence:
- **The ctor** (`0x824c6298`) never constructs 0x40. The graded diff shows our `lwz gNullStr; stw r11,0x40(r30)` as an insert, and retail's next construction is `addi r11,r30,0x44` (`mAnims`).
- **No access anywhere.** Every retail function the map names as a CamShot member, in `CameraShot.s` and `BandCamShot.s`, was scanned for displacement `0x40` off a non-frame register, and for `-0x220` off the `this+0x260` base CamShot::Save uses. There are no hits on the member. The two `0x40` displacements in `CameraShot.s` are a vtable slot (`LoadSubPart`: `lwz r11,0x4(r11); lwz r11,0x40(r11); mtctr; bctrl`) and a `CamShotFrame` member.
- **No string.** `"crowd_state_override"` is absent from `band.exe`, as W16-QF and the in-tree propsync note already found.
- **The inherited RB3 game source agrees.** It declares an unnamed `int` in exactly this position, right after `mCategory`, and never uses it.

The slot is real: `mAnims` and every later member are at their retail offsets, and the 100%-matching functions depend on that. So the member stays 4 bytes and becomes `int unk40;`, which has no constructor.

Also removed:
- the DC3-only `crowd_state_override` propsync arm. It sat under `RB3_KEEP_DC3_ONLY_HANDLERS`, which no build defines (not among the eleven `/D` flags in `build.ninja`, and absent from native);
- its comment's stale claim that the member "is still saved/copied/loaded" (nothing references it).

The header's old layout note quoted offsets from an earlier layout (mCrowds at 0x19c; the compiler now says 0xdc). It was replaced with the evidence above.

Measured in the lane worktree: full build, `report.json` rows compared before and after. **`CamShot::CamShot` 87.43 → 100.00 (+1 fn / +968 B). No other row changed.**
`class_layout_report` after: `0x40 unk40`, `0x44 mAnims`, sizeof 0x1c8.

## Measurement

**A/B, final branch.** `tools/ab_measure.py --worktree ~/tmp/wt-w16qj-ab --patch <git diff main..w16-qj>`. Log: `~/tmp/w16qj/ab.log`. Run dir: `~/tmp/wt-w16qj-ab/.ab_measure_runs/20261006-130548-w16qj-3310393`.
```
leg A: matched=53654 masked=25185 honest=28469 code%=58.120502  (recompiles: 0, settled)
leg B: matched=53661 masked=25185 honest=28476 code%=58.134865  (recompiles: 131, split=1, patch_steps=7, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+7  Δmasked_equal=+0  Δhonest=+7  Δcode%=+0.014363pp  Δcode_bytes=+1472
units at 100% [mpn ruler]: legA 578 -> legB 579  (FlowCommand reached 100, 0 fell off)
[control none] Δmatched_code=+1472 B
```

**Row diff, leg A vs leg B** (from the two archived reports; the byte column sums to exactly +1,472):

| Δ B | unit | row | leg A → leg B |
|---:|---|---|---|
| +968 | CameraShot | `CamShot::CamShot` | 87.43 → 100 |
| +84 | DataArraySongInfo | `__destroy_range<TrackChannels>` | (UIList's 99.71 row under the old name) → 100 |
| +80 | OutfitConfig | `_Destroy_Range<MeshAO>` | (Flow's 99.70 row) → 100 |
| +72 | FlowCommand | `list<CallbackFile>::_M_create_node` | (`list<DataNode>` 99.67) → 100 |
| +64 | rnddx9/Rnd | `DxRnd::ResetDevice` | (`SampleMarker` ctor 67.19) → 100 |
| +84 | Rnd_Xbox | `DxRnd::SetShrinkToSafeArea` | 99.76 → 100 |
| +80 | Rnd_Xbox | `DxRnd::SetAspect` | 99.75 → 100 |
| +40 | Character | `fn_82371388` (catch funclet) | CameraShot 99.5 → Character 100 |
| 0 | FlowCommand | `list<…>::insert`, same address | 100 (DataNode) → 100 (CallbackFile) |
| 0 | SynthSample | `vector<SampleMarker>::_M_fill_insert` | (ByteGrinder 99.44) → 99.81 |
| 0 | Character | `__uninitialized_fill_n<Lod>` | (CameraShot 85.92) → 99.79 |

Function counts: CameraShot nets 0 (ctor +1, funclet moved out −1), Character +1, plus the other five units. Total +7, equal to the whole-binary Δ.
The funclet was the one uncertain item before the run. In CameraShot it paired with a nearest-neighbour funclet at 99.5. In Character it pairs with its own twin.

**Native.** Final runs in `~/tmp/wt-w16qj` after the last source commit:
```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
LAYOUT_ODR_RESULT verdict=PASS x360_tus=1266 x360_failed=0 x360_split=0 x360_unresolved=0 x360_allowed=138 x360_stale=0 native_tus=1711 native_failed=0 native_split=0 native_unresolved=0 native_allowed=1 native_stale=1 rc=0
```
The first gate run on the same tree read `FAIL 16/18`, but with build rc=0, 0 errors and 0 linker diagnostics. The two "defects" were `STALE` on `rb3-frame` and `rb3-render`: ninja did not consider them up to date after the build, and the gate itself labels this "NOT attributable to this run". The A/B was building in another worktree at the time. A re-run with no source change read PASS 18/18. Both result lines are in `~/tmp/w16qj/native_gate.log` and `native_gate2.log`.
The `native_stale=1` allowlist warning (GpuMeshData) predates this lane.

**In-tree checks along the way** (lane worktree, full builds, `report.json` rows compared):
- map + splits + aliases: the rows table in section 1;
- `Movie_Xbox` extraction: whole-build measures identical (53,614 / 5,953,776 B at the pre-rebase base);
- CamShot: +1 / +968 B, only the ctor row moved.

## Deliberately not done

- **No alias for the two one-charge rows.** `_M_insert_overflow_aux<SampleMarker>` ↔ retail `0x823F0CD0`, and `_Param_Construct<Lod>` ↔ `0x8236F588`. Each needs its own retail-byte proof (`tools/icf_pair_adjudicate.py --chase`). Installing one without that proof is exactly the forgiveness hazard this lane removed from five groups.
- **Our original six spellings were not looked for in retail.** These are `__destroy_range<DynamicPropertyEntry>`, `__destroy_mv_srcs<vector<Vector3>>`, `list<DataNode>` create_node/insert, `vector<BattleStep>::_M_fill_insert`, `__uninitialized_copy<CamShotFrame>` and `SampleMarker::SampleMarker`. Their rows are unpaired now, which is the truthful state. Finding where (or whether) retail emits them is identification work.
- **`??_G pair<const Symbol, vector<int>>` at `0x827A3598`** (pinned in SongMgr) keeps its existing group, whose folded member is `??_GTrackChannels`. Not re-adjudicated here.
- The `CHECK ALIAS SURVIVORS VS MAP` edge was not bypassed. The relabel ran because of it.
- **Not pushed, not merged.**
