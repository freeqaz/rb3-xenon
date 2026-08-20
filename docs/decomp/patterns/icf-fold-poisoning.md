# ICF fold poisoning: when a NAME comparison manufactures a defect

**Lane VTGRIND-GUARD, 2026-08-20.** Tooling: `tools/icf_fold_safe.py`,
gate `tools/test_icf_fold_safe.py` (CI, `--self-break`-verified).

## The defect

ICF folds identical COMDATs, so one retail address serves many functions, and
`scripts/target_symbol_map.json` can name that address with only **one
arbitrary survivor spelling**. Any comparison of retail names against ours
therefore conflates *"folded"* with *"wrong"* — the same disease that makes
objdiff's `LINKER_MERGED` verdict uninformative.

That was known, documented, and fixed. **It was then rebuilt one day later**
by a new longest-common-prefix scan which forgave `<unnamed>` retail slots but
charged folded ones, and reported:

```
XboxContent  INTERIOR@3      # "every later slot shifted -> every caller's vcall is wrong"
```

for a table whose slots 0–13 had already been read **by hand** as aligning.

## Why "extract a helper" was NOT the fix

The author of the poisoned scan **knew about fold poisoning and had personally
fixed it the previous day**. The scan still shipped the defect, because it did
not start from the comparator — it started from the recorded slot *counts* and
re-derived names itself. There were already **two** copies of the
`occ[w] != 1 or within[w] != 1` predicate inside `vtable_order_sweep.py` alone
(`sweep_class` and `map_audit`); the poisoned scan was the third.

⇒ **A helper you must remember to call fails exactly the same way next time.**

## The fix: make the unsafe operation fail loudly

`icf_fold_safe.Slot.__eq__` **raises `FoldPoisonError`** when either side's
identity was destroyed, and `Slot` is deliberately **unhashable** so a
set-difference scan cannot silently treat poisoned slots as distinct members.
A future comparator that writes the natural `if retail != ours:` gets a
traceback naming the module — instead of a silent, confident, wrong verdict of
the exact kind that **closes veins and that nobody re-opens**.

> **Generalises beyond vtables:** when an instrument has a class of inputs on
> which it cannot have an opinion, represent that class as *a value that
> refuses to answer* — never as a name that compares unequal. An "unknown" that
> silently reads as "different" manufactures defects; one that raises cannot.

## Four shapes, and the first version caught only one

| shape | tier | how it hides |
|---|---|---|
| `folded_across` — one address is a slot in many vtables | HARD | the obvious one |
| `folded_within` — one address occupies two slots of the **same** vtable | HARD | `occ == 1`, so the across-filter passes it |
| `unnamed` — map has no name for the address | HARD | scoring it as agreement manufactures SAMEs |
| `nonvirtual_name` / `unrelated_owner` — the name is *suspect* | SOFT | see below |

`folded_within` was measured on `MCContainerXbox`, whose `Format()` and
`Unformat()` are both `{ return (MCResult)0xD; }` — retail's slots 9 and 10
hold **one** address, and the map names it `Format`. Reported as a
`SET_DIFFER "Format vs Unformat"` defect that does not exist.

## Two new criteria, and why they are not the same as folding

**`nonvirtual_name` — vtable membership PROVES virtuality.** A spelling that
decodes to access class `Q`/`A`/`I`/`S` cannot be the true occupant of a vtable
slot, so it proves the *name* is wrong, not our function. `map_audit` already
used this criterion; `sweep_class` never consulted it, so **the two halves of
one file disagreed about the same slot**.

**`unrelated_owner` — a slot of class C can only hold a function of C or a base
of C.** A name owned by a stranger is a fold survivor or a mis-pin. Measured:
51 of 141 reported mismatches; 40 of 86 mismatching classes are entirely this.
Hand-checked, all genuine artifacts:

| class[slot] | retail name | ours |
|---|---|---|
| `MemStream[4]` | `?Ranked@MatchmakingSettings@@QBA_NXZ` | `?Fail@MemStream@@` |
| `RndMat[0]` | `??_GModalKeyListener@@` | `??_GRndMat@@` |
| `SaveLoadManager[6]` | `?Handle@MemcardMgr@@$4PPPPPPPM@A@` | `?Handle@SaveLoadManager@@$4PPPPPPPM@A@` |
| `OvershellSlot[17]` | `?GetBufferSize@HttpGet@@QAAIXZ` | `?DataDir@OvershellSlot@@` |

Those are the three shapes that fold most readily: **deleting destructors,
adjustor thunks** (`addi r3,r3,-N; b target`), and **one-line accessors**.

## ⛔ The over-correction, and the rule that fixes it

Treating SOFT as HARD was measured and **cost coverage for nothing**:

| | mismatches | SAME | UNRESOLVED | comparable slots |
|---|---|---|---|---|
| fold-blind (before) | 141 | 401 | 1,323 | 2,180 |
| SOFT-as-HARD | 68 | 320 | 1,465 | 1,652 |

**127 classes moved SAME → UNRESOLVED.** Every slot they lost was an
**agreeing** slot — provably, since a class whose verdict was `SAME` had zero
mismatches by definition — so the strictness prevented **no false defect at
all**.

> ★ **THE RULE: a suspect name may CONFIRM, but may never ACCUSE.**
> A suspect spelling can only manufacture a false *disagreement*; it cannot
> manufacture a false *agreement*, because our side would have to independently
> produce the identical mangled name, which a fold survivor or a mis-pin cannot
> arrange. **Agreement needs no forgiveness.**

Implemented as `icf_fold_safe.charge(pairs) -> (agree, mismatches, withheld)`.
Withheld pairs are **returned, not dropped**, so they are a worklist for
byte-level adjudication rather than a silent verdict (CLAUDE.md, *"no silent
caps"*).

## Vacuity guard

`assert_can_agree(n_agree, n_population)` refuses a sweep that never once
agreed — the `SAME = 0` tell. The first full vtable sweep read
`SAME=0 / SET_DIFFER=472` purely because our COFF vtable symbol leads with the
`??_R4` Complete Object Locator and retail's does not, shifting every slot by
one. **An instrument that can never agree is reporting its own breakage, not
defects.**

## Reusing this

```python
import icf_fold_safe as ifs

occ    = ifs.fold_counts(all_tables)
retail = ifs.retail_slots(slots, occ, addr2name, hierarchy=hier)  # hier optional
ours   = ifs.our_slot_names(our_coff_entries)                     # drops the COL
agree, mismatches, withheld = ifs.charge(ifs.comparable_pairs(retail, ours))
ifs.assert_can_agree(len(agree), len(agree) + len(mismatches))
print(ifs.exclusion_counts(retail))   # report this; never swallow it
```

**Do not re-derive any of it.** If you find yourself writing
`occ[w] != 1`, you are writing the fourth copy.
