# Three dc3-sourced leads, adjudicated against RB3 retail bytes

**Lane W13-B, 2026-09-13.** Branch `w13-float-and-argorder` off `49b5a79f`.

Three hypotheses arrived from a dc3-decomp session. All three were claims about
*our* binary that were discovered in *dc3's*. dc3 had already produced one false
lead the same day by generalising from its own target (`Rnd::Terminate`, ported
verbatim, measured **-180 B**), so each was re-adjudicated here against the
retail image `orig/45410914/band.exe` and nothing else.

**Verdicts: 1 CONFIRMED · 1 REFUTED FOR RB3 · 1 CONFIRMED-BUT-BACKWARDS.**
The third lead was real, but the source comment asserting what retail does was
wrong *in the opposite direction* — the annotation would have sent the next
reader the wrong way, which is worse than no annotation.

Whole-binary A/B: **Δ0, as pre-registered.** See "Measurement" below.

---

## The instrument

`~/tmp/w13b/probe.py`. It parses `band.exe`'s PE section table, maps VA to file
offset, and offers `rd(va,n)` / `f32(va)` (big-endian reads), `dis(lo,hi)`
(capstone `CS_ARCH_PPC | CS_MODE_32 | CS_MODE_BIG_ENDIAN`), and
`find_be_float(v)` — `struct.pack('>f', v)` located in a **named section**,
4-byte aligned, so a hit is a real constant-pool entry rather than a coincidence
in code or `.pdata`.

Retail section map, for anyone reproducing this:

| section | VA | size | raw ptr |
|---|---|---|---|
| `.rdata` | `0x82000400` | `0x1f1184` | `0x400` |
| `.pdata` | `0x821f1600` | `0x70c28` | `0x1f1600` |
| `.text`  | `0x82270000` | `0x9dce3c` | `0x264e00` |

Two properties were exercised rather than asserted:

- **Discrimination.** Constants the target TU is *known* to use were located
  first — `255.0f` (3 hits), `1/255f` (7), `3.0f` (19) — and a junk pattern
  `0x7f7f7f7e` returns **0**. The probe can both find and fail to find.
- **A positive control in the same run** for every negative verdict. This is
  the whole load-bearing part of the Lead 2 refutation.

⚠ An early version of the literal extractor crashed on `0.0f`
(`float('0.0f')` raises). Worth recording because the *silent* version of that
bug — a regex that quietly drops literals instead of raising — returns a clean,
decisive, entirely fake negative. That is one's own regex, not the documented
`grep` shim, and the two are indistinguishable from the output.

---

## Lead 1 — TexBlendController easing curve: **CONFIRMED**

`src/system/rndobj/TexBlendController.cpp:144` read

```cpp
blend = t2 * 2.0f + t3 * 3.0f;      // 2t^2 + 3t^3
```

Retail `?GetBlendState@RndTexBlendController@@QBA?AW4BlendState@1@AAMM@Z` is at
`0x82489058`. Its easing block:

```
82489150  lfs    f12, 0(r30)        ; blend
82489154  lis    r11, -0x7dff       ; r11 = 0x82010000
82489158  fmuls  f11, f12, f12      ; t2 = blend*blend
8248915C  lis    r10, -0x7dfe       ; r10 = 0x82020000
82489160  lfs    f0,  0x4844(r11)   ; A = [0x82014844]
82489164  lfs    f13, -0x150(r10)   ; B = [0x8201FEB0]
82489168  fmuls  f12, f12, f11      ; t3 = blend*t2
8248916C  fmuls  f0,  f11, f0       ; t2*A
82489170  fmadds f0,  f12, f13, f0  ; t3*B + t2*A
82489174  stfs   f0, 0(r30)
```

Decoded from retail `.rdata`:

| constant | VA | bytes | value |
|---|---|---|---|
| A (multiplies `t2`) | `0x82014844` | `40400000` | **3.0** |
| B (multiplies `t3`) | `0x8201feb0` | `c0000000` | **-2.0** |

★ Cross-check *inside the same run*: the discrimination control, executed before
the disassembly and with no knowledge of it, listed `0x82014844` as the **first**
`.rdata` hit for `3.0f`. The address the instruction stream points at and the
address the value search returns independently are the same one.

⇒ retail computes **`3t^2 - 2t^3`** — textbook smoothstep, exactly as dc3 has
it. Our source was wrong in **both** coefficients **and** the sign of the second.

