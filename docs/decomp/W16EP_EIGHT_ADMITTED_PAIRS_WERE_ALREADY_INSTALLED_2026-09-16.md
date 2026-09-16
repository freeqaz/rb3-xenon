# W16-EP — the 8 pairs W16-EK's fix unblocked: 7 were already installed, 1 is not

> **⛔ CORRECTED 2026-09-16 (same lane, after coordinator review). §3 of the
> first version was WRONG and it changed two verdicts.** My whole-image census
> called `fold_thunk_gate.mask_word(w)` with **one argument**, taking the
> `relocated=True` default, so it masked the low 16 bits of every `IMM16_OPS`
> opcode. `li r3,N` is `addi` (opcode 14), so the **per-`T` node size** was
> masked away. The claimed *"eleven byte-identical 84-byte `list<T*>::erase`
> bodies retail did not fold"* **does not exist** — see §3, rewritten. The
> lane's install result, the stale `sites` column (§6) and the
> `ContextChecker.cpp:262` stub (§7) are **unaffected**.

Lane W16-EP, 2026-09-16, worktree `~/tmp/wt-w16-ep`, branch `w16-ep`, off `2d366ad2`.
Ruler: shipped graded `name_check` (read from `build/45410914/report.json`, not assumed).

## Result in one line

**Nothing installed. Measured delta 0, and no A/B was run because the tree is
byte-identical to HEAD** — every pair that clears the evidence bar was
**already in `scripts/symbol_aliases.json`**. *(Corrected: the bar-clearing set
is 7 of 8, not 2 of 8 — see §3. Exactly one pair, #1, is uninstalled, and it is
admissible rather than refused.)* The lane's real finding is that
**W16-EK's "8 blocked pairs" were overwhelmingly memberships the file already
shipped**: the defect was blocking the gate's ability to *re-derive* its own
aliases, not blocking new value.

## 0. Pre-registration (written before any gate was run)

| # | prediction | measured | |
|---|---|---|---|
| P1 | flip count = **8**, 0 ADMIT->REFUSE | 8 flips, 0 ADMIT->REFUSE, 8/8 carrying the asymmetry as before-reason | ✅ |
| P2 | tier split **7 FT2 + 1 FT-EMPTY** | 7 FT2 + 1 FT-EMPTY | ✅ |
| P3 | **I install 0 of 8** (allowing <=1) | installed 0 — but for a reason I did not predict | ⚠ **right answer, wrong reason** |
| P4 | **0 of 8 have differing sizes**; `compare()` refuses on `body length N vs M` before admission | 0 of 8; word counts equal by construction | ✅ |

**P3 is recorded as a near-miss, not a hit.** I predicted 0 installs because I
expected the 8 to be template twins refuted on their relocation targets (the
W16-EJ shape). That reasoning was **wrong**: the relocation targets are
NAME-EQUAL on both sides in every pair that has one, and two pairs (2 and 4)
do clear the bar on retail bytes. I would have installed them. They were
already installed, which is why the count is 0. Predicting the right number
from the wrong model is a miss.

## 1. Re-derivation — the count reproduces on this tree, not on EK's word

Method, deliberately EK's so the numbers are comparable: relabel **all 1,048**
pairs of `docs/plans/wrong-callee-triage-2026-08-12.json` to
`fold_thunk_naming`, run the pre-EK gate (`git show 97f76e42^:tools/fold_thunk_gate.py`)
and the current gate over the same worklist, same build, and diff the verdicts.

| | ADMIT | REFUSE |
|---|---|---|
| pre-EK gate | 7 / 1,507 sites | 1,041 |
| current gate | **15** / 1,523 sites | 1,033 |

⇒ **8 flips, all REFUSE->ADMIT, 0 ADMIT->REFUSE, and 8 of 8 carry
`relocated fields at different offsets` as their before-reason.** EK's figure
reproduces exactly.

