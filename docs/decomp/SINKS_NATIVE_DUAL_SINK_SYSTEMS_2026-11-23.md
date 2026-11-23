# Native dual sink systems: every Object-level call site, checked against retail (2026-11-23)

Lane `sinks`, worktree `~/tmp/wt-sinks`, base `56393ad26`. Brief: native `Hmx::Object`
carries DC3's Object-level sinks (`mSinks` / `GetOrAddSinks` / `ChainSource` / `MergeSinks`
/ the property sinks, plus a native `Hmx::Object::Export` body) alongside RB3's `MsgSource`
sinks. `NATIVE_ARM_DIVERGENCE_SWEEP_2026-11-22.md` ("Two sink systems") warns that anything
registered on the Object-level system cannot hear `MsgSource::Export`. Goal: find every
native-compiled site that registers, exports or dispatches through the wrong system compared
with retail, and fix the ones that change native behaviour.

**Result: 0 sites to fix.** Every native site that reaches the Object-level system is in code
that retail does not have (DC3 Flow, the DC3 `Automator`, dev-only highlight/display code,
DC3's `MsgSinks` internals, a native test). Every retail `MsgSource::AddSink` /
`RemoveSink` / `ChainSource` / `MergeSinks` caller has a native counterpart that already
binds to `MsgSource`. The three cross-over sites found earlier were fixed before this lane
(`SharedGroup::TryEnter` in `51082c8f4` and then `dcded50ff`, `RndDir::ChainSourceSubdir` in
`0aea04c47`, `FileMerger::MergeAction`). This lane makes no source change, so there was
nothing to price with `ab_measure` and nothing to run through `no610m.sh`.

## Retail ground truth

- Retail has no Object-level sink functions. The map names `?AddSink@MsgSource@@`
  (0x82767BA0), `?RemoveSink@MsgSource@@` (0x82766970), `?MergeSinks@MsgSource@@`
  (0x82767D08), `?ChainSource@MsgSource@@` (0x82768250) and the `MsgSource` `Export` thunks.
  It names no `@Object@Hmx@@` sink function.
- Retail `Hmx::Object::Export` (vtable slot 14) is the shared empty `blr` at 0x826C3888.
  I re-read it in the `AuditionSessionBuilder` table below (slot 14 = 0x826C3888). The
  native body `if (b) HandleType(a); if (mSinks) mSinks->Export(a);` is DC3's.
- A `bl` scan over retail `.text` (`~/tmp/sinks/blscan.py`, owners resolved by `.pdata`,
  `~/tmp/sinks/owners.py`) finds these callers:
  - `MsgSource::ChainSource`: two, `RndDir::ChainSourceSubdir` (0x82403810) and
    `SharedGroup::TryEnter` (0x824EA9A4).
  - `MsgSource::MergeSinks`: one, `FileMerger::MergeAction` (0x82390DEC).
  - `AddSink` and `RemoveSink`: 284 calls (157 and 127) in 132 functions.

## Method

The instrument is the compiler's own overload resolution. A modified copy of `Object.h`
puts `__attribute__((deprecated("OBJSINK:<name>")))` on these members:

- `AddSink`, `RemoveSink`, `AddPropertySink`, `RemovePropertySink`, `HasPropertySink`,
  `MergeSinks`, `ChainSource`, `GetOrAddSinks`, `Sinks`, `BroadcastPropertyChange` (both
  overloads), `ExportPropertyChange`, `OnAddSink`, `OnRemoveSink`;
- the `mSinks` field;
- the native `Hmx::Object::Export`.

Clang loads the copy through `-ivfsoverlay`, so the worktree file is never touched. Every
native TU then gets `clang++ -fsyntax-only -Wdeprecated-declarations`, using its own command
from a freshly configured `native/` compile database (`~/tmp/sinks/nb`). A warning fires
exactly where name lookup binds to the Object-level member. A call through a `MsgSource`
static type binds to `MsgSource::AddSink` and so on, and stays silent.

- **Coverage.** There were 2,091 distinct (file, `-D` set) compile configurations. Native
  targets compile 666 files with different define sets (`RB3_NATIVE_APP`,
  `RB3_ENGINE_RENDER`, ...), so a first-command-per-file pass covering 1,019 files was not
  enough. Results: 0 failed, 41 unique warnings. The 1,019-file passes found the same site
  list, so the extra configurations added nothing.
- **The instrument discriminates.** It fires on known Object-typed calls (`UI.cpp:1341`
  `obj->AddSink` through an `Hmx::Object *`, and the Flow sites). It stays silent on
  `Instance.cpp:638` and `Dir.cpp:264`, which name `ChainSource` but bind to `MsgSource`.
- **Blind spot.** Clang suppresses the warning inside a member that is itself tagged. That
  only hides calls inside `Object.cpp`'s own sink members (for example `AddSink` →
  `GetOrAddSinks`), and those were read by hand. A second pass tagged only the `mSinks`
  field. Its only hits outside tagged members are in `Object.cpp`: the dtor, `Replace`,
  `SyncProperty` and `SetProperty`. No other TU touches `mSinks` directly.
