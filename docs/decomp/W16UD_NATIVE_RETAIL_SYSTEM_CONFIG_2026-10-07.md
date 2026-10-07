# W16-UD — the ten no-disc native drivers now read retail's system config off the disc

Branch `w16-ud`. Follows W16-UC (`W16UC_NATIVE_FILE_SOURCES_AUDIT_2026-10-07.md`),
which recorded that ten native drivers (`rb3-gem`, `-hit`, `-score`, `-score2`,
`-score3`, `-score4`, `-vocal`, `-vocal2`, `-harmony`, `-crowd`) built their scoring,
beatmatcher and crowd config from DTA text compiled into the binary, not from the
shipped `gen/*.dtb`. W16-UC's file audit could not see that, because a compiled-in
string involves no file read.

## Decision: load it, no fallback

The task allowed two routes: load the shipped data, or prove a compiled-in copy is
identical and fail on drift. **I chose loading, with no compiled-in fallback.**

- The compiled-in copies were **not** identical. Every one differed from retail
  (see Defects). A proof-of-identity route would have had to regenerate ~348 k lines
  of config and macros into each driver.
- Loading uses the engine's own boot path (`DataReadFile` + the real `InitSystem`),
  so the native run also exercises `DataMergeTags` / `DataReplaceTags` /
  `StripEditorData` the way the console does.
- Without the disc a driver stops with **rc 2** and
  `[FAIL] retail-config -- no disc image at …`. That run cannot quietly use a config
  that no console reads.

## Method

### Which file retail reads

Retail's `App::App` (`??0App@@QAA@HPAPAD@Z`, `0x82270E68`) makes exactly one call to
`?SystemInit@@YAXPBD@Z`, at `0x82271014`. `tools/retail_boot_config.py`
(`retail_system_config()`) decodes the `r3` string for that call from the image and
raises if there is not exactly one call. The decoded file is
`config/band_keep.dta`; `macros` prints this evidence.

Combined with W16-UA's preinit read, retail's config is built as follows:

```
PreInitSystem  BeginDataRead; gSystemConfig = DataReadFile("config/band_preinit_keep.dta")
               DataVariable("syscfg") = gSystemConfig
InitSystem     sys = DataReadFile("config/band_keep.dta")
               DataMergeTags(sys, pre); DataReplaceTags(sys, pre); gSystemConfig = sys
               StripEditorData(); FinishDataRead()
```

All of this runs under the five boot macros `REGION_NA HX_XBOX HX_WIN HX_NG _SHIP`
(W16-UA).

### Native side: `native/src/retail_system_config.h`

`RetailSystemConfig::Boot()` does the following:

- mounts the disc (`NativeSetDataDir`, `SetUsingCD(true)`, `NativeArchiveInit`);
- performs PreInitSystem's read;
- calls the real `InitSystem("config/band_keep.dta")`;
- requires that both `scoring` and `beatmatcher` sections exist;
- prints `[PASS] retail-config`.

With `RB3_CONFIG_DUMP=<path>` it writes the installed config, followed by the whole
macro table (`# macro table after the read`, one `m NAME` per macro, sorted). The
format is the canonical one shared with `retail_boot_config.py`.

Each of the ten drivers now calls it once, immediately after `ObjectDir::PreInit`:

```cpp
if (int rc = RetailSystemConfig::Boot()) // retail's config + macros, off the disc (W16-UD)
    return rc;
```

### Reference side: `tools/retail_boot_config.py --full`

The script rebuilds the same config **with no engine code**:

- `read_full_config`: preinit read, system read, `merge_tags`, `strip_editor_data`;
- `dump_macros`;
- `check <dump> --full [--reference FILE]` prints `CONFIG-VIEW: EQUAL|DIFFERENT`
  with rc 0/1/2.

The preinit-only view (W16-UA) is byte-identical to its output before this change.

### Per-driver source table (measured, health run `1654587320`)

