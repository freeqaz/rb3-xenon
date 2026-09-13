# `?Handle@CustomizePanel@@` re-priced for the last time, and a FALSE 100 corrected — lane W12-B, 2026-09-13

Branch `w12-customize-and-false100`, worktree `~/tmp/wt-w12-b`, base main
**`96c3a685`**.  Ruler **`name_check` (graded)**, read from `report.json`'s
`provenance.diff_config` (22 keys), not assumed.

Baseline, this worktree's first full build — taken **before** any name-keyed
lookup, because a reflinked tree's target objs are pre-renamer.  The build's own
`CHECK TARGET OBJS RENAMED` step reported **25,528 / 29,043 map names present in
3,083 target objs = 87.9%** (W11-B measured 25,537 / 29,045 = 87.9%), so no
mangled-name lookup below is vacuous.

```
matched_functions     42,818
matched_code       3,886,704 B
matched_code_percent  37.934030
fuzzy_match_percent   49.149975
masked_equal          22,994        honest 19,824
total_code        10,245,956        total_functions 69,219
```

---

## 0. Headline

- **`?Handle@CustomizePanel@@` costs ONE instruction and is worth +1 function
  AND +5,036 bytes — and it is a PRICED REFUSAL, not an opportunity.**  Both
  halves matter, and every previous briefing got exactly one of them right.
- I am the **eighth** lane pointed at this row.  The price has now been derived
  independently four times (W35, W37, L6, me) and reproduces to the last digit
  each time.  **The re-pricing question is closed.  So is the row.**
- **`0x82574348` is `?NewObject@MetaPanel@@`, not `?NewObject@FlowAnimate@@`** —
  corrected.  It read a FALSE 100.0 carried by placeholder forgiveness.  The
  identification is **quadruply** corroborated, including by an in-tree record
  written months earlier that predicted the map defect from the opposite
  direction without knowing this address existed.  Measured **Δ exactly 0** —
  zero headline cost, strictly more accuracy.
- The `RegisterFactory` 2nd-argument screen over the meta_band `Init` at
  `0x82574E20` — **59 registration sites** — now shows **ZERO mis-named factory
  arguments**.  Mine was the last one.
- ⛔ **W11-B's "MetaPanel's six foreign holes smell like ONE cause" does NOT
  survive contact.**  Only one of the six was a defect (mine); two hold named
  symbols whose pinned TU matches their own class and look correct.  §3.

---

## 1. Task 1 — what `?Handle@CustomizePanel@@` actually costs today

### 1.1 The four accounts in circulation

| # | source | claim |
|---|---|---|
| 1 | the original briefing, ×3 lanes | "5,036 B behind exactly **3** mismatches" |
| 2 | RESIDUAL-1, 2026-08-14 | **5** charged sites (3 insdel + 2 `diff_arg` ICF fold-aliases); closing the instructions buys `mpn` 100 and **ZERO bytes** |
| 3 | W11-B, 2026-09-13, handoff 6 | **ONE** insert/delete away; "the largest single-instruction prize I saw all lane… worth a fresh look by someone" |
| 4 | NEAR_CROSSINGS, 2026-09-13 §4.2 | **ONE** `delete`, **DRAINED**, "already closed as unreachable" |

### 1.2 What I measured

Read out of **my own** `report.json`, inheriting nothing:

```
?Handle@CustomizePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z
  unit  default/CustomizePanel
  size  5,036
  fuzzy 99.92057      mpn 99.92057      <- EXACTLY EQUAL
```

And the charged-site list, `run_diff_inspect mode=mismatches ruler=graded`:

```
## Mismatched Instructions (1 of 1259 total)
99.92057% on this ruler · 98.67752% raw
| Idx | Type   | Target                  | Base |
| 530 | delete | `clrlwi   r11, r11, 24` | ---  |
```

**One charged site.**  Two facts sharing no arithmetic agree:

1. `fuzzy == mpn` **exactly**.  `objdiff-core` computes
   `normalized_diff_score = diff_score − arg_diff_score`, and a relocation-name
   arg mismatch is non-immediate, so it is charged to both counters and excluded
   from `mpn`.  Equality therefore **proves `arg_diff_score == 0`** — zero
   `diff_arg` charges.  One reloc charge would be `5/1259 = 0.00397 pp`, plainly
   visible at five decimals; this is not a rounding artefact.
2. The charged-site list itself returns exactly one row, of kind `delete`.

### 1.3 The verdict — which account is true

- **Account 1 is FALSE.**  Not 3 mismatches; one.  It was a `none`-ruler count,
  and a mismatch count is the wrong instrument anyway.
- **Account 2 is FALSE ON THIS TREE — and was TRUE when written.**  FOLDPROVE-1/2
  have since landed both ICF aliases, so they no longer charge; `fuzzy == mpn`
  is the proof.  ⇒ **"closing it buys zero bytes" is stale by a month.**  *A
  retirement is only valid on the tree it was measured on.*
- **Account 3 is TRUE about the price and MISLEADING about the prospect.**  One
  insert/delete is right.  But "worth a fresh look by someone" is what caused
  *this* lane to be dispatched, and it was derived arithmetically from a
  percentage **without reading the ~250 lines of in-file record** in
  `src/band3/meta_band/CustomizePanel.cpp` that had already closed it.
- **Account 4 is TRUE AND COMPLETE.**  One charge, and unreachable.

### ⇒ The statement that should be briefed from now on

> **`?Handle@CustomizePanel@@` is 5,036 B and `fuzzy == mpn == 99.92057`, behind
> EXACTLY ONE charged site: `[530] delete: clrlwi r11, r11, 24`.  Closing it is
> worth +1 function AND +5,036 bytes.  It is NOT reachable from source.  Do not
> open this row.**

Both clauses are load-bearing.  "+1 fn / 0 bytes" (account 2) understates it by
5,036 B and interests nobody; "one instruction from the biggest prize in the
tree" (account 3) overstates the prospect and has now recruited three lanes.
**The row is simultaneously the most valuable single instruction in the binary
and unreachable — that combination is why it keeps getting re-opened.**

### 1.4 Why it is unreachable — and why I did not try a 23rd spelling

I changed nothing here.  The in-file record spans **seven** prior lanes (DQ-1,
RESIDUAL-1, RESIDUAL-2, W11b-CUSTOMIZE, W35-CUSTOMIZE, W37-CLRLWI,
L6-STRUCTHEADS) and records **~22 measured-inert spellings** plus several
measured-worse ones.  The decisive item is **W37's transplant**, which refutes
the class rather than adding one more negative:

> `?OnMsg@BandUI@@…ContentReadFailureMsg` emits the exact retail shape from
> `bool GetBool() const { return mData->Int(2); }`.  Transplanted **verbatim**
> into this arm — same TU, same `/FAs` — `static bool ProbeX(DataArray *a)
> { return a->Int(2); }` + `HANDLE_EXPR(has_license, ProbeX(_msg))` emits **no
> mask**.  Identical source, different codegen at the two sites.

⇒ the discriminator is not the arm expression, not the callee shape, and not the
value's provenance.  That refutes the entire "find the right spelling" program.
W37 named the only remaining channel — a compiler/codegen one (a different 10224
QFE, or a pragma affecting bool canonicalisation) — and set the burden as moving
the ProbeX transplant.  **We have exactly one compiler and I have no such lever,
so I did not spend a leg pretending to look for one.**

★ Reading the in-tree record cost four tool calls and saved a whole A/B leg —
CLAUDE.md's *"READ THE IN-TREE RECORD FIRST"* paying out literally, for the
second lane running (NEAR_CROSSINGS said the same thing three days ago).

### 1.5 ⚠ The briefing loop is the real defect, and it is structural

