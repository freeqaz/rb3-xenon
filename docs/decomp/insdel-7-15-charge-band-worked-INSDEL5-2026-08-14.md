# `SOURCE_INSDEL` 7–15 charge band — sized, worked, and re-characterised (lane INSDEL-5, 2026-08-14)

Tree `b3e65016` (INSDEL-4 merged) + this lane. Ruler `functionRelocDiffs=name_check`,
read from `report.json` `provenance.diff_config` (`tool_commit 6bf7ba700ce5`,
`tool_binary_hash 69e6f63d2b725e33`) and re-confirmed on every per-row reading.

Baseline (leg A, settled): `matched_functions` **44,422** · `masked_equal` **22,897**
⇒ honest **21,525** · `matched_code` **3,730,660 B** · `matched_code_percent`
**36.147480** · `total_code` **10,320,664** (read from the key, not inherited).

## THE TWO NUMBERS THE BRIEF ASKED FOR

### 1. The 7–15 band is **160 rows / 45,192 B** — 4.1× the 4–6 band

| charge band | rows | bytes | avg | worked by |
|---|---:|---:|---:|---|
| ≤3 | 45 | 12,436 | 276 | INSDEL-1/2/3 |
| 4–6 | 54 | 10,912 | 202 | INSDEL-4 |
| **7–15** | **160** | **45,192** | **282** | **this lane** |
| 16+ | 545 | 378,184 | 693 | untouched |

⚠ **Staleness check, run rather than assumed: 0 of the 160 rows had closed** since
INSTR-1's census, joined against a `report.json` built fresh in this worktree.
0 rows were missing from the report either.

⚠ **A discrepancy with INSDEL-4's table I could not reconcile and am flagging
rather than papering over:** recomputing the ≤3 band from the same census column
(`billed`) gives **12,436 B**, where INSDEL-4's table says **3,652 B**. Its 4–6
figure (54 rows / 10,912 B) reproduces from that column **exactly**, so the method
is right and the ≤3 cell is the odd one out — most likely a post-closure remainder
mislabelled as the band total. It does not affect any conclusion here, but the
≤3 comparison below should be treated as approximate.

### 2. Control availability: **10 of 15 rows triaged (67%)** — it went UP, sharply

| lane | band | control existed for | rate |
|---|---|---|---|
| INSDEL-3 | ≤3 | 4 of 4 opened | 100% |
| INSDEL-4 | 4–6 | 3 of 19 triaged | **16%** |
| **INSDEL-5** | **7–15** | **10 of 15 triaged** | **67%** |

**This reverses INSDEL-4's projection.** It expected "well under 16%" at this
width, reasoning that more charges ⇒ more ways to draw an unclosable one. That
reasoning is sound *per row* and is exactly why the number moved the other way:

★★★ **At 7–15 charges the wins are SHARED-CAUSE FAMILIES, not individual rows —
and a family supplies its own control.** Two of the three closures were families,
and in each the control was *internal to the population*: a sibling that already
matches at 100% while carrying the rival source shape. Triage cost collapses
because one reading adjudicates 4–7 rows at once.

⚠ The third closure (`FilterTypeToSym`, a singleton) had the **same control
form** — a 100%-matching sibling in its own TU (`BuildFilters`) carrying the
rival numbering via raw integer literals. So the operative condition is not
"family" as such but **"does a 100%-matching neighbour encode the rival shape?"**
Families merely make that far more likely, and let one reading pay for many rows.

⇒ **The right unit of work at this width is the UNIT, not the ROW.** Measured on
the band: **75 rows / 18,848 B (41% of band bytes) live in 29 units holding more
than one row.**

### ⛔⛔ BUT "MULTI-ROW UNIT" IS NOT "SHARED CAUSE" — I tested my own rule and it FAILED

The obvious targeting rule — *rank by multi-row unit* — is **necessary but nowhere
near sufficient**, and I nearly briefed it before checking. `default/MeshAnim` has
the highest row count in the band (**6 rows / 836 B**) and is **not a family at
all**: `vector<Key<vector<Vector2>>>::_M_insert_overflow_aux`, `resize` and
`_M_fill_insert` over *different* element types, `RndShaderMgr::SetTransform`, an
`AccomplishmentConditional` dtor, and a `??_G` deleting thunk (already on the
do-not-reopen list as a fold-alias). Six unrelated causes that happen to share a
TU.

The discriminator that actually separates them is **register-cleanliness**: every
`MemTracker` family row had **zero** register charges (one cause, fully in the
source), every `MeshAnim` row carries them. Applying both conditions:

