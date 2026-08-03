# Non-metric liveness probe for per-TU `/D` gates

**Lane DK-3, 2026-08-03.** Tool: `tools/gate_liveness.py`.

> ★★★ **COMPARATOR CHANGE 2026-08-03 (lane DO-4) — every `owned`/`template`
> figure written in this file BEFORE that date is a **v1** figure, and v1 was
> defective.** The comparator paired the two legs' sections by **index**
> (`zip`), which misaligns after any inserted section and never compares past
> the shorter object. `owned` totals changed on **202 of 250** units (median
> |Δ|/v2 = **32.7%**, max **1062%**).
> **The LABELS did not move: 0 label flips in 250** (LIVE 223 · DEAD 21 · INERT 5).
> So every **verdict** in this document stands; the **magnitudes** are re-based.
> Old figures are reproducible on demand with `--comparator positional`, and
> every run now prints a banner naming the comparator.
> Full old → new table and the proof: **"THE COMPARATOR WAS PAIRING SECTIONS BY
> INDEX"** at the end of this file.

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

| control | result (v1 → **v2**) |
|---|---|
| positive: TourProgress (metric −20) | **LIVE**, owned=101 → **108** (`Handle` 33w, ctor 21w, `ResetTourData` 6w — per-symbol breakdown **IDENTICAL** under both) |
| null: `--flag RB3_NULL_CONTROL_XYZZY` | **INERT** on both TUs tested ⇒ compile is deterministic and the comparator can fire negative (DO-4: re-run on **5** real target TUs, all owned=0) |
| DJ-3's 6 NEEDED | **all LIVE**, owned 17…207 → **17…231** |
| DJ-3's 4 INERT | **all INERT**, owned=0, template=119 → **unchanged 4/4** |
| DJ-3's 1 WRONG (TourPerformerLocal) | **LIVE**, owned=6 → **6** (unchanged) |

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

| unit | penalty B | owned words (v1) | **v2** |
|---|---:|---:|---:|
| `network/Core/Scheduler.cpp` | 589 | 9 | 9 |
| `band3/bandtrack/TrackPanel.cpp` | 99 | 125 | **145** |
| `band3/meta_band/ProfileMgr.cpp` | 98 | 2 | 2 |
| `band3/meta_band/BandProfile.cpp` | 51 | 359 | **366** |
| `band3/game/FocusTracker.cpp` | 29 | 85 | 85 |
| `system/bandobj/GemTrackDir.cpp` | 24 | 365 | **411** |
| `band3/tour/TourDescPanel.cpp` | 22 | 5 | 5 |
| `band3/bandtrack/GemTrack.cpp` | 22 | 4 | 4 |
| `system/rndobj/EventTrigger.cpp` | 14 | 2 | 2 |
| `band3/meta_band/UIStats.cpp` | 9 | 25 | 25 |
| `band3/meta_band/SongRecord.cpp` | 8 | 55 | 55 |
| `band3/game/OverdriveTracker.cpp` | 5 | 48 | 48 |
| `band3/game/PerfectOverdriveTracker.cpp` | 4 | 25 | 25 |
| `band3/bandtrack/GemManager.cpp` | 0 | 178 | 178 |
| `band3/meta_band/MetaPanel.cpp` | 0 | 7 | 7 |

★ **(DO-4) This table doubles as a fidelity control on the legacy comparator:
re-running all 15 under `--comparator positional` reproduced the published
`owned words` column EXACTLY, 15/15.** Only 3 change under v2, because `MAP_0x1C`
rarely inserts a section — the defect is concentrated in gates that add
function-local statics (`HANDLE`/`SYNCPROP`), not in this one.

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

