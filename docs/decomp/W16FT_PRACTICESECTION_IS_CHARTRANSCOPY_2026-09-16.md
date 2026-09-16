# W16-FT — the four "PracticeSection" virtuals are a DIFFERENT CLASS: `CharTransCopy`

**Lane:** W16-FT · **Base:** `main` @ `44213c04` · **Branch:** `w16-ft` · **Date:** 2026-09-16

## 1. The brief's headline hypothesis was REFUTED

The brief argued that four consecutive virtuals of `PracticeSection` sitting at
`fuzzy 0 / mpn ~2` simultaneously — `SyncProperty` 268 B, `Load` 160 B, `Copy` 136 B,
`Save` 124 B, **688 B exactly** — is "one defect in the class shape or vtable layout,
not four independent body bugs", and instructed: *diagnose the class before touching
any body*.

The diagnosis was right that the cause is **shared and structural**. It was wrong
about **which** structure.

**Refutation, measured first:** all seven `$4PPPPPPPM@A@` adjustor thunks in that pin
were **already at fuzzy 100**. A wrong vtable layout or a wrong `this` adjustment
breaks the adjustor thunks *first* — they are nothing but the adjustment. A class-shape
defect that leaves every thunk byte-exact does not exist.

**The actual cause:** the `PracticeSection.cpp` pin at `0x823C7908–0x823C803C` /
`0x823C8040–0x823C8198` is a **mis-attribution**. Those bytes are not
`PracticeSection` at all. They are **`CharTransCopy`** — a Milo engine class that
rb3-xenon did not have in `src/` at all. `obj_target_symbol_renamer` had stamped
retail's `CharTransCopy` bodies with `PracticeSection` mangled names, and **objdiff
pairs target↔base by NAME**, so our real `PracticeSection` bodies were being diffed
against a different class's machine code.

### Why it hid for so long

The macro-generated bodies of two unrelated Milo classes are near-identical:
`SetType`, `Handle`, `ClassName`, `??_G` and the `$4` thunks differ only in a
relocation or two. Under the mis-attribution they scored **98–100%**, which reads as
"nearly done". Only the four bodies that **enumerate members** — `Save`, `Load`,
`Copy`, `SyncProperty` — depend on the class's actual field list, and those are exactly
the four that collapsed to `fuzzy 0`. **The 688 B signature was not a class-shape
defect; it was the subset of bodies capable of exposing a wrong class.**

### Triangulation (five independent lines, all agreeing)

1. Retail `Copy`'s `dynamic_cast` RTTI type descriptor at `0x82C6E390` decodes to
   **`.?AVCharTransCopy@@`** — read out of `orig/45410914/band.exe` directly.
2. Retail `SyncProperty`'s property-name literals are **`"src"` / `"dest"`**, with
   12-byte `ObjPtr` spacing at object `+0x08` / `+0x14`. `PracticeSection`'s properties
   are difficulty/display-name/steps/seqs.
3. The in-tree record already said so: `scripts/harvest/reloc_disc/README.md:404-405`
   and `scripts/harvest/tu_locate/located_spans.json` (`lo 0x823C7700`, `hi 0x823C8128`).
4. The map *already* placed `?ClassName@CharTransCopy@@` at `0x823C7908` — **inside**
   the `PracticeSection` pin.
5. `src/system/char/Char.cpp:112-118` carries a prior lane's comment naming
   `CharTransCopy` as a class "this tree does not have at all".

## 2. What was done

- **Ported `CharTransCopy`** (`src/system/char/CharTransCopy.{h,cpp}`) from the rb3-Wii
  oracle, with the layout solved from **retail bytes**: `this` is the `Hmx::Object`
  virtual-base subobject at object `+0x24`, vbptr `+0x04`, `mSrc` `+0x08`,
  `mDest` `+0x14`. Confirmed afterwards by `class_layout_report.py` (the compiler),
  not by header comments.
- **Re-pinned** `splits.txt`: the two `.text` blocks moved from `PracticeSection.cpp`
  to a new `CharTransCopy.cpp` heading. The **union of pinned addresses is unchanged**,
  so `total_code` must not move.
- **Renamed 15 map rows** `PracticeSection@@` → `CharTransCopy@@`, and **added 2 new
  rows** for previously anonymous addresses (see §4).
