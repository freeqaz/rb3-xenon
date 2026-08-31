# Pin re-homing of misattributed adjustor thunks — the vein is REAL, MEASURED, and ~3× smaller than briefed (lane PINHOME2, 2026-08-31)

Tree: `620da4d8`, worktree `~/tmp/wt-pinhome2`, branch `pin-rehome`, **freshly
built before any name lookup** (a reflinked worktree's target objs are
pre-renamer, so every retail mangled name reads "absent" until the first build).

**Headline: the vein is real and it pays, but every headline figure in my brief
was wrong, and two of my own instruments were wrong before I caught them.**
Measured, whole-binary, three A/Bs, all at a `symbols.txt` split fixed point.

---

## 0. ⛔ WHAT REFUTES THE BRIEF — read this before reusing any of its numbers

| brief said | measured here |
|---|---|
| "**269** demonstrably-wrong thunk names" | **89**. See §1 — 269 exceeds a ceiling the tool itself publishes. |
| of which "36 RTTI-proven / 233 weaker" | **36** ✅ exact / **53** ❌ |
| "many sitting in **12-byte blocks** starting exactly at the thunk, i.e. individually re-homable" | of the brief's own 9-row `char/` run, **2 of 9** are 12-byte solo blocks; the rest are 32–424 B. And **only 1 of the 9 is individually re-homable** — the run is a **rotation**. |
| "24 [of 29] are pin-gated … individually re-homable" | re-homable-**and**-nameable is **16 rows over the whole binary**, worth **≤192 B**, requiring **6,220 B** of pin movement (a 32:1 ratio). |
| "`total_code` … is guaranteed to move here" | **UNCHANGED at 10,245,956** across all three A/Bs. Re-homing reattributes addresses between units; it does not change *which* addresses are pinned. |
| THUNK-105's defect at `0x82289748` "confirmed still present" | The **pin** defect is real. The **naming** half is moot: `0x82289748` **has no map name at all**. It is an *unnamed* row, invisible to SLOTMAP's instrument *and* to mine. |

★ And the last row opens a **second population nobody has counted** — see §6.

---

## 1. The detector reproduces SLOTMAP exactly on the denominator, and refutes it on the split

Re-derived from scratch, not inherited. Anti-vacuity guard first:
`word_refs(0x823591e8) = 2770` ✅ and `decode_thunk` finds **2,164** adjustor
thunks ✅ — both reproduce SLOTMAP exactly, so this is the same instrument.

| stage | SLOTMAP | here |
|---|---|---|
| ADJUDICABLE (fan-in 1, one vtable owner, body names a method of that owner) | 1,390 | **1,390** ✅ |
| map name already correct | 1,121 | **1,301** |
| **map name DEMONSTRABLY WRONG** | **269** | **89** |
| …class differs from RTTI owner | 36 | **36** ✅ |
| …right class, wrong method | 233 | **53** |

**269 is impossible, on the tool's own published numbers.** `thunk_target_audit`
scores the *whole* 2,164-thunk population at **INCONSISTENT 113 + IRREDUCIBLE 17
= 130** prefix-mismatches. A "demonstrably wrong" count over a **subset** cannot
exceed 130. My 89 is ≤ 130 and I verified **containment: `mine − flagged = 0`**.
⇒ the 36 is solid; the 233 is not, and neither is the 269 that carries it.

---

## 2. ⛔ TWO INSTRUMENTS WERE DEFECTIVE — one shipped, one mine

### 2.1 `map_lint.parse_splits` reports the LAST `.text` BLOCK, not the unit extent

It assigns (not accumulates) `text_lo`/`text_hi` on every `.text` line, so a
multi-block unit ends up described by its final block alone. `MasterAudio.cpp`
has **30** `.text` blocks and is reported as `0x8277FC38..0x827800B0` — the 30th.

| measure | value |
|---|---|
| units with >1 `.text` block | **731 of 1,279 (57%)** |
| pinned `.text` bytes inside a "last block" | 3,412,400 (**39.2%**) |
| **invisible to any consumer of these extents** | **5,290,384 B (60.8%)** |

`tools/pin_audit.py` builds `PUnit.lo/hi`, `pin_size` and `PinIndex.at()`
directly on this. ⇒ **Any address lookup or sliver-size judgement through that
path is wrong for 60.8% of pinned code.** Not fixed here (out of lane scope) —
flagged, sized, and reproducible.

