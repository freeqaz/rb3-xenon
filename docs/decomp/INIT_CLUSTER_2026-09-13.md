# The `Init` cluster's one systematic cause -- lane W11-B, 2026-09-13

Branch `w11-init-cluster`, worktree `~/tmp/wt-w11-b`, base main **`77cac933`**.
Ruler **`name_check` (graded)**, read from `report.json`'s
`provenance.diff_config` -- not assumed.  Predecessor:
`docs/decomp/SPATIAL_FIRES_2026-09-13.md` (lane W10-A, merge `654dc785`), whose
S4.2 posed the question this lane answers.

Baseline, this worktree's first full build (built **before** any name-keyed
lookup -- a reflinked tree's target objs are pre-renamer).  The build's own
`CHECK TARGET OBJS RENAMED` step reported **25,537 / 29,045 map names present in
3,083 target objs = 87.9%**, against W10-A's 25,523 / 29,034 = 87.9%, so no
mangled-name lookup below is vacuous.

```
matched_functions     42,766
matched_code_percent  37.812890
fuzzy_match_percent   49.143230
total_code            10,245,956
total_functions       69,219
```

---

## 0. Headline

- **The six rows at exactly 59.090908 were ONE missing call**, and it is not
  subtle once you read retail bytes: `X::Init()` in retail is 22 instructions,
  ours was 13, and the nine missing ones are a `Symbol` ctor plus
  **`TheUI->InitResources("<ClassName>")`**.  Predicted **+6 fns / +528 B**,
  measured **+6 / +528**.
- **The screen is CLOSED, not open-ended.**  Exactly **23** retail bodies in the
  whole split set call `UIManager::InitResources`; 15 already scored 100 (our
  source has the call), 7 were the cluster, 1 was unnamed.  There is no eighth.
  A bounded vein, now drained -- right about the mechanism, wrong about the
  runway if anyone calls it a force multiplier with more to give.
- **A second self-witnessing screen fell out of it, and it is stronger than the
  first.**  W10-A's insight was that a Milo `Init()` names its own class via
  `StaticClassName`.  It *also* names its factory: the 2nd argument to
  `RegisterFactory(X::StaticClassName(), X::NewObject)` **can only be**
  `X::NewObject`.  Over all **322** RegisterFactory sites: 113 disagree with the
  map, but **111 are merely UNNAMED** (a forgiven placeholder, not a defect) and
  exactly **2 are actively MIS-named**.  One of those was the entire reason
  BandLabel sat 0.4545 pp below its six siblings.
- Four changes measured, **every one pre-registered before measuring**;
  **lane total +10 functions / +1,508 bytes**, 0 units fell off 100% on any run.
- **One of my own framings was wrong and a census killed it:** I described
  BandLabel's pin as "a byte-exact pin hole, the shape W10-A fixed".  There are
  **915** byte-exact single-block foreign holes in `splits.txt`.  The shape is
  near-worthless as evidence; the **body adjudication** is what justified the
  change, and the hole only said *where* to move the pin.

---

## 1. Testing the brief literally

W10-A's S4.2 reported six rows on exactly 59.090908 and BandLabel on 58.636364.
**Re-read out of `report.json` rather than inherited** -- all seven reproduce to
the last digit, at size 88:

| row | size | fuzzy |
|---|---:|---:|
| `?Init@MiniLeaderboardDisplay@@SAXXZ` | 88 | 59.090908 |
| `?Init@MeterDisplay@@SAXXZ` | 88 | 59.090908 |
| `?Init@UIButton@@SAXXZ` | 88 | 59.090908 |
| `?Init@UISlider@@SAXXZ` | 88 | 59.090908 |
| `?Init@LabelShrinkWrapper@@SAXXZ` | 88 | 59.090908 |
| `?Init@LabelNumberTicker@@SAXXZ` | 88 | 59.090908 |
| `?Init@BandLabel@@SAXXZ` | 88 | **58.636364** |

