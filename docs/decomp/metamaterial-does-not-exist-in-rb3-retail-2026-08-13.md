# `MetaMaterial` does not exist in RB3 retail — the class and all its machinery are DC3-era

Lane METAMAT-1, 2026-08-13. Closes the material-hierarchy thread opened by MAT-1
(`32f4fdb1`), BASEMAT-1 and BASEMAT-2 (`ddfe1602`).

**VERDICT: RB3-360 retail contains no `MetaMaterial` class, no metamaterial
`ObjectDir`, no `<prop>_edit_action` property system, and no code path that could
create or load one. The class, the `MatProp` / `MatPropEditAction` enums, the
static machinery (`sMetaMaterials`, `LoadMetaMaterials`, `CreateMetaMaterial`,
`OnGetMetaMaterials`, …) and the `IsEditable` gate are all REMOVED.**

The surviving `rndobj/MetaMaterial.{h,cpp}` are **unit-boundary artifacts only** —
the same disposition BASEMAT-2 gave `BaseMaterial.cpp`. `MetaMaterial.cpp` now
holds the **one** material `SyncProperty`, under `RndMat`.

---

## Why RTTI absence was NOT sufficient here, and what actually discriminated

`MetaMaterial: no COL found` is **not** a proof. Two documented escape hatches
apply:

1. This repo's own finding that RTTI type-name strings are not a `/GR` test (EH
   emits `??_R0` too), and that an abstract or never-instantiated class
   legitimately lacks a `??_R4` Complete Object Locator.
2. **The `??_R2` base-class-array instrument that settled `BaseMaterial` DOES NOT
   APPLY HERE.** That instrument works by reading a *derived* class's array,
   where a base must appear. `MetaMaterial` is not a base of anything — in DC3 it
   is a **sibling** of `RndMat` (both `: BaseMaterial`), and in our tree it was a
   **leaf child** of `RndMat`. Either way nothing's `??_R2` can constrain it.
   Retail's `RndMat` array (`numBaseClasses=3`, `ncb` chain 2,1,0 gapless) says
   nothing about a class *below* `RndMat`.

So a different instrument was required.

### The instrument: the string contract of the machinery itself

Every piece of the metamaterial system requires a **string literal that is passed
to a runtime function and therefore cannot be inlined away, folded, or
synthesised**:

| required literal | why it must exist | retail count |
|---|---|---|
| `metamaterial_path` | `cfg->FindData("metamaterial_path", …)` in `LoadMetaMaterials` | **0** |
| `metamaterials.milo` | `DirLoader::LoadObjects("metamaterials.milo", …)` | **0** |
| `MetaMaterial` | `OBJ_CLASSNAME` / `REGISTER_OBJ_FACTORY` / `ClassExt("MetaMaterial")` — any Milo object type loadable from data must intern its class name | **0** |
| `metamaterial` | `static Symbol metamaterial("metamaterial")` in `CreateMetaMaterial` | **0** |
| `{anon}` / `{milo}` | `kAnonMetaMatPrefix` / `kMiloMetaMatPrefix` | **0** / **0** |
| `_edit_action` | every `SYNC_MAT_PROP` builds `Symbol("<prop>_edit_action")` | **0** |
| `shader_combos` | `MetaMaterial`'s own propsync entry | **0** |

Plus a case-insensitive sweep of the whole 14,363,648-byte image: `metamat` **0**,
`meta_mat` **0**, `meta mat` **0**.

★ **The screen can fail, and its resolution is one occurrence.** 15/15 positive
controls fired, drawn deliberately from the *same* functions and the *same*
propsync list, most of them singletons:

```
allowed_next_pass 1 · allowed_normal_map 1 · next_pass 2 · alpha_threshold 1
shader_variation 1 · refract_strength 1 · norm_detail_tiling 1
environ_map_specmask 1 · rim_light_under 1 · emissive_multiplier 1
objects 31 · Mat 51 · RndMat 7 · NgMat 1 · DxMat 1
```

⇒ a screen that resolves `rim_light_under` at 1 occurrence, in the very TU under
test, returning 0 for `metamaterial_path` is a **decisive negative**, not a
tooling artifact.

★ **This also forecloses the "retail called it something else" escape hatch**,
which a class-name search alone cannot. `metamaterial_path` is a **DTA config
key** and `metamaterials.milo` a **filename** — data-contract strings that survive
any C++ identifier rename. Both are 0.

