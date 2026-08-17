# W17-FAMILYSWEEP — the `_Rb_tree` map-defect class, swept rather than spotted

**2026-08-17.** Baseline reproduced **exactly** as briefed, on the shipped
`name_check` ruler: **44,485 fns / 3,747,732 B / 36.312897%**, honest 21,585,
`total_code` 10,320,664, `total_functions` 69,226.

**Shipped: +1,652 B / +3 fns / +3 honest / +0.016006 pp**, units at 100%
254 → 255, `masked_equal` unchanged at 22,900. `none` control **+792 B**
(predicted +792, exact). Commit `7e9c2d01`.

Four lanes (W2, W12, W14, W15) each found one of these by hand. This lane built
the instrument, ran it over the whole class, and shipped four families in one
measured change.

---

## The instrument, and why it is not `node_size_screen.py` with a wider net

`node_size_screen.py` sizes the *mangled name* with a hand-written parser and
compares it to the builder's `li r3,N`. Its own docstring records the two limits
that make a clean run **not** a clearance: it screens only `_M_insert`, and its
map rule fires only for `value_type < 8`.

`tools/rbtree_family_sweep.py` changes the comparison instead of widening it.
For every mapped tree-member row it compares **retail's node function against
OUR OWN COMPILED `_M_create_node` for the tree the map declares** —
`li r3,N` *plus* the `(mnemonic, displacement)` sequence of its accesses.
Registers are excluded on purpose: they are the part regalloc may permute, while
width and offset are the part that encodes the `value_type`.

That single test subsumes both prior ones. Size settles the cases where
different-size COMDATs cannot fold (so a disagreement is a **wrong name**, never
an arbitrary ICF survivor); **shape** settles the cases size cannot, which W12
needed because `set<G>` and `map<G,G>` both allocate `0x14`. And because the
reference is a *compiled body* rather than a parsed name, it reaches class-typed
value_types where `pair_size()` returns `None` — which is exactly where the two
biggest finds of this lane were hiding.

### Four things that made the first version lie, all found by running it

1. ⛔ **ICF folds builders across trees, so grouping families BY BUILDER
   over-merges.** `0x8235c328` is physically reached by rows declaring
   `set<MoveDetector*>`, `set<ScoreType>` **and** `set<TrackWidget*>` — three
   real trees whose builders are byte-identical 4-byte word copies. "Several
   declared trees at one builder" is therefore **expected**. Fixed with a rule
   that calibrates itself off the data: *a callee whose mapped callers declare
   ≥ 2 distinct trees is SHARED and is never merged through.*
2. ⛔ **A `li r3,N` is not always an allocation.** `_M_erase` ends in
   `deallocate(node, sizeof(node))` and opens `li r3,N` too. Conflating it with
   a builder compares a **destructor against our constructor**, and the first
   run duly flagged **18 of 18** families — i.e. said nothing. A builder STORES
   the node base at `+8`/`+12`; an eraser LOADS them.
3. ⛔ **`tree_key()` ran to the end of the mangled name**, sweeping up each
   member's own function signature, so every row of one tree read as a
   *different* tree and the consistency test fired everywhere.
4. ⛔ **Body extents must come from `symbols.txt`.** A default 0x80-byte window
   runs off the end of short functions and attributes the *next* function's `bl`
   edges to this one — fabricated call edges, which is the one error class that
   would corrupt every conclusion downstream.

### The vacuity control

`--selftest` sabotages a healthy family's declared tree twice — once with a real
tree of a **different node size**, once with one of the **same size but a
different value shape** — and requires the flag count to rise. Both legs
**PASS**; the shape leg's clean count is **0**, which is what makes a shape flag
mean anything. A check that only ever passes proves nothing.

---

## ⛔⛔ The builder's NAME cannot anchor its callers — and this lane nearly shipped on it

