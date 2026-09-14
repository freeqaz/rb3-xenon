# W16-AO — the remaining inconsistent `list<T>` bijection rows

**Lane:** W16-AO (map / identification). **Branch:** `w16-ao`, based on main `424107a6`.
**Worktree:** `/home/free/tmp/wt-w16-ao`. **Ruler:** `name_check`, objdiff 4.2.9 `5a51cd51fe0a353f`.

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| leg A (base `424107a6`, settled) | 43,428 | 4,007,884 B | 39.116558 |
| lane tip | **43,436** | **4,008,828 B** | **39.125770** |
| **Δ** | **+8** | **+944 B** | **+0.009212 pp** |

`total_code` 10,246,004. Priced by set-diff of the `fuzzy == 100` row set
(`tools/rowset_snapshot.py`), **8 rows crossed in, 0 fell out**, and independently
re-measured by `tools/ab_measure.py --revert` (which reproduced −8 / −944 B exactly).
`masked_equal` is **unchanged** at 23,047 on both legs, so Δhonest is a real **+8**
and not funclet disclosure.

Leg A reproduced the briefed baseline (43,428 / 4,007,884 / 39.116558) **to the last
digit**, which is the control that the worktree is a faithful copy of main at this base.

---

## 0. The method, and the three ways it is not safe as briefed

W16-AM §2's anchor is: `resize<list<T>>` (144 B) calls `erase<list<T>>` + `insert<list<T>>`,
and the erase body's `~T` relocation names `T`. That is correct as far as it goes, and it is
what decided six of these rows. It is **not sufficient**, and each of the three gaps below
would have produced a confidently wrong name if the anchor had been taken on faith.

### (a) The erase anchor is DEGENERATE for trivially-destructible `T`

For such `T` there is no `~T` call at all: the range `erase(first,last)` is **96 B** (not 108)
and forwards to a single-iterator `erase` whose only callee is `MemOrPoolFreeSTL`. ICF then
folds that erase across **every `T` of equal node size**. Measured: `resize<list<String>>`
(`0x822c68f8`) and `0x823286c0` call **the same** erase `0x822c5e68`. On this stratum the
anchor identifies `sizeof(T)`, **not** `T`.

⇒ Read the erase body's size first: **0x6C ⇒ the `~T` anchor exists; 0x60 ⇒ it does not.**

### (b) The anchor can be poisoned one level down — and it contradicted a second channel

`0x822b6798`'s erase calls `??1EventCall@EventAnim@@`, but its only retail caller is named
`?resize@?$ObjList@UProxyCall@EventTrigger@@@@QAAXI@Z`. Both channels cannot be right: the
two resizes call erases with **different** `~T` relocations, so ICF cannot have folded them.

### (c) The discriminator that settles both — the node-size immediate

Every `list<T>` erase loads the node size into `r3` immediately before `MemOrPoolFreeSTL`
(`void(int size, void*)`), so **`sizeof(T) = node − 8`**, read straight off retail bytes and
independent of every name in the map:

| erase | `li r3` | ⇒ `sizeof(T)` | matching struct |
|---|---:|---:|---|
| `0x822b56b8` | 0x20 (32) | **24** | `EventAnim::EventCall {ObjPtr,ObjPtr}` = 24 ✓ (`ProxyCall` is 28) |
| `0x824a02c8` | 0x24 (36) | **28** | `EventTrigger::ProxyCall {ObjOwnerPtr,Symbol,ObjOwnerPtr}` = 28 ✓ |
| `0x824cdb80` | 0x20 (32) | **24** | `WorldDir::BitmapOverride {ObjPtr,ObjPtr}` = 24 ✓ |
| `0x824cdb10` | 0x20 (32) | **24** | `WorldDir::PresetOverride {ObjPtr,ObjPtr}` = 24 ✓ |
| `0x823c38e8` | 0x18 (24) | **16** | `CharBlendBone::ConstraintSystem {ObjPtr,float}` = 16 ✓ |
| `0x82328c90` | 0x84 (132) | **124** | `LayerDir::Layer` |
| `0x822a6600` | 0x3c (60) | **52** | `OldMatOption {3×ObjPtr, ObjVector}` = 52 ✓ |
| `0x824a0730` | 0x3c (60) | **52** | `EventTrigger::Anim` (last member `Symbol mType`@0x30) = 52 ✓ |
| `0x824e03a8` | 0x58 (88) | **80** | `OldMMInst {Transform 64, Hmx::Color 16}` = 80 ✓ |
| `0x824e0350` | 0x48 (72) | **64** | `Transform {Matrix3 48, Vector3 16}` = 64 |

