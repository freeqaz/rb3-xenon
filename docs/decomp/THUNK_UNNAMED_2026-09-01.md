# The free thunk repairs live in the population no instrument could see (lane THUNK3, 2026-09-01)

Tree: `dc605388` — main had advanced **5 commits past the briefed `26576070`**,
so this lane branched from current main. Worktree `~/tmp/wt-thunk3`, branch
`thunk3`, **fully built before any name lookup** (a reflinked worktree's target
objs are pre-renamer, so every retail mangled name reads "absent" until the
first build).

**Headline: the residual `SET_DIFFER` rows and the unnamed-thunk population are
THE SAME PHENOMENON, and that is why lane SLOTMAP measured 0 free renames over
269 candidates.** In every case the map's spelling sits on the *wrong one of two
byte-identical sibling adjustor thunks*, and the correct address is **unnamed** —
so no "rename X to its target's name" operation can reach it. The repair is
*"move a name onto an unnamed sibling"*, an operation outside the operation set
of every instrument that has looked at this vein.

Landed over three A/Bs: **+16 matched functions / +204 matched bytes /
+0.001993pp**, every one predicted exactly before the run, **0 units falling off
either ruler in any run**.

| # | change | Δmatched | Δcode_bytes | Δcode% |
|---|---|---:|---:|---:|
| 1 | the 2 residual `SET_DIFFER` rows (3 map rows) | **+1** | **+12** | +0.000120pp |
| 2 | the 13 provable, pin-compatible unnamed thunks | **+13** | **+168** | +0.001640pp |
| 3 | PINHOME2's deferred `0x822aba00`/`0x822aba30` chain (4 map rows) | **+2** | **+24** | +0.000233pp |
| | **total** | **+16** | **+204** | **+0.001993pp** |

Absolutes: `matched_functions` 42,276 → 42,292; `matched_code_percent`
36.822760 → 36.824753. Graded `functionRelocDiffs=name_check`, both legs settled
to zero work and to a `symbols.txt` split fixed point on every run.

---

## 0. ⛔ What refutes the brief

| brief said | measured here |
|---|---|
| `thunk_target_audit --validate` publishes `CONSISTENT 1835 · INCONSISTENT 113 · IRREDUCIBLE 17` ⇒ **130 flagged** | at `dc605388`: `CONSISTENT 1849 · INCONSISTENT 99 · IRREDUCIBLE 17` ⇒ **116 flagged**. The brief's figures were already stale by 14 rows. |
| **3** surviving `SET_DIFFER` classes | **2** — `StreakMeter` is `SAME` at `dc605388`, closed by an intervening lane |
| the rows look like `Handle`↔`ClassName` transpositions "in opposite directions" | **Neither is a transposition.** Both are one name on the wrong sibling thunk of the *same* class, with the right address unnamed. "Opposite directions" is an artifact of which sibling got the name. |
| `0x822aba00`/`0x822aba30` "want **two different** destination units (`Gem.cpp` vs `OutfitConfig.cpp`)" | **Both bodies are `OutfitConfig` methods.** PINHOME2 read the destination off the body's *pinned unit* instead of its *class*. |
| "**145** unnamed adjustor thunks … **95 of the 145** pinned in the wrong unit" | **138 adjudicable** of 344 unnamed vtable-referenced thunks, and **86** cross-unit. Reconciles exactly: 145 − 1 (landed here) = 144 = 138 + 6 whose body name is not a virtual member function. |
| main is at `26576070` | `dc605388` |

⚠ The brief's numbers that **did** reproduce exactly are the instrument's own
population size — `decode_thunk` finds **2,164** adjustor thunks in the map and
`word_refs(0x823591e8) = 2770`. Both predecessors publish these; reproducing
them is what proves this is the same instrument, and every script here asserts
them before doing anything.

---

## 1. The two `SET_DIFFER` rows, adjudicated on retail bytes

Neither needed an oracle, a ruler, or a declaration order. **A thunk IS its
branch target.**

### 1.1 AppLabel

