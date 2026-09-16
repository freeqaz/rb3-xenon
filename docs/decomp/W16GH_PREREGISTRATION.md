# W16-GH pre-registration — OvershellSlot::OnMsg(ButtonDownMsg&)

Written BEFORE any source edit or measurement. Worktree `~/tmp/wt-w16gh`, branch
`w16-gh`, off main `85b84e32`.

## Baseline (verified, not inherited)

`?OnMsg@OvershellSlot@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z`, unit
`default/OvershellSlot`, 1,396 B, **fuzzy 99.30373 == mpn 99.30373**.
Charges 1 insert / 1 delete / 43 diff_arg / 0 replace — reproduced exactly.
Immediate deltas (ours − retail): +8 x38, +16 x3, +4 x1, -16 x1. Reproduced exactly.

## Diagnosis

r31 is the FRAME POINTER (`subi r31,r1,0xf0` at [2] executes BEFORE `stwu` at [3],
so r31 == the new r1). `this` is in r25/r30. Every `OvershellSlot::<member>`
attribution printed by `run_objdiff`'s "Offset Mismatches (resolved)" block is
therefore FALSE — verified by reading the operands: all 22 are `0xNN(r31)`.

The defect is ONE thing: **we allocate three distinct 4-byte `Symbol` temporary
slots where retail allocates one.**

| instr | temporary | retail | ours |
|---|---|---|---|
| [68] | `Symbol("button_pulse")` -> `Message` ctor | 0x54 | 0x54 |
| [78] | `GetState()->GetView()` sret #1 | 0x54 | 0x54 |
| [85] | `GetState()->GetView()` sret #2 | 0x54 | **0x58** |
| [97] | implicit `Symbol(const char*)` -> `SetType` | 0x54 | **0x5c** |

8 extra bytes of locals; 0xf0 + 8 = 0xf8 is not 16-byte aligned, so the X360 ABI
rounds the frame to **0x100**. That is the whole +0x10, and every other charge is
the resulting uniform +8 slot shift.

Mechanism: MSVC resets its temporary-slot pool at each STATEMENT but assigns
distinct slots to temporaries WITHIN one full-expression. Our source puts all
three of temps [78]/[85]/[97] inside the single full-expression
`btnMsg.SetType(GetView()==join || GetView()==finding ? A : B)`.
Evidence for the mechanism being statement-scoped and not something else: temp
[68] (a different statement) and temp [78] DO share 0x54 on our side already.

`Symbol` is 4 bytes and trivially destructible, so this is not a destructor /
EH-state effect.

## Prediction (attempt A)

Split the one full-expression into three statements, preserving the emitted
control flow (compare join -> branch; compare finding -> branch; select literal;
one `Symbol` ctor):

```cpp
const char *pulseType;
if (GetState()->GetView() == join)          pulseType = "button_pulse_unjoined";
else if (GetState()->GetView() == finding)  pulseType = "button_pulse_unjoined";
else                                        pulseType = "button_pulse_joined";
btnMsg.SetType(pulseType);
```

**P1 (primary):** all three temps collapse to 0x54, frame goes 0x100 -> 0xf0, and
all 43 `diff_arg` charges vanish.
**P2 (secondary, genuinely uncertain):** the [99]/[102] insert/delete pair is a
scheduling artifact (retail hoists `lwz r4,0x0(r3)` above two `mr`s and leaves a
dead `mr r11,r3`). I do NOT know whether it is downstream of the slot choice. If
it survives, the row lands ~99.8-99.9 and 1,396 B are still NOT collected,
because `matched_code` keys on `fuzzy == 100` and is all-or-nothing per row.

### Falsifiers — any of these refutes P1
- Frame stays 0x100 after the edit.
- Fewer than 3 temps collapse (e.g. only [97] moves to 0x58): mechanism is right
  but the restructure is insufficient.
- The branch structure changes (instruction count != 350, or new insert/delete
  rows beyond the existing [99]/[102] pair): the restructure is not codegen-neutral.
- Score goes DOWN.

