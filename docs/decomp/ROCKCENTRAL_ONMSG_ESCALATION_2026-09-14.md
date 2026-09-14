# Lane W16-B (Fable escalation): `Handle@RockCentral` was NOT uncollectable -- 28 charged sites -> 0 by porting three `OnMsg` bodies

**Date:** 2026-09-14 · **Branch:** `w16-b` · **Worktree:** `~/tmp/wt-w16-b` · **Escalates:** `ROCKCENTRAL_HANDLE_AND_MOVIE_MAP_2026-09-14.md` (lane W15-E, Opus)
**Ruler:** `name_check` (read from `report.json` `provenance`); every number below is from a full `./tools/ninja-locked` build's `build/45410914/report.json`.

## Verdict in one paragraph

W15-E reported `Handle@RockCentral` (1,296 B) as "28/30 scheduling charges, no
charge names a source construct, uncollectable". The 28 charges were the
*caller-side* symptom of three stubbed `OnMsg` overloads: MSVC schedules a
`HANDLE_MESSAGE` block from what it can see of the callee, and a stub is a leaf
with no side effects. Porting the retail bodies of `fn_824F7C98` (UserLogin =
FriendsListChanged, ICF-folded) and `fn_824F7D48` (ProfileChanged) -- **source
only, no map or alias change** -- took the row from **fuzzy 94.111115 / 28
charged sites** to **100.0 / 0 charged sites** (+1 fn / +1,336 B; the row is
1,296 B plus a 40 B EH funclet that crossed with it). The Opus verdict was the
`REGISTER_SWAP`-class symptom-as-diagnosis error CLAUDE.md warns about, and the
lane's own escalation hint (6/6 correlation with stubbed overloads) was the
right lead.

## Whole-binary ledger (full builds, `name_check`)

| build | after | matched_functions | matched_code | matched_code_% | fuzzy | total_code |
|---|---|---:|---:|---:|---:|---:|
| 0 | baseline (main `5aa1cb7a`) | 42,883 | 3,904,048 | 38.103306 | 49.2141 | 10,245,956 |
| 1 | A `2f0782f5` OnMsg bodies | 42,884 | 3,905,384 | 38.116344 | 49.21506 | 10,245,956 |
| 2 | B `4dd48148` map + alias | 42,885 | 3,905,512 | 38.11759 | 49.2184 | 10,245,956 |
| 3 | D `bad8199d` + E `6ef460cf` + C `28ef9a0a` (+ `.pdata` re-derive `d2515623`) | 42,897 | 3,908,448 | 38.14625 | 49.22389 | 10,245,956 |
| 4 | E2 `e0291633` short-circuit `&&` | **42,898** | **3,908,748** | **38.149178** | **49.224045** | 10,245,956 |

**Net for the lane: +15 fns / +4,700 B / +0.045872 pp / fuzzy +0.009945**, `total_code` unchanged, zero rows fell off 100.

## Per-commit predicted vs measured

| commit | predicted | measured | miss? |
|---|---|---|---|
| A `2f0782f5` port 3 OnMsg bodies | +1 fn / +1,296 B (Handle crosses) | **+1 / +1,336 B** | +40 B: EH funclet `fn_824F7D18` crossed too; not predicted |
| B `4dd48148` name 0x824f7c98/0x824f7d48 + T1 alias for the fold | +2 / +344 B | **+1 / +128 B** | MISS: ProfileChanged row 98.888885, 1 charge (`cmplwi` vs our `cmpwi`); commit A's "matches exactly" was oracle-text, not retail-byte |
| D `bad8199d` TimeConversion | +11 / +2,720 B | **+11 / +2,720 B** | exact: 5 crossings (TimeConversionInit 156, AddInstance@Gem 1,704, GetStart@Gem 72, AddBeatMask@GemTrainerPanel 292, AddBeatMask@VocalTrainerPanel 124) + 6 new named rows at 100 (MsToBeat 64, BeatToMs 84, BeatToTick 12, SecondsToBeat 76, TickToSeconds 64, OnSecondsToBeat 72); OnBeatToMs stayed 100 at its new address; `??__E/FTheLocale` rows became an unpaired `fn_827C91A0` (0%, no matched loss); DrawBeatLine/DrawTrackElements/RemoveAllInstances fuzzy-only as predicted |
| E `6ef460cf` AttemptRemoveUser | +1 / +300 B | **+0 / +0 B** (row 14.906667 -> 94.293335) | MISS: the oracle-shaped `bool b1` cost a callee-saved reg (`__savegprlr_28`, frame 0x80 vs retail `__savegprlr_29`, 0x70) and 4 instructions; retail is a short-circuit `&&` -- fixed in E2 |
| E2 `e0291633` short-circuit `&&` | +1 / +300 B, nothing else moves | **+1 / +300 B**, 0 other rows moved | none |
| C `28ef9a0a` ProfileChanged unsigned context | +1 / +216 B if the cast is the construct | **+1 / +216 B** (98.888885 -> 100) | none -- the `(unsigned int)` cast at the site IS the construct |

