# Non-metric liveness probe for per-TU `/D` gates

**Lane DK-3, 2026-08-03.** Tool: `tools/gate_liveness.py`.

## The question this answers

We carry per-TU preprocessor gates (`/DRB3_MAP_0x1C`, `/DRB3_HANDLE_LOCAL_STATIC`,
`/DRB3_SYNCPROP_LOCAL_STATIC`, …) that model retail's per-TU ODR splits. Their
bookkeeping rots: a gate landed for one reason survives a refactor, and nobody
can tell whether it still does anything.

Lane DJ-3 (`7a529a8a`) removed `/DRB3_MAP_0x1C` from each of 11 gated TUs
independently and measured:

| verdict | TUs |
|---|---|
| **NEEDED** (removal regresses) | TourProgress −20 · CharacterCreatorPanel −16 · Campaign −15 · ChooseColorPanel −11 · Tour −7 · MoviePanel −3 |
| **INERT** (exactly 0, zero rows moved) | LightPresetManager · LessonMgr · InterstitialMgr · TourPerformer |
| **WRONG** (removal *gains*) | TourPerformerLocal +1 ← removed and landed |

It declined to remove the four INERT gates, correctly: **"the metric did not
move" is not "the flag does nothing."** The metric is a lossy function of
codegen — a function that is 0% with and without the flag contributes 0 either
way, and a COMDAT that is never paired contributes 0 forever. DJ-3 had no
instrument that could separate *dead* from *merely unmeasured*, and raw `.obj`
byte comparison is a **dead instrument** in this repo. So the question stayed open.

## The instrument

Compile the TU **twice from the repo root** — once with the flag, once without —
and compare the `.text` section bodies plus the `.text` relocation table.

Every known obj-byte-comparison hazard is neutralised **explicitly**, which is
the only reason this comparison is legitimate where the general one is not:

| hazard | neutralisation |
|---|---|
| cache-served objs embed the **populating** worktree's `/Fo` path (4 chars ⇒ ~96,681 differing bytes) | `OBJCACHE=off` on **both** legs — both objs are genuinely compiled here |
| the `/Fo` path string differs between roots | **both legs write the same `/Fo` path**, so the string is byte-identical and cannot fabricate a difference |
| the 4-byte COFF timestamp always differs | the COFF header is never read; only section bodies |
| the PCH consistency signature (~`0x010980`) differs on PCH rebuilds | it lives outside `.text`, and the PCH is not rebuilt between legs |

⚠ The determinism control that is **vacuous** elsewhere — "revert and rebuild
with the cache on", which merely re-serves identical cached bytes — is replaced
by a real null: run with `--flag` set to a name nothing tests. That must report
INERT, and it does. **Without that null the probe would be unfalsifiable**, since
"the bytes differ" is the answer a broken comparator gives for everything.

## The discriminator (raw `.text` identity is too blunt)

The first run reported **LIVE for all 9 TUs it could compile, including all four
DJ-3 called INERT** — with a *zero* byte-count delta. That looked like the
probe firing on everything. It was not; it was a real effect with a shared cause.

Attributing each changed word to its owning COMDAT symbol showed the four INERT
TUs had **byte-identical change signatures**: exactly 119 changed words, identical
delta histogram `{-6241:3, -390:3, -16:3, -4:14, +4:95, +16:1}`, every one of them
inside `stlpmtx_std::vector<stlpmtx_std::map<int,float,…>>` member functions
(`operator=`, `reserve`, `_M_fill_insert_aux`, `_M_erase`, …). The mechanism is
plain in the deltas: `mulli …,0x18` → `mulli …,0x1c` (array stride over
`std::map`) and the reciprocal-division magic constant `0xAAAAAAAB` (÷0x18) →
`0x92492493` (÷0x1c) for pointer difference.

That is a **shared template COMDAT**, emitted identically by many TUs, whose
winner the linker picks arbitrarily. It cannot move *this* unit's pairing.

⇒ **The signal is changed words in TU-OWNED symbols** — changed `.text` COMDATs
whose symbol is not an STL template instantiation.

```
owned == 0  ->  INERT.  The metric CANNOT move. Sound, decisive, non-metric.
owned  > 0  ->  LIVE.   The metric CAN move. DIRECTION IS NOT PREDICTED.
```

The negative side is the strong one: `owned == 0` is a *proof* of metric
inertness. `owned > 0` is only a screen — TourPerformerLocal is LIVE and the
gate was **harmful** there.

## Validation — 11/11 against an independent metric census