`owned` over the 139 LIVE gates runs **54 … 3460, median 293**
*(v2: **75 … 3965, median 377** — the argument below is **strengthened**, since the
floor rises 54 → 75 and moves further from `TourPerformerLocal`'s 6)*. The vestigial
low-`owned` tail that produced the map sweep's one WRONG gate
(`TourPerformerLocal`, `owned=6`) **does not exist here** — this population is
bimodal: 0 (the dead 12) or ≥54 (v2: ≥75). So there is no principled ranking for a WRONG
hunt, and the n=1 "low owned ⇒ suspect" rule has nothing to select on.

Six of the lowest-`owned` `band3/` gates were screened individually anyway (never
batched — DK-3 established that ranking does not predict sign). **All six
regressed; 0 WRONG gates.**

| unit removed | owned (v1) | **owned (v2)** | Δmatched | Δmasked_equal | **Δhonest** |
|---|---:|---:|---:|---:|---:|
| `band3/tour/TourSavable.cpp` | 54 | **77** | −1 | +0 | **−1** |
| `band3/meta_band/SetlistToStorePanel.cpp` | 75 | **106** | −2 | −1 | **−1** |
| `band3/game/FadePanel.cpp` | 80 | **97** | −3 | −2 | **−1** |
| `band3/meta_band/VoiceoverPanel.cpp` | 87 | **143** | −3 | −2 | **−1** |
| `band3/meta_band/BandStoreOffer.cpp` | 91 | **242** | −7 | −6 | **−1** |
| `band3/meta_band/InterstitialPanel.cpp` | 91 | **192** | −4 | −2 | **−2** |

★ **Report Δhonest separately — the headline overstates the cost here.** Δmatched
spans −1…−7 but Δhonest is **−1 or −2 in every single case**: most of the loss is
`masked_equal` funclet pairings. This is the mirror image of DK-3's RockCentral
result (+6 matched, +6 masked, Δhonest **0**), and the same lesson in the opposite
direction — quoting either figure alone misrepresents the change.

⚠ `owned` did **not** rank the damage (54 → −1 but 91 → −7): consistent with
DK-3's finding that magnitude counts *perturbation*, not lost matches. Do not use
`owned` to prioritise anything but LIVE-vs-DEAD.

⚠⚠ **(DO-4) The comparator fix does NOT rescue `owned` as a damage ranker — stated
explicitly because the opposite is the tempting claim.** It would have been easy to
present v2 as "now the magnitudes rank the damage": v2 does break v1's 91/91 tie in
the right direction (242 → −7 above 192 → −4). But scored properly over these six,
Spearman ρ is **0.97 for v1 and 0.90 for v2** — v2 is nominally *worse*, and at
**n = 6 neither is distinguishable from chance**. **DL-4's warning stands unchanged.**
What the fix buys is that `owned` is now a *correct* measure of perturbation, not a
*useful* measure of damage. (INSTRUMENT_DESIGN shape 5: an improvement is exactly
where a false positive is cheapest to believe.)

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

### ⛔ Coverage correction to DL-4 — RAISED BY DM-3, **WITHDRAWN** BY DN-3/DN-2

DM-3 reported **`system/synth/Faders.cpp` UNPROBEABLE**, both legs dying with
`C1094: '-Zm100' inconsistent with value used to build precompiled header ('-Zm0')`,
and concluded DL-4's "158/158, zero unprobeable" no longer held.

**That conclusion is withdrawn. DL-4's 158/158 STANDS.** The C1094 is neither a
tool bug nor a property of the TU — it is an **environment precondition**, and
DM-3's own report contained the tell: *the `on` leg failed too*, which makes it a
probe artifact rather than a verdict.

## ★★ THE TRUNCATED-PCH TRAP (lane DN-3 root cause, DN-2 corroboration, 2026-08-03)

**`scripts/setup_worktree.sh:323` deliberately truncates the reflinked PCH to
zero bytes:**

```sh
rm -f "$WT_BUILD/pch/decomp_pch.obj"
: > "$WT_BUILD/pch/system.pch"      # <-- 0-byte placeholder, ON PURPOSE
```

It is a placeholder so `cl.exe` **overwrites** `system.pch` rather than creating
it (WIBO_FS_CACHE cannot create a new file under the case-insensitive VERSION
dir). Ninja rebuilds it on first need — but **an out-of-band compile never
triggers that edge**, meets the 0-byte PCH, and dies `C1094`.

**Remedy — one command, no code change:**

```sh
ninja build/45410914/pch/system.pch
```

With the PCH built, `Faders.cpp` probes **LIVE, `owned=376`** (DN-3; reproduced
exactly by DN-2 with the shipped tool). `MessageTimer`, `FlowMultiSetProperty`
and `MetaMusic` unblock the same way.
⚠ **(DO-4) `376/211` is a v1 figure and it is the single worst one in this file:
corrected it is `owned=118, template=0`.** `Faders.cpp` is where the pairing defect
was reproduced — off=389 sections vs on=396, **188 of 389 `zip()` pairs comparing
two different symbols**. The `template=211` was pure mis-pairing artifact; the HANDLE
gate touches no STL instantiation, so its true template floor is **0**. The verdict
(LIVE) is unaffected.

⚠ **This affects ANY lane compiling a PCH-eligible TU outside ninja in a fresh
worktree** — the nine dirs `hamobj synth flow gesture meta obj os utl movie`.
Not just this probe: `scripts/harvest/class_layout_report.py` hits it too.

★ It also **renames a mystery that had been recorded as intermittency**. DL-2
observed that the C1094 "fires only in a fresh reflinked worktree and disappears
after a full `ninja`". That was never warm-PCH luck — it was ninja **rebuilding
the truncated PCH**. Same root cause, now named.

### DN-2 side-findings (measured, recorded rather than acted on)

- **Exposure is 379 of 380.** 380 TUs sit on the `msvc_pch` rule; exactly **one**
  is immune, `system/meta/MoviePanel.cpp`, because it carries `/Y-` in its own
  `objects.json` `extra_cflags`. That single per-TU workaround is why the failure
  looked *non-uniform* across TUs in a fresh worktree (Faders, NetLoader_Xbox and
  obj/Object.cpp all died while MoviePanel probed cleanly).
  ⇒ ⚠ **MoviePanel is therefore a VACUOUS choice of PCH-path control** — it cannot
  exhibit the defect under any PCH state, so a PASS from it certifies nothing
  (INSTRUMENT_DESIGN shape 1). Use `Faders.cpp` and assert the control TU does not
  already carry `/Y-`.
- ⛔ **Do NOT "fix" this inside the probe.** A `/Y-` change to `gate_liveness.py`
  was written, controlled and then **reverted**: the tool's whole method is
  *compile the same TU twice and compare bytes*, so perturbing PCH handling risks
  the comparison in order to avoid one `ninja` invocation, and it would **mask an
  environment problem inside the instrument**. Measured cost of that reverted
  change, for the record: over **30 `msvc_pch` TUs** probed `/Y-` vs `/Yu`,
  **LABEL agreement 100%**, but `owned`/`template` MAGNITUDES differed on 4
  (Faders `376/211` under `/Yu` vs `176/22` under `/Y-`), because `/FI` vs PCH
  perturbs untracked helper/template inlining. So the revert also preserves
  comparability with every published `owned` figure.
- ⚠ **STILL UNFIXED, and it is a real instrument defect:** `compare_objs` pairs
  the two legs' sections with `zip(A, B)`. That misaligns after any **inserted**
  section *and* — because `zip` truncates to the shorter list — **never compares
  sections past the shorter object's count**. Gates that add a function-local
  static change the section SET, so this fires in the wild (measured on
  `Faders.cpp` + `/DRB3_HANDLE_LOCAL_STATIC`). A symbol-keyed prototype
  reproduced DK-3's per-symbol figures (Handle 33w / ctor 21w / ResetTourData 6w),
  DK-3's 119-word template floor **4/4**, and DL-4's six DEAD gates **6/6**, while
  changing `owned` TOTALS (TourProgress 101 → 108). **No label flipped.** Recorded
  with numbers so a future lane can decide rather than re-derive — landing it
  would re-base every published `owned` total, which is not free while other
  lanes are quoting them.
  ✅ **FIXED AND MIGRATED by lane DO-4** (2026-08-03) — see
  "THE COMPARATOR WAS PAIRING SECTIONS BY INDEX" at the end of this file. DN-2's
  108 reproduced exactly; both comparators now ship (`--comparator positional`
  reproduces any pre-DO-4 figure); 0 label flips over 250 units.

## Lane DN-3 (2026-08-03): the predicate was wrong, PrefabMgr is not viable, and `_STATIC` was silently miscompiling

> ⚠ **COMPARATOR PROVENANCE.** Every `owned`/attributed word-count in this section
> was measured with the **v1 positional** comparator, which lane DO-4 later proved
> defective. v2 values are given inline as `v1 N → v2 M`. **Every conclusion in
> this section survives the correction** — that was checked, not assumed (§6).
> This section was written for commit `1c09c7d5` but its patch collided with DN-2's
> edit to the same file and was dropped; merged here by DO-4.

### 1. `HANDLE_*_STATIC` was SILENTLY WRONG under the gate — fixed, measured Δ0

DM-3 flagged this as "a live trap for the 160 amalgam TUs". It is worse than a
trap for amalgams: it was a **latent silent miscompile for the whole dialect**.
Preprocessing `band3/meta_band/CharSync.cpp` (`cl /E`) under both gate states:

```
gate OFF   { static Symbol _s("update_char_cache"); if (sym == _s) { (UpdateCharCache()); return 0; } }
gate ON    { static Symbol _s("update_char_cache");                                  // emitted, never compared
             { static Symbol _hs("_s"); if (sym == _hs) { (UpdateCharCache()); return 0; } } }
```

Under the gate the handler compares against `Symbol("_s")` and is **unreachable**.
It compiles, it links, and **the metric cannot see it** — the only difference is
the string relocation argument, which `match_percent_normalized` masks (CLAUDE.md,
"Reloc args are SCORE-INVISIBLE"). Correctness-not-metric class.

Fixed by writing the `_STATIC` family **per gate state**: under the gate the plain
`HANDLE` family already stringizes into a function-local `static Symbol _hs`, so
the `_STATIC` forms must **forward the name** rather than wrap it. The identical
hole existed under `obj/dialect_object_push.h` — its Object.h-dialect `HANDLE` also
stringizes — and is closed symmetrically.

**Exposure was 0** (census: no gated object's include closure contained a `_STATIC`
use; the only shimmed body, `rndobj/Text.cpp`, uses none), so the fix is Δ0 *by
construction*:

```
leg A  43668 matched / 22707 masked_equal / 20961 honest / 39.156220 code%
leg B  43668         / 22707              / 20961        / 39.156220     (337 real recompiles)
Δmatched +0  Δmasked_equal +0  Δhonest +0  Δcode% +0.000000pp  Δfuzzy +0.000000
```

The exact zero **is** the control — a nonzero delta would have falsified the census.
It matters prophylactically because the COMDAT-scatter workflow is *actively* adding
`#include "*.cpp"` edges, and scatter-including any of the 12 `_STATIC`-using files
(`OvershellSlot.cpp` alone has **129** sites) into a gated TU would have silently
disabled that many handlers.

⚠ **Residual, NOT closed:** a third dialect exists — a TU that includes `ObjMacros.h`
(so `HANDLE_STATIC` is defined) but where **`Object.h`'s stringizing `HANDLE` wins by
include order**, compiled *ungated*. There the wrapping form still yields
`Symbol("_s")`. Zero instances today, and closing it needs a self-contained `_STATIC`
definition that no longer forwards to `HANDLE` — which would **decouple** the two
families and let them drift. Left coupled deliberately; documented instead.

### 2. The convertibility predicate is on the CONSUMER SET, not on amalgam-ness

DM-3 screened "is this TU an amalgam?" That is **necessary but not sufficient**. The
`_STATIC` spelling is a property of a **file**; the gate is a property of an
**object**. When one file is compiled into objects with *different* gate states, no
spelling can satisfy both — `HANDLE_STATIC` is unconditionally local-static.

The correct predicate: *file `f` is rewritable iff **every** object whose include
closure contains `f` currently carries the gate.*

Sharpest counterexample: **`band3/tour/TourSavable.cpp` has zero `.cpp` includes** —
not an amalgam by any definition, and DL-4 individually A/B'd it (−1 on removal) —
yet `system/os/AsyncFile.cpp` (gate **OFF**) scatter-includes it. Converting it
changes `AsyncFile.obj`. Measured: AsyncFile `owned=54` (**v2: 77**), and **all 54
(v2: all 77) changed words are in `TourSavable` symbols**.

Re-derived over all **140** gated objects at `fac3e802`:

| class | n |
|---|---:|
| **convertible** (consumer set closed, no `_IF`) | **111** |
| blocked by **gate-OFF consumer conflict** | **24** |
| blocked by `HANDLE_ACTION_IF`/`_IF_ELSE` (no `_STATIC` form) | **5** |

★ The screen is a **conservative screen, not a proof** — it is structural, and the
probe adjudicates. Sampling 12 of the 24 blocks and **attributing changed words to
the embedded class** (not merely reading LIVE — a consumer can be LIVE for its own
handlers). ⚠ Note this is a *class-attributed* count, **not** the `owned` total;
`owned ≥ attributed`, and under v1 the gap was mostly mis-pairing spray:

| consumer / embedded class | v1 attributed | **v2 attributed** |
|---|---:|---:|
| `AsyncFile` / `TourSavable` | 54 | **77** |
| `Timer` / `Campaign` | 879 | **1613** |
| `DataNode` / `BandSongMetadata` | 178 | **395** |
| `Gem` / `OutfitConfig` | 525 | **199** |
| `UIList` / `GemTrack` | 374 | **365** |
| `CharIKFingers` / `SongSectionController` | 389 | **440** |
| `Accomplishment` / `BandCrowdMeter` | 311 | **453** |
| `CubeTex` / `AppLabel` | 748 | **1462** |
| `BandCharDesc` / `BandWardrobe` | 320 | **777** |
| `MeshDeform` / `BandUI` | 1246 | **1371** |
| *screen false positives:* `system/meta/StoreOffer.cpp`, `system/rndobj/Morph.cpp` | 0 | **0** |

⇒ ~83% precision; ~20 of the 24 blocks are real. **All ten stay > 0 and both false
positives stay 0 under v2, so this verdict is unchanged.** The two INERT cases
corroborate DL-4's mechanism #2 (their `HANDLE` comes from `Object.h`, so the gate
is a no-op) — and they are exactly the residual third dialect in §1, so they are
**not** freely convertible either.