**Build first, then trust a negative.** The reflinked worktree was built to a
fixed point before any of this (`verify_objs_patched.py --check`: *"tree is a
fixed point of 6 post-compile passes"*), and the EK trap was checked explicitly:
680,543 COMDATs indexed, with `?SetActive@Shuttle@@QAAX_N@Z` and
`?Enable@Metronome@@QAAX_N@Z` both present — the exact symbols that were
silently missing from EK's stale tree.

## 2. The 8 pairs

`our_bodytwins` / `retail_bodytwins` and the family pigeonhole come from
`tools/icf_pair_adjudicate.py --pairs ... --family`, whose `--selftest` was run
first and **discriminates** (positive control PROVEN, negative REFUTED).
`retail copies` is the **CORRECTED** whole-image true-copy census
(`tools/icf_true_copy_census.py`, §3) — the column in the first version of this
table came from a form-masked comparator and read 2 / 2 / 11 / 11 on pairs
1 / 3 / 5 / 7. Those four were inflated; the true count is 1 for all eight.

| # | sites | survivor <- folded | T1 | our_bodytwins | retail copies (incl. branch targets) | verdict |
|---|---:|---|---|---:|---:|---|
| 1 | 8 | `??2CriticalSection` <- `??2Task` | UNDECIDABLE (vacuous) | 214 | 1 | **not installed** — fold proven, see §3.2 |
| 2 | 2 | `list<AccomplishmentCondition>::insert` <- `list<Plane>::insert` | PROVEN | 137 | 1 | clears bar — **already installed** |
| 3 | 1 | `_Vector_base<RndTransformable*>::ctor` <- `GetSongSpecificEntriesForCategory` | PROVEN | **1031** | 1 | **REFUSE** + source defect |
| 4 | 1 | `_Param_Construct<EyeDesc>` <- `_Copy_Construct<EyeDesc>` | PROVEN | 271 | 1 | clears bar — **already installed** |
| 5 | 1 | `list<Voice*>::erase` <- `list<const char*>::erase` | PROVEN | 41 | 1 | clears bar — **already installed** (was: REFUSE) |
| 6 | 1 | `list<CharClip*>::insert` <- `list<Dep*>::insert` | UNDECIDABLE | 137 | 1 | fold proven on our bytes — **already installed** |
| 7 | 1 | `list<MsgSource::Sink>::erase` <- `list<pair<Symbol,Symbol>>::erase` | PROVEN | 11 | 1 | clears bar — **already installed** (was: REFUSE) |
| 8 | 1 | `pair<Symbol,SongRecord>::dtor` <- `pair<const Symbol,SongRecord>::dtor` | UNDECIDABLE (vacuous) | 75 | 1 | fold proven on our bytes — **already installed** |

**No pair has differing sizes on the two compared sides** — structurally
impossible among ADMITs, since `compare()` refuses on
`body length N words vs M` before any admission. The brief's different-size
defect class therefore cannot appear here; it would have to be hunted in the
*refusals*, which is a different lane.

### 2.1 FT2's discredit is sound, and here is the retail-byte reason

All 8 share one shape: the map parks the folded spelling on an 8-byte,
zero-reference body. That is not a coincidence and not a gate artifact —
**7 of 8 of those addresses are 8-byte EH prefixes**, and `addr+8` is a real
function start (`7d8802a6` = `mflr r12`) with its own map name and extent:
`?Poll@MoviePanel@@` (660 B), `??0SongSortBySong@@` (120 B),
`?RefreshSetlists@MusicLibraryNetSetlists@@` (132 B),
`??1MatSwap@OutfitConfig@@` (120 B). The 8th is a bare `blr` + padding. So the
map is parking fold losers on the EH prefix of an unrelated function, exactly
as the gate's docstring describes. **That clears a contradiction; it proves no
fold.**

## 3. ⛔ RETRACTED AND REPLACED — the census that "refuted" pairs 5 and 7 was my own defect

### 3.1 What the first version claimed, and why it is false

It claimed retail keeps **eleven byte-identical 84-byte `list<T*>::erase`
bodies** — identical *including* branch targets — unfolded at eleven distinct
addresses, and used that to REFUSE pairs 5 and 7.

**The eleven are not copies. They are eleven different functions that share a
shape.** Verified on raw retail bytes, no masked comparator involved:

```
822b1e60 vs 82447458, 84 B:  [ 7] 38600054 != 38600018    li r3,84  vs  li r3,24
                             [12] 4850abc1 != 483755c9    bl <different callee>
```

Word 7 is the **per-`T` node size**. A non-branch immediate differs, so
`/OPT:ICF` can never fold these *regardless* of `bl` targets.

**The mechanism was mine.** My census called
`fold_thunk_gate.mask_word(w)` with **one argument**, so it took the
`relocated=True` default and masked by instruction form unconditionally.
`li r3,N` is `addi` — opcode 14, in `IMM16_OPS` — so:

```
mask_word(38600054) = 38600000     mask_word(38600018) = 38600000   -> COLLAPSED
mask_word(4850abc1) = 48000001     mask_word(483755c9) = 48000001   -> COLLAPSED
```

⚠ **I reproduced, in my own instrument, the exact defect described in §1.2 of
the W16-EK document I had read before writing it** (`stb r4,8(r3)` /
`stb r4,0xc(r3)` / `stb r4,0x7ff(r3)` all masking to `0x98830000`). Form-masking
is correct for the gate's *linked-vs-linked homonym* path, where both sides are
relocated; it is wrong for a whole-image census, where a non-relocated literal
is **real content**, not a link-patched field.

### 3.2 The corrected census, and what survives

Corrected rule: mask **only** the displacement of op 18 (`b`/`bl`) and op 16
(`bc`), compare every other word as a **full 32-bit value**, and additionally
require the **resolved branch destinations** to be equal. Implemented as
`tools/icf_true_copy_census.py` (the first version was a throwaway; this one is
committed so the claim can be re-run).

| | eleven 84-byte bodies | 8 survivor bodies |
|---|---|---|
| defective (form-masked) | 1 class | 29 / 1 / 2 / 1 / 11 / 1 / 11 / 411 copies |
| **corrected (true-copy)** | **8 distinct forms among the 8 sampled — zero survive** | **1 copy each, all eight** |

The node-size immediates of the sampled eleven read
**84, 16, 24, 92, 72, 88, 36, 20** — every one different. One of them
(`82766828`) is not an `erase` at all; it is `??0NetLoaderRef@@QAA@ABU0@@Z`, a
20-byte body my comparator had pooled with `list<T*>::erase`.

★ **The coordinator predicted zero survivors and zero is what it measures.**
W16-EN reached the same result independently
(`--family-recheck erase,list --size 84` ⇒ 11 extents, 11 distinct masked forms).

⇒ **Every survivor body is a UNIQUE copy in the image, which is exactly what a
COMPLETED fold looks like.** So the defect did not merely inflate a number — it
inflated it in the one direction that **manufactures evidence against folding**.
Phantom "unfolded copies" are the raw material of a spurious REFUSE, and that is
precisely what pairs 5 and 7 got.

### 3.3 The control, and proof it discriminates

A comparator that can only ever answer "1" would have "confirmed" this
correction as readily as it refutes the original claim, so the answer is
worthless until the instrument is shown to report a **large** count too.
`tools/icf_true_copy_census.py --selftest` requires **both** directions:

| check | measured |
|---|---|
| can it report MANY? largest true-copy class image-wide | **278** members (40-byte body) — W16-EN's figure, recovered independently |
| classes with ≥2 members | **2,026** of 68,378 |
| does it SPLIT shape classes? 84-byte stratum | shape-twin largest **66** → true-copy largest **2** |
| the eleven | **1** true copy each |

`SELFTEST PASS`. The negative control (`shape_key`) is the defective comparator
itself, kept in the tool so the split is measured rather than asserted.

### 3.4 Blast radius — which other verdicts the defect reaches

The defect is **one-directional**: masking can only merge classes, never split
them, so a count can only be *inflated*. Measured, not assumed — every count
fell or held, none rose. It inflated **4 of the 8** (pairs 1, 3, 5, 7) and was
already correct on pairs 2, 4, 6, 8.

- **Pairs 2 and 4 are SAFE**, as pre-registered: their verdicts rest on a retail
  count of exactly 1, which a merging defect cannot have produced.
- **Pairs 5 and 7 FLIP.** Their sole refusal reason was the eleven. See §3.5.
- **Pairs 1 and 3 do NOT flip**, because the census was never their only
  reason — pair 1's T1 is vacuous (an 8-byte body is below
  `MIN_WORDS`/`MIN_UNMASKED_FRAC`), and pair 3 carries 1,031 of our own body
  twins plus the source defect of §7. Their *retail* column is corrected 2 → 1.
