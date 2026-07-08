# Native port + DC3 engine reuse — the playable-RB3 track and milo-native-engine extraction

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: endgame

## Summary

A parallel endgame track that is orthogonal to byte-matching: run RB3 as a native
x86_64 Linux binary by reusing DC3's already-decompiled Milo engine and porting
only RB3's game layer (`src/band3/`). Today `native/` boots headless and parses
`songs.dta`; DC3's `milo-viewer` already renders RB3-360 `.milo_xbox` assets. This
RFC roadmaps the ladder from there to "one playable song," the
`milo-native-engine` extraction, and — critically — the **synergy** with matching:
every band3 TU ported for matching is reusable native source, and native linking
forces ports that become match candidates. Verdict: **PURSUE as a slow parallel
track, not a path-to-100% substitute** — it does not move `matched_functions`, but
it de-risks the game-layer port that matching also needs.

## Motivation

The "paths to 100%" doc set is about the strict byte-match metric (11,240 /
65,619 functions). This RFC is the **honest counterweight**: a large fraction of
that 100% is engine code that the project has already decided *not* to spend
matching effort on, because DC3 is the same engine on the same platform and
supplies it wholesale (CLAUDE.md "Decomp priority: the GAME, not the engine";
`docs/plans/engine-reuse-and-asset-rendering.md`). If the engine is effectively
pre-solved for *running the game*, then the highest-leverage artifact of the whole
project may not be a bootable matched XEX (`19-shiftable-relink-milestone.md`) but
a **playable native RB3** built from DC3's engine + RB3's game code.

The two tracks are not competitors. They share one scarce resource — a correct,
compilable `src/band3/` port — and each produces it for the other:

- **Matching → native:** porting a band3 TU to compile-and-match under MSVC-X360
  also makes it (or a near-clone of it) compile under Clang-LP64 for native.
- **Native → matching:** wiring a band3 TU into the native link line surfaces every
  undefined symbol and layout bug; fixing them yields a *compilable, semantically
  live* source file, which is exactly the precondition for a match attempt (a TU
  that does not compile can never be pinned or diffed).

This RFC exists so a cold agent can pick up the native track deliberately, knows
what is real vs aspirational, and can account for the synergy when sequencing the
band3 port (see `03-master-sequencing-roadmap.md`).

## Current state (verified)

Verified 2026-07-08 against the live repo (main @a1312de).

### Native build — what actually exists
- `native/` is a plain gitignored build dir (`native/build/` in `.gitignore`),
  **not** a submodule. Source tree: `native/src/{main_dta.cpp, platform/,
  stl/, *.cpp, dta_link_stubs.s}`, `native/include/`, `native/CMakeLists.txt`.
- The **only** executable target is `rb3-dta` (`native/CMakeLists.txt`
  `add_executable(rb3-dta …)`). It links **only** the engine subset
  `src/system/{obj,utl,os,math}/*.cpp` (globbed, minus `_(Xbox|Win|Wii|X360)` and
  GPU TUs), the flex DTA lexer `src/system/obj/DataFlex.c`, curated DC3 platform
  shims, and `dta_link_stubs.s`. **No `src/band3/`, no `src/network/`, no renderer,
  no audio.** Verified: `grep -iE "band3|network|gem|scoring" native/CMakeLists.txt`
  → zero hits.
- What it does (`native/src/main_dta.cpp`): `InitMakeString()` + `Symbol::Init()`,
  then `DataReadFile(argv[1])` and prints each song's id/name/artist. Per the
  native memory (`project_native_port.md`), verified on a real RB3 `songs.dta` →
  **138 songs** parsed, no GPU/audio.
- **`native/build/rb3-dta` is NOT currently built** — the build dir has stale
  `build.ninja`/`.ninja_log` from 2026-05-27 but the binary is absent (cleaned).
  Rebuild: `cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang
  -DCMAKE_CXX_COMPILER=clang++ && cmake --build build`. **[UNVERIFIED whether it
  still builds clean on current main]** — the engine `src/system` has changed
  under matching since May; a rebuild is the first action any native-track agent
  should take.
- Weak link stubs: `native/src/dta_link_stubs.s` defines **74** symbols
  (`grep -oE '^[a-zA-Z_][a-zA-Z0-9_]*:' | wc -l` = 74), matching the CLAUDE.md
  "~74 rendering/MIDI/synth/Win32 symbols" claim. They are `xorq %rax,%rax; ret`
  no-ops for functions and zeroed storage for data — none on the DTA path.

