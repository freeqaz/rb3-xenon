# Naming two unpaired addresses — and what Tessellate's 552 B turned out to be

**Lane W14-A · 2026-09-13 · branch `w14-name-unpaired` off `main` `90be524c`**
**Tip `0539b59f`** (2 commits: `f31eac25` map, `0539b59f` source)

Lane W13-A (`docs/decomp/SWAPPED_ARGS_2026-09-13.md`, merge `d4564ac5`) fixed two
confirmed real defects and measured Δ0, because both enclosing rows were
**unpaired** — `fuzzy 0 / mpn 0`, since neither address was named. This lane
names them, and then uses the newly-visible row to find what the rest of
`Tessellate`'s divergence actually was.

Both landed. Nothing regressed. **0 bytes bought, and that was pre-registered.**

---

## Result summary

| # | thing | verdict |
|---|---|---|
| 1 | name `0x82491918` = `RndAmbientOcclusion::Tessellate` | **SAFE, landed** — fuzzy 0 → 57.44 |
| 2 | name `0x824f5b68` = `QuatSpline` | **SAFE, landed** — fuzzy 0 → 65.80 |
| 3 | Tessellate's remaining 552 B | **EXPLAINED and largely CLOSED** — fuzzy 57.44 → **64.15** |
| 4 | `MatAnim`'s `_M_allocate_and_copy<Key<Color>*>` row | **pre-existing WRONG MAP NAME**, recorded, not acted on |

---

## Task 1 — the two names

### The evidence is a 100%-matching caller, not a plausible mangling

The naming argument that carries weight here is **not** "our source has a method
with that mangled name". It is that each call site is reached from a caller row
that **already scores `fuzzy 100`** — our bytes reproduce retail exactly,
*including the branch itself* — and the branch sits at the **same section offset
in a section of the same size** on both sides:

```
target obj/AmbientOcclusion.obj  .text$dup sz=0x74   reloc va=0x54 -> fn_82491918
base   src/.../AmbientOcclusion.obj .text  sz=0x74   reloc va=0x54 -> ?Tessellate@RndAmbientOcclusion@@QAAXPAM0@Z
  enclosing ?OnCalculate@RndAmbientOcclusion@@IAAX_N@Z   116 B   fuzzy 100

target obj/Key.obj via PropKeys  .text$dup sz=0x114  reloc va=0x5c -> fn_824F5B68
base   src/.../PropKeys.obj      .text     sz=0x114  reloc va=0x5c -> ?QuatSpline@@YAXABV?$Keys@VQuat@Hmx@@V12@@@PBV?$Key@VQuat@Hmx@@@@1MAAVQuat@Hmx@@@Z
  enclosing ?QuatAt@QuatKeys@@UAAHMAAVQuat@Hmx@@@Z       276 B   fuzzy 100
```

If retail's branch at that offset went anywhere else, the caller could not be at
100. The mangling agreeing is then a corollary, not the argument.

### Preconditions, all checked BEFORE the edit

A map edit is its own risk class and none of these is optional:

- **Our base object DEFINES both names** — COFF storage class 2 (EXTERNAL),
  section 15629 and section 40 respectively. objdiff pairs target↔base **by name
  per unit**, so a name our object cannot define reads 0% forever however
  correct it is. Byte-`count` is not this test: it does not distinguish a
  definition from an `UNDEF` reference, and in fact `?QuatSpline@...` appears in
  `AmbientOcclusion.obj` too — as a reference.
- **Checked AFTER a full build.** A reflinked worktree carries the target objs
  but not the effect of `obj_target_symbol_renamer`, so every mangled-name lookup
  reads "absent" and any negative is vacuous. Asserted the renamer had run:
  176 mangled names present in `obj/AmbientOcclusion.obj` alongside 268 remaining
  `fn_`.
- **No injectivity break** — neither name already appears as a value in the map,
  and neither target obj already contains the name.
- **Alias check keyed on the ADDRESS, not the name** — `82491918` and `824f5b68`
  each occur **0 times** in `scripts/symbol_aliases.json`. (The nearby `824f5`
  hit is `0x824f5f10`, `InvExpInterpolator::Reset`.) A name-keyed census
  structurally cannot see an alias group orphaned by a rename; that is what cost
  an earlier lane a predicted +3,852 that measured +836.
- Neither address is in the map's `_denylist`.

### The caller census that priced the downside

Naming converts a **forgiven placeholder** call site into a **checked** one, so
the risk lives in callers, not in the named row. Census driven off the
**relocation table** (never a linear disassembly — capstone's PPC decoder halts
at the first undecodable word, which is how W13-A got a vacuous "0 bl callees"
in a 4,260 B function):

