# W16-Q — base-chaining adjudication, `RefIs`, the CamShot `Target`, and a self-test that can fail

Lane W16-Q, branch `w16-q` off `c0bd1581`, worktree `~/tmp/wt-w16-q`.
Follows `W16O_ALIAS_REVIEWDISPLAY_REPLACE_2026-09-14.md`.

Whole-binary, graded ruler (`functionRelocDiffs=name_check`, read from
`report.json` `provenance.diff_config` — not assumed):

| | matched_functions | matched_code | code% |
|---|---:|---:|---:|
| baseline `c0bd1581` | 43,216 | 3,938,664 | 38.441160 |
| final | **43,221** | **3,939,312** | **38.447483** |
| net | **+5** | **+648 B** | +0.006323 |

---

## Verdict table

| # | Claim under test | Instrument | Verdict | Bytes |
|---|---|---|---|---:|
| 1 | ~22 un-adjudicated `Hmx::Object::Replace` fallback sites chain to a base | retail disassembly of each recovered body | **8 adjudicated, all (a) no fallback arm**; 15 unpairable, untouched | +540 |
| 2 | `RefIs` must compare the `Hmx::Object` subobject, not the raw `Ptr()` | 7 proven retail addresses | **CONFIRMED**, fixed | +0 |
| 3 | `0x824c93c0` is `list<HamCamShot::Target>`; RB3's `Target` is a smaller struct | retail bytes + `.text`/`.pdata` geometry + COFF | **REFUTED both halves** — it is EventAnim's `list<EventCall>`, mis-pinned | +108 |
| 4 | `--self-test` is a no-op | executed it | **CONFIRMED**, wired to a control proven able to fail | n/a |
| 5 | Our 316 B `HamCamShot::Target` serializer should disappear | COFF after build | **Still emitted, now correctly unpaired** | 0 |
| — | W16-O's fold at `0x824c93c0` | `/OPT:ICF` reloc-identity requirement | **Unsound as a fold** — withdrawn | 0 |

---

## Item 1 — the `Hmx::Object::Replace` fallback vein

### The pairing problem, and how it was solved

The brief listed ~22 sites and said to adjudicate each on retail bytes. The
obstacle was not disassembly, it was **identification**: of the 23 sites in the
vein, exactly **one** (`EventTrigger`) had a map-named body. The rest had only
named MSVC **vtordisp adjustor thunks**
(`?Replace@X@@$4PPPPPPPM@A@AA...`).

That turned out to be the lever rather than the blocker. An adjustor thunk is
`lwz r11,-4(r3); subf r3,r11,r3; b <body>` — it branches to the real body **by
construction**, so the thunk's `b` target *is* the body address, for free and
unambiguously. Recovering 7 body addresses that way made the sites adjudicable.

**Control:** `EventTrigger`'s thunk `b` target is `0x824a0870`, which equals its
independently map-named body address. The recovery method reproduces a known
answer.

**Instrument validation:** a scratch big-endian PPC disassembler was written for
this. Before trusting it, it was made to reproduce W16-O's *independently
published* `RndTransformable::Replace` byte sequence — it matched exactly — and
it agreed operand-for-operand with the repo's own `tools/retail_body.py`.

### Adjudication

All 7 recovered bodies plus `EventTrigger` came out **verdict (a): no fallback
arm at all** — no base-class call of any kind. The only calls present are
`__RTDynamicCast` and the ICF-folded `SetOwnerObj`. Since `Hmx::Object::Replace`
is empty in the match build but still costs a real `bl` with no LTCG, (a) and
(c) are distinguishable exactly by whether retail emits the `bl`. It does not.

Cross-checks against `/d1reportSingleClassLayout` (authoritative; the `// 0xHEX`
header comments are measurably wrong in places), all four agreeing exactly:

| site | `addi r11,r3,N` ⇒ member | `subi r4,r31,N` ⇒ `Hmx::Object` vbase |
|---|---|---|
| `RndMatAnim` | 40 = `mKeysOwner` @ 0x28 | 56 |
| `RndTransAnim` | 28 = `mKeysOwner` @ 0x1c | 120 |
| `RndEnvAnim` | 64 = `mKeysOwner` @ 0x40 | 80 |
| `RndMovie` | 68 = `mKeysOwner` @ 0x44 | 88 |

### Sites touched (8) — commit `06a3bc6b`

`rndobj/MatAnim.cpp`, `rndobj/TransAnim.cpp`, `rndobj/EnvAnim.cpp`,
`rndobj/LitAnim.cpp`, `rndobj/Movie.cpp`, `char/CharWeightable.cpp`,
`char/CharBonesMeshes.cpp`, `rndobj/EventTrigger.cpp`.

Each fallback arm guarded `#ifdef HX_NATIVE` (keeps native behaviour; the match
build never defines it). Three additional shape fixes. Canonical retail anim
shape, now in all four anim classes:

```cpp
void RndXxxAnim::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mKeysOwner)) {
        if (!obj) mKeysOwner.SetOwnerObj(this);
        else mKeysOwner.SetOwnerObj(dynamic_cast<RndXxxAnim *>(obj)->mKeysOwner.Ptr());
        return;
    }
#ifdef HX_NATIVE
    Hmx::Object::Replace(ref, obj);
#endif
}
```

`CharWeightable`'s proven shape differs — the restore arm is an **unconditional
second statement**, not an `else`:

```cpp
void CharWeightable::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mWeightOwner))
        mWeightOwner.SetOwnerObj(dynamic_cast<CharWeightable *>(obj));
    if (!mWeightOwner.Ptr())
        mWeightOwner.SetOwnerObj(this);
#ifdef HX_NATIVE
    Hmx::Object::Replace(ref, obj);
#endif
}
```

Δ0/Δ0 on the metric, one row moved (`EventTrigger` 33.642 → 37.748). The bodies
were already byte-correct; what was missing was **pairability**.

### Sites NOT touched (15), and why

`gesture/SkeletonUpdate.cpp:169`, `flow/FlowSetProperty.cpp:142,454`,
`rndobj/Font.cpp:181`, `rndobj/Anim.cpp:444,449`, `obj/Task.cpp:60,115,195`,
`rndobj/Wind.cpp:75`, `synth/FxSend.cpp:22`, `flow/FlowAnimate.cpp:354`,
`bandobj/BandDirector.cpp:403`, `world/DefaultPhysicsManager.cpp:80`,
`rndobj/Env.cpp:123`.

**Reason, uniform: no retail row could be reached.** Neither a map-named body nor
a named vtordisp thunk whose `b` target would yield one. The brief's own rule is
*"do not touch a site you cannot pair to a retail row"*, and an unpaired row is
invisible to callee adjudication — it looks identical to a row with nothing
wrong. Editing these would have been a blind sweep, which the brief forbids.

### The naming lever — commit `47171493`

Four in-unit map names added for bodies recovered from thunks:

```
0x824619d8  ?Replace@RndMatAnim@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z
0x82486ad0  ?Replace@RndEnvAnim@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z
0x8245e440  ?Replace@RndTransAnim@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z
0x82478128  ?Replace@RndMovie@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z
```

**Predicted +4 functions / +540 B; measured +4 / +540 B, 0 rows fell out.**

That exactness is the point: naming cannot manufacture bytes. Paying out in full
on the first measurement **retroactively proves the retail-byte adjudication was
correct** — the bodies were already byte-exact and only pairability was absent.

---

## Item 2 — `RefIs` compares the wrong pointer

`src/system/obj/Object.h:920`. The X360 branch compared the raw `Ptr()`; retail
compares the `Hmx::Object` **subobject**. Fixed (commit `b520ae01`):

