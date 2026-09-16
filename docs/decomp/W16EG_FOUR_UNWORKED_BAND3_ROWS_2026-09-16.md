# W16-EG — four unworked structural rows in `src/band3/`: two fixed, two walled

**Date:** 2026-09-16 · **Branch:** `w16-eg` · **Base:** `0351e376` · **Worktree:** `~/tmp/wt-w16-eg`
**Ruler:** graded (`functionRelocDiffs=name_check`), objdiff 4.2.9, `tool_binary_hash=5a51cd51fe0a353f`,
read from `report.json`'s own `provenance` block — not assumed.

Four rows that appear only in census TSVs and in no prior lane write-up. Two were closed by
source work; two are documented walls so the next lane does not re-hunt them.

| row | size | fuzzy before | fuzzy after | outcome |
|---|---:|---:|---:|---|
| `?RefreshAll@ManageBandPanel@@QAAXXZ` | 412 | **0.0000** | **100.0** | ✅ **crossed, +412 B** |
| `?Poll@PatchPanel@@UAAXXZ` | 980 | 91.4571 | **94.5918** | improved, did not cross |
| `?Poll@StoreMainPanel@@UAAXXZ` | 664 | 92.9819 | 92.9819 | ⛔ **wall — MSVC cold-block outlining** |
| `?HandleExitExtent@PerfectSectionTracker@@QAA_NMH_N@Z` | 1256 | 92.0892 | 92.0892 | ⛔ **wall — regalloc, 62 swap instrs** |

---

## 1. The 0.0000 row was NOT a phantom or an identification problem — check byte geometry first

The brief's leading hypothesis was that `RefreshAll` at 0.0000 would be an unpaired/phantom row
(a dtk mis-carve), where naming is unrecoverable and source work is wasted. **It was measured and
refuted before any source was touched.**

Retail `.pdata` parsed directly out of `orig/45410914/band.exe` (big-endian `(BeginAddress,
UnwindData)` pairs, section table walked from the PE header):

| row | map address | exact `.pdata` BeginAddress? | `.pdata` extent | `report.json` size |
|---|---|---|---:|---:|
| `RefreshAll@ManageBandPanel` | `0x82625b58` | **yes** | 412 | 412 ✓ |
| `Poll@PatchPanel` | `0x8262af48` | **yes** | 980 | 980 ✓ |
| `Poll@StoreMainPanel` | `0x8263ae00` | **yes** | 688 | 664 (−24) |
| `HandleExitExtent@PerfectSectionTracker` | `0x826db400` | **yes** | 1256 | 1256 ✓ |

All four are exact `.pdata` function starts. `RefreshAll`'s extent agrees with the billed size to
the byte, it sits inside ManageBandPanel's second pinned `.text` block
(`0x826259E0`–`0x82626A3C`), the asm carves it cleanly (`.fn fn_82625B58` → `.fn fn_82625CF4`),
and the map names it. It was a genuine **source** divergence all along.

★ **Corroborating tell, cheaper than the `.pdata` parse:** `report.json` gave it
`fuzzy = 0.0000` but `mpn = 2.378641`. A non-zero `mpn` means objdiff *did* pair it against a base
symbol and compare instructions. **A truly unpaired row reads 0 on both keys.** Read both before
concluding "unidentified".

---

## 2. `RefreshAll@ManageBandPanel` — three retail-adjudicated deltas, 0.0000 → 100.0

Our base was **784 B against a 412 B target** — 95 inserts vs 2 deletes. Retail's body
(`fn_82625B58`) and the sibling it calls (`fn_826259E0`) were disassembled to settle why.

### 2a. Retail factors the vignette loop out of line; both oracles inline it

Retail `RefreshAll` ends its first half with

```
lwz r5, <Property(reward_vignettes,true)->Array(NULL)>
lwz r4, 0x50(r30)      ; mProfile
lwz r3, 0x54(r30)      ; mHistoryProvider
bl  fn_826259E0
```

and `fn_826259E0` (332 B) is the loop: `stw r5, 0x2c(r30)` (= `VignetteViewerProvider::unk20`),
`bl <AccessAccomplishmentProgress>(r4)`, `addi r26,r3,0xc4` / `addi r25,r3,0xbc` (the set `unkb0`
and list `mNewRewardVignettes`), the `0x30/0x34` `mEntries` clear, then `Array`/`Sym`/
`_M_find<Symbol>`/`push_back`. Header offsets confirm every one.

Extracted into `VignetteViewerProvider::RefreshVignettes(BandProfile*, DataArray*)`. MSVC did
**not** inline it back — no `__declspec(noinline)` was needed.

