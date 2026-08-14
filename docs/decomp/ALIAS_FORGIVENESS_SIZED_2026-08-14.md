# The 720,992 alias-forgiven bytes, adjudicated on retail bytes

**Lane GROUNDED-1, 2026-08-14, on `f4afdf90`.** Follow-up to ALIASAUDIT-1
(`df90b49f`), which sized the alias mechanism at **−720,992 B / −6.985907 pp** of
`matched_code` and correctly flagged that its `OK (grounded)` label means only
*map-consistency*, not proof of folding.

**The question:** of those forgiven bytes, how many can be raised to PROVEN, and
how many cannot?

## Answer

| class | rows | bytes | share | pp of `total_code` |
|---|---:|---:|---:|---:|
| **PROVEN on retail bytes** | 2,718 | **594,904** | **82.51%** | 5.7642 |
| UNPROVABLE — needs absent source | 218 | 44,140 | 6.12% | 0.4277 |
| UNPROVABLE — needs one map identification | 5 | 740 | 0.10% | 0.0072 |
| **CONTRADICTED** (withdrawn, `5a8a8bf9`) | 10 | **1,920** | 0.27% | 0.0186 |
| UNATTRIBUTED by this method | 1,889 | 79,288 | 11.00% | 0.7682 |
| **TOTAL** | **4,840** | **720,992** | | 6.9859 |

**Concentration:** the exposure is extremely concentrated. Only **448 of 1,493
groups forgive any bytes at all**; the top 10 carry **55.6%**, top 25 **72.3%**,
top 50 **85.0%**, top 100 **96.4%**. The single largest is
`??2CriticalSection@@SAPAXI@Z ← ??2@YAPAXI@Z` at 73,496 B.

## ⛔ The brief's premise was half wrong, and in the reassuring direction

The framing "1,287 groups carry essentially all 720,992 B on map-consistency"
conflates two different things:

* `OK (grounded)` is the **validator's re-check** label, computed at gate time
  from map-residency alone;
* the **installation** evidence is recorded per group in
  `scripts/symbol_aliases.json`'s `evidence` field as tiers **T1 / T2 / T3**,
  where T1 is *retail bytes at the survivor address, byte-identical modulo
  relocated fields, with relocation TARGET NAMES compared and anti-vacuity
  guards*. That is a retail-byte proof, and the majority of groups carry it.

So most of the bytes were never resting on map-consistency — the *label* was, and
the label is what should change.

**The mechanical demonstration that `grounded` cannot carry the weight:** the
eight groups this lane emptied of **every** folded spelling — groups that now
declare no fold at all — still land in the `grounded` bucket, and the count did
not move (1287 before, 1287 after). A label insensitive to the claim it appears
to certify is worse than no label. Renamed to `OK (MAP-CONSISTENT)`.

## Method, and why each step is trustworthy

**1. The row set is MEASURED, not modelled.** `scripts/symbol_aliases.json` feeds
`gen_symbol_alias_map.py` → `icf_aliases.map`, which objdiff reads at *report*
time. So a leg is a map regeneration plus a report regeneration with **zero
compiles** — verified, not assumed (the probe refuses if ninja compiled
anything). Running FULL vs EMPTY gives the exact per-row fall set:

```
FULL   1493 groups   matched_code 3,597,584 (34.858067%)
EMPTY     0 groups   matched_code 2,876,592 (27.872160%)
                     4,840 rows fell, totalling 720,992 B  -- RECONCILES EXACTLY
                     0 rows rose
```

Reproducing ALIASAUDIT-1's full `ab_measure` number **to the byte** from a
report-only leg is the control that licenses the cheap instrument.

**2. Attribution divides a known set; it never decides membership.** The
`icf_site_census.py` charged-pair records are used only to split the measured
rows among pairs, so a gap in the census's conservative alignment gate surfaces
as `UNATTRIBUTED` (11.00%) rather than as a wrong answer. Those 1,889 rows are
**anonymous** target-side symbols (`fn_8277C1AC`, median 40 B) that a name-keyed
census cannot pair — a sized, identified gap, not a different kind of evidence.

**3. Adjudication is LAYERED, because flat T1 is one of five channels this tree
owns and it bottoms out exactly where the others were built to work.** A
flat-T1-only sweep returns 55.5% proven / 37.8% undecidable / 4.5% refuted. That
is an artifact of using one instrument:

| channel | pair-bytes | what it establishes |
|---|---:|---|
| `L1_T1` flat retail-byte identity + reloc target names | 470,616 | `icf_alias_build` |
| `L3_EXACT` full-word compare, no vacuity floor | 208,704 | `comdat_fold_gate`'s comparator |
| `L4_OURSIDE` our two COMDATs byte+reloc identical ⇒ `/OPT:ICF` must fold | 101,760 | `ourside_fold_sweep` / `alloc_fold_gate` |
| `L2_RECURSIVE` the ICF fixpoint (`chase`) | 18,544 | name equality closed under proven folds |
| `L5_INCONSISTENCY` retail's own callees name ≥2 instantiations of one family | 1,432 | needs no fold model at all |

