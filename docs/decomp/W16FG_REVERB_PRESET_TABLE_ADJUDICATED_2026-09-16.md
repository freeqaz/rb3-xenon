# W16-FG — `FxSendReverb360::SyncEffectParams`: 3,280 B crossed, and the oracle survived adjudication (except one immediate)

**Lane:** W16-FG · **Date:** 2026-09-16 · **Branch:** `w16-fg` off main `d03dfea0`
**Target:** `?SyncEffectParams@FxSendReverb360@@UBAXPAUIXAudio2SubmixVoice@@@Z`
@ `0x82B68070`, 3,280 B, unit `default/system/synth_xbox/FxSendReverb`.
**Before:** `fuzzy 0.0000 / mpn 0.0000` — declared in the header, defined nowhere.
**After:** `fuzzy 100.0 / mpn 100.0`. Unit `matched_code` 732 -> 4,012 of 4,760.

## Measured whole-binary A/B (`tools/ab_measure.py --from-dirty`, ruler `name_check`)

```
leg A: matched=44009 masked=23235 honest=20774 code%=40.398440  (settled, 0 recompiles)
leg B: matched=44010 masked=23235 honest=20775 code%=40.430454  (1 recompile, patch_steps=6)
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.032014pp  Δcode_bytes=+3280
unit net (ALL units) = +1   vs whole-binary Δmatched = +1      <- zero collateral
```

### Pre-registration vs measured — 4/4 exact

| measure | predicted | measured |
|---|---|---|
| `Δmatched_functions` | +1 | **+1** |
| `Δmatched_code` | +3,280 B | **+3,280 B** |
| `Δmatched_code_percent` | +0.032014 pp | **+0.032014 pp** |
| `Δmasked_equal_functions` | 0 | **+0** |

All five pre-registered falsifier rows HELD: `?ReverbConvertI3DL2ToNative@@...`
stayed `fuzzy 0` (out of scope, untouched), `?CreateFx@FxSendReverb360@@...`,
`??0FxSendReverb360@@QAA@XZ` and `?SetType@FxSendReverb360@@UAAXVSymbol@@@Z`
stayed 100.0, `fn_82B67B40` stayed 0.

The prediction was easy to make and is reported as such: `matched_code` is
all-or-nothing per row, only one row could cross, and 3,280 / 10,247,068 =
0.0320094%. The value of pre-registering was the *falsifiers*, not the estimate.

## The part that mattered: the oracle was a hypothesis, and one clause of it was false

`../dc3-decomp/src/system/synth_xbox/FxSendReverb.cpp:27` carries a complete body
in a unit DC3 matches at **100.0%** — about as strong as an oracle gets here. But
CLAUDE.md's standing rule is that oracle text is a HYPOTHESIS, and on 2026-09-16
alone five rows were found where our source faithfully reproduced an oracle and
the ORACLE was the defect. So the table was adjudicated against retail bytes.

**Method:** symbolically execute retail `0x82B68070` out of
`build/45410914/asm/system/synth_xbox/FxSendReverb.s`, tracking GPR immediates,
FPR loads from `.rdata` pools **and FPR spills through the stack frame**, then
read the resulting `stw`/`stfs` stores into the table base `r30` (`lbl_82E121D0`).
String literals were resolved by parsing `band.exe`'s PE section table in Python
and reading `.rdata` — never `grep`, which is binary-blind in this shell and
yields false negatives shaped like decisive ones.

### What retail CONFIRMED (the oracle held)

| claim | retail evidence |
|---|---|
| entry count is **30** | the search compares a byte cursor: `cmplwi cr6, r9, 0x690`, and `0x690 = 30 * 0x38`. Last `Symbol` ctor targets slot `0x658` = entry 29. objdiff independently counts `??0Symbol@@QAA@PBD@Z` **30** times in the target. |
| **0x38 stride**, `Symbol` at +0x00, params at +0x04..+0x37 | ctor targets `r30+0x00, +0x38, +0x70, …`; loop advances `addi r10, r10, 0x38` |
| all 30 **names and their ORDER** | the `.rdata` literal each ctor call passes, read out of `band.exe` |
| all **390** constants (30 x 13) | every `stw`/`stfs` into the table decoded and compared at float32 |
| function-local static behind a **guard** | `lwz r11, lbl_82E12860`; `clrlwi. r9, r11, 31`; `bne` past the whole init; `ori r11, r11, 1` — the bit-0 `??_B`/`$S` static-init guard shape |
| no `MILO_FAIL` branch | the loop falls straight through to the index computation — consistent with the oracle, because `MILO_FAIL` is `((void)(__VA_ARGS__))` in the match build (`os/Debug.h:194`, `HX_NATIVE` undefined), so `if (idx == 30) MILO_FAIL(...)` is dead and emits nothing |
| `mEnvironmentPreset` at `this-0x20` | `lwz r8, -0x20(r29)`. `FxSendReverb::mEnvironmentPreset` is at +0x54 and the `FxSend360` base at +0x74, so the existing layout already produces this — **no layout change was needed** |

