# W26-MAPLEADS2 — three adjudicated-but-unshipped leads: one shipped, one priced-refused, one instrument

**2026-08-17, branch `w26-maplead`, worktree `~/tmp/wt-w26maplead`, from `9ea51d6d`.**

Baseline measured in-worktree, never inherited, ruler `name_check` read from
`report.json`'s own `provenance.diff_config` (22 keys):
**44,503 fns / 3,756,568 B / 36.398514%**, honest 21,593, `total_code`
10,320,664, `total_functions` 69,226.

**Shipped: +92 B / +1 fn / +1 honest, `none` control +92 B — all five
pre-registered lines EXACT.** Commit `4837ea6d`.

**No `src/**` was touched**, so the native gate was not required and was not run.

---

## Lead 1 — `0x826dc4c0` — SHIPPED, and every pre-registered line landed exact

W21 proved the map name wrong and deliberately did not ship it, on the standing
rule that *proving a name wrong does not make renaming it safe*. This lane found
the replacement, verified the destination obj can define it, and shipped a
**re-home** (not a rename).

### The identity, proved six ways, none of which reads the map for the assignment

| # | line of evidence | result |
|---|---|---|
| 1 | **body, ours** — of **3,903** distinct `_Rb_tree` COMDATs in our build, how many are masked-body-identical to retail@`0x826dc4c0`? | **exactly 1** |
| 2 | **body, retail** — of **674** retail functions of the same `0x5C` extent, how many carry that masked body? | **exactly 1** (itself) |
| 3 | **geometry** — `DeployCountTracker.cpp` owns `0x826DC128–0x826DC4BC` and resumes at `0x826DC51C` | the island sits **exactly in the hole** |
| 4 | **family completion** — the map already carries `_M_create_node` `0x826dc520`, `_M_insert` `0x826dc5e8`, `insert_unique` `0x826dc6c8`/`0x826dc8a0`, `operator[]` `0x826dcc70` | **`_M_erase` is the one missing member** |
| 5 | **sibling layout** — `StreakTracker::PlayerStreakData` has `_M_erase` `0x826df010` → `_M_create_node` `0x826df070` | same **0x60** stride as `0x826dc4c0` → `0x826dc520` |
| 6 | **defining obj** — `DeployCountTracker.obj` | defines it at exactly retail's **92 B** |

The tree is
`_Rb_tree<TrackerPlayerID, pair<const TrackerPlayerID, DeployCountTracker::PlayerDeployData>>`.
Node `0x28` = 40 B = 16 B header + a **24-byte** `value_type`, which is what
W21's `li r3,0x28` vs our `li r3,0x24` demanded and which
`pair<const u16, RndFont::CharInfo>` (20 B) cannot be.

⛔ **My own prediction was refuted, and that is the useful part.** `_M_erase` is a
recursive deallocating tree-walk with **no value-destructor call**, so I expected
a degenerate ICF-twin set and predicted *many* body matches. There is **one**, in
our build and in retail. The fold group is not degenerate here, so W20's
dispersion trap does not apply — and the direction matters: our two 92 B bodies
differ in the `li r3` immediate, and **different-size COMDATs cannot fold**, so
this is a genuinely wrong name rather than an arbitrary ICF survivor.

### Why it had to be a re-home, and why the wrong name survived so long

`cascade_price` reported **`Font.obj` BLOCKED** — it cannot define the
replacement, so an in-place rename sends the row to 0% **permanently** (W9's
−180 B failure mode). Re-homed the 92 B block `Font.cpp → DeployCountTracker.cpp`
and lifted the replacement spelling **verbatim** from the obj's symbol table. That
verbatim rule earned itself immediately: my hand-typed attempt had
`_Rb_tree_node_base@3@` where the real symbol has `@2@`.

