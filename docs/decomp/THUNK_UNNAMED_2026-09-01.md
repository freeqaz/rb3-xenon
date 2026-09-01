# The free thunk repairs live in the population no instrument could see (lane THUNK3, 2026-09-01)

Tree: `dc605388` — main had advanced **5 commits past the briefed `26576070`**,
so this lane branched from current main, not from the brief's base.
Worktree `~/tmp/wt-thunk3`, branch `thunk3`, **fully built before any name
lookup** (a reflinked worktree's target objs are pre-renamer, so every retail
mangled name reads "absent" until the first build).

**Headline: the two residual `SET_DIFFER` rows and the 145 unnamed thunks are
THE SAME PHENOMENON, and that is why lane SLOTMAP measured 0 free renames over
269 candidates.** In both surviving rows the map's spelling sits on the *wrong
one of two byte-identical sibling adjustor thunks*, and the correct address is
**unnamed** — so no "rename X to its target's name" operation can reach it. The
repair is "move a name onto an unnamed sibling", an operation outside the
operation set of every instrument that has looked at this vein.

Landed: **+1 matched function / +12 matched bytes**, predicted exactly, with
0 units falling off either ruler.

---

## 0. ⛔ What refutes the brief

| brief said | measured here |
|---|---|
| `thunk_target_audit --validate` publishes `CONSISTENT 1835 · INCONSISTENT 113 · IRREDUCIBLE 17` ⇒ **130 flagged** | at `dc605388`: `CONSISTENT 1849 · INCONSISTENT 99 · IRREDUCIBLE 17` ⇒ **116 flagged**. The brief's figures were already stale by 14 rows. |
| **3** surviving `SET_DIFFER` classes | **2** — `StreakMeter` is `SAME` at `dc605388`, closed by an intervening lane |
| the rows look like `Handle`↔`ClassName` transpositions "in opposite directions" | **Neither is a transposition.** Both are one name on the wrong sibling thunk of the *same* class, with the right address unnamed. The "opposite directions" is an artifact of which sibling got the name. |
| `0x822aba00`/`0x822aba30` "want **two different** destination units (`Gem.cpp` vs `OutfitConfig.cpp`)" | **Both bodies are `OutfitConfig` methods.** PINHOME2 read the destination off the body's *pinned unit* instead of the body's *class*; `OutfitConfig::SetType` is itself mis-pinned into `Gem.cpp`. The conflict is a second, independent pin defect — not a conflict between the two thunks. |
| main is at `26576070` | `dc605388` |

⚠ The one number in the brief that **did** reproduce exactly is the population
size of the instrument itself: `decode_thunk` finds **2,164** adjustor thunks in
the map, and `word_refs(0x823591e8) = 2770`. Both predecessor lanes publish
these; reproducing them is what proves this is the same instrument.

---

## 1. The two `SET_DIFFER` rows, adjudicated on retail bytes

Neither needed an oracle, a ruler, or a declaration order. **A thunk IS its
branch target.**

### 1.1 AppLabel — three independent lines, all agreeing

| retail slot | address | branch target | verdict |
|---|---|---|---|
| `AppLabel[4]` | `0x825720b8` | `0x82570650` (**unnamed**) | is `ClassName`'s thunk |
| `AppLabel[6]` | `0x825720c8` (**unnamed**) | `0x825c8d98` = `?Handle@AppLabel@@UAA…` (**named**) | is `Handle`'s thunk |

The map had `?Handle@AppLabel@@$4PPPPPPPM@A@…` on `0x825720b8`. It belongs on
`0x825720c8`.

`0x82570650` is proven **not by shape but by a named callee**:

```
82570650  mflr r12 / stw r12,-8(r1) / std r31,-0x10(r1) / stwu r1,-0x60(r1)
82570660  mr   r31, r3                 <- the sret pointer
82570664  bl   0x82570428              <- ?StaticClassName@AppLabel@@SA?AVSymbol@@XZ
82570668  mr   r3, r31
          ... blr
```

