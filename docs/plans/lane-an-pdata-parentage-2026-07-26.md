# laneAN — `.pdata` parent-funclet association: a HARD attribution signal (2026-07-26)

Baseline at lane start: **36,069** strict (`match_percent_normalized == 100.0`),
main HEAD `a059e4a8`. Baseline pickle `/home/free/tmp/laneAN_base_strict.pkl`.

Predecessors, read first: `docs/plans/lane-am-diffunit-2026-07-26.md` and
`docs/plans/lane-al-autocarve-2026-07-26.md`.

Tool: **`scripts/harvest/pdata_parent_owner.py`** (this lane).

## Why the previous lanes needed this

Every attribution channel built for *unowned* retail code so far is a
**similarity score**. objdiff's `pair_funclets_by_bytes` pairs a target
`fn_<VA>` with any funclet-shaped base symbol whose reloc-masked bytes are
equal. laneAM quantified how weak that is as *identity* evidence:

* only **128 of 1,531** evidenced functions have a byte signature unique to one
  unit tree-wide (1.8%); the modal 32-byte shape occurs in **694 of our 1,024
  objs**; median `LEFT_ONLY` multiplicity is **122 units**;
* the report driver sets `function_reloc_diffs: None`, so **"byte-true" means
  identical modulo every relocation target** — same instruction bytes, different
  callees, still a true 100%;
* a held-out leg handing every gap to a *certainly wrong* unit still scored
  **36.7%** of functions (57.1% at 17–32 B).

So a `LEFT_ONLY` verdict usually means only "the right neighbour happens not to
carry a stereotyped shape that 122 other units do". It cannot exclude a **third**
unit. laneAM closed by naming the missing signal:

> "The only way to improve attribution here is a *third* signal that can exclude
> third-unit owners: parent-funclet association (`.pdata` parent → owning TU) is
> the obvious candidate and was not built in this lane."

## The signal

MSVC emits every EH funclet **while compiling its parent function**, so a
funclet's object file *is* its parent's object file. Find the parent, find the
parent's unit, and every other unit is **impossible**. This never looks at bytes,
so it is immune to the relocation-masking hazard above.

Chain (already implemented in `scripts/harvest/funclet_cascade_rank.py`, reused
here rather than re-derived): `.pdata` exception flag → the two DWORDs
immediately *before* the entry point are `{handler, handlerData}` → `handlerData`
is an MSVC `_s_FuncInfo` (magic `0x19930522`) → its unwind-map actions and
try-block catch handlers are exactly the funclet entry points.

### ★ It is a near-bijection — measured, not assumed

| | |
|---|--:|
| `.pdata` entries | 57,733 |
| exception-flagged | 9,145 |
| resolved a valid `_s_FuncInfo` | 9,082 (all magic `0x19930522`) |
| **EH-derived funclets** | **26,321** |
| …with exactly ONE parent | **26,312 (99.97%)** |
| …multi-parent (ICF fold) | **9** |

The 9 multi-parent funclets are the one real failure mode (an ICF fold makes
ownership genuinely ambiguous). ★`funclet_cascade_rank.parse_eh` uses
`setdefault` and therefore **silently drops the second parent** — its "zero
mis-attribution" claim is not quite right, it is 9/26,321 = 0.034%.
`pdata_parent_owner.py` detects them and **refuses to attribute them**.

Cross-check against the independent prologue screen (`subi rX, r12, imm` as
instruction 0): |EH ∩ screen| = **16,999**, |screen − EH| = **0**, |EH − screen| =
9,322. Every prologue-screened funclet is EH-derived; the EH channel finds 55%
more. Funclet sizes: 9,309 at 17–32 B, 15,142 at 33–44 B, 1,812 at 45–68 B, 52 at
69–84 B, and **6 above 84 B** — see the cap section.

## ★ STEP 0: the funnel — and where it collapses

Funnel over laneAM's 7,265-function different-unit gap pool, with the parent's
unit taken from **pre-laneAM pins** so the evidence is independent of the fills
being judged (vendor window `0x828–0x82C` excluded *inside* the funnel):

| verdict | count |
|---|--:|
| `PROVES_LEFT` | 1,244 |
| `PROVES_RIGHT` | 31 |
| **`THIRD_UNIT` (excludes both neighbours)** | **54** |
| parent unpinned — no verdict | 2,625 |
| **no EH parentage at all — no verdict** | **3,311** |

**18.3% of the pool gets a hard verdict.** By laneAM's class:

| laneAM class | PROVES_LEFT | PROVES_RIGHT | THIRD_UNIT | parent unpinned | no EH parent |
|---|--:|--:|--:|--:|--:|
| LEFT_ONLY | 537 | 15 | 7 | 450 | 0 |
| RIGHT_ONLY | 62 | 14 | 7 | 438 | 1 |
| **BOTH** (coin flips) | **554** | **2** | **8** | 763 | 1 |
| NEITHER (unreachable) | 91 | 0 | 32 | 974 | **3,309** |

### The answer to the funding question, up front

The lane was funded to report *"how many of the 4,287 unreachable functions
parentage can exclude a wrong owner for"* before funding any fills.

