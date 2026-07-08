# Data-xref anchoring — vtables, RTTI, and .rdata/.data pins as an identification signal

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: identification

## Summary

Data cross-references (vtable installs, RTTI records, global/static loads) are a
signal orthogonal to call-graphs and strings for anchoring an anonymous function
to its class/TU. The vtable half of this channel is **already productionized**
(`fingerprint_match.py rtti`, since 2026-05-28) and its ceiling is honestly
measured: 384 HIGH-tier records at 71.1% precision — a manual-review oracle, not
an auto-merge win. The scout's "+385 HIGH ready to ship" claim is **wrong**: it's
384, and it is explicitly *below* the 90% auto-merge bar. Two genuinely
underexplored sub-veins remain: (a) global/static data-xref anchoring (never
built), and (b) `.data`/`.rdata` *byte* pinning (`matched_data` is 16 / 4,118,360
= 0.0004%). This RFC scopes both honestly; the verdict is PILOT-FIRST on a
narrow slice, DEFER on the rest.

## Motivation

The two confirmed identification walls (topo_locate precision 0.13; BSim
precision 0.24) are call-graph/structural. A method that **installs vtable `V`**
(`lis rN, vftable_V@ha; addi rN, rN, vftable_V@l`) or **loads global `G`** is
anchored to the owning class *by data reference*, independent of who calls it and
independent of its string content. Under `/O1` no-LTCG, TU spatial grouping holds
in `.data`/`.rdata` too (CLAUDE.md: the MasterAudio cluster packs contiguously) —
so a data anchor also localizes the function's TU span, which is what pinning
needs. If a meaningful count of *currently-unidentified* functions carry a
data-xref to *already-identified* data, that closes identifications the
call-graph oracle misses. This RFC measures whether that count is meaningful.

## Current state (verified)

**Match baseline** (`build/45410914/report.json`, main @a1312de, read
2026-07-08):
- `matched_functions`: 11,240 / 65,619
- `matched_code`: 962,656 / 11,074,108 bytes (8.69%)
- `matched_data`: **16 / 4,118,360 bytes = 0.00039%** — data sections are
  essentially unmatched.
- 2,456 units in the report.

**Identification already outstrips matches.** `unified_id.json` holds **11,582**
identified addresses — *more* than the 11,240 matched functions. Identification
is therefore **not** the binding constraint for the near-term backlog: we have
named more functions than we've matched. (This is the single most important fact
for this RFC's expected value — see Effort & EV.)

