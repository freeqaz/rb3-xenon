# Shiftable relink milestone — full splits, reloc-normalized equivalence, bootable XEX

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: endgame

## Summary

The classic dtk milestone ladder ends at "full relink": every byte owned by a
unit, the whole binary relinks from compiled+extracted objs, boots on hardware.
This RFC assesses that milestone for rb3-xenon and concludes: **the full bootable
relink is a post-~80% milestone, not a near-term one** — but two of its rungs
(reloc-normalized equivalence as a metric, and a `.text`-only "shiftable"
sub-bar) are cheap-to-adopt intermediate bars that are *already 90% built* in our
tooling. Pursue those two rungs now; defer the XEX repack.

## Motivation

Decomp projects use a milestone ladder as a north star:
1. **split** — dtk owns every section byte in `splits.txt`;
2. **shiftable** — the whole binary matches under *reloc-normalized* diff (code
   bytes identical modulo relocation addresses), so it would relink if you could
   link it;
3. **relink** — compiled+extracted objs actually link into a working binary that
   boots on hardware/emulator.

For a 17%-matched project the value of naming this ladder is not "let's boot a
relinked RB3 tomorrow" — it is (a) a *denominator-honest* progress axis that
climbs before STRICT does, and (b) a forcing function for **byte ownership**:
you cannot relink code you have not split, so "what fraction of `.text` is
owned?" becomes a first-class number. Sibling `01-endgame-definitions.md`
already names this "Endgame C" and defers the design here; this RFC provides it.

## Current state (verified)

All numbers verified 2026-07-08 against `build/45410914/report.json`,
`config/45410914/splits.txt`, `tools/fuzzy_progress.py`, and `../jeff` source.

### Section ownership — the byte-coverage picture

Whole-binary section sizes, from `dtk xex info orig/45410914/default.xex`
(VirtualSize):

| Section | VirtualSize | Bytes | Split-owned today |
|---------|-------------|-------|-------------------|
| `.text`  | 0x009B48D4 | 10,176,724 | **31.46%** (coalesced) |
| `.rdata` | 0x001E95AC |  2,004,396 | ~0% (no address-ranged pins) |
| `.pdata` | 0x0006F370 |    455,536 | auto-derived from `.text` pins |
| `.data`  | 0x001F35EC |  2,045,420 | ~0% |

`.text` ownership is computed by coalescing every `.text start:… end:…` range in
`splits.txt`: **1,215 raw spans → 921 non-overlapping spans → 3,201,460 bytes =
31.46% of the 10.18 MB `.text`**. The pinned extent runs
`0x82262960 → 0x82C0B578`, so ownership is *scattered within* a ~9.8 MB window,
not a contiguous prefix. (Note: naively summing `total_code` per report unit
gives 100% and summing raw `.text` section `size` fields gives 182% — both are
artifacts. The report's `total_code` denominator (11,074,108) is objdiff's
tracked-code figure, and raw span sums double-count ICF-shared spans. The
coalesced 31.46% over the true `.text` VirtualSize is the honest coverage.)

The three `.rdata`/`.data`/`.idata` lines in `splits.txt` are
**section-header templates** (`type:rodata align:65536`) with **no `start:end`
range** — they do not pin any bytes. So data-section ownership for relink
purposes is effectively zero. (`05-data-xref-anchoring.md` is the vein that would
change this.)

**Bottom line: ~31% of code bytes and ~0% of data bytes are split-owned. A relink
requires ~100% of both.**

### What jeff/dtk can and cannot do

