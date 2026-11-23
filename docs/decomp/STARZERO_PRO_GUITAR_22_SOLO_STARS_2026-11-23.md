# STARZERO: p0's solo stars read 0 on a 22-fret pro guitar song (2026-11-23)

Lane starzero, from chartplay2's open lead 4
(`~/tmp/wt-chartplay2/docs/decomp/CHARTPLAY2_KEYS_PROKEYS_PROGUITAR_2026-11-23.md` §4):
in some deterministic walks, p0's `Player::GetNumStarsFloat` stays 0.0000 for the whole
second song while the score climbs and the band's stars look normal, under autoplay too
(run r1GA).

**Verdict: not a native bug. Retail does the same.** With a 22-fret pro guitar
(`kJoypadXboxRealGuitar22Fret`, which is what `RB3_APP_JOYPAD_KIND=proguitar` emulates), on a
song whose MIDI has a `PART REAL_GUITAR_22` track, retail TU5's
`Player::GetNumStarsFloat` returns 0 all song. No `src/` change, so nothing to price with
`ab_measure`.

## Which song

r1GA (`~/tmp/chartplay2/r_r1GA/run.log`): song 1 is `stopmeifyouthink`, song 2 is
`beastandtheharlot` (`PreloadPanel::Load ... 'beastandtheharlot'`, then `MidiTrackLister`
lists `PART REAL_GUITAR` **and** `PART REAL_GUITAR_22`). Song 1's MIDI has no 22-fret part.
Both facts also hold in the official Xbox data
(`~/code/milohax/rb3/orig-assets/extracted-xbox-full/songs/<song>/<song>.mid`, a byte search
for the track names: beastandtheharlot has both parts, stopmeifyouthink only `PART REAL_GUITAR`).
At frame 12000 r1GA has band stars 3.0071 and p0 stars 0.0000 at a p0 score of 13,409.

## Cause

Two different track types meet in `Scoring::GetPlayerScoreInfo`:

1. `GameConfig::AssignTrack` turns the user's `kTrackRealGuitar` (6) into
   `kTrackRealGuitar22Fret` (7) for a user with `mHas22FretGuitar`, and
   `PlayerTrackConfigList::TrackNumOfType(7)` takes the song's `REAL_GUITAR_22` track when it
   exists (it falls back to the 17-fret track only when it does not). So on beastandtheharlot
   the player's song track has type 7, `MultiplayerAnalyzer::AddTrack` stores 7 in the
   player's `Data`, and `PostLoad` pushes the one base-score entry with type 7.
2. `Player::mTrackType` is the BandUser's own track type, still 6. `Player::GetNumStarsFloat`
   calls `GetSoloNumStarsFloat(GetIndividualScore(), mTrackType)` → `GetPlayerScoreInfo(6)`.
   That loop finds no entry of type 6, and the fallback only maps 22-fret to 17-fret (7→6,
   9→8), never 6→7, so it returns null and the stars are 0.

On song 1 the track is type 6, so the entry is 6 and the lookup succeeds; that is why only
the second song shows it.

## Measured natively (gdb on chartplay2's bin-p2, no source change)

`gdb -batch` with breakpoints on `MultiplayerAnalyzer::AddTrack` and `PostLoad`, and on the
600th `Scoring::GetSoloNumStarsFloat` call after each `PostLoad`, where it calls
`GetPlayerScoreInfo` for 6, 7 and the passed type (script `~/tmp/starzero/g1.gdb`, run
`~/tmp/starzero/r_gA/run.log`, the r1GA walk: det, proguitar, autoplay, 10,600 frames, rc 0).
Prediction: song 2 adds a track of type 7, and `info(6)` is null while `info(7)` is not.
Result, as predicted:

| song | tracks added (index=type) | solo call | info(6) | info(7) |
|---|---|---|---|---|
| stopmeifyouthink | 4=6, 5=4, 6=5 … (no 7) | score 1620, ty 6 | non-null | non-null (same entry, 7→6 fallback) |
| beastandtheharlot | 4=6, **5=7**, 6..9=3 | score 1183, ty 6 | **0** | non-null |

## Checked against retail (TU5 band.exe, `tools/va_disasm.py`)

- `Player::GetNumStarsFloat` 0x826A2720: loads `this+0x27c` and calls
  `GetSoloNumStarsFloat` 0x826A1128 with `GetIndividualScore()`.
- `Player::Player` 0x826A75C8: `stw r3, 0x27c(r30)` right after `bl 0x8252E068`, a folded
  `lwz r3, 0x10(r3)` on the BandUser (`BandUser::mTrackType`), then the `== 3` (vocals) test
  for `SetVocals`. So retail's `Player::mTrackType` is the user's type, 6, as natively.
- `Scoring::GetSoloNumStarsFloat` 0x826A1128: null info returns the 0.0 constant; else
  `GetNumStarsFloat`, capped at 5 unless expert. Same as ours.
- `Scoring::GetPlayerScoreInfo` 0x826A0FF8: the loop, then `cmpwi r30, 7 → li r30, 6` and
  `cmpwi r30, 9 → li r30, 8`, retry; else null. One-way fallback, same as ours. (The rb3-Wii
  dev oracle has no fallback at all; TU5 added only this direction.)
- `MultiplayerAnalyzer::AddTrack` 0x826CDB18 stores the song track's type unchanged
  (`stw r29, 0x10(r31)`); `PostLoad` 0x826CED40 copies `Data+0x10` unchanged into the
  `PlayerScoreInfo`.
- GemPlayer's vtables (.rdata 0x820EAFE0 and 0x820F2AB0) hold `GemPlayer::GetNumStars`
  0x826BC290 next to **Player's** `GetNumStarsFloat` 0x826A2720: retail GemPlayer does not
  override the float getter. Its integer `GetNumStars` goes through `GetStarsForScore`
  0x826F3AC8, which reads the track type from the game config's player track list
  (`lwz r5, 0x10(config)`, i.e. 7) and so does find the entry. Retail's integer star count
  and star rating for this player are therefore normal; only the float getter reads 0.

Every step of the chain is the same in retail, so retail reads 0 here too. Leaving the native
code as is keeps rb3-app retail-faithful; "fixing" it (a 6→7 fallback, or passing the config
type) would diverge from retail.

## Consequences for walks

- A walk that checks `p0 stars` from `Player::GetNumStarsFloat` must expect 0.0000 for a
  22-fret pro guitarist (and, by the same code, a 22-fret pro bassist: types 8/9) on songs with
  a 22-fret part. It is not a scoring failure. Integer `GetNumStars` / star rating are the
  values to check for that player.
- The band performer's stars (what the in-game scoreboard shows, `TrackPanel` →
  `GetMainPerformer()->GetNumStarsFloat()`) are computed from `mStarThresholds`, which sum
  every base-score entry, so they are unaffected.

## Not done

- No source change, so no `ab_measure` run and no native rebuild; the native selftest is the
  coordinator's.
- Did not measure the integer star path natively; it rests on the retail reading above.
- Did not run a 17-fret pro guitar (`kJoypadXboxButtonGuitar`) walk on beastandtheharlot,
  which by this analysis would score stars normally (config type 6 picks the 17-fret track).
