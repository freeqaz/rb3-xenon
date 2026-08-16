# The unproven alias remainder, adjudicated — lane ALIAS-2, 2026-08-16

Follow-up to GROUNDED-1 (`ALIAS_FORGIVENESS_SIZED_2026-08-14.md`), which split
the alias-forgiven bytes on retail-byte evidence and reported **11.00% /
79,288 B "UNATTRIBUTED by this method"**. This lane asked what that class
actually is, and adjudicated the rest of the unproven remainder.

## Headline

**The 11.00% class is not an evidence class. It is 100% PROVEN**, and it existed
only because the attributing instrument was name-keyed.

| GROUNDED-1 class | its size | this lane's verdict |
|---|---:|---|
| UNATTRIBUTED by this method | 79,288 B (11.00%) | **100% rests on PROVEN memberships — 0 B on anything weaker** |

And the mechanism as a whole is **bigger than it was**: re-measured on today's
1,528 groups it is worth **818,416 B / 7.929877 pp** (GROUNDED-1: 720,992 B /
6.985907 pp over 1,493 groups), `matched_functions` **+0**, `masked_equal` **+0**
— the documented arg-blind shape, reconciling exactly.

⚠ **Do not quote GROUNDED-1's byte figures as current.** They are a dated
measurement of a smaller group set, and this doc's will date the same way. The
mechanism is re-measurable in ~5 s (below); measure it.

## Why the class existed, and why it is now closed by construction

GROUNDED-1 attributed each fallen row to alias PAIRS drawn from
`icf_site_census`'s charged-pair records, which are keyed on the row's symbol
NAME. **1,886 of the 1,894 rows in this class (98.8%) have an anonymous
target-side symbol** (`fn_8277C1AC`, median **40 B**), which a name-keyed census
structurally cannot pair. The class was the census's blind spot wearing the
costume of a finding.

Two instruments replace it, and between them no name is matched anywhere:

* **`tools/alias_group_ablate.py`** — a report-only leg costs **2.5 s** (the
  alias map is a *report-time* input, so no compiles are involved). Remove one
  group, see which rows drop: attribution **by construction**, and an anonymous
  row attributes exactly as well as a named one.
* **`tools/alias_membership_adjudicate.py`** — `verdict(survivor, folded)` on
  retail bytes takes a MEMBERSHIP, not a call site, so it needs no census either.

`tools/alias_class_ablate.py` prices each evidence class in a handful of legs.

⚠ **Which instrument each number came from.** Every figure in this document is
from **class ablation + membership adjudication**, both of which are complete and
whole-tree. The **per-group** sweep is a finer-grained *confirmatory* artifact
(exact row→group attribution) and is slower; the two were cross-checked where
they overlap and agree. Do not read a per-group concentration figure quoted from
a partial sweep as final — GROUNDED-1's "only 448 of 1,493 groups forgive any
bytes" is measured here at **53.8% of the first 699 groups**, which is *not* the
same statistic and is not yet a refutation of it.

## The measured partition of the 819,108 B

⚠ **Two trees, two totals — do not mix them.** The headline re-measure above
(**818,416 B**) is worktree `wt-alias2` at `9dbea49f`; the partition below is
worktree `wt-alias2b` at `1a9f6648`, where the same mechanism measures
**819,108 B** (+692 B, from the map/pin work that landed between). Every row in
the table is from the second tree and sums to *its* total, exactly. This is the
`total_code`-is-not-a-constant rule applying to the alias mechanism too.

| evidence under the bytes | memberships | bytes | share |
|---|---:|---:|---:|
| **PROVEN** (L1_T1 / L2_RECURSIVE / L3_EXACT / L4_OURSIDE / L5_INCONSISTENCY) | 14,374 | **759,516** | **92.73%** |
| NEEDS_MAP_ID | 16 | 28,964 | 3.54% |
| NEEDS_SOURCE | 738 | 16,012 | 1.96% |
| CONTRADICTED | 156 | 14,616 | 1.78% |
| *of which DECISIVE (withdrawn)* | *80* | *10,916* | *1.33%* |

★ **The three non-proven classes sum EXACTLY to the "strip all non-PROVEN" leg**
(14,616 + 16,012 + 28,964 = 59,592 B measured) — they are disjoint in row
coverage. That additivity is the internal control on the partition.

Membership verdicts over all **15,284** installed memberships: PROVEN 94.05% ·
NEEDS_SOURCE 4.83% · CONTRADICTED 1.02% · NEEDS_MAP_ID 0.10%. **Reproduced
identically on two worktrees at different HEADs** (9dbea49f and 1a9f6648).

⚠ **`NEEDS_MAP_ID` is 16 memberships carrying 28,964 B — by far the densest
class per membership.** Sixteen map identifications are the cheapest remaining
alias work in the tree, and none of it needs new source.

