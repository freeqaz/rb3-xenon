# W22-FRAME — VocalPlayer's 0x10 frame shortfall: Handle CLOSED (+5,296 B), Poll sized and walled

**2026-08-17.** Baseline verified in-worktree before any edit, reproducing the
brief **exactly on all six figures**: `matched_functions` 44,488 · `matched_code`
3,750,264 B = 36.33743% · honest 21,588 · `total_code` 10,320,664 ·
`total_functions` 69,226. Ruler `name_check`, read from `report.json`'s own
`provenance.diff_config` (not assumed).

**Shipped: +5,296 B / +11 functions / +1 honest / +0.051314 pp**, measured by
`ab_measure --revert` of the one functional commit, both legs settled.

W19 diagnosed this cluster and deliberately did not attempt it. This lane
extends it: the frame wall is **real, and it was closable on `Handle`**. The
route W19 proposed (port `HandleDeactivateVolume`) was right in outcome and
**wrong in mechanism** — see below, because the stated mechanism would have sent
the next lane at the wrong lever.

---

## ★ The mechanism was NOT `/Ob2` deleting the temp

`VocalPlayer.cpp:1570` recorded, and W19 repeated, that `/Ob2` folds the
`OnMsg(const ButtonUpMsg&)` stub to a constant and **"deletes the whole temp,
costing Handle 12 instructions and 0x10 of frame"**, fixed by
`#pragma auto_inline(off)`.

**Measured: with the pragma already in place, the temp is NOT deleted.** Both
sides construct it and both call out of line —
`bl fn_826E5E98` vs `bl ?OnMsg@VocalPlayer@@…` at the same instruction index. The
pragma had already done its job. The residual was **slot assignment**, not
existence.

The real mechanism is **escape / memory-effect analysis**, and
`auto_inline(off)` does not touch it — it stops *inlining*, not MSVC's intra-TU
reasoning about what a callee does. With an **empty body**, MSVC proves `OnMsg`
neither writes memory nor lets `&msg` escape, and therefore in `Handle`:

| retail (`0xf0`) | ours (`0xe0`) | what MSVC did |
|---|---|---|
| `stw r30, 0x88(r31)` | *(absent)* | killed the dead `~Message` vptr store |
| `lwz r3, 0x8c(r31)` | `mr r3, r27` | forwarded `r27` instead of reloading `mData` from the temp |
| temp at `0x88` (private 8 B) | temp at `0x58` (shared scratch) | overlaid the temp onto a dead slot |

Retail keeps **four** 8-byte message temps (`0x58 / 0x88 / 0x90 / 0x98`); we kept
**three**. That missing 8 bytes rounds the frame down by 0x10 and displaces every
callee-saved register by one.

⇒ **Handing `&msg` to an out-of-TU callee is what defeats the analysis.** A body
that merely *exists* is not enough if MSVC can see through it.

### The probe that proved it — and the coupling W19 assumed

Before porting anything, a throwaway probe (`static const ButtonUpMsg *volatile
p; p = &msg;`) tested the hypothesis and, more importantly, tested W19's
**load-bearing unproven assumption** that the 212-site `r28`↔`r29` swap is
"downstream of the frame":

| | baseline | probe |
|---|---|---|
| frame | `-0xe0` | **`-0xf0`** |
| `base_size` | 4932 | **4936** == target |
| fuzzy | 97.97326 | 99.49352 |
| hard diffs | 15 | 6 |
| immediate charges | 13 | **0** |
| register charges | **219** (212 = `r28`↔`r29`) | **4** |

⇒ **W19's assumption is now PROVEN, not inherited.** All 212 register charges
are downstream of the frame; they dissolve with it. This is the 13th recorded
instance of a `REGISTER_SWAP` being a symptom rather than a diagnosis.

★ **But the probe did NOT reach 100 — and the real body did.** The probe left an
8-instruction **scheduling** residual (retail sinks the `addi r4,r31,0x88` /
`subi r3,r25,0x47c` argument setup to just before the `bl`; the probe hoisted
them). I pre-registered that this scheduling window would survive the real port
and cost Handle its bytes. **It did not: the faithful body scored fuzzy
100.00000, 0 charged sites.** ⇒ **A fake body can buy the frame and still miss
the schedule.** Do not conclude "permuter-class residual" from a probe; the
scheduler is sensitive to the real call's operands.

