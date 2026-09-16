# W16 targeting: the `obj/Utl` block — the banked row understates its own block by 62%

**Measured 2026-09-16 at main `bed9b148`**, read off `build/45410914/report.json`
and `bin/objdiff-cli diff` at the graded ruler (no `--build`, so no patcher was
skipped). Nothing here is inherited from a prior lane's figure.

## The block

Unit `default/system/obj/Utl` has **9 rows below `fuzzy == 100`, totalling
2,828 B**, and **all nine have ZERO instruction-byte differences** — every
charge is `diff_arg`. But they are three different classes, and the split is
what matters:

| row | B | `masked_equal` | retail callee vs ours | status |
|---|---:|---|---|---|
| `?CopyTypeProperties@@` | 1,472 | False (**honest**) | `_S_sort<int>` vs our `_S_sort<Symbol>` (4 sites, one pair) | **CHASED T1 PROVEN** by W16-GG, 0 `CYCLE-ASSUMED` frames. **No group exists yet.** |
| `?ObjectList@@` | 504 | False (**honest**) | `list<Object*>::insert` vs our `list<const char*>::insert` | withdrawn 2026-08-19, class `FABRICATED_CLOSURE_NOT_PARTITION` |
| `?WalkProps@@` | 408 | False (**honest**) | `list<Object*>::insert` vs our `list<Symbol>::insert` | same withdrawal, same class |
| 5 × `fn_8275*` thunks | 200 | **True (disclosure)** | `__destroy_aux<LocalePanel::Entry>` vs our `~list<…>` | group `0x828043a8` exists: **0 folded**, 2 withdrawn `CF2-FANIN` |
| `?PathCompare@@` | 244 | False | `add r3,r31,r11` vs `add r3,r11,r31` — **register operand order** | GA's drained residual. **DO NOT FUND.** |

**Honest block = 3 rows / 2,384 B — 1.62× the 1,472 B the row was banked at.**
Disclosure = 5 rows / 200 B. Excluded = 244 B.

## The finding that required reading the record, not the percentages

`?ObjectList@@` and `?WalkProps@@` look like ordinary wrong-callee defects and
were nearly written off as such. They are not adjudicated at all: both of our
spellings sit in group `0x823d14c0`'s `withdrawn[]` with

    lane:  ALIAS-CONSOLIDATION 2026-08-19
    class: FABRICATED_CLOSURE_NOT_PARTITION

That is a **procedural** withdrawal — a prior pass built a transitive closure
over alias relations that was not a valid partition, and everything derived that
way was withdrawn wholesale. It says *the derivation was invalid*. It does **not**
say the fold is false, and it carries no retail-byte evidence either way.

⇒ Contrast the *other* group's wording, which is a real bar:
`0x828043a8`'s withdrawals read **"Do NOT re-add. Lane CF2-FANIN. Re-adding
needs a POSITIVE warrant (body or resolved-relocation evidence)."**

**Two withdrawals, two completely different meanings, and only the record
distinguishes them.** A lane reading `withdrawn: 83` as "refuted" would close
912 honest bytes that were never adjudicated; a lane reading it as "free to
re-add" would fabricate an alias and lift the score by construction.

## Corroborating signal, and why it is NOT sufficient on its own

`?WalkProps@@`'s own retail symbol is spelled `…list@VSymbol@@…` — retail's
*name* says the caller is instantiated for `Symbol`, while retail's *body* calls
`list<Object*>::insert`. That is exactly the **CVEIN-1 T1 internal-inconsistency**
shape already recorded in `symbol_aliases.json`: a template instantiated for T
cannot call the T' instantiation, so the callee name must be an ICF survivor's
arbitrary name.

⚠ **That is a hypothesis, not a warrant.** It is derived from the map, and the
map is the thing under test. The standing rule stands: an unproven alias lifts
`name_check` **by construction**, and the `none` control is structurally blind to
it. The adjudication has to run on **retail bytes**.

## What to fund, in order

1. **`_S_sort<int>` ↔ `_S_sort<Symbol>`** — already CHASED T1 PROVEN. Create the
   group, install one membership. **+1,472 B honest, +1 fn.** Lowest risk in the
   block.
2. **`list<Object*>::insert` ↔ our two spellings** — run
   `tools/icf_pair_adjudicate.py`'s chase on retail bytes. **+912 B honest across
   2 rows if it admits.** A REFUSAL is a perfectly good outcome and must be
   recorded as one: W16-GG measured the chase admitting **22/4,000 (0.550%)**
   against a decoy's 87.225%, so a refusal here is the instrument working.
3. **`__destroy_aux<LocalePanel::Entry>`** — only **200 B and all of it
   disclosure** (`masked_equal == True`), against a group whose standing
   instruction demands a positive warrant. Fund last, or not at all.
4. **`?PathCompare@@` — do not fund.** Commutative-`add` operand order is GA's
   drained residual and permuter territory.

## Falsifier for whoever takes this

The three honest rows are `masked_equal == False`, so if they cross,
`matched_functions` and `matched_code` must both rise and `masked_equal_functions`
must hold **Δ0**. If `masked_equal` moves, the rows did not cross the way this
brief predicts and the attribution is wrong — record it as measured, do not
retro-fit.
