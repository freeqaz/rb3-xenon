# W16-FF — closing the two PostProc residuals W16-FC opened: PRE-REGISTRATION

Lane W16-FF, 2026-09-16, worktree `~/tmp/wt-w16-ff`, branch `w16-ff` off `96b4fd4d`.
Ruler: `name_check` (shipped default), objdiff 4.2.9, `tool_binary_hash 5a51cd51fe0a353f`,
read from `report.json` `provenance.diff_config`. Worktree BUILT before any symbol
lookup (reflinked target objs are pre-renamer); anti-vacuity check passed — both
retail mangled names are PRESENT in `build/45410914/obj/PostProc.obj`, and
`verify_objs_patched.py --verify-manifest` reports the tree a fixed point.

## 0. Baseline REPRODUCED, not inherited

| row | size | fuzzy | mpn |
|---|---:|---:|---:|
| `?LoadRev@RndPostProc@@QAAXAAVBinStream@@H@Z` | 1,728 | **94.0000** | 94.4051 |
| `?Load@RndPostProc@@UAAXAAVBinStream@@@Z` | 192 | **95.6875** | 95.6875 |

Agrees with the W16-FF brief **to the digit**. Whole binary at baseline:
`matched_functions 44,003 / total_functions 69,240 / matched_code 4,139,008 /
total_code 10,247,068 / matched_code_percent 40.39212 / fuzzy 50.25762`.
Unit `default/PostProc`: `matched_code 12,440 / 18,564`, fuzzy 94.717735.

## 1. Retail-byte adjudication (this lane's own, on `build/45410914/asm/PostProc.s`)

`LoadRev` = 68 charged of 446 aligned rows. **Size accounting closes exactly**:
14 inserts (ours-only) − 7 deletes (retail-only) = net +7 instructions = **+28 B**;
retail 432 instrs = 1,728 B, ours 439 = 1,756 B.
⚠ This CORRECTS W16FC §5.7's "ours 1784 B" — that figure over-counts by 28 B.

Nine insert/delete clusters, each traced to a source construct:

| cluster | idx | retail bytes say | source cause |
|---|---|---|---|
| C4 | 124–133 (10 ours-only) | retail goes `bl <RndColorXfm::Load>` → straight to `.L_824311BC`; **no `clrlwi.`, no branch, no `PathName`/`ClassName`** | we test the return and call `MILO_FAIL`; retail does not test it at all |
| C2/C5/C6/C7 | 102, 180, 290–291, 295 | retail **inlines** the `ObjPtr<RndDrawable>` ctor as 3 stores (`stw r30,0x74`; `stw r26,0x78`; `stw r11,0x70`) and hoists `li r26,0x0`, reusing that zero for `stb r26,0x141(r30)` | our ctor is an out-of-line `bl` |
| C8/C9 | 390–392, 397 | retail re-materializes `mr r3,r29` per `operator>>` in the `mRefract*` run | our chained `>>` threads the returned ref through `r28` |
| C1 | 56–59 | retail `fdivs`×3 by `range`; ours `fdivs`(recip by 1.0f)+`fmuls`×3. Retail also emits a **dead** `addi r11,r30,0x30` | `/fp:fast` reciprocal CSE — no named source construct |

Plus, not insert/delete but charged:
- idx 1–6 + 441–445: prologue/epilogue `__savegprlr_24` + frame `0xf0` vs our
  `_26` + `0xe0`. **Cause identified, and it is NOT W16FC §5.7's hypothesis.**
  Retail's `rev < 0x12` block reads `addi r28,r30,0xb8` then `addi r25,r28,0x20`,
  `addi r24,r28,0x10`, `addi r4,r28,0x30` — four member addresses offset from ONE
  base held in r28, which is what makes **r24/r25** live and forces `__savegprlr_24`.
  That is the rb3-Wii oracle's `Transform *ptxfm = &mColorXfm.mColorXfm;` pointer
  local, which our DC3 copy does not have. W16FC guessed the `rev > 0xE` region
  (`fn_823A0918`/`fn_823A07A8`); the bytes say otherwise. **REFUTED.**
