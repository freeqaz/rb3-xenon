# The compiled-but-unpinned unit class is NOT drainable wholesale — 5 rows / 544 B (lane PINSRC-1, 2026-08-14)

**WRONGCALL-4 traced its largest single result (+5,976 B) past the map to a
*pinning* defect and called the class "drainable wholesale at the source rather
than row by row". That hypothesis is REFUTED with a number.** The class is real,
its census is 162 units, and its entire metric-reachable surface is **5 rows /
544 B** — 1.35% of the orphan-pin population it lives inside, and 0.0053% of
`total_code`.

Cite this file before funding a pinning lane on "unpinned units".

## The class

`config/45410914/objects.json` units whose `src_path` exists — so
`tools/project.py` emits a compile edge and a base `.obj` — but which have **no
heading in `config/45410914/splits.txt`**, so no target `.obj` is ever split for
them and they appear in **no objdiff unit at all**. Their retail addresses do not
vanish; they fall inside whatever *other* unit's pin encloses them.

`tools/unpinned_unit_census.py` + `tools/unpinned_unit_quantify.py`. Baseline
`045b393d`, report `matched 44,367 / total_code 10,320,664`, ruler `name_check`.

Self-validation reproduces the known triangulation exactly: **declared 1,434 −
compiled 1,204 = 230** silently-dropped compile edges, and **0** splits headings
resolving to no object.

| | units | owned fns |
|---|---|---|
| **A — SCATTER-INCLUDED** (a *pinned* `.cpp` literally `#include`s this `.cpp`) | 23 | 406 |
| **B — ORPHAN-COMPILED** (nothing includes it; its base obj is consulted by nothing) | 139 | 2,904 |
| **total** | **162** | **3,310** |

