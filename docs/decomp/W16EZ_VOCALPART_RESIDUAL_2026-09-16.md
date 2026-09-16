# W16-EZ — the VocalPart residual: one proven alias (+1004 B), and two commutative-order rows that are NOT source-fixable

Lane W16-EZ, 2026-09-16, worktree `~/tmp/wt-w16-ez`, branch `w16-ez`, based at `2d9c4ead`.
Ruler: shipped graded `name_check`, read from `build/45410914/report.json`
`provenance.diff_config` (objdiff 4.2.9, `tool_commit a5f0ea903ec1`) — not assumed.

Baseline measured in **this** worktree, settled to zero compile work:

```
matched=43978  masked_equal=23224  honest=20754  code=4131656  code%=40.320374
fuzzy=50.014150   total_functions=69240   total_code=10247068
```

⚠ This is not W16-ET's baseline (43969 / 40.305344); main moved between the lanes.
Every figure below was measured in-run in my own worktree.

## Result in one line

`CalcNoteWeights` — **a row the brief does not list** — was one missing ICF alias
membership, proven on retail bytes and landed for **+3 functions / +1004 B**; and the
two rows the brief called the biggest untouched prizes are both **commutative-operand
-order residuals with no source lever**, one of which was already refuted in-file seven
weeks ago.

---

## 1. Three corrections to the brief, all established before editing anything

1. ⛔ **The brief missed a sub-100 row, and it was the most valuable one available.**
   `?CalcNoteWeights@VocalPart@@QAAXXZ`, 292 B, fuzzy 99.93150, **mpn 99.93150** — worth
   **+1 function AND +292 B**. The brief lists five rows; there are six. It turned out to
   be the only row in the unit that moved at all.
2. **"70 named rows, 31 at fuzzy 100" mislabels its denominator.** 70 is the TOTAL row
   count (36 named + 34 anonymous); named rows at fuzzy 100 = 30, all rows = 31. The
   integers are right, the noun is wrong.
3. ⛔ **"Nobody has opened either" of the bytes-only pair is FALSE for `HandlePhraseEnd`.**
   `src/band3/game/VocalPart.cpp` carried an in-source NOTE at the charged line, written
   by lane AG2 in `0ac748fc` on **2026-07-26**, recording that both operand orders and
   int/float temps had already been tried. This is the READ-THE-IN-TREE-RECORD-FIRST rule
   paying out again: the refutation was inside the file the lever was about.

The brief's own five figures are all **exact**, and its self-warning was well placed —
it was the row it omitted that carried the lane.

---

## 2. What landed — `CalcNoteWeights`, and why it is landed on bytes and not on the score

The entire 292 B residual was ONE `diff_arg` on a `bl` at idx 18: retail names
`?reserve@?$vector@PAUDep@CharPollableSorter@@…` and we name
`?reserve@?$vector@M…` (`vector<float>::reserve`). The target spelling is the **survivor**
of the existing group at `0x823715e0` in `scripts/symbol_aliases.json`; our spelling was not
among its two folded members.

★ **Positive control inside the same row:** `erase` shows the *identical* target/base split
(`vector<unsigned int>` vs `vector<float>`) and is **not charged**, because both of its
spellings are already folded in the group at `0x824b06c8`. That is the mechanism
demonstrating itself one instruction away from the defect.

⛔ **An alias lifts `name_check` BY CONSTRUCTION, and a `none` control is flat for a
FABRICATED alias too — so flatness there is the hazard's signature, not a clearance.**
This was therefore adjudicated on retail bytes, on all three channels this group's own
evidence documents:

| channel | result |
|---|---|
| (1) CHASED T1 (`icf_pair_adjudicate.py --chase`) | **PROVEN** — `retail_size == our_size == 188`; the only differing slot is `_M_allocate_and_copy<const int*>` vs `<float*>`, folding recursively through `MemOrPoolAlloc`/`MemOrPoolAllocSTL`, the `PoolAlloc` overloads and the `operator new` twins. **Flat T1 is REFUTED**, exactly as the group predicted. |
| (2) CALL-SITE DISPLACEMENT from retail bytes | `bl 0x823715e0` — the survivor |
| (3) HETEROGENEOUS FAN-IN | our own declarations name the element type |