`0x82456190` (mapped `set<FaderGroup*>::_M_insert`) calls `0x826e0950`, which the
map names `map<TrackType, PerfectOverdriveTracker::PlayerContribData>::_M_create_node`.
An `_M_insert` can only call its own tree's builder, so that looks decisive —
and it was *doubly* inviting, because W14 had just re-homed the **sibling**
tracker's family (`PerfectSectionTracker::PlayerStreakData`) three commits
earlier. Every prior would have accepted it.

**It is wrong.** `0x826e0950` is an **ICF survivor**: any tree with a 4-byte key
and a 12-byte trivially-copyable value folds into it, so its name is arbitrary.
Reading it is precisely W7's fixed-point problem — an anchor that consults the
map under suspicion.

The **caller cascade** broke the tie. All three external users of the family sit
inside `Text.cpp` pins, one of them `?SupportChar@RndText@@`, and `RndText`
declares `std::map<FontKey, MeshInfo> mMeshMap` with `FontKey` a typedef for
`unsigned int`. The family is `RndText::mMeshMap`.

★ **The metric then confirmed the assignment through a row nobody touched:**
`SupportChar@RndText` crossed 99.90566 → 100 (**+212 B**) because its callee
name is finally right. *A wrong assignment would have left it exactly where it
was.* That is the discriminating property the brief promised, observed working.

⇒ **Rule: a builder that is an ICF survivor is evidence of node SIZE and node
SHAPE only, never of tree IDENTITY.** Use it to refute; never to assign.

---

## What shipped

| | address(es) | from | to | measured |
|---|---|---|---|---|
| **A** | `0x82597098` | `clear@map<Symbol,int>` | `clear@map<Symbol,SetlistRecord>` | **+80** |
| **B** | `0x824f9288` | `_M_copy@set<Symbol>` | `_M_copy@map<String,DataNode>` | **+440** |
| **C** | `0x82456190` `0x824563d8` `0x824566e8` | `set<FaderGroup*>` ×2 + `map<Symbol,Award*>` | `map<unsigned int, RndText::MeshInfo>` | **+684** |
| **D** | `0x82472df0` `0x824730e8` `0x824731c0` `0x824732b8` `0x82473630` `0x824742e8` | `map<G,RndFont3d::CharInfo*>` ×4 + `map<CharClip*,float>` + 1 anon | `map<unsigned short, RndFont::CharInfo>` **by value** | **+448** |

**A** — in-place rename; `SongSortMgr.obj` already defines the replacement, so
hard limit #1 is satisfied without moving anything. Anchors: the `_M_erase` it
calls deallocates `0x54` (⇒ `SetlistRecord` 64 B, vs 8 B for the declared pair);
the class declares `mSongs`/`mSetlists` and **no `map<Symbol,int>` at all**; and
its two callers are `~SongSortMgr` and `BuildSetlistList@SongSortMgr` — a
semantic anchor from outside the tree family entirely.

**B** — `set<Symbol>` needs a `0x14` node; this calls a `0x24` builder that also
calls `_Copy_Construct@pair<const String,DataNode>`. Its one external caller is
the `_Rb_tree<String, pair<const String,DataNode>>` **copy-constructor**.
Re-homed `AccomplishmentDiscSongConditional.cpp → RockCentral.cpp` — an island
sandwiched between two RockCentral blocks — **because RockCentral.obj defines
the name and ADSC.obj does not.** A rename in place would have sent a
`mpn == 100` row permanently to 0%.

**C** — retail member sizes **204 / 224 / 472** match our `Text.obj` bodies
exactly, and the builder is `0x20` with our identical copy shape. This also
**settles W12's third row**, which it left as *"three different sizes cannot
describe one function, and nothing in reach picks which"*: the call graph shows
`_M_insert` has exactly two callers — its own two `insert_unique` overloads — so
all three are **one** tree and the two competing names cannot both be right.

**D** — the builder copies a **halfword key at +0** then **four words at
+4..+16**: a 16-byte `value_type`, which a pointer cannot be. All six retail
extents match our `Font.obj` bodies one-for-one (**112 / 204 / 200 / 224 / 472 /
208**) and all six are islands inside an otherwise contiguous `Font.cpp` region,
which the move closes up.

