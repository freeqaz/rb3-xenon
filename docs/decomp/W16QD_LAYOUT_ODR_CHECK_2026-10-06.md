# W16-QD: cross-TU class-layout ODR check (2026-10-06)

Lane W16-PZ (`W16PZ_BANDOBJ_BAND3_LAYOUT_AUDIT_2026-10-06.md` §7) found four
classes compiled with different layouts in different TUs. It did that with a
hand-run check over the 61 bandobj/band3 TUs the native targets link. This lane
turned that check into a committed tool, `tools/layout_odr.py`. The tool covers
every class in every compiled TU of both builds. This lane also fixed the splits
the tool found and wired the tool into `tools/native_build_gate.sh`, and through
the gate into CI, so a new split fails a check.

## 1. What it checks and why nothing else could

A class laid out differently in two TUs of one program is an ODR split: which
layout a function sees depends on which TU it was compiled in. The X360 match
build never links. The native link does not compare class layouts (clang has no
ODR checker without modules or LTO). So a green match build and a green native
link both say nothing about this defect class.

| domain | TUs | layout source | ODR domain |
|---|---|---|---|
| `x360` | every `msvc`/`msvc_pch` edge in `build.ninja` (1,264 compiles) | `cl.exe /d1reportAllClassLayout` on the build's own command (objcache prefix stripped, `/Y-`, no `/Yu /Yc /Fp`, `/w`, scratch `/Fo`, `/showIncludes` kept) | the whole binary |
| `native` | every distinct clang compile linked into a native executable (1,711) | `clang -fsyntax-only -Xclang -fdump-record-layouts-complete -H` | **one per executable** (`ninja -t inputs <exe>`); ~450 of 551 sources compile with different flags per target, so comparing across targets would report differences that never meet in one program |

The fingerprint is the compiler's own text for the class, normalized. Equal text
means equal layout. Unequal text means a difference in size, a member's offset,
name or type, a base, a vftable slot or a `this` adjustor.

**Verdicts** (per printed name with more than one layout):

- **SPLIT**: one class identity has two layouts in one program.
- **UNRESOLVED**: on x360, a layout that cannot be tied to one source definition.
- **COLLISION**: different classes with the same bare name. This passes.
- **TEMPLATE**: template-scoped nested classes. These pass and are counted.

MSVC prints namespaced classes by bare name (`Quazal::Station` prints as
`Station`). The x360 classifier therefore ties each layout to its source
definition before it calls anything a SPLIT. See `classify_x360`.

**Exit codes** (`check`):

| rc | verdict | meaning |
|---|---|---|
| 0 | PASS | every TU answered and every SPLIT/UNRESOLVED is allowlisted |
| 1 | FAIL | an unexplained SPLIT or UNRESOLVED (with `--strict`, a stale allowlist entry also fails) |
| 2 | UNRUNNABLE | no `build.ninja`, or no native build |
| 3 | UNANSWERED | a TU failed or timed out, or the TU count is below the allowlist floor |

Precedence is 1 > 3 > 2 > 0. The last line of every run is `LAYOUT_ODR_RESULT
verdict=... rc=N`.

### Usage

```
python3 tools/layout_odr.py check --domain all          # what the gate runs
python3 tools/layout_odr.py --native-build <dir> check --domain native
python3 tools/layout_odr.py show --domain x360 'PracticeSection'   # both layouts, diffed
python3 tools/layout_odr.py check --domain all --allow-template      # JSON stubs for new entries
python3 tools/layout_odr.py selftest            # 20 offline legs
python3 tools/layout_odr.py selftest --live     # + real-toolchain fixture legs
```

### Allowlist: `config/45410914/layout_odr_allow.json`

- **PIN** `{domain, name, verdict, fps, reason}` accepts exactly those layout
  fingerprints. If any pinned layout changes, or a new layout of that name
  appears, the entry re-fires. Unrelated same-named classes are not pinned.
- **LIST** `{domain, verdict, names, identity_regex, reason}` accepts a reviewed
  family whose layouts are expected to churn. A name not on the list still
  fails. So does a listed name whose split identity `identity_regex` does not
  match (`re.match`). The regex is what keeps an RB3 class named `Station` out
  of the Quazal family. A listed name that no longer splits is reported stale,
  per name.