⚠ **Channel 2 is not optional here, and the reason is structural: retail keeps TWO
188-byte body-twins of this shape** — the survivor `0x823715e0` and `fn_827A36F0` — so
body identity **cannot** pick the membership. The displacement was decoded straight out of
`orig/45410914/band.exe` through the PE section table, so it does not depend on
`target_symbol_map.json` naming that address:

```
?CalcNoteWeights@VocalPart@@QAAXXZ @ 0x826f3830, idx 18 @ 0x826f3878
word = 4bc7dd69  =>  bl 0x823715e0      (SURVIVOR, not the twin)
```

**Anti-vacuity:** `icf_pair_adjudicate.py --chasetest` passes all four arms and its
**IN-FAMILY DECOY arm is REFUTED under chase**, so `--chase` does not wave through
arbitrary same-template twins. A prover that cannot refuse proves nothing.

### The prediction MISSED — in my favour — and that needed scrutiny, not celebration

Pre-registered (`389f5db5`, committed before any measurement): **+292 B / +1 fn**.
Measured: **+1004 B / +3 fns**. Two further rows crossed because their only charge was the
same callee name.

⚠ **Two unproven crossings are an integrity risk, not a bonus.** My channel-2 proof covered
`CalcNoteWeights` only. Both were adjudicated on retail bytes *before* landing:

| caller | retail addr | charged idx | resolves to | our spelling |
|---|---|---|---|---|
| `?CalcNoteWeights@VocalPart@@` | `0x826f3830` | 18 | **SURVIVOR** | `mNoteWeights` = `std::vector<float>` (VocalPart.h:90) |
| `?WeightedCrowdLevel@BandPerformer@@` | `0x826ee200` | 20 | **SURVIVOR** | `crowdratings` = `std::vector<float>` (BandPerformer.cpp:179) |
| `?Init@TrackData@@` | `0x8277f250` | 22 | **SURVIVOR** | `mLastGemTimes` = `std::vector<float>` (MasterAudio.h:84) |

★★★ **The consistency check that makes this convincing — keep this, it generalises.**
The ONLY caller in the population that reaches the other twin is
`??0DataArraySongInfo@@QAA@PAVDataArray@@0VSymbol@@@Z` at `0x827a4020`, whose six reserve
sites split **4 → survivor** (idx 231/283/335/383) and **2 → TWIN `fn_827A36F0`**
(idx 445/505) — and that is **exactly** the row which improved (99.96795 → 99.98397) and
**did NOT cross, banking 0 bytes**. The row that would have been the false positive is the
one the ruler declined to pay. A membership that forgave wrongness rather than folding
would have shown the opposite pattern.

```
A/B run 20260916-072352 (kinds=['map'], forced re-split both legs, BOTH at a
symbols.txt fixed point, renamer_patched=1830, 0 recompiles as expected)
  Δmatched=+3  Δmasked_equal=+0  Δhonest=+3  Δcode%=+0.009796pp  Δcode_bytes=+1004
  unit net (ALL units) = +3  ==  whole-binary Δmatched = +3   (nothing paid for it)
  units at 100%: 189 -> 189 (0 reached, 0 fell off)
  ALIAS_SUSPECT fired — expected for a map-only patch; the defence is the retail-byte
  proof above, NEVER the `none` control.
```

`Δmasked_equal = +0` with `Δhonest = +3` ⇒ real honest matching, not a funclet artifact.

---

## 3. The two bytes-only rows: same species, and NEITHER is source-fixable

Both are commutative-operand-order `diff_arg` charges. Neither has a source lever, for
**two different and independently established reasons**.

### `SetDifficultyVariables` (768 B) — pre-registered NEGATIVE, deliberately not attempted

