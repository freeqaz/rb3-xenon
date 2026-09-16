# W16-FU — `BandSongMetadata` 4-arg ctor: two systematic defects, both decoded from retail bytes

Lane W16-FU, 2026-09-16. Base: `main` @ `44213c04`, ruler `name_check` (graded).
Baseline asserted in-worktree and byte-identical to main: `matched_functions` 44040 ·
`matched_code` 4150676 · code% 40.50599 · fuzzy 50.380030 · `total_code` 10247068 ·
`total_functions` 69240 · `masked_equal` 23246.

Target row: `??0BandSongMetadata@@QAA@PAVDataArray@@0_NPAVBandSongMgr@@@Z`,
unit `default/BandSongMetadata`, **7,948 B**, fuzzy **42.4826**, mpn **43.272774**.
Second-largest partially-matched named row in the binary.

## 0. Pre-registration (written BEFORE the edit and BEFORE any measurement)

Diagnosis: the row is **not** an edge grind. `objdiff-cli diff` (no `--build`, patched
tree) reports **Target Size 7,948 B vs Base Size 3,656 B** — retail's body is **2.17x
ours** — and of 1,999 aligned rows, **1,085 (54.3%) are `delete`** (target-only).
Frame `-0x280` vs ours `-0x220`.

Two independent systematic causes identified (evidence in §1 and §2):

- **D1 — missing feature.** Retail carries a ~700-instruction UGC *sub-genre remapping*
  block between the `genre` and `anim_tempo` parses. **Neither oracle has it**
  (rb3-Wii line 72 is a bare `mGenre = member_arr->Sym(1);`; DC3 has no
  `BandSongMetadata` at all).
- **D2 — wrong symbol storage class.** Every `FIND_WITH_BACKUP(sym)` symbol is a
  **function-local `static Symbol`** in retail; our source uses the `utl/Symbols.h`
  **global** (`?name@@3VSymbol@@A`). ~33 sites, ~4-9 instructions each.

### Predicted deltas

| measure | now | predicted after |
|---|---|---|
| base body size | 3,656 B | **7,500-8,300 B** |
| `delete` rows | 1,085 | **< 150** |
| row fuzzy | 42.4826 | **>= 85** (100 not expected first pass: ~80 statics + 2,000 instrs of regalloc) |
| whole-binary `matched_code` | 4,150,676 | **>= 0** (row pays 0 until fuzzy==100; gain only if it crosses) |

### Falsifiers (each would refute a named claim)

1. **Base size still < 6,500 B** after the change ⇒ a **third** defect exists that this
   analysis did not find. Refutes "D1+D2 explain the body".
2. **fuzzy DROPS below 42.4826** ⇒ D2 is wrong — retail's statics are not per-call-site
   function-local statics, and I have mis-read the guard-bit dance.
3. **`delete` count stays above ~400** ⇒ the reconstruction of the UGC block is
   structurally wrong (wrong nesting or wrong branch fan-out), not merely mis-ordered.
4. **Whole-binary A/B net negative** ⇒ the change damages other rows in the TU
   (most plausibly via `.bss`/guard-variable churn or the `matched_data_percent 100.0`
   the unit currently holds).
5. **fuzzy lands in [85,100)** ⇒ D1+D2 are right and the residue is regalloc/scheduling;
   the row still pays **exactly 0 bytes** and must be reported as a partial, not a win.

Explicitly: outcome 5 is the one I consider most likely, and it is a **0-byte** outcome.

---

## §5 Pre-registration #2 — the two commutative `add` sites (written BEFORE the build)

After D1/D2/D3 plus three shape fixes the row sits at **fuzzy 99.98742 / mpn 99.99748**
with exactly **three** charged sites (priced off `report.json`, not off a mismatch
count):

| idx | target | ours | class |
|---|---|---|---|
| 47 | `bl ??0?$hash_map@VSymbol@@H…` | `bl ??0?$hash_map@VSymbol@@M…` | `diff_arg` (ICF fold-alias) |
| 1728 | `add r3, r11, r30` | `add r3, r30, r11` | `diff_arg` (commutative operand order) |
| 1767 | `add r3, r30, r11` | `add r3, r11, r30` | `diff_arg` (commutative operand order) |

**The finding that motivates the experiment.** Retail's `real_guitar_tuning` and
`real_bass_tuning` loops are **instruction-for-instruction identical** across their
whole 13-instruction bodies, differing in exactly three words: the array base
(`addi r28, r24, 0xf8` vs `0x110`), the loop bound (`cmpwi cr6, r30, 0x30` vs `0x20`),
and the commutative operand order (`add r3, r11, r30` vs `add r3, r30, r11`). Same
register allocation, same schedule, same callees. **Identical source cannot produce
two different operand orders via a source difference**, so retail's own two loops are
separated by a compiler-internal tie-break. Our build makes the same arbitrary choice
in both loops — just landing the opposite way round in each, i.e. we are the exact
mirror of retail at both sites.

