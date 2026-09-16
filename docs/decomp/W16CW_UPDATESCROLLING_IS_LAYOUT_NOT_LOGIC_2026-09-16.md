# W16-CW — `?UpdateScrolling@VocalTrack@@QAAXM@Z` is a CODEGEN-SHAPE wall, not a reconstruction

**Date:** 2026-09-16 · **Branch:** `w16-cw` (off main `7b7b2edb`) · **Result: Δ0, no source
edits, by construction.** · **Row unchanged at fuzzy 72.27805 / mpn 73.985695, 8,948 B.**

## Verdict, up front

`UpdateScrolling` is **not** an unfinished body. Our source is semantically and
structurally **correct**. The entire 27.7 pp gap is **MSVC basic-block layout plus
register allocation** over a 2,511-instruction function — which is the permuter's
class, and **the permuter is banned by standing user directive**.

Because `matched_code` is all-or-nothing per row, **no partial work on this row buys
a single byte**. Closing it requires reproducing retail's exact block layout *and*
exact register allocation across 2,511 instructions. **That is not a one-lane job and
it is not a sculpt. Do not re-open it as one.**

⚠ This **corrects the prior at which W16-CM declined the row.** CM judged "72 % means
reconstruction, not near-miss sculpt" — entirely reasonable on the number alone, and
the right call for its lane. The number was misleading: it is a near-miss of a kind
the campaign has no sanctioned tool for.

## The evidence (all measured, graded `name_check` ruler)

Charges: **2,511 rows, 1,139 equal ⇒ 1,372 charges.** Breakdown:
`diff_arg 754 · insert 274 · delete 269 · replace 58 · diff_op 17`.

### 1. Neither side calls anything the other does not

Census of unmatched `bl` over **all 2,511 rows** (not the rendered context window —
that undercounts, it renders only 1,336 rows):

| | count | composition |
|---|---|---|
| retail-only calls | **16** | 12 = the reordered `GetLyricColor`×8 / `GetLyricAlpha`×4 block; 2 more (`GetNextLyricPlate`, `GetLastBakedLyric`) are halves of reorder pairs |
| our-only calls | **17** | 10 = that same block; rest (`GetNextLyricPlate`, `HookUpParents`, `GetLastBakedLyric`, `Empty`, `Width`) pair with retail-side deletes |

After accounting for reordering, **the call graphs are identical**.

### 2. The two functions are the same code

`target_size` **8,948** vs `base_size` **8,968** — we are **+5 instructions**.

Opcode-multiset control (independent of any human reading of the diff): over 73
distinct opcodes, **total absolute imbalance = 105 of 2,237 instructions = 4.69 %**.
The imbalance profile is exactly what a layout inversion predicts:

    bge +10 · beq -7 · bgt -3 · blt +2     <- branch-polarity flips (swapped fall-through)
    lwz +12 · addi -12                     <- marginally more reloading (register pressure)

### 3. 34 % of all insert/delete is ONE block in the wrong place

Clusters 55 (**89 deletes**, idx 1097-1185) and 87 (**94 inserts**, idx 1652-1749) are
**the same source block** — `*itPPtr = itT - &notes->mNotes[0]; int colorBase = …;`
then 8×`GetLyricColor` + 4×`GetLyricAlpha`. Same `li r10,0x34`, same `subfe`, same
`or r30,r11,r10`, same `ori r29,r30,0x1`, same call sequence.

Settled as a **reorder, not a duplication**: the full census finds `GetLyricColor`
**exactly 8× on each side** and `GetLyricAlpha` **exactly 4× on each side**, with none
matched. **183 of 543 insert/delete instructions (34 %) are this one block.**

Physical layout:

    retail:  [… 0x8118] [COLOR BLOCK 0x811c-0x827c] [PHRASE-LOOP BODY 0x8280-0x8918]
    ours:    [… 0x1dbfc] [PHRASE-LOOP BODY 0x1dc00-0x1e264] [COLOR BLOCK 0x1e268-0x1e3ec]

★ **It cannot be a source reordering**: the block stores `*itPPtr = itT - begin`, which
depends on `itT`'s value *after* the phrase loop. It is the **loop-exit tail**, and
both sides branch to it from the same five `break` sites (idx 1046, 1076, 1096, 1188,
1195). Retail falls through the last test into the tail (`beq → body`); we branch to it
(`bne → tail`). Identical CFG, mirrored block placement. The same inversion repeats at
the bake loop (retail `b 0x915c` to a late header; we fall straight into the body).

## Refuted — do NOT re-run these

Two concrete hypotheses were formed and **both were killed by retail bytes**. Recording
them is most of this lane's value.

