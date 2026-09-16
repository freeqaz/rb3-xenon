# W16-GK: adjudicating the `obj/Utl` block on retail bytes

**Worktree `~/tmp/wt-w16gk`, branch `w16-gk`, forked from main `2d49cc75`.**
Brief: `docs/decomp/W16_UTL_BLOCK_TARGETING_2026-09-16.md`. Every figure below was
re-measured in this tree; nothing is inherited.

## 0. The brief's census reproduces EXACTLY

`build/45410914/report.json`, unit `default/system/obj/Utl`, all numerics read
through `int()`/`float()`; 0 of 31 rows lack `fuzzy_match_percent` and 0 lack
`match_percent_normalized`, so no test here keys on a field's absence.

| row | B | fuzzy | mpn | `masked_equal` |
|---|---:|---|---|---|
| `?CopyTypeProperties@@` | 1,472 | 99.94566 | 99.945656 | False |
| `?ObjectList@@` | 504 | 99.96032 | 99.96032 | False |
| `?WalkProps@@` | 408 | 99.95098 | 99.95098 | False |
| `?PathCompare@@` | 244 | 99.83607 | **100.0** | False |
| 5 × `fn_8275*` | 5×40 | 99.5 | 100.0 | **True** |

9 rows / 2,828 B. Honest block 3 rows / 2,384 B; disclosure 200 B; excluded 244 B.
Charged-site census (`objdiff-cli diff`, no `--build`, graded ruler) confirms every
pairing the brief tabulated, and that the three honest rows carry **zero**
instruction-byte differences — 4×`bl` on one pair for `CopyTypeProperties`, 1×`bl`
each for the other two.

⚠ A screen that could not fire: my first charge census keyed on a `diff_kind`
field that does not exist in this objdiff's JSON and reported **`charged rows = 0`**
on rows that are provably below 100 — a clean, decisive-looking zero. The real key
is `match_type` (`equal` / `diff_arg`). Recorded because the shape is the
family tell, not because the slip was interesting.

## 1. Instrument gates — run in THIS tree, not inherited

- `icf_pair_adjudicate.py --selftest` → **PASS**, positive control PROVEN, negative REFUTED.
- `--chasetest` → all six controls land as specified, **in-family decoy REFUTED**.
- `--self-break` → with the destination proof removed the vacuous decoy flips to a
  wrong PROVEN, and the run reports `self-break OK -- the control discriminates`.

So the chase can both admit and refuse here.

## 2. Verdicts

*(filled in below as each target is adjudicated)*
### T1 — `_S_sort<I>` ↔ our `_S_sort<Symbol,less<Symbol>>` (1,472 B): **REFUSED**

The brief ranked this "lowest risk in the block" on W16-GG's CHASED T1 PROVEN.
**That chase result reproduces here exactly** — PROVEN, 12 `SLOT-FOLD-OK`,
4 `VACUOUS-DESTINATION-FOLD-PROVEN`, **0 `CYCLE-ASSUMED`** (and `CYCLE-ASSUMED`
is a live label at `icf_pair_adjudicate.py:194`, so that zero is a real negative,
not a label that cannot fire). It is nevertheless **not a warrant**, for three
independent reasons found here:

**(a) The map places our spelling on its own distinct body.**

| spelling | map address | `symbols.txt` size |
|---|---|---|
| survivor `_S_sort<I,less<I>>` | `0x824e1858` | `0x1A8` = 424 B |
| **ours** `_S_sort<Symbol,less<Symbol>>` | **`0x827e5d88`** | `0x1A8` = 424 B |

The group invariant is *exactly one member map-resident*. Installed as an
experiment (commit `95ae14e7`, reverted in `f75f62dc`), the validator refuses:

```
FAIL [W16GK_EXPERIMENT_S_sort @ 0x824e1858]: target objs name 2 members: [...]
CONTRADICTED (FATAL) 1 -- VALIDATE: FAIL        (rc=1)
```

The `contradiction_exempt` precedent at `0x828043a8` does **not** transfer: it
rests on the rival row being a 4-byte uncallable `b` tail-thunk, and `0x827e5d88`
is a full 424 B body with 22 relocations. (Its 4-byte *neighbour* `0x827e5d78` is
such a thunk — checking the neighbour instead of the row is an easy way to talk
yourself into the exemption.)

**(b) The chase is NON-DISCRIMINATING for this particular pair.** Our
`_S_sort<I>` *and* our `_S_sort<Symbol,less<Symbol>>` both chase-PROVE against the
same survivor `0x824e1858`. The chase is not a blanket admitter — an in-family
decoy over all **9** of our `_S_sort` body-twins **REFUTED 7** — but it cannot
separate these two, which is the tool's own documented uniqueness caveat
(`retail_bodytwins 9`, `our_bodytwins 9`) and was not weighed by the brief.