**The vtable channel is production, and its ceiling is measured.**
`docs/decomp/rtti-vtable-transitivity.md` (production 2026-05-28) +
`docs/plans/exploratory-techniques.md` §1.2–1.4:
- `tools/fingerprint_match.py rtti` walks RB3's non-standard X360 RTTI
  (verified present: `_rtti_walk_rb3` at `fingerprint_match.py:885`, subparser at
  `:1990`). RB3 has 1,396 RTTI TypeDescriptors; the walk recovers **1,321
  vtables** (vs jeff's heuristic 342 — a 3.8× expansion) / 1,317 named classes.
- It emits `unified_id_rtti.json` (verified on disk, 193 KB, Jun 20): **384
  HIGH-tier records** (I loaded the file and counted: 384, all `source="rtti"`,
  all `rtti_tier="HIGH"`). The POC reported 385; production is 384 (the delta is
  autoid-no-name overlap skips, per the doc's Merge Semantics section).
- **Precision: 59/83 = 71.1%** cross-verified against `unified_id` overlap.
  Confidence stamped at **0.80**, deliberately below `gen_target_map`'s 0.95
  default so a naive union is inert. **This is a manual-review / splits-derivation
  oracle, NOT auto-merge fodder.** LOW tier (`unified_id_rtti_low.json`, 730 recs)
  is 41.1% — rejected wholesale.
- Orthogonality: HIGH (384 recs) overlaps the callgraph oracle by **2**
  addresses; combined NEW set is 397/399 = 99.5% orthogonal. So the *signal* is
  genuinely additive — the problem is precision, not redundancy.
- Records are dominated by small dtors/thunks: sampled `dc3_name`s are
  `FilePath::{scalar deleting destructor}`, `StackString::{scalar deleting
  destructor}`, `std::exception::what`; median record size 76 bytes. These are
  exactly the ICF-fold-prone bodies the doc's ICF-alias caveat flags.

**`scripts/dump_vtable.py` is a MATCH-DEBUG tool, not an identification tool**
(scout implied otherwise). It reads `??_7Class@@6B@` symbols + relocations from
**our compiled `build/45410914/obj/*.obj`** (`dump_vtable.py:2-11,91-93`) to check
vtable slot correctness during matching. It does not scan the retail binary. The
`/vtable` and `/data-diff` skills (`.claude/skills/{vtable,data-diff}/SKILL.md`,
both present) wrap it for slot-mismatch debugging. RTTI *identification* lives
entirely in `fingerprint_match.py rtti`.

**How the data-xref signal is exposed today (verified).** dtk (jeff fork,
subcommands `xex/map/disc/...`, no `analyze`) emits named `vftable_<addr>`
symbols into the `.s` files for the **342 heuristically-detected** vtables only.
Grepping `build/45410914/asm/*.s`:
- **342** distinct `vftable_<addr>` symbols referenced (matches jeff's scanner,
  NOT the RTTI-walk's 1,321).
- **396** total `.text` reference sites (`lis rN, vftable_X@ha`).
- The other ~979 RTTI-walked vtables are **not labeled** in the `.s` — they sit
  as raw big-endian `.long 0x82…` pointers in `.rdata`. So the *readily
  grep-able* vtable-install xref signal is thin as currently emitted.

**`.data`/`.rdata` pinning is supported but unused.** `config/45410914/splits.txt`
has 773 TU entries but only **2** data lines — and both are the *global* section
headers (`.rdata type:rodata align:65536`, `.data type:data align:65536`), not
per-TU data ranges. Per CLAUDE.md, per-TU `.rdata`/`.data` ranges need manual
pinning. That work has essentially not started, which is why `matched_data` ≈ 0.

## Proposal

Three separable pieces, in ascending risk/uncertainty. Do **P1** first as a gate.

### P1 — Measure the global/static data-xref anchoring yield (1 day, decides the rest)

The brief's core question: *how many unidentified functions have a data-xref to
identifiable data?* This has never been measured. Build a read-only measurement,
no source edits.

Data source options (pick the cheapest that answers the question):
- **Ghidra MCP (port 8002)** — `tools/ghidra/ghidra-xrefs.py` +
  `tools/ghidra/mcp_client.py`. For each named data symbol (RTTI-walked vtable VA,
  or a global we can name), enumerate `.text` xrefs; keep those whose source
  function is NOT in `unified_id.json`.
- **dtk-emitted `.s` reloc scan** — grep `build/45410914/asm/*.s` for
  `@ha/@l` reference pairs and `.long` reloc targets that land in `.rdata`/`.data`.
  Cheaper, no Ghidra, but only covers the 342 labeled vtables + whatever globals
  jeff labeled.

Measurement (`tools/exploratory/data_xref_yield.py`, NEW, ~150 LOC):
1. Load `unified_id.json` addresses (identified set) and the RTTI-walk vtable→class
   map from `fingerprint_match.py rtti` internals (reuse `_rtti_walk_rb3`).
2. For every `.text` reference to a named vtable/global, record
   `(referencing_fn_addr, target_class)`.
3. Emit two counts: (a) referencing fns **not** in `unified_id` (= new
   identification candidates), and (b) of those, how many are *scattered*
   (small, un-pinned) — the class-B population the walls trapped.
4. Spot-precision: for the subset where the referencing fn *is* already in
   `unified_id`, does its known `dc3_obj` class match the anchored class? That's
   a free precision estimate exactly like the RTTI doc's overlap method.

**Kill gate:** if (a) < ~150 *new, non-trivial* fns, or precision < ~60%, the
global-data-xref vein is DEFER (write the null result to
`docs/decomp/research/2026-07-08-data-xref-yield.md`). The prior sits low: we
already have 11,582 IDs > 11,240 matches, so new IDs convert to matches only after
the *matching* work (which sibling RFCs 11/12/13/14 own) catches up.

### P2 — Rescue the RTTI multi-inheritance + drift-recovery tail (0.5–1 day each)

Two documented, un-taken follow-ups from `rtti-vtable-transitivity.md` §Known
limitations that *do* raise identification count without new infrastructure:
- **Multi-inheritance classes (226 deferred).** Classes with sub-object vtables
  are skipped because pairing needs the post-`6B` modifier in the mangled name.
  POC estimate: **+50–150 HIGH hits**, ~half-day. Concrete, bounded.
- **CHD drift-recovery for LOW tier.** A Class-Hierarchy-Descriptor walk could
  detect *which* slot drifted between dc3 and rb3 and rescue the unaffected slots,
  lifting LOW precision from 41% toward 70%+. Larger, speculative.

These stay a **manual-review oracle** (0.80 confidence). They feed splits
derivation, not auto-merge. Value only realizes if a matching wave consumes them.

### P3 — `.data`/`.rdata` byte pinning as *matched bytes* (open-ended; PILOT ONLY)

The brief asks: could data-section matching itself add matched bytes?
`matched_data` is 4.1 MB of unclaimed denominator. Mechanically it works — pin a
per-TU `.rdata`/`.data` range in `splits.txt`, and objdiff will byte-compare our
compiled data against the target. A vtable that matches at the *pointer* level
(all slots resolve to the same functions) counts as matched data.

But the honest read: **data matches are downstream of code matches.** A vtable's
slots are function pointers; the reloc slot only matches once the pointed-to
function is itself compiled into the right symbol. String pools match once the
string layout matches (`docs/decomp/string-layout-gap.md`). So `.rdata` pinning
does not *lead* — it *lags* code matching and mostly re-credits work already done,
against a separate `matched_data` denominator that sibling RFC
`10-middleware-and-denominator.md` argues about. **Pilot: pin `.rdata`+`.data` for
ONE already-100%-`.text` TU (e.g. MasterAudio.cpp), measure the `matched_data`
delta, and check `icf_alias_check` doesn't inflate.** If a clean 100%-`.text` TU
yields near-100% data trivially, there may be a cheap sweep; if it yields partial,
data is its own grind and DEFER.

## Alternatives considered

- **Fold into the RTTI oracle and call it done.** That's the status quo since
  2026-05-28. It leaves the *global/static* data-xref channel (P1) and the MI/drift
  tail (P2) unexplored, and never asks the `matched_data` question (P3). This RFC's
  only new claim is that P1's measurement is cheap and unmade.
- **Push RTTI HIGH into auto-merge by lowering the bar.** Rejected: 71.1% < 90%
  poisons `gen_target_map`; a false pin reads a *false 0%* on a game TU
  (CLAUDE.md obj-patcher note). Manual-review gating is correct.
- **Use `dump_vtable.py` for identification.** Category error — it reads *our*
  objs, not the retail binary. Not an identification path.
- **Ghidra BSim / topo_locate (siblings 07/08/15).** Those are the call-graph
  channel that already hit the walls. Data-xref is deliberately orthogonal to
  them; this RFC is the "different signal" hedge, not a competitor.

## Effort & expected value

Anchored to comparable past results in this repo:
- RTTI production (2026-05-28): 384 HIGH IDs, but **converted to ~0 direct
  matches** — it's a manual-review oracle that has sat unconsumed (its file is
  from Jun 20; matches came from the grind loop, not from RTTI promotion).
- Callgraph triangulation: +1,594 IDs at 94.1% — the high-yield ID oracle, and
  even *it* is gated behind matching-wave consumption.
- The binding fact: **11,582 IDs already > 11,240 matches.** Identification is
  ahead of matching. More IDs do not become matches until RFCs 11–14's matching
  levers catch up.

Honest EV:
- **P1 (measure):** 1 day, near-certain to produce a number. If the number is
  large it hands sibling `04-pinning-at-scale.md` a new pin-candidate stream; if
  small (likely) it's a clean kill. **EV of the measurement itself: high
  (decision value); EV of the vein it gates: low-to-moderate.**
- **P2 (MI tail):** +50–150 IDs (POC estimate), 0.5–1 day. But per the binding
  fact, these are **IDs, not matches** — realized value ≈ 0 strict-match fns until
  consumed by a matching wave. Treat as **feedstock**, EV in the low tens of
  eventual matches *at best*, and only if 11-14 pull them.
- **P3 (data pinning):** genuinely unknown. Could be +tens-of-KB `matched_data`
  per clean TU (cheap sweep) or a per-TU grind. **1-TU pilot decides.** Note this
  moves a *different* denominator (`matched_data`), which RFC 10 may argue should
  not count toward "100% of code."

Net: **the measurement work (P1 + P3 pilot) is ~1.5 days and worth doing for
decision value. The veins themselves are secondary feedstock, not a primary
path.** Do not staff a multi-agent wave here before P1's number lands.

## Risks & failure modes

- **ID-inflation mistaken for progress.** More `unified_id` entries look like
  motion but move neither `matched_functions` nor `matched_code`. Guard: report
  P1/P2 as *ID candidates handed to RFC 04/11-14*, never as matches.
- **ICF-alias false anchors.** RTTI HIGH's 24 disagreements are largely ICF folds
  (`??_E`/`??_G` dtors, `*Keys`/`*Vec` template bodies). A data-xref anchor to a
  folded body names the *wrong* mangled symbol. Guard: run `icf_alias_check`
  before crediting; keep both names as candidates.
- **Data pin reads false 0% / regresses.** A pinned data range whose target
  symbol map is wrong reads 0% and can drag WIRED denominators. Guard: cold-cache
  A/B (CLAUDE.md honesty gate), pin only 100%-`.text` TUs first.
- **Ghidra single-process lock.** RB3Xenon project is single-process; if an
  import holds it, P1's Ghidra path stalls. Guard: the dtk-`.s`-scan path (no
  Ghidra) answers the coarse yield question independently.

## Kill criteria

- **P1:** < ~150 new non-trivial fns anchored, OR anchor precision < ~60% on the
  overlap check → global-data-xref vein DEAD; record the null in
  `docs/decomp/research/2026-07-08-data-xref-yield.md`.
- **P2:** MI extension yields < ~40 HIGH hits after the half-day, OR none get
  consumed by a matching wave within the round → DEAD (it's ID-only feedstock).
- **P3:** the 1-TU pilot yields < ~50% `matched_data` on an already-100%-`.text`
  TU → data pinning is its own grind, DEFER to RFC 10's denominator decision.
- **Whole channel:** if `unified_id` size stays > `matched_functions` (i.e. IDs
  keep outrunning matches), every ID lever here is DEFER until matching catches up.

## Open questions

1. Of the ~979 RTTI-walked vtables dtk does NOT label as `vftable_` in the `.s`,
   how many install-sites reference them? (dtk only names 342; the raw `.long`
   pointers are the hidden signal — does labeling them widen P1's yield 3.8×?)
2. Do any *globals* (non-vtable `.data`) have enough xref concentration to anchor
   a TU, or is the whole channel effectively just vtables? (P1 must separate the
   two.)
3. Does `matched_data` even belong in the path-to-100 denominator? Coordinate with
   `10-middleware-and-denominator.md` and `01-endgame-definitions.md` before P3.
4. Can the MI post-`6B` modifier parse reuse `substitute_dc3_class_names`
   unchanged, or does sub-object naming need new logic?

## References

- `docs/decomp/rtti-vtable-transitivity.md` — production RTTI/vtable oracle; 384
  HIGH @ 71.1%, manual-review gating, MI/drift limitations (the P2 backlog).
- `docs/plans/exploratory-techniques.md` §1.2–1.4 — POCs + precision methodology
  (POC 385/753; production 384/730).
- `docs/decomp/callgraph-triangulation.md` — the sibling productionized ID
  oracle (+1,594 @ 94.1%); orthogonality baseline.
- `docs/decomp/string-layout-gap.md` — why `.rdata` string pools lag code
  matching (relevant to P3).
- `tools/fingerprint_match.py` — `_rtti_walk_rb3` (`:885`), `rtti` subparser
  (`:1990`), `vtable` subparser (`:2014`).
- `scripts/dump_vtable.py` — target-obj vtable slot decoder (MATCH-debug, not ID).
- `.claude/skills/{vtable,data-diff}/SKILL.md` — vtable/data slot-diff wrappers.
- `tools/ghidra/ghidra-xrefs.py`, `tools/ghidra/mcp_client.py` — Ghidra MCP
  (port 8002) xref path for P1.
- `config/45410914/splits.txt` — 773 TU entries, 2 (global-header-only) data
  lines; per-TU `.rdata`/`.data` pinning unused.
- `build/45410914/report.json` — matched 11,240/65,619 fns; matched_data 16 /
  4,118,360.
- `unified_id.json` (11,582 addrs), `unified_id_rtti.json` (384 HIGH),
  `unified_id_rtti_low.json` (730 LOW), `unified_id_callgraph.json`.
- Siblings: `04-pinning-at-scale.md` (consumes P1/P2 candidates),
  `07-icf-constraint-solver.md` + `08-ml-embedding-triage.md` (call-graph channel,
  orthogonal), `10-middleware-and-denominator.md` (matched_data denominator),
  `11-permuter-farm.md`/`12-grind-fleet-v2.md`/`13-codegen-idiom-library.md`/
  `14-systematic-symbol-sweeps.md` (the matching waves that must consume IDs).