All binary scanning was done in **Python**, never `grep` (binary-blind here —
false negatives shaped exactly like decisive ones, the worst failure mode for an
absence question). `tools/retail_rtti.py --selftest` was run first and reported
**8/8**; its `--sabotage naive-va` leg drops to **2/8**, so the RTTI half is
demonstrably falsifiable too.

### Corroboration (each weaker alone; all agree)

| # | evidence | value |
|---|---|---|
| 1 | string contract above | **DECISIVE** |
| 2 | **rb3-Wii — the RB3-*era* oracle — has `class RndMat : public Hmx::Object` and `MetaMaterial` **0 times tree-wide**; DC3 (newer) has `MetaMaterial.{h,cpp}` | strong, and *structural*: rb3-Wii's `BEGIN_PROPSYNCS(RndMat)` contains **zero** `IsEditable` calls and **zero** `_edit_action` symbols — plain `static Symbol _s("prop")` blocks with `mDirty \|= N`, exactly the shape `_edit_action`=0 predicts |
| 3 | no `??_R4` COL for `MetaMaterial` | corroboration only, per above |
| 4 | `target_symbol_map.json` carries **0** `MetaMaterial` rows | our own map never identified one either |

## What was ruled out explicitly

- **RTTI absence alone** — insufficient (above).
- **The `??_R2` array instrument** — structurally inapplicable to a leaf/sibling.
- **`sizeof`** — cannot discriminate; `MetaMaterial` adds one `std::vector`, and
  no retail allocation is attributable to it in the first place.
- **Unit names.** `splits.txt` / `objects.json` still say `MetaMaterial.cpp`.
  A unit name is not evidence a class exists, and this one demonstrably names a
  span of retail `RndMat` code.
- **A high match%.** `default/MetaMaterial` reported **53/67 matched** before this
  lane. That is *not* evidence the pin or the class attribution is right: **all 53
  were `masked_equal` byte-signature disclosures** (honest = **0**), all 40-/32-byte
  static-`Symbol` initializers, and `matched_code` was **1,696 / 7,828**. The two
  real bodies — `fn_82436488` (4,808 B) and `fn_82437E58` (464 B) — sat at **0%**.

## What the pin covers — and a prediction of mine that was REFUTED

I first moved `RndMat::SyncProperty` **into** `MetaMaterial.cpp`, reasoning from the
pin shape: `splits.txt` pinned that unit to `.text 0x82436488-0x82438138`, which is
retail's one material `SyncProperty`, so the propsync "belonged" there.

**That was wrong, and lane SPLITS-3 (`7782ed48`) refuted it with better evidence
while this lane was in flight.** It moved the span the *other* way — out of
`MetaMaterial.cpp` and into `Mat.cpp` — on:

- a whole-tree **defining-set census**: `?SyncProperty@RndMat@@` is defined by
  `{BaseMaterial.obj, Mat.obj}`; **`MetaMaterial.obj` is ABSENT**, so that pin could
  never have matched under any amount of source work; and
- **vtable-slot arithmetic**, controlled on two independently-known slots (`Handle`
  → `0x82438138`, already 100%; `ClassName` → `0x82438830`, already map-named),
  which then lands slot 8 `SyncProperty` on `0x82436488`.

A defining-set census beats reasoning from pin shape. The move was reverted: the
propsync stays in **`Mat.cpp`**, where the `SYNC_PROP` macros that build it live, and
`MetaMaterial.cpp` is left as a doc-only stub.

⚠ **Two spans pinned to `MetaMaterial.cpp` are now ORPHANED** (flagged for SPLITS-3,
not fixed here — that region is theirs):
`0x824382D4-0x8243833C` (104 B, two funclets left behind by `7782ed48`) and
`0x82578A90-0x82578C1C` (396 B, whose counterpart was
`vector<MatPropEditAction>::_M_insert_overflow_aux` — an instantiation that existed
only for `mMatPropEditActions`, so with the enum gone it can no longer pair from
here; it read 96.34 fuzzy, never 100, so no `matched_code` is lost by that row).

## The sibling→child correction

