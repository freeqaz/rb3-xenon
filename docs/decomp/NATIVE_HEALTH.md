# Native health — collected measurement and its committed baseline

**Baseline refreshed by lane N1-GPUGATES, 2026-09-10, worktree off `5aa1cb7a`.**
Tool: `tools/native_health.sh`. This file is the **committed baseline**; re-run
the tool and diff against the tables below. A gitignored artifact is invisible
institutional memory and has already cost this project duplicate lane funding
twice, so this is deliberately a tracked file.

*(Previous baseline: lane S5-NATIVE, 2026-09-01, off `412e3f85` — `verdict=INCOMPLETE`,
`runtime_ran=2`, `rb3-render:nogpu`. Superseded; the GPU works again.)*

## The baseline line

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=3 runtime_total=3 gates_pass=37 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=47 scatter_dirb=0 scatter_multihost=23 rc=0 handpose_controls=3/3 handpose_baseline_fail=1
```

The link half of it, verbatim from `tools/native_build_gate.sh`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

**The native build is HEALTHY** — 18/18, **0 SKIPs**, 0 errors, 0 warnings, 0
linker diagnostics — and for the first time **all three runtime instruments ran**.

| measure | 2026-09-01 | **2026-09-10** | note |
|---|---:|---:|---|
| link targets verified | 18 / 18 | **18 / 18** | 0 skipped, 0 failed |
| runtime targets exercised | 2 / 3 | **3 / 3** | rb3-render now runs |
| runtime gates passed | 17 | **37** | +20 from rb3-render |
| runtime gates failed | 0 | **0** | |
| negative controls demonstrated | 2 / 3 | **5 / 5** | see the control table |
| scatter guests reaching NO target | 47 | **47** | unchanged |
| scatter direction B | 0 | **0** | |
| scatter multi-host guests | 23 | **23** | |

⚠ Two new contract fields are appended **after `rc`** (the contract permits
additions at the END only): `handpose_controls` and `handpose_baseline_fail`.

## ✅ The GPU works again — verified functionally, not by version string

S5 recorded `rb3-render` as unrunnable: NVIDIA kernel module 610.43.03 vs
userspace 610.57.04, so the ICD loaded but
`vk_icdGetInstanceProcAddr(NULL,"vkCreateInstance")` returned NULL.

The module was reloaded on a package update. **The box has NOT rebooted** (up 19
days). Re-verified here by creating a real instance rather than comparing
strings: `vkCreateInstance` → `VK_SUCCESS`, **2 physical devices** enumerated.
S5's prescribed remedy (reboot) turned out not to be required — worth noting
because "reboot to fix" was recorded as the only path.

## What the ~24 newly-reachable rb3-render gates assert

First time these have ever been observed on this box. The positive run renders
**2 cells** and most gates are **per-cell**, so counts multiply. All 20 emitted
gate lines PASS; `RESULT: ALL GATES PASSED (0 gate failure(s))`.

| gate | asserts | verdict |
|---|---|---|
| `archive-mounted` | the real multi-part ark mounted (`gen/main_xbox_0.ark`) | PASS |
| `system-config` ×2 | the shipped preinit DTA parses: **25 sections**, `objects`=226 blocks, `rnd/font`=**128 entries** (`Rnd::SetupFont` indexes elements 66..123, so a short font array is an out-of-range `Array()`, not a degraded font) | PASS |
| `gpu-real-adapter` | a **real** adapter, not Dawn's Null fallback — 1280x720 headless, BC textures | PASS |
| `trig-table` | `Sine(pi/2)=1`, `Cosine(0)=1`, `Sine(0)=0` | PASS |
| `symbol-table` | `Symbol` round-trip interning | PASS |
| `gfx-mode` | `GetGfxMode()==kNewGfx` ⇒ `RndMesh::MaxBones()==40` (kOldGfx truncates to 4 and smears skinned meshes) | PASS |
| `rnd-dir` ×2 | the loaded dir is an `RndDir` with `SyncObjects` run — otherwise nothing can draw | PASS |
| `drawable-census` ×2 | cell 1: **130 meshes, 0 skinned**; cell 2: **6 meshes, 6 skinned, 6 with Mat + diffuse Tex** | PASS |
| `bbox` ×2 | at least one mesh contributed a vertex, so the camera frames something | PASS |
| `draws-issued` ×2 | **130/130** and **6/6** meshes issued a draw over 4 frames | PASS |
| `png` ×2 | headless readback written to PNG | PASS |
| `image-not-empty` ×2 | coverage **4.54%** / **11.07%** (≥1%) and **104** / **17,967** distinct colours (≥16) — i.e. not a blank frame reported as success | PASS |
| `all-cells-rendered` | every requested cell passed | PASS |

Gates present in the binary but **not** reached on a healthy run: `gpu-ready`
and the `gpu-real-adapter` **false** arm (failure paths), `readback`/`png` false
arms, `archive-lookup` (fires only for a path absent from the ark index),
`clip-driven` / `scene-clip-driven` / `palette-invariant` /
`bone-length-invariant` (animation flags), and the four `handpose-*` gates
(require `--hand-audit`, below). 28 `Gate(` call sites, ~24 distinct names.

## ⛔ The handpose oracle is RED, and the selftest was crediting a control on it

**This is the most important finding of the lane, and it was structurally
invisible while the GPU was broken.**

`native_health.sh` credited `render-stale` as *"control WORKS"* on `rc=1` with 7
failures. But the **unperturbed** `--hand-audit` baseline is *itself* `rc=1` with
**5** failures. The control was being scored on breakage it did not cause — the
same-breakage-twice / absent-vs-absent trap the script was explicitly written to
prevent, reintroduced through a **configuration mismatch**: the `was_green` pair
check compared against the positive `rb3-render` run, which does **not** pass
`--hand-audit` and therefore contains **zero handpose gates**. *The pair rule was
right; its subject was wrong.*

The 5 baseline failures split into two unrelated problems:

| cell | handpose failures | cause |
|---|---:|---|
| `ui/track/gen/tracksystem_meshes` | **4** | **harness defect** — 130 STATIC meshes, no skeleton at all, so all four gates are structurally vacuous ("no bone had a parent", "ZERO composed bones") |
| `char/crowd/gen/crowd_female01` | **1** | **genuine** — `handpose-measured-hand-geometry`: a skeleton is reached but **no hand mesh is measured**, so every geometry verdict above it is vacuous |

On the crowd cell alone the baseline is **19 PASS / 1 FAIL**. Skeleton,
`handpose-recompose` and `handpose-recompose-composed[X18]` all **PASS** there.

⇒ **`handpose_baseline_fail=1` is a REAL, tracked red**, not a regression from
this lane. It is the one substantive native defect these gates have surfaced.

**Fixed:** the controls are now **delta-scored against a single-cell baseline on
a figure that can actually have hands**, never on `rc` alone.

## Negative controls — all five DEMONSTRATED

A runtime gate nobody has shown able to FAIL is worth nothing.

| control | invoked | Δ vs baseline | result |
|---|---|---:|---|
| `rb3-ark --corrupt` | CLI flag | — | ✅ **RED** — rc=1, exactly 2 gates fail (`sha256`, `memcmp`); other 7 still pass ⇒ targeted |
| `rb3-milo <bogus path>` | positional | — | ✅ **RED** — rc=1, `path-resolved` + `all-milos-loaded` fail |
| `RB3_HANDPOSE_STALE=1.0` | **env var, float** | **+2** | ✅ **RED** — `handpose-recompose` **and** `...-composed[X18]` |
| `RB3_HANDPOSE_PUBLISH=1.0` | **env var, float** | **+1** | ✅ **RED** — `handpose-recompose` only |
| `RB3_HANDPOSE_PERTURB=1.0` | **env var, float** | **0** | ✅ **INERT BY DESIGN, and proven APPLIED** |

⛔⛔ **THE HANDPOSE CONTROLS ARE ENV VARS, NOT CLI FLAGS**
(`main_render.cpp:4980-4982`), each taking a float, and all three are **inert
without the real flag `--hand-audit`**. Spelling them as flags fails **silently
in the false-green direction**: `main_render.cpp:4965`'s loop ends in
`else pos.push_back(argv[i])`, so an unrecognised token becomes a **positional
arkPath**. Same family as the zsh word-splitting trap. **Verify every run by
reading the loaded cell name out of the log, never `rc` alone.**

### PERTURB is inert BY DESIGN — S5's prediction, now measured

S5 predicted only STALE would turn a gate red and asked that it be verified
rather than assumed. **Confirmed, and the mechanism is confirmed too** — the
quantitative half is far stronger than the gate counts:

| run | worst composed dev | COMPOSED bones | reading |
|---|---|---:|---|
| baseline | 0.000e+00 | 6 | — |
| PERTURB | 0.000e+00 | **6** | `SetLocalXfm` re-composes; bone stays COMPOSED |
| PUBLISH | 0.000e+00 | **5** | bone **leaves** the COMPOSED population (tag → PUBLISHED), so X18 correctly does not see it and only the uncorrected gate fires |
| STALE | **1.000e+00** | 5 | stale-but-COMPOSED, worst bone **`bone_L-hand.mesh`**, deviation **exactly the injected 1.0** — precisely what X18 exists to catch |

⇒ PERTURB is a control on the **measurement**, not on the **verdict**. Scoring it
as "must go red" would be wrong. It is credited only when it is **proven
APPLIED** — the binary's own `⚠⚠ RB3_HANDPOSE_PERTURB=1.000 ACTIVE` line — **and**
the verdict correctly did not move. Without that activation witness, the arm
would pass for a **deleted feature**.

### Proof the new scoring can fail

The delta-scorer is itself a gate, so it was shown able to fail:

| self-break | observed | scorer verdict |
|---|---|---|
| `RB3_HANDPOSE_STALE=0.0` (no-op perturbation) | delta **0** | **GREEN — "THE CONTROL DID NOT FIRE"** ⇒ RED arm can fail |
| misspelled var (never reaches the code) | **0** activation lines | **GREEN — "VACUOUS"** ⇒ INERT arm can fail |

## Scatter audit — 47, unchanged, now fully itemised

`scatter_unlinked=47`, identical to 2026-09-01 (it had drifted 42→47 unnoticed
for a month before that, because the only record was a dated plan doc). S5's
table listed 8 directories summing to 43; the full breakdown is:

| directory | x10 (08-03) | 09-01 | **09-10** |
|---|---:|---:|---:|
| `src/band3/meta_band` | 11 | 13 | **13** |
| `src/system/rnddx9` | — | 7 | **7** |
| `src/band3/game` | 4 | 5 | **5** |
| `src/system/hamobj` | 5 | 5 | **5** |
| `src/band3/bandtrack` | — | 4 | **4** |
| `src/system/meta` | — | 3 | **3** |
| `src/system/synth` | — | 3 | **3** |
| `src/system/synth_xbox` | — | 3 | **3** |
| `src/system/bandobj` | — | — | **2** |
| `src/system/net` | — | — | **1** |
| `src/system/gesture` | — | — | **1** |
| **total** | 42 | 47 | **47** |

⚠ **`47` is not a defect count.** It is the strictly-weaker *decidable predictor*
x10 defined: a file neither compiled standalone in any target nor emitted by any
scatter host. "Missing" is a **demand** property only the link can answer, and
every target links today. The raw `direction_a` row count (**2,103**) is
per-target and is **not** the headline.

## What the runtime instruments assert

Only **3 of 18** targets have `Gate(name, ok, detail)` + `gFailures`.

| target | gates | what it certifies |
|---|---:|---|
| `rb3-ark` | 11 (9 run) | the real multi-part `.ark` is mounted and a file **3.34 GB in — past 2^31** — reads **byte-exactly** (size, SHA-256, `memcmp` vs an independent extraction), then 138 song entries parse. Certifies the whole 64-bit cumulative-walk path. |
| `rb3-milo` | 12 (8 run) | a real `.milo_xbox` loads; the ObjectDir header table **reconciles** (131 own + 0 skipped = 131, header says 131); no class lacks a factory; graph non-empty. |
| `rb3-render` | 28 sites / ~24 names (**20 lines on a healthy 2-cell run**, +4 handpose under `--hand-audit`) | config, real GPU adapter, dir load, drawable census, draws issued, PNG readback, image-not-empty, all-cells-rendered, and the hand-pose/recompose algebra. |

⛔ **`main_score2.cpp` returns 0 unconditionally.** It prints `MATCH`/`DIVERGENT`
comparing the real `Player::GetMultiplier` path against the M5 `score_engine`
transcription — and a **`DIVERGENT` result exits green**. **Still unfixed**; this
lane did not touch it. It is not counted as a gate and must not be read as one
until it propagates its verdict to `main`'s return.

## ✅ CI — the LINK gate is now wired (and it cannot pass vacuously)

CI native coverage was **0** (`grep -ci native .github/workflows/build.yml` = 0).
Added step **`Native link gate`**, after the symbols.txt fixpoint guard:

```yaml
    - name: Native link gate
      env:
        NATIVE_GATE_ALLOW_INCOMPLETE: "1"
      run: ./tools/native_build_gate.sh .
```

**Only the link gate.** It catches the ODR / undefined-symbol / ILP32-only class
that the X360 build is **structurally blind** to (it never links) and that has
silently broken `main` **four times**. The runtime gates are deliberately **not**
in CI: they need the ~4 GB retail ark and a GPU, so they would be permanently
SKIPPED, and a job that always skips is a job everyone learns to ignore. The
runtime sweep stays an explicit **on-demand** target, never a build edge.

**`ALLOW_INCOMPLETE=1` does not weaken the gate — DEMONSTRATED, not assumed.**
The FAIL branch (`native_build_gate.sh:519`) exits 1 *before* the ALLOW branch
(`:546`) is reached. Proven by appending `#error` to a native TU **with the
variable set**:

```
NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=1 skipped=0 partial=0 failed=17 rc=1
```

⚠ If the CI container lacks cmake/clang the gate exits **2 (UNRUNNABLE)** and the
job fails **loudly** rather than passing vacuously — correct for a gate, but the
most likely first-run failure mode. **Fix the container; do not add `|| true`**,
which silently restores zero coverage behind a green check.

## Cadence

1. **Pre-landing, by hand — the hard rule.** Any lane touching shared `src/**`
   runs `tools/native_health.sh --selftest` and pastes the
   `NATIVE_HEALTH_RESULT` line. Require **`0 SKIPs`**, never just "PASS": the
   full-pass prose is `PASS  (rc=0, …` and the incomplete one is
   `PASS (INCOMPLETE: …`, **one space apart**, and that has been mis-relayed
   upstream before. Parse the contract line, not the prose.
2. **CI: the link gate**, as wired above.
3. **On-demand / weekly full run**, updating this file so drift surfaces in a diff.

Do **not** add the sweep to the default `ninja` build: a cold native build is
~3,700 edges / ~6 min and the runtime half needs assets outside the repo.

## What this lane did NOT verify

- **The genuine `handpose-measured-hand-geometry` red was not FIXED**, only
  isolated and characterised. Why the crowd figure exposes no measurable hand
  mesh is unknown; it needs an asset/oracle lane, not a tooling one.
- **The 4 vacuous handpose failures on the static-mesh cell were not fixed in
  `main_render.cpp`.** The right fix is for `--hand-audit` to skip cells with no
  skeleton (or to gate on the denominator), which is a `native/src/` change this
  lane deliberately scoped out. Worked around in the harness by baselining on a
  cell that has a figure.
- **`main_score2.cpp`'s unconditional `return 0`** — still vacuous, untouched.
- **The CI step has never executed in CI.** It is validated locally (YAML parses;
  the gate's FAIL/ALLOW_INCOMPLETE interaction demonstrated) but the container's
  toolchain availability is unverified — see the rc=2 warning above.
- **The 15 gateless targets** were linked but never executed; only `main_score2`
  was audited for the vacuous-return defect, the other 14 were not.
- **Whether the 47 unlinked scatter guests matter**, and **`scatter_audit.py`'s
  own correctness** (counts reproduced, method read, agreement with
  `ScatterIncludes.cmake` not independently tested).
- **Any X360 matching-build effect.** This lane edited only `tools/`, `docs/` and
  `.github/`, and **no `src/`**, so the matching metric is untouched by
  construction.

---

# 2026-09-10 — lane L4-NATIVESCATTER: the 47 scatter guests, classified and 31 wired

**Appended, not restructured.** Everything above is lane N1-GPUGATES' record and
stands. This section closes the two items N1 listed as not verified: *"whether
the 47 unlinked scatter guests matter"* and *"`scatter_audit.py`'s own
correctness"*. Both turned out to be real.

Rebased onto main after N1 landed (`a18a9885`). Result line from this branch:

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=3 runtime_total=3 gates_pass=37 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=16 scatter_dirb=0 scatter_multihost=23 rc=0 handpose_controls=3/3 handpose_baseline_fail=1
```

**Identical to N1's line in every field except `scatter_unlinked`: 47 → 16.**
The link gate, all 37 runtime gates, the 3/3 handpose controls and the tracked
baseline red are unchanged by wiring 31 TUs — which is the result that licenses
the wiring, not a formality.

⚠ **N1's control correction supersedes this lane's first reading, and this lane's
own data agrees with N1.** Before rebasing, the pre-N1 tool reported
`render-stale ... 7 gate(s) failed (control WORKS)`. N1 showed one of those was a
**pre-existing** red (`handpose-measured-hand-geometry`) and that stale is really
**+2 over baseline**. The old absolute count was over-credit. Nothing here
contradicts N1.

## They had ONE cause, not 47

Every one of the 47 was unlinked for the same reason, measured per file: **the
guest's only unconditional scatter host is itself compiled in no target.** Not
one was unlinked by a platform filter on *itself*.

The host/guest pairings are retail COMDAT placement and therefore arbitrary —
`synth/MidiInstrument.cpp` hosts `bandtrack/GemTrack.cpp`, `rnddx9/CubeTex.cpp`
hosts `meta_band/AppLabel.cpp`, `bandobj/BandSwatch.cpp` hosts
`rnddx9/Rnd_Xbox.cpp`. ⇒ **A guest's fitness for the native build has nothing to
do with its host's**, so "compile the host" is the wrong fix; compiling the
guest **standalone** is the right one, and it is safe *precisely because* the
host is absent (nothing else emits those bodies).

## Method — three measurements per candidate

1. **Compiles standalone** under the target's own flags (`clang -fsyntax-only`)?
   42 of 47 did.
2. **Does its scatter closure collide** with anything `rb3-milo` already
   compiles or emits? Predicted analytically, then confirmed by the linker.
3. **Does it link** — 0 undefined *and* 0 multiple definitions?

**NOT ONE of the 31 needed a stub.** Across the whole wave the link reported
**0 undefined references**; every failure was a *collision*, never a missing
symbol. There is no STUB-NEEDED-to-wire class here — not the expected outcome,
and the reason the "record the undefined symbols" column below is empty.

⚠ **"Wired" means compiled and presented to the linker, NOT present in the
image.** These targets carry `-Wl,--gc-sections`, so unreferenced sections are
dropped and the binaries do not grow — `nm` finds none of the new classes in
`rb3-render`. What is bought is that the **ODR / undefined-symbol /
does-it-even-compile** class is now covered for 31 TUs, exactly the class the
X360 match build is structurally blind to. Do **not** read this as "the native
port now runs this code".

## Counts

| disposition | n |
|---|---:|
| **WIRE — wired this lane** | **31** |
| PLATFORM-ONLY — must stay out | 12 |
| STUB-NEEDED / BLOCKED | 2 |
| REAL DEFECT — deliberately not papered over | 1 |
| FALSE POSITIVE — never was unlinked | 1 |
| **total** | **47** |

**UNKNOWN: 0.** Every file has a measured disposition.

## ⛔ Yes, they mattered: a native-only feature that had never compiled

`bandtrack/TrackPanel.cpp` carried an `#ifdef HX_NATIVE` block — the note-highway
depth-composite fix, with a written rationale and its own
`RB3_NO_TRACK_DEPTH_CLEAR` opt-out documented in place — that spelled
`extern Rnd &TheRnd` (a **reference**, `rndobj/Rnd.h:402`) as
`if (TheRnd) TheRnd->ClearDepthForOverlay()`. **That cannot compile.** Nobody
found out because the file reaches no native target.

⇒ That is the concrete answer to N1's open question. Fixed inside the existing
`HX_NATIVE` guard.

## ⚠ `meta_band/MainHubPanel.cpp` — a real defect, recorded rather than silenced

`MainHubPanel.cpp:414` reads `if (node8d8.Str() != "")` where `Str()` returns
`const char*`: a **pointer compared against a string literal's address**, always
true, so an empty MOTD still reaches `AddUnlinkedMotd`. `-Werror=string-compare`
(one of the curated real-bug diagnostics in `DECOMP_FLAGS`) catches it.

**Deliberately NOT fixed.** Retail MSVC compiles the same pointer comparison, so
retail behaves this way; "correcting" it would diverge from the target, and
silencing the diagnostic to get the TU wired would hide a real bug to buy a
number. **This needs a decision (stay bug-compatible vs. correct it), not a
workaround.**

## Per-file disposition

| file | disposition | why |
|---|---|---|
| `src/band3/bandtrack/GemTrack.cpp` | FALSE POSITIVE | already emitted by `ui/UIList.cpp` behind a self-`#define`d sentinel (`UILIST_SW3_PRIMARY_TU`) that `scatter_audit.py` cannot see |
| `src/band3/meta_band/CriticalUserListener.cpp` | STUB/BLOCKED | double-emits `flow/FlowManager.cpp`, already emitted by `char/CharBonesMeshes.cpp` — a multi_host guest, so no standalone-prune can resolve it. Measured: `multiple definition of TheFlowMgr`, `FlowManager::FlowManager()` |
| `src/band3/meta_band/MainHubPanel.cpp` | REAL DEFECT | `MainHubPanel.cpp:414` `node8d8.Str() != ""` compares a `const char*` against a literal — always true. Caught by `-Werror=string-compare`. Retail has the same bug, so this is NOT silently "fixed": see below |
| `src/band3/meta_band/SongUpgradeMgr.cpp` | STUB-NEEDED | `SongUpgradeMgr.h:27` uses STLport `_STLP_TEMPLATE_NULL`; defining it is necessary but NOT sufficient — `hash` is not declared in `namespace stlpmtx_std` natively, so it needs a real primary-template shim |
| `src/system/gesture/LiveCameraInput.cpp` | PLATFORM-ONLY | Kinect NUI (`NuiInitialize`, `E_NUI_DATABASE_NOT_FOUND`) |
| `src/system/rnddx9/Cam.cpp` | PLATFORM-ONLY | DX9 RndCam; native uses the dc3 Wgpu backend |
| `src/system/rnddx9/Lit.cpp` | PLATFORM-ONLY | DX9 RndLight |
| `src/system/rnddx9/Mat.cpp` | PLATFORM-ONLY | DX9 RndMat |
| `src/system/rnddx9/Mesh.cpp` | PLATFORM-ONLY | DX9 RndMesh |
| `src/system/rnddx9/MultiMesh.cpp` | PLATFORM-ONLY | DX9 RndMultiMesh |
| `src/system/rnddx9/Part.cpp` | PLATFORM-ONLY | DX9 RndParticleSys |
| `src/system/rnddx9/Rnd_Xbox.cpp` | PLATFORM-ONLY | the DX9/Xbox Rnd itself; also `_Xbox.cpp` platform-filtered |
| `src/system/synth/Mic.cpp` | PLATFORM-ONLY | real mic capture device; MicNull.cpp is the portable half and IS wired |
| `src/system/synth_xbox/FftIpp.cpp` | PLATFORM-ONLY | Intel IPP FFT, Xbox-only audio analysis |
| `src/system/synth_xbox/StreamReceiver360.cpp` | PLATFORM-ONLY | X360 XAudio2 stream receiver |
| `src/system/synth_xbox/soundtouch/source/SoundTouch/RateTransposer.cpp` | PLATFORM-ONLY | vendored SoundTouch under the Xbox synth tree |
| `src/band3/bandtrack/GemRepTemplate.cpp` | **WIRE** ✅ |  |
| `src/band3/bandtrack/TrackConfig.cpp` | **WIRE** ✅ |  |
| `src/band3/bandtrack/TrackPanel.cpp` | **WIRE** ✅ |  |
| `src/band3/game/ChordbookPanel.cpp` | **WIRE** ✅ |  |
| `src/band3/game/DirectInstrument.cpp` | **WIRE** ✅ |  |
| `src/band3/game/Game.cpp` | **WIRE** ✅ |  |
| `src/band3/game/GemPlayer.cpp` | **WIRE** ✅ |  |
| `src/band3/game/NetGameMsgs.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/AccomplishmentManager.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/AccomplishmentPlayerConditional.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/AccomplishmentProgress.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/AccomplishmentSongListConditional.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/AccomplishmentTrainerConditional.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/AppLabel.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/SessionUsersProviders.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/SongSortByPlays.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/SongSortMgr.cpp` | **WIRE** ✅ |  |
| `src/band3/meta_band/Utl.cpp` | **WIRE** ✅ |  |
| `src/system/bandobj/BandCrowdMeter.cpp` | **WIRE** ✅ |  |
| `src/system/bandobj/BandList.cpp` | **WIRE** ✅ |  |
| `src/system/hamobj/DancerSequence.cpp` | **WIRE** ✅ |  |
| `src/system/hamobj/HamBattleData.cpp` | **WIRE** ✅ |  |
| `src/system/hamobj/HamLabel.cpp` | **WIRE** ✅ |  |
| `src/system/hamobj/HamNavProvider.cpp` | **WIRE** ✅ |  |
| `src/system/hamobj/HamRibbon.cpp` | **WIRE** ✅ |  |
| `src/system/meta/ConnectionStatusPanel.cpp` | **WIRE** ✅ |  |
| `src/system/meta/Meta.cpp` | **WIRE** ✅ |  |
| `src/system/meta/MetaMusicManager.cpp` | **WIRE** ✅ |  |
| `src/system/net/WebSvcMgr.cpp` | **WIRE** ✅ |  |
| `src/system/synth/FxSendWah.cpp` | **WIRE** ✅ |  |
| `src/system/synth/MicNull.cpp` | **WIRE** ✅ |  |

### What PLATFORM-ONLY rests on

`rnddx9/*` implement `RndCam`/`RndMesh`/`RndMat`/`RndPart`/`RndMultiMesh` against
D3D9. The native build replaces that entire layer with the dc3 WebGPU backend, so
adding them would duplicate the renderer, and `native/CMakeLists.txt` already
filters `(Dx9|DX9|Wgpu|D3D|Xenon)` out of `MILO_FORK_SOURCES` as standing policy.
⚠ They pass `-fsyntax-only` — **compiling is not the test**; the test is that they
are the wrong implementation of a class the engine already supplies.

## `scatter_audit.py` IS wrong — by exactly one file

N1 listed its correctness as untested. `tools/scatter_emitted_truth.py` (new)
tests it by asking the **compiler**: `clang++ -M` resolves every conditional
exactly as the real compile does, with commands from `ninja -t compdb` so each
TU gets its own target's defines.

`scatter_audit` traverses **unconditional** `#include "x.cpp"` edges only. But an
edge can sit behind `#ifdef SENTINEL` where the host `#define`s SENTINEL a few
lines above itself — the idiom that fires the include when the host is the
PRIMARY TU and keeps it inert when the host is itself scatter-included.
`src/system/ui/UIList.cpp:7` does this for `bandtrack/GemTrack.cpp`. The state
machine sees `#ifdef`, files it conditional, never traverses it, and reports
GemTrack as unlinked — **while the linker already defines GemTrack's every symbol
out of `UIList.cpp.o`.** Found the hard way: wiring GemTrack produced 65
`multiple definition` errors.

Measured, 541 preprocess jobs, **0 failures** (a failure would read as "emits
nothing" — a vacuous pass — so failures are reported, not counted):

| | |
|---|---:|
| unlinked per `scatter_audit` (uncond-only) | 47 |
| unlinked per compiler truth | **46** |
| **false positives** | **1** (`bandtrack/GemTrack.cpp`) |
| instrument disagreements | 10 (5 files × 2 targets) |

★ **I expected a large false-positive class and was WRONG — it is one file.** The
mechanism is real, the magnitude is small, and both halves belong in the record.
All 10 disagreements are **one-directional**: the audit under-counts emission and
can therefore only *over*-report "unlinked". It never fabricates emission, which
is the reassuring direction for a conservative instrument.

⇒ The honest headline is **16 reported / 15 genuinely unlinked**, and every one
of the 15 is either platform-only or carries a recorded reason below.

## ✅ `main_score2.cpp` no longer returns 0 unconditionally

N1's "did NOT verify" list still reads *"`main_score2.cpp`'s unconditional
`return 0` — still vacuous, untouched."* **Fixed on this branch** (commit
`native(score2): make the scoring cross-check a gate that can actually FAIL`):
the two cross-checks emit the house `  [PASS]/[FAIL]` contract, the verdict
reaches `main`'s return, and a real CLI flag `--force-divergent` demonstrates it
red.

| invocation | rc | observed |
|---|---:|---|
| (none) | **0** | 3 gates pass |
| `--force-divergent` | **1** | exactly **2** gates FAIL; `scenB-longest-streak` still passes ⇒ **targeted, not blanket** |
| `--forcedivergent` (typo) | **2** | **REFUSED** |

That third row is why this is a flag and not an env var: the old parser was
`argv[1] == "guitar"` with everything else silently ignored — the same
false-green shape as the handpose env vars. It now rejects unknown arguments.

## What this lane did NOT do

- **Did not touch `tools/native_health.sh` or restructure this document.** N1's
  tool and text are authoritative; this is an append.
- **Did not wire `main_score2` into `native_health.sh`'s runtime list**, though it
  now has the contract for it. `native_health.sh:159`'s comment still describes
  `main_score2` as vacuous and is now stale — a one-line follow-up owned by
  whoever next edits that tool.
- **Did not resolve `MainHubPanel.cpp`** (needs a bug-compatibility decision) or
  **`SongUpgradeMgr.cpp`** (needs a `stlpmtx_std::hash` primary-template shim for
  native; defining `_STLP_TEMPLATE_NULL` alone is necessary but not sufficient —
  measured).
- **Did not attempt `CriticalUserListener.cpp`.** It double-emits
  `flow/FlowManager.cpp`, already emitted by `char/CharBonesMeshes.cpp`; as a
  `multi_host` guest no standalone-prune can resolve it. Resolving it means
  editing the scatter graph, not the build.
- **Did not measure any X360 effect, because none is possible.** The only `src/`
  edits are `bandtrack/TrackPanel.cpp` and `game/GemPlayer.cpp`, both **inside
  `#ifdef HX_NATIVE`**. The match build never defines `HX_NATIVE`, so its
  preprocessed text is byte-identical and an A/B would be absent-vs-absent by
  construction. Match-neutral by a **preprocessor property, not by measurement**.
- **Did not re-check the 23 multi-host guests**, unchanged at 23 throughout.