### 2b. Retail has **no outer `if (mProfile)`**

Retail opens `mr r30,r3; lwz r3,0x50(r3); bl <GetAssociatedLocalBandUser>` with no null test.
The *inner* `if (mProfile)` before `EarnAccomplishment` **is** present (`0x82625C38`). Our source
carried an outer guard the retail build does not have — visible as the `cmplwi`/`beq` inserts at
idx 7–8.

### 2c. `MetaPanel::sUnlockAll` is dev-build code retail compiled out

Our base loaded `?sUnlockAll@MetaPanel@@2_NA@` (inserts 68/93). **Retail's helper contains no
reference to it anywhere in its 332 bytes.** Gated per-site with the documented house pattern
`#if defined(MILO_DEBUG) && defined(HX_NATIVE)`, which keeps native behaviour and is inert in the
match build (`grep -c ' /DHX_NATIVE' build.ninja` = **0**, verified in this worktree).

Chosen over `/DRB3_STRIP_CHEAT_HANDLERS` + `objects.json` deliberately: that would have put a
generated-`build.ninja` change inside an A/B window for no measurement benefit.

### 2d. The residual was one more code motion — and it named its own prologue delta

After 2a–2c the row read 87.8641 with base size exactly 412 B. The remaining cluster: retail
initialises the `acc_bandlogo` static at `0x82625C08`, **before** the `if (mProfile)` at
`0x82625C38`. Ours declared it inside the `if`, so `GetAssociatedLocalBandUser`'s result had to
survive the `Symbol` ctor call and was pinned into a callee-saved register — which *was* the
`PROLOGUE_MISMATCH` (`r27-r31` vs `r28-r31`) and both `__savegprlr`/`__restgprlr` charges.

★ Hoisting the static fixed the code motion, the prologue and the register-save helpers **in one
edit**. Three separately-reported "RarelyHandFixable" patterns, one source cause. This is the
documented lesson again: a prologue/regalloc label on a sub-100 row is a *symptom*, not a verdict.

---

## 3. `Poll@PatchPanel` — retail does not cache members in locals (+3.13 pp)

Our source opened with an rb3-Wii/MWCC-ism:

```cpp
StickerProvider *_mStickerProvider = this->mStickerProvider;
PatchDir       *_mPatch            = this->mPatch;
```

Retail re-loads both on every use — `lwz r3, 0x3c(r30)` where we had `mr r3, r27`, and
`lwz r3, 0x44(r30)` where we had `mr r3, r28` — and orders the first load *after*
`UIPanel::Poll()`, which our pre-load could not do. Removing the two caches took the row
**91.4571 → 94.5918** and collapsed charged instructions **47 → 21**: `PROLOGUE_MISMATCH`,
`REGISTER_SAVE_HELPER_MISMATCH`, `ADDRESS_RELOCATION_NOISE` and every r26↔r27 swap dissolved
together — again one cause behind four labels.

**Residual (named, for the next lane), all inside the scale-clamp:** f0↔f10 ×5 and f10↔f12 ×3
register swaps; two commutative `fmuls` operand orders (idx 173/174); `lfs` scheduling slips at
77/79 and 157/159; and **four target-only `fcmpu/blt/fcmpu/ble` at 166–169** — retail keeps the
two `if (newScaleX < minS) / else if (> maxS)` clamps separate in both arms of
`if (newScaleX < 0.0f)` where our build CSE'd them into one. That last item is the only structural
lead left; the rest is scheduling.

⚠ `fn_82629FE8`×6 and `fn_822750A8`×2 in the call diff (vs our `CalcMotion` / `NumLoadingStickers`)
are **placeholder target names and therefore uncharged** — they appear in the "Function Call Diff"
but in no mismatch row. Do not read that section as a defect list.

---

## 4. ⛔ `Poll@StoreMainPanel` — a wall, and not a source problem

Only 12 charged instructions, one cluster. Retail places the `else` arm of `if (i < 2)` — the
`mCoverArtTexs[idx]` fetch — in an out-of-line block `.L_8263B07C` that sits **after the function
epilogue** (`b __restgprlr_25`); our build emits it inline.

Both sides already have the identical predicate and an identical *per-branch* null test
(`cmpwi` in the `i<2` arm, `cmplwi` in the `else` arm, on both sides), so the source shapes agree
— our `if/else` is already what retail wrote. What differs is **MSVC cold-block outlining**, a
layout decision not addressable from source. The only other tell, `lwzx r9,r11,r10` vs
`lwzx r4,r10,r11`, is the same commutative address form.

