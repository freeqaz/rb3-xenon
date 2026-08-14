# Fold groups that EXIST but are INCOMPLETE — censused tree-wide

**Lane INCOMPLETE-1, 2026-08-14, on `5a2ce6ba`.** Follows ONMSG-1 (`34b0a56a`),
which found that two groups *already present* in `scripts/symbol_aliases.json`
were **still charged**, because "a pairwise byte comparator finds only SOME
members while a dispatcher enumerates the class EXHAUSTIVELY" — completing them
paid **+11,424 B**. CVEIN-1 then spotted 30 more pairs in that state and
correctly declined to act. Neither lane censused the population. This one does.

## The census — the deliverable

Charged relocation-name pairs, tree-wide, with **both** forgiveness rules
applied (a hand-rolled reloc diff applies neither, which is the overcount trap in
`template-args-C-class-is-folds-2026-08-14.md`):

| step | pairs |
|---|---:|
| raw charged pairs (21,622 aligned function pairs) | 27,211 |
| − placeholder targets forgiven by `name_check` | −23,820 (**87.5%**) |
| − already equivalent inside one installed group | −1,233 |
| **real charged pairs** | **2,158** |

⚠ **The placeholder rule alone removes seven of every eight pairs.** Any census
of this stratum that skips it is wrong by a factor of ~8, in the exciting
direction.

Split against the 1,519 installed groups — **the incomplete-group population is
528 pairs / 1,592 sites**, and the rest is not this lane's:

| class | pairs | sites | closable rows | closable B |
|---|---:|---:|---:|---:|
| **MEMBERSHIP** (retail name is a group SURVIVOR, ours unowned) | **373** | 1,050 | 537 | **158,152** |
| MEMBERSHIP (retail name is a *folded* member) | 1 | 1 | 0 | 0 |
| **MERGE** (both names owned, by DIFFERENT groups) | **154** | 541 | 247 | **57,632** |
| — *incomplete-group subtotal* | **528** | 1,592 | **802** | **220,792** |
| OURS_SIDE (only our spelling owned; retail name needs identification) | 237 | 327 | 235 | 31,052 |
| FRESH (neither name in any group — a different vein) | 1,393 | 2,123 | 1,314 | 302,348 |

"Closable" is priced honestly: `matched_code` is **all-or-nothing per row**, so a
row counts only when `mpn == 100` (no instruction-level mismatch an alias could
never touch) **and every charge on it** is in the class being priced. The
census's gross "bytes on sub-100 rows" is ~30% larger and must not be quoted.

**RECALL BOUND:** the alignment gate is deliberately conservative, so a pair
whose enclosing functions never align is invisible. 528 is a **lower bound**.

## What was harvested: 40 memberships, +60,700 B measured

373 MEMBERSHIP candidates → existing gates (`relocs_agree` flat T1,
`icf_pair_adjudicate.chase`, `family` pigeonhole) → **205 PROVEN** → a
**retail-UNIQUENESS gate** → **40 installed**.

★ **The novelty of this lane is the ENUMERATOR, not the comparator.** ONMSG-1's
finding was that a pairwise comparator does not *enumerate* a fold class
exhaustively; it *verifies* one fine. So candidates come from a structure the
comparator never looked at — objdiff's own charged sites — and each is then put
through the already-validated gate, so a verdict here is the verdict the shipped
generator would reach.

### ⛔ The gate that removed 165 of 205 — retail uniqueness

**A fold class is usable as an alias only if retail kept ONE address for it.** If
the target objs hold the body at N>1 addresses, our call site's true target is one
of several and the alias may forgive a genuinely **wrong** callee.

`icf_pair_adjudicate`'s docstring already names this concern ("a body shared by
many functions proves nothing about which one a call site meant — picking one of
an ICF-folded group is a coin flip") but **reports** it rather than **enforcing**
it in the batch path. Enforced here, it rejects **80.5% of otherwise-PROVEN
pairs**:

| distinct retail addresses for the body | pairs |
|---:|---:|
| 1 — usable | **40** |
| 2 | 77 |
| 3 / 5 | 2 |
| 8 | 78 |
| 240 (`__destroy_aux` family) | 8 |

⚠ The gate is **exact** for the `nrel == 0` subset (masked body == raw body) and
deliberately **over-strict** for `nrel > 0` (masked-equal twins that would not
actually fold are counted as ambiguity and cost us the pair). Erring strict is the
intended direction: an unproven alias lifts `name_check` **by construction**, so a
false membership is not a miss, it is a fabricated gain.

### The keepers

40 pairs / 14 groups; 36 flat T1, 4 chase; 28 have `nrel == 0`.

* **`??BDataArrayPtr::operator DataArray*` ← `DataNode::Var` / `Command` /
  `LiteralInt` / `LiteralArray`** — 8 B, `lwz r3,K(r3); blr`, zero relocations.
* **`ArkFile::Size` ← `BandUser::GetTrackType` / `Profile::GetPadNum` /
  `NetCacheLoader::GetRemotePath`** — same shape.
* **`hashtable<Symbol,…>::_M_find<Symbol>` ← seven sibling instantiations**
  (`int`, `float`, `Award*`, `Accomplishment*`, `AccomplishmentGroup*`,
  `vector<int>`, `Symbol`) — **128 B with ZERO relocations**, the strongest single
  witness in the set: `_M_find` keys on `Symbol`, so the mapped-value type changes
  nothing in the code.

