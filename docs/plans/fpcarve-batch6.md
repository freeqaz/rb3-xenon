# Fingerprint carve — BATCH 6 (round-4 FIRST-PRINCIPLES census)

Author: round-4 identification lead. Mandate: full re-census of ALL remaining
unpinned `.text` mass (not seed-list iteration), per batch-5's recommendation
that the named-seed vein was exhausted.

**Baseline:** main @ 20,080 strict. splits.txt = 2,477 `.text` pins, 3.84 MB pinned.
**Total `.text`:** 9.86 MB / 67,285 fns. **Unpinned remainder: 5.93 MB / ~44k fns.**

Method: fresh `fp2_runs`/`fp2_span` + a rebuilt oracle string index (38,701
literals from `../rb3/src` + `../dc3-decomp/src`), regional density map, and a
truncated-pin under-carve scan. Tooling: `/tmp/census2.py`, `/tmp/attribute_runs.py`,
`/tmp/clean_extend.py`, `/tmp/oracle_index.py` (copy into `scripts/harvest/` if kept).

---

## 1. STRATEGIC REMAINING-MASS CLASS TABLE  (the denominator)

The binary splits into 5 contiguous VA regions. Unpinned mass by class:

| class | region(s) | unpinned | fns | oracle | verdict |
|---|---|---:|---:|---|---|
| **VENDOR-XDK (no source)** | `82840000–82a40000` | **1.99 MB** | 4,663 | NONE | **DEAD — do not chase** |
| **MAIN game+engine (workable)** | `82270000–82840000` | **2.31 MB** | 23,464 | game+dc3+wii | frontier (see split below) |
| **QUAZAL-NET (partial oracle)** | `82a40000–82b40000` | **0.97 MB** | 5,895 | thin/partial | mixed (see below) |
| **TAIL (engine+audio+funclet)** | `82bc0000–end` | 0.48 MB | 3,179 | Data.h engine + XMA vendor | low, mostly funclet/vendor |
| **ENGINE-mid** | `82b40000–82bc0000` | 0.18 MB | 1,251 | dc3 | ~70% already pinned |

