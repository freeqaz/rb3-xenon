# OBJECTCPP_TU — lane W6-B: retail's `Object.cpp` TU, reunified

**Branch** `w6-objectcpp` · **worktree** `~/tmp/wt-w6-b` · **base** `50fd112d`
(asserted `git merge-base --is-ancestor` before the first edit, re-checked before
the rebase)

**Composed result: +16 matched functions / +768 B / +0.007495 pp** over two A/B
runs, every leg at a `symbols.txt` split fixed point, **0 regressions and 0 units
off 100% in either run**.

| # | item | pre-registered | **measured** | kept |
|---|---|---|---|---|
| 1 | reunify `Object.cpp`: 9 blocks back from `DirLoader.cpp` | +8 fns / +320 B (range +200…+450), 0 loss | **+14 fns / +648 B** | KEPT |
| 3 | the two `??_G` islands + alias group 432 withdrawn | +2 fns / +188 B, withdrawal cost **0** | **+2 fns / +120 B** | KEPT |
| 2 | `Object::AddRef` / `::Release` 252 B body port | — | **DEFERRED — diagnosed, and it is not collectable by a body port at all** | — |

**Composition witness.** Run 2's leg A reproduces run 1's leg B exactly:
`matched` 42658 → 42672 → 42674; `matched_code` 3,847,988 → 3,848,636 →
3,848,756.

**Anti-vacuity check before any name-keyed work** (FOLDPROVE-2): the worktree was
FULLY BUILT first; `verify_objs_patched.py --verify-manifest` returned **exit 0**
over 1,205 decomp + 3,085 target objects, and retail mangled names were confirmed
resolvable in the target objs (179 across `Object.obj` + `DirLoader.obj`). A
reflinked tree reads every retail name as absent.

---

## 1. The block structure of `0x8275A384`–`0x8275D03C`

W5-A recorded in `e391e9a4`'s message that retail's `Object.cpp` TU spans
`0x8275A384`–`0x8275D03C` and that laneU's pins hand **alternate blocks** of it to
`DirLoader.cpp`. That is exactly right, and the span is *bounded* — it is
delimited below by `Utl.cpp`'s `?CloneObject@@` ending at `0x8275A384` and above
by `DataFunc.cpp` starting at `0x8275D040`. 11,448 B, partitioned:

| owner | blocks | bytes |
|---|---:|---:|
| `Object.cpp` (before) | 11 | 6,408 |
| `DirLoader.cpp` | **9** | **4,536** |
| `ContextChecker.cpp` | 1 | 72 |
| `UITransitionHandler.cpp` | 1 | 424 |
| unpinned (2 × 4 B alignment padding) | — | 8 |

The alternation is strict — `Object`, `DirLoader`, `Object`, `DirLoader`, … —
across the whole 11 KB.

### 1.1 Evidence, in the order it convinced me

