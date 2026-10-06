# W16-QL: `gTransListAlloc` node size and TaskMgr's destructor (2026-10-06)

Lane W16-QL followed up two residuals that W16-QI recorded in
`W16QI_STATIC_DTOR_REGION_2026-10-06.md` §4.1 and §4.3. Both are fixed in
`535d16c35` on branch `w16-ql`.

| row | before | after |
|---|---|---|
| `??__EgTransListAlloc@@YAXXZ` (Console, 64 B) | 99.94 | 100 |
| `??__FTheTaskMgr@@YAXXZ` (Task, 12 B) | 98.33 | 100 |

**A/B** (`tools/ab_measure.py --from-dirty`, worktree `~/tmp/wt-w16ql` off
`2d3582fa3`, `name_check` ruler): **Δmatched +2, Δcode +76 B, +0.000740 pp**,
Δmasked_equal 0, Δfuzzy 0.000000. `default/Task` reaches 100% (146 → 147 of
147). No unit fell off 100%. Leg B recompiled 629 objects because `obj/Task.h`
is a PCH-tree header. Before measuring I predicted +2 / +76 B (64 + 12), and
that is what the A/B measured.

Native gate: see §3.

## 1. `gTransListAlloc(0x48, "InstanceListNode")`

The premise was that retail's 0x48 means RB3's `RndMultiMesh::Instance` is
4 B smaller than DC3's. That is true, but our layout was already fixed. Only
the pool-size constant was still wrong.

**Retail evidence.**

- The initializer `0x82C3F2B8` (`??__EgTransListAlloc`) does `li r4, 0x48`
  before `bl ??0ReclaimableAlloc@@QAA@HPBD@Z` (`0x827BB268`).
- `list<RndMultiMesh::Instance, TransformListAlloc<…>>::_M_create_node` at
  `0x82419220` (Mesh unit) does `li r4, 0x48` and then
  `bl ?CustAlloc@ReclaimableAlloc@@QAAPAXH@Z` (`0x827BB260`). `allocate` passes
  `count * sizeof(_List_node<Instance>)`, so retail's node is 0x48 B: 8 B of
  links plus a 0x40 B `Instance`. These are the only two retail sites that
  load `gTransListAlloc` (`lbl_82CC63D4`) with a size. The third reference is
  `CustFree` in `erase` at `0x8241A7E8`.

**Compiler evidence.** I ran `scripts/harvest/class_layout_report.py
--tu src/system/rndobj/MultiMesh.cpp` on the worktree:

```
class ?$_List_node@UInstance@RndMultiMesh@@	size(72):
 0	| | _M_next
 4	| | _M_prev
 8	| Instance _M_data
class Instance	size(64):
 0	| Transform mXfm
```

So our node is already 0x48. The extra 4 B was DC3's `bool mIsVisible`
(padded to 4) at the front of `Instance`. Commit `248032bdd` (2026-06-06)
removed it from the struct, the save/load path and `WorldCrowd::Reset3DCrowd`,
but left DC3's `0x4C` in the `gTransListAlloc` definition. The fix is that one
constant, plus a comment citing both retail sites.

**Users of the layout.** I checked every user: `MultiMesh.cpp`,
`rnddx9/MultiMesh.cpp`, `rndobj/Utl.cpp`, `rndobj/Gen.{h,cpp}`,
`rndobj/Mesh.cpp`, `world/Crowd.cpp` and `bandtrack/VocalTrack.cpp`. None
references `mIsVisible` or hardcodes the node size, and each sizes through
`sizeof`. That covers native too: on native `TransformListAlloc::allocate`
uses `malloc(count * sizeof(T))`, so the pool constant never reaches it. The
`milo-native-engine` test objects that mention `gTransListAlloc` are built
from `../dc3-decomp`'s sources, not ours, so they are out of scope.

## 2. Why `??__FTheTaskMgr` calls `Hmx::Object::~Object`

**Retail evidence.**

- The stub at `0x82C49B00` is `lis/addi r3, TheTaskMgr; b ??1Object@Hmx@@UAA@XZ`
  (`0x8275CBF0`). It does not store TaskMgr's vptr first.
- The retail map has no `??1TaskMgr` at any address.
- The TaskMgr vtable is at `0x82103FFC`, the address the ctor at `0x82747998`
  stores at `+0`. Its slot 0, the scalar deleting dtor, is `0x8250D530`, which
  the map names `??_GEntityUploader@@UAAPAXI@Z`. The two were ICF-folded. Its
  body is `bl ~Object; if (flags & 1) operator delete(this); return this`.
  There is no vptr store and no `delete[] mTimelines`.

So retail's TaskMgr destructor does nothing of its own. Its members are a
pointer, `SongPos`, `bool`, `int`, `Timer` and `float`, and none has a
non-trivial destructor. The timelines are freed by `TaskMgr::Terminate`
(`0x8274A8F8`, already 100%), which `SystemTerminate` calls. DC3 moved
`delete[] mTimelines; mTimelines = nullptr;` into `~TaskMgr`. rb3-Wii has
`virtual ~TaskMgr() {}`.

**Why the declaration matters, and a failed first attempt.** With DC3's
out-of-line `~TaskMgr` (which does the `delete[]`), the stub called
`??1TaskMgr` and scored 98.33. That was the "fold-named callee" W16-QI
reported. My first fix used the rb3-Wii spelling, `virtual ~TaskMgr() {}`
inline. It dropped the stub to **0 / 3.3 canonical**. The inlined
user-declared dtor makes MSVC store `??_7TaskMgr@@6B@` into `this` before it
branches to `~Object` (three inserted instructions, 24 B against retail's
12 B). With the user-declared destructor removed entirely, MSVC's implicit
destructor skips that vptr store, the stub becomes a bare `b ~Object`, and the
row reads 100. `src/band3/net_band/EntityUploader.h` already uses this pattern
and comment, for the same retail shape at the same folded `??_G`.

⇒ **No user-declared destructor** is retail's declaration, not
`virtual ~TaskMgr() {}`. The vtable slot is still virtual because `Hmx::Object`'s
dtor is virtual.

**Native.** The native ctor allocates `mTimelines` (and `Init` allocates it
again, which is an existing native-only leak I did not touch). The dtor is
therefore kept under `#ifdef HX_NATIVE` with the old `delete[]` body, so
native behavior is unchanged. `TaskMgr` has no subclasses and nothing deletes
one explicitly.

**Not done.** Our `??_GTaskMgr` should now be byte-identical to
`??_GEntityUploader`, which would make a fold alias plausible. I did not add
an alias. Only `withdrawn` records in `scripts/symbol_aliases.json` mention
`??_GTaskMgr`, and adding a fold needs a separate retail-byte adjudication.

## 3. Native gate

I ran `tools/native_build_gate.sh` on the worktree at `535d16c35` (the source
commit). Unseeded worktree; its `layout_odr.py` phase also passed. Last lines:

```
layout:    LAYOUT_ODR_RESULT verdict=PASS x360_tus=1266 x360_failed=0 x360_split=0 x360_unresolved=0 x360_allowed=138 x360_stale=0 native_tus=1711 native_failed=0 native_split=0 native_unresolved=0 native_allowed=1 native_stale=0 rc=0
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
