# Pinning at scale — automating splits.txt backfill for the unpinned majority

> Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: identification

## Summary

~71.7% of RB3's `.text` (7.88 MB / 65,619 fns) sits in anonymous `auto_*_text`
catch-all buckets — never pinned to a source TU, so objdiff cannot measure it.
This RFC designs the automation to backfill `splits.txt` + `target_symbol_map`
for that majority. The core finding: **mass-pinning is NOT the lever people
assume.** Most of the pipeline already exists (`tools/pin_candidates.py`,
`tools/gen_game_target_map.py`, `tools/pin_audit.py`), and pinning a low-fuzzy
span *lowers* the primary WIRED-fuzzy metric while adding zero STRICT. The
honest recommendation is a **narrow, gated** pin loop (pin-for-match and
high-fuzzy micro-pins only) plus a proposed **shadow-pin** measurement mode —
not a blind bulk backfill.

## Motivation

Identification is one of the two confirmed walls (the other is body-divergence).
A function that is not pinned to a TU is invisible: objdiff only registers a
match when a unit has (a) a pinned `.text` range in `splits.txt` that makes dtk
emit a per-unit target `.obj`, and (b) a compiled `.obj` that objdiff equates by
symbol name. Everything else — the 7.88 MB residual — reads as 0% by
construction, not because the bytes disagree.

Two distinct questions hide inside "pin the rest":

1. **Pin-for-match** — pin a TU we can actually compile so its real matches
   register in STRICT. This is pure upside but is bottlenecked on *span ownership
   + compilable source*, both scarce.
2. **Pin-for-measurement** — pin a span with `NonMatching` (or partial) source
   just to make its fuzzy% visible. This has a perverse cost: it grows the WIRED
   denominator with low-fuzzy bytes and can *lower* the headline number.

The project already burned effort discovering that automated span-carving for
the unpinned bulk (class-B ICF-scattered methods) does not work at usable
precision (topo-locator killed at 0.13, BSim at 0.24). So the automation we
should build is not "find all the spans" — it's "cheaply exploit the spans we
*can* trust, and measure the rest without polluting the metric of record."

## Current state (verified)

All figures from `build/45410914/report.json` and `tools/fuzzy_progress.py` at
main @a1312de (2026-07-08), verified this session with `python3`:

- **STRICT:** 11,240 / 65,619 fns (17.13%); 962,656 / 11,074,108 code bytes (8.69%).
- **Pinned TUs:** 773 named units in `report.json` carry `total_code > 0` from a
  `splits.txt` pin, summing to **3,132,020 code bytes = 28.28% of the binary**,
  containing 23,494 functions. `config/45410914/splits.txt` has 774 unit headers
  (`^\S.*:$`) and 1,216 `.text` range lines (many TUs are ICF-scattered and get
  multiple per-function micro-ranges).
- **Unpinned residual:** ~922 `auto_*_text` section-dump buckets hold the
  remaining **7,876,796 bytes (~71.7%)**. These are the "unpinned majority" the
  brief names. *(Scout's "most .text is unpinned" is correct; the exact figure is
  ~71.7% by bytes.)*
- **FUZZY (metric of record):** WIRED-set fuzzy **94.602%** over 1,392,316
  attempted bytes, n=13,584 fns. The WIRED set = functions with both a pin and a
  compiled obj, i.e. the *only* fns that enter the fuzzy denominator.
  Staircase: >=95: 12,743 | >=90: 13,021 | >=80: 13,175 | >=50: 13,407.

### Tooling that already exists (verified by reading the files)

- **`tools/pin_candidates.py`** (751 LOC) — "unified oracle → pin ranker." Merges
  five oracle sources (bindiff, callgraph triangulation, RTTI+vtable
  transitivity, vtable-only, string-content autoid), weights by per-oracle
  precision + multi-oracle consensus, **gates to functions whose DC3/oracle
  source TU is present in our `src/` tree** (compilable-source gate), groups by
  target TU, **drops already-pinned TUs**, snaps a provisional `.text` span to
  `symbols.txt` boundaries, and emits a ranked pin-wave proposal. It does NOT
  touch `config/` or build — it emits a proposal for a separate gated apply step.
  *This is ~80% of the automation the brief asks for.*
