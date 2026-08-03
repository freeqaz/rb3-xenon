# The honest unit-completion frontier (lane DS-4, 2026-08-03)

> **REGENERATE, DO NOT CITE.** Every number here is a photograph of a tree that
> keeps moving. `python3 tools/reachable_ceiling.py --json <out>` recomputes it in
> ~2 min and REFUSES (distinct exit codes 2/3/4/5) rather than emitting a
> confident-looking zero. The previous static census was wrong within one wave of
> being written; this file exists to record *method and shape*, not to be read as
> current state.

## Why this lane ran

The campaign's standing goal is **"100% matching for the important units"** — a
**unit** target. Recent waves optimised **bytes** and **functions**, which is a
different ranking and does not move units. This lane regenerated the census,
audited the buckets, and worked the reachable shortlist.

## Baseline (HEAD `9023b42d`, settled, read from `report.json` by exact key)

| key | value |
|---|---|
| `matched_functions` | 43,678 |
| `matched_code` | 4,215,804 |
| `matched_code_percent` | 39.441795 |
| `masked_equal_functions` | 22,707 |
| honest (`matched − masked_equal`) | 20,971 |
| `fuzzy_match_percent` | 46.086872 |
| `total_code` | 10,688,672 |

## The headline number, and it is grim

Over the **1,022 pairable units** (mpn ruler — "at 100" == every row
`match_percent_normalized == 100`):

| bucket | n | share | meaning |
|---|---|---|---|
| `AT_100` | **208** | 20.4% | every row at 100 |
| `COMPLETABLE` | **48** | 4.7% | sub-100 rows exist but **none is an unpaired-anon row** ⇒ pure source work reaches 100% |
| `ANON_BLOCKED` | 186 | 18.2% | leftover rows are unpaired `fn_<8hex>` |
| `MIXED` | 570 | 55.8% | needs **both** source work and map rows |
| `OD_REGION` | 10 | 1.0% | pin overlaps the `/Od` window |

> ### ★★★ SOURCE-ONLY CEILING ON UNITS-AT-100 = **256 / 1022 = 25.0%**
>
> That is `AT_100 + COMPLETABLE`. **Three quarters of pinned units cannot be
> completed by source work alone at the current map coverage**, because they
> contain at least one retail address with no `target_symbol_map.json` entry, and
> an anon target body can never pair with our mangled name.

Two rulers, never conflated: units at 100 by **mpn** = 208; by **all-rows fuzzy**
(the byte ruler) = **189**. 19 units are counted complete while withholding bytes.

## The anon gate, re-verified this run

41,273 anon rows, **0** of which appear in the map ⇒ the identity *"anon row" ==
"retail address absent from the map"* holds with zero exceptions. So the map
lookup here is a **consistency check, not an independent classifier**. The
discriminating split is the three-way on anon mpn:

| | n | |
|---|---|---|
| unpaired (mpn 0) | 16,892 | needs a map row |
| **paired** (0 < mpn < 100) | **1,674** | **SOURCE-REACHABLE** |
| masked (mpn 100) | 22,707 | == `masked_equal_functions` exactly |

## Audit of the 48 `COMPLETABLE` — how many are *important*?

Keyed on `source_path` (⚠ keying on the unit path misclassifies: several game
units, e.g. `default/ChooseColorPanel` and `default/TourBand`, live under
`default/` with no `band3` prefix — I made exactly this error first pass).

| class | n | note |
|---|---|---|
| **GAME** (`src/band3/`, `src/network/`) | **9** | highest value — the RB3-specific layer |
| **ENGINE/HMX** (`src/system/`) | **26** | |
| VENDOR (json-c, soundtouch, IPP, FFT) | 8 | user scope says hard-skip |
| AUTO split, no source | 2 | `auto_03_*` |
| **bogus-pin, already rejected on evidence** | 3 | `SkeletonDir`, `HamDriver`, `FilterQueue` |

⇒ **35 of 48 are "important" units.** The census **cannot detect a bogus pin** —
the three rejected ones still sit in `COMPLETABLE` and always will.

## The extended frontier the old census mislabelled

