# W16-UG — script functions retail registers that native targets reached without one

Branch `w16-ug`. Follows W16-UD (`W16UD_NATIVE_RETAIL_SYSTEM_CONFIG_2026-10-07.md`),
which put ten native drivers on retail's disc config and so made two
`not function or object` notices visible: `frac` (rb3-vocal2 63×, rb3-harmony
56×) and `stagekit_present` (rb3-gem 1×). This lane finds every script function
retail registers that a native target reaches without a handler, implements
each from retail, and moves rb3-midi and rb3-song onto the disc config.

## Result

| name | reached by | retail registers it in | native now | why the notice is gone |
|---|---|---|---|---|
| `stagekit_present` | rb3-gem; rb3-midi once on retail config (`config/midi_parsers.dta:1775`) | stage-kit init `0x82522608`, called by `SystemInit` at `0x825113E0` | the whole stage-kit TU ported from retail bytes (`src/system/os/StageKit.cpp`), registered by `StageKitInit()` | the handler exists and returns `JoypadStageKitPadNum() != -1`: no pad whose controller type is `stagekit_xbox` (shipped `joypad.dta`) is connected, so it returns 0 and `midi_parsers.dta` builds none of its five stage-kit parsers, as on a console without a Stage Kit |
| `frac` | rb3-vocal2, rb3-harmony (`config/player_net.dta:53`) | **not a C++ registration**: `{func frac ($x) {int {'*' $x 256}}}` at `ui/global.dta:237`, run by `UIManager::Init`'s `Handle(init)` | retail's own definition, executed from the shipped config | `Boot()` runs the 109 `{func ...}` commands of `(ui (init ...))` |

No other retail-registered name is reached: after the change, **0** `not function
or object` lines across all 18 targets (measured below), and two selftest
controls show the gate reports each of the two when its registration is
dropped.

Correction to W16-UD: it said retail registers both as script functions and
that `frac`'s registrar "was not found by an r3/r4 decode". `frac` has no C++
registrar. The `frac` string W16-UD found (file offset 1,353,456,
VA `0x8214A6F0`) is one entry of the D3DX HLSL intrinsic table (`saturate`,
`rsqrt`, ..., `fwidth`, `frac`, `faceforward`, ...), referenced only by a data
pointer at `0x8214C544`, and no code builds its address.

## Method

### Retail's registrations: `tools/retail_script_funcs.py`

A registration is `DataRegisterFunc(Symbol(name), func)`. Retail emits it two
ways, and the tool decodes both from `band.exe`:

- **call**: `bl Symbol::Symbol(const char *)` with `r4` = the name, then
  `bl DataRegisterFunc` (`0x827639C0`) with `r4` = the function;
- **inline**: the same ctor, then `bl map<Symbol,DataFunc*>::operator[]`
  (`0x82359F28`) in a function that addresses `gDataFuncs` (`0x82E05D30`),
  with the function address completed by an `addi` before the store.

It refuses (rc 2) unless three known answers come out:

- `DataInitFuncs` gives the 147 string-literal names of our `DataFunc.cpp`, in
  source order;
- the stage-kit init gives 13 `stagekit_*` names;
- every `bl DataRegisterFunc`, and every `operator[]` call in a `gDataFuncs`
  function, is accounted for except exactly 17 sites whose name is built on the
  stack: `DataInitFuncs`' 7 `magic` keys, `Synth::InitSecurity` 2,
  `ByteGrinder::Init` 8. All three are in our source and shared with native.

The calibration caught one defect in the first version. `object` came out with
no function, because `DataObject` sits at `0x82760000` exactly. The filter for
`lis`-only half addresses had dropped it, so a function address now counts only
when an `addi` completes it.

Result: **284 named registrations in 41 registrars** (`list` prints them all).

### Reach: the runtime notice, over all 18 targets

`DataArray::Execute` prints `<name> not function or object (file F, line L)`
whenever script calls a name with no C++ function, no script `func` and no
object behind it. The HX_NATIVE arm is unconditional. No native code installs
`DataArray::sDefaultHandler` or disables `TheDebug` (checked), so a reached
missing handler cannot be silent.

