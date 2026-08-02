# Frontier re-census — where the next 100+ actually is (lane BV-4, 2026-07-30)

> **LANDED AS +152, NOT +176 — read this before quoting the numbers below.**
> The landing lane re-measured on current main (post BV-3 + BV-1) and shipped
> **152 of the 177 rows: +152 matched / +152 honest / +0.157085 pp code / 0
> regressions / same-split**. The other **25 rows were dropped as sibling/twin
> mispairs**, not for metric reasons.
>
> Why: those 25 map a VA to a mangled name that a *different* VA in the map
> already claims. Cross-unit placement makes them harmless to obj symbol tables,
> but disassembly shows each pair is **structurally identical and semantically
> different** — same instructions, same registers, same frame, differing only in
> an absolute address or a single `bl` target. `??1ObjRefOwner@@UAA@XZ` stores a
> different **vtable** pointer (`0x8203231c` vs `0x821113b4`); `??0Symbol@@QAA@XZ`
> a different string constant; `?ClassName@MsgSource@@UBA?AVSymbol@@XZ` differs
> only in the `bl` to its own `StaticClassName`; `?JoypadUnsubscribe@@YAX...@Z`
> reads a different global (`0xb2bc` vs `0xb29c`). One pair even differs in size
> (`?SetType@CharPollGroup@@UAAXVSymbol@@@Z`, 316B vs 252B — the 316B
> OBJ_SET_TYPE fingerprint). Taking them would have been reloc-masked false
> credit of exactly the class this campaign has been retiring.
>
> **This implies the lane's "byte-perfect" selector is reloc-masked.** It scored
> twins as perfect. The other 152 have no competing claimant so nothing is proven
> against them (and all 152 land at 100% with `masked_equal` unchanged), but the
> selector should resolve relocations before it is reused. Two traps confirmed in
> passing: (a) this lane's commit `json.dump`-rewrote
> `scripts/target_symbol_map.json`, flipping 1-space to 2-space indent and
> exploding the four compact metadata arrays — the 28,769/27,051 line count for
> 177 rows is the tell that the appliers were bypassed; (b) a raw byte-compare of
> two VAs **inverts** the verdict — PC-relative `bl` displacements must be
> resolved to absolutes first, per `project_map_defect_channels_2026-07-29.md`.
>
> ➡ Open follow-on: for each of the 25, the *pre-existing* row may be the mispair
> rather than the new one. Adjudicating that needs a twin discriminator plus a map
> DELETE (which is a silent no-op without a re-split). Dropped list:
> `/home/free/tmp/bv4land/dropped25.json`.

Baseline, measured in a clean `laneBV4` worktree with `report.cache`/`report.json`
removed and a full rebuild:

```
matched_functions      40953
masked_equal_functions  1518
honest (m - masked)    39435
matched_code_percent   34.513645
total_code          10580036      total_functions 69367
```

Main's on-disk `report.json` read `40925 / 1517 / 34.479504` at lane start — stale
by 28 functions because sibling lanes were mid-flight. **Any obj-derived scan run
against main's build dir today is contaminated; regenerate in your own worktree
first.** The rebuild recovered exactly the 28.

---

## TL;DR

**Yes, there is a 100+ vein, and it is not body-porting.** It is the
**anonymous-target naming gate**: 6,260 real function bodies inside already-pinned,
already-supplied units score exactly `0.0` because objdiff structurally cannot pair
an anonymous target symbol to a *named* base symbol. **+176 honest / +0.185368 pp
is already measured and verified in this lane** (whole-binary A/B, zero regressions,
`masked_equal` unchanged), with a further ~284 gated on a discriminator and ~1,084
functions convertible from invisible-`0.0` into the workable near-miss frontier.

Everything else is 10–25 per lane. The historical record agrees: every 100+ landing
this project has ever had came from **attribution/pinning**, never from body work.