**Experiment.** Change the guitar loop ONLY, from a named temp to the direct spelling
`mRealGuitarTuning[i] = member_arr->Array(1)->Int(i);` — semantically identical, a
plausible retail spelling rather than an arbitrary wedge, and it perturbs temp
numbering without changing the emitted call sequence.

**Predictions.**
- **P1** — the guitar site (1728) closes ⇒ charged sites 3 → 2.
- **P2** — whole-binary `matched_code` is **unchanged** and this row still pays
  **exactly 0 B**, because site 47 (the alias) remains and `matched_code` keys on
  `fuzzy == 100`. *Fuzzy movement is not bytes.*

**Falsifiers** (the set is deliberately able to express outcomes I do not expect):
- **F1 (inert)** — charged sites stay at 3. ⇒ the operand order is not reachable from
  this source lever; the two adds are permuter-class and get a priced refusal.
- **F2 (worse)** — fuzzy drops / new charged sites appear. ⇒ the direct spelling is
  wrong; revert it.
- **F3 (coupled)** — BOTH adds flip together off a change made to only ONE loop.
  ⇒ the tie is one shared parity bit, not a per-loop property. This would be the most
  informative outcome and I do **not** predict it.
- **F4 (collateral)** — whole-binary `matched_code` **drops**. ⇒ the perturbation moved
  other rows in the TU; revert regardless of what it did to this row.

### §5.1 Result — **F3 fired. The prediction I named as least likely is what happened.**

| measure | before | after |
|---|---|---|
| row `fuzzy` | 99.98742 | **99.99748** |
| row `mpn` | 99.99748 | 99.99748 |
| charged sites | **3** | **1** |
| WB `matched_code` | 4,152,028 | **4,152,028** (unchanged) |
| WB `matched_functions` | 44,082 | 44,082 (unchanged) |
| unit `default/BandSongMetadata` fuzzy | 82.71895 | 82.9212 |

**P1 was too weak and F3 is the truth.** I changed the *guitar* loop only and **both**
`add` sites (1728 *and* 1767) closed. The bass loop's source was not touched at all, yet
its operand order flipped too. ⇒ the commutative-operand tie in these two loops is **one
shared compiler-internal parity bit**, not an independent per-loop choice. That also
retro-explains the puzzle in §5: retail's two loops disagree *with each other* because the
bit flips once between them, and our build was simply carrying the opposite phase — so a
single perturbation anywhere in the phase chain re-phases both loops at once.

**P2 held exactly**: `matched_code` did not move a byte. The row is still sub-100, so it
still pays **exactly 0 B**. F1, F2 and F4 did not fire (no inertness, no regression, no
collateral).

★ The transferable lesson: **two charged sites that are mirror images of each other may be
ONE degree of freedom, not two.** Pricing them as two independent grinds — or declaring
them permuter-bound because "identical source cannot differ" — would both have been wrong.
The cheap discriminator is to perturb **one** of the pair and see whether the other moves.

---

## §6 The last charged site — the `hash_map<Symbol,float>` ctor fold (pre-registered)

One charged site remains, and it is the entire distance to 7,948 B:

```
 47  bl ??0?$hash_map@VSymbol@@H…  (target)   vs   bl ??0?$hash_map@VSymbol@@M…  (ours)
```

### §6.1 Adjudication on retail bytes (NOT on the metric)

Two rival hypotheses, and only one of them is a fold:

1. **Fold** — `mRanks` really is `hash_map<Symbol,float>` and its ctor folded with the
   int-valued one; the map simply carries the arbitrary survivor name.
2. **Source defect** — `mRanks` is genuinely `hash_map<Symbol,int>` in retail and our
   header's `float` is wrong. *An alias here would forgive a real bug.*

**Hypothesis 2 is refuted on retail bytes.** In retail's own ctor the member at `+0x9c`
is read back as a float:

```
addi r3, r24, 0x9c        ; &mRanks
addi r4, r31, 0xbc
bl   fn_823658F8          ; hash_map<Symbol,?>::operator[]
stfs f31, 0x0(r3)         ; FLOAT store into the returned node reference
```

**The map contradicts itself on this one member, which is the fold's signature.**
`target_symbol_map.json` names that same member's `operator[]` at `0x823658f8` with the
**float** spelling (`??A?$hash_map@VSymbol@@M…`) while naming its ctor at `0x8255d480`
with the **int** spelling. Both cannot be the member's true value type; the `stfs`
settles it for float, so the ctor's `H` name is fold-arbitrary rather than a type fact.

