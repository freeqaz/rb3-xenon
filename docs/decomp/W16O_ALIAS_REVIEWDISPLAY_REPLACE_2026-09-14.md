# W16-O — W16-N's four NOT-done items

Lane `w16-o`, worktree `~/tmp/wt-w16-o`, branch off main `fad9842d`.
Ruler: `functionRelocDiffs=name_check` (graded, read from `report.json`
`provenance.diff_config` — not assumed).

## Whole-binary, pre → post (`build/45410914/report.json`, int-coerced)

| measure | baseline | final | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,210 | **43,214** | **+4** |
| `matched_code` | 3,937,416 | **3,937,960** | **+544 B** |
| `matched_code_percent` | 38.426... | 38.434284 | +0.0079 pp |
| `total_code` | 10,245,956 | 10,245,956 | 0 |

Per step, each measured on a FULL `./tools/ninja-locked` (exit code written to
a file and read on the next line — never `| tail; echo rc=$?`), by
`tools/rowset_snapshot.py diff`:

| step | predicted | measured |
|---|---|---|
| 1 alias (list level) | +2 / +260 B | **+2 / +260 B** |
| 1 alias (element level) | +1 / +108 B | **REFUSED — not proven at T1** |
| 2 `ClassName@ReviewDisplay` | −60 B (brief) | **Δ0** (re-homed, not surrendered) |
| 3 `RndTransformable::Replace` v1 | row crosses | +1 / +0 B (mpn 100, fuzzy < 100) |
| 3 `RndTransformable::Replace` v2 | +0 / +124 B | **+1 / +124 B** |
| 4 `RndDir::Export` | +1 / +160 B | **+1 / +160 B** |

Validators, all run on the final tree: `scripts/verify_ruler_agreement.py
--check` rc=0; `scripts/verify_objs_patched.py --check` rc=0 (fixed point of
all six post-compile passes, 1043/1043 declared objects paired);
`tools/icf_alias_finder.py --validate` **PASS — 1,381 map-consistent, 248
tolerated, 0 CONTRADICTED (fatal), 1,630 groups**.

---

## Item 1 — the `fn_824C93C0` fold alias: SPLIT VERDICT (one half installed, one REFUSED)

The brief framed this as ONE check worth +3 / +368 B. It is **two claims at two
levels of a recursive fold**, and only the list-level one survives.

**INSTALLED — the list-level alias.** Group 899 at `0x824c93c0`, survivor
`operator<<(BinStream&, const list<HamCamShot::Target>&)`, folded spelling
`operator<<(BinStream&, const list<EventAnim::EventCall>&)`. T1 held: bodies
byte-IDENTICAL modulo relocated fields, **4 of 4 relocation TARGET NAMES
equal**, normalization notes empty, gate (a) clean (exactly one map-resident
survivor), pigeonhole clean (neither spelling appears in a second group), and
not a relocation-free thunk — so the masked-T1 vacuity guard does not apply.
Evidence recorded in `scripts/symbol_aliases.json`.

**REFUSED — the element-level alias.** Retail's element serializer is 72 B with
2 relocations; ours (`HamCamShot::Target`'s 12-field `operator<<`,
`src/system/hamobj/HamCamShot.cpp:452`) compiles to 316 B with 17. A 244-byte
gap is far beyond either known reader artifact (the STLPORT-1 one-sided 8-byte
EH-prefix billing, and the W39-PUSHBACK 4-byte `bctr`-carve). Independently
corroborated: the 72 B row already reads fuzzy 100.0. **This is an exposed
source divergence — our `Target` serializer writes 12 fields where retail
writes far fewer — not a fold.** Installing it would have been forgiveness of a
real bug.

Measured **+2 / +260 B**, exactly the pre-registered list-level figure. The
missing 108 B of the brief's +368 is precisely the refused half — the shortfall
is the finding, not a miss.

> Note on method: `tools/w33_fold_adjudicate.py --self-test` is a **NO-OP** —
> declared in argparse (line 317) and never referenced anywhere else in the
> file. A self-test that cannot fail is not a control. Discrimination was
> established instead by the fact that pair 2 returned DIFFERENT on the same
> code path.

## Item 2 — `ClassName@ReviewDisplay`: RE-HOMED at Δ0, better than the briefed −60 B

