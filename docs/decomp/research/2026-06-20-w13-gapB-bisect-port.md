# W13 — GAP B bisect-port (the REST of Gap B above/around Award)

**Date:** 2026-06-20  •  **Mode:** DISCOVER/PLANNER (read-only in main @ c00664c, 9404 matched)
**TU owned:** `gapB-bisect-port` — bisect the rest of Gap B `[0x825C3A44, 0x825D0EF0)`
that W12's Award.cpp pin (`[0x825D0380, 0x825D0D00)`) did NOT carve.

**Verdict: REAL_ACTIONABLE.** The remaining Gap B is dominated by the
`Accomplishment*Conditional` family — 13 sub-TUs, address-ordered, each with a
unique string + sequential vtable anchor. **KEY DISCOVERY that supersedes the W12
"fresh port" framing:** every one of these TUs is **ALREADY WIRED in objects.json
AND COMPILES CLEANLY** (all `.obj` present). They are NOT pinned to their real
`.text` clusters — 5 are pinned to **dead ICF slivers** (matched=0) at
`0x8243Exxx`/`0x8264Cxxx`, the rest are wired-but-unpinned. So the lever here is
**relocate/pin the real cluster** (splits-only or splits-mostly), NOT a fresh
MWCC→MSVC port. This is far lower-risk and higher-confidence than W12's Award port.

Chosen actionable first sub-TU = **`AccomplishmentSetlist.cpp`** (relocate the dead
sliver pin to its real 12-fn cluster). Rest emitted as `discovered_frontier`.

---

## Ground-truth state (main @ c00664c, 9404)

Current `.text` pins in/around Gap B `[0x825C3A44, 0x825D0EF0)`:
- OvershellSlot `[0x825C10D8, 0x825C3A44)` — ends exactly at Gap B start.
- **Award.cpp `[0x825D0380, 0x825D0D00)`** — W12 carve (landed).
- FixedSizeSaveable `[0x825D0EF0, 0x825D0F34)` — Gap B end (hard boundary).

So Gap B is OPEN except Award. The unpinned remainder == the dtk pre-split blob
`build/45410914/asm/auto_03_825C3A44_text.s` (covers `[0x825C3A44, 0x825D0380)`,
534 `.s` fns), plus the tiny `[0x825D0D00, 0x825D0EF0)` tail above Award.

### Two regions (string-anchored, from `fingerprints.json` resolved strings)

**Region B1 — front `[0x825C3A44, 0x825CB590)`** (OvershellSlot-coupled): strings
`join`/`waiting`/`swap_user`/`p_providers`/`ng/startup_autosave_esrb_keep.milo`.
Large (~30 KB), messy, OvershellSlot-extension-vs-fresh-provider ambiguous. **DEFER**
(same disposition as W12; frontier item, needs an extension decision).

**Region B2 — tail `[0x825CB590, 0x825D0380)`** ★ the rich, bisectable conditional
seam. 202 indexed fns, 13 address-ordered sub-TUs. Every owner already wired +
compiling.

### The conditional-family map (string + vtable + map-entry corroborated)

| TU (rb3-xenon, WIRED+compiles) | real cluster span | fns | unit matched now | pin state |
|---|---|---|---|---|
| SongList-base/`filter` transition | `[0x825cb590,0x825cbc58)` | 17 | — | unpinned (ambiguous) |
| **AccomplishmentSetlist** ★PORT TARGET | `[0x825cbc58,0x825cc010)` | 12 | **0/8** | **dead sliver @0x8243F220** |
| AccomplishmentOneShot | `[0x825cc010,0x825cc220)` | 7 | — | unpinned |
| AccomplishmentSongConditional | `[0x825cc220,0x825ccbe0)` | 24 | 3/3 | dead sliver @0x8264CEF0 |
| AccomplishmentPlayerConditional | `[0x825ccbe0,0x825ce5a8)` | 58 | 0/5 | dead sliver @0x8243F178 |
| AccomplishmentTrainerConditional | `[0x825ce5a8,0x825ce788)` | 7 | — | unpinned |
| AccomplishmentTrainerCategoryConditional | `[0x825ce788,0x825ced70)` | 16 | — | unpinned |
| AccomplishmentTrainerListConditional | `[0x825ced70,0x825cf0f0)` | 13 | — | unpinned |
| AccomplishmentSongListConditional | `[0x825cf0f0,0x825cf390)` | 6 | — | unpinned (1 map entry @0x825CEE08 is elsewhere) |
| AccomplishmentSongFilterConditional | `[0x825cf390,0x825cf8f8)` | 11 | 0/5 | dead sliver @0x8243F378 |
| AccomplishmentDiscSongConditional | `[0x825cf8f8,0x825cfc58)` | 10 | — | unpinned |
| AccomplishmentLessonSongListConditional | `[0x825cfc58,0x825cfdc0)` | 4 | — | unpinned |
| AccomplishmentLessonDiscSongConditional | `[0x825cfdc0,0x825d0380)` | 19 | — | unpinned |

