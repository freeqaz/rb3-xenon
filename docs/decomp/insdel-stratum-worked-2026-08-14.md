# The `SOURCE_INSDEL` stratum, worked — lane INSDEL-1 (2026-08-14)

Tree `0ceecdbb` + this lane. Shipped ruler `functionRelocDiffs=name_check`, read
from `report.json` `provenance.diff_config` and re-confirmed by driving
`objdiff-cli` with the grader's exact config on every per-row reading.

Baseline (leg A, settled): `matched_functions` **44,407** · `masked_equal`
**22,897** ⇒ honest **21,510** · `matched_code_percent` **36.116707**.

## Rows closed — +3 functions / +1,612 B, prediction pre-registered and EXACT

| measure | predicted | measured |
|---|---:|---:|
| Δ`matched_functions` | +3 | **+3** |
| Δ`matched_code` | +1,612 B | **+1,612 B** |
| Δ`matched_code_percent` | +0.015619 pp | **+0.015621 pp** |

0 regressions · 0 units fell off 100% · unit net +3 == whole-binary +3, in
exactly the three edited units · Native gate **PASS 18/18, 0 SKIPs, rc=0**.

### `?DataReadFile@@YAPAVDataArray@@PBD_N@Z` (480 B, 3 charges → 0)

The open-failure branch carried `if (!node) FinishDataRead();`, which retail does
not have — our 3 surplus instructions were exactly `cmplwi`/`bne`/`bl`. Decisive
retail-byte evidence: the whole body contains **one** `bl ?FinishDataRead@@YAXXZ`
(the else-branch), not two.

⚠ **The oracle cannot adjudicate this and would have said we were right.** DC3
carries the same guard at its own `DataFile.cpp:522`. `src/system` is a verbatim
DC3 copy and DC3 is the *newer* tree, so the source diff is empty **by
construction** — the same trap INSTR-1 hit on `RndParticleSys::UpdateParticles`.

### `??0SongRecord@@QAA@PBVBandSongMetadata@@@Z` (460 B, 3 charges → 0)

Retail calls the **non-virtual** `?Rank@BandSongMetadata@@QBAMVSymbol@@@Z` and
feeds its float return straight into `RankTier`; it never reads `it->second`. We
called the **virtual** `HasPart(Symbol,bool)` *with its return value discarded*
and then loaded `it->second`, costing `li r5,0` (HasPart's bool arg) and
`lfs f1,8(r25)` on top of the mis-paired callee name.

★ **The smell was the discarded return value** — a call whose result is dropped is
not real code. It was landed by an automated near-miss wave (`b2958f2d`), i.e.
metric-fitted. Corroboration before editing: `float Rank(Symbol) const` is
declared non-virtual at `BandSongMetadata.h:45`, which is exactly the `QBA...M`
in retail's mangled callee.

### `??0JoypadController@@QAA@PAVUser@@…` (672 B, 2 charges → 0)

Retail loads the by-value `Symbol` argument out of its frame home
(`lwz r5,0x50(r31)`); we held the sret pointer the call returned (`mr r11,r3` +
`lwz r5,0(r11)`) because the temporary was written inline in the argument list.
Binding it to a named local gives it a fixed home and both charges close.

★★ **This REFINES the class boundary SRCARG-1 drew, rather than contradicting
it.** That lane measured the mirror-image experiment as a **regression**
(`DataNode tmp(...); handled = tmp;`, 99.64 → 93.9) and concluded naming the
temporary does not pay. **The discriminator is the DESTRUCTOR:** a `DataNode`
temp dies at end-of-full-expression, so naming it moves the dtor to end-of-block
(5 deletes → 5 inserts) and costs more than the remat buys; `Symbol` is a trivial
4-byte POD with no dtor, so naming it changes **only** the home. ⇒ the lever is
**live for non-destructible by-value temporaries, dead for destructible ones.**

## Item 2 — the stack-slot sweep, closed out at 0 further rows

SRCARG-1 sized the class at **14 rows / 3,568 B**. Every row is now accounted
for, and **the sweep yields nothing further**:

| rows | bytes | disposition |
|---:|---:|---|
| 2 | 764 | permutation artifacts (Part, KerningTable) — refuted by SRCARG-1 |
| 2 | 1,420 | "make retail-shared slots share" — CharIKHand (drained by MATCH-G), SampleData |
| 2 | 464 | compiler temps for `bs << x`, no named local to scope (RndText, PlayerDiffIcon) |
| 2 | 224 | **NEW: addressable direction, blocked by an INLINING BOUNDARY** (below) |
| 1 | 268 | **NEW: CharBone::StuffBones — non-addressable direction, measured** (below) |
| 2 | 180 | **NOT stack slots at all — `MEMBER(this)` layout defects** (CampaignSongInfoPanel `+8`, OggMap `+44`) |
| 3 | 248 | `MultiTempoTempoMap::PointForTime` (mixed-sign, part artifact) + 2 STLport `_Destroy_Range<T*>` template bodies |

### ★ The addressable direction can be blocked STRUCTURALLY, not just by scope

`MicInputArrow::NewObject` and `ScrollbarDisplay::NewObject` (112 B each, 1
charge) are the FloatKeys class in its **addressable** direction — retail keeps
the discarded `StaticClassName()` Symbol temp at `0x50` and homes the
new-expression pointer at `0x54`; we reuse the dead Symbol's slot. It is
nevertheless **not source-addressable**, because the merged pair **straddles an
inlining boundary**: the Symbol temp is created inside the inlined
`operator new`, the pointer in `NewObject`. "Declare both at one function scope"
has no handle on a pair that never shares a source scope.

Measured, both **byte-identical** to baseline (99.96429, same single charge):
naming the pointer (`T *o = new T; return o;`) and naming **both** (adding
`Symbol cn = StaticClassName();`).

⛔ **Decisive against a liveness reading:** retail's Symbol temp is dead too and
*still* gets its own slot. This is compiler slot-colouring, not source liveness —
so "make the temp live longer" is not a lever to try next.

### The "make retail-shared slots share" direction is now 0-for-3

`CharBone::StuffBones` (268 B, 20 charges) *looks* like the richest instance of
the class — equal frame sizes, three disjoint `if` arms each declaring
`Symbol name` + `CharBones::Bone bone`, uniform single-signed `+8`/`+4` deltas.
It is the **non-addressable** direction: retail **shares** slot `0x54` across all
three arms (`stw r29,0x54(r1)` in every arm) while **we** over-allocate
(`0x54` then `0x50`), which pushes our Bone/Symbol temps uniformly lower.

Hoisting both to function scope in retail's order measured **99.70 → 76.16**,
frame **268 → 304**, charges **20 → 45** — `Bone()` is a user-provided ctor
(`weight(1.0f)`), so hoisting runs it unconditionally at the top. That is the
same hazard SRCARG-1 measured at −5.3 pp on FloatKeys, worse here because the
object is bigger. ⇒ after `SampleData::Load` and `CharIKHand::Load`, that
direction is **0-for-3**. Treat it as codegen.

⚠ Note the targeting trap: the row's *signature* (equal frames, single-signed
uniform delta) is the FloatKeys signature exactly. **The signature does not carry
the direction** — read which side shares before opening the row.