| retail slot | address | branch target | verdict |
|---|---|---|---|
| `AppLabel[4]` | `0x825720b8` | `0x82570650` (**unnamed**) | is `ClassName`'s thunk |
| `AppLabel[6]` | `0x825720c8` (**unnamed**) | `0x825c8d98` = `?Handle@AppLabel@@UAA…` (**named**) | is `Handle`'s thunk |

`0x82570650` is proven **not by shape but by a named callee**:

```
82570650  mflr r12 / stw r12,-8(r1) / std r31,-0x10(r1) / stwu r1,-0x60(r1)
82570660  mr   r31, r3                 <- the sret pointer
82570664  bl   0x82570428              <- ?StaticClassName@AppLabel@@SA?AVSymbol@@XZ
82570668  mr   r3, r31 ... blr
```

and `ObjMacros.h` defines

```cpp
#define OBJ_CLASSNAME(classname) \
    virtual Symbol ClassName() const { return StaticClassName(); } \
    static Symbol StaticClassName() { static Symbol name(#classname); return name; }
```

which is that body, instruction for instruction.

★ **A control that could have failed, and did not.** `class AppLabel : public
BandLabel` declares `OBJ_CLASSNAME(BandLabel)` — so our `AppLabel::
StaticClassName()` returns the string `"BandLabel"`, which reads like a
copy-paste bug. Retail's `StaticClassName` loads the literal at `0x8202f938`,
and that literal is **`'BandLabel'`**. Our source is right, and this
simultaneously confirms `0x82570428` really is `AppLabel::StaticClassName`.

### 1.2 GameMicManager

| retail slot | address | branch target | verdict |
|---|---|---|---|
| `GameMicManager[4]` | `0x8251cbc8` (**unnamed**) | `0x823e35e8` = `?ClassName@MsgSource@@UBA…` (**named**) | is `ClassName`'s thunk |
| `GameMicManager[6]` | `0x82681fb0` | `0x82768318` (**unnamed**) | is `Handle`'s thunk |

`0x82768318` is a `Handle` by *argument consumption*, not by shape guessing: it
takes three arguments and dereferences the third (`lwz r11,0(r5)`;
`addi r4,r11,8`) — `DataNode Handle(DataArray* msg, bool)` reading `msg`'s node
array. A `Symbol ClassName() const` has no such argument.

### 1.3 The displacement token fits BOTH addresses — which is why the matcher coin-flipped

| pair | prologue | disp | token |
|---|---|---|---|
| `0x825720b8` / `0x825720c8` | `lwz r11,-4(r4); subf r4,r11,r4`, no `addi` | 0 | `A@` |
| `0x82681fb0` / `0x8251cbc8` | same + `addi r4,r4,-0x2c` | 0x2C | `CM@` |

Each pair is byte-identical except the branch displacement — lane SETDIFF's
lesson exactly (*a twin pair distinguished only by an immediate cannot be
labelled by shape*). The branch target is the tie-breaker a shape-based matcher
does not have.

### 1.4 ⛔⛔ BOTH WRONG ROWS WERE SCORING A CLEAN 100.0

| row | unit | fuzzy | mpn | size |
|---|---|---|---|---|
| `?Handle@AppLabel@@$4PPPPPPPM@A@…` | `default/MetaPanel` | **100.0** | 100.0 | 12 B |
| `?ClassName@MsgSource@@$4PPPPPPPM@CM@…` | `default/GameMicManager` | **100.0** | 100.0 | 16 B |

Retail's branch target is **unnamed** in both cases and `name_check` *forgives* a
placeholder target, so our thunk scores 100 against the wrong retail address.
The DxShaderMgr lesson reproduced twice — and the reason no score-ranked search
would ever have surfaced these. **A 100% row is not evidence of identity.**

### 1.5 The one row deliberately left alone

`0x8251cbc8` is **PIN-GATED**: it pins into `MoggClipMap.cpp`, whose base obj
defines 248 symbols and **not** `?ClassName@MsgSource@@$4PPPPPPPM@CM@BA?AVSymbol@@XZ`.
Naming it would drive that row to a permanent 0%. SLOTMAP's 81% class, met and
respected.

---

