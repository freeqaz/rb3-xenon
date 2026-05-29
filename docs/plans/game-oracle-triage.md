# Whole-game identification triage (RB3-Wii oracle)

**Date:** 2026-05-29. **Status:** identification artifact, **read-only w.r.t. the
build** (no `ninja`, no `splits.txt`/`objects.json` edits, no commit to build
inputs). Tool: `tools/game_oracle_triage.py`. Input: `unified_id_rb3wii.json`
(the RB3-Wii BinDiff oracle from `docs/plans/bindiff-vs-rb3wii.md`).

## What this is and why

`bindiff-vs-rb3wii.md` proved the RB3-Wii oracle locates game TUs at 10–88%
density (vs DC3's 1–8%) but only triaged **meta_band** (26 TUs). The oracle
actually covers the **whole game tree** — 9,301 located fns across **703 TUs** —
and the doc itself flagged that it is "also a porting-priority map … it shows
which RB3-Wii game TUs have a recoverable RB3-360 home before any porting starts."
That map had never been produced. This is it.

`tools/game_oracle_triage.py` computes, for **every** located game TU: the
dominant contiguous cluster (8 KB gap-split), its density and purity, the
candidate `.text` span snapped to `config/45410914/symbols.txt`, the count of
high-confidence individual IDs (sim≥0.8 & conf≥0.9), and cross-references local
availability (WIRED in objects.json / PRESENT in `src/` / PORTABLE from `../rb3`).
Run: `python3 tools/game_oracle_triage.py [--json OUT] [--area band3/game]`.

> **The tool reproduces the doc's hand-derived meta_band spans byte-for-byte**
> (e.g. `Accomplishment.cpp 0x8243A18C-0x8243A614`), which validates the logic;
> spans are `[start, exclusive-end)` snapping to real `symbols.txt` boundaries.

## ⚠️ Pinning ≠ matching — the load-bearing caveat

This is an **identification/localization** map, not a list of free matches.
Per `[[project-rb3-xenon-roadmap]]` (CURRENT STATE) and
`[[project-game-code-instrumentation]]`, game matching is gated on **two** things
this triage does **not** solve, both owned by the pairing-stream worktrees:

1. **Pairing** — the renamer only auto-pairs at ≥0.95 BinDiff confidence; game
   oracle hits are ~0.62, so a pinned game TU shows a pure **0%** artifact until
   `fn_<addr> → MSVC-mangled` map entries are added (manual demangle-match).
   Pipeline: `tools/gen_game_target_map.py` + `docs/plans/game-code-pairing.md`.
2. **Retail coverage/PGO instrumentation** — trivial game accessors carry a
   32-byte per-function bit-poke prologue our 8-byte `/O1` build lacks → 17–35%,
   never 100%. Mechanically patchable (one stereotyped pattern) but unsolved.
   See `[[project-game-code-instrumentation]]`.

So the value of this triage is: it gives the pairing stream and the porting
stream **concrete targets ranked by recoverable yield**. Pinning a Bucket-A span
today = +0 matched until (1) and (2) land — but it is the prerequisite that makes
the target obj exist for the pairing experiment to run against.

## The buckets (live numbers, 2026-05-29)

```
A (wired+pinnable):     8 TUs /   78 fns -- pin NOW (feed pairing stream)
B (present+pinnable):   0 TUs /    0 fns
C (portable+pinnable): 37 TUs /  217 fns -- port+pin queue (the NEW breadth)
D (scatter):          658 TUs / 1486 dom fns -- needs better localization
```

A TU is **pinnable** when its dominant cluster has ≥3 fns, purity ≥70%, and
(density ≥15% **or** dom ≥8 — a large high-purity run is worth pinning even when
sparse). Purity is the real collision gate; density only screens tiny loose
hulls. (Density >100% just means the cluster's avg fn < 40 B — tiny accessors.)

### Bucket A — WIRED + PINNABLE (already compiles; ready-to-pin spans)

These 8 meta_band TUs are wired in `objects.json` and compile today; pinning is
pure span-derivation. Feed these spans to the pairing stream.

```
band3/meta_band/Accomplishment.cpp                    .text 0x8243A18C-0x8243A614  # 22 fns pur88% (contam SessionMessages:3)
band3/meta_band/AccomplishmentProgress.cpp            .text 0x8243854C-0x8243A12C  # 19 fns pur79% (contam VocalTrack:3,NetSession:1)
band3/meta_band/AccomplishmentManager.cpp             .text 0x8248BBA4-0x8248CB98  # 13 fns pur72% (contam BandProfile:3)
band3/meta_band/UIEvent.cpp                           .text 0x825519DC-0x82551B5C  #  8 fns pur100%
band3/meta_band/AccomplishmentSetlist.cpp             .text 0x8243F220-0x8243F330  #  5 fns pur83%
band3/meta_band/AccomplishmentSongFilterConditional.cpp .text 0x8243F378-0x8243F418 # 4 fns pur100%
band3/meta_band/AccomplishmentPlayerConditional.cpp   .text 0x8243F178-0x8243F220  #  4 fns pur100%
band3/meta_band/AccomplishmentCategory.cpp            .text 0x8243EF98-0x8243EFF8  #  3 fns pur100%
```

(The 3 sub-100%-purity spans interleave a few neighbour-TU fns; prefer to
start/end on a confirmed same-TU boundary and let dtk back-fill, per
`bindiff-vs-rb3wii.md` caveats.)

### Bucket C — PORTABLE + PINNABLE (port+pin queue, ranked by mass)

37 TUs / 217 dom fns whose Wii source exists in `../rb3` and whose dominant
cluster is cleanly pinnable. **This is the new breadth — none of this was
triaged before.** Top of the queue (full list: `tools/game_oracle_triage.py`):

```
band3/game/VocalPlayer.cpp        35 fns dens18.5% pur74% hiC4  0x8242057C-0x82422320  ← biggest single C target
band3/meta_band/MetaPerformer.cpp 13 fns          pur72% hiC1  0x824EE458-0x824EFE44
band3/game/BandUser.cpp           13 fns          pur81% hiC2  0x823934A8-0x82395714
band3/meta_band/BandSongMetadata.cpp 12 fns       pur86%       0x8249BC4C-0x8249CB78
band3/game/TrackerManager.cpp     11 fns dens68.8% pur100%     0x825CAF54-0x825CB1D4
band3/game/NetGameMsgs.cpp         8 fns          pur100% hiC1 0x827C3B54-0x827C4760
band3/game/AccuracyTracker.cpp     7 fns          pur88%       0x8234F69C-0x8234F79C
band3/game/Singer.cpp              7 fns          pur78% hiC3  0x824020B8-0x824021F8
network/net/SessionMessages.cpp    7 fns          pur100%      0x8265AB58-0x8265B0EC
band3/game/VocalTrainerPanel.cpp   6 fns          pur100%      0x824250D8-0x82425198
network/Protocol/Protocol.cpp      6 fns          pur86%  hiC1 0x822F4E84-0x822F4F64
... (+26 more across band3/game, band3/tour, band3/bandtrack, network/Core, meta_band)
```

### Bucket D — located but NOT pinnable (scatter)

658 TUs / 1486 dom fns. These carry the **bulk of the located mass** but the
oracle's address-sequence propagation fragmented them into 50–86 clusters each,
so no single tight span exists. The biggest by *total* located fns:

```
band3/game/GemPlayer.cpp           169 located, dom 15 @1.8% pur42% (86 clusters)
band3/game/VocalPlayer.cpp         167 located  (dominant cluster IS pinnable -> in C)
band3/bandtrack/VocalTrack.cpp     139 located, dom 12 @6.6% pur52% (76 clusters)
band3/net_band/RockCentral.cpp     135 located, dom 22 @9.1% pur58% (68 clusters)
band3/meta_band/ProfileMgr.cpp     108 located, dom 34 @4.2% pur47% (54 clusters)
band3/game/Game.cpp                103 located, dom 12 @1.9% pur41% (64 clusters)
```

These are *identified* (we know the addresses belong to these TUs) but need
**better localization** before they can be pinned as a unit — e.g. tighten the
BinDiff (per-function name transfer rather than span propagation), or pin the
dominant sub-cluster + extend once pairing reveals which neighbours objdiff wants.

## Whole-tree availability — the porting roadmap

Across all 703 located game TUs / 9,301 located fns:

```
wired              43 TUs / 1016 fns   already compile (the meta_band ports)
present_unwired     1 TU  /   37 fns
portable_from_wii 284 TUs / 5052 fns   Wii source exists -> PORTING BACKLOG
no_local_source   375 TUs / 3196 fns   (see below)
```

**Portable-from-Wii backlog by area (284 TUs / 5052 fns)** — where porting effort
buys the most located mass:

```
1795 fns  103 TUs  band3/meta_band   (playbook: docs/plans/meta_band-port-breaking-changes.md)
1782 fns   67 TUs  band3/game        (GemPlayer, VocalPlayer, Game, Singer, trackers)
 408 fns   12 TUs  band3/bandtrack   (VocalTrack, GemManager, Lyric, Tail)
 380 fns   22 TUs  network/net       (NetSession family — Wii source DOES exist)
 255 fns   22 TUs  band3/tour
 216 fns   26 TUs  network/Core
 140 fns   19 TUs  network/ObjDup
  44 fns    9 TUs  band3/net_band    (RockCentral)
```

**`no_local_source` (375 TUs / 3196 fns) — two causes:**
- **330 TUs / 2727 fns are `network/*`** = the **Quazal / Rendez-vous networking
  stack** (`ObjDup/Station`, `Plugins/Transport`, `Services/MatchMaking`,
  `Extensions/DupSpace`, …). The Wii decomp never produced these (third-party
  Quazal NetZ). Genuinely no transferable source — effectively infeasible without
  from-scratch RE or the Quazal SDK. **Treat as a no-source ceiling for network**
  (the network analogue of the XDK ceiling on the engine side).
- **45 TUs / 469 fns are `band3/*`**, mostly files the Wii repo places at its
  **repo root** (`../rb3/src/BandOffline.cpp`, `BudgetScreen.cpp`) rather than the
  DWARF subpath (`band3/src/band/…`) — these are *portable at a different path*,
  a slight undercount in `portable_from_wii`. Reconcile by basename when porting.

## Best force-pair anchor TUs (for the pairing experiment)

TUs with ≥3 high-confidence individual structural IDs (sim≥0.8 & conf≥0.9) — the
*safest* targets for the pairing stream's "force-pair one TU, read the fuzzy %"
experiment, because we trust the per-function name mapping:

```
band3/game/VocalPlayer.cpp   hiC4  dom35 pur74%  0x8242057C-0x82422320
band3/game/Singer.cpp        hiC3  dom7  pur78%  0x824020B8-0x824021F8
band3/game/TrainerPanel.cpp  hiC3  dom5  pur100% 0x8241B6D0-0x8241B7E8
band3/game/BandUserMgr.cpp   hiC3  dom3  pur100% 0x82398108-0x82398168
band3/bandtrack/Lyric.cpp    hiC3  dom3  pur100% 0x8236FACC-0x8236FB7C
```

## Recommended use (division of labour)

- **Pairing stream (worktrees `pair-experiment`/`pair-pipeline`):** Bucket A
  spans give ready target objs to pair against; the force-pair anchors above are
  the highest-confidence first experiments. Their findings (instrumentation
  blocker, demangle-match) gate whether any of this ticks the counter.
- **Porting stream (band3 port+pin wave, next-levers Lever #3):** Bucket C is the
  ranked port+pin queue with target spans up front; the area table is the
  longer-horizon porting roadmap. Port files that have a high-purity recoverable
  home first.
- **This file is read-only on the build.** Pinning/wiring is a separate
  build-touching session (single `.ninja-build.lock` writer; the pairing
  worktrees currently hold `splits.txt`/`target_symbol_map.json`).

## Files / refs

- `tools/game_oracle_triage.py` — the tool (committed). `--json` dumps the full
  703-TU table; `--area` filters.
- `unified_id_rb3wii.json` — input oracle (repo root, gitignored, regenerable
  per `docs/plans/bindiff-vs-rb3wii.md`).
- Related: `docs/plans/bindiff-vs-rb3wii.md` (oracle build),
  `docs/plans/game-code-anchoring.md`, `docs/plans/next-levers-2026-05-29.md`
  (§Q2 game priority), `docs/plans/meta_band-port-breaking-changes.md` (Wii→360
  playbook). Memories: `[[project-game-code-instrumentation]]`,
  `[[project-rb3-xenon-roadmap]]`, `[[bindiff-vs-rb3wii]]`.
