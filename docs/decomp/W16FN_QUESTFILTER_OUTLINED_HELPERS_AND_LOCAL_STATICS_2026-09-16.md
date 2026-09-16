# W16-FN — `QuestFilterProvider::Text`, and the two defect classes behind it

**Date** 2026-09-16 · **Base** `main` `3c6eacd1` · **Branch** `w16-fn` ·
**Ruler** `name_check` (graded), read from `report.json`.

Brief: close one row, `?Text@QuestFilterProvider@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z`
(512 B, fuzzy 0.000000) in `default/QuestFilterPanel`.

**Delivered +1,320 B / +4 functions** across three pre-registered whole-binary
A/Bs, every predicted delta hit exactly and no falsifier fired.

| # | change | Δcode_bytes | Δmatched | Δcode% |
|---|---|---:|---:|---|
| 1 | `Text` calls the two helpers retail calls | **+512** | +1 | +0.004997 pp |
| 2 | name `fn_82B7B550` = `UpdateSongLabel` **+** fix `cmpw` operand order | **+428** | +1 | +0.004177 pp |
| 3 | name `AreCurrentFiltersValid`, name+fix `GetSelectedSetlistType` | **+380** | +2 | +0.003708 pp |
| | **lane total** | **+1,320** | **+4** | **+0.012882 pp** |

Whole-binary: `matched_code` 4,147,612 → 4,148,932; `matched_functions`
44,030 → 44,034; `matched_code_percent` 40.476086 → 40.488968.
`total_code` **10,247,068** and `total_functions` **69,240** identical on all
six legs, so the percentages are comparable across the three runs.
Unit `default/QuestFilterPanel`: `matched_code` 4,764 → 6,084 of 7,004 (68.0%
→ 86.9%), rows at 100 62/71 → 67/71.

---

## 1. The primary finding: our source was VERBATIM the oracle, and that is why it was wrong

`src/band3/tour/QuestFilterPanel.cpp`'s `Text` was byte-for-byte the rb3-Wii
oracle (`../rb3/src/band3/tour/QuestFilterPanel.cpp`) — and compiled to **928 B
against a 512 B target at fuzzy 0.000000**. DC3 has no `QuestFilterProvider` at
all, so rb3-Wii was the only oracle, and it was the defect.

The oracle **hand-expands two helpers that retail emits once and calls**:

- `QuestFilterProvider::GetSetlistType(int)` — retail `fn_82B7A078`, called
  from **three** sites: `Mat` (0x82B7B2F0), `Text` (0x82B7B7DC), and 0x82B7A628
  inside `GetSelectedSetlistType`. `Mat` already called it (hence `Mat` was at
  100 while `Text` was at 0); the other two inlined it by hand.
- `QuestFilterProvider::UpdateSongLabel(UILabel*, Symbol, TourSetlistType, int)`
  — retail `fn_82B7B550` (428 B). The oracle writes the `song1`/`song2`/`song3`
  bodies out three times; retail sets `li r7, 0/1/2` and makes **one** call.
  The helper was already *declared* in our header and *defined* in the .cpp —
  nothing called it, so MSVC emitted nothing for it.

⚠ **The index is 0-based in retail** (`li r7,0/1/2`; `GetSongName(r27)` direct,
`addi r5, r27, 1` for the 1-based display number), and `UpdateSongLabel` owns
the `iSongNum >= iNumSongs -> SetTextToken(gNullStr)` guard that the inherited
source kept in the caller.

**Why the callee names cost nothing:** objdiff's `name_check` *forgives*
placeholder target names (`fn_`/`lbl_`/…), so our `bl ?UpdateSongLabel@…`
against retail's `bl fn_82B7B550` scores **equal**. This was visible in the
pre-fix listing, where target `bl fn_82B7A078` vs base
`bl ?GetCurrentGigNum@TourProgress@@QBAHXZ` was already scored a MATCH — i.e.
**a completely wrong callee read as a match**, which is exactly why a mismatch
count is the wrong instrument for picking targets here.

## 2. The second defect class: retail uses FUNCTION-LOCAL STATICS where we use file-scope globals

Retail constructs its `Symbol`/`Message` token objects **inside the function
body**, under a shared guard bitfield — the MSVC function-local-static idiom.
Our inherited source referenced the file-scope `extern Symbol` / `extern Message`
declarations from `utl/Symbols*.h` / `utl/Messages*.h` instead.

