# W16-UA: native targets read the config the Xbox retail game reads (2026-10-07)

Branch `w16-ua`. Follows W16-TW
(`W16TW_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md` §5.4), which found that
`rb3-render` reads the shipped config without the macros retail defines first, so
its joypad section has no `controllers` block and `JoypadInitCommon` aborts on it.

## Summary

- Every native driver that reads DTA (17 of 18; `rb3-frame` reads none) now
  defines `REGION_NA HX_XBOX HX_WIN HX_NG _SHIP` before its first read, in
  retail's order, through one shared header (`native/src/retail_boot_macros.h`).
  Each driver prints a `boot-macros` gate checking the macro table holds exactly
  those five.
- The list comes from the retail image, not from our source:
  `tools/retail_boot_config.py macros` decodes it from `band.exe` (§1).
- The same tool rebuilds the preinit config with no engine code, under those
  macros (§3). `rb3-ark` and `rb3-render` dump the config they actually read,
  and `tools/native_health.sh` requires both dumps to equal the reference.
  They do: **63,069 lines, 24 sections, EQUAL**.
- Doing that exposed a second, older native bug: **16 of the 27 files the
  preinit config reads were coming from host TEXT files in the rb3 Wii repo,
  not from the shipped `.dtb`** (§4). Fixed in `File_Native.cpp`.
- Retail's song list is **130**, not 138: eight test charts sit in
  `#ifndef _SHIP`. The `rb3-ark` and `rb3-song` song-count gates moved to 130.
- Native `PreInitSystem` (`src/system/os/System.cpp`, `HX_NATIVE` branch only)
  was missing `_SHIP`; added. The X360 branch already had it. The X360 path is
  unchanged token for token (§6).
- Controls: dropping any one of the five macros turns the health run red, on
  the `boot-macros` gate and on the config comparison; dropping `HX_XBOX` also
  fails `joypad-controllers`, and dropping `_SHIP` also fails
  `ship-dev-blocks-absent` and the song count (§5).
- `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`;
  `NATIVE HEALTH: PASS  (rc=0, link 18/18, 251 runtime gate(s), selftest=PASS)`.

## 1. What retail defines, and where the answer comes from

Retail boots as `App::App` → `SystemPreInit(argc, argv, "config/band_preinit_keep.dta")`.
Two functions write the DTA macro table before that file is read:

| order | name | defined by |
|---|---|---|
| 1 | `REGION_NA` | `SystemPreInit` (`0x82510EC8`) → `PlatformMgr::RegionInit` (`0x8251BD28`) → `SetRegion`. `XGetGameRegion()` 0xFF, and every non-European code, select NA; `SetRegion` defines `"REGION_" + upper("na")`. |
| 2–5 | `HX_XBOX`, `HX_WIN`, `HX_NG`, `_SHIP` | `PreInitSystem` (`0x82510BB8`), each as `(1)` from one `DataArrayPtr ptr(1)`, then one macro per `-define` option (a retail boot passes none), then `BeginDataRead()` and `DataReadFile(config)`. |

`DataInit` runs between the two and does not touch the macro table. The macros are
never undefined, so every later `DataReadFile` in the run sees them too.

The list is read off the retail image, not off our source, by
`tools/retail_boot_config.py macros`:

```
  SystemPreInit 0x82510ec8 calls RegionInit (call #6), DataInit (#26), PreInitSystem (#27)
  RegionInit 0x8251bd28: region 0xFF -> kRegionNA; SetRegion formats 'REGION_%s' -> REGION_NA
  PreInitSystem 0x82510bb8: DataSetMacro(Symbol(<const>)) x4 in order HX_XBOX HX_WIN HX_NG _SHIP, then the -define loop
retail boot macros, in order: REGION_NA HX_XBOX HX_WIN HX_NG _SHIP
```

- It decodes `band.exe` at the addresses `target_symbol_map.json` gives for the
  mangled names, tracking `lis`/`addi` constants into `r3`/`r4`. For each
  `bl ??0Symbol@@QAA@PBD@Z` it records the string in `r4`. A
  `bl ?DataSetMacro@@...` then takes the most recent one.
- It refuses unless `PreInitSystem` has exactly one more `DataSetMacro` call than
  constant names. That extra call is the `-define` loop, whose name is a runtime
  value. A decode that found nothing would therefore fail instead of reading as
  "no macros".
