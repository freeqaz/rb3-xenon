# The `Init()`-aggregator folds, adjudicated on retail bytes — lane W11-A, 2026-09-13

**Date** 2026-09-13 · **Branch** `w11-init-folds` off `main@77cac933` ·
**Worktree** `~/tmp/wt-w11-a` ·
**Ruler** `functionRelocDiffs=name_check` (graded), resolved at runtime from
`build/45410914/report.json` `provenance.diff_config` — never hardcoded.

## Result

| change | Δmatched_functions | Δmatched_code | predicted |
|---|---:|---:|---|
| 1. `list<T*>::insert` fold (alias, 2 spellings) | **+9** | **+4,848 B** | +9 / +4,848 — **exact** |
| 2. `0x8240e9c0` = `DOFProc::NewObject` (map) | *(see §5)* | *(see §5)* | +2 / +1,908 |

Both were pre-registered from the charge list before measuring.

## 0. The brief, and the two ways it was wrong

Three rows were briefed as *"4,760 bytes behind FIVE relocation-name charges"*:
`?BandInit@@YAXXZ` (1,008 B, 1 charge), `?PreInit@Rnd@@UAAXXZ` (1,836 B, 3),
`?Init@UIManager@@UAAXXZ` (1,916 B, 1). The charge **counts** were exactly right
and the `5/N` pricing screen reproduced to four decimals on all three
(`N = size/4`; 1×5/252 = 0.019841 vs measured 0.019844; 3×5/459 = 0.032680 vs
0.03268; 1×5/479 = 0.010438 vs 0.01044). The screen gives the count; it never
gives the identity, and the identity is where both corrections live.

**Correction 1 — the retail spelling.** `SYMBOL_HEADS_2026-09-10.md` recorded the
charged retail name on `?Init@UIManager` and `?PreInit@Rnd` as
`?insert@?$list@PAVCharClip@@…`. Today it is
`?insert@?$list@PAVObject@Hmx@@…` on **all three** rows — one pair, not two.
(`<CharClip*>` is now a *folded member* of that same group, which is probably how
the older reading arose.)

**Correction 2 — the size of the prize.** The brief priced the `insert` charge at
2,924 B over two rows. It is **4,848 B over nine**: `TheDebug.AddExitCallback()`
sits in every subsystem's `Init()` aggregator, so one fold is financed by all of
its callers — the same "one row is financed by ALL of its callers" shape
`SYMBOL_HEADS` found for a wrong map row, running here for a fold.

## 1. What the charge actually is

All three rows carry the identical pair (`tools/w25_charge_detail.py`, graded):

```
target: ?insert@?$list@PAVObject@Hmx@@V?$StlNodeAlloc@PAVObject@Hmx@@…
ours  : ?insert@?$list@P6AXXZV?$StlNodeAlloc@P6AXXZ@stlpmtx_std@@…
```

`P6AXXZ` is `void(*)()`. Our source is `std::list<ExitCallbackFunc*>
mExitCallbacks` (`src/system/os/Debug.h:40`) with
`AddExitCallback(ExitCallbackFunc*)` — semantically right. **Nothing in `src/`
was touched by this lane**; retyping the container to make the names agree was
the one thing the brief forbade, and it turns out to be unnecessary as well as
wrong.

## 2. The fold, proven — two levels and an already-landed floor

| level | our COMDAT | retail | relocations |
|---|---|---|---|
| `insert` | 100 B, sha `e0fc2b244a1fde0d` | @`0x823d14c0`, **RAW byte-identical** | exactly 1, at `+0x24` |
| `_M_create_node` | 64 B, sha `3abb9e316fdaedcf` | @`0x82520150`, **RAW byte-identical** | exactly 1, at `+0x18` |
| allocator | `MemOrPoolAllocSTL` | `MemOrPoolAlloc` | **already a landed fold**, group @`0x827bd208` |

Not merely masked-identical — *raw* identical, so the only thing that could
differ is the single relocation at each level, and each one resolves to a
type-independent target once the chain is closed. That is W7-D's rule exactly:
*a family folds iff its relocation targets are type-independent.*

Retail supplies its own tell: `insert`@`0x823d14c0`, which the map names
`<Hmx::Object*>`, calls a `_M_create_node` the map names
**`<CharPollableSorter::Dep*>`** — a third `T`. An `insert<Object*>` cannot call
a `create_node<Dep*>`; the internal inconsistency only exists because the
survivor's name is arbitrary, which is what a fold is.