★ v2 additionally **sharpens** the argument: for **8 of the 10**, v2's `owned`
equals its attributed count exactly, i.e. *every* changed word lives in the embedded
class. Under v1 that was never visible, because mis-paired sections sprayed
word-counts onto unrelated symbols (`UIList` v1 `owned=3555` vs attributed 374).

### 3. PrefabMgr: NOT VIABLE — and it is not a 4-TU patch

DM-3 recorded "needs a coupled 4-TU patch". Measured, the closure is **10 objects**,
and **5 of them are gate-OFF consumers**, each independently confirmed gate-sensitive
*with the change attributed to the embedded class*:

| gate-OFF consumer | v1 `owned` | **v2 `owned`** | top changed symbol |
|---|---:|---:|---|
| `system/rndobj/PropAnim.cpp` | 1707 | **1841** | `?Handle@GemPlayer@@…` 778 w |
| `system/flow/FlowMultiSetProperty.cpp` | 968 | **2267** | `?Handle@Band@@…` 157 w |
| `system/obj/MessageTimer.cpp` | 852 | **1841** | `?Handle@GemPlayer@@…` 778 w |
| `system/rnddx9/Rnd_Xbox.cpp` | 771 | **770** | `?Handle@AccomplishmentManager@@…` 371 w |
| `system/synth/MetaMusic.cpp` | 240 | **157** | `?Handle@MetaMusic@@…` + GemPlayer funclets |