```cpp
// was
return reinterpret_cast<void *>(from)
    == reinterpret_cast<void *>(const_cast<P &>(member).Ptr());
// now
return static_cast<Hmx::Object *>(const_cast<P &>(member).Ptr())
    == reinterpret_cast<Hmx::Object *>(from);
```

This is a PCH input and cascades to ~281 TUs, which is why it was done in a
worktree. Measured Δ0 functions / Δ0 bytes, fuzzy +0.000106, **0 rows fell out**
across every caller.

A Δ0 here is the *expected* result, not a disappointment: `mpn` is arg-blind and
cannot register this class of fix, and the upcast is a no-op for
single-inheritance pointees with `Object` at +0 — which is also why
`TexMovie.cpp:44`'s "compared directly with no vbtable adjust" note is
consistent with the fix rather than a counterexample. It is a correctness fix
that removes a real MI false-negative, landed on merit.

---

## Item 3 — the CamShot `Target`: **both halves of the premise are refuted**

The brief asked to prove RB3's `Target` field list from the bytes, on the theory
that retail's 72 B element serializer meant RB3's `Target` was a much smaller
struct than our 316 B `HamCamShot::Target` one.

### What the bytes actually say

Retail `0x824c93c0` (108 B) has **exactly one charged site** and it is the
element `bl`. The full listing:

```
 7  b <loop test>
17  lwz r30, 0x0(r31)
19  addi r4, r30, 0x8          <- STLport node: data at +8
20  bl ??6@...ABVEventCall@EventAnim@@@Z   <- THE ONLY CHARGE
21  lwz r30, 0x0(r30)          <- follow _M_next
22  cmplw cr6, r30, r31
```

The callee `0x824c8f68` was **already map-named**
`??6@YAAAVBinStream@@AAV0@ABVEventCall@EventAnim@@@Z`. A `list<T>` serializer
whose only callee is `EventCall`'s element serializer **is `list<EventCall>`**.
The map was inconsistent with itself.

**Two premises die here:**

1. **`sizeof(Target)` cannot be derived from this row, in principle.** The brief
   said to read "the list node stride in the 108 B row's loop". The loop is
   `lwz r30, 0x0(r30)` — a linked-list next-pointer walk. **A list has no
   stride.** What the bytes *do* disclose is the element offset: `+8`.
2. **RB3's `Target` is not a smaller struct.** The 72 B row is `EventCall`'s, not
   `Target`'s. RB3's real `BandCamShot::Target` element serializer is separately
   map-named at `0x822b1b30`, is **344 B**, and is **already at fuzzy 100.0**.
   The brief compared two different structs' serializers.

`BandCamShot::Target` also already exists and is already map-named in RB3 — live
paired rows `?resize@?$ObjList@UTarget@BandCamShot@@@@QAAXI@Z` (68 B, 99.706)
and `??4Target@BandCamShot@@QAAAAU01@ABU01@@Z` (248 B, 99.597).

### The actual defect: a one-function mis-pin

Two independent geometries agree the body belongs to `EventAnim.cpp`:

| instrument | evidence |
|---|---|
| `.text` | EventAnim spans `…0x824C9208..0x824C93C0` and `0x824C9430..0x824C95F0`; the 108 B island `0x824C93C0..0x824C942C` pinned to **BandCamShot** sat exactly in the gap, bracketed by EventAnim on both sides |
| `.pdata` | dtk's re-derivation moved unwind record `0x82213DD0..0x82213DD8` out of BandCamShot and **merged it contiguously** into EventAnim's adjacent record `0x82213DB0..0x82213DD0` |

The `.pdata` agreement was not authored by me — dtk rewrote `splits.txt` on its
own and the build instructed me to commit what it wrote. Retail's own unwind
table places this function in EventAnim's run.

**Why the wrong pin existed:** the DC3-derived name `list<HamCamShot::Target>` is
only definable by `BandCamShot.obj`, because `BandCamShot.cpp` scatter-includes
`HamCamShot.cpp` (467 HamCamShot symbols in that obj). The address was pinned to
the unit that could define the name. **The name drove the pin, not the bytes.**

