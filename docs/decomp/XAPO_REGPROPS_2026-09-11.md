# The XAPO `m_regProps` dynamic initialisers (lane W5-B, 2026-09-11)

**Branch** `w5-xapo-regprops` (worktree `~/tmp/wt-w5-b`, base `85af6249`, rebased onto
main before landing; `a8fdfd65` verified as an ancestor before any edit).
Follows lane W4-D's `VOICE_BODIES_2026-09-11.md` §4, which found the cluster and
handed it off, and lane W3-D's `XAUDIO_LAYOUT_AUDIT_2026-09-11.md` §1.11, which had
declared it out of reach.

Every retail figure was read off `build/45410914/asm/*.s` keyed on the `.fn fn_<addr>`
symbol (never the synthetic address column — this cluster is a textbook case: the
`.s` renders `fn_82C42DA0`'s body at `82B67098`), or out of `orig/45410914/band.exe`
through the PE section table in Python (`grep` is binary-blind here). Percentages are
the graded ruler (`name_check`) from `report.json`.

**Standing correction, same as W3-D's and W4-D's:** `native/CMakeLists.txt` excludes
all of `synth_xbox/`, so none of this pays as native impact. It pays in X360 bytes and
in accuracy — and three of the findings below are outright bugs in this tree's source.

---

## 1. Three corrections to the brief, all found by testing it literally

| briefed | measured |
|---|---|
| "twelve 144-byte rows" | **THIRTEEN.** `band.exe` holds **13** `L"SampleAPO"` objects in `.data` and, independently, **13** `.?AV?$CSampleXAPOBase@…` RTTI strings in `.rdata`. The extra one is **SynapseAPO**, whose `??__E` sits at `0x82C436F0` — *outside* W4-D's block, pinned to SpotlightDrawer.cpp. |
| "every anonymous row there ends in `bl atexit`" | **28 of 41 do; the twelve regProps rows do NOT.** They end in `blr` with no destructor registration — which is itself the tell that they are aggregate initialisers, not object constructions. |
| "the `memcpy 0x50` source is the registration blob — its strings and CLSID bytes identify the effect" | **All thirteen copy from the SAME source, `lbl_82194C68`.** It is the shared `L"Copyright (C)2008 Microsoft Corporation"` CopyrightInfo literal and identifies nothing. The discriminator is the **statically initialised `obj+0x00..0x23`** — CLSID plus the first 10 WCHARs of FriendlyName. |
| "adding a pin over previously-unpinned `auto_*` code is metric-neutral … (no A/B needed for that half)" | **Inapplicable.** The block was **already pinned**, whole, to `Voice.cpp` (`splits.txt` line 7127). Everything here is a **RE-HOME**, which is explicitly *not* neutral, so every change was measured. |
| W3-D: "RB3's map has **no** `??__E?m_regProps@…` initialisers … so this is not the same situation as DC3" | **It is exactly DC3's situation.** The initialisers exist in retail; the *map* lacked names for them. "Absent from the map" was read as "absent from the binary". |

## 2. Why the split exists at all (the mechanism)

`XAPO_REGISTRATION_PROPERTIES` is `0x42c`: `clsid` 0x0, `FriendlyName[256]` 0x10,
`CopyrightInfo[256]` 0x210, then seven `UINT` at 0x410..0x428. Every one of the
thirteen bodies is the same 36 instructions:

```
memset(obj + 0x24,  0, 0x1ec)     ; rest of FriendlyName
memcpy(obj + 0x210, lbl_82194C68, 0x50)   ; L"Copyright (C)2008 Microsoft Corporation"
memset(obj + 0x260, 0, 0x1b0)     ; rest of CopyrightInfo
obj->MajorVersion=1  MinorVersion=0  Flags=0x3f
obj->Min/MaxInput = Min/MaxOutput = 1
```

The memset starts at **+0x24**, not +0x10, because `0x00..0x23` — the CLSID and
`L"SampleAPO"` (9 chars + NUL = exactly 0x14 bytes) — is folded into `.data` at
compile time. That is the signature of `__uuidof`: **MSVC constant-folds an aggregate
initialiser up to the first element it cannot fold**, emits the folded prefix
statically, and generates a `.text$yc` dynamic initialiser for the remainder. A
literal GUID folds the whole `0x42c` block and emits **no `??__E` at all**. DC3
recorded this in `xdk/xaudio2/xapobase.h`; our copy of that header carried the
question instead (`// TODO: how am i supposed to instantiate this if every Effect
has a different guid?`, line 103) and no definition.

## 3. The thirteen identifications

Each row is identified **three independent ways that all agree**: (a) the CLSID bytes
at the `.data` object it writes, (b) DC3's `__declspec(uuid)` on the same effect
class, (c) the map name of the `CSampleXAPOBase<T>` ctor that passes that same object
to `CXAPOParametersBase`. A fourth, found later, confirms every template-argument
spelling: retail's own `.rdata` RTTI strings.

| # | `??__E` | regProps obj | retail CLSID | effect | ctor evidence (map) | owning unit |
|---|---|---|---|---|---|---|
| 1 | `0x82C42DA0` | `0x82CA4D40` | `48DD642E-DD9D-4ED0-81AA-0BC5F0A13C40` | CompressionEffect | `??0?$CSampleXAPOBase@VCompressionEffect@@UParams@1@@ATG@@` @ `0x82B5BCA8` | `system/synth_xbox/Synth.cpp` |
| 2 | `0x82C42E30` | `0x82CA5248` | `A46688F1-A161-452F-AF1C-3E6380456BDA` | DistortionEffect | @ `0x82B61910` | `system/synth_xbox/FxSendDistortion.cpp` |
| 3 | `0x82C42EC0` | `0x82CA5708` | `24BE678A-C537-4C1C-A82F-164CFB06E7A6` | DelayEffect | @ `0x82B620E0` | `system/synth_xbox/FxSendDelay.cpp` |
| 4 | `0x82C42F50` | `0x82CA5BC8` | `443A5BB5-2BD8-45FE-ACE8-3B512D6CBE68` | **Flanger**Effect | @ `0x82B62968` | `system/synth_xbox/FxSendChorus.cpp` |
| 5 | `0x82C42FE0` | `0x82CA6098` | `0E0F3600-B28E-4434-810D-21B8BE740619` | EQEffect | @ `0x82B635A0` | `system/synth_xbox/FxSendEQ.cpp` |
| 6 | `0x82C43070` | `0x82CA6590` | `20F3EB49-FF6B-41A0-8A09-3531A3501AE1` | WahEffect | @ `0x82B64818` | `FxSendWah.cpp` |
| 7 | `0x82C43390` | `0x82CA6B48` | `B4D4C8AA-…-64193551A9CC` | MeterEffect | @ `0x82B6B518` | `MeterEffect.cpp` |
| 8 | `0x82C43420` | `0x82CA6FF0` | `B4D4C8AA-…-64193551A9BE` | HeadsetXferEffect | @ `0x82B6B898` | `HeadsetXferEffect.cpp` |
| 9 | `0x82C434B0` | `0x82CA74C8` | `B4D4C8AA-…-64193551A9BC` | GainEffect | @ `0x82B6CBA0` | `GainEffect.cpp` |
| 10 | `0x82C43540` | `0x82CA7978` | `B4D4C8AA-…-64193551A9BF` | HeadsetPlaybackEffect | @ `0x82B6CE30` | `HeadsetPlaybackEffect.cpp` |
| 11 | `0x82C435D0` | `0x82CA7E20` | `B4D4C8AA-…-64193551A9BC` | EnvelopeGenerator | @ `0x82B6D070` | `EnvelopeGenerator.cpp` |
| 12 | `0x82C43660` | `0x82CA82C8` | `B4D4C8AA-…-64193551A9BD` | PitchShiftEffect | @ `0x82B6D718` | `PitchShiftEffect.cpp` |
| **13** | `0x82C436F0` | `0x82CA8768` | `03004D97-D165-4CC0-ABDD-6A98F04E6EB7` | **DSP::SynapseAPO** | **none — ctor is anonymous** (§6) | `SynapseAPO.cpp` |

Note #9 and #11 **genuinely share a CLSID** (`…A9BC`). That is not a misread: DC3
recovered the same collision independently from its own binary. Harmonix copy-paste,
preserved in both games.

The thirteen RTTI strings in retail `.rdata`, in `.data` order, which pin every
template-argument spelling used in the map edits below:

```
.?AV?$CSampleXAPOBase@VCompressionEffect@@UParams@1@@ATG@@        (…and Distortion,
Delay, Flanger, EQ, Wah — all `UParams@1@`, i.e. a nested T::Params)
.?AV?$CSampleXAPOBase@VMeterEffect@@UMeterEffectParams@@@ATG@@     (…and HeadsetXfer,
Gain, HeadsetPlayback, EnvelopeGenerator, PitchShift — free Params structs)
.?AV?$CSampleXAPOBase@VSynapseAPO@DSP@@USynapseAPOParams@2@@ATG@@
```

`.?AVCompressionEffect` etc. have **0** RTTI hits — those six are the non-polymorphic
inner DSP effect classes reached through `StandardEffect<T>`; only the six
`CSampleXAPOBase`-derived effect classes get their own type names. That asymmetry is a
consistency check on the table, not an anomaly.

## 4. What was reproducible

All thirteen. `__uuidof(Effect)` on a template parameter, inside a static-data-member
initialiser of a class template, **compiles under cl 10224** — this was the one
construct the brief flagged as possibly unreproducible, and it is not. Our emitted
`.text$yc` body is `0x90` bytes and **instruction-for-instruction identical to
retail's**: same 36 opcodes, same register assignment, same order; only the four
relocated operand fields differ.

Those four relocations are also why the rows land at `fuzzy == 100` rather than at
`mpn == 100` with the bytes withheld. Under `name_check` a relocation's target *name*
is compared — but objdiff **forgives placeholder targets**, and all four of retail's
are placeholders: `lbl_82CAxxxx` (the regProps object), `fn_8282AE30` (memset),
`fn_8282A900` (memcpy), `lbl_82194C68` (the copyright literal). Zero charged sites.

## 5. Per-change predicted vs measured

Each run: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w5-b --from-dirty`, one
change per run, committed between, both legs settled and at a split fixed point.

| run | change | predicted | measured |
|---|---|---|---|
| **A** `b8952c16` | `m_regProps` defined on the primary template with `__uuidof(Effect)`; `__declspec(uuid)` on thirteen effect classes; four stale uninitialised definitions removed | **+0 fns / +0 B / +0.000000 pp** — the new `??__E` symbols have no counterpart in any base obj yet, and no existing symbol's codegen changes | **+0 / +0 B / +0.000000 pp** exactly (42627 / 37.552258 / fuzzy 49.034412 on both legs), with **25 live recompiles** in leg B so it is not absent-vs-absent |
| **B** `89bac4a8` | splits: carve the twelve `0x90` extents out of Voice.cpp into their owning units; map: twelve names | **+12 fns / +1,728 B / +0.016865 pp** | **+12 / +1,728 B / +0.016864 pp** (42627/37.552258 → 42639/37.569122); unit net **+12** across exactly the twelve owning units |
| **C** `37e63afc` | SynapseAPO unified onto the shared template (+uuid), pin+name the 13th row | **+1 fn / +144 B / +0.001405 pp** | **+1 / +144 B / +0.001408 pp** (→ 42640/37.570530); unit net +1 on `default/SynapseAPO` |

Lane total (deltas compose): **+13 matched functions / +1,872 B / +0.018272 pp** over
`85af6249` (42,627 / 37.552258 → 42,640 / 37.570530). `masked_equal` **unmoved** at
22,928 throughout, so all +13 are honest.

`total_code` read from `report.json` at measurement time: **10,245,956**. Not inherited
from any prior doc — and note it differs from every figure in CLAUDE.md's history.

## 6. The naming half had no downside channel, and that was checked, not assumed

The standing hazard is that naming a previously-anonymous address converts every
caller's **forgiven** placeholder call site into a **checked** one. Here the caller
population is **empty**: none of the thirteen addresses is the target of a single `bl`
anywhere in the asm tree. Their only references are `.4byte` entries in the CRT
initialiser table, which lives in `.data` inside the unpinned `auto_06_82C64400_data`
unit and is not scored. So the only live channel was *pairing*, which is pure upside —
predicted, then measured that way.

The `dynamic_init` post-compile patcher (`??__E` `STATIC`→`EXTERNAL`) is what makes
this work at all: our `??__E` COMDATs are static, the renamed target symbols are
global, and objdiff pairs by name across that storage-class gap only because the
patcher closes it. It needed no change — it already covered these symbols.

The one place a name was **derived rather than corroborated** is #13. Unlike the
twelve, SynapseAPO has no map-named ctor, so its `??__E` spelling rests on the RTTI
string quoted in §3 plus our own namespace nesting. Said out loud because it is a
weaker footing than the other twelve, even though it is still retail evidence.

## 7. Three real bugs this exposed in our source

1. **`SynapseAPO.h` declared a SECOND `ATG::CSampleXAPOBase`** — a different template
   with a different `DoProcess` signature — alongside private copies of `CXAPOBase`,
   `IXAPOParameters` and `CXAPOParametersBase`. Retail's RTTI proves there is exactly
   **one** such template. Unified.
2. **`SynapseAPO.cpp` defined `struct XAPO_REGISTRATION_PROPERTIES { char data[0x58]; }`**
   — a local stub of the **wrong size**; the real struct is `0x42c`. Deleted.
3. **`DSP::SynapseAPO::DoProcess` had the wrong signature.** Retail's mangled name
   `?DoProcess@SynapseAPO@DSP@@UAAXABUSynapseAPOParams@2@PIAMII@Z` decodes to
   `(const SynapseAPOParams &, float *__restrict, unsigned, unsigned)`; we declared
   `(const Params &, unsigned int *, float &, unsigned, unsigned)`, which mangles
   differently and therefore **paired with nothing** (a flat 0%, indistinguishable
   from "not written yet"). Corrected; the row now pairs at 16.7%. The body is still
   an empty stub, so it buys no bytes — see §8.

Separately, four TUs used to define `m_regProps` **without an initialiser**, which
emitted a zeroed `.bss` block and no initialiser at all. Every effect would have
registered with a null CLSID, an empty name, version 0.0, no flags and a buffer-count
range of `[0,0]` — non-functional. The other nine defined it nowhere.

## 8. What this lane did NOT do

* **`BitCrushEffect`'s uuid is inherited from DC3 and is UNVERIFIABLE here.** Retail
  has **no** registration block for it — 13 `L"SampleAPO"` objects, none its own — so
  `CSampleXAPOBase<BitCrushEffect>` is never instantiated in retail at all. The
  attribute exists only because our `FxSendBitCrush360::CreateFx` instantiates
  `StandardEffect<BitCrushEffect>` and `__uuidof` demands one. Its value is
  metric-irrelevant, and it is labelled as such in `synth/BitCrushEffect.h`. **The
  useful signal is the other direction:** retail emitting nothing suggests retail's
  `FxSendBitCrush360::CreateFx` differs from ours. Not chased.
* **`?DoProcess@SynapseAPO@DSP@@…` body** (24 B, now 16.7%) — the signature is fixed
  and it pairs; the body is `{}`. Writing it needs the Synapse DSP path, not this lane.
* **`??0?$CSampleXAPOBase@VSynapseAPO@…` (124 B, 78.7%)** and
  **`??0SynapseAPO@DSP@@ (120 B, 93.3%)`** — left where they were. The unification did
  **not** move them, which is worth recording as a negative: the shared template's
  ctor is not, by itself, the reason they miss.
* **The 29 remaining `.text$yc` rows in Voice.cpp's two retained blocks**
  (`0x82C42958–0x82C42DA0`, `0x82C43100–0x82C43390`). These are ordinary dynamic
  initialisers — 28 of them do end in `bl atexit` — for globals spread over
  `0x82E11C98..0x82E1215C`, i.e. other `synth_xbox` TUs' file-scope objects. Each
  needs its owning TU identified before it can be re-homed the way the thirteen were;
  the `lbl_` each one writes is the handle. A concrete starting map is in the lane
  transcript. Four of them (`0x82C43340/50/60/70`, 16 B each) are 4-instruction
  bodies with no data reference at all.
* **Did not pin the `.data` side.** The thirteen `0x42c` regProps objects at
  `0x82CA4D40..0x82CA8768` still live in the unpinned `auto_06_82C64400_data` unit.
  Pinning `.data` is a separate, manual job (CLAUDE.md: non-`.text` sections are not
  auto-derived) and would be worth its own lane — it is 13 × 0x42c ≈ 13.4 kB.
* **Did not run the permuter.** Every change here is a retail-bytes correction.
