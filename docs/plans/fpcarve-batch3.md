# Fingerprint carve — BATCH 3 (2026-07-20)

Author: identification lead, ROUND 3. Refills the carve queue after
`docs/plans/fpcarve-batch2.md` (batch-2 wave landed **+351**, 19,191 → 19,542).
This batch consumes batch-2's seeds + recon-deferred candidates and enumerates
fresh maximal unpinned `.text` runs against the post-wave `splits.txt`
(2,445 `.text` pins, Jul-20 23:20).

**Method (unchanged from batch-2, re-run fresh):**
`scripts/harvest/fp2_runs.py` → `fp2_final.py` enumerate maximal unpinned runs
from ground truth (`fingerprints.json` 67,285 fns) minus current pins, annotated
with string-union + majority autoid vote. Then **per-fn string-family sub-split**
(`scripts/harvest/fp2_span.py 0xSTART 0xEND`) on every candidate before listing —
maximal runs straddle co-located unwired TUs. Every candidate's strings were
cross-checked against **BOTH** `../rb3/src` (rb3-Wii, game oracle, richer) and
`../dc3-decomp/src` (engine twin); channel/source assigned from where the strings
*actually* live, not the autoid vote.

**Batch-2 lesson applied:** every autoid channel guess that read "engine/dc3" for
a `band3`/`bandobj` span was WRONG — the strings live in rb3-Wii. All 20 candidates
below have their oracle **verified by grep**, and the sub-split boundaries are
listed so the carve foreman can pin the correct COMDAT run.

---

## ✅ PROOF carve (landed in worktree `wt-fp3`, branch `fp3`, commit acb2fcf4)

**StoreOfferProvider pin-extension** — the batch-2 "HamSongMetadata" candidate
(`826640cc..826662e0`) was **misidentified**: its head is the *unpinned continuation
tail of the already-wired StoreOfferProvider.cpp* (its pin ended at `826640CC`, exactly
where this run begins). Source already compiles in-tree → the cheapest possible carve
is a `splits.txt` `end:` bump.

- Added `.text 0x826640CC..0x82665BAC` (cut before the invite/StoreMenuProvider
  scatter at `82665df0`).
- `touch config.yml` + `configure.py` + `rm report.cache` + full `ninja-locked`.
- **Measured: 19403 → 19409 = +6 strict, 0 LOST**, zero map entries (pure
  positional/structural pairing on the already-compiled obj).

This validates the carve→pin→measure loop end-to-end on a grade-A candidate and
confirms the identification. The remaining tail (`82665bac..826662e0`) is invite +
StoreMenuProvider scatter (StoreMenuProvider itself is pinned at `82673B20`) — leave
unpinned. **Ready to cherry-pick to main as the first batch-3 landing.**

---

## ⚠ Systematic findings this round (read before pinning)

1. **Sub-split is mandatory — 13 of 20 candidates straddle ≥2 TUs.** The autoid
   label names only the majority TU; neighbors ride along. Every span below lists
   its cut boundary. Several "candidates" turned out to be scatter of an
   *already-wired* TU (ContentLoadingPanel tail = ManageBandPanel+CustomizePanel;
   SessionMgr tail = ModifierMgr; NextSongPanel = its own wired owner) — those pin
   as gap-fills/scatter-includes, not fresh wires.

2. **`../rb3/src/network/` DID decompile much of the Quazal Core SDK.** Batch-2's
   "network = no oracle, RE-only" premise is too broad. Real full/near-full `.cpp`
   oracles exist for `Core/WorkerThreads`, `Platform/{MemoryManager,BandwidthCounter,
   EventHandler,InetAddress}`, and all of `network/net/*`. Genuinely oracle-less:
   `StringConverter`, `PRUDPStream`/Transport, `IDGeneratorDDL`, `Job*`, PerfCounter
   stats, and the XDK ucode-compiler mass. Oracles that exist are Wii-targeted (MWCC +
   Wii socket/thread APIs) → pure-logic Core classes port cleanly, platform classes
   port logic-only (calls diverge on Xbox Winsock).

3. **MemoryManager mass-skip confirmed.** The `82a68f38..82a6dc40` run is 91 fns of
   XDK XGRAPHICS ucode compiler (`e:\xenon\xdk-main-feb10\...\ucode\compiler\...`) —
   SKIP. Only the 14-fn tail `82a6d428..82a6dc40` is real Quazal MM (oracle exists).

---

## RANKED BATCH 3