- **Reverse direction (retail calls with no native counterpart).**
  `~/tmp/sinks/xref.py` matches all 132 retail caller functions by name to their source
  bodies. In 131, our body has at least as many `AddSink(`/`RemoveSink(` calls as retail
  makes. The last one has no name in the map: 0x825EBFE0 / 0x825EC118, which RTTI
  identifies as `AuditionSessionBuilder`'s dtor and 5-argument ctor (vtable 0x820B954C,
  COL → `.?AVAuditionSessionBuilder@@`). It has no native body at all: it is a link stub in
  `native/src/rb3_app_link_stubs.s`. So it is a missing body, not a wrong-system site.
- **Script-driven registration.** `{obj add_sink ...}` only reaches the right system if the
  object is a `MsgSource` in native whenever it is one in retail.
  `~/tmp/sinks/rtti_msgsource.py` walks retail RTTI (type descriptor → base class
  descriptors → class hierarchy descriptors → COLs) and finds 61 classes with `MsgSource`
  among their bases. A textual transitive closure over `src/`, `native/src` and
  `milo-native-engine/src` (`~/tmp/sinks/native_msgsource.py`) finds 81.
  - Retail `MsgSource` but not native: none.
  - Native extras: Ham*, Wii*, RV*, `QuickplayPerformer`, `VoiceChatMgr`,
    `ChunkPathAccess`, `NativeCrowdPlayer`/`NativeScorePlayer`. None has a type
    descriptor in band.exe.
  - `Hmx::Object`'s own `add_sink`/`remove_sink` handlers stay behind
    `RB3_KEEP_DC3_ONLY_HANDLERS`, which no native compile command defines. A non-`MsgSource`
    object therefore leaves `add_sink` unhandled, as in retail.
- **Script modes.** Retail C++ always passes mode 0 (`kHandle`) to `AddSink`: 153 of 157
  sites have `li r7,0`. The others take the mode from a register (`StoreMainPanel::Load`,
  `MsgSource::OnAddSink`) or are `MergeSinks`. Vanilla scripts (`rock-band-3-deluxe/_ark`,
  excluding `dx/`) use `kSinkModeExportType` twice. Both sinks are `MsgSource` classes
  (`GamePanel` in `ui/game.dta:132`, `WorldDir` in `world/world_objects.dta:1181`), so
  they reach `MsgSource::Export` on both builds. No `{export ...}` script call exists.

## Sites and verdicts