### The fix, and why it was legal

The brief's safety test — *"correct the map spelling IF our compiled obj can
define it; check the COFF symbol table AFTER a build"* — was applied literally:

| obj | defines `??$?6VEventCall@EventAnim@@…` |
|---|---|
| `BandCamShot.obj` | **no** |
| `EventAnim.obj` | **yes** |

So renaming in place would have produced a permanent 0%. Re-homing the pin to
EventAnim *and* renaming is what makes the name definable.

**Predicted +108 B / +0 functions. Measured +108 B / +1 function.** Rowset
set-diff: 1 row crossed in, 0 fell out, nothing else moved; the row goes
99.815 → 100.0.

⚠ **The +1 function is one more than predicted and I could not confirm why.** My
model said +0 because the old row's `mpn` was already 100 and would simply move
units. The most consistent explanation is that the old BandCamShot row scored
`mpn < 100` and was therefore never counted — but the old row's `mpn` was not
captured before the rebuild, and `rowset_snapshot` stores only the `fuzzy == 100`
set, so it is unrecoverable. **Stated as unconfirmed, not as a finding.**

---

## The alias correction — identification is not folding

W16-O installed `list<EventCall>` as a **folded spelling** under survivor
`list<HamCamShot::Target>` at `0x824c93c0`, on a fresh T1 proof that retail's
bytes there are byte-identical to our compiled COMDAT for the EventCall
spelling. The byte evidence is sound; the inference is not:

> Byte-identity between **retail-at-address-X** and **our-COMDAT-for-name-N**
> proves that **X *is* N**. It does **not** prove that N *folded* with whatever
> name the map happens to carry for X.

These are different claims with different evidence requirements. A fold under
`/OPT:ICF` additionally requires **our two COMDATs to be identical to each other
including relocations**. Here they differ in exactly one relocation — the
element `bl` — because retail's element callee is EventCall's 72 B serializer
while our `HamCamShot::Target` element serializer is 316 B. Different size,
different target: **unfoldable by construction**, so the fold could never have
been real.

The T1 instrument is structurally incapable of separating the two claims,
because it only ever compares **one** side against retail. This is the same
family as the one-sided-instrument error recorded on 2026-08-16.

The correct action on an identification is to fix the name — which is what the
re-home did. Group kept, `folded` emptied, spelling recorded under `withdrawn`
as `IDENTIFICATION_NOT_A_FOLD`, survivor corrected. **Nothing pruned**, per house
rule. Predicted Δ0; **measured Δ0 exactly**.

**Verified non-vacuous** (a Δ0 is exactly the shape an unapplied change makes):
the regenerated `build/45410914/icf_aliases.map` carries the EventCall spelling
and **zero** occurrences of the `HamCamShot::Target` list spelling.

---

## Item 4 — `--self-test` wired to a control that can fail

`tools/w33_fold_adjudicate.py --self-test` was declared at line 317 and never
referenced. A self-test that cannot fail is not a control.

Verdict logic was split out of `adjudicate()` into a pure
`decide(r, tb, tr, ob, orl, min_bytes=16)`, then `SELF_TEST_CASES` + `self_test()`
were added and `--pairs` made optional. Cases:

| case | must return |
|---|---|
| `IDENTICAL_FOLD` | IDENTICAL |
| `DIFF_WORDS` | DIFFERENT |
| `SHAPE_ONLY_WRONG_CALLEE` | SHAPE_ONLY |
| `UNDECIDED_PLACEHOLDER` | UNDECIDED |
| `SIZE_DIFFERS` | DIFFERENT |

**Proven able to fail:** the comparator was sabotaged once → `rc=1`; restored →
`rc=0`.

The tool then earned its keep immediately — it is what produced the decisive
item 3 reading:

```
DIFFERENT  retail 72 B EventCall elem  vs  our Target@BandCamShot 344 B  (size differs)
DIFFERENT  retail 72 B EventCall elem  vs  our Target@HamCamShot  316 B  (size differs)
UNDECIDED  retail 72 B EventCall elem  vs  our EventCall elem  (bodies equal, 2 placeholder relocs)
```

---

## Item 5 — fate of the 316 B COMDAT

Still emitted into `BandCamShot.obj`, because `BandCamShot.cpp` scatter-includes
`HamCamShot.cpp`; that include chain is unchanged by this lane. It is now
**correctly unpaired** — no retail address claims it, because retail RB3 has no
`HamCamShot::Target` serializer at all. It costs nothing (a base-only symbol is
not in the target denominator). Final state of the three element serializers:

| symbol | unit | size | fuzzy |
|---|---|---:|---:|
| `??6@…ABUTarget@BandCamShot@@@Z` | `default/BandCamShot` | 344 | **100.0** |
| `??6@…ABVEventCall@EventAnim@@@Z` | `default/EventAnim` | 72 | **100.0** |
| `??6@…ABUTarget@HamCamShot@@@Z` | — | 316 | unpaired (correct) |

---

## Predicted vs measured

| step | predicted | measured | |
|---|---|---|---|
| `RefIs` upcast | Δ0 (arg-blind ruler) | Δ0/Δ0, fuzzy +0.000106, 0 fell out | ✅ |
| 8 Replace sites | Δ0 (pairability, not bytes) | Δ0/Δ0, EventTrigger 33.642→37.748 | ✅ |
| 4 map names | **+4 fns / +540 B** | **+4 / +540** | ✅ exact |
| re-home `0x824c93c0` | **+108 B / +0 fns** | **+108 B / +1 fn** | ✅ bytes exact; fn +1 unexplained |
| alias withdrawal | Δ0 | Δ0 exactly | ✅ |

---

## Commits

| commit | item |
|---|---|
| `b520ae01` | 2 — `RefIs` upcasts to the `Hmx::Object` subobject |
| `06a3bc6b` | 1 — eight Replace sites adjudicated on retail bytes, all verdict (a) |
| `47171493` | 1 — four in-unit map names; predicted +4/+540, measured +4/+540 |
| `61b9a1a3` | 4 — `--self-test` wired to a control proven able to fail |
| `d74fdcea` | 3 — `0x824c93c0` re-homed to EventAnim + renamed to `list<EventCall>`; +108 B |
| `a5426395` | alias — withdraw the fold as `IDENTIFICATION_NOT_A_FOLD`; Δ0 |

---

## NOT done

- **15 of 23 item-1 sites left untouched** — no reachable retail row (no map-named
  body, no vtordisp thunk to recover one from). Listed individually above.
- **`EventTrigger::Replace` not rewritten.** 636 B, now fuzzy 37.748. Retail uses a
  continue-scanning / erase-in-place shape; matching it is a full rewrite, not a
  fallback-arm adjudication, and is out of this lane's scope.
- **Three scattered bodies left unpairable** — `CharWeightable` (144 B),
  `CharBonesMeshes` (216 B), `RndLightAnim` (140 B). Each would need a
  scatter-include into a neighbouring unit (CharEyes, Rot, MeshAnim
  respectively) to become pairable. Considered, not attempted: that is a splits
  decision with its own blast radius.
- **The +1 function on the re-home is unexplained**, as recorded above.
- **The `HamCamShot.cpp` scatter-include into `BandCamShot.cpp` was not removed.**
  It is DC3 source compiled into an RB3 unit. Removing it is the obvious follow-up
  to item 5 but it touches 467 symbols and needs its own measurement.
- **No other alias group was audited** for the same identification-vs-fold
  confusion. Given that the T1 instrument is structurally one-sided, **other T1
  memberships may carry the same error**; that is a lane-sized sweep.

---

## Gate

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Full coverage (`skipped=0`), run as the lane's last action after all `src/`
edits, per the comment-only-commit precedent.