The brief assumed the 48 B had to be surrendered. It did not: the two bodies are
byte-identical (48 B, 1 relocation, same offset), so the row could be **re-homed
rather than dropped**. `.text` carve in `config/45410914/splits.txt` only
(StarDisplay `end:0x8231E588`→`0x8231E550`, ReviewDisplay
`start:0x8231E588`→`0x8231E550`); dtk re-derived `.pdata` by itself, as it must.
Map row `0x8231e550` corrected from the fabricated
`?ForceEmit_ReviewDisplay_StaticClassName@@YA?AVSymbol@@XZ` to the spelling our
obj actually defines, `?ClassName@ReviewDisplay@@UBA?AVSymbol@@XZ` (read from
COFF after a build, not hand-mangled).

Measured **Δ0**. The fabricated `ForceEmit_*` map name is retired at zero metric
cost — a pure accuracy win.

The Lane-AE scatter scaffold in `src/system/bandobj/StarDisplay.cpp` (~line 281)
was deliberately **LEFT IN PLACE**: removing it would unpair the genuine 88 B
`?StaticClassName@ReviewDisplay@@SA?AVSymbol@@XZ` row at `0x8231e450` for no
integrity gain.

## Item 3 — `RndTransformable::Replace`: FIXED, +1 / +124 B. The briefed diagnosis was WRONG.

⛔ **W16-N's stated diagnosis is refuted by retail's own bytes.** It briefed
"our body emits `??_R0` RTTI for a `dynamic_cast` retail does not do". Retail
DOES perform it: `bl __RTDynamicCast` at `0x22dc` with both type descriptors.
**Acting on the brief would have deleted correct code.** The RTTI relocations
never needed fixing either — retail's descriptors are unnamed and `name_check`
FORGIVES `lbl_*` placeholder targets.

The two real divergences:

**(a) The guard.** `/d1reportSingleClassLayout` (authoritative — not the `// 0x`
header comments) gives `RndTransformable` sizeof 0xec with `{vbptr}` at 0x4,
`mParent` at 0x8 (its `mObject` at 0x10), and **`Hmx::Object` as a VIRTUAL BASE
at 0xb8**. Retail loads `mParent.mObject` and then performs the null-checked
vbptr/vbtable upcast before comparing:

```
lwz    r11, -0xa8(r31)     ; r31 = the Object subobject = this+0xb8, so this+0x10 = mParent.mObject
cmplwi cr6, r11, 0x0
beq    cr6, <compare>      ; null in => null out; MSVC null-checks a pointer->vbase upcast
lwz    r10, 0x4(r11)       ; vbptr at +4
lwz    r10, 0x4(r10)       ; vbtable entry = 0xb4
add    r11, r10, r11
addi   r11, r11, 0x4       ; +4 is the VBPTR OFFSET added back, not a member offset: 4 + 0xb4 = 0xb8
cmplw  cr6, r11, r4        ; upcast == from
```

`RefIs`'s X360 branch (`src/system/obj/Object.h:914`) compares `from` against
the **raw** `member.Ptr()` — an address 0xb8 bytes below. This is exactly the
"MI/vbase cases can false-negative here" case its own comment admits, and it is
a **live behavioural bug**: `RndTransformable::Replace` never recognised its own
parent ref.

**(b) No else-arm.** Retail has exactly one `bl` besides `__RTDynamicCast`, and
it is `SetTransParent`. Our `else Hmx::Object::Replace(from, to)` is code retail
does not have. Note the base is *already* empty in the match build — its whole
body is `#ifdef HX_NATIVE` (`Object.cpp:228`) — but with **no LTCG** MSVC cannot
inline across TUs, so the call still emitted a real `bl`. Kept under
`#ifdef HX_NATIVE` so the native build still forwards to `mSinks`.

Two variants, both pre-registered:

- **v1** (upcast + drop else-arm): **+1 fn / +0 B**. 30/31 instructions equal;
  the row reached `mpn == 100` but not `fuzzy == 100` — the arg-blind gap
  exactly as documented. Sole residual: instruction 13, retail
  `cmplw cr6, r11, r4` vs ours `cmplw cr6, r4, r11` — **operand order of the
  `==`**.
- **v2** (write the upcast on the left): **+0 fns / +124 B**. Measured exactly.

Total +1 / +124 B, the full `.pdata` extent. **1 row crossed in, 0 fell out** —
the 24 `$4` adjustor-thunk callers did not move, as the brief required.

Fixed locally in `Trans.cpp` rather than in `RefIs`: `RefIs` has six other call
sites (`DirLoader.cpp:165`, `Msg.cpp:329/338`, `Task.cpp:52/109`,
`DataFunc.cpp:1570`) and the upcast is only a no-op for the single-inheritance
ones. **Repairing `RefIs` itself is the right follow-up, measured separately.**

