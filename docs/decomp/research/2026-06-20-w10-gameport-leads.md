# W10 — gameport-leads (DISCOVER/PLANNER, READ-ONLY in main)

**Date:** 2026-06-20 · **Mode:** Opus discover/planner, READ-ONLY in main @ d910dd9
(9037 matched). **Area:** gameport-leads — verify the W9 deep-loop game-port TU
leads near the landed 0x825fc/0x8263x meta_band cluster, emit self-contained
port-then-pin work-items.

## TL;DR verdict

The W9 leads are REAL but **partially consumed by wave-9 itself**. Ground-truth
recheck (COFF auto_03 text + auto_00 rdata, splits.txt, report.json):

- **ALREADY LANDED in wave-9 (do NOT re-emit):** SongSortMgr (78/168 matched),
  SongUpgradeMgr (40/57), StoreMainPanel (33/70), SongSelectPanel (34/68),
  BandSongMgr (63/176, which consumed the L3 `fn_8255F858` cluster). The
  port-then-pin pattern is validated as working end-to-end here.
- **STILL ACTIONABLE (clean, emitted as work-items):** LicenseMgr (cleanest —
  perfect neighbour-bounded gap), AppMiniLeaderboardDisplay+base
  MiniLeaderboardDisplay, MetaPanel (light-method cluster), StoreMenuPanel,
  plus the broader VoiceoverPanel-megacluster panels (one item each).
- **DEFERRED (scattered/multi-TU/recon-gate):** Campaign (200+-fn multi-TU gap,
  pulls in CampaignKey/Level/Era), the full megacluster boundary-derive scout,
  MetaMusic (co-resident with MetaPanel, needs its own derive).
- **No foundational/shared-header lever found.** The AppMini base layout is a
  per-TU bandobj-vs-hamobj header swap (consumed only by AppMini), NOT binary-wide.
  `flag_foundational=false`.

## Ground truth established (all verified this session)

COFF tooling at `/tmp/w10check.py` (VA-from-name fn enumeration + lis/addi/lwz
string-ref resolver over auto_03), `/tmp/readstr.py` (rdata string read by VA).

### Validated wave-9 landings (report.json measures.matched_functions per unit)
| unit | matched/total | code% | pin span |
|---|---|---|---|
| SongSortMgr | 78/168 | 24.1 | 0x82580040–0x82583DD8 |
| SongUpgradeMgr | 40/57 | 57.7 | 0x82630988–0x82632040 |
| StoreMainPanel | 33/70 | 22.5 | 0x8261E020–0x8261FE68 |
| SongSelectPanel | 34/68 | 44.8 | 0x8261AAF0–0x8261C660 |
| BandSongMgr | 63/176 | 17.3 | 0x8255DE88–0x82563500 |

Calibration: a clean panel TU (~70 fns) yields ~30–35 matched; a manager
(~170 fns) ~60–78. Use these for EV.

### Still-unpinned, verified-identity targets
- **LicenseMgr** [0x82632040, 0x82632F00) — 41 fns. fn_82632040→`licenses.dta`
  (rdata 0x820ceeb8), fn_82632050→`licenses` (0x820ceec8). Neighbours are EXACT
  pin edges: below SongUpgradeMgr ends 0x82632040, above Instarank-ctor TU starts
  0x82632F00. **Zero-slack clean gap — the safest pin of the batch.** Header in
  tree already has retail caching shape (`set<Symbol> mLicenses //0x4` + cache
  methods). Wired NonMatching, UNPINNED.
- **AppMiniLeaderboardDisplay** derived TU ~[0x8262F530, 0x82630988) + base
  **MiniLeaderboardDisplay** ~[0x8262E974, 0x8262F530). fn_826301E0→`leaderboard`
  /`title_label`/`icons_label` = AppMini::Update (confirmed). fn_8262F4B0→base
  classname `MiniLeaderboardDisplay` (0x8202FFE8). ⚠ AppMini end must bound at
  0x82630988 (SongUpgradeMgr pin start) — L4's 0x82630990 overlaps by 8 bytes.
  ⚠ LAYOUT TRAP (confirmed): xenon has only DC3's `system/hamobj/
  MiniLeaderboardDisplay.h` (extra mResourceDir/OldResourcePreload/OBJ_MEM_OVERLOAD
  members → offset shift). AppMini.h `#include "hamobj/MiniLeaderboardDisplay.h"`.
  rb3-Wii has correct `system/bandobj/MiniLeaderboardDisplay.{h,cpp}`. The item
  MUST port the bandobj base (h+cpp) and re-point the include. 2-TU port.
