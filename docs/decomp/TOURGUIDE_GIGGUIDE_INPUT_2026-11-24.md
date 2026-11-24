# TOURGUIDE: the tour gig guide's "ignored input" is the walk script, not a delay (2026-11-24)

Lane tourguide, branch `tourguide` (from main `f25eee490`), worktree `~/tmp/wt-tourguide`.
Scratch, binaries and logs: `~/tmp/tourguide/` (`r_<tag>/run.log`, decoded DTA in `dta/`).
Scripts: `tools/rb3_app_walks/tourguide/` (`build.sh`/`runb.sh` = tourwalk3's, pointed here;
`w_custom5.txt`). Previous lane: `TOURWALK3_TOUR_CLOSEOUT_2026-11-23.md` (its frame numbers are
legacy 30 Hz; every frame below is the 60 Hz frame of the run named, unless it says "legacy").
Every leg ran through `runb.sh` (RB3_APP_HZ=60, `gpu1.lock`, `no610m.sh`) with `RB3_APP_SONGSEL_LOG=1
RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1
RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1`.
The native selftest was not run: the only `native/` change is one focus-log line (below), and the
task said not to land.

## Question

tourwalk3 read the gig 2/3 guide (`tour_gigguide_screen`) as ignoring joypad input for legacy
+350..+570 frames after it appears ("most likely the map camera moving"), which kept every walk on
row 0 (the fixed set) and away from the custom setlist rows.

## Prediction (before the runs)

That there is no delay. The guide has two states, and the walk scripts' gig 2 sections began in the
wrong one. Predicted for w_custom5 (gig 2 opening `a, down, down, a`, with the `down`s at legacy +260
and +290, inside the window tourwalk3 called dead): `sel=1`, then `sel=2`, then song select with
`custom=1`. **Right** (section 3).

## 1. Retail's data: the guide eats every non-confirm press until a first confirm

`tour_gigguide_panel` is a `QuestFilterPanel` defined in `ui/tour/gen/tour_quests.dtb` (decrypted with
`tools/rb3_app_walks/trainwalk/dtb.py`; text in `~/tmp/tourguide/dta/tour_quests.dta`). The file
used is `orig-assets/extracted-xbox-full/ui/tour/gen/tour_quests.dtb`, 11,672 bytes, and those exact
bytes occur in `orig-assets/xbox-zip/gen/main_xbox_2.ark` at offset `0xcac843`. That is the archive
rb3-app loads, and the dtb is stored under its own per-file crypt key, so this is the same file, not
a lookalike. The panel declares `(is_showing_filters FALSE)`, its `enter` sets it FALSE, and:

```
(BUTTON_DOWN_MSG
  {switch $action
    (kAction_Confirm
      {if_else [is_showing_filters]
        {do {$this handle_filter_selected} {play_instr_sfx $user button_select}}
        {do {$this show_filters} {play_instr_sfx $user button_select} 0}})
    (kAction_Cancel ...)
    {if_else [is_showing_filters] kDataUnhandled 0}})
```

So on entry the filter list is hidden. The first confirm only runs `show_filters` (sets the flag and
fires `continue.trg`). Any other action, `down` included, returns `0`, which is handled, so the press
never reaches `filters.lst`. Only after the first confirm does `down` fall through as `kDataUnhandled`
to the list, and the next confirm calls `handle_filter_selected` on the highlighted row.

Dispatch order (why a handled `0` stops the list from seeing the press): `UIScreen::Handle` forwards to
the focus panel. `UIPanel::Handle` runs its class handlers, then `HANDLE_SUPERCLASS(Hmx::Object)`,
which runs the DTA type handler (the `BUTTON_DOWN_MSG` above). Only if that returns unhandled does it
reach `HANDLE_MEMBER_PTR(mDir)` (the PanelDir, then the focused component). `QuestFilterPanel::Handle`
has no button handler of its own (`HANDLE_SUPERCLASS(TexLoadPanel)`). All three bodies match retail
TU5 in main's `build/45410914/report.json` (ruler `functionRelocDiffs=name_check`):
`?Handle@QuestFilterPanel@@UAA…`, `?Handle@UIPanel@@UAA…` and `?Handle@UIScreen@@UAA…` are each at
`fuzzy_match_percent` 100 and `match_percent_normalized` 100. Caveat: `name_check` forgives
placeholder callees, so this settles the dispatch shape and order, not every callee name.

There is no timer, camera or `is_up` gate in the path. `tour_gigguide_map_panel`'s `enter` only
triggers `gig0<N>.trg` (the map animation); it holds no focus and no input handler.

## 2. Every tourwalk3 gig-2 attempt fits it (re-read from `~/tmp/tourwalk3/r_*/run.log`, legacy frames)

In each, the guide was left only on the **second** `a`, and the first `a` never changed `sel=`,
because opening the filters is not a selection change:

| leg | guide up | presses on the guide | outcome |
|---|---|---|---|
| custom4 | 17184 | `down` +199, `down` +259 (filters hidden, eaten), `a` +349 (show_filters), `a` +2350 | row 0 |
| custom2 | 16784 | `a` +268 (show_filters), `a` +568 | row 0 |
| custom3 | 18635 | (its `down`s had gone to newaward) `a` +1969 (show_filters), `a` +2169 | row 0 |

custom2's `a` at legacy +268 lands inside the claimed dead window and was taken: without it, the `a`
at +568 would only have opened the filters. Gig 1 "worked" in every leg because its script section
opened with an `a` (legacy +160) before the `down`s. tourwalk2's gig-2 "`a` at +570 taken" is the
same thing: it was the second confirm.

So the delay was a misreading. It looked like a delay because the first confirm leaves no `sel=` line.

## 3. w_custom5: the custom setlist reached (leg `c5w1`, bin-w1)

`w_custom5.txt` = tourwalk3's `w_custom4.txt` (already 60 Hz on main) with gig 2's guide section
changed to `+400 a`, `+520 down`, `+580 down`, `+700 a`.

| frame | event |
|---|---|
| 43298 | `tour_gigguide_screen`, `sel=0:tour01_gig02` |
| 43697 (+399) | `a`: show_filters, no `sel=` line |
| 43819 (+521) | `down` taken: `sel=1:filter_the80s` |
| 43879 (+581) | `down` taken: `sel=2:filter_the00s` (gig 2's custom row) |
| 43999 (+701) | `a`: `song_select_enter_screen`, tour line `filter=filter_the00s custom=1 random=0 set=` |
| 44009 | `song_select_screen`: `songsel sort=by_song n=43 making=1 filter=1:the2000s` |
| 44452, 44852 | `a`, `a`: `setlist=thehardestbutton`, then `thehardestbutton,rehab` |
| 45252 | third `a` plays the setlist: `tour_part_difficulty_screen`, `set=thehardestbutton,rehab` |
| 46905 | `game_screen` (tv3_c) |

The `down`s at +521/+581 are legacy +260/+290, inside the window tourwalk3 read as dead. Past
this, the script still expected the fixed-set flow (its `wait_screen tour_part_difficulty_screen`
lines timed out once), so the songs and gig 3 in this leg are not a walk result. Gig 1 on the same
run behaved the same: `a` +319, `down` +499/+559 taken, `a` +679.

## 4. Log: `filters=` on the focus line

`native/src/main_app.cpp` `LogFocus` appends ` filters=0|1` when the focused panel has an
`is_showing_filters` property (read through `Hmx::Object::Property(…, false)`, read-only), and logs a
new line when it flips, so a walk sees the guide's state. Log only.

Leg `c5b4` (bin4, w_custom5) repeats `c5w1` frame for frame on the guide, now with the state visible.
Gig 1: `filters=0` at 2165, `filters=1` at 2486 (the `a` polled at 2484), then `sel=1` 2666 and
`sel=2` 2726. Gig 2: `sel=0:tour01_gig02 filters=0` at 43298, `filters=1` at 43699 (the `a` at
43697), `sel=1:filter_the80s` 43819, `sel=2:filter_the00s` 43879, `song_select_enter_screen` with
`custom=1` at 43999. The flag flips two frames after the first confirm, not hundreds.

The control leg `c4b4` (bin4, tourwalk3's unchanged w_custom4, `down` `down` `a` on gig 2) was capped at
legacy 21000 = 42000 frames, which ends before gig 2's guide (43298 at 60 Hz; tourwalk3's 17184 was a
legacy-30 Hz frame, and 60 Hz is not an exact doubling here). So it shows only gig 1 (same lines as
c5b4) and not the eaten `down`s under `filters=0`. That control was not rerun. The evidence for the
eaten `down`s is tourwalk3's custom4 log (section 2): no `sel=` change, then two confirms needed to leave.

## Binaries

| binary | sha256[:12] | tree |
|---|---|---|
| bin-w1 | c4e9ff7195be | walk60's (`~/tmp/walk60/bin-w1`, main `82ff40429` + walk60's chartplay driver change), copied unchanged |
| bin4 | e9f2931cad4c | `ac8fbeb28` (this branch: main + the focus-log line) |

## Verdict

Native = retail. The behaviour is retail TU5's DTA (byte-identical to the shipped ark), dispatched by
`Handle` bodies that match retail. Nothing to fix in the game. The fix is to the walks: a guide section
must open with a confirm, wait for it, and then send the row moves and a second confirm.

## Not done

- Gig 3's custom rows (1-2) and a full custom gig: w_custom5's post-song-select tail still assumes the
  fixed-set flow. Next step: replace its gig-2 song-select block with the three `a`s above and append
  gig 3 (`+400 a`, `+520 down`, `+700 a` for row 1).
- Native selftest (only a log line changed; coordinator's when landing).
- Landing (per the brief).