## The port — from RETAIL BYTES, not from an oracle

Both oracles are **empty here**: rb3-Wii has `OnMsg(const ButtonUpMsg&) { return
0; }`, the same stub, and the RB2 dump
(`../rb3/doc/rb2_dump/.../VocalPlayer.cpp:857`) carries **signatures only** —
declared locals and referenced globals, no code. So the body was decoded from
retail's `fn_826E5E98` (0xF4 B), instruction by instruction:

| retail bytes | source |
|---|---|
| `lwz 0x260(this)` + vbase adjust (`lwz +4; lwz +4; add; addi +4`) | `(User *)GetUser()` |
| `bl fn_825150C8` + same adjust; differ ⇒ return | `msg.GetUser()` |
| guarded static init (`lbl_82E03510`, guard `lbl_82E03514`, string `lbl_8202B3CC`) | `static Symbol vocalist_volume("vocalist_volume")` |
| `mData->Node(3).Int(mData)` | `msg.GetButton()` (inline in `JoypadMsgs.h`) |
| `==1 T; <=2 F; <=5 T; ==7 T; else F` | predicate over `{R2, R1, Tri, Circle, Square}` |
| `mr r4,r3` held across, then `bl fn_826E50C0` | `HandleDeactivateVolume(but)` |

★ **Independent corroboration:** the string at `lbl_8202B3CC` is
`"vocalist_volume"`, and the RB2 dump declares `static class Symbol
vocalist_volume;` **immediately before this exact function**. Two unrelated
sources agree on a static that neither oracle could supply a body for.

Two spellings that are load-bearing and must not be "tidied":

* **The static Symbol is initialized and NEVER READ** in retail — its only effect
  is the ctor. Written as an unused local static deliberately.
* **The predicate materializes a bool** (`li r11,0/1; clrlwi.; beq`) instead of
  branching straight to the call ⇒ it is an inlined helper returning `bool`. A
  raw `if (a || b || c)` branches directly and never builds the 0/1.

`HandleDeactivateVolume` (retail `fn_826E50C0`, 72 B) is declared and left an
**empty out-of-line stub — its body is NOT invented.**

## Predicted vs measured

| | predicted | measured |
|---|---|---|
| Δ`matched_code` (revert) | −4,900 B | **−5,296 B** ✗ |
| Δ`matched_functions` (revert) | −9 | **−11** ✗ |
| Δhonest | — | −1 |
| Δcode% | — | −0.051314 pp |

**Both misses have one explanation.** I deducted −396 B for an apparent
regression in `_M_insert_overflow_aux<vector<MicClientID>>` (100 → 97.98,
98/98 instructions **equal**, losing only to 8 bytes of trailing COMDAT
alignment padding = two `<illegal>` `0x00000000` words). **That regression does
not exist in the graded build**: −5,296 is exactly 4,936 + 9×40, with nothing
deducted.

⚠ **The lesson generalises and is worth reusing:** my pre-check built
`VocalPlayer.obj` **directly** (`ninja <one obj>`), so the **six post-compile obj
patcher steps never ran** (`patch=6` appears in every real A/B leg). A targeted
single-obj build plus a hand-run `objdiff-cli` can **manufacture a phantom
regression** the graded report does not have. Price from a settled A/B, not from
a one-obj rebuild.

Also mis-predicted upward: **9** funclets crossed, not the 7 I counted from the
clean-`0xf0/0xe0` signature — `fn_826E95D0` and `fn_826E9670` carried a *second*
charge each and still crossed once the frame matched.

## The wall: `Poll@VocalPlayer` is NOT collectable, and the block is not the frame

`?Poll@VocalPlayer@@` (3,388 B, fuzzy 93.82645) is short `0x10` the same way
(`-0x1c0` vs `-0x1d0`, retail `__savegprlr_14` vs our `_15`), but **its prize is
unreachable in this lane for a reason that has nothing to do with the frame.**

`matched_code` keys on `fuzzy == 100`, all-or-nothing. Poll carries **10 real
name charges** (verified with W19's correction — only a **bare** `arg:{Symbol}`
counts; `arg:{Register,Symbol}` is charged by the register):

