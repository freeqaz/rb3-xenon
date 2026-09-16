# W16-EE — the `_bijection_arbitrary` sub-100 vein, worked to exhaustion

**Date:** 2026-09-16 · **Branch:** `w16-ee` off `af6b35a6` · **Worktree:** `~/tmp/wt-w16-ee`
**Ruler:** every figure is the **graded `name_check`** ruler, read from
`build/45410914/report.json` (provenance `functionRelocDiffs=name_check`,
`ppc.calculatePoolRelocations=false`, objdiff 4.2.9, binary hash `5a51cd51fe0a353f`,
commit `a5f0ea903ec1`) after a full `./tools/ninja-locked`, or from `tools/ab_measure.py`'s
archived leg reports. No figure is inherited from a prior lane without re-measurement.

This lane found **one** fix and spent most of its effort establishing that the rest of the
vein is empty. The negative result is the larger half of the deliverable, and §3 is where
it lives. Lane MPNGAP-1's verdict — *"~91% of the `diff_arg`-only stratum is irreducible
fold/map noise"* — **stands and is corroborated here**, not challenged.

## 0. Net result

| measure | leg A (`af6b35a6`) | leg B (W16-EE) | delta |
|---|---:|---:|---:|
| `matched_functions` | 43,950 | **43,952** | **+2** |
| `matched_code` | 4,122,600 B | **4,122,880 B** | **+280 B** |
| `matched_code_percent` | 40.232000 | **40.234730** | **+0.002730 pp** |
| `fuzzy_match_percent` | 49.998850 | 49.998860 | +0.000010 pp |
| `masked_equal_functions` | 23,224 | 23,224 | +0 |
| honest (`matched − masked_equal`) | 20,726 | **20,728** | **+2** |

Denominator read from the same reports, never hardcoded: `total_code` = 10,247,068 B,
`total_functions` = 69,240 (identical on both legs).

**Units:** `default/TrackPanelDir` 174 → 176. **Every other unit unchanged**; unit-net +2
equals whole-binary Δmatched +2. Units at 100%: `mpn` 189 → 189, all-rows-fuzzy 169 → 169
— **0 reached 100, 0 fell off**. Zero regressions, zero rows lost anywhere in the binary.

Both gained matches are **honest** (`Δmasked_equal = +0`), i.e. not funclet disclosure.

**Rows that moved** (all of them):

| unit | row | size | fuzzy |
|---|---|---:|---|
| `default/TrackPanelDir` | `?UnisonStart@TrackPanelDir@@UAAXH@Z` | 140 B | 99.85714 → **100** |
| `default/TrackPanelDir` | `?UnisonEnd@TrackPanelDir@@UAAXXZ` | 140 B | 99.85714 → **100** |

**Provenance:** `tools/ab_measure.py --worktree ~/tmp/wt-w16-ee --from-dirty`, run dir
`.ab_measure_runs/20260916-023155-from-dirty-3820337`, patch sha256 `a95c856e31ea772a`,
kinds `['map']`. Both legs settled to a zero-work build and **both reached a `symbols.txt`
split fixed point** (`1e8375f9 -> 1e8375f9`, 0 extra re-splits each), so the ABSPLIT-1
under-report does not apply. Leg B forced a re-split with **`renamer_patched=1830`**, so the
map edit was **not inert** and the reading is not absent-vs-absent. `legA_recompiles=0`,
`legB_recompiles=0` — correct for a map-only patch, which changes no TU.

## 1. The fix: `BandTrack::UnisonStart` / `UnisonEnd` were transposed (map, 2 rows)

| VA | was | is |
|---|---|---|
| `0x8234dd18` | `?UnisonEnd@BandTrack@@QAAXXZ` | **`?UnisonStart@BandTrack@@QAAXXZ`** |
| `0x8234dd30` | `?UnisonStart@BandTrack@@QAAXXZ` | **`?UnisonEnd@BandTrack@@QAAXXZ`** |

Both VAs were flagged `_bijection_arbitrary` — **the map itself declared which name
belonged on which VA to be unestablished.** The two bodies are byte-identical apart from a
single relocation, which is exactly why a bijection had to pick, and why it could pick wrong.

### The victims are the callers, not the mis-named rows