| driver | config source now | dump vs `.dtb` rebuild | was |
|---|---|---|---|
| rb3-gem | disc, `Boot()` | EQUAL, 348,563 lines | hand `(beatmatcher …)` string |
| rb3-hit | disc, `Boot()`; watcher = `SystemConfig("beatmatcher")->FindArray("watcher")` as `BeatMatcher.cpp:327` | EQUAL | hand beatmatcher + hand watcher string |
| rb3-score | disc, `Boot()`; `SystemConfig("scoring")`; star thresholds from `star_ratings/instrument_thresholds` | EQUAL | hand scoring block + hardcoded star floats |
| rb3-score2 | disc, `Boot()`; M5 cross-check reads `SystemConfig("scoring")` | EQUAL | `scoring_config_dta.h` + hand `TRACK_SYMBOLS` |
| rb3-score3 | disc, `Boot()` | EQUAL | `kConfigDta` splice + `TRACK_SYMBOLS` |
| rb3-score4 | disc, `Boot()` | EQUAL | same |
| rb3-vocal | disc, `Boot()` | EQUAL | same |
| rb3-vocal2 | disc, `Boot()` | EQUAL | same |
| rb3-harmony | disc, `Boot()` | EQUAL | same |
| rb3-crowd | disc, `Boot()`; crowd = `(scoring (crowd …))` of retail's config | EQUAL | `crowd_config_dta.h` splice |

`native/src/crowd_config_dta.h` and `native/src/scoring_config_dta.h` are deleted.

## Defects the compiled-in configs carried (hand copy vs retail)

| item | hand copy | retail |
|---|---|---|
| `track_mapping` audio types | GUITAR 4, BASS 3 | GUITAR 1, BASS 2; 19 entries incl. HARM1-3, PART REAL_KEYS_E; `drum_style_instruments (0)`, `vocal_style_instruments (3)` |
| `keyboard_range_shift_duration_ms` | 100 | 200 |
| `player_slot` / `hopo_threshold` | carried `low_vocal_pitch`/`high_vocal_pitch` | values 9 / 170, no vocal-pitch keys |
| watcher `pitch_bend_range` | 2 | 1 |
| watcher `trill_interval_ms` | 100s | `(0 0 160 160)`; `roll_interval_ms` non-empty, extra keys |
| controllers mapping | subset | larger |
| streak multipliers | guitar/drum/keys lists | `singleplayer bass real_bass multi default vocals` only |
| energy lists, overdrive | trivial / fewer keys | non-trivial / extra keys |
| `unison_phrase` (score2) | reward 0.5, penalty 0.5, no `point_bonus` | reward 2, penalty 2, point_bonus 1000 |
| `TRACK_SYMBOLS` | `real_guitar_22fret`, `real_bass_22fret`; no `none pending pending_vocals` | `drum guitar bass vocals keys real_keys real_guitar real_guitar_22 real_bass real_bass_22 none pending pending_vocals` |
| `(track_graphics …)` (score4/crowd) | inside `scoring` | top level |
| star `instrument_thresholds` | — | **matched** (the one block that was right) |

## Knock-on fixes the real config forced

1. **`score_engine.cpp` streak list.** Retail has no `guitar`/`drum`/`keys`
   streak list. `Scoring::GetStreakList` falls back to `default`, and the native
   score engine now does the same (`FindArray(name, false)`, else `"default"`).
2. **SongParser map binding (six drivers).** Retail's
   `vocal_style_instruments (3)` routes vocal tracks to the vocal parser. Its
   `SongData::AddLyricShift` reads `mTempoMap` mid-parse.
   - The drivers passed local `tempoMap`/`measureMap` objects, so
     `songData.mTempoMap` was null and **rb3-score3 crashed (SIGSEGV)**.
   - They now bind `songData.mTempoMap` / `songData.mMeasureMap` as
     `SongData::Load` does (`SongData.cpp:224`), and copy them out afterwards.
   - Affected drivers: crowd, score3 (×2), score, hit, score4.

## Output diffs vs baseline (same binaries before/after, `~/tmp/w16ud/{base,new}`)

| driver | change |
|---|---|
| gem, hit, score | `track_mapping` now knows HARM1-3 / PART REAL_KEYS_E. They were "bad track name" before, so track indices shift. Verdicts and scores unchanged. New notice: "No drum submix specified". gem also prints `stagekit_present not function or object (config/midi_parsers.dta:1775)`. |
| score2 | identical |
| score3 | main stage unchanged. Band stage: 37 → 44 phrase ids, scoring tracks 0xf → 0x2b, capturer 4 completed → 3 completed + 1 failed, windows 5/5 → 4/4, unison 19, PART BASS 3. |
| score4, crowd | numbers unchanged; analyzer phrase ids 31 → 47 |
| vocal, vocal2, harmony | numbers unchanged. vocal2/harmony now run retail's `(player (handlers))`, which prints `frac not function or object (config/player_net.dta:53)` 56–63 times. The "SystemConfig: no 'bass' section (native partial config)" lines are gone. |