2 charges (idx 45, 84): target `add r11,r11,r31`, ours `add r11,r31,r11`. The detector
calls it `COMMUTATIVE_OP_ORDER` / `REGISTER_SWAP` and suggests flipping the operands.

⛔ **That suggestion is a symptom label, not a diagnosis, and acting on it would LOSE
points.** There are **8** such sites, all from the identical construct
`voxCfg->FindArray(X)->Float(diff + 1)` (inlined node addressing: `r31 = diff*8`,
`add` the node base, `addi …,0x8`). The four preceding instructions are byte-identical at
every site **on both sides**. Yet:

| idx | 30 | 45 | 69 | 84 | 99 | 114 | 143 | 172 |
|---|---|---|---|---|---|---|---|---|
| target | A | A | B | A | A | A | B | A |
| ours | A | **B** | B | **B** | A | A | B | A |

(A = `r11,r31`; B = `r31,r11`.) **Both sides emit BOTH orders from one source construct**,
so the order is a per-site compiler tiebreak. A source flip moves all 8 together: it would
fix 2 and break 6, **net −4 charges**. Our B-set `{45,69,84,143}` is a strict *superset* of
target's `{69,143}` — a systematic bias, not noise, but still not source-selectable.
This is permuter-class; the permuter is OFF by standing directive.

### `HandlePhraseEnd` (1120 B) — AG2's 2026-07-26 claim RE-TESTED and it REPLICATES

Sole charge at idx 132: target `mullw r10, r29, r3`, ours `mullw r10, r3, r29`, where `r3`
is the live return of the `bctrl` at idx 131 (`GetIndividualMultiplier()`) and `r29` is
`total`. The other three `mullw` sites and both `mulli` sites AGREE, so exactly one
expression is at fault — unlike `SetDifficultyVariables`, a source flip here moves exactly
one site. That is why it was worth re-testing despite the note.

**Why re-test at all:** the note predates the 2026-08-12 `name_check` ruler flip AND the
`.end()` idiom change to this very body, and allocation/scheduling are global, so a
canonicalisation claim measured on a different body is not binding on this one.

**It replicates.** Flipping to `indMult * total` recompiled the TU (`[5/14] MSVC …
VocalPart.obj`, so this is **not** an absent-vs-absent reading) and left idx 132
bit-identical; the whole binary was byte-identical to baseline
(43978 / 4131656 / 40.320374 / 50.014150).

⚠ **It also refuted my own model, which is the useful part.** I predicted MSVC *reverses*
the operands — our source already reads retail's textual order and emits the reverse. It
does not reverse: it emits `indMult` first at this site and at the `odPts`/`bandPts`
multiplies whatever the source says. A "lower register number first" rule fits all four of
our `mullw` sites (r3<r29, r26<r27, r26<r28, r11<r30) and retail violates it only at idx
132 — but that rule **fails on `SetDifficultyVariables`**, where we emit `add r11,r31,r11`
(31>11) at four sites. **There is no register-numbering rule either.**

Reverted (Δ0 with no accuracy argument either way: since source order is provably not
respected here, retail's bytes say nothing about retail's *source* order). The in-source
note was upgraded with the re-test so the next lane does not spend the build.

---

## 4. `IsEmptyPhrase` (116 B) — narrowed, still open, one more hypothesis refuted

28 instructions on our side vs 29 on retail, **identical everywhere** except one extra
instruction at idx 14, between `bne cr6` and `subic.`:

```
10 lwz    r10, 0x10(r11)     ; unk10
11 lwz    r8,  0x14(r11)     ; unk14
12 cmpw   cr6, r10, r8
13 bne    cr6, ...
14 clrrwi r10, r10, 0        <-- TARGET ONLY (base 112 B vs target 116 B)
15 subic. r10, r10, 0x1
16 blt    ...
```

★ **`clrrwi r10,r10,0` is NOT a no-op self-move** — it encodes `rlwinm r10,r10,0,0,31`,
which on PPC64 zeroes the upper 32 bits: it is MSVC's **zero-extend-32-to-64** idiom. It
sits *before* the `subic.`, so the conversion applies to `unk10`, not to `idx`. A surviving
self-move after coalescing means the IR held two pseudo-registers with a real conversion
node between them. That is why ET's plain separate `int` local was inert — an `int` copy
needs no extension.

