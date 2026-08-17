# W14-FAMILY — the family is the unit of repair, twice proven and once refused

**2026-08-17.** Baseline reproduced **exactly** as briefed:
**44,483 fns / 3,745,128 B / 36.287666%**, honest 21,583, 254 units at 100%,
on the shipped `name_check` ruler.

**Shipped: +1,540 B / +3 fns / +3 honest, predicted exactly, `none` +924 B
(also predicted).** Commits `6d0fd499` (change) and `3bb15177` (`.pdata`
convergence).

---

## Target B — SETTLED. Two families, same shape as W12's Lead 1.

`node_size_screen.py` flagged `0x822fa428` and `0x826da488` as `_M_insert`
rows whose builder allocates the wrong size. Both turned out to be a whole
**three-row family** mis-named *consistently* — invisible to `name_check`
except at its one boundary edge, exactly W12's finding.

| | flagged as | truth | moved |
|---|---|---|---|
| **A** `0x822fa428` `0x822fa5f8` `0x822fc248` | `map<Symbol,int>` | `map<int,Hmx::Color>` | MoveMgr.cpp → VocalTrackDir.cpp |
| **B** `0x826da488` `0x826da558` `0x826da9a8` | `map<int,Symbol>` | `map<TrackType,PerfectSectionTracker::PlayerStreakData>` | Song.cpp → band3/game/PerfectSectionTracker.cpp |

### The anchors, none of which reads the suspect map for its conclusion

1. **Self-describing builders (retail bytes).** `0x822f8f80`: `li r3,0x24` +
   a five-word 20 B memberwise copy ⇒ `pair<const int,Color>` (4+16).
   `0x826da1f0`: `li r3,0x34`, copies **one word at +0** (the key) then
   `memcpy(dst+4, src+4, 0x20)` ⇒ **4 B key + 32 B POD value**. Both REFUTE
   the flagged 8 B / node-`0x18` names outright. Note the shape, not the size,
   is what carries the information — W12's lesson repeated.
2. **Call edges by disassembly**, never a `first_bl` heuristic. `_M_insert`
   can only call its own tree's `_M_create_node`, and each does.
3. **Call-graph closure.** Each `_M_insert` has exactly two callers — its own
   two `insert_unique` overloads (the hint overload falls back to the non-hint
   one, classic STLport). The family's **single external entry is the tree's
   own `operator[]`**: `map<int,Color>::operator[]` (already pinned
   VocalTrackDir) and `map<TrackType,PlayerStreakData>::operator[]` (already
   pinned PST). ⇒ **the map contradicted itself**, the `0x82272140` precedent
   in `retail_callers.py`'s own docstring.
4. **Completeness.** Each *true* tree was missing **exactly** `_M_insert` +
   two `insert_unique`; each *wrong* family held exactly those three in
   surplus. Two holes, one shape.
5. **Pin geometry.** All six rows were islands carved out of the destination's
   own region. After the move PST is a nearly unbroken run
   `0x826DA0E0–0x826DBC68`, and VocalTrackDir's `0x822FA24C–0x822FA500` is
   contiguous. The islands filled exactly the holes they came from.
6. **`MoveMgr.cpp` is a SECOND DC3-only scatter bucket, like `HamMove.cpp`.**
   `.?AVMoveMgr@@` = **0** and the raw token `MoveMgr` = **0** anywhere in
   `band.exe`, against four positive controls at 1
   (`.?AVVocalTrackDir@@`, `.?AVSong@@`, `.?AVRndFont@@`, `.?AVObject@Hmx@@`)
   and a fabricated negative `.?AVMoveMgrXYZZY@@` = 0. ⇒ nothing pinned to
   `MoveMgr.cpp` can be owned by a class `MoveMgr`. (MoveMgr is a polymorphic
   `Object` subclass, so RTTI-absence is valid here — W12's `FaderGroup`
   caveat does not bite.)

**Hard limit #1 satisfied by construction:** every replacement name was lifted
verbatim from the **destination** obj's own symbol table, matched *bijectively*
by relocation-normalized body hash, and checked for map collision (all free).

### ★ The control that mattered — and it discriminated

Body identity alone would have been a weak instrument here, because
`body_hash` **masks relocated fields** and therefore ignores callee identity.
Run against the *current* (wrong) home:

| family | our current-home instantiation vs retail |
|---|---|
| A (MoveMgr `map<Symbol,int>`) | **DIFFERENT** — 204/**224**/**472** vs retail 204/**232**/**488** |
| B (Song `map<int,Symbol>`) | **IDENTICAL** — the ICF-ambiguity hazard is real |

⇒ For **A** the body test is decisive; for **B** it is **inconclusive**, and B
rests entirely on anchors 1/3/4/5, which are independent of it. A body test
that cannot fail proves nothing — that is why the control was run.

### Predicted vs measured

Pre-registered **+1,540 B / +3 fns**; measured **+1,540 B / +3 fns**,
Δhonest +3, Δcode% +0.014920pp. Decomposition also as priced:

* VocalTrackDir **+3** — family A's three rows were sub-100 on **both** rulers
  (98.53 / 94.40 / 93.28), so they were earning **zero bytes**; re-homing made
  them exact. **+1,124 B** (924 of code + the 200 B `operator[]` crossing).
* PerfectSectionTracker **+3** / Song **−3** — a clean transfer, family B
  Δfns **0**, because both of its charged rows were *already* `mpn` 100 with
  bytes withheld. **+416 B.**
* MoveMgr **−0** — it lost three rows but no credit, since they were sub-100.
* The **720 B** of family B already at `fuzzy` 100 **held at 100**, as required.