**Do not re-fund this as a source lever.** 664 B, unreachable without a layout mechanism we do
not have. The `SetObjConcrete<Object>` vs `<RndTex>` callee pair here is an ICF fold already
forgiven by alias group 2 (see §6) — it is not a defect and not the blocker.

---

## 5. ⛔ `HandleExitExtent@PerfectSectionTracker` — a wall (regalloc), with two live correctness notes

Target and base are **both exactly 1256 B**. The charge list is 62 register-swap instructions
across 8 pairs (r27↔r28 ×26, r25↔r26 ×8, r10↔r11 ×6) plus 8 inserts / 8 deletes and three offset
swaps. With `matched_code` all-or-nothing per row, nothing is collectable short of 100, and a
62-instruction allocation-order divergence is not closable by hand. The permuter is OFF by
standing user directive, so this row is parked.

Two genuine source divergences were found anyway and are recorded because they are *correctness*
facts, not metric facts (expected Δ0):

- **Parenthesisation changes a sign.** Ours: `unkc - (m0x0c - unk8 - i11c)`; the rb3-Wii oracle:
  `unkc - (m0x0c - unk8) - i11c`. These differ in the sign of `i11c`. Retail's stream is a pure
  `subf` chain where ours emits `add` (base idx 91/94), i.e. **retail agrees with the oracle and
  our source is wrong here.**
- **Pre- vs post-increment.** Ours `GetMultiplier(++data.unk1c)`, oracle `GetMultiplier(it->second.unk1c++)`.

★ And the opposite direction on the same function: the oracle declares
`void HandleExitExtent(float,int,bool)`, but the retail mangled name is
`?HandleExitExtent@...@@QAA_NMH_N@Z` — `_N` return, i.e. **`bool`**. Our source is already right
and the oracle is wrong. *Retail outranks the oracle in both directions on one function.*

---

## 6. Three "wrong callee" pairs adjudicated — all ICF folds, all already forgiven

Checked against `scripts/symbol_aliases.json` (read-only) **before** treating any as a defect:

| target spelling | our spelling | shared alias group |
|---|---|---|
| `ObjRefConcrete<Object,ObjectDir>::SetObjConcrete` | `ObjRefConcrete<RndTex,ObjectDir>` | **2** |
| `_Rb_tree<int,pair<const int,SongStatus>>::_M_find` | `_Rb_tree<TrackType,pair<const TrackType,PlayerStreakData>>` | **30, 83, 169, 170** |
| `map<int,int>::operator[]` | `map<TrackType,int>::operator[]` | **35** |

None is charged. `TEMPLATE_ARGS_DIFFER` is what a fold looks like — grepping the alias file first
cost one tool call and removed a false lead from two of the four rows.

---

## 7. Source vs identification, priced before measuring (W16-CD's lesson)

Every one of the four units carries **more anonymous sub-100 bytes than named ones**:

| unit | sub-100 NAMED | sub-100 ANON |
|---|---:|---:|
| `band3/game/PerfectSectionTracker` | 1 row / 1,256 B | 12 rows / 2,128 B |
| `band3/meta_band/PatchPanel` | 6 rows / 1,788 B | 11 rows / 3,120 B |
| `band3/meta_band/StoreMainPanel` | 6 rows / 1,760 B | 7 rows / 1,180 B |
| `band3/meta_band/ManageBandPanel` | 2 rows / 608 B | 4 rows / 684 B |

The split was declared up front and it held: **`RefreshAll`'s 412 B is source and was collected;
the helper `fn_826259E0`'s 332 B is identification (map) and was NOT collected.** Our new
`RefreshVignettes` cannot pair — retail's counterpart is anonymous — so it contributes 0 and
costs 0. Naming `fn_826259E0` (and `fn_82624F88`, 272 B) would require a
`scripts/target_symbol_map.json` edit, deliberately not made here (lane ownership, §9).

⇒ A lane that had implemented `RefreshAll` perfectly and stopped would have measured exactly what
this one did: **+412, not +744.** The other 332 B is a different kind of work.

---

## 8. Pre-registrations and whether they held