- `min_tu_compiles` sets a per-domain floor. A run that saw fewer TU compiles is
  UNANSWERED, not PASS. This is what makes a CI run without the engine honest.

### Gate wiring

`tools/native_build_gate.sh` runs `layout_odr.py check` after a successful
native build, with `NATIVE_GATE_LAYOUT_ODR=all|native|off` (default `all`) and
`NATIVE_GATE_LAYOUT_JOBS` (default `nproc`). The gate passes its own build dir
with `--native-build`.

- A layout FAIL is a gate FAIL (rc=1).
- A skipped or unanswered domain is reported SKIPPED and makes the gate
  INCOMPLETE (rc=3). It is never read as full coverage.
- CI (`.github/workflows/build.yml`, step "Native link gate") already runs the
  gate, so CI now runs the check too. A CI run with no engine falls below the
  native floor and shows as INCOMPLETE, which `NATIVE_GATE_ALLOW_INCOMPLETE=1`
  forces to rc=0. A real split still fails, because the FAIL branch runs first.
- The layout log is `<gate log>.layout_odr.log`.

### Cache and cost

The cache lives at `~/.cache/rb3-layout-odr` (override: `RB3_LAYOUT_ODR_CACHE`).

- The key is the tool version per domain, the normalized argv, and the TU.
- An entry is served only if every file the layout compile itself reported
  (`/showIncludes`, `-H`) still hashes the same.
- The project root is written as `@ROOT@`, so worktrees share entries.
- A result is not cached if any of its dependencies changed after the compile
  started (mtime at or after start minus 1 s). Editing a header in the middle of
  a sweep therefore cannot bind an old layout to a new hash.

| run | measured |
|---|---|
| x360, cold, -j16, fleet load | 1,262 TUs in 2,081 s (34.7 min); 915 TUs in 1,336 s |
| x360, after an `Object.h`-wide change | ≈ the cold figure (every TU re-lays-out) |
| x360, warm, 1 TU changed | 2.4 s |
| x360 + native, warm, nothing changed (inside the gate) | seconds |
| x360, 112 TUs changed (the W16-PZ revert control) | 147 s |
| native, cold, -j16 | 1,711 TUs in 137 s |
| native, warm | 0–3 s |

### Parse traps

Each of these was measured on real output. Each produced false splits until it
was handled, and each has a selftest leg.

1. MSVC interleaves `/showIncludes` notes into the report **mid-line**. Notes are
   cut out as whole substrings, which splices the line back together (A1).
2. `/w` does not silence the **driver**. In the Quazal `/Od` TUs, `cl : Command
   line warning D9025` lands inside whichever block is printing. 47 TUs' copies
   of `_RTL_CRITICAL_SECTION` read as a second layout until this was handled
   (A4). The first full sweep had 22 UNRESOLVED; most were this.
3. Bitfield rows (`| mForceLod (bitstart=29,nbits=3)`) are kept as text, so no
   row parser can get them wrong.
4. clang prints a C TU's trailer differently from a C++ TU's. It prints the
   class-key the TU spelled (`struct Hmx::Color` vs `class Hmx::Color`), and it
   prints anonymous declarations by include *spelling*. All three are
   normalized.
5. clang prints an empty class as `class HolmesInput (empty)`. Until that suffix
   was stripped (A5), it made **every member-less stand-in for a real class
   invisible on native**. That is the exact shape of W16-PZ's Band.cpp stubs.

### Controls

- **Offline selftest: 20/20.** It covers parser, classifier and allowlist
  semantics. Sabotage check: with the `identity_regex` test bypassed, D3 and D4
  go red.
- **Live native selftest: 25/25.** It compiles a fixture whose
  macro-gated-member shape must come out SPLIT. Two nested classes sharing a
  bare name, and anonymous-namespace classes in two TUs, must not.