★ **Why the defect was invisible to `name_check` for so long:** the row's only two
relocations are the **recursive self-call** — self-consistent under *any* name —
and `?MemOrPoolFreeSTL@@YAXHPAX@Z`, a **non-template** function the map already
names correctly at `0x827bca50`. So the wrong name was **never charged**; only the
node-size immediate betrayed it, which is why the row sat at `fuzzy` 99.95652
rather than looking broken. *A wrong name on a self-recursive function is
structurally cheap to the grader.*

Riskless, established rather than assumed: the row earned **0 bytes and 0 matched
functions** before the edit (`fuzzy` 99.95652 / `mpn` 99.95652).

### Predicted vs measured — five for five

| line | predicted | measured | |
|---|---:|---:|---|
| graded | +92 B | **+92 B** | ✓ |
| `none` control | +92 B | **+92 B** | ✓ |
| Δfns | +1 | **+1** | ✓ |
| **cascade** | **+0** | **+0** | ✓ |
| **pairing** | **+92 B** | **+92 B** | ✓ |

legA 44,503 / 3,756,568 / 36.398514 → legB 44,504 / 3,756,660 / 36.399403.
`masked_equal` unchanged 22,910; units at 100% unchanged (255 mpn / 132 fuzzy);
`renamer_patched=1825`; both legs at a `symbols.txt` split fixed point; unit
improvements `DeployCountTracker 28→29`, and **unit net +1 == whole-binary +1**.

★ **The cascade line landed exact because it was STRUCTURALLY zero, not because it
was guessed well** — W21's rule (block the cascade rather than estimate it). The
only other row relocating against the address is `fn_826DC598` (anonymous, 0%),
which cannot move in either direction.

★ `none` moved **+92, not 0**, and that was pre-registered. Per W24 this is the
**PAIRING signature**: a re-home changes which base obj is consulted, and `none`
forgives relocation *names* but not an **absent base symbol**. Flatness in `none`
would have been the ALIAS signature.

✅ **Independently confirmed on a third instrument:** re-running
`rbtree_body_anchor.py` after the edit moves `SHAPE_MISMATCH` **16 → 15** and
`BODY_IDENTICAL` **215 → 216** — exactly the one row, on a tool that knows nothing
about the A/B.

---

## Lead 2 — `BandCamShot` / `HamCamShot` — REFUSED, with the numbers

### The anchor applies, it fires cleanly, and it confirms an existing in-tree claim

| probe in retail `band.exe` | count |
|---|---:|
| `.?AVBandCamShot@@` | **1** |
| `.?AVHamCamShot@@` | **0** |
| ASCII `BandCamShot` | **4** |
| ASCII `HamCamShot` | **0** |
| control `.?AVBandDirector@@` (polymorphic sibling) | 1 |
| control `.?AVFaderGroup@@` (the known non-polymorphic false negative) | 0 |

`BandCamShot` is `Object`-derived and therefore polymorphic, so the RTTI anchor is
applicable — and the result does not even depend on that, because the plain
class-name string `HamCamShot` occurs **zero times in the whole 14 MB binary**.

⇒ **RB3 retail's class is `BandCamShot`; `HamCamShot` does not exist in retail.**
⚠ **This CONFIRMS an in-tree claim rather than discovering one** —
`src/system/world/CameraShot.h:281` already says *"Retail RB3 has no HamCamShot"*,
and `src/system/bandobj/BandCamShot.cpp:856` deliberately
`#include "hamobj/HamCamShot.cpp"` as a scatter-include so `BandCamShot.obj`
emits both spellings. Read the in-tree record first.

### Why the map-only fix is refused

15 map rows are spelled `HamCamShot`, all pinned to `BandCamShot.cpp`,
**3,600 B total, of which 9 rows / 1,308 B are at `fuzzy == 100` and earning
today.** All 15 *do* pair, because the scatter-include makes `BandCamShot.obj`
define both spellings.

`cascade_price` estimates the 15-row rename at **cascade +1,652 B**
(GAIN +1,772 / LOSS −120), with a **local channel of −504 B** (two rows fall).
That looked like a shippable lead. It is not, for three reasons — and the
decomposition of the gain is the point:

| mechanism | bytes |
|---|---:|
| **GENUINE wrong-map-name repair** — our side already spells `BandCamShot` at the charged site | **+1,196** |
| **ALIAS-GROUP CAPTURE** — clears only because the post-rename name is the survivor of an existing alias group | **+576** |

1. ⛔ **4 of 15 rows' `BandCamShot` twin is ALREADY MAPPED at another address**
   (`0x824d1070`, `0x824d1280`, `0x822b55e0`, `0x822b3870`), so the rename puts
   **two addresses on one symbol**. The +576 B is precisely this: `0x822b7d30`'s
   twin is `?insert@?$list@PAVBandCamShot@@…`, which is **`symbol_aliases.json`
   group 85's survivor at `0x822b55e0`**. Those bytes are collected by pulling a
   second address into existing forgiveness — *adding an alias wearing a rename
   costume*, which the standing rule forbids and which the `none` control cannot
   catch.
2. ⛔ **The largest member blocks a coherent family rename.** `0x822b4298`
   (`PropSync`, 1692 B): our `BandCamShot`-spelled twin is **1604 B / 180 relocs**
   against retail's **1692 B / 194 relocs**. W25's rule — **different-size COMDATs
   cannot fold** — says our two class definitions genuinely diverge (~88 B), so a
   coherent rename would pair that row against the **wrong body**. The Ham/Band
   split is self-consistent only as a whole, and this member cannot join it.
3. The remainder is a partial rename, which breaks self-consistency: the two
   local FALLS (−396, −108) are caused by renaming *other* members of the same
   family.

⇒ **The accurate fix is a SOURCE fix** — reconcile `BandCamShot::Target` with
`HamCamShot::Target` (the 88 B of `PropSync` divergence), drop the scatter-include,
then rename the whole family. That is a `src/**` change needing the native gate and
is out of proportion to a ≤+1,196 B genuine prize. **Well-specified next lane.**

### ⛔ An instrument defect found in `cascade_price`, in the place it matters

`cascade_price`'s local verdict is **`PAIRS (obj defines the new name)`**, which
checks that our obj *defines* the symbol — **not that the defined body is the same
size as retail's**. When our obj defines **both** spellings (the scatter-include
case), a rename **swaps which body objdiff compares**, and the tool models the row
as unchanged. On `0x822b4298` it prints `no movement` for a row that would pair a
1604 B body against a 1692 B target. **Same disease as W20's "right bytes, wrong
mechanism": the verdict is not wrong about definition, it is silent about
identity.** Suggested fix: report the defined body's size next to retail's extent
and flag a mismatch.

### ⚠ A limitation of my own masked-body test, found by running it

On `0x822b2e20` (432 B, **42 relocations**) the test reports `DIFF` for a row that
scores `fuzzy 100`. The mask blanks **branch displacements only**, not relocated
data words, so on relocation-dense bodies it produces **false DIFFs**. It is
trustworthy only on low-relocation functions — which is exactly why it was
decisive on Lead 1's 2-relocation `_M_erase` and is not here. Do not carry a
`DIFF` verdict from this test on a reloc-heavy row.

---

## Lead 3 — the untestable `_Rb_tree` components — a third instrument, and an honest bound

### Re-derived, not inherited

`rbtree_body_anchor.py` on this tree: 251 mapped tree rows → 216
`BODY_IDENTICAL`, 15 `SHAPE_MISMATCH`, 9 `SIZE_MISMATCH`, **9 `NO_OUR_COMDAT`**,
2 `NO_EXTENT`. (W21's "4" is its `NO_NODE_FN`∩`NO_OUR_COMDAT` subset; the full
untestable class is **9**.)

**The class is 9 rows / 1,272 B = 0.0123% of `total_code`, and every one of them
is at `fuzzy 0.0000` — entirely unpaired.** Settling them therefore buys
**zero bytes directly**; the payout is accuracy and bug exposure (MAPID-1), not
metric.

⚠ **This class IS the population where the ungated `fuzzy == mpn` certificate is
vacuous** — `0 == 0` is trivially true for all 9. Gate that test on `fuzzy > 0`.

