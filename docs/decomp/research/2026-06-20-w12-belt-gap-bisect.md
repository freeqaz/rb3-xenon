# W12 — meta_band belt-gap bisection (the two BIG un-bisected gaps)

**Date:** 2026-06-20  •  **Mode:** DISCOVER/PLANNER (read-only in main @ 9301)
**TU owned:** `belt-gap-bisect` — the two un-bisected meta_band belt gaps:
- **Gap A** `[0x825BDF28, 0x825C10D8)` (~111 fns) — between CriticalUserListener
  (ends 0x825BDF28) and OvershellSlot (starts 0x825C10D8).
- **Gap B** `[0x825C3A44, 0x825D0EF0)` (~1112 .s fns / ~572 dedup) — between
  OvershellSlot (ends 0x825C3A44) and FixedSizeSaveable (starts 0x825D0EF0).

**Verdict: REAL_ACTIONABLE** — Gap B's tail bisects cleanly into 14 single-TU
sub-clusters of the `Accomplishment*Conditional` / `Award` / `AccomplishmentCategory`
family. Chosen actionable first sub-cluster = **`Award.cpp`** (the only UNWIRED,
fresh-port, no-sliver-ambiguity TU; all deps resolve). expected_delta ~+18.
Rest emitted as `discovered_frontier`.

## Method

Per-fn boundaries parsed from the dtk pre-split asm blobs that are split EXACTLY
at the gap edges: `build/45410914/asm/auto_03_825BDF28_text.s` (covers Gap A +
OvershellSlot pin) and `build/45410914/asm/auto_03_825C3A44_text.s` (== Gap B
exactly). TU ownership from THREE corroborating signals:
1. **String anchors** (`fingerprints.json` resolved-string field) → unique
   format/asset/Symbol literals → `grep ../rb3/src/band3/meta_band` for the owner.
2. **vtable/rdata `lbl_820Bxxxx` progression** in the asm (each distinct vtable
   block = one class; ascending order = address-ordered TU sequence).
3. **Existing `scripts/target_symbol_map.json` entries** inside the range
   (CanBeLaunched@AccomplishmentDiscSongConditional@0x825CB550,
   InqIncrementalSymbols@AccomplishmentSongListConditional@0x825CEE08,
   HasAward@AccomplishmentCategory@0x825D0E50) as ground-truth fixed points.

DC3 is a FALSE FRIEND for these RB3 game panels — oracle is **rb3-Wii**
(`../rb3/src/band3/meta_band/`), authoritative.

---

## GAP B bisection (the 565-fn gap) — TWO regions

### Region B1: front `[0x825C3A44, ~0x825CB000)` — OvershellSlot-coupled overshell state

`p_providers` / `swap_user` / `join` / `waiting` strings ALL resolve UNIQUELY to
`OvershellSlot.cpp` (rb3-Wii). OvershellSlot is a **77 807-byte** source but only
pinned to **10 604 bytes** at `[0x825C10D8, 0x825C3A44)`. The OvershellSlot pin
ENDS exactly at the Gap-B start. Ghidra on 0x825C4698 (`join`/`waiting`) shows the
`Symbol("join")`/`Symbol("waiting")` lazy-init pattern + overshell state walk →
consistent with OvershellSlot's continuation. The first ~1100 `.s` fns are tiny
`lbl_82DA0017` bit-manip registration thunks on a shared global.
**Disposition:** this front is either OvershellSlot's unpinned tail (→ EXTENSION
lever) or a tightly-coupled Overshell-provider TU (`OvershellSlotState.cpp` 5 282B
/ `SessionUsersProviders.cpp` 4 168B candidates). NOT a clean single-TU pin yet —
needs an OvershellSlot extension-vs-fresh-TU decision (deferred; see frontier).

### Region B2: tail `[~0x825CBC58, 0x825D0EF0)` — the Accomplishment*Conditional family ★

