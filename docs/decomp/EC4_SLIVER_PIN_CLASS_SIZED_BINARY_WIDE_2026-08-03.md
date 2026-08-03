# Lane EC-4 — the sliver-pin misattribution class, sized BINARY-WIDE

Tree: `155a0468` → landed at `cabf5002`. Successor to lane EC-2
(`EC2_MISATTRIBUTION_SIZED_2026-08-03.md`), which sized the class inside the
39-unit COMPLETABLE bucket and asked for a binary-wide sweep.

**Headline: EC-2's 80% / 9.58× DOES NOT REPLICATE, and the reason is a
confound in the instrument, not a difference between the populations.** The
neighbourhood oracle's charged and control groups differ 5× in the very
variable that drives the flag. Controlled for it, the enrichment collapses from
3.52× crude to **1.13–1.23×**, and **no isolation stratum reaches significance**.

**A refined oracle recovers real but much smaller signal** (2.63× standardised),
which sizes the class at **~36 rows binary-wide (3.2% of adjudicable charged
rows)** — against EC-2's implied ~10/48 = 21%.

**Convertibility is ZERO.** Of 62 flagged charged rows, **2** sit in a unit with
exactly one blocker, and **both are false positives**. Landed 4 adjudicated rows
for correctness: **Δmatched +0, 0 units fell off**.

## Baseline (leg A, re-measured — reproduces EC-2's post-landing state exactly)

| measure | value |
|---|---:|
| `matched_functions` | 43,852 |
| `total_functions` | 69,298 |
| `matched_code` | 4,234,020 |
| `total_code` | **10,689,000** |
| `matched_code_percent` | 39.611004 |
| `masked_equal_functions` | 22,724 |
| honest | 21,128 |
| AT_100 / COMPLETABLE / ceiling | **255 / 35 / 290** |

`total_code` did **not** move this time (10,689,000, same as EC-2). It is still
read from the key, never memorised — it moved twice on 2026-08-02.

## The instrument EC-2 never committed

EC-2's WHOLE/START/MID cross-tab is its most useful output and the classifier
that produced it existed only inside that session. `tools/ec4_block_position.py`
is that instrument, generalised to the whole binary. It derives each row's VA two
ways (anon rows carry the VA in the name; named rows via a **unique** map entry,
never a guess) and assigns it to its `splits.txt` `.text` block.

It **cross-checks its own proxy** rather than assuming it: for every single-row
block it compares the block span against the row size. 212 of 2,243 disagree by
>8 B — and **200 of those by exactly +12**, the known 8-byte EH prefix plus
alignment, which carries no function row and is not in `total_code`. Only 5
blocks have a gap ≥32 B that could conceivably hide a function.

Coverage: **946 pinned units, 53,806 rows placed, 20 UNPLACED** (reported and
excluded from every rate). The 81 census units it cannot pair are **all**
`auto_generated` carve units with no named splits entry.

| pos | sub-100 | at-100 | total |
|---|---:|---:|---:|
| WHOLE | 249 | 1,723 | 1,972 |
| START | 209 | 2,722 | 2,931 |
| MID | 1,183 | 15,430 | 16,613 |
| END | 103 | 1,248 | 1,351 |

⇒ the candidate population is **458 named sub-100 WHOLE+START rows**, against
EC-2's 17.

## ★★★ FINDING 1 — the pooled binary-wide read INVERTS, and that is Simpson's paradox

Run binary-wide, EC-2's oracle reports **CHARGED 1.98% vs CONTROL 2.26% =
0.88×** and its own built-in guard fires *"NOT DISCRIMINATING"*. That pooled
figure is not the refutation — it is contaminated, because the sweep includes
**auto_\* carve units** (4,297 rows with no splits entry, ~0.2% foreign) which
dilute the charged stratum ~4×. Stratified by position on the named-pin
population, every stratum is enriched and EC-2's **ordering replicates**:

| pos | charged foreign | control foreign | enr |
|---|---|---|---:|
| **WHOLE** | 62/104 = **59.62%** | 150/1,179 = 12.72% | **4.69×** |
| START | 21/134 = 15.67% | 141/2,167 = 6.51% | 2.41× |
| END | 9/74 = 12.16% | 37/1,008 = 3.67% | 3.31× |
| **MID** | 9/959 = **0.94%** | 50/12,408 = 0.40% | 2.33× |