### The third instrument: drop the same-NAME requirement, keep the LAYOUT

Both existing instruments compare retail against **our COMDAT of the same mangled
name**, so a name our build never instantiates is silent by construction. Replace
the reference: search **all 4,744** of our tree COMDATs for any that is
masked-body-identical at the same extent — i.e. any **node-layout-equivalent**
tree, regardless of spelling. This needs no new compilation.

| row | size | layout-equivalent COMDATs | verdict |
|---|---:|---:|---|
| `0x822e8a78` `_M_insert@map<Symbol,Symbol>` | 420 | **0** | ⚠ **flagged** |
| `0x822a0f38` `operator<<(map<int,float>)` | 100 | **0** | ⚠ **flagged** |
| `0x822ea818` `insert_unique@map<Symbol,Symbol>` | 472 | 23 | consistent |
| `0x823d9628` `_M_create_node@map<Symbol,Symbol>` | 92 | 3 | consistent |
| `0x826d98d8` `_M_find<int>@map<int,SongStatus>` | 88 | 11 | consistent |
| `0x82b992a0` `_M_find<Symbol>@map<Symbol,bool>` | 88 | 27 | consistent |
| `0x8256aef8` / `0x8260d168` / `0x826101b8` | 4 ea | **226** | ⛔ **VACUOUS** |

**Reached: 6 of 9 (1,260 B).** Two carry a defect signal worth adjudication
(520 B); four are shape-consistent with the declared key type.

★ **The instrument's ability to FAIL is demonstrated, not asserted** — 2 of the 6
return zero hits, so it is not a test that confirms whatever it is pointed at.
⛔ **And its blind spot is reported rather than rounded away:** the three 4-byte
rows are a bare `blr`, which matches **226** COMDATs. A 4-byte body carries no
layout information at all. Those 3 (12 B) remain **structurally unreachable by any
body-comparison instrument**, and the only thing that would settle them is
semantic (callers/geometry), not bytes.

**What it would take to settle the remaining 2 flagged + 3 vacuous:** a
**probe TU** that force-instantiates `map<Symbol,Symbol>`, `map<int,String>`,
`map<int,UIComponent*>`, `map<int,SongStatus>`, `map<Symbol,bool>`,
`map<int,float>` and is compiled **into a scratch dir only**. ⛔ It must never be
wired into `objects.json`: doing so would buy pairable rows at 0% with no content,
which is `ForceEmit_*`-class metric fitting. Given the class is worth 1,272 B of
currently-unpaired code, **this is correctly priced as accuracy work, not a byte
lever.**

---

## What I did NOT do, and why

* **No alias added or withdrawn**, including the group-85 expansion that would
  legitimise Lead 2's +576 B. Adding forgiveness lifts the score by construction
  and the `none` control reads +0 there by definition.
* **No `src/**` touched** ⇒ native gate not required and **not run**.
* **Lead 2 not shipped in any partial form.** A subset rename is available and
  would probably measure positive; it is refused because the gain is partly
  duplicate-name/alias capture and the family cannot be renamed coherently while
  `0x822b4298`'s body diverges.
* **The 3 four-byte `NO_OUR_COMDAT` stubs were not adjudicated** — no
  body-comparison instrument can reach a `blr`, and inventing a semantic one was
  out of budget.
* **The probe TU was specified but not built.** It is the concrete next step for
  Lead 3 and it is cheap; I stopped at sizing because the class is 0.0123% of
  `total_code` and pays no bytes.
* **`cascade_price`'s size-blind `PAIRS` verdict was recorded, not fixed** — out
  of lane scope, but it is live and it will mislead the next scatter-include case.

## Reproducing

```bash
python3 tools/cascade_price.py price --project-dir <wt> --edit-file /tmp/edits.json
python3 tools/rbtree_body_anchor.py --json /tmp/anchor.json     # --selftest to prove it can fail
python3 tools/ab_measure.py --worktree <wt> --from-dirty
```
