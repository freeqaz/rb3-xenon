# Native health — collected measurement and its committed baseline

**Lane S5-NATIVE, 2026-09-01, worktree off `412e3f85`.** Tool:
`tools/native_health.sh`. This file is the **committed baseline**; re-run the
tool and diff against the table below. A gitignored artifact is invisible
institutional memory and has already cost this project duplicate lane funding
twice, so this is deliberately a tracked file.

## The baseline line

```
NATIVE_HEALTH_RESULT verdict=INCOMPLETE link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=INCOMPLETE runtime_ran=2 runtime_total=3 gates_pass=17 gates_fail=0 unrunnable=rb3-render:nogpu selftest=PARTIAL scatter_unlinked=47 scatter_dirb=0 scatter_multihost=23 rc=3
```

The link half of it, verbatim from `tools/native_build_gate.sh`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

**The native build is HEALTHY** — 18/18, **0 SKIPs**, 0 errors, 0 warnings,
0 linker diagnostics. Measured in a **cold worktree with no seeding of any
kind**, which independently re-confirms the task-#90 claim that
`native/CMakeLists.txt` resolves its siblings from the real repo.

⚠ The overall verdict is `INCOMPLETE` (rc=3), **not** `FAIL`. Nothing is known
to be broken; `rb3-render` was **not tested** — see the GPU section.

| measure | value | note |
|---|---:|---|
| link targets verified | **18 / 18** | 0 skipped, 0 failed |
| runtime targets with gate machinery | **3 / 18** | rb3-render, rb3-milo, rb3-ark |
| runtime targets actually exercised | **2 / 3** | rb3-render unrunnable (no GPU) |
| runtime gates passed | **17** | rb3-milo 8, rb3-ark 9 |
| runtime gates failed | **0** | |
| negative controls DEMONSTRATED red | **2 / 3** | ark-corrupt, milo-badpath |
| scatter guests reaching NO target | **47** | was 42 — see drift |
| scatter direction B (excluded-but-emitted) | **0** | |
| scatter multi-host guests | **23** | |

## Scatter drift: 42 → 47, and where it went

`docs/plans/x10-band-geometry-2026-08-03.md:188` recorded **42** scatter-guest
files reaching no native target, "dominated by `src/band3/meta_band` (11),
`src/band3/game` (4), `src/system/hamobj` (5)". Today the count is **47**:

| directory | x10 (2026-08-03) | now (2026-09-01) |
|---|---:|---:|
| `src/band3/meta_band` | 11 | **13** |
| `src/system/rnddx9` | — | **7** |
| `src/band3/game` | 4 | **5** |
| `src/system/hamobj` | 5 | 5 |
| `src/band3/bandtrack` | — | 4 |
| `src/system/meta` | — | 3 |
| `src/system/synth` | — | 3 |
| `src/system/synth_xbox` | — | 3 |

This drifted **unnoticed for a month** because the only record was a dated plan
doc that nothing re-reads. That is the whole reason this file exists.

⚠ **`47` is not a defect count.** It is the strictly-weaker *decidable
predictor* x10 defined: a file that is neither compiled standalone in any target
nor emitted by any scatter host any target compiles. "Missing" is a **demand**
property that only the link can answer, and every target links today. Do not
report 47 as "47 broken files". The raw `direction_a` row count (**2,103**) is
per-target and is **not** the headline — x10 explicitly discarded that number.

## What the runtime instruments actually assert

Only **3 of 18** targets have the `Gate(name, ok, detail)` + `gFailures`
machinery. The other 15 print ad-hoc pass/fail text with no common contract.

| target | gates | what it certifies |
|---|---:|---|
| `rb3-ark` | 11 (9 run) | The real multi-part `.ark` is mounted and a file **3.34 GB in — past 2^31** — is read **byte-exactly**: size, SHA-256 and `memcmp` against an independently extracted reference, then 138 song entries parsed. This certifies the whole 64-bit cumulative-walk path, not "a file opened". |
| `rb3-milo` | 12 (8 run) | A real `.milo_xbox` loads; the ObjectDir header table **reconciles** against the object census (131 own + 0 skipped = 131, header says 131); no class lacks a factory; the graph is non-empty. |
| `rb3-render` | ~27 (**3 run**) | System config, GPU adapter, dir load, hand-pose/recompose algebra, alias-safety of the compose family, bone-length invariants, drawable census, draws issued, PNG readback, image-not-empty, all-cells-rendered. **24 of these were NOT reached** — see below. |