- The region is the one step it does not decode. It checks that the image holds
  `REGION_%s` and `na`, and takes NA from `RegionInit`, whose source matches retail
  at 100% (`report.json`), as does `SetRegion`.

## 2. What the macros change in the shipped data

A census over every `.dtb` in the ark (274 files, all parsed), counting the
`#ifdef`/`#ifndef` nodes that name each macro:

| macro | `#ifdef` | `#ifndef` | files | used as a bare symbol |
|---|---:|---:|---:|---|
| `HX_XBOX` | 660 | 1 | 30 | no |
| `_SHIP` | 3 | 40 | 24 | no |
| `REGION_NA` | 1 | 0 | 1 (`config/system.dtb`, inside `#ifdef HX_PS3`) | no |
| `HX_WIN` | 0 | 0 | 0 | no |
| `HX_NG` | 0 | 0 | 0 | no |

No engine code calls `DataGetMacro` on any of the five names either.

- **Prediction that failed:** I expected `REGION_NA` to show in the preinit
  config's `system` section. It does not: its one block (`band3 title_id`) sits
  inside `#ifdef HX_PS3`.
- So on Xbox only `HX_XBOX` and `_SHIP` select shipped content. A missing
  `HX_WIN`, `HX_NG` or `REGION_NA` can only be seen in the macro table itself,
  which is why every gate below checks the table as well as the content.

The preinit config, rebuilt by the reference under each macro set:

| macros | sections | differences from the retail read |
|---|---:|---|
| retail (all five) | 24 | — |
| none (native before W16-UA) | 25 | `joypad` has no `controllers` and has `breed_data_string_mappings`; `timer` lacks `world_regular`/`world_postproc`; `ui` has `cheat_init`; there is a `hostnames` section; `system/language` has `cheat_supported` |
| all but `HX_XBOX` | 24 | `joypad` has no `controllers`; `timer` lacks `world_regular`/`world_postproc` |
| all but `_SHIP` | 25 | `hostnames`, `ui/cheat_init`, `joypad/breed_data_string_mappings`, `system/language/cheat_supported` |
| all but `REGION_NA`, `HX_WIN` or `HX_NG` | 24 | none |

`songs/songs.dtb` holds 138 top-level songs. Eight sit inside `#ifndef _SHIP`:
`coldasice_nobass`, `_budget_test`, `_invalid_version_test`, `runs_16s`, `sustains`,
`_bre_test`, `framerate`, `vocaltrainertest`. A console reads 130, all 130 with a
`song_id`.

## 3. The reference: `tools/retail_boot_config.py`

The reference rebuilds the config with no engine code:

- It reads the shipped `.dtb` bytes straight out of the ark, using
  `native/tools/ark_extract.py`'s independent archive reader and `Rand2`.
- It loads them with its own copy of `DataArray::Load`'s rules:
  - the global conditional stack;
  - macro symbols spliced at element level;
  - `#define`/`#undef`, and `#autorun` (only function-defining autoruns are
    accepted, since they add no nodes; the one in the config is `band_macros.dta`'s
    `{func facing_string …}`);
  - `#include`, and `#merge` through `DataMergeTags`, where retail's
    `FindArray(int)` compares the raw tag word, i.e. symbol identity;
  - `FileMakePath` path resolution relative to each array's own file
    (`gFile = mFile` after an include);
  - `CachedDataFile`'s `gen/*.dtb` mapping;
  - the `gReadFiles` session cache, with merged arrays shared by reference as in
    the engine.
- `view` writes a canonical dump: a `# boot macros:` line, then one node per line
  with type tags (`i`, `f %.9g`, `s`, `t "…"`, `v`, `(`, `{`, `[`).
  `native/src/retail_boot_macros.h` `DumpConfig` writes the same format from a
  live `DataArray`.
- `check` diffs a native dump against `view` under the image-derived macros. It
  exits 0 when equal, 1 when different, and 2 when it cannot run.

Controls on `check` itself, run before any native build:

| input | result |
|---|---|
| its own `view` output | `EQUAL`, rc 0 |
| the same with two integers changed | `DIFFERENT`, 92 lines, rc 1 |
| the view with no macros (the old native config) | `DIFFERENT`, 2,105 lines, rc 1 |
| a missing file | rc 2 |