**Three independent witnesses to the fold.** Retail's `BandSongMetadata` ctor calls
`fn_8255D480` for this float map; retail's `Tour` ctor calls *the same single address*
for its `TourProperty*` and `TourDesc*` maps (already T1 in group 1656, lane W16-BU). One
body, three distinct value types.

**Anti-vacuity.** `hash_map<Symbol,X>::hash_map()` does **not** universally fold — retail
holds three distinct bodies (`0x8255d480`, `0x825af9e8` = `DataArray*`, `0x82278a20` =
`vector<PatchSticker*>`). This claims membership for ONE further spelling on a retail
call-site observation, not a class-wide rule. **No contradiction**: the float spelling is
at no address in `target_symbol_map.json`.

**Our side (corroboration only, not the primary evidence).** The two ctor COMDATs are
byte-identical (76 B, one type-6 reloc at `+0x30`), and their inner `_M_initialize_buckets`
pair is byte-identical with **all three** relocation targets identical. The residual
naming difference up the chain is EH metadata (`__ehfuncinfo$`, `__unwind$N`, `$T`/`$M`
numbering) and per-`T` slist dtor names.

### §6.2 Predictions and falsifiers

- **P1** — the row reaches `fuzzy == 100.0` and banks **7,948 B**.
- **P2** — `Δmatched_functions` **+1** (mpn 99.99748 → 100).
- **P3** — `Δcode_bytes` is **at least** +7,948 B; more is possible and legitimate if
  other rows elsewhere construct the same map.
- **F1 (inert)** — `Δcode_bytes == 0`. ⇒ the rendered `icf_aliases.map` did not refresh;
  a tooling problem to investigate, **not** evidence the fold is wrong.
- **F2 (over-broad)** — the delta lands largely in units with no `hash_map<Symbol,float>`
  construction. ⇒ the group is forgiving unrelated divergences; **withdraw it**.
- **F3 (guard fires)** — `ab_measure` reports **ALIAS_SUSPECT** on this map-only leg.
  ⚠ This is **EXPECTED and is NOT a clearance either way**: per CLAUDE.md a fabricated
  alias lifts `name_check` *by construction* and reads flat on `none`, so the metric
  cannot adjudicate this. It is settled in §6.1 on retail bytes or not at all.
- **F4 (collateral)** — any unit falls off 100%, or `Δmatched_functions` is negative.

**This is deliberately a SEPARATE commit from the source work**, so the alias faces a
map-only A/B and its own `ALIAS_SUSPECT` guard rather than hiding inside a mixed patch.

### §6.3 Result — measured map-only, `ALIAS_SUSPECT` fired as predicted

```
patch kinds: ['map']
leg A: matched=44082 masked=23288 honest=20794 code%=40.519180
leg B: matched=44086 masked=23288 honest=20798 code%=40.600452
Δmatched=+4  Δmasked_equal=+0  Δhonest=+4  Δcode%=+0.081272pp  Δcode_bytes=+8328
units at 100%: 192 -> 193 (default/band3/tour/TourWeightManager, MATCHED_ROSE)
[control none] Δmatched_code=+0 B — ALIAS_SUSPECT
```

**P1, P2, P3 all held**: the row reached `fuzzy == 100.0` and banked its **7,948 B**;
`Δmatched_functions` was +4 (≥ the predicted +1); `Δcode_bytes` +8,328 ≥ 7,948.
**Δhonest = +4** — these are honest matches, not `masked_equal` disclosures.

**F3 fired, exactly as pre-registered.** `ab_measure` raised `ALIAS_SUSPECT`: default
ruler up +8,328 B with `none` flat, on a map-only patch. ⚠ **That alert is expected here
and settles nothing in either direction** — CLAUDE.md is explicit that a fabricated alias
produces precisely this shape *by construction*, so the metric is structurally incapable
of adjudicating it. The evidence is §6.1, on retail bytes, or it is nothing. I am
reporting the alert rather than burying it.

**F2 (over-broad) is refuted by naming every moved row.** The +8,328 B is exactly four
rows and **zero rows fell off**:

| bytes | unit | row |
|---:|---|---|
| 7,948 | `default/BandSongMetadata` | `??0BandSongMetadata@@QAA@PAVDataArray@@0_NPAVBandSongMgr@@@Z` |
| 220 | `default/BandSongMetadata` | `??0BandSongMetadata@@QAA@PAVBandSongMgr@@@Z` |
| 96 | `default/band3/tour/TourPropertyCollection` | `??0TourPropertyCollection@@QAA@XZ` |
| 64 | `default/band3/tour/TourWeightManager` | `??0TourWeightManager@@QAA@XZ` |

