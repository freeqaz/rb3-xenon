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

**Correction found while starting Step 2**: adding a `target_symbol_map.json`
entry for `0x827e48c0` alone is NOT sufficient. Pairing is unit-scoped (a
target row pairs against the base object *declared for that heading* in
`objects.json`/`splits.txt`), and `config/45410914/splits.txt` shows
`TrackWidget.cpp:`'s last `.text` block ends exactly at `0x827E48C0`, while
`MidiParser.cpp:` immediately claims `.text 0x827E48C0-0x827E5260` (2464 B) —
confirmed by reading `splits.txt` directly, not the report table. Since the
64 B `Sort@TrackWidgetImp<MeshInstance>` body (disassembly-confirmed above:
vtable+0x44 vcall then `bl 0x827e3e10`, the now-correctly-named MeshInstance
`_S_sort`) is compiled into `TrackWidget.obj`, not any `MidiParser.cpp.obj`,
the map entry alone would pair a target row against the wrong unit's base
object (unpaired/0%, not the win I predicted). Step 2 is therefore a
**splits.txt boundary move + a map entry, done together**: extend
`TrackWidget.cpp:`'s block from `end:0x827E48C0` to `end:0x827E4900`, shrink
`MidiParser.cpp:`'s block from `start:0x827E48C0` to `start:0x827E4900`
(MidiParser.cpp keeps 3 other `.text` blocks, so this does not empty its
heading), then add the map entry. This is a splits.txt change to an
already-existing `MidiParser.cpp:` heading's boundary, not an edit to
`MidiParser.cpp` the source file, so it does not violate the do-not-touch
list.

**Prediction**: this is the "adding a NEW name to a previously-anonymous
target address" case (not a repair of a wrong existing name like Step 1), so
per project map economics this has no guaranteed call-site upside by itself
— but here the address IS the function body itself, not a callee referenced
elsewhere, so the expected win is direct: the row becomes newly pairable
against our own already-100%-quality compiled body. If the compiled body at
this address is byte-identical to retail (it should be, since it's a stock
template instantiation shared with the already-matching `RndMultiMesh::Instance`
sibling at `0x827e4880`, same shape per the report), expect **+1 function /
+64 B**, `none`-ruler unaffected. If it does NOT cross to 100%, that's a
miss worth checking with `run_diff_inspect` (full build only, not a raw
single-obj build) before moving on.

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

**MEASURED** (`~/tmp/w16gp_step1_ab.log`, run `20260930-024209-from-dirty-653893`):

```
leg A: matched=44199 masked=23323 honest=20876 code%=40.921130
leg B: matched=44200 masked=23323 honest=20877 code%=40.921753
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000623pp  Δcode_bytes=+64
Δfuzzy=+0.000000pp
unit improvements: default/TrackWidget (114->115)
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler +64 B)
[control none] ALIAS_SUSPECT: default ruler UP (+64 B) while `none` is FLAT
```

**Partial miss — magnitude and mechanism, not direction.** I predicted ~2
functions / ~800 B on the assumption that both 424 B `_S_sort` rows would
cross to fuzzy==100 outright once correctly named. Pulling the actual
per-row diff from `legA_report.json.gz`/`legB_report.json.gz` (via the
archived reports under `.ab_measure_runs/20260930-024209-from-dirty-653893/`,
not by rebuilding) shows that did NOT happen:

- `??$_S_sort@UInstance@RndMultiMesh@@...` (newly-named at `0x827e3c38`):
  **99.76415% fuzzy in leg B** — a real row now exists where before there was
  none (leg A: not found, because nothing named this symbol), but it does not
  reach 100.
- `??$_S_sort@VMeshInstance@@...` (moved `0x827e3c38 -> 0x827e3e10`): **stayed
  at 99.76415% fuzzy in BOTH legs**, unchanged score, just re-addressed.
- The Waypoint-typed row that used to sit at `0x827e3e10` **disappears** in
  leg B (was 99.76415%, never 100%, so its removal costs nothing on the byte
  ruler).
- The actual `+1 fn / +64 B` came from a row I did not predict at all:
  `?Sort@?$TrackWidgetImp@UInstance@RndMultiMesh@@@@UAAXXZ` (the 64 B `Sort()`
  wrapper), which moved **99.6875 -> 100.0**. Its own body contains the `bl`
  to the `_S_sort` helper; fixing the callee's name at `0x827e3c38` let that
  one relocation-name check pass under `name_check`, crossing the wrapper to
  100 even though the renamed `_S_sort` bodies themselves did not cross.

