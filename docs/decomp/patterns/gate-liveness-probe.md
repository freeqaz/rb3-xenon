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

✅ The 158 `HANDLE_LOCAL_STATIC` gates **were swept by lane DL-4** (2026-08-03) —
see "The HANDLE_LOCAL_STATIC sweep" below. Coverage **158/158, zero unprobeable**;
12 provably-dead gates removed at a measured Δ0.

```
python3 tools/gate_liveness.py band3/tour/TourProgress.cpp
python3 tools/gate_liveness.py --flag RB3_HANDLE_LOCAL_STATIC --quiet-inert <units...>
```

⚠ Cost is 2 real (uncached) compiles per TU. A 441-TU sweep at 8 workers takes
tens of minutes and contends with other lanes' builds.

## The HANDLE_LOCAL_STATIC sweep (lane DL-4, 2026-08-03) — 158/158, 12 dead

The largest gated population, swept in full. **316 uncached compiles, coverage
158/158, zero unprobeable units** (better than the map sweep's 95.9% — every
gated TU has a build edge).

| verdict | n | meaning |
|---|---:|---|
| **LIVE** | **139** | gate changes TU-owned codegen; kept |
| **DEAD** | **12** | obj byte-identical on **all** sections incl. relocations ⇒ removed |
| **NEEDED-TO-COMPILE** | **7** | the *off* leg does not compile at all |
| **INERT** (template-only) | **0** | no shared-template floor exists for this gate |

Two verdict classes differ from the `MAP_0x1C` sweep and both matter:

- **`INERT` is empty.** The template floor was a property of
  `vector<map<int,float>>`, not of gating in general. The HANDLE gate touches no
  STL instantiation, so the `owned` vs `template` split — indispensable for
  `MAP_0x1C` — never fires here. The discriminator is still *required*: you
  cannot know it will be empty until you measure it.
- **`NEEDED-TO-COMPILE` is new and is strictly stronger than LIVE.** Ungated,
  `HANDLE(symbol, func)` expands to `if (sym == symbol)` where `symbol` is a
  **bare identifier**; these 7 TUs were written for the local-static dialect and
  have no global `Symbol` of that name, so removal is a `C2065 undeclared
  identifier` (`update_char_cache`, `has_any_asset_offers`, `view_gamercard`, …).
  A gate the source cannot compile without is not a bookkeeping question.

### The mechanism behind the 12 dead — established, not assumed

`obj/Object.h:1288` defines the `HANDLE` family **unconditionally** through
`_NEW_STATIC_SYMBOL(s)` ≡ `static Symbol _s(#s);`. **Object.h's version is
already the local-static form the gate exists to switch on.** So the dead 12
split into exactly two mechanisms:

| mechanism | n | why the flag is a no-op |
|---|---:|---|
| includes `ObjMacros.h` but uses **zero** gated macros | 7 | nothing to switch |
| never includes `ObjMacros.h` | 5 | `Object.h`'s already-local-static version wins by include order |

⇒ Removal here costs **no faithfulness at all**, which is the crucial difference
from the four `MAP_0x1C` INERT gates that DJ-3/DK-3 correctly **kept**: those
change a shared template's stride with no retail evidence either way, so removal
would have been a guess. These 12 change nothing, and the 5 already emit the
retail shape unconditionally.

⚠ **This was found by chasing a discordance, not by assuming the mechanism.**
`BandDirector.cpp` (**40** gated-macro uses) and `CharClipGroup.cpp` (7) read
DEAD — flatly contradicting the naive "uses the macro ⇒ gate matters" model.
They are two of the five whose `HANDLE` comes from `Object.h`. Had that been
waved off as probe noise, the mechanism would have stayed hidden.

### Three independent instruments, 158/158 agreement

1. **all-sections object byte identity** (bodies *and* relocations) — the proof;
2. a **compile-free static rule**: `DEAD ⟺ (no ObjMacros.h) OR (0 gated uses)`;
3. the **ninja dep closure**: all 139 LIVE include `ObjMacros.h`, **0 counterexamples**.

⚠ Instrument 3 is the proxy CLAUDE.md warns is "wrong 18/231" for SYNC_PROP. It
is clean *here* — but only as corroboration; the byte identity is what proves it.

### Controls — re-run, never inherited

| control | result |
|---|---|
| positive `TourProgress` / `MAP_0x1C` | LIVE `owned=101`, DK-3's exact figure **and** symbol breakdown |
| null `--flag RB3_NULL_CONTROL_XYZZY` | DEAD on three of the **actual target** TUs |
| DK-3's 6 proven-dead SYNCPROP gates | **6/6 reproduce DEAD** under this lane's separate all-sections comparator |

