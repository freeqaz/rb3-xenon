# `_Rb_tree<Symbol, V>` — which `V`? Adjudicated on retail bytes (lane CONTAINER-1, 2026-08-14)

**Verdict: the tree at `0x824f8968` holds `DataNode`. Our source was RIGHT and
the MAP NAME was wrong.** Not an ICF fold, not a wrong container — a wrong
*template argument* in `scripts/target_symbol_map.json`, i.e. the GAMEROW-1
defect class (`3a1af7e3`) in a new dress.

Measured: **+7,672 B / 34.908997 → 34.983330 (+0.074333 pp), −2 matched
functions**, both of which were provably false 100%s.

---

## Why this needed a lane

GAMEROW-1 found `_Rb_tree<Symbol,String>::insert_unique` (target) vs our
`<Symbol,DataNode>` — 267 sites / 35 rows / 34,588 B — and **deliberately
declined it**: it is the classic ICF fold-alias shape, and an unproven alias
lifts the score by construction. The two live hypotheses had *opposite* correct
actions:

* **(A) ICF fold** ⇒ an alias is legitimate *if proven*.
* **(B) wrong container** ⇒ change our source; aliasing would conceal a bug.

The answer was **(C), which neither hypothesis named**: fix the map.

## ★ The discriminator: NODE ALLOCATION SIZE

An `_Rb_tree` node is `_Rb_tree_node_base` (**0x10**) + `pair<const K, V>`.
With `K = Symbol` (4 B), the `li r3, N` in `_M_create_node` reads off `sizeof(V)`
directly. Compiler-authoritative sizes
(`scripts/harvest/class_layout_report.py`, **not** header comments):

| `V` | `sizeof` | node |
|---|---|---|
| `DataNode` | 8 | `0x10 + 4 + 8` = **`0x1c`** |
| `String` | 12 (it is polymorphic — `TextStream` vfptr at +0) | `0x10 + 4 + 12` = **`0x20`** |

Retail `0x824f8968` does **`li r3, 0x1c`**. A `String` tree cannot be 0x1c.

★ **Different-size COMDATs cannot fold**, so this single instruction also kills
hypothesis (A) outright — there is nothing for `<Symbol,String>` to have folded
*into*.

## Four independent witnesses, none of which reads a map name as ground truth

1. **Node size** — `li r3, 0x1c` @ `0x824f8968` (above).
2. **Value copy ctor** — the ctor at `node+0x10` tail-calls `0x8274a9d0`, which
   copies exactly 8 bytes, tests bit `0x10` of word 1, and on set bumps a
   **halfword refcount at `+0xa`** of the pointed-to object. No allocation, no
   vfptr store. That is `DataNode`'s copy ctor; `String`'s must store a vfptr
   (see `??1String@@`, which does) and allocate.
3. **Node free** — `0x824f9050`, which the map **already** calls
   `<Symbol,DataNode>::_M_erase`, frees `li r3, 0x1c` — the exact size
   `0x824f8968` allocates. Same tree.
4. **Payload dtor** — the destructor `_M_erase` invokes on `node+0x10` is
   `0x824f8270` = `??_G?$pair@$$CBVSymbol@@VDataNode@@`, also already map-agreed.

### Internal inconsistency — no fold model required

One contiguous COMDAT run held **three different `T`**:

| addr | map name said | really |
|---|---|---|
| `0x824f8270` | `pair<const Symbol,DataNode>` dtor | DataNode ✅ |
| `0x824f8968` | `_M_create_node<Symbol,**String**>` | DataNode ❌ |
| `0x824f8f50` | `_M_insert<Symbol,**String**>` | DataNode ❌ |
| `0x824f9050` | `_M_erase<Symbol,DataNode>` | DataNode ✅ |
| `0x824f90a8` | `insert_unique<Symbol,**String**>` | DataNode ❌ |
| `0x824f91e0` | `clear<Symbol,DataNode>` | DataNode ✅ |

One instantiation cannot have two value types. The map contradicted itself; the
bytes said which side was right.

## ⚠ The instrument CAN return the other answer — and did, twice

A classifier that only ever says "DataNode" proves nothing. Same instrument,
same accused population, opposite verdict:

* `0x8256a100` / `0x827a8b10` (`_M_erase_after`): slist node **`li r3, 0x14`** =
  4 (next) + **16** payload = `pair<const Symbol,String>`, destroying the value
  at `node+8` via `??1String@@`. **Genuinely String — names left alone.**
