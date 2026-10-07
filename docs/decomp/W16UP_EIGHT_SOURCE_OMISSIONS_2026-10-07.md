# W16-UP: the eight addresses W16-UN left to source (2026-10-07)

Branch `w16-up`, worktree `~/tmp/wt-w16up`, base `c200efb55` (W16-UN merged).

W16-UN (`W16UN_MAP_DEFECTS_AND_PIN_MOVES_2026-10-07.md`) left eight addresses that
a pin or a name alone could not fix: `0x822DC828` (`VertVector::clear`, our
`Mesh.obj` inlines `resize` into it) and seven whose containing unit's obj
compiles no spelling of the retail body. All eight now pair at 100.

## Result

Every wave was priced with `tools/ab_measure.py --from-dirty` in this worktree,
ruler `name_check`, one change per run.

| wave | commit | addresses | change | Δfns | ΔB |
|---|---|---|---|---:|---:|
| 1 | `a9cc33046` | `0x82667FC8` | `OvershellProfileProvider::GetUser` out of line in its `.cpp`; pin to that unit; name | +1 | +16 |
| 2 | `0556a14b1` | `0x82272308`, `0x82272548`, `0x82272B90` | pin to `UI` / `PanelDir` / `UIScreen`; name; fix `SetlistToStorePanel::Poll`'s callee | +3 | +472 |
| 3 | `54d2b096a` | `0x824417D8` (+ `0x82441608`) | stand-in in `rndobj/Utl.cpp`; name both | +2 | +192 |
| 4 | `64f62ce23` | `0x825122B0` | stand-in in `os/Archive.cpp`; name; admit the `int` spelling as a fold | +1 | +104 |
| 5 | `e47fa1f1b` | `0x822B0D48` | stand-in in `bandobj/BandCamShot.cpp`; name | +1 | +112 |
| 6 | `4d4fcb7f1` | `0x822DC828` | `BuildChordMesh` spells `Verts().clear()` | +1 | +8 |
| – | `22f7bb6ce` | – | comment correction in the three stand-ins | 0 | 0 |

Total: **+9 functions / +904 B**, `matched_functions` 55,027 → 55,036,
`matched_code_percent` 59.390015 → 59.398834, units at 100% (mpn) 629 → 633:
`CharProvider`, `MessageTimer` and `CalibrationPanel` because rows left their
denominators, `Archive` because its matched count rose. No row in any landed
wave lost bytes (row diffs of each leg-A/leg-B `report.json`).

Native gate after the last commit:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## Per address

### `0x82667FC8` GetUser (wave 1): a real source fix

The 16-byte `return mUsers[i]` sits directly before `OvershellProfileProvider`'s
first function (`Handle` at `0x82667FE0`, whose 8-byte EH prefix is
`0x82667FD8`). Our header defined `GetUser` inline (`__declspec(noinline)`), so
only `OvershellSlot.obj` emitted it. It is now defined in
`OvershellProfileProvider.cpp`; `[0x82667FC8,0x82667FE0)` moved from
`CharProvider` to that unit; the map's `null` row became the name. The alias group
at the address (survivor `fn_82667FC8`, fold `IxToLeaderboardIx`) was relabelled
with `alias_survivor_relabel.py --rechase-key rechase_w16up`; the fold
re-chased PROVEN.

### The pre-main template run (wave 2): pins, no new source

`0x82272308` `Find<UIScreen>`, `0x82272548` `PropSync<UIPanel*>` and
`0x82272B90` `~UIScreen` lie in `0x82271800`–`0x82272E3C`, where W16-TU already
recorded that the unit labels are pairing choices, not ownership. None of the
units pinned in that run compiles any of the three. Each moved (with its EH
prefix and unwind funclet) to the unit DC3's `ham_xbox_r.map` gives that COMDAT
to (`ui:UI.obj`, `ui:PanelDir.obj`, `ui:UIScreen.obj`), whose obj compiles a body
that chases PROVEN at the address. The `~UIScreen` funclet (`0x82272BD8`) went
from 99.5 in `CalibrationPanel` to 100 in `UIScreen`.

The pins alone measured **+2 fns / −724 B**: naming `0x82272308` charged
`SetlistToStorePanel::Poll` (1,196 B), which called a decl-only
`FindStoreScreen(gStoreScreenCfg, ...)` on a made-up global. Retail loads the
same global into `r3` as the other 15 `Find<UIScreen>` sites, and those are
`ObjectDir::Main()` in our source, so `Poll` now calls
`ObjectDir::Main()->Find<UIScreen>(...)`. With that fix the wave measured
+3 / +472 B and `Poll` stayed at 100. This is the "naming exposes a wrong
callee" pattern again; the fix is behavioural, not cosmetic.

