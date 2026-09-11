# Lane W6-A — `?DataInitFuncs@@`, and the three W5-C handoffs

**Branch** `w6-datainitfuncs` · worktree `~/tmp/wt-w6-a` · base `50fd112d`
(`git merge-base --is-ancestor` asserted before the first edit).

**Result: +3 matched functions / +8,040 B / +0.078468 pp**, in four measured
steps. `?DataInitFuncs@@` — 8,068 B, the largest single row in these records —
went **71.4467 → 100.000** and crossed.

| | matched | matched_code | code% |
|---|---|---|---|
| baseline (full settled build) | 42,658 | 3,851,188 | 37.587395 |
| final | **42,661** | **3,859,228** | **37.665863** |

**Anti-vacuity check first** (FOLDPROVE-2): the worktree was FULLY BUILT before
any name-keyed work; `verify_objs_patched.py --verify-manifest` returned
**exit 0** over 1,205 decomp + 3,085 target objects, and the target
`DataFunc.obj` was confirmed to carry **211** mangled symbols. A reflinked tree
reads every retail name as absent, and that failure agrees with a "this name
does not exist" prior.

---

## 1. The misalignment: MSVC's inline budget stopped at site 69 of 154

W5-C handed this over as *"8,068 B at fuzzy 71.45, badly misaligned"* — 445
insert/delete instructions in 133 clusters. Every briefed figure reproduced to
the digit (`fuzzy 71.4467`, `mpn 72.94398`, `size 8068`).

**The list is not the defect. Retail's list is OUR list.** Decoding retail's
string table out of `band.exe` through the PE section table and diffing the
ordered registration strings against our source gives a single `equal 0..147`
opcode — **zero edits**. Retail makes **154** Symbol-ctor calls and **154** map
inserts; our source is 147 string literals + 7 `magic` key-deriv
registrations = 154, and the `magic` block is present in retail too (`li`
immediates `'O'`,`'6'`,`'4'`..`'9'`,`'7'`,`'0'`, one each).

The defect is **inlining**. Retail inlines `DataRegisterFunc` at all 154 sites,
13 instructions each:

```
lis r10, <str>@ha ; stw r11, 0x0(r3) ; addi r3,r1,0x54 ; addi r4,r10,<str>@l
bl ??0Symbol@@QAA@PBD@Z          <- Symbol ctor into the temp at 0x54
lwz r11, 0x0(r3) ; mr r3, r30    <- r30 = &gDataFuncs
addi r4,r1,0x50 ; stw r11, 0x50(r1)
bl ??A?$map@VSymbol@@...@Z       <- map::operator[](const Symbol&)
lis r11, <func>@ha ; addi r11,r11,<func>@l ; stw r11, 0x0(r3)
```

We inlined the first **69** and then emitted `bl ?DataRegisterFunc@@` for the
remaining 85 — MSVC's per-caller inline budget exhausting mid-function.

**The arithmetic is exact, not impressionistic.** The aligned prefix is idx
5..901 = **897 instructions**, and 897 / 69 registrations = **13.0 exactly**;
cluster 2 opens precisely at registration **#70** (`"random_float"`, line 1722).

Our `DataRegisterFunc` carries a `find()` + `MILO_FAIL` debug check, but
`MILO_*` is `#ifdef HX_NATIVE` and the match build never defines it, so the
body reduces to `gDataFuncs[s] = func;`. That is why the 69 inlined sites
already matched retail's 13-instruction form byte for byte: the budget was the
*only* defect.

`__forceinline` on the definition (one line) took the row to **99.93555** with
**zero** insert/delete instructions remaining.

### 1.1 Correcting W4-A's reading of this function

W4-A recorded *"a `__forceinline` helper DID reproduce retail's distinct 8-byte
Symbol temp slots but at +0.02 pp, with retail's main frame still 0x74 B larger
and one more callee-save … so the frame, not the inlining, is the open
question."* That experiment was a helper for the **7-entry alias/magic block**
and never touched `DataRegisterFunc`. The frame was never the question: our
`stwu r1,-0x100` vs retail's `-0x190`, and our one callee-save vs retail's two
(`std r30` + `std r31`, r30 holding `&gDataFuncs` across all 154 sites), were
**consequences** of the truncated inlining. Both resolved on their own here —
after `__forceinline` the prologue cluster is gone.

### 1.2 The residual was 26 relocation-NAME charges, priced exactly

