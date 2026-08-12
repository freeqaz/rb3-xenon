# The `name_check` alias fixpoint, and what the map-coverage lever is actually worth

2026-08-12, branch `laneK-mapcoverage`, build `45410914`. Companion to
`wrong-callee-triage-2026-08-12.md` (lane E) and to decomp-bench
`archive/runs/namecheck-lane-triage-and-fixers-20260812/`.

Two things this lane set out to do: size the `different_function` charge
population honestly from the retail image, and attack the
`reject_survivor_not_mapped` coverage limit that the banked triage MANIFEST
called "the lever worth pulling next for that project". The first produced a
census. The second produced a refutation and, separately, a gain — from a
different gap than the one the MANIFEST named.

Measured, through a synthetic overlay that is reverted afterwards:

| ruler | before | after | bytes | complete fns |
|---|---|---|--:|--:|
| `none` | 42.220000% | 42.220000% | +0 | +0 / −0 |
| `name_check` | 31.278600% | **31.606758%** | **+33,868** | **+133 / −0** |

`none` is byte-identical at every step. A more permissive variant reaches
31.638264% / +145 functions; see *Two artifacts* below.

---

## 1. The coverage-limit lever is 41 pairs, not 23,024

`tools/icf_alias_build.py --enumerate both` prints a decision census over its
own candidate supply — the `icf_site_census` observations plus the body-hash
enumerator, about 28k pairs and 100k relocation slots. **That is not the
population objdiff charges.** Reading a refusal count off it and calling it a
lever is a category error, because most of those slots are never charged, so
clearing them buys nothing.

`scripts/namecheck_gate_accounting.py` joins the two. Over the 3,879 charged
pairs / 11,344 sites / 8,955 exposed functions:

| decision on CHARGED pairs | pairs | sites | fns |
|---|--:|--:|--:|
| NEVER_PROPOSED / our callee is map-resident | 1153 | 4347 | 3813 |
| NEVER_PROPOSED / census MISS (alignment gate) | 927 | 2356 | 2349 |
| `reject_RELOC_TARGETS_DIFFER` | 582 | 1393 | 852 |
| `reject_RETAIL_DIFFER` | 516 | 730 | 630 |
| `reject_no_evidence` | 323 | 1320 | 969 |
| `reject_T2_over_cap` | 292 | 979 | 715 |
| `reject_gate_c_target_naming` | 43 | 53 | 47 |
| **`reject_survivor_not_mapped`** | **41** | **132** | **66** |
| not a symbol pair (`non_symbol_arg`) | 2 | 34 | 17 |

Zero charged pairs carry an ACCEPT verdict — the consistency check that
everything the generator accepts is already installed and no longer charged.

**22,983 of the 23,024 refused pairs have a survivor spelled `fn_<hex>` or
`lbl_<hex>`.** objdiff's `name_check` tolerates exactly that shape before it
ever compares names (`objdiff-core/src/diff/code.rs`,
`is_placeholder_symbol_name`, applied at the top of `reloc_eq`): a
placeholder-named target is an unidentified split symbol, so the site is
*unverifiable*, not a mismatch. No alias for them can move the metric.

The 41 that survive the join are all `vftable_<hex>` — dtk's vtable
placeholder, which objdiff's predicate does **not** cover. They are essentially
the entire `different_symbol` lane (43 charged pairs, 138 sites, 69 functions).
Section 3 closes them.

**The real blockers are candidate SUPPLY, not gate strictness.** 2,080 charged
pairs were never proposed at all.

---

## 2. /OPT:ICF is a fixpoint; the T1 adjudicator is not

`tools/icf_fold_evidence.py` already says it, for the T2 side: *"Target classes
are refined ITERATIVELY to a fixpoint, which is exactly what a real ICF pass
does (a fold can enable another fold)."* `icf_alias_build`'s T1 adjudicator
compares relocation target NAMES literally, so two template twins whose only
discriminator is a callee we have **already proven** is one folded body come
back `reject_RELOC_TARGETS_DIFFER`:

```
retail  ?_M_create_node@list<Plane>...            calls  ??2@YAPAXI@Z
ours    ?_M_create_node@list<AccomplishmentCond>  calls  ??2CriticalSection@@SAPAXI@Z
```

Those two callees are one address in the landed alias set. Every relocation in
the two bodies therefore resolves to the same address, the masked bytes and the
full `(offset, reloc_type)` sequence are identical, and the linker folds them.
That is the `/OPT:ICF` condition itself, applied one level up.