**(c) The map is internally inconsistent here and the direction is UNRESOLVED,
with opposite remedies.** The two retail bodies have *identical masked bytes*
(same sha1) but differ at **7 of 22 relocation slots**, so `/OPT:ICF` could not
have folded them (folding requires relocation-identity — CD-7). Yet:

- `0x824e1858`, labelled `<I>`, calls `??$_S_merge@VSymbol@@…less<Symbol>` — a
  **Symbol** merge. A `_S_sort<I>` cannot call a `_S_merge<Symbol>`: the CVEIN-1
  internal-inconsistency shape, i.e. the `<I>` label may be an arbitrary ICF
  survivor name.
- but a depth-0 self-pair `[our _S_sort<Symbol>, map row 0x827e5d88]` is
  **CHASED T1 REFUTED** — our COMDAT for that spelling is not that body either.

⇒ Reading A: retail folded them, `<I>` is arbitrary ⇒ remedy is an alias.
Reading B: retail kept both and our source calls the wrong instantiation ⇒ remedy
is a **source fix**, and an alias would *forgive a real wrong callee and hide it
permanently*. Both readings are live. Guessing has a 50% chance of cementing the
defect behind forgiveness, which is the one outcome the standing rule forbids
("a metric that hides real bugs is worse than a lower metric").

**Refusing costs 1,472 B and keeps the defect visible.** Filed for a map lane:
adjudicate the `_S_sort` rows at `0x824e1858` / `0x827e5d88`. The family has
**15 map rows at 15 distinct addresses, zero collisions**.

### T2 — `list<Object*>::insert` ↔ our `list<const char*>` / `list<Symbol>` (912 B banked): **ADMITTED**

The brief was right that the `FABRICATED_CLOSURE_NOT_PARTITION` withdrawal is
procedural, not evidential. Stronger than the brief states: this group has
**already restored three** memberships from that same class on retail bytes
(`P6AXXZ`, `PAD` — lane W16-Y; `PAVContent` — lane W16-FM), so re-adjudication on
a positive warrant is the established precedent, not a novelty.

Warrant, all measured here:

- **CHASED T1 PROVEN** for both spellings: 0 `CYCLE-ASSUMED`, 0 `SLOT-REFUTED`,
  0 `BYTES-DIFFER`; 8 `SLOT-FOLD-OK` + 2 `VACUOUS-BUT-IDENTICAL`. That vacuous
  admission demands **full** byte equality *including relocation target names* —
  `/OPT:ICF`'s own folding condition with nothing masked.
- **CF1 map residency — the discriminator that separated T2 from T1**: neither of
  our spellings is resident *anywhere* in `target_symbol_map.json`. Retail placed
  them "on the survivor's body or nowhere", so no contradiction is possible.
- **Pigeonhole** (`--family`): **53** of our spellings collapse onto exactly
  **1** retail address; 47 of the 48 retail body-twins are excluded by the slot-0
  discriminator; `our_slot0_matches_retail True`.
- The chain bottoms out at `_M_create_node@list<CharPollableSorter::Dep*>`,
  corroborating this group's own `restored[]` reading that retail allocates with
  `li r3,12` = 8 + `sizeof(void*)`. `Symbol` is exactly one `const char* mStr`
  (`src/system/utl/Symbol.h:13`) = 4 B on ILP32, so `list<Symbol>` and
  `list<const char*>` share the 12-byte node with `list<T*>`.
- `icf_pair_adjudicate.py`'s **own source comment** records that this very pair
  (`list<char*>::insert`) falsely blocked lanes **W8-D and W9-B** on the vacuity
  guard — the historical refusal was an instrument artifact, never evidence.

### T3 (200 B) and `?PathCompare@@` (244 B): **NOT FUNDED**, deliberately

T3 is 5 rows × 40 B, **all `masked_equal == True`** (disclosure, not honest), and
group `0x828043a8` carries an explicit *"Re-adding needs a POSITIVE warrant"*.
Its own record also notes such a member "was never in the admission predicate".
⚠ Worth recording: the T1 chase *did* surface the T3 pair as
`VACUOUS-DESTINATION-FOLD-PROVEN` en route. That is a hint, **not** the positive
warrant the group demands, and 200 B of disclosure did not justify spending the
lane's remaining budget to convert it. `?PathCompare@@` is commutative `add`
operand order — lane GA's drained residual, permuter territory, permuter OFF.

## 3. A/B — the brief and my own prediction under-priced this by 5×

`tools/ab_measure.py`, run in **both** directions, settled, split fixed point on
both legs, 0 recompiles, `renamer_patched=1833`, ruler `name_check` (from
`objdiff.json`).

| | pre-registered | **measured** |
|---|---|---|
| Δ`matched_functions` | +2 | **+13** |
| Δ`matched_code` | +912 B | **+4,576 B** |
| Δ`matched_code_percent` | — | **+0.044659 pp** |
| Δ`masked_equal_functions` | 0 | **0** ✅ |
| Δ`total_code` / Δ`total_functions` | 0 / 0 | **0 / 0** ✅ |