- **The `our_bodytwins` column is NOT affected.** `collect()` masks through
  `icf_fold_evidence.masked_body(raw, relocs)`, which zeroes only the four bytes
  **at each COFF relocation record offset**. A non-relocated `li r3,84` is never
  touched, so that path never had the defect. This is the one figure in the
  original table that needed no correction, and it is worth stating why: it is
  **relocation-record-driven**, not form-inferred.

### 3.5 Re-adjudicating pairs 5 and 7 — both CLEAR THE BAR

With the eleven withdrawn, the question returns to the standard: *why this
spelling and not a body twin?* Answered on our own compiled COMDATs, using the
linker's actual algorithm.

⚠ **A flat one-iteration class is the wrong instrument and would have produced a
false refutation.** Comparing masked body + relocation target **names** in a
single pass puts pair 2's and pair 6's `F` **outside** `S`'s class — their only
difference is the callee `_M_create_node<T>`. But ICF is **iterative**: the
linker folds `_M_create_node<AccomplishmentCondition>` with
`_M_create_node<Plane>` first (they land in one class of 5), after which the two
`insert` bodies are identical. Run through `icf_fold_evidence.icf_classes`, the
iterative closure, **`F` is in `S`'s class for all 8 pairs**:

| # | survivor | iterative ICF class | F in class |
|---|---|---:|---|
| 1 | `??2CriticalSection` | 124 | YES |
| 2 | `list<AccomplishmentCondition>::insert` | 5 | YES |
| 3 | `_Vector_base<RndTransformable*>` | 1031 | YES |
| 4 | `_Param_Construct<EyeDesc>` | **2** | YES |
| 5 | `list<Voice*>::erase` | 41 | YES |
| 6 | `list<CharClip*>::insert` | 53 | YES |
| 7 | `list<MsgSource::Sink>::erase` | 11 | YES |
| 8 | `pair<Symbol,SongRecord>::dtor` | 4 | YES |