⇒ (b) is settled **for the destructor and against the caller**: 24 is `EventCall`, not
`ProxyCall`. The two destructor names are right; it is the **caller's** map name at
`0x822b6ee8` that is wrong — that `ObjList<T>::resize` wrapper is part of the same shuffle.
This matters because AM used an `ObjList<T>::resize` wrapper as corroboration; the wrappers
are **not** an independent oracle in general, they are shuffled alongside the rows they wrap.

### The `ObjPtr` vs `ObjOwnerPtr` distinction, used twice

`ObjPtr<T>` *is* `ObjRefConcrete<T,ObjectDir>` and its destructor is called **directly**.
`ObjOwnerPtr<T>` is a separate class with its own 0x74 destructor that calls
`?Release@Object@Hmx@@`. So a `~T` that calls `??1?$ObjRefConcrete@...` directly has **`ObjPtr`**
members, while one that calls a 0x74 `Release` thunk has **`ObjOwnerPtr`** members. That is
what separates `EventCall` (two `ObjPtr`) from `ProxyCall` (two `ObjOwnerPtr`) on the member
side, and it is how `0x824a1ab0` was decided.

---

## 1. Per-address adjudication

Evidence classes: **R** = `~T` relocation target in the erase body · **N** = node-size
immediate (`sizeof(T) = node − 8`) · **C** = retail caller (`tools/retail_callers.py`) ·
**I** = insert callee's spelling · **S** = source struct layout.

### Repaired (10 briefed rows → 9 wrong, 1 correct-as-spelled)