The charge does not appear on the BandTrack rows at all — both read **100.0** in leg A. It
appears on their two callers in `TrackPanelDir`, each charged at the *same* instruction
index 24, in mirror image:

```
?UnisonStart@TrackPanelDir@@UAAXH@Z  [24]  TGT ?UnisonEnd@BandTrack@@QAAXXZ
                                            OUR ?UnisonStart@BandTrack@@QAAXXZ
?UnisonEnd@TrackPanelDir@@UAAXXZ     [24]  TGT ?UnisonStart@BandTrack@@QAAXXZ
                                            OUR ?UnisonEnd@BandTrack@@QAAXXZ
```

This is the doctrine's *"a wrong name is FINANCED BY ITS CALLERS"* observed directly: the
defect is at one address and the bytes are withheld at two others.

### Evidence route 1 — an asymmetric anchor one level up

The TrackPanelDir names are **correct**, and that is what fixes the orientation. Each body
tail-calls a different `EndingBonus` method, and *those* are not bijection-flagged:

- `0x822d4ab8` = `?UnisonStart@EndingBonus@@QAAXH@Z` — its body does `mr r30, r4`, so it
  genuinely consumes the `int`. The `(int)` overload is `UnisonStart`.
- `0x822d39e8` = `?UnisonEnd@EndingBonus@@QAAXXZ` — never reads `r4`, and **clears** the
  `+0x1dd` active flag that `UnisonStart` tests-and-sets. Start sets, End clears.

`fn_82305B00` calls the former ⇒ it is `TrackPanelDir::UnisonStart`; it inner-calls
`fn_8234DD18` ⇒ **`0x8234DD18` is `BandTrack::UnisonStart`.** The map said `UnisonEnd`.

⚠ **A discriminator I tried first and had to discard as VACUOUS.** `fn_82305B00` never
touches `r4`, which looked like proof it takes no `int` argument. It is not: `r4` is the
incoming `int i` being *passed through* to `EndingBonus::UnisonStart(i)`. **Both siblings
leave `r4` alone**, so the test cannot separate them in either direction. Recorded because
it is shaped exactly like a decisive negative and would have sent the swap the wrong way.

### Evidence route 2 — member ordering, verified by the COMPILER

`fn_8234DD18` tail-jumps to `fn_822D2BB8`, which loads member `+0x1E0`; `fn_8234DD30` jumps
to `fn_822D2BD0`, which loads `+0x1E4`. Our source has
`UnisonIcon::UnisonStart() { mStartTrig->Trigger(); }` and `UnisonEnd() { mEndTrig->Trigger(); }`.
Per `scripts/harvest/class_layout_report.py` — the **compiler**, not the `// 0xHEX`
comments, which CLAUDE.md warns are derived and have been measured wrong:

```
=== UnisonIcon   sizeof = 564 (0x234) ===
  0x1e0   480    mStartTrig
  0x1e4   484    mEndTrig
```

So `fn_822D2BB8` is `UnisonIcon::UnisonStart`, and the function that jumps to it —
`fn_8234DD18` — is `BandTrack::UnisonStart`. **Two independent routes, one verdict.**

### Why the collateral is structurally zero

- `0x822d2bb8` / `0x822d2bd0` are **unnamed in the map**, so they remain `fn_` placeholders,
  and `name_check` **forgives placeholder targets**. That is why both BandTrack rows read
  100 *while carrying swapped names* — and why swapping them cannot move those rows in
  either direction. Confirmed in both legs: all four rows read 100 in leg B.
- **No other retail call site references either VA** — only `TrackPanelDir.s` lines
  3961 / 4004 in the entire asm tree.
- **No alias group covers any Unison symbol** (checked by exact name, not substring).

Safety: `build/45410914/src/system/bandobj/BandTrack.obj` defines **both** mangled names, so
both renamed rows can pair — clearing the *"proving a name wrong does not make renaming
safe"* trap. Map injectivity is preserved (a swap adds no name); the 2 pre-existing duplicate
values are unrelated and involve no Unison symbol.

## 2. The instrument: a mechanical 2-cycle detector

Rather than eyeball same-shaped siblings, I ran a detector over **all 54** census rows:
row A is a transposition of row B iff A's charged **target** name equals B's **our-side**
name *and* B's target equals A's our-name. Over the whole census it returns **exactly two**
2-cycles — this one and `PropSync`. Everything else that *looks* like a sibling pair is not
one, and the detector says so without my having to adjudicate each by hand.