### Engine reuse — the "bigger play" doc
- `docs/plans/engine-reuse-and-asset-rendering.md` is real and flagged current in
  `docs/INDEX.md:111` (NOT stale-bannered). It documents a validated read-only
  experiment: DC3's `milo-viewer` (built at
  `../dc3-decomp/native/build/milo-viewer`, 127 MB, present) loads and renders
  RB3-360 `.milo_xbox` assets — a crowd character rendered textured (2 materials,
  4 textures), a tracksystem mesh dir loaded 130 meshes. Same-platform (Xbox 360)
  byte-identical texture tiling / vertex compression / endianness is why it works.
- The doc's own boundary is explicit and honest: **assets ≠ gameplay.** DC3's
  engine has no RB3 game logic; RB3 HUD/track dirs use classes DC3 lacks
  (`GemTrackDir` → "Can't make…", skipped). Rendering RB3 *scenes* ≠ *playing* RB3.

### milo-native-engine — the extraction target
- `../milo-native-engine` **exists** as a sibling repo with its own `CLAUDE.md`,
  `README.md`, `docs/`, `cmake/`, and ~15 `build-agent-*` dirs. It is a real,
  actively-developed shared runtime, not aspirational.
- Its model (`../milo-native-engine/CLAUDE.md`): a shared LP64 modern-C++ runtime
  that native ports of **three** decomps link against — dc3-decomp, rb3 (Wii),
  and rb3-xenon. Three-layer source model: (1) *matched fork* `<decomp>/src/system`
  compiles under the decomp's matched compiler, asm-match only, **not** on the
  native link line; (2) *engine runtime* `milo-native-engine/src/**` compiles under
  Clang LP64, the deliverable; (3) *per-decomp glue* `<decomp>/native/src/**`
  compat/SDK shims. The engine is **not standalone-compilable** — it builds inside
  a consumer's include/flag context via `MILO_ENGINE_DECOMP_INCLUDE_DIRS` /
  `_COMPAT_FLAGS` / `_PCH`.
- Canonical roadmap lives in a sibling: `../rb3/docs/native/NATIVE_PORT_ROADMAP.md`
  + `NATIVE_PORT_INVENTORY.md`; DC3's status at
  `../dc3-decomp/docs/native/NATIVE_PORT_STATUS.md`. **[UNVERIFIED — did not read
  these sibling-repo docs in this pass]**; a native-track agent must read them
  first, they are the authoritative plan.
- DC3's native is far ahead of ours: targets include `dc3-native`, `milo-viewer`,
  `render-test`, `milo2gltf`, `milo-tex-export`, `milo-mat-export`, `dc3-web`
  (`../dc3-decomp/native/CMakeLists.txt`). rb3-xenon has exactly one: `rb3-dta`.

### Game-code inventory (what a playable path needs)
- `src/band3/` = **154** `.cpp` files across `bandtrack/ game/ meta_band/ net_band/
  tour/`; `src/network/` = **9** `.cpp`. All 153 band3 entries in
  `config/45410914/objects.json` are `NonMatching` (they compile in the X360 build
  but none is byte-matched-complete). Gameplay-loop TUs present as source:
  `Game.cpp`, `Band.cpp`, `Track.cpp`, `GemTrack.cpp`, `VocalTrack.cpp`, `Gem.cpp`,
  `TrackPanel.cpp` (all in `src/band3/{game,bandtrack}/`). No `Scoring.cpp` (scoring
  lives inside other TUs — `AccuracyTracker.h`, `CrowdRating.h`, trackers in
  `game/`).
- Engine-vs-game cpp ratio: **753** `src/system` cpp vs **154** band3 + **9**
  network. The game layer is ~18% of the source-file count — matching CLAUDE.md's
  "value concentrates in the game code" framing.

### Refuted / corrected scout claims
See `refuted_scout_claims` in the schema block. Net: the brief is accurate; the
main corrections are (a) `native/build/rb3-dta` is not currently built, (b) band3
is *not yet* in the native build at all (the brief's "which band3 TUs, are they
ported yet?" answer is: **none are in the native link line** — they compile only
in the X360 matching build).

## Proposal

Treat native as a **milestone-laddered parallel track** with an explicit synergy
contract to the matching effort. Do not divert matching-fleet capacity to it;
run it as a low-frequency, high-owner-attention stream.