- Declared `CharTransCopy.cpp` in `objects.json` as `NonMatching`.

## 3. Corrections I had to make to my own work (recorded because they are the lesson)

- **My first layout derivation was WRONG.** I read `lwz r4, 0x10(r30)` in `Copy` as a
  *member address* and derived `this = object + 0x2C`. It is not a member address — it
  loads the **object pointer field inside the `ObjPtr`**, which sits at `ObjPtr+8`.
  Correct answer: `this = object + 0x24`. The compiler layout report then confirmed it
  exactly. *An offset read off a single load instruction is a hypothesis; the compiler
  is the instrument.*
- **The oracle was the defect, again.** rb3-Wii spells `CharTransCopy::Save` as
  `SAVE_OBJ(CharTransCopy, 0x2D)` — an **assert-only stub with no body**. RB3-360 retail
  has a real `Save` (`fn_823C7B88`: `SAVE_REVS(1,0)`, one `Hmx::Object::Save`, then the
  two members). Written from retail bytes per the standing rule that **retail outranks
  both oracles**. This is now the sixth recorded instance of the oracle being wrong.
- **Protected vs public ctor/dtor is not cosmetic here.** Access is pure name-mangling —
  identical machine code — but it decides whether our obj *defines the name the target
  map assigns*. In this very obj `$2` ↔ protected and `$4` ↔ public, and the retail
  deleting dtors are the `$4` / `UAA` forms. A protected dtor emits `??_G…MAA…` /
  `??_E…$2…`, which **no target row can ever pair with ⇒ a permanent 0%**. I verified
  **16/16** required mangled names were defined by our obj *before* touching the map.
- `HANDLE_CHECK(0x4C)` (rb3-Wii spelling) was **removed, not ported**: it belongs to the
  parallel `obj/ObjMacros.h` macro dialect whose `INIT_REVS`/`SYNC_PROP` signatures are
  incompatible with `obj/Object.h`'s, and including it cascaded into
  `'src'/'dest' undeclared` + `gRev is not a member of global namespace`. `Object.h`'s
  `END_HANDLERS` already emits the `PathName(this)` tail that retail's call to
  `fn_82757BA8` evidences.

## 4. `Load`: the `BinStreamRev` that retail never builds

After the port, `?Load@CharTransCopy@@` sat at **59.45%**. Cause, read straight off the
listing: our base constructed a **`BinStreamRev` local** (`??0BinStream@@`,
`??_7BinStreamRev@@6B@`, `??1BinStream@@`, `__savegprlr_28`, frame Δ +0x30) that retail
**never creates**. Retail instead splits the packed rev into two mutable file-scope
shorts (`lwz r11,0x50(r1)` / `srwi r11,r11,16` / `sth r11, lbl_82CBF7B4@l(r9)` /
`sth r10, 0x4(r8)`) and streams with the plain `bs`.

`Object.h`'s `LOAD_REVS` unconditionally builds `BinStreamRev d(bs, revs)`, and
`INIT_REVS` declares `gRev`/`gAltRev` `static **const**` — so the macro pair cannot
produce retail's codegen at all.

★ **The fix was already in the tree and I should have looked sooner.** Five
`src/system/char/` TUs (`CharBone`, `CharBonesBlender`, `CharEyeDartRuleset`,
`CharIKScale`, `CharLookAt`) already carry the solved pattern verbatim: one
`align(4)` aggregate holding `altRev` at `+0` and `rev` at `+4`, `#define`d over the
macro names, with a hand-written `Load` that does the split itself. The aggregate is
required — MSVC does not lay `.bss` out in declaration order, so two separate statics
get other globals interleaved and will not fold onto one base register (the
**MSVC global co-addressing** pattern: a compile-time `+4` implies **one aggregate**).
`getAltRev` = `packed >> 16` → `+0`, `getHmxRev` = low half → `+4`, which is exactly
retail's `srwi`-then-two-`sth` sequence.

Applied to `CharTransCopy::Load`: **59.45 → 100.0**, `+160 B`, all 40 instructions equal.
This is the "READ THE IN-TREE RECORD FIRST" rule paying out — the answer was five files
away in the same directory.

## 5. Naming two anonymous rows — adjudicated on bytes, not assumed

Two bodies inside the span were unnamed in the map, so objdiff could not pair them and
they read `fuzzy 0` regardless of source quality:

- **`fn_823C7AD8`** (128 B) restores two vtable pointers, then destroys `this-0x10`
  (`mDest`) and `this-0x1c` (`mSrc`) in **reverse declaration order**, and never calls a
  base dtor ⇒ `??1CharTransCopy@@UAA@XZ`. Spelling chosen from the direct in-map
  precedent `??1CharNeckTwist@@UAA@XZ` (same `$4`/public/`CharPollable` family), not
  from `??_D`.
- **`fn_823C8088`** (160 B) performs two identical `list` insert blocks: member pointer
  `0x1c` (= `mDest`, `ObjPtr+8`) into the **second** list argument, then `0x10`
  (= `mSrc`) into the **first** ⇒ `PollDeps`, and it independently confirms the
  argument order `change.push_back(mDest); changedBy.push_back(mSrc);`.

Both went to **fuzzy 100 on the first build**, `+288 B`. That two *member-enumerating*
bodies matched immediately is independent confirmation that the ported class shape is
right — a wrong layout breaks a destructor's member order first.

⚠ Note this is the **pairing** channel, not the call-site channel. The standing rule
that "naming an anonymous address has zero byte upside" concerns *callers* of that
address; here the rows themselves were unpairable, and our bodies were already written
and correct.

## 6. PRE-REGISTRATION (written BEFORE the authoritative A/B)

Predicted deltas, from the settled worktree reading against the brief's asserted base
(`matched_functions` 44040, `matched_code` 4150676, code% 40.50599, fuzzy 50.380030,
`total_code` 10247068, `total_functions` 69240, `masked_equal` 23246):

| measure | predicted Δ |
|---|---|
| `matched_functions` | **+12** |
| `matched_code` | **+1688 B** |
| `matched_code_percent` | **+0.01647 pp** |
| `fuzzy_match_percent` | **+0.00985 pp** |
| `total_code` | **0 (unchanged)** |

### Falsifiers (the set is able to express outcomes I do not expect)

1. **The brief's own falsifier, inverted.** My hypothesis says the four rows move
   **together**, but **under new names** — they leave `default/PracticeSection`
   entirely and reappear in `default/CharTransCopy`. If any of the four moves to 100
   while still named `PracticeSection`, mis-attribution is the wrong diagnosis and the
   brief's class-shape hypothesis is back in play.
2. **`total_code` must be exactly 10247068.** The splits edit only reattributes
   addresses between two headings; the union is unchanged. If `total_code` moves, I
   moved a boundary I did not intend and the measurement is not the one I priced.
3. **`default/PracticeSection` must SHRINK from 39 rows to 15.** If it keeps 39 rows,
   the splits edit was inert and any gain came from somewhere I have not identified.
4. **`default/CharTransCopy` must appear with 24 rows.** If the unit is absent, the
   forced re-split did not take on the A/B's leg B and the run is absent-vs-absent.
5. **Δ`matched_code` could come in BELOW +1688.** Of the 17 rows previously at fuzzy 100
   in `default/PracticeSection`, 12 move to the new unit; if any row that matched under
   the old pin fails under the new one, the net is smaller than predicted. A result
   between 0 and +1688 is a real possible outcome, not a bug.

## 7. RESULT — authoritative whole-binary A/B

`tools/ab_measure.py --pick ce642dcf`, both legs settled to zero work, both at a
`symbols.txt` split fixed point, ruler `name_check` (graded).

| measure | leg A | leg B | Δ | predicted |
|---|---|---|---|---|
| `matched_functions` | 44040 | 44052 | **+12** | +12 ✅ |
| `matched_code` | 4150676 | 4152364 | **+1688 B** | +1688 ✅ |
| `matched_code_percent` | 40.505990 | 40.522460 | **+0.016470 pp** | +0.01647 ✅ |
| `fuzzy_match_percent` | 50.380030 | 50.389880 | **+0.009850 pp** | +0.00985 ✅ |
| `masked_equal_functions` | 23246 | 23247 | +1 | — |
| honest (`matched − masked`) | 20794 | 20805 | +11 | — |
| `total_code` | 10247068 | 10247068 | **0** | 0 ✅ |

**All five pre-registered falsifiers cleared:**

