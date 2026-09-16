# W16-EM — `XboxPurchaser` has no `Hmx::Object` base: a live 2x over-allocation, settled on retail RTTI

Lane W16-EM, 2026-09-16, worktree `~/tmp/wt-w16-em`, branch `w16-em`, off `b4044f70`.
Ruler: shipped graded `name_check` (read from `build/45410914/report.json`, not assumed).

## Result in one line

**ADJUDICATED (A) — FIX. Measured +1,028 B / +0 functions / +0.010033 pp**, from one row
crossing (+1,104 B) minus one row I broke and chose not to chase (−76 B). `sizeof` goes
**0x50 (80) -> 0x28 (40)**: every site that allocates an `XboxPurchaser` was allocating
**twice** what retail does.

## 0. Verdict table

| claim | outcome |
|---|---|
| brief: retail `sizeof(XboxPurchaser) == 0x28`, ours `0x50` | **CONFIRMED**, five instruments |
| brief: retail has no `Hmx::Object` subobject | **CONFIRMED** on retail RTTI + ctor + dtor |
| brief: `mState@0xc` proves the missing base | **TRUE BUT NOT SUFFICIENT ALONE** — see §2 |
| brief: `0x1c` is the destruction test | **PASSED**, but only after correcting the member ORDER |
| brief's consumer list (6 units) | **OVERSTATED** — 3 of them contain zero `Purchas` references |
| `PurchaseMade()` returning `false` | **NEW — a live behavioural bug**, retail returns `[0x1c]` |
| `XboxMultipleItemsPurchaser` | **NOT IN RETAIL AT ALL** (0 occurrences); DC3-only |

## 1. Why this was not obvious: DC3 says the same wrong thing

`../dc3-decomp/src/system/meta/StorePurchaser.h:40` declares
`class XboxPurchaser : public StorePurchaser, public Hmx::Object` — identical to ours. This
is not a porting slip someone made here; it was inherited from the twin. Per CLAUDE.md's
standing caveat, **DC3 is NEWER than RB3**, and the `Object` base is one of its additions.
A source-diff against the oracle shows nothing, which is exactly why this survived.

## 2. The rival hypothesis the brief did not consider — and why `mState@0xc` alone cannot settle it

The brief's argument was: retail reads `mState` at `0xc`, we declare it at `0x34`, the
difference `0x28` is exactly the `Hmx::Object` contribution, therefore retail has no
`Hmx::Object`.

**That inference is not valid on its own.** Under MSVC multiple inheritance, a *secondary*
base's vtable slots receive a `this` already adjusted to that subobject. So if retail were
declared the other way round —

```cpp
class XboxPurchaser : public Hmx::Object, public StorePurchaser   // rival
```

— then `Hmx::Object` occupies `0x0..0x28`, `StorePurchaser` sits at `0x28`, and `mState` at
absolute `0x34` is read from inside those slots as **`this+0xc`**. The rival reproduces the
observed `0xc` with the `Object` base still present. `0xc` does not discriminate.

It is refuted by RTTI (§3): the rival predicts `StorePurchaser mdisp=0x28`; measured `0x0`.

## 3. The decisive instrument: retail's own RTTI base-class array

Retail is `/GR` (2,220 `??_R4` COLs, CLAUDE.md), so it *enumerates its own bases*.
Walked straight out of `orig/45410914/band.exe` (PE parsed in Python — never `grep`, which is
binary-blind in this shell):

```
??_R4 @0x821ec094   signature=0  offset=0x0   -> '.?AVXboxPurchaser@@'
  ??_R3  attributes=0x0   numBaseClasses=2
  ??_R2  [0] .?AVXboxPurchaser@@    mdisp=0x0
         [1] .?AVStorePurchaser@@   mdisp=0x0
```

Two facts, both load-bearing:

* **`numBaseClasses == 2`.** With an `Object` base it would be **four** — `Object@Hmx` *and*
  its own base, which retail's own `??_R3` for `.?AVObject@Hmx@@` shows is `.?AVObjRef@@`.
* **`attributes == 0x0`.** The `0x1` multiple-inheritance bit is **clear**.

Corroboration from a second angle: scanning all of `.rdata`, **exactly one `??_R4` references
XboxPurchaser's type descriptor**. A class with two vptrs emits one COL per vtable.

## 4. Retail's constructor and destructor: one vptr, written at offset 0

