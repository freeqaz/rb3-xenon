# TOURWALK: rb3-app walked through tour mode under autoplay (2026-11-23)

Lane tourwalk, branch `tourwalk` (from main `875c9a8d2`, with `menuwalk` merged in for
`RB3_APP_FOCUS_LOG=1`), worktree `~/tmp/wt-tourwalk`. Scratch, logs and binaries are in
`~/tmp/tourwalk/` (`r_<tag>/run.log`). The walk scripts are committed in
`tools/rb3_app_walks/tourwalk/`. `runb.sh` is a copy of `rb3-scout/a25/runb.sh` with
`A=~/tmp/tourwalk` and an optional `CONTENT=<store dir>` that replaces `memcard/content_a`. Every
run went through `coord/no610m.sh` under the shared `gpu1.lock`, used
`RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1` and the default (vocals) pad, and played the songs
with `RB3_APP_AUTOPLAY=1`. Engine: snapshot `eng-2dcbb17` (= engine HEAD). The full native
selftest was not run; that is the coordinator's job.

## Commits

| commit | what |
|---|---|
| merge of `menuwalk` | `RB3_APP_FOCUS_LOG=1` and the menuwalk scripts, as the brief allowed |
| `083684317` | `RB3_APP_FOCUS_LOG=1` also prints `rb3-app: frame N tour=<desc> on=<0/1> gigs=<done>/<total> cur=<gig> stars=<n> won=<0/1>` whenever the tour state changes. It only calls read accessors (native/src/main_app.cpp only). Also adds the walk scripts |

No `src/` file changed, so no `ab_measure` run was needed.

## Binaries

| binary | sha256[:12] | tree |
|---|---|---|
| bin-t0 | 49fe97de2bd0 | main `875c9a8d2` + menuwalk |
| bin-t1 | 652d1aa41a09 | `083684317` (adds the tour log) |

## Route (read from the focus log)

main_hub `mb_playnow.btn` → `pn_quickplay.btn` → down to `pn_tour.btn` → `tour_new.btn` →
tour_customize_screen → tour_customize_post_screen (`tours.lst`, sel 0 = `tour_local1`) →
tv5_a_screen → tour_welcome_screen (`continue.btn`) → tour_gigguide_init → tour_gigguide_screen
(`filters.lst`, sel 0 = `tour01_gig01`) → tour_part_difficulty_screen (overshell slot 2:
`choose_part_vocals`, then `choose_diff`) → tour_post_seldiff_screen → preloading → tv3_b →
game_screen.

After each song comes endgame_waiting → coop_endgame_popups → coop_endgame_screen. Confirm there
goes to preload_nextsong for the gig's next song. After the gig's last song it runs
`{tour performer} complete_quest` and goes to tour_challenge_results_screen. From there:
meta_loading_continue → accomplishments_newaward_screen → tour_quest_outro_screen →
tour_gigguide_screen on the next gig. This is the retail flow in `ui/endgame/endgame.dta` and
`ui/tour/*.dta`.

Walk-timing lessons (the waits themselves were fine):
- The gig guide takes its first press only after about 340 frames. The probe walk's two presses
  at entry +160 and +340 both land; one press at +200 or +400 is dropped.
- A tour gig ends on coop_endgame_screen, not on the results screen. A walk that waits only for
  tour_challenge_results_screen sits on coop_endgame forever.

## Results

### 1. Gig 1 of tour_local1: progression correct (t7, bin-t1, 2 songs, qst_tier1_streak)

The tour log over the gig:

```
frame   978  tour=tour_local1 on=1 gigs=0/3 cur=0 stars=0  won=0   (setup_new_tour)
frame  7926  tour=tour_local1 on=1 gigs=0/3 cur=0 stars=10 won=0   (song 1: 5 song + 5 challenge)
frame 15552  tour=tour_local1 on=1 gigs=0/3 cur=0 stars=20 won=0   (song 2)
frame 15857  tour=tour_local1 on=1 gigs=1/3 cur=0 stars=20 won=1   (CompleteQuest)
frame 16481  tour=tour_local1 on=1 gigs=1/3 cur=1 stars=20 won=1   (gig guide, gig 2 selected)
```