All five remain LIVE under v2 ⇒ **the "not viable" verdict is unchanged.**

The rewrite set is **5 files / 103 macro sites** (`PrefabMgr` 3, `BandSongMgr` 23,
`AccomplishmentManager` 21, `GemPlayer` 55 — reached via `MessageTimer.cpp`, which
DM-3's 4-file list missed — and `BandSwatch` 1). `rndobj/Text.cpp` is **not** in it:
it is wrapped in `dialect_object_push.h` inside PrefabMgr and is therefore
gate-independent there (probed standalone: **DEAD, byte-identical on all sections**).

★ PrefabMgr's off-leg failure is exactly **one** error —
`C2065: 'assign_prefabs_to_slots' : undeclared identifier`. One missing global forces
a gate that collaterally governs **99 macro sites in three foreign bodies**.

The only shape that works is a **local dialect shim**: convert PrefabMgr's own 3
sites and wrap its four embedded `#include`s in a push/pop restoring the *gated*
ObjMacros forms (exactly parallel to the existing `dialect_object_push.h`). That
retires one flag in exchange for ~130 lines of duplicated macro definitions to keep
in lockstep with `ObjMacros.h`, and relocates the silent-wrong hazard from a flag
visible in `objects.json` into a header buried at an include site. **Declined as a
bad trade.** PrefabMgr's gate is genuinely LIVE and genuinely needed-to-compile —
the gate list is already *honest* about it; converting would only make it *shorter*.

### 4. `Faders.cpp` is NOT unprobeable — DM-3's coverage correction is WITHDRAWN

Superseded in place by **"★★ THE TRUNCATED-PCH TRAP"** above, which carries the same
root cause (`setup_worktree.sh:323` truncates the reflinked PCH to 0 bytes) plus
DN-2's exposure census (379 of 380 TUs; `MoviePanel` is the one `/Y-` immune unit and
therefore a vacuous PCH control). **DL-4's "158/158, zero unprobeable" stands.**