| # | Site (native-compiled) | Binds to | Retail counterpart | Verdict |
|---|---|---|---|---|
| 1 | `flow/FlowTrigger.cpp:227,231,235,268,271` `AddSink`/`RemoveSink`; `:242,249,256,277,283` property sinks | Object | none: no `FlowTrigger` / `PropertyEventListener` / `on_reflected_property_changed` string in band.exe | DC3-only; RB3 data has no Flow objects. Leave. |
| 2 | `flow/Flow.cpp:441-457` property sinks | Object | none (as 1) | DC3-only. Leave. |
| 3 | `flow/PropertyEventListener.cpp:17,32` | Object | none (as 1) | DC3-only. Leave. |
| 4 | `obj/Object.cpp:1062` `ExportPropertyChange` in native `SetProperty` | Object | none: retail `SetProperty` has no sink path | Fires only when a property sink exists, which only Flow adds. Inert for RB3 data. |
| 5 | `obj/Object.cpp:1031-1032,158,204-205,280` `mSinks` reads | Object | none: field absent from retail layout | Cleanup and propsync of a list nothing populates in RB3 code paths. Inert. `MsgSource::SyncProperty` handles `sinks` first for every `MsgSource`. |
| 6 | `ui/UI.cpp:1341` `Automator::AddMessageType`: `obj->AddSink(this, sym)` on an `Hmx::Object *` | Object | none: `add_message_type`, `Automator`, `auto_script` absent from band.exe | DC3 dev tool. It would be deaf on a `MsgSource` target, but it has no retail behaviour to match. Leave. |
| 7 | `char/CharClipDisplay.cpp:44` `FindSource`: `it->Sinks()` | Object | none: no `CharClipDisplay` string in band.exe | Dev display; never finds a `MsgSource`-registered sink. No retail behaviour. Leave. |
| 8 | `char/CharIKMidi.cpp:125` `DoDebugDraws` (from `CharIKMidi::Highlight`) | Object | none: `Highlight`'s `frac %.3f new:%s` is absent from band.exe | Dev highlight; same deafness as 7. Leave. |
| 9 | `obj/Msg.cpp:93` `MsgSinks::ChainEventSinks`, `:311` `MsgSinks::MergeSinks` | Object | none: DC3 `MsgSinks` | Reached only from `Hmx::Object::ChainSource` / `MergeSinks`, which have **zero** native callers (the scan would flag them). Dead. |
| 10 | `obj/Msg.cpp:220,226` `MsgSinks::Sink::Export` → `obj->Export` | Object (virtual) | none: DC3 `MsgSinks` | Dispatches virtually; only Object-level sinks reach it. Inert. |
| 11 | `obj/Msg.cpp:490,496` `MsgSource::Sink::Export` kExport/kExportType → `obj->Export(da, b)` | Object static, virtual dispatch | same call in retail `MsgSource::Sink::Export` | Matches retail for a `MsgSource` sink. For a non-`MsgSource` sink in mode 3, native runs `HandleType` where retail's empty `Export` does nothing; no retail C++ or vanilla script sets up that case (see Method). Leave. |
| 12 | `obj/DataFunc.cpp:1001` `DataExport`: `obj->Export(a, i)` | Object static, virtual dispatch | same shape | Same residual as 11, and no vanilla script calls `export`. Leave. |
| 13 | `rndobj/Dir.cpp:94` `RndDir::Export` → `mSubDirs[i]->Export(a, false)` | Object static, virtual dispatch | retail chains `MsgSource::Export`, then subdirs | `b == false` with null `mSinks` is a no-op on a non-`MsgSource` dir, as in retail. Equal. |
| 14 | `rndobj/Anim.cpp:201` `RndAnimatable::FireFlowLabel` → `Export(flow_label_fired, true)` | Object static | none: `flow_label_fired`, `fire_flow_label`, `flow_labels` absent from band.exe | Only caller is the native-only `RndPropAnim` `fire_flow_label` propsync (DC3 rev-15 field). Inert. |
| 15 | `synth/MeterEffectMonitor.cpp:53` `Export(channel_data, true)` | Object static | none: `MeterEffectMonitor`, `channel_data` absent from band.exe | DC3-only class. Leave. |
| 16 | `native/src/font_replace_phase.cpp:68,79` | Object | none (native gate A18) | Native test of `mSinks` ring cleanup in `RndFont::Replace`. Intentional. |
| — | `world/Instance.cpp:638` `SharedGroup::TryEnter` | MsgSource | 0x824EA9A4 → `MsgSource::ChainSource` | Already correct (`51082c8f4`, `dcded50ff`). |
| — | `rndobj/Dir.cpp:264` `RndDir::ChainSourceSubdir`, callers `:197,462,482` | MsgSource | 0x82403810 → `MsgSource::ChainSource` | Already correct (`0aea04c47`). Callers pass a `MsgSource *` cast to `Hmx::Object *` and the callee casts it back; no override exists in rb3 source. |
| — | `char/FileMerger.cpp:447` `MergeAction` | MsgSource | 0x82390DEC → `MsgSource::MergeSinks` | Already correct. |
| — | the 281 other retail AddSink/RemoveSink calls (131 functions) | MsgSource (silent in scan) | named | Correct. |

## Residual risk, not fixed

The one behaviour difference this lane can still name is the native DC3 body of
`Hmx::Object::Export`. Retail's is empty; native runs `HandleType(msg)` when `b` is true
and then exports to `mSinks`. It is reachable from RB3 code only when a non-`MsgSource`
object receives `Export(..., true)`, through a script `add_sink` in mode
`kSinkModeExport`/`kSinkModeExportType` or a script `{export ...}`. Neither retail C++ nor the
vanilla scripts set that up. Making it retail-empty would deafen the Object-level system that
gate A18 and the DC3-only features rely on, for no RB3-visible gain, so it was left.

## Not done

- No source change, so no `ab_measure` run and no native binary run. Per the brief,
  `native_health --selftest` was not run.
- `AuditionSessionBuilder`'s missing body (row "Reverse direction") is outside this lane.
- The `rock-band-3-deluxe/_ark` script search is a sample of vanilla script (the
  Deluxe-modified tree minus `dx/`), not the retail ark itself.
- The scan scripts live in `~/tmp/sinks/` and are not committed.