★ This is exactly why the census self-validated. My block-level parser
disagreed with `map_lint` on **731 units**; rather than trust myself I checked
which was right, and the disagreement count matched the multi-block-unit count
one-for-one. My parser independently reproduces SLOTMAP §5's **6,588 `.text`
ranges / 1,278 units / 0 overlaps**.

### 2.2 I reintroduced the §4.2 substring bug, and my own funnel caught it

My first class extractor was `\?\?[_A-Z0-9]+([A-Za-z_]\w*)@@`. Greedy, it eats
the leading capitals of the class it is trying to read:

```
??_GRndLine@@UAAPAXI@Z         -> 'ndLine'          (want RndLine)
??_GBandScoreboard@@UAAPAXI@Z  -> 'andScoreboard'   (want BandScoreboard)
?Foo@Bar@Baz@@QAAXXZ           -> None              (want Bar@Baz)
```

Same family as the `name_owned_by` bug SLOTMAP fixed. It is **silent and
one-directional**: it inflated "body not a method of the RTTI owner" to 632 and
deflated ADJUDICABLE to **1,188**. Caught only because 1,188 ≠ SLOTMAP's 1,390.
With MSVC operator codes parsed by their real grammar (`_`/`__`+letter, or a
single `[0-9A-Z]`) and the qualified scope taken up to the first `@@`,
ADJUDICABLE lands on **1,390 exactly**.
⇒ **Reproducing a predecessor's denominator is a real control.** Had SLOTMAP not
published 1,390, this bug would have shipped as a "the vein is smaller" finding.

---

## 3. The `char/` run is a ROTATION, not a set of independent re-homes

The brief's 9-row `src/system/char/` run is one chain: each address's correct
spelling is **currently held by another address in the same run**.

| thunk | RTTI owner | map names it | wanted spelling held by |
|---|---|---|---|
| `0x823c6178` | CharForeTwist | CharIKMidi | **FREE** |
| `0x823c9570` | CharIKMidi | CharMirror | `0x823c6178` |
| `0x823cd9b0` | CharMirror | CharPollGroup | `0x823c9570` |
| `0x823c6e60` | CharUpperTwist | CharIKSliderMidi | `0x8232a120` (outside the run) |
| `0x823cb0f8` | CharIKSliderMidi | CharNeckTwist | `0x823c6e60` |
| `0x823ce698` | CharNeckTwist | CharServoBone | `0x823cb0f8` |
| `0x82375d10` | CharServoBone | CharSleeve | `0x823ce698` |
| `0x823cf948` | CharSleeve | CharDriver | `0x82375d10` |
| `0x823b40d0` | CharIKFingers | CharBlendBone | `0x823108d0` — **body UNNAMED, chain dead** |

⇒ **Renames must be applied atomically per chain** or they collide on a
duplicate map key. Each apply script asserts injectivity after the batch.

⚠ SLOTMAP reported "closed permutation cycles: **0**; per-unit conflict-free
batches: **0 of 32**" and concluded no local repair is injective. **Correct as
far as it goes, and it misses the actual shape**: these are not cycles and not
per-unit — they are **cross-unit chains terminating in a free name**. Three
exist. That is why this lane could land anything at all.

### 3.1 Evidence standard actually used (the map name is NOT one of the lines)

Per row, four independent lines, all agreeing:

1. **RTTI** — the thunk has image-wide fan-in 1 and its single referrer is a
   vtable whose `??_R4` COL names the owner class.
2. **The thunk's own branch target** — a thunk *is* its branch target, and that
   target is map-named a method of the owner class.
