# Permuter Readiness — work queue + workflow

**Audience:** the background permuter actor (and any agent driving fuzzy-gap
codegen iteration). **Status:** the queue generator is committed; the permuter
itself is being proper-ported in the background (see `scripts/permuter*` +
`.claude/skills/permute/SKILL.md` — re-check those before relying on an exact
invocation; they may be mid-rework).

## What the permuter is for (the fuzzy gap)

After a function is pinned + compiles, it lands somewhere on a 0–100% match
spectrum. The **80–99.99%** band is functions that are *almost* byte-exact — the
residue is real codegen the source can't trivially express: volatile/callee-save
**register-swaps** (f0↔f13, r29↔r30…), local **declaration-order**, statement
**reassociation**. Hand-editing these is unreliable and sometimes regresses (a
prior agent flipped **0** by hand and made one function *worse*). The source
permuter mechanizes the search. This is the lever for the band; pinning grows it.

Current snapshot (pre-wave-2, 509 matched): **240 functions in 80–99.99%**, of
which **186 are "real"** (winnable codegen residue) and **54 are likely-ICF**
(template bloat — see below). This count **grows with every pin wave** — refresh
after wave-2 lands.

## Generating the queue

```bash
venv/bin/python scripts/permuter_targets.py rank          # uses build/45410914/report.json
venv/bin/python scripts/permuter_targets.py rank --report /tmp/snapshot.json   # safe mid-build
```
Writes (gitignored, regenerable): `permuter_targets.json` (full records) +
`permuter_targets.txt` (human queue grouped real-first). Each record carries the
**mangled `name`** (pass verbatim to the permuter `--symbol`), demangled name,
unit, size, match%, score, and a `likely_icf` flag.

**Winnability score** (report.json-only — no diff residue without a build):
`band_weight × size_factor × icf_factor`.
- `band_weight`: 99–99.99 → 3.0, 95–99 → 2.0, 80–95 → 1.0 (closer = higher).
- `size_factor`: 1.0 for ≤128 B, decaying to 0.3 by 1 KB (small = small search
  space = higher permuter hit-rate).
- `icf_factor`: **0.15** if the mangled name is an STL container member-template
  instantiation (`?$vector@`, `_M_fill_insert`, `__uninitialized_*`,
  `_Destroy_Range`, `StlNodeAlloc`, …). These are overwhelmingly identical-COMDAT
  **folded (ICF)** in the target and **cannot be matched from source** — the
  permuter spins on them fruitlessly. Detected straight from the name, no build.
  Pass `--include-icf` to rank them inline anyway.

## Recommended order of attack

Work `permuter_targets.txt` top-down (real targets first). The top of the queue
is small functions at 99.9%+ — one or two register-swaps from done, the highest
permuter hit-rate. Examples from the current snapshot: `Achievements::Init`,
`CacheMgrXbox::CacheMgrXbox`, `Rnd::OnToggleTimers`, `CharClip::BeatToFrame`,
`UIScreen::InComponentSelect`, `RndVelocityBuffer::CacheTransform`.

## Reality check (do not over-promise)

A smoke-test flipped **0 of 3** tested near-misses (Geo::OnSide residue was
build-env noise; others unreachable in budget). The permuter cracks *some* of the
band, not all — it is per-function hit-or-miss, not a bulk flip. Spend its budget
on the high-score head; abandon a target after a bounded search and move on.
See `feedback-fuzzy-gap-needs-permuter` (memory) and
`docs/plans/codegen-iteration-targets.md` (the hand-built precursor, now stale —
this generator supersedes it).

## Optional enrichment — residue classification (needs a quiescent build)

`scripts/permuter_targets.py` ranks by band/size/name only. To additionally tag
each target's *actual* residue class (regswap vs offset-swap vs reloc-noise vs
ICF), chain `scripts/analysis/diff_inspect.py` (modes `regswaps`, `offsets`,
`replaces`) per unit **when no build is running** (it reads `build/45410914/`
objects, so it would collide with an in-flight pin-wave build). A future
`--classify` flag could fold this in; deferred to keep the generator build-free.
Targets whose residue is pure `ADDRESS_RELOCATION_NOISE` or `LINKER_MERGED` are
unfixable and should be dropped from the queue, same as `likely_icf`.

## Refresh cadence

Regenerate the queue **after each pin wave** (new partial TUs add near-misses)
and **after any jeff/splitting fix lands** (boundaries shift). It's a 1-second
read of report.json.