⛔ **`main_score2.cpp` returns 0 unconditionally.** It prints
`MATCH`/`DIVERGENT` comparing the real `Player::GetMultiplier` scoring path
against the M5 `score_engine` transcription — and a **`DIVERGENT` result exits
green**. It is not counted as a gate here and must not be read as one until it
propagates its own verdict to `main`'s return.

## ⛔ rb3-render cannot run on this machine — host driver mismatch

`rb3-render` is the largest runtime instrument (27 gates) and it reaches only
**3** of them before `main_render.cpp:5131` `return 2`s on `NO GPU`. The cause is
**not** the sandbox and **not** a repo defect:

```
NVRM kernel module : 610.43.03      (loaded)
userspace libGLX   : 610.57.04      (installed)
```

The ICD `dlopen`s fine and all its dependencies resolve, but
`vk_icdGetInstanceProcAddr(NULL, "vkCreateInstance")` returns **NULL** — proven
directly with `ctypes` — so the Vulkan loader reports *"Could not get
'vkCreateInstance'… Found no drivers"* and Dawn falls back to the Null backend.
That is the signature of an NVIDIA driver upgraded on disk without the matching
kernel module being loaded. No software rasterizer (lavapipe) is installed as a
fallback.

**Remedy: reboot** to load the 610.57.04 kernel module, then re-run
`tools/native_health.sh --selftest` and expect `verdict=PASS`, `runtime_ran=3`,
`selftest=PASS`. Not done by this lane — other agents were mid-build.

⇒ Until then, **every `rb3-render` claim in `docs/plans/x*.md` is unverifiable
on this box**, and the three `RB3_HANDPOSE_*` controls are UNDEMONSTRATED.

## Negative controls — which are DEMONSTRATED to go red

A runtime gate nobody has shown able to FAIL is worth nothing.

| control | how invoked | result |
|---|---|---|
| `rb3-ark --corrupt` | CLI flag; flips one bit of the ark-read buffer | ✅ **RED** — rc=1, exactly 2 gates fail (`sha256-match`, `byte-for-byte memcmp`); the other 7 still pass, so it is *targeted*, not a blanket failure |
| `rb3-milo <bogus path>` | no dedicated flag | ✅ **RED** — rc=1, `path-resolved` + `all-milos-loaded` fail |
| `RB3_HANDPOSE_STALE` | **env var, float** | ⛔ **UNDEMONSTRATED** — lives past the NO-GPU return |
| `RB3_HANDPOSE_PUBLISH` | **env var, float** | ⛔ **UNDEMONSTRATED** — same |
| `RB3_HANDPOSE_PERTURB` | **env var, float** | ⛔ **UNDEMONSTRATED** — same |

⛔⛔ **THE HANDPOSE CONTROLS ARE ENV VARS, NOT CLI FLAGS.** They are
`RB3_HANDPOSE_PERTURB` / `_PUBLISH` / `_STALE`, read at `main_render.cpp:4980-4982`
and each taking a **float**. There is no `--hand-perturb`, `--hand-publish` or
`--hand-stale` anywhere in the tree; the only similar flag is `--hand-audit`
(a boolean that *enables the oracle these controls perturb*, and without which
all three are inert).

⚠ **And spelling them as flags fails SILENTLY IN THE FALSE-GREEN DIRECTION.**
`main_render.cpp:4965`'s argument loop ends in `else pos.push_back(argv[i])`, so
an unrecognised `--hand-perturb 0.5` is not rejected — both tokens become
**positional arkPath arguments**. The renderer then tries to load them as milo
files. This is the same family as the zsh word-splitting trap that produced
false greens in three consecutive native lanes: *a harness whose failure mode is
"renders something else and passes" is worse than one that crashes.*

Per `main_render.cpp:2821-2913`, only **STALE** is predicted to turn a gate red
(it forges a stale-but-`COMPOSED` bone that
`handpose-recompose-composed[X18]` **must** catch). `PERTURB` re-composes by
construction and `PUBLISH` flips the writer tag, so both are controls on the
*measurement* rather than on the *verdict*. **Verify that prediction when the
GPU is back** — do not assume it.