Eight lanes have now been pointed at this row.  The record inside the `.cpp` is
excellent and it is **not where dispatchers look**; the handoff lists in
`docs/decomp/*.md` are — and W11-B's handoff 6 re-opened a row that its own §4.1
caveat told the reader to verify first.  Nobody was careless.  The failure mode
is that **a priced refusal recorded in a source comment is invisible to the
doc-level briefing pipeline**, so the row keeps resurfacing as an unclaimed 5 kB
prize.  This file exists to put the refusal where the briefings are.

---

## 2. Task 2 — the FALSE 100 at `0x82574348`

### 2.1 What was wrong

`0x82574348` was mapped `?NewObject@FlowAnimate@@SAPAVObject@Hmx@@XZ`, sat in
unit `default/Flow`, and scored **fuzzy 100.0** at 100 B.  It is
`?NewObject@MetaPanel@@`.

The 100 was **forgiveness, not correctness**.  Retail's body is

```
li   r3, 0x104
bl   ??2CriticalSection@@SAPAXI@Z      ; 0x827bd2f0
stw  r3, 0x50(r31) / cmplwi r3,0 / beq
li   r4, 1 / bl fn_82573EE0            ; <- the ctor, UNNAMED in the map
b ... / li r3,0
cmplwi cr6,r3,0 / beq
lwz r11,0x4(r3) / lwz r11,0x4(r11) / add r11,r11,r3 / addi r3,r11,0x4
```

The ctor callee `fn_82573EE0` is **unnamed**, and `name_check` forgives a
placeholder target by construction — so the one instruction that could have
distinguished `FlowAnimate` from `MetaPanel` was uncharged.  Everything else is
class-independent boilerplate (the vbase adjust is a vbtable lookup, not a
constant), and the allocation size **coincides** at `0x104`.  ⇒ our
`FlowAnimate::NewObject` scored a perfect 100 against *MetaPanel's* body.

★ This is the cleanest instance I have seen of *never read a 100% row as
evidence that a callee is right*.  Nothing about the row looked wrong; it was
one of 42,818 perfect rows.

### 2.2 Evidence — four independent corroborations, none inherited

1. **The `RegisterFactory` self-witnessing pair.**  At `0x822A6D9C` the factory
   argument is `fn_82574348`, and the `StaticClassName` resolved immediately
   before it is `0x8256df78 = ?StaticClassName@MetaPanel@@`.  The second
   argument of `RegisterFactory(X::StaticClassName(), X::NewObject)` **can only
   be** `X::NewObject`.  The three neighbouring sites in the same `Init` all
   agree (ManageBandPanel, MoviePanel, MultiSelectListPanel), which is the
   control that says the instrument reads correctly here.
2. **Size.**  The body allocates `0x104`, and `MetaPanel.h` proves
   `sizeof(MetaPanel) == 0x104` from `Handle` + `OnMsg` — **written before
   anyone looked at this address.**
3. **Absence.**  `?NewObject@MetaPanel@@` had **no home in the map at all** (23
   MetaPanel rows, none of them `NewObject`), and `?StaticClassName@FlowAnimate@@`
   has no home either — so the FlowAnimate claim was unanchored in both
   directions.
4. ★ **The rival claim was already dead, and the header killed it.**
   `MetaPanel.h:76` carries a `MAP MISPAIR (do not "fix" by padding)` note
   saying `?NewObject@MetaPanel@@` @ `0x8256ead8` "wants vbase 0xEC / sizeof
   0x114 … i.e. they are paired to a *different* 0x114-byte panel class".  The
   map today names `0x8256ead8` **`?NewObject@BandStorePanel@@`** — the header's
   prediction, since corrected by someone else.  So the one competing address
   for this name had already been adjudicated away.

