# SONGSELWALK: rb3-app song select and setlists walked (2026-11-23)

Lane songselwalk, branch `songselwalk` (from main `068db0471`), worktree `~/tmp/wt-songselwalk`.
Scratch, logs and binaries: `~/tmp/songselwalk/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`).
Walks are in `tools/rb3_app_walks/songselwalk/`: `mk.py` writes every `w_*.txt` (it expands a
held button into one press per frame, which the script player reads as held; song select needs
A held 1.5 s = 45 frames to start or finish a setlist). `disc.sh <tag> <bin> <walk> [frames]
[env…]` always sets `RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1`; `runb.sh` takes
`CONTENT=<store dir>` in place of `memcard/content_a`. Every run went through
`coord/no610m.sh` under the shared `gpu1.lock`. Movies off. The full native selftest was not run
(coordinator's job).

## Commits

| commit | what |
|---|---|
| `75f1c0f2f` | **fix**: native `VirtualKeyboard::ShowKeyboardUI` never answered, so a setlist (or band, or character) could not be named or renamed (finding 1) |
| `fa7ed27b8` (harness) | `RB3_APP_SONGSEL_LOG=1` in `native/src/main_app.cpp`: a read-only song-select trail (below) |
| (this commit) | walk scripts, this record |

`75f1c0f2f` priced with `tools/ab_measure.py --revert 75f1c0f2f`: **Δmatched +0, Δhonest +0,
Δcode +0 B** (`name_check`; legs A = B = 55,268 matched, 59.665310%). Predicted Δ0: the change
is inside `#ifdef HX_NATIVE`. Run dir `.ab_measure_runs/20261123-181423-songselwalk-vk-4051143/`.

## The instrument: `RB3_APP_SONGSEL_LOG=1`

The focus log cannot see song select: `song.lst`, `settings.lst` and `options.lst` rows have
empty `DataSymbol`s (menuwalk said so too). The new line, printed on change while the music
library is entered:
`songsel sort=<NodeSort short name> n=<rows> hl=<ix>:<type>:<token> making=<0|1>
setlist=<shortnames> filter=<MusicLibraryTask::GetSongFilterAsString> saved=<count>:"<title>"(<songs>)…`
(type letters: C shortcut, H header, U subheader, S song, F function, L setlist, T store song),
plus a `songsel list <sort> <n>: T:token …` dump of the whole row list whenever the sort, its row
count or the filter changes. Read-only (accessors and public members only).

Buttons on a gamepad, read off the walks: **Y** = `kAction_Option` (song details),
**Back/Select** = `kAction_ViewModify` (the sort and filter panel), **RB/LB** = page down/up
(skip to the next/previous shortcut; held 0.25 s opens the shortcut panel, released jumps),
**A held** = make/finish a setlist, **B** in a setlist = remove the last song, **X** does nothing
here.

## Screens reached and verdicts

Quickplay song select, the deterministic seed's store (`content_a`, 83 disc songs, 109 rows).

| area | walk (tag) | what happened | verdict |
|---|---|---|---|
| Sorts | `w_sorts` (sorts), `w_misc` (misc) | the settings row 1 options: by_song (109 rows, 24 alpha headers + 3 functions), by_artist (169: one header per artist, "The" ignored: The B-52's between Avenged Sevenfold and The Beach Boys), by_difficulty (93, `generic_tier1..`), by_stars (87, all `stars_0`), by_recent (87, all under `rb3`: nothing played), by_plays (87, all `0`), by_review (87, all `unreviewed`). by_rank (option 4) is skipped by the cursor: `SortViewSetting::IsActive(4)` needs a Live sign-in and RockCentral online | as retail's code (all units fuzzy-100 except block placement, below) |
| Title order | sorts | `AlphaKeyStrCmp`: "The Con" under C, "I Wanna Be Sedated" before "Imagine" (space < letter), digits under `123` | as retail |
| Filters | `w_filt0..8` (f0-f8) | every filter row 6-14 (genre, decade, keys, pro guitar, vocal parts, source, difficulty, length, rating) opened, first and second option toggled: genre alternative 27 rows, +classic rock 47; decade 1960s 13 rows = Break On Through, Crosstown Traffic, Good Vibrations (live), I Can See for Miles, Space Oddity; vocal parts 1: 33, +2: 60; difficulty tier1: 22, +tier2: 38; length short: 24, +medium: 94. Keys/pro guitar/source/rating have one disc-wide option and toggle it on and off | as retail (`DoesSongMatchFilter` differs from retail only in a redundant `!= 0` normalisation, same truth value) |
| Filter reset | f0-f8 | Y on the filter panel opens `filter_reset_confirm_screen` with focus on `no.btn` (dialog default); A = no, filters kept | as song_select_extras.dta; the walk's reset leg pressed "no" (walk limit, not a bug) |
| Shortcut skip / panel | p_rb, misc | RB moves the highlight to the next header; RB held 20 frames opens `song_select_shortcut_panel`, the release jumps (`BUTTON_UP_MSG` → `on_select`) to the shortcut (`123`) | as dta |
| Song details | p_y, misc | Y on a song: `details_1..4.btn` + `review_0.rvw`; on a setlist: details_1 = play, details_2 = edit_setlist | as dta (`details_buttons`) |
| Song preview | prv, prv2 (gdb `gdb_prev2.cmd`) | `SongPreview::Init` once; each highlight change calls `Start("")`, then after `song_preview_delay` `Start(<song>)` → `PreparePreview` → `PrepareSong(<song>)` for songs only (not headers/functions) | pipeline as retail; audio not heard (no device) |
| Make a setlist | `w_setlist` (sl1, sl2) | A held on 20thcenturyboy: making=1, setlist=[20thcenturyboy], `play_setlist` row appears (110 rows); A adds 25or6to4, antibodies; A held again → `setlist_save_screen` (opt1 play / opt2 save / opt3 share / opt4 battle; opt3-4 disabled offline); B in song select removes the last song | as dta + `MusicLibrary::SelectNode` |
| Header "play all" | p_rb | A on header B: all 6 B songs become the setlist, making=1, straight to part_difficulty (`PlaySetlist(false)`); backing out keeps making=1 | as retail `SelectNode` kNodeHeader |
| Save a setlist | sl1, nm1 | opt2 → `edit_setlist_screen` → ok.btn → message → saved=1:"My Custom Setlist"(3) (sl1, no name typed) / "Walk One"(3) (nm1, after the fix) → song select switches to the setlist view on the new setlist | as retail after finding 1 |
| Rename | `w_rename` (rn2) | setlist view → Y → details_2 (edit) → edit_name.btn → "Renamed List" → ok: saved=1:"Renamed List"(3), same GUID token | works after finding 1 |
| Play a saved setlist | rn1, rl1 | details_1 on the custom setlist → part_difficulty → game_screen; internal setlist (drumsongs) from the browser → part_difficulty → preloading | works |
| Random setlist | `w_random` (rnd1) | A held on `random_song`: a 1-song setlist, four A presses add four more random songs, no repeats (icanseeformiles, ifeelgoodalt, hereigoagain, freebird, whipit); A held → save dialog → opt1 plays it (tv3 → game_screen) | as retail (`GetRandomSongs` excludes the current setlist) |
| Reload after reboot | `w_reload` (rl1 on sl1's store, rl2 on rn2's) | boot straight to main_hub; song select shows saved=1 before the setlist view opens; `view_setlists` lists `setlist_header_custom` with the setlist (rl2: "Renamed List", token c4cb4163…, same as before the reboot) and the 7 internal setlists | works |

## Findings

### 1. The native virtual keyboard never answered (fixed, `75f1c0f2f`)

Native `VirtualKeyboard::ShowKeyboardUI` (the `HX_NATIVE` block of `src/system/os/
VirtualKeyboard.cpp`) was `return DataNode(0);`. Retail's (VirtualKeyboard_Xbox.cpp, 0x82532EB8)
opens the guide keyboard and `PlatformPoll` (0x82532DC8) later sets `mCallbackReady` with the
filtered text; if `XShowKeyboardUI` fails it answers at once with ok FALSE. Natively neither
happened: `edit_name.btn` (and `edit_desc.btn`, the character creator name and the overshell's
`show_keyboard` in slot_states, the only other callers in the TU5 dtbs) never received
`virtual_keyboard_result_msg`, so a setlist kept the default "My Custom Setlist" and could not be
renamed (sl1: A on edit_name at frame ~1543, nothing).

The fix answers every prompt through retail's two paths: an entry from `RB3_VK_TEXT`
(`|`-separated, one per prompt, in order) goes through `UTF8FilterKeyboardString` and
`RemoveSpaces` with ok TRUE, truncated to the prompt's length; with no entry left it is retail's
failure arm (empty, ok FALSE), which the callers treat as a cancel. The result goes out on the
next `Poll`, as retail's. Checks: nm1 named "Walk One", rn2 renamed it "Renamed List", rl2 read it
back after a reboot. sl2 (fix binary, no `RB3_VK_TEXT`) against sl1 (base binary): det-identical
over all 3,600 frames, same saved "My Custom Setlist"(3), plus the `cancelled` log line. A real
interactive host keyboard (the engine's `TextInput_Native` has no Enter event, and nothing draws a
text box) is not done.

### 2. ASan + UBSan: nothing new

`bin-asan-s2` (`-DRB3X_SANITIZE=address,undefined`, ASAN_OPTIONS as `tools/native_asan.sh`)
on `w_rename` (create, name, save, rename, save: asn1), `w_filt0` (asn2) and `w_random` (play a
random setlist into the game: asn3): **0 ASan reports** in all three. UBSan: the known sites
(mikktspace, Keygen_Stub, the bandobj `Delta` group, `BinStream.cpp:33`, `ChunkStream.cpp:501`,
`StandardStream.cpp:689`, `GameGem.h:84/85`, `SongParser.cpp:234`, `BeatMap.cpp:90`) and one not
in the earlier lists: `DateTime.cpp:70` signed overflow `-100 * 33177600` from
`LocalSavedSetlist::SaveFixed` → `DateTime::ToCode`. A new local setlist's `mDateTime` is the
zeroed default (year 1900); nothing on the creation path calls `GetDateAndTime`
(`SavedSetlist`, fuzzy-100, sets it only for internal setlists in `SongSortMgr`). Retail computes
the same wrapped code with the same 32-bit arithmetic, so the saved value is retail's; not
changed.

### 3. Checked and left alone

- `MusicLibrary::Text` (94.2% fuzzy) and `SongSortMgr::DoesSongMatchFilter` (98.6%): objdiff
  shows block placement (the `SetSongAndArtistName`/`SetSongName` arms moved) and a redundant
  bool normalisation; neither changes behaviour.
- The `SongSortMgr::BuildSetlistList` `HX_NATIVE` empty-setlists arm never fires on this data: the
  7 internal setlists build (drumsongs, guitarsongs, keyssongs, partyfavourites, proguitarsongs,
  endless3, secretshames), so its comment's premise ("internal_setlists shortnames don't match")
  does not hold for this extract. Harmless (it only skips a debug assert); not changed.
- Tie order in sorts follows `std::map<Symbol,…>` iteration (pointer order) through
  `lower_bound` inserts; only equal keys could differ from retail, and no two disc songs tie.

## Not done

- by_rank, net/shared setlists, battles, share/unshare, the store rows and setlist art (all need
  Live or RockCentral, out of scope offline); `delete_setlist_confirm_screen`; party shuffle's
  play; the 20-song max-setlist path (`full_setlist_screen`).
- Rendering of the song info and difficulty labels was not compared to retail pixels (no rb3-app
  screenshot knob); the walk checks the data and handlers behind them.
- Sort order against retail by run (no Xenia leg); the order was checked against retail's
  comparison code and the titles.
- Binaries: bin-s0 `2ad72f96ba6d`, bin-s1 `fd571b59693d` (main `068db0471` + songsel log),
  bin-s2 `0530ab69970e` (+ fix), bin-asan-s2 `7f5b28ea6590`. Nothing pushed.