Clean, address-ordered sequence of small single-TU classes, each with a unique
string + sequential vtable. **This is the rich, bisectable seam.** Boundaries are
TU-IDENTITY-confident (string+vtable+map); exact byte edges need the standard
per-TU Ghidra bounding at land time (the "boundary needs same string/vtable
bounding" caveat).

| .text span (approx) | size / fns | TU (rb3-Wii owner) | anchor | existing pin |
|---|---|---|---|---|
| ~0x825CB590–0x825CBC58 | (transition) | AccomplishmentDiscSongConditional + SongListConditional base | `filter`; map CanBeLaunched@0x825CB550 | NONE |
| 0x825CBC58–0x825CC010 | 952B / 26 | **AccomplishmentSetlist.cpp** | `min_stars`/`setlist` | **sliver@0x8243F220** (272B dead) |
| 0x825CC010–0x825CC220 | 528B / 10 | **AccomplishmentOneShot.cpp** | `oneshot_song`/`oneshot_playermin` | NONE |
| 0x825CC220–0x825CCBE0 | 2496B / 49 | **AccomplishmentSongConditional.cpp** | `awesomes`/`full_combo`/`saves` (6 vtbl) | **sliver@0x8264CEF0** (296B) |
| 0x825CCBE0–0x825CE5A8 | 6600B / 101 | **AccomplishmentPlayerConditional.cpp** | `best_score`/`career_fills`/`total_bre_hits` | **sliver@0x8243F178** (168B dead) |
| 0x825CE5A8–0x825CE788 | 480B / 11 | **AccomplishmentTrainerConditional.cpp** | vtbl 820B6048 | NONE |
| 0x825CE788–0x825CED70 | 1512B / 24 | **AccomplishmentTrainerCategoryConditional.cpp** | `lesson_category` | NONE |
| 0x825CED70–0x825CF0F0 | 896B / 21 | **AccomplishmentTrainerListConditional.cpp** + SongListConditional InqIncrementalSymbols@0x825CEE08 | vtbl 820B62DC | NONE |
| 0x825CF0F0–0x825CF390 | 672B / 9 | **AccomplishmentSongListConditional.cpp** | InqIncrementalSymbols (map) | NONE |
| 0x825CF390–0x825CF8F8 | 1384B / 17 | **AccomplishmentSongFilterConditional.cpp** | `num_songs`/`filter` | **sliver@0x8243F378** (160B dead) |
| 0x825CF8F8–0x825CFC58 | 864B / 14 | **AccomplishmentDiscSongConditional.cpp** | vtbl 820B6614; map @0x825CB550 | NONE |
| 0x825CFC58–0x825CFDC0 | 360B / 10 | **AccomplishmentLessonSongListConditional.cpp** | `lesson_complete` | NONE |
| 0x825CFDC0–0x825D0380 | 1472B / 30 | **AccomplishmentLessonDiscSongConditional.cpp** | `lesson_complete` (2nd) | NONE |
| **0x825D0380–0x825D0E50** | **2768B / 44** | **Award.cpp** ← PORT TARGET | `vignette`/`award_genericdesc`/`award`/`group` | NONE / **UNWIRED** |
| 0x825D0E50–0x825D0EF0 | 160B / 2 | **AccomplishmentCategory.cpp** | map HasAward@0x825D0E50 | **sliver@0x8243EF98** (96B dead) |

Note the **displaced-sliver vein**: Setlist/PlayerConditional/SongFilter/Category/
SongConditional all have tiny ICF slivers pinned at 0x8243Fxxx / 0x8264Cxxx while
their REAL bodies sit unpinned here. These are sliver-EVICT-and-relocate candidates
(splits-only, no porting) once their bodies are ported/identified — high-confidence,
low-cost. The NONE-pinned ones (OneShot, Trainer*, LessonDiscSong*, Award) are
fresh port-then-pin.

---

## GAP A bisection (the 111-fn gap)

| .text span (approx) | TU (rb3-Wii owner) | anchor | wired/pin |
|---|---|---|---|
| 0x825BDF28–~0x825BF878 | **CharData.cpp** (CharData / PrefabChar / GetPrefabPortraitPath) | `prefab_mgr`@0x825BE7A8; vtbl 820B2008/2098/20F0/21F0/2220/2418/2578 | UNWIRED (header present, identical) |
| ~0x825BF878–0x825C10D8 | **InputMgr.cpp** | `auto_vocals_confirm`@0x825BF878 (CheckTriggerAutoVocalsConfirm); vtbl 820A9430/820AA30C | UNWIRED (all 25 includes resolve) |

**CRITICAL disambiguation (resolves the W11 PrefabMgr footnote):** the `prefab_mgr`
string at 0x825BE7A8 is **`CharData.cpp`'s GetPrefabPortraitPath** (`SystemConfig(prefab_mgr)->FindStr(...)`),
NOT `PrefabMgr.cpp`. The real `PrefabMgr.cpp` is the cluster at `[0x82540840,0x825426A0)`
(landed W11). The W11-PrefabMgr dossier ("Adjacent frontier") already flagged
CharData.cpp as this Gap-A-front owner. CharData.cpp is tiny (98 lines / 4 named
methods: CharData ctor/dtor + PrefabChar ctor/dtor) → the front cluster (~40 fns)
must be CharData + PrefabChar + their inlined template helpers; **needs Ghidra
boundary confirm before pinning** (small-payoff, defer below the bigger Accomplishment
seam).

---

## ★ ACTIONABLE: Award.cpp port-then-pin (self-contained, independently landable vs main@9301)

### Exact coords (bounded vs BOTH neighbours)
`band3/meta_band/Award.cpp:  .text  start:0x825D0380  end:0x825D0E50`  (0xAD0 = 2768B)

- **Below:** AccomplishmentLessonDiscSongConditional cluster (unpinned, ends ~0x825D0380).
- **Above:** AccomplishmentCategory @0x825D0E50 (unpinned 2-fn tail, ICF sliver
  pinned elsewhere @0x8243EF98), then Gap-B end == **FixedSizeSaveable.cpp .text
  start 0x825D0EF0** (pinned hard boundary).
- ⚠ **Overlap self-check MUST be re-run at land time** (parse all ~638 `.text` +
  ~618 `.pdata` pins + proposed pin → assert 0 overlaps). The proposed span sits
  in a clean gap with no adjacency to any existing pin.
- ⚠ **Lower edge needs a Ghidra bound** — confirm 0x825D0380 is Award's first fn
  (it inits `vignette`+3 Symbols → `Award::Configure`/`GrantAward`) and that the
  last LessonDiscSong fn ends exactly at 0x825D0380. Upper edge: last Award fn
  ends at 0x825D0E50 where AccomplishmentCategory::HasAward (mapped) begins.

### Port feasibility — GREEN
- rb3-Wii oracle present: `../rb3/src/band3/meta_band/Award.cpp` (4497B, **13 named
  methods**: ctor/dtor/Configure/GetDescription/GetDisplayName/GetIconArt/GetName/
  GrantAward/GrantAwards/HasAssets/HasIconArt/InqAssets/IsBonus).
- Header `src/band3/meta_band/Award.h` already in tree, **byte-identical** to rb3-Wii.
- ALL 9 includes resolve in rb3-xenon (AccomplishmentManager.h, AssetMgr.h,
  MetaPerformer.h, obj/Data.h, os/Debug.h, utl/Symbol.h, utl/Symbols.h, decomp.h).
- Ground-truthed bodies (Ghidra): 0x825D0380 = multi-Symbol-init Configure-class;
  0x825D0648 = GetIconArt/HasIconArt-class (`award_genericdesc` fallback Symbol);
  0x825D0D50 = InqAssets-class (`award`/`group` DataArray walk via FindData). Real
  logic, port-tractable (NOT instrumentation stubs).
- NOT in objects.json (fresh wire). VoiceoverPanel/EditSetlistPanel show the
  `NonMatching` wiring pattern.

### Self-contained plan (one worktree)
1. `scripts/setup_worktree.sh /tmp/w12-award award-port` (CoW buildable worktree;
   re-run `configure.py` with explicit absolute `--dtk/--objdiff/--wrapper` per the
   worktree-dtk-trap note).
2. Copy `../rb3/src/band3/meta_band/Award.cpp` → `src/band3/meta_band/Award.cpp`.
   Port MWCC→MSVC X360. Keep RB3 game semantics from rb3-Wii (DC3 is a FALSE FRIEND
   here). Watch: `SystemConfig(...)`/`Symbol(...)` lazy globals, `DataArray::FindData`,
   `MetaPerformer`/`AssetMgr` cross-TU `bl` (resolve by address; no need to compile
   those TUs). `MILO_WARN`/`MILO_FAIL` gated by repo Debug.h.
3. Wire `objects.json`: add `"band3/meta_band/Award.cpp": "NonMatching"` near the
   other `meta_band/` entries.
4. Pin in `splits.txt` (`.text` only; `touch config.yml && ninja` auto-back-fills
   `.pdata` — never gap-shrink, never hand-pin pdata):
   ```
   band3/meta_band/Award.cpp:
       .text       start:0x825D0380 end:0x825D0E50
   ```
   (Ghidra-confirm both edges FIRST per the caveat above.)
5. Build the map entries: pair the compiled `Award.obj` defined MSVC symbols to
   cluster `fn_<addr>` by structure (size + call-graph + string xrefs), seeding the
   3 Ghidra anchors. **ADD only** to `scripts/target_symbol_map.json` — NEVER
   regenerate wholesale (poison rule, wave lesson 4). Do not touch any ICF-folded
   entry that names a different representative.
6. `rm -f build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml
   && NINJA_JOBS=8 tools/fresh_report.sh`; read `measures.matched_functions`; re-run
   once (splits-only FP warning is known).
7. Re-derive precise fn addrs from `asm/.../Award.s` (`^.fn fn_<hex>`); `/batch-check`.
   Match trivial accessors (GetName/IsBonus/HasIconArt) + Symbol getters first; then
   Configure/GrantAward/InqAssets bodies; add `reveal` map entries for any byte-exact-
   but-0% fn (no body change). Defer regalloc/funclet near-misses.
8. **Honesty gate before declaring landable:** re-run overlap self-check (0 overlaps);
   net ≥ +1; no ≥8-contiguous FOREIGN fn@0% run (own STL/MakeString/funclets bracketed
   by own named = OK); headline net == sum of Award unit gains.

### Expected delta
~+18 (44 fns in span; ~13 real methods + funclets/thunks/dtor variants; sibling small
panel ports landed +9–+24). Conservative floor +10.

---

## flag_foundational
**false** — self-contained RB3 game-code port; no binary-wide / shared-header change.

## Next-wave queue (frontier priority order)
1. **Award.cpp** (this, actionable). 2. Sliver-EVICT-relocate the displaced
Accomplishment*Conditional bodies (Setlist/PlayerConditional/SongFilter/SongConditional/
Category — splits-only once bodies ported). 3. The NONE-pinned conditionals (OneShot,
Trainer*, LessonDiscSong*, SongListConditional) — fresh port-then-pin, tiny TUs.
4. Gap A: CharData.cpp + InputMgr.cpp (both UNWIRED, deps resolve). 5. Region B1
OvershellSlot extension-vs-fresh-provider-TU decision. 6. Re-run pin_audit after each
landing wave.
