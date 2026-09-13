# W11-C — the one-arg DEFER_OWNER branch, and the binary-wide `bl`-count re-screen

**Lane:** W11-C · **Branch:** `w11-ctor-inline-sweep` · **Base:** `77cac933` · **2026-09-13**

Follow-on to W10-C (`docs/decomp/BASE_BODIES_2026-09-13.md`, merge `c5297f13`),
executing its handoffs #1 (a `DEFER_OWNER` branch on the **one-arg** in-class
`ObjPtr` ctor) and #2 (re-screen with the `bl`-count instrument beyond
`ui/`+`rndobj/`).

**Total landed: +10 functions / +920 B / +0.008980 pp.**
`matched_functions` 42,766 → 42,776; `matched_code` 3,874,292 → 3,875,212;
`matched_code_percent` 37.812890 → 37.821870. Every prediction hit exactly.

| # | change | predicted | measured | per-unit |
|---|---|---|---|---|
| 1 | `obj/Object.h` one-arg `DEFER_OWNER` branch | Δ **exactly 0** | Δfns **+0**, Δbytes **+0**, Δcode% **+0.000000pp** | 956 TUs recompiled, **0 units moved**, 0 fell off |
| 2 | `Gen.cpp` — 4 sites one-arg + `DEFER_OWNER` | Δfns **+1**, Δbytes **+596** | Δfns **+5**, Δbytes **+596**, Δcode% **+0.005817pp** | `default/Gen` 71→76; unit net (ALL) **+5** == whole-binary; **0 fell off** |
| 3 | `Instance.cpp` + `EventTrigger.cpp` + `PartLauncher.cpp` | Δfns **+5**, Δbytes **+324** | Δfns **+5**, Δbytes **+324**, Δcode% **+0.003163pp** | `EventTrigger` 264→267, `Instance` 94→96; unit net (ALL) **+5**; **0 fell off** |

Baselines re-derived, never inherited: the brief's "239 paired sub-100 `??0`
rows" re-measures to **237** at `77cac933`, and W10-C's `??0RndGenerator@@`
figures (596 B, 81.32, `bl` 5 vs 9) reproduce exactly.

---

## 1. The instrument, and proving it discriminates before trusting a negative

W10-C's screen counted `bl` **by callee name**, so an unnamed retail callee
(`bl fn_8229D7A0`) was invisible and *"retail inlines this"* read identically to
*"retail calls an unidentified address"* — it false-positived eight sites on
`RndMat`. The corrected instrument counts `bl` of **any** kind on both sides.