### Follow-up (not done): `frac` and `stagekit_present`

Retail registers both as script functions; both strings sit standalone in
`band.exe`.

- `stagekit_present` is registered by the unpinned `fn_82522608` (0x1E0 B).
- `frac`'s string is at file offset 1,353,456. Its registrar was **not** found
  by an r3/r4 decode.

The native engine lacks both, so these lines are a native gap now made visible.
It is not a config defect, and this lane leaves it as follow-up.

## Controls (predictions vs results)

`tools/native_health.sh` gains a **RETAIL CONFIG** section with these checks:

- the reference is built via `view --full`;
- a per-target `check --full --reference`, where DIFFERENT is a runtime failure;
- a source check that no SYSCFG driver writes `gSystemConfig`, calls
  `DataSetMacro`, or includes the deleted headers, and that each calls `Boot()`
  exactly once.

The ten targets now `--needs "$ASSETS"`, so the W16-UC file audit judges them as
disc readers. There are four new selftest controls, each paired with a green
positive run:

| control | prediction | result |
|---|---|---|
| `syscfg-edit`: one float in rb3-crowd's `(scoring (crowd …))` → 99 | DIFFERENT, rc 1 | **RED**, rc 1, 2 differing lines |
| `syscfg-drop-ship`: rb3-score4 with `_SHIP` dropped | dump DIFFERENT | **prediction failed**: the config `#include`s `ui/dev_only/selvenue.dta`, which the retail disc does not ship; `DataReadFile` can't open it and the run SIGSEGVs (rc 139) before any dump. This also confirms the disc is a `_SHIP` image. Replaced by: |
| `syscfg-drop-xbox`: rb3-score4 with `HX_XBOX` dropped | `[FAIL] boot-macros` + content diff >2 lines | **RED**: 78 vs 79 top-level sections, 1,734 differing lines |
| `syscfg-nodisc`: rb3-crowd, `RB3_ASSETS` = empty dir | rc 2, `[FAIL] retail-config` | **RED** |
| `syscfg-source`: copy of `main_crowd.cpp` + a planted `gSystemConfig = DataReadString(…)` | flagged | **RED** |

The first awk anchor in `syscfg-edit` would have edited the first depth-4
`s crowd` anywhere in the dump. It is now anchored on `s scoring`; a dry run
confirmed the edit lands at line 224,939, inside scoring.

## Results (worktree `~/tmp/wt-w16ud`)

Native build gate (`~/tmp/w16ud/gate_final.log`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Native health with selftest (`~/tmp/w16ud/health_final.log`):

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=297 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=13 scatter_dirb=0 scatter_multihost=10 rc=0 handpose_controls=3/3 handpose_baseline_fail=1 runtime_crashed=0 runtime_failed=none retail_config=10/10
```

Other lines from that run:

- `retail config: 10 of 10 run target(s) EQUAL to the .dtb rebuild`
- `file sources: 18 target(s) clean, 0 with reads outside the disc image`
  (rb3-crowd: 261 archive lookups, 0 misses)
- `selftest: PASS (21 red, 0 stuck-green, 0 skipped)`

`retail_config=<EQUAL>/<expected>` is a new field appended at the end of the
contract line.

## What this lane deliberately did not do

- **rb3-gem's MidiParser probe typedef** (`DataReadString("(track_name 'PART BASS') (gem …)")`)
  is kept. It is a test parser definition, not system config.
- **rb3-midi / rb3-song** minimal configs are untouched. They were not among
  W16-UC's ten.
- **Config mutation after boot is not covered.** The dump is taken right after
  `InitSystem`. Anything a driver later writes into `gSystemConfig` would not
  show, except where the source check forbids it.
- No X360 match-build change: no file under `src/` was touched, so there is no
  A/B to run.
- Did not chase `frac` / `stagekit_present` (see follow-up).
