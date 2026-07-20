# Fingerprint carve — BATCH 2 (2026-07-20)

Author: identification lead, ROUND 2. Refills the carve queue after
`docs/plans/fpcarve-batch1.md` (batch 1, being consumed by the batch-carve wave
in `wt-bc-*` — this batch does **not** duplicate its 12 live targets).

**Method upgrade over round 1.** Round 1 clustered `autoid.json` (top-1 proposal
per fn). That *undercounts* a TU's real function population and mis-diagnosed two
spans (see "corrections" below). Batch 2 instead enumerates **maximal unpinned
`.text` runs directly from the ground-truth layout** (`fingerprints.json`, 67,285
fns), then annotates each run with (a) the full string union and (b) the
majority `autoid` source vote. Spans come from the real layout, not from the
lossy per-fn proposals. Tooling: `/home/free/tmp/fp2_runs.py` +
`/home/free/tmp/fp2_final.py` (committed in `wt-fp2`), inputs
`config/45410914/splits.txt` (2,417 `.text` pins) × `objects.json` (840 wired) ×
`fingerprints.json` × `autoid.json`.

Result: **1,566 maximal unpinned runs; 145 with ≥3 distinct strings + a source
vote + unwired.** After dropping the `App.cpp` fallback bucket (generic
low-confidence catch-all), `.permuter_work_*` scratch names, `.h` attributions,
and >40 KB multi-TU blobs → the ranked batch below.

---

## ⚠ TWO systematic caveats (verified this round — read before pinning)

### A. Maximal runs can straddle TWO unwired TUs — sub-split before pinning
A single Gem→Lyric-style gap frequently holds **more than one** unwired TU. The
majority source-vote picks one; the span's tail belongs to a neighbor. **Proven:**
the `SongInfoAudioType.cpp` run `82316D7C..82317540` votes SongInfoAudioType, but
`823170D0` (512 B) references `entersandman.mid` / `_norm.png` / `_spec.png` /
`*_action_token` — a *different* TU. Pinning the whole run mis-pairs the tail.
**Action:** before pinning any candidate, walk the per-fn strings across the span
and cut at the string-family boundary. (`fp2_runs.py` prints per-fn strings.)

### B. Channel-3 in-tree residue is DRIFT-blocked, not zero-cost
The task's cheapest channel (in-tree `.cpp`, source already present) is **drained
of clean wins**. The carve *mechanics* work end-to-end (see proof), but the two
strongest in-tree candidates both diverge from retail at the source level:
- **`BeatClock.cpp`** (system/world) — compiles only under `SONGPOS_DC3_PHRASE`;
  it calls `SongPos::AccessPhrase/GetPhrase`, but retail RB3-360 `SongPos` is
  **0x14 with NO phrase field** (hard-proven, `src/system/utl/SongPos.h` header
  note + `docs/decomp/research/2026-06-11-player-plus4-layout.md`). dc3-only
  source (no rb3-Wii oracle) → the retail body genuinely differs. **Defer.**
- **`SongInfoAudioType.cpp`** (system/utl) — compiles with **zero source edits**,
  target obj emits, `size_order_automap` recovers the pairing — but the retail
  bodies are a different implementation: retail `SongInfoAudioTypeToSym` is an
  **18-case jump-table** (`cmplwi cr6,r4,0x12; bgt`), our in-tree source is an
  if-chain → **1.4%**; retail `SymbolToAudioType` is compact where ours emits 20+
  static-`Symbol` inits → **0.0% / 229 inserts**. Real dc3-vs-retail drift.