| # | address | old spelling (`T`) | evidence | new spelling (`T`) |
|---|---|---|---|---|
| 1 | `0x822b6798` | `ProxyCall@EventTrigger` | **R** erase `0x822b56b8` → `??1EventCall@EventAnim@@`; **N** node 0x20 ⇒ 24 = `EventCall`; **I** `insert<list<EventCall@EventAnim>>` `0x822b5728` | **`EventCall@EventAnim`** |
| 2 | `0x824cf800` | `Sink@MsgSinks` | **R** erase `0x824cdb80` → `??1BitmapOverride@WorldDir@@`; **N** node 0x20 ⇒ 24; **C** `?resize@?$ObjList@UBitmapOverride@WorldDir@@@@` | **`BitmapOverride@WorldDir`** |
| 3 | `0x824cf770` | *(absent key)* | **R** erase `0x824cdb10` → `??1PresetOverride@WorldDir@@`; **N** node 0x20 ⇒ 24; **C** `?resize@?$ObjList@UPresetOverride@WorldDir@@@@` | **`PresetOverride@WorldDir`** (ADD) |
| 4 | `0x823c40f0` | `PresetOverride@WorldDir` | **R** erase `0x823c38e8` → `??1?$ObjRefConcrete@VRndTransformable@@VObjectDir@@@@`; **N** node 0x18 ⇒ 16 = `{ObjPtr,float}`; **I** `insert<list<ConstraintSystem@CharBlendBone>>` `0x823c3ac8`; **S** `MsgSinks::Sink` is `{Hmx::Object*,SinkMode}` = 8 with **no destructor at all** | **`ConstraintSystem@CharBlendBone`** |
| 5 | `0x823296d0` | `FilePath` | **R** erase `0x82328c90` → `??1Layer@LayerDir@@`; **N** node 0x84; **C** `?resize@?$ObjList@VLayer@LayerDir@@@@QAAXI@Z` | **`Layer@LayerDir`** |
| 6 | `0x824a1b40` | `Anim@EventTrigger` | **R** erase `0x824a02c8` → `??1ProxyCall@EventTrigger@@`; **N** node 0x24 ⇒ 28 = `ProxyCall` | **`ProxyCall@EventTrigger`** |
| 7 | `0x824a1ab0` | *(explicit JSON `null`)* | **R** erase `0x824a0730` → `fn_82400B98`, whose vtable `0x8205b4cc` slot 0 is `??_G?$ObjOwnerPtr@VRndAnimatable@@@@` ⇒ `~ObjOwnerPtr<RndAnimatable>`; **N** node 0x3c ⇒ 52; **S** `EventTrigger::Anim` has exactly one non-trivial member `ObjOwnerPtr<RndAnimatable> mAnim` and ends at `Symbol mType`@0x30 ⇒ 52; **C** `?resize@?$ObjList@UAnim@EventTrigger@@@@` | **`Anim@EventTrigger`** (was null) |
| 8 | `0x824e1a28` | `Instance@RndMultiMesh` | **N** node 0x58 ⇒ 80 = `OldMMInst{Transform 64, Color 16}`; **I** `insert<list<OldMMInst>>` `0x824e0f68`; **C** `operator>>(BinStreamRev&, list<OldMMInst,StlNodeAlloc<OldMMInst>>&)` | **`OldMMInst`** |
| 9 | `0x822a8668` | `OldMMInst` | **R** erase `0x822a6600` → `??1OldMatOption@@`; **N** node 0x3c ⇒ 52 = `OldMatOption` | **`OldMatOption`** |
| 10 | `0x8246af50` | `Transform` | see §2 — **not a defect** | **LEFT ALONE** |

### The coupled 11th row

| # | address | old spelling | evidence | new spelling |
|---|---|---|---|---|
| 11 | `0x823286c0` | `Layer@LayerDir` | **C** its only caller `0x82328800` default-constructs a `String` (`??0String@@QAA@XZ`), calls this resize, destroys it (`??1String@@UAA@XZ`), then streams each element with **`??5@YAAAVBinStream@@AAV0@AAVFilePath@@@Z`**; **N** node 0x14 ⇒ 12 = `FilePath : String` | **`FilePath`** |

`0x823286c0` is **not** on the briefed worklist. It is included because
`FilePath` ↔ `Layer@LayerDir` is an exact **transposition**, and the ninja-wired
`map_name_injectivity_check` fails the build on a half-applied swap — the repair is
atomic or it is nothing. `FilePath` is the name displaced from worklist row #5, and
the displacement is proven on its own caller's bytes, not inferred by elimination.

`String` (`0x822c68f8`) is the control that keeps this honest: it shares `0x823286c0`'s
folded erase but has its **own distinct insert** (`0x82270b20` vs `0x82326fe8`), which is
why the two resizes did not fold into one address.

---

## 2. `0x8246af50` — LEFT ALONE, because it is not wrong

`0x8246af50` has **two** retail call sites:

```
0x8246b0e0  operator>>(BinStreamRev&, list<Transform,             StlNodeAlloc<Transform> >&)
0x824e1da0  operator>>(BinStreamRev&, list<RndMultiMesh::Instance,StlNodeAlloc<Instance>  >&)
```

`struct RndMultiMesh::Instance` has exactly one member, `Transform mXfm`
(`src/system/rndobj/MultiMesh.h:94`), so the two instantiations are byte-identical
**including relocations** and ICF folded them. **Both spellings are simultaneously true at
that address**, so there is no wrong name to repair, and the existing `Transform` is a
legitimate fold survivor. Node size agrees and cannot separate them: 0x48 ⇒ 64 =
`sizeof(Transform)` = `sizeof(Instance)`.