- idx 26/81/91: retail uses **three distinct** int slots `0x58`/`0x5c`/`0x60`;
  we reuse `0x54` twice because our legacy-bloom branch declares one `int dummy`
  and reads into it twice. The oracle declares a second `int dummy2`.

`Load` = 9 charged of 49. Eight are one defect: **stack slot ORDER**.

| var | retail | ours |
|---|---|---|
| bool | 0x50 | 0x50 |
| dRev | **0x58** | 0x5c |
| int i | **0x5c** | 0x60 |
| Vector3 v | **0x60** | 0x70 |
| float f | **0x70** | 0x58 |

Retail orders `[dRev, i, v, f]`; we order `[f, dRev, i, v]`, so our 16-byte-aligned
`Vector3` is pushed to 0x70 leaving a 12-byte hole at 0x64 — **that is the entire
16-byte frame excess** (0xb0 vs 0xa0), not an extra local. Ninth site is the
`mr r28,r3` placement (retail saves the `>> bool` return; we save the `>> Vector3`
return), which the oracle cannot explain — it spells the chain exactly as we do.

## 2. The changes (all in `src/system/rndobj/PostProc.cpp`, one TU)

- **F1** — guard the `mColorXfm.Load(bs)` return-value check + `MILO_FAIL` behind
  `#ifdef HX_NATIVE`, leaving `mColorXfm.Load(bs);` in the match build. House
  pattern; keeps native diagnostics. Targets C4.
- **F2** — `Transform *ptxfm = &mColorXfm.mColorXfm;` and stream through it
  (oracle spelling). Targets idx 107/110/111/113/115/117 **and the prologue**.
- **F3** — separate `int dummy2` for the third legacy-bloom read. Targets idx 81/91.
- **F4** — split the `mRefract*` chain into separate `bs >> …;` statements. Targets C8/C9.
- **F5** — `#define RB3_TU_OBJPTR_FORCEINLINE_CTOR` (existing per-TU lever,
  `obj/Object.h`; rndobj/ is PCH-excluded so the define is honoured). Targets C2/C5/C6/C7.
- **F6** — reorder the four `Load` locals to `[dRev, i, v, f]`. Targets 8 of 9 `Load` sites.

## 3. Predictions — bands fixed NOW, a miss is recorded as a miss

| # | claim | band |
|---|---|---|
| P1 | `LoadRev` improves: **96.5 ≤ fuzzy < 100**. It should NOT cross, because C1 (the `/fp:fast` reciprocal) names no source construct and none of F1–F5 addresses it. A 100 here means C1 dissolved as a downstream effect of register-pressure change — report it as a surprise, not as the plan working. | 96.5–99.9 |
| P2 | **Riskiest**: F2 alone restores `__savegprlr_24` + frame `0xf0`, closing idx 1–6 and 441–445. If the frame stays `0xe0`, F2 is insufficient and P2 is a MISS. | binary |
| P3 | `Load` reaches **fuzzy 100.0** if and only if the `mr r28,r3` site also closes; otherwise **97.5 ≤ fuzzy < 100** with exactly 1–2 charged sites left. | 97.5–100 |
| P4 | `Δtotal_code = 0`, `Δtotal_functions = 0`. | exact |
| P5 | `Δmatched_functions ∈ {0,+1,+2}`, `Δmatched_code ∈ {0, +192, +1728, +1920}` — bytes arrive ONLY for a row that reaches `fuzzy == 100`. | exact |
| P6 | **Designated non-movers, same TU, to the digit**: `?Save@RndPostProc@@UAAXAAVBinStream@@@Z` 99.878784 · `?Interp@RndPostProc@@QAAXPBV1@0M@Z` 84.54627 · `?SetEmulateFPS@ProcCounter@@AAAIH@Z` 96.5517 · `fn_824362B8` 0.0 · `fn_824363A0` 0.0. F5 is a per-TU ObjPtr inline-policy switch, so if any of these moves, F5 had collateral and must be isolated. | exact |
| P7 | No row outside `default/PostProc` changes, except call sites in `default/Anim` (`Dir.cpp`) if F1–F6 touch them — they should not; I edit only `PostProc.cpp`. | exact |

