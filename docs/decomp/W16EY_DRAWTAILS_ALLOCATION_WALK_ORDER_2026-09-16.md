# W16-EY — `TrainerGemTab::DrawTails`: are the 17 register-slot charges reachable by source spelling?

**Date:** 2026-09-16 · **Branch:** `w16-ey` · **Base:** `2d9c4ead`
**Row:** `?DrawTails@TrainerGemTab@@QAAXABVGameGem@@HHMM@Z`, 888 B, unit `default/band3/game/TrainerGemTab`.
**Entry state (read from `report.json` in the worktree, ruler `name_check`):** fuzzy **99.3018** / mpn **100.0**.
**Prize:** **+888 B and +0 functions.** `mpn == 100` means the row is already in `matched_functions`;
`matched_code` keys on `fuzzy == 100` and withholds every byte. ⇒ **Δfunctions = 0 is the PREDICTED
result of a successful fix**, not a failure. Do not revert a correct fix on seeing a zero there.

## 1. Pre-registration (written and committed BEFORE the first probe build)

Whole-binary leg A, settled (zero-work `./tools/ninja-locked`, only the three CHECK edges ran):
`43,978 fns / 4,131,656 B / 40.320374 % / fuzzy 50.014150 / total_code 10,247,068` — reproduces the
brief's figures exactly.

Charged sites (graded ruler, `run_diff_inspect mismatches`, 17 of 222, all `diff_arg` register slots):

| cluster | idx | shape |
|---|---|---|
| A | 40–61 (14) | 4-rotation `f0→f13→f12→f11→f0` over {A=tick, B=startTick, C=endTick, yRange}, plus idx 58/59 subtractions in swapped order |
| C | 176/178/180 (3) | `fmuls` operand slot: retail `f31,value`, ours `value,f31` — ONLY for the `overhang` (f31, callee-saved) scalar; the `drawScale`/`scale` (f0) sites at 137–141 / 201–204 match value-first |

### Working model
Volatile FPRs are handed out from the pool `f0, f13, f12, f11` in the order the allocator *walks*
the temporaries. Retail: `{C:f0, A:f13, B:f12, yRange:f11}` ⇒ walk `C, A, B, yRange`.
Ours: `{yRange:f0, C:f13, A:f12, B:f11}` ⇒ walk `yRange, C, A, B`. The ONLY difference is where
`yRange` sits relative to the three int→float conversions. Our order is what right-to-left tuple
creation of `quotient * yRange + fStartY` predicts (right operand first). Retail's is what you get if
`yRange`'s tuple is created AFTER the quotient subtree.

### Experiments (one source edit, one full `./tools/ninja-locked`, read via `run_diff_inspect mismatches`)

| # | edit | prediction under the model | falsifier |
|---|---|---|---|
| A1 | `(endY - fStartY) * (num/den) + fStartY` | cluster A collapses 14→0 incl. idx 58/59 | byte-identical ⇒ operand order is canonicalised at tuple creation for this shape too |
| A2 | named quotient local inside the loop, `t * (endY - fStartY) + fStartY` | cluster A collapses | byte-identical ⇒ walk order ≠ statement order; REGRESSION (bigger save set) ⇒ LICM hoisted `t` (ES lever-2 shape) |
| A3 | `fStartY + (num/den) * (endY - fStartY)` | INERT (control: flat sum canonicalised) | movement ⇒ the add's operand order is live, model incomplete |
| C1 | `extra.mXfm.m.y *= overhang` (`Vector3::operator*=`, inlined) | inert | movement ⇒ the inlined param copy re-keys the canonical order |
| C3 | second syntactic spelling of `overhang` at the ExtraTail site, CSE'd back to f31 (ES's FMA trick applied to tuple order) | cluster C collapses 3→0 IF canonical order keys on tuple id | inert ⇒ canonical order keys on something else (physreg class?) |
| — | cluster C may move as a SIDE EFFECT of any cluster-A fix (same allocation-order mechanism) | — | — |

Success criterion for landing: charged sites 17 → 0 with NO new site anywhere in the unit, then
`ab_measure --from-dirty` reading **Δmatched = 0 / Δcode_bytes = +888 / Δcode% = +0.008666 pp
(888 / 10,247,068)**, unit `TrainerGemTab` functions 12→12, `matched_code` 4,748 → 5,636.
Anything that closes fewer than all 17 buys **zero bytes** and is NOT landed (all-or-nothing per row);
byte-identical results are recorded as negatives and NOT landed.

## 2. Results

Each probe: one source edit, one full `./tools/ninja-locked` (log `~/tmp/rb3_build_w16ey_<probe>.log`,
verified `MSVC .../TrainerGemTab.obj` recompiled + all 6 `PATCH` edges ran), one
`run_diff_inspect mode=mismatches project_dir=<worktree>` on the graded ruler. NOT landed unless stated.

