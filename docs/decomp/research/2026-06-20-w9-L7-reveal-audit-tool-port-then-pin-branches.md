# W9 L7 — reveal-audit-tool + port-then-pin reveal residue

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 7), READ-ONLY in main repo.
**Baseline:** main @812e1df, 8314 / 65544 matched. All W9 branches PENDING (unmerged).
**Frontier:** `reveal-audit-tool-port-then-pin-branches` (kind=tooling, est +40).
**Verdict:** **REAL_ACTIONABLE** — but the "build the generic tool" framing is
half-REFUTED: the generic byte-exact reveal tool **already exists**
(`tools/reveal_sweep.py`). The real, live lever is **running a SECOND post-port
reveal pass on each W9 port-then-pin branch's freshly-built obj**, which the
branches did not do (they captured 0–4 reveals; SongStatusMgr proved +10 are left
on the table per unit).

---

## What the frontier asked vs ground truth

> "Build the generic tool (byte-diff every UNMAPPED target fn against same-size own
> methods, emit byte-exact as reveal worklist)…"

**That tool exists and implements the exact dossier recipe.** `tools/reveal_sweep.py`:
- reads the retail target COFF (`build/obj/<unit>.obj`, `fn_/lbl_` unmapped) and our
  compiled base COFF (`build/src/.../<unit>.obj`, incl. STATIC class-3 with
  `--include-static`), paired per-unit via `objdiff.json`;