**The tell is decisive and reusable:** a file-scope global is constructed in a
`??__E` dynamic initializer and *never* inside a function. So
`??0Symbol@@QAA@PBD@Z` → `??0Message@@QAA@VSymbol@@@Z` → `atexit` appearing in
a function body proves a local static.

Proven by reading the guard codegen plus the string literals out of the retail
PE (`orig/45410914/band.exe`, section-aware VA→offset; the labels are in
unpinned `.rdata` so they do not appear in any `.s`):

| label | string | guard word / bit |
|---|---|---|
| `lbl_82199A20` | `tour_setlist_random` | `lbl_82E12918` 0x1 |
| `lbl_82199A0C` | `tour_setlist_custom` | `lbl_82E12918` 0x2 |
| `lbl_821999F8` | `tour_setlist_fixed`  | `lbl_82E12918` 0x4 |
| `lbl_820B0B10` | `setlist_song_fmt`    | `lbl_82E12908` 0x1 **and** 0x4 |
| `lbl_8219994C` | `tour_random_song`    | `lbl_82E12908` 0x2 |
| `lbl_82199938` | `tour_custom_song`    | `lbl_82E12908` 0x8 |
| `lbl_821992C0` | `get_selected_filter_index` | `lbl_82E128B4` 0x1 |

★ `setlist_song_fmt` gets **two separate statics** (bits 0x1 and 0x4) because it
is declared once per branch. That is what told me the declarations sit inside
the `if` arms rather than at the top of the function.

The idiom was already in-tree at `src/band3/meta_band/AppLabel.cpp:207` and at
`QuestFilterPanel::GetBackScreen` (a prior lane found it for that one function),
so this is a known pattern that had simply not been applied TU-wide.

## 3. NEITHER HALF OF INCREMENT 2 PAYS ALONE — measured, not argued

Measured separately in-worktree on full settled builds:

| tree state | `matched_code` | the `UpdateSongLabel` row |
|---|---:|---|
| after increment 1 | 4,148,124 | `fn_82B7B550`, fuzzy **0.0**, unpaired |
| + map name only | 4,148,124 (**Δ0**) | fuzzy **99.85981**, base size == target size == 428 B |
| + `cmpw` operand flip | 4,148,552 (**Δ+428**) | fuzzy **100.0** |

The naming's own A/B value is **+0 B**, exactly as doctrine predicts — a row
pays only at `fuzzy == 100`, and 99.85981 pays nothing. What the naming bought
is **legibility**: it turned an invisible anonymous row into a two-instruction
diff. The residual was entirely:

```
target   cmpw cr6, r27, r3  ;  blt cr6, <work>     (iSongNum  <  iNumSongs)
base     cmpw cr6, r3, r27  ;  bgt cr6, <work>     (iNumSongs >  iSongNum)
```

Identical predicate; **MSVC takes the `cmpw` operand order from the source**, so
`iNumSongs <= iSongNum` → `iSongNum >= iNumSongs` closed both instructions.
This is a concrete, cheap, reusable lever: *a lone `diff_arg` on a `cmpw` plus a
`diff_op` on its branch is a comparison written with the operands the other way
round, not a regalloc wall.*

## 4. Identification standard used (and why a high score IS the adjudication)

Every map name added here was adjudicated on retail bytes **before** it was
written, and then confirmed by score:

- `0x82b7b550` → `UpdateSongLabel`: exact 4-arg-plus-`this`-const signature;
  body decodes term-for-term (`lwz r3, 0x3c(r3)` = `m_rProgress`,
  `GetNumSongsForCurrentGig`, `GetFixedSetlist`, `GetNumSongs` discarded,
  `GetSongName(r27)`, `__RTDynamicCast`, `SetSongAndArtistNameFromSymbol(s, r27+1)`).
  Confirmed: 0.0 → **99.85981** on naming alone.
- `0x82b7a288` → `AreCurrentFiltersValid`: tail-calls
  `SanityCheckQuestFilters@TourPerformerLocal`, which appears in **exactly one**
  method of this class. Confirmed: **100.0** on the first build, source untouched.
- `0x82b7a558` → `GetSelectedSetlistType`: `lwz r3, 0x58(r30)`
  (`m_pQuestFilterProvider`, header offset 0x58) then `bl fn_82B7A078`.
  Confirmed: **100.0**.