Fixed to `t2 * 3.0f + t3 * -2.0f`. That spelling is deliberate over the
equivalent `t2 * 3.0f - t3 * 2.0f`, so the compiler emits retail's `fmadds`
against a negative `.rdata` constant rather than an `fmsubs`/`fnmsubs` form.

★ The instruction **shape is unchanged** by the fix — same six instructions,
only the two relocation targets differ. **No diff and no match-% could ever
have surfaced this.** That is precisely why the byte probe was the required
instrument and not a convenience.

---

## Lead 2 — StubCameraInput float literals: **REFUTED FOR RB3**

The claim: our `joint[1]` differs from dc3's in the fifth digit.

The claim is **true about the two source trees**:

| | ours | dc3 |
|---|---|---|
| `joint[1].x` | `0.127627f` | `0.127672f` |
| `joint[1].y` | `0.178946f` | `0.178935f` |

— a digit transposition in each (`127627`/`127672`, `178946`/`178935`).

**But it is not adjudicable against RB3, because RB3 retail contains none of
this data.** Every float literal in the TU was probed against the whole
14,363,648-byte image, big-endian, with the literals *shared by both trees* as
in-pass positive controls:

| population | present in retail |
|---|---|
| literals **shared** by both trees (the control) | **4 of 66** |
| the 4 that hit | `0.0`, `0.5`, `1.0`, `2.3` — trivially common |
| the 62 distinctive joint constants | **0** |
| our two suspects (`0.127627`, `0.178946`) | **0** |
| dc3's two (`0.127672`, `0.178935`) | **0** |

⛔ **The control FAILS, so the negative on the two suspects carries no
independent information — and that is the finding, not a caveat.**
`StubSkeletonData`'s body is not in RB3 retail at any coefficient. RB3 is not a
Kinect title; this TU is a DC3 import.

Three reasons the zeros are not an instrument error:

1. The search covered **all 14 MB**, every section, not just `.rdata` — so a
   wrong-section mistake cannot explain them.
2. The same run returned 86,563 aligned `.rdata` hits for `0.0` and exactly
   **1** for `2.3`, so the search is live at both ends of the density range.
3. The retail span pinned to `StubCameraInput.cpp`
   (`.text 0x82364028-0x823641EC`, 452 bytes, 2 `.pdata` entries) disassembles
   to a constructor plus its EH funclet containing **zero `lfs` instructions of
   any kind**. There is no float data there to be right or wrong about.

**Action taken, and its exact standing.** `joint[1]` was set to dc3's values.
That is grounded in **DC3's bytes, not RB3's** — dc3's lane confirmed them in
dc3's `.rdata`, and RB3 offers no evidence either way. It is a transcription
repair to an imported file, metric-inert by construction (the unit is unpaired:
2 functions, `matched` absent). **It must not be cited as an RB3 adjudication,
and nobody should re-hunt this lead against RB3.**

---

## Lead 3 — UsbMidiGuitar argument order: **CONFIRMED, AND THE COMMENT WAS BACKWARDS**

Five sites in `UsbMidiGuitar::Poll` each carried

```cpp
RGPitchBendMsg pbMsg(i, pitchBend);   // retail arg order: (pad, value)
```

⛔ **That comment is refuted by retail bytes. Retail's order is `(value, pad)`.**

Retail `?Poll@UsbMidiGuitar@@SAXXZ` (`0x8251a340`) constructs **ten** messages.
`r26` is the pad index: set `li r26, 0` at `0x8251A364`, incremented per pad,
and passed as the sole argument to
`?JoypadGetPadData@@YAPAVJoypadData@@H@Z`. In **all ten** constructions `r26`
is the **last** argument register:

| retail site | ctor | argument registers | pad position |
|---|---|---|---|
| `0x8251a4dc` | `StringStrummedMsg(H,H,H,H)` | r4=r28, r5, r6, **r7=r26** | last of 4 |
| `0x8251a5ac` | `RGFretButtonDownMsg(H,H,_N)` | r4=r30, **r5=r26**, r6=r29 | 2nd of 3 |
| `0x8251a5d0` | `RGFretButtonUpMsg(H,H,_N)` | r4=r30, **r5=r26**, r6=r29 | 2nd of 3 |
| `0x8251a60c` | *(10th 2-arg msg — see map note)* | r4=r22, **r5=r26** | last |
| `0x8251a688` | `RGAccelerometerMsg(H,H,H,H)` | r4,r5,r6=axes, **r7=r26** | last of 4 |
| `0x8251a6c8` | `RGPitchBendMsg(H,H)` | r4=`clrlwi(0xd(r27),0x19)`, **r5=r26** | last |
| `0x8251a704` | `RGMutingMsg(H,H)` | r4=`clrlwi(0xd(r27),0x19)`, **r5=r26** | last |
| `0x8251a744` | `RGProgramChangeMsg(H,H)` | r4=`clrlwi(0xe(r27),0x19)`, **r5=r26** | last |
| `0x8251a784` | `RGStompBoxMsg(_N,H)` | r4=`srwi(0xd(r27),7)`, **r5=r26** | last |
| `0x8251a7dc` | `RGSwingMsg(H,H)` | r4=computed swing, **r5=r26** | last |