The arithmetic that made this worth opening (`N = size/4 = 22`):

- `100 - 59.090908 = 40.909092 = 9 x (100/22)` => **nine of 22 instructions are
  fully mismatched**, identically, across six independent classes.
- `100 - 58.636364 = 41.363636 = 9 x (100/22) + 2 x (5/22)` => BandLabel is the
  same nine **plus two relocation-name charges**.  That second term is a
  *separate* defect, and S3 is what it turned out to be.

> **My first search for these bodies returned a clean, decisive-looking NOTHING,
> and it was vacuous**: the `.s` files write `.fn fn_8231A498` in **uppercase**
> hex and I searched lowercase, so five of seven "did not exist".  Same family
> as the grep-binary trap -- *a negative that agrees with no prior at all is
> still worth re-testing with a different spelling.*

---

## 2. Change A -- the systematic cause

### 2.1 What retail actually does

Dumped from the split `.s`, keyed on the **`.fn fn_<addr>` symbol** (never the
synthetic address column), with every `bl` resolved through the map and every
`.rdata` string read out of `orig/45410914/band.exe` through the PE section
table. Retail `?Init@UIButton@@SAXXZ`, 22 instructions:

```
Symbol tmp("UIButton");          ; ??0Symbol@@QAA@PBD@Z, arg = lbl_821256C8 = "UIButton"
TheUI->InitResources(tmp);       ; lwz r3, lbl_82C721F0  ->  ?InitResources@UIManager@@QAAXVSymbol@@@Z
Hmx::Object::RegisterFactory(UIButton::StaticClassName(), UIButton::NewObject);
```

Our source was only the third line. **The string argument is literally the
class name in all seven cases** — read, not guessed:

| class | `.rdata` label | string |
|---|---|---|
| MiniLeaderboardDisplay | `lbl_82030C10` | `"MiniLeaderboardDisplay"` |
| MeterDisplay | — | `"MeterDisplay"` |
| UIButton | `lbl_821256C8` | `"UIButton"` |
| UISlider | — | `"UISlider"` |
| LabelShrinkWrapper | — | `"LabelShrinkWrapper"` |
| LabelNumberTicker | — | `"LabelNumberTicker"` |
| BandLabel | `lbl_8202F938` | `"BandLabel"` |

**Two call orders exist, and they are per-class.** This is the one thing that
could not be assumed:

| order | classes |
|---|---|
| `Register()` **then** `InitResources` | MiniLeaderboardDisplay, MeterDisplay, LabelShrinkWrapper, LabelNumberTicker |
| `InitResources` **then** `Register()` | UIButton, UISlider, BandLabel |

### 2.2 The positive control, run before touching anything

`TheUI->InitResources("X")` is **already** the house idiom — 14 sibling classes
carry it, and **every one scores 100.0 at size 88**, with both orders
represented (`ScoreDisplay` = Register-first, `BandButton` = InitResources-first):

> ScoreDisplay, BandButton, StarDisplay, CheckboxDisplay, BandHighlight,
> MicInputArrow, PlayerDiffIcon, ScrollbarDisplay, InstrumentDifficultyDisplay,
> ReviewDisplay, BandSwatch (100 B), InlineHelp, UILabel (112 B), UIList (304 B)

So the fix was not a hypothesis to test; it was a template to apply, with a
14-row control saying what a correct application scores.

### 2.3 The screen is closed

Indexed every `.fn` body in the split set that calls
`?InitResources@UIManager@@QAAXVSymbol@@@Z` (**3,083 `.s` files, including the
1,810 `auto_*` units**, so the scope is the whole split set, not just named
units):

| class | n |
|---|---:|
| retail bodies calling `InitResources` | **23** |
| already 100 (our source has the call) | 15 |
| the sub-100 cluster (this lane) | **7** |
| unnamed retail body | 1 — `0x8233F4E0` |

