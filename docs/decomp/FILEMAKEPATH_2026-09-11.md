# Lane W7-A — `FileMakePath`, and the rest of `default/File`

**Branch** `w7-filemakepath` · worktree `~/tmp/wt-w7-a` · base `3a6bfe40`
(`git merge-base --is-ancestor 3a6bfe40 HEAD` asserted before the first edit).

**Result: +9 matched functions / +368 B / +0.003592 pp**, in two measured
steps. `default/File` went **38 → 47** matched rows.

| | matched | matched_code | code% | masked_equal | honest |
|---|---|---|---|---|---|
| baseline (full settled build) | 42,676 | 3,859,912 | 37.672540 | 22,937 | 19,739 |
| final | **42,685** | **3,860,280** | **37.676132** | 22,946 | 19,739 |

**Anti-vacuity check first** (FOLDPROVE-2): the worktree was FULLY BUILT before
any name-keyed work. `verify_objs_patched.py --verify-manifest` returned
**exit 0** over 1,205 decomp + 3,084 target objects
(`tree_sha256=15806cc3d9ffdd35`), and the target `File.obj` was confirmed to
carry 47 mangled symbols including `FileMakePath`. A reflinked tree reads every
retail name as absent, and that failure agrees with a "this name does not
exist" prior.

**Both briefed figures reproduced to the digit before anything was built on
them**: `FileMakePath` fuzzy **5.641414** (792 B), `RecursePatternInternal`
fuzzy **85.865470** (892 B).

---

## 1. `FileMakePath` — the diagnosis is BODY, not inline budget

The brief asked to test W6-A's headline first: `?DataInitFuncs@@` was
misaligned because **MSVC's inline budget stopped at site 69 of 154**. That
hypothesis is **REFUTED here**, and it is refuted three ways that are all
independent of any inlining question.

W6-A's disease has a distinctive signature: a *small* helper (13 instructions)
inlined at 69 of 154 sites inside a *big* caller, so there is a long **aligned
prefix** (897 instructions, 897/69 = 13.0 exactly) and a high fuzzy (71.45).
`FileMakePath` has none of that. It reads **181 of 200 target instructions as
`delete`**, frame `-0x70` against retail's `-0x200`, and diverges at
instruction 1. Our body was a 19-instruction wrapper:

```cpp
const char *FileMakePath(const char *root, const char *file) {
    MainThread();
    static char static_buffer[256];
    return FileMakePathBuf(root, file, static_buffer);
}
```

This is **W5-C's shape, not W6-A's** — "retail's source has no `*Buf` helper":

1. **Retail calls the ONE-arg `FileGetDrive`.** The call is
   `mr r3,r31 ; bl fn_82516680` — **`r4` is never set**. Our helper calls the
   two-arg `FileGetDriveBuf(file, driveBuf)`.
2. **Retail's frame has no room for `char driveBuf[256]`.** `0x200` is exactly
   `0x50` saves + `0x80` `dirs[32]` + `0x100` `buf[256]` + `0x30` param area;
   a `driveBuf` would force `>= 0x300`.
3. **`buffer` IS the static, not a parameter.** `r28 = lbl_82CCA4B0` is loaded
   unconditionally and the aliasing checks compare against `r28` / `r28+0x100`.

⇒ `__forceinline` could not have produced retail's body however well it worked,
so the W6-A lever is not merely unhelpful here, it is **inapplicable**. (MSVC
at `/O1` will not inline a 198-instruction helper anyway — W5-C measured that
limit at ~40.) A fourth, cheaper check settles the signature: **`r5`/`r6` appear
only as `mr` destinations feeding `sprintf` and are never read**, so retail's
`FileMakePath` is **2-arg**, matching our `File.h` — the same "rb3-Wii carries
an extra buffer param that retail-Xbox does not" that W5-C found on
`FileGetBase`.

The three string literals decoded out of `band.exe` confirm the body exactly:
`lbl_82074E28 = "%s/%s"`, `lbl_82087C60 = "%s:%s"`, `lbl_820116D8 = "/"`.

### 1.1 `FileMakePathBuf` is KEPT — unlike W5-C's `FileGetBaseBuf`

