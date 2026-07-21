# Fingerprint carve — BATCH 8 (fresh lower-threshold repin census)

Author: batch-8 repin-census lead. Baseline: main @ **21,118** strict
(worktree clean-build baseline **21,117**; A/B is vs the worktree baseline).

The batch-6/7 truncated-pin repin mechanism ran at 12–30 flips/wkr-min while its
enumerated list lasted; batch-7 consumed the list. This is a FRESH census with a
lower threshold (down to ~256 B extensions, floor 0.30 attribution) over the
~2.17 MB of still-unpinned MAIN-region (82270000–82840000) fingerprint mass.

## TL;DR

- **Census tool committed:** `scripts/harvest/repin_census.py` (worktree branch
  `repin`). Read-only; ranks by **authoritative dtk target-body count**, not the
  unreliable fingerprints.json boundary (see calibration).
- **297 attributed repin candidates** in MAIN (up from batch-7's drained list).
  dtkBIG ceiling sum = **5,403** target bodies; **99 high-confidence** cands
  (score ≥ 0.60, dtkBIG ≥ 15).
- **Proof repin landed (#1): BeatMatcher.cpp tail `82791B60..82793908` → +27
  strict, zero-lost** (21,117 → 21,144). Commit on branch `repin`.
- **Realistic capture ≈ 0.41 × dtkBIG** (single-point calibration from the proof:
  +27 measured / 66 dtkBIG). Top-20 est ≈ **785**; whole-list est ≈ 3,779 (upper-ish
  — shrinks for already-matched / divergent / funclet-dense spans).
- Quazal `net/` subgroup is **already fully carved** (drained by batch-5); the
  82A–82B Quazal mass is COMDAT-scatter (DuplicatedObject = 12 frags over 600 KB),
  NOT clean-extend material — needs scatter-wiring / `/Od` flag work, not repin.

## Method

For every pinned TU in `splits.txt`, enumerate maximal unpinned fingerprint runs
that are **adjacent** (fwd/back, gap ≤ a few bytes) or in an **inter-fragment
gap** of an already-pinned TU, then attribute each run to its owner by string
membership in the owner's source text (own tree → rb3-Wii → dc3 oracle).

- **dir**: `FORWARD` = run sits right AFTER a pin (extend that TU's `.text` end);
  `BACKWARD` = run sits right BEFORE a pin (extend start — the ShaderMgr shape);
  `INTERNAL` = run in a gap between two fragments of one TU (merge/fill).
- **dtkBIG**: authoritative count of target functions ≥ 0x40 B that dtk emits in
  the run span (parsed from build asm). This is the automap `--span` EXACT-density
  ceiling = the honest "expected free yield" upper bound, and the **primary rank
  key**. NOTE fingerprints.json boundaries diverge from dtk in some regions
  (MusicLibrary: 11 fp-big vs **78** dtk-big) — always trust dtkBIG.
- **est**: ~0.41 × dtkBIG (proof calibration).
- **scr**: fraction of run strings found in owner source (attribution confidence).
- **oth**: the *other* neighbour's score = interleave/ambiguity risk. `oth ≈ scr`
  with the **same** TU on both sides ⇒ clean inter-fragment fill (high confidence).
  `oth ≪ scr` ⇒ unambiguous single-owner directional under-carve. `oth ≈ scr` from
  a *different* TU ⇒ shared-vocabulary ambiguity (verify before pinning).
- **nfr**: owner fragment count (high = more interleave, e.g. RockCentral = 56).

### Repin recipe (per candidate) — proven on BeatMatcher

1. Add `.text start:<run_start> end:<run_end>` to the owner TU block in `splits.txt`.
2. `touch config/45410914/config.yml && ./tools/ninja-locked` (dtk emits the
   extended target obj+asm; auto-derives the matching `.pdata`).
3. `size_order_automap.py --unit <TU> --emit frag.json` → recovers fn_VA↔mangled.
4. `tu5_map_apply_fragment.py frag.json scripts/target_symbol_map.json`.
5. `touch config.yml`, `rm -f build/45410914/report.cache`, `fresh_report.sh` → Δstrict.

### Ghidra TU-boundary validation (default_tu5.xex-c5a170)

Spot-checked the top candidates — owner-class named methods **bracket every run
exactly**, confirming TU-identity and clean boundaries:
- BandUser `8268b1f0..8268c920`: `ProfileName@8268b148` → run → `SetLoadedPrefabChar@8268cc20`.
- MetaPerformer `82580820..82582dfc`: `SetBattle@825807b8` → run → `Handle@82582e08`.
- MetaPerformer `8257d5d0..8257e058`: run end = `OnSynchronized@8257e058`.
- VocalPart `826f0f20..826f2144`: run end = `GetNoteSliceWeight@826f2148`.
- MusicLibrary cluster `8253b..8254x` brackets the `82541cc0..82545688` run (scr 0.96).

String attribution is accurate; no boundary corrections needed for the top tier.

### Stale `__unwind$` blocker sweep (top-60 spans)

Genuine `__unwind$NNNNN` map entries inside candidate spans (a known repin
blocker class) are **sparse** — only: RockCentral `82507634..8250b12c` (3),
MetaPerformer `82580820..82582dfc` (2), and one each in BandUser `8268b1f0..`,
BandCharacter `8228732c..`, BandTrack `8234d778..`, StorePreviewMgr `827b1d60..`,
GemPlayer `826c2780..`. (The many `?$…$…` hits are legit STL template names =
dormant activatable, not blockers.) Clear these before pinning RockCentral/
MetaPerformer; the rest are one-offs.

## Ranked candidate list (MAIN region)

### Game / meta_band / band3 layer (top 30)

| TU | dir | run span | dtkBIG | est | scr | oth | nfr |
|---|---|---|--:|--:|--:|--:|--:|
| Game.cpp | FORWARD | `82679868..8267c738` | 86 | ~60 | 0.71 | 0.71 | 7 |
| RockCentral.cpp | FORWARD | `82507634..8250b12c` | 84 | ~59 | 0.71 | 0.07 | 56 |
| MusicLibrary.cpp | BACKWARD | `82541cc0..82545688` | 78 | ~55 | 0.96 | 0.01 | 2 |
| RockCentral.cpp | FORWARD | `82500688..8250527c` | 70 | ~49 | 0.86 | 0.86 | 56 |
| OvershellSlot.cpp | FORWARD | `825dafe0..825de034` | 61 | ~43 | 0.82 | 0.82 | 13 |
| BandUser.cpp | FORWARD | `8268b1f0..8268c920` | 55 | ~38 | 0.80 |  | 7 |
| BandTrack.cpp | FORWARD | `8234d778..8234f4f8` | 53 | ~37 | 0.86 | 0.86 | 9 |
| BandCharacter.cpp | FORWARD | `8228a988..8228cb04` | 52 | ~36 | 0.88 | 0.16 | 6 |
| CustomizePanel.cpp | BACKWARD | `82614690..82616798` | 50 | ~35 | 0.82 | 0.02 | 3 |
| CharacterCreatorPanel.cpp | BACKWARD | `8260e70c..826107f8` | 50 | ~35 | 0.82 | 0.12 | 2 |
| MetaPerformer.cpp | FORWARD | `82580820..82582dfc` | 47 | ~33 | 0.78 | 0.78 | 7 |
| OvershellSlot.cpp | FORWARD | `825de078..825e0320` | 47 | ~33 | 0.91 | 0.91 | 13 |
| BandCharacter.cpp | BACKWARD | `8228732c..8228a23c` | 46 | ~32 | 0.71 | 0.10 | 6 |
| ProfileMgr.cpp | FORWARD | `82547668..8254911c` | 46 | ~32 | 0.75 | 0.75 | 2 |
| NextSongPanel.cpp | BACKWARD | `82643958..82644e30` | 43 | ~30 | 0.87 | 0.07 | 4 |
| UI.cpp | FORWARD | `82803494..82804df4` | 40 | ~28 | 1.00 | 1.00 | 6 |
| RockCentral.cpp | FORWARD | `8250577c..82507288` | 39 | ~27 | 0.85 | 0.85 | 56 |
| Game.cpp | FORWARD | `82677410..82678cfc` | 39 | ~27 | 0.79 | 0.79 | 7 |
| BandCharDesc.cpp | FORWARD | `82336e58..82338d1c` | 37 | ~26 | 0.96 | 0.96 | 8 |
| GemPlayer.cpp | FORWARD | `826c5ae4..826c75dc` | 37 | ~26 | 0.62 |  | 22 |
| MainHubMessageProvider.cpp | FORWARD | `82672770..82673334` | 35 | ~24 | 0.94 |  | 1 |
| MainHubPanel.cpp | FORWARD | `826202c8..82621a14` | 34 | ~24 | 0.78 | 0.78 | 3 |
| BandCharDesc.cpp | BACKWARD | `82339b10..8233b19c` | 34 | ~24 | 0.85 | 0.05 | 8 |
| Game.cpp | FORWARD | `8267d480..8267f354` | 33 | ~23 | 0.92 | 0.02 | 7 |
| ProfileMgr.cpp | FORWARD | `82545e90..82546a50` | 31 | ~22 | 1.00 | 1.00 | 6 |
| CustomizePanel.cpp | FORWARD | `82616810..82617cb0` | 30 | ~21 | 0.86 | 0.86 | 3 |
| GemPlayer.cpp | FORWARD | `826beee0..826c0234` | 29 | ~20 | 0.75 | 0.75 | 22 |
| OutfitConfig.cpp | FORWARD | `8229ff30..822a0ed4` | 29 | ~20 | 0.90 |  | 1 |
| GemPlayer.cpp | FORWARD | `826c2780..826c3d08` | 28 | ~20 | 0.86 | 0.86 | 22 |
| MainHubPanel.cpp | FORWARD | `82621ac0..82622f18` | 28 | ~20 | 0.75 | 0.75 | 3 |

### Engine layer (top 15)

| TU | dir | run span | dtkBIG | est | scr | oth | nfr |
|---|---|---|--:|--:|--:|--:|--:|
| Rnd_Xbox.cpp | BACKWARD | `827378b8..82739400` | 66 | ~46 | 0.75 | 0.62 | 5 |
| MasterAudio.cpp | FORWARD | `8277b828..8277cabc` | 44 | ~31 | 0.86 | 0.86 | 5 |
| CharCuff.cpp | FORWARD | `8239d518..8239f7a4` | 39 | ~27 | 0.70 | 0.05 | 3 |
| UIFontImporter.cpp | FORWARD | `8281a8d0..8281d074` | 34 | ~24 | 1.00 |  | 4 |
| UIFontImporter.cpp | BACKWARD | `82817c20..82818eb8` | 34 | ~24 | 1.00 | 0.53 | 4 |
| VocalPart.cpp | BACKWARD | `826f0f20..826f2144` | 33 | ~23 | 0.93 |  | 1 |
| Trans.cpp | FORWARD | `823faaa0..823fbde8` | 31 | ~22 | 0.73 |  | 12 |
| UIFontImporter.cpp | FORWARD | `82818f18..8281a838` | 30 | ~21 | 0.90 | 0.90 | 4 |
| Splash.cpp | FORWARD | `82742a80..82743a38` | 30 | ~21 | 1.00 |  | 2 |
| Lit.cpp | FORWARD | `82498d90..8249a120` | 29 | ~20 | 0.88 | 0.06 | 5 |
| BeatMatchController.cpp | FORWARD | `8278fbc0..82790d00` | 28 | ~20 | 0.73 | 0.18 | 6 |
| UIPicture.cpp | FORWARD | `82816f20..82817a64` | 28 | ~20 | 1.00 |  | 1 |
| DataFunc.cpp | FORWARD | `827639c0..82765b40` | 27 | ~19 | 0.95 | 0.14 | 11 |
| Env.cpp | FORWARD | `82409ac4..8240b0a0` | 25 | ~18 | 0.93 | 0.02 | 10 |
| SongParser.cpp | FORWARD | `82782b28..82783dc4` | 25 | ~18 | 1.00 | 1.00 | 7 |

## Interleave / risk guidance for the executor

- **RockCentral.cpp (nfr=56)**: three large runs (dtkBIG 84/70/39) but it is
  network `/Od` funclet-dense AND carries stale `__unwind$` entries. High ceiling,
  high friction — attempt AFTER the clean tier, and clear `__unwind$` first.
- **`oth ≈ scr` same-TU rows** (Game 82679868, OvershellSlot, BandTrack, UI,
  MetaPerformer, ProfileMgr 82545e90, MainHubPanel, GemPlayer, CustomizePanel
  82616810): safest — the run is genuinely enclosed by one TU's fragments.
- **`oth ≪ scr` rows** (MusicLibrary 0.96/0.01, CustomizePanel 0.82/0.02, Game
  8267d480 0.92/0.02, NextSongPanel 0.87/0.07, BandCharacter 8228732c 0.71/0.10):
  unambiguous single-owner directional under-carve. Also safe.
- **High nfr + moderate scr** (GemPlayer nfr=22, OvershellSlot nfr=13): fragment
  merges — expect the automap to pair a subset; net still positive per batch-7
  (TrackWatcherImpl 14-frag merge = +37).
- **est is a single-point calibration.** Gate on measured Δstrict > 0 with zero
  losses, not on hitting est. Some spans are already partly matched (dormant map
  entries) → over-perform; divergent bodies → under-perform.

## Secondary: Quazal / network TUs

18 network-group TUs. Two regions, two verdicts:

**`net/` subgroup (MAIN region, plain `/O1`) — DRAINED.** NetSession
`823E6F60..823E9158`, Synchronize `823E9158..823E9750`, SyncStore
`823E9750..823E9C70`, Server, SessionMessages, NetworkEmulator are pinned
**back-to-back with zero gaps** — the region census returns 0 candidates. Batch-5's
NetSession +40 already took this lane. No repin left here.

**Quazal core (82A–82B region, `Platform/`=`/Od /Oi- /EHs-c- /Ob1`, others `/O1`)
— NOT clean-extend.** These are COMDAT-scatter, not truncated pins:

| TU | pin span | frags | pinned B | note |
|---|---|--:|--:|---|
| DuplicatedObject.cpp | `82a70400..82afc2dc` | 12 | 7012 | 600 KB envelope = mostly foreign TUs (ObjDup scatter) |
| MD5.cpp | `82b438c8..82b4547c` | 1 | 7092 | single frag, likely complete |
| NetSession.cpp | `823e6f60..823e9158` | 1 | 8696 | drained |
| MemoryManager.cpp | `82a6d428..82a6dc40` | 1 | 2072 | Platform `/Od` |
| Scheduler.cpp | `82ac57f0..82ac7948` | 3 | 1588 | Core |
| SessionDiscoveryTable / KeyedChecksumAlgorithm / BandwidthCounter / ChecksumAlgorithm / StringConversion | small | 1 | <700 | mostly complete |
| Core/TimedSignal.cpp, Core/PeriodicJob.cpp | — | 0 | 0 | wired but UNPINNED (no target span) |

The 82A–82B census (region `0x82840000,0x82c00000`) surfaces almost no
network-attributed clean runs — the "~400 KB carvable" batch-7 seed is dominated
by scatter (DuplicatedObject/ObjDup) that the repin mechanism does NOT address.
**Route Quazal to the scatter-wiring / `/Od`-flag lane, not batch-8 repin.** The
only near-term repin-shaped network item is TimedSignal/PeriodicJob (unpinned,
need a fresh span pin + gen_game_target_map, not an extension).

## Reproduce

```
# full MAIN census (ranked), with authoritative dtk yield from a fresh worktree build:
python3 scripts/harvest/repin_census.py \
    --build-dir /home/free/tmp/wt-repin/build/45410914 \
    --json /home/free/tmp/repin_batch8.json
# Quazal region:
python3 scripts/harvest/repin_census.py --region 0x82840000,0x82c00000 --floor 0.3
```

JSON with all 297 rows (full strings, gaps, dtk counts) at
`/home/free/tmp/repin_batch8.json`.