Before (W16-UD health run `1654587320`, every target's log):

| target | lines |
|---|---|
| rb3-vocal2 | 63 × `frac` (`config/player_net.dta:53`) |
| rb3-harmony | 56 × `frac` |
| rb3-gem | 1 × `stagekit_present` (`config/midi_parsers.dta:1775`) |
| the other 15 | 0 |

rb3-midi and rb3-song printed none, but they ran no retail script then. Once on
the disc config, rb3-midi reaches `stagekit_present` too (see the controls).

### Static complement: what native lacks but never reaches

`RB3_DATAFUNCS_DUMP` writes every C++ function a Boot() driver has registered
at exit, and `retail_script_funcs.py census` compares the list with retail's.
Every Boot() driver gives the same answer: **169 registered; 122 of retail's
284 names absent, from 37 registrars**. All 122 come from engine or game
`Init`s the drivers do not run. Some are in our tree (`FileInit`, `TrigInit`,
`PreInitSystem`, `ObjectDir::Init`, `TimeConversionInit`, `OptionInit`,
`CheatsInit`, `AutoTimer::Init`, `TaskMgr::Init`, `LoadMgr::Init`, ...). Others
are UI or game objects a driver never constructs (`RndConsole` 14,
`GameMicManager` 5, `MicManagerXbox` 4, `GemTrack`, `GemManager`, `VocalTrack`,
`RealGuitarGemPlayer`). **None is reached** (0 lines), so each is recorded here
with that reason and not implemented.

The gate below fails the first time a target reaches one.

## The stage-kit TU (`src/system/os/StageKit.cpp`)

No source tree has it: DC3 dropped the Stage Kit, and Wii-target source only
stubs the `stagekit_*` cues. It was ported function by function from retail
`0x82521B30`..`0x825227E8`; each body cites its address. Its state:

- a 32-entry raw-command ring (full ring drops its oldest);
- per-bank mode, 8-step pattern and enable flag;
- the LED bytes to send and the bytes last sent;
- fog, re-sent every 2,000 ms (`Timer::SplitMs() > 2000.0f`);
- strobe.

`.data` initial values were read off the image: enable `{1,1,1,1}`, pattern id
`{6,6,6,6}`, last bank `3`.

The tree already declared five of its entry points without a body, under names
prior lanes chose. They match retail's call sites, and the port keeps them:

| name | address |
|---|---|
| `StageKitConnected` | `0x82521C80` (BandDirector::SetFog) |
| `StageKitSetFog` | `0x82521D80` |
| `StageKitSetLedPattern` / `SetLedState` / `SetStrobe` / `UpdateLeds` | `0x82521BF0` / `0x82521B98` / `0x82522028` / `0x82521E20` (LightPreset::Keyframe::ApplyStageKit) |
| `StageKitPoll` | `0x82521ED0` (SystemPoll's last call) |

What changed with it:

- `w16ts_link_support.cpp`'s two stub bodies are deleted.
- `LightPreset::Keyframe::ApplyStageKit` is no longer compiled empty natively.
- The native `SystemInit` / `SystemPoll` arms make retail's two calls.

All 13 handlers are registered, not only `stagekit_present`. Retail registers
them together, and shipped script reaches more than one of them without any
Stage Kit attached:

- the venue world's `enter`/`exit` (`world/world_objects.dta:1177,1190`) and
  `ui/game.dta`'s `game_over` call `{stagekit_reset}`;
- `enter` sets `$stagekit TRUE`, which turns on `{stagekit_left_right ...}` in
  `config/beatmatcher.dta`'s beat callbacks.

## X360 finding: `SystemInit` called the wrong function

Retail's `SystemInit` tail is:

1. `TheContentMgr->Init()` (the `bctrl` at `0x825113DC`);
2. `bl 0x82522608` (the stage-kit init);
3. the exit-callback insert.

Our retail arm called `GlitchFinder::Init()` in slot 2. **`band.exe` contains
no `glitch_find*` string and no GlitchFinder.** The row read 100 because
objdiff forgives a placeholder target name (`fn_82522608` has none), so a wrong
callee and a right one score the same. The arm now calls `StageKitInit()`.
`StageKit.cpp` is not in `objects.json` and not pinned: the call has a name
and no body in the match build, like `StageKitPoll` before it.

A/B (`tools/ab_measure.py --revert 2a4e78484`, so the sign is inverted):
predicted Δ0 on every measure. Measured: **Δ0 on every measure.**

```
  leg A: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 0, settled)
  leg B: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 16, split=0, patch_steps=6, settle iterations: 2)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000000pp   (legA 64.315315 -> legB 64.315315)
  units at 100% [mpn ruler]: legA 607 -> legB 607  (Δ+0; 0 reached 100, 0 fell off; pairable units 1732->1732)
```

Leg B recompiled 16 TUs, so the run was not absent-vs-absent. Δ0 is the expected
result: `SystemInit` was already 100 under the forgiven placeholder. The change is
for correctness: the call now names the function retail calls.

## rb3-midi and rb3-song on the disc config

Both drivers used a one-entry stand-in config. Now each calls
`RetailSystemConfig::Boot(dataDir)`, which mounts the `dataDir` it was given on
the command line; `Boot()` gained that optional argument. rb3-song now also
calls `DataInit()` and `ObjectDir::PreInit`, which Boot() requires. Loose mode
(`--loose`) still reads its config off the disc at `$RB3_ASSETS` and then turns
`UsingCD` off for its host file.

| driver | stand-in config | what changed |
|---|---|---|
| rb3-midi | an empty `(beatmatcher)`, so `MidiParserMgr` built none of retail's parsers | the manager's ctor now runs retail's `(beatmatcher (midi_parsers (init ...)))` and `FinishLoad` its `(finish_loading ...)`. Retail defines an `events_parser`, so the driver's two probes are renamed `probe_events_parser` / `probe_note_parser`. Gates and event counts are unchanged. It now reaches `{stagekit_present}`. |
| rb3-song | an empty `(missing_song_data)` | the 342 `Data X is not Int` lines are gone: `TRUE` 211, `FALSE` 1, `kTempoMedium` 97, `kTempoFast` 18, `kTempoSlow` 15 (one tempo per song for all 130). Those macros come from retail's config, so every song's tempo and boolean fields failed to read before. Song count (130), round-trips and ratings are unchanged. No other target's UD log has this error class. |

Both now dump their config, and `native_health.sh` checks it against the `.dtb`
rebuild: `SYSCFG_TARGETS` went from 10 to 12.

## `RetailSystemConfig::Boot()` and the health gate

After `InitSystem`, `Boot()` calls `RegisterScriptFuncs()`. It runs the two
places in retail's boot that register functions the shipped config calls:

- `StageKitInit()`;
- the `{func ...}` commands of `(ui (init ...))`, in order: **109 of its 540
  commands**.

The other 431 commands (`new` panels, `set` globals, ...) need a UI and are
counted and skipped. It prints:

```
  [PASS] retail-script-funcs -- StageKitInit: 13 stagekit_* registered; (ui (init ...)): 109 {func} command(s) run, 431 other command(s) skipped (need the UI)
```

`RB3_SCRIPT_FUNC_DROP=stagekit|ui_init` skips one of the two, for the selftest.

`tools/native_health.sh` changes:

- **UNHANDLED SCRIPT CALLS**: counts `not function or object` lines in every
  run target's log. Any line fails that target (`<t>:unhandled-call`). Each
  SYSCFG target must also print `[PASS] retail-script-funcs`.
- `unhandled_calls=<N>` is appended to the end of the contract line.
- The source check now counts `RetailSystemConfig::Boot(`, so `Boot(dataDir)`
  counts too.
- Two selftest controls, each paired with a green positive run that had 0 lines.

| control | prediction | result (by hand, before wiring) | result (health selftest) |
|---|---|---|---|
| `scriptfunc-drop-ui`: rb3-vocal2 with `ui_init` dropped | `frac` lines return | **63** × `frac (config/player_net.dta, line 53)`, the same count W16-UD measured | RED: `63 unhandled line(s), frac among them (control WORKS)` |
| `scriptfunc-drop-stagekit`: rb3-gem with `stagekit` dropped | `stagekit_present` returns | **1** × `stagekit_present (config/midi_parsers.dta, line 1775)`; rb3-midi likewise 1 | RED: `1 unhandled line(s), stagekit_present among them (control WORKS)` |

Other output changes vs W16-UD's logs, notices excluded:

- the new `[PASS] retail-script-funcs` line;
- rb3-gem prints one `Resizing hash table (521)` (more symbols interned);
- rb3-vocal2 is otherwise identical.

## Results (worktree `~/tmp/wt-w16ug`)

`tools/native_build_gate.sh`:

```
layout:    LAYOUT_ODR_RESULT verdict=PASS x360_tus=1267 x360_failed=0 x360_split=0 x360_unresolved=0 x360_allowed=138 x360_stale=0 native_tus=1953 native_failed=0 native_split=0 native_unresolved=0 native_allowed=2 native_stale=0 rc=0
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`tools/native_health.sh ~/tmp/wt-w16ug --selftest`:

```
  retail config: 12 of 12 run target(s) EQUAL to the .dtb rebuild (12 expected; ...)
  unhandled calls: 0 line(s) in 0 of 18 target log(s)
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=320 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=13 scatter_dirb=0 scatter_multihost=10 rc=0 handpose_controls=3/3 handpose_baseline_fail=1 runtime_crashed=0 runtime_failed=none retail_config=12/12 unhandled_calls=0
```

Every selftest control went RED, including the 14 from earlier lanes. With 0
unhandled lines across all 18 logs, rb3-harmony's 56 `frac` lines (W16-UD's run)
are gone too. I did not run that target by hand before this.

Process note: `ab_measure` restores the worktree on exit, so it **deleted the
untracked draft of this doc**, which I had written while the run was in
progress. The tool prints a loud banner when this happens. I recovered the doc
from the session transcript. Write deliverables to `~/tmp` while an A/B is
running.

## What this lane deliberately did not do

- **Did not pin or match `StageKit.cpp`** for the X360 build. Its extent is
  `0x82521B30`..`0x825227E8`, inside the unpinned `auto_03_825219A0_text` unit
  (which also holds an unrelated file-pattern routine at `0x825219A0`). That is
  a carve and pin lane of its own.
- **Did not run the other 431 `(ui (init ...))` commands.** They construct UI
  panels and set UI globals, and no native target has a UIManager.
- **Did not register the 122 unreached names** (see the census). Each one's
  `Init` belongs to a subsystem the drivers do not boot. The new gate names the
  first one a target reaches.
- **Non-Boot targets** (rb3-dta, -ark, -milo, -render, -frame, -save) have no
  function-table dump. Their reach evidence is the runtime gate alone
  (0 lines).
