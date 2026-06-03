# Partial-Match-Driven Porting Strategy

**Status:** validated 2026-06-03 against `build/45410914/report.json` at
`matched_functions = 3919` (5.98% of 65,562 fns; matched_code 2.97%).
Builds on the content-match split-relocation run (2526 → 3919, +55% this campaign;
see `memory/project_engine_split_relocation.md`).

This doc is the empirically-grounded model of how PARTIAL (fuzzy) byte matches
convert into 100% `matched_functions`, which levers actually move the number,
and the prioritized roadmap. Every load-bearing number below was measured this
session (worktree A/B builds + objdiff per-function JSON), not assumed.

---

## TL;DR — the corrected thesis

The brief's framing was **"naming is linking": a function stuck at 99% is usually
byte-correct but its relocations point at unnamed callees; name them and it tips
to 100%.** That is real but it is the **minority case** in the current build.
Measuring the actual mismatch causes of the near-miss band overturns the
emphasis:

- The **[99,100) band has 751 functions.** Of a 300-fn sample (largest first):
  **296/300 carry a struct/vtable OFFSET divergence**, 237/300 also have an
  unnamed-callee reloc, and **0 are "pure naming, climbs for free."**
- A live A/B (name 25 collision-safe unnamed callees, full rebuild) yielded
  **+3 matched_functions** — the flywheel's marginal efficiency right now is
  ~0.12 tips per name, because most callers are *also* offset-bound.
- The single biggest near-miss cause is the **engine base-class layout wall**
  (`memory/project_engine_baseclass_layout_wall.md`): offset deltas cluster at
  **±4 / ±8 / ±12 / ±16** (±4 alone = 32% of all pure-immediate diffs) — the
  signature of one missing/extra member or a vtable slot shifted by one.

So the highest-EV partial-match work is **(a) fix a small set of foundational
struct/vtable layouts that cascade across ~97 units, then (b) harvest the now-
unblocked near-misses by naming.** Naming is the *finisher*, not the *opener*.

The other large, *separate* lever is breadth, not depth: a **global fuzzy index**
finds **1,739 brand-new DC3→RB3 identifications in UNPINNED space** that none of
the existing scoped tools can see. That is pin-and-name expansion, gated by
collision-safe naming hygiene.

---

## 1. The linking flywheel — measured

**Model.** objdiff pairs symbols by name *per unit obj* and counts a function
"matched" at `match_percent_normalized == 100`. A `bl <callee>` or `lis/lwz
<data>` instruction matches only if the target-side symbol is *named the same
thing* our base side calls it. dtk names every target symbol `fn_<addr>` /
`lbl_<addr>` until `scripts/target_symbol_map.json` (consumed by
`scripts/obj_target_symbol_renamer.py`, a pre-compile build step) rewrites it.
So an unnamed callee shows as `diff_arg` and suppresses the caller's %.

**Fixpoint question — does naming cascade?** In principle: name leaf callees →
their callers tip to 100 → those become namable anchors → *their* callers tip …
We tested the first round empirically.

**Measured yield (worktree A/B, baseline 3919):**

| action | Δ matched_functions |
|---|---|
| name 25 collision-safe unnamed callees (from unified_id BinDiff) | **+3** |

**Why so low — the inventory is the bottleneck, not the cascade.** Of the
**462 distinct unnamed target refs** inside the [95,100) near-miss band:

| bucket | count |
|---|---|
| already in tsm (paired) | 0 |
| **namable now** (have a name in unified_id/content-match, not yet in tsm) | **30** |
| **truly unknown** (no identification at all) | **432** |

The easy "name a callee we already identified" wins are **already harvested** by
the prior content-match passes. 93% of the remaining unnamed callees are
functions we have *never identified*. The flywheel can't spin on names we don't
have — **its fuel is identification, which is mission item 4 (the global index),
not a name→build loop over the existing map.**

**Conclusion on automating a name→build→name loop:** NOT worth it as a
standalone loop *today* — round 1 yields +3 and round 2+ would be smaller. It
becomes worth it only *after* the global index injects thousands of fresh
identifications; then a single batched name→build pass (not an iterated loop)
captures the first-order tips. Build cost (~8 min full / incremental) dwarfs the
per-round yield otherwise.

---

## 2. Safe naming at scale — design

**The regression class (root-caused).** `obj_target_symbol_renamer.py` does
**no** collision detection or Ham/Band aliasing — it blindly applies the
addr→name map. All safety must therefore be a **validated invariant of
`target_symbol_map.json` itself.** Two failure modes:

1. **Duplicate names** (same mangled name on ≥2 addresses). When both addresses
   land in the *same unit's* obj, objdiff's name-based pairing goes ambiguous and
   *suppresses* matches. Audit of the live map: **37 duplicate names / 78
   addrs; 16 of them have ≥2 addresses pinned in the SAME unit** (e.g.
   `?ClassName@RndTexBlender@@…` 3× in TexBlender.cpp, `?Sine@@YAMM@Z` 2× in
   SHA1.cpp). These are latent suppressors *right now*.
2. **Wrong-identity reuse.** A low-similarity hit reusing a mangled name another
   address legitimately owns. This is what dragged the naive 1821-name bulk pass
   to **−138**.

**The collision-safe + alias-correct invariant** (validated to +73, commit
c32d9c9). A name may be added to the map iff ALL hold:

- **globally unique address**: the addr is not already mapped.
- **globally unique name**: the mangled name is not already a value, AND no other
  candidate in the same batch wants it (dedupe within-batch by name).
- **Ham/Band alias normalized**: RB3 `Band*` == DC3 `Ham*`. The renamer expects
  the RB3 name; a raw DC3-name merge bypasses the substitution and creates a
  phantom `Ham*` symbol that never pairs. Map generators MUST emit the
  `Band*`-substituted form (the relocate tools and `fuzzy_content_match`'s
  `find_dc3_obj` already do `Ham`↔`Band`; a raw tsm merge does not — never merge
  raw DC3 names).
- **ICF caveat for `??_G`/`??_E`/`StaticClassName`/template helpers**: these are
  byte-identical across many classes (the global index surfaces ~1,739 of them).
  They are *legitimately* the same code at multiple addresses; objdiff can only
  pair one per unit. Rule: for an ICF family, name **only the canonical address**
  the unit's base side actually defines; leave the folded twins `fn_<addr>`.

**Recommendation: fold this into a single `safe_name_merge.py` gate** (not yet
built) that every map generator pipes through, replacing ad-hoc per-tool dedupe.
It takes a `{addr:name}` fragment + the live map + splits.txt and emits only the
entries that satisfy the invariant, plus a report of what it rejected and why.
First action it should also do: **excise the 16 same-unit duplicate names already
in the map** (a pure hygiene win, likely +small with zero porting).

**Confidence threshold.** Per the prior band study (in `fuzzy_content_match.py`
header): the **≥0.99 similarity-vs-DC3 band converts ~83%** to objdiff-100 and is
safe to auto-wire; **0.90–0.99 converts 43–65%** (coin-flip — triage, never
auto-commit). For the global index, gate on **jaccard ≥ 0.97 AND identical size**
(filters the jump-table shingle collisions) → the strong-candidate set.

---

## 3. Partial → port prioritization framework

Rank a partial by `(band, cause-class, byte-mass, source-availability,
call-graph centrality)`. The cause-class (from `tools/classify_nearmiss.py`)
determines *which kind of work* converts it:

| cause class | what it is | converts via | EV |
|---|---|---|---|
| **NAME_RELOC only** | unnamed callee/data, code identical | name the callee (free) | HIGH but **tiny inventory** (30 now) |
| **OFFSET** (±4/±8/±12/±16) | struct field / vtable slot shifted | **fix base-class/struct layout** | **HIGHEST** (cascades across units) |
| **WRONG_PAIR** | objdiff paired our fn to the *wrong* target fn (different struct offsets + wrong-type callees) | **re-pin the correct target addr** | MED (fixes a false 99% + frees the real one) |
| **REG** | register allocation differs | source permuter | MED (permuter-class) |
| **OPCODE** | genuine code divergence | port/fix source from DC3/Wii | LOW-MED (one-by-one) |

**Buckets, sized (this build):**

- **"Name and it climbs for free":** ~30 functions. Do it, but it's nearly
  exhausted — the openers were already harvested.
- **"Needs a layout fix that cascades":** the dominant near-miss population.
  296/300 sampled near-misses touch an offset. Concentrated in Crowd,
  AccomplishmentManager, Mesh, LightPreset, DepthBuffer3D, HamCamTransform,
  MidiInstrument (+ ~90 more units, long tail). Fixing the shared *base* classes
  (RndDrawable / RndTransformable / Hmx::Object / the ObjPtr family — several
  already landed per memory) tips many at once.
- **"Big-mass single-function targets":** units where 1–3 partial functions hold
  huge byte headroom — OvershellSlot (6.8KB/3 fns), MD5 (5.8KB/3), inflate
  (5.3KB/1), VocalTrack (4.8KB/1), json_tokener (4.6KB/1). High value-per-fix;
  surfaced by `tools/fuzzy_progress.py --by-unit`.