### VENDOR-XDK is confirmed dead (1.99 MB, biggest single chunk)
Region `82840000–82a40000` is the **D3D/XGraphics shader compiler** (`pAS_Object`,
`e:\xenon\xdk-main-feb10\...\ucode\ssm\`, `unable to unroll a loop`,
`FMT_32_FLOAT`, `pShaderStore`) + **libjpeg** (`jerror.h`) + **tomcrypt**. Probed
every string-bearing fn against the oracle index with a ≥3-hit threshold:
**0 KB carvable non-vendor content.** 0.91 MB no-string + 1.08 MB vendor. This is
1/3 of all remaining unpinned mass and it is unmatchable — the coordinator should
formally **exclude it from the denominator** (it is what keeps "Progress %" low).

### MAIN 2.31 MB — the workable frontier, split game vs engine
Of the 23,464 unpinned MAIN fns, only 2,913 carry strings (the ID-able carve
targets); ~20,551 are no-string small helpers/accessors/funclets that only pair
as a byproduct of pinning their owning TU. Attributing the string-bearing runs
against the oracle (excl. vendor blob, `Symbols*.cpp` FP, ≥3 hits):

- **ENGINE with dc3/wii oracle: 768 KB, 7,375 fns, 157 runs** ← **BIGGEST with-oracle class**
- **GAME with rb3-Wii oracle:   531 KB, 3,948 fns,  48 runs**

### ★ ENGINE-with-dc3 IS the biggest with-oracle class remaining — say it loudly
As batch-5 predicted. 768 KB vs game's 531 KB, and the wave machinery has only
*lightly* touched engine gaps (LayerDir +33 was nearly free; `dc3-decomp` source
compiles byte-faithfully under the same `/O1` flags). Engine runs are more
numerous but individually smaller (157 runs, avg ~4.9 KB) vs game's chunkier
48 runs (avg ~11 KB) — so game gives fewer/bigger carves, engine gives a long
tail of small clean ones. **Both are live; engine is the larger untapped pool.**

### QUAZAL-NET 0.97 MB — genuinely partial oracle
Quazal Rendez-Vous/NetZ middleware (`.\Transport\PRUDP\PRUDPEndPoint.cpp`,
`.\SessionDiscovery\LAN\...`, `JobConnectStation::*`, `DuplicationSpace`,
`quazal.com`). rb3-Wii **has** `ByteStream.cpp`, `StationURL.h`,
`QuazalSession.cpp`, `AccountManagementClient.h` but **lacks** the transport
internals (PRUDPEndPoint, LANSessionDiscovery, DuplicationSpace, the `Job*`
facades, NATTraversal). ~0.4 MB is carvable via the 16 `src/network/*.cpp`
already in-tree + game online panels; ~0.5 MB is Quazal library internals with
no matching source (from-scratch RE, grade C). Also ~140 KB of interleaved XDK
shader-IL validator here (`REGTYPE_`, `IL_OP_EXT_LAST`) = dead.
**Confirmed: `net/` is plain `/O1` — the `/Od` caveat is `Platform/`-only (batch-5).**

---

## 2. THE DOMINANT MECHANISM THIS ROUND: truncated-pin under-carves

The seed vein is dead, but a *structural* vein is wide open. **2,477 pins but
many are severely truncated** — a TU pinned as 2–3 tiny `.text` fragments while
its real ~10–30 KB body sits unpinned right after, string-matching the same TU.
This is the same shape as batch-5's StreakTracker overflow-repin (+9), at scale.

`clean_extend.py` finds pins whose gap-to-next-*foreign*-pin holds ≥5 unpinned
fns with ≥4 same-TU string hits. **Caveat (proven on GemSmasher below):** these
regions are often **COMDAT-scatter interleaved** — a second TU's unpinned
fragments sit inside the span, so a blind full-span extend pins foreign fns too
(harmless: they read 0%, no regression, but no gain either). The productive
mechanism is the memory's scatter-include / careful per-fn pin, plus generating
`target_symbol_map` entries for the newly-covered fns (game TUs read false 0%
without them — `tools/gen_game_target_map.py`).

---

## 3. PROOF WIRED + MEASURED (branch `fp4`, commit in wt-fp4)

**GemSmasher.cpp** was pinned as 3 tiny fragments (16 B + 88 B + 168 B) inside a
real ~9 KB TU. Replaced with the full contiguous span `82BAF9F0..82BB05A8`
(bounded by the next foreign pin, Tail.cpp). Whole-binary A/B in worktree:
**20,080 → 20,081 (+1 strict, 0 lost).** Committed on branch `fp4`.

Only +1 because Tail.cpp COMDATs interleave the span and the larger newly-covered
fns lack map entries / are near-miss bodies — but it **validates the under-carve
vein is real and regression-safe.** The high-fn candidates below (OvershellPanel
244 fns, PitchArrow 126 fns) need map-gen + per-fn attention to convert their mass.

---

## 4. RANKED BATCH-6 CANDIDATES

Grades: **A** = clean single-TU carve/extend, strong oracle, source present.
**B** = under-carve/scatter (needs map-gen + interleave care). **C** = thin/no
oracle (from-scratch RE). All spans TU5-current (fingerprints.json is post-TU5-flip,
Jul 19; Ghidra :8002 `default_tu5.xex-c5a170` available for per-VA confirmation).

### Tier 1 — GAME panels, strong rb3-Wii oracle (chunky carves)

| # | file | span | fns | KB | hits | LOC | grade | key strings |
|---|---|---|---:|---:|---:|---:|:--:|---|
| 1 | `band3/meta_band/OvershellPanel.cpp` | `825b2798..825ba6c8` | 244 | 31 | 16 | 1667 | B | `mod_auto_vocals`, `does_tour_have_leaderboard`, `refresh_summary`, `all_machines_have_same_net_ui_state` — biggest single clean carve; already frag-pinned, extend+map-gen |
| 2 | `band3/game/ChordbookPanel.cpp` | `826b50d8..826b8598` | 116 | 13 | 12 | 541 | B | `on_new_track`, `skip_chordbook.trig`, `lefty_flip.anim` |
| 3 | `band3/meta_band/Leaderboard.cpp` | `8266be48..8266ff30` | 162 | 16 | 10 | 419 | B | `lb_friends`, `max_pct_complete`, `difficulty_bg` |
| 4 | `band3/game/PracticePanel.cpp` | `826b20a8..826b4ff0` | 99 | 11 | 13 | — | B | `practice_metronome`, `click_hat.cue`, `annoying_pass` |
| 5 | `band3/game/TrainerPanel.cpp` | `826c9818..826cd438` | 68 | 15 | 15 | 437 | B | `tracker_time_remaining`, `start_section`, `show_brief_band_message` |
| 6 | `band3/game/Band.cpp` | `8269a6a8..8269cbe8` | 69 | 9 | 11 | 1834 | B | `in_freestyle_section`, `crowd_boost`, `update_quest_result_label`, `coda_blown` |
| 7 | `band3/meta_band/SelectDifficultyPanel.cpp` | `82631e00..82633e70` | 75 | 8 | 10 | 203 | A | `party_shuffle`, `finish_saving.trig`, `update_instarank_solo_highscore_1_label` — **UNWIRED+UNPINNED**, fresh-wire |
| 8 | `band3/game/FreestylePanel.cpp` | `826b9960..826bbf48` | 61 | 9 | 7 | 175 | A | UNWIRED; fresh-wire game |
| 9 | `band3/meta_band/CharacterCreatorPanel.cpp` | `8260d250..8260e6ac` | 50 | 5 | 8 | 883 | B | under-carve tail |
| 10 | `band3/game/GemPlayer.cpp` | `826c0270..826c1760` | 44 | 5 | 7 | 2891 | B | under-carve tail |

### Tier 2 — ENGINE, dc3/wii oracle (biggest untapped pool; small clean carves)

| # | file | span | fns | KB | hits | LOC | grade | key strings |
|---|---|---|---:|---:|---:|---:|:--:|---|
| 11 | `system/beatmatch/TrackWatcherImpl.cpp` | `8279478c..8279b778` | 50 | 27 | 10 | — | B | `(%2d%10.1f PASS\t%d)`, `(%2d%10.1f GEM...)`, `loop_forever` — big span, likely funclet-heavy |
| 12 | `system/bandobj/PitchArrow.cpp` | `822f2044..822f5120` | 126 | 12 | 20 | 393 | B | `arrow_fx.grp`, `tilt.grp`, `arrow_style.anim`, `set_band_multiplier` — high fn count |
| 13 | `system/bandobj/OverdriveMeter.cpp` | `822db298..822dc7a8` | 68 | 5 | 11 | 129 | A | `be_deploying.trig`, `be_filling.trig`, `extend_anim.grp` — **UNWIRED+UNPINNED**, clean fresh-wire |
| 14 | `system/bandobj/GemTrackResourceManager.cpp` | `82355558..8235720c` | 71 | 7 | 8 | 67 | A | UNWIRED; small file, fresh-wire |
| 15 | `system/bandobj/StreakMeter.cpp` | `822ca3e8..822cc5cc` | 89 | 8 | 8 | 377 | B | under-carve |
| 16 | `system/bandobj/BandHighlight.cpp` | `823426f8..82345034` | 95 | 10 | 6 | 224 | B | under-carve |
| 17 | `system/bandobj/VocalTrackDir.cpp` | `822ecc48..822efb5c` | 172 | 11 | 7 | 1361 | B | big under-carve tail |
| 18 | `system/char/CharBoneDir.cpp` | `822efd98..822f1fd8` | 75 | 8 | 6 | 343 | B | under-carve |
| 19 | `system/meta/StoreOffer.cpp` | `8259ff30..825a3648` | 118 | 13 | 37 | 755 | ? | **37 hits but pinned span is far away (`827A63E0`)** — TU5-verify: likely `StoreOfferProvider` or a meta-store cluster, NOT a StoreOffer tail. Ghidra-confirm before pinning |
| 20 | `system/bandobj/GemTrackDir.cpp` | `822e6340..822e7538` | 24 | 4 | 15 | 1446 | B | clean under-carve |

### Tier 3 — QUAZAL / thin oracle (from-scratch RE; grade C, lower priority)

| # | file(attributed) | span | fns | KB | grade | note |
|---|---|---|---:|---:|:--:|---|
| 21 | `network/Platform/Platform.cpp` | `82ab12f8..82ac3110` | 375 | 71 | C | strings are actually Quazal (`JobConnectStation::*`, `ByteStream.cpp`) — middleware, oracle attribution incidental. RE carve. |
| 22 | `network/ObjDup/ConnectionInfoDDL.cpp` | `82ac7948..82ae4520` | 683 | 114 | C | Quazal LSP/AccountMgmt facades (`JobLSPLoginBypassSG`, `BackEndServicesLogin`); biggest span but thin oracle. |
| 23 | `network/net/*` in-tree cpps | `82a40000` region | — | ~400 | B | the 16 wired `src/network/*.cpp` — extend/pin like batch-5's NetSession (+40). Best Quazal ROI. |

### Deferred / walls (carried from batch-5, still valid but not fresh)
- CountOrCreateExpandedDetails body-port (+230 funclet cascade prize, single-fn high-risk).
- Profile::GetPadNum virtual-base + PlatformMgr Object-base (foundational, gated A/B).
- NavListNode-family `[826E34D0..826E3808)` from-scratch RE (no oracle).

---

## 5. RECOMMENDATION TO COORDINATOR

1. **Retire the vendor 1.99 MB from the denominator** — it is permanently dead
   (D3D shader compiler + libjpeg + tomcrypt, 0 KB carvable). This alone reframes
   "Progress %".
2. **Pivot the wave machinery to ENGINE-with-dc3** — it is the largest untapped
   with-oracle pool (768 KB / 157 runs) and dc3 source compiles byte-faithfully.
   Tier-2 above + a bodyport-batch aimed at `system/bandobj` / `system/beatmatch`.
3. **Run a truncated-pin repin wave** — the structural under-carve vein (this
   round's discovery) is mechanical: extend frag-pins to full span + gen map
   entries. GemSmasher proved it (+1 safe); OvershellPanel/PitchArrow/Leaderboard
   are the volume. Needs a per-TU interleave check (scatter-include where a
   second TU's COMDATs sit inside the span).
4. **Game panels (Tier 1)** are the chunkiest single carves; SelectDifficultyPanel
   + FreestylePanel + OverdriveMeter + GemTrackResourceManager are UNWIRED clean
   fresh-wires (grade A).

**Honest vein-health note:** raw seed yield is dead, but the *structural* frontier
(truncated pins + engine-dc3 pool) is large and the mechanisms are proven. This is
carve/wire labor, not identification labor — identification is now essentially
complete for the workable mass.
