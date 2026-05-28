# Call-graph triangulation (production)

**Tool:** `tools/fingerprint_match.py triangulate`
**Status:** production (2026-05-28). Productionizes the POC at
`tools/exploratory/callgraph_triangulate.py`.
**Companion:** `docs/plans/exploratory-techniques.md` §1.1 (the original POC +
precision methodology), `docs/plans/bindiff-integration.md` (the wave tooling
this oracle feeds).

## What it does

For any anchor function whose identity we already know — rb3 `fn_A` ⟶ dc3
`"Foo"` (from `unified_id.json`) — the `bl` call sequence in rb3's
dtk-emitted `.s` is positionally aligned with dc3's `"Foo"`. So if at call
site index *i*:

- rb3's `Foo` calls anonymous `fn_B`, and
- dc3's `Foo` calls named `"Bar"`,

then **rb3 `fn_B` IS dc3 `"Bar"`**. Votes accumulate across every named caller
that reaches a given anonymous callee. The technique is orthogonal to bindiff
(only ~9-fn overlap with the RTTI POC; it identifies fns *called by* known fns,
not fns that bindiff structurally matched).

Alignment is purely positional: a caller pair is only used when both sides have
the **same number of `bl`s** (85.2% of attempted anchor pairs align; the 15%
that don't are `/Ob2` inline divergences between dc3 and rb3).

## How to run

```bash
venv/bin/python3 tools/fingerprint_match.py triangulate
# -> writes unified_id_callgraph.json (NEW-only oracle)
```

Defaults (all overridable):
- `--unified unified_id.json` — anchor oracle *and* the NEW-only filter set.
- `--rb3-asm build/45410914/asm` — rb3 disassembly (stripped: every fn is `fn_<addr>`).
- `--dc3-asm ../dc3-decomp/build/373307D9/asm` — dc3 disassembly (recursive; named).
- `--dc3-map ../dc3-decomp/orig/373307D9/ham_xbox_r.map`,
  `--dc3-objects ../dc3-decomp/config/373307D9/objects.json` — for obj/cpp/demangle enrichment.
- `--report build/45410914/report.json` — informational only (see note below).
- `--majority-frac 0.5` — winner vote-fraction needed for a contested proposal.
- `--out unified_id_callgraph.json`.

No build is run; pure read-only data analysis (~30-60 s, dominated by parsing
2,224 dc3 `.s` files).

## Output schema + confidence tiers

Records are **schema-compatible with `unified_id.json`** (`rb3_fn`,
`rb3_addr`, `size`, `source="callgraph"`, `dc3_name`, `dc3_name_demangled`,
`dc3_obj`, `dc3_inline_only`, `bindiff_src`, `similarity`, `confidence`,
`algorithm`) plus callgraph-specific provenance (`cg_tier`,
`cg_distinct_callers`, `cg_total_votes`, `cg_top_votes`, `cg_alternatives`).

`bindiff_src` is populated (the dc3 `.cpp` resolved from the map obj) so the
wave tooling — `bindiff_clusters` (keys on `bindiff_src`) and `gen_target_map`
(keys on `confidence`/`similarity`) — consumes these records unchanged.

| Tier | Rule | confidence / similarity | Intent |
|---|---|---|---|
| `multi` | unanimous winner, ≥2 distinct named callers agree | 0.97 / 0.96 | auto-mergeable; clears `gen_target_map` defaults |
| `single` | unanimous winner, exactly 1 caller | 0.80 / 0.80 | manual-review; below `gen_target_map` defaults |
| `majority` | contested, winner has ≥`--majority-frac` of votes | 0.70 / 0.70 | manual-review |

The `multi` tier's 0.97/0.96 is deliberately above `gen_target_map`'s defaults
(`--min-confidence 0.95`, `--min-similarity 0.96`); `single`/`majority` sit
below so they stay out of the auto-emitted symbol map until promoted.
dc3 `HamX@`→`BandX@` class substitution (same as `merge_bindiff`/
`gen_target_map`) is applied to `dc3_name` so the rb3-side symbol is correct.

## Merge semantics (load-bearing)

The subcommand writes **NEW addresses only** — any rb3 address already present
in `unified_id.json` is skipped. This makes the union **collision-free by
construction**: `unified_id.json` (11,582 entries) is never read-modified, and
the callgraph file (1,555 entries) shares **zero** addresses with it.

To make the wave generators see the callgraph oracle, union the two files and
pass the result as `--unified`, e.g.:

```bash
venv/bin/python3 - <<'PY'
import json
u = json.load(open('unified_id.json'))
c = json.load(open('unified_id_callgraph.json'))
json.dump(u + c, open('unified_id_merged.json','w'), indent=1)
PY
venv/bin/python3 tools/fingerprint_match.py bindiff_clusters --unified unified_id_merged.json ...
venv/bin/python3 tools/fingerprint_match.py gen_target_map   --unified unified_id_merged.json ...
```

Both `bindiff_clusters` and `gen_target_map` filter on `source in
("both","bindiff")` / `"bindiff" in source` respectively. **`callgraph` does
not match those filters**, so a naive union is inert until either (a) the
wave tooling is taught to accept `source="callgraph"` for the `multi` tier, or
(b) the callgraph records are relabeled. Recommended path: have the wave agent
accept `multi`-tier `callgraph` records (they carry `bindiff_src` +
confidence ≥ 0.97 already). Keeping `callgraph` distinct from `bindiff` in the
`source` field preserves provenance and lets a future audit re-derive which
identifications came from triangulation.

## Verification (2026-05-28)

Reproduced the POC exactly:

- **Anchor alignment:** 8,858 / 10,392 (85.2%).
- **Cross-verify precision (unanimous overlap — the POC's headline metric):**
  **5,310 / 5,642 = 94.12%.** Bit-for-bit match with
  `docs/plans/exploratory-techniques.md`. The port is faithful.
- **Cross-verify precision (all-vote overlap, includes the lower-precision
  majority tier):** 5,437 / 5,885 = 92.39%.
- **NEW identifications:** 1,555 (`multi`=277, `single`=1,141, `majority`=137).
  (POC reported ~1,594/1,569; the small delta is that this port splits
  unanimous into `multi`/`single` and requires a canonical `fn_<addr>` call
  target. Equivalent set.)

**Disagreement analysis (load-bearing):** of the 332 unanimous disagreements
in the overlap, ~61% (122/200 sampled) share the leading method token —
they are STL/template ICF aliases (e.g. `?erase@?$list@VSymbol@@...` vs
`?erase@?$list@PAVFileCache@@...`): the *same folded code body* reached by
multiple instantiations, i.e. semantic ties, not errors. The remaining
disagreements are genuine misalignments, which is exactly why `single`-tier is
gated below auto-merge.

**Hand spot-checks** (`mcp__orchestrator__lookup_dc3` + asm inspection):
- `fn_82260430 ⟶ App::Run` (single): rb3 `main` (anchor `fn_82262cf8`) calls
  `fn_82260430` at the bl-index where dc3 `main` calls `?Run@App@@QAAXXZ`.
  Textbook-correct.
- `fn_823E4E30 ⟶ RndTransformable::WorldXfm_Force` (multi, 68 callers): dc3
  source confirmed at `rndobj/Trans.cpp` (matches recorded `bindiff_src`);
  sizes comparable (rb3 0x140, dc3 body extends past last bl at 0x1ec).
- `fn_82A2A438 ⟶ XGRAPHICS::IRInst::Make` (multi, 34 callers),
  `fn_82263E30 ⟶ NgRnd::{dtor}` (single, `rndobj/Rnd_NG.cpp`): plausible
  sizes and call patterns.

## Known limitations

- **Single-evidence (`single`, 1,141 of 1,555) is the bulk and lowest-precision
  tier.** Its per-fn precision is below the 94.1% unanimous-aggregate. Treat as
  manual-review / splits-derivation hints, not auto-symbol-map fodder.
- **ICF/template aliasing** is the dominant non-error disagreement. For a
  template/STL fn body, multiple mangled names are *all* valid candidates for
  the same code; don't treat a single winner as canonical for these.
- **Anchors are unified_id, not report.json.** rb3's `.s` is fully anonymous
  (`fn_<addr>` for every function, even the 394 that compile to 100%), so a
  matched function only anchors triangulation when its dc3 twin is known —
  which is precisely the unified_id mapping. 391/394 matched fns are already in
  unified_id; report.json adds no extra anchors. The subcommand reports the
  matched count for transparency but does not use it as a separate source.
- **No new source code.** Like all the exploratory techniques, this expands the
  *named* pool within the binary; it cannot identify the ~54k functions that
  have no oracle hint (the path-to-100 ceiling).
- **Bidirectional walk not implemented** (known-callee ⟶ unknown-caller). The
  POC follow-up note still applies; it would extend the cone to orphan callers.