`../jeff` (the local dtk fork) `xex` subcommands are: `Disasm`, `Extract`,
`Info`, `Map`, `Pdb`, `Split` (`../jeff/src/cmd/xex.rs`). Its `xex` util
(`../jeff/src/util/xex.rs`) exports `extract_exe`, `process_xex`, `write_coff`
(splits a unit's COFF **out** of the XEX), `list_exe_sections`,
`genuine_except_data_set`. There is **no** `link`, `pack`, `relink`, `write_xex`,
or `build_xex` function anywhere in jeff (grep confirmed). The `dol` command
(the GC/Wii heritage) has no link/make subcommand either.

**jeff is extraction-only: XEX → objs. The inverse (objs → bootable XEX) does not
exist in our toolchain.** This refutes any reading of the ladder that assumes dtk
"relink" is a button we already have. Upstream dtk's GC/Wii "relink" also is not
one tool — it delegates to devkitPPC `ld` under ninja; no equivalent linker path
exists for XEX2 in this repo.

### Reloc-normalized equivalence — already computed

The key good news: **the intermediate "shiftable" bar is already the scoring
basis of our fuzzy metric.** The objdiff fork
(`../objdiff/objdiff-core/src/diff/mod.rs`) computes
`match_percent_normalized` with `functionRelocDiffs=none` — code bytes compared
*ignoring relocation-address differences*. `tools/fuzzy_progress.py`'s HISTOGRAM
bins by exactly this field, and the MCP `run_diff_inspect` exposes
`diff_mode: normalized` (the default) vs `raw`. So "does this function match
modulo relocations?" is a query we can already answer per-function and aggregate.

Current normalized staircase (`tools/fuzzy_progress.py`, WIRED set):
`==100: 11,240 | >=95: 12,743 | >=90: 13,021 | >=80: 13,175 | >=50: 13,407`,
over n=13,584 wired functions of 65,619 total. The `==0` band is **51,999
functions** ≈ the **52,035 unwired** functions — i.e. ~79% of functions are not
even attempted, and **42,126 of those are `thirdparty/vendor/xdk` with 0 wired**
(Bink, Quazal, XDK/CRT — see `10-middleware-and-denominator.md`).

### The XEX2 repack problem — what the docs actually prove

A scout claimed docs describe a proven OSS PE-link + XEX-pack path. **Partly
true, but critically mis-scoped.** `docs/plans/oss-build-path.md` (Lane B) and
`docs/plans/same-instrument-packer-status.md` do exist and prove an
XDK-free compile→PE-link→XEX-pack pipeline **for a single additive hook TU**
(`SameInstrumentHooks.c`, an RB3Enhanced mod), injected into a fixed-VA **code
cave** and shipped as a Xenia `.patch.toml`. Verified reality per those docs:

- Compile `.c` → PPC COFF (`cl.exe` under wibo, machine `0x01f2`): **PROVEN**.
- Generate import libs from `.def` (`link /LIB`): **PROVEN** (mechanism).
- Link `.obj` → PPC PE DLL: **PROVEN** on a trivial TU.
- **PE → XEX2 pack: PLAUSIBLE, not proven** — "no open-source PE→XEX2 builder
  exists as a turnkey tool; all mature OSS tooling goes XEX→PE." Three
  mitigations (reuse a released RB3E XEX shell + patch; ~400-line packer from the
  Xenia spec; xorloser XexTool under wine).

**This proves we can build a bootable *mod DLL / code-cave patch*, NOT a full
relink of RB3 from our compiled objects.** Sibling `01-endgame-definitions.md`
lines 104–107 make the same correction. Do not cite oss-build-path.md as evidence
that objs→bootable-RB3-XEX is solved. It solves a ~1-TU additive artifact whose
XDK surface is nearly nil; a full relink must additionally: link *all* ~745 TUs
(including every XDK/CRT/Bink/Quazal consumer), resolve all imports, lay out all
data sections, and produce a correctly-relocated XEX2 image at base
`0x82000000`, entry `0x82816080`.

## Proposal

Adopt the ladder as a **metrics + sub-milestone** structure, and build only the
cheap rungs now. Concretely, three deliverables in priority order:

### D1 — Reloc-normalized equivalence as a reported bar (cheap, do now)

The metric already exists per-function; surface it as a whole-binary bar.

- Add a `--shiftable` mode to `tools/fuzzy_progress.py` that reports, over the
  **owned** (`.text`-pinned) bytes: `normalized-100 bytes / owned bytes` and
  `normalized-100 fns / owned fns`. This is "of the code we've split, how much
  would relink correctly ignoring reloc addresses." Today that is ~962,656
  strict but a *higher* normalized number (the staircase shows +1,503 fns reach
  >=95 normalized beyond the 11,240 strict) — quantify it exactly.
- Data flow: read `report.json`, filter to units with a pinned `.text` section,
  bucket functions by `match_percent_normalized` (already emitted per-fn), sum
  size-weighted. No new diff runs; pure report post-processing.
- This gives sibling `18-metrics-and-dashboard.md` a "shiftable coverage" column
  and an honest sub-goal that climbs before STRICT.

### D2 — `.text`-only "shiftable" sub-milestone (medium, do next)

Define the first *reachable* rung: **every function in `.text` is either
strict-100 or normalized-100**, i.e. the code section would relink if a linker
existed, with relocations re-resolved by the linker. This requires two things we
can drive incrementally:

1. **Full `.text` split ownership** — get coalesced `.text` coverage from 31.46%
   → ~100%. This is exactly the `04-pinning-at-scale.md` vein (automate
   `splits.txt` backfill) plus `02-gap-composition-atlas.md` (what's in the
   unowned 69%). Ownership does not require *matching* — a pinned-but-unmatched
   unit still owns its bytes; the relink just carries the extracted target obj
   for unmatched units (dtk's `write_coff` already emits per-unit target COFFs).
2. **Padding/FORCEACTIVE equivalence for gaps** — where a unit is unmatched, the
   relink substitutes the *extracted* target obj byte-for-byte. This is the
   "shiftable with extracted-obj fallback" model: matched units contribute *our*
   code, unmatched units contribute dtk-extracted code, and the union tiles
   `.text` completely. The metric to track: **owned-`.text` fraction** (byte
   tiling), independent of match%.

Deliverable: a `tools/relink_coverage.py` that reports owned-`.text` %,
owned-`.data`/`.rdata` %, and the largest unowned gaps (VA + size) as a worklist
for pinning-at-scale.

### D3 — XEX2 repack spike (large, DEFER until D2 near-complete)

Only once `.text`+`.data` are ~fully owned does a repack make sense. Path,
staged from the oss-build-path.md evidence:

1. Extend the OSS pipeline from 1 TU to N: link all compiled objs + all
   extracted target objs into one PPC PE image via `link.exe` under wibo,
   using generated import libs (`.def` → `link /LIB`) for XDK/kernel/XAM
   ordinals (tables from Xenia `kernel/xboxkrnl/*`, `xam_table.inc`).
2. PE → XEX2 pack via the ~400-line packer from the Xenia XEX2 spec
   (`docs/reference/FREE60_XEX_FORMAT.md` is the archived format reference), or
   reuse a released XEX as a shell and rewrite section bytes.
3. Boot on Xenia headless (the `xenia-gameplay` skill + the packer-status doc's
   verified loader analysis: image base `0x82000000`, entry `0x82816080`, PE
   name `band.exe`).

D3 is a multi-week effort and **gated on D2** — do not start it early.

## Alternatives considered

- **Relink now at 31% (partial-link probe).** Rejected: with 69% of `.text` and
  ~100% of data unowned, and 42k vendor functions unattempted, a link would fail
  on tens of thousands of unresolved symbols. No signal, high cost.
- **Skip the metric, treat relink as pure endgame.** Rejected: the
  reloc-normalized metric is nearly free (D1) and immediately useful as an
  honesty axis; leaving it unreported wastes an already-built capability.
- **Use extracted objs for *everything* and relink immediately** (a trivially
  "shiftable" binary that is 0% our code). This is real — dtk can already emit
  every unit's target COFF — and would prove the *link+pack* machinery end to
  end independent of match progress. Worth considering as a **D3-first spike**
  to de-risk the packer, but it delivers no match value and is easy to mistake
  for progress; if done, label it explicitly as a toolchain probe.
- **Native port instead of relink** (`20-native-port-and-engine-reuse.md`). A
  different, arguably better "playable RB3" track that sidesteps XEX2 entirely by
  running the engine on x86_64. For *playability* the native track dominates; the
  relink track's unique value is proving *machine-code fidelity on real
  hardware*, which native cannot.

## Effort & expected value

Anchored to comparable past results in this repo:

| Deliverable | Effort | Expected value |
|-------------|--------|----------------|
| D1 (normalized bar) | ~0.5 day | A reported "shiftable coverage" number; +0 matches but sharpens `18-`'s dashboard. Low risk, near-certain. |
| D2 (`.text` full ownership) | Weeks–months, = the `04-pinning-at-scale.md` cost | Owned-`.text` 31%→~100%. This is *identification/pinning* effort re-labeled; the relink framing adds a byte-tiling target but not new matches. |
| D3 (XEX2 repack) | Multi-week, high uncertainty on the PE→XEX2 packer | A bootable relinked XEX — the headline endgame artifact. Value is *demonstration*, not match%. |

**Honest EV verdict:** as a **match-producing** vein, this RFC is ~0 — relink
produces no new strict matches. Its value is (a) the near-free normalized metric
(D1), and (b) a *forcing frame* that makes byte-ownership legible and gives the
project a crisp definition of "done" (D2/D3). The bootable artifact (D3) is a
genuine endgame trophy but only makes sense **past ~80% `.text` ownership**, and
even then depends on the unproven PE→XEX2 packer. Compared to the biggest past
wins (class-A span harvest +403 in a session; grind loop +22), the *match* return
here is zero, so it must not compete for grind-fleet time.

## Risks & failure modes

- **PE→XEX2 packer never materializes.** The one unproven step. If no OSS packer
  works, D3 dies and only the metric (D1) + ownership target (D2) survive. This is
  the single biggest risk and it lives entirely in D3.
- **Extracted-obj fallback masks unmatched code.** If we count "owned" bytes as
  progress, a binary that is 100% *extracted* (0% ours) looks "shiftable" — a
  denominator trap. Mitigation: always report owned-% and matched-% as separate
  axes; never let owned-% stand in for match progress. This mirrors the project's
  standing honesty norm.
- **ICF / merged spans corrupt the ownership count.** `.text` spans are
  ICF-shared; naive summation over-counts (we saw 182%). Any coverage tool must
  coalesce and de-dupe (as this RFC's 31.46% figure does) and pass through
  `icf_alias_check`.
- **Relocation-normalized ≠ relink-correct.** Normalized-100 means bytes match
  ignoring reloc *addresses*; a real link must still resolve every reloc to the
  right symbol. A function can be normalized-100 yet reference a symbol we lay out
  at the wrong VA. So D2's metric is necessary-not-sufficient for D3.

## Kill criteria

- **Kill D1** if the normalized bar turns out identical to strict (no
  reloc-only-different functions) — then it adds no information. (Refuted already:
  staircase shows +1,503 fns at >=95 normalized beyond strict-100, so D1 has
  content.)
- **Kill D3** if an OSS PE→XEX2 pack cannot be demonstrated on even a trivial
  full-image link after a bounded (~1 week) spike — fall back to metric-only.
- **Kill the whole vein's *priority*** (not the metric) if grind/pinning ROI stays
  positive elsewhere: relink produces 0 matches, so any session with live
  match-veins should spend there first. This RFC is a **DEFER** on the artifact,
  **DO-NOW** on the D1 metric.

## Open questions

- Does dtk's `write_coff` emit target objs link-ready (correct symbol scope,
  section flags) or only diff-ready? If diff-ready-only, D3's extracted-obj
  fallback needs a COFF post-process step.
- What is the actual normalized-100 *byte* fraction over owned `.text` today?
  (D1 answers this; estimated between the 8.69% strict-code and the WIRED
  94.6% fuzzy — needs computation.)
- Do the 42,126 vendor/XDK functions need real source at all for a relink, or can
  Bink/Quazal/XDK ship as extracted-obj blobs permanently (see
  `10-middleware-and-denominator.md`)? If blobs are acceptable, D2's ownership
  target shrinks to the non-vendor `.text`.
- Base-VA fidelity: does a relinked image have to reproduce `0x82000000` /
  entry `0x82816080` exactly, or is any bootable layout acceptable for the
  "boots on Xenia" bar?

## References

- `config/45410914/splits.txt` — split ownership source (1,215 `.text` spans,
  coalesced to 921 / 3,201,460 B / 31.46% of `.text`).
- `build/45410914/report.json` — measures: `total_code` 11,074,108; `matched_code`
  962,656; `total_functions` 65,619; `matched_functions` 11,240.
- `tools/fuzzy_progress.py` — normalized staircase + WIRED fuzzy 94.602%.
- `../jeff/src/cmd/xex.rs`, `../jeff/src/util/xex.rs` — jeff XEX capabilities
  (extract/split/info only; **no link/pack**; `write_coff`,
  `genuine_except_data_set`, `list_exe_sections`).
- `../objdiff/objdiff-core/src/diff/mod.rs` — `match_percent_normalized`,
  `functionRelocDiffs=none` (reloc-normalized scoring already implemented).
- `docs/plans/oss-build-path.md` — Lane B XDK-free compile→PE-link→XEX-pack
  pipeline (PROVEN through PE link; PE→XEX2 pack PLAUSIBLE, for a single mod TU).
- `docs/plans/same-instrument-packer-status.md` — code-cave `.patch.toml` packer;
  Xenia loader analysis (base `0x82000000`, entry `0x82816080`, PE `band.exe`).
- `docs/plans/binary-patch-path.md` — "the feature needs a compiler, not the XDK"
  scoping.
- `docs/reference/FREE60_XEX_FORMAT.md` — [HIST] archived XEX2 format reference.
- `dtk xex info orig/45410914/default.xex` — section VirtualSizes (`.text`
  10,176,724 B; `.rdata` 2,004,396; `.pdata` 455,536; `.data` 2,045,420).
- Sibling RFCs: `01-endgame-definitions.md` (Endgame C, defers design here;
  refutes the oss-build overclaim), `02-gap-composition-atlas.md` (the unowned
  69%), `04-pinning-at-scale.md` (drives D2 ownership), `05-data-xref-anchoring.md`
  (data-section pins), `10-middleware-and-denominator.md` (vendor blobs),
  `18-metrics-and-dashboard.md` (consumes the D1 bar),
  `20-native-port-and-engine-reuse.md` (the alternative playable track).