`0x82472df0` was **anonymous and is now named**. Per CLAUDE.md that is normally a
bet with no byte upside, since a placeholder callee is already forgiven. It pays
here for the *other* reason — it makes a 112 B row **pairable** (+112) — and it
is safe because **all five of its retail call sites are the two family members
renamed in the same commit**, so no forgiven site becomes a wrong one.

---

## Predicted vs measured — and the third repetition of the same miss

| | predicted | measured | |
|---|---|---|---|
| A | +80 | **+80** | ✓ exact |
| B | +200 | **+440** | ✗ +240 |
| C | +472 | **+684** | ✗ +212 |
| D | +320 | **+448** | ✗ +128 |
| **graded total** | **+1,072** | **+1,652** | ✗ **+580** |
| **`none` control** | **+792** | **+792** | ✓ exact |
| Δfns | +3 | **+3** | ✓ exact |

★ **The entire +580 miss is the CALLER CASCADE — 35% of the yield, from four
rows that are not in the patch**: the `_Rb_tree<String,DataNode>` ctor +240,
`SupportChar@RndText` +212, `CharAdvance@RndFont` +76, `CharWidth@RndFont` +52.
W9 predicted ~0 and measured +268; W8-TWINPORT predicted +24 and measured +184.
**This is the third time**, so it should stop being reported as a surprise: a
wrong map name is financed by a charge on every caller that relocates against
it, and the repair collects that back. Price it in.

⚠ **And the 1,100 B of "live credit" on family D did not HOLD as predicted — it
MOVED.** 900 B fell off `Font3d` and 200 B off `HamCharacter`; the identical
bytes re-crossed inside `Font`. Same net, wrong mechanism — visible **only** in
the row-level diff, which is why `tools/rbtree_attribute.py` exists. Its check
is that the row-level net equals the headline: **+1,652 == +1,652, AGREE**, so
every byte is attributed and the decomposition above is scored, not asserted.

The `none`/graded split is the honest reading: **792 B is real code newly
PAIRING** on both rulers (the re-homes), and the other **860 B is name-only
crossing** visible solely to `name_check`. Alias control `NOT_APPLICABLE`, as for
every splits-carrying patch. **No alias was added.**

---

## Corrections to the in-tree record

* ✅ **W12's `RndFont::CharInfo` hand-back is CONFIRMED — and its `src/**`
  premise was WRONG.** W12 inferred `CharInfo` = 16 B and handed a header fix to
  lane W11b on the basis that `Font.h` made it 20 B. Our compiled
  `map<G,RndFont::CharInfo>::_M_create_node` **already allocates `0x24`**, so
  `CharInfo` is already 16 B and **no `src/**` change was needed at all**. The
  header comment was read; the compiler was not. (CLAUDE.md's own rule: ask the
  compiler, not the comments.)
* ⛔ **W15's caller claim for `0x8233c668` is WRONG.** It states the squatter's
  *"sole caller is `map<int,int>::operator[]`"*. Measured: `0x8233c668` has
  exactly one caller, `0x8233cbdc`, which lies inside the **anonymous**
  `fn_8233CB58`; and `map<int,int>::operator[]` (`0x8233cab8`) has exactly one
  `bl`, to **`0x827c68c0`** — a *different* 488-byte function with the same call
  shape. The two are **structural twins that did NOT fold** (their callees
  differ). W15's conclusion (the row is byte-valued, evict it) is unaffected;
  its stated call graph is not.
* ✅ **W12's Lead-1 third row (`0x82456190`) is SETTLED** — see family C.
* ⚠ **`FOREIGN_CALLEE` is a low-precision flag** (19 components) and should not
  be read as a defect signal: most hits are healthy families whose members call
  a comparator or a value-type constructor that the STL-name filter does not
  recognise. It is kept because it is how `0x826f0f98` surfaced, but it needs
  manual adjudication every time.

---

## Class coverage — and what this sweep does NOT test