## 2. The instrument for the unnamed population

`tools/unnamed_thunk_census.py` (new). Both shipped thunk instruments iterate
over map-**named** addresses, so an unnamed thunk is invisible to both — it
cannot be "named wrong". Yet it is the *other half* of every wrong name.

For an unnamed thunk with fan-in 1, a single RTTI-named vtable owner, and a
map-named body, the correct spelling is **derived, not guessed**: replace the
virtual-access char (`E`/`M`/`U`) with `$<0|2|4>PPPPPPPM@<disp>@`, map
`??_G` → `??_E`, and take `<disp>` from the **measured** `addi`.

### 2.1 Census (at `dc605388` + this lane's 3 map commits)

| class | before this lane | after |
|---|---:|---:|
| unnamed adjustor thunks referenced by a vtable | **344** | 331 |
| — excluded: fan-in>1 or multiple vtable owners | 44 | 44 |
| — excluded: **BODY UNNAMED** (identification wall) | **156** | 156 |
| — excluded: body name not a virtual member function | 6 | 6 |
| **ADJUDICABLE** | **138** | **125** |
| → PIN_GATED | 87 | 87 |
| → NAME_TAKEN | 30 | 30 |
| → **FREE_AND_DEFINED** *(actionable)* | **13** | **0** |
| → UNPINNED | 8 | 8 |

Self-validating: 44+156+6+138 = 344 and 87+30+13+8 = 138.

⇒ **The actionable class is DRAINED to 0.** What remains is 87 rows needing a
**pin re-home**, 30 needing a **rotation**, 8 unpinned, and 156 blocked at the
identification wall — *the same wall SLOTMAP and PINHOME2 both terminated at.*
**45.3% (156/344) of the whole population dies on an unnamed body**, and no
naming or pinning work moves any of it.

### 2.2 ★ The encoder control caught a bug that would have CONFIRMED my brief

`--validate` re-derives the spelling of every already-correctly-named thunk and
compares against the map. First run: **0 / 1960**. I emitted one `@` too many.

**The failure mode is the dangerous one:** a malformed spelling matches nothing,
so every candidate classifies `PIN_GATED`, and the lane reports a clean,
decisive *"this population is unadjudicable"* — **exactly what the brief primed
me to expect**. Same family as the `grep`-binary and `all([])` traps.

After the fix: **1,849 / 1,960 = 94.3%**, and 1,849 is *exactly* the `CONSISTENT`
count `thunk_target_audit --validate` reports from entirely separate code. Two
instruments agreeing to the row on that denominator is the validation.

★ And the 13 landed rows are the **stronger** evidence the control cannot give:
`--validate` only bounds the derivation on the ALREADY-NAMED population, and
**13/13 reached 100** on the unnamed one.

### 2.3 Three defects in my own instrument, all found by running it

1. **An equality guard on a non-invariant.** `decode_thunk over the map == 2164`
   is a property of the **map**, not the tree, and it **fired on this lane's own
   correct edit** (naming one more thunk makes it 2165). Replaced with a sanity
   band plus the guard that actually matters — require a *target* obj to define
   a mangled name, which proves the renamer has run.
2. ⛔ **An infinite loop that read as slowness.** `thunk_displacement` returns
   `-imm`, so an `addi` that ADDS yields a negative displacement, and
   `while n: n >>= 4` never terminates for `n < 0` in Python (`-1 >> 4 == -1`).
   The census span-locked at 98% CPU with no output for 22 minutes; it looked
   like an O(n²) scan, not a hang, and cost two aborted runs.
   ★ **`--validate` structurally CANNOT catch this**: no already-*named* thunk
   has a positive `addi`, so the pathology lives only in the population the
   control does not reach. *A passing control bounds the encoder on the named
   population only.*
3. **`basename()` join.** 24 objdiff unit basenames collide (`Movie` in
   `rnddx9/` vs `rndobj/`, `Utl` three ways, `Rnd`, `Dir`, `Synth`, four
   `FxSend*` pairs); 10 census rows sat in colliding units. Now joins on the
   full stem and reports `NO_BASE_OBJ` on an ambiguous basename rather than
   silently consulting the wrong object. ⚠ **Measured honestly: the fix changed
   ZERO verdicts** (PIN_GATED 87 before and after). Latent, not active.