and `ObjMacros.h` defines

```cpp
#define OBJ_CLASSNAME(classname) \
    virtual Symbol ClassName() const { return StaticClassName(); } \
    static Symbol StaticClassName() { static Symbol name(#classname); return name; }
```

which is that body, instruction for instruction.

★ **A control that could have failed, and did not.** `class AppLabel : public
BandLabel` declares `OBJ_CLASSNAME(BandLabel)` — i.e. our `AppLabel::
StaticClassName()` returns the string `"BandLabel"`, which looks like a
copy-paste bug. Retail's `StaticClassName` loads the literal at `0x8202f938`,
and that literal reads **`'BandLabel'`**. Our source is right, and this
simultaneously confirms `0x82570428` really is `AppLabel::StaticClassName`.

### 1.2 GameMicManager — same shape, mirrored

| retail slot | address | branch target | verdict |
|---|---|---|---|
| `GameMicManager[4]` | `0x8251cbc8` (**unnamed**) | `0x823e35e8` = `?ClassName@MsgSource@@UBA…` (**named**) | is `ClassName`'s thunk |
| `GameMicManager[6]` | `0x82681fb0` | `0x82768318` (**unnamed**) | is `Handle`'s thunk |

`0x82768318` is a `Handle` by consumption, not by shape guessing: it takes three
arguments and dereferences the third (`lwz r11,0(r5)`; `addi r4,r11,8`), i.e.
`DataNode Handle(DataArray* msg, bool)` reading `msg`'s node array. A
`Symbol ClassName() const` has no such argument.

### 1.3 The displacement tokens fit BOTH addresses — which is why the matcher coin-flipped

| pair | prologue | displacement | token |
|---|---|---|---|
| `0x825720b8` / `0x825720c8` | `lwz r11,-4(r4); subf r4,r11,r4`, no `addi` | 0 | `A@` |
| `0x82681fb0` / `0x8251cbc8` | same + `addi r4,r4,-0x2c` | 0x2C | `CM@` |

Each pair is byte-identical except the branch displacement. This is exactly lane
SETDIFF's lesson — *a twin pair distinguished only by an immediate cannot be
labelled by shape, so any map name on it is a coin flip* — and the branch target
is the tie-breaker the shape-based matcher does not have.

### 1.4 ⛔⛔ BOTH WRONG ROWS WERE SCORING A CLEAN 100.0

| row | unit | fuzzy | mpn | size |
|---|---|---|---|---|
| `?Handle@AppLabel@@$4PPPPPPPM@A@…` | `default/MetaPanel` | **100.0** | 100.0 | 12 B |
| `?ClassName@MsgSource@@$4PPPPPPPM@CM@…` | `default/GameMicManager` | **100.0** | 100.0 | 16 B |

Retail's branch target is **unnamed** in both cases, and `name_check` *forgives*
a placeholder target — so our thunk scores 100 against the wrong retail address.
This is the DxShaderMgr lesson from lane SETDIFF reproduced twice, and it is the
reason these rows were never going to be found by any score-ranked search.
**A 100% row is not evidence of identity.**

### 1.5 What landed, and the one row deliberately left alone

Three of the four rows landed. `0x8251cbc8` is **PIN-GATED**: it pins into
`MoggClipMap.cpp`, whose base obj defines 248 symbols and **not**
`?ClassName@MsgSource@@$4PPPPPPPM@CM@BA?AVSymbol@@XZ`. Naming it would drive
that row to a permanent 0%. This is SLOTMAP's 81% pin-gated class, met head-on
and respected.

**Measured** (`ab_measure --from-dirty`, graded `name_check`, both legs settled
to zero work **and** to a `symbols.txt` split fixed point, `renamer_patched=1823`):

| measure | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 42,276 | 42,277 | **+1** |
| `masked_equal` | 22,911 | 22,911 | +0 |
| honest | 19,365 | 19,366 | +1 |
| `matched_code_percent` | 36.822760 | 36.822880 | **+0.000120pp** |
| `matched_code` bytes | — | — | **+12** |
| aggregate fuzzy | 48.934082 | 48.934200 | +0.000118pp |
| units at 100% (mpn / fuzzy) | 149 / 121 | 149 / 121 | 0 reached, **0 fell off** |