- **`tools/gen_game_target_map.py`** — generates `scripts/target_symbol_map.json`
  entries for **game** TUs from the rb3-Wii BinDiff oracle
  (`unified_id_rb3wii.json`). Without a map entry, a pinned game TU reads a false
  0% (objdiff pairs by name; anonymous `fn_<addr>` never matches MSVC-mangled).
- **`tools/pin_audit.py`** (979 LOC, READ-ONLY) — the closest thing to the
  brief's "overlap_check." Detects sliver/under-pin (A), over-pin (B), and
  displaced-pin (C) signatures with all seven false-positive filters from
  `docs/decomp/research/2026-06-11-sliver-pin-hunt.md`. *Note: there is no tool
  literally named `overlap_check`; the scout's term maps to `pin_audit.py`.*
- **`tools/fingerprint_match.py`** — `extract / report / identify / autoid`
  subcommands (verified at lines 195/261/273/337). `autoid` produces the
  string-anchor density proposals the CLAUDE.md splits-bootstrap recipe consumes.
  Known systematic FP: `Symbols*.cpp` clusters (called out in CLAUDE.md).
- Supporting: `tools/pin_identified.py` (261), `tools/game_splits.py` (268),
  `tools/relocate_game_splits.py` (441), `tools/pin_audit.py`. The plumbing is
  dense and already battle-tested — this is a mature area, not greenfield.

### The "pin dilutes WIRED%" economics (verified)

`docs/decomp/handoff/verify-ab-reliability-2026-07-01.md` documents the A/B
reliability rules but the dilution economics are proven concretely by commit
**d696b52** (CharClipGroup): CharClipGroup is ICF-scattered (methods strewn
0x82265138–0x82b2c210, no contiguous span). Rather than a dishonest span pin, it
micro-pinned the **2 high-fuzzy own methods** (`Save` @0x8237C598 = 99.9% fuzzy,
`FindClip` @0x8237B698 = 91.5%). The A/B showed **WIRED +0.0003 pct-pt (up),
staircase >=95 +1**. The lesson baked into that commit: **pinning only helps the
WIRED metric when the pinned bytes are *above* the current WIRED mean (94.6%).**
Pinning a 40%-fuzzy span drags the denominator down. That is why the gate for a
measurement-pin must be **net-WIRED-positive**, not merely "the bytes exist."

## Proposal

Three tracks, in priority order. Do NOT do a blind bulk backfill.

### Track 1 — Wire `pin_candidates.py` into a gated apply loop (build, small)

`pin_candidates.py` already emits ranked, source-present, unpinned TU spans. It
has never been driven end-to-end into an automatic apply. Build the missing
apply half as a coordinator loop (mirrors the existing wave/land SOP in
`docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`):

```
for TU in $(pin_candidates.py rank --tier consensus>=2 --limit N):
    worktree = setup_worktree.sh ~/tmp/wt-pin-$TU $TU --cold-cache   # cold: dilution A/B must be trusted
    1. append TU span to splits.txt  (+ auto-derived .pdata backfill on next dtk run)
    2. add objects.json entry (NonMatching)
    3. gen_game_target_map.py  ->  target_symbol_map.json entries (game TUs)
    4. touch config/45410914/config.yml && ninja   # dtk emits target .obj + .s
    5. pin_audit.py <TU>                            # overlap / sliver / over-pin gate
    6. whole-binary A/B: STRICT delta AND WIRED-fuzzy delta
    LAND-GATE:  STRICT delta > 0  (pin-for-match)  OR
                WIRED-fuzzy delta >= 0 with >=1 staircase-95 gain (high-fuzzy micro-pin)
    else DISCARD the pin.
```

Key design decisions:

- **Compilable-source gate is already the ranker's job** — only TUs whose DC3 or
  rb3-Wii oracle source is present in `src/` are proposed, so every candidate is
  in principle pinnable-and-buildable.
- **`--cold-cache` is mandatory** for the dilution A/B. Per
  `verify-ab-reliability-2026-07-01.md` §1, a warm CoW worktree can serve a stale
  obj and report a **false net-zero** — fatal here because a real dilution would
  read as "harmless." Cold-cache or force-rebuild the changed objs and confirm
  obj mtime advanced.