```
fn_827B2800 (ctor)         fn_827B28A0 (dtor)
  stw r8, 0x4(r3)   mSource      stw r10,0x0(r3)   vptr := 0x82115258  (derived)
  stw r9, 0x8(r3)   mUserIndex   lwz r11,0xc(r3)   mState; IsPurchasing inlined
  std r5, 0x10(r3)  mOfferID     ... XCancelOverlapped(&sOverlapped) if pending ...
  stw r10,0xc(r3)   mState = 0   stw r11,0x0(r31)  vptr := 0x8211523c  (base)
  stw r11,0x0(r3)   vptr
  stw r4, 0x18(r3)  mUserIndex
```

The ctor stores **one** vptr and calls **no** base constructor. The dtor writes that **same
single slot twice** (derived table, then base table) and calls no `Object` destructor and no
`RemoveSink`. A multiple-inheritance object writes both vptrs in both places.

## 5. The `0x1c` destruction test — it PASSES, but the brief's member order was wrong

The brief correctly flagged the falsifier: slot [4] is `lbz r3,0x1c(r3); blr` and `Poll`
writes `stb r11,0x1c(r30)`, so **something must live at 0x1c**. Applying the brief's assumed
order (`mState, unk3c, mOfferID, mUserIndex`) puts `0x1c` *inside* `mOfferID` — which looks
like a refutation and is not. Retail's real order, read off instructions:

| off | member | the instruction that proves it |
|---|---|---|
| 0xc | `mState` | ctor `stw r10,0xc`; IsPurchasing `@0x827b2828` tests it vs {0,2,3}; Poll vs 1 |
| 0x10 | `u64 mOfferID` | ctor **`std`** (8-byte); Initiate passes `addi r5,r31,0x10` as `pOfferIDs`; Poll `ld r3,0x10(r30)` into `?IDToOfferString@StorePurchaseable@@SAX_KAAVString@@@Z` — a **named** callee taking `unsigned __int64` |
| 0x18 | `int mUserIndex` | ctor `stw r4,0x18`; Initiate `lwz r3,0x18(r31)` as `dwUserIndex` |
| **0x1c** | **`bool mPurchaseMade`** | **free byte**; Poll `stb r11,0x1c` on all four exits; slot [4] `lbz r3,0x1c` |
| 0x20 | `HRESULT mResult` | Initiate `addi r7,r31,0x20` as `phrResult` + zeroes it; Poll compares it against `0x8057F001/2/3` and (via the `0x8057F001+0x7FA80FFF` wrap) `S_OK` |

`0x20 + 4 = 0x24`, rounded to the 8-byte alignment `mOfferID` forces = **0x28 = 40**.

⛔ **No member here is invented to reach the size.** Each is load-bearing at a named retail
address. That distinction is the whole reason this is a fix and not the `SetlistArtRecord`
anti-pattern (88 B, dropped rather than invent 60 bytes of unknown members).

`unk3c` — a member this header carried with no attestation — **does not exist**; nothing in
the tree referenced it.

## 6. Three call sites, all 40

| site | evidence |
|---|---|
| `UGCPurchasePanel::Poll` `0x8263edf0` | the lane's target row |
| `TokenRedemptionPanel::ShowPurchaseUIForOffer` `0x8263fb98` | `li r3,0x28` -> `bl fn_827bd2f0` -> `bl fn_827b2800` |
| `StandIn` `0x825ecb54` | same chain |

⚠ Two probe traps, both real, both recorded because they look like refutations:
the `li r3,0x28` sits **four instructions** before the ctor `bl` (the allocator call is in
between), so a narrow grep window finds nothing; and the `stw r3,0x50(r31)` on the next line
at the StandIn site is **StandIn's own field offset**, a coincidence of value, not a size.

## 7. Compiler ground truth, after the change

`scripts/harvest/class_layout_report.py XboxPurchaser --project-dir <wt> --exact` (the
compiler is authoritative; the `// 0xHEX` comments are derived and can be wrong):

```
=== XboxPurchaser   sizeof = 40 (0x28) ===
  0x0 {vfptr}[StorePurchaser]  0x4 Symbol mSource  0x8 mUserIndex
  0xc mState  0x10 mOfferID  0x18 mUserIndex  0x1c mPurchaseMade  0x20 mResult
=== vtable XboxPurchaser (6 slots) ===   [ONE table; was two, 6 + 21, with a Handle -12 adjustor]
```

Every offset equals the retail-attested offset in §5. Before: `sizeof = 80`, vfptrs at `0x0`
**and `0xc`**, `Object` spanning `0xc..0x34` = exactly `0x28`.

## 8. Measured blast radius

Real radius is **4 TUs + 2 headers**, not the six units the brief listed:
`MusicLibraryStore.cpp`, `SetlistToStorePanel.cpp` and `StandIn.cpp` contain **zero**
`Purchas` references. Only `UGCPurchasePanel.cpp` and `StorePanel.cpp` construct one.

