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
