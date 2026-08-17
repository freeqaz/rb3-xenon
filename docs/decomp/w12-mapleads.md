# W12-MAPLEADS — assignment, not existence

**2026-08-17.** Three leads were handed to this lane sharing one shape: a prior
lane **proved a map defect exists** and **could not prove which name belongs
where**, and correctly shipped nothing. This lane's job was the assignment.

Baseline (leg A, reproduced exactly): **44,480 fns / 3,744,364 B / 36.280262%**,
honest 21,580, 254 units at 100%.

---

## Lead 1 — SETTLED for 2 of 3. Shipped +632 B / +2 fns (predicted +632 / +2).

### The anchor that broke the circularity

Lane W7 stalled because every available anchor read callee names out of the
same suspect map — *a fixed-point problem, not a proof*. The escape here is
that **`_M_create_node` bodies are self-describing**. Disassembled:

| builder | body | ⇒ value_type |
|---|---|---|
| `0x822dd240` | `lhz r10,0(r31)` / `sth r10,0(r11)` — **one** halfword | bare `unsigned short` ⇒ **`set<G>`** |
| `0x822ddb48` | `lhz/sth` at **+0 AND +2** | `pair<const G,G>` ⇒ **`map<G,G>`** |

Both builders allocate `0x14`, so size alone cannot separate them — **the copy
shape can.** That is retail-byte evidence with no map input, and it confirms
both builder names. Their callers then follow, since `_M_insert` can only call
its own tree's builder (call edges verified by disassembly, not by a
`first_bl` heuristic).

### What shipped

| address | was | is | now homed |
|---|---|---|---|
| `0x822dda78` | `map<unsigned long,int>::_M_insert` | `set<G>::_M_insert` | ChordShapeGenerator.cpp |
| `0x822dedf0` | `map<unsigned long,int>::insert_unique` | `set<G>::insert_unique` | ” |
| `0x822deed0` | `map<G,RndFont::CharInfo>::_M_insert` | `map<G,G>::_M_insert` | ” |
| `0x822defa0` | ” `insert_unique(const value_type&)` | `map<G,G>::` ” | ” |
| `0x822df300` | ” `insert_unique(iterator,cvt&)` | `map<G,G>::` ” | ” |

Corroborated four independent ways beyond the builder bodies:

1. **Source.** `ChordShapeGenerator.h:26` declares `std::set<unsigned short>
   mVerts;` and the file is full of `std::map<unsigned short, unsigned short>&`
   parameters. Both trees are real, in that class, and nowhere else nearby.
2. **Pin geometry.** Retail's two proven builders were *already* pinned to
   ChordShapeGenerator.cpp, and `0x822dda78` is an island sitting in the gap
   **immediately before** CSG's own `0x822ddb48` block.
3. **A fourth body.** The 48-byte tail of the `0x822defa0` block is a copy loop
   reading two halfwords at **+0/+2 with stride 4** — iterating
   `pair<const G,G>`. A `map<G,CharInfo>` copy would stride 24.
4. **Symbol availability.** Every replacement name was lifted **verbatim from
   `ChordShapeGenerator.obj`'s own symbol table**, so hard limit #1 ("the pinned
   unit's obj must be able to define the name") is satisfied *by construction*
   rather than by hope.

### ★ Why the metric was blind — the transferable lesson

The family `{insert_unique ×2, _M_insert}` was mis-named **consistently**, so
every *intra*-family relocation name agreed and two of the three rows sat at
**fuzzy 100**. Only the single edge to the correctly-named `_M_create_node` was
charged. ⇒ **A uniformly wrong family is invisible to `name_check`; only its
boundary with a correctly-named neighbour is charged.**

⛔ **Corollary that decides the economics: correcting the flagged row ALONE is
net-NEGATIVE.** Renaming `_M_insert` without its two `insert_unique` callers
breaks family consistency and charges them — **−696 B** against a **+204 B**
gain. The screen flags the member; the fix is the family.

### Predicted vs measured

Pre-registered **+632 B / +2 fns**; measured **+632 B / +2 fns**, Δhonest +2,
Δcode% +0.006128pp. The decomposition was also predicted correctly:

* `set<G>` family was a **free roll** — its rows sat at fuzzy 97.16 / 95.71,
  i.e. worth **zero bytes** under all-or-nothing `matched_code`. **+428 B.**
* `map<G,G>::_M_insert` crossed. **+204 B.**
* The 224 + 472 B rows moved units and stayed at 100 ⇒ **Δ0** (which is why the
  yield is +632, not +1,328).
* Caller cascade **Δ0**, as priced: the three external callers
  (`map<G,G>::operator[]`, the templated `insert_unique`, a `_M_fill_insert`)
  were already at 100 and are all in CSG.

Units: ChordShapeGenerator 118→123, Font 76→73, XLSPConnection Δ0.

**Two-ruler signature:** `none` **+428 B** vs graded **+632 B**. The 428 is real
code newly pairing on *both* rulers; the extra 204 is the name-only crossing
visible solely to `name_check`. ⚠ `ab_measure` reported the alias control
**NOT_APPLICABLE**, *not* `ALIAS_SUSPECT` — because the patch carries splits it
can move real code, so the alias shape is only adjudicable on a map-only patch.
**No alias was added.**

### Third row — existence proven, assignment OPEN