The same fold family carries the survivor spelling **inconsistently across functions** —
`0x824e0e40` erase is spelled `Transform` while `0x824e0f00` insert is spelled
`Instance@RndMultiMesh`. That is the arbitrary-survivor phenomenon, not a defect.

Independent control: `0x8241b618` is
`?resize@?$list@UInstance@RndMultiMesh@@V?$**TransformListAlloc**@...` — the *real*
`RndMultiMesh::InstanceList` from the source typedef uses `TransformListAlloc`, is a
different COMDAT, and did **not** fold. Its existence is also what confirms row #8: the
`StlNodeAlloc`-flavoured `Instance@RndMultiMesh` spelling at `0x824e1a28` was displaced,
not native.

Filed for W16-AL as **`docs/decomp/W16AO_ALIAS_PROPOSALS_FOR_W16AL.json`** (report only).
**`scripts/symbol_aliases.json` and every `icf_alias_*` file were never touched.**

---

## 3. Spelling construction — no fragment was hand-mangled

Every `StlNodeAlloc` resize row in the map satisfies

```
?resize@?$list@{T}V?$StlNodeAlloc@{T}@stlpmtx_std@@@stlpmtx_std@@QAAXIAB{T}@Z
```

verified to reproduce **20 of 20** such existing rows byte-exactly before it was used
(the 21st, `0x8241b618`, is the `TransformListAlloc` row above and is correctly *not*
matched). Each `{T}` was then **lifted verbatim** from a donor row that already names that
`T` (its own erase or insert), so MSVC back-reference indices and the `U` (struct) vs `V`
(class) tags are carried across rather than re-derived — `VEventCall@EventAnim@@` and
`VOldMatOption@@` are classes, `UProxyCall@EventTrigger@@` and `UOldMMInst@@` are structs.

`scripts/target_symbol_map.json` was round-tripped with
`json.dumps(d, indent=1, ensure_ascii=False)+'\n'`, proven byte-identical on a no-op first
(2,412,058 B in, 2,412,058 B out). Keys are lowercase; `_bijection_arbitrary` and every
other non-address key are preserved. The diff is exactly the 11 rows plus one cosmetic
trailing comma on the previously-last key.

**Injectivity:** `29,335 applied rows, 29,334 distinct names, injective (+1 enumerated
internal-linkage exception)`, rc=0. The two pre-existing duplicates (`?NodeCmp@@YAHPBX0@Z`,
`__destroy_aux<LevelData>`) are unchanged.

---

## 4. Price

**Set-diff, 8 rows crossed in (944 B), 0 rows fell out:**

| bytes | row |
|---:|---|
| +144 | `BandCamShot::?resize@?$list@VEventCall@EventAnim@@…` |
| +144 | `EventTrigger::?resize@?$list@UProxyCall@EventTrigger@@…` |
| +144 | `Crowd::?resize@?$list@UOldMMInst@@…` |
| +144 | `band3/meta_band/ViewSetting::?resize@?$list@UPresetOverride@WorldDir@@…` |
| +144 | `LayerDir::?resize@?$list@VLayer@LayerDir@@…` |
| +88 | `Crowd::??$?5UOldMMInst@@…` — `operator>>(BinStreamRev&, list<OldMMInst>&)` |
| +68 | `Shockwave::?resize@?$ObjList@UBitmapOverride@WorldDir@@@@QAAXI@Z` |
| +68 | `LayerDir::?resize@?$ObjList@VLayer@LayerDir@@@@QAAXI@Z` |