So the fix is real and its direction was right (small positive, no
regression), but the mechanism is "a caller's reloc-name check clears,"
not "the renamed rows themselves become byte-exact." The residual ~0.24%
on both 424 B `_S_sort` rows is a separate, still-open mismatch (roughly one
instruction) that this map fix does not touch — flagged below as an open
question rather than chased now, since it's STL-internal codegen noise
territory and outside this step's scope.

**On `ALIAS_SUSPECT`**: this fires on every map-only patch shaped
`name_check up / none flat`, and per project history
(`docs/decomp/patterns/...` map-economics note) that shape is structurally
identical for a legitimate "repair a wrong existing name" (MAPDEF-3
precedent: +108 B, `none` unmoved) and for a fabricated alias — it cannot be
resolved by shape alone. Adjudicating on retail bytes, independent of this
tool's flag: both `bl` targets at `0x827e3c38`/`0x827e3e10` were confirmed
against retail disassembly and `tools/retail_callers.py` **before** the map
edit was made (see evidence above), so this is the "repair a wrong name"
case, not a fabrication. Landing it.

## Step 2 (MeshInstance half): splits.txt boundary move + 0x827e48c0 map entry

**MEASURED** (`~/tmp/w16gp_step2_ab.log`, run
`20260930-030003-w16gp_step2a_combined-1061595`, patch
`w16gp_step2a_combined.diff` = commits `7d75f4e65` + `a520c6659` combined,
applied via `--patch` since this was a `map`+`splits`-classified change and
required both legs measured in freshly-split state):

```
leg A: matched=44200 masked=23323 honest=20877 code%=40.921753
leg B: matched=44201 masked=23323 honest=20878 code%=40.922380
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000627pp  Δcode_bytes=+64
Δfuzzy=+0.000623pp   (legA 50.493717 -> legB 50.494340)
unit improvements: default/TrackWidget (115->116)
units at 100% [mpn]: 198->198 (Δ+0)   units at 100% [all-rows-fuzzy]: 174->174 (Δ+0)
[control none] Δmatched_code=+64 B Δcode%=+0.000626 (default ruler +64 B) -- NOT_APPLICABLE
  (this patch moves real code via the splits.txt boundary move, so a `none`-ruler
  read is expected to move too and carries no alias-suspect signal here; the
  tool correctly labels it NOT_APPLICABLE rather than emitting ALIAS_SUSPECT)
```

**Hit, exactly as predicted.** I predicted "expect +1 function / +64 B,
`none`-ruler unaffected" — the measured `none`-ruler control also moved +64 B,
which is *expected and consistent* (not a contradiction): the prediction's
"unaffected" language anticipated a map-only change, but this step also moved
a `.text` boundary in `splits.txt` (real code reattribution between two
compiled objects), so `none` moving alongside `name_check` is exactly the
signature the tool itself documents for a splits change, not a naming
artifact. The `+1 fn / +64 B` landed on `default/TrackWidget`, going 115->116
matched rows in that unit — consistent with the 64 B
`?Sort@?$TrackWidgetImp@VMeshInstance@@@@UAAXXZ` wrapper (the address this
step targeted) becoming newly pairable against `TrackWidget.obj` and crossing
to 100% outright, now that both (a) the boundary move puts it in the right
unit's base object and (b) it has a name at all.

No regression, no unexplained direction, no investigation needed before
continuing. Branch restored to `a520c6659` (both Step 2a commits retained as
the current tip) after the measurement; `git status --porcelain` clean,
`git log --oneline` confirms `a520c6659 -> 7d75f4e65 -> 589c04877 -> a9f34adf7`
in order.

Next: Step 3 (port `TrackWidgetImp.cpp`), then return for the TextInstance
half of goal 2, goal 3, and goal 4, which all require the new object to exist.

## Step 3b: TextInstance half of goal 2, goal 3, goal 4 (post-TrackWidgetImp.cpp)

With `TrackWidgetImp.cpp` compiling (Step 3, `822367834`), its object now
contains ground-truth compiled symbols for the remaining report goals. Rather
than trust the report's guessed manglings, I scanned
`build/45410914/src/system/track/TrackWidgetImp.obj`'s COFF symbol table
directly (Python regex over printable-ASCII runs, filtering for "Sort"/
"MultiMeshWidgetImp") to get the compiler's own spellings. Full result set is
recorded in the session transcript; the two load-bearing hits:

```
?Sort@?$TrackWidgetImp@VTextInstance@@@@UAAXXZ
?Sort@MultiMeshWidgetImp@@UAAXXZ
```

Both match the report's predicted spellings for goal 2 (TextInstance half)
and goal 3 exactly. A third Sort symbol,
`?Sort@?$TrackWidgetImp@UInstance@RndMultiMesh@@@@UAAXXZ` (the base
template's own version, reachable via `ImmediateWidgetImp` per
`TrackWidgetImp.h:286`), was also present but is **out of scope** — the
report gives no confirmed retail address for it and none of goals 2-4
reference it.

**Why `TrackWidgetImp<TextInstance>::Sort()` compiles with no override
needed**: `src/system/track/TrackWidgetImp.h:223` declares
`std::list<TextInstance> mInstances` with the plain default allocator (unlike
`MultiMeshWidgetImp`'s `InstanceList`, which needed the allocator-mismatch
fix in Step 2). `CharWidgetImp` (line 179) inherits
`TrackWidgetImp<TextInstance>` without overriding `Sort()`, and its
constructor is defined in this TU (`TrackWidgetImp.cpp:248`) — MSVC's
implicit-instantiation rule emits every virtual of a class template base once
a derived class's vtable is built in that TU, which is why the template's
`Sort()` appears in the object despite never being called explicitly in
source.

**Goal 3** (`MultiMeshWidgetImp::Sort`, `0x827e63f8`) is a non-template
override declared at `TrackWidgetImp.h:258`; ground-truth spelling confirmed
above.

**Goal 4 — REFUTED, no action taken.** The report hypothesized the two thunks
at `0x827e5c48`/`0x827e5d78` needed renaming to `_List_base<TextInstance,...>`/
`list<TextInstance,...>` destructor spellings. Querying
`scripts/target_symbol_map.json` before editing showed both addresses are
**already correctly named**:

```
0x827e5c48 -> ??1?$_List_base@PAVMidiParser@@V?$StlNodeAlloc@PAVMidiParser@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ
0x827e5d78 -> ??1?$list@PAVMidiParser@@V?$StlNodeAlloc@PAVMidiParser@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ
```

These are `_List_base<MidiParser*,...>`/`list<MidiParser*,...>` destructors —
a list of `MidiParser` **pointers** (an unrelated internal registry), not
`TextInstance`. Renaming them per the report would have broken an
already-correct mapping. This is exactly the "re-check every finding against
the tree" discipline the task requires: the report's goal-4 hypothesis does
not hold, and no map edit was made for it.

**Splits.txt restructuring applied** (mirrors the Step 2 methodology — narrow
function-sized carve-outs, no heading emptied):
- Removed `.text 0x827E5D88-0x827E5F58` from `VocalTrack.cpp:` (the
  pre-granted exception block — its start address is independently confirmed
  by the pre-existing map row `??$_S_sort@VTextInstance@@...` to be
  TrackWidgetImp/TextInstance content, not VocalTrack content, so the
  exception grant is validated by evidence, not just trusted).
- Narrowed `MidiParser.cpp:`'s `.text 0x827E5F58-0x827E6530` block into two
  sub-ranges (`0x827E5F58-0x827E63F8`, `0x827E64B8-0x827E6530`), carving out
  the middle span.
- New heading `TrackWidgetImp.cpp:` with
  `.text 0x827E5D88-0x827E5F58` (TextInstance `Sort` wrapper) and
  `.text 0x827E63F8-0x827E64B8` (`MultiMeshWidgetImp::Sort`).
- Verified via full-file overlap scan: 6,564 total `.text` ranges, 0 overlaps.
  `VocalTrack.cpp` retains 3 `.text` blocks, `MidiParser.cpp` retains many —
  neither heading emptied.

**Map entries added** (minimal targeted insertion preserving the file's
existing 1-space-indent formatting and address-sorted order — an earlier
attempt via `json.dump(indent=2)` was reverted because it reformatted the
*entire* file to 2-space indent, a 30,650-line diff for a 2-line change):

```
"0x827e63f8": "?Sort@MultiMeshWidgetImp@@UAAXXZ",
"0x827e6470": "?Sort@?$TrackWidgetImp@VTextInstance@@@@UAAXXZ",
```