⇒ **there is no eighth row.** The vein is bounded and now drained. Calling this
a "force multiplier" is right about the mechanism and wrong about the runway.

`0x8233F4E0` is **`?Init@BandList@@SAXXZ`**, corroborating W10-A's handoff 2
(which listed `?Init@BandList@@` as pinned into `CharSignalApplier`). Its body,
which W10-A did not have, is:
`PhysMemTypeTracker t("D3D(phys):Global"); TheUI->InitResources("BandList"); Register();`

### 2.4 Predicted vs measured

Pre-registered in `~/tmp/w11b_predictions.md` before the first edit:

> 6 rows 59.090908 → 100.0 (+88 B each); BandLabel 58.636364 → **99.545455**,
> **+0 B** (its residual is the two relocation-name charges, and `matched_code`
> is all-or-nothing per row). **Δ = +6 functions / +528 B.** Range allowing for
> collateral from the five new `#include "ui/UI.h"`: Δfns [+5,+7], Δbytes
> [+440,+616]. Units falling off 100%: 0.

Measured (`ab_measure --from-dirty`, ruler `name_check`):

```
Δmatched=+6  Δmasked_equal=+0  Δhonest=+6  Δcode%=+0.005153pp  Δcode_bytes=+528
Δfuzzy=+0.002453pp   (49.143230 -> 49.145683)
unit net (ALL units) = +6   vs whole-binary Δmatched = +6
units at 100%: mpn 163->163, all-rows-fuzzy 135->135  (0 reached 100, 0 FELL OFF)
```

Per-unit, exactly the six expected and nothing else: `+1` each to
LabelNumberTicker, LabelShrinkWrapper, MeterDisplay, UIButton, UISlider,
system/hamobj/MiniLeaderboardDisplay.

And **BandLabel landed on 99.545456 against a prediction of 99.545455** — the
mechanism is confirmed to the last digit, which is what licenses §3 treating
the residual as exactly one relocation.

★ The run's real risk was the five new `#include "ui/UI.h"` (a header add can
perturb inlining across a whole TU, and W10-C's lesson is that an isolation
control licenses nothing about composition). **It did not materialise here** —
no unit outside the six moved at all. That is a measurement, not a guarantee
for the merge.

---

## 3. Change B — the second self-witnessing screen

### 3.1 The screen

W10-A found that a Milo `Init()` **states its own identity**, because it
registers `X::StaticClassName()`. Working on the cluster made a second,
stronger form obvious: the call is

```
Hmx::Object::RegisterFactory(X::StaticClassName(), X::NewObject)
```

so the **second argument can only be `X::NewObject`** — the class is already
pinned by the first argument. That is a self-witnessing pair with nothing to
tune, and unlike the spatial screen it needs no notion of neighbourhood.

Run over every `RegisterFactory` call site in the split set:

| class | n |
|---|---:|
| call sites found | **322** |
| 2nd arg agrees with `?NewObject@<registered class>@@` | 209 |
| 2nd arg address is **UNNAMED** | **111** |
| 2nd arg is **actively MIS-named** | **2** |

⛔ **The 111 are not defects and must not be counted as precision.** Under
`name_check` objdiff *forgives* a placeholder target (`fn_`/`lbl_`/…), so an
unnamed callee is already uncharged; those rows are an identification backlog,
not an error list. Counting them as "the screen fired 113 times" would be the
charge-counting mistake CLAUDE.md records — restating the detector's own input.
The screen's actual claim is **2 rows out of 322**.

### 3.2 Adjudicating the one that mattered

`0x82341FF8`, mapped `??_GSpotlightDrawer@@UAAPAXI@Z`, is the `NewObject`
argument of `?Init@BandLabel@@`. Its retail body:

```
addi r3, r31, 0x50 ; bl ?StaticClassName@BandLabel@@SA?AVSymbol@@XZ
li r4, 0 ; li r3, 0x290 ; bl ?MemAlloc@@YAPAXHH@Z      ; sizeof(BandLabel) = 0x290
li r4, 1 ; bl ??0BandLabel@@QAA@XZ
<standard Hmx::Object base adjust>  ; lwz 0x4 / lwz 0x4 / add / addi 0x4
```

That is `new BandLabel`. A **scalar deleting destructor it is not** — different
signature, different shape, and it would never appear in a `RegisterFactory`
factory-pointer slot.

Corroboration, and the reason this is not merely "the map disagrees with me":
**every other SpotlightDrawer symbol lives at `0x824D....`** —
`??1SpotlightDrawer@@` at `0x824d7438`, `?StaticClassName@SpotlightDrawer@@` at
`0x824d17c8`, `??_GNgSpotlightDrawer@@` at `0x824d45b0`. Only this one claim sat
2 MB away, inside BandLabel's run.

Two mandatory checks before editing:

- **Alias file** (standing rule — grep `symbol_aliases.json` before believing a
  relocation-name find): **0 groups** touch `0x82341ff8`, `0x82574348`,
  `??_GSpotlightDrawer@@UAAPAXI@Z`, `?NewObject@BandLabel@@` or
  `?NewObject@MetaPanel@@`. No forgiveness is at stake, so this is a real change
  and not an alias artefact.
- **Rename safety** (*proving a name wrong ≠ renaming is safe* — the base obj
  may not define it, leaving the row permanently 0%): read out of the COFF
  symbol table **after a full build**, our `BandLabel.obj` **does** define
  `?NewObject@BandLabel@@SAPAVObject@Hmx@@XZ`.

### 3.3 ⛔ A framing of mine that a census killed

My first note called `0x82341FF8` "a byte-exact pin hole punched out of
BandLabel's run — the shape W10-A fixed for RGTrainerPanel", as if the shape
were the evidence. It is not. A census of `splits.txt` finds **915 byte-exact
single-block foreign holes** tree-wide: interleaved COMDATs are the normal
state of this binary, and one more hole means essentially nothing.

⇒ **the hole told me WHERE to move the pin; the body adjudication is the only
thing that said anything was wrong.** Do not promote "it's a hole" to a screen —
at 915 instances it would be a fire hose with no precision.

### 3.4 Predicted vs measured

> **Predicted:** `?Init@BandLabel@@` 99.545456 → 100.0 (+1 fn / +88 B);
> `?NewObject@BandLabel@@` becomes a paired 112 B row, reaching 100 **uncertain**;
> `??_GSpotlightDrawer@@` (41.71, 0 B) leaves the denominator at no cost.
> **Δfns [+1,+2], Δbytes [+88,+200].** Units falling off: 0.

Measured — **the low end**:

```
Δmatched=+1  Δhonest=+1  Δcode%=+0.000857pp  Δcode_bytes=+88
unit net (ALL units) = +1   vs whole-binary Δmatched = +1
units at 100%: mpn 163->163, fuzzy 135->135  (0 reached 100, 0 FELL OFF)
```

| row | before | after |
|---|---|---|
| `?Init@BandLabel@@SAXXZ` | 99.545456 | **100.0** |
| `??_GSpotlightDrawer@@UAAPAXI@Z` in `default/MoveMgr`, 112 B | 41.714287 | *(row gone)* |
| `?NewObject@BandLabel@@` in `default/BandLabel`, 112 B | *(did not exist)* | **86.92857** |

★ `total_code` (10,245,956) and `total_functions` (69,219) are **identical on
both legs** — the pin move is pure reattribution and moved no denominator.
The accuracy gain is larger than the byte gain: a 112 B body that was being
scored against the wrong class in the wrong unit at 41.7 is now scored against
the right one at 86.9.

### 3.5 The split rewrote its own input — and the refusal was right