## Item 4 — `0x82403728`: the briefed re-home is REFUTED; fixed another way, +1 / +160 B

⛔ **Do not retry the briefed action.** All three of its premises fail:

1. `src/system/rndobj/Anim.cpp:661` is **`#include "rndobj/Dir.cpp"`** — Dir.cpp
   is compiled INTO Anim.cpp's TU. This is exactly the CLAUDE.md trap about
   `#include`d files reading as false "pinned but unwired" hits (`rnddx9/Cam.cpp`).
2. `Anim.obj` therefore **does** define `?Export@RndDir@@UAAXPAVDataArray@@_N@Z`
   (read from COFF after a full build; 172 RndDir symbols). The row already
   PAIRS where it is — it was scoring fuzzy 86.625 / mpn 87.375, a **source
   divergence, not a pinning defect**. The pin is CORRECT.
3. `system/rndobj/Dir.cpp` is **not in `objects.json` at all**, so there is no
   `Dir.obj` to receive the row. Moving it would have made the row permanently
   unpairable at 0%. Adding Dir.cpp as its own TU instead would duplicate every
   symbol it defines — the ScatterIncludes/`mtx.cpp` duplicate-definition mode.

Checked and NOT applicable: the DG-2 single-function-unit vanishing trap —
`Anim.cpp` has **18** `.text` blocks, so draining one cannot empty the unit.

**The real defect is the same shape as item 3** — a base-chaining call across a
virtual base. Retail reaches the intermediate base with a fixed
`subi r3, r3, 0x34` and calls `?Export@MsgSource@@UAAXPAVDataArray@@_N@Z`. We
spelled `Hmx::Object::Export`; since `class MsgSource : public virtual
Hmx::Object`, Object is only reachable through the vbtable, so MSVC emitted a
4-instruction virtual-base cast (`lwz -0x1dc(r3)` / `lwz 4(r11)` / `add` /
`subi`) in place of retail's single `subi`, **and called the wrong override** —
`MsgSource` overrides `Export` (`Msg.h:272`), so chaining to `Hmx::Object`
skipped it. Behavioural bug as well as a matching one.

The 6-instruction **r28↔r29 `REGISTER_SWAP` dissolved** when the cause was
fixed, with no register work — another instance of the standing rule that a
register-swap label is a symptom, not a diagnosis.

---

## Follow-up vein (NOT done, recorded for the next lane)

Both items 3 and 4 were the **same defect class**: a base-chaining call that
skips an overriding intermediate base across a virtual-inheritance edge, which
MSVC renders as a vbtable cast retail does not have. `src/` carries ~15 more
`Hmx::Object::Replace` / `Hmx::Object::Export` call sites
(`BandDirector.cpp:403`, `CharBonesMeshes.cpp:34`, `CharWeightable.cpp:23`,
`FlowAnimate.cpp:354`, `FlowSetProperty.cpp:142/454`,
`SkeletonUpdate.cpp:169`, `Msg.cpp:489`, `Task.cpp:60/115/195`, …), and
`movie/TexMovie.cpp:49` already carries a comment saying "the
`Hmx::Object::Replace` fallback, which retail does not have". Each needs
adjudicating on retail bytes individually — **do not sweep it blind**; item 3
shows the correct answer can be "remove the arm" and item 4 shows it can be
"retarget the arm", and only the disassembly distinguishes them.

## Commits on `w16-o`

| commit | item |
|---|---|
| `56c5c376` | 1 — list-level fold alias installed with T1 evidence; element-level refused |
| `9e69fac6` | 2 — ReviewDisplay pin move + map row repair, Δ0 |
| `2da87cea` | 3 — `RndTransformable::Replace` vbase-subobject compare, +124 B |
| `7fa6d7c5` | 4 — `RndDir::Export` chains to `MsgSource`, +160 B |

## NOT done, with reasons

- **Element-level fold alias** (`HamCamShot::Target` `operator<<`) — refused;
  not proven at T1 (72 B / 2 relocs retail vs 316 B / 17 ours). The brief's
  remaining 108 B is this. **An alias not proven at T1 must not be installed.**
- **`RefIs` itself not repaired** — the vbase upcast belongs there, but six
  other call sites make it a separate measured change. Item 3 is fixed locally.
- **`HamCamShot::Target::operator<<` source divergence not closed** — exposed by
  item 1's refusal (12 fields written vs retail's far fewer); out of lane scope.
- **The ~15-site base-chaining vein above** — identified, not adjudicated.