| unit | bytes | rows | genuine family? |
|---|---:|---:|---|
| `default/MemTracker` | 1,188 | 5 | **YES** — one template family, one type, one comparator ← drained |
| `default/MoveMgr` | 876 | 2 | no — `_Rb_tree<Symbol>` vs `vector<SpotlightEntry>` |
| `default/band3/meta_band/ViewSetting` | 616 | 2 | no — `FilterTypeToSym` vs `RndGroup::Draw` |
| `default/StorePanel` | 452 | 2 | **plausible** — `Load` + `IsLoaded`, same panel |
| `default/TourProgress` | 304 | 4 | **YES** — four sibling accessors, one guard ← drained |

**Only 5 units in the whole 160-row band have ≥2 register-clean rows, totalling
3,436 B — and this lane drained both of the genuine families (1,492 B).**

★★★ **Therefore the FAMILY half of this lane's economics does NOT GENERALISE to
the band's remainder.** The 67% control-availability and 1.25-rows-per-closure
figures are dominated by the two families; the ~154 rows left are overwhelmingly
singletons. **The family surface in 7–15 was ~1,492 B and it is gone.**

⚠ But do not over-bound it either: `FilterTypeToSym` was a **singleton worth
580 B on its own** — more than either family's per-row yield — and it was found
by exactly the same screen. **Singletons here are not worthless; they are merely
priced like 4–6 (≈6 rows triaged per closure), against rows averaging 282 B
instead of 202 B.**

## Result — +12 functions / +2,384 B, all three predictions pre-registered and EXACT

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| `matched_functions` | 44,422 | 44,434 | **+12** |
| `masked_equal` | 22,897 | 22,897 | +0 |
| honest | 21,525 | 21,537 | **+12** |
| `matched_code` | 3,730,660 | 3,733,044 | **+2,384 B** |
| `matched_code_percent` | 36.147480 | 36.170580 | **+0.023100 pp** |

**0 regressions. 0 units fell off 100% on EITHER ruler, in all three A/Bs.** Native
gate **PASS 18/18, 0 SKIPs, rc=0**.

## Closure 1 — `MemDiffEntry::operator<`: one line, +7 functions / +1,804 B

All five `MemTracker` rows in the band are STL sort/heap templates instantiated on
the **file-local** `MemDiffEntry`, and every charge in every row sits in ONE
contiguous cluster: the inlined comparator.

Our source:

```cpp
if (mHeap != other.mHeap) return mHeap < other.mHeap;
return mSizeDiff < other.mSizeDiff;      // ← wrong
```

Retail emits **two different comparison idioms** for these two returns:

| | idiom | decodes to |
|---|---|---|
| `mHeap` branch | `subfc/eqv/srwi/addze/clrlwi` | strict `<` |
| second branch | `srwi/srawi/subfc/adde` | **non-strict, reversed** |

Because we spell both the same way, MSVC shares one tail between them; retail
duplicates it.

★ **The control is INSIDE the row, and it is the strongest form available: the
`mHeap` half already matched us at 100%.** That pins "our `<` spelling → the eqv
idiom", so a `<` spelling *provably cannot* also produce retail's second idiom.
No oracle involved — and none could have helped, since `src/system` is a verbatim
DC3 copy and DC3 is newer.

★ **Derived twice under opposite operand placement**, which is what makes it
safe rather than a plausible story: `__push_heap` holds `this` in memory and the
comparand in registers, `__unguarded_linear_insert` the reverse. Both give
`mSizeDiff >= other.mSizeDiff`. Retail sorts descending by size within a heap;
`>=` rather than `>` is retail's own sloppiness, and it is what the bytes say.

★ Corroborating **size inequality**: retail is consistently 4–8 B **bigger** in
every affected row (156/148, 212/204, 284/280) — the direction that excludes
"retail inlined something we did not". And `mulli r11,r11,72` matched throughout,
so `sizeof == 72` was never the defect.

★★ **A held-back control population existed by construction and it verified
perfectly:** all **7** `MemDiffEntry` templates that inline `operator<` were
broken and all 7 crossed; the 9 that do **not** inline it (`__introsort_loop`,
`__insertion_sort`, `__final_insertion_sort`, `__pop_heap`, `__pop_heap_aux`,
`__make_heap`, `sort_heap`, `sort`, `__unguarded_insertion_sort_aux`) were
already at 100% and **stayed** there.

### ⚠ The prediction MISSED, and the miss is the most reusable finding here

Pre-registered **+5 / +1,188 B**; measured **+7 / +1,804 B**.