### Design principle: the shared band3 port
The single artifact both tracks need is a `src/band3/` that (a) compiles and (b)
is behaviorally faithful. Matching already forces (a)+(b) per-TU via the bodyport
skills (`bodyport-batch*`, `port-campaign`, `gameport1/2`). The native track
**consumes those same TUs** — with a Clang-LP64 compat shim instead of MSVC-X360.
So the sequencing rule is: **native never ports a band3 TU from scratch; it waits
for matching to produce a compilable one, then wires it into the native link line
and reports back any native-only breakage** (missing symbols, LP64 layout bugs) as
match-relevant bugs.

### Milestone ladder to "one playable song"
Anchored to the native memory's own ranking (`project_native_port.md` "Next steps")
and DC3's native as the template.

- **M0 — rebuild green (days).** Rebuild `rb3-dta` on current main; fix any drift
  from 5 months of engine matching. Deliverable: `rb3-dta songs.dta` parses again.
  This is the honesty gate — if M0 is red, everything below is blocked.
- **M1 — MIDI chart load (1–2 wks).** Add `src/system/midi/` + beatmatch to the
  native build; load a `.mid` chart into the engine's data structures headless.
  No band3 yet. Template: DC3 native already compiles the MIDI subsystem.
- **M2 — mogg audio decode (1–2 wks).** Decode a `.mogg` via libvorbis (DC3 uses
  miniaudio in `../dc3-decomp/native/src/audio/`). Headless audio-out or PCM dump.
- **M3 — asset render via milo-native-engine (2–4 wks).** Stand up a
  `rb3-native` executable that links `milo-native-engine` with
  `MILO_ENGINE_GPU_BACKEND=dc3` (per the engine-reuse doc's flavor switch) and
  renders an RB3-360 venue/character milo — i.e. reproduce the `milo-viewer`
  result inside *our* native target rather than borrowing DC3's binary. This is the
  first target that needs the milo-native-engine consumer wiring
  (`MILO_ENGINE_DECOMP_INCLUDE_DIRS` etc.).
- **M4 — first band3 TU on the native line (gated on matching).** Wire the
  smallest self-contained gameplay TU (candidate: `Gem.cpp` / `GemTrack.cpp` — leaf
  data structures) into `rb3-native`. Report every undefined symbol as a
  port-needed item back to the matching worklist. This is where the synergy becomes
  concrete and bidirectional.
- **M5 — the gameplay loop (months, ambitious).** Song select → chart playback →
  gem tracking → scoring → results. Needs `Game.cpp`, `Band.cpp`, `Track.cpp`,
  `VocalTrack.cpp`, the trackers in `game/`, and the classes DC3 lacks
  (`GemTrackDir` and friends — the "Can't make X" list from the engine-reuse doc).
  **This is the long pole and the honest ceiling of this RFC** — plausibly out of
  reach without dedicated multi-month effort.

### milo-native-engine extraction (parallel to the ladder)
Per CLAUDE.md architectural goal and `project_native_port.md`: the *shareable*
pieces of `native/src/` (platform shims, `msvc_compat.h`, STL/`__wrap_iter`
compat, CMake harness, `dta_link_stubs.s` regeneration tooling) should migrate into
`../milo-native-engine` as the per-decomp glue layer, leaving `native/` holding only
rb3-xenon-specific glue + the `add_subdirectory(../milo-native-engine)` wire-up. The
engine source (`src/system/**`) stays per-decomp (it is the match target) and is
pulled in as the "matched fork" layer — never copied. Concretely:
1. Read `../milo-native-engine/README.md` +
   `../rb3/docs/native/NATIVE_PORT_ROADMAP.md` for the current consumer contract.
2. Add rb3-xenon as a fourth consumer alongside dc3-decomp/rb3, injecting our
   include dirs + `-fms-compatibility` flags + (optionally) our PCH.
3. Replace `native/CMakeLists.txt`'s hand-rolled engine glob with the engine
   subdirectory + our glue.

### Rendering / audio / input strategy (replacing the 74 stubs)
The 74 `dta_link_stubs.s` no-ops are a DTA-path expedient; the playable path
replaces them by *compiling the real subsystems*:
- **Render:** `MILO_ENGINE_GPU_BACKEND=dc3` (Wgpu/Dawn/Vulkan), reusing DC3's
  `rnddx9→NgRnd` stack proven on RB3 assets. Do **not** port a renderer.
- **Audio:** miniaudio + libvorbis, DC3's `native/src/audio/` as template.
- **Input:** `native/src/platform/Joypad_Stub.cpp` → a real SDL/evdev joypad shim
  (this is per-decomp glue, small).
- Stubs shrink monotonically as subsystems come online; regenerate the residual set
  via the memory's recipe (link with `-Wl,--no-demangle`, grep undefined refs,
  classify `The*/g*/_ZT*` as data else function).