`scripts/icf_alias_fixpoint.py` relaxes **only** the name comparison, and only
to names already proven to share an address, then iterates. Byte identity, size
identity, the reloc sequence, the CD-9 strict-placeholder refutation and every
hard gate are unchanged. Round 0 is the audited installed file. It converges in
three rounds: +136, +3, +1.

Candidates come from objdiff's own charge list rather than from
`icf_site_census`, whose alignment gate requires the two *enclosing* bodies to
agree on size and reloc sequence — a property of the caller that says nothing
about whether the callee folded.

### A second adjudicator, not a tolerance

`icf_alias_build` merely TOLERATES a retail-side `lbl_`/`fn_` placeholder slot.
A tolerance is what let dc3's alias wave walk a rename into a string literal
with no ruler noticing. The address is in the name, so `ContentResolver` reads
the bytes out of `orig/45410914/band.exe` and compares them to our data COMDAT
with both sides' relocated fields masked: **314 slots resolved SAME, 4 REFUTED
outright**, the rest unreadable or too small to carry information.

### Precision: the decoy control, re-run with the relaxation

`scripts/icf_alias_fixpoint.py --decoy` is `tools/icf_decoy_control.py` with the
relaxation applied. The decoy population is what a naive masked-byte comparator
accepts; the relocation gate must kill the twins in it.

```
naive masked-byte comparator would accept : 619,670 pairs
selectivity, relocation gate STRICT       : 95.49%  (591,739 rejected)
selectivity, gate + alias-class relaxation: 94.44%  (585,230 rejected)
decoys the relaxation CONVERTS            :  6,509  (1.05% of the population)
```

Every converted decoy's differing callees are a proven one-address pair — that
is the whole claim, and the tool prints those slots specifically rather than the
first two differing slots (which are usually already-tolerated placeholders and
make the evidence look absent when it is not).

The relaxation's precision is bounded by round 0: one bad landed group
propagates. `--max-rounds` is capped and each round is reported separately.

---

## 3. The vftable coverage lever, and a COFF reading bug worth remembering

The 41 charged `vftable_<hex>` pairs are DATA symbols, so there is no function
body to adjudicate. The evidence is the vtable's **contents**: retail's function
pointers at the placeholder address, against the map addresses of the symbols
our COMDAT relocates to. 34 confirmed with zero disagreeing slots, **1 REFUTED**
by a slot pointing elsewhere, 7 with too few mapped slots to be worth anything.

The first run came back 0/43, `hit=0` on every pair. **A uniform negative from a
byte comparator is nearly always the wrong instrument, not a refutation.** MSVC
lays a vftable COMDAT out as `[RTTI CompleteObjectLocator*][slot 0][slot 1]…`
and puts `??_7X@@6B@` at section offset 4, so reading the section from 0 shifts
every comparison by one pointer:

```
shift  +0:   1 / 247 mapped slots agree
shift  -4: 246 / 247 mapped slots agree
shift  +4:   0 / 247
shift  -8:   0 / 247
```

The reader now rebases by the symbol's `value`, which is the general fix and not
specific to vtables.

---

## 4. What the `different_function` lane actually is

`scripts/namecheck_df_census.py`, derived from `orig/45410914/band.exe` and the
dtk split objects. Map residency is used only to LOCATE a body, never to decide
one — lane E showed why. 3,487 pairs, 10,637 sites, 8,569 exposed functions:

| bucket | pairs | sites | fns | %sites |
|---|--:|--:|--:|--:|
| `cannot_adjudicate` | 1582 | 4411 | 3746 | 41.5% |
| `fold_thunk_naming` | 411 | 2581 | 2422 | 24.3% |
| `different_callee` | 470 | 1335 | 773 | 12.6% |
| `wrong_callee` | 523 | 1071 | 1050 | 10.1% |
| `icf_fold` | 210 | 545 | 392 | 5.1% |
| `map_assignment_unresolved` | 221 | 491 | 430 | 4.6% |
| `transposition` | 70 | 203 | 200 | 1.9% |

**An alias states a fact for 3,126 sites / 29.4%** — `icf_fold` plus
`fold_thunk_naming`, the second adjudicated by tail-jump shape and `.text`-wide
fan-in rather than by bytes, because a 4-byte body compares equal to everything.
**A genuinely wrong callee is 1,071 sites / 10.1%**, plus 203 more where a
2-cycle means one of the two is wrong and the metric cannot say which.

Cross-check on an independent instrument: the `transposition` bucket comes out at
70 pairs / 203 sites, exactly lane E's count for the same class derived a
different way.

### The 41.5% is not uniform, and each slice names its instrument

That is the test for whether a large "cannot adjudicate" is an answer or a
broken comparator.