## 4. Deliberately NOT done
- **Not naming `fn_823A0918` / `fn_823A07A8`.** They are forgiven placeholder
  targets today; naming converts a forgiven site into a checked one — a bet, not a
  freebie. The brief forbids folding it into a source wave, and the retail bytes
  now show the prologue cause is F2, so there is no longer a reason to.
- **Not touching `MILO_FAIL`'s global definition.** `Debug.h` records that the
  arg-evaluating form is what flips the `ObjectDir::Find<T>` family to 100%; a
  global change would be a large regression. F1 is per-site.

## 5. Exploration log (in-worktree, full builds so the six obj patchers always ran)

Each row is one full `./tools/ninja-locked` + `report.json` read. These are
EXPLORATION, not the graded A/B; the graded number is §6.

| # | variant | `LoadRev` fuzzy | `Load` fuzzy | verdict |
|---|---|---:|---:|---|
| 0 | baseline | 94.0000 | 95.6875 | — |
| 1 | F1+F2+F3+F4+F6 | **97.3588** | 95.6875 | F1–F4 land; **F6 (declaration reorder) is 100% INERT** |
| 2 | +F6b named-reference chain (`BinStream &s = bs >> b70; s >> v40 >> f30; s >> i5c;`) | 97.3588 | **99.8542** | **HIT** — removes the `mr r28,r3` insert/delete; 48 rows, 0 insert/delete, structurally identical to retail |
| 3 | +F5 `RB3_TU_OBJPTR_FORCEINLINE_CTOR` | **98.5509** | 99.8542 | ⛔ **REVERTED — net −668 B**: `??0RndPostProc@@IAA@XZ` (668 B) collapsed **100.0 → 60.9581** |
| 4 | oracle bloom spelling (`float red = c.red;` hoist, `mBloomThreshold` after the 3 divisions) | 97.358795 | — | ⛔ **BYTE-INERT — refutes W16FC §5.7's designated experiment #1** |
| 5 | invert the `minVal < 4.0f` if/else | **94.1412** | — | ⛔ worse — block order flips; reverted |
| 6 | `float f30;` + separate `f30 = 0;` | — | 99.854164 | inert |
| 7 | drop `MILO_ASSERT(dRev == 3, …)` | — | 99.854164 | inert |

### 5.1 What the exploration ESTABLISHED

- **F6 as briefed is refuted, and the real lever was a different one.** W16FC §5.7
  offered two candidates for `Load`: split the chain, or reorder the declarations.
  Reordering is **completely inert** (byte-identical output, twice — with and
  without the initializer). The chain split is the whole effect, and it closed
  **both** listed defects at once: the `mr r28,r3` placement *and* nothing else —
  see §5.2 for what it did not close.
  ⇒ The two `Load` defects W16FC listed as separate are **one** defect in the
  sense that only one lever exists; but the frame layout did NOT follow.
- **The prologue cause in W16FC §5.7 is REFUTED and replaced.** It hypothesised
  the `rev > 0xE` region and the two unnamed callees. The bytes say the extra
  non-volatiles come from the `rev < 0x12` `Transform*` block (r24/r25) plus the
  hoisted `li r26,0x0` that only exists because retail inlines the ObjPtr ctor.
  F2 moved us `__savegprlr_26 → _25`; the last register is the ObjPtr zero.
- **The `/fp:fast` reciprocal is not reachable by the oracle's spelling.** Variant
  4 is byte-inert, so "rb3-Wii spells the block differently" — true as a source
  observation — has **no codegen consequence** here.

### 5.2 Two walls, stated precisely, with falsifiers

