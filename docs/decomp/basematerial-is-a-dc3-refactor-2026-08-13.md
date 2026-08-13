# `BaseMaterial` does not exist in RB3 retail — the two-class split is a DC3 refactor

Lane BASEMAT-1, 2026-08-13. Settles the question lane MAT-1 (`32f4fdb1`) flagged
as out of scope: *"retail also lacks `.?AVBaseMaterial@@` RTTI."*

**VERDICT: the `BaseMaterial` / `RndMat` split is a DC3-era refactor that RB3
retail does not have. Retail has ONE material class, `RndMat`, deriving directly
from `Hmx::Object`.** MAT-1's flag is CONFIRMED — but *not* for the reason it
gave, and the reason matters (see "why RTTI absence alone was insufficient").

Nothing was merged. This lane was scoped to settle + census + hand off.

---

## Why RTTI absence alone was NOT sufficient — and what actually discriminated

`CLAUDE.md` records that RTTI **type-name strings are not a `/GR` test** (`/GR-`
drops `??_R4` 44→0 but `??_R0` only 73→40, because EH also emits them), and a
**pure-abstract or never-instantiated base can legitimately lack a `??_R4` COL**
while still existing. Retail has 2,220 COLs; a class never constructed as a
complete object need not appear among them. So `BaseMaterial: no COL found` is
**consistent with both hypotheses** and settles nothing on its own.

What settles it is a *different* RTTI artifact with different emission rules:
the **`??_R2` base-class array** reached from a **derived** class's `??_R3`
ClassHierarchyDescriptor. That array is emitted **completely and
unconditionally** — it is what `dynamic_cast` walks — so an abstract,
never-instantiated, or private base still gets a `??_R1` + `??_R0` entry there.
A base cannot be in the hierarchy and absent from the derived class's array.

```
$ python3 tools/retail_rtti.py class DxMat
COL @ 0x821ea730  CHD@0x821ea744  numBaseClasses=5
   [0] .?AVDxMat@@       ncb=4 PMD(m=0,p=-1,v=0)
   [1] .?AVNgMat@@       ncb=3 PMD(m=0,p=-1,v=0)
   [2] .?AVRndMat@@      ncb=2 PMD(m=0,p=-1,v=0)
   [3] .?AVObject@Hmx@@  ncb=1 PMD(m=0,p=-1,v=0)
   [4] .?AVObjRef@@      ncb=0 PMD(m=0,p=-1,v=0)
```

★ **The `ncb` (numContainedBases) chain is 4,3,2,1,0 — gapless and
arithmetically complete.** `RndMat` contains exactly **2** bases: `Hmx::Object`
and `ObjRef`. An intermediate class between `RndMat` and `Hmx::Object` would
make `RndMat`'s `ncb` 3 and the array 6 entries long. **There is no room for an
intermediate under ANY name** — which also forecloses the obvious escape hatch
"maybe retail called it something else", without depending on a string at all.

`NgMat` (4 bases) and `DxMat` (5) prove intermediates *are* listed when they
exist. The test could have failed and did not.

### Corroborating strands (each weaker alone; all agree)

| # | evidence | value |
|---|---|---|
| 1 | `??_R2` base-class array, gapless `ncb` | **DECISIVE** (above) |
| 2 | One implementation of each material virtual | **DECISIVE** (below) |
| 3 | `ObjPtr<RndMat>` + `ObjPtrList<RndMat>` type descriptors exist; **no** `ObjPtr<BaseMaterial>` | positive ID of `mNextPass`'s type |
| 4 | literal `BaseMaterial` occurs **0×** in the 14 MB image | corroboration only |
| 5 | `REGISTER_OBJ_FACTORY(BaseMaterial)` needs the string `"BaseMaterial"` (0 hits) ⇒ retail cannot register such a factory | corroboration |
| 6 | rb3-Wii (RB3-**era** oracle) has `class RndMat : public Hmx::Object`, no `BaseMaterial.h`/`MetaMaterial.h`; DC3 (newer) has all three | corroboration, labelled |