| caller | size | fuzzy before | exposure |
|---|---|---|---|
| `?OnCalculate@RndAmbientOcclusion@@IAAX_N@Z` | 116 B | 100.0 | −116 B if our spelling differed |
| `?QuatAt@QuatKeys@@UAAHMAAVQuat@Hmx@@@Z` | 276 B | 100.0 | −276 B if our spelling differed |
| `MatAnim` `_M_allocate_and_copy<Key<Color>*>` | 1160 B | 5.59 | none — already contributes 0 bytes |

Worst case **−392 B**, and both exposures were then *eliminated* by reading our
base objects' relocations and confirming the identical name at the identical
offset. That is why this was landed rather than deferred.

### Pre-registration and measurement

| measure | pre-registered | measured |
|---|---|---|
| Δmatched_functions | 0 | **+0** |
| Δmatched_code | 0 | **+0 B** |
| Δcode% | 0 | **+0.000000 pp** |
| Δfuzzy | **POSITIVE, +0.02..+0.05 pp** | **+0.031406 pp** (49.154740 → 49.186146) |
| units off 100% | 0 | **0** (164 → 164) |

`none`-ruler control **FLAT**, consistent with a pure rename.

Per-row sweep over all **69,219** rows — the only change is the two renames,
**0 common rows better or worse**:

```
fn_82491918 fuzzy 0 -> ?Tessellate@...  fuzzy 57.442867 / mpn 59.57381
fn_824F5B68 fuzzy 0 -> ?QuatSpline@...  fuzzy 65.801530 / mpn 68.89313
?OnCalculate@...     100.0 -> 100.0 (held)
?QuatAt@QuatKeys@@   100.0 -> 100.0 (held)
```

**0 bytes.** Neither row reaches `fuzzy == 100`, and `matched_code` is
all-or-nothing per row. What the edit buys is **5,320 B of previously
unscoreable code becoming measurable** — the documented payout of naming an
anonymous address is bug exposure and visibility, not bytes. Task 2 is that
payout being collected.

**One prediction wrong, in the conservative direction.** I expected the misnamed
MatAnim row to dip from its newly-checked call site; it measured exactly 0. At
5.59% fuzzy that instruction is already an insert/delete mismatch, so an
argument-level charge adds nothing on top of it.

---

## Task 2 — Tessellate's remaining 552 B

W13-A left ours at 4,244 B vs retail 4,796 B. **The gap is one missing block**,
and it is not subtle once the row is visible.

### Located by cluster, not by reading 1,327 instructions

The diff put ~134 of the missing instructions in one contiguous tail run
(clusters at idx 1171-1197, 1207-1243, 1249-1276, 1283-1311, plus cluster 61) —
**536 of the 552 B in essentially one place**, all pure deletes.

### What it is

Retail's final "sync all meshes" loop does **not** stop at `mesh->Sync(0x3f)`.
Per mesh it also tests a `batcher.batching` data variable and, unless batching is
on, sends two messages to the global `"milo"` tool object so the editor records
the AO result.

Decoded from retail bytes by resolving every callee through
`target_symbol_map.json` and every string through `band.exe` (file offset is
simply `vma - 0x82000000`):

| retail | name | role |
|---|---|---|
| `fn_82417780` | `?Sync@RndMesh@@QAAXH@Z` | the part we had |
| `fn_827C0728` | `??0Symbol@@QAA@PBD@Z` | `"batcher.batching"` |
| `fn_8274B1A0` | `?DataVarExists@@YA_NVSymbol@@@Z` | |
| `fn_8274B948` | `?DataVariable@@YAAAVDataNode@@VSymbol@@@Z` | |
| `fn_8274B0F8` | `?Int@DataNode@@QBAHPBVDataArray@@@Z` | `r4=0` ⇒ `.Int(0)` |
| `fn_82750188` | `?FindObject@ObjectDir@@QAAPAVObject@Hmx@@PBD_N@Z` | `"milo", false` |
| `fn_8274AA08` | `??0DataNode@@QAA@PBD@Z` | `"Ambient Occlusion"` |
| `fn_82270580` | `??0Message@@QAA@VSymbol@@ABVDataNode@@1@Z` | `"record"` |
| `fn_82271C50` | `??0Message@@QAA@VSymbol@@ABVDataNode@@@Z` | `"update_objects"` |
| `fn_82270510` | `?Release@DataArray@@QAAXXZ` | the temporaries |

Strings: `0x820715F8` `batcher.batching` · `0x8207160C` `Ambient Occlusion` ·
`0x82071620` `record` · `0x82024680` `update_objects` · `0x82024690` `milo`.

The two `DataNode` temporaries are built inline and their **tags confirm the
spelling**: `{objptr, 4}` is `kDataObject` ⇒ `DataNode(mesh)`, and `{1, 0}` is
`kDataInt` ⇒ `DataNode(1)`. The guard order is read straight off the branches —
`FindObject` runs unconditionally, then `beq` on null and `bne` on batching, i.e.
`if (milo && !batching)`. The global at `0x82E054B8` is `ObjectDir::Main()`;
`ChordShapeGenerator::NameMesh` already spells that idiom in-tree.

