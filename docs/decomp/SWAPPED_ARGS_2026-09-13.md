# Two swapped-argument bugs — both CONFIRMED against retail bytes

**Lane W13-A · 2026-09-13 · branch `w13-swapped-args` off `main` `49b5a79f`**

Two suspected swapped-argument bugs were flagged by a dc3-decomp session.
**Both are correct as reported.** Our source had the arguments swapped in both
places; retail RB3 does the sensible thing in both.

Adjudicated **against RB3 retail bytes only** (`orig/45410914/band.exe`, read
through dtk's split asm under `build/45410914/asm/`), never against dc3's source
or dc3's target — dc3 is a different game and had already produced one false
lead by generalising from its own target to ours.

---

## Result summary

| # | site | verdict | retail says |
|---|---|---|---|
| 1 | `rndobj/AmbientOcclusion.cpp:1234` `std::sort(priEnd, priBegin)` | **CONFIRMED BUG** | `sort(begin, end)` — r3=begin, r4=end |
| 2 | `math/Key.cpp:141-142` `NormalizeTo` output overwritten | **CONFIRMED BUG** | `NormalizeTo(prevQuat, X)` — r3=prevQuat in all three calls |
| 3 | *(suspected in passing)* `FacePriority` member order | **REFUTED** | our layout already matches retail |
| 4 | *(found while verifying #1)* `&priorities[0] + size()` spelling | **CONFIRMED divergence, fixed** | retail loads `_M_finish` directly ⇒ `.end()`/`.begin()` |

**Whole-binary A/B: Δ0 on every measure, both waves, 0 units fell off 100%.**
That was **pre-registered** and is the expected result — see "Why the metric
cannot see any of this" below. Landed on merit.

---

## Bug 1 — `RndAmbientOcclusion::Tessellate`, reversed `std::sort`

`std::sort(first, last)` with `first > last` is undefined behaviour, and the
very next line used the same two pointers the right way round.

Retail `Tessellate` is **`fn_82491918`** (4,796 B) in `default/AmbientOcclusion`.
It contains exactly **one** sort call:

```
lwz  r30, 0xb4(r31)   ; priorities._M_finish  (END)
lwz  r29, 0xb0(r31)   ; priorities._M_start   (BEGIN)
mr   r4, r30          ; r4 = END
mr   r3, r29          ; r3 = BEGIN
bl   fn_824913F8      ; sort(BEGIN, END)
subf r11, r29, r30    ; end - begin
srawi. r24, r11, 3    ; /8 = count   (sizeof(FacePriority) == 8)
```

⇒ retail is `sort(begin, end)`; our call was reversed. **Fixed.**

**The declaration order was already right and is deliberately left alone.**
Retail loads `0xb4` (end) *before* `0xb0` (begin), which is exactly our
`priEnd`-then-`priBegin` declaration order. So the decomp author read the
declaration order correctly off the asm and then wrote the `sort` call in
*declaration* order rather than *semantic* order. Only the call is swapped.

### The callee is an ICF fold-alias, not a wrong callee

`fn_824913F8` resolves through `target_symbol_map.json` to
`??$sort@PAV?$Key@M@@@stlpmtx_std@@YAXPAV?$Key@M@@0@Z` — i.e.
`sort<Key<float>*>`, **not** `sort<FacePriority*>`. That is a genuine ICF fold,
fully explained:

- `FacePriority` is `{unsigned int faceIndex; float priority;}` — 8 B,
  `operator<` compares the float at **+4**
- `Key<float>` is `{float value; float frame;}` — 8 B, `operator<` compares
  `frame`, also at **+4**

Identical machine code including relocations ⇒ MSVC folds them and the map keeps
one arbitrary survivor spelling. This is the shape CLAUDE.md warns about
(`TEMPLATE_ARGS_DIFFER` / `LINKER_MERGED` *is* what a fold looks like) — do not
read it as a wrong callee.

## Bug 3 (refuted) — `FacePriority` member order

While reading the above I noticed retail storing the **int at +0 and the float
at +4** of the `FacePriority` temp (`stw r20, 0x1d8(r31)` / `stfs f0,
0x1dc(r31)`), which looked like our member order might be inverted. It is not:
our struct is already `{unsigned int faceIndex; float priority;}`, and retail's
comparator confirms the layout independently —
`??$__unguarded_partition@PAUFacePriority@@…` (`0x8248e060`) does
`lfs f13, 0x4(r3)` with stride 8 (`addi r3,r3,0x8` / `subi r4,r4,0x8`).

**No change made.** Recorded so the next lane does not re-hunt it.

## Bug 4 — `&priorities[0] + priorities.size()` is the wrong spelling

Found while *verifying* the Bug 1 fix by disassembling our own object. Our
expression emitted a redundant round-trip that retail does not have:

```
ours (before):  lwz r21,0xe8(r31) / lwz r11,0xec(r31) / mr r3,r21
                subf r11,r21,r11 / srawi r11,r11,3 / slwi r11,r11,3
                add r30,r11,r21  / mr r4,r30 / bl sort
```

`(finish-start)/8*8 + start` is just `finish`, and retail simply loads
`_M_finish`. Changing the two declarations to `priorities.end()` /
`priorities.begin()` (keeping the end-first order) makes our emitted code
**instruction-for-instruction identical to retail**:

```
ours (after):  lwz r30,0xec(r31) / lwz r21,0xe8(r31) / mr r4,r30 / mr r3,r21
               bl sort / subf r11,r21,r30 / mr r26,r20 / srawi. r23,r11,3
retail:        lwz r30,0xb4(r31) / lwz r29,0xb0(r31) / mr r4,r30 / mr r3,r29
               bl sort / subf r11,r29,r30 / mr r25,r15 / srawi. r24,r11,3
```

Same opcodes in the same order, including the interleaved `mr`; only register
allocation and the frame offsets differ. Our `Tessellate` shrank 4,260 → 4,244 B.

⚠ Our `Tessellate` is still **4,244 B vs retail's 4,796 B**, so substantial
divergence remains in that function. This lane fixed the sort site only.

---

## Bug 2 — `QuatSpline`, `NormalizeTo` output overwritten

`Mtx.h:569` is `inline void NormalizeTo(const Hmx::Quat &qin, Hmx::Quat &qout)`
— **the second parameter is the output** (it negates `qout` when
`qin·qout < 0`). Our code:

```cpp
NormalizeTo(q88, prevQuat);      // rewrites prevQuat
NormalizeTo(nextQuat, prevQuat); // rewrites prevQuat AGAIN — q88's alignment discarded
NormalizeTo(prevQuat, q58);      // opposite convention: prevQuat is the INPUT here
```

Three calls, and the third disagreed with the first two.

Retail `QuatSpline` is **`fn_824F5B68`** (524 B) in `default/Key`, and the
adjudication is unusually clean because **`NormalizeTo` is NOT inlined there** —
it is three `bl fn_823DE420`, which the map names
`?NormalizeTo@@YAXABVQuat@Hmx@@AAV12@@Z` (the exact two-arg form). So r3 = `qin`
and r4 = `qout` are directly readable.

Four quats live in the frame. Each was identified from the copy that fills it,
then **re-confirmed independently** by the interpolation loop, which walks
`r11 = r1+0x70` reading `-0x20 / -0x10 / 0x00 / +0x10` = q88 / prevQuat /
nextQuat / q58 — matching our source's read order:

| slot | contents | how identified |
|---|---|---|
| `r1+0x50` | `q88` | filled from the `idx==0 ? prevQuat : keys[idx-1]` select |
| `r1+0x60` | `prevQuat` | 16 B copied from `0x0(r4)` = `prev->value` |
| `r1+0x70` | `nextQuat` | 16 B copied from `0x0(r5)` = `next->value` |
| `r1+0x80` | `q58` | filled from the `idx+1==size-1 ? nextQuat : keys[idx+2]` select |

The three calls:

```
addi r4,r1,0x50 ; addi r3,r1,0x60 ; bl  ->  NormalizeTo(prevQuat, q88)
addi r4,r1,0x70 ; addi r3,r1,0x60 ; bl  ->  NormalizeTo(prevQuat, nextQuat)
addi r4,r1,0x80 ; addi r3,r1,0x60 ; bl  ->  NormalizeTo(prevQuat, q58)
```

**r3 is `prevQuat` in all three.** Retail aligns both neighbours *to* `prevQuat`
— exactly the suspected intent. The first two calls are **fixed**; the third was
already right.

(Corroboration that the whole function is QuatSpline: `Key<Quat>` stride is
`0x14`, the `divw`/`mulli 0x14` index arithmetic matches
`idx = prev - &keys.front()`, and the tail is
`bl fn_824EE3F8` = `?Normalize@@YAXABVQuat@Hmx@@AAV12@@Z` with r3=r4=qout, i.e.
`Normalize(qout, qout)`.)

### Verified on our own emitted code

After the fix our `QuatSpline` emits `r3 = r1+0x50` (prevQuat) **constant across
all three calls**, with r4 walking q88 → nextQuat → q58 — structurally identical
to retail's constant `r3 = r1+0x60` walking `0x50 → 0x70 → 0x80`. Before the
fix the pattern was the mirror image (r3 varying, r4 constant for the first two),
which retail's bytes rule out.

Stack-slot *assignment* still differs (our frame is `0xd0`, retail's `0xf0`) —
a separate, unaddressed matching concern.

---

## Why the metric cannot see any of this

Both fixes measured **Δ0**, pre-registered, and that is a **pass**:

1. **Both enclosing rows are UNPAIRED.** `fn_82491918` and `fn_824F5B68` both
   read `fuzzy 0 / mpn 0` because neither address is named in
   `target_symbol_map.json`. objdiff pairs by name, so **no source edit inside
   either function can score at all**, however correct it is.
2. **Even if they paired, a swapped argument pair is a *register* arg diff**,
   which `mpn` excludes by construction — both rows could read 100 before and
   after.

Doubly invisible. Per the standing rule (*a metric that hides real bugs is worse
than a lower metric*), these land on merit, and the verification that actually
carries weight is the **non-metric** one: compile our object, disassemble the
call site, compare register-for-register against retail. That instrument settled
all four questions above; the metric settled none of them.

### Measured

Two `tools/ab_measure.py --from-dirty` runs (wave 1 = bugs 1+2, wave 2 = bug 4),
each with real leg-B recompiles so neither is absent-vs-absent:

```
wave 1 (9 leg-B recompiles)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000000pp   (49.153210 -> 49.153210)
  units at 100% [mpn]:              164 -> 164   (0 reached, 0 fell off)
  units at 100% [all-rows-fuzzy]:   136 -> 136   (0 reached, 0 fell off)
```

```
wave 2 (3 leg-B recompiles)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000000pp   (49.153210 -> 49.153210)
  units at 100% [mpn]:              164 -> 164   (0 reached, 0 fell off)
  units at 100% [all-rows-fuzzy]:   136 -> 136   (0 reached, 0 fell off)
```

(Wave 2's leg A is the committed wave-1 state, so the two runs share a leg-A
baseline of `matched=42839 / code%=37.982048` — consistent across both.)

---

## An instrument failure worth recording

My first pass at reading our own `Tessellate` reported **"0 bl callees"** in a
4,260-byte function — a decisive-looking negative that would have killed the
Bug 4 finding outright. It was **vacuous**: capstone's PPC decoder stops at the
first word it cannot decode, so the disassembly never reached the `sort` call at
section offset `0x52c`. The section's relocation table said `nrel = 127`, not 0.

⇒ **Drive an object-file scan off the relocation table, not off a linear
disassembly**, and sanity-check any "found nothing" against a count the parse
did not produce. Same family as the `grep`-binary and `all([])` traps in
CLAUDE.md: *a vacuity that agrees with your prior is the hardest kind to catch.*

---

## Deliberately NOT done

- **Did not name `0x82491918` / `0x824f5b68` in `target_symbol_map.json`.**
  Naming them would make both rows pairable and these fixes finally
  *measurable* (our mangled names are
  `?Tessellate@RndAmbientOcclusion@@QAAXPAM0@Z` and
  `?QuatSpline@@YAXABV?$Keys@VQuat@Hmx@@V12@@@PBV?$Key@VQuat@Hmx@@@@1MAAVQuat@Hmx@@@Z`).
  Skipped because a map edit is a separate risk class — CLAUDE.md measures
  un-pairing as 80.5% of a map edit's delta, and it forces a full re-split on
  both A/B legs. **Recommended as a follow-up lane**, since the payoff of naming
  an anonymous address is documented as *bug exposure*, which is precisely what
  this lane is short of.
- **Did not pursue the remaining 552 B of divergence in `Tessellate`**, nor the
  stack-slot assignment difference in `QuatSpline`.
- **Did not touch the third `NormalizeTo` call** — retail confirms it was
  already correct.
- **Did not audit other `NormalizeTo` / `std::sort` call sites** tree-wide for
  the same swap. That is a cheap and plausibly productive sweep and is the other
  obvious follow-up.
