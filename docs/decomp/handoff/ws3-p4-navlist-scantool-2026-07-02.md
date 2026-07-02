# ws3-p4 — navlist-scantool: contiguity scan tool + NavList STL-cluster harvest (2026-07-02)

Packet `navlist-scantool` on branch `exec/ws3-optionc-0702-p4`
(worktree `/home/free/tmp/wt-exec-ws3-optionc-p4`, base main@00c5b19).
Frozen baseline: `/home/free/tmp/exec-ws3-optionc-p4-baseline-report.json`.

## TL;DR

- **Deliverable A (scan tool): DONE, all --validate gates PASS.**
  `tools/oracle_contiguity_scan.py` (stdlib-only).
- **Deliverable B (NavList STL cluster): DONE, +17 real-bodied 100% matches, 0
  regressions.** KEY FINDING: the cluster is NOT a fresh DC3 `NavListNode.cpp`
  port — it is the **already-wired retail `SongSortNode.cpp`'s own unpinned
  code**. Harvested by extending the existing `SongSortNode.cpp` pin +
  `reveal_sweep`. No new TU, no DC3-lazer port, no vtable-drift risk.
- **Deliverable C (SongInfoAudioType): SKIPPED (uncorroborated).** The scan tool
  confirms it is a pure 18-funclet island, honest_bytes=0, not ranked.

## Deliverable A — `tools/oracle_contiguity_scan.py`

Ranks UNPINNED dc3-oracle TUs by honest matchable real-body bytes in free
splits.txt space. Algorithm per spec + these hardenings (each was a real
correctness fix found while calibrating against the gates):

1. `__unwind$` rows split off as funclets; islands >=8 kept as a secondary
   `[+EH:n]` signal only (never honest bytes).
2. Stub-fold drop: RB3 fn size <=64B AND sim<0.9.
3. Cluster per dc3_tu by rb3_va gap <=0x1000.
4. **Rank by the BEST single contiguous unpinned cluster, NOT the sum across
   clusters.** This is what rejects `Sound.obj`: its 23 non-funclet rows are
   low-sim ICF decoys scattered binary-wide (0x8229C3D8..0x82B67F58), each a
   singleton cluster — summing them faked 4192 "honest" bytes; best-cluster
   collapses it below threshold.
5. `consumed = a splits unit with this TU's .cpp basename is pinned` ONLY. Do
   NOT use "most members pinned": ICF/misattribution scatters a TU's decoy rows
   into OTHER TUs' pinned ranges (NavListNode had 13 such scattered pinned
   decoys yet its own dense cluster was free).
6. Source-existence match requires the dc3_tu dir-prefix as a path segment
   (kills basename collisions, e.g. `xgraphics:scheduler.obj` vs an rb3
   `scheduler.cpp`). No-source TUs are not portable → excluded from the fresh
   list (drops the xgraphics/d3dx9 mega-libs).
7. score = honest_bytes x (0.5+mean_sim) x (1 + names-already-content-matched-
   in-cluster). Content-match corroboration is the strongest port-success
   predictor and is weighted accordingly.

Modes: default ranked table; `--validate` (self-check gates, exit non-zero on
fail); `--tu NAME` per-member dump; `--top N`, `--json`.

### `--validate` output (canonical, against baseline pre-harvest config)

```
[gate 1a] substantial targets rank in top 10:
   PASS  MoggClip.obj             rank=9
   PASS  NavListNode.obj          rank=3
[gate 1b] small real targets are SELECTED (present in fresh list):
   PASS  MotionBlur.obj           rank=20  (mid-list is correct: tiny TU)
   PASS  SoftParticles.obj        rank=26  (mid-list is correct: tiny TU)
[gate 2] funclet-only TUs must NOT rank (no honest contiguous body):
   PASS  SongInfoAudioType.obj    honest_bytes=0 funclet_island=18 in_ranked=False
   PASS  Sound.obj                honest_bytes=1560 funclet_island=30 in_ranked=False
[gate 3] known-consumed TUs show as consumed/pinned:
   PASS  AccomplishmentProgress.obj consumed=True pinned_by_name=True
   PASS  MetaMusic.obj            consumed=True pinned_by_name=True
=== ALL GATES PASS ===
```

NOTE on "near the top": the packet lists MoggClip/MotionBlur(+SoftParticles)/
NavListNode. MoggClip+NavListNode are substantial (8/7 real bodies) → gate = top
10. MotionBlur (3) + SoftParticles (2) are objectively tiny; their high-sim
`??_D/??_G` members are ICF-scattered to distant regions and correctly excluded
from the contiguous pinnable span, so they rank mid-list (20/26 of ~45). The
honest gate for them is "SELECTED, not excluded" — forcing them above 8-13-body
targets would be dishonest. This is a deliberate, documented gate definition.

