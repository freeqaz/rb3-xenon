# W16-S — the T1 alias sweep: identification is not folding

**Lane:** W16-S (opus) · **Date:** 2026-09-14 · **Base:** main `22b583d5`
· **Worktree:** `~/tmp/wt-w16-s`, branch `w16-s`
· **Instrument:** `tools/w16s_alias_census.py` (new) · **Sizing:** `tools/w16s_ablate.py` (new)
· **Census artifact:** `docs/decomp/W16S_alias_census_2026-09-14.json` (5,315 rows)

## What this lane was asked, and what it found

W16-Q found that one T1-proven alias membership was not a fold at all. T1 reads

> retail bytes at the survivor address are byte-identical (modulo relocated
> fields) to our compiled body for the folded spelling

which proves **`X` *is* `N`** (an identification) and says nothing about whether
**`N` folded with `S`**. A fold under `/OPT:ICF` is a claim about **two COMDATs
of ours** being identical *to each other*. T1 only ever looks at one side, so it
is structurally incapable of separating the two. W16-Q's closing recommendation
was that other T1 memberships may carry the same error and that auditing them is
a lane-sized sweep. This is that sweep, over **all 5,315 live memberships** in
`scripts/symbol_aliases.json` (not only the T1 ones — the whole file, so the T1
slice can be compared against populations that were installed differently).

**The headline is not the number of bad memberships. It is that the obvious
stricter test — "our `N` == our `S` including relocation target names" — is
*itself wrong*, because it is stricter than the linker.** Four of my own
intermediate results were refuted by controls before anything was written to the
alias file, and the largest correction went in the *conservative* direction: my
first run called 776 memberships contradictions; the final figure is 16, of
which exactly **one** survived review of the evidence already on record.

---

## The instrument, and the four defects found while building it

The discriminator has to be entirely our-side, because that is what the linker
tests:

    our COMDAT for N == our COMDAT for S, bytes AND relocations (offset, target NAME, type)

No map resolution, no retail address, no masking — so it is not subject to the
masked-T1 thunk vacuity. That is `tools/ourside_fold_sweep.py`'s argument,
applied to the **alias file's memberships** instead of to `name_check` charges
(that tool iterates `namecheck_df_census.json` rows, so it had to be extended;
its comparator and COMDAT indexing are reused, not reimplemented).

Getting there required fixing four things. Each was caught by a control, and
three of them contradicted what I expected.

### 1. `comdat_bytes.comdats()` bills the EH funclet into the function body

`body = whole[val:]` runs to the **end of the section**, and an EH-bearing MSVC
`/Gy` COMDAT holds the function **and its trailing `__unwind$` funclet** in one
section. A retail extent from `.pdata` is the function only.

This is a **one-sided reader artifact**: it *cancels* when both sides are read
the same way (our `N` vs our `S`) and does **not** cancel against retail, where
it manufactures confident size "contradictions". Measured: **587 memberships
over 30 STLport addresses, every one a constant −44 B**. A single dominant delta
across hundreds of independent memberships is the signature of a reader, not of
587 bugs — same family as STLPORT-1's phantom "+8 B STLport source bug".

Known-answer check: `_Copy_Construct` @`0x823d3ac8` — section 112, symbol value
8, `__unwind$15409` at 68 ⇒ the function is `[8,68)` = **60 B = retail's
`.pdata` extent exactly**.

Fixed **additively** (new `fn_size`/`fn_raw`/`fn_relocs`/`fn_end`/`fn_bounded`
keys; `size`/`raw`/`bytes`/`relocs` keep their old meanings) so no existing
consumer changes behaviour.

