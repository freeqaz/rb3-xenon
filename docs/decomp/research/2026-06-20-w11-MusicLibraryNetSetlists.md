# W11 — MusicLibraryNetSetlists: identify + port + wire + pin the head gap below SongStatusMgr

**Date:** 2026-06-20 (wave-11)
**Mode:** DISCOVER/PLANNER (Opus), READ-ONLY in main.
**Baseline:** main @ 053d2d0, **9159 matched** (fixed for all agents).
**TU owned:** `band3/meta_band/MusicLibraryNetSetlists.cpp`
**Verdict:** **REAL_ACTIONABLE.** Identity is COFF+string+pdata-triangulated to
near-certainty; the .cpp is unported (header already present), every dependency
is in-tree and wired, the pin is fully bounded against BOTH neighbours with zero
overlap. Self-contained, independently landable vs main@9159. **~+15.**

---

## The gap

```
Pose.cpp            .text 0x825B67B0–0x825B6804   .pdata 0x8221C2D8–0x8221C2E0   (lower neighbour)
[GAP]               .text 0x825B6808–0x825B7C0C   .pdata 0x8221C2E0–0x8221C428   ← MusicLibraryNetSetlists
SongStatusMgr.cpp   .text 0x825B7C0C–0x825BB090   .pdata 0x8221C428–0x8221C740   (upper neighbour)
```

(splits.txt: Pose L2269, SongStatusMgr L1358.) The gap is **0x1404 bytes / 41
functions**, all anonymous `fn_` (retail stripped names). It sits in the
`band3/meta_band` spatial cluster (Pose is band3/world, but the meta_band belt —
SongStatusMgr, SongRecord, MusicLibrary, AccomplishmentManager — packs here).

---

## Identity ground-truth (convergent, near-certain)

1. **Unique-string fingerprint (decisive).** `ParseDataResultsIntoSetlists`
   builds `MakeString("s_id%03i", i)` and `MakeString("s_name%03i", i)`
   (oracle `.cpp` L208/L210). Both literals exist in retail rdata
   (`auto_00_82000400_rdata.obj`, Ghidra string index VA **0x820b0be8** =
   `s_id%03i`, `0x820b0bf4` = `s_name%03i`). Across **all of `../rb3/src`** these
   two format strings appear in **exactly one file**:
   `band3/meta_band/MusicLibraryNetSetlists.cpp`. No other TU can own this gap.
   - (Ghidra reports 0 xrefs to the string VA — a known PPC `lis/addi`
     split-immediate auto-analysis miss; the auto blob's `lis`/`addi` immediate
     fields are reloc-zeroed, so a raw-byte immediate scan of the gap also finds
     0. This is the expected unrelocated-blob behaviour, not counter-evidence.)
   - The MILO_ASSERT / MILO_FAIL message strings ("Bad setlist type from
     RockCentral", "No setlist art matching id sym", "Bad SetlistType %i …") are
     **absent** from retail rdata — consistent with the project-wide retail
     debug-string stripping (`project_debug_output_stripping`). The surviving
     `s_id`/`s_name` are real `MakeString` args, not debug, so they remain.

2. **Spatial + structural fit.** 41 text functions, 41 pdata entries (one per
   frame fn; the two leaf accessors at gap-start 0x825B6808/0x825B6840 carry no
   unwind, so 39 framed + ... = the pdata set begins at fn 0x825B6878). The
   source TU is a `Hmx::Object` subclass with ctor/dtor/`Handle`/`OnMsg` +
   `BEGIN_HANDLERS` block + heavy STL template expansion
   (`std::vector<NetSavedSetlist*>` ×2, `std::list<SetlistArtRecord>`,
   `DataResultList` iteration) — exactly the function-multiplier shape that
   expands ~14 source methods into ~41 emitted functions (thunks, dtor funclets,
   STL `erase`/`_M_*` instantiations).

3. **Oracle exists, DC3 does not.** `../rb3/src/band3/meta_band/MusicLibraryNetSetlists.{cpp,h}`
   present; DC3 has **no** such file (RB3-specific game code — rb3-Wii is the
   authoritative oracle, DC3 a false friend here). Our
   `src/band3/meta_band/MusicLibraryNetSetlists.h` **already exists and is
   byte-identical to the Wii oracle header** (diff = identical). Only the `.cpp`
   is missing + unwired.

---

## Pin (fully bounded, overlap-checked)

```
band3/meta_band/MusicLibraryNetSetlists.cpp:
	.pdata      start:0x8221C2E0 end:0x8221C428
	.text       start:0x825B6808 end:0x825B7C0C
```

- **text start 0x825B6808** = first MusicLibraryNetSetlists instruction
  (`lwz r11,0x6c(r3)`). 0x825B6804 is a 4-byte zero pad after Pose's end
  (Pose .text end = 0x825B6804). Adjacent, no overlap below.
- **text end 0x825B7C0C** = SongStatusMgr's first fn `fn_825B7C0C` (COFF
  confirms next fns 0x825B7C0C, 0x825B7C38, 0x825B7C64). Adjacent, no overlap
  above.