Grade A = clean tractable port, in-tree header, tight source, verified strings.
B = viable (larger / net-glue friction / header-port needed / gap-fill).
C = weak/generic strings, oracle-less, or confirmed drift.
Channel: **game** = `band3`/`network` (rb3-Wii oracle); **engine** = `system`.
`fns` counts the sub-split TU, not the maximal run.

| # | TU | channel | pin span (after sub-split) | fns/bytes | ≥3 quoted strings (verified in source) | source (path, LOC) | header in-tree | grade |
|---|----|---------|----------------------------|-----------|----------------------------------------|--------------------|----------------|-------|
| 1 | **StoreOfferProvider** (pin-ext) ✅LANDED | game | `826640CC..82665BAC` | 89 / 6.6K | `find_offer`, `find_pack`, `build_list_no_grouping`, `get_shortcut_array`, `has_shortcuts`, `store_next_chunk` | `band3/meta_band/StoreOfferProvider.cpp` (471) | wired | **A** |
| 2 | **BandStarDisplay** | engine(bandobj) | `822CCDD0..822CDC10` | 29 / 3.6K | `achieve_star.cue`, `achieve_spade.cue`, `star%d`, `stars_offset.tnm`, `sweep.mnm`, `num_stars`, `star_type`, `pulse_success.trig` | `system/bandobj/BandStarDisplay.cpp` (175) | ✔ | **A** |
| 3 | **TrackPanelDirBase** | engine(bandobj) | `82357DD0..82359760` | 74 / 6.5K | `draw_order.grp`, `foreach_configurable_object`, `gem_tracks_size`, `configurable_objects`, `net_track_alpha`, `view_time_expert`, `configure_tracks` | `system/bandobj/TrackPanelDirBase.cpp` (394) | ✔ | **A** |
| 4 | **ContentLoadingPanel** | game | `826140F0..~82614690` | ~11 / ~1.4K | `finding.trg`, `loading.trg`, `finding_additional_content`, `loading_additional_content`, `timer_script`, `blackmask.mat` | `band3/meta_band/ContentLoadingPanel.cpp` (129) | ✔ | **A** |
| 5 | **StreakMeter** | engine(bandobj) | `822CC798..822CCDD0` | 17 / 1.5K | `part_color%d.anim`, `part_fade%d.anim`, `residue_fade.trig`, `num_parts.anim` | `system/bandobj/StreakMeter.cpp` (377) | ✔ | **A/B** |
| 6 | **SetlistToStorePanel** | game | `82642450..~82643300` (excl BandScreen head) | ~30 / ~3K | `get_songs_from_music_library`, `load_song_metadata`, `setlist_to_store_screen_timeout`, `setlist_upsell`, `dummy_upsell_offer` | `band3/meta_band/SetlistToStorePanel.cpp` (65) | ✔ | **A/B** |
| 7 | **GigFilter** | game(tour) | `82364AA0..~82365100` | ~10 / ~1.5K | `filter`, `is_internal`, `part_difficulty_filter`, `weight` (FindData/FindArray) | `band3/tour/GigFilter.cpp` (61) | ✔ | **A/B** |
| 8 | **FixedSetlist** | game(tour) | `82365120..8236537C` | ~9 / ~0.6K | `group`, `songs`, `weight` (FindData group/weight, FindArray songs) | `band3/tour/FixedSetlist.cpp` (72) | ✔ | **A/B** |
| 9 | **SessionMessages** (JoinResponseMsg cluster) | game(net) | `823F1068..823F14C0` | 16 / 1.1K | `JoinResponseMsg`, `Join Request Accepted\n`, `Join Request Rejected, Error=%i, CustomError=%i\n` | `network/net/SessionMessages.cpp` (283) | ✔ | **B** |
| 10 | **RGUtl** | engine(beatmatch) | `82779CC0..8277B394` (verify tail thunk `8277b178`) | 18 / 5.8K | `sus4/6`, `susb2`, `sus#4`, `b13`, `+-5` (chord-name fmt over gNoteNames[]) | `system/beatmatch/RGUtl.cpp` (531) | ✔ | **A-/B** |
| 11 | **FingerShape** | engine(bandobj) | `82345828..8234615C` | 7 / 2.3K | `%s_patch.mesh`, `Bone04.anim`, `Bone05.anim`, `shareBone_0low1_trans.tnm` | `system/bandobj/FingerShape.cpp` (137) | ✔ | **A-/B** |
| 12 | **SessionMgr** | game(net) | `82584B58..~82588D00` | ~180 / ~16K | `session_mgr_updated_msg`, `joining_allowed_in_transition`, `session_ready`, `get_net_random_seed`, `add_local_user`, `internal_setlists` | `band3/meta_band/SessionMgr.cpp` (523) | ✔ | **B** |
| 13 | **NextSongPanel** (interior gap-fill) | game | `8264519C..8264BBE0` (−tail SongSortByRecent thunk) | 308 / 27K | `completed_double_harmonies`, `endgame_note_streak`, `endgame_avg_multiplier`, `make_a_setlist`, `set_token_fmt`, `header_continued` | `band3/meta_band/NextSongPanel.cpp` (886) | wired | **B** |
| 14 | **NetSession + arbitration cluster** | game(net) | `823E6F60..823E9C70` (sub-split) | 89 / 11.5K | `BeginArbitrationMsg`, `JoinRequestMsg`, `SyncUserMsg`, `SyncObj tag = %s, dirtyMask = %x`, `connection_timeout`, `max_connection_silence` | `network/net/NetSession.cpp` (1089) + `SyncStore.cpp` (131) | ✔ | **B** |
| 15 | **StreakTracker + OverdriveTracker cluster** | game | `826DD570..826E3804` (heavy sub-split) | 191 / 25K | `send_tracker_end_deploy_streak`, `overdrive_tracker_description`, `streak_tracker_progress`, `chain_multipliers`, `overdrive_chain` | `band3/game/{StreakTracker.cpp (184), OverdriveTracker.cpp (208)}` | ✔ | **B** |
| 16 | **BandStorePanel** | game | `82606888..826078A4` + `825BCA38..825BD60C` | 66 / 6K | `store_load_failed`, `update_loading_status`, `dlc_store`, `preview_art`, `ml_store_purchase_error`, `content_installed` | `band3/meta_band/BandStorePanel.cpp` (398) | ✔ | **B** |
| 17 | **LayerDir** | engine(bandobj) | `823278F0..82329B14` + `823264D8..82326D1C` | 110 / 10.9K | `allow_alpha`, `alpha_max`, `bitmap_list`, `use_free_cam`, `layers`, `get_patch_tex`, `_spec.png` | `system/bandobj/LayerDir.cpp` (254) rb3-Wii only | ✘ (port LayerDir.h) | **B** |
| 18 | **Text/RndText** (deepen wired) | engine(rndobj) | `82459E78..8245C510` (−char head at 82459ba8) | ~85 / ~10K | `caps_mode`, `get_string_width`, `get_text_size`, `set_align`, `set_fixed_length`, `italics`, `leading` | `system/rndobj/Text.cpp` (in-tree, wired) | wired | **B** |
| 19 | **EnvAnim** | engine(rndobj) | `~8248696C..8248757C` (−TourProgress head at 824868f8) | ~30 / ~3K | `ambientColorKeys:`, `fogColorKeys:`, `fogRangeKeys:`, `keysOwner:` | `system/rndobj/EnvAnim.cpp` (121) rb3-Wii only | ✔ | **B** |
| 20 | **Leaderboards** | game(meta) | `82673390..82673B1C` | 20 / 1.9K | `on_select_gamertag_error`, `display_gamercard_privilege_error`, `display_gamercard_pad_error` | `dc3 lazer/meta_ham/Leaderboards.cpp` (384) — dc3 only | ✘ | **B/C** |

