# W16-PI: the layout and map defects W16-PC left, settled on retail bytes (2026-10-06)

Lane W16-PI, branch `w16-pi`, rebased on main `5bb85d98f`.
Brief: fix the five defects `W16PC_SOURCE_DIVERGENCE_ROWS_2026-10-03.md` found and left (§3 rows and §4 "left"):
1. `Hmx::Object::mRefs`'s container type
2. the MemMgr `.bss` layout
3. the two RemoteBandUser map/vtable defects
4. the MeasureMap mis-pin
5. the WaveFile map name that was blocked on the anonymous-namespace patcher

Lane rules: settle each defect on retail evidence, stay out of `src/network` and `src/xdk`, and let no row go down.

Working files (not committed): `~/tmp/w16pi/`.
- `rowcmp.py`: compares two `report.json` files row by row.
- `coffsyms.py`: dumps a COFF symbol table.
- `cc_memmgr.sh`: a scratch `OBJCACHE=off` compile.
- `install.py`: the W16-PH alias installer, adapted.
- `pairs*.json`, `items*.json`: the fold pairs and their evidence.

## 1. Result

All five are fixed. Nothing was stopped and left for a later lane. The rows that stay below 100 are listed with their
cause in each section.

| # | defect | change | fns | bytes |
|---|---|---|---:|---:|
| 1 | `mRefs` is `std::list<ObjRefOwner*>` | source + 3 fold memberships | +6 | +776 |
| 2 | MemMgr `.bss` | source (declaration order, 3 stand-ins, `gMemStackLock` defined) | +5 | +1,508 |
| 3 | RemoteBandUser `Handle` / `0x8268ED70` / `0x8268EF78` | source (Wii handler removed), 3 map names, 3 fold memberships | 0 | +4 |
| 4 | MeasureMap pin `0x82449998` | `.text` pin moved to Part.cpp, map name | +2 | +240 |
| 5 | WaveFile `0x827D33F8` | 5 map names (one anon-ns hash), 5 alias survivors relabelled | +2 | +192 |
| | **lane worktree, summed row-compare** | | **+15** | **+2,720** |

**Lane worktree, before rebasing.** The full-build row-compare against the base report (`~/tmp/w16pi_base_report.json`)
moved 53,469 → 53,484 matched functions and 5,910,896 → 5,913,616 matched bytes, with **16 rows up and 0 down**. That
count of 16 excludes renamed rows, which appear only as "ONLY A" or "ONLY B". Each renamed row is listed in its section.

Whole-binary A/B on the rebased branch: §7.

## 2. `Hmx::Object::mRefs` is an STLport `std::list<ObjRefOwner*>` (commit `e57fd4a3f`)

### Retail evidence

Retail's `Object` reaches `+0x20` in four ways, and the hand-rolled `ObjRefNode` ring reproduced none of them:
- `AddRef` calls `list::insert(begin(), ref)`, the out-of-line `push_front`.
- `Release` walks the list, then calls `list::erase(it)`.
- The constructor runs the STLport list constructor at `+0x20`.
- The implicit `Object::operator=` calls `list::operator=` at `+0x20`.

### Source change

`ObjRefList` is a typedef:
- `std::list<ObjRefOwner *>` under the match build;
- `ObjRef` natively, so the native intrusive ring is unchanged.

Walkers spell `ObjRefList::const_iterator` with `RefPtrOf(it)`, which compiles in both builds. That covers 17 files
across bandobj, rndobj, world, char, flow, hamobj and obj.

`MergeObjectsRecurse` needs the const `Refs()` spelling. A non-const `iterator` compared against `end()` spilled an
`end()` temporary and moved the `ObjDirPtr` slot by 8. The row read 99.25 with that spelling and 100 without it.

### Fold memberships admitted

All three were proven with `icf_pair_adjudicate --chase`, CHASED T1.

| our spelling | survivor | note |
|---|---|---|
| `list<ObjRefOwner*>::insert` | `0x823D14C0` | restored |
| `list<ObjRefOwner*>::erase` | `list<Voice*>::erase` | also flat T1 |
| `list<ObjRefOwner*>::operator=` | `list<Symbol>::operator=` | |

