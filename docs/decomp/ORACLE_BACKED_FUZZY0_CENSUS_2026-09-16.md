# The oracle-backed fuzzy-0 census — and a 740 B vein closed by a bad lookup

**2026-09-16, main `63fa1e86`, ruler `name_check`.** Tool:
`tools/oracle_backed_census.py` (re-run it; do not inherit the numbers below —
the standing rule is that every such figure moves).

## Why this exists

W16-FG banked **+3,280 B in a single build** by implementing one row. The shape
that paid was worth naming precisely, because it is reproducible:

> a big **NAMED** row at **fuzzy 0**, in a **PAIRABLE in-scope** unit, whose
> **DC3 oracle for THAT ROW** is matched.

Running that as a query over the whole binary produced two lanes immediately
(W16-FJ, 3,228 B; W16-FK, 740 B) and sized the remaining vein.

## The population, at `63fa1e86`

Named rows at fuzzy 0 in pairable units, excluding `xdk`/`auto_`, tiered by
**DC3's score for that row**:

| tier | rows | bytes | share |
|---|---:|---:|---:|
| **DC3 ≥99.9 (solved)** | **139** | **15,980** | 38.0% |
| **DC3 90–99.9 (near)** | **15** | **6,324** | 15.1% |
| DC3 50–90 | 2 | 1,040 | 2.5% |
| DC3 <50 (weak) | 42 | 9,236 | 22.0% |
| NO DC3 ROW | 116 | 9,432 | 22.5% |
| **total** | **314** | **42,012** | |

**Oracle-backed (≥90): 154 rows / 22,304 B.** FJ and FK take 3,968 B of it.

The top of this list is **code we do not have at all**, not divergent code:
`PeakDetector.cpp` is one comment line (header likewise) against DC3's 137;
`GranularSynth.cpp` holds only two STL vector ctors, with `ExtractGranules`,
`Synthesize`, `Flush` and the destructor all absent. Both units are confirmed
pairable, so the bytes are collectable.

⚠ That does **not** contradict the established finding that ~70% of the write
surface is where our source already equals or beats the better oracle. That
describes the *aggregate* surface; this describes the *size-ranked top of the
fuzzy-0 list*, which is different in character. Use the right one per question.

## Two screens that must not be used — both measured wrong the same day

**1. The oracle's UNIT percentage.** It is the wrong instrument and it discards
real prizes:

| unit | DC3 unit % | the row we wanted | DC3 row |
|---|---:|---|---:|
| `PeakDetector` | 5.3% | `Detect` | **96.8819** |
| `GranularSynth` | 35.7% | `Synthesize` | **99.9487** |
| `ContentMgr_Xbox` | 75.7% | `PollRefresh` | **95.1713** |

**2. A SAME-BASENAME unit lookup.** This one closed a vein. Resolving each of
our rows against DC3's unit of the same basename reported
`?ReverbConvertI3DL2ToNative@@` (740 B) as **"row absent in DC3"** — decisive,
and false. DC3 holds it in `.../synth_xbox/**Synth**` at **f=99.8289**, with a
written body at `../dc3-decomp/src/system/synth_xbox/Synth.cpp:106`.

On that false negative the row was recorded as *"MS vendor implementation, XDK
porting out of scope"* and closed. Both halves were wrong: our row is in an
**engine** unit, DC3's body is in an **engine** file (not `src/xdk/`), and the
function is pure field arithmetic calling no XDK API.

This is the `basename()` trap family (four lanes broken by it on splits/pins,
per CLAUDE.md) reappearing in *oracle resolution*, and it is the worst variant —
**a false negative shaped like a decisive verdict, which closes a vein nobody
reopens.**

⚠ Scoping by unit was not even buying disambiguation: only **17 of 48,348** DC3
names appear in more than one unit, and **none** were in the oracle-backed
population. Key oracle lookups on the **name across all units**.

## Pricing rules this census bakes in

