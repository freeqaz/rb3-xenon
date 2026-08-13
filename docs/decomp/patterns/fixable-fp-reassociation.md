# Fixable Patterns: `/fp:fast` Reassociation — the Parentheses Are the Barrier

> **This page is a correction, not just an addition.** Two generalizations that were
> standing guidance in this tree are **refuted** below, and both had already cost real
> yield:
>
> 1. **"Under `/fp:fast` MSVC reassociates the sum chain, so term order in a sum is not
>    source-controllable"** (lane DR-3). Its *measurement* was correct and is preserved
>    verbatim in [`src/system/math/Mtx.h`](../../../src/system/math/Mtx.h); its
>    *conclusion* was false. Written into `Mtx.h` as "do not retry … ~1.2 KB is NOT
>    reachable this way", it **turned away two lanes** — MATCH-A abandoned `RndMesh`
>    citing it. All four sites it wrote off are now at 100%.
> 2. **"Named temporaries are inert"** (lane GLM-LAND-3). Measured **site-specific in
>    both directions** — they *cost* points at one site and are *worth* the last 10 at
>    another.
>
> **Provenance.** Measured in **rb3-xenon** (this tree), lane **MATCH-M**, landed
> `76b54b0e` on **2026-08-10** (three fix commits `a43eafe6`, `06906b88`, `40955fdb`, plus
> a comment-only `895eae0d`). Re-verified against the tree by lane **DOC-A**, 2026-08-13. Toolchain `/O1 /Oi /GR /EHsc` with **implicit `/fp:fast`** — the
> reassociation freedom this page is about exists *only* because `/fp:fast` is on. Settled
> whole-binary A/B, both legs quiescent: **matched 44,305 → 44,308 (+3)**, **code%
> 42.206394 → 42.214417 (+828 B)**, 0 rows fell off either ruler.
>
> ### Standing note: percentages here are point-in-time
>
> Every per-function percentage and diff score on this page is a reading taken on one repo
> at one commit. **Re-measure before citing one** (rb3-xenon's own orchestrator
> `run_objdiff`, which is the one bound to our `decomp.db`). Per
> [fixable-liveness.md](fixable-liveness.md), *a number in a commit message is not a
> measurement* — and **this page applied that rule to its own source commit and found
> three claims that did not reproduce**; see
> [Correction: the product orders in the commit message are wrong](#correction-the-product-orders-in-the-commit-message-are-wrong).

---

## The mechanism

`/fp:fast` lets MSVC reassociate a floating-point sum chain. The lever is **not** which
order you write the terms in — it is **whether you grouped them at all**:

> **MSVC reassociates only what you did NOT group. An explicit parenthesisation is a
> reassociation barrier that `/fp:fast` respects.**

`cz + by + ax + d` and `((cz + by) + ax) + d` are the *same tree* under C's
left-associativity rules, and they **compile differently**. On `Intersect(Plane, Box)` the
bare form scores 9 and the parenthesised form is byte-exact (0).

The consequence that makes this a lever at all:

| source shape | product orders reachable |
|---|---|
| bare sum `t1 + t2 + t3 + d` | **2 of 6** — most targets are simply unreachable, and four of the six source orders are indistinguishable |
| explicitly parenthesised | **all 6** |

Corroboration that a paren-ignoring reassociator could not produce: on `BSPFace::OnSide`,
right-association scores **357** while pairwise grouping scores **228** — a 60× spread.
*(Both scores are commit-sourced, not re-verified here.)*

### ⚠ But the parenthesised source order is **not** literally the emitted order

Verified by decoding retail below. Contraction into `fmadds` **reverses the first pair**:
to fuse `cz + by`, one product must be computed by a plain `fmuls` and the other folded
into the add, so the *left* source operand can come out **second**. Do not assume the
order you wrote is the order retail evaluates — decode it, then search for a source shape
that produces it.

---

## What retail actually looks like

All three sites compile to the **same four-instruction shape** — one `fmuls`, two
`fmadds`, and a **separate `fadds` for `d`** (the `+ d` is never contracted). Only the
operand order differs:

```
lfs    f0,  0x4(r30)     # plane.b            <- first product is a plain fmuls
lfs    f13, 0x4(r11)     # vec.y
fmuls  f0,  f0, f13      # b*y
fmadds f0,  f13, f12, f0 # + c*z              <- remaining products fold into fmadds
fmadds f0,  f11, f10, f0 # + a*x
fadds  f0,  f0, f9       # + plane.d          <- d is a separate fadds, never fused
```

Read the order off the chain by **pairing the plane offset with the vector offset** in each
`fmuls`/`fmadds` (`0x0`→`a`/`x`, `0x4`→`b`/`y`, `0x8`→`c`/`z`, `0xc`→`d`). The pairing is
self-validating: a correct decode always pairs `b` with `y`, `c` with `z`, `a` with `x`. If
you get `b*z`, your stack-slot mapping is shifted.

### The three sites, decoded (verified against the split asm in this tree)

| site | size | retail emitted product order | landed source shape |
|---|---|---|---|
| `BSPFace::OnSide` (`fn_824F0AF8`) | 236 B | `a*x`, `b*y`, `c*z`, `+d` | named temps, `((ax + by) + cz) + plane.d` |
| `Intersect(Plane, Box)` (`fn_824EFF90`, **both** `Dot`s) | 284 B | `b*y`, `c*z`, `a*x`, `+d` | named temps, `((cz0 + by0) + ax0) + plane.d` |
| `UIList::GetDistanceToPlane` | 308 B | `b*y`, `c*z`, `a*x`, `+d` | `((p.b*y + p.c*z) + p.a*x)`, then **`dot += p.d;` as its own statement** |

★ **Context-sensitivity is the headline.** The *identical* source — `Plane::Dot`, written
`ax + by + cz + d` — does **not** produce one order. It produces at least **two different
emitted product orders** across these sites, which is exactly why the shared definition
cannot be made to satisfy all of them and the fix has to be per-call-site.

In `Intersect` the two inlined `Dot`s must be solved **together**: the plane components are
loaded once and stay live across both (`f13`/`f12`, with `f0` reused as the `fmuls` seed for
both), so the register sharing drives the operand assignment.

### Correction: the product orders in the commit message are wrong

`76b54b0e`'s "★ decisive evidence" line claims **three** different orders — `by,cz,ax` in
Mesh/OnSide, `ax,cz,by` in UIList, `by,ax,cz` in Intersect. **None of those three triples
reproduces.** Decoded from the retail chain (see table above) the orders are `a,b,c` /
`b,c,a` / `b,c,a` — **two** distinct orders across the three sites, not three. (`RndMesh`,
the fourth TU, was not re-decoded here.)

Likewise `Geo.cpp`'s inline comment on `Intersect(Plane, Box)` states *"retail evaluates
the products in the order c*z, b*y, a*x"*; the chain decodes to **`b*y, c*z, a*x`**. The
source is nonetheless byte-exact — which is not a contradiction, it is the
`fmuls`/`fmadds` reversal above, and it is the concrete proof that **source order ≠ emitted
order**. The *qualitative* claim (same source, different orders per call site; fix must be
per-call-site) survives intact; only the specific triples do not.

---

## The method — there is no transferable recipe

Each of the three sites needed a **different combination**, so do not copy a shape. Copy
the procedure:

1. **Decode retail's product order** from the `lfs`/`fmuls`/`fmadds` chain (above).
2. **Enumerate the shape space** and measure each point:
   - term **order** (6 permutations),
   - **parenthesisation** (bare vs explicitly grouped) — *the lever*,
   - **named temporaries vs one fused expression**,
   - **statement splitting** (is `+ d` part of the expression, or its own statement?).
3. **Measure, don't reason.** The winning point differed at every site:

| site | what it took |
|---|---|
| `Intersect(Plane, Box)` | `((cz + by) + ax) + d`, **and both `Dot` sites solved jointly**. Bare, same order, scores 9; parens make it 0. |
| `BSPFace::OnSide` | named temporaries **AND** parens, **jointly** — neither alone works. Bare fused 6, fused+parens 10, named-without-parens 14, named+parens **0**. |
| `UIList::GetDistanceToPlane` | a **third lever entirely**: `dot += p.d;` as its **own statement**. Folded into the expression it scores 10. |

---

## Refutation 1 — DR-3: "term order in a sum is not source-controllable"

**What DR-3 actually measured (still true):** changing the sum order *at the definition* in
`Plane::Dot` to `ax + cz + by + d` recompiled **974 TUs** and left all four dependent rows
**byte-identical** (99.958 / 99.818 / 99.915 / 99.898).

**Why that did not mean what it looked like.** Three separate reasons, each measured
per-call-site:

1. **Term order is a weak lever on a bare sum** — 6 source orders collapse to only 2
   distinct compiled product orders.
2. **The parenthesisation is the barrier** — with it, all 6 become reachable.
3. **The fix has to be per-call-site anyway** — each inlined site wants a *different*
   product order, and one shared definition can only supply one. That is precisely why
   editing the definition measured inert across all four rows *at once*.

⇒ An inert reading at a **shared definition** is **not** evidence that the pattern is
unreachable at its **call sites**. This is the generalizable epistemic lesson.

## Refutation 2 — "named temporaries are inert"

Measured **site-specific in both directions**:

| site | effect of naming the products |
|---|---|
| `OnSide`, bare | **costs** — 6 → 14 |
| `OnSide`, with parens | **worth the last 10 points** (10 → 0) |
| `UIList` | **worth** 14 → 4 |

Note this refines, and does not contradict, the DC3-era
[FMA Expression Order](fixable-operators.md#fma-expression-order) row for
`RndLine::GetDistanceToPlane` ("dot product split into t1, t2, t3 temps"). Splitting into
temporaries *can* be the lever — it is simply not reliably one.

---

## Measured INERT — recorded so nobody re-runs them

| thing tried | result |
|---|---|
| commutative operand order **inside** one multiply (`c * z` vs `z * c`) | **8/8 identical** (reproduces DQ-3) |
| **declaration order** of the temporaries | **12/12 identical** — it is a **stack** lever, not a register one (see [CLAUDE.md](../../../CLAUDE.md) and [fixable-liveness.md](fixable-liveness.md)) |
| all 6 term orders of the bare fused sum, `UIList` | all 14 |
| all 6 term orders with named temporaries, `UIList` | all 4 |

### Also tried and rejected

| variant | score |
|---|---|
| `plane.Dot()` / `p.Dot()` (leave it inlined) | 14 |
| `d` summed first | 6 |
| compound assignment | 4 / 24 |
| `const Vector3 &` alias | 10 |
| pointer access | 10 |
| `float *` plane indexing | 14 |
| **`Vector3` copy** | **catastrophic — 1957 / 74.6%** |

---

## The constraint: do NOT fix these by editing the shared definition

`Plane::Dot`'s current form — explicit temporaries in `a`, `c`, `b` evaluation order — is
**load-bearing for `RndDrawable::CollidePlane`** (100% with it, **82%** fused). **Verified:**
the MATCH-M merge left `Plane::Dot`'s code lines **unchanged** (its `Mtx.h` diff is
comment-only), so that constraint still binds.

⇒ **Per-call-site hand-expansion is the only zero-blast-radius lever here.** Editing the
definition also cascades ~974 TUs, so a wrong guess is expensive to even measure.

---

## Status: this vein is DRAINED

DR-3's "~1.2 KB" was wrong — 1,212 B **included `RndMesh`'s 384 B, already harvested** by
GLM-LAND-3. The real remaining pool was **828 B**, and MATCH-M took all of it
(284 + 236 + 308, reconciling exactly with the A/B's +828 B).

The remaining low rows in this family have **different root causes** — do not open them
expecting this pattern *(re-measure before citing; readings taken on this tree)*:

| row | mpn / fuzzy |
|---|---|
| `Intersect(Triangle, Box)` | 80.04 / 76.19 |
| `Intersect(Segment, Sphere)` | 91.37 / 88.55 |
| `kdTree<Triangle>::Intersect` | 90.03 / 89.43 |

⚠ `kdTree::Intersect` lives in the **`AmbientOcclusion`** unit, not Geo/Mesh (the merge
commit files it under "Geo/Mesh rows").

---

## Telling this pattern apart from its neighbours

Four distinct FP axes are documented in this tree; they are easy to confuse:

| axis | question it answers | doc |
|---|---|---|
| **Reassociation** (this page) | *In what ORDER are the products accumulated?* | this page |
| **Contraction** | *Is `a*b + c` one `fmadds` or a separate `fmuls` + `fadds`?* | [fixable-fsel-fma.md](fixable-fsel-fma.md#fma-control-via-pragma-fp_contract) |
| **FMA variant** | *`fmsubs` or `fnmsubs`?* | [fixable-operators.md](fixable-operators.md#fma-expression-order) |
| **Branchless select** | *`fsel` or a compare-and-branch?* | [fixable-fsel-fma.md](fixable-fsel-fma.md#fsel-via-clampminmax-templates) |

**Signature of *this* pattern:** a function that is already ≥99% with the *right*
instruction shape (`fmuls`, `fmadds`, `fmadds`, `fadds`) but the **wrong operands in each
slot** — i.e. the products are all present and all correct, just accumulated in a different
order. If instructions are missing or extra, you have a contraction or variant problem
instead.
