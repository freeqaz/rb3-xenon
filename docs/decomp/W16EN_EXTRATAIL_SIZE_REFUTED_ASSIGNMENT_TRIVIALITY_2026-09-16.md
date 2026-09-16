# W16-EN — `TrainerGemTab::ExtraTail`: the element size was never wrong; the *assignment operator* was

**Date:** 2026-09-16 · **Branch:** `w16-en` · **Base:** `391740f3`
**Measured:** `Δmatched=+1 · Δcode_bytes=+656 · Δcode%=+0.006400pp · Δhonest=+1`, 0 regressions.

## 1. The handoff claim, and its refutation

W16-EJ §6 listed `band3/game/TrainerGemTab` as a live **fold-REFUTED** pair:

> retail's `_M_erase<vector<RndLine::Point>>` is **96 B**; our
> `_M_erase<vector<TrainerGemTab::ExtraTail>>` is **88 B** ⇒ **element size wrong**

The two sizes are correct. **The inference from them is not.** Our element size is
exactly right, and retail's own instruction stream says so.

**Three independent legs, all on retail bytes:**

| leg | evidence |
|---|---|
| retail's own stride | retail's body at `0x8247b020` loads `li r10, 0x48` / `li r5, 0x48` and divides by it (`divw. r11, r11, r10`). Retail's element is **72 B**. |
| compiler layout | `class_layout_report.py ExtraTail --tu src/band3/game/TrainerGemTab.cpp` ⇒ `sizeof = 72 (0x48)`; same tool ⇒ `RndLine::Point` `sizeof = 72 (0x48)`. **Identical.** |
| a byte-exact sibling | `??$__uninitialized_copy@PAVExtraTail@TrainerGemTab@@…` — 84 B, **fuzzy 100.00000**. It is instantiated on the element type and encodes its stride. It could not match if the size were wrong. |

A fourth, for the member offsets specifically: `?DrawExtraTails@TrainerGemTab@@QAAXXZ`
(200 B) is **fuzzy 100.00000**, and it indexes `unk130[i].mXfm` / `.mSlot` / `.mIsRGChord`.
Size *and* offsets are confirmed.

⇒ **"element size wrong" is REFUTED.** Do not re-open it.

## 2. Where the claim came from — and why it was an honest misreading

The claim is a faithful copy of a **withdrawal record already in the tree**,
`scripts/symbol_aliases.json` group `@ 0x8247b020` (survivor = the `Point` erase):

```
class: SURVIVOR_SIZE_MISMATCH
why:   "our(S)=96 B vs our(F)=88 B [retail(S)=96] -- our own build gives them
        different-sized COMDATs"
note:  "Refuted WITHIN OUR BUILD ... Do NOT re-add."
```

Read closely, that record is a statement about **our build**, not retail's — it even
records `retail(S)=96`. Retail was always 96; **the 88 was always ours.** The withdrawal
was arithmetically correct (different-sized COMDATs cannot fold under `/OPT:ICF`) and its
*cause* was mis-attributed downstream to element size.

★ The durable lesson: **a size disagreement between our COMDAT and retail's is not
evidence about a data type.** It is evidence about *our codegen*, and the type is only one
of several possible causes.

## 3. The actual mechanism

`_M_erase(first,last,__false_type)` calls STLport's generic `__copy`. Retail **inlines**
it (a per-element `memcpy(dst,src,0x48)` loop, 96 B). We emitted `bl __copy<ExtraTail*>`
out of line (88 B).

Why the inliner diverged:

* `Transform::operator=` (`math/Mtx.h:245`) **and** `Hmx::Matrix3::operator=` (`:89`) are
  **user-declared** (each a `memcpy` shim).
* So `ExtraTail`'s implicit copy-assign is **memberwise**: a `0x40` memcpy **plus** two
  scalar stores — not one whole-object memcpy.
* `RndLine::Point` (Vector3 + Color + `int[10]`, no user `operator=`) is trivially
  assignable, so `*dst = *src` is a single `memcpy(…,0x48)`.
* A fatter loop body is what makes MSVC `/O1 /Ob2` decline to inline `__copy`.

**Fix (local, `src/band3/game/TrainerGemTab.h` only):** give `ExtraTail` an explicit
`operator=` doing `memcpy(this,&t,sizeof(*this))` — the same idiom `Transform` itself uses.
Semantically identical (it additionally copies the 3 tail padding bytes, unobservable).

**Result:** our body is now **96 B and instruction-identical** to the survivor's —
same `bl __savegprlr_27`, `li r10,0x48`, `divw.`, `bl memcpy` loop, `b __restgprlr_27`.