Only unit moved: `default/MetaPanel` 344→345. **Predicted +1 fn / +12 B before
the run and measured exactly that.**

★ The `none` control moved **+12 B** and `ab_measure` classified it
`REAL_PAIRING`, *not* `ALIAS_SUSPECT` — a first-naming of an anonymous address
pairs a body that never paired, so `none` moving is the measure rather than a
violation. Worth knowing before anyone reads a moving `none` on a map patch as
fabrication.

---

## 2. The instrument for the unnamed population

`tools/unnamed_thunk_census.py` (new). Both shipped thunk instruments iterate
over map-**named** addresses, so an unnamed thunk is invisible to both — it
cannot be "named wrong". Yet it is the *other half* of every wrong name.

For an unnamed thunk with fan-in 1, a single RTTI-named vtable owner, and a
map-named body, the correct spelling is **derived, not guessed**: replace the
virtual-access char (`E`/`M`/`U` = private/protected/public virtual) with
`$<0|2|4>PPPPPPPM@<disp>@`, map `??_G` → `??_E`, and take `<disp>` from the
**measured** `addi`, encoded in MSVC's number grammar.

### 2.1 ★ The encoder control caught a bug that would have confirmed my brief

`--validate` re-derives the spelling of every already-correctly-named thunk and
compares against the map. First run: **0 / 1960**. I was emitting one `@` too
many (`enc_number` already terminates).

**The failure mode is the dangerous one:** a malformed spelling matches nothing,
so every candidate would have classified `PIN_GATED`, and the lane would have
reported a clean, decisive *"this population is unadjudicable"* — **exactly the
outcome the brief primed me to expect**. Same family as the `grep`-binary and
`all([])` traps.

After the fix: **1,849 / 1,960 = 94.3%** — and 1,849 is *exactly* the
`CONSISTENT` count that `thunk_target_audit --validate` reports from entirely
separate code. Two instruments agreeing to the row on that denominator is the
validation.

### 2.2 A guard that was an equality on a non-invariant

The census initially asserted `decode_thunk over the map == 2164`. That is a
property of the **map**, not the tree, and it **fired on this lane's own correct
edit** (naming one more thunk makes it 2165). Replaced with a sanity band plus
the guard that actually matters: require a *target* obj to define a mangled name,
which proves the renamer has run — the real pre-renamer hazard, and unlike the
map-size constant it does not move when the map changes.

### 2.3 Census result

<!--CENSUS-->

---

## 3. What I deliberately did NOT do

- **No source edits.** Both `SET_DIFFER` rows have our source on the correct
  side; this remained a map-defect worklist, as SETDIFF's Wave-6 score card
  (6 map defects, 0 source defects) predicts.
- **No `splits.txt` edits.** Re-homing is not metric-neutral and PINHOME2
  measured the whole adjacent vein at under 200 bytes; my brief's own instruction
  was that this lane's value is accuracy, not bytes.
- **Did not name `0x8251cbc8`** — pin-gated, see §1.5.
- **Did not act on `0x822aba00`/`0x822aba30`.** I refuted PINHOME2's stated
  reason for deferring them (§0) but acting still requires a carve plus a
  re-home, which is a pin lane, and both are pin-gated where they sit.
- **Did not fix `map_lint.parse_splits`** (PINHOME2 §2.1, still broken — 60.8%
  of pinned `.text` invisible through it). I wrote an independent block-level
  parser instead and self-validated it; changing a shared instrument mid-lane
  would have invalidated my own baselines.
- **Flagged but not chased:** `?ClassName@MsgSource@@UBA?AVSymbol@@XZ`
  (`0x823e35e8`) is pinned into `FlowCommand.cpp` and scores **0.0%** — a
  pre-existing pin defect, a separate lane.