In every 2-arg case the value is computed **directly into r4** by the
instruction immediately preceding the call, and `mr r5, r26` supplies the pad.
There is no ambiguity to resolve.

Our source already obeyed the rule at 5 of the 10 sites (StringStrummed,
FretButtonDown, FretButtonUp, Swing, Accelerometer) and violated it at exactly
the 5 the lead named. All five were fixed to `(value, pad)`, and the five
comments corrected.

**Three independent confirmations, so this does not rest on the call sites
alone:**

1. **Our own header already said so.** `RGStompBoxMsg::GetPadNum()` returns
   `mData->Int(3)` — the *second* ctor argument. Across the family
   `GetPadNum()` is always the last slot (`Int(5)` for the 4-arg messages).
2. **Our own mangled signature already said so.** Retail's ctor is
   `??0RGStompBoxMsg@@QAA@_NH@Z` = `(bool, int)`, and our header declares
   `RGStompBoxMsg(bool, int)`. The call site was passing `(int i, bool stomp)`
   into it, narrowing the pad index to `bool` and widening the flag to `int`.
   **That site was type-inverted against our own declaration**, independent of
   any retail evidence at all.
3. **The ctor bodies are positionally identical.** `0x8251cbe0`, `0x8252ee20`
   and `0x8252f3e8` are instruction-for-instruction the same modulo the
   `Type()` callee: `stw r4, 0x58(r31)` (DataNode slot 2), `stw r5, 0x60(r31)`
   (slot 3). Role assignment is the same in all of them, so a role proven at
   one call site transfers to a ctor with no live call site — which is what
   settles `RGConnectedAccessoriesMsg`, whose only caller in the entire binary
   is outside `Poll`, at `0x8251dd08`.

---

## Measurement

Pre-registered before any build (`~/tmp/w13b/PREREGISTER.md`): Δ0 for leads 1
and 2; Δ ≥ 0 for lead 3, with an explicit commitment **not** to revert on Δ0
because retail bytes are the authority. Predicted whole-binary Δmatched in
[0, +1], Δcode in [0, +1244] B.

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w13-b --from-dirty`:

```
  leg A: matched=42839 masked=22997 honest=19842 code%=37.982048  (recompiles: 0, settled)
  leg B: matched=42839 masked=22997 honest=19842 code%=37.982048  (recompiles: 3, split=0, patch_steps=6)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000076pp   (legA 49.153210 -> legB 49.153286)
  units at 100% [mpn ruler]:             legA 164 -> legB 164  (0 reached 100, 0 fell off)
  units at 100% [all-rows-fuzzy ruler]:  legA 136 -> legB 136  (0 reached 100, 0 fell off)