⚠ Tooling note: `scripts/target_symbol_map.json` is **not globally sorted** and
its final entry carries no trailing comma, so positional insertion after "the
last address key below mine" corrupts the file. Insert new keys as a block after
the opening `{`; JSON key order is irrelevant.

---

## 3. PINHOME2's deferred pair — a 2-link chain, provable from both ends

The census closed the chain from the side PINHOME2 could not see. Two *unnamed*
thunks want exactly the spellings the deferred pair was holding:

| address | body (named) | pinned unit | base obj defines wanted spelling? |
|---|---|---|---|
| `0x8238f970` `CharClipGroup[0]` | `??_GCharClipGroup@@UAA…` | `CharClipGroup.cpp` | **yes** |
| `0x8232ecb0` `BandWardrobe[0]` | `??_GBandWardrobe@@UAA…` | `BandWardrobe.cpp` | **yes** |
| `0x822aba00` (holder) | `?SetType@OutfitConfig@@UAA…` | `HamCamTransform.cpp` | no ⇒ pin-gated |
| `0x822aba30` (holder) | `?Save@OutfitConfig@@UAA…` | `HamCamTransform.cpp` | no ⇒ pin-gated |

The two OutfitConfig spellings were **written rather than nulled** — they are
true, they make `thunk_target_audit` read CONSISTENT, and they hand a future
re-homing lane the answer. They score 0% because `HamCamTransform.cpp`'s base obj
cannot define an OutfitConfig thunk; that is a **pin** defect, recorded honestly
rather than hidden behind a null.

Note the two wrong names were sitting at **98.333336** — the audit docstring's
signature "one charged element in a 12-byte body" — i.e. contributing **0**
matched bytes, so correcting them could not lose anything.

---

## 4. ⛔⛔ `ALIAS_SUSPECT` fired on change 3, and it is a FALSE POSITIVE — proven per row

`ab_measure` reported the fabricated-alias shape: graded ruler **+24 B** while
`none` was **FLAT**, on a map-only patch. Per-row accounting from the archived
leg reports:

| row | `none` legA | `none` legB | graded legA | graded legB |
|---|---|---|---|---|
| `??_ECharClipGroup@@$4…` | HamCamTransform **100.0** (12 B) | CharClipGroup **100.0** (12 B) | **98.33** (0 B) | **100.0** (12 B) |
| `??_EBandWardrobe@@$4…` | HamCamTransform **100.0** (12 B) | BandWardrobe **100.0** (12 B) | **98.33** (0 B) | **100.0** (12 B) |

`none` ignores relocation **names**, so it was already paying full credit for the
**wrong** name. Moving the name to the right address leaves `none` flat **by
cancellation** (24 B → 24 B) while the graded ruler correctly goes 0 → 24.

★★ **GENERAL LESSON, reusable:** *a patch that MOVES a name from a wrong address
to a right one produces graded-UP / `none`-FLAT — bit-for-bit the
fabricated-alias shape — because `none` credits the name at BOTH addresses.* The
control cannot separate the two; only per-row accounting can. This is a fourth
instance of the mis-fire CLAUDE.md already records for first-namings, nullings
and signature+map changes.

**The disproof of fabrication:** reaching **graded** 100 requires our thunk's
branch-target NAME to equal retail's. A fabricated alias cannot produce that
under `name_check` — it can only stop a charge, and the charge here is on a
relocation target name that genuinely now agrees.

⚠ Conversely, changes 1 and 2 moved `none` (+12 B, +168 B) and were classified
`REAL_PAIRING`, because a first-naming pairs a row that never paired. So across
three A/Bs in one lane the same instrument produced both readings for correct
work — read the *mechanism*, never the label.

---

## 5. What I deliberately did NOT do

- **No source edits.** Every row here has our source on the correct side; this
  stayed a map-defect worklist, exactly as SETDIFF's Wave-6 score card
  (6 map defects, 0 source defects) predicts.