### 5. Judgement on the 111: DECLINED

- The churn does not retire the mechanism. 29 objects keep the gate regardless, so
  `ObjMacros.h`'s `#ifdef` block — and the dialect — survive in full. The list gets
  **shorter, not more honest**; §2's classification is what makes it honest, at
  ~1/100th the cost.
- 111 files of mechanical edits collide directly with the active COMDAT-scatter
  lanes editing these same `band3/` sources.
- Δ0 by construction, so there is no yield to offset either.

### What lane DN-3 did NOT do
- **Did not convert the 111.** All-or-nothing per the brief; reasoning above.
- **Did not add `HANDLE_ACTION_IF_STATIC`/`_IF_ELSE_STATIC`.** They would unblock 5
  TUs inside a conversion that is not happening — dead code in a shared header.
- **Did not close the third dialect** (§1 residual). Zero instances; the fix
  decouples the `_STATIC` and `HANDLE` families.
- **Did not probe 12 of the 24 blocked consumers** — 12 were sampled and the
  precision figure is reported instead of a per-row verdict.
- **Did not touch `tools/gate_liveness.py` or `scripts/setup_worktree.sh`** (§4 is a
  caller-side diagnosis; those files belong to another lane).

### 6. (DO-4) Does the comparator fix change any DN-3 conclusion? No — checked