```

**Δ0 on every headline measure, 0 units fell off 100%, and the run is not
absent-vs-absent** — leg B recompiled 3 TUs, satisfying the tool's application
assertion, so the change was genuinely built and measured.

★ The **Δfuzzy of +0.000076pp** is the useful number here. It is not noise and
not nothing: it proves the patch is **non-inert** — real code moved — while
crossing no row's all-or-nothing `fuzzy == 100` threshold. A strictly-zero
Δfuzzy would have been the signature of an edit that never reached the
compiler.

Baseline rows, for whoever picks these up next:

| row | size | mpn | fuzzy |
|---|---|---|---|
| `?GetBlendState@RndTexBlendController@@...` | 408 | 94.460785 | 94.411766 |
| `?Poll@UsbMidiGuitar@@SAXXZ` | 1244 | 99.077170 | 98.562700 |

Both are unchanged by this lane. `Poll` at mpn 99.08 with five genuinely
inverted argument pairs is itself the standing lesson restated: **a wrong
argument order is a register-argument difference, and the metric is nearly
blind to it.** These fixes were landed on merit, exactly as the brief directed.

---

## Per-row attribution — where the Δ0 actually came from

The whole-binary Δ0 hides two real, opposite movements. Read this table, not the
headline:

| row | mpn before → after | fuzzy before → after |
|---|---|---|
| `?Poll@UsbMidiGuitar@@SAXXZ` | 99.077170 → **99.913185** | 98.562700 → **99.816720** |
| `?GetBlendState@RndTexBlendController@@...` | 94.460785 → **93.872550** | 94.411766 → **93.774510** |

**The argument-order fix worked and is large** (+0.84 mpn / +1.25 fuzzy on a
1244 B row). It bought **zero bytes** only because `matched_code` is
all-or-nothing per row at `fuzzy == 100`, and the row stops just short.

**The smoothstep fix costs 0.637 pp of fuzzy on its row, and it is kept
anyway.** The constants are not in doubt — they were read out of retail
`.rdata`. What differs is the instruction FORM the compiler chooses:

```
target (retail):  lfs f13, lbl_8201FEB0    (= -2.0)
                  fmuls  f0, f11, f0       ; t2*3.0
                  fmadds f0, f12, f13, f0  ; t3*(-2.0) + that

base (ours):      lfs f13, __real@40400000 (= 3.0)
                  fmuls  f0, f12, f0       ; t3*2.0
                  fmsubs f0, f11, f13, f0  ; t2*3.0 - that
```

Both compute `3t^2 - 2t^3`. MSVC folded the negation into `fmsubs` against a
**positive** 2.0 instead of keeping `-2.0` in `.rdata`. The old, arithmetically
*wrong* `2t^2 + 3t^3` happened to produce retail's `fmadds` shape, which is why
being right scores slightly worse here. Per the standing directive that accuracy
beats the headline percentage, the correct constants stay.

### ⚠ A pre-registered prediction that FAILED

dc3 spells the same expression `blend = t3 * (-2.0f) + t2 * 3.0f` — negated term
first. Predicted: that operand order would make `t2*3.0f` the addend and restore
retail's `fmadds`, lifting the row back above 94.411766.

**Measured: exactly identical.** `mpn 93.87255 / fuzzy 93.77451` on *both* legs
of a second A/B, Δfuzzy `+0.000000pp` whole-binary.

⇒ **operand order in the source is INERT here** — MSVC canonicalises both
spellings to the same `fmsubs` form under `/fp:fast`. Whatever produces retail's
`fmadds`, it is not the order the terms are written in. Do not re-try this;
the remaining gap is a codegen-form residual, which is permuter-class work and
OFF by directive.

dc3's spelling was **kept** despite measuring identical, purely to reduce
divergence from the engine oracle on a shared file. That is a tie-break on
provenance, explicitly not on the metric.

⚠ Note the whole-binary Δfuzzy of `+0.000000pp` for round 2 does **not** by
itself show the edit was inert: a 408 B row moving 2 pp is worth ~0.0000008 pp
of a 10.3 MB denominator, an order of magnitude below the six-decimal print. The
inertness was established from the **per-row** figures in the archived leg
reports, not from the headline. Round 1's `+0.000076pp` came almost entirely
from `Poll`.

---

## ⛔ Tooling defect found: `ab_measure` SILENTLY DELETES untracked files created during the run

**This lane lost its own deliverable to it, and the log said the restore was
verified.** Reporting rather than fixing — see "did NOT do".

Writing `docs/decomp/FLOAT_AND_ARGORDER_2026-09-13.md` while an A/B was in
flight (a natural thing to do: the build is minutes long and the write-up is
independent of it) resulted in the file being deleted. The console said only:

```
  [tree] restored to the pre-run state (3 path(s), verified by re-reading the diff)
```

The three paths are the *tracked* files it checked out. The deletion is not
mentioned. `result.json` records it:

```json
"tree_restore": { "ok": true, "action": "restored", "verified": true,
  "checked_out": ["src/system/gesture/StubCameraInput.cpp",
                  "src/system/os/UsbMidiGuitar.cpp",
                  "src/system/rndobj/TexBlendController.cpp"],
  "removed": ["docs/decomp/FLOAT_AND_ARGORDER_2026-09-13.md"] }