Strand 4 is the one MAT-1 leaned on. It is real (positive controls fire:
`RndMat` 7, `NgMat` 1, `DxMat` 1, `RndTex`, `RndDrawable`, `Object@Hmx`; and
`RndFontBase` 0 reproduces the known `FontBase` precedent) — but it is
corroboration, not proof, and this lane does not rest on it.

### Evidence class 2: one virtual implementation, not two

Retail vtables `RndMat` @`0x8206571c`, `NgMat` @`0x82075294`, `DxMat`
@`0x8210315c` are each **21 slots**, and slots **6–10 are byte-identical across
all three**:

| slot | retail address | what our map calls it |
|---|---|---|
| 6 `Handle`       | `0x82438138` | `?Handle@RndMat@@` (pinned in `Mat.cpp`) |
| 7 `SyncProperty` | `0x82436488` | *(pinned inside `MetaMaterial.cpp`'s range)* |
| 8 `Save`         | `0x82435dc0` | `?Save@BaseMaterial@@` (pinned in `BaseMaterial.cpp`) |
| 9 `Copy`         | `0x82438c28` | unnamed |
| 10 `Load`        | `0x82438f40` | unnamed |

Retail has **one** `Handle`, **one** `SyncProperty`, **one** `Save`, **one**
`Copy`, **one** `Load` for the entire material hierarchy. Our tree emits
**fifteen** — `BEGIN_SAVES(BaseMaterial)`, `BEGIN_SAVES(RndMat)`,
`BEGIN_SAVES(MetaMaterial)` and likewise for the other four. Ten of those
fifteen have no retail counterpart.

⇒ **Our `BaseMaterial` *is* retail's `RndMat`.** `?Save@BaseMaterial@@` matches
100% at 988 B *because* it is retail's one material `Save`. Our `RndMat::Save`
(3 lines, `SAVE_SUPERCLASS(BaseMaterial)`) is surplus DC3 scaffolding.

⚠ The slot **count** (21) is *consistent* with the merge but does **not**
discriminate — a split whose derived class adds no new virtuals also yields 21.
Only the one-vs-two *implementation* count discriminates. Slots 0/3/5/18
resolving to `ModalKeyListener` / `TrackPanelDirBase` / `DanceRemixer` /
`XShaderPDBBuilder_AddRef` are ICF fold-aliases of trivial bodies — expected per
this repo's ICF finding, and evidence of nothing.

### What was ruled OUT, explicitly

- **`sizeof` did not discriminate.** After MAT-1, `sizeof(BaseMaterial) ==
  sizeof(RndMat) == 396` **by construction** (single inheritance, derived adds no
  members), so retail's `li r3, 396` at the material factory (`0x8240f5d0`,
  extent 84, then `bl 0x82438398`) is consistent with *both* hypotheses. It
  settled MAT-1's question, not this one. Its only contribution here is that
  retail has **one** factory where we emit two.
- **Absence of a `??_R4` COL for `BaseMaterial`** — insufficient, per above.
- **Oracle agreement** — DC3 is newer and rb3-Wii has holes; strand 6 is stated
  as corroboration. It does agree on a *specific structural prediction*
  (`RndMat : Hmx::Object`, ncb=2) rather than vaguely, which is worth something,
  but the verdict rests on retail bytes.

### Instrument provenance

`tools/retail_rtti.py`, reused not rebuilt (its header exists precisely because
three lanes re-derived this and two baked in the wrong `.data` VA skew, producing
**false absences shaped like decisive negatives** — the exact failure mode this
lane was most exposed to). `--selftest` **8/8**; `--selftest --sabotage naive-va`
drops to **2/8**, so the screen is demonstrably falsifiable. All binary scanning
done in Python — never `grep`, which is binary-blind here.

---

## Census — for the lane that executes the merge

**Map rows carrying any `BaseMaterial` mangled form: 6** (full-form matched, not
substring). Of those, **5 are at `mpn` 100**, totalling **1,532 B**:

| addr | symbol | unit | mpn / fuzzy | size |
|---|---|---|---|---|
| `0x824a8db0` | `?Queue@RndSoftParticleBuffer@@…W4Blend@BaseMaterial@@@Z` | `SoftParticleBuffer` | 100 / 100 | 188 |
| `0x8240f5d0` | `?NewObject@BaseMaterial@@SAPAVObject@Hmx@@XZ` | `rndobj/Rnd` | 100 / 99.76 | 84 |
| `0x82435858` | `??1?$ObjRefConcrete@VBaseMaterial@@VObjectDir@@@@UAA@XZ` | `BaseMaterial` | 100 / 100 | 100 |
| `0x82435dc0` | `?Save@BaseMaterial@@UAAXAAVBinStream@@@Z` | `BaseMaterial` | 100 / 100 | 988 |
| `0x824e2c20` | `?SetMatColorFlags@@…W4ColorModFlags@BaseMaterial@@…` | `Crowd` | 100 / 100 | 172 |
| `0x82435608` | `?SetObjConcrete@?$ObjRefConcrete@VBaseMaterial@@…@Z` | `BaseMaterial` | 56.69 / 55.73 | 104 |

Compare the two precedents: a lane that found **224 rows / 208 at 100% /
19,464 B** correctly **REFUSED**; a lane that found **1 row per symbol, none at
100%** correctly **PROCEEDED**. This sits far closer to the second — and unlike a
one-sided rename, all 6 can be updated **in lockstep** with our class rename, so
pairing is preserved and Δmatched should be ≈0.

**Source / symbol blast radius:**

- **121** distinct emitted symbols carry `BaseMaterial`, across 15+ objects
  (`BaseMaterial.obj` 80, `Mat.obj` 48, `MetaMaterial.obj` 43, `Shader.obj` 30…).
- **53 objects** carry a *nested-enum* mangled name — `Blend` and `ColorModFlags`
  are nested in `BaseMaterial`, so the owning class leaks into every signature
  taking one. `?SetBlend@RndMat@@QAAXW4Blend@BaseMaterial@@@Z` alone appears in
  **50 objects**. This is the widest ripple and the main recompile cost.
- Subclasses affected: `RndMat` (→ merges away), `MetaMaterial` (`: BaseMaterial`).
- We emit `??_R0/R1/R2/R3/R4 BaseMaterial` and the string literal
  `??_C@_0N@PIEAOGAC@BaseMaterial?$AA@`; retail has **none** of them, and our
  `??_R3RndMat@@8` therefore claims `numBaseClasses=4` where retail says **3**.

**Worth / risk.** The merge is **layout-neutral** — single inheritance with no
added members already yields byte-identical objects — so it buys no bytes
directly and is unlikely to move `matched_functions` if the 6 rows move in
lockstep. What it buys is **accuracy**: a correct hierarchy, correct emitted
RTTI, deletion of 10 surplus virtuals with no retail counterpart, and removal of
a string retail does not contain. Under the standing directive that *accuracy
beats headline %* and *a metric that hides real bugs is worse than a lower
metric*, that is worth doing — but it is a correctness play, not a points play,
and should be briefed as one.

## ⚠ Flagged, NOT verified by this lane

`MetaMaterial.cpp` is pinned to `.text` `0x82436488–0x82438138` and
`0x824382D4–0x8243833C`, and `0x82436488` is **retail `RndMat` vtable slot 7**.
Since `MetaMaterial` has zero string and zero RTTI presence in retail, that pin
cannot be covering `MetaMaterial` code — yet `default/MetaMaterial` reports
**53 of 67 functions matched**. The bodies genuinely match retail bytes (same
engine lineage); the *class attribution* is ours. Combined with the pins for
`BaseMaterial.cpp` (`0x82435528–0x8243619C`) and `Mat.cpp`
(`0x824361E0–0x82436278`, `0x82438138–0x824382D4`) interleaving into one
contiguous region ~`0x82435520–0x82439000`, this reads as **one retail material
TU carved into three of our units** — consistent with `/O1` preserving TU
spatial grouping. That is a hypothesis with good support, not a settled finding;
it needs its own lane and its own controls before anyone acts on it.

Unit figures above are read from a `report.json` dated **Aug 13 06:46**, i.e.
*before* MAT-1 landed at 18:50 (it still shows `BaseMaterial` 23, MAT-1's A/B
ended at 22). Ruler `name_check`, read from `provenance`. Treat them as
indicative magnitudes, not current absolutes.