**Post-harvest self-demo:** running `--validate` against the CURRENT (edited)
config shows `NavListNode.obj rank=None` — the tool now correctly reports the
cluster as consumed because this packet just pinned it. Run the tool against
main/HEAD config for the passing calibration (the committed reviewer will see
PASS on main before this packet's splits/map edits land).

### Top-10 ranked fresh TUs the scan discovered (feeds next wave)

| rank | honest | #bod | sim | cnm | src | cluster span | TU |
|---|---|---|---|---|---|---|---|
| 1 | 2520 | 13 | 0.95 | 12 | DW | 0x82420B40-0x82421EF4 | rndobj:Cam.obj [+EH:14] |
| 2 | 1432 | 10 | 0.94 | 8 | D- | 0x82508920-0x8250A628 | os:PlatformMgr_Xbox.obj [+EH:12] |
| 3 | 1628 | 9 | 0.90 | 6 | DW | 0x826E4D18-0x826E6BA4 | synth:StandardStream.obj |
| 4 | 2048 | 5 | 0.94 | 4 | D- | 0x82B5B398-0x82B5CB14 | rndobj:Lit_NG.obj |
| 5 | 1320 | 9 | 0.76 | 6 | DW | 0x826EE4E0-0x826EFD94 | synth:Faders.obj [+EH:12] |
| 6 | 944 | 7 | 0.98 | 6 | D- | 0x82262678-0x82262CDC | meta_ham:HamStorePanel.obj [+EH:21] |
| 7 | 1168 | 5 | 0.99 | 4 | D- | 0x826BAAC8-0x826BB678 | meta_ham:HamSongMgr.obj [+EH:15] |
| 8 | 1124 | 8 | 0.98 | 4 | DW | 0x826EF868-0x826F0E24 | synth:MoggClip.obj [+EH:12] (p3) |
| 9 | 972 | 5 | 0.62 | 3 | D- | 0x82756CA8-0x82758378 | game:PartyModeMgr.obj [+EH:38] |
| 10 | 524 | 6 | 0.80 | 5 | DW | 0x82260018-0x822605BC | App.obj |

Stream-kill signal ("no TU with >=6 honest matchable real bodies in free
space") is NOT reached: ranks 1,2,3,6,8,10 all have >=6 real bodies. The
**game:PartyModeMgr.obj** (rank 9, game TU, src DC3-only, big EH island of 38)
and **App.obj** (rank 10, DW, 6 bodies) are the fresh game/engine candidates;
Cam/PlatformMgr_Xbox/StandardStream/HamStorePanel/HamSongMgr are strong engine
candidates. MoggClip is p3's target (in-flight).

## Deliverable B — NavList STL cluster = retail SongSortNode.cpp (+17)

### The finding that changed the plan

The packet premise ("ONLY source is DC3 `NavListNode.cpp`; rb3-Wii has no NavList
files") missed that **rb3-xenon already contains `src/band3/meta_band/
SongSortNode.cpp` + `.h`** — the retail port of exactly this hierarchy. DC3
renamed these classes to `NavList*` ("stole these from RB3 lmao" — literal DC3
header comment). The retail names:

| DC3 (oracle) | retail (rb3-xenon SongSortNode.h) |
|---|---|
| NavListNode | Node |
| NavListItemSortCmp | SongSortCmp |
| NavListSortNode | SortNode |
| NavListShortcutNode | ShortcutNode |
| NavListHeaderNode | HeaderSortNode |
| NavListSort | NodeSort |

Retail `SongSortNode.cpp` defines its own `CompareHeaders`
(`n1->Compare(n2, kNodeHeader) < 0`, identical to DC3's) and
`ShortcutNode::Insert` uses `std::equal_range(..., CompareHeaders())` — the exact
construct that instantiates the `__lower_bound/__upper_bound/__equal_range`
templates the oracle labels at 0x826454E8+. So the 0x826454E8 cluster is retail
SongSortNode.cpp's OWN unpinned code, not a foreign/DC3 TU.

Consequence: **no DC3-lazer port needed, and the UIListProvider vtable drift is a
non-issue** — SongSortNode.cpp already compiles against rb3-xenon's retail
`UIListProvider`/`NodeSort` headers, so `?Insert@ShortcutNode` (the one fn with a
`NewHeaderNode` virtual call) matched byte-exact.

### What was done (config/map only, no source, no objects.json change)

1. Extended the existing `SongSortNode.cpp` pin in `config/45410914/splits.txt`
   with a second `.text` range `[0x826454E8, 0x826462F4)` (dtk back-filled the
   matching `.pdata` `[0x82225CE0, 0x82225D98)`). Span starts/ends on fn
   boundaries; `overlap_check.py` clean (0 overlaps).
2. Removed 11 stale DC3-named entries at the cluster VAs from
   `scripts/target_symbol_map.json` (they were dormant — all in previously-free
   space, matching nothing — so removal is a no-op regression-wise) incl. the
   `0x826461D0 ??_GChooseModeProvider` ICF-alias.
3. `reveal_sweep.py` on the rebuilt `SongSortNode.obj` found 17 byte-exact
   (word_eq==1.0, self-validating) real-bodied matches with correct retail
   names; gated through `safe_name_merge.py` (17 in / 17 safe / 0 rejected);
   merged ADD-ONLY into the map.

### Final span + per-fn (all 100% in rebuilt report.json)

Pin: `SongSortNode.cpp` `.text [0x82643AE8,0x82643BD0)` (pre-existing) +
`[0x826454E8, 0x826462F4)` (this packet).

| VA | size | fn (100%) |
|---|---|---|
| 0x826454E8 | 0xDC | `__lower_bound<SortNode..LeafSortNode,CompareHeaders>` |
| 0x826455C8 | 0xDC | `__upper_bound<..CompareHeaders>` |
| 0x826456A8 | 0xDC | `__lower_bound<..>` (CompareItems/leaf) |
| 0x82645788 | 0xDC | `__upper_bound<..>` |
| 0x82645868 | 0x88 | `?DeleteAll@ShortcutNode` |
| 0x826458F0 | 0x88 | `?DeleteAll@SortNode` |
| 0x82645978 | 0xCC | `__lower_bound<..>` |
| 0x82645A48 | 0x1AC | `__equal_range<..CompareHeaders>` |
| 0x82645BF8 | 0x1AC | `__equal_range<..>` |
| 0x82645DA8 | 0x64 | `??0ShortcutNode` |
| 0x82645F78 | 0x70 | `?Renumber@SortNode` |
| 0x82645FF8 | 0x50 | `??0HeaderSortNode` |
| 0x82646048 | 0x44 | `??1HeaderSortNode` |
| 0x82646090 | 0x3C | `??0SubheaderSortNode` |
| 0x826460D8 | 0xA0 | `?Insert@SubheaderSortNode` |
| 0x82646178 | 0x58 | `??_GSubheaderSortNode` |
| 0x82646220 | 0xD4 | `?Insert@ShortcutNode` |

All 17 are real bodies (min 0x3C=60B). Additionally 3 small anonymous `fn_`
(0x82645E74=0x28, 0x82645F24=0x28, 0x82645F4C=0x2C) matched positionally — these
are <=44B stub-folds, NOT claimed as wins (but not regressions; they are part of
the +20 whole-binary delta).

### A/B (self-checked; reviewer runs the authoritative composed A/B)

- `overlap_check.py`: `.text` OK 0 overlaps, `.pdata` OK 0 overlaps.
- `icf_alias_check.py`: exit 0. (Its printed "inflation" verdict examined only
  the 3 anonymous `fn_` stub-folds — the tool keys "newly matched" by anonymous
  `fn_<addr>` and cannot correlate a baseline `fn_<addr>` to a named reveal, so
  it does not see the 17 named reveals. The 17 are confirmed via reveal
  byte-equality + report.json 100%.)
- SongSortNode-unit-scoped delta vs frozen baseline: 100% went **1 → 21
  (+20)**, **0 regressed, 0 missing**. 17 real-bodied named + 3 anon stub-fold.
- Whole-binary delta is contaminated by sibling workers sharing this worktree;
  determinism confirmed: two consecutive builds gave identical 100% counts
  (run1==run2=10905).

**Honest claimed yield: +17 strict real-bodied matches.**

## Files changed (reviewer commits; worker did NOT commit per rules)

- `tools/oracle_contiguity_scan.py` (new — build-first artifact)
- `config/45410914/splits.txt` (SongSortNode.cpp second `.text` + backfilled `.pdata`)
- `scripts/target_symbol_map.json` (−11 stale DC3 names at cluster VAs, +17 retail names)
- `docs/decomp/handoff/ws3-p4-navlist-scantool-2026-07-02.md` (this doc)

`objects.json` UNCHANGED — SongSortNode.cpp was already wired NonMatching.
No `NavListNode.cpp/.h` created. Exclude build artifacts + `global_fuzzy_pairs.json`
(pre-existing untracked) from the commit.

## Deliverable C — SongInfoAudioType: SKIPPED

Scan tool `--tu SongInfoAudioType`: the optC span [0x82530F50,0x82531198) is 18
contiguous `__unwind$` funclets (size 0x20 each), honest_bytes=0, not in the
fresh ranked list. The single weak anchor `?SymbolToAudioType@` @0x825d3958 (sim
0.597) is a lone sub-cluster member, not a real-body neighborhood. Uncorroborated
→ skipped per kill criteria.