**Refuted here (new):** `int idx = (unsigned int)phrase->unk10 - 1;` — an explicit unsigned
intermediate, the direct expression of the mechanism above. **Inert**, fuzzy unchanged at
96.55173, with `recompiles=1` so the negative is not vacuous. MSVC's optimizer evidently
knows `lwz` already yields a 64-bit-clean value and elides the cast, so **an explicit cast
cannot reach this**.

**The oracle offers nothing**: rb3-Wii's `VocalPart.cpp:339` is character-for-character our
source (`int idx = phrase->mNoteStart - 1;`). Both `unk10` and `unk14` are plain `int` in
`src/system/beatmatch/VocalNote.h`, so there is no member-level signedness lever either.

Standing after two lanes: **the construct must make the conversion non-elidable**, which an
explicit cast does not. Whoever opens it next should start from "what makes MSVC unable to
prove the value clean", not from another cast spelling.

---

## 5. What I did NOT do, and why

- **`GetBestHit` (528 B) — not opened.** ET established it is 4 instructions of pure
  scheduling around a **byte-exact** callee (`ScoreNote`, fuzzy 100 / mpn 100), with
  `target_size == base_size == 528`, and that statement reordering is normalised away. A
  byte-identical callee has a byte-identical clobber set by construction. I had no
  non-clobber lever that differed from what ET already refuted, so I did not spend a build
  confirming someone else's negative. The brief asked me to say why if I believed it was
  different — I did not believe it was.
- **`GetNoteSliceWeight` (484 B) — not opened**, and this is the one real gap in my
  coverage. Retail saves `r30` and uses `__savefpr_21` (f21..f31); we save no `r30` and use
  `__savefpr_20` (f20..f31), i.e. **one extra FPR live** in our loop, with the whole
  residual being a shifted FPR numbering. ET already refuted the two obvious causes (a local
  copy of the global; un-hoisting the `2.0f` literal, which MSVC re-hoists). It is an
  allocation-pressure residual with no identified source lever and I judged the alias work
  the better use of the remaining budget. Unattempted, not refuted.
- **No permuter** (OFF by standing directive), although `SetDifficultyVariables`,
  `HandlePhraseEnd` and `GetBestHit` are all textbook permuter-class.
- **Did not re-fund the callee-clobber census** (ET: 0.87× against
  impossible-by-construction arms), the MWCC shim vein, or declaration/definition order.
- **Did not touch the 6 unadjudicable `data() + size()` sites** ET handed off; they still
  need their enclosing functions pinned before the change is adjudicable in either
  direction.

## 6. Handoff

1. ★ **The alias vein in this unit is not exhausted, and it is the highest-yield thing
   here.** One membership bought 1004 B across three units. The generalisable recipe:
   a row whose *only* charge is a `diff_arg` on a `bl` where target and base name the same
   method on different 4-byte element types is an alias candidate — but it must be proven
   on retail bytes by **decoding the call-site displacement**, because a template family can
   have several surviving twins (this one has two) and body identity cannot pick between
   them. Grep `scripts/symbol_aliases.json` first; the charge may already be forgiven for a
   sibling spelling, which is the positive control.
2. **`IsEmptyPhrase` is still one 4-byte instruction from +116 B / +1 fn**, now with two
   refuted hypotheses (separate `int` local — ET; explicit `(unsigned int)` cast — me) and a
   sharper statement of what is needed (§4).
3. ⛔ **Do not re-open `SetDifficultyVariables` or `HandlePhraseEnd` as commutative-order
   fixes.** Both are measured/structural negatives (§3). `SetDifficultyVariables` would
   actively regress if the detector's suggestion were followed.
4. **`GetNoteSliceWeight` (484 B) is the largest genuinely unexamined row left in the unit.**