- **Positive control on the real tree.** The W16-PZ fix (`5c1ec45ba`) was
  reverted in a scratch worktree and `check --domain x360` was run. Predicted: a
  FAIL naming MetaPerformer and the Band.cpp/CameraTilt stubs. Measured:
  ```
  LAYOUT_ODR_RESULT verdict=FAIL x360_tus=1264 x360_failed=0 x360_split=6 x360_unresolved=0 x360_allowed=137 x360_stale=1 rc=1
  ```
  - SPLIT `DialogDisplay`, `InstrumentDifficultyDisplay`, `MicInputArrow` and
    `PlayerDiffIcon`, each in BandCharacter.cpp vs its own TU.
  - SPLIT `ScrollbarDisplay`, with three layouts: BandCharacter.cpp,
    ScrollbarDisplay.cpp and CameraTilt.cpp.
  - SPLIT `MetaPerformer`, with a third fingerprint across 109 TUs. The PIN
    accepting the DC3/RB3 pair re-fired and was reported stale.

  This is every W16-PZ finding, on the whole binary rather than 61 TUs.

## 2. Every split found, and its resolution

### X360: before 147 SPLIT + 1 UNRESOLVED, after 137 SPLIT + 1 UNRESOLVED, all allowlisted

Both are full sweeps, 1,264/1,264 TUs answered, measured before the rebase onto
main. Before = the pre-fix commit `a0efd6d32` (now `e039180b9`); after =
`00b156606` (the last source fix). After the rebase, the gate's check
reproduced the after state exactly: 138 allowed (137 SPLIT + 1 UNRESOLVED),
0 unexplained, 0 stale (§3).

**Fixed (10).** Each one is gone from the after-sweep. `PitchDetector` remains
only as a COLLISION, because `DSP::Synapse::PitchDetector` and RB3's own
`PitchDetector` are two different classes.