Zero insert/delete but fuzzy 99.93555, because **an instruction-equality count
does not include argument-level charges**. All 26 were `[sym]` `diff_arg`, in
13 `lis`/`addi` pairs. Wave 4's screen prices one relocation-name charge at
`5/N` pp with `N = size/4 = 2017`, i.e. 0.0024789 pp:

| after | charges | predicted fuzzy | measured |
|---|---:|---|---|
| `e17a776c` | 26 | 99.93555 | **99.93555** |
| `01f87075` | 14 | 99.96530 | **99.965294** (implied count **14.000**) |
| `33bb08bb` | 0 | 100.0 | **100.000000** |

The screen closed to the digit at every step, which is why each prediction was
*collectable* rather than hopeful.

The 26 fell into four groups: floor/ceil (4), quote/quasiquote (8), the
notify family (12), and basename (2).

---

## 2. Two proven map TRANSPOSITIONS — +132 B / +2 fns

`DataFloor`↔`DataCeil` and `DataQuote`↔`DataQuasiquote` were transposed in
`target_symbol_map.json`. In both cases the decisive channel is a callee that
is **already named**, so the reasoning is not circular:

* `fn_8275F9C0` and `fn_8275FA10` are identical but for one CRT call.
  `fn_8282B650` is named **`floor`** and its body is unambiguous
  (`fsub f10,f1,f12` / `fsub f13,f12,f13` / `fsel` — trunc−1, downward);
  `fn_8282C610` is its mirror (`fsub f10,f12,f1` / `fadd f13,f12,f13` — trunc+1,
  upward) = `ceil`. `fn_8275F9C0` calls floor ⇒ **it is `DataFloor`**.
* `fn_82762DB8` calls `fn_82762B58`, which the map already names
  **`?Quasiquote@@YA?AVDataNode@@ABV1@@Z`** ⇒ **`fn_82762DB8` is
  `DataQuasiquote`**. `fn_82760BA8` calls the (unnamed) DataNode copy ctor =
  `return array->Node(1)` = `DataQuote`.

Corroborated independently by retail's **own** registration table: `"floor"`
registers `fn_8275F9C0` and `"quasiquote"` registers `fn_82762DB8`, matching
our source — which was right all along. Blast radius checked: all four
addresses are referenced **only** inside `DataFunc.s`.

★ **`?DataFloor@@` was reading a FALSE 100.0 the whole time.** It pointed at
retail's `DataCeil` body, whose `ceil` callee is **unnamed** — a placeholder,
which `name_check` forgives — so its one differing relocation cost nothing.
Its mirror `?DataCeil@@` pointed at the `DataFloor` body whose `floor` callee
**is** named, so only that half was ever charged. W5-C's `0x82517090` lesson
again, with the extra twist that **which half looks broken is an artifact of
which callee happened to be named.**

---

## 3. The notify family: six spellings, one 4-instruction body

Retail's registry stores **one and the same address `0x8228D358`** at six
registration sites (`notify`, `notify_beta`, `fail`, `notify_once`,
`disable_notify`, `filter_notify`). That body is four instructions —

```
li r11, 0x0 ; stw r11, 0x0(r3) ; stw r11, 0x4(r3) ; blr
```

— a zeroed 8-byte `DataNode`, and `symbol_aliases.json` **already** carried a
group with survivor `??0DataNode@@QAA@XZ` at exactly that address, folding 38
spellings onto it including `?DataNop@@`. A function with a live body cannot
fold onto a 4-instruction constructor, so retail's five ungated bodies were
compiled out. (`DataNotifyBeta` had already been gated by an earlier lane, with
a source comment stating this exact mechanism.)

Gated with the house pattern `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`,
so the **native port keeps every body** and only the match build sees
`return 0`. `#if`/`#endif` balance re-checked at EOF (depth 0) — a stray
directive in a shared file is the `ScatterIncludes` failure mode.

This is **not** the blanket `MILO_DEBUG` removal CLAUDE.md measures at −21: the
evidence is per-function, six independent registration sites, retail bytes.

### 3.1 The gate step MISSED its prediction: −160 B, all funclet churn

Pre-registered **Δ0** (none of the six names has a map address — they folded —
so no row of theirs is pairable, and a body change cannot move a call-site
*name* charge). **Measured −160 B**, Δmatched +0, Δmasked_equal +0.

Fully attributed, and not a code regression. All of it is eight **anonymous
40-byte EH funclet** rows in `default/DataFunc` reshuffling:

| rows | move | bytes |
|---|---|---|
| `fn_82762154` `fn_82761A24` `fn_82761FCC` `fn_8276194C` `fn_827622F0` `fn_827624D0` | 100 → 99.5 | −240 |
| `fn_82760E74` `fn_82760FFC` | 99.5 → 100 | +80 |

40 B is N=10, so 100 − 99.5 = 0.5 pp is **exactly one** relocation-name charge
per row. Removing the `String str;` locals removed their unwind funclets, so
our obj's funclet population moved **toward** retail's and the byte-signature
pairing of anonymous funclets re-shuffled underneath. W4-A recorded the
identical collateral in DirLoader (two 40 B funclets, −80 B).

Kept deliberately: it is the enabler, and the source is truer to retail
regardless of what the alias pays.

---

## 4. The alias decision: INSTALLED, because step 1 made it live — +8,068 B / +1 fn

W5-C proved the `?DataBasename@@` ≡ `?OnFileGetBase@@` fold and **deliberately
did not install it**, because `?DataInitFuncs@@`'s one aligned row touching
either spelling pitched retail's symbol against a base-side **immediate 84**,
which no alias can forgive. **That diagnosis was correct and the blocker was
exactly the misalignment its handoff H3 named.** After `e17a776c` the site is a
symbol-vs-symbol `diff_arg` at idx 1412/1414, and the alias forgives a real
charge.

Both legs were T1-verified on retail bytes **before** writing:

**Leg 1 — six notify spellings onto `??0DataNode@@QAA@XZ` @ `0x8228d358`**
(existing group, folded 38 → 44). Retail's bytes there are
`39600000 91630000 91630004 4e800020`; after the gates each of our six COMDATs
is that exact 16-byte body with **`relocs=[]`** — byte- *and*
relocation-identical, the linker's own `/OPT:ICF` condition. **Nothing is
masked** (there are no relocations), so the flat-T1 vacuity caveat that
understates provability elsewhere cannot apply here. And the identity is
**discriminating, not trivial**: the sibling `??0DataNode@@QAA@PAV0@@Z` is
`39600002 90830000 …` and differs.

**Leg 2 — `?DataBasename@@` onto `?OnFileGetBase@@` @ `0x82517120`** (new group
1592). W5-C's proof reproduced byte-for-byte on this tree: both COMDATs 72 B,
**sha256 `636b3dd852dde8dc`**, identical relocations compared **by target
name** — `(28,?Str@DataNode@@…)`, `(32,FileGetBase)`, `(44,??0DataNode@@QAA@PBD@Z)`.

**On the `ALIAS_SUSPECT` alert.** `ab_measure` fired it on both map-only legs
(name_check up, `none` flat). That is what these patches MUST look like:
forgiveness never creates byte agreement, and a *rename* cannot change
pairability either, so both read `+N` graded / `+0` none. The house rule is
explicit that the control **cannot** catch a fabricated alias and that the
flatness is the **signature, not a clearance** — so the adjudication is the
retail-byte evidence, not the control: exact bytes quoted, relocations compared
by name, a discriminating negative control on a sibling COMDAT, and two
independent registration tables.

---

## 5. Per-step predicted vs measured

All legs settled both sides, `functionRelocDiffs=name_check`, via
`tools/ab_measure.py --from-dirty`; map-bearing legs forced a re-split
(`renamer_patched=1823`) and **both legs reached a `symbols.txt` fixed point**.

| # | step | kind | predicted | measured | |
|---|---|---|---|---|---|
| 1 | `__forceinline DataRegisterFunc` | source | fuzzy ≥95; bytes **bimodal** +8,068 or +0 | fuzzy **71.4467 → 99.93555**, **Δ0 bytes** | ✓ (lower branch, as pre-registered) |
| 2 | map: two transpositions | map | +132 B / +2 | **+132 B / +2** | ✓ exact |
| 3 | gate 5 notify bodies | source | Δ0 | **−160 B / +0** | ✗ §3.1 |
| 4 | install both proven aliases | alias | +8,068 B / +1 | **+8,068 B / +1** | ✓ exact |
| | **lane total** | | | **+8,040 B / +3 fns / +0.078468 pp** | |

Step 1 was measured on a full settled build rather than `ab_measure` (it is
stated that way deliberately); steps 2–4 each had their own A/B run, one change
per run, committed immediately after measuring.