| probe | edit | charged sites | fuzzy | verdict |
|---|---|---|---|---|
| A1 | `(endY - fStartY) * (num/den) + fStartY` | **17, byte-identical to baseline** (every register the same; `fmadds` still `f13,f0` quotient-first) | 99.3018 | INERT — multiply operand order canonicalised; `yRange` still first in the walk |
| A2 | `float t = num/den;` then `SetFrame(t * (endY - fStartY) + fStartY, 1.0f)` | **17, byte-identical to baseline**; prologue unchanged (`__savefpr_23`, `stwu -0x1e0`) — no LICM of `t` | 99.3018 | INERT — statement order does not feed the walk either; `t` is in-loop so ES's pre-loop LICM regression did not fire |
| C1 | `extra.mXfm.m.y *= overhang;` (inlined `Vector3::operator*=(float)`) | **17, byte-identical to baseline** | 99.3018 | INERT — an inlined by-value `float f` is the same operand as the local at canonicalisation; `Scale(v,f,dst)` spells `v1.x * f` (same shape) and was NOT run for that reason |

Read of A1+A2 together: our allocation `{yRange:f0, C:f13, A:f12, B:f11}` did not move when the
subexpression order in source was rotated two different ways. So "tuple creation order = source
order = walk order" is REFUTED as a model of this cluster. What cluster A actually contains, once the
relabelling is subtracted, is ONE schedule fact: retail evaluates the denominator (`C-B`, idx 58)
before the numerator (`A-B`, idx 59); we evaluate num then den. The remaining probes target that.

| probe | edit | charged sites | fuzzy | verdict |
|---|---|---|---|---|
| C3′ | `float lim = unk12c;` before the guard; `overhang = 0.1f*((xfm.v.z+scaleX10)-lim)`; second spelling `overhang2 = 0.1f*(endZ-lim)` at the three `extra.mXfm.m.y.*` sites | **52** (3 insert / 2 delete / 1 replace / 46 `diff_arg`); prologue `__savefpr_23→_22`, `stwu -0x1e0→-0x1f0`; `unk12c`→`f31`, `endZ`→`f30` held across the calls | **95.9144** | REGRESSION, three separate facts: (a) `lim`/`endZ` LICM'd into callee-saved FPRs — ES's lever-2 mechanism on a *member* load; (b) CSE did NOT merge `overhang2` back into `overhang` (fresh `fsubs f0,f30,f31` + `fmuls f0,f0,f29` at idx 171/178); (c) with `overhang` now single-consumer, OUR idx 135 contracted to `fnmsubs f0,f13,f29,f0` where retail keeps `fmuls f31`+`fsubs` — ES's single-consumer FMA rule re-confirmed from the other side, and it proves retail's `overhang` has ≥2 consumers at fuse time, i.e. the baseline spelling is right |
| A5 | `float den = C-B; float num = A-B;` (in-loop, den first) then `SetFrame(num/den*(endY-fStartY)+fStartY, 1.0f)` | **17, byte-identical to baseline** — idx 58/59 still num-then-den | 99.3018 | INERT — evaluation order of named in-loop temporaries is canonicalised away too |
| A6a | `float yRange = endY - fStartY;` inside the `if (slots & …)` block, immediately before `SetFrame` | **17, byte-identical**; `yRange` still colour `f0`, not hoisted | 99.3018 | INERT — naming the value does not change its allocation priority |
| A6b | same, but declared at the top of the loop body (dominates the slot test) | **17, byte-identical**; still `f0`, still not hoisted, def not sunk | 99.3018 | INERT — def point above the branch changes nothing; only ES's *pre-loop* placement moves it, and that moves it into the save set |
| A8 | delete `float fStartY = startY;`, use the parameter directly (2 uses) | **17, byte-identical** | 99.3018 | INERT — the copy is propagated away before allocation |
| C5 | `const float &oh = overhang;` used at the three `*=` sites | **17, byte-identical** — no spill, reference folded | 99.3018 | INERT — a reference to a register local is the same operand node |

### What cluster A actually is (read off the identical schedule)

The schedule is instruction-for-instruction identical on both sides (every `lfd`/`fcfid`/`frsp`/
`fsubs`/`fdivs`/`fmadds` sits at the same index; only the register *names* differ). Subtract the
relabelling and the four webs {C, A, B, yRange} take retail `(f0, f13, f12, f11)` and ours
`(f13, f12, f11, f0)`: **the same cyclic hand-out order C→A→B→yRange, with `yRange` allocated
LAST in retail and FIRST in ours.** Every cluster-A site, including the idx 58/59 "swap", is the
rotation consequence of that one priority difference on one value. `yRange`'s live interval is
identical on both sides (def idx 45, use idx 61), so the priority is not interval-based on the final
schedule; and six spellings that move `yRange`'s textual position, statement boundary, name, def
point and operand (A1, A2, A5, A6a, A6b, A8) leave its colour at `f0` bit-for-bit.