dc3-decomp has `MetaMaterial : BaseMaterial`, a **sibling** of
`RndMat : BaseMaterial` (verified by reading dc3's headers; lane ENGINE-1). Ours
was the same until BASEMAT-2 (`9ea37046`) merged `BaseMaterial` into `RndMat`,
which mechanically re-parented `MetaMaterial` **sibling → child**.

That is one hop of our own history, not an independent divergence — but it has a
real consequence: as a child with `SYNC_SUPERCLASS(RndMat)`, our
`MetaMaterial::SyncProperty` chained the **entire material property list a second
time** on every instance. A shape neither retail nor DC3 has. Removing it removes
that duplication.

⇒ What was removed is a **leaf child**, confirmed by the compiler on both builds.
**This says nothing about `../milo-native-engine`, which keeps `BaseMaterial` and
was not touched.**

## Measured result (whole-binary A/B, `tools/ab_measure.py --from-dirty`)

Re-measured against main **after** SPLITS-3 landed (`234e46df`); the earlier
pre-SPLITS-3 reading is discarded, because composing deltas across a change to the
very pin this lane touches would be invalid. Ruler read from `report.json`
`provenance`: **`functionRelocDiffs=name_check`** (the shipped default), objdiff-cli
sha256 verified stable across both legs.

| | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 44,278 | 44,273 | **−5** |
| `masked_equal_functions` | 22,894 | 22,889 | **−5** |
| **honest** (`matched − masked_equal`) | 21,384 | 21,384 | **0** |
| `matched_code_percent` | 34.456192 | 34.452625 | −0.003567pp |
| `matched_code` bytes | — | — | **−368** |
| `fuzzy_match_percent` | 48.379547 | 48.382730 | **+0.003183pp** |

- units at 100%: **252 → 252** (mpn), **119 → 119** (fuzzy) — **0 reached, 0 fell
  off**, on either ruler.
- per-unit: `default/Mat` −3 (63→60), `default/MetaMaterial` −2 (2→0).

### ★ The number that actually matters: the real body improved by 18 points

`matched_code` fell 368 B, and that is not hand-waved away. But **Δmatched equals
Δmasked_equal exactly (−5 = −5), so Δhonest is 0** — every lost byte is in the
*disclosure* channel, the same channel SPLITS-3's own `+7` was in (its message: "any
one quoting this as '+7 functions' without that caveat is quoting the disclosure
channel"). The symmetry cuts both ways.

What the lost rows are: `fn_824380F0` (64 B, masked), `fn_824382D4` / `fn_8243831C`
(32 B each, masked) drop 100→0; `fn_82438050` / `fn_82438078` (40 B each, masked)
slip 100→99.4 / 99.9 and so leave `matched_code`, which is all-or-nothing per row.
All are 32-/40-/64-byte EH funclets re-shuffling as the body's shape changes.

Meanwhile, adjudicated by objdiff against retail bytes — **not** by the string screen:

| row | leg A | leg B |
|---|---|---|
| `?SyncProperty@RndMat@@` (**4,808 B** — retail's one material SyncProperty) | fuzzy **79.828** / mpn **80.864** | fuzzy **98.186** / mpn **99.118** |

**+18.36 pp fuzzy, +18.25 pp mpn on the largest body in the region.** That is
independent confirmation that removing the `<prop>_edit_action` Symbols and the
`IsEditable` gate is *correct*, and it is the reason whole-binary fuzzy rose while
the funclet count fell. Under the standing directive that accuracy beats headline %
— and that a metric hiding a real defect is worse than a lower metric — this lands.

★ **Follow-up lead, now the best in the region:** `?SyncProperty@RndMat@@` sits at
**mpn 99.118 on 4,808 B**, i.e. ~0.9 pp from 100. Crossing it is worth **+4,808 B**
of `matched_code` in one row. The residual is very likely the property **order** —
which this lane deliberately did not guess at.

## Coupled files

- **`scripts/target_symbol_map.json` — 0 `MetaMaterial` rows.** The exact coupling
  that cost BASEMAT-2 −248 B is **absent** here. Censused on full mangled forms via
  regex, not substring.
- **`scripts/symbol_aliases.json` — 6 distinct mangled forms, 74 sites.**
  `??1?$ObjRefConcrete@VMetaMaterial@@VObjectDir@@@@UAA@XZ` was a fold-group
  **SURVIVOR** at `0x824359e8`; deleting the class would have stranded the group.
  Re-pointed to `??1?$ObjRefConcrete@VRndFur@@VObjectDir@@@@UAA@XZ` — a member of
  the same group **which `target_symbol_map.json` already names at that exact
  address**. The other 73 were `folded` members and were dropped.
- ⚠ **MY CENSUS HAD A GAP, AND IT IS THE EXACT TRAP THE BRIEF NAMED.** I censused
  the coupled JSONs for the mangled form `MetaMaterial` and got **0** rows in
  `target_symbol_map.json`. That was true and *incomplete*: `0x82578a90` is named
  `?_M_insert_overflow_aux@?$vector@W4MatPropEditAction@@…` — coupled to this
  machinery through the **nested enum** `MatPropEditAction`, which my pattern never
  looked for. "Mangled names nest" is not a slogan; a class-name census misses every
  symbol that only mentions a nested type.
  Consequence, already priced by the A/B: our tree no longer instantiates
  `vector<MatPropEditAction>`, so that row goes 96.34 → 0 fuzzy. It was never at 100,
  so **no `matched_code` and no unit at 100% is lost**. But the row is now a
  **provably wrong name** — `MatPropEditAction` never existed in retail, so retail's
  function at `0x82578a90` is a `vector<T>::_M_insert_overflow_aux` for some *other*
  `T`. Renaming it needs the real `T`; that is a map lane's call and is **flagged,
  not guessed at, here**.
- `config/45410914/scope_map.json` — 67 hits, all `provenance` strings recording
  which file a pin came from. Pins unchanged, so left alone.

## Serialization: the read/write pair stayed symmetric

`MetaMaterial`'s `BEGIN_SAVES` / `BEGIN_LOADS` (`SAVE_REVS(2,0)`) were deleted
**together**, as the two halves of one class that does not exist. `RndMat::Save`
(retail `0x82435dc0`, 988 B, mpn/fuzzy 100) and `RndMat::Load` were **not
touched** on either side.

## Native

`tools/native_build_gate.sh`: **PASS, rc=0, 18/18 targets verified, 0 errors,
0 warnings, 0 SKIPs**, after seeding the cmake cache with all four absolute flags.

★ **The gate earned its keep again — and it proved it can fail on this very
change.** The first run came back **FAIL 16/18** on four
`use of undeclared identifier 'CreateAndSetMetaMat'` errors in
`milo-native-engine/src/platform/Rnd_Wgpu.cpp:131-143` — a call site the X360
build is structurally incapable of seeing. Since the engine is not ours to edit,
`CreateAndSetMetaMat` survives as a **`#ifdef HX_NATIVE` no-op shim** in
`rndobj/Utl.{h,cpp}`. The X360 cflags define no `HX_NATIVE` (they carry exactly
`/DCURL_STATICLIB` and `/D_XBOX360` — a small correction to CLAUDE.md's "no `/D`
at all"), so the match build sees a **pure removal**, verified: `matched=44274 /
masked=22891 / code%=34.452663` after the shim, bit-identical to leg B.

### ENGINE CHANGE REQUEST (text, for the coordinator — not landed here)

> `milo-native-engine/src/platform/Rnd_Wgpu.cpp:131,132,137,143` call
> `CreateAndSetMetaMat(...)` on `mWorkMat` / `mPostProcMat` /
> `mDrawHighlightMat` / `mDrawRectMat`. RB3 retail has no metamaterial system, and
> on RB3 data `LoadMetaMaterials()` returns NULL, so these four calls are no-ops
> at best. Deleting them lets rb3-xenon drop the `HX_NATIVE` shim in
> `rndobj/Utl.{h,cpp}` entirely. DC3 keeps `BaseMaterial`, so nothing else about
> the engine's material hierarchy should change.

## Deliberately NOT done

- **`ShaderMgr::mShowMetaMatErrors` (offset `0x6f`) was left in place.** It gates
  only the metamaterial error path (now deleted), but it is a **layout member of a
  different class** and this lane has no retail evidence about `ShaderMgr`'s
  layout. Removing it would shift offsets on nothing but a guess.
- **`fn_82436488` was not reconstructed** and no map row was added for it —
  `splits.txt` in `0x82435520–0x82439000` is lane SPLITS-3's, and the property
  order needs its own retail-byte adjudication.
- **`splits.txt` / `objects.json` were not touched.** `MetaMaterial.cpp` remains a
  pinned unit; draining it would have required deleting its `splits.txt` entry in
  SPLITS-3's region, and an emptied unit hard-fails `report.json`.
- **`../milo-native-engine` was not modified.**