* **Tree #2** `0x82743bd0` / `0x82743cb0` / `0x82743d80` / `0x827444f0`:
  **`li r3, 0x20`**, value ctor tail-calls `??0String@@QAA@ABV0@@Z`, and the
  undisputed `??A?$map@VSymbol@@VString@@` (`0x82744e48`) inserts through it.
  **Genuinely String** — it was misnamed `<Symbol,list<String>>`, and is the
  real home our `<Symbol,String>` COMDATs belong to.

Had tree #1 read `0x20` plus a `String` ctor, the correct action would have been
to change our **source**. It read `0x1c`.

## ★★★ A 100% row is NOT evidence the value type is right — proven at the byte level

Before the fix, `BandSongMetadata`'s real `<Symbol,String>` COMDATs were pairing
against tree #1 and reading **`insert_unique` fuzzy 100.0** and **`_M_insert`
fuzzy 100.0**. Those bodies are **T-INDEPENDENT** — they only touch
`_Rb_tree_node_base` pointers and differ solely in the `bl _M_create_node`
relocation — so they score 100 against the *wrong* `T` **by construction**.

The one T-dependent row never reached 100: `_M_create_node` sat at
**99.71 fuzzy**. Byte-comparing our `BandSongMetadata.obj` body against retail
`0x824f8968`:

```
ours    7D8802A6 38000000 90010004 4BFFFFED 3BE1FF80 9421FF80 38600020 7C9D2378 …
retail  7D8802A6 38000000 90010004 483308E9 3BE1FF80 9421FF80 3860001C 7C9D2378 …
                                   ^^ reloc                   ^^^^^^^^ li r3,0x20 vs 0x1c
```

Identical except the two relocated `bl` targets and **one immediate** — which is
exactly `sizeof(String)` vs `sizeof(DataNode)`, *measured*.

⇒ **The two −1s this lane cost are false pairings correctly removed.** Under the
standing `ACCURACY > headline %` directive that is a win, and it arrived with
+7,672 real bytes.

## The closed 3-cycle

`_M_insert` could not be repaired one row at a time — every single-row move
collided on map injectivity, because each of three addresses wore the name
belonging to the next (same shape as the DataFunc `*Eq` trio, `aa87034a`):

```
0x824f8f50  really <Symbol,DataNode>::_M_insert   wore <Symbol,String>
0x82743cb0  really <Symbol,String>::_M_insert     wore <Symbol,list<String>>
0x827d96d8  an XLSPConnection refcount helper     wore <Symbol,DataNode>
```

Rotated all three at once. `0x827d96d8` was set to **`null`** ("deliberately
unclaimed") rather than given an invented name — it is provably not that
function (it sits among `XLSPConnection`/`Pool`/`HeapStats`, decrements a
refcount at `+0x14`, calls `?erase@_Rb_tree<unsigned long,int>@`) but no proven
name exists for it. **Flagged for the map owner.**

## ⚠ The gain is capped by a PIN fact, not a map fact

`0x824f8968` / `0x824f8f50` / `0x824f90a8` are pinned to **`BandSongMetadata.cpp`**,
whose obj cannot define a `<Symbol,DataNode>` COMDAT — so those three sit at 0%
there *regardless of name*. Only `0x8274b638` is pinned to `DataNode.cpp`, which
does define it, and that row is where the second wave's +472 B came from.
**Not moved** — a splits change has its own blast radius.

This is why +7,004 B landed against a 34,588 B nominal: `matched_code` is
all-or-nothing per row, and most of those rows are structurally unpairable where
they sit.

## ⛔ Corrections to the in-tree record — it held BOTH answers

* `docs/decomp/near-miss-classification-2026-06-06.md` row 11 said our type
  should be `DataNode`. **Right**, and it was applied.
* `docs/plans/funclet-cascade-lever-2026-07-25.md` §27.2 said *"`RockCentral::
  UpdateChar`'s target-side `insert_unique` resolves to `pair<Symbol,String>`
  where ours is `pair<Symbol,DataNode>` — evidence retail's `DataPoint` stores
  `map<Symbol,String>`."* **Wrong.** Its stated "evidence" is the map name,
  which is *our own assignment* — retail symbols are stripped. This is precisely
  the reasoning GAMEROW-1 refuted.

Both are dated records and are left as written. Cite **this** file for the
current answer.

## No alias was installed

An unproven ICF alias lifts the score by construction and is an integrity
hazard. This needed a **name repair**, not forgiveness. `none`-ruler control
MOVED on both waves (−224 B, −204 B) and `ab_measure` classified both
**REAL_PAIRING**, not `ALIAS_SUSPECT`.