3. **Spatial** — the block is contiguous with the owner unit's own pinned span;
   in 5 of 9 it is fully **SANDWICHED** inside it (a hole punched in the middle
   of the owner's span and handed to another unit). Filling `0x823C9570..0x823C9708`
   makes `CharIKMidi.cpp` contiguous over `0x823C9540..0x823C9880`.
4. **Our side** — the owner unit's compiled obj already defines exactly the
   wanted `$4` spelling, unpaired.

⚠ **The map name corroborates nothing** — it appears to have been *derived from*
the pin (SLOTMAP §5), so map-and-pin agreeing is one witness, not two.

---

## 4. ⛔⛔ THE BLOCK-PURITY SCREEN — the defect that cost a −2 regression

**Chain A's first attempt was WRONG and the `none` control is what caught it.**

I moved `0x8232a120`'s **whole 648 B block** to `DialogDisplay.cpp` because it
passed the spatial test (sandwiched inside DialogDisplay's span). It is not a
thunk plus filler — it is **mostly CharUpperTwist code**:

```
0x8232a120  ?ClassName@DialogDisplay@@$4…      <- the only DialogDisplay symbol
0x8232a130  ??_ECharUpperTwist@@$2…
0x8232a148  ?SetType@CharUpperTwist@@UAA…      316 B
0x8232a2a8  ?SetType@CharUpperTwist@@$4…
0x8232a2c0  ??1CharUpperTwist@@MAA@XZ          136 B
```

| chain A variant | Δmatched | Δcode B | CharUpperTwist | `none` ruler |
|---|---|---|---|---|
| move the whole 648 B block | +5 | +60 | **−2 (REGRESSED)** | **−476 B** |
| split; move only the 12 B thunk | **+6** | **+72** | **+2** | **+0** |

⇒ **SCREEN, now applied to every candidate: before moving a block, list every
map-named address inside it; all must belong to the destination class or be
unnamed.** Of 7 blocks examined, **3 were impure** (`0x8232a120`, `0x827f4288`,
`0x822aba00`).

★★ **A block whose START is a foreign thunk tells you nothing about its
remaining bytes, and "SANDWICHED inside the owner's span" does NOT imply
purity.** The spatial test passed on the one block that was wrong.

★★ **On a *splits* patch the `none` control is a COLLATERAL-CODE-MOVEMENT
detector, even though `ab_measure` correctly labels it `NOT_APPLICABLE` for the
*alias* question.** The graded ruler still read **net-positive (+60)** while
real matched code was being dragged into the wrong unit; `none` read **−476 B**
and named the problem. Do not skip it on splits patches because the tool says
the alias shape is not adjudicable — those are different questions.

---

## 5. What landed, measured

All three A/Bs: `ab_measure --from-dirty`, both legs settled to zero work and to
a `symbols.txt` split fixed point, `total_code` **unchanged at 10,245,956**,
`total_functions` 69,219, ruler = shipped `name_check` (graded).

| commit | rows | Δmatched | Δcode B | Δcode% | `none` | units |
|---|---|---|---|---|---|---|
| `d9a59c31` chain B (CharForeTwist→CharIKMidi→CharMirror) | 3 | **+3** | **+36** | +0.000351pp | +0 | CharIKMidi 39→41, CharForeTwist 14→15 |
| `22cd705b` chain A (CharSleeve→…→DialogDisplay) | 6 | **+6** | **+72** | +0.000704pp | +0 | CharUpperTwist 13→15, CharIKSliderMidi 41→42, CharServoBone 46→47, CharSleeve 26→27, DialogDisplay 17→18 |
| `5ba6d35c` chain C + 2 singletons | 5 | **+5** | **+60** | +0.000586pp | +12 B | StreakMeter 162→164, Character 208→209, UILabel 159→160, VocalTrackDir 428→429, ReviewDisplay 21→22, **Waypoint 80→79** |
| **total** | **14** | **+14** | **+168** | **+0.001641pp** | | |

**All three predicted their result before measuring and hit it exactly**
(+3/+36; +6/+72 after the purity fix; +5/+60).

⚠ `Waypoint 80→79` is **bookkeeping, not a loss**: `fn_822D9F74` (32 B) is at
`fuzzy 100` on **both** legs and merely moved to `StreakMeter` with its block.
Nothing fell from 100. Distinguish this from chain A's first attempt, where rows
genuinely dropped 100 → 0. **The `none` control separates the two cases**:
**+12 B** here vs **−476 B** there — i.e. it discriminates in both directions,
which is what makes it worth reading.

### 5.1 A carve boundary must come from dtk's SYMBOL extent, not the instruction form

Batch 3's first run was **REFUSED**, correctly. I carved
`0x827F4288..0x827F4294` because the thunk decodes as 12 B (its 4th word is not
an `addi`). dtk carves the symbol as **16 B** — 12 B of thunk plus 4 B of
alignment padding — and the split hard-failed:

```
Failed: Split UILabel.cpp .text (0x827F4288..0x827F4294)
        ends within symbol 'fn_827F4288' (0x827F4288..0x827F4298)
```

⇒ **decoding the instruction form does not give you a carve boundary.** Take it
from the symbol. `ab_measure` refused rather than reporting a number over a
failed split — the refusal *is* the feature.

---

## 6. ★ A SECOND POPULATION NOBODY HAS COUNTED — unnamed thunks

Both SLOTMAP's instrument and mine iterate over **map-named** addresses, so an
adjustor thunk with **no map entry** is structurally invisible to both. The
brief's own THUNK-105 row is one: `0x82289748` **has no name**.

Scanning vtable-referenced addresses that decode as thunks and are **absent
from the map**, with fan-in 1, a single vtable owner, and a map-named body:

| measure | count |
|---|---|
| unnamed adjudicable thunks | **145** |
| …pinned in a **different unit from their own body** | **95** |
| …of those, solo 12/16-byte blocks | 4 |

These cannot be "renamed wrong" — they are unnamed — so the whole SLOTMAP
framing (misnaming) does not reach them, yet the **pin** defect is identical and
the same four evidence lines apply. `0x82289748` (12 B solo, `Line.cpp` →
`BandCharacter.cpp`, RTTI `BandCharacter[4]`) is the worked example.
⚠ Not attacked in this lane; naming a previously-anonymous address is a
*separate* economics question (CLAUDE.md: zero call-site upside, real downside,
pays in bug exposure).
★ Note chain A already fixed one of these **for free**: `0x82375d40` is an
unnamed sibling thunk that rode along inside the pure 64 B block — an argument
for moving whole **pure** blocks rather than minimal 12-byte carves.

---

## 7. Not re-homable, and why

| class | rows | why |
|---|---|---|
| thunk & body already in the same unit | 61 | rename-only; not a pin question |
| chain **BLOCKED** at an unadjudicable address | **56 of 72** | the chain terminates at a thunk whose **body is unnamed** — SLOTMAP §7.2's identification wall, reproduced. **Not a naming problem.** |
| block IMPURE with conflicting destinations | 2 | `0x822aba00`/`0x822aba30` share **one** 532 B block whose two thunks want **two different** destination units (`Gem.cpp` vs `OutfitConfig.cpp`). Cannot both be satisfied by moving the block; needs a carve plus adjudication of which body name is wrong. **Deferred.** |
| ambiguous (>1 candidate spelling) | 17 | `cand_in_body_unit` not unique |

---

## 8. What I deliberately did NOT do

- **No source edits.** Every row here has our source on the correct side; this is
  a pin/map defect worklist.
- **Did not fix `map_lint.parse_splits`** (§2.1) or `pin_audit`'s use of it.
  Sized and flagged; changing a shared instrument mid-lane would have invalidated
  my own baselines.
- **Did not attack the 145 unnamed thunks** (§6) — different economics, and
  naming an anonymous address is a bet, not a freebie.
- **Did not touch `0x822aba00`/`0x822aba30`** (§7) — the one case needing a real
  adjudication of *which* map name is wrong, not just where the pin points.
- **Did not chase the 56 blocked rows.** They need body identifications; no
  amount of pin or name work moves them.
- **No `.pdata` edits anywhere** — it is derived output, re-derived every split.
  All three patches touch `.text` only, and every split regenerated `.pdata`
  cleanly.

## 9. For the next lane

1. **The remaining resolvable rows are drained**: 16 identified, **14 landed
   here (+168 B of a ≤192 B ceiling)**. The 2 left are the impure
   `0x822aba00`/`0x822aba30` pair (§7). Do **not** fund this as a byte lever
   again — the honest total for the whole vein was under 200 bytes.
2. **The real headroom is §6's 95 mis-pinned unnamed thunks** — and its value is
   *accuracy* (correct unit attribution), not bytes.
3. **Fix `map_lint.parse_splits`** (§2.1) before anyone builds another census on
   `pin_audit`'s extents; 60.8% of pinned `.text` is invisible through it.
4. **Always run the block-purity screen** (§4) before moving a block, and read
   the `none` ruler on splits patches.