## 4. What defining the macros exposed: the config was not the shipped config

**Prediction:** with the five macros defined, `rb3-ark`'s dump would equal the
reference. **Result:** it did not. `check` reported DIFFERENT, and the diff was
nothing to do with macros: every backslash inside a string or symbol came out
doubled. The pre-fix dump is kept at `~/tmp/w16ua_ark_dump_prefix.txt`;
re-checked now:

```
CONFIG-VIEW: DIFFERENT /home/free/tmp/w16ua_ark_dump_prefix.txt -- 8 differing lines (retail 63069 lines, native 63069)
-    t " ABCDEFGHIJKLMNOPQRSTUVWXYZ...0123456789!\"#$%&'()*+,-./:;=?@[\\]^_`{|}~...
+    t " ABCDEFGHIJKLMNOPQRSTUVWXYZ...0123456789!\"#$%&'()*+,-./:;=?@[\\\\]^_`{|}~...
-          t "Number \\"
-          s across\"
+          t "Number \\\\"
+          s across\\"
```

(The other two of the 8 lines are the header, which at that point also listed
`songs.dta`'s `COMMON_*` macros because `rb3-ark` read songs first. `rb3-ark` now
checks the config before songs, which is also retail's order.)

The cause is in native `FileIsLocal`. W16-era loose-file support
(`NativeLooseFileExists`) treats a path as a loose host file when the archive does
not hold it. It asked the archive about the **`.dta`** name. The ark never holds a
`.dta`; `CachedDataFile` asks `FileIsLocal(foo.dta)` and, when that says "not
local", reads `dir/gen/foo.dtb` from the ark. So any `.dta` that also existed on
the host was read as text. With the data dir at
`~/code/milohax/rb3/orig-assets/xbox-zip`, `../../system/run/...` resolves into
the rb3 Wii repo's `system/run/`, and **16 of the 27 files** in the preinit read
(`default`, `macros`, `joypad`, `objects`, every `*_objects.dta`, ...) came from
there. They happen to be content-equal to the shipped `.dtb` except for how the
text tokenizer treats backslashes, which is why only 6 content lines differed.

Fix (`native/src/platform/File_Native.cpp`): a `.dta` is not loose when the ark
holds its `<dir>/gen/<base>.dtb`. After it, both dumps are EQUAL. This affects
every native target that reads DTA from a data dir with host-side neighbours, not
only the config: the songs, the UI and the milo `.dta` reads all go through the
same check.

## 5. Gates, and the controls that show they can fail

Gates added (all print `[PASS]`/`[FAIL]` and count toward the driver's rc):

| gate | where | checks |
|---|---|---|
| `boot-macros` | 17 drivers | the macro table, right after `Define()`, is exactly the five names |
| `preinit-config`, `config-dump` | `rb3-ark` | the preinit config reads, and its canonical dump is written (`--config-dump`) |
| `joypad-controllers` | `rb3-ark` | `joypad/controllers` exists; `strat_xbox_rb2` detects as type 6 (`kJoypadXboxHxGuitarRb2`) and `hx_drums_xbox` as 8 (`kJoypadXboxDrums`) |
| `joypad-ignore-empty` | `rb3-ark` | the game's empty `ignore` wins the merge |
| `ship-dev-blocks-absent` | `rb3-ark` | no `joypad/breed_data_string_mappings`, no `hostnames`, no `ui/cheat_init` |
| `song-count == 130` | `rb3-ark`, `rb3-song` | was 138 |
| `config-dump` | `rb3-render` | `StandUpConfig` dumps the config it read, before its `objects` merge edits it |
| `tw-joy-syscfg` | `rb3-render` | `SystemConfig("joypad")` equals W16-TW's macro-defined joypad fixture node for node (44,825 canonical bytes each) |

`tools/native_health.sh` adds a BOOT CONFIG section: all 17 drivers must print
`[PASS] boot-macros` (a missing line fails), and both dumps go through
`retail_boot_config.py check` (rc 1 fails the run, rc 2 is UNRUNNABLE).

```
  boot-macros: 17 of 17 DTA-reading target(s) printed [PASS] (0 unrunnable, not checked)
  rb3-ark CONFIG-VIEW: EQUAL /home/free/tmp/native_health_syscfg_ark_4121108429.txt -- 63069 lines, 24 top-level sections, macros REGION_NA HX_XBOX HX_WIN HX_NG _SHIP
  rb3-render CONFIG-VIEW: EQUAL /home/free/tmp/native_health_render_out_4121108429/syscfg_preinit.txt -- 63069 lines, 24 top-level sections, macros REGION_NA HX_XBOX HX_WIN HX_NG _SHIP