## Step 1-2: the OnMsg bodies and `Handle@RockCentral`

- Retail `fn_824F7C98` (UserLogin **and** FriendsListChanged, one folded body):
  `mJobMgr.QueueJob(new UpdateFriendsListJob(msg.GetPadNum()))`. Our source
  had paired FriendsListChanged with ProfileChanged; regrouped, and
  `FriendsListChangedMsg::GetPadNum()` added to `os/Friend.h` (`mData->Int(2)`).
- Retail `fn_824F7D48` (ProfileChanged): `HasValidSaveData()` first, then the
  server's `GetPlayerID(padnum)` vcall, then queue the job.
- The in-source note at `RockCentral.cpp:256` ("not in the pinned retail-Xbox
  range") was **false** and is corrected: both bodies are in `asm/RockCentral.s`.
- `Handle@RockCentral` charged sites: **28 -> 0** after commit A alone. No map
  or alias edit was needed for the crossing; commit B's naming is the separate
  pairing channel (+1 fn for the UserLogin row).
- Commit B residue: retail tests the `GetPlayerID` result with `cmplwi`
  (unsigned) at this site. A census of every retail site that calls
  `TheNet(0x82CBFAB8)+0x34 -> vtable+0x1c` finds **4 sites with `cmpwi`**
  (SongStatusMgr `fn_825D3C30`; MetaPerformer `fn_8257BDA0/BEF8/C050`, one of
  which is already 100% against our `int GetPlayerID(int)` with `!= 0`) and
  **only this one with `cmplwi`**. So the return type stays `int` (rb3-Wii
  `Server.h:25` agrees) and commit C casts at the site. ⚠ An earlier in-lane
  "refutation" of the unsigned hypothesis cited two 100% callers that turned out
  to use a *different* slot (`0x5c`) -- withdrawn; the census above replaces it.

## Step 3: TimeConversion -- two undefined functions, four wrong map names, one mis-homed pin

All bodies read from `build/45410914/asm/StringTable.s` keyed on `.fn fn_<addr>`
(the address column is synthetic for multi-block units). Raw `band.exe` reads
via the PE section table and via linear file offset **both failed validation**
on `MsToTick`'s known first word -- do not use them.

| addr | size | body | map before | map after |
|---|---:|---|---|---|
| 0x827C90D0 | 64 | TimeToTick vcall +0x8, `bl BeatMap::Beat` -- unguarded | (anon) | `?MsToBeat@@YAMM@Z` |
| 0x827C9128 | 84 | `bl BeatMap::BeatToTick`, TickToTime vcall +0x4 -- unguarded | (anon) | `?BeatToMs@@YAMM@Z` |
| 0x827C9180 | 12 | lis/lwz TheBeatMap; `b BeatMap::BeatToTick` | (anon, **pinned into MetaPerformer.cpp**) | `?BeatToTick@@YAMM@Z`, re-homed into StringTable's block |
| 0x827C91A0 | 36 | `TimeToTick(f1 * 1000)` tail call | `??__ETheLocale@@YAXXZ` **(wrong)** | anonymous, documented in the .cpp (no oracle names it; single caller GamePanel `fn_82695178` unidentified) |
| 0x827C91C8 | 76 | fmuls 1000, TimeToTick, `bl BeatMap::Beat` (MsToBeat inlined) | (anon) | `?SecondsToBeat@@YAMM@Z` |
| 0x827C9218 | 64 | TickToTime vcall +0x4, fmuls 0.001f | `??__FTheLocale@@YAXXZ` **(wrong)** | `?TickToSeconds@@YAMM@Z` |
| 0x827C9288 | 72 | `DataArray::Float`, `bl 0x827C91C8` (SecondsToBeat), store | `?OnBeatToMs…` **(wrong, financed a false 100 through a forgiven placeholder callee)** | `?OnSecondsToBeat…` |
| 0x827C9328 | 72 | `DataArray::Float`, `bl 0x827C9128` (BeatToMs), store | (anon) | `?OnBeatToMs…` |

- `TimeConversionInit`'s 2 charged sites (99.744) were the `@ha/@l` pair of the
  first registration: retail said `OnBeatToMs` (the wrong name at 0x827C9288)
  where we pass `OnSecondsToBeat`.
- The `if (TheBeatMap && TheTempoMap)` guards on MsToBeat/BeatToMs are DC3's
  (newer engine); neither retail bytes nor the rb3-Wii oracle has them. Removed.
- `OnSecondsToBeat` calls `SecondsToBeat`, not `MsToBeat(x*1000)`, per retail.
- Recursive census (`build/45410914/asm/**/*.s`, keyed on `bl fn_<addr>` inside
  `.fn` blocks): `fn_827C9218` (TickToSeconds) 25 sites / 11 functions / 5 files
  -- `DrawTrackElements@GemTrack` 1,432 B (6 name charges among 28),
  `DrawBeatLine@GemTrack` 684 B (1 among 13), `AddInstance@Gem` 1,704 B (2/2),
  `RemoveAllInstances@Gem` 416 B (1 + a `_Rb_tree::clear` fold residue),
  `GetStart@Gem` 72 B (1/1), `AddBeatMask@GemTrainerPanel` 292 B (2/2),
  `AddBeatMask@VocalTrainerPanel` 124 B (2/2), plus 10 sites in four unnamed
  rows. `fn_827C9180` (BeatToTick) 7 sites, all in fuzzy<100 or unnamed rows
  (VocalTrack, SongDB x3, GemTrack x2, band3/game/TrainerPanel) -- fuzzy-only.
  The newly named MsToBeat/BeatToMs/SecondsToBeat have 10 currently-100 callers;
  our source calls the same name at every one (checked), so naming converts
  forgiven sites to checked-and-equal.
- No native shim exists for either function (`native/src/m3_symbols.cpp`
  checked); nothing to drop.
- Measured (build 3): exactly the prediction, +11 fns / +2,720 B; see the per-commit table.

## Step 4: `AttemptRemoveUser@OvershellSlot`

Retail 0x825DF898 (300 B / 75 instr) has exactly four decision points after the
assert; the rb3-Wii DEV `TheWiiProfileMgr` pad loop and the
`IsPrimaryProfileCritical` branch are absent from retail bytes -- the same
contamination class as `RemoveUser`. Ported to: not-local-session && host ==
user -> `kState_RemoveUserDisconnectConfirm`; critical user == user ->
`kState_RemoveCriticalUserConfirm`; `InSong()` -> `kState_RemoveUserInSongConfirm`;
else `RemoveUser()`. Enum values verified in `OvershellSlotState.h`
(0x1a / 0x41 / 0x4d). Measured: build 3 read 94.293335 (11 charged sites, all
from a stored `bool b1` the oracle shape introduced -- retail branches from
both tests straight to the else-if chain and saves one register fewer); build 4
tested the short-circuit form: **100.0** (+300 B, predicted exactly).

⚠ Instrument note: the file saved as "build 3 snapshot" was copied *after* the
background build 4 had already rewritten `report.json` (build 4 is a one-TU
recompile and finished in ~2 min), so a build4-vs-"build3" row diff read 0
changes -- vacuous, same file on both sides. The build-3 figures above were
read from the live file *before* build 4 was launched, and build 4 was
re-scored against the clean build-2 snapshot (24 changed rows = build 3's 23 +
`AttemptRemoveUser`). Snapshot before launching the next build, not after.

## Contradictions of the brief / the Opus record / in-tree text

1. Opus: "uncollectable, 28 scheduling charges" -- **refuted**, source-only fix.
2. `RockCentral.cpp:256` note "not in the pinned retail-Xbox range" -- **false**, corrected.
3. Map names at `0x827c9218`, `0x827c91a0`, `0x827c9288` were wrong; `0x827c9328` was anonymous.
4. `splits.txt` had the 12 B `0x827C9180` pinned into `MetaPerformer.cpp`.
5. MsToBeat/BeatToMs guards were DC3 contamination, not retail.
6. This lane's own commit-A claim that ProfileChanged "matches exactly" was
   oracle-text, not retail-byte (`cmplwi`), and its first "GetPlayerID is
   signed elsewhere" refutation cited callers of a different vtable slot.
7. Out of scope but noted for a later lane: `DrawBeatLine@GemTrack` calls
   `GetLoopTick` where retail calls `?OnIsSpeechSupportable@SpeechMgr@@QBA_NXZ`
   (wrong callee or fold -- unadjudicated); `RemoveAllInstances@Gem` carries a
   `_Rb_tree<…CharLipSync…>::clear` vs `<…TrackWidget…>::clear` fold residue
   with no alias group.

## Not done

- `Handle@OvershellSlot` untouched (brief: out of scope).
- The anonymous 36 B `0x827C91A0` was not given a name; retail's real
  `??__ETheLocale`/`??__FTheLocale` addresses were not chased.
- `0x82695178` (GamePanel caller of 0x827C91A0) left unidentified.

## Native gate

Run at `e0291633` (docs-only commits after it), log `~/tmp/gate_native_w16b.log`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