| control | result |
|---|---|
| positive: TourProgress (metric −20) | **LIVE**, owned=101 (`Handle` 33w, ctor 21w, `ResetTourData` 6w) |
| null: `--flag RB3_NULL_CONTROL_XYZZY` | **INERT** on both TUs tested ⇒ compile is deterministic and the comparator can fire negative |
| DJ-3's 6 NEEDED | **all LIVE**, owned 17…207 |
| DJ-3's 4 INERT | **all INERT**, owned=0, template=119 |
| DJ-3's 1 WRONG (TourPerformerLocal) | **LIVE**, owned=6 |

Perfect concordance with a census produced by a completely different method.
Magnitudes rank only loosely with the metric penalty (CharacterCreatorPanel
owned=207 for −16; TourProgress owned=101 for −20), which is expected — owned
words count *perturbation*, not *lost matches*.

## Verdict on the four "dead flags"

They are **not dead, and they are not load-bearing either.** Precisely:

- **Inert for the TU's own code** — proven, non-metrically. Removing them cannot
  change any function body this unit owns.
- **Live for one shared template COMDAT** (`vector<map<int,float>>`), which the
  metric shows moves no rows.

So the honest label is **"inert-but-not-inconsequential"**, not "dead flag".

**Recommendation: keep them, and fix the bookkeeping instead.** Removal buys a
measured Δ0 and destroys information. Worse, it is not obviously *correct*: the
gate makes this TU's `vector<map<>>` stride 0x1c, and we have no evidence about
which stride retail's winning COMDAT used. Reverting to 0x18 on a hunch would be
guessing in the direction of *less* faithfulness. Under the standing intent —
equivalent and correct code — an unmeasurable-but-plausibly-faithful gate should
stay until someone adduces retail evidence about that specific template.

`MoviePanel` is worth noting separately: it is LIVE with `template=0`. Its entire
change is in `MetaMusicManager` methods, so it has no `vector<map>` floor at all.
The floor is a property of the instantiation, not of the gate.

## The converse sweep: REOPENED, but thin and dangerous

The converse question — *which currently-ungated TUs NEED the gate?* — was
recorded **CLOSED EMPTY at `7c24a93`**. That verdict is ~2 months old and was
measured at 6,965 matched against 43,644 now. The probe makes re-deriving it
cheap, so it was re-derived.

**It does not survive.** Sweeping all 441 TUs that instantiate `std::map`
(prescreened from built objs):

| | |
|---|---|
| swept | 441 |
| probed successfully | 423 (**95.9%**) |
| **LIVE** | **90** |
| LIVE, ungated, and touching a currently sub-100 function | **26** |

⚠ Coverage gap, stated plainly: 10 of the 18 failures are units with **no build
edge at all** — `AccomplishmentDiscSongConditional` and siblings are not
compiled. That gap is uncomfortable precisely because `AccomplishmentProgress`
is where the original `sizeof(map)==0x1c` evidence came from. 6 were a
regex bug in this tool (fixed, `d97658d0`); 2 were compile failures.

### But LIVE is a WEAK predictor of benefit — 1 in 11

Screening the top 11 candidates by whole-binary `matched_functions`:

| unit | Δmatched | Δcode% |
|---|---:|---:|
| **RockCentral** | **+6** | **+0.002243** |
| DefaultPhysicsManager | 0 | 0 |
| MusicLibrary | −1 | −0.009018 |
| *TourPerformerLocal (negative control)* | *−1* | *−0.007259* |
| DeployCountTracker | −6 | −0.011001 |
| VocalTrack | −12 | −0.031398 |
| RGTrainerPanel | −14 | −0.030910 |
| PerfectSectionTracker | −15 | −0.019458 |
| CustomizePanel | −25 | −0.036190 |
| BandList | −54 | −0.074432 |
| VocalTrackDir | **−172** | −0.196244 |

The negative control validates the screen end-to-end: **TourPerformerLocal
reads −1**, exactly mirroring DJ-3's independently measured +1-on-removal.

⇒ **One positive in eleven, and the downside is an order of magnitude larger
than the upside.** The gate is not a free win where applicable — it is a claim
about a specific TU's ODR layout, and that claim is usually *false*. The vein is
reopened but thin. **Never apply this gate speculatively or in a batch**; screen
per-unit first. Ranking candidates by penalty bytes did NOT predict sign
(VocalTrack and VocalTrackDir were ranked 1 and 2 and are the two worst losses).

### The 15 unscreened candidates (do not re-run the sweep)

Screened and resolved above: 11. These 15 remain, ranked by penalty bytes —
but note that ranking did not predict sign, so treat the order as arbitrary and
screen each one. `Scheduler.cpp` is Quazal, i.e. inside the `/Od` region and
explicitly low-value per the standing scope directive.

