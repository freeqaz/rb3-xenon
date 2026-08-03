# DC3↔RB3 structural matching — CALIBRATED, and the 147 "unpinned TUs" re-read

> **Lane DL-1, 2026-08-03.** Read-only: no source, map or splits change, so this
> **cannot move the metric** and no A/B is reported (`ab_measure` would correctly
> refuse it as absent-vs-absent). Baseline at lane start: 43,664 matched /
> 22,707 masked_equal / 20,957 honest / 39.153187 code%.

## ⛔ TRAP: `tools/bindiff_match.json` is TU0-era and address-INVALID

The file (11,057 DC3↔RB3 BinDiff rows, 2026-05-26) predates the **TU5 flip of
2026-07-15**. Measured on the current tree:

| test | current `band.exe` | `tu0-archive/band.exe` |
|---|---|---|
| `rb3_addr` values that are a `.pdata` **function start** | **3.13%** | **84.89%** |

BinDiff only ever emits function starts, so 96.9% of its keys not being
boundaries is proof of an address-space mismatch, not of a bad matcher.

Calibrated against byte-verified ground truth it scores **0/238 = 0.00%**
agreement, and **0/86 = 0.00%** at its strictest gate (`sim==1.0 &
conf>=0.99`). ★ **That zero is an ARTIFACT of stale addresses.** Reporting it as
a verdict on structural matching would have been a fabricated decisive negative
— the failure shape `INSTRUMENT_DESIGN.md` §2 exists to catch. Any lane
consuming this file must re-key it first (feasible: order-constrained body
hashing TU0→TU5 would resurrect all 11,057 rows; not attempted here).

## The channel, calibrated on the retail PEs directly

No Ghidra and no BinDiff re-run are needed: **both sides are plain PEs** —
`orig/45410914/band.exe` and `dc3-decomp/orig/373307D9/ham_xbox_r.exe`, the
latter beside its leaked `ham_xbox_r.map` (77,404 `.text` names). BinDiff is one
*implementation* of structural matching; the question is whether the
*information* is there.

Instrument: `.pdata` extents (**validated 21,376/21,377 = 100.0%** against
`report.json` sizes — the first decode was wrong and the control caught it) +
opcode-class-masked instruction tokens + 4-gram Jaccard. Ground truth = a named
function at `match_percent_normalized == 100`, i.e. a **byte-verified**
address↔name pairing (15,714 available).

| measurement | value |
|---|---|
| top-1 over 56,893 DC3 candidates | **234/500 = 46.8%** |
| … on token-**IDENTICAL** bodies | **40.7%** |
| … on **DIVERGENT** bodies | **49.4%** |
| **DECOY null** (RB3 fns DC3 provably lacks), top-1 score | med 0.234, **p95 = 1.000** |
| positives surviving a threshold set at decoy-p95 | **26.1%** |
| **scoped** to the correct DC3 `.obj` (median 86 fns) | **80.7%** |
| sabotage leg — a random **wrong** `.obj` | **0.0%** (harness not vacuous) |

⇒ **Unscoped body-level structural matching is NOT usable as a classifier.**
≥5% of RB3 functions DC3 *does not contain* still retrieve a DC3 body at
similarity **1.000**, so a perfect score carries no identity information
(`??_GHamPhotoDisplay` → `??_GTexMovie` at 1.000). This is rule 9 at population
scale: byte identity proves what a function **equals**, not where it **lives**.

★ **Counter-intuitive:** recovery on token-*identical* bodies (40.7%) is **lower**
than on *divergent* ones (49.4%). Identity here means *boilerplate* — `??_G`/`??_E`
deleting dtors, `StaticClassName`, template stamps — identical to dozens of
unrelated DC3 functions, so identity destroys uniqueness. Divergent bodies are
findable precisely because they carry distinctive logic.

★★★ **The bottleneck is LOCATION, not scoring power** (46.8% → 80.7% purely from
a scope). BinDiff's call-graph context is a location prior by another name, so
**re-running BinDiff cannot rescue the unpinned case**: to use structural
matching on an unpinned TU you need a location prior, and obtaining one is
exactly what pinning that TU means. **The channel cannot bootstrap itself.**
Note also that even with a *perfect* scope handed over free by the leaked map,
**19% are still wrong**.

This independently re-derives from the other direction why
`tools/dc3_content_match.py` requires uniqueness on both sides and why
`tools/fuzzy_content_match.py` had to scope per pinned unit.

## ★★★ The 147 unpinned TUs: mostly not RB3 code at all

DK-4 read "0 of 152 bounded on Tier-A evidence" as an identification failure.
Rival hypothesis, tested here: they are **DC3 code we inherited by porting from
`../dc3-decomp` that RB3 retail never contained.** (The list is **147** at HEAD,
not 152.)

Instrument: retail RTTI TypeDescriptor `.?AV<Class>@@` raw-byte presence — pure
Python, because the shell `grep` is binary-blind and would yield only false
negatives here. Control = the **already-pinned** TUs, which are known to be in
RB3.

| group | class present in RB3 retail |
|---|---|
| CONTROL — 919 pinned TUs | **56.1%** |
| TREATMENT — 147 unpinned TUs | **6.1%** |
| | **depletion 9.17×** |

`hamobj` **0/39**, `gesture` **0/20**, `flow` **0/6**, `synth_xbox` 0/7,
`utl` 0/18. **63/147** are present in DC3's image and absent from RB3's;
**127/147** correspond to a real DC3 `.obj`.

Corroborated by an RTTI-independent channel (literal strings): RB3 contains
**none** of `FlowNode`/`FlowPickOne`/`FlowSequence` nor
`HamLabel`/`HamMove`/`HamDriver`/`hamobj`, while DC3 contains all of them — and
the probe fires positive on `BandDirector`/`MasterAudio`/`CharClip`, so it can
say yes. "ham" is Dance Central's own codename (`ham_xbox_r.map`); `gesture` is
Kinect; the Flow absence was already noted independently.

⚠ **POPULATION-level, not per-TU.** 33.8% of *pinned* TUs also read absent
(units defining no RTTI-bearing class), so **do not retire a TU on this alone**.
And pinned TUs were selected partly *for* identifiability — an upward bias on the
control. But 9.17× plus whole-subsystem zeros (0/39, 0/20) is far outside what
selection explains, and it is an order of magnitude above the 1.25×/1.95×
enrichments this project has been burned by.

⇒ **The real unpinned backlog is 9 TUs, not 147.** The other ~138 are inert
scaffolding, and no identification channel — structural, string or otherwise —
can bound a function that is not in the binary.

## Tools (read-only; verified absent from `configure.py`, `tools/project.py`, `objects.json`, `build.ninja`)

- `tools/dl1_structmatch.py` — the calibrated structural matcher + positive
  control + decoy null. Hard-asserts a DC3-map known positive first.
- `tools/dl1_scoped_control.py` — the location-prior experiment, with a
  deliberately-wrong-scope sabotage leg that must read ~0.
- `tools/dl1_presence.py` — RTTI presence census with the pinned-TU control and
  a known-negative assertion.
- `tools/dl1_calibrate_bindiff.py` — calibrates `bindiff_match.json`; retained
  because it is what exposed the TU0 staleness. Its 0.00% is the artifact above.