Injectivity holds: neither of our two spellings appears anywhere in
`scripts/target_symbol_map.json`, and neither is placed by any existing alias
group. So no rival address is being contradicted.

### 2.1 ⛔ My first population control was VACUOUS, and it agreed with the prior

I scanned retail `.text` for the 100 B `insert` body **with the `+0x24` word
wildcarded** and got **48 addresses**. Pre-registered prediction had been 1, so
this read as a decisive refutation — and it matched `ALIAS_VEIN` §4.2's standing
refutation of this very family, which made it comfortable to believe.

It is not evidence of anything. I masked the one word that carries the
discriminator and then counted the results: two `insert`s that call *different*
`_M_create_node`s **should** be distinct, and a scan that cannot see the callee
cannot distinguish "retail refused to fold these" from "these were never
foldable". Same disease as the raw-`memcmp` trap CLAUDE.md records for ICF.

**The control that works resolves the destination.** Decoding the `bl` at
`+0x24` at all 48 addresses:

> **48 inserts → 48 DISTINCT `_M_create_node` destinations, 0 shared.**

So retail never held two byte-and-reloc-identical `insert`s apart. The family is
not "provably non-folding"; it is *exactly* per-`T` in its callee, and therefore
**`insert<T>` folds precisely when `_M_create_node<T>` folds.** The question
reduces cleanly one level, instead of being closed.

⚠ A decode bug nearly inverted this too: I read bit 0 (`LK`) as `AA`, which
produced absolute destinations like `0xfffffc84` and a bogus "3 shared
destinations". `AA` is bit 1. The tell was destinations outside `.text`.

### 2.2 The control one level down, which is the decisive one

Same scan on the 64 B `_M_create_node` body:

> **exactly 1 masked-identical body in the entire retail `.text`** — `0x82520150`
> — whose `+0x18` decodes to `0x827bd208` = `MemOrPoolAlloc`.

The whole 4-byte-POD `_M_create_node` family has **one survivor**. That is what a
fold looks like, and it is the contrast that killed the comparable candidate:
`ALIAS_VEIN` §4.4 refused `vector<T>::resize` because its population control found
masked-identical siblings **at distinct addresses**. Here there are none.

Anti-vacuity: the same scanner returns **48** for `insert` and **1** for
`create_node`, so it discriminates rather than always answering "1". And the
allocator destination was **decoded from retail bytes**, not read off the map.

### 2.3 Installation

Both groups **already existed and were missing only our spelling** — 1481
(`insert`@`0x823d14c0`) already folds `<char*>`, `<Dep*>`, `<CharClip*>`; 1476
(`_M_create_node`@`0x82520150`) already folds `<CharClip*>`. This is the
`STALE_SPELLING`-becomes-live mechanism `ALIAS_VEIN` §3.1 describes, and it is
independent corroboration: two earlier lanes proved the same fold at the same two
addresses with different instruments (the CF2 COMDAT-identity gate, and the ICF
fixpoint).

Minted with `scripts/icf_alias_fixpoint.py --grounded-only` — its strictest mode,
where *every* differing relocation slot must be resolved. It accepted in exactly
the order proved by hand:

```
round 1: +1 accepts (1 relaxed by an alias class), 1 still open   <- create_node, via the allocator group
round 2: +1 accepts (1 relaxed by an alias class), 0 still open   <- insert, via create_node
round 3: +0 accepts
DELTA: 2 groups, 2 folded names, 13 CHARGED sites
```

Both `tier: 1`, both `fully_grounded: true`, `tolerated_slots: 0`.

The `_M_create_node` spelling carries **0 charged sites** and buys **0 bytes** on
its own. It is installed because it is the relocation the `insert` fold runs
through; it was supplied to the fixpoint as a candidate (documented in the input
file), which proposes but weakens no gate.

## 3. Change 1 — measured

Pre-registered from the census, before any build: **+4,848 B, +9 matched
functions, 0 units off 100%**, and the exact 9-row crossing set.

```
Δmatched=+9  Δmasked_equal=+0  Δhonest=+9  Δcode%=+0.047317pp  Δcode_bytes=+4848
unit net (ALL units) = +9   vs whole-binary Δmatched = +9
units at 100% [mpn]:   163 -> 163  (0 reached, 0 fell off)
units at 100% [fuzzy]: 135 -> 135  (0 reached, 0 fell off)
```