**The displaced-sliver structure (decoded):** MSVC split each conditional's TRIVIAL
folded virtuals (`GetType`/`CanBeLaunched`/`GetIndex`/`GetGroup`…) into the shared
ICF region `0x8243Exxx`, while the SUBSTANTIAL methods (ctor/Configure/dtor/
CheckRequirements/InqRequiredScoreTypes/IsRelevantForSong) stayed in the
address-ordered TU cluster in Gap B. The slivers read **matched=0** because our
compiled obj emits each class's own copy of those trivials, which don't byte-match
the ICF-folded representatives. So: **relocating the pin sliver→cluster loses nothing
(sliver already 0) and gains the substantial methods.** This is the
`requires_sliver_eviction` vein from MEMORY ("evict when this-unit-IS-the-sliver").

---

## ★ ACTIONABLE: AccomplishmentSetlist relocate-then-pin (self-contained, independently landable vs main@c00664c)

### Exact coords (bounded vs BOTH neighbours, overlap-verified)
`AccomplishmentSetlist.cpp:  .text  start:0x825CBC58  end:0x825CC010`  (0x3B8 = 952B)

- **Lower bound 0x825CBC58 (confident):** first fn after the `filter`-transition
  cluster. fn_825CBC58 loads `lbl_820B5560`/`lbl_820B5554` (= Setlist vtable/Symbol
  anchors) and references the `setlist`/`min_stars`/`difficulty` strings → Setlist
  ctor/Configure. Preceded by `except_data_825CBC58` (8B) and fn_825CBC24
  (`subi r31,r12,0x70` funclet of the lower-neighbour TU). CLEAN.
- **Upper bound 0x825CC010 (confident):** AccomplishmentOneShot's first fn,
  string-anchored by `oneshot_song`/`oneshot_playermin`. The Setlist cluster ends
  with `lbl_825CBFA8` (a fn dtk emitted as bare `.sym`, runs to 0x825CC008) +
  `except_data_825CC010` (8B). CLEAN.
- **Overlap self-check: PASS.** Verified `[0x825CBC58, 0x825CC010)` vs all 644
  existing `.text` pins → ZERO overlaps. Nearest below = OvershellSlot
  (ends 0x825C3A44); nearest above = Award (0x825D0380). ⚠ RE-RUN at land time
  (pin set will have shifted).

### Why relocate, not fresh port — GREEN
- `src/band3/meta_band/AccomplishmentSetlist.cpp` ALREADY in tree, byte-faithful to
  rb3-Wii oracle (`../rb3/src/band3/meta_band/AccomplishmentSetlist.cpp` 1855B),
  WIRED `"AccomplishmentSetlist.cpp": "NonMatching"` in objects.json, and **compiles**
  (`build/45410914/src/band3/meta_band/AccomplishmentSetlist.obj` 37580B present).
- The obj defines all expected named methods: `??0` ctor, `??1`/`??_G`/`??_E` dtor
  variants, `?Configure`, `?GetType`, `?CanBeLaunched`, `?HasSpecificSongsToLaunch`,
  `?GetRequiredDifficulty`, `?CheckRequirements`, `?InqRequiredScoreTypes`, `??_7`
  vtable, `??_R*` RTTI. Real-logic, not instrumentation stubs.
- It is currently pinned to the **dead sliver** `[0x8243F220, 0x8243F330)`
  (pdata `[0x82205EF0, 0x82205F30)`), unit matched **0/8** → relocating costs nothing.
- The 4 trivial folded virtuals (`GetType`/`CanBeLaunched`/`HasSpecificSongsToLaunch`/
  `GetRequiredDifficulty`) live in the sliver region and become UNPINNED after the
  move — they were 0 anyway, so no regression. The cluster owns ctor/Configure/dtor/
  CheckRequirements/InqRequiredScoreTypes.

### Self-contained plan (one worktree)
1. `scripts/setup_worktree.sh /tmp/w13-setlist setlist-relocate` (CoW buildable;
   re-run `configure.py` with explicit absolute `--dtk/--objdiff/--wrapper` per the
   worktree-dtk-trap note).
2. **Relocate the pin in `splits.txt`:** change the AccomplishmentSetlist.cpp
   `.text` entry from `start:0x8243F220 end:0x8243F330` to
   `start:0x825CBC58 end:0x825CC010`. **Delete the stale `.pdata` line** (`start:0x82205EF0
   end:0x82205F30`) — dtk auto-derives the new `.pdata` from the `.text` pin on
   resplit (cluster fns fn_825CBE70/fn_825CBF00/fn_825CBF58 have unwind entries in
   `auto_01_8221CE28_pdata.s`; leaf/funclet fns have none — normal). **Never hand-pin
   pdata, never gap-shrink.**
