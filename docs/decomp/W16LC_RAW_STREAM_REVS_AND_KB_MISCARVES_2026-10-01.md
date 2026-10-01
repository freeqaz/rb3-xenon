# W16-LC — raw-stream rev Loads tree-wide, and W16-KB's mis-carves (2026-10-01)

**Branch `w16-lc`, off main `9d627e235`. Not merged.**
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).

Two leads from the last wave:

1. W16-KC §5.3/§11: Load bodies that keep their revision in a `BinStreamRev` stack wrapper where
   retail splits the packed rev into a file-scope aggregate and reads the raw stream. KC's tree-wide
   census crashed partway. Re-run it, convert every instance.
2. W16-KB §6: dtk mis-carves needing `symbols.txt` fixes — the `Keys<Vector3>` helper in Color's pin,
   ExpInterpolator's ctor, both Matrix4 products, `Normalize(Vector3)`, two MeshAnim leaves. Use
   W16-KE's method and `tools/carve_extent_scan.py`.

## 1. Whole-branch A/B

**main → tip: +26 functions / +19 honest / +8,292 B / +0.080577 pp code / +0.057895 pp fuzzy.
Units at 100 (`mpn`) 377 → 379 (Color, Interp; 0 fell off); all-rows-fuzzy 311 → 311. No row fell
off 100 on either ruler.**

Measured with W16-KE's two-run setup (§4.3), `objdiff-cli` pinned across legs by `ab_measure`, ruler
`name_check`:

| | matched | masked | honest | matched_code | total_code | code % | fuzzy % |
|---|---:|---:|---:|---:|---:|---:|---:|
| main (run 1 leg A) | 49,553 | 24,323 | 25,230 | 5,216,756 | 10,247,140 | 50.909390 | 58.721867 |
| main + carve (run 2 leg A) | 49,553 | 24,323 | 25,230 | 5,216,756 | 10,247,208 | 50.909050 | 58.730797 |
| tip (run 2 leg B) | 49,579 | 24,330 | 25,249 | 5,225,048 | 10,247,208 | 50.989967 | 58.779762 |

- **Run 1, control** (`~/tmp/w16lc/ab_runs/20261001-155633-w16lc-main-control-3142888`): comment-only patch, leg B
  recompiled exactly 1 TU, **Δ0 on every key**.
- **Run 2, branch** (`~/tmp/w16lc/ab_runs/20261001-155634-w16lc-branch-3143093`): kinds map + source + splits,
  leg B recompiled 1,001 TUs, both legs at a split fixed point after 0 extra re-splits (sha chain
  `fd46dffe -> fd46dffe`). In-run Δ: **+26 / +7 masked / +19 honest / +8,292 B**.
- The carve alone (run 2 leg A vs run 1 leg A) moves no matched key: `total_functions` 69,010 →
  68,983 (−27 fragments) and `total_code` +68 B (gap instructions now inside their functions).
- **Prediction, written before the runs** from the lane's own full builds: run 1 Δ0 with leg A
  49,553 / 5,216,756; run 2 leg A 49,553 / 5,216,756; leg B 49,579 / 5,225,048, **Δ +26 / +8,292 B**.
  Measured exactly that.

