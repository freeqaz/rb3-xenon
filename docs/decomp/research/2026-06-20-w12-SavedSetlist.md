# W12 — SavedSetlist: locate + port + wire + pin the Campaign↔LockStepMgr gap

**Date:** 2026-06-20 (wave-12)
**Mode:** DISCOVER/PLANNER (Opus), READ-ONLY in main.
**Baseline:** main @ d2d3e53, **9301 matched** (fixed for all agents).
**TU owned:** `band3/meta_band/SavedSetlist.cpp`
**Verdict:** **REAL_ACTIONABLE.** Identity is string-fingerprint + ICF-alias +
caller-triangulated to near-certainty; the `.cpp` is unported (header already
present, byte-identical to oracle), every dependency is in-tree, the pin is fully
bounded against BOTH neighbours with zero overlap. Self-contained, independently
landable vs main@9301. **~+12.**

This TU is directly coupled to the just-landed MusicLibraryNetSetlists (its
`.cpp` constructs `NetSavedSetlist`/`BattleSavedSetlist` and a SavedSetlist fn at
0x825921F0 is called from inside the MLNS pin at 0x825B71A0).

---

## The gap

```
Campaign.cpp      .text 0x8258C3D8–0x82590C70   .pdata 0x822193C0–0x822198D0   (lower neighbour)
[GAP]             .text 0x82590C70–0x82592270   .pdata auto-derived            ← SavedSetlist
LockStepMgr.cpp   .text 0x82592270–…            (UNPINNED, unwired)            (upper neighbour)
```

(splits.txt: Campaign L2527.) The gap is **0x1600 bytes / 65 functions**, all
anonymous `fn_` (retail stripped names). It sits in the `band3/meta_band`
spatial cluster. The MiniGameMgr split at 0x82595540 is the *next pinned* unit;
between SavedSetlist's end (0x82592270) and MiniGameMgr lies the unrelated
**LockStepMgr.cpp** TU (0x82592270–~0x82595540, 136 fns, `releasing_lock_step` /
`BasicStartLockMsg` / `EndLockMsg` / `LockResponseMsg` strings — a separate TU,
out of scope; oracle `band3/meta_band/LockStepMgr.{cpp,h}`).

---

## Identity ground-truth (convergent, near-certain)

The standalone-auto-blob string-xref path is dead (PPC `lis/addi` split-immediate
fields are reloc-zeroed in the auto OBJ, so Ghidra reports 0 xrefs to every
setlist string and the asm has no string-label loads — the same known limitation
documented in the MusicLibraryNetSetlists dossier). Identity here rests on the
**fingerprints.json per-function indexed strings** (which survive for 4164 fns),
the **ICF-alias map entry**, and **cross-TU caller triangulation**:

1. **Per-function string fingerprint (decisive).** In `fingerprints.json`:
   - `fn_82590C70` references `battle_friend`, `battle_friend_archived`,
     `setlist_local` → **`SavedSetlist::SetlistTypeToSym`** (the `switch(ty)`
     returning the 8 setlist enum symbols; oracle `.cpp` L20–42). 132 insns,
     calls one symbol-interning helper. This is the *first* fn in the gap and
     abuts Campaign's end.
   - `fn_82591090` references `hmx_%s` → **`NetSavedSetlist::GetIdentifyingToken`**
     (`MakeString("hmx_%s", GetTitle())`; oracle L161–182). The sibling format
     strings `fnd_%s` (0x820a76a0), `hmx_%s` (0x820a76a8), `btl_%i` (0x820a76b0)
     all exist contiguous in retail rdata.
   - `fn_825912A8` references `set_description`, `set_shared` → **the
     `BEGIN_HANDLERS(LocalSavedSetlist)` block** (`HANDLE_ACTION(set_description…)`
     / `HANDLE_ACTION(set_shared…)`; oracle L147–159). 1056 bytes / 262 insns =
     the big `Handle` dispatcher.
   - Across **all of `../rb3/src`**, the format strings `fnd_%s`/`hmx_%s`/`btl_%i`
     and the `setlist_*`/`battle_*` enum-symbol set co-occur in **exactly one
     file**: `band3/meta_band/SavedSetlist.cpp`. No other TU owns this gap.