**Two-ruler signature, predicted before measuring:** `none` **+924 B** vs
graded **+1,540 B**. The 924 is real code newly pairing on *both* rulers; the
other 616 B (two `operator[]`s + B's `_M_insert`) is name-only crossing visible
solely to `name_check`. Alias control reported **NOT_APPLICABLE** (splits-
carrying patch), as the brief predicted. **No alias was added.**

---

## Target A — briefed figures HELD; one region PROVEN, execution REFUSED

**43 rows / 5,720 B, 23 at `fuzzy` 100 = 3,036 B — exact.** 23 `.text` blocks
across nine–ten regions, confirming the catch-all-scatter reading. ⚠ The
heading is **bare** (`HamMove.cpp:`) while `objects.json` declares it nested
(`system/hamobj/HamMove.cpp`) — the bare-vs-nested trap is live in this very
unit; key on full path.

### The one region worth opening — and it is NOT re-homing, it is a rename

Buried in the 43 rows is `map<int,float>::operator[]` (`0x82272140`, 164 B,
**fuzzy 99.878 / mpn 100 — charged on a relocation name**). That is the row
lane W2 corrected for +8,048 B. **W2 fixed the member and left the family**:
its whole `_Rb_tree` is still named `map<Hmx::CRC,float>` —
`_M_create_node` `0x82271d48`, `_M_insert` `0x82271da0`, `insert_unique`
`0x82271e70` / `0x82271f58`, `_M_copy` `0x822fa6e8`, `operator=` `0x822fc430`
— **1,408 B, all at `fuzzy` 100.**

**PROVEN, on retail bytes and the call graph:**

* `0x82271d48` does `li r3,0x18`, copies the key `lwz/stw` and the value
  **`lfs f0,4(r31)` / `stfs f0,4(r11)`** ⇒ a **float-valued** tree. The
  builder's *copy shape* discriminates where size cannot: our `map<int,int>`
  builder hashes `b4f02acbffe3` and our `map<int,float>` builder
  `38a55a19f63c`, same 84 B.
* The family has exactly **two** external entries, both correctly named and
  independently pinned: `map<int,float>::operator[]` and
  **`VocalTrackDir::Copy`** (`0x822ffb10`). Every external user is
  **int-keyed**; `operator>>(BinStream&, map<int,float>&)` sits in the same
  region. **No CRC-keyed user exists anywhere in retail.**
* `HamMove` is absent from retail (W12's proof), so the DC3 TU that would have
  supplied a `map<CRC,float>` does not exist here. ⇒ the competing name has
  **no possible RB3 owner** — `retail_callers.py`'s own stated criterion for
  WRONG-and-fixable.

**The prize, priced from the charged-site list (not the patch):**
`operator[]` **+164 B** and **`VocalTrackDir::Copy` +976 B** = **+1,140 B**,
Δfns 0. The 976 B row is the whole play and it is nowhere near the patch —
`VocalTrackDir.obj` defines the complete `map<int,float>` tree including
`operator=`, so our side calls a name retail's `0x822fc430` does not carry.

### Why it was REFUSED anyway

The family is **one connected component** — `operator=` → `_M_copy` →
`_M_create_node` → `_M_insert` → both `insert_unique` — so it is
all-or-nothing (a partial fix is W12's measured **−696 B** trap). Closing it
requires freeing two squatted names, and **the map is asserted INJECTIVE ON
NAME globally**, so partial application is impossible:

1. `map<int,float>::insert_unique` is held by **`0x8233c668`** (Rot.cpp,
   488 B, `fuzzy` 100). Its sole caller is `map<int,int>::operator[]` — but
   its own callee chain runs into a *second* unresolved contradiction: the
   `_M_insert` it calls (`0x8233c2c8`, anonymous) branches to `0x8233bea0`,
   which is named **`map<int,Symbol>::_M_create_node`**. So that cluster is
   itself three-ways inconsistent and needs its own adjudication.
2. `map<int,float>::_M_copy` is held by **`0x8235c610`** (CharClip.cpp, 96 B,
   `fuzzy` 0 / `mpn` 3.54, **zero callers**). Its name is refuted by size —
   this tree's real `_M_copy` is 200 B — but **what it actually is cannot be
   proven**, only that its current name is wrong.

⛔ **And the deciding factor:** `scripts/symbol_aliases.json` already carries
**5 groups spanning exactly these trees**, and `map<int,int>::operator[]`
(`0x8233cab8`) sits at `fuzzy` **100 despite calling a `map<int,float>`-named
callee** — i.e. it is *already forgiven*. This name-space is governed by an
alias system this lane does not own and must not add to (hard limit #2). A
speculative eight-row chain rename here risks trading proven credit for
forgiveness, and would require **guessing** `0x8235c610` — precisely what
W12's Lead 3 refused to do. **No name was guessed.**

⇒ **Hand-off:** the head of the chain is proven; the blocker is two squatted
names, one of which (`0x8235c610`) needs an identification nobody has. A lane
that can adjudicate the Rot.cpp/CharClip.cpp cluster collects **+1,140 B**.

### The rest of the 43

Also necessarily mis-named, and recorded so nobody re-derives it: seven rows
templated on **`HamMove::LocalizedName`** plus `?Mirrored@HamMove@@` — nested
types/members of a class **proven absent from retail**, so those names cannot
be correct. They are *not* actionable: correcting them requires knowing the
true owner type, and they currently hold live credit. Six anonymous rows
(`fn_82714F18` 328 B, `fn_8269D4B8` 228 B, `fn_82484C00` 172 B, `fn_82714C00`
100 B, `fn_825748A0` 68 B, `fn_827150A8` 4 B ≈ 900 B) are at 0% and **cannot
pair by name by construction** — they need identification and source, not a
pin move.

⇒ **The honest verdict on the bucket: it cannot be profitably re-homed as a
bucket.** 3,036 B is live credit that a wrong assignment would destroy; ~900 B
is structurally unpairable; the only real vein is the `map<int,float>` family
above, and that is a *rename* blocked by name-injectivity, not a re-home.

---

## Instrument corrections (each cost this lane real time)

* ⛔ **`tools/body_match.py` is VACUOUS on named target rows.** Its target side
  is filtered to `fn_`/`lbl_` only, so any already-renamed row is excluded **by
  construction** — it returned a clean `0 bijective identifications` for six
  rows that are in fact all body-identical. A decisive-looking zero; caught
  only by reading `main()`. Use its `parse`/`body_hash` primitives directly.
* ⛔ **A raw COMDAT-size body hash is ONE-SIDED for EH-bearing functions.**
  `_M_copy` reads 200 B on the target side and 256 B on ours (8 B EH prefix +
  funclet), so the hashes cannot match and the tool reports "0 candidates" —
  an *artifact*, not evidence. Same family as the STLport `+8 B` error.
* ⚠ **"No mismatches — all 244 instructions match (100.0%)" on a row scoring
  99.959.** `run_diff_inspect`'s mismatch list is instruction-level;
  relocation-name charges are argument-level (`diff_arg`) and coexist with
  full instruction equality. Here that *was* the signal — the only possible
  penalty was a callee name — but never read it as "this row is clean".

## Figures from the brief that were wrong

**None.** Baseline, the HamMove 43 rows / 5,720 B / 23 at 100 / 3,036 B, the
nine-region scatter, and both Target-B addresses all held exactly as stated.