★ **The crossed set is the confirmation, not the payout.** Three of the eight are
**callers** of the corrected symbols — and they are the *same* functions used as
independent proof channels in §1 (rows #2, #5, #8). A wrong name cannot make an unrelated
caller row cross to 100 %. This is "a wrong name is financed by its callers" running in
reverse, and it is a stronger signal than +944 B.

Unit attribution sums exactly: Crowd +2, LayerDir +2, BandCamShot +1, EventTrigger +1,
Shockwave +1, ViewSetting +1 = **+8**.

**Zero un-pairing cost** (AM's comparable wave lost 476 B). That is a property of closing a
*complete* permutation: every displaced spelling landed on its own proven address, so no
row was orphaned.

### The `none` control — and why it is NOT the clearance

```
[leg A none ruler] matched=45105 code%=44.032852
[leg B none ruler] matched=45105 code%=44.032852
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler -944 B)
[control none] FLAT: `none` UNMOVED and default not up — consistent with a pure RE-name
```

`none` is **exactly flat on both measures** while the graded ruler carries the whole ∓944 B.
⚠ **This flatness clears nothing.** CLAUDE.md is explicit that `none` ignores relocation
names, so a name change reads +0 there **by construction** — the flatness is the *signature*
of the fabricated-alias hazard, not a pass, and on a **map-only** patch it is also the
`ALIAS_SUSPECT` shape. The clearance here is different and does not rest on the metric at all:

1. **No forgiveness was added.** `symbol_aliases.json` was untouched; every byte came from
   map rows, not from a new alias membership.
2. **Each name is proven on retail bytes** by ≥2 independent channels (§1), the primary one
   (`R`/`N`) being a relocation or an immediate in the body itself.
3. **Three caller rows crossed**, which a wrong name cannot cause.

This is the MAPDEF-3 class — *repairing a WRONG existing map name pays under `name_check`
with `none` unmoved* — and it is legitimate for the same reason MAPDEF-3 was: the names are
adjudicated on bytes, not on the score.

⚠ **Observation, flagged not asserted:** `matched_functions` is **not** ruler-invariant on
this tree — 43,436 (`name_check`) vs 45,105 (`none`), a 1,669-row gap — which does not
reproduce CLAUDE.md's RULER-SWEEP claim that the two are "bit-identical". Both legs move
together so it does not affect this lane's delta, but someone should re-derive that claim.

---

## 5. Findings recorded but NOT acted on

All are outside the briefed 10 addresses. Each is stated with its evidence so nobody
re-hunts it; none is a guess, and none was written to the map.

- **`0x822b6ee8`** is spelled `?resize@?$ObjList@UProxyCall@EventTrigger@@@@QAAXI@Z` but
  forwards to `0x822b6798`, proven above to be `list<EventCall@EventAnim>::resize`
  (node 0x20 ⇒ 24). `EventAnim::KeyFrame` holds `ObjList<EventCall> mCalls`, so the wrapper
  should be `?resize@?$ObjList@VEventCall@EventAnim@@@@`. **The true `ObjList<ProxyCall@EventTrigger>::resize`
  is a different address.** ⚠ This is the row that contradicted the destructor anchor, so
  fixing it also removes a live trap for the next lane.
- **`0x823c38e8`** is spelled `?erase@?$list@USink@MsgSinks@@…` but its own body calls
  `??1?$ObjRefConcrete@VRndTransformable@@VObjectDir@@@@` with node 0x18 ⇒ `sizeof(T)` = 16.
  `MsgSinks::Sink` is `{Hmx::Object*, SinkMode}` = 8 **with no destructor at all**, so this
  row is wrong; it is `erase<list<ConstraintSystem@CharBlendBone>>`.
- **`0x822a8bd0`** is spelled `?resize@?$ObjVector@UEyeDesc@CharEyes@@@@QAAXI@Z` yet calls
  `0x822a8668`, a **`list`** resize. An `ObjVector<T>::resize` cannot forward to a list
  resize, so either that name or the wrapper's identity is wrong. It did not overturn row #9,
  which rests on a direct `??1OldMatOption@@` relocation plus an exact size match.
- **`Sink@MsgSinks`, `FilePath`-as-`0x823296d0`, and `Instance@RndMultiMesh`-with-`StlNodeAlloc`**
  are the three spellings displaced by this wave. `FilePath` was re-homed (row #11) and
  `Instance@RndMultiMesh` is explained by the fold (§2). **`Sink@MsgSinks` is now
  unplaced in the resize family** — `MsgSinks::Sink` is trivially destructible, so if a
  `list<Sink>::resize` exists at all its erase is size-degenerate and only the caller channel
  can find it. Not hunted.
- **Anonymous addresses identified in passing, deliberately NOT named:** `0x82400b98`
  (`~ObjOwnerPtr<RndAnimatable>`, proven by vtable `0x8205b4cc` slot 0), `0x824ce0c8` /
  `0x824ce060` / `0x822a6670` / `0x82326fe8` (the inserts for `BitmapOverride`,
  `PresetOverride`, `OldMatOption`, `FilePath`), `0x8249b660` / `0x8249b5b8` (the two
  `ObjOwnerPtr` dtors under `~ProxyCall`). **Naming an anonymous address is a bet that pays
  in bug exposure, not bytes** (objdiff already forgives placeholder targets), so each needs
  its own adjudication and its own measurement. Listed as a vein, not claimed as work.

## 6. NOT done, and why

- **`0x8246af50` not renamed** — it is not wrong (§2). Renaming a true fold survivor would
  destroy evidence and could un-pair its callers.
- **`scripts/symbol_aliases.json` / `icf_alias_*` not touched** — W16-AL's files. The one
  alias this lane would propose is delivered as report-only JSON.
- **`config/45410914/splits.txt`, `src/`, and any GamePanel / PerfectOverdriveTracker file
  not touched** — concurrency bars. No identification here required a source change.
- **No rebase onto the new main.** W16-AL landed mid-lane (main `73f16555`, +6 alias groups,
  map rows `0x82682668` and `0x826100f8`). **Neither of AL's addresses is in this lane's
  set**, and this lane's eleven rows are disjoint from both, so the two are independent;
  the coordinator rebases at landing. All figures here are lane-internal against the
  `424107a6` base, which reproduced the old briefed baseline exactly.
- **The three wrong wrapper/erase rows in §5 not repaired** — outside the briefed ten. They
  are adjudicated and ready; a follow-up lane can land them without re-doing the analysis.

---

## 7. Gates

All run in the worktree, in order, on the final tree. Four full builds, all rc=0
(`~/tmp/rb3_build_w16ao_{1,2,3}.log` plus `~/tmp/rb3_build_w16ao_ab.log` for the A/B).

```
BUILD (full, after touch config.yml)              rc=0   ~/tmp/rb3_build_w16ao_3.log
scripts/verify_ruler_agreement.py --check         rc=0
    OK  functionRelocDiffs = name_check / combineDataSections = true
    OK  combineTextSections = true / ppc.calculatePoolRelocations = false
    OK: both objdiff-cli entry points resolve the same ruler.
scripts/verify_objs_patched.py --verify-manifest  rc=0
    [denylist] OK: 6 denylisted address(es), 3 with a live map string, none named
               in 3115 target objects (495648 symbols scanned)
    [patch-state] OK: 1215 decomp, 3115 target objects match
                  (tree_sha256=b4d62542edbbaa5d)
tools/native_build_gate.sh                        rc=0
```

`NATIVE_GATE_RESULT` line, verbatim:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The build was re-run and the measures re-read **after** `ab_measure` restored the tree, so
the numbers in §4 are the ones this tree actually carries: `43,436 / 4,008,828 B /
39.125770` with the same 8-crossed / 0-fell-out set-diff.

The gate was run again after this document was committed, since the doc commit is the
lane's last change; it touches only `docs/` and so is outside `ScatterIncludes.cmake`'s
scan of `src/`, but the "a comment-only commit broke the native link" incident makes the
re-run cheap insurance rather than ceremony.

## 8. Commits

| sha | what |
|---|---|
| `9435597c` | the 11 map rows + the W16-AL alias proposal (**+8 fns / +944 B**, `none` flat) |
| *this doc* | W16-AO write-up |
