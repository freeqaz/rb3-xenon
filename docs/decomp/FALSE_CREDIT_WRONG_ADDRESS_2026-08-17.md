# Rows that score 100 on the WRONG address — lane W9-FALSECREDIT, 2026-08-17

Three defects were handed to this lane, all previously adjudicated on retail
bytes by earlier lanes but left unapplied because each needs a coordinated
multi-file edit. Two are closed here. The lane was briefed to expect a
**net-negative** byte result (accuracy over headline). It measured **+320 B**
instead — and the reason that happened is the most reusable thing in this file.

| defect | verdict | measured |
|---|---|---|
| (1) `map<int,float>` node-builder false credits | **FIXED** | **+268 B**, Δfn 0 |
| (2) `0x827b7d88` = `StoreArtLoaderPanel::Unload` | **FIXED** | **+52 B**, Δfn 0 |
| (3) `ByteCode`/`StaticByteCode` bijection | see the section below | — |

Baseline `44,479 fns / 3,744,044 B / 36.277164% / honest 21,579 / 254 units at
100%`. After both: `36.280262%`. The two runs chain exactly (run 2's leg A ==
run 1's leg B == 36.279760), so the deltas compose.

---

## ★ Removing a false credit is not the same as losing bytes

The lane's premise — *a row at `fuzzy == 100` on a wrong name is credit we were
never owed, so removing it costs bytes* — held for the **row** and was wrong for
the **binary**, in both defects:

* Defect (1): the touched rows net to **exactly zero**. 232 B of false credit
  left `BandList`; the identical 232 B of honest credit arrived in
  `Accomplishment`. All **+268 B** came from three *callers I never touched*.
* Defect (2): the corrected row went from `fuzzy 99.23077` under the wrong name
  to **`fuzzy 100.0`** under the right one. The forgiveness was **worse than
  the truth**.

⇒ **A wrong map name is not a free +N. It is a local +N financed by a charge on
every caller that relocates against it.** Correcting it pays that back. Price
from `report.json`'s charged-site list — *including the call sites* — never from
the patch. (This lane predicted defect (1) at ~0 B and measured +268; that is
**the same miss W8-TWINPORT made** — predicted +24, measured +184. Twice now.)

---

## Defect (1): `0x825948C0` / `0x82594990` are `set<ScoreType>`, not `map<int,float>`

**Existence — decisive, map-independent.** `0x8235c328` does `li r3,0x14` →
`MemOrPoolAlloc`, then copies exactly **one word** to node+0x10. An `_Rb_tree`
node is `_Rb_tree_node_base` (16 B) + value_type, so its value_type is **4 B**.
`pair<const int,float>` is 8 B and needs `0x18`. `0x825948c0` calls that builder
**three times**, and `0x82594990` calls `0x825948c0`. Different-size COMDATs
cannot fold, so this is a **wrong name**, not an arbitrary ICF survivor name.

**Comparator control (it discriminates).** At the same slot, `0x82594914` emits
`cmpw` (**signed**) where the twin `0x8235c58c` emits `cmplw` (unsigned pointer).
So the key is a signed 4-byte int/enum — and the two 204 B `_M_insert` bodies at
distinct addresses are explained: they differ in exactly that one instruction,
so ICF could not fold them.

**Assignment — the separate claim.** Several signed 4-byte sets exist
(`set<int>`, `set<TrackType>`, `set<ScoreType>`), so this needed three agreeing
lines:

1. **An anchor outside the map.** The rb3-Wii oracle declares
   `virtual bool InqRequiredScoreTypes(std::set<ScoreType>&) const`
   (`../rb3/src/band3/meta_band/Accomplishment.h:37`). Three of the twelve retail
   callers are that override on `Accomplishment` / `AccomplishmentSetlist` /
   `AccomplishmentConditional`.
2. **`Accomplishment.cpp`'s own pin set brackets the island** — it pinned
   `…–0x825948C0` and `0x82594A78–…`, a hole of exactly 440 B = 204 + 4 pad +
   232. `BandList`'s other blocks are all at `0x8233xxxx`; this was a lone
   2-block island 2.5 MB away.
3. **Survivor provenance breaks the fold tie.** `Accomplishment.obj`'s *only*
   set instantiation is `set<ScoreType>`, and no `_M_insert`/`insert_unique` for
   it was mapped anywhere else — no collision.

**The metric then corroborated the assignment through rows nobody touched.**
The +268 B is `+116` `AccomplishmentConditional::InqRequiredScoreTypes`, `+76`
`Accomplishment::…`, `+76` `AccomplishmentSetlist::…`, crossing to `fuzzy 100`
because their `bl` finally resolves to the right callee name. **Pick `set<int>`
or `set<TrackType>` instead and those three do not cross.**

---

## Defect (2): `0x827b7d88` is `StoreArtLoaderPanel::Unload`

Four independent lines, none resting on the suspect name: the body calls
`ClearArt@StoreArtLoaderPanel` and `Unload@UIPanel`; `ProfileMgr : public
MsgSource` (not a `UIPanel`, no `ClearArt`) while `StoreArtLoaderPanel : public
UIPanel`; its two retail `bl` callers are `StoreInfoPanel::Unload` and
`StoreMainPanel::Unload`; and `system/meta/StoreArtLoaderPanel.cpp` already
pinned `…–0x827B7C20` and `0x827B7DC8–…`, so the 64 B block slots into its set.

### ★★ A T1 alias can be built on TRUE evidence and a FALSE inference

`symbol_aliases.json` carried a **T1** group: survivor
`?HandleProfileLoadComplete@ProfileMgr@@`, folded
`[?Unload@StoreArtLoaderPanel@@]`. Its stated evidence — *retail bytes at
`0x827b7d88` are byte-identical to our compiled `Unload@StoreArtLoaderPanel`* —
**is true.** The inference is not:

> **T1 byte-identity between retail-at-A and our-symbol-B is equally consistent
> with (i) A and B were folded, and (ii) A simply IS B and the map name is
> wrong.** `icf_alias_build.py` cannot distinguish these and here it chose (i).

This is the same disease as objdiff's `LINKER_MERGED`/`AT_LIMIT` conflation, one
level up: *a detector restating its own input*. **Measured cost of believing
it:** `fuzzy 99.23077` billed to the wrong unit, versus `100.0` in the right one
once the name was corrected and the alias withdrawn.

The group was **kept with `folded: []` plus a `withdrawn` record, not pruned** —
classes that forgive 0 today become live as porting advances, and a prior prune
cost +94,616 B to reverse.

**NOT done deliberately:** `?HandleProfileLoadComplete@ProfileMgr@@` is now
mapped nowhere. It is a genuine folded spelling of `0x825490e8`
(`HandleProfileSaveComplete`; the twins are source-identical and that survivor
calls exactly `HandlePendingProfileUploads` + `HandlePendingGamerpicRewards`).
**No alias was added for it.** Adding forgiveness lifts the score by
construction and needs its own T1 proof; leaving it unmapped is the conservative
direction.

---

## The generalized instrument: `tools/node_size_screen.py`

`16 + sizeof(value_type in _M_insert's name)` **must equal** the `li r3,N` in
the `_M_create_node` it actually branches to. Map-independent, and immune to the
fold defence because different-size COMDATs cannot fold.

★ **Its calibration is the lesson.** The first version checked only SET rows, so
against the pre-fix map it did **not** flag `0x825948c0` — the very row it was
written to generalize, which wore a MAP name. A "0 findings" run would have read
as a clearance. Adding the complementary rule (a map's `pair<const K,V>` is
**≥ 8 B always**, so a `0x14` node is impossible for any map) makes the positive
control fire on `0x825948c0` exactly.

**Three further mismatches, UNADJUDICATED — existence proven, assignment not:**

| address | current name | builder allocates | ⇒ |
|---|---|---|---|
| `0x822dda78` | `map<unsigned long,…>::_M_insert` | `0x14` | a 4 B-value SET |
| `0x822deed0` | `map<unsigned short,…>::_M_insert` | `0x14` | a 4 B-value SET |
| `0x82456190` | `set<FaderGroup*>::_M_insert` | `0x20` | a 16 B-value tree |

The last is defect (1)'s mirror image and is **harder**: its key compare is
`cmplw` (**unsigned**), which does *not* fit `map<TrackType,…>` — the family
owning the `0x826e0950` builder it calls. So the wrongness is proven and the
right name is not determined. **Do not guess one to move the metric.**

⛔ **Scope bound:** only **26** `_M_insert` rows are mapped at all, and the
MAP-side rule fires only at value_type < 8 B. A map whose pair is merely the
*wrong* size ≥ 8 is not caught. This is a screen, not a census.

---

## Traps this lane hit (all cost real time)

* ⛔ **`git diff` is the wrong way to build a splits patch.** HEAD's
  `splits.txt` is **not** at its own re-split fixed point, so a diff taken after
  any build carries derived-`.pdata` drift hunks (`lsp.c`, `CharClip.cpp` ↔
  `MoveAsyncDetector.cpp`) that were already applied post-settle — `ab_measure`
  then **REFUSED** on `patch does not apply`. Build the patch from a clean
  `git checkout -- config/45410914/splits.txt`, editing only `.text`.
* ⛔ **The bare-vs-nested heading trap, live.** `grep '^StoreArtLoaderPanel.cpp:'`
  returned nothing and I was one step from creating a duplicate entry — the real
  heading is **`system/meta/StoreArtLoaderPanel.cpp:`** and it already had pins.
  CLAUDE.md says this has broken four consecutive lanes; it nearly made five.
  **Key on the full path, never `basename()`.**
* ⚠ `scripts/target_symbol_map.json` has a non-address key
  (`_bijection_arbitrary`) — `int(k, 16)` over `.items()` crashes.
* ⚠ Do not name a scratch disassembler `dis.py`; it shadows the stdlib module
  `inspect` imports, and capstone fails with an unrelated-looking
  `AttributeError`.