186 tree-member rows → **86 components**, of which **51 reach a builder or an
eraser and are therefore testable**. The decisive class is
**`SIZE_DISAGREES`: 8 components, of which 7 are real** and 1
(`0x825990e0`, `map<Symbol,SongRecord>`) is a **BFS artifact** — its own eraser
allocates `0x120` and agrees exactly; the disagreeing `0x18` eraser is a
different, ICF-shared one reached from its sibling row.

**All 7 real ones are adjudicated: 6 shipped, 1 refused.**

⛔ **35 components reach no builder or eraser and are UNTESTED BY CONSTRUCTION**
(`_M_find`, `swap`, `begin`, `_M_lower_bound`, bare `_M_erase`). A clean sweep is
a clearance for the 51, never for the 86. That is the honest coverage statement
and it is the same shape as the limits `node_size_screen.py` records about
itself.

---

## Refused, with reasons — and one open lead now PROVED

* **`0x822dea78` `clear@map<Symbol,CharLipSync*>` → `clear@set<Symbol>`.
  PROVEN WRONG, DELIBERATELY NOT SHIPPED.** It calls `_M_erase@set<Symbol>`
  (`0x822dd9a0`), which deallocates `0x14`; the declared map needs `0x18`, and
  different-size COMDATs cannot fold. Corroborated from outside the map by
  **62 call sites** spread across the whole binary — a generic `set<Symbol>`,
  not a CharLipSync-specific member. **`CharLipSync.obj` does not define
  `clear@set<Symbol>`**, so a rename in place sends a `mpn == 100` row
  permanently to 0% (W9's −180 B failure mode). A re-home is possible in
  principle but the name is a template COMDAT defined in 10+ objs, so *which TU
  the linker's survivor came from is not adjudicable* — moving the pin would be
  a guess wearing the costume of a fix.
* **`0x826f0f98`, mapped `map<FlowNode*,QueueState>::_M_insert`, is not a tree
  member at all.** It is 68 bytes, has **zero** callers, and its body calls
  `?GetMapKey@RGTutor@@` and then `map<int,int>::operator[]`. An `_M_insert`
  does neither. Existence of the defect is proven; the true name is not, and it
  needs an RGTutor lane.
* ★ **`map<int,bool>` chain — EXISTENCE *AND* ASSIGNMENT NOW PROVED, blocked only
  on a carve.** The brief handed this over as *"existence is proven; the
  assignment is not."* The assignment is now proven:

  | | retail | ours (`BandList.obj` / `HamNavList.obj`) |
  |---|---|---|
  | builder `0x8233bea0` | 84 B, `li r3,0x18`, `[lwz+0, stw+0, **lbz+4, stb+4**, stw+8, stw+12]` | 84 B, `li r3,0x18`, **identical shape** |
  | `_M_insert` `0x8233c2c8` | 204 B | 204 B |
  | `insert_unique` `0x8233c668` | 488 B | 488 B |

  The builder is currently mapped `map<int,**Symbol**>::_M_create_node`, which
  the shape **refutes**: `lbz`/`stb` is a one-byte value and `Symbol` is four.
  Geometry picks the home — the region `0x8233ba00–0x8233d558` is dominated by
  **`BandList.cpp`**, with `Song.cpp` / `CharSignalApplier.cpp` / `Rot.cpp`
  appearing as islands at exactly the chain's addresses.
  **Not shipped** because all three members sit *inside* larger foreign blocks,
  so it needs a three-way carve, and each carve claims TU ownership of
  neighbouring bytes this lane has not identified. That is a contained, fully
  specified next lane.

## Deliberately not done

* **No `src/**` was touched**, so no native gate run was required.
* **No alias was added or withdrawn.** The alias system is not this lane's to
  edit, and adding forgiveness lifts the score by construction.
* **The CharInfo builder was the only anonymous address named.** Naming others
  was not attempted: it is a bet that pays in bug exposure rather than bytes
  unless it creates a pairing, and only this one did.
* **The 35 `NO_NODE_FN` components were not adjudicated** — the instrument has
  no test for them, and inventing one would have been unmeasured.