### What cluster C actually is

C3′ answers the mechanism question it was built for: a freshly computed scalar in **volatile `f0`**
(`overhang2`, base idx 179/181/183 `fmuls f13,f13,f0`) is placed second exactly like the baseline's
callee-saved `f31`. So our compiler puts the memory-loaded operand first whether the scalar is
callee-saved or volatile, old or fresh, named, by-value-param (C1), or reference (C5). Retail's
`f31,value` at 176/178/180 is the only register-scalar-first commutative site in the function
(constants go second at 126/127/135; `drawScale`/`scale` go second at 137–141/204–207), and no
spelling I can name reproduces it.

## 3. Verdict: NOT reachable by source spelling on the evidence — an evidenced negative

Nothing is landed. The pre-registered success criterion (17→0, then `ab_measure` reading
Δmatched 0 / Δcode_bytes +888) was not met by any probe; `ab_measure` was NOT run because the
only candidate patch is the empty patch and pricing it would be a vacuous Δ0. The worktree source
is at baseline (`git checkout --` after every probe, inside the worktree only), settle-rebuilt.

Eleven measured spellings across two lanes (ES: 3 incl. the pre-loop hoist; EY: A1, A2, A5, A6a,
A6b, A8, C1, C3′, C5) produce exactly two outcomes: **bit-identical 17 charges**, or a **strictly
larger** charge set via LICM into the callee-saved set. No spelling moved a single one of the 17
sites without adding others. The residual is one allocation-priority difference on `yRange` (14
sites) and one commutative-operand-slot difference on `overhang` (3 sites), both on an otherwise
identical instruction stream — the definition of the `diff_arg`-only stratum the house docs say
source-level reorder cannot reach (`fixable-operators.md`, Commutative Operand Order).

**What would falsify this** (so the next lane can check rather than re-hunt): any source spelling
that (i) leaves the 222-instruction schedule identical, and (ii) moves `yRange` off colour `f0`
or puts `overhang` in the first `fmuls` slot at idx 176. Candidates NOT in this lane's reach:
a different **upstream** type or signature — e.g. `unk12c`, `endY`/`startY` or the `SetFrame`
parameters having a different type in retail (a `double` anywhere in that expression would show
as `frsp`/`fmr` differences, and none exist), or a member of `TrainerGemTab` at a different
offset (every offset in the function matches). The ES/EY probes bound what *spelling* can do; they
say nothing about a compiler-side ordering keyed on symbol-table or tuple ids, which is the
permuter's domain and is OFF by directive.

## 4. Not done, and why

- **`Scale(extra.mXfm.m.y, overhang, extra.mXfm.m.y)`** — `Vec.h:237` spells it `v1.x * f` with
  a by-value `float f`, the shape C1 already measured inert. No `operator*(float, const Vector3&)`
  exists in `Vec.h` (checked), so the temporary-object shape is unavailable without header edits.
- **A3 control** (`fStartY + (num/den)*(endY-fStartY)`) — dropped after A1/A2/A5/A6/A8 showed
  every operand-position change is canonicalised; the control had nothing left to discriminate.
- **Pre-loop `int tick`** — retail loads `lwa r11, 0x4(r27)` inside the loop; a pre-loop copy would
  hoist into a callee-saved GPR (ES's mechanism, `__savegprlr` change). Not tried on that evidence.
- **`ab_measure`** — nothing to price; see §3.
- **Permuter** — OFF by standing directive; the residual is exactly its domain.

## 5. Durable lessons

1. **A rotation in a `diff_arg` register cluster is one priority difference, not N swaps.** Read
   the colour tuples as a cyclic order first; here 14 sites reduce to "where does `yRange` rank".
   That reframing is what made six spellings testable in six builds instead of guessing.
2. **MSVC canonicalises float subexpression *position* completely:** operand order, statement
   boundary, naming, def point (inside the guard / at loop top), and copy locals are all
   bit-inert at `/O1`. The only source-level lever that moves float locals in a loop is
   **pre-loop vs in-loop placement**, and it moves them via LICM into the save set (regression here).
3. **A second syntactic spelling of a CSE-able expression is NOT reliably CSE'd back** (C3′):
   `0.1f*(endZ-lim)` next to `0.1f*((xfm.v.z+scaleX10)-lim)` stayed two computations, and
   naming a *member* load (`lim = unk12c`) hoists it just like ES's `yRange`. Both directions of
   ES's single-consumer FMA rule are now measured on this row.
4. Price these rows from `report.json`'s charged-site list, and read a full-listing schedule
   diff before choosing a lever: "203 of 222 equal" hid that the schedule is *entirely* equal.