* **2 are the frame itself** — `__savegprlr_14`/`__restgprlr_14`, fixed by fixing the frame.
* **8 are ICF fold-survivor names**, and **our source is CORRECT in every one**:

| retail (ICF survivor) | ours |
|---|---|
| `vector<Dep*>::reserve` | `vector<int>` / `vector<VocalPart*>` / `vector<Singer*>` |
| `vector<ChatReceiver*>::push_back` | `vector<VocalPart*>` / `vector<Singer*>` |
| `vector<float*>::_M_fill_insert` | `vector<float>` |
| `remove_if<FilePath*, bool(*)(…)>` | `remove_if<VocalPart**, mem_fun_t<…>>` |

All are 4-byte-element instantiations that genuinely fold. ⇒ **No source change
can close them** — they need *proven* aliases (T1, retail-byte identity), which
is a separate lane by standing policy: an unproven alias lifts the score **by
construction** and the `none` control **cannot** catch a fabrication.

⇒ **Poll's own 3,388 B is unreachable here.** The only Poll-adjacent prize is its
**4 EH funclets = 160 B** (`fn_826EBD6C/BD94/BDBC/BDE4`, sole diff
`subi r31, r12, 0x1d0` vs `0x1c0`), which need only the frame.

### Why the frame was not chased for that 160 B

Poll is a genuine multi-defect body (50 hard diffs, ≥4 independent causes), and
**every instruction fixed buys 0 bytes and 0 functions** unless *all* of them go
(mpn 100 needs all 50 hard + 6 immediates). Diagnosed causes, left as diagnosed:

* **Constant handling.** Retail holds `lbl_820F14B4`'s **address** in
  callee-saved **`r22`** and reloads the float 3×; we materialize
  `__real@c47a0000` (= −1000.0f) once into `f30` and `fmr`. That extra live GPR
  is the plausible `__savegprlr_14` itself — retail spends a GPR to avoid an FPR.
* **`srawi` vs `clrrwi` swapped between two sites** (496/501 vs 783) — genuinely
  different operations (`>>2` arithmetic vs `& ~3`), a real logic difference.
* **Branch polarity + argument swap** at 803–805 (`beq`/`bne` with
  `mr r4,r17`/`li r4,0x1` exchanged) — an if/else inversion.

⛔ **REFUTED here — do not re-open as "a missing local at `0x78`".** The first
reading of retail's five extra stores to `0x78` (`stw r11` ×2 at 118/120,
`stfs` ×3 at 568/573/580) looks exactly like a local variable we lack. **It is
not.** `0x78` is a **shared compiler scratch slot used identically by both sides
in 23 places** (it is the `fctiwz`/`stfd`/`lwz 0x7c` float→int conversion
temp — `stfd f0, 0x78(r31)` is `equal` at 184/598/651/663/770). Tracing each
extra store: 118/120 are overwritten by the `stfd` at 184 before any read;
568/573/580 are overwritten at 598 and each is consumed from the **register** by
the immediately following `fcmpu`. **All five are dead stores retail failed to
eliminate and we correctly elide** — our output is *shorter* than retail's, the
same class W19 refuted on `Poll@VocalTrack`. Not source-addressable.

### What it would take (sized)

1. **Prove the 8 STL folds on retail bytes** (T1, `tools/alloc_fold_gate.py`
   machinery) — without this, Poll pays **nothing** no matter how good the body is.
2. Then close ~50 instruction diffs across ≥4 independent causes.
3. The frame/`r22` question is regalloc-adjacent; **permuter is OFF** by directive.

Prize if all three land: 3,388 B + 160 B. Step 1 gates the other two.

## Deliberately NOT done

* **No alias added to `symbol_aliases.json`, no map edit.** Same reasoning W19
  gave; nothing here changes it.
* **`fn_826E5E98` (244 B) and `fn_826E50C0` (72 B) left unpaired at 0%.** The map
  does not name them, so porting `OnMsg` collects **no bytes for the function
  itself** — the +4,936 B is entirely `Handle`'s. Naming them is a *bet* under
  `name_check` (zero call-site upside, real downside) and was not taken.
* **`UpdateScrolling` (8,948 B) not opened** — still a body rewrite, still
  carries real name charges, unchanged from W19's assessment.
* **Poll's frame not attempted**, for the sized reason above.