W5-C could contemplate deleting its helper because it had **zero** callers.
`FileMakePathBuf` has **five live ones** — `DirLoader.cpp` ×2, `File_Win.cpp`,
and the native port's `File_Native.cpp` ×2 — so the two bodies are deliberate
duplicates with a comment saying so. Retail's `File.cpp` span contains **no**
out-of-line 3-arg helper at all (the only 792 B function is `FileMakePath`
itself; the neighbouring `fn_82516E28` has a `-0x80` frame, far too small for
`buf[256]` + `dirs[32]`), so this is a DC3 refactor we carry for our own
callers, not something retail had.

### 1.2 One codegen lever HIT: the sixth callee-saved register

First compile of the ported body: **92.3% raw**, with a uniform
`r28/r29/r30 -> r27/r28/r29` shift across **63 arguments** and one
`insert: mr r30, r27`. Retail saves **r27-r31** (five); we saved six.

Cause: `char *c = static_buffer;` hoisted above the branch chain keeps the
initial value live across the whole chain. Retail assigns `c` on **every** path
— `.L_82347E5C: mr r31,r28` is a join-point assignment — so it coalesces `c`
into the register that held the now-dead `file`. Assigning on every path
instead: **92.3 → 94.5**, and the whole register shift dissolved.

### 1.3 ⛔ One lever REFUTED, four ways: the peeled `strtok` call

Retail emits **one** `strtok` call site, entered by `b .L_82347EE4`, with
`.L_82347EE0: li r3, 0x0` feeding NULL on the back edge. We emit **two** —
a peeled first call (`mr r4,r29 ; bl strtok ; cmplwi r3,0 ; beq`) plus the loop
call. Four source spellings were compiled and measured **byte-identical**
(203 instructions, same 33 mismatches at the same indices, every time):

| # | spelling | result |
|---|---|---|
| 1 | `p = strtok(c,"/"); while (p) { …; p = strtok(0,"/"); }` | 94.5 |
| 2 | `while ((p = strtok(arg,"/")) != 0) { …; arg = 0; }` | 94.5 |
| 3 | `for(;;) { p = strtok(arg,"/"); if (!p) break; …; arg = 0; }` | 94.5 |
| 4 | `goto` into a `do { … } while (p)` — **cannot** be guard-duplicated at source level | 94.5 |

Each was confirmed to have actually recompiled (obj mtime 3–4 s newer than the
source) before the negative was recorded — otherwise this is an
absent-vs-absent vacuity that agrees with whichever prior you hold.

⇒ **MSVC normalises the CFG and re-derives the peel regardless of how the loop
is written.** The loop shape is not the lever. Do not re-spend budget here; if
someone reopens `FileMakePath`, the remaining 33 mismatches are this peel
(~4 instructions), a consequent shift of the delimiter `mr r4,r29`, and a tail
where retail copies `c` into **r11** at `[161]` and uses r11 as the cursor in
both branches while we advance r31 directly.

### 1.4 Measured: Δ0, as pre-registered

Pre-registered: *fuzzy ≥ 90; bytes **bimodal** +792 or +0.*
Measured: fuzzy **5.641414 → ~94.5**, **Δmatched +0 / Δcode_bytes +0 /
Δcode% +0.000000pp / Δfuzzy +0.006935pp**, 0 units fell off 100.

The +0 is the pre-registered lower branch — `matched_code` is all-or-nothing
per row and 94.5 ≠ 100. **Landed on accuracy**: the body is now retail's actual
algorithm instead of a DC3 refactor reached through a retail-absent
`MainThread()`.

⛔ It also **refutes my own secondary hypothesis**. I predicted possible upside
from the ~12 callers: our 19-instruction wrapper was inlinable where retail's
callers all emit a plain `bl fn_82516B10`, which is exactly W5-C §5.1's
mechanism. **Nothing moved** — they were not inlining it. `?OnFileAbsolutePath@@`
was already at fuzzy 100 with a real `bl fn_82516B10` on both sides, which in
hindsight was the tell I had in hand before predicting.

---

## 2. `RecursePatternInternal` — 85.87 → 99.9776, five defects, +9 fns / +368 B

892 B, and the shape is the **inverse** of `FileMakePath`: retail saves
**r22-r31** where we saved **r23-r31**, with a `-0x10` frame delta and 20
deletes — retail had code we did not.

| step | fuzzy | what |
|---|---|---|
| baseline | 85.8655 | |
| a | 86.9 | drop spurious `MainThread()`; use the 1-arg `FileGetPath` |
| b | 98.6 | recompute `dirs.size()`; conditional-expression String temporary |
| c | 99.6 | `pttnLen` is `length()`, not `length()-1` |
| d | **99.9776** | backward walk runs to `-1`; its test stays `pos > 0` |