```cpp
RndMesh *mesh = *it;
mesh->Sync(0x3f);
bool batching = false;
if (DataVarExists("batcher.batching")) {
    batching = DataVariable("batcher.batching").Int(0) != 0;
}
Hmx::Object *milo = ObjectDir::Main()->FindObject("milo", false);
if (milo && !batching) {
    milo->Handle(Message("record", DataNode(mesh), DataNode("Ambient Occlusion")), true);
    milo->Handle(Message("update_objects", DataNode(1)), true);
}
```

### ⚠ NEITHER ORACLE HAS THIS CODE

`dc3-decomp`'s `AmbientOcclusion.cpp` Sync loop is bare (its line 1501), and so
is rb3-Wii's. The string `batcher.batching` appears **nowhere in either tree**.
This is reconstructed from retail bytes alone, not ported — and that absence is
*the reason our source was short*: we inherited dc3's newer, trimmed version.
This is a concrete instance of the standing caveat that dc3 is newer than RB3 and
may have dropped things.

### Measured

Pre-registered: Tessellate fuzzy POSITIVE into 65-85; Δ`matched_code` **0** from
this row. Note that a sub-100 row contributes 0 bytes either way, which is
exactly **why this experiment could not lose bytes** — the downside was bounded
to `fuzzy` before it was run.

`ab_measure --from-dirty`, 3 real leg-B recompiles:

```
Δmatched=+0  Δcode_bytes=+0  Δcode%=+0.000000pp
Δfuzzy=+0.003170pp   (49.186146 -> 49.189316)
units at 100%: 164 -> 164  (0 reached, 0 fell off)
```

Per-row sweep over all 69,219 rows — **only three rows moved**:

```
?Tessellate@...  4796 B  fuzzy 57.4429 -> 64.1518  mpn 59.5738 -> 66.3411
fn_82492C74        40 B  fuzzy 99.3000 -> 99.8000
fn_82492C9C        40 B  fuzzy 99.4000 -> 99.3000
```

The two 40 B rows are EH-adjacent thunks wobbling by ≤0.5 pp; both stay sub-100
and neither moves a byte.

### It is NOT finished, and the residual has changed character

Landed at **64.15**, below the predicted 65-85 band. Our emitted `Tessellate` is
now **4,932 B against retail's 4,796** — we **overshot by 136 B** having been
552 B short. So the block is present and correct in *structure* but not yet in
*codegen shape*: the residual is register allocation, stack/temporary layout and
EH-funclet placement, **not missing logic**. That is a different and much less
tractable class than what this lane closed.

---

## `MatAnim`'s misnamed row — recorded, not acted on

While censusing QuatSpline's callers: the target row named
`??$_M_allocate_and_copy@PBV?$Key@VColor@Hmx@@@@...` in `default/MatAnim` is
**1,160 B** and calls `QuatSpline`. Our function of that name is **160 B** and
calls `MemOrPoolAllocSTL` / `__uninitialized_copy` / `MemOrPoolFreeSTL`. A
vector's allocate-and-copy does not call a quaternion spline. **That is a wrong
map name**, pre-existing, and it explains the row's 5.59% fuzzy.

Not acted on — it belongs to a map-repair lane, and per the standing economics a
wrong name is financed by its callers, so it needs its own caller census before
anyone touches it.

---

## Gates

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE GATE: PASS  (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)

[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-13T08:38:26Z (tree_sha256=564a591dfe437573)

VALIDATE: PASS -- 1362 map-consistent, 239 tolerated, 0 contradicted, 1603 total
```

Native gate run **last**, after the final full `./tools/ninja-locked` (EXIT=0),
because `src/` was touched and a comment-only change has broken that link before.

---

## Deliberately NOT done

- **Did not chase Tessellate to 100%.** The residual is regalloc / stack-layout /
  funclet placement on a 4,796 B function — permuter-class territory, and the
  permuter is off by standing directive.
- **Did not repair the MatAnim wrong map name** (above), nor audit the other 105
  map-scaffold rows.
- **Did not touch QuatSpline's remaining 34%.** W13-A noted our frame is `0xd0`
  vs retail's `0xf0`; that is a stack-layout question, untouched here.
- **Did not audit other `NormalizeTo` / `std::sort` call sites tree-wide** —
  still the cheap sweep W13-A recommended, still not done.
- **Did not name any further anonymous addresses.** Two was the brief, and each
  one needs its own caller census; naming is a bet, not a freebie.
- **Did not verify the reconstructed block's runtime behaviour.** It is proven
  against retail's *bytes and callee names*, not executed. `"milo"` is a
  tool-side object that will simply not be found in a shipped run, so the block
  is inert in-game — which is consistent with retail shipping it.
