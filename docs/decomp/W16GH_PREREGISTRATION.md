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
