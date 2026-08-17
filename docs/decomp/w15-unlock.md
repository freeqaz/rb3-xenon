# W15-UNLOCK — the blocked family, unblocked: a PHANTOM row, not a missing identification

**2026-08-17.** Baseline reproduced exactly as briefed:
**44,486 fns / 3,746,668 B / 36.302586%**, honest 21,586, on `name_check`.

**Shipped: +1,156 B / Δfns 0 / Δhonest 0**, in two measured parts
(`02212462` −220 B, `0b1109a7` +1,376 B, `736f8616` `.pdata` convergence).
Final **3,747,824 B / 36.313786%**.

---

## The blocker was not an identification problem. It was a carving artifact.

W14 refused this family because freeing `map<int,float>::_M_copy` required
identifying `0x8235c610` — "96 B, `fuzzy` 0 / `mpn` 3.54, **zero callers**,
refuted by size but *what it actually is cannot be proven*."

**`0x8235c610` is not a function.** It is the interior (+8) of the 104-byte leaf
at `0x8235c608`, which the map *already named correctly*:
`_Rb_tree_base<pair<const int,float>,…>::_Rb_tree_base(__move_source<_Self>)`.
dtk had carved one function into an 8 B head row and a 96 B tail row, and the
tail got a name. Every one of W14's observations is a symptom of that: nothing
can call mid-function (zero callers), and a mid-function slice scores ~0.

The body is self-describing and reads no map — four word copies of the whole
`_Rb_tree_node_base` (color/parent/left/right), then `_M_rebind`'s three
conditionals **in source order (parent, right, left)**, then
`src._M_empty_initialize()` (`color=red(0)`, `parent=0`, `left=right=self`).
It matches `stl/_tree.h:242-262` instruction for instruction.

Extent is bracketed independently of any map: alignment padding `00000000` at
`0x8235c604`, and the **8-byte EH prefix** at `0x8235c670` (a `.text` + an
`.rdata` pointer; `+8` = `0x8235c678`, a real `.pdata` start beginning
`mflr r12`). Corroborated twice more, by artifacts that knew nothing of this
lane: `symbol_aliases.json` group 386 already recorded *"different body SIZE
(8 vs 104)"*, and the re-split converges `symbols.txt` to
`fn_8235C608 … size:0x68` (=104) while labelling `0x8235C670`
`except_data … size:0x8`.

⚠ **`.pdata` CANNOT adjudicate this address and was not used as if it could.**
Neither `0x8235c608` nor `0x8235c610` is a BeginAddress — an 8-byte leaf
touching neither stack nor LR gets no unwind record (AUDIT-NC). The `False` is
uninformative, not negative evidence.

⇒ **The row was DELETED, not renamed. No name was guessed** — W14's refusal was
correct *and* its blocker dissolved once the row was read as geometry rather
than as an unknown function. **Ask what a 0%-scoring, zero-caller row IS before
asking what it should be called.**

## The family, and why it is indivisible — mapped, not asserted

```
operator[] (0x82272140, CORRECT)  -> insert_unique[HINT] -> _M_insert -> _M_create_node
                                        \-> insert_unique[NOHINT] -/
VocalTrackDir::Copy (CORRECT)     -> operator=  -> _M_copy ---------> _M_create_node
```