```

**Mechanism** (`tools/ab_measure.py`): `_untracked()` runs
`git status --porcelain` over the **whole worktree** — not restricted to
build-relevant directories — and `plan_restore()` computes
`added = cur_untracked - pre_untracked`, which the cleanup path `unlink()`s.
The intent is right and the behaviour is deliberate: hand the tree back exactly
as found, which is the fix the tool's own 12-cell matrix exists to guarantee.

**Two things make it a hazard rather than a feature:**

1. ⛔ **The removal is invisible on the console.** Only the checkout count is
   printed. A lane that does not open `result.json` never learns a file was
   deleted.
2. ⛔ **`verified: true` is a ONE-SIDED verification.** The check is
   `now == self.pre_diff` — it re-reads the **tracked** diff only. Deleting an
   untracked file cannot make that predicate fail, so the tool truthfully
   reports "verified" about the half of the state it looked at, and silently
   about the half it changed. This is the same shape as the one-sided
   instrument error recorded in CLAUDE.md: *a check that cannot fail in the
   direction the damage occurs.*

This is exactly the class CLAUDE.md already flags for `prune_worktrees.py`
("untracked files are frequently a lane's **entire** deliverable"). The A/B
tool has the same exposure and no equivalent warning.

**Suggested fix (not applied here):** print `removed` on the console whenever
it is non-empty, and fold untracked-set equality into the `verified` predicate
so the tool stops vouching for state it did not examine. Both are small and
belong to whoever owns the shared tool.

**Workaround for lanes today:** write deliverables outside the worktree
(`~/tmp/...`) while an A/B is running and copy them in afterwards, or write
them before the run starts.

---

## Side findings — flagged, deliberately NOT acted on

**A likely wrong map name.** The 10th ctor called from `Poll` is `0x8252f3e8`,
which `scripts/target_symbol_map.json` names `??0SigninChangedMsg@@QAA@KK@Z` —
but its body is the *exact* 2-arg RG-message ctor shape, it is called from the
connected-accessories position in `Poll`, and its address is precisely the
`.text` **end** boundary of `UsbMidiGuitarMsgs.cpp` in `splits.txt`
(`start:0x8252E530 end:0x8252F3E8`). Relatedly, `RGPitchBendMsg`'s ctor at
`0x8252ee20` calls `0x8252e6b0`, which the map names
`?Type@RGConnectedAccessoriesMsg@@SA?AVSymbol@@XZ`. At least one of these names
is shuffled. Per the standing rule that *proving a name wrong is not the same
as showing a rename is safe*, **no map edit was made.**

**The `StubCameraInput.cpp` pin may be a mis-attribution.** The pinned span is a
constructor that calls `CameraInput`'s ctor at `0x82369568` and contains none of
the TU's float data. That is a splits question, not this lane's.

**Retail's `Poll` reads byte `0xd(r27)` masked with `& 0x7f` for *two* different
messages** (pitch-bend and muting, stored at `+0x11c` and `+0x12c`). Our source
mirrors the oddity — `connAcc` and `pitchBend` are both read from
`proData->mPitchBend`. Left exactly as-is: it reproduces retail.

---

## What this lane deliberately did NOT do

- **Did not fix `tools/ab_measure.py`**, despite finding a silent data-loss
  path in it. It is a shared tool with its own selftest, used by every
  concurrent lane; changing it mid-wave on a lane's own initiative trades a
  documented hazard for an undocumented one. Reported with a concrete fix
  instead.
- **Did not touch `scripts/target_symbol_map.json` or `splits.txt`**, despite
  evidence of a wrong name and a suspect pin.
- **Did not remove or reorder any message block** in `Poll`; only argument
  order changed. (Retail's `Poll` does construct ten messages and our source
  has ten, so there is no extra block once the `0x8252f3e8` naming is
  understood.)
- **Did not run `float_oracle2.py`.** The coordinator offered it and its method
  is sound, but it adjudicates against **target objects**, whereas this probe
  reads the **retail image** directly — one step closer to ground truth — and
  it already carried a known-answer fixture plus the in-pass positive control
  that tool's author identified as the load-bearing part. Two independent
  instruments would be better than one; a second instrument answering a
  *weaker* question was judged not worth the adaptation cost (hardcoded DC3
  title ID, object layout and source roots, plus four transient tmpfs
  dependencies).
- **Did not generalise the probe** into a tree-wide float-divergence sweep.
  `~/tmp/preserved-instruments-20260913/audit2_rb3-xenon.json` is a
  pre-computed cross-repo analysis of our tree and is the obvious seed for that
  wave; this lane stayed inside its three leads.