### Control that discriminates (required before concluding anything is inert)
Attempt B, run only if A fails: hoist ONLY the `SetType` argument, leaving the
`||` intact. This must move [97] from 0x5c to 0x58 and leave the frame at 0x100.
If B moves nothing at all, the pipeline is dead and no conclusion about A is
licensed.

## Stopping condition
If no local/temporary restructuring brings the frame to 0xf0, report **at_limit**
with each attempt and its measured score. Permuter is OFF by directive.

---

## Attempt A result (measured)

**P1 CONFIRMED.** Frame 0x100 -> 0xf0 on [2]/[3]/[348]; all three Symbol temps
now share 0x54 ([68]/[78]/[85]/[97]); instruction count unchanged at 350.
Charges: 1 ins / 1 del / 43 diff_arg  ->  1 ins / 1 del / 1 diff_arg / 1 diff_op.

New residual, as anticipated in the falsifier list ("branch structure changes"):
the two literal-loading blocks are SWAPPED in layout.
- retail: test1 `beq L_unjoined`; test2 `beq L_unjoined`; fall-through = joined;
  `b L_cont`; L_unjoined: unjoined.
- ours:   test1 `beq L_unjoined`; test2 `bne L_joined`; fall-through = unjoined;
  `b L_cont`; L_joined: joined.
MSVC placed the `else if` branch's `unjoined` assignment inline because it comes
first textually, and inverted test2 to reach it.

## Pre-registration — attempt A2

Flip the second test's polarity so the `joined` assignment is the middle
(fall-through) branch and both `unjoined` assignments tail-merge into the
out-of-line block both tests branch to:

```cpp
const char *pulseType;
if (GetState()->GetView() == join)        pulseType = "button_pulse_unjoined";
else if (GetState()->GetView() != finding) pulseType = "button_pulse_joined";
else                                       pulseType = "button_pulse_unjoined";
```

**P3:** [84]'s branch target and [91]'s opcode both match retail; charges drop to
the [99]/[102] insert/delete pair only.
**Falsifiers:** [91] stays `bne`; or the blocks stop tail-merging and the
instruction count leaves 350; or a new charge appears; or the frame leaves 0xf0
(would mean the temp-slot fix is coupled to block layout, which I do not expect).
**P4 (uncertain, unchanged):** I do not know whether [99]/[102] is downstream of
block layout. If it survives A2 the row lands ~99.8 and collects 0 bytes.

## Attempt A2 result (measured)

**P3 CONFIRMED.** [84] and [91] both match. Charges: 2 of 350 (the [99]/[102]
insert/delete pair only). Frame still 0xf0. **fuzzy 99.30373 -> 99.42693**
(read from report.json, graded ruler).

**P4 resolved NEGATIVELY: [99]/[102] is orthogonal to statement structure.** It
was present in the BASELINE (original ternary source) and survived BOTH
restructures unchanged. It is a scheduling artifact:
  retail  bl Symbol::Symbol ; lwz r4,0x0(r3) ; mr r11,r3 ; mr r3,r26 ; bl SetType
  ours    bl Symbol::Symbol ; mr r11,r3 ; mr r3,r26 ; lwz r4,0x0(r11) ; bl SetType
Retail's `mr r11, r3` is DEAD -- nothing reads r11 afterwards. That is the
signature of copy-propagation (r11->r3 in the load) plus a scheduler hoist of the
load above both `mr`s, with the now-dead copy left behind. Decisive context: the
function contains FIVE structurally identical `bl Symbol::Symbol ; mr r11,r3 ;
mr r3,rX ; lwz r4,0x0(r11) ; bl <callee taking Symbol by value>` sites
([70],[157],[208],[273],[98]). Retail hoists at EXACTLY ONE of the five and our
build hoists at none, so the asymmetry exists WITHIN retail and is not a property
of the source shape.

## Pre-registration — attempt V1 (last probe before stopping)