Corroboration that these two element types are fold-twins was **already in the tree**:
`__uninitialized_copy` (survivor `ExtraTail`, `Point` folded) and `__uninitialized_fill_n`
both already fold these same two types. `_M_erase` was the outlier, and it was ours.

## 4. Crossing the row required BOTH halves

objdiff pairs by **name**. Byte-identity alone moved nothing: `?Draw@TrainerGemTab@@QAAXH@Z`
(656 B) stayed at fuzzy 99.96951 with its single `diff_arg` intact after the source fix.
The row crossed only once the ICF membership was re-admitted.

**Re-adjudicated before re-admission** — `tools/icf_pair_adjudicate.py`:

```
FLAT T1 : PROVEN
  retail_size 96 · our_size 96 · reloc_tally {} · n_relocs 3
  survivor_map_resident True
```

and the same tool's `--selftest` **PASSES with a REFUTED negative control**, so this is a
verdict from an instrument shown able to fail — not a gate that cannot fail.

**Re-admission used the sanctioned route, not a quiet list-flip.** The withdrawal record is
**kept** (history + denylist intact); `scripts/alias_withdrawal_overrides.json` names it
explicitly. `load_overrides` requires `overrides_class` to equal the ledger record's class,
so the override is impossible to write without having read the record it overrides — and it
was accepted by the real loader, which is itself the proof the class matched.

⚠ **This override is valid only while `ExtraTail::operator=` exists.** If that is reverted,
the T1 gate refuses the pair on its own, independently of the override.

## 5. Measurement

`tools/ab_measure.py --from-dirty`, both legs at a split fixed point, leg B `msvc=16`,
`renamer_patched=1830`:

```
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.006400pp  Δcode_bytes=+656
unit improvements: +1  default/band3/game/TrainerGemTab (10->11)
unit net (ALL units) = +1  vs whole-binary Δmatched = +1
units at 100%: 189 -> 189 (0 fell off), all-rows-fuzzy 169 -> 169
```

Pre-registered prediction was **+656 B / +1 fn**. **Hit exactly.**

⚠ The `none` control is flat (+0 B vs +656 default). `ab_measure` itself labels this
**NOT_APPLICABLE** for alias adjudication: with `source` in the patch, default-UP/none-FLAT
is *also* the wrong-callee-fix signature. Do not read the flat `none` leg here as either
confirmation or refutation of the alias.

`tools/icf_alias_finder.py --validate`: **PASS — 1658 groups, 0 CONTRADICTED.**

## 6. Handoff: `?DrawTails@TrainerGemTab@@QAAXABVGameGem@@HHMM@Z` (888 B) — analysed, NOT attempted

Still 888 B @ fuzzy 98.58108 / mpn 99.27928, **19 charged sites**. I did not touch it, and
deliberately: `matched_code` is all-or-nothing per row, so closing 1 of 19 buys **0 bytes**,
and a speculative edit would have added noise to the A/B above. What I found, for whoever
takes it:

1. **An FMA-contraction difference (1 `replace` + 1 `delete`, and our body is 4 B SHORT).**
   Target: `fmuls f11, f0, f26` then `fadds f12, f12, f11` — **not contracted**.
   Ours: `fmadds f12, f0, f26, f12` — contracted. This is the
   `float scaleX10 = 10.0f * scale; float endZ = xfm.v.z + scaleX10;` pair.
2. **Three operand-order flips**: target `fmuls fN, f31, fN`, ours `fmuls fN, fN, f31`
   (indices 176/178/180, storing to `0xc0/0xc4/0xc8`). Ours is `x *= s`; retail looks like
   `x = s * x`. Cheap to test.
3. **~13 FPR-numbering shifts** (indices 40–61) around the `SetFrame(...)` expression —
   a rotation (`f13/f12/f11` vs `f12/f11/f13`). Per CLAUDE.md a register swap is a
   *symptom*; the likely root is the shape of that expression. Note rb3-Wii hoists
   `yRange` / `tickRange` into locals where our source inlines both subexpressions.

## 7. What I did NOT do

* **Did not touch `Transform` or `Hmx::Matrix3`.** Removing their `operator=` is the
  "root" fix, but the blast radius is the whole engine for a 656 B row. The local
  `ExtraTail::operator=` is a compromise and is labelled as one in the header comment.
* **Did not attempt `DrawTails`** (§6) — 19 charges, 0 bytes for a partial close.
* **Did not touch the other 10 sub-100 rows** in the unit: `fn_826F0170` (776 B),
  `fn_826EECE0` (96 B), `?Render@` (88 B) and six 8-byte `fn_826EEC*` stubs are all at
  fuzzy 0 and are a different (identification/body) problem.