| DN-3 claim | depends on | after the v2 correction |
|---|---|---|
| §2 predicate is the consumer set, not amalgam-ness | `TourSavable`'s words appear in `AsyncFile` | **holds** — 54 → 77, still 100% attributed to `TourSavable` |
| §2 ~83% precision, 10 genuine / 2 false positives | 10 blocks > 0, 2 blocks = 0 | **holds** — all 10 > 0, both FPs still exactly 0 |
| §3 PrefabMgr not viable (5 gate-OFF consumers) | all 5 LIVE | **holds** — all 5 LIVE |
| §1 exposure 0, Δ0 A/B | preprocessor census + `ab_measure` | **unaffected** — not a probe measurement |

## ★★★ THE COMPARATOR WAS PAIRING SECTIONS BY INDEX (lane DO-4, 2026-08-03)

DN-2 found this defect, prototyped the fix, and **correctly declined to ship it** —
because landing it re-bases every published `owned` figure while other lanes are
quoting them. That judgement was right, and it is why this section exists: the
deliverable is not the fix, it is the **migration**.

### The defect, reproduced BEFORE the fix

The comparator paired the two legs' sections with `zip(A, B)`. **Three** independent
bugs, and the first two fire exactly when a gate adds a function-local static — the
case the tool is most often pointed at:

| # | bug | consequence |
|---|---|---|
| 1 | **misalignment** — a local static inserts `.bss`/`.rdata`/`.pdata` **between** `.text` sections | `zip()` compares a `.text` body against a `.bss` body and bills the garbage word-count to the `.text` symbol |
| 2 | **truncation** — `zip()` stops at the shorter list | sections past the shorter object's count are **never compared**; a `.text` COMDAT the gate *adds* can be invisible |
| 3 | **tail loss** — the word loop ran to `min(len)−3` | the tail of any **size-changed** COMDAT was silently dropped |

Reproduced on `system/synth/Faders.cpp` + `/DRB3_HANDLE_LOCAL_STATIC`:

```
sections: off=389  on=396
zip() compares 389 pairs; 7 sections of the longer obj NEVER COMPARED
zip() pairs comparing DIFFERENT symbols:  188 of 389   (48%)
shipped tool verdict: owned=376 template=211   <-- the published Faders figure
```

The `.text` **set** is identical (231 symbols both legs, zero anon, zero duplicate
keys); it is the 7 inserted **non-`.text`** sections that shear the alignment.

⚠ Bug 2 is the dangerous one, because `owned == 0` is the tool's **one decisive
verdict** ("the metric CANNOT move"). A truncating comparator can manufacture it.

Bug 3's sole witness in the whole population — worth recording because it is
independent of pairing and would have survived a pairing-only fix:
`?GetAverageTestTime@CalibrationPanel@@QAAHXZ` shrinks **220 → 216 B** under
`/DRB3_LOG_NO_EVAL`; v1 counts **42** changed words, v2 counts **43**.

### The fix

Pair `.text` COMDATs **by symbol**; count a section present in only one leg as
wholly changed; count words to `max(len)`. Keyability was verified, not assumed:
**0 anon and 0 duplicate `.text` symbols** across the population. Unkeyable input
**REFUSES** rather than returning a number (INSTRUMENT_DESIGN rule 6).

**Both comparators ship**, so no published figure becomes unreproducible:

```
--comparator symbol       v2, correct, DEFAULT
--comparator positional   v1, legacy, reproduces any pre-DO-4 figure verbatim
```

Every run prints a **banner** naming the comparator, every result dict carries a
`comparator` key, and v1's rows print their own mis-pair count. A number with no
comparator attached is a v1 number.

### The migration, measured over 250 units

Both comparators are scored from the **same compile pair**, so the delta is
attributable purely to pairing.

| | |
|---|---:|
| units probed | **250** |
| **LABEL FLIPS** | **0** (LIVE 223 · DEAD 21 · INERT 5) |
| `owned` total changed | **202 / 250** |
| median \|v1−v2\|/v2 · p90 · max | **32.7% · 138.7% · 1062%** |
| v1 mis-paired ≥1 section | **200 / 240** (max **3,781** mis-pairs) |
| legs with different section counts | **194 / 240** |
| misaligned **and** number changed | **200 / 200** |
| not misaligned **and** number changed | **1 / 40** (the `CalibrationPanel` size case) |

⇒ Misalignment is very nearly a **deterministic** predictor of a wrong magnitude,
and its absence of a right one.

★★ **The LABELS were robust to the defect; the MAGNITUDES were not.** That is the
whole reason three lanes' controls passed over it: the label is a coarse threshold
(`owned > 0`) on a quantity that mis-pairing inflates rather than zeroes, so garbage
and signal both clear the bar. It also **vindicates DL-4's standing advice** — "do
not use `owned` to prioritise anything but LIVE-vs-DEAD" — which was the right call
on numbers that were, it turns out, mostly noise.

⚠ **Zero flips is a measurement, not a structural guarantee.** The selftest
demonstrates the two comparators returning **opposite labels** on synthetic input in
*both* directions, so they can disagree; they simply do not on this population.

### Worst movers (v1 → v2), with mis-pair counts

| unit | v1 | v2 | mis-paired pairs |
|---|---:|---:|---:|
| `system/bandobj/BandCamShot.cpp` | 5414 | **813** | 3781 |
| `system/ui/UIList.cpp` | 3555 | **365** | 2847 |
| `band3/meta_band/ViewSetting.cpp` | 3460 | **583** | 2554 |
| `band3/meta_band/MusicLibrary.cpp` | 1664 | **3965** | 385 |
| `band3/meta_band/OvershellPanel.cpp` | 1262 | **3630** | 136 |
| `system/track/TrackDir.cpp` | 1767 | **152** | 1393 |
| `band3/meta_band/ProfileMgr.cpp` | 1192 | **2697** | 171 |
| `system/bandobj/VocalTrackDir.cpp` | 680 | **2491** | 211 |

The error is **not** a consistent bias — it runs both ways, which is why no
correction factor could have salvaged a v1 figure.

### Controls, re-run rather than inherited

| control | v1 | **v2** | verdict |
|---|---|---|---|
| positive `TourProgress` / `MAP_0x1C` | owned=101 | **108** | LIVE, unchanged; **per-symbol breakdown IDENTICAL** (`Handle` 33w, ctor 21w, `ResetTourData` 6w) |
| null `--flag RB3_NULL_CONTROL_XYZZY` | owned=0 | **0** | on **5** real target TUs (TourProgress, Faders, PropAnim, BandDirector, RockCentral) |
| 4 `MAP_0x1C` INERT | 0 / tmpl 119 | **0 / 119** | **4/4** unchanged |
| DK-3's 6 DEAD SYNCPROP | byte-identical | **byte-identical** | **6/6** unchanged |
| DL-4's DEAD discordance case `BandDirector` | DEAD | **DEAD** | unchanged |
| DJ-3's 6 NEEDED | 17…207 | **17…231** | all LIVE |
| DJ-3's 1 WRONG `TourPerformerLocal` | 6 | **6** | unchanged |
| DK-3's 15-candidate table | — | — | **15/15 reproduced exactly** under `--comparator positional` |
| DN-3's 10 class-attributed blocks | — | — | **10/10 reproduced exactly** under v1 |

★ **Where the `TourProgress` +7 comes from — it is signal, not rounding.** v2 reports
`added=1, removed=1` `.text` COMDATs: the gate swaps the vbase-deleting-destructor
thunk `??_ETourProgress@@$4PPPPPPPM@BAA@AAPAXI@Z` for
`??_ETourProgress@@$4PPPPPPPM@PI@AAPAXI@Z` — **the changed `sizeof(map)` changes the
vbase displacement, which is encoded in the mangled name.** v1 cross-compared the two
different thunks as one positional pair and counted **1** differing word; v2 counts
both 4-word bodies (removed + added) = 8. 101 + 8 − 1 = **108**.