All four are constructors of a `std::hash_map<Symbol, float>` member, checked in the
headers rather than assumed — and `TourWeightManager.h` / `TourPropertyCollection.h`
record that **earlier lanes had already adjudicated those members as float-valued on
retail bytes**. So three lanes independently identified the same container type and all
three were left one charged relocation-name site short by the same missing fold
membership. That convergence is independent corroboration: a fabricated alias would not
land on exactly the population two other lanes had already proven by other means.

F1 (inert) and F4 (collateral) did not fire.

---

## §7 Net result

| | value |
|---|---|
| row `??0BandSongMetadata@@QAA@PAVDataArray@@0_NPAVBandSongMgr@@@Z` | **fuzzy 42.4826 → 100.0**, banks **7,948 B** |
| body size | 3,656 B → **7,948 B** (target 7,948 B) |
| whole binary, both commits | `matched_functions` 44,040 → **44,086** (+46) |
| | `matched_code` 4,150,676 → **4,160,356** (+9,680 B) |
| | `matched_code_percent` 40.505990 → **40.600452** (+0.094462 pp) |
| | `fuzzy_match_percent` 50.380030 → **50.436836** |
| | `honest` 20,794 → 20,798 (+4) |
| units at 100% | 192 → 193 (`default/band3/tour/TourWeightManager`) |

Split across the two commits: **source +42 fns / +1,352 B** (all of it *other* rows in
the unit — the target row paid 0 until the last site closed), **alias +4 fns / +8,328 B**.

★ Note the economics this makes concrete: the source commit did 99.5% of the *work* and
banked **none** of the target row's bytes, because `matched_code` is all-or-nothing per
row. A lane that had stopped at "fuzzy 99.99748, one charged site left" would have
reported a 0-byte result for a row that was in fact one map entry from 7,948 B.

## §8 What I did NOT do, and why

- **I did not edit `BandSongMetadata.h`.** ⚠ Stated prominently because the brief flagged
  it: the header is referenced from `VocalTrack.cpp` (lane W16-FS) and `GemManager.cpp`,
  so an edit would cascade at merge time. **No cascade exists — the header is untouched.**
  The `hash_map<Symbol,float> mRanks` declaration it already carried turned out to be
  *correct*, which is why the last site was an alias and not a header fix.
- **I did not run the permuter** (standing directive: OFF). It would have been the wrong
  instrument anyway — the two commutative `add` sites turned out to be one shared parity
  bit reachable from ordinary source spelling, not a register-allocation search problem.
- **I did not widen the alias group beyond one spelling.** `hash_map<Symbol,X>::hash_map()`
  demonstrably does *not* universally fold (retail holds three distinct bodies). Adding
  the other two spellings would have been unearned and is exactly the "forgiveness"
  failure mode the alias machinery exists to guard against.
- **I did not re-home or re-pin anything**, and did not touch `splits.txt` or
  `target_symbol_map.json`. In particular I did **not** "fix" the map's `H` name on
  `0x8255d480` to the float spelling: the name is fold-arbitrary among the group's
  members, renaming it would break the rows that legitimately spell it `H`, and group
  1656 already encodes the survivor relationship correctly.
- **I did not chase the our-side fold recursion to its base case.** It bottoms out in EH
  metadata symbol *names* (`__ehfuncinfo$`, `__unwind$N`, `$T`/`$M` numbering) and per-`T`
  slist dtor names. That work would only have corroborated a conclusion already settled
  directly on retail bytes, and the retail observation outranks our-side inference.
- **I did not treat `run_objdiff`'s mismatch count as the price.** Every figure here is
  read from `report.json`; the charged-site lists are from the graded (`name_check`)
  ruler, labelled as such by the tool.
- **The stale comment in `BandSongMetadata.h`** claiming the retail DataArray ctor is
  `fn_82584A08` is **wrong** — the real address is **`0x825A0B28`** per
  `scripts/target_symbol_map.json`. I left it alone rather than touch the header under the
  scope fence; it should be corrected by whoever next owns that file.

## §9 What would reopen this

The only claim here that rests on judgement rather than arithmetic is the fold membership
in §6.1. It would be **withdrawn** if either of these turned up:

- a retail call site that constructs a `hash_map<Symbol,float>` by calling an address
  *other than* `0x8255d480`, which would mean the float spelling is not a member of that
  fold group; or
- evidence that `BandSongMetadata+0x9c` is not the member our source calls `mRanks` —
  i.e. that the `stfs` I read belongs to a different field.

Both are checkable on retail bytes without re-running anything.