Of the 52 **one-away `ANON_BLOCKED`** units, sub-classification says **24 are
`SAMECLASS_DIFFSIZE` = SOURCE WORK, not a map row**: our obj *has* a symbol of the
blocker's class, at a *different size*, so the body diverges. A prior census filed
these under a `MAP_ONLY` label glossed "no source lane can ever finish these" —
which told source lanes to skip their most valuable follow-on. 10 of the 24 are
game code. Remaining one-away anon sub-buckets: 12 `NO_CLASS_ANCHOR`, 7
`AUTO_03_NO_OBJ`, 5 `MAP_FIXABLE_UNADJUDICABLE`, 3 `NO_SAMECLASS`, 1
`MAP_FIXABLE_CANDIDATE`.

⚠ Sub-classification ran **only on one-away units** (52 of 186 `ANON_BLOCKED`);
the other 134 are unclassified. The equivalent source-work pool inside `MIXED`
(570 units) has **never been sized at all**.

## Economics — state these separately, never let one stand for the other

- **Unit completion is NOT a code% play.** `matched_code` is **all-or-nothing per
  row**, so a 7.5 KB row crossing can move **zero units** while a 200 B row
  completes one. Rank by what you are scored on.
- **6 of the 48 `COMPLETABLE` units have `anon_paired` blockers (19 rows).**
  Driving a paired-anon row to 100 makes it anon@100 — and anon@100 **is**
  `masked_equal` exactly. So finishing those units pays `matched_functions` into
  the **masked** stratum, **not** into honest. The bucket is right; the *value* is
  what's easy to over-read.
- Naming alone pays **+1 honest, +0.000000pp code%** — but for a *unit* target an
  arg-only anon row is still worth naming.
- **8 of the 48 `COMPLETABLE` units vanish if drained** — a single-function unit
  can never be completed by a splits boundary move (the move drains its only
  `.text` block, the unit emits a 42-byte obj and `report.json` hard-fails). Those
  must be completed by source work or not at all.

## What this lane then CLOSED (all whole-binary A/B verified, same ruler)

Every figure below is from `tools/ab_measure.py` on a settled worktree. In *every*
run `unit net (ALL units)` equalled whole-binary `Δmatched` exactly — that
equality is the cross-check that the unit attribution is not double-counting.

| leg | kinds | Δmatched | Δhonest | Δcode% |
|---|---|---|---|---|
| C (DEFER_OWNER + ScaleAddEq) | source | **+4** | +4 | +0.005425pp |
| A+D (implicit dtor + decl order) | source | **+2** | +2 | +0.005090pp |
| D (re-carve + map rename) | map+splits | **+1** | +1 | +0.000038pp |
| B+A (5 attribution fixes + DataUtl, −1 deliberate) | map+source+splits | **+3** | +3 | +0.001160pp |
| A (??_E weak-external renames) | map+splits | **+1** | +1 | +0.000635pp |

### ★★★ THE UNIT RESULT — and why Δmatched CANNOT be used to count it

Regenerated census, start vs end, verified by **set diff of the AT_100 membership**
rather than by arithmetic:

> **units at 100%: 208 / 1022 → 221 / 1023 — `+13`, with ZERO units lost.**

**`Δmatched` structurally undercounts unit completions.** Three of the 13
(`UIStats`, `TrainerProvider`, `ScoreDisplay`) completed with **Δmatched = 0**,
because the fix removed a *wrongly attributed row from the denominator* rather
than matching a new one — 12/13 → 12/12. Any wave that counts unit completions
from `Δmatched`, or from `ab_measure`'s "unit improvements" line (which only lists
units whose matched count **rose**), will miss this class entirely. **Diff the
census.**

The economics played out exactly as predicted: **13 completed units moved code% by
+0.0123pp total.** Whole-binary `matched_functions` 43,678 → **43,691**.
⚠ That is `+13` absolute while the five measured legs sum to `+11`; the extra `+2`
is the forced-re-split baseline correction described above, **not** an unmeasured
gain. Deltas compose, absolutes do not.