The first A/B of this change was **REFUSED (exit 2, no verdict)** by the
split-guard: dtk re-derived the `.pdata` ranges from my `.text` move
(`MoveMgr` drops `0x821FE8F8-0x821FE900`; BandLabel's two merge into
`0x821FE7B8-0x821FE918`). That is documented behaviour — `.pdata` is derived
output, never input — and the tool restored my edit rather than pricing a
non-fixed-point tree. Recovery was two builds to reach the split's own fixed
point, then re-measure; the committed `splits.txt` is that fixed point.

---

## 4. Changes C and D — RGTrainerPanel, and what `unke5` really is

### 4.1 Change C — the ctor, priced before it was diffed

`??0RGTrainerPanel@@QAA@XZ`, 224 B at 98.19643. **Priced from the percentage
alone, before opening a diff** (`N = 56`):

```
100 − 98.19643 = 1.80357 = 1 × (100/56) + 1 × (1/56) = 1.8035714
```

⇒ exactly one insert/delete plus one immediate-argument charge. The graded
diff then named both, and they are two halves of one cause:

| idx | type | target | base |
|---|---|---|---|
| 42 | `diff_arg` | `stb r29, 0x118(r30)` | `stb r29, 0x10d(r30)` |
| 45 | `insert` | — | `stb r29, 0x118(r30)` |

Our `unke5(0)` emitted a store at **0x10d that retail does not emit at all**,
pushing our `mLefty` (0x118) store one slot out of alignment: our 0x10d store
paired against retail's 0x118 store (the immediate charge) and our real 0x118
store became an insert (the full charge). Retail's ctor stores 0x100/0x104/0x108
(the vector), 0x10c, 0x110 (`-1`), 0x114 (`-1.0f`; the constant at
`lbl_8200EDA8` reads back as exactly `-1.0`), 0x118 and 0x274 — never 0x10d.

**Predicted → 100.0, +1 fn / +224 B. Measured +1 / +224.**
`unit net (ALL) = +1 == whole-binary Δmatched`; 0 units fell off; 1 recompile.

⚠ **Two caveats on the pricing screen, both learned here.**
1. The decomposition is **never unique**: `100/N == 20 × (5/N) == 100 × (1/N)`,
   so "1 insert/delete" and "20 relocation-name args" are *arithmetically
   identical*. The percentage gives a deficit in instruction-equivalents; only
   the charged-site list disambiguates. I got C right because the diff agreed,
   not because the arithmetic was decisive.
2. It only discriminates on **small** rows. Sweeping the binary for
   "1 insdel + 1 imm" with a 1e-3 tolerance returned 2 rows — but for the 5,036 B
   one the two hypotheses differ by 0.0008 pp, far below the tolerance, so the
   screen could not tell them apart. Re-priced exactly,
   `?Handle@CustomizePanel@@` is **1 insert/delete** away
   (`100 − 99.92057 = 0.0794281 = 1 × 100/1259`, remainder 0.000002), which is a
   materially better position than the 3-mismatches-plus-2-aliases that
   RESIDUAL-1 recorded on 2026-08-14. Worth a fresh look by someone; not chased
   here.

### 4.2 Change D — the deferral latch is a dev-build construct

Change C left `Poll()` reading an **uninitialised** `unke5`, which retail does
not do — because retail's `Poll` does not read 0x10d at all. Retail's
`?Poll@RGTrainerPanel@@` is 20 instructions, in full:

```cpp
GemTrainerPanel::Poll();
if (mGemPlayer) if (mLegendMode) HandleChordLegend(true);
```

and `?SetLegendModeImpl@RGTrainerPanel@@` (`0x826b0f30`) has **exactly one
referencer in the entire split set**: `?Handle@RGTrainerPanel@@` (`0x826b1458`).
Retail calls it **directly from the handler**; there is no latch anywhere in the
binary. Our port has `Handle → SetLegendMode → unke5 → Poll → SetLegendModeImpl`.

