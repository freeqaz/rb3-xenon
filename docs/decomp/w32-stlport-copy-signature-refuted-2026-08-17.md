# W32-STLPORT — the "STLport `__copy` signature difference" is REFUTED as to CAUSE

**Date:** 2026-08-17 · **Lane:** W32-STLPORT · **Base:** `4f5b0cac` (`grounded2-restoration`)
**Verdict: the observation REPRODUCES EXACTLY; its stated mechanism is WRONG; the
family lever is DEAD, measured at −411 matched functions / −57,396 B.**

## The claim under test

Lane W28-UISRC, adjudicating `?_M_erase@?$vector@ULabelStyle@UILabel@@...` (112 B,
fuzzy 88.57), reported:

> retail zero-initializes a `random_access_iterator_tag` stack temp and passes
> **one fewer argument**; we pass an extra `Distance*` (`PAH`). That is an
> **STLport `__copy` signature difference in shared headers** used by every
> `vector` in the binary — a force-multiplier or a disaster.

It flagged and deliberately did not touch it. That was the right call on blast
radius, and the wrong diagnosis of cause.

## The observation is REAL — same measurement, both sides

Contrary to the STLPORT-1 precedent (a one-sided COMDAT-span reader artifact),
this one is *not* an instrument error. Both sides come from a single objdiff
pairing of one symbol, complete listing, 112 B = 28 instructions on the target
with no truncation:

| idx | target (retail) | base (ours) |
|---:|---|---|
| 11 | `addi r11, r1, 0x50` | `li r7, 0x0` |
| 12 | `li r10, 0x0` | `addi r6, r1, 0x50` |
| 13 | `addi r6, r1, 0x50` | *(delete)* |
| 14 | `mr r5, r31` | `mr r5, r31` |
| 15 | `stb r10, 0x0(r11)` | *(delete)* |
| 16 | `bl fn_8234C2D8` | `bl ??$__copy@...ABUrandom_access_iterator_tag@0@PAH@Z` |

Retail sets **r3,r4,r5,r6 only** — no `r7` anywhere in the function — and
**zero-initializes** the 1-byte temp it passes in r6. We set `r7 = 0` (the
`(ptrdiff_t*)0`) and leave the temp uninitialized. So "one fewer argument" and
"zero-inits the tag temp" are both accurate.

## …and the CAUSE is not a signature difference. Three independent refutations.

**1. The callee is structurally incapable of discriminating 4-arg from 5-arg.**
`fn_8234C2D8` reads **only r3, r4, r5**:

```
subf r11, r3, r4 ; li r10, 0x1c ; divw. r30, r11, r10 ; mr r29, r5 ; <loop>
```

r6 and r7 are never read. Both the tag and the `_Distance*` are dead
tag-dispatch parameters in *either* signature — so "does the callee use r7" is a
**vacuous** test, and any argument resting on the callee body is worthless.

**2. DC3's leaked `ham_xbox_r.map` — a real shipped Harmonix Milo build, same
compiler, same STLport — says our signature is CORRECT.** All **12/12** `__copy`
symbols in that map carry the 5-arg form `...ABUrandom_access_iterator_tag@0@PAH@Z`;
**zero** 4-arg forms exist. And the map contains *the identical instantiation*:

```
??$__copy@PAULabelStyle@UILabel@@PAU12@H@stlpmtx_std@@YAPAULabelStyle@UILabel@@PAU12@00ABUrandom_access_iterator_tag@0@PAH@Z
```

byte-identical to the symbol our build emits. The STLport version argument also
runs **backwards**: 4.x (5-arg, `_Distance*`) → 5.x (4-arg) is the progression,
and DC3 is *newer* than RB3 — so RB3 cannot plausibly hold the 4-arg form.

**3. The real mechanism: INLINE POLICY, proven mechanically.** `_M_erase` calls
`__copy_ptrs(__pos + 1, this->_M_finish, __pos, _TrivialAss())` (`_vector.h:541`),
and `__copy_ptrs(first, last, result, const __false_type&)` **has exactly 4
parameters**, its 4th being `_TrivialAss()` — an empty tag *value-initialized as a
temp* and bound to `const&`. That accounts for the missing r7 **and** the `stb`
zero-init with one mechanism: **retail out-lines `__copy_ptrs`; we inline it and
out-line `__copy` instead.**

Tested, not argued — `__declspec(noinline)` on the `__false_type` overload of
`__copy_ptrs`:

**88.57% → 100.0%, 28/28 equal, 0 mismatches**, with `addi r11,r1,0x50` and
`stb r10,0x0(r11)` both reappearing and our callee becoming
`??$__copy_ptrs@PAULabelStyle@UILabel@@PAU12@@stlpmtx_std@@...ABU__false_type@0@@Z`
— 4 params, no `_Distance*`, exactly retail's shape.

⇒ **There is no signature defect to fix. Dropping `_Distance*` from `__copy`
would have moved our source AWAY from shipped-Milo ground truth** while plausibly
moving the metric — textbook metric-fitting.

## The family lever is DEAD — measured

Population sized first: **330 distinct 5-arg `__copy` instantiations, 1,515
references across 303 objs.** W28 was right that the blast radius is binary-wide.

Pre-registered prediction: net positive, +10..+80 matched fns / +1,000..+12,000 B.
**Measured (whole-binary A/B, `name_check`, both legs settled):**

```
Δmatched = -411   Δhonest = -385   Δcode% = -0.556126pp   Δcode_bytes = -57396
unit improvements: 1 (default/UILabel +2)
unit REGRESSIONS: 134 units, sum -413
units at 100% [mpn]: 255 -> 245  (10 fell off, 0 reached)
```

**Wrong in sign, and the regression is itself the decisive measurement:** those
411 functions matched *because* retail inlines `__copy_ptrs`. So retail inlines it
at **≥411 sites** and out-lines it at **one** — `_M_erase@vector<LabelStyle>`.
Our default is right; that row is a per-site inline-heuristic divergence.

**REVERTED.** Do not re-run this as a family lever, in either direction.

## What this lane did NOT do

- Did not attempt a per-call-site hack (a synthetic noinline helper reached only
  from `_M_erase`). objdiff *would* forgive the callee name — retail's is the
  placeholder `fn_8234C2D8` — so it would likely "work", which is exactly why it
  is metric-fitting a 112 B row against a −57 kB family truth. Standing directive
  is accuracy over headline.
- Did not touch `__copy_backward_ptrs`, the symmetric pattern. Same reasoning
  applies by construction; a lane wanting it must A/B it independently.

## Durable lessons

- **"Count right, cause wrong" landed exactly as the standing hazard predicts.**
  W28's count reproduced perfectly and its mechanism was still wrong. A
  reproducing count is not evidence for its mechanism.
- **Ask whether the discriminator CAN discriminate.** The obvious test — "does
  the callee read r7?" — is vacuous here, because a dead tag-dispatch parameter is
  unread under *both* hypotheses. It would have "confirmed" whichever prior was
  brought to it.
- **A leaked map of a shipped sibling build settles a signature question that
  asm cannot.** Mangled names spell the parameter list; retail's stripped
  `fn_<addr>` never can.