### ★★ Structural corroboration, independent of every control above

DL-4 argued **on mechanism** that the `HANDLE` gate touches no STL instantiation, so
the shared-template floor — indispensable for `MAP_0x1C` — cannot exist for it, and
recorded `INERT (template-only) = 0`.

| comparator | HANDLE units with `template > 0` | `MAP_0x1C` units with `template > 0` |
|---|---:|---:|
| v1 | **132 / 153** ← contradicts the mechanism | 24 / 27 |
| **v2** | **0 / 153** ← matches it exactly | **24 / 27** (all = 119) |

v2 reproduces the mechanism's prediction **in both directions** — zero floor where
theory says none can exist, the exact 119-word floor where theory says it must.
v1 was silently contradicting a claim already in this document, and nobody noticed
because nobody cross-read the two.

### The regression case, and the control demonstrated FAILING

`python3 tools/gate_liveness.py --selftest` builds three synthetic COFF pairs
**in memory** (no toolchain, no filesystem — so it can never skip, and never writes
to the RAM-backed `/tmp`):

| fixture | ground truth | v2 | v1 |
|---|---|---|---|
| 1 — gate inserts `.bss` between `.text`, **no** body changes | INERT | **owned=0 ✓** | **owned=6, LIVE ✗** (false LIVE) |
| 2 — same insertion **plus** 2 genuinely changed words in `?B@@` | LIVE, 2w in `?B@@` | **owned=2, attributed to `?B@@` ✓** | — |
| 3 — gate **ADDS** a `.text` COMDAT past the shorter obj's end | LIVE, 4w | **owned=4, added=1 ✓** | **owned=0, INERT ✗** (false DECISIVE verdict) |
| refusal — duplicate `.text` symbol | refuse | **raises ✓** | — |

Fixture 2 exists so the fixture set cannot be passed by a comparator hardcoded to
zero (rule 2), and fixtures 1 and 3 make v1 fail in **opposite** directions.

★ The v1 assertions are deliberately written as **"v1 STAYS WRONG"**. If someone
later "cleans up" the legacy path, the selftest fails loudly instead of silently
destroying the reproducibility of every pre-DO-4 figure.

**Proven able to fail** — three sabotages, each failing in the expected assertions:

| sabotage | result |
|---|---|
| `compare_symbol := compare_positional` (re-introduce the bug) | **FAIL** (5 assertions) |
| `compare_symbol := const 0` (the "always INERT" vacuity) | **FAIL** (4 assertions) |
| `compare_positional := compare_symbol` (fix the legacy path) | **FAIL** (3 assertions) |

### Measurement status: read-only, and no A/B is reported

`gate_liveness.py` appears in **zero** of `configure.py`, `tools/project.py`,
`config/45410914/objects.json`, `build.ninja`. It has no build edge and emits no
build output, so **it cannot move the metric and no A/B was run** — reporting a Δ0
here would be fabricating a measurement (`ab_measure` would correctly refuse it as
absent-vs-absent). All probe writes go to a `mkdtemp` under `--scratch`
(default `~/tmp`) and are `rmtree`'d; `/Fo` points inside it; the build tree was
verified untouched after a 500-compile sweep.

### What lane DO-4 did NOT do

- **Did not re-run the whole-population sweeps' CONCLUSIONS** (DK-3's 441-TU converse
  sweep, DL-4's 158/158). Their *labels* are what those conclusions rest on, and
  labels are proven unmoved — but the sweeps themselves were not re-executed at HEAD.
- **Did not re-derive DN-3's 12 unsampled blocked consumers.** DN-3 sampled 12 of 24;
  the other 12 remain unprobed under either comparator.
- **Did not re-screen any gate for LIVE-vs-DEAD.** No verdict changed, so no lever
  reopened; in particular the 4 `MAP_0x1C` INERT gates stay kept for the same reason.
- **Did not attempt to make `owned` a damage predictor.** It is not one, and v2 does
  not make it one (see the Spearman note above).
- **Did not touch `scripts/setup_worktree.sh`.** The 0-byte-PCH trap is real but is an
  environment precondition; DN-2 established that "fixing" it inside the probe masks
  an environment problem inside the instrument. Documented in the tool's docstring.