### Synergy accounting (make it a metric)
Add a line to the metrics dashboard (`18-metrics-and-dashboard.md`): **"band3 TUs
compilable-under-both-toolchains."** Every increment is a match candidate *and* a
native-link candidate. This turns the two tracks into one shared burndown and
prevents double-counting effort.

## Alternatives considered

- **Keep borrowing DC3's `milo-viewer` for assets; never build `rb3-native`.**
  Cheapest — asset inspection/glTF export is free today. But it never becomes
  RB3-playable and contributes nothing to the game-layer port that matching needs.
  Good as a *tool*, not as the track. (This is the status quo.)
- **Fold native into `19-shiftable-relink-milestone.md` (bootable matched XEX).**
  Rejected: that milestone runs the *original* PPC code under emulation/relink and
  is gated on full splits + reloc-normalized equivalence — a different, heavier
  dependency chain. Native runs *ported* code on the host and needs zero matching.
  They share nothing operationally; conflating them buries native under XEX-boot
  prerequisites it doesn't have.
- **Port the engine natively from rb3-xenon's own `src/system` instead of reusing
  DC3.** Rejected by the whole project thesis (engine-reuse doc): DC3 is the same
  engine, already 360-ported and rendering RB3 assets. Re-deriving it natively is
  the exact waste CLAUDE.md warns against.
- **Use rb3-Wii's native port instead.** rb3-Wii (`../rb3`) is MWCC/Wii with a
  `rndwii` GX renderer — the *outlier* on every rndobj divergence point. Its
  `MILO_ENGINE_GPU_BACKEND=rb3` backend is Wii-shaped. DC3's HD-console backend is
  the correct base for RB3-**360**. (rb3-Wii is still the game-*logic* oracle.)

## Effort & expected value

**Expected value in path-to-100% terms: ZERO direct.** Native does not build the
matched XEX, does not pin splits, does not move `matched_functions`. If the reader's
only metric is strict match %, this RFC is a **DEFER** — say so plainly.

**Expected value in project terms: real but indirect and long-horizon.**
- The synergy is the entire case. Comparable anchor: the band3 port campaigns that
  *did* move matching (`port-campaign`, `gameport1/2`, `bodyport-batch*` produced
  the +22 grind close and the Waypoint/CharClipGroup wins in
  `3342b30`/`a1312de`/`d3c6e4f`/`d696b52`). Native would consume those same ported
  TUs at ~zero marginal port cost and, via M4, *generate* new port-needed items
  (undefined-symbol reports) that feed the matching worklist. Plausible indirect
  yield: a steady trickle of band3 match candidates surfaced by native link errors
  — order **single-digit to low-tens of functions per wired TU cluster**, same
  magnitude as a bodyport batch, but as a *byproduct*, not a dedicated wave.
- Effort to milestones (owner-attention, not fleet-parallel): M0 days; M1–M3
  weeks each; M4 gated on matching cadence; M5 months. The playable-song endgame
  (M5) is a **multi-month** commitment with a real chance of not landing.