| row | bytes | was | in band? |
|---|---:|---:|---|
| `__median` | 352 | 55.625 | **NO** |
| `__adjust_heap` | 308 | 83.052 | yes |
| `__linear_insert` | 284 | 81.620 | yes |
| `__unguarded_partition` | 264 | 69.167 | **NO** |
| `__partial_sort` | 228 | 78.860 | yes |
| `__push_heap` | 212 | 80.755 | yes |
| `__unguarded_linear_insert` | 156 | 68.718 | yes |

⇒ **616 B — 34% of the yield — landed OUTSIDE the band being worked**, because
`__median` (55.6% fuzzy) and `__unguarded_partition` (69.2%) inline the same
comparator but carry more charges.

★★★ **A charge-band census systematically UNDER-COUNTS shared-cause families:
the band boundary cuts through the family, not around it.** Every per-band ROI
figure in this lane series is therefore a floor, and the 16+ tail is not a
separate population from 7–15 — it holds the *other half* of the same families.

## Closure 2 — 4 `TourProgress` accessors: +4 functions and **exactly +0 bytes**

`GetTourName`, `GetTourWelcome`, `GetTourLeaderboardGoal`, `GetNextCity` each
carried `if (!pTourDesc) return gNullStr;`. **RB3-360 retail has no such guard.**
Our source faithfully ports the rb3-Wii **DEV** oracle, which does have it — the
oracle right about intent, wrong about retail.

Controls, all read before editing:

1. **In-source notes from a prior lane**, which had already removed this identical
   guard from three siblings in the same file and recorded the retail-byte
   verification (`GetNumTotalGigs` L248, `DoesTourHaveLeaderboard` L241,
   `GetTotalStarsForTour` L317). These four are the ones it did not reach.
2. **Size inequality, exact in all four**: retail 72/72/72/88 B vs our
   92/92/92/108 — smaller by exactly 20 B = the 5 guard instructions
   (`bne` / `lis`+`lwz gNullStr` / `bl Symbol` ctor / `b`). Retail **smaller** ⇒
   the guard is **absent**, not inlined.
3. **A 100%-structured sibling carrying the rival shape**: `AreAllTourGigsComplete`
   inlines the already-guard-free `GetNumTotalGigs`, runs straight
   `GetTourDesc → GetNumGigs` with no `cmplwi`/branch, at `target_size ==
   base_size == 84` with zero structural charges.

### ★★★ This row set is the RESIDUAL-1 pattern, and it was priced correctly IN ADVANCE

All four land at **`mpn` 100.0 / fuzzy ≈99.72**, and `matched_code` keys on
`fuzzy == 100`. The pre-registered prediction was **+4 functions, +0 bytes**, and
that is exactly what was measured.

The residual charge in every row is a single **ICF fold-alias**: retail's callee
resolves to `?GetAward@AccomplishmentManager@@` (pinned `0x8235cc80`) where we
spell `Tour::GetTourDesc`, which has no map entry. **Signature adjudication on
retail bytes CONFIRMS our source** — retail passes `TheTour` as `this` and feeds
the result straight into `bl ?GetNumGigs@TourDesc@@`, so it is a `TourDesc*`, not
an `Award*`. The map name is one arbitrary member of a fold group whose body
("hash_map at `this+0x80`; find; return `second` or NULL") is shared across
**52 call sites in 12 unrelated units**.

★ **The `none`-ruler control MEASURES the tax exactly: Δ+304 B on `none` vs +0 on
the shipped `name_check`.** The bytes are real and are withheld *solely* by the
relocation-name charge. Collecting them means installing a `SymbolEquivalences`
alias — forbidden without first proving the fold by relocation-normalized body
hashing, and deliberately **not** bundled here.

⇒ **A row's charge count does not predict its byte collectability.** Budget the
16+ tail knowing that some structurally-perfect closures pay 0 bytes.

## Closure 3 — `FilterType` was MISNUMBERED: +1 function / +580 B

`FilterTypeToSym` sat at fuzzy 96.310 with all 9 charges in one contiguous
cluster — the switch dispatch chain. Both sides emit the **same** lowering
(`mtctr` + CTR-countdown chain), identical case bodies, identical static-Symbol
guard blocks, identical relocation names. Only the value→case mapping differed:
**a data defect, not a codegen-shape one.** (The obvious hypotheses — jump table
vs branch chain, Symbol construction, comparison strength — were all wrong.)