The `insert` membership had been withdrawn by `ALIAS-CONSOLIDATION 2026-08-19` as `FABRICATED_CLOSURE_NOT_PARTITION`.
That is a generator-blanket class, not a per-membership refutation. The withdrawal record moved into a `restored`
entry's `superseded_records`, following the W9-D precedent. The restoring evidence has two channels: CHASED T1, and the
AddRef call site.

### Rows

All of these go to 100:
- the `Object` constructor and destructor;
- `AddRef` and `Release`;
- `fn_8275CD54`;
- PatchDir's `Object::operator=`.

`DrawToTexture` goes 96.30 → 96.66. **+6 fns / +776 B, 7 up, 0 down.**

## 3. MemMgr `.bss` (commit `baf9a231a`)

### Retail layout

Retail's MemMgr unit, with `MemHeap.cpp` compiled into it, has one `.bss` block anchored at `0x82E069A8`. The offsets
come from the users of each object: ThreadMemStack, MemFree, MemFindHeap, MemInit and `MemHeap::Init/Free/Truncate`.

| offset | object |
|---|---|
| +0x000 | `gNullMemStack` |
| +0x048 | `gThreadBuf` |
| +0x1F8 | `gSingleHeap` |
| +0x200 | `gHeaps` |
| +0x440 | `gThreadBufCurrentIndex` |
| +0x444 | `gNumThreads` |
| +0x448 | `gTimeStamp` |
| +0x454 | `gNumHeaps` |
| +0x458 | `gCheckConsistency` |
| +0x45C | `gInsideMemFunc` |
| +0x460 | `gMemLock` |
| +0x464 | `gMemStackLock` |

### How the layout is reproduced

This compiler lays uninitialized definitions out in **reverse** declaration order. Later items fill alignment holes
first-fit. Initialized definitions are appended afterwards. So the retail block is declared top-down, with three
changes:

1. **Stand-ins for three dead words.** No retail code reaches the words at `+0x1FC`, `+0x44C` and `+0x450`. Extern
   `gMemMgrUnknown*` stand-ins hold that space, following dc3's convention. They must be extern: an unreferenced static
   is dropped and its hole is refilled.
2. **`gMemStackLock` is now defined.** It had no definition anywhere in `src`; native weak-stubbed it. Retail defines
   it here.
3. **`gInsideMemFunc` loses its `= false`.** The initializer would move it into the later zero-init group.

MemFree walks `gHeaps[i]` instead of through a `heap` pointer. With that spelling the compiler anchors the co-addressed
block on `gHeaps`, as retail does, rather than on `gNumHeaps`.

### Rows

To 100:
- MemFree (from 97.45)
- MemAllocSize (from 99.98)
- MemInit (from 99.99)
- MemPrintOverview (from 99.97)

MemFindHeap reaches `mpn` 100; its fuzzy score is 99.57, from a register swap on the "physical" string.

**ThreadMemStack stops at 85.17** (from 85.05). Every `.bss` offset in it is now right. The residue is CritSecTracker
codegen:
- retail never stores the tracker to `0x50(r31)`, although its funclet `0x827BB9F4` destroys it there;
- retail shares one `Exit` tail.

That is not a layout defect and is left. AddHeap, MemTruncate and MemAlloc carry register-only residue.

**+5 fns / +1,508 B, 0 down** (53,475 → 53,480 fns; 5,911,672 → 5,913,180 B, by difference from §2).

## 4. RemoteBandUser (commit `29701f440`)

### 4.1 Retail has no `RemoteBandUser::Handle` override

The vtable at `0x820E026C` has its COL at `0x821E6CAC`: `.?AVRemoteBandUser@@`, at `+0x18`, the User subobject. Its
Handle slot (slot 6) holds `0x8268E2B0`. That is a 16-byte vtordisp thunk:

```
lwz r11,-4(r4); subf r4,r11,r4; addi r4,r4,0xdc; b fn_8268CE18
```