**Wall A — `LoadRev` needs `ObjPtr<RndDrawable>`'s ctor inlined, and no per-TU
lever can express that.** Retail inlines it (3 stores) and out-of-lines the
`ObjPtr<RndTex>` member ctors in `??0RndPostProc@@` — a *per-instantiation*
decision. Our levers (`RB3_TU_OBJPTR_FORCEINLINE_CTOR` et al.) are **per-TU**, so
turning it on inlines every instantiation and costs 668 B in the ctor to buy
+1.19 pp on a row that still does not cross. Measured, not assumed.
*What would change my mind:* a per-call-site or per-instantiation inline lever
(e.g. a lever keyed on the template argument), or evidence that retail's
`??0RndPostProc@@` also inlines its members and our 100.0 there is itself wrong.

**Wall B — `Load`'s last 7 charges are frame-BANK ordering, and source does not
steer it.** Both sides 16-align the `Vector3`, so exactly two 4-byte scalars may
precede it. Retail's third scalar (the `float`) is placed ABOVE the aggregate;
ours is placed below, which pushes `Vector3` to 0x70 and leaves a 12-byte hole —
the whole 16-byte frame excess. Declaration order (twice), initializer form, and
`MILO_ASSERT` presence are ALL inert. The grouping looks like "FP-typed locals in
their own bank", with the bank ORDER differing.
*What would change my mind:* any demonstration that the FP/int bank order is
steerable (a TU where changing a float local's spelling moved a frame bank), or
a retail-byte reading showing the 0x70 slot is a compiler temporary rather than
the named `float`.

**Wall C — the `/fp:fast` reciprocal (`fdivs`+3×`fmuls` vs 3×`fdivs`).** Not
addressed by any spelling tried. Note the f29/f30 constant-register swap (8
charged sites) is plausibly DOWNSTREAM of it: our reciprocal gives 1.0f an extra,
earlier use, and our prologue loads 0.0,1.0,4.0 where retail loads 1.0,0.0,4.0.
*What would change my mind:* a source form that suppresses MSVC's reciprocal CSE
with the divisor still CSE'd (retail computes `range` once and divides 3 times,
so "don't share the divisor" is not it), or evidence the constant order is a
constant-pool/link artifact rather than a first-reference-order artifact — in
which case the 8 f29/f30 sites are unreachable independently of the divisions.

## 6. MEASURED — the graded whole-binary A/B

`python3 tools/ab_measure.py --worktree /home/free/tmp/wt-w16-ff --from-dirty`,
rc=0, **no refusal**. Both legs settled to a zero-work fixed point (2 iterations
each); leg B recompiled exactly 1 TU with all 6 patch steps, so the run is not
absent-vs-absent. Ruler `functionRelocDiffs=name_check` off `objdiff.json`;
objdiff-cli pinned stable across both legs (`sha256:c1b7d95240a35cd6`).

```
leg A: matched=44003 masked=23235 honest=20768 code%=40.392120 fuzzy=50.257620
leg B: matched=44005 masked=23237 honest=20768 code%=40.392510 fuzzy=50.258255
Δmatched=+2  Δmasked_equal=+2  Δhonest=+0
Δcode_bytes=+40  Δcode%=+0.000390pp  Δfuzzy=+0.000635pp
unit improvements: 1 unit -- +2 default/PostProc (116->118)
unit net (ALL units) = +2  vs whole-binary Δmatched = +2
units at 100%: mpn 189->189, all-rows-fuzzy 169->169 (0 reached, 0 fell off)
```

### 6.1 Per-row, by A→B set-diff of the archived reports (EXACTLY 4 rows moved)

| row | size | fuzzy A→B | mpn A→B | masked |
|---|---:|---|---|---|
| `?LoadRev@RndPostProc@@QAAXAAVBinStream@@H@Z` | 1728 | **94.0000 → 97.358795** | 94.40509 → 97.671295 | no |
| `?Load@RndPostProc@@UAAXAAVBinStream@@@Z` | 192 | **95.6875 → 99.854164** | 95.6875 → 99.854164 | no |
| `fn_824316B8` | 40 | 99.8 → **100.0** | 99.8 → 100.0 | **yes** |
| `fn_82431690` | 40 | 99.4 → 99.5 | 99.9 → **100.0** | **yes** |

