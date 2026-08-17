# W19-VOCAL — VocalTrack + VocalPlayer: one win, two refuted spellings, and a sized wall

**2026-08-17.** Baseline verified in-worktree before any edit, reproducing the
brief **exactly**: `matched_functions` 44,485 · `matched_code` 3,747,732 B =
36.312897% · honest 21,585 · `total_code` 10,320,664 · `total_functions` 69,226,
ruler `name_check` (read from `report.json`'s own `provenance.diff_config`).

**Shipped: +732 B / Δfns 0 / Δhonest 0**, measured by `ab_measure --revert` of the
one functional commit. Tree with the fix: **36.319990%**.

---

## The brief's two headline sizes are the INSTRUCTION-DIVERGENT slice, not the prize

Reproduced exactly by an independent instrument: VocalTrack **18,572 B** (13 rows),
VocalPlayer **11,632 B** (8 rows) = bytes in rows carrying at least one
instruction-level diff. That is a real quantity, but it is **not** what
`matched_code` pays for, which is `fuzzy == 100` per row, all-or-nothing.

The residual decomposes three ways (self-validating — the three sum to each unit's
`total_code − matched_code`):

| unit | name-only (`mpn` 100, `fuzzy` < 100) | instruction-divergent | unpaired (`fuzzy` 0) |
|---|---|---|---|
| VocalTrack | 56 rows / 5,992 B | 15 rows / 18,652 B | 15 rows / 2,712 B |
| VocalPlayer | 12 rows / 3,060 B | 22 rows / 12,192 B | 16 rows / 2,876 B |

## ★ The instrument correction that decided the whole lane

objdiff's `reloc_eq` **forgives a placeholder target name** (`fn_` / `lbl_` /
`jumptable_` / `_bss_` / …; `is_placeholder_symbol_name`, objdiff-core
`diff/code.rs:915`). A site whose *only* differing argument is such a symbol
therefore **never reaches `diff_arg` at all**. So:

* `arg:{Register,Symbol}` ⇒ charged by the **register**; the symbol is incidental.
* `arg:{Symbol}` alone ⇒ a genuine, non-forgiven **name charge**.