**Pair 5** (`list<Voice*>::erase` ← `list<const char*>::erase`) and **pair 7**
(`list<MsgSource::Sink>::erase` ← `list<pair<Symbol,Symbol>>::erase`): both
element types are the same width on each side (4-byte pointers; 8-byte
two-`Symbol`/two-word structs), so the node-size immediate **agrees** — the very
word that refutes the eleven *confirms* these. Sizes are equal on both sides,
every non-relocated word is identical, and the members of each class are fully
ICF-equivalent **including relocation target names**. Retail keeps exactly one
copy of each survivor body, the signature of a completed fold.

⇒ **Both clear the bar, and both were already installed** — so the lane's
install result (§5) is unchanged; what changes is that two memberships I called
refuted are in fact justified. The class sizes (41, 11) are the *irreducible*
ambiguity CLAUDE.md names: the fold is real and total, and which spelling any
given call site meant was destroyed by ICF itself. That is a reason the
forgiveness is **correct**, not a reason to withhold it — the hazard the bar
exists to catch is an alias between bodies that did **not** fold.

★ Pair 1 (`??2Task` → `??2CriticalSection`, 8 sites) is the one member of the
eight **not** installed. On the corrected evidence it is admissible: 8 bytes,
one relocation, and both spellings relocate to the **same name**
`?MemAlloc@@YAPAXHH@Z` — not `_MemAllocTemp`, so MAPID-1's different-allocator
trap does not apply. **I did not install it** — see §8.

## 4. Why pairs 2 and 4 clear the bar — a CALL-SITE argument, not a byte argument

"The bytes match" is not an argument; 1031 of our symbols share pair 3's body.
The bar is: why *this* spelling and not a body twin?

**Pair 2.** Retail's callers of the survivor are
`??0list<Plane>::list(const list<Plane>&)`, `?resize@list<Plane>@`, and
`?Update@BSPFace@@` — functions whose own mangled names are `list<Plane>`. A
`list<Plane>::resize` cannot legitimately call
`list<AccomplishmentCondition>::insert`; the only explanation is that the two
inserts folded and the map kept one arbitrary name. Corroborating: retail's
survivor relocates to `_M_create_node<list<Plane>>` — the **Plane** helper,
NAME-EQUAL with ours — retail defines no separate `F`, and the whole-image
census finds exactly **one** body with that branch-target set.

