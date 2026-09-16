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