1. The four rows moved **together**, `0 → 100`, and **under the new `CharTransCopy`
   names** — `Save` 124 B, `Copy` 136 B, `Load` 160 B, `SyncProperty` 268 B. None moved
   while still named `PracticeSection`, which is precisely the outcome that separates
   mis-attribution from the brief's class-shape hypothesis.
2. `total_code` and `total_functions` are **identical on both legs** (10247068 / 69240).
3. `default/PracticeSection` shrank **39 → 15 rows** (matched 18 → 6).
4. `default/CharTransCopy` appeared with **24 rows, 24 matched, 1948/1996 B**.
5. The net came in **at** +1688, not below it, and decomposes exactly:
   new unit **+1948 B** − **260 B** of previously-matching rows that left
   `default/PracticeSection` = **+1688 B**.

Units at 100% (mpn): 192 → 193, mechanism `NEW_UNIT`. Note units and bytes are separate
measures and neither is being sold as the other.

⚠ The `none`-ruler control reads `NOT_APPLICABLE` by design: the patch carries source,
so movement on `none` is expected and the alias shape is only adjudicable on a map-only
patch.

## 8. What I did NOT do, and why

- **I did not fix the remaining 48 B** (`fn_823C7B58`, 99.58%). It is the dtor's EH
  cleanup funclet, and objdiff reports it as **paired by masked byte signature, not by
  name** — so its single charge (`~ObjRefConcrete<RndTransformable,ObjectDir>` vs our
  `~ObjPtr<RndTransformable>`) is *unfalsifiable by construction*, exactly as the
  `UNVERIFIABLE_PAIRING` detector says. Our obj emits `__ehfuncinfo$` and
  `__unwindtable$` but no `__unwindfunclet$` symbol, so there is nothing to name. The
  only real lever would be changing `ObjPtr<T>`'s destructor spelling, which is a
  tree-wide edit affecting every Milo class — far outside this lane and not worth 48 B.
- **I did not add `REGISTER_OBJ_FACTORY(CharTransCopy)` to `Char.cpp`.** Retail has it.
  It is a separate lever in a different TU that needs its own measurement, and folding
  it into this patch would have made the +1688 unattributable.
- **I did not touch `PracticeSection.{h,cpp}` at all.** The four rows the brief targeted
  were never its bodies. Its real remaining rows (the `vector<PracticeStep>` STL
  template family at 99.6–99.8, `ClearSteps` at 97.0) are ordinary decomp work, not part
  of this diagnosis.
- **I did not chase the rest of the old `PracticeSection` pin.** `fn_823C8128` (104 B)
  is provably *not* `CharTransCopy` (it is past the class's last body) but its true owner
  is unknown, so it was left pinned where it was rather than moved on a guess. The wider
  suspicion — that RB3's real `PracticeSection` is the POD in
  `src/band3/game/PracticeSectionProvider.h` and this DC3-era animatable class may not be
  the right shape for RB3 at all — is **recorded and unresolved**, not investigated.
- **I did not price the brief's adjacent rows** (`list<PracticeSectionMapping>::erase`
  108 B / `::insert` 100 B in `default/Group`, `??1PracticeSectionProvider` 116 B at
  48.48). They share a *name*, not the defect; the defect here was attribution of a
  specific address span.

## 9. Transferable lessons

1. **A shared signature does not identify a shared cause.** Four virtuals failing
   together correctly said "one structural cause", and the obvious structural cause
   (class shape) was wrong. The cheap discriminator was already on the board: *if the
   vtable were wrong, the adjustor thunks would not be at 100*.
2. **objdiff pairs by NAME, so a wrong map name is indistinguishable from a wrong body
   — except in the bodies that enumerate members.** Macro-generated Milo bodies read
   98–100% across *different classes*. When a unit shows "everything nearly matches
   except Save/Load/Copy/SyncProperty", suspect the attribution before the source.
3. **Read the in-tree record first — again.** The `Load` fix, the span, and even the
   class's absence from the tree were all already written down in this repository
   (`reloc_disc/README.md`, `located_spans.json`, `Char.cpp`, and five sibling TUs in
   the very directory the new file lands in). Nearly every hour of this lane was
   re-deriving something already recorded.
4. **Access specifiers are load-bearing for pairability**, not style: `$2` ↔ protected,
   `$4` ↔ public, and a mismatch is a permanent 0% no matter how correct the code is.