⇒ **the latch is an rb3-Wii DEV-build construct the port carried across**, and
Change C's "retail does not initialise 0x10d" is the same fact seen from the
ctor. `SetLegendModeImpl()` itself opens with `mLegendMode = mode;`, so
swapping the handler to call it directly is the *immediate* form of the same
operation, not a different one.

**Predicted +1 fn / +80 B (Poll only); range Δfns [+1,+2] / Δbytes [+80,+668].
Measured +2 / +668 — the top of the range.**

```
Δmatched=+2  Δhonest=+2  Δcode%=+0.006519pp  Δcode_bytes=+668
unit net (ALL units) = +2   vs whole-binary Δmatched = +2
units at 100%: mpn 163->163, fuzzy 135->135  (0 reached 100, 0 FELL OFF)
?Poll@RGTrainerPanel@@    59.7    -> 100.0   (+80 B)
?Handle@RGTrainerPanel@@  95.2381 -> 100.0   (+588 B)
```

⚠ **My central prediction was wrong, in the good direction, and the reason is
the reusable part.** I predicted `?Handle@` would *not* cross, reasoning that
one call-site swap cannot be worth seven insert/deletes. It was worth all seven
— because `SetLegendMode` is a two-store one-liner that `/O1` **inlines** into
`Handle`, so the latch cost the call site an inlined `stb`+`stw` pair plus the
branch/argument shuffle around them, not a single `bl`. ⇒ **when the construct
you are removing is inlinable, price it by what the INLINE costs at the call
site, not by the number of source lines deleted.** Quoting a range rather than
a point is what kept this an under-prediction instead of a miss.

---

---

## 5. Ledger

