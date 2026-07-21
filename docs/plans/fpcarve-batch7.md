# Fingerprint carve — BATCH 7 (game-repin residue drain)

Author: batch-7 foreman. Consumed the batch-6 §6 seeds (b73e14ba) per the
batch-6 economics ordering: game repins first, then interleave wires, then
engine-dc3 pool, then post-repin re-harvest.

**Baseline:** main @ 20,725 strict. **Final:** main @ 21,118 strict.
**Batch-7 net: +322** (5 workers / 9 landed commits, 0 regressions on every
gate). A concurrent correlator lane (r10, 636ce19f) landed +71 mid-wave
(21,006→21,077); rebase-per-landing absorbed it silently (batch-6 precedent).

## Per-item results

| worker | TU | mechanism | net | commit |
|---|---|---|---:|---|
| W3 | band3/ui/BandSongMetadata.cpp | StoreOffer-span TU5-verify → re-ID as BandSongMetadata under-carve; repin | **+62** | 24becac8 |
| W1 | band3/game/PracticePanel.cpp | frag-merge → full span 826B2040..826B4FF0 + automap (14) | +72¹ | 2689844b |
| W3 | band3/game/Band.cpp | full-span repin + automap | +52 | c7b2482e |
| W1 | band3/game/GemPlayer.cpp | tail extend 826C0238..826C1760 + automap (11) | +41 | 0feb2c9e |
| W4 | system/beatmatch/TrackWatcherImpl.cpp | 14-frag merge → single span 82794740..8279B778 + automap (41) | +37 | f26aac37 |
| W2 | band3/game/TrainerPanel.cpp | 4-frag merge → 826C97B0..826CD438 + automap (6) | +27 | cb712650 |
| W2 | band3/meta_band/CharacterCreatorPanel.cpp | gap-fill 8260D1B8..8260E6AC + automap (5) | +27 | b2e8a91f |
| W5 | system/rnddx9/ShaderMgr.cpp | BACKWARD under-carve (run sits *before* own pin) + automap (1) | +4 | b5cdd5e9 |
| W4 | system/flow/FlowWhile.cpp | map-only SyncProperty pairing (fuzzy, reloc-blocked from strict) | +0 | 565d924d |
| W4 | system/flow/DrivenPropertyEntry.cpp | at mapping ceiling — fully carved around 7 foreign TUs, no commit | 0 | — |
| W5 | Overshell/Leaderboard/Chordbook re-harvest | all 3 automap fragments EMPTY — batch-6 fully captured | 0 | — |

¹ W1's in-worktree claim was +113; composed-on-main measured +112 (one
marginal pairing). W2 composed +55 vs claimed +54 (same coin, other side).
All other claims reproduced exactly.

**StoreOffer verdict (the batch-6 open question):** census span
`8259FF30..825A3648` is **NOT** StoreOffer or StoreOfferProvider — it is
BandSongMetadata.cpp's under-carved body. Re-IDed and landed +62 by W3.

## Economics (flips per worker-minute, wall-clock proxy)

- **Game repins: +281 / ~24 wkr-min ≈ 12 flips/wkr-min.** Still the best lane,
  but DOWN from batch-6's 30 — the tier is thinning as predicted (batch-6 took
  the chunkiest carves first).
- **Engine repin (TrackWatcherImpl): +37 / ~10 wkr-min ≈ 4 flips/wkr-min** —
  in line with batch-6's 5.5. The census "funclet-heavy, may net 0" warning
  did NOT hold; the 14-frag merge exposed real bodies.
- **Engine-pool exploration (W5): +4 / ~15 wkr-min ≈ 0.3 flips/wkr-min** — the
  drain signal, not a work lane.
- Interleave wires (FlowWhile/DPE): 0. Both are boxed-in/at-ceiling, not cheap
  wires. Remove from seed lists.

## STRATEGIC ANSWERS

1. **Game-repin Tier-1/2 seed list: DRAINED.** Every batch-6 Tier-1 item is now
   taken (OvershellPanel, ChordbookPanel, Leaderboard, PracticePanel,
   TrainerPanel, Band, SelectDifficultyPanel, FreestylePanel,
   CharacterCreatorPanel, GemPlayer) plus the StoreOffer span (=BandSongMetadata).
   The *mechanism* is not proven dead — but no enumerated candidates remain; a
   fresh truncated-pin census (clean_extend.py shape) is needed to find more.
2. **Engine-dc3 CARVE pool: DRAINED** (W5, three-way confirmation): (a) all 8
   batch-6 Tier-2 engine TUs re-automap to 0 new entries; (b) every fresh
   engine census run except ShaderMgr is a string mis-attribution (Anim.h /
   EventTrigger.h template+base scatter — adjacent-pin owners don't match the
   attributed TU); (c) probed engine pin gaps are COMDAT-scatter interleaved
   near-miss bodies, not contiguous carveable mass. **Remaining engine work is
   body-port labor, not carve/automap labor.**
3. Re-harvest of batch-6 WEAK tails yields 0 — WEAK entries do not passively
   improve; they need body-ports first, then re-automap.

## Batch-8 seeds (ranked)

1. **Fresh truncated-pin census re-run** (clean_extend/fp2_runs over the
   post-batch-7 splits state, LOWER threshold than ≥5 fns/≥4 hits) — the
   game-repin mechanism ran at 12-30 flips/wkr-min while enumerated; only the
   list is empty, and ~2.0 MB of MAIN-region mass is still unpinned.
2. **Post-repin near-miss body-port lane** — the batch-6+7 repins exposed
   hundreds of newly-paired WEAK near-misses (PracticePanel, GemPlayer, Band,
   BandSongMetadata, TrainerPanel, TrackWatcherImpl, Overshell/Leaderboard
   tails). This is now the biggest visible vein; route through the bodyport
   machinery, then re-run automap per unit (coverage grows as bodies flip).
3. **Quazal in-tree extension** (batch-6 Tier-3 #23, untouched): extend/pin the
   16 wired `src/network/*.cpp` like batch-5's NetSession (+40); ~400 KB
   carvable, best Quazal ROI.
4. **Game/meta src=NONE gameport candidates** (from W5's fresh-census sweep):
   ChallengeSortNode, MetagameRank, HamUI, HamStorePanel, BustAMovePanel —
   rb3-Wii oracle, needs from-scratch source (gameport lane, grade B).
5. Carried deferred walls: CountOrCreateExpandedDetails (+230 funclet prize),
   NavListNode-family RE, Profile::GetPadNum/PlatformMgr foundational levers.
6. **De-listed:** FlowWhile, DrivenPropertyEntry (at ceiling); engine-dc3 carve
   pool (drained); Overshell/Leaderboard/Chordbook re-harvest (empty).

## Friction census (batch 7)

1. symbols.txt dirt blocked land.sh on 2 worktrees — regenerable, discarded
   per SOP (`git -C <wt> checkout -- config/45410914/symbols.txt`). Consider
   adding this to land.sh's auto-discard whitelist next to download_tool.py.
2. Mid-wave concurrent landing (correlator r10 +71) briefly appeared as
   uncommitted main dirt at foreman gate time (their WIP was committed seconds
   later); rebase-per-landing + gate-vs-live-main absorbed it, as in batch-6.
3. ±1 composed-vs-claimed drift on W1/W2 (one marginal pairing flipping between
   environments). Gate on net>0 + zero losses, not exact claim reproduction.
4. One funclet-echo slip accepted (ShaderMgr fn_827362B0 99.8→0, STL __fill_n
   ICF byte-twin re-claim — project-precedent artifact, no code change).