- **`matched_code` is all-or-nothing per row.** Only `fuzzy == 100.0` pays; 97%
  of a 1,728 B row banks zero.
- **Subtract the arg-only DRAINED class** before quoting a prize. GranularSynth
  carries 344 B in eight rows at f=99.45–99.90 (relocation-name-only). A unit's
  "unsolved bytes" total silently includes them.
- **A matched oracle is not a promise.** W16-FK's oracle stalls at 99.83 and
  DC3's own source comment says three independent spellings failed to close it
  (a `/fp:fast` operand-order backend floor). A faithful port may reproduce
  99.83 and bank zero. Such a row is still worth funding — DC3's residual is
  against the *DC3* binary, ours is RB3 retail TU5 on an **earlier XDK**, so the
  residual need not transfer — but it must be priced as hard, not as free bytes.
- **Version deltas are real and are not oracle defects.** DC3's
  `ReverbConvertI3DL2ToNative` assigns `WetDryMix`/`WetDryMixPct`, fields RB3's
  `XAUDIO2FX_REVERB_PARAMETERS` does not have (W16-FG adjudicated it to **0x34**
  on retail bytes; DC3's is 0x38). The row-size delta — 740 B ours vs 748 B DC3,
  ≈ two extra stores — corroborates it independently. **The tell separating a
  version delta from an oracle defect is that the oracle's own row scores 100
  with its value.**

## ⚠ CORRECTIONS — measured by the two lanes this census briefed (same day)

Both rules above were *directionally* right and *operationally* wrong, and each
cost real bytes. Both are now enforced in `tools/oracle_backed_census.py`'s
output rather than left to the reader.

### 1. Price by the ORACLE'S DISTANCE FROM 100, not by the row's size (W16-FJ)

The census printed `ORACLE-BACKED (>=90): N rows / B bytes <- work these first`,
and I briefed that byte total to W16-FJ as the prize. **It is not the prize.**
`matched_code` pays only at `fuzzy == 100`, so **a faithful port of a sub-100
oracle pays EXACTLY ZERO** — it reproduces the oracle's wall along with the
oracle's code.

Measured on FJ: of **3,228 B** briefed as oracle-backed, **1,112 B crossed** and
**2,116 B landed on DC3's wall, reproducing DC3's scores to four decimals.** That
four-decimal agreement is not a failure — it is the strongest available evidence
that the port was *faithful*. The failure was the pricing.

⇒ The tool now splits the line into **COLLECTABLE (oracle ≥99.9)** and
**ORACLE'S WALL (oracle 90–99.9)**, and says so. The third bullet above already
made this point *for `ReverbConvertI3DL2ToNative` specifically*; what it did not
do was generalise it into the rule the summary line was printing.

### 2. The DRAINED class is `mpn == 100 AND fuzzy < 100` — never "f looks like 99.x" (W16-FI)

The bullet above describes the drained class by its *symptom* (`f=99.45–99.90`).
That is not a predicate, and filing on it misclassifies rows in both directions.

Measured on FI: of **9 rows / 384 B** I filed as drained on the 99.x eyeball,
only **6 rows / 264 B** actually were. The other three read `mpn == fuzzy`
sub-100 on **both** rulers — ordinary broken rows — and **two of them crossed as
collateral, paying 80 B, which was 27% of that lane's entire delta.**

⇒ **A row misfiled into a closed class is invisible to the lane told to skip it.**
The drained class is defined by the two rulers *disagreeing* (`mpn` excludes
arg-only penalties, `fuzzy` does not); it is not defined by where `fuzzy` lands.

### Not a correction: the population moves

Re-running this census after W16-FG/FJ/FI/FH land returns **smaller** tiers than
the table in §"The population" — rows left the fuzzy-0 population by crossing.
That is the instrument working. **Re-run it; never inherit its figures**, exactly
as with `total_code` and the reachable ceiling.