`fn_8268CE18` is `BandUser::Handle`. If RemoteBandUser overrode Handle, this slot would branch to a RemoteBandUser
body. A second, independent fact points the same way: the message's type string `wii_friends_list_changed` occurs
nowhere in retail `band.exe`. A Python byte count over the image found 0 occurrences, while `RemoteBandUser` and
`AutoplayAuditionUser` each occur once.

So three things are removed from source: `RemoteBandUser::Handle`, `OnMsg(WiiFriendsListChangedMsg)` and
`ShowCustomCharacter`. Nothing else called them. cl.exe now emits exactly the thunk retail has,
`?Handle@BandUser@@$4PPPPPPPM@PPPPPPCE@AA?AVDataNode@@PAVDataArray@@_N@Z`. `0x8268E2B0` is renamed to that spelling,
and the row goes **73.75 → 100**.

### 4.2 `0x8268EF78` and `0x8268ED70` belong to AutoplayAuditionUser

The vtable at `0x820E0E14` has its COL at `0x821E6FA0`: `.?AVAutoplayAuditionUser@@`, at `+0x2c`.

| address | was mapped as | is |
|---|---|---|
| `0x8268ED70` | `?GetLocalBandUser@RemoteBandUser@@$4PPPPPPPM@A@BA…` | `??_EAutoplayAuditionUser@@$4PPPPPPPM@A@AAPAXI@Z` |
| `0x8268EF78` | `?GetLocalBandUser@RemoteBandUser@@UBA…` | `??_GAutoplayAuditionUser@@UAAPAXI@Z` |

- **`0x8268ED70`** is that vtable's slot 0, a vtordisp thunk into `0x8268EF78`.
- **`0x8268EF78`** subtracts `0x2c` from `this`, calls the destructor `0x8268EEE0`, and calls `operator delete` when
  `flag & 1` is set. That is a scalar deleting destructor.

AutoplayAuditionUser has no source in this repo, rb3 or dc3; it is TU5-era. Its methods sit at `0x8268ED20`–`0x8268EF78`,
inside BandUser's `.text`. Both rows are now unpaired. The 12 B thunk row had been a **false 100**: our
`GetLocalBandUser` thunk paired against a deleting-destructor thunk.

**The freed spellings fold where retail's vtable sends them.** RemoteBandUser's BandUser-subobject vtable
(`0x820E023C`, `+0x5c`) has slots 6 and 7, `GetLocalBandUser()` and the const overload, both pointing at `0x8268B8A8`.
That thunk's tail branch at `0x8268B8B0` reaches the return-0 survivor `0x823591E8`. Admitted with `--chase`
CHASED T1 PROVEN:

| our spelling | survivor | adjudication |
|---|---|---|
| `?GetLocalBandUser@RemoteBandUser@@$4PPPPPPPM@A@BAPAVLocalBandUser@@XZ` | `0x8268B8A8` | VACUOUS-DESTINATION-FOLD-PROVEN |
| `?GetLocalBandUser@RemoteBandUser@@UBAPAVLocalBandUser@@XZ` | `0x823591E8` | VACUOUS-BUT-IDENTICAL, admitted on the vtable witness |
| `?GetLocalBandUser@RemoteBandUser@@UAAPAVLocalBandUser@@XZ` | `0x823591E8` | VACUOUS-BUT-IDENTICAL, admitted on the vtable witness |

W15-D had withheld the first one only because the map gave it a second home at `0x8268ED70`, which this lane renamed.

**Measured:** +16 B (Handle thunk) − 12 B (the false thunk match) = **+4 B, 0 fns, 0 rows down.** The deliberate −12 B
is accuracy over headline.

## 5. MeasureMap's pin at `0x82449998` was Part.cpp's `list<Plane>::erase(first, last)` (commit `f8ee25e8c`)

The retail body at `0x82449998` is 96 B. It loops `while (*first != *last) erase(first++)` through `fn_82447458`
(`list<Plane>::erase(iterator)`) and returns `last`. That is `list::erase(iterator, iterator)`, not
`__uninitialized_copy<TimeSigChange>`.

Its only retail caller is `list<Plane>::resize` at `0x8244D190`, in Part.cpp. The address sits between Part's
`0x82446148–0x82449930` and `0x824499F8–0x8244D18C` pins, and our `Part.obj` defines the range-erase spelling.