`_M_create_node` is the **articulation point** feeding both halves, so there is
no separable sub-component and a partial rename leaks (W12's −696 B trap).
Six rows renamed `map<CRC,float>` → `map<int,float>`; **all six held at
`fuzzy` 100** (1,408 B preserved).

## The second squatter: proven wrong, deliberately NOT renamed

`0x8233c668` held `map<int,float>::insert_unique[HINT]`. `insert_unique` is
value-type-agnostic in its own body, so it can only be adjudicated through its
chain — `0x8233c668 → 0x8233c2c8 → 0x8233bea0` — and that builder does
`li r3,0x18` then copies the value with **`lbz`/`stb`**: a **one-byte** value.
That refutes `float` (`lfs`/`stfs`), and equally `int`/`Symbol` (`lwz`/`stw`).

`Rot.obj` *does* define `map<int,int>::insert_unique[HINT]`, which would have
preserved the row's 488 B. **Refused**: the same instrument refutes it, and
swapping a proven-wrong name for a known-wrong one to hold credit is
metric-fitting. Evicted + **denylisted** (the body is reloc-normalised
shape-identical to ours, so an autoid pass would re-insert and oscillate).
Deliberately **−488 B**, accuracy over headline — cf. MAPID-1.
The only 1-byte-valued int-keyed tree in the whole build is `map<int,bool>`,
emitted solely by `BandList.obj`/`HamNavList.obj`; **re-adjudicate by re-homing
that chain, not by renaming in place.**

## Part B — the shortfall diagnosed itself, and it was LEAD 1

Part A moved `VocalTrackDir::Copy` 99.959015 → **99.97951** — *exactly half the
remaining gap*, i.e. **two** charged sites, one closed. The other was
`bl 0x822fc180`, named `map<u16,RndFont::CharInfo>::operator=`.

The anchor needs no map: **VocalTrackDir has exactly two maps**
(`mLyricColorMap<int,Hmx::Color>`, `mLyricAlphaMap<int,float>`) and retail
`Copy` makes exactly **two** tree `operator=` calls, the other being the
int,float one. Confirmed by call-graph closure and by copy shape —
`0x822f8f80` does `li r3,0x24` then **five uniform `lwz`/`stw` at
+0,+4,+8,+0xc,+0x10** = a 20 B memberwise copy from offset 0 =
`pair<const int,Color>` (4+16). `pair<const u16,CharInfo>` has the **same 0x24
node size** but would copy the key with `lhz`/`sth` and start the value at +4.
Plus **completeness** (the `<int,Color>` tree was missing exactly `_M_copy` and
`operator=`; these two rows are exactly those) and **geometry** (both are 200 B
islands filling exact holes in VocalTrackDir's contiguous region).

★ **The obj test forced a RE-HOME rather than a rename**, and this is the whole
safety story: `Font.obj` defines the `<u16,CharInfo>` spellings and **not** the
`<int,Color>` ones; `VocalTrackDir.obj` is the reverse. Renaming in place would
have sent both rows permanently to 0% (W9's −180 B failure mode). Both rows were
at `fuzzy` 99.8/99.9 ⇒ **earning zero bytes**, so the move traded nothing away.

⛔ **Correction to the LEAD-1 brief:** *"`_M_create_node` ↔ `fn_822F8F80` likely
owed to RndFont"* is **REFUTED on retail bytes** — the five-word copy from offset
0 is the int,Color builder and a `u16` key cannot produce it. LEAD 1's
`_M_copy`/`operator=` half is correct and is what landed.

## Predicted vs measured

| | predicted | measured |
|---|---|---|
| part A | +756 (or +652) | **−220** ✗ |
| part A `none` | −384 | **−384** ✓ exact |
| part B | +1,376 | **+1,376** ✓ exact |
| part B `none` | ~0 | **+0** ✓ |
| part B Δfns | 0 | **0** ✓ |

Part A missed **only** because `Copy`'s 976 B needed *both* its sites; every
other component landed exactly (`0x8235c608` (8 B, 0) → (104 B, 100); six rows
held; `operator[]` +164; eviction −488). ★ **The `none` control is what made the
miss diagnosable**: graded −220 with `none` −384 decomposes to +164 of name-only
crossing, which is `operator[]` alone and therefore says `Copy` did not cross —
before ever opening the row.

## Instrument notes

* ★ **The applier's collision guard fired twice in development** — once on the
  not-yet-freed `_M_copy`, once asserting *global* injectivity where the map has
  **2 pre-existing duplicates** (`?NodeCmp@@YAHPBX0@Z` and the denylisted
  `LevelData` pair). So the correct post-check is a **delta** against the
  pre-edit collision set, never purity. A guard demonstrated to fail is worth
  more than one that only ever passes.
* ⚠ `report.json`'s `matched_code` counts `fuzzy == 100` only, so a row at
  99.8 earns **zero**. That is what made the re-home riskless, and it is not
  visible from `mpn` (both rows were `mpn` 100).

## Figures from the brief that were wrong

**None on the primary.** The +1,140 B pricing, both squatted names, the
`0x8235c610` description (96 B / `fuzzy` 0 / `mpn` 3.54 / zero callers), the
alias-forgiveness warning and the indivisibility all held exactly. The
coordinator's LEAD-1 `_M_create_node` claim is the one corrected item.
