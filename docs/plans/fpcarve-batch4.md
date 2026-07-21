# Fingerprint carve — BATCH 4 outcome (2026-07-21)

Author: batch-4 carve foreman. Consumed the batch-4 seeds of
`docs/plans/fpcarve-batch3.md` (831c7a96). **Baseline 19,786 → final 20,007
(= +221, incl. +14 latent batch-3 re-measure), 8 landings, 0 lost.**
Crossed **20,000** at PlayerLeaderboards (ff341c3c).

## Landings

| # | candidate | span(s) | delta | commit | notes |
|---|-----------|---------|-------|--------|-------|
| 1 | PerfectOverdriveTracker | 826DF798..826E1D80 | +23 | ac17ece3 | head is StreakTracker-overflow COMDATs (stay unmatched); +14 latent batch-3 matches surfaced by main full build |
| 2 | Synchronize.cpp split-out | 823E9158..823E9750 (cut SyncStore at 9750) | +12 | 0dd77f82 | seed's interior-insert hypothesis WRONG — two contiguous TUs, single cut; BandwidthCounter PerfCounter DROPPED (no-oracle RE) |
| 3 | OverdriveTimeTracker + AccuracyTracker | 826E1F98..826E2438 + 826E2438..826E29E0 | +15 | d2dccc1d | both TU5-correct |
| 4 | NextSongPanel gap-fill | 8264519C..8264BBE0 | +33 | bd1b54b9 | 34 funclets auto-pair + 2 map entries; 15 named methods = codegen walls |
| 5 | LayerDir header-port | 823264D8..82326D1C + 823278F0..82329B14 | +33 | 73e69fa1 | Wii-only oracle; engine positional pairing, no map needed |
| 6 | DeployCountTracker | 3 fragments 826DC128..826DD2F4 | +17 | 802f0300 | fingerprints TU0-STALE — located via vtable 0x820f010c + Ghidra; reclaimed PerfectSectionTracker over-carve + RockCentral mis-pin |
| 7 | SessionMgr | 82584B58..82588D00 | +51 | 686f2209 | biggest win; 50 strict via reloc-masked correlator |
| 8 | PlayerLeaderboards pin-ext | 82673338..82673B1C | +17 | ff341c3c | seed #20 "Leaderboards (dc3)" MISIDENTIFIED — span is PlayerLeaderboards scatter; DC3 oracle survival ~0% |
| 9 | StorePanel rb3-Wii re-port | (header/layout wave) | +6 | 2dc12400 | root cause: DC3 port carried +0x18 DC3-only members; sibling premise disproven (only BandStorePanel derives StorePanel) |