Twenty stars is the gig's maximum (`GetTotalStarsPossibleForCurrentGig` = 2 songs × 10). The
quest is won and the gig count advances. The gig guide then offers `tour01_gig02`. Retail check:
`QuestManager::CompleteQuest` (0x823598F0) and `TourProgress::UpdateMostStars` (0x82362890)
were read with `tools/va_disasm.py` and match our source call for call. Across all tour walks,
`^FAIL` 0, `UNREACHED` 0, and no `not function or object` lines.

### 2. Saving and reloading across gigs: works (rl1, bin-t1)

`save.dat` was snapshotted after gig 1 (t6, bin-t0, same deterministic timeline) and the game
was booted on it (`CONTENT=snap_g1`, `w_reload.txt`):

- Boot goes splash → main_hub, with no calibration or first-run hints.
- tour_customize_screen's enter shows `tour_local1 on=1 gigs=1/3 stars=20`: progress loaded.
- Choosing the same tour pushes **tour_newcontinue_screen** (`is_underway`, same tour). Continue
  (`opt1.btn`) reaches the gig guide at `cur=1`, with `tour01_gig02` selected.
- `won` reads 0 after the reload. That is retail-faithful: `TourProgress::SyncSave`/`SyncLoad`
  do not serialize `mWonQuest`.

### 3. Gig 2 (qst_tier1_deploycount): 5 of 10 stars on song 1. This is a walk artifact, not a bug

Song 1 of gig 2 adds 5 stars (song stars only, 0 challenge stars). The det line at the end of
the song reads `energy 1.0000 deploy 0`. An autoplayed singer fills overdrive but never
deploys it, because deploying is a player action. A deploy-count challenge therefore earns
nothing. To exercise this quest, a later walk needs a scripted deploy press.

### 4. The whole tour finishes (t7, bin-t1, 57,472 frames, rc 0, `^FAIL` 0)

```
frame 37729  gigs=1/3 cur=1 stars=30 won=1   (gig 2 song 2: 5 more stars, again 0 challenge stars)
frame 38034  gigs=2/3 cur=1 stars=30 won=0   (deploy-count quest lost, gig still counts: retail CompleteQuest)
frame 46713  gigs=2/3 cur=2 stars=40         (gig 3, qst_tier1_accuratesectioncount: 10)
frame 55231  gigs=2/3 cur=2 stars=50         (10)
frame 55536  gigs=3/3 cur=2 stars=50 won=1
frame 55838  tour_finished_screen; frame 56136 tour reset (on=0); accomplishments_newaward_screen
```

From the results screen, `are_all_tour_gigs_complete` sends the walk to tour_finished_screen. The
newaward screen then follows, and `Achievement awarded: acc_tourplayedlocal1` is logged. The
walk's leftover presses then start tour_local1 again (`gigs=0/3`) through tour_customize. The
final 50 of 60 stars passes the bronze (35) and silver (45) `tour_most_stars` goals in
`acc_grp_tour.dta`, but not gold (55). **Not verified:** whether bronze and silver were
recorded as earned. No log line names the accomplishments the newaward screen showed. That is
the next thing to check, with an `acc_mgr` earned-goal line in the focus log or by reading the
reloaded profile.

## Bugs found

None in `src/band3` (meta_band, tour, game) along this path. Every progression, reward-count
and save/load observation above matches the retail flow and the two retail functions checked.
No fix was committed. The prediction at the start was that tour screens would show missing
native pieces (stubs, DTA handlers) as menuwalk had expected. **Wrong:** the whole route ran
with 0 FAIL, 0 UNREACHED and 0 unknown handlers.

## Listed, out of scope or not done

- Not walked: tour_confirm_new (starting a
  second tour over an underway one), tour_notavailable, and FMP (online) tour. No ASan run.
  The deploy-count quest was not exercised with a real deploy.
- The results screen accepts Confirm 290 frames after it opens (`ready_to_continue` is set by
  the reveal-trigger chain). This was not compared with retail timing.
- Nothing was found in ui, rndobj, char, joypad or BandCharacter.