Note the shape of step 1: it pays **nothing** on its own because `matched_code`
is all-or-nothing per row, and step 4 pays everything. They are one coupled
system that cannot be half-landed — W5-C's §5.1 property, appearing again.

---

## 6. `FileGetPath` `0x82516550`: the carving verdict — a pin CANNOT act, measured

W5-C's geometry reproduced exactly: dtk carves the region into
`fn_82516550` (0x34) → `fn_82516584` (0x74) → `fn_825165F8` (0xC) →
`fn_82516604` (0x14), **fully contiguous, zero stranded bytes**, total
**0xC8 = 200 B**. Our compiled `FileGetPath` is **exactly 200 B**, so W5-C's
"our body already matches" is confirmed on size as well as shape.

It is one function beyond argument: `fn_82516550` copies `file` into the static
`lbl_82CCA0B0` and branches to the `'.'` tail on `file == 0`; `fn_82516584`
scans backwards for `/` or `\` and branches to the two tails; **`fn_82516550`
ends on `mr r11,r3 / mr r10,r3` — mid-basic-block, no terminator — and falls
through**, and `fn_82516584` even branches to *itself* (`bdnzf lt,
fn_82516584`). dtk promoted three internal branch targets to function starts.

### 6.1 ⛔ `.pdata` cannot arbitrate this one — the oracle is absent by construction

Retail's `.pdata` (big-endian; `PrologLen:8, FunctionLen:22, 32bit:1,
ExceptionFlag:1`, length in instructions) has a record **ending exactly at
`0x82516550`** and its next record begins at **`0x82516680`**:

```
Begin=0x825164F0  len=0x60   end=0x82516550   (FileNormalizePath)
Begin=0x82516680  len=0x64   end=0x825166E4   (FileGetDrive)
```

So the whole `0x82516550–0x82516680` span (0x130 B = our four symbols **plus**
`FileGetExt`) carries **no `.pdata` record at all** — leaf code touching
neither stack nor LR gets no unwind record. This is CLAUDE.md's sub-`.pdata`
stratum, and it means the authoritative extent oracle is **structurally
unavailable** precisely here. Worth carrying: the instinct to settle a carving
question with `.pdata` fails exactly on the stratum where carving goes wrong.

### 6.2 The pin experiment — measured, and it is a NEGATIVE

The brief asked whether this is one of W5-D's **2/156** sites where a `.text`
pin could act. **It is not one of the 2, and it is not one of the 156 either.**
W5-D's census is defined by *stranded material between `.endfn` and the next
`.fn`*; here there is **zero** stranded material. This is a distinct jeff
defect class:

| | W5-D's class | this site |
|---|---|---|
| shape | truncate, strand 1 instr, re-seed | **over-carve**, contiguous |
| stranded bytes | 1 instruction in 75.6% | **0** |
| successor alignment | 8-byte aligned in 150/156 | mixed (`0x…584`≡4, `0x…5F8`≡0, `0x…604`≡4) |
| shared root | slice ends mid-basic-block | **same** |

Rather than assert the pin verdict structurally, it was **executed**. File.cpp's
block `0x825164A8–0x82516E28` was split into three at **exactly the true
function start and end**, so the middle block contains this one function and
nothing else:

```
.text start:0x825164A8 end:0x82516550
.text start:0x82516550 end:0x82516618
.text start:0x82516618 end:0x82516E28
```

**Result: byte-identical carve, before and after — 5 symbols in
`[0x82516550,0x82516680)` with identical sizes.** A pin that isolates the
function perfectly still cannot merge the fall-through seam. The carve is
decided entirely by dtk's intra-block CFA and **no pin input reaches it**. The
experiment was reverted; `splits.txt` is unchanged on this branch.

⚠ **Two instrument failures were caught inside this one experiment**, both of
the family that produces a confident answer agreeing with your prior:

1. The first run's `ninja` **FAILED** (the split-guard: a `.text` edit makes dtk
   re-derive `.pdata`, so it rewrites its own input; the documented recovery is
   "build again, the retry is a fixed point"). The symbol listing taken from
   that aborted build was worthless. **Every subsequent reading was taken after
   a second, clean, `FAILED`-free build.**
2. The post-pin listing was then filtered with the regex
   `fn_825165[0-9A-F]{2}|fn_8251661[0-9A-F]`, which **cannot match
   `fn_82516604`** (prefix `825166`, suffix `04`). That produced a "4 → 3
   symbols, the pin helped!" reading which was pure regex artifact. Re-done
   with a **numeric** address-range filter, the true answer is "no change".

### 6.3 The jeff handoff, precisely

⛔ **jeff was NOT rebuilt** — `cargo build --release` there overwrites the live
fleet splitter shared with two sibling repos.

* **Symptom:** in `.pdata`-less leaf regions, a forward conditional branch that
  jumps *over* un-analysed code ends the slice; the skipped region is re-seeded
  as a new function, and every internal branch target of the re-seeded region
  becomes another `.fn`. Internal labels are emitted correctly *within* a slice
  (`.L_…` appear inside both halves here), so the bug is at the slice boundary,
  not in label handling.
* **Witness:** `FileGetPath` `0x82516550`, 200 B, carved 4 ways; the branches
  `beq cr6, fn_82516604` (from the first slice, jumping forward over the
  second) and `beq cr6, fn_825165F8` are the promotions.
* **Localisation** (from W5-D, same root): `function_end = slices.end()`
  (`src/analysis/cfa.rs:757`) and the boundary conditions in
  `src/analysis/slices.rs:402-492`.
* **Why it matters:** naming `0x82516550` today would score **52 B of target
  against our 200 B body** — a false near-0 — so the map cannot be repaired
  ahead of the splitter. `FileGetPath` remains correctly **unnamed**.

---

## 7. The `MainThread()` audit — 3 of 4 spurious, and ONE GENUINE

W5-C left four sites unaudited and recorded the prior that *"the four audited
ones were all spurious, so the prior is that more are"*. **The prior is mostly
right and NOT universally right** — which is the point of auditing rather than
assuming.

Instrument: `?MainThread@@YA_NXZ` is `0x824A4C10`; every `bl fn_824A4C10` in the
whole split asm tree was collected, **keyed on the enclosing `.fn` symbol**
(never the synthetic address column). 50 distinct callers tree-wide. **Exactly
one** lies anywhere in `0x82516000–0x82519000`, and the only two `auto_*`
callers are at `0x8273CBFC` / `0x82743BD0`, far from File.cpp — so the "is it
hiding in an unpinned unit?" gap is closed, not assumed.

| src line | our function | retail | verdict |
|---|---|---|---|
| 533 | `FileRelativePath` | `fn_82517718` (§8), not a caller | **SPURIOUS** |
| 615 | `FileMakePath` | `0x82516B10`, **0** `bl fn_824A4C10` in its 229-instruction body | **SPURIOUS** |
| 688 | `NewFile` | `fn_825173E0` — **CALLS IT** | **GENUINE — but see below** |
| 805 | `RecursePatternInternal` | `0x82517E28`, not a caller | **SPURIOUS** |

★ **The genuine one is not the shape our source has.** Retail's `fn_825173E0`
calls `bl fn_824A4C10` and then **discards the result** — the very next
instruction is `cmplwi cr6, r28, 0x0`, testing the *filename*, not the return
value — and the string `"NewFile(%s) from MainThread()"` appears **0 times** in
`band.exe`. So retail evaluates `MainThread()` and throws it away, the
`MILO_ASSERT(cond,line)` ⇒ `((void)(cond))` shape, while our source is
`if (!MainThread()) { TheDebug.Notify(…); }`.

**Deliberately not changed.** `NewFile` is unnamed in the map ⇒ its row is
unpaired ⇒ any edit is Δ0 and unmeasurable, and W5-C's own step 0 (deleting a
`MainThread()` moved `FileGetBase` 29.9 → **5.8**, sign wrong) is the standing
argument against reshaping a body you cannot measure. Recorded as a handoff
with the evidence rather than guessed at.

---

## 8. Two identifications found in passing

* ⛔ **`fn_82517718` is `FileRelativePath`, NOT `FileMakePathBuf`** — refuting
  W4-A's handoff #5 and W5-C §7's candidate. `?OnFileRelativePath@@` at
  `0x82517B48` calls `fn_8274B000` **twice** (two `DataNode::Str` args), then
  `bl fn_82517718`, then `fn_8274AA08` (`DataNode(const char*)`): a two-string
  argument list returning a string is `FileRelativePath(root, filepath)`, not a
  `*Buf` helper. Unnamed in the map today — a naming candidate, but a **bet**
  under `name_check`, and our body still carries the spurious `MainThread()`
  (§7) so it would not reach 100 today.
* **`FileMakePath` is the next member of W5-C's `*Buf` family** — `0x82516B10`,
  **792 B at fuzzy 5.641414**, the same collapsed signature W5-C measured on
  `FileGetBase` (5.833) when a body is a thin wrapper around an out-of-line
  `*Buf` helper that retail does not have. Its spurious `MainThread()` is
  proven absent from retail (§7). This is the largest single priced prize left
  in `default/File`.

---

## 9. What this lane did NOT do

* ⛔ **Did not body-port `FileMakePath`** (792 B at 5.64%). Fully evidenced in
  §8 and it is the obvious next step, but it is a body-port of the same size as
  W5-C's entire lane, and W5-C's step-0 lesson says the sign of a partial fix
  here is not guessable. Left as the top handoff, not started.
* ⛔ **Did not name `fn_82517718` (`FileRelativePath`)** despite identifying it.
  Naming an anonymous address is a bet that converts forgiven placeholder call
  sites into checked ones, our body is known-divergent (§7), and the end of a
  lane is the wrong place to gamble on a map edit.
* ⛔ **Did not change `NewFile`** — §7, unmeasurable because unpaired.
* ⛔ **Did not rebuild jeff, objdiff, wibo or objcache**, did not touch
  `symbols.txt`, did not hand-edit a `.pdata` line. The `splits.txt` pin
  experiment (§6.2) was reverted and this branch leaves `splits.txt`
  **unchanged**.
* ⛔ **Did not touch `?DataInitFuncs@@`'s registration list** — it needed no
  change; retail's list is ours, proven, and that negative is worth as much as
  the fix.
* ⚠ **Did not delete `FileGetBaseBuf`/`FileGetPathBuf`/`FileGetDriveBuf`** —
  W5-C's reasoning stands unchanged (an unreferenced COMDAT is what `/OPT:REF`
  strips, so retail's *source* may still have had them).
* ⚠ **Did not audit `MainThread()` outside `File.cpp`.** The scan incidentally
  shows 50 callers tree-wide with 21 in `Splash.s` and 5 in
  `HamCamTransform.s`; nothing was checked against our source there.

## 10. Handoffs, in the order I would fund them

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **`FileMakePath` body-port** — `0x82516B10`, 792 B at fuzzy 5.641414 | §8; the collapsed-wrapper signature, and its `MainThread()` proven spurious |
| H2 | **jeff: over-carve at internal branch targets in `.pdata`-less leaf code** — distinct from W5-D's truncate-and-strand class; pin workaround measured DEAD (byte-identical carve with the block cut at the exact function bounds) | §6.2, §6.3; `cfa.rs:757`, `slices.rs:402-492` |
| H3 | **`fn_82517718` = `FileRelativePath`** — identified, unnamed, W4-A's `FileMakePathBuf` guess refuted | §8 |
| H4 | **`NewFile` `fn_825173E0`** — retail calls `MainThread()` and DISCARDS it; the Notify string is absent from retail. Needs a map name before it is measurable | §7 |
| H5 | **`?RecursePatternInternal@@`** — `0x82517E28`, 892 B at 85.87%, `MainThread()` proven spurious | §7 |

## 11. Reusable lessons

* **An instruction-equality count is the wrong instrument twice over.** Zero
  insert/delete coexisted with 26 charges, because relocation-name charges are
  argument-level. But the *converse* also held: the charge count, priced at
  `5/N` pp, predicted all three intermediate fuzzy values to the digit. **Price
  from the charge list, never from a mismatch count.**
* **An exact ratio beats a plausible story.** "MSVC gave up inlining" only
  became a diagnosis when 897/69 came out at **13.0 exactly** and cluster 2
  opened at registration #70 on the nose.
* **Which half of a transposed pair looks broken is an artifact of which callee
  happens to be named.** `?DataFloor@@` read a clean 100.0 while pointing at
  `DataCeil`'s body, because its differing callee was an unnamed placeholder.
* **A vacuity can hide inside a correct-looking experiment twice in a row** —
  once as a build that FAILED and was read anyway, once as a regex that could
  not match the symbol whose absence was the finding. Both produced the answer
  I expected. Re-run after a clean build, and filter addresses **numerically**.
* **`.pdata` is not a universal extent oracle.** It is absent by construction
  exactly on the leaf stratum where dtk's carving goes wrong.
* **A prior that has been right four times is still worth testing.** Three of
  four `MainThread()` sites were spurious; the fourth was real, and "assume
  spurious" would have written a bug into `NewFile`.