- **No `splits.txt` edits.** Re-homing is not metric-neutral, PINHOME2 measured
  the adjacent vein at under 200 bytes, and my brief's own instruction was that
  this lane's value is accuracy.
- **Did not name `0x8251cbc8`** (pin-gated, §1.5) or any of the **87 PIN_GATED**
  census rows — naming a pin-gated address is a permanent 0%.
- **Did not attempt the 30 NAME_TAKEN rotations** beyond the one chain whose
  both ends were provable. Notably `0x82289748` — THUNK-105's famous address,
  which PINHOME2 called invisible to every instrument — **is visible to this
  census** as `NAME_TAKEN`: it wants `?ClassName@BandCharacter@@$4…`, currently
  held by `0x8227b050`. Adjudicating that holder is the next rotation.
- **Did not name `0x82570650`** (`?ClassName@AppLabel@@UBA?AVSymbol@@XZ`), even
  though it is proven (§1.1), free, and defined in `MetaPanel.obj`. It is a
  *body*, not a thunk — a different economic class (a 48 B bet on our body
  matching) that would have muddied a clean prediction. **Left as a ready
  candidate.**
- **Did not fix `map_lint.parse_splits`** (PINHOME2 §2.1, still broken — 60.8%
  of pinned `.text` invisible through it). I wrote an independent block-level
  parser and self-validated it against SLOTMAP's published 6,588/1,278/0
  (measured 6,591/1,275/0 — drift of exactly +3 blocks / −3 units, which is
  precisely what PINHOME2's three carve-and-move chains produce).
- **Flagged, not chased:** `?ClassName@MsgSource@@UBA?AVSymbol@@XZ`
  (`0x823e35e8`) is pinned into `FlowCommand.cpp` and scores **0.0%** — a
  pre-existing pin defect; and `?SetType@OutfitConfig@@UAA…` is mis-pinned into
  `Gem.cpp` (§3).

## 6. Verification, re-run at the branch tip

**`tools/vtable_order_sweep.py --sweep` — the `SET_DIFFER` class is DRAINED:**

| measure | at `dc605388` | at branch tip |
|---|---:|---:|
| `SET_DIFFER` | 2 | **0** |
| `SAME` | 974 | **976** |
| `PERMUTED` | 0 | 0 |
| comparable slots charged on | 5,117 | 5,135 |
| excluded `unnamed` | 1,082 | **1,066** (−16 = exactly this lane's namings) |
| withheld | 299 | 297 |

With lane SETDIFF's 6 and the intervening `StreakMeter` closure, the class that
began at **9** is now **0**.

**`tools/thunk_target_audit.py --validate`:**

| measure | at `dc605388` | at branch tip |
|---|---:|---:|
| adjustor thunks in map | 2,164 | **2,180** (+16) |
| CONSISTENT | 1,849 | **1,867** |
| INCONSISTENT | 99 | **97** |
| IRREDUCIBLE (fold hub) | 17 | 17 |
| TARGET_UNNAMED | 199 | 199 |
| flagged at fuzzy==100 / CONSISTENT at fuzzy==100 | 3.4% / 99.6% | **3.5% / 99.5%** |

Partition self-check: 1867 + 199 + 97 + 17 = 2180 ✅. The control still
separates by ~28×, so the instrument has **not** stopped discriminating — which
is the condition its own docstring says to check before trusting it.

## 7. For the next lane

1. **The naming vein is drained: FREE_AND_DEFINED is 0.** Do not re-fund a
   naming pass over unnamed thunks; re-run the census first and require a
   non-zero actionable class before spending anything.
2. **The wall is identification, again.** 156 of 344 (45.3%) die on an unnamed
   *body*. Neither naming nor pinning moves them — this is the third consecutive
   lane to terminate here.
3. **87 PIN_GATED rows are a pin re-homing question**, and PINHOME2 already
   priced that vein at under 200 bytes total. Its value is accuracy only.
4. **Re-measure every figure.** The brief's `--validate` numbers were 14 rows
   stale after one day, and `SET_DIFFER` had already dropped 3 → 2 underneath it.
