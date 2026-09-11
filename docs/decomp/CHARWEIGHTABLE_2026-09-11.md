# `OBJ_MEM_OVERLOAD`'s operator delete was `noinline` and retail inlines it — **the briefed fix was the wrong lever, and the right one is one attribute** (lane W4-G, 2026-09-11)

> **VERDICT SUMMARY.** Three handoffs from wave 3. The headline item (H1) was
> briefed as *"take CharWeightable off `OBJ_MEM_OVERLOAD`"*; tested literally,
> **that edit cannot produce retail's bytes** and the real defect is a single
> `__declspec(noinline)` in the macro. The other two handoffs both resolve as
> **negatives with evidence** — one of them refuting a premise of the record that
> raised it.
>
> | # | item | predicted | measured |
> |---|---|---:|---:|
> | 1 | `char/` — 32 proven classes to an inlinable operator delete | +31 fns / +2,328 B | **+30 fns / +2,240 B** |
> | 2 | extend to the 37 proven classes outside `char/` | +35 fns / +2,740 B | **+44 fns / +3,436 B** |
> | 3 | the 33 other `GetAssetMgr` callers | — | **16 of 18 named already at 100; 0 source bugs** |
> | 4 | the real `DataBasename` row | — | **LOCATED at `0x82517120` — it is ICF-folded with `OnFileGetBase`** |
>
> **Branch net, by chaining the two in-run legs (they chain exactly — run 1's
> leg B IS run 2's leg A: 42505 → 42535 → 42579):
> Δmatched +74, Δmatched_code +5,624 B, Δcode% +0.055397 pp, Δhonest +74,
> `masked_equal` +0, units at 100 % 152 → 160 (+8), and ZERO rows fell off on
> either ruler.** `masked_equal` unmoved at +0 across both runs means every byte
> here is an honest match, not surrendered funclet credit.
>
> ★ **The transferable finding is a correction to how H1 was reasoned, not the
> bytes.** H1 inferred a *source-shape* conclusion ("retail has no class operator
> delete") from a *callee-name* charge. The charge only says retail's `bl` goes to
> `MemFree`; "no operator delete" and "an INLINED operator delete" produce the
> identical relocation, and the second is what retail does. A one-instruction
> charge underdetermines the source construct behind it — which is why the census,
> not the charge, had to pick the population.

## 0. Provenance

Worktree `~/tmp/wt-w4-g`, branch `w4-charweightable`, off main `3ab3f494`
(`git merge-base --is-ancestor` asserted). A **full `./tools/ninja-locked`
(rc=0) was run before any name-keyed analysis** — a fresh worktree's reflinked
target objs are pre-renamer, and every item here is keyed on retail symbol
NAMES. `verify_objs_patched.py --verify-manifest` **exit 0** (1205 decomp /
3086 target objects) before the first edit.

Ruler `name_check`, resolved by `ab_measure` from `objdiff.json`'s options;
objdiff sha256 `a5c35b15d7d46ac4`, **stable across both legs of both runs**.
One change per run, committed between runs, both legs settled.

★ **The `.s` ADDRESS COLUMN IS SYNTHETIC and every scan here is keyed on the
`.fn fn_<addr>` symbol.** Live example from this lane: `CharWeightable.s`
renders `??_GCharWeightable`'s body under `.fn fn_823AF2B8` while the address
column counts from `823AF098`. Keying on the column would have read every body
at the wrong address.

## 1. The census — retail's deleting-destructor policy, and why H1's framing fails

H1 read `??_GCharWeightable`'s charge `[14]` correctly:

```
[14]  target  bl ?MemFree@@YAXPAX@Z            <- retail: the 1-arg global
      base    bl ??3CharWeightable@@SAXPAX@Z   <- ours: a per-class operator delete
```

and concluded *"retail's CharWeightable has no class-level `operator delete`"*,
prescribing removal of `OBJ_MEM_OVERLOAD`. **Tested literally, that prescription
does not reproduce retail.** With no class `operator delete`, the deleting dtor
calls the GLOBAL `::operator delete` = `??3@YAXPAX@Z`, which lives out of line in
MemMgr.cpp at `0x82bc6b70`. It would have swapped one wrong `bl` for another.

The actual mechanism is one attribute. `MemMgr.h` carries

```c
#define MemFree(ptr, ...) (MemFree)(ptr)
```

so the macro **discards every debug argument** and `OBJ_MEM_OVERLOAD`'s operator
delete body is literally `{ (MemFree)(v); }` — trivially inlinable, blocked only
by its own `__declspec(noinline)`. Retail's single `bl ?MemFree@@YAXPAX@Z` *is*
that body inlined. (Contrast the operator **new** half, which uses an explicit
`(void)StaticClassName().Str();` statement, so its name argument really is
evaluated — that asymmetry is why `new` is class-specific and `delete` is not.)

**Census over every named `??_G` body in the retail `.text`** (868 bodies,
`~/tmp/w4g/census.py`, keyed on `.fn`):

| retail policy | bodies |
|---|---:|
| inlined `bl ?MemFree@@YAXPAX@Z` | **193** |
| out-of-line class `operator delete` | **631** |
| global `??3` / no delete call | 44 |

Two properties make this usable rather than suggestive:

- **The classification is unconfounded.** The (calls MemFree ∧ calls a class
  operator delete) cell is **0** — no body does both, so a `MemFree` used for a
  member's own cleanup is never mistaken for an inlined delete.
- **All 631 out-of-line calls reach ONE name**, `??3BinStream@@SAXPAX@Z` — the
  ICF survivor of the folded `{ MemFree(v); }`. Those are exactly the calls
  **alias group 1540 already forgives** (196 operator-delete spellings folded,
  `??3CharWeightable@@SAXPAX@Z` among them).

### 1.1 The control — the census would be worthless if it read "inlined" everywhere

| population | inlined | share |
|---|---|---|
| classes carrying `OBJ_MEM_OVERLOAD` | 69 / 76 | **90.8 %** |
| every other class | 124 / 792 | 15.7 % |
| **within `char/` units only** — `OBJ_MEM_OVERLOAD` | **31 / 31** | **100 %** |
| **within `char/` units only** — their neighbours | 8 / 39 | 20.5 % |

⇒ **the MACRO predicts retail's policy, not the directory.** The per-directory
cut alone (`char/` 55.7 % vs 19.3 % elsewhere) looked like a directory effect and
would have licensed a `char/`-wide flip; the macro-keyed cut inside the *same TUs*
is what actually identifies the population. band3 2.3 %, beatmatch 0 %, xdk 0 %
are the untreated tail that proves the detector can read NOT-inlined.

### 1.2 The 7 apparent counterexamples — 3 are artifacts, and one is a map defect

| class | verdict |
|---|---|
| `LabelShrinkWrapper`, `InlineHelp` | **artifact** — vtable adjustor thunks (`lwz r11,-4(r3); subf r3,r11,r3; b <real ??_G>`); the real `??_G` *does* inline `MemFree`. My classifier did not follow the tail `b`. |
| `SpotlightDrawer` @`0x82341FF8` | **MAP DEFECT** — the body is not a deleting dtor at all. It is a textbook `NewObject`: `addi r3,r31,0x50; bl StaticClassName; li r4,0; li r3,0x290; bl MemAlloc; stw r3,0x54(r31)` — the exact inlined-operator-`new` shape `ObjMacros.h` documents. Handed off, not fixed. |
| `HamIKEffector`, `TransConstraint`, `FlowSlider`, `FitnessFilter` | **genuine** out-of-line; keep plain `OBJ_MEM_OVERLOAD`. |

Corrected totals: **71 inlined / 4 genuine out-of-line / 1 map defect = 94.7 %.**

★ Corroboration that emitting the COMDAT and inlining it are **not exclusive**:
retail *has* `??3CharEyeDartRuleset@@SAXPAX@Z` as a 4 B `b MemFree` row **and**
inlines it at the `??_G` site. That is what a normal inline operator delete does
and what `noinline` forbids — so "retail has a class operator delete symbol" was
never evidence against this fix.

## 2. What was changed — a variant macro, applied only where retail agrees

This is **not** a macro-wide flip. Flipping `OBJ_MEM_OVERLOAD` itself would push
the 631 out-of-line bodies from a **forgiven** name (group 1540) to a **charged**
`MemFree`. Added `OBJ_MEM_OVERLOAD_INLINE_DEL` — identical to `OBJ_MEM_OVERLOAD`
minus the `__declspec(noinline)` — defined in **both** the match and `HX_NATIVE`
branches, and applied per class.

### Run 1 — the 32 proven `char/` classes

Every one of the 32 adjudicable `char/` `OBJ_MEM_OVERLOAD` classes is
`INLINED_MEMFREE` in retail (31 named `??_G` rows + `CharEyeDartRuleset`). All 32
rows sat sub-100 at ~99.7 %, 2,416 B total.

**Predicted +31 fns / +2,328 B — measured +30 fns / +2,240 B.**

```
legA matched=42505 code=3834712 37.426590   (== main's baseline exactly)
legB matched=42535 code=3836952 37.448456
Δmatched=+30  Δmasked_equal=+0  Δhonest=+30  Δcode%=+0.021866pp  Δcode_bytes=+2240
units at 100% [mpn] 152 -> 153  (+1 reached: default/CharBonesBlender, MATCHED_ROSE; 0 fell off)
```

⚠ **The one miss is instructive and it was my screening error, not a surprise in
retail.** `CharClipGroup`'s row sat at fuzzy **99.545** where every single-charge
row sits at **99.773** — i.e. it carried a **second** charged site. The correct
screen is not `fuzzy < 100`.

★ **The exact screen, derived from this run and then used to price run 2:** one
charged relocation-name argument costs exactly **`5/N` pp**, N = instruction count
= size/4. Verified across five sizes with no exceptions:

| size | N | one-charge deficit | `5/N` |
|---:|---:|---:|---:|
| 88 | 22 | 0.227 | 0.227 |
| 84 | 21 | 0.238 | 0.238 |
| 80 | 20 | 0.250 | 0.250 |
| 76 | 19 | 0.263 | 0.263 |
| 68 | 17 | 0.294 | 0.294 |

`CharClipGroup` measured 0.455 = 2 × 0.227. The row that failed to cross is
exactly the row this screen excludes.

The **9 `char/` classes whose `??_G` is unnamed in the map are unadjudicable and
were left on the plain macro**: `CharBlendBone`, `CharDriver`, `CharForeTwist`,
`CharIKHead`, `CharMirror`, `CharNeckTwist`, `CharSignalApplier`,
`CharTransDraw`, `ClipCollide`.

### Run 2 — the 37 proven classes outside `char/`

Screened on the `5/N` rule: **35 rows / 2,740 B at exactly one charge**;
`MeterDisplay` (0.550 vs 0.250) and `PracticeSection` (0.412 vs 0.294) carry a
second charge and were switched anyway (retail proves they want the inline form)
but **not** predicted to cross.

**Predicted +35 fns / +2,740 B — measured +44 fns / +3,436 B.**

```
legA matched=42535 code=3836952 37.448456   (== run 1's legB exactly; the legs chain)
legB matched=42579 code=3840388 37.481987
Δmatched=+44  Δmasked_equal=+0  Δhonest=+44  Δcode%=+0.033531pp  Δcode_bytes=+3436
units at 100% [mpn] 153 -> 160  (+7, all MATCHED_ROSE; 0 fell off)
units at 100% [fuzzy] 124 -> 130 (+6)
```

★ **The overshoot is fully attributed, and the attribution matters more than the
bytes: ALL 35 predicted rows crossed** (the `5/N` screen was 35/35), and the +9
surplus is two mechanisms a `??_G`-row census structurally cannot see:

| surplus | rows | mechanism |
|---|---:|---|
| `system/synth_xbox/FxSend{Compress,Delay,EQ,MeterEffect,Reverb}` | 5 | a **SECOND unit compiling the same classes** — the screen counted one `report.json` row per class, in `system/synth` |
| `AppInlineHelp`, `StarDisplay`, `AppMiniLeaderboardDisplay`, `system/bandobj/ReviewDisplay` | 4 | **derived classes that declare no mem macro and INHERIT the base's `operator delete`**, so their deleting dtors inlined too |

⇒ **that second mechanism is exactly the base-class ripple H1 predicted for
CharWeightable** — it is real, it just materialises in `ui/`, `hamobj/` and
`bandobj/` rather than in `char/` (every `char/` derivative declares its own
`OBJ_MEM_OVERLOAD`, so there is nothing to inherit).

`Spotlight` and `UISlider` read **+2** because each unit hosts two of the 37
classes (`ColorPalette` and `PanelDir` respectively); the ROW count was right and
the unit count is not the same measure.

## 3. Handoff 2 — the other `AssetMgr::GetAssetMgr` callers: **a clean negative**

W3-A renamed `0x82569E78` from `SystemConfig` to `AssetMgr::GetAssetMgr`, noted
"36 callers", and deferred "the 33 remaining". Measured (`~/tmp/w4g/callers.py`,
every retail `bl`/`b` to `fn_82569E78` keyed on `.fn`): **34 retail callers, 18
named, 16 anonymous.** The population is not 33 and it is not a source-work
backlog:

| class | rows | disposition |
|---|---:|---|
| named, already `fuzzy == 100` | **16** | crossed on W3-A's rename; nothing owed |
| named, still sub-100 | **2** | adjudicated below — **neither is a callee defect** |
| anonymous in retail (no map name) | **16** | unpaired; an IDENTIFICATION problem, not source |

The 16 anonymous callers cluster in `CustomizePanel` (6), `AssetProvider` (5),
`ClosetMgr` (2), `NewAssetProvider` (2), `HamSupereasyData` (1).

**The briefed source-bug hypothesis is empty.** The brief expected "our source
calling `SystemConfig()` where retail calls `GetAssetMgr()`". Both sub-100 callers
*already* call `AssetMgr::GetAssetMgr()` (`CharacterCreatorPanel.cpp:171`,
`Award.cpp:49`), and a tree-wide sweep finds **46 `SystemConfig()` call sites,
every one of them consuming a `DataArray`** (`FindArray`/`FindData`/`File()`), i.e.
genuinely the config accessor. There is no `SystemConfig`/`GetAssetMgr` confusion
left in identified code.

### The two sub-100 rows, adjudicated on retail bytes

Both carry **exactly one** charged site by the `5/N` rule — 228 B → 5/57 = 0.0877
(actual 0.088); 608 B → 5/152 = 0.0329 (actual 0.033).

**`?Configure@Award@@UAAXPAVDataArray@@@Z` (608 B, 99.967) — an unaliased ICF
fold.** Retail's callee sequence has two fold-shaped names: `?HasLesson@LessonMgr@@QBA_NVSymbol@@@Z`
invoked on the `AssetMgr*` just returned by `GetAssetMgr` (our source calls
`pAssetMgr->HasAsset(curAsset)` — same signature shape, const/bool/Symbol-by-value),
and `?insert@?$list@UWeightContext@CharBone@@…` for our `mAwardEntries.push_back(entry)`.
**Checked `scripts/symbol_aliases.json` before believing either**, as required:
`?HasAsset@AssetMgr@@QBA_NVSymbol@@@Z` **is already folded into group 1590**
(survivor `?HasLesson@LessonMgr@@`), so that site is forgiven ⇒ the single
remaining charge is the **`list<AwardEntry>::insert` ≡ `list<CharBone::WeightContext>::insert`
fold, which is NOT aliased.** A fold, not a source bug. Left uninstalled: an
alias needs T1 retail-byte proof and `tools/ourside_fold_sweep.py --install`
exists to produce it — installing one by hand is the integrity hazard
`CLAUDE.md` names.

**`?AddGridThumbnails@CharacterCreatorPanel@@` (228 B, 99.912) — not a callee
defect at all.** Every named retail callee matches our source one-for-one:
`Symbol` ctor, `GetAssetMgr`, `MakeString<Symbol,…>`, `MakeString<const char*>`,
`?AddTex@TexLoadPanel@@QAAPAVDynamicTex@@PBD0_N1@Z`; the two unnamed callees
(`fn_8256B4C8` = `GetEyebrowsCount`, `fn_822AF1D8` = `BandHeadShaper::GetCount`)
are placeholder targets and already forgiven. Its residual is elsewhere in
codegen. **No source change is warranted, and none was made.**

## 4. Handoff 3 — the real `DataBasename` row: **LOCATED, `0x82517120`**

W3-A reported the row named `DataBasename` was actually
`DataLocalizeSeparatedInt`, and that the real one "could not be located because
`FileGetBase` is not in the map".

**Both halves tested literally. The second is refuted: `FileGetBase` IS in the
map, at `0x825166E8`.** And the first is confirmed independently, below.

### 4.1 Method — the registration table, and the off-by-one that nearly broke it

`DataRegisterFunc("basename", DataBasename)` pairs a name string with a function
pointer, so finding the string finds the function. String VAs from the retail PE
(Python, never `grep` — the shell's `grep` is binary-blind): `"basename"`
@`0x821089F0`, `"file_get_base"` @`0x82087DB4`.

⚠ **The naive pairing is WRONG and would have produced a confident
misidentification.** In `DataFunc.s` the block that loads `"basename"` stores
`fn_82761A50` = `?DataFindObj@@` — the *previous* entry's function. The Symbol is
constructed at the END of block N and consumed by the map insert at the START of
block N+1, so **the pointer stored in block N+1 belongs to the string loaded in
block N**.

★ **A known-answer control settles the direction rather than my reading of the
scheduling.** The block loading `"localize_separated_int"` stores
`fn_827600E8` = `?DataLocalize@@`; the **next** block stores `fn_82760158` =
`?DataLocalizeSeparatedInt@@`, which the map already places there. The control
both fixes the offset and **independently confirms W3-A's `DataLocalizeSeparatedInt`
identification.**

### 4.2 The result

Applying the corrected pairing, the block after `"basename"` stores
**`fn_82517120`** — and `File.s` independently registers `"file_get_base"` to the
**same address** (there the shape is a direct `DataRegisterFunc` call with no
off-by-one: `Symbol(&temp,str); lwz r3,0(r3); addi r4,fn_82517120; bl DataRegisterFunc`).

⇒ **`DataBasename` and `OnFileGetBase` are ICF-FOLDED INTO ONE FUNCTION at
`0x82517120`.** Two different name strings, two different TUs, one identical
function pointer. *That* is why the row was unlocatable — there is no separate
address to find, and no amount of map searching would have produced one.

The body confirms it and it is 100 % consistent with `DataBasename`'s source:

```
lwz r11,0(r4) ; mr r31,r3 ; addi r3,r11,8
bl ?Str@DataNode@@QBAPBDPBVDataArray@@@Z
bl FileGetBase                      <- fn_825166E8
mr r4,r3 ; mr r3,r31
bl ??0DataNode@@QAA@PBD@Z
```

= `DEF_DATA_FUNC(DataBasename) { return FileGetBase(array->Str(1)); }` exactly
(`DataFunc.cpp:1180`).

### 4.3 A real source divergence falls out of it — **found, not fixed**

Our `OnFileGetBase` (`File.cpp:374`) is **not** that body:

```cpp
DataNode OnFileGetBase(DataArray *da) {
    static char my_path[256];
    const char *str = da->Str(1);
    MainThread();
    return FileGetBaseBuf(str, my_path);      // own static buffer + MainThread()
}
```

Retail's has **no `MainThread()` and no local static buffer** — it calls
`FileGetBase`, which does both internally. So retail's source is
`{ return FileGetBase(da->Str(1)); }`, **identical to `DataBasename`, which is
precisely why the two fold.** Our DC3-inherited version cannot fold. The same
shape applies to `OnFileGetPath` (`fn_825170D8`, calls `fn_82516550`).

**Deliberately NOT changed in this lane** — see §6.

## 5. Final tree gates

Run on the rebased tip after a full `./tools/ninja-locked` (rc=0, 1,054 edges):

```
verify_objs_patched.py --verify-manifest   EXIT 0   1205 decomp, 3086 target objects match
                                                    2026-09-11T02:56:42Z  tree_sha256=af3093a87f368335
verify_objs_patched.py --check             EXIT 0   fixed point of 6 post-compile passes;
                                                    1044/1044 declared compiled objects pair (100.0%)
                                                    (relpath-only pairing would reach 343)
scripts/verify_split_current.py --check    EXIT 0   4 inputs match dtk's split_manifest.json
tools/symbols_fixpoint_guard.py            EXIT 0   sha256 1122319a1cea3ff1, 225965 lines, SPLIT ran
tools/icf_alias_finder.py --validate       EXIT 0   PASS -- 1356 map-consistent, 233 tolerated,
                                                    0 CONTRADICTED, 1591 total
tools/native_build_gate.sh                 see the NATIVE_GATE_RESULT line in the lane report
```

⚠ **No alias group was installed, removed or edited by this lane**, and no map or
splits row was touched — the branch is `src/` only (70 files: `MemMgr.h` + 69
class headers). The alias gate is run anyway because the whole argument in §1
turns on group 1540 still forgiving the 631 out-of-line bodies.

★ **The native gate is MANDATORY here and not a formality**: `MemMgr.h` is a
shared header and the 69 class headers are native-linked, so this change alters
`operator delete` emission in every native target. The X360 match build is
structurally incapable of catching an undefined symbol the native linker sees.

## 6. What this lane did NOT do

- ⛔ **Did not flip `OBJ_MEM_OVERLOAD` itself**, though 94.7 % of its adjudicable
  classes want the inline form. The remaining **170 headers** on the plain macro
  are (a) the 4 proven out-of-line classes and (b) ~161 classes whose `??_G` is
  **unnamed in the map and therefore unadjudicable**. Flipping those would be an
  unverified change to every `delete` site of ~161 types on a majority argument.
  The `??_G` census is only a *sample* of "what retail does when deleting one of
  these" — it says nothing per-class about the unnamed ones.
- ⛔ **Did not fix `OnFileGetBase` / `OnFileGetPath`** (§4.3), although the
  divergence is proven on retail bytes. Both rows are **unpaired** (`0x82517120`
  and `fn_825170D8` are unnamed in the map), so the fix is metric-invisible
  today, and the pairing half is a **map edit** whose economics are adversarial:
  naming `0x82517120` converts the currently-**forgiven** placeholder call at
  `DataFunc.cpp`'s registration site into a **checked** one against whichever of
  the two folded spellings we choose, so it needs an alias for the twin in the
  same wave. That is one coherent map+source+alias change and it did not fit
  after two full A/B runs. Handed off with the evidence rather than half-landed.
- ⛔ **Did not install the `list<AwardEntry>::insert` alias** (§3) — needs
  `tools/ourside_fold_sweep.py --install` T1 proof, not a hand edit.
- ⛔ **Did not fix the `SpotlightDrawer` map defect** (§1.2) — `0x82341FF8` is
  named `??_GSpotlightDrawer@@UAAPAXI@Z` but its body is a `NewObject`. A map
  edit, out of scope after the source runs.
- ⛔ **Did not touch `CharClipGroup`'s second charge**, `MeterDisplay`'s or
  `PracticeSection`'s — all three are now single-issue rows with the delete side
  correct, i.e. cheaper than they were.
- ⚠ **Did not adjudicate the 16 anonymous `GetAssetMgr` callers** (§3) — they are
  unpaired, so nothing in the source can register; identification work.

## 7. Open handoffs

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **`OnFileGetBase`/`OnFileGetPath` source + name `0x82517120` + alias its fold twin** | §4.2/§4.3 — retail body quoted; fold proven by two registration sites |
| H2 | **`FileGetBase` is a live 192 B near-miss at fuzzy 29.917** | map names it `0x825166E8`; our version calls `FileGetBaseBuf` out of line where retail's 192 B body looks inlined |
| H3 | **`SpotlightDrawer` `??_G` map row is a `NewObject`** | §1.2, body quoted |
| H4 | **`list<AwardEntry>::insert` fold alias** — the last charge on `Award::Configure` (608 B) | §3; sibling `HasAsset` already in group 1590 |
| H5 | **The ~161 unadjudicable `OBJ_MEM_OVERLOAD` classes** | §6; needs their `??_G` identified before the macro can be flipped wholesale |
| H6 | **4 proven out-of-line classes** (`HamIKEffector`, `TransConstraint`, `FlowSlider`, `FitnessFilter`) | §1.2 — these are the reason the macro cannot simply be flipped |