⛔ **READ THE HEADLINE HONESTLY: NEITHER TARGET ROW CROSSED, AND THE ENTIRE
+2 fn / +40 B COMES FROM TWO 40-BYTE EH FUNCLETS.** `matched_code` is
all-or-nothing per row, so `LoadRev` at 97.36 and `Load` at 99.85 have
contributed **zero of the 1,920 B**. Both movers are `masked_equal=True`
(byte-signature-paired funclets), which is exactly why **Δhonest = +0**: the
lane's headline number rests entirely on funclet disclosure, not on new honest
content. The funclets moved because F1 removed the `MILO_FAIL` call and with it
an EH region in this TU.
⇒ The truthful one-line summary is **"+0 honest, 1,920 B still uncollected"**,
not "+2 functions".

### 6.2 Predictions vs measurement — including the misses

| # | prediction | measured | verdict |
|---|---|---|---|
| P1 | `LoadRev` 96.5 ≤ fuzzy < 100 (must NOT cross) | 97.358795 | ✅ **HIT** |
| P2 | F2 alone restores `__savegprlr_24` + frame 0xf0 | `_25`, frame +8 not +16 | ⚠️ **PARTIAL MISS** |
| P3 | `Load` 97.5 ≤ fuzzy ≤ 100 | 99.854164 | ✅ **HIT** |
| P4 | Δtotal_code = 0, Δtotal_functions = 0 | 10,247,068 / 69,240 both legs | ✅ **HIT** |
| P5a | Δmatched_functions ∈ {0,+1,+2} | +2 | ✅ HIT |
| P5b | Δmatched_code ∈ {0,+192,+1728,+1920} | **+40** | ❌ **MISS** |
| P6 | 5 designated non-movers unchanged to the digit | all 5 unchanged | ✅ **HIT** |
| P7 | no row outside `default/PostProc` changes | 4 changed rows, all in `default/PostProc` | ✅ **HIT** |

**P5b is a real miss and worth stating plainly:** I enumerated the byte outcomes
as multiples of the two target rows and did not consider that a *third* row
could cross. The EH-funclet channel was not in my model at all. A prediction set
that cannot express the outcome that occurred is under-specified, even though
every individual band held.

**P2 is the substantive miss.** F2 bought exactly one non-volatile
(`_26 → _25`, frame +8), not two. §6.3 identifies the eighth.

### 6.3 ★ THE PROLOGUE CAUSE, FULLY RESOLVED — and it REFUTES this doc's own Wall A

The charged-site list (42 of 435) names the missing register directly:

```
idx 280  delete   TGT: lis  r11, lbl_82049FAC@h
idx 281  delete   TGT: stw  r30, 0x74(r31)
idx 282  replace  TGT: addi r11, r11, lbl_82049FAC@l   SRC: li   r5, 0x0
idx 283  replace  TGT: stw  r26, 0x78(r31)             SRC: mr   r4, r30
idx 284  replace  TGT: stw  r11, 0x70(r31)             SRC: addi r3, r31, 0x70
idx 285  insert   TGT: ---                             SRC: bl ??0?$ObjPtr@VRndDrawable@@@@QAA@PAVObject@Hmx@@PAVRndDrawable@@@Z
```

Retail **inlines** `ObjPtr<RndDrawable>`'s two-arg ctor as its three stores
(vtable@0x70, mOwner@0x74, mObject@0x78); we emit a `bl`. **ONE cause explains
~14 of the 42 charges:**

- 280–285 — the ctor body itself (6);
- 102 `li r26,0x0` deleted / 170 `li r11,0x0` inserted / 171 `stb r26` vs
  `stb r11` — retail hoists the zero into the **non-volatile r26** and reuses it
  for *both* the inlined ctor's `mObject` store and `stb r26,0x141(r30)` (3);
- 1–4 — that eighth non-volatile IS `__savegprlr_24` vs `_25` and the +8 frame (4);
- 110/111/113/115 — r24/r25/r26 renumbered one slot, downstream of the same (4).