The complementary signature is just as useful: charged **target** names that appear on more
than one census row are **shared fold survivors**, and a swap can never clear them.

## 3. Refuted / examined and deliberately NOT changed

- **`PropSync` (+640 B nominal) — EC's zero pricing CONFIRMED on my own measurement.** It
  *is* a true 2-cycle (the detector finds it), but both halves additionally charge
  `?_M_fill_insert@?$vector@PAVObject@Hmx@@...` — the **same** target on both sides, a shared
  fold survivor. A swap takes each row 3 charges → 1 and buys **0 bytes**. Not attempted.
- **`PropKeys::Copy` — EC's VERDICT HOLDS but its stated MECHANISM does not.** EC carried
  this as *"same shape, same zero pricing"* by analogy to PropSync's `_M_fill_insert_aux`
  story. Measured, that is not what is happening: `?Copy@BoolKeys@@` and `?Copy@SymbolKeys@@`
  **both** charge `??$_M_range_insert@PBV?$Key@M@@@...` — the identical `Key<float>`
  survivor — so they are **not a 2-cycle at all**, and the detector correctly declines to
  report one. Same conclusion (0 bytes), different and better-grounded reason.
- **The other five "sibling pairs" are not transpositions.** MeshAnim `Key<vector<VecN>>`
  (both → `resize<...Color...>`), StreakMeter `resize` (both → `_M_erase<ObjOwnerPtr<Waypoint>>`),
  VocalTrack `_Deque_base`, TrackWidget `Clear`, MeshAnim `vector<VecN>`. Their charges name
  fold survivors or unrelated third parties; no name swap can clear them.
- **No alias installed, none pruned, none withdrawn.** `scripts/symbol_aliases.json` is
  byte-untouched: 1,657 groups / 5,474 folded, before and after.
- **No `splits.txt` edit** and **no source change**. The patch is map-only.

### The census is 92% unreachable by this instrument

| stratum | rows | bytes | reachable by a name fix? |
|---|---:|---:|---|
| real-bodied | 26 | 2,808 B | only via a 2-cycle — **2 found, 1 payable** |
| UNPAIRED (`fuzzy == 0`) | 12 | 528 B | **no** — no charged site exists to read |
| vtable adjustor thunks (8–12 B) | 10 | 116 B | no — body evidence vacuous |
| tiny stubs (≤4 B) | 6 | 24 B | no — a 4-byte body compares equal to everything |
| **total** | **54** | **3,476 B** | **280 B adjudicated = 8.1%** |

## 4. Inherited figures, re-derived

- **The census is 54 rows / 3,476 B, not EC's 57 / 3,728 — and that RECONCILES EC rather
  than refuting it.** The difference is exactly 3 rows and `252 B = 60 + 60 + 132`: precisely
  the three rows EC's own fixes took to 100. EC's figure was correct at its commit. This
  double-entry agreement is also the census instrument validating itself.
- **`_bijection_arbitrary` holds 1,008 VAs** (1,006 after this lane). `obj_target_symbol_renamer`'s
  docstring says **939 live / 10 sub-100**; `namecheck_df_census.py`'s docstring says **1,109**.
  Both are stale — three artifacts, three different numbers. Re-derive, never inherit.
- **49 of the 1,008 VAs carry no name in the map at all** (44 have no key, 5 are JSON `null`).
  They are flagged name-ambiguous while having no name to be ambiguous about. Recorded as
  hygiene; not acted on.

## 5. Pre-registration, and the ALIAS_SUSPECT alert

Stated in writing **before** any measurement (`~/tmp/w16ee_prereg.md`, timestamped
2026-09-16T02:31:33Z, i.e. before the A/B started at 02:31:55).

> **Predicted +280 B / +2 fns**; the two TrackPanelDir rows 99.85714 → 100;
> `default/TrackPanelDir` +2; every other unit unchanged; **collateral predicted ZERO**
> because the BandTrack rows' sole relocation targets unnamed placeholders that
> `name_check` forgives. Named downside risk: if they are *not* forgiven, −40 B / −2 fns.