Two corrections to EC-2 already: the WHOLE rate is **59.6%, not 80%** (its 80%
was 8/10), and ⛔ **MID is NOT 0.0% — it is 0.94%, and 2.33× enriched over its
own control.** "0 of 31, no exceptions" was an n=31 sample. The operational rule
still holds directionally (a MID row is ~63× less likely to be flagged), but the
absolute claim is refuted: **9 flagged MID rows exist.**

## ★★★ FINDING 2 — the oracle measures ISOLATION, and the control is not the same population

Whether the oracle *can* answer "yes, a neighbour shares my class" is driven by a
structural variable nobody controlled: **what fraction of the neighbours it
inspects are the row's own unit's code.**

| position | charged own-unit neighbours | control own-unit neighbours |
|---|---:|---:|
| WHOLE | **11.13%** | **58.95%** |
| START | 48.41% | 67.03% |
| MID | **85.39%** | 90.57% |
| END | 55.51% | 70.98% |

Two consequences, both fatal to the raw cross-tab:

* **"MID blockers are 0% foreign" is very largely STRUCTURAL.** A MID row is
  bracketed by its own block's functions by construction — 85% of what the
  oracle looks at is the unit itself. That is a property of the *question*, not
  evidence that MID rows are correctly attributed.
* **The at-100 control is 5× less isolated than the charged rows it nulls.** A
  charged-vs-control ratio therefore pools two groups differing in the very
  variable that drives the flag.

Binning by isolation and standardising (`tools/ec4_isolation_matched.py`), both
directions agree the enrichment is ~1.2×:

| isolation bin | charged foreign | control foreign | enr | Fisher p |
|---|---|---|---:|---:|
| **iso=0%** | 87/94 = **92.55%** | 288/332 = **86.75%** | **1.07×** | 0.151 |
| 0-25% | 5/27 = 18.52% | 28/263 = 10.65% | 1.74× | 0.210 |
| 25-50% | 4/85 = 4.71% | 23/803 = 2.86% | 1.64× | 0.317 |
| 50-75% | 2/168 = 1.19% | 28/2,400 = 1.17% | 1.02× | 1.000 |
| 75-100% | 3/897 = 0.33% | 11/12,964 = 0.08% | 3.94× | 0.058 |

```
CRUDE        charged 7.95% vs control 2.26% = 3.52x   <- CONFOUNDED
STANDARDISED charged on control isolation mix 2.78% vs 2.26% = 1.23x
STANDARDISED control on charged isolation mix 7.05% vs 7.95% = 1.13x
```

**Not one bin is significant.** And the `iso=0%` bin — which is *what a WHOLE
sliver pin is* — is the demonstration: rows at **mpn == 100, byte-equal to
retail and therefore provably attributed correctly, read 86.75% foreign.**

Concretely, three proven-correct rows the oracle calls foreign:

| row (mpn 100, byte-equal to retail) | "foreign" neighbour |
|---|---|
| `CharSleeve::Poll` (1,980 B, WHOLE) | `CharNeckTwist` ×6 |
| `AccomplishmentOneShot::InitializeTrackerDesc` (600 B, WHOLE) | `AccomplishmentPlayerConditional` |
| `BinkClip::Handle` (460 B, START) | `MoggClip` ×5 |

⇒ **the failure mechanism**: the oracle tests *exact class-name string
equality*, but with no LTCG `.text` groups TUs **by subsystem**, so sibling
classes from adjacent TUs are the norm at every TU boundary — and a WHOLE sliver
pin is nothing *but* boundary.

⚠ The only bin approaching significance (75-100%, p=0.058) is itself
contaminated: 2 of its 3 rows (`RecursePatternInternal`, `MakeRotQuat`) are
**free functions**, which have no class and so can never match a neighbour. That
is EC-2's vacuity guard 3 leaking — it excluded a unit only when its own-class
set was *empty*, but a free function inside a *mixed* unit fires just as
trivially. The leak is 10.9% of charged foreign rows and 13.5% of control.

## ★★ FINDING 3 — a subsystem-aware oracle recovers real signal, at 2.63×

`tools/ec4_subsystem_oracle.py` resolves each neighbour **class** to the unit(s)
defining it in `report.json`, maps those to a **source directory** via the
census, and asks whether any neighbour is from the row's own directory — the
question the original instrument was reaching for. Measured against the same
isolation-matched control, and with guard 3 strengthened to drop free functions:

| test | isolation-standardised | crude |
|---|---:|---:|
| EC-2 exact class-name | 2.23% vs 1.88% = **1.19×** | 3.79× |
| **EC-4 same-source-directory** | 1.79% vs 0.68% = **2.63×** | 8.01× |

At `iso=0%` the **control** false-positive rate falls **85.52% → 30.34%** (the
directory test removes about two thirds of the structural false positives) and
enrichment there goes 1.07× → **2.24×**.

**Excess-over-control sizing** (observed flagged minus the control rate applied
to the charged n, per isolation bin):

| bin | flagged | expected FP | excess |
|---|---:|---:|---:|
| iso=0% | 55 / 81 | 24.6 | **30.4** |
| 0-25% | 4 / 23 | 0.8 | 3.2 |
| 25-50% | 2 / 70 | 0.4 | 1.6 |
| 50-75% | 0 / 149 | 0.1 | −0.1 |
| 75-100% | 1 / 814 | 0.1 | 0.9 |
| **TOTAL** | **62 / 1,137** | **26.0** | **≈ 36** |

⇒ **~36 genuinely misattributed named rows binary-wide = 3.16% of adjudicable
charged rows.** Position mix of the 62 flagged: WHOLE 42, START 15, MID 3, END 2
— so the sliver-pin concentration is real, just far weaker than 80%.

## ★★★ FINDING 4 — CONVERTIBILITY IS ZERO. This is not a unit-completion lever.

Of the 62 flagged charged rows, **only 2** sit in a unit with exactly one blocker
(i.e. where excision would complete the unit), and **both are false positives**:

| row | why it is NOT misattributed |
|---|---|
| `HamPhotoDisplay::Save` @`0x822CDC80` | EC-2 already **proved** this native — retail has our exact `WriteEndian`→`RndDir::Save` shape at 82.72%; residue is a real 40-byte vbase gap. |
| `PitchDetector::Detect` @`0x82B72C50` | `mydir = src/system/synth_xbox`, `nb_dirs = src/system/synth_xbox/soundtouch/source/SoundTouch` — the neighbour directory is a **SUBDIRECTORY of its own**. The dir test does exact string equality. Plus fuzzy 79.6% over 676 B. |

★ The refined oracle **cleared** `SHA1::Transform` (5,856 B, fuzzy 55.7%), which
the class-name test had flagged — a useful validation that the refinement
removes known-false candidates rather than merely reshuffling them.

⇒ **the sliver-pin class does not raise the unit ceiling at all.** It is a
correctness class of ~36 rows with **zero** unit-completion yield. This answers
the question the lane was funded to answer: it should **not** become a standing
sweep for unit completions.

## What was LANDED (`cabf5002`) — 4 rows, adjudicated on retail bytes

Two independent ICF-immune axes (subsystem-neighbourhood + incoming-argument
register set) **and then the retail body itself**. Only **5** rows binary-wide
clear both axes; 4 survived retail-byte adjudication.

| row | the retail body says |
|---|---|
| `??0DefaultPhysicsManager@@QAA@PAVRndDir@@@Z` @`0x82BAC148`, 316 B | stores **two incoming FPR args** (`stfs f1,0x20(r3)` / `stfs f2,0x24(r3)`) — a `(this, RndDir*)` ctor is r3/r4 only; calls `Gem::InitChordInfo` + a `String` ctor; **never stores a `DefaultPhysicsManager` vtable**, which our body correctly does. Pin sits **7 MB** from the unit's only other block. |
| `?HandleEnter@HamScrollSpeedIndicator@@QAAXXZ` @`0x822E36B8`, 16 B | `lwz r3,0x760(r3)`, tail call with `r5=1, r6=0` and an **unset r4 passed through**. `void HandleEnter()` is `(this)` only and 0x760 is far beyond the class. |
| `??4String@@QAAAAV0@VSymbol@@@Z` @`0x823E0420`, 12 B | `stw r4,0x28(r3)` / `lwz r3,0x30(r3)` / tail call. A `String` is pointer-sized, and an `operator=` returning `String&` cannot return `*(this+0x30)`. |
| `??0SpeechMgr@@QAA@PBVDataArray@@@Z` @`0x82459FC8`, 100 B | reads **r5** (`lwz r11,0x0(r5)`) before ever writing it = a **third incoming GPR arg** the signature cannot have; writes only 8 bytes at `this`, no vtable store. |