⛔ **A prediction that failed, recorded because it was wrong in an informative
way.** This lane pre-registered "stripping NEEDS_SOURCE costs ≈ 0 B" — reasoning
that if our spelling is in no compiled obj, no call site can name it. Measured:
**−16,012 B**. The class fires when *either* side's body is absent, so it also
holds pairs where RETAIL's survivor body is missing while our spelling is
genuinely called. **The class name misleads; read the predicate, not the label.**

## The anti-vacuity check that licenses the headline

"0 of 1,894 unattributed rows depend on a non-proven membership" is exactly the
shape of a vacuous result — the two row sets were measured in different
worktrees, and a key-space mismatch would produce 0 **by construction**.

Checked before believing it: **1,886 of 1,894 keys (99.6%) are present in the
tree AND at `fuzzy == 100` in the FULL leg**, so the test had 1,886 chances to
find a hit and found none. The same leg dropped **246 other rows / 48,676 B**, so
it was not a leg that drops nothing.

## Withdrawals: 80 memberships, −10,916 B, predicted exactly

| | predicted | measured |
|---|---:|---:|
| `Δmatched_code` | −10,916 B | **−10,916 B** |
| `Δmatched_code_percent` | −0.105766 pp | **−0.105766 pp** |
| `Δmatched_functions` | 0 | **+0** |
| `none` ruler | flat | **+0 B / +0.000000** |

Units at 100%: 251 → 251 (mpn), 129 → 129 (fuzzy); **0 fell off either ruler.**

```
VALIDATE: PASS -- 1321 map-consistent, 203 tolerated (enumerated above), 0 contradicted, 1528 total
```

The gate was shown able to FAIL first: a fabricated alias between two symbols the
map places at distinct addresses (`?ForceSym@DataNode@@` / `?Str@DataNode@@`)
yields `CONTRADICTED (FATAL) 1` / `VALIDATE: FAIL`.

⚠ The FLAT `none` control does **not** clear this change and is not cited as if
it did — `none` ignores relocation names, so it reads +0 by construction. The
licence is the retail-byte evidence.

### What was withdrawn

* **60 `CALLEE_SIZE_MISMATCH`** — parent bodies are twins, but the callee retail
  names and the callee we name have different-sized COMDATs *in our build*.
  `??0NgMat@@QAA@XZ` calls `??0RndMat@@IAA@XZ` (716 B) while its aliased spelling
  `??0AnimPtr@@QAA@ABV0@@Z` calls `??0?$ObjPtr@VRndPropAnim@@@@QAA@ABV0@@Z`
  (136 B).
* **20 `SURVIVOR_SIZE_MISMATCH`** — our own compiler gives the two spellings
  different-sized COMDATs. Our `??2CriticalSection@@SAPAXI@Z` is 8 B (byte-equal
  to retail) while `??2BandRetargetVignette@@`, `??2LayerDir@@`,
  `??2OverdriveMeter@@` and `??2UnisonIcon@@` are 60 B — the per-class
  `NEW_OBJ`/`MEM_OVERLOAD` macro misassignment NEWOBJ-1 named. **The alias was
  forgiving it.**

**Withdrawal is per-MEMBERSHIP, never per-group.** `??2CriticalSection` keeps its
other 121 spellings and ~73 kB of separately-proven credit. Groups are kept with a
`withdrawn` record and never pruned (`a745039e`: a prune cost +94,616 B to
reverse).

### What was deliberately NOT withdrawn — 76 of the 156 contradictions

`verdict()` returns `CONTRADICTED` as its **FALLBACK**: "no layer proved a fold
and some word differs", which is **not** "no fold reading exists". Withdrawing all
156 would have repeated the error GROUNDED-1 documented (three `hash_map` rows
that flat T1 called REFUTED are proven folds by internal inconsistency).

Kept: **65 UNRESOLVED**, **10 CALLEE_SAME_SIZE** (the callees may themselves
fold), **1 BUILD_DIVERGENCE**.

## ⛔ The size test must stay inside ONE build — and GROUNDED-1's did not

Comparing RETAIL's body size to OURS conflates two different things:

* **(a)** these two COMDATs really are different → the fold is refuted;
* **(b)** OUR build emits this family at the wrong size → refutes nothing about
  retail's link, and is a source defect.

Our STLport emits `__uninitialized_copy` and `_M_allocate_and_copy` **uniformly
+8 B vs retail** across ~95 same-name pairs (**52 of 57** and **43 of 47** of the
size-differing rows). A retail-vs-ours test fires on that entire family for the
wrong reason.