| # | vein | paired pop | Δhonest | Δcode | confidence |
|---|---|--:|--:|--:|---|
| 1 | Anonymous-target naming — uncontested byte-perfect | 177 | **+176 MEASURED** | **+0.1854 MEASURED** | **certain (landed in-lane)** |
| 2 | Anonymous-target naming — contested byte-perfect (needs reloc discriminator) | ~284 | +150…+284 | +0.10…+0.19 | medium-high |
| 3 | Naming as *conversion*: expose 1,084 ≥95%-similar anon bodies as near-misses | 1,084 | +0 direct | +0 direct | high (enabler, not yield) |
| 4 | One-row named residue (`nd` ≤ 160) | 569 | +90…+145 | +0.20…+0.28 | medium |
| 5 | Locate + pin + port unwired oracle TUs | ~340 game / ~1,092 engine | +100…+200 over many lanes | — | **low** (locating is the drained step) |
| 6 | `BranchDest` at-100% defects | 24 | +0 | +0.057 | high, but small |
| 7 | Mid-band body port (0–99% named) | 1,759 | +8…+23 **per lane** | +0.03…+0.09 | high — **not a wave** |

---

## 1. Population decomposition (how it was counted)

Computed directly from `report.json`, not from any shared filter.
⚠ `scripts/live_units.py` joins on **basename** and live basenames genuinely collide
— measured here: `Utl` ×3, `Rnd` ×2, `Dir` ×2, `Synth` ×2, `Movie` ×2, `FxSend*` ×2.
⚠ `splits.txt` uses bare basenames, `objects.json` uses paths.

| | code bytes | % | functions | % |
|---|--:|--:|--:|--:|
| total | 10,580,036 | 100 | 69,367 | 100 |
| **auto-carve, no `base_path`** | 4,520,516 | **42.73** | 17,963 | **25.90** |
| suppliable | 6,059,520 | 57.27 | 51,404 | 74.10 |

**42.73% of the code denominator can never score** under the current unit layout —
2,964 auto units, every one with `base_path: null`, 0 matched. So
`matched_code_percent` has a hard ceiling of **57.27%**, and we are at 34.51 of it.

Within the suppliable population: **79.67% matched, 76.72% honest, 60.26% code.**
Headroom = 11,969 functions, of which 1,518 are leverless masked funclets ⇒
**10,451 genuinely addressable functions.**

Splitting those 10,451:

| | fns | bytes | anon | named |
|---|--:|--:|--:|--:|
| score exactly 0.0 | 6,536 | 1,336,420 | 6,260 (95%) | 276 |
| partial (0 < n < 100) | 3,909 | 987,068 | 1,830 | 2,079 |

The asymmetry is the whole story: the zero pool is **95% anonymous**, the partial
pool is **54% named**.

### Only 2 non-auto units lack a `base_path`

`default/Rnd` (16 B, 1 fn) and `.../SessionDiscoveryTable` (640 B, 5 fns). `default/Rnd`
is the `Rnd` basename collision; `configure.py` prints `Missing configuration for
Rnd.cpp`. Cosmetic, not a vein.

---

## 2. Instruments and schema corrections

Three schema facts that change how anything here must be priced. All read from the
objdiff fork source, not inferred.

**(a) `matched_functions` and `matched_code` key off *different* fields.**
`report.rs` credits `matched_functions` on `match_percent_normalized == 100` but
`matched_code` on the raw `fuzzy_match_percent == 100`. The two axes genuinely move
independently — which is exactly why the amended pricing rule exists.