The change:
- the `.text` pin, and the `.pdata` derived from it (`0x8220D678`), move from MeasureMap.cpp to Part.cpp;
- the map name becomes `?erase@?$list@VPlane@@…@U32@0@Z`.

This is a re-home, so pairability changes. That is expected (CLAUDE.md, "pin neutrality is scoped to reattribution").

**Measured:** **+2 fns / +240 B, 0 down.**
- The erase row reads 100.
- `list<Plane>::resize` goes 99.86 → 100, because its call now names a defined target.
- MeasureMap is 10/10 on `mpn`. One row is at fuzzy 99.5: `fn_827D0E70`, a funclet that was not touched.

## 6. WaveFile: one anonymous-namespace hash (commit `336548dcb`)

### Why the rename alone loses bytes

The body at `0x827D33F8` copy-constructs 0x14-byte elements through `_Copy_Construct<WaveFileMarker>` and returns the
destination. It is `__uninitialized_copy<WaveFileMarker>`, not `_Destroy_Range<Label>`.

The rename alone reproduced W16-PC's loss: **−1,096 B, 6 rows down**:
- `ReadMarkers`
- `__adjust_heap<CuePoint>`
- `__final_insertion_sort<CuePoint>`
- three funclets

**The mechanism, read from `obj_anon_ns_patcher.plan_object`:**
1. The map spelled WaveFile.cpp's one anonymous namespace with three hashes: `81ddebd1`, `9335ac2a` and `9ea89d7a`.
   Those hashes are map-author choices, not retail facts; retail carries no names.
2. With the wrong `_Destroy_Range<Label@81ddebd1>` gone, the token `Label` in the paired retail object resolved
   unambiguously to `9335ac2a`. The patcher's `token` rule (rule 5) then respelled `vector<Label>`'s helpers.
3. Because `majority` counts token-rule edits too, our object's fallback hash flipped as well.
4. The alias groups list the `81ddebd1` spellings, so `ReadMarkers`' calls stopped being forgiven:
   `vector<Label>::~vector` → `vector<TypeCreatorPair>`, `push_back<CuePoint>` → `push_back<Vector2>`, and so on.

### The fix is in the map, not the patcher

One source file produces one hash, so every WaveFile-unit anonymous-namespace name now uses `81ddebd1`:

| address | before | after |
|---|---|---|
| `0x827D33F8` | `_Destroy_Range<Label@81ddebd1>` | `__uninitialized_copy<WaveFileMarker>` |
| `0x827D3340` | `__destroy_range_aux<reverse_iterator<Label@9335ac2a>>` | same with `@81ddebd1` |
| `0x827D3488` | `sort_heap<CuePoint@9335ac2a>` | `@81ddebd1` |
| `0x827D3900` | `__final_insertion_sort<CuePoint@9ea89d7a>` | `@81ddebd1` |
| `0x822A2648` (Gem `.text`) | `__destroy_range_aux<reverse_iterator<Label@81ddebd1>>` | `__destroy_range_aux<reverse_iterator<OutfitConfig::Overlay>>` |

`0x822A2648` is the second home that forced the `9335ac2a` spelling at `0x827D3340`, which is the body our Label
instantiation pairs with at 100. Its body differs from Label's: the re-chase gives REFUTED `BODY:BYTES-DIFFER`. Its
group already held `Overlay` as a proven member, which is now its name.

### Alias survivors

The five drifted survivors were relabelled by `tools/alias_survivor_relabel.py --write` after a full build, so the
re-chase saw the new spellings in both the target and our objects. The run was:
1. build once with a first relabel;
2. `git checkout` the alias file in the lane worktree;
3. relabel again against the fresh objects.

| outcome | count | detail |
|---|---:|---|
| old label withdrawn | 1 | REFUTED (`_Destroy_Range<Label>` at `0x827D33F8`) |
| old label refuted as a member | 1 | `0x822A2648`; that spelling is now the map name at `0x827D3340` |
| old label held, UNDECIDABLE | 3 | `MISSING(ours)`: the `9335ac2a`/`9ea89d7a` spellings no longer exist |
| folded members re-chased PROVEN | 4 | |
| folded member UNDECIDABLE | 1 | an existing `e9afadec` Label spelling (`MISSING(ours)`), kept |