The 9 improving units are exactly the 9 predicted rows: `?Init@UIManager@@`
(1,916 B), `?BandInit@@` (1,008), `?GameInit@@` (980), `?Init@Waypoint@@` (268),
`?Init@RndGraph@@` (180), `?Init@GameMicManager@@` (148), `?BandUserMgrInit@@`
(140), `?Init@UsbMidiGuitar@@` (128), `?Init@PatchDir@@` (80). Sum = 4,848.

Three rows carry the pair but do **not** cross, each blocked by exactly one other
charge — recorded in §6.

## 4. `?PreInit@Rnd@@` is NOT a fold — it is a map defect, and the vtable says so

Its remaining charge (sites 115/117, the `@h`/`@l` halves of one address-taken
relocation) is retail `?NewObject@Object@Hmx@@SAPAV12@XZ` vs our
`?NewObject@DOFProc@@SAPAVObject@Hmx@@XZ`.

`tools/s1_fold_family.py` returns **REFUTED / R-NAME**: retail@`0x8240e9c0`'s body
is identical to ours except two relocation names — `??2CriticalSection@@SAPAXI@Z`
vs `??2@YAPAXI@Z`, and `??0QuickplayPerformerImpl@@QAA@XZ` vs
`??0DOFProc@@QAA@XZ`.

★ **But that REFUTED is an instrument artifact, and it is worth stating plainly
because a prior lane's table carries it as a verdict.** `s1_fold_family --pairs`
compares relocation target names **literally**; it does not close under the
already-proven alias equivalence. *Both* of those name pairs are already-landed
folds — group 1542 (`operator_new_alloc_thunk`, 120+ members) and group 331. Under
closure the two bodies agree on every relocation. `icf_alias_fixpoint.py` is the
tool that closes; `s1_fold_family` is not. **A REFUTED from an un-closed
comparator is not a refutation.**

So the fold question had to be settled on retail bytes instead — and the answer is
that it is not a fold at all.

**Retail's own RTTI decides it.** Retail@`0x82466000` — the ctor
`0x8240e9c0` calls — stores vtable `0x8206AEB4`. Reading the `??_R4` Complete
Object Locator at `vt-4` → `0x821daa8c`, its `pTypeDescriptor` → `0x82c707d0`,
whose name string is:

```
.?AVDOFProc@@
```

A constructor that stores **DOFProc's** vtable is **DOFProc's constructor**.
Corroborated two further ways: that vtable's slots point at `0x82466240`,
`0x82466040`, `0x82466120` — i.e. the region *is* DOFProc's code; and
`Rnd::PreInit` is precisely where a DOFProc factory gets registered.

⇒ `0x8240e9c0` is `DOFProc::NewObject`, `0x82466000` is `DOFProc::DOFProc()`, and
**both map names are wrong**. This is the MPNGAP-1 shape: our source is right and
the map is not.

### 4.1 Alias group 331 is a `MAP_DEFECT_INVERTED_CONCLUSION`

Group 331 asserts `??0DOFProc@@QAA@XZ` folds into survivor
`??0QuickplayPerformerImpl@@QAA@XZ` @`0x82466000` on tier T1. Its **byte evidence
is correct and its conclusion is inverted**, exactly as `ALIAS_VEIN` §5 found for
`CharBlendBone`/`CharTransDraw`: the bytes matched not because two functions
folded but because **the address simply IS our function**.

⚠ And the reason nobody noticed: `??0QuickplayPerformerImpl@@QAA@XZ` reads
**fuzzy 100.0** today in `default/MetaPerformer`. That 100 is financed by a
**forgiven placeholder relocation** — retail's vtable slot is `lbl_8206AEB4`,
which `name_check` forgives, so the one word that distinguishes the two classes is
never charged. A 100 here is not evidence for the map name.

## 5. Change 2 — the `0x8240e9c0` rename

Exposure priced before predicting, because a map edit's delta is mostly
**un-pairing** (80.5%, per CLAUDE.md), not cascade:

* the correct name `?NewObject@DOFProc@@SAPAVObject@Hmx@@XZ` is **absent** from
  the map ⇒ no collision;
* the only report row bearing the wrong name scores **fuzzy 0 / mpn 0** ⇒ nothing
  to lose;
* **exactly one** target obj in the tree relocates to the wrong name — `Rnd.obj`,
  with the 2 sites already charged ⇒ **zero cascade risk**;
* our `Rnd.obj` defines the correct name, so the re-pair stays inside the same
  unit and no re-home is needed.