## The tool, and proof it can fail

`tools/native_health.sh [dir] [--selftest] [--skip-link]`. Exit codes share
`native_build_gate.sh`'s vocabulary: **0** healthy · **1** broken · **2** could
not run · **3** ran but does not vouch for full coverage.

All three failure paths were **executed**, not asserted:

| probe | rc | observed |
|---|---:|---|
| healthy tree | **3** | `INCOMPLETE`, naming `rb3-render:nogpu` as untested |
| `RB3_ARK_REF` pointed at a truncated file | **1** | `FAIL`, 3 ark gates red |
| `--badoption` | **2** | `UNRUNNABLE` |

Anti-vacuity properties, each of which exists because the house ledger contains
the corresponding failure:

- **A zero-gate run is `UNRUNNABLE`, never `PASS`.** A binary that prints no
  verdict has not passed; it has not been measured.
- **`rc=2` from `rb3-render` is `UNRUNNABLE`, not `FAIL`.** Conflating a
  driverless box with a broken build would cry wolf on every CI-like host.
- **The selftest ENFORCES the pair.** A negative control is only credited if the
  *same target's positive run was green first*. Demonstrated: with the sabotaged
  ark reference, `ark-corrupt` still exits 1 with gates red — and the tool
  **SKIPs** it with *"the positive run was not green, so a red here would prove
  nothing"* instead of scoring it as a working control. This gap was present in
  the first draft (documented but not enforced) and is exactly the
  absent-vs-absent trap `ab_measure.py` refuses on.
- **The link verdict is parsed from the `NATIVE_GATE_RESULT` contract line**,
  never from the prose — `PASS  (rc=0,` and `PASS (INCOMPLETE:` differ by one
  space and have been mis-relayed before.
- `grep` is pinned to `command grep -a` throughout (the shell shim is
  binary-blind and yields false negatives shaped like decisive ones).

## Cadence recommendation — NOT wired to CI or ninja

`tools/native_health.sh` is **not** wired into CI and **not** wired into the
default ninja build, deliberately, pending a decision.

CI has **zero** native coverage today (`grep -ci native .github/workflows/build.yml`
= **0**) and the CI container structurally cannot host the engine — no
`milo-native-engine`, no Dawn, and certainly no GPU. Recommended:

1. **Pre-landing, by hand — the only hard rule.** Any lane touching shared
   `src/**` runs `tools/native_health.sh --selftest` and pastes the
   `NATIVE_HEALTH_RESULT` line. This subsumes the existing "run the native gate"
   rule and costs ~40 s warm.
2. **CI: add the LINK gate only, with
   `NATIVE_GATE_ALLOW_INCOMPLETE=1`** — it catches the ODR/undefined-symbol
   class the X360 build is structurally blind to, which has silently broken
   `main` four times. Do **not** put the runtime gates in CI: they need the
   ~4 GB retail ark and a GPU, so they would be permanently skipped and would
   train everyone to ignore the job.
3. **Weekly or on-demand for the full run**, once the GPU is fixed, updating
   this file's baseline table so drift like 42→47 surfaces in a diff.

Do **not** add it to the default `ninja` build: a cold native build is ~3,700
edges / ~6 min, and the runtime half depends on assets outside the repo.

## What this lane did NOT verify

- **`rb3-render`'s 24 unreached gates** and all three `RB3_HANDPOSE_*` controls
  — blocked on the host driver mismatch above. This is the single biggest gap.
- **The 15 gateless targets.** They were linked (the link gate covers that) but
  never executed. Their ad-hoc pass/fail text was read only well enough to
  establish that `main_score2` cannot fail; the other 14 were not audited for
  the same defect.
- **Whether the 47 unlinked scatter guests matter.** Nothing references them
  today, so nothing is broken; no attempt was made to link any of them.
- **`scatter_audit.py`'s own correctness.** Its counts were reproduced and its
  method read, but its agreement with `ScatterIncludes.cmake` — which it claims
  is true by construction — was not independently tested.
- **Any X360 matching-build effect.** This lane touched only `native/`-adjacent
  tooling (`tools/`, `docs/`) and edited **no** `src/`, so the matching metric is
  untouched by construction.
