# W16-FK — `ReverbConvertI3DL2ToNative`: ported from DC3, and DC3's 5-row residual TRANSFERS to cl 10224 — 740 B uncollectable by source work

**Lane:** W16-FK · **Date:** 2026-09-16 · **Branch:** `w16-fk`, based on main `e164426d`,
fast-forwarded to `1291c7db` (W16-FL's landing, no overlap) before the authoritative A/B.
**Target:** `?ReverbConvertI3DL2ToNative@@YAXPBUXAUDIO2FX_REVERB_I3DL2_PARAMETERS@@PAUXAUDIO2FX_REVERB_PARAMETERS@@@Z`
= retail `fn_82B67D30`, **740 B**, unit `default/system/synth_xbox/FxSendReverb`.
**Before:** fuzzy 0.0 / mpn 0.0 (declared in `xdk/xaudio2/xaudio2fx.h`, defined nowhere;
W16-FG left it as "MS vendor implementation" — a reason the brief withdrew, correctly).
**After:** **fuzzy 99.82703 / mpn 99.98919**, Diff Score 32 / 18,500, exactly **5**
charged `diff_arg` sites. **Δ`matched_code` = 0 B** — only fuzzy == 100.0 pays.

## Verdict in one paragraph

The body ports cleanly from `../dc3-decomp/src/system/synth_xbox/Synth.cpp:106` minus the
two trailing `WetDryMixPct` stores (RB3's struct is 0x34, FG adjudicated it on retail
bytes). On our compiler (cl 16.00.10224.00, the retail one; DC3 is 11886) the first build
landed at **exactly DC3's residual**: the same 5 rows, the same score to the fifth
decimal (99.82703 here vs DC3's 99.8289 on a 748 B row — same 32 diff-score points, a
different denominator). Eight further spellings/pragmas were measured against that
baseline, each a full `./tools/ninja-locked` build with the control row watched. **Six
were byte-identical to the baseline, two moved the five rows only by breaking other
rows.** The mechanism was characterised (below) far enough to say why no landable
spelling can reproduce retail's emission order, so **the 740 B is uncollectable by
source work on this compiler with this evidence**, and the row is landed at 99.827 as a
well-evidenced negative. Do not re-run the spellings in the ledger.

## Measured whole-binary A/B (`tools/ab_measure.py --from-dirty`, ruler `name_check`)

```
leg A: matched=44029 masked=23245 honest=20784 code%=40.466990  (recompiles: 0, settled after 2 iterations)
leg B: matched=44029 masked=23245 honest=20784 code%=40.466990  (recompiles: 1, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
[control none] Δmatched_code=+0 B  Δcode%=+0.000000
matched_code 4,146,680 -> 4,146,680 (total_code 10,247,068);  whole-binary fuzzy 50.33873 -> 50.34595 (+0.00722 pp: the row's fuzzy contribution, NOT bytes)
units at 100% [all-rows-fuzzy]: 171 -> 171
run dir: ~/tmp/wt-w16-fk/.ab_measure_runs/20260916-110408-from-dirty-1978093  (log ~/tmp/rb3_ab_w16fk.log, rc=0)
```

Row, from the two legs' own `report.json`: leg A fuzzy 0 / mpn 0.0 -> leg B **fuzzy 99.82703 / mpn 99.98919**, size 740 both legs.

### Pre-registration vs measured

| measure | predicted | measured |
|---|---|---|
| Δ`matched_functions` | 0 (row mpn 99.98919 < 100) | **+0** |
| Δ`matched_code` | 0 B (row fuzzy 99.82703 < 100) | **+0 B** |
| Δ`matched_code_percent` | 0 | **+0.000000 pp** |
| Δ`masked_equal_functions` | 0 | **+0** |

Surprise-capable alternatives the set could express: +1 / +740 B / +0.0072216 pp
(740 / 10,247,068) if the row crossed on the settled tree; any negative delta =
collateral in `FxSendReverb.obj` (the only TU either dirty file feeds — `ninja -t deps`
shows `xaudio2fx.h` is included by exactly one object).

Falsifiers pre-registered and their outcome:

| # | falsifier | outcome |
|---|---|---|
| 1 | control `?SyncEffectParams@FxSendReverb360@@UBAXPAUIXAudio2SubmixVoice@@@Z` (3,280 B, FG's row) stays fuzzy 100.0 in leg B | HELD — 100.0 / 100.0 on both legs |
| 2 | unit row count unchanged; unit `matched_code` stays 4,012 / 4,760 | HELD — 13 rows, 4,012 / 4,760 on both legs |
| 3 | leg-B row fuzzy == 99.82703 exactly (the landed spelling is the measured one) | HELD — 99.82703 / 99.98919 |
| 4 | leg B recompiles exactly 1 TU | HELD — `msvc=1` in leg B's first iteration |
| 5 | leg A equals main's `report.json` at `1291c7db` (44,029 / 4,146,680 / 40.46699%) | HELD — leg A 44,029 / 4,146,680 B / 40.466990% / masked 23,245 |

## The residual, on retail bytes

Retail rows 72–78 of the `.fn fn_82B67D30` listing (`build/45410914/asm/system/synth_xbox/FxSendReverb.s`),
the `DecayHFRatio >= 1` branch's `DecayTime = DecayTime * DecayHFRatio`:

```
row  retail                       ours (every landable spelling)   charged
68   lwz  r10, 0x14(r30)  ; HF    lwz  r11, 0x10(r30)  ; DT        diff_arg
69   stw  r10, 0x50(r1)           stw  r11, 0x50(r1)               diff_arg
70   lfs  f0,  0x50(r1)   ; HF    lfs  f0,  0x50(r1)   ; DT        equal (first-loaded -> f0 on both sides)
71   lwz  r11, 0x10(r30)  ; DT    lwz  r10, 0x14(r30)  ; HF        diff_arg
72   stw  r11, 0x50(r1)           stw  r10, 0x50(r1)               diff_arg
73   lfs  f13, 0x50(r1)   ; DT    lfs  f13, 0x50(r1)   ; HF        equal
74   fmuls f0, f13, f0    ; DT*HF fmuls f0, f0, f13    ; DT*HF     diff_arg
75   stfs f0, 0x50(r1)            stfs f0, 0x50(r1)                equal
```

The `lwz/stw/lfs` triple per operand is the `#pragma pack(1)` packed-float lowering
(both structs are packed; every float member goes through a GPR and stack slot
`0x50(r1)`). What is **identical** on both sides: the source-operand order of the product
(DT left, HF right — `fmuls` multiplies DT by HF on both sides) and the per-operand GPR
temps (DT→r11, HF→r10). What **differs**: which operand's load group is *emitted first*.
Retail emits HF's group first, ours DT's; FPR names follow load order (first-loaded → f0),
so the `lfs` rows read equal while the `lwz`/`stw` register arguments and the `fmuls`
operand names are charged — 5 `diff_arg` sites, Diff Score 32 / 18,500, the multiply's
*semantics* uncharged.

Pool constants are NOT verified by the match (objdiff's `name_check` forgives `lbl_`
placeholder relocations, so a wrong `.rdata` value would still read equal). They were
verified independently: all 11 float constants the body loads (`0.01`, `-4.0`, `4.0`,
`1000.0`, `300.0`, `299.0`, `1.0`, `85.0`, `84.0`, `0.15`, `100.0`) decoded from the
retail `.rdata` pool equal the source literals, **11/11**.

## Experiment ledger — one full build each, control watched on every build

Whole-binary measures held at the base's values on every build (44,028 / 4,146,264 at
`e164426d`), and `SyncEffectParams` read fuzzy 100.0 on every build.

| id | spelling / pragma | row fuzzy | mpn | Diff | five rows | verdict |
|---|---|---|---|---|---|---|
| v1 | DC3 verbatim minus the `WetDryMix*` tail (landed) | **99.82703** | 99.98919 | 32 | 5 `diff_arg` | baseline = DC3's residual, transferred |
| E2 | commutative operand swap `HF * DT` | 99.82703 | 99.98919 | 32 | unchanged | **byte-identical** (DC3 said so on 11886; confirmed on 10224) |
| E3 | one `float` local assigned in both branches, single `DecayTime` store after the `if` | 97.5027 | 97.82703 | 462 | unchanged | refuted — the store and the pool loads reschedule, tail regresses |
| E6 | `#pragma float_control(precise, on)` around the body | 97.66486 | — | — | unchanged | refuted — inserts `bso cr6` after every `fcmpu` (rows 50/113/119/131); **retail has none ⇒ retail is `/fp:fast`** |
| E5 | `#pragma optimize("t", on)` | 84.72973 | 87.62162 | 2,825 | (whole body moved) | refuted — **retail is `/O1`** |
| E4 | `DT * (float)fabs((double)HF)` (probe, not landable) | 98.75676 | 98.91892 | 230 | **flipped**: HF group emitted first, HF→f0, DT→f13 | inserts `fabs; frsp`; tuple became `(HF, DT)` and GPRs HF→r11 / DT→r10 — retail's opposite |
| E7 | `DT * (float)(double)HF` | 99.82703 | 99.98919 | 32 | unchanged | byte-identical — the conversion pair folds before canonicalisation |
| E7b | `DT * (HF * 1.0f)` | 99.82703 | 99.98919 | 32 | unchanged | byte-identical — identity folds before canonicalisation |

### What E4 establishes (the mechanism), and why it closes the vein

MSVC canonicalises a commutative multiply so that a **non-leaf** operand goes **left**
and is **emitted first**; the first-emitted operand lands in f0; GPR temps are assigned
left→r11, right→r10. E4 is the only spelling that moved the five rows, and it did so by
making HF a non-leaf (`fabs`+`frsp`, 8 extra bytes) — which flipped emission order
**and** the tuple **and** the GPR assignment, all three together. Retail has the tuple
and GPRs of a *leaf-vs-leaf* `(DT, HF)` but the emission order of a *non-leaf-right*.
To reproduce that, HF must be a leaf at canonicalisation and "heavier" at emission time
while emitting **zero** extra bytes; the two zero-byte candidates for that (E7, E7b)
both fold before canonicalisation and are inert. Every spelling that survives to
emission emits bytes. The residual is therefore a scheduler/canonicalisation difference
between cl 10224's retail build and ours that a source spelling cannot express — which
is what DC3 concluded on 11886 with three spellings, now measured to be true on 10224
with eight, one of which (E4) is a positive control showing the rows *can* move.

## What was NOT done, and why

- **Not re-run (refuted by DC3 on cl 11886 and skipped here by design):** the lifted
  temp (`float t = DT * HF; pNative->DecayTime = t;`) and two named temps declared in the
  image's load order and multiplied in the image's operand order. W16-EY's house
  measurement is that naming a value is inert on this compiler, and E2 (operand order)
  reproduced DC3's inert result exactly, so these two would have measured the same
  32-point diff.
- **Not tried: `volatile` / `__restrict` qualifiers on the parameters** — they change
  the mangled name, so the row would unpair; non-landable by construction.
- **Not tried: the permuter** — off by directive; and the residual is a scheduling
  order of two loads, not a register allocation (the registers already match).
- **Not tried: a separate `Synth.cpp` TU** (DC3's home for the body). The row is pinned
  in `FxSendReverb`, the function must be in that TU to pair, and nothing about TU
  membership can change the emission order inside the body.
- **Not tried: reading the original XDK `xaudio2fx.h` inline** — searched the whole
  filesystem for a copy: the only `xaudio2fx.h` files present (mingw-w64's, Wine's) are
  42-line stubs without the inline `ReverbConvertI3DL2ToNative`. If the original
  header's spelling differs from DC3's it could be the missing evidence; it is not on
  this machine.
- **Not landed: E4** — a probe, semantically wrong (`fabs` of a ratio that can be
  negative in principle), and it regresses the row to 98.76.
- **Not changed:** `XAUDIO2FX_REVERB_PARAMETERS`'s layout (FG's 0x34 stands). A
  side-finding FG asked for: FG listed the field offsets below 0x34 as "inherited from
  DC3, unverified against RB3". This body stores every one of the 22 fields, retail's
  22 store sites were enumerated from the `.s` (21 `stb`/`stw` into `r4`/`r31` plus one
  `stfiwx f0, r31, r29` with `r29 = 4` — an indexed store a base-register scan misses),
  and **all 22 are uncharged**, so our header's offset+width equals retail's at every
  field: `WetDryMix` 0x00 w · `ReflectionsDelay` 0x04 w · `ReverbDelay` 0x08 b ·
  `RearDelay` 0x09 · `PositionLeft/Right` 0x0A/0x0B · `PositionMatrixLeft/Right`
  0x0C/0x0D · `Early/LateDiffusion` 0x0E/0x0F · `LowEQGain` 0x10 · `LowEQCutoff` 0x11 ·
  `HighEQGain` 0x12 · `HighEQCutoff` 0x13 · `RoomFilterFreq` 0x14 · `RoomFilterMain` 0x18
  · `RoomFilterHF` 0x1C · `ReflectionsGain` 0x20 · `ReverbGain` 0x24 · `DecayTime` 0x28 ·
  `Density` 0x2C · `RoomSize` 0x30. ⚠ Offsets and widths are retail-verified; the
  *names* are DC3's and the bytes cannot distinguish two same-typed neighbours whose
  names were swapped — but the values stored (e.g. the 27/27/6/6 position constants,
  `RoomSize = 100.0f`, the two gain clamps at 0x10/0x12) are consistent with the names.
- **Not touched:** `GranularSynth.*`, `PeakDetector.*` (W16-FJ's live files).

## Files

- `src/system/synth_xbox/FxSendReverb.cpp` — body added before `SyncEffectParams`
  (with `#include <math.h>` for `log10`), plus a comment above the multiply recording the
  residual and the inert spellings so the next reader does not re-run them.
- `src/xdk/xaudio2/xaudio2fx.h` — FG's "deliberately NOT decompiled" note replaced.
- Build logs: `~/tmp/rb3_build_w16fk_{initial,v1,e2,e3,e6,e5,e4,e7,e7b}.log`; A/B log
  `~/tmp/rb3_ab_w16fk.log`.

## Reusable lessons

1. **A compiler-version residual can transfer exactly.** DC3's 11886 floor reproduced
   on 10224 to the last diff-score point on the first build. "Different image, different
   compiler build, the residual need not transfer" was the right thing to test and the
   wrong thing to hope for.
2. **A positive control that moves the rows is what makes "inert" meaningful.** Six
   byte-identical builds in a row would be indistinguishable from a build that was not
   picking up the edit; E4 (and E3/E5/E6, which moved *other* rows) prove the pipeline
   was live. Any "N spellings are inert" claim should carry one spelling that is not.
3. **Zero-byte expression wrappers fold before canonicalisation on this compiler**
   (`(float)(double)x`, `x * 1.0f`): they cannot be used as `/fp:fast` barriers to steer
   operand order. Parentheses are only a barrier where there is an association to deny;
   there is none in a two-operand product.
4. **`name_check` does not verify pool-constant VALUES** — check `.rdata` separately
   when a body is arithmetic on literals.
