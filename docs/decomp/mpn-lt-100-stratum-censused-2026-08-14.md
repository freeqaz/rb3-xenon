# The `mpn < 100` stratum, censused — lane INSTR-1 (2026-08-14)

Tree `3eb85dfd`, settled worktree, **shipped ruler `functionRelocDiffs=name_check`**
(read from `report.json`'s `provenance.diff_config`, not assumed).
Companion data: `mpn-lt-100-census-INSTR1.tsv`.

Baseline: `total_code` **10,320,664** · `matched_code` **3,725,560 / 36.098064%** ·
`matched_functions` **44,404** · `masked_equal` **22,897** ⇒ honest **21,507**.

## Why this lane existed

The session worked the **arg-only / relocation-name** stratum (`mpn == 100`,
`fuzzy < 100`) to exhaustion. That whole body of work is, by construction, the
population where *every mnemonic already agrees* — RULERGAP-1 put it plainly:
instruction defects there are **0%**. The rows with genuine **instruction**
differences had never been censused, and ~25 pp of reachable surface was
attributed to no lever at all.

## THE CENSUS — the gap decomposes into FIVE classes, not two

Self-validating: rows sum to `total_functions` **69,227** and bytes to
`total_code` **10,320,664**, zero rows dropped; the matched bucket reproduces
`matched_code_percent` to the digit (36.098%). Run that control before believing
any successor to this table.

| class | rows | bytes | % `total_code` |
|---|---|---|---|
| **A** matched (`fuzzy == 100`) | 38,636 | 3,725,560 | 36.098% |
| **B** arg-only / reloc-name (`mpn == 100`, `fuzzy < 100`) | 5,768 | 784,012 | 7.597% |
| **C** `mpn < 100` — instruction defects | 10,268 | 1,978,676 | 19.172% |
| unpairable — `auto_*` (unattributed) | 10,101 | 1,726,060 | 16.724% |
| unpairable — no source (229/230 are `xdk/*`) | 4,454 | 2,106,356 | 20.409% |

Pairable surface = **62.87%** (6,488,248 B); `matched_code` is **57.42% of the
reachable surface**, not of 100%. Gap to ceiling = **2,762,688 B / 26.77 pp**,
and it decomposes **exactly**:

| gap class | bytes | % of gap |
|---|---|---|
| anon `fuzzy == 0` — **UNPAIRABLE at any quality of source** | 1,329,264 | **48.11%** |
| **arg-only / reloc-name (already drained this session)** | **784,012** | **28.38%** |
| named partial — **the DIVERGENCE stratum** | 537,656 | 19.46% |
| anon partial (`0 < fuzzy < 100`), avg 41 B | 60,732 | 2.20% |
| named `fuzzy == 0` (no body / stub) | 50,296 | 1.82% |

⛔ **Do NOT size instruction-matching work against ~25 pp.** Nearly half the gap
is *identification*, not matching, and another 28.4% is the arg-only class this
session already drained (RULERGAP-1: the ruler gap is a strict subset of it,
`|B \ A| = 0`, realisable ≤ 109,708 B). **The surface that source-level
instruction work can address is 537,656 B — 5.21 pp, 2.5× smaller than the
headline suggests.**

Independently confirmed: lane CONSOLIDATE-1 derived 2,762,688 / 1,329,264 /
537,656 / 62.87% / 57.42% from a different starting point and agrees to the byte.

## Shape of the 537,656 B divergence stratum

All 1,140 named partial rows diffed through `objdiff-cli` at the shipped ruler,
**0 errors**, and the CLI agrees with `report.json` on **all 1,498 rows — zero
disagreements even at 0.5 pp**.

| class | rows | bytes | % of stratum | median charged instrs |
|---|---|---|---|---|
| `SOURCE_INSDEL` — real body work | 804 | 446,724 | **83.1%** | **28** |
| `SOURCE_ARG` (imm / opcode / branch) | 251 | 46,356 | 8.6% | **2** |
| `CODEGEN_ORDERING` (scheduling) | 82 | 43,700 | 8.1% | 8 |
| other / relocname-only | 3 | 876 | 0.2% | 1 |

⇒ **`SOURCE_ARG` is the cheap vein** (median 2 charges), `SOURCE_INSDEL` is real
body work at ~10× the cost per row, `CODEGEN_ORDERING` is a floor while the
permuter is off.

Top units by divergence bytes: VocalTrack 18,572 · rndobj/Utl 16,856 ·
Spotlight 13,228 · VocalPlayer 11,632 · BandDirector 9,232 · CameraShot 9,140 ·
BandCharacter 8,992 · DataFunc 8,576 · Mesh 8,568 · BandWardrobe 8,056.

## ★★★ Pure register-allocation rows CANNOT appear in `mpn < 100`

Structural, not a sample. The stratum contains **28,688 register arg-diffs**
across 852 rows — but **0 rows are pure-register**; 806 of the 852 co-occur with
insert/delete. That is exactly what `mpn` excluding non-immediate arg diffs
predicts: a row whose only charges are register swaps scores `mpn == 100` and
lands in class **B**.

⇒ **The permuter-class floor inside the divergence stratum is `CODEGEN_ORDERING`
= 8.1%**, not the ~36% one would extrapolate from EC-3's fuzzy-stratified census
of *all* charged rows. The remaining work is in materially better shape than that
extrapolation implied.

⚠ EC-3 (2026-08-03) reported `CODEGEN_REGALLOC` at **zero in every stratum** and
correctly flagged it as a classifier bug in *its* population. Here zero is the
**right answer** for a different population — which is why the claim was checked
against the register-diff totals before publishing, rather than assumed either way.

## Two hazard corrections (both measured)

- ✅ **The `report.json` size-targeting hazard does NOT apply to named rows.**
  Billed size **equals the asm extent (`target_size`) on all 1,498 rows — 0.00%
  inflation, 0 rows differing.** The "billed 8,852 B, real body 12 B" failure is
  scoped to the anon / `auto_*` population. The general warning is over-broad;
  price named rows from `report.json` freely, anon rows never.
- ⛔ **`CustomizePanel::Handle` (5,036 B) is DRAINED, not open.** It is the
  fattest single-charge row in the tree (41.6% of all single-charge bytes) and
  reads as "one instruction from 5,036 B", but RESIDUAL-2 measured **9 source
  shapes inert and 3 worse**, plus a binary-wide 12-site scan establishing that
  the `clrlwi ,24` bool mask is emitted **only at a PHI**. Do not re-brief it.

## Rows closed (+1,304 B, whole-binary A/B, prediction pre-registered and exact)

Δmatched **+2** · Δ`matched_code` **+1,304 B** · Δcode% **+0.012636 pp** ·
**0 regressions, 0 units off 100%** — against a pre-registered +2 / +1,304 B /
+0.012635 pp.

- **`RndParticleSys::UpdateParticles`** (720 B → 100%). Retail `bne`, ours `beq`
  on the *same* `lbz r11,0x1e0(r3)` — polarity, not layout. `mPreserveParticles`
  set ⇒ return early; corroborated by every other use in the file (`SetPool` and
  the reaping loop are both guarded by `!mPreserveParticles`). ⚠ **DC3 carries
  the inverted test and cannot adjudicate** — our engine is a verbatim DC3 copy,
  so the source diff is empty by construction.
- **`MetaMusic::Start`** (584 B → 100%). `li r7,0x1` vs our `li r7,0x0`; with two
  floats in `f1`/`f2` the 4th arg of `NewStream(const char*,float,float,bool)`
  lands in `r7`, so retail passes `floatSamples = true`. rb3-Wii says `false`, but
  that is the **dev** build, and the `mPlayFromBuffer` branch in the same function
  already requests float samples.
- **`NgSpotlightDrawer::RenderScene`** (588 B, NOT crossed — 2 charges → 1).
  **We were calling the wrong virtual**: retail dispatches slot `0x58`, we
  dispatched `0x54`. Witness that settles it *without* touching layout —
  `SetupForPostProcess` calls `ClearPostProc()` and emits `lwz r11,0x58,r11`,
  which objdiff marks **EQUAL**, so retail agrees `0x58 == ClearPostProc`. Landed
  for correctness though it buys 0 bytes (`matched_code` is all-or-nothing).

## Negative results — do not re-fund

- ⛔ **Commutative FP term order is NOT a lever, re-confirmed twice.** Swapping
  `Add(mRelativeXfm.v, mLastWorldXfm.v, …)` (Part, 412 B, 4 charges) and swapping
  the `fmadds` multiplicands in `RefinePeriod2` (SndAnalysis, 352 B, 2 charges)
  each produced **byte-identical output**. MSVC canonicalises commutative operand
  order regardless of source spelling. Consistent with the standing finding that
  *term order alone FAILS and parens are the lever*.
- ⛔ `sLights.size()` for `end() - begin()` is **byte-identical** (STLport `size()`
  is literally `end() - begin()`).
- **Deferred as codegen, characterised not guessed:**
  `RenderScene`'s residual `srawi. r11,r11,3` vs our `clrrwi. r11,r11,3` — retail
  materialises the ptrdiff/8 where we strength-reduce to a mask, legal *because*
  `numLights` is used only in the `!= 0` test.
  `CharIKHand::Load` (980 B) — a uniform `r31 +0x10` delta that is **stack-slot
  placement, not layout**: `r31+0x50/0x58/0x90` all match and only the slot
  holding a local `String`/`ObjPtr` temp moves (the documented r31 coin-flip,
  resolved to the codegen side).
  `BandCrowdMeter::Handle` (1,384 B) and `StreakMeter::SyncObjects` (1,328 B) —
  one instruction moved one slot; pure scheduling.
  `BandCamShot::OnListAnimGroups` / `MetaPerformer::SyncSave` — retail
  rematerialises a just-constructed temporary's address (`addi r4,r31,0x80`)
  where MSVC hands us the ctor's returned `this` (`mr r4,r3`). Two instances of
  one shape; a possible force multiplier, not chased here.

## Tooling

`/home/free/tmp/instr1/census.py` (census) and `show.py` (per-row charged-
instruction dump at the shipped ruler). Both drive `objdiff-cli` with the
grader's exact config (`name_check` + `ppc.calculatePoolRelocations=false`), which
is what makes their percentages equal `report.json`.