★ **"Owned" is load-bearing and had to be measured, not assumed.** Raw *defined*
COFF symbols make this class look like **11,524** functions; almost all of it is
header-inlined template COMDATs every TU emits. Splitting on **COMDAT selection**
— `NO_DUPLICATES` (this `.cpp`'s body) vs `SELECT_ANY` (inline/template) —
gives 3,310. Three orders of magnitude of noise, and every ratio below depends
on having removed it (`tools/coff_owned.py`).

## The two halves are worth different amounts, and the brief's warning was right

**Class A is HEALTHY. The scatter-include is what makes these rows pairable, not
what breaks them.** Of its 406 owned functions, 87 appear as a named retail row
and **83 of those 87 pair** — most at mpn 100, because the includer's base obj
genuinely defines the included `.cpp`'s symbols. Pinning them separately buys
nothing they do not already have.

⇒ **TimeConversion was not representative of its own class.** Its foreign
`Movie` name required a *coincidence* — `StringTable.cpp` scatter-includes
`{GlitchFinder, Locale, Movie, TimeConversion}`, so the map had four symbol sets
to choose from at one address and chose wrong. That is a **map** error the
scatter-include made *possible*; it is not what the scatter-include *does*.

**Class B is near-empty. Of 2,904 owned functions, 5 appear as a named retail
row at all — 0.17%.**

## ⛔ Why class B is empty: most of it is DANCE CENTRAL 3 CODE THAT IS NOT IN ROCK BAND 3

**66 of the 162 units (40.7%), carrying 2,261 of the 3,310 owned functions
(68.3%), are `system/hamobj/` and `system/gesture/`** — HollaBackMinigame,
RhythmBattlePlayer, PoseFatalities, HamWardrobe, GestureMgr, HamSkeletonConverter,
StreamRenderer. They arrived with the dc3-decomp engine import. Between them they
account for **4** named retail rows.

Verified three ways, none of them the metric:

* `system/hamobj/` and `system/gesture/` exist in `../dc3-decomp` and **do not
  exist in `../rb3`** (the rb3-Wii oracle).
* The retail XEX contains **zero** occurrences of `HollaBack`, `RhythmBattle`,
  `PoseFatalities`, `HamWardrobe`, `GestureMgr` or `hamobj` (scanned in Python —
  the agent-shell `grep` is binary-blind and would have returned the same answer
  vacuously).
* The 3 rows that *do* land (`Difficulty.cpp`'s free functions) pair at mpn 100
  against **`band3/game/Defines.cpp`**, which re-defines them — i.e. RB3 ships
  its own copy and the hamobj one is the redundant twin.

⇒ **There is nothing to pin these to.** They are not an identification backlog;
they have no retail counterpart.

## The reachable surface, exactly

`tools/unpinned_provider_intersect.py` intersects PINHOME-1's orphan-pin
population (`orphan_pins.json` — rows whose paired base obj cannot define their
name, so they read 0% *by construction*) with this census:

| | rows | bytes |
|---|---|---|
| orphan pins, all | 286 | 40,240 |
| ⤷ provider is a **compiled-but-unpinned** unit → **fix = add a heading** | **5** | **544** |
| ⤷ provider is another *pinned* unit → fix = move the pin (PINHOME-1's class) | 6 | 104 |
| ⤷ no compiled obj owns the name → absent source / wrong map name | 275 | 39,592 |

**5 rows / 544 B is the whole lever.** Adding a sixth: `?SetPaused@Movie@@` (8 B)
sits in an `auto_*` unit, which has no base obj and is therefore invisible to the
orphan census — 6 rows / 552 B counting it.

## What was fixed: +7 matched / +252 B, and only +1 of it honest

Two TU-boundary overshoots, both pre-registered and measured exactly
(`ab_measure --from-dirty`, both legs at a split fixed point, `renamer_patched=1823`):

```
Δmatched     +7   (44367 -> 44374)      Δmasked_equal +6   ⇒ Δhonest +1
Δcode_bytes +252  (Δcode% +0.002444pp)  Δtotal_code    0
units at 100% +1  (250 -> 251)
```

| edit | |
|---|---|
| `VarTimer.cpp` `0x827D3FB0-0x827D44C4` → ends `0x827D4260` | +6 fns / +192 B |
| **new** `system/utl/SongInfoAudioType.cpp` `0x827D4260-0x827D44C4` | (all six masked) |
| `AccomplishmentDiscSongConditional.cpp` `0x825E9500-0x825E9880` → ends `0x825E9668` | +1 fn / +60 B |
| **new** `band3/meta_band/AccomplishmentLessonSongListConditional.cpp` `0x825E9668-0x825E9880` | (honest) |

⚠ **READ THE SPLIT.** Six of the seven are `SongInfoAudioType`'s unwind funclets
pairing by byte signature, fully disclosed as `masked_equal`. The one honest row
is the `LessonSongListConditional` ctor, **0.000 → 100.000**.

★ **Neither boundary was chosen to move the metric** — that is the MASKED-CLASS
FALSE PAIRING trap this class invites. VarTimer's pin held exactly the six
functions `VarTimer.cpp` **owns**, all six already at mpn 100, ending at
`0x827D4258`; the map already named `0x827D4260` `SymbolToAudioType`, which
`SongInfoAudioType.cpp` owns and `VarTimer.obj` defines neither owned nor shared.
The Accomplishment boundary is corroborated by size parallelism independent of
the map — `{60, 88, 168, 32}` then `{60, 88, 172, 168, 32}`, two sibling
condition classes — and **a 100% body match is evidence a pin is right, because
a wrong pin cannot produce one.**

⚠ `SymbolToAudioType` itself did **not** cross: `0.000 → 2.048`. What changed is
that it is **pairable at all**; it was structurally unmatchable where it sat. Its
420 B is *source* work now, not pinning work.

★ Side-effect the per-unit improvement list is blind to: **`default/VarTimer`
reached 100% by `DENOMINATOR_SHRANK`**, 13 rows → 6, matched 6 → 6. It could
never have completed while it carried another TU's rows.

## Also fixed: DepthBuffer3D's spurious block (Δ0, accuracy)

`DepthBuffer3D.cpp`'s `0x8278EB78-0x8278EBA8` — WRONGCALL-4's second leftover —
was a hole carved out of GameGem's accessor run. Removed; GameGem's two flanking
blocks merged to `0x8278E9A8-0x8278EBB4`. Three non-metric reasons: GameGem
accessors flank it on both sides, `GameGem.obj` **owns**
`?SetImportantStrings@GameGem@@` while `DepthBuffer3D.obj` does not define it at
all, and every one of DepthBuffer3D's other 23 `.text` blocks lies below
`0x82741098` while this one sat **300 kB away** inside beatmatch code.

Pre-registered Δ0 and measured Δ0 (Δmatched/Δmasked/Δhonest/Δbytes all +0, units
at 100% 251 → 251, nothing fell off; Δfuzzy +0.000070pp). `SetImportantStrings`
went **0.000 → 99.500** — one instruction from crossing. That instruction is
`stb r4, 0x40, r3` vs our `stb r4, 0x28, r3`, a **0x18 member-offset delta in
GameGem's layout** — not a pinning defect, deliberately left for whoever owns the
GameGem bitfield work.

## ⛔ Refuted: the Movie rows are a SOURCE divergence, not a pin defect

Four of the six reachable rows are `Movie` methods (`LockThread` 48 B,
`MsPerFrame` 8 B, `NumFrames` 8 B in `default/Splash`; `SetPaused` 8 B in an
`auto_*` unit) whose provider is the unpinned `system/movie/Movie.cpp`.
**Pinning cannot fix them.** A non-metric structural test — our compiled owned
function-size multiset vs the retail anonymous run at `0x82742CA8-0x82744068` —
overlaps **2 of 24**. Our `Movie.cpp` is a pimpl forwarder (`mImpl->X()`, 16 of
its 24 functions are 20 B); retail's `MsPerFrame` and `NumFrames` are **8 B**,
i.e. direct member loads. Retail RB3's `Movie` is not the DC3 pimpl class.

⇒ 72 B of the 552 B "reachable" figure is not reachable by this lane's lever at
all. **The honest realisable total is 480 B, of which 252 B was realised.**

## What the class cannot reach

**3,218 of 3,310 owned functions (97.2%) have no retail row bearing their name.**
Excluding unwind funclets, that is **1,548 functions / ~409,936 B of *our*
compiled bytes** — a proxy and an over-estimate, since 68.3% of the class is the
hamobj/gesture code proven above to have no retail counterpart at all.

Pinning cannot recover any of it: **a pin does not create a name.** Where such a
body exists in retail it is already inside `total_code`, attributed to whatever
unit encloses it, and reading 0% as an anonymous row. Naming it first is
identification work — the class AUTOID-1 measured as ~8.9% attributable-and-
portable — and only then is pinning applicable.

## Side-finding, not chased

**3 splits.txt headings resolve to an object a second heading already claims** —
`band3/meta_band/AccomplishmentProgress.cpp`, `band3/meta_band/UIStats.cpp` and
`band3/game/Game.cpp` each have *both* a path-qualified and a bare heading (1,275
headings → 1,272 distinct objects). Two target objs then pair against one base
obj. Not investigated here; flagged for a pinning lane.

## Not done

* The 275 no-provider orphan pins (39,592 B, 98.4% of the orphan bytes) are
  untouched — absent source or wrong map names, which no pin change reaches.
* `SymbolToAudioType`'s 420 B is now pairable and unmatched; nobody ported it.
* The GameGem `0x40`/`0x28` offset is diagnosed, not fixed.
* The 3 duplicate heading resolutions are reported, not resolved.
* `src/` did not move, so `tools/native_build_gate.sh` was correctly not run.