2. **ICF-alias map entry confirms a SavedSetlist body inside the span.**
   `scripts/target_symbol_map.json` already has ONE entry in the gap:
   `0x82592060 → ?AdvanceSong@MetaPerformer@@UAAXH@Z`. Ghidra decompile of
   0x82592060: `Function_8257F910(this + 0x10, &arg)` — i.e. push an int onto the
   member at **this+0x10**, which is `std::vector<int> mSongs; // 0x10`. This is
   **`SavedSetlist::AddSong(int id)` → `mSongs.push_back(id)`** (oracle L55–58).
   It ICF-folded with `MetaPerformer::AdvanceSong` (identical codegen: call a
   member fn on this+0x10 with one int arg), and the linker picked the
   MetaPerformer name as primary. This is the textbook Waypoint-audit pattern:
   an **ICF address-alias wearing a foreign name**, not a foreign-TU intrusion.
   MetaPerformer is unpinned and its 4 map symbols are scattered binary-wide
   (0x825245A0 / 0x82527C50 / 0x82592060 / 0x82BE39D0) = pure ICF aliases, not a
   contiguous TU. **Preserve this entry, never re-key it (lesson #4).**

3. **Cross-TU caller triangulation (independent corroboration).** `fn_825921F0`
   (near the gap tail) is called from **0x825B71A0**, which is inside the
   just-landed MusicLibraryNetSetlists pin (0x825B6808–0x825B7C0C). MLNS's
   `ParseDataResultsIntoSetlists` constructs `NetSavedSetlist`/`BattleSavedSetlist`
   — so a SavedSetlist ctor/dtor being called from inside MLNS is exactly
   expected, and it pins the gap-tail to the SavedSetlist family.

4. **Oracle exists, DC3 does not.** `../rb3/src/band3/meta_band/SavedSetlist.{cpp,h}`
   present; DC3 has **no** such file (RB3-specific game code — rb3-Wii is the
   authoritative oracle, DC3 a false friend here). Our
   `src/band3/meta_band/SavedSetlist.h` **already exists and is byte-identical to
   the Wii oracle header** (diff = identical; pulled in by the MLNS port). Only
   the `.cpp` is missing + unwired.

---

## Boundary proof (both edges clean)

- **Lower edge 0x82590C70.** Campaign `.text` ends *exactly* at 0x82590C70
  (splits L2529). `fn_82590C70` = SetlistTypeToSym (setlist-enum strings). The
  fns immediately below (0x82590B…C3C) are Campaign-own (callees into
  0x82547ED8 / 0x827E2560, the Campaign callee cluster). Adjacent, no overlap.
- **Upper edge 0x82592270.** SavedSetlist's last fn is `fn_82592240` (40B,
  ends 0x82592268); 8-byte align pad to 0x82592270. `fn_82592270` references
  `releasing_lock_step` and begins the **LockStepMgr** TU (callee cluster
  0x8279B788 + Lock* messages, called from 0x82592C68 / 0x826066B8 / 0x82619398
  = LockStepMgr-internal + its UI consumers). LockStepMgr is **unpinned and
  unwired**, so 0x82592270 as `end` collides with nothing. Adjacent, no overlap.
- **Whole-binary self-check:** `[0x82590C70, 0x82592270)` overlaps **0** existing
  splits `.text` ranges (verified by parsing all stanzas).

---

## Pin (fully bounded, overlap-checked)

```
band3/meta_band/SavedSetlist.cpp:
	.text       start:0x82590C70 end:0x82592270
```

- Pin `.text` only; let dtk **auto-derive `.pdata`** (splits-bootstrap recipe;
  same as MLNS). The derived pdata will start at 0x822198D0 (= Campaign pdata
  end, adjacent below) and end before LockStepMgr's pdata. dtk back-fills the
  exact `.pdata` line on the next `touch config.yml && ninja`.
