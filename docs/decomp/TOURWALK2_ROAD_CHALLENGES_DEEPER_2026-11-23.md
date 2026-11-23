# TOURWALK2: rb3-app deeper into Road Challenges (tour) by real input (2026-11-23)

Lane tourwalk2, branch `tourwalk2` (from main `6dd53794b`), worktree `~/tmp/wt-tourwalk2`.
Scratch, binaries and logs: `~/tmp/tourwalk2/` (`r_<tag>/run.log`). Scripts committed in
`tools/rb3_app_walks/tourwalk2/` (`runb.sh` = songselwalk's copy with `A=~/tmp/tourwalk2`).
Every run went through `coord/no610m.sh` under `gpu1.lock`, with `RB3_APP_DETERMINISTIC=1
RB3_APP_FOCUS_LOG=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1` (real pad input from the
chart, not autoplay) and, where overdrive mattered, `RB3_APP_CHARTPLAY_WHAMMY=od
RB3_APP_CHARTPLAY_TILT=1` (deploy by tilt). Previous lane: `TOURWALK_RB3APP_TOUR_MODE_WALK_2026-11-23.md`
(vocals autoplay, fixed setlists, 3 gigs, save/reload between gigs). The full native selftest was
not run (coordinator's job).

## Commits

| commit | what |
|---|---|
| `e16cd9b27` | `RB3_APP_FOCUS_LOG=1`'s tour line also prints `quest= filter= custom= random= set=<MetaPerformer songs> gig=<song:base+quest=total,...> most=<best-ever stars> medals=<b><s><g> accs=<bitmask of the tour's bronze/silver/gold accomplishments held>`. Read-only accessors, `native/src/main_app.cpp` only |
| (this record) | record + scripts |

No `src/` file changed, so there was nothing to price with `ab_measure`.

## Binaries

| binary | sha256[:12] | tree |
|---|---|---|
| bin1 | b2fbb8f8cbc0 | `6dd53794b` + first log edit |
| bin2 | 203ef43a9960 | `e16cd9b27` (adds `set=`) |
| bin1asan | (build-asan, `-DRB3X_SANITIZE=address,undefined`) | bin1's tree |

## Prediction

That random setlists, a failed song and a mid-gig reboot (the three things tourwalk never
reached) would turn up a native-only divergence. **Mostly wrong**: everything reached so far
reads as retail's code and data; details below.

## Results

### 1. Setlist kinds: there is no "vote" setlist in TU5

`config/tour_descs.dta` gives each gig three `setlist_type` entries, one per row of the gig
guide's `filters.lst`; the only values are a fixed set name (`tour01_gig01` ...), `random` and
`custom` (`QuestFilterProvider::GetSetlistType`: `random`, `custom`, else fixed). No `vote` exists in
the ui, config or the source (`grep -i vote` hits only locale strings about T-shirts). So the
"vote" part of the brief has nothing to walk on disc.

### 2. Random setlist (leg `full`, bin2): correct, including retail's missing-part warning

Row 1 of gig 1 (`+250 down` on the opened list) picks `filter_classicrock`, `random=1`, and the
performer's setlist is `werewolvesoflondon,imagine`. Choosing guitar then raises the overshell's
`kState_ChoosePartWarn` (78, `overshell_choose_part_warn`: "At least one of the songs in the
setlist is missing this part"). That is correct: `songs.dta` gives `imagine` `(guitar 0)`, and
`OvershellSlot::SelectPartImpl` asks `MetaPerformer::SetHasMissingPart`. A walk on a random gig
needs one extra press for that dialog (`gig N x` in the script). Song 1 by real input:
`werewolvesoflondon:5+5=10`.

### 3. Challenge stars by real input, including the deploy-count quest tourwalk could not earn

- Gig 1 (`qst_tier1_streak`, fixed set, bass by real input): `iloverocknroll:5+5=10,
  lastdance:5+5=10`, quest won, `gigs=1/3`, `most=20` after `CompleteQuest`.
- Gig 2 (`qst_tier1_deploycount`) with deploys by tilt: `yoshimibattles:5+5=10`, det line
  `deploys 2`. Tourwalk's autoplayed singer never deployed and got 5+0; real deploys earn the
  5 challenge stars. Quest stars come from `TrackerManager::GetQuestEarnedStars` clamped to 5
  (`TourPerformerImpl::UpdateGigDataForSong`; retail `0x82361240`, and the caller
  `HandleSongCompleted` `0x82361310`, read with `va_disasm.py`, match call for call).

### 4. Losing a song in a gig (Medium; Easy never fails, as in retail)

- On Easy the band cannot fail (`Player::CheckCrowdFailure`, see dropwalk); the first lose legs
  on Easy finished the song at 0 stars. Re-run on Medium (`+610 down` on `choose_diff`).
- `loseres`: all gems skipped → `lose_screen` at frame 3376 (focus `lose_resume.btn`; tour mode
  has `can_resume_songs TRUE`). Resume = `game_resume_no_score`: the rest of the song is played
  but unscored, `hit 0 miss 0`, the gig records `iloverocknroll:0+0=0` and the next song loads.
  That is retail's `lose.dta` flow.
- `losequit`: `lose_quit.btn` (`overshell_quit_tour`) → `lose_confirm_quit_screen` (the yes/no
  dialog is vertical: `up` reaches yes, `left` does nothing) → `meta_loading_continue_screen` →
  `tour_welcome_screen` → gig guide on the same gig, `gigs=0/3 stars=0`, tour still on. The gig
  is not counted, as `CompleteQuest` only runs from the results screen.

### 5. Reboot mid-gig (leg `reload`, bin2, store snapshot `snap_g2mid`)

The store was taken from leg `g2probe` killed at frame 36,000: gig 1 done (20 stars), gig 2
song 1 confirmed (in memory `stars=30`), song 2 finished but its endgame not confirmed. Boot on it:
`tour_local1 on=1 gigs=1/3 cur=0 stars=20 most=20` → same tour → `tour_newcontinue_screen` →
continue → gig guide at `cur=1`, `stars=20`. So the mid-gig song's 10 stars were not in the store.
`TourProgress::EarnStars`/`ClearNewStars`/`RemoveStars` match retail (`0x823625e0`,
`0x82362688`, `0x823625c8`: `mNewStars` at `+0xCC`, then `HandleDirty(2)`), and `SyncSave`
writes `mNewStars`, so a save after song 1 would have kept them. Whether retail writes the profile
between the songs of a gig was **not settled**: either retail does not save there (then this is
retail), or native's autosave trigger differs. Next step: log the memcard save calls in a
mid-gig window (`SaveLoadManager` state changes) and compare with retail's autosave triggers.
Either way, on retail the first song of a gig replays from `ClearNewStars` (`HandleSongCompleted`
`IsFirstSong` arm), so the gig restarts from song 1 after any reboot.

### 6. ASan + UBSan (leg `asan`, bin1asan, the `full` script)

At the time of writing it had reached gig 1 song 1 on the random set. 18 reports, all at sites already
in the asanwalk, asanwalk2, practicewalk or songselwalk records (SongParser.cpp:234 and
GameGem.h:84/85 `0xBE` fill, StandardStream.cpp:689, BeatMap.cpp:90, ChunkStream.cpp:501,
BinStream.cpp:33, BandHeadShaper/BandFaceDeform misaligned `Delta`). No AddressSanitizer
report. The tour screens added no new site.

## Not done / handoff

- **Still running at hand-off** (started ~18:55, bound by `timeout`): `r_full` (bin2, 64,000
  frames: three random gigs, results, `tour_finished_screen`, newaward screens; read the `tour=`
  lines for `medals=` / `accs=` after gig 3: bronze 35 and silver 45 should flip to `accs=3` if
  the walk reaches 45 stars, and gold 55 to 7) and `r_asan` (18,000 frames). Check with
  `command grep -a -E 'tour=|Achievement|RESULT' ~/tmp/tourwalk2/r_full/run.log | tail` and
  `command grep -a -E 'runtime error|AddressSanitizer' ~/tmp/tourwalk2/r_asan/run.log`.
- Custom setlists (song select inside a gig) were not reached: in `g2probe` the gig-2 presses
  landed on row 0 (fixed set). The script `w_g2custom_probe.txt` has a stray extra welcome press
  (the bug fixed in `pre.txt`); rebuild it from `pre.txt` with `gig 2` and probe song select.
- Unlocking later tours (`tour_local2` etc. via `required_campaign_level`) and the gear award
  (`acc_tourbronzelocal1` → `uncommon_nightin`) depend on the `full` leg finishing.
- No `src/` bug was found or fixed in this lane; nothing needed `ab_measure`.