### Network Quazal-Core sub-veins (from InetAddress `82afc2e0..82b02744` sub-split)
Oracle-backed B-grades within the Quazal stack (deps: `String`,`qChain`,`qMap`,
`CriticalSection`,`MemoryManager` must be present to compile+match — shared cost):
| sub-TU | approx VA | strings | oracle | grade |
|--------|-----------|---------|--------|-------|
| WorkerThreads | `~82b00210` | `.\WorkerThreads.cpp`, `WorkerThread ID %d`, `Nb of Protocol Msg in progress` | `network/Core/WorkerThreads.cpp` (57, full) | B |
| BandwidthCounter | `82afff48` | `.\Core\BandwidthCounter.cpp`, `PerfCounter %s: L=%d, T=%d` | `network/Platform/BandwidthCounter.cpp` (49, full) | B |
| MemoryManager (tail) | `82a6d428..82a6dc40` | `.\Core\MemoryManager.cpp`, `Default memory manager` | `network/Platform/MemoryManager.cpp` (86, full) | B |
| EventHandler | `82aff520` | `.\Core\EventHandler.cpp` | `network/Platform/EventHandler.cpp` (40, ctor only) | B- |

---

## DEFER / SKIP (oracle-less, drift-confirmed, or mass-noise)

| TU / region | span | reason |
|-------------|------|--------|
| **JointUtl** | `82360F40..82361400` | Confirmed dc3-vs-RB3 gesture DRIFT (best body ~52.5%); IN-TREE already; no clean oracle. |
| **ChordShapeGenerator** | `822DD290..822DD7AC` | 869 LOC but only 6 out-of-line fns (heavy `/Ob2` inlining) — inlining hard to reproduce; header in-tree, low yield. |
| **DataMinerJobs** | `82766B68..82767410`, `825BC65C..825BC908` | Weak generic DataArray strings (mode/obj/handler/event); dc3-only `net_ham`, high network drift. |
| **BeatClock** (batch-2 carry) | `82748BC8..82749354` | `SongPos` phrase-field drift (retail SongPos = 0x14, no phrase). Body-port only. |
| **SongInfoAudioType** (batch-2 carry) | `82316D7C..82317540` | retail = jump-table, in-tree = if-chain (0.0%/1.4%). Body-port + mixed span. |
| **XDK ucode mass** (in MemoryManager run) | `82a68f38..~82a6d428` | 91 fns of XDK XGRAPHICS shader compiler — not RB3 code, no oracle. SKIP. |
| **StringConverter / PRUDPStream / IDGeneratorDDL / Job* / PerfCounter** | within `82afc2e0..82b02744` | Genuinely oracle-less Quazal internals (not in rb3-Wii nor dc3) — RE-only. |
| **InetAddress** (platform) | scattered in `82afc2e0..` | Oracle is Wii `revolution/rvl/so.h` (SOInetPtoN) → Xbox Winsock calls diverge. C. |