- **Mandatory pre-land self-check (wave-9 lesson #3):** after editing splits.txt,
  parse ALL `.text`+`.pdata` ranges and assert zero pairwise overlaps before
  building (the back-filled pdata must not overlap Campaign's or LockStepMgr's).

---

## Map entries — ADD only, preserve the 1 pre-existing (lesson #4)

`scripts/target_symbol_map.json` already has **1** entry in the gap that is
counted and must be preserved verbatim:

```
0x82592060  ?AdvanceSong@MetaPerformer@@UAAXH@Z   ← ICF alias = SavedSetlist::AddSong
```

Generate the remaining ~64 entries with `tools/gen_game_target_map.py` against
the rb3-Wii oracle (SavedSetlist is a high-purity game TU). **ADD** keys only;
skip 0x82592060; never re-serialize the map wholesale (lesson #4 poison rule).

---

## Self-contained port plan (ONE worktree, independently landable vs main@9301)

1. **Add the source.** Copy `../rb3/src/band3/meta_band/SavedSetlist.cpp` →
   `src/band3/meta_band/SavedSetlist.cpp`. The header is already present and
   identical — do NOT touch it.
2. **One additive shared-header edit (FLAGGED — see flag_foundational).** The
   oracle `SaveFixed`/`LoadFixed` call `FixedSizeSaveable::SaveStd(fs, mSongs,
   100, 4)` / `LoadStd(fs, mSongs, 100, 4)` where `mSongs` is `std::vector<int>`.
   Our `src/system/meta/FixedSizeSaveable.h` declares only the **3-arg
   non-template** `SaveStd(stream, vector<Symbol>&, int)` family; it is **missing
   the 4-arg templated overloads** the oracle header carries. Port the two
   template members from oracle `FixedSizeSaveable.h` (L83–125 in the oracle):
   ```cpp
   template <class T, class Allocator>
   static void SaveStd(FixedSizeSaveableStream &stream,
       const std::vector<T, Allocator> &vec, int maxsize, int savesize);
   template <class T, class Allocator>
   static void LoadStd(FixedSizeSaveableStream &stream,
       std::vector<T, Allocator> &vec, int maxsize, int savesize);
   ```
   (header-only static template bodies; they stream each element + pad/depad).
   - **Layout-neutral and codegen-neutral for all other TUs:** these are
     *uninstantiated* static templates — they emit nothing unless called, so the
     23 TUs that include this header (incl. pinned SongStatusMgr / AccomplishmentProgress
     / FixedSizeSaveable[Stream]) cannot have their codegen perturbed. But it is
     a **shared header included by pinned units**, so it is FLAGGED for the
     coordinator to gate on the WHOLE-BINARY composed A/B (lesson #1), not the
     file touched. Keystone-schedulable but trivially additive.
   - The oracle also has `SaveStd`/`LoadStd` overloads for `map<Symbol,T>`,
     `map<T1,T2>`, and `SaveStdPtr`/`LoadStdPtr` — port only what SavedSetlist
     needs (the 2 `vector<T,Allocator>` overloads). Adding the rest is harmless
     but unnecessary; keep the diff minimal.
3. **Wire** in `config/45410914/objects.json` (alongside the meta_band block):
   `"band3/meta_band/SavedSetlist.cpp": "NonMatching",`
4. **Pin** the splits stanza above into `config/45410914/splits.txt` (full-path
   key form, matching the wave-10/11 convention). Run the overlap self-check.
5. **Map** — run `tools/gen_game_target_map.py` for this TU and ADD its emitted
   keys, **skipping the pre-existing 0x82592060**.
6. **Port MWCC→MSVC X360.** The oracle `.cpp` (201 lines) is clean and already
   uses the project's macros — all verified present in-tree:
   - `BEGIN_HANDLERS/HANDLE_EXPR/HANDLE_ACTION/HANDLE_MESSAGE/HANDLE_SUPERCLASS/`
     `HANDLE_CHECK` (obj/ObjMacros.h), `FOREACH` (utl/Std.h), `MakeString`,
     `MILO_ASSERT/MILO_ASSERT_RANGE/MILO_FAIL` (Debug.h — message strings
     gated/stripped to match retail, line numbers preserved), `REPORT_SIZE`
     (FixedSizeSaveable.h).
   - All called types/methods verified in-tree: `TheSongMgr.Data` +
     `BandSongMetadata::LengthMs` (BandSongMgr.h / BandSongMetadata.h),
     `HxGuid::{Generate,ToString,SaveSize,empty,c_str}` + `<<`/`>>` BinStream ops
     (HxGuid.h), `DateTime::{ToCode, DateTime(unsigned)}` (DateTime.h),
     `PatchDescriptor{patchType,patchIndex}` + `<<`/`>>` (PatchDir.h),
     `BandProfile::{GetTexAtPatchIndex,GetName}` (BandProfile.h),
     `FixedSizeSaveable::{SaveFixedString,LoadFixedString}` + the new 4-arg
     `SaveStd/LoadStd` (step 2), `TheUIEventMgr->TriggerEvent` (UIEventMgr.h),
     `TourSavable::UploadComplete`/`SecBetweenUploads` (TourSavable.h),
     `RockCentralOpCompleteMsg::{Arg1,Success}` (RockCentralMsgs.h),
     `error_setlist_title_profane`/`error_setlist_description_profane`/
     `error_message` (Symbols2.h L1384–1389). Includes resolve via `/I src/band3`
     + `/I src/system`.
7. **Convert + reveal + A/B.** Build with the VERIFY COMMAND
   (`rm -f build/45410914/target_symbol_renames.stamp && touch
   config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`), read
   `measures.matched_functions`, re-run once for the splits-only FP. Iterate
   per-fn body-port from the oracle until the framed/real-bodied fns reach
   normalized 100%.

### Expected delta

**~+12** honest. Rationale: 65 emitted fns, but 1 already counted (the AddSong
ICF alias). The TU is a **5-class hierarchy** (SavedSetlist abstract base +
InternalSavedSetlist + NetSavedSetlist + BattleSavedSetlist + LocalSavedSetlist
with triple inheritance SavedSetlist/TourSavable/FixedSizeSaveable), so ~44 of
the 65 are small (≤44B): vtable thunks, scalar-deleting dtors, dtor funclets, and
trivial virtual forwarders (SetTitle/SetDescription/SetDateTime/AddSong/SetSongs
×classes), plus STL `vector<int>`/`vector<String>`/`vector<Symbol>` template
instantiations. Many trivial accessors + the STL instantiations byte-match on
reveal (the core +12). The large real bodies — SetlistTypeToSym (switch),
LocalSavedSetlist::Handle (1056B dispatcher), SaveFixed/LoadFixed (the
240B/276B pair), NetSavedSetlist::GetIdentifyingToken (switch + MakeString) —
may land at near-miss (regalloc/funclet-class) on the first pass. Honest ceiling
if the trivial-virtual fan-out + STL all match: ~+20. Land whatever is
net-positive; defer regalloc/funclet near-misses with root cause.

---

## Honesty gate notes for the lander

- whole-binary A/B vs main@9301; net must be ≥ +1 and equal the sum of intended
  SavedSetlist gains (and absorb the FixedSizeSaveable.h additive edge — verify
  net across the binary, NOT just the file touched, per the flag).
- No ≥8-contiguous FOREIGN `fn_@0%` run inside the pin — its own STL/funclets/
  trivial virtuals bracketed by its own named fns are OK. The only foreign-NAMED
  entry (AdvanceSong @0x82592060) is an ICF alias to SavedSetlist::AddSong, not a
  foreign body; it is already 100%/counted. There is no other TU owner in the
  span (Campaign below, LockStepMgr above, both excluded), so foreign-run risk is
  nil.
- Include-graph blast radius: `SavedSetlist.h` is unchanged (already in-tree,
  identical). The only shared-header touch is the additive FixedSizeSaveable.h
  template overloads (step 2) — uninstantiated-template = zero sibling-TU codegen
  shift, but FLAGGED for binary-wide A/B because the header is included by pinned
  units.

---

## flag_foundational

**true** (soft). The port itself is fully self-contained and independently
landable, BUT it requires one additive shared-header edit to
`src/system/meta/FixedSizeSaveable.h` (two `vector<T,Allocator>` 4-arg
`SaveStd`/`LoadStd` template overloads). It is purely additive (uninstantiated
static templates = layout- and codegen-neutral for all 23 including TUs) and
cannot perturb any other unit's match%, so it is safe to bundle into this single
work-item — but because the header is included by *pinned* units, the lander MUST
gate on the WHOLE-BINARY composed A/B (lesson #1), and the coordinator should be
aware it is a (trivial) shared-header lever, schedulable keystone-first if a
later wave wants to batch other FixedSizeSaveable.h template additions. No
keystone semantics, no member/struct/vtable layout change.
