# W16-GE — `OvershellPanel::ResolveSlotStates`: the last 6 charges are ONE
# tail-merge PLACEMENT choice, and the source CFG is measurably NOT the lever

Date 2026-09-16 · base main `6159dfd5` · worktree `~/tmp/wt-w16ge` · branch `w16-ge`
Ruler: **graded `name_check`**, resolved from `report.json`'s own `provenance`
(confirmed: `objdiff-cli diff` fuzzy == `report.json` fuzzy to the last digit).
Every number below is from a **full `./tools/ninja-locked`** on a tree that
`verify_objs_patched.py --check` calls a fixed point of all six passes.

## Verdict

**DRAINED for source work — DO NOT RE-FUND as a source lane.**
The row does not cross. `matched_code` is all-or-nothing per row, so 98.84%
pays **exactly 0 bytes**; the 1,416 B stays unbanked. Remaining distance is
attributed in full below to a single MSVC block-layout decision.

## Measured state (unchanged by this lane)

    row ?ResolveSlotStates@OvershellPanel@@QAAXXZ
    size 1416   base_size 1416   -> SIZE IDENTITY EXACT
    fuzzy 98.841805   mpn 98.870056   350 / 356 instructions equal
    charges: 2 delete (113,114) + 2 diff_arg (115,131) + 2 insert (129,130)

## Two corrections to the brief this lane was given

1. **The two `diff_arg` are BRANCH DESTINATIONS, not relocation-name charges.**
   `b 0x4c2c` vs `b 0x1cdac`, and `b 0x4994` vs `b 0x1d00c`. Both sides call the
   *same* callee `?ShowState@OvershellSlot@@`. So the standing
   fold-alias-vs-wrong-callee adjudication does **not** apply to this row, and
   **no alias may be installed here** — there is no callee question to forgive.
   A lane briefed "2 diff_arg" will reach for `symbol_aliases.json` by reflex;
   that would be a fabricated alias lifting the score by construction.
2. The brief quoted `mpn 98.84180`, which is the **fuzzy** value. True mpn is
   **98.870056** (`mpn >= fuzzy` always).

## Diagnosis

Both compilers **tail-merge** the two `curSlot->ShowState(...)` arms of the
`ShouldSeeRealGuitarPrompt` if/else into one `mr r3,r30; bl ShowState`. Only
**which copy survives** differs:

    retail  copy kept in the THEN arm @0x4994; path B does `li r4,0x49; b BACK`
    ours    copy kept in the ELSE arm @0x1cdac; path A does `b FORWARD`

The instruction multiset is identical — which is *why* size identity is exact
and 350/356 are equal. The region extent is **0x48 bytes on both sides**. This
is pure basic-block placement: no missing code, no wrong callee, no struct
offset, no register-allocation difference.

Ruled out by measurement, not assumption: **a differing merge GROUP**. Both
sides emit exactly **2** `bl ShowState` and **66** `bl` total, so both merged
the same two source calls and left the third (the `kState_SignInWait` arm,
idx 199) separate.

## The lever that was tried, and why it was the right one

Retail's path B is `li r4,0x49` **with no store**. `ossID` is address-taken
(out-param of `ShouldSeeRealGuitarPrompt`, `addi r5,r31,0x50`) and therefore
memory-homed, so the naive `ossID = 0x49; goto L;` would force a `stw`+`lwz`,
add instructions and **break the exact size identity**. The shared value must
therefore travel in a local that is never address-taken. So:

    OvershellSlotStateID idToShow;              // never address-taken
    if (ShouldSeeRealGuitarPrompt(curLocalUser, ossID)) {
        curLocalUser->SetHasSeenRealGuitarPrompt();
        idToShow = ossID;                       // predict lwz r4,0x50(r31)
    showstate:
        curSlot->ShowState(idToShow);           // predict mr r3,r30; bl ShowState
    } else {
        ...
        if (chooseChar) {
            idToShow = (OvershellSlotStateID)0x49;   // predict li r4,0x49
            goto showstate;                          // predict b BACK
        } else if ...

This reproduces retail's control flow **and** its register usage exactly on
paper. It is not a guess: rb3-Wii's own decomp of this very function uses
`goto therest;` with a comment that goto-based tail folding is what matches, so
HMX demonstrably wrote gotos here.

### MEASURED: BYTE-IDENTICAL. Completely inert.

    before  fuzzy 98.841805  mpn 98.870056  1416/1416  charges 2del+2ins+2diff_arg
    after   fuzzy 98.841805  mpn 98.870056  1416/1416  charges 2del+2ins+2diff_arg

Not one charged site moved. The TU really recompiled (`[5/14] MSVC
…OvershellPanel.obj`, 14 edges including the patchers), the `goto` is live in
the source, and the compiler emitted **no** label/goto warning — so this is a
real inert result, not a silently-dropped edit or an absent-vs-absent A/B.

### The control that makes that negative trustworthy

"Nothing moved" is worthless unless the instrument *can* move. Bad-edit control,
an **immediate** (CLAUDE.md: a bad-edit control keyed on a relocation argument
measures nothing) — `0x49` -> `0x4A`:

    fuzzy 98.841805 -> 98.83898,  equal 350 -> 349,  new diff_arg at idx 128
    (`li r4, 0x49` vs `li r4, 0x4a`)

A one-instruction change is plainly visible. The witness discriminates.

## What this establishes

**MSVC canonicalizes this CFG before its block-layout / cross-jump pass.** The
source-level control-flow shape is *not* the lever for the merge direction:
writing retail's exact shape by hand produces our layout, byte for byte.

This is strictly stronger than the `permuter-class` label commit `2532bfd1`
left on the row. That label meant *"we did not try"*; this lane measured that
**the most direct structural attack is dead**. The merge direction is decided
by the compiler's own layout heuristic from a normalized CFG, so it is not
reachable by any semantically-neutral rewrite of this if/else.

## Deliberately NOT done, and why

- **No alias installed.** See correction 1 — there is no callee question here,
  and an unproven alias lifts the score by construction.
- **No condition inversion / else-if re-nesting.** Inverting the inner
  `InChooseCharFlow && cMgr && ...` test would change the branch polarity and
  destinations at idx 122/124/127, which currently match retail exactly. That
  trades 3 matching instructions for a speculative layout flip — a guaranteed
  loss against an unproven gain.
- **No declaration-order lever.** Known INERT for register scheduling; declaration
  order controls stack slots only.
- **No permuter.** OFF by standing directive. Note this row is precisely the
  permuter's domain, and it is the honest place to send it *after* the ceiling
  work — the prize is a clean 1,416 B behind a pure layout choice.
- **No `total_code` / ceiling re-measurement.** Nothing this lane did moves a
  denominator; re-deriving one would only invite a stale figure being quoted.

## Whole-binary result

Pre-registered before the final build (`~/tmp/w16ge_prereg.md`) and **held
exactly on every key**:

    matched_functions       44138   (D 0)
    matched_code          4168232   (D 0)
    matched_code_percent 40.677315  (D 0)
    fuzzy_match_percent    50.4972  (D 0)
    masked_equal_functions  23323   (D 0)
    honest                  20815   (D 0)

The lane lands **no net source change**: bytes banked **0**. What it lands is
the measured negative, so the next lane does not re-buy the goto.
