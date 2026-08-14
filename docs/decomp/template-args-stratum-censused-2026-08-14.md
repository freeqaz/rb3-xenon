# The `TEMPLATE_ARGS` stratum, censused (lane TEMPLATE-1, 2026-08-14)

**Verdict: the stratum is HALF FOLD, and the "wrong template argument in our
source" vein that motivated the lane is 4% of it — 105 pairs / 18,240 B, of
which 8,284 B is realisable.** The large numbers are (A) and *unresolvable*,
not (C).

Measured at `8e6eb9be`, ruler `name_check`, baseline `matched_functions 44,361 /
matched_code 3,611,744 / code% 34.995266 / total_code 10,320,664`.
Tool: `tools/tmplscan.py` (this lane). Queue:
`docs/decomp/template-args-queue-TEMPLATE1.tsv`.

## The census

955 charged pairs / 1,986 sites / 1,490 uncrossed rows / **446,280 B**.
(SIGSCAN-1 published 957 / 2,251 at `3a1af7e3`; three map fixes have landed
since, and reproducing its figure is this tool's self-validation.)

| class | pairs | rows | bytes | share | realisable |
|---|---:|---:|---:|---:|---:|
| **(A)** `A_FOLD_CONSISTENT` | 343 | 685 | **227,560** | 51.0% | 114,876 |
| **(B/C)** `BC_DIFFERENT_UNRESOLVED` | 312 | 414 | 101,492 | 22.7% | 52,868 |
| **undecidable** `UNDECIDABLE` | 190 | 269 | 96,000 | 21.5% | 45,164 |
| **(C)** `C_PROVEN_CALLEE` | 105 | 117 | **18,240** | 4.1% | 8,284 |
| **(A/B)** `AB_ONE_BODY` | 5 | 5 | 2,988 | 0.7% | 644 |

"Realisable" = rows where the pair accounts for **every** charged site, so the
row can actually cross (`matched_code` is all-or-nothing per row).

By tier, the stratum is engine-dominated in every class: (A) engine 142,356 B /
game 80,824 / network 4,380; (C) engine 11,920 / game 6,320 / network 0.

## ⛔ The obvious instrument is vacuous, and it fails SILENTLY

The natural generalisation of CONTAINER-1 is: walk retail's body for the map's
name against our body for our name, following relocation edges (the mechanised
version of its `insert_unique -> bl _M_create_node` walk). **That walk is
contaminated and reports a global constant as a per-pair verdict.**

Every STL container instantiation bottoms out at the shared allocator:

```
_M_create_node<CRC>      vs _M_create_node<int>       -- identical, differ 1 reloc
  ?MemOrPoolAlloc@@YAPAXHPBDH0@Z  vs  ?MemOrPoolAllocSTL@@YAPAXH@Z
    fn_827BCD38 (644 B)           vs  ?MemAlloc@@YAPAXHH@Z (20 B)   <- LEN_DIFF
```

`MemOrPoolAlloc` vs `MemOrPoolAllocSTL` is an unrelated signature divergence
shared by **every** container regardless of `T`, so the walk marks every
instantiation DIFFERENT. Measured against the retail-byte-proven folds in
`scripts/symbol_aliases.json`, it calls **51.4% of them DIFFERENT** — i.e. used
naively it would have reported roughly half the stratum as a phantom
source-defect vein.

★ **The fix is to compare OUR BUILD AGAINST ITSELF** — `ours[RN]` vs `ours[ON]`.
Both sides share every T-independent callee by construction, so the allocator
cancels exactly and only template-argument-caused differences survive. This is
CONTAINER-1's "different-size COMDATs cannot fold" generalised from node size to
the whole body: SAME ⇒ the two spellings are one body ⇒ retail folded them ⇒ the
charge is a fold alias; DIFFERENT ⇒ nothing to fold into ⇒ retail kept both.

## Calibration — both controls could fail, and the first one did

| control | population | result |
|---|---|---|
| POSITIVE, retail-vs-ours walk | 14,418 proven-fold pairs | **51.4% DIFFERENT** (vacuous) |
| POSITIVE, our-build fold test | 12,455 T1 pairs | 73.8% SAME / 25.3% DIFFERENT |
| POSITIVE, **the shipped `C` gate** | 6,636 T1 + corroborated | **94.1% SAME / 5.8% DIFFERENT** |
| NEGATIVE, unrelated pairs | 400 each walk | **99.8% DIFFERENT** (0.2% false SAME) |

⇒ **`A_FOLD_CONSISTENT` carries a ~0.2% false-positive rate; `C_PROVEN_CALLEE`
carries ~5.8%.** The 25.3% residue in the ungated fold test is cases where *our
own* copy of the survivor spelling is imperfect; gating on `mapname =
CORROBORATED` (our bytes reproduce retail's at that address exactly) removes
that branch, which is why the gated cell is 4× cleaner.

⚠ Restricting to T1 (retail-byte) alias groups rather than all groups moves the
figure by 0.8 pp — T2 (our build's own fold classes) and T3 (dc3 transfer) are
weaker tiers, and scoring a retail-grounded instrument against them measures
agreement with a weaker instrument, not correctness.

## ⛔ Two defects caught by disagreement with a published number, not by review

Both were silent, both were decisive-looking, and both inverted the headline:

1. **The alias subtraction forgave nothing.** Groups are `{survivor, folded[]}`;
   the code read `.get("symbols")`, got nothing, and subtracted zero already-
   forgiven pairs — inflating the census from 955 to **1,670** pairs. Caught only
   because 1,670 disagreed with SIGSCAN-1's published 957. The tool now asserts
   the alias set is non-empty.
2. **The map-name self-consistency test was a tautology.** `Resolver.same` had an
   `rn == on -> SAME` shortcut. `namediff` only ever holds *disagreeing* slots, so
   the shortcut was unreachable during recursion and fired **only** on the
   top-level `same(RN, RN)` query — returning CORROBORATED for every pair and
   producing **659 phantom (C) rows**. Removing it flipped the headline class.

⇒ Neither would have been caught by reading the output, which looked clean and
decisive in both cases.

## What is undecidable, and what would settle it

**`BC_DIFFERENT_UNRESOLVED` (312 pairs / 101,492 B)** — our two spellings are
genuinely different code, so no fold; but `same(RN,RN)` is REFUTED, which does
**not** establish (B). `REFUTED` conflates "the map name is wrong" with "our code
for that function simply is not matched yet", and this tree matches ~35% of the
binary. **Settled by:** matching our own copy of the map's spelling — every row
that reaches `fuzzy == 100` converts its pair from REFUTED to CORROBORATED and
falls into (B) or (C) automatically. This class is therefore *downstream of
ordinary matching progress*, not a lever of its own.

**`UNDECIDABLE` (190 pairs / 96,000 B)** — a body is unavailable: the map's name
is not defined in any pinned target obj, or we never instantiate that spelling.
**Settled by:** pinning (a PIN fact, not a map fact — cf. PINHOME-1).

## No alias was installed, and none should be on this evidence alone

`A_FOLD_CONSISTENT` is **T2-grade** evidence (our build's own fold classes),
which `icf_alias_build` already ranks *below* the retail-byte T1 tier. 343 pairs
is far too many to merge on one lane's authority: an unproven alias lifts the
score by construction and the `none`-ruler control **cannot** catch a fabricated
one (flatness there is the signature, not a clearance). The class is handed to
the map owner with its evidence tier attached, exactly as
`scripts/icf_alias_groups.json`'s 1,407 ungated groups were.

## Corrections to the in-tree record

* CONTAINER-1's doc is **correct and its method stands**; what this lane adds is
  that its walk cannot be mechanised naively, because the allocator edge poisons
  the transitive closure. Its own adjudication was hand-done and unaffected.
* This lane found **no** case where the map was circular in CONTAINER-1's sense,
  but it also **did not** re-adjudicate the (A) class per-pair.