- **MetaPanel** light-method cluster — fn_8255B638→meta_music(0x8209B6C4)/
  send_back_sound_msg_to_all(0x8209B6A8)/sync_game_timer(0x8209B698) = Handle
  (confirmed). Body methods ~[0x8255AECC, 0x8255BA10): fn_8255AECC→TriggerBackSound
  message table + BandEventPreviewMsg; fn_8255AF30→AppendSongToSetlist;
  fn_8255AFA8→RemoveLastSongFromSetlist. MetaMusic body starts fn_8255BA10
  (→sfx/shell_fx.milo/synth/metamusic/music) = the TU split. ⚠ Init() is COMDAT-
  scattered out of span (panel-registration block ~0x82559E64) → stub it.
  ⚠ Lower edge fuzzy (MoviePanel/earlier sliver below) — needs per-fn derive.
  Neighbours: Meta.cpp sliver ends 0x825595F8 below, BandSongMgr starts 0x8255DE88
  above (clean). MetaPanel.h in tree (mMusic@0x5c, mTour@0x38, full map).
- **StoreMenuPanel** [~0x8261FE68, 0x826211E8) — fn_8261FF18→`menu_list`
  (0x820ca5f8). 12 methods, 167 lines, NO Wii deps. Below = StoreMainPanel pin
  ends 0x8261FE68 (clean abut); above = NextSongPanel/NameGenerator region.
- **VoiceoverPanel** [0x826134E8, 0x82613568) — ⚠ Cam.cpp pin [0x82613568,
  0x826135CC) sits INSIDE L4's claimed span 0x826134E8–0x82614318. The
  VoiceoverPanel pin MUST stop at Cam start 0x82613568 (overlap hazard). The
  region 0x826135CC–0x8261AAF0 (above Cam, below SongSelectPanel) holds more
  panels (finding_presence/MetaPanel-FindPlayers per L4) — VoiceoverPanel's full
  span needs derive. set_voiceover_symbol/play_voiceover anchors confirmed in L4.
  ⚠ Wii dep: includes `os/ContentMgr_Wii.h` → swap to 360 `os/ContentMgr.h`.

## Deferred / refuted

- **Campaign** [~0x82590910] — anchor confirmed (campaign_levels 0x820a7470,
  campaign_keys 0x820a7460 at fn_82590910). BUT nearest pins are 0x8258be40 below
  and 0x82595540 above — a 200+-fn unpinned gap holding MULTIPLE TUs (Campaign +
  CampaignKey[already pinned @0x826421B8 elsewhere]/CampaignLevel/CampaignEra/
  Performer). Not a clean single-TU pin; recon-gate the boundary first. DEFER to
  frontier (needs its own boundary-derive scout).
- **MetaMusic** [0x8255BA10+] — co-resident with MetaPanel; own TU, but the
  MetaPanel/MetaMusic split + the MoviePanel sliver below MetaPanel both need a
  per-fn boundary-derive. Bundle into the MetaPanel item's derive step or defer.
- **Megacluster middle panels** (CustomizePanel, EditSetlist, MainHub, ManageBand,
  MultiSelectList, NewAward, PatchSelect, Training, TokenRedemption,
  SelectDifficulty, StoreInfo) — REAL forest, but COMDAT-interleaved with fuzzy
  string-anchor boundaries. Each needs a per-fn boundary-derive before pinning.
  Emit InterstitialPanel + NewAwardPanel as the cleanest mid-cluster items only
  after a boundary-derive scout; the rest are frontier.

## Pin-safety self-check (wave-9 lesson 3) — RAN this session
Overlap audit of every candidate against splits.txt neighbours:
- LicenseMgr: CLEAN (exact edges).
- MetaPanel-light, StoreMenuPanel: CLEAN.
- AppMini: OVERLAP at 0x82630988 (SongUpgradeMgr start) → bound end to 0x82630988.
- VoiceoverPanel: OVERLAP at Cam.cpp 0x82613568 → bound end to 0x82613568.
Every emitted item carries an explicit overlap-bound instruction + a
"re-run overlap self-check before declaring landable" step.

## Tools (reusable)
`/tmp/w10check.py fns <lo> <hi>` / `refs <va> <len>`; `/tmp/readstr.py <va...>`;
`/tmp/coffparse.py`. Oracle: rb3-Wii `~/code/milohax/rb3/src/band3/meta_band` +
`~/code/milohax/rb3/src/system/bandobj`.