3. **Map entries (ADD only — NEVER regenerate, poison rule):** the 4 trivial-virtual
   map entries currently at 0x8243F220/0x8243F268/0x8243F288/0x8243F2A8 point into the
   sliver; they will no longer be in a pinned range and can stay (harmless) OR be
   re-pointed to the cluster's virtuals if those compile into the cluster (verify at
   land time). **ADD** map entries pairing the cluster `fn_<addr>` to the obj's
   substantial symbols: `??0AccomplishmentSetlist`, `?Configure@AccomplishmentSetlist`,
   `?CheckRequirements@AccomplishmentSetlist`, `?InqRequiredScoreTypes@AccomplishmentSetlist`,
   `??1`/`??_G`/`??_E` dtor variants. Seed by size+callgraph+string-xref (fn_825CBC58
   = Configure via setlist/min_stars; fn_825CBF00 +88 = InqRequiredScoreTypes via the
   std::set insert). Use `tools/gen_game_target_map.py` from the rb3-Wii oracle (the
   same generator Award used) if available, else hand-add. Do NOT touch any ICF entry
   naming a different representative. Watch the `_comment`/`_comment*` non-hex keys in
   `target_symbol_map.json` — skip them when iterating.
4. `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml
   && NINJA_JOBS=8 tools/fresh_report.sh`; read `measures.matched_functions`; **re-run
   once** (splits-only FP warning is known).
5. Re-derive precise fn addrs from `asm/.../AccomplishmentSetlist.s` (`^.fn fn_<hex>`);
   `/batch-check`. Match ctor/Configure/dtor/CheckRequirements/InqRequiredScoreTypes;
   add `reveal` map entries for any byte-exact-but-0% fn (no body change). Defer
   regalloc/funclet near-misses.
6. **Honesty gate before declaring landable:** re-run overlap self-check (0 overlaps);
   net ≥ +1; AccomplishmentSetlist unit goes 0/8 → >0; no ≥8-contiguous FOREIGN fn@0%
   run (own STL/funclets bracketed by own named = OK); headline net == sum of Setlist
   unit gains (sliver was 0, so headline delta == cluster matches gained).

### Expected delta
~+6 to +10. 12 cluster fns; substantial methods ctor/Configure/InqRequiredScoreTypes/
CheckRequirements/dtor variants (~6-8) + their tail funclets/the `lbl_825CBFA8` fn.
Conservative floor +5. (Setlist is deliberately the SMALL clean first proof; the +20
items are PlayerConditional/SongConditional in the frontier.)

---

## flag_foundational
**false** — self-contained RB3 game-code splits-relocate; source already compiles;
no binary-wide / shared-header change.

## Next-wave queue (discovered_frontier, priority order)
After Setlist proves the relocate pattern, the SAME splits-relocate (zero or minimal
porting) applies to the rest of the family. Bigger TUs first for delta:
1. **AccomplishmentPlayerConditional** `[0x825ccbe0,0x825ce5a8)` 58 fns — relocate
   from dead sliver @0x8243F178 (0/5). Biggest single delta in the seam (~+20).
   Has 2-3 big Configure/Check methods + many funclets. ⚠ large/complex; verify
   substantial-fn pairing.
2. **AccomplishmentSongConditional** `[0x825cc220,0x825ccbe0)` 24 fns — relocate from
   sliver @0x8264CEF0 (currently 3/3; the 3 are trivials that go unpinned). Cluster
   has only 2 substantial fns (+1172, +600) + 22 funclets/Symbol-helpers → modest net;
   verify the 3 lost trivials are outweighed. Do AFTER the simpler ones.
3. **AccomplishmentSongFilterConditional** `[0x825cf390,0x825cf8f8)` 11 fns — relocate
   from dead sliver @0x8243F378 (0/5). `num_songs`/`filter` anchors.
4. **AccomplishmentCategory** `[0x825d0e50,0x825d0ef0)` (the 2-fn tail ABOVE Award,
   inside Gap B's top) — relocate from dead sliver @0x8243EF98. Tiny; combine with the
   Award-tail region `[0x825D0D00, 0x825D0E50)` (Award's own unpinned tail — Award is
   pinned only to 0xD00, not the 0xE50 W12 originally proposed; that 0xD00→0xE50 gap is
   an Award EXTENSION candidate worth a separate look).
5. Fresh-pin the unpinned-no-sliver conditionals (OneShot 7 / Trainer* 7+16+13 /
   SongListConditional 6 / DiscSong 10 / LessonSongList 4 / LessonDiscSong 19) — wired
   + compiling, just need a clean-bound `.text` pin + `gen_game_target_map` map entries.
6. **Region B1** `[0x825C3A44, 0x825CB590)` — OvershellSlot extension-vs-fresh-provider
   (`OvershellSlotState.cpp` 5282B / `SessionUsersProviders.cpp` candidates). Needs the
   extension-vs-fresh-TU decision (deferred, same as W12).
7. The `filter`-transition cluster `[0x825cb590,0x825cbc58)` 17 fns — ambiguous owner
   (SongList/DiscSong base or template helpers); needs Ghidra owner-confirm before pin.

Re-run `tools/pin_audit.py` after each landing — the slivers will be flagged
`requires_sliver_eviction` and the freshly-revealed clusters refill the bodyport pool.