**Pair 4.** Both spellings are STL helpers for the **same T**. Retail's callers
are `__uninitialized_copy<EyeDesc>`, `__uninitialized_fill_n<EyeDesc>` and
`vector<EyeDesc>::_M_insert_overflow_aux`; ours is that same last function, and
the single relocation names `??0EyeDesc@CharEyes@@QAA@ABU01@@Z` on **both**
sides. A body twin for another `T` would call that other `T`'s constructor.

★ Both arguments survive the W16-EJ test that killed 20 of 23 `--chase` pairs:
the discriminator (the relocation target) **agrees by name**, rather than being
tolerated, masked, or recursed around.

## 5. The finding that makes the lane's output zero

`tools/fold_thunk_gate.py --install --tier FT2` on a 2-pair worklist:

```
ADMIT 2 pairs / 3 sites in 2 groups; REFUSE 0 pairs / 0 sites
--tier FT2: installing 2 of 2 admitted pair(s) / 3 of 3 sites
installed: 0 new group(s), 0 updated; 1658 total
```

`git diff --stat scripts/symbol_aliases.json` — **empty**. Checked against the
file directly:

| # | survivor group exists | folded spelling already present |
|---|---|---|
| 1 | yes | **no** |
| 2,3,4,5,7,8 | yes | **yes** |
| 6 | **no** | — |

⇒ **6 of 8 are already installed**, including both pairs that clear the bar.
The two that are NOT installed are pair 1 and pair 6 — the two the evidence
refuses. There is nothing left to install.

⇒ **W16-EK's "8 blocked pairs" measured the defect's reach correctly and its
VALUE not at all.** The unblocked admissions are, six times out of eight, the
gate finally agreeing with memberships already shipped. That is worth having —
*"a generator you cannot re-run is a number you cannot re-derive"* — but it is a
**reproducibility** result, not a byte result, and it should not be briefed as
uncollected headroom.

## 6. The worklist's charged sites are STALE — do not price from them

Priced the candidate rows with `objdiff-cli diff` (no `--build`; the tree was
already built and patched) and **none of the three rows is charged by the pair I
adjudicated**:

| row | size | fuzzy | what is actually charged |
|---|---:|---:|---|
| `?Update@BSPFace@@QAAXXZ` | 612 B | 97.79085 | 36 charges, **all** register/FPR/stack-slot regalloc + 1 `delete`. **Zero** fold-name charges |
| `_M_insert_overflow_aux<EyeDesc>` | 328 B | 99.87805 | 2 charges, both `__uninitialized_copy<PBU EyeDesc>` (const) vs ours `<PAU EyeDesc>` — a **different** pair |
| `?resize@list<Plane>@` | 144 B | 99.86111 | 1 charge, retail `__uninitialized_copy<TimeSigChange>` vs our `list<Plane>::erase` — a **different** pair |

The worklist is dated **2026-08-12**; these aliases were installed after it. So
its `sites` counts describe charges that no longer exist. **Any lane pricing a
candidate off that file's `sites` column is pricing a stale number** — price
from `report.json`'s charged-site list on the current tree, per the standing
rule.

## 7. A real source defect, reported rather than aliased away

Pair 3's folded spelling is **our own stub** (`src/band3/meta_band/ContextChecker.cpp:262`):

```cpp
std::vector<const char *> GetSongSpecificEntriesForCategory(Symbol, bool) {
    return std::vector<const char *>();
}
```

Unnamed parameters, returns an empty vector, compiles to 20 bytes — which is
why it is byte-identical to a `_Vector_base` ctor. Its caller
`GetSongSpecificEntries` uses the result (`entries.size()`, `FOREACH`), so the
empty return is a behavioural gap, not a harmless placeholder.

⚠ **I am NOT claiming an alias was built to hide it.** The group at
`0x826b8b28` (installed by `1b26376b`) is a **blanket COMDAT-identity group with
1030 folded members** — every symbol sharing that trivial 20-byte body — and our
stub's membership is incidental. The honest statement is: **the stub is a real
defect on its own merits, and it is currently invisible to the metric.** Fixing
it is a `ContextChecker` source task, not an alias task.

## 8. What I did NOT do, and why