Leg A reproduces the recorded main baseline to the digit (44,140 / 4,171,744 /
40.711586 / 50.49751 / 23,323), and the revert leg is the exact mirror (−13 /
−4,576 B), so the delta is attributable rather than drift.

**Why the miss:** the brief priced the two rows *in `obj/Utl`*, but an alias
membership pays at **every caller of that spelling**. 13 rows cross to
`fuzzy == 100` across **9 units**, summing to exactly 4,576 B — and **all 13 are
`masked_equal == False`**, i.e. entirely honest, none disclosure:

| B | unit | row |
|---:|---|---|
| 948 | EventTrigger | `?LoadOldEvent@EventTrigger@@` |
| 504 | **obj/Utl** | `?ObjectList@@` |
| 424 | UITrigger | `?Load@UITrigger@@` |
| 416 | DirLoader | `?MakeFileList@@` |
| 408 | **obj/Utl** | `?WalkProps@@` |
| 388 | BandWardrobe | `?SelectExtra@BandWardrobe@@` |
| 352 | EventTrigger | `??$PropSync@VSymbol@@@@` |
| 292 | MoviePanel | `?ChooseMovie@MoviePanel@@` |
| 280 | MetaMusicScene | `?Configure@MetaMusicScene@@` |
| 220 | AccomplishmentManager | `?ConfigureAccomplishmentGroupToCategoriesData@` |
| 144 | EventTrigger | `?resize@?$list@VSymbol@@…` |
| 100 | UIListProvider | `?Disable@DataProvider@@` |
| 100 | UIListProvider | `?Dim@DataProvider@@` |

`default/MetaMusicScene` reaches **100%** (`MATCHED_ROSE`, mpn ruler 195→196).
Two further rows improve without crossing (they carry other charges):
`BandDirector::OnGetFaceOverrideClips` 99.074→99.136,
`BandWardrobe::OnSelectExtras` 99.957→99.978.

★ This is "**price the block, not the row**" in its *cross-unit* form: the Utl
block was where the rows were noticed, not where the value is. 3 of 13 crossing
rows (912 B of 4,576) are in `obj/Utl`; **80% of the payout is in eight other
units.** Any future alias candidate should be priced over its whole caller
population before being ranked against other work.

## 4. Gates

- **Liveness gate FIRED** (the requirement that distinguishes a live map edit from
  an inert one, lane CF-1): `GEN ICF-ALIAS MAP` 6,922 → **6,924 symbol lines**,
  and objdiff `Loaded 6106` → **6108 ICF equivalence entries** — +2, exactly the
  two memberships, in the direction the edit implies. Both legs forced a re-split
  (`split=1`, `renamer_patched=1833`).
- **Validator** `icf_alias_finder.py --validate`: **PASS before and after** —
  1,659 groups, 1,408 MAP-CONSISTENT, 250 tolerated, **0 CONTRADICTED**, rc=0.
  Class counts identical across the edit; **nothing pruned**, group total did not
  fall (the group grew 5 → 7 folded, 83 → 81 withdrawn).
- **`alias_withdrawal_audit.py`**: live-and-withdrawn **HOLDS at 4** (not 6) —
  the withdrawn records were removed and preserved under
  `withdrawal_record_superseded`, per the W16-FM convention whose absence
  previously drove that count 4 → 5.

### ⚠ The `ALIAS_SUSPECT` guard FIRES on this patch — and that is expected

```
[control none] ALIAS_SUSPECT: ⚠ SHAPE ALERT: default ruler UP (+4576 B) while
`none` is FLAT on a map-only patch — the FABRICATED-ALIAS shape.
Adjudicate on retail bytes before landing.
```

It fires on **every** map-only alias addition, real or fabricated, because `none`
ignores relocation names entirely — that flatness is the documented *blind spot*,
never a clearance. It is answered by §2's retail-byte warrant and by nothing else.

⚠ Note the direction trap: measured as a **revert**, the guard stays silent
("`none` UNMOVED and default not up"), because its predicate needs the default
ruler going *up*. A lane that measured only the revert direction would report a
quiet control and never learn the guard had an opinion. Both directions were run
here for exactly that reason.

## 5. What this lane deliberately did NOT do

- **No source edits.** The T1 finding (our `CopyTypeProperties` may call the wrong
  `_S_sort` instantiation) is a real decomp lead but belongs to a source lane, and
  reading B above must be settled before anyone acts on it.
- **No T3 conversion**, despite the T1 chase incidentally surfacing its pair as
  fold-proven: 200 B, all disclosure, against a standing "positive warrant" bar.
- **No `?PathCompare@@`** — permuter territory, directive says OFF.
- **No map repair** of the `_S_sort` rows. Renaming a map row is a different
  channel with its own economics (un-pairing is ~80% of a map edit's delta) and
  this lane did not measure it.