**390 of 390 values matched, 30 of 30 names matched, order identical.**

⚠ Three of those constants are invisible to a naive scan: retail runs out of
nonvolatile FPRs and spills `hallway.DecayHFRatio`, `mountains.ReflectionsDelay`
and `medium_hall.ReflectionsDelay` to stack slots `r31+0x50/0x54/0x58`. A decoder
that does not model the spill reports them `UNRESOLVED` — so the check would have
silently covered 387/390 while *looking* complete. The first version of the
decoder did exactly that; modelling `stfs`/`lfs` against `r31` resolved all three,
each to precisely the oracle's value.

Independent corroboration from RB3's own data: the `mEnvironmentPreset` doc
comment in `src/system/synth/FxSendReverb.h:29` lists the same 30 option names in
the same order.

### What retail REFUTED — and DC3's own binary proves it is a version delta, not a DC3 bug

**`sizeof(XAUDIO2FX_REVERB_PARAMETERS)` is `0x34` in RB3, not DC3's `0x38`.**
DC3's header declares a trailing `UINT32 WetDryMixPct` at 0x34. The two retail
binaries disagree at the *identical* instruction slot of the same eight-
instruction call sequence, where the source expression is `sizeof(native)`:

```
RB3 0x82B68D14   38 C0 00 34   li r6, 0x34
DC3 0x82E32334   38 C0 00 38   li r6, 0x38    (dc3 build/373307D9/asm/.../FxSendReverb.s:1399)
```

Both sequences read `add r3,r10,r11; bl ReverbConvertI3DL2ToNative; lwz r11,0(r28);
li r7,0; li r6,<SIZE>; addi r5,r31,0x60; li r4,0; lwz r11,0x18(r11); bctrl`, and
DC3's unit is 100.0% matched — so `0x38` is verified correct **for DC3** and
`0x34` verified correct **for RB3**. `WetDryMixPct` is an XDK revision that
postdates RB3. Because `matched_code` is all-or-nothing per row, carrying DC3's
field would have charged that one immediate and cost the entire 3,280 B.

★ **The durable point:** the refutation lives in the **binary**, not in either
source tree, and it took *both* binaries to state it as a version delta rather
than as "DC3 has a bug". A one-binary check would have shown a mismatch without
explaining it — and the natural (wrong) reaction to an unexplained mismatch is to
distrust your own port.

## What I did NOT do, and why

- **`?ReverbConvertI3DL2ToNative@@YAXPBU…@Z`** (740 B, fuzzy 0, same unit) is left
  at 0 **deliberately**. It is Microsoft XAudio2 vendor implementation and XDK
  porting is out of scope by standing user directive. It is declared `extern` in
  the new header and nothing more. It sits next door to a 3,280 B win and is the
  obvious next thing to reach for — it is out of scope anyway.
- **No `splits.txt` / `symbols.txt` / alias-file edits.** None were needed: the
  address was already in `scripts/target_symbol_map.json` and the row was already
  pairing (it read 0 because we had no body, not because it was unpaired).
- **Did NOT add DC3's `Recreate` / `UpdateMix` / `OnParametersChanged` /
  `IsStandard` virtuals** to `FxSendReverb360`, though DC3's header declares them.
  Our header's vtable shape is already producing 10 matched rows in this unit,
  including `??_GFxSendReverb360@@UAAPAXI@Z` and `?SetType@...`; adding virtuals
  would reorder the vtable and risk them for no measured gain. Out of this lane's
  scope — worth a separate look only if a future row demands it.
- **Did NOT run the permuter.** It was never needed; the row matched on the first
  build.
- **Did not verify `XAUDIO2FX_REVERB_PARAMETERS`'s internal field layout.** Only
  its *size* is observable from RB3's `.text` (it is only ever passed by pointer
  to a vendor function and to `SetEffectParameters`). The field names/offsets
  below 0x34 are inherited from DC3 and are **unverified against RB3**; they are
  codegen-irrelevant here, but do not treat them as retail-confirmed.

## Reusable lessons

1. **A spill-blind asm decoder under-verifies while looking complete.** Model
   `stfs`/`lfs` against the frame pointer, or report the unresolved count loudly.
2. **`ab_measure` deletes untracked files created DURING the run** — it warns, but
   this write-up was lost to it once and had to be rewritten. Write run-time
   deliverables to `~/tmp` and copy them in after.
3. **`ab_measure` refuses untracked files in build-relevant dirs** (they can change
   build output invisibly). For a change that adds a new header, commit the header
   first — it is inert until something includes it — and keep the `.cpp` dirty as
   the measured patch.