Whole-binary A/B (`tools/ab_measure.py --from-dirty`, both legs settled to zero work,
`objdiff-cli` sha pinned across legs):

```
leg A: matched=43957 masked=23224 honest=20733 code%=40.248314
leg B: matched=43957 masked=23224 honest=20733 code%=40.258347
Δmatched=+0  Δcode_bytes=+1028  Δcode%=+0.010033pp
  +1  default/band3/meta_band/UGCPurchasePanel  (40->41)
  -1  default/StorePurchaser                    (1->0)
```

`?Poll@UGCPurchasePanel@@UAAXXZ`: 1,104 B, fuzzy **99.996376 -> 100.0**.

## 9. Pre-registration vs measurement

| pre-registered | measured |
|---|---|
| `Poll` crosses, +1,104 B | ✅ crossed |
| `ShowPurchaseUIForOffer` improves, does not cross | ✅ (42.674698, unchanged rank) |
| **`??_GXboxPurchaser` (76 B) HOLDS at 100** | ❌ **fell to fuzzy 64.0** |
| whole binary +1,104 B / +1 fn | **+1,028 B / +0 fn** |

`+1,104 − 76 = +1,028` exactly. The headline was right; the risk I named as failure mode #1
is the one that fired. Recording that as a miss rather than rounding it into the win.

## 10. What I deliberately did NOT do

* **Did not chase the −76 B.** `??_GXboxPurchaser` shows target 76 B vs base **68 B** — our
  deleting destructor is 2 instructions shorter because `~XboxPurchaser()` became *trivial*
  and MSVC inlined it, dropping retail's `bl fn_827B28A0` and a register save/restore pair.
  Restoring it means porting retail's destructor body — `if (IsPurchasing() &&
  sOverlapped.InternalLow == ERROR_IO_PENDING) XCancelOverlapped(&sOverlapped);` with a
  class-static `XOVERLAPPED` (retail's lives at `0x82e0684c`, referenced by the ctor path,
  `Initiate` and `Poll` alike; DC3 has exactly this idiom on the sibling class). That is a
  **body port, not a layout fix**, and it would add a new undefined XDK symbol
  (`XCancelOverlapped`) to a shared `src/system/` TU that the **native target links** — the
  exact failure class the native gate exists to catch — to buy 0.0007 pp. Handed up as a
  sized, attested follow-up. ⛔ The cheap alternative (a dummy statement to force
  out-of-lining) is metric-fitting and was refused.
* **Did not port `Initiate`/`Poll` bodies.** Ours still use `XShowMarketplaceUI` where retail
  uses `XShowMarketplaceDownloadItemsUI` + `XGetOverlappedResult`. Separate lane.
* **Did not touch `XboxMultipleItemsPurchaser`.** It appears **0 times** in retail — a
  DC3-only class — so it cannot be scored, and changing it is pure risk. It still derives
  from `Hmx::Object`; that is now the only `Hmx::Object` user in this header.
* **Did not change `IsPurchasing`'s body.** Retail `@0x827b2828` tests `mState` against
  `{0,2,3}` (a 3-way compare) where ours is `mState == purchasestate1`. Behaviourally
  equivalent over the enum's range but a different codegen shape. It is **not pinned to our
  unit** (it lives in the `MeshAnim.s` auto region), so changing it cannot score, and the
  retail shape would be guesswork about the original source form.

## 11. Durable lessons

1. **`mState@0xc` was the right clue and the wrong proof.** A member offset read from a
   vtable slot body is measured *relative to the subobject that slot belongs to*, so under
   multiple inheritance it cannot distinguish "no second base" from "bases in the other
   order". **RTTI's `??_R2` `mdisp` is the instrument that discriminates**, and on a `/GR`
   binary it is free.
2. **A destruction test can fail against an assumed member order rather than against the
   hypothesis.** `0x1c` looked like it fell inside `mOfferID` and briefly looked like a
   refutation; the real order (from the ctor's **`std`**, an 8-byte store) puts `mOfferID` at
   `0x10` and leaves `0x1c` free. Derive the order from stores before testing an offset
   against it.
3. **An ICF fold survivor's NAME is not evidence about the class.** Slot [4] resolves in the
   map to `?Fail@ChunkStream@@UAA_NXZ` — `lbz r3,0x1c(r3); blr` folds with every class
   returning a bool at `0x1c`. The *offset* survives the fold; the *name* carries nothing.
4. **A correct layout fix can cost a row.** Making the destructor honest made it trivial,
   and trivial destructors inline. That is a real cost of a real fix, not a reason to undo it.