**Measured:** **+2 fns / +192 B, 0 down.**
- `__uninitialized_copy<WaveFileMarker>` goes 34.62 → 100.
- Gem's Overlay row goes 0 → 100.
- The six rows the rename alone dropped stay at 100.

**What was deliberately not done:** the patcher was not changed. One inconsistency is worth recording for whoever does
change it. The `plan_object` docstring calls rules 5–7 non-evidence fallbacks, yet `majority` counts token-rule
(rule 5) edits in its vote, so one map edit can flip a whole object's fallback hash. Changing that touches all 49
patched files and needs its own whole-binary A/B. After this lane's map fix, WaveFile no longer depends on it.

## 7. Whole-binary A/B

`tools/ab_measure.py --worktree ~/tmp/wt-w16-pi-ab --patch <git diff main w16-pi>`, at main `5bb85d98f`.
Patch kinds: map, source, splits. Both legs were force re-split and both sat at the `symbols.txt` fixed point.
Leg B recompiled 1,005 objects and the renamer patched 1,845 files.
Run dir: `~/tmp/wt-w16-pi-ab/.ab_measure_runs/20261006-084037-w16-pi-3771156/`.

```
leg A: matched=53469 masked=25191 honest=28278 code%=57.799340
leg B: matched=53484 masked=25192 honest=28292 code%=57.825882
Δmatched=+15  Δmasked_equal=+1  Δhonest=+14  Δcode%=+0.026542pp  Δcode_bytes=+2720
units at 100% [mpn]: 547 -> 548 (MeasureMap, DENOMINATOR_SHRANK); all-rows-fuzzy 488 -> 488
```

**Prediction:** +15 fns / +2,720 B, the sum of the per-item row-compares in §1 (measured before the rebase).
**Measured:** exactly +15 / +2,720 B.

`rowcmp.py` on the archived `legA_report.json` and `legB_report.json` gives **16 rows up and 0 rows down**. The renamed
rows appear only as "ONLY A" or "ONLY B" and are each accounted for above:
- 3 in BandUser (§4)
- 1 in MeasureMap (§5)
- 1 in Part (§5)
- 4 in WaveFile (§6)
- 1 in Gem (§6)

The only renamed row whose new spelling has no pair is AutoplayAuditionUser's, which costs the 12 B false match in §4.
The `none`-ruler control reads +2,576 B. The tool marks it `NOT_APPLICABLE` because the patch contains source.

## 8. Validators and native

On the rebased, fully built lane tree, whose `report.json` row-compares equal to the A/B's leg B (0 up, 0 down):
- `tools/alias_survivor_drift.py --quiet`: rc 0.
- `tools/map_name_injectivity.py`: OK. There are 35,204 applied rows and 35,203 distinct names, plus 1 enumerated
  exception.
- `tools/icf_alias_finder.py --validate`: PASS. 1,907 groups are map-consistent, 225 are tolerated and 0 are
  contradicted. It refused once, correctly, with "build owed" after the rebase; it was run again after the build.
- `tools/test_alias_survivor_drift.py`: 14 passed.

Native checks, run last on the final code. Only this section of the doc changed afterwards, and both checks were run
again after that commit.

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=57 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=17 scatter_dirb=0 scatter_multihost=20 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

## 9. Not done

- **AutoplayAuditionUser was not written.** It is TU5-era code with no oracle in any repo. Its two rows (92 B) stay
  unpaired and correctly named.
- **ThreadMemStack's CritSecTracker tail (85.17) was not chased.** It is codegen, not layout (§3).
- **The anon_ns patcher was not changed** (§6, last paragraph).
- **Nothing in `src/network` or `src/xdk` was touched.**
- **The suspicious pin found next door was not investigated.** `0x82449930–0x82449998` is pinned to Accomplishment.cpp
  as `vector<Hmx::Color>::_M_erase`, inside Part.cpp's span. It may be a correct ICF survivor; it was not this lane's
  item.