I did not reuse W10-C's script; I rebuilt it on `objdiff-cli diff --format json
--include-instructions`, which emits per-instruction `target`/`base` objects with
an `opcode` field. That is the **ruler's own view of both sides**, so it needs no
COFF reader of mine — deliberately avoiding `tools/coff_bodies_ext.py`, whose
known one-sided defect (billing the successor symbol's EH-funclet prefix into a
COMDAT span, `project_one_sided_instrument_error_invisible_to_two_sided_control_2026-08-16.md`)
would land squarely on a `bl` count. Run against `-p <worktree>`, so it picks up
`objdiff.json`'s pinned graded ruler rather than `diff`'s own base config.

**Two validations, both of which could have failed:**

1. **Known-answer fixture.** W10-C published six `(target bl, our bl)` pairs.
   Mine reproduces **five exactly** — `RndGenerator` 5/9, `RndPartLauncher` 3/6,
   `RndParticleSys` 8/9, `AnimTask` 10/11, and critically `RndGroup` **6/5**, the
   one *negative* delta (retail calls where we inline), which is the direction a
   broken counter would most easily lose. The sixth, `RndMat`, reads 13/**14**
   against W10-C's 13/15 at an identical fuzzy of 84.98148.
2. **Completeness, which is the real anti-vacuity check.** For all 237 rows,
   counted instructions × 4 == the reported size, **on both sides, 237/237, zero
   errors**. The instruction list is not truncated, so a `bl` count of zero is a
   real zero rather than a silently-empty read.

⚠ W10-C listed its `AnimTask` row as `??0AnimTask@@` and I first recorded it
"absent from report.json". That was **my** lookup error, not a vanished row —
W10-C had abbreviated the symbol, and the row is
`??0AnimTask@@QAA@PAVRndAnimatable@@MMM_NM@Z` (492 B, 80.56, 10/11), present and
reproducing. Corrected here so the next lane does not chase a phantom.

## 2. The screen: 237 rows → 19 → 5

Of **237** paired sub-100 `??0` rows (70,904 B):

| class | rows | bytes |
|---|---:|---:|
| positive delta (we emit MORE `bl` — the inline-policy signature) | **19** | 10,832 |
| zero delta — **not this defect class** | 209 | 57,308 |
| negative delta (retail calls where we inline) | 9 | 2,764 |

A `bl` delta alone is **not** the diagnosis, so I extended the screen to list
*which* callees we emit that retail does not. That immediately splits the 19:

- **Five rows where every ours-only callee is `ObjPtr`/`ObjOwnerPtr`-family and
  the delta equals that count** — Gen (596 B, 4 sites), PartLauncher (328 B, 3),
  SharedGroup (244 B, 1), `FileMerger::Merger` (216 B, 1), `EventTrigger::Anim`
  (188 B, 1 `ObjOwnerPtr`). **1,572 B.**
- **The rest are other defect classes** and the callee list says so plainly:
  `Splash` (6.89%) differs by `Timer`/`CriticalSection`/`SynchronizationEvent`
  ctors, `NetworkEmulator` (48.17%) by Quazal `PseudoSingleton`/`SignalError`,
  `TransformArea` (13.55%) by copy-ctors + `memcpy`, `VocalTrack` by
  `_Deque_base`, `RndMat` by its known `vector<Color>` idiom. **Ranking on the
  `bl` delta alone would have sent a lane at `TransformArea` (+4, the joint
  second-largest delta) for a defect that has nothing to do with smart pointers.**

## 3. Task 1 — the one-arg `DEFER_OWNER` branch

`obj/Object.h`, gated on **both** `RB3_OBJPTR_INLINE_OWNER_CTOR` and
`RB3_TU_OBJPTR_DEFER_OWNER`. Body is the exact one-arg analogue of the DEFER-BOTH
two-arg body in `obj/ObjPtr_p.h`: route through the empty `ObjRefConcrete()` base
and assign `mOwner` *and* `mObject` in the derived body, so both stores are pinned
after the derived vptr store. AddRef arm retained, for the reason the two sibling
branches retain it.

**Why it was needed.** DEFER_OWNER order was previously reachable only by pairing
`RB3_TU_OBJPTR_DEFER_OWNER` with `RB3_OBJPTR_INLINE_OWNER_CTOR_EH`, which resolves
to the **force-inlined two-arg** ctor. That destroys the per-site opt-out the same
header documents: with the two-arg overload force-inlined, `mFoo(this, nullptr)`
no longer buys a real `bl`, so a TU whose sites disagree cannot express the
disagreement at all. Reaching DEFER_OWNER through the **one-arg** ctor leaves the
two-arg overload declared-only/out-of-line, so the opt-out survives.

**The PCH-cascade control.** `Object.h` is a PCH input. Pre-registered: since **no
TU in the tree defines both macros**, the branch is never selected by the
preprocessor, so the change is inert *by construction* and not merely by
measurement — predicted Δ exactly 0. Measured: leg B recompiled **956 TUs** and
moved **nothing** (Δmatched +0, Δbytes +0, Δcode% +0.000000pp, Δfuzzy
+0.000000pp, units at 100% 163→163 mpn / 135→135 fuzzy, 0 reached, 0 fell off).

⚠ Per the brief's hazard, an isolation control licenses nothing about the composed
merge — W10-C saw fourteen units move on merged main from a shared template COMDAT
resolving differently. That mechanism is weaker here (the new branch is never
*textually selected* in any TU), but it is not zero, and the merging lane should
re-verify rather than inherit this Δ0.

## 4. ⛔ W10-C's handoff #1 predicted the right machinery for the WRONG reason

The handoff said the one-arg branch would let *"the ctor's four one-arg sites take
retail's order while Load's one-arg `ObjPtr<RndCam>` site keeps the shape it
already matches with."*

**That reasoning cannot be right, and noticing so before measuring was the
lane's main analytical step.** Gen's four ctor sites were spelled **two-arg**;
`Load`'s site is **one-arg**. Flipping the ctor to one-arg makes *all five* sites
one-arg in the same TU, and every gate here is TU-wide — so a one-arg gate
**cannot** separate them. The handoff's stated mechanism describes a
discrimination that does not exist.

It works anyway, for a different and now-measured reason: **`Load`'s site is a
LOCAL, not a member.** DEFER_OWNER's whole mechanism is moving member stores past
the *enclosing class's* vptr store; a local `ObjPtr<RndCam> cam(this)` is the
most-derived object, so there is no such store and the shape change is a no-op
there. Measured: `?Load@RndGenerator@@` stays byte-exact at **100.0**.

⇒ **Members are sensitive to DEFER_OWNER; locals are not.** That is the reusable
rule, and it is what makes the per-site opt-out unnecessary for Gen — not the
one-arg/two-arg distinction the handoff named. A lane that had taken the handoff
at face value would have concluded the lever could not work the moment it read
Gen's source, and dropped a collectable 596 B.

**Gen result:** `??0RndGenerator@@IAA@XZ` 81.32 → **100.0**, `?Load@` 100.0 held,
whole-binary **+5 fns / +596 B**, exactly one unit moved, 0 fell off. W10-C's two
probes of the same row measured **−416 B** and **−972 B**; the missing branch is
the entire difference.

## 5. Task 2 results, including two partials that collect nothing

- **`??0SharedGroup@@` (Instance.cpp), 90.41 → 100.0, 244 B collected.** One
  ours-only `ObjPtr<WorldInstance>` = the `mPollMaster(this)` member. TU had no
  gate at all; added `INLINE_OWNER_CTOR` + `DEFER_OWNER`.
- **`??0RndPartLauncher@@`, 74.87 → 96.29, 0 B collected.** Structurally Gen's
  twin (TU already opted in for `Load`'s one-arg local; ctor spelled its three
  members two-arg). Shape picked by measurement, not guess: **DEFER_OWNER 96.29
  vs plain one-arg 77.07.**
- **`??0Anim@EventTrigger@@`, 62.91 → 95.74, 0 B collected.** The `ObjOwnerPtr`
  lever W10-C built, applied to its first new consumer.

⚠ **`matched_code` is all-or-nothing per row, so 96.29 and 95.74 collect exactly
zero bytes.** Both are kept on **accuracy, not headline** (standing directive):
retail demonstrably inlines those ctors, and we now reproduce that in retail's
store order. The +324 B of change 3 is essentially all SharedGroup.

★ **PartLauncher's residual is NOT the defect it looks like.** Every inlined
`ObjPtr` block aligns byte-for-byte with retail. What remains is an **EH-frame**
difference: we emit one extra EH-state store (`li r11,0x1; stw r11,0x50(r31)`)
which displaces all three EH temps from retail's `0x50` to `0x54`, plus a `this`
home-slot store retail lacks — a 340 B frame against retail's 328. Reading this
off the aligned streams is what stopped it being logged as "wrong shape, keep
trying gates"; there is no store-order gate that closes it.

## 6. What I deliberately did NOT do

- **`FileMerger::Merger` (216 B, 1 ours-only `ObjPtr<ObjectDir>`)** — a clean
  candidate by the screen. Not applied: the ctor definition is not in the unit's
  own `.cpp` and locating the right TU to gate needed more time than remained.
  The screen evidence is recorded below; this is the cheapest unclaimed row.
- **The 14 mixed positive-delta rows** — `BandDirector` (1088 B, +5),
  `VocalTrack`, `BandCharacter` (2180 B), `CharDriver`, `CharFaceServo`,
  `CrowdAudio`, `PatchPair`, `RndParticleSys`, `AnimTask`. Screened, ranked,
  untouched; several are genuinely mixed (retail calls *some* of the same ctors).
- **The four low-fuzzy rows** (`Splash` 6.89, `TransformArea` 13.55, `Char3D`
  15.33, `NetworkEmulator` 48.17) — the callee lists show these are other defect
  classes, not this seam.
- **`??0RndMat@@`** (648 B) — W10-C's deferred `ResetColors` class, confirmed
  unrelated here. **`??0AnimTask@@`** — different base class + member-type
  divergence, per W10-C.
- **No permuter** (off by directive), **no map, splits or alias edits**
  (`scripts/symbol_aliases.json` untouched). No `??0`-adjacent header layout
  changes. The ceiling, the `auto_*` class and the obj-patcher census were not
  re-measured; nothing here depends on them.

## 7. Handoffs

1. **The EH-frame wall behind `??0RndPartLauncher@@` (328 B) — and it is a
   CLASS, not one row.** Store shape is solved; the residual is one surplus EH
   state store displacing every EH temp by 4. `??0Anim@EventTrigger@@` (188 B)
   sits at 95.74 with the store shape likewise solved. A lever that suppresses
   the extra EH state (or matches retail's state numbering) plausibly collects
   **both** rows, 516 B, and would apply to every future
   inline-the-owner-ctor row that lands in the mid-90s.
2. **`FileMerger::Merger`, 216 B** — screened clean (1 ours-only
   `ObjPtr<ObjectDir>`), just needs its defining TU located and gated.
3. **Re-run the screen after any inline-gate wave.** It costs ~30 s for all 237
   rows (`objdiff-cli diff -f json`, 8-way parallel) and its failure mode is
   checkable (instructions × 4 == size on both sides). The ranked list here is a
   snapshot of `77cac933`; the population moves.
4. **Rule to carry forward: members are sensitive to DEFER_OWNER, locals are
   not.** This is what lets a TU mix an inlined member ctor with a byte-exact
   local site under one TU-wide gate, and it is why Gen was collectable at all.