| class | layouts (TUs) | defect | resolution | commit |
|---|---|---|---|---|
| `HolmesInput` | HolmesClient.cpp (1) vs HolmesKeyboard.cpp (1) | local member-less stand-in (1 byte) while HolmesKeyboard addresses its streams | include `os/HolmesKeyboard.h`; `gInput(nullptr)` as the real ctor takes | `ae1efefb8` |
| `DxRnd` | LiveCameraInput.cpp + HamCamTransform.cpp (scatter) (2) vs 19 | local one-method stub, 1 byte vs the NgRnd-derived class | include `rnddx9/Rnd.h` | `95d01192d` |
| `DSP::Synapse::PitchDetector`, `PeakDetector`, `GranularSynth` | Synapse_dsp.cpp vs the defining TUs | stand-ins with invented member names and padded sizes | include the real headers. Every access is mapped to the real member at the same offset (mapping in the source comment). `GranularSynth::SetVoiceEnabled` is declared in the header | `fe67e9fb8`, `72d487b6b` |
| `MusicLibraryTaskMsg` | Tour.cpp (1) vs Game/NetGameMsgs/… (5) | three local copies; two declared `virtual ~X() {}` | one `game/MusicLibraryTaskMsg.h`, in Tour.cpp's form (implicit dtor, which is what retail's `??1MusicLibraryTaskMsg` compiles to) | `a90e56c56` |
| `WiiFriendsProvider` | MetaPanel.cpp vs MusicLibrary.cpp | two local copies disagreed on a 4-byte filler | one `meta_band/WiiFriendsProvider.h` | `6f770425a` |
| `TourDescPanel` | MetaPanel.cpp vs TourDescPanel.cpp | partial declaration: right size, but none of the overrides, so a different vftable | `tour/TourDescPanel.h`. Size 0x84: `class_layout_report` gives `size(132)`, and retail `?NewObject@TourDescPanel` (`fn_82570A50`) does `li r3, 0x84` | `17e387f2e` |
| `JsonConverter` | RockCentral.cpp (1) vs 20 | `network/net/JsonUtils.h` (older parallel header) spelled the member `objects` | spelled `mObjects` as in `system/net/JsonUtils.h` | `8fff5cd17` |
| `BINK` | four layouts: Movie.cpp+StringTable.cpp, BinkReader.cpp, BinkIntegration.cpp, moviebink/* | four partial SDK structs, one with an invented virtual dtor | one `movie/BinkSdk.h` with the SDK field names up to `NumTracks` (0x38); BinkReader's `BinkError` is the SDK's `ReadError` (0x1c) | `7869d88ea` |

**Allowlisted (accepted, with reason in the JSON).**

| entry | what it is | why it is not fixed here |
|---|---|---|
| LIST, 132 names, `identity_regex ^(Quazal::\|\?\$.*@Quazal@@)` | 127 `Quazal::` identities + 5 Quazal template ids (`?$DORefTemplate@VStation@Quazal@@`, …). Each `src/network/quazal/**` TU carries its own mock-up of the vendor classes it touches | Quazal is out of scope (low value). A non-Quazal identity under any of these names still fails |
| PIN `MetaPerformer`, `Instarank`; PIN `EndGameMsg` (UNRESOLVED) | **two different classes**: DC3's `meta_ham/MetaPerformer.h` (13 engine TUs reach it through retail scatter-includes of `hamobj/*.cpp`) vs RB3's band3 classes. `EndGameMsg` is a `DECLARE_MESSAGE` expansion the tool cannot attribute | the scatter-includes place DC3-generation bodies at retail addresses; renaming DC3's classes renames symbols in matched objects |
| PIN `PracticeSection` | DC3 `hamobj/PracticeSection.h` (48 TUs) vs RB3 band3 (127 TUs) | same DC3-vs-RB3 generation clash |
| PIN `MovieInternalBuffers` | RB3 `movie/Movie.cpp` file-scope class (also in StringTable.cpp, which scatter-includes Movie.cpp) vs `moviebink/BinkMovieImpl.h` | the moviebink TUs are a DC3 Bink generation that is compiled but has no retail unit |
| PIN `in_addr` | Quazal `BerkeleySocketDriver.cpp` declares its own Winsock `in_addr` (1 TU) vs the XDK's (197) | Quazal, out of scope |

### Native: before (rb3 at the pre-fix commit) 20 SPLIT, after 15 on engine main, 1 with the engine branch

| class | programs / TUs | defect | resolution | commit |
|---|---|---|---|---|
| `HolmesInput` | as on X360 | as on X360 (this is the one trap 5 had hidden) | as on X360 | `ae1efefb8` |
| `BeatMatcher` | 8 programs | `native/src/beatmatch/BeatMatcher_native.h` defined a member-less `BeatMatcher` with inline no-op `PostLoad`/`AddTrack` next to the real class | No native program links BeatMatcher.cpp, so no BeatMatcher can exist natively. SongData leaves the type incomplete there and compiles out its three BeatMatcher loops; `AddBeatMatcher` MILO_FAILs if a target ever links one without `RB3_NATIVE_HAS_BEATMATCHER`. Shim deleted. X360 token stream unchanged | `d8657f1c1` |
| `MetaPerformer`, `Instarank`, `EndGameMsg` | rb3-render, rb3-milo | DC3 meta_ham classes entered an RB3 program through `world/CameraShot.cpp`'s scatter-include of `hamobj/DanceRemixer.cpp` (via `rndobj/TexBlender.cpp`'s chain) | scatter-include wrapped in `#ifndef HX_NATIVE`; X360 unchanged | `1706ed28e` |
| `GpuMeshData`, `GpuTexData` + 12 `std::` instantiations over them (14 names) | rb3-frame, rb3-render | **milo-native-engine**: `GpuResourceRegistry.h` redefines both structs, which `platform/MeshGpuCache.h` and `Tex_Wgpu.cpp` also define | engine branch **`w16-qd-engine` @ `2330fe7`** (on `cfef5a2`) nests the registry's structs as `GpuResourceRegistry::MeshEntry/TexEntry/CubeTexEntry`. Against a scratch build on that branch, rb3-frame and rb3-render link and the native sweep leaves only PracticeSection. Until the coordinator merges it and bumps `MILO_ENGINE_PIN`, a LIST entry (`identity_regex .*\bGpu(Mesh\|Tex)Data\b`) accepts the family; after the bump it reports stale and should be deleted | engine `2330fe7` |
| `PracticeSection` | 31 TUs (band3) vs 6 (DC3 hamobj, via `milo_link_stubs.cpp` → HamDirector.h, PostProc_NG/Morph scatter-includes) | DC3-vs-RB3 clash, as on X360 | PIN | — |

## 3. Results

A/B (`tools/ab_measure.py --patch`, diff `e039180b9..HEAD` over `src native`,
leg A = rebased pre-fix commit):

```
  leg A: matched=53608 masked=25185 honest=28423 code%=58.093216  (recompiles: 0, settled)
  leg B: matched=53608 masked=25185 honest=28423 code%=58.093216  (recompiles: 34, split=0, patch_steps=6, settle iterations: 2)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000000pp   (legA 63.767338 -> legB 63.767338)
  units at 100% [mpn ruler]: legA 571 -> legB 571  (Δ+0; 0 reached 100, 0 fell off; pairable units 1733->1733)
```

Per-row comparison of the two archived reports (`legA_report.json.gz` vs
`legB_report.json.gz`): 68,909 rows on both legs, none appearing or vanishing.
**0 rows down** on `fuzzy_match_percent` or `match_percent_normalized`, and 1 up:
`??0Synapse@0DSP@@QAA@M@Z` fuzzy 98.402466 → 98.42716 (mpn 98.6 both legs).

The flat aggregate is the expected result. These fixes change which definition
a TU sees, and retail compiled one definition, so they move codegen only where
a stand-in had been leaking into a body. An earlier A/B of the Synapse header
switch alone caught the one place that happened: `ProcessInPlace` 99.953705 →
98.11574, because the vector `operator[]` on the left of the store was evaluated
before the call. The temporary in `72d487b6b` restores it.

Layout check on the branch HEAD, both domains:

```
LAYOUT_ODR_RESULT verdict=PASS x360_tus=1265 x360_failed=0 x360_split=0 x360_unresolved=0 x360_allowed=138 x360_stale=0 native_tus=1711 native_failed=0 native_split=0 native_unresolved=0 native_allowed=15 native_stale=0 rc=0
```

(`x360_tus` is 1,265 here and 1,264 in §2, because the rebase onto main added one TU.)

Native gate and health:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=77 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

## 4. Blind spots (what a PASS does not prove)

- **Template-nested classes** (x360 TEMPLATE, 9 names) are counted, not
  compared. MSVC prints them by bare name with no owning instantiation.
- **One name defined more than once inside a single TU** is recorded in
  `meta.intra_tu_multiple` and not compared. That covers `_Guard` in
  DataArray.cpp, and imgui's SIMD helper structs natively.
- **Macro-defined classes** (`DECLARE_MESSAGE`, …) can come out UNRESOLVED on
  x360, because the defining line is a macro expansion. `EndGameMsg` is the one
  current case.
- **Only complete definitions are compared.** A TU that sees a class only as a
  forward declaration contributes no layout, which is correct: it cannot depend
  on one.
- Native coverage is what the native build links. A TU no native target links
  is checked only on x360.

## 5. Found along the way, out of scope for this lane

- **Class-key mismatches** (`struct Stats` vs `class Stats`, `Hmx::Color`) are
  layout-neutral and not reported as splits. MSVC still mangles the class-key
  into names, so a mismatch is a latent symbol-name difference.
- **Differences with no member change.** MusicLibraryTaskMsg's copies had the
  same members and size. They differed only in a user-declared virtual
  destructor, which shows in the vftable/adjustor rows. TourDescPanel likewise
  differed only in its vftable. A size-or-offset comparison would have passed
  both. The fingerprint caught them because it includes those rows.
- **`src/network/net/JsonUtils.h`** is still a near-duplicate of
  `system/net/JsonUtils.h`, and `GetObjectAsString` is declared there but
  defined nowhere. Folding the two headers is the real fix; this lane only
  aligned the member name.
- **`src/system/moviebink/*`** is a DC3 Bink generation with no retail unit.
  Native does not link it. It is the other half of the MovieInternalBuffers
  clash.
- **The DC3 hamobj/meta_ham generation is compiled into both builds** through
  scatter-includes and link stubs. Every PIN above except Quazal's exists
  because of it.
- **Native PracticeSection is a latent COMDAT hazard.** Both definitions emit a
  destructor with the same mangled name, so the linker keeps one and a TU can
  run the other class's destructor. No misbehaviour has been observed, and this
  lane did not investigate which destructor survives. The engine's GpuMeshData/GpuTexData case had the
  same shape (`std::unordered_map` node types over two different structs) and is
  fixed on `w16-qd-engine`.