**Rename safety** (*proving a name wrong ≠ renaming is safe*): read from the
COFF symbol table **after a full build**, `MetaPanel.obj` **does** define
`?NewObject@MetaPanel@@SAPAVObject@Hmx@@XZ`; `Flow.obj` does not.  Without the
pin move the row would have been permanently 0%.

**Alias check** (standing rule): `symbol_aliases.json` has **0 groups** touching
`0x82574348`, `0x825743ac`, `?NewObject@FlowAnimate@@` or `?NewObject@MetaPanel@@`.
No forgiveness at stake ⇒ a real change, not an alias artefact.

### 2.3 The change, and the split rewriting its own input

- `scripts/target_symbol_map.json`: `0x82574348` →
  `?NewObject@MetaPanel@@SAPAVObject@Hmx@@XZ` (duplicate-name guard run: the
  name resolves to exactly 1 address afterwards, FlowAnimate's to 0).
- `config/45410914/splits.txt`: `.text 0x82574348-0x825743E0` moved out of
  `Flow.cpp` and merged into `MetaPanel.cpp`'s adjacent
  `0x82573904-0x82574348` block.

The first A/B was **REFUSED (exit 2, no verdict)** exactly as W11-B's §3.5
records: dtk re-derived the `.pdata` range `0x8221E698-0x8221E6A8` to follow the
`.text` move, so the split was not a fixed point.  `.pdata` is derived output,
never input.  One extra build reached the fixed point (verified by sha: two
further iterations produced byte-identical `splits.txt`), and the committed file
is that fixed point.  **The refusal was correct and the tool restored my edit
rather than pricing a non-fixed-point tree.**

### 2.4 Predicted vs measured

Pre-registered in `~/tmp/w12b_predictions.md` before the first edit:

> Flow loses a 100 B matched row (−1 fn / −100 B).  MetaPanel gains the target
> row; because our `FlowAnimate::NewObject` already scores 100 against **this
> exact body**, the `NEW_OBJ`-generated shape + `0x104` alloc + forgiven ctor
> callee should reproduce ⇒ MetaPanel's also 100 (+1 fn / +100 B).
> **Point estimate Δ 0 fns / 0 B; range [−1,0] fns / [−100,0] B.**
> `total_code`/`total_functions` unmoved.  Units falling off: 0.

Measured (`ab_measure --from-dirty`, ruler `name_check`):

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (49.149975 -> 49.149975)
unit net (ALL units) = +0   vs whole-binary Δmatched = +0
units at 100%: mpn 163->163, all-rows-fuzzy 135->135  (0 reached 100, 0 FELL OFF)
leg B: renamer_patched=1822, split=1, msvc recompiles=0
```

⚠ **A flat Δ is also what INERT looks like, so it needs a liveness witness.**
`renamer_patched=1822` and `split=1` on leg B are that witness — the map edit
demonstrably took.  Confirmed independently after a full build:

| row | before | after |
|---|---|---|
| `?NewObject@FlowAnimate@@` in `default/Flow`, 100 B | 100.0 (**false**) | *(row gone)* |
| `?NewObject@MetaPanel@@` in `default/MetaPanel`, 100 B | *(did not exist)* | **100.0 (true)** |
| `fn_825743AC` (its EH funclet), 40 B | in `default/Flow` | in `default/MetaPanel`, 100.0 |

`total_code` (10,245,956) and `total_functions` (69,219) are **identical on both
legs** — pure reattribution, no denominator movement.

★ **I predicted 0, W11-B predicted −100 B, and the reason I did better is worth
keeping**: the row's *false* 100 is itself the evidence that the true row will
also be 100.  Our `NEW_OBJ` body already matched this exact retail body; the
only question was which class's copy of an identical body gets the credit.  ⇒
**when a mis-identification is between two classes whose generated bodies are
byte-identical, the correction is priced at ZERO, not at the row's size.**

**Landed on accuracy, at zero headline cost.**  A body that was scored against
the wrong class in the wrong unit is now scored against the right one.