⇒ The comparison is now **our(S) vs our(F)**. If our own build gives the two
spellings different-sized COMDATs they cannot be one COMDAT in any build. If
`our(S) == our(F)` while retail differs, the pair is INTERNALLY CONSISTENT and the
gap is ours — `BUILD_DIVERGENCE`, explicitly **not** withdrawn.

**This corrected a real case rather than a hypothetical**: under the cross-build
test `??$__uninitialized_copy@PAULocalizedName@HamMove@@…` read as a decisive
withdrawal; within our build `our(S) = our(F) = 104 B`, `retail(S) = 96 B`.

### 7 of GROUNDED-1's 8 withdrawal records are corrected (reason only, Δ0)

Their stated reason — *"retail's instantiation is 96/100 B against our const
104/108 B ⇒ two COMDATs of different size cannot fold ⇒ the alias was forgiving
our use of the wrong overload"* — compares two builds. Measured within ours,
`our(survivor) == our(folded)` in **all 7** (104/104, 108/108).

⇒ The recorded diagnosis is false, and it would send the next lane to change a
**const-ness that is not the defect**. The real defect is **one shared +8 B
divergence in the STLport copy helpers**.

★ **They stay WITHDRAWN.** Refuting a refutation yields **UNPROVEN, not PROVEN**,
and re-adding an alias lifts `matched_code` **by construction** — the hazard
direction. Restoring them would need positive fold evidence, which this lane did
not produce and did not look for.

**The 8th is CONFIRMED and untouched**: `Keys<Quat>::Remove` — our `KeyLessEq` is
176 B and `KeyGreaterEq` 192 B, different bodies, so those callees cannot fold.
GROUNDED-1 was right there.

## The `ALL_FOLD` ruling STANDS — now measured, not precautionary

RULERGAP-1's 247,376 B `ALL_FOLD` stratum was left unaliased on the coordinator's
ruling that it is **classified, not proven**. Adjudicated 120 of the 2,198
distinct `(target, base)` pairs inside its 5,975 rows (60 most-charged + 60 random
tail, so the answer is not merely the head's):

| verdict | pairs | share | charged sites |
|---|---:|---:|---:|
| PROVEN | 72 | 60.0% | 2,320 |
| **CONTRADICTED** | **40** | **33.3%** | 914 |
| NEEDS_MAP_ID | 4 | 3.3% | 98 |
| NEEDS_SOURCE | 4 | 3.3% | 23 |

**Why the label was never proof:** the classifier's own vocabulary (`FOLD` vs
`GENUINE: different size` vs `GENUINE: same size, different code`) shows it decides
on SIZE plus CODE — exactly what the template-twin vacuity defeats.
`vector<Foo>::erase` and `vector<Bar>::erase` have identical machine bytes and
differ ONLY in the destructor they call.

The contradictions admit no fold reading — e.g.
`??1?$ObjRefConcrete@VFlowLabel@@VObjectDir@@@@UAA@XZ` (116 B) vs
`??1?$ObjPtr@VEventTrigger@@@@UAA@XZ` (**4 B**), 151 sites.

⇒ Installing `ALL_FOLD` would have bought ~247 kB by forgiving a stratum a third
of which is a real wrong-callee defect. **Nothing was installed** — including the
proven 60%, which is a separate decision.

## ⛔ The "46 REFUTED + 37 UNDECIDABLE" brief is a STALE SLICE — do not re-brief it

No artifact in the tree contains those counts. The phrase appears in exactly one
place: the **lane WRONGCALL-2 merge commit `abdbfd6b`**, which was briefed the
same numbers and reported it **could not reproduce them** — the tree then read
33 REFUTED / 26 UNDECIDABLE, because WRONGCALL-1's SessionMgr repair and
ALIASVAL-1 landed in between. They were never a count of open alias pairs; they
were a **top-100 slice** of the WRONGCALL relocation-name census, obsolete two
lanes earlier.

**Brief the full census instead** — WRONGCALL-2 replaced the slice with all 2,294
real-name charged pairs (**PROVEN 487 / REFUTED 1,021 / UNDECIDABLE 786**), at a
cost of ~50 s. This is a repeat of the `feedback_read_the_in_tree_record_first`
failure, and the third time in this corpus that a slice has been re-briefed as a
worklist after being retired.

⚠ Two counts in the brief were also miscounted: `nogroup-wrong-callee-queue`'s
100 UNDECIDABLE are **pair-for-pair identical** to `foldprove2`'s 100 (100/100
overlap) and already adjudicated; and `template-args-queue`'s "722/183/65" mixes
two different columns and counts victim ROWS, not pairs (deduplicated: 142/190/59).

## ⛔ Flat T1 `PROVEN` does not test RESIDENCY — 5 fabricated aliases avoided

