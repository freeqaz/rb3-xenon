# The body-write surface repriced: ~5 kB, not 52 kB — and its top unit is code we already hold (lane GRIND-1, 2026-08-14)

BODYWRITE-1 censused IDENT-1's class 3 and reported the genuine "write a body"
surface as **≤ 52,012 B over 60 units**, naming `MetaPerformer` (8,728 B),
`GemTrackDir` (4,052 B), `PatchDir` and `MeshDeform` (3,280 B) as the top
targets — while flagging its own suspicion that "the Wii **dev** build's surplus
functions are themselves suspect".

**That suspicion was correct, and it is bigger than the caveat suggests. The
surface is ~5 kB.** This lane did not overturn BODYWRITE-1's conclusion — it
*strengthens* it. Same tree, `dd8dc187`; class-3 census reproduced exactly
(6,973 rows / 1,229,420 B / 725 units, self-validation OK).

## 1. Why the two numbers differ: a line-count proxy vs a function-count one

BODYWRITE-1's 52,012 B came from comparing **source line counts** — "the oracle
file has more lines than ours" — over the 197 units whose pin its contamination
detector could clear. Line count conflates comments, formatting, and rb3-Wii
**dev-build-only** code with actually-missing functions.

`tools/nobody_why.py` already ships the direct measure: per unit,
`deficit = retail functions in the pin − code symbols our base obj defines`. Run
over **all 725 units** rather than its default top-40:

| | rows | bytes |
|---|---|---|
| `UNWRITTEN (short of code)` | 1,060 | **209,528** |
| `divergent/other` | 5,913 | 1,019,892 |

## 2. 96.9% of the "unwritten" class is Quazal vendor middleware

Joining those 122 `UNWRITTEN` units back to their `src_path`:

| origin | units | bytes | share |
|---|---|---|---|
| **`src/network` (Quazal)** | **103** | **202,900** | **96.9%** |
| `src/system` engine | 16 | 4,304 | 2.1% |
| `src/band3` game | 2 | 2,272 | 1.1% |
| `src/xdk` vendor | 1 | 52 | 0.0% |

Six of the largest carry **`ourfn = 0`** — the 7-line `namespace Quazal {}` map
scaffolds AUTOID-1 sized. BODYWRITE-1 ruled these out explicitly and was right
to: no oracle, `/Od` vendor middleware, and a body there buys a pairable row
with no content.

**Non-vendor residue: 18 units / 6,576 B.** Three of those are third-party
libraries that merely *live* under `src/system` — oggvorbis `lsp.c` (944 B),
curl `ssluse.c` (216 B), tomcrypt `aes.c` (68 B) — leaving **15 units /
5,348 B ≈ 0.052% of `total_code`.**

## 3. The top non-vendor unit is NOT unwritten — adjudicated on retail bytes

`default/band3/tour/TourCondition`, 9 class-3 rows / 1,424 B, deficit **+17**,
i.e. 22% of the whole non-vendor surface and its #1 target.

**Our `TourCondition.cpp` is 145 lines — the SAME line count as the rb3-Wii
oracle** — and defines all 15 methods. So "ours < oracle" never applied.

8 of the 9 rows sit in a **detached second `.text` block**,
`0x82364428-0x82364AA0`, ~40 kB from the unit's main run
(`0x8235A480-0x8235AECC`) and wedged between TourProgress blocks (which end at
`0x82364424`) and GigFilter (`0x82364AA0`). Four rows are exactly 120 B at a
uniform 0x78 stride.

⚠ **This lane predicted a mis-pin — a second `rnddx9/Rnd` — and was WRONG.**
The detached block calls `GetPropertyValue@TourPropertyCollection@@`,
`GetPerformanceProperties@TourProgress@@` and `HandleTourRewardApplied@TourProgress@@`,
which reads as TourProgress's. But our own source refutes it:

```cpp
bool TourCondition::IsGreaterConditionMet(const TourPropertyCollection &tp,
                                          const DataArray *i_pArray) const {
    Symbol key = i_pArray->Sym(1);
    float  val = i_pArray->Float(2);
    return tp.GetPropertyValue(key) > val;      // Sym -> Float -> GetPropertyValue
}
```

That is *exactly* the retail signature of the four 120 B siblings
(`?Sym@DataNode@@` → `?Float@DataNode@@` → `?GetPropertyValue@TourPropertyCollection@@`)
— they are `IsGreater` / `IsLess` / `IsGreaterEqual` / `IsLessEqual`. The 380 B
row at `0x823646E0` calls `??0Symbol@@QAA@PBD@Z` repeatedly: it is
`IsComparisonConditionMet` building its six `static Symbol` comparators.
**The pin is correct and the block is TourCondition's.** Recording the failed
prediction because "detached block wedged in another TU's run" looked like a
strong mis-pin tell and is not one.

⇒ A unit labelled `UNWRITTEN` on a +17 deficit is one where **we hold every
function**. Its work is divergence (and per-row identification), not writing.
So **5,348 B is itself an over-estimate**: at least TourCondition's 1,424 B —
27% of it — is not write surface at all.

## 4. BODYWRITE-1's own #1 target, measured

`default/MetaPerformer`, 8,728 B: **deficit −666** (retail 358 functions in the
pin, we compile 1,024 symbols). Not short of code. Its asm extent equals its
`report.json` size (8,728 == 8,728), so this one is *not* a size-inflation
artifact — the line-count proxy simply pointed at a unit that is divergent.

★ The `report.json` targeting hazard is nonetheless live in this data:
`default/Shader` ranks #6 by billed bytes (12,072 B) and **falls out of the top
25 entirely** when re-ranked on asm extents, exactly as BODYWRITE-1 found for its
8,852 B / 12-real-bytes `fn_824A59D4`.

## 5. Instrument caveat — the bias runs BOTH ways

`nobody_why.py`'s docstring warns its supply side is biased **up** (our objs
carry COMDAT instantiations retail folded), so "a positive deficit is strong
evidence of missing code; a negative one is not proof of its absence". The
TourCondition case shows the demand side is biased up **too** — retail's row
count includes funclets and static-init/guard thunks our obj emits differently.
⇒ treat `UNWRITTEN` as a **high-precision-claimed but empirically leaky**
screen, and never as a work order on its own. Every candidate still needs
retail-byte adjudication before a line is written.

## 6. What this lane deliberately did not do

- **Did not write any body.** The measurement removed the premise: outside
  vendor middleware there is ~5 kB of claimed surface, and its largest unit is
  code we already hold. Writing there would be invention, not decomp.
- **Did not touch the Quazal units** — same reasoning as BODYWRITE-1.
- **Did not re-pin TourCondition.** The mis-pin hypothesis was refuted; the pin
  stays as it is.
- **Did not land the deferred sub-gate rows** (<128 B FP 2.66%, template FP
  4.71%) — an unproven map repair is worse than no repair.

## 7. Status of the IDENT-1 tier-A gate queue: DRAINED

31 rows / 9,824 B pass IDENT-1's exact landing gate (same-unit ∧ `'?$' not in
name` ∧ ≥128 B): **20 engine landed by IDENT-1**, **10 band3 landed by this lane**
(`dd8dc187`, +10 matched / +3,084 B), and **1 blocked on name-injectivity** —
`?SetJump@StandardStream@@UAAXMMPBD@Z` is already assigned to `0x827029d8` while
the queue proposes `0x827035f0`. Adjudicating which address owns that name is a
separate job; it is correctly not a landing-gate decision.