All geometry keyed on `.fn fn_<ADDR>` symbols, never the synthetic address column
(dtk renders `fn_8275C7DC`'s body at `82753714`).

1. **`DirLoader.cpp`'s genuine TU home is contiguous at `0x82754A00`–`0x82759978`**
   — 2.5 KB *below* the span, 7 blocks, 18,800 B. The 9 in-span blocks are
   interlopers, not an extension of it.
2. **Strict alternation across 11 KB is impossible for two genuine TUs.** There is
   no whole-program optimization in this build and TU spatial grouping in `.text`
   is preserved; two objects' contributions do not interleave nine times.
3. **Five *named* `Object@Hmx` methods sit inside `DirLoader`'s pins:**
   `?DataDir@` (`0x8275A4E8`), `?Save@` (`0x8275AB90`), `?HandleType@`
   (`0x8275ABD8`), `?HandleProperty@` (`0x8275AF78`), `?PropertyClear@`
   (`0x8275BC40`).
4. ★ **Decisive, and map-independent: the 936-B run of 28 functions at
   `0x8275C7DC`–`0x8275CB84` are EH unwind funclets** — every one carries the
   MSVC X360 funclet prologue `subi r31, r12, 0xe0` (r12 = parent frame pointer)
   and `bl`s a destructor on a parent-frame local. They are the funclets of
   `?Handle@Object@Hmx@@UAA?AVDataNode@@PAVDataArray@@_N@Z`, a 2,660-B
   `Object.cpp` row at `0x8275BD78` that **already scores 100.0000**.
   *Retail's own EH machinery names the owner of those bytes.*

### 1.2 ⛔ The span is NOT wholly `Object.cpp` — the strong hypothesis is false

The seductive reading is "everything between `Utl.cpp` and `DataFunc.cpp` belongs
to `Object.cpp`". Two blocks refute it, and both were **left alone**:

* **`?IsContextUsed@?A0x1e5d0754@@YA_NVSymbol@@@Z` @ `0x8275B868` (72 B)** reads
  **100.0** today — our `ContextChecker.obj` byte-matches retail *at that
  address*. It is an **island 1.99 MB** from ContextChecker's nearest other
  block, so W5-D's block-isolation column alone would have condemned it.
  **Byte identity outranks the geometry.** Moving it would have cost 72 B for
  nothing, and `Object.obj` cannot define that anonymous-namespace name.
* **`UITransitionHandler.cpp`'s `0x8275CE4C`–`0x8275CFF4` (424 B)**, whose
  `??$?5VTexPtr@RndMatAnim@@…` operator>> template pairs at **32.9** because
  `UITransitionHandler.obj` defines it. `Object.obj` does not.

⇒ **Genuine foreign COMDATs do get placed inside another TU's span.** An island
is a *suspicion*, not a verdict; the verdict comes from whether some object
actually defines the body.

---

## 2. The funclet pairability instrument — three vacuous attempts, and the control that caught them

The move put **1,316 B of currently-matched code at risk** (380 B of named
methods + 936 B of anonymous funclets scoring `fuzzy == 100` under
`masked_equal`). Pricing it needs the answer to: *does base `Object.obj` contain
byte-signature partners for those funclets?*

★ **My first three instruments all answered "0 partners" — and all three were
vacuous.** Each also reported 0 partners in base `DirLoader.obj`, where the rows
**demonstrably pair at 100 today**.

| attempt | result | why it was wrong |
|---|---|---|
| raw body bytes via `coff_bodies_ext.function_bodies_ext` | 0 / 936 B | relocations differ; no masking |
| + relocation masking | 0 / 936 B | still only 5 base candidates found |
| count funclet-shaped prologues | base `Object.obj` = **0** | prologue heuristic misses them |
| **corrected COFF walk** | **936 / 936 B** | ✅ |

**Root cause:** `tools/coff_bodies_ext.py`'s `is_aux_code_symbol()` **skips
`__unwind$*`**, so base `Object.obj` read **13** funclet candidates where its COFF
string table holds **103** (`__unwind$` × 90). The raw string-table count is what
exposed it.

**The control is the load-bearing part.** objdiff's rule
(`objdiff-core/src/diff/mod.rs:pair_funclets_by_bytes`) pairs unmatched
funclet-like symbols by relocation-masked byte signature, with pass 2b allowing
**many-to-one** onto an already-consumed identical base partner. Replicating it
and requiring it to reproduce **today's** DirLoader scores gave:

```
CONTROL over 158 DirLoader target funclets:  TP=85  FP=14  FN=0   (recall 100%)
```

**FN = 0** is the property that matters: signature-presence is *necessary*, so a
signature **absent** from base `Object.obj` is a sound refusal. The 14 FPs are
rows whose signature exists but whose relocation *names* disagree — masking drops
names, `name_check` does not.

⇒ Predicted: **936 / 936 B preserved**, plus **624 B of upside** over 15 sub-100
rows that acquire a partner. Measured: preserved in full, **+648 B**.

⚠ **Had I trusted the first negative I would have refused a correct, +648 B
move.** The usual warning is that a vacuity agrees with your *hope*; this one
agreed with *caution*, which is just as expensive and much easier to accept.

---

## 3. Item 1 — measured, and the prediction missed HIGH

`.text` only, 9 lines moved **verbatim** (no merging of abutting ranges, so
ownership is the single variable). dtk then re-derived `.pdata` by itself, moving
exactly **eight** 8-byte records — one per ceded block *that contains a function*;
the 4-byte padding block `0x8275B864`–`0x8275B868` has none. The split-guard
fired on the first attempt precisely because `.pdata` is derived output; the
retry is a fixed point, as the guard says. `DirLoader` keeps 9 blocks; neither
unit drained.

```
PRE-REGISTERED  +8 fns  (range +5..+11) / +320 B (range +200..+450), 0 loss
MEASURED       +14 fns / +648 B / +0.006325 pp   Dfuzzy +0.005420 pp
  default/Object     17 -> 66 matched (+49)
  default/DirLoader 135 -> 100        (-35)   [reattribution, not regression]
  net +14;  Dhonest +0, Dmasked_equal +14;  0 units off 100% on either ruler
```

**Attribution is exact: 15 symbols, all 15 crossing 0 → 100, summing to +648 B.**
14 of my 15 predicted rows crossed (`fn_8275ADE4` did not) and one I had not
predicted did (`fn_8275BBF8`, 64 B). Net +24 B — the *entire* prediction miss.

★ **Why I under-predicted:** I discounted the 624 B ceiling by the control's 86%
*precision* and again for the reloc-name gate. Neither discount applied — realised
precision was **93%** (14/15). The control's **recall** was the load-bearing
property; applying its precision figure to a population it was not measured on
turned a correct instrument into a pessimistic forecast. `Δhonest = +0` with
`Δmasked_equal = +14` confirms every new match came through the funclet channel
the instrument modelled.

---

## 4. Item 3 — the two `??_G` islands, and a correction to the brief

Two `MeterDisplay.cpp` islands, each a **wedge** sandwiched between two blocks of
the unit it actually belongs to:

| island | size | wedged between |
|---|---:|---|
| `0x82738A48`–`0x82738AB4` | 108 B | `Rnd_Xbox.cpp`'s `0x827378B8`–`0x82738A48` and `0x82738AB4`–`0x82739020` |
| `0x8248B930`–`0x8248B980` | 80 B | `TexBlender.cpp`'s `0x8248B784`–`0x8248B930` and `0x8248B980`–`0x8248C5E4` |

MeterDisplay's real home is `0x8231A4F8`–`0x8231C150`; it keeps 8 blocks.

**Identification — map-independent, then corroborated twice more each:**

* `0x8248B930`: `subi r31,r3,0x84` → `bl 0x8248B250` = `??_DRndTexBlender` →
  `flags&1` → operator delete. A class's `??_G` calls its **own** dtor ⇒
  `??_GRndTexBlender`. Corroborated: the two functions immediately below it are
  `?SyncProperty@RndTexBlender@@$4…` and `?Copy@RndTexBlender@@$4…` thunks.
* `0x82738A48`: reached by `bl` from `??_GDxMesh@0x82738AB8`, which adjusts by the
  **same `0x178`** sub-object offset this `??_D` uses and frees **`0x1AC`** bytes,
  matching this body's own `stw r10,0x1a4(r31)` vtable write inside a `0x1AC`
  object. Third witness: the body `bl`s `0x827385D0`, which the map already names
  `??1DxMesh@@UAA@XZ`.

**Pairability gated before the move:** base `Rnd_Xbox.obj` defines `??_DDxMesh`,
base `TexBlender.obj` defines `??_GRndTexBlender`; neither name was anywhere in
the map, so the renames are injective (duplicate-name count **2 before, 2 after**
— the same pre-existing pair W4-B recorded).

### 4.1 ★ Group 432's withdrawal cost ZERO — the brief expected a cost

The brief said: *"alias group 432 must be withdrawn, and UNLIKE group 1013 it DOES
forgive a site — that is why `??_GDxMesh` currently reads a forgiven 100.0. Expect
the withdrawal to COST bytes and pre-register that cost."*

**I pre-registered a cost of zero instead, and that is what measured.** The
reason: the same commit renames the survivor address to the name our source
**already spells** at the only forgiven site — verified in base `Rnd_Xbox.obj`,
where `??_GDxMesh`'s `+0x20` relocation targets `??_DDxMesh@@QAAXXZ`. Forgiveness
is therefore **replaced by a genuine repair**, not removed. `??_GDxMesh` measured
**100.0 → 100.0**, held exactly.

⇒ **Generalisation: withdrawing an alias is only a cost if the charge it was
forgiving survives the same commit.** Withdraw-plus-repair is free; the sign is
decided by what else is in the patch, and it is cheap to check by reading the
call site's relocation target out of the base obj. Group 432 is otherwise the
same defect as 1013 — survivor name taken from the map, T1 evidence that actually
proves `0x82738a48` **IS** DxMesh's `??_D`. `folded` emptied, a `withdrawn`
record added, **nothing pruned**.

### 4.2 Measured

```
PRE-REGISTERED  +2 fns / +188 B, withdrawal cost 0
MEASURED        +2 fns / +120 B / +0.001170 pp, Dhonest +2, Dmasked_equal +0
  ??_DDxMesh                        108 B  99.667 -> 100.0    +108   as predicted
  ??_GRndTexBlender                  80 B  99.700 ->  99.75      0   MISSED
  ??_GDxMesh                         84 B  100.0  -> 100.0       0   held, as predicted
  ??_ERndTexBlender@@$4PPPPPPPM@A@   12 B   98.33 -> 100.0     +12   NOT PREDICTED
```

★ **That last row is W5-D §1.1's own rule firing again** — *"when pricing a `??_G`
rename, count the `??_E` thunks that branch into it; collateral in both
directions."* I was handed that rule and did not apply it. **Note the shape,
which W5-D did not record:** the `??_E` crossed while the `??_G` it branches into
**did not**. So the collateral is not a function of the `??_G` *crossing* — it is
a function of the `??_G`'s **name becoming right**. Price it off the thunk layer,
never off the renamed row.

`control none` **+188 B** vs default **+120 B**: `none` moving *more* is the
pairing channel paying in full (both 108 + 80 B rows become definable in their new
units) while the graded ruler withholds 80 B behind `??_GRndTexBlender`'s
surviving relocation-name charge.

---

## 5. Item 2 — `AddRef` / `Release`: DEFERRED, and the deferral is *principled*

W5-D made these visible (`fuzzy` 0.000 → **69.680** / **77.342**); W5-A flagged
the residue as `std::list` vs our ring, "a STRUCT lane of its own, ~20 X360
files". Diagnosing it from retail bytes sharpens that in **both** directions.

### 5.1 The body logic ALREADY matches — the entire residue is one call

Retail `AddRef` @ `0x8275BD08` (100 B):
`if (ref->RefOwner() != this) mRefs.insert(mRefs.begin(), ref);`
Retail `Release` @ `0x8275B378` (152 B):
`if (this != sDeleting && ref->RefOwner() != this) { linear scan of mRefs for *it == ref; mRefs.erase(it); }`

**Our `#ifndef HX_NATIVE` source already has both control flows, guard for guard
and branch for branch.** The single divergence is that retail *calls out-of-line*
STLport container methods where we inline the splice/unlink:

| | retail | ours |
|---|---|---|
| insert | `bl 0x823D14C0` = `list<T*>::insert(sret, &mRefs, begin, &ref)` | `ObjRingInsert(&mRefs, ref)` — 2 args, inlined splice |
| erase | `bl 0x82BB33D8` = `list<T*>::erase(sret, &mRefs, it)` | inlined unlink + `PoolFree` |

★ And the **layout is already correct**: `mRefs` is 8 B `{next,prev}` at `+0x20`
and `ObjRefNode` is `0xc` `{next,prev,refPtr}` — bit-for-bit STLport's
`_List_node<ObjRefOwner*>`. **Our ring is a hand-rolled `std::list`.** So the
"struct lane" is *not* a layout change; it is a **retype** of an already-correct
layout, so that the compiler emits the out-of-line container calls.

### 5.2 ⛔ But the 252 B is NOT collectable by the body port alone

Under `name_check`, a retyped `mRefs` makes our call sites spell
`list<ObjRefOwner*>::insert` / `::erase`, while the map names the retail callees
by their **ICF survivor** spellings:

```
0x823d14c0 -> ?insert@?$list@PAVObject@Hmx@@…      (survivor: list<Hmx::Object*>)
0x82bb33d8 -> ?erase@?$list@PAVVoice@@…            (survivor: list<Voice*>)
```

Measured on this tree:

* `symbol_aliases.json` group **1481** (`0x823d14c0`, 1 folded spelling) and group
  **11** (`0x82bb33d8`, 40 folded spellings) contain **no `ObjRefOwner`
  spelling**;
* `target_symbol_map.json` names **0** addresses `list<ObjRefOwner*>::*`;
* the only `ObjRefOwner` list spelling in the whole alias file is
  `list<ObjRefOwner*>::clear`, folded in group **7** (`0x82718880`).

⇒ **A perfect body port lands `AddRef` and `Release` just below 100 with exactly
one charged relocation-name site each, billing 0 bytes.** This is the
`?Handle@CustomizePanel@@` / RESIDUAL-1 shape: the headline prize is
uncollectable by source work alone.

**The sufficient condition is two things, not one:** (a) retype `mRefs` to
`std::list<ObjRefOwner*>` across the 15 files that walk it, **and** (b) an
adjudication that `list<ObjRefOwner*>::insert`/`::erase` genuinely fold into
groups 1481 and 11. (b) is a real, provable fold — lists of 4-byte pointers are
byte-identical instantiations — but it is an alias **addition**, the class
CLAUDE.md flags as lifting the score by construction, so it needs retail-byte T1
evidence and cannot ride along on (a).

**I did not attempt it**, because (a) alone measures **+0 B** and touches
`HX_NATIVE`-shared headers (native-gate risk), and shipping a `reinterpret_cast`
of `mRefs` to a `std::list` — which would compile and match — is a lie in the type
system chosen to move a metric. Deferring with the blocker named is worth more
than a hack landed under time pressure.

---

## 6. Gates

Run on the rebased branch after a full `./tools/ninja-locked` (rc=0):

```
scripts/verify_objs_patched.py --verify-manifest  -> exit 0
tools/icf_alias_finder.py --validate              -> PASS, 0 CONTRADICTED
tools/symbols_fixpoint_guard.py                   -> see report (run LAST, then rebuild)
tools/native_build_gate.sh                        -> NATIVE_GATE_RESULT pasted verbatim
```

---

## 7. What I did NOT do

* **Item 2's body port** (§5) — deferred with the blocker named, not for budget.
* **`?IsContextUsed@?A0x1e5d0754@@` and `UITransitionHandler`'s 424 B** (§1.2) —
  proven foreign, deliberately left inside `Object.cpp`'s span.
* **The two 4-byte unpinned alignment gaps** (`0x8275CB84`, `0x8275CFF4`) — left
  unpinned so the carve geometry is byte-identical to before; pinning them is
  metric-neutral but would have added a variable to the A/B.
* **Merging abutting blocks.** Every moved range was re-homed **verbatim**, so
  ownership is the only variable in run 1. Merging would have relaxed carve
  constraints and confounded the measurement.
* **`??_GRndTexBlender`'s surviving charge** — 80 B, one charged site, not
  diagnosed. Diagnosing it needs a full-build objdiff read; `run_objdiff` does a
  one-`.obj` incremental build that skips the six obj patchers and would have left
  the tree measurably wrong mid-lane.
* **Any `src/` change**, any `symbols.txt` edit, any hand-edited `.pdata` line,
  any jeff/objdiff/wibo rebuild, any **new** alias group. One alias spelling was
  **withdrawn**; none was added.
* **`objects.json`** — untouched; no unit drained.
* **`tools/coff_bodies_ext.py`'s `is_aux_code_symbol` gap** (§2) — reported, not
  fixed. It is correct for that tool's own purpose (function slices); it is only
  wrong as a *funclet* census, and callers should not reach for it for that.

---

## 8. Handoffs, in the order I would fund them

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **`Object::AddRef`/`Release` — 252 B, and it needs BOTH a retype and an alias adjudication** | §5; bodies already match, layout already correct, groups 1481 + 11 named |
| H2 | **`??_GRndTexBlender` @ `0x8248B930`** — 80 B behind one charged site after the rename | §4.2; needs a full-build `report.json` charged-site read |
| H3 | **`DirLoader.cpp`'s own 9 remaining blocks** — now that the interlopers are gone, is `0x82754A00`–`0x82759978` complete? 68 of its 203 rows still read 0 | §1.1 |
| H4 | **`fn_8275ADE4` (40 B)** — the one predicted crossing that did not happen; its signature partner exists in base `Object.obj` but a reloc name disagrees | §3 |
| H5 | **`tools/coff_bodies_ext.py` is not a funclet census** — `is_aux_code_symbol` hides `__unwind$*` (13 vs 103 on one obj) | §2 |

---

## 9. Reusable lessons

* ★ **Retail's EH funclets name the owner of a span.** A run of
  `subi r31, r12, <parent frame>` prologues each `bl`-ing a dtor on a parent-frame
  local belongs to whichever function's frame they unwind — a TU-attribution
  channel that needs no map and no source.
* ★ **An island is a suspicion; byte identity is the verdict.** A 72-B block
  1.99 MB from its unit's nearest sibling looked exactly like W5-D's three
  mis-pins, and was *correct* — because our obj byte-matches retail at that
  address. Check "does some object actually define this body" before the
  isolation column.
* ★ **A vacuity that agrees with CAUTION is as expensive as one that agrees with
  hope**, and far easier to accept. Three instruments said "0 partners, do not
  move"; the move was worth +648 B. The control that had to reproduce the
  *current* state is what caught it.
* ★ **Use a validated instrument's RECALL, not its precision, when you carry it to
  a new population.** Discounting a 624 B ceiling by an 86% precision figure
  measured elsewhere turned a correct predictor into a −328 B forecast error.
* ★ **Withdrawing an alias costs bytes only if the charge it forgave survives the
  same commit.** Withdraw-plus-rename is free. Read the call site's relocation
  target out of the base obj to know which case you are in.
* ★ **`??_E` collateral is driven by the `??_G`'s NAME, not by the `??_G`
  crossing** — here the thunk crossed while the row it branches into did not.
* **`control none` read as a fraction of the default delta still decomposes the
  gain**: +188/+120 here = pairing paid in full, name channel withheld 80 B.