Pre-registered: `?PreInit@Rnd@@` crosses (**+1,836 B / +1 fn**), and the 72 B
`NewObject` row re-pairs and reaches 100 through groups 1542 and 331
(**+72 B / +1 fn**) ⇒ **+1,908 B / +2 fns, 0 units off**. Floor if the 72 B row
misses: +1,836 / +1.

Measured:

```
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.018619pp  Δcode_bytes=+1908
unit improvements: 1 unit(s), sum +2
    +2  default/system/rndobj/Rnd  (289->291)
unit net (ALL units) = +2   vs whole-binary Δmatched = +2
units at 100% [mpn]:   163 -> 163  (0 reached, 0 fell off)
units at 100% [fuzzy]: 135 -> 135  (0 reached, 0 fell off)
```

**Exact on both axes**, and the decomposition was confirmed by an instrument I
had not counted on. `ab_measure`'s `none`-ruler control moved **+72 B** and was
flagged `REAL_PAIRING`:

> `none` MOVED (+72 B) — a REAL pairing change, not a naming artifact: a
> first-naming of an anon address pairs a body that never paired.

`none` ignores relocation names, so it is structurally blind to the 1,836 B
`PreInit` crossing and can only see the 72 B row that genuinely *re-paired*.
+72 is exactly that row. So the two halves of the prediction — 1,836 name-only
plus 72 real-pairing — are separated by an instrument that cannot conflate them.

★ **The 72 B currently reaches 100 through alias group 331, which §4.1 refutes.**
That is not a defect in this change: under the RTTI-proven reading, `0x82466000`
*is* `DOFProc::DOFProc()`, so once handoff 1 lands and the address is renamed the
site compares equal with **no alias at all** and the row keeps its 100 for the
right reason. Group 331 is coinciding with the truth, not creating it. The
1,836 B `PreInit` crossing does not depend on group 331 in any way.

## 5.1 Branch total

| | Δmatched_functions | Δmatched_code |
|---|---:|---:|
| change 1 — `list<T*>::insert` fold | +9 | +4,848 B |
| change 2 — `0x8240e9c0` map rename | +2 | +1,908 B |
| **branch total** | **+11** | **+6,756 B** |

Both legs measured on the same pinned `objdiff-cli` (`sha256:a5c35b15d7d46ac4`),
and change 2's leg A equals change 1's leg B exactly (`matched=42775`,
`code%=37.860207`), so the two deltas compose. `matched_functions` 42,766 →
42,777; `matched_code_percent` 37.812890 → 37.878826 (**+0.065936 pp**).
Zero regressions and zero units off 100% in either run.


## 5.2 Gates, verbatim

Full `./tools/ninja-locked` (never a targeted `.obj` — the six post-compile
patchers are part of the ruler), `EXIT=0`, log `~/tmp/rb3_build_w11a.log`.

```
[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-13T06:24:35Z (tree_sha256=1c48710d1b4bfce5)
```

```
VALIDATE: PASS -- 1352 map-consistent, 241 tolerated (enumerated above), 0 contradicted, 1595 total
```

`tools/native_build_gate.sh` **not run, and not required**: the branch touches
no `src/` and no `config/` (`git diff --name-only 77cac933..HEAD` → 3 files under
`scripts/`, 1 doc; grep for `^(src/|config/)` returns 0).

Third, independent confirmation that the two deltas compose — `report.json` read
off the final built tree rather than from an A/B leg:

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| main `77cac933` | 42,766 | 3,874,292 | 37.812890 |
| branch tip | **42,777** | **3,881,048** | **37.878826** |
| delta | **+11** | **+6,756 B** | +0.065936 pp |

⚠ Pre-renamer sanity check, run *before* any name-keyed analysis, because a
fresh worktree's reflinked target objs carry the objs but not the renamer's
effect and every retail mangled name would read "absent": 3,083 target objs in
both trees, `?Init@UIManager@@UAAXXZ` present in `UI.obj`, and the build's own
`[renamed-check] 25537/29045 map names present in 3083 target objs = 87.9%`.

## 6. Handoffs