- **"Source-fidelity bound":** ~17% of sim==1.0-vs-DC3 pairs land <80% because
  OUR ported source diverges from DC3 (objdiff compares vs our base, DC3 is a
  proxy). Bounded by source fidelity, not similarity — needs the §6 port loop.
- **"Vendor names-only":** d3dx9/xgraphics — DC3 has no source for ~186 units;
  can't compile → can't match, but naming them helps *other* units' cross-refs.

---

## 4. Scaling beyond per-unit scoping — the global fuzzy index (PROTOTYPED)

`fuzzy_content_match.py` is per-unit-scoped (trusts existing pins) to avoid
O(N²). It therefore **cannot see unpinned regions** — exactly where the COMDAT
template-pool stragglers (`.text$yc`) live.

**Prototype: `tools/global_fuzzy_index.py`** (built + validated this session).
Banded **MinHash LSH** over reloc-masked opcode 4-shingles. 64 minhashes, 16
bands × 4 rows. Indexes all DC3 named fns + all UNPINNED RB3 fns, candidate pairs
= share ≥1 LSH band, verified by exact Jaccard.

**Measured result (min-size 96, jaccard ≥ 0.88):**

- DC3 fns indexed: 29,691 · RB3 unpinned fns: 22,791
- **2,480 global fuzzy pairs** in unpinned space
- **1,894 with jaccard ≥ 0.97 AND identical size** (jump-table collisions removed)
- **1,739 are truly NEW** — not in `dc3_content_match.json`, `target_symbol_map`,
  or `unified_id.json`. The existing scoped tools never found them.