**Row level** (run 1 leg A vs run 2 leg B, keyed by unit and name; crossing runs is safe here because
the same diff over the lane's own step builds gives the identical list): **25 rows up, 0 down, 0 off
100 on either ruler.** 37 rows vanished and 10 appeared: every vanished row read 0 except
`?Load@TypeProps@@QAAXAAVBinStreamRev@@@Z` (92.61), which reappears renamed at 100 (§3.3). The 27
absorbed fragments, the anonymous rows that gained names, and the four re-homed anonymous rows all
read 0. The `$4 Load@CharBoneTwist` name moved addresses inside one unit at 100 on both sides; its old
address now reads as the anonymous `fn_822F1080` (0).

**Byte reconciliation (after the run, not a prediction):** the 28 rows newly at fuzzy 100 sum to
**8,292 B**, the measured Δ exactly, and no row left fuzzy 100. On `mpn`, 26 rows reached 100 and none
left: the +26. Three of those are `mpn`-only (two StreakMeter funclets at 99.5, Normalize at 99.38).

Contribution by part (lane step builds, each a full build with forced re-split where map or splits
changed, each diffed against the previous):

| step | functions | bytes |
|---|---:|---:|
| §2 rev-wrapper bodies on scored rows (incl. TypeProps rename) | +16 | +5,168 |
| §3.1 CharWeightable Save/Load identified + written | +2 | +232 |
| §3.1 CharBoneTwist / CharUpperTwist Loads | +2 | +348 |
| §3.2 CharacterTest::Load | +1 | +584 |
| §3.4 CharMeshHide Hide reader | 0 | 0 |
| §4 carve (symbols only) | 0 | 0 |
| §4 names on the grown heads | +4 | +1,960 |
| §4.2 Normalize body | +1 | 0 |
| §4.2 Transform product body | 0 | 0 |
| **total** | **+26** | **+8,292** |

## 2. The rev-wrapper census

### 2.1 Retail has no `BinStreamRev` at all

Before converting anything: `.?AVBinStreamRev@@` occurs **0** times in `band.exe` (positive control:
`.?AVBinStream@@` occurs once). So every `BinStreamRev` our build constructs is non-retail, not only
the ones a scored row happens to expose. (CameraShot.cpp already recorded this; it is now re-measured.)

### 2.2 Two instruments

- **The scored census** (KC's `binrev_scan_all.py`, crash fixed: units without a `functions` key are
  skipped; script `~/tmp/w16lc/binrev_census.py`). For every sub-100 row of every unit with a
  source path, it asks objdiff for the instructions and flags rows whose *base* (our object)
  relocates to any `BinStreamRev` symbol. It covered **757 of 757** units with sub-100 rows, no
  failures. It found **10 rows** (KC's partial run had seen 7). A converse pass (retail side names
  `BinStreamRev`, ours does not) found nothing new: the only target-side hit is a map name
  (`Load@TypeProps@@...BinStreamRev`) that was itself wrong (§3.3).
- **The object census** (`~/tmp/w16lc/coff_binrev.py`). The scored census cannot see a function
  whose retail body is anonymous or mis-pinned, because there is no row to diff. This one reads our
  compiled COFF objects directly: every function COMDAT whose relocations name a `BinStreamRev`
  symbol, whatever its score. On main it found **132** such functions (excluding the inline ctor and
  EH funclets). Every one was checked against retail RTTI: the owning class either exists in retail
  (`.?AV<Class>@@` present) or not.

### 2.3 What was converted

Every function in either census whose class exists in retail. All use the shape KC used: one aligned
file static `{ altRev @+0, rev @+4 }` (MSVC addresses both halves off one base register), the raw
stream for every field, no `ASSERT_REVS`.

| function | retail | before → after | notes |
|---|---|---|---|
| `RndDir::PreLoad` | `0x82406178` | 38.86 → 100 | push before `ObjectDir::PreLoad` |
| `RndDir::PostLoad` | `0x82404F80` | 64.16 → 100 | `ObjectDir::PostLoad` first, then pop; `bs >> s >> s` chained |
| `RndAnimatable::Load` | `0x824028B8` | 91.81 → 100 | `sAnimRev` static folded into the aggregate |
| `WorldDir::PostLoad` | `0x824D08F0` | 60.49 → 100 | see §2.4 |
| `StreakMeter::PreLoad` | `0x822D7CC0` | 73.32 → 100 | |
| `BandList::PreLoad` | `0x8233B3E0` | 28.59 → 100 | calls the existing `UIList::PreLoadWithRev(BinStream &, int)` |
| `BandSongPref::Load` | `0x822C0DD0` | 41.95 → 100 | different shape: one 4-byte read into a file `int`, tested signed |
| `Hmx::Object::LoadRest` | `0x8275CE78` | 28.77 → 100 | see §3.3 |
| `CharWeightable::Load` + `Save` | `0x823AF230`, `0x823AEE20` | no row → 100, 100 | identified, §3.1 |
| `CharBoneTwist::Load` | `0x823B82E8` | no row → 100 | identified, §3.1 |
| `CharUpperTwist::Load` | `0x823C7338` | no row → 100 | identified, §3.1 |
| `CharacterTest::Load` | `0x823DDFC0` | no row → 59.06 → 100 | identified, §3.2 |
| `operator>>(BinStream &, CharMeshHide::Hide &)` | | 72.17 → 96.5 | §3.4 |

Twelve EH funclets of these bodies rose with them (StreakMeter 6, Anim 5, PropSync 1), ten of them to
100 and two to 99.5.

**Census at the tip:** the scored census finds one row (a 40-B FlowValueCase funclet, §6); the
object census finds 119 functions, owned by **102 classes, none of which has retail RTTI** (Dance
Central classes: Flow*, Ham*, Skeleton*, RhythmDetector, …). Nothing in scope is left.

### 2.4 `WorldDir::PostLoad`

Converting the wrapper took it to 97.4; three more retail details closed it:
- Retail ends at `SyncHUD()`. The FxSend, doppler, listener and alt-rev reads (revs 0x1A–0x1D) are
  newer-engine revisions; they now sit under the existing `WORLDDIR_DC3_TAIL` gate with the members
  they fill.
- `gOldTexDir` is an internal static in retail: it is stored through the rev aggregate's base
  register (`stw r11, 0xc(r20)`). As an external it took its own relocation.
- The old-chars dir class is a named `Symbol` local (retail reloads it from its stack slot). This
  also restored retail's stack layout; with the inline `ObjPtr<RndCam>` ctor alone, `cam` moved from
  0x60 to 0x58 and a 40-B funclet re-paired from 100 to 99.5 (it returned to 100).

The rev stack is strictly LIFO (`BinStream::PopRev` takes the back). RndDir's old match-build order
pushed before `ObjectDir::PreLoad` but popped before `ObjectDir::PostLoad`, i.e. it popped the base's
rev. Both builds now push before and pop after.

## 3. Identification and map repairs found on the way

### 3.1 Three char Loads whose bodies were anonymous

Each `$4` vtordisp thunk names its class and branches to the real body:

| body | thunk | was pinned in | fix |
|---|---|---|---|
| `CharWeightable::Save` `0x823AEE20` (0x70) | `0x823AF210` | FlowValueCase (map key nulled as a non-RB3 class) | block `0x823AEE20–0x823AEE98` re-homed, named |
| `CharWeightable::Load` `0x823AF230` (0x78) | `0x823AF2A8` | CharPollGroup | block `0x823AF22C–0x823AF2A8` re-homed, named |
| `CharBoneTwist::Load` `0x823B82E8` (0xA8) | `0x823B85A0` | CharBoneDir (correct: CharBoneTwist.cpp compiles into CharBoneDir.obj) | named |
| `CharUpperTwist::Load` `0x823C7338` (0xB4) | `0x823C75B8` | FlowIf (a class RB3 does not contain) | block `0x823C7334–0x823C73F8` re-homed, named |

Retail CharWeightable is **rev 2 with no `Hmx::Object` superclass** in Save or Load (Save writes `2`,
`mWeight`, `mWeightOwner`). Ours saved rev 3 with `SAVE_VIRTUAL_SUPERCLASS`, and the in-source note
claimed both were "already at 100% with them" — neither had a row. The note is corrected, and so is
the Copy address it quoted (`0x823AE918`, not `0x823AEE30`).

The `$4 Load@CharBoneTwist` name had been on `0x822F1080`, whose target (`0x822F0F20`) takes a
`Symbol` and calls `SystemConfig`/`FindArray` — a `SetType`. That key is nulled and the name moved
to `0x823B85A0` (net 0 rows at 100; the 12-B thunk row moved with the name).

### 3.2 `CharacterTest::Load` (`0x823DDFC0`, 0x248)

Anonymous inside CharacterTest's own pin. Named (0 → 59.06), then written: retail has **no version
checks** (ours evaluated two `PathName` calls for the `MILO_FAIL` arguments), and **does not resolve
the two clips** — both are read as `Symbol`s and dropped, and each `ObjPtr` is released open-coded
(`ReleaseObjConcrete`). The checks and the `Clips()` lookup remain the HX_NATIVE body.

### 3.3 `Object::LoadRest` and `TypeProps::Load`

Retail LoadRest pops into LoadType's `gObjectRevs`, then calls `TypeProps::Load(bs, rev < 2)` — the
caller computes the bool (`subfc/subfe/clrlwi`), and the callee reads r5. The map spelled the callee
`?Load@TypeProps@@QAAXAAVBinStreamRev@@@Z`; it is renamed `?Load@TypeProps@@QAAXAAVBinStream@@_N@Z`
and the signature changed to match (92.61 → 100). The note is read in place: free the old pool copy
by `strlen + 1`, `bs >> len`, pool-allocate `len + 1`, `Read`, terminate (an empty note stays
`gNullStr`).

### 3.4 CharMeshHide's `Hide` reader

Retail's `operator>>(BinStream &, Hide &)` gates `mShow` on the rev CharMeshHide::Load left in the file
static (`0x82CBF384` = aggregate + 4). That logic lived in a `BinStreamRev` overload nothing called;
the live reader read `mShow` unconditionally. Overload removed, gate moved: 72.17 → 96.5. The residue
is one `addi`: retail addresses the rev field through a direct `+4` label, ours materialises the
aggregate base first.

## 4. W16-KB's mis-carves

### 4.1 Extents

`tools/carve_extent_scan.py --base` at the lane's start, filtered to KB's list. Every extent was also
read off the disassembly by hand; every absorbed fragment read 0, and nothing outside the grown span
references it.

| head | function | carve | retail extent | frags | ours compiled | row before → after |
|---|---|---|---|---:|---|---|
| `0x824F56B0` | `SplineTangent(Keys<Vector3>, int, Vector3 &)` (KB's "Keys<Vector3> helper") | 0x7C | 0x218 | 6 | 0x218 | anon → **100** |
| `0x824F62F8` | `ExpInterpolator` ctor | 0x44 | 0x5C | 1 | 0x5C | 0 → **100** |
| `0x8246D718` | `InterpVertData<Vector2, GetVertTex>` | 0x6C | 0x224 | 7 | 0x224 | anon → **100** |
| `0x8246D940` | `InterpVertData<Color, GetVertColor>` | 0x74 | 0x310 | 6 | 0x310 | anon → **100** |
| `0x824A7AA0` | `Hmx::operator*(Matrix4, Matrix4)` | 0x1A4 | 0x350 | 3 | 0x350 | 0 → 77.14 |
| `0x82433A68` | `Hmx::operator*(Transform, Matrix4)` | 0x7C | 0x2B0 | 3 | 0x3C4 → 0x2B0 | 0 → 85.61 |
| `0x822C1280` | `Normalize(const Vector3 &, Vector3 &)` | 0x60 | 0x80 | 1 | 0x5C → 0x80 | 0 → 99.38 (mpn 100) |

Identification of the anonymous three:
- `0x824F56B0` tests `size == 2` over a 0x14-stride key array, is called from `InterpVector`, and
  follows `InterpTangent` — our Key.cpp's second function. Color.cpp `#include`s Key.cpp, so Color's
  object defines it. Its last 8-byte fragment (`0x824F58C0`) sat at the head of Key.cpp's pin, so the
  Color/Key boundary moves to `0x824F58C8`, where `InterpVector` starts.
- `0x8246D718` blends 2-float keys into vertex `+0x5C` (UV) at a 0x60 vertex stride; `0x8246D940`
  is the 4-float colour twin. They follow the two Vector3 instantiations, which read 100. Our
  compiled sizes equal both extents exactly.

### 4.2 Bodies

- **`Normalize(Vector3)`** (header-wide inline in `math/Vec.h`): retail tests x, y, z against zero
  and writes zeros, otherwise scales by `1 / Length(in)`. Written so; no other row in the report
  moved. Residue: operand order of two `fmuls`. Three other spellings were measured and dropped
  (explicit z/y/x products 90.6; a named `inv` local, inert; `out = in; out *= …` 47.8 — aliasing).
- **`operator*(Transform, Matrix4)`**: ours built `Vector3` column temporaries (0x3C4). Retail stores
  each element before reloading operands, which MSVC only does when the stores go through a
  reference. Two forced-inline row helpers (`MultiplyRow`, `MultiplyPoint`; the names are ours) give
  retail's size and shape: 32.01 → 85.61. A plain 16-assignment body read 19.9 (MSVC hoisted all
  loads, since it can see `out` does not alias the inputs).
- **Residue on both products is the per-element product order**, which MSVC picks itself under
  `/fp:fast`: retail's chains run `[x,z,w,y]`, `[w,z,y,x]` or `[x,z,y,w]` by element, ours differ, and
  rewriting the terms in z, x, y order produced **byte-identical** output. Left for the permuter.

### 4.3 How it was measured

W16-KE's setup. `ab_measure` refuses a patch that touches `symbols.txt`, so the carve is its own
commit **directly on main** (`f898fd692`: `symbols.txt` + the two Color/Key `splits.txt` lines), and:

- **Run 1 (main control)**: worktree at main, comment-only patch to `CrowdRating.cpp`. Its leg A is
  the main baseline.
- **Run 2 (branch)**: worktree at `f898fd692`, patch = `f898fd692..w16-lc` (map + splits + source).
  Its leg A is main + carve; leg B is the tip.

The branch was reordered to put that commit first (the pre-order tip is kept as
`w16-lc-preorder`; `git diff w16-lc-preorder w16-lc` is empty).

## 5. Gates

Run on the tip after a forced re-split full build (clean tree, no `splits.txt`/`symbols.txt` drift;
measures equal run 2 leg B):

```
[map-injectivity] OK: 32907 applied rows, 32906 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1644 map-consistent, 271 tolerated (enumerated above), 0 contradicted, 1916 total
[patch-state] OK: 1249 decomp, 3120 target objects match 2026-10-01T16:01:46Z (tree_sha256=81900141f63bfae3)
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

On main: injectivity 32,899 applied rows (the +8 are the eight new names), validator identical
(1,644 / 271 / 0 / 1,916). No alias membership was touched.

## 6. Left

- **`Hmx::operator*(Matrix4, Matrix4)`** (848 B at 77.14) and **`operator*(Transform, Matrix4)`**
  (688 B at 85.61): per-element FP product order, scheduling-class (§4.2).
- **`operator>>(BinStream &, CharMeshHide::Hide &)`** at 96.5: one `addi` (§3.4).
- **`Normalize(Vector3)`** at fuzzy 99.38: two `fmuls` operand orders.
- **FlowValueCase `fn_823BA5FC`** (40 B, 99.3): a funclet of a Dance Central class pinned in the char
  region; it pairs by byte signature. FlowValueCase/FlowIf blocks interleaved with CharWeightable and
  CharUpperTwist look like more of the same mis-pinning (`0x823C7000–0x823C70E8`,
  `0x823C72DC–0x823C7308`, `0x823C75D4–0x823C761C`, `0x823AEF8C–0x823AEF98`); only the Load blocks
  were moved.
- **The `0x822F0xxx` thunk region** names CharBoneTwist (`??_ECharBoneTwist@@$4…` at `0x822F0EF8`)
  around a body that is a `SetType`; the class there was not identified. Only the provably wrong
  `$4 Load` key was nulled.
- **`0x823AEE98`** is mapped `?Handle@CharData@@…` and its `$4` thunk (`0x823AF220`) sits among
  CharWeightable's thunks; it may be `CharWeightable::Handle`. Not checked.
- **WorldDir native behaviour**: the newer-revision tail (0x1A–0x1D) now only exists under
  `WORLDDIR_DC3_TAIL`, so a native build reading a rev ≥ 0x1A world would stop after `SyncHUD()`.
  RB3 assets cannot carry those revisions (retail cannot read them).

## 7. Not done

- Not merged to main.
- No alias memberships added or withdrawn; `scripts/symbol_aliases.json` untouched.
- The permuter was not run.
- No Dance Central classes were converted.