Honest framing: this is a **strategic/morale/portfolio artifact** (a playable RB3
is the project's most legible external win) with a **modest, indirect** contribution
to the strict metric. Fund it as a slow parallel track owned by one person, not as
a fleet workstream competing with `12-grind-fleet-v2.md` / `11-permuter-farm.md`.

## Risks & failure modes

- **M0 red (drift rot).** 5 months of engine matching may have broken the native
  build (types, STL shims, new `src/system` TUs pulling GPU deps). Mitigation: M0 is
  the gate; if it can't be made green in ~days, the track is dormant, not started.
- **milo-native-engine consumer wiring is nontrivial.** The engine builds only
  inside a consumer's include/flag/PCH context; getting rb3-xenon's context right
  (STLport-first includes, `/Fp` PCH path, ILP32-vs-LP64 `types.h`) is fiddly. DC3
  is the working reference, but rb3-xenon's engine is *more complete* than DC3's
  (per memory — several DC3 `_Native` shims cause "multiple definition" here), so
  it is not copy-paste.
- **The "Can't make X" game-class wall.** M3–M5 need RB3 game classes DC3's engine
  lacks (`GemTrackDir`, gem-highway HUD widgets). These come *only* from the band3
  port — so M5 is fully gated on matching-track progress on exactly the hardest,
  RB3-specific dirs.
- **Divergence between DC3 engine and RB3's expectations.** CLAUDE.md caveat:
  dc3-decomp is *newer* than RB3; subtle behavioral/version differences. A
  faithfully-rendered asset ≠ a faithfully-simulated game; scoring/timing bugs
  from engine drift are plausible and hard to detect natively.
- **Effort sink with no metric payoff.** The biggest risk is that months go into a
  playable demo while the strict metric stalls. The synergy contract + the
  dashboard line are the guardrails; if they show near-zero band3-TU flow after a
  quarter, kill it.

## Kill criteria

- **M0 stays red** after a focused rebuild attempt (native build cannot be
  restored on current main within ~a week) → declare native dormant; keep using
  DC3's `milo-viewer` as an asset tool only.
- **Synergy is illusory:** after M4, wiring band3 TUs into the native line surfaces
  **zero** match-relevant bugs across ≥3 TU clusters (i.e. everything that compiles
  for MSVC-X360 already links clean under Clang-LP64 and vice-versa) → the "native
  forces useful ports" thesis is false; downgrade native to pure demo, stop
  claiming path-to-100% relevance.
- **milo-native-engine rejects a fourth consumer** or its per-consumer context cost
  exceeds the value (weeks of CMake plumbing per milestone) → keep `native/`
  self-contained, do not extract; revisit only if dc3/rb3 prove the shared model.
- **M5 game-class wall is unbreakable** because the required RB3 game classes never
  get matched/ported → cap the track at M3 (native asset render) and publish that
  as the deliverable; do not chase the playable loop.

## Open questions

- Does `rb3-dta` still build clean on current main? (M0 — answer by building.)
- What is the current state of `../rb3/docs/native/NATIVE_PORT_ROADMAP.md` and
  `../dc3-decomp/docs/native/NATIVE_PORT_STATUS.md`? Those are authoritative and
  were **not** read in this pass. A native-track agent must start there.
- Which single band3 TU is the best M4 beachhead — smallest undefined-symbol
  closure? (`Gem.cpp`/`GemTrack.cpp` are the leaf guess; verify by trial link.)
- Can the native track's undefined-symbol reports be auto-fed into the matching
  worklist / `decomp.db` as port-needed rows? (Would make the synergy mechanical.)
- Does `MILO_ENGINE_GPU_BACKEND=dc3` compose with rb3-xenon's include context, or
  does it need a `=rb3xenon` near-clone flavor as the engine-reuse doc speculates?

## References

- `docs/plans/engine-reuse-and-asset-rendering.md` — the "bigger play" doc; DC3
  engine renders RB3-360 assets; game-vs-engine value split. (Current per
  `docs/INDEX.md:111`.)
- `native/CMakeLists.txt` — the `rb3-dta` target, engine glob, shim list.
- `native/src/main_dta.cpp` — the DTA-parse entrypoint (138 songs verified).
- `native/src/dta_link_stubs.s` — 74 weak stubs (rendering/MIDI/synth/Win32).
- `native/src/platform/` — 18 platform shims (File/System/Thread native +
  Joypad/Net/Memcard stubs).
- `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_native_port.md`
  — hard-won native lessons, next-steps ladder, extraction intent.
- `../milo-native-engine/CLAUDE.md` + `README.md` — the shared-runtime three-layer
  model and consumer contract.
- `../rb3/docs/native/NATIVE_PORT_ROADMAP.md` + `NATIVE_PORT_INVENTORY.md` —
  canonical native roadmap (**read first**, unread in this pass).
- `../dc3-decomp/native/CMakeLists.txt` — the target template (viewer, render-test,
  milo2gltf, dc3-native).
- `../dc3-decomp/native/build/milo-viewer` — the working RB3-asset renderer.
- `config/45410914/objects.json` — band3 (153) / network TU inventory, all
  `NonMatching`.
- CLAUDE.md — "Decomp priority: the GAME, not the engine"; native build track;
  milo-native-engine architectural goal.

### Sibling RFCs
- `19-shiftable-relink-milestone.md` — the *other* endgame (bootable matched XEX via
  relink); complementary, not overlapping — native runs ported code, relink runs
  matched code.
- `03-master-sequencing-roadmap.md` — must account for the shared band3-port
  resource this RFC's synergy depends on.
- `18-metrics-and-dashboard.md` — home for the proposed "compilable-under-both"
  synergy metric.
- `12-grind-fleet-v2.md`, `11-permuter-farm.md` — the fleet workstreams native must
  **not** compete with for capacity.
- `13-codegen-idiom-library.md` — MWCC→MSVC idioms; native adds a *third* target
  (Clang-LP64) whose successful compile is a weaker but useful faithfulness signal.