A naive "count instructions whose Symbol args differ" reads **138 name charges**
on `?Handle@VocalPlayer@@` and files the row as fold-alias-walled. The true count
is **ZERO** — all 138 are one uniform `r29`↔`r28` displacement against forgiven
`lbl_82E035xx` slots (the `HANDLE_*` macro's function-local `static Symbol _hs`).
That is the difference between "unfixable by construction" and "the largest
source-collectable row in either unit". Encoded in `tools/w19_charge_census.py`.

Corrected split — rows whose prize is collectable by **source work alone**:

| unit | collectable | also needs a name/map fix |
|---|---|---|
| VocalTrack | 7 rows / **3,592 B** | 21,052 B |
| VocalPlayer | 16 rows / **6,124 B** | 9,128 B |

⚠ Also re-confirmed the EB-4 trap the hard way: `objdiff-cli diff`'s
`normalized_match_percent` is the **relocation**-normalized percent (== report
`fuzzy`), **not** report's arg-blind `match_percent_normalized`. `diff` never
emits `mpn`. Read `mpn` from `report.json` or you will compute a "certificate"
that is uniformly vacuous.

## The win: UpdateLyricZ 99.945 → 100.000 (+732 B)

One charged site: retail `fadds f0, f0, f31`, ours `fadds f0, f31, f0`.

**What settled it was a two-half asymmetry, not the oracle.** The function has two
structurally identical halves (lead / harmony), each ending in
`...DirtyLocalXfm().v.z += delta`. Retail emits a **different operand order in
each** — lead `f0, f31`, harmony `f31, f0`. Identical source cannot produce two
different orders, so retail's lead-half source is *not* the `+=` form. Our
symmetric `+=` was right for harmony and wrong for lead.

⇒ **Mechanism, proved here:** MSVC emits a commutative `a + b` as
`fadds fD, fb, fa` — **reversed**. So `delta + z` is what yields
`fadds fD, f(z), f(delta)`.

★ **Negative result worth more than the fix.** The obvious spelling

```cpp
Transform &xfm = plate->mText->DirtyLocalXfm();
xfm.v.z = delta + xfm.v.z;
```

**flipped the `fadds` correctly and still scored WORSE — 99.94536 → 99.39891.** It
cost an extra `addi r11, r31, 0x1c` to materialize the `Transform&` base, *and* it
perturbed regalloc enough to flip the **untouched harmony half's** `fadds` from
matching to mismatching. Binding a `float&` to the member needs no second base
pointer and leaves harmony alone:

```cpp
float &leadZ = plate->mText->DirtyLocalXfm().v.z;
leadZ = delta + leadZ;
```

⇒ fuzzy **100.00000**, 0 charged sites. **Getting the arithmetic right is not
sufficient — the reference you bind is itself codegen.**

### Predicted vs measured

| | predicted | measured |
|---|---|---|
| Δ`matched_code` (revert) | −732 B | **−732 B** ✓ exact |
| Δ`matched_functions` | 0 (row already `mpn` 100) | **+0** ✓ |
| Δhonest | 0 | **+0** ✓ |
| `none` control | also ≈ −732 (register-class, not a reloc name) | **−732** ✓ |

## Two spellings REFUTED — do not retry

**`GetHarmonyScore` (228 B, one `lwzx r3,r11,r30` vs `r3,r30,r11`).** laneBF-3 had
tested `*(begin() + part)` — which is literally `operator[]`'s own spelling and so
could not have changed anything. The untested direction was the reverse, and it
was worth trying because the commutative reversal above is *real*. Tested
`*(part + mPlayer->mVocalParts.begin())`: predicted fuzzy 100, **measured
99.82456, completely unchanged**. ⇒ **the FP-operand reversal does NOT generalise
to integer indexed loads**; MSVC canonicalizes `lwzx` rA/rB independently of
source order. BF-3's regalloc verdict now stands on two refuted spellings instead
of one vacuous one.

**`Poll@VocalTrack` (512 B).** Retail computes `InRollback()`'s inlined result in a
scratch `r11` and copies it into `gamebool`'s `r30` with a redundant
`clrlwi r30, r11, 24`; we assign `r30` directly in both arms and MSVC elides the
mask. **Our output is one instruction SHORTER than retail's and otherwise
identical.** Tried the source shape that literally mirrors retail's asm
(`bool g = true; if (!TheGame->InRollback()) g = false;`): **measured 97.617 →
95.195**, because applying `!` to an already-bool value makes MSVC negate it
*arithmetically* (`subfic`/`subfe`/`and`), costing 4 instructions to buy 1.
Verified `r30` is genuinely long-lived (re-read at 0x194), so the named local is
real. ⇒ failure-to-coalesce **in retail**; not source-addressable.

## The wall: VocalPlayer is gated behind two 0x10 frame shortfalls

Not a fold-alias wall and not a name wall — a **frame-size** wall, and it is
shared, sized, and has one clean root cause per function.

| function | retail frame | ours | spill directive | dependent EH funclets |
|---|---|---|---|---|
| `?Handle@VocalPlayer@@` | `stwu r1,-0xf0` | `-0xe0` | both `__savegprlr_24` | 7 × 40 B |
| `?Poll@VocalPlayer@@` | `stwu r1,-0x1d0` | `-0x1c0` | retail **`_14`** vs ours **`_15`** | 4 × 40 B |

The funclets are the corroboration: 11 of the 40 B `fn_826E9xxx` / `fn_826EBDxx`
rows differ **only** in `subi r31, r12, 0x<parent frame>` — `0xf0` vs `0xe0` and
`0x1d0` vs `0x1c0`. They are not independent rows; they are the same defect
observed 11 more times.

* **`Poll`** — retail saves **one more callee-saved GPR** (r14). It holds one more
  long-lived value than our source produces. That single extra register drives the
  0x10 frame *and* the `r14`/`r15`-onward displacement.
* **`Handle`** — **same** spill count, so the 0x10 is pure locals: retail gives the
  `ButtonUpMsg` temp its own slot (`0x88`/`0x8c`) where we overlay it at
  `0x58`/`0x5c`. Root cause is already recorded in-source at
  `src/band3/game/VocalPlayer.cpp:1570` — `OnMsg(const ButtonUpMsg&)` is a
  **deliberate stub** because `HandleDeactivateVolume`'s helpers are not ported,
  and a prior lane already had to add `#pragma auto_inline(off)` to restore
  retail's call shape. All 15 of `Handle`'s hard diffs sit in **one ~60-instruction
  window (1116–1173)** out of 1,240 — the ButtonDown/ButtonUp dispatch — where
  retail also hoists `&Message::vtable` into `r30` once instead of recomputing
  `lis`/`addi` per site.

**Size of the cluster: ~8,764 B** (Handle 4,936 + 7×40, Poll 3,388 + 4×40), though
`Poll` additionally carries 10 real name charges so it is not purely collectable.

⛔ **And `Handle`'s prize is NOT collectable by closing its instructions.** `mpn`
excludes arg-only penalties, so fixing all 15 hard diffs takes `mpn` 98.88 → 100
(**+1 function**) and leaves `fuzzy` at ~98 ⇒ **exactly 0 bytes**. The bytes
require the `r28`↔`r29` displacement (212 sites) to go, which is downstream of the
frame. This is the RESIDUAL-1 trap in a fresh instance: **price the row from the
charged-site list, not from the mismatch count.**

### What it would take

Port `HandleDeactivateVolume` and its helpers so `OnMsg(const ButtonUpMsg&)` stops
being a stub, restoring the temp that `/Ob2` currently deletes. If the frame then
matches, the register displacement plausibly dissolves with it (CLAUDE.md records
12 instances of exactly that). If it does **not** dissolve, the remainder is
permuter-class and therefore out of scope by standing directive. Separately, find
the extra long-lived value in `Poll` that costs retail its 18th callee-saved GPR.
**Neither was attempted here** — both are body-porting jobs, not spelling jobs.

## Deliberately NOT done

* **No alias added to `symbol_aliases.json`, no map edit.** The name-charged strata
  (21,052 B in VocalTrack, 9,128 B in VocalPlayer) are largely ICF fold-alias
  candidates — e.g. `_S_sort<Symbol>`'s 4 sites are retail's
  `~_List_base<MidiParser*>` against our `~list<Symbol>`, which genuinely fold for
  a 4-byte POD element. An unproven alias lifts the score **by construction** and
  the `none` control **cannot** catch a fabrication. Proving them on retail bytes
  is a separate lane.
* **`UpdateScrolling` (8,948 B, fuzzy 72.25, 618 hard diffs) not opened.** It is
  the single largest row in either unit and it is a body rewrite, not a spelling
  fix; it also carries 12 real name charges, so source work alone cannot cross it.
* **The 15 + 16 `fuzzy == 0` rows (5,588 B) not touched.** Those are unpaired
  anonymous rows; naming buys a pairable row at 0% with no content.
* **Permuter not run** (standing directive), so every row whose residual is a pure
  register permutation — `GetHarmonyScore`, `Poll@VocalTrack`,
  `ProcessStaticLyrics` — is left as diagnosed, not as attempted.
