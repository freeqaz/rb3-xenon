# Ranked carvable GAME TUs — string-fingerprint channel (batch 1, 2026-07-20)

Author: game-TU identification lead. Method: **string-fingerprint** (not BinDiff — the
BinDiff carving hints are ~72% source-less library mass, see
`docs/plans/carve-pilot-2026-07-20.md`). RB3 game code has a rich rb3-Wii source oracle
(`../rb3/src`, MILO_ASSERT path strings + named functions). This batch is derived from
`autoid.json` (fresh Jul-19), gap-aware, cross-checked against `splits.txt` pins and
`objects.json` wiring.

## How the candidates were derived

1. `tools/fingerprint_match.py autoid` maps every anonymous `fn_<VA>` to a proposed
   rb3-Wii source by referenced-string overlap.
2. Filtered to GAME oracle files (`band3/`, `network/`), dropped `.h` (inlined) and
   already-`objects.json`-wired files.
3. Kept only fns that land in **unpinned `.text` gaps** (not inside an existing
   `splits.txt` range), then clustered into contiguous runs (<0x2000 inter-fn gap).
4. `clean=True` = the run's `[start,end)` intersects **no** pinned interval → safe carve.
5. Ranked by **port-cost** (rb3-Wii `.cpp` LOC = proxy) × evidence strength. **Every
   candidate's header already exists in-tree** (`src/...`), so port = `.cpp` bodies only.

## ⚠ The real per-TU cost is SYMBOL-MAP GENERATION, not the port

Refined economics beyond carve-pilot. For a clean leaf panel the port can be **zero source
changes** (NewAwardPanel below). But **objdiff pairs target↔base by symbol NAME**, and:
- game `.text` gaps have **~0** existing `target_symbol_map.json` coverage (verified),
- the BinDiff oracle (`unified_id_rb3wii.json`) VAs are **scattered across the whole
  binary**, not contiguous → useless for pinning a carved span,
- `tools/gen_game_target_map.py` therefore produces **nothing** for these gaps.

So each carved game TU needs its `fn_<VA>→MSVC-mangled` map built by hand (mangle the
ported names, align to VAs). This is mechanical (mangled strings are literally in the
compiled `.obj` — `strings foo.obj | grep Method`) but it is the dominant recurring cost.
**A batch wave should build a size/order-alignment auto-mapper** (compiled-obj fn sizes ↔
target `.s` fn sizes, in source order) to amortize this — that is the highest-leverage
tooling investment for a carve campaign.

Also note: even a 139-line panel carves into **~100 target functions** (2 classes ×
vtables + message tables + `??_9`/`??_E`/`??_G` thunk skirt). Only ~10-15 are real bodies;
the rest need the full class + mixins present to pair. Matching is per-real-body.

---

## PROOF — top 2 wired + measured (worktree `~/tmp/wt-fpcarve`, branch `fpcarve`)

### #1 `band3/meta_band/NewAwardPanel.cpp` — VALIDATED, real 100% match ✅
- Pinned `.text start:0x8262709C end:0x82629878` (the full gap between UIList.cpp and
  PatchPanel.cpp — confirmed empty of pins). dtk emitted 104 target fns + auto `.pdata`.
- Ported rb3-Wii `.cpp` (139 lines) with **ZERO source changes**; header already in-tree.
- Hand-mangled 3 distinctive fns (identified by their string refs) into
  `target_symbol_map.json`, verified vs compiled obj. Measured:
  - `AwardAssetProvider::InitData(RndDir*)` — **100.0% normalized** (byte-identical) ✅
  - `AwardAssetProvider::Mat(int,int,UIListMesh*) const` — **98.4%** (1 insert + 1 reg)
  - `NewAwardPanel::PopAndShowFirstAward()` — **77.3%** (reg-swap r26↔r28 + control-flow,
    verdict LikelyFixable)
- Whole-binary: **additive only** (no other unit's source/pin touched; 3 new map VAs all
  inside the new pin). No regression possible. Committed on `fpcarve`.

**This beats the carve-pilot 0% stub — it proves the string-fingerprint→carve→port→match
loop produces genuine matches on game code.**

### #2 `band3/meta_band/ContentLoadingPanel.cpp` — carved+attempted, FINDING: engine-API drift
- Pins clean, but the port does **not** compile as-is: in-tree `ContentMgr` is accessed
  differently than rb3-Wii (`TheContentMgr->` vs value type; `ShowCurRefreshProgress`
  not a member; `TheUI.InTransition`; `MeterDisplay.h` header error). Reverted to keep the
  build green.
- **Data point for the ranking:** panels that call singletons whose API drifted between
  rb3-Wii and the dc3/retail engine carry real (non-zero) port cost. Prefer TUs whose
  logic is self-contained (NewAwardPanel talks only to `TheAccomplishmentMgr`/`AssetMgr`,
  which are stable) over ContentMgr/UI-callback panels.

---

## Ranked batch (clean-gap, unwired, in-tree header, GAME)

Grade A = wire next (strong strings + tractable port); B = viable, larger/network friction;
C = weak evidence or known API-drift. `LOC` = rb3-Wii `.cpp` line count (port-cost proxy).