| sites | pairs | why | what would settle it |
|--:|--:|---|---|
| 2366 | 533 | retail's body is too small or too masked, so the byte comparator is vacuous | enumerate the fold class at `addr(t)` from `.text` fan-in and ask whether our callee is in it — the argument `fold_thunk_naming` already runs |
| 1539 | 739 | our callee is not map-resident and the bodies differ | equally well explained by our callee not MATCHING yet, so it says nothing about which function is called. The instrument is progress on the callee, not a better comparator |
| 503 | 309 | the survivor is in no live pinned target obj | a coverage question of the split, reachable by pinning |
| 3 | 1 | no readable retail body at either VA | — |

---

## 5. Two artifacts, and the one command that installs either

Nothing here writes `scripts/symbol_aliases.json` — another lane owns it this
session.

| artifact | groups | folded names | `name_check` | complete fns |
|---|--:|--:|---|--:|
| `scripts/icf_alias_delta_fixpoint_grounded.json` | 101 | 139 | 31.606758% | +133 / −0 |
| `scripts/icf_alias_delta_fixpoint_tolerant.json` | 112 | 155 | 31.638264% | +145 / −0 |

**Grounded is the recommendation.** Every differing relocation slot is resolved,
by the proven alias equivalence or by content against the retail image — no
information-free placeholder tolerance anywhere, which is *stricter* evidence
than the shipped file was built on. Tolerant adds 16 pairs carrying 22 tolerated
slots and buys 12 more functions; it is exactly as strict as
`scripts/symbol_aliases.json` already is, no stricter.

```sh
python3 scripts/icf_alias_merge.py --into scripts/symbol_aliases.json \
    --delta scripts/icf_alias_delta_fixpoint_grounded.json --in-place \
  && python3 tools/gen_symbol_alias_map.py
```

Verified to reproduce the measured `build/45410914/icf_aliases.map`
byte-for-byte. The merge is by ADDRESS and additive, so it still applies over
the fold-thunk lane's 1,347 → 1,350 groups.

### Reproduce

```sh
python3 tools/icf_site_census.py  --root . --out <d>/sites_census.json
python3 tools/icf_fold_evidence.py --out <d>/evidence.json
python3 tools/icf_alias_build.py --enumerate both --sites <d>/sites_census.json \
    --evidence <d>/evidence.json --out <d>/alias.json --why <d>/why.json
python3 scripts/namecheck_gate_accounting.py --why <d>/why.json \
    --charges <d>/sites.jsonl --census <d>/sites_census.json
python3 scripts/icf_alias_fixpoint.py --charges <d>/sites.jsonl --vftable \
    --grounded-only --out <d>/delta.json
python3 scripts/icf_alias_fixpoint.py --decoy --out /dev/null     # the control
python3 scripts/namecheck_df_census.py --charges <d>/sites.jsonl -o <d>/df.json
```

`<d>/sites.jsonl` is the charge list from `namecheck_triage.py`.

---

## 6. Negative results, each measured

**Widening the candidate supply with the site census buys nothing.** The obvious
recall fix — feed `icf_site_census`'s observations on top of the charge list —
adds 2 groups and **zero** charged sites. Applying the unchanged gates to the
927 charged pairs the census's alignment gate missed yields **zero** T1 accepts:
387 pairs are vacuous, 274 have no retail body, 245 have genuinely differing
bodies. The alignment gate was not costing recall at T1.

**The map-resident-callee population is not an alias lane.** Lifting the
ingestion drop on the 1,153 charged pairs whose own callee is map-resident and
applying the unchanged gates: 1,134 are refused by gate C (retail's objects name
BOTH spellings, so aliasing them would destroy a real distinction), 1 by
anti-vacuity, 6 by body difference, and only 4 accept. Both refusals are doing
their job. Lane E's finding stands — the repair for that population is a
`target_symbol_map.json` correction, which is deliberately out of scope here
(shared build state, and eval pins ride on that file).

**The installed alias file already violates one-name-one-address 842 times.**
Of its 6,209 names, 842 sit in more than one group; the worst is in 67.
objdiff's `parse_msvc_map` inserts one `name -> group` entry per symbol,
last-wins over a `HashMap` iteration, so a duplicated name's forward lookup
lands in an arbitrary one of its groups — nondeterministically across runs. It
is **not** an over-merge (the per-address group sets are never unioned) and
`reloc_eq` checks both directions, so the survivor's side still resolves. But a
pair whose *both* names are duplicated is a coin flip.
`scripts/icf_alias_merge.py` reports it and refuses under `--strict`. Not fixed
here: it is a property of the file another lane owns.
