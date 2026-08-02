# `?Copy@RndSpline@@` adjudicated — MAP DEFECT CONFIRMED (lane DA-4, 2026-08-02)

**REPORTED, NOT EDITED.** Lane DA-4 owns `docs/`, not the map. Adjudicated on retail
content only; no metric instrument was used, and none could have settled this (a
wrong callee/constant is reloc-masked and scores 100 — see the Known-traps rule in
`docs/INDEX.md`).

## The claim under test

Lane CY-2 (`b67464fe`) flagged `?Copy@RndSpline@@` in `Line.cpp` as its **weaker**
mispair suspect: *"class absent from that TU, 0 literal support."* The coordinator
briefed lane DA-4 to **expect a refutation**.

## Verdict: MISPAIR CONFIRMED. The expectation does not hold.

Two map rows, both inside `Line.cpp`'s pinned span `0x824791F8–0x8247B8C0`
(`config/45410914/splits.txt:5111`), are a body/thunk pair:

| VA | current (wrong) name | correct name |
|---|---|---|
| `0x8247a6c0` | `?Copy@RndSpline@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | `?SyncProperty@RndLine@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` |
| `0x8247b7e0` | `?Copy@RndSpline@@$4PPPPPPPM@A@AAXPBV…` | its `$4PPPPPPPM@A@` thunk |

The 16-byte thunk at `0x8247b7e0` (`lwz r11,-4(r3); subf r3,r11,r3; b 0x8247a6c0`)
branches straight into the body.

**The body at `0x8247a6c0` is `RndLine::SyncProperty`** — 248 B, plus a 32 B
static-guard unwind funclet at `0x8247a7b8` with its own `.pdata` entry. Evidence,
all read out of `orig/45410914/band.exe`:

1. **Shape is impossible for `Copy`.** `Copy(const Hmx::Object*, CopyType)` is `void`
   with 2 params. This body consumes r3–r7 (4 params) and returns a bool
   (`clrlwi`/`addic`/`subfe`). It opens `lha r11,8(r5); cmpw r6,r11; → li r3,1` —
   Milo's canonical `if (i == prop->Size()) return true;` PROPSYNC terminator.
2. **The literal, read from the binary.** The static `Symbol` is built from
   `0x8205B170`, which is `77 69 64 74 68 00` = **`width`**.
3. **Compiler-authoritative layout agrees.** The body `PropSync`s a float at
   `this-0x34`. `scripts/harvest/class_layout_report.py RndLine` puts `mWidth` at
   `0xd8` and the `Object` vbase vfptr at `0x10c` ⇒ `0xd8 − 0x10c = -0x34`. ✔
   Control (theory-free): `?Print@RndLine@@UAA` at `0x8247a560` reads five members and
   prints `"   width: "`, `"   points: "`, `"   foldAngle: "`, `"   hasCaps: "`,
   `"   linePairs: "` — **all five offsets reproduce** under the same `−0x10c` rule.
4. **Callees resolve consistently:** `PropSync` (`float&` overload) on `this-0x34`;
   `RndDrawable::SyncProperty` on `this-0xe4`; `RndTransformable::SyncProperty` on
   `this-0x30`; `??0Symbol@@QAA@PBD@Z`.
5. **The oracle is a literal structural match.** `~/code/milohax/rb3/src/system/
   rndobj/Line.cpp:808` is `BEGIN_PROPSYNCS(RndLine) / SYNC_PROP(width, mWidth) /
   SYNC_SUPERCLASS(RndDrawable) / SYNC_SUPERCLASS(RndTransformable) / END_PROPSYNCS`
   — exactly one own property plus those two supers. (Our DC3-derived `Line.cpp:70-85`
   lists seven properties; RB3-era retail has one. Worth noting independently.)
6. `?SyncProperty@RndLine@@UAA…` is **absent from the entire map**, while its `$4`
   thunk name is present — the slot this body should occupy.

## Both CY-2 premises, re-graded

- **"RndSpline absent from the `Line.cpp` TU" — HOLDS**, three ways: zero `RndSpline`
  references in our `src/system/rndobj/Line.{h,cpp}`; zero in DC3's; and both our
  `Spline.h:10` and DC3's declare `class RndSpline : public RndPollable`, *not*
  RndDrawable+RndTransformable — so RndSpline cannot be the class whose `SyncProperty`
  chains to those two. (rb3-Wii has no `RndSpline` at all.) A separate pinned
  `Spline.cpp` unit exists, with spans elsewhere.
- **"0 literal support" — true but MISGRADED as weak.** The body is not literal-poor:
  it interns one literal, and that literal *identifies a different class*. This is
  **evidence against**, not absence of evidence. CY-2 ranked this suspect below
  `?Copy@HamMove@@` on literal *count*; on literal *content* it is at least as strong.
  ⇒ Grade mispair suspects on whether the literals **coherently describe another
  function**, not on how many there are.

## ⛔ The repair is BLOCKED by a name collision — do not apply naively

`0x8247b638` is currently labelled `?SyncProperty@RndLine@@$4` but actually branches to
`0x8247b930` = `??_GRndLine@@UAA`, i.e. it is the `??_E`/`??_G` deleting-destructor
thunk. So renaming `0x8247b7e0` to `SyncProperty@RndLine@@$4` **collides** unless
`0x8247b638` is corrected in the same edit. Any repair must be a single atomic edit
with the duplicate-NAME count checked before and after.

## Bycatch — flagged, NOT adjudicated

The whole vbase-thunk run `0x8247b620–0x8247b810` looks scrambled, consistent with
`_bijection_arbitrary` assigning names across 16 B thunks that are byte-identical but
for a masked branch target. Resolved target vs label:

- `0x8247b7c0` labelled `?SetType@RndSpline@@$4` → `0x8247b660` = `?SetType@RndLine@@UAA`
- `0x8247b648` labelled `?ClassName@RndSpline@@$4` → `0x8247b5f0`, which calls
  `?StaticClassName@RndLine@@SA` — conflicts with the existing `ClassName@RndLine` pair
  at `0x822896e0`/`0x82289748`; **not** adjudicated which is genuine
- `0x8247b7f0` labelled `?Load@RndLine@@$4` → `0x8247a218` = `?Save@RndLine@@UAA`
- `0x8247b7d0` (Print) and `0x8247b800` (Handle) are **correct**

Also unadjudicated: `?Load@RndSpline@@UAA` @ `0x8247bed0`; the genuine
`?Copy@RndLine@@UAA` @ `0x8247c050`; and `?SyncProperty@RndSpline@@UAA` @ `0x82406178`,
which sits beside `?Copy@RndDrawable@@UAA` and `??0RndDrawable@@IAA` — a
`Draw.cpp`-shaped neighbourhood, and plausibly mislabelled the same way.