★ The 6/6 also rules out the **opposite vacuity**: had MSVC embedded the command
line in any section (`.drectve`, `.debug$S`), the flag string would differ and
**nothing could ever have read DEAD**. A comparator that can only say LIVE would
have looked exactly like a thorough sweep finding no dead flags.

### A/B — the exact zero IS the control

```
leg A  43664 / 22707 / 20957 / 39.153187
leg B  43664 / 22707 / 20957 / 39.153187     (12 REAL recompiles, configgen re-run)
Δmatched +0  Δmasked_equal +0  Δhonest +0  Δcode% +0.000000  Δfuzzy +0.000000
```

Twelve TUs genuinely recompiled under changed cflags and every scoring key is
unchanged. That is not a null result — a nonzero delta would have **falsified**
the byte-identity proof.

### No WRONG gates, and the reason is structural

`owned` over the 139 LIVE gates runs **54 … 3460, median 293**. The vestigial
low-`owned` tail that produced the map sweep's one WRONG gate
(`TourPerformerLocal`, `owned=6`) **does not exist here** — this population is
bimodal: 0 (the dead 12) or ≥54. So there is no principled ranking for a WRONG
hunt, and the n=1 "low owned ⇒ suspect" rule has nothing to select on.

Six of the lowest-`owned` `band3/` gates were screened individually anyway (never
batched — DK-3 established that ranking does not predict sign). **All six
regressed; 0 WRONG gates.**

| unit removed | owned | Δmatched | Δmasked_equal | **Δhonest** |
|---|---:|---:|---:|---:|
| `band3/tour/TourSavable.cpp` | 54 | −1 | +0 | **−1** |
| `band3/meta_band/SetlistToStorePanel.cpp` | 75 | −2 | −1 | **−1** |
| `band3/game/FadePanel.cpp` | 80 | −3 | −2 | **−1** |
| `band3/meta_band/VoiceoverPanel.cpp` | 87 | −3 | −2 | **−1** |
| `band3/meta_band/BandStoreOffer.cpp` | 91 | −7 | −6 | **−1** |
| `band3/meta_band/InterstitialPanel.cpp` | 91 | −4 | −2 | **−2** |

★ **Report Δhonest separately — the headline overstates the cost here.** Δmatched
spans −1…−7 but Δhonest is **−1 or −2 in every single case**: most of the loss is
`masked_equal` funclet pairings. This is the mirror image of DK-3's RockCentral
result (+6 matched, +6 masked, Δhonest **0**), and the same lesson in the opposite
direction — quoting either figure alone misrepresents the change.

⚠ `owned` did **not** rank the damage (54 → −1 but 91 → −7): consistent with
DK-3's finding that magnitude counts *perturbation*, not lost matches. Do not use
`owned` to prioritise anything but LIVE-vs-DEAD.

Combined with the 7 TUs that do not compile ungated, the evidence is that this
gate is **claimed correctly wherever it is claimed** — the opposite of `MAP_0x1C`,
whose converse sweep hit 1-in-11 with 10× downside.

## What was NOT done
- **The 15 remaining converse candidates were not screened** (table above).
  Expected value is low and it is recorded rather than ground.
- **(DL-4) The 133 unscreened LIVE HANDLE gates were not individually A/B'd.**
  6 of 139 were screened, all regressing. Screening the rest is ~133 A/B runs to
  chase a class whose one known instance had a structural signature (`owned=6`)
  that **provably does not occur in this population** (minimum 54). Recorded with
  numbers so the next lane can decide rather than re-derive.
- **(DL-4) No converse sweep for `HANDLE_LOCAL_STATIC`** — i.e. which *ungated*
  TUs would benefit. That is a different population from this lane's charge, and
  the `MAP_0x1C` converse result (1 positive in 11, worst case −172) is an
  explicit warning against speculative application.
- **(DL-4) The 7 NEEDED-TO-COMPILE units were not investigated further.** They
  are settled as un-removable; whether their *sources* should instead be rewritten
  to the `Object.h` dialect (which would make the gate unnecessary, as it already
  is for the 5 dead ones) is a real question this lane did not open.
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

## The 7 NEEDED-TO-COMPILE gates: dissolved, not just settled (lane DM-3, 2026-08-03)

DL-4 left this open: *should the 7 sources be rewritten to the `Object.h` dialect,
making the gate unnecessary as it already is for the 12 dead ones?* **Yes — and it
costs nothing, because the rewrite does not have to be invented.**

