# TOURWALK3: closing tourwalk2's open tour items (2026-11-23)

Lane tourwalk3, branch `tourwalk3` (from main `3627ae9eb`), worktree `~/tmp/wt-tourwalk3`.
Scratch, binaries and logs: `~/tmp/tourwalk3/` (`r_<tag>/run.log`). Scripts: `tools/rb3_app_walks/tourwalk3/`
(`runb.sh`/`build.sh` = tourwalk2's, pointed here; the `full` leg reuses `tourwalk2/w_full.txt`).
Every run went through `coord/no610m.sh` under `gpu1.lock` with `RB3_APP_DETERMINISTIC=1
RB3_APP_FOCUS_LOG=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_WHAMMY=od
RB3_APP_CHARTPLAY_TILT=1` (real pad input from the chart). Previous lane:
`TOURWALK2_ROAD_CHALLENGES_DEEPER_2026-11-23.md`. The full native selftest was not run (coordinator's job).

## Commits

| commit | what |
|---|---|
| `23df0c1b7` | `RB3_APP_FOCUS_LOG=1` also logs every content-package mount and `WriteFile` (`rb3-app: content mount root=… pkg=…`, `content write fd= bytes=`), so a walk sees each profile save. `native/src/xdk_shims.cpp`, log only |
| `aaccbca6c` | enumerators for `TourGameType`, `TrackerChallengeType` (names and values from `macros.dta`) and `TrackerPlayerDisplay::NetDisplayMsg` (retail names unknown; named after the sending method). Δ0, below |
| `9c9dd265e`, `e960bcee6` | `build.sh`, `runb.sh`, `w_custom*.txt` (w_custom4 committed with this record) |
| `5ce1ee3b6` | the tour focus line adds `awards=<b><s><g>` (profile holds each medal accomplishment's award), `assets=` (closet assets held), `meta=` (meta score) and `level=` (campaign level for that score) |

## Binaries

| binary | sha256[:12] | tree |
|---|---|---|
| bin3 | f34245af81fb | `aaccbca6c` + the main_app log edit, committed unchanged as `5ce1ee3b6` |

## Predictions

1. That native's autosave trigger differed from retail's, since a mid-gig reboot lost the first song's stars.
   **Wrong.** Retail never writes an in-progress gig to the profile, and native matches it (item 1).
2. That a whole tour would show some award or unlock divergence. **Wrong.** Every reading matches
   retail's data (item 2). The custom setlist was not reached.
3. That the enumerators would price Δ0. **Right** (item 3).

## 1. Stars lost on a reboot mid-gig: retail behaviour, no fix

- **Retail does autosave between the songs of a gig.** The TU5 `ui/endgame/gen/endgame.dtb`
  (decrypted with `tools/rb3_app_walks/trainwalk/dtb.py`) ends `coop_endgame_panel`'s `enter` with
  `{saveload_mgr autosave}`, so a save is asked for after every song. Native does the same.
  With the new mount log (leg `custom`, bin3), the `band3` profile package is written at frame 10853
  (song 1's `coop_endgame_screen` appeared at 10851) and at 15455 (song 2's at 15453), 4,434,217 bytes each
  time. The `full` leg saves at 9280, 15558, 27632, 36523, 50449 and 59858, one per song endgame, plus
  the saves after the results screens.
- **But the profile has no field for the stars of a gig in progress.** The profile path is
  `TourProgress::SaveFixed`, not `SyncSave`. `SyncSave` is the `MetaPerformer` net-sync stream; tourwalk2's
  "SyncSave writes mNewStars, so a save after song 1 would have kept them" read the wrong one.
  Retail `SaveFixed` at `0x823627c0` (read with `va_disasm.py`) writes, in order: +0x50 last-touch time,
  +0x08 quests, +0x2C tour properties, +0x54 on-tour byte, +0x58 tour desc (symbol ID), +0x5C completed gigs,
  +0x6C current quest, three null symbols (the quest filters, `gNullStr` loaded from `0x82C71838`) and the +0x60
  finished-gig star vector (`SaveStd …, 10, 4`). Two fields are missing: `mNewStars` (+0xCC, where `EarnStars`
  `0x823625e0` adds) and the current gig number. Native's `SaveFixed`/`LoadFixed` have the same field list.
  `EarnStars` tail-calls `HandleDirty(2)` (`0x82362390`), which calls `SetDirty(true, 2)`. That sets only
  the upload flag, so the stars alone do not even make the profile save-dirty. The per-song saves still
  happen because the song's scores dirty the profile.
- So after any reboot, retail resumes the tour at the last *finished* gig, with the finished gigs'
  stars. That is exactly tourwalk2's reboot reading (`cur=0 stars=20` at boot after gig 2's song 1).
  The gig is replayed from song 1 (`HandleSongCompleted`'s first-song arm calls `ClearNewStars`).
  Native = retail; nothing to fix.

## 2. A whole tour (leg `full`, bin3, tourwalk2's `w_full.txt`, 64,000 frames)

All three gigs on real input: 10+10, 10+10 (deploy quest by tilt), 10+10, so `stars=50`, `gigs=3/3`.
That reproduces tourwalk2's bin2 run song for song. The new fields:

| frame | reading |
|---|---|
| 59852 (gig 3 endgame) | `most=30 medals=000 accs=0 awards=0 assets=5 meta=95 level=campaignlevel_nothing` |
| 60349 | `most=50 medals=110` (`UpdateMostStars` on the results screen) |
| 60355 | `accs=3 awards=3 assets=9 meta=140`: bronze (35) and silver (45) accomplishments, their awards and 4 more closet assets |
| 60459 | `tour_finished_screen`, then `accomplishments_newaward_screen`, `tour_quest_outro`, `tour_customize_post` with `tours.lst sel=0:tour_local1` |

- **Medals:** `acc_grp_tour.dta` sets `tour_local1`'s goals at `tour_most_stars` 35 / 45 / 55. 50 stars gives
  bronze and silver and not gold, which is what the log shows.
- **Gear award:** bronze awards `uncommon_nightin` (assets `flatslips_stripe`, `hotpantsplain_barelegs`) and
  silver `uncommon_stoner`. Both are held after the tour (`awards=3`), and the closet grows from 5 to 9 assets.
  The 5 before it came from song accomplishments during the tour (`assets=0` at the tour's start). Leg
  `custom2` crossed bronze alone (40 stars after gig 2): `awards=1`, assets 3 → 5, so each of the two
  awards adds its 2 assets.
- **Later tours:** a tour is gated only by `required_campaign_level` (`TourDescProvider::IsTourDescAvailable`
  → `Campaign::HasScoreReachedCampaignLevel` on the leader's meta score). `tour_local1` and `tour_local2` need
  `campaignlevel_nothing` (0), so both are open from the start. `regional*` need `campaignlevel_van`
  (250 points off-PS3) and `continental*` need `campaignlevel_bus` (450). One tour pays `career_fans_tour`
  (15) per medal accomplishment. The walk ends the tour at meta 140, still `campaignlevel_nothing`.
  So finishing a tour unlocks nothing by itself, in retail's data as here. Reaching `van` needs more
  meta score than one tour can give. Not walked: scrolling `tours.lst` to read the locked rows' unlock labels.
- **Venues:** `tour_descs.dta` names one venue per gig; there is no tour-driven venue unlock. The only venue
  unlock in the data is `award_videovenues` (key `key_video_venues`), from a song-count band accomplishment
  (`acc_grp_band.dta`), outside tour.
- **After the tour:** `TourProgress` resets (`tour= on=0`). The walk's next press starts `tour_local1` again with
  `most=50 medals=110` kept: best-ever stars persist across tours, as `GetTourMostStars` reads the
  accomplishment progress.

### Custom setlist

**Not reached.** The custom rows are gig 2 row 2 and gig 3 rows 1-2 (TU5 `config/gen/tour_descs.dtb`
`setlist_type`: gig 1 `tour01_gig01 random random`, gig 2 `tour01_gig02 random custom`, gig 3
`random custom custom`). Gig 1 has no custom row. `w_custom` picked its row 2 and got a second random filter
(`filter_the00s`, `livinginamerica,getfree`, 10+10 stars). Four gig-2 attempts all went to row 0
(the fixed set), each for a different reason:

- `w_custom2`: the results section's blind `+700…+1300 a` (copied from `w_full`) fire after the guide is
  up and pick row 0 before the script reaches the next `wait_screen`. This is also why tourwalk2's
  `g2probe` and `full` never left row 0 on gigs 2 and 3.
- `w_custom3`: one `+400 a` does not clear `accomplishments_newaward_screen` (it took a press only about
  700 frames after appearing in tourwalk2). So the `down`s went to that screen.
- `w_custom4` (stopped at frame ~20,300 once it was playing the fixed set): newaward cleared at +700,
  guide at frame 17184. `down` at +199 and +259 and `a` at +349 were all ignored (no `sel=` change).
  The next `a`, at +2352, was taken. tourwalk2's gig 2 took an `a` at +570.

So on gigs 2-3 the guide ignores input for somewhere between +350 and +570 frames after it appears,
while gig 1's took `down` at +251. Most likely this is the map camera moving to the next gig; not checked
against retail. Next step: `w_custom4.txt` with the guide presses moved to `+700 down`, `+760 down`, `+850 a`.
After that, song select inside a gig is still unwalked; the `RB3_APP_SONGSEL_LOG=1` line is already in
these legs' env.

## 3. The empty enums: declared, Δ0

`ab_measure --revert aaccbca6c` (run `~/tmp/wt-tourwalk3/.ab_measure_runs/20261123-193422-tourwalk3-enums-*`):
leg B recompiled 259 objects. Shipped ruler `functionRelocDiffs=name_check`: Δmatched +0, Δmasked_equal +0,
Δhonest +0, Δcode_bytes +0, Δfuzzy +0.000000 pp (64.709015 both legs). Units at 100% were 635 on both legs,
and the `none` control was +0 B. On native the three enums now hold every value the data and tracker
store, which removes the UBSan "not a valid value" class tourwalk2 saw (TourGameRules.cpp:49,
TrackerManager.cpp:386, Tracker.cpp:205/208/209, TrackerDisplay.cpp:88/332). The casts at the use sites are
unchanged. No ASan/UBSan leg was rerun in this lane, so that removal is by construction, not measured.

## Not done

- Native selftest (coordinator's).
- A UBSan rerun of the tour walk to show the enum reports gone.
- A custom setlist inside a gig (above: the gig-2/3 guide input delay).
- Walking the locked tours' rows in `tours.lst`, and a second tour (`tour_local2`).
- `TourDescProvider::IsTourDescAvailable` was read in source only; its retail body (`0x82b7be50`
  in the map) was not compared.