> ⚠ **This defect is not confined to my census.**
> `tools/ourside_fold_sweep.py`'s `corroborates()` does `n != len(shared_raw)
> → return False`, so it has been **silently refusing every EH-bearing pair**
> for as long as it has existed. Its *admissions* are unaffected (a refusal
> cannot admit anything), so nothing it installed is in doubt — but its
> coverage is smaller than it appears.

**Prediction stated before re-running, then measured:** `FOLD_CONFIRMED` must be
*unchanged* by this fix, since the artifact cancels in the our-vs-our test.
**Measured 3,636 → 3,636, exactly unchanged.** 658 memberships moved out of
CONTRADICTED, and CONTRADICTED did not collapse to zero — so the fix did not
turn the test into one that cannot fail.

### 2. My own first "identification" class was the masked-T1 vacuity, one level up

My first classification called **940** memberships identifications. Inspecting
the biggest group (`gi=114`, 41 identifications at one address) showed
`survivor_vs_retail = EQ`: retail's body at `X` matched our `N` *and* our `S`.
The retail comparison masks relocated words whose destination the map does not
name, so two bodies differing **only in relocation target names** both read EQ —
`masked_only` was true on 659 of the 940.

**An identification requires the address to discriminate.** The rule is now:
retail@`X` must match our `N` **and NOT match our `S`**. 940 → **208**.

### 3. Data COMDATs were skipped, which mislabelled live vtable aliases as stale

`if not v.get("is_code"): continue` dropped every data COMDAT. **143 of the 289
spellings the census called `STALE_SPELLING` are `??_7X@@6B@` vtables.**
`/OPT:ICF` folds identical data COMDATs too, and `reloc_eq` does not care
whether a relocation target is code. Labelling live vtable aliases "stale" is
exactly what would license the prune the house rule forbids.

### 4. ⛔ THE NULL FAILED — liveness is REFERENCE, not DEFINITION

**Prediction:** ablating `STALE_SPELLING` reads exactly 0 — a spelling our build
does not define cannot forgive anything.
**Measured: −8 functions / −8,936 B.**

Cause: objdiff compares relocation **target names**, and we **compile but never
link**, so a relocation may name a symbol no obj of ours defines. Two candidate
explanations were tested and **both refuted** before the right one was found —
0 of 289 are in the target objs, and 0 are defined as data COMDATs — leaving
"referenced but undefined", which the symbol tables confirm.

Split into `REFERENCED_UNDEFINED` (108) and `ABSENT_FROM_BUILD` (273) and
re-ablated: `REFERENCED_UNDEFINED` carries the **whole** −8 / −8,936, and
`ABSENT_FROM_BUILD` reads **0.000000 on all four measures**, as does
`DATA_NEEDS_SOURCE`. The null now behaves as a null.

> ⛔ A Δ0 today still does **not** license a prune. That is the standing rule
> (a prior prune cost +94,616 B to reverse) and nothing here weakens it.

---

## ★★★ The load-bearing correction: `/OPT:ICF` is a FIXED POINT, not name identity

"our `N` == our `S` including relocation target **names**" is **stricter than
the linker**. MSVC folds to a fixed point: two bodies that differ only in *which
symbols their relocations name* still fold **if those targets are themselves
co-folded**.

This is not a marginal concern. Measured on the strict test's own output, before
the closure was added:

| class under the strict test | masked bytes + reloc shape identical, **names differ** | genuinely different shape/size |
|---|---:|---:|
| `CONTRADICTED_ON_RETAIL` (118) | **115** | 3 |
| `UNDECIDED_MASKED` (732) | **732** | 0 |
| `IDENTIFICATION_NOT_A_FOLD` (208) | 73 | 135 |

⇒ the strict test's "contradictions" were **overwhelmingly the closure's
candidates**. Shipping them would have closed veins on an artifact of my own
instrument — the disease CLAUDE.md records as *"a tool's confident 'unfixable'
is the claim most worth auditing, because it CLOSES veins and nobody re-opens
them."*

`icf_closure()` is partition refinement, exactly as ICF computes it:

* **seed** — (masked body bytes, relocation offsets+types)
* **refine** — append the *current class id* of every relocation target, in order
* **stop** — when the class count stops changing

A target no COMDAT of ours defines is its own singleton (we compile, we never
link), which is **fail-closed**: it can only keep classes apart, never merge
them. **87,463 COMDATs → 43,050 fold classes** — a real reduction, not a
degenerate one, and pairs differing in masked bytes can never merge by
construction.

Effect: `CONTRADICTED` **118 → 21**, and **→ 16** once the closure is applied to
the **branch-destination** comparison as well. Charging a spelling difference at
one remove, in the callee, would defeat the closure's whole purpose. All 79
address-less "partitioned" memberships resolve as genuine closure folds.

**Reproducibility defect fixed alongside it:** `max()` over a *set* of tuples
containing bytes breaks ties by set iteration order, which `PYTHONHASHSEED`
randomises — two runs on an **unchanged tree** gave 43,053 and 43,051 classes. A
census that does not reproduce is not evidence. Tie-breaks now include the
payload; two consecutive runs now produce **byte-identical** census JSON.

---

## The census

All 5,315 live memberships of `scripts/symbol_aliases.json` at `22b583d5`.

**Bytes are measured by ABLATION** — the class's memberships are removed from
`folded` in the alias file, `ninja` regenerates `icf_aliases.map` and
`report.json` (the report cache is purged by the build's own stamp), and the
difference is read out by exact key with `int()` coercion. This is ALIAS-2's
rule: a name-keyed census of what an alias "covers" books its own blind spot as
risk. The "our-body B" column is the name-keyed figure and is **informational
only** — note how badly it tracks the ablation.

| verdict | memberships | groups | our-body B *(informational)* | **Δ matched_functions** | **Δ matched_code** | **Δ code %** |
|---|---:|---:|---:|---:|---:|---:|
| `FOLD_CONFIRMED` | 3,636 | 639 | 182,120 | **−2,967** | **−798,520** | −7.793513 |
| `UNDECIDED_MASKED` | 643 | 47 | 41,360 | −124 | −40,768 | −0.397895 |
| `FOLD_CONFIRMED_CLOSURE` | 309 | 120 | 37,508 | −149 | −49,356 | −0.481710 |
| `ABSENT_FROM_BUILD` | 273 | 86 | 0 | **0** | **0** | **0.000000** |
| `IDENTIFICATION_NOT_A_FOLD` | 175 | 98 | 16,760 | −98 | −25,420 | −0.248100 |
| `NEEDS_SOURCE` | 120 | 35 | 9,096 | −64 | −15,692 | −0.153155 |
| `REFERENCED_UNDEFINED` | 108 | 107 | 0 | −8 | −8,936 | −0.087214 |
| `DATA_NEEDS_SOURCE` | 35 | 34 | 1,504 | **0** | **0** | **0.000000** |
| `CONTRADICTED_ON_RETAIL` | 16 | 12 | 456 | **0** | **0** | **0.000000** |
| **TOTAL** | **5,315** | — | 288,804 | −3,410 | −938,692 | — |

⚠ The TOTAL row is the **sum of nine independent single-class ablations**. Class
sizes are not guaranteed to compose into a joint ablation, and no joint ablation
was run, so treat it as an order-of-magnitude cross-check against ALIAS-2's
whole-mechanism figure (818,416 B at `64088f62`), not as a measurement.

Mapping onto the brief's four classes, which had no cell for two of these:

* **fold** = `FOLD_CONFIRMED` + `FOLD_CONFIRMED_CLOSURE` = **3,945** memberships
* **identification** = **175**
* **contradicted** = **16**
* **needs source** = `NEEDS_SOURCE` + `DATA_NEEDS_SOURCE` = **155**
* **stale** = `REFERENCED_UNDEFINED` + `ABSENT_FROM_BUILD` = **381** — ⛔ and
  these are *not* one class: 108 are LIVE and 273 forgive 0
* **`UNDECIDED_MASKED` = 643** — the brief's table has no cell for this, and it
  is the second-largest class. See below.

### `UNDECIDED_MASKED` (643) is the honest name for a large slice

These are memberships where retail@`X` compares equal to our `N` **and** our
`S`, because every relocated word whose destination the map does not name is
masked. The address cannot discriminate, so **nothing is claimed** — neither
fold nor identification. All 643 are "masked bytes + reloc shape identical,
names differ", i.e. they are exactly the population the closure *would* resolve
if the differing targets could be placed. They forgive **−124 fns / −40,768 B**.

Turning these into verdicts needs **map identifications** for the branch
destinations, not source work. That is the same lever MAPID-1 used at
`0x827bcd38`, and its payout there was **bug exposure, not bytes**.

### `IDENTIFICATION_NOT_A_FOLD` (175 over 98 groups) — W16-Q's error at scale

W16-Q's finding replicates, though at a quarter of my first estimate. Shape:

* **156 of 175** have ≥1 branch destination adjudicated **by name** (not masked-only)
* `survivor_vs_retail`: **104 NE**, **67 SIZE** — in every case our `S` is
  demonstrably *not* the body at `X`
* **135 of 208** (pre-closure count) were "genuinely different shape/size", so
  the class is not an artifact of the name-vs-closure distinction
* 92 are singleton groups whose one live membership is the identification —
  **exactly the W16-Q shape**

Worked example, `0x82772870`: the map names it
`vector<JumpInstance>::erase`; our COMDAT for that spelling is **64 B**, while
retail's body there is **92 B** and equals our `vector<Phrase>::erase`. The
survivor name is wrong for that address.

**None of the 175 was corrected.** Reasons under NOT-DONE.

---

## The control: 53 groups that already carry the our-side proof

The 53 groups whose `evidence` begins `tools/ourside_fold_sweep.py` were
installed on the our-side COMDAT-identity argument, so the answer is
independently known and the instrument must agree on every one.

> **53 groups / 279 memberships → `FOLD_CONFIRMED` on 279/279.**

Re-run and unchanged at every stage of the instrument's development (before the
extent fix, after it, after the discriminate-rule, after data COMDATs, after the
closure, after the determinism fix). A control that never moves while the thing
it controls moves by 760 memberships is doing its job.

⚠ **What this control does and does not license.** It is a *positive* control on
a population selected for being provable; it shows the instrument does not
*refute* known folds. It cannot show the instrument does not *over*-admit —
that job is done by the surviving 16 contradictions and 175 identifications
(a test that admitted everything could produce neither) and by the closure being
unable, by construction, to merge bodies whose masked bytes differ.

---

## Corrections: predicted vs measured

### Applied — 1 withdrawal, `0x8280cb08`

| | |
|---|---|
| group | `gi=890`, survivor `?Export@RndDir@@$4PPPPPPPM@IE@AAXPAVDataArray@@_N@Z` |
| withdrawn spelling | `?Replace@RndTransformable@@$4PPPPPPPM@IE@AAXPAVObjRef@@PAVObject@Hmx@@@Z` |
| why | 16-byte vtordisp thunk. Retail branches to `?Export@RndDir@@UAA…`; ours to `?Replace@RndTransformable@@UAA…` — different methods, different arity. Both callees defined our-side, different fold classes **under the closure, applied to destinations as well as bodies**. Installed on MASKED T1 — the tier `w33_fold_adjudicate` documents as vacuous for forwarders — and recorded `0 census sites`. |
| **predicted** | **Δ0 exactly** (the whole 16-membership class ablates to 0.000000) |
| **measured** | **Δ0 exactly**: 43,219 / 3,939,232 / 38.4467% / fuzzy 49.344624, unchanged on all four |
| non-vacuity | regenerated `icf_aliases.map` carries **0** occurrences of the withdrawn spelling and **1** of the survivor ⇒ applied, and the group was kept |

Group **kept**, `folded` emptied of that one spelling, a dict record appended
under `withdrawn`. **Nothing pruned.**

### Applied, then REVERTED — 5 withdrawals. This is the more useful result.

I withdrew six, then read the `evidence` strings in the diff and reverted five,
because **the evidence already on record outranks my instrument**:

* **4 are `VT1` (lane W15-D) groups proven by retail RTTI VTABLE GEOMETRY** —
  both classes' vtables hold the one address at the same subobject offset and
  slot (e.g. `CharData` and `CharWeightable`, offset 28, slot 6, at
  `0x823af220`), with all members branching to one retail destination. That is
  **direct retail evidence the fold is real**, and it is better evidence about
  *retail* than an our-side closure can ever be. My "contradiction" states only
  that **our two implementations differ** ⇒ these are **our-side source
  divergences at addresses where retail demonstrably folded** — a worklist for a
  porting lane, not alias defects. Rows: `0x8234ebc8` (BandTrack::Copy /
  BandDirector::Replace), `0x823af220` (CharData/CharWeightable::Handle),
  `0x824863e8` (RndCamAnim/RndLightAnim::SyncProperty), `0x82493ca0`
  (RndMotionBlur::Save / CharTransDraw::Save).
* **1 was installed by `icf_alias_fixpoint.py`**, whose closure closes name
  equality **under the already-installed aliases**, while mine deliberately does
  not. The disagreement is a difference of closure **input**, not a demonstrated
  contradiction. (`0x822a83c0`, `_Destroy_Range<NavItem*>` /
  `_Destroy_Range<Piercing*>`.)

### Not withdrawn — the remaining 10 of 16

* **9 are WEAK**: our own relocation names a callee our build does not
  **define**, so `closure.get()` returns `None` and the test **fails closed into
  a false contradiction**. Three of these are `??_GUIPanel` (retail) vs
  `??_EUIPanel` (ours) — *same class*, scalar-vs-vector deleting destructor, a
  very plausible real fold.
* **1 rests on a SIZE argument** (`0x827f42a8`, retail 16 B vs our 12 B). Our
  side is clean (`section_size 12`, `fn_bounded False` — no truncation), but a
  size-based withdrawal is precisely what cost this project **six wrongly
  withdrawn folds** in STLPORT-1, so it is left in place pending an explicit
  two-sided normalization.

---

## Final worktree measures

Read from `build/45410914/report.json` with `int()` coercion, after the
withdrawal build (`rc=0`), ruler `functionRelocDiffs=name_check` per
`provenance.diff_config`:

```
matched_functions     43219
matched_code        3939232
fuzzy_match_percent  49.344624
matched_code_percent 38.4467      total_functions 69217   total_code 10245956
```

Identical to the `22b583d5` baseline on every key, as predicted.

---

## What I did NOT do

* **None of the 175 identifications was corrected.** Each needs a map re-home at
  `X` from the survivor's spelling to `N`, and the map/name economics make that
  a *bet*: un-pairing is 80.5% of a map edit's delta and the cascade only 19.5%,
  and proving a name wrong does **not** make renaming safe — if the base obj for
  the unit cannot define the new name, the row is permanently 0%. I measured
  that gate: of the 208 pre-closure rows, **113 could pair after a rename and 95
  could not**. 98 groups is not a blast radius this lane could predict per-row
  and verify, and the brief requires the prediction to be written down first.
  A follow-up lane has the ranked worklist in the census JSON
  (`verdict == IDENTIFICATION_NOT_A_FOLD`, with `survivor_vs_retail` and
  `masked_only` per row).
* **No joint ablation** across classes, so the TOTAL row is a sum, not a
  measurement.
* **`UNDECIDED_MASKED` (643) was not resolved.** It needs map identifications
  for branch destinations, which is the MAPID-1 lever, not source work.
* **The `NEEDS_SOURCE` (120) and `DATA_NEEDS_SOURCE` (35) classes were not
  investigated** beyond classification and sizing.
* **W16-Q's secondary items 2–4 were not started** — the three scattered bodies
  (`CharWeightable` 144 B, `CharBonesMeshes` 216 B, `RndLightAnim` 140 B), the
  unexplained +1 function on W16-Q's re-home, and the `HamCamShot.cpp`
  scatter-include removal. The brief sequenced them after item 1 had a census
  *and a landed first batch*; the census consumed the lane, and the first batch
  deliberately shrank to one row.
* **`tools/ourside_fold_sweep.py` was not re-run** after the `comdat_bytes` fix.
  Its coverage should grow now that EH-bearing pairs are no longer refused on
  the funclet artifact; its existing admissions are unaffected.
* **No `src/` file was touched**, so nothing in this lane can affect the native
  build. The gate was run anyway.

## Nothing here is claimed unfixable

The one verdict that closes a vein is `CONTRADICTED_ON_RETAIL`, and this lane
*shrank* it from 776 to 16 and then acted on 1. For the 9 WEAK rows the blocking
instrument is named precisely: the `/OPT:ICF` partition refinement in
`tools/w16s_alias_census.py`, which cannot place a callee our build does not
define and therefore fails closed. **What would overturn it:** a definition of
that callee in our build (porting), or a retail-side identification of the
address the callee occupies, after which the closure decides the row without
change to the method. For the 4 VT1 rows nothing is unfixable at all — they are
source divergences with a known correct answer in retail's vtable geometry.

## Commits on `w16-s` (off main `22b583d5`)

```
5164fd14  W16-S: census instrument -- fold vs identification, plus a COMDAT extent fix
419d6608  W16-S: data COMDATs + reference-liveness; ablation harness; the failed null
e6b20345  W16-S: model /OPT:ICF as a FIXED POINT, not as name identity
c2f34762  W16-S: withdraw ONE refuted masked-T1 thunk fold -- predicted Delta 0, measured Delta 0
```

Diff vs `22b583d5`: 5 files, **19 lines changed outside the new tools and the
census artifact** (`scripts/symbol_aliases.json` 15/4, one group) --
`tools/comdat_bytes.py` +37 (additive), `tools/w16s_alias_census.py` +456 (new),
`tools/w16s_ablate.py` +104 (new),
`docs/decomp/W16S_alias_census_2026-09-14.json` (new, the census).
**No `src/` file was modified.**

## Gate

Run in `~/tmp/wt-w16-s` as this lane's last action; full log at
`/home/free/tmp/native_gate_tmp_wt-w16-s_543463257.log`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, so this is full coverage and not the INCOMPLETE shape.