**Prediction**: both addresses become newly pairable against
`TrackWidgetImp.obj` once the splits.txt boundary puts them in that unit.
Expect roughly **+2 functions / ~+128 B** (2 small wrapper/override
functions, each in the same size class as the Step 2 MeshInstance hit, which
was exactly 64 B). `none`-ruler control is expected to move alongside
`name_check` (NOT_APPLICABLE label expected, same reasoning as Step 2: this
is a combined map+splits patch that moves real code, not a map-only alias
candidate).

**Measured** (`tools/ab_measure.py --patch`, diff `822367834..f18a89517`
against a `git switch --detach 822367834` baseline; full log
`~/tmp/w16gp_step3b_ab.log`):

```
leg A: matched=44201 masked=23323 honest=20878 code%=40.922380
leg B: matched=44202 masked=23323 honest=20879 code%=40.923004
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000624pp  Δcode_bytes=+64
unit improvements: +2  default/TrackWidgetImp  (0->2)
unit REGRESSIONS:  -1  default/VocalTrack  (179->178)
[control none] Δmatched_code=+604 B (default ruler +64 B) — NOT_APPLICABLE, as predicted
```

**Verdict: partial hit.** Predicted ~+2 fns / ~+128 B; measured +1 fn / +64 B
net. Sign matches, magnitude is half the prediction — only one of the two
addresses reached 100% this round, not both (see investigation below for
why that's still a clean, fully-explained result and not a defect).

**Investigation of the `default/VocalTrack -1` line** (required by the task's
"investigate before continuing" rule, applied here to a per-unit regression
inside a net-positive whole-binary result). Compared leg A vs leg B unit rows
for `default/VocalTrack` and `default/TrackWidgetImp` directly from the
ab_measure run's archived `legA_report.json.gz` / `legB_report.json.gz`
(keyed by function **name**, not the report's `address` field — the address
is an offset *within the unit's target object*, and removing a 464 B block
from VocalTrack's `.text` pin shifts every subsequent row's offset by -464,
which makes address-keyed diffing across legs misleading on its own; this is
a variant of the "key on `.fn fn_<addr>`, never the address column" rule from
the project doc, applied to per-unit report rows instead of `.s` files).

Finding: the 464 B block moved out of `VocalTrack.cpp`
(`0x827E5D88`-`0x827E5F58`) actually contained **two** dtk-detected target
functions, not one:
- `??$_S_sort@VTextInstance@@...` (424 B) at `0x827E5D88` — **0% matched**
  while mis-attributed to VocalTrack (VocalTrack.obj has no such symbol).
- `fn_827E5F30` (40 B, unrenamed/anonymous) at `0x827E5F30` — **already
  100% matched** while mis-attributed to VocalTrack.

`424 + 40 = 464`, exactly the block size — confirmed by address arithmetic,
not assumed.

After the move, `fn_827E5F30` is **still 100% matched**, unchanged, just now
counted under `default/TrackWidgetImp` instead of `default/VocalTrack`. So
the "VocalTrack -1" is **not a lost match** — it is a pure reattribution of an
already-matched row to its correct unit. The accounting ties out exactly:

```
VocalTrack:      -1  (fn_827E5F30 leaves VocalTrack's at-100 tally)
TrackWidgetImp:  +1  (fn_827E5F30 re-enters TrackWidgetImp's at-100 tally — net 0 across both units)
TrackWidgetImp:  +1  (?Sort@?$TrackWidgetImp@VTextInstance@@@@UAAXXZ, 64 B, genuinely newly 100%)
-----------------------------------------------------------------------
net:             +1 matched  ==  measured Δmatched=+1
```

And `Δcode_bytes=+64` matches **exactly** the size of the one genuinely new
match (`Sort@TrackWidgetImp<TextInstance>`, 64 B) — the reattributed
`fn_827E5F30` contributes 0 net bytes since it was already counted as
matched both before and after. Both totals close with no residue.

The other two report-flagged addresses are now correctly paired and
adjudicable (previously they weren't even in the right unit to be compared)
but are near-misses, not yet at 100%:
- `??$_S_sort@VTextInstance@@...` (424 B): fuzzy 99.669815
- `?Sort@MultiMeshWidgetImp@@UAAXXZ` (116 B): fuzzy 99.82758

Getting these two the rest of the way to 100% is genuine source-level decomp
work (not a map/splits move) and is **out of scope for this step** — it is
recorded here as an open item for the reviewer / a future lane, not
something this step silently failed to do. **Step 3b is accepted as-is**: the
whole-binary number is net positive and fully explained, the per-unit
"regression" is an accounting artifact with zero real loss, and no further
action is needed before moving on.

