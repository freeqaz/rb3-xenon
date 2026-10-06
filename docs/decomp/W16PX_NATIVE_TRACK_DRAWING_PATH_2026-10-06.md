# W16-PX: the track and vocal drawing path now runs on host (2026-10-06)

Lane W16-PX, branch `w16-px`, rebased onto main `9480cb75e` (clean retail TU5
image, `orig/45410914/default.xex` sha1 `d56e7f31…`). Brief: link the in-scope
band3/bandobj files on the track and vocal drawing path that native did not build
yet (CAMPAIGN_STATE_2026-10-06 §4–5: VocalTrack.cpp was the largest such file at
13,720 B of in-scope gap), so that their behaviour actually runs.

## 1. What is now linked

`tools/native_linked_tus.py` reports **483 → 520 src/ TUs** in the union of all
18 targets: 37 new and 0 lost. Before is the lane's start, after is this branch.

- band3/bandtrack: Track, VocalTrack, Lyric, Gem, GemSmasher, NowBar (scatter guest).
- bandobj: TrackPanelDirBase, TrackPanelDir, GemTrackDir, VocalTrackDir,
  GemTrackResourceManager, BandTrack, NoteTube, PitchArrow, StreakMeter,
  OverdriveMeter, CrowdMeterIcon, EndingBonus, BandLabel, BandScoreboard,
  UnisonIcon, ChordShapeGenerator, FingerShape, BandStarDisplay, BandButton.
- system/track: TrackDir, TrackWidget, TrackWidgetImp.
- beatmatch: RGState, RGUtl.
- rb3-render only: BandUser, GameConfig, GameMode, CharData, TourChar,
  TourCharLocal, TourSavable.

36 of the 37 new files map to objdiff units. On main's report they hold
**467,576 B of code, with 124 rows / 38,588 B below fuzzy 100**. The largest are
VocalTrack 13,720, VocalTrackDir 4,444, ChordShapeGenerator 3,536, TrackPanelDir
2,096, BandUser 1,968 and BandTrack 1,964 B. Those rows can now misbehave
observably on host.

Two source lists in `native/CMakeLists.txt` carry this:

- `BANDTRACK_DRAW_SOURCES` goes into rb3-milo and rb3-render.
- `VOCALTRACK_GAME_SOURCES` goes into rb3-render only. It holds the vocal gameplay
  layer that a live VocalTrack's vtable reaches: VocalPlayer, Singer, VocalPart,
  SongDB, the M6 scoring graph, BandUser, GameConfig and the rest.

TrackPanel.cpp, GemTrack.cpp and GemManager.cpp were already linked through
`L4_SCATTER_WIRE`, but nothing executed them. The new directories are registered
in `milo_object_factories.cpp`, so the shipped track milos load with zero
"Can't make" lines for any track class.

## 2. What now runs: the bandtrack phase (16 gates)

`native/src/bandtrack_phase.cpp` runs at the end of rb3-render's default mode,
which is what `tools/native_health.sh` already runs. `--no-bandtrack` skips it.
It adds about 0.6 s. Each gate compares the engine against a value the phase
derives some other way:

| gate | what is checked |
|---|---|
| bt-track-graphics, bt-tour-config | `config/track_graphics.dta` and `config/tour.dta` spliced as their band_keep sections |
| bt-trackpanel-load / -tracks | `ui/track/gen/trackpanel.milo_xbox` loads as TrackPanelDir with 4 GemTrackDir + 1 VocalTrackDir (the file header's count) |
| bt-vocaldir-load / -range / -middle-c / -monotone | `vocals.milo_xbox`; `SetRange(48,72)` against `PitchToZ`, two code paths agreeing |
| bt-vocaltrack-tracknum | real `PlayerTrackConfigList::Process` over a 5-track layout puts vocals on track 4; `VocalTrack::Init` reads it back through `GameConfig::GetTrackNum` |
| bt-vocaltrack-timing | Init's lyric/deploy timing equals the shipped DTA's values |
| bt-vocaltrack-marker-pool / -place / -visibility / -invalidate | 32-mesh marker pool; `x = width·t/window`; show only inside [from,to]; return to the pool once scrolled past |
| bt-vocaltrack-notetube | 9 style×part `ConfigNoteTube` selections equal the directory's materials |
| bt-vocaltrack-plates | `HookupTubePlates` takes the pool heads; pool stays at 4 |

**Prediction against measurement.** Two gates failed before they passed. Both
failures were harness errors that the real code caught:

- **tracknum read −1.** `AddConfig`'s 4th argument is the slot, not the track
  number. The track number is only assigned by `Process()` over the song's
  layout.
- **SetUserGuid asserted `!IsLocal()`.** A local user owns the GUID its
  constructor generated.

In both cases the phase was changed and the code was not, so the gates can fail.

## 3. Behaviour findings

1. **`Track::GetObj` fell off the end of a non-void function.** The else branch
   called `dir->FindObject(...)` without `return`. On X360 r3 happens to still
   hold the result. Natively it is undefined behaviour, and clang may trap or
   return garbage. Fixed by spelling the `return` (`b0dee28be`). The X360 row
   `?GetObj@Track@@…` (76 B) is fuzzy 100.0 in both A/B legs.
2. **The shipped tube style `tubes_default` has `harmony_2_front = <null>`, and
   `same_as_harmony_1 = 0`.** This was read off the loaded object's TypeProps map,
   and every other material slot resolves.
   - The engine follows the data, so the harmony-2 pitched tube gets no front
     layer.
   - The gate accepts a null only where the shipped style itself says null.
     Deploy materials, which are directory members, are never excused.
   - Data, not a code defect. Recorded so nobody chases it as a load bug.
3. **`sNullMicClientID` is defined in four retail TUs** (VocalTrack, VocalPlayer,
   MetaPerformer, ProfileMgr), all equal to MicClientID's default (-1,-1).
   Linking those TUs together natively made it a duplicate symbol. Each copy now
   has internal linkage under `HX_NATIVE`.
4. **Native-only defects fixed while linking** (`bb52a5b29`):
   - VocalTrackDir's legacy prototype conversion dereferenced a null native
     TypeProps pointer.
   - UILabel::OldResourcePreload and the ArpeggioShape/ArpeggioShapePool
     destructors were declared but had no body anywhere. These are native-only
     bodies, with no retail row.
   - Five duplicate scatter emitters (BandWardrobe, Tail, BandDirector, Anim,
     ClipCollide) and two wrong prunes (TrackConfig, OutfitConfig) got
     `!HX_NATIVE` guards.
   - BandLabel/BandButton register without `Init()`, because their
     `InitResources` walks a UIManager neither driver initialises (SIGSEGV).

## 4. What is stubbed, and why it stops there

The profile, save, session and prefab edge of `BandUser` and `VocalTrack` is
**not** linked.

- **Measured cost of linking it.** Adding just PrefabMgr, PracticeSectionProvider
  and BandUserMgr gave **95 undefined + 105 duplicate definitions**: SongMgr,
  LicenseMgr, Jukebox, SaveLoadManager, RockCentral, UIEventMgr, and a second
  RndText emitter.
- **`native/src/bandtrack_link_stubs.cpp` (rb3-render only)** holds:
  - **27 stubs that abort** with their own name. A wrong call turns the run red
    rather than silently returning a default.
  - **8 singletons**: 7 null pointers, plus zero-filled storage for the by-value
    `TheProfileMgr`.
  - **2 ProfileMgr bodies copied verbatim** (`GetProfileForUser`,
    `GetGameplayOptionsFromUser`). VocalTrack::Init reaches them, and for a
    null/unsaved user the real answer is "no profile".
- **The stubs are shown to fire:** before the bodies were copied, the first run
  aborted with `UNREACHED stub called: …GetGameplayOptionsFromUser`.
- **Not run:** `VocalTrack::UpdateScrolling` / `Restart`, the largest gap row.
  They need a live VocalPlayer, TheGame and TheSongMgr, so that is the next step
  for whoever extends this.

## 5. X360 build: unchanged, measured

`tools/ab_measure.py --patch` covered every src/ hunk on the branch (16 files) on
a fresh worktree at main `9480cb75e`. Prediction: Δ0, since every edit except
GetObj is `HX_NATIVE`-only and GetObj's r3 is unchanged. Measured:

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
leg A/B matched=53525 code%=57.959490; leg B recompiles: 20; units at 100% 554 -> 554
```

The later comment-only commit `5d914b61d` changes no line counts, so it was not
re-measured.

## 6. Gates

- `tools/native_health.sh`: verdict PASS, link 18/18, runtime 18/18, gates_pass
  **73** (was 57; rb3-render is **36**, of which 16 are `bt-`), scatter_unlinked
  16 (was 17), multihost 17 (was 20, from the duplicate-emitter guards).
  `NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=73 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none`
- `tools/native_build_gate.sh`: the result line is in the lane's final report; the
  gate was run last.