- **Did not install anything.** Seven of the eight were already installed. The
  eighth (pair 1) is **admissible on the corrected evidence** — see §3.5 — and I
  still did not install it, for two reasons that are about process, not bytes.
  (a) **Scope.** This lane landed at `079beb54`; the coordinator's follow-up
  asked me to correct the record, not to expand the install set. Reopening
  `scripts/symbol_aliases.json` on a landed lane, after the finding that made the
  lane's output zero, is how a correction turns into an unpriced change.
  (b) **It needs its own price.** An install must go through the gate's
  `--install --tier` path and then `tools/ab_measure.py`; 8 sites is a real but
  small prize and deserves a measured delta, not a rider.
  ⚠ **The reason I gave in the first version was partly wrong and is withdrawn**:
  it cited "retail keeping **2** unfolded copies", which was the masking defect
  (§3), and it treated the shared `?MemAlloc@@YAPAXHH@Z` relocation as evidence
  *against* the fold. Name-equal relocations are the strongest fold evidence we
  have — the hazard MAPID-1 documents is `MemAlloc` vs **`_MemAllocTemp`**, two
  *different* allocators, and both spellings here name the same one. Handing pair
  1 to the next lane as a **candidate**, not as a refusal.
- **Did not run `ab_measure`.** The tree is byte-identical to HEAD
  (`git status --porcelain` empty), so there is no change to price. Running an
  A/B on an unmodified tree measures settling noise, not a change, and reporting
  it as a delta would be fabrication. **The delta is 0 because nothing was
  installed** — stated, not measured, and the two are not the same thing.
- **Did not withdraw pair 3's membership.** I cannot: the gate has no
  `--withdraw` path and hand-editing `scripts/symbol_aliases.json` is forbidden.
  It is also a 1030-member blanket group, so withdrawing one member is a
  separate, priceable decision, not a by-product of this lane.
- **Did not treat the `none` ruler as a control.** No alias change was made, so
  there was nothing for it to be blind to — but the standing point holds: for a
  map-only patch `none` reads +0 by construction, and that flatness is the
  signature of the hazard, never a clearance.
- **Did not re-litigate `tools/comdat_fold_gate.py`** (it imports `mask_word`;
  the coordinator audited the defaulted-parameter compatibility). I changed no
  shared helper, so no importer blast radius exists to grep.
- **Did not re-run the permuter** (OFF by standing directive).

## 9. Guard state

All six pre-existing gate/alias tests PASS on this tree, unchanged:

```
test_fold_thunk_gate_mask.py            PASS
test_fold_thunk_gate_install.py         PASS
test_fold_gate_function_extent.py       PASS
test_shape_key_reloc.py                 PASS
test_icf_alias_survivor_gate.py         PASS
test_icf_alias_withdrawal_guard.py      PASS
```

`scripts/symbol_aliases.json` is unchanged at **1,658 groups**; a backup was
taken at `~/tmp/w16ep/symbol_aliases.bak.json` before the install attempt.

## 10. For the next lane

1. ⛔ **RETRACTED: there is no "11-copy unfolded population".** The first
   version of this item told the next lane that any alias against the 84-byte
   `list<T*>::erase` family is refuted before it is written. That was my masking
   defect (§3), and it would have closed a vein that is in fact open — the
   family folds, and pairs 5 and 7 are justified memberships.
   ★ **The replacement rule: use `tools/icf_true_copy_census.py`, and never a
   form-masked comparator, for a whole-image copy count.** A count from a
   form-masked comparator can only be inflated, and it inflates in the direction
   that manufactures evidence *against* folding — i.e. it produces spurious
   REFUSALs, the verdict class nobody reopens.
   ★ **And use the ITERATIVE closure (`icf_fold_evidence.icf_classes`), not a
   flat one-pass class.** A single pass puts pairs 2 and 6 outside their own
   survivor's class because the callee `_M_create_node<T>` has not folded yet.
4. **Pair 1 (`??2Task` → `??2CriticalSection`, 8 sites) is an uninstalled
   candidate**, admissible on corrected evidence (§3.5, §8). It needs the gate's
   `--install --tier` path and its own `ab_measure` price.
2. **`docs/plans/wrong-callee-triage-2026-08-12.json` is stale in its `sites`
   column** (§6). It is still a fine *enumeration* of pairs; it is not a pricing
   instrument.
3. **`GetSongSpecificEntriesForCategory` is unimplemented** (§7).
