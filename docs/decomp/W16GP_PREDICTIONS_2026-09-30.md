# W16-GP predictions log

Lane W16-GP, worktree `/home/free/tmp/wt-w16-gp`, branch `w16-gp`, base `4af5f32f0`.
Input: frozen scout report `/home/free/tmp/w16gp_report_frozen.md` (8 numbered
goals). Each step below is written BEFORE the edit, then measured with
`tools/ab_measure.py --worktree /home/free/tmp/wt-w16-gp --from-dirty`, and the
measured numbers are appended after. A miss is recorded as a miss, not
smoothed over.

## Re-scoping note (before Step 1)

The report frames goals 1-4 as a block that (per the task assignment) might
require `TrackWidgetImp.cpp` to exist first, to get exact mangled spellings
from our own compiled object. Checking this against the live tree first
(`tools/objsym_find.py` against `build/45410914/src/**/*.obj`) shows that is
only PARTLY true:

- `TrackWidget.obj` (already compiled today, no source changes needed)
  **already contains full instantiations** of `_S_sort<MeshInstance,...>`,
  `_S_sort<RndMultiMesh::Instance,...>`, `TrackWidgetImp<MeshInstance>::Sort`,
  `DoSort`, ctor/dtor, and `_List_base<MeshInstance,...>::clear` etc. This is
  because `MatWidgetImp` (`TrackWidgetImp<MeshInstance>`) has ALL its members
  inline in `TrackWidgetImp.h` already (Q2a in the report), so something in
  the currently-compiled tree already constructs/uses a `MatWidgetImp` and the
  compiler emitted the whole template chain for `MeshInstance` and (via a
  shared call) `RndMultiMesh::Instance` without needing the new `.cpp` at all.
- By contrast, `TextInstance`-typed and `MultiMeshWidgetImp`-typed symbols
  (`_List_base<TextInstance,...>`, `list<TextInstance,...>`,
  `WidgetInstanceCmp<TextInstance,...>`, `MultiMeshWidgetImp::Sort`) are
  **absent from every compiled object** in the tree right now — confirmed by
  `objsym_find.py` returning zero hits for all of them. `CharWidgetImp` and
  `MultiMeshWidgetImp` only have their `operator new`/`operator delete`
  compiled (someone `new`s them somewhere), not their real bodies, because
  those classes have out-of-line virtuals that only exist in the oracle
  `.cpp` we haven't ported yet.

⇒ **Goal 1 (the 0x827e3c38/0x827e3e10 relabel) and the MeshInstance half of
goal 2 (0x827e48c0) can be fixed NOW**, with exact mangled spellings pulled
from `TrackWidget.obj` as it stands today — no dependency on Step 3.
**The TextInstance half of goal 2 (0x827e6470), goal 3 (0x827e63f8,
`MultiMeshWidgetImp::Sort`), and goal 4 (the two thunks 0x827e5c48/0x827e5d78)
genuinely require Step 3 (porting `TrackWidgetImp.cpp`) first** — there is no
compiled body anywhere yet to pull an exact spelling from, confirming the
report's own framing for those specifically.

Revised commit order:
1. Goal 1 relabel (this section).
2. Goal 2 (MeshInstance half only): add `0x827e48c0` = `Sort@TrackWidgetImp<MeshInstance>`.
3. Port `TrackWidgetImp.cpp` (Step 3 / goals 5-7).
4. Goal 2 (TextInstance half), goal 3, goal 4: add map entries once the new
   object exists and defines them, plus splits.txt moves.

## Step 1: fix the 0x827e3c38 / 0x827e3e10 relabel

**Evidence already independently confirmed this session** (re-verified
against retail bytes, not taken from the report on faith):
- `0x827e3c38` (424 B) ends in `bl 0x8243dbf0`; `0x8243dbf0` is an
  undisputed, already-correct map entry for
  `_List_base<RndMultiMesh::Instance,...>::clear`. So `0x827e3c38` sorts
  `RndMultiMesh::Instance`, not `MeshInstance` as currently mapped.
- `0x827e3e10` (424 B) ends in `bl 0x827e1708`; that address frees 76-byte
  (`0x4c`) nodes (`li r3, 0x4c` at disassembly), consistent with `MeshInstance`
  (a `WidgetInstance` + one `RndMesh*`), not `Waypoint*` as currently mapped.
  (0x827e1708 itself is ICF-folded with an unrelated `SetlistArtRecord` clear
  of the same node size — expected, not something to "fix".)
- `tools/retail_callers.py 827e3c38 827e3e10` reproduces the report's caller
  lists exactly (`0x827e3c38` <- `0x827e48ac`, `0x827e6440`; `0x827e3e10` <-
  `0x827e48ec`).
- Exact mangled spellings pulled today from `build/.../TrackWidget.obj` via
  `tools/objsym_find.py` (not hand-typed):
  - RndMultiMesh::Instance-typed `_S_sort`:
    `??$_S_sort@UInstance@RndMultiMesh@@V?$StlNodeAlloc@UInstance@RndMultiMesh@@@stlpmtx_std@@V?$WidgetInstanceCmp@UInstance@RndMultiMesh@@@@@stlpmtx_std@@YAXAAV?$list@UInstance@RndMultiMesh@@V?$StlNodeAlloc@UInstance@RndMultiMesh@@@stlpmtx_std@@@0@V?$WidgetInstanceCmp@UInstance@RndMultiMesh@@@@@Z`
  - MeshInstance-typed `_S_sort`:
    `??$_S_sort@VMeshInstance@@V?$StlNodeAlloc@VMeshInstance@@@stlpmtx_std@@V?$WidgetInstanceCmp@VMeshInstance@@@@@stlpmtx_std@@YAXAAV?$list@VMeshInstance@@V?$StlNodeAlloc@VMeshInstance@@@stlpmtx_std@@@0@V?$WidgetInstanceCmp@VMeshInstance@@@@@Z`

**Edit**: in `scripts/target_symbol_map.json`,
- `0x827e3c38` <- the RndMultiMesh::Instance-typed spelling (currently holds
  the MeshInstance one, which is wrong for this address).
- `0x827e3e10` <- the MeshInstance-typed spelling (currently holds a
  Waypoint-typed spelling, which is wrong for this address and for which
  there is a separate open question in the report about whether the Waypoint
  case exists anywhere at all — not investigated by this lane, left alone).

Note this is NOT a simple value-swap of what's already at these two
addresses: the MeshInstance spelling moves 3c38->3e10, but the value going
INTO 3c38 (RndMultiMesh::Instance) was not previously present at either
address.

**Prediction**: this is a "repair a wrong existing map name" edit, which per
project history (map economics doc) PAYS under `name_check` because the base
object already defines both correct symbols (confirmed above, live in
`TrackWidget.obj` today) — so this should be a pure pairing gain, not
requiring any source change. Expect a small positive `matched_functions` /
`matched_code` delta (order of ~1-2 functions, ~800 B given both are 424 B
bodies, assuming they cross to fuzzy==100 outright) and the `none`-ruler
control to move less (or not at all), since `none` doesn't charge relocation
names either way. I do NOT expect a regression. If the delta is negative or
zero on `name_check`, that's a miss worth investigating before continuing —
possible cause: one or both rows may already be paired-but-divergent for a
reason unrelated to naming (i.e. a real body mismatch elsewhere in the
function), in which case renaming corrects identity but may not immediately
cross to 100%.

<!-- MEASURED RESULT APPENDED BELOW AFTER RUNNING ab_measure.py -->