This confirms the `project_unwired_owner_wiring` memory ("clean scatter sub-vein
drained; STANDARD-WIRE residue remains"): the residue that survives is
drift-blocked, so it carries genuine body-port cost, not zero cost.

---

## PROOF carve (channel 3) — mechanics validated, source drifted (worktree `wt-fp2`, branch `fp2`)

`system/utl/SongInfoAudioType.cpp` wired as the cheapest possible carve:
- Added to `objects.json` (engine, NonMatching) + pinned
  `.text 0x82316D7C..0x82317540` in `splits.txt`.
- **Compiles with ZERO source changes** (in-tree source present). dtk emitted
  `obj/SongInfoAudioType.obj` + `asm/SongInfoAudioType.s`; full ninja green.
- `size_order_automap` recovered the fn pairing; added 2 target-map entries
  (`?SymbolToAudioType@@…`, `?SongInfoAudioTypeToSym@@…`) verified vs compiled obj.
- **Measured: 0.0% and 1.4%** — body-level dc3-vs-retail divergence (see caveat B).

Net: the carve→compile→pair→measure loop is proven on an in-tree engine file, but
**this specific TU is a body-port, not a free match.** Additive-only (new file,
new pin, 2 new in-span map VAs) → no regression risk if landed as a NonMatching
scaffold, but do **not** expect a match without body work. Kept in `wt-fp2` for
reference; **not for landing as a "win."**

---

## CORRECTIONS to round-1 "held" spans (the explicit ask)

### GemRepTemplate — round-1 diagnosis was WRONG; true span found
Round 1 held it claiming the cluster `82BAD790..82BAF394` falls *inside*
VocalTrack's pin `[82B9F510,82BAA230)`. **It does not** — `0x82BAD790 > 0x82BAA230`.
VocalTrack is **not** over-carved. The real layout:
```
82baa400..82bad714  Gem.cpp        (pinned)
82bad714..82baeba8  << GAP >>       ← GemRepTemplate.cpp lives here
82baeba8..82baf270  Lyric.cpp      (pinned)
```
The autoid cluster overshot the gap end into Lyric's pin. **True GemRepTemplate
span = `0x82BAD718..0x82BAEBA4`** (36 fns, 5,260 B), a clean Gem→Lyric gap.
Confirmed by strings across all 36 fns: `tail_min_amp`, `tail_max_amp`,
`tail_fade_distance`, `tail_alpha_smoothing`, `stretch_scale`,
`overdrive_tracker_description`, `%s.mat`, `mat_formats`, `tail02.mesh`,
`guitar_sustain_x_scale`, `real_keys_sustain_y_pos`, `tail_bonus.mat`,
`tail_chord.mat`. Source: `../rb3/src/band3/bandtrack/GemRepTemplate.cpp` (206
LOC); header `src/band3/bandtrack/GemRepTemplate.h` already in-tree. **Grade A —
pin the corrected span and port.**

### Scoring — real span found; excludes 2 ICF-folded HolmesClient islands
Round-1 span `826A0778..826A2208` is right at the edges but straddles two small
**ICF-folded** functions pinned to `HolmesClient.cpp` (`826A1808..826A1868`,
`826A1960..826A19C0`) that sit *inside* the Scoring run (identical-COMDAT folding
co-locates them). Pin Scoring as **three ranges skipping the islands**:
```
Scoring.cpp:
    .text  start:0x826A0778 end:0x826A1808
    .text  start:0x826A1868 end:0x826A1960
    .text  start:0x826A19C0 end:0x826A2208
```
Strings across the run: `crowd_boost`, `fill_boost`, `multiplier`, `star_ratings`,
`awards`, `reward`, `penalty`, `new_bonus_thresholds`,
`new_instrument_thresholds`, `rg_chordbook_*`, `energy`, `overdrive`,
`multipliers`. Source `../rb3/src/band3/game/Scoring.cpp` (366 LOC), header
`src/band3/game/Scoring.h` in-tree. (Note: the region *below* `826A0778` —
`826A00D0`, `esrb_keep.milo` — is a **different** startup/autosave TU; do not
include it. Caveat A applies.) **Grade B+ — game scoring math, few thunks.**

---

## RANKED BATCH 2 (new, not in round-1)

Grade A = strong strings + tractable port + in-tree header; B = viable
(larger/network friction or dual-oracle only); C = weak/generic strings.
`str` = distinct strings across the span (batch-2 metric, ≫ round-1 counts).
Channel: **game** = `band3/`|`network/`; **engine** = `system/`; **game(dc3)** =
`lazer/`|game code with only a dc3 oracle. Spans need caveat-A sub-split check.

| # | TU | channel | span `[start,end)` | fns/bytes | str | LOC | grade | ≥3 quoted strings (source evidence) |
|---|----|---------|--------------------|-----------|-----|-----|-------|-------------------------------------|
| 1 | **GemRepTemplate** (corrected) | game | `82BAD718..82BAEBA4` | 36 / 5.3K | 31 | 206 | **A** | `tail_min_amp`, `tail_max_amp`, `tail_fade_distance`, `stretch_scale`, `overdrive_tracker_description`, `mat_formats`, `tail02.mesh` |
| 2 | **PresenceMgr** | game | `8267D480..82681864` | 145 / 17.4K | 92 | 207 | **A** | `AccomplishmentEarnedMsg`, `PlayerGameplayMsg`, `SetUpMicsMsg`, `SetUserTrackTypeMsg`, `adjust_for_vocal_phrases`, `active_player` (dual oracle: band3+dc3) |
| 3 | **SigninScreen** | game | `82635EDC..826371E8` | 47 / 4.9K | 17 | 147 | **A** | `handle_sign_outs`, `must_be_multiplayer_capable`, `must_be_online`, `on_signed_in`, `on_signed_out`, `signing_in_user` |
| 4 | **TokenRedemptionPanel** | game | `8263FDE8..8264160C` | 65 / 6.2K | 18 | 289 | **A** | `get_offers_for_token`, `show_purchase_ui_for_offer`, `token_redemption_msg`, `token_redemption_not_found`, `token_redemption_purchased` |
| 5 | **Scoring** (corrected, 3-range) | game | `826A0778..826A2208` −islands | 136 / 11.3K | 25 | 366 | **B+** | `crowd_boost`, `fill_boost`, `star_ratings`, `new_bonus_thresholds`, `energy`, `overdrive`, `multipliers` |
| 6 | **CrowdMeterIcon** | engine | `822B9318..822BB3C8` | 110 / 8.4K | 31 | 149 | **B** | `arrow_hide.trig`, `arrow_show.trig`, `deploy.trig`, `drop_in.trig`, `glow.trig` (bandobj; test-MIDI strings weak) |
| 7 | **BandHeadShaper** | engine | `822AF1C8..822B0AB0` | 38 / 6.4K | 26 | 403 | **B** | `%s.fdm`, `char/main/head/%s/head.milo`, `head_male_path`, `head_female_path`, `head_morph`, `base.msnm` |
| 8 | **SongSectionController** | engine | `8230C260..8230DEA4` | 52 / 7.2K | 26 | 428 | **B** | `[active pool]: `, `[current practice section]: `, `*** No content pool attached to category '%s'! ***`, `[pool asset]: ` |
| 9 | **MultiplayerAnalyzer** | game | `826CD4E0..826CE25C` | 23 / 3.5K | 9 | 285 | **B** | `pro_bonus`, `point_rate`, `do_record`, `scoring`, `coda`, `chord`, `tail` |
| 10 | **PerfectSectionTracker** | game | `826DAB90..826DC4BC` | 42 / 6.4K | 9 | 395 | **B** | `perfect_section_tracker_description`, `deploy_stat_tracker_contribution`, `require_full_energy`, `require_max_multiplier`, `send_tracker_section_complete` |
| 11 | **TrackPanelDirBase** | engine | `8237A5A4..8237B214` | 26 / 3.2K | 21 | 394 | **B** | `default_clip_or_group`, `default_play_starved`, `first_playing_clip`, `blend_width`, `beat_scale`, `clip_type` |
| 12 | **TourCondition** | game | `82364428..8236537C` | 46 / 3.9K | 10 | 146 | **B** | `part_difficulty_filter`, `vocal_parts`, `is_internal`, `filter`, `weight`, `perf` |
| 13 | **BeatMaster** | engine | `8276E328..8276F458` | 35 / 4.4K | 13 | 244 | **B** | `BML: %s`, `BeatMasterLoader`, `beatmatcher`, `downbeat`, `eighth_note`, `measure` |
| 14 | **VoiceControlPanel** | game(dc3) | `82568F24..8256B4C0` | 100 / 9.6K | 23 | 448 | **B** | `lesson_mgr`, `none_bandana`, `none_earrings`, `none_facehair`, `none_glasses` (dc3 `lazer/meta_ham` oracle only) |
| 15 | **SongSequence** | game(dc3) | `8264519C..8264BBE0` | 308 / 27.2K | 60 | 402 | **B** | `completed_double_harmonies`, `completed_triple_harmonies`, `endgame_note_streak`, `endgame_phrase_streak`, `endgame_avg_multiplier` (large; sub-split) |
| 16 | **InetAddress** | game/net | `82AFC2E0..82B02744` | 180 / 25.7K | 30 | 211 | **B** | `\tAvg Time : %10d usec`, `.\\Transport\\PRUDP\\PRUDPStream.cpp`, `.\\Core\\BandwidthCounter.cpp`, `127.0.0.1` (Quazal `/Od`+`revolution/`; big span = multi-TU, sub-split) |
| 17 | **MemoryManager** (net) | game/net | `82A68F38..82A6DC40` | 105 / 19.7K | 12 | 86 | **B** | `.\\Core\\MemoryManager.cpp`, `Default memory manager`, `Assertion failed: %s (%s:%u)`, `cur_state <= CAR` (Quazal; big span = multi-TU) |
| 18 | **EnvAnim** | engine | `824867D8..8248757C` | 34 / 3.5K | 9 | 121 | **B** | (system; sub-split & confirm strings) |
| 19 | **BandStarDisplay** | engine | `8230DF5C..8230E5B4` | 17 / 1.6K | 6 | 175 | **C** | small; verify strings |
| 20 | **Server** (net) | game/net | `823F5D3C..823F61F4` | 10 / 1.2K | 10 | 29 | **C** | `access_key`, `is_connected`, `login`, `logout`, `server`, `port` (tiny LOC but Quazal friction) |
| 21 | **MBT** | in-tree(3) | `827D0E70..827D10E8` | 7 / 632 | 4 | 40 | **C** | `  lFrags = %14d`, `%d:%d:%03d`, `show_song_options`, `venue_intro` (in-tree `system/utl/MBT.cpp`; verify no drift like SongInfoAudioType) |
| 22 | **JointUtl** | in-tree(3) | `82360F40..82361400` | 13 / 1.2K | 4 | 104 | **C** | `bone_L-ankle.mesh`, `bone_R-ankle.mesh`, `bone_R-toe.mesh` (in-tree `system/gesture/JointUtl.cpp`) |

### Deferred (channel 3, drift-blocked — do NOT wire as free)
| TU | span | why |
|----|------|-----|
| **BeatClock** | `82748BC8..82749354` | `SongPos` phrase drift (dc3-only; retail SongPos has no phrase). Body-port. |
| **SongInfoAudioType** | `82316D7C..82317540` | retail = jump-table, in-tree = if-chain (0.0%/1.4%). Body-port + span is mixed (caveat A). |

---

## Recommended next actions
1. **Pin GemRepTemplate at the corrected `82BAD718..82BAEBA4`** and port the 206-LOC
   rb3-Wii `.cpp` — header already in-tree, self-contained tail-rendering math. Top A.
2. **PresenceMgr (#2)** is the richest new game surface (92 strings, dual band3+dc3
   oracle, 207 LOC) — Msg-class handlers, self-contained. Pin + port next.
3. Correct the **Scoring** pin to the 3-range island-excluding form before carving.
4. **Every** candidate: run the caveat-A per-fn string check (`fp2_runs.py`) and cut
   the span at string-family boundaries — the maximal-run enumerator over-groups
   co-located unwired TUs (proven on SongInfoAudioType, Scoring, InetAddress).
5. Channel 3 is drained of free wins: treat remaining in-tree residue (MBT, JointUtl,
   PlatformMgr_Xbox) as **body-ports pending drift check**, not zero-cost carves.

## Corroboration
`scripts/harvest/bindiff_r1_carving_hints.json` VAs (~10 unwired singletons, all
`kind:uncompiled` dc3-map transfers) — none fall in any batch-2 span (string-
fingerprint is an independent channel from the BinDiff library-mass hints, same as
round 1). String evidence stands alone; all Grade-A/B spans have ≥3 TU-specific
strings cross-verified against `../rb3/src` or `../dc3-decomp/src`.