### Reattributions (fold into existing map, NOT fresh carves)
- `823573ac..82357dd0` head of #3's run, and `82348310..823484ec` → **ModifierMgr.cpp**
  scatter (already pinned `82357218..`). `always_show_hud`, `modifier_mgr`, `is_modifier_active`.
- `82588df0..82589594` tail of #12 → **ModifierMgr.cpp** scatter (`mod_auto_vocals`, `save_value`).
- `82614690..82616798` tail of #4 → **ManageBandPanel.cpp** (pinned `82624748`) +
  **CustomizePanel.cpp** (pinned) scatter.
- `82665b90..826662e0` tail of #1 → invite + **StoreMenuProvider.cpp** (pinned `82673B20`) scatter.

---

## Recommended landing order (grade-A first, ascending risk)
1. **#1 StoreOfferProvider pin-ext** — already proven +6/0-LOST in `wt-fp3` (acb2fcf4). Cherry-pick.
2. **#2 BandStarDisplay** + **#5 StreakMeter** — adjacent bandobj TUs, in-tree headers, 175/377 LOC, clean strings. Scaffold both `.cpp` from rb3-Wii, pin, local-static Symbol lever (`/DRB3_HANDLE_LOCAL_STATIC`).
3. **#4 ContentLoadingPanel** (129 LOC) + **#6 SetlistToStorePanel** (65 LOC) + **#7 GigFilter** (61) + **#8 FixedSetlist** (72) — small fresh meta_band/tour, in-tree headers, DataArray-parse bodies.
4. **#3 TrackPanelDirBase** (394) + **#11 FingerShape** (137) + **#10 RGUtl** (531, self-contained chord math).
5. **#9/#14 net glue** (SessionMessages, NetSession/SyncStore) — real oracles, BinStream serialization.
6. **B tail**: #12 SessionMgr, #13 NextSongPanel gap-fill, #15 tracker cluster, #16 BandStorePanel, #17 LayerDir (header port), #18 Text deepen, #19 EnvAnim, #20 Leaderboards.

## Corroboration / reproducibility
- Enumerator: `venv/bin/python scripts/harvest/fp2_runs.py` (→ `/home/free/tmp/fp2_runs.json`)
  then `fp2_final.py`. Sub-split: `scripts/harvest/fp2_span.py 0xSTART 0xEND`.
- Every source path is absolute-verified; every string quoted was grep-confirmed in
  the cited `.cpp`. All spans confirmed zero-overlap with current `.text` pins.
- Proof leg reproducible in `wt-fp3` (`git log` → acb2fcf4).