`obj/ObjMacros.h` **already carries** the gate-independent spellings, 12 lines
below the `#endif` that closes the gate:

```c
#define HANDLE_STATIC(sym, func)        { _NEW_STATIC_SYMBOL(sym)    HANDLE(_s, func); }
#define HANDLE_EXPR_STATIC(symbol, expr) { _NEW_STATIC_SYMBOL(symbol) HANDLE_EXPR(_s, expr) }
#define HANDLE_ACTION_STATIC(symbol, expr){ _NEW_STATIC_SYMBOL(symbol) HANDLE_ACTION(_s, expr) }
```

They expand through the **ungated** `HANDLE` family to
`static Symbol _s(#sym); if (sym == _s)` — precisely `Object.h`'s local-static
dialect. So the fix is a spelling change, and the gate falls out.

⚠ These are correct **only when the gate is OFF**: with it on, `HANDLE(_s, func)`
stringizes the token `_s`, comparing against `Symbol("_s")`. That is a live trap
for the **160 amalgam TUs** that `#include` another `.cpp` — a rewritten file
pulled into a still-gated TU compiles and is silently wrong. None of the six
converted units is amalgamated (checked).

### Proof, and the control that validates it

Per TU, obj compiled *(gate on, old spelling)* vs *(gate off, new spelling)* is
byte-identical in **every section body and every relocation**. The only
symbol-table delta is the local static's own `.bss` name `?_hs@…` → `?_s@…`,
**exactly one per COMPILED macro use**.

★ That per-use count is what turned a plausible claim into a checked one.
OvershellSlot showed **101 diffs against 129 macro uses**; chasing the discordance
rather than waving it off found exactly **28 uses inside `#ifdef HX_NATIVE`**,
which the match build never defines. 101 + 28 = 129.

Whole-binary A/B — all six together **and each individually** (settled, real
recompiles each leg):

```
leg A  43665 matched / 22707 masked_equal / 20958 honest / 39.154087 code%
leg B  43665         / 22707              / 20958        / 39.154087
Δmatched +0  Δmasked_equal +0  Δhonest +0  Δcode% +0.000000pp  Δfuzzy +0.000000
units at 100% (matched_functions == total_functions): 206/1022 -> 206/1022
```

Controls re-run, never inherited: positive `TourProgress`/`MAP_0x1C` reproduced
DK-3's **owned=101** and its exact symbol breakdown; the null
`--flag RB3_NULL_CONTROL_XYZZY` read **INERT owned=0 on 4 of the actual target
TUs**; the byte comparator demonstrably fires positive (PrefabMgr, below).

### PrefabMgr is the one that does NOT convert

It is an **amalgam TU** (`#include`s `BandSongMgr.cpp`, `AccomplishmentManager.cpp`,
`MessageTimer.cpp`, `Text.cpp`), and three of those are *separately gated objects*.
Dropping its gate changes the amalgamated code too — the probe reads **7731 vs
7443 sections**. It needs a coupled 4-TU patch and cannot land independently.

### Is the dialect a lever anywhere else? For the METRIC, no — and here is the null

The rewrite was also run against **DL-4's six individually-A/B'd LIVE gates**,
chosen because plain removal there measurably cost **−1…−7 matched**. A positive
control that could fail. **6/6 IDENTICAL (all sections).**

⇒ Holding gate *semantics* fixed, the dialect **spelling** has exactly zero metric
content — 12 units, 0 differences. The only way the `HANDLE` dialect moves the
metric is by genuinely flipping a TU between global-Symbol and local-static, which
is the `/D` application lever already **measured exhausted** (14 TUs ⇒ −2; the
`MAP_0x1C` converse hit 1-in-11 with a −172 worst case). **So: no new lever. Say
it plainly.**

What it *is* is a complete **hygiene** result: **121 of 146** remaining gated TUs
use only `_STATIC`-able macros and are non-amalgam, so the gate could be retired
almost entirely at proven-zero cost. **6 are blocked** — `HANDLE_ACTION_IF` and
`HANDLE_ACTION_IF_ELSE` have no `_STATIC` form (adding two macros would unblock
them); **22 are amalgam-coupled**. Recorded with numbers rather than executed:
121 TUs of churn buys Δ0 and collides with other lanes.

### Coverage correction to DL-4

**`system/synth/Faders.cpp` is UNPROBEABLE today**, so DL-4's "158/158, zero
unprobeable" no longer holds. Both legs fail with
`C1094: '-Zm100' inconsistent with value used to build precompiled header ('-Zm0')`
— the *on* leg too, so it is a probe artifact, not a verdict. Its gate's liveness
is **unknown**.