★ **The dominant finding is not a source finding.** Of the 13 units closed, only
**four** were source defects (`DEFER_OWNER` ×3, `ScaleAddEq`, `DataPushVar`,
`TourPerformerLocal` decl-order — 4 distinct causes). **The rest were
IDENTIFICATION defects**: `.text` blocks carved into the wrong unit, map rows
naming the wrong symbol, `??_E`/`??_G` thunk-kind mix-ups, and pins with no valid
home at all. A source lane staring at those units would have found nothing wrong
with the source, because nothing was.

⚠ **Do not sum the absolutes across these runs.** The map/splits run's leg A read
`43686` where the previous run's leg B read `43684`; the difference is the
**forced re-split** `ab_measure` performs in the settle phase for map/splits
patches so both legs sit in freshly-split state. It is a pre-existing splits-state
correction, not an effect of any commit in this lane. Deltas compose; absolutes do
not.

★ **Two of the seven were ATTRIBUTION defects, not source defects** — a `.text`
block carved into the wrong unit, plus map rows naming the wrong symbol (including
a `??_E`/`??_G` thunk-kind mix-up). A source lane staring at those units would
have found nothing wrong with the source, because nothing was.

Native gate after the shared-header change: **PASS 18/18, rc=0** (seeded with all
four flags first, so no target silently SKIPped).

### The two root causes worth carrying forward

1. **`RB3_TU_OBJPTR_DEFER_OWNER`** — retail emits `{vptr-lis, mOwner, mObject,
   …, vptr-store}`; a member store written from the **base mem-init list** sits
   in the base ctor's scheduling region and may float **above** the derived vptr
   materialization, while one written in the **derived body** is pinned after it.
   The mechanism was already documented for `mObject`; it had simply never been
   applied to `mOwner`.
   ★★ **Epistemic correction this forced:** `UIListLabel.cpp` and `Object.h` both
   recorded this residual as *"NOT source-steerable / scheduler wall, not
   source"* on **three-way byte-identical evidence**. That evidence was sound but
   not decisive — all three spellings left `mOwner` in the base mem-init list, so
   for this store they were **one experiment run three times**, which is exactly
   why they agreed. **Byte-identity across variants is evidence only if the
   variants actually vary the thing under test.**
2. **Explicit empty `virtual ~X() {}` is not free.** With a virtual base +
   vtordisp it makes MSVC emit the vptr/vtordisp reset preamble inside the dtor;
   that preamble inlines into `??_D`, pushing it past the `/Ob2` threshold, so
   `??_G` calls `??_D` out of line. Removing the explicit declaration (the base
   dtor is virtual, so the implicit one still is) shrinks `??_D` to 5
   instructions, `/Ob2` inlines it, and `??_G` goes byte-exact.

### The frontier AFTER this lane (regenerated, same instrument)

| bucket | before | after |
|---|---|---|
| `AT_100` | 208 | **221** |
| `COMPLETABLE` | 48 | **32** |
| `ANON_BLOCKED` | 186 | 191 |
| `MIXED` | 570 | 569 |
| `OD_REGION` | 10 | 10 |
| total | 1022 | 1023 |
| **source-only ceiling** | 256 (25.0%) | **253 (24.7%)** |

**The ceiling barely moves, and that is the structural point:** completing a unit
moves it from `COMPLETABLE` to `AT_100`, so the sum is ~conserved. **Closing units
does not raise the ceiling — only map/identification work does.** Four bogus-pin
units (`FreestyleMotionFilter`, `GestureMgr`, `HamSong`, `auto_03_827685E0`) were
removed outright, and five `auto_03_*` units appeared from the orphaned ranges.

### Next wave, sized on this tree

- **`??_E<Class>@@UAAPAXI@Z` is a COFF WEAK EXTERNAL, not a body** — MSVC emits the
  deleting dtor under `??_G` and `??_E` only as a zero-byte alias unless a real
  `delete[]` exists. Retail is stripped, so both names denote one address and the
  map picked the wrong one; objdiff then pairs the target against a **0-byte
  alias**, which is why these read a flat **0.0%** rather than a low score.
  **Measured: 46 such rows, 45 at mpn 0.0.**
  ⚠ Sized honestly: only **5** have `Class == unit stem`; the other 40 sit in units
  whose stem differs and need per-row adjudication. Unit-stem match is itself a
  weak proxy (one TU defines several classes), so 5 is a **floor on the easy
  subset**, not a ceiling on the lever. The clean fix is shared tooling: a
  post-compile patcher materialising `??_E` as a real symbol aliasing `??_G`.