| | mapping |
|---|---|
| **retail** | 0 genres · 1 decades · 2 keys · 3 pro_guitar · 4 vocal_parts · 5 sources · 6 difficulties · 7 lengths · 8 ratings |
| **ours** | 0 difficulties · 1 vocal_parts · 2 lengths · 3 sources · 4 ratings · 5 decades · 6 genres · 7 pro_guitar · 8 keys |

★ **Control 1 — a 100%-matching sibling in the same TU carrying the rival shape.**
`ViewSettingsProvider::BuildFilters` (924 B, **mpn 100.0000**) indexes
`filterSyms[]` with **raw integer literals**, so it is wholly independent of this
enum — and it encodes retail's mapping exactly.

★ **Control 2 — semantic corroboration at the only other call sites in the tree.**
`MusicLibrary::SetupTaskForTrainer`: a `kControllerRealGuitar` case passed
`kFilterSource` (**3**) and a `kControllerKeys` case passed `kFilterLength` (**2**).
Under the corrected numbering 3 *is* pro_guitar and 2 *is* keys — **the numbers
were right all along; only the names were nonsense.**

★ **Control 3 — size inequality.** Retail 580 B vs our 584: we were larger by
exactly the one surplus branch the scrambled mapping forces. Retail's case 0
(genres) is also its physically first body, so the compiler folds the existing
`cmpwi cr6,r19,0` into every step (`bdzf cr6eq`) and lets `ft == 0` fall through —
8 branches. Ours put case 0 third, forcing a leading `beq` **and** a trailing `b`.

★ **Control 4 — the header's own in-source note** asked *"should 2 actually be
has keys? should 3 actually be has pro strings?"* Answered **yes** by all three
of the above. ⇒ **An in-source note is not always a veto — this one was an open
question that INVITED the chase, and the lane's other note (`CamShot`) was a
veto. Read what the note actually claims.**

All 9 charges accounted for. Measured **+1 / +580 B / +0.005620 pp**, exactly as
pre-registered, with **0 units off 100% despite 88 TUs recompiling** from the
header change.

⚠ Case-label **text order** in the switch is deliberately unchanged — it fixes
both the static-Symbol init order and the physical case-body layout, and both
already matched retail byte-for-byte. Only the two bare `case 7:`/`case 8:`
literals became named.

★ Independently of the metric this fixes a **live user-visible bug**: a filter
row's label comes from `FilterTypeToSym(ft)` while its data comes from
`BuildFilters`' raw indices, so every row in the filter view-settings menu was
mislabelled (the row holding *keys* data was titled *lengths*).

## Declined — `GetBlendState` (408 B), 5 of 7 charges have no derivable cause

Two independent clusters. Cluster 2 (2 charges) is understood and one line from
correct — our source keeps the value 64-bit typed (`(long long)(blend*255.0f) &
0xFF`) so MSVC emits `ld` + `rldicl` where retail extracts the spilled low byte
with a single `lbz` at +7. Cluster 1 (5 charges) is **not**: retail materializes
the bool branchily and additionally executes a **dead `fmr f0,f31`** whose
else-value is a `0.0f` our source has no definition for.

⛔ **The one candidate control REFUTED the obvious lever rather than supporting
it.** `RndTexBlendController::IsValid` (136 B, fuzzy 100) carries both an explicit
two-arm `bool` if/else *and* a `refDist > 0` test — and MSVC deleted its `= false`
arm entirely. So "write it as an explicit two-arm if/else" demonstrably does not
determine the emitted shape in this TU.

Since the row is sub-100 on both rulers, landing cluster 2 alone buys **0 bytes
and 0 functions** while perturbing scheduling right next to the cluster-1 charges.
Declined. DC3 adjudicates nothing here — its `TexBlendController.cpp` is
near-verbatim ours, exactly as the standing warning predicts.

## Declined — `CamShot::Copy` (676 B), and the in-source note is why

The charges decode cleanly: retail makes **four** member-assign calls in the
`COPY_MEMBER` run, we make **three**. Frame decoding gives `r30 == this + 416`,
so every source offset maps 1:1 to a dest slot, and retail's order is
`mHideList(88) → mGenHideList(148) → <168> → mShowList(128)` — exactly our source
order. The member at 168 is copied by a `vector`-shaped `operator=`, and our
source has `COPY_MEMBER(mGenHideVector)` **guarded out by `#ifdef HX_NATIVE`** at
precisely that position. Size arithmetic reconciles exactly: retail 676 vs our
672 = missing 3 instrs (12 B) − surplus 2 instrs (8 B).

⛔ **Declined anyway, because the control that exists points AGAINST the edit.**
`CameraShot.h:280`:

> `// NB(rb3-xenon): DC3-only std::vector mirror used by HamCamShot in the native`
> `// build. Retail RB3 has no HamCamShot, so this field is absent in the matching`
> `// layout — guarded out to keep CamShot size correct.`

Three reasons not to overrule it on this lane's budget:

1. **Blast radius is class-wide, not row-local.** `sizeof(std::vector)` is 12, so
   un-guarding shifts `mParentDir` and every member above it by 12 B across every
   function touching `CamShot` — the `volatile` trap at scale (that one closed its
   row at +116 B and knocked a sibling off 100 for **net −60 B**).
2. **Not every charge is accounted for.** A surplus copy of offset **64** remains
   unexplained by the `mGenHideVector` hypothesis, and one unclosable charge
   withholds the whole row.
3. **The prize is probably 0 bytes anyway.** Retail's call is
   `vector<int,StlNodeAlloc<int>>::operator=` where we would emit
   `vector<RndDrawable*,…>` — an ICF fold survivor (identical bodies for any
   4-byte element type). Unless `icf_aliases.map` already carries that
   equivalence, the row stops at `fuzzy < 100` exactly like `TourProgress`.

⇒ **Recorded as a genuine open contradiction, not as noise: the note asserts the
field is absent from retail, and retail's own bytes show a member copy at 168
between `mGenHideList` and `mShowList`.** Settling it needs a layout-wide A/B
with an AT_100 set-diff over all of `CameraShot`/`HamCamShot`, which is a lane of
its own. **Do not "fix" it cheaply.**

## Is 7–15 worth funding further? YES — and it is the best band so far

| | ≤3 | 4–6 | **7–15** |
|---|---:|---:|---:|
| rows closed | 4 | 3 | **12** |
| bytes | +316 | +576 | **+2,384** |
| bytes per closed row | 79 | 192 | **199** (298 counting only byte-paying rows) |
| rows triaged per closure | ~1.0 | 6.3 | **1.25** |
| control availability | 100% | 16% | **67%** |
| band total | 12,436 B | 10,912 B | **45,192 B** |

**7–15 pays 4.1× the 4–6 band's bytes at roughly one-fifth the triage cost per
closure.** The mechanism is not that the rows are easier — individually they are
harder — it is that **at this width rows cluster into families, and a family is
adjudicated once and closed wholesale.**

⚠ Do not over-read the triage-per-closure figure: it is low *because* families
close in bulk. A lane that picks singleton rows here will see 4–6's economics or
worse. **The targeting rule is: multi-row units first.**

### ⚠ …but do NOT fund a general 7–15 sweep on these numbers

Read the two paragraphs above together: the band paid 3.1× because it contained
**two genuine shared-cause families**, and this lane took both. What remains is
~155 largely-singleton rows whose economics should be assumed to match 4–6's
(6.3 rows triaged per closure, 16% control availability) until measured otherwise.

### Recommended next, in order

1. **`default/StorePanel` (452 B, `Load` + `IsLoaded`)** — the only plausible
   untouched family left in this band by the register-clean test. Small, but it is
   the one candidate with a real prior.
2. **★ Re-census the ENTIRE `SOURCE_INSDEL` stratum BY UNIT AND TYPE, ignoring
   charge bands altogether.** This is the highest-value follow-up and this lane is
   the evidence for it: the band boundary cut straight through the `MemDiffEntry`
   family (`__median` at 55.6% fuzzy and `__unguarded_partition` at 69.2% sat
   *outside* 7–15 and fell to the same one-line edit, 616 B = 34% of the yield).
   **A family-first census over all 804 rows would find the 16+ tail's families
   without anyone having to "work the 16+ band" at all.** Screen with:
   *≥2 rows sharing a unit* **AND** *register-clean* **AND** *sharing a type or
   code path* — that conjunction found both of this lane's wins and correctly
   rejected `MeshAnim`.
3. `CamShot::Copy` + the `CameraShot` layout contradiction above — as a dedicated
   layout lane with a whole-class AT_100 set-diff.
4. **Not** a general 16+ row-by-row sweep, and **not** the fold-alias lever until
   the folds are proven by relocation-normalized body hashing.

## Tooling

`~/tmp/insdel5_show.py` (per-row charged-instruction dump at the shipped ruler,
inherited from INSDEL-4 with `WT` repointed) and `~/tmp/insdel5_join.py`
(census → fresh-`report.json` join, emitting `~/tmp/insdel5_live.tsv`).
Per-row findings are recorded as **in-source notes** at `MemTracker.cpp` and
`TourProgress.cpp`.
