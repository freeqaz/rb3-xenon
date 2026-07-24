# Batch-14 map-recovery foreman — results (2026-07-24)

Foreman: BATCH-14. Baseline main **24,928** → final main **24,955**
(`measures.matched_functions`, composed full-rebuild A/B via `tools/fresh_report.sh`,
`report.cache` cleared each leg). **Net +27 strict, 0 strict regressions.**
3 Opus workers (one per surviving mechanism); foreman landed serial via
`scripts/harvest/land.sh` union-resolve + `git merge --ff-only` + a combined
full-rebuild verify (24949→24953→24955 confirmed 1:1 to the three lands).

Concurrent, byte-neutral: a native-build restore (`749f7137`) landed on main
mid-wave; land.sh rebased cleanly (union files + independent source edits).

## Landed (per worker / mechanism)

| worker | mechanism | Δstrict | detail | commit |
|---|---|--:|---|---|
| CONT | continuation-span pins | **+21** | 4 owner-with-gap units repinned; `size_order_automap` recovered 8 EXACT/STRONG pairings. HamMove +5 (BACKWARD gap), band3/game/Tracker +5 (INTERNAL swiss-cheese), AccomplishmentTourConditional +6 (FORWARD tail), CharCache +5 (FORWARD tail). 0 source edits. | 48cecb25 |
| SCATTER | targeted body-dup | **+4** | 4 `#ifndef HX_NATIVE` one-method body-dups emitting scattered COMDATs into their span-owner TUs: SongUpgradeMgr←LicenseMgr::HasLicense, SongMgr←SongMetadata::InitSongMetadata, StoreInfoPanel←StoreMainPanel::Load, FlowSetProperty←Song::SetLoopStart. | caf2db2e |
| RELAYOUT | struct-relayout→map | **+2** | SongPreview: 3 DC3-newer members (mTexMovie/mInitted/mPreviewDb, 0x14 total) relocated to class end so front members regain retail offsets; PreparePreview + 1 more flip byte-identical. | f574b7df |

No fuzzy slips this wave (all lands additive / pure layout).

## Mechanism economics (measured this cycle)

- **Continuation-span pins — +21, still the top residual lever.** 4/12 screened
  candidates paid. The `est_yield` ranking from `repin_census.py` MISPREDICTS:
  the highest-est candidate (BeatMatchController est=8) was foreign-scatter
  interleave (BeatMatcher/GemTrack/MoggClip in the gap → 0 pairs, reverted), as
  were StorePreviewMgr/Song/UIComponent (WEAK-only or already-mapped). The real
  wins were mid-list est=1–2 units whose gap held a small run of the SAME TU's
  bodies. **Screen by size_order_automap EXACT/STRONG output, not by est.** All
  three gap shapes paid (FORWARD tail, BACKWARD lead, INTERNAL swiss-cheese).
- **Scatter targeted body-dup — +4, whole-file include is dead.** Every TIER-B
  whole-file `#include "Owner.cpp"` either exploded (gRev/LOAD_REVS macro context
  in BandIKEffector←HamIKEffector) or the pairing landed a near-miss (regalloc /
  frame-Δ wall, not a clean flip). The productive shape is now ONLY the
  **single-method `#ifndef HX_NATIVE` body-dup** where the scattered fn is a
  small, self-contained retail-arity method (1 fn = +1). The named-seed
  AccomplishmentManager←PracticeSection was confirmed a **hard DUD** — all-STL
  template instantiations (`??$…`), which never pair into the includer.
- **Struct-relayout→map — +2, the clean member-delta vein is DRAINED.**
  `member_delta_finder2` over the 85–99.99% band returned **0 MEMBER_DELTA / 0
  SIZED_VECTOR**; every candidate was a VBASE_WALL (coupled-base, defer) or a
  single-fn/single-access UNKNOWN. 1/5 probed UNKNOWNs (SongPreview) was a
  genuine one-member layout fix; the other 4 were REJECTED as base-class-coupled
  (HamMaster, BeatMatchController), vtable-coupled (SampleInst: retail has 2 extra
  virtuals), or logic/different-sub-object (MoggClip). Single shifted-access
  candidates are mostly noise, exactly as the tool self-flags.

## HX_NATIVE guard (standing rule reinforced this wave)

Every scatter `#include "<owner>.cpp"` AND every body-dup MUST be
`#if !HX_NATIVE`/`#ifndef HX_NATIVE`-wrapped: the native x86_64 build compiles the
same TUs and ungated dups cause duplicate-symbol link failures + pull game/render
code onto the DTA path (native had bit-rotted to 109 errors from exactly this;
fixed concurrently in `749f7137`). The guard is byte-neutral for the X360 A/B gate.

## Honest frontier statement (feeds next portfolio decision)

**The three mechanical map-recovery veins are now drained to a thin residue.**
With batch-14's +27, the near-free harvesting classes remaining are:

- **Continuation pins**: a handful of unscreened mid-est owner-gap units still in
  `~/tmp/b14_census.json` (est=1 tail, ~40 rows) — realistic **~+5..+10** more,
  a few funcs at a time, but each must be size_order_automap-screened (est lies).
- **Scatter body-dup**: ~34 untouched rows in `~/tmp/b14_scatter/…txt`, but after
  this wave's filter the *distinct-owner single-real-method* fraction is small and
  each is +1. Realistic **~+5..+15**, high screening cost (STL/self-owner/gRev
  duds dominate). BandIKEffector←HamIKEffector is a confirmed real +1 parked at
  78.9% behind a regalloc wall (needs a sanctioned permuter pass).
- **Struct-relayout**: essentially **exhausted** for the near-miss band (0 clean
  finder hits); the remaining member-delta signal is behind the VBASE/coupled-base
  wall, which is deep multi-inheritance work, not one-line fixes.

**The mass class that remains is NOT map-recovery — it is body-divergence and the
UNKNOWN-anon frontier.** `native_scope_map.py` at this baseline: native-scope
(CORE+SOON) is 63.2% fns / 45.9% bytes; whole-binary 36.0% fns is dragged by
**29,772 UNKNOWN-anon functions (5.77 MB) at 0%** — unidentified dtk auto-split
blobs with no source path. That pool, plus the per-unit body-divergence tail on
identified units (RockCentral, SaveLoadManager, NextSongPanel, UIStats,
MoveMgr — all confirmed no census gap), is the real remaining decomp mass.

**Price of the next tranche:** the cheap mechanical levers are ~drained (residual
~+15..+35 total across all three, at rising screening cost per flip). Materially
larger gains now require a **different class of work**: (a) game-TU *location* for
the UNKNOWN-anon pool (BinDiff/BSim structural transfer of DC3 names onto
anonymous VAs — the ~906-name spike in `project_bindiff_spike_2026-07-20`), or
(b) sanctioned **body-porting / permuter** passes on the identified near-miss
tail (currently gated off). Continuing pure map-recovery waves will yield
diminishing single-digit returns.

decomp.db re-ingested at 24,955 (5,299 complete / 65.4% avg).