**(a)** Retail's body makes **zero** `bl fn_824A4C10` and exactly **one**
`bl fn_82516550`. W6-A adjudicated this `MainThread()` site SPURIOUS; that
reproduced literally. `FileGetPath` owns its own static (`lbl_82CCA0B0`), so
our local `static char pathBuf[256]` was ours alone.

**(b)** Two fixes, and the second is the big one:
* Retail **recomputes `dirs.size()` every iteration** — the loop test is
  `lwz 0x74(r31); lwz 0x70(r31); subf; divw r11,r11,r27; cmplw`, i.e.
  `(end-begin)/12`, **inside** the loop. Hoisting it into `numDirs` gives a
  countdown (`subic. r30,r30,1`) instead.
* `dirStr` is assigned from a **conditional expression whose arms are String
  temporaries**, not by two plain assignments. The tell is MSVC's
  **conditional-destruction bitmask at `0x54(r31)`** — `li r30,0x1` /
  `li r30,0x2` / `rlwinm.` tests / `rlwinm` clears — plus
  `??0String@@QAA@PBD@Z` (construct) where we emitted
  `??4String@@QAAAAV0@PBD@Z` (assign). Two assignments need no temporary and
  emit no bitmask at all; that was clusters 4, 9 and 10 — 11 target-only
  instructions. **This one fix also dissolved the prologue mismatch, the frame
  delta and every register swap**, which is why the earlier
  `PROLOGUE_MISMATCH` / `REGISTER_SWAP` labels were symptoms rather than
  diagnoses — the same lesson CLAUDE.md records for `REGISTER_SWAP`.

**(c)** We kept `length()-1` and paid an extra `subi r28,r11,0x1`, flipped
strictness (`ble`/`bgt` where retail has `blt`/`bge`) and an extra
`addi r6,r11,1` on the substr count.

**(d)** ⚠ A trap worth carrying: the loop and its test take **different
bounds**. Retail's back-edge is `subic. r30,r30,0x1` / `bge` (it walks to −1,
so `pttn[0]` IS examined) but its final test is still `cmpwi cr6,r30,0x0` /
`bgt` (`pos > 0`). I flipped **both** together, which re-inverted the test and
cost a round trip. The two are behaviourally equivalent either way — with
`pos > 0` the loop stops at 0 and with `pos >= 0` it stops at −1, and both
reach the `"."` branch — so only retail's bytes distinguish them.

⚠ **rb3-Wii DISAGREES with retail on three of these five.** `File.cpp:589`
hoists `numDirs`, uses `length()-1`, and calls the 2-arg
`FileGetPath(pttn.c_str(), 0)`. Retail bytes outrank the oracle — the standing
rule demonstrating itself, in the same file where W5-C first hit it.

### 2.1 The measurement MISSED, favourably, and the miss is the finding

Pre-registered: *Δmatched +1 (mpn crosses 100), Δcode_bytes +0.*
Measured: **Δmatched +9 · Δcode_bytes +368 · Δcode% +0.003592pp ·
Δmasked_equal +9 · Δhonest +0 · Δfuzzy +0.001782pp**, 0 units fell off 100.

Fully attributed, and it sums exactly:

| rows | move | bytes |
|---|---|---|
| `fn_825181A4` `fn_825181CC` `fn_82518234` `fn_8251825C` `fn_825182AC` `fn_825182D4` | 99.9 → 100 | +240 |
| `fn_825181F4` | 95.6875 → 100 | +64 |
| `fn_825182FC` | **0.0 → 100** (previously unpaired) | +64 |
| `fn_82518284` | mpn 99.9 → **100** (fuzzy only 99.5) | +0 |
| `?RecursePatternInternal@@` | 85.8655 → **99.97758** on BOTH rulers | +0 |
| | | **+368 B / +9 fns** |

⛔ **CORRECTION, caught by reading `report.json` instead of the tool's
display: `?RecursePatternInternal@@` crossed NEITHER ruler.** Its mpn is
**99.97758**, identical to its fuzzy — yet `run_objdiff` printed
*"Match: 100.0% canonical"*, because that display **rounds**. All **nine**
matched functions are therefore anonymous rows in `default/File`, and the row
this lane actually worked contributes **zero to both headline measures**.
CLAUDE.md's rule — *never trust a displayed 100, read `fuzzy_match_percent`
from `report.json`* — fired on me here, and it also invalidated the +1 I had
pre-registered *for the right reason but on a mis-read ruler*.