- **pdata [0x8221C2E0, 0x8221C428)** derived from auto_01 pdata: the 41 in-gap
  functions' pdata BeginAddress relocations land exactly in this span
  (first slot 0x8221C2E0→fn_825B6878 … last 0x8221C420→fn_825B7BE0, +8 =
  0x8221C428). Pose pdata ends 0x8221C2E0 (adjacent below); SongStatusMgr pdata
  starts 0x8221C428 (adjacent above). **Zero overlap on either side, both
  sections.** You may also omit the explicit `.pdata` line and let dtk
  auto-derive it from the text span (it will produce this exact range).
- **Mandatory pre-land self-check (wave-9 lesson #3):** after editing splits.txt,
  parse ALL `.text`+`.pdata` ranges and assert zero pairwise overlaps before
  building.

---

## Map entries — ADD only, do NOT clobber 4 pre-existing (lesson #4 / deferred-ports point d)

`scripts/target_symbol_map.json` already has **4** entries inside the gap that
were counted by the keystone reveal cascade — **preserve them, never re-key**:

```
0x825B6878  ?erase@?$list@U?$pair@VSymbol@@V1@@...   ← ICF-merged STL list::erase
0x825B7978  __unwind$110593                          ← funclet
0x825B79C8  __unwind$110657                          ← funclet
0x825B7A90  __unwind$110606                          ← funclet
```

The `0x825B6878` ICF entry (`list<pair<Symbol,Symbol>>::erase`) is genuinely
**this TU's** `mSetlists.erase` (FinishGettingSetlistArt L107): `SetlistArtRecord`
= `{Symbol unk0; RndTex* unk4}` has identical layout/codegen to
`pair<Symbol,Symbol>`, so the linker ICF-folded them under one symbol name. It's
already matched and counted; do not regenerate it.

Generate the remaining ~37 entries with `tools/gen_game_target_map.py` against
the rb3-Wii oracle (MusicLibraryNetSetlists is a high-purity game TU). **ADD**
keys only; never re-serialize the map wholesale (lesson #4 poison rule).

---

## Self-contained port plan (ONE worktree, independently landable vs main@9159)

1. **Add the source.** Copy `../rb3/src/band3/meta_band/MusicLibraryNetSetlists.cpp`
   → `src/band3/meta_band/MusicLibraryNetSetlists.cpp`. The header is already
   present and identical — do NOT touch it.
2. **One additive engine-header decl.** `SwapDxtEndianness(RndBitmap*)` (called
   in `Poll()`) is declared in the oracle's `rndobj/Utl.h` but absent from ours
   and from DC3. Add the single line
   `void SwapDxtEndianness(RndBitmap *bmap);` to `src/system/rndobj/Utl.h`
   (matching oracle Utl.h L57). **This is purely additive — a free-function
   *declaration*, no definition, no struct/layout/member change** — so it is
   layout-neutral and cannot perturb any other TU's codegen.
   - Body lives in rndobj/Utl.cpp in the oracle; on big-endian Xbox the swap is
     a 16-bit byteswap loop over DXT pixels. If Utl.cpp isn't compiled in our
     build, the call resolves cross-TU at link via the auto blob (the def is
     already in the binary). Confirm at compile: a decl is enough to compile
     this TU; the renamer/objdiff pair compares only MusicLibraryNetSetlists'
     own emitted code. (If a definition is needed for the native build, that's
     a separate concern — this is a decomp-matching pin.) **NOT foundational**:
     no shared keystone semantics, no header layout.
3. **Wire** in `config/45410914/objects.json` (alongside the meta_band block,
   e.g. near L708 SongStatusMgr):
   `"band3/meta_band/MusicLibraryNetSetlists.cpp": "NonMatching",`
4. **Pin** the splits stanza above into `config/45410914/splits.txt` (full-path
   key form, matching the wave-10 StoreMenuPanel/VoiceoverPanel convention).
5. **Map** — run `tools/gen_game_target_map.py` for this TU and ADD its emitted
   keys to `scripts/target_symbol_map.json`, **skipping the 4 pre-existing
   in-gap keys** (0x825B6878, 0x825B7978, 0x825B79C8, 0x825B7A90).
6. **Port MWCC→MSVC X360.** The oracle .cpp is clean and already uses the
   project's macros — all verified present in-tree:
   - `BEGIN_HANDLERS/HANDLE_MESSAGE/HANDLE_SUPERCLASS/HANDLE_CHECK`
     (obj/ObjMacros.h), `FOREACH` + `DeleteAll` (utl/Std.h), `RELEASE`,
     `MakeString`, `MILO_ASSERT/MILO_LOG/MILO_FAIL` (Debug.h — message strings
     gated/stripped to match retail, line numbers preserved).
   - The `#define kArchivedHarmonix kBattleHarmonixArchived` alias around
     `RefreshSetlistArt` (oracle L232/L285) is REQUIRED — keep it verbatim so
     the assert resolves to the renamed enum (our SavedSetlist.h has
     `kBattleHarmonixArchived = 6`, no `kArchivedHarmonix`).
   - All called types/methods verified in-tree:
     `MusicLibrary::{RebuildAndSortSetlists,GetHighlightedNode,SetlistArtFinished}`,
     `RockCentral::{GetAllSonglists,GetSetlistArt,GetBattleArt,CancelOutstandingCalls}`,
     `ProfileMgr::GetSignedInProfiles`, `NetCacheMgr::{AddNetCacheLoader,DeleteNetCacheLoader}`,
     `NetSavedSetlist`/`BattleSavedSetlist` ctors + enum values (SavedSetlist.h,
     already byte-faithful), `DataResultList`, `RockCentralOpCompleteMsg`
     (RockCentralMsgs.h). Includes resolve via `/I src/band3` (`net_band/...`)
     + `/I src/system`.
7. **Convert + reveal + A/B.** Build with the VERIFY COMMAND
   (`rm -f build/45410914/target_symbol_renames.stamp && touch
   config/45410914/config.yml && NINJA_JOBS=8 tools/fresh_report.sh`), read
   `measures.matched_functions`, re-run once for the splits-only FP. Iterate
   per-fn body-port from the oracle until the framed/real-bodied fns reach
   normalized 100%.

### Expected delta

**~+15** honest. Rationale: 41 emitted fns, but ~4 already counted (the ICF
erase + 3 funclets), a chunk are STL `_M_*`/dtor funclets that pair only with
matching codegen, and several real bodies (`ParseDataResultsIntoSetlists`,
`RefreshSetlistArt`) are large switch/dynamic_cast functions that may sit at
near-miss (regalloc/funclet-class) on the first pass. The clean accessors
(ctor, IsSetlistArtReady, GetSetlistArt, CleanUpArt, Poll, the trivial
Refresh* forwarders) + the STL instantiations that byte-match on reveal are the
core +15. Honest ceiling if the big switch bodies also match: ~+25. Land
whatever is net-positive; defer regalloc/funclet near-misses with root cause.

---

## Honesty gate notes for the lander

- whole-binary A/B vs main@9159; net must be ≥ +1 and equal the sum of intended
  MusicLibraryNetSetlists gains.
- No ≥8-contiguous FOREIGN `fn_@0%` run inside the pin — its own STL/funclets
  bracketed by its own named fns are OK (the 41 fns are all this one TU; there
  is no other owner in the span, so foreign-run risk is nil).
- Include-graph blast radius: `MusicLibraryNetSetlists.h` is included by
  MusicLibrary.cpp/SongRecord and a couple panels, but all consume it via
  pointer/method calls (no inlined member-offset access from a sibling), and the
  header is unchanged from main — so no sibling-TU codegen shift. The only
  shared-header touch is the additive Utl.h decl (layout-neutral, step 2).

---

## flag_foundational

**false.** No keystone / binary-wide lever. The one shared-header edit
(`SwapDxtEndianness` decl in rndobj/Utl.h) is a purely additive free-function
declaration with zero layout/semantic ripple, bundled into this single
self-contained work-item. The Handle/MILO_MESSAGE_TIMERS keystone is already
landed and is NOT touched.