`0x82456190` (named `set<FaderGroup*>::_M_insert`, needs `0x14`) branches to a
**`0x20`** builder. Not assigned, and the reason is worth recording: it is
called by **two structurally incompatible trees** — `insert_unique<set<FaderGroup*>>`
(node `0x14`) at `0x824563d8` and `insert_unique<map<Symbol,Award*>>` (node
`0x18`) at `0x824566e8`. **Three different sizes cannot describe one function**,
so at least two of those three names are wrong, and nothing in reach picks
which. Note `.?AVFaderGroup@@` occurs **0** times in retail, but `FaderGroup`
does — the class is simply **non-polymorphic**, so RTTI-absence proves nothing
here (unlike `HamMove`).

---

## Lead 2 — briefed figure HELD; premise re-verified; deliberately NOT re-homed

**43 rows is exactly right** (`default/HamMove`: 43 rows / 5,720 B, 23 at
fuzzy 100 = 3,036 B). Binary-absence re-verified in Python (never `grep`, which
is binary-blind here), with controls that can fail:

| probe | count |
|---|---|
| `.?AVHamMove@@` | **0** |
| `.?AVChordShapeGenerator@@`, `.?AVRndFont@@`, `.?AVObject@Hmx@@`, `.?AVRndMesh@@` | 1 each |
| fabricated `.?AVHamMoveXYZZY@@` | 0 |
| raw token `HamMove` **anywhere** in `band.exe` | **0** |

`src/system/hamobj/HamMove.cpp` exists — it is a **DC3 engine TU that RB3 does
not have**. Its pin is 23 `.text` blocks scattered across **nine** widely
separated regions (`0x8227…`, `0x822d…`, `0x8248…`, `0x8257…`, `0x827b…`).
⇒ **It is not a TU cluster, it is a catch-all scatter bucket** — the rows are
STL COMDATs that belong to many different owners.

**Not attempted, deliberately.** Re-homing 43 rows means 43 separate
`ChordShapeGenerator`-style adjudications, each needing its own owner proof and
its own obj-can-define check; 3,036 B are currently credited and would be
staked on getting *all* of them right. That is a lane of its own, and the
method this lane demonstrates is exactly the one it should use.

---

## Lead 3 — NOT opened, and why that was the right call

W7's trap is real and this lane's Lead 1 confirms the *mechanism* behind it:
the anchor must be outside the map. The `FileCache` sort cluster has no such
anchor in reach (both clusters pin to `FileCache.cpp`, so spatial grouping
cannot discriminate, and the token check is contaminated by construction).
The whole `ALL_RECIPROCAL` class is **16,620 B** and a transposition must be
fixed on *both* sides to pay. Budget went to Lead 1, where an outside anchor
existed and produced a measured, exact-prediction ship. **No swap was guessed.**

---

## `tools/node_size_screen.py` — audited, one FALSE CLAIM corrected, generalized

⚠ **The tool's stated rule was wrong.** It asserted *"a map's value_type is
`pair<const K,V>`, which is >= 8 B always ⇒ this is a SET"*. **False:**
`pair<const unsigned short, unsigned short>` is **4 B**. `0x822deed0` was
flagged "⇒ this is a SET" and is in fact `map<G,G>`. **The flag was right, the
diagnosis was wrong** — and left uncorrected it would have sent the next lane
hunting a set that does not exist.

Replaced with an **exact** test: `pair_size()` sizes the declared
`pair<const A,B>` from the mangled name (alignment-aware, so `pair<const G,G>`
correctly comes out 4) and flags only a genuine disagreement with the builder's
own `li r3,N`. It returns `None` — no flag — when either half is a class it
cannot size, so it stays conservative.

Effect, measured: the two rows corrected above **stop** flagging (the old rule
would have kept flagging the now-correct `map<G,G>` **by construction**), and
**three new defects the old rule could not see** appear:

| address | name declares | builder allocates |
|---|---|---|
| `0x822fa428` | pair = 8 B (node `0x18`) | `0x24` (value_type 20 B) |
| `0x824730e8` | pair = 8 B (node `0x18`) | `0x24` (value_type 20 B) |
| `0x826da488` | pair = 8 B (node `0x18`) | `0x34` (value_type 36 B) |

All three are **existence proven, assignment open**.

Two **known limits** are now recorded in the tool's docstring, so a clean run is
never read as a clearance: it screens **only `_M_insert`** (the defect is
usually a whole family, and the family is what must move), and the old map rule
**only fired for value_type < 8**.

---

## ⇒ Hand-back to a `src/**` lane (this lane owns the map side only)

**`RndFont::CharInfo` is very likely 4 bytes too big in our header.**
`src/system/rndobj/Font.h:65` gives it five 4-byte members (20 B) and carries
the comment *"`mUnk10` — unidentified; sized by the 0x28 node"*. But
`0x824730e8` is a `G`-keyed tree whose builder allocates **`0x24`**, i.e.
value_type 20 B ⇒ `pair<const unsigned short, CharInfo>` = 20 ⇒
**`CharInfo` = 16 B**, the four floats and **no `mUnk10`**. The `0x28` node the
comment reasons from appears to be *our* node, not retail's — the same
"header comment reasoning from a wrong premise" shape W7 found in `Rnd.h`.
Not acted on here: `src/**` belongs to lane W11b this session.