**Δmasked_equal is also +9, so every one is a funclet byte-signature pairing,
not an honest match** (Δhonest +0). Reshaping the String lifetimes changed
`File.obj`'s EH funclet population and moved it **toward** retail's — the same
collateral W6-A measured running the other way (−160 B) and W4-A saw in
DirLoader. Predicting +1 was reasoning about the named row only and **ignoring
the funclet channel entirely**; that channel has now been observed moving in
both directions in consecutive lanes and should be part of any File.cpp
prediction.

### 2.2 ★ The `5/N` screen closed to the digit

One relocation-name charge costs `5/N` pp with `N = size/4`. For this row
`N = 892/4 = 223`, so one charge = `5/223 = 0.022422` pp ⇒ predicted
**99.977578**. Measured **99.9776**. ⇒ **exactly one charge remains**, and it
is worth the full **892 B** *plus* the row's own `+1 matched function`, since
mpn is pinned to the same 99.97758 by that same charge. This is the instrument the brief prescribed, and
it is what makes §4's handoff a priced prize rather than a hope.

---

## 3. The `MainThread()` per-site adjudication — retail bytes, one site at a time

The brief was explicit that W6-A's audit was **3 of 4 spurious, ONE GENUINE**,
and that the prior must not be applied. Each surviving site was adjudicated
independently, keyed on the enclosing `.fn` symbol (never the synthetic address
column):

| our function | retail | `bl fn_824A4C10` | verdict | action |
|---|---|---|---|---|
| `RecursePatternInternal` | `0x82517E28` | **0** | SPURIOUS | **removed** (§2a) |
| `FileMakePath` | `0x82516B10` | **0** | SPURIOUS | **removed** (§1) |
| `FileRelativePath` | `fn_82517718` | 0 | SPURIOUS | not touched (§5) |
| `NewFile` | `fn_825173E0` | **1** | **GENUINE** | not changed (§3.1) |

### 3.1 `NewFile` — GENUINE, but not the shape our source has

Reproduced literally. Retail's `fn_825173E0` does call it, and **the very next
instruction is `cmplwi cr6, r28, 0x0`** — testing the *filename* saved from r3
at `mr r28, r3`, not the return value in r3. The result is **discarded**: the
`MILO_ASSERT(cond,line)` ⇒ `((void)(cond))` shape. Our source instead has
`if (!MainThread()) { TheDebug.Notify("NewFile(%s) from MainThread()"); }`.

I re-ran W6-A's string check rather than inherit it: in `band.exe`,
`"NewFile(%s) from MainThread"` occurs **0** times, `"from MainThread"` **0**,
and the bare substring **`NewFile` occurs 0 times** — so there is no debug
string for this site at all. `TheDebug.Notify(...)` is a plain call, **not** a
`MILO_*` macro, so unlike `MILO_ASSERT` it is **not** compiled out of the match
build: our body really does carry a `bl` and a string literal that retail lacks.