**DECLINED, one axis only:** `SongDB::GetVocalNoteList(int)` @`0x82770730`.
Retail uses exactly `r3,r4` — *matching* the signature — and indexing two
`this`-arrays at 0x50/0xb0 then tail-calling is plausible for the named method.
A single flagged axis is not an adjudication; this is the `HamPhotoDisplay`
discipline applied prospectively.

`Str.cpp` is a **boundary move, not a block deletion** (that row is START of a
multi-function block): `0x823E0420 → 0x823E0430`, keeping the 8-byte EH prefix
with the function it belongs to. All four units retain ≥1 `.text` block so none
**VANISHES** (DG-2). `.pdata` untouched — derived output.

```
Δmatched +0 · Δmasked_equal +0 · Δhonest +0 · Δcode% +0.000000pp · Δcode_bytes +0
Δfuzzy −0.000637pp
units at 100%: 255 → 255  (0 reached 100, 0 FELL OFF)
pairable units 1027 → 1031
```

Forced re-split on **both** legs (`renamer_patched=1048`), both settled to zero
work. The zero is expected — a name/pin is a masked relocation argument.

★ **Not metric-fitting, by EC-2's own check — denominator-neutral GLOBALLY.**
`total_functions` (69,298) and `total_code` (10,689,000) are **identical on both
legs**, no unit left the denominator, and the four rows moved into
`auto_03_822E36B8_text`, `auto_03_823E0420_text`, `auto_03_82459FC8_text`,
`auto_03_82BAC148_text` (+2 re-derived `auto_01_*_pdata`) — re-attributed from a
TU that does not own them to *unidentified retail code*.

## Lessons

* ★★★ **An oracle whose flag is driven by a structural variable must be
  controlled on that variable, not merely against "untreated" rows.** The
  at-100 control looked impeccable — it is untreated, ICF-immune, and drawn from
  the same units — and it was still the wrong null, because being at 100%
  correlates with being deeply embedded in your own unit. The tell was available
  cheaply: *measure the control's structural profile, not just its rate.*
* ★★★ **A "0 of N, no exceptions" result should be checked for whether the
  instrument CAN fire in that stratum.** MID reads 0% partly because 85% of what
  the oracle inspects for a MID row is the row's own unit.
* ★★ **Pooled binary-wide rates inverted the sign** (0.88×) while every stratum
  was enriched. Both the pooled figure and the naive cross-tab were misleading,
  in opposite directions; only the isolation-matched analysis was stable, and it
  agreed in both standardisation directions.
* ★ **A diagnosed failure mechanism is worth more than a discarded instrument.**
  Naming the cause (exact class-name equality vs subsystem grouping) turned a
  1.19× dead flag into a 2.63× live one.

## What I did NOT do

- **Did not adjudicate the remaining ~58 flagged rows** on retail bytes. The
  ~36-row figure is a *statistical* estimate from excess-over-control, not 36
  adjudicated rows; only 5 clear both automated axes and 4 survived byte
  adjudication. Treat 4 as the confirmed floor and ~36 as the estimated size.
- **Did not re-pin** EC-2's three vanished units (`FilterQueue`, `HamDriver`,
  `system/gesture/SkeletonDir`), nor the four homes vacated here. Their retail
  homes remain **unknown**; that is identification work, and identification is
  what raises the ceiling.
- **Did not adjudicate EC-2's 15 one-axis suspects** individually — this lane's
  binary-wide sweep supersedes that list, since the same isolation confound
  applies to it.
- **Did not touch `symbols.txt`** and **did not touch `src/`**, so the native
  gate does not apply.
- **Did not re-fund** `HamPhotoDisplay`, `FLOAT_TARGET_ONLY` (anti-enriched
  0.33×), or the MID stratum.

## Reusable output

- `tools/ec4_block_position.py` — block-position classifier with its own proxy
  cross-check and an explicit UNPLACED report.
- `tools/ec4_isolation_matched.py` — isolation-binned, direct-standardised
  enrichment. **Use this instead of a raw charged-vs-control ratio** for any
  neighbourhood-style flag.
- `tools/ec4_subsystem_oracle.py` — same-source-directory oracle with the
  strengthened free-function guard.
- `tools/ec4_axis2_driver.py` — drives EC-2's incoming-arg witness over an
  explicit shortlist (imports its `probe`/`classify` verbatim).
