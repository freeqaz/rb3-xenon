# W16-CO — `default/band3/bandtrack/Gem` adjudicated: the gap is 93.2% relocation-name class

Lane W16-CO, 2026-09-15, worktree `~/tmp/wt-w16-co`, branch `w16-co`, base `0702a14f`.
**Net source change: ZERO.** Two source hypotheses were tried, measured, and reverted.
This document is the deliverable.

## 1. Baseline (read from `report.json`, not inherited)

```
default/band3/bandtrack/Gem
  matched_code 16,812 / total_code 30,264 = 55.551147 %   gap 13,452 B
  matched_functions 178 / 222              fuzzy 82.545204
  59 sub-100 rows
```
Whole-binary at baseline: `matched_functions 43,915 / matched_code 4,109,688 / 40.10599 %`.
Re-measured identically after a full build (tree was already a fixed point), and restored
byte-identically after the experiments were reverted.

## 2. The gap decomposes into two classes, and NEITHER is ordinary source work

| class | rows | bytes | share of gap |
|---|---:|---:|---:|
| `fuzzy == 0` — our object defines **no counterpart** (`base_size = 0`, verdict STUB) | 18 | 5,276 | 39.2 % |
| `0 < fuzzy < 100` — near-miss | 41 | 8,176 | 60.8 % |

⚠ **The brief said the zero class was "four rows, ~3,412 B". It is 18 rows / 5,276 B**, and
two of them are *named* (anonymous-namespace `__destroy_range_aux<reverse_iterator<Unlockable*>>`
and `<Label*>`), not `fn_` rows. Do not inherit the four-row figure.

★ **The 41 near-miss rows carry ≈ 11 bytes of instruction-level divergence in total**
(Σ size×(1−fuzzy/100)). They withhold 8,176 B because `matched_code` is all-or-nothing per
row — on average **one charged site per row**. That is the correct way to read this unit:
not "8 kB of wrong code" but "41 rows each one charge from paying out".

## 3. Every one of the 41 near-miss rows is charged ONLY on `diff_arg`

Priced from `report.json`'s charged-site list via `objdiff-cli diff --batch` on the **graded**
ruler (`objdiff.json` pins `functionRelocDiffs=name_check` + the three companion keys), never
from a mismatch count and never from an "N/N instructions equal" reading.

Adjudicated against `scripts/symbol_aliases.json` (1,657 groups, 1,657 distinct survivors):

| class | rows | bytes |
|---|---:|---:|
| ALIAS_PRESENT (already forgiven) | 0 | 0 |
| **ALIAS_MISSING** — survivor group exists, our spelling absent from `folded` | 13 | 1,740 |
| **NO_GROUP** — target callee has no alias group at all | 23 | 5,876 |
| **NONRELOC** — genuine instruction-level charges | 5 | 560 |

⇒ **7,616 B / 93.2 % of the near-miss surface is relocation-name (ICF fold) class.**
A lane forbidden to edit `symbol_aliases.json` cannot collect it. That is the single most
important fact about this unit, and it is why the unit's headline 55.55 % is not a measure of
how much of `Gem` is written wrong.

The fold reading is not the detector restating its input — the target-side callee names are
semantically impossible for their call sites, which is the signature of ICF naming the survivor
arbitrarily, **not** of a wrong callee:

| row | our callee (correct) | target's callee (survivor) |
|---|---|---|
| `Gem::AddRep` | `push_back<vector<Tail*>>` — `mTails` **is** `vector<Tail*>` | `push_back<vector<ChatReceiver*>>` |
| `Gem::InitChordInfo` | `GameGem::GetChordNameOverride` | `NetCacheMgr::GetXLSPFilter` |
| `Gem::~Gem` | `DeleteAll<vector<Tail*>>` | `DeleteAll<vector<NetSavedSetlist*>>` |

Both members of each pair are byte-identical template/accessor instantiations. Our source is
already right; the charge is the ruler pricing a fold it has no alias for.

## 4. ⛔ THREE ALIAS_MISSING ROWS ARE UNDER A STANDING WITHDRAWAL — 776 B IS NOT COLLECTABLE

Checked before reporting, because a lane was retracted on main this same day for exactly this
(`7ec1e249`). Of the 13 ALIAS_MISSING rows (1,740 B):

| bytes | row | our spelling's status in the survivor's group |
|---:|---|---|
| 368 | `PropSync<ObjPtr<RndTex>>` | ⛔ **WITHDRAWN** |
| 324 | `vector<MatSwap@OutfitConfig>::operator=` | ⛔ **WITHDRAWN** |
| 84 | `OldColorOption::OldColorOption` | ⛔ **WITHDRAWN** |
| 1,288 | the other 10 rows (incl. `AddRep` 604, `InitChordInfo` 188, `~Gem` 96) | no withdrawal of our spelling |

⇒ **1,288 B**, not 1,740 B, is the un-withdrawn ALIAS_MISSING surface. Any future proposal
here must re-run this check; a withdrawal is a recorded adjudication, not noise.

## 5. Negative result #1 — commutative operand order is NOT source-steerable (`PartialHit`, 284 B)

`?PartialHit@Gem@@QAAXI@Z` has exactly 3 charges: `and. r10, r11, r10` ×2 (target puts the
shift **first**; ours puts `mSlots` first) and one `lwzx` base/index swap.

**Predicted:** rewriting `mSlots & mask` → `mask & mSlots` at both sites closes the two `and.`
charges. **Measured: NO CHANGE WHATSOEVER** — fuzzy `99.57746` before and after, identical to
five decimal places, and a re-diff of the edited build showed *the same three charges*.