- ⛔ **`MILO_WARN`/`TheDebug` missing-debug-block lever — REFUTED for this function.**
  This was the strongest prior: it is CM's own lever that took `PrepareNoteTubes`
  73.40 → 90.42 on this very file, and `MILO_WARN` does compile away here (`Debug.h:269`
  `MiloStripEval`) while `TheDebug` does not. **Retail contains no such block in
  `UpdateScrolling`** — the unmatched-call census above is the proof: there is no
  `MakeString`/`TheDebug` call on the retail side at all. Prediction failed.
- ⛔ **`float &lastLyricX = staticLyrics ? (lead ? unk23c : unk240) : scrollingLastX;`
  reference-through-pointer — REFUTED.** At idx 1082 retail reads the value from FPR
  `f23` while we do `lwz r11,0x78(r1); lfs f0,0x0(r11)`, which looked like retail using
  a value where we use a reference. It does not: retail loads `0x23c(r31)` and
  `0x240(r31)` at idx 309/313 exactly as we do at 316/318, and pointer-dereferenced
  float accesses are **23 (retail) vs 24 (ours)**. Same construct. The `f23` read is
  register allocation, nothing more.
- ⛔ **No wrong-constant defect exists** (the W16-CQ class). Of 60 apparent immediate
  divergences, nearly all are `addi rX, r1, <slot>` stack-frame offsets. The only two
  non-stack pairs are **swapped, not wrong**: idx 1024/1034 carry `0x4a` and `0x4c` on
  *both* sides in opposite order; idx 1392/1393 vs 1547/1548 carry `li 0x0`/`li 0x1` on
  both sides in opposite order.
- ⚠ **Do NOT "fix" the `goto window_ok` toward the rb3-Wii oracle.** The oracle writes
  `if (!playerOk || !windowOk || !sectionBoundsOk) break;`; ours uses two
  `goto window_ok` then `break`. **Ours is the better match** — retail emits
  `beq → body` / `beq → body` / fall-through-to-break, i.e. `break if (A && B)`, which is
  our form. The oracle's `||` chain would emit branch-to-break, which is the shape we
  are already trying to get away from.

## Confirmations our source already wins (leave alone)

- `if (mPlayer->IsGameOver())` at the top is **right**; the rb3-Wii oracle's
  `if (mPlayer->IsNet()) return;` is **wrong** (and leaves dead code below it). idx
  10-12 `lbz r11,0x238(r3)` is **equal**.
- Struct sizes are right: `li r10,0x34` (VocalNote, 52 B) and `mulli r10,r10,0x38`
  (VocalPhrase, 56 B) are **equal** on both sides.
- `fn_82BAF0C8` (idx 1762) and the `NewFrame@CameraInput` / `TrackNum@TrackConfig`
  pair (idx 1771) both read **equal** — placeholder-forgiven / already aliased. Not
  defects; do not chase them.

## Routing recommendation

The row needs **layout control**, not source logic. Options, in order of honesty:

1. **Leave it.** 8,948 B at 0 collectable bytes until the *last* charge closes. There
   are 119 further insert/delete clusters beyond the two big ones, plus 754 register/
   stack `diff_arg`s. Nothing here converts to bytes incrementally.
2. If the permuter directive is ever revisited, **this row is the canonical candidate**:
   the source is correct, the call graph matches, and the residue is precisely
   allocation + scheduling.
3. Do **not** fund a "body port" — there is no body to port. That would re-derive
   source we already have right and risk regressing the 179/220 rows the unit holds.

## Side-observation (flagged, not chased — per brief)

Three named rows in `default/VocalTrack` sit at **fuzzy 0** and look misfiled:
`336 B` `?_M_insert_overflow_aux@…Unlockable…`, `136 B` `??1?$vector@…Unlockable…`,
`96 B` `??$__destroy_range_aux@…Unlockable…` — **568 B** of anonymous-namespace
`Unlockable` vector templates, which is meta_band material, not vocals. Confirmed
present at fuzzy 0.0 in `report.json`. This smells like a **pin/attribution** question,
not source work.

## Reproduction

    python3 - <<'EOF'   # after a full ./tools/ninja-locked in the worktree
    import json,collections
    J=json.load(open('<diff json from run_diff_inspect>'))
    T=collections.Counter(); S=collections.Counter()
    for r in J['instructions']:
        t=r.get('target') or {}; b=r.get('base') or {}
        if t.get('opcode'): T[t['opcode']]+=1
        if b.get('opcode'): S[b['opcode']]+=1
    print(sum(abs(S[k]-T[k]) for k in set(T)|set(S)), 'of', sum(T.values()))
    EOF

⚠ Use the **graded** ruler (`run_diff_inspect` resolves it from `report.json`
`provenance.diff_config`). A `none`-ruler reading of this row is structurally blind to
the relocation-name class and will mis-price it.