### `0x822DC828` VertVector::clear (wave 6): a source spelling

W16-UN found that moving the row into `Mesh` reads 0 because our `Mesh.obj`
inlines `resize` into an 80-byte `clear`. The retail copy (`li r4,0; b resize`)
is the first function of `ChordShapeGenerator`'s `.text`, and that TU does not
define `resize`. Changing `BuildChordMesh`'s first vertex reset from
`Verts().resize(0)` to `Verts().clear()` makes `ChordShapeGenerator.obj` emit
the 8-byte COMDAT; the call site still inlines to the same `li r4,0` / `bl
resize`, and `BuildChordMesh`'s row did not move.

★ **Finding: MSVC emits a used inline function's COMDAT even when every call to
it is inlined.** Measured here: the only call site inlines `clear`, and the obj
still carries the 8-byte `clear`. So "retail emits a COMDAT in unit U but no
function in U calls it" does **not** imply the referencing code was discarded by
`/OPT:REF`; it may be live code that inlined the call. An orphan stand-in
calling `clear()` was tried first and produced the same COMDAT; the source
spelling was kept instead.

### Three stand-ins (waves 3, 4, 5)

For these the referencing code is not identifiable, so each unit gets an
external, `HX_NATIVE`-excluded stand-in that only instantiates the template.
This is dc3-decomp's w8-a pattern in its own `rndobj/Utl.cpp`, and
`BandCamShot.cpp` already did the same for its `list<Target>` stream operator.
Each comment says the code is a stand-in and why.

| address | unit | evidence that the unit emits it | other callers (link later) |
|---|---|---|---|
| `0x824417D8` `vector<Key<TexPtr>>::_M_erase` | `rndobj/Utl` | DC3's map attributes the same COMDAT to `rndobj:Utl.obj`, also with no in-unit caller | `MatAnim`'s `resize<Key<TexPtr>>` |
| `0x825122B0` `MakeString<5 args>` | `Archive` | between `Archive::GetGuid` and Archive's FileEntry sort COMDATs | `MemTracker`, `PoolAlloc` |
| `0x822B0D48` `ObjPtr<EventTrigger>(Object*, EventTrigger*)` | `BandCamShot` | between BandCamShot's `ObjPtr<ObjectDir>` and `ObjPtr<EventTrigger>` vtable members | `BandCrowdMeter`, `BandStarDisplay` |

- Utl's stand-in also emits `_Destroy_Range<Key<TexPtr>>`, which chases PROVEN at
  `0x82441608` (unnamed before); named too. `__destroy_range<Key<TexPtr>>` also
  chases there but has 0 call sites in our objs, so no alias was added.
- Archive: `MakeString<const char* x5>` and `<int x5>` are byte-identical in
  retail (both `FormatString::operator<<` overloads fold at `0x827C40E8`). The
  `const char*` spelling (2 call sites) is the survivor and the `int` one
  (1 site) is admitted as a fold. Then `alias_callee_name_drift.py --reprove
  --write`.
- BandCamShot: our out-of-line body under the TU's `RB3_TU_OBJPTR_DEFER_OWNER`
  policy is byte-identical to `BandCrowdMeter.obj`'s. The likeliest real
  referencer is `Load`'s two owner-only trigger `ObjPtr`s, if retail spelled them
  through the two-arg ctor with its default `nullptr` and inlined it. This TU
  spells them through the one-arg ctor to get `Load`'s store order, so that was
  not tried.

## Deliberately not done

- No stand-in was replaced by a guessed "real" referencer. In particular, the
  `BandCamShot::Load` hypothesis above would touch the per-TU `ObjPtr` ctor
  policy that `Load`'s match depends on.
- `0x82BB33D8` / `0x82BB3430` / `0x82BB3510` (W16-UN's probable VorbisReader
  rows) were not touched; they are paired today, so moving them is a re-homing.
- Merging and pushing (coordinator).

## Hazards met

- A map rename at an address that already has an alias group fails the build at
  `CHECK ALIAS SURVIVORS VS MAP` (wave 1) or `CHECK ALIAS CALLEE NAMES VS MAP`
  (wave 4) until `alias_survivor_relabel.py --write` /
  `alias_callee_name_drift.py --reprove --write` are run. `ab_measure` refuses
  that leg, as it should.
- Wave 2's leg B re-derived five `.pdata` lines in `splits.txt`; they are in the
  wave's commit (`legB_splits_rederived.diff`).

## Tool use

`tools/name_proven_placeholders.py map|alias` with a hand-written ledger
(`~/tmp/w16up/ledger.json`), W16-UN's `move_text.py` for the `.text` moves, and
`~/tmp/w16un/locate.py` to confirm that each compiled body chases PROVEN at its
address before naming. A/B run dirs are under `~/tmp/w16up/ab/`.