> ★ **It collapses. Of the ~4,400 unreachable functions, only 123 get any
> verdict (2.8%), and only 32 of those are third-unit exclusions — because
> 3,309 of them (75%) have no EH parentage at all.** Parentage does not rescue
> that residue. It is a source problem, exactly as laneAL and laneAM both
> concluded for their own residues, and it stays that way.

Third-unit exclusion across the whole gap pool is **54 functions**. As a
*discovery* channel this is small. What parentage is actually worth is
**re-classifying evidence that already exists**, and there the numbers are large.

### ★ A mechanical fact the previous lane could not see

`PROVES_LEFT` outnumbers `PROVES_RIGHT` **1,244 : 31 — 40:1.** Worker B's
spatial census explains it: **all 26,321 funclets lay out at a *positive* offset
from their parent** (0 negative-delta cases; 8,003 immediately adjacent, 18,318
pooled 65 B–16 KB later). The linker places a TU's funclets *after* its parent's
code, so an unowned gap sitting immediately after unit L's pinned span is
overwhelmingly L's own funclet pool.

laneAM observed the same left bias, then **retracted** its "left bias is a
genuineness floor" argument after a held-out calibration showed that handing a
gap LEFT manufactures a match for 78.8% of truth-*right* functions vs 46.0% the
other way — i.e. the null is not symmetric, so the excess bounds nothing.

★That retraction remains correct *as a statement about byte evidence*, but
parentage supplies the missing physical reason and reaches the same conclusion
by an independent route: the left bias in the real pool **is** real, at 40:1
among functions that carry hard evidence. laneAM's decision to fund LEFT-heavy
sub-range cuts was better-founded than laneAM itself believed.

## ★★ The main result: 95.4% of the anonymous match pool is now PROVEN

Applied to **every** anonymous `fn_` function currently at strict 100%, not just
this lane's or laneAM's fills (vendor window excluded):

| | count | share |
|---|--:|--:|
| anonymous `fn_` at strict 100% | **18,439** | 100% |
| **`PROVEN`** — parent's unit == claiming unit | **17,590** | **95.4%** |
| **`CONTRADICTED`** — parent lives in a different unit | **532** | 2.9% |
| parent unpinned — undecidable | 314 | 1.7% |
| no EH parentage — undecidable | 3 | 0.02% |

That is **just over half of all 36,069 strict matches**, re-classified by a
signal that never looks at bytes.

### Is it circular? Measured, and mostly no

The concern is real: both sides of the comparison come from `splits.txt` pins, so
if a pin was *itself* created by a byte-signature fill, `PROVEN` only says "the
pin covers both funclet and parent". Quantified by re-running the parent's unit
lookup against **pre-laneAL pins** (i.e. pins that predate today's entire
byte-signature fill era):

| | count |
|---|--:|
| PROVEN with the parent pinned in the **pre-fill era** (non-circular) | **16,291** |
| PROVEN whose parent's pin was created by today's fills (circular) | 1,299 |
| PROVEN whose pre-fill pin **disagrees** with the claim | **0** |
| …and, independently: PROVEN whose **parent is `target_symbol_map`-NAMED** | **11,985** |

★The test is also **not vacuous**: if `PROVEN` merely meant "one big span
happens to cover both", the contradiction rate would be ≈0. It is **2.9%
(532/18,439)**, and the contradictions concentrate in specific units
(`SaveLoadManager.cpp` 73, `RockCentral.cpp` 56) rather than spreading uniformly
— i.e. the signal discriminates, and it localises real pin defects.

So **92.6% of the PROVEN set rests on a pin that predates the fills**, and
**68% of the parents carry a mangled name** — the strongest identity evidence
this project has. The mechanism is an *evidence transfer*: a 40-byte stereotyped
funclet inherits the (strong, often name-backed) attribution of its parent, which
is a large real function.

## Retro-classification of the existing unresolved fills

`docs/plans/laneAM/fills-T1.json` (1,419 `exclusive_byte_signature` + 208
`identity_unresolved`), audited against **pre-laneAM** pins:

| tier | n | PROVEN | CONTRADICTED | undecidable |
|---|--:|--:|--:|--:|
| `exclusive_byte_signature` | 1,419 | 508 | 87 | 824 |
| `identity_unresolved` (pure coin flips) | 208 | **89** | 1 | 118 |
| **total** | **1,627** | **597** | **88** | 942 |

laneAL's interior-hole sweep has no manifest, so it was reconstructed by
differencing `splits.pre_laneAL.bak` against `splits.pre_laneAM.bak` (3,558
functions newly claimed), with the parent's unit taken from **pre-laneAL** pins:

| | n |
|---|--:|
| PROVEN | **1,608** |
| CONTRADICTED | 74 |
| parent unpinned | 867 |
| no EH parent | 1,009 |

> **Retro-classification total: 2,205 fills upgraded from "byte-true,
> attribution-unresolved" to PROVEN** (597 laneAM + 1,608 laneAL), of which
> **89 were laneAM's pure coin flips** — the tier laneAM described as "funded
> only because they are geometrically interior to an evidence-carrying cut".
> **162 fills are contradicted** (88 laneAM + 74 laneAL) and 370 more
> contradictions predate both lanes (532 tree-wide).

