# W16-GK pre-registration (written BEFORE any alias edit or A/B)

Baselines measured in this tree at `2d49cc75` + docs commit:
- `report.json`: matched_functions **44,140** · matched_code **4,171,744** ·
  matched_code_percent **40.711586** · fuzzy **50.49751** ·
  masked_equal_functions **23,323** · total_functions **69,240** ·
  total_code **10,247,068**
- validator `icf_alias_finder.py --validate`: **PASS**, 1,659 groups,
  1,408 MAP-CONSISTENT, 250 tolerated, **0 CONTRADICTED**, rc=0
- build liveness reference: `Loaded 6106 ICF equivalence entries`

## Prediction A — T1 (`_S_sort<I>` ↔ our `_S_sort<Symbol,less<Symbol>>`)
Our spelling is MAP-RESIDENT at its own real 424 B body (0x827e5d88) while the
survivor is at 0x824e1858. The group invariant is "exactly one member map-resident".
**Predict: installing T1 makes the validator report CONTRADICTED (FATAL) and exit 1.**
Falsifier: if it validates clean, my map-residency reading is wrong and T1 must be
re-examined rather than refused.

## Prediction B — T2 (`list<Object*>::insert` ↔ our `list<const char*>` / `list<Symbol>`)
Neither of our spellings is map-resident anywhere (CF1: "on the survivor's body or
nowhere"). Restoring both into existing group `0x823d14c0`:
- validator: **PASS**, still **1,659 groups** (no new group), **0 CONTRADICTED**
- alias map: `Loaded 6106` → **6108** ICF equivalence entries (+2 members)
- A/B: **matched_functions +2**, **matched_code +912**,
  **masked_equal_functions Δ0**, **total_code Δ0**, **total_functions Δ0**

Falsifiers, any of which means the attribution is wrong and must be recorded as
measured rather than retro-fitted:
- `masked_equal_functions` moves at all
- `total_code` or `total_functions` moves at all
- matched_code moves by anything other than +912
- the equivalence-entry count does not move (⇒ inert / un-resplit map edit,
  an absent-vs-absent A/B; lane CF-1)

## Stop condition
Adjudicate T1 and T2 and measure ONE A/B of whatever survives. T3 (200 B, all
`masked_equal` disclosure, group carries an explicit "needs a POSITIVE warrant")
is NOT funded. `?PathCompare@@` is NOT funded (commutative `add` operand order,
permuter territory, permuter OFF by directive). No source edits: this is an alias
lane, and a wrong-callee source fix is a different lane's work.