1. ⛔ **`0x82466000` is a splits mis-carve, and the map rename must not be done
   without it.** `MetaPerformer.cpp` pins `[0x82466000,0x82466070)` — a 112 B
   sliver whose nearest sibling in that unit is **1.1 MB away** (MetaPerformer's
   real cluster is at `0x8257xxxx`) — while **`DOFProc.cpp` pins
   `[0x82466070,0x82466080)`, 16 bytes, immediately after it.** DOFProc's head was
   carved `0x70` too late and MetaPerformer took it. Renaming `0x82466000` to
   `??0DOFProc@@QAA@XZ` **without** re-homing the sliver to `DOFProc.cpp` would
   leave a target row whose base obj cannot define that name ⇒ a pure −60 B.
   The correct change is one commit: re-home the sliver, rename the address,
   withdraw group 331 with `folded: []` per house convention (nothing pruned).
   Re-homing is **not** metric-neutral, so it needs its own A/B. Deliberately not
   done here.
2. **The two `insert`-pair rows still blocked, both the same shape.** Each is our
   real function against retail's tiny-STL ICF-survivor name — Group A of
   `SYMBOL_HEADS`' taxonomy, and in both cases the blocked callee is the *exit
   callback itself*:
   `default/MoviePanel ?MetaInit@@YAXXZ` (176 B) — retail
   `~_List_base<const char*>` vs our `?MetaTerminate@@YAXXZ`; and
   `default/network/net/NetSession ??0NetSession@@QAA@XZ` (632 B) — retail
   `StlNodeAlloc<_List_node<int>>::ctor` vs our
   `?DisconnectOnFail@?A0x055cc49d@@YAXXZ`. 808 B combined. Worth one adjudication
   pass: the `AddExitCallback` argument is a tiny function and tiny functions fold.
3. ⛔ **`icf_alias_merge.py --strict` is dead and nobody can use it.** It refuses on
   **82 pre-existing** names that sit at more than one address (worst: 17,
   `??$_Copy_Construct@UDebugGraph@…`), none of them from any delta. The one
   safety flag the merge tool has cannot currently be turned on. Either the 82 are
   legitimate and the check is wrong, or they are 82 latent group-merges — and the
   file has 1,595 groups, so this should be settled rather than left refusing.
4. **`s1_fold_family.py` should say so when it refuses on an already-proven pair.**
   Its `R-NAME` REFUTED (§4) is correct as "the literal names differ" and is read
   downstream as "not a fold". Closing the comparison under `symbol_aliases.json`
   — or merely *labelling* the verdict when every differing name is an installed
   group — would have saved this lane a wrong turn and would repair the
   `?NewObject` row of `SYMBOL_HEADS_2026-09-10.md`.
5. **The 51 `address: null` groups now crash two tools** (fixed here, §7) and are
   invisible to every address-keyed check. `ALIAS_DURABILITY_2026-09-13` H1 already
   flags them; they are accumulating consumers.

## 7. Tool fixes landed here

* `scripts/icf_alias_fixpoint.py` — crashed with `AttributeError` on
  `g["address"].lower()` for the 51 null-address groups, in the **reporting**
  path, *after* the adjudication had succeeded. A null address now gets a
  per-group sentinel: an unknown address can never compare EQUAL to a delta
  group's address, so this can only DROP a candidate, never admit one.
* `scripts/icf_alias_merge.py` — same crash, but here the naive repair is
  **destructive**: `by_addr` buckets *by address*, so one shared `None` key would
  have merged all 51 null-address groups into one. The sentinel is the dict **key
  only**; `dict(g)` keeps `address: None`, so the written value round-trips
  (verified 51 → 51, 0 groups lost a folded name). A delta group with no address
  is now refused outright rather than merged blind.

## 8. What this lane deliberately did NOT do

* **No `src/` change of any kind.** The container retype was refused as
  metric-fitting against a real semantic difference, and the fold proof makes it
  unnecessary. Consequently `tools/native_build_gate.sh` was not required.
* **Did not touch `0x82466000`, the MetaPerformer sliver, or alias group 331** —
  all three are one change and it needs a splits re-home plus its own A/B
  (handoff 1). Group 331 is left standing, *refuted in prose but not withdrawn*,
  because withdrawing it alone loses bytes and fixes nothing.
* **Did not run a whole-binary fixpoint sweep.** `icf_alias_fixpoint.py` was
  driven from a 2-candidate charge file restricted to this lane's pair. A
  whole-binary run would mint a large, unaudited alias wave; the delta was
  inspected group-by-group before merging.
* **Did not re-fund the `list<T*>::insert` family generally.** What is proven here
  is the *4-byte-POD* subfamily whose `_M_create_node` has one survivor. Value-`T`
  `insert`s have genuinely distinct `create_node`s (48 destinations, all distinct)
  and must not be aliased.