| # | prediction, recorded before measuring | outcome |
|---|---|---|
| P1 | drop `_mPatch`/`_mStickerProvider` caching: fixes prologue + save-helper + the `mr`↔`lwz` replaces; **improves but does not cross**; whole-binary Δ0 B | ✅ **HELD** — 91.4571 → 94.5918, 47 → 21 charges, Δ0 B |
| P2 | `StoreMainPanel`: 12 charges are one cluster, **50/50 it crosses**, +664 B if so | ❌ **MISSED** — not a source lever at all; MSVC cold-block outlining (§4) |
| P3 | `RefreshAll`: 412 B source-reachable, helper's 332 B is map work I will not collect | ✅ **HELD** — +412 B exactly, 332 B left uncollected as predicted |
| P3b | hoisting `acc_bandlogo` fixes code motion **and** prologue together → fuzzy 100, +412 B | ✅ **HELD to the byte** — +1 fn / +412 B |
| P4 | `HandleExitExtent` does not cross; expression fixes are Δ0 correctness | ✅ **HELD** (parked, §5) |

**The miss was the informative one.** P2 was predicted as a source lever because 12 charges in one
cluster *looks* like a missing/misplaced statement. Reading the retail asm past the charge list —
specifically noticing `.L_8263B07C` sits after `b __restgprlr_25` — is what converted it from
"unworked row" to "documented wall". A charge-list cluster is not evidence of a source construct;
the block's *placement relative to the epilogue* is.

---

## 9. What this lane did NOT do, and why

- **No `scripts/target_symbol_map.json` or `scripts/symbol_aliases.json` edit.** Lanes W16-EE and
  W16-EF were live on both. The 332 B `fn_826259E0` and 272 B `fn_82624F88` in ManageBandPanel are
  left on the table for a map lane; both are now *described* (§2a), which is the expensive half.
- **No `src/band3/game/Game.cpp` / `Game` unit** — owned by W16-EF.
- **No `splits.txt` edit, no permuter** (standing user directive).
- **No grind on `PatchPanel`'s residual.** Named and located in §3 instead; the row is at 94.59
  with one structural lead (the CSE'd clamp) and the rest scheduling.
- **`StoreMainPanel`'s −24 B geometry gap** (`.pdata` 688 vs billed 664) was noted but not chased;
  it is the trailing EH-prefix/padding of the next function and does not affect the row's pairing.

---

## 10. Whole-binary A/B — settled, both legs quiescent

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-eg --from-dirty`, patch
`11f5b96c946e068a`, classified `kinds=['source']`, ruler `name_check` (resolved from
`objdiff.json` options, not assumed), objdiff-cli pinned across both legs
(`sha256:c1b7d95240a35cd6`).

```
leg A: matched=43950 masked=23224 honest=20726 code%=40.232000  (recompiles: 0, settled)
leg B: matched=43951 masked=23224 honest=20727 code%=40.236015  (recompiles: 3, settle iterations: 2)
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.004015pp  Δcode_bytes=+412
Δfuzzy=+0.004194pp   (legA 49.998850 -> legB 50.003044)
unit improvements: 1 unit(s), sum +1
    +1  default/band3/meta_band/ManageBandPanel  (74->75)
unit net (ALL units) = +1   vs whole-binary Δmatched = +1
units at 100%: mpn 189->189, all-rows-fuzzy 169->169
```

Both legs reached a zero-work build before being read (leg A settled in 2 iterations with
0 recompiles at the read; leg B did its 3 recompiles then went quiescent), so no settling
noise is in the delta. `Δhonest = +1` — the gain is a real new body, not a masked-equal
funclet pairing. Unit net equals whole-binary Δ, so nothing regressed elsewhere.

★ **P3b predicted +412 B and the settled A/B measured +412 B.** That is the cleanest kind
of confirmation available here: the row's `.pdata` extent, the `report.json` billed size,
the predicted delta and the measured whole-binary delta are all the same number.

⚠ The `[control none]` leg is correctly reported **NOT_APPLICABLE** — it moved +412 B too,
which is expected for a source patch and adjudicates nothing. The alias-shape control is
only meaningful on a map-only patch, and this lane made none.

---

## 11. Ledger

| | |
|---|---|
| whole-binary | **+1 matched function / +412 matched code bytes** (settled A/B) |
| rows crossed | `?RefreshAll@ManageBandPanel@@QAAXXZ` 0.0000 → 100.0 |
| rows improved | `?Poll@PatchPanel@@UAAXXZ` 91.4571 → 94.5918 (Δ0 B — all-or-nothing ruler) |
| rows walled, documented | `?Poll@StoreMainPanel@@UAAXXZ` (cold-block outlining), `?HandleExitExtent@PerfectSectionTracker@@` (62-instr regalloc) |
| map / alias edits | **none** — no merge-order dependency on W16-EE or W16-EF |
| files touched | `src/band3/meta_band/{ManageBandPanel.cpp,ManageBandPanel.h,PatchPanel.cpp}` |
