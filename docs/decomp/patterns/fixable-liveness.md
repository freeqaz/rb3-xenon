# Fixable Patterns: Liveness and Scheduling (the real levers behind REGISTER_SWAP)

> **This page is a correction, not just an addition.** The standing guidance for a
> register-swap residual — [Variable Declaration Order](fixable-declarations.md#variable-declaration-order)
> — is the right lever for the *wrong* pattern. Measured on this toolchain, declaration
> reorder was **inert** for register-only swaps: 12+ hand variants produced *byte-identical*
> `.obj` output and a beam-search permuter sweep returned zero improvements. What moved the
> registers was **liveness** (what stays live across a call) and **scheduling** (where a
> value is materialised relative to its consumer).
>
> **Provenance.** The five levers below were measured in **dc3-decomp**, 2026-08-02/03,
> n = 3 functions plus one later. That is the sister MSVC X360 project and it compiles with
> the **identical** `base` cflags to ours — `/nologo /wd4355 /wd4164 /c /GR /O1 /Oi /EHsc`
> (`config/373307D9/config.json` vs our `config/45410914/config.json`) — so the codegen
> mechanism transfers. What does **not** automatically transfer is the *target* side: DC3's
> target is a dev/debug build, ours is retail with ICF. Treat the mechanism as established
> and each per-function number as DC3's, not ours.
>
> ### Standing note: percentages here are point-in-time
>
> **Every per-function percentage on this page is a reading taken on one repo at one
> commit. Re-measure before citing one** — for **dc3-decomp** figures with
> `mcp__orchestrator__run_objdiff`; for anything in *this* tree with rb3-xenon's own
> orchestrator (`scripts/orchestrator/mcp_server.py::_run_objdiff`), which is the only
> one bound to our `decomp.db`. Two ways these numbers rot:
>
> 1. **Neighbours drift.** A match% can move when a *different* function in the same
>    translation unit changes; inlining, ICF and `.text` layout are all TU-wide.
> 2. **The number in a commit message is not a measurement.** `RndText::SizeCheck`
>    carried a fabricated **99.1%** on this page for two days, sourced from a DC3 commit
>    subject line rather than a diff. See the correction box in Lever 3.
>
> **Always name the repo next to a number.** dc3-decomp, rb3-xenon and rb3 share the
> Milo engine, so the *same symbol name* exists in all three with different code and
> different match%. An unattributed figure will be refuted against the wrong tree.
>
> **One lever has been measured here and came back negative** — see
> [Local control: Lever 1 on our `ObjectDir::Iterate`](#local-control-lever-1-on-our-objectdiriterate).
>
> **⚠ Read [Triage Split](#triage-split-statement-level-vs-within-one-expression) first if you
> are picking functions to open.** A 31-function AT_LIMIT sweep found register swaps were
> symptoms in **100% of cases** — but that the *cause* is frequently **not** liveness. It is
> just as often control flow, an inline-level count, or a signed/unsigned compare. The robust
> claim on this page is **"do not chase the register"**, not "the cause is always liveness".

---

## The rule

Register swaps are **symptoms**. Nobody permutes a register name; the whole swap set flips
at once when the underlying cause is fixed, because register assignment is downstream of
the interference graph and the interference graph is downstream of live ranges and schedule.

The boundary that matters:

| You change… | It moves… | Diff signal it addresses |
|---|---|---|
| **Scoping / packing** — which block a declaration lives in, braces, named-vs-temp | **Stack slots**: frame size, slot order, packing | `OFFSET_SWAP`, `[off:-N]` on `r1`, `mode=stack-layout` SHIFTED/SWAPPED rows |
| **Liveness / scheduling** — what is carried across a call, where a value is computed | **Registers**: which callee-saved register holds what, how many are saved | `REGISTER_SWAP` clusters, `__savegprlr_NN` delta |

If your residual is register swaps, declaration *order* is the wrong axis — reach for Levers
1-3. If your residual is offset shifts on stack locals, declaration *scope* is the right
axis — Lever 4, and its inverse Lever 5.

**`PROLOGUE_MISMATCH` is not floor evidence.** A `__savegprlr_NN` delta is the fingerprint
of a value held across a call — i.e. a *live-set* difference, which is exactly what Lever 2
fixes. `PERMUTER_ROI_ANALYSIS.md` lists `PROLOGUE_MISMATCH` under "when NOT to bother";
that holds for the `_RtlCheckStack12` / `alloca` sub-case, not for a save-count delta.

---

## Lever 1 — Live-range shortening: read the args back out of the aggregate you just built

**Impact:** +0.6% (99.4% → **100%**) in DC3 · **byte-identical here** (see control below)
**Success Rate:** unknown (1 for 1 in DC3, 0 for 1 here)
**Time:** 5 minutes once diagnosed

### Symptom

A loop body calls a function while several callee-saved values are live. The diff is a pure
register **rotation** across the whole function with **no instruction-stream change** — same
opcodes, same order, same count, only operand registers differ.

DC3's `ObjectDir::Iterate` at 99.4%: a 4-cycle rotation over `{var, b, arr, s2}`, 17 swapped
registers, 153/153 instructions otherwise equal, both sides 612 bytes. A **rotation** (not a
2-cycle swap) is the tell that the *set* of simultaneously live values differs, not just
their colours.

### Why it works

`key` is built from `first` and `s2` on the line before the call, and MSVC hoists the
`std::make_pair` store into the loop preheader. Reading the call's arguments out of `key`
rather than out of the locals ends `s2`'s live range **at the pair store** instead of
carrying it across the `IsASubclass` call inside the loop. One fewer value live across the
call = a different interference graph = the target's colour→register mapping.

The edit is a provable no-op: `key` is constructed on the preceding line and neither
component is modified in between.

### Fix

```cpp
// BEFORE — carries s2 across the call
bbb = IsASubclass(first, s2);

// AFTER — s2's live range ends at the pair store
bbb = IsASubclass(key.first, key.second);
```

### Local control: Lever 1 on our `ObjectDir::Iterate`

Applied verbatim to `src/system/obj/Dir.cpp` here (`dd144927`, control leg): **byte-identical
`.obj`**, and the parent's EH funclet `fn_8274FEC8` stayed at 100.0%. So the lever is real
but not universal — our `Iterate` and DC3's are the same source shape at different match
levels, and ours had no slack on that axis. **Record this as the calibration point: a
byte-identical result from Lever 1 means the live set was already what the target's is, not
that the lever is broken.**

### Detection

- `run_diff_inspect mode=regswaps` shows a rotation of length ≥ 3, not a 2-cycle.
- `mode=clusters` shows the swap spanning the whole function with zero insert/delete
  clusters — instruction counts and sizes identical.
- The candidate value is one that is *both* consumed by a pre-call expression *and* passed
  to the call: it is redundantly live if the pre-call expression already stored it somewhere
  addressable.

---

## Lever 2 — Call through the cached local; don't re-load the member at the call site

**Impact:** +4.0% (92.7% → 96.7%) in **dc3-decomp**
**Success Rate:** unknown (1 for 1)
**Time:** 5 minutes

> **96.7% is an intermediate, not `FitTextScroll`'s final figure.** Lever 4 below takes
> the same **dc3-decomp** function 96.7% → **98.2%**, which is where it stands on that
> repo's `main` (re-measured 2026-08-04). Cite 98.2% for "where `FitTextScroll` is".

### Symptom

The function already has a local caching a member pointer, but a later call site spells the
member path again. The re-load costs a whole callee-saved register — visible in the prologue:

```
target: bl __savegprlr_22
base:   bl __savegprlr_23      <-- we burn one more callee-saved GPR
```

…and that single extra register cascaded into ~40 register swaps across three pairs
(`r27`↔`r28`, `r22`↔`r23`, `f12`↔`f13`) in DC3's `RndText::FitTextScroll`.

### Why it works

The target keeps the member in **one** callee-saved register across the intervening call and
dispatches through it. Re-loading forces the compiler to keep the *base* (`this` / the array)
alive across the call **as well as** whatever it needed the local for, so the allocator has
one more simultaneously-live value. The `__savegprlr_NN` delta is the cheapest possible
confirmation that this is a live-set problem and not a colouring problem.

This is the mirror image of
[Pre-Compute References Before Clobbering Calls](fixable-declarations.md#pre-compute-references-before-clobbering-calls):
that pattern says *create* the local before the call; this one says *use* it after the call.
Creating the local and then not calling through it is the worst of both worlds — you pay for
the local's live range *and* for the reload.

### Sub-lever: drop `= 0` / `= 0.0f` on pure out-params

A pure out-param the callee writes on every reachable path gets **no** init store in the
target. An initialiser adds a store the target does not have and gives the value an
artificially early live-range start, which can pull it into a different colour.

Check the callee's contract first: it must write unconditionally. If it writes conditionally
the initialiser is load-bearing and removing it is a real bug, not a match win — see
[harmful-avoid.md: Constructor Zero-Init That Doesn't Exist in Target](harmful-avoid.md#constructor-zero-init-that-doesnt-exist-in-target).

### Detection

1. `__savegprlr_NN` / `__restgprlr_NN` differ by one or two → the live *set* differs.
2. Grep the function for a member path spelled out at a call site while a local already
   caches it.
3. Grep for locals initialised at declaration whose first real use is as an out-param.

---

## Lever 3 — Fix the schedule first, then the comparison polarity

**Impact:** +2.1% (96.5% → **98.6%**) in **dc3-decomp**
**Success Rate:** unknown (1 for 1)
**Time:** 15 minutes

> **Corrected 2026-08-04: this was documented as 99.1%; it is 98.6%.** The 99.1% was
> never a direct measurement — it originated in **dc3-decomp** commit `0c2b0c38`'s own
> subject line and was copied outward, into this file among others. Direct
> `run_objdiff` on `?SizeCheck@RndText@@IAAXXZ` in **dc3-decomp**: 96.5% at the parent
> `f0275669`, **98.6%** at `0c2b0c38` itself, **98.6%** on `main` today. The hypothesis
> that it hit 99.1% in isolation and was later perturbed by a sibling lane's change to
> the same `Text.cpp` was tested at `0c2b0c38` with a from-scratch rebuild and
> **refuted**. See dc3-decomp `docs/decomp/patterns/fixable-liveness.md` Lever 3 for the
> full table. **None of these figures are rb3-xenon measurements** — this function has
> not been measured in this tree.

### Symptom

Float register swaps (`f30`↔`f31`, `f12`↔`f13`) around a compare. Tempting to read as "FPR
colouring, unfixable". The actual cause in DC3's `RndText::SizeCheck` was that a product was
computed *inside* a later `if` condition while the target computed it earlier, so the
multiply landed in a different scheduling slot and the compare consumed a different register.

```
target: fmuls f12, f30, f1      ; product computed BEFORE the compare that consumes it
        fcmpu cr6, f13, f0
        bge   ...
base:   fcmpu cr6, f0, f12      ; operands reversed; product materialised inside the cond
        ble   ...
```

### Why it works — in this order

1. **Scheduling.** Collapse the operands into one named product so the `fmuls` sits at the
   statement position the target has it in, ahead of the `fcmpu` that consumes it. *Do this
   first* — it is what moves the FPRs.
2. **Polarity.** *Then* flip the compares to the target's operand order. `a <= b` and
   `b >= a` are exact equivalences **including NaN** (both false when either operand is NaN).
   `a <= b` → `!(a > b)` is **not** equivalent — do not do that.

With the schedule fixed, all nine swaps fell out automatically.

Note these were *volatile* FPRs (`f0`–`f13`), which the detector labels `RarelyHandFixable`.
Hand analysis closed them anyway — that label is not "hand-editing won't work".

### Detection

- FPR swaps clustered *around* an `fcmpu`, with the arithmetic feeding the compare at a
  different instruction index in target vs base (`mode=mismatches` with `full_listing`).
- Order matters: flipping the compare **before** fixing the schedule just moves the swap to
  the other side of the compare and looks like a wash.

See [fixable-operators.md: Comparison Operand Order](fixable-operators.md#comparison-operand-order)
and [fixable-control-flow.md: Branch Polarity Steering](fixable-control-flow.md#branch-polarity-steering-beqbne-blebge)
for the polarity half in isolation.

---

## Lever 4 — Scope a Declaration Into the Block That Uses It (stack lever, not a register lever)

**Impact:** +1.5% (96.7% → **98.2%**) in **dc3-decomp**; killed 14 offset diffs at once.
Second of `FitTextScroll`'s two levers, so 98.2% is that function's final figure.
**Success Rate:** unknown (1 for 1)
**Time:** 5 minutes

Here for contrast: a **scoping** change that moved **stack slots** and no registers, exactly
as the rule at the top predicts.

### Symptom

`mode=stack-layout` shows a run of SHIFTED slots — a group of locals all displaced by the
same delta — with no register differences in the same region.

### Why it works

MSVC assigns stack homes per lexical scope and packs same-scope locals together. Declaring
two locals inside the `if` block that uses them puts them in the same inner scope, so they
pack adjacently instead of each claiming a slot in the outer frame region.

**Check the block is faithful, not a hack.** In DC3's case the target genuinely branched past
the whole measurement block on the null path (assert-fail path ends in a `b` to the join
point rather than falling through), so the `if` reproduced real control flow.

### Detection

`mode=stack-layout` SHIFTED rows with a uniform delta over a contiguous set. Confirm the
residual is offsets and not registers first — scoping will not move a register swap.

---

## Lever 5 — Name the temporaries so they are built up front and frame-packed

**Impact:** +31.8% (68.1% → **99.9%**) in **dc3-decomp**
**Success Rate:** unknown (1 for 1)
**Time:** 30 minutes

> **The baseline is 68.1%, not 80.4%.** 80.4% was itself a match-hack state (see
> [Match-hack smell](#match-hack-smell) below), so measuring against it reported +19.4%
> for what is really **+31.8%**. Ladder in **dc3-decomp**: 68.1% → 90.6% → **99.9%**.
> **Not an rb3-xenon measurement.** This tree has its *own* `LabelShrinkWrapper::UpdateAndDrawWrapper`
> — same symbol name, different binary — sitting at 0.00% (a stub) per
> `docs/plans/lane-bo3-uilabel-layout-2026-07-29.md`. Do not conflate the two.

The inverse of Lever 4. Lever 4 *narrows* a scope so locals pack. This one *widens* the live
range of unnamed temporaries — by naming them — so the frame packer sees them at all.

### Symptom

A run of calls each taking a by-const-ref aggregate built at the call site:

```cpp
m_pTopLeftBone->SetLocalPos(Vector3(minX, 0.0f, maxZ));
m_pTopRightBone->SetLocalPos(Vector3(maxX, 0.0f, maxZ));
// ...
```

The frame comes out **too small** — `stwu r1, -0xb0` against the target's `-0xc0` — and
essentially every FPR and GPR downstream of the first call is permuted.

### Why it works

An unnamed temporary passed by const-ref dies at the end of its own full expression, so each
one dies at its own call and the next reuses the same slot: N temporaries, **one** slot.
Naming them extends each live range to the end of the enclosing block, so all N are in the
frame at once — and *then* MSVC's frame packer re-coalesces the pairs whose live ranges still
do not overlap. In DC3's `LabelShrinkWrapper::UpdateAndDrawWrapper`: four names, three slots,
exactly the target.

Getting the slot *count* right fixed everything downstream for free — FPR assignment and
instruction schedule both fell out with no further edits.

**Count the target's slots before editing anything.** Read every `stfs`/`stw` with an `r1`
base out of `full_listing=true` and group them by 16-byte slot.

### Do not half-fix it

Two intermediate shapes were measured and both are traps — they buy real points and then
stall, which reads exactly like a floor:

- Naming only the corners that need their own slot (two named + two temps): **90.6%**.
  Right slot *count*, wrong assignment.
- Three names plus a mid-function `Set()` to recycle one: **86.2%**. Correct slots *and*
  frame, but the `Set()` sits after the second call, so that value is materialised in the
  third basic block instead of the first.

**Fewer names is not closer.** Match the target's *number of live values* and let the packer
choose the sharing. Do not hand-recycle a slot.

### Match-hack smell

The 80.4% state this replaced used a single `auto _tmp0 = Vector3(...)` hoisted above the
other calls so one call could run *last*, against the target's call order. That is the shape
of a hack that found half of this lever by accident. A lone named temp whose only purpose is
to reorder calls usually means: name **all** of them and restore the natural order.

### Detection

- Frame-size delta on `stwu r1, -N` that is an exact multiple of the aggregate size, with the
  *base* frame smaller than the target's.
- `mode=stack-layout` reporting TGT_ONLY rows for a whole 16-byte group.
- Source-side tell: an aggregate constructed inside a call argument list.

---

## Negative results — do not re-run these

| Function | Lever tried | Variants | Result |
|---|---|---:|---|
| DC3 `ObjectDir::Iterate` | Declaration reorder / scope moves | 6 | **Byte-identical `.obj`** — not "no improvement", literally the same bytes |
| DC3 `ObjectDir::Iterate` | Declaration reorder, 2 further variants | 2 | Regressed 99.4% → ~95.8% |
| DC3 `ObjectDir::Iterate` | Beam-search permuter sweep | 65 candidates | **0 improvements** |
| DC3 `RndText::FitTextScroll` | Declaration reorder (before Lever 2 was found) | several | No movement |
| DC3 `LabelShrinkWrapper::UpdateAndDrawWrapper` | Commutative operand order on a 2-term `fadds` (source swap, `+=` split, hoisting either addend, flat-sum reorder, explicit grouping) | 6 | **Byte-identical** every time |
| DC3 `LabelShrinkWrapper::UpdateAndDrawWrapper` | Beam-search permuter sweep, chain-depth 4 | 56 | **0 improvements** over 99.86% |
| **our `ObjectDir::Iterate`** | **Lever 1 (live-range shortening)** | **1** | **Byte-identical `.obj`; funclet held 100.0%** |

The `Iterate` reorder result is the sharpest: the allocator's colouring is **deterministic
given the interference graph**, and none of those edits changed the interference graph, so
the output *could not* change. That is consistent with the c2.dll mechanism in
[unfixable-compiler.md: Register Allocation](unfixable-compiler.md#register-allocation) —
declaration order only permutes the colour→register mapping *when the constraints leave
slack*. With no slack it is provably inert, not a lottery ticket.

**Read a byte-identical result as a routing rule:** it is positive evidence that you are on
the wrong axis and must change the live set or the schedule.

---

## Triage Split: statement-level vs within-one-expression

*From a seven-lane, 31-function sweep of the AT_LIMIT + `REGISTER_SWAP` bucket in
dc3-decomp, 2026-08-04.* **This decides which functions to open.** Register swaps do not
classify a function; this does.

| Residual implicates… | Verdict | Tells |
|---|---|---|
| **A statement** — control flow, which field is read, which call is made, what stays live across a call, the shape of an *explicitly nested* expression | **Investigate.** Every win came from here. | insert/delete clusters, function-call diff rows, `addi`/`lwz` field-offset diffs, `__savegprlr_NN` deltas, branch polarity, signed-vs-unsigned compares, bool materialisation |
| **One arithmetic expression** — commutative operand order, flat-sum term order, which of two independent loads issues first | **Floor. Skip.** | a lone `fadds` / `fmuls` / `add` operand swap with no surrounding structural difference |

The exception is explicit parenthesization — a nested chain preserves its shape and its
term order is recoverable. See
[fixable-operators.md: a FLAT sum is canonicalised; an explicitly NESTED one is not](fixable-operators.md#sub-case-a-flat-sum-is-canonicalised-an-explicitly-nested-one-is-not).
**Confirm the nesting survived before spending builds on it** — one lane tested the
exception on a dot product and both groupings were byte-identical, because MSVC had
already flattened it.

### A third bucket: stack-slot allocation (looks statement-level, is not)

Insert/delete clusters that contain the **same instructions placed differently**, plus
`mode=stack-layout` showing many DIFFER / PERMUTED rows, is *slot allocation* — MSVC
reusing a slot across disjoint nested scopes where the target does not. Renaming the
locals apart does nothing; the packing is lifetime-based. **Drop these.**

### How well the rule performs

It is a **filter for what to open, not a predictor of what will close.**

- It never misfired in the costly direction — every expression-level residual dropped
  stayed a floor under test.
- Statement-level does **not** reliably convert. At ≥98% it is necessary but not
  sufficient; one lane went 2-for-5 with three *correct* diagnoses that measured worse.
- Budget an unfiltered sweep of the AT_LIMIT + `REGISTER_SWAP` bucket at roughly **1 win
  per 3 functions**. The post-filter rate was much better (~1 per 1.3) but that number is
  post-filter and must not be used to budget an unfiltered pass.

---

## Diagnostic order for a register-swap residual

### First: which register class swapped?

objdiff labels the class for you, and the class selects the lever. **This split is an ABI
consequence, not an n=3 generalisation** — a volatile register cannot hold a value across a
call, so a swap confined to volatiles is never itself a disagreement about what stays live
across a call.

| objdiff hint | Registers | Lever |
|---|---|---|
| `[callee-saved — check liveness across calls]` | r14-r31, f14-f31 | **Liveness** — Levers 1-2 |
| `[volatile — scheduling/operand order, not liveness]` | r0, r3-r12, f0-f13 | **Scheduling** — Lever 3 |
| `[mixed volatile+callee-saved — one liveness cause, start there]` | both | **One liveness cause**; the volatile half is its shadow |

The converse is not symmetric: a volatile swap can be *downstream* of a liveness problem
elsewhere even though it cannot *be* one. DC3's `FitTextScroll` showed callee-saved
`r27`↔`r28` / `r22`↔`r23` alongside volatile `f12`↔`f13` simultaneously, all from a single
member reload at a call site.

### Then

1. **Instruction counts and sizes.** Equal, with every mismatch a `diff_arg` on a register
   operand → the logic is right; you are purely in allocation territory.
2. **`__savegprlr_NN` delta.** One or two apart → the live set differs → Lever 2.
3. **Swap cycle length** (`mode=regswaps`). 2-cycle = colouring flip; 3+ rotation = the live
   set or live ranges differ → Lever 1.
4. **Is a producer at a different index than the target's?** → schedule problem → Lever 3.
   Fix the schedule before touching polarity.
5. **Only then** consider declaration order, and stop after the first byte-identical result.
6. **Offsets, not registers?** Different problem → Lever 4 /
   [Offset Swap](fixable-declarations.md#offset-swap).

---

## Floor evidence: the three-part standard

A residual is a floor only if **all** of:

- **(a) Hand variants return byte-identical output.** Not "no improvement" — *identical
  bytes*. A variant that changes the output without improving it means the axis is live and
  you have not found the right point on it.
- **(b) A permuter sweep returns zero improvements.** Record date, config, candidate count.
- **(c) Ghidra-decompile the *target* and show the construct is inexpressible.**

(a) and (b) only ever prove "I ran out of ideas". (c) converts that into "I proved this is
unreachable from C++". Run the `ghidra-decompile` skill against the **target** symbol and
read the residual instructions in the context of the target's own decompilation. You are
looking for one of:

- a spill/reload of a value with no source-level identity (allocator scratch),
- a slot reused for two unrelated values (live-range splitting),
- a store with no corresponding read on any path (dead conditional spill).

Any of those three names an allocator artifact. Anything else — a real computation, an extra
call, a different constant — means there **is** a missing source construct and the function
is not at a floor.

Worked example: DC3's `RndText::FitTextScroll` residual of 8/232 instructions decompiled (on
the *target*) as a **dead conditional spill** — a pointer written into a `float` local's
stack home on one path and never read. No C++ statement expresses that.

Do **not** claim a floor on (a) alone. See also
[at-limit-systemic.md](at-limit-systemic.md) for the systemic classes that genuinely are.

---

## See also

- [fixable-declarations.md: Variable Declaration Order](fixable-declarations.md#variable-declaration-order) — the corrected lever; still valid for stack/scope effects
- [fixable-declarations.md: Local Pointer Reload to Break Member-Address Reuse](fixable-declarations.md#local-pointer-reload-to-break-member-address-reuse) — sibling live-range-shortening pattern
- [fixable-declarations.md: Pre-Compute References Before Clobbering Calls](fixable-declarations.md#pre-compute-references-before-clobbering-calls) — the "create the local" half of Lever 2
- [fixable-declarations.md: Offset Swap](fixable-declarations.md#offset-swap) — stack-side residuals
- [fixable-operators.md: Comparison Operand Order](fixable-operators.md#comparison-operand-order) — the polarity half of Lever 3
- [unfixable-compiler.md: Register Allocation](unfixable-compiler.md#register-allocation) — c2.dll colouring mechanism; why byte-identical reorders are expected
- [../EH_FUNCLET_CASCADE.md](../EH_FUNCLET_CASCADE.md) — a liveness lever that does not move the frame leaves the parent's funclets at 100.0; one that does move it will wobble them
- [PERMUTER_ROI_ANALYSIS.md](PERMUTER_ROI_ANALYSIS.md) — `declaration_reorder` ROI, corrected for `REGISTER_SWAP`
