# W9 L5 adversarial scout — "songsortmgr-family-cluster-pin"

Date: 2026-06-20 · Layer 5 (adversarial discover/planner, READ-ONLY in main)
Baseline: main @ 812e1df = **8314** matched (report.json measures.matched_functions)
Frontier item (kind=scout, est +40): "SongSort family (0x82558EE0-~0x8255DE88):
SongSortMgr + ~9 SongSortByX float-comparator TUs … worktree
w9-songsortmgr-port-then-pin already at 8392 (+78). Bound each small comparator
TU; wire+pin+port from rb3-Wii."

## Verdict: REAL_ACTIONABLE — but the frontier's framing is half-wrong

Two distinct claims in the frontier; one is a proven win, the other is largely a
COMDAT-scatter wall.

### CLAIM 1 (TRUE, proven, ready-to-land): SongSortMgr port-then-pin = +78

The worktree `wt-w9-songsortmgr-port-then-pin@8d4d72c` is a **single clean commit
on top of main@812e1df** (parent == main HEAD exactly). Its built
`build/45410914/report.json` reads **8392 = +78 vs main 8314**, and the entire
gain localizes to one new pinned unit `default/SongSortMgr` (168 total / **78
matched_normalized==100**). I re-audited it directly:
- longest FOREIGN(named)-at-<100 contiguous run = **2** (`??1KerningTable…`),
  passes the ≤7 honesty gate.
- longest ANY-at-<100 run = 9 (own anon `fn_` TU-grouped unported tails — OK).
- header change `src/system/meta/StoreOffer.h` adds **declarations only**
  (Genre/Decade/LengthSym/RatingSym/VocalPartsSym/HasSolo/Rating/IsRbn) — no
  member, layout-neutral; the worktree's own whole-binary A/B already showed net
  == unit gain with zero regressions, confirming no cross-TU ripple.

What it does: ports `band3/meta_band/SongSortMgr.cpp` + `SongRecord.cpp` from the
rb3-Wii oracle (MWCC→MSVC), wires both in objects.json, pins the SongSortMgr
.text cluster `[0x82580040,0x82583DD8)` carved into 3 sub-ranges to step around
two ICF-folded StaticClassName slivers (LabelShrinkWrapper @0x82582EF8,
LabelNumberTicker @0x82582F78), and merges 3 reveal_sweep target_symbol_map
entries (ClearInternalSetlists / `operator>>` / `_Rb_tree::_M_erase`).

**This is independently landable vs main@8314 AS-IS (ff-merge).** It is the
single highest-value action here. attribution_risk = true (pin + relocation).

### CLAIM 2 (mostly FALSE): "SongSort family at 0x82558EE0-0x8255DE88, bound
### each of ~9 comparator TUs and pin"

The stated address range is **wrong / not a coherent SongSort cluster**:
- `ContextChecker.cpp` is pinned on main `[0x82555398,0x82558EAC)` and ends at
  0x82558EAC — i.e. the frontier's lower bound 0x82558EE0 is just above it.
- The range `[0x82558EE0,0x8255DE90)` holds **224 functions / ~20KB** — far more
  than ~9 small comparator TUs (~60-90 fns). Its string content is UI panels
  (RetailAudioPanel, TourDescPanel, SongSelectPanel, MoviePanel, CreditsPanel…)
  and audio (synth, postprocess, sfx/streams) — **not** SongSort.
- `fn_8255DE88` IS a real `SongSortByX::Compare` (verified by disasm: calls the
  singleton `0x82803f38` then `0x825875D8` TWICE = double-compare-then-tiebreak,
  ends in `rlwinm` sign extraction — the float-comparator tiebreak shape). But it
  sits **isolated**: immediately after `fn_8255D810` (a 409-insn
  VerifyBuildVersionMsg handler, unrelated TU) and before 3-insn stub tails.
  Classic **COMDAT template-scatter** — the linker dropped this one comparator
  method between unrelated TUs.

The SongSort comparator family is **scattered across at least 4 disjoint regions**:
`0x8255DE88` (isolated Compare), `0x82580040` (SongSortMgr, now harvested),
`0x82586338-0x82588000` (an 81-fn unpinned gap whose strings are
real_bass/real_keys/drum/song_lengths = Diff/instrument metadata sorting — a
plausible SongSortNode/SongSort cluster but bleed-mixed with an
esrb_keep.milo-startup TU), and `0x825A6640-0x825A7038` (StoreSongSortNode,
already pinned on main, only 4/25 matched). The comparator strings
("invalid type of node comparison.\n", "Couldn't find a letter") appear **0
times** in fingerprints — retail STRIPPED the MILO_FAIL/MILO_ASSERT strings, so
string-fingerprinting cannot locate these bodies; they're pure logic.

This is the SAME signature the roadmap already flagged REFUTED for Waypoint:
"COMDAT template-scatter, DC3 map corroborates — do not relocate." Pinning each
scattered comparator method individually is high-cost, low-yield, and
honesty-risky (a tight pin around a single Compare drags in foreign neighbors).

The wired comparator .cpps (SongSortByArtist/Diff/Plays/Rank/Recent/Song in
objects.json on main) **compile but contribute zero measured functions** — no
`.text` pin ⇒ no target obj ⇒ they don't even appear as units in report.json.

## Actionable items emitted

1. **Land SongSortMgr port-then-pin (+78).** Harvest worktree 8d4d72c as-is;
   ff-merge onto main. Self-contained, proven, zero-regression. (attribution_risk)

2. **SongSortNode/SongSort gap pin+port (the 0x82586338-0x82588000 cluster).**
   The only OTHER coherent SongSort cluster with a plausible contiguous span. Port
   `SongSortNode.cpp`(+`SongSort.cpp`) from rb3-Wii, wire, pin a tight span inside
   the gap (bound by NetSync end 0x82586338 and MoveVariant start 0x82588000),
   reveal+convert. Lower confidence (bleed-mixed startup-milo TU in the gap; must
   honesty-audit own-vs-foreign and carve around the esrb_keep sliver
   @0x82586340). Est +10..20, NOT +40. (attribution_risk)

## Discovered frontier (adjacent leads, seed later layers)

- **StoreSongSortNode deepen (already pinned, 4/25).** Pinned on main
  `[0x825A6640,0x825A7038)` but only 4/25 matched — a body-port deepen target
  (port StoreSongSortNode.cpp factory/compare bodies from rb3-Wii to lift the
  21 near-misses). No new pin needed; pure bodyport. ~+8.
- **mRankings std::map<int,pair<int,bool>> = hash_map vein tie-in.** SongSortByRank
  (rb3-Wii) uses `std::map<int,std::pair<int,bool>>` find/lower_bound/insert —
  the int-key map that the W9 hash_map vein converts to `std::hash_map` (int-key
  COMDAT lbl_82552CD0). If pinned, SongSortByRank::NewSongNode/OnMsg are
  find()-using fns that flip byte-exact under the hash_map fix. Feeds the
  re-opened hash_map campaign, not a standalone pin.
- **The 0x82558EE0-0x8255DE88 panel cluster is its own unmined vein.** 224
  unpinned fns whose strings name a dozen UI panels (RetryAudioPanel,
  TourDescPanel, TourChallengeResultsPanel, SelectDifficultyPanel,
  SongSelectPanel, TrainingPanel, CreditsPanel, MoviePanel, BandScreen). These
  are panel TUs sitting in the auto_03 blob — a candidate for a separate
  panel-family port+pin scout (NOT SongSort). Each panel is a wired-or-portable
  band3 game TU.