⚠ **Flat T1's vacuity guard is right as a guard and wrong as a verdict.** It
exists so a masked `b X` cannot compare equal to `b Y`. But when the destination
is *not* masked — because the relocation target NAMES agree — the destination is
the entire information content of a thunk, so comparing it is the **strongest**
test available, not the weakest (`fold_thunk_gate`'s argument). And when a body
carries **no relocation at all**, nothing is masked and byte identity *is*
`/OPT:ICF`'s complete criterion.

**4. Both new rules were shown able to return the other answer.** Fired at 4,000
random same-size non-alias pairs: `L3_EXACT` says YES on **0.07%**, `L4_OURSIDE`
on **0.05%** (an upper bound on false positives — some decoys genuinely fold).

## Epistemic split inside the thunk class — read this before quoting 82.51%

| | pair-bytes |
|---|---:|
| thunk WITH a relocation whose target name agrees (destination **verified**) | 144,780 |
| thunk with NO relocation (fold proven; **source-level callee irrecoverable**) | 129,360 |
| body ≥16 B (ordinary function identity) | 36,324 |

The middle row is the honest limit. For `lwz r3,0x14(r3); blr`, byte identity
proves the linker folded the bodies — but **which name the call site meant was
destroyed by ICF itself**. The objdiff-level claim ("our `bl` reaches the right
address") holds; the source-level claim ("we call the function HMX called") is
not recoverable from the image by any tool. That is an *irreducible* remainder,
not an unfunded one.

## What the unprovable remainder would need

* **44,140 B — absent source.** e.g. `?MemOrPoolAlloc@@YAPAXHPBDH0@Z ←
  ?MemOrPoolAllocSTL@@YAPAXH@Z` (28,624 B) and `?adler32@D3DX@@YAKKPBEI@Z ←
  adler32` (7,716 B): our spelling is in no compiled obj, so there is no body to
  compare. Porting or pinning decides it — no new instrument required.
* **740 B — one map identification.**
* ★ **The largest single block is one address away from fully verified.**
  `??2CriticalSection@@SAPAXI@Z ← ??2@YAPAXI@Z` (73,496 B) is currently proven
  via L4 (our two COMDATs are byte+reloc identical, so the linker must fold
  them). Both sides are `li r4,0; b <dest>`; retail's dest is **`0x827bcd38`,
  which `target_symbol_map.json` does not name**, and there is **no
  `?MemAlloc@@YAPAXHH@Z` entry in the map at all** (`?_MemAllocTemp@@YAPAXHH@Z`
  sits nearby at `0x827bcff0`). Naming `0x827bcd38` would upgrade this block
  from L4 to fully destination-verified L3.

⚠ **A stale in-tree claim, corrected by re-measurement.**
`tools/alloc_fold_gate.py`'s docstring says `??2@YAPAXI@Z` is **refused on body**
— "ours is 12 bytes (`lis`/`lwz` of `?gNewOperatorAlign@@3HA` then the branch);
retail's is 8". **Today ours is 8 bytes, `38800000 4bfffffc`, byte-identical to
retail's.** The `gNewOperatorAlign` divergence has since been fixed in source, so
that refusal no longer reproduces. The docstring is a dated record; do not act on
it without re-measuring.

## The withdrawal (`5a8a8bf9`): predicted −1,920 B, measured −1,920 B

Eight memberships are refuted by evidence admitting no fold reading — six
`__uninitialized_copy` / `_M_allocate_and_copy` pairs where retail's
instantiation takes a **non-const** pointer and is 96/100 B against our const
104/108 B (two COMDATs of different size cannot fold, so the alias was forgiving
our use of the wrong overload); `Keys<Quat>::Remove` where retail calls
`KeyLessEq` and we call `KeyGreaterEq` (**opposite comparison**); and a
`_Rb_tree_base` pair at 8 B vs 104 B.

The groups are **kept** with `folded: []` plus a `withdrawn` record carrying the
refutation, so a future generator cannot silently re-propose them — and
deliberately **not pruned**, because per `a745039e` a zero-forgiveness spelling
becomes live as porting advances and that prune cost **+94,616 B** to reverse.

⛔ **The size-mismatch class is decisive; the template-twin class is NOT.** Three
`hash_map` rows that flat T1 calls REFUTED are **proven folds by internal
inconsistency** — retail's survivor is named for `SongUpgradeData` while calling
a `_M_find` named for `SongMetadata` and an `_M_insert` named for `SongStatus`:
three `T` in one function is impossible without folding. Withdrawing those would
have been exactly the error this lane exists to avoid.

⚠ **And that check first returned the WRONG answer, conservatively.** The family
parser matched the leading `??$` **function**-template prefix instead of the
class template, so `??$_M_find@H@?$hashtable@…` read as family `_M_find` rather
than `hashtable`; the two callees looked like different families and the rule
refused to prove a provable fold. It was caught only because CLAUDE.md's
documented answer disagreed with the tool. A parser that silently mis-reads
mangled names fails quietly in whichever direction its bug points.

## Reusable finding

**A tool's confident label is worth auditing precisely when it is load-bearing
and cheap to earn.** `OK (grounded)` was earned by map-residency, quoted upstream
as proof, and is provably insensitive to the claim — a group declaring no fold at
all still earns it. The fix was not to distrust the mechanism (82.51% of its
bytes are genuinely proven) but to make the label say what it checks.