- **The explicit-empty-`virtual ~X(){}` lever** should also close `QuestFilterPanel`
  (90.5%), `NewAwardPanel` (90.5%), `SelectDifficultyPanel` (84%) — same family.

## Negatives worth not re-funding

- **`EQEffect::Process`** (mine): the `off:-24` on the `addi` and `off:+24` on the
  loads **cancel** — same effective address, a different split between base
  constant and load displacement. **Not a layout defect**; anchor-selection /
  regalloc, i.e. permuter-class. Do not read paired ± offset deltas as a struct
  bug without checking they don't cancel.
- **`NewObject` stack-slot cluster** (mine): retail stores the new pointer at a
  **separate** stack slot while we reuse the dead `Symbol` temp's slot. Tempting
  as a force multiplier — it is not. Control: **202 of 211** `NewObject` rows are
  already at 100, and **81 of 84** at size 112. Only 5 rows show it, and all live
  in `MIXED` units, so closing them completes **zero** units.
- **`HamPhotoDisplay::Save`** (lane C): misidentified 3-block carve, not DC3 drift
  — rb3-Wii has no `hamobj/` at all, so there is no game-side oracle. This is
  identification work; "fixing" the body would corrupt a DC3-correct class to fit
  a wrong target.
- **`main`** (lane B) — **proven** unclosable, not assumed. 18/19 instructions are
  byte-exact; the residual `4280D1F1` is an op16 B-form `bcl` where we emit op18
  `bl` — same unconditional call, same target, **encoding only**. Census over
  retail `.text`: **178,015 op18/LK=1 vs exactly ONE op16/LK=1**, this
  instruction. Binary-unique ⇒ post-link artifact, unreachable from any C++
  spelling.
- **`ChooseColorPanel`** (lane A) — a genuine either/or, **28/29 both ways**.
  `#pragma inline_depth(0)` around the ctor (needed for an out-of-line `std::map`
  ctor) also suppresses `??_D` inlining, because MSVC generates the implicit dtor
  family inside whatever `inline_depth` region the ctor sits in. Three escapes all
  failed. Keep the pragma (+180 code bytes vs +68).
- **`StorePurchaser` / `auto_03_*`** (lane D) — an `auto_*` unit has **no compiled
  base object at all**, so no source edit can move it; and `StorePurchaser`'s one
  60-byte function is a `Job` subclass ctor belonging to `SessionJobs_Xbox.cpp`.
  Fixing it **deletes** a unit rather than completing it (its last `.text` block),
  which is an accounting call, so it was deliberately not landed.
- **`Rnd_NG ??$MakeString@HH@@`** (lane C): wrong splits pin. Map-independent
  proof: all 114 `bl` sites to `??0FormatString@@` have frames `0x870-0x8b0`,
  while the pinned address's frame is `0x70`. ⚠ And the ICF fold class at
  `0x827c40e8` serves int/char/uchar/`char*`, so it can only ever **refute** a
  candidate, never confirm one.

## Instrument controls executed before trusting any of the above

- `--selftest`: 3/3 class-anchor pins pass, and the *defective* v1 anchor fails 4
  of 7 ⇒ the pin **can** fail.
- `--sabotage retail-blind` (models the binary-blind `grep` shim): correctly
  **REFUSED with exit 5**, no census produced.
- Attribution scanner: 2 known-positives found, 1 fabricated class reads 0,
  labelling verified purely additive (1,022 unit records identical before/after).

⛔ **Do not quote the pooled 5.34× blocker-attribution enrichment.** It exceeds the
enrichment in *either* stratum (2.03× plain / 1.45× namespaced) — Simpson's
paradox, manufactured by composition. 1.45× sits inside the band already refuted
for classifier use. The **unit-stem** form of the test measured **0.84× —
anti-enriched** — and must not be re-funded.