| # | TU | area | span `[start,end)` | fns/bytes | LOC | grade | ≥3 corroborating strings |
|---|----|------|--------------------|-----------|-----|-------|--------------------------|
| 1 | **NewAwardPanel** ✅DONE | game/meta_band | `8262709C..82629878` | 104t / 10.2K | 139 | **A** | `male.mat`, `female.mat`, `unisex.mat`, `gender`, `icon`, `update_provider` |
| 2 | **CampaignGoalsLeaderboardPanel** | game/meta_band | `825EFC78..825F1E4C` | 3+ / 0x21D4 | 148 | **A** | `campaign`, `lb_success`, `tour`, `update_leaderboard_provider` |
| 3 | **CharCache** | game/meta_band | `8256BC58..8256C310` | 2+ / 0x6B8 | 257 | **A** | `char_cache`, `../world/shared/chars.milo`, `patch_panel`, `patch_preview.tex`, `customize_panel` |
| 4 | **StoreOfferProvider** | game/meta_band | `82663968..826640CC` | 2+ / 0x764 | 471 | **B** | `album.mat`, `song.mat`, `group.mat`, `famousby`, `purchased`, `rbn_icon` |
| 5 | **RGTrainerPanel** | game/game | `826ADD68..826B1350` | 6 / 0x35E8 | 636 | **B** | `chord_legend`, `fret_success.cue`, `fret_fail.cue`, `lefty_flip.anim`, `rg_chordbook_a_short`, `rg_chordbook_b_string` |
| 6 | **InterstitialMgr** | game/meta_band | `825AFE98..825B0150` | 2 / 0x2B8 | 159 | **B** | `%s_panel`, `%s_screen` (+ neighbor corroboration) |
| 7 | **ModifierMgr** | game/meta_band | `82357218..823573AC` (+`825895A0..82589E24`) | 2+2 | 189 | **B** | `mod_auto_vocals`, `modifier_mgr`, `name`, `status` |
| 8 | **MetaNetMsgs** | game/meta_band | `825BC1D8..825BC65C` | 2 / 0x484 | 90 | **B** | `tour_band_event_panel`, `"A machine with %s just joined our machine with %s..."` |
| 9 | **BandStorePanel** | game/meta_band | `825BCC10..825BCF24` | 2 / 0x314 | 398 | **C** | `dlc_store` (single distinctive — weak) |
| 10 | **Platform** | network/Platform | `82AC0DD8..82AC2814` | 5 / 0x1A3C | 72 | **B** | `Assertion Failed`, `Deadlock detected`, `Buffer extraction overflow`, `Product key does not exist or is invalid`, `Invalid wait` (⚠ 29-entry error-string TABLE in .rdata; Quazal `/Od`+`revolution/` headers → port friction despite tiny LOC) |
| 11 | **ConnectionInfoDDL** | network/ObjDup | `82AE0138..82AE0444` | 2 / 0x30C | 64 | **B** | `m_strStationURL3`, `m_uiInputBandwidth`, `m_uiOutputBandwidth` |
| 12 | **BandwidthCounter** | network/Platform | `82AF71D8..82AF7394` | 2 / 0x1BC | 49 | **C** | `%s %d`, `/Incoming`, `/Outgoing` (weak/generic) |

### Held (high-value but need span refinement — cluster crosses a pinned interval)
| TU | area | LOC | why held |
|----|------|-----|----------|
| **Scoring** | game/game | 366 | `clean=False`: run `826A0778..826A2208` straddles the Band.cpp/Performer.cpp pin boundary. Strong strings (`crowd_boost`, `multipliers`, `new_bonus_thresholds`, `overdrive`, `energy`). Refine the span (split around Performer's pin) then carve — **high value, game scoring math, not a panel = fewer thunks**. |
| **GemRepTemplate** | bandtrack | 206 | `clean=False`: cluster `82BAD790..82BAF394` falls **inside** the currently-pinned `VocalTrack.cpp [82B9F510,82BAA230)` range → VocalTrack's pin is likely **over-carved** (includes GemRepTemplate). Investigate/repair the VocalTrack boundary first; strong strings (`tail_*`, `%s.mat`, `mat_formats`). |
| **ContentLoadingPanel** | game/meta_band | 129 | engine-API drift (see proof #2) — needs `ContentMgr`/`TheUI`/`MeterDisplay` port work. |

## BinDiff-hint corroboration
`scripts/harvest/bindiff_r1_carving_hints.json` — **none** of its VAs fall in any candidate
gap. Expected: string-fingerprint is an **independent** identification channel from the
BinDiff library-mass hints. No overlap corroboration available; the string evidence stands
on its own (all clusters have ≥3 game-specific asset/config strings cross-verified against
the rb3-Wii source).

## Recommended next actions
1. **Build the size/order auto-mapper** (compiled-obj fn sizes ↔ target `.s` fn sizes in
   source order) — removes the dominant per-TU manual-mangle cost; unlocks a real campaign.
2. Wire grades A (#2 CampaignGoalsLeaderboardPanel, #3 CharCache) next — self-contained
   logic, strong strings, in-tree headers, <260 LOC.
3. Refine the two "held" high-value spans (Scoring, GemRepTemplate/VocalTrack boundary).
4. NewAwardPanel `Mat` (98.4%) and `PopAndShowFirstAward` (77.3%) are close near-misses —
   a body-port pass can finish them.
</content>
</invoke>