★ **`nrel == 0` is a STRENGTH here, not a weakness** (FATAL-1's scoping note):
with no relocation, nothing is masked, so byte identity is `/OPT:ICF`'s **complete**
criterion. The honest limit is GROUNDED-1's: the fold is proven, but *which* name
the call site meant was destroyed by ICF itself. The objdiff-level claim (our `bl`
reaches the right address) holds; the source-level one is not recoverable.

### The gate demonstrably can fail

The **4th-largest naive prize**, `SyncProperty@Tour ← SyncProperty@Object@Hmx`
(10,688 B), was **REFUTED on size — 72 vs 544 B**. It was flagged suspicious a
priori (the `SYNC_PROP` vein is a documented refutation) and the size gate killed
it without special-casing. Also refuted: 38 on size, 69 on map-residency, 31 on
relocations, 8 on bytes.

**FP control, drawn from the population being judged** (ALIASAUDIT-2's warning
that a prior calibration was scored against pairs that fold by construction, so it
could not fail): 2,488 decoys built by **re-pairing the same charged sites**, so
the decoy population shares the treatment's size/shape distribution.

```
decoy  PROVEN 0 / 2488 = 0.00%      treatment PROVEN 205 / 373 = 54.96%
```

⚠ Honest caveat: 71.95% of decoys die on size, so the control is easier than the
treatment on that axis. It is a real opportunity to fail — the gate has no
size-blind path — but it is not size-matched.

### Measured

Predicted **+65,772 B / 173 rows / Δmatched 0**, pre-registered before the run.

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.588142pp  Δcode_bytes=+60700
VALIDATE: PASS -- 1313 map-consistent, 202 tolerated, 0 contradicted, 1519 total
```

92.3% of prediction; the shortfall is the expected gap between this COFF-derived
charge set and objdiff's (the alignment gate). `ALIAS_SUSPECT` fired — **expected
and documented** for a map-only patch (`none` flat, `name_check` up); it is
answered per group in the `evidence` field with the retail-byte witness, never by
the `none` control, which cannot clear an alias.

## All 154 MERGE candidates DECLINED — 60 of them positively REFUTED

A membership says "one more spelling landed on an address the group already
claims". A merge says "**these TWO DISTINCT RETAIL ADDRESSES are one body**" — a
strictly stronger claim the membership comparator **cannot** test, since flat T1
compares our body against retail at `addr(S1)` and never looks at `addr(S2)`
(lane T1-AUDIT: "the T1 warrant NEVER adjudicates `addr(F)`").

The decisive test is **retail-vs-retail at the two addresses**, and it is a fork
with no favourable branch:

* bodies **differ** ⇒ different code, no fold; the charge is a real defect or a
  map error, and an alias would forgive it;
* bodies **same** ⇒ two *live* addresses hold one body, i.e. `/OPT:ICF` did **not**
  fold them — which refutes the fold too, unless `addr(S2)` is not a real function
  start (the FOLD-THUNK scenario), and that is a **map-row repair**, not a
  unilateral alias merge.

| verdict | pairs | sites |
|---|---:|---:|
| NOT_A_MERGE — our spelling is map-absent (a cross-group membership) | 94 | 194 |
| **REFUTED — the two addresses hold different code** | **60** | **347** |
| ARGUABLE (map parked a fold's loser on debris) | **0** | 0 |

⛔ **A comparator correct for one question was wrong for this one, and it changed
the reported reason.** The first run reused `icf_alias_build.relocs_agree`, which
**tolerates** a retail-side `fn_`/`lbl_` placeholder — correct for retail-vs-**ours**
(dtk spells a callee `fn_<B>` only when B is absent from the map, so the name
carries nothing our side could contradict) and **wrong for retail-vs-retail**,
where two different placeholder names in one slot mean dtk resolved that slot to
two **different symbols**. It reported 41 pairs as "identical bodies at two live
addresses". Compared literally, they are simply different functions:

> `ObjRefConcrete<FlowLabel>::~ObjRefConcrete` @`0x822b0e60` and
> `ObjRefConcrete<EventTrigger>::~ObjRefConcrete` @`0x822cd918` are
> masked-byte-identical (116 B) and **share their callee** `fn_8275B378`, but
> differ at slots 24/36 — `lbl_8201BA34` vs `lbl_8202158C`, **each type's own
> `.rdata`**. ICF correctly declined to fold them because their *relocations*
> resolve differently — CD-7's criterion exactly.

The verdict (refuse) is unchanged either way, so the refusal was robust; only the
*reason* moved — from "a fold ICF missed" to "not a fold at all", which is the
stronger statement. (Name **inequality** is safe evidence even though CLAUDE.md
warns `lbl_` names lie about their address: only "dtk resolved these to distinct
symbols" is used, never the encoded address.)

## Reusable findings

1. **A declared fold group is not necessarily a complete one, and the gap is
   sized: 528 pairs / 1,592 sites / ≤220,792 B**, of which this lane could prove
   40 pairs / +60,700 B. The rest is not cheap — 80% of it dies on retail
   uniqueness.
2. **Enumerate from a structure that lists the class; verify with the pairwise
   comparator.** The two are different jobs and the tree already had a good
   verifier and no enumerator.
3. **A tool's tolerance is scoped to the question it was written for.** `relocs_agree`'s
   placeholder tolerance is load-bearing and correct in its own direction and
   silently wrong when the comparison is turned around. Same disease as the
   `OK (grounded)` label: right check, wrong question.
4. **Enforce uniqueness, don't just report it.** It is the difference between an
   alias that forgives a fold and one that forgives a wrong callee, and it
   rejected 80.5% of a set every other gate had passed.