**Deliberately NOT changed**, for the reason W6-A gave and which I confirmed in
the baseline: `fn_825173E0` reads **fuzzy 0.000000** — it is **unpaired** (no
map name), so any edit is Δ0 and unmeasurable, while File.obj body changes
demonstrably shuffle anonymous funclet pairings in *both* directions (§2.1,
and W6-A §3.1's −160 B). Changing a body you cannot measure, in a TU whose
collateral channel is live, is the trade W5-C's step 0 warns about. The
correct edit is recorded in §5 H3 so it can be made *with* the map name.

---

## 4. Handoff: `RecursePatternInternal`'s last charge is an ALIAS, not source

The single remaining charge at `[187]` is:

```
TGT: bl ??1?$vector@UNode@?$ObjPtrVec@VObject@Hmx@@VObjectDir@@@@V?$StlNodeAlloc@…@Z
SRC: bl ??1?$vector@VString@@V?$StlNodeAlloc@VString@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ
```

`scripts/symbol_aliases.json` **already carries a group at `0x822d8cc0`** —
which is exactly the `bl fn_822D8CC0` in retail's own call inventory — whose
survivor is the target spelling and which already folds
`??1?$vector@V?$ObjDirPtr@VObjectDir@@@@…` on **T1** evidence. Our
`vector<String>` spelling is simply **not a member**.

The circumstantial case is strong and non-circular: retail's *own* body
constructs `std::vector<String> dirs(gDirList)` at `[132]` via
`??0?$vector@VString@@…` — an instruction objdiff scores **equal on both
sides** — and then destroys that same object at `[187]`. A destructor reached
from a `vector<String>` copy-construct, spelled `vector<Node<ObjPtrVec>>`, is
what ICF folding looks like; and the group's existing member proves this
family folds.

⛔ **NOT installed by this lane.** CLAUDE.md is explicit that an *unproven*
alias lifts `name_check` **by construction**, that the `none` control **cannot**
catch a fabricated one, and that hand-adding a group the sanctioned gate never
admitted is an integrity hazard — W5-C refused exactly this and W6-A installed
only after quoting both COMDATs' bytes and relocations. The right instrument is
`tools/ourside_fold_sweep.py` (which walks live `name_check` charges, and which
W5-C repaired) or `tools/icf_alias_build.py`; the end of a lane, without budget
to T1-verify and re-gate, is the wrong place to add group 1,593.

**Priced exactly: 892 B, one charge, `5/223` confirmed to the digit.**

---

## 5. `fn_82517718` = `FileRelativePath` — identified, NOT named, and now COSTED

W6-A's identification **reproduced**: `?OnFileRelativePath@@` at `0x82517B48`
calls `fn_8274B000` twice (two `DataNode::Str` args), then `bl fn_82517718`,
then `fn_8274AA08` — a two-string argument list returning a string.

The brief asked to name it *if the caller population makes that non-negative*.
**I measured that population, which W6-A did not have**: `bl fn_82517718`
appears at **35 sites across 30 distinct enclosing functions in 22 files**
(Dir ×4, CubeTex ×7, Tex ×2, Movie ×2, TexMovie ×3, FileMerger ×2, plus
UIComponent, UIPicture, UIFontImporter, PanelDir, Overlay, Screenshot,
SpeechMgr, SynthSample, MoggClip, LightHue, Instance, PropSync, CharBoneDir,
CharClipSet, Utl, File).

⇒ **Do not name it yet.** Under `name_check`, naming an anonymous address
converts **35 currently-forgiven placeholder call sites into checked ones** in
one step (CLAUDE.md: naming is a *bet* whose payout is bug exposure, not
bytes). The upside is one pairing (+1 honest, 0 bytes) on a 904 B row that
**cannot reach 100 today anyway** — our `FileRelativePath` is the same
collapsed wrapper `FileMakePath` was (`MainThread(); … return
FileRelativePathBuf(root, filepath, relative);`) against a 904 B target.

**The correct order is therefore: body-port first, name second.** §1 is the
worked template — and at 904 B this is now the largest priced prize left in
`default/File`.

---

## 6. Per-step predicted vs measured

Both legs settled, `functionRelocDiffs=name_check`, via
`tools/ab_measure.py --from-dirty`; one change per run; each committed
immediately after measuring.

| # | step | kind | predicted | measured | |
|---|---|---|---|---|---|
| 1 | `FileMakePath` body port | source | fuzzy ≥ 90; bytes **bimodal** +792 or +0 | fuzzy **5.641414 → 94.5**, **Δ0 / Δ0** | ✓ (lower branch, as pre-registered) |
| 2 | `RecursePatternInternal` ×5 fixes | source | +1 fn / +0 B | **+9 fns / +368 B** | ✗ §2.1 — right total sign, wrong mechanism: the +1 I predicted did NOT happen and 9 unmodelled funclet rows did |
| | **lane total** | | | **+9 fns / +368 B / +0.003592 pp** | |

Sub-predictions inside step 1, all measured: the `c`-hoist lever **hit**
(92.3 → 94.5); the `strtok` loop-shape lever **refuted 4×**; the
caller-inlining upside **refuted** (Δ0).

---

## 7. What this lane did NOT do

* ⛔ **Did not reach 100 on `FileMakePath`** (94.5%, 33 mismatches). The peel in
  §1.3 is not source-reachable across four spellings, and `matched_code` is
  all-or-nothing, so the row pays nothing until *all* of it closes. Landed for
  accuracy with that stated.
* ⛔ **Did not install the `vector<String>` alias** into group `0x822d8cc0`
  (§4) — 892 B, proven-adjacent but not T1-verified by me, and ungated hand
  installs are the documented integrity hazard.
* ⛔ **Did not name `fn_82517718`** (§5). Now costed at 35 sites / 30 callers,
  which is the input the decision needed; the body port should come first.
* ⛔ **Did not change `NewFile`** (§3.1) — genuine `MainThread()`, wrong shape,
  but the row is unpaired so the edit is unmeasurable and the funclet
  collateral is live.
* ⛔ **Did not delete `FileMakePathBuf`** — unlike W5-C's `FileGetBaseBuf` it
  has five live callers including two in the native port (§1.1).
* ⛔ **Did not touch `splits.txt`, `symbols.txt`, `target_symbol_map.json`,
  `symbol_aliases.json`, `.pdata`**, and did not rebuild jeff, objdiff, wibo or
  objcache. This branch changes exactly one file: `src/system/os/File.cpp`.
* ⚠ **Did not re-examine `MainThread()` outside `File.cpp`.** W6-A's tree-wide
  scan found 50 callers, 21 in `Splash.s`; none was checked against our source.
* ⚠ **Did not revisit `FileGetPath` `0x82516550`'s carving** (W6-A §6/H2). It
  is a jeff over-carve, W6-A measured the pin workaround dead, and nothing here
  changes that.

## 8. Handoffs, in the order I would fund them

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **`vector<String>::~vector` into alias group `0x822d8cc0`** — closes `RecursePatternInternal` to 100 | §4; priced **892 B + 1 fn**, exactly one charge, `5/223` confirmed to the digit; group already exists on T1 with a sibling member. Today the row scores 99.97758 on BOTH rulers and pays **nothing** |
| H2 | **`FileRelativePath` `fn_82517718`: body-port THEN name** — 904 B, largest remaining in the unit | §5; identification reproduced, caller population costed at 35 sites / 30 callers, §1 is the worked template |
| H3 | **`NewFile` `fn_825173E0`** — replace the `Notify` branch with `MILO_ASSERT(MainThread(), …)`; needs a map name first to be measurable | §3.1; retail discards the result, and `NewFile` occurs **0** times in `band.exe` |
| H4 | **`FileMakePath`'s last 33** — the `strtok` peel + the r11 tail cursor | §1.3; four spellings refuted, so this needs a different mechanism, not another rewrite |

## 9. Reusable lessons

* **Two "collapsed wrapper" rows can have different diseases.** W6-A's
  inline-budget finding and W5-C's no-helper finding produce similar-looking
  low scores; the discriminator is cheap and structural — **count the argument
  registers at the call site** (`r4` unset ⇒ the one-arg form) and **do the
  frame arithmetic** (`0x200` leaves no room for a second 256-byte local).
  Neither needs a build.
* **A compiler decision can be invariant to every source spelling of it.** Four
  `strtok` loop forms, including a `goto` that cannot be guard-duplicated at
  source level, produced byte-identical code. Record the negative with the
  recompile proof, or the next lane rewrites the loop a fifth time.
* **`PROLOGUE_MISMATCH` and `REGISTER_SWAP` were symptoms again.** An extra
  callee-saved register, a `-0x10` frame delta and 29 swapped instructions all
  dissolved when one *source* construct was corrected — a conditional
  expression with String temporaries. Do not defer a row as regalloc-bound on
  those labels.
* **The oracle lost three times in one function.** rb3-Wii disagrees with retail
  on `numDirs`, `length()-1` and the `FileGetPath` arity. Retail bytes outrank
  the oracle, and here the oracle was wrong about the majority of the fixes.
* **Predict the funclet channel, not just the named row.** Two consecutive
  lanes have now measured File.obj's anonymous EH funclets re-pairing as
  collateral — W6-A at −160 B, this lane at **+368 B and 8 of the 9 crossings**.
  A prediction that models only the row you edited will keep missing.
* ⛔ **`run_objdiff`'s percentage ROUNDS, and 99.97758 prints as `100.0%
  canonical`.** I recorded a false mpn crossing from it and only caught it by
  re-deriving the +9 from `report.json`'s archived A/B legs. An A/B's Δmatched
  being *correct in total* does not mean your attribution of it is — check
  which rows moved, on the ruler that counts them.
* **The `5/N` screen turns a residual into a price.** 99.9776 on a 892 B row is
  not "nearly done, unclear what is left" — it is *exactly one relocation-name
  charge worth exactly 892 B*, which is what makes H1 fundable.
