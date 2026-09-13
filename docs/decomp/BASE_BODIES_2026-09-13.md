# W10-C — the base-class bodies W9-A newly paired

**Lane:** W10-C · **Branch:** `w10-base-bodies` · **Base:** `dee126a1` · **2026-09-13**

Follow-on to W9-A's map repair (merge `88c944d4`,
`docs/decomp/SPATIAL_SCREEN_2026-09-13.md`), whose handoff was:
*"`??0UIComponent@@` (496 B, 81.07) and `??0RndTransformable@@` (512 B, 73.94)
are correctly paired for the first time — about 1,008 B of ordinary body
porting."*

Both are now **byte-exact**, plus one more found by screening the same seam.

| # | change | predicted | measured | per-unit |
|---|---|---|---|---|
| 1 | `??0UIComponent@@IAA@XZ` | Δfns **+3**, Δbytes **+496** | Δfns **+3**, Δbytes **+496**, Δcode% **+0.004837pp** | `default/UIComponent` 108→111; unit net (ALL) **+3** == whole-binary; **0 fell off** |
| 2 | `??0RndTransformable@@IAA@XZ` + new `ObjOwnerPtr` gate | Δfns **+3**, Δbytes **+512** | Δfns **+3**, Δbytes **+512**, Δcode% **+0.004997pp** | `default/Trans` 116→119; unit net (ALL) **+3**; **0 fell off** |
| 3 | `??0UIListArrow@@IAA@XZ` | Δfns **+3**, Δbytes **+144** | Δfns **+3**, Δbytes **+144**, Δcode% **+0.001408pp** | `default/UIListArrow` 17→20; unit net (ALL) **+3**; **0 fell off** |
| — | Gen probes ×2 | — | **−416 B** and **−972 B** — NOT applied | see §4 |

**Total landed: +9 functions / +1,152 B.** Every prediction hit exactly.
`matched_functions` 42,749 → 42,758; `matched_code` 3,871,308 → 3,872,460;
`matched_code_percent` 37.783768 → 37.795010.

---

## 0. The briefed figures were re-derived, and they held

Standing rule: never inherit a prior lane's number. Read from `report.json` on
the graded ruler (`provenance.diff_config` = `functionRelocDiffs=name_check`,
`ppc.calculatePoolRelocations=false`) after a **full** build of a fresh
worktree — a reflinked tree's target objs are pre-renamer, so any name-keyed
read before the first build is vacuous.

| row | briefed | re-derived | verdict |
|---|---|---|---|
| `??0UIComponent@@IAA@XZ` | 496 B, 81.07 | 496 B, **81.07258** | holds |
| `??0RndTransformable@@IAA@XZ` | 512 B, 73.94 | 512 B, **73.93750** | holds |

Both reproduce. This is the first briefed pair in several waves that did.

## 1. The handoff called it "ordinary body porting". It was not.

Both ctors already existed in source and both were *semantically* correct. The
entire 1,008 B was a **code-generation policy** difference, and the same one in
both rows: **retail INLINES the owner-only smart-pointer constructor where we
emit a `bl`.**

`??0UIComponent@@` — three defects, all read off retail bytes:

1. **`ObjPtr<UIComponent>` at `mNavRight` (0xe4) and `mNavDown` (0xf0).**
   Retail's stream is
   `{mOwner, vptr-lis, mObject, vptr-addi, vptr-store}` — exactly the shape
   `obj/Object.h` documents for `RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT`, so
   both defines are needed, not just the inline one. **81.07 → 94.78**
2. **`ObjDirPtr<ObjectDir>` at `mResourceDir` (0x124) — the OPPOSITE direction.**
   Retail *calls* `??0?$ObjDirPtr@VObjectDir@@@@QAA@PAVObjectDir@@@Z` with
   `li r4,0x0`: the one-arg `ObjDirPtr(C*)` overload, which is declared
   out-of-line. We spelled `mResourceDir()`, binding the in-class default ctor,
   and inlined it. Fixed per-site as `mResourceDir(nullptr)`. **94.78 → 99.06**
3. **Retail never initializes `mSelectingUser` (0xfc).** It stores 0 to `0xf8`
   then to `0x100` and never to `0xfc`. Offset compiler-verified with
   `/d1reportSingleClassLayout`, not read off a header comment. Kept under
   `HX_NATIVE` (house pattern), omitted in the match build. **99.06 → 100.0**

`??0RndTransformable@@` — same root cause, but the class has one member of each
kind, and only one of them had a lever:

| member | type | lever |
|---|---|---|
| `mTarget` (0xa8) | `ObjPtr<RndTransformable>` | existing → **73.94 → 81.45** |
| `mParent` (0x8) | `ObjOwnerPtr<RndTransformable>` | **none existed** → **81.45 → 100.0** |

So change 2 adds **`RB3_OBJOWNERPTR_INLINE_OWNER_CTOR`**, the exact analogue of
the `ObjPtr` pair. Retail's `mParent` stream is
`{mOwner@0xc, vptr-lis, mObject@0x10, vptr-addi, vptr-store@0x8}` and retail
also stores `&mParent` to the EH temp slot (`addi r9,r30,0x8; stw r9,0x50(r31)`)
— the inlined-ctor EH-state signature — which is why the deliberately dead
`if (mObject) AddRef(...)` arm is kept: it folds away but still opens the EH
region that bounds the temp's live range. It is spelled with `OwnerRef()`, not
`this`, because `ObjOwnerPtr`'s ring-ref is `mOwner` (cf. `SetOwnerObj`); dead
code should not teach the wrong ring discipline. The gate requires **both**
defines so a half-opted-in TU is a compile error, not a silent fallback.

## 2. The blast-radius control came free — and it was pre-registered

`obj/Object.h` is a PCH input, so change 2's leg B recompiled **956 TUs**. The
per-unit set-diff shows **exactly one unit moved**. 955 recompiled units
measuring Δ0 is what *proves* the `#ifdef` is inert, rather than assuming it.
This is the directory CLAUDE.md flags as perturbation-prone (`rndobj/`), so the
check was the point.

On all three changes `unit net (ALL units) == whole-binary Δmatched` and units
at 100% were `162→162` (mpn) / `134→134` (fuzzy) with **0 falling off**. That
pairing is the guard against the failure mode the brief names — reading +N
matched while bytes fall because an unrelated perfect row broke.

## 3. Continuing into the seam — and the screen that found it

Rather than guess, screen every **paired sub-100 `??0` row** in `ui/` and
`rndobj/` (26 of them) by **counting `bl` on both sides**. Our build emitting
*more* calls than retail is the inline-policy signature.

```
??0RndGenerator@@       596 B  81.32   bl: target= 5 ours= 9   +4
??0RndPartLauncher@@    328 B  74.87   bl: target= 3 ours= 6   +3
??0UIListArrow@@        144 B  61.22   bl: target= 2 ours= 4   +2
??0RndMat@@             648 B  84.98   bl: target=13 ours=15   +2
??0AnimTask@@           492 B  80.56   bl: target=10 ours=11   +1
??0RndParticleSys@@    1280 B  84.96   bl: target= 8 ours= 9   +1
??0RndGroup@@           540 B  95.56   bl: target= 6 ours= 5   -1
??0RndEnviron@@         416 B    —     bl: target= 6 ours= 6    0
```

⚠ **The first screen I wrote was wrong and would have produced false positives.**
It counted `bl ??0?$Obj{Ptr,OwnerPtr,DirPtr}@…` by NAME. But an unnamed retail
callee renders as `bl fn_8229D7A0`, which the regex cannot see, so
"target has no ObjPtr call" reads identically for *retail inlines it* and
*retail calls an unidentified address*. On `??0RndMat@@` it said "retail
inlines" for eight sites where retail plainly calls `fn_8229D7A0` /
`fn_8229D9C8` — and `name_check` forgives those precisely because `fn_` is a
placeholder. **Counting `bl` of ANY kind on both sides is the instrument that
works.** `RndMat`'s real +2 is unrelated: a `vector<Color>` idiom (retail calls
a `ResetColors` helper, we emit `erase` + `_M_fill_insert`) — a different and
larger class, deferred.

**`??0UIListArrow@@` (change 3)** was the clean hit: both our
`bl ??0?$ObjPtr@…` align against retail's inlined store plus EH-temp store.

★ **But it needed a *different* gate, and that is the transferable lesson.**
Applying the UIComponent/Trans pair took it 61.22 → 88.42 and stopped. The
residual was pure store *order*: retail wants
`{vptr-lis, mOwner, mObject, vptr-addi, vptr-store}` while `DEFER_OBJECT`
emits `{mOwner, vptr-lis, …}`, because `mOwner` still comes from the base
mem-init list and floats above the derived vptr materialization. That is what
`RB3_TU_OBJPTR_DEFER_OWNER` exists for, and the `ui/` neighbour
`UIListLabel.cpp` already pairs it with `RB3_OBJPTR_INLINE_OWNER_CTOR_EH`.
**88.42 → 100.0.**