Tracker family (seeds 1,3,6 + batch-3 #15) is now **fully drained**:
all 8 game tracker TUs wired and pinned.

## TU0-stale / misidentification census (the batch-3 rule, measured)

- **1/8 spans truly TU0-stale**: DeployCountTracker (fingerprints' 0x826d0xxx
  cluster = AccomplishmentProgress in TU5). Recovery: vtable VA + per-fn Ghidra.
- **2/8 misidentified otherwise**: Synchronize (wrong sub-split topology, right
  TU), Leaderboards (wrong TU name/oracle, right span).
- **Identification oracles (unified_id_rb3wii.json, gen_game_target_map.py) are
  TU0-stale in EVERY game carve** — 0 usable entries all batch. The working
  replacements: `scripts/harvest/tu5_reloc_masked_correlate.py` (byte-exact
  pairs), hand-derivation from carved fn sizes + call signatures, and Ghidra
  ground truth. Retire gen_game_target_map for TU5 work.
- `gen_game_target_map.py --apply` also **json.dump-reorders the entire 18k-line
  map** — two workers hit it, both recovered by textual re-splice. Add a guard
  or never use --apply.

## Friction census

- **StorePanel layout wave blast radius**: tiny — `meta/StorePanel.h` has
  exactly 4 includers; only BandStorePanel derives the class. The scary
  cross-TU wave never materialized; the real work was the DC3→Wii re-port
  (recon worker + port worker, ~2 worker-slots for +6). Economics: poor as a
  carve, good as a foundation (4 StorePanel logic fns now mapped at 30-82%).
- **Tracker-family economics**: excellent. 4 seeds → +55 strict at high
  per-worker hit rate; oracle quality high; layout drift small (base Tracker
  ends 0x64 on Xbox — Wii `// 0x58` comment stale, NO padding needed).
- **Foreman landing traps**: none new. Merge-base diffs + `git apply -3` with
  union conflict resolution handled all splits/objects/map collisions between
  same-region landings; map fragments always spliced textually (line-count
  sanity held: 9-53 line fragments, never re-serialization).
- Landing-1 surplus lesson: main's post-landing full build surfaced +14 latent
  matches from batch-3 landings (their landing worktree reports undercounted
  those units). Always attribute per-landing delta from the worker's own
  clean-worktree A/B; main's authoritative count re-baselines each landing.

## Transferable cracks (new this batch)

- Function-local static Symbols must be declared **interleaved at
  point-of-use**, not batched at fn top (decisive on 6+ fns).
- Reference hoist (`Stats &s = p->mStats;`) materializes the spill retail emits.
- Bool materialization (`bool b = x != -1; if (b || ...)`) → `li/li/clrlwi.`.
- `map::find` not `lower_bound`; cache repeated getter calls.
- FP precision is PER-CALLSITE: retail mixes single (`ceilf`, frsp'd
  `std::max((float)...)`) and double (`bl floor` + `frsp`) — read the carved
  asm, don't apply the floorf crack blindly.
- Implicit-dtor vtable-store elision: removing empty `virtual ~X(){}` restores
  retail's ICF fold (PlayerLeaderboards 68→100).
- Explicit `__unwind$` map entries can BLOCK objdiff funclet auto-pairing —
  removing 3 gave +3.

## Batch-5 seeds

1. **Leaderboard::ShowGamercard base virtual** — fn_826733F0 (436B + 2 Symbol
   dtors, `on_select_gamertag_error`/`display_gamercard_*` strings), present in
   6 vtables, no in-tree source. Needs a `Leaderboard.cpp` base-class carve.
2. **Tracker tail 826E29E0..826E3804** — unidentified ~1.4K after
   AccuracyTracker; census with fp2_span + Ghidra (fingerprints strings empty).
3. **StreakTracker overflow head** [826DF798,826E03D0): big-method COMDATs
   pinned under the POT obj, unmatched — needs a repin/transplant strategy
   (their bodies exist in-tree in StreakTracker.cpp).
4. **BandUserMgr::mSessionMgr +0x90 vs +0x94** shared-header drift — blocks
   SessionMgr ctor (99.996%) and possibly other BandUserMgr-touching fns;
   needs a gated cross-TU A/B like the StorePanel wave.
5. **StorePanel bodyport tail**: Load 82%, EnumerateOffers 62%, Poll 58%,
   UpdateOffers 30% — mapped with retail IDs, need Ghidra-faithful body tuning.
6. **SessionMgr mediums**: Handle 75%, AreInvitesAllowed 63%,
   OnMsg(NewRemoteUser) 71% + ~10 more — genuine Wii-vs-retail divergence,
   Ghidra reconstruction lane.
7. **NextSongPanel deep grind**: 238 anonymous STL instantiations at 90-99%
   (1-2 insn) + 15 named walls — cheap if a systematic STL lever emerges.
8. **FingerShape 6 scattered methods** — NOT consumed this batch (structural
   correlation, no unique strings). Current pin at 0x82355148 is TU5-correct;
   fingerprints' 0x82345828 span is the TU0 ghost. Low priority.
9. **NetSession** (batch-3 #14 remainder): Synchronize + session headers now
   in — the deferred NetSession body cluster [823E6F60,823E9158) is unblocked.