## Negative result: MSVC bool-materialization re-tested, not inherited

`StreakFocusTracker::GetNextFocusPlayer` (692 B, 3 charges) — retail
materialises the inlined `PlayerCanHaveFocus` bool (`cntlzw` + `extrwi.`) where
we fold it into `cmpwi`/`bne`. Lane CN-3e had characterised this exact shape for
the **sibling** `FocusTracker::GetNextFocusPlayer` in an in-source note and
concluded "permuter class (banned)" after three worse alternatives. Since a
screen borrowed from an adjacent vein is only a hypothesis, it was re-tested
here: hoisting to a named `bool canfocus` is **byte-identical** (98.728325 both,
same 3 charges at the same indices). Confirmed permuter class; permuter is OFF.

## Reading of the stratum

`SOURCE_INSDEL` (804 rows / 446,724 B, median 28 charged) is **not** uniformly
28-charge body work. Sorted by charge count it has a long cheap head — 10 rows at
1 charge, 11 at 2, ~15 at 3 — and **that head is where all three closures came
from**. Every one was diagnosable in a single charged-instruction dump and fixed
by 1–3 lines.

⇒ **The stratum is materially more tractable than `SOURCE_ARG` was**, which
inverts the ordering INSTR-1's medians implied (2 vs 28). The reason is the one
SRCARG-1 identified and this lane confirms from the other side: with only 2
charges, the odds that *all* of them are source-shaped are poor and the row is
usually fold-capped, codegen, or a map defect; an insert/delete row's charges are
**structural**, so when they are explicable at all they are explicable *together*.
`matched_code` is all-or-nothing per row, and that all-or-nothing is what makes a
low-charge `SOURCE_INSDEL` row the better bet.

**Still: price by defect signature, not by charge count.** Of the 6 low-charge
rows opened here, 3 closed and 3 were characterised as codegen/structural —
and the 3 that closed were the ones whose charges named a *source construct*
(a surplus guard, a wrong callee, an unnamed temporary), not a *codegen artifact*
(bool materialisation, slot colouring across an inline boundary).

## Tooling

`/home/free/tmp/insdel1/show.py` (per-row charged-instruction dump at the shipped
ruler; ported from SRCARG-1 with `WT` repointed). The per-row findings are also
recorded as **in-source notes** at each deferred row — `FocusTracker.cpp`,
`CharBone.cpp`, `MicInputArrow.h` — which is the placement that has actually
stopped lanes re-hunting drained veins.