The `_splits_fill_unresolved_comment` doctrine in `scripts/target_symbol_map.json`
— "treat all 1,627 as unit-attribution-unresolved" — can now be replaced by a
**per-function verdict**, and should be.

## ★ The uniqueness-gating contradiction: settled — it was never true

Three artifacts described objdiff's anonymous pairing as a "uniqueness-gated
reloc-masked byte signature": `lane-al-autocarve-2026-07-26.md` (3 spots),
`scripts/harvest/splits_move.py`, and `scripts/harvest/autocarve_funnel.py` (the
last never flagged by any lane). laneAM said "pass 1 only". laneAM is right:

| pass | line | uniqueness? |
|---|--:|---|
| 1 | ~1471 | **yes** — `left_indices.len() != 1` and `right_indices.len() != 1` both skip |
| 2 | ~1507 | no — ambiguous groups paired greedily by name-sorted `zip` |
| 2b | ~1553 | no — over-subscribed groups paired **many-to-one** onto an already-consumed base funclet, deliberately, without setting `right_used` |
| 3 | ~1578 | no — same-size fuzzy at ≥50% masked byte equality |

★**And it was not a claim that went stale.** `git show b01e3efa` (objdiff,
2026-05-27) — the commit that *introduced* `pair_funclets_by_bytes` — already
contains passes 1, 2 **and** 3 side by side. Only 2b was added later
(`48a52557`, 2026-05-29). The claim was wrong when it was written, two months
later.

Measured counter-example (read-only, no rebuild): `default/RockCentral`'s
`fn_82286DDC` (40 B) scores `normalized_match_percent: 100.0` with
`masked_equal_symbol: true` on a signature carried by **15 target-side and 7
base-side** funclets. Pass 1 cannot have produced that pairing.

Net rule: **a target funclet scores 100% iff its masked bytes equal *at least
one* funclet-shaped Code symbol in the assigned unit's obj — multiplicity on
either side is irrelevant.** A wrong owner does not need a unique signature to
score. Corrections landed in commit `7f7c057b`.

## The 84-byte cap

`is_funclet_like` contains no size constant. The cap is **emergent**: the target
side names every anonymous function `fn_<VA>` (always funclet-like), but our
compiled base objs contain no `fn_` names, so base candidates are only
`__unwind$N` / `__catch$N` / `??__E*` / `??__F*` — and those are small. This
lane's own funclet census corroborates the number from the other side: of 26,321
retail EH funclets, **26,315 are ≤ 84 B and only 6 exceed it**.

*(Worker A's measurement of the base-obj size histogram, whether the base-side
gate is load-bearing, and the measured A/B of lifting it — filled in below.)*

## Reproducing

```bash
python3 scripts/harvest/pdata_parent_owner.py census
python3 scripts/harvest/pdata_parent_owner.py span 0x822715F8 0x822717A0
python3 scripts/harvest/pdata_parent_owner.py \
        --splits /home/free/tmp/splits.pre_laneAM.bak \
        audit docs/plans/laneAM/fills-T1.json --json out.json
python3 scripts/harvest/pdata_parent_owner.py actionable --json micropins.json
python3 scripts/harvest/pdata_parent_owner.py gaps --json gaps.json
```

`--splits` pointing at a pre-edit backup is what makes an audit **independent of
the edit being audited**; without it the audit is circular. All modes are static
— **no build required**.

## Remaining, honestly

* **Third-unit exclusion is a small discovery channel** (54 in the gap pool). Its
  value is evidence class, not match count.
* **The 3,311 gap functions with no EH parentage stay unreachable by any
  attribution signal.** They are ordinary non-EH code we simply do not compile.
* **532 tree-wide contradictions** are a live handoff: byte-true matches credited
  to a unit that parentage says cannot own them. Listed in
  `docs/plans/laneAN/contradictions-treewide.json`. By era: **420 predate
  laneAL** (i.e. they are not today's doing), 22 laneAL, 90 laneAM. 319 of 532
  are 32 B — the modal static-init-guard shape. Top claiming units:
  `band3/meta_band/SaveLoadManager.cpp` 73, `RockCentral.cpp` 56,
  `band3/meta_band/SessionMgr.cpp` 36, `TourProgress.cpp` 29.
  (The 74 laneAL / 88 laneAM figures above count *fills*; these count fills that
  are *currently at 100%* under *current* pins — a slightly different set.)
* **The obvious extension is drained — do not fund it.** When *both* funclet and
  parent are unpinned (1,210 tree-wide), the parent's own identity could in
  principle supply the unit via `scripts/target_symbol_map.json`. Measured:
  **only 30 of the 1,210 have a map-named parent** (22 distinct parents), and
  they are STL/`StaticClassName` boilerplate. Recorded at
  `/home/free/tmp/laneAN/named_parent_leads.json`.
* The parent-unpinned residue (2,625 in the gap pool, 314 tree-wide) becomes
  decidable for free as more parents get pinned — **re-run this after any splits
  wave**; it costs no build.