⇒ **W16FC §5.7's prologue hypothesis is refuted for the second time, and so is
§5.2's Wall A as I first wrote it.** It is neither the `rev > 0xE` region (FC's
claim) nor "no lever can express this" (my claim): the lever
`RB3_OBJPTR_INLINE_TWOARG_CTOR` in `src/system/obj/Object.h` targets *precisely*
this ctor, and `rndobj/` is PCH-excluded so it is valid in this TU.

### 6.4 ★ Why I did NOT pull that lever — the economics, from a MEASURED number

**Because it is worth exactly zero bytes, and F5 already proved it.** F5
(`RB3_TU_OBJPTR_FORCEINLINE_CTOR`, exploration row 3) *did* inline this ctor and
took `LoadRev` to **98.5509** — still short of 100, so still **0 bytes** — while
collapsing `??0RndPostProc@@IAA@XZ` from 100.0 to 60.9581 for **−668 B**. That
98.55 is the empirical ceiling of the whole ObjPtr class: with the ctor inlined,
the ~20 charges of the `/fp:fast` cluster (56/58/59/61/63), the f29/f30 constant
swaps (54/75–78/134/135/140) and the r10/r11 swaps (50/65–69/72) all **survive**.

⇒ **`LoadRev` cannot reach 100 without solving the `/fp:fast` reciprocal, and
until that is solved every other `LoadRev` fix — the granular ObjPtr lever
included — pays 0 B while carrying real regression risk.** The granular-lever
question is therefore *moot*, not unresolved. **Wall A as originally stated was
wrong in its reason and right in its conclusion**, which is exactly the kind of
"count right, cause wrong" the house docs warn about — I am recording both.

### 6.5 The one lead I did NOT chase, with its retail-byte evidence

Retail's bloom block round-trips through the stack in a way our version does not:

```
lfs   f10, 0x8c(r31)      ; read back
stfs  f30, 0x8c(r31)      ; store 0.0
stfs  f10, 0x40(r30)
fdivs f11, f11, f0
stfs  f11, 0x80(r31)      ; 0x80/0x84/0x88/0x8c = a 4-float aggregate
fdivs f12, f12, f0
stfs  f12, 0x84(r31)
fdivs f0, f13, f0
stfs  f0, 0x88(r31)
```

`0x80/0x84/0x88/0x8c` is a **stack-resident 4-float aggregate** (an
`Hmx::Color` temp) whose alpha is set to 0.0 and whose red is read back *before*
being overwritten. Our source assigns members directly. **Hypothesis for the
next lane:** retail's source divides into a *local `Hmx::Color`* rather than
into members, and it is the aggregate temp — not the expression spelling — that
denies MSVC the reciprocal CSE. I did not test it: it is a new wave, it needed
its own pre-registration, and §6.4 shows it is the ONLY remaining path to any
bytes on this row, so it deserves a lane that can measure it properly rather
than a rushed addendum to this one.
*What would change my mind about the row being unfixable:* a variant in which
the three divisions are written into a stack-local aggregate and the emitted
code shows **three `fdivs`** instead of `fdivs`+`fmuls`. That single observation
would reopen the whole 1,728 B.

### 6.6 Verdict on the two rows

- **`Load` (192 B) — 95.6875 → 99.854164, 7 charges left, ALL offset-only.**
  Structurally identical to retail: 48 rows, zero insert/delete. Blocked only by
  Wall B (frame bank order). **Not crossed; 192 B uncollected.**
- **`LoadRev` (1,728 B) — 94.0000 → 97.358795, 42 charges left.** Blocked by
  Wall C (`/fp:fast`), with the ObjPtr class (Wall A) moot behind it.
  **Not crossed; 1,728 B uncollected.**

**Net honest deliverable: +0 honest functions, +40 B of funclet disclosure, two
rows moved substantially closer, three hypotheses refuted with evidence, and one
new lead localized to a specific 4-float stack aggregate.**