`btnMsg.SetType(Symbol(pulseType));` -- make the temporary explicit instead of an
implicit conversion, to test whether the front-end's temp FORM (not its
statement) steers the schedule.
**P5:** no change (I expect the implicit conversion and the explicit temp to
produce identical IL). **Falsifier for "not source-steerable":** ANY change to
[99]/[102] would show the form does matter and reopen the vein.

## Attempt V1 result + DISCRIMINATION CONTROL

**P5 CONFIRMED: V1 is a true no-op.** `SetType(Symbol(pulseType))` and
`SetType(pulseType)` produce byte-identical codegen; charges stayed at 2.

**Control (required, because byte-identical is also what a dead pipeline
produces):** reverted V1 and injected an immediate-only sabotage in the same
build -- `mOvershellDir->Handle(msg, false)` -> `true`, i.e. `li r6,0x0` ->
`li r6,0x1` at [322]. Measured: charges 2 -> 3, the new one being exactly
`[322] diff_arg: li [off:+1]` -- predicted index, predicted kind. **The witness
fires.** V1's inertness is therefore a measurement, not a dead pipeline.

## Pre-registration — attempt V2 (final probe)

Give the Symbol a NAME in a dead scope, so it is an object rather than a
temporary, and see whether MSVC then keeps the ctor's returned pointer in r3 and
loads through it (retail's `lwz r4, 0x0(r3)`) instead of saving to r11:

```cpp
{
    Symbol pulseSym(pulseType);
    btnMsg.SetType(pulseSym);
}
```

**P6:** [99]/[102] resolves and the row reaches fuzzy 100 (+1,396 B).
**Falsifiers:** (a) the frame leaves 0xf0 -- a named local that does not share
slot 0x54 with the later on_cancel/on_start/on_view_modify temps at
[155]/[206]/[271] would re-grow the frame, which is the main risk; (b) the load
becomes `lwz r4, 0x54(r31)` (frame-relative) -- same cost as now, still not 100;
(c) no change at all.
**If V2 fails: STOP and report at_limit on the residual**, per the pre-registered
stopping condition. The permuter is OFF by directive and is not an option.

## Attempt V2 result (measured) — REFUTED, reverted

Falsifier (b) fired exactly as written: the load became frame-relative
`lwz r4, 0x50(r31)` instead of retail's `lwz r4, 0x0(r3)`, and the named local
additionally displaced slot assignment. Charges 2 -> 12, fuzzy 99.42693 -> ~99.1
(4-byte shift on [68]/[78]/[85]/[97]/[155]/[206]/[271], OFFSET_SWAP at
0x50/0x54, and an extra `mr r11,r3` delete). Reverted; score restored to exactly
99.42693.

**STOPPING CONDITION REACHED.** Reporting at_limit on the [99]/[102] residual.

## Whole-binary A/B (tools/ab_measure.py --patch, base 85b84e32)

Note: `main` advanced from 85b84e32 to 32322eef during this lane, so the A/B is
based on the lane's ACTUAL parent, not on current main.

```
leg A: matched=44139 masked=23323 honest=20816 code%=40.692772  (settled, 0 recompiles)
leg B: matched=44139 masked=23323 honest=20816 code%=40.692772  (1 recompile, settled)
  dmatched=+0  dmasked_equal=+0  dhonest=+0  dcode%=+0.000000pp  dcode_bytes=+0
  dfuzzy=+0.000019pp   (legA 50.497276 -> legB 50.497295)
  units at 100% [mpn]: 195 -> 195 (+0);  [all-rows-fuzzy]: 173 -> 173 (+0)
```

**Δ0 on both headline measures, as pre-registered.** `matched_code` keys on
`fuzzy == 100` and is all-or-nothing per row, so a row that moves 99.30373 ->
99.42693 collects nothing. The only measure that moved is aggregate `fuzzy`, by
exactly the amount one 1,396 B row improving 0.1232 pp of its own score
contributes. That is the honest signature of this change and it is NOT a reason
to withhold it: 43 charges of a real codegen defect (a 16-byte-wrong stack frame)
were closed, and the row now sits ONE scheduling instruction from collecting
1,396 B.