**MEASURED +280 B / +2 fns — the prediction HELD, including the zero-collateral claim.**
`default/TrackPanelDir` 174 → 176, 0 units fell off, unit-net +2 = whole-binary +2. My
predicted `code%` was 40.234732 against a measured 40.234730 — a rounding artifact in my own
arithmetic, not a discrepancy. The named downside risk did not materialise: all four Unison
rows read 100 in leg B.

Unlike W16-EC, this lane's prediction did not miss, so it produced no second fix by that
route. I note this plainly rather than claiming a hit is worth more than EC's instructive miss.

### The `none` control fired ALIAS_SUSPECT — and it is a FALSE POSITIVE here, for a stated reason

```
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler +280 B)
[control none] ALIAS_SUSPECT: default ruler UP (+280 B) while `none` is FLAT on a
map-only patch — the FABRICATED-ALIAS shape. Adjudicate on retail bytes before landing.
```

The guard is right to fire and right to demand retail bytes; I am not overriding it, I am
answering it. Four reasons this is the wrong-callee-fix shape, not the fabricated-alias one:

1. **`none` is structurally incapable of registering this change.** Under
   `functionRelocDiffs=none`, relocation names are not compared at all, and this patch
   changes *nothing but* a relocation name. A correct rename and a fabricated alias both read
   +0 there. The flatness is **vacuous by construction** — as CLAUDE.md says, it is "the
   SIGNATURE of the hazard, not a clearance," and equally it is not a conviction.
2. **No alias exists to have been fabricated.** `symbol_aliases.json` is untouched — 1,657
   groups / 5,474 folded on both legs. The ALIAS_SUSPECT premise is factually inapplicable;
   the guard keys on patch *kind*, and `map` is its proxy for "alias", not the thing itself.
3. **An alias lifts the score by construction; a swap does not.** Forgiveness is
   unidirectional — it can only help. A *swap* is self-inverse and bidirectional: the wrong
   orientation is the status quo, which scores **worse** (99.85714). The metric could have
   refused this and did not, so the lift is information, not arithmetic.
4. **Retail-byte adjudication was performed, twice, independently** (§1 routes 1 and 2), one
   of which is the compiler's own class layout.

## 6. Deliberately NOT done, and why

- **No permuter** (standing user directive).
- **EC's `0x8277B530` lead was NOT taken.** It is unnamed and is BandUser's `Symbol → TrackType`
  converter. Naming an anonymous address converts **forgiven** call sites into **checked**
  ones: the payout is bug exposure, not bytes, and the downside is real. It is legitimate
  work, but it is a *bet* and a different lane from this transposition vein; I preferred to
  finish the census cleanly and hand the bet on un-hedged rather than mix a speculative
  naming into a falsifiable map repair whose delta would then be uninterpretable.
- **The 12 unpaired (`fuzzy == 0`) rows were not pursued.** An unpaired row is invisible to
  callee adjudication by construction — there is no charged site to read.
- **The 10 adjustor thunks and 6 tiny stubs (140 B) were not pursued** — body evidence on a
  4-byte function is vacuous.
- **`map_misassignment` was not re-run** (drained, 0 of 27 live, per standing instruction).
- **I did not overturn MPNGAP-1.** This lane is *consistent* with it: of 3,476 census bytes,
  the adjudicable surface was 280 B (8.1%).

## 7. Leads for the next lane

- **The `_bijection_arbitrary` sub-100 vein is now DRAINED of transpositions.** The 2-cycle
  detector returns exactly two over the whole census; one is landed here and the other
  (`PropSync`) is priced at 0 by a shared fold survivor. Re-running this census for *bytes*
  is not worth funding. Re-derive it before believing that, as I had to.
- **`0x8277B530` remains unnamed** (BandUser's `Symbol → TrackType` converter, called only by
  `SetTrackType(Symbol)`). Still a bug-exposure bet, still unattempted.
- **The 49 name-less `_bijection_arbitrary` VAs** are a hygiene defect nobody has examined:
  flagged as name-ambiguous with no name present. Cheap to audit, unknown yield.
- **The shared-fold-survivor signature is reusable.** "Both halves of a sibling pair charge
  the *same* target name" identifies an unswappable pair in one pass and would have saved
  this lane most of its adjudication time had it been applied first.