- size-buckets, then normalized word-equality compare masking reloc slots
  (`fuzzy_content_match.word_eq_frac`, big-endian instruction words; COFF header
  fields are LE for both sides — the dossier's "LE-COFF build/src vs BE-COFF
  build/obj" split is imprecise, the existing reader handles both uniformly);
- precision floors (MIN_SIZE 0x18, MIN_REAL_WORDS 5) + UNIQUE-1:1 gate to reject
  ICF/adjustor-thunk collisions;
- emits `target_symbol_map.json` fragment for `tools/safe_name_merge.py --gate`
  (the ICF/collision/non-real gate), self-validating (a wrong addr cannot read
  byte-exact at 100%).

So **WI to "build" the tool is unnecessary**. The frontier's real value is the
SECOND clause: "run it across all in-flight W9 port-then-pin branches." That is
live and unexecuted.

## Why a SECOND reveal pass matters (the SongStatusMgr proof)

The reveal-vs-port asymmetry, from the landed precedent:
- `w9-land-songstatusmgr-hashmap-pin-evict` (5edc67f): hash_map re-layout +
  find-accessor port + evict dead MoggClip orphan pin = **+34**, maps only the
  find-accessors.
- `w9-songstatusmgr-base-land-plus-reveal` (bd9705b): on TOP of +34, reveal **10**
  already-byte-exact own-unit methods (GetTotalSongs, ??0SongStatus,
  SaveToStream, SaveFixed, LoadFixed, GetBestSongStatusFlag, GetSongStatusFlag,
  UpdateCachedTotalScore, Clear, ??_E vector-dtor thunk) + fix 3 star-caps
  5000→15000 = **+10**, and the commit notes "all 27 remaining unmapped fn_ are
  own-unit reveals in [0x825B8058,0x825BA440)" — i.e. MORE still left.

**Mechanism:** the port-then-pin step changes the compiled obj. Methods that were
NOT byte-exact pre-port (so reveal_sweep at main-baseline skipped them) become
byte-exact AFTER the port lands. reveal_sweep run once against main's pre-port obj
cannot see them. A second pass against the post-port obj harvests them. This is
the same two-stage pattern (+N port, then +M reveal) every port-then-pin branch
underran.

## Per-branch reveal-residue audit (read-only COFF + commit inspection)

All branches PENDING vs main@8314. Map-entry counts below are NET-NEW curated
reveals (the 2846/12508 deltas on SongUpgradeMgr/LicenseMgr are
`gen_target_map` regenerations of the whole bindiff map, NOT curated reveals).

| branch (head) | gain | curated reveals added | residue signal |
|---|---|---|---|
| w9-land-songsortmgr-port-then-pin (dc30ed0) | +78 | **3** (ClearInternalSetlists, operator>>, _Rb_tree erase) | 78/168 matched → **90 fns <100**; only 1 reveal pass at main-baseline |
| w9-bandsongmgr-port-then-pin (46f77f3) | +6 | **0** | NO reveal pass run; small gain ⇒ pin barely scratches the β cluster |
| w9-songupgrademgr-wire-pin-port (52eccdb) | +39 | 0 curated (map regen only) | post-port reveal unrun |
| w9-licensemgr-pin-port-reconstruct (190de75) | +27 | 0 curated (map regen only) | post-port reveal unrun |
| w9-w9-fsss-residual-bodyports (e4871c2) | +2 | 0 | target-boundary fix, not a reveal; FSSS has 8/11 — recheck |
| w9-songstatusmgr-base-land-plus-reveal (bd9705b) | +10 (on +34) | 10 | **DONE pattern** — and even it notes 27 fn_ residue remaining |

SongSortMgr's `w9-songsortmgr-port-then-pin` (8d4d72c) is byte-identical to the
`w9-land-` mirror (dc30ed0) — same branch, two names. SongUpgradeMgr has TWO
divergent branches (52eccdb +39 vs 2a962d4 +24) with DIFFERENT pins — a partition
conflict, see below.

## CRITICAL: the 0x82630xxx–0x82632xxx pin partition is CONTESTED across 4 branches

These branches claim OVERLAPPING `.text` spans in the same dense anon region
(verified against `auto_03_82260000_text.obj`, fn layout dumped). They **cannot
all land** — they pin the same VAs:

| branch | pinned .text |
|---|---|
| BandSongMgr | `[0x82631298, 0x82631FD0)` |
| SongUpgradeMgr v1 (52eccdb, +39) | `[0x82630988, 0x82632040)` — **swallows BandSongMgr** |
| SongUpgradeMgr v2 (2a962d4, +24) | `[0x82630A98, 0x82632C98)` — swallows BandSongMgr AND overlaps LicenseMgr |
| LicenseMgr | `[0x82632040, 0x82632F00)` |

This is the roadmap-β multi-class gap ("GamePanel/CharMeshHide/LicenseMgr slivers
precede the cluster"). The reveal-audit on these THREE units (BandSongMgr,
SongUpgradeMgr, LicenseMgr) is **GATED on first resolving the partition** — you
cannot reveal own-unit methods until each unit's correct VA span is settled. A
reveal pass on a wrong/overlapping pin will mis-attribute foreign fns. ⚠
attribution_risk on all three.

SongSortMgr (`[0x82580040,0x82583DD8)`) and SongStatusMgr
(`[0x825B8058,0x825BA440)`) are in a DIFFERENT region — NO partition conflict,
clean reveal targets.

## Tooling state (all present, no build needed)

- `tools/reveal_sweep.py --units <substr> --include-static --emit-fragment frag.json`
- `tools/safe_name_merge.py --gate frag.json --out safe.json` (ICF/non-real gate)
- merge `safe.json` into `scripts/target_symbol_map.json`,
  `rm build/45410914/target_symbol_renames.stamp && touch config/45410914/config.yml && ninja`,
  keep ONLY entries landing at 100% in the rebuilt report.json.
- `scripts/setup_worktree.sh`, `tools/fresh_report.sh` for A/B.

## Self-containment constraint

Every W9 port-then-pin branch is PENDING vs main@8314. A reveal-on-top is NOT
independent of its base port. Each work-item MUST, in ONE worktree:
1. rebase the worktree branch onto (or cherry-pick) the prerequisite port-then-pin
   commit, so the post-port obj exists;
2. build; run reveal_sweep on THAT obj; gate; merge fragment; rebuild;
3. keep only 100%-landing reveals; whole-binary A/B vs main@8314 (the net must ==
   base-port gain + reveal gain).

The deliverable patch therefore contains BOTH the port-then-pin and the reveal
map-adds, landing as one independent unit vs main@8314.

## Verdict

REAL_ACTIONABLE. Tool exists; the lever is the unrun second-pass reveal across
the 5 W9 branches. Clean targets first (SongSortMgr +6–10, SongStatusMgr-residue
+2–5). The 0x8263x trio (BandSongMgr/SongUpgradeMgr/LicenseMgr) is reveal-rich but
partition-gated — emit as a recon-gated item + a discovered_frontier for the
partition resolution itself.