Every A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w11-b --from-dirty`,
one change per run, ruler `name_check`, per-unit attribution checked on every
run, committed immediately.

| # | change | kind | predicted | measured | unit net == Δmatched? | fell off 100% |
|---|---|---|---|---|---|---|
| A | `TheUI->InitResources` × 7 | source | **+6 / +528** (range [+5,+7]/[+440,+616]) | **+6 / +528** | +6 == +6 ✓ | **0** |
| B | `0x82341FF8` → `?NewObject@BandLabel@@` + pin move | map+splits | [+1,+2] / [+88,+200] | **+1 / +88** (low end) | +1 == +1 ✓ | **0** |
| C | ctor: drop `unke5(0)` | source | **+1 / +224** | **+1 / +224** | +1 == +1 ✓ | **0** |
| D | remove the `unke5` latch (Poll + handler) | source | +1 / +80 (range [+1,+2]/[+80,+668]) | **+2 / +668** (high end) | +2 == +2 ✓ | **0** |

**Lane total: +10 functions / +1,508 bytes.** The legs compose:
`42,766 → 42,772 → 42,773 → 42,774 → 42,776` and
`code% 37.812890 → 37.818043 → 37.818900 → 37.821087 → 37.827606`
(+0.014716 pp), each leg B being the next leg A, measured in-run — no absolute
was inherited from anywhere.

**Two of four predictions were exact, one landed at the bottom of its range and
one at the top.** Both misses are informative and both are recorded above (§3.4
`NewObject` did not cross; §4.2 the inline cost). The two exact ones came from
the same discipline W10-A identified: price from the charged-site list and the
`5/N` arithmetic, and read the *positive control* before predicting.

---

## 6. What this lane did NOT do

- **Did not fix `0x82574348`** — the *other* actively mis-named `RegisterFactory`
  argument, and the more interesting of the two. See handoff 1; it is fully
  adjudicated but deliberately left, because it is the one change in sight that
  is likely to be **net-negative on the headline while being more accurate**,
  and that deserves its own measured lane rather than being bundled behind three
  positive ones.
- **Did not name any of the 111 unnamed `NewObject` addresses**, even though the
  `RegisterFactory` argument position *proves* each identity. Under `name_check`
  a placeholder target is already forgiven, so the call-site byte upside is
  **exactly zero**; the payout is pairing (+1 honest each) and bug exposure, and
  many need pin moves. That is an identification lane, not this one.
- **Did not touch the MetaPanel pin cluster** (handoff 2), which is six foreign
  single-block holes in one run and almost certainly one cause.
- **Did not chase `?Handle@RGTrainerPanel@@`** (588 B, 95.2381 = 7 insert/deletes)
  beyond whatever Change D moved incidentally. Change D was scoped to the latch;
  the remaining insert/deletes are a separate body port.
- **Did not touch `scripts/symbol_aliases.json`.** I read it (0 groups relevant)
  and made no edit.
- **Did not promote either screen to `tools/`.** The `RegisterFactory`
  2nd-argument screen is ~40 lines and has a measured precision of 2 real
  defects out of 322 sites with 111 abstentions — a real number, but one lane's
  worth. W10-A declined to promote its screen for the same reason and I am not
  going to be the lane that ships an unmeasured detector.
- **Did not re-adjudicate W10-A's spatial fires.** Its own §5 said not to fund
  another pass over that stratum and nothing here contradicts that.

---

## 7. Handoffs

1. ★★ **`0x82574348` is `?NewObject@MetaPanel@@`, not `?NewObject@FlowAnimate@@`
   — and it currently reads a FALSE 100.0.** This is the second of the two
   mis-named `RegisterFactory` arguments. Evidence:
   - It is the factory argument of the `RegisterFactory(MetaPanel::…)` call at
     owner `0x82574E20`.
   - Body: `li r3, 0x104; bl ??2CriticalSection@@SAPAXI@Z; bl fn_82573EE0` —
     `new MetaPanel` with `sizeof == 0x104`.
   - It is a single foreign block pinned to `Flow.cpp` punched out of
     MetaPanel.cpp's run (`0x82573904→0x82574348`, `0x825744B8→…`).
   - Our `MetaPanel.obj` **does** define `?NewObject@MetaPanel@@`, so the rename
     is safe; `Flow.obj` defines `?NewObject@FlowAnimate@@` and not the other.
   ⛔ **The row scores 100.0 today only by FORGIVENESS**: retail's ctor callee
   `fn_82573EE0` is unnamed, hence a forgiven placeholder, and the allocation
   size coincides — so our `FlowAnimate::NewObject` scores 100 against
   *MetaPanel's* body. A textbook instance of *never read a 100% row as evidence
   that a callee is right.* **Expect the fix to cost 100 B and possibly return
   152 B; price it, and land it on accuracy grounds even if it is negative.**
2. ★ **MetaPanel's run has SIX foreign single-block holes and they smell like one
   cause**: `Flow.cpp` (0x82574348, 152 B), `CalibrationPanel.cpp` (0x825743E0,
   216 B), `HamMove.cpp` (0x825748A0, 68 B), `HamIKEffector.cpp` (0x825748E8,
   80 B), `FlowDistance.cpp` (0x825749E4, 68 B), `UIComponent.cpp` (0x825738B8,
   76 B). The multi-register `Init` at `0x82574E20` that registers MetaPanel,
   CalibrationWelcomePanel, NextSongPanel, AppScoreDisplay and AppLabel — all
   *meta_band* classes — is itself pinned to `Flow.cpp`. ⚠ Do **not** open this
   on the hole shape alone (§3.3: 915 tree-wide); open it on the
   `RegisterFactory` argument evidence, which names five of the classes.
3. ★ **111 unnamed `NewObject` addresses, each identity PROVEN by the
   `RegisterFactory` argument position.** Concentrated: `Char` 22,
   `BandCharacter` 20, `CheatProvider` 12, `system/rndobj/Rnd` 10, `LightPreset`
   6, `UIList` 5, `UI` 4, `Flow` 4. ⚠ `CheatProvider.s` holding
   `Fader`/`FxSend*`/`MoggClip` factories is itself a mis-pin signal worth a
   look. Economics: **zero call-site bytes** (placeholders are already forgiven),
   +1 honest per pairing, and bug exposure — price it as identification work,
   not as a byte lever.
4. **`?Init@BandList@@SAXXZ` is `0x8233F4E0`**, pinned into `CharSignalApplier.s`
   (W10-A handoff 2 listed it without the address). Full body now known:
   `PhysMemTypeTracker t("D3D(phys):Global"); TheUI->InitResources("BandList");
   Register();` — so when its pin is moved it needs the `InitResources` line too,
   i.e. it is an eighth member of §2's family that the screen could only see as
   "unnamed".
5. **`?NewObject@BandLabel@@` sits at 86.92857 (112 B)** after Change B — newly
   pairable, not yet matched. Its deficit is 13.07143 on `N=28`; note §4.1's
   caveat that the decomposition is not unique, so read its charged-site list
   rather than trusting an arithmetic split.
6. **`?Handle@CustomizePanel@@` is now ONE insert/delete away** (5,036 B,
   99.92057, remainder 0.000002 after a single `100/N`), which does **not**
   match the 3-mismatches-plus-2-ICF-aliases that RESIDUAL-1 recorded on
   2026-08-14. Main has moved. It is the largest single-instruction prize I saw
   all lane; verify against the charged-site list before believing my
   arithmetic, for exactly the reason in §4.1.
7. **`?Poll@RGTrainerPanel@@` / `?Handle@RGTrainerPanel@@`** — see §4.2 and the
   Change D result. Whatever Change D leaves open in `Handle` is a body port
   with the oracle disagreement already localised (the dev-build latch).
8. **The `RegisterFactory` 2nd-argument screen is ~40 lines and reproducible**:
   index every `.fn` body calling
   `?RegisterFactory@Object@Hmx@@SAXVSymbol@@P6APAV12@XZ@Z`, pair each call with
   the `?StaticClassName@X@@` resolved immediately before it and the
   `addi r4, r11, fn_<addr>` operand, and report where the operand is **named but
   not** `?NewObject@X@@SAPAVObject@Hmx@@XZ`. ⚠ Two things it MUST do or it lies:
   resolve callees **through the map** (the `.s` shows only `bl fn_XXXXXXXX`),
   and key on the **`.fn fn_<addr>` symbol** in **uppercase** hex — see §1, where
   a lowercase search produced a clean, decisive, entirely false negative.

---

## 8. Reproducing

```bash
# the two screens (read-only; run against a BUILT tree, or every name is absent)
python3 tools/spatial_map_screen.py --json ~/tmp/fires.json   # W10-A's, unchanged
# the RegisterFactory 2nd-arg screen: see handoff 8; ~40 lines, not committed
python3 tools/icf_alias_finder.py --validate
python3 scripts/verify_objs_patched.py --verify-manifest
```

---

## 9. Gates at the branch tip

```
matched_functions     42,776      (baseline 42,766, +10)
matched_code       3,875,800 B    (+1,508)
matched_code_percent  37.827606   (baseline 37.812890, +0.014716 pp)
fuzzy_match_percent   49.146790   (baseline 49.143230)
masked_equal          22,986      honest 19,790
total_code        10,245,956      total_functions 69,219   (both UNMOVED)
```

The four legs compose to the digit: `528+88+224+668 = 1,508`,
`6+1+1+2 = 10`, and `0.005153+0.000857+0.002187+0.006519 = 0.014716`.
Every leg B was the next leg A, measured in-run; no absolute was inherited.

Gates, verbatim (all four run at the tip, after a full `./tools/ninja-locked`):

```
[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-13T06:21:10Z (tree_sha256=c6f4ac262aeb9e72)
VALIDATE: PASS -- 1352 map-consistent, 241 tolerated (enumerated above), 0 contradicted, 1595 total
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate is required because Changes A, C and D touch `src/`; `skipped=0`
is the rule with the track record, not `PASS` alone.