★ **A 100% score on a 276 B body is stronger evidence than any signature
argument** — a wrong name scores ~0, not 100. Conversely a *low* score after
naming is ambiguous (wrong name **or** right name with a divergent body), so it
can never be used as a "revert it" rule on its own. That asymmetry is why I
adjudicated first and scored second, rather than batch-guessing.

**Blast radius was checked before each map edit**, because naming an address
converts every call site to it from placeholder-FORGIVEN to NAME-CHECKED.
`0x82b7b550` has exactly one `bl` tree-wide (from `Text`, same unit); the other
references are `.4byte fn_82B7B550+0xNN` EH IP-to-state entries inside an
unpinned `auto_*` rdata unit, which is unpairable in both legs and cannot move.
Predicted "no other unit moves" as an explicit falsifier all three times;
`unit net (ALL units)` equalled whole-binary Δmatched in all three runs, so it
never fired.

---

## What I did NOT do, and why

**Four anonymous rows (880 B) remain open in this unit.** I identified all four
with high confidence but did not land them — the lane's budget was spent and a
half-adjudicated map edit is worse than none. The evidence is recorded here so
the next lane does not re-derive it:

| row | size | almost certainly | evidence |
|---|---:|---|---|
| `fn_82B7A3D8` | 300 B | `?GetSelectedFilter@QuestFilterPanel@@QAA?AVSymbol@@XZ` | two `Symbol("")` ctors + `DataNode::Int` + `Release`; matches the two `return ""` paths |
| `fn_82B7BCB0` | 196 B | `?Refresh@QuestFilterPanel@@QAAXXZ` | calls `?UpdateFilters@QuestFilterPanel@@QAAXXZ` by name |
| `fn_82B7A7D0` | 192 B | `GetSongSelectScreen` **or** `GetDiffSelectScreen` | ⚠ **not separable by callees** |
| `fn_82B7A8E0` | 192 B | the other of that pair | identical call shape to the above |

⚠ **The 192 B pair is the trap.** Both are `Symbol` ctor → `Message` ctor →
`atexit` → `DataNode::Sym` → `Release`, i.e. *identical* call sequences, so a
callee-based identification cannot tell them apart and a 50/50 guess would
install a wrong name that scores ~0 and looks like a body defect. Separate them
by reading the `Symbol` ctor's **string-literal argument** out of
`orig/45410914/band.exe` (the recipe is in §2 above) — `get_songselect_screen`
vs `get_diffselect_screen` — and only then name them.

All four also need the §2 local-static-`Message` conversion, since every one of
them constructs its `Message` inside the body. Expect the §3 shape to repeat:
the naming alone will read **+0 B** and the source fix collects.

**`fn_82B7A260` (40 B, fuzzy 99.50, mpn 100.0) is deliberately left alone.** It
is a `??1NetMessage@@UAA@XZ` destructor thunk, not a member of either class in
this TU; it is already pairing by funclet byte-signature (`masked_equal`), so
it is disclosure-class, not source work.

**`GetSetlistType` itself (`fn_82B7A078`) was not named.** It sits at
`0x82B7A078`, **below** this unit's pinned `.text` (which starts `0x82B7A1A8`)
and inside the span attributed to `GemManager.s`. Naming it would require
re-homing a pin, and **re-homing an already-pinned address is not
metric-neutral** (measured elsewhere at +3 functions / +428 B) — a different
lever with a different risk profile, and out of scope for this lane. It costs
nothing to leave anonymous: placeholder forgiveness already makes all three call
sites score equal.

**No permuter run.** Every residual here was a source-level structural defect;
nothing in this lane was regalloc- or scheduling-bound.

## Correction to the brief

The brief stated the unit's other sub-100 rows were "three ANONYMOUS rows". The
measured count at base `3c6eacd1` was **seven** anonymous rows at fuzzy 0
(`fn_82B7B550` 428, `fn_82B7A3D8` 300, `fn_82B7A558` 276, `fn_82B7BCB0` 196,
`fn_82B7A7D0` 192, `fn_82B7A8E0` 192, `fn_82B7A288` 104) plus `fn_82B7A260` at
99.5. Three of those seven are now closed. Testing the briefed figure literally
is what surfaced the 380 B of increment 3.