These are overwhelmingly ICF/template instantiations in unpinned COMDAT pools
(e.g. `??$__uninitialized_copy@PAULongJoyCheat…` in Cheats.obj,
`?_M_fill_insert_aux@?$vector@UBeamDef@Spotlight…`). Harvesting them = pin their
regions (extend the owning unit's split or add a `.text$yc` fragment range) +
name via the §2 safe gate (ICF caveat applies heavily — most are folded twins, so
expect the *converted* count to be a fraction of 1,739, but a real net positive
and currently invisible to every other tool).

**Run it:** `tools/global_fuzzy_index.py [min_size] [jaccard]` →
`global_fuzzy_pairs.json`. NOTE harness quirk: its stderr prints can be swallowed
when piped under some shells; invoke it directly / redirect to a file.

**Alternative considered:** reuse `bindiff_match.json` (11k BinDiff pairs). It is
structural, not byte-masked, so it's noisier for the "is this byte-identical"
question; the LSH index is purpose-built for masked-byte similarity and finds the
unpinned ICF pool that BinDiff's function-granular matching under-weights.

---

## 5. The honest progress metric — `tools/fuzzy_progress.py` (BUILT)

`matched_functions` (==100 count) is a lagging indicator while linking is
incomplete. The reporter adds leading indicators:

```
matched (==100)        3919   (5.98%)
near [99,100)           751    (1-insn-off pool)
>=99                   4670
>=90                   4877
>=50                   5069
fuzzy fn-equivalents   5033.4  (+1114.4 over matched)   <- sum(mp)/100
code-byte fuzzy%       4.6111%
```

**`fuzzy fn-equivalents` = Σ(match_percent_normalized)/100** is the key metric:
it moves when *any* function climbs (40→70 counts), so it shows porting/naming
work landing *before* anything crosses 100. There are **+1114 fn-equivalents of
matched work in flight beyond the 3919 hard-matched** — i.e., the true state is
~5033, and that gap is the predictable upside as layout fixes + naming land.
`--by-unit N` ranks units by invested-but-unfinished byte mass (the §3 big-mass
targets). `--baseline saved.json` prints A/B deltas for build experiments.

---

## 6. The port angle — driving source-fidelity fixes at scale

For OFFSET / OPCODE partials, objdiff compares target vs OUR compiled base, so
the fix is in *our* source, with DC3/Wii as the oracle. Patterns a tool can
auto-suggest from the per-function JSON diff (objdiff-cli `-f json`):

- **OFFSET deltas → struct/vtable layout.** A *consistent* delta on multiple
  `lwz/stw/lfs (disp, rX)` across a class's functions ⇒ a member is mis-placed by
  that many bytes, or (delta = ±4, on a vtable load `lwz r,disp,r; mtctr`) a
  virtual is missing/extra → vtable slot shift. The histogram of deltas
  *per class* localizes the bug to one base/member. Auto-suggest: "class X
  appears N bytes too small/large; check member at offset ~Y." This is the
  mechanizable form of the base-class-layout-wall grind.
- **WRONG_PAIR.** When the paired target fn's offsets/callees are for a
  *different* type, the pin is mis-attributed — re-pin the correct target addr
  (the content-match / global index gives it).
- **REG-only.** Hand off to the source permuter (`/permute`).
- **signedness / decl-order.** Already covered by `/stack-layout` (SWAPPED pairs
  = decl reorder) and `/permute` (signed/unsigned). The new value-add is the
  **per-class offset-delta aggregator** above; everything else has a skill.

A `tools/suggest_layout_fix.py` (NOT built — proposed) would: for a class, pull
all its partial functions, extract OFFSET deltas, and report the dominant delta +
candidate member offset. Highest leverage of the unbuilt tools because it attacks
the dominant near-miss cause.

---

## Prioritized ROADMAP (highest EV first)

| # | action | tool | est. yield | risk |
|---|---|---|---|---|
| 1 | **Excise the 16 same-unit duplicate names** in target_symbol_map | safe_name_merge (build §2) | small +, frees suppressed pairs | none (pure hygiene) |
| 2 | **Per-class offset-delta aggregator → fix base-class layouts** (RndDrawable/RndTransformable/Hmx::Object/ObjPtr family + per-class member order) | suggest_layout_fix (build §6) + objdiff | **HIGH** — cascades across ~97 units, the 751-near-miss pool's dominant cause | med (layout edits ripple; A/B each) |
| 3 | **Harvest the global index**: pin+name the 1,739 new unpinned ids (ICF-caveat filtered) | `global_fuzzy_index.py` + safe_name_merge + relocate_engine_splits | MED-HIGH breadth; net positive currently invisible to all other tools | med (ICF folds — name canonical only) |
| 4 | **Big-mass single-fn targets** (OvershellSlot, MD5, inflate, VocalTrack, json_tokener…) | `fuzzy_progress.py --by-unit` + `/permute` + `/compare-asm` | MED, high value-per-fn | low |
| 5 | **Game pinned-but-unnamed pass** (1086 fns): drive `gen_game_target_map.py` over pinned game spans through the safe gate | `gen_game_target_map.py` (needs candidate_spans for pinned units — recipe below) | MED (bounded by Wii→MSVC source fidelity) | med (demangle-match arity) |
| 6 | **Game unpinned port fan-out** (7623 fns): pin + Wii→MSVC port + name | `relocate_game_splits.py`, `fingerprint_pipeline.py`, port loop | LARGE but slow (per-TU porting) | high effort |
| 7 | name→build→name **single batched pass** AFTER #3 injects fresh ids (not an iterated loop) | safe_name_merge + ninja | first-order tips of #3 | low |

**Diminishing returns / do-not-do:**
- An automated iterated name→build→name **loop** over the *current* map: +3/round,
  not worth the build cost. Revisit only post-#3.
- Bulk raw-tsm merges of DC3 names (bypasses Ham/Band substitution + collision
  gate): proven −138.
- Engine stub-fill via DC3 content-match: **exhausted** — only 2 stub units have
  remaining recoverable DC3 clusters (the +765/+195/+68 passes drained it).
- Hand-cracking REG-only near-misses: that's the permuter's job.

---

## Tools left in `tools/` this session

- **`tools/global_fuzzy_index.py`** — banded MinHash LSH global cross-binary
  fuzzy index (DC3→RB3, unpinned). Finds the 1,739 new ids. (PROTOTYPE, working.)
- **`tools/classify_nearmiss.py`** — classifies each near-miss function's
  mismatch cause (NAME_RELOC / OFFSET / WRONG_PAIR / REG / OPCODE) via objdiff
  per-fn JSON. Drives the §3 prioritization.
- **`tools/fuzzy_progress.py`** — the honest fuzzy-weighted progress reporter
  (§5). `--by-unit`, `--baseline`.

Proposed but NOT built (next session): `safe_name_merge.py` (§2 gate),
`suggest_layout_fix.py` (§6 per-class offset-delta aggregator). #2's aggregator
is the single highest-leverage unbuilt tool.

---

## Appendix: recipe to drive the game pinned-but-unnamed pass (#5)

`gen_game_target_map.py` is gated on a `candidate_spans.json` (`{tu,start,end,
purity}` with hex start/end). Build it from the *currently pinned* game units:

```python
# emit candidate_spans.json from splits.txt game-unit pins that have rb3wii oracle coverage
# (each entry: {"tu": base, "start":"0x..","end":"0x..","purity":1.0})
```

then `tools/gen_game_target_map.py --spans candidate_spans.json --area "" --purity 0.5`
(dry-run) → pipe its output through the §2 safe gate before `--apply`. Measured
this session: ~50 names across 48 pinned game TUs (the demangle-and-match only
pairs `class::method` + arity, so coverage is partial; many high-oracle TUs pair
0 because the obj's defined symbols don't overlap the span / weren't compiled with
those symbols — investigate per-TU).