```

`--selftest` gains one control per macro: run `rb3-ark` with
`RB3_BOOT_MACRO_DROP=<name>` and require both a failed gate and a DIFFERENT view.
**Prediction** (from §2): `REGION_NA`, `HX_WIN` and `HX_NG` change only the macro
table, so only `boot-macros` and the view's header line can catch them;
`HX_XBOX` also breaks `joypad-controllers`; `_SHIP` also breaks the dev-block and
song-count gates. **Result**, all five as predicted:

```
  RED   bootmacro-drop-REGION_NA -- rc=1, 1 gate(s) failed (boot-macros ); config view: DIFFERENT -- 2 differing lines (control WORKS)
  RED   bootmacro-drop-HX_XBOX -- rc=1, 2 gate(s) failed (boot-macros joypad-controllers ); config view: DIFFERENT -- 1043 differing lines (control WORKS)
  RED   bootmacro-drop-HX_WIN -- rc=1, 1 gate(s) failed (boot-macros ); config view: DIFFERENT -- 2 differing lines (control WORKS)
  RED   bootmacro-drop-HX_NG -- rc=1, 1 gate(s) failed (boot-macros ); config view: DIFFERENT -- 2 differing lines (control WORKS)
  RED   bootmacro-drop-_SHIP -- rc=1, 3 gate(s) failed (boot-macros ship-dev-blocks-absent song-count ); config view: DIFFERENT -- 1064 differing lines (control WORKS)
```

The 2-line diffs for `REGION_NA`/`HX_WIN`/`HX_NG` are the header line alone. That
is the point of §2: for those three, the table is the only place a mistake shows.
Each control is only scored when `rb3-ark` was green and its view EQUAL in the
same run, so a control cannot pass on a run that was already broken.

## 6. The X360 build

The `System.cpp` edit is inside `#ifdef HX_NATIVE` (lines 584–626). The X360
`#else` branch already defines all four, `_SHIP` included. Instead of a
whole-binary build, I preprocessed the TU with the X360 compile's own flags plus
`/EP` (cl 10224 under wibo), once for the edited file and once for the
`b3015fcb2` version placed next to it, and compared the output with blank lines
removed: **byte-identical, 25,011 non-blank lines** (`cmp` rc 0). The 4 extra
lines are blank lines from the dropped native branch. `/EP` expands `__LINE__`,
so identical output also rules out a line-number-dependent token. The compiler
sees the same tokens, so the object is unchanged. I did not run an X360 build
(memory was short, and this is the stronger check).

## 7. Results

Measured on branch `w16-ua` at `6fd481428` (base `b3015fcb2`), worktree
`~/tmp/wt-w16ua`, with nothing else building:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE HEALTH: PASS  (rc=0, link 18/18, 251 runtime gate(s), selftest=PASS)
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=251 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=13 scatter_dirb=0 scatter_multihost=12 rc=0 handpose_controls=3/3 handpose_baseline_fail=1 runtime_crashed=0 runtime_failed=none
```

The selftest reported 14 controls red, 0 stuck green, 0 skipped.

`main` moved to `37eaf0644` (W16-TY) after this branch was cut. `git merge-tree`
reports a clean merge. I did not rebase, and the gates above were not re-run on
the merged tree.

## 8. Not done

- No native driver runs the real `PreInitSystem`; they still hand-roll bring-up
  (`boot_invariants.h`) and call `Define()` instead. The `System.cpp` fix is for
  whatever runs it later.
- `-define` options are not supported. A retail boot passes none.
- The loose-file check now covers `.dta` → `gen/*.dtb`. The milo path mapping
  (`.milo` → `.milo_xbox`) goes through a different check, which I did not audit.
- `rb3-render`'s `JoypadChecks` still builds W16-TW's fixture rather than using
  `SystemConfig("joypad")` directly. `tw-joy-syscfg` shows the two are equal.
- `config/band_keep.dta` (the `SystemInit` half) is still not read anywhere; see
  `main_render.cpp`'s `StandUpConfig` comment.