* **Did not run `icf_alias_build.py` to regenerate** the whole alias file — a 1658-group
  regeneration is a campaign-scale diff, not a lane's. The membership is re-derivable
  (flat T1 is the tier the generator implements) and the override file is in place for
  when someone does regenerate.

## 8. Corrected in passing

`TrainerGemTab.h` carried `// size 0x38` on `ExtraTail`. The compiler says **0x48**; the
`0x38` is inherited from the rb3-Wii header, where `Transform` is smaller. Comment fixed —
per CLAUDE.md, `// 0xHEX` comments are derived and can be wrong; the compiler is
authoritative.

---

## 9. Addendum — the fold survives a whole-image census, and the census corrects W16-EP

The coordinator blocked the alias half on a fair challenge: my admission rested on
`FLAT T1 PROVEN, 96/96, reloc_tally {}`, and `retail_bodytwins` counts only
**pinned** objs, so it cannot see an unpinned retail copy. W16-EL and W16-EP both
established that **"byte-identical ⇒ folded" is false.** Settled with a new
instrument, `tools/retail_body_multiplicity.py`.

**Answer: exactly 1.** Three legs:

| probe | result |
|---|---|
| every `.text` function extent of size 0x60 (1,002 of them) | **1** masked-equal body — itself |
| whole-image classes, all sizes (69,056 extents, 0 dropped) | survivor's class has **1** member |
| **slide**: every 4-byte-aligned `.text` window, symbol boundaries ignored (48,164 candidates by first word) | **1** masked-equal window |

The slide leg is what closes the hole a size-keyed census leaves: a second copy
carved under a wrong extent would still be found, and none exists.

**`our_bodytwins` = 2** — our `ExtraTail` spelling and our `Point` spelling, nothing
else. Compare the `--chase` tier rejected this week on this very column (median 14,
max 568).

★ **And the premise was wrong about this pair.** `reloc_tally {}` with `n_relocs 3`
does not mean *relocation-free*; it means **three relocations whose target names
AGREE**: `__savegprlr_27` @4, `memcpy` @64, `__restgprlr_27` @92, identical on both
sides. Per GROUNDED-1, when relocation names agree the destination is not masked at
all. (`our_slot0_matches_retail: False` is a cross-universe artifact — `slot0` hashes
the *callee body*, and all three helpers exist in the retail objs but in none of ours,
so it compares a hash against a name. It says nothing about the pair.)

### ⛔ The census REFUTES W16-EP's refutation

W16-EP refused its pairs 5 and 7 on:

> retail keeps **eleven** byte-identical 84-byte `list<T*>::erase` bodies — identical
> **including their `bl` targets** — unfolded at eleven distinct addresses.

The eleven bodies exist. **They are not copies of one another.**
`--family-recheck erase,list --size 84` ⇒ **11 extents, 11 DISTINCT masked forms.**
Word-level, `0x822b1e60` (`list<TargetCache>`) vs `0x82447458` (`list<Plane>`):

```
[ 7] 38600054 != 38600018   <- li r3, 84  vs  li r3, 24   (per-T NODE SIZE)
[12] 4850abc1 != 483755c9   <- bl (PC-relative, masked)
```

A **non-branch immediate** differs, so these can never fold under `/OPT:ICF`,
whatever their `bl` targets do. Ten of the eleven merely share one callee
(`MemOrPoolFreeSTL`) — which is shape similarity, not identity. This is exactly the
mechanism CLAUDE.md already records for `_List_base<T>::clear`: 42 addresses,
reloc-identical surplus **0**, differing in per-`T` node deallocators.

⇒ **A shape-only comparator reads eleven different functions as eleven copies.** The
correction does not restore EP's pairs (they may still fail on other evidence) but
its stated *reason* does not hold, and the "eleven unfolded copies" figure must not
be briefed onward as a fact about ICF.

### The instrument, and why it must be self-controlled

A census reporting "1" is indistinguishable from a broken census, so
`--control` prints the whole-image multiplicity distribution. It **finds**
duplicates at scale — 769 two-copy classes, 281 three-copy, ten eleven-copy, and a
40-byte body at **278** unfolded copies. A "1" from an instrument that tops out at
278 is a measurement; a "1" from an untested one is a guess. The 40-byte / 278-copy
stratum is also the vacuity floor to stay away from: our 96-byte body carries two
`li 0x48` stride immediates and a `memcpy`, so its discriminator does real work.