**(b) `match_percent_normalized` = `match_percent` − `arg_diff_score`**
(`objdiff-core/src/diff/code.rs:285-291`, comment: *"excludes arg-only penalties
(register swaps, offset swaps)"*). **Register allocation is therefore normalized
away before the honest metric ever sees it.** Consequence, and it inverts a premise
this lane started with: *the permuter ban does not block Δhonest at all.* It only
caps Δcode%.

**(c) The true reloc-masked defect class is NOT countable from `report.json`.**
`report generate` hardcodes `function_reloc_diffs=None` globally, so relocation
differences are masked in **both** percent fields. I initially reported a
"213-function reloc-masked class" — **that was wrong and is retracted**; those 213
are the *arg-diff*-masked class (below). The reloc class remains unquantified.

### Exact score inversion (validated to the integer)

```
norm_diff  = 100·(insert+delete) + 60·replace + 1·(immediate diffs)
fuzzy_diff = norm_diff + 5·(register swaps)
norm_diff  = N·(100 − pct),   N = size/4
argd       = fuzzy_diff − norm_diff
```

Validated on ground truth: `Game::Handle` 1 replace + 1 insert + 2 delete =
60+100+200 = its `nd` of exactly 360; `OvershellSlot::Handle` 4 inserts = exactly
400; `RockCentral` 27 replaces + 2 offset shifts = exactly 1622; `MetaPerformer`
20 register swaps × 5 = its `argd` of exactly 100. Across all 69,367 functions,
**zero `argd < 0` anomalies.**

This lets any lane compute an exact defect decomposition for every function in the
tree **straight from `report.json`, with no diffing at all** — and it fixes the
long-standing blind spot that scanners sort by *percentage* rather than *penalty*.

---

## 3. Re-derived honest floor (supersedes 37,490–38,098)

> ⚠ **RULER-ANNOTATED 2026-08-02 (lane DA-4) — historical, left intact.** This
> section's table is an old-ruler derivation: its `masked_equal` row is explicitly
> labelled *"funclet pass-2b oversubscription"*, which is exactly what changed. Since
> 2026-08-02 `masked_equal_functions` discloses **all** funclet byte-signature
> pairings (**1,096 → 22,640**), so the same subtraction now yields **20,814**, not
> 39,435 — at an unchanged tree, with **no score key moved**. The section's *method*
> (headline → honest → byte-identical floor) is still the right shape; only the
> middle row's population changed, and it is now a **broader, mostly supply-backed**
> set rather than an unsupported-credit set, so "over-states by 4.22%" cannot simply
> be recomputed against it. ⛔ Do **not** read 20,814 as this floor collapsing.
> See [`docs/decomp/RULER_CHANGE_2026-08-02.md`](../RULER_CHANGE_2026-08-02.md).

| definition | count |
|---|--:|
| headline `matched_functions` (norm == 100) | 40,953 |
| − `masked_equal` (funclet pass-2b oversubscription) → **honest** | **39,435** |
| byte-identical (`fuzzy == 100`, non-masked) → **floor** | **39,223** |
| − previously-asserted ~109 at-100%-with-wrong-constants | ~39,114 |

**Headline over-states the byte-identical floor by 1,730 = 4.22%**, materially better
than the historical 6.0–7.5% claim. Retire the 37,490–38,098 range. The residual
reloc-masked class is a further unquantified deduction.

`masked_equal` is narrow and clean: all 1,518 are anonymous, 1,098 at 32 B and 361
at 40 B, none in auto units — pure EH funclets.

---

## 4. Vein 1–3: the anonymous-target naming gate ★ the 100+

### Mechanism (read from source, not inferred)

`objdiff-core/src/diff/mod.rs:1410 pair_funclets_by_bytes` is the **only** path by
which an anonymous target symbol can pair. It requires `is_funclet_like` on **both**
sides, and base-side funclet-like means only `__unwind$N`, `__catch$N`, `??__E`,
`??__F`, `fn_XXXXXXXX`. **A base symbol carrying a normal MSVC mangled name is never
a candidate.** So an anonymous target *body* whose counterpart in our object has a
proper mangled name can never pair, at any similarity. Pass 3 further demands exact
size equality, so there is no partial credit — it is binary.

The empirical signature matches exactly: anon functions ≤48 B match 92%, 48–100 B
35%, and **4 of 3,397 above 100 B**. No anon function above 76 B ever scores.
Named functions of identical sizes match fine.

### Supply split of all 6,260 (masked-signature reimplementation vs all base symbols)

| class | fns | bytes | share of mass |
|---|--:|--:|--:|
| A byte-perfect partner exists | 628 | 52,664 | 4% |
| B same-size, ≥50% masked equality | 1,636 | 129,916 | 10% |
| C same-size, <50% | 2,187 | 312,012 | 24% |
| D no same-size candidate at all | 1,809 | 793,180 | **61%** |

623 of 628 class-A partners carry mangled non-funclet names — confirming the
mechanism. **It is a naming gate mechanically, a supply gate economically:** 85% of
the mass (C+D) has no byte-level counterpart and is genuinely dead to this lever.

### Vein 1 — MEASURED AND VERIFIED IN THIS LANE

177 uncontested, uniquely-named byte-perfect entries were merged into
`scripts/target_symbol_map.json` (27,050 → 27,227; **0 address collisions**), the
split was re-forced via `touch config/45410914/config.yml`, and the whole binary was
rebuilt:

| metric | baseline | after | Δ |
|---|--:|--:|--:|
| `matched_functions` | 40,953 | 41,129 | **+176** |
| `masked_equal_functions` | 1,518 | 1,518 | **+0** |
| **honest** | 39,435 | **39,611** | **+176** |
| `matched_code_percent` | 34.513645 | 34.699013 | **+0.185368 pp** |

Per-function regression check against the pre-change report: **0 functions lost from
100**, 177 symbols renamed across 93 units, 176 reached exact 100. `masked_equal`
unchanged is the decisive check — the gain is **supply-backed, not funclet
over-subscription**. Passes both axes of the amended pricing rule.

The change is committed on branch `laneBV4` for the coordinator to land.

### Vein 2 — the contested byte-perfect remainder, and why `reloc_disc` is NOT drained

The byte-perfect ceiling is 461 functions (+0.377 pp); 177 are uniquely attributable,
leaving **~284 contested** — a base symbol byte-identical to several target VAs (one
`?Load@UIComponent@@$4...` was byte-identical to 19 targets). Banking those blind is
precisely the map-mispair correctness risk.

**The discriminator already exists and has been mis-aimed.**
`scripts/harvest/reloc_disc/` measured **99.41% precision** at its ship gate — but
that figure is *conditional on truth-present*, because `heldout_reloc.py` only admits
a VA whose true name exists as a code symbol in that unit's base obj. Lane BU-4
correctly refused to inherit it for the LIVE pool (288 unmapped VAs), because there
truth may be absent entirely — `livecontrol.py`'s truth-ablation control exists
exactly to prove that.

**The contested byte-perfect pool satisfies truth-present by construction**: we found
the byte-perfect partner *in our own supply*, so the correct answer is guaranteed to
be among the candidates. That is the calibrated population, not the refused one.
The vein was declared drained on the strength of a pool whose supply the tool was
never calibrated for; pointed at this pool, its measured precision should apply.

### Vein 3 — naming as *conversion* (0 direct yield, large enabling value)

| similarity to best partner | fns | bytes |
|---|--:|--:|
| ≥0.95 | **1,084** | 104,260 |
| 0.50–0.95 | 1,180 | 78,320 |
| <0.50 (greenfield) | 3,996 | 1,105,192 |

**1,084 functions sit at ≥95% similarity while reporting `0.0`** — one- and
two-instruction misses that are invisible to every near-miss scanner in the tree
because they are anonymous. Spot-checks after naming: `Anim` 99.96%, `BandList`
99.97%, `BandCharacter::SyncProperty` (1,560 B) 99.69%, and
**`?Handle@Rnd@@UAA?AVDataNode@@PAVDataArray@@_N@Z`, 6,416 bytes, at 99.71% —
currently scoring 0.0.**

Naming these yields ~0 honest immediately. Its value is that it **refills the
near-miss frontier** that veins 4 and 7 are otherwise running out of.

---

## 5. Vein 4 — the one-row named residue (`nd` ≤ 160)

Defined by *defect magnitude*, not percentage: **569 named functions in supplied
units whose entire normalized gap is at most one structural row** — 91,064 B.
Ceiling +569 honest / +0.8607 pp; the `argd == 0` subset that pays *both* axes is
458 fns / +0.5547 pp.

Composition: `nd`=1 → 133 fns (one wrong immediate), `nd`=60 → 145 (exactly one
replace), plus 128 at `nd` 100–159 (exactly one insert/delete).

**Overlap with the cohorts drained this week is 2 functions** (99.8481% SetType/vbase)
— this pool is essentially fresh.

Cause sample (n=25, stratified): 22 live / 2 dead / 1 ambiguous, **zero pure-regalloc**
— because regalloc is normalized out before the pool is entered. Buckets: wrong
`sizeof(T)` 7, wrong member offset 5, vbase displacement 2, wrong literal 2, real
logic divergence 6, frame-slot shuffle 2 (dead).

**Cost driver: diffuseness.** 569 functions across **316 distinct units, median 1 per
unit**. Only 155 functions (30 units, 4–11 each, +0.2209 pp) are batchable; 414 are
one-per-unit and expensive.

Realistic **+90…+145 honest / +0.20…+0.28 pp**. The *landing fraction* is a judgement,
not a measurement, and dominates the error bar (±2×). Only empirical anchor: lane
BQ-2 got +8 verified from 3 layout fixes (~2.7 net functions per fix).

Five defects already precisely located: `Game::Handle` (extra literal argument at one
call site, idx 76–83); `Player::LocalSetEnabledState` (target `srawi. r11,r11,3` vs our
`clrrwi.` — signed divide vs mask); `SongParser` ctor (idx 391, `mr r4,r3` vs
`addi r4,r31,0x70`); `InputMgr::Handle` (one insert/delete); `PassiveMessageQueue`.

---

## 6. Vein 6 — the arg-diff-masked at-100% class (213 functions)

Functions with `match_percent_normalized == 100` but `fuzzy_match_percent < 100`:
exactly **213**, 83,820 B. Counted in `matched_functions`, excluded from
`matched_code`. (`fuzzy==100 & norm<100` = 0, so normalized strictly contains raw.)

| differing arg type | fns | bytes | pp | verdict |
|---|--:|--:|--:|---|
| Register only | 144 | 63,180 | 0.597 | **DEAD** |
| `BranchDest` | 24 | 6,032 | 0.057 | **LIVE — real wrong control flow** |
| Register+Symbol | 19 | 12,372 | 0.117 | mostly dead |
| shift-amount / other | 24 | 1,376 | 0.013 | see §7 |
| Register+Signed+Symbol | 2 | 860 | 0.008 | dead |

**The 24 `BranchDest` cases are a genuine correctness finding:** each is credited as
a matched function while branching to a *different instruction*. Verified two by
hand — `RndTexRenderer::SyncProperty` target branches to +0x210, ours to +0x340;
`BandMatchmaker::CancelFindImpl` target to +0x0, ours to +0x60 — in functions of
identical size with otherwise identical instruction sequences. Every one has exactly
**one** divergent branch, which makes them tractable (inverted condition / misplaced
early-out). Worth +0 honest, +0.057 pp, and real correctness value.

### Killed in this lane: the 79-function commutative-swap sub-vein

The largest single item looked outstanding — `NextSongPanel::CountOrCreateExpandedDetails`,
12,220 B at raw 99.993%, differing in exactly two `add r3,r11,r28` vs `add r3,r28,r11`
operand-order swaps, i.e. +0.1155 pp for one source edit. 79 such functions,
42,796 B, +0.4045 pp.

**It is dead.** `docs/decomp/patterns/unfixable-compiler.md:346-350` records a Wave-1
AT_LIMIT audit: for >99% of `COMMUTATIVE_OP_ORDER` mismatches, source-level operand
reordering produced a **byte-identical `.obj` across dozens of attempts**. MSVC picks
operand registers from liveness and the register allocator's colour map, not AST
order — *"the swap is a regalloc artifact, not an expression-evaluator artifact."*

⚠ **Live documentation defect:** `docs/decomp/patterns/fixable-operators.md:256` still
advertises this rewrite with an **"80% success rate"**, directly contradicted by the
later audit. It cost this lane time and will cost the next lane more.

---

## 7. Explicitly down-ranked, with reasons

**Mid-band body port (0–99% named, 1,759 genuinely paired / 758,796 B) — not a wave.**
Seven prior body-port waves plus `deepen*`, `grind-execute*`, `engine-easy-wins`,
`permuter-sweep` covered 11 of the top 14 residue units by byte mass, several as
*confirmed walls*: `rndobj/Utl` at-limit; `VocalPlayer` vbase wall (sized-vector theory
refuted, tree-wide −504); `BandDirector` on the DROP list with `est_recoverable 0`;
`CameraShot` vbase wall; `VocalTrackDir` tail proven **mis-pins, not body-ports**.
There are 14 explicit "drained" statements in the docs; a 151-function permuter sweep
produced **one** committable win. Clean-cause rate 3/19 = 16%, Wilson CI [5.5%, 37%].
Expect **+8…+23 per lane**. Oracle coverage is *not* the gate (99.9% of residue has an
oracle file); 264 functions are TARGET_ONLY (our TU never emits them), ~103 of those
being over-carve/map-mispairs that belong to an attribution lane.

**STL `sizeof(T)` / element-stride oracle — EXHAUSTIVELY REFUTED this lane. Defund.**

The lead looked strong: shift-amount mismatches (`srawi r11,r11,6` vs `,3`) really do
read out `log2(sizeof(T))`, and the family failure rates are anomalous
(`_M_insert_overflow_aux` 37.1% vs a 10.8% named baseline). It was scanned
exhaustively — 3,981 candidates → 3,453 with `diff_arg > 0` diffed individually →
320 with shift-op arg diffs → 71 sizeof-shaped → 49 parseable to an element type
(~40 distinct types).

**A control-group discriminator settles it.** For each type, count sibling functions
that provably encode `sizeof(T)` (same `vector<T>` instantiation, or algorithms over
`T*`). **41 of 49 records have ≥1 sibling matching retail at exact 100% using OUR
size** — retail byte-confirms our `sizeof`, so the shift difference cannot be a size
error. Three independent channels agree:

1. **Target self-contradiction** (map-independent): the same type implies two different
   retail sizes while our side stays consistent — FlowMathOp (12 *and* 40),
   ArchiveSkeleton (12 *and* 28), MoveRating (24 *and* 44), SongPattern (12 *and* 48).
2. **Language-forced sizes, 6/6 against the oracle**: `pair<RndTexBlendController*,float>`
   is 8, not the implied 64; `unsigned short` is 2, not 4; `MoveParent const*` is 4,
   not 2; an enum is 4, not 8. Wherever ground truth is independently knowable, **our
   size is right and the retail-implied size is the artifact.**
3. **Type-specific non-STL corroboration**: `LightPreset::SpotlightDrawerEntry` has
   `Save`, `Load`, `operator!=`, an 876-byte `PropSync`, and a vector-indexing PropSync
   **all at 100.000%** with our 16-byte layout.

Cause is ICF: these tiny growth-path COMDATs fold across every type in a size class,
so the name our map attaches to the surviving body is arbitrary among the folded set.
This independently reproduces
`docs/decomp/research/2026-07-20-stl-element-stride-ground-truth.md` (fill-family
strides LIE via ICF folding; `scripts/truth_table.py` is "a SCREEN, not truth"; a blind
`DataArrayPtr` 4→12 pad measured **−211 matched_functions**; `Transform` 64→120 is
localized — do not touch `math/Mtx.h`). It confirms two of that doc's recommendations
were landed (SongPattern now 24, LocalizedName now 84) and that the residual witnesses
on those types are exactly the fold artifacts it predicted.

**One real defect survives: `FlowMathOp`, ours 52 (`0x34`), retail 12 (`0xc`)** — held by
the gold-standard evidence class, `__uninitialized_copy@PAVFlowMathOp` (a type-specific
copy loop, which folds rarely) stepping both src and dst by `0xc` vs our `0x34`;
`class_layout_report.py` confirms our `sizeof = 52`. Already known, not landed.
Realistic paired yield **≈5 fns / 760 B / +0.0072 pp**, against a demonstrated
−211-scale downside — 52→12 is a type-identity reconstruction, not a pad (our layout
carries `DataNode mLhs/mRhs` plus a 24-byte `FlowPtr` that 12 bytes cannot hold).
If a lane insists: reconstruct the true 12-byte layout from retail member-access
offsets in `DrivenPropertyEntry`/`FlowNode` non-template code *first*, and gate on
`__uninitialized_copy@PAVFlowMathOp`'s `addi 0xc`. Do not lead with an A/B of a resize.

⚠ **Tooling trap found while doing this:** `objdiff-cli diff --batch` **hardcodes
`instructions: None`** (`objdiff-cli/src/cmd/diff.rs:1674`) — `--include-instructions`
is *silently ignored* in batch mode. Batch is usable as a cheap pre-filter (it does
emit `instruction_summary`), but real instruction rows must come from single-symbol
mode. Also: `fuzzy_match_percent` is **omitted from `report.json` when it is 0.0**, so
a naive read counts 6,677 unpaired 0% functions as "missing".

**Adjustor/vbase thunks — drained, confirmed by control group.** 37 residual functions
/ 1,404 B / +0.013 pp against **1,957 already at 100% — a 1.9% failure rate** versus a
10.8% named baseline. BQ-2→BS-1→BU-1 closed it.

**Auto-carve / new-TU pinning — the historical 100+ recipe, but the locating step is
the drained one.** Unclaimed `.text` is *not* fragmented: 744 `auto_*_text` spans hold
4,455,224 B / 17,822 fns, and **13 spans ≥64 KB hold 3,525,184 B / 10,105 fns** (79% of
the mass), the largest being 1.23 MB / 2,574 fns. Historically this is the only lever
family that ever produced 100+: laneBL **+559** (31 TUs), wave BO **+190**, laneBO2
**+135**, laneBP1 **+100**, laneBO4 **+99**, laneBD **+71** — versus a body-port
high-water mark of **+23**. But: ~38.5% of unmatched bytes are **vendor** (5,949 fns,
0 pinned, hard-skip per standing directive); game code is already 77% pinned; and the
remaining oracle TUs are small and mostly picked over — ~60 real unwired rb3-Wii game
TUs ≈ **340 functions** (after excluding three `.permuter_work_*_VocalPlayer.cpp`
scratch artifacts worth 348 phantom functions, and 93 Wii/RV-only network functions),
plus ~1,092 non-`lazer` dc3 engine functions. Prior measurement put the auto-carve
ceiling at **+25…+85**. Real, but the bottleneck is identification, and that is the
drained step.

---

## 8. Recommended sequencing

1. **Land the verified 177-entry map fragment** (branch `laneBV4`) — +176 honest /
   +0.185 pp, already measured, 0 regressions.
   ⚠ Landing note: this was measured against `4045aea5`. Main advanced to `e406eef4`
   (lane BV-3, 40,957 / 39,439 / 34.518597) during the lane. BV-3 touched
   `scripts/target_symbol_map.json` (4 lines) but **0 of my 177 addresses collide with
   current main**, so it should compose — re-A/B on current main to confirm rather
   than assuming additivity. `splits.txt` was byte-identical across my re-split, so the
   delta carries no split churn.
2. **Aim `reloc_disc` at the ~284 contested byte-perfect candidates**, where its
   truth-present precondition actually holds. Re-run its own truth-ablation control on
   *this* population first — do not inherit the 99.41% unexamined.
3. **Name the 1,084 ≥95%-similar anon bodies** to refill the near-miss frontier. 0
   direct yield; it is what keeps veins 4 and 7 fed.
4. **Work the 155 batchable one-row fixes across 30 units** (+0.2209 pp), leading with
   the 12 `argd == 0` structural functions ≥900 B (21,284 B = 0.2012 pp) that pay both axes.
5. Opportunistically, the **24 `BranchDest`** correctness defects.

---

## 9. What this lane did NOT do

No work on: the auto-carve `base_path` resolution or the 32 asserted map defects
(sibling lanes); `StoreOfferProvider`. I did not attempt any of the 569 one-row fixes
— the yield band for vein 4 is a judgement, not a measurement, and I say so. I did not
diagnose the 264 TARGET_ONLY functions. The reloc-masked defect class remains
**unquantified** and unquantifiable from `report.json`. The `reloc_disc`-on-contested
claim (vein 2) is an argument from the tool's calibration precondition — it is
**untested** on that population, and it is the ranked item I am least sure of.

Scratch (regenerable): `~/tmp/laneBV4/` — `nd_all.json` (exact `nd`/`argd` for all
69,367), `verdict.json` (6,260 anon classifications), `map_fragment_177.json`,
`cls213.json`, `argtype213.json`.