`icf_pair_adjudicate.py`'s flat T1 asks only "do retail's bytes at S equal our
compiled body for F". It **never checks whether retail kept F at its own
address** — which is the criterion several pairs were refuted on. Of the top-40
open pairs, 5 read `FLAT T1: PROVEN` while `target_symbol_map.json` places the two
at **distinct addresses**, e.g. `?getRandomSequence32B@@` @ `0x827255f0` vs
`?getRandomSequence32A@@` @ `0x827256c8`. Retail keeping two addresses **is** the
definition of not folded.

⇒ **Installing off a bare `FLAT T1: PROVEN` would have fabricated 5 aliases from
the top 40 alone.** The tool prints the tell (`retail_bodytwins`) but not as a
verdict.

## Leads handed on, not taken

* ★ **The +8 STLport family divergence** (`__uninitialized_copy` 96→104,
  `_M_allocate_and_copy` 100→108, ~95 pairs). One shared source defect; fixing it
  would plausibly restore the folds it caused to be withdrawn. **Not read at the
  instruction level here — this is a size census only.**
* ★ **16 `NEEDS_MAP_ID` memberships carrying 28,964 B rest on just NINE distinct
  unidentified addresses** — the densest remaining alias vein by a wide margin,
  and it needs **map identifications, not source**:

  | address | memberships | what it gates |
  |---|---:|---|
  | **`fn_827BCD38`** | 2 | `?MemOrPoolAlloc@@YAPAXHPBDH0@Z` ← the 1-arg forms |
  | `fn_82BF72E0` · `fn_82BD4C48` · `fn_82BC4B28` | 2 each | XAUDIO2 / LEAPFX vector-dtor thunks vs `ObjRefConcrete<RndCubeTex\|RndFur>` |
  | `fn_82BD0EB8` · `fn_82BD2E78` · `fn_82BD4EF0` | 2 each | same family |
  | `fn_82327050` · `fn_824C8F68` | 1 each | `operator<<(BinStream&, list<T>)` twins |

  ⚠ **`0x827bcd38` is the SAME address GROUNDED-1 named as gating its largest
  single block** — `??2CriticalSection@@SAPAXI@Z ← ??2@YAPAXI@Z` (73,496 B) is
  proven only via L4 because retail's branch destination `0x827bcd38` is unnamed
  and there is no `?MemAlloc@@YAPAXHH@Z` row in the map at all. **One
  identification upgrades that block to destination-verified L3 AND clears 2 of
  these 16 memberships.** It is the highest evidence-per-unit-of-work item in the
  alias corpus.
* **The 4 allocator-thunk classes** (`BandRetargetVignette`, `LayerDir`,
  `OverdriveMeter`, `UnisonIcon`) at 60 B vs our 8 B survivor — NEWOBJ-1's
  per-class macro misassignment, now no longer forgiven.

## What this lane did NOT do

* **Installed no aliases**, including the 60% of `ALL_FOLD` that adjudicates
  PROVEN and the 5 flat-T1 "PROVEN" pairs above.
* **Did not restore** the 7 corrected GROUNDED-1 withdrawals.
* **Did not adjudicate the TEMPLATE-1 population** (142 UNDECIDABLE + 190
  unresolved distinct pairs, ~98 kB) — its columns are demangled and need a
  demangled→mangled join that was not built. **This is the largest genuinely-open
  alias queue and it is untouched.**
* **Did not read the instruction-level diff** for any withdrawal; every
  withdrawal rests on a COMDAT-size argument, which is mechanical.
* **Did not price the `129,360 B` irreducible relocation-free thunk floor** — per
  GROUNDED-1 it is irreducible, and this lane accepted that rather than re-testing.

## Reproducing

```bash
python3 tools/alias_forgiveness_audit.py measure --wt <wt> --fell ~/tmp/fell.json
python3 tools/alias_membership_adjudicate.py --wt <wt> --out ~/tmp/mem.json
python3 tools/alias_contradiction_refine.py  --wt <wt> --memberships ~/tmp/mem.json --out ~/tmp/wd.json
python3 tools/alias_class_ablate.py --wt <wt> --memberships ~/tmp/mem.json --withdraw ~/tmp/wd.json
python3 tools/alias_group_ablate.py --wt <wt> --out ~/tmp/ablate.jsonl   # ~85 min, exact per-group
```

⛔ **Run these against a tree that is BUILT and not being mutated.** A fresh
worktree's reflinked target objs are pre-renamer, so every retail mangled name
reads "absent" and every verdict reads `NEEDS_SOURCE` — a decisive-looking
negative that is pure instrument failure. `alias_membership_adjudicate.py`
refuses below 1,000 mangled target names for exactly this reason. Main was
observed **rebuilding mid-read** during this lane (`??0RndMat@@IAA@XZ` measured
648 B then 716 B an hour apart), which is why every number here was re-verified
on a private worktree.
