# `StorePurchaser` / `XboxPurchaser` — the +1 slot, and a slot-ORDER swap the count sweep could not see (2026-08-27, lane PURCHASER)

> Companion to `VTABLE_SLOT_COUNT_FIXES_2026-08-20.md`. **Deliberately a
> separate file** — four concurrent lanes collided on section numbers in that
> doc and four more were running when this lane started. Nothing here is
> appended there; cross-reference only.
>
> Prior art this builds on and CORRECTS:
> - `EH_FUNCLET_CASCADE.md:149` — already found `NeedsEnum` had zero RB3 call
>   sites and "was shifting `Poll` 0x14 → 0x18". That lane **parked** it at the
>   tail instead of deleting it. Parking restored `Poll`'s slot but left the
>   COUNT wrong — and, in the same edit, introduced the 3/4 swap fixed below.
> - `VTABLE_SLOT_COUNT_FIXES_2026-08-20.md` §17b — the "compensating errors"
>   shape. This lane hit exactly that shape and handled it atomically.

## 0. Verdict

| claim | outcome |
|---|---|
| brief: `StorePurchaser` retail 6 / ours 7 | **CONFIRMED on retail bytes** |
| brief: `XboxPurchaser` retail 6 / ours 7 | **CONFIRMED on retail bytes** |
| brief: same +1, one spurious virtual on the base | **CONFIRMED — it is `NeedsEnum`** |
| repair | **DELETED** (not de-virtualized) from all three classes |
| NEW, not in the brief | **slots 3/4 were SWAPPED** — `IsSuccess`/`PurchaseMade` |

The brief was right. The bonus finding is that the *order* was also wrong, in a
way the sweep reported as `UNRESOLVED` and could never have caught.

## 1. The COUNT — settled by retail's own `.rdata` geometry, with no name

`tools/retail_rtti.py` locates the table from the `??_R4` COL, i.e. from
retail's own RTTI, not from a map. Instrument validated before use: **8/8
controls pass and BOTH sabotage legs (`--sabotage naive-va`, `--sabotage
overscan`) exit 1.** A negative from it therefore means something.

```
0x8211523c -> .?AVStorePurchaser@@
  [0] 0x827b27b8 <.text>     destructor
  [1] 0x828299b8 <.text>  \
  [2] 0x828299b8 <.text>   |
  [3] 0x828299b8 <.text>   |  five _purecall
  [4] 0x828299b8 <.text>   |
  [5] 0x828299b8 <.text>  /
  [6] 0x821ec094 <.rdata>    STOP  <- XboxPurchaser's OWN ??_R4 COL
```

Three independent reasons this is six and not seven:

1. **Slot 6 is not code at all.** It is `0x821ec094`, in `.rdata`, and
   `retail_rtti.py class XboxPurchaser` independently reports that exact VA as
   XboxPurchaser's COL. The next class's table butts directly against this one,
   so there is no room for a seventh slot.
2. **`0x82115258 − 0x8211523c = 0x1c` = 24 B (6 slots) + the 4-byte COL
   pointer** that precedes every MSVC vtable. The arithmetic agrees.
3. **`NeedsEnum` has a BODY** (`return true` / `return false`), so it can never
   be one of the five `_purecall` slots — and there is no non-`purecall` slot
   for it to occupy.

★ `0x828299b8` was verified to be `_purecall` **independently of the brief**:
**848 references from `.rdata`, 0 from `.data`** — the signature of a function
reachable only through vtable slots — and its body dispatches through a global
handler pointer at `0x82E07EDC`. (§17a of the companion doc independently cites
849 refs for the same VA.)

⇒ **The base's own table is bounded at 6, and `NeedsEnum` would necessarily be a
BASE slot. So the count is settled without scanning for derived classes at
all.**

Our side, from the compiled COFF (`scripts/dump_vtable.py`, indices corrected
for its off-by-one — it numbers the COL as index 0):

| slot | `??_7StorePurchaser@@6B@` (ours) | retail |
|---|---|---|
| 0 | `??_EStorePurchaser@@UAAPAXI@Z` | dtor |
| 1–5 | `_purecall` ×5 | `_purecall` ×5 |
| 6 | **`?NeedsEnum@StorePurchaser@@UBA_NXZ`** | **— (does not exist)** |

The first six slots correspond exactly. The sole difference is the trailing
`NeedsEnum`. Same picture for `??_7XboxPurchaser@@6BStorePurchaser@@@` and
`??_7XboxMultipleItemsPurchaser@@6BStorePurchaser@@@` (both ours = 7).

### 1a. Deleted, not de-virtualized