⇒ **MSVC canonicalises commutative `and` operand order; the source spelling is discarded before
codegen.** Do not re-fund operand-order edits on commutative ops in this compiler.
A supporting control: `?CreateWidgetInstances@Gem@@` (472 B) and `?AddInstance@Gem@@` (1,704 B)
both contain `mSlots & 1 << i` and both sit at **fuzzy 100.0** — the `mSlots`-first spelling is
demonstrably not wrong in general.

## 6. Negative result #2 — hoisting the element REGRESSES the row 9.4 pp (`UpdateTailPositions`, 156 B)

Single charge: `lwzx r3, r11, r29` (target, pointer-first) vs `r3, r29, r11` (ours, index-first).

**Predicted:** ~25 % chance that hoisting `Tail *t = mTails[i];` reshapes the address expression.
**Measured: a clear regression** — fuzzy `99.7436 → 90.333336`, mpn `→ 92.25641`, charges
**1 → 14** (register allocation shifted across the whole loop: `subf`/`srawi` retargeted, plus an
insert/delete pair), costing **−1 matched function** whole-binary. Reverted.

★ **The control that explains why no spelling will work.** `?Hit@Gem@@`, `?Release@Gem@@` and
`?KillDuration@Gem@@` each contain exactly one `lwzx` over the identical `mTails[i]->X()`
construct, and in all three it is `lwzx r3, r29, r11` — **index-first, EQUAL on both sides**.
So retail itself emits index-first for this construct in three functions of this very TU and
pointer-first in two others. One source spelling, two retail orders ⇒ the operand order is a
downstream scheduling artifact of the surrounding expression, **not a spelling we can select**.
This is the permuter's domain, and the permuter is OFF by standing directive.

## 7. The two remaining NONRELOC rows are EH funclets and are NOT source-fixable

`fn_823CAA9C` (40 B): target `addi r3, r31, 0x70; bl fn_82308470` vs ours
`addi r3, r31, 0x60; bl ~Overlay@OutfitConfig`. `fn_822AB764` / `fn_822AB7B8` (40 B each):
`lwz r3, 0x54(r31)` vs `0x94(r31)`, plus a second reloc-name charge.

These are EH unwind funclets paired by **byte signature**, not by identity — the target funclet
destroys a *different* member (via an unnamed `fn_82308470`) at a different frame offset. The
offset delta is therefore not evidence of a struct-layout defect on our side, and chasing it
would be chasing an arbitrary pairing. ⚠ Do not read `0x70` vs `0x60` here as a layout oracle.

⇒ After removing these, the genuinely source-closable surface in this unit is **440 B**
(`PartialHit` + `UpdateTailPositions`), and §5–§6 show both are compiler-scheduling-bound.

## 8. The 18 STUB rows (5,276 B) — identification, not source

All 18 have `base_size = 0`: no compiled object in this unit defines a counterpart. They sit in
the `.text` blocks already pinned to this unit, clustered in three regions:

| region | rows | note |
|---|---:|---|
| `0x822Axxxx` (OutfitConfig cluster) | 8 | incl. the largest, `fn_822A5A38` @ 1,164 B |
| `0x82BAxxxx`–`0x82BBxxxx` (NowBar / GemRepTemplate cluster) | 7 | |
| `0x823Cxxxx` | 2 | |
| named anon-ns `__destroy_range_aux` ×2 | 2 | 96 B each, from another TU's anonymous namespace |

`Gem.cpp` is already a scatter-include hub (lanes AE and W16-CE added `OutfitConfig.cpp`,
`GemRepTemplate.cpp`, `NowBar.cpp` and a `__median` instantiation for exactly this reason), so
the mechanism to close these is known. ⚠ But per CLAUDE.md a scatter-include buys a **pairable**
row, not a matching one — the payout is only realised if the ported body then matches. Nothing
here was attempted: identifying `fn_822A5A38` requires map work, which this lane was scoped out of.

## 9. What would close each open row

- **1,288 B (10 ALIAS_MISSING rows, incl. `AddRep` 604 / `InitChordInfo` 188 / `~Gem` 96)** —
  a coordinator-level alias membership addition to an **existing** survivor group, gated on T1
  retail-byte proof. Not a lane action; `symbol_aliases.json` edits were out of scope here.
- **776 B (3 ALIAS_MISSING rows)** — ⛔ closed by standing withdrawal. Needs the withdrawal
  overturned on evidence, not a new alias.
- **5,876 B (23 NO_GROUP rows)** — needs a *new* fold group proven on retail bytes
  (relocation-normalised body hashing), a strictly larger decision than a membership addition.
- **440 B (`PartialHit`, `UpdateTailPositions`)** — permuter-class instruction scheduling.
  Evidence that would close them: a permuter run, or a demonstration that some source form
  changes `lwzx` operand order *without* perturbing register allocation. §5–§6 rule out the two
  obvious spellings.
- **5,276 B (18 STUB rows)** — identification of the retail bodies + scatter-include, then a
  body port. Start with `fn_822A5A38` (1,164 B, block `0x822A59D0-0x822A6024`, OutfitConfig cluster).

## 10. Deliberately not done

- No `symbol_aliases.json` edit (out of scope; and 3 of the candidates are withdrawn anyway).
- No `splits.txt` or map edits.
- No permuter (OFF by standing directive).
- No scatter-include attempt for the STUB rows — it needs map identification this lane
  was not scoped for, and a pairable-but-unmatched row buys nothing.

Native gate on the final tree:
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`