⇒ *"Retail inlines this"* is only **half** a diagnosis. There are three retail
shapes behind one inline decision, differing only in where the `mOwner` /
`mObject` stores land relative to the vptr store, and choosing the wrong one
leaves a row stuck in the high 80s **looking like a scheduler wall**. Read the
store ORDER off retail bytes before choosing the gate.

## 4. Negative: Gen — a refuted in-tree record, and a vein that needs new machinery

`Gen.cpp` asserted *"retail's `??0RndGenerator@@` calls these ObjPtr ctors OUT
OF LINE"*, citing 81.3% → 61.1% when inlined, and spelled four sites two-arg to
opt out. **Refuted on retail bytes:** `bl` count is retail 5 / ours 9, the four
ObjPtr ctors being exactly the difference. The original inference was
metric-only, and `obj/Object.h` already documents why that is unsafe — inlining
in the wrong store shape scores *worse* than not inlining (RndMultiMeshProxy,
84.8% inlined vs 92.7% called). The 61.1% was a wrong-shape artifact, not
evidence about retail's policy.

Byte-proof: `DEFER_OWNER + INLINE_OWNER_CTOR_EH` with one-arg spellings takes
`??0RndGenerator@@IAA@XZ` to **100.0%**.

**Not applied — both probes are net-negative**, because every gate is TU-WIDE
and `?Load@RndGenerator@@`'s `ObjPtr<RndCam>` one-arg site is currently
byte-exact (measured against 42,758 / 3,872,460):

| probe | ctor | Load | whole-binary |
|---|---|---|---|
| `DEFER_OWNER + INLINE_OWNER_CTOR_EH` | **100.0** | 100 → 95.165 | **−416 B** |
| `INLINE_OWNER_CTOR + DEFER_OBJECT` | 98.55 | 100 → 99.053 | **−972 B** |

Reverted to the committed spelling — the better-evidenced state, since Load is
byte-exact there and byte-exactness outranks a ctor stuck at 98.55. The
misleading comment was corrected in place so the next lane does not re-derive
the refutation from the metric, which will lie the same way.

## 5. Native behaviour

One change has a native-visible effect, and it is deliberately **neutralised**:
retail leaves `UIComponent::mSelectingUser` uninitialized, so the match build
now omits that store, but the initializer is retained under `HX_NATIVE`. The
native runtime therefore still starts that pointer at null and its behaviour is
**unchanged** by this lane.

No other change alters semantics: the smart-pointer gates only move *where* the
same three stores are emitted, and the `ObjDirPtr` change swaps a default ctor
for the one-arg ctor with a null argument. **No driver in this lane exercised
any UI or transform path at runtime** — this is a codegen-shape argument backed
by retail bytes and by the native build gate, not by runtime coverage.

## 6. What I did NOT do

- **`??0RndMat@@` (648 B)** — its +2 `bl` is a `vector<Color>` resize idiom, not
  this defect class. Widely-inherited `rndobj/` base; worth a lane.
- **`??0AnimTask@@` (492 B)** — deliberately skipped: it is *not* purely this
  class. Retail calls `??0Object@Hmx@@` where we call `??0Task@@` (different
  base), and retail's member is `ObjOwnerPtr<CharInterest>` where ours is
  `ObjOwnerPtr<RndAnimatable>` — a member **type** divergence. Bigger problem.
- **`??0RndParticleSys@@` (1280 B)**, `??0RndPartLauncher@@` (328 B),
  `??0RndGroup@@` (−1 `bl`: retail calls where we inline) — screened, ranked,
  untouched.
- **No permuter** (off by directive), no map or splits edits, no alias edits
  (`scripts/symbol_aliases.json` untouched).
- The three idle obj patchers, the ceiling, and the `auto_*` class were not
  re-measured; nothing here depends on them.

## 7. Handoffs

1. **A `DEFER_OWNER` branch on the ONE-ARG in-class `ObjPtr` ctor.** Only the
   two-arg ctor has one today (`obj/ObjPtr_p.h`). With it, a TU could give its
   one-arg sites retail's `{vptr-lis, mOwner, …}` order *without* force-inlining
   the two-arg ctor that the per-site opt-out depends on — which is the exact
   thing blocking Gen's 596 B. Shared-header change; wants its own lane and a
   PCH-cascade control like §2's.
2. **Re-screen with the `bl`-count instrument beyond `ui/`+`rndobj/`.** It is
   cheap, has an obvious failure mode that the named-callee version does not,
   and there were 239 paired sub-100 `??0` rows binary-wide against the 26 I
   screened.
3. **`??0RndMat@@`'s `ResetColors` idiom** — retail calls a free
   `ResetColors(vector<Color>&, …)` helper where we emit `erase` +
   `_M_fill_insert`. 648 B in a widely-inherited base.