`NeedsEnum` has **zero call sites** in `src/`, `native/`, **and the sibling
`milo-native-engine`** (the native targets link a superset of `src/`, so the
sibling had to be checked — a `src/`-only grep would have been a false
clearance). A non-virtual survivor would be dead code, so it was deleted.

⚠ It had to come out of **all three** classes: with the base's copy gone, a
surviving derived copy becomes a **NEW** virtual and re-adds the slot.

Corroboration, non-vacuous: the retail binary contains **0** `NeedsEnum`
literals, while the same reader finds `StorePurchaser`=1 and `XboxPurchaser`=1
(their RTTI type descriptors) — so the reader is not blind. On its own this is
weak (retail keeps RTTI names, not method names); the vtable geometry is the
evidence.

## 2. The ORDER — slots 3 and 4 were swapped

The sweep returns `UNRESOLVED` on both tables (5 of 6 `StorePurchaser` slots are
`folded_across`, 4 of `XboxPurchaser`'s are `unnamed`), so this had to come off
bodies. `XboxPurchaser`'s table @ `0x82115258` carries the evidence; the base's
own five slots are all `_purecall` and carry none.

| slot | retail VA | body evidence | identity |
|---|---|---|---|
| 0 | `0x827b29c0` | tail-calls `0x827b28a0`, which does `lis/addi` → `0x82115258` and `stw r10,0(r3)` — installs this very table | dtor |
| 1 | `0x827b2928` | `memset(0x82E0684C, 0, 0x1c)` (sizeof `XOVERLAPPED`), `mState=1`, then `XShowMarketplaceDownloadItemsUI(...)` and tests `0x3E5` | **Initiate** |
| 2 | `0x827b2828` | reads `mState@0xc`, returns true iff ∉ {0,2,3} | **IsPurchasing** |
| 3 | `0x827b2858` | `bctrl` through its own slot 2, then `mState-2 / cntlzw / rlwinm 27,31,31` | **IsSuccess** |
| 4 | `0x827ca3a8` | `lbz r3,0x1c(r3); blr` | **PurchaseMade** |
| 5 | `0x827b2c30` | static-`Symbol` guard, `mState==1`, `XGetOverlappedResult`, tests `0x3E4`/`0x4C7` | **Poll** |

Two things make slot 3 unambiguous:

- `mState - 2 → cntlzw → bit-extract` is the MSVC idiom for `mState == 2`, and
  `kPurchaseSuccess == 2`. Preceded by a virtual call to `IsPurchasing`, that is
  character-for-character our `IsSuccess`:
  `MILO_ASSERT(!IsPurchasing(), …); return mState == kPurchaseSuccess;`
- `PurchaseMade` is `return false` and **cannot** produce it.

★ **Slots 4 and 5 close on each other.** `Poll` writes `stb r11, 0x1c(r30)` on
**all four** of its exit paths, and slot 4 is `lbz r3, 0x1c(r3); blr` — slot 4
returns exactly the byte slot 5 computes. That is a `Poll`-sets-flag /
`PurchaseMade`-reads-flag pair, and it is name-free.

⇒ **Retail: `dtor, Initiate, IsPurchasing, IsSuccess, PurchaseMade, Poll`.**
Ours had `PurchaseMade` and `IsSuccess` transposed.

★ Note the shape of the original error: **dc3's order with `NeedsEnum` simply
DELETED is exactly retail's order.** The prior lane instead moved `NeedsEnum` to
the tail *and* transposed 3/4 — getting `Poll`'s slot right by a second error.

### 2a. ⛔ The refuted instrument, and why it looked convincing

The header comment being replaced read:

> Retail vtable order (from StorePanel::Poll's inlined slot loads):
> 0x8 IsPurchasing, 0xc PurchaseMade, 0x10 IsSuccess, 0x14 Poll.

The `0x8`/`0x14` halves are right; the `0xc`/`0x10` assignment is not. **The
call-site instrument is AMBIGUOUS here in a way the sweep's docstring does not
warn about**: it tells you which *position* is called first, and you can only
convert that into a *name* if you already know the caller's source order — and
**our own callers disagree**:

| caller | our source order |
|---|---|
| `StorePanel.cpp:202` | `PurchaseMade() && IsSuccess()` |
| `TokenRedemptionPanel.cpp:85` | `IsSuccess()` then `PurchaseMade()` |
| `UGCPurchasePanel.cpp:113` | `PurchaseMade()` then `IsSuccess()` |

Read `StorePanel` and you conclude `0xc = PurchaseMade`. Read
`TokenRedemptionPanel` and you conclude the opposite. **Bodies have no such
degree of freedom** — which is why they, not the call site, settled this.

★ Independent semantic corroboration, the §17b "plausibility" discriminator:
our `PurchaseMade()` asserts `MILO_ASSERT(mState == kPurchaseSuccess)`. That
assert is only sane if `PurchaseMade` runs **after** `IsSuccess()` has
short-circuited. Under the old arrangement it fires on every unsuccessful
purchase.

### 2b. ⛔ Compensating errors — why the order fix had to be ATOMIC

This is §17b's `Cache`/`CacheXbox` disease, second sighting. A **whole-`.text`
scan for the purchaser dispatch idiom finds 5 sites, and every one emits the
same order**:

```
0x825bdde4  0x14, 0x8, 0xc, 0x10, 0xc
0x825ed150  0x14, 0x8, 0xc, 0x10, 0xc
0x8263efe8  0x14, 0x8, 0xc, 0x10      (UGCPurchasePanel::Poll)
0x826416ac  0x14, 0x8, 0xc, 0x10
0x827b642c  0x14, 0x8, 0xc, 0x10      (StorePanel::Poll)
```

**Unanimous: retail always dispatches slot 3 before slot 4.** Therefore:

- `StorePanel` / `UGCPurchasePanel` spell `PurchaseMade`-first, which under the
  OLD declaration order emitted `0xc, 0x10` — **correct code, wrong names.**
  Swapping the declaration ALONE would have flipped them to `0x10, 0xc` and
  regressed. So the operands were swapped **in the same change**, leaving the
  emitted offsets byte-identical.
- `TokenRedemptionPanel` spells `IsSuccess`-first, so under the OLD order it
  emitted `0x10, 0xc` — **the one genuinely wrong call site**, and the swap
  fixes it with no source edit.

⇒ The landed change is: **base declaration swap + operand swap at the two
`PurchaseMade`-first callers + `TokenRedemptionPanel` untouched.** Net emitted
change is confined to `TokenRedemptionPanel`.

## 3. What was changed

- `src/system/meta/StorePurchaser.h` — `NeedsEnum` deleted from all three
  classes; base order → `IsPurchasing, IsSuccess, PurchaseMade, Poll`; the
  refuted comment replaced with the retail-byte evidence.
- `src/system/meta/StorePanel.cpp` — `IsSuccess() && PurchaseMade()`.
- `src/band3/meta_band/UGCPurchasePanel.cpp` — `if (IsSuccess()) { … unk4c =
  PurchaseMade(); }`.

## 4. A/B — whole binary, `tools/ab_measure.py --from-dirty`

Two runs, one per commit, so attribution is clean. Both are `kinds=['source']`
and **leg B did 17 real MSVC recompiles in each**, so neither is an
absent-vs-absent reading. Report ruler is the shipped `name_check`; `none` is
the opt-in control.

| | commit 1 — delete `NeedsEnum` | commit 2 — swap slots 3/4 |
|---|---|---|
| `Δmatched_functions` | **+0** | **+0** |
| `Δmatched_code_percent` (name_check) | **+0.000000 pp** | **+0.000000 pp** |
| `Δmatched_code` bytes | **+0 B** | **+0 B** |
| `Δfuzzy` | **+0.000000 pp** (48.912700 → 48.912700) | **+0.000000 pp** |
| `Δhonest` | +0 | +0 |
| `none` ruler control | matched 44485 / 43.159935 both legs, **+0** | **+0** |
| units at 100% [mpn] | 150 → 150, **0 fell off** | 150 → 150, **0 fell off** |
| units at 100% [all-rows-fuzzy] | 122 → 122, **0 fell off** | 122 → 122, **0 fell off** |
| leg B recompiles | 17 | 17 |

Absolute baseline for both: `matched=42252 masked_equal=22912 honest=19340
code%=36.807613`.

**Δ0 was the PREDICTION, not a disappointment.** A vtable lives in `.rdata`, and
`total_code` is exactly Σ(listed function sizes) — so slot contents and slot
order are not in the denominator at all. The three `NeedsEnum` bodies were
unpaired (retail has no such function). And by construction the order swap
leaves `StorePanel`/`UGCPurchasePanel` emitting byte-identical dispatch
offsets. **The signal that matters here is `0 units fell off on either ruler`**,
which held for both commits.

⚠ For commit 2 the prediction admitted a possible small POSITIVE, from
`TokenRedemptionPanel` (the one site whose emitted offsets genuinely change,
`0x10,0xc` → `0xc,0x10`). It measured `+0`, which corroborates lead #8 below:
that row is not scored, because none of the five retail dispatch sites falls
inside `TokenRedemptionPanel.cpp`'s pinned span.

## 5. Deliberately NOT done — leads, with the evidence that opened them

Each is a real divergence found in passing; none is a vtable-count/order issue,
and each needs its own lane.

1. ⛔ **Retail's `XboxPurchaser` is NOT an `Hmx::Object`.** Three independent
   readings: its CHD has **`numBaseClasses=2`** listing only
   `{XboxPurchaser, StorePurchaser}`; there is exactly **one** COL for it (a
   second base sub-object would need its own vtable + COL); and `mState` sits at
   **`0xc`**, precisely where our `Hmx::Object` sub-object vptr lives
   (`dump_vtable` reports `sub_object_offset 12`). Non-vacuous:
   `.?AVObject@Hmx@@` **is** present in the binary, so the reader can see it.
   This is coherent with the bodies — retail's `Initiate` never calls `AddSink`
   and retail's `Poll` polls `XGetOverlappedResult` directly, i.e. **a polling
   design with no message sink**, which needs no `Hmx::Object`. Ours is DC3's
   sink-based design. **Removing the base is a whole-class port.**
2. **`XboxMultipleItemsPurchaser` appears absent from retail** — no COL, and 0
   name literals (while `StorePurchaser`/`XboxPurchaser` each have 1). It is
   also never constructed in *our* tree. Likely a DC3-era split of the single
   RB3 class: retail's one `XboxPurchaser` calls
   `XShowMarketplaceDownloadItemsUI` with **count = 1** and `&this->0x10`,
   i.e. the "multiple items" API used for a single offer.
3. **Our `XboxPurchaser::Poll()` is an empty stub; retail's is a real body**
   (`XGetOverlappedResult`, `0x3E4` = `ERROR_IO_INCOMPLETE`, `0x4C7` =
   `ERROR_CANCELLED`, four `stb …,0x1c`). ~140+ bytes unwritten.
4. **Our `PurchaseMade()` returns `false`; retail returns the byte at `0x1c`**
   that `Poll` writes.
5. **Our `Initiate()` calls `XShowMarketplaceUI`; retail calls
   `XShowMarketplaceDownloadItemsUI`** and tests `0x3E5`
   (`ERROR_IO_PENDING`).
6. **Implied retail layout** (from the bodies, for whoever takes #1):
   `vptr 0`, `mSource 4`, `mUserIndex 8`, `mState 0xc`, `mOfferID 0x10` (u64),
   own `mUserIndex 0x18`, **`bool` 0x1c**, `mSelectedCount 0x20`. Our header's
   `// 0x38` comments do not describe retail.
7. ⛔ **MIS-PIN.** The purchaser bodies are pinned to **`MeshAnim.cpp`**:
   `0x827b2770–0x827b29c0` and `0x827b2a0c–0x827b2da8` swallow `IsPurchasing`,
   `IsSuccess` and `Poll`, while `StorePurchaser.cpp`'s own
   `0x827B29C0–0x827B2A0C` pin is carved out of the gap between them and covers
   only the deleting destructor. Re-homing is **not** metric-neutral
   (CLAUDE.md), so this needs its own measured lane.
8. **`TokenRedemptionPanel.cpp`'s pinned spans contain none of the 5 purchaser
   dispatch sites**, so retail's `TokenRedemptionPanel::Poll` purchaser code is
   not where the pin says. Possibly related to #7.
9. **Retail `XboxPurchaser::IsPurchasing` is spelled as the three-way
   `!= 0 && != 2 && != 3`** (our `XboxMultipleItemsPurchaser`'s form), not our
   `XboxPurchaser`'s `== purchasestate1`.

## 6. Instrument notes for the next lane

- `tools/vtable_order_sweep.py`'s verdict column describes **ORDER, not
  LENGTH** — `UNRESOLVED` coexists with a count mismatch and says nothing about
  it. Both tables here were `UNRESOLVED` while being decisively +1.
- `scripts/dump_vtable.py` takes a class name and returns the **first** matching
  `??_7`; for a multiple-inheritance class that is not necessarily the one you
  want (it returned the 22-entry `Hmx::Object` sub-object table for
  `XboxPurchaser`). Use `enumerate_all_vtables()` and select on `base_name`.
  Its printed indices are **off by one** (it numbers the COL as index 0) and its
  "Annotation" column is generic `Hmx::Object` boilerplate — ignore it.
- ⚠ `StorePurchaser` slot 0 reads as `??_GPlayerGameplayMsg@@UAAPAXI@Z` in the
  sweep's withheld list. That is ICF (deleting dtors fold heavily) and is
  evidence of nothing. The sweep correctly withholds it.