| unit | penalty B | owned words |
|---|---:|---:|
| `network/Core/Scheduler.cpp` | 589 | 9 |
| `band3/bandtrack/TrackPanel.cpp` | 99 | 125 |
| `band3/meta_band/ProfileMgr.cpp` | 98 | 2 |
| `band3/meta_band/BandProfile.cpp` | 51 | 359 |
| `band3/game/FocusTracker.cpp` | 29 | 85 |
| `system/bandobj/GemTrackDir.cpp` | 24 | 365 |
| `band3/tour/TourDescPanel.cpp` | 22 | 5 |
| `band3/bandtrack/GemTrack.cpp` | 22 | 4 |
| `system/rndobj/EventTrigger.cpp` | 14 | 2 |
| `band3/meta_band/UIStats.cpp` | 9 | 25 |
| `band3/meta_band/SongRecord.cpp` | 8 | 55 |
| `band3/game/OverdriveTracker.cpp` | 5 | 48 |
| `band3/game/PerfectOverdriveTracker.cpp` | 4 | 25 |
| `band3/bandtrack/GemManager.cpp` | 0 | 178 |
| `band3/meta_band/MetaPanel.cpp` | 0 | 7 |

Given the measured 1-in-11 hit rate and the size of the losses, the expected
value of grinding this list is low. It is recorded so the next lane can decide
with numbers rather than re-derive them.

## Reuse — demonstrated, not asserted: 6 genuinely dead SYNCPROP gates

The tool takes `--flag`, so it generalises to every per-TU gate we carry. We
carry a lot, and nobody has ever checked their liveness:

| gate | gated TUs |
|---|---:|
| `/DRB3_HANDLE_LOCAL_STATIC` | 158 |
| `/DRB3_SYNCPROP_LOCAL_STATIC` | 40 |
| `/DRB3_MAP_0x1C` | 11 |
| `/DRB3_STRIP_CHEAT_HANDLERS` | 9 |
| others (`NOTIFY_ONCE_EVAL`, `LOG_NO_EVAL`, …) | 8 |

Sweeping all **40** `SYNCPROP_LOCAL_STATIC` TUs: **34 LIVE, 6 INERT, 0 errors.**

And unlike the `MAP_0x1C` INERT four — which are inert only for *owned* code and
still move a shared template COMDAT — these six have **`owned=0` AND
`template=0`**. A follow-up comparison over **every section** (not just `.text`:
bodies *and* relocations, `.data`/`.rdata`/`.bss` included) reports
**IDENTICAL** for all six:

```
system/char/CharUpperTwist.cpp       IDENTICAL (all sections)
system/rndobj/Group.cpp              IDENTICAL (all sections)
system/char/CharInterest.cpp         IDENTICAL (all sections)
system/char/CharSignalApplier.cpp    IDENTICAL (all sections)
system/rndobj/Wind.cpp               IDENTICAL (all sections)
system/char/CharClipGroup.cpp        IDENTICAL (all sections)
```

The compiler emits a byte-identical object with and without the flag. That is a
**proof** of deadness — the thing DJ-3 correctly said it could not obtain from a
silent metric — and it makes these six removable as pure hygiene at zero risk.
Removed here; the A/B is the control (see below).

⚠ The 158 `HANDLE_LOCAL_STATIC` gates were **not** swept — see "What was not
done".

```
python3 tools/gate_liveness.py band3/tour/TourProgress.cpp
python3 tools/gate_liveness.py --flag RB3_HANDLE_LOCAL_STATIC --quiet-inert <units...>
```

⚠ Cost is 2 real (uncached) compiles per TU. A 441-TU sweep at 8 workers takes
tens of minutes and contends with other lanes' builds.

## What was NOT done

- **The 158 `/DRB3_HANDLE_LOCAL_STATIC` gates were not swept.** That is the
  largest gated population by a factor of four and the most likely place for
  more provably-dead flags, on the evidence that 6 of 40 SYNCPROP gates were
  completely dead. It is ~316 uncached compiles, deferred purely for budget.
  Nothing about it is hard; the tool takes `--flag`.
- **The 15 remaining converse candidates were not screened** (table above).
  Expected value is low and it is recorded rather than ground.
- **The 4 `MAP_0x1C` INERT gates were NOT removed** — deliberately. They are
  inert for owned code but do change a shared template COMDAT, so unlike the
  six SYNCPROP flags they are not byte-identical and the deadness proof does
  not apply. Removal buys a measured 0 and we have no retail evidence about
  which stride that template had, so reverting 0x1c→0x18 would be guessing
  toward *less* faithfulness. The complaint that motivated the question was
  that the bookkeeping had rotted; documenting them fixes that without the
  guess.
- **10 map-using units could not be probed at all** because they have no build
  edge (`AccomplishmentDiscSongConditional` and siblings are not compiled).
  Uncomfortable, because `AccomplishmentProgress` is the origin of the whole
  `sizeof(map)==0x1c` finding. Not chased.
- **No claim is made about which stride retail actually used** for
  `vector<map<int,float>>`. The probe answers "does this flag change codegen",
  never "is this flag correct".