- **`pin_audit.py` as the overlap gate** — before landing, run it to reject
  spans that collide with an existing pin, under-pin (leaving own-class methods
  in the residual), or over-pin (swallowing a neighbor's content).

Expected yield: this is the pin-for-match path. It is bounded by how many
*correctly-identified, source-present, contiguous-span, un-pinned* TUs remain —
which is small, because the class-A TU-pure span harvest is EXHAUSTED (wave-8
+0, per shared facts). The residual candidates are mostly class-B scattered
(no contiguous span → not span-pinnable) or already pinned.

### Track 2 — High-fuzzy micro-pin sweep (build, small; the CharClipGroup vein)

For ICF-scattered TUs that are wired+sourced but unpinned, do not attempt a span
pin. Instead, for each own-class method already identified at high fuzzy
(>= WIRED mean ≈ 95%), emit a **per-function micro-range** pin (single-symbol
`.text` range) exactly as d696b52 did. Automate the candidate list:

```
fuzzy_progress.py / report.json  ->  fns in auto_*_text buckets
    with a target_symbol_map entry AND fuzzy >= 95%
    ->  emit micro-range splits entry per fn
    ->  A/B gate: WIRED-fuzzy delta >= 0 AND staircase-95 += 1
```

This is the *only* measurement-pin that is metric-safe by construction: a
>=95%-fuzzy pin lands above the WIRED mean, so it lifts (or holds) the number.
Yield is small per pin (fractional pct-pt, +1 staircase each) but strictly
non-negative and it converts invisible near-100% work into visible progress.

### Track 3 — SHADOW-pin mode (build, medium; measure without polluting the bar)

The brief's key idea. Today there is exactly one denominator: pin a span and its
bytes enter WIRED-fuzzy whether or not they help. A **shadow pin** would let us
*measure* a span's fuzzy% for triage — "which residual TUs are 80% vs 5%?" —
**without** those bytes entering the metric of record.

Design: add a `shadow:` flag to `splits.txt` entries (or a parallel
`config/45410914/splits.shadow.txt`) that dtk-splits and objdiff-measures the
span into a **separate report bucket** (`report.shadow.json`), which
`fuzzy_progress.py` reports as an out-of-band "SHADOW" line but excludes from the
WIRED denominator. This turns pin-for-measurement from a metric liability into a
free triage signal: bulk-shadow-pin the whole residual, read the per-TU fuzzy
histogram, and promote only the TUs whose shadow fuzzy clears the WIRED mean into
real pins.

Cost (honest): this is a real tooling change touching dtk (jeff fork) split
emission, objdiff report bucketing, and `fuzzy_progress.py`. The dtk/objdiff
forks are `../jeff` and `../objdiff` and must be rebuilt manually
(`cargo build --release`). Estimated 1–2 focused sessions. **Only worth it if
Track-1/2 confirm there is a meaningful population of residual TUs sitting at
high fuzzy that we currently can't see** — otherwise there is nothing to promote
and shadow-pinning measures a wasteland. Gate Track 3 behind a cheap probe:
manually shadow-pin (via a throwaway worktree pin + revert) ~20 residual
candidate TUs and check the fuzzy distribution first.

### Data flow summary

```
oracles (unified_id*.json)  ->  pin_candidates.py rank
    -> [source-present, unpinned, consensus-tiered TU spans]
    -> Track1 apply loop (splits + objects + gen_game_target_map)
        -> ninja (dtk target obj) -> pin_audit gate -> cold A/B -> land/discard
residual auto_*_text  ->  Track2 (high-fuzzy own-method micro-pins, metric-safe)
                      ->  Track3 SHADOW pins (triage-only, needs fork changes)
```

## Alternatives considered

- **Blind bulk span-carve of the residual.** Rejected. Requires locating
  class-B ICF-scattered spans, which is the confirmed identification wall:
  topo-locator killed at held-out precision **0.13** (`docs/decomp/research/
  2026-06-30-topo-locator-design.md` BUILD VERDICT — the 0.61 pilot was
  non-reproducible); Ghidra BSim seed-propagation killed at 0.24. Carving spans
  we can't locate produces wrong pins that read as garbage fuzzy and pollute the
  metric. See sibling `07-icf-constraint-solver.md` for the identification-side
  attack that would have to succeed *first*.
- **Pin everything as `NonMatching` to "see the whole picture."** Rejected: this
  is precisely the WIRED-dilution failure mode. It would crater the 94.6% headline
  by dragging ~72% of the binary (mostly 0–40% fuzzy) into the denominator, for
  zero STRICT gain and a *worse-looking* metric. Track 3's shadow bucket is the
  correct way to get visibility without this cost.
- **Data-xref anchoring to locate residual spans** (vtables/RTTI/.rdata pins).
  Complementary, not an alternative — see sibling `05-data-xref-anchoring.md`.
  A vtable slot resolving to `fn_<addr>` is an identification signal that could
  feed `pin_candidates.py` as a sixth oracle; out of scope here.

## Effort & expected value

Anchored to comparable past results in this repo:

- **Track 1 (gated pin-candidates apply loop):** ~1 session to wire the apply
  half + land-gate; then per-wave harvest. **EV honestly LOW for STRICT:**
  class-A span harvest is exhausted (wave-8 +0); the remaining source-present
  un-pinned contiguous TUs are few. Realistic **+0 to +15 STRICT** total across
  all waves, front-loaded, then zero. Worth building only because the machinery
  is 80% there and each pin is cheap.
- **Track 2 (high-fuzzy micro-pin sweep):** ~0.5 session to automate the
  candidate list. Per-pin yield matches d696b52 (+0.0003 WIRED pct-pt,
  +1 staircase-95 each). With the staircase showing 1,503 fns in [95,100), if
  even a few hundred are in unpinned buckets with existing map entries, that's
  **+low-hundreds staircase-95** and a few tenths of a WIRED pct-pt — small but
  strictly non-negative, and it makes real near-done work visible. Best
  ROI-per-effort of the three.
- **Track 3 (shadow-pin mode):** 1–2 sessions (jeff + objdiff + fuzzy_progress).
  EV is *informational*: it does not itself move STRICT or WIRED. Its value is
  de-risking every future pin decision (never dilute-by-accident again) and
  producing the residual-TU triage histogram that sibling docs
  `02-gap-composition-atlas.md` and `18-metrics-and-dashboard.md` want. **Do the
  cheap 20-TU probe before committing.**

Net: this is a **maintenance-and-hygiene** vein, not a frontier lever. It keeps
the pipeline honest and harvests the last cheap pins; it does not break either
wall. The big STRICT gains live in the identification (`05`, `07`, `09`) and
codegen (`11`, `13`, `14`) siblings.

## Risks & failure modes

- **False net-zero A/B (warm cache).** The single biggest trap. A dilution or a
  win can both read as 0 if ninja serves a stale obj. Mitigation: `--cold-cache`
  or forced obj rebuild + mtime check on every pin A/B. A pin showing *exactly* 0
  whole-binary change is SUSPECT (`verify-ab-reliability-2026-07-01.md` §1).
- **Metric dilution.** Any measurement-pin below the WIRED mean lowers the bar.
  Mitigation: the net-WIRED-positive land-gate; shadow-pins for anything below.
- **Wrong span → false 0% or worse, false partial.** A mis-located span pairs
  the wrong target bytes and reads as noise. Mitigation: `pin_audit.py` gate +
  `pin_candidates.py`'s consensus tiering + source-present gate. Never pin below
  consensus-2 without independent confirmation.
- **Overlap corruption.** A new span overlapping an existing pin can silently
  steal bytes from a landed TU, regressing it. `pin_audit.py` signature C
  (displaced) + an explicit overlap check against current `splits.txt` ranges
  before append.
- **ICF-alias inflation.** Micro-pinning an ICF-folded method can double-count a
  merged blob. Run `icf_alias_check` (honesty gate, per shared facts) on any
  micro-pin wave.
- **Concurrency.** Never `git stash` in the shared tree during A/B
  (`verify-ab-reliability-2026-07-01.md` §3). All pin work in `~/tmp` CoW
  worktrees; land serially.

## Kill criteria

- **Track 1:** if the first pin wave produces < +3 STRICT and the
  `pin_candidates.py rank` output of source-present, unpinned, contiguous-span,
  consensus-2 TUs is < ~15, declare the pin-for-match vein exhausted (consistent
  with the class-A harvest already being done) and stop iterating — bank the
  apply loop as a one-shot.
- **Track 2:** if the automated high-fuzzy micro-pin candidate list (unpinned
  fns with map entry AND fuzzy >= 95%) is < ~50, the sweep isn't worth the
  A/B overhead; do the handful by hand and stop.
- **Track 3:** if the cheap 20-TU shadow probe shows < ~5 residual TUs above the
  WIRED mean, there is nothing to promote — do NOT build the shadow-pin tooling.
  The residual is genuinely low-fuzzy and pinning it (shadow or real) buys
  nothing but a longer dashboard.

## Open questions

- What is the actual per-TU fuzzy distribution of the residual `auto_*_text`
  bytes? We know the WIRED mean but not the shadow histogram — Track 3's probe
  answers this and should arguably run *first* to size the whole opportunity.
  (Sibling `02-gap-composition-atlas.md` may already have partial data.)
- How many of the 1,503 fns in [95,100) are in unpinned buckets vs already
  pinned-but-imperfect? Determines Track-2 yield directly. Answerable now from
  `report.json` cross-referenced with `target_symbol_map.json` membership.
- Should `pin_candidates.py`'s source-present gate be relaxed to "source
  portable from oracle" (not yet in `src/`) to feed the porting pipeline
  (`gameport*` skills) rather than only measuring what's already ported?
- Is a `shadow:` flag in `splits.txt` cleaner than a parallel
  `splits.shadow.txt`? The former keeps one source of truth; the latter avoids
  touching the hot path that every dtk run parses.

## References

- `config/45410914/splits.txt` — the pin file (774 unit headers, 1,216 `.text`
  ranges @a1312de).
- `config/45410914/objects.json` — TU compile declarations + match status.
- `build/45410914/report.json` — whole-binary measures (verified figures above).
- `tools/fuzzy_progress.py` — WIRED-fuzzy + staircase (metric of record).
- `tools/pin_candidates.py` — oracle→pin ranker (751 LOC; the automation core).
- `tools/gen_game_target_map.py` — game-TU `target_symbol_map.json` generator.
- `tools/pin_audit.py` — sliver/over-pin/displaced-pin detector (979 LOC; the
  overlap gate).
- `tools/fingerprint_match.py` — `autoid` string-anchor density (splits-bootstrap
  input; `Symbols*.cpp` is a systematic FP).
- `scripts/target_symbol_map.json` — target `fn_<addr>` → MSVC-mangled map (read
  by the wired pre-compile `obj_target_symbol_renamer`).
- `scripts/setup_worktree.sh` — CoW worktree; use `--cold-cache` for trusted A/B.
- `docs/decomp/handoff/verify-ab-reliability-2026-07-01.md` — the false-net-zero
  A/B trap + stash race; the dilution-hygiene rules.
- `docs/decomp/handoff/wave-loop-SOP-2026-06-20.md` — wave discover/execute/
  audit/land protocol the apply loop mirrors.
- `docs/decomp/research/2026-06-11-sliver-pin-hunt.md` — the sliver-pin detectors
  `pin_audit.py` implements.
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — topo-locator BUILD
  VERDICT (killed at 0.13); why blind span-carve of the residual fails.
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — class-B
  body-divergence wall (why located-but-diverging fns still stall).
- Commit **d696b52** — CharClipGroup high-fuzzy micro-pin (the Track-2 exemplar).
- `CLAUDE.md` — splits-bootstrap recipe, obj patchers wired list, worktree rules.
- Siblings: `02-gap-composition-atlas.md` (what the residual is),
  `05-data-xref-anchoring.md` (data pins as identification),
  `07-icf-constraint-solver.md` (locating class-B spans — the prerequisite),
  `18-metrics-and-dashboard.md` (shadow histogram consumer).
